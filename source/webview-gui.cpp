#include "../include/webview-gui/webview-gui.h"

#include "./choc/gui/choc_WebView.h"

#include <iostream>
#define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;

#if CHOC_APPLE
#include <CoreFoundation/CFBundle.h>

struct WebviewGui::Impl {
	Impl(const choc::ui::WebView::Options &options) : main(main), webview(options) {}
	
	~Impl() {
		using namespace choc::objc;
		id subview = (id)webview.getViewHandle();
		call<void>(subview, "removeFromSuperview");
	}
	
	void attach(void *nativeView) {
		using namespace choc::objc;
		id parent = (id)nativeView;
		id subview = (id)webview.getViewHandle();
		call<void>(parent, "addSubview:", subview);
	}
	void setSize(double width, double height) {
		using namespace choc::objc;
		struct CGRect rect = {0, 0, CGFloat(width), CGFloat(height)};
		id subview = (id)webview.getViewHandle();
		call<void>(subview, "setFrame:", rect);
	}

	WebviewGui *main;
	choc::ui::WebView webview;
};
#else
struct WebviewGui::Impl {
	Impl(const choc::ui::WebView::Options &options) : main(main), webview(options) {}
	
	void attach(void *parent) {
		LOG_EXPR(parent);
	}
	void setSize(double width, double height) {
		LOG_EXPR(width);
		LOG_EXPR(height);
	}

	WebviewGui *main;
	choc::ui::WebView webview;
};
#endif

WebviewGui * WebviewGui::create(WebviewGui::Platform p, const char *startPathCStr, size_t width, size_t height) {
	if (!supports(p)) return nullptr;

	choc::ui::WebView::Options options;
	options.acceptsFirstMouseClick = true;
	options.transparentBackground = true;
	std::string startPath = startPathCStr;
	options.webviewIsReady = [startPath](choc::ui::WebView &wv){
		wv.setHTML("<body style=\"background:blue;color:white;display:flex;align-items:center;justify-content:center;\">HTML: " + startPath + "</body>");
		//wv.navigate(startPath);
	};
	
	auto *impl = new WebviewGui::Impl(options);
	if (!impl->webview.loadedOK()) {
		delete impl;
		return nullptr;
	}
	auto *ptr = new WebviewGui(impl);
	ptr->setSize(width, height);
	return ptr;
}
WebviewGui::WebviewGui(WebviewGui::Impl *impl) : impl(impl) {
	impl->main = this;
}
WebviewGui::~WebviewGui() {
	delete impl;
}

bool WebviewGui::supports(WebviewGui::Platform p) {
	return (p != NONE);
}
void WebviewGui::attach(void *platformNative) {
	impl->attach(platformNative);
}
void WebviewGui::send(const unsigned char *bytes, size_t length) {
	//impl->send(bytes, length);
}
void WebviewGui::setSize(double width, double height) {
	impl->setSize(width, height);
}
void WebviewGui::setVisible(bool visible) {}
