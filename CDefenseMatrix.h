#ifndef CDEFENSEMATRIX_H
#define CDEFENSEMATRIX_H

#include <map>

#include "headers/Defines.h"
#include "headers/HEngine.h"

#define CLUSTER_RADIUS 320.0f

class AIClasses;
class UnitType;
class CUnit;

class CDefenseMatrix {

public:
	CDefenseMatrix(AIClasses* ai);
	~CDefenseMatrix();

	/* Determine all clusters currently ingame */
	void update();
	/* Get the spot that needs defense the most */
	float3 getDefenseBuildSite(UnitType* tower);
	/* Number of "big" clusters */
	int getClusters();
	/* Get the nth best defended pos */
	float3 getBestDefendedPos(int n);
	
	bool isPosInBounds(const float3& pos) const;
	/* Distance to defense matrix border */
	float distance2D(const float3& pos) const;

	bool switchDebugMode();

protected:
	AIClasses* ai;

private:
	/* A group with spacing s between each building such that s < n */
	struct Cluster {
		Cluster() {
			reset();
		}
		/* Sum of all units cost registered in current cluster? */
		float value;
		/* Center of cluster */
		float3 center;
		/* Number of defense buildings in a cluster */
		int defenses;
		/* Static units in a cluster (<unit_cost, unit>) */
		std::multimap<float, CUnit*> members;

		void reset() {
			value = 0.0f;
			center = ZeroVector;
			defenses = 0;
		}
	};

	bool drawMatrix;
	int X, Z;
	/* Heightmap data */
	const float *hm;
	/* Total clustervalue */
	float totalValue;
	/* The clusters, sorted on importance */
	std::multimap<float, Cluster*> clusters;

	float getValue(CUnit* building);

	void draw();
};

#endif
