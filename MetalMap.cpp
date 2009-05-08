#include "MetalMap.h"

CMetalMap::CMetalMap(AIClasses *ai) {
	this->ai = ai;
	threshold = 64;

	X = ai->call->GetMapWidth() / 2;
	Z = ai->call->GetMapHeight() / 2;
	N = 100;

	callMap = ai->call->GetMetalMap();
	radius = (int) round( (ai->call->GetExtractorRadius()) / SCALAR);
	diameter = radius*2;
	squareRadius=radius*radius;

	map = new unsigned char[X*Z];
	coverage = new unsigned int[diameter*diameter];
	bestCoverage = new unsigned int[diameter*diameter];

	for (int i = 0; i < X*Z; i++)
		map[i] = callMap[i];
	delete[] callMap;

	findBestSpots();
}

CMetalMap::~CMetalMap() {
	delete[] map;
	delete[] coverage;
	delete[] bestCoverage;
}

void CMetalMap::findBestSpots() {
	bool metalSpots;
	int i, x, z, k, bestX, bestZ, highestSaturation, saturation;
	int counter = 0;

	while (true) {
		counter++;
		metalSpots = false;
		highestSaturation = 0;
		k = 0;
		
		for (z = radius; z < Z-radius; z+=2) {
			for (x = radius; x < X-radius; x+=2) {
				if (map[z*X+x] > threshold) {
					metalSpots = true;
					saturation = getSaturation(x, z, &k);
					if (saturation > highestSaturation) {
						highestSaturation = saturation;
						for (i = 0; i < k; i++)
							bestCoverage[i] = coverage[i];
						bestX = x;
						bestZ = z;
					}
				}
			}
		}
		
		if (!metalSpots) break;

		float3 pos = float3(bestX*SCALAR,ai->call->GetElevation(bestX*SCALAR,bestZ*SCALAR), bestZ*SCALAR);
		spots.push_back(MSpot(spots.size(), pos, highestSaturation));

		if (counter >= N) {
			LOGS("Speedmetal infects my skills... I won't play");
			break;
		}

		for (i = 0; i < k; i++)
			map[bestCoverage[i]] = 0;
	}
}


int CMetalMap::getSaturation(int x, int z, int *k) {
	int index, i, j, sum;
	*k = sum = 0;
	float q,d;
	
	for (i = z-radius; i < z+radius; i++) {
		for (j = x-radius; j < x+radius; j+=2) {
			d = squareDist(x, z, j, i);
			if (d > squareRadius) continue;
			index = i*X+j;
			q = radius - sqrt(d)/radius;
			sum += (int) (q * (map[index]+map[index+1]));
			coverage[(*k)++] = index;
			coverage[(*k)++] = index+1;
		}
	}
	return sum;	
}


int CMetalMap::squareDist(int x, int z, int j, int i) {
	x -= j;
	z -= i;
	return x*x + z*z;
}

bool CMetalMap::buildMex(int builder, UnitType *mex) {
	if (taken.size() == spots.size()) return false;
	float3 pos = ai->call->GetUnitPos(builder);
	float pathLength, shortestPathLength = 100000000.0f;
	int i;
	MSpot *ms, *bestMs;
	
	for (i = 0; i < spots.size(); i++) {
		ms = &spots[i];

		std::map<int,float3>::iterator j;
		bool skip = false;
		for (j = taken.begin(); j != taken.end(); j++) {
			float3 diff = (j->second - ms->f);
			if (diff.Length2D() <= ai->call->GetExtractorRadius())
				skip = true;
		}
		if (skip) continue;

		const UnitDef *ud = ai->call->GetUnitDef(builder);
		pathLength = ai->call->GetPathLength(pos, ms->f, ud->movedata->pathType);
		if (pathLength < shortestPathLength) {
			shortestPathLength  = pathLength;
			bestMs              = ms;
		}
	}

	ai->metalMap->taken[builder] = bestMs->f;
	ai->metaCmds->build(builder, mex, bestMs->f);
	return true;
}

void CMetalMap::removeFromTaken(int mex) {
	float3 pos = ai->call->GetUnitPos(mex);
	int erase = -1;
	std::map<int,float3>::iterator i;
	for (i = taken.begin(); i != taken.end(); i++) {
		float3 diff = pos - i->second;
		if (diff.Length2D() <= ai->call->GetExtractorRadius()*2.0f)
			erase = i->first;
	}
	assert(erase != -1);
	taken.erase(erase);
}
