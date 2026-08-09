#include "gui_interpreter.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>

namespace gui
{

namespace
{

enum class TokenKind
{
	End,
	Identifier,
	String,
	Number,
	LeftBrace,
	RightBrace,
	Equals,
	Invalid
};

struct Token
{
	TokenKind kind = TokenKind::End;
	std::string text;
	int line = 1;
};

class Lexer
{
public:
	explicit Lexer(std::string source)
		: source_(std::move(source))
	{
	}

	Token Next()
	{
		SkipWhitespaceAndComments();

		if (position_ >= source_.size())
		{
			return {TokenKind::End, {}, line_};
		}

		const char character = source_[position_];

		if (character == '{')
		{
			++position_;
			return {TokenKind::LeftBrace, "{", line_};
		}

		if (character == '}')
		{
			++position_;
			return {TokenKind::RightBrace, "}", line_};
		}

		if (character == '=')
		{
			++position_;
			return {TokenKind::Equals, "=", line_};
		}

		if (character == '"')
		{
			return ReadString();
		}

		if (std::isdigit(static_cast<unsigned char>(character))
			|| character == '-'
			|| character == '+')
		{
			return ReadNumber();
		}

		if (IsIdentifierCharacter(character))
		{
			return ReadIdentifier();
		}

		++position_;
		return {TokenKind::Invalid, std::string(1, character), line_};
	}

private:
	static bool IsIdentifierCharacter(char character)
	{
		return std::isalnum(static_cast<unsigned char>(character))
			|| character == '_'
			|| character == '-'
			|| character == '.';
	}

	void SkipWhitespaceAndComments()
	{
		while (position_ < source_.size())
		{
			const char character = source_[position_];

			if (std::isspace(static_cast<unsigned char>(character)))
			{
				if (character == '\n')
				{
					++line_;
				}

				++position_;
				continue;
			}

			if (character == '#')
			{
				while (position_ < source_.size()
					&& source_[position_] != '\n')
				{
					++position_;
				}

				continue;
			}

			if (character == '/'
				&& position_ + 1 < source_.size()
				&& source_[position_ + 1] == '/')
			{
				position_ += 2;

				while (position_ < source_.size()
					&& source_[position_] != '\n')
				{
					++position_;
				}

				continue;
			}

			break;
		}
	}

	Token ReadString()
	{
		const int tokenLine = line_;
		++position_;
		std::string value;

		while (position_ < source_.size())
		{
			const char character = source_[position_++];

			if (character == '"')
			{
				return {TokenKind::String, value, tokenLine};
			}

			if (character == '\\'
				&& position_ < source_.size())
			{
				const char escaped = source_[position_++];
				value.push_back(escaped);
				continue;
			}

			if (character == '\n')
			{
				++line_;
			}

			value.push_back(character);
		}

		return {TokenKind::Invalid, value, tokenLine};
	}

	Token ReadNumber()
	{
		const int tokenLine = line_;
		const size_t start = position_;

		while (position_ < source_.size())
		{
			const char character = source_[position_];

			if (std::isdigit(static_cast<unsigned char>(character))
				|| character == '.'
				|| character == '-'
				|| character == '+')
			{
				++position_;
				continue;
			}

			break;
		}

		return {
			TokenKind::Number,
			source_.substr(start, position_ - start),
			tokenLine
		};
	}

	Token ReadIdentifier()
	{
		const int tokenLine = line_;
		const size_t start = position_;

		while (position_ < source_.size()
			&& IsIdentifierCharacter(source_[position_]))
		{
			++position_;
		}

		return {
			TokenKind::Identifier,
			source_.substr(start, position_ - start),
			tokenLine
		};
	}

	std::string source_;
	size_t position_ = 0;
	int line_ = 1;
};

class Parser
{
public:
	Parser(std::string source, std::string& error)
		: lexer_(std::move(source)), error_(error)
	{
	}

	bool Parse(GuiObject& output)
	{
		return ParseObject(output, false);
	}

private:
	bool ParseObject(GuiObject& output, bool hasOpeningBrace)
	{
		if (hasOpeningBrace)
		{
			const Token opening = lexer_.Next();
			if (opening.kind != TokenKind::LeftBrace)
			{
				SetError(opening, "expected '{'");
				return false;
			}
		}

		while (true)
		{
			const Token key = lexer_.Next();

			if (key.kind == TokenKind::End)
			{
				if (hasOpeningBrace)
				{
					SetError(key, "unexpected end of file");
					return false;
				}

				return true;
			}

			if (key.kind == TokenKind::RightBrace)
			{
				if (!hasOpeningBrace)
				{
					SetError(key, "unexpected '}'");
					return false;
				}

				return true;
			}

			if (key.kind != TokenKind::Identifier)
			{
				SetError(key, "expected property name");
				return false;
			}

			const Token equals = lexer_.Next();
			if (equals.kind != TokenKind::Equals)
			{
				SetError(equals, "expected '=' after property name");
				return false;
			}

			GuiValue value;
			if (!ParseValue(value))
			{
				return false;
			}

			output.fields.push_back({key.text, std::move(value)});
		}
	}

	bool ParseValue(GuiValue& output)
	{
		const Token value = lexer_.Next();

		if (value.kind == TokenKind::LeftBrace)
		{
			output.kind = ValueKind::Block;
			output.block = std::make_shared<GuiObject>();

			if (!ParseObjectAfterOpeningBrace(*output.block))
			{
				return false;
			}

			return true;
		}

		if (value.kind == TokenKind::Identifier
			|| value.kind == TokenKind::String
			|| value.kind == TokenKind::Number)
		{
			output.kind = ValueKind::Scalar;
			output.scalar = value.text;
			return true;
		}

		SetError(value, "expected scalar or block");
		return false;
	}

	bool ParseObjectAfterOpeningBrace(GuiObject& output)
	{
		while (true)
		{
			const Token key = lexer_.Next();

			if (key.kind == TokenKind::RightBrace)
			{
				return true;
			}

			if (key.kind == TokenKind::End)
			{
				SetError(key, "unexpected end of file inside block");
				return false;
			}

			if (key.kind == TokenKind::Number
				|| key.kind == TokenKind::String)
			{
				GuiValue value;
				value.kind = ValueKind::Scalar;
				value.scalar = key.text;
				output.fields.push_back({{}, std::move(value)});
				continue;
			}

			if (key.kind != TokenKind::Identifier)
			{
				SetError(key, "expected property name inside block");
				return false;
			}

            const Token equals = lexer_.Next();
            if (equals.kind != TokenKind::Equals)
            {
                SetError(equals, "expected '=' inside block");
                return false;
			}

			GuiValue value;
			if (!ParseValue(value))
			{
				return false;
			}

            output.fields.push_back({key.text, std::move(value)});
        }
    }

	void SetError(const Token& token, const std::string& message)
	{
		if (error_.empty())
		{
			error_ = "line "
				+ std::to_string(token.line)
				+ ": "
				+ message;
		}
	}

	Lexer lexer_;
	std::string& error_;
};

const GuiValue* FindValue(
	const GuiObject& object,
	const std::string& name
)
{
	for (const GuiField& field : object.fields)
	{
		if (field.name == name)
		{
			return &field.value;
		}
	}

	return nullptr;
}

std::string GetScalar(
	const GuiObject& object,
	const std::string& name
)
{
	const GuiValue* value = FindValue(object, name);

	if (!value || value->kind != ValueKind::Scalar)
	{
		return {};
	}

	return value->scalar;
}

int GetInteger(
	const GuiObject& object,
	const std::string& name,
	int fallback
)
{
	const std::string value = GetScalar(object, name);

	if (value.empty())
	{
		return fallback;
	}

	try
	{
		return std::stoi(value);
	}
	catch (...)
	{
		return fallback;
	}
}

bool GetBoolean(
	const GuiObject& object,
	const std::string& name,
	bool fallback
)
{
	std::string value = GetScalar(object, name);

	std::transform(
		value.begin(),
		value.end(),
		value.begin(),
		[](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		}
	);

	if (value == "yes" || value == "true" || value == "1")
	{
		return true;
	}

	if (value == "no" || value == "false" || value == "0")
	{
		return false;
	}

	return fallback;
}

void ReadColor(
	const GuiObject& object,
	const std::string& name,
	float output[3]
)
{
	const GuiValue* value = FindValue(object, name);

	if (!value || value->kind != ValueKind::Block || !value->block)
	{
		return;
	}

	const std::string red = GetScalar(*value->block, "r");
	const std::string green = GetScalar(*value->block, "g");
	const std::string blue = GetScalar(*value->block, "b");
	std::vector<std::string> positionalValues;

	for (const GuiField& field : value->block->fields)
	{
		if (field.name.empty()
			&& field.value.kind == ValueKind::Scalar)
		{
			positionalValues.push_back(field.value.scalar);
		}
	}

	try
	{
		if (!red.empty())
		{
			output[0] = std::stof(red);
		}
		else if (positionalValues.size() > 0)
		{
			output[0] = std::stof(positionalValues[0]);
		}

		if (!green.empty())
		{
			output[1] = std::stof(green);
		}
		else if (positionalValues.size() > 1)
		{
			output[1] = std::stof(positionalValues[1]);
		}

		if (!blue.empty())
		{
			output[2] = std::stof(blue);
		}
		else if (positionalValues.size() > 2)
		{
			output[2] = std::stof(positionalValues[2]);
		}
	}
	catch (...)
	{
		return;
	}
}

void ReadSize(
	const GuiObject& object,
	const std::string& name,
	int& width,
	int& height
)
{
	const GuiValue* value = FindValue(object, name);

	if (!value || value->kind != ValueKind::Block || !value->block)
	{
		return;
	}

	width = GetInteger(*value->block, "x", width);
	height = GetInteger(*value->block, "y", height);
}

void RegisterObjectResources(
	const GuiObject& object,
	std::unordered_map<std::string, SpriteResource>& sprites,
	std::unordered_map<std::string, ProgressBarResource>& progressBars
)
{
	for (const GuiField& field : object.fields)
	{
		if (field.value.kind == ValueKind::Block && field.value.block)
		{
			if (field.name == "spriteType")
			{
				SpriteResource sprite;
				sprite.name = GetScalar(*field.value.block, "name");
				sprite.textureFile = GetScalar(
					*field.value.block,
					"texturefile"
				);
				if (sprite.textureFile.empty())
				{
					sprite.textureFile = GetScalar(
						*field.value.block,
						"textureFile"
					);
				}
				sprite.effectFile = GetScalar(
					*field.value.block,
					"effectFile"
				);
				sprite.loadType = GetScalar(
					*field.value.block,
					"loadType"
				);
				sprite.frameCount = std::max(
					1,
					GetInteger(*field.value.block, "noOfFrames", 1)
				);
				sprite.noRefCount = GetBoolean(
					*field.value.block,
					"norefcount",
					false
				);

				if (!sprite.name.empty())
				{
					sprites[sprite.name] = std::move(sprite);
				}
			}
			else if (field.name == "progressbartype")
			{
				ProgressBarResource progressBar;
				progressBar.name = GetScalar(
					*field.value.block,
					"name"
				);
				progressBar.textureFile1 = GetScalar(
					*field.value.block,
					"textureFile1"
				);
				progressBar.textureFile2 = GetScalar(
					*field.value.block,
					"textureFile2"
				);
				progressBar.effectFile = GetScalar(
					*field.value.block,
					"effectFile"
				);
				ReadColor(
					*field.value.block,
					"color",
					progressBar.color
				);
				ReadColor(
					*field.value.block,
					"colortwo",
					progressBar.secondColor
				);
				ReadSize(
					*field.value.block,
					"size",
					progressBar.width,
					progressBar.height
				);
				progressBar.horizontal = GetBoolean(
					*field.value.block,
					"horizontal",
					true
				);

				if (!progressBar.name.empty())
				{
					progressBars[progressBar.name] =
						std::move(progressBar);
				}
			}

			RegisterObjectResources(
				*field.value.block,
				sprites,
				progressBars
			);
		}
	}
}

std::string ReadFile(
	const std::filesystem::path& path
)
{
	std::ifstream file(path, std::ios::binary);

	if (!file)
	{
		return {};
	}

	std::ostringstream content;
	content << file.rdbuf();
	return content.str();
}

}

bool GuiInterpreter::LoadDirectory(
	const std::filesystem::path& root,
	std::string& error
)
{
	if (!std::filesystem::is_directory(root))
	{
		error = "GUI interface directory not found: "
			+ root.string();
		return false;
	}

	bool loadedAny = false;
	std::string firstError;

	for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
	{
		if (!entry.is_regular_file())
		{
			continue;
		}

		const std::string extension = entry.path().extension().string();
		if (extension != ".gfx" && extension != ".gui")
		{
			continue;
		}

		std::string fileError;
		if (!LoadFile(entry.path(), fileError))
		{
			if (firstError.empty())
			{
				firstError = fileError;
			}
			continue;
		}

		loadedAny = true;
	}

	if (!firstError.empty())
	{
		error = firstError;
	}

	if (!loadedAny && error.empty())
	{
		error = "no .gfx or .gui files found in: "
			+ root.string();
	}

	return loadedAny;
}

bool GuiInterpreter::LoadFile(
    const std::filesystem::path& path,
    std::string& error
)
{
	const std::string source = ReadFile(path);
	if (source.empty())
	{
		error = "cannot read GUI file: " + path.string();
		return false;
	}

	GuiDocument document;
	document.path = path;

	Parser parser(source, error);
	if (!parser.Parse(document.root))
	{
		error = path.string() + ": " + error;
		return false;
	}

	RegisterResources(document.root);
    documents_.push_back(std::move(document));
    return true;
}

void GuiInterpreter::RegisterResources(const GuiObject& object)
{
    RegisterObjectResources(
        object,
        sprites_,
        progressBars_
    );
}

const SpriteResource* GuiInterpreter::FindSprite(
	const std::string& name
) const
{
	const auto iterator = sprites_.find(name);
	return iterator == sprites_.end() ? nullptr : &iterator->second;
}

const ProgressBarResource* GuiInterpreter::FindProgressBar(
	const std::string& name
) const
{
	const auto iterator = progressBars_.find(name);
	return iterator == progressBars_.end()
		? nullptr
		: &iterator->second;
}

std::filesystem::path GuiInterpreter::ResolveTexture(
	const std::string& resourceName,
	const std::filesystem::path& projectRoot
) const
{
	const SpriteResource* sprite = FindSprite(resourceName);
	if (!sprite || sprite->textureFile.empty())
	{
		return {};
	}

	std::string texture = sprite->textureFile.string();
	std::replace(texture.begin(), texture.end(), '\\', '/');

	constexpr std::string_view gfxPrefix = "gfx/";
	if (texture.rfind(gfxPrefix, 0) == 0)
	{
		return projectRoot / texture;
	}

	return projectRoot / "gfx" / texture;
}

}
