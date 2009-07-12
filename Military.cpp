#include "Military.h"

CMilitary::CMilitary(AIClasses *ai) {
	this->ai      = ai;
	scoutGroup    =  1;
	attackerGroup = -1;
}

void CMilitary::init(int unit) {
	createNewGroup(G_ATTACKER);
}

int CMilitary::selectTarget(float3 &ourPos, std::vector<int> &targets, std::vector<int> &occupied) {
	int target = -1;
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
		bool isOccupied = false;
		for (unsigned j = 0; j < occupied.size(); j++) {
			if (target == occupied[j]) {
				isOccupied = true;
				break;
			}
		}
		if (isOccupied) continue;
		else break;
	}
	return target;
}

int CMilitary::selectHarrasTarget(int scout) {
	std::vector<int> occupiedTargets;
	ai->tasks->getMilitaryTasks(HARRAS, occupiedTargets);
	float3 pos = ai->call->GetUnitPos(scout);
	std::vector<int> targets;
	targets.insert(targets.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());
	targets.insert(targets.end(), ai->intel->mobileBuilders.begin(), ai->intel->mobileBuilders.end());
	return selectTarget(pos, targets, occupiedTargets);
}

int CMilitary::selectAttackTarget(int group) {
	std::vector<int> occupiedTargets;
	ai->tasks->getMilitaryTasks(ATTACK, occupiedTargets);
	int target = -1;
	float3 pos = groups[group].pos();

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
	std::map<int, CMyGroup>::iterator i;
	for (i = groups.begin(); i != groups.end(); i++) {
		bool isNew = (i->first == attackerGroup || i->first == scoutGroup);
		CMyGroup *G = &(i->second);

		if (G->busy && G->type == G_SCOUT) busyScouts++;

		if (isNew || G->busy) continue;

		int target     = -1;
		float strength = MAX_FLOAT;
		task plan      = UNKNOWN;

		switch(G->type) {
			case G_ATTACKER:
				target   = selectAttackTarget(i->first);
				strength = G->strength;
				plan     = ATTACK;
			break;
			case G_SCOUT:
				target   = selectHarrasTarget(i->first);
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
			if (strength >= enemyStrength*1.2f) {

				/* Add the taskplan */
				ai->tasks->addMilitaryPlan(plan, i->first, target);

				/* We are now busy */
				G->busy = true;

				/* Bootstrap the path */
				float3 start = G->pos();
				ai->pf->addPath(i->first, start, goal); 
				break;
			}
		}
	}

	/* Always have enough scouts */
	if (busyScouts+newScouts == 0)
		ai->wl->push(SCOUT, HIGH);
	else if (newScouts == 0)
		ai->wl->push(SCOUT, NORMAL);
		
	/* If enemy offensive is within our perimeter, attack it */
	if (ai->intel->enemyInbound()) {
		/* Regroup our offensives such that we have enough power to engage */

		/* If we cannot get the power, put factories on HIGH priority for offensives */
	}

	/* Else pick a target, decide group power required and attack */
	else {
		/* Pick a target */
		if (groups[attackerGroup].units.size() >= MINGROUPSIZE) {
			int target = selectAttackTarget(attackerGroup);
			if (target != -1) {
				float3 goal = ai->cheat->GetUnitPos(target);
				float enemyStrength = ai->threatMap->getThreat(goal, 100.0f);

				/* If we can confront the enemy, do so */
				if (groups[attackerGroup].strength >= enemyStrength*1.2f) {
					/* Add the taskplan */
					ai->tasks->addMilitaryPlan(ATTACK, attackerGroup, target);

					/* We are busy */
					groups[attackerGroup].busy = true;

					/* Bootstrap the path */
					float3 start = groups[attackerGroup].pos();
					ai->pf->addPath(attackerGroup, start, goal); 

					/* Create new group */
					createNewGroup(G_ATTACKER);
				}
			}
		}
	}

	/* Always build some unit */
	ai->wl->push(randomUnit(), NORMAL);
}

void CMilitary::createNewGroup(groupType type) {
	int group;

	switch (type) {
		case G_ATTACKER:
			group = attackerGroup++;
		break;
		case G_SCOUT:
			group = scoutGroup--;
		break;
		default: return;
	}

	CMyGroup G(ai, group, type);
	groups[group] = G;
}

void CMilitary::addToGroup(int unit, groupType type) {
	int group;

	switch (type) {
		case G_ATTACKER:
			group = attackerGroup;
		break;
		case G_SCOUT:
			group = scoutGroup;
			/* Scout groups initially have one unit, so create a new group */
			createNewGroup(type);
		break;
		default: return;
	}
	groups[group].add(unit);
	units[unit] = group;
}

void CMilitary::removeFromGroup(int unit) {
	int group = units[unit];
	groups[group].remove(unit);
	units.erase(unit);
	bool isNew = (group == attackerGroup || group == scoutGroup);
	if (groups[group].units.empty() && group != isNew) {
		groups.erase(group);
		ai->tasks->militaryplans.erase(group);
		ai->pf->removePath(group);
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
