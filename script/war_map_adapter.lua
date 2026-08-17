local P = {}

local ChinaWarMap = require('overlay_gui')
local GuiActionBridge = require('gui_action_bridge')
local GuiDataBridge = require('gui_data_bridge')
local GuiPersistence = require('scripted_gui_persistence')

P.Version = 8
P.RegionNames = {}
P.LastDay = nil
P.LastSnapshot = nil
P.SelectedRegionId = 0
P.SelectedRegionSource = "none"
P.WindowOpen = false
P.Revision = 0
P.ChannelName = "china_anti_jap"
P.SessionId = ""
P.PersistenceKey = ""
P.PersistenceNamespace = "china_war"
P.RuntimeContext = nil
P.PersistenceAvailable = false
P.PersistenceRevision = 0
P.LastPersistedRevision = 0
P.LastObservedGameDay = nil
P.PendingPersistenceRevision = nil
P.PendingProfileToken = nil
P.PendingProfileTicks = 0
P.PersistenceDirty = false
P.PublisherOrdinal = ""
P.SessionGeneration = 0
P.PersistenceAckTimeoutTicks = 8
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
	},
	{
		id = 2,
		leaderId = "ju_zheng",
		leaderType = P.LeaderTypes.ADMINISTRATIVE,
		role = P.LeaderTypes.ADMINISTRATIVE,
		textKey = "WARMAP_LEADER_LIST_JU_ZHENG",
		portrait = "GFX_warmap_officer_ju_zheng",
		nameKey = "WARMAP_LEADER_JU_ZHENG",
		descriptionKey = "WARMAP_LEADER_JU_ZHENG_DESC"
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

local function OpenWarMap()
	P.WindowOpen = true
	P.LastDay = nil
	return true
end

local function CloseWarMap()
	P.WindowOpen = false
	P.LastDay = nil
	return true
end

GuiActionBridge.Register(
	"open_china_anti_jap_warmap",
	OpenWarMap
)
GuiActionBridge.Register(
	"close_china_anti_jap_warmap",
	CloseWarMap
)

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
	snapshot.values["state.sessionid"] = P.SessionId
	snapshot.values["state.persistencekey"] = P.PersistenceKey
	snapshot.values["state.persistenceavailable"] =
		P.PersistenceAvailable
	snapshot.values["state.persistenceerror"] =
		GuiPersistence.GetLastError()
	snapshot.values["state.persistencerevision"] =
		P.PersistenceRevision
	snapshot.values["state.persistedrevision"] =
		P.LastPersistedRevision
	snapshot.values["state.persistencependingrevision"] =
		P.PendingPersistenceRevision
		and P.PendingPersistenceRevision.next or 0
	snapshot.values["state.persistencependingticks"] =
		P.PendingPersistenceRevision
		and P.PendingPersistenceRevision.ticks or 0
	snapshot.values["state.persistenceobservedday"] =
		P.LastObservedGameDay or -1
	for _, leader in ipairs(P.Leaders) do
		local assignment = P.LeaderAssignments[leader.id]
		snapshot.values[
			"state.leader" .. tostring(leader.id) .. "region"
		] = assignment and assignment.regionId or 0
	end
	snapshot.values["state.windowopen"] = P.WindowOpen
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

local function NormalizePersistenceRevision(value)
	return math.max(0, math.floor(tonumber(value) or 0))
end

local function AdvanceSession()
	P.SessionGeneration = P.SessionGeneration + 1
	P.SessionId = (P.PersistenceKey ~= ""
		and P.PersistenceKey or "unbound")
		.. ":reload:" .. tostring(P.SessionGeneration)
end

local function PreparePersistenceContext(context)
	if type(context) ~= "table" then
		return context
	end
	if context.persistenceTagObject and context.persistenceCountry then
		return context
	end

	local playerTagObject = context.playerTagObject
	local success, playerCountry = pcall(function()
		return playerTagObject:GetCountry()
	end)
	if not success then
		success, playerTagObject, playerCountry = pcall(function()
			local tag = CCountryDataBase.GetTag(context.playerTag)
			return tag, tag:GetCountry()
		end)
	end
	if success and playerTagObject and playerCountry then
		context.persistenceTagObject = playerTagObject
		context.persistenceCountry = playerCountry
	end
	return context
end

local function SynchronizePersistence(context)
	context = PreparePersistenceContext(context)
	if type(context) ~= "table" or P.PersistenceKey == "" then
		return false
	end

	local currentDay = math.floor(
		tonumber(context.currentDay) or 0
	)
	local dayRolledBack = P.LastObservedGameDay ~= nil
		and currentDay < P.LastObservedGameDay
	P.LastObservedGameDay = currentDay

	local profileKey, profileAvailable, profileToken =
		GuiPersistence.ReadProfileKey(
			context,
			P.PersistenceNamespace
		)
	local persistedRevision, revisionAvailable =
		GuiPersistence.ReadNumber(
			context,
			P.PersistenceNamespace,
			"revision",
			0
		)
	if not profileAvailable or not revisionAvailable then
		P.PersistenceAvailable = false
		return false
	end

	P.PersistenceAvailable = true
	persistedRevision = NormalizePersistenceRevision(
		persistedRevision
	)
	P.LastPersistedRevision = persistedRevision
	if dayRolledBack then
		P.PendingProfileToken = nil
		P.PendingProfileTicks = 0
		P.PendingPersistenceRevision = nil
	end
	if P.PendingProfileToken then
		if profileToken == P.PendingProfileToken then
			P.PendingProfileToken = nil
			P.PendingProfileTicks = 0
		elseif profileToken <= 0 then
			P.PendingProfileTicks = P.PendingProfileTicks + 1
			if P.PendingProfileTicks
				< P.PersistenceAckTimeoutTicks then
				return false
			end
			P.PendingProfileToken = nil
		else
			P.PendingProfileToken = nil
			P.PendingProfileTicks = 0
		end
	end

	local externalState = dayRolledBack
		or profileKey ~= P.PersistenceKey
	local pendingRevision = P.PendingPersistenceRevision
	if not externalState and pendingRevision then
		if persistedRevision == pendingRevision.next then
			P.PersistenceRevision = pendingRevision.next
			P.PendingPersistenceRevision = nil
		elseif persistedRevision == pendingRevision.previous then
			pendingRevision.ticks = pendingRevision.ticks + 1
			if pendingRevision.ticks
				< P.PersistenceAckTimeoutTicks then
				return false
			end
			externalState = true
		else
			externalState = true
		end
	elseif not externalState
		and persistedRevision ~= P.PersistenceRevision then
		externalState = true
	end

	if not externalState then
		return false
	end

	GuiPersistence.ResetWriteCache()
	local restored = P.RestoreState(context)
	if restored then
		AdvanceSession()
	end
	return restored
end

function P.PumpActions(context, maximumActions)
	P.RuntimeContext = PreparePersistenceContext(
		context or P.RuntimeContext
	)
	local restored = SynchronizePersistence(P.RuntimeContext)
	if restored
		and type(P.RuntimeContext) == "table"
		and type(P.RuntimeContext.requestRefresh) == "function" then
		P.RuntimeContext.requestRefresh()
	end

	local dispatched = GuiDataBridge.DispatchActions(
		P.ChannelName,
		maximumActions or 64
	)
	if dispatched > 0 then
		P.PersistenceDirty = true
	end
	if P.PersistenceDirty and not P.PendingPersistenceRevision then
		P.PersistState(P.RuntimeContext)
	end
	return dispatched
end

function P.RestoreState(context)
	context = PreparePersistenceContext(context)
	if type(context) ~= "table" then
		return false
	end
	P.RuntimeContext = context
	local profileKey, profileToken, profileCreated,
		profileWritten = GuiPersistence.EnsureProfileKey(
		context,
		P.PersistenceNamespace
	)
	P.PersistenceKey = profileKey
	if P.SessionId == "" then
		P.SessionId = P.PersistenceKey
	end
	P.PendingProfileToken = profileCreated and profileWritten
		and profileToken or nil
	P.PendingProfileTicks = 0
	local windowOpen, persistenceAvailable =
		GuiPersistence.ReadBoolean(
		context,
		P.PersistenceNamespace,
		"window_open",
		false
	)
	P.WindowOpen = windowOpen
	local persistedRevision, revisionAvailable =
		GuiPersistence.ReadNumber(
			context,
			P.PersistenceNamespace,
			"revision",
			0
		)
	P.PersistenceRevision = NormalizePersistenceRevision(
		persistedRevision
	)
	P.LastPersistedRevision = P.PersistenceRevision
	P.LastObservedGameDay = math.floor(
		tonumber(context.currentDay) or 0
	)
	P.PendingPersistenceRevision = nil
	P.PersistenceDirty = false
	P.PersistenceAvailable = persistenceAvailable
		and revisionAvailable
		and (not profileCreated or profileWritten)
	P.SelectedRegionId = math.max(0, math.floor(
		GuiPersistence.ReadNumber(
			context,
			P.PersistenceNamespace,
			"selected_region",
			0
		)
	))
	local sourceCode = math.floor(GuiPersistence.ReadNumber(
		context,
		P.PersistenceNamespace,
		"selected_source",
		0
	))
	P.SelectedRegionSource = sourceCode == 1 and "combat"
		or sourceCode == 2 and "map" or "none"
	P.LeaderAssignments = {}
	P.NextAssignmentOrder = 1
	for _, leader in ipairs(P.Leaders) do
		local prefix = "leader_" .. tostring(leader.id) .. "_"
		local regionId = math.floor(GuiPersistence.ReadNumber(
			context,
			P.PersistenceNamespace,
			prefix .. "region",
			0
		))
		if regionId > 0 and regionId <= #P.RegionNames then
			local assignmentOrder = math.max(1, math.floor(
				GuiPersistence.ReadNumber(
					context,
					P.PersistenceNamespace,
					prefix .. "order",
					P.NextAssignmentOrder
				)
			))
			P.LeaderAssignments[leader.id] = {
				leaderId = leader.leaderId,
				leaderType = leader.leaderType or leader.role,
				regionId = regionId,
				assignmentOrder = assignmentOrder,
				x = GuiPersistence.ReadNumber(
					context,
					P.PersistenceNamespace,
					prefix .. "x",
					-10000
				) / 10000,
				y = GuiPersistence.ReadNumber(
					context,
					P.PersistenceNamespace,
					prefix .. "y",
					-10000
				) / 10000
			}
			P.NextAssignmentOrder = math.max(
				P.NextAssignmentOrder,
				assignmentOrder + 1
			)
		end
	end
	P.LastDay = nil
	P.LastSnapshot = nil
	return true
end

function P.PersistState(context)
	context = PreparePersistenceContext(
		context or P.RuntimeContext
	)
	if type(context) ~= "table" then
		return false
	end
	P.RuntimeContext = context
	if not P.PersistenceDirty then
		return true
	end
	if P.PendingPersistenceRevision then
		return true
	end
	if P.PersistenceKey == "" then
		local profileKey, profileToken, profileCreated,
			profileWritten = GuiPersistence.EnsureProfileKey(
			context,
			P.PersistenceNamespace
		)
		P.PersistenceKey = profileKey
		P.PendingProfileToken = profileCreated and profileWritten
			and profileToken or nil
		P.PendingProfileTicks = 0
	end
	local success = true
	success = GuiPersistence.WriteBoolean(
		context,
		P.PersistenceNamespace,
		"window_open",
		P.WindowOpen
	) and success
	success = GuiPersistence.WriteNumber(
		context,
		P.PersistenceNamespace,
		"selected_region",
		P.SelectedRegionId
	) and success
	local sourceCode = P.SelectedRegionSource == "combat" and 1
		or P.SelectedRegionSource == "map" and 2 or 0
	success = GuiPersistence.WriteNumber(
		context,
		P.PersistenceNamespace,
		"selected_source",
		sourceCode
	) and success
	for _, leader in ipairs(P.Leaders) do
		local assignment = P.LeaderAssignments[leader.id]
		local prefix = "leader_" .. tostring(leader.id) .. "_"
		success = GuiPersistence.WriteNumber(
			context,
			P.PersistenceNamespace,
			prefix .. "region",
			assignment and assignment.regionId or 0
		) and success
		success = GuiPersistence.WriteNumber(
			context,
			P.PersistenceNamespace,
			prefix .. "order",
			assignment and assignment.assignmentOrder or 0
		) and success
		success = GuiPersistence.WriteNumber(
			context,
			P.PersistenceNamespace,
			prefix .. "x",
			assignment and math.floor((assignment.x or -1) * 10000)
				or -10000
		) and success
		success = GuiPersistence.WriteNumber(
			context,
			P.PersistenceNamespace,
			prefix .. "y",
			assignment and math.floor((assignment.y or -1) * 10000)
				or -10000
		) and success
	end
	local previousRevision = P.PersistenceRevision
	local nextRevision = previousRevision % 999999 + 1
	if success then
		success = GuiPersistence.WriteNumber(
			context,
			P.PersistenceNamespace,
			"revision",
			nextRevision
		)
	end
	if success then
		P.PendingPersistenceRevision = {
			previous = previousRevision,
			next = nextRevision,
			ticks = 0
		}
		P.PersistenceDirty = false
	end
	P.PersistenceAvailable = success
	return success
end

function P.ShouldRefresh(context)
	local currentDay = context and context.currentDay
		or CCurrentGameState.GetCurrentDate():GetTotalDays()
	return not P.LastSnapshot or P.LastDay ~= currentDay
end

function P.BuildUpdate(context)
	P.RuntimeContext = context or P.RuntimeContext
	if P.PersistenceKey == "" and P.RuntimeContext then
		P.PersistenceKey = GuiPersistence.EnsureProfileKey(
			P.RuntimeContext,
			P.PersistenceNamespace
		)
		if P.SessionId == "" then
			P.SessionId = P.PersistenceKey
		end
	end
	P.LastDay = context and context.currentDay
		or CCurrentGameState.GetCurrentDate():GetTotalDays()
	P.LastSnapshot = P.BuildSnapshot()
	return P.LastSnapshot
end

function P.OnPublisherAcquired(context)
	GuiPersistence.ResetWriteCache()
	P.PublisherOrdinal = tostring(
		context and context.publisherStateOrdinal or ""
	)
	P.SessionGeneration = 0
	P.SessionId = ""
	P.PersistenceKey = ""
	P.PersistenceRevision = 0
	P.LastPersistedRevision = 0
	P.LastObservedGameDay = nil
	P.PendingPersistenceRevision = nil
	P.PendingProfileToken = nil
	P.PendingProfileTicks = 0
	P.PersistenceDirty = false
	P.RuntimeContext = context
	P.LastDay = nil
	P.LastSnapshot = nil
	return true
end

function P.OnPublisherLost(context)
	P.RuntimeContext = context or P.RuntimeContext
	return true
end

function P.Tick()
	local dispatched = P.PumpActions(P.RuntimeContext, 64)
	local context = {
		currentDay = CCurrentGameState.GetCurrentDate():GetTotalDays()
	}
	if not P.ShouldRefresh(context) then
		return P.LastSnapshot
	end
	P.BuildUpdate(context)
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
