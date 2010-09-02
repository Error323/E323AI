#include "CIntel.h"

#include "CAI.h"
#include "CUnitTable.h"
#include "CUnit.h"
#include "GameMap.hpp"
#include <ctime>

CIntel::CIntel(AIClasses *ai) {
	this->ai = ai;
	
	units = &ai->unitIDs[0]; // save about 4x32KB of memory

	selector.push_back(ASSAULT);
	selector.push_back(SCOUTER);
	selector.push_back(SNIPER);
	selector.push_back(ARTILLERY);
	selector.push_back(ANTIAIR);
	selector.push_back(AIR);
	
	for (size_t i = 0; i < selector.size(); i++)
		counts[selector[i]] = 1;
	
	enemyvector = float3(1.0f, 1.0f, 1.0f);
	
	targets[ENGAGE].push_back(&commanders);
	targets[ENGAGE].push_back(&attackers);
	targets[ENGAGE].push_back(&energyMakers);
	targets[ENGAGE].push_back(&metalMakers);
	targets[ENGAGE].push_back(&defenseAntiAir);
	targets[ENGAGE].push_back(&defenseGround);
	targets[ENGAGE].push_back(&mobileBuilders);
	targets[ENGAGE].push_back(&factories);
	targets[ENGAGE].push_back(&restUnarmedUnits);
	
	targets[SCOUT].push_back(&mobileBuilders);
	targets[SCOUT].push_back(&metalMakers);
	targets[SCOUT].push_back(&energyMakers);
	targets[SCOUT].push_back(&restUnarmedUnits);

	targets[BOMBER].push_back(&defenseAntiAir);
	targets[BOMBER].push_back(&defenseGround);
	targets[BOMBER].push_back(&commanders);
	targets[BOMBER].push_back(&factories);
	targets[BOMBER].push_back(&energyMakers);
	targets[BOMBER].push_back(&metalMakers);
	
	initialized = false;
	strategyTechUp = false;
}

float3 CIntel::getEnemyVector() {
	return enemyvector;
}

void CIntel::init() {
	if (initialized) return;
	
	int numUnits = ai->cbc->GetEnemyUnits(units, MAX_UNITS);
	// FIXME: when commanders are spawned with wrap gate option enabled then assert raises
	if (numUnits > 0) {
		enemyvector = ZeroVector;
		for (int i = 0; i < numUnits; i++) {
			enemyvector += ai->cbc->GetUnitPos(units[i]);
		}
		enemyvector /= numUnits;
	}

	LOG_II("CIntel::init Number of enemy units: " << numUnits)
	
	/* FIXME:
		I faced situation that on maps with less land there is a direct
		path to enemy unit, but algo below starts to play a non-land game. 
		I could not think up an apporpiate algo to avoid this. I though tracing
		a path in the beginning of the game from my commander to enemy would 
		be ok, but commander is an amphibious unit. It is not trivial stuff 
		without external helpers in config files.
	*/
	if(ai->gamemap->IsWaterMap()) {
		allowedFactories.push_back(NAVAL);
		allowedFactories.push_back(HOVER);
	} 
	else {
		unitCategory nextFactory;
		if (ai->gamemap->IsKbotMap()) {
			allowedFactories.push_back(KBOT);
			nextFactory = VEHICLE;
		} 
		else {
			allowedFactories.push_back(VEHICLE);
			nextFactory = KBOT;
		}
		
		if (ai->gamemap->IsHooverMap()) {
			if (ai->gamemap->GetAmountOfWater() > 0.5) {
				allowedFactories.push_back(HOVER);
			}
			else {
				allowedFactories.push_back(nextFactory);
				nextFactory = HOVER;
			}
		}
		
		allowedFactories.push_back(nextFactory);
	}
	// TODO: do not build air on too small maps?
	allowedFactories.push_back(AIRCRAFT);

	// vary first factory among allied AIs...
	int i = ai->allyAITeam;
	while (i > 1) {
		allowedFactories.push_back(allowedFactories.front());
		allowedFactories.pop_front();
		i--;
	}
	
	// FIXME: engineer better decision algo
	if (ai->gamemap->IsMetalMap())
		strategyTechUp = true;
	else
		// NOTE: clock() gives much better results than rng.RndFloat() (at least under MSVC)
		strategyTechUp = ((clock() % 3) == 0);

	LOG_II("CIntel::init Tech-up strategy: " << strategyTechUp)

	initialized = true;
}

void CIntel::update(int frame) {
	mobileBuilders.clear();
	factories.clear();
	attackers.clear();
	metalMakers.clear();
	energyMakers.clear();
	navalUnits.clear();
	underwaterUnits.clear();
	restUnarmedUnits.clear();
	rest.clear();
	defenseGround.clear();
	defenseAntiAir.clear();
	commanders.clear();
	
	resetCounters();

	int numUnits = ai->cbc->GetEnemyUnits(units, MAX_UNITS);
	
	for (int i = 0; i < numUnits; i++) {
		const int uid = units[i];
		const UnitDef *ud = ai->cbc->GetUnitDef(uid);
		UnitType      *ut = UT(ud->id);
		unsigned int    c = ut->cats;
		bool armed = !ud->weapons.empty();

		/* Ignore units being built and cloaked units */
		if ((ai->cbc->UnitBeingBuilt(uid) && (armed || !(c&STATIC)))
		|| 	ai->cbc->IsUnitCloaked(uid))
			continue;
		
		if (c&COMMANDER) {
			commanders.push_back(uid);
		}
		else if (c&SEA && ai->cbc->GetUnitPos(uid).y < 0.0f) {
			underwaterUnits.push_back(uid);
		}
		else if (!(c&(LAND|AIR)) && c&SEA) {
			navalUnits.push_back(uid);
		}
		else if ((c&STATIC) && (c&ANTIAIR)) {
			defenseAntiAir.push_back(uid);
		}
		else if ((c&STATIC) && (c&DEFENSE)) {
			defenseGround.push_back(uid);
		}
		else if ((c&ATTACKER) && !(c&AIR)) {
			attackers.push_back(uid);
		}
		else if (c&FACTORY) {
			factories.push_back(uid);
		}
		else if (c&BUILDER && c&MOBILE && !(c&AIR)) {
			mobileBuilders.push_back(uid);
		}
		else if (c&(MEXTRACTOR|MMAKER)) {
			metalMakers.push_back(uid);
		}
		else if (c&EMAKER) {
			energyMakers.push_back(uid);
		}
		else if (!armed)
			restUnarmedUnits.push_back(uid);
		else {
			rest.push_back(uid);
		}

		if ((c&ATTACKER) && (c&MOBILE))
			updateCounts(c);
	}
}

unitCategory CIntel::counter(unitCategory c) {
	switch(c) {
		case ASSAULT: return SNIPER;
		case SCOUTER: return ASSAULT;
		case SNIPER: return ARTILLERY;
		case ARTILLERY: return ASSAULT;
		case ANTIAIR: return ARTILLERY;
		case AIR: return ANTIAIR;
		default: return ARTILLERY;
	}
}

void CIntel::updateCounts(unsigned c) {
	for (size_t i = 0; i < selector.size(); i++) {
		if (selector[i] & c) {
			// TODO: counts[ai->cfgparser->counter(selector[i])]++
			counts[counter(selector[i])]++;
			totalCount++;
		}
	}
}

void CIntel::resetCounters() {
	roulette.clear();
	
	/* Put the counts in a normalized reversed map first and reset counters*/
	for (size_t i = 0; i < selector.size(); i++) {
		roulette.insert(std::pair<float,unitCategory>(counts[selector[i]]/float(totalCount), selector[i]));
		counts[selector[i]] = 1;
	}
	
	counts[ANTIAIR] = 0;
	counts[AIR] = 0;
	counts[ASSAULT] = 3;
	
	if (ai->difficulty == DIFFICULTY_EASY || ai->military->idleScoutGroupsNum() >= MAX_IDLE_SCOUT_GROUPS)
		counts[SCOUTER] = 0;
	else
		counts[SCOUTER] = 1; // default value
	
	totalCount = selector.size();
}

bool CIntel::enemyInbound() {
	return false;
}
