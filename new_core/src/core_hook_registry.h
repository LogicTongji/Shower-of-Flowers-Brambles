#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace core
{

struct HookDefinition
{
    std::string id;
    int priority = 0;
    std::function<bool(std::string&)> install;
    std::function<void()> uninstall;
    std::function<bool()> isInstalled;
    std::function<void()> maintain;
};

struct HookStatus
{
    std::string id;
    int priority = 0;
    bool installed = false;
};

class HookRegistry
{
public:
    bool Register(HookDefinition definition, std::string& error);
    bool InstallAll(std::string& error);
    void Maintain();
    void UninstallAll();

    bool AreAllInstalled() const;
    std::vector<HookStatus> Status() const;

private:
    void SortUnlocked();

    mutable std::mutex mutex_;
    std::vector<HookDefinition> hooks_;
    bool sorted_ = true;
    bool sealed_ = false;
};

}
