#include "CUnitTable.h"

#include <iostream>
#include <fstream>
#include <string>

#include "CAI.h"
#include "CUnit.h"
#include "CConfigParser.h"
#include "Util.hpp"
#include "ReusableObjectFactory.hpp"

std::map<std::string, unitCategory> CUnitTable::str2cat;
std::map<unitCategory, std::string> CUnitTable::cat2str;
std::vector<unitCategory> CUnitTable::cats;

CUnitTable::CUnitTable(AIClasses *ai): ARegistrar(100) {
	this->ai = ai;
	
	if (cat2str.empty()) {
		/* techlevels */
		cat2str[TECH1]       = "TECH1";
		cat2str[TECH2]       = "TECH2";
		cat2str[TECH3]       = "TECH3";
		cat2str[TECH4]       = "TECH4";
		cat2str[TECH5]       = "TECH5";

		/* main categories */
		cat2str[AIR]         = "AIR";
		cat2str[SEA]         = "SEA";
		cat2str[LAND]        = "LAND";
				
		cat2str[STATIC]      = "STATIC";
		cat2str[MOBILE]      = "MOBILE";

		/* builders */
		cat2str[FACTORY]     = "FACTORY";
		cat2str[BUILDER]     = "BUILDER";
		cat2str[ASSISTER]    = "ASSISTER";
		cat2str[RESURRECTOR] = "RESURRECTOR";

		/* offensives */
		cat2str[COMMANDER]   = "COMMANDER";
		cat2str[ATTACKER]    = "ATTACKER";
		cat2str[ANTIAIR]     = "ANTIAIR";
		cat2str[SCOUTER]     = "SCOUTER";
		cat2str[ARTILLERY]   = "ARTILLERY";
		cat2str[SNIPER]      = "SNIPER";
		cat2str[ASSAULT]     = "ASSAULT";

		/* economic */
		cat2str[MEXTRACTOR]  = "MEXTRACTOR";
		cat2str[MMAKER]      = "MMAKER";
		cat2str[EMAKER]      = "EMAKER";
		cat2str[MSTORAGE]    = "MSTORAGE";
		cat2str[ESTORAGE]    = "ESTORAGE";

		/* factory types */
		cat2str[KBOT]        = "KBOT";
		cat2str[VEHICLE]     = "VEHICLE";
		cat2str[HOVER]       = "HOVER";
		cat2str[AIRCRAFT]    = "AIRCRAFT"; // not used yet
		cat2str[NAVAL]       = "NAVAL"; // not used yet

		cat2str[DEFENSE]     = "DEFENSE";
	}

	if (str2cat.empty()) {
		/* Create the str2cat table and cats vector */
		std::map<unitCategory,std::string>::iterator i;
		for (i = cat2str.begin(); i != cat2str.end(); ++i) {
			cats.push_back(i->first);
			str2cat[i->second] = i->first;
		}
	}

	maxUnitPower = 0.0f;
	numUnits = ai->cb->GetNumUnitDefs();

 	/* Build the techtree, note that this is actually a graph in XTA */
	buildTechTree();

	bool success = false;

	std::string filename = ai->cfgparser->getFilename(GET_CAT|GET_TEAM);
	if (ai->cfgparser->fileExists(filename))
		success = ai->cfgparser->parseCategories(filename, units);
	if (!success) {
		filename = ai->cfgparser->getFilename(GET_CAT);
		success = ai->cfgparser->parseCategories(filename, units);
		if (!success) {
			filename = util::GetAbsFileName(ai->cb, std::string(CFG_FOLDER) + filename, false);
			generateCategorizationFile(filename);
		}
	}
	if (success) {
		// try loading overload file...
		filename = ai->cfgparser->getFilename(GET_CAT|GET_VER);
		if (ai->cfgparser->fileExists(filename))
			ai->cfgparser->parseCategories(filename, units);
	}		

	/* Generate the buildBy and canBuild lists per UnitType */
	/*
	std::map<int, UnitType*>::iterator l;
	std::string buildBy, canBuild;
	std::map<int, UnitType>::iterator j;
	for (j = units.begin(); j != units.end(); j++) {
		UnitType *utParent = &(j->second);

		debugCategories(utParent);
		debugUnitDefs(utParent);
		debugWeapons(utParent);
		canBuild = buildBy = "";
		for (l = utParent->buildBy.begin(); l != utParent->buildBy.end(); l++) {
			std::stringstream out;
			out << l->first;
			buildBy += l->second->def->name + "(" + out.str() + "), ";
		}
		buildBy = buildBy.substr(0, buildBy.length() - 2);
		for (l = utParent->canBuild.begin(); l != utParent->canBuild.end(); l++) {
			std::stringstream out;
			out << l->first;
			canBuild += l->second->def->name + "(" + out.str() + "), ";
		}
		canBuild = canBuild.substr(0, canBuild.length() - 2);
	}
	*/

	LOG_II("CUnitTable::CUnitTable Number of unit types: " << numUnits);
	LOG_II("CUnitTable::CUnitTable Max unit power: " << maxUnitPower);
}

CUnitTable::~CUnitTable()
{
}

void CUnitTable::generateCategorizationFile(std::string &fileName) {
	std::ofstream file(fileName.c_str(), std::ios::trunc);
	file << "# Unit Categorization for E323AI\n\n# Categories to choose from:\n";
	std::map<unitCategory,std::string>::iterator i;
	std::map<int, UnitType>::iterator j;
	UnitType *utParent;
	for (i = cat2str.begin(); i != cat2str.end(); i++) {
		file << "# " << i->second << "\n";
	}
	
	file << "\n\n# " << numUnits << " units in total\n\n";
	for (j = units.begin(); j != units.end(); j++) {
		utParent = &(j->second);
		file << "# " << utParent->def->humanName << "\n";
		file << utParent->def->name;
		for (unsigned i = 0; i < cats.size(); i++)
			if (cats[i] & utParent->cats)
				file << "," << cat2str[cats[i]];
		file << "\n\n";
	}
	file.close();
	LOG_II("Generated categorizations " << fileName)
}

void CUnitTable::remove(ARegistrar &object) {
	CUnit *unit = dynamic_cast<CUnit*>(&object);
	LOG_II("CUnitTable::remove " << (*unit))
	builders.erase(unit->key);
	idle.erase(unit->key);
	metalMakers.erase(unit->key);
	activeUnits.erase(unit->key);
	factories.erase(unit->key);
	defenses.erase(unit->key);
	unitsAliveTime.erase(unit->key);
	energyStorages.erase(unit->key);
	unitsUnderPlayerControl.erase(unit->key);
	unitsUnderConstruction.erase(unit->key);
	unitsBuilding.erase(unit->key);
	unit->unreg(*this);
	ReusableObjectFactory<CUnit>::Release(unit);
}

CUnit* CUnitTable::getUnit(int uid) {
	std::map<int, CUnit*>::iterator u = activeUnits.find(uid);
	if (u == activeUnits.end())
		return NULL;
	else
		return u->second;
}

CUnit* CUnitTable::requestUnit(int uid, int bid) {
	CUnit *unit = ReusableObjectFactory<CUnit>::Instance();
	unit->ai = ai;
	unit->reset(uid, bid);
	unit->reg(*this);
	if (bid > 0)
		builders[bid] = false;
	activeUnits[uid] = unit;
	idle[bid] = false;
	idle[uid] = false;
	if ((unit->type->cats&MOBILE) && bid >= 0) {
		unit->techlvl = (activeUnits[bid]->type->cats&TECH1) ? TECH1 : unit->techlvl;
		unit->techlvl = (activeUnits[bid]->type->cats&TECH2) ? TECH2 : unit->techlvl;
		unit->techlvl = (activeUnits[bid]->type->cats&TECH3) ? TECH3 : unit->techlvl;
		unit->techlvl = (activeUnits[bid]->type->cats&TECH4) ? TECH4 : unit->techlvl;
		unit->techlvl = (activeUnits[bid]->type->cats&TECH5) ? TECH5 : unit->techlvl;
	}
	// NOTE: remember that NOTA has mobile defenses
	if ((unit->type->cats&STATIC) && (unit->type->cats&ATTACKER) || (unit->type->cats&DEFENSE))
		defenses[unit->key] = unit;
	if (unit->type->cats&ESTORAGE)
		energyStorages[unit->key] = unit;
	return unit;
}

void CUnitTable::update() {
	std::map<int, int>::iterator i;
	for (i = unitsAliveTime.begin(); i != unitsAliveTime.end(); i++) {
		/* Makes sure new units are not instantly assigned tasks */
		if(!activeUnits[i->first]->isMicroing())
			i->second += MULTIPLEXER;
	}
}

bool CUnitTable::canPerformTask(CUnit &unit) {
	/* lifetime of more then 5 seconds */
	return unitsAliveTime.find(unit.key) != unitsAliveTime.end() && unitsAliveTime[unit.key] > IDLE_UNIT_TIMEOUT;
}

void CUnitTable::buildTechTree() {
	if (!units.empty())
		return; // alreay initialized

	std::map<int, std::string> buildOptions;
	std::map<int, std::string>::iterator j;
	std::vector<const UnitDef*> unitdefs(numUnits);
	
	ai->cb->GetUnitDefList(&unitdefs[0]);

	// NOTE: -1 movetype means a graph for aircraft
	moveTypes[-1] = NULL;
	
	for (int i = 0; i < numUnits; i++) {
		const UnitDef *ud = unitdefs[i];
		if (ud == NULL) continue;
		std::map<int, UnitType>::iterator u = units.find(ud->id);

		UnitType *utParent, *utChild;

		if (u == units.end()) {
			utParent = insertUnit(ud);
			MoveData* md = utParent->def->movedata;
			if (md != NULL)
				moveTypes[md->pathType] = md;
		}
		else
			utParent = &(u->second);

		buildOptions = ud->buildOptions;
		for (j = buildOptions.begin(); j != buildOptions.end(); j++) {
			ud = ai->cb->GetUnitDef(j->second.c_str());
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
	ut.costMetal  = ud->metalCost;
	ut.energyMake = ud->energyMake - ud->energyUpkeep;
	ut.metalMake  = ud->metalMake  - ud->metalUpkeep;
	ut.dps        = calcUnitDps(&ut);
	units[ud->id] = ut;
	
	if (maxUnitPower < ut.dps)
		maxUnitPower = ut.dps;

	return &units[ud->id];
}

bool CUnitTable::hasAntiAir(const std::vector<UnitDef::UnitDefWeapon> &weapons) {
	for (unsigned int i = 0; i < weapons.size(); i++) {
		const UnitDef::UnitDefWeapon *weapon = &weapons[i];
		/*
		weapon->def->canAttackGround
		weapon->def->onlyTargetCategory 
		*/
		//FIXME: Still selects Brawlers and Rangers
		if (weapon->def->tracks && !weapon->def->waterweapon && !weapon->def->stockpile)
			return true;
	}
	return false;
}

unsigned int CUnitTable::categorizeUnit(UnitType *ut) {
	const UnitDef *ud = ut->def;
	unsigned int cats = 0;
	
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
		cats |= ASSISTER;

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

	// NOTE: we aren't checking for "canMove" becasue it is usually used to set rally point
	// for factory
	if (!ud->buildOptions.empty()) {
		cats |= FACTORY;

		// precise factory type...
		std::map<int, std::string>::const_iterator j;
		for (j = ud->buildOptions.begin(); j != ud->buildOptions.end(); ++j) {
			const UnitDef *canbuild = ai->cb->GetUnitDef(j->second.c_str());
			
			if (canbuild->speed < EPS && (cats&FACTORY))
				// this is a static builder, not a factory
				cats -= FACTORY; 

			if (canbuild == NULL)
				continue;
			
			if (canbuild->canfly) {
				cats |= AIRCRAFT;
				break;
			}
			
			if (canbuild->movedata == NULL) 
				continue;
			
			if (canbuild->movedata->moveFamily == MoveData::KBot &&
				ud->minWaterDepth < 0.0f) {
				cats |= KBOT;
				break;
			}
			
			if (canbuild->movedata->moveFamily == MoveData::Tank &&
				ud->minWaterDepth < 0.0f) {
				cats |= VEHICLE;
				break;
			}
			
			if (canbuild->movedata->moveFamily == MoveData::Hover) {
				cats |= HOVER;
				break;
			}

			if (canbuild->movedata->moveFamily == MoveData::Ship) {
				cats |= NAVAL;
				break;
			}			
		}
		
		// HACK: true
		if (ud->metalCost < 2000.0f)
			cats |= TECH1;
		else
			cats |= TECH2;
	}

	if (ud->metalStorage / ut->cost > 0.1f)
		cats |= MSTORAGE;

	if (ud->energyStorage / ut->cost > 0.2f)
		cats |= ESTORAGE;

	if (ud->makesMetal >= 0.5f && (ud->energyUpkeep > (ud->makesMetal * 40.0f)))
		cats |= MMAKER;

	if ((ud->energyMake - ud->energyUpkeep) / ut->cost > 0.002 ||
		ud->tidalGenerator || ud->windGenerator)
		cats |= EMAKER;
	
	if (ud->extractsMetal)
		cats |= MEXTRACTOR;
	
/*
	if (ud->loadingRadius > 0.0f && ud->transportCapacity > 0)
		cats |= TRANSPORT;
*/

	/* 0 = only low, 1 = only high, 2 both */
	if (ud->highTrajectoryType >= 1)
		cats |= ARTILLERY;

	if ((cats&ATTACKER) && (cats&MOBILE) && !(cats&BUILDER) && ud->speed >= 50.0f) {
		std::map<int, UnitType*>::iterator i,j;
		for (i = ut->buildBy.begin(); i != ut->buildBy.end(); ++i) {
			bool isCheapest = true;
			UnitType *bb = i->second;
			for (j = bb->canBuild.begin(); j != bb->canBuild.end(); ++j) {
				if (ut->cost > j->second->cost && !j->second->def->weapons.empty()) {
					isCheapest = false;
					break;
				}
			}
			if (isCheapest) {
				cats |= SCOUTER;
				break;
			}
		}
	}

	return cats;
}

float CUnitTable::calcUnitDps(UnitType *ut) {
	// FIXME: make our own *briljant* dps calc here
	return ut->def->power;
}

int CUnitTable::factoryCount(unsigned int c) {
	int result = 0;
	std::map<int, CUnit*>::iterator i;

	for (i = factories.begin(); i != factories.end(); ++i) {
		if ((c&i->second->type->cats) == c)
			result++;
	}
	
	return result;
}

bool CUnitTable::gotFactory(unsigned int c) {
	return factoryCount(c) > 0;
}

void CUnitTable::getBuildables(UnitType *ut, unsigned include, unsigned exclude, std::multimap<float, UnitType*> &candidates) {
	static const unsigned int envCats = (AIR|SEA|LAND);
	unsigned int incEnvCats = (envCats&include);
	std::vector<unitCategory> incCats, excCats;
	
	// split categories...
	for (unsigned int i = 0; i < cats.size(); i++) {
		if (include&cats[i])
			incCats.push_back(cats[i]);
		else if (exclude&cats[i])
			excCats.push_back(cats[i]);
	}

	std::map<int, UnitType*>::iterator j;
	for (j = ut->canBuild.begin(); j != ut->canBuild.end(); ++j) {
		bool valid = true;
		unsigned int cat = j->second->cats;
		for (unsigned int i = 0; i < incCats.size(); i++) {
			// NOTE: evironment tags are handled differently: if requested
			// AIR, LAND & SEA in any combination that means having at least
			// one match automatically qualifies unit as valid
			if (incCats[i]&envCats) {
				if (incEnvCats) {
					// filter by environment tags is active
					if (!(incEnvCats&cat)) {
						valid = false;
						break;
					}
				}
			}
			else if (!(incCats[i]&cat)) {
				valid = false;
				break;
			}
		}

		if (valid) {
			/* Filter out excludes */
			for (unsigned int i = 0; i < excCats.size(); i++) {
				if (excCats[i]&cat) {
					valid = false;
					break;
				}
			}
			
			if (valid) {
				float cost = j->second->cost;
				candidates.insert(std::pair<float,UnitType*>(cost, j->second));
			}
		}
	}
	
	if (candidates.empty())
		LOG_WW("CUnitTable::getBuildables no candidates found INCLUDE(" << debugCategories(include) << ") EXCLUDE("<<debugCategories(exclude)<<") for unitdef(" << ut->def->humanName << ")")
}

UnitType* CUnitTable::canBuild(UnitType *ut, unsigned int c) {
	std::map<int, UnitType*>::iterator it;
	// TODO: make it compatible with environment tags
	for (it = ut->canBuild.begin(); it != ut->canBuild.end(); it++) {
		if ((it->second->cats & c) == c)
			return it->second;
	}

	//LOG_WW("CUnitTable::canBuild failed to build " << debugCategories(c))
	
	return NULL;
}

CUnit* CUnitTable::getUnitByDef(std::map<int, CUnit*> &dic, const UnitDef *udef) {
	return CUnitTable::getUnitByDef(dic, udef->id);
}

CUnit* CUnitTable::getUnitByDef(std::map<int, CUnit*> &dic, int did) {
	CUnit* unit;
	std::map<int, CUnit*>::const_iterator i;
	for(i = dic.begin(); i != dic.end(); i++) {
		unit = i->second;
		if(unit->def->id == did) {
			return unit;
		}
	}
	return NULL;
}

UnitType* CUnitTable::getUnitTypeByCats(unsigned int c) {
	std::map<int, UnitType>::iterator it;
	for (it = units.begin(); it != units.end(); it++) {
		if ((it->second.cats&c) == c)
			return &(it->second);
	}
	return NULL;
}

int CUnitTable::setOnOff(std::map<int, CUnit*>& list, bool value) {
	int result = 0;
	std::map<int, CUnit*>::iterator i;
	
	for (i = list.begin(); i != list.end(); ++i) {
		CUnit *unit = i->second;
		if (value != unit->isOn()) {
			unit->setOnOff(value);
			result++;
		}
	}

	return result;
}

std::string CUnitTable::debugCategories(unsigned categories) {
	std::string cats("");
	std::map<unitCategory, std::string>::iterator i;
	for (i = cat2str.begin(); i != cat2str.end(); i++) {
		int v = categories & i->first;
		if (v == i->first)
			cats += i->second + " | ";
	}
	cats = cats.substr(0, cats.length()-3);
	return cats;
}

std::string CUnitTable::debugCategories(UnitType *ut) {
	std::string cats("");
	std::map<unitCategory, std::string>::iterator i;
	for (i = cat2str.begin(); i != cat2str.end(); i++) {
		int v = ut->cats & i->first;
		if (v == i->first)
			cats += i->second + " | ";
	}
	cats = cats.substr(0, cats.length()-3);
	return cats;
}

void CUnitTable::debugUnitDefs(UnitType *ut) {
	const UnitDef *ud = ut->def;
	sprintf(buf, "metalUpKeep(%0.2f), metalMake(%0.2f), makesMetal(%0.2f), energyUpkeep(%0.2f), energyMake(%0.2f)\n", ud->metalUpkeep, ud->metalMake, ud->makesMetal, ud->energyUpkeep, ud->energyMake);
	sprintf(buf, "buildTime(%0.2f), mCost(%0.2f), eCost(%0.2f)\n", ud->buildTime, ud->metalCost, ud->energyCost);
}

void CUnitTable::debugWeapons(UnitType *ut) {
	const UnitDef *ud = ut->def;
	for (unsigned int i = 0; i < ud->weapons.size(); i++) {
		const UnitDef::UnitDefWeapon *w = &(ud->weapons[i]);
		sprintf(buf, "Weapon name = %s\n", w->def->type.c_str());
	}
}
