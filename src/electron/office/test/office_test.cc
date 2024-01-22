// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office_test.h"

#include <memory>
#include "base/at_exit.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "gin/public/isolate_holder.h"
#include "gin/try_catch.h"
#include "gtest/gtest.h"
#include "net/base/filename_util.h"
#include "office/office_client.h"
#include "office/office_instance.h"
#include "office/office_web_plugin.h"
#include "office/promise.h"
#include "office/test/fake_render_frame.h"
#include "office/test/simulated_input.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace blink {
class WebCoalescedInputEvent {
 public:
  explicit WebCoalescedInputEvent(std::unique_ptr<blink::WebInputEvent> event)
      : event_(std::move(event)) {}
  const blink::WebInputEvent& Event() const { return *event_.get(); }

  std::unique_ptr<blink::WebInputEvent> event_;
};
}  // namespace blink

namespace electron::office {

OfficeTest::OfficeTest() = default;
OfficeTest::~OfficeTest() = default;
void OfficeTest::SetUp() {
  exit_manager_ = std::make_unique<base::ShadowingAtExitManager>();
  gin::V8Test::SetUp();
  // this is a workaround because we only want the shell runner to own a context
  {
    v8::HandleScope handle_scope(instance_->isolate());
    v8::Local<v8::Context>::New(instance_->isolate(), context_)->Exit();
    context_.Reset();
  }

  run_loop_ = std::make_unique<base::RunLoop>();
  runner_ = std::make_unique<gin::ShellRunner>(this, instance_->isolate());
  environment_ = base::Environment::Create();
  environment_->SetVar("FONTCONFIG_FILE", "/etc/fonts/fonts.conf");
}

void OfficeTest::TearDown() {
  {
    RunScope scope(runner_.get());
    OfficeClient::RemoveFromContext(GetContextHolder()->context());
  }
  runner_.reset();
  run_loop_.reset();
  instance_->isolate()->Exit();
  instance_.reset();
  environment_.reset();
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
    base::RunLoop run_loop_;
    RunScope scope(runner_.get());
    run_loop_.RunUntilIdle();
    runner_->Run("libreoffice.__handleBeforeUnload();", "before_unload");
    run_loop_.RunUntilIdle();
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

constexpr base::TimeDelta kTestTimeout = base::Seconds(30);

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
  base::RunLoop().RunUntilIdle();
}

std::string OfficeTest::ToString(v8::Local<v8::Value> val) {
  return gin::V8ToString(
      GetContextHolder()->isolate(),
      val->ToString(GetContextHolder()->context()).ToLocalChecked());
}

PluginTest::PluginTest(const base::FilePath& path) : JSTest(path) {}
PluginTest::~PluginTest() = default;

thread_local PluginTest* PluginTest::self_ = nullptr;

void PluginTest::SetUp() {
  self_ = this;
  render_frame_ = std::make_unique<content::RenderFrameImpl>(false);
  blink::WebPluginParams dummy_params;
  plugin_ = new OfficeWebPlugin(dummy_params, render_frame_.get());
  container_ = std::make_unique<blink::WebPluginContainer>();
  plugin_->Initialize(container_.get());
  visible_ = true;
  rect_ = {800, 600};  // just an arbitrary initial size
  plugin_->UpdateGeometry(rect_, rect_, rect_, true);
  JSTest::SetUp();

  std::string builtins = R"(
		globalThis.loadEmptyDoc = function loadEmptyDoc() {
			return libreoffice.loadDocument('private:factory/swriter');
    };

		globalThis.ready = function ready(doc) {
			let resolveReady;
			const readyPromise = new Promise((resolve) => {
				resolveReady = resolve;
			});
			doc.on('ready', () => {
				resolveReady();
			});
			return readyPromise;
    };

		globalThis.invalidate = function invalidate(doc) {
			let resolveInvalidate;
			const readyPromise = new Promise((resolve) => {
				resolveInvalidate = resolve;
			});
			doc.on('invalidate_tiles', () => {
				resolveInvalidate();
			});
			return readyPromise;
    };
  )";

  RunScope scope(runner_.get());
  runner_->Run(builtins, "builtins");

  v8::Isolate* isolate = runner_->GetContextHolder()->isolate();
  gin::Dictionary global(isolate,
                         runner_->GetContextHolder()->context()->Global());
  auto keyEventType = gin::Dictionary::CreateEmpty(isolate);
  keyEventType.Set("Down", simulated_input::kKeyDown);
  keyEventType.Set("Up", simulated_input::kKeyUp);
  keyEventType.Set("Press", simulated_input::kKeyPress);
  auto mouseEventType = gin::Dictionary::CreateEmpty(isolate);
  mouseEventType.Set("Down", simulated_input::kMouseDown);
  mouseEventType.Set("Move", simulated_input::kMouseMove);
  mouseEventType.Set("Up", simulated_input::kMouseUp);
  mouseEventType.Set("Click", simulated_input::kMouseClick);
  auto mouseButton = gin::Dictionary::CreateEmpty(isolate);
  mouseButton.Set("Left", simulated_input::kLeft);
  mouseButton.Set("Middle", simulated_input::kMiddle);
  mouseButton.Set("Right", simulated_input::kRight);
  mouseButton.Set("Back", simulated_input::kBack);
  mouseButton.Set("Forward", simulated_input::kForward);

  global.Set("KeyEventType", keyEventType);
  global.Set("MouseEventType", mouseEventType);
  global.Set("MouseButton", mouseButton);
}

void PluginTest::TearDown() {
  self_ = nullptr;
  {
    RunScope scope(runner_.get());
    plugin_->Destroy();
    render_frame_.reset();
    plugin_ = nullptr;
    container_.reset();
    container_painted_resolver_.Reset();
  }
  JSTest::TearDown();
  for (auto& path : temp_files_to_clean_) {
    base::DeleteFile(path);
  }
}

v8::Local<v8::ObjectTemplate> PluginTest::GetGlobalTemplate(
    gin::ShellRunner* runner,
    v8::Isolate* isolate) {
  return gin::ObjectTemplateBuilder(isolate)
      .SetMethod("getEmbed",
                 [](v8::Isolate* isolate) {
                   return self_->plugin_->V8ScriptableObject(isolate);
                 })
      .SetMethod("setDeviceScale",
                 [](float scale) {
                   DCHECK(self_);
                   self_->container_->device_scale_factor_ = scale;
                 })
      .SetMethod(
          "sendMouseEvent",
          [](int event_type, int button, float x, float y,
             gin::Arguments* args) {
            DCHECK(self_);
            std::string maybe_modifiers{};
            args->GetNext(&maybe_modifiers);
            ui::Cursor cursor;
            if (event_type == simulated_input::kMouseClick) {
              self_->plugin_->HandleInputEvent(
                  blink::WebCoalescedInputEvent(
                      simulated_input::CreateMouseEvent(
                          simulated_input::kMouseDown, button, x, y,
                          maybe_modifiers)),
                  &cursor);
              event_type = simulated_input::kMouseUp;
            }
            DCHECK(self_);
            self_->plugin_->HandleInputEvent(
                blink::WebCoalescedInputEvent(simulated_input::CreateMouseEvent(
                    event_type, button, x, y, maybe_modifiers)),
                &cursor);
          })
      .SetMethod("sendKeyEvent",
                 [](int event_type, const std::string& key) {
                   DCHECK(self_);
                   ui::Cursor cursor;
                   if (event_type == simulated_input::kKeyPress) {
                     self_->plugin_->HandleInputEvent(
                         blink::WebCoalescedInputEvent(
                             simulated_input::TranslateKeyEvent(
                                 simulated_input::kKeyDown, key)),
                         &cursor);
                     event_type = simulated_input::kKeyUp;
                   }
                   self_->plugin_->HandleInputEvent(
                       blink::WebCoalescedInputEvent(
                           simulated_input::TranslateKeyEvent(event_type, key)),
                       &cursor);
                 })
      .SetMethod("idle",
                 [](v8::Isolate* isolate) {
                   office::Promise<void> promise(isolate);
                   auto handle = promise.GetHandle();
                   office::Promise<void>::ResolvePromise(std::move(promise));
                   return handle;
                 })
      .SetMethod("log",
                 [](v8::Isolate* isolate, v8::Local<v8::Value> val) {
                   LOG(ERROR) << gin::V8ToString(isolate, val);
                 })
      .SetMethod(
          "resizeEmbed",
          [](v8::Isolate* isolate, int64_t width, int64_t height) {
            DCHECK(self_);
            // downcasting because gin's converter for int32_t returns false for
            // floating types instead of truncating
            self_->rect_ = {static_cast<int>(width), static_cast<int>(height)};
            self_->plugin_->UpdateGeometry(self_->rect_, self_->rect_,
                                           self_->rect_, self_->visible_);
          })
      .SetMethod("updateFocus",
                 [](bool focused, gin::Arguments* args) {
                   DCHECK(self_);
                   bool scripted = false;
                   args->GetNext(&scripted);

                   self_->plugin_->UpdateFocus(
                       focused, scripted ? blink::mojom::FocusType::kScript
                                         : blink::mojom::FocusType::kMouse);
                 })
      .SetMethod("canUndo",
                 []() {
                   DCHECK(self_);
                   return self_->plugin_->CanUndo();
                 })
      .SetMethod("canRedo",
                 []() {
                   DCHECK(self_);
                   return self_->plugin_->CanRedo();
                 })
      .SetMethod(
          "tempFileURL",
          [](v8::Isolate* isolate,
             std::string extension) -> v8::Local<v8::Value> {
            DCHECK(self_);
            base::FilePath path;
            if (!base::GetTempDir(&path)) {
              NOTREACHED();
            }
            path = path.AppendASCII(
                base::GUID::GenerateRandomV4().AsLowercaseString() + extension);
            self_->temp_files_to_clean_.emplace_back(path);
            GURL file_url = net::FilePathToFileURL(path);
            return gin::StringToV8(isolate, file_url.spec());
          })

      .SetMethod("fileURLExists",
                 [](std::string url) {
                   base::FilePath path;
                   return net::FileURLToFilePath(GURL(url), &path) &&
                          base::PathExists(path);
                 })
      .SetMethod("painted",
                 [](v8::Isolate* isolate) {
                   DCHECK(self_);
                   v8::Local<v8::Promise::Resolver> resolver =
                       v8::Promise::Resolver::New(isolate->GetCurrentContext())
                           .ToLocalChecked();

                   self_->plugin_->Container()->invalidated = base::BindOnce(
                       [](v8::Global<v8::Promise::Resolver> resolver,
                          v8::Isolate* isolate) {
                         resolver.Get(isolate)
                             ->Resolve(isolate->GetCurrentContext(),
                                       v8::Undefined(isolate))
                             .Check();
                       },
                       v8::Global<v8::Promise::Resolver>(isolate, resolver),
                       isolate);

                   return resolver->GetPromise();
                 })
      .SetMethod("remountEmbed",
                 []() {
                   DCHECK(self_);

                   self_->plugin_->Destroy();
                   blink::WebPluginParams dummy_params;
                   self_->plugin_ = new OfficeWebPlugin(
                       dummy_params, self_->render_frame_.get());
                   self_->container_ =
                       std::make_unique<blink::WebPluginContainer>();
                   self_->plugin_->Initialize(self_->container_.get());
                   self_->visible_ = true;
                   self_->plugin_->UpdateGeometry(self_->rect_, self_->rect_,
                                                  self_->rect_, true);
                 })
      .Build();
}
}  // namespace electron::office
