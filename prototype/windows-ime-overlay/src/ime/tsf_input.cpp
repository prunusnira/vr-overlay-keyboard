#include "tsf_input.h"

#include <wrl/client.h>

#include <array>
#include <utility>

using Microsoft::WRL::ComPtr;

class TsfInput::CandidateSink final : public ITfUIElementSink {
public:
    CandidateSink(ITfUIElementMgr *manager, CandidateCallback callback)
        : m_manager(manager), m_callback(std::move(callback)) {
        if (m_manager) {
            m_manager->AddRef();
        }
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (iid == IID_IUnknown || iid == __uuidof(ITfUIElementSink)) {
            *object = static_cast<ITfUIElementSink *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++m_refCount;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --m_refCount;
        if (remaining == 0) {
            delete this;
        }
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE BeginUIElement(DWORD elementId, BOOL *show) override {
        if (!show) {
            return E_POINTER;
        }

        ComPtr<ITfCandidateListUIElement> candidates;
        const HRESULT queryResult = findCandidateElement(elementId, &candidates);
        if (FAILED(queryResult)) {
            *show = TRUE;
            return S_OK;
        }

        m_activeElementId = elementId;
        *show = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE UpdateUIElement(DWORD elementId) override {
        ComPtr<ITfCandidateListUIElement> candidates;
        const HRESULT queryResult = findCandidateElement(elementId, &candidates);
        if (FAILED(queryResult)) {
            return S_OK;
        }

        m_activeElementId = elementId;
        CandidateSnapshot snapshot;
        snapshot.uiElementId = elementId;

        UINT count = 0;
        if (SUCCEEDED(candidates->GetCount(&count))) {
            for (UINT index = 0; index < count; ++index) {
                BSTR text = nullptr;
                if (SUCCEEDED(candidates->GetString(index, &text)) && text) {
                    snapshot.candidates.push_back(QString::fromWCharArray(text, SysStringLen(text)));
                    SysFreeString(text);
                } else {
                    snapshot.candidates.push_back(QString());
                }
            }
        }

        candidates->GetSelection(&snapshot.selectedIndex);
        candidates->GetCurrentPage(&snapshot.currentPage);

        UINT pageCount = 0;
        if (SUCCEEDED(candidates->GetPageIndex(nullptr, 0, &pageCount)) && pageCount > 0) {
            snapshot.pageStarts.resize(pageCount);
            if (FAILED(candidates->GetPageIndex(snapshot.pageStarts.data(), pageCount, &pageCount))) {
                snapshot.pageStarts.clear();
            }
        }

        if (m_callback) {
            m_callback(snapshot);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE EndUIElement(DWORD elementId) override {
        if (elementId == m_activeElementId) {
            m_activeElementId = TF_INVALID_UIELEMENTID;
            if (m_callback) {
                CandidateSnapshot empty;
                m_callback(empty);
            }
        }
        return S_OK;
    }

    DWORD activeElementId() const {
        return m_activeElementId;
    }

private:
    HRESULT findCandidateElement(DWORD elementId, ComPtr<ITfCandidateListUIElement> *result) const {
        if (!m_manager || !result) {
            return E_POINTER;
        }
        ComPtr<ITfUIElement> element;
        const HRESULT getResult = m_manager->GetUIElement(elementId, element.GetAddressOf());
        if (FAILED(getResult)) {
            return getResult;
        }
        return element.As(result);
    }

    ULONG m_refCount = 1;
    ITfUIElementMgr *m_manager = nullptr;
    CandidateCallback m_callback;
    DWORD m_activeElementId = TF_INVALID_UIELEMENTID;
};

TsfInput::TsfInput() = default;

TsfInput::~TsfInput() {
    shutdown();
}

void TsfInput::setCandidateCallback(CandidateCallback callback) {
    m_candidateCallback = std::move(callback);
}

void TsfInput::setStatusCallback(StatusCallback callback) {
    m_statusCallback = std::move(callback);
}

bool TsfInput::initialize(QString *errorMessage) {
    shutdown();

    HRESULT result = CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(m_threadManager.GetAddressOf()));
    if (FAILED(result)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("CoCreateInstance(CLSID_TF_ThreadMgr) failed: 0x%1")
                                .arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'));
        }
        return false;
    }

    result = m_threadManager->ActivateEx(&m_clientId, TF_TMAE_UIELEMENTENABLEDONLY);
    if (FAILED(result)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ITfThreadMgrEx::ActivateEx failed: 0x%1")
                                .arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'));
        }
        shutdown();
        return false;
    }
    m_activated = true;

    result = m_threadManager.As(&m_uiElementManager);
    if (FAILED(result)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ITfUIElementMgr is unavailable: 0x%1")
                                .arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'));
        }
        shutdown();
        return false;
    }

    result = m_uiElementManager.As(&m_source);
    if (FAILED(result)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ITfSource is unavailable: 0x%1")
                                .arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'));
        }
        shutdown();
        return false;
    }

    m_sink = new CandidateSink(m_uiElementManager.Get(), m_candidateCallback);
    result = m_source->AdviseSink(__uuidof(ITfUIElementSink), m_sink, &m_sinkCookie);
    if (FAILED(result)) {
        m_sink->Release();
        m_sink = nullptr;
        m_sinkCookie = TF_INVALID_COOKIE;
        if (errorMessage) {
            *errorMessage = QStringLiteral("ITfUIElementSink registration failed: 0x%1")
                                .arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'));
        }
        shutdown();
        return false;
    }

    reportStatus(QStringLiteral("TSF UI-less candidate sink registered."));
    return true;
}

void TsfInput::shutdown() {
    if (m_source && m_sinkCookie != TF_INVALID_COOKIE) {
        m_source->UnadviseSink(m_sinkCookie);
    }
    m_sinkCookie = TF_INVALID_COOKIE;
    if (m_sink) {
        m_sink->Release();
        m_sink = nullptr;
    }
    m_source.Reset();
    m_uiElementManager.Reset();

    if (m_threadManager && m_activated) {
        m_threadManager->Deactivate();
    }
    m_activated = false;
    m_clientId = TF_CLIENTID_NULL;
    m_threadManager.Reset();
}

bool TsfInput::sendVirtualKey(WORD virtualKey, bool withShift, QString *errorMessage) const {
    const HWND foregroundWindow = GetForegroundWindow();
    DWORD foregroundProcessId = 0;
    if (foregroundWindow) {
        GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId);
    }
    if (foregroundProcessId != GetCurrentProcessId()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Key blocked: this app does not own the Windows foreground window.");
        }
        return false;
    }

    std::array<INPUT, 4> inputs{};
    UINT count = 0;
    // Preserve the physical key position so the active IME can map it like hardware input.
    const bool useScanCode = (virtualKey >= 'A' && virtualKey <= 'Z') ||
                             virtualKey == VK_BACK || virtualKey == VK_SPACE ||
                             virtualKey == VK_RETURN;
    auto appendKey = [&inputs, &count](WORD key, DWORD flags, bool scanCode) {
        INPUT &input = inputs[count++];
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = key;
        input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(key, MAPVK_VK_TO_VSC));
        input.ki.dwFlags = flags | (scanCode ? KEYEVENTF_SCANCODE : 0);
        input.ki.time = 0;
        input.ki.dwExtraInfo = 0;
    };

    if (withShift) {
        appendKey(VK_SHIFT, 0, useScanCode);
    }
    appendKey(virtualKey, 0, useScanCode);
    appendKey(virtualKey, KEYEVENTF_KEYUP, useScanCode);
    if (withShift) {
        appendKey(VK_SHIFT, KEYEVENTF_KEYUP, useScanCode);
    }

    const UINT sent = SendInput(count, inputs.data(), sizeof(INPUT));
    if (sent != count) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("SendInput sent %1 of %2 events (Windows error %3).")
                                .arg(sent)
                                .arg(count)
                                .arg(GetLastError());
        }
        return false;
    }
    return true;
}

bool TsfInput::selectCandidate(UINT index, QString *errorMessage) {
    if (!m_uiElementManager || !m_sink || m_sink->activeElementId() == TF_INVALID_UIELEMENTID) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No active TSF candidate-list UI element.");
        }
        return false;
    }

    ComPtr<ITfUIElement> element;
    HRESULT result = m_uiElementManager->GetUIElement(m_sink->activeElementId(), element.GetAddressOf());
    if (FAILED(result)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not retrieve the active candidate UI element.");
        }
        return false;
    }

    ComPtr<ITfCandidateListUIElementBehavior> behavior;
    result = element.As(&behavior);
    if (FAILED(result)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("This IME does not expose candidate selection behavior.");
        }
        return false;
    }

    result = behavior->SetSelection(index);
    if (SUCCEEDED(result)) {
        result = behavior->Finalize();
    }
    if (FAILED(result)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("IME candidate selection/finalization failed: 0x%1")
                                .arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'));
        }
        return false;
    }
    return true;
}

void TsfInput::reportStatus(const QString &message) const {
    if (m_statusCallback) {
        m_statusCallback(message);
    }
}
