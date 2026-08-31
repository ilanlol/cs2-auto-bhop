#pragma once
#include <Windows.h>
#include <atomic>
#include <thread>

class OffsetManager;
class ConfigManager;

class Overlay {
public:
    void Start(OffsetManager* offsets, ConfigManager* config);
    void Stop();
    void ToggleVisibility();
    bool IsVisible() const { return m_visible.load(); }

private:
    void Run();
    bool InitWindow();
    bool InitD3D();
    void CleanupD3D();
    void RenderUI();

    OffsetManager*    m_offsets = nullptr;
    ConfigManager*    m_config  = nullptr;
    std::thread       m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_visible{false};

    HWND m_hwnd = nullptr;

    struct D3DState;
    D3DState* m_d3d = nullptr;

    int  m_rebindTarget = -1; // -1 = none, 0 = openSettings, 1 = exitProgram
};
