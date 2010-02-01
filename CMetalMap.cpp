#include "CMetalMap.h"

#include "CAI.h"
#include "CUnit.h"
#include "CGroup.h"
#include "CUnitTable.h"
#include "CThreatMap.h"
#include "MathUtil.h"

CMetalMap::CMetalMap(AIClasses *ai): ARegistrar(300) {
	this->ai = ai;
	threshold = 64;

	X = ai->cb->GetMapWidth() / 2;
	Z = ai->cb->GetMapHeight() / 2;
	N = 100; // spot count threshold to decide map is speedmetal

	cbMap = ai->cb->GetMetalMap();
	radius = (int) round( (ai->cb->GetExtractorRadius()) / SCALAR);
	diameter = radius * 2;
	squareRadius = radius * radius;

	map = new unsigned char[X*Z];
	coverage = new unsigned int[diameter*diameter];
	bestCoverage = new unsigned int[diameter*diameter];

#if _MSC_VER > 1310 // >= Visual Studio 2005
	memcpy_s(map, sizeof(unsigned char)*X*Z, cbMap, X*Z);
#else
	memcpy(map, cbMap, sizeof(unsigned char)*X*Z);
#endif

	findBestSpots();
}

CMetalMap::~CMetalMap() {
	delete[] map;
	delete[] coverage;
	delete[] bestCoverage;
}

void CMetalMap::remove(ARegistrar &unit) {
	CUnit *u = dynamic_cast<CUnit*>(&unit);
	if (u->type->cats&MEXTRACTOR)
		removeFromTaken(unit.key);
	else
		taken.erase(unit.key);
	LOG_II("CMetalMap::remove " << *u)
}

void CMetalMap::addUnit(CUnit &mex) {
	LOG_II("CMetalMap::addUnit " << mex)

	mex.reg(*this);
	taken[mex.key] = ai->cb->GetUnitPos(mex.key);
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

		float3 pos = float3(bestX*SCALAR, ai->cb->GetElevation(bestX*SCALAR,bestZ*SCALAR), bestZ*SCALAR);
		// TODO: remove this when underwater MSpots will be supported
		if(pos.y >= 0.0)
			spots.push_back(MSpot(spots.size(), pos, highestSaturation));

		if (counter >= N) {
			LOG_SS("Speedmetal infects my skills... I won't play");
			break;
		}

		for (i = 0; i < k; i++)
			map[bestCoverage[i]] = 0;
	}

	std::random_shuffle(spots.begin(), spots.end());
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

bool CMetalMap::getMexSpot(CGroup &group, float3 &spot) {
	if (taken.size() == spots.size())
		// all spots are taken
		return false;

	std::vector<MSpot> sorted;
	float3 pos = group.pos();
	float pathLength;
	MSpot *ms, *bestMs = NULL;
	
	for (unsigned i = 0; i < spots.size(); i++) {
		ms = &(spots[i]);
		
		// TODO: compare spot depth against group move type maxWater & minWater

		bool skip = false;

		// skip already taken (in reality or by task planner) spots...
		for (std::map<int,float3>::iterator j = taken.begin(); j != taken.end(); j++) {
			float3 diff = (j->second - ms->f);
			if (diff.Length2D() <= ai->cb->GetExtractorRadius()*1.5f) {
				skip = true;
				break;
			}
		}
		if (skip) continue;

		// skip spots taken by allied teams...
		int numAllies = ai->cb->GetFriendlyUnits(&ai->unitIDs[0], ms->f, ai->cb->GetExtractorRadius()*1.5f);
		for(unsigned int j = 0; j < numAllies; j++)
		{
			const UnitDef *ud = ai->cb->GetUnitDef(ai->unitIDs[j]);
			if(ud->extractsMetal > 0.0f)
			{
				skip = true;
				break;
			}
		}
		if (skip) continue;		

		pathLength = (pos - ms->f).Length2D() - ms->c*EPSILON;
		ms->dist = pathLength;
		sorted.push_back(*ms);
	}

	std::sort(sorted.begin(), sorted.end());

	float lowestThreat = MAX_FLOAT;
	for (unsigned i = 0; i < sorted.size(); i++) {
		float threat = ai->threatmap->getThreat(sorted[i].f, 0.0f);
		if (threat < lowestThreat) {
			bestMs = &sorted[i];
			lowestThreat = threat;
			if (lowestThreat <= 1.0f) break;
		}
	}
		
	if (lowestThreat > 1.0f) 
		return false;

	CUnit *unit = group.units.begin()->second;
	taken[unit->key] = bestMs->f;
	unit->reg(*this);
	spot = bestMs->f;
	
	return true;
}

void CMetalMap::removeFromTaken(int mex) {
	float3 pos = ai->cb->GetUnitPos(mex);
	int erase = -1;
	std::map<int,float3>::iterator i;
	for (i = taken.begin(); i != taken.end(); i++) {
		float3 diff = pos - i->second;
		if (diff.Length2D() <= ai->cb->GetExtractorRadius()*1.2f)
			erase = i->first;
	}
	assert(erase != -1);
	taken.erase(erase);
}
