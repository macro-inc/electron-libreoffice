// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/atomic_ref_count.h"
#include "base/memory/weak_ptr.h"
#include "base/token.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/wrappable.h"
#include "office/destroyed_observer.h"
#include "office/document_event_observer.h"
#include "office/document_holder.h"
#include "office/renderer_transferable.h"
#include "office/v8_callback.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "v8/include/v8-persistent-handle.h"

namespace lok {
class Document;
}  // namespace lok

namespace electron::office {

class OfficeClient;

class DocumentClient : public gin::Wrappable<DocumentClient>,
                       public DocumentEventObserver,
                       public DestroyedObserver {
 public:
  DocumentClient();
  ~DocumentClient() override;

  explicit DocumentClient(DocumentHolderWithView holder);

  // disable copy
  DocumentClient(const DocumentClient&) = delete;
  DocumentClient& operator=(const DocumentClient&) = delete;

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  // v8 EventBus
  void On(v8::Isolate* isolate,
          const std::u16string& event_name,
          v8::Local<v8::Function> listener_callback);
  void Off(const std::u16string& event_name,
           v8::Local<v8::Function> listener_callback);
  void Emit(v8::Isolate* isolate,
            const std::u16string& event_name,
            v8::Local<v8::Value> data);

  // Exposed to v8 {
  // Loaded and capable of receiving events
  bool IsReady() const;
  std::vector<gfx::Rect> PageRects() const;
  gfx::Size Size() const;
  void SetAuthor(const std::string& author, gin::Arguments* args);
  void PostUnoCommand(const std::string& command, gin::Arguments* args);
  void PostUnoCommandInternal(const std::string& command,
                              std::unique_ptr<char[]> json_buffer,
                              bool notifyWhenFinished);
  v8::Local<v8::Value> GotoOutline(int idx, gin::Arguments* args);
  v8::Local<v8::Promise> SaveToMemory(v8::Isolate* isolate,
                                      gin::Arguments* args);
  v8::Local<v8::Promise> SaveAs(v8::Isolate* isolate, gin::Arguments* args);
  void SetTextSelection(int n_type, int n_x, int n_y);
  v8::Local<v8::Value> GetClipboard(gin::Arguments* args);
  bool SetClipboard(std::vector<v8::Local<v8::Object>> clipboard_data,
                    gin::Arguments* args);
  bool Paste(const std::string& mime_type,
             const std::string& data,
             gin::Arguments* args);
  void SetGraphicSelection(int n_type, int n_x, int n_y);
  void ResetSelection();
  v8::Local<v8::Promise> GetCommandValues(const std::string& command,
                                          gin::Arguments* args);
  v8::Local<v8::Value> As(const std::string& type, v8::Isolate* isolate);
  // }

  // DocumentEventObserver
  void DocumentCallback(int type, std::string payload) override;

  // DestroyedObserver
  void OnDestroyed() override;

  gfx::Size DocumentSizeTwips();

  // returns true if this is the first mount for the document
  bool Mount(v8::Isolate* isolate);
  // return true if this is the last remaining mount for the document
  bool Unmount();

  void MarkRendererWillRemount(base::Token restore_key,
                               RendererTransferable&& renderer_transferable);
  RendererTransferable GetRestoredRenderer(const base::Token& restore_key);

  int GetNumberOfPages() const;

  // Editing State {
  bool CanUndo();
  bool CanRedo();
  // }

  std::string Path();

  v8::Local<v8::Value> NewView(v8::Isolate* isolate);

  DocumentHolderWithView GetDocument();

  base::WeakPtr<DocumentClient> GetWeakPtr();

 private:
  void HandleStateChange(const std::string& payload);
  void HandleUnoCommandResult(const std::string& payload);
  void HandleDocSizeChanged();
  void HandleInvalidate();

  void RefreshSize();

  void EmitReady(v8::Isolate* isolate, v8::Global<v8::Context> context);
  void ForwardEmit(int type, const std::string& payload);

  v8::Local<v8::Promise> InitializeForRendering(v8::Isolate* isolate);

  // has a
  DocumentHolderWithView document_holder_;

  long document_height_in_twips_;
  long document_width_in_twips_;

  std::vector<gfx::Rect> page_rects_;

  // holds state changes until the document is mounted
  std::vector<std::string> state_change_buffer_;

  bool is_ready_;
  std::unordered_map<base::Token, RendererTransferable, base::TokenHash>
      tile_buffers_to_restore_;
  // this doesn't really need to be atomic since all access should remain on the
  // same renderer thread, but it makes the intention of its use clearer
  base::AtomicRefCount mount_counter_;

  std::unordered_map<int, std::vector<SafeV8Function>> event_listeners_;
  // used to track what has a registered observer
  std::unordered_set<int> event_types_registered_;

  bool can_undo_ = false;
  bool can_redo_ = false;

  raw_ptr<v8::Isolate> isolate_ = nullptr;

  // prevents from being garbage collected
  v8::Global<v8::Value> mounted_;

  base::WeakPtrFactory<DocumentClient> weak_factory_{this};
};

// This only exists so that ForwardEmit doeesn't need to use trickery to invoke
// callbacks
struct EventPayload {
  EventPayload(const int type, const std::string& payload)
      : type(type), payload(payload) {}
  const int type;
  const std::string& payload;
};

}  // namespace electron::office

namespace gin {
using namespace electron::office;
template <>
struct Converter<EventPayload> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const EventPayload& val) {
    Dictionary dict = Dictionary::CreateEmpty(isolate);
    dict.Set("payload", lok_callback::PayloadToLocalValue(isolate, val.type,
                                                          val.payload.c_str()));
    return ConvertToV8(isolate, dict);
  }
};
}  // namespace gin
