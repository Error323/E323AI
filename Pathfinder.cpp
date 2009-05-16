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

void CPathfinder::updateMap(float *weights) {
	for (unsigned i = 0; i < map.size(); i++)
		map[i].w = weights[i];
}

void CPathfinder::updatePaths() {
	std::map<int, std::vector<float3> >::iterator p;
	std::map<int, bool>::iterator u;

	/* Go through all the paths */
	for (p = paths.begin(); p != paths.end(); p++) {

		/* if this path isn't found in a group, it's a path for a single unit */
		if (ai->military->groups.find(p->first) == ai->military->groups.end()) {
			// ignore for now
		}

		/* Else its a group path */
		else {
			float maxGroupLength = ai->military->groups[p->first].size()*30.0f;
			std::map<float, int> M;
			int   waypoint = 0;

			/* Go through all the units in a group */
			for (u = ai->military->groups[p->first].begin(); u != ai->military->groups[p->first].end(); u++) {
				/* unwait all waiters */
				if (!u->second) {
					ai->metaCmds->wait(u->first);
					u->second = true;
				}

				float segDist = MAX_FLOAT;
				int s1 = 0, s2 = 0;
				float3 upos = ai->call->GetUnitPos(u->first);

				/* Go through the path to determine the unit's segment on the path */
				unsigned s;
				for (s = 1; s < p->second.size(); s++) {
					float3 d1  = upos - p->second[s-1];
					float3 d2  = upos - p->second[s];
					float dist = d1.Length2D() + d2.Length2D();
					/* When the dist between the unit and the segment is increasing: break */
					if (dist > segDist) break;
					waypoint = s > waypoint ? s : waypoint;
					segDist  = dist; 
					s1       = s-1; 
					s2       = s;
				}

				/* Move the unit to the next two waypoints */
				if (ai->eco->gameIdle.find(u->first) != ai->eco->gameIdle.end()) {
					ai->metaCmds->move(u->first, p->second[s1], true);
					ai->metaCmds->move(u->first, p->second[s2], true);
				}
				
				/* Now calculate the projection of upos onto the line spanned by s2-s1 */
				float3 uP = (p->second[s2] - p->second[s1]).Normalize();
				float3 up = upos - p->second[s1];
				/* proj_P(x) = (x dot u) * u */
				float3 uproj = uP * up.dot(uP);
				/* calc pos on total path */
				float uposonpath = uproj.Length2D() + (s1*RES);
				/* A map sorts on key (low to high) by default */
				M[uposonpath] = u->first;
			}
			float rearval = M.begin()->first;
			for (std::map<float,int>::iterator i = --M.end(); i != M.begin(); i--) {
				if ((i->first - rearval) > maxGroupLength) {
					/* Make the unit wait */
					if (ai->military->groups[p->first][i->second]) {
						ai->metaCmds->wait(i->second);
						ai->military->groups[p->first][i->second] = false;
					}
				}
				else break;
			}
			/* Recalculate the path every now and then */
			int target = ai->tasks->getTarget(p->first);
			float3 goal = ai->cheat->GetUnitPos(target);
			if (waypoint % 3 == 0) {
				addPath(p->first, p->second[waypoint], goal);
			}
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
			if (x < X-1 && x >= 1 && z < Z-1 && z >= 1) { 
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
	start = &map[id(sx, sz)];
	goal = &map[id(gx, gz)];
	dx2 = sx - gx;
	dz2 = sz - gz;
	std::vector<ANode*> nodepath;
	bool success = findPath(nodepath);
	if (success) {
		for (unsigned i = nodepath.size()-1; i > 0; i--) {
			Node *n = dynamic_cast<Node*>(nodepath[i]);
			float3 f = n->toFloat3();
			f *= RES;
			path.push_back(f);
		}
	}
	return success;
}

float CPathfinder::heuristic(ANode *an1, ANode *an2) {
	Node *n1 = dynamic_cast<Node*>(an1);
	Node *n2 = dynamic_cast<Node*>(an2);
	int dx = n1->x - n2->x;
	int dz = n1->z - n2->z;
	float h = sqrt(dx*dx + dz*dz);
	return h + abs(dx*dz2 - dx2*dz)*0.001;
}
