# VRChat용 SteamVR 오버레이 키보드 기획

상태: 기획 초안. 공식 API 및 공개 소스 조사는 [기술 조사](TECHNICAL_FEASIBILITY.md)에 정리했다. Windows, SteamVR, VRChat에서의 실제 동작은 아직 검증하지 않았다.

## 목표와 범위

Windows PC에서 SteamVR 오버레이 키보드를 띄우고, 사용자가 오버레이 안에서 문장을 작성한 뒤 VRChat 기본 Chatbox의 입력란에 채운다. 기존 오버레이 키보드가 Windows에서는 동작해도 VRChat 안에서는 원하는 대로 입력되지 않는 불편을 해결하는 것이 목적이다. 기존 도구가 실패한 정확한 원인은 확인되지 않았다.

대상은 **VRChat 기본 Chatbox**다. 다른 VRChat 텍스트 화면을 같은 방식으로 제어할 수 있는지는 확인되지 않았다. VRChat OSC는 사용자가 켜야 한다. 공식 문서 기준 활성화 경로는 Action Menu의 `OSC > Enabled`이고, VRChat의 기본 OSC 수신 포트는 `9000`이다. [VRChat OSC 개요](https://docs.vrchat.com/docs/osc-overview)

## 사용자 흐름

1. SteamVR에서 오버레이 키보드를 연다.
2. 컨트롤러로 키를 누르면 오버레이의 입력란에 입력 과정과 결과가 표시된다.
3. 한글 조합, 일본어 로마자 변환, 중국어 병음 변환을 Windows IME로 처리한다. 변환 후보는 헤드셋에서 보고 선택할 수 있어야 한다.
4. 사용자가 `VRChat 입력란 채우기`를 누르면 완성된 문장을 OSC `/chatbox/input`에 `send=false`로 보낸다.
5. VRChat Chatbox 키보드가 열리고 문장이 채워진다. 사용자는 VRChat에서 내용을 확인하고 최종 전송한다.

VRChat 문서는 `/chatbox/input`의 `send=false`가 키보드를 열어 문장을 채우고, `send=true`가 키보드를 거치지 않고 바로 전송한다고 설명한다. 따라서 현재 흐름의 버튼은 즉시 게시가 아닌 **입력란 채우기**로 정의한다. [VRChat Chatbox OSC 입력](https://docs.vrchat.com/docs/osc-as-input-controller)

## 필수 입력 동작

| 언어 | 필요한 동작 |
| --- | --- |
| 영어 | 일반 키보드처럼 글자, 숫자, 기호, Shift, Space, Backspace를 입력하고 편집한다. |
| 한국어 | 자모를 누를 때 조합 중인 글자를 입력란에 표시한다. 예를 들어 `ㄱ → 가 → 감`의 각 상태를 볼 수 있어야 한다. |
| 일본어 | 로마자로 입력하고, 변환 후보에서 가나 또는 한자를 선택해 확정한다. |
| 중국어 | 병음으로 입력하고, 변환 후보에서 한자를 선택해 확정한다. 간체와 번체의 정확한 지원 범위는 아직 정하지 않았다. |

오버레이의 각 키는 단순히 완성 문자를 입력란에 덧붙이는 버튼이 아니라 **선택된 IME가 처리할 키 입력**으로 동작해야 한다. 그래야 조합, 변환, 후보 선택, 조합 중 Backspace가 일반 키보드 입력과 같은 흐름을 따른다. 이 방식이 SteamVR 오버레이에서 실제로 작동하는지는 구현 전 검증이 필요하다.

여기서 키보드와 동일한 입력 동작은 **오버레이 입력란에서의 조합과 변환**을 뜻한다. VRChat Chatbox로 이동할 때는 완성된 문자열을 OSC로 전달한다.

변환 후보는 오버레이를 사용하는 동안 헤드셋에서 읽고 선택할 수 있어야 한다. OpenVR 오버레이는 앱이 제공한 화면 텍스처를 표시하므로, Windows가 별도 창으로 그리는 기본 IME 후보창에 의존하지 않는다. 앱이 후보 데이터를 받아 오버레이 화면 안에 직접 그린다.

## 제안 기술 구성

| 구성 요소 | 역할 | 현재 선택 |
| --- | --- | --- |
| C++와 Qt | 입력란, 가상 키, 입력 상태 표시 | Qt 화면은 우선 검토하되 Qt 입력란과 TSF의 결합은 미확인 |
| Windows IME와 TSF | 한글 조합, 일본어와 중국어 변환 및 후보 데이터 제공 | Windows 입력기를 사용하려는 후보 경로. 현행 입력기별 UI-less 후보 지원은 미확인 |
| OpenVR `IVROverlay` | SteamVR 화면 표시와 컨트롤러 포인터 이벤트 | Valve 공식 오버레이 API 사용 |
| OSC 클라이언트 | 완성된 문장을 VRChat에 전달 | `/chatbox/input`으로 전송 |

Valve의 `IVROverlay`는 2D 이미지를 VR 화면 위에 표시하고 오버레이 입력 이벤트를 받는다. 공식 `helloworldoverlay` 예제는 Qt를 이용해 오버레이 화면과 입력을 다룬다. Qt의 입력 메서드 이벤트는 조합 중 문자열과 확정 문자열을 구분한다. [OpenVR 오버레이 개요](https://github.com/ValveSoftware/openvr/wiki/IVROverlay_Overview), [Valve 오버레이 예제](https://github.com/ValveSoftware/openvr/tree/master/samples/helloworldoverlay), [Qt 입력 메서드 이벤트](https://doc.qt.io/qt-6/qinputmethodevent.html)

오버레이가 Windows 텍스트 입력 포커스를 얻는 방법, 컨트롤러로 누른 가상 키가 Windows IME에 전달되는 방법은 아직 확인되지 않았다. 일반 Windows 창에서 동작하는 입력란을 OpenVR 오버레이에 표시하는 것만으로 두 기능이 자동 연결된다고 가정하지 않는다.

## HMD 안의 변환 후보 표시

Windows TSF의 UI-less mode는 앱이 IME의 후보 UI를 대신 그리도록 설계되어 있다. 앱은 `ITfUIElementSink`의 시작, 갱신, 종료 알림을 받고, 해당 UI 요소에서 `ITfCandidateListUIElement`를 얻는다. 이 인터페이스는 후보 문자열, 현재 선택 항목, 페이지 정보를 제공한다. 앱은 그 데이터를 오버레이의 후보 목록에 반영한다. Microsoft의 게임 IME 예제도 TSF 후보 데이터를 받아 게임 화면에 그리는 구현을 포함한다. [Microsoft TSF UI-less mode](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview), [TSF 후보 목록 인터페이스](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfcandidatelistuielement), [Microsoft DXUT IME 예제](https://github.com/microsoft/DXUT/blob/main/Optional/ImeUi.cpp)

컨트롤러로 후보를 선택할 때는 활성 IME가 `ITfCandidateListUIElementBehavior`를 제공하면 선택 변경과 확정 API를 사용할 수 있다. 이 인터페이스의 제공 여부와 실제 동작은 사용할 IME에서 확인해야 한다. 중국어 입력기의 읽기 정보가 조합 문자열과 별도로 제공되는 경우에는 `ITfReadingInformationUIElement`도 표시 대상에 포함한다. [TSF 후보 선택 인터페이스](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfcandidatelistuielementbehavior), [Microsoft TSF UI-less mode](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview)

앱이 이 방식으로 IME 후보 데이터를 받으려면 해당 입력기가 TSF UI-less mode를 지원해야 한다. 모든 Windows 입력기가 이 기능을 제공한다고 단정할 수 없고, 현재 Microsoft 일본어 및 중국어 IME의 호환성은 아직 확인되지 않았다. 특히 Qt의 Windows 입력 처리는 IMM 계열 API를 사용하므로 Qt 입력란과 별도 TSF 후보 수신 코드가 함께 동작하는지도 확인해야 한다. [Qt Windows 입력 컨텍스트 소스](https://github.com/qt/qtbase/blob/dev/src/plugins/platforms/windows/qwindowsinputcontext.cpp)

현재 확인이 필요한 항목은 다음과 같다.

- SteamVR 오버레이를 조작하는 동안 오버레이 앱의 입력란이 Windows IME 포커스를 유지하는가?
- 가상 키 입력으로 한글 조합 과정 전체와 일본어·중국어 변환이 동작하는가?
- 사용할 일본어 및 중국어 IME가 TSF UI-less mode에서 후보 목록과 선택 동작을 제공하는가?
- Qt 입력란과 별도 TSF 후보 수신 코드를 같은 입력 흐름으로 연결할 수 있는가? 필요하면 앱이 TSF 텍스트 저장소를 직접 구현해야 하는가?
- 중국어 간체와 번체 중 어느 범위까지 필수로 지원할 것인가?

각 항목의 공식 근거, 현재 판정과 실기기 확인 조건은 [기술 조사](TECHNICAL_FEASIBILITY.md)에 있다. 문서 조사만으로 미확인 항목을 통과 처리하지 않는다.

## 실기기 확인과 구현 순서

1. **기술 경로 판정:** Windows와 HMD에서 [기술 조사의 실기기 판정 항목](TECHNICAL_FEASIBILITY.md#windows-실기기에서-필요한-판정)을 확인한다. 이를 위해 기능별 최소 확인 프로그램은 필요하다. 이 확인이 끝나기 전에는 Qt와 TSF의 결합 방식을 확정하지 않는다.
2. **입력부 구현:** 통과한 경로로 영어 키 입력, 한글 조합, 일본어 및 중국어 후보 표시·선택을 오버레이에 구현한다.
3. **VRChat 연결:** 확정한 문장을 OSC로 Chatbox 입력란에 채우고 전체 흐름을 확인한다.

VRChat 공식 문서의 Chatbox 제한은 최대 144자와 최대 9줄이다. 줄 수에는 직접 입력한 줄바꿈과 자동 줄바꿈이 포함된다. 입력란과 전송 동작에 이 제한을 반영해야 한다. [VRChat Chatbox OSC 입력](https://docs.vrchat.com/docs/osc-as-input-controller)

## 라이선스 메모

현재 저장소의 `LICENSE`는 MIT다. Qt Virtual Keyboard 모듈은 공식 문서에서 GPLv3 또는 상용 라이선스로 안내한다. 현재 제안은 Windows IME 사용을 우선하며, 실제 배포에 사용할 Qt 구성 요소와 다른 의존성의 라이선스는 구현 시 확인한다. [Qt 라이선스 안내](https://doc.qt.io/qt-6/licensing.html)
