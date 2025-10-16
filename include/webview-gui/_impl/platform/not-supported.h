#pragma once

namespace webview_gui {

// No native webview - do absolutely nothing
struct WebviewGui::Impl {};
bool WebviewGui::supports(Platform p) {
	return false;
}
WebviewGui * WebviewGui::create(Platform platform, const std::string &startPath, ResourceGetter getter) {
	return nullptr;
}
WebviewGui * WebviewGui::create(Platform platform, const std::string &startPath, const std::string &baseDir) {
	return nullptr;
}

// None of these should ever be called, because no instances can ever be created
WebviewGui::WebviewGui(WebviewGui::Impl *) {}
WebviewGui::~WebviewGui() {}
void WebviewGui::attach(void *platformNative) {}
void WebviewGui::send(const unsigned char *, size_t) {}
void WebviewGui::setSize(double width, double height) {}
void WebviewGui::setVisible(bool visible) {}

} // namespace
