// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office_test.h"

#include <memory>
#include "base/at_exit.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "gin/converter.h"
#include "gin/public/isolate_holder.h"
#include "gin/try_catch.h"
#include "gtest/gtest.h"
#include "office/office_client.h"
#include "office/office_instance.h"
#include "office/office_web_plugin.h"
#include "office/test/fake_render_frame.h"
#include "v8/include/v8-exception.h"

namespace electron::office {

OfficeTest::OfficeTest() = default;
OfficeTest::~OfficeTest() = default;
void OfficeTest::SetUp() {
  exit_manager_ = std::make_unique<base::ShadowingAtExitManager>();
  gin::V8Test::SetUp();
  run_loop_ = std::make_unique<base::RunLoop>();
  runner_ = std::make_unique<gin::ShellRunner>(this, instance_->isolate());
  environment_ = base::Environment::Create();
  environment_->SetVar("FONTCONFIG_FILE", "/etc/fonts/fonts.conf");
}

void OfficeTest::TearDown() {
  run_loop_.reset();
  {
    RunScope scope(runner_.get());
    OfficeClient::RemoveFromContext(GetContextHolder()->context());
  }
  environment_.reset();
  runner_.reset();
  gin::V8Test::TearDown();
  exit_manager_.reset();
}

void OfficeTest::DidCreateContext(gin::ShellRunner* runner) {
  OfficeInstance::Create();
  OfficeClient::InstallToContext(runner->GetContextHolder()->context());
}

gin::ContextHolder* OfficeTest::GetContextHolder() {
  return runner_->GetContextHolder();
}

v8::Local<v8::Value> OfficeTest::Run(const std::string& source) {
  return runner_->Run(source, "office_test.js")
      .FromMaybe(v8::Local<v8::Value>());
}

void JSTest::TearDown() {
  {
    RunScope scope(runner_.get());
    runner_->Run("libreoffice.__handleBeforeUnload();", "before_unload");
  }

  OfficeTest::TearDown();
}

void JSTest::UnhandledException(gin::ShellRunner* runner,
                                gin::TryCatch& try_catch) {
  RunScope scope(runner);
  ADD_FAILURE() << try_catch.GetStackTrace();
  run_loop_->Quit();
}

namespace {

v8::Local<v8::String> GetSourceLine(v8::Isolate* isolate,
                                    v8::Local<v8::Message> message) {
  auto maybe = message->GetSourceLine(isolate->GetCurrentContext());
  v8::Local<v8::String> source_line;
  return maybe.ToLocal(&source_line) ? source_line : v8::String::Empty(isolate);
}

std::string GetStacktrace(v8::Isolate* isolate,
                          v8::Local<v8::Message> message) {
  std::stringstream ss;
  ss << gin::V8ToString(isolate, message->Get()) << std::endl
     << gin::V8ToString(isolate, GetSourceLine(isolate, message)) << std::endl;

  v8::Local<v8::StackTrace> trace = message->GetStackTrace();
  if (trace.IsEmpty())
    return ss.str();

  int len = trace->GetFrameCount();
  for (int i = 0; i < len; ++i) {
    v8::Local<v8::StackFrame> frame = trace->GetFrame(isolate, i);
    ss << gin::V8ToString(isolate, frame->GetScriptName()) << ":"
       << frame->GetLineNumber() << ":" << frame->GetColumn() << ": "
       << gin::V8ToString(isolate, frame->GetFunctionName()) << std::endl;
  }
  return ss.str();
}

}  // namespace

constexpr base::TimeDelta kTestTimeout = base::Seconds(3);

void JSTest::TestBody() {
  base::test::ScopedRunLoopTimeout loop_timeout(FROM_HERE, kTestTimeout);

  int64_t file_size = 0;
  if (!base::GetFileSize(path_, &file_size)) {
    FAIL() << "Unable to get file size" << path_;
  }
  // test is larger than 10 MB, something is probably wrong
  if (file_size > 1024 * 1024 * 10) {
    FAIL() << "Extremely large JS file: " << file_size / 1024 / 1024 << " MB";
  }

  std::string script;
  if (!base::ReadFileToString(path_, &script)) {
    FAIL() << "Unable to read file";
  }
  std::string assert_script = R"(
		globalThis.assert = function assert(cond, message = "Assertion failed") {
      if(cond) return;
      const err = new Error(message);
      // ignore the assert() itself
      Error.captureStackTrace(err, globalThis.assert);
      throw err;
    };
  )";

  RunScope scope(runner_.get());
  runner_->Run(assert_script, "assert");
  GetContextHolder()->isolate()->SetCaptureStackTraceForUncaughtExceptions(
      true);

  v8::MaybeLocal<v8::Value> maybe_result = runner_->Run(script, path_.value());

  v8::Local<v8::Value> result;
  if (maybe_result.ToLocal(&result) && result->IsPromise()) {
    v8::Local<v8::Promise> promise = result.As<v8::Promise>();

    auto quit_loop = run_loop_->QuitClosure();

    v8::Local<v8::Function> fulfilled =
        CreateFunction(GetContextHolder(), [&](gin::Arguments* args) {
          SUCCEED();
          quit_loop.Run();
        });
    v8::Local<v8::Function> rejected =
        CreateFunction(GetContextHolder(), [&](gin::Arguments* args) {
          v8::Local<v8::Value> val;
          if (args->GetNext(&val) && val->IsNativeError()) {
            v8::Local<v8::Message> message = v8::Exception::CreateMessage(
                GetContextHolder()->isolate(), promise->Result());
            ADD_FAILURE() << GetStacktrace(GetContextHolder()->isolate(),
                                           message);
            quit_loop.Run();
          } else {
            ADD_FAILURE();
            quit_loop.Run();
          }
        });

    ASSERT_FALSE(
        promise->Then(promise->GetCreationContextChecked(), fulfilled, rejected)
            .IsEmpty());

    run_loop_->Run();
  }
}

std::string OfficeTest::ToString(v8::Local<v8::Value> val) {
  return gin::V8ToString(
      GetContextHolder()->isolate(),
      val->ToString(GetContextHolder()->context()).ToLocalChecked());
}

PluginTest::PluginTest(const base::FilePath& path) : JSTest(path) {}
PluginTest::~PluginTest() = default;

void PluginTest::SetUp() {
  render_frame_ = std::make_unique<content::RenderFrameImpl>(false);
  blink::WebPluginParams dummy_params;
  plugin_ = new OfficeWebPlugin(dummy_params, render_frame_.get());
  JSTest::SetUp();
}

void PluginTest::TearDown() {
  JSTest::TearDown();
  plugin_->Destroy();
}

}  // namespace electron::office
