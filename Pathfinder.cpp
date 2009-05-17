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
	draw = false;
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
			float maxGroupLength = ai->military->groups[p->first].size()*10.0f;
			std::map<float, int> M;
			int   waypoint = 0;

			/* Go through all the units in a group */
			for (u = ai->military->groups[p->first].begin(); u != ai->military->groups[p->first].end(); u++) {
				/* unwait all waiters */
				if (!u->second) {
					ai->metaCmds->wait(u->first);
					u->second = true;
				}

				float sl1 = MAX_FLOAT, sl2 = MAX_FLOAT;
				float length = 0.0f;
				int s1 = 0, s2 = 1;
				float3 upos = ai->call->GetUnitPos(u->first);

				/* Go through the path to determine the unit's segment on the path */
				for (unsigned s = 1; s < p->second.size()-3; s++) {
					float3 d1  = upos - p->second[s-1];
					float3 d2  = upos - p->second[s];
					float l1 = d1.Length2D();
					float l2 = d2.Length2D();
					/* When the dist between the unit and the segment is increasing: break */
					length  += (p->second[s1] - p->second[s2]).Length2D();
					waypoint = s > waypoint ? s : waypoint;
					if (l1 > sl1 || l2 > sl2) break;
					s1       = s-1;
					s2       = s;
					sl1      = l1; 
					sl2      = l2; 
				}

				/* Move the unit to the next two waypoints */
				if (ai->call->GetCurrentUnitCommands(u->first)->size() <= 1) {
					ai->metaCmds->move(u->first, p->second[s2+2]);
					ai->metaCmds->move(u->first, p->second[s2+3], true);
				}
				
				/* Now calculate the projection of upos onto the line spanned by s2-s1 */
				float3 uP = (p->second[s1] - p->second[s2]);
				uP.y = 0.0f;
				uP.Normalize();
				float3 up = upos - p->second[s2];
				/* proj_P(x) = (x dot u) * u */
				float3 uproj = uP * (up.x * uP.x + up.z * uP.z);
				/* calc pos on total path */
				float uposonpath = length - uproj.Length2D();
				/* A map sorts on key (low to high) by default */
				M[uposonpath] = u->first;
			}
			/* Set a wait cmd on units that are going to fast */
			float rearval = M.begin()->first;
			for (std::map<float,int>::iterator i = --M.end(); i != M.begin(); i--) {
				if (i->first - rearval > maxGroupLength) {
					if (ai->military->groups[p->first][i->second]) {
						ai->metaCmds->wait(i->second);
						ai->military->groups[p->first][i->second] = false;
					}
				}
				else break;
			}
			/* Recalculate the path every now and then */
			if (waypoint % 3 == 0) {
				if (draw) ai->call->DeleteFigureGroup(1);
				int target = ai->tasks->getTarget(p->first);
				float3 goal = ai->cheat->GetUnitPos(target);
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
		/* Insert a pre-waypoint at the beginning of the path */
		float3 s0 = dynamic_cast<Node*>(nodepath[nodepath.size()-1])->toFloat3();
		float3 s1 = dynamic_cast<Node*>(nodepath[nodepath.size()-2])->toFloat3();
		float3 seg= s0 - s1;
		seg *= 10.0f;
		seg += s0;
		seg *= RES;
		seg.y = ai->call->GetElevation(seg.x, seg.z)+10;
		path.push_back(seg);

		for (unsigned i = nodepath.size()-1; i > 0; i--) {
			Node *n = dynamic_cast<Node*>(nodepath[i]);
			float3 f = n->toFloat3();
			f *= RES;
			f.y = ai->call->GetElevation(f.x, f.z)+10;
			path.push_back(f);
		}

		if (draw) {
			for (unsigned i = 2; i < path.size(); i++) 
				ai->call->CreateLineFigure(path[i-1], path[i], 8.0f, 0, 3000, 1);
			ai->call->SetFigureColor(1, 0.0f, 0.0f, 1.0f, 1.0f);
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
