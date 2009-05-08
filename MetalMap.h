#ifndef METALMAP_H
#define METALMAP_H

#include "E323AI.h"

#define SCALAR 16.0f

class CMetalMap {
	public:
		CMetalMap(AIClasses *ai);
		~CMetalMap();

		struct MSpot {
			int id;
			float3 f;				// point in 3d space
			int c;					// metal coverage
			float dist;
			
			MSpot(int id, float3 f, int c) {
				this->id = id;
				this->f  = f;
				this->c  = c;
				dist = 0.0f;
			}
		};

		bool buildMex(int builder, UnitType *mex);
		void removeFromTaken(int mex);
		std::map<int,float3> taken;

	private:
		void findBestSpots();
		int getSaturation(int x, int z, int *k);
		int squareDist(int x, int z, int j, int i);

		int X, Z, N;
		int threshold;
		int radius;
		int squareRadius;
		int diameter;
		const unsigned char *callMap;
		unsigned char *map;
		unsigned int *coverage;
		unsigned int *bestCoverage;
		std::list<MSpot> spots;

		AIClasses *ai;
		char buf[1024];
};

#endif
