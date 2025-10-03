#include <string>
#include <iostream>
#include <thread>
#include "Memory.h"
#include "Offsets.h"
#include "Ark.h"
#include "imguiutil.h"

// Main code
int main(int, char**)
{
    // Create a console for logging status messages
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);  // Redirect stdout to console
    std::cout << "Starting overlay..." << std::endl;

    // Wait for ARK game window to appear
    HWND game_hwnd = NULL;
    while (!game_hwnd)
    {
        game_hwnd = ::FindWindowW(NULL, L"ARK: Survival Evolved");  // Adjust title if needed
        if (!game_hwnd)
        {
            std::cout << "Waiting for ARK to open..." << std::endl;
            Sleep(500);  // Check every 5 seconds (adjust as needed)
        }
    }
    std::cout << "[+] Connected to ARK." << std::endl;

    // Launch the actor update thread
    Memory ark = Memory(std::wstring(L"ShooterGame.exe"));
    initGlobals(ark);
    std::thread actorUpdateThread(updateActorsThread, std::ref(ark));
    actorUpdateThread.detach();  // Let it run in background

    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Overlay", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Overlay", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);

    // Set initial extended style for overlay (topmost, layered, toolwindow)
    LONG ex_style = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    SetWindowLong(hwnd, GWL_EXSTYLE, ex_style | WS_EX_TRANSPARENT);  // Start with transparent (click-through)
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);  // Black as color key for transparency

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window (non-activating)
    ::ShowWindow(hwnd, SW_SHOWNA);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);  // Black for color key transparency

    // Menu toggle state
    bool show_menu = false;
    static bool last_insert = false;

    // Function to sync overlay position with game window
    auto UpdateOverlayPosition = [&](HWND overlay, HWND target) {
        if (!IsWindowVisible(target) || IsIconic(target)) {  // Hide if game is minimized or not visible
            ShowWindow(overlay, SW_HIDE);
            return;
        }
        RECT rect;
        GetWindowRect(target, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        SetWindowPos(overlay, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_SHOWWINDOW | SWP_NOACTIVATE);
        ShowWindow(overlay, SW_SHOWNA);
        };

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Check for Insert key press to toggle menu (debounced)
        bool insert_down = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (insert_down && !last_insert) {
            show_menu = !show_menu;

            // Dynamically toggle WS_EX_TRANSPARENT for click-through
            LONG current_ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
            if (show_menu) {
                // Menu shown: Allow interaction (remove transparent)
                SetWindowLong(hwnd, GWL_EXSTYLE, current_ex_style & ~WS_EX_TRANSPARENT);
            }
            else {
                // Menu hidden: Make click-through (add transparent)
                SetWindowLong(hwnd, GWL_EXSTYLE, current_ex_style | WS_EX_TRANSPARENT);
            }
            UpdateWindow(hwnd);  // Apply style changes immediately
        }
        last_insert = insert_down;

        // Update overlay position every frame to follow the game window (handles move/resize/minimize)
        UpdateOverlayPosition(hwnd, game_hwnd);

        // Handle lost D3D9 device
        if (g_DeviceLost)
        {
            HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST)
            {
                ::Sleep(10);
                continue;
            }
            if (hr == D3DERR_DEVICENOTRESET)
                ResetDevice();
            g_DeviceLost = false;
        }

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();


        if (show_menu)
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("ARK Survival Evolved by ThNks");
            ImGui::Checkbox("Ocean Loots", &g_config.OceanData);

            // Lock g_config for thread safety
            {
                std::lock_guard<std::mutex> lock(g_configMutex);
                for (size_t i = 0; i < g_config.Actors.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(i)); // Unique ID for each actor

                    // Use std::string for safe string handling
                    constexpr size_t max_name_length = 64; // Match RenderDinos limit
                    std::string name_buffer = g_config.Actors[i].Name.empty() ? "Unknown" : g_config.Actors[i].Name;
                    // Ensure buffer can hold max_name_length characters
                    name_buffer.resize(max_name_length, '\0'); // Pad with nulls to allow full input

                    if (ImGui::InputText("Actor Name:", name_buffer.data(), max_name_length + 1))
                    {
                        // Trim nulls and sanitize input
                        name_buffer = name_buffer.c_str(); // Convert back to std::string, removing trailing nulls
                        if (!name_buffer.empty())
                        {
                            // Sanitize: Replace non-printable characters
                            for (char& c : name_buffer)
                            {
                                if (!std::isprint(static_cast<unsigned char>(c)))
                                {
                                    c = '?';
                                }
                            }
                            // Truncate to safe length
                            g_config.Actors[i].Name = name_buffer.substr(0, max_name_length);
                        }
                        else
                        {
                            g_config.Actors[i].Name = "Unknown"; // Fallback for empty input
                        }
                    }

                    // Inputs for minlvl and maxlvl with bounds checking
                    int minlvl = g_config.Actors[i].minlvl;
                    int maxlvl = g_config.Actors[i].maxlvl;
                    ImGui::InputInt("Min Level:", &minlvl);
                    ImGui::InputInt("Max Level:", &maxlvl);
                    g_config.Actors[i].minlvl = std::clamp(minlvl, 0, 999);
                    g_config.Actors[i].maxlvl = std::clamp(maxlvl, 0, 999);

                    // Button to remove this actor
                    if (ImGui::Button("Remove Actor"))
                    {
                        g_config.Actors.erase(g_config.Actors.begin() + i);
                        --i; // Adjust index after erasure
                    }

                    ImGui::PopID();
                    ImGui::Separator(); // Visual separator
                }

                if (ImGui::Button("New Actor"))
                {
                    ActorConfig temp;
                    temp.Name = "Unknown"; // Initialize with default name
                    g_config.Actors.push_back(temp);
                }
            } // Mutex unlocks here

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // RenderDinos with mutex-protected access to g_actorsCache
        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            std::vector<ActorInfo> actors_copy = g_actorsCache; // Copy to avoid holding mutex during rendering
            RenderDinos(ark, actors_copy, screenWidth, screenHeight);
        }

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_XRGB(0, 0, 0);  // Clear to black (color key for transparency outside drawn areas)
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST)
            g_DeviceLost = true;
    }

    // Cleanup
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    // Free console before exit
    FreeConsole();

    return 0;
}
