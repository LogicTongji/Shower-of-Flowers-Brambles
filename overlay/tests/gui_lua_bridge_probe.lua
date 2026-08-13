package.path = package.path .. ";script/?.lua"

local updates = {}
local actions = {
	{
		action = "activate_item",
		functionName = "ProbeGui.ActivateItem",
		listItemId = 17,
		hasListItemId = true
	}
}

_G.ScriptedGuiNative = {
	PublishUpdate = function(channelName, update)
		table.insert(updates, {
			channel = channelName,
			update = update
		})
		return true
	end,
	TryPopAction = function(channelName)
		assert(channelName == "probe")
		if #actions == 0 then
			return nil
		end
		return table.remove(actions, 1)
	end
}

local GuiActionBridge = require('gui_action_bridge')
local GuiDataBridge = require('gui_data_bridge')
local activatedItem = 0

GuiActionBridge.Register(
	"ProbeGui.ActivateItem",
	function(payload)
		activatedItem = payload.listItemId
		return true
	end
)

assert(GuiDataBridge.PublishSnapshot("probe", {
	values = {
		["state.visible"] = true,
		counter = 2
	},
	lists = {}
}))
assert(updates[1].update.fullSnapshot == true)
assert(updates[1].update.baseRevision == 0)
assert(updates[1].update.revision == 1)

assert(GuiDataBridge.PublishDelta("probe", {
	values = {
		counter = 5
	}
}))
assert(updates[2].update.fullSnapshot == false)
assert(updates[2].update.baseRevision == 1)
assert(updates[2].update.revision == 2)

assert(GuiDataBridge.DispatchActions("probe", 8) == 1)
assert(activatedItem == 17)

print("Lua protocol revision: " .. updates[2].update.revision)
print("Lua callback item: " .. activatedItem)
