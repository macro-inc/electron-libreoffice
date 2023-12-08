// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <unordered_map>
#include <atomic>
#include "base/hash/hash.h"
#include "base/observer_list_threadsafe.h"
#include "document_event_observer.h"
#include "office/destroyed_observer.h"
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

  DocumentEventId(size_t doc_id, int evt_id, int view_id_)
      : document_id(doc_id), event_id(evt_id), view_id(view_id_) {}

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
  static void Unset();

  void AddLoadObserver(OfficeLoadObserver* observer);
  void RemoveLoadObserver(OfficeLoadObserver* observer);

  static void HandleDocumentCallback(int type,
                                     const char* payload,
                                     void* documentContext);
  void AddDocumentObserver(DocumentEventId id, DocumentEventObserver* observer);
  void RemoveDocumentObserver(DocumentEventId id,
                              DocumentEventObserver* observer);
  void RemoveDocumentObservers(size_t document_id,
                               DocumentEventObserver* observer);
  void RemoveDocumentObservers(size_t document_id);

  void AddDestroyedObserver(DestroyedObserver* observer);
  void RemoveDestroyedObserver(DestroyedObserver* observer);
	void HandleClientDestroyed();

  // disable copy
  OfficeInstance(const OfficeInstance&) = delete;
  OfficeInstance& operator=(const OfficeInstance&) = delete;

 private:
  std::unique_ptr<lok::Office> instance_;
	std::atomic<bool> unset_ = false;
	std::atomic<bool> destroying_ = false;
  void Initialize();

  using OfficeLoadObserverList =
      base::ObserverListThreadSafe<OfficeLoadObserver>;
  using DocumentEventObserverList =
      base::ObserverListThreadSafe<DocumentEventObserver>;
  using DestroyedObserverList =
      base::ObserverListThreadSafe<DestroyedObserver>;
  const scoped_refptr<OfficeLoadObserverList> loaded_observers_;
  std::unordered_map<DocumentEventId, scoped_refptr<DocumentEventObserverList>>
      document_event_observers_;
  std::unordered_multimap<size_t, DocumentEventId>
      document_id_to_document_event_ids_;
  const scoped_refptr<DestroyedObserverList> destroyed_observers_;

  base::WeakPtrFactory<OfficeInstance> weak_factory_{this};
};

}  // namespace electron::office
