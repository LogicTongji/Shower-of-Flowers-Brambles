local P = {}

local GuiActionBridge = require('gui_action_bridge')
local GuiDataBridge = require('gui_data_bridge')

P.Version = 1
P.ChannelName = "parliament"
P.TotalSeats = 120
P.Scenario = 5
P.Slider = 5
P.SelectedSeat = 0
P.Visible = true
P.Active = true
P.Revision = 0
P.Dirty = true
P.LastSnapshot = nil

P.RowCounts = { 12, 16, 19, 22, 24, 27 }
P.Scenarios = {
	{ nameKey = "PARLIAMENT_SCENARIO_1", seats = { 54, 30, 22, 14 } },
	{ nameKey = "PARLIAMENT_SCENARIO_2", seats = { 46, 32, 25, 17 } },
	{ nameKey = "PARLIAMENT_SCENARIO_3", seats = { 38, 35, 28, 19 } },
	{ nameKey = "PARLIAMENT_SCENARIO_4", seats = { 34, 32, 29, 25 } },
	{ nameKey = "PARLIAMENT_SCENARIO_5", seats = { 30, 30, 30, 30 } },
	{ nameKey = "PARLIAMENT_SCENARIO_6", seats = { 25, 29, 32, 34 } },
	{ nameKey = "PARLIAMENT_SCENARIO_7", seats = { 19, 28, 35, 38 } },
	{ nameKey = "PARLIAMENT_SCENARIO_8", seats = { 17, 25, 32, 46 } },
	{ nameKey = "PARLIAMENT_SCENARIO_9", seats = { 14, 22, 30, 54 } }
}

local function ClampInteger(value, minimum, maximum)
	value = math.floor(tonumber(value) or minimum)
	return math.max(minimum, math.min(maximum, value))
end

local function ActionItemId(payload)
	payload = payload or {}
	local parameters = payload.parameters or {}
	return tonumber(
		payload.listItemId
		or payload.itemId
		or parameters.listItemId
		or parameters.itemId
	)
end

local function ActionParameter(payload, name)
	payload = payload or {}
	local parameters = payload.parameters or {}
	return parameters[name]
		or parameters[string.lower(name)]
		or payload[name]
end

local function BuildPoliticalSeatOrder()
	local seats = {}
	local seatId = 1

	for row, count in ipairs(P.RowCounts) do
		for index = 0, count - 1 do
			table.insert(seats, {
				id = seatId,
				position = index / (count - 1),
				row = row
			})
			seatId = seatId + 1
		end
	end

	table.sort(seats, function(first, second)
		if first.position == second.position then
			return first.row > second.row
		end
		return first.position < second.position
	end)

	local order = {}
	for index, seat in ipairs(seats) do
		order[index] = seat.id
	end
	return order
end

P.PoliticalSeatOrder = BuildPoliticalSeatOrder()

local function AssignScenarioSeats(values, scenarioIndex, scenario)
	local prefix = "scenarios." .. tostring(scenarioIndex) .. "."
	values[prefix .. "namekey"] = scenario.nameKey

	local total = 0
	for partyIndex, seatCount in ipairs(scenario.seats) do
		seatCount = ClampInteger(seatCount, 0, P.TotalSeats)
		values[prefix .. "party" .. tostring(partyIndex)] = seatCount
		total = total + seatCount
	end

	if total ~= P.TotalSeats then
		return false
	end

	local position = 1
	for partyIndex, seatCount in ipairs(scenario.seats) do
		for _ = 1, seatCount do
			local seatId = P.PoliticalSeatOrder[position]
			values[
				prefix .. "seats." .. tostring(seatId)
			] = partyIndex
			position = position + 1
		end
	end

	return position == P.TotalSeats + 1
end

local function SelectScenario(payload)
	local scenario = ActionItemId(payload)
	if not scenario or scenario < 1 or scenario > #P.Scenarios then
		return false
	end

	P.Scenario = scenario
	P.Slider = scenario
	P.Dirty = true
	return true
end

local function DragScenario(payload)
	local scenario = tonumber(ActionParameter(payload, "stepindex"))
	local slider = tonumber(ActionParameter(payload, "value"))
	if not scenario or scenario < 1 or scenario > #P.Scenarios then
		return false
	end

	P.Scenario = ClampInteger(scenario, 1, #P.Scenarios)
	P.Slider = math.max(1, math.min(#P.Scenarios, slider or scenario))
	P.Dirty = true
	return true
end

local function SelectSeat(payload)
	local seatId = ActionItemId(payload)
	if not seatId or seatId < 1 or seatId > P.TotalSeats then
		return false
	end

	P.SelectedSeat = seatId
	P.Dirty = true
	return true
end

GuiActionBridge.Register("select_parliament_scenario", SelectScenario)
GuiActionBridge.Register("drag_parliament_scenario", DragScenario)
GuiActionBridge.Register("select_parliament_seat", SelectSeat)

function P.SetVisible(visible)
	P.Visible = visible == true
	P.Dirty = true
end

function P.SetActive(active)
	P.Active = active == true
	P.Dirty = true
end

function P.SetScenario(scenario)
	return SelectScenario({ itemId = scenario })
end

function P.BuildSnapshot()
	local values = {
		["state.visible"] = P.Visible,
		["state.active"] = P.Active,
		["parliament.scenario"] = P.Scenario,
		["parliament.slider"] = P.Slider,
		["parliament.selectedseat"] = P.SelectedSeat,
		["parliament.totalseats"] = P.TotalSeats
	}

	for scenarioIndex, scenario in ipairs(P.Scenarios) do
		if not AssignScenarioSeats(values, scenarioIndex, scenario) then
			return nil
		end
	end

	local seatItems = {}
	for seatId = 1, P.TotalSeats do
		table.insert(seatItems, {
			id = seatId,
			text = ""
		})
	end

	P.Revision = P.Revision + 1
	return {
		version = 2,
		revision = P.Revision,
		fullSnapshot = true,
		values = values,
		lists = {
			parliament_seat_list = {
				revision = P.Revision,
				items = seatItems
			}
		}
	}
end

function P.PublishSnapshot(snapshot)
	snapshot = snapshot or P.BuildSnapshot()
	if not snapshot then
		return false
	end

	P.LastSnapshot = snapshot
	P.Dirty = false
	return GuiDataBridge.PublishSnapshot(P.ChannelName, snapshot)
end

function P.Tick()
	GuiDataBridge.DispatchActions(P.ChannelName, 64)

	if P.Dirty or not P.LastSnapshot then
		local snapshot = P.BuildSnapshot()
		P.LastSnapshot = snapshot
		P.Dirty = false
		if snapshot then
			GuiDataBridge.PublishSnapshot(P.ChannelName, snapshot)
		end
	end

	return P.LastSnapshot
end

return P
