#ifndef THREATMAP_H
#define THREATMAP_H

#include "E323AI.h"

class CThreatMap {
	public:
		CThreatMap(AIClasses *ai);
		~CThreatMap();

		void update(int frame);
		float getThreat(float3 pos);

	private:
		AIClasses *ai;	

		void reset();
		void draw();

		int W, H;
		float *map;
		int   *units;
		int RES;
		float REAL;
		float totalPower;
};

#endif
