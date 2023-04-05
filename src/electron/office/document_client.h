// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef OFFICE_DOCUMENT_CLIENT_H_
#define OFFICE_DOCUMENT_CLIENT_H_

#include <map>
#include <string>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "office/event_bus.h"
#include "shell/common/gin_helper/pinnable.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "v8/include/v8-persistent-handle.h"

namespace base {
class SequencedTaskRunner;
}

namespace lok {
class Document;
}  // namespace lok

namespace electron::office {

class OfficeClient;

class DocumentClient : public gin::Wrappable<DocumentClient> {
 public:
  // disable copy
  DocumentClient(const DocumentClient&) = delete;
  DocumentClient& operator=(const DocumentClient&) = delete;

  DocumentClient(base::WeakPtr<OfficeClient> office_client,
                 lok::Document* document,
                 std::string path);

  static void HandleLibreOfficeCallback(int type,
                                        const char* payload,
                                        void* callback);

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  // v8 EventBus
  void On(const std::string& event_name,
          v8::Local<v8::Function> listener_callback);
  void Off(const std::string& event_name,
           v8::Local<v8::Function> listener_callback);
  void Emit(const std::string& event_name, v8::Local<v8::Value> data);

  // Exposed to v8 {
  // Loaded and capable of receiving events
  bool IsReady() const;
  std::vector<gfx::Rect> PageRects() const;
  gfx::Size Size() const;
  void PostUnoCommand(const std::string& command, gin::Arguments* args);
  void PostUnoCommandInternal(const std::string& command,
                              char* json_buffer,
                              bool notifyWhenFinished);
  v8::Local<v8::Value> GotoOutline(int idx, gin::Arguments* args);
  std::vector<std::string> GetTextSelection(const std::string& mime_type,
                                            gin::Arguments* args);
  void SetTextSelection(int n_type, int n_x, int n_y);
  v8::Local<v8::Value> GetPartName(int n_part, gin::Arguments* args);
  v8::Local<v8::Value> GetPartHash(int n_part, gin::Arguments* args);
  void SendDialogEvent(uint64_t n_window_id, gin::Arguments* args);
  v8::Local<v8::Value> GetSelectionTypeAndText(const std::string& mime_type,
                                               gin::Arguments* args);
  v8::Local<v8::Value> GetClipboard(gin::Arguments* args);
  bool SetClipboard(std::vector<v8::Local<v8::Object>> clipboard_data,
                    gin::Arguments* args);
  bool OnPasteEvent(ui::Clipboard* clipboard, std::string clipboard_type);
  bool Paste(const std::string& mime_type,
             const std::string& data,
             gin::Arguments* args);
  void SetGraphicSelection(int n_type, int n_x, int n_y);
  void ResetSelection();
  v8::Local<v8::Value> GetCommandValues(const std::string& command,
                                        gin::Arguments* args);
  void SetOutlineState(bool column, int level, int index, bool hidden);
  void SetViewLanguage(int id, const std::string& language);
  void SelectPart(int part, int select);
  void MoveSelectedParts(int position, bool duplicate);
  void RemoveTextContext(unsigned window_id, int before, int after);
  void CompleteFunction(const std::string& function_name);
  void SendFormFieldEvent(const std::string& arguments);
  bool SendContentControlEvent(const v8::Local<v8::Object>& arguments,
                               gin::Arguments* args);
  v8::Local<v8::Value> As(const std::string& type, v8::Isolate* isolate);
  // }

  lok::Document* GetDocument();

  gfx::Size DocumentSizeTwips();

  // returns the view ID associated with the mount
  int Mount(v8::Isolate* isolate);
  // returns if the view was unmounted, if false could be due to one view
  // left/never mounted
  // bool Unmount();

  int GetNumberOfPages() const;

  // Editing State {
  bool CanCopy();
  bool CanUndo();
  bool CanRedo();
  bool CanEditText();
  bool HasEditableText();
  // }

  std::string Path();
  base::WeakPtr<OfficeClient> GetOfficeClient() { return office_client_; }
  base::WeakPtr<EventBus> GetEventBus() { return event_bus_.GetWeakPtr(); }
  void ForwardLibreOfficeEvent(int type, std::string payload);

  void Unmount();

  base::WeakPtr<DocumentClient> GetWeakPtr();

 private:
  DocumentClient();
  ~DocumentClient() override;

  void HandleStateChange(std::string payload);
  void HandleUnoCommandResult(std::string payload);
  void HandleDocSizeChanged(std::string payload);
  void HandleInvalidate(std::string payload);

  void OnClipboardChanged();

  void RefreshSize();

  // has a
  lok::Document* document_ = nullptr;
  std::string path_;
  int view_id_ = -1;
  int tile_mode_;

  long document_height_in_twips_;
  long document_width_in_twips_;

  std::vector<gfx::Rect> page_rects_;
  std::unordered_map<std::string, std::string> uno_state_;

  // holds state changes until the document is mounted
  std::vector<std::string> state_change_buffer_;

  bool is_ready_;
  EventBus event_bus_;
  base::WeakPtr<OfficeClient> office_client_;

  // prevents from being garbage collected
  v8::Global<v8::Value> mounted_;

  base::WeakPtrFactory<DocumentClient> weak_factory_{this};
};

}  // namespace electron::office
#endif  // OFFICE_DOCUMENT_CLIENT_H_
