#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct GuiBehaviorDefinition
{
    std::string name;
    std::string functionName;
    std::string fallbackOperation;
    std::string enabledWhen;
    std::unordered_map<std::string, std::string> parameters;
    std::unordered_set<std::string> phases;

    bool AcceptsPhase(
        std::string_view phase
    ) const;
};

class GuiBehaviorRegistry
{
public:
    bool LoadDirectory(
        const std::filesystem::path& root,
        std::string& error,
        std::vector<std::string>* diagnostics = nullptr
    );

    bool LoadFile(
        const std::filesystem::path& path,
        std::string& error
    );

    void Clear();

    const GuiBehaviorDefinition* Find(
        std::string_view name
    ) const;

    std::size_t Size() const;

private:
    std::unordered_map<
        std::string,
        GuiBehaviorDefinition
    > definitions_;
};
