#include "core_lifecycle.h"

#include <algorithm>
#include <cctype>

namespace core
{
namespace
{

std::string NormalizePlayerTag(
    GamePhase phase,
    std::string_view value
)
{
    if (phase == GamePhase::Frontend)
    {
        return "---";
    }
    if (phase != GamePhase::Gameplay)
    {
        return {};
    }
    std::string result(value);
    result.erase(
        std::remove_if(
            result.begin(),
            result.end(),
            [](unsigned char character)
            {
                return std::isspace(character) != 0;
            }
        ),
        result.end()
    );
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::toupper(character));
        }
    );
    return result;
}

}

bool LifecycleService::Start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.runtimeActive)
    {
        return false;
    }
    const LifecycleSnapshot previous = snapshot_;
    snapshot_.runtimeActive = true;
    ++snapshot_.generation;
    PushEventUnlocked(
        LifecycleEventReason::RuntimeStarted,
        LifecycleEventSource::Core,
        previous
    );
    return true;
}

bool LifecycleService::Observe(
    GamePhase phase,
    std::string_view playerTag,
    LifecycleEventSource source
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string normalizedTag = NormalizePlayerTag(
        phase,
        playerTag
    );
    if (snapshot_.phase == phase
        && snapshot_.playerTag == normalizedTag)
    {
        return false;
    }
    const LifecycleSnapshot previous = snapshot_;
    snapshot_.phase = phase;
    snapshot_.playerTag = normalizedTag;
    if (phase != GamePhase::Gameplay)
    {
        snapshot_.saveKey.clear();
    }
    ++snapshot_.generation;
    PushEventUnlocked(
        LifecycleEventReason::Observation,
        source,
        previous
    );
    return true;
}

bool LifecycleService::NotifySaveLoaded(
    std::string_view saveKey,
    LifecycleEventSource source
)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!snapshot_.runtimeActive
        || snapshot_.phase != GamePhase::Gameplay)
    {
        return false;
    }
    const LifecycleSnapshot previous = snapshot_;
    snapshot_.saveKey.assign(saveKey);
    ++snapshot_.saveGeneration;
    ++snapshot_.generation;
    PushEventUnlocked(
        LifecycleEventReason::SaveLoaded,
        source,
        previous
    );
    return true;
}

bool LifecycleService::Stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!snapshot_.runtimeActive)
    {
        return false;
    }
    const LifecycleSnapshot previous = snapshot_;
    snapshot_.runtimeActive = false;
    snapshot_.phase = GamePhase::Unknown;
    snapshot_.playerTag.clear();
    snapshot_.saveKey.clear();
    ++snapshot_.generation;
    PushEventUnlocked(
        LifecycleEventReason::RuntimeStopping,
        LifecycleEventSource::Core,
        previous
    );
    return true;
}

LifecycleSnapshot LifecycleService::Snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

std::vector<LifecycleEvent> LifecycleService::DrainEvents()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LifecycleEvent> output;
    output.reserve(events_.size());
    while (!events_.empty())
    {
        output.push_back(std::move(events_.front()));
        events_.pop_front();
    }
    return output;
}

void LifecycleService::PushEventUnlocked(
    LifecycleEventReason reason,
    LifecycleEventSource source,
    const LifecycleSnapshot& previous
)
{
    LifecycleEvent event;
    event.reason = reason;
    event.source = source;
    event.previous = previous;
    event.current = snapshot_;
    event.enteredGameplay =
        previous.phase != GamePhase::Gameplay
        && snapshot_.phase == GamePhase::Gameplay;
    event.exitedGameplay =
        previous.phase == GamePhase::Gameplay
        && snapshot_.phase != GamePhase::Gameplay;
    event.playerChanged =
        previous.playerTag != snapshot_.playerTag;
    events_.push_back(std::move(event));
}

}
