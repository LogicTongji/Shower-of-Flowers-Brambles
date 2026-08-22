#pragma once

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>

#include "new_core_handshake.h"

namespace new_core
{

enum class LauncherMode
{
    OriginalLauncher,
    InjectedGame
};

struct LauncherConfig
{
    LauncherMode mode = LauncherMode::InjectedGame;
    std::filesystem::path gameExecutable;
    std::filesystem::path originalLauncher;
    std::filesystem::path coreLibrary;
    std::filesystem::path projectRoot;
    std::filesystem::path modDescriptor;
    std::wstring extraArguments;
    uint32_t handshakeTimeoutMilliseconds = 125000;
    bool preventDuplicateGame = true;
};

struct LaunchResult
{
    bool success = false;
    bool processStarted = false;
    bool libraryInjected = false;
    bool coreReady = false;
    DWORD processId = 0;
    HandshakeState handshakeState = HandshakeState::Empty;
    std::wstring message;
    std::string modules;
    std::string hooks;
};

LauncherConfig MakeDefaultLauncherConfig(
    const std::filesystem::path& launcherDirectory
);

bool LoadLauncherConfig(
    const std::filesystem::path& path,
    LauncherConfig& config,
    std::wstring& error
);

bool SaveLauncherConfig(
    const std::filesystem::path& path,
    const LauncherConfig& config,
    std::wstring& error
);

bool ValidateLauncherConfig(
    const LauncherConfig& config,
    std::wstring& error
);

bool IsPe32I386(
    const std::filesystem::path& path,
    std::wstring& error
);

std::wstring BuildInjectedGameCommandLine(
    const LauncherConfig& config
);

LaunchResult LaunchFromConfig(const LauncherConfig& config);

}
