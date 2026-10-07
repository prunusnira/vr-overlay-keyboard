# VR Overlay Keyboard app

`app/` is an independent Windows CMake project. It uses Dear ImGui with the Win32 platform backend and OpenGL3 renderer. The Win32/OpenGL host renders the UI in a desktop window and reads the same frame as RGBA pixels for a persistent OpenGL texture submitted to the OpenVR overlay. Overlay readback runs only while the overlay is visible, at up to about 60 frames per second; the desktop window continues to render every frame. The prototype remains unchanged and is the behavior reference for controller input, language switching, Japanese IME mode, and Chatbox OSC.

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

The app registers `resources/steamvr/actions.json` at startup and reads the `ToggleKeyboard`, `ControllerPose`, `PointerClick`, `PointerManipulation`, and left/right summon-button actions. A default SteamVR binding is included for Meta Quest (`oculus_touch`): both controller poses and triggers drive the pointer, the selected controller's thumbstick drives overlay size and distance while Grip is held, and grip drives overlay dragging. Other controller types still need their actions bound in SteamVR. If SteamVR has an older custom binding saved for this app, select the app's default binding in **Controller Input Binding** to load the shipped profile. The app uses OpenVR `ComputeOverlayIntersection` within the visible panel and projects the ray onto the overlay plane outside it, then sends pointer events to Dear ImGui.

The overlay is placed 1.25 m in front of the HMD the first time it is shown, then keeps its position in standing tracking space while rotating to face the HMD. In **Options**, choose the pointer hand and adjust horizontal and vertical offsets from -50% to +50% of the overlay size. Positive values shift the pointer right or down; negative values shift it left or up. The shift is constant across the panel, so pointer speed remains uniform. The active hit area extends invisibly beyond the panel by the offset amount, keeping the full UI reachable. The default vertical offset is +2.4%. Aim that controller at the overlay, hold **Grip**, move the controller, and release Grip to leave the overlay at the new position. While holding Grip, move the thumbstick left or right to shrink or enlarge the panel, and up or down to move it farther away or closer. The drag keeps its starting offset so the panel does not jump when grabbed.

Options preserves the editor's input focus and remains above the main keyboard. Pointer adjustment sliders use the same virtual input path as keyboard buttons. VR pointer events are queued after the Win32 backend's desktop mouse update so the visible cursor and click position agree. Grip dragging still requires the selected controller to point at the panel when starting; it can cancel an ongoing trigger click. Release the summon Grip once before dragging. Position and facing rotation are submitted together from the app's stored world transform. **Input diagnostics** shows each Grip's binding and pressed state, and the status line records drag start, end, and OpenVR errors. The previous offset mapping and full-area reachability were confirmed on an HMD; this uniform-speed mapping and its extended hit area still need HMD confirmation.

## Options and controller summon

Use **Options** in the keyboard screen to open a separate ImGui settings window. It is drawn in the same frame as the keyboard, so the window appears in both the desktop view and the OpenVR overlay. The language setting changes the app UI only; Windows input language remains a separate control. Korean is the first-run UI language.

The default summon combination is **Right Grip + Right B**, with a **0 second** hold time. Select one or more logical controls in Options and adjust the hold time from 0 to 3 seconds in 0.1-second steps. All selected controls must be pressed together. The combination shows a hidden overlay once; release the controls before it can trigger again. The existing **Toggle Keyboard** SteamVR action remains a show/hide toggle.

Settings are saved to `%LOCALAPPDATA%\VROverlayKeyboard\settings.ini`. Available logical summon actions are left/right Grip, Trigger, A, B, Menu, Joystick, and Trackpad. Other controller models may not expose all controls; bind the selected actions in SteamVR and confirm that Options reports them active. The keyboard screen's **Hide keyboard overlay** button hides the VR overlay without closing the desktop app.

## Text and Chatbox input

The text field receives focus automatically when this app starts in the foreground and retains it while the app is active. Returning to the app restores input focus. Clicking the text field in the VR overlay activates the ImGui editor without changing Windows foreground focus. Virtual characters and editing keys go directly to this editor, so another application can remain the Windows foreground window. In Korean keyboard mode, the app composes 2-beolsik jamo into Hangul syllables locally. In Japanese Hiragana mode, the app converts romaji to kana locally while unfocused; kanji conversion and native Japanese IME input still require **Focus input**. Focus is restored only when missing; an active editor is not reinitialized on each frame or key. The Windows key adapter checks foreground ownership immediately before sending each key.

`ui/imgui_input_session` follows the prototype's pointer flow: a virtual button consumes its press and release without delivering that gesture to the editor, and runs its action once when released inside the same button. This prevents Dear ImGui's outside-click handler from deactivating the editor or changing its selection. Candidate scrolling also retains editor focus. `KeyboardUi` draws the screen and invokes app actions; Windows focus, `SendInput`, and TSF candidate handling remain in their own adapters.

`platform/windows/ime_composition` handles Windows IMM composition messages once, ahead of the ImGui Win32 backend. `GCS_COMPSTR` updates the fixed preview without modifying the edit buffer; only `GCS_RESULTSTR` supplies committed text to ImGui. When this app owns Windows input focus, Windows IME handles editing keys during composition so Backspace or Space does not also edit the committed buffer. Without Windows focus, Korean 2-beolsik input is composed locally into Hangul syllables and Japanese Hiragana mode converts romaji to kana; local Japanese input does not provide kanji candidates. Clear cancels Windows preedit as well as clearing the editor. Fill requests native IME confirmation first and waits for the resulting text before sending OSC. TSF continues to supply candidates and selection behavior. Word conversion with the installed Windows input methods still requires native execution validation.

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
- OpenGL readback performance at the current 60 Hz overlay update rate has not been measured on the target setup.
- The prototype confirmed controller pointer interaction in a dashboard overlay and the basic Chatbox OSC flow. The ordinary overlay pointer path through `ControllerPose`, `PointerClick`, and `ComputeOverlayIntersection`, plus `ToggleKeyboard` delivery with the dashboard closed, need HMD validation in this app.
- Windows may deny foreground focus. Native IME keystrokes remain guarded against reaching another foreground process; Korean Hangul and Japanese Hiragana are composed locally, while Japanese kanji conversion still needs the Windows IME.
- The user still observed per-jamo Korean commits with the mouse before the thread-scoped mouse router was added. The new native mouse route needs validation with the installed Korean IME. Controller input is also unverified in this app.
- TSF candidate selection, Chinese IME behavior, and multilingual OSC round trips are unverified.
- The Meta Quest Touch default profile and visible pointer cursor are implemented but still need HMD confirmation. Other controller models need a SteamVR binding profile.

## Third-party license

Dear ImGui is distributed under the MIT License. Its notice is included in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and copied beside the built executable.
