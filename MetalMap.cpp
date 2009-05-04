#include "MetalMap.h"

CMetalMap::CMetalMap(AIClasses *ai) {
	this->ai = ai;
	threshold = 64;

	X = ai->call->GetMapWidth() / 2;
	Z = ai->call->GetMapHeight() / 2;
	N = 100;

	callMap = ai->call->GetMetalMap();
	radius = (int) round( (ai->call->GetExtractorRadius()) / 16);
	diameter = radius*2;
	squareRadius=radius*radius;

	map = new unsigned char[X*Z];
	coverage = new unsigned int[diameter*diameter];
	bestCoverage = new unsigned int[diameter*diameter];

	int i;
	for (i = 0; i < X*Z; i++)
		map[i] = callMap[i];

	findBestSpots();


	MSpot *ms;	
	for (i = 0; i < spots.size(); i++) {
		ms = &spots[i];
		float3 p0(ms->f.x*16, ms->f.y, ms->f.z*16);
		float3 p1(ms->f.x*16, ms->f.y+300, ms->f.z*16);
		ai->call->CreateLineFigure(p0, p1, 20, 1, 3600*60, 0);
	}
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

		float3 pos = float3(bestX,ai->call->GetElevation(bestX,bestZ), bestZ);
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
	float3 pos = ai->call->GetUnitPos(builder);
	float ratio, bestRatio = 100000000.0f;
	int i;
	MSpot *ms, *bestMs;
	pos.x/=16;
	pos.z/=16;
	
	for (i = 0; i < spots.size(); i++) {
		ms = &spots[i];

		std::map<int,float3>::iterator j;
		bool skip = false;
		for (j = taken.begin(); j != taken.end(); j++) {
			float3 scaled(j->second.x / 16.0f, j->second.y / 16.0f, j->second.z / 16.0f);
			float3 diff = (scaled - ms->f);
			if (diff.Length2D() <= 5.0f)
				skip = true;
		}
		if (skip) continue;

		//ratio = euclid(&(ms->f), &pos);
		const UnitDef *ud = ai->call->GetUnitDef(builder);
		ratio = ai->call->GetPathLength(float3(pos.x*16,pos.y*16,pos.z*16), float3(ms->f.x*16,ms->f.y*16,ms->f.z*16), ud->movedata->pathType);
		sprintf(buf,"length = %0.2f", ratio);
		LOGN(buf);
		if (ratio < bestRatio) {
			bestRatio  = ratio;
			bestMs     = ms;
		}
	}

	float3 buildPos = float3(bestMs->f.x*16, bestMs->f.y, bestMs->f.z*16);
	ai->call->CreateLineFigure(buildPos, float3(buildPos.x, buildPos.y+200, buildPos.z), 20, 1, 30*10, 0);
	ai->metaCmds->build(builder, mex->def, buildPos);
	return true;
}

float CMetalMap::euclid(float3 *v0, float3 *v1) {
	float x, y, z;
	x = v0->x - v1->x;
	y = v0->y - v1->y;
	z = v0->z - v1->z;
	return sqrt(x*x + y*y + z*z);
}
