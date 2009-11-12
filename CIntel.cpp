#include "CIntel.h"

#include "CAI.h"
#include "CUnitTable.h"
#include "CUnit.h"

CIntel::CIntel(AIClasses *ai) {
	this->ai = ai;
	units = new int[MAX_UNITS];
	selector.push_back(ASSAULT);
	selector.push_back(SCOUTER);
	selector.push_back(SNIPER);
	selector.push_back(ARTILLERY);
	selector.push_back(ANTIAIR);
	selector.push_back(AIR);
	for (size_t i = 0; i < selector.size(); i++)
		counts[selector[i]] = 1;
	numUnits = 1;
}

float3 CIntel::getEnemyVector() {
	return enemyvector;
}

void CIntel::init() {
	numUnits = ai->cbc->GetEnemyUnits(units, MAX_UNITS);
	assert(numUnits > 0);
	enemyvector = float3(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < numUnits; i++) {
		enemyvector += ai->cbc->GetUnitPos(units[i]);
	}
	enemyvector /= numUnits;
	LOG_II("Number of enemies: " << numUnits)
}

void CIntel::update(int frame) {
	mobileBuilders.clear();
	factories.clear();
	attackers.clear();
	metalMakers.clear();
	energyMakers.clear();
	rest.clear();
	resetCounters();
	numUnits = ai->cbc->GetEnemyUnits(units, MAX_UNITS);
	for (int i = 0; i < numUnits; i++) {
		const UnitDef *ud = ai->cbc->GetUnitDef(units[i]);
		UnitType      *ut = UT(ud->id);
		unsigned int    c = ut->cats;

		if (
			ai->cbc->UnitBeingBuilt(units[i]) || /* Ignore units being built */
			ai->cbc->IsUnitCloaked(units[i])     /* Ignore cloaked units */
		) continue;
		
		if (c&ATTACKER) {
			attackers.push_back(units[i]);
		}
		else if (c&FACTORY) {
			factories.push_back(units[i]);
		}
		else if (c&BUILDER && c&MOBILE) {
			mobileBuilders.push_back(units[i]);
		}
		else if (c&MEXTRACTOR || c&MMAKER) {
			metalMakers.push_back(units[i]);
		}
		else if (c&EMAKER) {
			energyMakers.push_back(units[i]);
		}
		else {
			rest.push_back(units[i]);
		}

		if (c&ATTACKER && c&MOBILE)
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
	totalCount = selector.size();
}

bool CIntel::enemyInbound() {
	return false;
}
