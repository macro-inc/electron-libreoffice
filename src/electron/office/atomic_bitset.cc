// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atomic_bitset.h"
#include "base/check_op.h"

AtomicBitset::AtomicBitset() : size_(0), container_count_(0) {}
AtomicBitset::AtomicBitset(size_t size)
    : size_(size),
      container_count_((size + kBitsPerContainer - 1) / kBitsPerContainer),
      data_(std::make_unique<AtomicContainer[]>(container_count_)){};

AtomicBitset::~AtomicBitset() = default;

AtomicBitset::AtomicBitset(AtomicBitset&& other) noexcept : AtomicBitset() {
  *this = std::move(other);
}

AtomicBitset& AtomicBitset::operator=(AtomicBitset&& other) noexcept {
  size_ = other.size_;
  container_count_ = other.container_count_;
  data_ = std::move(other.data_);
  other.size_ = 0;
  other.container_count_ = 0;
  return *this;
}

bool AtomicBitset::Set(size_t index, std::memory_order order) {
  DCHECK_LT(index, size_);
  DCHECK_GE(index, 0ul);
  DCHECK(data_);
  Container mask = kSetBit << bit_index(index);
  return data_[container_index(index)].fetch_or(mask, order) & mask;
}

bool AtomicBitset::Reset(size_t index, std::memory_order order) {
  DCHECK_LT(index, size_);
  DCHECK_GE(index, 0ul);
  DCHECK(data_);
  Container mask = kSetBit << bit_index(index);
  return data_[container_index(index)].fetch_and(~mask, order) & mask;
}

void AtomicBitset::ResetRange(size_t index_start,
                              size_t index_end,
                              std::memory_order order) {
  DCHECK_LT(index_start, size_);
  DCHECK_LT(index_end, size_);
  DCHECK_GE(index_start, 0ul);
  DCHECK_GE(index_end, 0ul);
  DCHECK_LE(index_start, index_end);
  DCHECK(data_);

  const size_t container_start = container_index(index_start);
  const size_t container_end = container_index(index_end);
  // single container case
  if (container_end == container_start) {
    Container mask = (kAllBitsSet << index_start) ^
                     (kAllBitsSet >> (kBitsPerContainer - index_end - 1));
    data_[container_start].fetch_and(mask, order);
    return;
  }

  // all bits at and left to the index are kept
  Container mask = ~(kAllBitsSet << bit_index(index_start));
  data_[container_start].fetch_and(mask, order);

  // the middle containers will be cleared
  for (size_t i = container_start + 1; i < container_end; i++) {
    data_[i].store(0, order);
  }

  // all bits at and right to the index are kept
  mask = ~(kAllBitsSet >> (kBitsPerContainer - bit_index(index_end) - 1));
  data_[container_end].fetch_and(mask, order);
}

void AtomicBitset::Clear(std::memory_order order) {
  DCHECK(data_);
  for (size_t i = 0; i < container_count_; i++) {
    data_[i].store(0, order);
  }
}

bool AtomicBitset::IsSet(size_t index, std::memory_order order) const {
  DCHECK_LT(index, size_);
  DCHECK_GE(index, 0ul);
  DCHECK(data_);
  Container mask = kSetBit << bit_index(index);
  return data_[container_index(index)].load(order) & mask;
}

bool AtomicBitset::operator[](size_t index) const {
  return IsSet(index);
}
