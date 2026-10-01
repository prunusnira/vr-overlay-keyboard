cmake_minimum_required(VERSION 3.21)

project(vr-overlay-keyboard LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The application uses Windows IME, TSF, and OpenVR Windows SDK libraries.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Configure the application with a 64-bit toolchain.")
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

add_library(app_core STATIC
    src/core/keyboard_application.cpp
    src/core/keyboard_application.h
    src/core/app_contracts.h
)
target_include_directories(app_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(keyboard_ui STATIC
    src/ui/keyboard_widget.cpp
    src/ui/keyboard_widget.h
)
target_link_libraries(keyboard_ui PUBLIC app_core Qt6::Widgets)

add_library(steamvr_action_input STATIC
    src/input/steamvr_action_source.cpp
    src/input/steamvr_action_source.h
)
target_include_directories(steamvr_action_input PUBLIC "${OPENVR_INCLUDE_DIR}")
target_link_libraries(steamvr_action_input PUBLIC "${OPENVR_LIBRARY}")

add_library(openvr_overlay_adapter STATIC
    src/overlay/openvr_overlay.cpp
    src/overlay/openvr_overlay.h
)
target_include_directories(openvr_overlay_adapter PUBLIC "${OPENVR_INCLUDE_DIR}")
target_link_libraries(openvr_overlay_adapter PUBLIC app_core "${OPENVR_LIBRARY}")

add_library(windows_input_language STATIC
    src/platform/windows/input_language_service.cpp
    src/platform/windows/input_language_service.h
)
target_link_libraries(windows_input_language PUBLIC app_core)

add_library(windows_virtual_key_sender STATIC
    src/platform/windows/virtual_key_sender.cpp
    src/platform/windows/virtual_key_sender.h
)
target_link_libraries(windows_virtual_key_sender PUBLIC app_core user32)

add_library(windows_tsf_input STATIC
    src/ime/tsf_input.cpp
    src/ime/tsf_input.h
)
target_link_libraries(windows_tsf_input PUBLIC app_core PRIVATE ole32 oleaut32 uuid)

add_library(chatbox_osc STATIC
    src/osc/osc_client.cpp
    src/osc/osc_client.h
)
target_link_libraries(chatbox_osc PUBLIC app_core ws2_32)

add_executable(vr-overlay-keyboard
    src/main.cpp
)
target_compile_definitions(app_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(keyboard_ui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(steamvr_action_input PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(openvr_overlay_adapter PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(windows_input_language PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(windows_virtual_key_sender PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(windows_tsf_input PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(chatbox_osc PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(vr-overlay-keyboard PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

target_link_libraries(vr-overlay-keyboard PRIVATE
    app_core
    keyboard_ui
    steamvr_action_input
    openvr_overlay_adapter
    windows_input_language
    windows_virtual_key_sender
    windows_tsf_input
    chatbox_osc
    Qt6::Widgets
    ole32
)

set(STEAMVR_ACTION_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/resources/steamvr/actions.json")
add_custom_command(TARGET vr-overlay-keyboard POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:vr-overlay-keyboard>/resources/steamvr"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${STEAMVR_ACTION_MANIFEST}"
        "$<TARGET_FILE_DIR:vr-overlay-keyboard>/resources/steamvr/actions.json"
    VERBATIM
)

find_file(OPENVR_RUNTIME_DLL openvr_api.dll
    HINTS "${OPENVR_ROOT}/bin/win64"
)
if(OPENVR_RUNTIME_DLL)
    add_custom_command(TARGET vr-overlay-keyboard POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OPENVR_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:vr-overlay-keyboard>/openvr_api.dll"
        VERBATIM
    )
endif()
