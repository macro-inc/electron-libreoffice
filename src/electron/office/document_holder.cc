// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.
#include "office/document_holder.h"
#include <vector>
#include "LibreOfficeKit/LibreOfficeKit.hxx"
#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "office/office_instance.h"

namespace electron::office {

DocumentHolder::DocumentHolder(lok::Document* owned_document,
                               const std::string& path)
    : base::RefCountedDeleteOnSequence<DocumentHolder>(
          base::SequencedTaskRunnerHandle::Get()),
      path_(path),
      doc_(owned_document) {}

DocumentHolder::~DocumentHolder() = default;

// holder constructor
DocumentHolderWithView::DocumentHolderWithView(
    const scoped_refptr<DocumentHolder>& holder)
    : holder_(holder) {
  auto& owned_document = holder_->doc_;
  int count = owned_document->getViewsCount();
  if (count == 0) {
    view_id_ = owned_document->createView();
  } else if (holder_->HasOneRef()) {
    CHECK(count == 1);
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
      new DocumentCallbackContext(PtrToId(), view_id_, OfficeInstance::Get()));
  deregisters_callback_ = true;
}

// pointer constructor
DocumentHolderWithView::DocumentHolderWithView(lok::Document* owned_document,
                                               const std::string& path)
    : DocumentHolderWithView(
          base::MakeRefCounted<DocumentHolder>(owned_document, path)) {}

// move assignment
DocumentHolderWithView& DocumentHolderWithView::operator=(
    DocumentHolderWithView&& other) noexcept {
  if (this != &other) {
    holder_ = std::move(other.holder_);
    view_id_ = other.view_id_;
    other.deregisters_callback_ = false;
  }
  return *this;
}

// move constructor
DocumentHolderWithView::DocumentHolderWithView(
    DocumentHolderWithView&& other) noexcept
    : view_id_(other.view_id_),
      holder_(std::move(other.holder_)),
      deregisters_callback_(other.deregisters_callback_) {
  other.deregisters_callback_ = false;
}

// copy constructor
DocumentHolderWithView::DocumentHolderWithView(
    const DocumentHolderWithView& other)
    : view_id_(other.view_id_), holder_(other.holder_) {}

// copy assignment
DocumentHolderWithView& DocumentHolderWithView::operator=(
    const DocumentHolderWithView& other) {
  if (this != &other) {
    holder_ = other.holder_;
    view_id_ = other.view_id_;
    deregisters_callback_ = false;
  }
  return *this;
}

scoped_refptr<DocumentHolder> DocumentHolderWithView::holder() const {
  return holder_;
}

int DocumentHolderWithView::ViewId() const {
  return view_id_;
}

void DocumentHolderWithView::SetAsCurrentView() const {
  CHECK(view_id_ > -1);
  holder_->doc_->setView(view_id_);
}

lok::Document& DocumentHolderWithView::operator*() const {
  DCHECK(holder_);
  SetAsCurrentView();

  return *holder_->doc_;
}

lok::Document* DocumentHolderWithView::operator->() const {
  DCHECK(holder_);

  SetAsCurrentView();
  return holder_->doc_.get();
}

DocumentHolderWithView::operator bool() const {
  return holder_ && holder_->doc_;
}

bool DocumentHolderWithView::operator==(
    const DocumentHolderWithView& other) const {
  return view_id_ == other.view_id_ && holder_ == other.holder_;
}

bool DocumentHolderWithView::operator!=(
    const DocumentHolderWithView& other) const {
  return !(*this == other);
}

DocumentHolderWithView::DocumentHolderWithView() = default;

DocumentHolderWithView::~DocumentHolderWithView() {
  if (!deregisters_callback_)
    return;

  SetAsCurrentView();
  holder_->doc_->registerCallback(nullptr, nullptr);
}

DocumentHolderWithView DocumentHolderWithView::NewView() {
  return DocumentHolderWithView(holder_);
}

const std::string& DocumentHolderWithView::Path() const {
  return holder_->path_;
}

void DocumentHolderWithView::Post(
    base::OnceCallback<void(DocumentHolderWithView holder)> callback,
    const base::Location& from_here) const {
  holder_->owning_task_runner()->PostTask(
      from_here, base::BindOnce(std::move(callback), *this));
}

void DocumentHolderWithView::Post(
    base::RepeatingCallback<void(DocumentHolderWithView holder)> callback,
    const base::Location& from_here) const {
  holder_->owning_task_runner()->PostTask(
      from_here, base::BindOnce(std::move(callback), *this));
}

void DocumentHolderWithView::PostBlocking(
    base::OnceCallback<void(DocumentHolderWithView holder)> callback,
    const base::Location& from_here) const {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(std::move(callback), *this));
}

void DocumentHolderWithView::AddDocumentObserver(
    int event_id,
    DocumentEventObserver* observer) {
  DCHECK(OfficeInstance::IsValid());
  OfficeInstance::Get()->AddDocumentObserver({PtrToId(), event_id, view_id_},
                                             observer);
}

void DocumentHolderWithView::RemoveDocumentObserver(
    int event_id,
    DocumentEventObserver* observer) {
  DCHECK(OfficeInstance::IsValid());
  OfficeInstance::Get()->RemoveDocumentObserver({PtrToId(), event_id, view_id_},
                                                observer);
}
void DocumentHolderWithView::RemoveDocumentObservers(
    DocumentEventObserver* observer) {
  DCHECK(OfficeInstance::IsValid());
  OfficeInstance::Get()->RemoveDocumentObservers(PtrToId(), observer);
}

void DocumentHolderWithView::RemoveDocumentObservers() {
  DCHECK(OfficeInstance::IsValid());
  OfficeInstance::Get()->RemoveDocumentObservers(PtrToId());
}

size_t DocumentHolderWithView::PtrToId() {
  return holder_ ? (size_t)holder_->doc_.get() : 0;
}

}  // namespace electron::office
