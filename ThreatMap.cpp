#include "ThreatMap.h"

CThreatMap::CThreatMap(AIClasses *ai) {
	RES = 8;
	REAL = RES*8.0f;
	this->ai = ai;
	this->W  = ai->call->GetMapWidth() / RES;
	this->H  = ai->call->GetMapHeight() / RES;

	map   = new float[W*H];
	units = new int[MAX_UNITS];
	reset();
}

CThreatMap::~CThreatMap() {
	delete[] map;
	delete[] units;
}

float CThreatMap::getThreat(float3 pos) {
	int x = (int) pos.x/REAL;
	int z = (int) pos.z/REAL;
	return map[x*H+z];
}

void CThreatMap::update(int frame) {
	int numUnits = ai->cheat->GetEnemyUnits(units, MAX_UNITS);
	for (int i = 0; i < numUnits; i++) {
		const UnitDef *ud = ai->cheat->GetUnitDef(units[i]);
		UnitType      *ut = UT(ud->id);
		float3 upos = ai->cheat->GetUnitPos(units[i]);
		float3 pos(0.0f, 0.0f, 0.0f);
		
		if (ut->cats&ATTACKER && !ai->cheat->UnitBeingBuilt(units[i])) {
			float power = ai->cheat->GetUnitPower(units[i]);
			float range = ud->maxWeaponRange/REAL;
			int   R = (int) round(range)+1;
			for (int x = -R; x <= R; x++) {
				for (int z = -R; z <= R; z++) {
					pos.x = x;
					pos.z = z;
					if (pos.Length2D() <= range) {
						pos.x += upos.x/REAL;
						pos.z += upos.z/REAL;
						int mx = (int) round(pos.x);
						int mz = (int) round(pos.z);
						if (mx >= 0 && mx < W && mz >= 0 && mz < H) {
							map[mx*H+mz] += power;
						}
					}
				}
			}
			totalPower += power;
		}
	}
	reset();
}

void CThreatMap::draw() {
	for (int x = 0; x < W; x++) {
		for (int z = 0; z < H; z++) {
			if (map[x*H+z] > 0.0f) {
				float3 p0(x*REAL, 0.0f, z*REAL);
				float3 p1(p0);
				p1.y += map[x*H+z]/totalPower;
				p1.y *= 200.0f;
				p1.y += 100.0f;
				ai->call->CreateLineFigure(p0, p1, 20, 1, 100, 1);
			}
		}
	}
}

void CThreatMap::reset() {
	for (int i = 0; i < W*H; i++)
		map[i] = 0.0f;
	totalPower = 0.0f;
}
