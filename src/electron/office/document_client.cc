// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/document_client.h"

#include <string_view>
#include <vector>
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
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
#include "office/event_bus.h"
#include "office/lok_callback.h"
#include "office/office_client.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/libreofficekit/LibreOfficeKitEnums.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
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
      .SetLazyDataProperty("isReady", &DocumentClient::IsReady);
}

const char* DocumentClient::GetTypeName() {
  return "DocumentClient";
}

bool DocumentClient::IsReady() {
  return is_ready_;
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

int DocumentClient::Mount(v8::Isolate* isolate) {
  if (view_id_ != -1) {
    return view_id_;
  }

  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Value> wrapper;
    if (this->GetWrapper(isolate).ToLocal(&wrapper)) {
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
void DocumentClient::PageOffsetUpdated(const gfx::Vector2d& page_offset) {
  page_offest_ = page_offset;
}
void DocumentClient::PluginSizeUpdated(const gfx::Size& size) {
  visible_area_ = size;
}

void DocumentClient::PrePaint() {
  DCHECK(document_);
  document_->setView(view_id_);
}
void DocumentClient::Paint(const gfx::Rect& rect,
                           SkBitmap& image_data,
                           std::vector<gfx::Rect>& ready,
                           std::vector<gfx::Rect>& pending) {
  // TODO: progressive renders in cancellable sequence task runner
  gfx::Rect dirty_rect_twips_ =
      gfx::ScaleToEnclosedRect(rect, lok_callback::kTwipPerPx);
  constexpr int scale_factor = 1;
  constexpr int tile_size_px = 256;

  int tile_size_px_scaled = tile_size_px * scale_factor;
  int nTileSizePixelsScaledTwips =
      tile_size_px_scaled * lok_callback::kTwipPerPx;
  long doc_width_scaled = document_size_px_.width() * scale_factor;
  long doc_height_scaled = document_size_px_.height() * scale_factor;

  // Total number of rows / columns in this document.
  int rows = ceil(static_cast<double>(doc_height_scaled) / tile_size_px_scaled);
  int columns =
      ceil(static_cast<double>(doc_width_scaled) / tile_size_px_scaled);

  SkCanvas canvas(image_data);

  // Render the tiles.
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      gfx::Rect tile_rect_twips, tile_rect_px;

      // Determine size of the tile: the rightmost/bottommost tiles may
      // be smaller, and we need the size to decide if we need to repaint.
      if (column == columns - 1)
        tile_rect_px.set_width(doc_width_scaled - column * tile_size_px_scaled);
      else
        tile_rect_px.set_width(tile_size_px_scaled);
      if (row == rows - 1)
        tile_rect_px.set_height(doc_height_scaled - row * tile_size_px_scaled);
      else
        tile_rect_px.set_height(tile_size_px_scaled);

      // Determine size and position of the tile in document coordinates,
      // so we can decide if we can skip painting for partial rendering.
      tile_rect_twips = gfx::Rect(
          lok_callback::PixelToTwip(tile_size_px_scaled, zoom_) * column,
          lok_callback::PixelToTwip(tile_size_px_scaled, zoom_) * row,
          lok_callback::PixelToTwip(tile_rect_px.width(), zoom_),
          lok_callback::PixelToTwip(tile_rect_px.height(), zoom_));

      if (dirty_rect_twips_.Intersects(tile_rect_twips)) {
        SkImageInfo image_info = SkImageInfo::Make(
            tile_size_px_scaled, tile_size_px_scaled,
            tile_mode_ == LOK_TILEMODE_BGRA ? kBGRA_8888_SkColorType
                                            : kRGBA_8888_SkColorType,
            kPremul_SkAlphaType);
        SkBitmap tile;
        uint8_t* pixels = static_cast<uint8_t*>(
            calloc(tile_size_px_scaled * tile_size_px_scaled, 4));

        document_->paintPartTile(
            pixels, 0, tile_size_px_scaled, tile_size_px_scaled,
            tile_rect_twips.x(), tile_rect_twips.y(),
            nTileSizePixelsScaledTwips, nTileSizePixelsScaledTwips);

        tile.installPixels(
            image_info, pixels, image_info.minRowBytes(),
            [](void* addr, void* context) {
              if (addr)
                free(addr);
            },
            nullptr);
        canvas.drawImage(tile.asImage(), tile_size_px_scaled * column,
                         tile_size_px_scaled * row);
      }
    }
  }

  ready.emplace_back(rect);
}
void DocumentClient::PostPaint() {}
int DocumentClient::GetNumberOfPages() const {
  return document_->getParts();
}
gfx::Rect DocumentClient::GetPageScreenRect(int page_index) const {
  if ((size_t)page_index >= page_rects_.size())
    return gfx::Rect();

  return page_rects_[page_index];
}
void DocumentClient::ZoomUpdated(double new_zoom_level) {}
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
