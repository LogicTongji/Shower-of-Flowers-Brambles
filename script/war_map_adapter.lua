local P = {}

local ChinaWarMap = require('overlay_gui')

P.Version = 3
P.RegionNames = {}
P.LastDay = nil
P.LastSnapshot = nil

local function BuildUniqueRegionNames()
	local names = {}
	local seen = {}

	for _, regionName in ipairs(ChinaWarMap.DisplayRegionNames) do
		if not seen[regionName] then
			seen[regionName] = true
			table.insert(names, regionName)
		end
	end

	return names
end

P.RegionNames = BuildUniqueRegionNames()

function P.BuildSnapshot()
	local state = ChinaWarMap.Tick()
	local snapshot = {
		version = P.Version,
		date = state.date or 0,
		visible = state.visible == true,
		active = state.active == true,
		playerTag = state.playerTag,
		regionCount = #P.RegionNames,
		percentages = {},
		populations = {},
		warProgress = state.warProgress
	}

	for regionId, regionName in ipairs(P.RegionNames) do
		local regionState =
			(state.regions or {})[regionName]
		local percentage = 0
		local population = {
			known = false,
			total = 0,
			affected = 0,
			remaining = 0
		}

		if regionState then
			percentage = tonumber(
				regionState.japaneseControlledPercentage
			) or 0

			if regionState.population then
				population.known =
					regionState.population.known == true
				population.total = tonumber(
					regionState.population.total
				) or 0
				population.affected = tonumber(
					regionState.population.affected
				) or 0
				population.remaining = tonumber(
					regionState.population.remaining
				) or 0
			end
		end

		snapshot.percentages[regionId] = percentage
		snapshot.populations[regionId] = population
	end

	return snapshot
end

function P.PublishSnapshot(snapshot)
	local bridge = rawget(_G, 'WarMapBridge')

	if type(bridge) == 'table'
		and type(bridge.PublishSnapshot) == 'function' then
		local success = pcall(
			bridge.PublishSnapshot,
			snapshot
		)

		return success
	end

	local publishFunction = rawget(
		_G,
		'WarMapBridge_PublishSnapshot'
	)

	if type(publishFunction) == 'function' then
		local success = pcall(publishFunction, snapshot)
		return success
	end

	return false
end

function P.Tick()
	local currentDay = CCurrentGameState.GetCurrentDate():GetTotalDays()

	if P.LastSnapshot and P.LastDay == currentDay then
		return P.LastSnapshot
	end

	P.LastDay = currentDay
	P.LastSnapshot = P.BuildSnapshot()
	P.PublishSnapshot(P.LastSnapshot)

	return P.LastSnapshot
end

function P.GetSnapshot()
	if P.LastSnapshot then
		return P.LastSnapshot
	end

	return P.Tick()
end

function P.GetRegionName(regionId)
	return P.RegionNames[regionId]
end

return P
