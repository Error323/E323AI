#include "Pathfinder.h"

CPathfinder::CPathfinder(AIClasses *ai, int X, int Z, float RES) {
	this->ai  = ai;
	this->X   = X;
	this->Z   = Z;
	this->RES = RES;

	sprintf(buf, "[CPathfinder::CPathfinder]\t <%d, %d, %0.2f>", X, Z, RES);
	LOGN(buf);

	/* initialize nodes */
	for (int x = 0; x < X; x++)
		for (int z = 0; z < Z; z++)
			map.push_back(Node(id(x,z), x, z, 1.0f));
	draw = true;
}

void CPathfinder::updateMap(float *weights) {
	for (unsigned i = 0; i < map.size(); i++)
		map[i].w = weights[i];
}

void CPathfinder::updatePaths() {
	std::map<int, std::vector<float3> >::iterator i;
	for (i = paths.begin(); i != paths.end(); i++) {
		if (ai->military->groups.find(i->first) == ai->military->groups.end()) {
			// ignore for now
		}
		else {
			
		}
	}
}

void CPathfinder::addPath(int unitOrGroup, float3 &start, float3 &goal) {
	std::vector<float3> path;
	getPath(start, goal, path);
	paths[unitOrGroup] = path;
}

void CPathfinder::removePath(int unitOrGroup) {
	paths.erase(unitOrGroup);
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

bool CPathfinder::getPath(float3 &s, float3 &g, std::vector<float3> &path) {
	/* If exceeding, snap to boundaries */
	int sx  = int(round(s.x/RES)); sx = std::max<int>(sx, 1); sx = std::min<int>(sx, X-2);
	int sz  = int(round(s.z/RES)); sz = std::max<int>(sz, 1); sz = std::min<int>(sz, Z-2);
	int gx  = int(round(g.x/RES)); gx = std::max<int>(gx, 1); gx = std::min<int>(gx, X-2);
	int gz  = int(round(g.z/RES)); gz = std::max<int>(gz, 1); gz = std::min<int>(gz, Z-2);
	sprintf(buf, "[CPathfinder::getPath]\t <%d, %d> : <%d, %d>", sx, sz, gx, gz);
	LOGN(buf);
	start = &map[id(sx, sz)];
	goal = &map[id(gx, gz)];
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
