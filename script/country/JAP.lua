local P = {}
AI_JAP = P

-- #######################################
-- Start of Trade Weights
function P.TradeWeights(voResourceData)
	local laResouces = {
		METAL = {
			Buffer = 2,
			BufferSaleCap = 5000},
		ENERGY = {
			BufferSaleCap = 5000},
		RARE_MATERIALS = {
			Buffer = 2,
			BufferSaleCap = 5000},
		CRUDE_OIL = {
			Buffer = 2,
			BufferSaleCap = 10000},
		FUEL = {
			Buffer = 2,
			BufferSaleCap = 10000}}
	
	return laResouces
end

-- #######################################
-- Static Production Variables overide
function P.LandToAirRatio(voProductionData)
	local laArray = {
		5, -- Land Brigades
		1}; -- Air
	
	return laArray
end

-- Tech weights
--   1.0 = 100% the total needs to equal 1.0
function P.TechWeights(voTechnologyData)
	local laTechWeights = {
		0.20, -- landBasedWeight
		0.08, -- landDoctrinesWeight
		0.12, -- airBasedWeight
		0.15, -- airDoctrinesWeight
		0.18, -- navalBasedWeight
		0.10, -- navalDoctrinesWeight
		0.09, -- industrialWeight
		0.0, -- SophisticatedEquipmentTechs
		0.04, -- secretWeaponsWeight
		0.04}; -- otherWeight
	
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
		{"paratrooper_infantry", 0},
		{"desert_warfare_equipment", 0},
		{"artic_warfare_equipment", 0},
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
		{"super_heavy_tank_brigade", 0},
		{"super_heavy_tank_gun", 0},
		{"super_heavy_tank_engine", 0},
		{"super_heavy_tank_armour", 0},
		{"super_heavy_tank_reliability", 0},
		{"rocket_art", 0},
		{"rocket_art_ammo", 0},
		{"rocket_carriage_sights", 0}};
		
	local preferTech = {
		"militia_smallarms",
		"militia_support",
		"militia_guns",
		"militia_at",
		"infantry_activation",
		"smallarms_technology",
		"infantry_support",
		"infantry_guns",
		"infantry_at",
		"marine_infantry",
		"art_barrell_ammo",
		"art_carriage_sights",
		"at_ammo_muzzel",
		"at_barrell_sights"};
		
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
		{"superior_firepower", 0},
		{"mechanized_offensive", 0},
		{"combined_arms_warfare", 0},
		{"large_formations", 0}};
		
	local preferTech = {
		"infantry_warfare",
		"central_planning",
		"mass_assault",
		"peoples_army",
		"guerilla_warfare"};	
		
	return ignoreTech, preferTech
end

function P.AirTechs(voTechnologyData)
	local ignoreTech = {
		{"basic_four_engine_airframe", 0},
		{"basic_strategic_bomber", 0},
		{"large_fueltank", 0},
		{"four_engine_airframe", 0},
		{"strategic_bomber_armament", 0},
		{"cargo_hold", 0},
		{"large_bomb", 0},
		{"large_airsearch_radar", 0},
		{"large_navagation_radar", 0},
		{"drop_tanks", 0}};

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
		{"fighter_targerting_focus", 0}, 
		{"heavy_bomber_pilot_training", 0},
		{"heavy_bomber_groundcrew_training", 0},
		{"strategic_bombardment_tactics", 0},
		{"airborne_assault_tactics", 0},
		{"strategic_air_command", 0}};

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
		{"battlecruiser_armour", 0}};

	local preferTech = {
		"amphibious_invasion_craft",
		"advanced_invasion_craft",
		"destroyer_technology",
		"destroyer_armament",
		"destroyer_antiaircraft",
		"destroyer_engine",
		"destroyer_armour",
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
		"largewarship_radar",
		"smallwarship_radar",
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
	local ignoreTech = {
		{"heavy_aa_guns", 0},
		{"rocket_tests", 0},
		{"rocket_engine", 0},
		{"atomic_research", 0},
		{"nuclear_research", 0},
		{"isotope_seperation", 0},
		{"civil_nuclear_research", 0}};

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
	local ignoreTech = {"all"}
	
	return ignoreTech, nil
end
		
function P.SecretWeaponTechs(voTechnologyData)
	local ignoreTech = {}
	
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
	local laArray = {
		0.25, -- Land
		0.25, -- Air
		0.45, -- Sea
		0.05}; -- Other
	
	-- Check to see if manpower is to low
	-- More than 150 brigades so build stuff that does not use manpower
	if voProductionData.ManpowerTotal  < 100 then
		laArray = {
			0.0, -- Land
			0.45, -- Air
			0.50, -- Sea
			0.05}; -- Other	
	else
		local lbUSAWar = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag('USA')):HasWar()
		local lbCHIWar = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag('CHI')):HasWar()
		local lbCXBWar = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag('CXB')):HasWar()
		-- If Japan not at war with the USA
		if not(lbUSAWar) then
		-- If China still controls Shanghai keep hitting them
	         if lbCHIWar and not (voProductionData.ministerCountry:GetFlags():IsFlagSet("jap_seizes_coast"))  then
				laArray = {
					0.70, -- Land
					0.10, -- Air
					0.15, -- Sea
					0.05}; -- Other
				elseif lbCHIWar and (voProductionData.ministerCountry:GetFlags():IsFlagSet("jap_seizes_coast")) then
				laArray = {
					0.25, -- Land
					0.30, -- Air
					0.40, -- Sea
					0.05}; -- Other
			end
		end
		if  ((voProductionData.ministerCountry:GetFlags():IsFlagSet("conque_Eastern_Asian")) and not(lbCXBWar))  then
			laArray = {
				0.35, -- Land
				0.15, -- Air
				0.45, -- Sea
				0.05}; -- Other
			elseif lbCXBWar then
				laArray = {
					0.50, -- Land
					0.35, -- Air
					0.10, -- Sea
					0.05}; -- Other
		end
	end
	
	return laArray
end
--define japneseese production infantry
function P.Build_infantry_brigade(vIC, viManpowerTotal, voType, voProductionData, viUnitQuantity)
	local lbUSAWar = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag('USA')):HasWar()
	local lbCXBWar = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag('CXB')):HasWar()
	if (not lbUSAWar and not (voProductionData.ministerCountry:GetFlags():IsFlagSet("jap_seizes_coast"))) and (voProductionData.LandCountTotal < 400) then	
		-- If China still controls WuHan keep hitting them
	         local laSupportUnit = {
		     "artillery_brigade",
		     "engineer_brigade"}
	         voType.Size = 4
	         voType.Support = 1
			 return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
			elseif (not lbUSAWar and voProductionData.ministerCountry:GetFlags():IsFlagSet("jap_seizes_coast")) and (voProductionData.LandCountTotal < 400) then	
				local laSupportUnit = {
					"alpine_artillery_brigade",
					"engineer_brigade"}
					voType.Size = 3
					voType.Support = 1
				return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
	end
	if(lbUSAWar) then
		local laSupportUnit = {
			"field_battalion"}
			voType.Size = 3
			voType.Support = 1
			return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
  end
	if (voProductionData.ministerCountry:GetFlags():IsFlagSet("conque_Eastern_Asian")) and not(lbCXBWar) then
				if (voProductionData.ManpowerTotal > 150 and voProductionData.Year <= 1940) and (voProductionData.LandCountTotal < 200) then
					 local laSupportUnit = {
					 "artillery_brigade",
					 "engineer_brigade"}
					 voType.Size = 4
					 voType.Support = 1
					 return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
					elseif (voProductionData.ManpowerTotal > 150 and voProductionData.Year >= 1940) then
						local laSupportUnit = {
							"alpine_artillery_brigade",
							"field_battalion"}
							voType.Size = 4
							voType.Support = 1
							return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
				 end
			end
	if(lbCXBWar) and (voProductionData.LandCountTotal < 800) then
		local laSupportUnit = {
			"artillery_brigadeB",
			"field_battalion"}
			voType.Size = 3
			voType.Support = 1
			return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
  end
  local laSupportUnit = {
	"artillery_brigade"}
	voType.Size = 3
	voType.Support = 1
	return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
end

function P.Build_bergsjaeger_brigade(vIC, viManpowerTotal, voType, voProductionData, viUnitQuantity)
	local lbCXBWar = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag('CXB')):HasWar()
	if ((voProductionData.ministerCountry:GetFlags():IsFlagSet("conque_Eastern_Asian")) and not(lbCXBWar)) and (voProductionData.LandCountTotal < 800)   then
		local laSupportUnit = {
			"alpine_artillery_brigade",
			"field_battalion"}
			voType.Size = 4
			voType.Support = 1
			return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
		elseif (lbCXBWar)  then
			local laSupportUnit = {
				"alpine_artillery_brigade"}
				voType.Size = 2
				voType.Support = 1
				return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
	end
	local laSupportUnit = {
		"alpine_artillery_brigade"}
		voType.Size = 3
		voType.Support = 1
		return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
end
-- Land ratio distribution
function P.LandRatio(voProductionData)
	local laArray
	if voProductionData.Year <= 1938 then
		laArray = {
			garrison_brigade = 1,
			infantry_brigade = 15};
        else
		laArray = {
			garrison_brigade = 3,
			infantry_brigade = 15};
	end
	
	return laArray
end

-- Special Forces ratio distribution
function P.SpecialForcesRatio(voProductionData)
	local laUnits = nil
	local lbMarine = voProductionData.TechStatus:IsUnitAvailable(CSubUnitDataBase.GetSubUnit("marine_brigade"))
	local laRatio = {
		3, -- Land
		1}; -- Special Force Unit
	
	if lbMarine then
		laUnits = {
			bergsjaeger_brigade = 1,
			marine_brigade = 6};
	end

	-- Returning a nul for laUnits means no Special forces will be built
	return laRatio, laUnits
end

-- Elite Units
function P.EliteUnits(voProductionData)
	local laUnits = {"imperial_brigade"};
	
	return laUnits	
end

-- Which units should get 1 more Support unit with Superior Firepower tech
--- Firepower is on ignore list but just in case human player researches it
function P.FirePower(voProductionData)
	local laArray = {
		"imperial_brigade",
		"infantry_brigade"};
		
	return laArray
end

-- Air ratio distribution
function P.AirRatio(voProductionData)
       local laArray
       if not(voProductionData.ministerCountry:GetFlags():IsFlagSet("jap_seizes_coast")) then
	laArray = {
		interceptor = 2,
		multi_role = 0.5,
		cas = 1,
		tactical_bomber = 8, -- Bomb the Chinese!
		naval_bomber = 1};
       else laArray = {
		interceptor = 2,
		multi_role = 1,
		cas = 0.5,
		tactical_bomber = 1,
		naval_bomber = 5};   -- Rule the Pacific!
        end
	return laArray
end

-- Naval ratio distribution
function P.NavalRatio(voProductionData)
	local laArray = {
		torpedo_boat = 2,
		destroyer = 6,
		submarine = 2,
		nuclear_submarine = 2,
		light_cruiser = 2,
		heavy_cruiser = 2,
		battleship = 1,
		escort_carrier = 0.5,
		seaplane_carrier = 2,
		carrier = 1.50};
	
	return laArray
end

-- Transport to Land unit distribution
function P.TransportLandRatio(voProductionData)
	local laArray = {
		20, -- Land
		5,  -- transport
		5}  -- invasion craft
  
	return laArray
end

-- Convoy Ratio control
--- NOTE: If goverment is in Exile these parms are ignored
function P.ConvoyRatio(voProductionData)
	local laArray = {
		0, -- Percentage extra (adds to 100 percent so if you put 10 it will make it 110% of needed amount)
		10, -- If Percentage extra is less than this it will force it up to the amount entered
		40, -- If Percentage extra is greater than this it will force it down to this
		10} -- Escort to Convoy Ratio (Number indicates how many convoys needed to build 1 escort)
  
	return laArray
end

function P.Build_Industry(vIC, voProductionData)
	if voProductionData.Year < 1942 then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_CoastalFort(vIC, voProductionData)
	if voProductionData.Year < 1942 then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_AirBase(vIC, voProductionData)
	if voProductionData.Year < 1942 then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_NavalBase(vIC, voProductionData)
	if voProductionData.Year < 1942 then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_Radar(vIC, voProductionData)
	if voProductionData.Year < 1942 then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_AntiAir(vIC, voProductionData)
	if voProductionData.Year < 1942 then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_Infrastructure(vIC, voProductionData)
	if voProductionData.Year < 1942 then
		return vIC, false
	end

	return vIC, true	
end

function P.Build_Fort(vIC, voProductionData)
	if voProductionData.Year < 1942 then
		return vIC, false
	end

	return vIC, true	
end
		
-- END OF PRODUTION OVERIDES
-- #######################################

function P.DiploScore_InfluenceNation(voDiploScoreObj)
	-- Only do this if we are in the allies
	if voDiploScoreObj.FactionName == "axis" then
		local loInfluences = {
			SIA = {Score = 100}}	
	
		-- Are they on our list
		if loInfluences[voDiploScoreObj.TargetName] then
			return (voDiploScoreObj.Score + loInfluences[voDiploScoreObj.TargetName].Score)
		end
	end

	return voDiploScoreObj.Score	
end

function P.DiploScore_Embargo(voDiploScoreObj)
	if tostring(voDiploScoreObj.EmbargoTag) == "SOV" then
		-- We must be in the Axis
		if voDiploScoreObj.Faction == CCurrentGameState.GetFaction("axis") then
			-- They must be in the comintern
			if voDiploScoreObj.EmbargoCountry:GetFaction() == CCurrentGameState.GetFaction("comintern") then
				-- Do not embargo Russia under any circumstances other than war
				voDiploScoreObj.Score = 0
			end
		end
	end
	
	return voDiploScoreObj.Score
end

function P.DiploScore_OfferTrade(voDiploScoreObj)
	local laTrade = {
		SIA = {Score = 100},
		USA = {Score = 150},
		ENG = {Score = 150},
		FRA = {Score = 50},
		HOL = {Score = 150}}
	
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

function P.DiploScore_NonAgression(viScore, voAI, voCountryOne, voCountryTwo, voObserverTag)
	-- If we are at war
	if voCountryOne.IsAtWar then
		local loAxis = CCurrentGameState.GetFaction("axis")
		
		-- If we are part of the Axis
		if voCountryOne.Faction == loAxis then
			local loComintern = CCurrentGameState.GetFaction("comintern")
			
			-- If Soviets are in the Comintern
			if tostring(voCountryTwo.ministerTag) == "SOV"
			and voCountryTwo.Faction == loComintern then
				local chiTag = CCountryDataBase.GetTag("CHI")
				local loChiCountry = chiTag:GetCountry()
				local loChiJapRelation = voCountryOne.ministerCountry:GetRelation(chiTag)
				
				-- If we are at war with China sign a pact with Russia
				if loChiJapRelation:HasWar()
				and not(loChiCountry:IsGovernmentInExile())
				and loChiCountry:Exists() then
					viScore = 100
				else
					-- Go through our current wars if we are atwar with another major power stay away from the Soviets
					for loEnemyTag in voCountryOne.ministerCountry:GetCurrentAtWarWith() do
						local loEnemyCountry = loEnemyTag:GetCountry()
						
						-- We are atwar with another major power so yes we want a NAP
						if loEnemyCountry:IsMajor() then
							viScore = 100
							break
						end
					end
				end
			end
		end
	end
	
	return viScore
end

--##########################
-- Foreign Minister Hooks
function P.ForeignMinister_EvaluateDecision(voDecision, voForeignMinisterData)
	if voDecision.Name == "marco_polo_bridge_incident" then
		local loCSXTag = CCountryDataBase.GetTag("CSX") -- Shanxi
		local loCHITag = CCountryDataBase.GetTag("CHI") -- China

		voForeignMinisterData.Strategy:PrepareWarDecision(loCSXTag, 100, voDecision.Decision, false)
		voForeignMinisterData.Strategy:PrepareWarDecision(loCHITag, 100, voDecision.Decision, false)
				
		return 0
	elseif voDecision.Name == "independent_mengkukuo" then
		if not (voForeignMinisterData.ministerCountry:GetFlags():IsFlagSet("jap_seizes_coast")) then
			return 0
		end
	end
	
	return voDecision.Score
end

function P.ForeignMinister_ProposeWar(voForeignMinisterData)
	local lsIdeology = voForeignMinisterData.ministerCountry:GetRulingIdeology():GetGroup():GetKey()
    local lbnoPreparingWartoUSA = voForeignMinisterData.ministerCountry:GetFlags():IsFlagSet("conque_Eastern_Asian")
	-- Japan just make sure their Ideology is leaning toward Fascist as they may not be part of the Axis
	if (lsIdeology == "fascist" or voForeignMinisterData.FactionName == "axis") then
		local liMajorWars = 0
		local laMajorWars = {}
	
		-- Which Major powers are we at war with
		for loTargetCountry in CCurrentGameState.GetCountries() do
			local loTargetTag = loTargetCountry:GetCountryTag()
			
			-- If we are already at war with another major power save the information
			if loTargetCountry:IsMajor() then
				local loRelation = voForeignMinisterData.ministerCountry:GetRelation(loTargetTag)
				
				if loRelation:HasWar() then
					liMajorWars = liMajorWars + 1
					laMajorWars[tostring(loTargetTag)] = {
						Tag = loTargetTag,
						Country = loTargetCountry,
						Relation = loRelation}
				end
			end
		end
		local loAxisTag = CCurrentGameState.GetFaction("axis"):GetFactionLeader()
		local loCominternTag = CCurrentGameState.GetFaction("comintern"):GetFactionLeader()
		local lbPreparingWar = false -- Make sure that we do not check other war conditions
		local loUSATag = CCountryDataBase.GetTag("USA")
		local loUSACountry = loUSATag:GetCountry()

		-- If we are not at war with any Major powers consider this
		if liMajorWars == 0 then
			local loCominternCountry = loCominternTag:GetCountry()
				
			-- Make sure Axis and Cominterm are at war
			if loCominternCountry:GetRelation(loAxisTag):HasWar() then
				
				-- Now check if we actually can DOW the Soviets
				local lbDOW = Support.GoodToWarCheck(loCominternTag, loCominternCountry, voForeignMinisterData, false, true, true)
			
				if lbDOW then
					-- Are the communist in trouble and if so lets DOW
					if loCominternCountry:CalcDesperation():Get() > 0.4 then
						voForeignMinisterData.Strategy:PrepareLimitedWar(loCominternTag, 100)
						lbPreparingWar = true
					end
				end
			end
			
			-- Check to see if USA is in the war early
			if not(lbPreparingWar) and not(lbnoPreparingWartoUSA) then
				-- Check to see if the USA is at war with the axis
				if loUSACountry:GetRelation(loAxisTag):HasWar() then
					-- They are at war with the Axis so come help them
					if Support.GoodToWarCheck(loUSATag, loUSACountry, voForeignMinisterData, true, true, true) then
						voForeignMinisterData.Strategy:PrepareLimitedWar(loUSATag, 100)
					end
				end
			end
		end
		
		-- We are not at war with Axis or Comintern
		local vSurrenderLevel = voForeignMinisterData.ministerCountry:GetSurrenderLevel():Get()

		if not(lbPreparingWar) and (vSurrenderLevel < 0.15) then
			if not(laMajorWars[tostring(loCominternTag)]) and not(laMajorWars[tostring(loAxisTag)]) then
				if ((voForeignMinisterData.Year >= 1942) and (voForeignMinisterData.Month >= 10)) or (voForeignMinisterData.Year >= 1943) then
					for k, v in pairs(P.WarTargetList(voForeignMinisterData)) do
						-- DOW Everyone we can
						if Support.GoodToWarCheck(v.Tag, v.Country, voForeignMinisterData, true, true, true) then
							voForeignMinisterData.Strategy:PrepareLimitedWar(v.Tag, 100)
						end
					end		
				
				-- Early War if we are Embargo lets do it!
				elseif ((voForeignMinisterData.Year >= 1941) and (voForeignMinisterData.Month >= 10)) or (voForeignMinisterData.Year >= 1942) then

					-- Do any of them have me on Embargo
					for k, v in pairs(P.WarTargetList(voForeignMinisterData)) do
						local loTargetRelation = v.Country:GetRelation(voForeignMinisterData.ministerTag)
						
						-- DOW the ones who have us on Embargo
						if loTargetRelation:HasEmbargo() and not(loTargetRelation:HasWar()) then
							if Support.GoodToWarCheck(v.Tag, v.Country, voForeignMinisterData, true, true, true) then
								voForeignMinisterData.Strategy:PrepareLimitedWar(v.Tag, 100)
							end
						end
					end

					if ( loUSACountry:GetRelation(voForeignMinisterData.ministerTag):HasWar() and loUSACountry:HasFaction() ) then -- if attacking US, lets go for UK as well. even if no embargo
						local loENGTag = CCountryDataBase.GetTag("ENG")
						local loENGCountry = loENGTag:GetCountry()

						if (not( loENGCountry:GetRelation(voForeignMinisterData.ministerTag):HasWar())) and 
							(Support.GoodToWarCheck(loENGTag, loENGCountry, voForeignMinisterData, true, true, true) )
						then
							voForeignMinisterData.Strategy:PrepareLimitedWar(loENGTag, 100)
						end
					end
				end
			end
		end
	end
end

function P.WarTargetList(voForeignMinisterData)
	local laTargetProvinces = {
		CCurrentGameState.GetProvince(5825):GetController(), -- Honolulu
		CCurrentGameState.GetProvince(6394):GetController(), -- Singapore
		CCurrentGameState.GetProvince(6236):GetController(), -- Saigon
		CCurrentGameState.GetProvince(6142):GetController(), -- Manila
		CCurrentGameState.GetProvince(6507):GetController(), -- Batavia
		CCurrentGameState.GetProvince(5868):GetController(), -- Hong Kong
		CCurrentGameState.GetProvince(6467):GetController(), -- Raboul
		CCurrentGameState.GetProvince(7417):GetController() -- Yan'an
		  
	} 

	-- if china is gone only because of bad border
	local loChiCountry = CCountryDataBase.GetTag("CHI"):GetCountry()
	if loChiCountry:IsGovernmentInExile() or not (loChiCountry:Exists()) then
		table.insert(laTargetProvinces, CCurrentGameState.GetProvince(5862):GetController() ) -- nanning
	end
	
	local laTargetTags = {}
	
	for i = 1, table.getn(laTargetProvinces) do
		-- Make sure no duplicate country stored in array
		if voForeignMinisterData.ministerTag ~= laTargetProvinces[i] then
			if not(laTargetTags[tostring(laTargetProvinces[i])]) then
				laTargetTags[tostring(laTargetProvinces[i])] = {
					Tag = laTargetProvinces[i],
					Country = laTargetProvinces[i]:GetCountry()}
			end
		end
	end
	
	return laTargetTags
end

function P.ForeignMinister_Influence(voForeignMinisterData)
	local laIgnoreWatch -- Ignore this country but monitor them if they are about to join someone else
	local laWatch -- Monitor them and also find their score is high enough they can be influenced normally
	local laIgnore -- Ignore them completely

	if voForeignMinisterData.FactionName == "axis" then
		laWatch = {"SIA"} -- Siam
		laIgnore = {}
		
		-- Make a list of countries that are not in Asia and ignore them
		for loTCountry in CCurrentGameState.GetCountries() do
			if loTCountry:Exists() then
				-- If they are not in Asia then ignore them
				if tostring(loTCountry:GetActingCapitalLocation():GetContinent():GetTag()) ~= "asia" then
					table.insert(laIgnore, tostring(loTCountry:GetCountryTag()))
				end
			end
		end	
	end
	
	return laWatch, laIgnoreWatch, laIgnore
end




return AI_JAP
