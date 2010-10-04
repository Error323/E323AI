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
	
	X = ai->cb->GetMapWidth() / (HEIGHT2SLOPE*I_MAP_RES);
	Z = ai->cb->GetMapHeight() / (HEIGHT2SLOPE*I_MAP_RES);
	REAL = HEIGHT2SLOPE * HEIGHT2REAL * I_MAP_RES;
	
	lastUpdateFrame = 0;

	maps[TMT_AIR] = new float[X*Z];
	maps[TMT_SURFACE] = new float[X*Z];
	maps[TMT_SURFACE_WATER] = new float[X*Z];
	maps[TMT_UNDERWATER] = new float[X*Z];

#if !defined(BUILDING_AI_FOR_SPRING_0_81_2)
	handles[TMT_AIR] = ai->cb->DebugDrawerAddOverlayTexture(maps[TMT_AIR], X, Z);
	ai->cb->DebugDrawerSetOverlayTexturePos(handles[TMT_AIR], -0.95f, -0.95f);
	ai->cb->DebugDrawerSetOverlayTextureSize(handles[TMT_AIR], 0.4f, 0.4f);
	ai->cb->DebugDrawerSetOverlayTextureLabel(handles[TMT_AIR], "Air ThreatMap");

	handles[TMT_SURFACE] = ai->cb->DebugDrawerAddOverlayTexture(maps[TMT_SURFACE], X, Z);
	ai->cb->DebugDrawerSetOverlayTexturePos(handles[TMT_SURFACE], -0.95f, -0.5f);
	ai->cb->DebugDrawerSetOverlayTextureSize(handles[TMT_SURFACE], 0.4f, 0.4f);
	ai->cb->DebugDrawerSetOverlayTextureLabel(handles[TMT_SURFACE], "Surface ThreatMap");

	handles[TMT_SURFACE_WATER] = ai->cb->DebugDrawerAddOverlayTexture(maps[TMT_SURFACE_WATER], X, Z);
	ai->cb->DebugDrawerSetOverlayTexturePos(handles[TMT_SURFACE_WATER], -0.95f, -0.05f);
	ai->cb->DebugDrawerSetOverlayTextureSize(handles[TMT_SURFACE_WATER], 0.4f, 0.4f);
	ai->cb->DebugDrawerSetOverlayTextureLabel(handles[TMT_SURFACE_WATER], "Surface + Underwater ThreatMap");

	handles[TMT_UNDERWATER] = ai->cb->DebugDrawerAddOverlayTexture(maps[TMT_UNDERWATER], X, Z);
	ai->cb->DebugDrawerSetOverlayTexturePos(handles[TMT_UNDERWATER],  -0.95f, 0.45f);
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
	std::map<ThreatMapType, float*>::iterator i;
	// NOTE: no threat value equals to ONE, not ZERO!
	for (i = maps.begin(); i != maps.end(); ++i) {
		maxPower[i->first] = 1.0f;
		std::fill_n(i->second, X*Z, 1.0f);
	}
}

float *CThreatMap::getMap(ThreatMapType type) {
	std::map<ThreatMapType,float*>::iterator i = maps.find(type);
	if (i == maps.end())
		return NULL;
	return i->second;
}

float CThreatMap::getThreat(float3 &center, float radius, ThreatMapType type) {
	if (type == TMT_NONE)
		return 1.0f;

	int i = center.z / REAL;
	int j = center.x / REAL;
	const float* map = maps[type];
	
	if (radius < EPS)
		return map[ID(j, i)];
	
	int sectorsProcessed = 0;
	int R = ceil(radius / REAL);
	float power = 0.0f;
	for (int z = -R; z <= R; z++) {
		int zz = z + i;
		
		if (zz > Z-1 || zz < 0)
			continue;
		
		for (int x = -R; x <= R; x++) {
			int xx = x+j;
			if (xx < X-1 && xx >= 0) {
				power += map[ID(xx,zz)];
				sectorsProcessed++;
			}
		}
	}

	// calculate number of sectors in R x R...
	R = 2 * R + 1;
	R *= R;

	// fixing area threat for map edges...
	if (sectorsProcessed < R)
		power += (R - sectorsProcessed);
	
	return power / R;
}

float CThreatMap::getThreat(float3 &center, float radius, CGroup *group) {
	ThreatMapType type = TMT_NONE;
	
	// TODO: deal with LAND units when they are temporary under water
	// TODO: deal with units which have tags LAND|SUB
	// TODO: dealth with units which have tags AIR|SUB
	
	// NOTE: we do not support walking ships

	if ((group->cats&LAND).any())
		type = TMT_SURFACE; // hovers go here too because they can't be hit by torpedo
	else if ((group->cats&AIR).any())
		type = TMT_AIR;
	else if ((group->cats&SEA).any())
		type = TMT_SURFACE_WATER;
	else if ((group->cats&SUB).any())
		type = TMT_UNDERWATER;
	return getThreat(center, radius, type);
}

void CThreatMap::update(int frame) {
	static const unitCategory catsCanShootGround = ASSAULT|SNIPER|ARTILLERY|SCOUTER|PARALYZER;
	
	if ((frame - lastUpdateFrame) < MULTIPLEXER)
		return;

	const bool isWaterMap = ai->gamemap->IsWaterMap();
	std::list<ThreatMapType> activeTypes;
	std::list<ThreatMapType>::iterator itMapType;

	reset();

	int numUnits = ai->cbc->GetEnemyUnits(&ai->unitIDs[0], MAX_UNITS_AI);

	/* Add enemy threats */
	for (int i = 0; i < numUnits; i++) {
		const int uid = ai->unitIDs[i];
		const UnitDef *ud = ai->cbc->GetUnitDef(uid);
		
		if (ud == NULL)
			continue;

		const UnitType *ut = UT(ud->id);
		const unitCategory ecats = ut->cats;
		
		if ((ecats&ATTACKER).none() || ai->cbc->IsUnitParalyzed(uid)
		|| ai->cbc->UnitBeingBuilt(uid))
			continue;
		
		if ((ecats&AIR).any() && (ecats&ASSAULT).none())
			continue; // ignore air fighters & bombers

		const float3 upos = ai->cbc->GetUnitPos(uid);

		// NOTE: for now i don't know what exact "y" position for naval units,
		// so ignoring them for sure
		if (upos.y < 0.0f && (ecats&(SEA|SUB|TORPEDO)).none())
			continue; // ignore units which can't shoot under the water

		// FIXME: think smth cleverer
		if (ud->maxWeaponRange > MAX_WEAPON_RANGE_FOR_TM)
			continue;

		activeTypes.clear();

		if ((ecats&ANTIAIR).any()) {
			activeTypes.push_back(TMT_AIR);
		}
		
		// TODO: remove DEFENSE hack after fixing all config files
		if (((catsCanShootGround&ecats).any() && (ecats&SUB).none())
		||  ((ecats&DEFENSE).any() && (ecats&ANTIAIR).none())) {
			activeTypes.push_back(TMT_SURFACE);
			if (isWaterMap)
				activeTypes.push_back(TMT_SURFACE_WATER);
		}
		
		// NOTE: TMT_SURFACE_WATER map can exist twice in a list which is ok
		if (isWaterMap && (ecats&TORPEDO).any()) {
			activeTypes.push_back(TMT_UNDERWATER);
			activeTypes.push_back(TMT_SURFACE_WATER);
		}

		if (activeTypes.empty())
			continue;

		const float uRealX = upos.x/REAL;
		const float uRealZ = upos.z/REAL;
		const float  range = (ud->maxWeaponRange + 100.0f) / REAL;
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
					const unsigned int mx = (unsigned int) round(pos.x);
					const unsigned int mz = (unsigned int) round(pos.z);
					if (mx < X && mz < Z) {
						for (itMapType = activeTypes.begin(); itMapType != activeTypes.end(); ++itMapType) {
							maps[*itMapType][ID(mx, mz)] += power;
						}
					}
				}
			}
		}
		
		for (itMapType = activeTypes.begin(); itMapType != activeTypes.end(); ++itMapType) {
			maxPower[*itMapType] = std::max<float>(power, maxPower[*itMapType]);
		}
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

void CThreatMap::visualizeMap(ThreatMapType type) {
	float *map = maps[type];
	float total = maxPower[type];
	
	for (int z = 0; z < Z; z++) {
		for (int x = 0; x < X; x++) {
			if (map[ID(x,z)] > 1.0f + EPSILON) {
				float3 p0(x*REAL, ai->cb->GetElevation(x*REAL,z*REAL), z*REAL);
				float3 p1(p0);
				p1.y += (map[ID(x,z)]/total) * 300.0f;
				ai->cb->CreateLineFigure(p0, p1, 4, 10.0, MULTIPLEXER, 5);
			}
		}
	}
	ai->cb->SetFigureColor(5, 1.0f, 0.0f, 0.0f, 1.0f);
}

bool CThreatMap::switchDebugMode() {
	int i = drawMap;
	i++;
	drawMap = (ThreatMapType)i;
	if (drawMap >= TMT_LAST)
		drawMap = TMT_NONE;
	return drawMap != TMT_NONE;
}