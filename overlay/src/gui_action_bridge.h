#pragma once

#include <cstddef>
#include <functional>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gui_behavior.h"
#include "gui_runtime.h"

struct GuiActionContext
{
    std::string action;
    std::string functionName;
    std::string fallbackOperation;
    std::string phase;
    std::string windowName;
    std::string widgetName;
    std::string listName;
    int listIndex = -1;
    uint64_t listItemId = 0;
    bool hasListItemId = false;
    int mouseX = 0;
    int mouseY = 0;
    std::unordered_map<std::string, std::string> parameters;
};

using GuiLuaActionInvoker = std::function<bool(
    std::string_view,
    const GuiActionContext&
)>;

using GuiFallbackActionInvoker = std::function<bool(
    const GuiActionContext&
)>;

using GuiActionConditionEvaluator = std::function<bool(
    std::string_view
)>;

using GuiListItemIdResolver = std::function<bool(
    std::string_view,
    int,
    uint64_t&
)>;

class GuiLuaActionBridge
{
public:
    void SetInvoker(
        GuiLuaActionInvoker invoker
    );

    void SetFallbackInvoker(
        GuiFallbackActionInvoker invoker
    );

    void SetBehaviorRegistry(
        const GuiBehaviorRegistry* registry
    );

    void SetConditionEvaluator(
        GuiActionConditionEvaluator evaluator
    );

    void SetListItemIdResolver(
        GuiListItemIdResolver resolver
    );

    bool Dispatch(
        std::string_view windowName,
        const GuiActionEvent& event,
        int mouseX = 0,
        int mouseY = 0
    ) const;

    std::size_t DispatchEvents(
        std::string_view windowName,
        const std::vector<GuiActionEvent>& events,
        int mouseX = 0,
        int mouseY = 0
    ) const;

private:
    GuiLuaActionInvoker invoker_;
    GuiFallbackActionInvoker fallbackInvoker_;
    GuiActionConditionEvaluator conditionEvaluator_;
    GuiListItemIdResolver listItemIdResolver_;
    const GuiBehaviorRegistry* behaviorRegistry_ = nullptr;
};
