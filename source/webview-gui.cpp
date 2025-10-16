#include "webview-gui/webview-gui.h"

#include <iostream>
#define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;

#ifndef WEBVIEW_GUI_HEADER_ONLY
#	include "../include/webview-gui/_impl/webview-gui.hxx"
#endif
