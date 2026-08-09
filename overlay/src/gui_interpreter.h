#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gui
{

enum class ValueKind
{
	Scalar,
	Block
};

struct GuiObject;

struct GuiValue
{
	ValueKind kind = ValueKind::Scalar;
	std::string scalar;
	std::shared_ptr<GuiObject> block;
};

struct GuiField
{
	std::string name;
	GuiValue value;
};

struct GuiObject
{
	std::vector<GuiField> fields;
};

struct GuiDocument
{
	std::filesystem::path path;
	GuiObject root;
};

struct SpriteResource
{
	std::string name;
	std::filesystem::path textureFile;
	std::string effectFile;
	std::string loadType;
	int frameCount = 1;
	bool noRefCount = false;
};

struct ProgressBarResource
{
	std::string name;
	std::string textureFile1;
	std::string textureFile2;
	std::string effectFile;
	float color[3] = {1.0f, 1.0f, 1.0f};
	float secondColor[3] = {1.0f, 1.0f, 1.0f};
	int width = 0;
	int height = 0;
	bool horizontal = true;
};

class GuiInterpreter
{
public:
	bool LoadDirectory(
		const std::filesystem::path& root,
		std::string& error
	);

	bool LoadFile(
		const std::filesystem::path& path,
		std::string& error
	);

	const SpriteResource* FindSprite(
		const std::string& name
	) const;

	const ProgressBarResource* FindProgressBar(
		const std::string& name
	) const;

	std::filesystem::path ResolveTexture(
		const std::string& resourceName,
		const std::filesystem::path& projectRoot
	) const;

	const std::vector<GuiDocument>& Documents() const
	{
		return documents_;
	}

	const std::unordered_map<std::string, SpriteResource>& Sprites() const
	{
		return sprites_;
	}

	const std::unordered_map<std::string, ProgressBarResource>& ProgressBars() const
	{
		return progressBars_;
	}

private:
	void RegisterResources(const GuiObject& object);

	std::vector<GuiDocument> documents_;
	std::unordered_map<std::string, SpriteResource> sprites_;
	std::unordered_map<std::string, ProgressBarResource> progressBars_;
};

}
