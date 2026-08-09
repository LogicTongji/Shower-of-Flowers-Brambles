local M = {}

M.JapaneseTags = {
	JAP = true,
	MAN = true,
	WJW = true,
	CHB = true,
	MEB = true,
	MEA = true,
	MEC = true,
	CJB = true,
	CJS = true,
	CJD = true
}

M.ChineseTags = {
	CHI = true,
	CHC = true,
	CGX = true,
	CSC = true,
	CYN = true,
	CQM = true,
	CNM = true,
	CSX = true,
	CSD = true,
	CXK = true
}

M.JapaneseUnitTechnologyMultiplier = 1.10

M.ExcludedRegions = {
	xinjiang_region = true,
	utang_region = true,
	taiwan_region = true,
	Mongolia_Regions = true,
	tannuuriankhai_region = true
}

local function isInSet(set, tag)
	return tag and set[tostring(tag)] == true
end

local function getCountry(tagName)
	local success, result = pcall(function()
		local tag = CCountryDataBase.GetTag(tagName)

		if not tag then
			return nil
		end

		return tag:GetCountry()
	end)

	if success then
		return result
	end

	return nil
end

local function isAtWarWithSide(country, opponentTags)
	local success, result = pcall(function()
		if not country:IsAtWar() then
			return false
		end

		for enemyTag in country:GetCurrentAtWarWith() do
			if isInSet(opponentTags, enemyTag) then
				return true
			end
		end

		return false
	end)

	return success and result == true
end

local function readCountryMetrics(country)
	local success, result = pcall(function()
		local units = country:GetUnits()

		return {
			units = tonumber(
				units:GetTotalAmountOfDivisions()
			) or 0,
			manpower = tonumber(
				country:GetManpower():Get()
			) or 0,
			industrialCapacity = tonumber(
				country:GetTotalIC()
			) or 0
		}
	end)

	if success and result then
		return result
	end

	return {
		units = 0,
		manpower = 0,
		industrialCapacity = 0
	}
end

local function collectSideMetrics(
	sideTags,
	opponentTags
)
	local result = {
		units = 0,
		manpower = 0,
		industrialCapacity = 0,
		countries = 0
	}

	for tagName in pairs(sideTags) do
		local country = getCountry(tagName)

		if country
			and isAtWarWithSide(country, opponentTags) then
			local metrics = readCountryMetrics(country)

			result.units = result.units + metrics.units
			result.manpower = result.manpower + metrics.manpower
			result.industrialCapacity =
				result.industrialCapacity
				+ metrics.industrialCapacity
			result.countries = result.countries + 1
		end
	end

	return result
end

local function collectTerritoryMetrics(regions)
	local result = {
		japanese = 0,
		chinese = 0,
		other = 0
	}
	local seenProvinceIds = {}

	for regionName, region in pairs(regions or {}) do
		if not M.ExcludedRegions[regionName] then
			for _, provinceId in ipairs(
				region.provinceids or {}
			) do
				if not seenProvinceIds[provinceId] then
					seenProvinceIds[provinceId] = true

					local success, province = pcall(
						CCurrentGameState.GetProvince,
						provinceId
					)

					if success and province then
						local controller =
							province:GetController()
						local controllerName =
							tostring(controller)

						if M.JapaneseTags[
							controllerName
						] then
							result.japanese =
								result.japanese + 1
						elseif M.ChineseTags[
							controllerName
						] then
							result.chinese =
								result.chinese + 1
						else
							result.other =
								result.other + 1
						end
					end
				end
			end
		end
	end

	return result
end

local function normalizedScore(
	japaneseValue,
	chineseValue,
	japaneseMultiplier
)
	local adjustedJapanese =
		japaneseValue * japaneseMultiplier
	local total = adjustedJapanese + chineseValue

	if total <= 0 then
		return 0.5, 0.5
	end

	local japaneseScore = adjustedJapanese / total
	return japaneseScore, 1.0 - japaneseScore
end

function M.Collect(regions, viewerTag)
	local japan = collectSideMetrics(
		M.JapaneseTags,
		M.ChineseTags
	)
	local china = collectSideMetrics(
		M.ChineseTags,
		M.JapaneseTags
	)
	local territory = collectTerritoryMetrics(regions)

	local japanUnitScore, chinaUnitScore =
		normalizedScore(
			japan.units,
			china.units,
			M.JapaneseUnitTechnologyMultiplier
		)
	local japanManpowerScore, chinaManpowerScore =
		normalizedScore(
			japan.manpower,
			china.manpower,
			1.0
		)
	local japanICScore, chinaICScore =
		normalizedScore(
			japan.industrialCapacity,
			china.industrialCapacity,
			1.0
		)
	local japanTerritoryScore, chinaTerritoryScore =
		normalizedScore(
			territory.japanese,
			territory.chinese,
			1.0
		)

	local japanScore =
		japanUnitScore * 0.25
		+ japanManpowerScore * 0.20
		+ japanICScore * 0.40
		+ japanTerritoryScore * 0.15
	local chinaScore = 1.0 - japanScore

	local ownScore = chinaScore
	local enemyScore = japanScore

	if viewerTag == "JAP" then
		ownScore = japanScore
		enemyScore = chinaScore
	end

	return {
		japan = {
			units = japan.units,
			manpower = japan.manpower,
			industrialCapacity =
				japan.industrialCapacity,
			score = japanScore
		},
		china = {
			units = china.units,
			manpower = china.manpower,
			industrialCapacity =
				china.industrialCapacity,
			score = chinaScore
		},
		territory = territory,
		components = {
			unit = japanUnitScore,
			manpower = japanManpowerScore,
			industry = japanICScore,
			territory = japanTerritoryScore
		},
		own = ownScore,
		enemy = enemyScore
	}
end

return M
