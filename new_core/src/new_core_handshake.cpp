#include "new_core_handshake.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace new_core
{
namespace
{

template <std::size_t Capacity>
void CopyText(char (&output)[Capacity], std::string_view value)
{
    const std::size_t count = std::min(
        value.size(),
        Capacity - 1
    );
    std::memcpy(output, value.data(), count);
    output[count] = '\0';
}

bool ValidateMapping(
    const HandshakeMapping& mapping,
    std::string* error
)
{
    if (!mapping.handle || !mapping.block)
    {
        if (error)
        {
            *error = "new_core_handshake_mapping_missing";
        }
        return false;
    }
    if (mapping.block->magic != HandshakeMagic
        || mapping.block->version != HandshakeVersion
        || mapping.block->size != sizeof(HandshakeBlock))
    {
        if (error)
        {
            *error = "new_core_handshake_version_mismatch";
        }
        return false;
    }
    return true;
}

}

bool CreateHandshakeMapping(
    std::wstring_view name,
    HandshakeMapping& mapping,
    std::wstring& error
)
{
    CloseHandshakeMapping(mapping);
    if (name.empty())
    {
        error = L"handshake_name_missing";
        return false;
    }
    const std::wstring ownedName(name);
    mapping.handle = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(HandshakeBlock),
        ownedName.c_str()
    );
    if (!mapping.handle)
    {
        error = L"CreateFileMappingW failed: "
            + std::to_wstring(GetLastError());
        return false;
    }
    mapping.block = static_cast<HandshakeBlock*>(MapViewOfFile(
        mapping.handle,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(HandshakeBlock)
    ));
    if (!mapping.block)
    {
        error = L"MapViewOfFile failed: "
            + std::to_wstring(GetLastError());
        CloseHandshakeMapping(mapping);
        return false;
    }
    ZeroMemory(mapping.block, sizeof(HandshakeBlock));
    mapping.block->magic = HandshakeMagic;
    mapping.block->version = HandshakeVersion;
    mapping.block->size = sizeof(HandshakeBlock);
    MemoryBarrier();
    InterlockedExchange(
        &mapping.block->state,
        static_cast<LONG>(HandshakeState::LauncherReady)
    );
    error.clear();
    return true;
}

bool OpenHandshakeMappingFromEnvironment(
    HandshakeMapping& mapping,
    std::string& error
)
{
    CloseHandshakeMapping(mapping);
    std::array<wchar_t, 512> name{};
    const DWORD length = GetEnvironmentVariableW(
        HandshakeEnvironmentName,
        name.data(),
        static_cast<DWORD>(name.size())
    );
    if (length == 0 || length >= name.size())
    {
        error = "new_core_handshake_not_requested";
        return false;
    }
    mapping.handle = OpenFileMappingW(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        name.data()
    );
    if (!mapping.handle)
    {
        error = "new_core_handshake_open_failed: "
            + std::to_string(GetLastError());
        return false;
    }
    mapping.block = static_cast<HandshakeBlock*>(MapViewOfFile(
        mapping.handle,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(HandshakeBlock)
    ));
    if (!mapping.block)
    {
        error = "new_core_handshake_map_failed: "
            + std::to_string(GetLastError());
        CloseHandshakeMapping(mapping);
        return false;
    }
    if (!ValidateMapping(mapping, &error))
    {
        CloseHandshakeMapping(mapping);
        return false;
    }
    error.clear();
    return true;
}

void CloseHandshakeMapping(HandshakeMapping& mapping)
{
    if (mapping.block)
    {
        UnmapViewOfFile(mapping.block);
        mapping.block = nullptr;
    }
    if (mapping.handle)
    {
        CloseHandle(mapping.handle);
        mapping.handle = nullptr;
    }
}

void PublishHandshake(
    HandshakeMapping& mapping,
    HandshakeState state,
    std::string_view message,
    std::string_view modules,
    std::string_view hooks,
    uint32_t processId,
    uint32_t abiVersion
)
{
    if (!ValidateMapping(mapping, nullptr))
    {
        return;
    }
    if (processId != 0)
    {
        mapping.block->processId = processId;
    }
    if (abiVersion != 0)
    {
        mapping.block->abiVersion = abiVersion;
    }
    CopyText(mapping.block->message, message);
    CopyText(mapping.block->modules, modules);
    CopyText(mapping.block->hooks, hooks);
    InterlockedIncrement(&mapping.block->sequence);
    MemoryBarrier();
    InterlockedExchange(
        &mapping.block->state,
        static_cast<LONG>(state)
    );
}

HandshakeState ReadHandshakeState(
    const HandshakeMapping& mapping
)
{
    if (!ValidateMapping(mapping, nullptr))
    {
        return HandshakeState::Empty;
    }
    return static_cast<HandshakeState>(
        InterlockedCompareExchange(
            &mapping.block->state,
            0,
            0
        )
    );
}

const char* HandshakeStateName(HandshakeState state)
{
    switch (state)
    {
    case HandshakeState::LauncherReady:
        return "launcher_ready";
    case HandshakeState::ProcessCreated:
        return "process_created";
    case HandshakeState::DllWorkerStarted:
        return "dll_worker_started";
    case HandshakeState::RuntimeInitialized:
        return "runtime_initialized";
    case HandshakeState::HooksInstalling:
        return "hooks_installing";
    case HandshakeState::Ready:
        return "ready";
    case HandshakeState::Failed:
        return "failed";
    case HandshakeState::Empty:
    default:
        return "empty";
    }
}

}
