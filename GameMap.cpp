#include "GameMap.hpp"

#ifndef _USE_MATH_DEFINES
	#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <iostream>
#include <map>

#include "CAI.h"
#include "CGroup.h"
#include "MathUtil.h"
#include "CThreatMap.h"
#include "CUnitTable.h"
#include "CUnit.h"

#include "headers/HEngine.h"

#define METAL_THRESHOLD 24

bool  GameMap::isInitialized = false;
float GameMap::heightVariance = 0.0f;
float GameMap::waterAmount = 0.0f;
float GameMap::metalAmount = 0.0f;

std::list<float3> GameMap::geospots;
std::list<float3> GameMap::metalfeatures;
std::list<float3> GameMap::energyfeatures;
std::list<float3> GameMap::metalspots;

GameMap::GameMap(AIClasses *ai) {
	this->ai = ai;

	if (GameMap::isInitialized)
		return;

	CalcMapHeightFeatures();
	CalcMetalSpots();
	GameMap::isInitialized = true;
}

float3 GameMap::GetClosestOpenMetalSpot(CGroup* group) {
	float bestDist = FLT_MAX;
	float3 bestSpot = ZeroVector;
	float3 gpos = group->pos();
	std::list<float3>::iterator i;
	for (i = GameMap::metalspots.begin(); i != GameMap::metalspots.end(); i++) {
		int numUnits = ai->cb->GetFriendlyUnits(&ai->unitIDs[0], *i, ai->cb->GetExtractorRadius() * 1.1f, 50);
		bool taken = false;
		for (int j = 0; j < numUnits; j++) {
			if (ai->cb->GetUnitDef(ai->unitIDs[j])->extractsMetal > EPSILON) {
				taken = true;
				break;
			}
		}
		if (!taken && ai->threatmap->getThreat(*i, 0.0f) <= 1.0f) {
			float dist = (gpos - *i).Length2D();
			if (bestDist > dist) {
				bestDist = dist;
				bestSpot = *i;
			}
		}
	}
	return bestSpot;
}

// SLOGIC: this method has nothing to do with GamMap class, it is pure CEconomy
// related stuff where we should track already built MExes. Also it somewhat 
// heavy
int GameMap::GetClosestUpgradableMetalSpot(CGroup* group) {
	float bestDist = FLT_MAX;
	int mexUpgrade = -1;
	float3 gpos = group->pos();
	std::list<float3>::iterator i;
	for (i = GameMap::metalspots.begin(); i != GameMap::metalspots.end(); i++) {
		CUnit *unit;
		bool taken = false;
		int numUnits = ai->cb->GetFriendlyUnits(&ai->unitIDs[0], *i, ai->cb->GetExtractorRadius(), 50);
		for (int j = 0; j < numUnits; j++) {
			unit = ai->unittable->getUnit(ai->unitIDs[j]);
			bool isMex = unit->def->extractsMetal > EPSILON;
			bool isUpgradable = (unit->techlvl&TECH1 > 0);
			if (isMex && isUpgradable) {
				taken = true;
				break;
			}
		}
		if (taken && ai->threatmap->getThreat(*i, 0.0f) <= 1.0f ) {
			float dist = (gpos - *i).Length2D();
			if (bestDist > dist) {
				bestDist = dist;
				mexUpgrade = unit->key;
			}
		}
	}
	return mexUpgrade;
}

void GameMap::CalcMetalSpots() {
	int X = int(ai->cb->GetMapWidth()/4);
	int Z = int(ai->cb->GetMapHeight()/4);
	int R = int(round(ai->cb->GetExtractorRadius() / 32.0f));
	const unsigned char *metalmapData = ai->cb->GetMetalMap();
	unsigned char *metalmap;
		
	metalmap = new unsigned char[X*Z];

	// Calculate circular stamp
	std::vector<int> circle;
	for (int i = -R; i <= R; i++) {
		for (int j = -R; j <= R; j++) {
			float r = sqrt((float)i*i + j*j);
			if (r > R) continue;
			circle.push_back(i);
			circle.push_back(j);
		}
	}

	// Copy metalmap to mutable metalmap
	std::vector<int> M;
	for (int z = 0; z < Z; z++) {
		for (int x = 0; x < X; x++) {
			float sum = 0.0f;
			for (int i = -1; i <= 1; i++) {
				for (int j = -1; j <= 1; j++) {
					int zz = z*2+i; int xx = x*2+j;
					if (zz < 0 || zz > (Z*2-1) || xx < 0 || xx > (X*2-1))
						continue;
					sum += metalmapData[zz*(X*2)+xx];
				}
			}

			metalmap[ID(x,z)] = int(round(sum/9.0f));
			if (metalmap[ID(x,z)] >= METAL_THRESHOLD) {
				M.push_back(z);
				M.push_back(x);
			}
		}
	}

	while (true) {
		float highestSaturation = 0.0f;
		int bestX, bestZ;
		bool mexSpotFound = false;

		// Using a greedy approach, find the best metalspot
		for (size_t i = 0; i < M.size(); i+=2) {
			int z = M[i]; int x = M[i+1];
			float saturation = 0.0f; float sum = 0.0f;
			for (size_t c = 0; c < circle.size(); c+=2) {
				int zz = circle[c]+z; int xx = circle[c+1]+x;
				if (xx < 0 || xx > X-1 || zz < 0 || zz > Z-1)
					continue;
				float r = sqrt((float)circle[c]*circle[c] + circle[c+1]*circle[c+1]);
				saturation += metalmap[ID(xx, zz)] * (1.0f / (r+1.0f));
				sum += metalmap[ID(xx,zz)];
			}
			if (saturation > highestSaturation && sum > (METAL_THRESHOLD*M_PI*pow((0.7*R),2))) {
				bestX = x; bestZ = z;
				highestSaturation = saturation;
				mexSpotFound = true;
			}
		}

		// No more mex spots
		if (!mexSpotFound) break;

		metalAmount += metalmap[bestZ*Z+bestX];

		// "Erase" metal under the bestX bestZ radius
		for (size_t c = 0; c < circle.size(); c+=2) {
			int z = circle[c]+bestZ; int x = circle[c+1]+bestX;
			if (x < 0 || x > X-1 || z < 0 || z > Z-1)
				continue;
			metalmap[ID(x,z)] = 0;
		}
		
		// Increase to world size
		bestX *= 32.0f; bestZ *= 32.0f;

		// Store metal spot
		float3 metalspot(bestX, ai->cb->GetElevation(bestX,bestZ), bestZ);
		GameMap::metalspots.push_back(metalspot);

		// Debug
		//ai->cb->DrawUnit("armmex", metalspot, 0.0f, 10000, 0, false, false, 0);
	}

	delete[] metalmap;
}


void GameMap::CalcMapHeightFeatures() {
	// Compute some height features
	int X = int(ai->cb->GetMapWidth());
	int Z = int(ai->cb->GetMapHeight());
	const float *hm = ai->cb->GetHeightMap();

	float fmin =  FLT_MAX;
	float fmax = -FLT_MAX;
	float fsum = 0.0f;

	unsigned count = 0;
	unsigned total = 0;
	// Calculate the sum, min and max
	for (int z = 0; z < Z; z++) {
		for (int x = 0; x < X; x++) {
			float h = hm[ID(x,z)];
			if (h >= 0.0f) {
				fsum += h;
				fmin = std::min<float>(fmin,h);
				fmax = std::max<float>(fmax,h);
				count++;
			}
			total++;
		}
	}

	float favg = fsum / count;

	// Calculate the variance
	for (int z = 0; z < Z; z++) {
		for (int x = 0; x < X; x++) {
			float h = hm[ID(x,z)];
			if (h >= 0.0f) 
				heightVariance += (h/fsum) * std::pow((h - favg), 2.0f);
		}
	}

	// Calculate amount of water in [0,1]
	waterAmount = 1.0f - (count / float(total));
}
