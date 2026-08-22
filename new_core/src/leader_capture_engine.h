#pragma once

#include <cstdint>
#include <string>

namespace leader_capture
{

bool Initialize(std::string& error);
bool InstallHooks(std::string& error);
void UninstallHooks();
bool AreHooksInstalled();

void SetGameplayActive(bool active);
bool IsGameplayActive();
void ResetSessionState();
void Tick(uint64_t nowMilliseconds);

}
