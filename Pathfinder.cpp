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
			float maxGroupLength = ai->military->groups[p->first].size()*10.0f;
			float front = -MAX_FLOAT; /* The pathlength of the unit up front */
			float rear  =  MAX_FLOAT; /* The pathlength of the unit in the rear */
			int   waypoint = -1;

			/* Go through all the units in a group */
			for (u = ai->military->groups[p->first].begin(); u != ai->military->groups[p->first].end(); u++) {
				/* unwait all waiters */
				if (!u->second) {
					ai->metaCmds->wait(u->first);
					u->second = true;
				}

				int s1 = -1, s2 = -1;
				float sl1 = MAX_FLOAT, sl2 = MAX_FLOAT;
				float3 upos = ai->call->GetUnitPos(u->first);

				/* Go through the path for the unit to determine its segment on the path */
				unsigned s;
				for (s = 1; s < p->second.size(); s++) {
					float3 d1       = upos - p->second[s-1];
					float3 d2       = upos - p->second[s];
					float l1        = d1.Length2D();
					float l2        = d2.Length2D();

					/* When sl{1,2} is increasing again, we found the segment: break */
					if (l1 < sl1) {sl1 = l1; s1 = s-1; s2 = s;} else break;
					if (l2 < sl2) {sl2 = l2; s1 = s-1; s2 = s;} else break;
				}

				/* Move the unit to the next two waypoints */
				if (ai->eco->gameIdle.find(u->first) != ai->eco->gameIdle.end()) {
					ai->metaCmds->move(u->first, p->second[s1], true);
					ai->metaCmds->move(u->first, p->second[s2], true);
				}
				
				/* Now calculate the projection of upos onto the vector spawned by s1-s2 */
				float3 uP = (p->second[s1] - p->second[s2]).Normalize();
				float3 up = upos - p->second[s2];
				/* proj_P(x) = (x dot u) * u */
				float3 uproj = uP * up.dot(uP);
				/* calc pos on total path */
				float uposonpath = uproj.Length2D() + (s1*RES);
				if (uposonpath > front) {front = uposonpath; waypoint = s2;}
				if (uposonpath < rear)  rear = uposonpath;
				printf("front = %0.2f, rear = %0.2f, front-rear = %0.2f, maxGroupLength = %0.2f\n", front, rear, (front-rear), maxGroupLength);
				if ((front - rear) > maxGroupLength && s2 == waypoint) {
					/* if it's not waiting yet, wait it */
					if (u->second) {
						ai->metaCmds->wait(u->first);
						u->second = false;
					}
				}
			}
			//if (waypoint % 4 == 0) addPath(p->first;
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
			f.y = ai->call->GetElevation(f.x,f.z)+10;
			path.push_back(f);
			if (draw && i < nodepath.size()-2) {
				ai->call->CreateLineFigure(path[i], path[i+1], 10, 0, 300, 1);
				//ai->call->CreateLineFigure(path[i], path[i], 10, 0, 300, 1);
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
	float h = sqrt(dx*dx + dz*dz);
	return h + abs(dx*dz2 - dx2*dz)*0.001;
}
