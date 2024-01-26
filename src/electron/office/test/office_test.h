// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_OFFICE_TEST_H
#define OFFICE_OFFICE_TEST_H

#include "base/at_exit.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "gin/function_template.h"
#include "gin/shell_runner.h"
#include "gin/test/v8_test.h"
#include "office/office_web_plugin.h"
#include "office/test/fake_render_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-persistent-handle.h"
#include "fake_web_plugin_container.h"

namespace electron::office {

template <
    typename Lambda,
    std::enable_if_t<base::internal::HasConstCallOperator<Lambda>>* = nullptr>
decltype(auto) CreateFunction(gin::ContextHolder* holder, Lambda&& lambda) {
  return gin::CreateFunctionTemplate(holder->isolate(),
                                     base::BindLambdaForTesting(lambda))
      ->GetFunction(holder->context())
      .ToLocalChecked();
}

using RunScope = gin::Runner::Scope;

class OfficeTest : public gin::V8Test, public gin::ShellRunnerDelegate {
 public:
  OfficeTest(const OfficeTest&) = delete;
  OfficeTest& operator=(const OfficeTest&) = delete;
  void SetUp() override;
  void TearDown() override;
  void DidCreateContext(gin::ShellRunner* runner) override;

  gin::ContextHolder* GetContextHolder();
  v8::Local<v8::Value> Run(const std::string& source);
  std::string ToString(v8::Local<v8::Value> val);

 protected:
  OfficeTest();
  ~OfficeTest() override;
  std::unique_ptr<base::Environment> environment_;
  std::unique_ptr<base::ShadowingAtExitManager> exit_manager_;
  std::unique_ptr<gin::ShellRunner> runner_;
  std::unique_ptr<gin::Runner::Scope> scope_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class JSTest : public OfficeTest {
 public:
  explicit JSTest(const base::FilePath& path) : path_(path) {}

  void TearDown() override;
  void TestBody() override;
  void UnhandledException(gin::ShellRunner* runner,
                          gin::TryCatch& try_catch) override;

 private:
  const base::FilePath path_;
};

class PluginTest : public JSTest {
 public:
  explicit PluginTest(const base::FilePath& path);
  ~PluginTest() override;

  void SetUp() override;
  void TearDown() override;
  v8::Local<v8::ObjectTemplate> GetGlobalTemplate(
      gin::ShellRunner* runner,
      v8::Isolate* isolate) override;

 private:
	static thread_local PluginTest* self_;
  raw_ptr<OfficeWebPlugin> plugin_;
	std::unique_ptr<blink::WebPluginContainer> container_;
  std::unique_ptr<content::RenderFrameImpl> render_frame_;
	bool visible_;
	gfx::Rect rect_;
	std::vector<base::FilePath> temp_files_to_clean_;
  v8::Global<v8::Promise::Resolver> container_painted_resolver_;
};

}  // namespace electron::office

#endif  // OFFICE_OFFICE_TEST_H
