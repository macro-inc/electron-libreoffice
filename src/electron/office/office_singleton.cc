// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office_singleton.h"
#include "LibreOfficeKit/LibreOfficeKit.hxx"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/path_service.h"

namespace electron::office {

OfficeSingleton::OfficeSingleton() {
  base::FilePath module_path;
  if (!base::PathService::Get(base::DIR_MODULE, &module_path)) {
    NOTREACHED();
  }

  base::FilePath libreoffice_path =
      module_path.Append(FILE_PATH_LITERAL("libreofficekit"))
          .Append(FILE_PATH_LITERAL("program"));

  instance.reset(lok::lok_cpp_init(libreoffice_path.AsUTF8Unsafe().c_str()));
}

OfficeSingleton::~OfficeSingleton() = default;

OfficeSingleton* OfficeSingleton::GetInstance() {
  return base::Singleton<OfficeSingleton>::get();
}

lok::Office* OfficeSingleton::GetOffice() {
  return GetInstance()->instance.get();
}

bool OfficeSingleton::IsValid() {
  OfficeSingleton* optional = base::Singleton<OfficeSingleton>::GetIfExists();
  return optional && optional->instance;
}
}  // namespace electron::office
