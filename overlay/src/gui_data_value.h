#pragma once

#include <cstdint>
#include <string>
#include <variant>

using GuiDataValue = std::variant<
    std::monostate,
    bool,
    int64_t,
    double,
    std::string
>;
