#include "ui_text_catalog.h"

#include <array>
#include <iterator>

namespace keyboard::ui_text {
namespace {
struct TextTriple {
    const char *korean;
    const char *japanese;
    const char *english;
};

constexpr std::array<TextTriple, static_cast<std::size_t>(TextId::Count)> kTexts = {{
    {"VR 오버레이 키보드", "VR オーバーレイキーボード", "VR Overlay Keyboard"},
    {"Windows 입력기를 사용해 입력하세요. SteamVR에서 표시, 포인터 자세, 클릭 액션을 바인딩하세요.",
     "Windows IMEで入力します。SteamVRで表示・ポインター姿勢・クリックを割り当ててください。",
     "Type with the active Windows IME. Bind SteamVR toggle, pointer pose, and click actions."},
    {"Chatbox 입력", "Chatbox 入力", "Chatbox text"},
    {"입력창 포커스", "入力欄にフォーカス", "Focus input"},
    {"오버레이 표시", "オーバーレイを表示", "Show keyboard overlay"},
    {"오버레이 숨기기", "オーバーレイを非表示", "Hide keyboard overlay"},
    {"입력 지우기", "入力を消去", "Clear input"},
    {"VRChat Chatbox 채우기", "VRChat Chatbox に入力", "Fill VRChat Chatbox"},
    {"옵션", "オプション", "Options"},
    {"상태", "状態", "Status"},
    {"Windows 전경", "Windows 前景", "Windows foreground"},
    {"편집기 포커스", "編集欄のフォーカス", "Editor focus"},
    {"이 앱", "このアプリ", "this app"},
    {"다른 앱", "別のアプリ", "another app"},
    {"준비됨", "準備完了", "ready"},
    {"선택 안 됨", "未選択", "not selected"},
    {"Windows 입력 언어", "Windows 入力言語", "Windows input language"},
    {"현재 입력 언어", "現在の入力言語", "Current input language"},
    {"확인 중...", "確認中...", "detecting..."},
    {"불러온 Windows 입력 언어가 없습니다.", "読み込まれた Windows 入力言語がありません。", "No loaded Windows input languages were found."},
    {"IME 변환 미리보기 / 후보", "IME 変換プレビュー / 候補", "IME conversion preview / candidates"},
    {"조합 중...", "変換中...", "Composing..."},
    {"IME 조합 미리보기", "IME 変換プレビュー", "IME composition preview"},
    {"IME 후보가 여기에 표시됩니다.", "IME 候補がここに表示されます。", "IME candidates appear here when available."},
    {"키보드", "キーボード", "Keyboard"},
    {"한국어 조합과 IME 후보는 사용 중인 Windows 입력기로 확인해야 합니다.",
     "韓国語の変換と IME 候補は、使用する Windows 入力方式で確認してください。",
     "Korean composition and IME candidates need validation with the target Windows input methods."},
    {"Shift", "Shift", "Shift"},
    {"Shift 켜짐", "Shift オン", "Shift (on)"},
    {"Backspace", "Backspace", "Backspace"},
    {"한글 모드", "ハングルモード", "Korean mode"},
    {"Space", "Space", "Space"},
    {"Enter", "Enter", "Enter"},
    {"かな", "かな", "かな"},
    {"漢字", "漢字", "漢字"},
    {"입력 진단", "入力診断", "Input diagnostics"},
    {"옵션 설정", "オプション設定", "Options"},
    {"닫기", "閉じる", "Close"},
    {"앱 표시 언어", "アプリの表示言語", "Application language"},
    {"키보드 소환 버튼 조합", "キーボード表示ボタンの組み合わせ", "Keyboard summon button combination"},
    {"유지 시간", "長押し時間", "Hold duration"},
    {"0초부터 3초까지 0.1초 단위로 설정할 수 있습니다.",
     "0秒から3秒まで、0.1秒単位で設定できます。",
     "Choose from 0 to 3 seconds in 0.1-second steps."},
    {"소환 버튼을 하나 이상 선택하세요.", "表示ボタンを1つ以上選択してください。", "Select at least one summon button."},
    {"선택한 SteamVR 버튼 액션이 활성 상태가 아닙니다. SteamVR 컨트롤러 설정에서 선택한 액션을 바인딩하세요.",
     "選択した SteamVR ボタンアクションが有効ではありません。SteamVR コントローラー設定で割り当ててください。",
     "A selected SteamVR button action is inactive. Bind it in SteamVR controller settings."},
    {"선택한 SteamVR 버튼 액션이 활성화되어 있습니다.",
     "選択した SteamVR ボタンアクションは有効です。",
     "The selected SteamVR button actions are active."},
    {"지원 컨트롤러가 정해지기 전까지 SteamVR에서 각 소환 액션을 직접 바인딩해야 합니다.",
     "対応コントローラーが決まるまでは、SteamVR で各表示アクションを手動で割り当ててください。",
     "Until a controller profile is selected, bind each summon action in SteamVR manually."},
}};

static_assert(kTexts.size() == static_cast<std::size_t>(TextId::Count));

struct ButtonLabel {
    ControllerButton button;
    const char *korean;
    const char *japanese;
    const char *english;
    bool rightHand;
};

constexpr ButtonLabel kButtonLabels[] = {
    {ControllerButton::LeftGrip, "그립", "グリップ", "Grip", false},
    {ControllerButton::LeftTrigger, "트리거", "トリガー", "Trigger", false},
    {ControllerButton::LeftA, "A 버튼", "A ボタン", "A", false},
    {ControllerButton::LeftB, "B 버튼", "B ボタン", "B", false},
    {ControllerButton::LeftMenu, "메뉴", "メニュー", "Menu", false},
    {ControllerButton::LeftJoystick, "스틱", "スティック", "Joystick", false},
    {ControllerButton::LeftTrackpad, "트랙패드", "トラックパッド", "Trackpad", false},
    {ControllerButton::RightGrip, "그립", "グリップ", "Grip", true},
    {ControllerButton::RightTrigger, "트리거", "トリガー", "Trigger", true},
    {ControllerButton::RightA, "A 버튼", "A ボタン", "A", true},
    {ControllerButton::RightB, "B 버튼", "B ボタン", "B", true},
    {ControllerButton::RightMenu, "메뉴", "メニュー", "Menu", true},
    {ControllerButton::RightJoystick, "스틱", "スティック", "Joystick", true},
    {ControllerButton::RightTrackpad, "트랙패드", "トラックパッド", "Trackpad", true},
};
}

const char *text(UiLanguage language, TextId id) {
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= kTexts.size()) {
        return "";
    }
    const TextTriple &entry = kTexts[index];
    switch (language) {
    case UiLanguage::Korean: return entry.korean;
    case UiLanguage::Japanese: return entry.japanese;
    case UiLanguage::English: return entry.english;
    }
    return entry.korean;
}

const char *languageName(UiLanguage language) {
    switch (language) {
    case UiLanguage::Korean: return "한국어";
    case UiLanguage::Japanese: return "日本語";
    case UiLanguage::English: return "English";
    }
    return "한국어";
}

std::string controllerButtonLabel(UiLanguage language, ControllerButton button) {
    for (const ButtonLabel &entry : kButtonLabels) {
        if (entry.button != button) {
            continue;
        }
        const char *side = nullptr;
        const char *control = nullptr;
        switch (language) {
        case UiLanguage::Korean:
            side = entry.rightHand ? "오른쪽 " : "왼쪽 ";
            control = entry.korean;
            break;
        case UiLanguage::Japanese:
            side = entry.rightHand ? "右 " : "左 ";
            control = entry.japanese;
            break;
        case UiLanguage::English:
            side = entry.rightHand ? "Right " : "Left ";
            control = entry.english;
            break;
        }
        return std::string(side ? side : "") + (control ? control : "");
    }
    return "?";
}

} // namespace keyboard::ui_text
