// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "fake_render_frame.h"
#include "base/threading/thread_task_runner_handle.h"

namespace blink::scheduler {
class WebAgentGroupScheduler {};
}  // namespace blink::scheduler

namespace blink::web_pref {
struct WebPreferences {};
}  // namespace blink::web_pref

namespace IPC {
std::string Listener::ToDebugString() {
  return "FakeIPCListener";
}
}  // namespace IPC

namespace content {
struct RenderFrameMediaPlaybackOptions {};

RenderFrameImpl::RenderFrameImpl(bool) {}
RenderFrameImpl::~RenderFrameImpl() = default;

content::RenderFrame* RenderFrameImpl::GetMainRenderFrame() {
  return this;
}

content::RenderAccessibility* RenderFrameImpl::GetRenderAccessibility() {
  return nullptr;
}

std::unique_ptr<content::AXTreeSnapshotter>
RenderFrameImpl::CreateAXTreeSnapshotter(ui::AXMode ax_mode) {
  return nullptr;
}

int RenderFrameImpl::GetRoutingID() {
  return -1;
}

blink::WebView* RenderFrameImpl::GetWebView() {
  return nullptr;
}

const blink::WebView* RenderFrameImpl::GetWebView() const {
  return nullptr;
}

blink::WebLocalFrame* RenderFrameImpl::GetWebFrame() {
  return nullptr;
}

const blink::WebLocalFrame* RenderFrameImpl::GetWebFrame() const {
  return nullptr;
}

const blink::web_pref::WebPreferences& RenderFrameImpl::GetBlinkPreferences() {
  static blink::web_pref::WebPreferences dummy_preferences;
  return dummy_preferences;
}

void RenderFrameImpl::ShowVirtualKeyboard() {}

blink::WebPlugin* RenderFrameImpl::CreatePlugin(
    const content::WebPluginInfo& info,
    const blink::WebPluginParams& params) {
  return nullptr;
}

void RenderFrameImpl::ExecuteJavaScript(const std::u16string& javascript) {}

bool RenderFrameImpl::IsMainFrame() {
  return false;
}

bool RenderFrameImpl::IsInFencedFrameTree() const {
  return false;
}

bool RenderFrameImpl::IsHidden() {
  return false;
}

void RenderFrameImpl::BindLocalInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {}

blink::BrowserInterfaceBrokerProxy*
RenderFrameImpl::GetBrowserInterfaceBroker() {
  return nullptr;
}

blink::AssociatedInterfaceRegistry*
RenderFrameImpl::GetAssociatedInterfaceRegistry() {
  return nullptr;
}

blink::AssociatedInterfaceProvider*
RenderFrameImpl::GetRemoteAssociatedInterfaces() {
  return nullptr;
}

void RenderFrameImpl::SetSelectedText(const std::u16string& selection_text,
                                      size_t offset,
                                      const gfx::Range& range) {}

void RenderFrameImpl::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {}

bool RenderFrameImpl::IsPasting() {
  return false;
}

void RenderFrameImpl::LoadHTMLStringForTesting(const std::string& html,
                                               const GURL& base_url,
                                               const std::string& text_encoding,
                                               const GURL& unreachable_url,
                                               bool replace_current_item) {}

scoped_refptr<base::SingleThreadTaskRunner> RenderFrameImpl::GetTaskRunner(
    blink::TaskType task_type) {
  return base::ThreadTaskRunnerHandle::Get();
}

int RenderFrameImpl::GetEnabledBindings() {
  return 0;
}

void RenderFrameImpl::SetAccessibilityModeForTest(ui::AXMode new_mode) {}

const content::RenderFrameMediaPlaybackOptions&
RenderFrameImpl::GetRenderFrameMediaPlaybackOptions() {
  static content::RenderFrameMediaPlaybackOptions dummy_options;
  return dummy_options;
}

void RenderFrameImpl::SetRenderFrameMediaPlaybackOptions(
    const content::RenderFrameMediaPlaybackOptions& opts) {}

void RenderFrameImpl::SetAllowsCrossBrowsingInstanceFrameLookup() {}

gfx::RectF RenderFrameImpl::ElementBoundsInWindow(
    const blink::WebElement& element) {
  return gfx::RectF();
}

void RenderFrameImpl::ConvertViewportToWindow(gfx::Rect* rect) {}

float RenderFrameImpl::GetDeviceScaleFactor() {
  return 1.0f;
}

blink::scheduler::WebAgentGroupScheduler&
RenderFrameImpl::GetAgentGroupScheduler() {
  static blink::scheduler::WebAgentGroupScheduler dummy_scheduler;
  return dummy_scheduler;
}

bool RenderFrameImpl::OnMessageReceived(const IPC::Message& message) {
  return false;
}

bool RenderFrameImpl::Send(IPC::Message* msg) {
  return false;
}

content::RenderView* RenderFrameImpl::GetRenderView() {
  return nullptr;
}

void RenderFrameImpl::PluginDidStartLoading() {}

void RenderFrameImpl::PluginDidStopLoading() {}

bool RenderFrameImpl::IsBrowserSideNavigationPending() {
  return false;
}

scoped_refptr<network::SharedURLLoaderFactory>
RenderFrameImpl::GetURLLoaderFactory() {
  return nullptr;
}

}  // namespace content
