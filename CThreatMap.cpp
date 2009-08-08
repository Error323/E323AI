#include "CThreatMap.h"
#include "ScopedTimer.h"

CThreatMap::CThreatMap(AIClasses *ai) {
	REAL = THREATRES*8.0f;
	this->ai = ai;
	this->X  = ai->call->GetMapWidth() / THREATRES;
	this->Z  = ai->call->GetMapHeight() / THREATRES;

	map   = new float[X*Z];
	units = new int[MAX_UNITS];
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
		return map[id(x,z)];
	else
		return 0.0f;
}

float CThreatMap::getThreat(float3 &center, float radius) {
	int R = (int) ceil(radius);
	float3 pos(0.0f, 0.0f, 0.0f);
	float summedPower = 0.0f;
	int count = 0;
	for (int x = -R; x <= R; x+=2) {
		for (int z = -R; z <= R; z+=2) {
			pos.x = x;
			pos.z = z;
			if (pos.Length2D() <= radius) {
				pos.x += center.x;
				pos.z += center.z;
				summedPower += getThreat(pos);
				count++;
			}
		}
	}
	return summedPower/count;
}

void CThreatMap::update(int frame) {
	totalPower = 0.0f;
	for (int i = 0; i < X*Z; i++)
		map[i] = 1.0f;

	int numUnits = ai->cheat->GetEnemyUnits(units, MAX_UNITS);
	for (int i = 0; i < numUnits; i++) {
		const UnitDef *ud = ai->cheat->GetUnitDef(units[i]);
		UnitType      *ut = UT(ud->id);
		float3 upos = ai->cheat->GetUnitPos(units[i]);
		float3 pos(0.0f, 0.0f, 0.0f);
		
		if (ut->cats&ATTACKER && !ai->cheat->UnitBeingBuilt(units[i])) {
			float range = (ud->maxWeaponRange*1.2f)/REAL;
			float power = ai->cheat->GetUnitPower(units[i]);
			if (ut->cats&COMMANDER)
				power /= 100.0f; /* dgun gives overpowered dps */

			int   R = (int) ceil(range);
			for (int x = -R; x <= R; x++) {
				for (int z = -R; z <= R; z++) {
					pos.x = x;
					pos.z = z;
					if (pos.Length2D() <= range) {
						pos.x += upos.x/REAL;
						pos.z += upos.z/REAL;
						int mx = (int) round(pos.x);
						int mz = (int) round(pos.z);
						if (mx >= 0 && mx < X && mz >= 0 && mz < Z)
							map[id(mx,mz)] += power;
					}
				}
			}
			totalPower += power;
		}
	}
}

void CThreatMap::draw() {
	for (int x = 0; x < X; x++) {
		for (int z = 0; z < Z; z++) {
			if (map[id(x,z)] > 1.0f) {
				float3 p0(x*REAL, 0.0f, z*REAL);
				float3 p1(p0);
				p1.y += map[id(x,z)]/totalPower;
				p1.y *= 30.0f;
				p1.y += 100.0f;
				ai->call->CreateLineFigure(p0, p1, 4, 1, 300, 1);
			}
		}
	}
}
