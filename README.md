# Webview GUI

Webview GUIs in C++ with simple message-passing, intended for (audio) plugins.  The goals are:

* This library handles all the platform-specific stuff
	* many plugin formats give you a `void *` (actually a platform-native view), so this is what you pass on
	* provide fixes for common issues (e.g. keypress events)
* You pass opaque bytes back and forth
	* the C++ side sends/receives `const unsigned char *`s
	* the webpage sens/receives `ArrayBuffer`s (using `parent.postMessage()` and `addEventListener('message', ...)`)
* Serve files from a directory, or a custom callback
* CMake support, _or_ header-only C++ for simplicity

## How to use

See [`webview-gui.h`](include/webview-gui/webview-gui.h) for the API.  Here's an example using the VST3 platform identifiers:

```cpp
#include "webview-gui/webview-gui.h"

struct MyPlugin {
	using WebviewGui = webview_gui::WebviewGui; 
	std::shared_ptr<WebviewGui> webview;
	
	void createAndAttachView(const char *platformType, void *nativeParent) {
		auto platform = WebviewGui::NONE;
		// check VST3 platform identifiers
		if (!std::strcmp(platformType, "NSView")) {
			platform = WebviewGui::COCOA;
		} else if (!std::strcmp(platformType, "HWND")) {
			platform = WebviewGui::HWND;
		} else if (!std::strcmp(platformType, "X11EmbedWindowID")) {
			platform = WebviewGui::X11EMBED;
		}
		
		// There are also `::create()` and `::createUnique()` functions
		webview = WebviewGui::createShared(
			platform,
			"relative/path.html",
			"/absolute/path/including/all/resources/"
		);
		if (webview) {
			// The type is known from the `platform` argument
			webview->attach(nativeParent);
		} 
	}
};
```

When included via CMake, it adds a source file for the actual implementation.

To use in a header-only way (without this source file), use `#define WEBVIEW_GUI_HEADER_ONLY` before including the above header.

### Why not use CHOC?

[CHOC's WebView class](https://github.com/Tracktion/choc/blob/main/choc/gui/choc_WebView.h) is great, but it still requires platform-specific code to attach to the native views.  It also doesn't handle the gnarly event-view stuff, and is slightly more opinionated about how values are passed between C++/JS.

This library does (currently) use CHOC under the hood, except for OSX (Cocoa).

## TODOs

Thanks to August / Imagiro for sharing their [implementation](https://github.com/augustpemberton/imagiro_webview/tree/main), and giving me permission to raid it.  These are the things they said they'd done:

* Key event stuff (Mac and Windows have separate problems)
* Absolute paths for dragged-in files
* A crash on Mac if you press Esc(?)

Additionally, I would like to make some existing JS APIs usable in webviews: 

* [`Element.setPointerCapture()`](https://developer.mozilla.org/en-US/docs/Web/API/Element/setPointerCapture) (hiding the mouse while dragging) without the big warning banner.
* Right-click / context-menu stuff

## CLAP helper

There is a [draft CLAP extension](https://github.com/free-audio/clap/blob/ee8af6c82551aac6f5e8a0d5bd1980cc9c8d832b/include/clap/ext/draft/webview.h) for using webview UIs.  This is the primary way that WCLAPs (CLAPs compiled to WebAssembly) can provide a GUI, but it's an increasingly common pattern for native apps/plugins in general.  The extension follows the pattern above: passing messages as opaque bytes between the (W)CLAP plugin and the webview/`<iframe>`, as well as optionally providing custom resources.

Native hosts don't support this webview extension (and are unlikely to), so this repo includes a helper (in [`clap-webview-gui.h`](include/webview-gui/clap-webview-gui.h)) which implements the `clap.gui` extension, based on the plugin's webview extension.  You can replace any methods from this extension, for example to define your own (re)sizing logic (which can be used by webview-based hosts as well). 

The helper provides a "host webview extension", which you should use instead of checking the host's actual webview extension.  Even if a host *does* support the webview extension, this replacement will send messages to its native webview instead, if one currently exists.  This helps the plugin pretend that only webview GUIs are actually being used.
 
The idea is for webview-based CLAP plugins to primarily use the webview extension, and let this helper wire up the `clap.gui`, only overriding that where relevant.

```cpp
struct MyClapPlugin {
	const clap_plugin clapPlugin{...};
	const clap_host *host;

	webview_gui::ClapWebviewGui guiHelper;
	const clap_host_webview *hostWebview;

	MyClapPlugin(const clap_host *host) : host(host) {
		guiHelper.setSize(600, 300);
	}
	
	void someMainThreadMethod() {
		auto *message = "message-bytes";
		auto *bytes = (const unsigned char *)message;
		uint32_t length = std::strlen(message);

		// You can either call the helper to send messages directly:
		auto success = guiHelper.send(bytes, length);
		
		// Or use the host webview extension (actually provided by the helper)
		hostWebview->send(host, bytes, length);
	}
	
	//----- `clap_plugin` methods -----
	static bool plugin_init(const clap_plugin *plugin) {
		auto &self = getSelf(plugin); // somehow - in this case just casting the pointer would work
		
		// ... various setup bits

		// This queries the plugin itself for (webview) extensions, so only call this when that's ready 
		self.guiHelper.init(plugin, self.host);
		self.hostWebview = self.guiHelper.extHostWebview;
	}
	static void * plugin_get_extension(const clap_plugin *plugin, const char *extId) {
		auto &myClapPlugin = getSelf(plugin);
		// ... all your existing stuff

		// This provides the `clap.gui` extension, as well as adapters for any version of the webview extension
		if (!std::strcmp(extId, CLAP_EXT_GUI)) {
			// Override with a custom size function
			guiHelper.extPluginGui->adjustSize = gui_adjust_size;
			
			return guiHelper.extPluginGui;
		}
	}
	
	//----- subset of `clap_gui` methods -----
	static bool gui_adjust_size(const clap_plugin *plugin, uint32_t *w, uint32_t *h) {
		// enforce minimum size
		if (*w < 300) *w = 300;
		if (*h < 200) *h = 200;
		return true;
	}
};
```
