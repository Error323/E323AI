#include "ThreatMap.h"

CThreatMap::CThreatMap(AIClasses *ai) {
	RES = 8;
	REAL = RES*8.0f;
	this->ai = ai;
	this->W  = ai->call->GetMapWidth() / RES;
	this->H  = ai->call->GetMapHeight() / RES;

	printf("ThreatMap <%d, %d>\n", W, H);
	map   = new float[W*H];
	units = new int[500];
	reset();
}

void CThreatMap::update(int frame) {
	if (frame < 65) return;
	int numUnits = ai->cheat->GetEnemyUnits(units, 500);
	for (int i = 0; i < numUnits; i++) {
		const UnitDef *ud = ai->cheat->GetUnitDef(units[i]);
		float3 upos = ai->cheat->GetUnitPos(units[i]);
		float3 pos(0.0f, 0.0f, 0.0f);
		
		if (!ai->cheat->IsUnitNeutral(units[i]) && !ai->cheat->UnitBeingBuilt(units[i])) {
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
	if (frame % 20 == 0)
		draw();
	reset();
}

void CThreatMap::draw() {
	ai->call->SetFigureColor(1, 1.0f, 0.0f, 0.0f, 0.5f);
	for (int x = 0; x < W; x++) {
		for (int z = 0; z < H; z++) {
			if (map[x*H+z] > 0.0f) {
				float3 p0(x*REAL, 0.0f, z*REAL);
				float3 p1(p0);
				p1.y += map[x*H+z]/totalPower;
				p1.y *= 200.0f;
				p1.y += 100.0f;
				//ai->call->CreateLineFigure(p0, p1, 20, 1, 100, 1);
				ai->call->DrawUnit("arm_peewee", p1, 0.0f, 20, 0, true, false, 0);
			}
		}
	}
}

void CThreatMap::reset() {
	for (int i = 0; i < W*H; i++)
		map[i] = 0.0f;
	totalPower = 0.0f;
}
