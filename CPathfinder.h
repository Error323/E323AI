#ifndef CPATHFINDER_H
#define CPATHFINDER_H

#include <map>
#include <vector>
#include <iostream>

#include "headers/HEngine.h"
#include "headers/Defines.h"

#include "ARegistrar.h"
#include "AAStar.h"
#include "CThreatMap.h"

#define MOVE_BUFFER 2

#define CACHE_VERSION "CACHEv03"

class CGroup;
class AIClasses;

class CPathfinder: public AAStar, public ARegistrar {
	public:
		CPathfinder(AIClasses *ai);
		~CPathfinder();

		class Node : public ANode {
			public:
				Node(unsigned short int id, unsigned char x, unsigned char z, float w) : ANode(id, w) {
					this->x = x;
					this->z = z;
				}
				std::map<int, std::vector<unsigned short int> > neighbours;
				unsigned char x, z;

				float3 toFloat3() const {
					float fx = x * HEIGHT2REAL * HEIGHT2SLOPE * I_MAP_RES;
					float fy = 0.0f;
					float fz = z * HEIGHT2REAL * HEIGHT2SLOPE * I_MAP_RES;
					return float3(fx, fy, fz);
				}

				void serialize(std::ostream &os);

				static Node* unserialize(std::istream &is) {
					unsigned char x, z;
					unsigned short int m, id;
					char N, M, K;

					is.read((char*)&id, sizeof(unsigned short int));
					is.read((char*)&x, sizeof(unsigned char));
					is.read((char*)&z, sizeof(unsigned char));

					Node *n = new Node(id, x, z, 1.0f);

					is.read((char*)&N, sizeof(char));
					for (unsigned int i = 0; i < N; i++) {
						is.read((char*)&K, sizeof(char));
						std::vector<unsigned short int> V;
						n->neighbours[(int)K] = V;
						is.read((char*)&M, sizeof(char));
						for (unsigned int j = 0; j < M; j++) {
							is.read((char*)&m, sizeof(unsigned short int));
							n->neighbours[(int)K].push_back(m);
						}
					}

					return n;
				}
		};

		/* Get estimated time of arrival */
		float getETA(CGroup&, float3&, float radius = 0.0f);

		/* Update groups following paths */
		void updateFollowers();

		/* update the path itself NOTE: should always be called after
		 * updateFollowers() 
		 */
		void updatePaths();

		/* Add a group to the pathfinder */
		bool addGroup(CGroup &group);

		/* Overload */
		void remove(ARegistrar &obj);

		/* Get closest Node id to real world vector f, return NULL on failure */
		Node* getClosestNode(const float3 &f);

		/* x and z in slopemap resolution */
		bool isBlocked(int x, int z, int movetype);
		
		bool pathExists(CGroup &group, const float3 &s, const float3 &g);

		/* NOTE: slopemap 1:2 heightmap 1:8 realmap, GetMapWidth() and
		 * GetMapHeight() give map dimensions in heightmap resolution
		 */
		int X,Z,XX,ZZ;
		float REAL;


	private:
		AIClasses *ai;

		char buf[1024];

		/* Node Graph */
		static std::vector<Node*> graph;

		/* Number of threads (is not used currently) */
		size_t nrThreads;

		/* Group to receive repathing event next updatePaths() call */
		int repathGroup;

		/* Active map (graph[activeMap]), CRUCIAL to A* */
		int activeMap;

		/* Controls which path may be updated, (round robin-ish) */
		unsigned int update;

		/* The paths <group, path> */
		std::map<int, std::vector<float3> > paths;

		/* The groups */
		std::map<int, CGroup*> groups;

		/* Regrouping */
		std::map<int, bool> regrouping;

		/* draw the path ? */
		bool draw;

		unsigned int graphSize;

		const float *sm;
		const float *hm;

		/* overload */
		void successors(ANode *an, std::queue<ANode*> &succ);

		/* overload */
		float heuristic(ANode *an1, ANode *an2);

		/* Add a path to a unit or group */
		bool addPath(CGroup&, float3 &start, float3 &goal);

		/* Reset the map nodes */
		void resetMap(CGroup&, ThreatMapType = TMT_NONE);

		/* Calculate the nodes */
		void calcNodes();

		/* Determine the nodes their neighbours */
		void calcGraph();

		/* Start pathfinding ("radius" not implemented yet) */
		bool getPath(float3 &s, float3 &g, std::vector<float3> &path, CGroup&, float radius = EPSILON);

		/* Draw the map */
		//void drawMap(int map);
		void drawGraph(int map);
		void drawNode(Node *n);
};

#endif
