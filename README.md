# CS2 Auto Bhop

External bunny-hop tool for Counter-Strike 2. Reads game memory externally (no DLL injection) and resolves all offsets at runtime — no hardcoded values, no cs2_dumper dependency.

## Features

- **Runtime offset resolution** — pattern scans `client.dll` and parses Source 2's SchemaSystem to find `dwLocalPlayerPawn`, `m_fFlags`, and `jump` automatically
- **Auto-refresh** — re-resolves offsets every 60 seconds in a background thread, surviving mid-session game updates
- **Always-on bhop** — hold spacebar to bunny hop, no toggle needed
- **ANSI console HUD** — gradient header, animated wave bar, live status display
- **ImGui settings overlay** — DirectX 11 overlay toggled with INSERT, featuring keybind rebinding and offset status
- **Hidden from taskbar** — both console and overlay use `WS_EX_TOOLWINDOW` (no taskbar icon, no Alt-Tab entry)
- **Config persistence** — keybinds saved to `config.ini` in simple INI format

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 (v143 toolset)
- [Dear ImGui](https://github.com/ocornut/imgui) v1.91.x source files

## Building

1. Clone this repository
2. Open `cs2-bhop.sln` in Visual Studio 2022
3. Select **Release | x64**
4. Build (Ctrl+Shift+B)

The executable will be in `bin/Release/`.

## Usage

1. **Run as Administrator** (required for process memory access)
2. Launch `cs2-bhop.exe` — it will wait for CS2 to start
3. Once attached, offsets are resolved and the bhop loop begins
4. Hold **SPACE** to bunny hop
5. Press **INSERT** to open the settings overlay
6. Press **END** to exit

## Controls

| Key | Action |
|-----|--------|
| SPACE | Hold to bhop |
| INSERT | Toggle settings overlay |
| END | Exit program |

Settings and exit keys can be rebound in the ImGui overlay.

## How Offset Resolution Works

### dwLocalPlayerPawn
Pattern scans `client.dll` for instruction signatures (LEA/MOV with RIP-relative addressing) that reference the global local-player-pawn pointer. Tries multiple fallback signatures.

### m_fFlags
Primary: Parses CS2's Source 2 SchemaSystem at runtime — finds the `SchemaSystem_001` singleton in `schemasystem.dll`, walks `CSchemaSystem` → `CSchemaSystemTypeScope("client.dll")` → `CSchemaClassBinding("C_BaseEntity")` → `SchemaClassFieldData_t("m_fFlags")`, and reads the field offset. Fallback: pattern scan.

### jump (dwForceJump)
Scans `client.dll` for the `+jump` command string, then locates cross-references to find the force-jump global address. Falls back to multiple pattern signatures.

## Project Structure

```
cs2-bhop/
├── cs2-bhop.sln                  # VS2022 solution
├── cs2-bhop.vcxproj              # Project file (x64, C++20, /MT)
├── cs2-bhop.vcxproj.filters
├── config.ini                    # Created at runtime
├── src/
│   ├── main.cpp                  # Entry point, thread orchestration
│   ├── memory.h                  # RPM/WPM helpers, process/module enumeration
│   ├── scanner.h / scanner.cpp   # IDA-style pattern scanner (1MB chunked reads)
│   ├── schema.h / schema.cpp     # Source 2 SchemaSystem runtime parser
│   ├── offsets.h / offsets.cpp    # Offset resolver + background auto-refresh
│   ├── config.h / config.cpp     # INI config load/save, VKey name lookup
│   ├── overlay.h / overlay.cpp   # ImGui/DX11 settings overlay
│   ├── console_ui.h              # ANSI drawing helpers (header, HUD, status)
│   └── bhop.h / bhop.cpp         # Bhop controller thread
└── imgui/                        # Drop Dear ImGui sources here
```

## Config File

`config.ini` is auto-created next to the executable on first run:

```ini
[Keybinds]
OpenSettings=45
ExitProgram=35
```

Values are Windows virtual-key codes (45 = VK_INSERT, 35 = VK_END).

## Disclaimer

This tool is provided for educational purposes. Use at your own risk. The author is not responsible for any consequences of using this software.
