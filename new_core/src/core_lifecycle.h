#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace core
{

enum class GamePhase
{
    Unknown,
    Frontend,
    Gameplay
};

enum class LifecycleEventReason
{
    RuntimeStarted,
    Observation,
    SaveLoaded,
    RuntimeStopping
};

enum class LifecycleEventSource
{
    Core,
    NativeProbe,
    Lua,
    External
};

struct LifecycleSnapshot
{
    bool runtimeActive = false;
    GamePhase phase = GamePhase::Unknown;
    uint64_t generation = 0;
    uint64_t saveGeneration = 0;
    std::string playerTag;
    std::string saveKey;
};

struct LifecycleEvent
{
    LifecycleEventReason reason =
        LifecycleEventReason::Observation;
    LifecycleEventSource source =
        LifecycleEventSource::Core;
    LifecycleSnapshot previous;
    LifecycleSnapshot current;
    bool enteredGameplay = false;
    bool exitedGameplay = false;
    bool playerChanged = false;
};

class LifecycleService
{
public:
    bool Start();
    bool Observe(
        GamePhase phase,
        std::string_view playerTag,
        LifecycleEventSource source
    );
    bool NotifySaveLoaded(
        std::string_view saveKey,
        LifecycleEventSource source
    );
    bool Stop();

    LifecycleSnapshot Snapshot() const;
    std::vector<LifecycleEvent> DrainEvents();

private:
    void PushEventUnlocked(
        LifecycleEventReason reason,
        LifecycleEventSource source,
        const LifecycleSnapshot& previous
    );

    mutable std::mutex mutex_;
    LifecycleSnapshot snapshot_;
    std::deque<LifecycleEvent> events_;
};

}
