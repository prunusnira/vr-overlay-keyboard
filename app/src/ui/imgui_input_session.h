#pragma once

#include "../core/app_contracts.h"

#include <imgui.h>

#include <array>
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
    void dispatchControllerPointerEvent(const keyboard::PointerEvent &event);
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
    bool hasActiveEditorState() const;
    bool editorCursorMatchesRange(int start, int end) const;
    bool editEditorText(std::string &text, int replaceStart, int replaceEnd,
                        const std::string &replacement, const std::string &activeSuffix,
                        int &activeStart, int &activeEnd);
    bool backspaceEditor(std::string &text);
    bool isVirtualControlAt(int x, int y) const;

private:
    struct PointerTarget {
        ImVec4 bounds;
        ImGuiID id = 0;
        bool virtualControl = false;
    };

    struct ControllerPointerState {
        bool visible = false;
        bool down = false;
        int x = -1;
        int y = -1;
        ImGuiID pressedTargetId = 0;
        bool editorMouseCapture = false;
    };

    struct ControllerClick {
        ImGuiID targetId = 0;
        ImVec2 position{};
    };

    void registerPointerTarget(const ImVec4 &bounds, ImGuiID id, bool virtualControl = true);
    ImGuiID pointerTargetAt(int x, int y) const;
    bool controllerPointerOver(ImGuiID id, const ImVec4 &bounds) const;
    bool controllerPointerDownOn(ImGuiID id) const;
    bool consumeControllerClick(ImGuiID id, ImVec2 *position = nullptr);

    ImGuiID m_editorId = 0;
    ImGuiID m_pressedButtonId = 0;
    ImGuiID m_draggedScrollbarId = 0;
    ImGuiID m_draggedSliderId = 0;
    float m_scrollbarGrabOffset = 0.0f;
    bool m_editorPointerGesture = false;
    // 직전 화면에서 실제로 보인 버튼/스크롤바의 영역만 Windows 포인터 어댑터가 조회한다.
    std::vector<PointerTarget> m_virtualControlBounds;
    std::vector<PointerTarget> m_previousPointerTargets;
    std::array<ControllerPointerState, 2> m_controllerPointers{};
    std::vector<ControllerClick> m_controllerClicks;
    int m_controllerEditorOwner = -1;
};
