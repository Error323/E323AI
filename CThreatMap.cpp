#include "CThreatMap.h"

#include "CAI.h"
#include "CUnitTable.h"
#include "CIntel.h"
#include "CUnit.h"

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

float CThreatMap::getThreat(float3 &center, float radius) {
	int i = center.z / REAL;
	int j = center.x / REAL;
	if (radius == 0.0f)
		return map[ID(j,i)];
	int R = ceil(radius / REAL);
	float power = 0.0f;
	for (int z = -R; z <= R; z++) {
		int zz = z+i;
		if (zz > Z-1 || zz < 0)
			continue;
		for (int x = -R; x <= R; x++) {
			int xx = x+j;
			if (xx < X-1 && xx >= 0)
				power += map[ID(xx,zz)];
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

	/* Add enemy threats */
	for (int i = 0; i < numUnits; i++) {
		const UnitDef  *ud = ai->cbc->GetUnitDef(units[i]);
		const UnitType *ut = UT(ud->id);
		
		/* Don't let air be part of the threatmap */
		if (ut->cats&ATTACKER && ut->cats&AIR && ut->cats&MOBILE)
			continue;

		/* Ignore paralyzed units */
		if (ai->cbc->IsUnitParalyzed(units[i]))
			continue;

		if (ut->cats&ATTACKER && !ai->cbc->UnitBeingBuilt(units[i])) {
			const float3  upos = ai->cbc->GetUnitPos(units[i]);
			const float uRealX = upos.x/REAL;
			const float uRealZ = upos.z/REAL;
			const float  range = (ud->maxWeaponRange+100.0f)/REAL;
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
}

float CThreatMap::gauss(float x, float sigma, float mu) {
	float a = 1.0f / (sigma * sqrt(2*M_PI));
	float b = exp( -( pow(x-mu, 2) / (2*(pow(sigma,2))) ) );
	return a * b;
}

void CThreatMap::draw() {
	for (int z = 0; z < Z; z++) {
		for (int x = 0; x < X; x++) {
			if (map[ID(x,z)] > 1.0f+EPSILON) {
				float3 p0(x*REAL, ai->cb->GetElevation(x*REAL,z*REAL), z*REAL);
				float3 p1(p0);
				p1.y += (map[ID(x,z)]/totalPower) * 300.0f;
				ai->cb->CreateLineFigure(p0, p1, 4, 1, DRAW_TIME, 1);
			}
		}
	}
}
