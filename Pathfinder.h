#ifndef E323PATHFINDER_H
#define E323PATHFINDER_H

#include "E323AI.h"


class CPathfinder: public AAStar {
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
				float3 toFloat3() {return float3(x,0.0f,z);}
		};

		/* Update the map weights */
		void updateMap(float* weights);

		/* Update group paths */
		void updatePaths();

		/* Register subgroups */
		void addGroup(CMyGroup &G, float3 &start, float3 & goal);

		/* Remove a path from a unit or group */
		void removeGroup(CMyGroup &G);


		int X,Z,dx2,dz2;
		float REAL;

		std::map<int, std::vector<Node> > maps;

	private:
		AIClasses *ai;

		char buf[1024];

		/* Active map (maps[activeMap]), CRUCIAL to A* */
		int activeMap;

		/* controls which path may be updated, (round robin-ish) */
		unsigned update;

		/* The paths <group, path> */
		std::map<int, std::vector<float3> > paths;

		/* The group pointers */
		std::map<int, CMyGroup*> groups;

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
		void addPath(int group, float3 &start, float3 &goal);

		/* Start pathfinding */
		bool getPath(float3 &s, float3 &g, std::vector<float3> &path, int group, float radius = EPSILON);

		/* Draw the map */
		void drawMap(int map);

		inline int idx(int x, int z);

};

#endif
