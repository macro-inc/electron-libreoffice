// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"
#include "office/test/office_test.h"

namespace electron::office {

using OfficeClientTest = OfficeTest;

TEST_F(OfficeClientTest, ExistsOnContext) {
  RunScope scope(runner_.get());
  EXPECT_FALSE(Run("libreoffice")->IsNullOrUndefined());
  EXPECT_TRUE(Run("libreoffice")->IsObject());
}

}  // namespace electron::office
