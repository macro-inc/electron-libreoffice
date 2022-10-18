// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Portions from PdfWebViewPlugin Copyright 2020 The Chromium Authors. All
// rights reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in src/.

#ifndef OFFICE_OFFICE_WEB_PLUGIN_H_
#define OFFICE_OFFICE_WEB_PLUGIN_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/paint/paint_image.h"
#include "include/core/SkImage.h"
#include "office/document_client.h"
#include "office/event_bus.h"
#include "office/office_client.h"
#include "pdf/paint_manager.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/libreofficekit/LibreOfficeKit.hxx"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "v8/include/v8-value.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace electron {

namespace office {
// MIME type of the internal office plugin.
extern const char kInternalPluginMimeType[];
// Tries to create an instance of the internal PDF plugin, returning `nullptr`
// if the plugin cannot be created.
blink::WebPlugin* CreateInternalPlugin(blink::WebPluginParams params,
                                       content::RenderFrame* render_frame);
}  // namespace office

class OfficeWebPlugin : public blink::WebPlugin,
                        public chrome_pdf::PaintManager::Client {
 public:
  OfficeWebPlugin(blink::WebPluginParams params,
                  content::RenderFrame* render_frame);

  // disable copy
  OfficeWebPlugin(const OfficeWebPlugin& other) = delete;
  OfficeWebPlugin& operator=(const OfficeWebPlugin& other) = delete;

  // blink::WebPlugin {
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  blink::WebPluginContainer* Container() const override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate) override;
  bool SupportsKeyboardFocus() const override;

  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason reason) override;
  void Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) override;

  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const gfx::Rect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focused, blink::mojom::FocusType focus_type) override;
  void UpdateVisibility(bool visibility) override;
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      ui::Cursor* cursor) override;

  // no-op
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(const char* data, size_t data_length) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError& error) override;

  // TODO: See if we need to support printing through Chromium?
  // bool SupportsPaginatedPrint() override;
  // bool GetPrintPresetOptionsFromDocument(
  //     blink::WebPrintPresetOptions* print_preset_options) override;
  // int PrintBegin(const blink::WebPrintParams& print_params) override;
  // void PrintPage(int page_number, cc::PaintCanvas* canvas) override;
  // void PrintEnd() override;

  // TODO: Support copy/paste
  // bool HasSelection() const override;
  // blink::WebString SelectionAsText() const override;
  // blink::WebString SelectionAsMarkup() const override;
  bool CanEditText() const override;
  bool HasEditableText() const override;

  bool CanUndo() const override;
  bool CanRedo() const override;

  // TODO: Update after Electron's Chromium is bumped, this is introduced in
  // newer versions
  // bool CanCopy() const override;

  // TODO: Check if we need this
  // bool ExecuteEditCommand(const blink::WebString& name,
  //                         const blink::WebString& value) override;

  // TODO: This could probably be used for defs, but it might make more sense to
  // just use an event handler
  // blink::WebURL LinkAtPosition(const gfx::Point& /*position*/) const
  // override;

  // TODO: Check if we need this
  // bool StartFind(const blink::WebString& search_text,
  //                bool case_sensitive,
  //                int identifier) override;
  // void SelectFindResult(bool forward, int identifier) override;
  // void StopFind() override;
  // bool CanRotateView() override;
  // void RotateView(blink::WebPlugin::RotationType type) override;

  // TODO: Support IME events
  // bool ShouldDispatchImeEventsToPlugin() override;
  // blink::WebTextInputType GetPluginTextInputType() override;
  // gfx::Rect GetPluginCaretBounds() override;
  // void ImeSetCompositionForPlugin(
  //     const blink::WebString& text,
  //     const std::vector<ui::ImeTextSpan>& ime_text_spans,
  //     const gfx::Range& replacement_range,
  //     int selection_start,
  //     int selection_end) override;
  // void ImeCommitTextForPlugin(
  //     const blink::WebString& text,
  //     const std::vector<ui::ImeTextSpan>& ime_text_spans,
  //     const gfx::Range& replacement_range,
  //     int relative_cursor_pos) override;
  // void ImeFinishComposingTextForPlugin(bool keep_selection) override;

  // } blink::WebPlugin

  // PaintManager::Client
  void InvalidatePluginContainer() override;

  content::RenderFrame* render_frame() const;

  void Invalidate(const gfx::Rect& rect);
  void TriggerFullRerender();
  base::WeakPtr<OfficeWebPlugin> GetWeakPtr();

 private:
  // call `Destroy()` instead.
  ~OfficeWebPlugin() override;
  bool HandleKeyEvent(const blink::WebKeyboardEvent event, ui::Cursor* cursor);
  bool HandleMouseEvent(blink::WebInputEvent::Type type,
                        gfx::PointF position,
                        int modifiers,
                        int clickCount,
                        ui::Cursor* cursor);

  // PaintManager::Client
  void OnPaint(const std::vector<gfx::Rect>& paint_rects,
               std::vector<chrome_pdf::PaintReadyRect>& ready,
               std::vector<gfx::Rect>& pending) override;
  void UpdateSnapshot(sk_sp<SkImage> snapshot) override;
  void UpdateScale(float scale) override;
  void UpdateLayerTransform(float scale,
                            const gfx::Vector2dF& translate) override;

  // Converts a scroll offset (which is relative to a UI direction-dependent
  // scroll origin) to a scroll position (which is always relative to the
  // top-left corner).
  gfx::PointF GetScrollPositionFromOffset(
      const gfx::Vector2dF& scroll_offset) const;

  // PaintManager::Client::OnPaint() should be its only caller.
  void DoPaint(const std::vector<gfx::Rect>& paint_rects,
               std::vector<chrome_pdf::PaintReadyRect>& ready,
               std::vector<gfx::Rect>& pending);

  // The preparation when painting on the image data buffer for the first
  // time.
  void PrepareForFirstPaint(std::vector<chrome_pdf::PaintReadyRect>& ready);

  // Updates the available area and the background parts, notifies the PDF
  // engine, and updates the accessibility information.
  void OnGeometryChanged(double old_zoom, float old_device_scale);

  // A helper of OnGeometryChanged() which updates the available area and
  // the background parts, and notifies the PDF engine of geometry changes.
  void RecalculateAreas(double old_zoom, float old_device_scale);

  // Figures out the location of any background rectangles (i.e. those that
  // aren't painted by the PDF engine).
  void CalculateBackgroundParts();

  // Computes document width/height in device pixels, based on current zoom and
  // device scale
  int GetDocumentPixelWidth() const;
  int GetDocumentPixelHeight() const;

  // Schedules invalidation tasks after painting finishes.
  void InvalidateAfterPaintDone();

  // Callback to clear deferred invalidates after painting finishes.
  void ClearDeferredInvalidates();

  void UpdateScaledValues();
  void OnViewportChanged(const gfx::Rect& plugin_rect_in_css_pixel,
                         float new_device_scale);
  void UpdateScroll(const gfx::PointF& scroll_position);

  // prepares the embed as the document client's mounted viewer
  bool RenderDocument(v8::Isolate* isolate,
                      gin::Handle<office::DocumentClient> client);

  void HandleInvalidateTiles(std::string payload);

  // owns this class
  blink::WebPluginContainer* container_;
  SkColor background_color_ = SK_ColorLTGRAY;

  // Painting State {
  chrome_pdf::PaintManager paint_manager_{this};
  // Image data buffer for painting.
  SkBitmap image_data_;
  // current image snapshot
  cc::PaintImage snapshot_;
  // Translate from snapshot to device pixels.
  gfx::Vector2dF snapshot_translate_;
  // snapshot to device pixels ratios
  float snapshot_scale_ = 1.0f;
  // viewport coordinates to device-independent pixel ratio
  float viewport_to_dip_scale_ = 1.0f;
  // device pixel to css pixel ratio
  float device_to_css_scale_ = 1.0f;
  // combined translate snapshot -> device -> CSS pixels
  gfx::Vector2dF total_translate_;
  // plugin rect in CSS pixels
  gfx::Rect css_plugin_rect_;
  // size of plugin rectangle in DIPs.
  gfx::Size plugin_dip_size_;
  // plugin rectangle in device pixels.
  gfx::Rect plugin_rect_;
  // Remaining area, in pixels, to render the view in after accounting for
  // horizontal centering.
  gfx::Rect available_area_;
  gfx::Rect available_area_twips_;
  // zoom factor
  double zoom_ = 1.0;
  // current device scale factor. viewport * device_scale_ == screen, screen /
  // device_scale_ == viewport
  float device_scale_ = 1.0f;
  // first paint, requiring the full canvas of tiles to be painted
  bool first_paint_ = true;
  // currently painting, to track deferred invalidates
  bool in_paint_ = false;
  // True if last bitmap was smaller than the screen.
  bool last_bitmap_smaller_ = false;
  // True if we request a new bitmap rendering.
  bool needs_reraster_ = true;
  struct BackgroundPart {
    gfx::Rect location;
    uint32_t color;
  };
  std::vector<BackgroundPart> background_parts_;
  // Deferred invalidates while `in_paint_` is true.
  std::vector<gfx::Rect> deferred_invalidates_;
  // The UI direction.
  // base::i18n::TextDirection ui_direction_ = base::i18n::UNKNOWN_DIRECTION;

  // The scroll offset for the last raster in CSS pixels, before any
  // transformations are applied.
  gfx::Vector2dF scroll_offset_at_last_raster_;
  // If this is true, then don't scroll the plugin in response to calls to
  // `UpdateScroll()`. This will be true when the extension page is in the
  // process of zooming the plugin so that flickering doesn't occur while
  // zooming.
  bool stop_scrolling_ = false;

  LibreOfficeKitTileMode tile_mode_;
  // }

  // UI State {
  // current cursor
  ui::mojom::CursorType cursor_type_ = ui::mojom::CursorType::kPointer;
  bool has_focus_;
  std::vector<gfx::Rect> paint_points_;
  // }
  /*

  // Text Handle State {
  gfx::Rect handle_start_rect_;
  gfx::Rect handle_middle_rect_;
  gfx::Rect handle_end_rect_;
  bool in_drag_start_handle_;
  bool in_drag_middle_handle_;
  bool in_drag_end_handle_;
  // }

  // 8-point Graphic Handle State {
  gfx::Rect graphic_handle_rects_[8];
  gfx::Rect in_drag_graphic_handles_[8];
  // }
  */

  // owned by
  content::RenderFrame* render_frame_ = nullptr;
  lok::Office* office_ = nullptr;
  office::OfficeClient* office_client_ = nullptr;

  // maybe has a
  lok::Document* document_ = nullptr;
  office::DocumentClient* document_client_ = nullptr;
  int view_id_ = -1;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // invalidates when destroy() is called
  base::WeakPtrFactory<OfficeWebPlugin> weak_factory_{this};
};

}  // namespace electron
#endif  // OFFICE_OFFICE_WEB_PLUGIN_H_
