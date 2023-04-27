// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_OFFICE_UNITTEST_UTILS_H
#define OFFICE_OFFICE_UNITTEST_UTILS_H

#include "base/scoped_environment_variable_override.h"

namespace electron::office {
class ScopedEnvironmentVariables {
public:
  ScopedEnvironmentVariables()
      : sal_log("SAL_LOG", "-WARN.configmgr"),
        font_config("FONTCONFIG_FILE", "/etc/fonts/fonts.conf") {}

 private:
  const base::ScopedEnvironmentVariableOverride sal_log;
  const base::ScopedEnvironmentVariableOverride font_config;
};
}  // namespace electron::office

#endif  // OFFICE_OFFICE_UNITTEST_UTILS_H
