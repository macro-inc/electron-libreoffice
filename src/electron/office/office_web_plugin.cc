// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/office_web_plugin.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "include/core/SkColor.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkTextBlob.h"
#include "office/event_bus.h"
#include "office/lok_callback.h"
#include "office/office_client.h"
#include "office/office_keys.h"
#include "shell/common/gin_helper/dictionary.h"
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
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
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
                                 content::RenderFrame* render_frame) {
  office_client_ = office::OfficeClient::GetInstance();
  office_ = office_client_->GetOffice();
  render_frame_ = render_frame;
};

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
  delete this;
}

v8::Local<v8::Object> OfficeWebPlugin::V8ScriptableObject(
    v8::Isolate* isolate) {
  gin_helper::Dictionary dict = gin::Dictionary::CreateEmpty(isolate);
  dict.SetReadOnlyNonConfigurable("office",
                                  gin::CreateHandle(isolate, office_client_));
  dict.SetMethod("renderDocument",
                 base::BindRepeating(&OfficeWebPlugin::RenderDocument,
                                     weak_factory_.GetWeakPtr()));
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
  SkRect invalidate_rect =
      gfx::RectToSkRect(gfx::IntersectRects(css_plugin_rect_, rect));
  cc::PaintCanvasAutoRestore auto_restore(canvas, true);
  canvas->clipRect(invalidate_rect);

  // nothing drawn yet
  if (snapshot_.GetSkImageInfo().isEmpty()) {
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kDst);
    flags.setColor(SK_ColorTRANSPARENT);

    canvas->drawRect(invalidate_rect, flags);
  }

  if (!total_translate_.IsZero())
    canvas->translate(total_translate_.x(), total_translate_.y());

  if (device_to_css_scale_ != 1.0f)
    canvas->scale(device_to_css_scale_, device_to_css_scale_);

  if (!plugin_rect_.origin().IsOrigin())
    canvas->translate(plugin_rect_.x(), plugin_rect_.y());

  if (snapshot_scale_ != 1.0f)
    canvas->scale(snapshot_scale_, snapshot_scale_);

  canvas->drawImage(snapshot_, 0, 0);
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
  std::unique_ptr<blink::WebInputEvent> scaled_event =
      ui::ScaleWebInputEvent(event.Event(), viewport_to_dip_scale_);
  const blink::WebInputEvent& event_to_handle =
      scaled_event ? *scaled_event : event.Event();

  auto event_type = event_to_handle.GetType();

  // TODO: handle gestures
  if (blink::WebInputEvent::IsGestureEventType(event_type))
    return blink::WebInputEventResult::kNotHandled;

  if (container_ && container_->WasTargetForLastMouseEvent()) {
    *cursor = cursor_type_;
  }

  if (blink::WebInputEvent::IsMouseEventType(event_type)) {
    return HandleMouseEvent(
               static_cast<const blink::WebMouseEvent&>(event_to_handle),
               cursor)
               ? blink::WebInputEventResult::kHandledApplication
               : blink::WebInputEventResult::kNotHandled;
  }

  if (blink::WebInputEvent::IsKeyboardEventType(event_type))
    return HandleKeyEvent(
               static_cast<const blink::WebKeyboardEvent&>(event_to_handle),
               cursor)
               ? blink::WebInputEventResult::kHandledApplication
               : blink::WebInputEventResult::kNotHandled;

  return blink::WebInputEventResult::kNotHandled;
}

bool OfficeWebPlugin::HandleKeyEvent(const blink::WebKeyboardEvent& event,
                                     ui::Cursor* cursor) {
  if (!document_client_ || view_id_ == -1)
    return false;

  // only handle provided key events
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kRawKeyDown:
    case blink::WebInputEvent::Type::kKeyUp:
      break;
    default:
      return false;
  }

  // intercept some special key events on Ctr/Command
  if (event.GetModifiers() & (event.kControlKey | event.kMetaKey)) {
    switch (event.dom_code) {
      // don't close the internal LO window
      case office::DomCode::US_W:
        return false;
    }
  }

  int lok_key_code =
      office::DOMKeyCodeToLOKKeyCode(event.dom_code, event.GetModifiers());

  document_client_->GetDocument()->setView(view_id_);
  document_client_->GetDocument()->postKeyEvent(
      event.GetType() == blink::WebInputEvent::Type::kKeyUp
          ? LOK_KEYEVENT_KEYUP
          : LOK_KEYEVENT_KEYINPUT,
      event.text[0], lok_key_code);

  needs_reraster_ = true;

  return true;
}

bool OfficeWebPlugin::HandleMouseEvent(const blink::WebMouseEvent& event,
                                       ui::Cursor* cursor) {
  if (!document_client_ || view_id_ == -1)
    return false;

  LibreOfficeKitMouseEventType event_type;
  switch (event.GetType()) {
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

  // TODO: handle offsets
  gfx::Point pos = gfx::ToRoundedPoint(gfx::ScalePoint(
      event.PositionInWidget(), office::lok_callback::TWIP_PER_PX));

  // allow focus even if not in area
  if (!available_area_twips_.Contains(pos)) {
    return event_type == LOK_MOUSEEVENT_MOUSEBUTTONDOWN;
  }

  int buttons = 0;
  int raw_modifiers = event.GetModifiers();
  if (raw_modifiers & blink::WebInputEvent::kLeftButtonDown)
    buttons |= 1;
  if (raw_modifiers & blink::WebInputEvent::kMiddleButtonDown)
    buttons |= 2;
  if (raw_modifiers & blink::WebInputEvent::kRightButtonDown)
    buttons |= 4;

  document_client_->GetDocument()->postMouseEvent(
      event_type, pos.x(), pos.y(), event.ClickCount(), buttons,
      office::EventModifiersToLOKModifiers(raw_modifiers));

  if (event.GetModifiers() & event.kLeftButtonDown) {
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

void OfficeWebPlugin::OnPaint(const std::vector<gfx::Rect>& paint_rects,
                              std::vector<chrome_pdf::PaintReadyRect>& ready,
                              std::vector<gfx::Rect>& pending) {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);
  DoPaint(paint_rects, ready, pending);
}

gfx::PointF OfficeWebPlugin::GetScrollPositionFromOffset(
    const gfx::Vector2dF& scroll_offset) const {
  gfx::PointF scroll_origin;

  return scroll_origin + scroll_offset;
}

void OfficeWebPlugin::DoPaint(const std::vector<gfx::Rect>& paint_rects,
                              std::vector<chrome_pdf::PaintReadyRect>& ready,
                              std::vector<gfx::Rect>& pending) {
  // not mounted
  if (!document_client_) {
    return;
  }

  if (image_data_.drawsNothing()) {
    DCHECK(plugin_rect_.IsEmpty());
    return;
  }

  PrepareForFirstPaint(ready);

  if (!needs_reraster_)
    return;

  document_client_->PrePaint();

  std::vector<gfx::Rect> ready_rects;
  for (const gfx::Rect& paint_rect : paint_rects) {
    // Intersect with plugin area since there could be pending invalidates from
    // when the plugin area was larger.
    gfx::Rect rect =
        gfx::IntersectRects(paint_rect, gfx::Rect(plugin_rect_.size()));
    if (rect.IsEmpty())
      continue;

    // Paint the rendering of the document.
    gfx::Rect pdf_rect = gfx::IntersectRects(rect, available_area_);
    if (!pdf_rect.IsEmpty()) {
      pdf_rect.Offset(-available_area_.x(), 0);

      std::vector<gfx::Rect> pdf_ready;
      std::vector<gfx::Rect> pdf_pending;
      document_client_->Paint(pdf_rect, image_data_, pdf_ready, pdf_pending);
      for (gfx::Rect& ready_rect : pdf_ready) {
        ready_rect.Offset(available_area_.OffsetFromOrigin());
        ready_rects.push_back(ready_rect);
      }
      for (gfx::Rect& pending_rect : pdf_pending) {
        pending_rect.Offset(available_area_.OffsetFromOrigin());
        pending.push_back(pending_rect);
      }
    }

    // Ensure the background parts are filled.
    for (const BackgroundPart& background_part : background_parts_) {
      gfx::Rect intersection =
          gfx::IntersectRects(background_part.location, rect);
      if (!intersection.IsEmpty()) {
        image_data_.erase(background_part.color,
                          gfx::RectToSkIRect(intersection));
        ready_rects.push_back(intersection);
      }
    }
  }

  document_client_->PostPaint();

  // TODO(crbug.com/1263614): Write pixels directly to the `SkSurface` in
  // `PaintManager`, rather than using an intermediate `SkBitmap` and `SkImage`.
  sk_sp<SkImage> painted_image = image_data_.asImage();
  for (const gfx::Rect& ready_rect : ready_rects)
    ready.emplace_back(ready_rect, painted_image);

  InvalidateAfterPaintDone();
}

void OfficeWebPlugin::PrepareForFirstPaint(
    std::vector<chrome_pdf::PaintReadyRect>& ready) {
  if (!first_paint_)
    return;

  // Fill the image data buffer with the background color.
  first_paint_ = false;
  image_data_.eraseColor(background_color_);
  ready.emplace_back(gfx::SkIRectToRect(image_data_.bounds()),
                     image_data_.asImage(), /*flush_now=*/true);
}

void OfficeWebPlugin::OnGeometryChanged(double old_zoom,
                                        float old_device_scale) {
  RecalculateAreas(old_zoom, old_device_scale);
}

void OfficeWebPlugin::RecalculateAreas(double old_zoom,
                                       float old_device_scale) {
  if (!document_client_)
    return;

  if (zoom_ != old_zoom || device_scale_ != old_device_scale)
    document_client_->ZoomUpdated(zoom_ * device_scale_);

  available_area_ = gfx::Rect(plugin_rect_.size());
  int doc_width = GetDocumentPixelWidth();
  if (doc_width < available_area_.width()) {
    // Center the document horizontally inside the plugin rectangle.
    // available_area_.Offset((plugin_rect_.width() - doc_width) / 2, 0);
    available_area_.set_width(doc_width);
  }

  // The distance between top of the plugin and the bottom of the document in
  // pixels.
  int bottom_of_document = GetDocumentPixelHeight();
  if (bottom_of_document < plugin_rect_.height())
    available_area_.set_height(bottom_of_document);

  available_area_twips_ = gfx::ScaleToEnclosingRect(
      available_area_, office::lok_callback::TWIP_PER_PX);

  CalculateBackgroundParts();

  document_client_->PageOffsetUpdated(available_area_.OffsetFromOrigin());
  document_client_->PluginSizeUpdated(available_area_.size());
}

void OfficeWebPlugin::CalculateBackgroundParts() {
  background_parts_.clear();
  int left_width = available_area_.x();
  int right_start = available_area_.right();
  int right_width = std::abs(plugin_rect_.width() - available_area_.right());
  int bottom = std::min(available_area_.bottom(), plugin_rect_.height());

  BackgroundPart part = {gfx::Rect(left_width, bottom), background_color_};
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);

  // Add the right rectangle.
  part.location = gfx::Rect(right_start, 0, right_width, bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);

  // Add the bottom rectangle.
  part.location = gfx::Rect(0, bottom, plugin_rect_.width(),
                            plugin_rect_.height() - bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);
}

int OfficeWebPlugin::GetDocumentPixelWidth() const {
  return static_cast<int>(
      std::ceil(document_client_->DocumentSizePx()
                    .width()));  // * zoom_ * device_scale_));
}

int OfficeWebPlugin::GetDocumentPixelHeight() const {
  return static_cast<int>(
      std::ceil(document_client_->DocumentSizePx()
                    .height()));  // * zoom_ * device_scale_));
}

void OfficeWebPlugin::InvalidateAfterPaintDone() {
  if (deferred_invalidates_.empty())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&OfficeWebPlugin::ClearDeferredInvalidates,
                                weak_factory_.GetWeakPtr()));
}

void OfficeWebPlugin::Invalidate(const gfx::Rect& rect) {
  if (in_paint_) {
    LOG(ERROR) << "IN PAINT!";
    deferred_invalidates_.push_back(rect);
    return;
  }

  gfx::Rect offset_rect = rect + available_area_.OffsetFromOrigin();
  paint_manager_.InvalidateRect(offset_rect);
}

void OfficeWebPlugin::ClearDeferredInvalidates() {
  DCHECK(!in_paint_);
  for (const gfx::Rect& rect : deferred_invalidates_)
    Invalidate(rect);
  deferred_invalidates_.clear();
}

void OfficeWebPlugin::UpdateSnapshot(sk_sp<SkImage> snapshot) {
  snapshot_ =
      cc::PaintImageBuilder::WithDefault()
          .set_image(std::move(snapshot), cc::PaintImage::GetNextContentId())
          .set_id(cc::PaintImage::GetNextId())
          .TakePaintImage();
  if (!plugin_rect_.IsEmpty())
    InvalidatePluginContainer();
}

void OfficeWebPlugin::UpdateScaledValues() {
  total_translate_ = snapshot_translate_;

  if (viewport_to_dip_scale_ != 1.0f)
    total_translate_.Scale(1.0f / viewport_to_dip_scale_);
}

void OfficeWebPlugin::UpdateScale(float scale) {
  if (scale <= 0.0f) {
    NOTREACHED();
    return;
  }

  viewport_to_dip_scale_ = scale;
  device_to_css_scale_ = 1.0f;
  UpdateScaledValues();
}

void OfficeWebPlugin::UpdateLayerTransform(float scale,
                                           const gfx::Vector2dF& translate) {
  snapshot_translate_ = translate;
  snapshot_scale_ = scale;
  UpdateScaledValues();
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

  // TODO(crbug.com/1250173): We should try to avoid the downscaling in this
  // calculation, perhaps by migrating off `plugin_dip_size_`.
  plugin_dip_size_ =
      gfx::ScaleToEnclosingRect(plugin_rect_in_css_pixel, 1.0f).size();

  paint_manager_.SetSize(plugin_rect_.size(), device_scale_);

  // Initialize the image data buffer if the context size changes.
  const gfx::Size old_image_size = gfx::SkISizeToSize(image_data_.dimensions());
  const gfx::Size new_image_size = chrome_pdf::PaintManager::GetNewContextSize(
      old_image_size, plugin_rect_.size());
  if (new_image_size != old_image_size) {
    image_data_.allocPixels(
        SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(new_image_size)));
    first_paint_ = true;
  }

  // Skip updating the geometry if the new image data buffer is empty.
  if (image_data_.drawsNothing())
    return;

  OnGeometryChanged(zoom_, old_device_scale);
}

void OfficeWebPlugin::RenderDocument(v8::Isolate* isolate,
                                     office::DocumentClient* client) {
  if (!client) {
    LOG(ERROR) << "invalid document client";
  }

  document_client_ = client;

  view_id_ = document_client_->Mount(this);
}

void OfficeWebPlugin::TriggerFullRerender() {
  needs_reraster_ = true;
  OnGeometryChanged(zoom_, device_scale_);
  if (document_client_ && !document_client_->DocumentSizePx().IsEmpty()) {
    paint_manager_.InvalidateRect(gfx::Rect(plugin_rect_.size()));
    LOG(ERROR) << "INVALIDATING RECT ON FULL RERENDER: "
               << plugin_rect_.ToString();
  }
}

// } blink::WebPlugin

namespace {}  // namespace

// OfficeWebPlugin::~OfficeWebPlugin() = default;

}  // namespace electron
