// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include "base/callback_forward.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "office/document_event_observer.h"

namespace lok {
class Document;
}

namespace electron::office {

class DocumentHolder : public base::RefCountedDeleteOnSequence<DocumentHolder> {
 public:
  explicit DocumentHolder(lok::Document* owned_document,
                          const std::string& path);

 private:
  const std::string path_;
  std::unique_ptr<lok::Document> doc_;
  friend class base::RefCountedDeleteOnSequence<DocumentHolder>;
  friend class base::DeleteHelper<DocumentHolder>;
  friend class DocumentHolderWithView;
  ~DocumentHolder();
};

// A thread-safe document holder that deletes an lok::Document only when it no
// longer has references
class DocumentHolderWithView {
 public:
  // has the side effect registering document callbacks
  DocumentHolderWithView(lok::Document* owned_document,
                         const std::string& path);
  explicit DocumentHolderWithView(const scoped_refptr<DocumentHolder>& holder);

  // enable copy, disable side effects on this
  DocumentHolderWithView(const DocumentHolderWithView& other);
  DocumentHolderWithView& operator=(const DocumentHolderWithView& other);

  // enable move, disable side effects on other
  DocumentHolderWithView& operator=(DocumentHolderWithView&& other) noexcept;
  DocumentHolderWithView(DocumentHolderWithView&& other) noexcept;

  // when not moved, has the side effect deregistering document callbacks
  DocumentHolderWithView();
  ~DocumentHolderWithView();

  lok::Document& operator*() const;
  lok::Document* operator->() const;
  explicit operator bool() const;
  bool operator==(const DocumentHolderWithView& other) const;
  bool operator!=(const DocumentHolderWithView& other) const;
  DocumentHolderWithView NewView();
  scoped_refptr<DocumentHolder> holder() const;

  int ViewId() const;

  /** You probably don't need to use this.
   * When you call functions through this class, it will set the current view
   * first.
   */
  void SetAsCurrentView() const;

  void Post(base::OnceCallback<void(DocumentHolderWithView holder)> callback,
            const base::Location& from_here = FROM_HERE) const;
  void Post(
      base::RepeatingCallback<void(DocumentHolderWithView holder)> callback,
      const base::Location& from_here = FROM_HERE) const;

  // This likely won't run on the renderer thread, so if something is crashing
  // just switch to using Post
  void PostBlocking(
      base::OnceCallback<void(DocumentHolderWithView holder)> callback,
      const base::Location& from_here = FROM_HERE) const;

  const std::string& Path() const;

  void AddDocumentObserver(int event_id, DocumentEventObserver* observer);
  void RemoveDocumentObserver(int event_id, DocumentEventObserver* observer);
  void RemoveDocumentObservers(DocumentEventObserver* observer);
  void RemoveDocumentObservers();

 private:
  int view_id_ = -1;
  scoped_refptr<DocumentHolder> holder_;
  bool deregisters_callback_ = false;

  size_t PtrToId();
};

struct DocumentCallbackContext {
 public:
  DocumentCallbackContext(const size_t id_,
                          const int view_id_,
                          const void* office_instance_)
      : id(id_), view_id(view_id_), office_instance(office_instance_) {}

  const size_t id;
  const int view_id;
  const void* office_instance;
};
}  // namespace electron::office

