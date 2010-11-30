#include "CIntel.h"

#include <ctime>

#include "CAI.h"
#include "CUnit.h"
#include "GameMap.hpp"


CIntel::CIntel(AIClasses *ai) {
	this->ai = ai;

	initialized = false;
	strategyTechUp = false;
	enemyvector = float3(1.0f, 1.0f, 1.0f);

	selector.push_back(ASSAULT);
	selector.push_back(SCOUTER);
	selector.push_back(SNIPER);
	selector.push_back(ARTILLERY);
	selector.push_back(ANTIAIR);
	selector.push_back(AIR);
	
	targets[ENGAGE].push_back(&commanders);
	targets[ENGAGE].push_back(&attackers);
	targets[ENGAGE].push_back(&nukes);
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
	targets[BOMBER].push_back(&nukes);
	
	targets[AIRFIGHTER].push_back(&airUnits);
}

float3 CIntel::getEnemyVector() {
	return enemyvector;
}

void CIntel::init() {
	if (initialized) return;
	
	resetCounters();
	updateRoulette();
	
	int numUnits = ai->cbc->GetEnemyUnits(&ai->unitIDs[0], MAX_UNITS);
	// FIXME: when commanders are spawned with wrap gate option enabled then 
	// assertion raises
	if (numUnits > 0) {
		enemyvector = ZeroVector;
		for (int i = 0; i < numUnits; i++) {
			enemyvector += ai->cbc->GetUnitPos(ai->unitIDs[i]);
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
	airUnits.clear();
	nukes.clear();
			
	resetCounters();

	int numUnits = ai->cbc->GetEnemyUnits(&ai->unitIDs[0], MAX_UNITS);
	
	for (int i = 0; i < numUnits; i++) {
		const int uid = ai->unitIDs[i];
		const UnitDef *ud = ai->cbc->GetUnitDef(uid);
		unitCategory    c = UT(ud->id)->cats;

		if (ai->cbc->IsUnitCloaked(uid))
			continue;
		
		if ((c&NUKE).any()) {
			nukes.push_back(uid);
			continue;
		}
		
		/* Ignore armed & mobile units being built */
		if ((ai->cbc->UnitBeingBuilt(uid) && ((c&ATTACKER).any() || (c&STATIC).none())))
			continue;
		
		if ((c&COMMANDER).any()) {
			commanders.push_back(uid);
		}
		else if ((c&SEA).any() && ai->cbc->GetUnitPos(uid).y < 0.0f) {
			underwaterUnits.push_back(uid);
		}
		else if ((c&(LAND|AIR)).none() && (c&SEA).any()) {
			navalUnits.push_back(uid);
		}
		else if ((c&STATIC).any() && (c&ANTIAIR).any()) {
			defenseAntiAir.push_back(uid);
		}
		else if ((c&STATIC).any() && (c&ATTACKER).any()) {
			defenseGround.push_back(uid);
		}
		else if ((c&ATTACKER).any() && (c&AIR).none()) {
			attackers.push_back(uid);
		}
		else if ((c&AIR).any()) {
			airUnits.push_back(uid);
		}
		else if ((c&FACTORY).any()) {
			factories.push_back(uid);
		}
		else if ((c&BUILDER).any() && (c&MOBILE).any()) {
			mobileBuilders.push_back(uid);
		}
		else if ((c&(MEXTRACTOR|MMAKER)).any()) {
			metalMakers.push_back(uid);
		}
		else if ((c&EMAKER).any()) {
			energyMakers.push_back(uid);
		}
		else if ((c&ATTACKER).none()) {
			restUnarmedUnits.push_back(uid);
		}
		else {
			rest.push_back(uid);
		}

		if ((c&ATTACKER).any() && (c&MOBILE).any())
			updateCounters(c);
	}

	updateRoulette();
}

unitCategory CIntel::counter(unitCategory c) {
	// TODO: implement customizable by config counter units
	if (c == ASSAULT)	return SNIPER;
	if (c == SCOUTER)	return ASSAULT;
	if (c == SNIPER)	return ARTILLERY;
	if (c == ARTILLERY)	return ASSAULT;
	if (c == ANTIAIR)	return ARTILLERY;
	if (c == AIR)		return ANTIAIR;
	return ARTILLERY;
}

void CIntel::updateCounters(unitCategory ecats) {
	for (size_t i = 0; i < selector.size(); i++) {
		const unitCategory c = selector[i];
		if ((c&ecats).any()) {
			enemyCounter[c]++;
			counterCounter[counter(c)]++;
			totalEnemyCount++;
			totalCounterCount++;
		}
	}
}

void CIntel::updateRoulette() {
	roulette.clear();

	if (totalCounterCount > 0) {
		/* Put the counts in a normalized reversed map first and reset counters */
		for (size_t i = 0; i < selector.size(); i++) {
			const unitCategory c = selector[i];
			const float weight = counterCounter[c] / float(totalCounterCount);
			roulette.insert(std::pair<float, unitCategory>(weight, c));
		}
	}
}

void CIntel::resetCounters() {
	// set equal chance of building military unit for all cats by default...
	for (size_t i = 0; i < selector.size(); i++) {
		counterCounter[selector[i]] = 1;
	}
	// without need do not build aircraft & anti-air units...
	counterCounter[ANTIAIR] = 0;
	counterCounter[AIR] = 0;
	// boost chance of assault units to be built by default
	counterCounter[ASSAULT] = 3;
	// adjust scout appearance chance by default...
	if (ai->difficulty == DIFFICULTY_EASY 
	|| ai->military->idleScoutGroupsNum() >= MAX_IDLE_SCOUT_GROUPS)
		counterCounter[SCOUTER] = 0;
	
	totalCounterCount = totalEnemyCount = 0;
	for (size_t i = 0; i < selector.size(); i++) {
		totalCounterCount += counterCounter[selector[i]];
	}
}

bool CIntel::enemyInbound() {
	// TODO:
	return false;
}
