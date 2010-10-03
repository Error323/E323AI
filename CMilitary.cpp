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
#include "Util.hpp"


CMilitary::CMilitary(AIClasses *ai): ARegistrar(200) {
	this->ai = ai;

	groups[SCOUT] = &activeScoutGroups;
	groups[ENGAGE] = &activeAttackGroups;
	groups[BOMBER] = &activeBomberGroups;
	groups[AIRFIGHTER] = &activeAirFighterGroups;

	drawTasks = false;
}

void CMilitary::remove(ARegistrar &object) {
	CGroup *group = dynamic_cast<CGroup*>(&object);
	
	LOG_II("CMilitary::remove " << (*group))
	
	activeScoutGroups.erase(group->key);
	activeAttackGroups.erase(group->key);
	activeBomberGroups.erase(group->key);
	activeAirFighterGroups.erase(group->key);
	mergeGroups.erase(group->key);
	
	for (std::map<int,CGroup*>::iterator i = assemblingGroups.begin(); i != assemblingGroups.end(); ++i) {
		if (i->second->key == group->key) {
			assemblingGroups.erase(i->first);
			break;
		}
	}

	group->unreg(*this);

	ReusableObjectFactory<CGroup>::Release(group);
}

void CMilitary::addUnit(CUnit& unit) {
	LOG_II("CMilitary::addUnit " << unit)
	
	assert(unit.group == NULL);

	unitCategory c = unit.type->cats;

	if ((c&MOBILE).any() && (c&DEFENSE).none()) {
		unitCategory wishedCats = ai->unittable->unitsUnderConstruction[unit.key];
		CGroup *group;
		
		if ((c&SCOUTER).any() && wishedCats.any() && (wishedCats&SCOUTER).none())
			c &= ~SCOUTER; // scout was not requested

		if ((c&SCOUTER).any()) {
			group = requestGroup(SCOUT);
		}
		else if((c&AIR).any() && (c&ARTILLERY).any()) {
			group = requestGroup(BOMBER);
		}
		else if((c&AIR).any() && (c&ASSAULT).none()) {
			group = requestGroup(AIRFIGHTER);
		}
		else {
			/* If there is a new factory, or the current group is busy, request
			   a new group */
			std::map<int,CGroup*>::iterator i = assemblingGroups.find(unit.builtBy);
			if (i == assemblingGroups.end()	|| i->second->busy || !i->second->canAdd(&unit)) {
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
		case AIRFIGHTER:
			activeAirFighterGroups[group->key] = group;
			break;
		default:
			LOG_EE("CMilitary::requestGroup invalid group behaviour: " << type)
	}

	return group;
}

void CMilitary::update(int frame) {
	int busyScoutGroups = 0;
	std::vector<int> keys;
	std::vector<int> occupied;
	std::map<int, bool> isOccupied;
	std::map<int, ATask*>::iterator itTask;
	std::map<MilitaryGroupBehaviour, std::map<int, CGroup*>* >::iterator itGroup;
	TargetsFilter tf;

	// NOTE: we store occupied targets in two formats because vector is used
	// for list of targets (which can be used if no suitable primary
	// targets are found), second one is used for TargetFilter to filter out 
	// occupied targets when searching for primary targets
	for (itTask = ai->tasks->activeTasks[TASK_ATTACK].begin(); itTask != ai->tasks->activeTasks[TASK_ATTACK].end(); ++itTask) {
		AttackTask *task = (AttackTask*)itTask->second;
		occupied.push_back(task->target);
		isOccupied[task->target] = true;
	}
	
	for(itGroup = groups.begin(); itGroup != groups.end(); itGroup++) {
		int target = -2;
		MilitaryGroupBehaviour behaviour = itGroup->first;
		
		// setup common target filter params per behaviour...
		tf.reset();
		switch(behaviour) {
			case SCOUT:
				tf.threatRadius = 300.0f;
				tf.threatFactor = 1000.0f;
				break;
			case ENGAGE:
				tf.threatFactor = 0.001f;
				break;
			case BOMBER:
				tf.threatFactor = 100.0f;
				// TODO: replace constant with maneuvering radius of plane?
				tf.threatRadius = 1000.0f;
				break;
			case AIRFIGHTER:
				tf.threatFactor = 100.0f;
				break;
		}

		std::vector<std::vector<int>* > *targetBlocks = &(ai->intel->targets[behaviour]);

		// NOTE: start with random group ID because some groups can't reach the 
		// target (e.g. Fleas); this helps to overcome the problem when there 
		// is a target, but first group can't reach it, and AI constantly 
		// trying to add same task again and again which leads to attack stall
		keys.clear();
		util::GetShuffledKeys<int, CGroup*>(keys, *(itGroup->second));
		
		for (int i = 0; i < keys.size(); ++i) {
			CGroup *group = (*(itGroup->second))[keys[i]];

			// if group is busy, don't bother...
			if (group->busy || !group->canPerformTasks()) {
				if (group->busy) {
					if (behaviour == SCOUT)
						busyScoutGroups++;
				
    				if (drawTasks)
    					visualizeTasks(group);
				}

				continue;
			}

			// NOTE: each group can have different score on the same target
			// because of their disposition, strength etc.
			tf.scoreCeiling = std::numeric_limits<float>::max();
			tf.excludeId = &isOccupied;

			// setup custom target filter params per current group...
			switch(behaviour) {
				case SCOUT:
					if ((group->cats&AIR).any())
						tf.threatCeiling = 1.1f;
					else
						tf.threatCeiling = (std::max<float>((float)MAX_SCOUTS_IN_GROUP / group->units.size(), 1.0f)) * group->strength - EPS;
					break;
				case BOMBER:
					tf.threatCeiling = group->strength + group->firstUnit()->type->dps;
					break;
			}

			// basic target selection...
			if (target != -1) {
				for(int b = 0; b < targetBlocks->size(); b++) {
					std::vector<int> *targets = (*targetBlocks)[b];
					target = group->selectTarget(*targets, tf);
				}
			}

			bool isAssembling = isAssemblingGroup(group);
			
			if ((!isAssembling && behaviour == SCOUT) || target < 0) {
				// scan for better target among existing targets...
				tf.excludeId = NULL;
				int assistTarget = group->selectTarget(occupied, tf);
				if (assistTarget >= 0 && assistTarget != target) {
					ATask *task = ai->tasks->getTaskByTarget(assistTarget);
					if (task) {
						bool canAssist = false;
						int assisters = task->assisters.size();
						float cumulativeStrength = task->firstGroup()->strength;
						std::list<ATask*>::iterator itATask;
						for (itATask = task->assisters.begin(); itATask != task->assisters.end(); itATask++) {
							cumulativeStrength += (*itATask)->firstGroup()->strength;
						}

						switch(behaviour) {
							case SCOUT:
								canAssist = assisters == 0;
								break;
							case ENGAGE: 
								canAssist = cumulativeStrength < 2.0f * tf.threatValue;
								break;
							case BOMBER:
								canAssist = assisters < 9;
								break;
							case AIRFIGHTER:
								canAssist = assisters < 3;
								break;
						}
						
						if (canAssist) {
							mergeGroups.erase(group->key);
							if (!ai->tasks->addTask(new AssistTask(ai, *task, *group)))
								group->addBadTarget(assistTarget);
							break;
						}
					} 
				}
			}
			
			bool isStrongEnough = true;
			
			if (target >= 0) {
				isStrongEnough = group->strength >= (tf.threatValue - EPS);
				bool isSizeEnough = (behaviour == ENGAGE) ? group->units.size() >= ai->cfgparser->getMinGroupSize(group->techlvl) : true;

				if (behaviour != ENGAGE)
					isAssembling = false;
				
				if ((isAssembling && isSizeEnough) || (!isAssembling && isStrongEnough)) {
					ATask::NPriority taskPriority = (behaviour == BOMBER || behaviour == AIRFIGHTER) ? ATask::HIGH : ATask::NORMAL;
					mergeGroups.erase(group->key);
					if (ai->tasks->addTask(new AttackTask(ai, target, *group), taskPriority)) {
						occupied.push_back(target);
						isOccupied[target] = true;
					}
					else {
						group->addBadTarget(target);
					}
					break;
				}
			}

			bool bMerge = !(isStrongEnough || isAssembling);
			
			switch(behaviour) {
				case SCOUT:
					bMerge = bMerge && activeScoutGroups.size() > 1 && group->units.size() < MAX_SCOUTS_IN_GROUP;
					break;
				default:
					bMerge = bMerge && groups[behaviour]->size() > 1;
			}
				
			if (bMerge)
				mergeGroups[group->key] = group;
		}
	}

	/* Merge the groups that were not strong enough */
	if (mergeGroups.size() >= 2) {
		std::list<CGroup*> merge;
		for (std::map<int, CGroup*>::iterator base = mergeGroups.begin(); base != mergeGroups.end(); ++base) {
			if (!base->second->busy) {
				for (std::map<int,CGroup*>::iterator compare = mergeGroups.begin(); compare != mergeGroups.end(); ++compare) {
					if (!compare->second->busy && base->first != compare->first) {
						if (base->second->canMerge(compare->second)) {
							bool canMerge = false;
							
							if ((base->second->cats&SCOUTER).any())
								// TODO: replace MERGE_DISTANCE with ETA?
								canMerge = (base->second->pos().distance2D(compare->second->pos()) < MERGE_DISTANCE);
							else
								canMerge = true;

							if (canMerge) {
								if (merge.empty())
									merge.push_back(base->second);
								merge.push_back(compare->second);
								break;
							}
						}
					}
				}
				
				if (!merge.empty()) {
					ai->tasks->addTask(new MergeTask(ai, merge));
					merge.clear();
					break;
				}
			}
		}

		// remove busy (merging) groups...
		std::map<int, CGroup*>::iterator it = mergeGroups.begin();
		while(it != mergeGroups.end()) {
			int key = it->first; ++it;
			if (mergeGroups[key]->busy)
				mergeGroups.erase(key);
		}
	}

	bool gotAirFactory = ai->unittable->gotFactory(AIRCRAFT);
	bool gotSeaFactory = ai->unittable->gotFactory(NAVAL|HOVER);
	
	if (ai->difficulty == DIFFICULTY_HARD) {
		// when all scouts are busy create some more...
		// FIXME: when scouts are stucked AI will not build them anymore,
		// while there are scout targets available
		if (busyScoutGroups == activeScoutGroups.size()) {
			unitCategory baseType = ai->gamemap->IsWaterMap() ? SEA : LAND;
			Wish::NPriority p = activeScoutGroups.size() < ai->cfgparser->getMinScouts() ? Wish::HIGH: Wish::NORMAL;

			if(gotAirFactory && rng.RandFloat() > 0.66f)
				baseType = AIR;
				
			ai->wishlist->push(baseType | MOBILE | SCOUTER, p);
		} 
	}

	/* Always build some unit */
	if(gotAirFactory && rng.RandFloat() > 0.66f) {
		ai->wishlist->push(requestUnit(AIR), Wish::NORMAL);
	}
	else {
		if (ai->gamemap->IsWaterMap())
			ai->wishlist->push(requestUnit(SEA), Wish::NORMAL);
		else
			ai->wishlist->push(requestUnit(LAND), Wish::NORMAL);
	}	
}

unitCategory CMilitary::requestUnit(unitCategory basecat) {
	float r = rng.RandFloat();
	float sum = 0.0f;
	std::multimap<float, unitCategory>::iterator i;
	
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
	for(i = activeScoutGroups.begin(); i != activeScoutGroups.end(); ++i)
		if(!i->second->busy)
			result++;
	return result;
}

bool CMilitary::isAssemblingGroup(CGroup *group) {
	std::map<int, CGroup*>::iterator i;
	for (i = assemblingGroups.begin(); i != assemblingGroups.end(); ++i) {
		if (i->second->key == group->key) {
			return true;
		}
	}
	return false;	
}

void CMilitary::onEnemyDestroyed(int enemy, int attacker) {
	std::map<int, CGroup*> *activeGroups;
	std::map<int, CGroup*>::iterator itGroup;
	std::map<MilitaryGroupBehaviour, std::map<int, CGroup*>* >::iterator itGroups;

	for (itGroups = groups.begin(); itGroups != groups.end(); ++itGroups) {
		activeGroups = itGroups->second;
		for (itGroup = activeGroups->begin(); itGroup != activeGroups->end(); ++itGroup) {
			if (!itGroup->second->badTargets.empty()) {
				LOG_II("CMilitary::onEnemyDestroyed bad target Unit(" << enemy << ") destroyed for " << (*(itGroup->second)))
				itGroup->second->badTargets.erase(enemy);
			}
		}
	}
}

bool CMilitary::switchDebugMode() {
	drawTasks = !drawTasks;
	return drawTasks;
}

void CMilitary::visualizeTasks(CGroup *g) {
	ATask *task = ai->tasks->getTask(*g);
    
	if (task == NULL)
		return;
    
    float R, G, B;
    switch(task->t) {
    	case TASK_ATTACK:
    		R = 1.0f; G = 0.0f; B = 0.0f;
    		break;
    	case TASK_MERGE:
    		R = 1.0f; G = 1.0f; B = 0.0f;
    		break;
    	case TASK_ASSIST:
    		R = 1.0f; G = 0.0f; B = 1.0f;
    		break;
    	default:
    		R = G = B = 0.0f;
    }

	float3 fp = g->pos();
	fp.y = ai->cb->GetElevation(fp.x, fp.z) + 50.0f;
	float3 fn = task->pos;
	fn.y = ai->cb->GetElevation(fn.x, fn.z) + 50.0f;
	// draw arrow for MERGE task only because otherwise it looks ugly (arrow
	// is stretched, not fixed in size)
	ai->cb->CreateLineFigure(fp, fn, 6.0f, task->t == TASK_MERGE ? 1 : 0, MULTIPLEXER, task->t);
	ai->cb->SetFigureColor(task->t, R, G, B, 0.5f);
}
