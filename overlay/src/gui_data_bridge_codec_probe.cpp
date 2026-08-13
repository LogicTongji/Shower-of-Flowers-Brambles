#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "gui_data_bridge_codec.h"

namespace
{

GuiDataBridgeUpdate MakeUpdate(bool reverseInsertion)
{
    GuiDataBridgeUpdate update;
    update.revision = 42;
    update.fullSnapshot = true;
    if (reverseInsertion)
    {
        update.values["title"] = std::string("bridge\nprotocol");
        update.values["ratio"] = 0.625;
        update.values["exact_integer"] = int64_t{9007199254740993LL};
        update.values["visible"] = true;
        update.values["empty"] = std::monostate{};
    }
    else
    {
        update.values["empty"] = std::monostate{};
        update.values["visible"] = true;
        update.values["exact_integer"] = int64_t{9007199254740993LL};
        update.values["ratio"] = 0.625;
        update.values["title"] = std::string("bridge\nprotocol");
    }
    update.removedValues = {"obsolete_b", "obsolete_a"};

    GuiListModel tasks;
    tasks.revision = 9;
    GuiListItem first{101, "first item"};
    GuiListItem second{102, "second item"};
    if (reverseInsertion)
    {
        second.fields["portrait"] = std::string("GFX_second");
        second.fields["regionid"] = int64_t{24};
    }
    else
    {
        second.fields["regionid"] = int64_t{24};
        second.fields["portrait"] = std::string("GFX_second");
    }
    tasks.items.push_back(std::move(first));
    tasks.items.push_back(std::move(second));
    update.lists["tasks"] = std::move(tasks);
    update.removedLists = {"old_tasks"};
    return update;
}

GuiActionContext MakeAction(bool reverseInsertion)
{
    GuiActionContext action;
    action.action = "activate_item";
    action.functionName = "ProbeGui.ActivateItem";
    action.fallbackOperation = "send_action";
    action.phase = "click";
    action.windowName = "probe_window";
    action.widgetName = "activate_button";
    action.listName = "probe_list";
    action.listIndex = -7;
    action.listItemId = 9007199254740993ULL;
    action.hasListItemId = true;
    action.mouseX = -320;
    action.mouseY = 480;
    if (reverseInsertion)
    {
        action.parameters["item"] = "second_item";
        action.parameters["mode"] = "fast";
    }
    else
    {
        action.parameters["mode"] = "fast";
        action.parameters["item"] = "second_item";
    }
    return action;
}

bool ContainsAllUpdateData(const GuiDataBridgeUpdate& update)
{
    const auto integer = update.values.find("exact_integer");
    const auto title = update.values.find("title");
    const auto ratio = update.values.find("ratio");
    const auto empty = update.values.find("empty");
    const auto tasks = update.lists.find("tasks");
    return update.revision == 42
        && update.baseRevision == 0
        && update.fullSnapshot
        && integer != update.values.end()
        && std::get<int64_t>(integer->second) == 9007199254740993LL
        && title != update.values.end()
        && std::get<std::string>(title->second)
            == "bridge\nprotocol"
        && ratio != update.values.end()
        && std::get<double>(ratio->second) == 0.625
        && empty != update.values.end()
        && std::holds_alternative<std::monostate>(empty->second)
        && update.removedValues
            == std::vector<std::string>({"obsolete_a", "obsolete_b"})
        && tasks != update.lists.end()
        && tasks->second.revision == 9
        && tasks->second.items.size() == 2
        && tasks->second.items[1].id == 102
        && tasks->second.items[1].text == "second item"
        && tasks->second.items[1].Find("regionid")
        && GuiDataValueToNumber(
            *tasks->second.items[1].Find("regionid")
        ) == 24.0
        && tasks->second.items[1].Find("portrait")
        && GuiDataValueToText(
            *tasks->second.items[1].Find("portrait")
        ) == "GFX_second"
        && update.removedLists
            == std::vector<std::string>({"old_tasks"});
}

bool ActionsEqual(
    const GuiActionContext& first,
    const GuiActionContext& second
)
{
    return first.action == second.action
        && first.functionName == second.functionName
        && first.fallbackOperation == second.fallbackOperation
        && first.phase == second.phase
        && first.windowName == second.windowName
        && first.widgetName == second.widgetName
        && first.listName == second.listName
        && first.listIndex == second.listIndex
        && first.listItemId == second.listItemId
        && first.hasListItemId == second.hasListItemId
        && first.mouseX == second.mouseX
        && first.mouseY == second.mouseY
        && first.parameters == second.parameters;
}

bool ExpectDecodeFailure(
    const std::vector<uint8_t>& frame,
    std::string_view expectedError
)
{
    GuiDataBridgeUpdate output;
    output.revision = 777;
    std::string error;
    return !DecodeGuiDataBridgeUpdate(frame, output, error)
        && output.revision == 777
        && error.find(expectedError) != std::string::npos;
}

bool AllSingleBitCorruptionsRejected(
    const std::vector<uint8_t>& frame
)
{
    for (std::size_t byteIndex = 0;
        byteIndex < frame.size();
        ++byteIndex)
    {
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            std::vector<uint8_t> corrupted = frame;
            corrupted[byteIndex] ^= static_cast<uint8_t>(1u << bit);
            GuiDataBridgeFrameInfo info;
            std::string error;
            if (InspectGuiDataBridgeFrame(corrupted, info, error))
            {
                return false;
            }
        }
    }
    return true;
}

}

int main()
{
    std::string error;
    std::vector<uint8_t> updateFrame;
    std::vector<uint8_t> reorderedUpdateFrame;
    if (!EncodeGuiDataBridgeUpdate(
            MakeUpdate(false),
            updateFrame,
            error
        )
        || !EncodeGuiDataBridgeUpdate(
            MakeUpdate(true),
            reorderedUpdateFrame,
            error
        )
        || updateFrame != reorderedUpdateFrame)
    {
        std::cerr << "Deterministic update encoding failed: "
                  << error << '\n';
        return 1;
    }

    GuiDataBridgeFrameInfo updateInfo;
    GuiDataBridgeUpdate decodedUpdate;
    if (!InspectGuiDataBridgeFrame(
            updateFrame,
            updateInfo,
            error
        )
        || updateInfo.version != kGuiDataBridgeProtocolVersion
        || updateInfo.type != GuiDataBridgeMessageType::DataUpdate
        || updateInfo.payloadBytes + kGuiDataBridgeFrameHeaderBytes
            != updateFrame.size()
        || !DecodeGuiDataBridgeUpdate(
            updateFrame,
            decodedUpdate,
            error
        )
        || !ContainsAllUpdateData(decodedUpdate))
    {
        std::cerr << "Update codec round trip failed: "
                  << error << '\n';
        return 1;
    }

    std::vector<uint8_t> actionFrame;
    std::vector<uint8_t> reorderedActionFrame;
    const GuiActionContext action = MakeAction(false);
    if (!EncodeGuiDataBridgeAction(action, actionFrame, error)
        || !EncodeGuiDataBridgeAction(
            MakeAction(true),
            reorderedActionFrame,
            error
        )
        || actionFrame != reorderedActionFrame)
    {
        std::cerr << "Deterministic action encoding failed: "
                  << error << '\n';
        return 1;
    }

    GuiDataBridgeFrameInfo actionInfo;
    GuiActionContext decodedAction;
    if (!InspectGuiDataBridgeFrame(
            actionFrame,
            actionInfo,
            error
        )
        || actionInfo.type != GuiDataBridgeMessageType::Action
        || !DecodeGuiDataBridgeAction(
            actionFrame,
            decodedAction,
            error
        )
        || !ActionsEqual(action, decodedAction)
        || DecodeGuiDataBridgeUpdate(
            actionFrame,
            decodedUpdate,
            error
        )
        || error != "bridge_codec_message_type_mismatch")
    {
        std::cerr << "Action codec round trip failed: "
                  << error << '\n';
        return 1;
    }

    std::vector<uint8_t> truncated = updateFrame;
    truncated.pop_back();
    std::vector<uint8_t> badMagic = updateFrame;
    badMagic[0] ^= 0xFF;
    std::vector<uint8_t> badVersion = updateFrame;
    badVersion[4] = 3;
    std::vector<uint8_t> badChecksum = updateFrame;
    badChecksum.back() ^= 0x01;
    std::vector<uint8_t> badLength = updateFrame;
    badLength[8] ^= 0x01;
    if (!ExpectDecodeFailure(truncated, "payload_size_mismatch")
        || !ExpectDecodeFailure(badMagic, "magic_invalid")
        || !ExpectDecodeFailure(badVersion, "version_unsupported")
        || !ExpectDecodeFailure(badChecksum, "checksum_mismatch")
        || !ExpectDecodeFailure(badLength, "payload_size_mismatch")
        || !AllSingleBitCorruptionsRejected(updateFrame))
    {
        std::cerr << "Corrupt frame validation failed\n";
        return 1;
    }

    GuiDataBridgeCodecLimits smallFrameLimits;
    smallFrameLimits.maxFrameBytes = updateFrame.size() - 1;
    GuiDataBridgeFrameInfo ignoredInfo;
    if (InspectGuiDataBridgeFrame(
            updateFrame,
            ignoredInfo,
            error,
            smallFrameLimits
        )
        || error != "bridge_codec_frame_too_large")
    {
        std::cerr << "Frame size limit validation failed\n";
        return 1;
    }

    GuiDataBridgeCodecLimits smallStringLimits;
    smallStringLimits.maxStringBytes = 4;
    if (EncodeGuiDataBridgeUpdate(
            MakeUpdate(false),
            reorderedUpdateFrame,
            error,
            smallStringLimits
        )
        || error != "bridge_codec_string_too_large")
    {
        std::cerr << "String size limit validation failed\n";
        return 1;
    }

    GuiDataBridgeUpdate limitedUpdate;
    GuiDataBridgeCodecLimits decodeValueLimits;
    decodeValueLimits.maxValues = 1;
    if (DecodeGuiDataBridgeUpdate(
            updateFrame,
            limitedUpdate,
            error,
            decodeValueLimits
        )
        || error != "bridge_codec_count_limit: values")
    {
        std::cerr << "Decode value limit validation failed\n";
        return 1;
    }
    if (DecodeGuiDataBridgeUpdate(
            updateFrame,
            limitedUpdate,
            error,
            smallStringLimits
        )
        || error != "bridge_codec_string_too_large")
    {
        std::cerr << "Decode string limit validation failed\n";
        return 1;
    }

    GuiActionContext limitedAction;
    GuiDataBridgeCodecLimits decodeActionLimits;
    decodeActionLimits.maxActionParameters = 1;
    if (DecodeGuiDataBridgeAction(
            actionFrame,
            limitedAction,
            error,
            decodeActionLimits
        )
        || error != "bridge_codec_count_limit: action_parameters")
    {
        std::cerr << "Decode action limit validation failed\n";
        return 1;
    }

    GuiDataBridgeUpdate invalidNumber;
    invalidNumber.revision = 1;
    invalidNumber.fullSnapshot = true;
    invalidNumber.values["nan"] =
        std::numeric_limits<double>::quiet_NaN();
    if (EncodeGuiDataBridgeUpdate(
            invalidNumber,
            reorderedUpdateFrame,
            error
        )
        || error != "bridge_codec_number_not_finite")
    {
        std::cerr << "Non-finite number validation failed\n";
        return 1;
    }

    std::cout
        << "Bridge wire version: " << updateInfo.version << '\n'
        << "Update frame bytes: " << updateFrame.size()
        << " checksum=" << updateInfo.checksum << '\n'
        << "Action frame bytes: " << actionFrame.size()
        << " checksum=" << actionInfo.checksum << '\n'
        << "Exact integer: "
        << std::get<int64_t>(
            decodedUpdate.values.at("exact_integer")
        ) << '\n';
    return 0;
}
