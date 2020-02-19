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

#ifndef RIEGELI_BYTES_CORD_READER_H_
#define RIEGELI_BYTES_CORD_READER_H_

#include <stddef.h>

#include <tuple>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "riegeli/base/base.h"
#include "riegeli/base/chain.h"
#include "riegeli/base/dependency.h"
#include "riegeli/base/object.h"
#include "riegeli/base/resetter.h"
#include "riegeli/bytes/backward_writer.h"
#include "riegeli/bytes/pullable_reader.h"
#include "riegeli/bytes/writer.h"

namespace riegeli {

// Template parameter independent part of `CordReader`.
class CordReaderBase : public PullableReader {
 public:
  // Returns the `absl::Cord` being read from. Unchanged by `Close()`.
  virtual const absl::Cord* src_cord() const = 0;

  bool SupportsRandomAccess() const override { return true; }
  bool SupportsSize() const override { return true; }
  absl::optional<Position> Size() override;

 protected:
  explicit CordReaderBase(InitiallyClosed) noexcept
      : PullableReader(kInitiallyClosed) {}
  explicit CordReaderBase(InitiallyOpen) noexcept
      : PullableReader(kInitiallyOpen) {}

  CordReaderBase(CordReaderBase&& that) noexcept;
  CordReaderBase& operator=(CordReaderBase&& that) noexcept;

  void Reset(InitiallyClosed);
  void Reset(InitiallyOpen);
  void Initialize(const absl::Cord* src);

  void Done() override;
  bool PullSlow(size_t min_length, size_t recommended_length) override;
  using PullableReader::ReadSlow;
  bool ReadSlow(Chain* dest, size_t length) override;
  bool ReadSlow(absl::Cord* dest, size_t length) override;
  using PullableReader::CopyToSlow;
  bool CopyToSlow(Writer* dest, Position length) override;
  bool CopyToSlow(BackwardWriter* dest, size_t length) override;
  bool SeekSlow(Position new_pos) override;

  // Invariant:
  //   if `closed()` then `iter_` is `absl::Cord::CharIterator()`
  //   else `iter_` reads from `*src_cord()`
  absl::Cord::CharIterator iter_;

 private:
  // Moves `iter_` to account for data which have been read from the buffer.
  void SyncBuffer();

  // Sets buffer pointers to `absl::Cord::ChunkRemaining(iter_)`,
  // or to `nullptr` if `iter_ == src->char_end()`.
  void MakeBuffer(const absl::Cord* src);

  // Invariants if `!closed()` and scratch is not used:
  //   `start() == (iter_ == src_cord()->char_end()
  //                    ? nullptr
  //                    : absl::Cord::ChunkRemaining(iter_).data())`
  //   `buffer_size() == (iter_ == src_cord()->char_end()
  //                          ? 0
  //                          : absl::Cord::ChunkRemaining(iter_).size())`
  //   `start_pos()` is the position of `iter_` in `*src_cord()`
};

// A `Reader` which reads from an `absl::Cord`. It supports random access.
//
// The `Src` template parameter specifies the type of the object providing and
// possibly owning the `absl::Cord` being read from. `Src` must support
// `Dependency<const absl::Cord*, Src>`, e.g.
// `const absl::Cord*` (not owned, default), `absl::Cord` (owned).
//
// The `absl::Cord` must not be changed until the `CordReader` is closed or no
// longer used.
template <typename Src = const absl::Cord*>
class CordReader : public CordReaderBase {
 public:
  // Creates a closed `CordReader`.
  CordReader() noexcept : CordReaderBase(kInitiallyClosed) {}

  // Will read from the `absl::Cord` provided by `src`.
  explicit CordReader(const Src& src);
  explicit CordReader(Src&& src);

  // Will read from the `absl::Cord` provided by a `Src` constructed from
  // elements of `src_args`. This avoids constructing a temporary `Src` and
  // moving from it.
  template <typename... SrcArgs>
  explicit CordReader(std::tuple<SrcArgs...> src_args);

  CordReader(CordReader&& that) noexcept;
  CordReader& operator=(CordReader&& that) noexcept;

  // Makes `*this` equivalent to a newly constructed `CordReader`. This avoids
  // constructing a temporary `CordReader` and moving from it.
  void Reset();
  void Reset(const Src& src);
  void Reset(Src&& src);
  template <typename... SrcArgs>
  void Reset(std::tuple<SrcArgs...> src_args);

  // Returns the object providing and possibly owning the `absl::Cord` being
  // read from. Unchanged by `Close()`.
  Src& src() { return src_.manager(); }
  const Src& src() const { return src_.manager(); }
  const absl::Cord* src_cord() const override { return src_.get(); }

 private:
  void MoveSrc(CordReader&& that);

  // The object providing and possibly owning the `absl::Cord` being read from.
  Dependency<const absl::Cord*, Src> src_;
};

// Implementation details follow.

inline CordReaderBase::CordReaderBase(CordReaderBase&& that) noexcept
    : PullableReader(std::move(that)) {
  // `iter_` will be moved by `CordReader<Src>::MoveSrc()`.
}

inline CordReaderBase& CordReaderBase::operator=(
    CordReaderBase&& that) noexcept {
  PullableReader::operator=(std::move(that));
  // `iter_` will be moved by `CordReader<Src>::MoveSrc()`.
  return *this;
}

inline void CordReaderBase::Reset(InitiallyClosed) {
  PullableReader::Reset(kInitiallyClosed);
  iter_ = absl::Cord::CharIterator();
}

inline void CordReaderBase::Reset(InitiallyOpen) {
  PullableReader::Reset(kInitiallyOpen);
  // `iter_` will be set by `Initialize()`;
}

inline void CordReaderBase::Initialize(const absl::Cord* src) {
  RIEGELI_ASSERT(src != nullptr)
      << "Failed precondition of CordReader: null Cord pointer";
  iter_ = src->char_begin();
  MakeBuffer(src);
}

inline void CordReaderBase::MakeBuffer(const absl::Cord* src) {
  if (iter_ == src->char_end()) {
    set_buffer();
    return;
  }
  const absl::string_view fragment = absl::Cord::ChunkRemaining(iter_);
  set_buffer(fragment.data(), fragment.size());
  move_limit_pos(available());
}

template <typename Src>
inline CordReader<Src>::CordReader(const Src& src)
    : CordReaderBase(kInitiallyOpen), src_(src) {
  Initialize(src_.get());
}

template <typename Src>
inline CordReader<Src>::CordReader(Src&& src)
    : CordReaderBase(kInitiallyOpen), src_(std::move(src)) {
  Initialize(src_.get());
}

template <typename Src>
template <typename... SrcArgs>
inline CordReader<Src>::CordReader(std::tuple<SrcArgs...> src_args)
    : CordReaderBase(kInitiallyOpen), src_(std::move(src_args)) {
  Initialize(src_.get());
}

template <typename Src>
inline CordReader<Src>::CordReader(CordReader&& that) noexcept
    : CordReaderBase(std::move(that)) {
  MoveSrc(std::move(that));
}

template <typename Src>
inline CordReader<Src>& CordReader<Src>::operator=(CordReader&& that) noexcept {
  CordReaderBase::operator=(std::move(that));
  MoveSrc(std::move(that));
  return *this;
}

template <typename Src>
inline void CordReader<Src>::Reset() {
  CordReaderBase::Reset(kInitiallyClosed);
  src_.Reset();
}

template <typename Src>
inline void CordReader<Src>::Reset(const Src& src) {
  CordReaderBase::Reset(kInitiallyOpen);
  src_.Reset(src);
  Initialize(src_.get());
}

template <typename Src>
inline void CordReader<Src>::Reset(Src&& src) {
  CordReaderBase::Reset(kInitiallyOpen);
  src_.Reset(std::move(src));
  Initialize(src_.get());
}

template <typename Src>
template <typename... SrcArgs>
inline void CordReader<Src>::Reset(std::tuple<SrcArgs...> src_args) {
  CordReaderBase::Reset(kInitiallyOpen);
  src_.Reset(std::move(src_args));
  Initialize(src_.get());
}

template <typename Src>
inline void CordReader<Src>::MoveSrc(CordReader&& that) {
  if (src_.kIsStable() || ABSL_PREDICT_FALSE(closed())) {
    src_ = std::move(that.src_);
    iter_ = std::exchange(that.iter_, absl::Cord::CharIterator());
  } else {
    BehindScratch behind_scratch(this);
    const size_t position = IntCast<size_t>(start_pos());
    const size_t cursor_index = read_from_buffer();
    src_ = std::move(that.src_);
    // Reset `that.iter_` before `iter_` to support self-assignment.
    that.iter_ = absl::Cord::CharIterator();
    if (position == src_->size()) {
      iter_ = src_->char_end();
      set_buffer();
    } else {
      iter_ = src_->char_begin();
      absl::Cord::Advance(&iter_, position);
      const absl::string_view fragment = absl::Cord::ChunkRemaining(iter_);
      set_buffer(fragment.data(), fragment.size(), cursor_index);
    }
  }
}

template <typename Src>
struct Resetter<CordReader<Src>> : ResetterByReset<CordReader<Src>> {};

}  // namespace riegeli

#endif  // RIEGELI_BYTES_CORD_READER_H_
