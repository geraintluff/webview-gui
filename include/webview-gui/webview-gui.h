#pragma once

#include <functional>
#include <vector>
#include <string>

namespace webview_gui {

#ifdef WEBVIEW_GUI_HEADER_ONLY
#	define WEBVIEW_GUI_IMPL inline
#else
#	define WEBVIEW_GUI_IMPL
#endif

struct WebviewGui {
	enum class Platform {
		NONE, HWND, COCOA, X11
	};
	static constexpr Platform NONE = Platform::NONE;
	static constexpr Platform HWND = Platform::HWND;
	static constexpr Platform COCOA = Platform::COCOA;
	static constexpr Platform X11 = Platform::X11;
	
	struct Resource {
		std::string mediaType;
		std::vector<unsigned char> bytes;
	};
	using ResourceGetter = std::function<bool(const char *path, Resource &resource)>;
	
	WEBVIEW_GUI_IMPL static bool supports(Platform p);
	WEBVIEW_GUI_IMPL static WebviewGui * create(Platform platform, const std::string &startPath, ResourceGetter getter);
	WEBVIEW_GUI_IMPL static WebviewGui * create(Platform platform, const std::string &startPath, const std::string &baseDir);
	WebviewGui(const WebviewGui &other) = delete;
	WEBVIEW_GUI_IMPL ~WebviewGui();
	
	WEBVIEW_GUI_IMPL void attach(void *platformNative);

	// Assign this to receive messages
	std::function<void(const unsigned char *, size_t)> receive;
	WEBVIEW_GUI_IMPL void send(const unsigned char *, size_t);
	
	WEBVIEW_GUI_IMPL void setSize(double width, double height);
	WEBVIEW_GUI_IMPL void setVisible(bool visible);
private:
	struct Impl;
	Impl *impl;
	WEBVIEW_GUI_IMPL WebviewGui(Impl *);
};

#undef WEBVIEW_GUI_IMPL

} // namespace

using WebviewGui = ::webview_gui::WebviewGui;

#ifdef WEBVIEW_GUI_HEADER_ONLY
#	include "./_impl/webview-gui.hxx"
#endif
