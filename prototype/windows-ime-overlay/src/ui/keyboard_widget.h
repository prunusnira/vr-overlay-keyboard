#pragma once

#include "../ime/tsf_input.h"

#include <QImage>
#include <QEvent>
#include <QPointF>
#include <QWidget>

#include <functional>

class QLineEdit;
class QButtonGroup;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QLabel;

class KeyboardWidget final : public QWidget {
public:
    using KeyCallback = std::function<bool(WORD, bool, QString *)>;
    using CandidateCallback = std::function<bool(UINT, QString *)>;
    using ShowOverlayCallback = std::function<void()>;

    explicit KeyboardWidget(QWidget *parent = nullptr);

    QImage renderToImage();
    void dispatchOverlayMouseEvent(QEvent::Type type, const QPointF &position, Qt::MouseButton button);
    void setKeyCallback(KeyCallback callback);
    void setCandidateCallback(CandidateCallback callback);
    void setShowOverlayCallback(ShowOverlayCallback callback);
    void updateCandidates(const CandidateSnapshot &snapshot);
    void appendLog(const QString &message);
    void refreshInputStatus();
    void refreshInputLanguage();
    bool editorHasFocus() const;

private:
    void requestEditorFocus();
    void submitKey(WORD virtualKey, bool withShift = false);
    void handleCandidateClick(int row);
    QWidget *deepestChildAt(const QPoint &position, QPoint *childPosition) const;

    QLineEdit *m_editor = nullptr;
    QWidget *m_languageChoices = nullptr;
    QButtonGroup *m_languageButtons = nullptr;
    QLabel *m_languageLabel = nullptr;
    QListWidget *m_candidates = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_focusStatus = nullptr;
    QPushButton *m_shiftButton = nullptr;
    KeyCallback m_keyCallback;
    CandidateCallback m_candidateCallback;
    ShowOverlayCallback m_showOverlayCallback;
    Qt::MouseButtons m_pressedButtons = Qt::NoButton;
    bool m_shiftForNextKey = false;
};
