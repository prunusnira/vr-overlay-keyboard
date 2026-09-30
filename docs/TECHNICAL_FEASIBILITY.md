# SteamVR 오버레이 키보드 기술 조사

조사일: 2026-09-30. 이 문서는 공식 API 문서와 공개 소스에서 확인한 내용이다. 현재 작업 환경은 macOS라 Windows, SteamVR, VRChat의 실제 연동 결과는 포함하지 않는다. 문서의 **확인**은 API와 문서 수준의 확인을 뜻하며, 제품에서의 동작 검증을 뜻하지 않는다.

## 결론

VRChat Chatbox에 완성된 문자열을 채우는 OSC 경로와 SteamVR 오버레이를 표시하고 컨트롤러 클릭을 받는 경로는 공식 API에 있다. Windows IME의 조합 중 문자열과 후보 목록을 앱 화면에 다시 그릴 수 있는 API도 있다. 그러나 이 셋을 연결한 **Windows IME 기반 4개 언어 입력 흐름 전체는 아직 검증되지 않았다**. 특히 컨트롤러 클릭을 IME가 실제 키로 받아들이는지, 오버레이를 누르는 동안 IME 포커스가 유지되는지, 현재 Microsoft 일본어·중국어 입력기가 필요한 후보 인터페이스를 제공하는지는 공식 문서만으로 확정할 수 없다.

따라서 착수 조건은 “모든 API가 존재한다”가 아니라 아래의 Windows 실기기 확인 항목을 통과하는 것이다. 조사만으로 해결할 수 없는 항목을 통과한 것으로 표시하지 않는다.

## 항목별 조사 결과

| 항목 | 문서 조사 결과 | 상태 |
| --- | --- | --- |
| VRChat 기본 Chatbox 입력란 채우기 | `/chatbox/input`에 문자열과 `false`를 보내면 VRChat 키보드를 열고 문자열을 채운다. `true`는 즉시 전송한다. 기본 수신 포트는 9000이며 사용자가 OSC를 켜야 한다. | API 확인, VRChat 실행 확인 필요 |
| Chatbox 글자·줄 제한 | 공식 문서는 최대 144자, 표시 최대 9줄을 명시한다. 줄 수에는 자동 줄바꿈도 포함한다. | 문서 확인 |
| 한글·일본어·중국어 OSC 문자열 | VRChat 문서는 Chatbox 인수를 문자열로 정의하지만, 네 언어별 인코딩과 144자 계산 단위는 명시하지 않는다. OSC 1.0 원문은 기본 문자열을 ASCII로 정의한다. 다국어 문자열의 실제 왕복은 별도 확인이 필요하다. | **확실하지 않음**, VRChat 실측 필수 |
| SteamVR 오버레이 화면·클릭 | `IVROverlay`는 2D 화면을 VR에 표시하고 컨트롤러 포인터를 마우스 이벤트로 전달한다. Valve의 `helloworldoverlay` 예제는 Qt 화면에 이 이벤트를 전달한다. | API와 예제 확인, HMD 실행 확인 필요 |
| Windows IME 조합 과정 표시 | Qt Windows 입력 컨텍스트 소스는 `WM_IME_COMPOSITION`을 받아 조합 중 문자열과 확정 문자열을 Qt 입력 객체에 전달한다. 한국어 처리 분기도 있다. | Qt 소스 확인, 가상 키 조합 확인 필요 |
| 일본어·중국어 변환 | Microsoft의 일본어 IME, 간체 중국어 Pinyin IME는 로마자·병음 변환과 후보 선택을 지원한다. 번체 중국어는 Microsoft Bopomofo의 HanYu Pinyin 배열도 있다. | 일반 Windows 입력 기능 확인, 이 앱과의 연동 확인 필요 |
| HMD 안에 후보 목록 그리기 | TSF UI-less 모드에서 `ITfUIElementSink`로 후보 UI 갱신을 받고 `ITfCandidateListUIElement`로 후보 문자열·선택·페이지를 읽을 수 있다. 앱이 이 데이터를 OpenVR 텍스처에 그려야 한다. | API 확인, 대상 IME별 동작 확인 필요 |
| 후보 클릭으로 선택·확정 | 입력기가 `ITfCandidateListUIElementBehavior`를 제공하면 `SetSelection`, `Finalize`, `Abort`가 있다. 인터페이스 제공 여부는 입력기별로 다를 수 있다. 대체 경로인 숫자·방향키·Space 입력도 가상 키 전달에 성공해야 쓸 수 있다. | API 확인, 대상 IME별 동작 확인 필요 |
| 오버레이 조작과 Windows 입력 포커스 | OpenVR은 오버레이 자체의 포커스·마우스 이벤트를 정의한다. 이것이 앱의 Win32 키보드 포커스를 자동으로 만든다는 문서는 찾지 못했다. Windows의 키보드 이벤트는 포커스가 있는 전경 스레드로 전달되며, 전경 창 변경도 제한된다. | **확실하지 않음**, Windows 실측 필수 |
| 컨트롤러 클릭을 IME 키 입력으로 전달 | `SendInput`은 시스템 입력 스트림에 키를 넣으므로 현재 전경 창 문제를 해결하지 않는다. TSF의 `ITfKeystrokeMgr::KeyDown`은 키 이벤트를 텍스트 서비스에 전달하는 API다. 어느 경로가 SteamVR과 목표 IME에서 안정적인지는 알 수 없다. | API 확인, 통합 동작 미확인 |
| Qt 입력란과 TSF UI-less 후보 수신의 공존 | Qt의 Windows 입력 컨텍스트는 공개 소스 기준 IMM 계열 API와 `WM_IME_*` 메시지로 조합을 처리한다. 별도 TSF UI-less 활성화 및 sink를 붙여 후보 데이터가 같은 입력 흐름에서 오는지는 확인되지 않았다. | **확실하지 않음**, Windows 실측 필수 |
| 중국어 간체·번체 범위 | Windows는 둘 모두에 병음 경로가 있지만 입력기가 다르다. 두 종류를 제품 필수 범위에 넣을지는 기술 조사가 아니라 제품 범위 결정이다. | 범위 미정 |

근거: [VRChat Chatbox OSC](https://docs.vrchat.com/docs/osc-as-input-controller), [VRChat OSC 개요](https://docs.vrchat.com/docs/osc-overview), [OSC 1.0 명세](https://opensoundcontrol.stanford.edu/spec-1_0.html), [OpenVR 오버레이 개요](https://github.com/ValveSoftware/openvr/wiki/IVROverlay_Overview), [Valve Qt 오버레이 예제](https://github.com/ValveSoftware/openvr/blob/master/samples/helloworldoverlay/openvroverlaycontroller.cpp), [Qt Windows 입력 컨텍스트 소스](https://github.com/qt/qtbase/blob/dev/src/plugins/platforms/windows/qwindowsinputcontext.cpp), [Microsoft 일본어 IME](https://support.microsoft.com/en-us/windows/hardware/input-devices/microsoft-japanese-ime), [Microsoft 간체 중국어 IME](https://support.microsoft.com/en-us/windows/hardware/input-devices/microsoft-simplified-chinese-ime), [Microsoft 번체 중국어 IME](https://support.microsoft.com/en-us/windows/hardware/input-devices/microsoft-traditional-chinese-ime), [Windows 키보드 입력 개요](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input), [TSF UI-less 모드](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview).

## 입력과 후보 UI의 경계

OpenVR 헤더의 `SetFocusOverlay` 관련 이벤트는 오버레이의 **gamepad focus**로 설명된다. 이를 Windows `HWND`의 키보드 포커스로 취급할 근거는 없다. Windows의 `SendInput`을 선택해도 시스템 키 입력은 현재 포커스 창으로 가고, `SetForegroundWindow`는 호출 조건과 거부 가능성이 있다. 앱 안에서 IME 입력 대상으로 사용할 문서와 포커스를 명시적으로 관리할 경로가 필요한 이유다. [OpenVR 헤더](https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h), [Windows 키보드 입력 개요](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input), [Windows 전경 창 API](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow), [TSF 포커스 API](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfthreadmgr-setfocus)

OpenVR이 표시하는 것은 앱이 제공한 텍스처다. Windows IME가 별도 데스크톱 창으로 띄운 후보 UI가 그 텍스처에 자동으로 포함된다는 근거는 없다. 따라서 후보 목록을 HMD에서 보여주려면 앱이 후보 **데이터**를 받아 텍스처 안에 다시 그려야 한다. 이는 OpenVR 텍스처 모델과 TSF UI-less 설계에서 도출한 구조적 판단이다. [OpenVR 오버레이 개요](https://github.com/ValveSoftware/openvr/wiki/IVROverlay_Overview), [TSF UI-less 모드](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview)

TSF UI-less 모드의 공식 절차는 `ITfThreadMgrEx::ActivateEx(TF_TMAE_UIELEMENTENABLEDONLY)`로 스레드를 활성화하고 `ITfUIElementSink`를 등록하는 것이다. 후보 UI를 직접 그릴 때 `BeginUIElement`에서 원래 UI 표시를 막고, 첫 `UpdateUIElement`부터 데이터를 읽는다. 입력기가 UI-less와 UI element 기능을 지원하지 않으면 이 방식으로 후보 목록을 얻을 수 없다. 중국어 입력기의 별도 읽기 정보는 `ITfReadingInformationUIElement`가 담당할 수 있다. [Microsoft TSF UI-less 모드](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview), [후보 목록 인터페이스](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfcandidatelistuielement), [후보 선택 인터페이스](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfcandidatelistuielementbehavior)

Microsoft의 DXUT 게임 IME 예제에는 TSF UI-less sink와 후보 문자열을 만드는 코드가 있다. 다만 예제는 일부 후보 처리를 IMM API로 수행하고, 오래된 입력기 분기도 포함한다. 현재 Windows 11의 기본 입력기 전체를 보증하는 근거로 쓰지 않는다. [Microsoft DXUT IME 예제](https://github.com/microsoft/DXUT/blob/main/Optional/ImeUi.cpp)

현재 Microsoft Pinyin에서 TSF UI element를 얻지 못했다는 사용자 보고도 있다. 공식 호환성 보증이나 재현 결과가 아니므로 실패가 확정됐다고 보지는 않는다. 다만 **현행 Pinyin을 확인 전 통과 처리할 수 없는 추가 근거**다. Microsoft는 일본어 및 Pinyin 입력기에 이전 버전 사용 설정을 제공하지만, 이를 장기 해결책으로 권하지 않는다. [Pinyin 호환성 보고](https://learn.microsoft.com/en-us/answers/questions/64354/pinyin-tsf-ime-compatibility-mode-changes-to-itfui), [Microsoft 이전 IME 버전 안내](https://support.microsoft.com/en-us/windows/hardware/input-devices/revert-to-a-previous-version-of-an-input-method-editor-ime)

## 구현 경로 비교

| 경로 | 장점 | 지금 확인되지 않은 점 |
| --- | --- | --- |
| **A. Qt 입력란 + Windows IME + 별도 TSF 후보 sink** | Qt가 조합 문자열·입력란 편집을 이미 처리한다. Valve의 Qt 오버레이 예제가 있다. | Qt의 IMM 메시지 처리와 TSF UI-less sink가 같은 IME 후보를 공유하는지, 컨트롤러 가상 키가 Qt 입력란의 IME에 들어가는지 |
| **B. 앱 자체 TSF 텍스트 저장소 + UI-less 후보 sink + Qt는 화면만 그림** | 텍스트, 조합, 후보 UI를 한 TSF 문서에 연결할 수 있는 공식 인터페이스가 있다. `ITfKeystrokeMgr`로 키를 전달할 경로도 있다. | `ITextStoreACP`, 잠금, 포커스, 키 상태와 편집 동작을 직접 구현해야 한다. SteamVR 조작 중의 실제 IME 동작은 여전히 확인해야 한다. |
| **C. Qt Virtual Keyboard의 입력 엔진** | Qt 문서는 한국어·일본어·중국어 입력과 후보 UI를 지원한다고 안내한다. | Windows 기본 IME를 직접 사용하는 요구와 다르다. Qt Virtual Keyboard는 GPLv3 또는 상용 라이선스라 배포 조건 결정이 필요하다. |

현재로서는 **A를 확정 기술 스택으로 승인할 근거가 부족하다**. Windows 환경에서 A의 후보 수신과 가상 키 전달이 통과하면 가장 작은 구성이다. 실패하면 B를 검토한다. B도 특정 입력기가 UI-less 후보를 제공하지 않으면 요구사항을 충족하지 못한다. C는 Windows IME 직접 연동이라는 선택을 바꿀 때만 대안이 된다. [Qt Windows 입력 컨텍스트 소스](https://github.com/qt/qtbase/blob/dev/src/plugins/platforms/windows/qwindowsinputcontext.cpp), [TSF 텍스트 저장소](https://learn.microsoft.com/en-us/windows/win32/tsf/text-stores), [TSF 키 전달](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfkeystrokemgr-keydown), [Qt Virtual Keyboard 개요](https://doc.qt.io/qt-6/qtvirtualkeyboard-overview.html), [Qt 라이선스](https://doc.qt.io/qt-6/licensing.html)

## Windows 실기기에서 필요한 판정

조사로 끝낼 수 없는 항목이다. 대상 Windows 버전과 설치된 IME 버전을 기록한 다음, 각 항목의 **관찰 결과**가 있어야 통과로 표시할 수 있다. 현재는 모두 미실행이다.

| 판정 | 통과 조건 | 실패 시 의미 |
| --- | --- | --- |
| 1. OSC | VRChat에서 OSC를 켜고 `/chatbox/input` `send=false`로 영문과 `한글 日本語 中文`을 각각 보냈을 때 Chatbox 키보드가 열리며 문자열이 손실 없이 채워진다. | OSC 설정·주소·다국어 인코딩·VRChat 버전을 먼저 확인 |
| 2. 오버레이 이벤트 | HMD에서 키를 조준해 누를 때 앱이 해당 키의 클릭 이벤트를 받는다. | OpenVR 입력·오버레이 설정 수정 |
| 3. 입력 포커스와 가상 키 | VRChat 실행 중 오버레이 키를 눌러 입력란에 영문, Shift, Space, Backspace가 실제 키 동작과 같이 적용된다. Windows 전경 창과 TSF 포커스를 함께 기록한다. | 입력 전달 경로 변경 필요 |
| 4. 한글 조합 | 키 세 번으로 입력란에 `ㄱ → 가 → 감` 상태가 순서대로 보이고, 조합 중 Backspace가 상태를 되돌린다. | 현재 입력란 또는 키 전달 경로로 한국어 요구 충족 불가 |
| 5. 일본어 후보 | 로마자 입력 후 가나 조합·한자 변환을 실행했을 때 후보 문자열, 선택, 페이지가 앱으로 들어오고 HMD에 표시되며 클릭으로 확정된다. | 현행 일본어 IME와 후보 API 또는 선택 경로 재검토 |
| 6. 간체 중국어 후보 | Pinyin 입력 후 동일한 후보 표시·선택·확정이 된다. | 현행 Pinyin의 UI-less 또는 IMM 호환성 재검토 |
| 7. 번체 중국어 후보 | 번체까지 필수 범위라면 Bopomofo의 HanYu Pinyin 설정에서 동일하게 확인한다. | 번체 지원 경로 재검토 |
| 8. 전체 왕복 | 한글·일본어·중국어가 섞인 확정 문자열을 OSC로 보내 VRChat 입력란에서 손실 없이 확인한다. | Unicode 전송·VRChat Chatbox 제한 점검 |

실측 환경은 Windows 11을 우선 권장한다. Windows 10까지 지원하려면 별도 환경에서 같은 판정을 반복해야 한다. Microsoft는 현재 Windows 10 지원 종료를 안내하고 있다. 이는 지원 범위 제안이며, 사용자 OS를 확인한 결과는 아니다. [Microsoft 이전 IME 버전 안내](https://support.microsoft.com/en-us/windows/hardware/input-devices/revert-to-a-previous-version-of-an-input-method-editor-ime)

## 현재 알 수 없는 것

- 현재 사용자의 Windows 버전, SteamVR 버전, 기본 입력기 버전, 중국어 간체·번체 필수 여부는 알 수 없다.
- OVR Toolkit의 입력이 VRChat 기본 Chatbox에서 실패하는 정확한 원인은 재현 자료가 없어 알 수 없다.
- 현행 Microsoft 일본어·Pinyin·Bopomofo IME가 이 앱의 TSF UI-less 스레드에서 후보 및 행동 인터페이스를 모두 제공하는지는 알 수 없다.
- Qt 입력란과 TSF UI-less 후보 sink를 함께 썼을 때 중복 조합, 후보 팝업 잔존, 포커스 손실이 없는지는 알 수 없다.

이 항목은 실제 Windows 관찰이나 제품 범위 결정이 있어야 닫을 수 있다.
