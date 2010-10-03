#include "CDefenseMatrix.h"

#include <math.h>
#include <limits>

#include "CAI.h"
#include "CUnit.h"
#include "CUnitTable.h"
#include "CThreatMap.h"
#include "CIntel.h"
#include "MathUtil.h"


CDefenseMatrix::CDefenseMatrix(AIClasses *ai) {
	this->ai = ai;
	hm = ai->cb->GetHeightMap();
	X  = ai->cb->GetMapWidth();
	Z  = ai->cb->GetMapHeight();
	drawMatrix = false;
}

CDefenseMatrix::~CDefenseMatrix() {
	std::multimap<float, Cluster*>::iterator x;

	for (x = clusters.begin(); x != clusters.end(); ++x)
		delete x->second;
}

float3 CDefenseMatrix::getBestDefendedPos(int n) {
	n = std::min<int>(n, clusters.size() - 1);
	std::multimap<float, Cluster*>::iterator i;
	int j = 0;
	for (i = clusters.begin(); i != clusters.end(); ++i) {
		if (j == n)
			break;
		j++;
	}
	
	return i->second->center;
}

float3 CDefenseMatrix::getDefenseBuildSite(UnitType* tower) {
	Cluster *c = (--clusters.end())->second;
	float3 dir = ai->intel->getEnemyVector() - c->center;
	dir.SafeNormalize();
	float alpha = 0.0f;
	
	switch(c->defenses) {
		case 1:  alpha = M_PI;       break;
		case 2:  alpha = -M_PI/2.0f; break;
		case 3:  alpha = M_PI/2.0f;  break;
		default: alpha = 0.0f;       break;
	}
	
	dir.x = dir.x*cos(alpha)-dir.z*sin(alpha);
	dir.z = dir.x*sin(alpha)+dir.z*cos(alpha);
	dir *= tower->def->maxWeaponRange*0.5f;
	
	float3 pos = dir + c->center;
	float3 best = pos;
	float radius = tower->def->maxWeaponRange*0.3f;
	float min = std::numeric_limits<float>::max();
	float max = std::numeric_limits<float>::min();
	float maxHeight = std::numeric_limits<float>::min();
	float D = ((ai->intel->getEnemyVector() - pos).Length2D() + radius)/HEIGHT2REAL;
	int R = ceil(radius);
	
	for (int i = -R; i <= R; i++) {
		for (int j = -R; j <= R; j++) {
			int x = round((pos.x+j)/HEIGHT2REAL);
			int z = round((pos.z+i)/HEIGHT2REAL);
			if (x < 0 || z < 0 || x > X-1 || z > Z-1)
				continue;
			float3 dist = ai->intel->getEnemyVector() - float3(pos.x+j,pos.y,pos.z+i);
			dist /= HEIGHT2REAL;
			float height = hm[ID(x,z)]*(D - dist.Length2D());
			if (height > maxHeight) {
				best = float3(pos);
				best.x += j;
				best.z += i;
				maxHeight = height;
			}
			if (hm[ID(x,z)] < min)
				min = hm[ID(x,z)];
			if (hm[ID(x,z)] > max)
				max = hm[ID(x,z)];
		}
	}
	
	best.y = ai->cb->GetElevation(best.x, best.z);
	
	return (max - min) > 20.0f ? best : pos;
}

int CDefenseMatrix::getClusters() {
	int bigClusters = 0;
	std::multimap<float, Cluster*>::iterator i;

	for (i = clusters.begin(); i != clusters.end(); ++i) {
		/*
		if (i->second->members.size() > (DIFFICULTY_HARD - ai->difficulty))
			bigClusters++;
		*/
		if (i->second->members.size() > 2)
			bigClusters++;
	}

	return bigClusters;
}

void CDefenseMatrix::update() {
	std::multimap<float, Cluster*>::iterator x;
	std::map<int, Cluster*> buildingToCluster;
	std::map<int, CUnit*> buildings;
	
	for (x = clusters.begin(); x != clusters.end(); ++x)
		delete x->second;
	clusters.clear();
	
	totalValue = 0.0f;

	/* Gather the non attacking, non mobile buildings */
	std::map<int, CUnit*>::iterator i, j;
	std::multimap<float, CUnit*>::iterator k;
	for (i = ai->unittable->staticUnits.begin(); i != ai->unittable->staticUnits.end(); ++i) {
		if ((i->second->type->cats&ATTACKER).none())
			buildings[i->first] = i->second;
	}

	/* Calculate cumulative defensive power */
	float sumDefPower = 0.0f;
	std::map<int, CUnit*>::iterator l;
	for (l = ai->unittable->defenses.begin(); l != ai->unittable->defenses.end(); ++l)
		sumDefPower += ai->cb->GetUnitPower(l->first);

	/* Determine clusters */
	for (i = buildings.begin(); i != buildings.end(); ++i) {
		/* Continue if the building is already contained in a cluster */
		if (buildingToCluster.find(i->first) != buildingToCluster.end())
			continue;

		/* Define a new cluster */
		Cluster *c = new Cluster();
		c->members.insert(std::pair<float,CUnit*>(i->second->type->cost, i->second));
		buildingToCluster[i->first] = c;
		float3 summedCenter(i->second->pos());
		c->value = getValue(i->second);

		for (++(j = i); j != buildings.end(); ++j) {
			/* Continue if the building is already contained in a cluster */
			if (buildingToCluster.find(j->first) != buildingToCluster.end())
				continue;
			
			/* If the unit is within range of the cluster, add it to the cluster */
			const float3 pos1 = j->second->pos();
			for (k = c->members.begin(); k != c->members.end(); ++k) {
				const float3 pos2 = k->second->pos();
				if ((pos1 - pos2).Length2D() <= CLUSTER_RADIUS) {
					float buildingValue = getValue(j->second);
					c->members.insert(std::pair<float,CUnit*>(buildingValue, j->second));
					c->value += buildingValue;
					summedCenter += pos1;
					buildingToCluster[j->first] = c;
					break;
				}
			}
		}

		/* Calculate coverage of current defense for this cluster */
		c->center = (summedCenter / c->members.size());
		for (l = ai->unittable->defenses.begin(); l != ai->unittable->defenses.end(); ++l) {
			const float3 pos1 = l->second->pos();
			float range = l->second->def->maxWeaponRange*0.8f;
			float power = ai->cb->GetUnitPower(l->first)/sumDefPower;
			bool hasDefense = false;
			for (k = c->members.begin(); k != c->members.end(); ++k) {
				const float3 pos2 = k->second->pos();
				float dist = (pos1 - pos2).Length2D();
				if (dist < range) {
					c->value -= (power*(k->first*(range-dist))) / c->members.size();
					hasDefense = true;
				}
			}
			if (hasDefense)
				c->defenses++;
		}
		
		/* Add the cluster */
		clusters.insert(std::pair<float, Cluster*>(c->value, c));
		totalValue += c->value;

		/* All buildings have a cluster, stop */
		if (buildingToCluster.size() == buildings.size())
			break;
	}

	if (drawMatrix)
		draw();
}

float CDefenseMatrix::getValue(CUnit* unit) {
	return unit->type->cost;
}

bool CDefenseMatrix::isPosInBounds(const float3 &pos) const {
	std::multimap<float, Cluster*>::const_iterator i;
	for (i = clusters.begin(); i != clusters.end(); ++i) {
		if(i->second->center.distance2D(pos) <= CLUSTER_RADIUS)
			return true;
	}
	return false;
}

float CDefenseMatrix::distance2D(const float3 &pos) const {
	float result = std::numeric_limits<float>::max();
	std::multimap<float, Cluster*>::const_iterator i;
	for (i = clusters.begin(); i != clusters.end(); ++i) {
		float distance = i->second->center.distance2D(pos);
		if (distance < result)
			result = distance;
	}
	return result;
}

bool CDefenseMatrix::switchDebugMode() {
	drawMatrix = !drawMatrix;
	return drawMatrix;
}

void CDefenseMatrix::draw() {
	std::multimap<float, Cluster*>::iterator i;
	for (i = clusters.begin(); i != clusters.end(); ++i) {
		int group = int(i->first);
		float3 p0(i->second->center);
		p0.y = ai->cb->GetElevation(p0.x, p0.z) + 10.0f;
		if (i->second->members.size() == 1) {
			float3 p1(p0);
			p1.y += 100.0f;
			ai->cb->CreateLineFigure(p0, p1, 10.0f, 0, MULTIPLEXER, group);
		}
		else {
			std::multimap<float, CUnit*>::iterator j;
			for (j = i->second->members.begin(); j != i->second->members.end(); j++) {
				float3 p2 = j->second->pos();
				ai->cb->CreateLineFigure(p0, p2, 5.0f, 0, MULTIPLEXER, group);
			}
		}
		ai->cb->SetFigureColor(group, 0.0f, 0.0f, i->first/totalValue, 1.0f);
	}
}
