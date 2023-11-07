// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office_instance.h"

#include <memory>
#include "LibreOfficeKit/LibreOfficeKit.hxx"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

#include "base/logging.h"
#include "base/threading/thread_local.h"

namespace electron::office {

namespace {
static base::NoDestructor<base::ThreadLocalOwnedPointer<OfficeInstance>>
    lazy_tls;
}  // namespace

void OfficeInstance::Create() {
  lazy_tls->Set(std::make_unique<OfficeInstance>());
}

OfficeInstance* OfficeInstance::Get() {
  return lazy_tls->Get();
}

OfficeInstance::OfficeInstance()
    : loaded_observers_(base::MakeRefCounted<OfficeLoadObserverList>()) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&OfficeInstance::Initialize, base::Unretained(this)));
}

OfficeInstance::~OfficeInstance() = default;

void OfficeInstance::Initialize() {
  base::FilePath module_path;
  if (!base::PathService::Get(base::DIR_MODULE, &module_path)) {
    NOTREACHED();
  }

  base::FilePath libreoffice_path =
      module_path.Append(FILE_PATH_LITERAL("libreofficekit"))
          .Append(FILE_PATH_LITERAL("program"));

  instance_.reset(lok::lok_cpp_init(libreoffice_path.AsUTF8Unsafe().c_str()));
  loaded_observers_->Notify(FROM_HERE, &OfficeLoadObserver::OnLoaded,
                            instance_.get());
}

bool OfficeInstance::IsValid() {
  return Get() && Get()->instance_;
}

void OfficeInstance::AddLoadObserver(OfficeLoadObserver* observer) {
  if (instance_) {
    observer->OnLoaded(instance_.get());
  } else {
    loaded_observers_->AddObserver(observer);
  }
}

void OfficeInstance::RemoveLoadObserver(OfficeLoadObserver* observer) {
  loaded_observers_->RemoveObserver(observer);
  // Is the observer pattern overkill if there's only one global OfficeClient?
  // Yes. But we clear when the first observer says farewell because it's thread
  // safe and we can guarantee there is no context without tracking it
  if (lazy_tls->Get())
    lazy_tls->Set(nullptr);
}

void OfficeInstance::HandleDocumentCallback(int type,
                                            const char* payload,
                                            void* document) {
  if (!IsValid()) {
    LOG(ERROR) << "Uninitialized for doc callback";
    return;
  }
  std::string payload_(payload);
  auto& observers = Get()->document_event_observers_;
  auto it = observers.find((size_t)document);
  if (it == observers.end()) {
    // document received an event, but wasn't observed
    return;
  }
  it->second->Notify(FROM_HERE, &DocumentEventObserver::DocumentCallback,
                     payload, document);
}

void OfficeInstance::AddDocumentObserver(size_t id,
                                         DocumentEventObserver* observer) {
  DCHECK(IsValid());
  auto& observers = Get()->document_event_observers_;
  auto it =
      observers
          .try_emplace(id, base::MakeRefCounted<DocumentEventObserverList>())
          .first;
  it->second->AddObserver(observer);
}

void OfficeInstance::RemoveDocumentObserver(size_t id,
                                            DocumentEventObserver* observer) {
  DCHECK(IsValid());
  auto& observers = Get()->document_event_observers_;
  auto it = observers.find(id);
  if (it == observers.end()) {
    return;
  }
  it->second->RemoveObserver(observer);
}
}  // namespace electron::office
