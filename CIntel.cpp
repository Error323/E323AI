#include "CIntel.h"

#include <ctime>

#include "CAI.h"
#include "CUnit.h"
#include "GameMap.hpp"


CIntel::CIntel(AIClasses *ai) {
	this->ai = ai;

	initialized = false;
	strategyTechUp = false;

	selector.push_back(ASSAULT);
	selector.push_back(SCOUTER);
	selector.push_back(SNIPER);
	selector.push_back(ARTILLERY);
	selector.push_back(ANTIAIR);
	selector.push_back(AIR);
	selector.push_back(SUB);
	selector.push_back(COMMANDER);

	// NOTE: the order is somewhat target priority
	targets[ENGAGE].push_back(CategoryMatcher(COMMANDER));
	targets[ENGAGE].push_back(CategoryMatcher(ATTACKER));
	targets[ENGAGE].push_back(CategoryMatcher(EMAKER));
	targets[ENGAGE].push_back(CategoryMatcher(MMAKER));
	targets[ENGAGE].push_back(CategoryMatcher(DEFENSE));
	targets[ENGAGE].push_back(CategoryMatcher(BUILDER));
	targets[ENGAGE].push_back(CategoryMatcher(FACTORY));
	targets[ENGAGE].push_back(CategoryMatcher(CATS_ANY, ATTACKER));

	targets[SCOUT].push_back(CategoryMatcher(BUILDER, COMMANDER|FACTORY));
	targets[SCOUT].push_back(CategoryMatcher(MMAKER|MEXTRACTOR));
	targets[SCOUT].push_back(CategoryMatcher(EMAKER));
	targets[SCOUT].push_back(CategoryMatcher(CATS_ANY, ATTACKER));

	targets[BOMBER].push_back(CategoryMatcher(DEFENSE));
	targets[BOMBER].push_back(CategoryMatcher(COMMANDER));
	targets[BOMBER].push_back(CategoryMatcher(FACTORY));
	targets[BOMBER].push_back(CategoryMatcher(EMAKER));
	targets[BOMBER].push_back(CategoryMatcher(MMAKER));
	targets[BOMBER].push_back(CategoryMatcher(NUKE));

	targets[AIRFIGHTER].push_back(CategoryMatcher(AIR));

	for (TargetCategoryMap::iterator it = targets.begin(); it != targets.end(); ++it) {
		for (int i = 0; i < it->second.size(); i++) {
			enemies.registerMatcher(it->second[i]);
		}
	}
}

float3 CIntel::getEnemyVector() {
	return enemyvector;
}

void CIntel::init() {
	if (initialized) return;

	resetCounters();
	updateRoulette();

	updateEnemyVector();

	/* FIXME:
		I faced situation that on maps with less land there is a direct
		path to enemy unit, but algo below starts to play a non-land game.
		I could not think up an appropriate algo to avoid this. I thought about
		tracing a path in the beginning of the game from my commander to enemy
		would be ok, but commander is an amphibious unit. It is not trivial
		stuff without external helpers in config files or terrain analysis.
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
	int i = ai->allyIndex;
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
	resetCounters();

	if (enemyvector == ZeroVector)
		updateEnemyVector();

	int numUnits = ai->cbc->GetEnemyUnits(&ai->unitIDs[0], MAX_UNITS);

	for (int i = 0; i < numUnits; i++) {
		const int uid = ai->unitIDs[i];
		const UnitDef* ud = ai->cbc->GetUnitDef(uid);

		if (ud == NULL)
			continue;

		unitCategory c = UT(ud->id)->cats;

		if ((c&ATTACKER).any() && (c&MOBILE).any())
			updateCounters(c);
	}

	updateRoulette();
}

unitCategory CIntel::counter(unitCategory c) {
	// TODO: implement customizable by config counter units

	// NOTE: current algo is not perfect because does not consider
	// environmental tags

	if (c == AIR)		return ANTIAIR;
	if (c == SUB)		return TORPEDO;
	if (c == ASSAULT)	return SNIPER;
	if (c == SCOUTER)	return ASSAULT;
	if (c == SNIPER)	return ARTILLERY;
	if (c == ARTILLERY)	return ASSAULT;
	if (c == ANTIAIR)	return ARTILLERY;
	if (c == COMMANDER) return ASSAULT;

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
	// without need do not build the following unit cats...
	counterCounter[TORPEDO] = 0;
	counterCounter[ANTIAIR] = 0;
	counterCounter[AIR] = 0;
	counterCounter[SUB] = 0;
	counterCounter[COMMANDER] = 0;
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

void CIntel::onEnemyCreated(int enemy) {
	const UnitDef* ud = ai->cbc->GetUnitDef(enemy);
	if (ud) {
		LOG_II("CIntel::onEnemyCreated Unit(" << enemy << ")")
		//assert(ai->cbc->GetUnitTeam(enemy) != ai->team);
		enemies.addUnit(UT(ud->id), enemy);
	}
}

void CIntel::onEnemyDestroyed(int enemy, int attacker) {
	LOG_II("CIntel::onEnemyDestroyed Unit(" << enemy << ")")
	enemies.removeUnit(enemy);
}

void CIntel::updateEnemyVector() {
	int numUnits = ai->cbc->GetEnemyUnits(&ai->unitIDs[0], MAX_PLAYERS);

	enemyvector = ZeroVector;
	for (int i = 0; i < numUnits; i++) {
		enemyvector += ai->cbc->GetUnitPos(ai->unitIDs[i]);
	}
	enemyvector /= numUnits;
}
