# VR Overlay Keyboard app

`app/` is an independent Windows CMake project. It uses Dear ImGui with the Win32 platform backend and OpenGL3 renderer. The Win32/OpenGL host renders the UI in a desktop window and reads the same frame as RGBA pixels for the OpenVR overlay. Overlay readback runs only while the overlay is visible, at about 30 frames per second; the desktop window continues to render every frame. The prototype remains unchanged and is the behavior reference for controller input, language switching, Japanese IME mode, and Chatbox OSC.

## Requirements

- Windows 10 or 11, x64
- Steam and SteamVR running, with an HMD connected for overlay use
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.21 or later
- Git and network access during CMake configuration so FetchContent can obtain Dear ImGui v1.92.9b
- Valve OpenVR SDK with `headers/openvr.h`, `lib/win64/openvr_api.lib`, and `bin/win64/openvr_api.dll`
- Windows input languages and IMEs installed for the languages to use

Qt is not used by this application. Dear ImGui is compiled into the executable, so the app does not need Qt runtime DLLs. CMake copies the OpenVR runtime DLL, SteamVR action manifest, and third-party license notice next to the executable.

## Configure and build

Set `OPENVR_ROOT` to the extracted OpenVR SDK root:

```powershell
cmake -S app -B app/build -A x64 -DOPENVR_ROOT="C:/SDK/openvr"
cmake --build app/build --config Release
```

Run `app/build/Release/vr-overlay-keyboard.exe` while SteamVR is running. The app opens a desktop window for Windows text input and diagnostics. The OpenVR overlay starts hidden and can be toggled with the configured SteamVR Input action.

## SteamVR Input setup

The app registers `resources/steamvr/actions.json` at startup and reads three actions: `/actions/keyboard/in/ToggleKeyboard`, `ControllerPose`, and `PointerClick`. No controller-specific default binding is included until the supported controller is selected. In SteamVR's controller input settings, bind the toggle action, the target controller's aim pose to **Controller Pointer Pose**, and its select input to **Controller Pointer Click**. The app maps the pose ray to the ordinary overlay with OpenVR `ComputeOverlayIntersection`, then sends pointer events to Dear ImGui. Verify the action bindings and pointer path on the target setup; neither has been checked in an HMD yet.

## Text and Chatbox input

The text field receives focus automatically when this app starts in the foreground and retains it while the app is active. Returning to the app restores input focus. Clicking the text field or **Focus input** also requests Windows foreground focus, and the app reports whether Windows granted it. Focus is restored only when missing; an active editor is not reinitialized on each frame or key. The Windows key adapter checks foreground ownership immediately before sending each key.

`ui/imgui_input_session` follows the prototype's pointer flow: a virtual button consumes its press and release without delivering that gesture to the editor, and runs its action once when released inside the same button. This prevents Dear ImGui's outside-click handler from deactivating the editor or changing its selection. Candidate scrolling also retains editor focus. `KeyboardUi` draws the screen and invokes app actions; Windows focus, `SendInput`, and TSF candidate handling remain in their own adapters.

`platform/windows/ime_composition` handles Windows IMM composition messages once, ahead of the ImGui Win32 backend. `GCS_COMPSTR` updates the fixed preview without modifying the edit buffer; only `GCS_RESULTSTR` supplies committed text to ImGui. During composition, Windows IME handles editing keys so Backspace or Space does not also edit the committed buffer. Clear cancels preedit as well as clearing the editor. Fill requests IME confirmation first and waits for the resulting text before sending OSC. TSF continues to supply candidates and selection behavior. Word conversion with the installed Windows input methods still requires native execution validation.

`platform/windows/virtual_mouse_router` intercepts Korean-layout mouse clicks on registered virtual controls using a hook scoped to this app's UI thread. It converts them to the same pointer events used by the controller before forwarding them to ImGui. Editor clicks and selection remain normal mouse input. The router does not acquire OS mouse capture; releasing outside the window or leaving the app cancels the gesture. It leaves the Japanese and English mouse paths unchanged. This addresses the reported Korean behavior where each native mouse click ended the previous jamo composition despite both focus indicators staying ready.

The IME candidate panel reserves a fixed-height row so candidate updates do not move the keyboard. Candidates scroll horizontally when they exceed the panel width.

**Fill VRChat Chatbox** sends OSC `/chatbox/input` to `127.0.0.1:9000` with `send=false` and notifications disabled. Enable OSC in VRChat. A successful local UDP send only means Windows accepted the datagram; confirm that the text appears in VRChat. The 144 Unicode code-point and embedded-null checks are applied before sending. Non-ASCII round trips require separate verification.

## Input regression checks

The optional headless test runs the actual Dear ImGui editor and UI input session without opening a desktop window:

```powershell
cmake -S app -B app/build -DKEYBOARD_BUILD_INPUT_TESTS=ON
cmake --build app/build --config Release
ctest --test-dir app/build -C Release --output-on-failure
```

It reproduces the original outside-click focus loss and checks startup focus, focus retention without resetting selection, app focus event delivery, one action per button release, cancelled drags, normal editor selection, clearing the active buffer, Unicode backspace, and horizontal scrolling. It does not exercise Windows IMM preedit or an HMD.

## Known limits

- The Windows x64 Release build succeeds. The user confirmed Japanese desktop input and conversion after the IMM composition/focus changes. Exact IME/version and HMD behavior remain unrecorded.
- OpenGL readback performance at the current 30 Hz overlay update rate has not been measured on the target setup.
- The prototype confirmed controller pointer interaction in a dashboard overlay and the basic Chatbox OSC flow. The ordinary overlay pointer path through `ControllerPose`, `PointerClick`, and `ComputeOverlayIntersection`, plus `ToggleKeyboard` delivery with the dashboard closed, need HMD validation in this app.
- Windows may deny foreground focus. The app reports that state and does not send virtual keys to another foreground process.
- The user still observed per-jamo Korean commits with the mouse before the thread-scoped mouse router was added. The new native mouse route needs validation with the installed Korean IME. Controller input is also unverified in this app.
- TSF candidate selection, Chinese IME behavior, and multilingual OSC round trips are unverified.
- The SteamVR action manifest currently has no default bindings. User bindings can be created in SteamVR settings.

## Third-party license

Dear ImGui is distributed under the MIT License. Its notice is included in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and copied beside the built executable.
