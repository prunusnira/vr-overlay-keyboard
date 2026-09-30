# Windows IME Overlay Prototype

This is a Windows-only VK-1 prototype. It renders a Qt input widget into a SteamVR dashboard overlay, forwards controller pointer events to that widget, sends virtual keys through Win32 `SendInput`, subscribes to TSF UI-less candidate notifications, and fills the VRChat Chatbox through OSC.

It is meant to answer whether this combination works with the installed Windows IMEs and can fill the VRChat Chatbox. It does not implement a TSF text store (`ITextStoreACP`).

## Requirements

- Windows 10 or 11 x64
- Steam and SteamVR running, with an HMD connected
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.21 or later
- Qt 6.5 or later, built for the same MSVC toolchain
- Valve OpenVR SDK with `headers/openvr.h`, `lib/win64/openvr_api.lib`, and preferably `bin/win64/openvr_api.dll`
- Windows Korean and Japanese IMEs installed for those checks

Make `Qt6_DIR` discoverable by CMake, or pass the Qt installation prefix using `CMAKE_PREFIX_PATH`. Set `OPENVR_ROOT` to the extracted OpenVR SDK root.

```powershell
cmake -S . -B build -A x64 `
  -DOPENVR_ROOT="C:/SDK/openvr" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release
```

Run `build/Release/windows-ime-overlay.exe` while SteamVR is running. The app window is also kept on the Windows desktop because the first experiment needs a real Qt top-level window and Win32 keyboard focus. Use **Show in SteamVR** to open its dashboard tab.

## Input path and limits

- The editor is a normal Qt `QLineEdit`. Qt's Windows input context handles the edit control and Windows IME messages.
- The overlay shows the current Windows input language and keeps the loaded input locales visible as selectable rows. Selecting one calls `ActivateKeyboardLayout` for this app's UI thread. To add a missing Korean or Japanese IME, install it in Windows Settings first; this prototype does not install or configure IMEs.
- The language row and candidate list are part of the rendered widget so they remain clickable inside the overlay. The prototype refreshes its raw overlay image at 10 Hz; this keeps the diagnostic upload path simple.
- Letter, Space, Enter, Backspace, and Shift buttons submit Win32 key-down/key-up events with `SendInput`. Regular typing/editing keys use scan-code events to model physical key positions; IME mode buttons use their Windows virtual-key codes. They do not append a finished character directly to the input field.
- The `한글 모드`, `かな`, and `漢字` buttons send Windows IME virtual keys. `かな` sends `VK_IME_ON` to request Hiragana mode; `Clear input` only resets the diagnostic editor so a composition can be repeated.
- Key submission is refused unless the app owns the Windows foreground window and the Qt editor still has focus. Click **Focus input** and check the status first. Windows can deny foreground activation; the prototype reports that result rather than sending keys to a different foreground app.
- **VRChat 입력란 채우기** sends the editor text to `127.0.0.1:9000` as OSC `/chatbox/input`, with `send=false` and notification disabled. It fills/opens the VRChat Chatbox; the user confirms and sends from VRChat. The button rejects strings longer than 144 Unicode code points and OSC strings containing a null character. UDP success confirms only that Windows accepted the local datagram, not that VRChat received it.
- Enable OSC in VRChat from the Action Menu under **OSC > Enabled**. The prototype uses the default incoming port 9000; a custom VRChat OSC port is not configurable yet.
- The diagnostic log records editor focus transitions, key events, IME preedit/commit events, and foreground/focus state changes. If composition fails, compare those entries for physical keyboard input and virtual keyboard input.
- TSF UI-less mode is activated with `TF_TMAE_UIELEMENTENABLEDONLY`. When the IME exposes a candidate-list UI element, the sink hides that IME UI, reads its candidates and page/selection data, and shows the strings in the overlay. Candidate clicks request `SetSelection` and `Finalize` through `ITfCandidateListUIElementBehavior`.
- Launch with `--disable-tsf-sink` to compare text composition without activating the separate TSF candidate sink. Candidate capture is unavailable in this diagnostic run.
- Qt's IMM-based input handling and this separate TSF sink may not share the same composition. That is one of the prototype's open questions. If the IME UI disappears but no candidate list appears here, record it as a failure. This prototype does not create the TSF document/text store required to route `ITfKeystrokeMgr` key events directly.
- A candidate-list UI element or candidate behavior interface may be unavailable for a particular IME. The UI-less sink leaves unknown UI elements to the text service; unsupported candidate selection is reported in the log.

## Manual checks

Record the Windows build, SteamVR version, HMD, Qt version, and exact IME names/versions in the run log before testing. Keep the app's desktop window available for checking its log.

1. Start SteamVR and the prototype. Check the overlay's **Current Windows input language** label and the visible language rows. Confirm the rows contain the Korean and Japanese layouts you intend to test.
2. Select English, Korean, and Japanese from the overlay rows. Confirm the current-language label changes each time. The rows only list locales already loaded in Windows; install a missing IME in Windows Settings before rerunning.
3. Select English, click **Focus input**, then **Show in SteamVR**. Point at the dashboard tab and click several keys. Confirm pointer events update the app log or input field. Confirm the editor still has focus and this app owns the Windows foreground window.
4. With English active, use virtual `A`, `B`, Space, Backspace, and Shift. Compare with a physical keyboard.
5. Select Korean. If the IME is in Latin mode, click **한글 모드**. On standard Dubeolsik, click `R`, `K`, `A` in order and record whether the editor visibly passes through `ㄱ`, `가`, `감`. Click Backspace during composition and record the result.
6. Select Japanese. If the IME is not in Hiragana mode, click **かな** or switch the Japanese IME mode through Windows and record which action worked. The **かな** button sends `VK_IME_ON` (the Japanese IME's Hiragana-mode key). Click `N`, `I`, `H`, `O`, `N`, `G`, `O` in order. Record whether the editor shows `にほんご` as the composition. Press Enter and record whether kana is committed.
7. Click **Clear input**, type `N`, `I`, `H`, `O`, `N`, `G`, `O` again, then Space. Record whether a candidate list arrives in the overlay, whether `日本語` is present, and whether clicking it finalizes the conversion.
8. Repeat after returning focus to the editor, and note whether the IME's desktop candidate UI was hidden, remained visible, or appeared in addition to the overlay list.
9. Enable OSC in VRChat. Enter English and mixed Korean/Japanese text, click **VRChat 입력란 채우기**, and confirm the VRChat Chatbox opens with the same text without immediately posting it. Record whether non-ASCII text is preserved.

The prototype does not establish non-ASCII OSC round trips, Chinese input, or compatibility with all Windows IMEs. Confirm them on the target Windows/VRChat setup.

## Run log

| Field | Result |
| --- | --- |
| Date and tester | 2026-09-30–2026-10-01, user-reported observations |
| Windows build | Not recorded |
| SteamVR / HMD | Not recorded |
| Qt version | 6.12.0 |
| Input-language selection | User confirmed the displayed language changes |
| Korean IME and result | Virtual input produced a preedit immediately followed by a commit for each jamo (`ㄱ`, `ㅏ`, etc.); physical input updated the preedit across keys (`ㅁ` → `마`) before committing. User reports editor focus does not drop during typing and the commit occurs when pressing a virtual key. Disabling the TSF candidate sink did not change the per-jamo commits. Exact IME/version not recorded |
| Japanese IME and result | User confirmed the Japanese mode-switch issue is resolved after the `かな` button change. Exact IME/version not recorded |
| Foreground/focus status | `FocusOut` and foreground changes were logged after the Korean key events; user reports focus remained during typing |
| TSF candidate events and selection | Not recorded. The user repeated the Korean input with `--disable-tsf-sink`; per-jamo commits persisted. |
| VRChat OSC and result | User confirmed text from the prototype arrived in the VRChat Chatbox through OSC. Exact text, language, and runtime versions were not recorded. |
| Prototype status / remaining limits | Prototype is complete for the confirmed OSC-to-Chatbox flow. Korean virtual input still commits jamo separately; TSF candidate selection, Chinese input, and multilingual OSC round-trip were not recorded. |
| Notes / failure boundary | The TSF candidate sink is not the cause of the Korean per-jamo commits. The prototype intercepts button mouse events from both the desktop UI and VR pointer path, then invokes the button action directly to avoid mouse-event side effects on the editor's active IME composition. |

## References

- [Valve OpenVR Hello World Overlay](https://github.com/ValveSoftware/openvr/tree/master/samples/helloworldoverlay)
- [OpenVR overlay overview](https://github.com/ValveSoftware/openvr/wiki/IVROverlay_Overview)
- [Microsoft TSF UI-less mode](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview)
- [Microsoft ITfUIElementSink::BeginUIElement](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfuielementsink-beginuielement)
- [Microsoft ITfCandidateListUIElement](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfcandidatelistuielement)
- [Microsoft ITfCandidateListUIElementBehavior::SetSelection](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcandidatelistuielementbehavior-setselection)
- [Microsoft GetKeyboardLayout](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getkeyboardlayout)
- [Microsoft ActivateKeyboardLayout](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-activatekeyboardlayout)
- [Microsoft Virtual-Key Codes](https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes)
