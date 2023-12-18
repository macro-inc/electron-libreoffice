// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"
#include "office/lok_callback.h"

namespace electron::office::lok_callback {

TEST(IsUnoCommandResultSuccessfulTest, ValidSuccess) {
  std::string_view name = "testCommand";
  std::string payload = R"({"commandName":"testCommand","success":true})";

  bool result = IsUnoCommandResultSuccessful(name, payload);
  ASSERT_TRUE(result);
}

TEST(IsUnoCommandResultSuccessfulTest, ValidSuccessWithSpacing) {
  std::string_view name = "testCommand";
  std::string payload = R"({ "commandName": "testCommand", "success": true })";

  bool result = IsUnoCommandResultSuccessful(name, payload);

  ASSERT_TRUE(result);
}

TEST(IsUnoCommandResultSuccessfulTest, ValidFailure) {
  std::string_view name = "testCommand";
  std::string payload = R"({"commandName":"testCommand","success":false})";

  bool result = IsUnoCommandResultSuccessful(name, payload);

  ASSERT_FALSE(result);
}

TEST(IsUnoCommandResultSuccessfulTest, InvalidName) {
  std::string_view name = "testCommand";
  std::string payload = R"({"commandName":"invalidCommand","success":true})";

  bool result = IsUnoCommandResultSuccessful(name, payload);

  ASSERT_FALSE(result);
}

TEST(IsUnoCommandResultSuccessfulTest, MissingSuccessField) {
  std::string_view name = "testCommand";
  std::string payload = R"({"commandName":"testCommand"})";

  bool result = IsUnoCommandResultSuccessful(name, payload);

  ASSERT_FALSE(result);
}

TEST(IsUnoCommandResultSuccessfulTest, InvalidJSON) {
  std::string_view name = "testCommand";
  std::string payload = "Invalid JSON";

  bool result = IsUnoCommandResultSuccessful(name, payload);

  ASSERT_FALSE(result);
}

}  // namespace electron::office
