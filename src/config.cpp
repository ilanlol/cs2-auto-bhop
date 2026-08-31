#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

void ConfigManager::SetDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = AppConfig{};
}

void ConfigManager::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        SetDefaults();
        Save(path);
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = AppConfig{};

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '[' || line[0] == ';' || line[0] == '#')
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = Trim(line.substr(0, eq));
        std::string val = Trim(line.substr(eq + 1));

        try {
            int v = std::stoi(val);
            if (key == "OpenSettings") m_config.openSettingsKey = v;
            else if (key == "ExitProgram") m_config.exitProgramKey = v;
        } catch (...) {}
    }
}

void ConfigManager::Save(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream file(path);
    if (!file.is_open()) return;

    file << "[Keybinds]\n";
    file << "OpenSettings=" << m_config.openSettingsKey << "\n";
    file << "ExitProgram=" << m_config.exitProgramKey << "\n";
}

AppConfig ConfigManager::Get() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void ConfigManager::Set(const AppConfig& cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = cfg;
}

std::string ConfigManager::VKeyName(int vk) {
    switch (vk) {
        case VK_INSERT:   return "INSERT";
        case VK_DELETE:   return "DELETE";
        case VK_HOME:     return "HOME";
        case VK_END:      return "END";
        case VK_PRIOR:    return "PAGE UP";
        case VK_NEXT:     return "PAGE DOWN";
        case VK_F1:       return "F1";
        case VK_F2:       return "F2";
        case VK_F3:       return "F3";
        case VK_F4:       return "F4";
        case VK_F5:       return "F5";
        case VK_F6:       return "F6";
        case VK_F7:       return "F7";
        case VK_F8:       return "F8";
        case VK_F9:       return "F9";
        case VK_F10:      return "F10";
        case VK_F11:      return "F11";
        case VK_F12:      return "F12";
        case VK_ESCAPE:   return "ESCAPE";
        case VK_TAB:      return "TAB";
        case VK_CAPITAL:  return "CAPS LOCK";
        case VK_LSHIFT:   return "LEFT SHIFT";
        case VK_RSHIFT:   return "RIGHT SHIFT";
        case VK_LCONTROL: return "LEFT CTRL";
        case VK_RCONTROL: return "RIGHT CTRL";
        case VK_LMENU:    return "LEFT ALT";
        case VK_RMENU:    return "RIGHT ALT";
        case VK_NUMPAD0:  return "NUMPAD 0";
        case VK_NUMPAD1:  return "NUMPAD 1";
        case VK_NUMPAD2:  return "NUMPAD 2";
        case VK_NUMPAD3:  return "NUMPAD 3";
        case VK_NUMPAD4:  return "NUMPAD 4";
        case VK_NUMPAD5:  return "NUMPAD 5";
        case VK_NUMPAD6:  return "NUMPAD 6";
        case VK_NUMPAD7:  return "NUMPAD 7";
        case VK_NUMPAD8:  return "NUMPAD 8";
        case VK_NUMPAD9:  return "NUMPAD 9";
        case VK_PAUSE:    return "PAUSE";
        default: break;
    }

    if (vk >= 0x30 && vk <= 0x39) {
        return std::string(1, static_cast<char>(vk));
    }
    if (vk >= 0x41 && vk <= 0x5A) {
        return std::string(1, static_cast<char>(vk));
    }

    char buf[32];
    UINT scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    if (scanCode) {
        LONG lParam = (scanCode << 16);
        if (GetKeyNameTextA(lParam, buf, sizeof(buf)) > 0)
            return std::string(buf);
    }

    snprintf(buf, sizeof(buf), "VK_%d", vk);
    return std::string(buf);
}

std::string ConfigManager::GetExeDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string dir(path);
    auto pos = dir.find_last_of("\\/");
    if (pos != std::string::npos)
        dir = dir.substr(0, pos + 1);
    return dir;
}

std::string ConfigManager::Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}
