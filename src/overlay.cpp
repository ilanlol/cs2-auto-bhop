#include "overlay.h"
#include "offsets.h"
#include "config.h"

#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct Overlay::D3DState {
    ID3D11Device*           device        = nullptr;
    ID3D11DeviceContext*    context       = nullptr;
    IDXGISwapChain*         swapChain     = nullptr;
    ID3D11RenderTargetView* renderTarget  = nullptr;
};

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void Overlay::Start(OffsetManager* offsets, ConfigManager* config) {
    m_offsets = offsets;
    m_config  = config;
    m_running = true;
    m_thread  = std::thread(&Overlay::Run, this);
}

void Overlay::Stop() {
    m_running = false;
    if (m_hwnd)
        PostMessageW(m_hwnd, WM_QUIT, 0, 0);
    if (m_thread.joinable())
        m_thread.join();
}

void Overlay::ToggleVisibility() {
    bool vis = !m_visible.load();
    m_visible.store(vis);

    if (m_hwnd) {
        if (vis) {
            LONG exStyle = GetWindowLongW(m_hwnd, GWL_EXSTYLE);
            exStyle &= ~WS_EX_TRANSPARENT;
            SetWindowLongW(m_hwnd, GWL_EXSTYLE, exStyle);
            SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);
            ShowWindow(m_hwnd, SW_SHOW);
            SetForegroundWindow(m_hwnd);
        } else {
            LONG exStyle = GetWindowLongW(m_hwnd, GWL_EXSTYLE);
            exStyle |= WS_EX_TRANSPARENT;
            SetWindowLongW(m_hwnd, GWL_EXSTYLE, exStyle);
            SetLayeredWindowAttributes(m_hwnd, 0, 0, LWA_ALPHA);
        }
    }
}

bool Overlay::InitWindow() {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = GetModuleHandleW(nullptr);
    wc.lpszClassName  = L"CS2BhopOverlay";
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        L"CS2BhopOverlay", L"",
        WS_POPUP,
        (screenW - 420) / 2, (screenH - 460) / 2, 420, 460,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr
    );

    if (!m_hwnd) return false;

    SetLayeredWindowAttributes(m_hwnd, 0, 0, LWA_ALPHA);
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

bool Overlay::InitD3D() {
    m_d3d = new D3DState();

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount        = 1;
    sd.BufferDesc.Width   = 420;
    sd.BufferDesc.Height  = 460;
    sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow       = m_hwnd;
    sd.SampleDesc.Count   = 1;
    sd.Windowed           = TRUE;
    sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &m_d3d->swapChain, &m_d3d->device, &featureLevel, &m_d3d->context
    );
    if (FAILED(hr)) return false;

    ID3D11Texture2D* backBuffer = nullptr;
    m_d3d->swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    m_d3d->device->CreateRenderTargetView(backBuffer, nullptr, &m_d3d->renderTarget);
    backBuffer->Release();

    return true;
}

void Overlay::CleanupD3D() {
    if (!m_d3d) return;
    if (m_d3d->renderTarget) m_d3d->renderTarget->Release();
    if (m_d3d->swapChain)    m_d3d->swapChain->Release();
    if (m_d3d->context)      m_d3d->context->Release();
    if (m_d3d->device)       m_d3d->device->Release();
    delete m_d3d;
    m_d3d = nullptr;
}

void Overlay::RenderUI() {
    GameOffsets off = m_offsets->Get();
    AppConfig  cfg = m_config->Get();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(420, 460));
    ImGui::Begin("CS2 Bhop \xe2\x80\x94 Settings", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    ImGui::SeparatorText("Keybinds");

    auto drawKeybindRow = [&](const char* label, int& keyRef, int targetId) {
        ImGui::Text("%s", label);
        ImGui::SameLine(200);

        char keyLabel[64];
        snprintf(keyLabel, sizeof(keyLabel), " %s ", ConfigManager::VKeyName(keyRef).c_str());

        ImGui::BeginDisabled();
        ImGui::Button(keyLabel, ImVec2(100, 0));
        ImGui::EndDisabled();

        ImGui::SameLine();
        char btnId[32];
        snprintf(btnId, sizeof(btnId), "Change##%d", targetId);

        if (m_rebindTarget == targetId) {
            ImGui::Button("Press a key...", ImVec2(100, 0));
            for (int vk = 1; vk < 256; vk++) {
                if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON)
                    continue;
                if (GetAsyncKeyState(vk) & 1) {
                    if (vk == VK_ESCAPE) {
                        m_rebindTarget = -1;
                    } else {
                        keyRef = vk;
                        m_rebindTarget = -1;
                    }
                    break;
                }
            }
        } else {
            if (ImGui::Button(btnId, ImVec2(100, 0)))
                m_rebindTarget = targetId;
        }
    };

    int openKey = cfg.openSettingsKey;
    int exitKey = cfg.exitProgramKey;

    drawKeybindRow("Open Settings", openKey, 0);
    drawKeybindRow("Exit Program",  exitKey, 1);

    if (openKey != cfg.openSettingsKey || exitKey != cfg.exitProgramKey) {
        cfg.openSettingsKey = openKey;
        cfg.exitProgramKey  = exitKey;
        m_config->Set(cfg);
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Status");

    ImGui::Text("Bhop");
    ImGui::SameLine(200);
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "ALWAYS ON");

    char hexBuf[32];

    auto drawOffsetRow = [&](const char* label, uintptr_t val) {
        ImGui::Text("%s", label);
        ImGui::SameLine(200);
        snprintf(hexBuf, sizeof(hexBuf), "0x%llX", (unsigned long long)val);
        ImGui::TextColored(val ? ImVec4(1,1,1,1) : ImVec4(1,0.3f,0.3f,1), "%s", hexBuf);
    };

    drawOffsetRow("controller", off.dwLocalPlayerController);
    drawOffsetRow("entityList", off.dwEntityList);
    drawOffsetRow("m_hPlayerPawn", off.m_hPlayerPawn);
    drawOffsetRow("m_fFlags", off.m_fFlags);
    drawOffsetRow("jump", off.dwForceJump);

    ImGui::Text("Last refresh");
    ImGui::SameLine(200);
    int secs = m_offsets->SecondsSinceLastRefresh();
    char timeBuf[32];
    snprintf(timeBuf, sizeof(timeBuf), "%ds ago", secs);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.7f, 1.0f), "%s", timeBuf);

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Button("Save Config", ImVec2(180, 30))) {
        m_config->Save(ConfigManager::GetExeDir() + "config.ini");
    }

    ImGui::End();
}

void Overlay::Run() {
    if (!InitWindow() || !InitD3D()) {
        m_running = false;
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding  = 4.0f;
    style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.08f, 0.08f, 0.12f, 0.95f);
    style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.10f, 0.06f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.10f, 0.28f, 1.00f);
    style.Colors[ImGuiCol_Button]        = ImVec4(0.24f, 0.14f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.34f, 0.20f, 0.55f, 1.00f);
    style.Colors[ImGuiCol_FrameBg]       = ImVec4(0.12f, 0.10f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_Header]        = ImVec4(0.20f, 0.12f, 0.32f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.18f, 0.48f, 1.00f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40f, 0.24f, 0.65f, 1.00f);
    style.Colors[ImGuiCol_Text]          = ImVec4(0.85f, 0.85f, 0.90f, 1.00f);

    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_d3d->device, m_d3d->context);

    MSG msg{};
    while (m_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!m_running) break;

        if (m_visible.load()) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            RenderUI();

            ImGui::Render();
            const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            m_d3d->context->OMSetRenderTargets(1, &m_d3d->renderTarget, nullptr);
            m_d3d->context->ClearRenderTargetView(m_d3d->renderTarget, clearColor);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            m_d3d->swapChain->Present(1, 0);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupD3D();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}
