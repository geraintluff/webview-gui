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

There is a [draft CLAP extension](https://github.com/free-audio/clap/blob/ee8af6c82551aac6f5e8a0d5bd1980cc9c8d832b/include/clap/ext/draft/webview.h) for using webview UIs.  This is the primary way that WCLAPs (CLAPs compiled to WebAssembly) can provide a GUI, but it's an increasingly common pattern for native plugins as well.  The extension follows exactly the pattern above: passing opaque bytes between the (W)CLAP plugin and the webview/`<iframe>`.

Native hosts don't support this extension (and are unlikely to), so this repo includes a helper (in[`clap-webview-gui.h`](include/webview-gui/clap-webview-gui.h)) which implements the `clap.gui` extension, based on the plugin's webview extension.  This helper can be configured with size/resizing hints, which even webview-supporting hosts may inspect through `clap.gui`. 

The idea is for webview-based CLAP plugins to just implement the webview extension, and use this helper for `clap.gui`.  This helper _also_ provides compatibility layers, to support older/newer versions of the webview extension.

```cpp
struct MyClapPlugin {
	const clap_plugin clapPlugin{...};
	const clap_host *host;

	MyClapPlugin(const clap_host *host) : host(host) {...}

	// The helper needs to be able to find itself from its callbacks
	static void * pluginToGuiHelper(const clap_plugin *plugin) {
		auto &myClapPlugin = getSelf(plugin);
		return &myClapPlugin.guiHelper;
	}
	webview_gui::ClapWebviewGui<pluginToGuiHelper> guiHelper;

	// Use the helper to send messages (adapts to whichever extensions the host actually supports)
	void someMainThreadMethod() {
		auto *message = "message-bytes";
		auto success = guiHelper.send((const unsigned char *)message, std::strlen(message));
		// ... it *shouldn't* fail, but it's allowed to according to the webview extension(s)
	}
	
	//----- `clap_plugin` methods -----
	static bool plugin_init(const clap_plugin *plugin) {
		auto &myClapPlugin = getSelf(plugin);
		// ... all your existing stuff

		// If the webview extension returns a relative path, it's resolved against this
		auto resourcePath = getBundleResourcePath();

		// This queries the plugin itself for (webview) extensions, so only call when that's ready 
		myClapPlugin.guiHelper.init(plugin, myClapPlugin.host, resourcePath);
	}
	static void * plugin_get_extension(const clap_plugin *plugin, const char *extId) {
		auto &myClapPlugin = getSelf(plugin);
		// ... all your existing stuff

		// This provides the `clap.gui` extension, as well as adapters for any version of the webview extension
		return myClapPlugin.guiHelper.getExtension(extId);
	}
};
```
