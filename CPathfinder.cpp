#include "CPathfinder.h"

#include <math.h>

#include "CAI.h"
#include "CTaskHandler.h"
#include "CGroup.h"
#include "CUnit.h"
#include "CUnitTable.h"
#include "CThreatMap.h"
#include "MathUtil.h"
#include "Util.hpp"

std::vector<CPathfinder::Node*> CPathfinder::graph;

CPathfinder::CPathfinder(AIClasses *ai): ARegistrar(600, std::string("pathfinder")) {
	this->ai   = ai;
	this->X    = int(ai->cb->GetMapWidth()/HEIGHT2SLOPE);
	this->Z    = int(ai->cb->GetMapHeight()/HEIGHT2SLOPE);
	this->XX   = this->X / I_MAP_RES;
	this->ZZ   = this->Z / I_MAP_RES;
	update     = 0;
	repathGroup= -1;

	hm = ai->cb->GetHeightMap();
	sm = ai->cb->GetSlopeMap();

	if (CPathfinder::graph.empty()) {
		std::string filename(ai->cb->GetMapName());
		filename = std::string(CACHE_FOLDER) + filename.substr(0, filename.size()-4) + "-graph.bin";
		filename = util::GetAbsFileName(ai->cb, filename);
		std::fstream file;
		file.open(filename.c_str(), std::ios::binary | std::fstream::in);

		/* See if we can read from binary */
		if (file.good() && file.is_open()) {
			LOG_II("CPathfinder reading graph from " << filename)

			unsigned int N;
			file.read((char*)&N, sizeof(unsigned int));
			for (unsigned int i = 0; i < N; i++) {
				Node *n = Node::unserialize(file);
				CPathfinder::graph.push_back(n);
			}
			LOG_II("CPathfinder parsed " << CPathfinder::graph.size() << " nodes")
			file.close();
		}
		else {
			LOG_II("CPathfinder creating graph at " << filename)
			calcGraph();
			file.open(filename.c_str(), std::ios::binary | std::fstream::out);

			unsigned int N = CPathfinder::graph.size();
			file.write((char*)&N, sizeof(unsigned int));
			for (unsigned int i = 0; i < N; i++)
				CPathfinder::graph[i]->serialize(file);
			file.flush();
			file.close();
		}
	}

	drawGraph(1);
	draw = false;

	this->REAL = HEIGHT2REAL*HEIGHT2SLOPE;
	LOG_II("CPathfinder::CPathfinder Heightmap dimensions " << ai->cb->GetMapWidth() << "x" << ai->cb->GetMapHeight())
	LOG_II("CPathfinder::CPathfinder Pathmap dimensions   " << XX << "x" << ZZ)

	nrThreads = 1;
	threads.resize(nrThreads-1);
}

CPathfinder::~CPathfinder() {
	for (unsigned int i = 0; i < CPathfinder::graph.size(); i++)
		delete CPathfinder::graph[i];

	CPathfinder::graph.clear();
}

void CPathfinder::Node::serialize(std::ostream &os) {
	char M, N = (char) neighbours.size();
	os.write((char*)&id, sizeof(unsigned short int));
	os.write((char*)&x, sizeof(unsigned char));
	os.write((char*)&z, sizeof(unsigned char));

	os.write((char*)&N, sizeof(char));
	std::map<int, std::vector<unsigned short int> >::iterator i;
	for (i = neighbours.begin(); i != neighbours.end(); i++) {
		M = (char) i->first;
		os.write((char*)&M, sizeof(char));
		N = (char) i->second.size();
		os.write((char*)&N, sizeof(char));
		for (unsigned int j = 0; j < N; j++)
			os.write((char*)&(i->second[j]), sizeof(unsigned short int));
	}
}

bool CPathfinder::isBlocked(int x, int z, int movetype) {
	MoveData *md = ai->unittable->moveTypes[movetype];
	int smidx = ID(x,z);
	int hmidx = (z*HEIGHT2SLOPE)*(X*HEIGHT2SLOPE)+(x*HEIGHT2SLOPE);

	/* Block edges */
	if (z == 0 || z == Z-1 || x == 0 || x == X-1)
		return true;

	/* Block too steep slopes */
	if (sm[smidx] > md->maxSlope)
		return true;

	switch(md->moveType) {
		case MoveData::Ship_Move: {
			if (-hm[hmidx] < md->depth)
				return true;
		} break;

		case MoveData::Ground_Move: {
			if (-hm[hmidx] > md->depth)
				return true;
		} break;

		case MoveData::Hover_Move: {
			/* can go everywhere */
		} break;
	}

	return false;
}

void CPathfinder::calcGraph() {
	/* Initialize nodes */
	for (int z = 0; z < ZZ; z++)
		for (int x = 0; x < XX; x++)
			CPathfinder::graph.push_back(new Node(ID_GRAPH(x,z), x, z, 1.0f));

	/* Precalculate surrounding nodes */
	std::vector<int> surrounding;
	float radius = 0.0f;
	while (radius < I_MAP_RES*HEIGHT2SLOPE+EPSILON) {
		radius += 1.0f;
		for (int z = -radius; z <= radius; z++) {
			for (int x = -radius; x <= radius; x++) {
				if (x == 0 && z == 0) continue;
				float length = sqrt(float(x*x + z*z));
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

	/* Define closest neighbours */
	std::map<int, MoveData*>::iterator k;
	for (k = ai->unittable->moveTypes.begin(); k != ai->unittable->moveTypes.end(); k++) {
		int map = k->first;
		for (int x = 0; x < X; x+=I_MAP_RES) {
			for (int z = 0; z < Z; z+=I_MAP_RES) {
				Node *parent = CPathfinder::graph[ID_GRAPH(x/I_MAP_RES,z/I_MAP_RES)];

				bool s[] = {false, false, false, false, false, false, false, false};
				for (size_t p = 0; p < surrounding.size(); p+=2) {
					int i = surrounding[p]; //z
					int j = surrounding[p+1]; //x
					
					int zz = i + z;
					int xx = j + x;

					if (xx < 0)   {s[5] = s[6] = s[7] = true; continue;} // west
					if (xx > X-1) {s[1] = s[2] = s[3] = true; continue;} // east
					if (zz < 0)   {s[7] = s[0] = s[1] = true; continue;} // north
					if (zz > Z-1) {s[5] = s[4] = s[3] = true; continue;} // south

					if (
						(xx % I_MAP_RES != 0 || zz % I_MAP_RES != 0) &&
						!isBlocked(xx,zz,map)
					) continue;

					float a;
					if (j == 0) {
						if (i >= 0)
							a = 0.5f * M_PI;
						else
							a = 1.5f * M_PI;
					}
					else if (j > 0) {
						if (i < 0)
							a = 2.0f * M_PI + atan(float(i/j));
						else
							a = atan(float(i/j));
					}
					else
						a = M_PI + atan(float(i/j));

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
						if (!isBlocked(xx, zz, map))
							parent->neighbours[map].push_back(ID_GRAPH(xx/I_MAP_RES,zz/I_MAP_RES));
					}
					if (s[0] && s[1] && s[2] && s[3] && s[4] && s[5] && s[6] && s[7]) {
						break;
					}
				}
			}
		}
	}
}

void CPathfinder::resetMap(int thread) {
	for (unsigned int z = 0; z < ZZ; z++) {
		for (unsigned int x = 0; x < XX; x++) {
			int id = ID_GRAPH(x,z);
			Node *n = CPathfinder::graph[id];
			n->w = ai->threatmap->map[id] + sm[ID(x*I_MAP_RES, z*I_MAP_RES)]*5.0f;
			n->reset();
		}
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
			segment--;

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
		if (!group->isMicroing() && !regrouping[group->key])
			group->move(path->second[segment+waypoint]);

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

	/* group has no real task */
	if (!groups[repathGroup]->busy)
		return;

	float3 start = groups[repathGroup]->pos();
    float3 goal  = ai->tasks->getPos(*groups[repathGroup]);

    if (!addPath(repathGroup, start, goal)) {
		LOG_EE("CPathfinder::updatePaths failed for " << (*groups[repathGroup]))
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
	bool success = addPath(group.key, s, g);
	if (!success) {
		remove(group);
		group.unreg(*this);
	}
	return success;
}

bool CPathfinder::addPath(int group, float3 &start, float3 &goal) {
	activeMap = groups[group]->moveType;
	std::vector<float3> path;
	/* Reset the nodes of this map using threads */
	resetMap(0);

	/* If we found a path, add it */
	bool success = getPath(start, goal, path, group);

	/* Add it when not empty */
	if (success && !path.empty())
		paths[group] = path;

	return success;
}

CPathfinder::Node* CPathfinder::getClosestNodeId(float3 &f) {
	if(f == ERRORVECTOR)
		return NULL;

	int fz = int(round(f.z/REAL/I_MAP_RES));
	int fx = int(round(f.x/REAL/I_MAP_RES));

	std::map<float, Node*> candidates;
	for (int i = -1; i <= 1; i++) {
		for (int j = -1; j <= 1; j++) {
			int z = fz + i;
			int x = fx + j;
			if (z < 0 || z > ZZ-1 || x < 0 || x > XX-1)
				continue;
			Node *n = CPathfinder::graph[ID_GRAPH(x,z)];
			if (!isBlocked(x,z,activeMap)) {
				float3 s = n->toFloat3();
				float dist = (s - f).Length2D();
				candidates[dist] = n;
			}
		}
	}

	if (candidates.empty()) {
		LOG_EE("CPathfinder::getClosestNode failed to lock node("<<fx<<","<<fz<<") for pos("<<f.x<<","<<f.z<<")")
		return NULL;
	}
	else
		return candidates.begin()->second;
}

void CPathfinder::drawNode(Node *n) {
	float3 fp = n->toFloat3();
	float3 fn = n->toFloat3();
	fn.y = ai->cb->GetElevation(fn.x, fn.z) + 200.0f;
	ai->cb->CreateLineFigure(fp, fn, 10.0f, 1, 10000, 11);
	ai->cb->SetFigureColor(11, 0.0f, 1.0f, 1.0f, 0.5f);
}

bool CPathfinder::getPath(float3 &s, float3 &g, std::vector<float3> &path, int group, float radius) {
	start = getClosestNodeId(s);
	goal = getClosestNodeId(g);

	std::list<ANode*> nodepath;
	
	bool success = (start != NULL && goal != NULL && (findPath(nodepath)));
	if (success) {
		/* Insert a pre-waypoint at the starting of the path */
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
		LOG_EE("CPathfinder::getPath failed for " << (*groups[group]) << " start("<<s.x<<","<<s.z<<") goal("<<g.x<<","<<g.z<<")")
	}

	return success;
}

float CPathfinder::heuristic(ANode *an1, ANode *an2) {
	Node *n1 = dynamic_cast<Node*>(an1);
	Node *n2 = dynamic_cast<Node*>(an2);
	int dx1 = n1->x - n2->x;
	int dz1 = n1->z - n2->z;
	return sqrt(float(dx1*dx1 + dz1*dz1))*1.000001f;
}

void CPathfinder::successors(ANode *an, std::queue<ANode*> &succ) {
	Node *n = dynamic_cast<Node*>(an);
	for (size_t u = 0; u < n->neighbours[activeMap].size(); u++)
		succ.push(CPathfinder::graph[n->neighbours[activeMap][u]]);
}

void CPathfinder::drawGraph(int map) {
	for (unsigned int i = 0; i < CPathfinder::graph.size(); i++) {
		Node *p = CPathfinder::graph[i]; 
		float3 fp = p->toFloat3();
		fp.y = ai->cb->GetElevation(fp.x, fp.z) + 20.0f;
		for (size_t j = 0; j < p->neighbours[map].size(); j++) {
			Node *n = CPathfinder::graph[p->neighbours[map][j]];
			float3 fn = n->toFloat3();
			fn.y = ai->cb->GetElevation(fn.x, fn.z) + 20.0f;
			ai->cb->CreateLineFigure(fp, fn, 10.0f, 1, 10000, 10);
			ai->cb->SetFigureColor(10, 0.0f, 0.0f, 1.0f, 0.5f);
		}
	}
}

/*
void CPathfinder::drawMap(int map) {
	std::map<int, int>::iterator i;
	for (i = CPathfinder::graph[map].begin(); i != CPathfinder::graph[map].end(); i++) {
		Node *node = NODE(i->second); 
		float3 p0 = node->toFloat3();
		p0.y = ai->cb->GetElevation(p0.x, p0.z);
		float3 p1(p0);
		if (node->isBlocked()) {
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
*/
