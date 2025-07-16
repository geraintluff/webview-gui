#pragma once

#include <functional>

struct WebviewGui {
	enum class Platform {
		NONE, HWND, COCOA, X11
	};
	static constexpr Platform NONE = Platform::NONE;
	static constexpr Platform HWND = Platform::HWND;
	static constexpr Platform COCOA = Platform::COCOA;
	static constexpr Platform X11 = Platform::X11;
	
	static bool supports(Platform p);
	static WebviewGui * create(Platform platform, const char *startPath, size_t width, size_t height);
	WebviewGui(const WebviewGui &other) = delete;
	~WebviewGui();
	
	void attach(void *platformNative);

	// Assign this to receive messages
	std::function<void(const unsigned char *, size_t)> receive;
	void send(const unsigned char *, size_t);
	
	void setSize(double width, double height);
	void setVisible(bool visible);
private:
	struct Impl;
	Impl *impl;
	WebviewGui(Impl *);
};
