#include "gui_behavior.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace
{

std::string Trim(std::string value)
{
    const auto notSpace = [](unsigned char character)
    {
        return !std::isspace(character);
    };
    value.erase(
        value.begin(),
        std::find_if(value.begin(), value.end(), notSpace)
    );
    value.erase(
        std::find_if(
            value.rbegin(),
            value.rend(),
            notSpace
        ).base(),
        value.end()
    );
    return value;
}

std::string Lower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
}

std::string RemoveComment(std::string line)
{
    bool quoted = false;
    for (size_t index = 0; index < line.size(); ++index)
    {
        if (line[index] == '"')
        {
            quoted = !quoted;
        }
        else if (line[index] == '#' && !quoted)
        {
            line.resize(index);
            break;
        }
    }
    return line;
}

std::string ParseScalar(std::string value)
{
    value = Trim(std::move(value));
    if (value.size() >= 2
        && value.front() == '"'
        && value.back() == '"')
    {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::vector<std::string> ParseTokens(std::string value)
{
    value = Trim(std::move(value));
    if (!value.empty() && value.front() == '{')
    {
        value.erase(value.begin());
    }
    if (!value.empty() && value.back() == '}')
    {
        value.pop_back();
    }

    std::vector<std::string> tokens;
    std::istringstream stream(value);
    std::string token;
    while (stream >> token)
    {
        tokens.push_back(ParseScalar(std::move(token)));
    }
    return tokens;
}

bool ParseAssignment(
    const std::string& line,
    std::string& key,
    std::string& value
)
{
    const size_t equalPosition = line.find('=');
    if (equalPosition == std::string::npos)
    {
        return false;
    }

    key = Lower(Trim(line.substr(0, equalPosition)));
    value = Trim(line.substr(equalPosition + 1));
    return !key.empty();
}

}

bool GuiBehaviorDefinition::AcceptsPhase(
    std::string_view phase
) const
{
    return phases.empty()
        || phases.find(std::string(phase)) != phases.end();
}

bool GuiBehaviorRegistry::LoadDirectory(
    const std::filesystem::path& root,
    std::string& error,
    std::vector<std::string>* diagnostics
)
{
    if (!std::filesystem::exists(root)
        || !std::filesystem::is_directory(root))
    {
        error = "behavior_directory_not_found";
        return false;
    }

    std::vector<std::filesystem::path> paths;
    for (const std::filesystem::directory_entry& entry
        : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file()
            || entry.path().extension() != ".txt")
        {
            continue;
        }

        paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());

    for (const std::filesystem::path& path : paths)
    {
        std::string fileError;
        if (!LoadFile(path, fileError))
        {
            if (diagnostics)
            {
                diagnostics->push_back(fileError);
            }
        }
    }

    error.clear();
    return true;
}

bool GuiBehaviorRegistry::LoadFile(
    const std::filesystem::path& path,
    std::string& error
)
{
    std::ifstream file(path);
    if (!file)
    {
        error = "behavior_file_not_found: " + path.string();
        return false;
    }

    GuiBehaviorDefinition current;
    std::vector<GuiBehaviorDefinition> pending;
    std::unordered_set<std::string> pendingNames;
    bool inBehavior = false;
    int depth = 0;
    int lineNumber = 0;
    std::string rawLine;
    while (std::getline(file, rawLine))
    {
        ++lineNumber;
        const std::string line = Trim(RemoveComment(rawLine));
        if (line.empty())
        {
            continue;
        }

        if (!inBehavior)
        {
            if (line.find("behavior") != std::string::npos
                && line.find('{') != std::string::npos)
            {
                current = {};
                inBehavior = true;
                depth = 1;
            }
            continue;
        }

        std::string key;
        std::string value;
        if (ParseAssignment(line, key, value))
        {
            if (key == "name")
            {
                current.name = ParseScalar(value);
            }
            else if (key == "function"
                || key == "lua"
                || key == "callback")
            {
                current.functionName = ParseScalar(value);
            }
            else if (key == "fallback"
                || key == "fallbackoperation")
            {
                current.fallbackOperation = ParseScalar(value);
            }
            else if (key == "enabledwhen"
                || key == "condition")
            {
                current.enabledWhen = ParseScalar(value);
            }
            else if (key == "phase")
            {
                current.phases.insert(
                    Lower(ParseScalar(value))
                );
            }
            else if (key == "phases")
            {
                for (std::string token : ParseTokens(value))
                {
                    current.phases.insert(Lower(std::move(token)));
                }
            }
            else if (!value.empty() && value.front() != '{')
            {
                current.parameters[key] = ParseScalar(value);
            }
        }

        depth += static_cast<int>(
            std::count(line.begin(), line.end(), '{')
        );
        depth -= static_cast<int>(
            std::count(line.begin(), line.end(), '}')
        );

        if (depth <= 0)
        {
            if (current.name.empty())
            {
                error = "behavior_name_missing: "
                    + path.string()
                    + ":"
                    + std::to_string(lineNumber);
                return false;
            }

            if (current.functionName.empty())
            {
                current.functionName = current.name;
            }

            if (definitions_.find(current.name) != definitions_.end()
                || !pendingNames.insert(current.name).second)
            {
                error = "behavior_name_duplicate: "
                    + current.name + ": " + path.string()
                    + ":" + std::to_string(lineNumber);
                return false;
            }
            pending.push_back(std::move(current));
            inBehavior = false;
            depth = 0;
        }
    }

    if (inBehavior)
    {
        error = "behavior_block_not_closed: " + path.string();
        return false;
    }

    for (GuiBehaviorDefinition& definition : pending)
    {
        definitions_.emplace(
            definition.name,
            std::move(definition)
        );
    }

    error.clear();
    return true;
}

void GuiBehaviorRegistry::Clear()
{
    definitions_.clear();
}

const GuiBehaviorDefinition* GuiBehaviorRegistry::Find(
    std::string_view name
) const
{
    const auto iterator = definitions_.find(std::string(name));
    return iterator == definitions_.end()
        ? nullptr
        : &iterator->second;
}

std::size_t GuiBehaviorRegistry::Size() const
{
    return definitions_.size();
}
