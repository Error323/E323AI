#include "CIntel.h"

#include "CAI.h"
#include "CUnitTable.h"

CIntel::CIntel(AIClasses *ai) {
	this->ai = ai;
	units = new int[MAX_UNITS];
	selector.push_back(ASSAULT);
	selector.push_back(SCOUTER);
	selector.push_back(SNIPER);
	selector.push_back(ARTILLERY);
	selector.push_back(ANTIAIR);
	for (size_t i = 0; i < selector.size(); i++)
		counts[selector[i]] = 1;
}

void CIntel::update(int frame) {
	mobileBuilders.clear();
	factories.clear();
	attackers.clear();
	metalMakers.clear();
	energyMakers.clear();
	rest.clear();
	resetCounters();
	int numUnits = ai->cbc->GetEnemyUnits(units, MAX_UNITS);
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

void CIntel::updateCounts(unsigned c) {
	for (size_t i = 0; i < selector.size(); i++) {
		if (selector[i] & c) {
			counts[selector[i]]++;
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

	totalCount = selector.size();
}

bool CIntel::enemyInbound() {
	return false;
}
