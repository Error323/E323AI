#include "CDefenseMatrix.h"

#include "CAI.h"
#include "CUnit.h"
#include "CUnitTable.h"

CDefenseMatrix::CDefenseMatrix(AIClasses *ai) {
	this->ai = ai;
}

void CDefenseMatrix::update() {
	/* Reset variables */
	buildingToCluster.clear();
	buildings.clear();
	std::multimap<float, Cluster*>::iterator x;
	for (x = clusters.begin(); x != clusters.end(); x++)
		delete x->second;
	clusters.clear();
	totalValue = 0.0f;

	/* Gather the non attacking, non mobile buildings */
	std::map<int, CUnit*>::iterator i, j;
	for (i = ai->unittable->activeUnits.begin(); i != ai->unittable->activeUnits.end(); i++) {
		unsigned c = i->second->type->cats;
		if (!(c&MOBILE) && !(c&ATTACKER))
			buildings[i->first] = i->second;
	}

	/* Determine clusters */
	for (i = buildings.begin(); i != buildings.end(); i++) {
		/* Continue if the building is already contained in a cluster */
		if (buildingToCluster.find(i->first) != buildingToCluster.end())
			continue;

		/* Define a new cluster */
		Cluster *c = new Cluster();
		c->members.insert(std::pair<float,CUnit*>(i->second->type->cost, i->second));
		buildingToCluster[i->first] = c;
		float3 summedCenter(i->second->pos());
		c->value = i->second->type->cost;

		for (++(j = i); j != buildings.end(); j++) {
			/* Continue if the building is already contained in a cluster */
			if (buildingToCluster.find(j->first) != buildingToCluster.end())
				continue;
			
			const float3 pos1 = j->second->pos();
			std::multimap<float,CUnit*>::iterator k;
			for (k = c->members.begin(); k != c->members.end(); k++) {
				const float3 pos2 = k->second->pos();
				if ((pos1 - pos2).Length2D() <= 300.0f) {
					float buildingValue = j->second->type->cost;
					c->members.insert(std::pair<float,CUnit*>(buildingValue, j->second));
					c->value += buildingValue;
					summedCenter += pos1;
					buildingToCluster[j->first] = c;
					break;
				}
			}
		}

		/* Add the cluster */
		c->center = (summedCenter / c->members.size());
		clusters.insert(std::pair<float,Cluster*>(c->value, c));
		totalValue += c->value;

		/* All buildings have a cluster, stop */
		if (buildingToCluster.size() == buildings.size())
			break;
	}

	draw();
}

void CDefenseMatrix::draw() {
	std::multimap<float, Cluster*>::iterator i;
	for (i = clusters.begin(); i != clusters.end(); i++) {
		int group = int(i->first);
		float3 p0(i->second->center);
		p0.y = ai->cb->GetElevation(p0.x, p0.z) + 10.0f;
		if (i->second->members.size() == 1) {
			float3 p1(p0);
			p1.y += 100.0f;
			ai->cb->CreateLineFigure(p0, p1, 10.0f, 0, DRAW_TIME, group);
		}
		else {
			std::multimap<float, CUnit*>::iterator j;
			for (j = i->second->members.begin(); j != i->second->members.end(); j++) {
				float3 p2 = j->second->pos();
				ai->cb->CreateLineFigure(p0, p2, 5.0f, 0, DRAW_TIME, group);
			}
		}
		ai->cb->SetFigureColor(group, 0.0f, 0.0f, i->first/totalValue, 1.0f);
	}
}
