#ifndef CDEFENSEMATRIX_H
#define CDEFENSEMATRIX_H

#include <map>

#include "headers/Defines.h"
#include "headers/HEngine.h"

class AIClasses;
class UnitType;
class CUnit;

class CDefenseMatrix {
	public:
		CDefenseMatrix(AIClasses *ai);
		~CDefenseMatrix(){};

		/* Determine all clusters currently ingame */
		void update();

		/* Get the spot that needs defense the most */
		float3 getDefenseBuildSite(UnitType *tower);

		/* Get clusters >= 2 */
		int getClusters();


	private:
		/* A group with spacing s between each building such that s < n */
		struct Cluster {
			Cluster() {
				value = 0.0f;
				center = ZEROVECTOR;
				defenses = 0;
			}
			float value;

			float3 center;

			int defenses;

			std::multimap<float, CUnit*> members;
		};

		int X, Z;
		const float *hm;

		/* Total clustervalue */
		float totalValue;

		/* The clusters, sorted on importance */
		std::multimap<float, Cluster*> clusters;

		/* The building to cluster table */
		std::map<int, Cluster*> buildingToCluster;

		/* The static buildings */
		std::map<int, CUnit*> buildings;

		AIClasses *ai;

		float getValue(CUnit *building);

		void draw();
};

#endif
