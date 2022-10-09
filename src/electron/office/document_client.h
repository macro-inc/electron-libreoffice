// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_DOCUMENT_CLIENT_H_
#define OFFICE_DOCUMENT_CLIENT_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "office/event_bus.h"
#include "third_party/libreofficekit/LibreOfficeKit.hxx"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

// prevent circular include
#ifndef OFFICE_DOCUMENT_CLIENT_DEFINED
#define OFFICE_DOCUMENT_CLIENT_DEFINED
namespace electron::office {
class DocumentClient;
}
#include "office/office_web_plugin.h"
#endif

namespace base {
class SequencedTaskRunner;
}

namespace electron::office {

class DocumentClient : public gin::Wrappable<DocumentClient> {
 public:
  DocumentClient();
  ~DocumentClient() override;
  // disable copy
  DocumentClient(const DocumentClient&) = delete;
  DocumentClient& operator=(const DocumentClient&) = delete;

  DocumentClient(lok::Office* office,
                 lok::Document* document,
                 std::string path);

  static gin::Handle<DocumentClient> Create(v8::Isolate* isolate,
                                            lok::Office* office,
                                            lok::Document* document,
                                            std::string path);

  static void HandleLOKCallback(int type, const char* payload, void* callback);

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  // Loaded and capable of receiving events
  bool IsReady();

  lok::Document* GetDocument();

  gfx::SizeF DocumentSizePx();
  gfx::Size DocumentSizeTwips();

  // returns the view ID associated with the mount
  int Mount(OfficeWebPlugin* renderer);
  // returns if the view was unmounted, if false could be due to one view
  // left/never mounted
  bool Unmount();

  // Plugin Engine {
  void PageOffsetUpdated(const gfx::Vector2d& page_offset);
  void PluginSizeUpdated(const gfx::Size& size);
  void PrePaint();
  void Paint(const gfx::Rect& rect,
             SkBitmap& image_data,
             std::vector<gfx::Rect>& ready,
             std::vector<gfx::Rect>& pending);
  void PostPaint();
  int GetNumberOfPages() const;
  gfx::Rect GetPageScreenRect(int page_index) const;
  void ZoomUpdated(double new_zoom_level);
  // }

  // Editing State {
  bool CanCopy();
  bool CanUndo();
  bool CanRedo();
  bool CanEditText();
  bool HasEditableText();
  // }

  base::WeakPtr<DocumentClient> GetWeakPtr();

 protected:
  // nothing
 private:
  void EmitLOKEvent(int type, std::string payload);
  void HandleStateChange(const char* payload);
  void HandleLOKEvent(int type, const char* payload);
  void RefreshSize();
  void Invalidate(const char* payload);

  // owned by
  lok::Office* office_;
  OfficeWebPlugin* renderer_ = nullptr;

  // has a
  lok::Document* document_;
  std::string path_;
  int view_id_ = -1;
  LibreOfficeKitTileMode tile_mode_;

  float zoom_ = 1.0;
  long document_height_in_twips_;
  long document_width_in_twips_;
  gfx::SizeF document_size_px_;
  gfx::Size visible_area_;
  gfx::Vector2d page_offest_;

  std::vector<gfx::Rect> page_rects_;
  std::unordered_map<std::string, std::string> uno_state_;

  // holds state changes until the document is mounted
  std::vector<std::string> state_change_buffer_;

  bool is_ready_;
  EventBus event_bus_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<DocumentClient> weak_factory_{this};
};

}  // namespace electron::office
#endif  // OFFICE_DOCUMENT_CLIENT_H_
