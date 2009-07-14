#include "Military.h"

CMilitary::CMilitary(AIClasses *ai) {
	this->ai      = ai;
	minGroupSize  = 3;
	maxGroupSize  = 7;
}

void CMilitary::init(int unit) {
}

int CMilitary::selectTarget(float3 &ourPos, std::vector<int> &targets, std::vector<int> &occupied) {
	int target;
	bool isOccupied = true;
	std::map<float, int> M;

	/* First sort the targets on distance */
	for (unsigned i = 0; i < targets.size(); i++) {
		int t = targets[i];
		float3 epos = ai->cheat->GetUnitPos(t);
		if (epos == NULLVECTOR) continue;
		float dist = (epos-ourPos).Length2D();
		M[dist] = t;
	}

	/* Select a non taken target */
	for (std::map<float,int>::iterator i = M.begin(); i != M.end(); i++) {
		target = i->second;
		isOccupied = false;
		for (unsigned j = 0; j < occupied.size(); j++) {
			if (target == occupied[j]) {
				isOccupied = true;
				break;
			}
		}
		if (isOccupied) continue;
		else break;
	}
	return isOccupied ? -1 : target;
}

int CMilitary::selectHarrasTarget(CMyGroup &G) {
	std::vector<int> occupiedTargets;
	ai->tasks->getMilitaryTasks(HARRAS, occupiedTargets);
	float3 pos = G.pos();
	std::vector<int> targets;
	targets.insert(targets.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());
	targets.insert(targets.end(), ai->intel->mobileBuilders.begin(), ai->intel->mobileBuilders.end());
	return selectTarget(pos, targets, occupiedTargets);
}

int CMilitary::selectAttackTarget(CMyGroup &G) {
	std::vector<int> occupiedTargets;
	ai->tasks->getMilitaryTasks(ATTACK, occupiedTargets);
	int target = -1;
	float3 pos = G.pos();

	/* Select an energyMaking target */
	target = selectTarget(pos, ai->intel->energyMakers, occupiedTargets);
	if (target != -1) return target;

	/* Select a mobile builder target */
	target = selectTarget(pos, ai->intel->mobileBuilders, occupiedTargets);
	if (target != -1) return target;

	/* Select a factory target */
	target = selectTarget(pos, ai->intel->factories, occupiedTargets);
	if (target != -1) return target;

	/* Select an offensive target */
	target = selectTarget(pos, ai->intel->attackers, occupiedTargets);
	if (target != -1) return target;

	return target;
}

void CMilitary::update(int frame) {
	int busyScouts = 0, newScouts = 0;

	/* Give idle groups a new attack plan */
	std::map<int, std::map<int, CMyGroup> >::iterator j;
	std::map<int, CMyGroup>::iterator i;
	for (j = groups.begin(); j != groups.end(); j++) {
		int factory = j->first;
		for (i = groups[factory].begin(); i != groups[factory].end(); i++) {
			bool isNew = (i->first == attackGroup[factory] || 
						  i->first == scoutGroup[factory]);
			CMyGroup *G = &(i->second);

			/* Don't assign a plan if we are busy already */
			if (G->busy) {
				if (G->type == G_SCOUT)
					busyScouts++;
				continue;
			}
			/* Don't assign a plan to an empty group */
			if (G->units.empty())
				continue;

			int target     = -1;
			float strength = MAX_FLOAT;
			task plan      = UNKNOWN;

			switch(G->type) {
				case G_ATTACKER:
					target   = selectAttackTarget(*G);
					strength = G->strength;
					plan     = ATTACK;
					/* Group isn't ``mature'' enough */
 					if (isNew && i->second.units.size() < groupSize)
						continue;
				break;
				case G_SCOUT:
					target   = selectHarrasTarget(*G);
					strength = MAX_FLOAT; /* Make sure to always harras */
					plan     = HARRAS;
					newScouts++;
				break;
				default: continue;
			}

			if (target != -1) {
				float3 goal = ai->cheat->GetUnitPos(target);
				float enemyStrength = ai->threatMap->getThreat(goal, G->range);

				/* If we can confront the enemy, do so */
				if (strength >= enemyStrength) {

					/* Add the taskplan */
					ai->tasks->addMilitaryPlan(plan, *G, target);

					/* We are now busy */
					G->busy = true;

					/* Bootstrap the path */
					float3 start = G->pos();
					ai->pf->addGroup(*G, start, goal); 

					/* If this was the new group, get another one */
					if (isNew) createNewGroup(G->type, factory);
				}
			}
		}
	}
	/* Always have enough scouts */
	if (busyScouts+newScouts == 0)
		ai->wl->push(SCOUT, HIGH);
	else if (newScouts == 0)
		ai->wl->push(SCOUT, NORMAL);

	/* Always build some unit */
	ai->wl->push(randomUnit(), NORMAL);
}

void CMilitary::initSubGroups(int factory) {
	std::map<int, CMyGroup> subgroups;
	groups[factory]      = subgroups;
	attackGroup[factory] = 0;
	scoutGroup[factory]  = 0;
	createNewGroup(G_SCOUT, factory);
	createNewGroup(G_ATTACKER, factory);
}

void CMilitary::createNewGroup(groupType type, int factory) {
	int group;
	switch (type) {
		case G_ATTACKER:
			attackGroup[factory]++;
			group     = attackGroup[factory];
			groupSize = rng.RandInt(maxGroupSize-minGroupSize) + minGroupSize;
		break;
		case G_SCOUT:
			scoutGroup[factory]--;
			group     = scoutGroup[factory];
		break;
		default: return;
	}

	CMyGroup G(ai, type);
	groups[factory][group] = G;
	sprintf(buf, "[CMilitary::createNewGroup]\tfactory(%d) group(%d)", factory, group);
	LOGN(buf);
}

void CMilitary::addToGroup(int factory, int unit, groupType type) {
	int group;
	switch (type) {
		case G_ATTACKER:
			group = attackGroup[factory];
		break;
		case G_SCOUT:
			group = scoutGroup[factory];
		break;
		default: return;
	}
	groups[factory][group].add(unit);
	units[factory][unit] = group;
}

void CMilitary::removeFromGroup(int factory, int unit) {
	int group = units[factory][unit];
	groups[factory][group].remove(unit);
	units[factory].erase(unit);
	bool isNew = (group == attackGroup[factory] || 
				  group == scoutGroup[factory]);
				  
	if (groups[factory][group].units.empty() && group != isNew) {
		groups.erase(group);
		ai->tasks->militaryplans.erase(group);
		ai->pf->removeGroup(groups[factory][group]);
	}
}

unsigned CMilitary::randomUnit() {
	float r = rng.RandFloat();
	if (r > 0.1 && r < 0.6)
		return MOBILE|ARTILLERY;
	else if(r >= 0.6)
		return MOBILE|ANTIAIR;
	else 
		return MOBILE|SCOUT;
}
