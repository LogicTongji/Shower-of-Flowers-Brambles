local P = {}
AI_SOV = P

-- #######################################
-- Tech weights
--   1.0 = 100% the total needs to equal 1.0
function P.TechWeights(voTechnologyData)
	local laTechWeights
	local liCYear = CCurrentGameState.GetCurrentDate():GetYear()
	local lbAtWarGER = voTechnologyData.ministerCountry:GetRelation(CCountryDataBase.GetTag("GER")):HasWar()
	
	-- If we are at war with Germany or the year is less than 1941 and not wat war
	if lbAtWarGER or (liCYear < 1942 and not(voTechnologyData.IsAtWar)) then
		local lbControlMoscow = (CCurrentGameState.GetProvince(1409):GetController() == voTechnologyData.ministerTag)

		-- We still control Moscow and the year is greater than 1943 spread out research
		if lbControlMoscow and liCYear > 1942 then
			laTechWeights = {
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
		else
			laTechWeights = {
				0.35, -- landBasedWeight
				0.20, -- landDoctrinesWeight
				0.11, -- airBasedWeight
				0.15, -- airDoctrinesWeight
				0.00, -- navalBasedWeight
				0.00, -- navalDoctrinesWeight
				0.15, -- industrialWeight
				0.00, -- SophisticatedEquipmentTechs
				0.00, -- secretWeaponsWeight
				0.04}; -- otherWeight
		end
	else
		laTechWeights = {
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
	end

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
			{"militia_smallarms", 0},
			{"militia_support", 0},
			{"militia_guns", 0},
			{"militia_at", 0},
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
		"heavy_tank_armour",
		"art_barrell_ammo",
		"art_carriage_sights",
		"at_ammo_muzzel",
		"at_barrell_sights",
		"cavalry_smallarms"};
		
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
		"spotting"};		
		
	return ignoreTech, preferTech
end

function P.IndustrialTechs(voTechnologyData)
	local ignoreTech = {
		{"oil_to_coal_conversion", 0},
		{"oil_refinning", 0},
		{"steel_production", 0},
		{"raremetal_refinning_techniques", 0},
		{"coal_processing_technologies", 0}};

	local preferTech = {
		"agriculture",
		"industral_production",
		"industral_efficiency",
		"supply_production",
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
	local laArray
	local gerTag = CCountryDataBase.GetTag("GER")
	local loGerCountry = gerTag:GetCountry()
	local loGerSovDiplo = voProductionData.ministerCountry:GetRelation(gerTag)
	
	-- Check to see if manpower is to low
	-- More than 400 brigades so build stuff that does not use manpower
	if voProductionData.ManpowerTotal < 50 then
		laArray = {
			0.0, -- Land
			0.80, -- Air
			0.10, -- Sea
			0.10}; -- Other
	
	elseif (voProductionData.ManpowerTotal < 300 and voProductionData.LandCountTotal > 400) then
		laArray = {
			0.20, -- Land
			0.60, -- Air
			0.10, -- Sea
			0.10}; -- Other
	elseif loGerSovDiplo:HasWar() then
		local loWar = loGerSovDiplo:GetWar()
		local liWarMonths = loWar:GetCurrentRunningTimeInMonths()
	
		-- War is less than 10 months or we lost Moscow build massive land units
		if liWarMonths < 12 or not(lbControlMoscow) then
			laArray = {
				0.95, -- Land
				0.05, -- Air
				0.0, -- Sea
				0.0}; -- Other
				
		-- War has been going on for atleast 2 years and we still have Moscow
		else
			local lbControlMoscow = (CCurrentGameState.GetProvince(1409):GetController() == voProductionData.ministerTag)

			if lbControlMoscow and liWarMonths > 23 then
				laArray = {
					0.65, -- Land
					0.25, -- Air
					0.05, -- Sea
					0.05}; -- Other
			else
				laArray = {
					0.75, -- Land
					0.25, -- Air
					0.0, -- Sea
					0.0}; -- Other
			end
		end
	-- We are atwar with someone other than Germany
	elseif voProductionData.IsAtWar then
		laArray = {
			0.50, -- Land
			0.25, -- Air
			0.15, -- Sea
			0.10}; -- Other

	-- Produce lots of industry in the early years
	--   as long as Germany is not at war with anyone
	elseif voProductionData.Year <= 1938 and not(loGerCountry:IsAtWar()) then
		laArray = {
			0.50, -- Land
			0.25, -- Air
			0.0, -- Sea
			0.25}; -- Other
	elseif voProductionData.Year <= 1940 then
		laArray = {
			0.50, -- Land  --45
			0.30, -- Air --30
			0.0, -- Sea
			0.20}; -- Other -25
	else
		laArray = {
			0.65, -- Land
			0.26, -- Air
			0.04, -- Sea
			0.05}; -- Other
	end
	
	return laArray
end
-- Land ratio distribution

function P.LandRatio(voProductionData)
	local laArray
	
	if voProductionData.Year < 1942 then
		laArray = {
			garrison_brigade = 2,
			infantry_brigade = 24,
			motorized_brigade = 3,
			armor_brigade = 1};
	else
		laArray = {
			garrison_brigade = 2,
			infantry_brigade = 24,
			motorized_brigade = 4,
			mechanized_brigade = 1,
			armor_brigade = 1,
			heavy_armor_brigade = 0.3};
	end
	
	return laArray
end
-- Special Forces ratio distribution

function P.SpecialForcesRatio(voProductionData)
	local laUnits = nil
	local laRatio = {
		11, -- Land
		1}; -- Special Force Unit


	-- No special forces till 1942 or later
	if voProductionData.Year > 1941 then
		laUnits = {
			bergsjaeger_brigade = 0.3,
			paratrooper_brigade = 0.1};

	end
	
	return laRatio, laUnits
end

-- Elite Units
function P.EliteUnits(voProductionData)
	local laUnits = {"guards_brigade"};
	
	return laUnits	
end

-- Which units should get 1 more Support unit with Superior Firepower tech
function P.FirePower(voProductionData)
	local laArray = {
		"guards_brigade",
		"motorized_brigade",
		"mechanized_brigade",
		"armor_brigade",
		"heavy_armor_brigade"};
		
	return laArray
end

-- Air ratio distribution
function P.AirRatio(voProductionData)
	local laArray = {
		interceptor = 6,
		multi_role = 3,
		rocket_interceptor = 1,
		cas = 5,
		tactical_bomber = 4,
		naval_bomber = 0.5};
	
	return laArray
end

-- Naval ratio distribution
function P.NavalRatio(voProductionData)
	local laArray = {
		destroyer = 6,
		submarine = 4,
		nuclear_submarine = 2,
		heavy_cruiser = 4,
		battleship = 1};
	
	return laArray
end

-- Transport to Land unit distribution
function P.TransportLandRatio(voProductionData)
	local laArray = {
		0, -- Land
		0,  -- Transport
		0}; -- Invasion
	
	if voProductionData.Year > 1942 then
		-- No need to make these checks till later so performance pushed inside the if statement
		local lbAtWarGER = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag("GER")):HasWar()
		
		if not(lbAtWarGER) or voProductionData.IsAtWar then
			laArray = {
				120, -- Land
				1,  -- Transport
				1}; -- Invasion
		end
	end

	return laArray
end

-- Convoy Ratio control
--- NOTE: If goverment is in Exile these parms are ignored
function P.ConvoyRatio(voProductionData)
	local laArray = {
		5, -- Percentage extra (adds to 100 percent so if you put 10 it will make it 110% of needed amount)
		20, -- If Percentage extra is less than this it will force it up to the amount entered
		30, -- If Percentage extra is greater than this it will force it down to this
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

function P.Build_Industry(ic, voProductionData)
	-- Mass Build Industry in the Urals
	if voProductionData.Year <= 1938 and not(voProductionData.IsAtWar) then
		ic = Support.Build_Industry(ic, voProductionData, 8595, 10) -- Kemerovo
		ic = Support.Build_Industry(ic, voProductionData, 1330, 10) -- Kaznan
		ic = Support.Build_Industry(ic, voProductionData, 8191, 10) -- Perm
		ic = Support.Build_Industry(ic, voProductionData, 6632, 10) -- Syktyukar
		ic = Support.Build_Industry(ic, voProductionData, 8151, 10) -- Nizhnaya Tura
		ic = Support.Build_Industry(ic, voProductionData, 6727, 10) -- Krasnojarskij
		ic = Support.Build_Industry(ic, voProductionData, 8393, 10) -- Kurgan
		ic = Support.Build_Industry(ic, voProductionData, 968, 10) -- Kologriv
		ic = Support.Build_Industry(ic, voProductionData, 8529, 10) -- Blinkovo
		ic = Support.Build_Industry(ic, voProductionData, 8762, 10) -- Akshtau
		ic = Support.Build_Industry(ic, voProductionData, 8928, 10) -- Matay
		ic = Support.Build_Industry(ic, voProductionData, 6746, 10) -- Orenburg
		ic = Support.Build_Industry(ic, voProductionData, 6768, 10) -- Orsk
		ic = Support.Build_Industry(ic, voProductionData, 6788, 10) -- Aktobe
		
		return ic, false
	end
	
	return ic, true
end

-- Make SOV Fortify some key positions
function P.Build_Fort(ic, voProductionData)
	-- We cut our mass building of industry off so now look at forts
    if voProductionData.Year > 1938 then 
		ic = Support.Build_Fort(ic, voProductionData, 3309, 2) -- Odessa
		ic = Support.Build_Fort(ic, voProductionData, 782, 5)  -- Leningrad
		ic = Support.Build_Fort(ic, voProductionData, 1409, 5) -- Moskva
		ic = Support.Build_Fort(ic, voProductionData, 3581, 2) -- Sevastopol
		ic = Support.Build_Fort(ic, voProductionData, 2401, 1) -- Kharkov
		ic = Support.Build_Fort(ic, voProductionData, 2857, 2) -- Stalingrad
	end
	
	return ic, false
end

function P.Build_AirBase(ic, voProductionData, vbGoOver)
-- We cut our mass building of industry off so now look at air bases
   if voProductionData.Year > 1938 then
		return ic, true
    else
		return ic, false
    end
end

function P.Build_AntiAir(ic, voProductionData)
	if voProductionData.Year <= 1944 then 
		return ic, false
	end
	
	return ic, true
end

-- Do not build coastal forts
function P.Build_CoastalFort(ic, voProductionData)
	return ic, false
end	

function P.Build_NavalBase(ic, voProductionData)
	return ic, false
end

function P.Build_Infrastructure(ic, voProductionData)
	if voProductionData.Year <= 1942 then 
		return ic, false
	end
	
	return ic, true
end

function P.Build_Radar(ic, voProductionData)
	if voProductionData.Year > 1938 then
		-- Ok to build a few
		ic = Support.Build_Radar(ic, voProductionData, 1409, 10) -- Moskva
		ic = Support.Build_Radar(ic, voProductionData, 782, 10) -- Leningrad
		ic = Support.Build_Radar(ic, voProductionData, 1991, 10) -- Homel
		ic = Support.Build_Radar(ic, voProductionData, 2401, 10) -- Kharkov
		
		return ic, true
	end

	return ic, false
end

-- Do not build Rocket Sites
function P.Build_RocketTest(ic, voProductionData)
	if voProductionData.Year > 1943 then
		local lbControlMoscow = (CCurrentGameState.GetProvince(1409):GetController() == voProductionData.ministerTag)
		if lbControlMoscow then
			return ic, true
		end
	end	
	return ic, false
end
-- END OF PRODUTION OVERIDES
-- #######################################

function P.HandleMobilization(minister)
	local ministerTag = minister:GetCountryTag()
	local ministerCountry = ministerTag:GetCountry()
	local gerTag = CCountryDataBase.GetTag("GER")
	local loGerCountry = gerTag:GetCountry()
	
	-- Make sure Germany is at war and has a border with us
	if loGerCountry:IsAtWar() and ministerCountry:IsNonExileNeighbour(gerTag) and not(ministerCountry:IsAtWar()) then
		local ai = minister:GetOwnerAI()
		local fraTag = CCountryDataBase.GetTag("FRA")
		local year = ai:GetCurrentDate():GetYear()
		
		-- Check to see if France no longer controls Paris
		if not(CCurrentGameState.GetProvince(2613):GetController() == fraTag) then
			local liGermanFrontWarsCount = 0
			local norTag = CCountryDataBase.GetTag("NOR")
			
			for neighborTag in loGerCountry:GetNeighbours() do
				if loGerCountry:GetRelation(neighborTag):HasWar() then
					
					-- Do not count Norway as we are invading them
					if not(norTag == neighborTag) then
						liGermanFrontWarsCount = liGermanFrontWarsCount + 1
					end
				end
			end			
			
			-- 10% random Chance if Germany is in a war and no fronts if the year is 1941 or better then mobilize
			if math.random(100) < 11 and liGermanFrontWarsCount == 0 then
				ai:Post(CToggleMobilizationCommand( ministerTag, true ))
			elseif math.random(100) < 26 and year >= 1941 then
				ai:Post(CToggleMobilizationCommand( ministerTag, true ))
			end
		
		-- Germany is atwar and has a front with us so go ahead and mobilize
		elseif year >= 1942 then
			ai:Post(CToggleMobilizationCommand( ministerTag, true ))
		end
	end
end

function P.DiploScore_Embargo(voDiploScoreObj)
	if tostring(voDiploScoreObj.EmbargoTag) == "JAP" then
		-- We must be in the cominterm
		if voDiploScoreObj.Faction == CCurrentGameState.GetFaction("comintern") then
			-- They must be in the Axis
			if voDiploScoreObj.EmbargoCountry:GetFaction() == CCurrentGameState.GetFaction("axis") then
				-- Do not embargo Japan under any circumstances other than war
				voDiploScoreObj.Score = 0
			end
		end
	end
	
	return voDiploScoreObj.Score
end

function P.DiploScore_NonAgression(viScore, voAI, voCountryOne, voCountryTwo, voObserver)
	-- If we are at war
	if voCountryOne.IsAtWar then
		local loComintern = CCurrentGameState.GetFaction("comintern")
		
		-- If we are part of the cominterm
		if voCountryOne.Faction == loComintern then
			local loAxis = CCurrentGameState.GetFaction("axis")
			
			-- If Japan is in the Axis
			if tostring(voCountryTwo.ministerTag) == "JAP"
			and voCountryTwo.Faction == loAxis then
				local gerTag = CCountryDataBase.GetTag("GER")
				local loGerCountry = gerTag:GetCountry()
				local loSovGerRelation = voCountryOne.ministerCountry:GetRelation(gerTag)
				
				-- If we are atwar with Germany then heavily consider a non aggression pact with Japan
				if loSovGerRelation:HasWar()
				and not(loGerCountry:IsGovernmentInExile())
				and loGerCountry:Exists() then
					viScore = 100
				end
			end
		end
	end
	
	return viScore
end

function P.DiploScore_OfferTrade(voDiploScoreObj)
	local laTrade = {
		GER = {Score = 100},
		ITA = {Score = 75},
		CHC = {Score = 20},
		SPR = {Score = 20},
		SIK = {Score = 20},
		CHI = {Score = 20}}
	
	if laTrade[voDiploScoreObj.TagName] then
		return voDiploScoreObj.Score + laTrade[voDiploScoreObj.TagName].Score
	end
	
	return voDiploScoreObj.Score
end


--##########################
-- Foreign Minister Hooks
function P.ForeignMinister_ProposeWar(voForeignMinisterData)
	-- If we are pat of the Comintern then process this
	if not(voForeignMinisterData.Strategy:IsPreparingWar()) then
		if voForeignMinisterData.FactionName == "comintern" then
			-- We will not consider DOWing Germany if we are in a war already
			if not(voForeignMinisterData.IsAtWar) then
				-- Make sure we control Moscow
				if CCurrentGameState.GetProvince(1409):GetController() == voForeignMinisterData.ministerTag then
					local loAxisFaction = CCurrentGameState.GetFaction("axis")
					local loAxisTag = loAxisFaction:GetFactionLeader()
					local loAxisCountry = loAxisTag:GetCountry()
				
					-- Make sure we never surrendered in the past
					if not(loAxisCountry:GetFlags():IsFlagSet("su_signs_peace")) then
						local loAlliesTag = CCurrentGameState.GetFaction("allies"):GetFactionLeader()
						local loAxisAlliesRelation = loAxisCountry:GetRelation(loAlliesTag)
						local lbSealion = P.SealionCheck(loAxisAlliesRelation, loAxisFaction)
						
						-- Can we DOW the Axis Leader
						if lbSealion then
							if math.random(100) < 40 then
								voForeignMinisterData.Strategy:PrepareLimitedWar(loAxisTag, 100)
							end							
						else
							local lbDOW = Support.GoodToWarCheck(loAxisTag, loAxisCountry, voForeignMinisterData, false, true, true)
							
							if lbDOW then
								if voForeignMinisterData.Year >= 1942 then
									if math.random(100) < 10 then
										voForeignMinisterData.Strategy:PrepareLimitedWar(loAxisTag, 100)
									end						
								end
							elseif voForeignMinisterData.Year >= 1942 then
								-- Poland Check if we can go through them
								local loPOLTag = CCountryDataBase.GetTag("POL")
								local loPolandCountry = loPOLTag:GetCountry()
								lbDOW = Support.GoodToWarCheck(loPOLTag, loPolandCountry, voForeignMinisterData, false, true, true)
								
								if lbDOW then
									if math.random(100) < 30 then
										voForeignMinisterData.Strategy:PrepareLimitedWar(loPOLTag, 100)
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

function P.SealionCheck(voAxisAlliesRelation, voAxisFaction)
	-- Check for Sea Lion and if so lets get involved before its to late
	local laProvinceCheck = {
		1964, -- london
		2250, -- plymouth
		2135, -- bornmouth
		2021, -- dover
		1790, -- lowestoft
		1616, -- grimsby
		1524, -- hull
		1255, --newcastle
		1128, -- edinburgh
		894, -- aberdeen
		604, -- scapa flow
		2018} -- bristol
	
	if voAxisAlliesRelation:HasWar() then	
		for i = 1, table.getn(laProvinceCheck) do
			loProvinceFaction = CCurrentGameState.GetProvince(laProvinceCheck[i]):GetController():GetCountry():GetFaction()
			
			-- Is the province controlled by the Axis
			if loProvinceFaction == voAxisFaction then
				return true
			end
		end
	end
	
	return false
end

function P.ForeignMinister_EvaluateDecision(voDecision, voForeignMinisterData)
	if voDecision.Name == "ultimatum_to_the_baltic_states" then
		local loFRATag = CCountryDataBase.GetTag("FRA")
		local loBELTag = CCountryDataBase.GetTag("BEL")
		
		if voForeignMinisterData.Year >= 1941
		or not(CCurrentGameState.GetProvince(2613):GetController() == loFRATag) -- Paris
		or not(CCurrentGameState.GetProvince(2311):GetController() == loBELTag) then -- Brussels
			return 100
		else
			return 0
		end
	end
	
	return voDecision.Score
end

function P.ForeignMinister_Influence(voForeignMinisterData)
	local laIgnoreWatch -- Ignore this country but monitor them if they are about to join someone else
	local laWatch -- Monitor them and also find their score is high enough they can be influenced normally
	local laIgnore -- Ignore them completely

	if voForeignMinisterData.FactionName == "comintern" then
		laIgnore = {
			"AUS", -- Austria
			"CZE", -- Czechoslovakia
			"SCH", -- Switzerland
			"LAT", -- Lativia
			"LIT", -- Lithuania
			"EST", -- Estonia
			"LUX", -- Luxemburg
			"VIC", -- Vichy
			"DEN", -- Denmark
			"ITA", -- Italy
			"JAP"} -- Japan
	end
	
	return laWatch, laIgnoreWatch, laIgnore
end


-- Soviets want more troops, let them learn on the battlefield.
--   helps them produce troops faster
function P.CallLaw_training_laws(minister, voCurrentLaw)
	local ministerTag = minister:GetCountryTag()
	local ministerCountry = ministerTag:GetCountry()
	local lbAtWarGER = ministerCountry:GetRelation(CCountryDataBase.GetTag("GER")):HasWar()
	
	-- If atwar with Germany check for special conditions on training
	if lbAtWarGER then
		local liCYear = CCurrentGameState.GetCurrentDate():GetYear()
		local lbControlMoscow = (CCurrentGameState.GetProvince(1409):GetController() == ministerCountry:GetCountryTag() )
		
		-- If its 1943 and we still control Moscow make better trained troops
		if liCYear >= 1942 and lbControlMoscow then
			return CLawDataBase.GetLaw(28) -- _BASIC_TRAINING_
		else
			return CLawDataBase.GetLaw(27) -- _MINIMAL_TRAINING_
		end
	else
		return CLawDataBase.GetLaw(28) -- _BASIC_TRAINING_
	end

end

return AI_SOV
