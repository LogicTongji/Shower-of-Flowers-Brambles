local root = arg[1] or "."

package.path = table.concat({
	root .. "/script/?.lua",
	package.path
}, ";")

local ParliamentGui = require("parliament_gui")
local GuiActionBridge = require("gui_action_bridge")

local snapshot = assert(ParliamentGui.BuildSnapshot())
assert(snapshot.values["parliament.scenario"] == 5)
assert(snapshot.values["parliament.slider"] == 5)
assert(#snapshot.lists.parliament_seat_list.items == 120)
assert(snapshot.values["scenarios.5.party1"] == 30)
assert(snapshot.values["scenarios.5.party2"] == 30)
assert(snapshot.values["scenarios.5.party3"] == 30)
assert(snapshot.values["scenarios.5.party4"] == 30)

for scenarioIndex = 1, 9 do
	local counts = { 0, 0, 0, 0 }
	for seatId = 1, ParliamentGui.TotalSeats do
		local party = snapshot.values[
			"scenarios."
			.. tostring(scenarioIndex)
			.. ".seats."
			.. tostring(seatId)
		]
		assert(party and party >= 1 and party <= 4)
		counts[party] = counts[party] + 1
	end

	for partyIndex = 1, 4 do
		assert(
			counts[partyIndex]
			== snapshot.values[
				"scenarios."
				.. tostring(scenarioIndex)
				.. ".party"
				.. tostring(partyIndex)
			]
		)
	end
end

assert(GuiActionBridge.Dispatch("select_parliament_scenario", {
	listItemId = 9
}))
assert(ParliamentGui.Scenario == 9)
assert(ParliamentGui.Slider == 9)
assert(GuiActionBridge.Dispatch("drag_parliament_scenario", {
	parameters = {
		stepindex = "3",
		value = "3.375"
	}
}))
assert(ParliamentGui.Scenario == 3)
assert(ParliamentGui.Slider == 3.375)
assert(GuiActionBridge.Dispatch("select_parliament_seat", {
	listItemId = 120
}))
assert(ParliamentGui.SelectedSeat == 120)
assert(not GuiActionBridge.Dispatch("select_parliament_seat", {
	listItemId = 121
}))

print("Parliament scenarios: 9")
print("Parliament seats: 120")
print("Four-party allocation: passed")
print("Scenario and seat actions: passed")
