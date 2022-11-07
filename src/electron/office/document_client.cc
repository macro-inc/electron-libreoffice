// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/document_client.h"

#include <iterator>
#include <string_view>
#include <vector>
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_runner_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "include/core/SkAlphaType.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorType.h"
#include "include/core/SkPaint.h"
#include "net/base/filename_util.h"
#include "office/event_bus.h"
#include "office/lok_callback.h"
#include "office/office_client.h"
#include "shell/common/gin_converters/gfx_converter.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/libreofficekit/LibreOfficeKitEnums.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "url/gurl.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"

namespace electron::office {
gin::WrapperInfo DocumentClient::kWrapperInfo = {gin::kEmbedderNativeGin};

DocumentClient::DocumentClient(lok::Document* document,
                               std::string path,
                               EventBus* event_bus)
    : document_(document), path_(path), event_bus_(event_bus) {
  // assumes the document loaded succesfully from OfficeClient
  DCHECK(document_);

  event_bus->Handle(LOK_CALLBACK_DOCUMENT_SIZE_CHANGED,
                    base::BindRepeating(&DocumentClient::HandleDocSizeChanged,
                                        base::Unretained(this)));
  event_bus->Handle(LOK_CALLBACK_INVALIDATE_TILES,
                    base::BindRepeating(&DocumentClient::HandleInvalidate,
                                        base::Unretained(this)));
  event_bus->Handle(LOK_CALLBACK_STATE_CHANGED,
                    base::BindRepeating(&DocumentClient::HandleStateChange,
                                        base::Unretained(this)));
}

DocumentClient::~DocumentClient() {
  LOG(ERROR) << "DOC CLIENT DESTROYED";
}

// gin::Wrappable
gin::ObjectTemplateBuilder DocumentClient::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::FunctionTemplate> constructor =
      data->GetFunctionTemplate(&kWrapperInfo);
  if (constructor.IsEmpty()) {
    constructor = v8::FunctionTemplate::New(isolate);
    constructor->SetClassName(gin::StringToV8(isolate, GetTypeName()));
    constructor->ReadOnlyPrototype();
    data->SetFunctionTemplate(&kWrapperInfo, constructor);
  }
  return event_bus_
      ->Extend(gin::ObjectTemplateBuilder(isolate, GetTypeName(),
                                          constructor->InstanceTemplate()))
      .SetMethod("twipToPx", &DocumentClient::TwipToPx)
      .SetLazyDataProperty("pageRects", &DocumentClient::PageRects)
      .SetLazyDataProperty("size", &DocumentClient::Size)
      .SetLazyDataProperty("isReady", &DocumentClient::IsReady);
}

const char* DocumentClient::GetTypeName() {
  return "DocumentClient";
}

bool DocumentClient::IsReady() const {
  return is_ready_;
}

std::vector<gfx::Rect> DocumentClient::PageRects() const {
  std::vector<gfx::Rect> result;
  float zoom = zoom_;  // want CSS pixels, which are already scaled to the
                       // device, so don't use TotalScale
  std::transform(page_rects_.begin(), page_rects_.end(),
                 std::back_inserter(result), [zoom](const gfx::Rect& rect) {
                   float scale = zoom / lok_callback::kTwipPerPx;
                   return gfx::Rect(
                       gfx::ScaleToCeiledPoint(rect.origin(), scale),
                       gfx::ScaleToCeiledSize(rect.size(), scale));
                 });
  return result;
}

gfx::Size DocumentClient::Size() const {
  return gfx::ToCeiledSize(document_size_px_);
}

// actually TwipTo_CSS_Px, since the pixels are device-indpendent
float DocumentClient::TwipToPx(float in) const {
  return lok_callback::TwipToPixel(in, zoom_);
}

lok::Document* DocumentClient::GetDocument() {
  return document_;
}

gfx::Size DocumentClient::DocumentSizeTwips() {
  return gfx::Size(document_width_in_twips_, document_height_in_twips_);
}

gfx::SizeF DocumentClient::DocumentSizePx() {
  return document_size_px_;
}

namespace {
void SaveBackup(const std::string& path) {
  GURL url(path);
  base::FilePath file_path;
  if (!url.SchemeIsFile()) {
    LOG(ERROR) << "Unable to make backup. Expected a URL as a path, but got: "
               << path;
    return;
  }
  if (!net::FileURLToFilePath(url, &file_path)) {
    LOG(ERROR) << "Unable to make backup. Unable to make file URL into path: "
               << path;
    return;
  }

  auto now = base::Time::Now();
  base::Time::Exploded exploded;
  now.UTCExplode(&exploded);
  // ex: .bak.2022-11-07-154601
  auto backup_suffix =
      base::StringPrintf(FILE_PATH_LITERAL(".bak.%04d-%02d-%02d-%02d%02d%02d"),
                         exploded.year, exploded.month, exploded.day_of_month,
                         exploded.hour, exploded.minute, exploded.second);

  base::FilePath backup_path = file_path.InsertBeforeExtension(backup_suffix);

  base::CopyFile(file_path, backup_path);
}
}  // namespace

int DocumentClient::Mount(v8::Isolate* isolate) {
  if (view_id_ != -1) {
    return view_id_;
  }

  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Value> wrapper;
    if (this->GetWrapper(isolate).ToLocal(&wrapper)) {
      event_bus_->SetContext(isolate, isolate->GetCurrentContext());
      // prevent garbage collection
      mounted_.Reset(isolate, wrapper);
    }
  }

  if (OfficeClient::GetInstance()->MarkMounted(document_)) {
    view_id_ = document_->getView();
  } else {
    view_id_ = document_->createView();
  }

  RefreshSize();

  tile_mode_ = static_cast<LibreOfficeKitTileMode>(document_->getTileMode());

  // emit the state change buffer when ready
  v8::Local<v8::Array> ready_value =
      v8::Array::New(isolate, state_change_buffer_.size());
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  if (!state_change_buffer_.empty()) {
    for (size_t i = 0; i < state_change_buffer_.size(); i++) {
      // best effort
      std::ignore = ready_value->Set(
          context, i,
          lok_callback::PayloadToLocalValue(isolate, LOK_CALLBACK_STATE_CHANGED,
                                            state_change_buffer_[i].c_str()));
    }

    state_change_buffer_.clear();
  }

  // save a backup before we continue
  SaveBackup(path_);

  event_bus_->Emit("ready", ready_value);

  return view_id_;
}

void DocumentClient::Unmount() {
  // not mounted
  if (view_id_ == -1)
    return;

  if (document_->getViewsCount()) {
    document_->destroyView(view_id_);
    view_id_ = -1;
  }

  // allow garbage collection
  mounted_.Reset();
}

// Plugin Engine {
void DocumentClient::BrowserZoomUpdated(double new_zoom_level) {
  view_zoom_ = new_zoom_level;
  RefreshSize();
}

int DocumentClient::GetNumberOfPages() const {
  return document_->getParts();
}
//}

// Editing State {
bool DocumentClient::CanCopy() {
  auto res = uno_state_.find(".uno:Copy");
  return res != uno_state_.end() && res->second == "enabled";
}

bool DocumentClient::CanUndo() {
  auto res = uno_state_.find(".uno:Undo");
  return res != uno_state_.end() && res->second == "enabled";
}
bool DocumentClient::CanRedo() {
  auto res = uno_state_.find(".uno:Redo");
  return res != uno_state_.end() && res->second == "enabled";
}

// TODO: read only mode?
bool DocumentClient::CanEditText() {
  return true;
}
bool DocumentClient::HasEditableText() {
  return true;
}
// }

base::WeakPtr<DocumentClient> DocumentClient::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DocumentClient::HandleStateChange(std::string payload) {
  std::pair<std::string, std::string> pair =
      lok_callback::ParseStatusChange(payload);
  if (!pair.first.empty()) {
    uno_state_.insert(pair);
  }

  if (view_id_ == -1 || !is_ready_) {
    state_change_buffer_.emplace_back(payload);
  }
}

void DocumentClient::HandleDocSizeChanged(std::string payload) {
  RefreshSize();
}

void DocumentClient::HandleInvalidate(std::string payload) {
  is_ready_ = true;
}

void DocumentClient::RefreshSize() {
  // not mounted
  if (view_id_ == -1 || !is_ready_)
    return;

  document_->getDocumentSize(&document_width_in_twips_,
                             &document_height_in_twips_);

  float zoom = zoom_;
  document_size_px_ =
      gfx::SizeF(lok_callback::TwipToPixel(document_width_in_twips_, zoom),
                 lok_callback::TwipToPixel(document_height_in_twips_, zoom));

  std::string_view page_rect_sv(document_->getPartPageRectangles());
  std::string_view::const_iterator start = page_rect_sv.begin();
  int new_size = GetNumberOfPages();
  // TODO: is there a better way, like updating a page only when it changes?
  page_rects_ =
      lok_callback::ParseMultipleRects(start, page_rect_sv.end(), new_size);
}

DocumentClient::DocumentClient() = default;
}  // namespace electron::office
