#include "CMilitary.h"

#include <limits>

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
#include "CConfigParser.h"

CMilitary::CMilitary(AIClasses *ai): ARegistrar(200, std::string("military")) {
	this->ai = ai;
}

CMilitary::~CMilitary()
{
	for(int i = 0; i < groups.size(); i++)
		delete groups[i];	
}

void CMilitary::remove(ARegistrar &group) {
	LOG_II("CMilitary::remove group(" << group.key << ")")
	
	free.push(lookup[group.key]);
	lookup.erase(group.key);
	activeScoutGroups.erase(group.key);
	activeAttackGroups.erase(group.key);
	mergeScouts.erase(group.key);
	mergeGroups.erase(group.key);

	for (std::map<int,CGroup*>::iterator i = currentGroups.begin(); i != currentGroups.end(); i++) {
		if (i->second->key == group.key) {
			currentGroups.erase(group.key);
			break;
		}
	}
	
	// NOTE: CMilitary is registered inside group, so the next lines 
	// are senseless because records.size() == 0 always
	std::list<ARegistrar*>::iterator i;
	for (i = records.begin(); i != records.end(); i++)
		(*i)->remove(group);

	group.unreg(*this);
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
			if (currentGroups.find(unit.builtBy) == currentGroups.end() ||
				currentGroups[unit.builtBy]->busy) {
				CGroup *group = requestGroup(ENGAGE);
				currentGroups[unit.builtBy] = group;
			}
			currentGroups[unit.builtBy]->addUnit(unit);
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

int CMilitary::selectTarget(float3 &ourPos, float radius, bool scout, std::vector<int> &targets) {
	int target = -1;
	float estimate = std::numeric_limits<float>::max();
	float factor = scout ? 10000 : 1;

	/* First sort the targets on distance */
	for (unsigned i = 0; i < targets.size(); i++) {
		int tempTarget = targets[i];

		float3 epos = ai->cbc->GetUnitPos(tempTarget);
		float dist = (epos - ourPos).Length2D();
		dist += (factor * ai->threatmap->getThreat(epos, radius));
		
		if(dist < estimate) {
			estimate = dist;
			target = tempTarget;
		}
	}

	return target;
}

void CMilitary::prepareTargets(std::vector<int> &targets1, std::vector<int> &targets2) {
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
	all.insert(all.end(), ai->intel->restUnarmedUnits.begin(), ai->intel->restUnarmedUnits.end());

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

		targets1.push_back(target);
	}

	std::vector<int> harras;
	// TODO: put radars on the fist place when appropriate tag will be 
	// introduced
	harras.insert(harras.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());
	harras.insert(harras.end(), ai->intel->mobileBuilders.begin(), ai->intel->mobileBuilders.end());
	harras.insert(harras.end(), ai->intel->energyMakers.begin(), ai->intel->energyMakers.end());
	harras.insert(harras.end(), ai->intel->factories.begin(), ai->intel->factories.end());
	harras.insert(harras.end(), ai->intel->restUnarmedUnits.begin(), ai->intel->restUnarmedUnits.end());
	

	for (size_t i = 0; i < harras.size(); i++) {
		int target = harras[i];
		bool isOccupied = false;
		for (size_t j = 0; j < occupiedTargets.size(); j++) {
			if (target == occupiedTargets[j]) {
				isOccupied = true;
				break;
			}
		}

		if (isOccupied) 
			continue;

		targets2.push_back(target);
	}
}

void CMilitary::update(int frame) {
	int target = 0;

	std::vector<int> all, harras;
	
	prepareTargets(all, harras);

	std::map<int, CGroup*>::iterator i,k;
	
	/* Give idle scouting groups a new attack plan */
	for (i = activeScoutGroups.begin(); i != activeScoutGroups.end(); i++) {
		CGroup *group = i->second;

		/* This group is busy, don't bother */
		if (group->busy)
			continue;

		// NOTE: if once target is not found it will never appear during
		// current loop execution
		if (target >= 0) {
			float3 pos = group->pos();
			target = selectTarget(pos, 300.0f, true, harras);
			/* There are no harras targets available */
			if (target < 0)
				target = selectTarget(pos, 300.0f, true, all);
		}

		/* Nothing available */
		if (target < 0) {
			if (group->units.size() < MAX_SCOUTS_IN_GROUP)
				mergeScouts[group->key] = group;
		}
		else {
			float3 tpos = ai->cbc->GetUnitPos(target);
			float threat = ai->threatmap->getThreat(tpos, 0.0f);
			if (threat < group->strength) {
				mergeScouts.erase(group->key);
				ai->tasks->addAttackTask(target, *group);
				break;
			}
			else {
				//LOG_II("CMilitary::update target at (" << tpos << ") is too dangerous (threat=" << threat << ") for scout Group(" << group->key << "):strength(" << group->strength << ")")
				if (group->units.size() < MAX_SCOUTS_IN_GROUP)
					mergeScouts[group->key] = group;
			}
		}

		// prevent merging of more than 2 groups
		if(mergeScouts.size() >= 2)
			break;
	}

	/* Merge the scout groups that were not strong enough */
	if (mergeScouts.size() >= 2) {
		ai->tasks->addMergeTask(mergeScouts);
		mergeScouts.clear();
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

		/* Select a target */
		float3 pos = group->pos();
		int target = selectTarget(pos, 0.0f, false, all);

		/* There are no targets available, assist an attack */
		if (target == -1) {
			if (!ai->tasks->activeAttackTasks.empty()) {
				mergeGroups.erase(group->key);
				ai->tasks->addAssistTask(*(ai->tasks->activeAttackTasks.begin()->second), *group);
				break;
			}
		}

		/* Determine if the group has the strength */
		float3 targetpos    = ai->cbc->GetUnitPos(target);
		bool isStrongEnough = group->strength >= ai->threatmap->getThreat(targetpos, 0.0f);
		bool isSizeEnough   = group->units.size() >= ai->cfgparser->getMinGroupSize(group->techlvl);

		if (!isCurrent && !isStrongEnough)
			mergeGroups[group->key] = group;

		if ((isCurrent && isSizeEnough) || (!isCurrent && isStrongEnough)) {
			mergeGroups.erase(group->key);
			ai->tasks->addAttackTask(target, *group);
			break;
		}
	}

	/* Merge the groups that were not strong enough */
	if (mergeGroups.size() >= 2) {
		ai->tasks->addMergeTask(mergeGroups);
		mergeGroups.clear();
	}

	/* Always have enough scouts */
	if (activeScoutGroups.size() < ai->cfgparser->getMinScouts())
		// TODO: LAND cat should vary between LAND|SEA|AIR actually depending
		// on map type
		ai->wishlist->push(LAND | SCOUTER, HIGH);

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
			// TODO: LAND cat should vary between LAND|SEA|AIR actually 
			// depending on map type
			return LAND | MOBILE | i->second;
		}
	}
	return LAND | MOBILE | ASSAULT; // unreachable code :)
}

int CMilitary::idleScoutGroupsNum() {
	int result = 0;
	std::map<int, CGroup*>::iterator i;
	for(i = activeScoutGroups.begin(); i != activeScoutGroups.end(); i++)
		if(!i->second->busy)
			result++;
	return result;
}
