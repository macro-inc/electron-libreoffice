// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Portions from PdfWebViewPlugin Copyright 2020 The Chromium Authors. All
// rights reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in src/.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "gin/handle.h"
#include "office/destroyed_observer.h"
#include "office/document_client.h"
#include "office/document_event_observer.h"
#include "office/document_holder.h"
#include "office/lok_tilebuffer.h"
#include "office/office_client.h"
#include "office/paint_manager.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/rect.h"
#include "v8/include/v8-template.h"
#include "v8/include/v8-value.h"

namespace lok {
class Document;
}  // namespace lok

namespace content {
class RenderFrame;
}  // namespace content

namespace blink {
class WebPluginContainer;
}

namespace electron {

namespace office {

class DocumentClient;
class TileBuffer;

// MIME type of the internal office plugin.
extern const char kInternalPluginMimeType[];
// Tries to create an instance of the internal PDF plugin, returning `nullptr`
// if the plugin cannot be created.
blink::WebPlugin* CreateInternalPlugin(blink::WebPluginParams params,
                                       content::RenderFrame* render_frame);
}  // namespace office

class OfficeWebPlugin : public blink::WebPlugin,
                        public office::PaintManager::Client,
                        public office::DocumentEventObserver,
                        public office::DestroyedObserver {
 public:
  OfficeWebPlugin(blink::WebPluginParams /*params*/,
                  content::RenderFrame* render_frame);

  // disable copy
  OfficeWebPlugin(const OfficeWebPlugin& other) = delete;
  OfficeWebPlugin& operator=(const OfficeWebPlugin& other) = delete;

  // blink::WebPlugin {
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  blink::WebPluginContainer* Container() const override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate) override;

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

  // constant
  bool SupportsKeyboardFocus() const override;

  // no-op
  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason reason) override;
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
  base::WeakPtr<office::PaintManager::Client> GetWeakClient() override;
  scoped_refptr<office::TileBuffer> GetTileBuffer() override;

  content::RenderFrame* render_frame() const;

  void TriggerFullRerender();
  void ScheduleAvailableAreaPaint(bool invalidate = true);
  base::WeakPtr<OfficeWebPlugin> GetWeakPtr();
  void UpdateSnapshot(const office::Snapshot snapshot);

  // DocumentEventObserver
  void DocumentCallback(int type, std::string payload) override;

  // DestroyedObserver
  void OnDestroyed() override;

 private:
  // call `Destroy()` instead.
  ~OfficeWebPlugin() override;
  blink::WebInputEventResult HandleKeyEvent(const blink::WebKeyboardEvent event,
                                            ui::Cursor* cursor);
  bool HandleMouseEvent(blink::WebInputEvent::Type type,
                        gfx::PointF position,
                        int modifiers,
                        int clickCount,
                        ui::Cursor* cursor);

  // Updates the available area
  void OnGeometryChanged(double old_zoom, float old_device_scale);

  // Computes document width/height in device pixels, based on the total scale
  gfx::Size GetDocumentPixelSize();

  void OnViewportChanged(const gfx::Rect& plugin_rect_in_css_pixel,
                         float new_device_scale);

  void UpdateScroll(int64_t y_position);

  float TwipToPx(float in);
  float TotalScale();

  // Exposed methods {
  gfx::Size GetDocumentCSSPixelSize();
  std::vector<gfx::Rect> PageRects();
  void SetZoom(float zoom);
  void InvalidateAllTiles();
  float GetZoom();
  float TwipToCSSPx(float in);

  // updates the first and last intersecting page number within view
  void UpdateIntersectingPages();

  // renders the document in the plugin and assigns a unique key
  std::string RenderDocument(v8::Isolate* isolate,
                             gin::Handle<office::DocumentClient> client,
                             gin::Arguments* args);
  // debounces the renders at the specified interval
  void DebounceUpdates(int interval);

  // }

  // LOK event handlers {
  void HandleInvalidateTiles(std::string payload);
  void HandleDocumentSizeChanged(std::string payload);
  void HandleCursorInvalidated(std::string payload);
  // }

  void DebouncedResumePaint();
  void TryResumePaint();

  // owns this class
  blink::WebPluginContainer* container_;
  void InvalidateWeakContainer();

  // Painting State {
  // plugin rect in CSS pixels
  gfx::Rect css_plugin_rect_;
  // plugin rectangle in device pixels.
  gfx::Rect plugin_rect_;
  // Remaining area, in pixels, to render the view in after accounting for
  // horizontal centering.
  gfx::Rect available_area_;
  gfx::Rect available_area_twips_;
  // browser viewport zoom factor
  double viewport_zoom_ = 1.0;
  // current device scale factor. viewport * device_scale_ == screen, screen /
  // device_scale_ == viewport
  float device_scale_ = 1.0f;
  float zoom_ = 1.0f;
  float old_zoom_ = 1.0f;
  // first paint, requiring the full canvas of tiles to be painted
  bool first_paint_ = true;
  // first paint, requiring the full canvas of tiles to be painted
  bool scale_pending_ = false;
  // currently painting, to track deferred invalidates
  bool in_paint_ = false;
  // the offset for input events, adjusted by the scroll position
  int scroll_y_position_ = 0;
  // If this is true, then don't scroll the plugin in response to calls to
  // `UpdateScroll()`. This will be true when the extension page is in the
  // process of zooming the plugin so that flickering doesn't occur while
  // zooming.
  bool stop_scrolling_ = false;
  // }

  // UI State {
  // current cursor
  ui::mojom::CursorType cursor_type_ = ui::mojom::CursorType::kPointer;
  bool has_focus_;
  std::string last_cursor_rect_;
  base::TimeTicks last_css_cursor_time_ = base::TimeTicks();
  // }

  // owned by
  content::RenderFrame* render_frame_ = nullptr;

  // maybe has a
  office::DocumentHolderWithView document_;
  base::WeakPtr<office::DocumentClient> document_client_;

  // painting
  scoped_refptr<office::TileBuffer> tile_buffer_;
  std::unique_ptr<office::PaintManager> paint_manager_;
  bool take_snapshot_ = true;
  office::Snapshot snapshot_;
  bool scrolling_ = false;
  std::vector<gfx::Rect> page_rects_cached_;
  int first_intersect_ = -1;
  int last_intersect_ = -1;
  base::Token restore_key_;

  bool visible_ = true;
  bool disable_input_ = false;
  bool doomed_ = false;
  bool registered_observers_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  office::CancelFlagPtr paint_cancel_flag_;
  base::TimeTicks last_full_invalidation_time_ = base::TimeTicks();

  v8::Global<v8::ObjectTemplate> v8_template_;
  v8::Global<v8::Object> v8_object_;

  std::unique_ptr<base::DelayTimer> update_debounce_timer_;

  // invalidates when destroy() is called, must be last
  base::WeakPtrFactory<OfficeWebPlugin> weak_factory_{this};
};

}  // namespace electron
