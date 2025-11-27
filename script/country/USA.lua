
local P = {}
AI_USA = P

-- #######################################

-- Tech weights
--   1.0 = 100% the total needs to equal 1.0
function P.TechWeights(voTechnologyData)
	local laTechWeights = {
		0.18, -- landBasedWeight
		0.09, -- landDoctrinesWeight
		0.12, -- airBasedWeight
		0.15, -- airDoctrinesWeight
		0.18, -- navalBasedWeight
		0.10, -- navalDoctrinesWeight
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
			{"amphibious_invasion_technology", 3},
			{"amphibious_assault_units", 3}
		}
	else
		ignoreTech = {
			{"cavalry_smallarms", 3}, 
			{"cavalry_support", 3},
			{"cavalry_guns", 3}, 
			{"cavalry_at", 3},
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
			{"lighttank_reliability", 2},
			{"amphibious_invasion_technology", 3},
			{"amphibious_assault_units", 3}
		}
	end
		
	local preferTech = {
		"cavalry_smallarms",
		"mechanised_infantry",
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
		"art_carriage_sights",
		"advanced_invasion_craft",
		"amphibious_invasion_craft",
		"amphibious_invasion_technology",
		"amphibious_assault_units",
		"at_ammo_muzzel",
		"at_barrell_sights"
	}
		
	return ignoreTech, preferTech
end

function P.LandDoctrinesTechs(voTechnologyData)
	local ignoreTech = {
		{"guerilla_warfare", 0},
		{"large_formations", 0}};
		
	local preferTech = {
		"superior_firepower",
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
	local ignoreTech = {};

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
		"basic_bomb",
		"nav_development",
		"air_launched_torpedo"};
		
	return ignoreTech, preferTech
end

function P.AirDoctrineTechs(voTechnologyData)
	local ignoreTech = {
		{"forward_air_control", 0},
		{"battlefield_interdiction", 0},
		{"bomber_targerting_focus", 0},
		{"fighter_targerting_focus", 0}};

	local preferTech = {
		"fighter_pilot_training",
		"interception_tactics",
		"ground_attack_tactics",
		"tac_pilot_training",
		"interdiction_tactics",
		"tactical_air_command",
		"nav_pilot_training",
		"portstrike_tactics",
		"navalstrike_tactics",
		"naval_air_targeting",
		"naval_tactics"};		
		
	return ignoreTech, preferTech
end
		
function P.NavalTechs(voTechnologyData)
	local ignoreTech = {
		{"battlecruiser_technology", 0},
		{"battlecruiser_antiaircraft", 0},
		{"battlecruiser_engine", 0},
		{"battlecruiser_armour", 0},
		{"super_heavy_battleship_technology", 0}};

	local preferTech = {
		"largewarship_radar",
		"smallwarship_radar",
		"light_ship_fire_control_system",
		"heavy_ship_fire_control_system",
		"destroyer_technology",
		"smallnaval_gun",
		"smallnaval_torpedo",
		"light_boats_antiaircraft",
		"smallnaval_engine",
		"smallnaval_armour",
		"cag_development",
		"escort_carrier_technology",
		"carrier_technology",
		"carrier_antiaircraft",
		"carrier_engine",
		"carrier_armour",
		"carrier_hanger"};		
		
	return ignoreTech, preferTech
end
		
function P.NavalDoctrineTechs(voTechnologyData)
	local ignoreTech = {};

	local preferTech = {
		"fleet_auxiliary_carrier_doctrine",
		"light_cruiser_escort_role",
		"carrier_group_doctrine",
		"light_cruiser_crew_training",
		"carrier_crew_training",
		"carrier_task_force",
		"naval_underway_repleshment",
		"radar_training",
		"sea_lane_defence",
		"destroyer_escort_role",
		"battlefleet_concentration_doctrine",
		"destroyer_crew_training",
		"battleship_crew_training",
		"commerce_defence",
		"fire_control_system_training",
		"commander_decision_making",
		"cruiser_warfare",
		"cruiser_crew_training",
		"spotting",
		"basing"};		
		
	return ignoreTech, preferTech
end

function P.IndustrialTechs(voTechnologyData)
	local ignoreTech = {};

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
-- Production Overides the main LUA with country specific ones

-- Production Weights
--   1.0 = 100% the total needs to equal 1.0
function P.ProductionWeights(voProductionData)

	-- Set the default in the array incase no condition is met
	local laArray = {
			0.30, -- Land
			0.30, -- Air
			0.40, -- Sea
			0.00}; -- Other	         

	
	-- Not atwar so
	if not(voProductionData.IsAtWar) and voProductionData.Year < 1942 then
		if voProductionData.Year <= 1939 then
			laArray = {
				0.05, -- Land 
				0.27, -- Air
				0.25, -- Sea
				0.43}; -- Other
		elseif voProductionData.Year <= 1940 then
			laArray = {
				0.15, -- Land 
				0.24, -- Air
				0.25, -- Sea
				0.36}; -- Other
		elseif voProductionData.ManpowerTotal < 50 then
			laArray = {
				0.00, -- Land
				0.58, -- Air
				0.42, -- Sea
				0.00}; -- Other
		end
	else
		local loGerUsaDiplo = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag("GER"))
		local loJapUsaDiplo = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag("JAP"))
		local lbGERWar = loGerUsaDiplo:HasWar() 
		local lbJAPWar = loJapUsaDiplo:HasWar()
	
		if lbGERWar or lbJAPWar then
			local liGERWar = 12
			local liJAPWar = 12
			
			if lbGERWar then
				liGERWar = loGerUsaDiplo:GetWar():GetCurrentRunningTimeInMonths()
			end
				
			if lbJAPWar then
				liJAPWar = loJapUsaDiplo:GetWar():GetCurrentRunningTimeInMonths()
			end
			
			local liWarMonths = math.min(liGERWar, liJAPWar)
			
			if liWarMonths < 12 then
				laArray = {
					0.90, -- Land
					0.05, -- Air
					0.05, -- Sea
					0.00}; -- Other
			end
		end
	end
	
	return laArray
end

-- Land ratio distribution
function P.LandRatio(voProductionData)
	local laArray
	
	if voProductionData.Year < 1941 or not(voProductionData.IsAtWar) then
		laArray = {
			garrison_brigade = 30,
			infantry_brigade = 30,
			motorized_brigade = 3,
			mechanized_brigade = 2,
			armor_brigade = 1
		}
	else
		laArray = {
			garrison_brigade = 2,
			infantry_brigade = 20,
			motorized_brigade = 4,
			mechanized_brigade = 2,
			armor_brigade = 2
		}
	end
	
	return laArray
end

-- Special Forces ratio distribution
function P.SpecialForcesRatio(voProductionData)
	local laRatio = {
		5, -- Land
		1}; -- Special Force Unit

	local laUnits = {
		marine_brigade = 3,
		bergsjaeger_brigade = 0.5,
		paratrooper_brigade = 1};
	
	return laRatio, laUnits	
end

-- Elite Units
function P.EliteUnits(voProductionData)
	local laUnits = {"ranger_brigade"};
	
	return laUnits	
end

-- Which units should get 1 more Support unit with Superior Firepower tech
function P.FirePower(voProductionData)
	local laArray = {
		"ranger_brigade",
		"infantry_brigade",
		"motorized_brigade",
		"mechanized_brigade",
		"armor_brigade"};
		
	return laArray
end

-- Air ratio distribution
function P.AirRatio(voProductionData)
	local laArray = {
		interceptor = 4,
		multi_role = 1,
		rocket_interceptor = 1,
		cas = 1,
		tactical_bomber = 3,
		naval_bomber = 1,
		strategic_bomber = 1.6};
	
	return laArray
end
-- Naval ratio distribution
function P.NavalRatio(voProductionData)
	local laArray = {
		destroyer = 8,
		submarine = 0.75,
		nuclear_submarine = 0.5,
		light_cruiser = 2,
		heavy_cruiser = 1.25,
		battleship = 2,
		escort_carrier = 0.25,
		carrier = 1};
	
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
		150, -- If Percentage extra is less than this it will force it up to the amount entered
		250, -- If Percentage extra is greater than this it will force it down to this
		5} -- Escort to Convoy Ratio (Number indicates how many convoys needed to build 1 escort)
  
	return laArray
end

--function P.Build_armor_brigade(vIC, viManpowerTotal, voType, voProductionData, viUnitQuantity)
--	if voProductionData.TechStatus:IsUnitAvailable(CSubUnitDataBase.GetSubUnit("mechanized_brigade")) then
--		voType.SecondaryMain = "mechanized_brigade"
--	end
--
--	return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
--end

function P.Build_motorized_brigade(vIC, viManpowerTotal, voType, voProductionData, viUnitQuantity)
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

function P.Build_CoastalFort(ic, voProductionData)
	ic = Support.Build_CoastalFort(ic, voProductionData, 10669, 4) --Midway
	ic = Support.Build_CoastalFort(ic, voProductionData, 5825, 4) --Honolulu
	ic = Support.Build_CoastalFort(ic, voProductionData, 5712, 4) -- AmamiOshima 
	ic = Support.Build_CoastalFort(ic, voProductionData, 5720, 4) -- TokunoShima
	ic = Support.Build_CoastalFort(ic, voProductionData, 5748, 4) -- Nago
	ic = Support.Build_CoastalFort(ic, voProductionData, 5759, 4) -- Naha
	ic = Support.Build_CoastalFort(ic, voProductionData, 10642, 4) -- Iwo Jima
	ic = Support.Build_CoastalFort(ic, voProductionData, 14129, 4) -- Bonin Islands
	ic = Support.Build_CoastalFort(ic, voProductionData, 10664, 4) --Wake island

	if voProductionData.Year < 1943 then
		return ic, false
	end
	
	return ic, true
end

function P.Build_AirBase(ic, voProductionData)
	ic = Support.Build_AirBase(ic, voProductionData, 10669, 10) --Midway
	ic = Support.Build_AirBase(ic, voProductionData, 5825, 10) --Honolulu
	ic = Support.Build_AirBase(ic, voProductionData, 5712, 10) -- AmamiOshima 
	ic = Support.Build_AirBase(ic, voProductionData, 5720, 10) -- TokunoShima
	ic = Support.Build_AirBase(ic, voProductionData, 5748, 10) -- Nago
	ic = Support.Build_AirBase(ic, voProductionData, 5759, 10) -- Naha
	ic = Support.Build_AirBase(ic, voProductionData, 10642, 10) -- Iwo Jima
	ic = Support.Build_AirBase(ic, voProductionData, 14129, 10) -- Bonin Islands
	ic = Support.Build_AirBase(ic, voProductionData, 10664, 10) --Wake island

	if voProductionData.Year < 1942 then
		return ic, false
	end
	
	return ic, true
end

function P.Build_NavalBase(ic, voProductionData)
	ic = Support.Build_NavalBase(ic, voProductionData, 10669, 10) --Midway
	ic = Support.Build_NavalBase(ic, voProductionData, 5825, 10) --Honolulu
	ic = Support.Build_NavalBase(ic, voProductionData, 5712, 6) -- AmamiOshima
	ic = Support.Build_NavalBase(ic, voProductionData, 5720, 6) -- TokunoShima
	ic = Support.Build_NavalBase(ic, voProductionData, 5748, 6) -- Nago
	ic = Support.Build_NavalBase(ic, voProductionData, 5759, 6) -- Naha
	ic = Support.Build_NavalBase(ic, voProductionData, 10642, 6) -- Iwo Jima
	ic = Support.Build_NavalBase(ic, voProductionData, 14129, 6) -- Bonin Islands
	ic = Support.Build_NavalBase(ic, voProductionData, 10664, 10) --Wake island

	-- Ports in Spain in case Germany takes them over
	ic = Support.Build_NavalBase(ic, voProductionData, 3884, 10) 
	ic = Support.Build_NavalBase(ic, voProductionData, 3814, 10) 
	ic = Support.Build_NavalBase(ic, voProductionData, 3676, 10) 
	ic = Support.Build_NavalBase(ic, voProductionData, 3877, 10) 
	ic = Support.Build_NavalBase(ic, voProductionData, 3679, 10) 
	ic = Support.Build_NavalBase(ic, voProductionData, 3610, 10) 
	ic = Support.Build_NavalBase(ic, voProductionData, 3675, 10) 
		
	if voProductionData.Year < 1942 then
		return ic, false
	end
	
	return ic, true
end

function P.Build_Radar(ic, voProductionData)
	ic = Support.Build_Radar(ic, voProductionData, 10669, 10) --Midway
	ic = Support.Build_Radar(ic, voProductionData, 5825, 10) --Honolulu
	ic = Support.Build_Radar(ic, voProductionData, 5712, 10) -- AmamiOshima
	ic = Support.Build_Radar(ic, voProductionData, 5720, 10) -- TokunoShima
	ic = Support.Build_Radar(ic, voProductionData, 5748, 10) -- Nago
	ic = Support.Build_Radar(ic, voProductionData, 5759, 10) -- Naha
	ic = Support.Build_Radar(ic, voProductionData, 10642, 10) -- Iwo Jima
	ic = Support.Build_Radar(ic, voProductionData, 14129, 10) -- Bonin Islands
	ic = Support.Build_Radar(ic, voProductionData, 10664, 10) --Wake island

	if voProductionData.Year < 1942 then
		return ic, false
	end
	
	return ic, true
end

function P.Build_AntiAir(ic, voProductionData)
	return ic, false
end

function P.Build_Infrastructure(ic, voProductionData)
	if voProductionData.Year < 1944 then
		return ic, false
	end

	return ic, true
end

function P.Build_Fort(ic, voProductionData)
 	if voProductionData.Year < 1944 then
		return ic, false
	end
	
	return ic, true
end

function P.Build_Industry(vIC, voProductionData)

	if voProductionData.Year < 1941 then

		return vIC, true

	end

	return vIC, false
end

-- END OF PRODUTION OVERIDES
-- #######################################

function P.ForeignMinister_Alignment(...)
	return Support.AlignmentPush("allies", ...)
end

function P.DiploScore_Embargo(voDiploScoreObj)

	if voDiploScoreObj.EmbargoHasFaction then
		local loAllyFaction = CCurrentGameState.GetFaction("allies")

		-- If USA is leaning toward the allies and UK then embargo their enemies
		if Support.IsFriend(voDiploScoreObj.ministerAI, loAllyFaction, voDiploScoreObj.ministerCountry) then
			local allyTag = loAllyFaction:GetFactionLeader()
			local loAllyCountry = allyTag:GetCountry()
			
			if loAllyCountry:GetRelation(voDiploScoreObj.EmbargoTag):HasWar() then
				voDiploScoreObj.Score = 100
			end
			
			-- Push Japan to the top of the que if they are in the Axis
			if tostring(voDiploScoreObj.EmbargoTag) == "JAP" then
				local loAxisFaction = CCurrentGameState.GetFaction("axis")
				local chiTag = CCountryDataBase.GetTag("CHI")
				
				if voDiploScoreObj.EmbargoCountry:GetFaction() == loAxisFaction
				or voDiploScoreObj.EmbargoCountry:GetRelation(chiTag):HasWar() then
					voDiploScoreObj.Score = 100
				end
			end
		end
	end
	
	return voDiploScoreObj.Score
end

function P.DiploScore_Debt(voDiploScoreObj)
	local loAllyFaction = CCurrentGameState.GetFaction("allies")
	
	-- If the requesting country is part of the Allies then
	if voDiploScoreObj.ToCountry:GetFaction() == loAllyFaction then
		-- Make sure the USA is not part of a faction already
		if not(voDiploScoreObj.FromCountry:HasFaction()) then
			if Support.IsFriend(voDiploScoreObj.ministerAI, loAllyFaction, voDiploScoreObj.FromCountry) then
				-- Check to see if they are at war
				if voDiploScoreObj.ToCountry:IsAtWar() then
					-- Calculate the score based on USA neutrality the lower it is the more likely they will allow the debt
					local liNeutrality = voDiploScoreObj.FromCountry:GetEffectiveNeutrality():Get()
					voDiploScoreObj.Score = 110 - liNeutrality
				end
			end
		end
	else
		local lsToTag = tostring(voDiploScoreObj.ToTag)
		
		-- If it is China do a special check
		if lsToTag == "CHI" then
			-- If we are friendly to the Allied faction
			if Support.IsFriend(voDiploScoreObj.ministerAI, loAllyFaction, voDiploScoreObj.FromCountry) then
				local japTag = CCountryDataBase.GetTag("JAP")
				
				-- If China and Japan are at war then let China be allowed debt even if not in the Allies
				if voDiploScoreObj.ToCountry:GetRelation(japTag):HasWar() then
					voDiploScoreObj.Score = 100
				end
			end
		end
	end
	
	return voDiploScoreObj.Score
end

function P.DiploScore_OfferTrade(voDiploScoreObj)
	local laTrade = {
		JAP = {Score = 100},
		ENG = {Score = 50},
		FRA = {Score = 50},
		GER = {Score = -10},
		ITA = {Score = -10},
		SOV = {Score = -10},
		CHI = {Score = 50},
		CHC = {Score = 50},
		CGX = {Score = 50},
		CSX = {Score = 50},
		CXB = {Score = 50},
		CYN = {Score = 50},
		SIK = {Score = 50}}
	
	if laTrade[voDiploScoreObj.TagName] then
		return voDiploScoreObj.Score + laTrade[voDiploScoreObj.TagName].Score
	end
	
	return voDiploScoreObj.Score
end

function P.DiploScore_InviteToFaction(voDiploScoreObj)
	local loAllies = CCurrentGameState.GetFaction("allies")
	
	-- Only go through these checks if we are being asked to join the Allies
	if voDiploScoreObj.Faction == loAllies then
		local liYear = CCurrentGameState.GetCurrentDate():GetYear()
		local liMonth = CCurrentGameState.GetCurrentDate():GetMonthOfYear()
		local chiTag = CCountryDataBase.GetTag("CHI")
		local lochiTagCountry = chiTag:GetCountry()
		local lbChinaExists = lochiTagCountry:Exists() 
		
		-- Date check to make sure they come in within resonable time
		if liYear >= 1943 then
			voDiploScoreObj.Score = voDiploScoreObj.Score + 30
		elseif liYear >= 1942 then
			voDiploScoreObj.Score = voDiploScoreObj.Score + 20
		elseif liYear == 1941 and liMonth >= 10 then
			voDiploScoreObj.Score = voDiploScoreObj.Score + 10
		end
		
		-- China check see if Japan is being aggressive in China
		if lbChinaExists then
			local japTag = CCountryDataBase.GetTag("JAP")
			local loChiJapRelation = lochiTagCountry:GetRelation(japTag)
			
			-- Check to see who they are a puppet of
			if lochiTagCountry:IsPuppet() then
				local lojapTagCountry = japTag:GetCountry()
			
				-- China has been taken over by Japan
				if (loChiJapRelation:HasAlliance())
				or (lochiTagCountry:HasFaction() and lochiTagCountry:GetFaction() == lojapTagCountry:GetFaction()) then
					voDiploScoreObj.Score = voDiploScoreObj.Score + 50
				end
			else
				local lbChiJapHasWar = loChiJapRelation:HasWar()
				
				if lochiTagCountry:IsGovernmentInExile() and lbChiJapHasWar then
					voDiploScoreObj.Score = voDiploScoreObj.Score + 50
				elseif lbChiJapHasWar then
					voDiploScoreObj.Score = voDiploScoreObj.Score + 10
				end
			end
		else
			-- China is out of the war for some reason
			voDiploScoreObj.Score = voDiploScoreObj.Score + 50
		end
	end
	
	return voDiploScoreObj.Score
end

function P.DiploScore_Guarantee(voDiploScoreObj)

	local recipientCountry = voDiploScoreObj.TargetTag:GetCountry()
	if not voDiploScoreObj.HasFaction then
		local continent = tostring( recipientCountry:GetCapitalLocation():GetContinent():GetTag() )
		if (continent == "north_america" or continent == "south_america")
		and not (tostring(voDiploScoreObj.TargetTag) == 'CAN') then
			return 100
		end
	end
	
	return voDiploScoreObj.Score

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
			"POR"} -- Portugal
			
		laIgnore = {
			"HUN", -- Hungary
			"ROM", -- Romania
			"BUL", -- Bulgaria
			"FIN", -- Finland
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
			"VIC"} -- Vichy
			
		-- Make a list of countries that are not in Asia and ignore them
		for loTCountry in CCurrentGameState.GetCountries() do
			if loTCountry:Exists() then
				local lsContinent = tostring(loTCountry:GetActingCapitalLocation():GetContinent():GetTag())
				-- If they are not in Asia then ignore them
				if lsContinent == "north_america" or lsContinent == "south_america" then
					table.insert(laWatch, tostring(loTCountry:GetCountryTag()))
				end
			end
		end				
	end
	
	return laWatch, laIgnoreWatch, laIgnore
end

function P.ForeignMinister_ProposeWar(voForeignMinisterData)
	if not(voForeignMinisterData.Strategy:IsPreparingWar()) then
		if voForeignMinisterData.FactionName == "allies" then
	
			-- Generic DOW for countries not part of the same faction
			if not(voForeignMinisterData.IsAtWar) then
				for loDiploStatus in voForeignMinisterData.ministerCountry:GetDiplomacy() do
					local loTargetTag = loDiploStatus:GetTarget()

					if loTargetTag:IsValid() then
						local loTargetCountry = loTargetTag:GetCountry()
						
						if loDiploStatus:GetThreat():Get() > voForeignMinisterData.ministerCountry:GetMaxNeutralityForWarWith(loTargetTag):Get()  then
							if Support.GoodToWarCheck(loTargetTag, loTargetCountry, voForeignMinisterData, true, true) then
								voForeignMinisterData.Strategy:PrepareWar(loTargetTag, 100 )
							end
						end
					end
				end
			end

			-- Special Checks Start after this point
			local loAxisTag = CCurrentGameState.GetFaction("axis"):GetFactionLeader()
			
			-- If we are atwar with the leader of the Axis then look for Vichy
			if voForeignMinisterData.ministerCountry:GetRelation(loAxisTag):HasWar() then
				local loVICTag = CCountryDataBase.GetTag("VIC")
				local loVichyCountry = loVICTag:GetCountry()
				
				if Support.GoodToWarCheck(loVICTag, loVichyCountry, voForeignMinisterData, true, false) then
					voForeignMinisterData.Strategy:PrepareWar(loVICTag, 100)
				end
			end
		end
	end
end

-- Produce slightly better trained troops
function P.CallLaw_training_laws(minister, voCurrentLaw)
	local _ADVANCED_TRAINING_ = 29
	return CLawDataBase.GetLaw(_ADVANCED_TRAINING_)
end

return AI_USA

