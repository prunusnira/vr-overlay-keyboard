#include "settings_store.h"

#include <Windows.h>

#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
struct ButtonSettingName {
    keyboard::ControllerButton button;
    const char *name;
};

constexpr std::array<ButtonSettingName, keyboard::kControllerButtons.size()> kButtonNames = {{
    {keyboard::ControllerButton::LeftGrip, "LeftGrip"},
    {keyboard::ControllerButton::LeftTrigger, "LeftTrigger"},
    {keyboard::ControllerButton::LeftA, "LeftA"},
    {keyboard::ControllerButton::LeftB, "LeftB"},
    {keyboard::ControllerButton::LeftMenu, "LeftMenu"},
    {keyboard::ControllerButton::LeftJoystick, "LeftJoystick"},
    {keyboard::ControllerButton::LeftTrackpad, "LeftTrackpad"},
    {keyboard::ControllerButton::RightGrip, "RightGrip"},
    {keyboard::ControllerButton::RightTrigger, "RightTrigger"},
    {keyboard::ControllerButton::RightA, "RightA"},
    {keyboard::ControllerButton::RightB, "RightB"},
    {keyboard::ControllerButton::RightMenu, "RightMenu"},
    {keyboard::ControllerButton::RightJoystick, "RightJoystick"},
    {keyboard::ControllerButton::RightTrackpad, "RightTrackpad"},
}};

void setError(std::string *error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

bool settingsPath(std::filesystem::path *path, std::string *error) {
    const DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (required == 0) {
        setError(error, "LOCALAPPDATA is not available.");
        return false;
    }

    std::vector<wchar_t> buffer(required);
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), required);
    if (length == 0 || length >= required) {
        setError(error, "Could not read LOCALAPPDATA.");
        return false;
    }

    *path = std::filesystem::path(std::wstring(buffer.data(), length)) /
        L"VROverlayKeyboard" / L"settings.ini";
    return true;
}

const char *languageName(keyboard::UiLanguage language) {
    switch (language) {
    case keyboard::UiLanguage::Korean: return "ko";
    case keyboard::UiLanguage::Japanese: return "ja";
    case keyboard::UiLanguage::English: return "en";
    }
    return "";
}

bool parseLanguage(std::string_view name, keyboard::UiLanguage *language) {
    if (name == "ko") {
        *language = keyboard::UiLanguage::Korean;
    } else if (name == "ja") {
        *language = keyboard::UiLanguage::Japanese;
    } else if (name == "en") {
        *language = keyboard::UiLanguage::English;
    } else {
        return false;
    }
    return true;
}

const char *handName(keyboard::ControllerHand hand) {
    switch (hand) {
    case keyboard::ControllerHand::Left: return "left";
    case keyboard::ControllerHand::Right: return "right";
    }
    return "";
}

bool parseHand(std::string_view name, keyboard::ControllerHand *hand) {
    if (name == "left") {
        *hand = keyboard::ControllerHand::Left;
    } else if (name == "right") {
        *hand = keyboard::ControllerHand::Right;
    } else {
        return false;
    }
    return true;
}

const char *buttonName(keyboard::ControllerButton button) {
    for (const ButtonSettingName &entry : kButtonNames) {
        if (entry.button == button) {
            return entry.name;
        }
    }
    return "";
}

bool parseButton(std::string_view name, keyboard::ControllerButton *button) {
    for (const ButtonSettingName &entry : kButtonNames) {
        if (name == entry.name) {
            *button = entry.button;
            return true;
        }
    }
    return false;
}

bool parseUnsigned(std::string_view text, std::uint32_t *value) {
    const char *first = text.data();
    const char *last = first + text.size();
    const auto result = std::from_chars(first, last, *value);
    return result.ec == std::errc{} && result.ptr == last;
}

bool parseFloat(std::string_view text, float *value) {
    float parsed = 0.0f;
    const char *first = text.data();
    const char *last = first + text.size();
    const auto result = std::from_chars(first, last, parsed, std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != last || !std::isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

std::string serializeButtons(const std::vector<keyboard::ControllerButton> &buttons) {
    std::string result;
    for (const keyboard::ControllerButton button : buttons) {
        const char *name = buttonName(button);
        if (name[0] == '\0') {
            continue;
        }
        if (!result.empty()) {
            result.push_back(',');
        }
        result += name;
    }
    return result;
}
}

bool WindowsSettingsStore::load(keyboard::AppSettings *settings, std::string *error) {
    if (!settings) {
        setError(error, "Settings destination is missing.");
        return false;
    }
    *settings = keyboard::AppSettings{};

    std::filesystem::path path;
    if (!settingsPath(&path, error)) {
        return false;
    }

    std::error_code filesystemError;
    const bool exists = std::filesystem::exists(path, filesystemError);
    if (filesystemError) {
        setError(error, "Could not inspect the settings file: " + filesystemError.message());
        return false;
    }
    if (!exists) {
        // 첫 실행은 계약에 정의한 한국어·오른쪽 Grip+B·0초 기본값을 사용한다.
        return true;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        setError(error, "Could not open the settings file.");
        return false;
    }

    keyboard::AppSettings loaded;
    bool foundLanguage = false;
    bool foundButtons = false;
    bool foundHold = false;
    bool validPointerHand = true;
    bool validPointerOffsets = true;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string_view key(line.data(), separator);
        const std::string_view value(line.data() + separator + 1, line.size() - separator - 1);
        if (key == "language") {
            foundLanguage = parseLanguage(value, &loaded.uiLanguage);
            if (!foundLanguage) {
                break;
            }
        } else if (key == "summon_buttons") {
            foundButtons = true;
            loaded.summonButtons.clear();
            std::size_t begin = 0;
            while (begin <= value.size()) {
                const std::size_t end = value.find(',', begin);
                const std::size_t length = end == std::string_view::npos ? value.size() - begin : end - begin;
                keyboard::ControllerButton button{};
                if (length == 0 || !parseButton(value.substr(begin, length), &button)) {
                    foundButtons = false;
                    break;
                }
                loaded.summonButtons.push_back(button);
                if (end == std::string_view::npos) {
                    break;
                }
                begin = end + 1;
            }
            if (!foundButtons) {
                break;
            }
        } else if (key == "summon_hold_milliseconds") {
            foundHold = parseUnsigned(value, &loaded.summonHoldMilliseconds);
            if (!foundHold) {
                break;
            }
        } else if (key == "pointer_hand") {
            // 이전 설정 파일에는 이 항목이 없으므로 오른손 기본값을 그대로 유지한다.
            validPointerHand = parseHand(value, &loaded.pointerHand);
            if (!validPointerHand) {
                break;
            }
        } else if (key == "pointer_offset_x_percent") {
            validPointerOffsets = parseFloat(value, &loaded.pointerOffsetXPercent);
            if (!validPointerOffsets) {
                break;
            }
        } else if (key == "pointer_offset_y_percent") {
            validPointerOffsets = parseFloat(value, &loaded.pointerOffsetYPercent);
            if (!validPointerOffsets) {
                break;
            }
        }
    }

    std::string validationError;
    if (!input.eof() || !foundLanguage || !foundButtons || !foundHold || !validPointerHand || !validPointerOffsets ||
        !keyboard::validateAppSettings(loaded, &validationError)) {
        setError(error, validationError.empty() ? "The settings file is incomplete or invalid." : validationError);
        return false;
    }
    *settings = std::move(loaded);
    return true;
}

bool WindowsSettingsStore::save(const keyboard::AppSettings &settings, std::string *error) {
    if (!keyboard::validateAppSettings(settings, error)) {
        return false;
    }

    std::filesystem::path path;
    if (!settingsPath(&path, error)) {
        return false;
    }
    std::error_code filesystemError;
    std::filesystem::create_directories(path.parent_path(), filesystemError);
    if (filesystemError) {
        setError(error, "Could not create the settings folder: " + filesystemError.message());
        return false;
    }

    const std::filesystem::path temporaryPath = path.wstring() + L"." +
        std::to_wstring(GetCurrentProcessId()) + L".tmp";
    {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            setError(error, "Could not create the temporary settings file.");
            return false;
        }
        output << "language=" << languageName(settings.uiLanguage) << '\n';
        output << "pointer_hand=" << handName(settings.pointerHand) << '\n';
        output << "pointer_offset_x_percent=" << settings.pointerOffsetXPercent << '\n';
        output << "pointer_offset_y_percent=" << settings.pointerOffsetYPercent << '\n';
        output << "summon_buttons=" << serializeButtons(settings.summonButtons) << '\n';
        output << "summon_hold_milliseconds=" << settings.summonHoldMilliseconds << '\n';
        output.flush();
        if (!output) {
            setError(error, "Could not write the settings file.");
            return false;
        }
    }

    if (!MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD result = GetLastError();
        DeleteFileW(temporaryPath.c_str());
        setError(error, "Could not replace the settings file (Windows error " + std::to_string(result) + ").");
        return false;
    }
    return true;
}
