#pragma once

#include "../core/app_contracts.h"

#include <functional>
#include <memory>
#include <string>

class TsfInput final : public keyboard::CandidateSelectionPort {
public:
    using CandidateCallback = std::function<void(const keyboard::CandidateSnapshot &)>;

    TsfInput();
    ~TsfInput() override;

    TsfInput(const TsfInput &) = delete;
    TsfInput &operator=(const TsfInput &) = delete;

    void setCandidateCallback(CandidateCallback callback);
    bool initialize(std::string *error);
    void shutdown();
    bool selectCandidate(std::uint32_t index, std::string *error) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    CandidateCallback m_candidateCallback;
};
