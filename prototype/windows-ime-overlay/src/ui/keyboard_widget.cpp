#include "keyboard_widget.h"

#include <QApplication>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QColor>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

#include <Windows.h>

#include <algorithm>
#include <vector>
#include <iterator>
#include <utility>

namespace {
QString describeInputLanguage(HKL layout) {
    const LANGID languageId = LOWORD(reinterpret_cast<ULONG_PTR>(layout));
    const LCID localeId = MAKELCID(languageId, SORT_DEFAULT);
    wchar_t localizedName[128] = {};
    const int length = GetLocaleInfoW(localeId, LOCALE_SLANGUAGE, localizedName,
                                      static_cast<int>(std::size(localizedName)));
    const QString language = length > 0
        ? QString::fromWCharArray(localizedName)
        : QStringLiteral("Language 0x%1").arg(languageId, 4, 16, QLatin1Char('0'));
    return QStringLiteral("%1 [HKL 0x%2]")
        .arg(language)
        .arg(static_cast<quintptr>(reinterpret_cast<ULONG_PTR>(layout)), 0, 16);
}

std::vector<HKL> getLoadedInputLanguages() {
    const int count = GetKeyboardLayoutList(0, nullptr);
    std::vector<HKL> layouts(static_cast<size_t>(count));
    const int copied = count > 0 ? GetKeyboardLayoutList(count, layouts.data()) : 0;
    layouts.resize(static_cast<size_t>(copied));

    const HKL current = GetKeyboardLayout(0);
    if (std::find(layouts.begin(), layouts.end(), current) == layouts.end()) {
        layouts.push_back(current);
    }
    return layouts;
}
}

KeyboardWidget::KeyboardWidget(QWidget *parent)
    : QWidget(parent) {
    setWindowTitle(QStringLiteral("SteamVR Windows IME Prototype"));
    setFixedSize(1024, 1024);
    setStyleSheet(QStringLiteral(
        "QWidget { background: #161a22; color: #f1f5f9; font-size: 16px; }"
        "QLineEdit { background: #ffffff; color: #111827; padding: 10px; font-size: 24px; }"
        "QPushButton { background: #303949; border: 1px solid #667085; border-radius: 6px; padding: 8px; }"
        "QPushButton:pressed { background: #52637d; }"
        "QPushButton:checked { background: #2563eb; border-color: #60a5fa; }"
        "QListWidget, QPlainTextEdit { background: #0b0e13; border: 1px solid #475467; }"
        "QListWidget::item:selected { background: #2563eb; }"
    ));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("SteamVR Windows IME Prototype"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto *description = new QLabel(
        QStringLiteral("Compose text with the active Windows IME, then fill the VRChat Chatbox using OSC. Click Focus input before sending virtual keys."), this);
    description->setWordWrap(true);
    root->addWidget(description);

    m_editor = new QLineEdit(this);
    m_editor->setPlaceholderText(QStringLiteral("Type here with the active Windows IME"));
    m_editor->setFocusPolicy(Qt::StrongFocus);
    m_editor->installEventFilter(this);
    root->addWidget(m_editor);

    auto *languageRow = new QHBoxLayout();
    m_languageLabel = new QLabel(QStringLiteral("Current Windows input language: detecting..."), this);
    languageRow->addWidget(m_languageLabel, 1);
    root->addLayout(languageRow);
    m_languageChoices = new QWidget(this);
    auto *languageGrid = new QGridLayout(m_languageChoices);
    languageGrid->setContentsMargins(0, 0, 0, 0);
    languageGrid->setSpacing(5);
    m_languageButtons = new QButtonGroup(this);
    m_languageButtons->setExclusive(true);
    const std::vector<HKL> inputLanguages = getLoadedInputLanguages();
    for (size_t index = 0; index < inputLanguages.size(); ++index) {
        const HKL layout = inputLanguages[index];
        auto *button = new QPushButton(describeInputLanguage(layout), m_languageChoices);
        button->setCheckable(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setMinimumHeight(46);
        button->setProperty("inputLayout", QVariant::fromValue<qulonglong>(reinterpret_cast<ULONG_PTR>(layout)));
        m_languageButtons->addButton(button);
        languageGrid->addWidget(button, static_cast<int>(index / 3), static_cast<int>(index % 3));
        connect(button, &QPushButton::clicked, this, [this, button, layout]() {
            if (!ActivateKeyboardLayout(layout, 0)) {
                appendLog(QStringLiteral("Windows could not activate input language %1 (error %2).")
                              .arg(button->text())
                              .arg(GetLastError()));
            } else {
                appendLog(QStringLiteral("Activated input language: %1").arg(button->text()));
            }
            refreshInputLanguage();
        });
    }
    const int languageRows = static_cast<int>((inputLanguages.size() + 2) / 3);
    m_languageChoices->setFixedHeight(languageRows * 46 + qMax(0, languageRows - 1) * 5);
    root->addWidget(m_languageChoices);

    auto *controls = new QHBoxLayout();
    auto *focusButton = new QPushButton(QStringLiteral("Focus input"), this);
    auto *showButton = new QPushButton(QStringLiteral("Show in SteamVR"), this);
    auto *clearButton = new QPushButton(QStringLiteral("Clear input"), this);
    auto *sendChatboxButton = new QPushButton(QStringLiteral("VRChat 입력란 채우기"), this);
    focusButton->setFocusPolicy(Qt::NoFocus);
    showButton->setFocusPolicy(Qt::NoFocus);
    clearButton->setFocusPolicy(Qt::NoFocus);
    sendChatboxButton->setFocusPolicy(Qt::NoFocus);
    sendChatboxButton->setMinimumHeight(48);
    controls->addWidget(focusButton);
    controls->addWidget(showButton);
    controls->addWidget(clearButton);
    controls->addWidget(sendChatboxButton);
    controls->addStretch(1);
    root->addLayout(controls);

    m_status = new QLabel(QStringLiteral("Waiting for IME and SteamVR initialization."), this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    m_focusStatus = new QLabel(QStringLiteral("입력 상태 — Windows 전경 창: 확인 중 | 편집기 포커스: 확인 중"), this);
    m_focusStatus->setWordWrap(true);
    m_focusStatus->setStyleSheet(QStringLiteral(
        "QLabel { background: #26364a; border: 1px solid #60a5fa; border-radius: 5px; "
        "padding: 7px; font-weight: 700; }"));
    root->addWidget(m_focusStatus);

    auto *candidateHeading = new QLabel(QStringLiteral("TSF candidates"), this);
    root->addWidget(candidateHeading);
    m_candidates = new QListWidget(this);
    m_candidates->setSelectionMode(QAbstractItemView::SingleSelection);
    m_candidates->setFocusPolicy(Qt::NoFocus);
    m_candidates->setMaximumHeight(178);
    root->addWidget(m_candidates);

    auto *keyboardHeading = new QLabel(QStringLiteral("Virtual keyboard (Win32 key events)"), this);
    auto *compositionGuide = new QLabel(
        QStringLiteral("Korean check: R → K → A should show ㄱ → 가 → 감. Japanese: N I H O N G O → Space, then choose 日本語 if offered."), this);
    compositionGuide->setWordWrap(true);
    root->addWidget(compositionGuide);
    root->addWidget(keyboardHeading);
    auto *keyboard = new QGridLayout();
    keyboard->setSpacing(5);
    const QStringList rows = {
        QStringLiteral("QWERTYUIOP"),
        QStringLiteral("ASDFGHJKL"),
        QStringLiteral("ZXCVBNM")
    };
    const int rowOffsets[] = {0, 1, 2};
    for (int row = 0; row < rows.size(); ++row) {
        for (int column = 0; column < rows[row].size(); ++column) {
            const QChar letter = rows[row][column];
            auto *key = new QPushButton(QString(letter), this);
            key->setFocusPolicy(Qt::NoFocus);
            key->setMinimumSize(70, 48);
            keyboard->addWidget(key, row, column + rowOffsets[row]);
            connect(key, &QPushButton::clicked, this, [this, letter]() {
                submitKey(static_cast<WORD>(letter.toLatin1()), m_shiftForNextKey);
                m_shiftForNextKey = false;
                if (m_shiftButton) {
                    m_shiftButton->setText(QStringLiteral("Shift"));
                }
            });
        }
    }

    m_shiftButton = new QPushButton(QStringLiteral("Shift"), this);
    auto *backspaceButton = new QPushButton(QStringLiteral("Backspace"), this);
    auto *spaceButton = new QPushButton(QStringLiteral("Space"), this);
    auto *enterButton = new QPushButton(QStringLiteral("Enter"), this);
    auto *hangulButton = new QPushButton(QStringLiteral("한글 모드"), this);
    auto *kanaButton = new QPushButton(QStringLiteral("かな"), this);
    auto *kanjiButton = new QPushButton(QStringLiteral("漢字"), this);
    for (QPushButton *button : {m_shiftButton, backspaceButton, spaceButton, enterButton,
                                hangulButton, kanaButton, kanjiButton}) {
        button->setFocusPolicy(Qt::NoFocus);
        button->setMinimumHeight(48);
    }
    keyboard->addWidget(m_shiftButton, 2, 0, 1, 2);
    keyboard->addWidget(backspaceButton, 2, 9, 1, 3);
    keyboard->addWidget(hangulButton, 3, 0, 1, 2);
    keyboard->addWidget(spaceButton, 3, 2, 1, 4);
    keyboard->addWidget(enterButton, 3, 6, 1, 2);
    keyboard->addWidget(kanaButton, 3, 8, 1, 2);
    keyboard->addWidget(kanjiButton, 3, 10, 1, 2);
    root->addLayout(keyboard);

    auto *logHeading = new QLabel(QStringLiteral("Diagnostic log"), this);
    root->addWidget(logHeading);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(80);
    root->addWidget(m_log, 1);

    connect(focusButton, &QPushButton::clicked, this, [this]() { requestEditorFocus(); });
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        m_editor->clear();
        appendLog(QStringLiteral("Editor cleared."));
    });
    connect(showButton, &QPushButton::clicked, this, [this]() {
        if (m_showOverlayCallback) {
            m_showOverlayCallback();
        } else {
            appendLog(QStringLiteral("SteamVR overlay is not initialized."));
        }
    });
    connect(m_shiftButton, &QPushButton::clicked, this, [this]() {
        m_shiftForNextKey = !m_shiftForNextKey;
        m_shiftButton->setText(m_shiftForNextKey ? QStringLiteral("Shift (on)") : QStringLiteral("Shift"));
    });
    connect(backspaceButton, &QPushButton::clicked, this, [this]() { submitKey(VK_BACK); });
    connect(spaceButton, &QPushButton::clicked, this, [this]() { submitKey(VK_SPACE); });
    connect(enterButton, &QPushButton::clicked, this, [this]() { submitKey(VK_RETURN); });
    connect(hangulButton, &QPushButton::clicked, this, [this]() { submitKey(VK_HANGUL); });
    connect(kanaButton, &QPushButton::clicked, this, [this]() { submitKey(VK_IME_ON); });
    connect(kanjiButton, &QPushButton::clicked, this, [this]() { submitKey(VK_KANJI); });
    connect(sendChatboxButton, &QPushButton::clicked, this, [this]() {
        if (!m_sendChatboxCallback) {
            m_status->setText(QStringLiteral("VRChat OSC sender is not initialized."));
            appendLog(QStringLiteral("OSC send failed: no Chatbox sender is connected."));
            return;
        }

        QString error;
        if (!m_sendChatboxCallback(m_editor->text(), &error)) {
            m_status->setText(error);
            appendLog(QStringLiteral("OSC send failed: %1").arg(error));
            return;
        }

        m_status->setText(QStringLiteral("OSC packet sent to VRChat Chatbox. Confirm OSC is enabled in VRChat."));
        appendLog(QStringLiteral("OSC sent to 127.0.0.1:9000: /chatbox/input (send=false)."));
    });
    connect(m_candidates, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        handleCandidateClick(m_candidates->row(item));
    });
    for (QAbstractButton *button : findChildren<QAbstractButton *>()) {
        button->installEventFilter(this);
    }

    appendLog(QStringLiteral("Widget ready. Click Focus input, then Show in SteamVR."));
    refreshInputLanguage();
}

QImage KeyboardWidget::renderToImage() {
    QImage image(size(), QImage::Format_RGBA8888);
    image.fill(QColor(22, 26, 34, 255));
    QPainter painter(&image);
    render(&painter);
    painter.end();
    return image;
}

void KeyboardWidget::dispatchOverlayMouseEvent(QEvent::Type type, const QPointF &position, Qt::MouseButton button) {
    if (type == QEvent::MouseButtonPress) {
        m_pressedButtons |= button;
    } else if (type == QEvent::MouseButtonRelease) {
        m_pressedButtons &= ~button;
    }

    QPoint localPosition;
    QWidget *target = deepestChildAt(position.toPoint(), &localPosition);
    auto *clickedButton = qobject_cast<QAbstractButton *>(target);
    if (type == QEvent::MouseButtonRelease && m_pressedPointerButton) {
        QAbstractButton *pressedButton = m_pressedPointerButton;
        m_pressedPointerButton = nullptr;
        const bool releasedOnSameButton = clickedButton == pressedButton;
        pressedButton->setDown(false);
        if (releasedOnSameButton && pressedButton->isEnabled()) {
            // Invoke the button action without sending Qt a mouse event that can
            // finalize the editor's active IME composition.
            pressedButton->click();
        }
        return;
    }

    if (!target) {
        return;
    }
    if (type == QEvent::MouseButtonPress && clickedButton) {
        if (m_pressedPointerButton) {
            m_pressedPointerButton->setDown(false);
        }
        m_pressedPointerButton = clickedButton;
        clickedButton->setDown(true);
        return;
    }

    const QPoint globalPosition = target->mapToGlobal(localPosition);
    QMouseEvent mouseEvent(type,
                           QPointF(localPosition),
                           QPointF(globalPosition),
                           button,
                           m_pressedButtons,
                           Qt::NoModifier);
    QApplication::sendEvent(target, &mouseEvent);
}

void KeyboardWidget::setKeyCallback(KeyCallback callback) {
    m_keyCallback = std::move(callback);
}

void KeyboardWidget::setCandidateCallback(CandidateCallback callback) {
    m_candidateCallback = std::move(callback);
}

void KeyboardWidget::setShowOverlayCallback(ShowOverlayCallback callback) {
    m_showOverlayCallback = std::move(callback);
}

void KeyboardWidget::setSendChatboxCallback(SendChatboxCallback callback) {
    m_sendChatboxCallback = std::move(callback);
}

void KeyboardWidget::updateCandidates(const CandidateSnapshot &snapshot) {
    m_candidates->clear();
    for (const QString &candidate : snapshot.candidates) {
        m_candidates->addItem(candidate);
    }
    if (snapshot.selectedIndex < static_cast<UINT>(m_candidates->count())) {
        m_candidates->setCurrentRow(static_cast<int>(snapshot.selectedIndex));
    }

    if (snapshot.uiElementId == TF_INVALID_UIELEMENTID) {
        m_status->setText(QStringLiteral("No active TSF candidate list."));
        appendLog(QStringLiteral("TSF candidate UI ended."));
    } else {
        m_status->setText(QStringLiteral("TSF UI %1 | %2 candidates | selected %3 | page %4 | page starts: %5")
                              .arg(snapshot.uiElementId)
                              .arg(snapshot.candidates.size())
                              .arg(snapshot.selectedIndex)
                              .arg(snapshot.currentPage)
                              .arg([&snapshot]() {
                                  QStringList starts;
                                  for (UINT start : snapshot.pageStarts) {
                                      starts.push_back(QString::number(start));
                                  }
                                  return starts.join(QStringLiteral(", "));
                              }()));
        appendLog(QStringLiteral("TSF candidate update: %1 candidates, selected %2, page %3.")
                      .arg(snapshot.candidates.size())
                      .arg(snapshot.selectedIndex)
                      .arg(snapshot.currentPage));
    }
}

void KeyboardWidget::appendLog(const QString &message) {
    m_log->appendPlainText(message);
}

void KeyboardWidget::refreshInputStatus() {
    const HWND foregroundWindow = GetForegroundWindow();
    DWORD foregroundProcessId = 0;
    if (foregroundWindow) {
        GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId);
    }
    const bool foregroundIsApp = foregroundProcessId == GetCurrentProcessId();
    const bool focused = editorHasFocus();
    m_focusStatus->setText(QStringLiteral("입력 상태 — Windows 전경 창: %1 | 편집기 포커스: %2")
                               .arg(foregroundIsApp ? QStringLiteral("이 앱") : QStringLiteral("다른 앱"))
                               .arg(focused ? QStringLiteral("예") : QStringLiteral("아니요")));
    if (!m_hasPreviousInputStatus || foregroundIsApp != m_previousForegroundIsApp ||
        focused != m_previousEditorFocused) {
        appendLog(QStringLiteral("Input focus changed: foreground=%1, editorFocus=%2")
                      .arg(foregroundIsApp ? QStringLiteral("this app") : QStringLiteral("another app"))
                      .arg(focused ? QStringLiteral("yes") : QStringLiteral("no")));
        m_previousForegroundIsApp = foregroundIsApp;
        m_previousEditorFocused = focused;
        m_hasPreviousInputStatus = true;
    }
}

void KeyboardWidget::refreshInputLanguage() {
    const HKL current = GetKeyboardLayout(0);
    const QString language = describeInputLanguage(current);
    m_languageLabel->setText(QStringLiteral("Current Windows input language: %1").arg(language));

    for (QAbstractButton *button : m_languageButtons->buttons()) {
        const ULONG_PTR rawLayout = static_cast<ULONG_PTR>(button->property("inputLayout").toULongLong());
        if (reinterpret_cast<HKL>(rawLayout) == current) {
            button->setChecked(true);
            break;
        }
    }
}

bool KeyboardWidget::editorHasFocus() const {
    return QApplication::focusWidget() == m_editor;
}

bool KeyboardWidget::eventFilter(QObject *watched, QEvent *event) {
    auto logAfterEvent = [this](const QString &message) {
        QMetaObject::invokeMethod(this, [this, message]() { appendLog(message); }, Qt::QueuedConnection);
    };

    auto *button = qobject_cast<QAbstractButton *>(watched);
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (button && event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton) {
        if (m_pressedMouseButton) {
            m_pressedMouseButton->setDown(false);
        }
        m_pressedMouseButton = button;
        button->setDown(true);
        return true;
    }
    if (button && event->type() == QEvent::MouseMove && m_pressedMouseButton == button) {
        button->setDown(button->rect().contains(mouseEvent->position().toPoint()));
        return true;
    }
    if (button && event->type() == QEvent::MouseButtonRelease && m_pressedMouseButton) {
        QAbstractButton *pressedButton = m_pressedMouseButton;
        m_pressedMouseButton = nullptr;
        const bool releasedOnSameButton = button == pressedButton && mouseEvent->button() == Qt::LeftButton;
        const bool releasedInsideButton = pressedButton->rect().contains(mouseEvent->position().toPoint());
        pressedButton->setDown(false);
        if (releasedOnSameButton && releasedInsideButton && pressedButton->isEnabled()) {
            pressedButton->click();
        }
        return true;
    }

    if (watched == m_editor && event->type() == QEvent::FocusIn) {
        logAfterEvent(QStringLiteral("Editor focus event: FocusIn."));
    } else if (watched == m_editor && event->type() == QEvent::FocusOut) {
        logAfterEvent(QStringLiteral("Editor focus event: FocusOut."));
    } else if (watched == m_editor && event->type() == QEvent::KeyPress) {
        const auto *keyEvent = static_cast<const QKeyEvent *>(event);
        logAfterEvent(QStringLiteral("Editor key event: key=0x%1 text=[%2] nativeVK=0x%3 nativeScan=0x%4")
                          .arg(static_cast<quint32>(keyEvent->key()), 0, 16)
                          .arg(keyEvent->text())
                          .arg(keyEvent->nativeVirtualKey(), 0, 16)
                          .arg(keyEvent->nativeScanCode(), 0, 16));
    } else if (watched == m_editor && event->type() == QEvent::InputMethod) {
        const auto *inputMethodEvent = static_cast<const QInputMethodEvent *>(event);
        logAfterEvent(QStringLiteral("Editor IME event: preedit=[%1] commit=[%2] replaceStart=%3 replaceLength=%4")
                          .arg(inputMethodEvent->preeditString())
                          .arg(inputMethodEvent->commitString())
                          .arg(inputMethodEvent->replacementStart())
                          .arg(inputMethodEvent->replacementLength()));
    }
    return QWidget::eventFilter(watched, event);
}

void KeyboardWidget::requestEditorFocus() {
    const HWND window = reinterpret_cast<HWND>(winId());
    m_editor->setFocus(Qt::OtherFocusReason);
    activateWindow();
    raise();
    SetForegroundWindow(window);

    if (GetForegroundWindow() == window) {
        SetFocus(window);
        m_editor->setFocus(Qt::OtherFocusReason);
        m_status->setText(QStringLiteral("App is foreground. Confirm the editor retains focus before typing."));
        appendLog(QStringLiteral("Focus request succeeded; foreground HWND belongs to this app."));
    } else {
        m_status->setText(QStringLiteral("Windows did not grant foreground focus. No virtual keys will be sent."));
        appendLog(QStringLiteral("Focus request was denied or redirected by Windows."));
    }
}

void KeyboardWidget::submitKey(WORD virtualKey, bool withShift) {
    if (!editorHasFocus()) {
        appendLog(QStringLiteral("Key blocked: Qt editor does not have focus. Click Focus input first."));
        return;
    }
    if (!m_keyCallback) {
        appendLog(QStringLiteral("Key sender is not initialized."));
        return;
    }

    QString error;
    if (!m_keyCallback(virtualKey, withShift, &error)) {
        appendLog(error);
    } else if (virtualKey == VK_IME_ON) {
        appendLog(QStringLiteral("Requested Japanese Hiragana mode (VK_IME_ON) through SendInput."));
    }
}

void KeyboardWidget::handleCandidateClick(int row) {
    if (row < 0 || !m_candidateCallback) {
        return;
    }
    QString error;
    if (m_candidateCallback(static_cast<UINT>(row), &error)) {
        appendLog(QStringLiteral("Requested candidate %1 selection and finalization.").arg(row));
    } else {
        appendLog(error);
    }
}

QWidget *KeyboardWidget::deepestChildAt(const QPoint &position, QPoint *childPosition) const {
    QWidget *target = const_cast<KeyboardWidget *>(this);
    QPoint localPosition = position;
    while (QWidget *child = target->childAt(localPosition)) {
        localPosition = child->mapFrom(target, localPosition);
        target = child;
    }
    if (childPosition) {
        *childPosition = localPosition;
    }
    return target;
}
