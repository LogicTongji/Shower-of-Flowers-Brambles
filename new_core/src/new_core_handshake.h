#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace new_core
{

constexpr uint32_t HandshakeMagic = 0x4E434853;
constexpr uint32_t HandshakeVersion = 1;
constexpr wchar_t HandshakeEnvironmentName[] =
    L"NEW_CORE_HANDSHAKE_NAME";

enum class HandshakeState : LONG
{
    Empty = 0,
    LauncherReady = 1,
    ProcessCreated = 2,
    DllWorkerStarted = 3,
    RuntimeInitialized = 4,
    HooksInstalling = 5,
    Ready = 6,
    Failed = 7
};

#pragma pack(push, 4)
struct HandshakeBlock
{
    uint32_t magic = HandshakeMagic;
    uint32_t version = HandshakeVersion;
    uint32_t size = 0;
    volatile LONG state = static_cast<LONG>(HandshakeState::Empty);
    volatile LONG sequence = 0;
    uint32_t processId = 0;
    uint32_t abiVersion = 0;
    char message[512]{};
    char modules[512]{};
    char hooks[1024]{};
};
#pragma pack(pop)

struct HandshakeMapping
{
    HANDLE handle = nullptr;
    HandshakeBlock* block = nullptr;
};

bool CreateHandshakeMapping(
    std::wstring_view name,
    HandshakeMapping& mapping,
    std::wstring& error
);

bool OpenHandshakeMappingFromEnvironment(
    HandshakeMapping& mapping,
    std::string& error
);

void CloseHandshakeMapping(HandshakeMapping& mapping);

void PublishHandshake(
    HandshakeMapping& mapping,
    HandshakeState state,
    std::string_view message,
    std::string_view modules = {},
    std::string_view hooks = {},
    uint32_t processId = 0,
    uint32_t abiVersion = 0
);

HandshakeState ReadHandshakeState(
    const HandshakeMapping& mapping
);

const char* HandshakeStateName(HandshakeState state);

}
