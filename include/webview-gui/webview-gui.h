#pragma once

#include <functional>
#include <vector>
#include <string>
#include <memory>

namespace webview_gui {

#ifdef WEBVIEW_GUI_HEADER_ONLY
#	define WEBVIEW_GUI_IMPL inline
#else
#	define WEBVIEW_GUI_IMPL
#endif

struct WebviewGui {
	enum class Platform {
		NONE, HWND, COCOA, X11EMBED
	};
	static constexpr Platform NONE = Platform::NONE;
	static constexpr Platform HWND = Platform::HWND;
	static constexpr Platform COCOA = Platform::COCOA;
	static constexpr Platform X11EMBED = Platform::X11EMBED;
	
	struct Resource {
		std::string mediaType;
		std::vector<unsigned char> bytes;
	};
	using ResourceGetter = std::function<bool(const char *path, Resource &resource)>;
	
	WEBVIEW_GUI_IMPL static bool supports(Platform p);
	WEBVIEW_GUI_IMPL static WebviewGui * create(Platform platform, const std::string &startUrl);
	// The starting URL may be relative for these:
	WEBVIEW_GUI_IMPL static WebviewGui * create(Platform platform, const std::string &startUrl, const std::string &baseDir);
	WEBVIEW_GUI_IMPL static WebviewGui * create(Platform platform, const std::string &startUrl, ResourceGetter getter);
	WEBVIEW_GUI_IMPL ~WebviewGui();
	
	// Convenience template for creating shared/unique pointers
	using UniquePtr = std::unique_ptr<WebviewGui>;
	template<class... Args>
	static UniquePtr createUnique(Args &&...args) {
		return UniquePtr{create(std::forward<Args>(args)...)};
	}
	using SharedPtr = std::shared_ptr<WebviewGui>;
	template<class... Args>
	static SharedPtr createShared(Args &&...args) {
		return SharedPtr{create(std::forward<Args>(args)...)};
	}
	
	WEBVIEW_GUI_IMPL void attach(void *platformNative);

	// Assign this to receive messages
	std::function<void(const unsigned char *, size_t)> receive;
	WEBVIEW_GUI_IMPL void send(const unsigned char *, size_t);
	
	WEBVIEW_GUI_IMPL void setSize(double width, double height);
	WEBVIEW_GUI_IMPL void setVisible(bool visible);
private:
	struct Impl;
	Impl *impl;
	// Can only be created using the static methods
	WEBVIEW_GUI_IMPL WebviewGui(Impl *);
	WebviewGui(const WebviewGui &other) = delete;
};

#undef WEBVIEW_GUI_IMPL

} // namespace

using WebviewGui = ::webview_gui::WebviewGui;

#ifdef WEBVIEW_GUI_HEADER_ONLY
#	include "./_impl/webview-gui.hxx"
#endif
