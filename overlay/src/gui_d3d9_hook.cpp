#include "gui_d3d9_hook.h"

#include <d3d9.h>
#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "gui_diagnostics.h"
#include "scripted_gui_overlay_api.h"

namespace
{

using Direct3DCreate9Function = IDirect3D9* (WINAPI*)(UINT);
using DeviceQueryInterfaceFunction = HRESULT (WINAPI*)(
    IDirect3DDevice9*,
    REFIID,
    void**
);
using CreateDeviceFunction = HRESULT (WINAPI*)(
    IDirect3D9*,
    UINT,
    D3DDEVTYPE,
    HWND,
    DWORD,
    D3DPRESENT_PARAMETERS*,
    IDirect3DDevice9**
);
using ResetFunction = HRESULT (WINAPI*)(
    IDirect3DDevice9*,
    D3DPRESENT_PARAMETERS*
);
using PresentFunction = HRESULT (WINAPI*)(
    IDirect3DDevice9*,
    const RECT*,
    const RECT*,
    HWND,
    const RGNDATA*
);
using EndSceneFunction = HRESULT (WINAPI*)(IDirect3DDevice9*);
using SwapChainPresentFunction = HRESULT (WINAPI*)(
    IDirect3DSwapChain9*,
    const RECT*,
    const RECT*,
    HWND,
    const RGNDATA*,
    DWORD
);

constexpr std::size_t CreateDeviceVTableIndex = 16;
constexpr std::size_t DeviceQueryInterfaceVTableIndex = 0;
constexpr std::size_t ResetVTableIndex = 16;
constexpr std::size_t PresentVTableIndex = 17;
constexpr std::size_t EndSceneVTableIndex = 42;
constexpr std::size_t SwapChainPresentVTableIndex = 3;

constexpr DWORD SupportedHoi3Timestamp = 0x50978B2F;
constexpr DWORD SupportedHoi3ImageSize = 0x018B0000;
constexpr std::size_t EngineCallsiteCount = 4;
constexpr std::size_t EngineCallsiteByteCount = 9;
constexpr std::array<std::uintptr_t, EngineCallsiteCount>
    EngineCallsiteRvas{
        0x0066F19D,
        0x0067144B,
        0x006715DF,
        0x00675F77
    };
constexpr std::array<uint8_t, EngineCallsiteByteCount>
    EngineCallsiteSignature{
        0x8B, 0x91, 0xA8, 0x00, 0x00, 0x00, 0x50, 0xFF, 0xD2
    };

thread_local int DevicePresentDepth = 0;

struct EngineCallsitePatch
{
    uint8_t* address = nullptr;
    std::array<uint8_t, EngineCallsiteByteCount> replacement{};
    bool installed = false;
};

struct DeviceFunctions
{
    DeviceQueryInterfaceFunction queryInterface = nullptr;
    ResetFunction reset = nullptr;
    PresentFunction present = nullptr;
    EndSceneFunction endScene = nullptr;
};

struct HookState
{
    std::mutex mutex;
    void** direct3DCreate9Slot = nullptr;
    Direct3DCreate9Function originalDirect3DCreate9 = nullptr;
    std::unordered_map<void**, CreateDeviceFunction> createDeviceFunctions;
    std::unordered_map<void**, DeviceFunctions> deviceFunctions;
    std::unordered_map<void**, SwapChainPresentFunction>
        swapChainPresentFunctions;
    HWND window = nullptr;
    WNDPROC originalWindowProcedure = nullptr;
    bool installed = false;
    std::atomic<IDirect3DDevice9*> presentationDevice{nullptr};
    std::atomic<bool> activeDeviceVTableOwned{false};
    std::atomic<uint64_t> endSceneCalls{0};
    std::atomic<uint64_t> presentCalls{0};
    std::atomic<uint64_t> swapChainPresentCalls{0};
    std::array<std::atomic<uint64_t>, EngineCallsiteCount>
        engineCallsiteCalls{};
    std::array<std::atomic<IDirect3DDevice9*>, EngineCallsiteCount>
        engineCallsiteDevices{};
    std::array<EngineCallsitePatch, EngineCallsiteCount>
        engineCallsitePatches{};
    std::atomic<uint64_t> lastMaintenanceMilliseconds{0};
    uint64_t lastDiagnosticMilliseconds = 0;
    uint64_t lastDiagnosticEndSceneCalls = 0;
    uint64_t lastDiagnosticPresentCalls = 0;
    uint64_t lastDiagnosticSwapChainPresentCalls = 0;
    std::array<uint64_t, EngineCallsiteCount>
        lastDiagnosticEngineCallsiteCalls{};
};

HookState& State()
{
    static HookState state;
    return state;
}

bool IsReadableImage(const uint8_t* base);

#if defined(_MSC_VER) && defined(_M_IX86)

void __cdecl RecordEngineCallsite(
    IDirect3DDevice9* device,
    uint32_t siteId
)
{
    if (siteId >= EngineCallsiteCount)
    {
        return;
    }
    HookState& state = State();
    state.engineCallsiteDevices[siteId].store(
        device,
        std::memory_order_relaxed
    );
    state.engineCallsiteCalls[siteId].fetch_add(
        1,
        std::memory_order_relaxed
    );
}

__declspec(naked) void EngineCallsiteThunk0()
{
    __asm
    {
        pushfd
        pushad
        push 0
        push eax
        call RecordEngineCallsite
        add esp, 8
        popad
        popfd
        mov edx, dword ptr [ecx + 0A8h]
        push eax
        call edx
        ret
    }
}

__declspec(naked) void EngineCallsiteThunk1()
{
    __asm
    {
        pushfd
        pushad
        push 1
        push eax
        call RecordEngineCallsite
        add esp, 8
        popad
        popfd
        mov edx, dword ptr [ecx + 0A8h]
        push eax
        call edx
        ret
    }
}

__declspec(naked) void EngineCallsiteThunk2()
{
    __asm
    {
        pushfd
        pushad
        push 2
        push eax
        call RecordEngineCallsite
        add esp, 8
        popad
        popfd
        mov edx, dword ptr [ecx + 0A8h]
        push eax
        call edx
        ret
    }
}

__declspec(naked) void EngineCallsiteThunk3()
{
    __asm
    {
        pushfd
        pushad
        push 3
        push eax
        call RecordEngineCallsite
        add esp, 8
        popad
        popfd
        mov edx, dword ptr [ecx + 0A8h]
        push eax
        call edx
        ret
    }
}

void* GetEngineCallsiteThunk(std::size_t siteId)
{
    switch (siteId)
    {
        case 0:
            return reinterpret_cast<void*>(EngineCallsiteThunk0);
        case 1:
            return reinterpret_cast<void*>(EngineCallsiteThunk1);
        case 2:
            return reinterpret_cast<void*>(EngineCallsiteThunk2);
        case 3:
            return reinterpret_cast<void*>(EngineCallsiteThunk3);
        default:
            return nullptr;
    }
}

bool WriteEngineCallsitePatch(
    uint8_t* address,
    const std::array<uint8_t, EngineCallsiteByteCount>& bytes
)
{
    DWORD oldProtection = 0;
    if (!address
        || !VirtualProtect(
            address,
            bytes.size(),
            PAGE_EXECUTE_READWRITE,
            &oldProtection
        ))
    {
        return false;
    }
    std::memcpy(address, bytes.data(), bytes.size());
    DWORD ignored = 0;
    VirtualProtect(address, bytes.size(), oldProtection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), address, bytes.size());
    return true;
}

void RestoreEngineCallsiteProbes(HookState& state)
{
    for (EngineCallsitePatch& patch : state.engineCallsitePatches)
    {
        if (patch.installed
            && patch.address
            && std::memcmp(
                patch.address,
                patch.replacement.data(),
                patch.replacement.size()
            ) == 0)
        {
            WriteEngineCallsitePatch(
                patch.address,
                EngineCallsiteSignature
            );
        }
        patch = {};
    }
}

bool InstallEngineCallsiteProbes(
    HookState& state,
    std::string& error
)
{
    HMODULE executable = GetModuleHandleW(nullptr);
    auto* base = reinterpret_cast<uint8_t*>(executable);
    if (!IsReadableImage(base))
    {
        error = "HOI3 executable image is invalid";
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        base + dos->e_lfanew
    );
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386
        || nt->FileHeader.TimeDateStamp != SupportedHoi3Timestamp
        || nt->OptionalHeader.SizeOfImage != SupportedHoi3ImageSize)
    {
        error = "Unsupported HOI3 executable version for frame probes";
        return false;
    }

    for (std::size_t siteId = 0;
        siteId < EngineCallsiteCount;
        ++siteId)
    {
        const std::uintptr_t rva = EngineCallsiteRvas[siteId];
        if (rva + EngineCallsiteByteCount
            > nt->OptionalHeader.SizeOfImage)
        {
            error = "HOI3 frame probe RVA is outside the executable";
            RestoreEngineCallsiteProbes(state);
            return false;
        }
        uint8_t* address = base + rva;
        if (std::memcmp(
                address,
                EngineCallsiteSignature.data(),
                EngineCallsiteSignature.size()
            ) != 0)
        {
            error = "HOI3 frame probe signature mismatch at site "
                + std::to_string(siteId);
            RestoreEngineCallsiteProbes(state);
            return false;
        }

        void* thunk = GetEngineCallsiteThunk(siteId);
        const std::intptr_t displacement =
            reinterpret_cast<uint8_t*>(thunk) - (address + 5);
        if (!thunk
            || displacement < std::numeric_limits<int32_t>::min()
            || displacement > std::numeric_limits<int32_t>::max())
        {
            error = "HOI3 frame probe thunk is out of range";
            RestoreEngineCallsiteProbes(state);
            return false;
        }

        EngineCallsitePatch& patch =
            state.engineCallsitePatches[siteId];
        patch.address = address;
        patch.replacement.fill(0x90);
        patch.replacement[0] = 0xE8;
        const int32_t relative = static_cast<int32_t>(displacement);
        std::memcpy(
            patch.replacement.data() + 1,
            &relative,
            sizeof(relative)
        );
        if (!WriteEngineCallsitePatch(address, patch.replacement))
        {
            error = "Failed to write HOI3 frame probe at site "
                + std::to_string(siteId);
            RestoreEngineCallsiteProbes(state);
            return false;
        }
        patch.installed = true;
    }

    error.clear();
    return true;
}

#else

void RestoreEngineCallsiteProbes(HookState&)
{
}

bool InstallEngineCallsiteProbes(HookState&, std::string& error)
{
    error = "HOI3 engine frame probes require an x86 MSVC build";
    return false;
}

#endif

bool IsReadableImage(const uint8_t* base)
{
    if (!base)
    {
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
    {
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        base + dos->e_lfanew
    );
    return nt->Signature == IMAGE_NT_SIGNATURE
        && nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
}

void** FindImportSlot(
    HMODULE image,
    const char* importedModule,
    const char* functionName
)
{
    auto* base = reinterpret_cast<uint8_t*>(image);
    if (!IsReadableImage(base))
    {
        return nullptr;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        base + dos->e_lfanew
    );
    const IMAGE_DATA_DIRECTORY& directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (directory.VirtualAddress == 0 || directory.Size == 0)
    {
        return nullptr;
    }
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        base + directory.VirtualAddress
    );
    for (; descriptor->Name != 0; ++descriptor)
    {
        const char* moduleName = reinterpret_cast<const char*>(
            base + descriptor->Name
        );
        if (_stricmp(moduleName, importedModule) != 0)
        {
            continue;
        }
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA32*>(
            base + (descriptor->OriginalFirstThunk != 0
                ? descriptor->OriginalFirstThunk
                : descriptor->FirstThunk)
        );
        auto* addresses = reinterpret_cast<IMAGE_THUNK_DATA32*>(
            base + descriptor->FirstThunk
        );
        for (; names->u1.AddressOfData != 0; ++names, ++addresses)
        {
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))
            {
                continue;
            }
            const auto* import = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
                base + names->u1.AddressOfData
            );
            if (std::strcmp(
                    reinterpret_cast<const char*>(import->Name),
                    functionName
                ) == 0)
            {
                return reinterpret_cast<void**>(&addresses->u1.Function);
            }
        }
    }
    return nullptr;
}

bool ReplacePointer(
    void** slot,
    void* replacement,
    void*& previous
)
{
    DWORD oldProtection = 0;
    if (!slot
        || !VirtualProtect(
            slot,
            sizeof(void*),
            PAGE_EXECUTE_READWRITE,
            &oldProtection
        ))
    {
        return false;
    }
    previous = InterlockedExchangePointer(slot, replacement);
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), oldProtection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
    return true;
}

bool IsReadablePointerSlot(void** slot)
{
    MEMORY_BASIC_INFORMATION memory{};
    if (!slot
        || VirtualQuery(slot, &memory, sizeof(memory)) == 0
        || memory.State != MEM_COMMIT
        || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
    {
        return false;
    }
    return reinterpret_cast<std::uintptr_t>(slot) + sizeof(void*)
        <= reinterpret_cast<std::uintptr_t>(memory.BaseAddress)
            + memory.RegionSize;
}

void RestorePointerIfOwned(
    void** slot,
    void* replacement,
    void* original
)
{
    if (!IsReadablePointerSlot(slot) || *slot != replacement)
    {
        return;
    }
    void* ignored = nullptr;
    ReplacePointer(slot, original, ignored);
}

LRESULT CALLBACK HookedWindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (ScriptedGui_HandleWindowMessage(
            window,
            message,
            wParam,
            lParam
        ))
    {
        if (message == WM_LBUTTONDOWN)
        {
            SetCapture(window);
        }
        else if (message == WM_LBUTTONUP
            && GetCapture() == window)
        {
            ReleaseCapture();
        }
        return 0;
    }

    HookState& state = State();
    WNDPROC original = nullptr;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        original = state.originalWindowProcedure;
    }
    return original
        ? CallWindowProcW(original, window, message, wParam, lParam)
        : DefWindowProcW(window, message, wParam, lParam);
}

void EnsureWindowHook(IDirect3DDevice9* device)
{
    D3DDEVICE_CREATION_PARAMETERS parameters{};
    if (!device
        || FAILED(device->GetCreationParameters(&parameters))
        || !parameters.hFocusWindow)
    {
        return;
    }

    HookState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.window == parameters.hFocusWindow
        && state.originalWindowProcedure)
    {
        return;
    }
    if (state.window && state.originalWindowProcedure)
    {
        SetWindowLongPtrW(
            state.window,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(state.originalWindowProcedure)
        );
    }

    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrW(
        parameters.hFocusWindow,
        GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(HookedWindowProcedure)
    );
    if (previous == 0 && GetLastError() != ERROR_SUCCESS)
    {
        state.window = nullptr;
        state.originalWindowProcedure = nullptr;
        return;
    }
    state.window = parameters.hFocusWindow;
    state.originalWindowProcedure = reinterpret_cast<WNDPROC>(previous);
    WriteGuiDiagnostic("HOI3 window procedure hooked");
}

DeviceFunctions FindDeviceFunctions(IDirect3DDevice9* device)
{
    if (!device)
    {
        return {};
    }
    void** vtable = *reinterpret_cast<void***>(device);
    HookState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto found = state.deviceFunctions.find(vtable);
    return found != state.deviceFunctions.end()
        ? found->second
        : DeviceFunctions{};
}

bool HookDevice(IDirect3DDevice9* device, std::string& error);

HRESULT WINAPI HookedDeviceQueryInterface(
    IDirect3DDevice9* device,
    REFIID interfaceId,
    void** output
)
{
    const DeviceFunctions functions = FindDeviceFunctions(device);
    const HRESULT result = functions.queryInterface
        ? functions.queryInterface(device, interfaceId, output)
        : E_NOINTERFACE;
    if (SUCCEEDED(result)
        && output
        && *output
        && (IsEqualIID(interfaceId, __uuidof(IDirect3DDevice9))
            || IsEqualIID(interfaceId, __uuidof(IDirect3DDevice9Ex))))
    {
        std::string error;
        if (!HookDevice(
                static_cast<IDirect3DDevice9*>(*output),
                error
            ))
        {
            WriteGuiDiagnostic(
                "D3D9 queried device hook failed: " + error
            );
        }
        else
        {
            WriteGuiDiagnostic(
                "HOI3 queried D3D9 device interface hooked"
            );
        }
    }
    return result;
}

SwapChainPresentFunction FindSwapChainPresentFunction(
    IDirect3DSwapChain9* swapChain
)
{
    if (!swapChain)
    {
        return nullptr;
    }
    void** vtable = *reinterpret_cast<void***>(swapChain);
    HookState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto found = state.swapChainPresentFunctions.find(vtable);
    return found != state.swapChainPresentFunctions.end()
        ? found->second
        : nullptr;
}

void SelectPresentationDevice(IDirect3DDevice9* device)
{
    if (!device)
    {
        return;
    }
    device->AddRef();
    HookState& state = State();
    IDirect3DDevice9* previousDevice =
        state.presentationDevice.exchange(
            device,
            std::memory_order_acq_rel
        );
    if (previousDevice == device)
    {
        device->Release();
        return;
    }
    state.activeDeviceVTableOwned.store(
        false,
        std::memory_order_release
    );
    WriteGuiDiagnostic(
        previousDevice
            ? "HOI3 D3D9 presentation device changed"
            : "HOI3 D3D9 presentation device selected"
    );
    if (previousDevice)
    {
        previousDevice->Release();
    }
}

void RenderBeforePresent(IDirect3DDevice9* device)
{
    SelectPresentationDevice(device);
    EnsureWindowHook(device);
    if (SUCCEEDED(device->BeginScene()))
    {
        ScriptedGui_OnEndScene(device);
        device->EndScene();
    }
}

HRESULT WINAPI HookedEndScene(IDirect3DDevice9* device)
{
    State().endSceneCalls.fetch_add(1, std::memory_order_relaxed);
    const DeviceFunctions functions = FindDeviceFunctions(device);
    return functions.endScene ? functions.endScene(device) : D3D_OK;
}

HRESULT WINAPI HookedPresent(
    IDirect3DDevice9* device,
    const RECT* sourceRect,
    const RECT* destinationRect,
    HWND destinationWindow,
    const RGNDATA* dirtyRegion
)
{
    State().presentCalls.fetch_add(1, std::memory_order_relaxed);
    const DeviceFunctions functions = FindDeviceFunctions(device);
    RenderBeforePresent(device);
    ++DevicePresentDepth;
    const HRESULT result = functions.present
        ? functions.present(
            device,
            sourceRect,
            destinationRect,
            destinationWindow,
            dirtyRegion
        )
        : D3D_OK;
    --DevicePresentDepth;
    return result;
}

HRESULT WINAPI HookedSwapChainPresent(
    IDirect3DSwapChain9* swapChain,
    const RECT* sourceRect,
    const RECT* destinationRect,
    HWND destinationWindow,
    const RGNDATA* dirtyRegion,
    DWORD flags
)
{
    State().swapChainPresentCalls.fetch_add(
        1,
        std::memory_order_relaxed
    );
    const SwapChainPresentFunction original =
        FindSwapChainPresentFunction(swapChain);
    IDirect3DDevice9* device = nullptr;
    if (DevicePresentDepth == 0
        && swapChain
        && SUCCEEDED(swapChain->GetDevice(&device))
        && device)
    {
        RenderBeforePresent(device);
        device->Release();
    }
    return original
        ? original(
            swapChain,
            sourceRect,
            destinationRect,
            destinationWindow,
            dirtyRegion,
            flags
        )
        : D3D_OK;
}

bool HookSwapChain(
    IDirect3DSwapChain9* swapChain,
    std::string& error
)
{
    if (!swapChain)
    {
        error = "D3D9 swap chain is missing";
        return false;
    }
    void** vtable = *reinterpret_cast<void***>(swapChain);
    if (!vtable)
    {
        error = "D3D9 swap chain has no vtable";
        return false;
    }

    HookState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.swapChainPresentFunctions.find(vtable)
        != state.swapChainPresentFunctions.end())
    {
        error.clear();
        return true;
    }

    void* previous = nullptr;
    if (!ReplacePointer(
            &vtable[SwapChainPresentVTableIndex],
            reinterpret_cast<void*>(HookedSwapChainPresent),
            previous
        ))
    {
        error = "Failed to patch IDirect3DSwapChain9::Present";
        return false;
    }
    state.swapChainPresentFunctions.emplace(
        vtable,
        reinterpret_cast<SwapChainPresentFunction>(previous)
    );
    error.clear();
    WriteGuiDiagnostic("HOI3 D3D9 swap-chain Present hooked");
    return true;
}

HRESULT WINAPI HookedReset(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* parameters
)
{
    const DeviceFunctions functions = FindDeviceFunctions(device);
    HookState& state = State();
    const bool presentationReset =
        state.presentationDevice.load(std::memory_order_acquire)
        == device;
    if (presentationReset)
    {
        WriteGuiDiagnostic("HOI3 presentation device reset started");
        ScriptedGui_OnBeforeReset();
    }
    const HRESULT result = functions.reset
        ? functions.reset(device, parameters)
        : D3DERR_INVALIDCALL;
    if (presentationReset)
    {
        ScriptedGui_OnAfterReset(device, result);
        WriteGuiDiagnostic(
            SUCCEEDED(result)
                ? "HOI3 presentation device reset completed"
                : "HOI3 presentation device reset failed"
        );
    }
    return result;
}

bool HookDevice(IDirect3DDevice9* device, std::string& error)
{
    if (!device)
    {
        error = "D3D9 CreateDevice returned no device";
        return false;
    }
    void** vtable = *reinterpret_cast<void***>(device);
    if (!vtable)
    {
        error = "D3D9 device has no vtable";
        return false;
    }

    HookState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto existing = state.deviceFunctions.find(vtable);
    if (existing != state.deviceFunctions.end())
    {
        DeviceFunctions& functions = existing->second;
        bool refreshed = false;
        const auto ensureOwned = [&refreshed, &error](
            void** slot,
            void* replacement,
            auto& original,
            const char* method
        ) -> bool
        {
            if (!IsReadablePointerSlot(slot))
            {
                error = std::string("Active D3D9 vtable slot is invalid: ")
                    + method;
                return false;
            }
            if (*slot == replacement)
            {
                return true;
            }
            void* previous = nullptr;
            if (!ReplacePointer(slot, replacement, previous))
            {
                error = std::string("Failed to refresh active D3D9 method: ")
                    + method;
                return false;
            }
            original = reinterpret_cast<
                std::remove_reference_t<decltype(original)>
            >(previous);
            refreshed = true;
            return true;
        };
        const bool owned = ensureOwned(
                &vtable[DeviceQueryInterfaceVTableIndex],
                reinterpret_cast<void*>(HookedDeviceQueryInterface),
                functions.queryInterface,
                "QueryInterface"
            )
            && ensureOwned(
                &vtable[ResetVTableIndex],
                reinterpret_cast<void*>(HookedReset),
                functions.reset,
                "Reset"
            )
            && ensureOwned(
                &vtable[PresentVTableIndex],
                reinterpret_cast<void*>(HookedPresent),
                functions.present,
                "Present"
            )
            && ensureOwned(
                &vtable[EndSceneVTableIndex],
                reinterpret_cast<void*>(HookedEndScene),
                functions.endScene,
                "EndScene"
            );
        state.activeDeviceVTableOwned.store(
            owned,
            std::memory_order_release
        );
        if (!owned)
        {
            return false;
        }
        if (refreshed)
        {
            WriteGuiDiagnostic(
                "HOI3 active D3D9 device vtable hooks refreshed"
            );
        }
        error.clear();
        return true;
    }

    DeviceFunctions functions;
    void* previousQueryInterface = nullptr;
    void* previousReset = nullptr;
    void* previousPresent = nullptr;
    void* previousEndScene = nullptr;
    if (!ReplacePointer(
            &vtable[DeviceQueryInterfaceVTableIndex],
            reinterpret_cast<void*>(HookedDeviceQueryInterface),
            previousQueryInterface
        ))
    {
        error = "Failed to patch actual IDirect3DDevice9::QueryInterface";
        return false;
    }
    if (!ReplacePointer(
            &vtable[ResetVTableIndex],
            reinterpret_cast<void*>(HookedReset),
            previousReset
        ))
    {
        RestorePointerIfOwned(
            &vtable[DeviceQueryInterfaceVTableIndex],
            reinterpret_cast<void*>(HookedDeviceQueryInterface),
            previousQueryInterface
        );
        error = "Failed to patch actual IDirect3DDevice9::Reset";
        return false;
    }
    if (!ReplacePointer(
            &vtable[PresentVTableIndex],
            reinterpret_cast<void*>(HookedPresent),
            previousPresent
        ))
    {
        RestorePointerIfOwned(
            &vtable[DeviceQueryInterfaceVTableIndex],
            reinterpret_cast<void*>(HookedDeviceQueryInterface),
            previousQueryInterface
        );
        RestorePointerIfOwned(
            &vtable[ResetVTableIndex],
            reinterpret_cast<void*>(HookedReset),
            previousReset
        );
        error = "Failed to patch actual IDirect3DDevice9::Present";
        return false;
    }
    if (!ReplacePointer(
            &vtable[EndSceneVTableIndex],
            reinterpret_cast<void*>(HookedEndScene),
            previousEndScene
        ))
    {
        RestorePointerIfOwned(
            &vtable[DeviceQueryInterfaceVTableIndex],
            reinterpret_cast<void*>(HookedDeviceQueryInterface),
            previousQueryInterface
        );
        RestorePointerIfOwned(
            &vtable[PresentVTableIndex],
            reinterpret_cast<void*>(HookedPresent),
            previousPresent
        );
        RestorePointerIfOwned(
            &vtable[ResetVTableIndex],
            reinterpret_cast<void*>(HookedReset),
            previousReset
        );
        error = "Failed to patch actual IDirect3DDevice9::EndScene";
        return false;
    }

    functions.queryInterface =
        reinterpret_cast<DeviceQueryInterfaceFunction>(
            previousQueryInterface
        );
    functions.reset = reinterpret_cast<ResetFunction>(previousReset);
    functions.present = reinterpret_cast<PresentFunction>(previousPresent);
    functions.endScene = reinterpret_cast<EndSceneFunction>(previousEndScene);
    state.deviceFunctions.emplace(vtable, functions);
    state.activeDeviceVTableOwned.store(
        true,
        std::memory_order_release
    );
    error.clear();
    WriteGuiDiagnostic("HOI3 actual D3D9 device hooked");
    return true;
}

CreateDeviceFunction FindCreateDeviceFunction(IDirect3D9* direct3D)
{
    if (!direct3D)
    {
        return nullptr;
    }
    void** vtable = *reinterpret_cast<void***>(direct3D);
    HookState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto found = state.createDeviceFunctions.find(vtable);
    return found != state.createDeviceFunctions.end()
        ? found->second
        : nullptr;
}

HRESULT WINAPI HookedCreateDevice(
    IDirect3D9* direct3D,
    UINT adapter,
    D3DDEVTYPE deviceType,
    HWND focusWindow,
    DWORD behaviorFlags,
    D3DPRESENT_PARAMETERS* presentationParameters,
    IDirect3DDevice9** outputDevice
)
{
    const CreateDeviceFunction original =
        FindCreateDeviceFunction(direct3D);
    if (!original)
    {
        return D3DERR_INVALIDCALL;
    }
    const HRESULT result = original(
        direct3D,
        adapter,
        deviceType,
        focusWindow,
        behaviorFlags,
        presentationParameters,
        outputDevice
    );
    if (SUCCEEDED(result) && outputDevice && *outputDevice)
    {
        std::string error;
        if (!HookDevice(*outputDevice, error))
        {
            WriteGuiDiagnostic("D3D9 device hook failed: " + error);
        }
        IDirect3DSwapChain9* swapChain = nullptr;
        if (SUCCEEDED((*outputDevice)->GetSwapChain(0, &swapChain))
            && swapChain)
        {
            if (!HookSwapChain(swapChain, error))
            {
                WriteGuiDiagnostic(
                    "D3D9 swap-chain hook failed: " + error
                );
            }
            swapChain->Release();
        }
    }
    return result;
}

bool HookDirect3DInterface(IDirect3D9* direct3D, std::string& error)
{
    if (!direct3D)
    {
        error = "Direct3DCreate9 returned null";
        return false;
    }
    void** vtable = *reinterpret_cast<void***>(direct3D);
    if (!vtable)
    {
        error = "IDirect3D9 has no vtable";
        return false;
    }

    HookState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.createDeviceFunctions.find(vtable)
        != state.createDeviceFunctions.end())
    {
        error.clear();
        return true;
    }
    void* previous = nullptr;
    if (!ReplacePointer(
            &vtable[CreateDeviceVTableIndex],
            reinterpret_cast<void*>(HookedCreateDevice),
            previous
        ))
    {
        error = "Failed to patch actual IDirect3D9::CreateDevice";
        return false;
    }
    state.createDeviceFunctions.emplace(
        vtable,
        reinterpret_cast<CreateDeviceFunction>(previous)
    );
    error.clear();
    WriteGuiDiagnostic("HOI3 IDirect3D9::CreateDevice hooked");
    return true;
}

IDirect3D9* WINAPI HookedDirect3DCreate9(UINT sdkVersion)
{
    Direct3DCreate9Function original = nullptr;
    {
        HookState& state = State();
        std::lock_guard<std::mutex> lock(state.mutex);
        original = state.originalDirect3DCreate9;
    }
    IDirect3D9* direct3D = original ? original(sdkVersion) : nullptr;
    if (direct3D)
    {
        std::string error;
        if (!HookDirect3DInterface(direct3D, error))
        {
            WriteGuiDiagnostic("D3D9 CreateDevice hook failed: " + error);
        }
    }
    return direct3D;
}

}

bool InstallGuiD3D9Hooks(std::string& error)
{
    HookState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.installed)
    {
        error.clear();
        return true;
    }

    void** slot = FindImportSlot(
        GetModuleHandleW(nullptr),
        "d3d9.dll",
        "Direct3DCreate9"
    );
    if (!slot)
    {
        error = "Direct3DCreate9 import not found in main executable";
        return false;
    }
    const auto original = reinterpret_cast<Direct3DCreate9Function>(*slot);
    if (!original)
    {
        error = "Direct3DCreate9 import is null";
        return false;
    }

    state.direct3DCreate9Slot = slot;
    state.originalDirect3DCreate9 = original;
    void* previous = nullptr;
    if (!ReplacePointer(
            slot,
            reinterpret_cast<void*>(HookedDirect3DCreate9),
            previous
        ))
    {
        state.direct3DCreate9Slot = nullptr;
        state.originalDirect3DCreate9 = nullptr;
        error = "Failed to patch Direct3DCreate9 import";
        return false;
    }
    if (previous != reinterpret_cast<void*>(original))
    {
        state.originalDirect3DCreate9 =
            reinterpret_cast<Direct3DCreate9Function>(previous);
    }
    for (std::size_t siteId = 0;
        siteId < EngineCallsiteCount;
        ++siteId)
    {
        state.engineCallsiteCalls[siteId].store(
            0,
            std::memory_order_relaxed
        );
        state.engineCallsiteDevices[siteId].store(
            nullptr,
            std::memory_order_relaxed
        );
        state.lastDiagnosticEngineCallsiteCalls[siteId] = 0;
    }
    std::string probeError;
    if (InstallEngineCallsiteProbes(state, probeError))
    {
        WriteGuiDiagnostic(
            "HOI3 engine frame callsite probes installed"
        );
    }
    else
    {
        WriteGuiDiagnostic(
            "HOI3 engine frame callsite probes unavailable: "
            + probeError
        );
    }
    state.installed = true;
    error.clear();
    WriteGuiDiagnostic("HOI3 Direct3DCreate9 import hooked");
    return true;
}

void UninstallGuiD3D9Hooks()
{
    HookState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.window && state.originalWindowProcedure)
    {
        SetWindowLongPtrW(
            state.window,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(state.originalWindowProcedure)
        );
    }
    state.window = nullptr;
    state.originalWindowProcedure = nullptr;

    RestoreEngineCallsiteProbes(state);

    for (const auto& [vtable, functions] : state.deviceFunctions)
    {
        RestorePointerIfOwned(
            &vtable[DeviceQueryInterfaceVTableIndex],
            reinterpret_cast<void*>(HookedDeviceQueryInterface),
            reinterpret_cast<void*>(functions.queryInterface)
        );
        RestorePointerIfOwned(
            &vtable[ResetVTableIndex],
            reinterpret_cast<void*>(HookedReset),
            reinterpret_cast<void*>(functions.reset)
        );
        RestorePointerIfOwned(
            &vtable[PresentVTableIndex],
            reinterpret_cast<void*>(HookedPresent),
            reinterpret_cast<void*>(functions.present)
        );
        RestorePointerIfOwned(
            &vtable[EndSceneVTableIndex],
            reinterpret_cast<void*>(HookedEndScene),
            reinterpret_cast<void*>(functions.endScene)
        );
    }
    state.deviceFunctions.clear();

    for (const auto& [vtable, original]
        : state.swapChainPresentFunctions)
    {
        RestorePointerIfOwned(
            &vtable[SwapChainPresentVTableIndex],
            reinterpret_cast<void*>(HookedSwapChainPresent),
            reinterpret_cast<void*>(original)
        );
    }
    state.swapChainPresentFunctions.clear();

    for (const auto& [vtable, original] : state.createDeviceFunctions)
    {
        RestorePointerIfOwned(
            &vtable[CreateDeviceVTableIndex],
            reinterpret_cast<void*>(HookedCreateDevice),
            reinterpret_cast<void*>(original)
        );
    }
    state.createDeviceFunctions.clear();

    RestorePointerIfOwned(
        state.direct3DCreate9Slot,
        reinterpret_cast<void*>(HookedDirect3DCreate9),
        reinterpret_cast<void*>(state.originalDirect3DCreate9)
    );
    state.direct3DCreate9Slot = nullptr;
    state.originalDirect3DCreate9 = nullptr;
    IDirect3DDevice9* presentationDevice =
        state.presentationDevice.exchange(
            nullptr,
            std::memory_order_acq_rel
        );
    state.activeDeviceVTableOwned.store(false, std::memory_order_release);
    state.installed = false;
    if (presentationDevice)
    {
        presentationDevice->Release();
    }
}

bool AreGuiD3D9HooksInstalled()
{
    HookState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.installed;
}

void MaintainGuiD3D9Hooks()
{
    HookState& state = State();
    const uint64_t now = GetTickCount64();
    uint64_t previousMaintenance =
        state.lastMaintenanceMilliseconds.load(
            std::memory_order_relaxed
        );
    if (now - previousMaintenance < 1000
        || !state.lastMaintenanceMilliseconds.compare_exchange_strong(
            previousMaintenance,
            now,
            std::memory_order_acq_rel
        ))
    {
        return;
    }

    IDirect3DDevice9* presentationDevice = nullptr;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        if (!state.installed)
        {
            return;
        }
        presentationDevice = state.presentationDevice.load(
            std::memory_order_acquire
        );
        if (presentationDevice)
        {
            presentationDevice->AddRef();
        }
    }

    if (presentationDevice)
    {
        std::string refreshError;
        if (!HookDevice(presentationDevice, refreshError))
        {
            WriteGuiDiagnostic(
                "D3D9 active device refresh failed: " + refreshError
            );
        }
        IDirect3DSwapChain9* swapChain = nullptr;
        if (SUCCEEDED(presentationDevice->GetSwapChain(0, &swapChain))
            && swapChain)
        {
            if (!HookSwapChain(swapChain, refreshError))
            {
                WriteGuiDiagnostic(
                    "D3D9 active swap-chain refresh failed: "
                    + refreshError
                );
            }
            swapChain->Release();
        }
        presentationDevice->Release();
    }

    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.installed)
    {
        return;
    }
    if (now - state.lastDiagnosticMilliseconds >= 30000)
    {
        const uint64_t endSceneCalls =
            state.endSceneCalls.load(std::memory_order_relaxed);
        const uint64_t presentCalls =
            state.presentCalls.load(std::memory_order_relaxed);
        const uint64_t swapChainPresentCalls =
            state.swapChainPresentCalls.load(
                std::memory_order_relaxed
            );
        std::string engineCallsites = "[";
        for (std::size_t siteId = 0;
            siteId < EngineCallsiteCount;
            ++siteId)
        {
            const uint64_t calls =
                state.engineCallsiteCalls[siteId].load(
                    std::memory_order_relaxed
                );
            if (siteId != 0)
            {
                engineCallsites += ", ";
            }
            engineCallsites += std::to_string(siteId)
                + "=" + std::to_string(calls)
                + " (+" + std::to_string(
                    calls
                    - state.lastDiagnosticEngineCallsiteCalls[siteId]
                ) + ")";
            state.lastDiagnosticEngineCallsiteCalls[siteId] = calls;
        }
        engineCallsites += "]";
        WriteGuiDiagnostic(
            "D3D9 hook heartbeat: EndScene="
            + std::to_string(endSceneCalls)
            + " (+"
            + std::to_string(
                endSceneCalls - state.lastDiagnosticEndSceneCalls
            )
            + "), Present="
            + std::to_string(presentCalls)
            + " (+"
            + std::to_string(
                presentCalls - state.lastDiagnosticPresentCalls
            )
            + "), SwapChainPresent="
            + std::to_string(swapChainPresentCalls)
            + " (+"
            + std::to_string(
                swapChainPresentCalls
                - state.lastDiagnosticSwapChainPresentCalls
            )
            + "), EngineCallsites="
            + engineCallsites
            + ", ActiveVTableOwned="
            + (state.activeDeviceVTableOwned.load(
                    std::memory_order_acquire
                )
                ? "yes"
                : "no")
        );
        state.lastDiagnosticMilliseconds = now;
        state.lastDiagnosticEndSceneCalls = endSceneCalls;
        state.lastDiagnosticPresentCalls = presentCalls;
        state.lastDiagnosticSwapChainPresentCalls =
            swapChainPresentCalls;
    }
}
