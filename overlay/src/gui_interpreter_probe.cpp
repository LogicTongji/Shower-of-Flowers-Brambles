#include "gui_interpreter.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
	const std::filesystem::path root = argc >= 2
		? std::filesystem::path(argv[1])
		: std::filesystem::current_path();

	gui::GuiInterpreter interpreter;
	std::string error;

	if (!interpreter.LoadDirectory(root / "interface", error))
	{
		std::cerr << error << '\n';
		return 1;
	}

	std::cout << "Loaded GUI documents: "
		<< interpreter.Documents().size() << '\n';
	std::cout << "Registered sprites: "
		<< interpreter.Sprites().size() << '\n';
	std::cout << "Registered progress bars: "
		<< interpreter.ProgressBars().size() << '\n';

	const gui::SpriteResource* sidebar = interpreter.FindSprite(
		"GFX_china_war_sidebar"
	);

	if (sidebar)
	{
		std::cout << "China war sidebar: "
			<< interpreter.ResolveTexture(
				"GFX_china_war_sidebar",
				root
			).string()
			<< '\n';
	}

	return 0;
}
