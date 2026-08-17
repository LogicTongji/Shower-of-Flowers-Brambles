#include "gui_hoi3_lifecycle.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <utility>

namespace
{

constexpr DWORD SupportedHoi3Timestamp = 0x50978B2F;
constexpr DWORD SupportedHoi3ImageSize = 0x018B0000;
constexpr std::uintptr_t GameStateSingletonRva = 0x01689790;
constexpr std::uintptr_t PlayerTagOffset = 0x00000C30;
constexpr std::size_t PlayerTagStorageSize = 4;

bool TryCopyMemory(
    const void* source,
    void* destination,
    std::size_t size
) noexcept
{
#if defined(_MSC_VER)
    __try
    {
        std::memcpy(destination, source, size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
#else
    std::memcpy(destination, source, size);
    return true;
#endif
}

bool IsSupportedExecutable(const uint8_t* base)
{
    IMAGE_DOS_HEADER dos{};
    if (!TryCopyMemory(base, &dos, sizeof(dos))
        || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew <= 0)
    {
        return false;
    }
    IMAGE_NT_HEADERS32 nt{};
    if (!TryCopyMemory(base + dos.e_lfanew, &nt, sizeof(nt)))
    {
        return false;
    }
    return nt.Signature == IMAGE_NT_SIGNATURE
        && nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC
        && nt.FileHeader.TimeDateStamp == SupportedHoi3Timestamp
        && nt.OptionalHeader.SizeOfImage == SupportedHoi3ImageSize;
}

bool IsTagCharacter(uint8_t character)
{
    return (character >= 'A' && character <= 'Z')
        || (character >= '0' && character <= '9')
        || character == '-';
}

}

bool DecodeGuiHoi3PlayerTag(
    const uint8_t* bytes,
    std::size_t size,
    std::string& playerTag
)
{
    playerTag.clear();
    if (!bytes
        || size < PlayerTagStorageSize
        || bytes[3] != 0
        || !IsTagCharacter(bytes[0])
        || !IsTagCharacter(bytes[1])
        || !IsTagCharacter(bytes[2]))
    {
        return false;
    }
    playerTag.assign(
        reinterpret_cast<const char*>(bytes),
        3
    );
    return true;
}

GuiHoi3LifecycleProbeResult ProbeGuiHoi3Lifecycle()
{
    const auto* base = reinterpret_cast<const uint8_t*>(
        GetModuleHandleW(nullptr)
    );
    if (!base || !IsSupportedExecutable(base))
    {
        return {
            GuiHoi3LifecycleProbeStatus::UnsupportedExecutable,
            {}
        };
    }

    std::uintptr_t gameState = 0;
    if (!TryCopyMemory(
            base + GameStateSingletonRva,
            &gameState,
            sizeof(gameState)
        ))
    {
        return {GuiHoi3LifecycleProbeStatus::Unavailable, {}};
    }
    if (gameState == 0)
    {
        return {GuiHoi3LifecycleProbeStatus::Frontend, "---"};
    }

    uint8_t tagBytes[PlayerTagStorageSize]{};
    if (!TryCopyMemory(
            reinterpret_cast<const void*>(gameState + PlayerTagOffset),
            tagBytes,
            sizeof(tagBytes)
        ))
    {
        return {GuiHoi3LifecycleProbeStatus::Unavailable, {}};
    }

    std::string playerTag;
    if (!DecodeGuiHoi3PlayerTag(
            tagBytes,
            sizeof(tagBytes),
            playerTag
        ))
    {
        return {GuiHoi3LifecycleProbeStatus::Unavailable, {}};
    }
    return {
        playerTag == "---"
            ? GuiHoi3LifecycleProbeStatus::Frontend
            : GuiHoi3LifecycleProbeStatus::Gameplay,
        std::move(playerTag)
    };
}
