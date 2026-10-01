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

find_package(OpenGL REQUIRED)
find_path(OPENVR_INCLUDE_DIR openvr.h
    HINTS "${OPENVR_ROOT}/headers"
    REQUIRED
)
find_library(OPENVR_LIBRARY NAMES openvr_api
    HINTS "${OPENVR_ROOT}/lib/win64"
    REQUIRED
)

include(FetchContent)
FetchContent_Declare(dear_imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b
    GIT_SHALLOW TRUE
)
# upstream에는 루트 CMakeLists가 없으므로 소스만 가져오고 필요한 파일을 아래에서 직접 타깃으로 묶는다.
FetchContent_MakeAvailable(dear_imgui)

# 정적 링크를 사용해 Dear ImGui 런타임 DLL 없이 단일 앱 실행 파일을 만든다.
add_library(dear_imgui STATIC
    "${dear_imgui_SOURCE_DIR}/imgui.cpp"
    "${dear_imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${dear_imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${dear_imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${dear_imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
    "${dear_imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    "${dear_imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(dear_imgui PUBLIC
    "${dear_imgui_SOURCE_DIR}"
    "${dear_imgui_SOURCE_DIR}/backends"
    "${dear_imgui_SOURCE_DIR}/misc/cpp"
)
target_compile_definitions(dear_imgui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(dear_imgui PUBLIC OpenGL::GL dwmapi)

add_library(app_core STATIC
    src/core/keyboard_application.cpp
    src/core/keyboard_application.h
    src/core/controller_summon_trigger.cpp
    src/core/controller_summon_trigger.h
    src/core/app_contracts.h
)
target_include_directories(app_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(MSVC)
    target_compile_options(app_core PRIVATE /utf-8)
endif()

add_library(keyboard_ui STATIC
    src/ui/keyboard_ui.cpp
    src/ui/keyboard_ui.h
    src/ui/settings_ui.cpp
    src/ui/settings_ui.h
    src/ui/ui_text_catalog.cpp
    src/ui/ui_text_catalog.h
    src/ui/imgui_input_session.cpp
    src/ui/imgui_input_session.h
)
target_link_libraries(keyboard_ui PUBLIC app_core dear_imgui)
target_compile_definitions(keyboard_ui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(keyboard_ui PRIVATE /utf-8)
endif()

add_library(win32_imgui_host STATIC
    src/platform/windows/imgui_host.cpp
    src/platform/windows/imgui_host.h
    src/platform/windows/ime_composition.cpp
    src/platform/windows/ime_composition.h
    src/platform/windows/virtual_mouse_router.cpp
    src/platform/windows/virtual_mouse_router.h
)
target_include_directories(win32_imgui_host PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(win32_imgui_host PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(win32_imgui_host PUBLIC dear_imgui opengl32 gdi32 user32 imm32)

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

add_library(windows_settings_store STATIC
    src/platform/windows/settings_store.cpp
    src/platform/windows/settings_store.h
)
target_include_directories(windows_settings_store PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(windows_settings_store PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(windows_settings_store PUBLIC app_core)
if(MSVC)
    target_compile_options(windows_settings_store PRIVATE /utf-8)
endif()

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

add_executable(vr-overlay-keyboard WIN32
    src/main.cpp
)
target_compile_definitions(app_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
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
    win32_imgui_host
    steamvr_action_input
    openvr_overlay_adapter
    windows_input_language
    windows_settings_store
    windows_virtual_key_sender
    windows_tsf_input
    chatbox_osc
    ole32
)

set(STEAMVR_ACTION_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/resources/steamvr/actions.json")
add_custom_command(TARGET vr-overlay-keyboard POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:vr-overlay-keyboard>/resources/steamvr"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${STEAMVR_ACTION_MANIFEST}"
        "$<TARGET_FILE_DIR:vr-overlay-keyboard>/resources/steamvr/actions.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "$<TARGET_FILE_DIR:vr-overlay-keyboard>/THIRD_PARTY_NOTICES.md"
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

option(KEYBOARD_BUILD_INPUT_TESTS "Build headless ImGui input regression tests" OFF)
if(KEYBOARD_BUILD_INPUT_TESTS)
    enable_testing()
    add_executable(imgui_input_session_tests tests/imgui_input_session_tests.cpp)
    target_link_libraries(imgui_input_session_tests PRIVATE keyboard_ui)
    if(MSVC)
        target_compile_options(imgui_input_session_tests PRIVATE /utf-8)
    endif()
    add_test(NAME imgui_input_session COMMAND imgui_input_session_tests)
endif()
