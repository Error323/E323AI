#include "Intel.h"

CIntel::CIntel(AIClasses *ai) {
	this->ai = ai;
	units = new int[MAX_UNITS];
}

CIntel::~CIntel() {
	delete[] units;
}

void CIntel::update(int frame) {
	mobileBuilders.clear();
	factories.clear();
	attackers.clear();
	metalMakers.clear();
	energyMakers.clear();
	int numUnits = ai->cheat->GetEnemyUnits(units, MAX_UNITS);
	for (int i = 0; i < numUnits; i++) {
		const UnitDef *ud = ai->cheat->GetUnitDef(units[i]);
		UnitType      *ut = UT(ud->id);
		unsigned int    c = ut->cats;
		
		if (c&ATTACKER && c&MOBILE) {
			attackers.push_back(units[i]);
		}
		else if (c&FACTORY) {
			factories.push_back(units[i]);
			if (c&AIR)
				hasAir = true;
			else
				hasAir = false;
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
	}
}

bool CIntel::enemyInbound() {
	return false;
}
