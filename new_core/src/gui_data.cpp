#include "gui_data.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
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

std::string Normalize(std::string value)
{
    value = Trim(std::move(value));
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

bool IsTruthy(std::string value)
{
    value = Normalize(std::move(value));
    return value == "yes"
        || value == "true"
        || value == "1"
        || value == "on";
}

std::vector<std::string> Split(
    const std::string& expression,
    const std::string& separator
)
{
    std::vector<std::string> parts;
    size_t start = 0;
    size_t position = expression.find(separator);
    while (position != std::string::npos)
    {
        parts.push_back(expression.substr(start, position - start));
        start = position + separator.size();
        position = expression.find(separator, start);
    }
    parts.push_back(expression.substr(start));
    return parts;
}

std::string ValueToText(const GuiDataValue& value)
{
    if (const auto* boolean = std::get_if<bool>(&value))
    {
        return *boolean ? "true" : "false";
    }

    if (const auto* integer = std::get_if<int64_t>(&value))
    {
        return std::to_string(*integer);
    }

    if (const auto* number = std::get_if<double>(&value))
    {
        std::ostringstream stream;
        stream << std::setprecision(15) << *number;
        return stream.str();
    }

    if (const auto* string = std::get_if<std::string>(&value))
    {
        return *string;
    }

    return {};
}

double TextToNumber(const std::string& value)
{
    char* end = nullptr;
    const double number = std::strtod(value.c_str(), &end);
    return end == value.c_str() ? 0.0 : number;
}

}

std::string GuiDataValueToText(const GuiDataValue& value)
{
    return ValueToText(value);
}

double GuiDataValueToNumber(const GuiDataValue& value)
{
    if (const auto* integer = std::get_if<int64_t>(&value))
    {
        return static_cast<double>(*integer);
    }
    if (const auto* number = std::get_if<double>(&value))
    {
        return *number;
    }
    if (const auto* boolean = std::get_if<bool>(&value))
    {
        return *boolean ? 1.0 : 0.0;
    }
    return TextToNumber(ValueToText(value));
}

bool GuiDataValueToBool(const GuiDataValue& value)
{
    return IsTruthy(ValueToText(value));
}

void GuiDataRegistry::Clear()
{
    values_.clear();
    lists_.clear();
}

void GuiDataRegistry::Set(
    std::string name,
    GuiDataValue value
)
{
    values_[Normalize(std::move(name))] = std::move(value);
}

void GuiDataRegistry::Set(
    std::string name,
    bool value
)
{
    Set(std::move(name), GuiDataValue(value));
}

void GuiDataRegistry::Set(
    std::string name,
    int64_t value
)
{
    Set(std::move(name), GuiDataValue(value));
}

void GuiDataRegistry::Set(
    std::string name,
    int value
)
{
    Set(std::move(name), static_cast<int64_t>(value));
}

void GuiDataRegistry::Set(
    std::string name,
    uint64_t value
)
{
    Set(
        std::move(name),
        static_cast<int64_t>(value)
    );
}

void GuiDataRegistry::Set(
    std::string name,
    double value
)
{
    Set(std::move(name), GuiDataValue(value));
}

void GuiDataRegistry::Set(
    std::string name,
    std::string value
)
{
    Set(std::move(name), GuiDataValue(std::move(value)));
}

void GuiDataRegistry::Set(
    std::string name,
    const char* value
)
{
    Set(
        std::move(name),
        std::string(value ? value : "")
    );
}

void GuiDataRegistry::SetList(
    std::string name,
    GuiListModel value
)
{
    lists_[Normalize(std::move(name))] = std::move(value);
}

bool GuiDataRegistry::Remove(std::string_view name)
{
    return values_.erase(Normalize(std::string(name))) > 0;
}

bool GuiDataRegistry::RemoveList(std::string_view name)
{
    return lists_.erase(Normalize(std::string(name))) > 0;
}

const GuiDataValue* GuiDataRegistry::Find(
    std::string_view name
) const
{
    const auto iterator = values_.find(Normalize(std::string(name)));
    return iterator == values_.end() ? nullptr : &iterator->second;
}

const GuiListModel* GuiDataRegistry::FindList(
    std::string_view name
) const
{
    const auto iterator = lists_.find(Normalize(std::string(name)));
    return iterator == lists_.end() ? nullptr : &iterator->second;
}

std::string GuiDataRegistry::ResolveDataPath(
    std::string_view path,
    int depth
) const
{
    std::string resolved(path);
    if (depth >= 8)
    {
        return resolved;
    }

    size_t open = resolved.find('{');
    while (open != std::string::npos)
    {
        const size_t close = resolved.find('}', open + 1);
        if (close == std::string::npos)
        {
            break;
        }

        const std::string binding = resolved.substr(
            open + 1,
            close - open - 1
        );
        const std::string bindingPath = ResolveDataPath(
            binding,
            depth + 1
        );
        const GuiDataValue* bindingValue = Find(bindingPath);
        const std::string replacement = bindingValue
            ? ValueToText(*bindingValue)
            : std::string{};
        resolved.replace(
            open,
            close - open + 1,
            replacement
        );
        open = resolved.find('{', open + replacement.size());
    }
    return resolved;
}

std::string GuiDataRegistry::ResolveText(
    std::string_view name
) const
{
    const GuiDataValue* value = Find(ResolveDataPath(name));
    return value ? ValueToText(*value) : std::string{};
}

double GuiDataRegistry::ResolveNumber(
    std::string_view name
) const
{
    const GuiDataValue* value = Find(ResolveDataPath(name));
    if (!value)
    {
        return 0.0;
    }
    if (const auto* integer = std::get_if<int64_t>(value))
    {
        return static_cast<double>(*integer);
    }
    if (const auto* number = std::get_if<double>(value))
    {
        return *number;
    }
    if (const auto* boolean = std::get_if<bool>(value))
    {
        return *boolean ? 1.0 : 0.0;
    }
    return TextToNumber(ValueToText(*value));
}

bool GuiDataRegistry::ResolveBool(
    std::string_view name
) const
{
    const GuiDataValue* value = Find(ResolveDataPath(name));
    return value && IsTruthy(ValueToText(*value));
}

bool GuiDataRegistry::EvaluateCondition(
    std::string_view expression
) const
{
    std::string value = Trim(std::string(expression));
    if (value.empty())
    {
        return true;
    }

    const std::vector<std::string> orParts = Split(value, "||");
    if (orParts.size() > 1)
    {
        for (const std::string& part : orParts)
        {
            if (EvaluateCondition(part))
            {
                return true;
            }
        }
        return false;
    }

    const std::vector<std::string> andParts = Split(value, "&&");
    if (andParts.size() > 1)
    {
        for (const std::string& part : andParts)
        {
            if (!EvaluateCondition(part))
            {
                return false;
            }
        }
        return true;
    }

    if (!value.empty() && value.front() == '!')
    {
        return !EvaluateCondition(value.substr(1));
    }

    const size_t equalPosition = value.find("==");
    const size_t notEqualPosition = value.find("!=");
    const size_t operatorPosition = equalPosition != std::string::npos
        ? equalPosition
        : notEqualPosition;
    if (operatorPosition != std::string::npos)
    {
        const bool notEqual = notEqualPosition != std::string::npos
            && notEqualPosition == operatorPosition;
        const std::string left = Normalize(
            value.substr(0, operatorPosition)
        );
        std::string right = Trim(value.substr(
            operatorPosition + 2
        ));
        if (right.size() >= 2
            && ((right.front() == '"' && right.back() == '"')
                || (right.front() == '\'' && right.back() == '\'')))
        {
            right = right.substr(1, right.size() - 2);
        }

        const GuiDataValue* actualValue = Find(ResolveDataPath(left));
        const std::string actual = actualValue
            ? Normalize(ValueToText(*actualValue))
            : left;
        const bool equal = actual == Normalize(std::move(right));
        return notEqual ? !equal : equal;
    }

    std::string key = Normalize(std::move(value));
    if (key.size() >= 2
        && key.front() == '('
        && key.back() == ')')
    {
        return EvaluateCondition(key.substr(1, key.size() - 2));
    }

    const GuiDataValue* actualValue = Find(ResolveDataPath(key));
    return actualValue
        ? IsTruthy(ValueToText(*actualValue))
        : IsTruthy(key);
}

gui::GuiLayoutContext GuiDataRegistry::MakeLayoutContext() const
{
    return {
        [this](std::string_view expression)
        {
            return EvaluateCondition(expression);
        },
        [this](std::string_view name)
        {
            return ResolveText(name);
        },
        [this](std::string_view name)
            -> const GuiListModel*
        {
            return FindList(ResolveDataPath(name));
        },
        [this](std::string_view name)
        {
            return ResolveNumber(name);
        }
    };
}
