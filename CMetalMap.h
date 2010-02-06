#ifndef METALMAP_H
#define METALMAP_H

#include <vector>

#include "ARegistrar.h"

#include "headers/HEngine.h"

#define SCALAR 16.0f

class CGroup;
class CUnit;
class AIClasses;

class CMetalMap: public ARegistrar {
	public:
		CMetalMap(AIClasses *ai);
		~CMetalMap();

		/* Overload */
		void remove(ARegistrar &unit);

		/* add a mex/moho */
		void addUnit(CUnit &unit);

		struct MSpot {
			int id;
			float3 f;	// point in 3d space
			int c;		// metal coverage
			// TODO: storing temp var (dist) in a member which is used from outside is a bad design, fix it
			float dist;	// used as temp var to store distance in getMexSpot() and for sorting procedure
			
			MSpot(int id, float3 f, int c) {
				this->id = id;
				this->f  = f;
				this->c  = c;
				dist = 0.0f;
			}

			bool operator<(const MSpot &a) const { 
				return dist < a.dist; 
			}
		};

		bool getMexSpot(CGroup &group, float3 &pos);
		void removeFromTaken(int mex);
		std::map<int,float3> taken; // key = mex unit id, value = spot position

	private:
		void findBestSpots();
		int getSaturation(int x, int z, int *k);
		int squareDist(int x, int z, int j, int i);

		static std::vector<MSpot> spots;

		int X, Z, N;
		int threshold;
		int radius;
		int squareRadius;
		int diameter;
		const unsigned char *cbMap;
		unsigned char *map;
		unsigned int *coverage;
		unsigned int *bestCoverage;

		AIClasses *ai;
};

#endif
