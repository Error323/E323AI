#include "CThreatMap.h"

#include "CAI.h"
#include "CUnitTable.h"
#include "CIntel.h"

CThreatMap::CThreatMap(AIClasses *ai) {
	this->ai = ai;
	this->X  = ai->cb->GetMapWidth() / (HEIGHT2SLOPE);
	this->Z  = ai->cb->GetMapHeight() / (HEIGHT2SLOPE);
	REAL = (HEIGHT2SLOPE)*HEIGHT2REAL;

	map   = new float[X*Z];
	units = new int[MAX_UNITS_AI];
	for (int i = 0; i < X*Z; i++)
		map[i] = 1.0f;
}

CThreatMap::~CThreatMap() {
	delete[] map;
	delete[] units;
}

float CThreatMap::getThreat(float3 &pos) {
	int x = (int) pos.x/REAL;
	int z = (int) pos.z/REAL;
	if (x >= 0 && x < X && z >= 0 && z < Z)
		return map[ID(x,z)];
	else
		return 0.0f;
}

float CThreatMap::getThreat(float3 &center, float radius) {
	float power = 0.0f;
	for (size_t i = 0; i < ai->intel->attackers.size(); i++) {
		int enemy = ai->intel->attackers[i];
		float3 pos = ai->cbc->GetUnitPos(enemy);
		if ((pos - center).Length2D() < radius) {
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
		const UnitDef *ud = ai->cbc->GetUnitDef(units[i]);
		UnitType      *ut = UT(ud->id);
		float3 upos = ai->cbc->GetUnitPos(units[i]);
		float3 pos(0.0f, 0.0f, 0.0f);
		
		if (ut->cats&ATTACKER && !ai->cbc->UnitBeingBuilt(units[i])) {
			float range = (ud->maxWeaponRange*1.2f)/REAL;
			float power = ai->cbc->GetUnitPower(units[i]);
			if (ut->cats&COMMANDER)
				power /= 100.0f; /* dgun gives overpowered dps */

			int   R = (int) ceil(range);
			for (int z = -R; z <= R; z++) {
				for (int x = -R; x <= R; x++) {
					pos.x = x;
					pos.z = z;
					if (pos.Length2D() <= range) {
						pos.x += upos.x/REAL;
						pos.z += upos.z/REAL;
						int mx = (int) round(pos.x);
						int mz = (int) round(pos.z);
						if (mx >= 0 && mx < X && mz >= 0 && mz < Z)
							map[ID(mx,mz)] += power;
					}
				}
			}
			totalPower += power;
		}
	}
}

void CThreatMap::draw() {
	for (int z = 0; z < Z; z++) {
		for (int x = 0; x < X; x++) {
			if (map[ID(x,z)] > 1.0f) {
				float3 p0(x*REAL, 0.0f, z*REAL);
				float3 p1(p0);
				p1.y += map[ID(x,z)]/totalPower;
				p1.y *= 30.0f;
				p1.y += 100.0f;
				ai->cb->CreateLineFigure(p0, p1, 4, 1, 300, 1);
			}
		}
	}
}
