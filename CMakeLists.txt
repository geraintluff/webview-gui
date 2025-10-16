cmake_minimum_required(VERSION 3.24)

add_library(webview-gui
	${CMAKE_CURRENT_SOURCE_DIR}/source/webview-gui.cpp
)
target_include_directories(webview-gui PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ---
# Linking instructions as per CHOC (tests/CMakeLists.txt)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(webview-gui PRIVATE "-framework WebKit")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(gtk3 REQUIRED gtk+-3.0 IMPORTED_TARGET)
    pkg_check_modules(webkit2 REQUIRED webkit2gtk-4.1 IMPORTED_TARGET)
    target_link_libraries(webview-gui PRIVATE PkgConfig::gtk3 PkgConfig::webkit2)
endif()