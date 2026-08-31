#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>
#include <string>

struct PatternEntry {
    std::string pattern;
    int relOffset;
    int instrLen;
};

class PatternScanner {
public:
    PatternScanner(HANDLE process, uintptr_t moduleBase, DWORD moduleSize);

    uintptr_t Find(const std::string& pattern) const;
    uintptr_t FindRelative(const std::string& pattern, int relOffset, int instrLen) const;
    uintptr_t FindFirstRelative(const std::vector<PatternEntry>& entries) const;

private:
    struct ParsedPattern {
        std::vector<uint8_t> bytes;
        std::vector<bool>    mask;
    };

    static ParsedPattern Parse(const std::string& pattern);
    uintptr_t ScanBuffer(const uint8_t* data, size_t dataSize, const ParsedPattern& pat) const;

    HANDLE    m_process;
    uintptr_t m_base;
    DWORD     m_size;
    static constexpr size_t CHUNK_SIZE = 1024 * 1024;
};
