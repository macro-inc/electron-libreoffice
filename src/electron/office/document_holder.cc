// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.
#include "office/document_holder.h"
#include <vector>
#include "LibreOfficeKit/LibreOfficeKit.hxx"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "office/office_instance.h"

namespace electron::office {

DocumentHolder::DocumentHolder(lok::Document* owned_document)
    : base::RefCountedDeleteOnSequence<DocumentHolder>(
          base::SequencedTaskRunnerHandle::Get()),
      owned_document_(owned_document) {}

DocumentHolder::~DocumentHolder() = default;

namespace {
inline size_t PtrToId(lok::Document* ptr) {
  return (size_t)ptr;
};
}  // namespace

DocumentHolderWithView::DocumentHolderWithView(
    const scoped_refptr<DocumentHolder>& holder)
    : holder_(holder) {
  auto& owned_document = holder_->owned_document_;
  int count = owned_document->getViewsCount();
  if (count == 0) {
    LOG(ERROR) << "Document didn't have count on construction";
    view_id_ = owned_document->createView();
  }
  if (holder_->HasOneRef()) {
    CHECK(count == 1);
    LOG(ERROR) << "First document ref!";
    // getting the current view is not reliable, so we get the list
    std::vector<int> ids(count);
    owned_document->getViewIds(ids.data(), count);
    view_id_ = ids[0];
  } else {
    view_id_ = owned_document->createView();
  }
  DCHECK(OfficeInstance::IsValid());
  SetAsCurrentView();
  owned_document->registerCallback(
      &OfficeInstance::HandleDocumentCallback,
      new DocumentIdWithViewId(PtrToId(holder_->owned_document_.get()),
                               view_id_));
}

int DocumentHolderWithView::ViewId() const {
  return view_id_;
}

void DocumentHolderWithView::SetAsCurrentView() const {
  CHECK(view_id_ > -1);
  holder_->owned_document_->setView(view_id_);
}

lok::Document& DocumentHolderWithView::operator*() const {
  SetAsCurrentView();

  return *holder_->owned_document_;
}

lok::Document* DocumentHolderWithView::operator->() const {
  SetAsCurrentView();

  return holder_->owned_document_.get();
}

DocumentHolderWithView::~DocumentHolderWithView() {
  SetAsCurrentView();

  holder_->owned_document_->registerCallback(nullptr, nullptr);
}

DocumentHolderWithView DocumentHolderWithView::NewView() {
  return DocumentHolderWithView(holder_);
}

}  // namespace electron::office
