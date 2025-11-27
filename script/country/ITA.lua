
local P = {}
AI_ITA = P

-- #######################################
-- Start of Trade Weights
function P.TradeWeights(voResourceData)
	local laResouces = {
		CRUDE_OIL = {
			Buffer = 3,
			BufferSaleCap = 3000},
		FUEL = {
			Buffer = 0.5,
			BufferSaleCap = 1000}}
	
	return laResouces
end

-- #######################################
-- Start of Tech Research
-- Tech weights
--   1.0 = 100% the total needs to equal 1.0
function P.TechWeights(voTechnologyData)
	local laTechWeights = {
		0.22, -- landBasedWeight
		0.09, -- landDoctrinesWeight
		0.12, -- airBasedWeight
		0.15, -- airDoctrinesWeight
		0.18, -- navalBasedWeight
		0.10, -- navalDoctrinesWeight
		0.10, -- industrialWeight
		0.0, -- SophisticatedEquipmentTechs
		0.0, -- secretWeaponsWeight
		0.04}; -- otherWeight
	
	return laTechWeights
end

-- Techs that are used in the main file to be ignored
--   techname|level (level must be 1-9 a 0 means ignore all levels
--   use as the first tech name the word "all" and it will cause the AI to ignore all the techs
function P.LandTechs(voTechnologyData)
	local ignoreTech = {
			{"cavalry_smallarms", 3}, 
			{"cavalry_support", 3},
			{"cavalry_guns", 3}, 
			{"cavalry_at", 3},
			{"marine_infantry", 0},
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
			{"super_heavy_tank_brigade", 0},
			{"super_heavy_tank_gun", 0},
			{"super_heavy_tank_engine", 0},
			{"super_heavy_tank_armour", 0},
			{"super_heavy_tank_reliability", 0},
			{"rocket_art", 0},
			{"rocket_art_ammo", 0},
			{"rocket_carriage_sights", 0}};
		
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
		"art_barrell_ammo",
		"art_carriage_sights"};
		
	return ignoreTech, preferTech
end

function P.LandDoctrinesTechs(voTechnologyData)
	local ignoreTech = {
		{"mobile_warfare", 0},
		{"operational_level_command_structure", 0},
		{"spearhead_doctrine", 0},
		{"schwerpunkt", 0},
		{"elastic_defence", 0},
		{"blitzkrieg", 0},
		{"superior_firepower", 0},
		{"large_formations", 0},
		{"guerilla_warfare", 0}};
		
	local preferTech = {
		"tactical_command_structure",
		"mechanized_offensive",
		"delay_doctrine",
		"combined_arms_warfare",
		"infantry_warfare",
		"mass_assault",
		"peoples_army",
		"central_planning",
		"assault_concentration",
		"operational_level_organisation"};	
		
	return ignoreTech, preferTech
end

function P.AirTechs(voTechnologyData)
	local ignoreTech = {
		{"basic_strategic_bomber", 0},
		{"large_fueltank", 0},
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
		{"cas_pilot_training", 0}, 
		{"cas_groundcrew_training", 0}, 
		{"ground_attack_tactics", 0}, 
		{"heavy_bomber_pilot_training", 0},
		{"heavy_bomber_groundcrew_training", 0},
		{"strategic_bombardment_tactics", 0},
		{"strategic_air_command", 0}};

	local preferTech = {
		"fighter_pilot_training",
		"interception_tactics",
		"tac_pilot_training",
		"interdiction_tactics",		
		"nav_pilot_training",
		"navalstrike_tactics",
		"naval_air_targeting",
		"naval_tactics"};		
		
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
-- Production Overides the main LUA with country specific ones

-- Production Weights
--   1.0 = 100% the total needs to equal 1.0
function P.ProductionWeights(voProductionData)
	local laArray
	
	-- Check to see if manpower is to low
	-- More than 200 brigades so build stuff that does not use manpower
	if (voProductionData.ManpowerTotal < 100 and voProductionData.LandCountTotal > 100)
	or voProductionData.ManpowerTotal < 50 then
		laArray = {
			0.0, -- Land
			0.52, -- Air
			0.46, -- Sea
			0.01}; -- Other	
	elseif voProductionData.IsAtWar then
		laArray = {
			0.34, -- Land
			0.25, -- Air
			0.40, -- Sea
			0.01}; -- Other
	else
		laArray = {
			0.36, -- Land
			0.25, -- Air
			0.38, -- Sea
			0.01}; -- Other
	end
	
	return laArray
end
-- Land ratio distribution
function P.LandRatio(voProductionData)
	local laArray = {
		garrison_brigade = 4,
		infantry_brigade = 20,
		motorized_brigade = 4,
		mechanized_brigade = 1,
		light_armor_brigade = 1};
	
	return laArray
end
-- Special Forces ratio distribution
function P.SpecialForcesRatio(voProductionData)
	local laRatio = {
		10, -- Land
		1}; -- Special Force Unit

	local laUnits = {bergsjaeger_brigade = 3,
		paratrooper_brigade = 0.5};
	
	return laRatio, laUnits	
end

-- Elite Units
function P.EliteUnits(voProductionData)
	local laUnits = {"alpini_brigade","camicie_nere_brigade"};
	
	return laUnits	
end

-- Air ratio distribution
function P.AirRatio(voProductionData)
	local laArray = {
		interceptor = 3,
		multi_role = 1,
		tactical_bomber = 2,
		naval_bomber = 4};
	
	return laArray
end
-- Naval ratio distribution
function P.NavalRatio(voProductionData)
	local laArray = {
		destroyer = 4,
		submarine = 2,
		heavy_cruiser = 2,
		battleship = 1};
	
	return laArray
end

-- Transport to Land unit distribution
function P.TransportLandRatio(voProductionData)
	local laArray = {
		12, -- Land
		1,  -- transport
		1}  -- invasion craft
  
	return laArray
end

-- Convoy Ratio control
--- NOTE: If goverment is in Exile these parms are ignored
function P.ConvoyRatio(voProductionData)
	local laArray = {
		10, -- Percentage extra (adds to 100 percent so if you put 10 it will make it 110% of needed amount)
		50, -- If Percentage extra is less than this it will force it up to the amount entered
		70, -- If Percentage extra is greater than this it will force it down to this
		5} -- Escort to Convoy Ratio (Number indicates how many convoys needed to build 1 escort)
  
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

-- END OF PRODUTION OVERIDES
-- #######################################

function P.HandlePuppets(minister)
	local ministerTag =  minister:GetCountryTag()
	local ministerCountry = ministerTag:GetCountry()
	
	for loPuppetTag in ministerCountry:GetPossiblePuppets() do
		if tostring(loPuppetTag) == "ETH" then
			minister:GetOwnerAI():Post(CCreateVassalCommand(loPuppetTag, ministerTag))
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
		GER = {Score = 100},
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

function P.DiploScore_InviteToFaction(voDiploScoreObj)
	-- If we are not part of the same ideology as the requesting country do not even consider it
	if not(voDiploScoreObj.IdeologyGroup == voDiploScoreObj.TargetIdeologyGroup) then
		voDiploScoreObj.Score = 0
	end
	
	return voDiploScoreObj.Score	
end

function P.DiploScore_Alliance(voDiploScoreObj)
	-- If we are not part of the same ideology as the requesting country do not even consider it
	if not(voDiploScoreObj.IdeologyGroup == voDiploScoreObj.TargetIdeologyGroup) then
		voDiploScoreObj.Score = 0
	end
	
	return voDiploScoreObj.Score	
end

--##########################
-- Foreign Minister Hooks
function P.ForeignMinister_EvaluateDecision(voDecision, voForeignMinisterData)
	if voDecision.Name == "the_future_of_greece" then
		local fraTag = CCountryDataBase.GetTag("FRA")
		local vicTag = CCountryDataBase.GetTag("VIC")
		local loVicCountry = vicTag:GetCountry()
		
		-- Vichy exists go for Greece
		if loVicCountry:Exists() then
			voForeignMinisterData.Strategy:PrepareWarDecision(CCountryDataBase.GetTag("GRE"), 100, voDecision.Decision, false)
		
		-- 10% random Chance and Check to see if France no longer controls Paris		
		elseif not(CCurrentGameState.GetProvince(2613):GetController() == fraTag)
		and math.random(100) <= 10 then
			voForeignMinisterData.Strategy:PrepareWarDecision(CCountryDataBase.GetTag("GRE"), 100, voDecision.Decision, false)
		end
		
		return 0
		
	elseif voDecision.Name == "annexation_of_albania" then
		voForeignMinisterData.Strategy:PrepareWarDecision(CCountryDataBase.GetTag("ALB"), 100, voDecision.Decision, false)
		return 0
	end
	
	return voDecision.Score
end

function P.ForeignMinister_ProposeWar(voForeignMinisterData)
	-- Only process this if Italy is part of the Axis
	if not(voForeignMinisterData.Strategy:IsPreparingWar()) then
		if voForeignMinisterData.FactionName == "axis" then
		
			-- Greece Check
			if voForeignMinisterData.Month >= 2 and voForeignMinisterData.Month <= 9 then
				local loParisFaction = CCurrentGameState.GetProvince(2613):GetController():GetCountry():GetFaction()
			
				-- Do the Axis Control Paris
				if loParisFaction == voForeignMinisterData.Faction then
					local loAlliesTag = CCurrentGameState.GetFaction("allies"):GetFactionLeader()
					local loAxisCountry = voForeignMinisterData.Faction:GetFactionLeader():GetCountry()
					
					local loAxisAlliesRelation = loAxisCountry:GetRelation(loAlliesTag)
					
					if loAxisAlliesRelation:HasWar() then
						local loWar = loAxisAlliesRelation:GetWar()
						local liWarMonths = loWar:GetCurrentRunningTimeInMonths()
						
						-- Check to see if the war has been going on for a while
						if liWarMonths > 11 then
							local loGRETag = CCountryDataBase.GetTag('GRE')
							local loGreeceCountry = loGRETag:GetCountry()
						
							if Support.GoodToWarCheck(loGRETag, loGreeceCountry, voForeignMinisterData, false, true) then
								voForeignMinisterData.Strategy:PrepareWar(loGRETag, 100)
							end
						end
					end
				end
			end
		end
	end
end

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

return AI_ITA
