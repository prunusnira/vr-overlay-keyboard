# SteamVR 오버레이 키보드 기술 조사

기술 조사일: 2026-09-30. OpenVR 헤더 재확인 및 아키텍처 반영: 2026-10-01. 이 문서는 공식 API 문서와 공개 소스에서 확인한 내용을 정리한다. 프로토타입 구현 후 사용자가 확인한 Windows·SteamVR·VRChat 결과는 [프로토타입 README의 실행 기록](../prototype/windows-ime-overlay/README.md)에 별도로 남긴다. 모듈 책임과 의존성 방향은 [모듈 아키텍처](ARCHITECTURE.md)를 참고한다. 아래 문서의 **확인**은 API와 문서 수준의 확인을 뜻하며, 제품 동작의 통과 판정은 실측 기록에 한정한다.

## 결론

VRChat Chatbox OSC 입력과 SteamVR 컨트롤러 입력은 프로토타입의 **대시보드 오버레이 경로**에서 사용자가 동작을 확인했다. 이 결과는 대시보드를 열지 않는 일반 오버레이의 표시 커맨드나 컨트롤러 입력을 검증한 것은 아니다. 일본어 IME 모드 전환도 확인했지만, **Windows IME 기반 4개 언어 입력과 후보 선택 전체는 검증되지 않았다**. 한국어 가상 키 입력에서 자모별 커밋 문제가 남아 있다. 중국어 입력과 현재 Microsoft 일본어·중국어 입력기의 후보 인터페이스 지원도 확인되지 않았다.

따라서 착수 조건은 “모든 API가 존재한다”가 아니라 아래의 Windows 실기기 확인 항목을 통과하는 것이다. 조사만으로 해결할 수 없는 항목을 통과한 것으로 표시하지 않는다.

## 항목별 조사 결과

| 항목 | 문서 조사 결과 | 상태 |
| --- | --- | --- |
| VRChat 기본 Chatbox 입력란 채우기 | `/chatbox/input`에 문자열과 `false`를 보내면 VRChat 키보드를 열고 문자열을 채운다. `true`는 즉시 전송한다. 기본 수신 포트는 9000이며 사용자가 OSC를 켜야 한다. | API 확인, 사용자가 프로토타입의 기본 OSC 입력 성공을 확인함; 다국어 왕복은 미기록 |
| Chatbox 글자·줄 제한 | 공식 문서는 최대 144자, 표시 최대 9줄을 명시한다. 줄 수에는 자동 줄바꿈도 포함한다. | 문서 확인 |
| 한글·일본어·중국어 OSC 문자열 | VRChat 문서는 Chatbox 인수를 문자열로 정의하지만, 네 언어별 인코딩과 144자 계산 단위는 명시하지 않는다. OSC 1.0 원문은 기본 문자열을 ASCII로 정의한다. 다국어 문자열의 실제 왕복은 별도 확인이 필요하다. | **확실하지 않음**, VRChat 실측 필수 |
| 대시보드 오버레이 화면·클릭 | 프로토타입은 `CreateDashboardOverlay`와 `ShowDashboard`로 탭을 만들고 표시한다. Valve 예제는 대시보드 컨트롤러 포인터 이벤트를 Qt 화면에 전달한다. | API·예제 확인, 사용자가 프로토타입 동작 확인 |
| 일반 오버레이 표시·숨김·위치 | `CreateOverlay`는 대시보드 탭이 아닌 오버레이를 만들고, `ShowOverlay`·`HideOverlay`로 표시 상태를 제어한다. 추적 장치 기준 위치를 설정할 수 있다. | API 확인, 대시보드 없는 실제 표시와 HMD 위치는 미검증 |
| SteamVR Input 토글·포인터 액션 | 앱은 manifest에 `ToggleKeyboard`, 컨트롤러 `ControllerPose`, `PointerClick` 액션을 정의해 바인딩할 수 있다. 현재 앱 초안은 이 세 액션을 읽는다. | 현재 헤더·문서 확인, 대상 컨트롤러 기본 바인딩과 대시보드가 닫힌 상태에서의 수신은 미검증 |
| 일반 오버레이 컨트롤러 포인터 | 현재 Valve OpenVR 헤더에는 `ComputeOverlayIntersection`이 선언되어 있다. 앱은 SteamVR Input의 컨트롤러 자세·클릭 액션을 받아 광선과 일반 오버레이의 교차점·UV를 UI 좌표로 바꾼다. 오래된 위키에 남은 `HandleControllerOverlayInteractionAsMouse` 예제 대신 현재 헤더의 API를 기준으로 구현한다. | 현재 헤더·API 확인, 대상 컨트롤러 바인딩·HMD 포인터 동작·게임 입력 영향은 미검증 |
| Windows IME 조합 과정 표시 | Qt Windows 입력 컨텍스트 소스는 `WM_IME_COMPOSITION`을 받아 조합 중 문자열과 확정 문자열을 Qt 입력 객체에 전달한다. 한국어 처리 분기도 있다. | Qt 소스 확인, 가상 키 조합 확인 필요 |
| 일본어·중국어 변환 | Microsoft의 일본어 IME, 간체 중국어 Pinyin IME는 로마자·병음 변환과 후보 선택을 지원한다. 번체 중국어는 Microsoft Bopomofo의 HanYu Pinyin 배열도 있다. | 일반 Windows 입력 기능 확인, 이 앱과의 연동 확인 필요 |
| HMD 안에 후보 목록 그리기 | TSF UI-less 모드에서 `ITfUIElementSink`로 후보 UI 갱신을 받고 `ITfCandidateListUIElement`로 후보 문자열·선택·페이지를 읽을 수 있다. 앱이 이 데이터를 OpenVR 텍스처에 그려야 한다. | API 확인, 대상 IME별 동작 확인 필요 |
| 후보 클릭으로 선택·확정 | 입력기가 `ITfCandidateListUIElementBehavior`를 제공하면 `SetSelection`, `Finalize`, `Abort`가 있다. 인터페이스 제공 여부는 입력기별로 다를 수 있다. 대체 경로인 숫자·방향키·Space 입력도 가상 키 전달에 성공해야 쓸 수 있다. | API 확인, 대상 IME별 동작 확인 필요 |
| 오버레이 조작과 Windows 입력 포커스 | OpenVR은 오버레이 자체의 포커스·마우스 이벤트를 정의한다. 이것이 앱의 Win32 키보드 포커스를 자동으로 만든다는 문서는 찾지 못했다. Windows의 키보드 이벤트는 포커스가 있는 전경 스레드로 전달되며, 전경 창 변경도 제한된다. | **확실하지 않음**, Windows 실측 필수 |
| 컨트롤러 포인터 클릭을 UI로 전달 | 대시보드 경로의 프로토타입은 컨트롤러 포인터 이벤트를 Qt UI로 전달한다. 일반 오버레이 입력은 별도 확인이 필요하다. | 대시보드 경로는 사용자 확인, 일반 오버레이 경로는 미검증 |
| UI 가상 키를 IME 입력으로 전달 | 프로토타입은 앱이 Windows 전경 프로세스일 때 `SendInput`으로 키 이벤트를 넣는다. TSF의 `ITfKeystrokeMgr::KeyDown`은 텍스트 서비스에 키 이벤트를 전달하는 별도 API다. | 프로토타입 한국어 입력은 자모별 커밋으로 미통과; 대안 경로 미검증 |
| Qt 입력란과 TSF UI-less 후보 수신의 공존 | Qt의 Windows 입력 컨텍스트는 공개 소스 기준 IMM 계열 API와 `WM_IME_*` 메시지로 조합을 처리한다. 별도 TSF UI-less 활성화 및 sink를 붙여 후보 데이터가 같은 입력 흐름에서 오는지는 확인되지 않았다. | **확실하지 않음**, Windows 실측 필수 |
| 중국어 간체·번체 범위 | Windows는 둘 모두에 병음 경로가 있지만 입력기가 다르다. 두 종류를 제품 필수 범위에 넣을지는 기술 조사가 아니라 제품 범위 결정이다. | 범위 미정 |

근거: [VRChat Chatbox OSC](https://docs.vrchat.com/docs/osc-as-input-controller), [VRChat OSC 개요](https://docs.vrchat.com/docs/osc-overview), [OSC 1.0 명세](https://opensoundcontrol.stanford.edu/spec-1_0.html), [OpenVR 오버레이 개요](https://github.com/ValveSoftware/openvr/wiki/IVROverlay_Overview), [일반 오버레이 생성](https://github.com/ValveSoftware/openvr/wiki/IVROverlay%3A%3ACreateOverlay), [일반 오버레이 표시·숨김](https://github.com/ValveSoftware/openvr/wiki/IVROverlay%3A%3AShowOverlay), [추적 장치 기준 오버레이 위치](https://github.com/ValveSoftware/openvr/wiki/IVROverlay%3A%3ASetOverlayTransformTrackedDeviceRelative), [광선 교차 계산](https://github.com/ValveSoftware/openvr/wiki/IVROverlay%3A%3AComputeOverlayIntersection), [현재 Valve OpenVR 헤더](https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h), [SteamVR Input](https://github.com/ValveSoftware/openvr/wiki/SteamVR-Input), [SteamVR 액션 manifest](https://github.com/ValveSoftware/openvr/wiki/Action-manifest), [Valve Qt 오버레이 예제](https://github.com/ValveSoftware/openvr/blob/master/samples/helloworldoverlay/openvroverlaycontroller.cpp), [Qt Windows 입력 컨텍스트 소스](https://github.com/qt/qtbase/blob/dev/src/plugins/platforms/windows/qwindowsinputcontext.cpp), [Microsoft 일본어 IME](https://support.microsoft.com/en-us/windows/hardware/input-devices/microsoft-japanese-ime), [Microsoft 간체 중국어 IME](https://support.microsoft.com/en-us/windows/hardware/input-devices/microsoft-simplified-chinese-ime), [Microsoft 번체 중국어 IME](https://support.microsoft.com/en-us/windows/hardware/input-devices/microsoft-traditional-chinese-ime), [Windows 키보드 입력 개요](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input), [TSF UI-less 모드](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview).

## 입력과 후보 UI의 경계

OpenVR 헤더의 `SetFocusOverlay` 관련 이벤트는 오버레이의 **gamepad focus**로 설명된다. 이를 Windows `HWND`의 키보드 포커스로 취급할 근거는 없다. Windows의 `SendInput`을 선택해도 시스템 키 입력은 현재 포커스 창으로 가고, `SetForegroundWindow`는 호출 조건과 거부 가능성이 있다. 앱 안에서 IME 입력 대상으로 사용할 문서와 포커스를 명시적으로 관리할 경로가 필요한 이유다. [OpenVR 헤더](https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h), [Windows 키보드 입력 개요](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input), [Windows 전경 창 API](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow), [TSF 포커스 API](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfthreadmgr-setfocus)

컨트롤러의 자세·클릭 액션과 `ComputeOverlayIntersection`은 일반 오버레이 안의 UI 좌표·클릭을 만드는 경로다. 키보드 UI가 그 클릭을 가상 키로 바꾸고 Windows IME에 전달하는 일은 별도의 키 입력 경로다. 따라서 일반 오버레이 포인터 이벤트가 확인되어도 IME 키 전달이 확인된 것은 아니다.

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

조사로 끝낼 수 없는 항목이다. 일부 사용자 실측은 프로토타입 README에 기록했다. 확인된 대시보드 내 컨트롤러 포인터, 언어 전환, 일본어 모드 전환과 기본 Chatbox OSC 흐름은 새 앱에서 이어받을 동작 기준이다. 아래 새 실행 경로의 통과 여부는 `app/`에서 확인한다. 특히 프로토타입 기록은 대시보드 없는 토글, 일반 오버레이 포인터, 한국어 조합, TSF 후보 선택, 중국어 입력이나 다국어 OSC 왕복을 입증하지 않는다.

| 판정 | 통과 조건 | 실패 시 의미 |
| --- | --- | --- |
| 1. OSC | VRChat에서 OSC를 켜고 `/chatbox/input` `send=false`로 영문과 `한글 日本語 中文`을 각각 보냈을 때 Chatbox 키보드가 열리며 문자열이 손실 없이 채워진다. | OSC 설정·주소·다국어 인코딩·VRChat 버전을 먼저 확인 |
| 2. 일반 오버레이 토글 | 앱과 SteamVR을 실행하고 대시보드를 닫는다. VRChat이 실행 중인 상태에서 바인딩한 SteamVR Input 액션을 실행하면 일반 오버레이가 나타나고, 다시 실행하면 숨는다. 두 동작 중 대시보드가 열리지 않는다. | 액션 수신과 OpenVR 표시 상태를 각각 기록해 문제 경계를 구분 |
| 3. HMD 기준 위치 | 토글로 표시한 오버레이가 정한 HMD 기준 위치에 나타나고, HMD 움직임에 맞춰 설정한 상대 위치를 유지한다. | 추적 장치 인덱스 또는 오버레이 변환 설정 확인 |
| 4. 일반 오버레이 포인터 | 대시보드가 닫힌 상태에서 바인딩한 컨트롤러 자세·클릭 액션으로 UI를 가리키고 클릭하면 광선 교차 좌표와 버튼 이벤트가 UI에 전달된다. VRChat 장면 입력에 미치는 영향도 기록한다. | SteamVR 바인딩, 광선 좌표 변환 또는 게임 입력 상호작용 재검토 |
| 5. 포커스와 가상 키 | VRChat이 실행 중일 때 오버레이 키를 눌러 입력란에 영문, Shift, Space, Backspace가 의도한 대로 적용된다. Windows 전경 창, 편집기 포커스, TSF 상태를 함께 기록한다. | 포인터 이벤트와 별개로 Windows 입력 전달 경로 변경 필요 |
| 6. 한글 조합 | 키 세 번으로 입력란에 `ㄱ → 가 → 감` 상태가 순서대로 보이고, 조합 중 Backspace가 상태를 되돌린다. | 현재 입력란 또는 키 전달 경로로 한국어 요구 충족 불가 |
| 7. 일본어 후보 | 로마자 입력 후 가나 조합·한자 변환을 실행했을 때 후보 문자열, 선택, 페이지가 앱으로 들어오고 HMD에 표시되며 클릭으로 확정된다. | 현행 일본어 IME와 후보 API 또는 선택 경로 재검토 |
| 8. 간체 중국어 후보 | Pinyin 입력 후 동일한 후보 표시·선택·확정이 된다. | 현행 Pinyin의 UI-less 또는 IMM 호환성 재검토 |
| 9. 번체 중국어 후보 | 번체까지 필수 범위라면 Bopomofo의 HanYu Pinyin 설정에서 동일하게 확인한다. | 번체 지원 경로 재검토 |
| 10. 전체 왕복 | 한글·일본어·중국어가 섞인 확정 문자열을 OSC로 보내 VRChat 입력란에서 손실 없이 확인한다. | Unicode 전송·VRChat Chatbox 제한 점검 |

실측 환경은 Windows 11을 우선 권장한다. Windows 10까지 지원하려면 별도 환경에서 같은 판정을 반복해야 한다. Microsoft는 현재 Windows 10 지원 종료를 안내하고 있다. 이는 지원 범위 제안이며, 사용자 OS를 확인한 결과는 아니다. [Microsoft 이전 IME 버전 안내](https://support.microsoft.com/en-us/windows/hardware/input-devices/revert-to-a-previous-version-of-an-input-method-editor-ime)

## 현재 알 수 없는 것

- 현재 사용자의 Windows 버전, SteamVR 버전, 기본 입력기 버전, 중국어 간체·번체 필수 여부는 알 수 없다.
- SteamVR Input 액션이 대시보드가 닫히고 VRChat이 장면 앱으로 실행 중일 때 오버레이 앱에 도착하는지는 알 수 없다.
- 일반 오버레이의 컨트롤러 포인터가 목표 컨트롤러에서 동작하는지, 표시 중 VRChat 입력에 어떤 영향을 주는지는 알 수 없다.
- OVR Toolkit의 입력이 VRChat 기본 Chatbox에서 실패하는 정확한 원인은 재현 자료가 없어 알 수 없다.
- 현행 Microsoft 일본어·Pinyin·Bopomofo IME가 이 앱의 TSF UI-less 스레드에서 후보 및 행동 인터페이스를 모두 제공하는지는 알 수 없다.
- Qt 입력란과 TSF UI-less 후보 sink를 함께 썼을 때 중복 조합, 후보 팝업 잔존, 포커스 손실이 없는지는 알 수 없다.

이 항목은 실제 Windows 관찰이나 제품 범위 결정이 있어야 닫을 수 있다.
