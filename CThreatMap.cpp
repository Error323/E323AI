#include "CThreatMap.h"

#include "CAI.h"
#include "CUnitTable.h"
#include "CIntel.h"

CThreatMap::CThreatMap(AIClasses *ai) {
	this->ai = ai;
	this->X  = ai->cb->GetMapWidth() / (HEIGHT2SLOPE*I_MAP_RES);
	this->Z  = ai->cb->GetMapHeight() / (HEIGHT2SLOPE*I_MAP_RES);
	REAL     = HEIGHT2SLOPE*HEIGHT2REAL*I_MAP_RES;
	map      = new float[X*Z];
	units    = new int[MAX_UNITS_AI];

	for (int i = 0; i < X*Z; i++)
		map[i] = 1.0f;
}

CThreatMap::~CThreatMap() {
	delete[] map;
	delete[] units;
}

float CThreatMap::getThreat(float3 &center) {
	float power = 0.0f;
	for (size_t i = 0; i < ai->intel->attackers.size(); i++) {
		int enemy = ai->intel->attackers[i];
		float health = ai->cbc->GetUnitHealth(enemy);
		if (health <= 0.0f) 
			continue;

		float3 pos = ai->cbc->GetUnitPos(enemy);
		const UnitDef *ud = ai->cbc->GetUnitDef(enemy);
		float range = ud->maxWeaponRange*1.2f;
		if ((pos - center).Length2D() < range) {
			power += ai->cbc->GetUnitPower(enemy);
		}
	}

	return power;
}

void CThreatMap::update(int frame) {
	totalPower = 0.0f;
	for (int i = 0; i < X*Z; i++)
		map[i] = 1.0f;

	int numUnits = ai->cbc->GetEnemyUnits(units, MAX_UNITS_AI);
	if (numUnits > MAX_UNITS_AI)
		LOG_WW("CThreatMap::update " << numUnits << " > " << MAX_UNITS_AI)

	for (int i = 0; i < numUnits; i++) {
		const UnitDef  *ud = ai->cbc->GetUnitDef(units[i]);
		const UnitType *ut = UT(ud->id);
		
		if (ut->cats&ATTACKER && !ai->cbc->UnitBeingBuilt(units[i])) {
			const float3  upos = ai->cbc->GetUnitPos(units[i]);
			const float uRealX = upos.x/REAL;
			const float uRealZ = upos.z/REAL;
			const float  range = (ud->maxWeaponRange*1.2f)/REAL;
			float       powerT = ai->cbc->GetUnitPower(units[i]);
			if (ut->cats&COMMANDER)
				powerT /= 100.0f; /* dgun gives overpowered dps */
			const float  power = powerT;
			float3 pos(0.0f, 0.0f, 0.0f);

			const int        R = (int) ceil(range);
			for (int z = -R; z <= R; z++) {
				for (int x = -R; x <= R; x++) {
					pos.x = x;
					pos.z = z;
					if (pos.Length2D() <= range) {
						pos.x += uRealX;
						pos.z += uRealZ;
						const unsigned int mx = (unsigned int) round(pos.x);
						const unsigned int mz = (unsigned int) round(pos.z);
						if (mx < X && mz < Z)
							map[ID(mx,mz)] += power;
					}
				}
			}
			totalPower += power;
		}
	}
	//draw();
}

void CThreatMap::draw() {
	for (int z = 0; z < Z; z++) {
		for (int x = 0; x < X; x++) {
			if (map[ID(x,z)] > 1.0f+EPSILON) {
				float3 p0(x*REAL, ai->cb->GetElevation(x*REAL,z*REAL), z*REAL);
				float3 p1(p0);
				p1.y += (map[ID(x,z)]/totalPower) * 300.0f;
				ai->cb->CreateLineFigure(p0, p1, 4, 1, 360, 1);
			}
		}
	}
}
