#pragma once
#include <Windows.h>
#include <cstdint>
#include <atomic>
#include <thread>

class OffsetManager;

class BhopController {
public:
    void Start(HANDLE process, uintptr_t clientBase, OffsetManager* offsets);
    void Stop();

    uint64_t GetJumpCount() const { return m_jumpCount.load(); }
    bool IsOnGround() const { return m_onGround.load(); }
    bool IsSpaceHeld() const { return m_spaceHeld.load(); }
    uintptr_t GetDebugPawn() const { return m_debugPawn.load(); }
    uint32_t GetDebugFlags() const { return m_debugFlags.load(); }

private:
    void Run();
    uintptr_t ResolvePawn(const struct GameOffsets& off);

    HANDLE           m_process    = nullptr;
    uintptr_t        m_clientBase = 0;
    OffsetManager*   m_offsets    = nullptr;
    std::thread      m_thread;
    std::atomic<bool> m_running{false};

    std::atomic<uint64_t>  m_jumpCount{0};
    std::atomic<bool>      m_onGround{false};
    std::atomic<bool>      m_spaceHeld{false};
    std::atomic<uintptr_t> m_debugPawn{0};
    std::atomic<uint32_t>  m_debugFlags{0};

    static constexpr int JUMP_PRESS   = 65537;
    static constexpr int JUMP_RELEASE = 256;
    static constexpr int FL_ONGROUND  = (1 << 0);
};
