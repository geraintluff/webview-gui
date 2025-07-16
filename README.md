# Webview GUI

Webview GUIs with simple message-passing, intended for audio effects.  See [`webview-gui.h`](include/webview-gui/webview-gui.h) for the API.

General goals:

* Load a webview, with resources either from a custom callback or from a directory
* The webpage can use `parent.postMessage()` and `addEventListener('message', ...)` to send/receive `ArrayBuffer`s.

It's (currently, possibly forever) based on [CHOC](https://github.com/Tracktion/choc/blob/main/gui/choc_WebView.h).  However, there are still some pieces missing if using CHOC directly: some gnarly event-flow stuff, and actually attaching to the native platform-view.

## TODOs

Thanks to August / Imagiro for sharing their [implementation](https://github.com/augustpemberton/imagiro_webview/tree/main), and giving me permission to raid it.  These are the things they said they'd done:

* Key event stuff (Mac and Windows have separate problems)
* Absolute paths for dragged-in files
* A crash on Mac if you press Esc(?)

Additionally, I would like to make some existing JS APIs usable in webviews: 

* [`Element.setPointerCapture()`](https://developer.mozilla.org/en-US/docs/Web/API/Element/setPointerCapture) (hiding the mouse while dragging) without the big warning banner.
* Right-click / context-menu stuff

## CLAP helper

There is a [draft CLAP extension](https://github.com/free-audio/clap/blob/ee8af6c82551aac6f5e8a0d5bd1980cc9c8d832b/include/clap/ext/draft/webview.h) for using webview UIs.  This is the primary way that WCLAPs (CLAPs compiled to WebAssembly) can provide a GUI, and it follows exactly the pattern above: passing opaque bytes between the native/WASM plugin and the webview/`<iframe>`.

Native hosts don't support this extension (and are unlikely to), so this repo includes [`clap-webview-gui.h`](include/webview-gui/clap-webview-gui.h), which implements the `clap.gui` extension, forwarding messages to/from plugin's webview extension.  The goal is for webview-based CLAP plugins to just implement the webview extension (and give some size/resizing hints through the `clap.gui` extension). 
