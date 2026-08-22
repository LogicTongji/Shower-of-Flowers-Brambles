#include <d3d9.h>
#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

#include "gui_data_bridge.h"
#include "gui_host_d3d9.h"
#include "gui_lua_bridge.h"

namespace
{

LRESULT CALLBACK ProbeWindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    return DefWindowProcW(window, message, wParam, lParam);
}

HWND CreateProbeWindow(HINSTANCE instance)
{
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = ProbeWindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"ScriptedGuiD3D9Probe";
    if (!RegisterClassW(&windowClass)
        && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return nullptr;
    }
    return CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"Scripted GUI D3D9 Probe",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1600,
        800,
        nullptr,
        nullptr,
        instance,
        nullptr
    );
}

IDirect3DDevice9* CreateProbeDevice(
    IDirect3D9* direct3D,
    HWND window
)
{
    D3DPRESENT_PARAMETERS parameters{};
    parameters.Windowed = TRUE;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.hDeviceWindow = window;
    parameters.BackBufferWidth = 1600;
    parameters.BackBufferHeight = 800;
    parameters.BackBufferFormat = D3DFMT_UNKNOWN;
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9* device = nullptr;
    if (SUCCEEDED(direct3D->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &parameters,
            &device
        )))
    {
        return device;
    }
    if (SUCCEEDED(direct3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_REF,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &parameters,
        &device
    )))
    {
        return device;
    }
    direct3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_NULLREF,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &parameters,
        &device
    );
    return device;
}

}

int main(int argc, char** argv)
{
    const std::filesystem::path root = argc >= 2
        ? std::filesystem::path(argv[1])
        : std::filesystem::current_path();
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HWND window = CreateProbeWindow(instance);
    IDirect3D9* direct3D = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9* device = direct3D
        ? CreateProbeDevice(direct3D, window)
        : nullptr;
    if (!window || !direct3D || !device)
    {
        std::cerr << "Failed to create the D3D9 probe device\n";
        if (device)
        {
            device->Release();
        }
        if (direct3D)
        {
            direct3D->Release();
        }
        if (window)
        {
            DestroyWindow(window);
        }
        return 77;
    }

    GuiD3D9Host host;
    std::string error;
    if (!host.Initialize(root, device, error))
    {
        std::cerr << error << '\n';
        device->Release();
        direct3D->Release();
        DestroyWindow(window);
        return 1;
    }

    GuiDataBridgeUpdate snapshot;
    snapshot.revision = 1;
    snapshot.fullSnapshot = true;
    snapshot.values["state.visible"] = true;
    snapshot.values["state.active"] = true;
    snapshot.values["state.viewertag"] = std::string("CHI");
	snapshot.values["state.windowopen"] = false;
    snapshot.values["warprogress.known"] = true;
    snapshot.values["warprogress.own"] = 0.45;
    snapshot.values["warprogress.enemy"] = 0.55;
    for (int itemId = 1; itemId <= 41; ++itemId)
    {
        snapshot.values[
            "regions." + std::to_string(itemId)
                + ".controlledpercentage"
        ] = static_cast<double>((itemId * 7) % 101);
    }
	GuiListModel assignedLeaders;
	assignedLeaders.revision = 1;
	GuiListItem assignedLeader;
	assignedLeader.id = 1;
	assignedLeader.fields["leadertype"] = std::string("military");
	assignedLeader.fields["regionid"] = int64_t{24};
	assignedLeader.fields["assignmentorder"] = int64_t{1};
	assignedLeader.fields["x"] = 0.45;
	assignedLeader.fields["y"] = 0.55;
	assignedLeaders.items.push_back(std::move(assignedLeader));
	snapshot.lists["assigned_leader_list"] = std::move(assignedLeaders);
    if (!GetGuiLuaBridgeService().PublishUpdate(
            "china_anti_jap",
            std::move(snapshot),
            error
        ))
    {
        std::cerr << error << '\n';
        host.Shutdown();
        device->Release();
        direct3D->Release();
        DestroyWindow(window);
        return 1;
    }

    if (SUCCEEDED(device->BeginScene()))
    {
        host.TickAndRender(device);
        device->EndScene();
    }
	const bool transparentDown = host.HandleWindowMessage(
        window,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        MAKELPARAM(700, 350)
    );
	const bool transparentUp = host.HandleWindowMessage(
        window,
        WM_LBUTTONUP,
        0,
        MAKELPARAM(700, 350)
    );
	if (transparentDown || transparentUp)
	{
		std::cerr << "Transparent root canvas blocked host input\n";
		host.Shutdown();
		device->Release();
		direct3D->Release();
		DestroyWindow(window);
		return 1;
	}

    host.Shutdown();
    device->Release();
    direct3D->Release();
    DestroyWindow(window);
    std::cout << "D3D9 in-process host probe: passed\n";
    return 0;
}
