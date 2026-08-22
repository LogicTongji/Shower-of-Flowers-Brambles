#include "gui_interpreter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>

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

			output.fields.push_back({key.text, std::move(value), key.line});
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
				output.fields.push_back({{}, std::move(value), key.line});
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

            output.fields.push_back({key.text, std::move(value), key.line});
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
	std::initializer_list<const char*> names,
	std::string fallback = {}
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

	return fallback;
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

std::string ReplaceItemId(std::string value, uint64_t itemId)
{
	constexpr std::string_view placeholder = "{id}";
	const std::string replacement = std::to_string(itemId);
	std::size_t position = value.find(placeholder);
	while (position != std::string::npos)
	{
		value.replace(position, placeholder.size(), replacement);
		position = value.find(
			placeholder,
			position + replacement.size()
		);
	}
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

	if (value == "yes" || value == "true" || value == "on"
		|| value == "1")
	{
		return true;
	}

	if (value == "no" || value == "false" || value == "off"
		|| value == "0")
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

void ReadFloatPair(
	const GuiObject& object,
	const std::string& name,
	float& first,
	float& second
)
{
	const GuiValue* value = FindValue(object, name);
	if (!value || value->kind != ValueKind::Block || !value->block)
	{
		return;
	}
	first = GetFloat(*value->block, "x", first);
	second = GetFloat(*value->block, "y", second);
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

void ReadNineSlice(
    const GuiObject& object,
	GuiNineSliceInsets& output,
	std::string_view primaryName = "nineSlice",
	std::string_view aliasName = "nine_slice"
)
{
    const GuiValue* value =
        FindValue(
            object,
			std::string(primaryName)
        );

    // 顺便兼容 nine_slice 写法。
    if (!value)
    {
        value =
            FindValue(
                object,
				std::string(aliasName)
            );
    }

    if (
        !value
        || value->kind != ValueKind::Block
        || !value->block
    )
    {
        return;
    }

    output.left =
        std::max(
            0,
            GetInteger(
                *value->block,
                "left",
                0
            )
        );

    output.top =
        std::max(
            0,
            GetInteger(
                *value->block,
                "top",
                0
            )
        );

    output.right =
        std::max(
            0,
            GetInteger(
                *value->block,
                "right",
                0
            )
        );

    output.bottom =
        std::max(
            0,
            GetInteger(
                *value->block,
                "bottom",
                0
            )
        );
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

using GuiFieldNameSet = std::unordered_set<std::string>;

const GuiFieldNameSet& WidgetFieldNames()
{
	static const GuiFieldNameSet names = {
		"name", "parent", "font", "text", "textsource",
		"textbinding", "textvalue", "localizationkey",
		"localisationkey", "textkey", "orientation", "layout",
		"layoutmode", "itemlayout", "positiontype", "position_type",
		"customtype", "type", "spritetype", "spritesource",
		"spritebinding", "texturesource", "spritevalueprefix",
		"spriteprefix", "frame", "framesource", "framebinding",
		"rotation", "rotationsource", "pivot", "transformscale",
		"scalesource", "transformscalesource", "scalexsource",
		"transformscalexsource", "scaleysource",
		"transformscaleysource", "flipx", "flipy",
		"animate", "animationmode", "animationtimesource",
		"animationframetime", "animationframeduration", "animationfps",
		"animationstartframe", "animationendframe", "animationoffset",
		"bordersprite", "framesprite", "windowframe",
		"quadtexturesprite", "pressedtexturesprite",
		"pressedspritesource", "pressedtexturesource",
		"pressedquadtexturesprite", "itemtemplate", "disableitemsinlist",
		"disabledbylist", "disablematchingfield", "disabledmatchfield",
		"disablefilterfield", "disabledfilterfield",
		"disablefiltervaluesource", "disabledfiltervaluesource",
		"itemfilterfield", "filterfield", "itemfiltervaluesource",
		"filtervaluesource", "scrollbartype", "slider", "track",
		"progressbar", "progressresource", "progresstype", "mapresource",
		"effecttype", "effectresource", "effectsource",
		"effecttimesource",
		"indexedmap", "indexedmapresource", "resource", "datasource",
		"listsource", "itemssource", "catalogsource", "itemcatalog",
		"fallbackdatasource", "mapwidget", "targetmap",
		"indexedmapwidget", "portraitsource", "imagesource",
		"itemspritesource", "regionsource", "regionidsource",
		"anchoritemsource", "xsource", "markerxsource", "ysource",
		"markerysource", "descriptionsource", "markerdescriptionsource",
		"namesource", "titlesource", "progresscolor", "colorindex",
		"scalemode", "scale", "fit", "nineslice", "nine_slice",
		"alignment", "textalignment", "align", "rendermode", "drawmode",
		"valuesource", "valuebinding", "progresssource", "tooltiptext",
		"tooltip", "delayedtooltiptext", "tooltipsource",
		"tooltipbinding", "tooltipvalue", "tooltiplocalizationkey",
		"tooltiptextkey", "tooltipsprite", "tooltipbackgroundsprite",
		"tooltipfont", "tooltipscalemode", "tooltipplacement",
		"tooltipside", "tooltipnineslice", "tooltip_nine_slice",
		"markeractionsprite", "selectedactionsprite", "onmarkeraction",
		"markeraction", "selectedaction", "markeractionlocalizationkey",
		"markeractiontextkey", "stacksource", "markerstacksource",
		"stackgroupsource", "stackordersource", "markerstackordersource",
		"stackdirection", "markerstackdirection", "dragaxis",
		"dragorientation", "dragtrack", "dragbounds", "trackwidget",
		"dragvaluesource", "dragbinding", "visiblewhen", "visible_if",
		"showif", "condition", "enabledwhen", "enabled_if", "onclick",
		"clickaction", "action", "callback", "onpress", "pressaction",
		"onrelease", "releaseaction", "onhoverenter", "onhover",
		"onmouseenter", "hoverenteraction", "onhoverleave",
		"onmouseleave", "hoverleaveaction", "ondragstart", "ondrag",
		"ondragend", "spacing", "columnspacing", "polarcenterx",
		"polarcentery", "polarcenter", "polarringcount",
		"polarinnerradius", "polarouterradius", "polarringspacing",
		"polarringitemcounts", "fontsize", "textsize", "linespacing",
		"markeractionfontsize", "stackspacing", "markerstackspacing",
		"dragsteps", "polarstartangle", "polarendangle", "dragminimum",
		"dragmaximum", "dragstep", "disabledbrightness",
		"disabledopacity", "color", "linecolor", "tooltipcolor",
		"tooltiptextcolor", "position", "size", "markersize",
		"portraitposition", "portraitsize", "tooltipsize", "tooltipoffset",
		"markeractionposition", "markeractionsize", "zorder", "z",
		"layer", "framezorder", "clipchildren", "clip_children", "clip",
		"value", "opacity", "alpha", "fillfromend", "reverse",
		"drawbackground", "moveable", "draggable", "draginverted",
		"localizetooltip", "localisetooltip", "tooltipwrap",
		"avoidtooltipoverlap", "tooltipavoidmarkers", "linewidth",
		"tooltippadding", "tooltipsearchstep", "tooltipfontsize",
		"tooltiplinespacing", "tooltipdelay", "minimumthumbsize",
		"localized", "localised", "wrap", "wordwrap", "dragheight",
		"visible", "dontrender", "disabled", "enabled", "fullscreen",
		"styledefaults",
		// 已识别的原版窗口兼容字段；当前尚未参与自定义布局。
		"horizontalborder", "verticalborder", "background"
	};
	return names;
}

const GuiFieldNameSet& StyleDefaultFieldNames()
{
	static const GuiFieldNameSet names = {
		"color", "linecolor", "tooltipcolor", "tooltiptextcolor",
		"tooltipsize", "tooltipoffset", "tooltipnineslice",
		"tooltip_nine_slice", "tooltipsprite", "tooltipbackgroundsprite",
		"tooltipfont", "tooltipscalemode", "tooltipplacement",
		"framezorder", "fontsize", "linespacing", "linewidth",
		"tooltippadding", "tooltipsearchstep", "tooltipfontsize",
		"tooltiplinespacing", "tooltipdelay", "minimumthumbsize",
		"disabledbrightness", "disabledopacity", "localizetooltip",
		"localisetooltip", "tooltipwrap"
	};
	return names;
}

const GuiFieldNameSet& WidgetTypeNames()
{
	static const GuiFieldNameSet names = {
		"windowtype", "icontype", "textboxtype", "instanttextboxtype",
		"guibuttontype", "listboxtype", "progressbartype",
		"scrollbartype", "colorboxtype", "indexedmaptype",
		"markerlayertype", "customwidgettype"
	};
	return names;
}

const GuiFieldNameSet& SpriteFieldNames()
{
	static const GuiFieldNameSet names = {
		"name", "texturefile", "effectfile", "loadtype", "noofframes",
		"norefcount", "framelayout", "animation"
	};
	return names;
}

const GuiFieldNameSet& SpriteAnimationFieldNames()
{
	static const GuiFieldNameSet names = {
		"enabled", "mode", "frametime", "frameduration", "fps",
		"startframe", "endframe", "offset"
	};
	return names;
}

const GuiFieldNameSet& ProgressResourceFieldNames()
{
	static const GuiFieldNameSet names = {
		"name", "texturefile1", "texturefile2", "effectfile", "color",
		"colortwo", "size", "width", "height", "horizontal"
	};
	return names;
}

const GuiFieldNameSet& IndexedMapResourceFieldNames()
{
	static const GuiFieldNameSet names = {
		"name", "texturefile", "indexfile", "sourcedefinitionfile",
		"definitionfile", "sourceprovincefile", "provincefile",
		"sourcegroupfile", "groupfile", "sourcefillcolor",
		"sourceboundarycolor", "boundarycolor", "hovercolor",
		"boundarywidth", "croppadding", "flipvertical", "drawboundaries",
		"sourceitem", "colorstop"
	};
	return names;
}

const GuiFieldNameSet& EffectResourceFieldNames()
{
	static const GuiFieldNameSet names = {
		"name", "effect", "color", "minimum", "maximum", "speed",
		"phase", "enabled"
	};
	return names;
}

const GuiFieldNameSet& ContainerFieldNames()
{
	static const GuiFieldNameSet names = {
		"guitypes", "spritetypes", "windowtype", "icontype",
		"textboxtype", "instanttextboxtype", "guibuttontype",
		"listboxtype", "progressbartype", "scrollbartype", "colorboxtype",
		"indexedmaptype", "markerlayertype", "customwidgettype",
		"spritetype", "indexedmapresourcetype", "effecttype",
		"positiontype"
	};
	return names;
}

std::size_t GuiFieldEditDistance(
	std::string_view first,
	std::string_view second
)
{
	std::vector<std::size_t> previous(second.size() + 1);
	std::vector<std::size_t> current(second.size() + 1);
	std::iota(previous.begin(), previous.end(), 0);
	for (std::size_t firstIndex = 1;
		firstIndex <= first.size();
		++firstIndex)
	{
		current[0] = firstIndex;
		for (std::size_t secondIndex = 1;
			secondIndex <= second.size();
			++secondIndex)
		{
			const std::size_t substitution = previous[secondIndex - 1]
				+ (first[firstIndex - 1] == second[secondIndex - 1] ? 0 : 1);
			current[secondIndex] = std::min({
				previous[secondIndex] + 1,
				current[secondIndex - 1] + 1,
				substitution
			});
		}
		previous.swap(current);
	}
	return previous.back();
}

std::string SuggestGuiField(
	std::string_view name,
	const GuiFieldNameSet& allowed
)
{
	const std::string normalized = ToLower(std::string(name));
	const std::size_t maximumDistance = normalized.size() <= 4
		? 1
		: normalized.size() <= 8 ? 2 : 3;
	std::size_t bestDistance = maximumDistance + 1;
	std::string best;
	for (const std::string& candidate : allowed)
	{
		const std::size_t distance = GuiFieldEditDistance(
			normalized,
			candidate
		);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = candidate;
		}
	}
	return bestDistance <= maximumDistance ? best : std::string{};
}

void AddUnknownGuiFieldDiagnostic(
	const GuiField& field,
	const std::filesystem::path& path,
	std::string_view context,
	const GuiFieldNameSet& allowed,
	std::vector<std::string>& diagnostics
)
{
	std::string diagnostic = path.string();
	if (field.line > 0)
	{
		diagnostic += ":" + std::to_string(field.line);
	}
	diagnostic += ": unknown field '" + field.name
		+ "' in " + std::string(context);
	const std::string suggestion = SuggestGuiField(field.name, allowed);
	if (!suggestion.empty())
	{
		diagnostic += "; did you mean '" + suggestion + "'?";
	}
	diagnostics.push_back(std::move(diagnostic));
}

void AddGuiSchemaDiagnostic(
	const std::filesystem::path& path,
	int line,
	std::string_view context,
	std::string message,
	std::vector<std::string>& diagnostics
)
{
	std::string diagnostic = path.string();
	if (line > 0)
	{
		diagnostic += ":" + std::to_string(line);
	}
	diagnostic += ": " + std::string(context) + ": " + message;
	diagnostics.push_back(std::move(diagnostic));
}

std::vector<const GuiField*> FindGuiFields(
	const GuiObject& object,
	std::initializer_list<const char*> names
)
{
	GuiFieldNameSet normalizedNames;
	for (const char* name : names)
	{
		normalizedNames.insert(ToLower(name));
	}
	std::vector<const GuiField*> result;
	for (const GuiField& field : object.fields)
	{
		if (normalizedNames.find(ToLower(field.name))
			!= normalizedNames.end())
		{
			result.push_back(&field);
		}
	}
	return result;
}

const GuiField* FindGuiField(
	const GuiObject& object,
	std::initializer_list<const char*> names
)
{
	const std::vector<const GuiField*> fields = FindGuiFields(object, names);
	return fields.empty() ? nullptr : fields.front();
}

bool ParseGuiInteger(std::string_view value, int64_t& output)
{
	try
	{
		std::size_t parsed = 0;
		const long long number = std::stoll(std::string(value), &parsed);
		if (parsed != value.size())
		{
			return false;
		}
		output = static_cast<int64_t>(number);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool ParseGuiNumber(std::string_view value, double& output)
{
	try
	{
		std::size_t parsed = 0;
		const double number = std::stod(std::string(value), &parsed);
		if (parsed != value.size() || !std::isfinite(number))
		{
			return false;
		}
		output = number;
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool ParseGuiBoolean(std::string value, bool& output)
{
	value = ToLower(std::move(value));
	if (value == "yes" || value == "true" || value == "on"
		|| value == "1")
	{
		output = true;
		return true;
	}
	if (value == "no" || value == "false" || value == "off"
		|| value == "0")
	{
		output = false;
		return true;
	}
	return false;
}

void ValidateGuiFieldKind(
	const GuiField& field,
	ValueKind expected,
	const std::filesystem::path& path,
	std::string_view context,
	std::vector<std::string>& diagnostics
)
{
	if (field.value.kind == expected)
	{
		return;
	}
	AddGuiSchemaDiagnostic(
		path,
		field.line,
		context,
		"field '" + field.name + "' requires a "
			+ (expected == ValueKind::Scalar ? "scalar" : "block")
			+ " value",
		diagnostics
	);
}

void ValidateGuiScalarType(
	const GuiField& field,
	std::string_view expected,
	const std::filesystem::path& path,
	std::string_view context,
	std::vector<std::string>& diagnostics,
	double minimum = -std::numeric_limits<double>::infinity(),
	double maximum = std::numeric_limits<double>::infinity()
)
{
	if (field.value.kind != ValueKind::Scalar)
	{
		ValidateGuiFieldKind(
			field, ValueKind::Scalar, path, context, diagnostics
		);
		return;
	}
	bool valid = true;
	double numeric = 0.0;
	if (expected == "boolean")
	{
		bool parsed = false;
		valid = ParseGuiBoolean(field.value.scalar, parsed);
	}
	else if (expected == "integer")
	{
		int64_t parsed = 0;
		valid = ParseGuiInteger(field.value.scalar, parsed);
		numeric = static_cast<double>(parsed);
	}
	else if (expected == "number")
	{
		valid = ParseGuiNumber(field.value.scalar, numeric);
	}
	if (!valid)
	{
		AddGuiSchemaDiagnostic(
			path,
			field.line,
			context,
			"field '" + field.name + "' requires "
				+ std::string(expected) + ", got '"
				+ field.value.scalar + "'",
			diagnostics
		);
		return;
	}
	if ((expected == "integer" || expected == "number")
		&& (numeric < minimum || numeric > maximum))
	{
		std::ostringstream message;
		message << "field '" << field.name << "' value " << numeric
			<< " is outside [" << minimum << ", " << maximum << "]";
		AddGuiSchemaDiagnostic(
			path, field.line, context, message.str(), diagnostics
		);
	}
}

void ValidateGuiRequiredField(
	const GuiObject& object,
	std::initializer_list<const char*> names,
	ValueKind kind,
	std::string_view displayName,
	const std::filesystem::path& path,
	std::string_view context,
	std::vector<std::string>& diagnostics
)
{
	const GuiField* field = FindGuiField(object, names);
	if (!field)
	{
		AddGuiSchemaDiagnostic(
			path,
			0,
			context,
			"missing required field '" + std::string(displayName) + "'",
			diagnostics
		);
		return;
	}
	ValidateGuiFieldKind(*field, kind, path, context, diagnostics);
	if (kind == ValueKind::Scalar && field->value.scalar.empty())
	{
		AddGuiSchemaDiagnostic(
			path,
			field->line,
			context,
			"required field '" + std::string(displayName)
				+ "' cannot be empty",
			diagnostics
		);
	}
}

void ValidateGuiMutuallyExclusive(
	const GuiObject& object,
	std::initializer_list<const char*> names,
	const std::filesystem::path& path,
	std::string_view context,
	std::vector<std::string>& diagnostics
)
{
	const std::vector<const GuiField*> fields = FindGuiFields(object, names);
	if (fields.size() <= 1)
	{
		return;
	}
	std::string message = "mutually exclusive fields are set:";
	for (const GuiField* field : fields)
	{
		message += " '" + field->name + "'";
	}
	AddGuiSchemaDiagnostic(
		path, fields.front()->line, context, std::move(message), diagnostics
	);
}

void ValidateGuiEnum(
	const GuiField& field,
	const GuiFieldNameSet& values,
	const std::filesystem::path& path,
	std::string_view context,
	std::vector<std::string>& diagnostics
)
{
	if (field.value.kind != ValueKind::Scalar)
	{
		ValidateGuiFieldKind(
			field, ValueKind::Scalar, path, context, diagnostics
		);
		return;
	}
	if (values.find(ToLower(field.value.scalar)) != values.end())
	{
		return;
	}
	AddGuiSchemaDiagnostic(
		path,
		field.line,
		context,
		"field '" + field.name + "' has unsupported value '"
			+ field.value.scalar + "'",
		diagnostics
	);
}

std::string GuiObjectContext(
	std::string_view typeName,
	const GuiObject& object
)
{
	const std::string name = GetScalar(object, "name");
	return name.empty()
		? std::string(typeName)
		: std::string(typeName) + " '" + name + "'";
}

bool IsGuiWidgetFieldApplicable(
	std::string_view fieldName,
	WidgetType type
)
{
	const std::string name = ToLower(std::string(fieldName));
	static const GuiFieldNameSet windowOnly = {
		"fullscreen", "moveable", "dragheight", "horizontalborder",
		"verticalborder", "background", "styledefaults"
	};
	static const GuiFieldNameSet listOnly = {
		"itemtemplate", "disableitemsinlist", "disabledbylist",
		"disablematchingfield", "disabledmatchfield",
		"disablefilterfield", "disabledfilterfield",
		"disablefiltervaluesource", "disabledfiltervaluesource",
		"itemfilterfield", "filterfield", "itemfiltervaluesource",
		"filtervaluesource", "scrollbartype", "layout", "layoutmode",
		"itemlayout", "spacing", "columnspacing", "polarcenterx",
		"polarcentery", "polarcenter", "polarringcount",
		"polarinnerradius", "polarouterradius", "polarringspacing",
		"polarringitemcounts", "polarstartangle", "polarendangle"
	};
	static const GuiFieldNameSet progressOnly = {
		"progressbar", "progressresource", "progresstype",
		"progresscolor", "colorindex", "fillfromend", "reverse",
		"drawbackground"
	};
	static const GuiFieldNameSet spriteFrameOnly = {
		"frame", "framesource", "framebinding", "animate",
		"animationmode", "animationtimesource", "animationframetime",
		"animationframeduration", "animationfps", "animationstartframe",
		"animationendframe", "animationoffset"
	};
	static const GuiFieldNameSet scrollOnly = {
		"slider", "track", "minimumthumbsize"
	};
	static const GuiFieldNameSet indexedMapOnly = {
		"mapresource", "indexedmap", "indexedmapresource", "resource"
	};
	static const GuiFieldNameSet markerOnly = {
		"catalogsource", "itemcatalog", "fallbackdatasource",
		"mapwidget", "targetmap", "indexedmapwidget",
		"portraitsource", "imagesource", "itemspritesource",
		"regionsource", "regionidsource", "anchoritemsource",
		"xsource", "markerxsource", "ysource", "markerysource",
		"descriptionsource", "markerdescriptionsource", "namesource",
		"titlesource", "markersize", "portraitposition",
		"portraitsize", "markeractionsprite", "selectedactionsprite",
		"onmarkeraction", "markeraction", "selectedaction",
		"markeractionlocalizationkey", "markeractiontextkey",
		"markeractionposition", "markeractionsize",
		"markeractionfontsize", "stacksource", "markerstacksource",
		"stackgroupsource", "stackordersource",
		"markerstackordersource", "stackdirection",
		"markerstackdirection", "stackspacing", "markerstackspacing"
	};
	static const GuiFieldNameSet transformOnly = {
		"rotation", "rotationsource", "pivot", "transformscale",
		"scalesource", "transformscalesource", "scalexsource",
		"transformscalexsource", "scaleysource",
		"transformscaleysource", "flipx", "flipy"
	};
	static const GuiFieldNameSet effectOnly = {
		"effecttype", "effectresource", "effectsource",
		"effecttimesource"
	};
	if (windowOnly.find(name) != windowOnly.end())
	{
		return type == WidgetType::Window;
	}
	if (listOnly.find(name) != listOnly.end())
	{
		return type == WidgetType::ListBox;
	}
	if (progressOnly.find(name) != progressOnly.end())
	{
		return type == WidgetType::ProgressBar;
	}
	if (spriteFrameOnly.find(name) != spriteFrameOnly.end())
	{
		return type == WidgetType::Image || type == WidgetType::Button;
	}
	if (scrollOnly.find(name) != scrollOnly.end())
	{
		return type == WidgetType::ScrollBar;
	}
	if (indexedMapOnly.find(name) != indexedMapOnly.end())
	{
		return type == WidgetType::IndexedMap;
	}
	if (markerOnly.find(name) != markerOnly.end())
	{
		return type == WidgetType::MarkerLayer;
	}
	if (transformOnly.find(name) != transformOnly.end())
	{
		return type == WidgetType::Image
			|| type == WidgetType::Text
			|| type == WidgetType::Button
			|| type == WidgetType::ProgressBar
			|| type == WidgetType::ScrollBar
			|| type == WidgetType::ColorBox;
	}
	if (effectOnly.find(name) != effectOnly.end())
	{
		return type != WidgetType::ListBox
			&& type != WidgetType::MarkerLayer
			&& type != WidgetType::Custom
			&& type != WidgetType::Unknown;
	}
	if (name == "customtype" || name == "type")
	{
		return type == WidgetType::Custom;
	}
	return true;
}

void ValidateStrictGuiWidgetScalar(
	const GuiField& field,
	const std::filesystem::path& path,
	std::string_view context,
	std::vector<std::string>& diagnostics
)
{
	const std::string name = ToLower(field.name);
	static const GuiFieldNameSet booleans = {
		"fillfromend", "reverse", "drawbackground", "moveable",
		"draggable", "draginverted", "localizetooltip",
		"localisetooltip", "tooltipwrap", "avoidtooltipoverlap",
		"tooltipavoidmarkers", "clipchildren", "clip_children", "clip",
		"localized", "localised", "wrap", "wordwrap", "visible",
		"dontrender", "disabled", "enabled", "fullscreen", "animate",
		"flipx", "flipy"
	};
	static const GuiFieldNameSet nonNegativeIntegers = {
		"spacing", "columnspacing", "polarcenterx", "polarcentery",
		"polarringcount", "polarinnerradius", "polarouterradius",
		"polarringspacing", "fontsize", "textsize", "linespacing",
		"markeractionfontsize", "stackspacing", "markerstackspacing",
		"dragsteps", "dragheight", "linewidth", "tooltippadding",
		"tooltipsearchstep", "tooltipfontsize", "tooltiplinespacing",
		"tooltipdelay", "minimumthumbsize",
		"animationframetime", "animationframeduration",
		"animationstartframe", "animationendframe"
	};
	static const GuiFieldNameSet integers = {
		"zorder", "z", "layer", "framezorder", "animationoffset"
	};
	static const GuiFieldNameSet normalizedNumbers = {
		"opacity", "alpha", "disabledbrightness", "disabledopacity",
		"value"
	};
	static const GuiFieldNameSet numbers = {
		"polarstartangle", "polarendangle", "dragminimum",
		"dragmaximum", "dragstep", "animationfps", "rotation"
	};
	if (name == "frame")
	{
		ValidateGuiScalarType(
			field, "integer", path, context, diagnostics, 1.0
		);
	}
	else if (name == "progresscolor" || name == "colorindex")
	{
		ValidateGuiScalarType(
			field, "integer", path, context, diagnostics, 0.0, 1.0
		);
	}
	else if (name == "animationfps")
	{
		ValidateGuiScalarType(
			field, "number", path, context, diagnostics,
			std::numeric_limits<double>::min()
		);
	}
	else if (booleans.find(name) != booleans.end())
	{
		ValidateGuiScalarType(
			field, "boolean", path, context, diagnostics
		);
	}
	else if (nonNegativeIntegers.find(name) != nonNegativeIntegers.end())
	{
		ValidateGuiScalarType(
			field, "integer", path, context, diagnostics, 0.0
		);
	}
	else if (integers.find(name) != integers.end())
	{
		ValidateGuiScalarType(
			field, "integer", path, context, diagnostics
		);
	}
	else if (normalizedNumbers.find(name) != normalizedNumbers.end())
	{
		ValidateGuiScalarType(
			field, "number", path, context, diagnostics, 0.0, 1.0
		);
	}
	else if (numbers.find(name) != numbers.end())
	{
		ValidateGuiScalarType(
			field, "number", path, context, diagnostics
		);
	}
	else if (name == "orientation")
	{
		static const GuiFieldNameSet orientations = {
			"upper_left", "upper_right", "lower_left", "lower_right",
			"center", "centre", "center_up", "center_down",
			"center_left", "center_right", "center_top",
			"center_bottom"
		};
		ValidateGuiEnum(
			field, orientations, path, context, diagnostics
		);
	}
	else if (name == "scalemode" || name == "scale" || name == "fit"
		|| name == "tooltipscalemode")
	{
		static const GuiFieldNameSet scales = {
			"stretch", "contain", "preserve", "preserveaspect",
			"aspect", "center", "none"
		};
		ValidateGuiEnum(field, scales, path, context, diagnostics);
	}
	else if (name == "alignment" || name == "textalignment"
		|| name == "align")
	{
		static const GuiFieldNameSet alignments = {
			"left", "center", "centre", "right"
		};
		ValidateGuiEnum(field, alignments, path, context, diagnostics);
	}
	else if (name == "layout" || name == "layoutmode"
		|| name == "itemlayout")
	{
		static const GuiFieldNameSet layouts = {
			"grid", "vertical", "horizontal", "polar", "radial",
			"semicircle"
		};
		ValidateGuiEnum(field, layouts, path, context, diagnostics);
	}
	else if (name == "dragaxis" || name == "dragorientation")
	{
		static const GuiFieldNameSet axes = {
			"horizontal", "vertical", "x", "y"
		};
		ValidateGuiEnum(field, axes, path, context, diagnostics);
	}
	else if (name == "animationmode")
	{
		static const GuiFieldNameSet modes = {
			"loop", "pingpong", "once"
		};
		ValidateGuiEnum(field, modes, path, context, diagnostics);
	}
	else
	{
		ValidateGuiFieldKind(
			field, ValueKind::Scalar, path, context, diagnostics
		);
	}
}

void ValidateStrictGuiWidget(
	const GuiObject& object,
	std::string_view typeName,
	const std::filesystem::path& path,
	std::vector<std::string>& diagnostics
);

void ValidateStrictGuiComponents(
	const GuiObject& object,
	const GuiFieldNameSet& allowed,
	const std::filesystem::path& path,
	std::string_view context,
	std::vector<std::string>& diagnostics
)
{
	for (const GuiField& field : object.fields)
	{
		if (field.name.empty())
		{
			continue;
		}
		if (allowed.find(ToLower(field.name)) == allowed.end())
		{
			AddUnknownGuiFieldDiagnostic(
				field,
				path,
				context,
				allowed,
				diagnostics
			);
		}
	}
}

enum class GuiComponentSchema
{
	Position,
	Size,
	PositivePair,
	NormalizedPair,
	Rgb,
	Rgba,
	Insets,
	IntegerList
};

void ValidateStrictGuiComponentValues(
	const GuiField& field,
	GuiComponentSchema schema,
	const std::filesystem::path& path,
	std::string_view context,
	std::vector<std::string>& diagnostics
)
{
	if (field.value.kind != ValueKind::Block || !field.value.block)
	{
		ValidateGuiFieldKind(
			field, ValueKind::Block, path, context, diagnostics
		);
		return;
	}
	const bool integerValues = schema == GuiComponentSchema::Position
		|| schema == GuiComponentSchema::Size
		|| schema == GuiComponentSchema::Insets
		|| schema == GuiComponentSchema::IntegerList;
	const bool normalizedValues = schema == GuiComponentSchema::Rgb
		|| schema == GuiComponentSchema::Rgba
		|| schema == GuiComponentSchema::NormalizedPair;
	const bool positiveValues = schema == GuiComponentSchema::PositivePair;
	std::size_t positionalCount = 0;
	GuiFieldNameSet namedComponents;
	for (const GuiField& component : field.value.block->fields)
	{
		if (component.value.kind != ValueKind::Scalar)
		{
			ValidateGuiFieldKind(
				component,
				ValueKind::Scalar,
				path,
				field.name,
				diagnostics
			);
			continue;
		}
		if (component.name.empty())
		{
			++positionalCount;
		}
		else
		{
			namedComponents.insert(ToLower(component.name));
		}
		if (integerValues)
		{
			const double minimum = schema == GuiComponentSchema::Size
				|| schema == GuiComponentSchema::Insets
				|| schema == GuiComponentSchema::IntegerList
				? 0.0
				: -std::numeric_limits<double>::infinity();
			ValidateGuiScalarType(
				component,
				"integer",
				path,
				field.name,
				diagnostics,
				minimum
			);
		}
		else
		{
			ValidateGuiScalarType(
				component,
				"number",
				path,
				field.name,
				diagnostics,
				positiveValues
					? std::numeric_limits<double>::min()
					: normalizedValues ? 0.0
					: -std::numeric_limits<double>::infinity(),
				positiveValues ? 100.0
					: normalizedValues ? 1.0
					: std::numeric_limits<double>::infinity()
			);
		}
	}
	if (schema == GuiComponentSchema::IntegerList)
	{
		return;
	}
	const bool hasNamed = !namedComponents.empty();
	std::size_t required = 0;
	if (schema == GuiComponentSchema::Position
		|| schema == GuiComponentSchema::Size
		|| schema == GuiComponentSchema::PositivePair
		|| schema == GuiComponentSchema::NormalizedPair)
	{
		required = 2;
		if (hasNamed
			&& (namedComponents.find("x") == namedComponents.end()
				|| namedComponents.find("y") == namedComponents.end()))
		{
			AddGuiSchemaDiagnostic(
				path,
				field.line,
				context,
				"field '" + field.name
					+ "' requires both x and y components",
				diagnostics
			);
		}
	}
	else if (schema == GuiComponentSchema::Rgb)
	{
		required = 3;
	}
	else if (schema == GuiComponentSchema::Rgba)
	{
		required = 4;
	}
	if (hasNamed && (schema == GuiComponentSchema::Rgb
		|| schema == GuiComponentSchema::Rgba))
	{
		const bool missingRgb = namedComponents.find("r") == namedComponents.end()
			|| namedComponents.find("g") == namedComponents.end()
			|| namedComponents.find("b") == namedComponents.end();
		const bool missingAlpha = schema == GuiComponentSchema::Rgba
			&& namedComponents.find("a") == namedComponents.end();
		if (missingRgb || missingAlpha)
		{
			AddGuiSchemaDiagnostic(
				path,
				field.line,
				context,
				"field '" + field.name + "' is missing required color components",
				diagnostics
			);
		}
	}
	if (!hasNamed && positionalCount < required)
	{
		AddGuiSchemaDiagnostic(
			path,
			field.line,
			context,
			"field '" + field.name + "' requires at least "
				+ std::to_string(required) + " positional components",
			diagnostics
		);
	}
	if (hasNamed && positionalCount > 0)
	{
		AddGuiSchemaDiagnostic(
			path,
			field.line,
			context,
			"field '" + field.name
				+ "' cannot mix named and positional components",
			diagnostics
		);
	}
}

void ValidateStrictGuiStyle(
	const GuiObject& object,
	const std::filesystem::path& path,
	std::vector<std::string>& diagnostics
)
{
	static const GuiFieldNameSet xy = {"x", "y"};
	static const GuiFieldNameSet rgb = {"r", "g", "b"};
	static const GuiFieldNameSet rgba = {"r", "g", "b", "a"};
	static const GuiFieldNameSet insets = {"left", "top", "right", "bottom"};
	for (const GuiField& field : object.fields)
	{
		const std::string name = ToLower(field.name);
		if (StyleDefaultFieldNames().find(name)
			== StyleDefaultFieldNames().end())
		{
			AddUnknownGuiFieldDiagnostic(
				field,
				path,
				"styleDefaults",
				StyleDefaultFieldNames(),
				diagnostics
			);
			continue;
		}
		if (name == "tooltipsize" || name == "tooltipoffset")
		{
			if (field.value.block)
			{
				ValidateStrictGuiComponents(
					*field.value.block, xy, path, field.name, diagnostics
				);
			}
			ValidateStrictGuiComponentValues(
				field,
				name == "tooltipsize"
					? GuiComponentSchema::Size
					: GuiComponentSchema::Position,
				path,
				"styleDefaults",
				diagnostics
			);
		}
		else if (name == "color" || name == "tooltiptextcolor")
		{
			if (field.value.block)
			{
				ValidateStrictGuiComponents(
					*field.value.block, rgb, path, field.name, diagnostics
				);
			}
			ValidateStrictGuiComponentValues(
				field, GuiComponentSchema::Rgb, path,
				"styleDefaults", diagnostics
			);
		}
		else if (name == "linecolor" || name == "tooltipcolor")
		{
			if (field.value.block)
			{
				ValidateStrictGuiComponents(
					*field.value.block, rgba, path, field.name, diagnostics
				);
			}
			ValidateStrictGuiComponentValues(
				field, GuiComponentSchema::Rgba, path,
				"styleDefaults", diagnostics
			);
		}
		else if (name == "tooltipnineslice"
			|| name == "tooltip_nine_slice")
		{
			if (field.value.block)
			{
				ValidateStrictGuiComponents(
					*field.value.block, insets, path, field.name, diagnostics
				);
			}
			ValidateStrictGuiComponentValues(
				field, GuiComponentSchema::Insets, path,
				"styleDefaults", diagnostics
			);
		}
		else if (name == "disabledbrightness"
			|| name == "disabledopacity")
		{
			ValidateGuiScalarType(
				field, "number", path, "styleDefaults",
				diagnostics, 0.0, 1.0
			);
		}
		else if (name == "localizetooltip"
			|| name == "localisetooltip" || name == "tooltipwrap")
		{
			ValidateGuiScalarType(
				field, "boolean", path, "styleDefaults", diagnostics
			);
		}
		else if (name == "framezorder")
		{
			ValidateGuiScalarType(
				field, "integer", path, "styleDefaults", diagnostics
			);
		}
		else if (name == "fontsize" || name == "linespacing"
			|| name == "linewidth" || name == "tooltippadding"
			|| name == "tooltipsearchstep" || name == "tooltipfontsize"
			|| name == "tooltiplinespacing" || name == "tooltipdelay"
			|| name == "minimumthumbsize")
		{
			ValidateGuiScalarType(
				field, "integer", path, "styleDefaults",
				diagnostics, 0.0
			);
		}
		else
		{
			ValidateGuiFieldKind(
				field, ValueKind::Scalar, path,
				"styleDefaults", diagnostics
			);
		}
	}
	ValidateGuiMutuallyExclusive(
		object,
		{"localizeTooltip", "localiseTooltip"},
		path,
		"styleDefaults",
		diagnostics
	);
}

void ValidateStrictGuiStaticData(
	const GuiObject& object,
	std::string_view typeName,
	const std::filesystem::path& path,
	std::vector<std::string>& diagnostics
)
{
	static const GuiFieldNameSet valueFields = {"name", "value", "type"};
	static const GuiFieldNameSet listFields = {"name", "revision", "item"};
	const bool valueType = ToLower(std::string(typeName)) == "datavaluetype";
	const GuiFieldNameSet& allowed = valueType ? valueFields : listFields;
	for (const GuiField& field : object.fields)
	{
		const std::string name = ToLower(field.name);
		if (allowed.find(name) == allowed.end())
		{
			AddUnknownGuiFieldDiagnostic(
				field,
				path,
				typeName,
				allowed,
				diagnostics
			);
			continue;
		}
		if (name == "item")
		{
			ValidateGuiFieldKind(
				field, ValueKind::Block, path, typeName, diagnostics
			);
			continue;
		}
		ValidateGuiFieldKind(
			field, ValueKind::Scalar, path, typeName, diagnostics
		);
		if (name == "revision")
		{
			ValidateGuiScalarType(
				field, "integer", path, typeName,
				diagnostics, 0.0
			);
		}
		else if (name == "type")
		{
			static const GuiFieldNameSet valueTypes = {
				"string", "text", "bool", "boolean", "int",
				"integer", "number", "float", "double"
			};
			ValidateGuiEnum(
				field, valueTypes, path, typeName, diagnostics
			);
		}
		// item 内部字段是声明式业务目录，名称必须保持开放。
	}
}

void ValidateStrictGuiWidget(
	const GuiObject& object,
	std::string_view typeName,
	const std::filesystem::path& path,
	std::vector<std::string>& diagnostics
)
{
	static const GuiFieldNameSet xy = {"x", "y"};
	static const GuiFieldNameSet rgb = {"r", "g", "b"};
	static const GuiFieldNameSet rgba = {"r", "g", "b", "a"};
	static const GuiFieldNameSet insets = {"left", "top", "right", "bottom"};
	const std::string context = GuiObjectContext(typeName, object);
	const WidgetType widgetType = GetWidgetType(std::string(typeName));
	for (const GuiField& field : object.fields)
	{
		const std::string name = ToLower(field.name);
		if (field.value.kind == ValueKind::Block && field.value.block)
		{
			if (WidgetTypeNames().find(name) != WidgetTypeNames().end())
			{
				ValidateStrictGuiWidget(
					*field.value.block,
					field.name,
					path,
					diagnostics
				);
				continue;
			}
			if (name == "datavaluetype" || name == "datalisttype")
			{
				ValidateStrictGuiStaticData(
					*field.value.block,
					field.name,
					path,
					diagnostics
				);
				continue;
			}
			if (name == "styledefaults")
			{
				ValidateStrictGuiStyle(*field.value.block, path, diagnostics);
				continue;
			}
		}
		if (WidgetFieldNames().find(name) == WidgetFieldNames().end())
		{
			AddUnknownGuiFieldDiagnostic(
				field,
				path,
				context,
				WidgetFieldNames(),
				diagnostics
			);
			continue;
		}
		if (!IsGuiWidgetFieldApplicable(name, widgetType))
		{
			AddGuiSchemaDiagnostic(
				path,
				field.line,
				context,
				"field '" + field.name + "' is not applicable to "
					+ std::string(typeName),
				diagnostics
			);
		}
		if (field.value.kind != ValueKind::Block || !field.value.block)
		{
			ValidateStrictGuiWidgetScalar(
				field, path, context, diagnostics
			);
			continue;
		}
		if (name == "position" || name == "size" || name == "markersize"
			|| name == "portraitposition" || name == "portraitsize"
			|| name == "tooltipsize" || name == "tooltipoffset"
			|| name == "markeractionposition" || name == "markeractionsize"
			|| name == "polarcenter" || name == "pivot"
			|| name == "transformscale")
		{
			ValidateStrictGuiComponents(
				*field.value.block, xy, path, field.name, diagnostics
			);
			ValidateStrictGuiComponentValues(
				field,
				name == "pivot"
					? GuiComponentSchema::NormalizedPair
					: name == "transformscale"
						? GuiComponentSchema::PositivePair
						: name == "position" || name == "portraitposition"
					|| name == "tooltipoffset"
					|| name == "markeractionposition"
					|| name == "polarcenter"
					? GuiComponentSchema::Position
						: GuiComponentSchema::Size,
				path,
				context,
				diagnostics
			);
		}
		else if (name == "color" || name == "tooltiptextcolor")
		{
			ValidateStrictGuiComponents(
				*field.value.block, rgb, path, field.name, diagnostics
			);
			ValidateStrictGuiComponentValues(
				field, GuiComponentSchema::Rgb,
				path, context, diagnostics
			);
		}
		else if (name == "linecolor" || name == "tooltipcolor")
		{
			ValidateStrictGuiComponents(
				*field.value.block, rgba, path, field.name, diagnostics
			);
			ValidateStrictGuiComponentValues(
				field, GuiComponentSchema::Rgba,
				path, context, diagnostics
			);
		}
		else if (name == "nineslice" || name == "nine_slice"
			|| name == "tooltipnineslice"
			|| name == "tooltip_nine_slice")
		{
			ValidateStrictGuiComponents(
				*field.value.block, insets, path, field.name, diagnostics
			);
			ValidateStrictGuiComponentValues(
				field, GuiComponentSchema::Insets,
				path, context, diagnostics
			);
		}
		else if (name == "polarringitemcounts")
		{
			ValidateStrictGuiComponentValues(
				field, GuiComponentSchema::IntegerList,
				path, context, diagnostics
			);
		}
		else
		{
			ValidateGuiFieldKind(
				field, ValueKind::Scalar, path, context, diagnostics
			);
		}
	}
	ValidateGuiRequiredField(
		object, {"name"}, ValueKind::Scalar,
		"name", path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object, {"textSource", "textBinding", "textValue"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object,
		{"localizationKey", "localisationKey", "textKey"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object, {"text", "textSource", "textBinding", "textValue",
			"localizationKey", "localisationKey", "textKey"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object, {"positionType", "position_type"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object, {"visible", "dontRender"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object, {"enabled", "disabled"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object, {"value", "valueSource", "valueBinding", "progressSource"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object, {"frameSource", "frameBinding"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object,
		{"animationFrameTime", "animationFrameDuration", "animationFps"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object, {"effectType", "effectResource"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object, {"scaleSource", "transformScaleSource"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object, {"scaleXSource", "transformScaleXSource"},
		path, context, diagnostics
	);
	ValidateGuiMutuallyExclusive(
		object, {"scaleYSource", "transformScaleYSource"},
		path, context, diagnostics
	);
	if (widgetType == WidgetType::Window)
	{
		const GuiField* fullScreen = FindGuiField(object, {"fullScreen"});
		const GuiField* moveable = FindGuiField(object, {"moveable"});
		bool fullScreenValue = false;
		bool moveableValue = false;
		if (!fullScreen
			|| fullScreen->value.kind != ValueKind::Scalar
			|| !ParseGuiBoolean(fullScreen->value.scalar, fullScreenValue)
			|| !fullScreenValue)
		{
			ValidateGuiRequiredField(
				object, {"size"}, ValueKind::Block,
				"size", path, context, diagnostics
			);
		}
		if (fullScreen && moveable
			&& fullScreen->value.kind == ValueKind::Scalar
			&& moveable->value.kind == ValueKind::Scalar
			&& ParseGuiBoolean(fullScreen->value.scalar, fullScreenValue)
			&& ParseGuiBoolean(moveable->value.scalar, moveableValue)
			&& fullScreenValue && moveableValue)
		{
			AddGuiSchemaDiagnostic(
				path,
				moveable->line,
				context,
				"fullScreen=yes and moveable=yes are mutually exclusive",
				diagnostics
			);
		}
	}
	else if (widgetType == WidgetType::ListBox)
	{
		ValidateGuiRequiredField(
			object, {"itemTemplate"}, ValueKind::Scalar,
			"itemTemplate", path, context, diagnostics
		);
	}
	else if (widgetType == WidgetType::ProgressBar)
	{
		ValidateGuiRequiredField(
			object, {"progressBar", "progressResource", "progressType"},
			ValueKind::Scalar, "progressBar", path, context, diagnostics
		);
	}
	else if (widgetType == WidgetType::ScrollBar)
	{
		ValidateGuiRequiredField(
			object, {"slider"}, ValueKind::Scalar,
			"slider", path, context, diagnostics
		);
		ValidateGuiRequiredField(
			object, {"track"}, ValueKind::Scalar,
			"track", path, context, diagnostics
		);
	}
	else if (widgetType == WidgetType::IndexedMap)
	{
		ValidateGuiRequiredField(
			object, {"mapResource", "indexedMap", "indexedMapResource", "resource"},
			ValueKind::Scalar, "mapResource", path, context, diagnostics
		);
	}
	else if (widgetType == WidgetType::Custom)
	{
		ValidateGuiRequiredField(
			object, {"customType", "type"}, ValueKind::Scalar,
			"customType", path, context, diagnostics
		);
	}
}

void ValidateStrictGuiResource(
	const GuiObject& object,
	std::string_view typeName,
	const std::filesystem::path& path,
	std::vector<std::string>& diagnostics
)
{
	static const GuiFieldNameSet xy = {"x", "y"};
	static const GuiFieldNameSet rgb = {"r", "g", "b"};
	static const GuiFieldNameSet rgba = {"r", "g", "b", "a"};
	static const GuiFieldNameSet sourceItemFields = {"id", "name"};
	static const GuiFieldNameSet colorStopFields = {
		"minimum", "threshold", "color"
	};
	const std::string normalizedType = ToLower(std::string(typeName));
	const GuiFieldNameSet& allowed = normalizedType == "spritetype"
		? SpriteFieldNames()
		: normalizedType == "progressbartype"
			? ProgressResourceFieldNames()
			: normalizedType == "effecttype"
				? EffectResourceFieldNames()
				: IndexedMapResourceFieldNames();
	const std::string context = GuiObjectContext(typeName, object);
	for (const GuiField& field : object.fields)
	{
		const std::string name = ToLower(field.name);
		if (allowed.find(name) == allowed.end())
		{
			AddUnknownGuiFieldDiagnostic(
				field, path, context, allowed, diagnostics
			);
			continue;
		}
		if (normalizedType == "spritetype" && name == "animation")
		{
			if (field.value.kind == ValueKind::Scalar)
			{
				ValidateGuiScalarType(
					field, "boolean", path, context, diagnostics
				);
				continue;
			}
			if (!field.value.block)
			{
				ValidateGuiFieldKind(
					field, ValueKind::Block, path, context, diagnostics
				);
				continue;
			}
			ValidateStrictGuiComponents(
				*field.value.block,
				SpriteAnimationFieldNames(),
				path,
				"animation",
				diagnostics
			);
			for (const GuiField& animationField
				: field.value.block->fields)
			{
				const std::string animationName = ToLower(
					animationField.name
				);
				if (animationName == "enabled")
				{
					ValidateGuiScalarType(
						animationField, "boolean", path,
						"animation", diagnostics
					);
				}
				else if (animationName == "mode")
				{
					static const GuiFieldNameSet modes = {
						"loop", "pingpong", "once"
					};
					ValidateGuiEnum(
						animationField, modes, path,
						"animation", diagnostics
					);
				}
				else if (animationName == "fps")
				{
					ValidateGuiScalarType(
						animationField, "number", path,
						"animation", diagnostics,
						std::numeric_limits<double>::min()
					);
				}
				else if (animationName == "offset")
				{
					ValidateGuiScalarType(
						animationField, "integer", path,
						"animation", diagnostics
					);
				}
				else if (animationName == "frametime"
					|| animationName == "frameduration"
					|| animationName == "startframe"
					|| animationName == "endframe")
				{
					ValidateGuiScalarType(
						animationField, "integer", path,
						"animation", diagnostics, 0.0
					);
				}
			}
			ValidateGuiMutuallyExclusive(
				*field.value.block,
				{"frameTime", "frameDuration", "fps"},
				path,
				"animation",
				diagnostics
			);
			continue;
		}
		if (field.value.kind != ValueKind::Block || !field.value.block)
		{
			if (name == "noofframes")
			{
				ValidateGuiScalarType(
					field, "integer", path, context,
					diagnostics, 1.0
				);
			}
			else if (name == "norefcount" || name == "horizontal"
				|| name == "flipvertical" || name == "drawboundaries"
				|| name == "enabled")
			{
				ValidateGuiScalarType(
					field, "boolean", path, context, diagnostics
				);
			}
			else if (name == "width" || name == "height"
				|| name == "boundarywidth" || name == "croppadding")
			{
				ValidateGuiScalarType(
					field, "integer", path, context,
					diagnostics, 0.0
				);
			}
			else if (name == "framelayout")
			{
				static const GuiFieldNameSet layouts = {
					"horizontal", "vertical"
				};
				ValidateGuiEnum(
					field, layouts, path, context, diagnostics
				);
			}
			else if (normalizedType == "effecttype" && name == "effect")
			{
				static const GuiFieldNameSet effects = {
					"tint", "pulse", "brightness_pulse",
					"opacity_pulse", "color_pulse"
				};
				ValidateGuiEnum(
					field, effects, path, context, diagnostics
				);
			}
			else if (normalizedType == "effecttype"
				&& (name == "minimum" || name == "maximum"))
			{
				ValidateGuiScalarType(
					field, "number", path, context, diagnostics, 0.0, 1.0
				);
			}
			else if (normalizedType == "effecttype" && name == "speed")
			{
				ValidateGuiScalarType(
					field, "number", path, context, diagnostics, 0.0, 100.0
				);
			}
			else if (normalizedType == "effecttype" && name == "phase")
			{
				ValidateGuiScalarType(
					field, "number", path, context, diagnostics
				);
			}
			else
			{
				ValidateGuiFieldKind(
					field, ValueKind::Scalar, path, context, diagnostics
				);
			}
			continue;
		}
		if (name == "size")
		{
			ValidateStrictGuiComponents(
				*field.value.block, xy, path, field.name, diagnostics
			);
			ValidateStrictGuiComponentValues(
				field, GuiComponentSchema::Size,
				path, context, diagnostics
			);
		}
		else if (name == "color" || name == "colortwo")
		{
			ValidateStrictGuiComponents(
				*field.value.block, rgb, path, field.name, diagnostics
			);
			ValidateStrictGuiComponentValues(
				field, GuiComponentSchema::Rgb,
				path, context, diagnostics
			);
		}
		else if (normalizedType == "effecttype" && name == "color")
		{
			ValidateStrictGuiComponents(
				*field.value.block, rgba, path, field.name, diagnostics
			);
			ValidateStrictGuiComponentValues(
				field, GuiComponentSchema::Rgba,
				path, context, diagnostics
			);
		}
		else if (name == "sourcefillcolor"
			|| name == "sourceboundarycolor" || name == "boundarycolor"
			|| name == "hovercolor")
		{
			ValidateStrictGuiComponents(
				*field.value.block, rgba, path, field.name, diagnostics
			);
			ValidateStrictGuiComponentValues(
				field, GuiComponentSchema::Rgba,
				path, context, diagnostics
			);
		}
		else if (name == "sourceitem")
		{
			ValidateStrictGuiComponents(
				*field.value.block,
				sourceItemFields,
				path,
				field.name,
				diagnostics
			);
			ValidateGuiRequiredField(
				*field.value.block, {"id"}, ValueKind::Scalar,
				"id", path, "sourceItem", diagnostics
			);
			ValidateGuiRequiredField(
				*field.value.block, {"name"}, ValueKind::Scalar,
				"name", path, "sourceItem", diagnostics
			);
			if (const GuiField* id = FindGuiField(
					*field.value.block, {"id"}))
			{
				ValidateGuiScalarType(
					*id, "integer", path, "sourceItem",
					diagnostics, 1.0, 65535.0
				);
			}
		}
		else if (name == "colorstop")
		{
			for (const GuiField& stopField : field.value.block->fields)
			{
				const std::string stopName = ToLower(stopField.name);
				if (colorStopFields.find(stopName) == colorStopFields.end())
				{
					AddUnknownGuiFieldDiagnostic(
						stopField,
						path,
						"colorStop",
						colorStopFields,
						diagnostics
					);
				}
				else if (stopName == "color"
					&& stopField.value.kind == ValueKind::Block
					&& stopField.value.block)
				{
					ValidateStrictGuiComponents(
						*stopField.value.block,
						rgba,
						path,
						"colorStop.color",
						diagnostics
					);
					ValidateStrictGuiComponentValues(
						stopField, GuiComponentSchema::Rgba,
						path, "colorStop", diagnostics
					);
				}
				else if (stopName == "minimum" || stopName == "threshold")
				{
					ValidateGuiScalarType(
						stopField, "number", path,
						"colorStop", diagnostics
					);
				}
			}
			ValidateGuiRequiredField(
				*field.value.block, {"minimum", "threshold"},
				ValueKind::Scalar, "minimum", path,
				"colorStop", diagnostics
			);
			ValidateGuiRequiredField(
				*field.value.block, {"color"}, ValueKind::Block,
				"color", path, "colorStop", diagnostics
			);
		}
		else
		{
			ValidateGuiFieldKind(
				field, ValueKind::Scalar, path, context, diagnostics
			);
		}
	}
	ValidateGuiRequiredField(
		object, {"name"}, ValueKind::Scalar,
		"name", path, context, diagnostics
	);
	if (normalizedType == "spritetype")
	{
		ValidateGuiRequiredField(
			object, {"texturefile"}, ValueKind::Scalar,
			"texturefile", path, context, diagnostics
		);
	}
	else if (normalizedType == "progressbartype")
	{
		const bool hasSize = FindGuiField(object, {"size"}) != nullptr;
		const bool hasWidth = FindGuiField(object, {"width"}) != nullptr;
		const bool hasHeight = FindGuiField(object, {"height"}) != nullptr;
		if (!hasSize && (!hasWidth || !hasHeight))
		{
			AddGuiSchemaDiagnostic(
				path, 0, context,
				"requires either size or both width and height",
				diagnostics
			);
		}
		if (hasSize && (hasWidth || hasHeight))
		{
			AddGuiSchemaDiagnostic(
				path, 0, context,
				"size is mutually exclusive with width and height",
				diagnostics
			);
		}
	}
	else if (normalizedType == "effecttype")
	{
		ValidateGuiRequiredField(
			object, {"effect"}, ValueKind::Scalar,
			"effect", path, context, diagnostics
		);
		const float minimum = GetFloat(object, "minimum", 0.5f);
		const float maximum = GetFloat(object, "maximum", 1.0f);
		if (minimum > maximum)
		{
			AddGuiSchemaDiagnostic(
				path, 0, context,
				"minimum must not be greater than maximum",
				diagnostics
			);
		}
	}
	else
	{
		ValidateGuiRequiredField(
			object, {"texturefile"}, ValueKind::Scalar,
			"texturefile", path, context, diagnostics
		);
		ValidateGuiRequiredField(
			object, {"indexfile"}, ValueKind::Scalar,
			"indexfile", path, context, diagnostics
		);
	}
}

void ValidateStrictGuiContainer(
	const GuiObject& object,
	bool resourceContext,
	const std::filesystem::path& path,
	std::vector<std::string>& diagnostics
)
{
	static const GuiFieldNameSet positionFields = {"name", "position"};
	for (const GuiField& field : object.fields)
	{
		const std::string name = ToLower(field.name);
		if (field.value.kind != ValueKind::Block || !field.value.block)
		{
			if (ContainerFieldNames().find(name)
				!= ContainerFieldNames().end())
			{
				ValidateGuiFieldKind(
					field,
					ValueKind::Block,
					path,
					resourceContext ? "GFX root" : "GUI root",
					diagnostics
				);
			}
			else
			{
				AddUnknownGuiFieldDiagnostic(
					field,
					path,
					resourceContext ? "GFX root" : "GUI root",
					ContainerFieldNames(),
					diagnostics
				);
			}
			continue;
		}
		if (name == "guitypes" || name == "spritetypes")
		{
			ValidateStrictGuiContainer(
				*field.value.block,
				name == "spritetypes",
				path,
				diagnostics
			);
			continue;
		}
		if (name == "positiontype")
		{
			for (const GuiField& positionField : field.value.block->fields)
			{
				const std::string positionName = ToLower(positionField.name);
				if (positionFields.find(positionName) == positionFields.end())
				{
					AddUnknownGuiFieldDiagnostic(
						positionField,
						path,
						"positionType",
						positionFields,
						diagnostics
					);
				}
			}
			const std::string positionContext = GuiObjectContext(
				"positionType",
				*field.value.block
			);
			ValidateGuiRequiredField(
				*field.value.block,
				{"name"},
				ValueKind::Scalar,
				"name",
				path,
				positionContext,
				diagnostics
			);
			ValidateGuiRequiredField(
				*field.value.block,
				{"position"},
				ValueKind::Block,
				"position",
				path,
				positionContext,
				diagnostics
			);
			if (const GuiField* position = FindGuiField(
					*field.value.block, {"position"}))
			{
				static const GuiFieldNameSet xy = {"x", "y"};
				if (position->value.block)
				{
					ValidateStrictGuiComponents(
						*position->value.block,
						xy,
						path,
						"positionType.position",
						diagnostics
					);
				}
				ValidateStrictGuiComponentValues(
					*position,
					GuiComponentSchema::Position,
					path,
					positionContext,
					diagnostics
				);
			}
			continue;
		}
		if (name == "spritetype" || name == "indexedmapresourcetype"
			|| name == "effecttype"
			|| (resourceContext && name == "progressbartype"))
		{
			ValidateStrictGuiResource(
				*field.value.block,
				field.name,
				path,
				diagnostics
			);
			continue;
		}
		if (WidgetTypeNames().find(name) != WidgetTypeNames().end())
		{
			ValidateStrictGuiWidget(
				*field.value.block,
				field.name,
				path,
				diagnostics
			);
			continue;
		}
		AddUnknownGuiFieldDiagnostic(
			field,
			path,
			resourceContext ? "GFX root" : "GUI root",
			ContainerFieldNames(),
			diagnostics
		);
	}
}

void ValidateStrictGuiDocument(
	const GuiDocument& document,
	bool strictLegacyFiles,
	std::vector<std::string>& diagnostics
)
{
	const std::string extension = ToLower(document.path.extension().string());
	const bool scripted = extension == ".sgui" || extension == ".sgfx";
	const bool legacy = extension == ".gui" || extension == ".gfx";
	if (!scripted && !(strictLegacyFiles && legacy))
	{
		return;
	}
	ValidateStrictGuiContainer(
		document.root,
		extension == ".sgfx" || extension == ".gfx",
		document.path,
		diagnostics
	);
}

struct WidgetStyleDefaults
{
	std::optional<std::array<float, 3>> textColor;
	std::optional<std::array<float, 3>> tooltipTextColor;
	std::optional<GuiRgbaColor> lineColor;
	std::optional<GuiRgbaColor> tooltipColor;
	std::optional<GuiRect> tooltipRect;
	std::optional<GuiNineSliceInsets> tooltipNineSlice;
	std::optional<std::string> tooltipSpriteName;
	std::optional<std::string> tooltipFont;
	std::optional<std::string> tooltipScaleMode;
	std::optional<std::string> tooltipPlacement;
	std::optional<int> frameZOrder;
	std::optional<int> fontSize;
	std::optional<int> lineSpacing;
	std::optional<int> lineWidth;
	std::optional<int> tooltipPadding;
	std::optional<int> tooltipSearchStep;
	std::optional<int> tooltipFontSize;
	std::optional<int> tooltipLineSpacing;
	std::optional<int> tooltipDelayMilliseconds;
	std::optional<int> minimumThumbSize;
	std::optional<float> disabledBrightness;
	std::optional<float> disabledOpacity;
	std::optional<bool> localizeTooltip;
	std::optional<bool> tooltipWrap;
};

void MergeStyleDefaults(
	const GuiObject& object,
	WidgetStyleDefaults& style
)
{
	if (const GuiValue* value = FindValue(object, "color");
		value && value->kind == ValueKind::Block)
	{
		std::array<float, 3> color = style.textColor.value_or(
			std::array<float, 3>{1.0f, 1.0f, 1.0f}
		);
		ReadColor(object, "color", color.data());
		style.textColor = color;
	}
	if (const GuiValue* value = FindValue(object, "tooltipTextColor");
		value && value->kind == ValueKind::Block)
	{
		std::array<float, 3> color = style.tooltipTextColor.value_or(
			std::array<float, 3>{1.0f, 1.0f, 1.0f}
		);
		ReadColor(object, "tooltipTextColor", color.data());
		style.tooltipTextColor = color;
	}
	if (const GuiValue* value = FindValue(object, "lineColor");
		value && value->kind == ValueKind::Block)
	{
		GuiRgbaColor color = style.lineColor.value_or(GuiRgbaColor{});
		ReadRgbaColor(object, "lineColor", color);
		style.lineColor = color;
	}
	if (const GuiValue* value = FindValue(object, "tooltipColor");
		value && value->kind == ValueKind::Block)
	{
		GuiRgbaColor color = style.tooltipColor.value_or(GuiRgbaColor{});
		ReadRgbaColor(object, "tooltipColor", color);
		style.tooltipColor = color;
	}
	if (const GuiValue* value = FindValue(object, "tooltipSize");
		value && value->kind == ValueKind::Block)
	{
		GuiRect rect = style.tooltipRect.value_or(GuiRect{});
		ReadSize(object, "tooltipSize", rect.width, rect.height);
		style.tooltipRect = rect;
	}
	if (const GuiValue* value = FindValue(object, "tooltipOffset");
		value && value->kind == ValueKind::Block)
	{
		GuiRect rect = style.tooltipRect.value_or(GuiRect{});
		ReadSize(object, "tooltipOffset", rect.x, rect.y);
		style.tooltipRect = rect;
	}
	if (FindValue(object, "tooltipNineSlice")
		|| FindValue(object, "tooltip_nine_slice"))
	{
		GuiNineSliceInsets insets = style.tooltipNineSlice.value_or(
			GuiNineSliceInsets{}
		);
		ReadNineSlice(
			object,
			insets,
			"tooltipNineSlice",
			"tooltip_nine_slice"
		);
		style.tooltipNineSlice = insets;
	}
	const auto readString = [&object](
		const char* name,
		std::optional<std::string>& output)
	{
		const std::string value = GetScalar(object, name);
		if (!value.empty()) output = value;
	};
	const std::string tooltipSprite = GetFirstScalar(
		object,
		{"tooltipSprite", "tooltipBackgroundSprite"}
	);
	if (!tooltipSprite.empty())
	{
		style.tooltipSpriteName = tooltipSprite;
	}
	readString("tooltipFont", style.tooltipFont);
	readString("tooltipScaleMode", style.tooltipScaleMode);
	readString("tooltipPlacement", style.tooltipPlacement);
	const auto readInteger = [&object](
		const char* name,
		std::optional<int>& output)
	{
		if (!GetScalar(object, name).empty())
		{
			output = GetInteger(object, name, output.value_or(0));
		}
	};
	readInteger("frameZOrder", style.frameZOrder);
	readInteger("fontSize", style.fontSize);
	readInteger("lineSpacing", style.lineSpacing);
	readInteger("lineWidth", style.lineWidth);
	readInteger("tooltipPadding", style.tooltipPadding);
	readInteger("tooltipSearchStep", style.tooltipSearchStep);
	readInteger("tooltipFontSize", style.tooltipFontSize);
	readInteger("tooltipLineSpacing", style.tooltipLineSpacing);
	readInteger("tooltipDelay", style.tooltipDelayMilliseconds);
	readInteger("minimumThumbSize", style.minimumThumbSize);
	if (!GetScalar(object, "disabledBrightness").empty())
	{
		style.disabledBrightness = std::clamp(
			GetFloat(object, "disabledBrightness", 1.0f),
			0.0f,
			1.0f
		);
	}
	if (!GetScalar(object, "disabledOpacity").empty())
	{
		style.disabledOpacity = std::clamp(
			GetFloat(object, "disabledOpacity", 1.0f),
			0.0f,
			1.0f
		);
	}
	if (!GetScalar(object, "localizeTooltip").empty()
		|| !GetScalar(object, "localiseTooltip").empty())
	{
		style.localizeTooltip = GetBoolean(
			object,
			"localizeTooltip",
			GetBoolean(object, "localiseTooltip", false)
		);
	}
	if (!GetScalar(object, "tooltipWrap").empty())
	{
		style.tooltipWrap = GetBoolean(
			object,
			"tooltipWrap",
			true
		);
	}
}

void ApplyStyleDefaults(
	WidgetDefinition& widget,
	const WidgetStyleDefaults& style
)
{
	if (style.textColor)
	{
		std::copy(
			style.textColor->begin(),
			style.textColor->end(),
			widget.textColor
		);
	}
	if (style.lineColor) widget.lineColor = *style.lineColor;
	if (style.tooltipColor) widget.tooltipColor = *style.tooltipColor;
	if (style.tooltipTextColor)
	{
		std::copy(
			style.tooltipTextColor->begin(),
			style.tooltipTextColor->end(),
			widget.tooltipTextColor
		);
	}
	if (style.tooltipRect) widget.tooltipRect = *style.tooltipRect;
	if (style.tooltipNineSlice)
	{
		widget.tooltipNineSlice = *style.tooltipNineSlice;
	}
	if (style.tooltipSpriteName)
	{
		widget.tooltipSpriteName = *style.tooltipSpriteName;
	}
	if (style.tooltipFont) widget.tooltipFont = *style.tooltipFont;
	if (style.tooltipScaleMode)
	{
		widget.tooltipScaleMode = *style.tooltipScaleMode;
	}
	if (style.tooltipPlacement)
	{
		widget.tooltipPlacement = *style.tooltipPlacement;
	}
	if (style.frameZOrder) widget.frameZOrder = *style.frameZOrder;
	if (style.fontSize) widget.fontSize = *style.fontSize;
	if (style.lineSpacing) widget.lineSpacing = *style.lineSpacing;
	if (style.lineWidth) widget.lineWidth = std::max(0, *style.lineWidth);
	if (style.tooltipPadding)
	{
		widget.tooltipPadding = std::max(0, *style.tooltipPadding);
	}
	if (style.tooltipSearchStep)
	{
		widget.tooltipSearchStep = std::max(
			0,
			*style.tooltipSearchStep
		);
	}
	if (style.tooltipFontSize)
	{
		widget.tooltipFontSize = std::max(0, *style.tooltipFontSize);
	}
	if (style.tooltipLineSpacing)
	{
		widget.tooltipLineSpacing = std::max(
			0,
			*style.tooltipLineSpacing
		);
	}
	if (style.tooltipDelayMilliseconds)
	{
		widget.tooltipDelayMilliseconds = std::max(
			0,
			*style.tooltipDelayMilliseconds
		);
	}
	if (style.minimumThumbSize)
	{
		widget.minimumThumbSize = std::max(0, *style.minimumThumbSize);
	}
	if (style.disabledBrightness)
	{
		widget.disabledBrightness = *style.disabledBrightness;
	}
	if (style.disabledOpacity)
	{
		widget.disabledOpacity = *style.disabledOpacity;
	}
	if (style.localizeTooltip)
	{
		widget.localizeTooltip = *style.localizeTooltip;
	}
	if (style.tooltipWrap) widget.tooltipWrap = *style.tooltipWrap;
}

WidgetDefinition BuildWidgetDefinition(
	const std::string& typeName,
	const GuiObject& object,
	const WidgetStyleDefaults& inheritedStyle
)
{
	WidgetDefinition widget;
	ApplyStyleDefaults(widget, inheritedStyle);
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
	widget.fullScreenSpecified = FindValue(object, "fullScreen") != nullptr;
	widget.fullScreen = GetBoolean(object, "fullScreen", false);
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
	widget.frame = std::max(1, GetInteger(object, "frame", 1));
	widget.frameSource = GetFirstScalar(
		object,
		{"frameSource", "frameBinding"}
	);
	widget.transform.rotationDegrees = GetFloat(object, "rotation", 0.0f);
	widget.rotationSource = GetScalar(object, "rotationSource");
	ReadFloatPair(
		object,
		"pivot",
		widget.transform.pivotX,
		widget.transform.pivotY
	);
	ReadFloatPair(
		object,
		"transformScale",
		widget.transform.scaleX,
		widget.transform.scaleY
	);
	widget.transformScaleSource = GetFirstScalar(
		object,
		{"scaleSource", "transformScaleSource"}
	);
	widget.transformScaleXSource = GetFirstScalar(
		object,
		{"scaleXSource", "transformScaleXSource"}
	);
	widget.transformScaleYSource = GetFirstScalar(
		object,
		{"scaleYSource", "transformScaleYSource"}
	);
	widget.transform.flipX = GetBoolean(object, "flipX", false);
	widget.transform.flipY = GetBoolean(object, "flipY", false);
	widget.animateSpecified = FindValue(object, "animate") != nullptr;
	widget.animate = GetBoolean(object, "animate", false);
	widget.animationMode = GetScalar(object, "animationMode");
	widget.animationTimeSource = GetScalar(
		object,
		"animationTimeSource"
	);
	widget.animationFrameTimeMilliseconds = std::max(
		0,
		GetInteger(
			object,
			"animationFrameTime",
			GetInteger(object, "animationFrameDuration", 0)
		)
	);
	if (widget.animationFrameTimeMilliseconds == 0)
	{
		const float framesPerSecond = GetFloat(
			object,
			"animationFps",
			0.0f
		);
		if (framesPerSecond > 0.0f)
		{
			widget.animationFrameTimeMilliseconds = std::max(
				1,
				static_cast<int>(std::lround(1000.0f / framesPerSecond))
			);
		}
	}
	widget.animationStartFrame = std::max(
		0,
		GetInteger(object, "animationStartFrame", 0)
	);
	widget.animationEndFrame = std::max(
		0,
		GetInteger(object, "animationEndFrame", 0)
	);
	widget.animationOffsetMilliseconds = GetInteger(
		object,
		"animationOffset",
		0
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
	widget.itemFilterField = GetFirstScalar(
		object,
		{"itemFilterField", "filterField"}
	);
	widget.itemFilterValueSource = GetFirstScalar(
		object,
		{"itemFilterValueSource", "filterValueSource"}
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
	widget.effectResourceName = GetFirstScalar(
		object,
		{"effectType", "effectResource"}
	);
	widget.effectSource = GetScalar(object, "effectSource");
	widget.effectTimeSource = GetScalar(object, "effectTimeSource");
	widget.dataSource = GetFirstScalar(
		object,
		{"dataSource", "listSource", "itemsSource"}
	);
	widget.catalogSource = GetFirstScalar(
		object,
		{"catalogSource", "itemCatalog", "fallbackDataSource"}
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
		{"descriptionSource", "markerDescriptionSource"}
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
	/*
		scaleMode:

		stretch
		contain
		preserve
		preserveAspect
		aspect
		center
		none

		Windows D3D9 renderer 会在 DrawSprite() 中解释这些值。
	*/
	widget.scaleMode = GetFirstScalar(
		object,
		{"scaleMode", "scale", "fit"}
	);
	ReadNineSlice(
    object,
    widget.nineSlice
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
	widget.tooltipSource = GetFirstScalar(
		object,
		{"tooltipSource", "tooltipBinding", "tooltipValue"}
	);
	widget.tooltipLocalizationKey = GetFirstScalar(
		object,
		{"tooltipLocalizationKey", "tooltipTextKey"}
	);
	widget.tooltipSpriteName = GetFirstScalar(
		object,
		{"tooltipSprite", "tooltipBackgroundSprite"},
		widget.tooltipSpriteName
	);
	widget.tooltipFont = GetFirstScalar(
		object,
		{"tooltipFont"},
		widget.tooltipFont
	);
	widget.tooltipScaleMode = GetFirstScalar(
		object,
		{"tooltipScaleMode"},
		widget.tooltipScaleMode
	);
	widget.tooltipPlacement = GetFirstScalar(
		object,
		{"tooltipPlacement", "tooltipSide"},
		widget.tooltipPlacement
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
		GetInteger(object, "textSize", widget.fontSize)
	);
	widget.lineSpacing = GetInteger(
		object,
		"lineSpacing",
		widget.lineSpacing
	);
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
	widget.disabledBrightness = std::clamp(
		GetFloat(
			object,
			"disabledBrightness",
			widget.disabledBrightness
		),
		0.0f,
		1.0f
	);
	widget.disabledOpacity = std::clamp(
		GetFloat(
			object,
			"disabledOpacity",
			widget.disabledOpacity
		),
		0.0f,
		1.0f
	);
	ReadColor(object, "color", widget.textColor);
	ReadRgbaColor(object, "lineColor", widget.lineColor);
	ReadRgbaColor(object, "tooltipColor", widget.tooltipColor);
	ReadColor(object, "tooltipTextColor", widget.tooltipTextColor);
	ReadNineSlice(
		object,
		widget.tooltipNineSlice,
		"tooltipNineSlice",
		"tooltip_nine_slice"
	);

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
		"tooltipOffset",
		widget.tooltipRect.x,
		widget.tooltipRect.y
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
		widget.frameZOrder
	);
	widget.clipChildren = GetBoolean(
		object,
		"clipChildren",
		GetBoolean(
			object,
			"clip_children",
			GetBoolean(object, "clip", false)
		)
	);
	widget.value = GetFloat(object, "value", 0.0f);
	/*
		通用透明度。

		标准写法：
		    opacity = 0.5

		alpha 暂时保留为兼容别名：
		    alpha = 0.5

		最终始终限制在 0.0 ~ 1.0。
	*/
	widget.opacity = std::clamp(
		GetFloat(object,"opacity",GetFloat(object,"alpha",1.0f)),
		0.0f,
		1.0f
	);
	widget.transform.scaleX = std::clamp(
		widget.transform.scaleX,
		0.001f,
		100.0f
	);
	widget.transform.scaleY = std::clamp(
		widget.transform.scaleY,
		0.001f,
		100.0f
	);
	widget.transform.pivotX = std::clamp(
		widget.transform.pivotX,
		0.0f,
		1.0f
	);
	widget.transform.pivotY = std::clamp(
		widget.transform.pivotY,
		0.0f,
		1.0f
	);
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
		GetBoolean(
			object,
			"localiseTooltip",
			widget.localizeTooltip
		)
	);
	widget.tooltipWrap = GetBoolean(
		object,
		"tooltipWrap",
		widget.tooltipWrap
	);
	widget.avoidTooltipOverlap = GetBoolean(
		object,
		"avoidTooltipOverlap",
		GetBoolean(object, "tooltipAvoidMarkers", false)
	);
	widget.lineWidth = std::max(
		0,
		GetInteger(object, "lineWidth", widget.lineWidth)
	);
	widget.tooltipPadding = std::max(
		0,
		GetInteger(object, "tooltipPadding", widget.tooltipPadding)
	);
	widget.tooltipSearchStep = std::max(
		0,
		GetInteger(
			object,
			"tooltipSearchStep",
			widget.tooltipSearchStep
		)
	);
	widget.tooltipFontSize = std::max(
		0,
		GetInteger(
			object,
			"tooltipFontSize",
			widget.tooltipFontSize
		)
	);
	widget.tooltipLineSpacing = std::max(
		0,
		GetInteger(
			object,
			"tooltipLineSpacing",
			widget.tooltipLineSpacing
		)
	);
	widget.tooltipDelayMilliseconds = std::max(
		0,
		GetInteger(
			object,
			"tooltipDelay",
			widget.tooltipDelayMilliseconds
		)
	);
	widget.minimumThumbSize = std::max(
		0,
		GetInteger(
			object,
			"minimumThumbSize",
			widget.minimumThumbSize
		)
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

	WidgetStyleDefaults childStyle = inheritedStyle;
	if (const GuiValue* defaults = FindValue(object, "styleDefaults");
		defaults
		&& defaults->kind == ValueKind::Block
		&& defaults->block)
	{
		MergeStyleDefaults(*defaults->block, childStyle);
	}

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
				*field.value.block,
				childStyle
			)
		);
	}

	return widget;
}

bool SupportsSpriteTransform(WidgetType type)
{
	return type == WidgetType::Window
		|| type == WidgetType::Image
		|| type == WidgetType::Button
		|| type == WidgetType::ScrollBar;
}

bool IsSupportedScaleMode(std::string_view value)
{
	const std::string normalized = ToLower(std::string(value));
	return normalized.empty()
		|| normalized == "stretch"
		|| normalized == "contain"
		|| normalized == "preserve"
		|| normalized == "preserveaspect"
		|| normalized == "aspect"
		|| normalized == "center"
		|| normalized == "none";
}

void ValidateWidgetDefinition(
	const WidgetDefinition& widget,
	const std::filesystem::path& path,
	std::vector<std::string>& diagnostics
)
{
	const std::string widgetName = widget.name.empty()
		? "<unnamed>"
		: widget.name;
	if (!IsSupportedScaleMode(widget.scaleMode))
	{
		diagnostics.push_back(
			path.string() + ": widget '" + widgetName
			+ "' has unsupported scaleMode '" + widget.scaleMode
			+ "'; using stretch"
		);
	}
	if (!IsSupportedScaleMode(widget.tooltipScaleMode))
	{
		diagnostics.push_back(
			path.string() + ": widget '" + widgetName
			+ "' has unsupported tooltipScaleMode '"
			+ widget.tooltipScaleMode + "'; using stretch"
		);
	}
	if ((!widget.scaleMode.empty() || widget.nineSlice.Enabled())
		&& !SupportsSpriteTransform(widget.type))
	{
		diagnostics.push_back(
			path.string() + ": widget '" + widgetName
			+ "' defines scaleMode or nineSlice, but its type does not render a sprite"
		);
	}
	if (widget.type == WidgetType::MarkerLayer)
	{
		if (widget.dataSource.empty())
		{
			diagnostics.push_back(
				path.string() + ": marker layer '" + widgetName
				+ "' requires dataSource"
			);
		}
		if (widget.mapWidgetName.empty())
		{
			diagnostics.push_back(
				path.string() + ": marker layer '" + widgetName
				+ "' requires mapWidget"
			);
		}
		if (widget.regionSource.empty())
		{
			diagnostics.push_back(
				path.string() + ": marker layer '" + widgetName
				+ "' requires regionSource"
			);
		}
		if (widget.markerRect.width <= 0
			|| widget.markerRect.height <= 0)
		{
			diagnostics.push_back(
				path.string() + ": marker layer '" + widgetName
				+ "' requires a positive markerSize"
			);
		}
		const bool hasTooltipSource = !widget.nameSource.empty()
			|| !widget.descriptionSource.empty();
		if (hasTooltipSource
			&& (widget.tooltipRect.width <= 0
				|| widget.tooltipRect.height <= 0))
		{
			diagnostics.push_back(
				path.string() + ": marker layer '" + widgetName
				+ "' requires a positive tooltipSize when tooltip sources are configured"
			);
		}
		if (hasTooltipSource && widget.fontSize <= 0)
		{
			diagnostics.push_back(
				path.string() + ": marker layer '" + widgetName
				+ "' requires a positive fontSize when tooltip sources are configured"
			);
		}
	}
	else
	{
		const bool hasTooltip = !widget.tooltip.empty()
			|| !widget.tooltipSource.empty()
			|| !widget.tooltipLocalizationKey.empty();
		if (hasTooltip
			&& (widget.tooltipRect.width <= 0
				|| widget.tooltipRect.height <= 0))
		{
			diagnostics.push_back(
				path.string() + ": widget '" + widgetName
				+ "' requires a positive tooltipSize when a tooltip is configured"
			);
		}
		if (hasTooltip
			&& widget.tooltipFontSize <= 0
			&& widget.fontSize <= 0)
		{
			diagnostics.push_back(
				path.string() + ": widget '" + widgetName
				+ "' requires tooltipFontSize or fontSize when a tooltip is configured"
			);
		}
	}
	for (const WidgetDefinition& child : widget.children)
	{
		ValidateWidgetDefinition(child, path, diagnostics);
	}
}

void CollectWindows(
	const GuiObject& object,
	std::vector<WindowDefinition>& output,
	const std::filesystem::path& path,
	std::vector<std::string>& diagnostics
)
{
	auto parseBoolean = [](std::string value, bool& outputValue)
	{
		value = ToLower(std::move(value));
		if (value == "yes" || value == "true" || value == "on"
			|| value == "1")
		{
			outputValue = true;
			return true;
		}
		if (value == "no" || value == "false" || value == "off"
			|| value == "0")
		{
			outputValue = false;
			return true;
		}
		return false;
	};
	auto parseInteger = [](std::string_view value, int64_t& outputValue)
	{
		try
		{
			std::size_t parsed = 0;
			const long long number = std::stoll(std::string(value), &parsed);
			if (parsed != value.size())
			{
				return false;
			}
			outputValue = static_cast<int64_t>(number);
			return true;
		}
		catch (...)
		{
			return false;
		}
	};
	auto parseNumber = [](std::string_view value, double& outputValue)
	{
		try
		{
			std::size_t parsed = 0;
			const double number = std::stod(std::string(value), &parsed);
			if (parsed != value.size() || !std::isfinite(number))
			{
				return false;
			}
			outputValue = number;
			return true;
		}
		catch (...)
		{
			return false;
		}
	};
	auto parseValue = [&](std::string_view value, std::string type,
		GuiDataValue& outputValue)
	{
		type = ToLower(std::move(type));
		if (type == "string" || type == "text")
		{
			outputValue = std::string(value);
			return true;
		}
		if (type == "bool" || type == "boolean")
		{
			bool boolean = false;
			if (!parseBoolean(std::string(value), boolean))
			{
				return false;
			}
			outputValue = boolean;
			return true;
		}
		if (type == "int" || type == "integer")
		{
			int64_t integer = 0;
			if (!parseInteger(value, integer))
			{
				return false;
			}
			outputValue = integer;
			return true;
		}
		if (type == "number" || type == "float" || type == "double")
		{
			double number = 0.0;
			if (!parseNumber(value, number))
			{
				return false;
			}
			outputValue = number;
			return true;
		}
		if (!type.empty())
		{
			return false;
		}
		bool boolean = false;
		const std::string normalized = ToLower(std::string(value));
		if (parseBoolean(normalized, boolean)
			&& normalized != "0" && normalized != "1")
		{
			outputValue = boolean;
			return true;
		}
		int64_t integer = 0;
		if (parseInteger(value, integer))
		{
			outputValue = integer;
			return true;
		}
		double number = 0.0;
		if (parseNumber(value, number))
		{
			outputValue = number;
			return true;
		}
		outputValue = std::string(value);
		return true;
	};
	auto parseUnsigned = [](std::string_view value, uint64_t& outputValue)
	{
		if (value.empty()
			|| std::any_of(
				value.begin(),
				value.end(),
				[](unsigned char character)
				{
					return !std::isdigit(character);
				}
			))
		{
			return false;
		}
		try
		{
			std::size_t parsed = 0;
			const unsigned long long number = std::stoull(
				std::string(value),
				&parsed
			);
			if (parsed != value.size())
			{
				return false;
			}
			outputValue = static_cast<uint64_t>(number);
			return true;
		}
		catch (...)
		{
			return false;
		}
	};
	auto parseWindowData = [&](const GuiObject& windowObject,
		WindowDefinition& window)
	{
		std::unordered_set<std::string> valueNames;
		std::unordered_set<std::string> listNames;
		for (const GuiField& dataField : windowObject.fields)
		{
			if (dataField.value.kind != ValueKind::Block
				|| !dataField.value.block)
			{
				continue;
			}
			const std::string dataType = ToLower(dataField.name);
			if (dataType == "datavaluetype")
			{
				StaticDataValueDefinition definition;
				definition.name = GetScalar(*dataField.value.block, "name");
				const std::string key = ToLower(definition.name);
				if (definition.name.empty())
				{
					diagnostics.push_back(path.string()
						+ ": window '" + window.name
						+ "' has dataValueType without name");
					continue;
				}
				if (!valueNames.insert(key).second)
				{
					diagnostics.push_back(path.string()
						+ ": window '" + window.name
						+ "' has duplicate static value '"
						+ definition.name + "'");
					continue;
				}
				if (!parseValue(
						GetScalar(*dataField.value.block, "value"),
						GetScalar(*dataField.value.block, "type"),
						definition.value
					))
				{
					diagnostics.push_back(path.string()
						+ ": window '" + window.name
						+ "' has invalid static value '"
						+ definition.name + "'");
					continue;
				}
				window.staticValues.push_back(std::move(definition));
				continue;
			}
			if (dataType != "datalisttype")
			{
				continue;
			}
			StaticDataListDefinition definition;
			definition.name = GetScalar(*dataField.value.block, "name");
			const std::string key = ToLower(definition.name);
			if (definition.name.empty())
			{
				diagnostics.push_back(path.string()
					+ ": window '" + window.name
					+ "' has dataListType without name");
				continue;
			}
			if (!listNames.insert(key).second)
			{
				diagnostics.push_back(path.string()
					+ ": window '" + window.name
					+ "' has duplicate static list '"
					+ definition.name + "'");
				continue;
			}
			uint64_t revision = 0;
			const std::string revisionText = GetScalar(
				*dataField.value.block,
				"revision"
			);
			if (!revisionText.empty()
				&& !parseUnsigned(revisionText, revision))
			{
				diagnostics.push_back(path.string()
					+ ": window '" + window.name
					+ "' has invalid revision for static list '"
					+ definition.name + "'");
				continue;
			}
			definition.model.revision = revision;
			std::unordered_set<uint64_t> itemIds;
			uint64_t nextId = 1;
			bool valid = true;
			for (const GuiField& itemField : dataField.value.block->fields)
			{
				if (ToLower(itemField.name) != "item"
					|| itemField.value.kind != ValueKind::Block
					|| !itemField.value.block)
				{
					continue;
				}
				GuiListItem item;
				const std::string idText = GetScalar(*itemField.value.block, "id");
				if (!idText.empty()
					&& (!parseUnsigned(idText, item.id) || item.id == 0))
				{
					valid = false;
					break;
				}
				if (item.id == 0)
				{
					item.id = nextId;
				}
				nextId = std::max(nextId, item.id + 1);
				if (!itemIds.insert(item.id).second)
				{
					valid = false;
					break;
				}
				for (const GuiField& itemValue : itemField.value.block->fields)
				{
					if (ToLower(itemValue.name) == "id"
						|| itemValue.value.kind != ValueKind::Scalar)
					{
						continue;
					}
					GuiDataValue parsed;
					if (!parseValue(itemValue.value.scalar, {}, parsed))
					{
						valid = false;
						break;
					}
					item.fields[ToLower(itemValue.name)] = std::move(parsed);
				}
				if (!valid)
				{
					break;
				}
				const GuiDataValue* textValue = item.Find("text");
				if (textValue)
				{
					if (const std::string* text = std::get_if<std::string>(textValue))
					{
						item.text = *text;
					}
				}
				definition.model.items.push_back(std::move(item));
			}
			if (!valid)
			{
				diagnostics.push_back(path.string()
					+ ": window '" + window.name
					+ "' has invalid or duplicate item id in static list '"
					+ definition.name + "'");
				continue;
			}
			window.staticLists.push_back(std::move(definition));
		}
	};

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
				*field.value.block,
				{}
			);
			WindowDefinition window;
			static_cast<WidgetDefinition&>(window) =
				std::move(base);
			parseWindowData(*field.value.block, window);
			output.push_back(std::move(window));
		}

		CollectWindows(*field.value.block, output, path, diagnostics);
	}
}

void RegisterObjectResources(
	const GuiObject& object,
	std::unordered_map<std::string, SpriteResource>& sprites,
	std::unordered_map<std::string, ProgressBarResource>& progressBars,
	std::unordered_map<std::string, IndexedMapResource>& indexedMaps,
	std::unordered_map<std::string, GuiEffectResource>& effects,
	std::unordered_map<std::string, PositionResource>& positions
)
{
	for (const GuiField& field : object.fields)
	{
		if (field.value.kind == ValueKind::Block && field.value.block)
		{
			const std::string fieldType = ToLower(field.name);
			if (fieldType == "positiontype")
			{
				PositionResource position;
				position.name = GetScalar(*field.value.block, "name");
				ReadSize(
					*field.value.block,
					"position",
					position.x,
					position.y
				);
				if (!position.name.empty())
				{
					positions[position.name] = std::move(position);
				}
			}
			else if (fieldType == "spritetype")
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
				sprite.frameLayout = GetFirstScalar(
					*field.value.block,
					{"frameLayout"},
					"horizontal"
				);
				if (const GuiValue* animation = FindValue(
						*field.value.block,
						"animation"))
				{
					if (animation->kind == ValueKind::Scalar)
					{
						sprite.autoAnimate = GetBoolean(
							*field.value.block,
							"animation",
							false
						);
					}
					else if (animation->block)
					{
						const GuiObject& definition = *animation->block;
						sprite.autoAnimate = GetBoolean(
							definition,
							"enabled",
							true
						);
						sprite.animationMode = GetFirstScalar(
							definition,
							{"mode"},
							"loop"
						);
						sprite.animationFrameTimeMilliseconds = std::max(
							0,
							GetInteger(
								definition,
								"frameTime",
								GetInteger(definition, "frameDuration", 0)
							)
						);
						if (sprite.animationFrameTimeMilliseconds == 0)
						{
							const float framesPerSecond = GetFloat(
								definition,
								"fps",
								0.0f
							);
							if (framesPerSecond > 0.0f)
							{
								sprite.animationFrameTimeMilliseconds =
									std::max(
										1,
										static_cast<int>(std::lround(
											1000.0f / framesPerSecond
										))
									);
							}
						}
						sprite.animationStartFrame = std::max(
							1,
							GetInteger(definition, "startFrame", 1)
						);
						sprite.animationEndFrame = std::max(
							0,
							GetInteger(definition, "endFrame", 0)
						);
						sprite.animationOffsetMilliseconds = GetInteger(
							definition,
							"offset",
							0
						);
					}
				}
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
						indexedMap.boundaryWidth
					)
				);
				indexedMap.cropPadding = std::max(
					0,
					GetInteger(
						*field.value.block,
						"cropPadding",
						indexedMap.cropPadding
					)
				);
				indexedMap.flipVertical = GetBoolean(
					*field.value.block,
					"flipVertical",
					indexedMap.flipVertical
				);
				indexedMap.drawBoundaries = GetBoolean(
					*field.value.block,
					"drawBoundaries",
					indexedMap.drawBoundaries
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
			else if (fieldType == "effecttype")
			{
				GuiEffectResource effect;
				effect.name = GetScalar(*field.value.block, "name");
				effect.effect = GetScalar(*field.value.block, "effect");
				ReadRgbaColor(*field.value.block, "color", effect.color);
				effect.minimum = std::clamp(
					GetFloat(*field.value.block, "minimum", 0.5f),
					0.0f,
					1.0f
				);
				effect.maximum = std::clamp(
					GetFloat(*field.value.block, "maximum", 1.0f),
					0.0f,
					1.0f
				);
				if (effect.maximum < effect.minimum)
				{
					std::swap(effect.minimum, effect.maximum);
				}
				effect.speed = std::clamp(
					GetFloat(*field.value.block, "speed", 1.0f),
					0.0f,
					100.0f
				);
				effect.phaseDegrees = GetFloat(
					*field.value.block,
					"phase",
					0.0f
				);
				effect.enabled = GetBoolean(
					*field.value.block,
					"enabled",
					true
				);
				if (!effect.name.empty())
				{
					effects[effect.name] = std::move(effect);
				}
			}

			RegisterObjectResources(
				*field.value.block,
				sprites,
				progressBars,
				indexedMaps,
				effects,
				positions
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

int ResolveSpriteFrameIndex(
	const SpriteResource& resource,
	const WidgetDefinition& widget,
	uint64_t animationTimeMilliseconds,
	int sourcedFrame,
	bool hasSourcedFrame
)
{
	const int frameCount = std::max(1, resource.frameCount);
	if (hasSourcedFrame)
	{
		return std::clamp(sourcedFrame, 1, frameCount);
	}

	const bool animate = widget.animateSpecified
		? widget.animate
		: resource.autoAnimate;
	if (!animate || frameCount == 1)
	{
		return std::clamp(widget.frame, 1, frameCount);
	}

	const int firstFrame = std::clamp(
		widget.animationStartFrame > 0
			? widget.animationStartFrame
			: resource.animationStartFrame,
		1,
		frameCount
	);
	const int lastFrame = std::clamp(
		widget.animationEndFrame > 0
			? widget.animationEndFrame
			: (resource.animationEndFrame > 0
				? resource.animationEndFrame
				: frameCount),
		firstFrame,
		frameCount
	);
	const int frameRange = lastFrame - firstFrame + 1;
	if (frameRange <= 1)
	{
		return firstFrame;
	}

	const int frameTime = std::max(
		1,
		widget.animationFrameTimeMilliseconds > 0
			? widget.animationFrameTimeMilliseconds
			: (resource.animationFrameTimeMilliseconds > 0
				? resource.animationFrameTimeMilliseconds
				: 100)
	);
	const int64_t maximumTime = std::numeric_limits<int64_t>::max();
	int64_t elapsed = animationTimeMilliseconds
		> static_cast<uint64_t>(maximumTime)
		? maximumTime
		: static_cast<int64_t>(animationTimeMilliseconds);
	const int64_t offset = static_cast<int64_t>(
		resource.animationOffsetMilliseconds
	) + static_cast<int64_t>(widget.animationOffsetMilliseconds);
	if (offset > 0 && elapsed > maximumTime - offset)
	{
		elapsed = maximumTime;
	}
	else
	{
		elapsed = std::max<int64_t>(0, elapsed + offset);
	}
	const int64_t step = elapsed / frameTime;

	std::string mode = widget.animationMode.empty()
		? resource.animationMode
		: widget.animationMode;
	mode = ToLower(std::move(mode));
	if (mode == "once")
	{
		return firstFrame + static_cast<int>(std::min<int64_t>(
			step,
			frameRange - 1
		));
	}
	if (mode == "pingpong")
	{
		const int64_t period = static_cast<int64_t>(frameRange) * 2 - 2;
		const int position = static_cast<int>(step % period);
		return firstFrame + (position < frameRange
			? position
			: static_cast<int>(period - position));
	}
	return firstFrame + static_cast<int>(step % frameRange);
}

GuiRect ResolveSpriteFrameSourceRect(
	const SpriteResource& resource,
	int textureWidth,
	int textureHeight,
	int frame
)
{
	if (textureWidth <= 0 || textureHeight <= 0)
	{
		return {};
	}
	const int frameCount = std::max(1, resource.frameCount);
	const int frameIndex = std::clamp(frame, 1, frameCount) - 1;
	if (ToLower(resource.frameLayout) == "vertical")
	{
		const int top = static_cast<int>(
			static_cast<int64_t>(textureHeight) * frameIndex / frameCount
		);
		const int bottom = static_cast<int>(
			static_cast<int64_t>(textureHeight) * (frameIndex + 1)
				/ frameCount
		);
		return {0, top, textureWidth, std::max(0, bottom - top)};
	}
	const int left = static_cast<int>(
		static_cast<int64_t>(textureWidth) * frameIndex / frameCount
	);
	const int right = static_cast<int>(
		static_cast<int64_t>(textureWidth) * (frameIndex + 1) / frameCount
	);
	return {left, 0, std::max(0, right - left), textureHeight};
}

GuiRgbaColor SampleGuiEffect(
	const GuiEffectResource& resource,
	uint64_t elapsedMilliseconds
)
{
	GuiRgbaColor output;
	if (!resource.enabled)
	{
		return output;
	}

	const std::string effect = ToLower(resource.effect);
	if (effect == "tint")
	{
		return resource.color;
	}

	constexpr double pi = 3.14159265358979323846;
	const double seconds = static_cast<double>(elapsedMilliseconds) / 1000.0;
	const double phase = static_cast<double>(resource.phaseDegrees)
		* pi / 180.0;
	const float wave = static_cast<float>(
		(std::sin(seconds * static_cast<double>(resource.speed)
			* 2.0 * pi + phase) + 1.0) * 0.5
	);
	const float pulse = resource.minimum
		+ (resource.maximum - resource.minimum) * wave;
	if (effect == "opacity_pulse")
	{
		output.r = resource.color.r;
		output.g = resource.color.g;
		output.b = resource.color.b;
		output.a = resource.color.a * pulse;
	}
	else if (effect == "color_pulse")
	{
		output.r = 1.0f + (resource.color.r - 1.0f) * pulse;
		output.g = 1.0f + (resource.color.g - 1.0f) * pulse;
		output.b = 1.0f + (resource.color.b - 1.0f) * pulse;
		output.a = resource.color.a;
	}
	else
	{
		output.r = resource.color.r * pulse;
		output.g = resource.color.g * pulse;
		output.b = resource.color.b * pulse;
		output.a = resource.color.a;
	}
	return output;
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
			loadDiagnostics_.push_back(fileError);
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
	ValidateStrictGuiDocument(
		document,
		strictLegacyFiles_,
		loadDiagnostics_
	);

	RegisterResources(document.root);
	const std::size_t firstWindow = windows_.size();
	RegisterLayouts(document.root, path);
	for (std::size_t index = firstWindow; index < windows_.size(); ++index)
	{
		ValidateWidgetDefinition(windows_[index], path, loadDiagnostics_);
	}
    documents_.push_back(std::move(document));
    return true;
}

void GuiInterpreter::RegisterResources(const GuiObject& object)
{
	RegisterObjectResources(
		object,
		sprites_,
		progressBars_,
		indexedMaps_,
		effects_,
		positions_
	);
}

void GuiInterpreter::RegisterLayouts(
	const GuiObject& object,
	const std::filesystem::path& path
)
{
	CollectWindows(object, windows_, path, loadDiagnostics_);
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

const GuiEffectResource* GuiInterpreter::FindEffect(
	const std::string& name
) const
{
	const auto iterator = effects_.find(name);
	return iterator == effects_.end() ? nullptr : &iterator->second;
}

GuiRect GuiInterpreter::ResolveRootRect(
	const std::string& name,
	const GuiLayoutContext& context
) const
{
	const WindowDefinition* window = FindWindow(name);
	if (!window)
	{
		return {};
	}
	if (window->fullScreen && context.hasRootClientRect)
	{
		return context.rootClientRect;
	}
	GuiRect rect = window->rect;
	if (!window->positionType.empty())
	{
		if (const PositionResource* position = FindPosition(
				window->positionType))
		{
			rect.x += position->x;
			rect.y += position->y;
		}
	}
	return rect;
}

GuiRect GuiInterpreter::ResolveChildRect(
	const GuiRect& parent,
	const WidgetDefinition& child
) const
{
	GuiRect local = child.rect;
	if (!child.positionType.empty())
	{
		if (const PositionResource* position = FindPosition(
				child.positionType))
		{
			local.x += position->x;
			local.y += position->y;
		}
	}
	const std::string orientation = ToLower(child.orientation);
	GuiRect resolved = {
		parent.x + local.x,
		parent.y + local.y,
		local.width,
		local.height
	};
	if (orientation == "upper_right")
	{
		resolved.x = parent.x + parent.width - local.width - local.x;
	}
	else if (orientation == "lower_left")
	{
		resolved.y = parent.y + parent.height - local.height - local.y;
	}
	else if (orientation == "lower_right")
	{
		resolved.x = parent.x + parent.width - local.width - local.x;
		resolved.y = parent.y + parent.height - local.height - local.y;
	}
	else if (orientation == "center" || orientation == "centre")
	{
		resolved.x = parent.x + (parent.width - local.width) / 2 + local.x;
		resolved.y = parent.y + (parent.height - local.height) / 2 + local.y;
	}
	else if (orientation == "center_up" || orientation == "center_top")
	{
		resolved.x = parent.x + (parent.width - local.width) / 2 + local.x;
	}
	else if (orientation == "center_down"
		|| orientation == "center_bottom")
	{
		resolved.x = parent.x + (parent.width - local.width) / 2 + local.x;
		resolved.y = parent.y + parent.height - local.height - local.y;
	}
	else if (orientation == "center_left")
	{
		resolved.y = parent.y + (parent.height - local.height) / 2 + local.y;
	}
	else if (orientation == "center_right")
	{
		resolved.x = parent.x + parent.width - local.width - local.x;
		resolved.y = parent.y + (parent.height - local.height) / 2 + local.y;
	}
	return resolved;
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

	auto intersectRects = [](
		const GuiRect& first,
		const GuiRect& second,
		GuiRect& output
	)
	{
		const int left = std::max(first.x, second.x);
		const int top = std::max(first.y, second.y);
		const int right = std::min(
			first.x + first.width,
			second.x + second.width
		);
		const int bottom = std::min(
			first.y + first.height,
			second.y + second.height
		);
		output = {
			left,
			top,
			std::max(0, right - left),
			std::max(0, bottom - top)
		};
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
			entry.resolved.rect = ResolveRootRect(name, context);
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
			entry.resolved.opacity = std::clamp(
                entry.definition->opacity,
                 0.0f,
                 1.0f
                );
			entry.resolved.transform = entry.definition->transform;
			entry.resolved.depth = 0;
			entry.resolved.zOrder =
				entry.definition->zOrder;
			entry.resolved.clipRect = {};
			entry.resolved.hasClipRect = false;
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

			entry.resolved.rect = ResolveChildRect(
				parent.rect,
				definition
			);
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
			entry.resolved.opacity = std::clamp(
                parent.opacity * definition.opacity,
                0.0f,
                1.0f
                );
			entry.resolved.transform = definition.transform;
			entry.resolved.depth = parent.depth + 1;
			entry.resolved.zOrder =
				parent.zOrder + definition.zOrder;
			entry.resolved.clipRect = parent.clipRect;
			entry.resolved.hasClipRect = parent.hasClipRect;
			if (entries[entry.parent].definition->clipChildren)
			{
				if (entry.resolved.hasClipRect)
				{
					intersectRects(
						entry.resolved.clipRect,
						parent.rect,
						entry.resolved.clipRect
					);
				}
				else
				{
					entry.resolved.clipRect = parent.rect;
					entry.resolved.hasClipRect = true;
				}
			}
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
	output.itemFilterField = listDefinition->itemFilterField;
	output.itemFilterValueSource = listDefinition->itemFilterValueSource;
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
		output.minimumThumbSize =
			scrollbarDefinition->minimumThumbSize;
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
				text = context.textResolver(ReplaceItemId(
					std::move(source),
					item.id
				));
			}
			if (!textDefinition->localizationKey.empty()
				&& context.localizationResolver)
			{
				text = context.localizationResolver(
					ReplaceItemId(
						textDefinition->localizationKey,
						item.id
					)
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

	return ResolveAssetPath(sprite->textureFile, projectRoot);
}

std::filesystem::path GuiInterpreter::ResolveAssetPath(
	const std::filesystem::path& asset,
	const std::filesystem::path& projectRoot
) const
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

bool PointInsideTransformedWidget(
	const GuiResolvedWidget& widget,
	int mouseX,
	int mouseY
)
{
	const GuiRect& rect = widget.rect;
	const GuiTransform2D& transform = widget.transform;
	if (std::abs(transform.rotationDegrees) <= 0.000001f
		&& std::abs(transform.scaleX - 1.0f) <= 0.000001f
		&& std::abs(transform.scaleY - 1.0f) <= 0.000001f
		&& !transform.flipX
		&& !transform.flipY)
	{
		return mouseX >= rect.x
			&& mouseX < rect.x + rect.width
			&& mouseY >= rect.y
			&& mouseY < rect.y + rect.height;
	}
	const float scaleX = transform.scaleX
		* (transform.flipX ? -1.0f : 1.0f);
	const float scaleY = transform.scaleY
		* (transform.flipY ? -1.0f : 1.0f);
	if (std::abs(scaleX) < 0.000001f || std::abs(scaleY) < 0.000001f)
	{
		return false;
	}

	const float pivotX = static_cast<float>(rect.x)
		+ static_cast<float>(rect.width) * transform.pivotX;
	const float pivotY = static_cast<float>(rect.y)
		+ static_cast<float>(rect.height) * transform.pivotY;
	const float radians = transform.rotationDegrees
		* 3.14159265358979323846f / 180.0f;
	const float cosine = std::cos(radians);
	const float sine = std::sin(radians);
	const float translatedX = static_cast<float>(mouseX) - pivotX;
	const float translatedY = static_cast<float>(mouseY) - pivotY;
	const float unrotatedX = cosine * translatedX + sine * translatedY;
	const float unrotatedY = -sine * translatedX + cosine * translatedY;
	const float sourceX = unrotatedX / scaleX + pivotX;
	const float sourceY = unrotatedY / scaleY + pivotY;
	return sourceX >= static_cast<float>(rect.x)
		&& sourceX < static_cast<float>(rect.x + rect.width)
		&& sourceY >= static_cast<float>(rect.y)
		&& sourceY < static_cast<float>(rect.y + rect.height);
}

const GuiResolvedWidget* HitTestGuiWidgetsInternal(
	const std::vector<GuiResolvedWidget>& widgets,
	int mouseX,
	int mouseY,
	bool includeTooltipOnlyWidgets
)
{
	for (auto iterator = widgets.rbegin();
		 iterator != widgets.rend();
		 ++iterator)
	{
		if (!iterator->definition
			|| !iterator->visible
			|| !iterator->enabled
			|| iterator->opacity <= 0.0f)
		{
			continue;
		}

		const WidgetType type = iterator->definition->type;
		if (type == WidgetType::MarkerLayer)
		{
			continue;
		}
		const GuiActionBinding& actions =
			iterator->definition->actions;
		const bool hasTooltip =
			!iterator->definition->tooltip.empty()
			|| !iterator->definition->tooltipSource.empty()
			|| !iterator->definition->tooltipLocalizationKey.empty();
		const bool hasPointerActions =
			!actions.onClick.empty()
			|| !actions.onPress.empty()
			|| !actions.onRelease.empty()
			|| !actions.onHoverEnter.empty()
			|| !actions.onHoverLeave.empty()
			|| !actions.onDragStart.empty()
			|| !actions.onDrag.empty()
			|| !actions.onDragEnd.empty();
		const bool isInteractiveWindow =
			type == WidgetType::Window
			&& iterator->definition->moveable
			&& iterator->definition->dragHeight > 0;
		if (!iterator->definition->draggable
			&& !hasPointerActions
			&& !isInteractiveWindow
			&& !(includeTooltipOnlyWidgets && hasTooltip)
			&& type != WidgetType::Button
			&& type != WidgetType::ListBox
			&& type != WidgetType::ScrollBar
			&& type != WidgetType::IndexedMap
			&& type != WidgetType::Custom)
		{
			continue;
		}

		if (iterator->hasClipRect
			&& (mouseX < iterator->clipRect.x
				|| mouseX >= iterator->clipRect.x
					+ iterator->clipRect.width
				|| mouseY < iterator->clipRect.y
				|| mouseY >= iterator->clipRect.y
					+ iterator->clipRect.height))
		{
			continue;
		}
		if (PointInsideTransformedWidget(*iterator, mouseX, mouseY))
		{
			return &*iterator;
		}
	}

	return nullptr;
}

const PositionResource* GuiInterpreter::FindPosition(
	const std::string& name
) const
{
	const auto iterator = positions_.find(name);
	return iterator == positions_.end() ? nullptr : &iterator->second;
}

const GuiResolvedWidget* HitTestGuiWidgets(
	const std::vector<GuiResolvedWidget>& widgets,
	int mouseX,
	int mouseY
)
{
	return HitTestGuiWidgetsInternal(
		widgets,
		mouseX,
		mouseY,
		false
	);
}

const GuiResolvedWidget* HitTestGuiHoverWidgets(
	const std::vector<GuiResolvedWidget>& widgets,
	int mouseX,
	int mouseY
)
{
	return HitTestGuiWidgetsInternal(
		widgets,
		mouseX,
		mouseY,
		true
	);
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
