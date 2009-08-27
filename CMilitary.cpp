#include "CMilitary.h"

CMilitary::CMilitary(AIClasses *ai): ARegistrar(200) {
	this->ai = ai;
}

void CMilitary::remove(ARegistrar &group) {
	sprintf(buf, "[CMilitary::remove]\tremove group(%d)", group.key);
	LOGN(buf);
	free.push(lookup[group.key]);
	lookup.erase(group.key);
	activeScoutGroups.erase(group.key);
	activeAttackGroups.erase(group.key);

	std::list<ARegistrar*>::iterator i;
	for (i = records.begin(); i != records.end(); i++)
		(*i)->remove(group);
}

void CMilitary::addUnit(CUnit &unit) {
	unsigned c = unit.type->cats;

	if (c&MOBILE) {
		if (c&SCOUTER) {
			/* A scout is initially alone */
			CGroup *group = requestGroup(SCOUT);
			group->addUnit(unit);
		}
		else {
			/* If there is a new factory, or the current group is busy, request
			 * a new group 
			 */
			unit.moveForward(500.0f);
			if (currentGroups.find(unit.builder) == currentGroups.end() ||
				currentGroups[unit.builder]->busy) {
				CGroup *group = requestGroup(ENGAGE);
				currentGroups[unit.builder] = group;
			}
			currentGroups[unit.builder]->addUnit(unit);
		}
	}
}

CGroup* CMilitary::requestGroup(groupType type) {
	CGroup *group = NULL;
	int index     = 0;

	/* Create a new slot */
	if (free.empty()) {
		group = new CGroup(ai);
		groups.push_back(group);
		index = groups.size()-1;
	}

	/* Use top free slot from stack */
	else {
		index = free.top(); free.pop();
		group = groups[index];
		group->reset();
	}

	lookup[group->key] = index;
	group->reg(*this);

	switch(type) {
		case SCOUT:
			activeScoutGroups[group->key] = group;
		break;
		case ENGAGE:
			activeAttackGroups[group->key] = group;
		break;
		default: return group;
	}

	return group;
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

int CMilitary::selectHarrasTarget(CGroup &group) {
	float3 pos = group.pos();
	std::vector<int> targets;
	targets.insert(targets.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());
	targets.insert(targets.end(), ai->intel->mobileBuilders.begin(), ai->intel->mobileBuilders.end());
	return selectTarget(pos, targets, occupiedTargets);
}

int CMilitary::selectAttackTarget(CGroup &group) {
	int target = -1;
	float3 pos = group.pos();

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

	/* Select remaining targets */
	target = selectTarget(pos, ai->intel->rest, occupiedTargets);
	if (target != -1) return target;

	return target;
}

void CMilitary::update(int frame) {
	occupiedTargets.clear();
	std::map<int, CTaskHandler::AttackTask*>::iterator j;
	for (j = ai->tasks->activeAttackTasks.begin(); j != ai->tasks->activeAttackTasks.end(); j++)
		occupiedTargets.push_back(j->second->target);

	std::map<int, CGroup*>::iterator i,k;
	int busyScouts = 0;
	/* Give idle scouting groups a new attack plan */
	for (i = activeScoutGroups.begin(); i != activeScoutGroups.end(); i++) {
		CGroup *group = i->second;

		/* This group is busy, don't bother */
		if (group->busy) {
			busyScouts++;
			continue;
		}

		int target = selectHarrasTarget(*group);

		/* There are no harras targets available */
		if (target == -1)
			target = selectAttackTarget(*group);

		/* Nothing available */
		if (target == -1)
			break;

		ai->tasks->addAttackTask(target, *group);
	}

	/* Give idle, strong enough groups a new attack plan */
	for (i = activeAttackGroups.begin(); i != activeAttackGroups.end(); i++) {
		CGroup *group = i->second;

		/* This group is busy, don't bother */
		if (group->busy)
			continue;

		int target = selectAttackTarget(*group);

		/* There are no targets available, assist an attack */
		if (target == -1)
			break;
		
		float3 goal = ai->cheat->GetUnitPos(target);

		/* Not strong enough */
		if (group->units.size() < 4 || 
			ai->threatMap->getThreat(goal, group->range) > group->strength) 
		{
			bool isCurrent = false;
			for (k = currentGroups.begin(); k != currentGroups.end(); k++)
				if (k->second->key == group->key)
					isCurrent = true;

			if (!isCurrent) {
				//TODO: Defend?
			}
			continue;
		}

		ai->tasks->addAttackTask(target, *group);
	}

	/* Always have enough scouts */
	int harrasTargets = ai->intel->metalMakers.size();
	if (harrasTargets > activeScoutGroups.size())
		ai->wl->push(SCOUTER, HIGH);

	/* Always build some unit */
	ai->wl->push(requestUnit(), NORMAL);
}

unsigned CMilitary::requestUnit() {
	float r = rng.RandFloat();
	std::multimap<float, unitCategory>::iterator i;
	float sum = 0.0f;
	for (i = ai->intel->roulette.begin(); i != ai->intel->roulette.end(); i++) {
		sum += i->first;
		if (r <= sum) {
			return MOBILE | i->second;
		}
	}
	return MOBILE | ASSAULT;
}
