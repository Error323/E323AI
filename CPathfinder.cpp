#include "CPathfinder.h"


CPathfinder::CPathfinder(AIClasses *ai): ARegistrar(600) {
	this->ai   = ai;
	this->X    = int(ai->call->GetMapWidth() / THREATRES);
	this->Z    = int(ai->call->GetMapHeight() / THREATRES);
	this->REAL = THREATRES*8.0f;
	int SLOPE  = THREATRES/2;
	update     = 0;
	repathGroup= -1;

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

	nrThreads = boost::thread::hardware_concurrency();
	threads.resize(nrThreads-1);

	draw = false;
}

void CPathfinder::resetMap(int thread) {
	int size = (X*Z) / nrThreads;
	int offset = size*thread;
	for (unsigned i = 0; i < size; i++)
		maps[activeMap][i+offset].reset();
	//printf("Reset using thread %d, range[%d, %d] total: %d\n", thread, offset, offset+size, X*Z);
}

void CPathfinder::addTask(ATask &task) {
	task.reg(*this);
	std::map<int, CGroup*>::iterator i;
	tasks[task.key] = &task;
	for (i = task.groups.begin(); i != task.groups.end(); i++) {
		CGroup *group = i->second;
		float3 pos = group->pos();
		addGroup(*group, pos, task.pos);
		lookup[group->key] = task.key;
	}
}

void CPathfinder::remove(ARegistrar &obj) {
	ATask *task = dynamic_cast<ATask*>(&obj);
	sprintf(buf, "[CPathfinder::remove]\tremove task %s(%d)", ai->tasks->taskStr[task->t].c_str(),task->key);
	LOGN(buf);
	std::map<int, CGroup*>::iterator i;
	for (i = task->groups.begin(); i != task->groups.end(); i++) {
		CGroup *group = i->second;
		remove(*group);
	}
	tasks.erase(task->key);
}

void CPathfinder::remove(CGroup &group) {
	paths.erase(group.key);
	groups.erase(group.key);
	lookup.erase(group.key);
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

void CPathfinder::updateFollowers() {
	std::map<int, std::vector<float3> >::iterator path;
	std::map<int, CUnit*>::iterator u;
	unsigned groupnr = 0;
	repathGroup = -1;
	/* Go through all the paths */
	for (path = paths.begin(); path != paths.end(); path++) {
		unsigned segment     = 1;
		int     waypoint     = 1;
		CGroup *group        = groups[path->first];
		float maxGroupLength = group->maxLength();
		std::map<float, CUnit*> M;
		/* Go through all the units in a group */
		for (u = group->units.begin(); u != group->units.end(); u++) {
			CUnit *unit = u->second;
			float sl1 = MAX_FLOAT, sl2 = MAX_FLOAT;
			float length = 0.0f;
			int s1 = 0, s2 = 1;
			float3 upos = unit->pos();
			/* Go through the path to determine the unit's segment on the path
			 */
			for (segment = 1; segment < path->second.size()-waypoint; segment++) {
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
			M[uposonpath] = unit;
		}
		group->move(path->second[segment+waypoint]);
		/* Set a wait cmd on units that are going to fast, (They can still
		 * attack during a wait) 
		 */
		if (M.size() > 1) {
			float rearval = M.begin()->first;
			for (std::map<float,CUnit*>::iterator i = --M.end(); i != M.begin(); i--) {
				CUnit *unit = i->second;
				if (i->first - rearval > maxGroupLength) {
					if (!group->waiters[unit->key]) {
						unit->wait();
						group->waiters[unit->key] = true;
					}
				}
				else {
					if (group->waiters[unit->key]) {
						unit->wait();
						group->waiters[unit->key] = false;
					}
				}
			}
		}
		else {
			CUnit *unit = group->units.begin()->second;
			if (group->waiters[unit->key]) {
				unit->wait();
				group->waiters[unit->key] = false;
			}
		}
		/* See who will get their path updated by updatePaths() */
		if (update % paths.size() == groupnr)
			repathGroup = path->first;
		groupnr++;
	}
}

void CPathfinder::updatePaths() {
	update++;

	/* nothing to update */
	if (groups.find(repathGroup) == groups.end())
		return;

	ATask *task  = tasks[lookup[repathGroup]];
	if (task == NULL) {
		remove(*groups[repathGroup]);
		return;
	}
	float3 start = groups[repathGroup]->pos();
	float3 goal  = task->pos;
	addPath(repathGroup, start, goal);
}

void CPathfinder::addGroup(CGroup &G, float3 &start, float3 &goal) {
	groups[G.key] = &G;
	addPath(G.key, start, goal);
}

void CPathfinder::addPath(int group, float3 &start, float3 &goal) {
	activeMap = groups[group]->moveType;
	std::vector<float3> path;
	/* Initialize threads */
	for (size_t i = 1; i < nrThreads; i++)
		threads[i-1] = new boost::thread(boost::bind(&CPathfinder::resetMap, this, i));

	/* Reset the nodes of this map using threads */
	resetMap(0);
	for (size_t i = 1; i < nrThreads; i++) {
		threads[i-1]->join();
		delete threads[i-1];
	}

	/* Reset leftovers */
	int rest   = (X*Z)%nrThreads;
	int offset = (X*Z)-rest;
	for (unsigned i = 0; i < rest; i++)
		maps[activeMap][i+offset].reset();

	/* If we found a path, add it */
	if (getPath(start, goal, path, group)) {
		/* Add it when not empty */
		if (!path.empty())
			paths[group] = path;
	}
	/* Otherwise, remove this task we can't perform it */
	else tasks[lookup[group]]->remove();
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
	bool success;
	success = findPath(nodepath);
	
	if (success && !nodepath.empty()) {
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
		path.push_back(g);

		if (draw) {
			for (unsigned i = 2; i < path.size(); i++) 
				ai->call->CreateLineFigure(path[i-1], path[i], 8.0f, 0, 500, group);
			ai->call->SetFigureColor(group, rng.RandFloat(), rng.RandFloat(), rng.RandFloat(), 1.0f);
		}
	}
	return success;
}

float CPathfinder::heuristic(ANode *an1, ANode *an2) {
	Node *n1 = dynamic_cast<Node*>(an1);
	Node *n2 = dynamic_cast<Node*>(an2);
	int dx1 = n1->x - n2->x;
	int dz1 = n1->z - n2->z;
	return sqrt(dx1*dx1 + dz1*dz1)*1.000001f;
}

void CPathfinder::successors(ANode *an, std::queue<ANode*> &succ) {
	Node *s, *n = dynamic_cast<Node*>(an);
	int x,z;

	for (unsigned u = 0; u < surrounding.size(); u+=2) {
		x = n->x+surrounding[u]; z = n->z+surrounding[u+1];
		s = &maps[activeMap][idx(x, z)];
		if (!s->blocked()) succ.push(s);
	}
}

void CPathfinder::drawMap(int map) {
	for (int x = 0; x < X; x+=1) {
		for (int z = 0; z < Z; z+=1) {
			float3 p0(x*REAL, ai->call->GetElevation(x*REAL,z*REAL), z*REAL);
			float3 p1(p0);
			p1.y += 100.0f;
			if (maps[map][idx(x,z)].blocked())
				ai->call->CreateLineFigure(p0, p1, map, 1, 10000, 4);
			ai->call->SetFigureColor(map, 1.0f, 0.0f, 0.0f, 1.0f);
		}
	}
}

inline int CPathfinder::idx(int x, int z) {
	return x*Z+z;
}
