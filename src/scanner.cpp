#include "scanner.h"
#include "memory.h"
#include <sstream>
#include <algorithm>

PatternScanner::PatternScanner(HANDLE process, uintptr_t moduleBase, DWORD moduleSize)
    : m_process(process), m_base(moduleBase), m_size(moduleSize) {}

PatternScanner::ParsedPattern PatternScanner::Parse(const std::string& pattern) {
    ParsedPattern result;
    std::istringstream stream(pattern);
    std::string token;
    while (stream >> token) {
        if (token == "?" || token == "??") {
            result.bytes.push_back(0);
            result.mask.push_back(false);
        } else {
            result.bytes.push_back(static_cast<uint8_t>(std::stoul(token, nullptr, 16)));
            result.mask.push_back(true);
        }
    }
    return result;
}

uintptr_t PatternScanner::ScanBuffer(const uint8_t* data, size_t dataSize, const ParsedPattern& pat) const {
    if (pat.bytes.empty() || dataSize < pat.bytes.size())
        return 0;

    size_t patLen = pat.bytes.size();
    size_t limit = dataSize - patLen;

    for (size_t i = 0; i <= limit; i++) {
        bool found = true;
        for (size_t j = 0; j < patLen; j++) {
            if (pat.mask[j] && data[i + j] != pat.bytes[j]) {
                found = false;
                break;
            }
        }
        if (found)
            return i;
    }
    return UINTPTR_MAX;
}

uintptr_t PatternScanner::Find(const std::string& pattern) const {
    ParsedPattern pat = Parse(pattern);
    if (pat.bytes.empty()) return 0;

    std::vector<uint8_t> buffer(CHUNK_SIZE + pat.bytes.size());
    size_t overlap = pat.bytes.size() - 1;

    for (size_t offset = 0; offset < m_size; ) {
        size_t readSize = (std::min)(CHUNK_SIZE + overlap, static_cast<size_t>(m_size - offset));
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(m_process, reinterpret_cast<LPCVOID>(m_base + offset),
                               buffer.data(), readSize, &bytesRead) || bytesRead == 0)
        {
            offset += CHUNK_SIZE;
            continue;
        }

        uintptr_t result = ScanBuffer(buffer.data(), static_cast<size_t>(bytesRead), pat);
        if (result != UINTPTR_MAX)
            return m_base + offset + result;

        offset += CHUNK_SIZE;
    }
    return 0;
}

uintptr_t PatternScanner::FindRelative(const std::string& pattern, int relOffset, int instrLen) const {
    uintptr_t addr = Find(pattern);
    if (!addr) return 0;

    int32_t rel = mem::RPM<int32_t>(m_process, addr + relOffset);
    return addr + instrLen + rel;
}

uintptr_t PatternScanner::FindFirstRelative(const std::vector<PatternEntry>& entries) const {
    for (const auto& e : entries) {
        uintptr_t result = FindRelative(e.pattern, e.relOffset, e.instrLen);
        if (result)
            return result;
    }
    return 0;
}
