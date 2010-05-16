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

CMilitary::CMilitary(AIClasses *ai): ARegistrar(200, std::string("military")) {
	this->ai = ai;

	groups[SCOUT] = &activeScoutGroups;
	groups[ENGAGE] = &activeAttackGroups;
	groups[BOMBER] = &activeBomberGroups;

	drawTasks = false;
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

void CMilitary::update(int frame) {
	int busyScoutGroups = 0;
	std::vector<int> keys, occupied;
	std::map<int, bool> isOccupiedTarget;
	std::map<int, CTaskHandler::AttackTask*>::iterator itTask;
	std::map<MilitaryGroupBehaviour, std::map<int, CGroup*>* >::iterator itGroup;
	TargetsFilter tf;

	// NOTE: occupied targets are being analyzed only if there are no suitable
	// primary targets
	for (itTask = ai->tasks->activeAttackTasks.begin(); itTask != ai->tasks->activeAttackTasks.end(); itTask++) {
		occupied.push_back(itTask->second->target);
		isOccupiedTarget[itTask->second->target] = true;
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
		}

		std::vector<std::vector<int>* > *targetBlocks = &(ai->intel->targets[behaviour]);

		// NOTE: start with random group ID because some groups can't reach the 
		// target (e.g. Fleas); this helps to overcome the problem when there 
		// is a target, but first group can't reach it, and AI constantly trying to add 
		// same task again and again which leads to attack stall
		keys.clear();
		util::GetShuffledKeys<int, CGroup*>(keys, *(itGroup->second));
		
		for (int i = 0; i < keys.size(); i++) {
			CGroup *group = (*(itGroup->second))[keys[i]];

			// if group is busy, don't bother...
			if (group->busy || !ai->unittable->canPerformTask(*group->firstUnit())) {
				if (group->busy) {
					if (behaviour == SCOUT)
						busyScoutGroups++;
				
    				if (drawTasks)
    					visualizeTasks(group);
				
					continue;
				}
			}

			// NOTE: each group can have different score on the same target
			// because of their disposition, strength etc.
			tf.scoreCeiling = std::numeric_limits<float>::max();

			// setup custom target filter params per current group...
			switch(behaviour) {	
				case SCOUT:
					if (group->cats&AIR)
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
					target = group->selectTarget(*targets, isOccupiedTarget, tf);
				}
			}

			bool isAssembling = isAssemblingGroup(group);
			
			if ((!isAssembling && behaviour == SCOUT) || target < 0) {
				// scan for better target among existing targets...
				std::map<int, bool> dummy;
				int assistTarget = group->selectTarget(occupied, dummy, tf);
				if (assistTarget >= 0 && assistTarget != target) {
					ATask *task = ai->tasks->getTaskByTarget(assistTarget);
					if (task) {
						bool canAssist = false;
						int assisters = task->assisters.size();
						float cumulativeStrength = task->group->strength;
						std::list<ATask*>::iterator itATask;
						for (itATask = task->assisters.begin(); itATask != task->assisters.end(); itATask++) {
							cumulativeStrength += (*itATask)->group->strength;
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
						}
						
						if (canAssist) {
							mergeGroups.erase(group->key);
							ai->tasks->addAssistTask(*task, *group);
							break;
						}
					} 
				}
			}
			
			bool isStrongEnough = true;
			
			if (target >= 0) {
				isStrongEnough = group->strength >= (tf.threatValue - EPS);
				bool isSizeEnough = behaviour == ENGAGE ? group->units.size() >= ai->cfgparser->getMinGroupSize(group->techlvl) : true;

				if (behaviour != ENGAGE)
					isAssembling = false;
				
				if ((isAssembling && isSizeEnough) || (!isAssembling && isStrongEnough)) {
					mergeGroups.erase(group->key);
					if (ai->tasks->addAttackTask(target, *group, behaviour == BOMBER)) {
						occupied.push_back(target);
						isOccupiedTarget[target] = true;
					}
					break;
				}
			}

			bool bMerge = !(isStrongEnough || isAssembling);
			
			switch(behaviour) {
				case SCOUT:
					bMerge = bMerge && activeScoutGroups.size() > 1 && group->units.size() < MAX_SCOUTS_IN_GROUP;
					break;
				case ENGAGE:
					bMerge = bMerge && activeAttackGroups.size() > 1;
					break;
				case BOMBER:
					bMerge = bMerge && activeBomberGroups.size() > 1;
					break;
			}
				
			if (bMerge)
				mergeGroups[group->key] = group;
		}
	}

	/* Merge the groups that were not strong enough */
	if (mergeGroups.size() >= 2) {
		std::map<int,CGroup*> merge;
		for (std::map<int,CGroup*>::iterator base = mergeGroups.begin(); base != mergeGroups.end(); base++) {
			if (!base->second->busy) {
				for (std::map<int,CGroup*>::iterator compare = mergeGroups.begin(); compare != mergeGroups.end(); compare++) {
					if (!compare->second->busy && base->first != compare->first) {
						if (base->second->canMerge(compare->second)) {
							bool canMerge = false;
							
							if (base->second->cats&SCOUTER)
								// TODO: replace MERGE_DISTANCE with ETA?
								canMerge = (base->second->pos().distance2D(compare->second->pos()) < MERGE_DISTANCE);
							else
								canMerge = true;

							if (canMerge) {
								if (merge.empty())
									merge[base->first] = base->second;
								merge[compare->first] = compare->second;
								break;
							}
						}
					}
				}
				
				if (!merge.empty()) {
					ai->tasks->addMergeTask(merge);
					merge.clear();
					break;
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

unsigned int CMilitary::requestUnit(unsigned int basecat) {
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
    	case ATTACK:
    		R = 1.0f; G = 0.0f; B = 0.0f;
    		break;
    	case MERGE:
    		R = 1.0f; G = 1.0f; B = 0.0f;
    		break;
    	case ASSIST:
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
	ai->cb->CreateLineFigure(fp, fn, 6.0f, task->t == MERGE ? 1 : 0, MULTIPLEXER, task->t);
	ai->cb->SetFigureColor(task->t, R, G, B, 0.5f);
}