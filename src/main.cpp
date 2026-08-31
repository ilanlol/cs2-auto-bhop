#include <Windows.h>
#pragma comment(lib, "user32.lib")
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <cstdio>

#include "memory.h"
#include "offsets.h"
#include "config.h"
#include "console_ui.h"
#include "bhop.h"
#include "overlay.h"

static void hideFromTaskbar(HWND hwnd) {
    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TOOLWINDOW;
    exStyle &= ~WS_EX_APPWINDOW;
    SetWindowLongW(hwnd, GWL_EXSTYLE, exStyle);
    ShowWindow(hwnd, SW_HIDE);
    ShowWindow(hwnd, SW_SHOW);
}

int main() {
    SetConsoleTitleW(L"CS2 Bhop v2");
    ui::setupConsole();

    HWND consoleHwnd = GetConsoleWindow();
    if (consoleHwnd)
        hideFromTaskbar(consoleHwnd);

    std::string configPath = ConfigManager::GetExeDir() + "config.ini";
    ConfigManager configMgr;
    configMgr.Load(configPath);

    ui::drawHeader();

    std::cout << "   " << pal::warn() << "\xAF " << pal::text() << "Waiting for "
              << pal::accent() << "cs2.exe" << pal::text() << "...\n";

    DWORD pid = 0;
    while (!(pid = mem::GetProcessId(L"cs2.exe")))
        std::this_thread::sleep_for(std::chrono::seconds(1));

    ui::drawStatusLine("\xFE", "Process", "cs2.exe [" + std::to_string(pid) + "]", pal::good(), pal::white());

    HANDLE process = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
    if (!process) {
        ui::drawStatusLine("X", "Error", "Open failed - Run as Admin", pal::bad(), pal::bad());
        std::cout << ansi::show;
        std::cin.get();
        return 1;
    }

    uintptr_t clientBase = 0;
    DWORD clientSize = 0;
    while (true) {
        auto info = mem::GetModuleInfo(pid, L"client.dll");
        if (info.base) {
            clientBase = info.base;
            clientSize = info.size;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    char hexBuf[32];
    snprintf(hexBuf, sizeof(hexBuf), "0x%llX", (unsigned long long)clientBase);
    ui::drawStatusLine("\xFE", "client.dll", hexBuf, pal::good(), pal::white());

    OffsetManager offsetMgr;
    std::cout << "   " << pal::warn() << "\xAF " << pal::text() << "Resolving offsets..." << ansi::reset << "\n";

    bool resolved = offsetMgr.ResolveAll(process, pid, clientBase, clientSize);
    GameOffsets off = offsetMgr.Get();

    if (resolved) {
        snprintf(hexBuf, sizeof(hexBuf), "0x%llX", (unsigned long long)off.dwLocalPlayerPawn);
        ui::drawStatusLine("\xFE", "dwLocalPlayerPawn", hexBuf, pal::good(), pal::white());

        snprintf(hexBuf, sizeof(hexBuf), "0x%llX", (unsigned long long)off.m_fFlags);
        ui::drawStatusLine("\xFE", "m_fFlags", hexBuf, pal::good(), pal::white());

        snprintf(hexBuf, sizeof(hexBuf), "0x%llX", (unsigned long long)off.dwForceJump);
        ui::drawStatusLine("\xFE", "jump", hexBuf, pal::good(), pal::white());

        ui::drawStatusLine("\xFE", "Addresses", "resolved", pal::good(), pal::muted());
    } else {
        if (off.dwLocalPlayerPawn == 0)
            ui::drawStatusLine("X", "dwLocalPlayerPawn", "FAILED", pal::bad(), pal::bad());
        else {
            snprintf(hexBuf, sizeof(hexBuf), "0x%llX", (unsigned long long)off.dwLocalPlayerPawn);
            ui::drawStatusLine("\xFE", "dwLocalPlayerPawn", hexBuf, pal::good(), pal::white());
        }

        if (off.m_fFlags == 0)
            ui::drawStatusLine("X", "m_fFlags", "FAILED", pal::bad(), pal::bad());
        else {
            snprintf(hexBuf, sizeof(hexBuf), "0x%llX", (unsigned long long)off.m_fFlags);
            ui::drawStatusLine("\xFE", "m_fFlags", hexBuf, pal::good(), pal::white());
        }

        if (off.dwForceJump == 0)
            ui::drawStatusLine("X", "jump", "FAILED", pal::bad(), pal::bad());
        else {
            snprintf(hexBuf, sizeof(hexBuf), "0x%llX", (unsigned long long)off.dwForceJump);
            ui::drawStatusLine("\xFE", "jump", hexBuf, pal::good(), pal::white());
        }

        ui::drawStatusLine("X", "Addresses", "partial - bhop may not work", pal::bad(), pal::warn());
    }

    offsetMgr.StartAutoRefresh(process, pid, clientBase, clientSize, 60000);

    std::cout << "\n";
    ui::drawGradientBar();
    std::cout << "\n";

    AppConfig cfg = configMgr.Get();
    ui::drawStatusLine("\x10", "Bhop", "ACTIVE", pal::accent(), pal::good());
    std::string controlsStr = std::string(ConfigManager::VKeyName(cfg.openSettingsKey)) + "=Settings  "
                            + ConfigManager::VKeyName(cfg.exitProgramKey) + "=Exit";
    ui::drawStatusLine("\x10", "Controls", controlsStr, pal::accent(), pal::muted());
    std::cout << "\n";
    ui::drawHUD(false, false, 0);

    BhopController bhop;
    bhop.Start(process, clientBase, &offsetMgr);

    Overlay overlay;
    overlay.Start(&offsetMgr, &configMgr);

    bool lastSpace    = false;
    bool lastOnGround = false;
    int  frameCounter = 0;
    bool running      = true;

    while (running) {
        cfg = configMgr.Get();

        if (GetAsyncKeyState(cfg.exitProgramKey) & 0x8000)
            break;

        if (GetAsyncKeyState(cfg.openSettingsKey) & 1)
            overlay.ToggleVisibility();

        DWORD exitCode = 0;
        if (!GetExitCodeProcess(process, &exitCode) || exitCode != STILL_ACTIVE)
            break;

        bool spaceHeld = bhop.IsSpaceHeld();
        bool onGround  = bhop.IsOnGround();
        uint64_t jumps = bhop.GetJumpCount();

        frameCounter++;
        if (spaceHeld != lastSpace || onGround != lastOnGround || (frameCounter % 5 == 0)) {
            ui::drawHUD(spaceHeld, onGround, jumps);

            std::cout << ansi::moveTo(ui::HUD_ROW + 2, 1) << ansi::clearLine();
            char dbgBuf[128];
            snprintf(dbgBuf, sizeof(dbgBuf), "   %sPawn: 0x%llX  Flags: 0x%X%s",
                     pal::muted().c_str(),
                     (unsigned long long)bhop.GetDebugPawn(),
                     bhop.GetDebugFlags(),
                     ansi::reset);
            std::cout << dbgBuf;

            lastSpace    = spaceHeld;
            lastOnGround = onGround;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    bhop.Stop();
    overlay.Stop();
    offsetMgr.Stop();

    std::cout << ansi::moveTo(ui::HUD_ROW, 1) << ansi::clearLine();
    std::cout << ansi::moveTo(ui::HUD_ROW + 1, 1) << ansi::clearLine();
    std::cout << ansi::moveTo(ui::HUD_ROW, 1);
    std::cout << "   " << pal::warn() << "[!] " << pal::text() << "Exited cleanly." << ansi::reset << "\n";
    std::cout << ansi::show;

    configMgr.Save(configPath);
    CloseHandle(process);
    return 0;
}
