#include "CPathfinder.h"

#include <math.h>
#include <limits>

#include "CAI.h"
#include "CTaskHandler.h"
#include "CGroup.h"
#include "CUnit.h"
#include "CUnitTable.h"
#include "CThreatMap.h"
#include "MathUtil.h"
#include "Util.hpp"
#include "CScopedTimer.h"

#include "../AI/Wrappers/LegacyCpp/AIGlobalAI.h"


extern std::map<int, CAIGlobalAI*> myAIs;

std::vector<CPathfinder::Node*> CPathfinder::graph;
int CPathfinder::drawPathGraph = -2;

CPathfinder::CPathfinder(AIClasses *ai): ARegistrar(600, std::string("pathfinder")) {
	this->ai   = ai;
	// NOTE: X and Z are in slope map resolution units
	this->X    = int(ai->cb->GetMapWidth()/HEIGHT2SLOPE);
	this->Z    = int(ai->cb->GetMapHeight()/HEIGHT2SLOPE);
	// NOTE: XX and ZZ are in pathgraph map resolution units
	this->XX   = this->X / I_MAP_RES;
	this->ZZ   = this->Z / I_MAP_RES;
	graphSize  = XX * ZZ;
	update     = 0;
	repathGroup = -1;
	drawPaths  = false;

	hm = ai->cb->GetHeightMap();
	sm = ai->cb->GetSlopeMap();

	if (CPathfinder::graph.empty()) {
		const std::string cacheVersion(CACHE_VERSION);
		const std::string modShortName(ai->cb->GetModShortName());
		const std::string modVersion(ai->cb->GetModVersion());
		const std::string modHash = util::IntToString(ai->cb->GetModHash(), "%x");
		const std::string mapName(ai->cb->GetMapName());
		const std::string mapHash = util::IntToString(ai->cb->GetMapHash(), "%x");

		bool readOk = false;
		unsigned int N;
		std::string cacheMarker;
		std::string filename = 
			std::string(CACHE_FOLDER) + modShortName + "-" + modVersion + "-" +
			modHash + "/" + mapName + "-" + mapHash + "-graph.bin";

		util::SanitizeFileNameInPlace(filename);
		filename = util::GetAbsFileName(ai->cb, filename);
		
		cacheMarker.resize(cacheVersion.size());

		/* See if we can read from binary */
		std::ifstream fin;
		fin.open(filename.c_str(), std::ios::binary | std::ios::in);
		if (fin.good() && fin.is_open()) {
			LOG_II("CPathfinder reading graph from " << filename)
			fin.read(&cacheMarker[0], cacheMarker.size());
			if (!fin.eof() && cacheMarker == cacheVersion) {
				fin.read((char*)&N, sizeof(N));
				if(N == graphSize) {
					for (unsigned int i = 0; i < N; i++) {
						Node *n = Node::unserialize(fin);
						CPathfinder::graph.push_back(n);
					}
					LOG_II("CPathfinder parsed " << CPathfinder::graph.size() << " nodes")
					readOk = true;
				}
			}
			fin.close();
			if(!readOk)
				// TODO: explain for user why?
				LOG_WW("CPathfinder detected invalid cache data")
		}

		if (!readOk)
		{
			std::ofstream fout;

			LOG_II("CPathfinder creating graph at " << filename)
			
			calcGraph();
			
			fout.open(filename.c_str(), std::ios::binary | std::ios::out);
			N = CPathfinder::graph.size();
			fout.write(&cacheVersion[0], cacheVersion.size());
			fout.write((char*)&N, sizeof(N));
			for (unsigned int i = 0; i < N; i++)
				CPathfinder::graph[i]->serialize(fout);
			fout.close();
		}
	}

	this->REAL = HEIGHT2REAL*HEIGHT2SLOPE;
	
	LOG_II("CPathfinder::CPathfinder Heightmap dimensions: " << ai->cb->GetMapWidth() << "x" << ai->cb->GetMapHeight())
	LOG_II("CPathfinder::CPathfinder Pathmap dimensions:   " << XX << "x" << ZZ)

	nrThreads = 1;
}

CPathfinder::~CPathfinder() {
	if(myAIs.size() > 1)
		return; // there are another instances of current AI type

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
	if (md == NULL)
		return false;

	int smidx = ID(x,z);
	int hmidx = (z*HEIGHT2SLOPE)*(X*HEIGHT2SLOPE)+(x*HEIGHT2SLOPE);

	/* Block edges and out of bounds */
	if (z <= 0 || z >= Z-1 || x <= 0 || x >= X-1)
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
			CPathfinder::graph.push_back(new Node(ID_GRAPH(x, z), x, z, 1.0f));

	/* Precalculate surrounding nodes */
	std::vector<int> surrounding;
	float radius = 0.0f;
	while (radius < I_MAP_RES*HEIGHT2SLOPE+EPSILON) {
		radius += 1.0f;
		for (int z = -radius; z <= radius; z++) {
			for (int x = -radius; x <= radius; x++) {
				if (x == 0 && z == 0) continue;
				float length = sqrt(float(x*x + z*z));
				if (length > (radius - 0.5f) && length < (radius + 0.5f)) {
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
		for (int x = 0; x < X; x += I_MAP_RES) {
			for (int z = 0; z < Z; z += I_MAP_RES) {
				Node *parent = CPathfinder::graph[ID_GRAPH(x/I_MAP_RES,z/I_MAP_RES)];
				if (isBlocked(x, z, map))
					continue;

				bool s[] = {false, false, false, false, false, false, false, false};
				for (size_t p = 0; p < surrounding.size(); p += 2) {
					int i = surrounding[p]; //z
					int j = surrounding[p + 1]; //x
					
					int zz = i + z;
					int xx = j + x;

					if (xx < 0)   {s[5] = s[6] = s[7] = true; continue;} // west
					if (xx > X-1) {s[1] = s[2] = s[3] = true; continue;} // east
					if (zz < 0)   {s[7] = s[0] = s[1] = true; continue;} // north
					if (zz > Z-1) {s[5] = s[4] = s[3] = true; continue;} // south

					if (
						(xx % I_MAP_RES != 0 || zz % I_MAP_RES != 0)
						&& !isBlocked(xx, zz, map)
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

void CPathfinder::resetMap(CGroup &group, ThreatMapType tm_type) {
	int idSlopeMap = 0;
	float *threatMap = ai->threatmap->getMap(tm_type);

	if (!threatMap) {
		for(unsigned int id = 0; id < graphSize; id++) {
			Node *n = CPathfinder::graph[id];
			n->reset();
			if (group.cats&LAND) {
				n->w = 1.0f + sm[idSlopeMap] * 2.0f;
				idSlopeMap += I_MAP_RES;
			} else {
				n->w = 1.0f;
			}
		}
	} else {
		for(unsigned int id = 0; id < graphSize; id++) {
			Node *n = CPathfinder::graph[id];
			n->reset();
			if (group.cats&LAND) {
				n->w = threatMap[id] + sm[idSlopeMap] * 5.0f;
				idSlopeMap += I_MAP_RES;
			}
			else
				n->w = threatMap[id] + 1.0f;				
		}
	}
}

float CPathfinder::getPathLength(CGroup &group, float3 &pos) {
	float result = -1.0f;
	float3 gpos = group.pos();

	if (group.pathType < 0) {
		float distance = gpos.distance2D(pos);

		if (distance < EPS)
			result = 0.0f;
		else {
			unsigned int cats = group.cats;

			if (cats&STATIC) {
				if ((cats&BUILDER) && distance < group.buildRange)
					result = 0.0f;
			}
			else if(cats&AIR)
				result = distance;
		}
	}
	else {
		result = ai->cb->GetPathLength(gpos, pos, group.pathType);
	}

	return result;
}

float CPathfinder::getETA(CGroup &group, float3 &pos) {
	float result = getPathLength(group, pos);

	if (result < 0.0f) {
		result = std::numeric_limits<float>::max();	// pos is unreachable
	} else if (result > EPS) {
		if (group.speed	> EPS) {
			result /= group.speed;
		} else {
			result = std::numeric_limits<float>::max(); // can't move to remote pos
		}
	}

	return result;
}

void CPathfinder::updateFollowers() {
	const int currentFrame = ai->cb->GetCurrentFrame();
	
	unsigned int groupnr = 0;
	std::map<int, std::vector<float3> >::iterator path;
	std::map<int, CUnit*>::iterator u;
	std::map<float, CUnit*> M; // key = <distance>
	
	repathGroup = -1;
	
	/* Go through all the paths */
	for (path = paths.begin(); path != paths.end(); path++) {
		CGroup *group = groups[path->first];
		
		if (group->isMicroing()
		|| (currentFrame - regrouping[group->key]) <= FRAMES_TO_REGROUP) {
			groupnr++;
			continue;
		}
		
		int segment = 1;
		int waypoint = std::min<int>((group->cats&AIR) ? 2 * MOVE_BUFFER : MOVE_BUFFER, path->second.size() - segment - 1);
		float maxGroupLength = group->maxLength();
		
		if (maxGroupLength < 200.0f)
			maxGroupLength += 200.0f;
		else
			maxGroupLength *= 2.0f;
	
		// NOTE: among aircraft units only Brawler type units (ASSAULT) 
		// can regroup (TODO: detect this by moving behaviour?)
		bool enableRegrouping = 
			group->units.size() < GROUP_CRITICAL_MASS 
			&& (!(group->cats&AIR) || (group->cats&(ASSAULT|SNIPER|ARTILLERY|ANTIAIR)) == ASSAULT);
		
		M.clear();

		/* Go through all the units in a group */
		for (u = group->units.begin(); u != group->units.end(); ++u) {
			int s1 = 0, s2 = 1;
			float sl1 = MAX_FLOAT, sl2 = MAX_FLOAT;
			float l1, l2;
			float length = 0.0f;
			CUnit *unit = u->second;

			float3 upos = unit->pos();
			
			// go through the path to determine the unit's segment 
			// on the path...
			for (segment = 1; segment < path->second.size() - waypoint; segment++) {
				if (segment == 1)
					l1 = upos.distance2D(path->second[segment - 1]);
				else
					l1 = l2;
				l2 = upos.distance2D(path->second[segment]);
				
				/* When the dist between the unit and the segment is
				 * increasing: break */
				length += (path->second[s1] - path->second[s2]).Length2D();
				if (l1 > sl1 || l2 > sl2) break;
				s1       = segment - 1;
				s2       = segment;
				sl1      = l1; 
				sl2      = l2; 
			}
			
			segment--;

			if (enableRegrouping) {
				/* Now calculate the projection of upos onto the line spanned by
				 * s2-s1 */
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
		}

		/* Regroup when units are getting to much apart from eachother */
		if (M.size() > 1) {
			float lateralDisp = M.rbegin()->first - M.begin()->first;
			if (lateralDisp > maxGroupLength) {
				regrouping[group->key] = currentFrame;
			} else if (lateralDisp < maxGroupLength*0.6f) {
				regrouping[group->key] = 0;
			}
		} else {
			regrouping[group->key] = 0;
		}
		
		if (regrouping[group->key] == 0) {
			/* If not under fine control, advance on the path */
			group->move(path->second[segment + waypoint]);
		} else {
			/* If regrouping, finish that first */
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

	// nothing to update
	if (repathGroup < 0)
		return;
	
	std::map<int, CGroup*>::iterator it = groups.find(repathGroup);
	// group is absent
	if (it == groups.end())
		return;
	CGroup *group = it->second;

	// group has no real task
	if (!group->busy)
		return;

	// group is not following main path
	if (group->isMicroing())
		return;

	float3 start = group->pos(true);
	float3 goal  = ai->tasks->getPos(*group);

	if (!addPath(*groups[repathGroup], start, goal)) {
		LOG_EE("CPathfinder::updatePaths failed for " << (*groups[repathGroup]))
	}
}

void CPathfinder::remove(ARegistrar &obj) {
	if (groups.find(obj.key) == groups.end())
		return;

	CGroup *group = dynamic_cast<CGroup*>(&obj);
	LOG_II("CPathfinder::remove " << (*group))
	paths.erase(group->key);
	groups.erase(group->key);
	regrouping.erase(group->key);
	group->unreg(*this);
}

bool CPathfinder::addGroup(CGroup &group) {
	float3 s = group.pos(true);
	float3 g = ai->tasks->getPos(group);
	bool success = addPath(group, s, g);

	if (success) {
		LOG_II("CPathfinder::addGroup " << group)
		groups[group.key] = &group;
		regrouping[group.key] = 0;
		group.reg(*this);
	}

	return success;
}

bool CPathfinder::pathExists(CGroup &group, const float3 &s, const float3 &g) {
	activeMap = group.pathType;

	resetMap(group);
	
	this->start = getClosestNode(s);
	this->goal = getClosestNode(g);
	
	return (start != NULL && goal != NULL && findPath());
}

bool CPathfinder::addPath(CGroup &group, float3 &start, float3 &goal) {
	activeMap = group.pathType;
	
	// reset the nodes...
	if (activeMap < 0)
		resetMap(group, TMT_AIR);
	else
		resetMap(group, TMT_SURFACE);

	/* If we found a path, add it */
	std::vector<float3> path;
	bool success = getPath(start, goal, path, group);
	if (success && !path.empty())
		paths[group.key] = path;

	return success;
}

float3 CPathfinder::getClosestPos(const float3 &f, CGroup *group) {
	CPathfinder::Node *node = getClosestNode(f, group);
	if (node)
		return node->toFloat3();
	return ERRORVECTOR;
}

CPathfinder::Node* CPathfinder::getClosestNode(const float3 &f, CGroup *group) {
	if(f == ERRORVECTOR)
		return NULL;

	int fx = int(round(f.x/REAL/I_MAP_RES));
	int fz = int(round(f.z/REAL/I_MAP_RES));
	int pathType;
	
	if (group)
		pathType = group->pathType;
	else
		pathType = activeMap;

	Node *bestNode = NULL;
	float bestScore = std::numeric_limits<float>::max();

	for (int i = -1; i <= 1; i++) {
		for (int j = -1; j <= 1; j++) {
			int z = fz + i;
			int x = fx + j;
			
			if (z < 0 || z > ZZ-1 || x < 0 || x > XX-1)
				continue;
			
			if (!isBlocked(x * I_MAP_RES, z * I_MAP_RES, pathType)) {
				Node *n = CPathfinder::graph[ID_GRAPH(x, z)];
				float3 s = n->toFloat3();
				float dist = (s - f).Length2D();
				if(dist < bestScore) {
					bestNode = n;
					bestScore = dist;
				}
			}
		}
	}

	if (bestNode == NULL)
		LOG_EE("CPathfinder::getClosestNode failed to lock node("<<fx<<","<<fz<<") for pos("<<f.x<<","<<f.z<<")")
	
	return bestNode;
}

bool CPathfinder::getPath(float3 &s, float3 &g, std::vector<float3> &path, CGroup &group, float radius) {
	//PROFILE(A*)
	this->start = getClosestNode(s);
	this->goal = getClosestNode(g);

	std::list<ANode*> nodepath;
	
	bool success = (start != NULL && goal != NULL && findPath(&nodepath));
	if (success) {
		/* Insert a pre-waypoint at the starting of the path */
		int waypoint = std::min<int>(4, nodepath.size() - 1);
		std::list<ANode*>::iterator wp;
		int x = 0;
		for (wp = nodepath.begin(); wp != nodepath.end(); ++wp) {
			if (x >= waypoint) break;
			x++;
		}

		float3 ss  = dynamic_cast<Node*>(*wp)->toFloat3();
		float3 seg = s - ss;
		seg *= 1000.0f; /* Blow up the pre-waypoint */
		seg += s;
		seg.y = ai->cb->GetElevation(seg.x, seg.z) + 10.0f;
		path.push_back(seg);

		for (std::list<ANode*>::iterator i = nodepath.begin(); i != nodepath.end(); ++i) {
			Node *n = dynamic_cast<Node*>(*i);
			float3 f = n->toFloat3();
			f.y = ai->cb->GetElevation(f.x, f.z) + 20.0f;
			path.push_back(f);
		}
		path.push_back(g);

		if (drawPaths) {
			int life = MULTIPLEXER * path.size(); // in frames
			for (unsigned i = 2; i < path.size(); i++)
				ai->cb->CreateLineFigure(path[i - 1], path[i], 8.0f, 0, life, group.key + 10);
			ai->cb->SetFigureColor(group.key + 10, group.key/float(CGroup::counter), 1.0f-group.key/float(CGroup::counter), 1.0f, 1.0f);
		}
	}
	else {
		LOG_EE("CPathfinder::getPath failed for " << (group) << " start("<<s.x<<","<<s.z<<") goal("<<g.x<<","<<g.z<<")")
	}

	return success;
}

float CPathfinder::heuristic(ANode *an1, ANode *an2) {
	// tie breaker, gives preference to existing paths
	static const float p = 1.0f / (XX*ZZ) + 1.0f;
	Node *n1 = dynamic_cast<Node*>(an1);
	Node *n2 = dynamic_cast<Node*>(an2);
	int dx1 = n1->x - n2->x;
	int dz1 = n1->z - n2->z;
	return sqrt(float(dx1*dx1 + dz1*dz1))*p;
}

void CPathfinder::successors(ANode *an, std::queue<ANode*> &succ) {
	std::vector<unsigned short int> &V = dynamic_cast<Node*>(an)->neighbours[activeMap];
	for (size_t u = 0, N = V.size(); u < N; u++)
		succ.push(CPathfinder::graph[V[u]]);
}

bool CPathfinder::pathAssigned(CGroup &group) {
	return paths.find(group.key) != paths.end();
}

bool CPathfinder::switchDebugMode(bool graph) {
	if (!graph) {
		drawPaths = !drawPaths;
		return drawPaths;
	}

	if (!ai->isMaster())
		return false;

	if (ai->cb->GetSelectedUnits(&ai->unitIDs[0], 1) == 1) {
		CUnit *unit = ai->unittable->getUnit(ai->unitIDs[0]);
		if (unit && !(unit->type->cats&STATIC)) {
			int pathType;
			MoveData *md = unit->def->movedata;
			
			if (md)
				pathType = md->pathType;
			else
				pathType = -1;

			if (drawPathGraph != pathType) {
				if (drawPathGraph > -2)
					ai->cb->DeleteFigureGroup(10);
				drawGraph(pathType);
				drawPathGraph = pathType;
			}

			return true;
		}
	}
	
	if (drawPathGraph > -2) {
		ai->cb->DeleteFigureGroup(10);		
		drawPathGraph = -2;
	}

	return false;
}

void CPathfinder::drawGraph(int map) {
	static const int figureID = 10;

	for (unsigned int i = 0; i < CPathfinder::graph.size(); i++) {
		Node *p = CPathfinder::graph[i];
		float3 fp = p->toFloat3();
		fp.y = ai->cb->GetElevation(fp.x, fp.z) + 50.0f;
		std::vector<unsigned short> *temp = &p->neighbours[map];
		for (size_t j = 0; j < temp->size(); j++) {
			Node *n = CPathfinder::graph[(*temp)[j]];
			float3 fn = n->toFloat3();
			fn.y = ai->cb->GetElevation(fn.x, fn.z) + 50.0f;
			ai->cb->CreateLineFigure(fp, fn, 10.0f, 1, 10000, figureID);
			ai->cb->SetFigureColor(figureID, 0.0f, 0.0f, 1.0f, 0.5f);
		}
	}
}

void CPathfinder::drawNode(Node *n) {
	static const int figureID = 11;

	float3 fp = n->toFloat3();
	float3 fn = fp;
	fp.y = ai->cb->GetElevation(fp.x, fp.z);
	fn.y = fp.y + 150.0f;
	ai->cb->CreateLineFigure(fp, fn, 10.0f, 1, 10000, figureID);
	ai->cb->SetFigureColor(figureID, 0.0f, 1.0f, 1.0f, 0.5f);
}
