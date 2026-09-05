#include "offsets.h"
#include "scanner.h"
#include "schema.h"
#include "memory.h"
#include <chrono>

uintptr_t OffsetManager::FindLocalPlayerPawn(HANDLE process, uintptr_t clientBase, DWORD clientSize) {
    PatternScanner scanner(process, clientBase, clientSize);

    std::vector<PatternEntry> patterns = {
        { "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 48 83 EC 28 E8", 3, 7 },
        { "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 48 89 5C 24", 3, 7 },
        { "48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 88", 3, 7 },
        { "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40", 3, 7 },
        { "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 48 8D 0D", 3, 7 },
    };

    uintptr_t result = scanner.FindFirstRelative(patterns);
    if (result && result > clientBase && result < clientBase + clientSize) {
        uintptr_t rva = result - clientBase;
        if (rva > 0x100000)
            return rva;
    }
    return 0;
}

uintptr_t OffsetManager::FindFlags(HANDLE process, DWORD pid, uintptr_t clientBase, DWORD clientSize) {
    int32_t offset = schema::FindFieldOffset(process, pid, "client.dll", "C_BaseEntity", "m_fFlags");
    if (offset > 0x100 && offset < 0x2000)
        return static_cast<uintptr_t>(offset);

    PatternScanner scanner(process, clientBase, clientSize);

    std::vector<PatternEntry> patterns = {
        { "8B 81 ? ? ? ? C3 CC CC CC CC CC 48 8D 0D", 2, 0 },
        { "8B 83 ? ? ? ? 85 C0 0F 84", 2, 0 },
        { "8B 87 ? ? ? ? A8 01 74", 2, 0 },
    };

    for (const auto& entry : patterns) {
        uintptr_t addr = scanner.Find(entry.pattern);
        if (addr) {
            uint32_t fieldOff = mem::RPM<uint32_t>(process, addr + entry.relOffset);
            if (fieldOff > 0x100 && fieldOff < 0x2000)
                return static_cast<uintptr_t>(fieldOff);
        }
    }
    return 0;
}

uintptr_t OffsetManager::FindJump(HANDLE process, uintptr_t clientBase, DWORD clientSize) {
    std::vector<uint8_t> moduleData(clientSize);
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(clientBase),
                           moduleData.data(), clientSize, &bytesRead) || bytesRead == 0)
        return 0;

    uintptr_t jumpStringAddr = 0;
    const char* target = "+jump";
    size_t targetLen = 5;
    for (size_t i = 0; i + targetLen < bytesRead; i++) {
        if (memcmp(moduleData.data() + i, target, targetLen) == 0 && moduleData[i + targetLen] == '\0') {
            jumpStringAddr = clientBase + i;
            break;
        }
    }
    if (!jumpStringAddr)
        return 0;

    for (size_t i = 0; i + 7 < bytesRead; i++) {
        if (moduleData[i] != 0x48 && moduleData[i] != 0x4C)
            continue;
        if (moduleData[i + 1] != 0x8D)
            continue;

        uint8_t modrm = moduleData[i + 2];
        if ((modrm & 0xC7) != 0x05)
            continue;

        int32_t rel = *reinterpret_cast<int32_t*>(moduleData.data() + i + 3);
        uintptr_t resolved = (clientBase + i + 7) + rel;

        if (resolved != jumpStringAddr)
            continue;

        size_t searchStart = (i > 64) ? i - 64 : 0;
        for (size_t j = searchStart; j < i; j++) {
            if (moduleData[j] != 0x48 && moduleData[j] != 0x4C)
                continue;
            if (moduleData[j + 1] != 0x8D)
                continue;

            uint8_t modrm2 = moduleData[j + 2];
            if ((modrm2 & 0xC7) != 0x05)
                continue;

            int32_t rel2 = *reinterpret_cast<int32_t*>(moduleData.data() + j + 3);
            uintptr_t buttonAddr = (clientBase + j + 7) + rel2;

            if (buttonAddr > clientBase && buttonAddr < clientBase + clientSize && buttonAddr != jumpStringAddr) {
                uintptr_t rva = buttonAddr - clientBase;
                if (rva > 0x100000)
                    return rva;
            }
        }

        size_t searchEnd = (i + 64 + 7 < bytesRead) ? i + 64 : bytesRead - 7;
        for (size_t j = i + 7; j < searchEnd; j++) {
            if (moduleData[j] != 0x48 && moduleData[j] != 0x4C)
                continue;
            if (moduleData[j + 1] != 0x8D)
                continue;

            uint8_t modrm2 = moduleData[j + 2];
            if ((modrm2 & 0xC7) != 0x05)
                continue;

            int32_t rel2 = *reinterpret_cast<int32_t*>(moduleData.data() + j + 3);
            uintptr_t buttonAddr = (clientBase + j + 7) + rel2;

            if (buttonAddr > clientBase && buttonAddr < clientBase + clientSize && buttonAddr != jumpStringAddr) {
                uintptr_t rva = buttonAddr - clientBase;
                if (rva > 0x100000)
                    return rva;
            }
        }
    }

    return 0;
}

bool OffsetManager::ResolveAll(HANDLE process, DWORD pid, uintptr_t clientBase, DWORD clientSize) {
    GameOffsets newOffsets;

    newOffsets.dwLocalPlayerPawn = FindLocalPlayerPawn(process, clientBase, clientSize);
    newOffsets.m_fFlags = FindFlags(process, pid, clientBase, clientSize);
    newOffsets.dwForceJump = FindJump(process, clientBase, clientSize);

    newOffsets.valid = (newOffsets.dwLocalPlayerPawn != 0 &&
                       newOffsets.m_fFlags != 0 &&
                       newOffsets.dwForceJump != 0);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_offsets = newOffsets;
        m_lastRefreshTick = GetTickCount();
    }

    return newOffsets.valid;
}

void OffsetManager::StartAutoRefresh(HANDLE process, DWORD pid, uintptr_t clientBase, DWORD clientSize, int intervalMs) {
    m_running = true;
    m_refreshThread = std::thread([=, this]() {
        while (m_running) {
            for (int i = 0; i < intervalMs / 100 && m_running; i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!m_running) break;
            ResolveAll(process, pid, clientBase, clientSize);
        }
    });
}

void OffsetManager::Stop() {
    m_running = false;
    if (m_refreshThread.joinable())
        m_refreshThread.join();
}

GameOffsets OffsetManager::Get() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_offsets;
}

int OffsetManager::SecondsSinceLastRefresh() const {
    return static_cast<int>((GetTickCount() - m_lastRefreshTick) / 1000);
}
