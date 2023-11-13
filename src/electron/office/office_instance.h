// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_OFFICE_SINGLETON_H_
#define OFFICE_OFFICE_SINGLETON_H_

#include <map>
#include <set>
#include <unordered_map>
#include "base/hash/hash.h"
#include "base/observer_list_threadsafe.h"
#include "document_event_observer.h"
#include "office_load_observer.h"

namespace lok {
class Office;
class Document;
}  // namespace lok

namespace electron::office {
struct DocumentEventId {
  const size_t document_id;
  const int event_id;
  const int view_id;

  DocumentEventId(size_t doc_id, int evt_id, int view_id)
      : document_id(doc_id), event_id(evt_id), view_id(view_id) {}

  bool operator==(const DocumentEventId& other) const {
    return (document_id == other.document_id && event_id == other.event_id &&
            view_id == other.view_id);
  }
};
}  // namespace electron::office

namespace std {
using electron::office::DocumentEventId;
template <>
struct ::std::hash<DocumentEventId> {
  std::size_t operator()(const DocumentEventId& id) const {
    return base::HashInts64(id.document_id,
                            base::HashInts32(id.view_id, id.event_id));
  }
};
}  // namespace std

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
                                     void* idPair);
  void AddDocumentObserver(DocumentEventId id, DocumentEventObserver* observer);
  void RemoveDocumentObserver(DocumentEventId id,
                              DocumentEventObserver* observer);
  void RemoveDocumentObservers(size_t document_id,
                               DocumentEventObserver* observer);
  void RemoveDocumentObservers(size_t document_id);

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
  std::unordered_map<DocumentEventId, scoped_refptr<DocumentEventObserverList>>
      document_event_observers_;
  std::unordered_multimap<size_t, DocumentEventId>
      document_id_to_document_event_ids_;
};

}  // namespace electron::office

#endif  // OFFICE_OFFICE_SINGLETON_H_
