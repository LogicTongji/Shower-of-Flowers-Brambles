local P = {}
AI_CXB = P

-- Tech weights
--   1.0 = 100% the total needs to equal 1.0
function P.TechWeights(voTechnologyData)
	local laTechWeights = {
		0.60, -- landBasedWeight
		0.25, -- landDoctrinesWeight
		0.0, -- airBasedWeight
		0.0, -- airDoctrinesWeight
		0.0, -- navalBasedWeight
		0.0, -- navalDoctrinesWeight
		0.15, -- industrialWeight
		0.0, -- SophisticatedEquipmentTechs
		0.0, -- secretWeaponsWeight
		0.0}; -- otherWeight
	
	return laTechWeights
end

-- Techs that are used in the main file to be ignored
--   techname|level (level must be 1-9 a 0 means ignore all levels
--   use as the first tech name the word "all" and it will cause the AI to ignore all the techs
function P.LandTechs(voTechnologyData)
	local ignoreTech = {
		{"cavalry_smallarms", 0}, 
		{"cavalry_support", 0},
		{"cavalry_guns", 0}, 
		{"cavalry_at", 0},
		{"armored_car_armour", 0},
		{"armored_car_gun", 0},
		{"paratrooper_infantry", 0},
		{"marine_infantry", 0},
		{"imporved_police_brigade", 0},
		{"desert_warfare_equipment", 0},
		{"jungle_warfare_equipment", 0},
		{"artic_warfare_equipment", 0},
		{"amphibious_warfare_equipment", 0},
		{"airborne_warfare_equipment", 0},
		{"lighttank_brigade", 0},
		{"lighttank_gun", 0},
		{"lighttank_engine", 0},
		{"lighttank_armour", 0},
		{"lighttank_reliability", 0},
		{"tank_brigade", 0},
		{"tank_gun", 0},
		{"tank_engine", 0},
		{"tank_armour", 0},
		{"tank_reliability", 0},
		{"heavy_tank_brigade", 0},
		{"heavy_tank_gun", 0},
		{"heavy_tank_engine", 0},
		{"heavy_tank_armour", 0},
		{"heavy_tank_reliability", 0},
		{"SP_brigade", 0},
		{"mechanised_infantry", 0},
		{"super_heavy_tank_brigade", 0},
		{"super_heavy_tank_gun", 0},
		{"super_heavy_tank_engine", 0},
		{"super_heavy_tank_armour", 0},
		{"super_heavy_tank_reliability", 0},
		{"at_barrell_sights", 0},
		{"at_ammo_muzzel", 0},
		{"aa_barrell_ammo", 0},
		{"aa_carriage_sights", 0},
		{"rocket_art", 0},
		{"rocket_art_ammo", 0},
		{"rocket_carriage_sights", 0}};
		
	local preferTech = {
		"infantry_activation",
		"smallarms_technology",
		"infantry_support",
		"infantry_guns",
		"infantry_at",
		"art_barrell_ammo",
		"art_carriage_sights"};
		
	return ignoreTech, preferTech
end

function P.LandDoctrinesTechs(voTechnologyData)
	local ignoreTech = {
		{"mobile_warfare", 0},
		{"elastic_defence", 0},
		{"spearhead_doctrine", 0},
		{"schwerpunkt", 0},
		{"blitzkrieg", 0},
		{"operational_level_command_structure", 0},
		{"tactical_command_structure", 0},
		{"integrated_support_doctrine", 0},
		{"mechanized_offensive", 0},
		{"combined_arms_warfare", 0},
		{"special_forces", 0}};
		
	local preferTech = {
		"infantry_warfare",
		"mass_assault",
		"peoples_army"};	
		
	return ignoreTech, preferTech
end

function P.AirTechs(voTechnologyData)
	local ignoreTech = {"all"};
	
	return ignoreTech, nil
end

function P.AirDoctrineTechs(voTechnologyData)
	local ignoreTech = {"all"};

	return ignoreTech, nil
end
		
function P.NavalTechs(voTechnologyData)
	local ignoreTech = {"all"}

	return ignoreTech, nil
end
		
function P.NavalDoctrineTechs(voTechnologyData)
	local ignoreTech = {"all"};

	return ignoreTech, nil
end

function P.IndustrialTechs(voTechnologyData)
	local ignoreTech = {
		{"oil_to_coal_conversion", 0},
		{"heavy_aa_guns", 0},
		{"radio_detection_equipment", 0},
		{"radar", 0},
		{"decryption_machine", 0},
		{"encryption_machine", 0},
		{"rocket_tests", 0},
		{"rocket_engine", 0},
		{"theorical_jet_engine", 0},
		{"atomic_research", 0},
		{"nuclear_research", 0},
		{"isotope_seperation", 0},
		{"civil_nuclear_research", 0},
		{"oil_refinning", 0},
		{"steel_production", 0},
		{"raremetal_refinning_techniques", 0},
		{"coal_processing_technologies", 0}};

	local preferTech = {
		"agriculture",
		"industral_production",
		"industral_efficiency",
		"supply_production",
		"education"};
		
	return ignoreTech, preferTech
end
		
function P.SophisticatedEquipmentTechs(voTechnologyData)
	local ignoreTech = {"all"}
	
	return ignoreTech, nil
end
		
function P.SecretWeaponTechs(voTechnologyData)
	local ignoreTech = {"all"}
	
	return ignoreTech, nil
end

function P.OtherTechs(voTechnologyData)
	local ignoreTech = {"all"}

	return ignoreTech, nil
end

-- END OF TECH RESEARCH OVERIDES
-- #######################################


-- #######################################
-- Production Overides the main LUA with country specific ones

-- Production Weights
--   1.0 = 100% the total needs to equal 1.0
function P.ProductionWeights(voProductionData)
	local laArray

	-- Check to see if manpower is to low
	-- More than 30 brigades so build stuff that does not use manpower
	if (voProductionData.ManpowerTotal < 30 and voProductionData.LandCountTotal > 30)
	or voProductionData.ManpowerTotal < 10 then
		laArray = {
			0.0, -- Land
			0.50, -- Air
			0.0, -- Sea
			0.50}; -- Other	
	else
		laArray = {
			0.90, -- Land
			0.0, -- Air
			0.0, -- Sea
			0.10}; -- Other
	end
	
	return laArray
end
-- Land ratio distribution
function P.LandRatio(voProductionData)
	local laArray = {
		infantry_brigade = 4,
		militia_brigade = 2};
	
	return laArray
end
-- Special Forces ratio distribution
function P.SpecialForcesRatio(voProductionData)
	local laArray = {
		0, -- Land
		0}; -- Special Forces Unit
	
	return laArray, nil
end
-- Air ratio distribution
function P.AirRatio(voProductionData)
	local laArray = {
		interceptor = 3, 
		cas = 1, 
		tactical_bomber = 2};
	
	return laArray
end

-- Transport to Land unit distribution
function P.TransportLandRatio(voProductionData)
             local laArray = {
			0, -- Land
			0,  -- transport
                        0,  -- invasion transport ship
                        0}; -- assault ships
	return laArray
end

-- Do not build coastal forts
function P.Build_CoastalFort(ic, voProductionData)
	return ic, false
end

-- Have Com. China Fortify their country
function P.Build_Fort(ic, voProductionData)
	-- Only build the forts if its 1938 or less
	if voProductionData.Year <= 1938 then
		ic = Support.Build_Fort(ic, voProductionData, 7390, 2) -- Shizuishan
		ic = Support.Build_Fort(ic, voProductionData, 7404, 2) -- Liangzhen
	end
	
	return ic, false
end

-- END OF PRODUTION OVERIDES
-- #######################################

-- Want more troops, let them learn on the battlefield.
--   helps them produce troops faster
function P.CallLaw_training_laws(minister, voCurrentLaw)
	local _MINIMAL_TRAINING_ = 27
	return CLawDataBase.GetLaw(_MINIMAL_TRAINING_)
end

function P.DiploScore_Alliance(voDiploScoreObj)
	-- Make sure we are not in a faction already and China is the one asking
	if not(voDiploScoreObj.TargetHasFaction) and tostring(voDiploScoreObj.ministerTag) == "CHI" then
		-- If China and Japan are atwar then come to their help
		if voDiploScoreObj.ministerCountry:GetRelation(CCountryDataBase.GetTag("JAP")):HasWar() then
			voDiploScoreObj.Score = voDiploScoreObj.Score + 50
		end
	end
	
	return voDiploScoreObj.Score
end

function P.DiploScore_GiveMilitaryAccess(viScore, voAI, voCountry)
	local lsCountry = tostring(voCountry)

	-- Do not let Japan in our territory
	if lsCountry == "JAP" then
		viScore = 0
	end
	
	return viScore
end

function P.Build_infantry_brigade(vIC, viManpowerTotal, voType, voProductionData, viUnitQuantity)
	local lbJAPWar = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag('JAP')):HasWar()
	local lbCHIWar = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag('CHI')):HasWar()
	local lbartillery = voProductionData.TechStatus:IsUnitAvailable(CSubUnitDataBase.GetSubUnit("artillery_brigade"))
	local lbalpineArtillery = voProductionData.TechStatus:IsUnitAvailable(CSubUnitDataBase.GetSubUnit("alpine_artillery_brigade"))
	if (not (lbJAPWar) and not (lbCHIWar)) and (voProductionData.LandCountTotal < 400) then	
		-- If China still controls WuHan keep hitting them
	         local laSupportUnit = nil
	         voType.Size = 3
	         voType.Support = 0
			 return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
			elseif lbartillery then	
				local laSupportUnit = {
					"artillery_brigade"}
					voType.Size = 3
					voType.Support = 1
				return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
			elseif lbalpineArtillery then	
				local laSupportUnit = {
					"alpine_artillery_brigade",
				    "artillery_brigade"}
					voType.Size = 3
					voType.Support = 1
				return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
	end
	if(lbCHIWar and lbJAPWar) then
		local laSupportUnit = {
			"alpine_artillery_brigade",
			"field_battalion"}
			voType.Size = 3
			voType.Support = 1
			return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
        elseif lbCHIWar then
			local laSupportUnit = nil
			voType.Size = 3
			voType.Support = 0
			return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
	end
	local laSupportUnit = nil
			voType.Size = 3
			voType.Support = 0
			return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
end

return AI_CXB 

