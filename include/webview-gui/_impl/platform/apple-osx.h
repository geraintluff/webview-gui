#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <objc/runtime.h>
#include <objc/message.h>

namespace webview_gui {

// Objective-C helpers - we use `objc_msgSend` for "simple" return values (not structs, not floating-point)
namespace _objc {
	static constexpr const char *classNameStr = "FetchPP_fetch_handler_block";

	template<typename Return=id, class... Args>
	Return callSimple(id obj, const char *method, Args... args) {
		// We have to cast `objc_msgSend` to a version with the arguments/return types
		auto fn = (Return(*)(id, SEL, Args...))(objc_msgSend);
		return fn(obj, sel_registerName(method), args...);
	}

	template<typename Return=id, class... Args>
	Return callSimple(const char *className, const char *method, Args... args) {
		return callSimple<Return>((id)objc_getClass(className), method, args...);
	}

	template<class... Args>
	void callVoid(Args... args) {
		callSimple<void>(args...);
	}

	id nsNumber(bool b) {
		return callSimple("NSNumber", "numberWithBool:", b);
	}

	id nsString(const char *utf8) {
		return callSimple("NSString", "stringWithUTF8String:", utf8);
	}
}

// No native webview - do absolutely nothing
struct WebviewGui::Impl {
	id webview = nullptr;
	ResourceGetter getter;

	Impl(ResourceGetter g) : Impl() {
		getter = std::move(g);
	}

	Impl() {
		using namespace _objc;
		
		id config = callSimple("WKWebViewConfiguration", "new");
		if (!config) return;
		// Wait until everything's ready until showing anything
		callVoid(config, "setSuppressesIncrementalRendering:", nsNumber(true));
		
		id preferences = callSimple(config, "preferences");
		callVoid(preferences, "setElementFullscreenEnabled:", nsNumber(false));
		// Require click/etc. to open popups
		callVoid(preferences, "setJavaScriptCanOpenWindowsAutomatically:", nsNumber(false));
		
		webview = callSimple("WKWebView", "alloc");
		CGRect frame{{0, 0}, {100, 100}};
		if (webview) webview = callSimple(webview, "initWithFrame:configuration:", frame, config);
		callVoid(config, "release");
	}
	
	~Impl() {
		using namespace _objc;
		if (webview) callVoid(webview, "removeFromSuperview");
	}
};

bool WebviewGui::supports(Platform p) {
	return p == Platform::COCOA;
}
WebviewGui * WebviewGui::create(Platform platform, const std::string &startPath, ResourceGetter getter) {
	using namespace _objc;
	id baseUrl = callSimple("NSURL", "URLWithString:", "webview-gui://");
	id url = callSimple("NSURL", "URLWithString:relativeToURL:", nsString(startPath.c_str()), baseUrl);
	auto *request = _objc::callSimple("NSMutableURLRequest", "requestWithURL:", url);
	
	auto *impl = new Impl();
	//callSimple(impl->webview, "loadRequest:", request);
	callSimple(impl->webview, "loadHTMLString:baseURL:", nsString("custom ResourceGetter not implemented yet"), baseUrl);
	return new WebviewGui(impl);
}
WebviewGui * WebviewGui::create(Platform platform, const std::string &startPath, const std::string &baseDir) {
	using namespace _objc;
	id baseUrl = callSimple("NSURL", "fileURLWithPath:isDirectory:", nsString(baseDir.c_str()), nsNumber(true));
	auto *startPathC = startPath.c_str();
	if (startPathC[0] == '/') ++startPathC; // skip any leading `/`, since we have a base directory
	id url = callSimple("NSURL", "URLWithString:relativeToURL:", nsString(startPathC), baseUrl);
	auto *impl = new Impl();
	if (callSimple<bool>(url, "fileUrl")) {
		callSimple(impl->webview, "loadFileURL:allowingReadAccessToURL:", url, baseUrl);
	} else {
		auto *request = _objc::callSimple("NSMutableURLRequest", "requestWithURL:", url);
LOG_EXPR(request);
		callSimple(impl->webview, "loadRequest:", request);
	}
	return new WebviewGui(impl);
}

WebviewGui::WebviewGui(WebviewGui::Impl *impl) : impl(impl) {}
WebviewGui::~WebviewGui() {
	delete impl;
}
void WebviewGui::attach(void *platformNative) {
	using namespace _objc;
	callVoid((id)platformNative, "addSubview:", impl->webview);
}
void WebviewGui::send(const unsigned char *, size_t) {}
void WebviewGui::setSize(double width, double height) {
	using namespace _objc;
	CGRect rect{{0, 0}, {width, height}};
	callSimple(impl->webview, "setFrame:", rect);
}
void WebviewGui::setVisible(bool visible) {}

} // namespace

