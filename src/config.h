#pragma once
#include <Windows.h>
#include <string>
#include <mutex>

struct AppConfig {
    int openSettingsKey = VK_INSERT;  // 0x2D = 45
    int exitProgramKey  = VK_END;    // 0x23 = 35
};

class ConfigManager {
public:
    void Load(const std::string& path);
    void Save(const std::string& path);
    void SetDefaults();

    AppConfig Get() const;
    void Set(const AppConfig& cfg);

    static std::string VKeyName(int vk);
    static std::string GetExeDir();

private:
    mutable std::mutex m_mutex;
    AppConfig m_config;

    static std::string Trim(const std::string& s);
};
