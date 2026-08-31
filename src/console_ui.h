#pragma once
#include <Windows.h>
#include <iostream>
#include <string>
#include <cstdio>
#include <cmath>
#include <cstdint>

namespace ansi {
    inline std::string fg(int r, int g, int b) {
        char buf[24];
        snprintf(buf, sizeof(buf), "\x1b[38;2;%d;%d;%dm", r, g, b);
        return buf;
    }
    constexpr const char* reset = "\x1b[0m";
    constexpr const char* bold  = "\x1b[1m";
    constexpr const char* dim   = "\x1b[2m";
    constexpr const char* hide  = "\x1b[?25l";
    constexpr const char* show  = "\x1b[?25h";
    constexpr const char* clear = "\x1b[2J\x1b[H";
    inline std::string moveTo(int row, int col) {
        char buf[16];
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
        return buf;
    }
    inline std::string clearLine() { return "\x1b[2K"; }
}

namespace pal {
    inline std::string h1()     { return ansi::fg(160, 120, 255); }
    inline std::string h2()     { return ansi::fg(120,  80, 220); }
    inline std::string accent() { return ansi::fg(0,   220, 180); }
    inline std::string good()   { return ansi::fg(80,  255, 120); }
    inline std::string warn()   { return ansi::fg(255, 200,  60); }
    inline std::string bad()    { return ansi::fg(255,  80,  80); }
    inline std::string muted()  { return ansi::fg(90,   90, 110); }
    inline std::string text()   { return ansi::fg(200, 200, 220); }
    inline std::string white()  { return ansi::fg(255, 255, 255); }
    inline std::string air()    { return ansi::fg(200, 100, 255); }
}

namespace ui {

inline void setupConsole() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    std::cout << ansi::hide;
}

inline void drawGradientBar() {
    std::cout << "  ";
    for (int i = 0; i < 56; i++) {
        float t = (float)i / 55.0f;
        int r = (int)(100 + t * 80);
        int g = (int)(60 + t * 80);
        int b = (int)(180 + t * 75);
        std::cout << ansi::fg(r, g, b) << "\xC4";
    }
    std::cout << ansi::reset << "\n";
}

inline void drawStatusLine(const char* icon, const std::string& label, const std::string& value,
                           const std::string& iconColor, const std::string& valColor) {
    std::cout << "   " << iconColor << icon << "  "
              << pal::text() << label;
    int pad = 20 - (int)label.size();
    if (pad > 0) std::cout << pal::muted() << std::string(pad, '.');
    std::cout << valColor << " " << value
              << ansi::reset << "\n";
}

inline void drawHeader() {
    std::cout << ansi::clear << "\n";
    drawGradientBar();
    std::cout << ansi::bold;
    std::cout << pal::h1() << "      ____  __  __ ____  ____\n";
    std::cout << pal::h1() << "     | __ )| || || _ \\| _ \\\n";
    std::cout << ansi::fg(140, 100, 245) << "     |  _ \\|    ||  _/|  _/\n";
    std::cout << ansi::fg(120, 80, 230) << "     |____/|_||_||_|  |_|\n";
    std::cout << ansi::reset << "\n";
    std::cout << "  ";
    const char* subtitle = "  CS2 Auto Bunny Hop  \xFA  v2.0  ";
    size_t len = strlen(subtitle);
    for (size_t i = 0; i < len; i++) {
        float t = (float)i / (float)len;
        int r = (int)(80 + t * 120);
        int g = (int)(180 - t * 40);
        int b = (int)(255 - t * 55);
        std::cout << ansi::fg(r, g, b) << subtitle[i];
    }
    std::cout << ansi::reset << "\n";
    drawGradientBar();
    std::cout << "\n";
}

constexpr int HUD_ROW = 19;

inline void drawHUD(bool spaceHeld, bool onGround, uint64_t jumpCount) {
    std::cout << ansi::moveTo(HUD_ROW, 1) << ansi::clearLine();
    std::cout << "   ";
    std::cout << pal::muted() << "SPACE ";
    if (spaceHeld)
        std::cout << pal::good() << ansi::bold << "\xFE HELD  " << ansi::reset;
    else
        std::cout << pal::muted() << "\xFE IDLE  " << ansi::reset;
    std::cout << pal::muted() << "\xB3 ";
    std::cout << pal::muted() << "STATE ";
    if (!spaceHeld)
        std::cout << pal::muted() << "\xFE ---   " << ansi::reset;
    else if (onGround)
        std::cout << pal::warn() << ansi::bold << "\xFE GROUND" << ansi::reset;
    else
        std::cout << pal::air() << ansi::bold << "\xFE AIR   " << ansi::reset;
    std::cout << pal::muted() << " \xB3 ";
    std::cout << pal::muted() << "HOPS ";
    std::cout << pal::white() << ansi::bold << jumpCount << ansi::reset;

    std::cout << ansi::moveTo(HUD_ROW + 1, 1) << ansi::clearLine();
    std::cout << "   ";
    if (spaceHeld) {
        static int frame = 0;
        frame++;
        for (int i = 0; i < 50; i++) {
            float t = (float)i / 49.0f;
            float wave = 0.5f + 0.5f * sinf(t * 6.28f + frame * 0.3f);
            int brightness = (int)(40 + 80 * wave);
            if (onGround)
                std::cout << ansi::fg(brightness / 3, brightness, brightness / 2);
            else
                std::cout << ansi::fg(brightness, brightness / 4, brightness);
            std::cout << "\xDB";
        }
    } else {
        for (int i = 0; i < 50; i++)
            std::cout << ansi::fg(35, 35, 45) << "\xB0";
    }
    std::cout << ansi::reset;
    std::cout << ansi::moveTo(HUD_ROW + 3, 1) << std::flush;
}

} // namespace ui
