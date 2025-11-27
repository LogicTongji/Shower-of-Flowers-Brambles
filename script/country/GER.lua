local P = {}
AI_GER = P

-- #######################################
-- Start of Trade Weights
function P.TradeWeights(voResourceData)
	local laResouces = {
		RARE_MATERIALS = {
			Buffer = 5,
			BufferSaleCap = 10000},
		CRUDE_OIL = {
			Buffer = 2,
			BufferSaleCap = 10000},
		FUEL = {
			Buffer = 2,
			BufferSaleCap = 10000}}
	
	return laResouces
end

-- #######################################
-- Start of Tech Research

-- Tech weights
--   1.0 = 100% the total needs to equal 1.0
function P.TechWeights(voTechnologyData)
	local laTechWeights = {
		0.24, -- landBasedWeight
		0.18, -- landDoctrinesWeight
		0.11, -- airBasedWeight
		0.18, -- airDoctrinesWeight
		0.06, -- navalBasedWeight
		0.05, -- navalDoctrinesWeight
		0.10, -- industrialWeight
		0.04, -- SophisticatedEquipmentTechs
		0.00, -- secretWeaponsWeight
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
			{"marine_infantry", 0},
			{"jungle_warfare_equipment", 0},
			{"amphibious_warfare_equipment", 0},
			{"super_heavy_tank_brigade", 0},
			{"super_heavy_tank_gun", 0},
			{"super_heavy_tank_engine", 0},
			{"super_heavy_tank_armour", 0},
			{"super_heavy_tank_reliability", 0}};	
	else
		ignoreTech = {
			{"cavalry_smallarms", 3}, 
			{"cavalry_support", 3},
			{"cavalry_guns", 3}, 
			{"cavalry_at", 3},
			{"marine_infantry", 0},
			{"jungle_warfare_equipment", 0},
			{"amphibious_warfare_equipment", 0},
			{"lighttank_gun", 2},
			{"lighttank_engine", 2},
			{"lighttank_armour", 2},
			{"lighttank_reliability", 2},
			{"super_heavy_tank_brigade", 0},
			{"super_heavy_tank_gun", 0},
			{"super_heavy_tank_engine", 0},
			{"super_heavy_tank_armour", 0},
			{"super_heavy_tank_reliability", 0}};
	end

	local preferTech = {
		"mechanised_infantry",
		"infantry_activation",
		"smallarms_technology",
		"infantry_support",
		"infantry_guns",
		"infantry_at",
		"paratrooper_brigade",
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
		"heavy_tank_brigade",
		"heavy_tank_gun",
		"heavy_tank_engine",
		"heavy_tank_armour",
		"heavy_tank_reliability",
		"art_barrell_ammo",
		"art_carriage_sights",
		"at_ammo_muzzel",
		"at_barrell_sights"};
		
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
		"heavycruiser_armour",
		"submarine_technology",
		"submarine_antiaircraft",
		"submarine_engine",
		"submarine_hull",
		"submarine_torpedoes",
		"submarine_modern_modification",
		"submarine_sonar",
		"submarine_airwarningequipment"};	
		
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
		"fleet_auxiliary_submarine_doctrine",
		"trade_interdiction_submarine_doctrine",
		"cruiser_warfare",
		"submarine_crew_training",
		"cruiser_crew_training",
		"unrestricted_submarine_warfare_doctrine",
		"spotting",
		"fire_control_system_training",
		"commander_decision_making"};		
		
	return ignoreTech, preferTech
end

function P.IndustrialTechs(voTechnologyData)
	local ignoreTech = {
		{"steel_production", 0},
		{"coal_processing_technologies", 0}};

	local preferTech = {
		"agriculture",
		"industral_production",
		"industral_efficiency",
		"oil_to_coal_conversion",
		"supply_production",
		"oil_refinning",
		"education",
		"mechnical_computing_machine"};
		
	return ignoreTech, preferTech
end
		
function P.SophisticatedEquipmentTechs(voTechnologyData)
	local ignoreTech = {}
	
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

	local preferTech = {
		"supply_transportation",
		"supply_organisation",
		"civil_defence"};		

	return ignoreTech, preferTech
end

-- END OF TECH RESEARCH OVERIDES
-- #######################################

-- #######################################
-- Production Weights
--   1.0 = 100% the total needs to equal 1.0
function P.ProductionWeights(voProductionData)
	local laArray = {
		0.55, -- Land
		0.25, -- Air
		0.15, -- Sea
		0.05}; -- Other
	
	-- Check to see if manpower is to low
	if voProductionData.ManpowerTotal < 50 then
		laArray = {
			0.0, -- Land
			0.50, -- Air
			0.20, -- Sea
			0.30}; -- Other
	elseif voProductionData.Year <= 1939 and not(voProductionData.IsAtWar) then
		laArray = {
			0.50, -- Land
			0.30, -- Air
			0.15, -- Sea
			0.05}; -- Other
	
	-- More than 400 brigades so build stuff that does not use manpower
	elseif (voProductionData.ManpowerTotal < 300 and voProductionData.LandCountTotal > 400) then
		laArray = {
			0.15, -- Land
			0.40, -- Air
			0.20, -- Sea
			0.25}; -- Other
			
	elseif voProductionData.IsAtWar then
		local sovTag = CCountryDataBase.GetTag('SOV')
		local lbControlMoscow = (CCurrentGameState.GetProvince(1409):GetController() == sovTag)
		local lbSovGerWar = voProductionData.ministerCountry:GetRelation(sovTag):HasWar()
		
		-- Desperation check and if so heavy production of land forces
		if voProductionData.ministerCountry:CalcDesperation():Get() > 0.4 then
			laArray = {
				0.70, -- Land
				0.20, -- Air
				0.10, -- Sea
				0.0}; -- Other
				
		-- If the Soviets have Moscow and we are at war
		elseif lbControlMoscow and lbSovGerWar then
			laArray = {
				0.60, -- Land 
				0.23, -- Air
				0.12, -- Sea        
				0.05}; -- Other
		end
	end
	
	return laArray
end
-- Land ratio distribution

-- Lots of armor at the beginning for the Blitzkrieg, lots of infantry later to invade the USSR
function P.LandRatio(voProductionData)
	local laArray
	
	-- Build lots of infantry in the early years
	if voProductionData.Year <= 1939 and not(voProductionData.IsAtWar) then
		laArray = {
			garrison_brigade = 4,
			infantry_brigade = 20,
			motorized_brigade = 3,
			armor_brigade = 1};
	else
		laArray = {
			garrison_brigade = 4,
			infantry_brigade = 20,
			motorized_brigade = 3,
			mechanized_brigade = 1,
			armor_brigade = 1,
			heavy_armor_brigade = 0.5};
	end
	
	return laArray
end

-- Special Forces ratio distribution
function P.SpecialForcesRatio(voProductionData)
	local laRatio = nil
	local laUnits = nil
	local lbPara = voProductionData.TechStatus:IsUnitAvailable(CSubUnitDataBase.GetSubUnit("paratrooper_brigade"))

	if lbPara then
		laRatio = {
			25, -- Land
			1}; -- Special Force Unit

		laUnits = {
			bergsjaeger_brigade = 3,
			paratrooper_brigade = 0.5};
	end
	
	return laRatio, laUnits
end

-- Elite Units
function P.EliteUnits(voProductionData)
	local laUnits = {"waffen_brigade",
	                 "GER_ss"};
	
	return laUnits	
end

-- Which units should get 1 more Support unit with Superior Firepower tech
function P.FirePower(voProductionData)
	local laArray = {
		"waffen_brigade",
		"motorized_brigade",
		"mechanized_brigade",
		"armor_brigade",
		"GER_ss",
		"heavy_armor_brigade"};
		
	return laArray
end

-- Air ratio distribution
function P.AirRatio(voProductionData)
	local laArray = {
		interceptor = 6,
		multi_role = 2,
		rocket_interceptor = 1,
		cas = 3,
		tactical_bomber = 4,
		naval_bomber = 1};
	
	return laArray
end

-- Naval ratio distribution
function P.NavalRatio(voProductionData)
	local laArray = {
		destroyer = 4, -- Destroyers
		submarine = 6, -- Submarines
		heavy_cruiser = 1, -- Heavy Cruisers
		battleship = 1}; -- Battleship
	
	return laArray
end

-- Transport to Land unit distribution
function P.TransportLandRatio(voProductionData)
	local laArray
	local norTag = CCountryDataBase.GetTag('NOR')
	local loNorwayCountry = norTag:GetCountry()
	
	-- If Norway is present build more transports to invade it with
	if loNorwayCountry:Exists()
	and not(voProductionData.ministerCountry:GetRelation(norTag):HasAlliance()) then
		laArray = {
			80, -- Land
			1, -- Transport
			1}; -- Invasion
	else
		laArray = {
			120, -- Land
			1, -- Transport
			1}; -- Invasion
	end
	
	return laArray
end

-- Convoy Ratio control
--- NOTE: If goverment is in Exile these parms are ignored
function P.ConvoyRatio(voProductionData)
	local laArray = {
		10, -- Percentage extra (adds to 100 percent so if you put 10 it will make it 110% of needed amount)
		30, -- If Percentage extra is less than this it will force it up to the amount entered
		50, -- If Percentage extra is greater than this it will force it down to this
		3} -- Escort to Convoy Ratio (Number indicates how many convoys needed to build 1 escort)
  
	return laArray
end

-- Create Custom Motorized for France
function P.Build_motorized_brigade(vIC, viManpowerTotal, voType, voProductionData, viUnitQuantity)
	-- Build old style formations till 1940
	if voProductionData.Year < 1940 and not(voProductionData.IsAtWar) and not(voProductionData.IsExile) then
		local laSupportUnit = {
			"engineer_brigade",
			"armored_car_brigade",
			"sp_rct_artillery_brigade",
			"mot_aa_brigade",
			"sp_artillery_brigade"}
			
		voType.SecondaryMain = "tank_destroyer_brigade"
		voType.Support = 1
	end

	return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
end

-- Build Atleast 1 rocket test site
function P.Build_RocketTest(ic, voProductionData)
	return ic, true	
end

-- Never build and underground
function P.Build_Underground(ic, voProductionData)
	return ic, false
end

-- Have Germany Fortify the French Line
-- NOTE: if 2490 Province is not the last one please update the land production ratio
function P.Build_Fort(ic, voProductionData)
	-- Only build the forts if its 1938 or less
	if voProductionData.Year <= 1938 then
		ic = Support.Build_Fort(ic, voProductionData, 2751, 1) -- Karlsruhe
		ic = Support.Build_Fort(ic, voProductionData, 2882, 1) -- Baden
		ic = Support.Build_Fort(ic, voProductionData, 2948, 1) -- Offenburg
		ic = Support.Build_Fort(ic, voProductionData, 3016, 1) -- Freiburg
		ic = Support.Build_Fort(ic, voProductionData, 3084, 1) -- Lörrach (See Note)
	end
	return ic, false
end

function P.Build_CoastalFort(vIC, voProductionData)
	if voProductionData.Year > 1938 then
		return vIC, true
	end
	
	return vIC, false
end

function P.Build_Industry(vIC, voProductionData)
	-- Do not build factories till 1939 or if we go to war early
	if voProductionData.Year > 1940 then
		return vIC, true
	end
	
	return vIC, false
end

function P.Build_Infrastructure(vIC, voProductionData)
	if voProductionData.Year > 1937 then
		return vIC, true
	end
	
	return vIC, false
end

function P.Build_NavalBase(vIC, voProductionData)
	if voProductionData.Year > 1940 then
		return vIC, true
	end
	
	return vIC, false
end

function P.Build_AirBase(vIC, voProductionData)
	if voProductionData.Year > 1937 then
		return vIC, true
	end
	
	return vIC, false
end

function P.Build_AntiAir(vIC, voProductionData)
	if voProductionData.Year > 1937 then
		return vIC, true
	end
	
	return vIC, false
end

function P.Build_Radar(vIC, voProductionData)
	if voProductionData.Year > 1937 then
		return vIC, true
	end
	
	return vIC, false
end

-- END OF PRODUTION OVERIDES
-- #######################################


-- #######################################
-- Diplomacy Hooks
function P.HandleMobilization(minister)
	local ai = minister:GetOwnerAI()
	local ministerTag =  minister:GetCountryTag()
	local sovTag = CCountryDataBase.GetTag('SOV')
	local loRelation = ai:GetRelation(ministerTag, sovTag)

	-- We have a NAP with Russia so mobilize for Poland
	if loRelation:HasNap() then
		local liYear = CCurrentGameState.GetCurrentDate():GetYear()
		local liMonth = CCurrentGameState.GetCurrentDate():GetMonthOfYear()
		
		if liYear == 1939 and liMonth > 6 then
			ai:Post(CToggleMobilizationCommand( ministerTag, true ))
		end
	else
		local ministerCountry = ministerTag:GetCountry()
		local liTotalIC = ministerCountry:GetTotalIC()
		local liNeutrality = ministerCountry:GetNeutrality():Get() * 0.9

		-- Regular loop to see if anyone is threatening to us
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

function P.DiploScore_InfluenceNation(voDiploScoreObj)
	-- Only do this if we are in the axis
	if voDiploScoreObj.FactionName == "axis" then
		local loInfluences = {
			JAP = {Score = 500},
			ITA = {Score = 500},
			ROM = {Score = 200, Province = 3377, Year = 1942},
			BUL = {Score = 200, Province = 4058, Year = 1942},
			FIN = {Score = 200, Province = 698, Year = 1942},
			HUN = {Score = 300}}
	
		-- Are they on our list
		if loInfluences[voDiploScoreObj.TargetName] then
			if loInfluences[voDiploScoreObj.TargetName].Province then
				if loInfluences[voDiploScoreObj.TargetName].Year <= voDiploScoreObj.Year then
					if CCurrentGameState.GetProvince(loInfluences[voDiploScoreObj.TargetName].Province):GetOwner() ~= voDiploScoreObj.TargetTag then
						return 0 -- No need to influence, assume they will align
					end
				end
			end
			
			return (voDiploScoreObj.Score + loInfluences[voDiploScoreObj.TargetName].Score)
		end
	end

	return voDiploScoreObj.Score
end

function P.DiploScore_OfferTrade(voDiploScoreObj)
	local laTrade = {
		SOV = {Score = 50},
		SWE = {Score = 100},
		ITA = {Score = 200},
		TUR = {Score = 50},
		FIN = {Score = 100},
		BUL = {Score = 100},
		ROM = {Score = 100},
		HUN = {Score = 100},
		VIC = {Score = 50},
		SPA = {Score = 50},
		SPR = {Score = 50},
		ENG = {Score = -20},
		FRA = {Score = -20},
		POR = {Score = 30}}
	
	if laTrade[voDiploScoreObj.TagName] then
		return voDiploScoreObj.Score + laTrade[voDiploScoreObj.TagName].Score
	else
		-- If we are the buyer dont bother as we wont get our stuff but we can sell to them
		if tostring(voDiploScoreObj.BuyerTag) == voDiploScoreObj.TagName then
			-- If we are atwar dont do buy trades (sell is ok)
			if voDiploScoreObj.BuyerCountry:IsAtWar() then
				if voDiploScoreObj.SellerContinent ~= voDiploScoreObj.BuyerContinent and not(voDiploScoreObj.IsNeighbor) then
					return 0
				end
			end
		end
	end
	
	return voDiploScoreObj.Score
end

function P.DiploScore_Alliance( viScore, ai, actor, recipient, observer)
	local loAxis = CCurrentGameState.GetFaction("axis")
	
	-- Make sure we are part of the Axis
	if voDiploScoreObj.Faction == loAxis then
		local lsTargetTag = voDiploScoreObj.TargetTag
		
		if lsTargetTag == 'AUS' then -- we got better plans for you...
			voDiploScoreObj.Score = 0
		end
	end
	
	return voDiploScoreObj.Score
end

function P.DiploScore_JoinFactionGoal(viScore, ai, actor, recipient, observer, goal )
	local lsSender = tostring(actor)
	local ministerCountry = observer:GetCountry()
	
	if lsSender == 'AUS' then -- we got better plans for you...
		return 0
	elseif lsSender == 'FIN' then
		return 0
	end
    
    return viScore
end

--##########################
-- Foreign Minister Hooks
function P.ForeignMinister_Influence(voForeignMinisterData)
	local laIgnoreWatch -- Ignore this country but monitor them if they are about to join someone else
	local laWatch -- Monitor them and also fi their score is high enough they can be influenced normally
	local laIgnore -- Ignore them completely

	if voForeignMinisterData.FactionName == "axis" then
		laWatch = {
			"NOR", -- Norway
			"BUL", -- Bulgaria
			"FIN", -- Finland
			"ROM", -- Romania
			"HUN"};	 -- Hungary
			
		laIgnoreWatch = {
			"TUR", -- Turkey
			"SPA", -- Spain
			"SPR", -- Republic Spain
			"POR", -- Portugal
			"SWE", -- Sweden
			"CHI", -- China
			"BEL", -- Belgium
			"HOL"} -- Holland
			
		laIgnore = {
			"AST", -- Australia
			"CAN", -- Canada
			"SAF", -- South Africa
			"NZL", -- New Zealand
			"AUS", -- Austria
			"CZE", -- Czechoslovakia
			"SCH", -- Switzerland
			"LAT", -- Lativia
			"LIT", -- Lithuania
			"EST", -- Estonia
			"LUX", -- Luxemburg
			"VIC", -- Vichy
			"DEN", -- Denmark
			"POL", -- Poland
			"ETH", -- Ethiopia			
			"CYN", -- Yunnan
			"SIK", -- Sikiang
			"CGX", -- Guangxi Clique
			"CSX", -- Shanxi
			"TIB", -- Tibet
			"AFG", -- Afghanistan
			"CHC"};	-- Communist China
	end
	
	return laWatch, laIgnoreWatch, laIgnore
end

function P.ForeignMinister_EvaluateDecision(voDecision, voForeignMinisterData)
	local loDecisions = {
		molotov_ribbentrop_pact = {Year = 1939, Month = 7, Day = 5, Score = 100},
		danzig_or_war = {Year = 1939, Month = 8, Day = 0, War = true, Country = "POL", Score = 100 },
		anschluss_of_austria = {Year = 1938, Month = 2, Day = 8, Score = 100 },
		the_treaty_of_munich = {Year = 1938, Month = 8, Day = 8, Score = 100 },
		first_vienna_award = {Year = 1939, Month = 2, Day = 25, Score = 100 },
		independent_croatia = {Year = 1936, Month = 0, Day = 0, Score = 0 }}

	if loDecisions[voDecision.Name] then
		if (voForeignMinisterData.Year == loDecisions[voDecision.Name].Year
		and voForeignMinisterData.Month >= loDecisions[voDecision.Name].Month
		and voForeignMinisterData.Day >= loDecisions[voDecision.Name].Day )
		or
		(voForeignMinisterData.Year == loDecisions[voDecision.Name].Year
		and voForeignMinisterData.Month > loDecisions[voDecision.Name].Month)
		or
		(voForeignMinisterData.Year > loDecisions[voDecision.Name].Year) then
			if loDecisions[voDecision.Name].War then
				voForeignMinisterData.Strategy:PrepareWarDecision(CCountryDataBase.GetTag(loDecisions[voDecision.Name].Country), 100, voDecision.Decision, false)
			else
				return loDecisions[voDecision.Name].Score
			end
		else
			return 0
		end
	end
	
	return voDecision.Score
end

function P.ForeignMinister_CallAlly(voForeignMinisterData)
	-- Make sure Germany is in the Axis and if not let default code run
	if voForeignMinisterData.FactionName ~= "axis" then
		return true
	end
	
	-- Get a list of all your allies
	local laAllies = {}
	for loAllyTag in voForeignMinisterData.ministerCountry:GetAllies() do
		local loAllyCountry = loAllyTag:GetCountry()

		-- Exclude Puppets from this list
		if not(loAllyCountry:IsPuppet()) then
			local loAlly = {
				AllyTag = loAllyTag,
				AllyCountry = loAllyCountry,
				Continent = tostring(loAllyCountry:GetActingCapitalLocation():GetContinent():GetTag()),
			}
			
			laAllies[tostring(loAllyTag)] = loAlly
		end
	end
	
	local loSOVtag = CCountryDataBase.GetTag("SOV")
	
	-- Go through our Wars
	for loDiploStatus in voForeignMinisterData.ministerCountry:GetDiplomacy() do
		local loTargetTag = loDiploStatus:GetTarget()
		
		if loTargetTag:IsValid() and loDiploStatus:HasWar() then
			local loWar = loDiploStatus:GetWar()
			
			if loWar:IsLimited() then
				local lsTargetTag = tostring(loTargetTag)
				local liWarMonths = loWar:GetCurrentRunningTimeInMonths()
				local loTargetCountry = loTargetTag:GetCountry()
				
				-- Call in all potential allies
				for k, v in pairs(laAllies) do
					-- Check to see if the two are already at war?
					if not(v.AllyCountry:GetRelation(loTargetTag):HasWar()) then
						-- Call noone whose capital is in Asia and the target is not USA
						if v.Continent ~= "asia" and lsTargetTag ~= "USA" then
							-- We are desperate so overide all else statements
							if lsTargetTag == "SOV" then
								if (liWarMonths > 5) or voForeignMinisterData.humanTag == loSOVtag then
									Support.ExecuteCallAlly(voForeignMinisterData.ministerAI, voForeignMinisterData.ministerTag, v, loTargetTag)
								end
								
							-- When to Call Italy into the war
							elseif k == "ITA" then
								local loParisFaction = CCurrentGameState.GetProvince(2613):GetController():GetCountry():GetFaction()
								local loBruxellesFaction = CCurrentGameState.GetProvince(2311):GetController():GetCountry():GetFaction()
								
								if loParisFaction == voForeignMinisterData.Faction -- Paris
								or loBruxellesFaction == voForeignMinisterData.Faction -- Bruxelles
								or liWarMonths > 10	then
									Support.ExecuteCallAlly(voForeignMinisterData.ministerAI, voForeignMinisterData.ministerTag, v, loTargetTag)
								end
							-- Standard catch all call anyone on our enemies border
							elseif lsTargetTag ~= "POL" then
								if voForeignMinisterData.Year <= 1941 then
									-- Do not call Allies who are on the border with Russia
									if not(v.AllyCountry:IsNonExileNeighbour(loSOVtag)) then
										-- Call in Allies that are neighbors to our enemy
										if loTargetCountry:IsNonExileNeighbour(v.AllyTag) then
											Support.ExecuteCallAlly(voForeignMinisterData.ministerAI, voForeignMinisterData.ministerTag, v, loTargetTag)
										end
									end
								else
									-- Call in Allies that are neighbors to our enemy
									if loTargetCountry:IsNonExileNeighbour(v.AllyTag) then
										Support.ExecuteCallAlly(voForeignMinisterData.ministerAI, voForeignMinisterData.ministerTag, v, loTargetTag)
									end
								end
							end
						elseif lsTargetTag == "USA" then
							-- It is USA so call everyone
							Support.ExecuteCallAlly(voForeignMinisterData.ministerAI, voForeignMinisterData.ministerTag, v, loTargetTag)
						end
					end
				end
			end
		end
	end
	
	return false
end

function P.ForeignMinister_MilitaryAccess(voForeignMinisterData)
	-- Make sure Germany is in the Axis and if not let default code run
	if voForeignMinisterData.FactionName == "axis" then
		-- Special Access Checks for Sweden
		if voForeignMinisterData.IsAtWar then
			local loSWETag = CCountryDataBase.GetTag("SWE")
			local loRelationSwe = voForeignMinisterData.ministerAI:GetRelation(voForeignMinisterData.ministerTag, loSWETag)
			
			if not(loRelationSwe:HasMilitaryAccess()) then
				if not(voForeignMinisterData.Strategy:IsPreparingWarWith(loSWETag)) then
					local loOsloFaction = CCurrentGameState.GetProvince(812):GetController():GetCountry():GetFaction()
					
					-- Ask Sweden for Access if Oslo is controlled by someone in the Axis
					if loOsloFaction == voForeignMinisterData.Faction then
						local loCommand = CMilitaryAccessAction(voForeignMinisterData.ministerTag, loSWETag)

						if loCommand:IsSelectable() then
							local liScore = DiploScore_DemandMilitaryAccess(voForeignMinisterData.ministerAI, voForeignMinisterData.ministerTag, loSWETag, voForeignMinisterData.ministerTag)

							if liScore > 50 then
								voForeignMinisterData.minister:Propose(loCommand, liScore)
							end
						end
					end
				end
			end
		end
	end
	
	return true
end

function P.Intel_Priority(voIntelligenceData, voIntelCountry)
	local liReturn = 0
	
	if not(voIntelCountry.IsExiled) then
		-- Annexation of Austria has an or clause so support our party 
		if not(voIntelligenceData.IsAtWar) and voIntelligenceData.Year <= 1938 then
			if tostring(voIntelCountry.ministerTag) == "AUS" then
				liReturn = 3
			end
		end
	end
	
	return liReturn
end

function P.Intel_Mission(voIntelligenceData, voIntelCountry)
	local liReturn = nil
	
	if not(voIntelCountry.IsExiled) then
		-- Annexation of Austria has an or clause so support our party 
		if not(voIntelligenceData.IsAtWar) and voIntelligenceData.Year <= 1938 then
			if tostring(voIntelCountry.ministerTag) == "AUS" then
				liReturn = SpyMission.SPYMISSION_BOOST_OUR_PARTY
			end
		end
	end
	
	return liReturn
end

function P.ForeignMinister_ProposeWar(voForeignMinisterData)
	if not(voForeignMinisterData.Strategy:IsPreparingWar()) then
		if voForeignMinisterData.FactionName == "axis" then	
			local loSOVTag = CCountryDataBase.GetTag("SOV")
			local loGerSovRelation = voForeignMinisterData.ministerCountry:GetRelation(loSOVTag)
			
			-- If we are atwar with Russia then do not even think of DOWing anyone else
			if not(loGerSovRelation:HasWar()) then
				local laWarTags = {}
				local laPeaceTags = {}
				local liTotalNeighborWars = 0
				
				local loUSATag = CCountryDataBase.GetTag("USA")
				local loUsaSovRelation = voForeignMinisterData.ministerCountry:GetRelation(loUSATag)
				local loUSAWar = loUsaSovRelation:HasWar()
				
				-- Since atwar figure out how many fronts we have
				for loNeighborTag in voForeignMinisterData.ministerCountry:GetControllerNeighbours() do
					local lsNeighborTag = tostring(loNeighborTag)
					local loNeighborRelation = voForeignMinisterData.ministerCountry:GetRelation(loNeighborTag)
					local loNeighborCountry = loNeighborTag:GetCountry()
					
					if loNeighborRelation:HasWar() then
						laWarTags[lsNeighborTag] = {
							Tag = loNeighborTag,
							Country = loNeighborCountry,
							Realtion = loNeighborRelation}

						-- If we are at war with USA count everyone
						if loUSAWar then
							liTotalNeighborWars = liTotalNeighborWars + 1
						else
							if lsNeighborTag ~= "NOR" then
								-- Do not count these as a front
								if lsNeighborTag ~= "ENG"
								or lsNeighborTag ~= "LUX"
								or lsNeighborTag ~= "BEL" 
								or lsNeighborTag ~= "HOL" then
									if lsNeighborTag == "SPA" or lsNeighborTag == "SPR" then
										local loGibraltarTag = CCurrentGameState.GetProvince(5191):GetController()
										local loGibraltarRelation = voForeignMinisterData.ministerCountry:GetRelation(loGibraltarTag)
										
										-- Gibraltar check, if Germany is not at war with the owner in case something weird happens
										if loGibraltarRelation:HasWar() then 
											liTotalNeighborWars = liTotalNeighborWars + 1
										end
									else
										liTotalNeighborWars = liTotalNeighborWars + 1
									end
								end
							end
						end
					else
						if lsNeighborTag == "SOV" then
							if not(voForeignMinisterData.ministerCountry:GetFlags():IsFlagSet("su_signs_peace")) then
								if Support.GoodToWarCheck(loNeighborTag, loNeighborCountry, voForeignMinisterData, true, true, true, true) then
									-- Add them to an array in case we do not have neighbors and need a target
									laPeaceTags[lsNeighborTag] = {
										Tag = loNeighborTag,
										Name = lsNeighborTag,
										Country = loNeighborCountry,
										IsFriend = Support.IsFriend(voForeignMinisterData.ministerAI, voForeignMinisterData.Faction, loNeighborCountry),
										Realtion = loNeighborRelation}
								end
							end
						else
							-- Make sure they are ok for us to DOW
							if Support.GoodToWarCheck(loNeighborTag, loNeighborCountry, voForeignMinisterData, true, true) then
								laPeaceTags[lsNeighborTag] = {
									Tag = loNeighborTag,
									Name = lsNeighborTag,
									Country = loNeighborCountry,
									IsFriend = Support.IsFriend(voForeignMinisterData.ministerAI, voForeignMinisterData.Faction, loNeighborCountry),
									Realtion = loNeighborRelation}
							end
						end
					end
				end		

				-- Only process these requests if prior to 1941 for performance reasons
				--   no reason for Germany to constantly monitor countries after this time
				if voForeignMinisterData.IsAtWar and voForeignMinisterData.Year <= 1940 then
					-- If we have more than one front do nothing
					if liTotalNeighborWars <= 1 then
						P.DenmarkCheck(voForeignMinisterData, laPeaceTags)
						P.NorwayCheck(voForeignMinisterData)
						
						-- Plan the Low Countries, Denmark and Norway invasions
						if laWarTags["FRA"] then
							P.LowCountriesCheck(voForeignMinisterData, laPeaceTags)
						else
							-- Check if we need to invade Low Countries
							if not(P.LowCountriesCheck(voForeignMinisterData, laPeaceTags, true)) then
								if liTotalNeighborWars == 0	and (math.random(100) < 10) then
									if not(P.YugoslaviaCheck(voForeignMinisterData, laPeaceTags)) then
										P.GreeceCheck(voForeignMinisterData, laPeaceTags)
									end
								end
							end
						end
					end
					
				-- Something happend and no war so start one in 1940
				elseif not(voForeignMinisterData.IsAtWar) and voForeignMinisterData.Year == 1940 then
					-- Declare random war on one of our neighbors
					for k, v in pairs(laPeaceTags) do
						if math.random(100) < 25 and not(v.IsFriend) then
							voForeignMinisterData.Strategy:PrepareLimitedWar(laPeaceTags[k].Tag, 100)
						end
					end
					
				-- We have no fronts
				elseif liTotalNeighborWars == 0 then
					-- Only look at this if we are at war or the year is 1941 or greater
					if voForeignMinisterData.IsAtWar or voForeignMinisterData.Year >= 1941 then
						local lbYugoWar = false
						local lbGreeceWar = false
						
						P.DenmarkCheck(voForeignMinisterData, laPeaceTags)
						P.NorwayCheck(voForeignMinisterData)
						
						-- Low Countries check
						if not(P.LowCountriesCheck(voForeignMinisterData, laPeaceTags, true)) then
							-- Look at the Baklans for a war
							if voForeignMinisterData.IsAtWar then
								lbYugoWar = P.YugoslaviaCheck(voForeignMinisterData, laPeaceTags)
								
								-- Do not do the Greece check if Yugo is starting as it already called GreeceCheck
								if lbYugoWar then
									lbGreeceWar = P.GreeceCheck(voForeignMinisterData, laPeaceTags)
								end
							end
						
							if not(lbYugoWar) and not(lbGreeceWar) then
								-- Potential Wars with neighbors
								for k, v in pairs(laPeaceTags) do
									if math.random(100) < 25 and not(v.IsFriend) then
										if v.Name == "SOV" then
											if voForeignMinisterData.Month >= 4 and voForeignMinisterData.Month <= 7 then
												local loParisFaction = CCurrentGameState.GetProvince(2613):GetController():GetCountry():GetFaction()
												-- If an Axis member controls Paris then consider DOWing Soviets
												if loParisFaction == voForeignMinisterData.Faction then
													voForeignMinisterData.Strategy:PrepareLimitedWar(laPeaceTags[k].Tag, 100)
												end
											end
										else
											voForeignMinisterData.Strategy:PrepareLimitedWar(laPeaceTags[k].Tag, 100)
										end
									end
								end
							end
						end
					end
				end
			end
		end
	end
end

function P.DenmarkCheck(voForeignMinisterData, vaPeaceTags)
	if vaPeaceTags["DEN"] then
		voForeignMinisterData.Strategy:PrepareLimitedWar(vaPeaceTags["DEN"].Tag, 100)
		return true
	end
	
	return false
end

function P.NorwayCheck(voForeignMinisterData)
	-- Took to long to DOW so don't do it or we are desperate
	if voForeignMinisterData.Year > 1940 or voForeignMinisterData.Desperation > 0.4 then
		return false
	end
	
	-- Wait for Good Weather
	if voForeignMinisterData.Month >= 2 and voForeignMinisterData.Month <= 8 then
		-- Check to see if we have the Invasion Craft for this
		local laTempCurrent = voForeignMinisterData.ministerAI:GetDeployedSubUnitCounts()
		
		for loUnit in CSubUnitDataBase.GetSubUnitList() do
			local lsUnitType = loUnit:GetKey():GetString() 

			if lsUnitType == "transport_ship" then
				if laTempCurrent:GetAt(loUnit:GetIndex()) < 3 then
					return false
				end
			end
		end
		-- End of Transport check
		
		local loCopenhagenFaction = CCurrentGameState.GetProvince(1482):GetController():GetCountry():GetFaction()

		-- If someone in our Faction controls Copenhagen
		if loCopenhagenFaction == voForeignMinisterData.Faction then
			local loNORTag = CCountryDataBase.GetTag("NOR")
			local loNorwayCountry = loNORTag:GetCountry()
			local lbDOW = Support.GoodToWarCheck(loNORTag, loNorwayCountry, voForeignMinisterData, true, true)
			
			if lbDOW then
				voForeignMinisterData.Strategy:PrepareLimitedWar(loNORTag, 100)
			end			
			
			return lbDOW
		end
	end
	
	return false
end

function P.LowCountriesCheck(voForeignMinisterData, vaPeaceTags, vbOveride)
	local lbDOW = false

	-- Wait for good weather months to attack
	if (voForeignMinisterData.Month >= 3 and voForeignMinisterData.Month <= 7) or vbOveride then
		-- Belgium Check
		if vaPeaceTags["BEL"] then
			voForeignMinisterData.Strategy:PrepareLimitedWar(vaPeaceTags["BEL"].Tag, 100)
			lbDOW = true
		end
		
		-- Netherlands Check
		if vaPeaceTags["HOL"] then
			voForeignMinisterData.Strategy:PrepareLimitedWar(vaPeaceTags["HOL"].Tag, 100)
			lbDOW = true
		end
		
		-- Luxemburg Check
		if vaPeaceTags["LUX"] then
			voForeignMinisterData.Strategy:PrepareLimitedWar(vaPeaceTags["LUX"].Tag, 100)
			lbDOW = true
		end
	end
	
	return lbDOW
end

function P.YugoslaviaCheck(voForeignMinisterData, vaPeaceTags)
	if vaPeaceTags["YUG"] then
		voForeignMinisterData.Strategy:PrepareLimitedWar(vaPeaceTags["YUG"].Tag, 100)
		P.GreeceCheck(voForeignMinisterData, vaPeaceTags, true)
		return true
	end
	
	return false
end

function P.GreeceCheck(voForeignMinisterData, vaPeaceTags, vbYugoDOW)
	if not(vaPeaceTags["GRE"]) then
		local loGRETag = CCountryDataBase.GetTag("GRE")
		local loGreeceCountry = loGRETag:GetCountry()

		if Support.GoodToWarCheck(loGRETag, loGreeceCountry, voForeignMinisterData, vbYugoDOW, true) then
			voForeignMinisterData.Strategy:PrepareLimitedWar(loGRETag, 100)
			return true
		end
	else
		voForeignMinisterData.Strategy:PrepareLimitedWar(vaPeaceTags["GRE"].Tag, 100)
		return true
	end
	
	return false
end

--##########################
-- Politics Minister Hooks

-- Create very highly trained troops
function P.CallLaw_training_laws(minister, voCurrentLaw)
	local _SPECIALIST_TRAINING_ = 30
	return CLawDataBase.GetLaw(_SPECIALIST_TRAINING_)
end

-- we dont bother accepting for LL to JAP, its too dangerous and not very historical
function P.DiploScore_RequestLendLease( liScore, voAI, voActorTag )
	if tostring(voActorTag) == "JAP" then
		return 0
	end
	return liScore
end

return AI_GER
