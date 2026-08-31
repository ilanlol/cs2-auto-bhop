#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <string>

namespace mem {

inline DWORD GetProcessId(const wchar_t* processName) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, processName) == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    return pid;
}

struct ModuleInfo {
    uintptr_t base = 0;
    DWORD     size = 0;
};

inline ModuleInfo GetModuleInfo(DWORD pid, const wchar_t* moduleName) {
    ModuleInfo info{};
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        if (Module32FirstW(snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, moduleName) == 0) {
                    info.base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                    info.size = me.modBaseSize;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
    }
    return info;
}

inline uintptr_t GetModuleBase(DWORD pid, const wchar_t* moduleName) {
    return GetModuleInfo(pid, moduleName).base;
}

template <typename T>
__forceinline T RPM(HANDLE process, uintptr_t address) {
    T value{};
    ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), &value, sizeof(T), nullptr);
    return value;
}

template <typename T>
__forceinline void WPM(HANDLE process, uintptr_t address, T value) {
    WriteProcessMemory(process, reinterpret_cast<LPVOID>(address), &value, sizeof(T), nullptr);
}

inline bool ReadBuffer(HANDLE process, uintptr_t address, void* buffer, size_t size) {
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead)
           && bytesRead == size;
}

inline std::string ReadString(HANDLE process, uintptr_t address, size_t maxLen = 256) {
    std::string result(maxLen, '\0');
    SIZE_T bytesRead = 0;
    ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), result.data(), maxLen, &bytesRead);
    size_t nullPos = result.find('\0');
    if (nullPos != std::string::npos)
        result.resize(nullPos);
    return result;
}

} // namespace mem
