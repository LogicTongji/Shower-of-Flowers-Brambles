#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <d3d9.h>
#include <d3dx9.h>

#include <string>
#include <vector>

#include "war_map_input.h"
#include "war_map_regions.h"
#include "war_map_state.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

namespace
{

HWND g_window = nullptr;
IDirect3D9* g_direct3D = nullptr;
IDirect3DDevice9* g_device = nullptr;
ID3DXSprite* g_sprite = nullptr;
IDirect3DTexture9* g_mapTexture = nullptr;
IDirect3DTexture9* g_overlayTexture = nullptr;
D3DPRESENT_PARAMETERS g_presentParameters{};

bool g_dragging = false;
POINT g_dragOffset{};
std::string g_texturePath = "china_map.bmp";

RegionMap g_regionMap;
MockWarMapStateSource g_stateSource;
WarMapState g_state;
WarMapState g_previousState;
std::vector<RgbaPixel> g_overlayPixels;
bool g_hasState = false;

constexpr int kWindowWidth = 720;
constexpr int kWindowHeight = 680;
constexpr int kMapX = 30;
constexpr int kMapY = 25;

MapRect GetMapRect()
{
    return {
        kMapX,
        kMapY,
        static_cast<int>(g_regionMap.width),
        static_cast<int>(g_regionMap.height)
    };
}

void UpdateRegionOverlayTexture();

bool RefreshState()
{
    WarMapState nextState;

    if (!g_stateSource.Read(nextState))
    {
        return false;
    }

    if (!g_hasState
        || HasStateChanged(
            g_previousState,
            nextState
        ))
    {
        g_state = nextState;
        g_previousState = nextState;
        g_hasState = true;
        UpdateRegionOverlayTexture();
    }

    return true;
}

bool InitializeRegionOverlay()
{
    if (!LoadRegionMap(
        "china_region_ids.bin",
        g_regionMap
    ))
    {
        return false;
    }

    const HRESULT result =
        g_device->CreateTexture(
            g_regionMap.width,
            g_regionMap.height,
            1,
            0,
            D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED,
            &g_overlayTexture,
            nullptr
        );

    if (FAILED(result))
    {
        return false;
    }

    return RefreshState();
}

void UpdateRegionOverlayTexture()
{
    if (!g_overlayTexture)
    {
        return;
    }

    BuildOverlayFromState(
        g_regionMap,
        g_state,
        g_overlayPixels
    );

    D3DLOCKED_RECT lockedRect{};

    if (FAILED(g_overlayTexture->LockRect(
        0,
        &lockedRect,
        nullptr,
        0
    )))
    {
        return;
    }

    for (uint32_t y = 0;
         y < g_regionMap.height;
         ++y)
    {
        auto* destination =
            reinterpret_cast<uint32_t*>(
                static_cast<uint8_t*>(lockedRect.pBits)
                + static_cast<size_t>(y)
                * lockedRect.Pitch
            );

        const RgbaPixel* source =
            g_overlayPixels.data()
            + static_cast<size_t>(y)
            * g_regionMap.width;

        for (uint32_t x = 0;
             x < g_regionMap.width;
             ++x)
        {
            const RgbaPixel& pixel = source[x];

            destination[x] = D3DCOLOR_ARGB(
                pixel.a,
                pixel.r,
                pixel.g,
                pixel.b
            );
        }
    }

    g_overlayTexture->UnlockRect(0);
}

void ReportRegionClick(int mouseX, int mouseY)
{
    if (g_regionMap.regionIds.empty())
    {
        return;
    }

    const uint16_t regionId = PickRegion(
        g_regionMap,
        GetMapRect(),
        mouseX,
        mouseY
    );

    if (regionId == 0
        || regionId > kWarMapRegionNames.size())
    {
        return;
    }

    const std::string message =
        "WarMap region clicked: "
        + std::string(kWarMapRegionNames[regionId - 1])
        + "\n";

    OutputDebugStringA(message.c_str());
}

void ReleaseDeviceResources()
{
    if (g_overlayTexture)
    {
        g_overlayTexture->Release();
        g_overlayTexture = nullptr;
    }

    if (g_mapTexture)
    {
        g_mapTexture->Release();
        g_mapTexture = nullptr;
    }

    if (g_sprite)
    {
        g_sprite->Release();
        g_sprite = nullptr;
    }
}

void ShutdownDirect3D()
{
    ReleaseDeviceResources();

    if (g_device)
    {
        g_device->Release();
        g_device = nullptr;
    }

    if (g_direct3D)
    {
        g_direct3D->Release();
        g_direct3D = nullptr;
    }
}

bool ResetDevice()
{
    if (!g_device)
    {
        return false;
    }

    if (g_sprite)
    {
        g_sprite->OnLostDevice();
    }

    const HRESULT result =
        g_device->Reset(&g_presentParameters);

    if (FAILED(result))
    {
        return false;
    }

    if (g_sprite)
    {
        g_sprite->OnResetDevice();
    }

    return true;
}

bool InitializeDirect3D()
{
    g_direct3D =
        Direct3DCreate9(D3D_SDK_VERSION);

    if (!g_direct3D)
    {
        return false;
    }

    ZeroMemory(
        &g_presentParameters,
        sizeof(g_presentParameters)
    );

    g_presentParameters.Windowed = TRUE;
    g_presentParameters.SwapEffect =
        D3DSWAPEFFECT_DISCARD;
    g_presentParameters.BackBufferFormat =
        D3DFMT_UNKNOWN;
    g_presentParameters.EnableAutoDepthStencil =
        FALSE;
    g_presentParameters.PresentationInterval =
        D3DPRESENT_INTERVAL_IMMEDIATE;
    g_presentParameters.hDeviceWindow =
        g_window;

    DWORD behaviorFlags =
        D3DCREATE_HARDWARE_VERTEXPROCESSING;

    HRESULT result =
        g_direct3D->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            g_window,
            behaviorFlags,
            &g_presentParameters,
            &g_device
        );

    if (FAILED(result))
    {
        behaviorFlags =
            D3DCREATE_SOFTWARE_VERTEXPROCESSING;

        result =
            g_direct3D->CreateDevice(
                D3DADAPTER_DEFAULT,
                D3DDEVTYPE_HAL,
                g_window,
                behaviorFlags,
                &g_presentParameters,
                &g_device
            );
    }

    if (FAILED(result))
    {
        return false;
    }

    result =
        D3DXCreateSprite(
            g_device,
            &g_sprite
        );

    if (FAILED(result))
    {
        return false;
    }

    result =
        D3DXCreateTextureFromFileA(
            g_device,
            g_texturePath.c_str(),
            &g_mapTexture
        );

    if (FAILED(result))
    {
        return false;
    }

    if (!InitializeRegionOverlay())
    {
        return false;
    }

    return true;
}

void Render()
{
    if (!g_device || !g_sprite || !g_mapTexture)
    {
        return;
    }

    const HRESULT cooperativeResult =
        g_device->TestCooperativeLevel();

    if (cooperativeResult == D3DERR_DEVICELOST)
    {
        return;
    }

    if (cooperativeResult == D3DERR_DEVICENOTRESET
        && !ResetDevice())
    {
        return;
    }

    g_device->Clear(
        0,
        nullptr,
        D3DCLEAR_TARGET,
        D3DCOLOR_ARGB(255, 24, 30, 38),
        1.0f,
        0
    );

    if (FAILED(g_device->BeginScene()))
    {
        return;
    }

    D3DXVECTOR3 mapPosition(
        static_cast<float>(kMapX),
        static_cast<float>(kMapY),
        0.0f
    );

    g_sprite->Begin(D3DXSPRITE_ALPHABLEND);

    g_sprite->Draw(
        g_mapTexture,
        nullptr,
        nullptr,
        &mapPosition,
        D3DCOLOR_ARGB(255, 255, 255, 255)
    );

    if (g_overlayTexture)
    {
        g_sprite->Draw(
            g_overlayTexture,
            nullptr,
            nullptr,
            &mapPosition,
            D3DCOLOR_ARGB(255, 255, 255, 255)
        );
    }

    g_sprite->End();
    g_device->EndScene();
    g_device->Present(
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
}

void BeginWindowDrag()
{
    POINT cursorPosition{};
    RECT windowRect{};

    GetCursorPos(&cursorPosition);
    GetWindowRect(g_window, &windowRect);

    g_dragOffset.x =
        cursorPosition.x - windowRect.left;

    g_dragOffset.y =
        cursorPosition.y - windowRect.top;

    g_dragging = true;
    SetCapture(g_window);
}

void UpdateWindowDrag()
{
    if (!g_dragging)
    {
        return;
    }

    POINT cursorPosition{};
    GetCursorPos(&cursorPosition);

    SetWindowPos(
        g_window,
        nullptr,
        cursorPosition.x - g_dragOffset.x,
        cursorPosition.y - g_dragOffset.y,
        0,
        0,
        SWP_NOSIZE
        | SWP_NOZORDER
        | SWP_NOACTIVATE
    );
}

void EndWindowDrag()
{
    g_dragging = false;
    ReleaseCapture();
}

LRESULT CALLBACK WindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(window);
        }
        return 0;

    case WM_LBUTTONDOWN:
        ReportRegionClick(
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        );
        BeginWindowDrag();
        return 0;

    case WM_MOUSEMOVE:
        UpdateWindowDrag();
        return 0;

    case WM_LBUTTONUP:
        EndWindowDrag();
        return 0;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            g_presentParameters.BackBufferWidth =
                LOWORD(lParam);

            g_presentParameters.BackBufferHeight =
                HIWORD(lParam);

            ResetDevice();
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProcA(
        window,
        message,
        wParam,
        lParam
    );
}

bool CreateMapWindow(HINSTANCE instance)
{
    WNDCLASSEXA windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEXA);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = "WarMapPreviewWindow";

    if (!RegisterClassExA(&windowClass))
    {
        return false;
    }

    g_window = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        windowClass.lpszClassName,
        "China War Map Preview",
        WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    return g_window != nullptr;
}

}

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE,
    LPSTR commandLine,
    int)
{
    if (commandLine && commandLine[0] != '\0')
    {
        g_texturePath = commandLine;
    }

    if (!CreateMapWindow(instance))
    {
        MessageBoxA(
            nullptr,
            "Failed to create map window.",
            "War Map Preview",
            MB_ICONERROR
        );
        return 1;
    }

    if (!InitializeDirect3D())
    {
        MessageBoxA(
            g_window,
            "Failed to initialize DirectX 9 or load china_map.bmp.",
            "War Map Preview",
            MB_ICONERROR
        );

        ShutdownDirect3D();
        return 1;
    }

    MSG message{};

    while (message.message != WM_QUIT)
    {
        while (PeekMessageA(
            &message,
            nullptr,
            0,
            0,
            PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        Render();
        Sleep(1);
    }

    ShutdownDirect3D();
    return 0;
}
