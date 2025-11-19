#pragma once

#include "clap/clap.h"
#include "webview-gui.h"

#include <memory>
#include <string>
#include <cctype>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace webview_gui {

// Copy the definitions here, in case you're stuck on an older CLAP version
static constexpr const char CLAP_EXT_WEBVIEW[] = "clap.webview/3";
static constexpr const char CLAP_WINDOW_API_WEBVIEW[] = "webview";
struct clap_plugin_webview {
	int32_t (CLAP_ABI *get_uri)(const clap_plugin_t *plugin, char *uri, uint32_t uri_capacity);
	bool (CLAP_ABI *get_resource)(const clap_plugin_t *plugin, const char *path, char *mime, uint32_t mime_capacity, const clap_ostream_t *stream);
	bool (CLAP_ABI *receive)(const clap_plugin_t *plugin, const void *buffer, uint32_t size);
};
struct clap_host_webview {
	bool(CLAP_ABI *send)(const clap_host_t *host, const void *buffer, uint32_t size);
};

struct ClapWebviewGui {
	uint32_t width = 400, height = 250;

	// Default plugin GUI extension - replace any methods you like (e.g. for sizing)
	clap_plugin_gui *extPluginGui;

	// This SHOULD be used in preference to the actual host one, to cover the case where the host supports both extensions but has chosen to ask for a native GUI
	// If no native webview is active, then this forwards to the actual host extension anyway
	const clap_host_webview *extHostWebview;

	ClapWebviewGui(const clap_plugin *plugin=nullptr, const clap_host *host=nullptr, std::string resourcePath="") : plugin(plugin), host(host), resourcePath(std::move(resourcePath)) {
		setSelf(plugin);
		setSelf(host);
		extPluginGui = &pluginGuiProxy;
		extHostWebview = &hostWebviewProxy;
	}
	
	// Call from `plugin.init()`
	void init(const clap_plugin *initPlugin, const clap_host *initHost, const std::string &initResourcePath="") {
		clearSelf(plugin);
		clearSelf(host);
		plugin = initPlugin;
		host = initHost;
		setSelf(plugin);
		setSelf(host);
		if (initResourcePath.size()) resourcePath = initResourcePath;

		init();
	}
	void init() {
		pluginWebview = (const clap_plugin_webview *)plugin->get_extension(plugin, CLAP_EXT_WEBVIEW);
		hostWebview = (const clap_host_webview *)host->get_extension(host, CLAP_EXT_WEBVIEW);
	}

	/* ---- Plugin GUI methods ----
	The simplest way to use this helper is the two pointers above.  These are the methods which the plugin GUI extension calls by default.
	
	You shouldn't need to call these unless you're overriding them.
	*/

	// Static methods for our proxies
	bool isApiSupported(const char *api, bool is_floating) {
		if (!std::strcmp(api, CLAP_WINDOW_API_WEBVIEW)) return true;
		if (is_floating) return false;
		return WebviewGui::supports(clapApiToPlatform(api));
	}
	
	bool getPreferredApi(const char **api, bool *is_floating) {
		*api = CLAP_WINDOW_API_WEBVIEW;
		*is_floating = false;
		return true;
	}
	
	bool create(const char *api, bool is_floating) {
		if (!std::strcmp(api, CLAP_WINDOW_API_WEBVIEW)) {
			return true;
		}

		if (is_floating) return false;
		
		std::string startUrl = getNativeStartUrl();
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
			ptr = WebviewGui::create(platform, startUrl.c_str(), [this](const char *path, WebviewGui::Resource &resource){
				if (!pluginWebview) return false;
				
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
				bool success = pluginWebview->get_resource(plugin, path, mediaType, 255, &resourceStream);
				if (success) resource.mediaType = mediaType;
				return success;
			});
		}
		if (!ptr) return false;

		nativeWebview = std::unique_ptr<WebviewGui>{ptr};
		nativeWebview->receive = [this](const unsigned char *bytes, size_t length){
			if (pluginWebview) {
				pluginWebview->receive(plugin, (const void *)bytes, uint32_t(length));
			}
		};
		return true;
	}

	void destroy() {
		nativeWebview = nullptr;
	}
	
	bool setScale(double scale) {
		return true;
	}
	
	bool getSize(uint32_t *w, uint32_t *h) {
		*w = width;
		*h = height;
		return true;
	}
	
	bool canResize() {
		return true;
	}
	
	bool getResizeHints(clap_gui_resize_hints_t *hints) {
		*hints = {true, true, false, 0, 0};
		return true;
	}
	
	bool adjustSize(uint32_t *w, uint32_t *h) {
		return true;
	}

	bool setSize(uint32_t w, uint32_t h) {
		width = w;
		height = h;
		if (nativeWebview) nativeWebview->setSize(w, h);
		return true;
	}
	
	bool setParent(const clap_window *window) {
		if (nativeWebview) {
			nativeWebview->attach(window->ptr);
			return true;
		}
		return false;
	}
	
	bool setTransient(const clap_window *window) {
		// TODO: this
		return false;
	}
	
	void suggestTitle(const char *title) {}
	
	bool show() {
		if (nativeWebview) {
			nativeWebview->setVisible(true);
			return true;
		}
		return false;
	}
	
	bool hide() {
		if (nativeWebview) {
			nativeWebview->setVisible(false);
			return true;
		}
		return false;
	}

	/* ---- Host Webview methods ----
	Th
	
	If that seems weird, use this instead
	*/

	// This is a host method
	bool send(const void *buffer, size_t length) {
		if (nativeWebview) {
			// If we're currently providing a native GUI, send to that (even if the host also supports the extension)
			nativeWebview->send((const unsigned char *)buffer, length);
			return true;
		} else if (hostWebview) {
			return hostWebview->send(host, buffer, length);
		}
		return false;
	}
	
private:
	const clap_plugin *plugin = nullptr;
	const clap_host *host = nullptr;
	std::string resourcePath;

	std::unique_ptr<WebviewGui> nativeWebview;

	// Map used to create proxy plugin/host extensions, even though they're called with `plugin`/`host` arguments
	// C++17 inline variables are really useful for this
	inline static std::unordered_map<size_t, ClapWebviewGui*> pointerMap;
	inline static std::shared_mutex pointerMapMutex;

	static ClapWebviewGui & getSelf(const void *pluginOrHost) {
		std::shared_lock guard{pointerMapMutex};
		return *pointerMap[(size_t)pluginOrHost];
	}
	void setSelf(const void *pluginOrHost) {
		if (!pluginOrHost) return;
		std::unique_lock guard{pointerMapMutex};
		pointerMap.insert_or_assign((size_t)pluginOrHost, this);
	}
	void clearSelf(const void *pluginOrHost) {
		if (!pluginOrHost) return;
		std::unique_lock guard{pointerMapMutex};
		pointerMap.erase((size_t)pluginOrHost);
	}
	
	char startUrlBuffer[2048] = {0};
	const char * getNativeStartUrl() {
		if (pluginWebview) {
			auto uriLength = pluginWebview->get_uri(plugin, startUrlBuffer, 2047);
			if (uriLength >= 2048) {
				std::strcpy(startUrlBuffer, "data:text/html,URI%20too%20long");
			} else if (uriLength <= 0) {
				std::strcpy(startUrlBuffer, "data:text/html,get_uri%20error");
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

	const clap_plugin_webview *pluginWebview;
	const clap_host_webview *hostWebview;

	// Our proxies
	clap_host_webview hostWebviewProxy{
		.send=host_webview_send
	};
	clap_plugin_gui pluginGuiProxy{
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
	// Static methods for our proxies
	static bool gui_is_api_supported(const clap_plugin *plugin, const char *api, bool is_floating) {
		return getSelf(plugin).isApiSupported(api, is_floating);
	}
	static bool gui_get_preferred_api(const clap_plugin *plugin, const char **api, bool *is_floating) {
		return getSelf(plugin).getPreferredApi(api, is_floating);
	}
	static bool gui_create(const clap_plugin *plugin, const char *api, bool is_floating) {
		return getSelf(plugin).create(api, is_floating);
	}
	static void gui_destroy(const clap_plugin *plugin) {
		return getSelf(plugin).destroy();
	}
	static bool gui_set_scale(const clap_plugin *plugin, double scale) {
		return getSelf(plugin).setScale(scale);
	}
	static bool gui_get_size(const clap_plugin *plugin, uint32_t *w, uint32_t *h) {
		return getSelf(plugin).getSize(w, h);
	}
	static bool gui_can_resize(const clap_plugin *plugin) {
		return getSelf(plugin).canResize();
	}
	static bool gui_get_resize_hints(const clap_plugin *plugin, clap_gui_resize_hints_t *hints) {
		return getSelf(plugin).getResizeHints(hints);
	}
	static bool gui_adjust_size(const clap_plugin *plugin, uint32_t *w, uint32_t *h) {
		return getSelf(plugin).adjustSize(w, h);
	}
	static bool gui_set_size(const clap_plugin *plugin, uint32_t w, uint32_t h) {
		return getSelf(plugin).setSize(w, h);
	}
	static bool gui_set_parent(const clap_plugin *plugin, const clap_window *window) {
		return getSelf(plugin).setParent(window);
	}
	static bool gui_set_transient(const clap_plugin *plugin, const clap_window *window) {
		return getSelf(plugin).setTransient(window);
	}
	static void gui_suggest_title(const clap_plugin *plugin, const char *title) {
		return getSelf(plugin).suggestTitle(title);
	}
	static bool gui_show(const clap_plugin *plugin) {
		return getSelf(plugin).show();
	}
	static bool gui_hide(const clap_plugin *plugin) {
		return getSelf(plugin).hide();
	}
	static bool host_webview_send(const clap_host_t *host, const void *buffer, uint32_t size) {
		return getSelf(host).send(buffer, size);
	}
};

} // namespace
