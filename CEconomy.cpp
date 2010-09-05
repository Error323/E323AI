#include "CEconomy.h"

#include <cfloat>
#include <limits>

#include "headers/HEngine.h"

#include "CAI.h"
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


CEconomy::CEconomy(AIClasses *ai): ARegistrar(700, std::string("economy")) {
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
}

CEconomy::~CEconomy()
{
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

	initialized = true;
}
		
bool CEconomy::hasBegunBuilding(CGroup &group) {
	std::map<int, CUnit*>::iterator i;
	for (i = group.units.begin(); i != group.units.end(); i++) {
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

void CEconomy::addUnitOnCreated(CUnit &unit) {
	unsigned int c = unit.type->cats;
	if (c&MEXTRACTOR) {
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

	unsigned c = unit.type->cats;

	if (c&BUILDER) {
		CGroup *group = requestGroup();
		group->addUnit(unit);
		if (c&FACTORY)
			ai->unittable->factories[unit.key] = &unit;
	}
	else if (c&MMAKER) {
		ai->unittable->metalMakers[unit.key] = &unit;
	}
}

void CEconomy::buildOrAssist(CGroup &group, buildType bt, unsigned include, unsigned exclude) {
	ATask *task = canAssist(bt, group);
	if (task != NULL) {
		ai->tasks->addTask(new AssistTask(ai, *task, group));
		return;
	}
	
	CUnit *unit = group.firstUnit();
	/* Retrieve the allowed buildable units */
	std::multimap<float, UnitType*> candidates;
	ai->unittable->getBuildables(unit->type, include, exclude, candidates);
	if (candidates.empty())
		return; // builder can build nothing we need

	int alternativesCount = candidates.size();

	unsigned int envCats = (include&(AIR|SEA|LAND)) & ~exclude;

	/* Determine which of these we can afford */
	std::multimap<float, UnitType*>::iterator i = candidates.begin();
	int iterations = candidates.size() / (ai->cfgparser->getTotalStates() - state + 1);
	bool affordable = false;
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
	
	/* Determine the location where to build */
	float3 pos = group.pos();
	float3 goal = pos;

	/* Perform the build */
	switch(bt) {
		case BUILD_EPROVIDER: {
			if ((i->second->def->windGenerator > EPS) && alternativesCount > 1) {
	        	if (!windmap || ai->cb->GetCurWind() < 10.0f) {
	        		// select first non-wind emaker...
	        		i = candidates.begin();
	        		while (i != candidates.end() && (i->second->def->windGenerator))
	        			i++;
					if (i == candidates.end())
						// all emakers are based on wind power
						i--;
	        	}
			}
			
			if (i->second->def->needGeo && alternativesCount > 1) {
				float e = eNow/eStorage;
				goal = getClosestOpenGeoSpot(group);
				if (goal == ZeroVector || (e > 0.15f && ai->pathfinder->getETA(group, goal) > 450.0f)) {
					// build anything another...
	        		i = candidates.begin();
	        		while (i != candidates.end() && (i->second->def->windGenerator || i->second->def->needGeo))
	        			i++;
					if (i == candidates.end())
						// all emakers are geo- or wind-based
						i--;
					goal = pos;
				}
			}

			ai->tasks->addTask(new BuildTask(ai, bt, i->second, group, goal));

			break;
		}

		case BUILD_MPROVIDER: {
			goal = getClosestOpenMetalSpot(group);
			bool canBuildMMaker = (eIncome - eUsage) >= METAL2ENERGY || eexceeding;
			if (goal != ZeroVector) {
				bool allowDirectTask = true;
				bool isComm = unit->type->cats&COMMANDER;
				bool tooSmallIncome = (mIncome < mStart);

				// TODO: there is a flaw in logic because when spot is under
				// threat we anyway send builders there
				
				if (isComm && !tooSmallIncome) {
					// if commander can build mmakers and there is no
					// urgent metal need then prevent it walking for more than 
					// 10 sec...
					if (ai->unittable->canBuild(unit->type, MMAKER|envCats))
						allowDirectTask = ai->pathfinder->getETA(group, goal) < 450.0f;
				}

				if (allowDirectTask) {
					ai->tasks->addTask(new BuildTask(ai, bt, i->second, group, goal));
				}
				else if (mstall && (isComm || areMMakersEnabled) && canBuildMMaker) {
					UnitType *mmaker = ai->unittable->canBuild(unit->type, MMAKER|envCats);
					if (mmaker != NULL)
						ai->tasks->addTask(new BuildTask(ai, bt, mmaker, group, pos));
				}
			}
			else if (mstall && areMMakersEnabled && canBuildMMaker) {
				UnitType *mmaker = ai->unittable->canBuild(unit->type, MMAKER|envCats);
				if (mmaker != NULL)
					ai->tasks->addTask(new BuildTask(ai, bt, mmaker, group, pos));
			}
			else if (!eexceeding) {
				buildOrAssist(group, BUILD_EPROVIDER, EMAKER|envCats);
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
				
				/*
				if (numFactories > 0) {
					int maxTechLevel = ai->cfgparser->getMaxTechLevel();
					unsigned int cats = getNextFactoryToBuild(unit, maxTechLevel);
					if (cats > 0) {
						cats |= FACTORY;
						UnitType *ut = ai->unittable->getUnitTypeByCats(cats);
						if (ut) {
							if (ut->costMetal > EPS)
								build = ((mNow / ut->costMetal) > 0.8f);
							else
								build = true;
						}
					}
				}
				*/				
				
				if (!build) {
					float m = mNow / mStorage;
			
					switch(state) {
						case 0: case 1: case 2: {
							build = (m > 0.4f && affordable);
							break;
						}
						case 3: {
							build = (m > 0.4f);
							break;
						}
						case 4: {
							build = (m > 0.35f);
							break;
						}
						case 5: {
							build = (m > 0.3f);
							break;
						}
						case 6: {
							build = (m > 0.25f);
							break;
						}
						default: {
							build = (m > 0.2f);
						}
					}
				}

				if (build) {
					// TODO: fix getBestDefendedPos() for static builders
					if (numFactories > 1 && (unit->type->cats&MOBILE))
						pos = ai->defensematrix->getBestDefendedPos(numFactories);
					ai->tasks->addTask(new BuildTask(ai, bt, i->second, group, pos));
				}
			}
			break;
		}
		
		case BUILD_AG_DEFENSE: case BUILD_AA_DEFENSE: {
			if (!taskInProgress(bt)) {
				bool allowTask = true;
				bool isComm = unit->type->cats&COMMANDER;

				// TODO: rethink defense spot selection for static builders
				goal = ai->defensematrix->getDefenseBuildSite(i->second);
				
				if (isComm)
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

float3 CEconomy::getBestSpot(CGroup &group, std::list<float3> &resources, std::map<int, float3> &tracker, bool metal) {
	bool staticBuilder = group.cats&STATIC;
	float bestDist = std::numeric_limits<float>::max();
	float3 bestSpot = ZeroVector;
	float3 gpos = group.pos();
	float radius;
	
	if (metal)
		radius = ai->cb->GetExtractorRadius();
	else
		radius = 16.0f;

	std::list<float3>::iterator i;
	std::map<int, float3>::iterator j;
	for (i = resources.begin(); i != resources.end(); ++i) {
		// TODO: compare with actual group properties
		if (i->y < 0.0f)
			continue; // spot is under water
		
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
				taken = UC(ud->id) & MEXTRACTOR;
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

	if (bestSpot != ZeroVector)
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

void CEconomy::update(int frame) {
	int builderGroupsNum = 0;
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

		if (group->cats&MOBILE)
			builderGroupsNum++;

		if (group->busy || !ai->unittable->canPerformTask(*unit))
			continue;

		if (group->cats&FACTORY) {
			ai->tasks->addTask(new FactoryTask(ai, *group));
		
			/* TODO: put same factories in a single group. This requires refactoring of task
			assisting, because when factory is dead assisting tasks continue working on ex. factory
			position.

			CUnit *factory = CUnitTable::getUnitByDef(ai->unittable->factories, unit.def);
			if(factory && factory->group) {
				factory->group->addUnit(unit);
			} else {
				CGroup *group = requestGroup();
				group->addUnit(unit);
				ai->tasks->addFactoryTask(*group);
			}
			*/
			
			continue;
		}

		if (group->cats&STATIC && !(group->cats&BUILDER)) {
			// we don't have a task for current unit type yet
			continue;
		}
		
		unsigned int envCats = group->cats&(AIR|SEA|LAND);
		if (!ai->gamemap->IsWaterMap())
			envCats &= ~SEA;
		if ((group->cats&AIR) && !(group->cats&LAND))
			envCats |= LAND; // air units usually can build land units
		// TODO: enforce SEA if land is far away, also enforce LAND if 
		// water is far away

		float3 pos = group->pos();

		// NOTE: we're using special algo for commander to prevent
		// it walking outside the base
		if (unit->def->isCommander) {
			/* If we are estalling deal with it */
			if (estall && !eexceeding) {
				buildOrAssist(*group, BUILD_EPROVIDER, EMAKER|envCats);
				if (group->busy) continue;
			}
			
			/* If we are mstalling deal with it */
			if (mstall && !mexceeding) {
				buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR|envCats);
				if (group->busy) continue;
			}
			
			/* If we don't have a factory, build one */
			if (ai->unittable->factories.empty()) {
				unsigned int factory = getNextFactoryToBuild(unit, maxTechLevel);
				if (factory > 0)
					buildOrAssist(*group, BUILD_FACTORY, factory);
				if (group->busy) continue;
			}

			/* If we are exceeding and don't have estorage yet, build estorage */
			if (eexceeding && !ai->unittable->factories.empty()) {
				if (ai->unittable->energyStorages.size() < ai->cfgparser->getMaxTechLevel())
					buildOrAssist(*group, BUILD_ESTORAGE, ESTORAGE|envCats);
				if (group->busy) continue;
			}
			
			// HACK: in NOTA only static commanders can build factories
			if (unit->type->cats&STATIC) {
				/* See if this unit can build desired factory */
				unsigned int factory = getNextFactoryToBuild(unit, maxTechLevel);
				if (factory > 0)
					buildOrAssist(*group, BUILD_FACTORY, factory);
				if (group->busy) continue;
			}
			
			// build nearby metal extractors if possible...
			if (!(mexceeding && ai->gamemap->IsMetalMap())) {
				// NOTE: there is a special hack withing buildOrAssist() 
				// to prevent building unnecessary MMAKERS
				buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR|envCats);
				if (group->busy) continue;
			}
			
			// if we can assist a lab and it's close enough, do so...
			ATask *task = NULL;
			// TODO: "close enough" condition is NOT executed at all
			if ((task = canAssistFactory(*group)) != NULL) {
				ai->tasks->addTask(new AssistTask(ai, *task, *group));
				if (group->busy) continue;
			}
			
			// see if we can build defense (but ususally commander ends up 
			// with assisting a factory if mod rules allow this)
			if (!mstall) {
				if (ai->defensematrix->getClusters() > ai->unittable->defenses.size()) {
					buildOrAssist(*group, BUILD_AG_DEFENSE, DEFENSE, ANTIAIR);
					if (group->busy) continue;
				}
			}
			
			continue;
		}

		/* If we are estalling deal with it */
		if (estall) {
			buildOrAssist(*group, BUILD_EPROVIDER, EMAKER|envCats);
			if (group->busy) continue;
		}
		/* If we are mstalling deal with it */
		if (mstall) {
			buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR|envCats);
			if (group->busy) continue;
		}
		
		/* See if this unit can build desired factory */
		unsigned int factory = getNextFactoryToBuild(unit, maxTechLevel);
		if (factory > 0)
			buildOrAssist(*group, BUILD_FACTORY, factory);
		if (group->busy) continue;
		
		/* See if we can build defense */
		if (!mstall) {
			if (ai->defensematrix->getClusters() > ai->unittable->defenses.size()) {
				buildOrAssist(*group, BUILD_AG_DEFENSE, DEFENSE, ANTIAIR);
				if (group->busy) continue;
			}
		}
		
		/* If we are overflowing energy build an estorage */
		if (eexceeding) {
			if (ai->unittable->energyStorages.size() < ai->cfgparser->getMaxTechLevel())
				buildOrAssist(*group, BUILD_ESTORAGE, ESTORAGE|envCats);
			if (group->busy) continue;
		}

		/* If we are overflowing metal build an mstorage */
		if (mexceeding) {
			buildOrAssist(*group, BUILD_MSTORAGE, MSTORAGE|envCats);
			if (group->busy) continue;
		}
		
		/* If both requested, see what is required most */
		if (eRequest && mRequest) {
			if ((mNow / mStorage) > (eNow / eStorage))
				buildOrAssist(*group, BUILD_EPROVIDER, EMAKER|envCats);
			else
				buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR|envCats);
			if (group->busy) continue;
		}
		
		/* Else just provide that which is requested */
		if (eRequest) {
			buildOrAssist(*group, BUILD_EPROVIDER, EMAKER|envCats);
			if (group->busy) continue;
		}
		if (mRequest) {
			buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR|envCats);
			if (group->busy) continue;
		}

		/* If we can afford to assist a lab and it's close enough, do so */
		ATask *task = NULL;
		if ((task = canAssistFactory(*group)) != NULL) {
			ai->tasks->addTask(new AssistTask(ai, *task, *group));
			if (group->busy) continue;
		}
		
		/* Otherwise just expand */
		if (!ai->gamemap->IsMetalMap()) {
			if ((mNow / mStorage) > (eNow / eStorage))
				buildOrAssist(*group, BUILD_EPROVIDER, EMAKER|envCats);
			else
				buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR|envCats);
		}
	}

	if (builderGroupsNum < ai->cfgparser->getMaxWorkers() 
	&& (builderGroupsNum < ai->cfgparser->getMinWorkers()))
		ai->wishlist->push(BUILDER, HIGH);
	else if (builderGroupsNum < ai->cfgparser->getMaxWorkers())
		ai->wishlist->push(BUILDER, NORMAL);
}

bool CEconomy::taskInProgress(buildType bt) {
	std::map<int, ATask*>::iterator it;
	for (it = ai->tasks->activeTasks[TASK_BUILD].begin(); it != ai->tasks->activeTasks[TASK_BUILD].end(); ++it) {
		if (((BuildTask*)(it->second))->bt == bt)
			return true;
	}
	return false;
}

void CEconomy::controlMetalMakers() {
	float eRatio = eNow / eStorage;

	if (eRatio < 0.3f) {
		int count = ai->unittable->setOnOff(ai->unittable->metalMakers, false);
		if (count > 0) {
			estall = false;
			areMMakersEnabled = false;
			return;
		}
	}

	if (eRatio > 0.7f) {
		int count = ai->unittable->setOnOff(ai->unittable->metalMakers, true);
		if (count > 0) {
			mstall = false;
			areMMakersEnabled = true;
			return;
		}
	}
}

void CEconomy::preventStalling() {
	bool needMStallFixing, needEStallFixing;	
	AssistTask
		*taskWithPowerBuilder = NULL,
		*taskWithFastBuilder = NULL;
	std::map<int, ATask*>::iterator it;

	// if factorytask is on wait, unwait him...
	for (it = ai->tasks->activeTasks[TASK_FACTORY].begin(); it != ai->tasks->activeTasks[TASK_FACTORY].end(); ++it) {
		FactoryTask *task = (FactoryTask*)it->second;
		task->setWait(false);
	}

	// if we're not stalling, return...
	if (!mstall && !estall)
		return;

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

	// stop all guarding workers which do not help in fixing the problem...
	it = ai->tasks->activeTasks[TASK_ASSIST].begin();
	while (it != ai->tasks->activeTasks[TASK_ASSIST].end()) {
		AssistTask *task = (AssistTask*)it->second; ++it;
		
		// if the assisting group is moving, continue...
		if (task->isMoving)
			continue;
		
		if (task->assist->t == TASK_BUILD) {
			// ignore builders which are fixing the problems...
			BuildTask* build = dynamic_cast<BuildTask*>(task->assist);
			if ((mstall || mRequest) && build->bt == BUILD_MPROVIDER)
				continue;
			if ((estall || eRequest) && build->bt == BUILD_EPROVIDER)
				continue;

			task->remove();

			return;
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
				if (task->firstGroup()->speed > taskWithFastBuilder->firstGroup()->speed)
					taskWithFastBuilder = task;
			}
			else
				taskWithFastBuilder = task;
		}

		/* Unless it is the commander, he should be fixing the problem */
		/*
		if (task->firstGroup()->firstUnit()->def->isCommander) {
			if ((mstall && !eRequest) || (estall && !mstall)) {
				task->remove();
				return;
			}
		}
		*/		
	}

	if (needMStallFixing && taskWithFastBuilder) {
		taskWithFastBuilder->remove();
		return;
	}
	
	if (needEStallFixing && taskWithPowerBuilder) {
		taskWithPowerBuilder->remove();
		return;
	}

	// wait all factories and their assisters...
	for (it = ai->tasks->activeTasks[TASK_FACTORY].begin(); it != ai->tasks->activeTasks[TASK_FACTORY].end(); ++it) {
		FactoryTask *task = (FactoryTask*)it->second;
		task->setWait(true);
	}
}

void CEconomy::updateIncomes(int frame) {
	if (!stallThresholdsReady) {
		if (utCommander->cats&FACTORY) {
			mStart = utCommander->def->metalMake;
			eStart = 0.0f;
		}
		else {
			bool oldAlgo = true;
			int initialFactory = getNextFactoryToBuild(utCommander, MIN_TECHLEVEL);
			if (initialFactory) {
				UnitType *facType = ai->unittable->canBuild(utCommander, initialFactory);
				if (facType != NULL) {
					float buildTime = facType->def->buildTime / utCommander->def->buildSpeed;
					float mDrain = facType->def->metalCost / buildTime;
					float eDrain = facType->def->energyCost / buildTime;
					float mTotalIncome = utCommander->def->metalMake * buildTime;
		
					mStart = (1.5 * facType->def->metalCost - ai->cb->GetMetal() - mTotalIncome) / buildTime;
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

	//mNowSummed    += ai->cb->GetMetal();
	//eNowSummed    += ai->cb->GetEnergy();
	//mIncomeSummed += ai->cb->GetMetalIncome();
	//eIncomeSummed += ai->cb->GetEnergyIncome();
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

	/*
	std::map<int, CUnit*>::iterator i;
	float mU = 0.0f, eU = 0.0f;
	for (i = ai->unittable->activeUnits.begin(); i != ai->unittable->activeUnits.end(); ++i) {
		unsigned int c = i->second->type->cats;
		if (!(c&MMAKER) || !(c&EMAKER) || !(c&MEXTRACTOR)) {
			mU += i->second->type->metalMake;
			eU += i->second->type->energyMake;
		}
	}
	uMIncomeSummed += mU;
	uEIncomeSummed += eU;
	
	uMIncome   = alpha*(uMIncomeSummed / incomes) + (1.0f-alpha)*mU;
	uEIncome   = beta*(uEIncomeSummed / incomes) + (1.0f-beta)*eU;
	*/

	mstall     = (mNow < (mStorage*0.1f) && mUsage > mIncome) || mIncome < mStart;
	estall     = (eNow < (eStorage*0.1f) && eUsage > eIncome) || eIncome < eStart;

	mexceeding = (mNow > (mStorage*0.9f) && mUsage < mIncome);
	eexceeding = (eNow > (eStorage*0.9f) && eUsage < eIncome);

	mRequest   = (mNow < (mStorage*0.5f));
	eRequest   = (eNow < (eStorage*0.5f));

	int tstate = ai->cfgparser->determineState(mIncome, eIncome);
	if (tstate != state) {
		char buf[255];
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
		BuildTask *buildtask = (BuildTask*)i->second;

		/* Only build tasks we are interested in */
		float travelTime;
		if (buildtask->bt != t || !buildtask->assistable(group, travelTime))
			continue;
		
		suited.insert(std::pair<float, BuildTask*>(travelTime, buildtask));
	}

	/* There are no suited tasks that require assistance */
	if (suited.empty())
		return NULL;

	bool isCommander = group.firstUnit()->def->isCommander;

	if (isCommander) {
		float3 g = (suited.begin())->second->pos;
		float eta = ai->pathfinder->getETA(group, g);

		/* Don't pursuit as commander when walkdistance is more then 10 seconds */
		if (eta > 300.0f) return NULL;
	}

	return suited.begin()->second;
}

ATask* CEconomy::canAssistFactory(CGroup &group) {
	if (!group.canAssist())
		return NULL;
	
	CUnit *unit = group.firstUnit();
	float3 pos = group.pos();
	std::map<int, ATask*>::iterator i;
	std::map<float, FactoryTask*> candidates;

	for (i = ai->tasks->activeTasks[TASK_FACTORY].begin(); i != ai->tasks->activeTasks[TASK_FACTORY].end(); ++i) {
		/* TODO: instead of euclid distance, use pathfinder distance */
		float dist = pos.distance2D(i->second->pos);
		candidates[dist] = (FactoryTask*)i->second;
	}

	if (candidates.empty())
		return NULL;

	//if (unit->def->isCommander)
	//	return candidates.begin()->second;

	std::map<float, FactoryTask*>::iterator j;
	for (j = candidates.begin(); j != candidates.end(); ++j) {
		if (!j->second->assistable(group))
			continue;
		return j->second;
	}

	return NULL;
}

bool CEconomy::canAffordToBuild(UnitType *builder, UnitType *utToBuild) {
	/*
	// NOTE: "Salary" is provided every 32 logical frames
	float buildTime   = (utToBuild->def->buildTime / builder->def->buildSpeed) * 32.0f;
	float mCost       = utToBuild->def->metalCost;
	float eCost       = utToBuild->def->energyCost;
	float mPrediction = (mIncome - mUsage - mCost/buildTime)*buildTime - mCost + mNow;
	float ePrediction = (eIncome - eUsage - eCost/buildTime)*buildTime - eCost + eNow;
	mRequest          = mPrediction < 0.0f;
	eRequest          = ePrediction < 0.0f;
	*/
	float buildTime = utToBuild->def->buildTime / builder->def->buildSpeed;
	float mPrediction = mNow + (mIncome - mUsage) * buildTime - utToBuild->def->metalCost;
	float ePrediction = eNow + (eIncome - eUsage) * buildTime - utToBuild->def->energyCost;

	if (!mRequest)
		mRequest = mPrediction < 0.0f;
	if (!eRequest)
		eRequest = ePrediction < 0.0f;

	return (mPrediction >= 0.0f && ePrediction >= 0.0f && mNow/mStorage >= 0.1f);
}

unsigned int CEconomy::getNextFactoryToBuild(CUnit *unit, int maxteachlevel) {
	return getNextFactoryToBuild(unit->type, maxteachlevel);
}

unsigned int CEconomy::getNextFactoryToBuild(UnitType *ut, int maxteachlevel) {
	std::list<unitCategory>::iterator f;

	if (ai->intel->strategyTechUp) {
		for(f = ai->intel->allowedFactories.begin(); f != ai->intel->allowedFactories.end(); ++f) {
			for(int techlevel = maxteachlevel; techlevel >= MIN_TECHLEVEL; techlevel--) {
				unsigned int factory = *f|(1<<(techlevel - 1));
				if(ai->unittable->canBuild(ut, factory))
					if(!ai->unittable->gotFactory(factory)) {
						return factory;
					}
			}
		}
	}
	else {
		for(int techlevel = MIN_TECHLEVEL; techlevel <= maxteachlevel; techlevel++) {
			unsigned int techlevelCat = 1<<(techlevel - 1);
			for(f = ai->intel->allowedFactories.begin(); f != ai->intel->allowedFactories.end(); f++) {
				unsigned int factory = *f|techlevelCat;
				if(ai->unittable->canBuild(ut, factory))
					if(!ai->unittable->gotFactory(factory)) {
						return factory;
					}
			}
		}
	}	
	
	return 0;
}
