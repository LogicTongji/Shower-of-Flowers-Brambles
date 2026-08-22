#include "gui_localization.h"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    const std::filesystem::path root = argc > 1
        ? std::filesystem::path(argv[1])
        : std::filesystem::current_path();
    GuiLocalizationRegistry registry;
    std::string error;
    if (!registry.LoadFile(
            root / "localisation" / "warmap_interface.csv",
            error
        ))
    {
        std::cerr << error << '\n';
        return 1;
    }
    const std::string name = registry.Resolve(
        "WARMAP_LEADER_LI_ZONGREN"
    );
    const std::string description = registry.Resolve(
        "WARMAP_LEADER_LI_ZONGREN_DESC"
    );
    if (name != "李宗仁"
        || description.empty()
        || description == "WARMAP_LEADER_LI_ZONGREN_DESC")
    {
        std::cerr << "localization probe mismatch\n";
        return 1;
    }
    std::cout << "Localization entries: " << registry.Size() << '\n';
    std::cout << "Leader: " << name << '\n';
    return 0;
}
