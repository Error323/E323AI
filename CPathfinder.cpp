#include "CPathfinder.h"

#include <math.h>
#include <boost/bind.hpp>
#include <boost/version.hpp>

#include "CAI.h"
#include "CTaskHandler.h"
#include "CGroup.h"
#include "CUnit.h"
#include "CUnitTable.h"
#include "CThreatMap.h"

CPathfinder::CPathfinder(AIClasses *ai): ARegistrar(600, std::string("pathfinder")) {
	this->ai   = ai;
	this->X    = int(ai->cb->GetMapWidth()/HEIGHT2SLOPE);
	this->Z    = int(ai->cb->GetMapHeight()/HEIGHT2SLOPE);
	update     = 0;
	repathGroup= -1;

	hm = ai->cb->GetHeightMap();
	sm = ai->cb->GetSlopeMap();

	/* Initialize nodes per map type */
	std::map<int, MoveData*>::iterator i;
	for (i = ai->unittable->moveTypes.begin(); i != ai->unittable->moveTypes.end(); i++) {
		std::vector<Node*> reset;
		std::map<int, Node*> map;
		maps[i->first]        = map;
		activeNodes[i->first] = reset;
		MoveData *md          = i->second;

		for (int z = 0; z < Z; z++) {
			for (int x = 0; x < X; x++) {
				Node *node = new Node(ID(x,z), x, z, 1.0f);
				int j = ID(x,z);

				/* Block edges */
				if (z == 0 || z == Z-1 || x == 0 || x == X-1) {
					node->setType(BLOCKED);
					maps[i->first][ID(x,z)] = node;
					continue;
				}

				/* Block too steep slopes */
				if (sm[j] > md->maxSlope) {
					node->setType(BLOCKED);
				}
				/* Block land */
				if (md->moveType == MoveData::Ship_Move) {
					if (hm[j] >= -md->depth)
						node->setType(BLOCKED);
				}
				/* Block water */
				else {
					if (hm[j] <= -md->depth && md->moveType != MoveData::Hover_Move)
						node->setType(BLOCKED);
				}
				/* Store the usefull nodes */
				if ((x % I_MAP_RES == 0 && z % I_MAP_RES == 0) || node->blocked()) {
					maps[i->first][ID(x,z)] = node;

					if (!node->blocked())
						activeNodes[i->first].push_back(node);
				}
				/* Delete the others */
				else { delete node; }
			}
		}
	}

	/* Precalculate surrounding nodes */
	float radius    = 0.0f;
	while (radius < I_MAP_RES*HEIGHT2SLOPE+EPSILON) {
		radius += 1.0f;
		for (int z = -radius; z <= radius; z++) {
			for (int x = -radius; x <= radius; x++) {
				if (x == 0 && z == 0) continue;
				float length = sqrt(x*x + z*z);
				if (length > radius-0.5f && length < radius+0.5f) {
					surrounding.push_back(z);
					surrounding.push_back(x);
				}
			}
		}
	}

	/* Create the 8 quadrants a node can be in */
	float r[9]; float angle = 22.5f;
	for (int g = 0; g < 8; g++) {
		r[g] = angle;
		angle += 45.0f;
	}
	r[8] = -22.5f;

	/* Define neighbours closest neighbours */
	std::map<int, std::vector<Node*> >::iterator j;
	for (j = activeNodes.begin(); j != activeNodes.end(); j++) {
		LOG_II("CPathfinder::CPathfinder MoveType(" << j->first << ") has " << activeNodes[j->first].size() << " active nodes")
		for (size_t k = 0; k < activeNodes[j->first].size(); k++) {
			Node *parent = activeNodes[j->first][k];

			bool s[] = {false, false, false, false, false, false, false, false};
			for (size_t p = 0; p < surrounding.size(); p+=2) {
				int z = surrounding[p];
				int x = surrounding[p+1];
				
				int zz = z + parent->z;
				int xx = x + parent->x;

				if (xx < 0)   {s[5] = s[6] = s[7] = true; continue;}
				if (xx > X-1) {s[1] = s[2] = s[3] = true; continue;}
				if (zz < 0)   {s[7] = s[0] = s[1] = true; continue;}
				if (zz > Z-1) {s[5] = s[4] = s[3] = true; continue;}

				if (maps[j->first].find(ID(xx,zz)) == maps[j->first].end())
					continue;

				float a;
				if (x == 0) {
					if (z >= 0)
						a = 0.5f * M_PI;
					else
						a = 1.5f * M_PI;
				}
				else if (x > 0) {
					if (z < 0)
						a = 2.0f * M_PI + atan(z/x);
					else
						a = atan(z/x);
				}
				else
					a = M_PI + atan(z/x);

				a = int((2.5 * M_PI - a) / M_PI * 180) % 360;

				float min = r[8];
				int index = 0;
				for (; index < 8; index++) {
					float max = r[index];
					if (a > min && a < max)
						break;
					min = max;
				}

				if (!s[index]) {
					s[index] = true;
					Node *neighbour = maps[j->first][ID(xx,zz)];
					if (!neighbour->blocked())
						parent->neighbours.push_back(neighbour);
				}
				if (s[0] && s[1] && s[2] && s[3] && s[4] && s[5] && s[6] && s[7]) {
					break;
				}
			}
		}
	}

	draw = false;

	this->REAL = HEIGHT2REAL*HEIGHT2SLOPE;
	LOG_II("CPathfinder::CPathfinder Heightmap dimensions " << ai->cb->GetMapWidth() << "x" << ai->cb->GetMapHeight())
	LOG_II("CPathfinder::CPathfinder Pathmap dimensions   " << X/I_MAP_RES << "x" << Z/I_MAP_RES)

/*
	#if (BOOST_VERSION >= 103500)
	nrThreads = boost::thread::hardware_concurrency();
	#else
	nrThreads = 2;
	#endif
*/
	nrThreads = 1;
	threads.resize(nrThreads-1);
}

void CPathfinder::resetMap(int thread) {
	int size = activeNodes[activeMap].size() / nrThreads;
	int offset = size*thread;
	for (unsigned i = 0; i < size; i++) {
		Node *n = activeNodes[activeMap][i+offset];

		int j = (n->z/I_MAP_RES)*(X/I_MAP_RES)+(n->x/I_MAP_RES);
		n->w = ai->threatmap->map[j] + sm[n->id]*5.0f;
		n->reset();
	}
}

float CPathfinder::getETA(CGroup &group, float3 &pos) {
	float3 gpos = group.pos();
	float dist = (pos - gpos).Length2D()+MULTIPLEXER;
	float travelTime = (dist / group.speed);
	return travelTime;
}

void CPathfinder::updateFollowers() {
	std::map<int, std::vector<float3> >::iterator path;
	std::map<int, CUnit*>::iterator u;
	unsigned groupnr = 0;
	repathGroup = -1;
	/* Go through all the paths */
	for (path = paths.begin(); path != paths.end(); path++) {
		unsigned segment     = 1;
		int     waypoint     = std::min<int>(MOVE_BUFFER, path->second.size()-segment-1);
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

		/* Regroup when they are getting to much apart from eachother */
		if (M.size() > 1) {
			float rearval = M.begin()->first;

			std::map<float,CUnit*>::iterator i = --M.end();
			float lateralDisp = i->first - rearval;
			if (lateralDisp > maxGroupLength) {
				regrouping[group->key] = true;
			}
			else if (lateralDisp < maxGroupLength*0.6f) {
				regrouping[group->key] = false;
			}
		} else regrouping[group->key] = false;

		/* If not under fine control, advance on the path */
		if (!group->isMicroing() && !regrouping[group->key]) {
			group->move(path->second[segment+waypoint]);
		}
		/* If regrouping, finish that first */
		else if (!group->isMicroing() && regrouping[group->key]) {
			float3 gpos = group->pos();
			group->move(gpos);
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

	float3 start = groups[repathGroup]->pos();
	float3 goal  = ai->tasks->getPos(*groups[repathGroup]);
	if (!addPath(repathGroup, start, goal)) {
		LOG_EE("CPathfinder::updatePaths failed for " << (*groups[repathGroup]))
		ai->tasks->removeTask(*groups[repathGroup]);
	}
}

void CPathfinder::remove(ARegistrar &obj) {
	CGroup *group = dynamic_cast<CGroup*>(&obj);
	LOG_II("CPathfinder::remove " << (*group))
	paths.erase(group->key);
	groups.erase(group->key);
	regrouping.erase(group->key);
}

bool CPathfinder::addGroup(CGroup &group) {
	LOG_II("CPathfinder::addGroup " << group)
	groups[group.key] = &group;
	regrouping[group.key] = true;
	group.reg(*this);
	float3 s = group.pos();
	float3 g = ai->tasks->getPos(group);
	return addPath(group.key, s, g);
}

bool CPathfinder::addPath(int group, float3 &start, float3 &goal) {
	activeMap = groups[group]->moveType;
	std::vector<float3> path;
	/* Initialize threads */
	/*
	for (size_t i = 1; i < nrThreads; i++)
		threads[i-1] = new boost::thread(boost::bind(&CPathfinder::resetMap, this, i));
	*/

	/* Reset the nodes of this map using threads */
	resetMap(0);
	/*
	for (size_t i = 1; i < nrThreads; i++) {
		threads[i-1]->join();
		delete threads[i-1];
	}
	*/

	/* Reset leftovers */
	/*
	int rest   = activeNodes[activeMap].size() % nrThreads;
	int offset = activeNodes[activeMap].size() - rest;
	for (unsigned i = 0; i < rest; i++)
		activeNodes[activeMap][i+offset]->reset();
	*/

	/* If we found a path, add it */
	bool success = getPath(start, goal, path, group);

	/* Add it when not empty */
	if (success && !path.empty())
		paths[group] = path;

	return success;
}

int CPathfinder::getClosestNodeId(float3 &f) {
	int fz = int(round(f.z/REAL));
	int fx = int(round(f.x/REAL));
	if (maps[activeMap].find(ID(fx,fz)) != maps[activeMap].end()) {
		Node *n = maps[activeMap][ID(fx,fz)];
		if (n != NULL && !n->blocked()) {
			float3 f = n->toFloat3();
			return ID(fx, fz);
		}
	}

	for (size_t i = 0; i < surrounding.size(); i+=2) {
		int z = fz + surrounding[i];
		int x = fx + surrounding[i+1];
		if (maps[activeMap].find(ID(x,z)) != maps[activeMap].end()) {
			Node *n = maps[activeMap][ID(x,z)];
			if (n != NULL && !n->blocked()) {
				float3 f = n->toFloat3();
				return ID(x, z);
			}
		}
	}

	LOG_EE("CPathfinder::getClosestNode failed to lock node")
	return -1;
}

bool CPathfinder::getPath(float3 &s, float3 &g, std::vector<float3> &path, int group, float radius) {

	int sid = getClosestNodeId(s);
	start = maps[activeMap][sid];

	int gid = getClosestNodeId(g);
	goal = maps[activeMap][gid];

	std::list<ANode*> nodepath;
	bool success = (sid != -1 && gid != -1 && (findPath(nodepath)));
	if (success) {
		/* Insert a pre-waypoint at the startning of the path */
		int waypoint = std::min<int>(4, nodepath.size()-1);
		std::list<ANode*>::iterator wp;
		int x = 0;
		for (wp = nodepath.begin(); wp != nodepath.end(); wp++) {
			if (x >= waypoint) break;
			x++;
		}

		float3 ss  = dynamic_cast<Node*>(*wp)->toFloat3();
		float3 seg = s - ss;
		seg *= 1000.0f; /* Blow up the pre-waypoint */
		seg += s;
		seg.y = ai->cb->GetElevation(seg.x, seg.z)+10;
		path.push_back(seg);

		for (std::list<ANode*>::iterator i = nodepath.begin(); i != nodepath.end(); i++) {
			Node *n = dynamic_cast<Node*>(*i);
			float3 f = n->toFloat3();
			f.y = ai->cb->GetElevation(f.x, f.z)+20;
			path.push_back(f);
		}
		path.push_back(g);

		if (draw) {
			for (unsigned i = 2; i < path.size(); i++) 
				ai->cb->CreateLineFigure(path[i-1], path[i], 8.0f, 0, DRAW_TIME, group);
			ai->cb->SetFigureColor(group, group/float(CGroup::counter), 1.0f-group/float(CGroup::counter), 1.0f, 1.0f);
		}
	}
	else {
		LOG_EE("CPathfinder::getPath failed for " << (*groups[group]) << " sid " << sid << " gid " << gid << " start("<<s.x<<","<<s.z<<") goal("<<g.x<<","<<g.z<<")")
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
	Node *n = dynamic_cast<Node*>(an);
	for (size_t u = 0; u < n->neighbours.size(); u++)
		succ.push(n->neighbours[u]);
}

void CPathfinder::drawGraph(int map) {
	for (size_t i = 0; i < activeNodes[map].size(); i++) {
		Node *p = activeNodes[map][i]; 
		if (p->x < X/6 || p->x > (X/6)*3 || p->z > Z/2) continue;
		if (p->x % 8 == 0 && p->z % 8 == 0) {
			float3 fp = p->toFloat3();
			fp.y = ai->cb->GetElevation(fp.x, fp.z) + 20.0f;
			for (size_t j = 0; j < p->neighbours.size(); j++) {
				Node *n = p->neighbours[j];
				float3 fn = n->toFloat3();
				fn.y = ai->cb->GetElevation(fn.x, fn.z) + 20.0f;
				ai->cb->CreateLineFigure(fp, fn, 10.0f, 1, DRAW_TIME, 10);
				ai->cb->SetFigureColor(10, 0.0f, 0.0f, 1.0f, 0.5f);
			}
		}
	}
}

void CPathfinder::drawMap(int map) {
	std::map<int, Node*>::iterator i;
	for (i = maps[map].begin(); i != maps[map].end(); i++) {
		Node *node = i->second; 
		float3 p0 = node->toFloat3();
		p0.y = ai->cb->GetElevation(p0.x, p0.z);
		float3 p1(p0);
		if (node->blocked()) {
			p0.y += 100.0f;
			ai->cb->CreateLineFigure(p0, p1, 10.0f, 1, DRAW_TIME, 10);
			ai->cb->SetFigureColor(10, 1.0f, 0.0f, 0.0f, 1.0f);
		}
		else if (node->w > 10.0f) {
			p1.y += node->w;
			ai->cb->CreateLineFigure(p0, p1, 10.0f, 1, DRAW_TIME, 20);
			ai->cb->SetFigureColor(20, 1.0f, 1.0f, 1.0f, 0.1f);
		}
	}
}
