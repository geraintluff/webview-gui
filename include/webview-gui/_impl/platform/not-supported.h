#pragma once

namespace webview_gui {

// No native webview - do absolutely nothing
struct WebviewGui::Impl {};
bool WebviewGui::supports(Platform) {
	return false;
}
WebviewGui * WebviewGui::create(Platform, const std::string &) {
	return nullptr;
}
WebviewGui * WebviewGui::create(Platform, const std::string &, const std::string &) {
	return nullptr;
}
WebviewGui * WebviewGui::create(Platform, const std::string &, ResourceGetter) {
	return nullptr;
}

// None of these should ever be called, because no instances can ever be created
WebviewGui::WebviewGui(WebviewGui::Impl *) {}
WebviewGui::~WebviewGui() {}
void WebviewGui::attach(void *) {}
void WebviewGui::send(const unsigned char *, size_t) {}
void WebviewGui::setSize(double, double) {}
void WebviewGui::setVisible(bool) {}

} // namespace
