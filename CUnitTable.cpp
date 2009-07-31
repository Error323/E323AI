#include "CUnitTable.h"

CUnitTable::CUnitTable(AIClasses *ai) {
	this->ai = ai;
	/* techlevels */
	categories[TECH1]       = "TECH1";  
	categories[TECH2]       = "TECH2";  
	categories[TECH3]       = "TECH3";  

	/* main categories */
	categories[AIR]         = "AIR";        
	categories[SEA]         = "SEA";        
	categories[LAND]        = "LAND";       
	categories[STATIC]      = "STATIC";     
	categories[MOBILE]      = "MOBILE";     

	/* builders */
	categories[FACTORY]     = "FACTORY";    
	categories[BUILDER]     = "BUILDER";    
	categories[ASSIST]      = "ASSIST";      
	categories[RESURRECTOR] = "RESURRECTOR";

	/* offensives */
	categories[COMMANDER]   = "COMMANDER";  
	categories[ATTACKER]    = "ATTACKER";   
	categories[ANTIAIR]     = "ANTIAIR";    
	categories[SCOUT]       = "SCOUT";  
	categories[ARTILLERY]   = "ARTILLERY";  
	categories[SNIPER]      = "SNIPER";  
	categories[ASSAULT]     = "ASSAULT";  

	/* economic */
	categories[MEXTRACTOR]  = "MEXTRACTOR";  
	categories[MMAKER]      = "MMAKER";      
	categories[EMAKER]      = "EMAKER";      
	categories[MSTORAGE]    = "MSTORAGE";    
	categories[ESTORAGE]    = "ESTORAGE";    
	categories[WIND]        = "WIND";  
	categories[TIDAL]       = "TIDAL";  

	/* ground types */
	categories[KBOT]        = "KBOT";  
	categories[VEHICLE]     = "VEHICLE";  

	sprintf(buf, "%s", CFG_FOLDER);
	ai->call->GetValue(AIVAL_LOCATE_FILE_W, buf);
	sprintf(
		buf, "%s%s-categorization.cfg", 
		std::string(CFG_PATH).c_str(),
		ai->call->GetModName()
	);
	std::ofstream UC(buf, std::ios::app);

	UC << "# Unit Categorization for E323AI\n\n# Categories to choose from:\n";
	std::map<unitCategory,std::string>::iterator i;
	for (i = categories.begin(); i != categories.end(); i++) {
		UC << "# " << i->second << "\n";
		cats.push_back(i->first);
	}
	numUnits = ai->call->GetNumUnitDefs();
	UC << "\n\n# " << numUnits << " units in total\n\n";

	buildTechTree(); /* this is actually a graph in XTA */

	std::map<int, UnitType>::iterator j;
	UnitType *utParent;

	std::map<int, UnitType*>::iterator l;
	std::string buildBy, canBuild;

	for (j = units.begin(); j != units.end(); j++) {
		utParent = &(j->second);
		MoveData* movedata = utParent->def->movedata;
		UC << "# " << utParent->def->humanName << " - " << utParent->def->name << "\n";
		UC << utParent->id;
		for (unsigned i = 0; i < cats.size(); i++)
			if (cats[i] & utParent->cats)
				UC << "," << categories[cats[i]];
		UC << "\n\n";


		if (movedata != NULL)
			moveTypes[movedata->pathType] = movedata;

		LOG("Cost " << utParent->def->name << " = " << utParent->cost << "\n");
		debugCategories(utParent);
		debugUnitDefs(utParent);
		debugWeapons(utParent);
		canBuild = buildBy = "";
		for (l = utParent->buildBy.begin(); l != utParent->buildBy.end(); l++) {
			std::stringstream out;
			out << l->first;
			buildBy += l->second->def->name + "(" + out.str() + "), ";
		}
		buildBy = buildBy.substr(0, buildBy.length()-2);
		for (l = utParent->canBuild.begin(); l != utParent->canBuild.end(); l++) {
			std::stringstream out;
			out << l->first;
			canBuild += l->second->def->name + "(" + out.str() + "), ";
		}
		canBuild = canBuild.substr(0, canBuild.length()-2);
		LOG(utParent->def->name << "\nbuild by : {" << buildBy << "}\ncan build: {" << canBuild << "}\n\n");
		LOG("\n-------------\n");
	}
	UC.close();
}

void CUnitTable::remove(ARegHandler &unit) {
	free.push(lookup[unit.key]);
	lookup.erase(unit.key);
	builders.erase(unit.key);
}

CUnit* CUnitTable::getUnit(int uid) {
	return &ingameUnits[lookup[uid]];
}

CUnit* CUnitTable::requestUnit(int uid, int bid) {
	CUnit *unit = NULL;
	int index   = 0;

	/* Create a new slot */
	if (free.empty()) {
		CUnit u(ai,uid,bid);
		ingameUnits.push_back(u);
		unit  = &ingameUnits.back();
		index = ingameUnits.size()-1;
	}

	/* Use top free slot from stack */
	else {
		index = free.top(); free.pop();
		unit  = &ingameUnits[index];
		unit->reset(uid, bid);
	}

	lookup[uid] = index;
	unit->reg(*this);
	builders[bid] = false;
	return unit;
}

void CUnitTable::buildTechTree() {
	const UnitDef *unitdefs[numUnits];
	ai->call->GetUnitDefList(unitdefs);

	std::map<int, std::string> buildOptions;
	std::map<int, std::string>::iterator j;

	for (int i = 0; i < numUnits; i++) {
		const UnitDef *ud = unitdefs[i];
		if (ud == NULL) continue;
		std::map<int, UnitType>::iterator u = units.find(ud->id);

		UnitType *utParent, *utChild;

		if (u == units.end())
			utParent = insertUnit(ud);
		else
			utParent = &(u->second);

		buildOptions = ud->buildOptions;
		for (j = buildOptions.begin(); j != buildOptions.end(); j++) {
			ud = ai->call->GetUnitDef(j->second.c_str());
			u = units.find(ud->id);

			if (u == units.end())
				utChild = insertUnit(ud);
			else 
				utChild = &(u->second);

			utChild->buildBy[utParent->id]  = utParent;
			utParent->canBuild[utChild->id] = utChild;
		}
	}
	for (int i = 0; i < numUnits; i++) {
		const UnitDef *ud = unitdefs[i];
		if (ud == NULL) continue;
		units[ud->id].cats = categorizeUnit(&units[ud->id]);
	}
}

UnitType* CUnitTable::insertUnit(const UnitDef *ud) {
	UnitType ut;
	ut.def        = ud;
	ut.id         = ud->id;
	ut.cost       = ud->metalCost*METAL2ENERGY + ud->energyCost;
	ut.energyMake = ud->energyMake - ud->energyUpkeep;
	ut.metalMake  = ud->metalMake  - ud->metalUpkeep;
	ut.dps        = calcUnitDps(&ut);
	units[ud->id] = ut;
	return &units[ud->id];
}

bool CUnitTable::hasAntiAir(const std::vector<UnitDef::UnitDefWeapon> &weapons) {
	for (unsigned int i = 0; i < weapons.size(); i++) {
		const UnitDef::UnitDefWeapon *weapon = &weapons[i];
		//FIXME: Still selects Brawlers and Rangers
		if (weapon->def->tracks && !weapon->def->waterweapon && !weapon->def->stockpile)
			return true;
	}
	return false;
}

unsigned int CUnitTable::categorizeUnit(UnitType *ut) {
	unsigned int cats = 0;
	const UnitDef *ud = ut->def;
	
	if (ud->isCommander)
		cats |= COMMANDER;

	if (ud->speed > 0.0f)
		cats |= MOBILE;
	else
		cats |= STATIC;

	if (ud->canfly)
		cats |= AIR;

	if (ud->floater || ud->canSubmerge || ud->canhover || ud->minWaterDepth > 0.0f)
		cats |= SEA;

	if ((ud->canhover || ud->minWaterDepth < 0.0f) && !ud->canfly)
		cats |= LAND;

	if (!ud->buildOptions.empty())
		cats |= BUILDER;

	if (ud->canAssist)
		cats |= ASSIST;

	if (!ud->weapons.empty())
		cats |= ATTACKER;

	if (hasAntiAir(ud->weapons))
		cats |= ANTIAIR;

/*
	if (ud->radarRadius > 0)
		cats |= RADAR;

	if (ud->sonarRadius > 0)
		cats |= SONAR;
	
	if (ud->jammerRadius > 0)
		cats |= JAMMER;
*/

	if (ud->canResurrect)
		cats |= RESURRECTOR;

	if ((!ud->buildOptions.empty() && !ud->canmove) || ud->TEDClassString == "PLANT") {
		cats |= FACTORY;

		std::map<int, std::string>::iterator j;
		std::map<int, std::string> buildOptions = ud->buildOptions;
		for (j = buildOptions.begin(); j != buildOptions.end(); j++) {
			const UnitDef *canbuild = ai->call->GetUnitDef(j->second.c_str());
			if (canbuild->canfly) {
				cats |= AIR;
				cats -= LAND;
				break;
			}
			if (canbuild->movedata == NULL) continue;
			if (canbuild->movedata->moveFamily == MoveData::KBot &&
				ud->minWaterDepth < 0.0f) {
				cats |= KBOT;
				break;
			}
			else if (canbuild->movedata->moveFamily == MoveData::Tank &&
				ud->minWaterDepth < 0.0f) {
				cats |= VEHICLE;
				break;
			}
		}
		//XXX: hack
		if (ud->metalCost < 2000.0f)
			cats |= TECH1;
		else
			cats |= TECH2;
	}

	if (ud->metalStorage / ut->cost > 0.1f)
		cats |= MSTORAGE;

	if (ud->energyStorage / ut->cost > 0.2f)
		cats |= ESTORAGE;

	if (ud->isMetalMaker)
		cats |= MMAKER;

	if ((ud->energyMake - ud->energyUpkeep) / ut->cost > 0.002 ||
		ud->tidalGenerator || ud->windGenerator)
		cats |= EMAKER;

	if (ud->windGenerator)
		cats |= WIND;

	if (ud->tidalGenerator)
		cats |= TIDAL;

	if (ud->extractsMetal)
		cats |= MEXTRACTOR;
	
/*
	if (ud->loadingRadius > 0.0f && ud->transportCapacity > 0)
		cats |= TRANSPORT;
*/

	/* 0 = only low, 1 = only high, 2 both */
	if (ud->highTrajectoryType >= 1)
		cats |= ARTILLERY;

	if (cats&ATTACKER && cats&MOBILE && !(cats&BUILDER) && ud->speed >= 50.0f) {
		std::map<int, UnitType*>::iterator i,j;
		for (i = ut->buildBy.begin(); i != ut->buildBy.end(); i++) {
			bool isCheapest = true;
			UnitType *bb = i->second;
			for (j = bb->canBuild.begin(); j != bb->canBuild.end(); j++) {
				if (ut->cost > j->second->cost && !j->second->def->weapons.empty()) {
					isCheapest = false;
					break;
				}
			}
			if (isCheapest) {
				cats |= SCOUT;
				break;
			}
		}
	}

	return cats;
}

float CUnitTable::calcUnitDps(UnitType *ut) {
	// FIXME: Make our own *briljant* dps calc here
	return ut->def->power;
}

UnitType* CUnitTable::canBuild(UnitType *ut, unsigned int c) {
	std::vector<unitCategory> utcats;
	for (unsigned int i = 0; i < cats.size(); i++)
		if (c&cats[i])
			utcats.push_back(cats[i]);
	std::map<int, UnitType*>::iterator j;
	for (j = ut->canBuild.begin(); j != ut->canBuild.end(); j++) {
		bool qualifies = true;
		unsigned int ccb = j->second->cats;
		for (unsigned int i = 0; i < utcats.size(); i++)
			if (!(utcats[i]&ccb))
				qualifies = false;
		if (qualifies)
			return j->second;
	}
	return NULL;
}

void CUnitTable::debugCategories(UnitType *ut) {
	std::string cats("");
	std::map<unitCategory, std::string>::iterator i;
	for (i = categories.begin(); i != categories.end(); i++) {
		int v = ut->cats & i->first;
		if (v == i->first)
			cats += i->second + " | ";
	}
	cats = cats.substr(0, cats.length()-3);
	LOG(ut->def->name + " categories: ");
	LOG(cats << "\n");
}

void CUnitTable::debugUnitDefs(UnitType *ut) {
	const UnitDef *ud = ut->def;
	sprintf(buf, "metalUpKeep(%0.2f), metalMake(%0.2f), makesMetal(%0.2f), energyUpkeep(%0.2f), energyMake(%0.2f)\n", ud->metalUpkeep, ud->metalMake, ud->makesMetal, ud->energyUpkeep, ud->energyMake);
	LOG(ud->name << " unitdefs: ");
	LOG(buf);
}

void CUnitTable::debugWeapons(UnitType *ut) {
	const UnitDef *ud = ut->def;
	LOG(ud->name << " weapons:\n");
	for (unsigned int i = 0; i < ud->weapons.size(); i++) {
		const UnitDef::UnitDefWeapon *w = &(ud->weapons[i]);
		LOG(w->def->name << ": ");
		sprintf(buf, "Weapon name = %s\n", w->def->type.c_str());
		LOG(buf);
	}
	LOG("\n");
}
