#include "gui_interpreter.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <cctype>
#include <fstream>
#include <functional>
#include <initializer_list>
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
	const auto equalsIgnoreCase = [](
		const std::string& first,
		const std::string& second
	)
	{
		if (first.size() != second.size())
		{
			return false;
		}

		for (size_t index = 0; index < first.size(); ++index)
		{
			if (std::tolower(static_cast<unsigned char>(first[index]))
				!= std::tolower(static_cast<unsigned char>(second[index])))
			{
				return false;
			}
		}

		return true;
	};

	for (const GuiField& field : object.fields)
	{
		if (field.name == name)
		{
			return &field.value;
		}
	}

	for (const GuiField& field : object.fields)
	{
		if (equalsIgnoreCase(field.name, name))
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

std::string GetFirstScalar(
	const GuiObject& object,
	std::initializer_list<const char*> names
)
{
	for (const char* name : names)
	{
		const std::string value = GetScalar(object, name);
		if (!value.empty())
		{
			return value;
		}
	}

	return {};
}

std::string ToLower(std::string value)
{
	std::transform(
		value.begin(),
		value.end(),
		value.begin(),
		[](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		}
	);

	return value;
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

float GetFloat(
	const GuiObject& object,
	const std::string& name,
	float fallback
)
{
	const std::string value = GetScalar(object, name);
	if (value.empty())
	{
		return fallback;
	}

	try
	{
		return std::stof(value);
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

void ReadRgbaColor(
	const GuiObject& object,
	const std::string& name,
	GuiRgbaColor& output
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
	const std::string alpha = GetScalar(*value->block, "a");
	std::vector<std::string> positionalValues;

	for (const GuiField& field : value->block->fields)
	{
		if (field.name.empty()
			&& field.value.kind == ValueKind::Scalar)
		{
			positionalValues.push_back(field.value.scalar);
		}
	}

	auto readComponent = [&positionalValues](
		const std::string& namedValue,
		size_t index,
		float fallback
	)
	{
		const std::string& source = !namedValue.empty()
			? namedValue
			: index < positionalValues.size()
				? positionalValues[index]
				: std::string{};
		if (source.empty())
		{
			return fallback;
		}
		try
		{
			return std::stof(source);
		}
		catch (...)
		{
			return fallback;
		}
	};

	output.r = readComponent(red, 0, output.r);
	output.g = readComponent(green, 1, output.g);
	output.b = readComponent(blue, 2, output.b);
	output.a = readComponent(alpha, 3, output.a);
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

std::vector<int> ReadIntegerList(
	const GuiObject& object,
	const std::string& name
)
{
	std::vector<int> output;
	const GuiValue* value = FindValue(object, name);
	if (!value || value->kind != ValueKind::Block || !value->block)
	{
		return output;
	}

	for (const GuiField& field : value->block->fields)
	{
		if (field.value.kind != ValueKind::Scalar)
		{
			continue;
		}
		try
		{
			const int number = std::stoi(field.value.scalar);
			if (number > 0)
			{
				output.push_back(number);
			}
		}
		catch (...)
		{
		}
	}
	return output;
}

void ReadRect(
	const GuiObject& object,
	GuiRect& rect
)
{
	ReadSize(object, "position", rect.x, rect.y);
	ReadSize(object, "size", rect.width, rect.height);
}

WidgetType GetWidgetType(const std::string& name)
{
	const std::string type = ToLower(name);

	if (type == "windowtype") return WidgetType::Window;
	if (type == "icontype") return WidgetType::Image;
	if (type == "textboxtype"
		|| type == "instanttextboxtype")
	{
		return WidgetType::Text;
	}
	if (type == "guibuttontype") return WidgetType::Button;
	if (type == "listboxtype") return WidgetType::ListBox;
	if (type == "progressbartype") return WidgetType::ProgressBar;
	if (type == "scrollbartype") return WidgetType::ScrollBar;
	if (type == "colorboxtype") return WidgetType::ColorBox;
	if (type == "indexedmaptype") return WidgetType::IndexedMap;
	if (type == "markerlayertype") return WidgetType::MarkerLayer;
	if (type == "customwidgettype") return WidgetType::Custom;

	return WidgetType::Unknown;
}

WidgetDefinition BuildWidgetDefinition(
	const std::string& typeName,
	const GuiObject& object
)
{
	WidgetDefinition widget;
	widget.type = GetWidgetType(typeName);
	widget.name = GetScalar(object, "name");
	widget.parent = GetScalar(object, "parent");
	widget.font = GetScalar(object, "font");
	widget.text = GetScalar(object, "text");
	widget.textSource = GetFirstScalar(
		object,
		{"textSource", "textBinding", "textValue"}
	);
	widget.localizationKey = GetFirstScalar(
		object,
		{"localizationKey", "localisationKey", "textKey"}
	);
	widget.orientation = GetFirstScalar(
		object,
		{"orientation", "Orientation"}
	);
	widget.layoutMode = GetFirstScalar(
		object,
		{"layout", "layoutMode", "itemLayout"}
	);
	widget.positionType = GetFirstScalar(
		object,
		{"positionType", "position_type"}
	);
	widget.customType = GetScalar(object, "customType");

	if (widget.customType.empty())
	{
		widget.customType = GetScalar(object, "type");
	}

	widget.spriteName = GetScalar(object, "spriteType");
	widget.spriteSource = GetFirstScalar(
		object,
		{"spriteSource", "spriteBinding", "textureSource"}
	);
	widget.spriteValuePrefix = GetFirstScalar(
		object,
		{"spriteValuePrefix", "spritePrefix"}
	);
	widget.frameSpriteName = GetFirstScalar(
		object,
		{"borderSprite", "frameSprite", "windowFrame"}
	);

	if (widget.spriteName.empty())
	{
		widget.spriteName = GetScalar(object, "quadTextureSprite");
	}

	widget.pressedSpriteName = GetScalar(
		object,
		"pressedTextureSprite"
	);
	widget.pressedSpriteSource = GetFirstScalar(
		object,
		{"pressedSpriteSource", "pressedTextureSource"}
	);

	if (widget.pressedSpriteName.empty())
	{
		widget.pressedSpriteName = GetScalar(
			object,
			"pressedQuadTextureSprite"
		);
	}

	widget.templateName = GetScalar(object, "itemTemplate");
	widget.disabledByListName = GetFirstScalar(
		object,
		{"disableItemsInList", "disabledByList"}
	);
	widget.disabledMatchField = GetFirstScalar(
		object,
		{"disableMatchingField", "disabledMatchField"}
	);
	widget.disabledFilterField = GetFirstScalar(
		object,
		{"disableFilterField", "disabledFilterField"}
	);
	widget.disabledFilterValueSource = GetFirstScalar(
		object,
		{
			"disableFilterValueSource",
			"disabledFilterValueSource"
		}
	);
	widget.scrollBarName = GetScalar(object, "scrollbartype");
	if (widget.scrollBarName.empty())
	{
		widget.scrollBarName = GetScalar(object, "scrollbarType");
	}
	widget.sliderName = GetScalar(object, "slider");
	widget.trackName = GetScalar(object, "track");
	widget.progressResourceName = GetFirstScalar(
		object,
		{"progressBar", "progressbar", "progressResource", "progressType"}
	);
	widget.indexedMapResourceName = GetFirstScalar(
		object,
		{"mapResource", "indexedMap", "indexedMapResource", "resource"}
	);
	widget.dataSource = GetFirstScalar(
		object,
		{"dataSource", "listSource", "itemsSource"}
	);
	widget.mapWidgetName = GetFirstScalar(
		object,
		{"mapWidget", "targetMap", "indexedMapWidget"}
	);
	widget.portraitSource = GetFirstScalar(
		object,
		{"portraitSource", "imageSource", "itemSpriteSource"}
	);
	widget.regionSource = GetFirstScalar(
		object,
		{"regionSource", "regionIdSource", "anchorItemSource"}
	);
	widget.markerXSource = GetFirstScalar(
		object,
		{"xSource", "markerXSource"}
	);
	widget.markerYSource = GetFirstScalar(
		object,
		{"ySource", "markerYSource"}
	);
	widget.descriptionSource = GetFirstScalar(
		object,
		{"descriptionSource", "tooltipSource"}
	);
	widget.nameSource = GetFirstScalar(
		object,
		{"nameSource", "titleSource"}
	);
	widget.progressColorIndex = GetInteger(
		object,
		"progressColor",
		GetInteger(object, "colorIndex", 0)
	);
	widget.scaleMode = GetFirstScalar(
		object,
		{"scaleMode", "scale", "fit"}
	);
	widget.alignment = GetFirstScalar(
		object,
		{"alignment", "textAlignment", "align"}
	);
	widget.renderMode = GetFirstScalar(
		object,
		{"renderMode", "drawMode"}
	);
	widget.valueSource = GetFirstScalar(
		object,
		{"valueSource", "valueBinding", "progressSource"}
	);
	widget.tooltip = GetFirstScalar(
		object,
		{"tooltipText", "tooltip", "delayedTooltipText"}
	);
	widget.tooltipPlacement = GetFirstScalar(
		object,
		{"tooltipPlacement", "tooltipSide"}
	);
	widget.markerActionSpriteName = GetFirstScalar(
		object,
		{"markerActionSprite", "selectedActionSprite"}
	);
	widget.markerActionName = GetFirstScalar(
		object,
		{"onMarkerAction", "markerAction", "selectedAction"}
	);
	widget.markerActionLocalizationKey = GetFirstScalar(
		object,
		{"markerActionLocalizationKey", "markerActionTextKey"}
	);
	widget.markerStackSource = GetFirstScalar(
		object,
		{"stackSource", "markerStackSource", "stackGroupSource"}
	);
	widget.markerStackOrderSource = GetFirstScalar(
		object,
		{"stackOrderSource", "markerStackOrderSource"}
	);
	widget.markerStackDirection = GetFirstScalar(
		object,
		{"stackDirection", "markerStackDirection"}
	);
	widget.dragAxis = GetFirstScalar(
		object,
		{"dragAxis", "dragOrientation"}
	);
	widget.dragTrackName = GetFirstScalar(
		object,
		{"dragTrack", "dragBounds", "trackWidget"}
	);
	widget.dragValueSource = GetFirstScalar(
		object,
		{"dragValueSource", "dragBinding"}
	);
	widget.visibleWhen = GetFirstScalar(
		object,
		{"visibleWhen", "visible_if", "showIf", "condition"}
	);
	widget.enabledWhen = GetFirstScalar(
		object,
		{"enabledWhen", "enabled_if"}
	);
	widget.actions.onClick = GetFirstScalar(
		object,
		{"onClick", "onclick", "clickAction", "action", "callback"}
	);
	widget.actions.onPress = GetFirstScalar(
		object,
		{"onPress", "onpress", "pressAction"}
	);
	widget.actions.onRelease = GetFirstScalar(
		object,
		{"onRelease", "onrelease", "releaseAction"}
	);
	widget.actions.onHoverEnter = GetFirstScalar(
		object,
		{
			"onHoverEnter",
			"onhoverenter",
			"onHover",
			"onhover",
			"onMouseEnter",
			"onmouseenter",
			"hoverEnterAction"
		}
	);
	widget.actions.onHoverLeave = GetFirstScalar(
		object,
		{
			"onHoverLeave",
			"onhoverleave",
			"onMouseLeave",
			"onmouseleave",
			"hoverLeaveAction"
		}
	);
	widget.actions.onDragStart = GetFirstScalar(
		object,
		{"onDragStart", "ondragstart"}
	);
	widget.actions.onDrag = GetFirstScalar(
		object,
		{"onDrag", "ondrag"}
	);
	widget.actions.onDragEnd = GetFirstScalar(
		object,
		{"onDragEnd", "ondragend"}
	);
	widget.spacing = GetInteger(object, "spacing", 0);
	widget.columnSpacing = GetInteger(
		object,
		"columnSpacing",
		0
	);
	widget.polarCenterX = GetInteger(object, "polarCenterX", -1);
	widget.polarCenterY = GetInteger(object, "polarCenterY", -1);
	ReadSize(
		object,
		"polarCenter",
		widget.polarCenterX,
		widget.polarCenterY
	);
	widget.polarRingCount = std::max(
		0,
		GetInteger(object, "polarRingCount", 0)
	);
	widget.polarInnerRadius = std::max(
		0,
		GetInteger(object, "polarInnerRadius", 0)
	);
	widget.polarOuterRadius = std::max(
		0,
		GetInteger(object, "polarOuterRadius", 0)
	);
	widget.polarRingSpacing = std::max(
		0,
		GetInteger(object, "polarRingSpacing", 0)
	);
	widget.polarRingItemCounts = ReadIntegerList(
		object,
		"polarRingItemCounts"
	);
	widget.fontSize = GetInteger(
		object,
		"fontSize",
		GetInteger(object, "textSize", 0)
	);
	widget.lineSpacing = GetInteger(object, "lineSpacing", 0);
	widget.markerActionFontSize = GetInteger(
		object,
		"markerActionFontSize",
		0
	);
	widget.markerStackSpacing = std::max(
		0,
		GetInteger(
			object,
			"stackSpacing",
			GetInteger(object, "markerStackSpacing", 0)
		)
	);
	widget.dragSteps = std::max(
		0,
		GetInteger(object, "dragSteps", 0)
	);
	widget.polarStartAngle = GetFloat(
		object,
		"polarStartAngle",
		180.0f
	);
	widget.polarEndAngle = GetFloat(
		object,
		"polarEndAngle",
		360.0f
	);
	widget.dragMinimum = GetFloat(object, "dragMinimum", 0.0f);
	widget.dragMaximum = GetFloat(object, "dragMaximum", 1.0f);
	widget.dragStep = std::max(
		0.0f,
		GetFloat(object, "dragStep", 0.0f)
	);
	ReadColor(object, "color", widget.textColor);
	ReadRgbaColor(object, "lineColor", widget.lineColor);
	ReadRgbaColor(object, "tooltipColor", widget.tooltipColor);

	ReadRect(object, widget.rect);
	ReadSize(
		object,
		"markerSize",
		widget.markerRect.width,
		widget.markerRect.height
	);
	ReadSize(
		object,
		"portraitPosition",
		widget.portraitRect.x,
		widget.portraitRect.y
	);
	ReadSize(
		object,
		"portraitSize",
		widget.portraitRect.width,
		widget.portraitRect.height
	);
	ReadSize(
		object,
		"tooltipSize",
		widget.tooltipRect.width,
		widget.tooltipRect.height
	);
	ReadSize(
		object,
		"markerActionPosition",
		widget.markerActionRect.x,
		widget.markerActionRect.y
	);
	ReadSize(
		object,
		"markerActionSize",
		widget.markerActionRect.width,
		widget.markerActionRect.height
	);
	widget.zOrder = GetInteger(
		object,
		"zOrder",
		GetInteger(
			object,
			"z",
			GetInteger(object, "layer", 0)
		)
	);
	widget.frameZOrder = GetInteger(
		object,
		"frameZOrder",
		1000000
	);
	widget.value = GetFloat(object, "value", 0.0f);
	widget.fillFromEnd = GetBoolean(
		object,
		"fillFromEnd",
		GetBoolean(object, "reverse", false)
	);
	widget.drawBackground = GetBoolean(
		object,
		"drawBackground",
		true
	);
	widget.moveable = GetBoolean(object, "moveable", false);
	widget.draggable = GetBoolean(object, "draggable", false);
	widget.dragInverted = GetBoolean(object, "dragInverted", false);
	widget.localizeTooltip = GetBoolean(
		object,
		"localizeTooltip",
		GetBoolean(object, "localiseTooltip", false)
	);
	widget.avoidTooltipOverlap = GetBoolean(
		object,
		"avoidTooltipOverlap",
		GetBoolean(object, "tooltipAvoidMarkers", false)
	);
	widget.lineWidth = std::max(
		1,
		GetInteger(object, "lineWidth", 1)
	);
	widget.tooltipPadding = std::max(
		0,
		GetInteger(object, "tooltipPadding", 10)
	);
	widget.localized = GetBoolean(
		object,
		"localized",
		GetBoolean(object, "localised", false)
	);
	widget.wrap = GetBoolean(
		object,
		"wrap",
		GetBoolean(object, "wordWrap", false)
	);
	widget.dragHeight = GetInteger(object, "dragHeight", 0);
	widget.visible = GetBoolean(
		object,
		"visible",
		!GetBoolean(object, "dontRender", false)
	);
	widget.enabled = !GetBoolean(
		object,
		"disabled",
		false
	) && GetBoolean(object, "enabled", true);

	for (const GuiField& field : object.fields)
	{
		if (field.value.kind != ValueKind::Block
			|| !field.value.block)
		{
			continue;
		}

		const WidgetType childType = GetWidgetType(field.name);
		if (childType == WidgetType::Unknown)
		{
			continue;
		}

		widget.children.push_back(
			BuildWidgetDefinition(
				field.name,
				*field.value.block
			)
		);
	}

	return widget;
}

void CollectWindows(
	const GuiObject& object,
	std::vector<WindowDefinition>& output
)
{
	for (const GuiField& field : object.fields)
	{
		if (field.value.kind != ValueKind::Block
			|| !field.value.block)
		{
			continue;
		}

		if (GetWidgetType(field.name) == WidgetType::Window)
		{
			WidgetDefinition base = BuildWidgetDefinition(
				field.name,
				*field.value.block
			);
			WindowDefinition window;
			static_cast<WidgetDefinition&>(window) =
				std::move(base);
			output.push_back(std::move(window));
		}

		CollectWindows(*field.value.block, output);
	}
}

void RegisterObjectResources(
	const GuiObject& object,
	std::unordered_map<std::string, SpriteResource>& sprites,
	std::unordered_map<std::string, ProgressBarResource>& progressBars,
	std::unordered_map<std::string, IndexedMapResource>& indexedMaps
)
{
	for (const GuiField& field : object.fields)
	{
		if (field.value.kind == ValueKind::Block && field.value.block)
		{
			const std::string fieldType = ToLower(field.name);
			if (fieldType == "spritetype")
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
			else if (fieldType == "progressbartype")
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
			else if (fieldType == "indexedmapresourcetype")
			{
				IndexedMapResource indexedMap;
				indexedMap.name = GetScalar(
					*field.value.block,
					"name"
				);
				indexedMap.textureFile = GetFirstScalar(
					*field.value.block,
					{"texturefile", "textureFile"}
				);
				indexedMap.indexFile = GetFirstScalar(
					*field.value.block,
					{"indexfile", "indexFile"}
				);
				indexedMap.sourceDefinitionFile = GetFirstScalar(
					*field.value.block,
					{"sourceDefinitionFile", "definitionFile"}
				);
				indexedMap.sourceProvinceFile = GetFirstScalar(
					*field.value.block,
					{"sourceProvinceFile", "provinceFile"}
				);
				indexedMap.sourceGroupFile = GetFirstScalar(
					*field.value.block,
					{"sourceGroupFile", "groupFile"}
				);
				ReadRgbaColor(
					*field.value.block,
					"sourceFillColor",
					indexedMap.sourceFillColor
				);
				ReadRgbaColor(
					*field.value.block,
					"sourceBoundaryColor",
					indexedMap.sourceBoundaryColor
				);
				ReadRgbaColor(
					*field.value.block,
					"boundaryColor",
					indexedMap.boundaryColor
				);
				ReadRgbaColor(
					*field.value.block,
					"hoverColor",
					indexedMap.hoverColor
				);
				indexedMap.boundaryWidth = std::max(
					0,
					GetInteger(
						*field.value.block,
						"boundaryWidth",
						1
					)
				);
				indexedMap.cropPadding = std::max(
					0,
					GetInteger(
						*field.value.block,
						"cropPadding",
						16
					)
				);
				indexedMap.flipVertical = GetBoolean(
					*field.value.block,
					"flipVertical",
					true
				);
				indexedMap.drawBoundaries = GetBoolean(
					*field.value.block,
					"drawBoundaries",
					true
				);

				for (const GuiField& resourceField
					: field.value.block->fields)
				{
					const std::string resourceFieldType = ToLower(
						resourceField.name
					);
					if (resourceFieldType == "sourceitem"
						&& resourceField.value.kind == ValueKind::Block
						&& resourceField.value.block)
					{
						IndexedMapSourceItem item;
						item.id = static_cast<uint16_t>(std::clamp(
							GetInteger(
								*resourceField.value.block,
								"id",
								0
							),
							0,
							static_cast<int>(UINT16_MAX)
						));
						item.name = GetScalar(
							*resourceField.value.block,
							"name"
						);
						if (item.id != 0 && !item.name.empty())
						{
							indexedMap.sourceItems.push_back(
								std::move(item)
							);
						}
						continue;
					}

					if (resourceFieldType != "colorstop"
						|| resourceField.value.kind != ValueKind::Block
						|| !resourceField.value.block)
					{
						continue;
					}

					IndexedMapColorStop stop;
					stop.minimum = GetFloat(
						*resourceField.value.block,
						"minimum",
						GetFloat(
							*resourceField.value.block,
							"threshold",
							0.0f
						)
					);
					ReadRgbaColor(
						*resourceField.value.block,
						"color",
						stop.color
					);
					indexedMap.colorStops.push_back(stop);
				}

				std::stable_sort(
					indexedMap.colorStops.begin(),
					indexedMap.colorStops.end(),
					[](const IndexedMapColorStop& first,
					   const IndexedMapColorStop& second)
					{
						return first.minimum < second.minimum;
					}
				);

				if (!indexedMap.name.empty())
				{
					indexedMaps[indexedMap.name] =
						std::move(indexedMap);
				}
			}

			RegisterObjectResources(
				*field.value.block,
				sprites,
				progressBars,
				indexedMaps
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
		if (extension != ".gfx"
			&& extension != ".gui"
			&& extension != ".sgfx"
			&& extension != ".sgui")
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
		error = "no GUI definition files found in: "
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
	RegisterLayouts(document.root);
    documents_.push_back(std::move(document));
    return true;
}

void GuiInterpreter::RegisterResources(const GuiObject& object)
{
	RegisterObjectResources(
		object,
		sprites_,
		progressBars_,
		indexedMaps_
	);
}

void GuiInterpreter::RegisterLayouts(const GuiObject& object)
{
	CollectWindows(object, windows_);
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

const IndexedMapResource* GuiInterpreter::FindIndexedMap(
	const std::string& name
) const
{
	const auto iterator = indexedMaps_.find(name);
	return iterator == indexedMaps_.end()
		? nullptr
		: &iterator->second;
}

const WindowDefinition* GuiInterpreter::FindWindow(
	const std::string& name
) const
{
	for (const WindowDefinition& window : windows_)
	{
		if (window.name == name)
		{
			return &window;
		}
	}

	return nullptr;
}

std::vector<GuiResolvedWidget> GuiInterpreter::ResolveWindowLayout(
	const std::string& name,
	const GuiLayoutContext& context
) const
{
	const WindowDefinition* window = FindWindow(name);
	if (!window)
	{
		return {};
	}

	struct LayoutEntry
	{
		const WidgetDefinition* definition = nullptr;
		std::size_t lexicalParent = 0;
		std::size_t parent = 0;
		GuiResolvedWidget resolved;
		bool resolving = false;
		bool resolvedAlready = false;
	};

	std::vector<LayoutEntry> entries;
	std::unordered_map<std::string, std::size_t> names;

	entries.push_back({});
	entries[0].definition = window;
	entries[0].resolved.definition = window;
	entries[0].resolved.order = 0;
	if (!window->name.empty())
	{
		names[window->name] = 0;
	}

	std::function<void(
		const WidgetDefinition&,
		std::size_t
	)> collect = [&](
		const WidgetDefinition& parent,
		std::size_t parentIndex
	)
	{
		for (const WidgetDefinition& child : parent.children)
		{
			const std::size_t index = entries.size();
			entries.push_back({});
			entries[index].definition = &child;
			entries[index].lexicalParent = parentIndex;
			entries[index].resolved.definition = &child;
			entries[index].resolved.order = index;
			if (!child.name.empty())
			{
				names[child.name] = index;
			}
			collect(child, index);
		}
	};

	collect(*window, 0);

	for (std::size_t index = 1;
		 index < entries.size();
		 ++index)
	{
		const WidgetDefinition& definition =
			*entries[index].definition;
		entries[index].parent = entries[index].lexicalParent;

		if (!definition.parent.empty())
		{
			const auto parentIterator = names.find(
				definition.parent
			);
			if (parentIterator != names.end()
				&& parentIterator->second != index)
			{
				entries[index].parent = parentIterator->second;
			}
		}
	}

	auto evaluateCondition = [&context](
		const std::string& condition
	)
	{
		if (condition.empty()
			|| !context.conditionEvaluator)
		{
			return true;
		}

		return context.conditionEvaluator(condition);
	};

	std::function<void(std::size_t)> resolve = [&](
		std::size_t index
	)
	{
		LayoutEntry& entry = entries[index];
		if (entry.resolvedAlready)
		{
			return;
		}

		if (entry.resolving)
		{
			entry.parent = entry.lexicalParent;
			entry.resolving = false;
		}

		entry.resolving = true;
		if (index == 0)
		{
			entry.resolved.rect = entry.definition->rect;
			entry.resolved.visible =
				entry.definition->visible
				&& evaluateCondition(
					entry.definition->visibleWhen
				);
			entry.resolved.enabled =
				entry.definition->enabled
				&& evaluateCondition(
					entry.definition->enabledWhen
				);
			entry.resolved.depth = 0;
			entry.resolved.zOrder =
				entry.definition->zOrder;
		}
		else
		{
			if (entries[entry.parent].resolving)
			{
				entry.parent = entry.lexicalParent;
			}

			resolve(entry.parent);
			const GuiResolvedWidget& parent =
				entries[entry.parent].resolved;
			const WidgetDefinition& definition =
				*entry.definition;

			entry.resolved.rect = {
				parent.rect.x + definition.rect.x,
				parent.rect.y + definition.rect.y,
				definition.rect.width,
				definition.rect.height
			};
			entry.resolved.visible =
				parent.visible
				&& definition.visible
				&& evaluateCondition(
					definition.visibleWhen
				);
			entry.resolved.enabled =
				parent.enabled
				&& definition.enabled
				&& evaluateCondition(
					definition.enabledWhen
				);
			entry.resolved.depth = parent.depth + 1;
			entry.resolved.zOrder =
				parent.zOrder + definition.zOrder;
		}

		entry.resolving = false;
		entry.resolvedAlready = true;
	};

	for (std::size_t index = 0;
		 index < entries.size();
		 ++index)
	{
		resolve(index);
	}

	for (std::size_t index = 1;
		 index < entries.size();
		 ++index)
	{
		LayoutEntry& entry = entries[index];
		const WidgetDefinition& definition = *entry.definition;
		if (!definition.draggable
			|| definition.dragTrackName.empty()
			|| definition.dragValueSource.empty()
			|| !context.valueResolver)
		{
			continue;
		}
		const auto track = names.find(definition.dragTrackName);
		if (track == names.end())
		{
			continue;
		}
		const GuiRect& trackRect = entries[track->second].resolved.rect;
		double minimum = definition.dragMinimum;
		double maximum = definition.dragMaximum;
		if (maximum < minimum)
		{
			std::swap(minimum, maximum);
		}
		const double value = context.valueResolver(
			definition.dragValueSource
		);
		double normalized = maximum > minimum
			? std::clamp(
				(value - minimum) / (maximum - minimum),
				0.0,
				1.0
			)
			: 0.0;
		if (definition.dragInverted)
		{
			normalized = 1.0 - normalized;
		}
		const std::string axis = ToLower(definition.dragAxis);
		if (axis == "vertical" || axis == "y")
		{
			entry.resolved.rect.y = trackRect.y
				+ static_cast<int>(std::lround(
					normalized * std::max(
						0,
						trackRect.height - entry.resolved.rect.height
					)
				));
		}
		else
		{
			entry.resolved.rect.x = trackRect.x
				+ static_cast<int>(std::lround(
					normalized * std::max(
						0,
						trackRect.width - entry.resolved.rect.width
					)
				));
		}
	}

	std::vector<GuiResolvedWidget> output;
	output.reserve(entries.size());
	for (const LayoutEntry& entry : entries)
	{
		output.push_back(entry.resolved);
	}

	std::stable_sort(
		output.begin(),
		output.end(),
		[](const GuiResolvedWidget& first,
		   const GuiResolvedWidget& second)
		{
			if (first.zOrder != second.zOrder)
			{
				return first.zOrder < second.zOrder;
			}

			return first.order < second.order;
		}
	);

	return output;
}

std::vector<GuiListItemLayout> GuiInterpreter::InstantiateListItems(
	const std::string& windowName,
	const std::string& listName,
	std::size_t itemCount,
	const GuiLayoutContext& context
) const
{
	const WindowDefinition* window = FindWindow(windowName);
	if (!window)
	{
		return {};
	}

	std::unordered_map<
		std::string,
		const WidgetDefinition*
	> namedDefinitions;
	std::function<void(const WidgetDefinition&)> collectDefinitions =
		[&](const WidgetDefinition& widget)
	{
		if (!widget.name.empty())
		{
			namedDefinitions[widget.name] = &widget;
		}

		for (const WidgetDefinition& child : widget.children)
		{
			collectDefinitions(child);
		}
	};
	collectDefinitions(*window);

	const auto listIterator = namedDefinitions.find(listName);
	if (listIterator == namedDefinitions.end())
	{
		return {};
	}
	const WidgetDefinition* listDefinition =
		listIterator->second;

	const auto templateIterator = namedDefinitions.find(
		listDefinition->templateName
	);
	const WidgetDefinition* templateDefinition =
		templateIterator == namedDefinitions.end()
			? nullptr
			: templateIterator->second;

	if (!listDefinition || listDefinition->templateName.empty())
	{
		return {};
	}

	const std::vector<GuiResolvedWidget> resolved =
		ResolveWindowLayout(windowName, context);
	const GuiResolvedWidget* listResolved = nullptr;
	const GuiResolvedWidget* templateResolved = nullptr;
	for (const GuiResolvedWidget& widget : resolved)
	{
		if (!widget.definition)
		{
			continue;
		}

		if (widget.definition == listDefinition)
		{
			listResolved = &widget;
		}
		else if (widget.definition == templateDefinition)
		{
			templateResolved = &widget;
		}
	}

	if (!listResolved || !templateResolved)
	{
		return {};
	}
	if (!listResolved->visible || !templateResolved->visible)
	{
		return {};
	}

	const int itemWidth = templateResolved->rect.width;
	const int itemHeight = templateResolved->rect.height;
	if (itemWidth <= 0 || itemHeight <= 0)
	{
		return {};
	}

	const int columnGap = listDefinition->columnSpacing;
	const int rowGap = listDefinition->spacing;
	const int columnStep = itemWidth + columnGap;
	const int rowStep = itemHeight + rowGap;
	const std::string layoutMode = ToLower(
		listDefinition->layoutMode
	);
	if (layoutMode == "polar"
		|| layoutMode == "radial"
		|| layoutMode == "semicircle")
	{
		std::vector<int> ringCounts =
			listDefinition->polarRingItemCounts;
		const int requestedRingCount = std::max(
			0,
			listDefinition->polarRingCount
		);
		if (ringCounts.empty())
		{
			const int ringCount = std::max(1, requestedRingCount);
			ringCounts.assign(
				static_cast<std::size_t>(ringCount),
				static_cast<int>(itemCount / ringCount)
			);
			for (std::size_t index = 0;
				 index < itemCount % static_cast<std::size_t>(ringCount);
				 ++index)
			{
				++ringCounts[index];
			}
		}
		else
		{
			const std::size_t assigned = std::accumulate(
				ringCounts.begin(),
				ringCounts.end(),
				std::size_t{0}
			);
			if (assigned < itemCount)
			{
				ringCounts.back() += static_cast<int>(
					itemCount - assigned
				);
			}
		}

		const int ringCount = static_cast<int>(ringCounts.size());
		const int centerX = listResolved->rect.x
			+ (listDefinition->polarCenterX >= 0
				? listDefinition->polarCenterX
				: listResolved->rect.width / 2);
		const int centerY = listResolved->rect.y
			+ (listDefinition->polarCenterY >= 0
				? listDefinition->polarCenterY
				: listResolved->rect.height / 2);
		const int innerRadius = listDefinition->polarInnerRadius;
		int outerRadius = listDefinition->polarOuterRadius;
		if (outerRadius <= 0)
		{
			outerRadius = std::max(
				innerRadius,
				std::min(
					listResolved->rect.width,
					listResolved->rect.height * 2
				) / 2 - std::max(itemWidth, itemHeight) / 2
			);
		}
		int ringSpacing = listDefinition->polarRingSpacing;
		if (ringSpacing <= 0 && ringCount > 1)
		{
			ringSpacing = std::max(
				0,
				(outerRadius - innerRadius) / (ringCount - 1)
			);
		}

		constexpr double degreesToRadians =
			3.14159265358979323846 / 180.0;
		const double startAngle =
			listDefinition->polarStartAngle * degreesToRadians;
		const double endAngle =
			listDefinition->polarEndAngle * degreesToRadians;
		std::vector<GuiListItemLayout> output;
		output.reserve(itemCount);
		std::size_t itemIndex = 0;
		for (int ring = 0;
			 ring < ringCount && itemIndex < itemCount;
			 ++ring)
		{
			const int count = std::max(0, ringCounts[ring]);
			const int radius = ringCount <= 1
				? innerRadius
				: std::min(
					outerRadius,
					innerRadius + ring * ringSpacing
				);
			for (int position = 0;
				 position < count && itemIndex < itemCount;
				 ++position, ++itemIndex)
			{
				const double fraction = count <= 1
					? 0.5
					: static_cast<double>(position)
						/ static_cast<double>(count - 1);
				const double angle = startAngle
					+ (endAngle - startAngle) * fraction;
				const int x = static_cast<int>(std::lround(
					centerX + std::cos(angle) * radius
					- itemWidth / 2.0
				));
				const int y = static_cast<int>(std::lround(
					centerY + std::sin(angle) * radius
					- itemHeight / 2.0
				));
				output.push_back({
					templateDefinition,
					itemIndex,
					{x, y, itemWidth, itemHeight},
					templateResolved->visible,
					templateResolved->enabled,
					listResolved->zOrder + templateResolved->zOrder
				});
			}
		}
		return output;
	}
	const int columns = std::max(
		1,
		columnStep > 0
			? (listResolved->rect.width + columnGap)
				/ columnStep
			: 1
	);

	std::vector<GuiListItemLayout> output;
	output.reserve(itemCount);
	for (std::size_t index = 0;
		 index < itemCount;
		 ++index)
	{
		const int column = static_cast<int>(index)
			% columns;
		const int row = static_cast<int>(index)
			/ columns;

		output.push_back({
			templateDefinition,
			index,
			{
				listResolved->rect.x + column * columnStep,
				listResolved->rect.y + row * rowStep,
				itemWidth,
				itemHeight
			},
			templateResolved->visible,
			templateResolved->enabled,
			listResolved->zOrder + templateResolved->zOrder
		});
	}

	return output;
}

std::vector<GuiResolvedWidget> GuiInterpreter::InstantiateListWidgets(
	const std::string& windowName,
	const std::string& listName,
	std::size_t itemCount,
	int scrollOffset,
	const GuiLayoutContext& context
) const
{
	std::vector<GuiResolvedWidget> output;
	const std::vector<GuiListItemLayout> items = InstantiateListItems(
		windowName,
		listName,
		itemCount,
		context
	);
	output.reserve(items.size());

	for (const GuiListItemLayout& item : items)
	{
		if (!item.definition)
		{
			continue;
		}

		GuiResolvedWidget widget;
		widget.definition = item.definition;
		widget.rect = item.rect;
		widget.rect.y -= scrollOffset;
		widget.visible = item.visible;
		widget.enabled = item.enabled;
		widget.zOrder = item.zOrder;
		widget.order = item.index;
		widget.listName = listName;
		widget.listIndex = static_cast<int>(item.index);
		output.push_back(std::move(widget));
	}

	return output;
}

bool GuiInterpreter::ResolveListBinding(
	const std::string& windowName,
	const std::string& listName,
	GuiListBinding& output,
	const GuiLayoutContext& context
) const
{
	output = {};
	output.listName = listName;

	const WindowDefinition* window = FindWindow(windowName);
	if (!window)
	{
		return false;
	}

	std::unordered_map<
		std::string,
		const WidgetDefinition*
	> namedDefinitions;
	std::function<void(const WidgetDefinition&)> collectDefinitions =
		[&](const WidgetDefinition& widget)
	{
		if (!widget.name.empty())
		{
			namedDefinitions[widget.name] = &widget;
		}

		for (const WidgetDefinition& child : widget.children)
		{
			collectDefinitions(child);
		}
	};
	collectDefinitions(*window);

	const auto listIterator = namedDefinitions.find(listName);
	if (listIterator == namedDefinitions.end())
	{
		return false;
	}

	const WidgetDefinition* listDefinition =
		listIterator->second;
	output.templateName = listDefinition->templateName;
	output.scrollbarName = listDefinition->scrollBarName;
	output.disabledByListName = listDefinition->disabledByListName;
	output.disabledMatchField = listDefinition->disabledMatchField;
	output.disabledFilterField = listDefinition->disabledFilterField;
	output.disabledFilterValueSource =
		listDefinition->disabledFilterValueSource;
	output.layoutMode = listDefinition->layoutMode;
	output.spacing = listDefinition->spacing;
	output.columnSpacing = listDefinition->columnSpacing;

	const auto templateIterator = namedDefinitions.find(
		output.templateName
	);
	const WidgetDefinition* templateDefinition =
		templateIterator == namedDefinitions.end()
			? nullptr
			: templateIterator->second;

	const auto scrollbarIterator = namedDefinitions.find(
		output.scrollbarName
	);
	const WidgetDefinition* scrollbarDefinition =
		scrollbarIterator == namedDefinitions.end()
			? nullptr
			: scrollbarIterator->second;

	const std::vector<GuiResolvedWidget> resolved =
		ResolveWindowLayout(windowName, context);
	const GuiResolvedWidget* listResolved = nullptr;
	const GuiResolvedWidget* templateResolved = nullptr;
	const GuiResolvedWidget* scrollbarResolved = nullptr;
	for (const GuiResolvedWidget& widget : resolved)
	{
		if (widget.definition == listDefinition)
		{
			listResolved = &widget;
		}
		else if (widget.definition == templateDefinition)
		{
			templateResolved = &widget;
		}
		else if (widget.definition == scrollbarDefinition)
		{
			scrollbarResolved = &widget;
		}
	}

	if (!listResolved || !templateResolved)
	{
		return false;
	}
	if (!listResolved->visible || !templateResolved->visible)
	{
		return false;
	}

	output.viewport = listResolved->rect;
	output.item = templateResolved->rect;
	if (scrollbarResolved)
	{
		output.scrollbar = scrollbarResolved->rect;
		output.sliderName = scrollbarDefinition->sliderName;
		output.trackName = scrollbarDefinition->trackName;
	}
	output.valid = true;
	return true;
}

std::vector<GuiTextCommand> GuiInterpreter::BuildTextCommands(
	const std::string& windowName,
	const GuiLayoutContext& context
) const
{
	std::vector<GuiTextCommand> output;
	const std::vector<GuiResolvedWidget> widgets =
		ResolveWindowLayout(windowName, context);

	for (const GuiResolvedWidget& widget : widgets)
	{
		if (!widget.definition
			|| widget.definition->type != WidgetType::Text
			|| !widget.visible
				|| ToLower(widget.definition->renderMode) == "custom")
		{
			continue;
		}

		GuiTextAlignment alignment = GuiTextAlignment::Left;
		const std::string alignmentName = ToLower(
			widget.definition->alignment
		);
		if (alignmentName == "center"
			|| alignmentName == "centre")
		{
			alignment = GuiTextAlignment::Center;
		}
		else if (alignmentName == "right")
		{
			alignment = GuiTextAlignment::Right;
		}

		GuiTextCommand command;
		command.definition = widget.definition;
		command.rect = widget.rect;
		command.text = widget.definition->text;
		if (!widget.definition->textSource.empty()
			&& context.textResolver)
		{
			command.text = context.textResolver(
				widget.definition->textSource
			);
		}
		if (!widget.definition->localizationKey.empty()
			&& context.localizationResolver)
		{
			command.text = context.localizationResolver(
				widget.definition->localizationKey
			);
		}
		else if (widget.definition->localized
			&& context.localizationResolver)
		{
			command.text = context.localizationResolver(command.text);
		}
		if (command.text.empty())
		{
			continue;
		}
		command.font = widget.definition->font;
		command.alignment = alignment;
		command.fontSize = widget.definition->fontSize > 0
			? widget.definition->fontSize
			: std::max(12, widget.rect.height * 2 / 3);
		command.color[0] = widget.definition->textColor[0];
		command.color[1] = widget.definition->textColor[1];
		command.color[2] = widget.definition->textColor[2];
		command.zOrder = widget.zOrder;
		command.lineSpacing = widget.definition->lineSpacing;
		command.wrap = widget.definition->wrap;
		output.push_back(std::move(command));
	}

	return output;
}

std::vector<GuiTextCommand> GuiInterpreter::BuildListTextCommands(
	const std::string& windowName,
	const std::string& listName,
	const std::vector<std::string>& texts,
	const GuiLayoutContext& context
) const
{
	std::vector<GuiTextCommand> output;
	const std::vector<GuiListItemLayout> items = InstantiateListItems(
		windowName,
		listName,
		texts.size(),
		context
	);
	output.reserve(items.size());

	for (const GuiListItemLayout& item : items)
	{
		if (!item.definition
			|| !item.visible
			|| item.index >= texts.size())
		{
			continue;
		}

		GuiTextCommand command;
		command.definition = item.definition;
		command.rect = item.rect;
		command.text = texts[item.index];
		command.font = item.definition->font;
		const std::string alignmentName = ToLower(
			item.definition->alignment
		);
		if (alignmentName == "center"
			|| alignmentName == "centre")
		{
			command.alignment = GuiTextAlignment::Center;
		}
		else if (alignmentName == "right")
		{
			command.alignment = GuiTextAlignment::Right;
		}
		command.fontSize = item.definition->fontSize > 0
			? item.definition->fontSize
			: std::max(12, item.rect.height * 2 / 3);
		command.color[0] = item.definition->textColor[0];
		command.color[1] = item.definition->textColor[1];
		command.color[2] = item.definition->textColor[2];
		command.zOrder = item.zOrder;
		output.push_back(std::move(command));
	}

	return output;
}

std::vector<GuiTextCommand> GuiInterpreter::BuildListTextCommands(
	const std::string& windowName,
	const std::string& listName,
	const GuiLayoutContext& context
) const
{
	if (!context.listResolver)
	{
		return {};
	}

	const GuiListModel* model =
		context.listResolver(listName);
	if (!model)
	{
		return {};
	}

	const WindowDefinition* window = FindWindow(windowName);
	if (!window)
	{
		return {};
	}
	const WidgetDefinition* listDefinition = nullptr;
	const WidgetDefinition* templateDefinition = nullptr;
	std::function<void(const WidgetDefinition&)> findDefinitions =
		[&](const WidgetDefinition& widget)
	{
		if (widget.name == listName)
		{
			listDefinition = &widget;
		}
		for (const WidgetDefinition& child : widget.children)
		{
			findDefinitions(child);
		}
	};
	findDefinitions(*window);
	if (!listDefinition)
	{
		return {};
	}
	std::function<void(const WidgetDefinition&)> findTemplate =
		[&](const WidgetDefinition& widget)
	{
		if (widget.name == listDefinition->templateName)
		{
			templateDefinition = &widget;
		}
		for (const WidgetDefinition& child : widget.children)
		{
			findTemplate(child);
		}
	};
	findTemplate(*window);
	if (!templateDefinition)
	{
		return {};
	}

	const WidgetDefinition* textDefinition = templateDefinition;
	std::function<void(const WidgetDefinition&)> findText =
		[&](const WidgetDefinition& widget)
	{
		if (textDefinition != templateDefinition)
		{
			return;
		}
		for (const WidgetDefinition& child : widget.children)
		{
			if (child.type == WidgetType::Text)
			{
				textDefinition = &child;
				return;
			}
			findText(child);
		}
	};
	findText(*templateDefinition);

	const std::vector<GuiListItemLayout> items = InstantiateListItems(
		windowName,
		listName,
		model->items.size(),
		context
	);
	std::vector<GuiTextCommand> output;
	output.reserve(items.size());
	for (const GuiListItemLayout& layout : items)
	{
		if (!layout.definition
			|| !layout.visible
			|| layout.index >= model->items.size())
		{
			continue;
		}
		const GuiListItem& item = model->items[layout.index];
		std::string text = item.text;
		if (textDefinition != templateDefinition)
		{
			text = textDefinition->text;
			std::string source = textDefinition->textSource;
			constexpr std::string_view prefix = "item.";
			if (source.rfind(prefix, 0) == 0)
			{
				if (const GuiDataValue* value = item.Find(
					source.substr(prefix.size())
				))
				{
					if (const auto* string = std::get_if<std::string>(value))
					{
						text = *string;
					}
					else if (const auto* integer = std::get_if<int64_t>(value))
					{
						text = std::to_string(*integer);
					}
					else if (const auto* number = std::get_if<double>(value))
					{
						text = std::to_string(*number);
					}
					else if (const auto* boolean = std::get_if<bool>(value))
					{
						text = *boolean ? "true" : "false";
					}
				}
			}
			else if (!source.empty() && context.textResolver)
			{
				text = context.textResolver(source);
			}
			if (!textDefinition->localizationKey.empty()
				&& context.localizationResolver)
			{
				text = context.localizationResolver(
					textDefinition->localizationKey
				);
			}
			else if (textDefinition->localized
				&& context.localizationResolver)
			{
				text = context.localizationResolver(text);
			}
		}
		if (text.empty())
		{
			continue;
		}

		GuiTextCommand command;
		command.definition = textDefinition;
		command.rect = layout.rect;
		if (textDefinition != templateDefinition)
		{
			command.rect.x += textDefinition->rect.x;
			command.rect.y += textDefinition->rect.y;
			command.rect.width = textDefinition->rect.width;
			command.rect.height = textDefinition->rect.height;
		}
		command.text = std::move(text);
		command.font = textDefinition->font;
		const std::string alignmentName = ToLower(
			textDefinition->alignment
		);
		if (alignmentName == "center" || alignmentName == "centre")
		{
			command.alignment = GuiTextAlignment::Center;
		}
		else if (alignmentName == "right")
		{
			command.alignment = GuiTextAlignment::Right;
		}
		command.fontSize = textDefinition->fontSize > 0
			? textDefinition->fontSize
			: std::max(12, command.rect.height * 2 / 3);
		command.color[0] = textDefinition->textColor[0];
		command.color[1] = textDefinition->textColor[1];
		command.color[2] = textDefinition->textColor[2];
		command.zOrder = layout.zOrder + textDefinition->zOrder;
		command.lineSpacing = textDefinition->lineSpacing;
		command.wrap = textDefinition->wrap;
		output.push_back(std::move(command));
	}
	return output;
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

namespace
{

std::filesystem::path ResolveIndexedMapAsset(
	const std::filesystem::path& asset,
	const std::filesystem::path& projectRoot
)
{
	if (asset.empty())
	{
		return {};
	}
	if (asset.is_absolute())
	{
		return asset;
	}

	std::string normalized = asset.string();
	std::replace(normalized.begin(), normalized.end(), '\\', '/');
	if (normalized.rfind("gfx/", 0) == 0)
	{
		return projectRoot / normalized;
	}

	const std::filesystem::path rootRelative = projectRoot / normalized;
	if (std::filesystem::exists(rootRelative))
	{
		return rootRelative;
	}
	return projectRoot / "gfx" / normalized;
}

}

std::filesystem::path GuiInterpreter::ResolveIndexedMapTexture(
	const std::string& resourceName,
	const std::filesystem::path& projectRoot
) const
{
	const IndexedMapResource* resource = FindIndexedMap(resourceName);
	return resource
		? ResolveIndexedMapAsset(resource->textureFile, projectRoot)
		: std::filesystem::path{};
}

std::filesystem::path GuiInterpreter::ResolveIndexedMapIndex(
	const std::string& resourceName,
	const std::filesystem::path& projectRoot
) const
{
	const IndexedMapResource* resource = FindIndexedMap(resourceName);
	return resource
		? ResolveIndexedMapAsset(resource->indexFile, projectRoot)
		: std::filesystem::path{};
}

const GuiResolvedWidget* HitTestGuiWidgets(
	const std::vector<GuiResolvedWidget>& widgets,
	int mouseX,
	int mouseY
)
{
	for (auto iterator = widgets.rbegin();
		 iterator != widgets.rend();
		 ++iterator)
	{
		if (!iterator->definition
			|| !iterator->visible
			|| !iterator->enabled)
		{
			continue;
		}

		const WidgetType type = iterator->definition->type;
		if (type == WidgetType::MarkerLayer)
		{
			continue;
		}
		if (!iterator->definition->draggable
			&& type != WidgetType::Window
			&& type != WidgetType::Button
			&& type != WidgetType::ListBox
			&& type != WidgetType::ScrollBar
			&& type != WidgetType::IndexedMap
			&& type != WidgetType::Custom)
		{
			continue;
		}

		const GuiRect& rect = iterator->rect;
		if (mouseX >= rect.x
			&& mouseX < rect.x + rect.width
			&& mouseY >= rect.y
			&& mouseY < rect.y + rect.height)
		{
			return &*iterator;
		}
	}

	return nullptr;
}

int HitTestGuiListItems(
	const std::vector<GuiListItemLayout>& items,
	const GuiRect& viewport,
	int scrollOffset,
	int mouseX,
	int mouseY
)
{
	if (mouseX < viewport.x
		|| mouseX >= viewport.x + viewport.width
		|| mouseY < viewport.y
		|| mouseY >= viewport.y + viewport.height)
	{
		return -1;
	}

	for (auto iterator = items.rbegin();
		 iterator != items.rend();
		 ++iterator)
	{
		GuiRect rect = iterator->rect;
		rect.y -= scrollOffset;
		if (mouseX >= rect.x
			&& mouseX < rect.x + rect.width
			&& mouseY >= rect.y
			&& mouseY < rect.y + rect.height)
		{
			return static_cast<int>(iterator->index);
		}
	}

	return -1;
}

}
