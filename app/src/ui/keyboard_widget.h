#pragma once

#include "../core/app_contracts.h"

#include <QPointer>
#include <QWidget>

#include <functional>
#include <string>
#include <vector>

class QAbstractButton;
class QButtonGroup;
class QLabel;
class QListWidget;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QGridLayout;

class KeyboardWidget final : public QWidget {
public:
    using FocusRequestCallback = std::function<bool(std::string *)>;

    explicit KeyboardWidget(keyboard::KeyboardActions &actions, QWidget *parent = nullptr);

    keyboard::ImageFrame renderFrame();
    void dispatchOverlayPointerEvent(const keyboard::PointerEvent &event);
    void setAppState(const keyboard::AppUiState &state);
    void setFocusRequestCallback(FocusRequestCallback callback);
    void refreshFocusStatus(bool appIsForeground);
    void prepareEditorFocus();
    bool editorHasFocus() const;
    void appendLog(const QString &message);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void requestEditorFocus();
    void sendKey(keyboard::KeyCode key, bool withShift = false);
    void rebuildInputLanguages(const std::vector<keyboard::InputLanguage> &languages);
    void updateCandidates(const keyboard::CandidateSnapshot &snapshot);
    QWidget *deepestChildAt(const QPoint &position, QPoint *childPosition) const;

    keyboard::KeyboardActions &m_actions;
    FocusRequestCallback m_focusRequestCallback;
    QLineEdit *m_editor = nullptr;
    QWidget *m_languageChoices = nullptr;
    QGridLayout *m_languageGrid = nullptr;
    QButtonGroup *m_languageButtons = nullptr;
    QLabel *m_languageLabel = nullptr;
    QLabel *m_candidateHeading = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_focusStatus = nullptr;
    QListWidget *m_candidates = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QPushButton *m_toggleOverlayButton = nullptr;
    QPushButton *m_shiftButton = nullptr;
    std::vector<std::string> m_languageIds;
    std::vector<std::string> m_languageLabels;
    std::string m_candidateSignature;
    QAbstractButton *m_pressedPointerButton = nullptr;
    QAbstractButton *m_pressedMouseButton = nullptr;
    QPointer<QWidget> m_pressedPointerTarget;
    QPoint m_lastPointerTargetPosition;
    Qt::MouseButtons m_pressedPointerButtons = Qt::NoButton;
    bool m_shiftForNextKey = false;
};
