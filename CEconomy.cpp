#include "CEconomy.h"

#include <cfloat>
#include <limits>

#include "headers/HEngine.h"

#include "CAI.h"
#include "CRNG.h"
#include "CTaskHandler.h"
#include "CUnitTable.h"
#include "GameMap.hpp"
#include "CWishList.h"
#include "CDefenseMatrix.h"
#include "CGroup.h"
#include "CUnit.h"
#include "CPathfinder.h"
#include "CConfigParser.h"
#include "CIntel.h"
#include "GameMap.hpp"
#include "ReusableObjectFactory.hpp"
#include "CCoverageHandler.h"

CEconomy::UnitCategory2UnitCategoryMap CEconomy::canBuildEnv;

CEconomy::CEconomy(AIClasses *ai): ARegistrar(700) {
	this->ai = ai;
	state = 0;
	incomes  = 0;
	mNow     = mNowSummed     = eNow     = eNowSummed     = 0.0f;
	mIncome  = mIncomeSummed  = eIncome  = eIncomeSummed  = 0.0f;
	uMIncome = uMIncomeSummed = uEIncome = uEIncomeSummed = 0.0f;
	mUsage   = mUsageSummed   = eUsage   = eUsageSummed   = 0.0f;
	mStorage = eStorage                                   = 0.0f;
	mstall = estall = mexceeding = eexceeding = mRequest = eRequest = false;
	initialized = stallThresholdsReady = false;
	utCommander = NULL;
	areMMakersEnabled = ai->gamemap->IsMetalFreeMap();
	windmap = worthBuildingTidal = false;

	if (canBuildEnv.empty()) {
		canBuildEnv[AIR] = LAND|SEA|SUB; // TODO: -SUB?
		canBuildEnv[LAND] = LAND; // TODO: +SEA?
		canBuildEnv[SEA] = SEA|SUB; // TODO: +LAND?
		canBuildEnv[SUB] = SEA|SUB;
	}
}

void CEconomy::init(CUnit &unit) {
	if (initialized) return;
	// NOTE: expecting "unit" is a commander unit
	const UnitDef *ud = ai->cb->GetUnitDef(unit.key);
	
	utCommander = UT(ud->id);
	windmap = ((ai->cb->GetMaxWind() + ai->cb->GetMinWind()) / 2.0f) >= 10.0f;
	//float avgWind   = (ai->cb->GetMinWind() + ai->cb->GetMaxWind()) / 2.0f;
	//float windProf  = avgWind / utWind->cost;
	//float solarProf = utSolar->energyMake / utSolar->cost;
	worthBuildingTidal = ai->cb->GetTidalStrength() > 5.0f;
	
	initialized = true;
}
		
bool CEconomy::hasBegunBuilding(CGroup &group) const {
	std::map<int, CUnit*>::const_iterator i;
	for (i = group.units.begin(); i != group.units.end(); ++i) {
		CUnit *unit = i->second;
		if (ai->unittable->idle.find(unit->key) == ai->unittable->idle.end()
		|| !ai->unittable->idle[unit->key])
			return true;
	}
	
	return false;
}

bool CEconomy::hasFinishedBuilding(CGroup &group) {
	std::map<int, CUnit*>::iterator i;
	for (i = group.units.begin(); i != group.units.end(); i++) {
		CUnit *unit = i->second;
		if (ai->unittable->builders.find(unit->key) != ai->unittable->builders.end() &&
			ai->unittable->builders[unit->key]) {
			ai->unittable->builders[unit->key] = false;
			return true;
		}
	}
	
	return false;
}

CGroup* CEconomy::requestGroup() {
	CGroup *group = ReusableObjectFactory<CGroup>::Instance();
	group->ai = ai;
	group->reset();
	group->reg(*this);

	activeGroups[group->key] = group;
	
	return group;
}

void CEconomy::remove(ARegistrar &object) {
	CGroup *group = dynamic_cast<CGroup*>(&object);
	LOG_II("CEconomy::remove " << (*group))

	activeGroups.erase(group->key);
	takenMexes.erase(group->key);
	takenGeo.erase(group->key);

	group->unreg(*this);
	
	ReusableObjectFactory<CGroup>::Release(group);
}

void CEconomy::addUnitOnCreated(CUnit& unit) {
	unitCategory c = unit.type->cats;
	if ((c&MEXTRACTOR).any()) {
		CGroup *group = requestGroup();
		group->addUnit(unit);
		takenMexes[group->key] = group->pos();
		CUnit *builder = ai->unittable->getUnit(group->firstUnit()->builtBy);
		if (builder) {
			assert(group->key != builder->group->key);
			takenMexes.erase(builder->group->key);
		}
	}
	else if(unit.type->def->needGeo) {
		CGroup *group = requestGroup();
		group->addUnit(unit);
		takenGeo[group->key] = group->pos();	
		CUnit *builder = ai->unittable->getUnit(group->firstUnit()->builtBy);
		if (builder) {
			assert(group->key != builder->group->key);
			takenGeo.erase(builder->group->key);
		}
	}
}

void CEconomy::addUnitOnFinished(CUnit &unit) {
	LOG_II("CEconomy::addUnitOnFinished " << unit)

	unitCategory c = unit.type->cats;

	if ((c&BUILDER).any() || ((c&ASSISTER).any() && (c&MOBILE).any())) {
		CGroup *group = requestGroup();
		group->addUnit(unit);
	}
}

void CEconomy::buildOrAssist(CGroup& group, buildType bt, unitCategory include, unitCategory exclude) {
	ATask* task = canAssist(bt, group);
	if (task != NULL) {
		ai->tasks->addTask(new AssistTask(ai, *task, group));
		return;
	}
	
	if ((group.cats&BUILDER).none())
		return;

	float3 pos = group.pos();
	float3 goal = pos;
	unitCategory catsWhere = canBuildWhere(group.cats);
	unitCategory catsWhereStrict = canBuildWhere(group.cats, true);

	if (bt == BUILD_EPROVIDER) {
		if (!windmap)
			exclude |= WIND;
		if (!worthBuildingTidal)
			exclude |= TIDAL;
	}
	else if (bt == BUILD_MPROVIDER) {
		if ((include&MEXTRACTOR).any()) {
			goal = getClosestOpenMetalSpot(group);
			if (goal != ERRORVECTOR)
				if (goal.y < 0.0f)
					exclude |= (LAND|AIR);
				else
					exclude |= (SEA|SUB);
		}
	}

	const CUnit* unit = group.firstUnit();
	bool isComm = (unit->type->cats&COMMANDER).any();
	std::multimap<float, UnitType*> candidates;
	
	// retrieve the allowed buildable units...
	if ((include&MEXTRACTOR).any())
		ai->unittable->getBuildables(unit->type, include|catsWhereStrict, exclude, candidates);
	else		
		ai->unittable->getBuildables(unit->type, include|catsWhere, exclude, candidates);
	
   	std::multimap<float, UnitType*>::iterator i = candidates.begin();
   	int iterations = candidates.size() / (ai->cfgparser->getTotalStates() - state + 1);
   	bool affordable = false;

	if (!candidates.empty()) {
    	/* Determine which of these we can afford */
    	while(iterations >= 0) {
    		if (canAffordToBuild(unit->type, i->second))
    			affordable = true;
    		else
    			break;

    		if (i == --candidates.end())
    			break;
    		iterations--;
    		i++;
    	}
	}
	else if (bt != BUILD_MPROVIDER) {
		return;
	}
	else {
		goal = ERRORVECTOR;
	}
	
	/* Perform the build */
	switch(bt) {
		case BUILD_EPROVIDER: {
			if (i->second->def->needGeo) {
				goal = getClosestOpenGeoSpot(group);
				if (goal != ERRORVECTOR) {
					// TODO: prevent commander walking?
					if (!eRequest && ai->pathfinder->getETA(group, goal) > 450.0f) {
						goal = ERRORVECTOR;
					}
				}
			}
			else if ((i->second->cats&EBOOSTER).any()) {
				goal = ai->coverage->getNextClosestBuildSite(unit, i->second);
			}

			if (goal == ERRORVECTOR) {
				goal = pos;
				if (i != candidates.begin())
					--i;
				else
					++i;
			}
			
			if (i != candidates.end())
				ai->tasks->addTask(new BuildTask(ai, bt, i->second, group, goal));

			break;
		}

		case BUILD_MPROVIDER: {
			bool canBuildMMaker = (eIncome - eUsage) >= METAL2ENERGY || eexceeding;
			
			if (goal != ERRORVECTOR) {
				bool allowDirectTask = true;
				
				// TODO: there is a flaw in logic because when spot is under
				// threat we anyway send builders there
				
				if (isComm) {
					int numOtherBuilders = ai->unittable->builders.size() - ai->unittable->factories.size();
					// if commander can build mmakers and there is enough
					// ordinary builders then prevent it walking for more than 
					// 20 sec...
					if (numOtherBuilders > 2 && ai->unittable->canBuild(unit->type, MMAKER))
						allowDirectTask = ai->pathfinder->getETA(group, goal) < 600.0f;
				}

				if (allowDirectTask) {
					ai->tasks->addTask(new BuildTask(ai, bt, i->second, group, goal));
				}
				else if (mstall && (isComm || areMMakersEnabled) && canBuildMMaker) {
					UnitType* mmaker = ai->unittable->canBuild(unit->type, MMAKER);
					if (mmaker != NULL)
						ai->tasks->addTask(new BuildTask(ai, bt, mmaker, group, pos));
				}
			}
			else if (mstall && areMMakersEnabled && canBuildMMaker) {
				UnitType* mmaker = ai->unittable->canBuild(unit->type, MMAKER);
				if (mmaker != NULL)
					ai->tasks->addTask(new BuildTask(ai, bt, mmaker, group, pos));
			}
			else if (!eexceeding) {
				buildOrAssist(group, BUILD_EPROVIDER, EMAKER);
			}
			break;
		}

		case BUILD_MSTORAGE: case BUILD_ESTORAGE: {
			/* Start building storage after enough ingame time */
			if (!taskInProgress(bt) && ai->cb->GetCurrentFrame() > 30*60*7) {
				pos = ai->defensematrix->getBestDefendedPos(0);
				ai->tasks->addTask(new BuildTask(ai, bt, i->second, group, pos));
			}
			break;
		}

		case BUILD_FACTORY: {
			if (!taskInProgress(bt)) {
				int numFactories = ai->unittable->factories.size();

				bool build = numFactories <= 0;
				
				// TODO: add some delay before building next factory

				if (!build && affordable && !eRequest) {
					float m = mNow / mStorage;
			
					switch(state) {
						case 0: {
							build = (m > 0.45f);
							break;
						}
						case 1: {
							build = (m > 0.40f);
							break;
						}
						case 2: {
							build = (m > 0.35f);
							break;
						}
						case 3: {
							build = (m > 0.3f);
							break;
						}
						case 4: {
							build = (m > 0.25f);
							break;
						}
						case 5: {
							build = (m > 0.2f);
							break;
						}
						case 6: {
							build = (m > 0.15f);
							break;
						}
						default: {
							build = (m > 0.1f);
						}
					}
				}

				if (build) {
					// TODO: fix getBestDefendedPos() for static builders
					if (numFactories > 1 && (unit->type->cats&MOBILE).any()) {
						goal = ai->coverage->getClosestDefendedPos(pos);
						if (goal == ERRORVECTOR) {
							goal = pos;
						}
					}
					ai->tasks->addTask(new BuildTask(ai, bt, i->second, group, goal));
				}
			}
			break;
		}
		
		case BUILD_IMP_DEFENSE: {
			// NOTE: important defense placement selects important place,
			// not closest one as other BUILD_XX_DEFENSE algoes do
			if (!taskInProgress(bt)) {
				bool allowTask = true;
				
				// TODO: implement in getNextXXXBuildSite() "radius" argument
				// and fill it for static builders
            	goal = ai->coverage->getNextImportantBuildSite(i->second);
				
				allowTask = (goal != ERRORVECTOR);

				if (allowTask && isComm)
					allowTask = ai->pathfinder->getETA(group, goal) < 300.0f;
				
				if (allowTask)
					ai->tasks->addTask(new BuildTask(ai, bt, i->second, group, goal));
			}
			break;
		}
		
		case BUILD_AG_DEFENSE: case BUILD_AA_DEFENSE: case BUILD_UW_DEFENSE: case BUILD_MISC_DEFENSE: {
			if (affordable && !taskInProgress(bt)) {
				bool allowTask = true;
				
				// TODO: implement in getNextBuildSite() "radius" argument
				// and fill it for static builders
				goal = ai->coverage->getNextClosestBuildSite(unit, i->second);
				
				allowTask = (goal != ERRORVECTOR);

				if (allowTask && isComm)
					allowTask = ai->pathfinder->getETA(group, goal) < 300.0f;
				
				if (allowTask)
					ai->tasks->addTask(new BuildTask(ai, bt, i->second, group, goal));
			}
			break;
		}

		default: {
			if (affordable && !taskInProgress(bt))
				ai->tasks->addTask(new BuildTask(ai, bt, i->second, group, goal));
			break;
		}
	}
}

float3 CEconomy::getBestSpot(CGroup& group, std::list<float3>& resources, std::map<int, float3>& tracker, bool metal) {
	bool staticBuilder = (group.cats&STATIC).any();
	bool canBuildUnderWater = (group.cats&(SEA|SUB|AIR)).any();
	bool canBuildAboveWater = (group.cats&(LAND|AIR)).any();
	float bestDist = std::numeric_limits<float>::max();
	float3 bestSpot = ERRORVECTOR;
	float3 gpos = group.pos();
	float radius;
	
	if (metal)
		radius = ai->cb->GetExtractorRadius();
	else
		radius = 16.0f;

	std::list<float3>::iterator i;
	std::map<int, float3>::iterator j;
	for (i = resources.begin(); i != resources.end(); ++i) {
		if (i->y < 0.0f) {
			if (!canBuildUnderWater)
				continue;
		}
		else {
			if (!canBuildAboveWater)
				continue;
		}
		
		bool taken = false;
		for (j = tracker.begin(); j != tracker.end(); ++j) {
			if (i->distance2D(j->second) < radius) {
				taken = true;
				break;
			}
		}
		if (taken) continue; // already taken or scheduled by current AI

		int numUnits = ai->cb->GetFriendlyUnits(&ai->unitIDs[0], *i, 1.1f * radius);
		for (int u = 0; u < numUnits; u++) {
			const int uid = ai->unitIDs[u];
			const UnitDef *ud = ai->cb->GetUnitDef(uid);
			if (metal)
				taken = (UC(ud->id) & MEXTRACTOR).any();
			else
				taken = ud->needGeo;
			if (taken) break;
		}
		if (taken) continue; // already taken by ally team

		float dist = gpos.distance2D(*i);

		if (staticBuilder) {
			if (dist > group.buildRange)
				continue; // spot is out of range
		}
		
		/*
		// NOTE: getPathLength() also considers static units
		float dist = ai->pathfinder->getPathLength(group, *i);
		if (dist < 0.0f)
			continue; // spot is out of build range or unreachable
		*/

		// TODO: actually any spot with any threat should be skipped; 
		// to implement this effectively we need to refactor tasks, cause
		// builder during approaching should scan target place for threat
		// periodically; currently it does not, so skipping dangerous spot
		// has no real profit
		
		dist += 1000.0f * group.getThreat(*i, 300.0f);
		if (dist < bestDist) {
			bestDist = dist;
			bestSpot = *i;
		}
	}

	if (bestSpot != ERRORVECTOR)
		// TODO: improper place for this
		tracker[group.key] = bestSpot;

	return bestSpot;
}

float3 CEconomy::getClosestOpenMetalSpot(CGroup &group) {
	return getBestSpot(group, GameMap::metalspots, takenMexes, true);
}

float3 CEconomy::getClosestOpenGeoSpot(CGroup &group) {
	return getBestSpot(group, GameMap::geospots, takenGeo, false);
}

void CEconomy::update() {
	int buildersCount = 0;
	int assistersCount = 0;
	int maxTechLevel = ai->cfgparser->getMaxTechLevel();

	/* See if we can improve our eco by controlling metalmakers */
	controlMetalMakers();

	/* If we are stalling, do something about it */
	preventStalling();

	/* Update idle worker groups */
	std::map<int, CGroup*>::iterator i;
	for (i = activeGroups.begin(); i != activeGroups.end(); ++i) {
		CGroup *group = i->second;
		CUnit *unit = group->firstUnit();

		if ((group->cats&MOBILE).any() && (group->cats&BUILDER).any())
			buildersCount++;
		if ((group->cats&MOBILE).any() && (group->cats&ASSISTER).any() && (group->cats&BUILDER).none())
			assistersCount++;

		if (group->busy || !group->canPerformTasks())
			continue;

		if ((group->cats&FACTORY).any()) {
			ai->tasks->addTask(new FactoryTask(ai, *group));
			continue;
		}
		
		if ((group->cats&STATIC).any() && (group->cats&BUILDER).none()) {
			// we don't have a task for current unit type yet (these types are:
			// MEXTRACTOR & geoplant)
			continue;
		}

		float3 pos = group->pos();

		// NOTE: we're using special algo for commander to prevent
		// it walking outside the base
		if ((unit->type->cats&COMMANDER).any()) {
			tryFixingStall(group);
			if (group->busy) continue;
			
			/* If we don't have a factory, build one */
			if (ai->unittable->factories.empty() || mexceeding) {
				unitCategory factory = getNextTypeToBuild(unit, FACTORY, maxTechLevel);
				if (factory.any())
					buildOrAssist(*group, BUILD_FACTORY, factory);
				if (group->busy) continue;
			}

			/* If we are exceeding and don't have estorage yet, build estorage */
			if (eexceeding && !ai->unittable->factories.empty()) {
				if (ai->unittable->energyStorages.size() < ai->cfgparser->getMaxTechLevel())
					buildOrAssist(*group, BUILD_ESTORAGE, ESTORAGE);
				if (group->busy) continue;
			}
			
			// NOTE: in NOTA only static commanders can build TECH1 factories
			if ((unit->type->cats&STATIC).any()) {
				/* See if this unit can build desired factory */
				unitCategory factory = getNextTypeToBuild(unit, FACTORY, maxTechLevel);
				if (factory.any())
					buildOrAssist(*group, BUILD_FACTORY, factory);
				if (group->busy) continue;

				factory = getNextTypeToBuild(unit, BUILDER|STATIC, maxTechLevel);
				if (factory.any())
					// TODO: invoke BUILD_ASSISTER building algo instead of
					// BUILD_FACTORY to properly place static builders?
					buildOrAssist(*group, BUILD_FACTORY, factory);
				if (group->busy) continue;
			}
			
			// build nearby metal extractors if possible...
			if (mstall && !ai->gamemap->IsMetalMap()) {
				// NOTE: there is a special hack withing buildOrAssist() 
				// to prevent building unnecessary MMAKERS
				buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR);
				if (group->busy) continue;
			}
			
			// see if we can build defense
			tryBuildingDefense(group);
			if (group->busy) continue;
			
			tryAssistingFactory(group);
			if (group->busy) continue;
		}
		else if ((unit->type->cats&BUILDER).any()) {
			tryFixingStall(group);
			if (group->busy) continue;
    		
    		/* See if this unit can build desired factory */
    		unitCategory factory = getNextTypeToBuild(unit, FACTORY, maxTechLevel);
    		if (factory.any())
    			buildOrAssist(*group, BUILD_FACTORY, factory);
    		if (group->busy) continue;
    		
    		/* If we are overflowing energy build an estorage */
    		if (eexceeding && !mRequest) {
    			if (ai->unittable->energyStorages.size() < ai->cfgparser->getMaxTechLevel())
    				buildOrAssist(*group, BUILD_ESTORAGE, ESTORAGE);
    			if (group->busy) continue;
    		}

    		/* If we are overflowing metal build an mstorage */
    		if (mexceeding && !eRequest) {
    			buildOrAssist(*group, BUILD_MSTORAGE, MSTORAGE);
    			if (group->busy) continue;
    		}

    		/* If both requested, see what is required most */
    		if (eRequest && mRequest) {
    			if ((mNow / mStorage) > (eNow / eStorage))
    				buildOrAssist(*group, BUILD_EPROVIDER, EMAKER);
    			else
    				buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR);
    			if (group->busy) continue;
    		}
    		
    		/* See if we can build defense */
    		tryBuildingDefense(group);
    		if (group->busy) continue;
    		
    		tryBuildingAntiNuke(group);
    		if (group->busy) continue;
    		
			tryBuildingJammer(group);
			if (group->busy) continue;
    		
    		tryBuildingStaticAssister(group);
    		if (group->busy) continue;
    		
    		/* Else just provide what is requested */
    		if (eRequest) {
    			buildOrAssist(*group, BUILD_EPROVIDER, EMAKER);
    			if (group->busy) continue;
    		}
    		
    		if (mRequest) {
    			buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR);
    			if (group->busy) continue;
    		}

    		tryAssistingFactory(group);
    		if (group->busy) continue;
    		
    		tryBuildingShield(group);
    		if (group->busy) continue;

    		/* Otherwise just expand */
    		if (!ai->gamemap->IsMetalMap()) {
    			if ((mNow / mStorage) > (eNow / eStorage))
    				buildOrAssist(*group, BUILD_EPROVIDER, EMAKER);
    			else
    				buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR);
    		}
		}
		else if ((unit->type->cats&ASSISTER).any()) {
			// TODO: repair damaged buildings (& non-moving units?)
			// TODO: finish unfinished bulidings
			tryAssist(group, BUILD_IMP_DEFENSE);
			if (group->busy) continue;
    		tryAssistingFactory(group);
    		if (group->busy) continue;
			tryAssist(group, BUILD_AG_DEFENSE);
			if (group->busy) continue;
			tryAssist(group, BUILD_AA_DEFENSE);
			if (group->busy) continue;
			tryAssist(group, BUILD_UW_DEFENSE);
			if (group->busy) continue;
			tryAssist(group, BUILD_MISC_DEFENSE);
			if (group->busy) continue;
		}
	}

	// TODO: consider assistersCount & military groups count for
	// requesting assisters

	if (buildersCount < ai->cfgparser->getMaxWorkers()
	&& (buildersCount < ai->cfgparser->getMinWorkers()))
		ai->wishlist->push(BUILDER, 0, Wish::HIGH);
	else {
		if (buildersCount < ai->cfgparser->getMaxWorkers())
			ai->wishlist->push(BUILDER, 0, Wish::NORMAL);
	}
}

bool CEconomy::taskInProgress(buildType bt) {
	int tasksCounter = 0;
	int maxTechLevel = ai->cfgparser->getMaxTechLevel();
	std::map<int, ATask*>::iterator it;
	
	for (it = ai->tasks->activeTasks[TASK_BUILD].begin(); it != ai->tasks->activeTasks[TASK_BUILD].end(); ++it) {
		if (((BuildTask*)(it->second))->bt == bt) {
			tasksCounter++;
		}
	}
	
	if (tasksCounter > 0) {
		/*
		switch (ai->difficulty) {
			case DIFFICULTY_EASY:
				return true;
			case DIFFICULTY_NORMAL:
			    if (maxTechLevel > MIN_TECHLEVEL && !(mRequest || eRequest))
					return (tasksCounter >= maxTechLevel);
				break;
			case DIFFICULTY_HARD:
				if (!(mRequest || estall))
					return (tasksCounter >= maxTechLevel * 2);
				break;
		}
		*/
		return true;
	}	
	
	return false;
}

void CEconomy::controlMetalMakers() {
	float eRatio = eNow / eStorage;

	if (eRatio < 0.3f) {
		int count = ai->unittable->setOnOff(ai->unittable->metalMakers, false);
		if (count > 0) {
			if (METAL2ENERGY < 1.0f)
				estall = false;
			areMMakersEnabled = false;
			return;
		}
	}

	if (eRatio > 0.7f) {
		int count = ai->unittable->setOnOff(ai->unittable->metalMakers, true);
		if (count > 0) {
			if (METAL2ENERGY > 1.0f)
				mstall = false;
			areMMakersEnabled = true;
			return;
		}
	}
}

void CEconomy::preventStalling() {
	bool taskRemoved = false;
		// for tracking so only one task is removed per update
	bool needMStallFixing, needEStallFixing;
	AssistTask
		*taskWithPowerBuilder = NULL,
		*taskWithFastBuilder = NULL;
	std::map<int, ATask*>::iterator it;

	// if we're not stalling, return...
	if (!mstall && !estall) {
		// if factorytask is on wait, unwait it...
		for (it = ai->tasks->activeTasks[TASK_FACTORY].begin(); it != ai->tasks->activeTasks[TASK_FACTORY].end(); ++it) {
			FactoryTask *task = (FactoryTask*)it->second;
			task->setWait(false);
		}
		return;
	}

	needMStallFixing = mstall;
	needEStallFixing = estall;

	// check if there are tasks which are fixing current resource stall problems...
	for (it = ai->tasks->activeTasks[TASK_BUILD].begin(); it != ai->tasks->activeTasks[TASK_BUILD].end(); ++it) {
		BuildTask *task = (BuildTask*)it->second;
		if (needMStallFixing && task->bt == BUILD_MPROVIDER)
			needMStallFixing = false;
		if (needEStallFixing && task->bt == BUILD_EPROVIDER)
			needEStallFixing = false;			
	}

	if (needMStallFixing || needEStallFixing) {
    	// stop all guarding workers which do not help in fixing the problem...
    	it = ai->tasks->activeTasks[TASK_ASSIST].begin();
    	while (it != ai->tasks->activeTasks[TASK_ASSIST].end()) {
    		AssistTask *task = (AssistTask*)it->second; ++it;
    		
    		// if the assisting group is moving, skip it...
    		if (task->isMoving)
    			continue;
    		
    		if (task->assist->t == TASK_BUILD) {
    			// ignore builders which are fixing the problems...
    			BuildTask* build = dynamic_cast<BuildTask*>(task->assist);
    			if ((mstall || mRequest) && build->bt == BUILD_MPROVIDER)
    				continue;
    			if ((estall || eRequest) && build->bt == BUILD_EPROVIDER)
    				continue;

    			task->remove(); taskRemoved = true;

    			break;
    		}

    		// detect fastest & most powerful builders among factory assisters...
    		if (task->assist->t == TASK_FACTORY) {
    			if (taskWithPowerBuilder) {
    				if (task->firstGroup()->firstUnit()->type->def->buildSpeed > taskWithPowerBuilder->firstGroup()->firstUnit()->type->def->buildSpeed)
    					taskWithPowerBuilder = task;
    			}
    			else
    				taskWithPowerBuilder = task;

    			if (taskWithFastBuilder) {
    				// NOTE: after commander walking has been reduced it has 
    				// limited power to solve mstall problem
    				if ((task->firstGroup()->cats&COMMANDER).none() 
    				&& task->firstGroup()->speed > taskWithFastBuilder->firstGroup()->speed)
    					taskWithFastBuilder = task;
    			}
    			else
    				taskWithFastBuilder = task;
    		}
    	}
	}

	if (!taskRemoved && needMStallFixing && taskWithFastBuilder) {
		taskWithFastBuilder->remove();
		taskRemoved = true;
	}
	
	if (!taskRemoved && needEStallFixing && taskWithPowerBuilder) {
		taskWithPowerBuilder->remove();
		taskRemoved = true;
	}

	// wait all factories and their assisters...
	for (it = ai->tasks->activeTasks[TASK_FACTORY].begin(); it != ai->tasks->activeTasks[TASK_FACTORY].end(); ++it) {
		FactoryTask *task = (FactoryTask*)it->second;
		task->setWait(true);
	}
}

void CEconomy::updateIncomes() {
	const int currentFrame = ai->cb->GetCurrentFrame();

	// FIXME:
	// 1) algo sucks when game is started without initial resources
	// 2) these values should be recalculated each time AI has lost almost
	// all of its units (it is like another game start)
	// 3) these values should slowly decay to zero
	if (!stallThresholdsReady) {
		if ((utCommander->cats&FACTORY).any()) {
			mStart = utCommander->def->metalMake;
			eStart = 0.0f;
		}
		else {
			bool oldAlgo = true;
			unitCategory initialFactory = getNextTypeToBuild(utCommander, FACTORY, MIN_TECHLEVEL);
			if (initialFactory.any()) {
				UnitType* facType = ai->unittable->canBuild(utCommander, initialFactory);
				if (facType != NULL) {
					float buildTime = facType->def->buildTime / utCommander->def->buildSpeed;
					float mDrain = facType->def->metalCost / buildTime;
					float eDrain = facType->def->energyCost / buildTime;
					float mTotalIncome = utCommander->def->metalMake * buildTime;
		
					mStart = (1.5f * facType->def->metalCost - ai->cb->GetMetal() - mTotalIncome) / buildTime;
					if (mStart < 0.0f)
						mStart = 0.0f;
					eStart = 0.9f * eDrain;
				
					oldAlgo = false;
				}
			}
		
			if (oldAlgo) {
				mStart = 2.0f * utCommander->def->metalMake;
				eStart = 1.5f * utCommander->def->energyMake;
			}
		}	

		LOG_II("CEconomy::updateIncomes Metal stall threshold: " << mStart)
		LOG_II("CEconomy::updateIncomes Energy stall threshold: " << eStart)
		
		stallThresholdsReady = true;
	}

	incomes++;

	mUsageSummed  += ai->cb->GetMetalUsage();
	eUsageSummed  += ai->cb->GetEnergyUsage();
	mStorage       = ai->cb->GetMetalStorage();
	eStorage       = ai->cb->GetEnergyStorage();

	mNow     = ai->cb->GetMetal();
	eNow     = ai->cb->GetEnergy();
	mIncome  = ai->cb->GetMetalIncome();
	eIncome  = ai->cb->GetEnergyIncome();
	mUsage   = alpha*(mUsageSummed / incomes) + (1.0f-alpha)*(ai->cb->GetMetalUsage());
	eUsage   = beta *(eUsageSummed / incomes) + (1.0f-beta) *(ai->cb->GetEnergyUsage());

	//LOG_II("mIncome = " << mIncome << "; mNow = " << mNow << "; mStorage = " << mStorage)
	//LOG_II("eIncome = " << eIncome << "; eNow = " << eNow << "; eStorage = " << eStorage)

	// FIXME: think smth better to avoid hardcoding, e.g. implement decaying

	if (mIncome < EPS && currentFrame > 32) {
		mstall = (mNow < (mStorage*0.1f));
	}
	else {
		if (ai->unittable->activeUnits.size() < 5)
			mstall = (mNow < (mStorage*0.1f) && mUsage > mIncome) || mIncome < mStart;
		else
			mstall = (mNow < (mStorage*0.1f) && mUsage > mIncome);
	}

	if (eIncome < EPS && currentFrame > 32) {
	    estall = (eNow < (eStorage*0.1f));
	}
	else {
		if (ai->unittable->activeUnits.size() < 5)
			estall = (eNow < (eStorage*0.1f) && eUsage > eIncome) || eIncome < eStart;
		else
			estall = (eNow < (eStorage*0.1f) && eUsage > eIncome);
	}

	mexceeding = (mNow > (mStorage*0.9f) && mUsage < mIncome);
	eexceeding = (eNow > (eStorage*0.9f) && eUsage < eIncome);

	mRequest = (mNow < (mStorage*0.5f) && mUsage > mIncome);
	eRequest = (eNow < (eStorage*0.5f) && eUsage > eIncome);

	int tstate = ai->cfgparser->determineState(mIncome, eIncome);
	if (tstate != state) {
		char buf[64];
		sprintf(buf, "State changed to %d, activated techlevel %d", tstate, ai->cfgparser->getMaxTechLevel());
		LOG_II(buf);
		state = tstate;
	}
}

ATask* CEconomy::canAssist(buildType t, CGroup &group) {
	std::map<int, ATask*>::iterator i;
	std::multimap<float, BuildTask*> suited;

	if (!group.canAssist())
		return NULL;

	for (i = ai->tasks->activeTasks[TASK_BUILD].begin(); i != ai->tasks->activeTasks[TASK_BUILD].end(); ++i) {
		BuildTask* buildtask = (BuildTask*)i->second;

		// only build tasks we are interested in...
		float travelTime;
		if (buildtask->bt != t || !buildtask->assistable(group, travelTime))
			continue;
		
		suited.insert(std::pair<float, BuildTask*>(travelTime, buildtask));
	}

	// there are no suited tasks that require assistance
	if (suited.empty())
		return NULL;

	bool isCommander = (group.cats&COMMANDER).any();

	if (isCommander) {
		float eta = (suited.begin())->first;

		// don't pursuit as commander when walkdistance is more than 15 seconds
		if (eta > 450.0f) return NULL;
	}

	return suited.begin()->second;
}

ATask* CEconomy::canAssistFactory(CGroup &group) {
	if (!group.canAssist())
		return NULL;
	
	CUnit* unit = group.firstUnit();
	float3 pos = group.pos();
	std::map<int, ATask*>::iterator i;
	std::map<float, FactoryTask*> candidates;
	int maxTechLevel = ai->cfgparser->getMaxTechLevel();
	unitCategory maxTechLevelCat;
	
	maxTechLevelCat.set(maxTechLevel - 1);

	// NOTE: prefer assising more advanced factories depending
	// on current possible tech level. Skip factories full of assisters to
	// prevent unneeded cotinuous assist attempts on the same factory.

	for (i = ai->tasks->activeTasks[TASK_FACTORY].begin(); i != ai->tasks->activeTasks[TASK_FACTORY].end(); ++i) {
		/* TODO: instead of euclid distance, use pathfinder distance */
		float dist = pos.distance2D(i->second->pos);
		if ((i->second->firstGroup()->cats&maxTechLevelCat).any())
			dist -= 1000.0f;
		dist += (1000.0f * i->second->assisters.size()) / FACTORY_ASSISTERS;
		candidates[dist] = (FactoryTask*)i->second;
	}

	if (candidates.empty())
		return NULL;

	std::map<float, FactoryTask*>::iterator j;
	for (j = candidates.begin(); j != candidates.end(); ++j) {
		if (!j->second->assistable(group))
			continue;
		return j->second;
	}

	return NULL;
}

bool CEconomy::canAffordToBuild(UnitType *builder, UnitType *utToBuild) {
	float buildTime = utToBuild->def->buildTime / builder->def->buildSpeed;
	float mPrediction = mNow + (mIncome - mUsage) * buildTime - utToBuild->def->metalCost;
	float ePrediction = eNow + (eIncome - eUsage) * buildTime - utToBuild->def->energyCost;

	if (!mRequest)
		mRequest = mPrediction < 0.0f;
	if (!eRequest)
		eRequest = ePrediction < 0.0f;

	return (mPrediction >= 0.0f && ePrediction >= 0.0f && mNow/mStorage >= 0.1f);
}

unitCategory CEconomy::getNextTypeToBuild(CUnit *unit, unitCategory cats, int maxteachlevel) {
	return getNextTypeToBuild(unit->type, cats, maxteachlevel);
}

unitCategory CEconomy::getNextTypeToBuild(UnitType *ut, unitCategory cats, int maxteachlevel) {
	std::list<unitCategory>::iterator f;

	if (ai->intel->strategyTechUp) {
		for(f = ai->intel->allowedFactories.begin(); f != ai->intel->allowedFactories.end(); ++f) {
			for(int techlevel = maxteachlevel; techlevel >= MIN_TECHLEVEL; techlevel--) {
				unitCategory type((*f)|cats);
				type.set(techlevel - 1);
				if (isTypeRequired(ut, type, maxteachlevel))
					return type;
			}
		}
	}
	else {
		for(int techlevel = MIN_TECHLEVEL; techlevel <= maxteachlevel; techlevel++) {
			unitCategory techlevelCat;
			techlevelCat.set(techlevel - 1);
			for(f = ai->intel->allowedFactories.begin(); f != ai->intel->allowedFactories.end(); f++) {
				unitCategory type((*f)|cats|techlevelCat);
				if (isTypeRequired(ut, type, maxteachlevel))
					return type;
			}
		}
	}	
	
	return 0;
}

bool CEconomy::isTypeRequired(UnitType* builder, unitCategory cats, int maxteachlevel) {
	UnitType* ut = ai->unittable->canBuild(builder, cats);
	if(ut) {
		if ((cats&FACTORY).any()) {
			int numRequired = ut->def->canBeAssisted ? 1: maxteachlevel;
			if (ai->unittable->factoryCount(cats) < numRequired)
				return true;
		}
		else if(ai->unittable->unitCount(cats) == 0)
			return true;
	}
	return false;	
}

void CEconomy::tryBuildingDefense(CGroup* group) {
	if (group->busy)
		return;
	if (mstall || estall)
		return;

	bool allow;
	int size;
	float k;
	unitCategory incCats, excCats;
	buildType bt;
	CCoverageCell::NType layer;
	
	if (ai->intel->getEnemyCount(AIR) > 0 && rng.RandFloat() > 0.66f) {
		bt = BUILD_AA_DEFENSE;
		layer = CCoverageCell::DEFENSE_ANTIAIR;
		// TODO: replace STATIC with DEFENSE after all config files updated
		incCats = STATIC|ANTIAIR;
		excCats = TORPEDO;
	}
	else if (ai->gamemap->IsWaterMap() && rng.RandFloat() > 0.5f) {
		bt = BUILD_UW_DEFENSE;
		layer = CCoverageCell::DEFENSE_UNDERWATER;
		// TODO: replace STATIC with DEFENSE after all config files updated
		incCats = STATIC|TORPEDO;
		// NOTE: we do not support coastal torpedo launchers
		excCats = LAND;
	}
	else
	{
		bt = BUILD_AG_DEFENSE;
		layer = CCoverageCell::DEFENSE_GROUND;
		incCats = ATTACKER|DEFENSE;
		excCats = ANTIAIR|TORPEDO;
	}
				
	size = ai->coverage->getLayerSize(layer);
	k = size / (ai->unittable->staticUnits.size() - size + 1.0f);
	
	switch (ai->difficulty) {
		case DIFFICULTY_EASY:
			allow = k < 0.11f;
			break;
		case DIFFICULTY_NORMAL:
			allow = k < 0.31f;
			break;
		case DIFFICULTY_HARD:
			allow = k < 0.51f;
			break;
	}
	
	buildOrAssist(*group, bt, incCats, excCats);
}

void CEconomy::tryBuildingShield(CGroup* group) {
	if (group->busy)
		return;
	if (estall)
		return;
	if (ai->difficulty == DIFFICULTY_EASY)
		return;
	if (ai->intel->getEnemyCount(ARTILLERY) < 20)
		return;

	// TODO: also build shields when enemy got LRPC

	// TODO: introduce importance threshold otherwise AI
	// finally will cover all buildings, including cheap one

	buildOrAssist(*group, BUILD_IMP_DEFENSE, SHIELD);
}

void CEconomy::tryBuildingStaticAssister(CGroup* group) {
	if (group->busy)
		return;
	if (ai->difficulty == DIFFICULTY_EASY)
		return;
	if (mstall || estall)
		return;

	buildOrAssist(*group, BUILD_MISC_DEFENSE, NANOTOWER);
}

void CEconomy::tryBuildingJammer(CGroup* group) {
	if (group->busy)
		return;
	if (ai->difficulty == DIFFICULTY_EASY)
		return;
	if (mstall || estall)
		return;
	
	buildOrAssist(*group, BUILD_MISC_DEFENSE, JAMMER);
}

void CEconomy::tryBuildingAntiNuke(CGroup* group) {
	if (group->busy)
		return;
	if (ai->difficulty == DIFFICULTY_EASY)
		return;
	// TODO: avoid using unitCount()
	if (ai->unittable->unitCount(ANTINUKE) >= ai->intel->enemies.getUnits(NUKE)->size())
		return;
	
	buildOrAssist(*group, BUILD_IMP_DEFENSE, ANTINUKE);
}

void CEconomy::tryAssistingFactory(CGroup* group) {
	if (group->busy)
		return;
	if (mstall || estall)
		return;

	ATask *task;
	if ((task = canAssistFactory(*group)) != NULL) {
		ai->tasks->addTask(new AssistTask(ai, *task, *group));
	}
}

void CEconomy::tryAssist(CGroup* group, buildType bt) {
	if (group->busy)
		return;
	if (mstall || estall)
		return;

	ATask *task = canAssist(bt, *group);
	if (task != NULL) {
		ai->tasks->addTask(new AssistTask(ai, *task, *group));
		return;
	}
}

void CEconomy::tryFixingStall(CGroup* group) {
	bool mStall = (mstall && !mexceeding);
	bool eStall = (estall && !eexceeding);
	std::list<buildType> order;

	if (group->busy)
		return;
	
	if (mStall && eStall
	&& (((mIncome - mUsage) * METAL2ENERGY) < (eIncome - eUsage))) {
		order.push_back(BUILD_MPROVIDER);
		order.push_back(BUILD_EPROVIDER);
	}
	else {
		if (eStall)
			order.push_back(BUILD_EPROVIDER);
		if (mStall)
			order.push_back(BUILD_MPROVIDER);
	}

	for (std::list<buildType>::const_iterator it = order.begin(); it != order.end(); ++it) {
		buildOrAssist(*group, *it, *it == BUILD_EPROVIDER ? EMAKER : MEXTRACTOR);
		if (group->busy) break;
	}
}

unitCategory CEconomy::canBuildWhere(unitCategory unitCats, bool strictly) {
	unitCategory result;
	UnitCategory2UnitCategoryMap::iterator it;
	
	for (it = canBuildEnv.begin(); it != canBuildEnv.end(); ++it) {
		if ((unitCats&it->first).any())
			result |= it->second;
	}
	
	if (strictly)
		return result;

	// explicitly exclude useless units on specific maps...	
	if (ai->gamemap->IsLandFreeMap()) {
		result &= ~(LAND);
	}
	else if (ai->gamemap->IsWaterFreeMap()) {
		result &= ~(SEA|SUB);
	}
	else {	
		// TODO: detect unit's sector land/sea percent and enforce/remove tags
		// accordingly
	}
	
	return result;
}
