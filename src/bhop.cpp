#include "bhop.h"
#include "offsets.h"
#include "memory.h"
#include <chrono>

void BhopController::Start(HANDLE process, uintptr_t clientBase, OffsetManager* offsets) {
    m_process    = process;
    m_clientBase = clientBase;
    m_offsets    = offsets;
    m_running    = true;
    m_thread     = std::thread(&BhopController::Run, this);
}

void BhopController::Stop() {
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();
}

void BhopController::Run() {
    bool lastOnGround = false;

    while (m_running) {
        bool spaceHeld = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        m_spaceHeld.store(spaceHeld);

        if (spaceHeld) {
            GameOffsets off = m_offsets->Get();
            if (!off.valid) {
                SwitchToThread();
                continue;
            }

            uintptr_t pawnAddr = m_clientBase + off.dwLocalPlayerPawn;
            uintptr_t pawnPtr = mem::RPM<uintptr_t>(m_process, pawnAddr);

            m_debugPawn.store(pawnPtr);

            if (pawnPtr != 0) {
                uint32_t flags = mem::RPM<uint32_t>(m_process, pawnPtr + off.m_fFlags);
                m_debugFlags.store(flags);
                bool onGround = (flags & FL_ONGROUND) != 0;
                m_onGround.store(onGround);

                uintptr_t jumpAddr = m_clientBase + off.dwForceJump;

                if (onGround) {
                    mem::WPM<int>(m_process, jumpAddr, JUMP_PRESS);
                    std::this_thread::sleep_for(std::chrono::microseconds(500));
                    mem::WPM<int>(m_process, jumpAddr, JUMP_RELEASE);

                    if (!lastOnGround)
                        m_jumpCount.fetch_add(1);
                }

                lastOnGround = onGround;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            m_onGround.store(false);
            lastOnGround = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
