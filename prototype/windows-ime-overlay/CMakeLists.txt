cmake_minimum_required(VERSION 3.21)

project(windows-ime-overlay-prototype LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This prototype uses Windows IME, TSF, and OpenVR Windows SDK libraries.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Configure this prototype with a 64-bit toolchain.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(OPENVR_ROOT "" CACHE PATH "Root directory of the Valve OpenVR SDK")

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
find_path(OPENVR_INCLUDE_DIR openvr.h
    HINTS "${OPENVR_ROOT}/headers"
    REQUIRED
)
find_library(OPENVR_LIBRARY NAMES openvr_api
    HINTS "${OPENVR_ROOT}/lib/win64"
    REQUIRED
)

add_executable(windows-ime-overlay
    src/main.cpp
    src/ime/tsf_input.cpp
    src/ime/tsf_input.h
    src/overlay/openvr_overlay.cpp
    src/overlay/openvr_overlay.h
    src/ui/keyboard_widget.cpp
    src/ui/keyboard_widget.h
)

target_include_directories(windows-ime-overlay PRIVATE "${OPENVR_INCLUDE_DIR}")
target_compile_definitions(windows-ime-overlay PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(windows-ime-overlay PRIVATE
    Qt6::Widgets
    "${OPENVR_LIBRARY}"
    ole32
    oleaut32
    msctf
    user32
)

find_file(OPENVR_RUNTIME_DLL openvr_api.dll
    HINTS "${OPENVR_ROOT}/bin/win64"
)
if(OPENVR_RUNTIME_DLL)
    add_custom_command(TARGET windows-ime-overlay POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENVR_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:windows-ime-overlay>/openvr_api.dll"
    )
endif()
