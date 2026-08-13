#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gui_data_bridge.h"

inline constexpr uint16_t kGuiDataBridgeProtocolVersion = 2;
inline constexpr std::size_t kGuiDataBridgeFrameHeaderBytes = 16;

enum class GuiDataBridgeMessageType : uint16_t
{
    DataUpdate = 1,
    Action = 2
};

struct GuiDataBridgeCodecLimits
{
    std::size_t maxFrameBytes = 4 * 1024 * 1024;
    std::size_t maxStringBytes = 64 * 1024;
    std::size_t maxValues = 65536;
    std::size_t maxLists = 4096;
    std::size_t maxListItems = 262144;
    std::size_t maxListItemFields = 1048576;
    std::size_t maxActionParameters = 4096;
};

struct GuiDataBridgeFrameInfo
{
    uint16_t version = 0;
    GuiDataBridgeMessageType type =
        GuiDataBridgeMessageType::DataUpdate;
    uint32_t payloadBytes = 0;
    uint32_t checksum = 0;
};

bool InspectGuiDataBridgeFrame(
    const std::vector<uint8_t>& frame,
    GuiDataBridgeFrameInfo& info,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits = {}
);

bool EncodeGuiDataBridgeUpdate(
    const GuiDataBridgeUpdate& update,
    std::vector<uint8_t>& frame,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits = {}
);

bool DecodeGuiDataBridgeUpdate(
    const std::vector<uint8_t>& frame,
    GuiDataBridgeUpdate& update,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits = {}
);

bool EncodeGuiDataBridgeAction(
    const GuiActionContext& context,
    std::vector<uint8_t>& frame,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits = {}
);

bool DecodeGuiDataBridgeAction(
    const std::vector<uint8_t>& frame,
    GuiActionContext& context,
    std::string& error,
    const GuiDataBridgeCodecLimits& limits = {}
);
