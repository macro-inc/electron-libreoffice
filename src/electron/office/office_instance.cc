// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office_instance.h"

#include <memory>
#include "LibreOfficeKit/LibreOfficeKit.hxx"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

#include "base/logging.h"
#include "office/document_holder.h"

// Uncomment to log all document events
// #define DEBUG_EVENTS

namespace electron::office {

namespace {
// this doesn't work well because LOK has a per-process global lock, otherwise
// it would be ideal static
// base::NoDestructor<base::ThreadLocalOwnedPointer<OfficeInstance>> lazy_tls;
// instead we use a function-local static that's shared across all threads
OfficeInstance& get_instance() {
  static base::NoDestructor<OfficeInstance> instance;
  return *instance;
}
}  // namespace

void OfficeInstance::Create() {
  static std::atomic<bool> once = false;
  if (once)
    return;
  once = true;

  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&OfficeInstance::Initialize,
                     get_instance().weak_factory_.GetWeakPtr()));
}

OfficeInstance* OfficeInstance::Get() {
  return &get_instance();
}

OfficeInstance::OfficeInstance()
    : loaded_observers_(base::MakeRefCounted<OfficeLoadObserverList>()),
      destroyed_observers_(base::MakeRefCounted<DestroyedObserverList>()) {}

// this is required by Chromium's code conventions, but will never be called due
// to base::NoDestructor
OfficeInstance::~OfficeInstance() = default;

void OfficeInstance::Initialize() {
  base::FilePath module_path;
  if (!base::PathService::Get(base::DIR_MODULE, &module_path)) {
    NOTREACHED();
  }

  base::FilePath libreoffice_path =
      module_path.Append(FILE_PATH_LITERAL("libreofficekit"))
          .Append(FILE_PATH_LITERAL("program"));

  if (!unset_)
    instance_.reset(lok::lok_cpp_init(libreoffice_path.AsUTF8Unsafe().c_str()));
  if (!unset_)
    instance_->setOptionalFeatures(
        LibreOfficeKitOptionalFeatures::LOK_FEATURE_NO_TILED_ANNOTATIONS);
  if (!unset_)
    loaded_observers_->Notify(FROM_HERE, &OfficeLoadObserver::OnLoaded,
                              instance_.get());
}

bool OfficeInstance::IsValid() {
  return static_cast<bool>(Get()->instance_);
}

void OfficeInstance::Unset() {
  Get()->unset_ = true;
  Get()->instance_.reset(nullptr);
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
}

void OfficeInstance::HandleDocumentCallback(int type,
                                            const char* payload,
                                            void* documentContext) {
  DocumentCallbackContext* context =
      static_cast<DocumentCallbackContext*>(documentContext);
  const OfficeInstance* office_instance =
      static_cast<const OfficeInstance*>(context->office_instance);

  if (!office_instance->instance_) {
    LOG(ERROR) << "Uninitialized for doc callback";
    return;
  }

  auto& observers = office_instance->document_event_observers_;
  auto it =
      observers.find(DocumentEventId(context->id, type, context->view_id));
  if (it == observers.end()) {
    // document received an event, but wasn't observed
    return;
  }
#ifdef DEBUG_EVENTS
  LOG(ERROR) << lokCallbackTypeToString(type) << " " << payload;
#endif
  it->second->Notify(FROM_HERE, &DocumentEventObserver::DocumentCallback, type,
                     std::string(payload));
}

void OfficeInstance::AddDocumentObserver(DocumentEventId id,
                                         DocumentEventObserver* observer) {
  DCHECK(IsValid());
  auto it =
      document_event_observers_
          .try_emplace(id, base::MakeRefCounted<DocumentEventObserverList>())
          .first;
  it->second->AddObserver(observer);
  auto& document_event_ids = Get()->document_id_to_document_event_ids_;
  document_event_ids.emplace(id.document_id, id);
}

void OfficeInstance::RemoveDocumentObserver(DocumentEventId id,
                                            DocumentEventObserver* observer) {
  DCHECK(IsValid());
  auto it = document_event_observers_.find(id);
  if (it == document_event_observers_.end()) {
    return;
  }
  it->second->RemoveObserver(observer);
}

void OfficeInstance::RemoveDocumentObservers(size_t document_id) {
  DCHECK(IsValid());
  auto& event_ids = document_id_to_document_event_ids_;
  auto range = event_ids.equal_range(document_id);
  for (auto it = range.first; it != range.second; ++it) {
    document_event_observers_.erase(it->second);
  }
  event_ids.erase(document_id);
}

void OfficeInstance::RemoveDocumentObservers(size_t document_id,
                                             DocumentEventObserver* observer) {
  DCHECK(IsValid());
  auto range = document_id_to_document_event_ids_.equal_range(document_id);
  for (auto it = range.first; it != range.second; ++it) {
    auto obs_it = document_event_observers_.find(it->second);
    if (obs_it != document_event_observers_.end()) {
      obs_it->second->RemoveObserver(observer);
    }
  }
}

void OfficeInstance::AddDestroyedObserver(DestroyedObserver* observer) {
  destroyed_observers_->AddObserver(observer);
}

void OfficeInstance::RemoveDestroyedObserver(DestroyedObserver* observer) {
  destroyed_observers_->RemoveObserver(observer);
}

void OfficeInstance::HandleClientDestroyed() {
	destroying_ = true;
  destroyed_observers_->Notify(FROM_HERE, &DestroyedObserver::OnDestroyed);
}

}  // namespace electron::office
