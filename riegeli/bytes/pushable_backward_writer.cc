// Copyright 2019 Google LLC
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

#include "riegeli/bytes/pushable_backward_writer.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "riegeli/base/base.h"
#include "riegeli/base/chain.h"
#include "riegeli/bytes/backward_writer.h"

namespace riegeli {

void PushableBackwardWriter::Done() {
  if (ABSL_PREDICT_TRUE(healthy())) SyncScratch();
  BackwardWriter::Done();
}

void PushableBackwardWriter::PushFromScratchSlow(size_t min_length) {
  RIEGELI_ASSERT_GT(min_length, 1u)
      << "Failed precondition of "
         "PushableBackwardWriter::PushFromScratchSlow(): "
         "trivial min_length";
  if (ABSL_PREDICT_FALSE(!healthy())) return;
  if (ABSL_PREDICT_FALSE(scratch_ == nullptr)) {
    scratch_ = std::make_unique<Scratch>();
  } else {
    if (ABSL_PREDICT_FALSE(!SyncScratch())) return;
  }
  const absl::Span<char> flat_buffer =
      scratch_->buffer.PrependFixedBuffer(min_length);
  set_start_pos(pos());
  scratch_->original_limit = limit();
  scratch_->original_buffer_size = buffer_size();
  scratch_->original_written_to_buffer = written_to_buffer();
  set_buffer(flat_buffer.data(), flat_buffer.size());
}

bool PushableBackwardWriter::SyncScratchSlow() {
  RIEGELI_ASSERT(limit() == scratch_->buffer.data())
      << "Failed invariant of PushableBackwardWriter: "
         "scratch used but buffer pointers do not point to scratch";
  RIEGELI_ASSERT_EQ(buffer_size(), scratch_->buffer.size())
      << "Failed invariant of PushableBackwardWriter: "
         "scratch used but buffer pointers do not point to scratch";
  const size_t length_to_write = written_to_buffer();
  set_buffer(scratch_->original_limit, scratch_->original_buffer_size,
             scratch_->original_written_to_buffer);
  set_start_pos(start_pos() - written_to_buffer());
  ChainBlock buffer = std::move(scratch_->buffer);
  RIEGELI_ASSERT(scratch_->buffer.empty())
      << "Moving should have left the source ChainBlock cleared";
  bool ok;
  if (length_to_write == buffer.size()) {
    ok = Write(Chain(std::move(buffer)));
  } else if (length_to_write <= kMaxBytesToCopy) {
    ok = Write(absl::string_view(
        buffer.data() + buffer.size() - length_to_write, length_to_write));
  } else {
    Chain data;
    buffer.AppendSubstrTo(
        absl::string_view(buffer.data() + buffer.size() - length_to_write,
                          length_to_write),
        &data);
    ok = Write(std::move(data));
  }
  return ok;
}

void PushableBackwardWriter::BehindScratch::Enter() {
  RIEGELI_ASSERT(context_->limit() == context_->scratch_->buffer.data())
      << "Failed invariant of PushableBackwardWriter: "
         "scratch used but buffer pointers do not point to scratch";
  RIEGELI_ASSERT_EQ(context_->buffer_size(), context_->scratch_->buffer.size())
      << "Failed invariant of PushableBackwardWriter: "
         "scratch used but buffer pointers do not point to scratch";
  written_to_scratch_ = context_->written_to_buffer();
  context_->set_buffer(context_->scratch_->original_limit,
                       context_->scratch_->original_buffer_size,
                       context_->scratch_->original_written_to_buffer);
  context_->set_start_pos(context_->start_pos() -
                          context_->written_to_buffer());
}

void PushableBackwardWriter::BehindScratch::Leave() {
  context_->set_start_pos(context_->pos());
  context_->scratch_->original_limit = context_->limit();
  context_->scratch_->original_buffer_size = context_->buffer_size();
  context_->scratch_->original_limit = context_->limit();
  context_->set_buffer(const_cast<char*>(context_->scratch_->buffer.data()),
                       context_->scratch_->buffer.size(), written_to_scratch_);
}

}  // namespace riegeli