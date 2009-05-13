#include "Pathfinder.h"

CPathfinder::CPathfinder(AIClasses *ai, int X, int Z, float RES) {
	this->ai  = ai;
	this->X   = X;
	this->Z   = Z;
	this->RES = RES;

	/* initialize nodes */
	for (int x = 0; x < X; x++)
		for (int z = 0; z < Z; z++)
			map.push_back(Node(id(x,z), x, z, 1.0f));
	draw = true;
}

void CPathfinder::update(float *weights) {
	for (unsigned i = 0; i < map.size(); i++)
		map[i].w = weights[i];
}

void CPathfinder::successors(ANode *an, std::queue<ANode*> &succ) {
	Node *s, *n = dynamic_cast<Node*>(an);
	int x,z;

	for (int i = -1; i <= 1; i++) {
		for (int j = -1; j <= 1; j++) {
			/* Don't add the parent node */
			if (i == 0 && j == 0) continue;
			x = n->x+i; z = n->z+j;

			/* Check we are within boundaries */
			if (x < X && x >= 0 && z < Z && z >= 0) { 
				s = &map[id(x, z)];
				if (!s->blocked()) succ.push(s);
			}
		}
	}
}

bool CPathfinder::path(float3 &s, float3 &g, std::vector<float3> &path) {
	int ids = id(round(s.x/RES), round(s.z/RES));
	int idg = id(round(g.x/RES), round(g.z/RES));
	start = &map[ids];
	goal = &map[idg];
	std::vector<ANode*> nodepath;
	bool success = findPath(nodepath);
	if (success) {
		for (unsigned i = 0; i < nodepath.size(); i++) {
			Node *n = dynamic_cast<Node*>(nodepath[i]);
			float3 f = n->toFloat3();
			f *= RES;
			f.y = ai->call->GetElevation(f.x,f.z)+10;
			path.push_back(f);
			if (draw && i > 0) {
				ai->call->CreateLineFigure(path[i-1], path[i], 10, 0, 300, 1);
				ai->call->CreateLineFigure(path[i-1], path[i], 10, 0, 300, 1);
			}
		}
	}
	return success;
}

float CPathfinder::heuristic(ANode *an1, ANode *an2) {
	Node *n1 = dynamic_cast<Node*>(an1);
	Node *n2 = dynamic_cast<Node*>(an2);
	int dx = n1->x - n2->x;
	int dz = n1->z - n2->z;
	return sqrt(dx*dx + dz*dz);
}
