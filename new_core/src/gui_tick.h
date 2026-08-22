#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct GuiTickContext
{
    uint64_t frame = 0;
    uint64_t nowMilliseconds = 0;
    uint32_t deltaMilliseconds = 0;
    bool forced = false;
};

using GuiTickCallback = std::function<bool(
    const GuiTickContext&
)>;

struct GuiTickResult
{
    bool ran = false;
    bool changed = false;
    std::size_t callbacks = 0;
};

class GuiTickScheduler
{
public:
    void SetInterval(
        uint32_t intervalMilliseconds
    );

    bool Register(
        std::string name,
        GuiTickCallback callback
    );

    bool Unregister(
        std::string_view name
    );

    void Clear();

    void RequestRefresh();

    GuiTickResult Tick(
        uint64_t nowMilliseconds
    );

    uint64_t Frame() const;

private:
    struct Entry
    {
        std::string name;
        GuiTickCallback callback;
    };

    uint32_t intervalMilliseconds_ = 100;
    uint64_t frame_ = 0;
    uint64_t lastTickMilliseconds_ = 0;
    bool hasTicked_ = false;
    bool refreshRequested_ = true;
    std::vector<Entry> entries_;
};
