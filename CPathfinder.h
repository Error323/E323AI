#ifndef CPATHFINDER_H
#define CPATHFINDER_H

#include <map>
#include <vector>
#include <boost/thread.hpp>

#include "headers/HEngine.h"
#include "headers/Defines.h"

#include "ARegistrar.h"
#include "AAStar.h"

#define MOVE_BUFFER 3

class ATask;
class CGroup;
class AIClasses;

class CPathfinder: public AAStar, public ARegistrar {
	public:
		enum nodeType{BLOCKED, START, GOAL, NORMAL};

		CPathfinder(AIClasses *ai);
		~CPathfinder(){};


		class Node : public ANode {
			public:
				Node(int id, int x, int z, float w) : ANode(id, w) {
					this->x = x;
					this->z = z;
					this->type = CPathfinder::NORMAL;
				}
				CPathfinder::nodeType type;
				int x, z;
				bool blocked() {return type == CPathfinder::BLOCKED;}
				void setType(nodeType nt) {type = nt;}
				float3 toFloat3() {
					float fx = x*HEIGHT2REAL*I_MAP_RES-(I_MAP_RES/2);
					float fy = 0.0f;
					float fz = z*HEIGHT2REAL*I_MAP_RES-(I_MAP_RES/2);
					return float3(fx,fy,fz);
				}
		};

		/* Update the map weights */
		void updateMap(float* weights);

		/* Update groups following paths */
		void updateFollowers();

		/* update the path itself NOTE: should always be called after
		 * updateFollowers() 
		 */
		void updatePaths();

		/* Register a new group */
		bool addGroup(CGroup &G, float3 &start, float3 &goal);

		/* Add task */
		bool addTask(ATask &task);

		/* Overload */
		void remove(ARegistrar &obj);

		/* NOTE: slopemap 1:2 heightmap 1:8 realmap, GetMapWidth() and
		 * GetMapHeight() give map dimensions in heightmap resolution
		 */
		int X,Z;
		float REAL;

		std::map<int, std::vector<Node> > maps;

	private:
		AIClasses *ai;

		char buf[1024];

		/* The threads */
		std::vector<boost::thread*> threads;

		/* Number of threads */
		size_t nrThreads;

		/* group to receive repathing event next updatePaths() call */
		int repathGroup;

		/* Active map (maps[activeMap]), CRUCIAL to A* */
		int activeMap;

		/* controls which path may be updated, (round robin-ish) */
		unsigned update;

		/* The paths <group, path> */
		std::map<int, std::vector<float3> > paths;

		/* The groups */
		std::map<int, CGroup*> groups;

		/* spring maps */
		std::vector<float> heightMap, slopeMap;

		/* surrounding squares (x,y) */
		std::vector<int> surrounding;
	
		/* draw the path ? */
		bool draw;

		/* overload */
		void successors(ANode *an, std::queue<ANode*> &succ);

		/* overload */
		float heuristic(ANode *an1, ANode *an2);

		/* Add a path to a unit or group */
		bool addPath(int group, float3 &start, float3 &goal);

		/* Reset the map nodes */
		void resetMap(int thread);

		/* Start pathfinding */
		bool getPath(float3 &s, float3 &g, std::vector<float3> &path, int group, float radius = EPSILON);
		/* Draw the map */
		void drawMap(int map);

		inline int idx(int x, int z);
};

#endif
