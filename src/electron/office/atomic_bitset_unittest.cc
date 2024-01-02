// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atomic_bitset.h"
#include "base/barrier_closure.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"

namespace electron::office {

TEST(AtomicBitsetTest, EmptySet) {
  AtomicBitset set;
  EXPECT_EQ(set.Size(), size_t(0));
}

TEST(AtomicBitsetTest, SmallSet) {
  constexpr size_t kSize(32);
  AtomicBitset set(kSize);
  EXPECT_EQ(set.Size(), kSize);

  for (size_t i = 0; i < kSize; i++) {
    ASSERT_FALSE(set.IsSet(i));
  }

  for (size_t i = 0; i < kSize; i++) {
    set.Set(i);
    ASSERT_TRUE(set.IsSet(i));
  }

  set.Clear();
  for (size_t i = 0; i < kSize; i++) {
    ASSERT_FALSE(set.IsSet(i));
  }

  for (size_t i = 0; i < kSize; i++) {
    set.Set(i);
  }
  set.ResetRange(1, kSize - 2);
  EXPECT_TRUE(set[0]);
  EXPECT_TRUE(set[kSize - 1]);

  for (size_t i = 1; i < kSize - 1; i++) {
    ASSERT_FALSE(set.IsSet(i));
  }

  set.Set(kSize / 2);
  AtomicBitset other(std::move(set));
  EXPECT_TRUE(other[kSize / 2]);
  other.Reset(kSize / 2);
  EXPECT_FALSE(other[kSize / 2]);
}

TEST(AtomicBitsetTest, LargeSet) {
  constexpr size_t kSize(1048576);
  AtomicBitset set(kSize);
  EXPECT_EQ(set.Size(), kSize);

  for (size_t i = 0; i < kSize; i++) {
    ASSERT_FALSE(set.IsSet(i));
  }

  for (size_t i = 0; i < kSize; i++) {
    set.Set(i);
    ASSERT_TRUE(set.IsSet(i));
  }

  set.Clear();
  for (size_t i = 0; i < kSize; i++) {
    ASSERT_FALSE(set.IsSet(i));
  }

  for (size_t i = 0; i < kSize; i++) {
    set.Set(i);
  }
  set.ResetRange(1, kSize - 2);
  EXPECT_TRUE(set[0]);
  EXPECT_TRUE(set[kSize - 1]);

  for (size_t i = 1; i < kSize - 1; i++) {
    ASSERT_FALSE(set.IsSet(i));
  }

  set.Set(kSize / 2);
  AtomicBitset other;
  other = std::move(set);
  EXPECT_TRUE(other[kSize / 2]);
  other.Reset(kSize / 2);
  EXPECT_FALSE(other[kSize / 2]);
}

TEST(AtomicBitsetTest, ParallelSet) {
  constexpr size_t kSize(2048);
  AtomicBitset set(kSize);
  EXPECT_EQ(set.Size(), kSize);
  using Pool = base::ThreadPool;

  base::test::TaskEnvironment task_env;
  {
    base::RunLoop loop;
    base::RepeatingClosure quit_closure =
        base::BarrierClosure(kSize, loop.QuitClosure());

    for (size_t i = 0; i < kSize; i++) {
      Pool::PostTask(FROM_HERE, base::BindLambdaForTesting([&, i] {
                       EXPECT_FALSE(set.IsSet(i));
                       quit_closure.Run();
                     }));
    }
    loop.Run();
  }

  {
    base::RunLoop loop;
    base::RepeatingClosure quit_closure =
        base::BarrierClosure(kSize, loop.QuitClosure());
    for (size_t i = 0; i < kSize; i++) {
      Pool::PostTask(FROM_HERE, base::BindLambdaForTesting([&, i] {
                       set.Set(i);
                       EXPECT_TRUE(set.IsSet(i));
                       quit_closure.Run();
                     }));
    }
    loop.Run();
  }

  set.Clear();
  {
    base::RunLoop loop;
    base::RepeatingClosure quit_closure =
        base::BarrierClosure(kSize, loop.QuitClosure());
    for (size_t i = 0; i < kSize; i++) {
      Pool::PostTask(FROM_HERE, base::BindLambdaForTesting([&, i] {
                       EXPECT_FALSE(set.IsSet(i));
                       quit_closure.Run();
                     }));
    }
    loop.Run();
  }
}

#if DCHECK_IS_ON()

TEST(AtomicBitsetDeathTest, OutOfBounds) {
  constexpr size_t kSize = 32;
  AtomicBitset set(kSize);

  EXPECT_DCHECK_DEATH({ set[32]; });
  EXPECT_DCHECK_DEATH({ set[-1]; });
  EXPECT_DCHECK_DEATH({ set[-1]; });
  EXPECT_DCHECK_DEATH({ set.Set(32); });
  EXPECT_DCHECK_DEATH({ set.Set(-1); });
  EXPECT_DCHECK_DEATH({ set.Reset(32); });
  EXPECT_DCHECK_DEATH({ set.Reset(-1); });
  EXPECT_DCHECK_DEATH({ set.ResetRange(0, 32); });
  EXPECT_DCHECK_DEATH({ set.ResetRange(32, 0); });
  EXPECT_DCHECK_DEATH({ set.ResetRange(32, 32); });
  EXPECT_DCHECK_DEATH({ set.ResetRange(-1, -1); });
  EXPECT_DCHECK_DEATH({ set.ResetRange(1, 0); });
}

TEST(AtomicBitsetDeathTest, UseAfterMove) {
  constexpr size_t kSize(32);
  AtomicBitset set(kSize);
  AtomicBitset other_set(std::move(set));

  EXPECT_DCHECK_DEATH({ set[0]; });
  EXPECT_DCHECK_DEATH({ set.Set(0); });
  EXPECT_DCHECK_DEATH({ set.Reset(0); });
  EXPECT_DCHECK_DEATH({ set.ResetRange(0, 0); });
}

TEST(AtomicBitsetDeathTest, Uninitialized) {
  AtomicBitset set;

  EXPECT_DCHECK_DEATH({ set[0]; });
  EXPECT_DCHECK_DEATH({ set.Set(0); });
  EXPECT_DCHECK_DEATH({ set.Reset(0); });
  EXPECT_DCHECK_DEATH({ set.ResetRange(0, 0); });
}

#endif

}  // namespace electron::office
