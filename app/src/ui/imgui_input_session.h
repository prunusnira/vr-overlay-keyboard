#pragma once

#include <imgui.h>

#include <string>
#include <vector>

// 가상 버튼의 포인터 입력과 편집창의 포커스를 조정하는 ImGui 전용 모듈이다.
// Windows 전경 창 전환, SendInput, TSF 처리는 이 모듈 밖의 어댑터가 담당한다.
class ImGuiInputSession final {
public:
    struct EditorInteraction {
        bool clicked = false;
        bool active = false;
    };

    void beginFrame();
    void endFrame();
    EditorInteraction drawEditor(const char *label, std::string &text,
                                 const ImVec2 &size, bool requestFocus);
    bool button(const char *label, const ImVec2 &size, bool selected = false, bool enabled = true);
    bool sliderFloat(const char *label, const ImVec2 &size, float &value,
                     float minimum, float maximum, const char *format);
    bool horizontalScrollbar(const char *label, const ImVec2 &size,
                             float maxScroll, float visibleWidth, float &scroll);
    bool verticalScrollbar(const char *label, const ImVec2 &size,
                           float maxScroll, float visibleHeight, float &scroll);
    void clearEditor(std::string &text);
    bool isVirtualControlAt(int x, int y) const;

private:
    ImGuiID m_editorId = 0;
    ImGuiID m_pressedButtonId = 0;
    ImGuiID m_draggedScrollbarId = 0;
    ImGuiID m_draggedSliderId = 0;
    float m_scrollbarGrabOffset = 0.0f;
    bool m_editorPointerGesture = false;
    // 직전 화면에서 실제로 보인 버튼/스크롤바의 영역만 Windows 포인터 어댑터가 조회한다.
    std::vector<ImVec4> m_virtualControlBounds;
};
