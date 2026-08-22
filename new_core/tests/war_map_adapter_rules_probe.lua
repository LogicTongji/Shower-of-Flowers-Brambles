package.path = package.path .. ";script/?.lua"

local mockState = {
	playerTag = "CHI",
	date = 1,
	visible = true,
	active = true,
	regions = {
		hubei_region = {
			japaneseControlledPercentage = 50
		},
		henan_region = {
			japaneseControlledPercentage = 50
		},
		shaanxi_region = {
			japaneseControlledPercentage = 50
		},
		utang_region = {
			japaneseControlledPercentage = 50
		}
	}
}

package.preload["overlay_gui"] = function()
	return {
		DisplayRegionNames = {
			"hubei_region",
			"henan_region",
			"shaanxi_region",
			"utang_region"
		},
		Tick = function()
			return mockState
		end
	}
end

local GuiActionBridge = require("gui_action_bridge")
local WarMapAdapter = require("war_map_adapter")

local function leaderPayload(itemId, leaderType)
	return {
		itemId = itemId,
		parameters = {
			leadertype = leaderType,
			role = leaderType
		}
	}
end

local function assertEligibility(
	viewerTag,
	regionName,
	percentage,
	combatMilitary,
	combatAdministrative,
	mapMilitary,
	mapAdministrative
)
	local result = WarMapAdapter.GetAppointmentEligibility(
		viewerTag,
		regionName,
		percentage
	)
	assert(result.combatMilitary == combatMilitary)
	assert(result.combatAdministrative == combatAdministrative)
	assert(result.mapMilitary == mapMilitary)
	assert(result.mapAdministrative == mapAdministrative)
end

assertEligibility("CHI", "hubei_region", 50, true, true, false, false)
assertEligibility("CHI", "hubei_region", 95, false, false, false, false)
assertEligibility("CHI", "shaanxi_region", 50, false, false, false, false)
assertEligibility("CHI", "utang_region", 50, false, false, false, false)

assertEligibility("CHC", "hubei_region", 50, true, true, false, false)
assertEligibility("CHC", "hubei_region", 95, false, false, true, false)
assertEligibility("CHC", "utang_region", 50, false, false, false, false)
assertEligibility("CHC", "utang_region", 95, false, false, false, false)

assertEligibility("JAP", "hubei_region", 50, true, true, false, false)
assertEligibility("JAP", "hubei_region", 95, false, false, false, true)
assertEligibility("JAP", "utang_region", 95, false, false, false, true)

assert(GuiActionBridge.Dispatch("select_war_map_region", {
	itemId = 1
}))
assert(not GuiActionBridge.Dispatch(
	"assign_war_map_leader",
	leaderPayload(1, "military")
))

assert(GuiActionBridge.Dispatch("select_combat_region", {
	itemId = 1
}))
assert(GuiActionBridge.Dispatch(
	"assign_war_map_leader",
	leaderPayload(1, "military")
))
assert(GuiActionBridge.Dispatch(
	"assign_war_map_leader",
	leaderPayload(2, "administrative")
))
assert(not GuiActionBridge.Dispatch(
	"assign_war_map_leader",
	leaderPayload(3, "military")
))

local snapshot = WarMapAdapter.BuildSnapshot()
local assigned = snapshot.lists.assigned_leader_list.items
assert(#assigned == 2)
assert(assigned[1].regionid == 1)
assert(assigned[1].leadertype == "military")
assert(assigned[1].assignmentorder == 1)
assert(assigned[2].regionid == 1)
assert(assigned[2].leadertype == "administrative")
assert(assigned[2].assignmentorder == 2)

assert(GuiActionBridge.Dispatch("move_war_map_leader", {
	itemId = 2,
	parameters = {
		normalizedx = 0.35,
		normalizedy = 0.45
	}
}))
snapshot = WarMapAdapter.BuildSnapshot()
assigned = snapshot.lists.assigned_leader_list.items
assert(assigned[1].x == 0.35 and assigned[1].y == 0.45)
assert(assigned[2].x == 0.35 and assigned[2].y == 0.45)

assert(GuiActionBridge.Dispatch("select_combat_region", {
	itemId = 2
}))
assert(not GuiActionBridge.Dispatch(
	"assign_war_map_leader",
	leaderPayload(1, "military")
))
assert(GuiActionBridge.Dispatch(
	"assign_war_map_leader",
	leaderPayload(3, "military")
))

snapshot = WarMapAdapter.BuildSnapshot()
assigned = snapshot.lists.assigned_leader_list.items
assert(#assigned == 3)
assert(assigned[3].regionid == 2)
assert(assigned[3].leadertype == "military")

assert(GuiActionBridge.Dispatch("step_down_war_map_leader", {
	itemId = 1
}))
assert(GuiActionBridge.Dispatch("step_down_war_map_leader", {
	itemId = 2
}))
assert(GuiActionBridge.Dispatch("step_down_war_map_leader", {
	itemId = 3
}))
snapshot = WarMapAdapter.BuildSnapshot()
assert(#snapshot.lists.assigned_leader_list.items == 0)

print("CHI/CHC/JAP appointment policy matrix: passed")
print("Per-Region military/administrative slots: passed")
print("Grouped marker position and step-down: passed")
