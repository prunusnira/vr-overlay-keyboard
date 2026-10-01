# VR Overlay Keyboard app

`app/` is an independent Windows CMake project. Its C++ implementation is written for this module layout. The prototype remains unchanged; its confirmed controller, language-switching, Japanese IME mode, and Chatbox OSC flows are the behavior baseline for this application.

## Requirements

- Windows 10 or 11, x64
- Steam and SteamVR running, with an HMD connected
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.21 or later
- Qt 6.5 or later, built for the same MSVC toolchain
- Valve OpenVR SDK with `headers/openvr.h`, `lib/win64/openvr_api.lib`, and `bin/win64/openvr_api.dll`
- Windows input languages and IMEs installed for the languages to use

## Configure and build

Set `OPENVR_ROOT` to the extracted OpenVR SDK root. Make `Qt6_DIR` discoverable or set `CMAKE_PREFIX_PATH` to the Qt installation prefix.

```powershell
cmake -S app -B app/build -A x64 `
  -DOPENVR_ROOT="C:/SDK/openvr" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build app/build --config Release
```

Run `app/build/Release/vr-overlay-keyboard.exe` while SteamVR is running. The app keeps a normal desktop window for Windows text-input focus and status diagnostics, and renders the same keyboard UI into a hidden-by-default ordinary OpenVR overlay.

## SteamVR Input setup

The app registers `resources/steamvr/actions.json` at startup and reads three actions: `/actions/keyboard/in/ToggleKeyboard`, `ControllerPose`, and `PointerClick`. No controller-specific default binding is included until the supported controller is selected. In SteamVR's controller input settings, bind the toggle action, the target controller's aim pose to **Controller Pointer Pose**, and its select input to **Controller Pointer Click**. The app maps the pose ray to the ordinary overlay with OpenVR `ComputeOverlayIntersection`, then sends pointer events to the UI. Verify the action bindings and pointer path on the target setup; neither has been checked in an HMD yet.

## Text and Chatbox input

Use **Focus input** from the desktop window before entering text. The app reports whether Windows granted foreground focus. Virtual key events are sent only while this process owns the Windows foreground window. The controller pointer interaction and language controls follow the flow confirmed in the prototype. Japanese mode switching uses the Windows IME key action. Korean composition and TSF candidate UI support remain input-method-specific checks.

**Fill VRChat Chatbox** sends OSC `/chatbox/input` to `127.0.0.1:9000` with `send=false` and notifications disabled. Enable OSC in VRChat. A successful local UDP send only means Windows accepted the datagram; confirm that the text appears in VRChat. The 144 Unicode code-point and embedded-null checks are applied before sending. Non-ASCII round trips require separate verification.

## Known limits

- This is the first source implementation. It has not been built or exercised in Windows, SteamVR, or an HMD yet.
- The prototype confirmed controller pointer interaction in a dashboard overlay and the basic Chatbox OSC flow. The ordinary overlay pointer path through `ControllerPose`, `PointerClick`, and `ComputeOverlayIntersection`, plus `ToggleKeyboard` delivery with the dashboard closed, need HMD validation in this app.
- Windows may deny foreground focus. The app reports that state and does not send virtual keys to another foreground process.
- Korean virtual input previously committed jamo separately. The cause is not known, and this app has not been validated against that case.
- TSF candidate selection, Chinese IME behavior, and multilingual OSC round trips are unverified.
- The SteamVR action manifest currently has no default bindings. User bindings can be created in SteamVR settings.
