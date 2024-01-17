// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/office_client.h"
#include "gtest/gtest.h"
#include "office/test/office_test.h"
#include "base/process/process.h"

namespace electron::office {

using OfficeClientTest = OfficeTest;

TEST_F(OfficeClientTest, ExistsOnContext) {
  RunScope scope(runner_.get());
  EXPECT_FALSE(Run("libreoffice")->IsNullOrUndefined());
  EXPECT_TRUE(Run("libreoffice")->IsObject());
}

TEST(OfficeClientProcessTest, WithoutInstance) {
  // use a "death" test to run an out-of-process test
  EXPECT_EXIT(
      {
        base::test::TaskEnvironment task_environment;
				OfficeClient client;

				client.RemoveFromContext({});
				client.GetWeakPtr();
				client.HandleBeforeUnload();
        base::Process::TerminateCurrentProcessImmediately(0);
      },
      testing::ExitedWithCode(0), ".*");
}

}  // namespace electron::office
