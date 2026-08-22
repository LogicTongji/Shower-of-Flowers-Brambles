#include "gui_declarative_data.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "gui_interpreter.h"

namespace
{

std::string Lower(std::string value)
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

std::vector<std::string> SplitFieldNames(std::string value)
{
	std::vector<std::string> output;
	std::size_t start = 0;
	while (start <= value.size())
	{
		const std::size_t end = value.find(',', start);
		std::string field = value.substr(
			start,
			end == std::string::npos
				? std::string::npos
				: end - start
		);
		field.erase(
			field.begin(),
			std::find_if(
				field.begin(),
				field.end(),
				[](unsigned char character)
				{
					return !std::isspace(character);
				}
			)
		);
		field.erase(
			std::find_if(
				field.rbegin(),
				field.rend(),
				[](unsigned char character)
				{
					return !std::isspace(character);
				}
			).base(),
			field.end()
		);
		if (!field.empty())
		{
			output.push_back(Lower(std::move(field)));
		}
		if (end == std::string::npos)
		{
			break;
		}
		start = end + 1;
	}
	return output;
}

bool EqualsIgnoreCase(
    std::string_view first,
    std::string_view second
)
{
    if (first.size() != second.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < first.size(); ++index)
    {
        if (std::tolower(static_cast<unsigned char>(first[index]))
            != std::tolower(static_cast<unsigned char>(second[index])))
        {
            return false;
        }
    }
    return true;
}

const gui::GuiValue* FindValue(
    const gui::GuiObject& object,
    std::string_view name
)
{
    for (const gui::GuiField& field : object.fields)
    {
        if (EqualsIgnoreCase(field.name, name))
        {
            return &field.value;
        }
    }
    return nullptr;
}

std::string FindScalar(
    const gui::GuiObject& object,
    std::string_view name
)
{
    const gui::GuiValue* value = FindValue(object, name);
    return value && value->kind == gui::ValueKind::Scalar
        ? value->scalar
        : std::string{};
}

void CollectDataBlocks(
    const gui::GuiObject& object,
    std::vector<const gui::GuiObject*>& output
)
{
    for (const gui::GuiField& field : object.fields)
    {
        if (field.value.kind != gui::ValueKind::Block
            || !field.value.block)
        {
            continue;
        }
        if (EqualsIgnoreCase(field.name, "guiData"))
        {
            output.push_back(field.value.block.get());
        }
        else
        {
            CollectDataBlocks(*field.value.block, output);
        }
    }
}

bool ParseBoolean(std::string value, bool& output)
{
    value = Lower(std::move(value));
    if (value == "yes"
        || value == "true"
        || value == "on"
        || value == "1")
    {
        output = true;
        return true;
    }
    if (value == "no"
        || value == "false"
        || value == "off"
        || value == "0")
    {
        output = false;
        return true;
    }
    return false;
}

bool ParseInteger(std::string_view value, int64_t& output)
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

bool ParseNumber(std::string_view value, double& output)
{
    try
    {
        std::size_t parsed = 0;
        output = std::stod(std::string(value), &parsed);
        return parsed == value.size() && std::isfinite(output);
    }
    catch (...)
    {
        return false;
    }
}

bool ParseDataValue(
    std::string_view value,
    std::string type,
    GuiDataValue& output
)
{
    type = Lower(std::move(type));
    if (type == "string" || type == "text")
    {
        output = std::string(value);
        return true;
    }
    if (type == "bool" || type == "boolean")
    {
        bool boolean = false;
        if (!ParseBoolean(std::string(value), boolean))
        {
            return false;
        }
        output = boolean;
        return true;
    }
    if (type == "int" || type == "integer")
    {
        int64_t integer = 0;
        if (!ParseInteger(value, integer))
        {
            return false;
        }
        output = integer;
        return true;
    }
    if (type == "number"
        || type == "float"
        || type == "double")
    {
        double number = 0.0;
        if (!ParseNumber(value, number))
        {
            return false;
        }
        output = number;
        return true;
    }
    if (!type.empty())
    {
        return false;
    }

    const std::string normalized = Lower(std::string(value));
    if (normalized == "yes"
        || normalized == "no"
        || normalized == "true"
        || normalized == "false"
        || normalized == "on"
        || normalized == "off")
    {
        bool boolean = false;
        ParseBoolean(normalized, boolean);
        output = boolean;
        return true;
    }

    int64_t integer = 0;
    if (ParseInteger(value, integer))
    {
        output = integer;
        return true;
    }
    double number = 0.0;
    if (ParseNumber(value, number))
    {
        output = number;
        return true;
    }
    output = std::string(value);
    return true;
}

bool ParseUnsigned(std::string_view value, uint64_t& output)
{
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
        output = static_cast<uint64_t>(number);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool SetParsedValue(
    GuiDataRegistry& registry,
    std::string name,
    const std::string& value,
    const std::string& type,
    std::string& error
)
{
    if (name.empty())
    {
        error = "declarative_data_value_name_missing";
        return false;
    }
    GuiDataValue parsed;
    if (!ParseDataValue(value, type, parsed))
    {
        error = "declarative_data_value_invalid: " + name;
        return false;
    }
    registry.Set(std::move(name), std::move(parsed));
    return true;
}

bool ParseList(
    const gui::GuiObject& object,
    GuiDataRegistry& registry,
    std::string& error
)
{
    const std::string name = FindScalar(object, "name");
    if (name.empty())
    {
        error = "declarative_data_list_name_missing";
        return false;
    }

    GuiListModel model;
    uint64_t revision = 0;
    if (ParseUnsigned(FindScalar(object, "revision"), revision))
    {
        model.revision = revision;
    }

    uint64_t nextId = 1;
    for (const gui::GuiField& field : object.fields)
    {
        if (!EqualsIgnoreCase(field.name, "item")
            || field.value.kind != gui::ValueKind::Block
            || !field.value.block)
        {
            continue;
        }

        GuiListItem item;
        const std::string id = FindScalar(*field.value.block, "id");
        if (!id.empty() && !ParseUnsigned(id, item.id))
        {
            error = "declarative_data_list_item_id_invalid: " + name;
            return false;
        }
        if (item.id == 0)
        {
            item.id = nextId;
        }
        nextId = std::max(nextId, item.id + 1);
        for (const gui::GuiField& itemField
            : field.value.block->fields)
        {
            if (EqualsIgnoreCase(itemField.name, "id")
                || itemField.value.kind != gui::ValueKind::Scalar)
            {
                continue;
            }
            GuiDataValue value;
            if (!ParseDataValue(itemField.value.scalar, {}, value))
            {
                error = "declarative_data_list_item_field_invalid: "
                    + name + "." + itemField.name;
                return false;
            }
            item.fields[Lower(itemField.name)] = std::move(value);
        }
        const GuiDataValue* text = item.Find("text");
        item.text = text ? GuiDataValueToText(*text) : std::string{};
        model.items.push_back(std::move(item));
    }

    registry.SetList(name, std::move(model));
    return true;
}

bool ParseDataBlock(
    const gui::GuiObject& object,
    GuiDataRegistry& registry,
    std::string& error
)
{
    for (const gui::GuiField& field : object.fields)
    {
        if (field.value.kind == gui::ValueKind::Scalar
            && !field.name.empty())
        {
            if (!SetParsedValue(
                    registry,
                    field.name,
                    field.value.scalar,
                    {},
                    error
                ))
            {
                return false;
            }
            continue;
        }
        if (field.value.kind != gui::ValueKind::Block
            || !field.value.block)
        {
            continue;
        }
        if (EqualsIgnoreCase(field.name, "value"))
        {
            if (!SetParsedValue(
                    registry,
                    FindScalar(*field.value.block, "name"),
                    FindScalar(*field.value.block, "value"),
                    FindScalar(*field.value.block, "type"),
                    error
                ))
            {
                return false;
            }
        }
        else if (EqualsIgnoreCase(field.name, "list")
            && !ParseList(*field.value.block, registry, error))
        {
            return false;
        }
    }
    return true;
}

std::string FindParameter(
    const GuiActionContext& context,
    std::string_view name
)
{
    const auto found = context.parameters.find(Lower(std::string(name)));
    return found == context.parameters.end()
        ? std::string{}
        : found->second;
}

std::string ResolveActionValue(
    const GuiActionContext& context,
    std::string value,
    const GuiDataRegistry* registry = nullptr
)
{
    const std::string token = Lower(value);
    if (token == "$list_item_id" || token == "$listitemid")
    {
        return context.hasListItemId
            ? std::to_string(context.listItemId)
            : std::string{};
    }
    if (token == "$list_index" || token == "$listindex")
    {
        return std::to_string(context.listIndex);
    }
    if (token == "$mouse_x" || token == "$mousex")
    {
        return std::to_string(context.mouseX);
    }
    if (token == "$mouse_y" || token == "$mousey")
    {
        return std::to_string(context.mouseY);
    }
	if (token == "$drag_value" || token == "$dragvalue")
	{
		return FindParameter(context, "value");
	}
	if (token == "$drag_normalized" || token == "$dragnormalized")
	{
		return FindParameter(context, "normalized");
	}
	if (token == "$drag_step" || token == "$dragstep")
	{
		return FindParameter(context, "stepindex");
	}
	if (token == "$drag_delta_x" || token == "$dragdeltax")
	{
		return FindParameter(context, "deltax");
	}
	if (token == "$drag_delta_y" || token == "$dragdeltay")
	{
		return FindParameter(context, "deltay");
	}
    if (token == "$widget")
    {
        return context.widgetName;
    }
    if (token == "$list")
    {
        return context.listName;
    }
    constexpr std::string_view parameterPrefix = "$parameter:";
    if (token.rfind(parameterPrefix, 0) == 0)
    {
        return FindParameter(
            context,
            token.substr(parameterPrefix.size())
        );
    }
    constexpr std::string_view dataPrefix = "$data:";
    if (registry && token.rfind(dataPrefix, 0) == 0)
    {
        return registry->ResolveText(
            value.substr(dataPrefix.size())
        );
    }
    return value;
}

bool SetListItemField(
    GuiListItem& item,
    std::string name,
    const std::string& value
)
{
    name = Lower(std::move(name));
    if (name.empty())
    {
        return false;
    }
    GuiDataValue parsed;
    if (!ParseDataValue(value, {}, parsed))
    {
        return false;
    }
    item.fields[name] = std::move(parsed);
    if (name == "text")
    {
        item.text = value;
    }
    return true;
}

}

GuiDeclarativeDataStore::GuiDeclarativeDataStore()
    : registry_(std::make_shared<GuiDataRegistry>())
{
}

bool GuiDeclarativeDataStore::LoadFile(
    const std::filesystem::path& path,
    std::string& error
)
{
    return LoadFiles({path}, error);
}

bool GuiDeclarativeDataStore::LoadFiles(
    const std::vector<std::filesystem::path>& paths,
    std::string& error
)
{
    error.clear();
    if (paths.empty())
    {
        error = "declarative_data_file_list_empty";
        return false;
    }

    auto nextRegistry = std::make_shared<GuiDataRegistry>();
    std::size_t blockCount = 0;
    for (const std::filesystem::path& path : paths)
    {
        gui::GuiInterpreter parser;
        if (!parser.LoadFile(path, error))
        {
            return false;
        }

        for (const gui::GuiDocument& document : parser.Documents())
        {
            std::vector<const gui::GuiObject*> blocks;
            CollectDataBlocks(document.root, blocks);
            blockCount += blocks.size();
            for (const gui::GuiObject* block : blocks)
            {
                if (!ParseDataBlock(*block, *nextRegistry, error))
                {
                    error = path.string() + ": " + error;
                    return false;
                }
            }
        }
    }
    if (blockCount == 0)
    {
        error = "declarative_data_block_missing";
        return false;
    }

    registry_ = std::move(nextRegistry);
    return true;
}

void GuiDeclarativeDataStore::Clear()
{
    registry_ = std::make_shared<GuiDataRegistry>();
}

std::shared_ptr<GuiDataRegistry>
GuiDeclarativeDataStore::Registry() const
{
    return registry_;
}

void GuiDeclarativeDataStore::SetRegistry(
    std::shared_ptr<GuiDataRegistry> registry
)
{
    registry_ = registry
        ? std::move(registry)
        : std::make_shared<GuiDataRegistry>();
}

bool GuiDeclarativeDataStore::SetFromText(
    std::string_view name,
    std::string_view value,
    std::string_view type
)
{
    if (name.empty())
    {
        return false;
    }

    std::string resolvedType(type);
    if (resolvedType.empty())
    {
        const GuiDataValue* current = registry_->Find(name);
        if (current)
        {
            if (std::holds_alternative<bool>(*current))
            {
                resolvedType = "bool";
            }
            else if (std::holds_alternative<int64_t>(*current))
            {
                resolvedType = "integer";
            }
            else if (std::holds_alternative<double>(*current))
            {
                resolvedType = "number";
            }
            else if (std::holds_alternative<std::string>(*current))
            {
                resolvedType = "string";
            }
        }
    }

    GuiDataValue parsed;
    if (!ParseDataValue(value, resolvedType, parsed))
    {
        return false;
    }
    registry_->Set(std::string(name), std::move(parsed));
    return true;
}

bool GuiDeclarativeDataStore::ApplyAction(
    const GuiActionContext& context
)
{
    const std::string operation = Lower(context.fallbackOperation);
    const std::string target = FindParameter(context, "target");
    if (target.empty())
    {
        return false;
    }

    if (operation == "select_item"
        || operation == "select_indexed_item"
        || operation == "select_indexed_map_item")
    {
        if (!context.hasListItemId
            || !SetFromText(
                target,
                std::to_string(context.listItemId),
                "integer"
            ))
        {
            return false;
        }
        for (const auto& parameter : context.parameters)
        {
            constexpr std::string_view prefix = "set.";
            if (parameter.first.rfind(prefix, 0) != 0)
            {
                continue;
            }
            if (!SetFromText(
                    parameter.first.substr(prefix.size()),
                    ResolveActionValue(
                        context,
                        parameter.second,
                        registry_.get()
                    )
                ))
            {
                return false;
            }
        }
        return true;
    }

	if (operation == "set_value"
        || operation == "set_data"
        || operation == "set")
    {
        return SetFromText(
            target,
            ResolveActionValue(
                context,
                FindParameter(context, "value"),
                registry_.get()
            ),
            FindParameter(context, "type")
        );
    }
	if (operation == "set_drag_value"
		|| operation == "set_drag_data")
	{
		const std::string value = FindParameter(context, "value");
		if (!SetFromText(target, value, "number"))
		{
			return false;
		}
		const std::string stepTarget = FindParameter(
			context,
			"steptarget"
		);
		const std::string stepIndex = FindParameter(
			context,
			"stepindex"
		);
		if (!stepTarget.empty()
			&& !SetFromText(stepTarget, stepIndex, "integer"))
		{
			return false;
		}
		const std::string normalizedTarget = FindParameter(
			context,
			"normalizedtarget"
		);
		if (!normalizedTarget.empty()
			&& !SetFromText(
				normalizedTarget,
				FindParameter(context, "normalized"),
				"number"
			))
		{
			return false;
		}
		return true;
	}
    if (operation == "set_text")
    {
        return SetFromText(
            target,
            ResolveActionValue(
                context,
                FindParameter(context, "value"),
                registry_.get()
            ),
            "string"
        );
    }
    if (operation == "toggle_value" || operation == "toggle")
    {
        registry_->Set(target, !registry_->ResolveBool(target));
        return true;
    }
    if (operation == "add_value"
        || operation == "increment_value"
        || operation == "add")
    {
        std::string amountText = FindParameter(context, "amount");
        if (amountText.empty())
        {
            amountText = FindParameter(context, "value");
        }
        double amount = 0.0;
        if (!ParseNumber(amountText, amount))
        {
            return false;
        }

        const GuiDataValue* current = registry_->Find(target);
        const double next = registry_->ResolveNumber(target) + amount;
        if (current
            && std::holds_alternative<int64_t>(*current)
            && std::floor(next) == next
            && next >= static_cast<double>(
                std::numeric_limits<int64_t>::min()
            )
            && next <= static_cast<double>(
                std::numeric_limits<int64_t>::max()
            ))
        {
            registry_->Set(target, static_cast<int64_t>(next));
        }
        else
        {
            registry_->Set(target, next);
        }
        return true;
    }
    if (operation == "copy_list_item"
        || operation == "upsert_list_item")
    {
        const GuiListModel* source = registry_->FindList(
            context.listName
        );
        if (!source
            || context.listIndex < 0
            || context.listIndex >= static_cast<int>(source->items.size()))
        {
            return false;
        }

        GuiListModel output;
        if (const GuiListModel* existing = registry_->FindList(target))
        {
            output = *existing;
        }
        GuiListItem item = source->items[context.listIndex];
		if (const GuiDataValue* enabled = item.Find("enabled"))
		{
			if (!GuiDataValueToBool(*enabled))
			{
				return false;
			}
		}
		if (const GuiDataValue* condition = item.Find("enabledwhen"))
		{
			if (!registry_->EvaluateCondition(
					GuiDataValueToText(*condition)
				))
			{
				return false;
			}
		}
        for (const auto& parameter : context.parameters)
        {
            constexpr std::string_view prefix = "field.";
            if (parameter.first.rfind(prefix, 0) != 0)
            {
                continue;
            }
            if (!SetListItemField(
                    item,
                    parameter.first.substr(prefix.size()),
                    ResolveActionValue(
                        context,
                        parameter.second,
                        registry_.get()
                    )
                ))
            {
                return false;
            }
        }
		const std::string sequenceField = Lower(
			FindParameter(context, "sequence_field")
		);
		if (!sequenceField.empty())
		{
			double nextValue = 1.0;
			for (const GuiListItem& existing : output.items)
			{
				if (const GuiDataValue* value = existing.Find(sequenceField))
				{
					nextValue = std::max(
						nextValue,
						GuiDataValueToNumber(*value) + 1.0
					);
				}
			}
			if (!SetListItemField(
					item,
					sequenceField,
					std::to_string(
						static_cast<int64_t>(std::llround(nextValue))
					)
				))
			{
				return false;
			}
		}
		const std::vector<std::string> rejectFields = SplitFieldNames(
			FindParameter(context, "reject_matching_fields")
		);
		if (!rejectFields.empty())
		{
			for (const GuiListItem& existing : output.items)
			{
				bool matches = true;
				for (const std::string& field : rejectFields)
				{
					const GuiDataValue* expected = item.Find(field);
					const GuiDataValue* actual = existing.Find(field);
					if (!expected
						|| !actual
						|| GuiDataValueToText(*expected)
							!= GuiDataValueToText(*actual))
					{
						matches = false;
						break;
					}
				}
				if (matches)
				{
					return false;
				}
			}
		}
        const auto found = std::find_if(
            output.items.begin(),
            output.items.end(),
            [&item](const GuiListItem& candidate)
            {
                return candidate.id == item.id;
            }
        );
        if (found == output.items.end())
        {
            output.items.push_back(std::move(item));
        }
        else
        {
			const std::string rejectExisting = Lower(
				FindParameter(context, "reject_existing")
			);
			if (rejectExisting == "yes"
				|| rejectExisting == "true"
				|| rejectExisting == "1")
			{
				return false;
			}
            *found = std::move(item);
        }
        ++output.revision;
        registry_->SetList(target, std::move(output));
        return true;
    }
	if (operation == "remove_list_item"
		|| operation == "erase_list_item")
	{
		if (!context.hasListItemId)
		{
			return false;
		}
		const GuiListModel* existing = registry_->FindList(target);
		if (!existing)
		{
			return false;
		}
		GuiListModel output = *existing;
		const auto found = std::remove_if(
			output.items.begin(),
			output.items.end(),
			[&context](const GuiListItem& item)
			{
				return item.id == context.listItemId;
			}
		);
		if (found == output.items.end())
		{
			return false;
		}
		output.items.erase(found, output.items.end());
		++output.revision;
		registry_->SetList(target, std::move(output));
		return true;
	}
    if (operation == "update_list_item")
    {
        if (!context.hasListItemId)
        {
            return false;
        }
        const GuiListModel* existing = registry_->FindList(target);
        if (!existing)
        {
            return false;
        }
        GuiListModel output = *existing;
		const std::string matchingField = Lower(
			FindParameter(context, "update_matching_field")
		);
		const std::string matchingValue = matchingField.empty()
			? std::string{}
			: FindParameter(context, matchingField);
		bool changed = false;
		for (GuiListItem& item : output.items)
		{
			bool selected = item.id == context.listItemId;
			if (!matchingField.empty())
			{
				const GuiDataValue* value = item.Find(matchingField);
				selected = value
					&& GuiDataValueToText(*value) == matchingValue;
			}
			if (!selected)
			{
				continue;
			}
			for (const auto& parameter : context.parameters)
			{
				constexpr std::string_view prefix = "field.";
				if (parameter.first.rfind(prefix, 0) != 0)
				{
					continue;
				}
				if (!SetListItemField(
						item,
						parameter.first.substr(prefix.size()),
						ResolveActionValue(
							context,
							parameter.second,
							registry_.get()
						)
					))
				{
					return false;
				}
			}
			changed = true;
		}
		if (!changed)
		{
			return false;
		}
        ++output.revision;
        registry_->SetList(target, std::move(output));
        return true;
    }
    return false;
}
