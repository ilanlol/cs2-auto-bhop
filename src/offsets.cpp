#include "offsets.h"
#include "scanner.h"
#include "schema.h"
#include "memory.h"
#include <chrono>

uintptr_t OffsetManager::FindLocalPlayerPawn(HANDLE process, uintptr_t clientBase, DWORD clientSize) {
    PatternScanner scanner(process, clientBase, clientSize);

    std::vector<PatternEntry> patterns = {
        { "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 48 83 EC 28", 3, 7 },
        { "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 48 8D 0D", 3, 7 },
        { "48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 88", 3, 7 },
        { "48 8D 05 ? ? ? ? 48 89 44 24 ? 48 8D 05 ? ? ? ? 48 89 44 24 ? 48 8D 05", 3, 7 },
        { "48 8B 0D ? ? ? ? 48 85 C9 74 ? E8 ? ? ? ? 48 8B C8", 3, 7 },
    };

    uintptr_t result = scanner.FindFirstRelative(patterns);
    if (result)
        return result - clientBase;
    return 0;
}

uintptr_t OffsetManager::FindFlags(HANDLE process, DWORD pid, uintptr_t clientBase, DWORD clientSize) {
    int32_t offset = schema::FindFieldOffset(process, pid, "client.dll", "C_BaseEntity", "m_fFlags");
    if (offset > 0)
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
            if (fieldOff > 0 && fieldOff < 0x10000)
                return static_cast<uintptr_t>(fieldOff);
        }
    }
    return 0;
}

uintptr_t OffsetManager::FindJump(HANDLE process, uintptr_t clientBase, DWORD clientSize) {
    PatternScanner scanner(process, clientBase, clientSize);

    uintptr_t jumpStringAddr = 0;
    {
        std::vector<uint8_t> searchBuf(clientSize);
        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(process, reinterpret_cast<LPCVOID>(clientBase), searchBuf.data(), clientSize, &bytesRead)) {
            const char* target = "+jump";
            size_t targetLen = 5;
            for (size_t i = 0; i + targetLen < bytesRead; i++) {
                if (memcmp(searchBuf.data() + i, target, targetLen) == 0 && searchBuf[i + targetLen] == '\0') {
                    jumpStringAddr = clientBase + i;
                    break;
                }
            }
        }
    }

    if (!jumpStringAddr)
        return 0;

    std::vector<PatternEntry> patterns = {
        { "48 8D 15 ? ? ? ? 8D 4B 08 E8", 3, 7 },
        { "48 8D 05 ? ? ? ? 48 89 05 ? ? ? ? 48 8D 05", 3, 7 },
        { "48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 48 89 05", 3, 7 },
    };

    uintptr_t result = scanner.FindFirstRelative(patterns);
    if (result)
        return result - clientBase;

    {
        std::vector<uint8_t> moduleMem(clientSize);
        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(process, reinterpret_cast<LPCVOID>(clientBase), moduleMem.data(), clientSize, &bytesRead)) {
            uint32_t rva = static_cast<uint32_t>(jumpStringAddr - clientBase);
            for (size_t i = 0; i + 7 < bytesRead; i++) {
                if (moduleMem[i] == 0x48 && (moduleMem[i + 1] == 0x8D || moduleMem[i + 1] == 0x8B)) {
                    int32_t rel = *reinterpret_cast<int32_t*>(moduleMem.data() + i + 3);
                    uintptr_t resolved = (clientBase + i + 7) + rel;
                    if (resolved == jumpStringAddr) {
                        for (size_t j = i; j > 0 && j > i - 128; j--) {
                            if (moduleMem[j] == 0x48 && moduleMem[j + 1] == 0x89 && moduleMem[j + 2] == 0x05) {
                                int32_t jumpRel = *reinterpret_cast<int32_t*>(moduleMem.data() + j + 3);
                                uintptr_t jumpAddr = (clientBase + j + 7) + jumpRel;
                                if (jumpAddr > clientBase && jumpAddr < clientBase + clientSize)
                                    return jumpAddr - clientBase;
                            }
                        }
                        for (size_t j = i + 7; j + 7 < bytesRead && j < i + 256; j++) {
                            if (moduleMem[j] == 0x48 && moduleMem[j + 1] == 0x89 && moduleMem[j + 2] == 0x05) {
                                int32_t jumpRel = *reinterpret_cast<int32_t*>(moduleMem.data() + j + 3);
                                uintptr_t jumpAddr = (clientBase + j + 7) + jumpRel;
                                if (jumpAddr > clientBase && jumpAddr < clientBase + clientSize)
                                    return jumpAddr - clientBase;
                            }
                        }
                    }
                }
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
