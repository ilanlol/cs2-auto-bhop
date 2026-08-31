#pragma once
#include <Windows.h>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <thread>

struct GameOffsets {
    uintptr_t dwLocalPlayerPawn = 0;
    uintptr_t m_fFlags          = 0;
    uintptr_t dwForceJump       = 0;
    bool      valid             = false;
};

class OffsetManager {
public:
    bool ResolveAll(HANDLE process, DWORD pid, uintptr_t clientBase, DWORD clientSize);
    void StartAutoRefresh(HANDLE process, DWORD pid, uintptr_t clientBase, DWORD clientSize, int intervalMs = 60000);
    void Stop();

    GameOffsets Get() const;
    int SecondsSinceLastRefresh() const;

private:
    uintptr_t FindLocalPlayerPawn(HANDLE process, uintptr_t clientBase, DWORD clientSize);
    uintptr_t FindFlags(HANDLE process, DWORD pid, uintptr_t clientBase, DWORD clientSize);
    uintptr_t FindJump(HANDLE process, uintptr_t clientBase, DWORD clientSize);

    mutable std::mutex m_mutex;
    GameOffsets        m_offsets;
    std::thread        m_refreshThread;
    std::atomic<bool>  m_running{false};
    DWORD              m_lastRefreshTick = 0;
};
