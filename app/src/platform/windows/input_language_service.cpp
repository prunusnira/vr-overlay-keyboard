#include "input_language_service.h"

#include <Windows.h>

#include <algorithm>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace {
std::string languageId(std::uintptr_t layout) {
    std::ostringstream value;
    value << "windows-layout-" << std::hex << layout;
    return value.str();
}

std::string localizedLanguageName(HKL layout) {
    const LANGID language = LOWORD(reinterpret_cast<ULONG_PTR>(layout));
    const LCID locale = MAKELCID(language, SORT_DEFAULT);
    wchar_t name[128] = {};
    const int length = GetLocaleInfoW(locale, LOCALE_SLANGUAGE, name, static_cast<int>(std::size(name)));
    if (length <= 1) {
        return "Windows input language";
    }
    const int byteCount = WideCharToMultiByte(CP_UTF8, 0, name, length - 1, nullptr, 0, nullptr, nullptr);
    if (byteCount <= 0) {
        return "Windows input language";
    }
    std::string result(static_cast<std::size_t>(byteCount), '\0');
    WideCharToMultiByte(CP_UTF8, 0, name, length - 1, result.data(), byteCount, nullptr, nullptr);
    return result;
}
}

std::vector<keyboard::InputLanguage> WindowsInputLanguageService::loadedLanguages() {
    m_layouts.clear();
    const int count = GetKeyboardLayoutList(0, nullptr);
    if (count <= 0) {
        return {};
    }

    std::vector<HKL> layouts(static_cast<std::size_t>(count));
    const int copied = GetKeyboardLayoutList(count, layouts.data());
    if (copied <= 0) {
        return {};
    }
    layouts.resize(static_cast<std::size_t>(copied));

    const HKL current = GetKeyboardLayout(0);
    if (std::find(layouts.begin(), layouts.end(), current) == layouts.end()) {
        layouts.push_back(current);
    }

    std::vector<keyboard::InputLanguage> languages;
    languages.reserve(layouts.size());
    for (HKL layout : layouts) {
        const std::uintptr_t rawLayout = reinterpret_cast<std::uintptr_t>(layout);
        const std::string id = languageId(rawLayout);
        m_layouts.emplace(id, rawLayout);
        languages.push_back({id, localizedLanguageName(layout), layout == current});
    }
    return languages;
}

bool WindowsInputLanguageService::activate(const std::string &id, std::string *error) {
    const auto found = m_layouts.find(id);
    if (found == m_layouts.end()) {
        if (error) {
            *error = "The selected Windows input language is no longer available.";
        }
        return false;
    }

    const HKL layout = reinterpret_cast<HKL>(found->second);
    if (!ActivateKeyboardLayout(layout, 0)) {
        if (error) {
            *error = "Windows could not activate the selected input language (error " +
                     std::to_string(GetLastError()) + ").";
        }
        return false;
    }
    return true;
}
