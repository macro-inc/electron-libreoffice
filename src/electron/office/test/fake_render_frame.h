// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#pragma once

#include "content/public/renderer/render_frame.h"

namespace content {
class RenderFrameImpl : public content::RenderFrame {
 public:
  explicit RenderFrameImpl(bool /* no-op */);
  ~RenderFrameImpl() override;
  // static RenderFrame* FromWebFrame(blink::WebLocalFrame* web_frame) override;
  // static RenderFrame* FromRoutingID(int routing_id) override;
  // static void ForEach(content::RenderFrameVisitor* visitor) override;
  content::RenderFrame* GetMainRenderFrame() override;
  content::RenderAccessibility* GetRenderAccessibility() override;
  std::unique_ptr<content::AXTreeSnapshotter> CreateAXTreeSnapshotter(
      ui::AXMode ax_mode) override;
  int GetRoutingID() override;
  blink::WebView* GetWebView() override;
  const blink::WebView* GetWebView() const override;
  blink::WebLocalFrame* GetWebFrame() override;
  const blink::WebLocalFrame* GetWebFrame() const override;
  const blink::web_pref::WebPreferences& GetBlinkPreferences() override;
  void ShowVirtualKeyboard() override;
  blink::WebPlugin* CreatePlugin(const content::WebPluginInfo& info,
                                 const blink::WebPluginParams& params) override;
  void ExecuteJavaScript(const std::u16string& javascript) override;
  bool IsMainFrame() override;
  bool IsInFencedFrameTree() const override;
  bool IsHidden() override;
  void BindLocalInterface(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) override;
  blink::BrowserInterfaceBrokerProxy* GetBrowserInterfaceBroker() override;
  blink::AssociatedInterfaceRegistry* GetAssociatedInterfaceRegistry() override;
  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;
  void SetSelectedText(const std::u16string& selection_text,
                       size_t offset,
                       const gfx::Range& range) override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override;
  bool IsPasting() override;
  void LoadHTMLStringForTesting(const std::string& html,
                                const GURL& base_url,
                                const std::string& text_encoding,
                                const GURL& unreachable_url,
                                bool replace_current_item) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      blink::TaskType task_type) override;
  int GetEnabledBindings() override;
  void SetAccessibilityModeForTest(ui::AXMode new_mode) override;
  const content::RenderFrameMediaPlaybackOptions&
  GetRenderFrameMediaPlaybackOptions() override;
  void SetRenderFrameMediaPlaybackOptions(
      const content::RenderFrameMediaPlaybackOptions& opts) override;
  void SetAllowsCrossBrowsingInstanceFrameLookup() override;
  gfx::RectF ElementBoundsInWindow(const blink::WebElement& element) override;
  void ConvertViewportToWindow(gfx::Rect* rect) override;
  float GetDeviceScaleFactor() override;
  blink::scheduler::WebAgentGroupScheduler& GetAgentGroupScheduler() override;

  bool OnMessageReceived(const IPC::Message& message) override;
  bool Send(IPC::Message* msg) override;
  content::RenderView* GetRenderView() override;
  void PluginDidStartLoading() override;
  void PluginDidStopLoading() override;
  bool IsBrowserSideNavigationPending() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
};
}  // namespace content
