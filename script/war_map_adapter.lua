local P = {}

local ChinaWarMap = require('overlay_gui')
local GuiActionBridge = require('gui_action_bridge')
local GuiDataBridge = require('gui_data_bridge')

P.Version = 7
P.RegionNames = {}
P.LastDay = nil
P.LastSnapshot = nil
P.SelectedRegionId = 0
P.SelectedRegionSource = "none"
P.Revision = 0
P.ChannelName = "china_anti_jap"
P.LeaderAssignments = {}
P.NextAssignmentOrder = 1
P.LeaderTypes = {
	MILITARY = "military",
	ADMINISTRATIVE = "administrative"
}

P.Leaders = {
	{
		id = 1,
		leaderId = "li_zongren",
		leaderType = P.LeaderTypes.MILITARY,
		role = P.LeaderTypes.MILITARY,
		textKey = "WARMAP_LEADER_LIST_LI_ZONGREN",
		portrait = "GFX_warmap_leader_li_zongren",
		nameKey = "WARMAP_LEADER_LI_ZONGREN",
		descriptionKey = "WARMAP_LEADER_LI_ZONGREN_DESC"
	}
}

P.LeaderButtonSprites = {
	CHI = "GFX_warmap_leader_button_chi",
	CHC = "GFX_warmap_leader_button_chc",
	JAP = "GFX_warmap_leader_button_jap"
}

P.LeaderEnabledConditions = {
	military = "selectedregion.source == combat && regions.{selectedregion.id}.combatmilitaryeligible || selectedregion.source == map && regions.{selectedregion.id}.mapmilitaryeligible",
	administrative = "selectedregion.source == combat && regions.{selectedregion.id}.combatadministrativeeligible || selectedregion.source == map && regions.{selectedregion.id}.mapadministrativeeligible"
}

P.RegionDisplayNames = {
	guangdong_region = "广东省",
	shanxi_region = "山西省",
	yunnan_region = "云南省",
	guangxi_region = "广西省",
	xikang_region = "西康省",
	ningxia_region = "宁夏省",
	gansu_region = "甘肃省",
	qinghai_region = "青海省",
	chahar_region = "察哈尔省",
	suiyuan_region = "绥远省",
	sichuan_region = "四川省",
	guizhou_region = "贵州省",
	shaanxi_region = "陕西省",
	hebei_region = "河北省",
	east_hebei_region = "东河北地区",
	shandong_region = "山东省",
	fujian_region = "福建省",
	hunan_region = "湖南省",
	jiangsu_region = "江苏省",
	jiangxi_region = "江西省",
	chekiang_region = "浙江省",
	anhui_region = "安徽省",
	henan_region = "河南省",
	hubei_region = "湖北省",
	xingan_region = "兴安省",
	rehe_region = "热河省",
	fengtian_region = "奉天省",
	liaonning_region = "辽宁省",
	andong_region = "安东省",
	nenjiang_region = "嫩江省",
	heihe_region = "黑河省",
	heilongjiang_region = "黑龙江省",
	songjiang_region = "松江省",
	jiandao_region = "间岛省",
	jilin_region = "吉林省",
	xinjiang_region = "新疆省",
	utang_region = "卫藏地方",
	taiwan_region = "台湾省",
	Mongolia_Regions = "蒙古地方",
	SF_Shanghai = "上海市",
	tannuuriankhai_region = "唐努乌梁海"
}

P.RegionCapitals = {
	guangdong_region = "广州",
	shanxi_region = "太原",
	yunnan_region = "昆明",
	guangxi_region = "南宁",
	xikang_region = "康定",
	ningxia_region = "银川",
	gansu_region = "兰州",
	qinghai_region = "西宁",
	chahar_region = "张家口",
	suiyuan_region = "呼和浩特",
	sichuan_region = "成都",
	guizhou_region = "贵阳",
	shaanxi_region = "西安",
	hebei_region = "保定",
	east_hebei_region = "待补充",
	shandong_region = "济南",
	fujian_region = "福州",
	hunan_region = "长沙",
	jiangsu_region = "南京",
	jiangxi_region = "南昌",
	chekiang_region = "杭州",
	anhui_region = "合肥",
	henan_region = "开封",
	hubei_region = "武汉",
	xingan_region = "海拉尔",
	rehe_region = "承德",
	fengtian_region = "沈阳",
	liaonning_region = "沈阳",
	andong_region = "安东",
	nenjiang_region = "齐齐哈尔",
	heihe_region = "黑河",
	heilongjiang_region = "哈尔滨",
	songjiang_region = "哈尔滨",
	jiandao_region = "延吉",
	jilin_region = "吉林",
	xinjiang_region = "迪化",
	utang_region = "拉萨",
	taiwan_region = "台北",
	Mongolia_Regions = "乌兰巴托",
	SF_Shanghai = "上海",
	tannuuriankhai_region = "乌里雅苏台"
}

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

local function FormatPopulation(value, prefix)
	value = math.max(0, math.floor((tonumber(value) or 0) + 0.5))

	if value >= 10000 then
		return prefix .. string.format("%.1f 万", value / 10000)
	end

	return prefix .. tostring(value)
end

local function FormatPercentage(value, prefix)
	return prefix .. string.format("%.1f%%", tonumber(value) or 0)
end

local function SelectRegion(payload)
	payload = payload or {}
	local parameters = payload.parameters or {}
	local regionId = tonumber(
		payload.listItemId
		or payload.itemId
		or payload.regionId
		or parameters.regionId
		or parameters.region_id
	)

	if not regionId
		or regionId < 1
		or regionId > #P.RegionNames then
		return false
	end

	P.SelectedRegionId = math.floor(regionId)
	P.SelectedRegionSource = tostring(
		parameters.source
		or parameters.selectionsource
		or (payload.action == "select_combat_region"
			and "combat" or "map")
	)
	P.LastDay = nil
	return true
end

local function IsForbiddenRegion(viewerTag, regionName)
	if regionName == "utang_region" then
		return viewerTag == "CHI" or viewerTag == "CHC"
	end

	return viewerTag == "CHI"
		and regionName == "shaanxi_region"
end

function P.GetAppointmentEligibility(
	viewerTag,
	regionName,
	percentage
)
	local eligibility = {
		combatMilitary = false,
		combatAdministrative = false,
		mapMilitary = false,
		mapAdministrative = false
	}
	percentage = tonumber(percentage) or 0

	if IsForbiddenRegion(viewerTag, regionName) then
		return eligibility
	end

	if percentage > 0 and percentage < 90 then
		eligibility.combatMilitary = true
		eligibility.combatAdministrative = true
	end

	if percentage >= 90 then
		if viewerTag == "CHC" then
			eligibility.mapMilitary = true
		elseif viewerTag == "JAP" then
			eligibility.mapAdministrative = true
		end
	end

	return eligibility
end

local function CanAssignLeader(leader, state)
	local leaderType = leader.leaderType or leader.role
	local regionName = P.RegionNames[P.SelectedRegionId]
	local regionState = regionName
		and (state.regions or {})[regionName] or nil
	local percentage = regionState
		and tonumber(regionState.japaneseControlledPercentage) or 0
	local viewerTag = state.playerTag or ""
	local eligibility = P.GetAppointmentEligibility(
		viewerTag,
		regionName,
		percentage
	)

	if P.SelectedRegionSource == "combat" then
		return leaderType == "military"
			and eligibility.combatMilitary
			or leaderType == "administrative"
			and eligibility.combatAdministrative
	end
	if P.SelectedRegionSource == "map" then
		return leaderType == "military"
			and eligibility.mapMilitary
			or leaderType == "administrative"
			and eligibility.mapAdministrative
	end

	return false
end

local function IsLeaderSlotOccupied(regionId, leaderType)
	for _, assignment in pairs(P.LeaderAssignments) do
		if assignment.regionId == regionId
			and assignment.leaderType == leaderType then
			return true
		end
	end

	return false
end

local function FindRegionMarkerPosition(regionId)
	for _, assignment in pairs(P.LeaderAssignments) do
		if assignment.regionId == regionId then
			return assignment.x, assignment.y
		end
	end

	return nil, nil
end

local function GetLeader(payload)
	payload = payload or {}
	local parameters = payload.parameters or {}
	local leaderId = tostring(
		parameters.leaderid
		or parameters.leaderId
		or ""
	)
	local numericId = tonumber(
		payload.listItemId
		or payload.itemId
		or parameters.id
	)

	for _, leader in ipairs(P.Leaders) do
		if leader.leaderId == leaderId
			or leader.id == numericId then
			return leader
		end
	end

	return nil
end

local function AssignLeader(payload)
	local leader = GetLeader(payload)
	local state = ChinaWarMap.Tick()
	local leaderType = leader
		and (leader.leaderType or leader.role) or nil
	if not leader
		or P.SelectedRegionId <= 0
		or P.LeaderAssignments[leader.id]
		or IsLeaderSlotOccupied(P.SelectedRegionId, leaderType)
		or not CanAssignLeader(leader, state) then
		return false
	end

	local x, y = FindRegionMarkerPosition(P.SelectedRegionId)
	P.LeaderAssignments[leader.id] = {
		regionId = P.SelectedRegionId,
		leaderType = leaderType,
		assignmentOrder = P.NextAssignmentOrder,
		x = x,
		y = y
	}
	P.NextAssignmentOrder = P.NextAssignmentOrder + 1
	P.LastDay = nil
	return true
end

local function StepDownLeader(payload)
	local leader = GetLeader(payload)
	if not leader or not P.LeaderAssignments[leader.id] then
		return false
	end

	P.LeaderAssignments[leader.id] = nil
	P.LastDay = nil
	return true
end

local function MoveLeader(payload)
	local leader = GetLeader(payload)
	local parameters = payload and payload.parameters or {}
	local assignment = leader
		and P.LeaderAssignments[leader.id] or nil
	local x = tonumber(
		parameters.normalizedx
		or parameters.normalizedX
	)
	local y = tonumber(
		parameters.normalizedy
		or parameters.normalizedY
	)

	if not assignment or not x or not y then
		return false
	end

	x = math.max(0, math.min(1, x))
	y = math.max(0, math.min(1, y))
	for _, regionAssignment in pairs(P.LeaderAssignments) do
		if regionAssignment.regionId == assignment.regionId then
			regionAssignment.x = x
			regionAssignment.y = y
		end
	end
	P.LastDay = nil
	return true
end

GuiActionBridge.Register("select_combat_region", SelectRegion)
GuiActionBridge.Register("select_war_map_region", SelectRegion)
GuiActionBridge.Register("assign_war_map_leader", AssignLeader)
GuiActionBridge.Register("move_war_map_leader", MoveLeader)
GuiActionBridge.Register("step_down_war_map_leader", StepDownLeader)

function P.BuildSnapshot()
	local state = ChinaWarMap.Tick()
	local viewerTag = state.playerTag or ""
	local controlledPopulationPrefix = viewerTag == "JAP"
		and "控制人口："
		or "沦陷人口："
	local controlledPercentagePrefix = viewerTag == "JAP"
		and "控制程度："
		or "沦陷程度："
	P.Revision = P.Revision + 1
	local snapshot = {
		version = P.Version,
		revision = P.Revision,
		baseRevision = 0,
		fullSnapshot = true,
		date = state.date or 0,
		visible = state.visible == true,
		active = state.active == true,
		playerTag = viewerTag,
		regionCount = #P.RegionNames,
		percentages = {},
		populations = {},
		warProgress = state.warProgress,
		values = {},
		lists = {
			combat_region_list = {
				revision = state.date or P.Revision,
				items = {}
			},
			leader_candidate_list = {
				revision = P.Revision,
				items = {}
			},
			assigned_leader_list = {
				revision = P.Revision,
				items = {}
			}
		}
	}

	snapshot.values["state.visible"] = snapshot.visible
	snapshot.values["state.active"] = snapshot.active
	snapshot.values["state.viewertag"] = viewerTag
	snapshot.values["state.date"] = snapshot.date
	snapshot.values["selectedregion.id"] = P.SelectedRegionId
	snapshot.values["selectedregion.source"] = P.SelectedRegionSource
	snapshot.values["warProgress.known"] =
		type(snapshot.warProgress) == "table"
		and snapshot.warProgress.known == true
	snapshot.values["warProgress.own"] =
		type(snapshot.warProgress) == "table"
		and tonumber(snapshot.warProgress.own) or 0
	snapshot.values["warProgress.enemy"] =
		type(snapshot.warProgress) == "table"
		and tonumber(snapshot.warProgress.enemy) or 0

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

		local dataPrefix = "regions." .. regionId .. "."
		snapshot.values[dataPrefix .. "name"] =
			P.RegionDisplayNames[regionName] or regionName
		snapshot.values[dataPrefix .. "capital"] =
			"省会：" .. (P.RegionCapitals[regionName] or "待补充")
		snapshot.values[dataPrefix .. "totalPopulation"] =
			FormatPopulation(population.total, "总人口：")
		snapshot.values[dataPrefix .. "affectedPopulation"] =
			FormatPopulation(
				population.affected,
				controlledPopulationPrefix
			)
		snapshot.values[dataPrefix .. "remainingPopulation"] =
			FormatPopulation(population.remaining, "剩余人口：")
		snapshot.values[dataPrefix .. "controlledPercentage"] =
			percentage
		local eligibility = P.GetAppointmentEligibility(
			viewerTag,
			regionName,
			percentage
		)
		snapshot.values[dataPrefix .. "combatMilitaryEligible"] =
			eligibility.combatMilitary
		snapshot.values[
			dataPrefix .. "combatAdministrativeEligible"
		] = eligibility.combatAdministrative
		snapshot.values[dataPrefix .. "mapMilitaryEligible"] =
			eligibility.mapMilitary
		snapshot.values[
			dataPrefix .. "mapAdministrativeEligible"
		] = eligibility.mapAdministrative
		snapshot.values[
			dataPrefix .. "combatAppointmentEligible"
		] = eligibility.combatMilitary
			or eligibility.combatAdministrative
		snapshot.values[
			dataPrefix .. "mapAppointmentEligible"
		] = eligibility.mapMilitary
			or eligibility.mapAdministrative
		snapshot.values[
			dataPrefix .. "controlledPercentageText"
		] = FormatPercentage(
			percentage,
			controlledPercentagePrefix
		)

		if snapshot.active
			and percentage > 0
			and percentage < 90 then
			table.insert(
				snapshot.lists.combat_region_list.items,
				{
					id = regionId,
					text = P.RegionDisplayNames[
						regionName
					] or regionName,
					regionName = regionName,
					percentage = percentage
				}
			)
		end
	end

	for _, leader in ipairs(P.Leaders) do
		local leaderType = leader.leaderType or leader.role
		local buttonSprite = P.LeaderButtonSprites[viewerTag]
			or P.LeaderButtonSprites.CHI
		table.insert(
			snapshot.lists.leader_candidate_list.items,
			{
				id = leader.id,
				text = leader.textKey,
				textkey = leader.textKey,
				leaderid = leader.leaderId,
				role = leaderType,
				leadertype = leaderType,
				buttonsprite = buttonSprite,
				enabledwhen = P.LeaderEnabledConditions[leaderType],
				portrait = leader.portrait,
				namekey = leader.nameKey,
				descriptionkey = leader.descriptionKey
			}
		)

		local assignment = P.LeaderAssignments[leader.id]
		if assignment then
			table.insert(
				snapshot.lists.assigned_leader_list.items,
				{
					id = leader.id,
					text = leader.textKey,
					textkey = leader.textKey,
					leaderid = leader.leaderId,
					role = assignment.leaderType,
					leadertype = assignment.leaderType,
					portrait = leader.portrait,
					namekey = leader.nameKey,
					descriptionkey = leader.descriptionKey,
					regionid = assignment.regionId,
					assignmentorder = assignment.assignmentOrder,
					x = assignment.x or -1,
					y = assignment.y or -1
				}
			)
		end
	end

	return snapshot
end

function P.PublishSnapshot(snapshot)
	return GuiDataBridge.PublishSnapshot(
		P.ChannelName,
		snapshot
	)
end

function P.Tick()
	GuiDataBridge.DispatchActions(P.ChannelName, 64)

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
