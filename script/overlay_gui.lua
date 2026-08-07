local P = {}
China_aginst_japan_map = P

P.VisibleTags = {
	JAP = true,
	CHI = true,
	CHC = true
}
P.regionfilepath = "map\\region.txt"
P.DisplayRegionNames = {
	"guangdong_region",
	"shanxi_region",
	"yunnan_region",
	"guangxi_region",
	"xikang_region",
	"ningxia_region",
	"gansu_region",
	"qinghai_region",
	"chahar_region",
	"suiyuan_region",
	"sichuan_region",
	"guizhou_region",
	"shaanxi_region",
	"hebei_region",
	"east_hebei_region",
	"shandong_region",
	"fujian_region",
	"hunan_region",
	"jiangsu_region",
	"jiangxi_region",
	"chekiang_region",
	"anhui_region",
	"henan_region",
	"hubei_region",
	"xingan_region",
	"rehe_region",
	"fengtian_region",
	"liaonning_region",
	"andong_region",
	"nenjiang_region",
	"heihe_region",
	"heilongjiang_region",
	"songjiang_region",
	"jiandao_region",
	"jilin_region",
	"xinjiang_region",
	"utang_region",
	"taiwan_region",
	"Mongolia_Regions",
	"tannuuriankhai_region"
}
P.Regions = {}
P.State = nil
P.Initialized = false
P.RegionPopulation = {
    ["guangdong_region"] = 32453000 ,
    ["shanxi_region"] = 11601000,
    ["yunnan_region"] = 12042000,
    ["guangxi_region"] = 13385000,
    ["xikang_region"] = 1651100,
    ["ningxia_region"] = 978000,
    ["gansu_region"] = 6716000,
    ["qinghai_region"] = 1196000,
    ["chahar_region"] = 2036000,
    ["suiyuan_region"] = 2084000,
    ["sichuan_region"] = 53674000,
    ["guizhou_region"] = 9919000,
    ["shaanxi_region"] = 9986000,
    ["hebei_region"] = 31410000,
    ["east_hebei_region"] = 3000000,
    ["shandong_region"] = 38837000,
    ["fujian_region"] = 11756000,
    ["hunan_region"] = 28294000,
    ["jiangsu_region"] = 41215000,
    ["jiangxi_region"] = 15805000,
    ["chekiang_region"] = 21240000,
    ["anhui_region"] = 23354000,
    ["henan_region"] = 34290000,
    ["hubei_region"] = 25516000,
    ["xingan_region"] = 2122000,
    ["rehe_region"] = 2185000,
    ["fengtian_region"] = 7565000,
    ["liaonning_region"] = 3005000,
    ["andong_region"] = 2231000,
    ["nenjiang_region"] = 680000,
    ["heihe_region"] = 149000,
    ["heilongjiang_region"] = 2093000,
    ["songjiang_region"] = 688000,
    ["jiandao_region"] = 848000,
    ["jilin_region"] = 5608000,
    ["xinjiang_region"] = 4360000,
	["utang_region"] = 1100000,
	["taiwan_region"] = 5291000,
	["Mongolia_Regions"] = 616000,
	["tannuuriankhai_region"] = 800000 
}
P.PopulationLossExponent = 1.2
P.JapaneseControlTags = {
	JAP = true,
	MAN = true,
	WJW = true,
	CHB = true,
	MEB = true,
	MEA = true,
	MEC = true
}

local function China_war_with_japan()
    local japTag = CCountryDataBase.GetTag('JAP'):GetCountry()
    return japTag:GetRelation(CCountryDataBase.GetTag('CHI')):HasWar()
    or
    japTag:GetRelation(CCountryDataBase.GetTag('CHC')):HasWar()
end

local function parseRegionFile(path)
	local file = io.open(path, "r")
    if not file then
        return nil,"region_file_not_found"
    end
    local region = {}
    local current_analysis_region = nil
    for rawLine in file:lines() do
        local line = rawLine:gsub("#.*", "")
		local regionName = line:match(
			"^%s*([%w_]+)%s*=%s*{%s*$"
		)
    if regionName then
        current_analysis_region ={
            id = regionName,
            provinceids = {},
            provinceSet = {}
        }
        region[regionName] = current_analysis_region
    elseif current_analysis_region then
        if line:match("^%s*}%s*$") then
            current_analysis_region = nil
        else
            for token in line:gmatch("%d+") do
                local provinceids = tonumber(token)
                if not current_analysis_region.provinceSet[provinceids] then
                current_analysis_region.provinceSet[provinceids] = true
                table.insert(current_analysis_region.provinceids, provinceids)
                end
            end
        end
    end
end
    file:close()
    return region
end

local function scanprovince(provinceid)
	local province =
		CCurrentGameState.GetProvince(provinceid)

	if not province then
		return nil
	end

	local ownerTag =
		province:GetOwner()

	local controllerTag =
		province:GetController()

    local controllerName =
		tostring(controllerTag)

	local lbJAPcontrol =
		P.JapaneseControlTags[controllerName] == true

	return {
		id = provinceid,
		ownerTag = tostring(ownerTag),
		controllerTag = controllerName,
		lbJAPcontrol = lbJAPcontrol
	}
end

local function getRegionColor(percentage)
	if percentage >= 90 then
		return {
			id = "dark_red",
			r = 80,
			g = 0,
			b = 0,
			a = 220
		}
	elseif percentage >= 80 then
		return {
			id = "deep_red",
			r = 150,
			g = 20,
			b = 20,
			a = 210
		}
	elseif percentage >= 60 then
		return {
			id = "light_red",
			r = 230,
			g = 100,
			b = 100,
			a = 200
		}
	elseif percentage >= 20 then
		return {
			id = "yellow",
			r = 240,
			g = 210,
			b = 60,
			a = 190
		}
	end

	return {
		id = "normal",
		r = 80,
		g = 130,
		b = 80,
		a = 160
	}
end

local function clamp(value, minimum, maximum)
	if value < minimum then
		return minimum
	elseif value > maximum then
		return maximum
	end

	return value
end

local function calculatePopulationData(
	regionName,
	controlledPercentage
)
	local totalPopulation =
		tonumber(P.RegionPopulation[regionName]) or 0

	local territoryRatio = clamp(
		controlledPercentage / 100,
		0,
		1
	)

	local affectedRatio =
		territoryRatio ^ P.PopulationLossExponent

	local affectedPopulation = math.floor(
		totalPopulation * affectedRatio + 0.5
	)

	return {
		known = totalPopulation > 0,
		total = totalPopulation,
		affected = affectedPopulation,
		remaining = totalPopulation - affectedPopulation,
		affectedRatio = affectedRatio
	}
end

local function scanregion(region)
    local regionData = {}
    local totalProvinces = 0
    local japaneseControlledProvinces = 0
    for _, provinceid in ipairs(region.provinceids) do
        local provinceData = scanprovince(provinceid)
        if provinceData then
            totalProvinces = totalProvinces + 1
            regionData[provinceid] = provinceData
            if provinceData.lbJAPcontrol then
                japaneseControlledProvinces = japaneseControlledProvinces +1
            end
        end
    end
    local percentage = 0
    if totalProvinces > 0 then
        percentage = (japaneseControlledProvinces / totalProvinces) * 100
    end
	local population = calculatePopulationData(
		region.id,
		percentage
	)

    return {
        provinces = regionData,
		totalProvinces = totalProvinces,
		japaneseControlledProvinces = japaneseControlledProvinces,
		japaneseControlledPercentage = percentage,
		population = population,
        color = getRegionColor(percentage)
	}
end

function P.Initialize()
	local parsedRegions, errorCode =
		parseRegionFile(P.regionfilepath)

	if not parsedRegions then
		return false, errorCode
	end

	P.Regions = {}

	for _, regionName in ipairs(P.DisplayRegionNames) do
		if parsedRegions[regionName] then
			P.Regions[regionName] =
				parsedRegions[regionName]
		end
	end

	P.Initialized = true

	return true
end

function P.SetRegionFilePath(path)
	if path and path ~= "" then
		P.regionfilepath = path
	end
end

function P.SetDisplayRegions(regionNames)
	if type(regionNames) == "table" then
		P.DisplayRegionNames = regionNames
		P.Initialized = false
	end
end

function P.IsVisible()
	local playerTag =
		CCurrentGameState.GetPlayer()

	if not playerTag then
		return false
	end

	local playerName =
		tostring(playerTag)

	return P.VisibleTags[playerName] == true
		and China_war_with_japan()
end

function P.Update()
	if not P.Initialized then
		local initialized, errorCode =
			P.Initialize()

		if not initialized then
			P.State = {
				visible = false,
				active = false,
				error = errorCode,
				regions = {}
			}

			return P.State
		end
	end

	local playerTag = CCurrentGameState.GetPlayer()

	local playerName = nil

	if playerTag then
		playerName = tostring(playerTag)
	end

	local warActive = China_war_with_japan()

	local visible =
		P.VisibleTags[playerName] == true
		and warActive

	P.State = {
		visible = visible,
		active = warActive,
		playerTag = playerName,
		date =
			CCurrentGameState.GetCurrentDate():GetTotalDays(),
		regions = {}
	}

	if not visible then
		return P.State
	end

	for regionName, region in pairs(P.Regions) do
		P.State.regions[regionName] =
			scanregion(region)
	end

	return P.State
end

function P.Tick()
	local currentDay = CCurrentGameState.GetCurrentDate():GetTotalDays()
    if P.Initialized and P.State and P.State.date == currentDay and P.State.active then
		return P.State
	end
    return P.Update()
end

function P.GetState()
	return P.State
end

function P.GetRegionState(regionName)
	if not P.State then
		return nil
	end

	return P.State.regions[regionName]
end

function P.GetProvinceState(regionName, provinceid)
	local regionState =
		P.GetRegionState(regionName)

	if not regionState then
		return nil
	end

	return regionState.provinces[provinceid]
end

return P
