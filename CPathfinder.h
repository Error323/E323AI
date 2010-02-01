#ifndef CPATHFINDER_H
#define CPATHFINDER_H

#include <map>
#include <vector>
#include <iostream>
#include <boost/thread.hpp>

#include "headers/HEngine.h"
#include "headers/Defines.h"

#include "ARegistrar.h"
#include "AAStar.h"

#define MOVE_BUFFER 2

class CGroup;
class AIClasses;

class CPathfinder: public AAStar, public ARegistrar {
	public:
		enum nodeType{BLOCKED, START, GOAL, NORMAL};

		CPathfinder(AIClasses *ai);
		~CPathfinder();

		class Node : public ANode {
			public:
				Node(int id, int x, int z, float w) : ANode(id, w) {
					this->x = x;
					this->z = z;
				}
				std::map<int, std::vector<int> > neighbours;
				std::map<int, nodeType> types;
				int x, z;
				bool blocked(int map) {return types.find(map) != types.end() && types[map] == BLOCKED;}
				void setType(int map, nodeType nt) {types[map] = nt;}

				float3 toFloat3() const {
					float fx = x * HEIGHT2REAL * HEIGHT2SLOPE;
					float fy = 0.0f;
					float fz = z * HEIGHT2REAL * HEIGHT2SLOPE;
					return float3(fx, fy, fz);
				}

				void serialize(std::ostream &os);

				static Node* unserialize(std::istream &is) {
					int x, z, m, id;
					char N, M, K;
					nodeType t;

					is.read((char*)&id, sizeof(int));
					is.read((char*)&x, sizeof(int));
					is.read((char*)&z, sizeof(int));

					Node *n = new Node(id, x, z, 1.0f);

					is.read((char*)&N, sizeof(char));
					for (unsigned int i = 0; i < N; i++) {
						is.read((char*)&K, sizeof(char));
						std::vector<int> V;
						n->neighbours[(int)K] = V;
						is.read((char*)&M, sizeof(char));
						for (unsigned int j = 0; j < M; j++) {
							is.read((char*)&m, sizeof(int));
							n->neighbours[(int)K].push_back(m);
						}
					}

					is.read((char*)&N, sizeof(char));
					for (unsigned int i = 0; i < N; i++) {
						is.read((char*)&K, sizeof(char));
						is.read((char*)&t, sizeof(nodeType));
						n->types[(int)K] = t;
					}
					
					return n;
				}
		};

		/* Get estimated time of arrival */
		float getETA(CGroup&, float3&);

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

		/* NOTE: slopemap 1:2 heightmap 1:8 realmap, GetMapWidth() and
		 * GetMapHeight() give map dimensions in heightmap resolution
		 */
		int X,Z;
		float REAL;


	private:
		AIClasses *ai;

		char buf[1024];

		/* Flat vector of nodes */
		std::vector<Node*> nodes;

		/* The threads */
		std::vector<boost::thread*> threads;

		/* Surrounding nodes */
		std::vector<int> surrounding;

		/* Number of threads */
		size_t nrThreads;

		/* group to receive repathing event next updatePaths() call */
		int repathGroup;

		/* Active map (graph[activeMap]), CRUCIAL to A* */
		int activeMap;

		/* controls which path may be updated, (round robin-ish) */
		unsigned update;

		/* The paths <group, path> */
		std::map<int, std::vector<float3> > paths;

		/* The groups */
		std::map<int, CGroup*> groups;

		/* Regrouping */
		std::map<int, bool> regrouping;

		/* draw the path ? */
		bool draw;

		const float *sm;
		const float *hm;

		/* overload */
		void successors(ANode *an, std::queue<ANode*> &succ);

		int getClosestNodeId(float3 &f);

		/* overload */
		float heuristic(ANode *an1, ANode *an2);

		/* Add a path to a unit or group */
		bool addPath(int group, float3 &start, float3 &goal);

		/* Reset the map nodes */
		void resetMap(int thread);

		/* Calculate the nodes */
		void calcNodes();

		/* Determine the nodes their neighbours */
		void calcNeighbours();

		/* Start pathfinding */
		bool getPath(float3 &s, float3 &g, std::vector<float3> &path, int group, float radius = EPSILON);

		/* Draw the map */
		//void drawMap(int map);
		void drawGraph(int map);
		void drawNode(Node *n);
};

#endif
