// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_OFFICE_SINGLETON_H_
#define OFFICE_OFFICE_SINGLETON_H_

#include <memory>
namespace lok {
class Office;
class Document;
}  // namespace lok

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace electron::office {

class OfficeSingleton {
 public:
  static OfficeSingleton* GetInstance();
  static lok::Office* GetOffice();
  static bool IsValid();

  OfficeSingleton(const OfficeSingleton&) = delete;
  OfficeSingleton& operator=(const OfficeSingleton&) = delete;

 private:
  OfficeSingleton();
  ~OfficeSingleton();

  std::unique_ptr<lok::Office> instance;

  friend struct base::DefaultSingletonTraits<OfficeSingleton>;
};

}  // namespace electron::office

#endif  // OFFICE_OFFICE_SINGLETON_H_
