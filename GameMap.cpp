#include "GameMap.hpp"

#ifndef _USE_MATH_DEFINES
	#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <iostream>
#include <map>
#include <limits>

#include "CAI.h"
#include "MathUtil.h"
#include "CScopedTimer.h"

#include "headers/HEngine.h"

std::list<float3> GameMap::geospots;
std::list<float3> GameMap::metalfeatures;
std::list<float3> GameMap::energyfeatures;
std::list<float3> GameMap::metalspots;

GameMap::GameMap(AIClasses *ai) {
	this->ai = ai;
	heightVariance = 0.0f;
	waterAmount = 0.0f;
	metalCount = nonMetalCount = 0;
	CalcMapHeightFeatures();
	if (GameMap::metalspots.empty())
		CalcMetalSpots();
}

void GameMap::CalcMetalSpots() {
	PROFILE(metalspots)
	int X = int(ai->cb->GetMapWidth()/4);
	int Z = int(ai->cb->GetMapHeight()/4);
	int R = int(floor(ai->cb->GetExtractorRadius() / 32.0f));
	const unsigned char *metalmapData = ai->cb->GetMetalMap();
	unsigned char *metalmap;
		
	metalmap = new unsigned char[X*Z];

	// Calculate circular stamp
	std::vector<int> circle;
	std::vector<float> sqrtCircle;
	for (int i = -R; i <= R; i++) {
		for (int j = -R; j <= R; j++) {
			float r = sqrt((float)i*i + j*j);
			if (r > R) continue;
			circle.push_back(i);
			circle.push_back(j);
			sqrtCircle.push_back(r);
		}
	}

	// Copy metalmap to mutable metalmap
	std::vector<int> M;
	int minMetal = std::numeric_limits<int>::max();
	for (int z = R; z < Z-R; z++) {
		for (int x = R; x < X-R; x++) {
			metalmap[ID(x,z)] = metalmapData[z*2*X*2+x*2];
			if (metalmap[ID(x,z)] > 0) {
				M.push_back(z);
				M.push_back(x);
				minMetal = std::min<int>(minMetal, metalmap[ID(x,z)]);
				metalCount++;
			}
			else {
				nonMetalCount++;
			}
		}
	}

	if (IsMetalMap()) {
		M.clear();
		for (int z = R; z < Z-R; z+=5) {
			for (int x = R; x < X-R; x+=5) {
				if (metalmap[ID(x,z)] > 0) {
					M.push_back(z);
					M.push_back(x);
				}
			}
		}
	}

	float minimum = (minMetal*M_PI*R*R);
	R++;
	while (true) {
		float highestSaturation = 0.0f;
		int bestX = 0, bestZ = 0;
		bool mexSpotFound = false;

		// Using a greedy approach, find the best metalspot
		for (size_t i = 0; i < M.size(); i+=2) {
			int z = M[i]; int x = M[i+1];
			if (metalmap[ID(x,z)] == 0)
				continue;

			float saturation = 0.0f; float sum = 0.0f;
			for (size_t c = 0; c < circle.size(); c+=2) {
				unsigned char &m = metalmap[ID(x+circle[c+1],z+circle[c])];
				saturation += m * (R-sqrtCircle[c/2]);
				sum += m;
			}
			if (saturation > highestSaturation && sum > minimum) {
				bestX = x; bestZ = z;
				highestSaturation = saturation;
				mexSpotFound = true;
			}
		}

		// No more mex spots
		if (!mexSpotFound) break;

		// "Erase" metal under the bestX bestZ radius
		for (size_t c = 0; c < circle.size(); c+=2) {
			int z = circle[c]+bestZ; int x = circle[c+1]+bestX;
			metalmap[ID(x,z)] = 0;
		}
		
		// Increase to world size
		bestX *= 32.0f; bestZ *= 32.0f;

		// Store metal spot
		float3 metalspot(bestX, ai->cb->GetElevation(bestX,bestZ), bestZ);
		GameMap::metalspots.push_back(metalspot);

		// Debug
		// ai->cb->DrawUnit("armmex", metalspot, 0.0f, 10000, 0, false, false, 0);
	}
	std::string maptype;
	if(IsMetalMap())
		maptype = "speedmetal";
	else if (nonMetalCount == 0)
		maptype = "no metalmap";
	else
		maptype = "normal metalmap"
	LOG_II("GameMap::CalcMetalSpots Maptype: "<<maptype)
	LOG_II("GameMap::CalcMetalSpots found "<<GameMap::metalspots.size()<<" metal spots")

	delete[] metalmap;
}


void GameMap::CalcMapHeightFeatures() {
	// Compute some height features
	int X = int(ai->cb->GetMapWidth());
	int Z = int(ai->cb->GetMapHeight());
	const float *hm = ai->cb->GetHeightMap();

	float fmin = std::numeric_limits<float>::max();
	float fmax = std::numeric_limits<float>::min();
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
				heightVariance += (h/fsum) * pow((h - favg), 2);
		}
	}
	heightVariance = sqrt(heightVariance);

	// Calculate amount of water in [0,1]
	waterAmount = 1.0f - (count / float(total));

	std::string type(IsKbotMap() ? "Kbot" : "Vehicle");
	std::string hoover(IsHooverMap() ? "Enabled" : "Disabled");
	LOG_II("GameMap::CalcMapHeightFeatures Primary lab: "<<type<<", Hoover lab: " << hoover)
}
