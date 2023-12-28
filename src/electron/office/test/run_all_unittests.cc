// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "office/test/office_test.h"
#include "office/office_instance.h"

#if BUILDFLAG(IS_APPLE)
#include "office/test/run_all_unittests_mac.h"
#endif

namespace {
// nothing special here yet
class OfficeTestSuite : public base::TestSuite {
 public:
  OfficeTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

	void Shutdown() override {
		electron::office::OfficeInstance::Unset();
		base::TestSuite::Shutdown();
	}

  OfficeTestSuite(const OfficeTestSuite&) = delete;
  OfficeTestSuite& operator=(const OfficeTestSuite&) = delete;
};

base::FilePath TestRootDir() {
  static bool set = false;
  static base::FilePath source_root_dir;
  if (!set) {
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    source_root_dir =
        source_root_dir.AppendASCII("electron").AppendASCII("office");
    set = true;
  }

  return source_root_dir;
}

void RegisterJSTest(const base::FilePath& path) {
  testing::RegisterTest("JSTest", path.BaseName().value().c_str(), nullptr,
                        nullptr, __FILE__, __LINE__,
                        [path]() -> electron::office::OfficeTest* {
                          return new electron::office::JSTest(path);
                        });
}

void RegisterPluginTest(const base::FilePath& path) {
  testing::RegisterTest("PluginTest", path.BaseName().value().c_str(), nullptr,
                        nullptr, __FILE__, __LINE__,
                        [path]() -> electron::office::OfficeTest* {
                          return new electron::office::PluginTest(path);
                        });
}

}  // namespace

void RegisterJSTests() {
  base::FilePath js_test_path = TestRootDir().AppendASCII("js_test");
  base::FileEnumerator e(js_test_path, false, base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.js"));
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    RegisterJSTest(name);
  }

  base::FilePath plugin_test_path = TestRootDir().AppendASCII("plugin_test");
  base::FileEnumerator e2(plugin_test_path, false, base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.js"));
  for (base::FilePath name = e2.Next(); !name.empty(); name = e2.Next()) {
    RegisterPluginTest(name);
  }
}

int main(int argc, char** argv) {
#if BUILDFLAG(IS_APPLE)
  mac_quirks::main(argc, argv);
#endif
  OfficeTestSuite test_suite(argc, argv);
  // TODO: why is foreground process priority necessary? not required to run on
  // macOS and it just crashes tests if the scheduler de-prioritizes
  test_suite.DisableCheckForThreadAndProcessPriority();
  RegisterJSTests();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&OfficeTestSuite::Run, base::Unretained(&test_suite)));
}
