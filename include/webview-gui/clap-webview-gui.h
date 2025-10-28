#pragma once

#include "clap/clap.h"
#include "webview-gui.h"

#include <memory>
#include <string>
#include <cctype>
#include <algorithm>

namespace webview_gui {

static constexpr const char CLAP_EXT_WEBVIEW[] = "clap.webview/3";
struct clap_plugin_webview {
	int32_t (CLAP_ABI *get_uri)(const clap_plugin_t *plugin, char *uri, uint32_t uri_capacity);
	bool (CLAP_ABI *get_resource)(const clap_plugin_t *plugin, const char *path, char *mime, uint32_t mime_capacity, const clap_ostream_t *stream);
	bool (CLAP_ABI *receive)(const clap_plugin_t *plugin, const void *buffer, uint32_t size);
};

template<void *(pluginToThis)(const clap_plugin *)>
struct ClapWebviewGui {
	uint32_t width = 350, height = 200;
	uint32_t minWidth = 200, minHeight = 200;
	clap_gui_resize_hints resizeHints{true, true, false, 0, 0}; // Horizontal and vertical, no fixed aspect ratio

	ClapWebviewGui(const clap_plugin *plugin=nullptr, const clap_host *host=nullptr, std::string resourcePath="") : plugin(plugin), host(host), resourcePath(std::move(resourcePath)) {}
	
	// Call from `plugin.init()`
	void init(const clap_plugin *initPlugin, const clap_host *initHost, const std::string &initResourcePath="") {
		plugin = initPlugin;
		host = initHost;
		if (initResourcePath.size()) resourcePath = initResourcePath;

		init();
	}
	void init() {
		hostGui = (const clap_host_gui *)host->get_extension(host, CLAP_EXT_GUI);

		pluginWebview1 = (const clap_plugin_webview1 *)plugin->get_extension(plugin, CLAP_EXT_WEBVIEW1);
		hostWebview1 = (const clap_host_webview1 *)host->get_extension(host, CLAP_EXT_WEBVIEW1);
		pluginWebview2 = (const clap_plugin_webview2 *)plugin->get_extension(plugin, CLAP_EXT_WEBVIEW2);
		hostWebview2 = (const clap_host_webview2 *)host->get_extension(host, CLAP_EXT_WEBVIEW2);
		pluginWebview3 = (const clap_plugin_webview3 *)plugin->get_extension(plugin, CLAP_EXT_WEBVIEW3);
		hostWebview3 = (const clap_host_webview3 *)host->get_extension(host, CLAP_EXT_WEBVIEW3);
		
		initComplete = true;
	}

#ifdef WIN32
	static constexpr char pathSep = '\\';
#else
	static constexpr char pathSep = '/';
#endif

	// Call from `plugin.get_extension()`
	const void * getExtension(const char *extId) {
		if (!std::strcmp(extId, CLAP_EXT_GUI)) {
			static clap_plugin_gui ext{
				gui_is_api_supported,
				gui_get_preferred_api,
				gui_create,
				gui_destroy,
				gui_set_scale,
				gui_get_size,
				gui_can_resize,
				gui_get_resize_hints,
				gui_adjust_size,
				gui_set_size,
				gui_set_parent,
				gui_set_transient,
				gui_suggest_title,
				gui_show,
				gui_hide
			};
			return &ext;
		}
		if (!initComplete) return nullptr; // no proxies until we've finished `.init()`
		if (!std::strcmp(extId, CLAP_EXT_WEBVIEW1)) {
			if (pluginWebview3) {
				static const clap_plugin_webview1 ext{
					webview3to1_provide_starting_uri,
					pluginWebview2->receive /* compatible */
				};
				auto *startUri = getNativeStartUrl();
				if (!isAbsolute(startUri)) return nullptr; // relative URIs in v3 rely on `.get_resource()`
				return &ext;
			} else if (pluginWebview2) {
				static const clap_plugin_webview1 ext{
					webview2to1_provide_starting_uri,
					pluginWebview2->receive /* compatible */
				};
				return &ext;
			}
		} else if (!std::strcmp(extId, CLAP_EXT_WEBVIEW2)) {
			if (pluginWebview3) {
				static const clap_plugin_webview2 ext{
					webview3to2_get_uri,
					pluginWebview3->receive /* compatible */
				};
				auto *startUri = getNativeStartUrl();
				if (!isAbsolute(startUri)) return nullptr; // relative URIs in v3 rely on `.get_resource()`
				return &ext;
			} else if (pluginWebview1) {
				static const clap_plugin_webview2 ext{
					webview1to2_get_uri,
					pluginWebview1->receive /* compatible */
				};
				return &ext;
			}
		} else if (!std::strcmp(extId, CLAP_EXT_WEBVIEW3)) {
			if (pluginWebview2) {
				static const clap_plugin_webview3 ext{
					webview1to3_get_uri,
					webview3_ignore_get_resource,
					pluginWebview1->receive /* compatible */
				};
				return &ext;
			} else if (pluginWebview1) {
				static const clap_plugin_webview3 ext{
					webview1to3_get_uri,
					webview3_ignore_get_resource,
					pluginWebview1->receive /* compatible */
				};
				return &ext;
			}
		}
		return nullptr;
	}
	
	bool isOpen() const {
		if (guiActive) return guiVisible; // if the GUI was created (native or webview) then we can use show/hide
		return true; // we don't know, assume true
	}
	
	bool send(const unsigned char *bytes, size_t length) {
		if (nativeWebview) {
			nativeWebview->send(bytes, length);
			return true;
		} else if (hostWebview1) {
			return hostWebview1->send(host, bytes, (uint32_t)length);
		} else if (hostWebview2) {
			return hostWebview2->send(host, bytes, (uint32_t)length);
		}
		return false;
	}
	
private:
	bool initComplete = false;
	bool guiActive = false, guiVisible = false;
	// if it's active but has no native implentation, then we're using the CLAP webview stuff directly
	std::unique_ptr<WebviewGui> nativeWebview;

	// Include all versions of the webview structs here, for maximum compatibility
	static constexpr const char *CLAP_EXT_WEBVIEW1 = "clap.webview/1";
	static constexpr const char *CLAP_WINDOW_API_WEBVIEW = "webview";
	struct clap_plugin_webview1 {
		bool(CLAP_ABI *provide_starting_uri)(const clap_plugin_t *plugin, char *out_buffer, uint32_t out_buffer_capacity);
		bool(CLAP_ABI *receive)(const clap_plugin_t *plugin, const void *buffer, uint32_t size);
	};
	struct clap_host_webview1 {
		bool(CLAP_ABI *is_open)(const clap_host_t *host);
		bool(CLAP_ABI *send)(const clap_host_t *host, const void *buffer, uint32_t size);
	};
	static constexpr const char *CLAP_EXT_WEBVIEW2 = "clap.webview/2";
	struct clap_plugin_webview2 {
		int32_t(CLAP_ABI *get_uri)(const clap_plugin_t *plugin, char *uri, uint32_t uri_capacity);
		bool(CLAP_ABI *receive)(const clap_plugin_t *plugin, const void *buffer, uint32_t size);
	};
	struct clap_host_webview2 {
		bool(CLAP_ABI *send)(const clap_host_t *host, const void *buffer, uint32_t size);
	};

	static constexpr const char *CLAP_EXT_WEBVIEW3 = "clap.webview/3";
	struct clap_plugin_webview3 {
		int32_t(CLAP_ABI *get_uri)(const clap_plugin_t *plugin, char *uri, uint32_t uri_capacity);
		bool(CLAP_ABI *get_resource)(const clap_plugin_t *plugin, const char *path, char *mime, uint32_t mime_capacity, const clap_ostream_t *data_stream);
		bool(CLAP_ABI *receive)(const clap_plugin_t *plugin, const void *buffer, uint32_t size);
	};
	using clap_host_webview3 = clap_host_webview2;

	const clap_plugin *plugin = nullptr;
	const clap_host *host = nullptr;
	std::string resourcePath;

	const clap_host_gui *hostGui = nullptr;
	const clap_plugin_webview1 *pluginWebview1 = nullptr;
	const clap_host_webview1 *hostWebview1 = nullptr;
	const clap_plugin_webview2 *pluginWebview2 = nullptr;
	const clap_host_webview2 *hostWebview2 = nullptr;
	const clap_plugin_webview3 *pluginWebview3 = nullptr;
	const clap_host_webview3 *hostWebview3 = nullptr; // for clarity, even though it's the same as v2

	static ClapWebviewGui & getSelf(const clap_plugin *plugin) {
		return *(ClapWebviewGui *)pluginToThis(plugin);
	}
	
	char startUrlBuffer[2048] = {0};
	const char * getNativeStartUrl() {
		if (pluginWebview3) {
			auto uriLength = pluginWebview3->get_uri(plugin, startUrlBuffer, 2047);
			if (uriLength > 2048) {
				std::strcpy(startUrlBuffer, "data:text/html,URI%20too%20long");
			} else if (uriLength <= 0) {
				std::strcpy(startUrlBuffer, "data:text/html,get_uri%20error");
			}
		} else if (pluginWebview2) {
			auto uriLength = pluginWebview2->get_uri(plugin, startUrlBuffer, 2047);
			if (uriLength > 2048) {
				std::strcpy(startUrlBuffer, "data:text/html,URI%20too%20long");
			} else if (uriLength <= 0) {
				std::strcpy(startUrlBuffer, "data:text/html,get_uri%20error");
			}
		} else if (pluginWebview1) {
			if (!pluginWebview1->provide_starting_uri(plugin, startUrlBuffer, 2048)) {
				std::strcpy(startUrlBuffer, "data:text/html,provide_starting_uri%20false");
			}
		} else {
			std::strcpy(startUrlBuffer, "data:text/html,no%20plugin%20webview%20ext");
		}
		return startUrlBuffer;
	}
	
	static bool isAbsolute(const char *uri) {
		if (*uri == ':') return false; // absolute URIs can't start with `:`
		while (*uri) {
			auto c = *(uri++);
			if (c == ':') return true; // a valid scheme followed by `:`
			if (c >= 'A' && c <= 'Z') continue;
			if (c >= 'a' && c <= 'z') continue;
			if (c >= '0' && c <= '9') continue;
			if (c == '+' || c == '.' || c == '-') continue;
			return false; // not a valid scheme
		}
		return false; // reached the end without any non-scheme characters, but no `:`
	}
	
	static WebviewGui::Platform clapApiToPlatform(const char *api) {
		auto platform = WebviewGui::NONE;
		if (!std::strcmp(api, CLAP_WINDOW_API_WIN32)) platform = WebviewGui::HWND;
		if (!std::strcmp(api, CLAP_WINDOW_API_COCOA)) platform = WebviewGui::COCOA;
		if (!std::strcmp(api, CLAP_WINDOW_API_X11)) platform = WebviewGui::X11EMBED;
		return platform;
	}

	static bool gui_is_api_supported(const clap_plugin *plugin, const char *api, bool is_floating) {
		if (is_floating) return false;
		if (!std::strcmp(api, CLAP_WINDOW_API_WEBVIEW)) return true;
		return WebviewGui::supports(clapApiToPlatform(api));
	}
	
	static bool gui_get_preferred_api(const clap_plugin *plugin, const char **api, bool *is_floating) {
		*api = CLAP_WINDOW_API_WEBVIEW;
		*is_floating = false;
		return true;
	}
	
	static bool gui_create(const clap_plugin *plugin, const char *api, bool is_floating) {
		auto &self = getSelf(plugin);

		if (is_floating) return false;
		if (!std::strcmp(api, CLAP_WINDOW_API_WEBVIEW)) {
			self.guiActive = true;
			return true;
		}
		
		std::string startUrl = self.getNativeStartUrl();
		if (!isAbsolute(startUrl.c_str()) && startUrl[0] != '/') {
			// relative URLs assumed to be absolute paths
			startUrl = "/" + startUrl;
		}

		auto platform = clapApiToPlatform(api);
		WebviewGui *ptr;

		if (startUrl.substr(0, 5) == "file:") { // absolute file path
			// strip `file:` and all leading `/`s
			size_t pos = 5;
			while (startUrl[pos] == '/') ++pos;
			startUrl = startUrl.substr(pos);
			
			std::string baseDir = startUrl;
			// drop the file
			while (!baseDir.empty() && baseDir.back() != '/') baseDir.pop_back();
			// and the final `/`
			if (!baseDir.empty()) baseDir.pop_back();
			startUrl = startUrl.substr(baseDir.size());
#if defined (_WIN32) || defined (_WIN64)
			for (auto &c : baseDir) {
				if (c == '/') c = '\\';
			}
#else
			// We stripped the `/` above, add it back in
			if (baseDir[0] != '/') baseDir = "/" + baseDir;
#endif
			ptr = WebviewGui::create(platform, startUrl.c_str(), baseDir);
		} else {
			ptr = WebviewGui::create(platform, startUrl.c_str(), [plugin](const char *path, WebviewGui::Resource &resource){
				auto *pluginWebview3 = getSelf(plugin).pluginWebview3;
				if (!pluginWebview3) return false;
				
				char mediaType[256] = {0};
				struct ResourceStream : public clap_ostream {
					WebviewGui::Resource &resource;
					
					ResourceStream(WebviewGui::Resource &resource) : resource(resource) {
						*(clap_ostream *)this = {
							.ctx=this,
							.write=write
						};
					}
					static int64_t write(const clap_ostream *stream, const void *buffer, uint64_t length) {
						auto *byteBuffer = (const unsigned char *)buffer;
						auto &self = *(ResourceStream *)stream;
						self.resource.bytes.insert(self.resource.bytes.end(), byteBuffer, byteBuffer + length);
						return int64_t(length);
					};
				} resourceStream{resource};
				bool success = pluginWebview3->get_resource(plugin, path, mediaType, 255, &resourceStream);
				if (success) resource.mediaType = mediaType;
				return success;
			});
		}
		
		if (ptr) {
			self.nativeWebview = std::unique_ptr<WebviewGui>{ptr};
			self.nativeWebview->receive = [plugin](const unsigned char *bytes, size_t length){
				auto &self = getSelf(plugin);
				if (self.pluginWebview3) {
					self.pluginWebview3->receive(self.plugin, (const void *)bytes, uint32_t(length));
				} else if (self.pluginWebview2) {
					self.pluginWebview2->receive(self.plugin, (const void *)bytes, uint32_t(length));
				} else if (self.pluginWebview1) {
					self.pluginWebview1->receive(self.plugin, (const void *)bytes, uint32_t(length));
				}
			};
			self.guiActive = true;
			return true;
		}
		return false;
	}

	static void gui_destroy(const clap_plugin *plugin) {
		auto &self = getSelf(plugin);
		self.nativeWebview = nullptr;
		self.guiActive = false;
		self.guiVisible = false;
	}
	
	static bool gui_set_scale(const clap_plugin *plugin, double scale) {
		return true;
	}
	
	static bool gui_get_size(const clap_plugin *plugin, uint32_t *w, uint32_t *h) {
		auto &self = getSelf(plugin);
		*w = self.width;
		*h = self.height;
		return true;
	}
	
	static bool gui_can_resize(const clap_plugin *plugin) {
		auto &self = getSelf(plugin);
		return self.resizeHints.can_resize_horizontally || self.resizeHints.can_resize_vertically;
	}
	
	static bool gui_get_resize_hints(const clap_plugin *plugin, clap_gui_resize_hints_t *hints) {
		auto &self = getSelf(plugin);
		*hints = self.resizeHints;
		return true;
	}
	
	static bool gui_adjust_size(const clap_plugin *plugin, uint32_t *w, uint32_t *h) {
		auto &self = getSelf(plugin);
		if (*w < self.minWidth) *w = self.minWidth;
		if (*h < self.minHeight) *h = self.minHeight;
		// Adjust based on the resizing hints
		auto &resizeHints = self.resizeHints;
		if (resizeHints.preserve_aspect_ratio) {
			auto area = double(*w)*(*h);
			auto refArea = double(resizeHints.aspect_ratio_width)*resizeHints.aspect_ratio_height;
			double scale = std::sqrt(area/refArea);
			*w = std::round(scale*resizeHints.aspect_ratio_width);
			*h = std::round(scale*resizeHints.aspect_ratio_height);
		}
		if (!resizeHints.can_resize_horizontally) *w = self.width;
		if (!resizeHints.can_resize_vertically) *h = self.height;
		return true;
	}
	
	static bool gui_set_size(const clap_plugin *plugin, uint32_t w, uint32_t h) {
		auto &self = getSelf(plugin);
		self.width = w;
		self.height = h;
		if (self.nativeWebview) self.nativeWebview->setSize(w, h);
		return (w >= self.minWidth && h >= self.minHeight);
	}
	
	static bool gui_set_parent(const clap_plugin *plugin, const clap_window *window) {
		auto &self = getSelf(plugin);
		if (self.nativeWebview) {
			self.nativeWebview->attach(window->ptr);
			return true;
		}
		return false;
	}
	
	static bool gui_set_transient(const clap_plugin *plugin, const clap_window *window) {
		// TODO: this
		return false;
	}
	
	static void gui_suggest_title(const clap_plugin *plugin, const char *title) {}
	
	static bool gui_show(const clap_plugin *plugin) {
		auto &self = getSelf(plugin);
		self.guiVisible = true;
		if (self.nativeWebview) self.nativeWebview->setVisible(true);
		return true;
	}
	
	static bool gui_hide(const clap_plugin *plugin) {
		auto &self = getSelf(plugin);
		self.guiVisible = false;
		if (self.nativeWebview) self.nativeWebview->setVisible(false);
		return true;
	}

	static bool webview2to1_provide_starting_uri(const clap_plugin_t *plugin, char *startingUri, uint32_t capacity) {
		auto &self = getSelf(plugin);
		auto uriLength = self.pluginWebview2->get_uri(plugin, startingUri, capacity);
		return (uriLength > 0) && (uriLength <= capacity);
	}

	static bool webview3to1_provide_starting_uri(const clap_plugin_t *plugin, char *startingUri, uint32_t capacity) {
		auto &self = getSelf(plugin);
		auto uriLength = self.pluginWebview3->get_uri(plugin, startingUri, capacity);
		return (uriLength > 0) && (uriLength <= capacity) && isAbsolute(startingUri);
	}

	static int32_t webview1to2_get_uri(const clap_plugin_t *plugin, char *startingUri, uint32_t capacity) {
		auto &self = getSelf(plugin);
		if (!self.pluginWebview1->provide_starting_uri(plugin, startingUri, capacity)) {
			return -1; // no way to report truncation
		}
		for (uint32_t i = 0; i < capacity; ++i) {
			if (startingUri[i] == 0) return i;
		}
		return capacity;
	}
	static int32_t webview3to2_get_uri(const clap_plugin_t *plugin, char *startingUri, uint32_t capacity) {
		auto &self = getSelf(plugin);
		auto uriLength = self.pluginWebview3->get_uri(plugin, startingUri, capacity);
		if (uriLength <= 0 || uriLength > capacity) return uriLength; // failure
		if (!isAbsolute(startingUri)) return -1; // can't translate relative URIs, since they need `.get_resource()`
		return uriLength;
	}

	static int32_t webview1to3_get_uri(const clap_plugin_t *plugin, char *startingUri, uint32_t capacity) {
		if (capacity < 7) return -1; // too short
		auto uriLength2 = webview1to2_get_uri(plugin, startingUri, capacity - 7);
		if (uriLength2 <= 0) return uriLength2; // failure
		std::rotate(startingUri, startingUri + (capacity - 7), startingUri + capacity);
		std::strncpy(startingUri, "file://", 7);
		return uriLength2 + 7;
	}
	
	static bool webview3_ignore_get_resource(const clap_plugin_t *, const char *, char *, uint32_t, const clap_ostream_t *) {
		return false;
	}
};

} // namespace
