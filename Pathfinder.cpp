#include "Pathfinder.h"

CPathfinder::CPathfinder(AIClasses *ai) {
	this->ai   = ai;
	this->X    = int(ai->call->GetMapWidth() / THREATRES);
	this->Z    = int(ai->call->GetMapHeight() / THREATRES);
	this->REAL = THREATRES*8.0f;
	int SLOPE  = THREATRES/2.0f;
	update     = 0;

	heightMap.resize(X*Z, 0.0f);
	slopeMap.resize(X*Z, 0.0f);
	const float *hm = ai->call->GetHeightMap();
	const float *sm = ai->call->GetSlopeMap();

	for (int i = -1; i <= 1; i++) {
		for (int j = -1; j <= 1; j++) {
			if (i == 0 && j == 0)
				continue;
			surrounding.push_back(i);
			surrounding.push_back(j);
		}
	}

	for (int x = 0; x < X; x++)
		for (int z = 0; z < Z; z++)
			heightMap[x*Z+z] = hm[z*X*THREATRES*THREATRES+x*THREATRES];
	
	for (int x = 0; x < X; x++) {
		for (int z = 0; z < Z; z++) {
			float maxSlope = sm[z*SLOPE*SLOPE*X+x*SLOPE];
			for (unsigned u = 0; u < surrounding.size(); u+=2) {
				int i = surrounding[u] + x;
				int j = surrounding[u+1] + z;
				if (i < 0 || i > X-1 || j < 0 || j > Z-1)
					continue;
				float slope = sm[j*SLOPE*SLOPE*X+i*SLOPE];
				maxSlope = std::max<float>(slope, maxSlope);
			}
			slopeMap[idx(x,z)] = maxSlope;
		}
	}

	/* Initialize nodes per map type */
	std::map<int, MoveData*>::iterator i;
	for (i = ai->unitTable->moveTypes.begin(); i != ai->unitTable->moveTypes.end(); i++) {
		std::vector<Node> map;
		maps[i->first] = map;
		MoveData *md   = i->second;

		for (int x = 0; x < X; x++) {
			for (int z = 0; z < Z; z++) {
				maps[i->first].push_back(Node(idx(x,z), x, z, 1.0f));
				float slope = slopeMap[idx(x,z)];

				/* Block the edges of the map */
				if (x == 0 || x == X-1 || z == 0 || z == Z-1) {
					maps[i->first][idx(x,z)].setType(BLOCKED);
				}
				/* Block too steep slopes */
				if (slope > md->maxSlope) {
					maps[i->first][idx(x,z)].setType(BLOCKED);
				}
				/* Block land */
				if (md->moveType == MoveData::Ship_Move) {
					if (heightMap[idx(x,z)] >= -md->depth)
						maps[i->first][idx(x,z)].setType(BLOCKED);
				}
				/* Block water */
				else {
					if (heightMap[idx(x,z)] <= -md->depth)
						maps[i->first][idx(x,z)].setType(BLOCKED);
				}
			}
		}
	}
	draw = false;
}

void CPathfinder::updateMap(float *weights) {
	std::map<int, std::vector<Node> >::iterator i;
	for (i = maps.begin(); i != maps.end(); i++) {
		for (unsigned j = 0; j < i->second.size(); j++) {
			/* Give a slight preference to non-steep slopes */
			i->second[j].w = weights[j] + slopeMap[j]*5.0f;
		}
	}
}

void CPathfinder::updatePaths() {
	std::map<int, std::vector<float3> >::iterator path;
	std::map<int, bool>::iterator u;

	int groupnr = 1;
	/* Go through all the paths */
	for (path = paths.begin(); path != paths.end(); path++) {
		unsigned segment     = 1;
		int     waypoint     = 0;
		CMyGroup *group      = groups[path->first];
		float maxGroupLength = group->maxLength();
		std::map<float, int> M;

		/* Go through all the units in a group */
		for (u = group->units.begin(); u != group->units.end(); u++) {
			/* unwait all waiters */
			if (u->second) {
				ai->metaCmds->wait(u->first);
				u->second = false;
			}

			float sl1 = MAX_FLOAT, sl2 = MAX_FLOAT;
			float length = 0.0f;
			int s1 = 0, s2 = 1;
			float3 upos = ai->call->GetUnitPos(u->first);

			/* Go through the path to determine the unit's segment on the path
			 */
			for (segment = 1; segment < path->second.size()-1; segment++) {
				float3 d1  = upos - path->second[segment-1];
				float3 d2  = upos - path->second[segment];
				float l1 = d1.Length2D();
				float l2 = d2.Length2D();
				/* When the dist between the unit and the segment is
				 * increasing: break 
				 */
				length  += (path->second[s1] - path->second[s2]).Length2D();
				if (l1 > sl1 || l2 > sl2) break;
				s1       = segment-1;
				s2       = segment;
				sl1      = l1; 
				sl2      = l2; 
				waypoint = s2 > waypoint ? s2 : waypoint;
			}

			/* Now calculate the projection of upos onto the line spanned by
			 * s2-s1 
			 */
			float3 uP = (path->second[s1] - path->second[s2]);
			uP.y = 0.0f;
			uP.Normalize();
			float3 up = upos - path->second[s2];
			/* proj_P(x) = (x dot u) * u */
			float3 uproj = uP * (up.x * uP.x + up.z * uP.z);
			/* calc pos on total path */
			float uposonpath = length - uproj.Length2D();
			/* A map sorts on key (low to high) by default */
			M[uposonpath] = u->first;
		}

		/* Set a wait cmd on units that are going to fast, (They can still
		 * attack during a wait). Only if the groupsize is > 1 ofcourse.
		 */
		if (M.size() > 1) {
			float rearval = M.begin()->first;
			for (std::map<float,int>::iterator i = --M.end(); i != M.begin(); i--) {
				if (i->first - rearval > maxGroupLength) {
					if (!group->units[i->second]) {
						ai->metaCmds->wait(i->second);
						group->units[i->second] = true;
					}
				}
				else break;
			}
		}

		/* store the group waypoint */
		waypoints[group->id] = waypoint;

		/* Round robin through the groups updating their paths */
		if (update % paths.size() == groupnr) {
			int target   = ai->tasks->getTarget(path->first);
			float3 goal  = ai->cheat->GetUnitPos(target);
			float3 start = group->pos();
			addPath(path->first, start, goal);
		}

		if (waypoints[group->id] < path->second.size()-1) {
			/* Enqueue the path if we can */
			if (path->second.size()-1 >= waypoints[group->id]+4) {
				ai->metaCmds->moveGroup(*group, path->second[waypoints[group->id]+2]);
				ai->metaCmds->moveGroup(*group, path->second[waypoints[group->id]+4], true);
			}
			/* Else just move to the goal */
			else {
				ai->metaCmds->moveGroup(*group, path->second[path->second.size()-1]);
				waypoints[group->id] = path->second.size()-1;
			}
		}

		groupnr++;
	}
	update++;
}

void CPathfinder::addGroup(CMyGroup &G, float3 &start, float3 &goal) {
	groups[G.id] = &G;
	addPath(G.id, start, goal);
}

void CPathfinder::removeGroup(CMyGroup &G) {
	paths.erase(G.id);
	groups.erase(G.id);
	waypoints.erase(G.id);
}

void CPathfinder::addPath(int group, float3 &start, float3 &goal) {
	activeMap = groups[group]->moveType;
	std::vector<float3> path;

	/* Try to find a path */
	if (getPath(start, goal, path, group)) {
		paths[group]     = path;
		waypoints[group] = 1;
	}
	else ai->tasks->militaryplans.erase(group);
}

void CPathfinder::successors(ANode *an, std::queue<ANode*> &succ) {
	Node *s, *n = dynamic_cast<Node*>(an);
	Node *g     = dynamic_cast<Node*>(goal);
	int x,z;

	for (unsigned u = 0; u < surrounding.size(); u+=2) {
		x = n->x+surrounding[u]; z = n->z+surrounding[u+1];
		/* Check we are within boundaries */
		if (x < X-1 && x >= 1 && z < Z-1 && z >= 1) { 
			s = &maps[activeMap][idx(x, z)];
			if (!s->blocked() || s == g) succ.push(s);
		}
	}
}

bool CPathfinder::getPath(float3 &s, float3 &g, std::vector<float3> &path, int group, float radius) {
	/* If exceeding, snap to boundaries */
	int sx  = int(round(s.x/REAL)); sx = std::max<int>(sx, 1); sx = std::min<int>(sx, X-2);
	int sz  = int(round(s.z/REAL)); sz = std::max<int>(sz, 1); sz = std::min<int>(sz, Z-2);
	int gx  = int(round(g.x/REAL)); gx = std::max<int>(gx, 1); gx = std::min<int>(gx, X-2);
	int gz  = int(round(g.z/REAL)); gz = std::max<int>(gz, 1); gz = std::min<int>(gz, Z-2);
	start   = &maps[activeMap][idx(sx, sz)];
	goal    = &maps[activeMap][idx(gx, gz)];
	dx2     = sx - gx;
	dz2     = sz - gz;

	std::list<ANode*> nodepath;
	bool success = (findPath(nodepath) && !nodepath.empty());
	if (success) {
		/* Insert a pre-waypoint at the beginning of the path */
		float3 s0, s1;
		s0 = dynamic_cast<Node*>(*(nodepath.begin()))->toFloat3();
		if (nodepath.size() >= 2)
			s1 = dynamic_cast<Node*>(*(++nodepath.begin()))->toFloat3();
		else 
			s1 = g;

		float3 seg= s0 - s1;
		seg *= 100.0f; /* Blow up the pre-waypoint */
		seg += s0;
		seg *= REAL;
		seg.y = ai->call->GetElevation(seg.x, seg.z)+10;
		path.push_back(seg);

		for (std::list<ANode*>::iterator i = nodepath.begin(); i != nodepath.end(); i++) {
			Node *n = dynamic_cast<Node*>(*i);
			float3 f = n->toFloat3();
			f *= REAL;
			f.y = ai->call->GetElevation(f.x, f.z)+20;
			path.push_back(f);
		}

		if (draw) {
			for (unsigned i = 2; i < path.size(); i++) 
				ai->call->CreateLineFigure(path[i-1], path[i], 8.0f, 0, 500, group);
			float3 c((group%1)/1.0f, (group%2)/2.0f, (group%3)/3.0f);
			ai->call->SetFigureColor(group, c[0], c[1], c[2], 1.0f);
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
	return h + abs(dx*dz2 - dx2*dz)*EPSILON;
}

void CPathfinder::drawMap(int map) {
	for (int x = 0; x < X; x+=1) {
		for (int z = 0; z < Z; z+=1) {
			float3 p0(x*REAL, ai->call->GetElevation(x*REAL,z*REAL), z*REAL);
			float3 p1(p0);
			p1.y += 100.0f;
			if (maps[map][idx(x,z)].blocked())
				ai->call->CreateLineFigure(p0, p1, 4, 1, 10000, 4);
			ai->call->SetFigureColor(4, 1.0f, 0.0f, 0.0f, 1.0f);
		}
	}
}

inline int CPathfinder::idx(int x, int z) {
	return x*Z+z;
}
