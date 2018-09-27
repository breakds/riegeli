// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "riegeli/records/record_reader.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/utility/utility.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "riegeli/base/base.h"
#include "riegeli/base/chain.h"
#include "riegeli/base/object.h"
#include "riegeli/bytes/chain_backward_writer.h"
#include "riegeli/bytes/chain_reader.h"
#include "riegeli/bytes/message_parse.h"
#include "riegeli/bytes/reader.h"
#include "riegeli/chunk_encoding/chunk.h"
#include "riegeli/chunk_encoding/chunk_decoder.h"
#include "riegeli/chunk_encoding/constants.h"
#include "riegeli/chunk_encoding/transpose_decoder.h"
#include "riegeli/records/chunk_reader.h"
#include "riegeli/records/record_position.h"
#include "riegeli/records/records_metadata.pb.h"
#include "riegeli/records/skipped_region.h"

namespace riegeli {

class RecordsMetadataDescriptors::ErrorCollector
    : public google::protobuf::DescriptorPool::ErrorCollector {
 public:
  void AddError(const std::string& filename, const std::string& element_name,
                const google::protobuf::Message* descriptor,
                ErrorLocation location, const std::string& message) override {
    descriptors_->Fail(absl::StrCat("Error in file ", filename, ", element ",
                                    element_name, ": ", message));
  }

  void AddWarning(const std::string& filename, const std::string& element_name,
                  const google::protobuf::Message* descriptor,
                  ErrorLocation location, const std::string& message) override {
  }

 private:
  friend class RecordsMetadataDescriptors;

  explicit ErrorCollector(RecordsMetadataDescriptors* descriptors)
      : descriptors_(descriptors) {}

  RecordsMetadataDescriptors* descriptors_;
};

RecordsMetadataDescriptors::RecordsMetadataDescriptors(
    const RecordsMetadata& metadata)
    : Object(State::kOpen), record_type_name_(metadata.record_type_name()) {
  if (record_type_name_.empty() || metadata.file_descriptor().empty()) return;
  pool_ = absl::make_unique<google::protobuf::DescriptorPool>();
  ErrorCollector error_collector(this);
  for (const google::protobuf::FileDescriptorProto& file_descriptor :
       metadata.file_descriptor()) {
    if (ABSL_PREDICT_FALSE(pool_->BuildFileCollectingErrors(
                               file_descriptor, &error_collector) == nullptr)) {
      return;
    }
  }
}

void RecordsMetadataDescriptors::Done() {
  record_type_name_ = std::string();
  pool_.reset();
}

const google::protobuf::Descriptor* RecordsMetadataDescriptors::descriptor()
    const {
  if (pool_ == nullptr) return nullptr;
  return pool_->FindMessageTypeByName(record_type_name_);
}

RecordReaderBase::RecordReaderBase(State state) noexcept : Object(state) {}

RecordReaderBase::RecordReaderBase(RecordReaderBase&& that) noexcept
    : Object(std::move(that)),
      chunk_begin_(absl::exchange(that.chunk_begin_, 0)),
      chunk_decoder_(std::move(that.chunk_decoder_)),
      recoverable_(absl::exchange(that.recoverable_, Recoverable::kNo)) {}

RecordReaderBase& RecordReaderBase::operator=(
    RecordReaderBase&& that) noexcept {
  Object::operator=(std::move(that));
  chunk_begin_ = absl::exchange(that.chunk_begin_, 0);
  chunk_decoder_ = std::move(that.chunk_decoder_);
  recoverable_ = absl::exchange(that.recoverable_, Recoverable::kNo);
  return *this;
}

void RecordReaderBase::Initialize(ChunkReader* src, Options&& options) {
  chunk_begin_ = src->pos();
  chunk_decoder_ = ChunkDecoder(ChunkDecoder::Options().set_field_projection(
      std::move(options.field_projection_)));
}

void RecordReaderBase::Done() {
  recoverable_ = Recoverable::kNo;
  if (ABSL_PREDICT_FALSE(!chunk_decoder_.Close())) Fail(chunk_decoder_);
}

bool RecordReaderBase::CheckFileFormat() {
  if (ABSL_PREDICT_FALSE(!healthy())) return false;
  ChunkReader* const src = src_chunk_reader();
  if (chunk_decoder_.index() < chunk_decoder_.num_records()) return true;
  if (ABSL_PREDICT_FALSE(!src->CheckFileFormat())) {
    chunk_decoder_.Reset();
    if (ABSL_PREDICT_FALSE(!src->healthy())) {
      recoverable_ = Recoverable::kRecoverChunkReader;
      return Fail(*src);
    }
    return false;
  }
  return true;
}

bool RecordReaderBase::ReadMetadata(RecordsMetadata* metadata) {
  if (ABSL_PREDICT_FALSE(!healthy())) return false;
  ChunkReader* const src = src_chunk_reader();
  if (ABSL_PREDICT_FALSE(src->pos() != 0)) {
    return Fail(
        "RecordReaderBase::ReadMetadata() must be called "
        "while the RecordReader is at the beginning of the file");
  }

  chunk_begin_ = src->pos();
  Chunk chunk;
  if (ABSL_PREDICT_FALSE(!src->ReadChunk(&chunk))) {
    if (ABSL_PREDICT_FALSE(!src->healthy())) {
      recoverable_ = Recoverable::kRecoverChunkReader;
      return Fail(*src);
    }
    return false;
  }
  RIEGELI_ASSERT(chunk.header.chunk_type() == ChunkType::kFileSignature)
      << "Unexpected type of the first chunk: "
      << static_cast<unsigned>(chunk.header.chunk_type());

  chunk_begin_ = src->pos();
  const ChunkHeader* chunk_header;
  if (ABSL_PREDICT_FALSE(!src->PullChunkHeader(&chunk_header))) {
    if (ABSL_PREDICT_FALSE(!src->healthy())) {
      recoverable_ = Recoverable::kRecoverChunkReader;
      return Fail(*src);
    }
    return false;
  }
  if (chunk_header->chunk_type() != ChunkType::kFileMetadata) {
    // Missing file metadata chunk, assume empty RecordMetadata.
    metadata->Clear();
    return true;
  }
  if (ABSL_PREDICT_FALSE(!src->ReadChunk(&chunk))) {
    if (ABSL_PREDICT_FALSE(!src->healthy())) {
      recoverable_ = Recoverable::kRecoverChunkReader;
      return Fail(*src);
    }
    return false;
  }
  if (ABSL_PREDICT_FALSE(!ParseMetadata(chunk, metadata))) {
    metadata->Clear();
    recoverable_ = Recoverable::kRecoverChunkDecoder;
    return false;
  }
  return true;
}

inline bool RecordReaderBase::ParseMetadata(const Chunk& chunk,
                                            RecordsMetadata* metadata) {
  RIEGELI_ASSERT(chunk.header.chunk_type() == ChunkType::kFileMetadata)
      << "Failed precondition of RecordReaderBase::ParseMetadata(): "
         "wrong chunk type";
  if (ABSL_PREDICT_FALSE(chunk.header.num_records() != 0)) {
    return Fail(absl::StrCat(
        "Invalid file metadata chunk: number of records is not zero: ",
        chunk.header.num_records()));
  }
  ChainReader<> data_reader(&chunk.data);
  TransposeDecoder transpose_decoder;
  ChainBackwardWriter<Chain> serialized_metadata_writer((Chain()));
  std::vector<size_t> limits;
  const bool ok = transpose_decoder.Reset(
      &data_reader, 1, chunk.header.decoded_data_size(), FieldProjection::All(),
      &serialized_metadata_writer, &limits);
  if (ABSL_PREDICT_FALSE(!serialized_metadata_writer.Close())) {
    return Fail(serialized_metadata_writer);
  }
  if (ABSL_PREDICT_FALSE(!ok)) {
    return Fail("Invalid metadata chunk", transpose_decoder);
  }
  if (ABSL_PREDICT_FALSE(!data_reader.VerifyEndAndClose())) {
    return Fail("Invalid metadata chunk", data_reader);
  }
  RIEGELI_ASSERT_EQ(limits.size(), 1u)
      << "Metadata chunk has unexpected record limits";
  RIEGELI_ASSERT_EQ(limits.back(), serialized_metadata_writer.dest().size())
      << "Metadata chunk has unexpected record limits";
  std::string error_message;
  if (ABSL_PREDICT_FALSE(!ParseFromChain(
          metadata, serialized_metadata_writer.dest(), &error_message))) {
    return Fail(error_message);
  }
  return true;
}

template <typename Record>
bool RecordReaderBase::ReadRecordSlow(Record* record, RecordPosition* key) {
  if (chunk_decoder_.healthy()) {
    RIEGELI_ASSERT_EQ(chunk_decoder_.index(), chunk_decoder_.num_records())
        << "Failed precondition of RecordReaderBase::ReadRecordSlow(): "
           "records available, use ReadRecord() instead";
  }
  if (ABSL_PREDICT_FALSE(!healthy())) return false;
  for (;;) {
    if (ABSL_PREDICT_FALSE(!chunk_decoder_.healthy())) {
      recoverable_ = Recoverable::kRecoverChunkDecoder;
      return Fail(chunk_decoder_);
    }
    if (ABSL_PREDICT_FALSE(!ReadChunk())) return false;
    if (ABSL_PREDICT_TRUE(chunk_decoder_.ReadRecord(record))) {
      RIEGELI_ASSERT_GT(chunk_decoder_.index(), 0u)
          << "ChunkDecoder::ReadRecord() left record index at 0";
      if (key != nullptr) {
        *key = RecordPosition(chunk_begin_, chunk_decoder_.index() - 1);
      }
      return true;
    }
  }
}

template bool RecordReaderBase::ReadRecordSlow(
    google::protobuf::MessageLite* record, RecordPosition* key);
template bool RecordReaderBase::ReadRecordSlow(absl::string_view* record,
                                               RecordPosition* key);
template bool RecordReaderBase::ReadRecordSlow(std::string* record,
                                               RecordPosition* key);
template bool RecordReaderBase::ReadRecordSlow(Chain* record,
                                               RecordPosition* key);

bool RecordReaderBase::Recover(SkippedRegion* skipped_region) {
  if (recoverable_ == Recoverable::kNo) return false;
  ChunkReader* const src = src_chunk_reader();
  RIEGELI_ASSERT(!healthy()) << "Failed invariant of RecordReader: "
                                "recovery applicable but RecordReader healthy";
  const Recoverable recoverable = recoverable_;
  recoverable_ = Recoverable::kNo;
  if (recoverable != Recoverable::kRecoverChunkReader) {
    RIEGELI_ASSERT(!closed()) << "Failed invariant of RecordReader: "
                                 "recovery does not apply to chunk reader "
                                 "but RecordReader is closed";
  }
  MarkNotFailed();
  switch (recoverable) {
    case Recoverable::kNo:
      RIEGELI_ASSERT_UNREACHABLE() << "kNo handled above";
    case Recoverable::kRecoverChunkReader:
      if (ABSL_PREDICT_FALSE(!src->Recover(skipped_region))) return Fail(*src);
      return true;
    case Recoverable::kRecoverChunkDecoder: {
      const uint64_t index_before = chunk_decoder_.index();
      if (ABSL_PREDICT_FALSE(!chunk_decoder_.Recover())) chunk_decoder_.Reset();
      if (skipped_region != nullptr) {
        const Position region_begin = chunk_begin_ + index_before;
        const Position region_end = pos().numeric();
        *skipped_region = SkippedRegion(region_begin, region_end);
      }
      return true;
    }
  }
  RIEGELI_ASSERT_UNREACHABLE()
      << "Unknown recoverable method: " << static_cast<int>(recoverable);
}

bool RecordReaderBase::SupportsRandomAccess() const {
  const ChunkReader* const src = src_chunk_reader();
  return src != nullptr && src->SupportsRandomAccess();
}

bool RecordReaderBase::Size(Position* size) {
  if (ABSL_PREDICT_FALSE(!healthy())) return false;
  ChunkReader* const src = src_chunk_reader();
  if (ABSL_PREDICT_FALSE(!src->Size(size))) return Fail(*src);
  return true;
}

bool RecordReaderBase::Seek(RecordPosition new_pos) {
  if (ABSL_PREDICT_FALSE(!healthy())) return false;
  ChunkReader* const src = src_chunk_reader();
  if (new_pos.chunk_begin() == chunk_begin_) {
    if (new_pos.record_index() == 0 || src->pos() > chunk_begin_) {
      // Seeking to the beginning of a chunk does not need reading the chunk,
      // which is important because it may be non-existent at end of file.
      //
      // If src->pos() > chunk_begin_, the chunk is already read.
      goto skip_reading_chunk;
    }
  } else {
    if (ABSL_PREDICT_FALSE(!src->Seek(new_pos.chunk_begin()))) {
      chunk_begin_ = src->pos();
      chunk_decoder_.Reset();
      recoverable_ = Recoverable::kRecoverChunkReader;
      return Fail(*src);
    }
    if (new_pos.record_index() == 0) {
      // Seeking to the beginning of a chunk does not need reading the chunk,
      // which is important because it may be non-existent at end of file.
      chunk_begin_ = src->pos();
      chunk_decoder_.Reset();
      return true;
    }
  }
  if (ABSL_PREDICT_FALSE(!ReadChunk())) return false;
skip_reading_chunk:
  chunk_decoder_.SetIndex(new_pos.record_index());
  return true;
}

bool RecordReaderBase::Seek(Position new_pos) {
  if (ABSL_PREDICT_FALSE(!healthy())) return false;
  ChunkReader* const src = src_chunk_reader();
  if (new_pos >= chunk_begin_ && new_pos <= src->pos()) {
    // Seeking inside or just after the current chunk which has been read,
    // or to the beginning of the current chunk which has been located,
    // or to the end of file which has been reached.
  } else {
    if (ABSL_PREDICT_FALSE(!src->SeekToChunkContaining(new_pos))) {
      chunk_begin_ = src->pos();
      chunk_decoder_.Reset();
      recoverable_ = Recoverable::kRecoverChunkReader;
      return Fail(*src);
    }
    if (src->pos() >= new_pos) {
      // Seeking to the beginning of a chunk does not need reading the chunk,
      // which is important because it may be non-existent at end of file.
      //
      // It is possible that the chunk position is greater than new_pos if
      // new_pos falls after all records of the previous chunk. This also seeks
      // to the beginning of the chunk.
      chunk_begin_ = src->pos();
      chunk_decoder_.Reset();
      return true;
    }
    if (ABSL_PREDICT_FALSE(!ReadChunk())) return false;
  }
  chunk_decoder_.SetIndex(IntCast<uint64_t>(new_pos - chunk_begin_));
  return true;
}

inline bool RecordReaderBase::ReadChunk() {
  ChunkReader* const src = src_chunk_reader();
  chunk_begin_ = src->pos();
  Chunk chunk;
  if (ABSL_PREDICT_FALSE(!src->ReadChunk(&chunk))) {
    chunk_decoder_.Reset();
    if (ABSL_PREDICT_FALSE(!src->healthy())) {
      recoverable_ = Recoverable::kRecoverChunkReader;
      return Fail(*src);
    }
    return false;
  }
  if (ABSL_PREDICT_FALSE(!chunk_decoder_.Reset(chunk))) {
    recoverable_ = Recoverable::kRecoverChunkDecoder;
    return Fail(chunk_decoder_);
  }
  return true;
}

template class RecordReader<Reader*>;
template class RecordReader<std::unique_ptr<Reader>>;
template class RecordReader<ChunkReader*>;
template class RecordReader<std::unique_ptr<ChunkReader>>;
template class RecordReader<DefaultChunkReader<Reader*>>;
template class RecordReader<DefaultChunkReader<std::unique_ptr<Reader>>>;

}  // namespace riegeli
