#ifndef THREATMAP_H
#define THREATMAP_H

#include "headers/Defines.h"
#include "headers/HEngine.h"

class AIClasses;

class CThreatMap {
	public:
		CThreatMap(AIClasses *ai);
		~CThreatMap();

		void update(int frame);
		float getThreat(float3 &center, float radius);
		int X, Z;
		float *map;
		int RES;

	private:
		AIClasses *ai;	
		int   *units;

		void draw();
		float totalPower;
		float REAL;
		float gauss(float x, float sigma, float mu);
};

#endif
