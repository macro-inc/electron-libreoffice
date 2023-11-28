// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>

namespace electron::office {

// A mostly thread-safe bitset that initializes with all bits unset.
// This bitset assumes that its lifetime will outlast the threads using it or that the threads will verify it exists first.
class AtomicBitset {
 public:
  AtomicBitset();
  explicit AtomicBitset(size_t size);
  ~AtomicBitset();

  // no copy
  AtomicBitset(AtomicBitset&) = delete;
  AtomicBitset& operator=(const AtomicBitset&) = delete;

  // trivial move
  AtomicBitset(AtomicBitset&& other) noexcept;
  AtomicBitset& operator=(AtomicBitset&& other) noexcept;

  bool Set(size_t index, std::memory_order order = std::memory_order_seq_cst);
  bool Reset(size_t index, std::memory_order order = std::memory_order_seq_cst);
  void ResetRange(size_t index_start,
                  size_t index_end,
                  std::memory_order order = std::memory_order_seq_cst);
  void Clear(std::memory_order order = std::memory_order_seq_cst);

  size_t Size() const { return size_; }

  bool IsSet(size_t index,
             std::memory_order order = std::memory_order_seq_cst) const;
  bool operator[](size_t index) const;

 private:
#if ATOMIC_LLONG_LOCK_FREE == 2
  typedef unsigned long long BitSetContainer;
#elif ATOMIC_LONG_LOCK_FREE == 2
  typedef unsigned long Container;
#else
  // fallback to unsigned 32-bit
  typedef unsigned int Container;
#endif
  typedef std::atomic<BitSetContainer> AtomicContainer;

  static constexpr size_t container_index(size_t index) {
    return index / kBitsPerContainer;
  }

  static constexpr size_t bit_index(size_t index) {
    return index % kBitsPerContainer;
  }

  static constexpr BitSetContainer kSetBit = 1;
  static constexpr BitSetContainer kAllBitsSet = ~0;
  static constexpr size_t kBitsPerContainer =
      std::numeric_limits<BitSetContainer>::digits;
  size_t size_;
  size_t container_count_;
  std::unique_ptr<AtomicContainer[]> data_;
};

}

