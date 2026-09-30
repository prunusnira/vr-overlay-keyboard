#pragma once

#include <Windows.h>
#include <msctf.h>

#include <QString>
#include <QStringList>

#include <wrl/client.h>

#include <functional>
#include <memory>
#include <vector>

struct CandidateSnapshot {
    DWORD uiElementId = TF_INVALID_UIELEMENTID;
    QStringList candidates;
    UINT selectedIndex = 0;
    UINT currentPage = 0;
    std::vector<UINT> pageStarts;
};

class TsfInput final {
public:
    using CandidateCallback = std::function<void(const CandidateSnapshot &)>;
    using StatusCallback = std::function<void(const QString &)>;

    TsfInput();
    ~TsfInput();

    TsfInput(const TsfInput &) = delete;
    TsfInput &operator=(const TsfInput &) = delete;

    void setCandidateCallback(CandidateCallback callback);
    void setStatusCallback(StatusCallback callback);

    bool initialize(QString *errorMessage);
    void shutdown();

    bool sendVirtualKey(WORD virtualKey, bool withShift, QString *errorMessage) const;
    bool selectCandidate(UINT index, QString *errorMessage);

private:
    class CandidateSink;

    void reportStatus(const QString &message) const;

    Microsoft::WRL::ComPtr<ITfThreadMgrEx> m_threadManager;
    Microsoft::WRL::ComPtr<ITfUIElementMgr> m_uiElementManager;
    Microsoft::WRL::ComPtr<ITfSource> m_source;
    CandidateSink *m_sink = nullptr;
    TfClientId m_clientId = TF_CLIENTID_NULL;
    DWORD m_sinkCookie = TF_INVALID_COOKIE;
    bool m_activated = false;
    CandidateCallback m_candidateCallback;
    StatusCallback m_statusCallback;
};
