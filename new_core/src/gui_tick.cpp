#include "gui_tick.h"

#include <algorithm>
#include <limits>

void GuiTickScheduler::SetInterval(
    uint32_t intervalMilliseconds
)
{
    intervalMilliseconds_ = intervalMilliseconds;
}

bool GuiTickScheduler::Register(
    std::string name,
    GuiTickCallback callback
)
{
    if (name.empty() || !callback)
    {
        return false;
    }

    const auto iterator = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&name](const Entry& entry)
        {
            return entry.name == name;
        }
    );
    if (iterator != entries_.end())
    {
        iterator->callback = std::move(callback);
        return true;
    }

    entries_.push_back({std::move(name), std::move(callback)});
    return true;
}

bool GuiTickScheduler::Unregister(
    std::string_view name
)
{
    const auto iterator = std::find_if(
        entries_.begin(),
        entries_.end(),
        [name](const Entry& entry)
        {
            return entry.name == name;
        }
    );
    if (iterator == entries_.end())
    {
        return false;
    }

    entries_.erase(iterator);
    return true;
}

void GuiTickScheduler::Clear()
{
    entries_.clear();
    refreshRequested_ = true;
}

void GuiTickScheduler::RequestRefresh()
{
    refreshRequested_ = true;
}

GuiTickResult GuiTickScheduler::Tick(
    uint64_t nowMilliseconds
)
{
    const bool intervalElapsed = !hasTicked_
        || nowMilliseconds < lastTickMilliseconds_
        || nowMilliseconds - lastTickMilliseconds_
            >= intervalMilliseconds_;
    if (!refreshRequested_ && !intervalElapsed)
    {
        return {};
    }

    const uint64_t delta = hasTicked_
        ? nowMilliseconds >= lastTickMilliseconds_
            ? nowMilliseconds - lastTickMilliseconds_
            : 0
        : 0;
    GuiTickContext context;
    context.frame = ++frame_;
    context.nowMilliseconds = nowMilliseconds;
    context.deltaMilliseconds = static_cast<uint32_t>(
        std::min<uint64_t>(
            delta,
            std::numeric_limits<uint32_t>::max()
        )
    );
    context.forced = refreshRequested_ && !intervalElapsed;

    GuiTickResult result;
    result.ran = true;
    for (const Entry& entry : entries_)
    {
        ++result.callbacks;
        result.changed = entry.callback(context) || result.changed;
    }

    lastTickMilliseconds_ = nowMilliseconds;
    hasTicked_ = true;
    refreshRequested_ = false;
    return result;
}

uint64_t GuiTickScheduler::Frame() const
{
    return frame_;
}
