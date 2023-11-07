// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_OFFICE_SINGLETON_H_
#define OFFICE_OFFICE_SINGLETON_H_

#include <map>
#include "base/observer_list_threadsafe.h"
#include "document_event_observer.h"
#include "office_load_observer.h"

namespace lok {
class Office;
class Document;
}  // namespace lok

namespace electron::office {

// This is separated from OfficeClient for two reasons:
// 1. LOK is started before the V8 context arrives
// 2. Keeps the thread-local magic safe from the V8 GC
class OfficeInstance {
 public:
  OfficeInstance();
  ~OfficeInstance();

  static void Create();
  static OfficeInstance* Get();
  static bool IsValid();

  void AddLoadObserver(OfficeLoadObserver* observer);
  void RemoveLoadObserver(OfficeLoadObserver* observer);

  static void HandleDocumentCallback(int type,
                                     const char* payload,
                                     void* document);
  void AddDocumentObserver(size_t id, DocumentEventObserver* observer);
  void RemoveDocumentObserver(size_t id, DocumentEventObserver* observer);

  // disable copy
  OfficeInstance(const OfficeInstance&) = delete;
  OfficeInstance& operator=(const OfficeInstance&) = delete;

 private:
  std::unique_ptr<lok::Office> instance_;
  void Initialize();

  using OfficeLoadObserverList =
      base::ObserverListThreadSafe<OfficeLoadObserver>;
  using DocumentEventObserverList =
      base::ObserverListThreadSafe<DocumentEventObserver>;
  const scoped_refptr<OfficeLoadObserverList> loaded_observers_;
  std::map<size_t, scoped_refptr<DocumentEventObserverList>>
      document_event_observers_;
};

}  // namespace electron::office

#endif  // OFFICE_OFFICE_SINGLETON_H_
