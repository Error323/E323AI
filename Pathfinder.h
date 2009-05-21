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

		/* Update unit/group paths */
		void updatePaths();

		/* Add a path to a unit or group */
		void addPath(int unitOrGroup, float3 &start, float3 &goal);

		/* Remove a path from a unit or group */
		void removePath(int unitOrGroup);

		int X,Z,dx2,dz2;
		float REAL;

		std::vector<Node> map;

	private:
		AIClasses *ai;

		char buf[1024];

		/* overload */
		void successors(ANode *an, std::queue<ANode*> &succ);

		/* overload */
		float heuristic(ANode *an1, ANode *an2);

		/* Start pathfinding */
		bool getPath(float3 &s, float3 &g, std::vector<float3> &path, int unitOrGroup, float radius = EPSILON);

		void drawMap();

		/* The paths for individual units, or groups */
		std::map<int, std::vector<float3> > paths;

		std::vector<float> heightMap, slopeMap;
	
		/* draw the path ? */
		bool draw;
};

#endif
