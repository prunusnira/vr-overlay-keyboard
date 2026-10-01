#include "tsf_input.h"

#include <Windows.h>
#include <msctf.h>
#include <wrl/client.h>

#include <atomic>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {
using Microsoft::WRL::ComPtr;

std::string wideToUtf8(const wchar_t *text, int length) {
    if (!text || length <= 0) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, length, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, length, result.data(), size, nullptr, nullptr);
    return result;
}

void setHresultError(std::string *error, const char *operation, HRESULT result) {
    if (error) {
        std::ostringstream description;
        description << operation << " failed with HRESULT 0x" << std::hex << std::uppercase
                    << static_cast<unsigned long>(result) << ".";
        *error = description.str();
    }
}

class CandidateSink final : public ITfUIElementSink {
public:
    CandidateSink(ITfUIElementMgr *manager, TsfInput::CandidateCallback callback)
        : m_manager(manager), m_callback(std::move(callback)) {}

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
        return m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = m_refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (remaining == 0) {
            delete this;
        }
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE BeginUIElement(DWORD elementId, BOOL *show) override {
        if (!show) {
            return E_POINTER;
        }
        ComPtr<ITfCandidateListUIElement> candidateElement;
        if (FAILED(getCandidateElement(elementId, &candidateElement))) {
            *show = TRUE;
            return S_OK;
        }
        m_activeElementId = elementId;
        // Windows가 별도 후보 창을 띄우지 않게 하고, 후보 데이터는 앱 UI에서 직접 그린다.
        *show = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE UpdateUIElement(DWORD elementId) override {
        ComPtr<ITfCandidateListUIElement> candidateElement;
        if (FAILED(getCandidateElement(elementId, &candidateElement))) {
            return S_OK;
        }

        m_activeElementId = elementId;
        keyboard::CandidateSnapshot snapshot;
        snapshot.active = true;
        // TSF 문자열은 UTF-16 BSTR이므로 앱 계약에 맞춰 UTF-8 snapshot으로 복사한다.
        UINT count = 0;
        if (SUCCEEDED(candidateElement->GetCount(&count))) {
            snapshot.candidates.reserve(count);
            for (UINT index = 0; index < count; ++index) {
                BSTR candidate = nullptr;
                if (SUCCEEDED(candidateElement->GetString(index, &candidate)) && candidate) {
                    snapshot.candidates.push_back(wideToUtf8(candidate, static_cast<int>(SysStringLen(candidate))));
                    SysFreeString(candidate);
                } else {
                    snapshot.candidates.emplace_back();
                }
            }
        }

        candidateElement->GetSelection(&snapshot.selectedIndex);
        candidateElement->GetCurrentPage(&snapshot.currentPage);
        UINT pageCount = 0;
        if (SUCCEEDED(candidateElement->GetPageIndex(nullptr, 0, &pageCount)) && pageCount > 0) {
            snapshot.pageStarts.resize(pageCount);
            if (FAILED(candidateElement->GetPageIndex(snapshot.pageStarts.data(), pageCount, &pageCount))) {
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
                m_callback(keyboard::CandidateSnapshot{});
            }
        }
        return S_OK;
    }

    DWORD activeElementId() const {
        return m_activeElementId;
    }

private:
    HRESULT getCandidateElement(DWORD elementId, ComPtr<ITfCandidateListUIElement> *result) const {
        if (!m_manager || !result) {
            return E_POINTER;
        }
        ComPtr<ITfUIElement> element;
        const HRESULT getResult = m_manager->GetUIElement(elementId, element.GetAddressOf());
        return FAILED(getResult) ? getResult : element.As(result);
    }

    std::atomic<ULONG> m_refCount{1};
    ComPtr<ITfUIElementMgr> m_manager;
    TsfInput::CandidateCallback m_callback;
    DWORD m_activeElementId = TF_INVALID_UIELEMENTID;
};
}

struct TsfInput::Impl {
    ComPtr<ITfThreadMgrEx> threadManager;
    ComPtr<ITfUIElementMgr> uiElementManager;
    ComPtr<ITfSource> source;
    CandidateSink *sink = nullptr;
    TfClientId clientId = TF_CLIENTID_NULL;
    DWORD sinkCookie = TF_INVALID_COOKIE;
    bool activated = false;
};

TsfInput::TsfInput() : m_impl(std::make_unique<Impl>()) {}

TsfInput::~TsfInput() {
    shutdown();
}

void TsfInput::setCandidateCallback(CandidateCallback callback) {
    m_candidateCallback = std::move(callback);
}

bool TsfInput::initialize(std::string *error) {
    shutdown();
    HRESULT result = CoCreateInstance(CLSID_TF_ThreadMgr,
                                      nullptr,
                                      CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(m_impl->threadManager.GetAddressOf()));
    if (FAILED(result)) {
        setHresultError(error, "CoCreateInstance(CLSID_TF_ThreadMgr)", result);
        return false;
    }

    result = m_impl->threadManager->ActivateEx(&m_impl->clientId, TF_TMAE_UIELEMENTENABLEDONLY);
    if (FAILED(result)) {
        setHresultError(error, "ITfThreadMgrEx::ActivateEx", result);
        shutdown();
        return false;
    }
    m_impl->activated = true;

    result = m_impl->threadManager.As(&m_impl->uiElementManager);
    if (FAILED(result)) {
        setHresultError(error, "QueryInterface(ITfUIElementMgr)", result);
        shutdown();
        return false;
    }
    result = m_impl->uiElementManager.As(&m_impl->source);
    if (FAILED(result)) {
        setHresultError(error, "QueryInterface(ITfSource)", result);
        shutdown();
        return false;
    }

    m_impl->sink = new CandidateSink(m_impl->uiElementManager.Get(), m_candidateCallback);
    result = m_impl->source->AdviseSink(__uuidof(ITfUIElementSink), m_impl->sink, &m_impl->sinkCookie);
    if (FAILED(result)) {
        m_impl->sink->Release();
        m_impl->sink = nullptr;
        m_impl->sinkCookie = TF_INVALID_COOKIE;
        setHresultError(error, "ITfUIElementSink registration", result);
        shutdown();
        return false;
    }
    return true;
}

void TsfInput::shutdown() {
    if (!m_impl) {
        return;
    }
    if (m_impl->source && m_impl->sinkCookie != TF_INVALID_COOKIE) {
        m_impl->source->UnadviseSink(m_impl->sinkCookie);
    }
    m_impl->sinkCookie = TF_INVALID_COOKIE;
    if (m_impl->sink) {
        m_impl->sink->Release();
        m_impl->sink = nullptr;
    }
    m_impl->source.Reset();
    m_impl->uiElementManager.Reset();
    if (m_impl->threadManager && m_impl->activated) {
        m_impl->threadManager->Deactivate();
    }
    m_impl->activated = false;
    m_impl->clientId = TF_CLIENTID_NULL;
    m_impl->threadManager.Reset();
}

bool TsfInput::selectCandidate(std::uint32_t index, std::string *error) {
    if (!m_impl->uiElementManager || !m_impl->sink ||
        m_impl->sink->activeElementId() == TF_INVALID_UIELEMENTID) {
        if (error) {
            *error = "There is no active TSF candidate list.";
        }
        return false;
    }

    ComPtr<ITfUIElement> element;
    HRESULT result = m_impl->uiElementManager->GetUIElement(
        m_impl->sink->activeElementId(), element.GetAddressOf());
    if (FAILED(result)) {
        setHresultError(error, "GetUIElement", result);
        return false;
    }

    ComPtr<ITfCandidateListUIElementBehavior> behavior;
    result = element.As(&behavior);
    if (FAILED(result)) {
        if (error) {
            *error = "The active IME does not expose candidate selection behavior.";
        }
        return false;
    }

    // 선택 인덱스를 먼저 적용한 뒤 Finalize를 호출해 현재 조합을 확정한다.
    result = behavior->SetSelection(index);
    if (SUCCEEDED(result)) {
        result = behavior->Finalize();
    }
    if (FAILED(result)) {
        setHresultError(error, "Candidate selection/finalization", result);
        return false;
    }
    return true;
}
