#pragma once

#include "../helpers.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CGGeometry.h>
#include <objc/runtime.h>
#include <objc/message.h>

namespace webview_gui {

// Objective-C helpers - we use `objc_msgSend` for "simple" return values (not structs, not floating-point)
namespace _objc {
	template<typename Return=id, class... Args>
	Return callSimple(id obj, const char *method, Args... args) {
		// We have to cast `objc_msgSend` to a version with the arguments/return types
		auto fn = (Return(*)(id, SEL, Args...))(objc_msgSend);
		auto selector = sel_registerName(method);
		if (!class_respondsToSelector(object_getClass(obj), selector)) return Return();
		return fn(obj, selector, args...);
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
	
	bool instanceOf(id obj, const char *className) {
		return callSimple<bool>(obj, "isKindOfClass:", (id)objc_getClass(className));
	}
	
	struct ScopedRelease {
		id obj;
		ScopedRelease(id obj) : obj(obj) {}
		~ScopedRelease() {
			if (obj) callVoid(obj, "release");
		}
	};
#define SCOPED_RELEASE(obj) _objc::ScopedRelease release_##obj{obj};
}

struct WebviewGui::Impl {
	WebviewGui *main = nullptr;
	id webview = nullptr;
	id messageHandler = nullptr, schemeHandler = nullptr;
	ResourceGetter getter;

	static constexpr const char * associatedObjectKey = "WebviewGui::Impl";

	static void messageHandlerImpl(id self, SEL, id /*controller*/, id message) {
		using namespace _objc;
		auto *impl = (Impl *)objc_getAssociatedObject(self, associatedObjectKey);
		if (!impl || !impl->main) return;

		auto &receiveFn = impl->main->receive;
		if (!receiveFn) return;

		id body = callSimple(message, "body");
		if (instanceOf(body, "NSString")) {
			auto *base64 = callSimple<const char *>(body, "UTF8String");
			auto binary = _helpers::decodeBase64(base64);
			receiveFn((const unsigned char *)binary.data(), binary.size());
		}
	}
	
	static id createMessageHandlerClass() {
		using namespace _objc;
		static constexpr const char *className = "WebviewGUI_WKScriptMessageHandler";
		id alreadyRegistered = (id)objc_getClass(className);
		if (alreadyRegistered) return alreadyRegistered;
		
		auto handlerClass = objc_allocateClassPair(objc_getClass("NSObject"), className, 0);
		class_addProtocol(handlerClass, objc_getProtocol("WKScriptMessageHandler"));
		class_addMethod(handlerClass, sel_registerName("userContentController:didReceiveScriptMessage:"), (IMP)messageHandlerImpl, "v@:@@");
		objc_registerClassPair(handlerClass);
		
		return (id)objc_getClass(className);
	}

	static void schemeHandlerStartImpl(id self, SEL, id /*webview*/, id urlSchemeTask) {
		using namespace _objc;
		auto *impl = (Impl *)objc_getAssociatedObject(self, associatedObjectKey);
		if (!impl || !impl->main) return;
		id request = callSimple(urlSchemeTask, "request");
		id url = callSimple(request, "URL");
		id path = callSimple(url, "path");
		auto *pathStr = callSimple<const char *>(path, "UTF8String");
		Resource resource;
		resource.mediaType = _helpers::guessMediaType(pathStr);
		if (!impl->getter(pathStr, resource)) {
			id response = callSimple("NSHTTPURLResponse", "alloc");
			SCOPED_RELEASE(response);
			response = callSimple(
				response,
				"initWithURL:statusCode:HTTPVersion:headerFields:",
				url,
				long(404),
				nsString("HTTP/1.1"),
				nil
			);
			callSimple(urlSchemeTask, "didReceiveResponse:", response);
			callSimple(urlSchemeTask, "didFinish");
			return;
		}

		id headerNames[] = {nsString("Content-Length"), nsString("Content-Type"), nsString("Cache-Control"), nsString("Access-Control-Allow-Origin")};
		auto lengthStr = std::to_string(resource.bytes.size());
		id headerValues[] = {nsString(lengthStr.c_str()), nsString(resource.mediaType.c_str()), nsString("no-store"), nsString("*")};
		id headers = callSimple("NSDictionary", "dictionaryWithObjects:forKeys:count:", headerValues, headerNames, (unsigned long)4);

		id response = callSimple("NSHTTPURLResponse", "alloc");
		response = callSimple(
			response,
			"initWithURL:statusCode:HTTPVersion:headerFields:",
			url,
			long(200),
			nsString("HTTP/1.1"),
			headers
		);
		SCOPED_RELEASE(response);
		callSimple(urlSchemeTask, "didReceiveResponse:", response);
		id data = callSimple("NSData", "dataWithBytes:length:", (const void *)resource.bytes.data(), (unsigned long)resource.bytes.size());
		callSimple(urlSchemeTask, "didReceiveData:", data);
		callSimple(urlSchemeTask, "didFinish");
	}
	static void schemeHandlerStopImpl(id self, SEL, id /*webview*/, id urlSchemeTask) {
	}
	static id createSchemeHandlerClass() {
		using namespace _objc;
		static constexpr const char *className = "WebviewGUI_WKURLSchemeHandler";
		id alreadyRegistered = (id)objc_getClass(className);
		if (alreadyRegistered) return alreadyRegistered;
		
		auto handlerClass = objc_allocateClassPair(objc_getClass("NSObject"), className, 0);
		class_addProtocol(handlerClass, objc_getProtocol("WKURLSchemeHandler"));
		class_addMethod(handlerClass, sel_registerName("webView:startURLSchemeTask:"), (IMP)schemeHandlerStartImpl, "v@:@@");
		class_addMethod(handlerClass, sel_registerName("webView:stopURLSchemeTask:"), (IMP)schemeHandlerStopImpl, "v@:@@");
		objc_registerClassPair(handlerClass);
		
		return (id)objc_getClass(className);
	}

	Impl(ResourceGetter g={}) {
		getter = std::move(g);

		using namespace _objc;
		static id messageHandlerClass = createMessageHandlerClass();
		
		id config = callSimple("WKWebViewConfiguration", "new");
		if (!config) return;
		// Wait until everything's ready until showing anything
		callVoid(config, "setSuppressesIncrementalRendering:", nsNumber(true));
		
		if (getter) {
			static id schemeHandlerClass = createSchemeHandlerClass();
			schemeHandler = callSimple(schemeHandlerClass, "new");
			objc_setAssociatedObject(schemeHandler, associatedObjectKey, (id)this, OBJC_ASSOCIATION_ASSIGN);
			callSimple(config, "setURLSchemeHandler:forURLScheme:", schemeHandler, nsString("webview-gui"));
		}
		
		id preferences = callSimple(config, "preferences");
		callVoid(preferences, "setElementFullscreenEnabled:", nsNumber(false));
		// Require click/etc. to open popups
		callVoid(preferences, "setJavaScriptCanOpenWindowsAutomatically:", nsNumber(false));
		
		id contentController = callSimple(config, "userContentController");
		const char *initJs = R"JS(
			if (!Uint8Array.prototype.toBase64) {
				Uint8Array.prototype.toBase64 = function() {
					let binaryString = "";
					for (var i = 0; i < this.length; i++) {
						binaryString += String.fromCharCode(this[i]);
					}
					return btoa(binaryString);
				};
			}
			if (!Uint8Array.fromBase64) {
				Uint8Array.fromBase64 = b64 => {
					let binaryString = atob(b64);
					let array = new Uint8Array(b64.length);
					for (let i=0; i < array.length; ++i) {
						array[i] = binaryString.charCodeAt(i);
					}
					return array;
				};
			}
			window.addEventListener('message', e=>{
				if (e.source == window) { // this happens if we attempt to send using `window.parent` from the main frame
					e.stopImmediatePropagation();
					let data = e.data;
					if (data instanceof ArrayBuffer) data = new Uint8Array(data);
					window.webkit.messageHandlers.webviewGui_receive.postMessage(data.toBase64());
				}
			}, {capture: true});
		)JS";
		id initScript = callSimple("WKUserScript", "alloc");
		SCOPED_RELEASE(initScript);
		if (initScript) initScript = callSimple(initScript, "initWithSource:injectionTime:forMainFrameOnly:", nsString(initJs), int(0)/*WKUserScriptInjectionTimeAtDocumentStart*/, true);
		if (!initScript) return;
		callSimple(contentController, "addUserScript:", initScript);
		
		messageHandler = callSimple(messageHandlerClass, "new");
		objc_setAssociatedObject(messageHandler, associatedObjectKey, (id)this, OBJC_ASSOCIATION_ASSIGN);
		callSimple(contentController, "addScriptMessageHandler:name:", messageHandler, nsString("webviewGui_receive"));
		callVoid(messageHandler, "release");
		
		webview = callSimple("WKWebView", "alloc");
		CGRect frame{{0, 0}, {100, 100}};
		if (webview) webview = callSimple(webview, "initWithFrame:configuration:", frame, config);
	}
	
	~Impl() {
		using namespace _objc;
		if (messageHandler) objc_setAssociatedObject(messageHandler, associatedObjectKey, (id)nullptr, OBJC_ASSOCIATION_ASSIGN);
		if (schemeHandler) objc_setAssociatedObject(schemeHandler, associatedObjectKey, (id)nullptr, OBJC_ASSOCIATION_ASSIGN);
		if (webview) {
			callVoid(webview, "removeFromSuperview");
			callVoid(webview, "release");
		}
	}
};

bool WebviewGui::supports(Platform p) {
	return p == Platform::COCOA;
}
WebviewGui * WebviewGui::create(Platform platform, const std::string &startPath, ResourceGetter getter) {
	if (!supports(platform)) return nullptr;
	
	using namespace _objc;
	id baseUrl = callSimple("NSURL", "URLWithString:", nsString("webview-gui://"));
	if (!baseUrl) return nullptr;
	id url = callSimple("NSURL", "URLWithString:relativeToURL:", nsString(startPath.c_str()), baseUrl);
	auto *request = _objc::callSimple("NSMutableURLRequest", "requestWithURL:", url);
	
	auto *impl = new Impl(std::move(getter));
	if (!impl->webview) {
		delete impl;
		return nullptr;
	}
	callSimple(impl->webview, "loadRequest:", request);
	return new WebviewGui(impl);
}
WebviewGui * WebviewGui::create(Platform platform, const std::string &startUrl) {
	if (!supports(platform)) return nullptr;

	using namespace _objc;
	id url = callSimple("NSURL", "URLWithString:", nsString(startUrl.c_str()));
	if (!url) return nullptr;

	auto *impl = new Impl();
	if (!impl->webview) {
		delete impl;
		return nullptr;
	}
	auto *request = _objc::callSimple("NSMutableURLRequest", "requestWithURL:", url);
	callSimple(impl->webview, "loadRequest:", request);
	return new WebviewGui(impl);
}
WebviewGui * WebviewGui::create(Platform platform, const std::string &startPathOrUrl, const std::string &baseDir) {
	if (!baseDir.size()) return create(platform, startPathOrUrl);
	if (!supports(platform)) return nullptr;

	using namespace _objc;
	id baseUrl = callSimple("NSURL", "fileURLWithPath:isDirectory:", nsString(baseDir.c_str()), nsNumber(true));
	if (!baseUrl) return nullptr;
	
	auto *startUrlC = startPathOrUrl.c_str();
	if (startUrlC[0] == '/') ++startUrlC; // skip any leading `/`, since we have a base directory
	id url = callSimple("NSURL", "URLWithString:relativeToURL:", nsString(startUrlC), baseUrl);
	if (!url) return nullptr;
	
	auto *impl = new Impl();
	if (!impl->webview) {
		delete impl;
		return nullptr;
	}

	if (callSimple<bool>(url, "isFileURL")) {
		callSimple(impl->webview, "loadFileURL:allowingReadAccessToURL:", url, baseUrl);
	} else {
		auto *request = _objc::callSimple("NSMutableURLRequest", "requestWithURL:", url);
		callSimple(impl->webview, "loadRequest:", request);
	}
	return new WebviewGui(impl);
}

WebviewGui::WebviewGui(WebviewGui::Impl *impl) : impl(impl) {
	impl->main = this;
}
WebviewGui::~WebviewGui() {
	delete impl;
}
void WebviewGui::attach(void *platformNative) {
	using namespace _objc;
	callVoid((id)platformNative, "addSubview:", impl->webview);
}
void WebviewGui::send(const unsigned char *bytes, size_t length) {
	std::string js = "window.dispatchEvent(new MessageEvent('message',{data:Uint8Array.fromBase64('";
	_helpers::encodeBase64(bytes, length, js);
	js += "').buffer}))";

	using namespace _objc;
	callSimple(impl->webview, "evaluateJavaScript:completionHandler:", nsString(js.c_str()), (id)nullptr);
}
void WebviewGui::setSize(double width, double height) {
	using namespace _objc;
	CGRect rect{{0, 0}, {width, height}};
	callSimple(impl->webview, "setFrame:", rect);
}
void WebviewGui::setVisible(bool visible) {}

} // namespace

#undef SCOPED_RELEASE
