#include "CThreatMap.h"

#include <math.h>

#include "CAI.h"
#include "CUnitTable.h"
#include "CIntel.h"
#include "CUnit.h"
#include "CGroup.h"
#include "MathUtil.h"
#include "GameMap.hpp"

CThreatMap::CThreatMap(AIClasses *ai) {
	this->ai = ai;
	
	// NOTE: X & Z are in pathgraph resolution
	X = int(ai->cb->GetMapWidth() / (PATH2SLOPE * SLOPE2HEIGHT));
	Z = int(ai->cb->GetMapHeight() / (PATH2SLOPE * SLOPE2HEIGHT));
	
	lastUpdateFrame = 0;

	maps[TMT_AIR] = new float[X*Z];
	maps[TMT_SURFACE] = new float[X*Z];
	maps[TMT_UNDERWATER] = new float[X*Z];

#if !defined(BUILDING_AI_FOR_SPRING_0_81_2)
	handles[TMT_AIR] = ai->cb->DebugDrawerAddOverlayTexture(maps[TMT_AIR], X, Z);
	ai->cb->DebugDrawerSetOverlayTexturePos(handles[TMT_AIR], -0.95f, -0.8f);
	ai->cb->DebugDrawerSetOverlayTextureSize(handles[TMT_AIR], 0.4f, 0.4f);
	ai->cb->DebugDrawerSetOverlayTextureLabel(handles[TMT_AIR], "Air ThreatMap");

	handles[TMT_SURFACE] = ai->cb->DebugDrawerAddOverlayTexture(maps[TMT_SURFACE], X, Z);
	ai->cb->DebugDrawerSetOverlayTexturePos(handles[TMT_SURFACE], -0.95f, -0.2f);
	ai->cb->DebugDrawerSetOverlayTextureSize(handles[TMT_SURFACE], 0.4f, 0.4f);
	ai->cb->DebugDrawerSetOverlayTextureLabel(handles[TMT_SURFACE], "Surface ThreatMap");

	handles[TMT_UNDERWATER] = ai->cb->DebugDrawerAddOverlayTexture(maps[TMT_UNDERWATER], X, Z);
	ai->cb->DebugDrawerSetOverlayTexturePos(handles[TMT_UNDERWATER],  -0.95f, 0.4f);
	ai->cb->DebugDrawerSetOverlayTextureSize(handles[TMT_UNDERWATER], 0.4f, 0.4f);
	ai->cb->DebugDrawerSetOverlayTextureLabel(handles[TMT_UNDERWATER], "Underwater ThreatMap");
#endif
	
	drawMap = TMT_NONE;

	reset();
}

CThreatMap::~CThreatMap() {
	std::map<ThreatMapType, float*>::iterator itMap;
	for (itMap = maps.begin(); itMap != maps.end(); ++itMap)
		delete[] itMap->second;
#if !defined(BUILDING_AI_FOR_SPRING_0_81_2)
	std::map<ThreatMapType, int>::iterator itHandler;
	for (itHandler = handles.begin(); itHandler != handles.end(); ++itHandler)
		ai->cb->DebugDrawerDelOverlayTexture(itHandler->second);
#endif
}

void CThreatMap::reset() {
	std::map<ThreatMapType, float*>::iterator it;
	// NOTE: no threat value equals to ONE, not ZERO!
	for (it = maps.begin(); it != maps.end(); ++it) {
		maxPower[it->first] = 1.0f;
		std::fill_n(it->second, X*Z, 1.0f);
	}
}

const float* CThreatMap::getMap(ThreatMapType type) {
	std::map<ThreatMapType,float*>::const_iterator i = maps.find(type);
	if (i == maps.end())
		return NULL;
	return i->second;
}

float CThreatMap::getThreat(float3 center, float radius, ThreatMapType type) {
	if (type == TMT_NONE)
		return 1.0f;

	int i = int(round(center.z / PATH2REAL));
	int j = int(round(center.x / PATH2REAL));
	const float* tmData = maps[type];

	assert(tmData != NULL);
	
	if (radius < EPS) {
		checkInBounds(j, i);
		return tmData[ID(j, i)];
	}
	
	int sectorsProcessed = 0;
	int R = int(round(radius / PATH2REAL));
	float power = 0.0f;
	
	// FIXME: search within circle instead of square
	for (int z = -R; z <= R; z++) {
		int zz = i + z;
		
		if (zz >= 0 && zz < Z) {
			for (int x = -R; x <= R; x++) {
				int xx = j + x;
				if (xx >= 0 && xx < X) {
					power += tmData[ID(xx, zz)];
					sectorsProcessed++;
				}
			}
		}
	}

	// calculate total number of sectors within requested area...
	R = R + R + 1;
	R *= R;

	// fixing area threat for map edges...
	if (sectorsProcessed < R)
		power += (R - sectorsProcessed);
	
	return (power / R);
}

float CThreatMap::getThreat(const float3& center, float radius, CGroup* group) {
	float temp, result = 1.0f;
	
	// TODO: deal with LAND units when they are temporary under water
	// TODO: deal with units which have tags LAND|SUB
	// TODO: dealth with units which have tags AIR|SUB
	
	if ((group->cats&AIR).any()) {
		temp = getThreat(center, radius, TMT_AIR);
		if (temp > 1.0f) result += temp - 1.0f;
	}
	if ((group->cats&SUB).any()) {
		temp = getThreat(center, radius, TMT_UNDERWATER);
		if (temp > 1.0f) result += temp - 1.0f;
	}
	// NOTE: hovers (LAND|SEA) can't be hit by underwater weapons that is why 
	// LAND tag check is a priority over SEA below
	if ((group->cats&LAND).any()) {
		temp = getThreat(center, radius, TMT_SURFACE);
		if (temp > 1.0f) result += temp - 1.0f;
	}
	else if ((group->cats&SEA).any() && (group->cats&SUB).none()) {
		temp = getThreat(center, radius, TMT_UNDERWATER);
		if (temp > 1.0f) result += temp - 1.0f;
	}
	
	return result;
}

void CThreatMap::update(int frame) {
	static const unitCategory catsCanShootGround = ASSAULT|SNIPER|ARTILLERY|SCOUTER/*|PARALYZER*/;
	
	if ((frame - lastUpdateFrame) < MULTIPLEXER)
		return;

	const bool isWaterMap = !ai->gamemap->IsWaterFreeMap();
	std::list<ThreatMapType> activeTypes;
	std::list<ThreatMapType>::const_iterator itMapType;

	reset();

	int numUnits = ai->cbc->GetEnemyUnits(&ai->unitIDs[0], MAX_UNITS_AI);

	/* Add enemy threats */
	for (int i = 0; i < numUnits; i++) {
		const int uid = ai->unitIDs[i];
		const UnitDef* ud = ai->cbc->GetUnitDef(uid);
		
		if (ud == NULL)
			continue;

		const UnitType* ut = UT(ud->id);
		const unitCategory ecats = ut->cats;
		
		if ((ecats&ATTACKER).none() || ai->cbc->IsUnitParalyzed(uid)
		|| ai->cbc->UnitBeingBuilt(uid))
			continue; // ignore unamred, paralyzed & being built units
		
		if ((ecats&AIR).any() && (ecats&ASSAULT).none())
			continue; // ignore air fighters & bombers

		// FIXME: using maxWeaponRange below (twice) is WRONG; we need
		// to calculate different max. ranges per each threatmap layer

		// FIXME: think smth cleverer
		if (ud->maxWeaponRange > MAX_WEAPON_RANGE_FOR_TM)
			continue; // ignore units with extra large range
		
		const float3 upos = ai->cbc->GetUnitPos(uid);

		activeTypes.clear();

		if ((ecats&ANTIAIR).any() && upos.y >= 0.0f) {
			activeTypes.push_back(TMT_AIR);
		}
		
		if (((ecats&SEA).any() || upos.y >= 0.0f)
		&&  ((ecats&ANTIAIR).none() || (catsCanShootGround&ecats).any())) {
			activeTypes.push_back(TMT_SURFACE);
		}
		
		if (isWaterMap && (ecats&TORPEDO).any()) {
			activeTypes.push_back(TMT_UNDERWATER);
		}

		if (activeTypes.empty())
			continue;

		const float uRealX = upos.x / PATH2REAL;
		const float uRealZ = upos.z / PATH2REAL;
		const float  range = (ud->maxWeaponRange + 100.0f) / PATH2REAL;
		float       powerT = ai->cbc->GetUnitPower(uid);
		const float  power = (ecats&COMMANDER).any() ? powerT/20.0f : powerT;
		float3 pos(0.0f, 0.0f, 0.0f);

		const int R = (int) ceil(range);
		for (int z = -R; z <= R; z++) {
			for (int x = -R; x <= R; x++) {
				pos.x = x;
				pos.z = z;
				if (pos.Length2D() <= range) {
					pos.x += uRealX;
					pos.z += uRealZ;
					const int mx = int(round(pos.x));
					const int mz = int(round(pos.z));
					if (isInBounds(mx, mz)) {
						for (itMapType = activeTypes.begin(); itMapType != activeTypes.end(); ++itMapType) {
							int id = ID(mx, mz);
							maps[*itMapType][id] += power;
							maxPower[*itMapType] = std::max(maps[*itMapType][id], maxPower[*itMapType]);
						}
					}
				}
			}
		}
		
		/*
		for (itMapType = activeTypes.begin(); itMapType != activeTypes.end(); ++itMapType) {
			maxPower[*itMapType] = std::max<float>(power, maxPower[*itMapType]);
		}
		*/
	}

#if !defined(BUILDING_AI_FOR_SPRING_0_81_2)
	if (ai->cb->IsDebugDrawerEnabled()) {
		std::map<ThreatMapType, int>::iterator i;
		for (i = handles.begin(); i != handles.end(); ++i) {
			float power = maxPower[i->first];
			// normalize the data...
			for (int j = 0, N = X*Z; j < N; j++)
				maps[i->first][j] /= power;
			// update texturemap
			ai->cb->DebugDrawerUpdateOverlayTexture(i->second, maps[i->first], 0, 0, X, Z);
			// restore the original data...
			for (int j = 0, N = X*Z; j < N; j++)
				maps[i->first][j] *= power;
		}
	}
#endif

	if (drawMap != TMT_NONE)
		visualizeMap(drawMap);

	lastUpdateFrame = frame;
}

float CThreatMap::gauss(float x, float sigma, float mu) {
	float a = 1.0f / (sigma * sqrt(2*M_PI));
	float b = exp( -( pow(x-mu, 2) / (2*(pow(sigma,2))) ) );
	return a * b;
}

void CThreatMap::checkInBounds(int& x, int& z) {
	if (x < 0)
		x = 0;
	else if (x >= X)
		x = X - 1;

	if (z < 0)
		z = 0;
	else if (z >= Z)
		z = Z - 1;
}

void CThreatMap::visualizeMap(ThreatMapType type) {
	static const int figureID = 5;

	std::map<ThreatMapType,float*>::const_iterator i = maps.find(type);
	
	if (i == maps.end())
		return;	

	const float* map = i->second;
	float total = maxPower[type];
	
	for (int z = 0; z < Z; z++) {
		for (int x = 0; x < X; x++) {
			if (map[ID(x,z)] > 1.0f + EPS) {
				float3 p0(x * PATH2REAL, ai->cb->GetElevation(x*PATH2REAL,z*PATH2REAL), z*PATH2REAL);
				float3 p1(p0);
				p1.y += (map[ID(x,z)]/total) * 250.0f;
				ai->cb->CreateLineFigure(p0, p1, 4, 10.0f, MULTIPLEXER, figureID);
			}
		}
	}
	ai->cb->SetFigureColor(figureID, 1.0f, 0.0f, 0.0f, 1.0f);
}

bool CThreatMap::switchDebugMode() {
	int i = drawMap;
	i++;
	drawMap = (ThreatMapType)i;
	if (drawMap >= TMT_LAST)
		drawMap = TMT_NONE;
	return drawMap != TMT_NONE;
}
