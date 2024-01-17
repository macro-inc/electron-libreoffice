// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/document_client.h"
#include "gtest/gtest.h"

namespace electron::office {
// Probably not needed in any practical application, solely for test coverage and Chromium style rules
TEST(DocumentClientTest, DoesNotCrashOutsideOfV8) {
	DocumentClient client;
}
}  // namespace electron::office
