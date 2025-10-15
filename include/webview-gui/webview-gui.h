#pragma once

#include <functional>
#include <vector>
#include <string>

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
	
	inline static bool supports(Platform p);
	inline static WebviewGui * create(Platform platform, const std::string &startPath, ResourceGetter getter);
	inline static WebviewGui * create(Platform platform, const std::string &startPath, const std::string &baseDir);
	WebviewGui(const WebviewGui &other) = delete;
	inline ~WebviewGui();
	
	inline void attach(void *platformNative);

	// Assign this to receive messages
	std::function<void(const unsigned char *, size_t)> receive;
	inline void send(const unsigned char *, size_t);
	
	inline void setSize(double width, double height);
	inline void setVisible(bool visible);
private:
	struct Impl;
	Impl *impl;
	inline WebviewGui(Impl *);
};

#if defined(__has_include) && __has_include("choc/gui/choc_WebView.h")
#	include "./_platform/choc.h"
#elif __APPLE__ && TARGET_OS_MAC
#	include "./_platform/apple-osx.h"
#else
#	include "./_platform/not-supported.h"
#endif
