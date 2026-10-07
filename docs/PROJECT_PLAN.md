# VRChat용 SteamVR 오버레이 키보드 기획

상태: 프로토타입의 확인된 흐름을 이어받은 Dear ImGui 기반 Windows 앱을 `app/`에 구성했고 Windows x64 Release 빌드가 성공했다. 현재 소스는 Win32 창·OpenGL3 렌더러, 일반 OpenVR 오버레이, SteamVR Input, Windows 언어·TSF 어댑터와 Chatbox OSC를 연결한다. HMD 동작은 확인이 필요하다. 프로토타입 코드는 제품 앱으로 옮기거나 수정하지 않는다. 기존 확인 흐름은 회귀 기준으로 삼고, 대시보드 없는 표시·숨김과 미해결 입력 경로를 추가로 검증한다. 한국어 가상 키 입력은 자모별로 커밋되는 문제가 남아 있고, TSF 후보 선택·중국어 입력·다국어 OSC 왕복은 확인되지 않았다. 상세 결과는 [프로토타입 README](../prototype/windows-ime-overlay/README.md)에 기록했다.

## 목표와 범위

Windows PC에서 SteamVR 오버레이 키보드를 띄우고, 사용자가 오버레이 안에서 문장을 작성한 뒤 VRChat 기본 Chatbox의 입력란에 채운다. 기존 오버레이 키보드가 Windows에서는 동작해도 VRChat 안에서는 원하는 대로 입력되지 않는 불편을 해결하는 것이 목적이다. 기존 도구가 실패한 정확한 원인은 확인되지 않았다.

대상은 **VRChat 기본 Chatbox**다. 다른 VRChat 텍스트 화면을 같은 방식으로 제어할 수 있는지는 확인되지 않았다. VRChat OSC는 사용자가 켜야 한다. 공식 문서 기준 활성화 경로는 Action Menu의 `OSC > Enabled`이고, VRChat의 기본 OSC 수신 포트는 `9000`이다. [VRChat OSC 개요](https://docs.vrchat.com/docs/osc-overview)

## 사용자 흐름

1. Meta Quest Touch에서는 포함된 SteamVR 기본 바인딩으로 포인터 자세, 트리거 클릭, Grip 이동, Grip을 누른 동안 스틱으로 크기·거리 조정, 기본 Grip+B 소환을 사용한다. 다른 컨트롤러 유형은 SteamVR Input 설정에서 바인딩한다. 옵션에서 포인터 손을 고를 수 있다.
2. SteamVR이 실행 중일 때 기존 토글 액션은 일반 OpenVR 오버레이를 전환하고, 옵션에 지정한 버튼 조합은 숨겨진 오버레이를 표시한다. 소환 기본 조합은 오른쪽 Grip+B, 기본 유지시간은 0초다.
3. 첫 표시 때 HMD 앞에 놓인 오버레이는 방의 standing 좌표에 고정되고, 회전은 사용자를 향하도록 갱신된다. 선택한 컨트롤러의 보이는 포인터로 키를 누르고, 오버레이를 가리킨 채 Grip을 누르면 이동한다. Grip을 누른 상태에서 스틱 좌우로 크기를, 위아래로 거리를 조절하고 Grip을 놓아 위치를 고정한다.
4. 컨트롤러 포인터로 오버레이의 키를 누르면 입력란에 입력 과정과 결과가 표시된다.
5. 한글 조합, 일본어 로마자 변환, 중국어 병음 변환을 Windows IME로 처리한다. 변환 후보는 헤드셋에서 보고 선택할 수 있어야 한다.
6. 사용자가 `VRChat 입력란 채우기`를 누르면 완성된 문장을 OSC `/chatbox/input`에 `send=false`로 보낸다.
7. VRChat Chatbox 키보드가 열리고 문장이 채워진다. 사용자는 VRChat에서 내용을 확인하고 최종 전송한다.

SteamVR Input 바인딩은 최초 설정에 필요하지만, 일반 사용 중 오버레이를 표시할 때 SteamVR 대시보드를 열 필요는 없다. 전역 단축키나 외부 프로세스 명령은 추후 같은 앱 커맨드에 연결할 수 있는 확장 경로다. 모듈 경계와 상세 실행 흐름은 [모듈 아키텍처](ARCHITECTURE.md)를 참고한다.

옵션 버튼은 별도 Dear ImGui 창을 열고 같은 프레임을 사용해 데스크톱과 HMD 오버레이에 내용을 표시한다. 여기서 한국어·일본어·영어 앱 UI, 포인터를 조작할 왼손/오른손, 포인터 가로·세로 보정(-50%~+50%), 좌우 Grip·Trigger·A·B·Menu·Joystick·Trackpad 논리 버튼 중 소환 조합과 0~3초 홀드 시간을 지정한다. Windows 입력 언어 선택은 이 앱 UI 언어 설정과 분리되어 유지된다. 설정은 `%LOCALAPPDATA%\VROverlayKeyboard\settings.ini`에 저장한다. SteamVR 기본 프로필에 없는 컨트롤러는 선택한 액션을 직접 물리 버튼에 바인딩해야 한다.

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

## 현재 기술 구성

| 구성 요소 | 역할 | 현재 선택 |
| --- | --- | --- |
| C++와 Dear ImGui, Win32, OpenGL3 | 데스크톱 UI, 입력란, 가상 키, 상태 표시와 OpenVR용 프레임 생성 | 후보 영역은 고정 높이·가로 스크롤로 배치하고 오버레이 표시 중 약 60Hz로 프레임을 전달한다. IME 조합과 readback 성능은 검증 필요 |
| Windows IME와 TSF | 한글 조합, 일본어와 중국어 변환 및 후보 데이터 제공 | Windows 입력기를 사용하려는 후보 경로. 현행 입력기별 UI-less 후보 지원은 미확인 |
| SteamVR Input | 기존 토글, 컨트롤러 포인터 자세·클릭, Grip 조작용 스틱 축, 소환 조합 버튼 입력을 전달 | Meta Quest Touch 기본 프로필을 추가했다. 실제 HMD 수신은 미검증 |
| OpenVR `IVROverlay` | 대시보드와 독립적인 표시·숨김, standing 공간 고정, 사용자 방향 회전, Grip 이동·크기·거리 조정, 컨트롤러 포인터 이벤트 | 절대 위치·사용자 방향 회전·드래그·크기 및 거리 조절을 추가했다. 포인터 오프셋은 -50%~+50%에서 일정한 속도로 적용하고 패널 밖에 입력 영역을 확장한다. 새 좌표 보정의 HMD 동작은 미검증 |
| OSC 클라이언트 | 완성된 문장을 VRChat에 전달 | `/chatbox/input`으로 전송 |

Valve의 `IVROverlay`는 2D 이미지를 VR 화면 위에 표시하고 오버레이 입력 이벤트를 받는다. 현재 앱은 Dear ImGui의 Win32 입력 backend와 OpenGL3 renderer로 데스크톱 프레임을 만들고, 프레임버퍼를 RGBA 이미지로 읽어 OpenGL 텍스처로 갱신해 OpenVR에 전달한다. `SetOverlayRaw`는 갱신 사이 오버레이가 사라질 수 있어 지속 OpenGL 텍스처를 사용한다. 이 구조는 Qt 런타임에 의존하지 않지만 OpenGL readback 비용과 IME 조합 동작은 별도 확인이 필요하다. [OpenVR 오버레이 개요](https://github.com/ValveSoftware/openvr/wiki/IVROverlay_Overview), [SetOverlayRaw](https://github.com/ValveSoftware/openvr/wiki/IVROverlay%3A%3ASetOverlayRaw), [Dear ImGui backends](https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md), [Dear ImGui examples](https://github.com/ocornut/imgui/blob/master/docs/EXAMPLES.md)

일반 오버레이의 표시·숨김, 컨트롤러 포인터 입력, Windows 텍스트 입력 포커스와 IME 키 전달은 서로 다른 경로다. 일반 Windows 창에서 동작하는 입력란을 OpenVR 오버레이에 표시하는 것만으로 이 기능들이 자동 연결된다고 가정하지 않는다. 특히 현재 프로토타입의 `SendInput` 경로는 앱이 Windows 전경 프로세스여야 한다.

## HMD 안의 변환 후보 표시

Windows TSF의 UI-less mode는 앱이 IME의 후보 UI를 대신 그리도록 설계되어 있다. 앱은 `ITfUIElementSink`의 시작, 갱신, 종료 알림을 받고, 해당 UI 요소에서 `ITfCandidateListUIElement`를 얻는다. 이 인터페이스는 후보 문자열, 현재 선택 항목, 페이지 정보를 제공한다. 앱은 그 데이터를 오버레이의 후보 목록에 반영한다. Microsoft의 게임 IME 예제도 TSF 후보 데이터를 받아 게임 화면에 그리는 구현을 포함한다. [Microsoft TSF UI-less mode](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview), [TSF 후보 목록 인터페이스](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfcandidatelistuielement), [Microsoft DXUT IME 예제](https://github.com/microsoft/DXUT/blob/main/Optional/ImeUi.cpp)

컨트롤러로 후보를 선택할 때는 활성 IME가 `ITfCandidateListUIElementBehavior`를 제공하면 선택 변경과 확정 API를 사용할 수 있다. 이 인터페이스의 제공 여부와 실제 동작은 사용할 IME에서 확인해야 한다. 중국어 입력기의 읽기 정보가 조합 문자열과 별도로 제공되는 경우에는 `ITfReadingInformationUIElement`도 표시 대상에 포함한다. [TSF 후보 선택 인터페이스](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfcandidatelistuielementbehavior), [Microsoft TSF UI-less mode](https://learn.microsoft.com/en-us/windows/win32/tsf/uiless-mode-overview)

앱이 이 방식으로 IME 후보 데이터를 받으려면 해당 입력기가 TSF UI-less mode를 지원해야 한다. 모든 Windows 입력기가 이 기능을 제공한다고 단정할 수 없고, 현재 Microsoft 일본어 및 중국어 IME의 호환성은 아직 확인되지 않았다. 현재 UI는 Dear ImGui 입력란의 편집 문자열과 TSF 후보 snapshot을 별도로 보유한다. Win32 backend, ImGui 편집 문자열과 TSF 후보 sink가 한글·일본어·중국어 조합 및 선택에서 함께 동작하는지 Windows에서 확인해야 한다. [Dear ImGui backends](https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md)

2026-10-01 앱에서 IMM 조합 문자열과 확정 문자열을 분리하고 활성 앱의 입력 포커스를 유지한 뒤, 사용자가 데스크톱 일본어 입력·변환을 정상으로 확인했다. 한국어는 마우스 클릭마다 자모가 확정되는 현상이 남아 앱 UI 스레드에서 가상 버튼 마우스 클릭을 컨트롤러와 같은 포인터 이벤트로 바꾸는 어댑터를 추가했다. 해당 한국어 수정과 HMD 동작은 미검증 상태이며, 일본어 TSF 후보 클릭·다국어 OSC 전체 왕복도 이 보고만으로 통과 처리하지 않는다.

현재 확인이 필요한 항목은 다음과 같다.

- SteamVR 오버레이를 조작하는 동안 오버레이 앱의 입력란이 Windows IME 포커스를 유지하는가?
- 가상 키 입력으로 한글 조합 과정 전체와 일본어·중국어 변환이 동작하는가?
- 사용할 일본어 및 중국어 IME가 TSF UI-less mode에서 후보 목록과 선택 동작을 제공하는가?
- Dear ImGui 편집 문자열과 별도 TSF 후보 수신 코드를 같은 입력 흐름으로 연결할 수 있는가? 실패하면 앱이 TSF 텍스트 저장소를 직접 구현해야 하는가?
- 중국어 간체와 번체 중 어느 범위까지 필수로 지원할 것인가?

각 항목의 공식 근거, 현재 판정과 실기기 확인 조건은 [기술 조사](TECHNICAL_FEASIBILITY.md)에 있다. 문서 조사만으로 미확인 항목을 통과 처리하지 않는다.

## 실기기 확인과 구현 순서

1. **일반 오버레이와 컨트롤러 판정:** Meta Quest Touch 기본 바인딩으로 대시보드가 닫힌 상태의 포인터·클릭을 확인하고, 커서의 아래쪽 보정, standing 공간 고정과 사용자를 향하는 회전, Grip 드래그와 Grip 중 스틱 크기·거리 조절을 실기기에서 확인한다.
2. **소환 옵션:** UI 언어·버튼 조합·0~3초 홀드 시간이 앱 재시작 뒤에도 유지되는지 확인한다. 기본 오른쪽 Grip+B·0초 조합이 한 번만 표시하고, 버튼을 놓은 다음 새 입력에서 다시 발화하는지 확인한다.
3. **텍스트 입력 경로 판정:** Windows와 HMD에서 [기술 조사의 실기기 판정 항목](TECHNICAL_FEASIBILITY.md#windows-실기기에서-필요한-판정)을 확인한다. VRChat에 게임 포커스가 있는 동안의 입력 포커스, 영어·한글 조합, 일본어 및 필요한 경우 중국어 후보 표시·선택을 Dear ImGui 앱에서 판정한다. 이 확인 결과에 따라 편집 문자열과 TSF 문서 상태의 책임을 확정한다.
4. **현재 앱 구조 보완:** `app/`의 Dear ImGui 기반 CMake 프로젝트와 앱 코어, 커맨드 입력, 키보드 UI, Windows 언어·TSF 어댑터, OpenVR 어댑터와 OSC 모듈을 실기기 판정 결과에 맞춰 보완한다. 기존 프로토타입 소스는 수정하거나 이동하지 않는다. 구체적인 책임과 의존성 원칙은 [모듈 아키텍처](ARCHITECTURE.md)에 따른다.
5. **VRChat 연결:** 확정한 문장을 OSC로 Chatbox 입력란에 채우고, 다국어 왕복과 전체 흐름을 확인한다.

VRChat 공식 문서의 Chatbox 제한은 최대 144자와 최대 9줄이다. 줄 수에는 직접 입력한 줄바꿈과 자동 줄바꿈이 포함된다. 입력란과 전송 동작에 이 제한을 반영해야 한다. [VRChat Chatbox OSC 입력](https://docs.vrchat.com/docs/osc-as-input-controller)

## 라이선스 메모

현재 저장소의 `LICENSE`와 Dear ImGui는 MIT다. Dear ImGui의 저작권·MIT 라이선스 전문은 `app/THIRD_PARTY_NOTICES.md`에 보존하고 빌드 출력 폴더에도 복사한다. Windows IME를 직접 사용하며 Qt 프레임워크 의존성은 두지 않는다. [Dear ImGui MIT License](https://github.com/ocornut/imgui/blob/master/LICENSE.txt)
