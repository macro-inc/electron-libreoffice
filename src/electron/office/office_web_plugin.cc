// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/office_web_plugin.h"

#include <algorithm>
#include <string_view>

#include "LibreOfficeKit/LibreOfficeKit.hxx"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_canvas.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "include/core/SkColor.h"
#include "office/cancellation_flag.h"
#include "office/document_client.h"
#include "office/lok_callback.h"
#include "office/lok_tilebuffer.h"
#include "office/office_client.h"
#include "office/office_instance.h"
#include "office/office_keys.h"
#include "office/paint_manager.h"
#include "shell/common/gin_converters/gfx_converter.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "web_plugin_utils.h"

namespace electron {

namespace office {
extern const char kInternalPluginMimeType[] = "application/x-libreoffice";

blink::WebPlugin* CreateInternalPlugin(blink::WebPluginParams params,
                                       content::RenderFrame* render_frame) {
  return new OfficeWebPlugin(params, render_frame);
}

}  // namespace office

using namespace office;
OfficeWebPlugin::OfficeWebPlugin(blink::WebPluginParams /*params*/,
                                 content::RenderFrame* render_frame)
    : render_frame_(render_frame),
      tile_buffer_(base::MakeRefCounted<TileBuffer>()),
      restore_key_(base::Token::CreateRandom()),
      task_runner_(render_frame->GetTaskRunner(
          blink::TaskType::kInternalMediaRealTime)) {
  paint_manager_ = std::make_unique<office::PaintManager>(this);
  auto* inst = office::OfficeInstance::Get();
  if (inst)
    inst->AddDestroyedObserver(this);
}

bool OfficeWebPlugin::Initialize(blink::WebPluginContainer* container) {
  container_ = container;
  return container::Initialize(container);
}

void OfficeWebPlugin::Destroy() {
  paint_manager_->OnDestroy();
  if (document_client_.MaybeValid()) {
    document_client_->Unmount();
    document_client_->MarkRendererWillRemount(
        std::move(restore_key_),
        {std::move(tile_buffer_), std::move(paint_manager_),
         std::move(snapshot_), std::move(page_rects_cached_), first_intersect_,
         last_intersect_, std::move(last_cursor_rect_), zoom_});
  }
  if (!doomed_) {
    auto* inst = office::OfficeInstance::Get();
    if (inst)
      inst->RemoveDestroyedObserver(this);
  }
  if (document_) {
    document_.RemoveDocumentObservers(this);
  }

  delete this;
}

// the office client was destroyed, explicitly remove the hold on the document
void OfficeWebPlugin::OnDestroyed() {
  doomed_ = true;
  auto* inst = office::OfficeInstance::Get();
  if (inst)
    inst->RemoveDestroyedObserver(this);
  document_ = {};
}

v8::Local<v8::Object> OfficeWebPlugin::V8ScriptableObject(
    v8::Isolate* isolate) {
  if (v8_template_.IsEmpty()) {
    v8::Local<v8::ObjectTemplate> template_ =
        gin::ObjectTemplateBuilder(isolate, "OfficeWebPlugin")
            .SetMethod("renderDocument",
                       base::BindRepeating(&OfficeWebPlugin::RenderDocument,
                                           base::Unretained(this)))
            .SetMethod("updateScroll",
                       base::BindRepeating(&OfficeWebPlugin::UpdateScroll,
                                           base::Unretained(this)))
            .SetMethod("getZoom", base::BindRepeating(&OfficeWebPlugin::GetZoom,
                                                      base::Unretained(this)))
            .SetMethod("setZoom", base::BindRepeating(&OfficeWebPlugin::SetZoom,
                                                      base::Unretained(this)))
            .SetMethod("invalidateAllTiles",
                       base::BindRepeating(&OfficeWebPlugin::InvalidateAllTiles,
                                           base::Unretained(this)))
            .SetMethod("twipToPx",
                       base::BindRepeating(&OfficeWebPlugin::TwipToCSSPx,
                                           base::Unretained(this)))
            .SetMethod("debounceUpdates",
                       base::BindRepeating(&OfficeWebPlugin::DebounceUpdates,
                                           base::Unretained(this)))
            .SetProperty(
                "documentSize",
                base::BindRepeating(&OfficeWebPlugin::GetDocumentCSSPixelSize,
                                    base::Unretained(this)))
            .SetProperty("pageRects",
                         base::BindRepeating(&OfficeWebPlugin::PageRects,
                                             base::Unretained(this)))
            .Build();
    v8_template_.Reset(isolate, template_);
  }

  if (v8_object_.IsEmpty()) {
    v8_object_.Reset(isolate, v8_template_.Get(isolate)
                                  ->NewInstance(isolate->GetCurrentContext())
                                  .ToLocalChecked());
  }
  return v8_object_.Get(isolate);
}

blink::WebPluginContainer* OfficeWebPlugin::Container() const {
  return container_;
}

bool OfficeWebPlugin::SupportsKeyboardFocus() const {
  return true;
}

void OfficeWebPlugin::UpdateAllLifecyclePhases(
    blink::DocumentUpdateReason reason) {}

void OfficeWebPlugin::UpdateSnapshot(const office::Snapshot snapshot) {
  if (snapshot.tiles.empty())
    return;
  snapshot_ = std::move(snapshot);
}

void OfficeWebPlugin::Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);
  if (!visible_) {
    return;
  }

  SkRect invalidate_rect =
      gfx::RectToSkRect(gfx::IntersectRects(css_plugin_rect_, rect));
  cc::PaintCanvasAutoRestore auto_restore(canvas, true);

  if (scale_pending_ || first_paint_)
    canvas->drawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);

  canvas->clipRect(invalidate_rect);

  // not mounted
  if (!document_) {
    return;
  }

  if (!plugin_rect_.origin().IsOrigin())
    canvas->translate(plugin_rect_.x(), plugin_rect_.y());

  gfx::Rect size(invalidate_rect.width(), invalidate_rect.height());
  std::vector<office::TileRange> missing =
      tile_buffer_->PaintToCanvas(paint_cancel_flag_, canvas, snapshot_, size,
                                  TotalScale(), scale_pending_, scrolling_);

  if (missing.size() == 0 && take_snapshot_ && !scrolling_) {
    UpdateSnapshot(tile_buffer_->MakeSnapshot(paint_cancel_flag_, size));
    take_snapshot_ = false;
  }
  if (update_debounce_timer_ && !scrolling_)
    paint_manager_->PausePaint();

  // the temporary scale is painted, now
  if (scale_pending_) {
    scale_pending_ = false;
    tile_buffer_->ResetScale(TotalScale());
    ScheduleAvailableAreaPaint();
    first_paint_ = false;
  } else {
    if (!paint_manager_->ScheduleNextPaint(missing) && missing.size() != 0) {
      ScheduleAvailableAreaPaint();
    }
    first_paint_ = false;
  }
  scrolling_ = false;
}

void OfficeWebPlugin::UpdateGeometry(const gfx::Rect& window_rect,
                                     const gfx::Rect& clip_rect,
                                     const gfx::Rect& unobscured_rect,
                                     bool is_visible) {
  // nothing to render inside of
  if (window_rect.IsEmpty())
    return;

  OnViewportChanged(window_rect, container::DeviceScale(container_));
}

void OfficeWebPlugin::UpdateFocus(bool focused,
                                  blink::mojom::FocusType focus_type) {
  if (disable_input_)
    return;
  // focusing without cursor interaction doesn't register with LOK, so for JS to
  // register a .focus() on the embed, simply simulate a click at the last
  // cursor position
  if (document_ && focused && focus_type == blink::mojom::FocusType::kScript) {
    if (last_cursor_rect_.empty())
      return;

    std::string_view payload_sv(last_cursor_rect_);
    std::string_view::const_iterator start = payload_sv.begin();
    gfx::Rect pos = office::lok_callback::ParseRect(start, payload_sv.end());
    document_.Post(base::BindOnce(
        [](gfx::Rect pos, DocumentHolderWithView holder) {
          holder->postMouseEvent(LOK_MOUSEEVENT_MOUSEBUTTONDOWN, pos.x(),
                                 pos.y(), 1, 1, 0);
          holder->postMouseEvent(LOK_MOUSEEVENT_MOUSEBUTTONUP, pos.x(), pos.y(),
                                 1, 1, 0);
        },
        std::move(pos)));
  }

  has_focus_ = focused;
}

void OfficeWebPlugin::UpdateVisibility(bool visibility) {
  visible_ = visibility;
}

namespace {
// this is kind of stupid, since there's probably a way to get this directly
// from blink, but it works
ui::mojom::CursorType cssCursorToMojom(const std::string& css) {
  using ui::mojom::CursorType;
  static const base::NoDestructor<std::unordered_map<std::string, CursorType>>
      map({{"auto", CursorType::kNull},
           {"default", CursorType::kNull},
           {"none", CursorType::kNone},
           {"context-menu", CursorType::kContextMenu},
           {"help", CursorType::kHelp},
           {"pointer", CursorType::kPointer},
           {"progress", CursorType::kProgress},
           {"wait", CursorType::kWait},
           {"cell", CursorType::kCell},
           {"crosshair", CursorType::kCross},
           {"text", CursorType::kIBeam},
           {"vertical-text", CursorType::kVerticalText},
           {"alias", CursorType::kAlias},
           {"copy", CursorType::kCopy},
           {"move", CursorType::kMove},
           {"no-drop", CursorType::kNoDrop},
           {"not-allowed", CursorType::kNotAllowed},
           {"grab", CursorType::kGrab},
           {"grabbing", CursorType::kGrabbing},
           {"e-resize", CursorType::kEastResize},
           {"n-resize", CursorType::kNorthResize},
           {"ne-resize", CursorType::kNorthEastResize},
           {"nw-resize", CursorType::kNorthWestResize},
           {"s-resize", CursorType::kSouthResize},
           {"se-resize", CursorType::kSouthEastResize},
           {"sw-resize", CursorType::kSouthWestResize},
           {"w-resize", CursorType::kWestResize},
           {"ew-resize", CursorType::kEastWestResize},
           {"ns-resize", CursorType::kNorthSouthResize},
           {"nesw-resize", CursorType::kNorthEastSouthWestResize},
           {"nwse-resize", CursorType::kNorthWestSouthEastResize},
           {"col-resize", CursorType::kColumnResize},
           {"row-resize", CursorType::kRowResize},
           {"all-scroll", CursorType::kMove},
           {"zoom-in", CursorType::kZoomIn},
           {"zoom-out", CursorType::kZoomOut}});
  auto it = map->find(css);
  return it == map->end() ? CursorType::kNull : it->second;
}
}  // namespace

blink::WebInputEventResult OfficeWebPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& event,
    ui::Cursor* cursor) {
  base::TimeTicks now = base::TimeTicks::Now();
  // we debounce this because getting the computed value calculates layout and
  // the cursor is often the same
  if (last_css_cursor_time_.is_null() ||
      (now - last_css_cursor_time_) > base::Milliseconds(10)) {
    last_css_cursor_time_ = now;
    cursor_type_ = cssCursorToMojom(container::CSSCursor(container_));
  }
  *cursor = cursor_type_;
  if (disable_input_)
    return blink::WebInputEventResult::kNotHandled;

  blink::WebInputEvent::Type event_type = event.Event().GetType();

  // TODO: handle gestures
  if (blink::WebInputEvent::IsGestureEventType(event_type))
    return blink::WebInputEventResult::kNotHandled;

  if (blink::WebInputEvent::IsKeyboardEventType(event_type))
    return HandleKeyEvent(
        std::move(static_cast<const blink::WebKeyboardEvent&>(event.Event())),
        cursor);

  switch (event_type) {
    case blink::WebInputEvent::Type::kMouseDown:
    case blink::WebInputEvent::Type::kMouseUp:
    case blink::WebInputEvent::Type::kMouseMove:
      break;
    default:
      return blink::WebInputEventResult::kNotHandled;
  }

  int modifiers = event.Event().GetModifiers();

  return HandleMouseEvent(
             event_type,
             input::GetRelativeMousePosition(
                 event.Event(), gfx::Vector2dF(-available_area_.x(), 0)),
             modifiers, input::GetClickCount(event.Event()), cursor)
             ? blink::WebInputEventResult::kHandledApplication
             : blink::WebInputEventResult::kNotHandled;
}

blink::WebInputEventResult OfficeWebPlugin::HandleKeyEvent(
    const blink::WebKeyboardEvent event,
    ui::Cursor* cursor) {
  if (!document_) {
    return blink::WebInputEventResult::kNotHandled;
  }

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

#if BUILDFLAG(IS_MAC)
  office::Modifiers base_modifier = office::Modifiers::kMetaKey;
#else
  office::Modifiers base_modifier = office::Modifiers::kControlKey;
#endif

  // intercept some special key events
  if (event.GetModifiers() & base_modifier) {
    switch (event.dom_code) {
      // don't close the internal LO window
      case office::DomCode::US_W:
        return blink::WebInputEventResult::kNotHandled;
      // prevent paste, paste unformatted (handle the events in JS instead)
      case office::DomCode::US_V:
        return blink::WebInputEventResult::kNotHandled;
    }
  }

  int modifiers = event.GetModifiers();

#if BUILDFLAG(IS_MAC)
  modifiers &= ~office::Modifiers::kControlKey;
  if (modifiers & office::Modifiers::kMetaKey) {
    modifiers |= office::Modifiers::kControlKey;
    modifiers &= ~office::Modifiers::kMetaKey;
  }
#endif

  int lok_key_code = office::DOMKeyCodeToLOKKeyCode(event.dom_code, modifiers);

  document_.Post(base::BindOnce(
      [](LibreOfficeKitKeyEventType key_event, char16_t text, int lok_key_code,
         DocumentHolderWithView holder) {
        holder->postKeyEvent(key_event, text, lok_key_code);
      },
      type == blink::WebInputEvent::Type::kKeyUp ? LOK_KEYEVENT_KEYUP
                                                 : LOK_KEYEVENT_KEYINPUT,
      event.text[0], lok_key_code));

  return blink::WebInputEventResult::kHandledApplication;
}

bool OfficeWebPlugin::HandleMouseEvent(blink::WebInputEvent::Type type,
                                       gfx::PointF position,
                                       int modifiers,
                                       int clickCount,
                                       ui::Cursor* cursor) {
  if (!document_)
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
  position.Offset(0, scroll_y_position_);

  gfx::Point pos = gfx::ToRoundedPoint(gfx::ScalePoint(
      position, office::lok_callback::kTwipPerPx / TotalScale()));

  int buttons = 0;
  if (modifiers & blink::WebInputEvent::kLeftButtonDown)
    buttons |= 1;
  if (modifiers & blink::WebInputEvent::kMiddleButtonDown)
    buttons |= 2;
  if (modifiers & blink::WebInputEvent::kRightButtonDown)
    buttons |= 4;

  if (buttons > 0) {
    document_.Post(base::BindOnce(
        [](LibreOfficeKitMouseEventType event_type, gfx::Point pos,
           int clickCount, int buttons, int modifiers,
           DocumentHolderWithView holder) {
          holder->postMouseEvent(event_type, pos.x(), pos.y(), clickCount,
                                 buttons, modifiers);
        },
        event_type, std::move(pos), clickCount, buttons,
        office::EventModifiersToLOKModifiers(modifiers)));
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
  return document_client_.MaybeValid() && document_client_->CanUndo();
}

bool OfficeWebPlugin::CanRedo() const {
  return document_client_.MaybeValid() && document_client_->CanRedo();
}

content::RenderFrame* OfficeWebPlugin::render_frame() const {
  return render_frame_;
}

OfficeWebPlugin::~OfficeWebPlugin() = default;

void OfficeWebPlugin::InvalidateWeakContainer() {
  if (!in_paint_) {
    container::Invalidate(container_);
  }
}

void OfficeWebPlugin::InvalidatePluginContainer() {
  if (container_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OfficeWebPlugin::InvalidateWeakContainer,
                                  GetWeakPtr()));
  }
}

void OfficeWebPlugin::OnGeometryChanged(double old_zoom,
                                        float old_device_scale) {
  if (!document_)
    return;

  if (viewport_zoom_ != old_zoom || device_scale_ != old_device_scale) {
    tile_buffer_->ResetScale(TotalScale());
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

std::vector<gfx::Rect> OfficeWebPlugin::PageRects() {
  std::vector<gfx::Rect> result;

  if (!document_ || !document_client_.MaybeValid())
    return result;
  auto page_rects_ = document_client_->PageRects();

  float scale = zoom_ / office::lok_callback::kTwipPerPx;
  for (auto& rect : page_rects_) {
    result.emplace_back(gfx::ScaleToCeiledPoint(rect.origin(), scale),
                        gfx::ScaleToCeiledSize(rect.size(), scale));
  }
  page_rects_cached_.assign(result.begin(), result.end());
  UpdateIntersectingPages();
  return result;
}

void OfficeWebPlugin::InvalidateAllTiles() {
  // not mounted
  if (!document_)
    return;

  if (!tile_buffer_)
    return;

  tile_buffer_->InvalidateAllTiles();
}

gfx::Size OfficeWebPlugin::GetDocumentPixelSize() {
  if (!document_client_.MaybeValid())
    return {};
  auto size = document_client_->DocumentSizeTwips();
  return gfx::Size(ceil(TwipToPx(size.width())), ceil(TwipToPx(size.height())));
}

gfx::Size OfficeWebPlugin::GetDocumentCSSPixelSize() {
  if (!document_client_.MaybeValid())
    return {};
  auto size = document_client_->DocumentSizeTwips();
  return gfx::Size(
      ceil(office::lok_callback::TwipToPixel(size.width(), zoom_)),
      ceil(office::lok_callback::TwipToPixel(size.height(), zoom_)));
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

  bool need_fresh_paint =
      plugin_rect_in_css_pixel.height() != plugin_rect_.height();

  const float old_device_scale = device_scale_;
  device_scale_ = new_device_scale;
  plugin_rect_ = plugin_rect_in_css_pixel;

  OnGeometryChanged(viewport_zoom_, old_device_scale);

  if (!document_)
    return;

  if (need_fresh_paint) {
    ScheduleAvailableAreaPaint(false);
  }
}

void OfficeWebPlugin::HandleInvalidateTiles(std::string payload) {
  // not mounted
  if (!document_) {
    return;
  }

  std::string_view payload_sv(payload);

  // TODO: handle non-text document types for parts
  if (payload_sv.substr(0, 5) == "EMPTY") {
    auto num_payload = payload_sv.substr(5);
    // if there is a page number, skip every invalidation that isn't the last
    // visible page this allows earlier paints on large documents
    if (!num_payload.empty()) {
      std::string_view::const_iterator start = num_payload.begin();
      auto num = office::lok_callback::ParseCSV(start, payload_sv.end());
      if (num.empty()) {
        return;
      } else if ((int)num[0] != last_intersect_) {
        return;
      }
    }

    base::TimeTicks now = base::TimeTicks::Now();
    if (last_full_invalidation_time_.is_null() ||
        (now - last_full_invalidation_time_) > base::Milliseconds(10)) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&OfficeWebPlugin::TryResumePaint, GetWeakPtr()));

      ScheduleAvailableAreaPaint();
      last_full_invalidation_time_ = now;
    }
    // weirdly, LOK seems to be issuing a full tile invalidation FOR EVERY PAGE,
    // then the whole document skip those page invalidations which are of the
    // form "EMPTY, #, #" rendering was getting N+1 full document re-renders
    // where N=number of pages, that's bad
  } else if (payload_sv.substr(0, 5) != "EMPTY") {
    std::string_view::const_iterator start = payload_sv.begin();
    gfx::Rect dirty_rect =
        office::lok_callback::ParseRect(start, payload_sv.end());

    if (dirty_rect.IsEmpty())
      return;

    gfx::RectF offset_area(available_area_);
    offset_area.Offset(0, scroll_y_position_);
    auto view_height = offset_area.height();
    auto range = tile_buffer_->InvalidateTilesInTwipRect(dirty_rect);
    auto limit = tile_buffer_->LimitIndex(scroll_y_position_, view_height);

    // avoid scheduling out of bounds paints
    if (range.index_start > limit.index_end ||
        range.index_end < limit.index_start)
      return;
    range.index_start = std::max(range.index_start, limit.index_start);
    range.index_end = std::min(range.index_end, limit.index_end);

    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OfficeWebPlugin::TryResumePaint, GetWeakPtr()));

    take_snapshot_ = true;
    paint_manager_->SchedulePaint(document_, scroll_y_position_, view_height,
                                  TotalScale(), false, {range});
  }
}

float OfficeWebPlugin::TotalScale() {
  return zoom_ * device_scale_ * viewport_zoom_;
}

namespace {
// clip to nearest 8px (Alex discovered this was the crispest render)
float clipToNearest8PxZoom(int w, float s) {
  float scaled_width = static_cast<float>(w) * s;
  int mod = static_cast<int>(std::ceil(scaled_width)) % 8;
  if (mod == 0)
    return s;

  float low_scale = (std::ceil(scaled_width) - mod) / w;
  float high_scale = (std::ceil(scaled_width) + 8 - mod) / w;

  return std::abs(low_scale - s) < std::abs(high_scale - s) ? low_scale
                                                            : high_scale;
}
}  // namespace

void OfficeWebPlugin::SetZoom(float zoom) {
  zoom = clipToNearest8PxZoom(256, zoom);

  if (abs(zoom_ - zoom) < 0.0001f) {
    return;
  }

  old_zoom_ = zoom_;
  scroll_y_position_ = zoom / zoom_ * scroll_y_position_;
  zoom_ = zoom;

  if (!document_)
    return;
  scale_pending_ = true;

  // immediately flush the container to scale without invalidating tiles
  if (!in_paint_) {
    tile_buffer_->SetActiveContext(0);
    InvalidatePluginContainer();
  }
}

float OfficeWebPlugin::GetZoom() {
  return zoom_;
}

float OfficeWebPlugin::TwipToPx(float in) {
  return office::lok_callback::TwipToPixel(in, TotalScale());
}

float OfficeWebPlugin::TwipToCSSPx(float in) {
  return ceil(office::lok_callback::TwipToPixel(in, zoom_));
}

void OfficeWebPlugin::UpdateIntersectingPages() {
  float view_height =
      plugin_rect_.height() / device_scale_ / (float)viewport_zoom_;
  gfx::Rect scroll_rect(gfx::Point(0, scroll_y_position_ / device_scale_),
                        gfx::Size(800, view_height));
  int i = 0;
  first_intersect_ = -1;
  last_intersect_ = -1;

  for (auto& it : page_rects_cached_) {
    if (first_intersect_ == -1 && it.Intersects(scroll_rect)) {
      first_intersect_ = i;
    }
    if (first_intersect_ != -1) {
      if (!it.Intersects(scroll_rect)) {
        break;
      } else {
        last_intersect_ = i;
      }
    }
    i++;
  }
}

void OfficeWebPlugin::UpdateScroll(int64_t y_position) {
  if (!document_ || !document_client_.MaybeValid() || stop_scrolling_)
    return;
  if (!tile_buffer_ || tile_buffer_->IsEmpty()) {
    LOG(ERROR) << "Tile buffer is empty during scroll";
    return;
  }

  float view_height =
      plugin_rect_.height() / device_scale_ / (float)viewport_zoom_;
  float max_y = std::max(
      TwipToPx(document_client_->DocumentSizeTwips().height()) - view_height,
      0.0f);

  float scaled_y = std::clamp((float)y_position, 0.0f, max_y) * device_scale_;
  scroll_y_position_ = scaled_y;

  // TODO: paint PRIOR not ahead for scroll up
  office::TileRange range =
      tile_buffer_->NextScrollTileRange(scroll_y_position_, view_height);
  tile_buffer_->SetYPosition(scaled_y);
  paint_manager_->ResumePaint(false);
  // TODO: schedule paint _ahead_ / _prior_ to scroll position
  paint_manager_->SchedulePaint(document_, scroll_y_position_,
                                view_height * device_scale_, TotalScale(),
                                false, {range});
  UpdateIntersectingPages();
  scrolling_ = true;
  take_snapshot_ = true;
}

std::string OfficeWebPlugin::RenderDocument(
    v8::Isolate* isolate,
    gin::Handle<office::DocumentClient> client,
    gin::Arguments* args) {
  if (client.IsEmpty()) {
    LOG(ERROR) << "invalid document client";
    return {};
  }
  absl::optional<base::Token> maybe_restore_key;

  v8::Local<v8::Object> options;
  if (args->GetNext(&options)) {
    gin::Dictionary options_dict(isolate, options);

    float zoom;
    if (options_dict.Get("zoom", &zoom)) {
      zoom_ = clipToNearest8PxZoom(256, zoom);
    }

    bool disable_input;
    if (options_dict.Get("disableInput", &disable_input)) {
      disable_input_ = disable_input;
    }

    std::string restore_key;
    if (options_dict.Get("restoreKey", &restore_key)) {
      maybe_restore_key = base::Token::FromString(restore_key);
    }
  }

  bool needs_reset = document_ && document_ != client->GetDocument();

  if (registered_observers_ && document_) {
    document_.RemoveDocumentObservers(this);
  }
  if (needs_reset && document_client_.MaybeValid()) {
    document_client_->Unmount();
  }
  bool needs_restore = !document_ && maybe_restore_key.has_value();

  document_ = client->GetDocument();
  document_client_ = client->GetWeakPtr();

  if (!document_) {
    LOG(ERROR) << "document not held in client";
    return {};
  }

  if (needs_reset) {
    first_paint_ = true;
    tile_buffer_->InvalidateAllTiles();
  }

  if (needs_restore) {
    auto transferable = client->GetRestoredRenderer(maybe_restore_key.value());
    if (transferable.tile_buffer && !transferable.tile_buffer->IsEmpty()) {
      tile_buffer_ = std::move(transferable.tile_buffer);
    }
    snapshot_ = std::move(transferable.snapshot);
    if (transferable.paint_manager) {
      paint_manager_ = std::make_unique<office::PaintManager>(
          this, std::move(transferable.paint_manager));
    }
    first_paint_ = false;
    page_rects_cached_ = std::move(transferable.page_rects);
    first_intersect_ = transferable.first_intersect;
    last_intersect_ = transferable.last_intersect;
    last_cursor_rect_ = std::move(transferable.last_cursor_rect);
    if (transferable.zoom > 0) {
      zoom_ = transferable.zoom;
    }
  }

  client->Mount(isolate);
  if (needs_restore) {
    scroll_y_position_ = snapshot_.scroll_y_position;
  } else {
    auto size = document_client_->DocumentSizeTwips();
    scroll_y_position_ = 0;
    // TODO: figure out why zoom_ can sometimes be set to NaN
    if (zoom_ != zoom_ || zoom_ < 0) {
      zoom_ = 1.0f;
    }
    tile_buffer_->SetYPosition(0);
    tile_buffer_->Resize(size.width(), size.height(), TotalScale());
  }

  if (needs_reset) {
    // this is an awful hack
    float device_scale = device_scale_;
    device_scale_ = 0;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OfficeWebPlugin::OnViewportChanged, GetWeakPtr(),
                       css_plugin_rect_, device_scale));
  }

  if (!needs_restore) {
    document_->resetSelection();
  }

  document_.AddDocumentObserver(LOK_CALLBACK_DOCUMENT_SIZE_CHANGED, this);
  document_.AddDocumentObserver(LOK_CALLBACK_INVALIDATE_TILES, this);
  document_.AddDocumentObserver(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR, this);
  registered_observers_ = true;

  if (needs_reset) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OfficeWebPlugin::OnGeometryChanged,
                                  GetWeakPtr(), viewport_zoom_, device_scale_));
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OfficeWebPlugin::UpdateScroll, GetWeakPtr(), 0));
  }
  if (needs_restore) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OfficeWebPlugin::OnGeometryChanged,
                                  GetWeakPtr(), viewport_zoom_, device_scale_));
    paint_manager_->ResumePaint();
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OfficeWebPlugin::UpdateScroll, GetWeakPtr(),
                                  scroll_y_position_));
  }

  return restore_key_.ToString();
}

void OfficeWebPlugin::ScheduleAvailableAreaPaint(bool invalidate) {
  gfx::RectF offset_area(available_area_);
  offset_area.Offset(0, scroll_y_position_);
  auto view_height = offset_area.height();
  // this is a crash case that should not occur anymore
  if (tile_buffer_->IsEmpty()) {
    LOG(ERROR) << "Full area paint, but tile buffer is empty";
    return;
  }
  auto range = tile_buffer_->InvalidateTilesInRect(offset_area, !invalidate);
  auto limit = tile_buffer_->LimitIndex(scroll_y_position_, view_height);

  // avoid scheduling out of bounds paints
  if (range.index_start > limit.index_end ||
      range.index_end < limit.index_start) {
    return;
  }
  take_snapshot_ = true;
  range.index_start = std::max(range.index_start, limit.index_start);
  range.index_end = std::min(range.index_end, limit.index_end);
  paint_manager_->SchedulePaint(document_, scroll_y_position_, view_height,
                                TotalScale(), true, {range});
}

void OfficeWebPlugin::TriggerFullRerender() {
  first_paint_ = true;
  if (document_client_.MaybeValid() &&
      !document_client_->DocumentSizeTwips().IsEmpty()) {
    tile_buffer_->InvalidateAllTiles();
    ScheduleAvailableAreaPaint();
  }
}

scoped_refptr<office::TileBuffer> OfficeWebPlugin::GetTileBuffer() {
  return tile_buffer_;
}

base::WeakPtr<office::PaintManager::Client> OfficeWebPlugin::GetWeakClient() {
  return GetWeakPtr();
}

base::WeakPtr<OfficeWebPlugin> OfficeWebPlugin::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void OfficeWebPlugin::DebounceUpdates(int interval) {
  if (interval <= 0 && update_debounce_timer_) {
    update_debounce_timer_.reset();
  } else {
    update_debounce_timer_ = std::make_unique<base::DelayTimer>(
        FROM_HERE, base::Milliseconds(interval), this,
        &OfficeWebPlugin::DebouncedResumePaint);
    update_debounce_timer_->Reset();
    paint_manager_->PausePaint();
  }
}

void OfficeWebPlugin::TryResumePaint() {
  if (update_debounce_timer_)
    update_debounce_timer_->Reset();
}

void OfficeWebPlugin::DebouncedResumePaint() {
  if (!paint_manager_)
    return;

  paint_manager_->ResumePaint();
}

void OfficeWebPlugin::DocumentCallback(int type, std::string payload) {
  switch (type) {
    case LOK_CALLBACK_DOCUMENT_SIZE_CHANGED: {
      if (!document_)
        return;
      long width, height;
      document_->getDocumentSize(&width, &height);
      tile_buffer_->Resize(width, height);
      break;
    }
    case LOK_CALLBACK_INVALIDATE_TILES: {
      HandleInvalidateTiles(payload);
      break;
    }
    case LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR: {
      if (!payload.empty()) {
        last_cursor_rect_ = std::move(payload);
      }
      break;
    }
  }
}

}  // namespace electron
