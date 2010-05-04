#include "CThreatMap.h"

#include <math.h>

#include "CAI.h"
#include "CUnitTable.h"
#include "CIntel.h"
#include "CUnit.h"
#include "CGroup.h"
#include "MathUtil.h"

CThreatMap::CThreatMap(AIClasses *ai) {
	this->ai = ai;
	X = ai->cb->GetMapWidth() / (HEIGHT2SLOPE*I_MAP_RES);
	Z = ai->cb->GetMapHeight() / (HEIGHT2SLOPE*I_MAP_RES);
	REAL = HEIGHT2SLOPE * HEIGHT2REAL * I_MAP_RES;
	
	units = &ai->unitIDs[0]; // save about 4x32KB of memory

	maps[TMT_AIR] = new float[X*Z];
	maps[TMT_SURFACE] = new float[X*Z];
	
	reset();
}

CThreatMap::~CThreatMap() {
	std::map<ThreatMapType,float*>::iterator i;
	for (i = maps.begin(); i != maps.end(); i++)
		delete[] i->second;
}

void CThreatMap::reset() {
	float *map;
	std::map<ThreatMapType,float*>::iterator i;
	for (i = maps.begin(); i != maps.end(); i++) {
		maxPower[i->first] = 0.0f;
		map = i->second;
		for (int i = 0; i < X*Z; i++)
			map[i] = 1.0f;
	}
}

float *CThreatMap::getMap(ThreatMapType type) {
	std::map<ThreatMapType,float*>::iterator i = maps.find(type);
	if (i == maps.end())
		return NULL;
	return i->second;
}

float CThreatMap::getThreat(float3 &center, float radius, ThreatMapType type) {
	int i = center.z / REAL;
	int j = center.x / REAL;
	float *map = maps[type];
	
	if (radius < EPS)
		return map[ID(j,i)];
	
	int R = ceil(radius / REAL);
	float power = 0.0f;
	for (int z = -R; z <= R; z++) {
		int zz = z + i;
		
		if (zz > Z-1 || zz < 0)
			continue;
		
		for (int x = -R; x <= R; x++) {
			int xx = x+j;
			if (xx < X-1 && xx >= 0)
				power += map[ID(xx,zz)];
		}
	}
	
	return power/(2.0f*R*M_PI);
}

float CThreatMap::getThreat(float3 &center, float radius, CGroup *group) {
	unsigned int cats = group->cats;
	ThreatMapType type;
	
	if (cats&LAND)
		type = TMT_SURFACE;
	else
		type = TMT_AIR;
	
	return getThreat(center, radius, type);
}

void CThreatMap::update(int frame) {
	std::list<ThreatMapType> activeTypes;
	std::list<ThreatMapType>::iterator tmi;

	reset();

	int numUnits = ai->cbc->GetEnemyUnits(units, MAX_UNITS_AI);
	if (numUnits > MAX_UNITS_AI)
		LOG_WW("CThreatMap::update " << numUnits << " > " << MAX_UNITS_AI)

	/* Add enemy threats */
	for (int i = 0; i < numUnits; i++) {
		const int uid = units[i];
		const UnitDef  *ud = ai->cbc->GetUnitDef(uid);
		
		if (ud == NULL)
			continue;

		const UnitType *ut = UT(ud->id);
		
		if (!(ut->cats&ATTACKER) || ai->cbc->IsUnitParalyzed(uid) || ai->cbc->UnitBeingBuilt(uid))
			continue;
		
		if ((ut->cats&AIR) && !(ut->cats&ASSAULT))
			continue;

		// FIXME: think smth cleverer
		if (ud->maxWeaponRange > MAX_WEAPON_RANGE_FOR_TM)
			continue;

		activeTypes.clear();

		if (ut->cats&ANTIAIR)
			activeTypes.push_back(TMT_AIR);
		// NOTE: assuming DEFENSE unit can shoot ground units
		if (((ut->cats&AIR) && (ut->cats&ASSAULT)) || (ut->cats&LAND && ((ut->cats&DEFENSE) || !(ut->cats&ANTIAIR))))
			activeTypes.push_back(TMT_SURFACE);
		/*
		if (ut->cats&TORPEDO)
			activeTypes.push_back(TMT_UNDERWATER);
		*/
		const float3  upos = ai->cbc->GetUnitPos(uid);
		const float uRealX = upos.x/REAL;
		const float uRealZ = upos.z/REAL;
		const float  range = (ud->maxWeaponRange + 100.0f)/REAL;
		float       powerT = ai->cbc->GetUnitPower(uid);
		const float  power = ut->cats&COMMANDER ? powerT/20.0f : powerT;
		float3 pos(0.0f, 0.0f, 0.0f);

		const int R = (int) ceil(range);
		for (int z = -R; z <= R; z++) {
			for (int x = -R; x <= R; x++) {
				pos.x = x;
				pos.z = z;
				if (pos.Length2D() <= range) {
					pos.x += uRealX;
					pos.z += uRealZ;
					const unsigned int mx = (unsigned int) round(pos.x);
					const unsigned int mz = (unsigned int) round(pos.z);
					if (mx < X && mz < Z) {
						for (tmi = activeTypes.begin(); tmi != activeTypes.end(); tmi++) {
							maps[*tmi][ID(mx,mz)] += power;
						}
					}
				}
			}
		}
		
		for (tmi = activeTypes.begin(); tmi != activeTypes.end(); tmi++) {
			maxPower[*tmi] = std::max<float>(power, maxPower[*tmi]);
		}
	}
}

float CThreatMap::gauss(float x, float sigma, float mu) {
	float a = 1.0f / (sigma * sqrt(2*M_PI));
	float b = exp( -( pow(x-mu, 2) / (2*(pow(sigma,2))) ) );
	return a * b;
}

void CThreatMap::draw(ThreatMapType type) {
	float *map = maps[type];
	float total = maxPower[type];
	
	for (int z = 0; z < Z; z++) {
		for (int x = 0; x < X; x++) {
			if (map[ID(x,z)] > 1.0f + EPSILON) {
				float3 p0(x*REAL, ai->cb->GetElevation(x*REAL,z*REAL), z*REAL);
				float3 p1(p0);
				p1.y += (map[ID(x,z)]/total) * 300.0f;
				ai->cb->CreateLineFigure(p0, p1, 4, 10.0, DRAW_TIME, 5);
			}
		}
	}
	ai->cb->SetFigureColor(5, 1.0f, 0.0f, 0.0f, 1.0f);
}
