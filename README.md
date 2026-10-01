# vr-overlay-keyboard

Windows SteamVR에서 VRChat Chatbox에 문장을 입력하기 위한 오버레이 키보드 프로젝트.

제품 앱은 `app/`에 새 CMake 프로젝트로 구현한다. 프로토타입에서 확인한 사용자 흐름은 새 앱이 이어받을 동작 기준으로 삼고, 코드는 새 모듈 구조에 맞춰 다시 작성한다. 프로토타입 소스는 제품 앱에 복사하거나 수정하지 않는다. [프로토타입 실행 기록](prototype/windows-ime-overlay/README.md)

- [프로젝트 기획](docs/PROJECT_PLAN.md)
- [기술 조사와 실기기 확인 항목](docs/TECHNICAL_FEASIBILITY.md)
- [모듈 아키텍처와 오버레이 실행 흐름](docs/ARCHITECTURE.md)
