#include "CMilitary.h"

#include "headers/HEngine.h"

#include "CRNG.h"
#include "CAI.h"
#include "CUnit.h"
#include "CGroup.h"
#include "CTaskHandler.h"
#include "CThreatMap.h"
#include "CIntel.h"
#include "CWishList.h"
#include "CUnitTable.h"

CMilitary::CMilitary(AIClasses *ai): ARegistrar(200) {
	this->ai = ai;
}

void CMilitary::remove(ARegistrar &group) {
	LOG_II("CMilitary::remove group(" << group.key << ")")
	free.push(lookup[group.key]);
	lookup.erase(group.key);
	activeScoutGroups.erase(group.key);
	activeAttackGroups.erase(group.key);

	std::list<ARegistrar*>::iterator i;
	for (i = records.begin(); i != records.end(); i++)
		(*i)->remove(group);
}

void CMilitary::addUnit(CUnit &unit) {
	LOG_II("CMilitary::addUnit " << unit)
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

int CMilitary::selectTarget(float3 &ourPos, std::vector<int> &targets) {
	std::multimap<float, int> candidates;

	/* First sort the targets on distance */
	for (unsigned i = 0; i < targets.size(); i++) {
		int target = targets[i];

		float3 epos = ai->cbc->GetUnitPos(target);
		float dist = (epos-ourPos).Length2D();
		dist *= (ai->threatmap->getThreat(epos, 300.0f)+1);
		candidates.insert(std::pair<float,int>(dist, target));
	}

	return candidates.empty() ? -1 : candidates.begin()->second;
}

int CMilitary::selectHarrasTarget(CGroup &group) {
	float3 pos = group.pos();
	std::vector<int> targets;
	targets.insert(targets.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());
	targets.insert(targets.end(), ai->intel->mobileBuilders.begin(), ai->intel->mobileBuilders.end());
	return selectTarget(pos, targets);
}

void CMilitary::prepareTargets(std::vector<int> &targets) {
	occupiedTargets.clear();
	std::map<int, CTaskHandler::AttackTask*>::iterator j;
	for (j = ai->tasks->activeAttackTasks.begin(); j != ai->tasks->activeAttackTasks.end(); j++)
		occupiedTargets.push_back(j->second->target);

	std::vector<int> all;
	all.insert(all.end(), ai->intel->energyMakers.begin(), ai->intel->energyMakers.end());
	all.insert(all.end(), ai->intel->factories.begin(), ai->intel->factories.end());
	all.insert(all.end(), ai->intel->attackers.begin(), ai->intel->attackers.end());
	all.insert(all.end(), ai->intel->rest.begin(), ai->intel->rest.end());
	all.insert(all.end(), ai->intel->mobileBuilders.begin(), ai->intel->mobileBuilders.end());
	all.insert(all.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());

	for (size_t i = 0; i < all.size(); i++) {
		int target = all[i];
		bool isOccupied = false;
		for (size_t j = 0; j < occupiedTargets.size(); j++) {
			if (target == occupiedTargets[j]) {
				isOccupied = true;
				break;
			}
		}

		if (isOccupied) 
			continue;

		targets.push_back(target);
	}
}

void CMilitary::update(int frame) {
	std::vector<int> targets;
	prepareTargets(targets);

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
		float3 pos = group->pos();

		int target = selectHarrasTarget(*group);

		/* There are no harras targets available */
		if (target == -1)
			target = selectTarget(pos, targets);

		/* Nothing available */
		if (target == -1)
			break;

		ai->tasks->addAttackTask(target, *group);
		break;
	}

	/* Give idle, strong enough groups a new attack plan */
	for (i = activeAttackGroups.begin(); i != activeAttackGroups.end(); i++) {
		CGroup *group = i->second;

		/* This group is busy, don't bother */
		if (group->busy)
			continue;

		/* Determine if this group is the current group */
		bool isCurrent = false;
		for (k = currentGroups.begin(); k != currentGroups.end(); k++)
			if (k->second->key == group->key)
				isCurrent = true;

		float3 pos = group->pos();
		/* Not strong enough */
		if (isCurrent && group->strength < ai->threatmap->getThreat(pos, group->range))
			continue;

		int target = selectTarget(pos, targets);

		/* There are no targets available, assist an attack */
		if (target == -1) {
			int j = 0, r = rng.RandInt(ai->tasks->activeAttackTasks.size()-1);
			std::map<int,CTaskHandler::AttackTask*>::iterator i;
			for (i = ai->tasks->activeAttackTasks.begin(); i != ai->tasks->activeAttackTasks.end(); i++) {
				if (j == r) {
					ai->tasks->addAssistTask(*(i->second), *group);
					break;
				}
				j++;
			}
			break;
		}
		
		ai->tasks->addAttackTask(target, *group);
		break;
	}

	/* Always have enough scouts */
	int harrasTargets = ai->intel->metalMakers.size();
	if (harrasTargets > activeScoutGroups.size())
		ai->wishlist->push(SCOUTER, HIGH);

	/* Always build some unit */
	ai->wishlist->push(requestUnit(), NORMAL);
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
	return MOBILE | ASSAULT; // unreachable code :)
}
