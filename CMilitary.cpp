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
#include "CDefenseMatrix.h"
#include "ReusableObjectFactory.hpp"
#include "GameMap.hpp"

CMilitary::CMilitary(AIClasses *ai): ARegistrar(200, std::string("military")) {
	this->ai = ai;
}

CMilitary::~CMilitary()
{
}

void CMilitary::remove(ARegistrar &object) {
	CGroup *group = dynamic_cast<CGroup*>(&object);
	
	LOG_II("CMilitary::remove " << (*group))
	
	activeScoutGroups.erase(group->key);
	activeAttackGroups.erase(group->key);
	activeBomberGroups.erase(group->key);
	mergeGroups.erase(group->key);
	
	for (std::map<int,CGroup*>::iterator i = assemblingGroups.begin(); i != assemblingGroups.end(); i++) {
		if (i->second->key == group->key) {
			assemblingGroups.erase(i->first);
			break;
		}
	}

	group->unreg(*this);

	ReusableObjectFactory<CGroup>::Release(group);
}

void CMilitary::addUnit(CUnit &unit) {
	LOG_II("CMilitary::addUnit " << unit)
	
	assert(unit.group == NULL);

	unsigned int c = unit.type->cats;

	if (c&MOBILE) {
		unsigned int wishedCats = ai->unittable->unitsUnderConstruction[unit.key];
		CGroup *group;
		
		if (c&SCOUTER && wishedCats && !(wishedCats&SCOUTER))
			c &= ~SCOUTER; // scouter was not requested

		if (c&SCOUTER) {
			/* A scout is initially alone */
			group = requestGroup(SCOUT);
		}
		else if((c&AIR) && (c&ARTILLERY)) {
			group = requestGroup(BOMBER);
		}
		else {
			/* If there is a new factory, or the current group is busy, request
			   a new group */
			std::map<int,CGroup*>::iterator i = assemblingGroups.find(unit.builtBy);
			if (i == assemblingGroups.end()	|| i->second->busy) {
				group = requestGroup(ENGAGE);
				assemblingGroups[unit.builtBy] = group;
			} else {
				group = i->second;
			}
		}
		group->addUnit(unit);
	}
}

CGroup* CMilitary::requestGroup(MilitaryGroupBehaviour type) {
	CGroup *group = ReusableObjectFactory<CGroup>::Instance();
	group->ai = ai;
	group->reset();
	group->reg(*this);
	LOG_II("CMilitary::requestGroup " << (*group))

	switch(type) {
		case SCOUT:
			activeScoutGroups[group->key] = group;
			break;
		case BOMBER:
			activeBomberGroups[group->key] = group;
			break;
		case ENGAGE:
			activeAttackGroups[group->key] = group;
			break;
		default:
			LOG_EE("CMilitary::requestGroup invalid group behaviour: " << type)
	}

	return group;
}

int CMilitary::selectTarget(CGroup &group, float radius, std::vector<int> &targets) {
	bool scout = group.cats&SCOUTER;
	bool bomber = (group.cats&AIR) && (group.cats&ARTILLERY);
	int target = -1;
	float estimate = std::numeric_limits<float>::max();
	float fearFactor = scout || bomber ? 10000.0f : 1.0f;
	float unitDamageK;
	float3 ourPos = group.pos();

	/* First sort the targets on distance */
	for (unsigned int i = 0; i < targets.size(); i++) {
		int tempTarget = targets[i];
		
		if (!group.canAttack(tempTarget))
			continue;
		
		const UnitDef *ud = ai->cbc->GetUnitDef(tempTarget);
		
		float unitMaxHealth = ai->cbc->GetUnitMaxHealth(tempTarget);
		if (unitMaxHealth > EPS)
			unitDamageK = (unitMaxHealth - ai->cbc->GetUnitHealth(tempTarget)) / unitMaxHealth;
		else
			unitDamageK = 0.0f;
		
		const unsigned int ecats = UC(ud->id);
		float3 epos = ai->cbc->GetUnitPos(tempTarget);
		float dist = ourPos.distance2D(epos);
		dist += (fearFactor * group.getThreat(epos, radius)) - unitDamageK * 150.0f;
		if (ai->defensematrix->isPosInBounds(epos))
			// enemy at our base has higher priority
			dist -= 1000.0f;
		else if(!scout && ecats&SCOUTER) {
			// remote scouts are not interesting for engage groups
			dist += 10000.0f;
		}
		
		if(dist < estimate) {
			estimate = dist;
			target = tempTarget;
		}
	}

	return target;
}

void CMilitary::prepareTargets(std::vector<int> &all, std::vector<int> &harass, std::vector<int> &defense) {
	std::map<int, CTaskHandler::AttackTask*>::iterator j;
	std::vector<int> targets;

	// NOTE: tasks for different tactic groups can overlap

	occupiedTargets.clear();
	for (j = ai->tasks->activeAttackTasks.begin(); j != ai->tasks->activeAttackTasks.end(); j++)
		if (!(j->second->group->cats&SCOUTER))
			occupiedTargets.push_back(j->second->target);
		
	targets.insert(targets.end(), ai->intel->commanders.begin(), ai->intel->commanders.end());
	targets.insert(targets.end(), ai->intel->attackers.begin(), ai->intel->attackers.end());
	targets.insert(targets.end(), ai->intel->energyMakers.begin(), ai->intel->energyMakers.end());
	targets.insert(targets.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());
	targets.insert(targets.end(), ai->intel->defenseAntiAir.begin(), ai->intel->defenseAntiAir.end());
	if (targets.empty()) {
		targets.insert(targets.end(), ai->intel->defenseGround.begin(), ai->intel->defenseGround.end());
		targets.insert(targets.end(), ai->intel->rest.begin(), ai->intel->rest.end());
		targets.insert(targets.end(), ai->intel->mobileBuilders.begin(), ai->intel->mobileBuilders.end());
		targets.insert(targets.end(), ai->intel->factories.begin(), ai->intel->factories.end());
		targets.insert(targets.end(), ai->intel->restUnarmedUnits.begin(), ai->intel->restUnarmedUnits.end());
	}

	filterOccupiedTargets(targets, all);
	targets.clear();

	occupiedTargets.clear();
	for (j = ai->tasks->activeAttackTasks.begin(); j != ai->tasks->activeAttackTasks.end(); j++) {
		if (j->second->group->cats&SCOUTER)
			occupiedTargets.push_back(j->second->target);
	}
	
	targets.insert(targets.end(), ai->intel->mobileBuilders.begin(), ai->intel->mobileBuilders.end());
	targets.insert(targets.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());
	targets.insert(targets.end(), ai->intel->energyMakers.begin(), ai->intel->energyMakers.end());
	targets.insert(targets.end(), ai->intel->restUnarmedUnits.begin(), ai->intel->restUnarmedUnits.end());
	
	filterOccupiedTargets(targets, harass);
	targets.clear();

	occupiedTargets.clear();
	for (j = ai->tasks->activeAttackTasks.begin(); j != ai->tasks->activeAttackTasks.end(); j++) {
		if ((j->second->group->cats&AIR) && (j->second->group->cats&ARTILLERY))
			occupiedTargets.push_back(j->second->target);
	}

	targets.insert(targets.end(), ai->intel->defenseGround.begin(), ai->intel->defenseGround.end());
	targets.insert(targets.end(), ai->intel->commanders.begin(), ai->intel->commanders.end());
	targets.insert(targets.end(), ai->intel->factories.begin(), ai->intel->factories.end());
	if (targets.empty()) {
		targets.insert(targets.end(), ai->intel->energyMakers.begin(), ai->intel->energyMakers.end());
		targets.insert(targets.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());
	}

	filterOccupiedTargets(targets, defense);
	targets.clear();
}

void CMilitary::filterOccupiedTargets(std::vector<int> &source, std::vector<int> &dest) {
	for (size_t i = 0; i < source.size(); i++) {
		bool isOccupied = false;
		int target = source[i];
		
		for (size_t j = 0; j < occupiedTargets.size(); j++) {
			if (target == occupiedTargets[j]) {
				isOccupied = true;
				break;
			}
		}

		if (!isOccupied) 
			dest.push_back(target);
	}
}

void CMilitary::update(int frame) {
	int busyScoutGroups = 0;
	int target = 0;
	std::vector<int> all, harass, defense;
	std::map<int, CGroup*>::iterator i;

	prepareTargets(all, harass, defense);
	
	/* Give idle scouting groups a new attack plan */
	for (i = activeScoutGroups.begin(); i != activeScoutGroups.end(); i++) {
		CGroup *group = i->second;
		
		/* This group is busy, don't bother */
		if (group->busy || !ai->unittable->canPerformTask(*group->firstUnit())) {
			busyScoutGroups++;
			continue;
		}		

		// NOTE: once target is not found it will never appear during
		// current loop execution
		if (target >= 0) {
			target = selectTarget(*group, 300.0f, harass);
			/* There are no harass targets available */
			if (target < 0)
				target = selectTarget(*group, 300.0f, all);
		}
		
		bool assistNearbyTask = target < 0;

		if (!assistNearbyTask) {
			float3 tpos = ai->cbc->GetUnitPos(target);
			float threat = group->getThreat(tpos, 0.0f);
			if (threat < group->strength) {
				mergeGroups.erase(group->key);
				// TODO: what to do with units which can't reach target?
				ai->tasks->addAttackTask(target, *group);
				break;
			}
			else {
				if ((threat / group->strength) <= MAX_SCOUTS_IN_GROUP) {
					if (group->units.size() < MAX_SCOUTS_IN_GROUP
					&& activeScoutGroups.size() > 1)
						mergeGroups[group->key] = group;
					else
						assistNearbyTask = true;
				}
				else
					assistNearbyTask = true;
			}
		}

		if (assistNearbyTask) {
			// try to assist in destroying nearby scout targets...
			if (!ai->tasks->activeAttackTasks.empty()) {
				float minDist = std::numeric_limits<float>::max();
				float3 pos = group->pos();
				ATask* task = NULL;
				std::map<int, CTaskHandler::AttackTask*>::iterator i;
				for (i = ai->tasks->activeAttackTasks.begin(); i != ai->tasks->activeAttackTasks.end(); i++) {
					if (!(i->second->group->cats&SCOUTER))
						continue;
					float targetDist = i->second->pos.distance2D(pos);
					if (targetDist < minDist) {
						task = i->second;
						minDist = targetDist;
					}
				}
				if (task && minDist < 1000.0f) {
					ai->tasks->addAssistTask(*task, *group);
					mergeGroups.erase(group->key);
					break;
				}
			}
		}
	}

	/* Give idle, strong enough groups a new attack plan */
	for (i = activeAttackGroups.begin(); i != activeAttackGroups.end(); i++) {
		CGroup *group = i->second;

		/* This group is busy, don't bother */
		if (group->busy || !ai->unittable->canPerformTask(*group->firstUnit()))
			continue;

		/* Select a target */
		target = selectTarget(*group, 0.0f, all);

		/* There are no targets available, assist an attack */
		if (target < 0) {
			if (!ai->tasks->activeAttackTasks.empty() && group->units.size() > 3) {
				float minStrength = std::numeric_limits<float>::max();
				ATask* task = NULL;
				std::map<int, CTaskHandler::AttackTask*>::iterator i;
				for (i = ai->tasks->activeAttackTasks.begin(); i != ai->tasks->activeAttackTasks.end(); i++) {
					if (i->second->group->strength < minStrength) {
						if (group->canAttack(i->second->target)) {
							task = i->second;
							minStrength = i->second->group->strength;
						}
					}
				}
				if (task) {
					ai->tasks->addAssistTask(*task, *group);
					mergeGroups.erase(group->key);
					break;
				}
			}
			continue;			
		}

		/* Determine if the group has the strength */
		float3 targetpos    = ai->cbc->GetUnitPos(target);
		bool isStrongEnough = group->strength >= group->getThreat(targetpos, 0.0f);
		bool isSizeEnough   = group->units.size() >= ai->cfgparser->getMinGroupSize(group->techlvl);
		bool isAssembling   = isAssemblingGroup(group);

		if (!isAssembling && !isStrongEnough)
			mergeGroups[group->key] = group;

		if ((isAssembling && isSizeEnough) || (!isAssembling && isStrongEnough)) {
			mergeGroups.erase(group->key);
			ai->tasks->addAttackTask(target, *group);
			break;
		}
	}

	/* Give idle, strong enough groups a new attack plan */
	for (i = activeBomberGroups.begin(); i != activeBomberGroups.end(); i++) {
		CGroup *group = i->second;

		/* This group is busy, don't bother */
		if (group->busy || !ai->unittable->canPerformTask(*group->firstUnit()))
			continue;

		bool isStrongEnough = false;

		/* Select a target */
		target = selectTarget(*group, 0.0f, defense);

		if (target >= 0) {
			float3 targetpos = ai->cbc->GetUnitPos(target);
			isStrongEnough = group->strength >= group->getThreat(targetpos, 0.0f);
		}
				
		if (isStrongEnough) {
			mergeGroups.erase(group->key);
			ai->tasks->addAttackTask(target, *group);
			break;
		}
		else
			mergeGroups[group->key] = group;
	}

	/* Merge the groups that were not strong enough */
	if (mergeGroups.size() >= 2) {
		std::map<int,CGroup*> merge;
		for (std::map<int,CGroup*>::iterator base = mergeGroups.begin(); base != mergeGroups.end(); base++) {
			if (!base->second->busy) {
				for (std::map<int,CGroup*>::iterator compare = mergeGroups.begin(); compare != mergeGroups.end(); compare++) {
					if (!compare->second->busy && base->first != compare->first) {
						if (base->second->canMerge(compare->second)) {
							bool canMerge;
							
							if (base->second->cats&SCOUTER)
								canMerge = (base->second->pos().distance2D(compare->second->pos()) < 1000.0f);
							else
								canMerge = true;

							if (canMerge) {
								if (merge.empty())
									merge[base->first] = base->second;
								merge[compare->first] = compare->second;
							}
						}
					}
				}
				
				if (!merge.empty()) {
					ai->tasks->addMergeTask(merge);		
					merge.clear();
				}
			}
		}

		// remove busy (merging) groups...
		std::map<int,CGroup*>::iterator it = mergeGroups.begin();
		while(it != mergeGroups.end()) {
			int key = it->first; it++;
			if (mergeGroups[key]->busy)
				mergeGroups.erase(key);
		}
	}

	bool gotAirFactory = ai->unittable->gotFactory(AIR);
	bool gotSeaFactory = ai->unittable->gotFactory(SEA);
	
	// if all scouts are busy create some more...
	if (busyScoutGroups == activeScoutGroups.size()) {
		unsigned int baseType = ai->gamemap->IsWaterMap() ? SEA : LAND;
		buildPriority priority = activeScoutGroups.size() < ai->cfgparser->getMinScouts() ? HIGH: NORMAL;

		if(gotAirFactory && rng.RandFloat() > 0.66f)
			baseType = AIR;
				
		ai->wishlist->push(baseType | MOBILE | SCOUTER, priority);
	} 

	/* Always build some unit */
	if (ai->gamemap->IsWaterMap()) {
		if(gotAirFactory && rng.RandFloat() > 0.66f)
			ai->wishlist->push(requestUnit(AIR), NORMAL);			
		else
			ai->wishlist->push(requestUnit(SEA), NORMAL);
	}
	else {
		if(gotAirFactory && rng.RandFloat() > 0.66f)
			ai->wishlist->push(requestUnit(AIR), NORMAL);			
		else
			ai->wishlist->push(requestUnit(LAND), NORMAL);
	}
}

unsigned CMilitary::requestUnit(unsigned int basecat) {
	float r = rng.RandFloat();
	std::multimap<float, unitCategory>::iterator i;
	float sum = 0.0f;
	for (i = ai->intel->roulette.begin(); i != ai->intel->roulette.end(); i++) {
		sum += i->first;
		if (r <= sum) {
			return basecat | MOBILE | i->second;
		}
	}
	return basecat | MOBILE | ASSAULT; // unreachable code :)
}

int CMilitary::idleScoutGroupsNum() {
	int result = 0;
	std::map<int, CGroup*>::iterator i;
	for(i = activeScoutGroups.begin(); i != activeScoutGroups.end(); i++)
		if(!i->second->busy)
			result++;
	return result;
}

bool CMilitary::isAssemblingGroup(CGroup *group) {
	std::map<int, CGroup*>::iterator i;
	for (i = assemblingGroups.begin(); i != assemblingGroups.end(); i++) {
		if (i->second->key == group->key) {
			return true;
		}
	}
	return false;	
}
