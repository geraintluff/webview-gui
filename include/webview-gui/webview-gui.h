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
	
	struct Resource {
		std::string mediaType;
		std::vector<unsigned char> bytes;
		
		void set(const char *str, size_t length, const std::string &type="text/html;charset=UTF-8") {
			bytes.assign((const unsigned char *)str, (const unsigned char *)str + std::strlen(str));
			mediaType = type;
		}
		void set(const std::string &str, const std::string &type="text/html;charset=UTF-8") {
			set(str.c_str(), str.size(), type);
		}
	};
	using ResourceGetter = std::function<bool(const char *path, Resource &resource)>;
	
	static bool supports(Platform p);
	static WebviewGui * create(Platform platform, const std::string &startPath, ResourceGetter getter);
	static WebviewGui * create(Platform platform, const std::string &startPath, const std::string &baseDir);
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
