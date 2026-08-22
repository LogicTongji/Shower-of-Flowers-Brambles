#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "gui_data_value.h"
#include "gui_interpreter.h"
#include "gui_list_model.h"

std::string GuiDataValueToText(const GuiDataValue& value);
double GuiDataValueToNumber(const GuiDataValue& value);
bool GuiDataValueToBool(const GuiDataValue& value);

class GuiDataRegistry
{
public:
    void Clear();

    void Set(
        std::string name,
        GuiDataValue value
    );

    void Set(
        std::string name,
        bool value
    );

    void Set(
        std::string name,
        int64_t value
    );

    void Set(
        std::string name,
        int value
    );

    void Set(
        std::string name,
        uint64_t value
    );

    void Set(
        std::string name,
        double value
    );

    void Set(
        std::string name,
        std::string value
    );

    void Set(
        std::string name,
        const char* value
    );

    void SetList(
        std::string name,
        GuiListModel value
    );

    bool Remove(std::string_view name);

    bool RemoveList(std::string_view name);

    const GuiDataValue* Find(
        std::string_view name
    ) const;

    const GuiListModel* FindList(
        std::string_view name
    ) const;

    std::string ResolveText(
        std::string_view name
    ) const;

    double ResolveNumber(
        std::string_view name
    ) const;

    bool ResolveBool(
        std::string_view name
    ) const;

    bool EvaluateCondition(
        std::string_view expression
    ) const;

    gui::GuiLayoutContext MakeLayoutContext() const;

private:
    std::string ResolveDataPath(
        std::string_view path,
        int depth = 0
    ) const;

    std::unordered_map<std::string, GuiDataValue> values_;
    std::unordered_map<std::string, GuiListModel> lists_;
};
