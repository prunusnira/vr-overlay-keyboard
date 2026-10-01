#include "keyboard_widget.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QColor>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <utility>

namespace {
QString fromUtf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::string toUtf8(const QString &text) {
    const QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::string candidateSignature(const keyboard::CandidateSnapshot &snapshot) {
    std::ostringstream signature;
    signature << snapshot.active << ':' << snapshot.selectedIndex << ':' << snapshot.currentPage;
    for (const std::string &candidate : snapshot.candidates) {
        signature << ':' << candidate.size() << ':' << candidate;
    }
    return signature.str();
}

Qt::MouseButton toQtButton(keyboard::PointerButton button) {
    switch (button) {
    case keyboard::PointerButton::Left: return Qt::LeftButton;
    case keyboard::PointerButton::Right: return Qt::RightButton;
    default: return Qt::NoButton;
    }
}

keyboard::KeyCode letterKey(QChar letter) {
    const int offset = letter.toUpper().unicode() - QLatin1Char('A').unicode();
    return static_cast<keyboard::KeyCode>(static_cast<int>(keyboard::KeyCode::A) + offset);
}
}

KeyboardWidget::KeyboardWidget(keyboard::KeyboardActions &actions, QWidget *parent)
    : QWidget(parent), m_actions(actions) {
    setWindowTitle(QStringLiteral("VR Overlay Keyboard"));
    setFixedSize(1024, 1024);
    setStyleSheet(QStringLiteral(
        "QWidget { background: #161a22; color: #f1f5f9; font-size: 16px; }"
        "QLineEdit { background: #ffffff; color: #111827; padding: 10px; font-size: 24px; }"
        "QPushButton { background: #303949; border: 1px solid #667085; border-radius: 6px; padding: 8px; }"
        "QPushButton:pressed, QPushButton:checked { background: #2563eb; border-color: #60a5fa; }"
        "QListWidget, QPlainTextEdit { background: #0b0e13; border: 1px solid #475467; }"
        "QListWidget::item:selected { background: #2563eb; }"
    ));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("VR Overlay Keyboard"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto *description = new QLabel(
        QStringLiteral("Type with the active Windows IME. Bind SteamVR toggle, pointer pose, and click actions to use the overlay in VR."), this);
    description->setWordWrap(true);
    root->addWidget(description);

    m_editor = new QLineEdit(this);
    m_editor->setPlaceholderText(QStringLiteral("Compose text with the selected Windows input method"));
    m_editor->setFocusPolicy(Qt::StrongFocus);
    m_editor->installEventFilter(this);
    root->addWidget(m_editor);

    m_languageLabel = new QLabel(QStringLiteral("Current Windows input language: detecting..."), this);
    root->addWidget(m_languageLabel);
    m_languageChoices = new QWidget(this);
    m_languageGrid = new QGridLayout(m_languageChoices);
    m_languageGrid->setContentsMargins(0, 0, 0, 0);
    m_languageGrid->setSpacing(5);
    m_languageButtons = new QButtonGroup(this);
    m_languageButtons->setExclusive(true);
    root->addWidget(m_languageChoices);

    auto *controls = new QHBoxLayout();
    auto *focusButton = new QPushButton(QStringLiteral("Focus input"), this);
    m_toggleOverlayButton = new QPushButton(QStringLiteral("Show keyboard overlay"), this);
    auto *clearButton = new QPushButton(QStringLiteral("Clear input"), this);
    auto *sendChatboxButton = new QPushButton(QStringLiteral("Fill VRChat Chatbox"), this);
    for (QAbstractButton *button : {focusButton, m_toggleOverlayButton, clearButton, sendChatboxButton}) {
        button->setFocusPolicy(Qt::NoFocus);
        button->setMinimumHeight(48);
    }
    controls->addWidget(focusButton);
    controls->addWidget(m_toggleOverlayButton);
    controls->addWidget(clearButton);
    controls->addWidget(sendChatboxButton);
    root->addLayout(controls);

    m_status = new QLabel(QStringLiteral("Waiting for Windows IME and SteamVR."), this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);
    m_focusStatus = new QLabel(QStringLiteral("Windows foreground: checking | Editor focus: checking"), this);
    m_focusStatus->setWordWrap(true);
    m_focusStatus->setStyleSheet(QStringLiteral(
        "QLabel { background: #26364a; border: 1px solid #60a5fa; border-radius: 5px; padding: 7px; font-weight: 700; }"));
    root->addWidget(m_focusStatus);

    m_candidateHeading = new QLabel(QStringLiteral("IME candidates"), this);
    root->addWidget(m_candidateHeading);
    m_candidates = new QListWidget(this);
    m_candidates->setSelectionMode(QAbstractItemView::SingleSelection);
    m_candidates->setFocusPolicy(Qt::NoFocus);
    m_candidates->setMaximumHeight(150);
    root->addWidget(m_candidates);

    auto *keyboardHeading = new QLabel(QStringLiteral("Keyboard"), this);
    root->addWidget(keyboardHeading);
    auto *compositionGuide = new QLabel(
        QStringLiteral("Prototype baseline: controller pointer, language switching, Japanese IME mode switching, and Chatbox OSC. Korean composition and IME candidates still need validation."), this);
    compositionGuide->setWordWrap(true);
    root->addWidget(compositionGuide);

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
            key->setMinimumSize(70, 44);
            keyboard->addWidget(key, row, column + rowOffsets[row]);
            connect(key, &QPushButton::clicked, this, [this, letter]() {
                sendKey(letterKey(letter), m_shiftForNextKey);
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
        button->setMinimumHeight(44);
    }
    keyboard->addWidget(m_shiftButton, 2, 0, 1, 2);
    keyboard->addWidget(backspaceButton, 2, 9, 1, 3);
    keyboard->addWidget(hangulButton, 3, 0, 1, 2);
    keyboard->addWidget(spaceButton, 3, 2, 1, 4);
    keyboard->addWidget(enterButton, 3, 6, 1, 2);
    keyboard->addWidget(kanaButton, 3, 8, 1, 2);
    keyboard->addWidget(kanjiButton, 3, 10, 1, 2);
    root->addLayout(keyboard);

    auto *logHeading = new QLabel(QStringLiteral("Input diagnostics"), this);
    root->addWidget(logHeading);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(80);
    root->addWidget(m_log, 1);

    connect(focusButton, &QPushButton::clicked, this, [this]() { requestEditorFocus(); });
    connect(m_toggleOverlayButton, &QPushButton::clicked, this, [this]() { m_actions.toggleOverlay(); });
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        m_editor->clear();
        appendLog(QStringLiteral("Editor cleared."));
    });
    connect(sendChatboxButton, &QPushButton::clicked, this, [this]() {
        if (m_actions.submitChatboxText(toUtf8(m_editor->text()))) {
            appendLog(QStringLiteral("OSC request sent to 127.0.0.1:9000: /chatbox/input (send=false)."));
        }
    });
    connect(m_shiftButton, &QPushButton::clicked, this, [this]() {
        m_shiftForNextKey = !m_shiftForNextKey;
        m_shiftButton->setText(m_shiftForNextKey ? QStringLiteral("Shift (on)") : QStringLiteral("Shift"));
    });
    connect(backspaceButton, &QPushButton::clicked, this, [this]() { sendKey(keyboard::KeyCode::Backspace); });
    connect(spaceButton, &QPushButton::clicked, this, [this]() { sendKey(keyboard::KeyCode::Space); });
    connect(enterButton, &QPushButton::clicked, this, [this]() { sendKey(keyboard::KeyCode::Enter); });
    connect(hangulButton, &QPushButton::clicked, this, [this]() { sendKey(keyboard::KeyCode::HangulMode); });
    connect(kanaButton, &QPushButton::clicked, this, [this]() {
        sendKey(keyboard::KeyCode::JapaneseHiraganaMode);
        appendLog(QStringLiteral("Requested Japanese Hiragana mode."));
    });
    connect(kanjiButton, &QPushButton::clicked, this, [this]() { sendKey(keyboard::KeyCode::JapaneseKanjiMode); });
    connect(m_candidates, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const int row = m_candidates->row(item);
        if (row >= 0) {
            m_actions.selectCandidate(static_cast<std::uint32_t>(row));
        }
    });
    for (QAbstractButton *button : findChildren<QAbstractButton *>()) {
        button->installEventFilter(this);
    }

    m_candidateHeading->hide();
    m_candidates->hide();
    appendLog(QStringLiteral("Ready. Focus the input field before sending virtual keys."));
}

keyboard::ImageFrame KeyboardWidget::renderFrame() {
    QImage image(size(), QImage::Format_RGBA8888);
    image.fill(QColor(22, 26, 34, 255));
    QPainter painter(&image);
    render(&painter);
    keyboard::ImageFrame frame;
    frame.width = static_cast<std::uint32_t>(image.width());
    frame.height = static_cast<std::uint32_t>(image.height());
    const std::size_t rowBytes = static_cast<std::size_t>(frame.width) * 4;
    frame.rgbaPixels.resize(rowBytes * frame.height);
    for (std::uint32_t row = 0; row < frame.height; ++row) {
        std::memcpy(frame.rgbaPixels.data() + rowBytes * row,
                    image.constScanLine(static_cast<int>(row)),
                    rowBytes);
    }
    return frame;
}

void KeyboardWidget::dispatchOverlayPointerEvent(const keyboard::PointerEvent &event) {
    if (event.type == keyboard::PointerEventType::Cancel) {
        if (m_pressedPointerButton) {
            m_pressedPointerButton->setDown(false);
            m_pressedPointerButton = nullptr;
        }
        if (m_pressedPointerTarget) {
            const QPoint globalPosition = m_pressedPointerTarget->mapToGlobal(m_lastPointerTargetPosition);
            QMouseEvent releaseEvent(QEvent::MouseButtonRelease,
                                     QPointF(m_lastPointerTargetPosition),
                                     QPointF(globalPosition),
                                     Qt::LeftButton,
                                     Qt::NoButton,
                                     Qt::NoModifier);
            QApplication::sendEvent(m_pressedPointerTarget, &releaseEvent);
        }
        m_pressedPointerTarget = nullptr;
        m_pressedPointerButtons = Qt::NoButton;
        return;
    }
    if (event.type == keyboard::PointerEventType::Leave) {
        if (m_pressedPointerButton) {
            m_pressedPointerButton->setDown(false);
        }
        return;
    }

    const Qt::MouseButton button = toQtButton(event.button);
    if (event.type == keyboard::PointerEventType::Press) {
        m_pressedPointerButtons |= button;
    } else if (event.type == keyboard::PointerEventType::Release) {
        m_pressedPointerButtons &= ~button;
    }

    QPoint childPosition;
    QWidget *target = deepestChildAt(QPoint(event.x, event.y), &childPosition);
    auto *clickedButton = qobject_cast<QAbstractButton *>(target);

    if (event.type == keyboard::PointerEventType::Move && m_pressedPointerButton) {
        m_pressedPointerButton->setDown(clickedButton == m_pressedPointerButton);
        return;
    }
    if (event.type == keyboard::PointerEventType::Release && button == Qt::LeftButton && m_pressedPointerButton) {
        QAbstractButton *pressedButton = m_pressedPointerButton;
        m_pressedPointerButton = nullptr;
        const bool releasedOnSameButton = clickedButton == pressedButton;
        pressedButton->setDown(false);
        if (releasedOnSameButton && pressedButton->isEnabled()) {
            // Button actions do not deliver a Qt mouse release to the active IME editor.
            pressedButton->click();
        }
        return;
    }
    if (event.type == keyboard::PointerEventType::Press && button == Qt::LeftButton && clickedButton) {
        if (m_pressedPointerButton) {
            m_pressedPointerButton->setDown(false);
        }
        m_pressedPointerButton = clickedButton;
        clickedButton->setDown(true);
        return;
    }
    if (!target || (button == Qt::NoButton && event.type != keyboard::PointerEventType::Move)) {
        return;
    }

    QEvent::Type eventType = QEvent::MouseMove;
    if (event.type == keyboard::PointerEventType::Press) {
        eventType = QEvent::MouseButtonPress;
    } else if (event.type == keyboard::PointerEventType::Release) {
        eventType = QEvent::MouseButtonRelease;
    }
    const QPoint globalPosition = target->mapToGlobal(childPosition);
    if (event.type == keyboard::PointerEventType::Press) {
        m_pressedPointerTarget = target;
        m_lastPointerTargetPosition = childPosition;
    } else if (event.type == keyboard::PointerEventType::Move && m_pressedPointerTarget == target) {
        m_lastPointerTargetPosition = childPosition;
    } else if (event.type == keyboard::PointerEventType::Release) {
        m_pressedPointerTarget = nullptr;
    }
    QMouseEvent mouseEvent(eventType,
                           QPointF(childPosition),
                           QPointF(globalPosition),
                           button,
                           m_pressedPointerButtons,
                           Qt::NoModifier);
    QApplication::sendEvent(target, &mouseEvent);
}

void KeyboardWidget::setAppState(const keyboard::AppUiState &state) {
    m_status->setText(fromUtf8(state.status));
    m_toggleOverlayButton->setText(state.overlayVisible
                                       ? QStringLiteral("Hide keyboard overlay")
                                       : QStringLiteral("Show keyboard overlay"));

    std::vector<std::string> ids;
    std::vector<std::string> labels;
    ids.reserve(state.inputLanguages.size());
    labels.reserve(state.inputLanguages.size());
    for (const keyboard::InputLanguage &language : state.inputLanguages) {
        ids.push_back(language.id);
        labels.push_back(language.label);
    }
    if (ids != m_languageIds || labels != m_languageLabels) {
        rebuildInputLanguages(state.inputLanguages);
    }
    for (const keyboard::InputLanguage &language : state.inputLanguages) {
        if (language.active) {
            m_languageLabel->setText(QStringLiteral("Current Windows input language: %1").arg(fromUtf8(language.label)));
            for (QAbstractButton *button : m_languageButtons->buttons()) {
                if (button->property("languageId").toString() == fromUtf8(language.id)) {
                    button->setChecked(true);
                    break;
                }
            }
            break;
        }
    }
    updateCandidates(state.candidates);
}

void KeyboardWidget::setFocusRequestCallback(FocusRequestCallback callback) {
    m_focusRequestCallback = std::move(callback);
}

void KeyboardWidget::refreshFocusStatus(bool appIsForeground) {
    m_focusStatus->setText(QStringLiteral("Windows foreground: %1 | Editor focus: %2")
                               .arg(appIsForeground ? QStringLiteral("this app") : QStringLiteral("another app"))
                               .arg(editorHasFocus() ? QStringLiteral("yes") : QStringLiteral("no")));
}

void KeyboardWidget::prepareEditorFocus() {
    m_editor->setFocus(Qt::OtherFocusReason);
    activateWindow();
    raise();
}

bool KeyboardWidget::editorHasFocus() const {
    return QApplication::focusWidget() == m_editor;
}

void KeyboardWidget::appendLog(const QString &message) {
    m_log->appendPlainText(message);
}

bool KeyboardWidget::eventFilter(QObject *watched, QEvent *event) {
    auto *button = qobject_cast<QAbstractButton *>(watched);
    auto *mouseEvent = event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove ||
                               event->type() == QEvent::MouseButtonRelease
        ? static_cast<QMouseEvent *>(event)
        : nullptr;
    if (button && mouseEvent && event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton) {
        if (m_pressedMouseButton) {
            m_pressedMouseButton->setDown(false);
        }
        m_pressedMouseButton = button;
        button->setDown(true);
        return true;
    }
    if (button && mouseEvent && event->type() == QEvent::MouseMove && m_pressedMouseButton == button) {
        button->setDown(button->rect().contains(mouseEvent->position().toPoint()));
        return true;
    }
    if (button && mouseEvent && event->type() == QEvent::MouseButtonRelease && m_pressedMouseButton) {
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
        appendLog(QStringLiteral("Editor focus: FocusIn."));
    } else if (watched == m_editor && event->type() == QEvent::FocusOut) {
        appendLog(QStringLiteral("Editor focus: FocusOut."));
    } else if (watched == m_editor && event->type() == QEvent::KeyPress) {
        const auto *keyEvent = static_cast<const QKeyEvent *>(event);
        appendLog(QStringLiteral("Editor key: 0x%1, text=[%2]")
                      .arg(static_cast<quint32>(keyEvent->key()), 0, 16)
                      .arg(keyEvent->text()));
    } else if (watched == m_editor && event->type() == QEvent::InputMethod) {
        const auto *inputMethodEvent = static_cast<const QInputMethodEvent *>(event);
        appendLog(QStringLiteral("IME event: preedit=[%1] commit=[%2]")
                      .arg(inputMethodEvent->preeditString())
                      .arg(inputMethodEvent->commitString()));
    }
    return QWidget::eventFilter(watched, event);
}

void KeyboardWidget::requestEditorFocus() {
    prepareEditorFocus();
    std::string error;
    if (!m_focusRequestCallback) {
        m_status->setText(QStringLiteral("Windows foreground focus support is unavailable."));
        appendLog(QStringLiteral("Focus request failed: no Windows focus adapter is connected."));
        return;
    }
    if (!m_focusRequestCallback(&error)) {
        m_status->setText(fromUtf8(error));
        appendLog(fromUtf8(error));
        return;
    }
    m_editor->setFocus(Qt::OtherFocusReason);
    m_status->setText(QStringLiteral("App is foreground. Confirm the editor keeps focus before typing."));
    appendLog(QStringLiteral("Focus request succeeded."));
}

void KeyboardWidget::sendKey(keyboard::KeyCode key, bool withShift) {
    if (!editorHasFocus()) {
        appendLog(QStringLiteral("Key blocked: the editor does not have focus. Click Focus input first."));
        return;
    }
    m_actions.sendKey(key, withShift);
}

void KeyboardWidget::rebuildInputLanguages(const std::vector<keyboard::InputLanguage> &languages) {
    while (QLayoutItem *item = m_languageGrid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    delete m_languageButtons;
    m_languageButtons = new QButtonGroup(this);
    m_languageButtons->setExclusive(true);
    m_languageIds.clear();
    m_languageLabels.clear();

    for (std::size_t index = 0; index < languages.size(); ++index) {
        const keyboard::InputLanguage &language = languages[index];
        m_languageIds.push_back(language.id);
        m_languageLabels.push_back(language.label);
        auto *button = new QPushButton(fromUtf8(language.label), m_languageChoices);
        button->setCheckable(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->installEventFilter(this);
        button->setMinimumHeight(42);
        button->setProperty("languageId", fromUtf8(language.id));
        button->setChecked(language.active);
        m_languageButtons->addButton(button);
        m_languageGrid->addWidget(button, static_cast<int>(index / 3), static_cast<int>(index % 3));
        const std::string id = language.id;
        connect(button, &QPushButton::clicked, this, [this, id]() {
            m_actions.activateInputLanguage(id);
        });
    }
    const int rows = static_cast<int>((languages.size() + 2) / 3);
    m_languageChoices->setFixedHeight(rows > 0 ? rows * 42 + (rows - 1) * 5 : 0);
}

void KeyboardWidget::updateCandidates(const keyboard::CandidateSnapshot &snapshot) {
    const std::string signature = candidateSignature(snapshot);
    m_candidateHeading->setVisible(snapshot.active);
    m_candidates->setVisible(snapshot.active);
    if (signature == m_candidateSignature) {
        return;
    }
    m_candidateSignature = signature;
    m_candidates->clear();
    for (const std::string &candidate : snapshot.candidates) {
        m_candidates->addItem(fromUtf8(candidate));
    }
    if (snapshot.selectedIndex < static_cast<std::uint32_t>(m_candidates->count())) {
        m_candidates->setCurrentRow(static_cast<int>(snapshot.selectedIndex));
    }
    if (snapshot.active) {
        appendLog(QStringLiteral("TSF candidate list: %1 entries, selected %2, page %3.")
                      .arg(static_cast<qulonglong>(snapshot.candidates.size()))
                      .arg(snapshot.selectedIndex)
                      .arg(snapshot.currentPage));
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
