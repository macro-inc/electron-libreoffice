// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/event_bus.h"
#include "base/test/bind.h"
#include "gin/dictionary.h"
#include "gmock/gmock.h"
#include "office_client.h"

#include "LibreOfficeKit/LibreOfficeKitEnums.h"
#include "gin/converter.h"
#include "office/test/office_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace electron::office {

using EventBusTest = OfficeTest;
using ::testing::MockFunction;

TEST_F(EventBusTest, SingleEventHandlerNoContext) {
  EventBus bus;
  MockFunction<EventBus::EventCallback> handled;
  static const std::string expected_payload = "1,2,3,4";

  { EXPECT_CALL(handled, Call(expected_payload)); }
  bus.Handle(LOK_CALLBACK_INVALIDATE_TILES,
             base::BindLambdaForTesting(
                 [&](std::string payload) { handled.Call(expected_payload); }));
  bus.EmitLibreOfficeEvent(LOK_CALLBACK_INVALIDATE_TILES, expected_payload);
}

TEST_F(EventBusTest, MultipleEventHandlerNoContext) {
  EventBus bus;
  MockFunction<EventBus::EventCallback> handled;
  MockFunction<EventBus::EventCallback> handled2;
  static const std::string expected_payload = "1,2,3,4";

  {
    EXPECT_CALL(handled, Call(expected_payload));
    EXPECT_CALL(handled2, Call(expected_payload));
  }
  bus.Handle(LOK_CALLBACK_INVALIDATE_TILES,
             base::BindLambdaForTesting(
                 [&](std::string payload) { handled.Call(payload); }));
  bus.Handle(LOK_CALLBACK_INVALIDATE_TILES,
             base::BindLambdaForTesting(
                 [&](std::string payload) { handled2.Call(payload); }));
  bus.EmitLibreOfficeEvent(LOK_CALLBACK_INVALIDATE_TILES, expected_payload);
}

TEST_F(EventBusTest, SingleEventHandler) {
  EventBus bus;
  MockFunction<void(std::string, std::string)> handled;
  static const std::string expected_payload = "1,2,3,4";

  { EXPECT_CALL(handled, Call("invalidate_tiles", expected_payload)); }

  gin::Runner::Scope scope(runner_.get());
  bus.SetContext(GetContextHolder()->isolate(), GetContextHolder()->context());
  bus.On("invalidate_tiles",
         CreateFunction(GetContextHolder(), [&](v8::Local<v8::Object> obj) {
           v8::Context::Scope scope(GetContextHolder()->context());
           gin::Dictionary dict(GetContextHolder()->isolate(), obj);

           v8::Local<v8::Value> payload_val;
           dict.Get("payload", &payload_val);
           std::string payload = ToString(payload_val);

           v8::Local<v8::Value> type_val;
           dict.Get("type", &type_val);
           std::string type = ToString(type_val);

           handled.Call(type, payload);
         }));
  bus.EmitLibreOfficeEvent(LOK_CALLBACK_INVALIDATE_TILES, expected_payload);
}

TEST_F(EventBusTest, MultipleEventHandler) {
  EventBus bus;
  MockFunction<void(std::string, std::string)> handled;
  MockFunction<void(std::string, std::string)> handled2;
  static const std::string expected_payload = "1,2,3,4";

  {
    EXPECT_CALL(handled, Call("invalidate_tiles", expected_payload));
    EXPECT_CALL(handled2, Call("invalidate_tiles", expected_payload));
  }

  gin::Runner::Scope scope(runner_.get());
  bus.SetContext(GetContextHolder()->isolate(), GetContextHolder()->context());
  bus.On("invalidate_tiles",
         CreateFunction(GetContextHolder(), [&](v8::Local<v8::Object> obj) {
           v8::Context::Scope scope(GetContextHolder()->context());
           gin::Dictionary dict(GetContextHolder()->isolate(), obj);

           v8::Local<v8::Value> payload_val;
           dict.Get("payload", &payload_val);
           std::string payload = ToString(payload_val);

           v8::Local<v8::Value> type_val;
           dict.Get("type", &type_val);
           std::string type = ToString(type_val);

           handled.Call(type, payload);
         }));
  bus.On("invalidate_tiles",
         CreateFunction(GetContextHolder(), [&](v8::Local<v8::Object> obj) {
           v8::Context::Scope scope(GetContextHolder()->context());
           gin::Dictionary dict(GetContextHolder()->isolate(), obj);

           v8::Local<v8::Value> payload_val;
           dict.Get("payload", &payload_val);
           std::string payload = ToString(payload_val);

           v8::Local<v8::Value> type_val;
           dict.Get("type", &type_val);
           std::string type = ToString(type_val);

           handled2.Call(type, payload);
         }));
  bus.EmitLibreOfficeEvent(LOK_CALLBACK_INVALIDATE_TILES, expected_payload);
}
}  // namespace electron::office
