// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_DOCUMENT_HOLDER_H_
#define OFFICE_DOCUMENT_HOLDER_H_

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"

namespace lok {
class Document;
}

namespace electron::office {
class DocumentHolder : public base::RefCountedDeleteOnSequence<DocumentHolder> {
 public:
  explicit DocumentHolder(lok::Document* owned_document);

 private:
  std::unique_ptr<lok::Document> owned_document_;
  friend class base::RefCountedDeleteOnSequence<DocumentHolder>;
  friend class base::DeleteHelper<DocumentHolder>;
  friend class DocumentHolderWithView;
  ~DocumentHolder();
};

// A thread-safe document holder that deletes an lok::Document only when it no
// longer has references
class DocumentHolderWithView {
 public:
  explicit DocumentHolderWithView(const scoped_refptr<DocumentHolder>& holder);
  explicit DocumentHolderWithView(lok::Document* owned_document);

  DocumentHolderWithView NewView();

  int ViewId() const;
  // you probably don't need to do this
  void SetAsCurrentView() const;

  lok::Document& operator*() const;

  lok::Document* operator->() const;

 private:
  ~DocumentHolderWithView();
  int view_id_ = -1;
  const scoped_refptr<DocumentHolder> holder_;
};

struct DocumentIdWithViewId {
 public:
  DocumentIdWithViewId(const size_t id, const int view_id)
      : id_(id), view_id_(id) {}

  const size_t id_;
  const int view_id_;
};

}  // namespace electron::office

#endif  // OFFICE_DOCUMENT_HOLDER_H_
