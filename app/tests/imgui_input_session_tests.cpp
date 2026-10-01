#include "ui/imgui_input_session.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class InputHarness final {
public:
    explicit InputHarness(bool protectEditor = true) : m_protectEditor(protectEditor) {
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(600.0f, 400.0f);
        io.DeltaTime = 1.0f / 60.0f;
        unsigned char *pixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        io.Fonts->SetTexID(1);
    }

    ~InputHarness() {
        ImGui::DestroyContext();
    }

    void frame(bool requestFocus = false) {
        letterPressed = false;
        backspacePressed = false;
        ImGui::NewFrame();
        applicationFocusLost = ImGui::GetIO().AppFocusLost;
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(600.0f, 400.0f), ImGuiCond_Always);
        ImGui::Begin("Input session test", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        m_session.beginFrame();
        editorId = ImGui::GetID("##editor");
        if (m_protectEditor) {
            m_session.drawEditor("##editor", text, ImVec2(450.0f, 90.0f), requestFocus);
        } else {
            // 원래 InputText의 바깥 클릭 해제를 재현해 회귀 시나리오가 실제 문제를 잡는지 확인한다.
            if (requestFocus) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputTextMultiline("##editor", &text, ImVec2(450.0f, 90.0f));
        }
        editorBounds = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        letterPressed = m_session.button("A", ImVec2(70.0f, 42.0f));
        letterBounds = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImGui::SameLine();
        backspacePressed = m_session.button("Backspace", ImVec2(150.0f, 42.0f));
        backspaceBounds = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        m_session.horizontalScrollbar("##scroll", ImVec2(450.0f, 14.0f),
                                      900.0f, 450.0f, scrollOffset);
        scrollbarBounds = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        if (letterPressed) {
            ++letterCount;
            ImGui::GetIO().AddInputCharacter('a');
        }
        if (backspacePressed) {
            ++backspaceCount;
            // Win32 backend와 같은 큐에 키 누름·뗌을 넣어 다음 프레임의 실제 편집 동작을 검사한다.
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Backspace, true);
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Backspace, false);
        }
        m_session.endFrame();
        ImGui::End();
        ImGui::Render();
    }

    void focus() {
        frame();
        frame(true);
        frame();
        requireFocus();
    }

    void requireFocus() const {
        require(ImGui::GetActiveID() == editorId, "Editor lost its active ID");
    }

    void move(const ImVec2 &position) {
        ImGui::GetIO().AddMousePosEvent(position.x, position.y);
    }

    void press(const ImVec2 &position) {
        move(position);
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        frame();
    }

    void release(const ImVec2 &position) {
        move(position);
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        frame();
    }

    void clear() {
        m_session.clearEditor(text);
        frame();
    }

    bool isVirtualControlAt(const ImVec2 &point) const {
        return m_session.isVirtualControlAt(static_cast<int>(point.x), static_cast<int>(point.y));
    }

    std::string text;
    ImGuiID editorId = 0;
    ImRect editorBounds;
    ImRect letterBounds;
    ImRect backspaceBounds;
    ImRect scrollbarBounds;
    float scrollOffset = 0.0f;
    bool letterPressed = false;
    bool backspacePressed = false;
    int letterCount = 0;
    int backspaceCount = 0;
    bool applicationFocusLost = false;

private:
    ImGuiInputSession m_session;
    bool m_protectEditor = true;
};

void reproduceOriginalFocusLoss() {
    InputHarness harness(false);
    harness.focus();
    harness.press(harness.letterBounds.GetCenter());
    require(ImGui::GetActiveID() != harness.editorId,
            "Original InputText outside-click deactivation was not reproduced");
}

void retainFocusAndTypeOnRelease() {
    InputHarness harness;
    harness.focus();
    ImGui::GetIO().AddInputCharactersUTF8(u8"日本語");
    harness.frame();
    require(harness.text == u8"日本語", "Japanese text was not inserted");
    harness.press(harness.letterBounds.GetCenter());
    harness.requireFocus();
    require(!harness.letterPressed && harness.letterCount == 0, "Key fired before release");
    harness.move(harness.backspaceBounds.GetCenter());
    harness.frame();
    harness.requireFocus();
    require(!ImGui::GetInputTextState(harness.editorId)->HasSelection(),
            "Dragging a virtual key changed the editor selection");
    harness.release(harness.letterBounds.GetCenter());
    harness.requireFocus();
    require(harness.letterPressed && harness.letterCount == 1, "Key did not fire once on release");
    harness.frame();
    require(harness.text == u8"日本語a", "Released key did not reach the active editor");

    const char *expected[] = {u8"日本語", u8"日本", u8"日", ""};
    for (const char *value : expected) {
        harness.press(harness.backspaceBounds.GetCenter());
        harness.requireFocus();
        harness.release(harness.backspaceBounds.GetCenter());
        harness.requireFocus();
        require(harness.backspacePressed, "Backspace click was dropped");
        harness.frame();
        harness.frame();
        require(harness.text == value, "Backspace did not remove exactly one Unicode character");
    }
    require(harness.backspaceCount == 4, "Backspace produced duplicate or missing key requests");
}

void cancelDraggedButtonsAndKeepEditorGestures() {
    InputHarness harness;
    harness.focus();
    ImGui::GetIO().AddInputCharactersUTF8("abcdef");
    harness.frame();
    harness.press(harness.letterBounds.GetCenter());
    harness.release(harness.backspaceBounds.GetCenter());
    harness.requireFocus();
    require(harness.letterCount == 0 && harness.backspaceCount == 0,
            "Releasing over another button activated an action");
    require(harness.text == "abcdef", "Cancelled button changed the text");

    harness.press(harness.letterBounds.GetCenter());
    harness.release(ImVec2(-100.0f, -100.0f));
    harness.requireFocus();
    require(harness.letterCount == 0, "Pointer cancellation activated the button");

    const ImVec2 selectionStart(harness.editorBounds.Min.x + 5.0f,
                                harness.editorBounds.Min.y + 10.0f);
    harness.press(selectionStart);
    harness.move(ImVec2(selectionStart.x + 100.0f, selectionStart.y));
    harness.frame();
    require(ImGui::GetInputTextState(harness.editorId)->HasSelection(),
            "Dragging inside the editor no longer selects text");
    harness.release(ImVec2(selectionStart.x + 100.0f, selectionStart.y));
    // 활성 앱의 매 프레임 포커스 유지 요청이 선택을 재초기화하지 않는지 검사한다.
    for (int frame = 0; frame < 5; ++frame) {
        harness.frame(true);
        harness.requireFocus();
        require(ImGui::GetInputTextState(harness.editorId)->HasSelection(),
                "Retaining focus reset the editor selection");
    }
    harness.clear();
    harness.requireFocus();
    require(harness.text.empty(), "Clearing the active editor did not update its internal buffer");
    harness.frame();
    require(harness.text.empty(), "Cleared text came back on the next frame");
}

void scrollWithoutChangingEditorFocus() {
    InputHarness harness;
    harness.focus();
    require(harness.isVirtualControlAt(harness.letterBounds.GetCenter()) &&
            harness.isVirtualControlAt(harness.backspaceBounds.GetCenter()) &&
            harness.isVirtualControlAt(harness.scrollbarBounds.GetCenter()),
            "Visible virtual controls were not registered for mouse routing");
    require(!harness.isVirtualControlAt(harness.editorBounds.GetCenter()) &&
            !harness.isVirtualControlAt(ImVec2(590.0f, 390.0f)),
            "Mouse routing included the editor or empty window area");
    ImGui::GetIO().AddInputCharactersUTF8(u8"日本語");
    harness.frame();
    harness.press(ImVec2(harness.scrollbarBounds.Max.x - 10.0f,
                         harness.scrollbarBounds.GetCenter().y));
    harness.requireFocus();
    require(harness.scrollOffset > 0.0f, "Scrollbar click did not move the preview");
    harness.move(ImVec2(harness.scrollbarBounds.Min.x, harness.scrollbarBounds.GetCenter().y));
    harness.frame();
    harness.requireFocus();
    require(harness.scrollOffset == 0.0f, "Scrollbar drag did not reach the beginning");
    harness.release(harness.scrollbarBounds.GetCenter());
    harness.requireFocus();
    require(harness.text == u8"日本語" && !ImGui::GetInputTextState(harness.editorId)->HasSelection(),
            "Scrollbar gesture changed the text or selection");
}

void focusByMouseAndRespectApplicationFocusLoss() {
    InputHarness harness;
    // 앱 시작과 동일하게 첫 프레임부터 유지 요청을 전달한다. 클릭 없이 입력창이 활성화되어야 한다.
    harness.frame(true);
    harness.frame(true);
    harness.requireFocus();
    harness.frame();
    harness.press(harness.editorBounds.GetCenter());
    harness.requireFocus();
    harness.release(harness.editorBounds.GetCenter());
    harness.requireFocus();
    // ImGui는 앱 비활성 시 ActiveId를 유지하지만 키·마우스 상태는 해제한다. 포커스 이벤트도 차단하지 않는다.
    ImGui::GetIO().AddFocusEvent(false);
    harness.frame();
    require(harness.applicationFocusLost, "Application focus loss event was swallowed");
    require(!ImGui::GetIO().MouseDown[ImGuiMouseButton_Left], "Mouse remained down after application focus loss");
    ImGui::GetIO().AddFocusEvent(true);
    harness.frame(true);
    harness.frame(true);
    harness.requireFocus();
}
}

int main() {
    try {
        reproduceOriginalFocusLoss();
        retainFocusAndTypeOnRelease();
        cancelDraggedButtonsAndKeepEditorGestures();
        scrollWithoutChangingEditorFocus();
        focusByMouseAndRespectApplicationFocusLoss();
        std::cout << "PASS: focus retention, release activation, Unicode backspace, drag cancellation, editor selection, clear, horizontal scrolling, application focus loss\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
