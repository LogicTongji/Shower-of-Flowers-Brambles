local P = {}
AI_CHI = P
function P.TechWeights(voTechnologyData)
	local laTechWeights
	local lbAtWarJAP = voTechnologyData.ministerCountry:GetRelation(CCountryDataBase.GetTag("JAP")):HasWar()
	
	if lbAtWarJAP then
		laTechWeights = {
			0.60, -- landBasedWeight
			0.35, -- landDoctrinesWeight
			0.0, -- airBasedWeight
			0.0, -- airDoctrinesWeight
			0.0, -- navalBasedWeight
			0.0, -- navalDoctrinesWeight
			0.05, -- industrialWeight
			0.0, -- SophisticatedEquipmentTechs
			0.0, -- secretWeaponsWeight
			0.0}; -- otherWeight
	else
		laTechWeights = {
			0.55, -- landBasedWeight
			0.15, -- landDoctrinesWeight
			0.0, -- airBasedWeight
			0.0, -- airDoctrinesWeight
			0.0, -- navalBasedWeight
			0.0, -- navalDoctrinesWeight
			0.30, -- industrialWeight
			0.0, -- SophisticatedEquipmentTechs
			0.0, -- secretWeaponsWeight
			0.0}; -- otherWeight	
	end
	return laTechWeights
end

function P.LandTechs(voTechnologyData)
	local lbAtWarJAP = voTechnologyData.ministerCountry:GetRelation(CCountryDataBase.GetTag("JAP")):HasWar()
	local preferTech
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
	
	if lbAtWarJAP then
		preferTech = {
			"militia_smallarms",
			"militia_support",
			"militia_guns",
			"militia_at"};
	else
		preferTech = {
			"infantry_activation",
			"smallarms_technology",
			"infantry_support",
			"infantry_guns",
			"infantry_at"};
	end
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
		{"mechanized_offensive", 0},
		{"integrated_support_doctrine", 0},
		{"special_forces", 0},
		{"combined_arms_warfare", 0}};
	local preferTech = {
		"infantry_warfare",
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
	local ignoreTech = {"all"};

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
	local ignoreTech = {"all"};
	
	return ignoreTech, nil
end	

function P.OtherTechs(voTechnologyData)
	local ignoreTech = {"all"};

	return ignoreTech, nil
end

function P.ProductionWeights(voProductionData)
	local laArray
	if voProductionData.ManpowerTotal < 30 then
		laArray = {
			0.0, -- Land
			0.50, -- Air
			0.0, -- Sea
			0.50}; -- Other
	else
		if voProductionData.IsAtWar then
			laArray = {
				0.97, -- Land
				0.0, -- Air
				0.0, -- Sea
				0.03}; -- Other
		else
			laArray = {
				0.95, -- Land
				0.0, -- Air
				0.0, -- Sea
				0.05}; -- Other
		end
	end
	
	return laArray
end

function P.LandRatio(voProductionData)
	local lbAtWarJAP = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag("JAP")):HasWar()
	local laArray
	if lbAtWarJAP then
		laArray = {
			infantry_brigade = 1,
			militia_brigade = 6};
	else
		laArray = {
			infantry_brigade = 4,
			militia_brigade = 2};
	end
	if (voForeignMinisterData.ministerCountry:GetFlags():IsFlagSet("Japanese_puppet_Nanking")) and not(lbCXBWar) then
		laArray = {
			infantry_brigade = 10,
			militia_brigade = 2};
		elseif (lbCXBWar) then
		laArray = {
			infantry_brigade = 10,
			militia_brigade = 0};
	end
	return laArray
end

function P.SpecialForcesRatio(voProductionData)
	local laArray = {
		0, -- Land
		0}; -- Special Forces Unit
	
	return laArray, nil
end

function P.AirRatio(voProductionData)
	local laArray = {
		interceptor = 3, 
		cas = 1, 
		tactical_bomber = 2};
	
	return laArray
end

function P.EliteUnits(voProductionData)
	local laUnits = {"chi_star_destroyer"};
	
	return laUnits
end	

function P.FirePower(voProductionData)
	local laArray = {
		"chi_star_destroyer",
		"infantry_brigade"};
		
	return laArray
end

function P.TransportLandRatio(voProductionData)
	local laArray = {
		0, -- Land
		0,  -- transport
		0}  -- invasion craft
  
	return laArray
end

function P.ConvoyRatio(voProductionData)
	local laArray = {
		0, -- Percentage extra (adds to 100 percent so if you put 10 it will make it 110% of needed amount)
		5, -- If Percentage extra is less than this it will force it up to the amount entered
		10, -- If Percentage extra is greater than this it will force it down to this
		0} -- Escort to Convoy Ratio (Number indicates how many convoys needed to build 1 escort)
  
	return laArray
end

function P.Build_CoastalFort(ic, voProductionData)
	return ic, false
end

function P.Build_Fort(ic, voProductionData)
	return ic, false
end

function P.DiploScore_InviteToFaction(voDiploScoreObj)
	local japTag = CCountryDataBase.GetTag("JAP")
	
	-- if we are not at war with JAP, only join if we lost previously and they are busy with allies
	if not (voDiploScoreObj.TargetCountry:GetRelation(japTag):HasWar()) then
		if voDiploScoreObj.Faction == CCurrentGameState.GetFaction("allies") then
			if japTag:GetCountry():GetSurrenderLevel():Get() < 0.06 then -- they must have lost islands
				if (CCurrentGameState.GetProvince(5405):GetController() == japTag) then
					voDiploScoreObj.Score = 0
				end
			end
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

-- Want more troops, let them learn on the battlefield.
--   helps them produce troops faster
function P.CallLaw_training_laws(minister, voCurrentLaw)
	local _MINIMAL_TRAINING_ = 27
	return CLawDataBase.GetLaw(_MINIMAL_TRAINING_)
end
--define China's production of infantry_activation
function P.Build_infantry_brigade(vIC, viManpowerTotal, voType, voProductionData, viUnitQuantity, voForeignMinisterData)
	local lbCXBWar = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag('CXB')):HasWar()
	local lbJAPWar = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag('JAP')):HasWar()
	local chiTag = CCountryDataBase.GetTag('CHI')
	local lbControlWuHan = (CCurrentGameState.GetProvince(7508):GetController() == chiTag)
	if not(lbJAPWar) then	
		-- If China still controls WuHan keep hitting them
	         local laSupportUnit = {nil}
	         voType.Size = 3
	         voType.Support = 0
			 return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
			elseif (lbJAPWar) and not(voForeignMinisterData.ministerCountry:GetFlags():IsFlagSet("jap_seizes_coast_chi")) then	
				local laSupportUnit = {nil}
					voType.Size = 2
					voType.Support = 0
				return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
	        elseif(lbJAPWar) and (voForeignMinisterData.ministerCountry:GetFlags():IsFlagSet("jap_seizes_coast_chi")) then
		        local laSupportUnit = {
			           "field_battalion"}
			       voType.Size = 2
			       voType.Support = 1
			return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
  end
	if (voForeignMinisterData.ministerCountry:GetFlags():IsFlagSet("Japanese_puppet_Nanking")) and not(lbCXBWar) then
				if (voProductionData.ManpowerTotal > 150 and voProductionData.Year <= 1940) and (voProductionData.LandCountTotal < 600)then
					 local laSupportUnit = {
							"field_battalion"}
					 voType.Size = 3
					 voType.Support = 1
					 return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
					elseif (voProductionData.ManpowerTotal > 150 and voProductionData.Year >= 1940) and (voProductionData.LandCountTotal < 800)then
						local laSupportUnit = {
							"alpine_artillery_brigade",
							"field_battalion"}
							voType.Size = 3
							voType.Support = 1
							return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
				 end
			end
	if(lbCXBWar) then
		local laSupportUnit = {
			"field_battalion"}
			voType.Size = 3
			voType.Support = 1
			return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
  end
end
--define China's production of militia_activation
function P.Build_militia_brigade(vIC, viManpowerTotal, voType, voProductionData, viUnitQuantity, voForeignMinisterData)
	-- Early war: standard infantry brigade with artillery and engineer support
	
local lbJAPWar = voProductionData.ministerCountry:GetRelation(CCountryDataBase.GetTag('JAP')):HasWar()
	if not(lbJAPWar) then	
		local laSupportUnit = {nil}
		voType.Size = 3
		voType.Support = 0
		return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
    elseif (lbJAPWar) and not(voForeignMinisterData.ministerCountry:GetFlags():IsFlagSet("jap_seizes_coast_chi")) then	
		local laSupportUnit = {nil}
		voType.Size = 2
		voType.Support = 0
		return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
	elseif (lbJAPWar) and not(voForeignMinisterData.ministerCountry:GetFlags():IsFlagSet("jap_seizes_coast_chi"))  then
		local laSupportUnit = {nil}
		voType.Size = 3
		voType.Support = 0
		return Support.CreateUnit(voType, vIC, viUnitQuantity, voProductionData, laSupportUnit)
	end
end

return AI_CHI