#pragma once

#if __APPLE__ && !TARGET_OS_IPHONE
#	include "./platform/apple-osx.h"
#elif defined(__has_include) && (__has_include("choc/gui/choc_WebView.h") ||  __has_include("./platform/choc/gui/choc_WebView.h"))
#	include "./platform/choc.h"
#else
#	include "./platform/not-supported.h"
#endif
