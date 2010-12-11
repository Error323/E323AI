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

#include "headers/HEngine.h"

std::list<float3> GameMap::metalspots;
std::list<float3> GameMap::geospots;
std::list<float3> GameMap::metalfeatures;
std::list<float3> GameMap::energyfeatures;

GameMap::GameMap(AIClasses *ai) {
	this->ai = ai;
	heightVariance = 0.0f;
	waterAmount = 0.0f;
	metalCount = nonMetalCount = 0;
	debug = false;
	CalcMapHeightFeatures();
	if (metalspots.empty())
		CalcMetalSpots();
	if (geospots.empty())
		CalcGeoSpots();
}

void GameMap::CalcMetalSpots() {
	const int METAL2REAL = 32.0f;
	int X = int(ai->cb->GetMapWidth() / 4);
	int Z = int(ai->cb->GetMapHeight() / 4);
	int R = int(round(ai->cb->GetExtractorRadius() / METAL2REAL));
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
	float minimum = 10*M_PI*R*R;

	// Copy metalmap to mutable metalmap
	std::vector<int> M;
	avgMetal = 0;
	minMetal = std::numeric_limits<int>::max();
	maxMetal = std::numeric_limits<int>::min();
	for (int z = R; z < Z-R; z++) {
		for (int x = R; x < X-R; x++) {
			int m = 0;
			for (int i = -1; i <= 1; i++)
				for (int j = -1; j <= 1; j++)
					if (metalmapData[(z*2+i)*X*2+(x*2+j)] > 1)
						m = std::max<int>(metalmapData[(z*2+i)*X*2+(x*2+j)], m);

			if (m > 1) {
				metalCount++;
				minMetal = std::min<int>(minMetal, m);
				maxMetal = std::max<int>(maxMetal, m);
				M.push_back(z);
				M.push_back(x);
			}
			else
				nonMetalCount++;
			metalmap[ID(x,z)] = m;
			avgMetal += m;
		}
	}
	avgMetal /= (metalCount + nonMetalCount);

	if (IsMetalMap()) {
		int step = (R+R) > 4 ? (R+R) : 4;
		for (int z = R; z < Z-R; z+=step) {
			for (int x = R; x < X-R; x+=step) {
				if (metalmap[ID(x,z)] > 1) {
					float3 metalspot(x*METAL2REAL, ai->cb->GetElevation(x*METAL2REAL,z*METAL2REAL), z*METAL2REAL);
					metalspots.push_back(metalspot);
					if (debug)
						ai->cb->DrawUnit("armmex", metalspot, 0.0f, 10000, 0, false, false, 0);
				}
			}
		}
	}
	else {
		R++;
		while (true) {
			float highestSaturation = 0.0f, saturation, sum;
			int bestX = 0, bestZ = 0;
			bool mexSpotFound = false;

			// Using a greedy approach, find the best metalspot
			for (size_t i = 0; i < M.size(); i+=2) {
				int z = M[i]; int x = M[i+1];
				if (metalmap[ID(x,z)] == 0)
					continue;

				saturation = 0.0f; sum = 0.0f;
				for (size_t c = 0; c < circle.size(); c+=2) {
					unsigned char &m = metalmap[ID(x+circle[c+1],z+circle[c])];
					saturation += m * (R-sqrtCircle[c/2]);
					sum        += m;
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
			for (size_t c = 0; c < circle.size(); c+=2)
				metalmap[ID(circle[c+1]+bestX,circle[c]+bestZ)] = 0;
			
			// Increase to world size
			bestX *= METAL2REAL; bestZ *= METAL2REAL;

			// Store metal spot
			float3 metalspot(bestX, ai->cb->GetElevation(bestX,bestZ), bestZ);
			metalspots.push_back(metalspot);

			if (debug)
				ai->cb->DrawUnit("armmex", metalspot, 0.0f, 10000, 0, false, false, 0);
		}
	}

	delete[] metalmap;

	std::string maptype;
	if(IsMetalMap())
		maptype = "speedmetal";
	else if (nonMetalCount == 0)
		maptype = "non-metalmap";
	else
		maptype = "normal metalmap";
	
	LOG_II("GameMap::CalcMetalSpots map type: " << maptype)
	LOG_II("GameMap::CalcMetalSpots found " << metalspots.size() << " metal spots")
	LOG_II("GameMap::CalcMetalSpots minMetal(" << minMetal << ") maxMetal(" << maxMetal << ") avgMetal(" << avgMetal << ")")
}

void GameMap::CalcGeoSpots() {
	const int numFeatures = ai->cb->GetFeatures(&ai->unitIDs[0], ai->unitIDs.size());
	for (int i = 0; i < numFeatures; i++) {
		const int fid = ai->unitIDs[i];
		const FeatureDef *fd = ai->cb->GetFeatureDef(fid);
		if (fd && fd->geoThermal) {
			geospots.push_back(ai->cb->GetFeaturePos(fid));
		}
	}
	LOG_II("GameMap::CalcGeoSpots found " << geospots.size() << " geothermal spots");
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
	
	LOG_II("GameMap::CalcMapHeightFeatures Primary lab: " << type << ", Hoover lab: " << hoover)
	LOG_II("GameMap::CalcMapHeightFeatures Water amount: " << waterAmount)
}
