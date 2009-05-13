#ifndef E323PATHFINDER_H
#define E323PATHFINDER_H

#include "E323AI.h"


class CPathfinder: public AAStar {
	public:
		CPathfinder(AIClasses *ai, int X, int Z, float RES);
		~CPathfinder(){};

		enum nodeType{BLOCKED, START, GOAL, NORMAL};

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
				void setType(CPathfinder::nodeType nt) {type = nt;}
				float3 toFloat3() {return float3(x,0.0f,z);}
		};

		void updateMap(float* weights);

		void updatePaths();

		void addPath(int unitOrGroup, float3 &start, float3 &goal);

		void removePath(int unitOrGroup);

		int X,Z;
		float RES;

		std::vector<Node> map;

	private:
		AIClasses *ai;

		/* overload */
		void successors(ANode *an, std::queue<ANode*> &succ);

		/* overload */
		float heuristic(ANode *an1, ANode *an2);

		/* get the id of the node */
		inline int id(int x, int z) { return x*Z+z; }

		/* Start pathfinding */
		bool getPath(float3 &s, float3 &g, std::vector<float3> &path);

		/* The paths for individual units, or groups */
		std::map<int, std::vector<float3> > paths;

		/* draw the path ? */
		bool draw;
};

#endif
