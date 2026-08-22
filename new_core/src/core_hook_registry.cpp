#include "core_hook_registry.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace core
{

bool HookRegistry::Register(
    HookDefinition definition,
    std::string& error
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (sealed_)
    {
        error = "core_hook_registry_sealed";
        return false;
    }
    if (definition.id.empty())
    {
        error = "core_hook_id_missing";
        return false;
    }
    if (!definition.install
        || !definition.uninstall
        || !definition.isInstalled)
    {
        error = "core_hook_callbacks_missing: " + definition.id;
        return false;
    }
    const auto duplicate = std::find_if(
        hooks_.begin(),
        hooks_.end(),
        [&definition](const HookDefinition& hook)
        {
            return hook.id == definition.id;
        }
    );
    if (duplicate != hooks_.end())
    {
        error = "core_hook_duplicate: " + definition.id;
        return false;
    }
    hooks_.push_back(std::move(definition));
    sorted_ = false;
    error.clear();
    return true;
}

bool HookRegistry::InstallAll(std::string& error)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sealed_ = true;
    SortUnlocked();
    error.clear();
    bool ready = true;
    for (HookDefinition& hook : hooks_)
    {
        try
        {
            if (hook.isInstalled())
            {
                continue;
            }
            std::string hookError;
            if (!hook.install(hookError))
            {
                ready = false;
                if (error.empty())
                {
                    error = "core_hook_install_failed: " + hook.id;
                    if (!hookError.empty())
                    {
                        error += ": " + hookError;
                    }
                }
            }
        }
        catch (const std::exception& exception)
        {
            ready = false;
            if (error.empty())
            {
                error = "core_hook_install_exception: "
                    + hook.id + ": " + exception.what();
            }
        }
        catch (...)
        {
            ready = false;
            if (error.empty())
            {
                error = "core_hook_install_exception: " + hook.id;
            }
        }
    }
    return ready;
}

void HookRegistry::Maintain()
{
    std::lock_guard<std::mutex> lock(mutex_);
    SortUnlocked();
    for (HookDefinition& hook : hooks_)
    {
        try
        {
            if (hook.maintain && hook.isInstalled())
            {
                hook.maintain();
            }
        }
        catch (...)
        {
        }
    }
}

void HookRegistry::UninstallAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    SortUnlocked();
    for (auto iterator = hooks_.rbegin();
        iterator != hooks_.rend();
        ++iterator)
    {
        try
        {
            if (iterator->isInstalled())
            {
                iterator->uninstall();
            }
        }
        catch (...)
        {
        }
    }
}

bool HookRegistry::AreAllInstalled() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (hooks_.empty())
    {
        return false;
    }
    return std::all_of(
        hooks_.begin(),
        hooks_.end(),
        [](const HookDefinition& hook)
        {
            try
            {
                return hook.isInstalled();
            }
            catch (...)
            {
                return false;
            }
        }
    );
}

std::vector<HookStatus> HookRegistry::Status() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HookStatus> output;
    output.reserve(hooks_.size());
    for (const HookDefinition& hook : hooks_)
    {
        bool installed = false;
        try
        {
            installed = hook.isInstalled();
        }
        catch (...)
        {
        }
        output.push_back({hook.id, hook.priority, installed});
    }
    std::sort(
        output.begin(),
        output.end(),
        [](const HookStatus& left, const HookStatus& right)
        {
            return left.priority != right.priority
                ? left.priority < right.priority
                : left.id < right.id;
        }
    );
    return output;
}

void HookRegistry::SortUnlocked()
{
    if (sorted_)
    {
        return;
    }
    std::stable_sort(
        hooks_.begin(),
        hooks_.end(),
        [](const HookDefinition& left, const HookDefinition& right)
        {
            return left.priority != right.priority
                ? left.priority < right.priority
                : left.id < right.id;
        }
    );
    sorted_ = true;
}

}
