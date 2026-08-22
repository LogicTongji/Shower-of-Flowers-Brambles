#include "core_module_registry.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace core
{

bool ModuleRegistry::Register(
    std::unique_ptr<IModule> module,
    std::string& error
)
{
    if (initialized_)
    {
        error = "core_module_registry_initialized";
        return false;
    }
    if (!module || module->Id().empty())
    {
        error = "core_module_id_missing";
        return false;
    }
    if (Find(module->Id()))
    {
        error = "core_module_duplicate: "
            + std::string(module->Id());
        return false;
    }
    entries_.push_back({std::move(module), false});
    error.clear();
    return true;
}

bool ModuleRegistry::InitializeAll(
    Services& services,
    std::string& error
)
{
    if (initialized_)
    {
        error.clear();
        return true;
    }
    diagnostic_ = services.diagnostic;
    std::stable_sort(
        entries_.begin(),
        entries_.end(),
        [](const Entry& left, const Entry& right)
        {
            return left.module->Priority()
                    != right.module->Priority()
                ? left.module->Priority()
                    < right.module->Priority()
                : left.module->Id() < right.module->Id();
        }
    );

    for (Entry& entry : entries_)
    {
        try
        {
            std::string moduleError;
            if (!entry.module->Initialize(services, moduleError))
            {
                error = "core_module_initialize_failed: "
                    + std::string(entry.module->Id());
                if (!moduleError.empty())
                {
                    error += ": " + moduleError;
                }
                ShutdownAll();
                return false;
            }
            entry.initialized = true;
        }
        catch (const std::exception& exception)
        {
            error = "core_module_initialize_exception: "
                + std::string(entry.module->Id())
                + ": " + exception.what();
            ShutdownAll();
            return false;
        }
        catch (...)
        {
            error = "core_module_initialize_exception: "
                + std::string(entry.module->Id());
            ShutdownAll();
            return false;
        }
    }
    initialized_ = true;
    error.clear();
    return true;
}

void ModuleRegistry::DispatchLifecycle(
    const LifecycleEvent& event
)
{
    for (Entry& entry : entries_)
    {
        if (!entry.initialized)
        {
            continue;
        }
        try
        {
            entry.module->OnLifecycleEvent(event);
        }
        catch (const std::exception& exception)
        {
            Diagnose(
                "core module lifecycle exception: "
                + std::string(entry.module->Id())
                + ": " + exception.what()
            );
        }
        catch (...)
        {
            Diagnose(
                "core module lifecycle exception: "
                + std::string(entry.module->Id())
            );
        }
    }
}

void ModuleRegistry::Tick(uint64_t nowMilliseconds)
{
    for (Entry& entry : entries_)
    {
        if (!entry.initialized)
        {
            continue;
        }
        try
        {
            entry.module->Tick(nowMilliseconds);
        }
        catch (const std::exception& exception)
        {
            Diagnose(
                "core module tick exception: "
                + std::string(entry.module->Id())
                + ": " + exception.what()
            );
        }
        catch (...)
        {
            Diagnose(
                "core module tick exception: "
                + std::string(entry.module->Id())
            );
        }
    }
}

void ModuleRegistry::ShutdownAll()
{
    for (auto iterator = entries_.rbegin();
        iterator != entries_.rend();
        ++iterator)
    {
        if (!iterator->initialized)
        {
            continue;
        }
        try
        {
            iterator->module->Shutdown();
        }
        catch (...)
        {
        }
        iterator->initialized = false;
    }
    initialized_ = false;
}

IModule* ModuleRegistry::Find(std::string_view id) const
{
    const auto found = std::find_if(
        entries_.begin(),
        entries_.end(),
        [id](const Entry& entry)
        {
            return entry.module && entry.module->Id() == id;
        }
    );
    return found == entries_.end()
        ? nullptr
        : found->module.get();
}

bool ModuleRegistry::IsInitialized() const
{
    return initialized_;
}

std::vector<std::string> ModuleRegistry::ModuleIds() const
{
    std::vector<std::string> output;
    output.reserve(entries_.size());
    for (const Entry& entry : entries_)
    {
        output.emplace_back(entry.module->Id());
    }
    return output;
}

void ModuleRegistry::Diagnose(std::string_view message) const
{
    if (diagnostic_)
    {
        diagnostic_(message);
    }
}

}
