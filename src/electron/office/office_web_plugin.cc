// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/office_web_plugin.h"

#include <chrono>
#include <memory>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "include/core/SkColor.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkTextBlob.h"
#include "office/document_client.h"
#include "office/event_bus.h"
#include "office/lok_callback.h"
#include "office/lok_tilebuffer.h"
#include "office/office_client.h"
#include "office/office_keys.h"
#include "shell/common/gin_converters/gfx_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/function_template_extensions.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/libreofficekit/LibreOfficeKitEnums.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"

namespace electron {

namespace office {
extern const char kInternalPluginMimeType[] = "application/x-libreoffice";

blink::WebPlugin* CreateInternalPlugin(blink::WebPluginParams params,
                                       content::RenderFrame* render_frame) {
  return new OfficeWebPlugin(params, render_frame);
}

}  // namespace office

using Modifiers = blink::WebInputEvent::Modifiers;

OfficeWebPlugin::OfficeWebPlugin(blink::WebPluginParams params,
                                 content::RenderFrame* render_frame)
    : render_frame_(render_frame),
      task_runner_(render_frame->GetTaskRunner(
          blink::TaskType::kInternalMediaRealTime)){};

// blink::WebPlugin {
bool OfficeWebPlugin::Initialize(blink::WebPluginContainer* container) {
  container_ = container;

  // TODO: figure out what false means?
  return true;
}

void OfficeWebPlugin::Destroy() {
  if (container_) {
    // TODO: release client container value
  }
  if (document_client_) {
    document_client_->Unmount();
  }
  delete this;
}

v8::Local<v8::Object> OfficeWebPlugin::V8ScriptableObject(
    v8::Isolate* isolate) {
  gin_helper::Dictionary dict = gin::Dictionary::CreateEmpty(isolate);
  dict.SetMethod("renderDocument",
                 base::BindRepeating(&OfficeWebPlugin::RenderDocument,
                                     base::Unretained(this)));
  dict.SetMethod("updateScroll",
                 base::BindRepeating(&OfficeWebPlugin::UpdateScrollInTask,
                                     base::Unretained(this)));
  return dict.GetHandle();
}

blink::WebPluginContainer* OfficeWebPlugin::Container() const {
  return container_;
}

bool OfficeWebPlugin::SupportsKeyboardFocus() const {
  return true;
}

void OfficeWebPlugin::UpdateAllLifecyclePhases(
    blink::DocumentUpdateReason reason) {}

void OfficeWebPlugin::Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);

  SkRect invalidate_rect =
      gfx::RectToSkRect(gfx::IntersectRects(css_plugin_rect_, rect));
  cc::PaintCanvasAutoRestore auto_restore(canvas, true);

  canvas->clipRect(invalidate_rect);

  // nothing drawn yet
  if (first_paint_) {
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setColor(SK_ColorTRANSPARENT);

    canvas->drawRect(invalidate_rect, flags);
  }

  // not mounted
  if (!document_client_)
    return;

  if (!plugin_rect_.origin().IsOrigin())
    canvas->translate(plugin_rect_.x(), plugin_rect_.y());

  document_->setView(view_id_);

  part_tile_buffer_.at(0).Paint(canvas, rect);
}

void OfficeWebPlugin::UpdateGeometry(const gfx::Rect& window_rect,
                                     const gfx::Rect& clip_rect,
                                     const gfx::Rect& unobscured_rect,
                                     bool is_visible) {
  // nothing to render inside of
  if (window_rect.IsEmpty())
    return;

  // get the document root frame's scale factor so that any widget scaling does
  // not affect the device scale
  blink::WebWidget* widget =
      container_->GetDocument().GetFrame()->LocalRoot()->FrameWidget();
  OnViewportChanged(window_rect,
                    widget->GetOriginalScreenInfo().device_scale_factor);
}

void OfficeWebPlugin::UpdateFocus(bool focused,
                                  blink::mojom::FocusType focus_type) {
  if (has_focus_ != focused) {
    // TODO: update focus, input state, and selection bounds
  }

  has_focus_ = focused;
}

void OfficeWebPlugin::UpdateVisibility(bool visibility) {}

blink::WebInputEventResult OfficeWebPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& event,
    ui::Cursor* cursor) {
  blink::WebInputEvent::Type event_type = event.Event().GetType();

  // TODO: handle gestures
  if (blink::WebInputEvent::IsGestureEventType(event_type))
    return blink::WebInputEventResult::kNotHandled;

  if (container_ && container_->WasTargetForLastMouseEvent()) {
    *cursor = cursor_type_;
  }

  if (blink::WebInputEvent::IsKeyboardEventType(event_type))
    return HandleKeyEvent(
        std::move(static_cast<const blink::WebKeyboardEvent&>(event.Event())),
        cursor);

  std::unique_ptr<blink::WebInputEvent> transformed_event =
      ui::TranslateAndScaleWebInputEvent(
          event.Event(), gfx::Vector2dF(-available_area_.x(), 0), 1.0);

  const blink::WebInputEvent& event_to_handle =
      transformed_event ? *transformed_event : event.Event();

  switch (event_type) {
    case blink::WebInputEvent::Type::kMouseDown:
    case blink::WebInputEvent::Type::kMouseUp:
    case blink::WebInputEvent::Type::kMouseMove:
      break;
    default:
      return blink::WebInputEventResult::kNotHandled;
  }

  blink::WebMouseEvent mouse_event =
      static_cast<const blink::WebMouseEvent&>(event_to_handle);

  int modifiers = mouse_event.GetModifiers();
  return HandleMouseEvent(event_type, mouse_event.PositionInWidget(), modifiers,
                          mouse_event.ClickCount(), cursor)
             ? blink::WebInputEventResult::kHandledApplication
             : blink::WebInputEventResult::kNotHandled;
}

blink::WebInputEventResult OfficeWebPlugin::HandleKeyEvent(
    const blink::WebKeyboardEvent event,
    ui::Cursor* cursor) {
  if (!document_ || view_id_ == -1)
    return blink::WebInputEventResult::kNotHandled;

  blink::WebInputEvent::Type type = event.GetType();

  // supress scroll event for any containers when pressing space
  if (type == blink::WebInputEvent::Type::kChar &&
      event.dom_code == office::DomCode::SPACE) {
    return blink::WebInputEventResult::kHandledApplication;
  }

  // only handle provided key events
  switch (type) {
    case blink::WebInputEvent::Type::kRawKeyDown:
    case blink::WebInputEvent::Type::kKeyUp:
      break;
    default:
      return blink::WebInputEventResult::kNotHandled;
  }

  // intercept some special key events on Ctr/Command
  if (event.GetModifiers() & (event.kControlKey | event.kMetaKey)) {
    switch (event.dom_code) {
      // don't close the internal LO window
      case office::DomCode::US_W:
        return blink::WebInputEventResult::kNotHandled;
      case office::DomCode::US_C:
        return HandleCopyEvent();
      case office::DomCode::US_V:
        return HandlePasteEvent();
    }
  }

  int lok_key_code =
      office::DOMKeyCodeToLOKKeyCode(event.dom_code, event.GetModifiers());

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&lok::Document::setView,
                                        base::Unretained(document_), view_id_));
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&lok::Document::postKeyEvent, base::Unretained(document_),
                     type == blink::WebInputEvent::Type::kKeyUp
                         ? LOK_KEYEVENT_KEYUP
                         : LOK_KEYEVENT_KEYINPUT,
                     event.text[0], lok_key_code));

  return blink::WebInputEventResult::kHandledApplication;
}

blink::WebInputEventResult OfficeWebPlugin::HandleCopyEvent() {
  document_client_->PostUnoCommandInternal(".uno:Copy", nullptr, true);
  return blink::WebInputEventResult::kHandledApplication;
}

blink::WebInputEventResult OfficeWebPlugin::HandlePasteEvent() {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::vector<std::u16string> res =
      clipboard->ReadAvailableStandardAndCustomFormatNames(
          ui::ClipboardBuffer::kCopyPaste, nullptr);

  std::string clipboard_type = "";

  for (std::u16string r : res) {
    if (r == u"text/plain") {
      clipboard_type = "text/plain;charset=utf-8";
      break;
    } else if (r == u"image/png") {
      clipboard_type = "image/png";
      break;
    }
  }

  if (!document_client_->OnPasteEvent(clipboard, clipboard_type)) {
    LOG(ERROR) << "Failed to set lok clipboard";
    return blink::WebInputEventResult::kNotHandled;
  }

  return blink::WebInputEventResult::kHandledApplication;
}

bool OfficeWebPlugin::HandleMouseEvent(blink::WebInputEvent::Type type,
                                       gfx::PointF position,
                                       int modifiers,
                                       int clickCount,
                                       ui::Cursor* cursor) {
  if (!document_ || view_id_ == -1)
    return false;

  LibreOfficeKitMouseEventType event_type;
  switch (type) {
    case blink::WebInputEvent::Type::kMouseDown:
      event_type = LOK_MOUSEEVENT_MOUSEBUTTONDOWN;
      break;
    case blink::WebInputEvent::Type::kMouseUp:
      event_type = LOK_MOUSEEVENT_MOUSEBUTTONUP;
      break;
    case blink::WebInputEvent::Type::kMouseMove:
      event_type = LOK_MOUSEEVENT_MOUSEMOVE;
      break;
    default:
      return false;
  }

  // allow focus even if not in area
  if (!available_area_.Contains(gfx::ToCeiledPoint(position))) {
    return event_type == LOK_MOUSEEVENT_MOUSEBUTTONDOWN;
  }

  // offset by the scroll position
  position.Offset(0, input_y_offset);

  // TODO: handle offsets
  gfx::Point pos = gfx::ToRoundedPoint(gfx::ScalePoint(
      position,
      office::lok_callback::kTwipPerPx / document_client_->TotalScale()));

  int buttons = 0;
  if (modifiers & blink::WebInputEvent::kLeftButtonDown)
    buttons |= 1;
  if (modifiers & blink::WebInputEvent::kMiddleButtonDown)
    buttons |= 2;
  if (modifiers & blink::WebInputEvent::kRightButtonDown)
    buttons |= 4;

  if (buttons > 0) {
    document_->postMouseEvent(event_type, pos.x(), pos.y(), clickCount, buttons,
                              office::EventModifiersToLOKModifiers(modifiers));
    return true;
  }

  return false;
}

void OfficeWebPlugin::DidReceiveResponse(
    const blink::WebURLResponse& response) {}

// no-op
void OfficeWebPlugin::DidReceiveData(const char* data, size_t data_length) {}
void OfficeWebPlugin::DidFinishLoading() {}
void OfficeWebPlugin::DidFailLoading(const blink::WebURLError& error) {}

bool OfficeWebPlugin::CanEditText() const {
  return true;
}

bool OfficeWebPlugin::HasEditableText() const {
  return true;
}

bool OfficeWebPlugin::CanUndo() const {
  return document_client_ && document_client_->CanUndo();
}

bool OfficeWebPlugin::CanRedo() const {
  return document_client_ && document_client_->CanRedo();
}

content::RenderFrame* OfficeWebPlugin::render_frame() const {
  return render_frame_;
}

OfficeWebPlugin::~OfficeWebPlugin() = default;

void OfficeWebPlugin::InvalidatePluginContainer() {
  if (container_)
    container_->Invalidate();
}

namespace {
void ResetTileBuffers(std::vector<office::TileBuffer>& buffers,
                      lok::Document* document,
                      int parts,
                      float scale) {
  // TODO: handle case where buffer exceeds the number of parts
  int missing = parts - buffers.size();
  if (missing < 0)
    missing = 0;

  for (int part = 0; part < missing; ++part) {
    buffers.emplace_back(document, scale, part);
  }

  parts -= missing;
  for (int part = 0; part < parts; ++part) {
    buffers[part] = std::move(office::TileBuffer(document, scale, part));
    LOG(ERROR) << "BUFFER RESET SCALE" << scale;
  }
}
}  // namespace

void OfficeWebPlugin::OnGeometryChanged(double old_zoom,
                                        float old_device_scale) {
  if (!document_client_)
    return;

  if (viewport_zoom_ != old_zoom || device_scale_ != old_device_scale) {
    document_client_->BrowserZoomUpdated(viewport_zoom_ * device_scale_);
    ResetTileBuffers(part_tile_buffer_, document_,
                     // there is only one tile buffer for text documents
                     document_->getDocumentType() == LOK_DOCTYPE_TEXT
                         ? 1
                         : document_->getParts(),
                     document_client_->TotalScale());
  }

  available_area_ = gfx::Rect(plugin_rect_.size());
  gfx::Size doc_size = GetDocumentPixelSize();
  if (doc_size.width() < available_area_.width()) {
    available_area_.set_width(doc_size.width());
  }

  // The distance between top of the plugin and the bottom of the document in
  // pixels.
  int bottom_of_document = doc_size.height();
  if (bottom_of_document < plugin_rect_.height())
    available_area_.set_height(bottom_of_document);

  available_area_twips_ = gfx::ScaleToEnclosingRect(
      available_area_, office::lok_callback::kTwipPerPx);
}

gfx::Size OfficeWebPlugin::GetDocumentPixelSize() const {
  return gfx::ToCeiledSize(gfx::ScaleSize(document_client_->DocumentSizePx(),
                                          viewport_zoom_ * device_scale_));
}

void OfficeWebPlugin::OnViewportChanged(
    const gfx::Rect& plugin_rect_in_css_pixel,
    float new_device_scale) {
  DCHECK_GT(new_device_scale, 0.0f);

  css_plugin_rect_ = plugin_rect_in_css_pixel;

  if (new_device_scale == device_scale_ &&
      plugin_rect_in_css_pixel == plugin_rect_) {
    return;
  }

  const float old_device_scale = device_scale_;
  device_scale_ = new_device_scale;
  plugin_rect_ = plugin_rect_in_css_pixel;

  OnGeometryChanged(viewport_zoom_, old_device_scale);
}

void OfficeWebPlugin::HandleInvalidateTiles(std::string payload) {
  // not mounted
  if (view_id_ == -1)
    return;

  std::string_view payload_sv(payload);

  // TODO: handle non-text document types for parts
  if (payload_sv == "EMPTY") {
    part_tile_buffer_.at(0).InvalidateAllTiles();
    InvalidatePluginContainer();
  } else {
    std::string_view::const_iterator start = payload_sv.begin();
    gfx::Rect dirty_rect =
        office::lok_callback::ParseRect(start, payload_sv.end());

    if (dirty_rect.IsEmpty())
      return;
    part_tile_buffer_.at(0).InvalidateTilesInTwipRect(dirty_rect);
    InvalidatePluginContainer();
  }
}

void OfficeWebPlugin::HandleDocumentSizeChanged(std::string payload) {
  ResetTileBuffers(part_tile_buffer_, document_,
                   // there is only one tile buffer for text documents
                   document_->getDocumentType() == LOK_DOCTYPE_TEXT
                       ? 1
                       : document_->getParts(),
                   document_client_->TotalScale());
}

void OfficeWebPlugin::UpdateScrollInTask(int y_position) {
  if (task_runner_)
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&OfficeWebPlugin::UpdateScroll,
                                          base::Unretained(this), y_position));
}

void OfficeWebPlugin::UpdateScroll(int y_position) {
  if (!document_client_ || stop_scrolling_)
    return;

  float max_y = std::max(
      document_client_->DocumentSizePx().height() - plugin_rect_.height() / device_scale_,
      0.0f);

  float scaled_y = base::clamp((float)y_position, 0.0f, max_y) * device_scale_;
  part_tile_buffer_.at(0).SetYPosition(scaled_y);
  input_y_offset = scaled_y;

  InvalidatePluginContainer();
}

bool OfficeWebPlugin::RenderDocument(
    v8::Isolate* isolate,
    gin::Handle<office::DocumentClient> client) {
  if (client.IsEmpty()) {
    LOG(ERROR) << "invalid document client";
    return false;
  }
  office::OfficeClient* office = office::OfficeClient::GetInstance();

  // TODO: honestly, this is terrible, need to do this properly
  // already mounted
  bool needs_reset = view_id_ != -1;
  if (needs_reset) {
    part_tile_buffer_.clear();
    office->CloseDocument(document_client_->Path());
    document_client_->Unmount();
    delete document_;
  }

  document_ = client->GetDocument();
  document_client_ = client.get();
  view_id_ = client->Mount(isolate);
  document_client_->BrowserZoomUpdated(viewport_zoom_ * device_scale_);
  part_tile_buffer_.emplace_back(document_, document_client_->TotalScale());

  if (needs_reset) {
    // this is an awful hack
    auto device_scale = device_scale_;
    device_scale_ = 0;
    OnViewportChanged(css_plugin_rect_, device_scale);
  }

  document_->setViewLanguage(view_id_, "en-US");
  document_->setView(view_id_);
  document_->resetSelection();

  office->HandleDocumentEvent(
      document_, LOK_CALLBACK_INVALIDATE_TILES,
      base::BindRepeating(&OfficeWebPlugin::HandleInvalidateTiles,
                          base::Unretained(this)));
  office->HandleDocumentEvent(
      document_, LOK_CALLBACK_DOCUMENT_SIZE_CHANGED,
      base::BindRepeating(&OfficeWebPlugin::HandleDocumentSizeChanged,
                          base::Unretained(this)));

  if (needs_reset) {
    OnGeometryChanged(viewport_zoom_, device_scale_);
  }
  TriggerFullRerender();
  return true;
}

void OfficeWebPlugin::TriggerFullRerender() {
  // OnGeometryChanged(viewport_zoom_, device_scale_);
  if (document_client_ && !document_client_->DocumentSizePx().IsEmpty()) {
    part_tile_buffer_.at(0).InvalidateAllTiles();
    InvalidatePluginContainer();
  }
}

base::WeakPtr<OfficeWebPlugin> OfficeWebPlugin::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// } blink::WebPlugin

}  // namespace electron
