#include "../../helpers.h"

#include "choc/platform/choc_Platform.h"

#if !CHOC_APPLE && !CHOC_WINDOWS && !CHOC_LINUX
#	include "./not-supported.h"
#else
#	include "choc/gui/choc_WebView.h"
#	include "choc/memory/choc_Base64.h"

#	include <unordered_map>
#	include <fstream>
#	include <memory>
#	include <iostream>
#	define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;

#	if CHOC_APPLE
#		include <CoreFoundation/CFBundle.h>
#	elif CHOC_WINDOWS
#		define WIN32_LEAN_AND_MEAN
#		include <windows.h>
#	endif

namespace webview_gui {

struct WebviewGui::Impl {
	~Impl() {
		if (webview) {
#	if CHOC_APPLE
			using namespace choc::objc;
			id subview = (id)webview->getViewHandle();
			call<void>(subview, "removeFromSuperview");
#	elif CHOC_WINDOWS
			HWND handle = static_cast<HWND>(webview->getViewHandle());
			::ShowWindow(handle, SW_SHOW);
			::SetParent(handle, NULL);
#	endif
		}
	}
	
	void init(const choc::ui::WebView::Options &options) {
		webview = std::unique_ptr<choc::ui::WebView>{
			new choc::ui::WebView(options)
		};
	}
	
	void attach(void *parentView) {
		if (!webview) return;
#	if CHOC_APPLE
		using namespace choc::objc;
		id parent = static_cast<id>(parentView);
		id subview = static_cast<id>(webview->getViewHandle());
		call<void>(parent, "addSubview:", subview);
#	elif CHOC_WINDOWS
		HWND handle = static_cast<HWND>(webview->getViewHandle());
		::SetParent(handle, static_cast<HWND>(parentView));
		::ShowWindow(handle, SW_SHOW);
		::UpdateWindow(static_cast<HWND>(parentView));
#	endif
	}
	void setSize(double width, double height) {
		if (!webview) return;
#	if CHOC_APPLE
		using namespace choc::objc;
		struct CGRect rect = {0, 0, CGFloat(width), CGFloat(height)};
		id subview = (id)webview->getViewHandle();
		call<void>(subview, "setFrame:", rect);
#	elif CHOC_WINDOWS
		HWND handle = static_cast<HWND>(webview->getViewHandle());
		::MoveWindow(handle, 0, 0, int(std::round(width)), int(std::round(height)), TRUE);
#	endif
	}

	WebviewGui *main = nullptr;
	std::unique_ptr<choc::ui::WebView> webview;
};

WebviewGui * WebviewGui::create(WebviewGui::Platform p, const std::string &startPath, WebviewGui::ResourceGetter getter) {
	if (!supports(p)) return nullptr;

	auto *impl = new WebviewGui::Impl();
	
	choc::ui::WebView::Options options;
	options.acceptsFirstMouseClick = true;
	options.transparentBackground = true;
#	if CHOC_WINDOWS
	// Copied from CHOC - not sure why, maybe ensuring a secure context?
	options.customSchemeURI = "https://choc.localhost";
#	else
	options.customSchemeURI = "choc://choc.choc/";
#	endif
	auto startUri = options.customSchemeURI + startPath;
   	options.fetchResource = [getter](const std::string &path) {
		using ChocResource = choc::ui::WebView::Options::Resource;
		std::optional<ChocResource> chocResource;
		Resource resource;
		const char *cPath = path.c_str();
		if (cPath[0] == '/' && cPath[1] == '/') ++cPath;
		if (getter(cPath, resource)) {
			chocResource.emplace();
			chocResource->data = std::move(resource.bytes);
			if (resource.mediaType.size()) {
				chocResource->mimeType = std::move(resource.mediaType);
			} else {
				chocResource->mimeType = helpers::guessMediaType(path.c_str());
			}
		}
		return chocResource;
	};
	options.webviewIsReady = [startUri, impl](choc::ui::WebView &wv){
		wv.addInitScript(R"jsCode(
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
				if (e.source == window) {
					e.stopImmediatePropagation();
					_WebviewGui_receive64(new Uint8Array(e.data).toBase64());
				}
			}, {capture: true});
			function _WebviewGui_send64(b64){
				window.dispatchEvent(new MessageEvent('message', {data: Uint8Array.fromBase64(b64).buffer}));
			}
		)jsCode");

		wv.bind("_WebviewGui_receive64", [impl](const choc::value::ValueView& args){
			auto *gui = impl->main;
			if (gui && gui->receive && args.isArray() && args.size() == 1) {
				auto base64 = args[0].getString();
				std::vector<unsigned char> bytes;
				choc::base64::decodeToContainer(bytes, base64);
				gui->receive(bytes.data(), bytes.size());
			}
			return choc::value::Value{true};
		});

		wv.navigate(startUri);
	};

	impl->init(options);
	if (!impl->webview->loadedOK()) {
		delete impl;
		return nullptr;
	}

	return new WebviewGui(impl);
}

WebviewGui * WebviewGui::create(WebviewGui::Platform p, const std::string &startUrl) {
	return create(p, startUrl, [](const char *path, Resource &resource){
		// No custom resources - the start URL needs to be absolute
		return false;
	});
}

WebviewGui * WebviewGui::create(WebviewGui::Platform p, const std::string &startPath, const std::string &baseDir) {
	return create(p, startPath, [baseDir](const char *path, Resource &resource){
		// Read resources from disk
		auto fullPath = baseDir + path;
#	if CHOC_WINDOWS
		for (size_t i = baseDir.size(); i < fullPath.size(); ++i) {
			if (fullPath[i] == '/') fullPath[i] = '\\';
		}
#	endif
		std::ifstream fileStream{fullPath, std::ios::binary | std::ios::ate};
		if (!fileStream) return false;
		size_t length = fileStream.tellg();
		resource.bytes.resize(length);
		fileStream.seekg(0);
		fileStream.read((char *)resource.bytes.data(), length);
		return bool(fileStream);
	});
}

WebviewGui::WebviewGui(WebviewGui::Impl *impl) : impl(impl) {
	impl->main = this;
}
WebviewGui::~WebviewGui() {
	delete impl;
}

bool WebviewGui::supports(Platform p) {
	return (p != Platform::NONE);
}
void WebviewGui::attach(void *platformNative) {
	impl->attach(platformNative);
}
void WebviewGui::send(const unsigned char *bytes, size_t length) {
	auto base64 = choc::base64::encodeToString(bytes, length);
	impl->webview->evaluateJavascript("_WebviewGui_send64(\"" + base64 + "\");");
}
void WebviewGui::setSize(double width, double height) {
	impl->setSize(width, height);
}
void WebviewGui::setVisible(bool visible) {}

//-------------

} // namespace

#endif
