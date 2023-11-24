// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/atomic_flag.h"
#include "base/logging.h"

namespace electron::office {

namespace {
using CancellationFlagInternal = base::RefCountedData<base::AtomicFlag>;
}

using CancelFlagPtr = scoped_refptr<CancellationFlagInternal>;

namespace CancelFlag {

inline CancelFlagPtr Create() {
  return base::MakeRefCounted<CancellationFlagInternal>();
}

inline bool IsCancelled(const CancelFlagPtr& flag) {
  return flag && flag->data.IsSet();
}

inline void Set(const CancelFlagPtr& flag) {
  if (flag) flag->data.Set();
}

// cancels the flag and resets the shared pointer to a new one
inline void CancelAndReset(CancelFlagPtr& flag) {
  Set(flag);
  flag = Create();
}

}

}  // namespace electron::office
