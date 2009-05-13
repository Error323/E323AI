#ifndef THREATMAP_H
#define THREATMAP_H

#include "E323AI.h"

class CThreatMap {
	public:
		CThreatMap(AIClasses *ai);
		~CThreatMap();

		void update(int frame);
		float getThreat(float3 &pos);
		float getThreat(float3 &center, float radius);
		int W, H;
		float *map;
		float REAL;

	private:
		AIClasses *ai;	

		void draw();

		int   *units;
		int RES;
		float totalPower;
};

#endif
