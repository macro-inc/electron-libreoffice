// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office_client.h"
#include "document_client.h"

#include "base/logging.h"
#include "base/test/bind.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gtest/gtest.h"
#include "office/test/office_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace electron::office {

using OfficeClientTest = OfficeTest;

TEST_F(OfficeClientTest, ExistsOnContext) {
  RunScope scope(runner_.get());
  EXPECT_FALSE(Run("libreoffice")->IsNullOrUndefined());
  EXPECT_TRUE(Run("libreoffice")->IsObject());
}

}  // namespace electron::office
