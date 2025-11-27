local P = {}
AI_FRA = P

-- #######################################

-- Tech weights
--   1.0 = 100% the total needs to equal 1.0
function P.TechWeights(voTechnologyData)
	local laTechWeights = {
		0.32, -- landBasedWeight
		0.27, -- landDoctrinesWeight
		0.10, -- airBasedWeight
		0.10, -- airDoctrinesWeight
		0.05, -- navalBasedWeight
		0.04, -- navalDoctrinesWeight
		0.08, -- industrialWeight
		0.0, -- SophisticatedEquipmentTechs
		0.0, -- secretWeaponsWeight
		0.04}; -- otherWeight
	
	return laTechWeights
end

-- Techs that are used in the main file to be ignored
--   techname|level (level must be 1-9 a 0 means ignore all levels
--   use as the first tech name the word "all" and it will cause the AI to ignore all the techs
function P.LandTechs(voTechnologyData)
	local lbArmor = voTechnologyData.TechStatus:IsUnitAvailable(CSubUnitDataBase.GetSubUnit("armor_brigade"))
	local ignoreTech
	
	if lbArmor then
		ignoreTech = {
			{"cavalry_smallarms", 3}, 
			{"cavalry_support", 3},
			{"cavalry_guns", 3}, 
			{"cavalry_at", 3},
			{"militia_smallarms", 0},
			{"militia_support", 0},
			{"militia_guns", 0},
			{"militia_at", 0},
			{"marine_infantry", 0},
			{"jungle_warfare_equipment", 0},
			{"amphibious_warfare_equipment", 0},
			{"heavy_tank_brigade", 0},
			{"heavy_tank_gun", 0},
			{"heavy_tank_engine", 0},
			{"heavy_tank_armour", 0},
			{"heavy_tank_reliability", 0},
			{"super_heavy_tank_brigade", 0},
			{"super_heavy_tank_gun", 0},
			{"super_heavy_tank_engine", 0},
			{"super_heavy_tank_armour", 0},
			{"super_heavy_tank_reliability", 0},
			{"rocket_art", 0},
			{"rocket_art_ammo", 0},
			{"rocket_carriage_sights", 0}};	
	else
		ignoreTech = {
			{"cavalry_smallarms", 3}, 
			{"cavalry_support", 3},
			{"cavalry_guns", 3}, 
			{"cavalry_at", 3},
			{"militia_smallarms", 0},
			{"militia_support", 0},
			{"militia_guns", 0},
			{"militia_at", 0},
			{"marine_infantry", 0},
			{"paratrooper_infantry", 0},
			{"jungle_warfare_equipment", 0},
			{"amphibious_warfare_equipment", 0},
			{"heavy_tank_brigade", 0},
			{"heavy_tank_gun", 0},
			{"heavy_tank_engine", 0},
			{"heavy_tank_armour", 0},
			{"heavy_tank_reliability", 0},
			{"super_heavy_tank_brigade", 0},
			{"super_heavy_tank_gun", 0},
			{"super_heavy_tank_engine", 0},
			{"super_heavy_tank_armour", 0},
			{"super_heavy_tank_reliability", 0},
			{"rocket_art", 0},
			{"rocket_art_ammo", 0},
			{"rocket_carriage_sights", 0},
			{"lighttank_gun", 2},
			{"lighttank_engine", 2},
			{"lighttank_armour", 2},
			{"lighttank_reliability", 2}};
	end
		
	local preferTech = {
		"infantry_activation",
		"smallarms_technology",
		"infantry_support",
		"infantry_guns",
		"infantry_at",
		"lighttank_brigade",
		"lighttank_gun",
		"lighttank_engine",
		"lighttank_armour",
		"lighttank_reliability",
		"tank_brigade",
		"tank_gun",
		"tank_engine",
		"tank_armour",
		"tank_reliability",
		"art_barrell_ammo",
		"art_carriage_sights"};
		
	return ignoreTech, preferTech
end

function P.LandDoctrinesTechs(voTechnologyData)
	local ignoreTech = {
		{"guerilla_warfare", 0},
		{"large_formations", 0}};
		
	local preferTech = {
		"mobile_warfare",
		"elastic_defence",
		"spearhead_doctrine",
		"schwerpunkt",
		"blitzkrieg",
		"operational_level_command_structure",
		"tactical_command_structure",
		"delay_doctrine",
		"integrated_support_doctrine",
		"mechanized_offensive",
		"combined_arms_warfare",
		"infantry_warfare",
		"central_planning",
		"mass_assault",
		"grand_battle_plan",
		"assault_concentration",
		"operational_level_organisation"};
		
	return ignoreTech, preferTech
end

function P.AirTechs(voTechnologyData)
	local ignoreTech = {
		{"basic_strategic_bomber", 0},
		{"large_fueltank", 0},
		{"basic_four_engine_airframe", 0},
		{"four_engine_airframe", 0},
		{"strategic_bomber_armament", 0},
		{"large_bomb", 0},
		{"large_airsearch_radar", 0},
		{"large_navagation_radar", 0}};

	local preferTech = {
		"single_engine_aircraft_design",
		"basic_aeroengine",
		"basic_small_fueltank",
		"basic_single_engine_airframe",
		"basic_aircraft_machinegun",
		"multi_role_fighter_development",
		"twin_engine_aircraft_design",
		"basic_medium_fueltank",
		"basic_twin_engine_airframe",
		"basic_bomb"};
		
	return ignoreTech, preferTech
end

function P.AirDoctrineTechs(voTechnologyData)
	local ignoreTech = {
		{"forward_air_control", 0},
		{"battlefield_interdiction", 0},
		{"bomber_targerting_focus", 0},
		{"fighter_targerting_focus", 0}, 
		{"heavy_bomber_pilot_training", 0},
		{"heavy_bomber_groundcrew_training", 0},
		{"strategic_bombardment_tactics", 0},
		{"strategic_air_command", 0}};

	local preferTech = {
		"fighter_pilot_training",
		"interception_tactics",
		"cas_pilot_training",
		"cas_groundcrew_training",
		"ground_attack_tactics",
		"tac_pilot_training",
		"interdiction_tactics",
		"tactical_air_command"};		
		
	return ignoreTech, preferTech
end
		
function P.NavalTechs(voTechnologyData)
	local ignoreTech = {
		{"lightcruiser_technology", 0},
		{"lightcruiser_maingun", 0},
		{"lightcruiser_secondarygun", 0},
		{"lightcruiser_engine", 0},
		{"lightcruiser_armour", 0},
		{"battlecruiser_technology", 0},
		{"battlecruiser_antiaircraft", 0},
		{"battlecruiser_engine", 0},
		{"battlecruiser_armour", 0},
		{"super_heavy_battleship_technology", 0},
		{"cag_development", 0},
		{"escort_carrier_technology", 0},
		{"carrier_technology", 0},
		{"carrier_antiaircraft", 0},
		{"carrier_engine", 0},
		{"carrier_armour", 0},
		{"carrier_hanger", 0}};

	local preferTech = {
		"destroyer_technology",
		"smallnaval_gun",
		"smallnaval_torpedo",
		"light_boats_antiaircraft",
		"smallnaval_engine",
		"smallnaval_armour",
		"heavycruiser_technology",
		"heavycruiser_armament",
		"heavycruiser_antiaircraft",
		"heavycruiser_engine",
		"heavycruiser_armour"};
		
	return ignoreTech, preferTech
end
		
function P.NavalDoctrineTechs(voTechnologyData)
	local ignoreTech = {
		{"fleet_auxiliary_carrier_doctrine", 0},
		{"light_cruiser_escort_role", 0},
		{"light_cruiser_crew_training", 0},
		{"carrier_group_doctrine", 0},
		{"carrier_crew_training", 0},
		{"carrier_task_force", 0},
		{"naval_underway_repleshment", 0},
		{"radar_training", 0}};

	local preferTech = {
		"sea_lane_defence",
		"destroyer_escort_role",
		"battlefleet_concentration_doctrine",
		"destroyer_crew_training",
		"battleship_crew_training",
		"commerce_defence",
		"fire_control_system_training",
		"commander_decision_making"};		
		
	return ignoreTech, preferTech
end

function P.IndustrialTechs(voTechnologyData)
	local ignoreTech = {
		{"heavy_aa_guns", 0},
		{"steel_production", 0},
		{"raremetal_refinning_techniques", 0},
		{"coal_processing_technologies", 0}};

	local preferTech = {
		"agriculture",
		"industral_production",
		"industral_efficiency",
		"oil_to_coal_conversion",
		"supply_production",
		"oil_refinning",
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
	local ignoreTech = {
		{"naval_engineering_research", 0},
		{"submarine_engineering_research", 0},
		{"aeronautic_engineering_research", 0},
		{"rocket_science_research", 0},
		{"chemical_engineering_research", 0},
		{"nuclear_physics_research", 0},
		{"jetengine_research", 0},
		{"mechanicalengineering_research", 0},
		{"automotive_research", 0},
		{"electornicegineering_research", 0},
		{"artillery_research", 0},
		{"mobile_research", 0},
		{"militia_research", 0},
		{"infantry_research", 0}};

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
	-- More than 150 brigades so build stuff that does not use manpower
	if (voProductionData.ManpowerTotal < 100 and voProductionData.LandCountTotal > 150)
	or voProductionData.ManpowerTotal < 50 then
		laArray = {
			0.0, -- Land
			0.55, -- Air
			0.35, -- Sea
			0.10}; -- Other	
			
	-- We no longer control Paris (so we lost)
	elseif voProductionData.IsExile then
		laArray = {
			0.30, -- Land
			0.20, -- Air
			0.30, -- Sea
			0.20}; -- Other	(Help build Undergrounds)
	elseif voProductionData.Year >= 1940 then
		laArray = {
			0.65, -- Land
			0.20, -- Air
			0.10, -- Sea
			0.05}; -- Other
	else
		laArray = {
			0.80, -- Land
			0.18, -- Air
			0.00, -- Sea
			0.02}; -- Other
	end
	
	return laArray
end

-- Land ratio distribution
function P.LandRatio(voProductionData)
	local laArray
	
	-- Early years focus on Infantry
	if not(voProductionData.IsExile) then
		laArray = {
			garrison_brigade = 2,
			infantry_brigade = 20,
			motorized_brigade = 2,
			mechanized_brigade = 1,
			armor_brigade = 1};
			
	-- France has learned it's lesson now focus on a more modern setup
	else
		laArray = {
			infantry_brigade = 20,
			motorized_brigade = 3,
			mechanized_brigade = 1,
			armor_brigade = 1}		
	end
	
	return laArray
end

-- Special Forces ratio distribution
function P.SpecialForcesRatio(voProductionData)
	local laRatio = {
		40, -- Land
		1}; -- Special Force Unit

	local laUnits = {bergsjaeger_brigade = 3};
	
	return laRatio, laUnits	
end

-- Elite Units
function P.EliteUnits(voProductionData)
	local laUnits = {"alpins_brigade"};
	
	return laUnits	
end

-- Which units should get 1 more Support unit with Superior Firepower tech
function P.FirePower(voProductionData)
	local laArray = {
		"alpins_brigade",
		"motorized_brigade",
		"mechanized_brigade",
		"armor_brigade"};
		
	return laArray
end

-- Air ratio distribution
function P.AirRatio(voProductionData)
	local laArray = {
		interceptor = 6,
		multi_role = 2,
		cas = 2,
		tactical_bomber = 5,
		naval_bomber = 1};
	
	return laArray
end

-- Naval ratio distribution
function P.NavalRatio(voProductionData)
	local laArray = {
		destroyer = 6,
		submarine = 2,
		heavy_cruiser = 2,
		battleship = 1};
	
	return laArray
end

-- Transport to Land unit distribution
function P.TransportLandRatio(voProductionData)
	local laArray = {
		25, -- Land
		1,  -- transport
		1}  -- invasion craft
  
	return laArray
end

-- Convoy Ratio control
--- NOTE: If goverment is in Exile these parms are ignored
function P.ConvoyRatio(voProductionData)
	local laArray
	
	if voProductionData.Year < 1940 and not(voProductionData.IsExile) then
		laArray = {
			5, -- Percentage extra (adds to 100 percent so if you put 10 it will make it 110% of needed amount)
			5, -- If Percentage extra is less than this it will force it up to the amount entered
			10, -- If Percentage extra is greater than this it will force it down to this
			10} -- Escort to Convoy Ratio (Number indicates how many convoys needed to build 1 escort)
	else
		laArray = {
			5, -- Percentage extra (adds to 100 percent so if you put 10 it will make it 110% of needed amount)
			5, -- If Percentage extra is less than this it will force it up to the amount entered
			10, -- If Percentage extra is greater than this it will force it down to this
			5} -- Escort to Convoy Ratio (Number indicates how many convoys needed to build 1 escort)
	end
  
	return laArray
end

-- Create Custom infantry for France
function P.Build_infantry_brigade(vIC, viManpowerTotal, voType, voProductionData, viUnitQuantity)
	-- Build old style formations till 1940
	if voProductionData.Year < 1940 and not(voProductionData.IsAtWar) and not(voProductionData.IsExile) then
		local laSupportUnit = {
			"artillery_brigade",
			"anti_air_brigade",
			"anti_tank_brigade"}

		voType.Size = 3
		voType.Support = 1
	end

	return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
end

-- Create Custom Motorized for France
function P.Build_motorized_brigade(vIC, viManpowerTotal, voType, voProductionData, viUnitQuantity)
	-- Build old style formations till 1940
	if voProductionData.Year < 1940 and not(voProductionData.IsAtWar) and not(voProductionData.IsExile) then
		voType.Size = 3
		voType.Support = 0
	end

	return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
end

-- Do not build coastal forts
function P.Build_CoastalFort(vIC, voProductionData)
	return vIC, false
end

function P.Build_RocketTest(vIC, voProductionData)
	if voProductionData.Year <= 1942 then
		return vIC, false
	else
		if voProductionData.IsExile then
			return vIC, false
		elseif (voProductionData.icTotal < 50 and voProductionData.Year <= 1945) then
			return vIC, false
		end
	end
	
	return vIC, true	
end

function P.Build_AntiAir(vIC, voProductionData)
	if voProductionData.Year < 1942 then
		return vIC, false
	elseif voProductionData.IsExile then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_Infrastructure(vIC, voProductionData)
	if voProductionData.Year < 1942 then
		return vIC, false
	elseif voProductionData.IsExile then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_Fort(vIC, voProductionData)
	if voProductionData.Year < 1942 then
		return vIC, false
	elseif voProductionData.IsExile then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_NavalBase(vIC, voProductionData)
	if voProductionData.IsExile then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_Radar(vIC, voProductionData)
	if voProductionData.IsExile then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_Underground(vIC, voProductionData)
	if voProductionData.IsExile then
		return vIC, true
	end
	
	return vIC, false
end

-- END OF PRODUTION OVERIDES
-- #######################################

function P.DiploScore_OfferTrade(voDiploScoreObj)
	local laTrade = {
		ENG = {Score = 20},
		AST = {Score = 20},
		SAF = {Score = 20},
		NZL = {Score = 20},
		USA = {Score = 20},
		JAP = {Score = 50},
		GER = {Score = -20},
		ITA = {Score = -20},
		CAN = {Score = 20}}
	
	if laTrade[voDiploScoreObj.TagName] then
		return voDiploScoreObj.Score + laTrade[voDiploScoreObj.TagName].Score
	end
	
	return voDiploScoreObj.Score
end

function P.DiploScore_Debt(voDiploScoreObj)
	local lsToTag = tostring(voDiploScoreObj.ToTag)
	
	if lsToTag == "CHI" then
		local loAllyFaction = CCurrentGameState.GetFaction("allies")
		
		-- We must be in the allies before we do this
		if voDiploScoreObj.FromCountry:GetFaction() == loAllyFaction then
			local japTag = CCountryDataBase.GetTag("JAP")
			
			-- If China and Japan are at war then let China be allowed debt even if not in the Allies
			if voDiploScoreObj.ToCountry:GetRelation(japTag):HasWar() then
				voDiploScoreObj.Score = 100
			end
		end
	end
	
	return voDiploScoreObj.Score
end

function P.DiploScore_InfluenceNation(voDiploScoreObj)
	-- Only do this if we are in the allies
	if voDiploScoreObj.FactionName == "allies" then
		local loInfluences = {
			AST = {Score = 70},
			CAN = {Score = 70},
			SAF = {Score = 70},
			NZL = {Score = 70},
			USA = {Score = 70},
			BRA = {Score = 40},
			YUG = {Score = 20},
			GRE = {Score = 20}}	
	
		-- Are they on our list
		if loInfluences[voDiploScoreObj.TargetName] then
			return (voDiploScoreObj.Score + loInfluences[voDiploScoreObj.TargetName].Score)
		end
	end

	return voDiploScoreObj.Score	
end

function P.HandleMobilization(minister)
	local ai = minister:GetOwnerAI()
	
	local ministerTag = minister:GetCountryTag()
	local gerTag = CCountryDataBase.GetTag("GER")

	-- If Germany Controls Czechoslovakia then
	if CCurrentGameState.GetProvince(2562):GetController() == gerTag then -- Praha check
		ai:Post(CToggleMobilizationCommand( ministerTag, true ))					
	else
		-- Check if a neighbor is starting to look threatening
		-- This code should be idential to the one in ai_politics_minsiter.lua
		local ministerCountry = ministerTag:GetCountry()
		local liTotalIC = ministerCountry:GetTotalIC()
		local liNeutrality = ministerCountry:GetNeutrality():Get() * 0.9
		
		for loCountryTag in ministerCountry:GetNeighbours() do
			local liThreat = ministerCountry:GetRelation(loCountryTag):GetThreat():Get()
			
			if (liNeutrality - liThreat) < 10 then
				local loCountry = loCountryTag:GetCountry()
				
				liThreat = liThreat * CalculateAlignmentFactor(ai, ministerCountry, loCountry)
				
				if liTotalIC > 50 and loCountry:GetTotalIC() < liTotalIC then
					liThreat = liThreat / 2 -- we can handle them if they descide to attack anyway
				end
				
				if liThreat > 30 then
					if CalculateWarDesirability(ai, loCountry, ministerTag) > 70 then
						ai:Post(CToggleMobilizationCommand( ministerTag, true ))
					end
				end
			end
		end
	end
end

--##########################
-- Foreign Minister Hooks
function P.ForeignMinister_Influence(voForeignMinisterData)
	local laIgnoreWatch -- Ignore this country but monitor them if they are about to join someone else
	local laWatch -- Monitor them and also fi their score is high enough they can be influenced normally
	local laIgnore -- Ignore them completely

	if voForeignMinisterData.FactionName == "allies" then
		laWatch = {
			"BEL", -- Belgium
			"HOL", -- Holland
			"SWE", -- Sweden
			"CHI", -- China
			"NOR"} -- Norway
			
		laIgnoreWatch = {
			"TUR", -- Turkey
			"SPA", -- Spain
			"SPR", -- Republic Spain
			"POR", -- Portugal
			"AFG", -- Afghanistan
			"PER", -- Persia
			"SAU", -- Saudi Arabia
			"ARG", -- South America
			"BOL", 
			"BRA",
			"CHL",
			"COL",
			"ECU",
			"GUY",
			"PAR",
			"PRU",
			"URU",
			"VEN",
			"CUB", -- Central America
			"COS",
			"DOM",
			"GUA",
			"HAI",
			"HON",
			"MEX",
			"NIC",
			"PAN",
			"SAL"}
			
		laIgnore = {
			"HUN", -- Hungary
			"ROM", -- Romania
			"BUL", -- Bulgaria
			"FIN", -- Finland
			"CYN", -- Yunnan
			"SIK", -- Sikiang
			"CGX", -- Guangxi Clique
			"CSX", -- Shanxi
			"TIB", -- Tibet
			"CHC", -- Communist China
			"LAT", -- Lativia
			"LIT", -- Lithuania
			"EST", -- Estonia
			"LUX", -- Luxemburg
			"VIC", -- Vichy
			"DEN", -- Denmark
			"ETH", -- Ethiopia
			"AUS", -- Austria
			"CZE", -- Czechoslovakia
			"SCH", -- Switzerland
			"VIC", -- Vichy
			"JAP", -- Japan
			"ITA"} -- Italy
	end
	
	return laWatch, laIgnoreWatch, laIgnore
end


return AI_FRA

