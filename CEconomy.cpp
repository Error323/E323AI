#include "CEconomy.h"

#include "headers/HEngine.h"

#include "CAI.h"
#include "CTaskHandler.h"
#include "CUnitTable.h"
#include "CMetalMap.h"
#include "CWishList.h"
#include "CDefenseMatrix.h"
#include "CGroup.h"
#include "CUnit.h"
#include "CRNG.h"
#include "CPathfinder.h"
#include "CConfigParser.h"

CEconomy::CEconomy(AIClasses *ai): ARegistrar(700, std::string("economy")) {
	this->ai = ai;
	incomes  = 0;
	mNow     = mNowSummed     = eNow     = eNowSummed     = 0.0f;
	mIncome  = mIncomeSummed  = eIncome  = eIncomeSummed  = 0.0f;
	uMIncome = uMIncomeSummed = uEIncome = uEIncomeSummed = 0.0f;
	mUsage   = mUsageSummed   = eUsage   = eUsageSummed   = 0.0f;
	mStorage = eStorage                                   = 0.0f;
	mstall = estall = mexceeding = eexceeding = mRequest = eRequest = false;
}

void CEconomy::init(CUnit &unit) {
	const UnitDef *ud = ai->cb->GetUnitDef(unit.key);
	UnitType *utCommander = UT(ud->id);
	windmap = (ai->cb->GetMaxWind() + ai->cb->GetMinWind())/2.0f >= 10.0f;
	//float avgWind   = (ai->cb->GetMinWind() + ai->cb->GetMaxWind()) / 2.0f;
	//float windProf  = avgWind / utWind->cost;
	//float solarProf = utSolar->energyMake / utSolar->cost;
	mStart = utCommander->def->metalMake;
	eStart = utCommander->def->energyMake;
}
		
bool CEconomy::hasBegunBuilding(CGroup &group) {
	std::map<int, CUnit*>::iterator i;
	for (i = group.units.begin(); i != group.units.end(); i++) {
		CUnit *unit = i->second;
		if (ai->unittable->idle.find(unit->key) == ai->unittable->builders.end()
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
	activeGroups[group->key] = group;
	return group;
}

void CEconomy::remove(ARegistrar &group) {
	LOG_II("CEconomy::remove group(" << group.key << ")")
	free.push(lookup[group.key]);
	lookup.erase(group.key);
	activeGroups.erase(group.key);

	std::list<ARegistrar*>::iterator i;
	for (i = records.begin(); i != records.end(); i++)
		(*i)->remove(group);
}

void CEconomy::addUnit(CUnit &unit) {
	LOG_II("CEconomy::addUnit " << unit)

	unsigned c = unit.type->cats;
	if (c&FACTORY) {
		ai->tasks->addFactoryTask(unit);
		ai->unittable->factories[unit.key] = true;
	}

	else if (c&BUILDER && c&MOBILE) {
		CGroup *group = requestGroup();
		group->addUnit(unit);
	}

	else if (c&MMAKER) {
		ai->unittable->metalMakers[unit.key] = true;
	}
}

void CEconomy::buildOrAssist(CGroup &group, buildType bt, unsigned include, unsigned exclude) {
	ATask *task = canAssist(bt, group);
	if (task != NULL)
		ai->tasks->addAssistTask(*task, group);
	else {
		/* Retrieve the allowed buildable units */
		CUnit *unit = group.units.begin()->second;
		std::multimap<float, UnitType*> candidates;
		ai->unittable->getBuildables(unit->type, include, exclude, candidates);

		if (candidates.empty()) 
			return;

		/* Determine which of these we can afford */
		std::multimap<float, UnitType*>::iterator i = candidates.begin();
		int iterations = candidates.size() / (ai->cfgparser->getTotalStates()+1-state);
		bool affordable = false;
		while(iterations >= 0) {
			if (canAffordToBuild(group, i->second))
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
		facing f = unit->getBestFacing(pos);
		int mindist = 5;
		float startRadius = unit->def->buildDistance;
		float3 goal = ai->cb->ClosestBuildSite(i->second->def, pos, startRadius, mindist, f);
		int tries = 0;
		while (goal == ERRORVECTOR) {
			startRadius += i->second->def->buildDistance;
			goal = ai->cb->ClosestBuildSite(i->second->def, pos, startRadius, mindist, f);
			tries++;
			if (tries > 10) {
				goal = pos;
				break;
			}
		}

		/* Perform the build */
		switch(bt) {
			case BUILD_EPROVIDER: {
				if (windmap)
					ai->tasks->addBuildTask(bt, i->second, group, goal);
				else if (i->second->cats&WIND)
					ai->tasks->addBuildTask(bt, (++candidates.begin())->second, group, goal);
				else
					ai->tasks->addBuildTask(bt, i->second, group, goal);
				break;
			}

			case BUILD_MPROVIDER: {
				bool canBuildMex = ai->metalmap->getMexSpot(group, goal);
				if (canBuildMex) {
					bool b1 = mIncome < 3.0f;
					bool b2 = !unit->def->isCommander;
					if (b1 || (b2 || ai->pathfinder->getETA(group, goal) < 32*20))
						ai->tasks->addBuildTask(BUILD_MPROVIDER, i->second, group, goal);
					else if (!eRequest && !estall) {
						UnitType *mmaker = ai->unittable->canBuild(unit->type, LAND|MMAKER);
						ai->tasks->addBuildTask(BUILD_MPROVIDER, mmaker, group, pos);
					}
				}
				else if (!eRequest) {
					UnitType *mmaker = ai->unittable->canBuild(unit->type, LAND|MMAKER);
					ai->tasks->addBuildTask(BUILD_MPROVIDER, mmaker, group, pos);
				}
				break;
			}
			
			case BUILD_AG_DEFENSE: case BUILD_AA_DEFENSE: {
				if (!taskInProgress(bt)) {
					pos = ai->defensematrix->getDefenseBuildSite(i->second);
					facing f = unit->getBestFacing(pos);
					int mindist = 5;
					float startRadius = unit->def->buildDistance;
					goal = ai->cb->ClosestBuildSite(i->second->def, pos, startRadius, mindist, f);
					ai->tasks->addBuildTask(bt, i->second, group, goal);
				}
				break;
			}

			default: {
				if (affordable && !taskInProgress(bt))
					ai->tasks->addBuildTask(bt, i->second, group, goal);
				break;
			}
		}
	}
}

void CEconomy::update(int frame) {
	/* If we are stalling, do something about it */
	preventStalling();

	/* Update idle worker groups */
	std::map<int, CGroup*>::iterator i;
	for (i = activeGroups.begin(); i != activeGroups.end(); i++) {
		CGroup *group = i->second;
		CUnit *unit = group->units.begin()->second;

		if (group->busy || !ai->unittable->canPerformTask(*unit)) continue;

		float3 pos = group->pos();

		if (unit->def->isCommander) {
			/* If we are mstalling deal with it */
			if (mstall) {
				buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR|LAND);
				if (group->busy) continue;
			}
			/* If we are estalling deal with it */
			if (estall) {
				buildOrAssist(*group, BUILD_EPROVIDER, EMAKER|LAND);
				if (group->busy) continue;
			}
			/* If we don't have a factory, build one */
			if (ai->unittable->factories.empty()) {
				buildOrAssist(*group, BUILD_FACTORY, KBOT|TECH1);
				if (group->busy) continue;
			}
			ATask *task = NULL;
			/* If we can assist a lab and it's close enough, do so */
			if ((task = canAssistFactory(*group)) != NULL) {
				ai->tasks->addAssistTask(*task, *group);
				if (group->busy) continue;
			}
		}
		else {
			/* If we are estalling deal with it */
			if (estall) {
				buildOrAssist(*group, BUILD_EPROVIDER, EMAKER|LAND);
				if (group->busy) continue;
			}
			/* If we are mstalling deal with it */
			if (mstall) {
				buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR|LAND);
				if (group->busy) continue;
			}
			/* If we don't have a factory, build one */
			if (ai->unittable->factories.empty()) {
				buildOrAssist(*group, BUILD_FACTORY, KBOT|TECH1);
				if (group->busy) continue;
			}
			/* See if we can build defense */
			if (ai->defensematrix->getClusters() > ai->unittable->defenses.size()) {
				buildOrAssist(*group, BUILD_AG_DEFENSE, DEFENSE, ANTIAIR);
				if (group->busy) continue;
			}
			/* If we are overflowing energy build a estorage */
			if (eexceeding) {
				if (ai->unittable->energyStorages.size() >= ai->cfgparser->getMaxTechLevel())
					buildOrAssist(*group, BUILD_ESTORAGE, LAND|MMAKER);
				else
					buildOrAssist(*group, BUILD_ESTORAGE, LAND|ESTORAGE);
				if (group->busy) continue;
			}
			/* If we are overflowing metal build an mstorage */
			if (mexceeding) {
				buildOrAssist(*group, BUILD_MSTORAGE, LAND|MSTORAGE);
				if (group->busy) continue;
			}
			/* If both requested, see what is required most */
			if (eRequest && mRequest) {
				if ((mNow / mStorage) > (eNow / eStorage))
					buildOrAssist(*group, BUILD_EPROVIDER, EMAKER|LAND);
				else
					buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR|LAND);
				if (group->busy) continue;
			}
			/* Else just provide that which is requested */
			if (eRequest) {
				buildOrAssist(*group, BUILD_EPROVIDER, EMAKER|LAND);
				if (group->busy) continue;
			}
			if (mRequest) {
				buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR|LAND);
				if (group->busy) continue;
			}
			ATask *task = NULL;
			/* If we can afford to assist a lab and it's close enough, do so */
			if ((task = canAssistFactory(*group)) != NULL) {
				ai->tasks->addAssistTask(*task, *group);
				if (group->busy) continue;
			}
			/* See if we can build a new factory */
			if (!mRequest && !eRequest) {
				int allowedTechlvl = ai->cfgparser->getMaxTechLevel();
				int type = rng.RandInt(2) == 1 ? KBOT : VEHICLE;
				if (!ai->unittable->gotFactory(type|allowedTechlvl))
					buildOrAssist(*group, BUILD_FACTORY, type|allowedTechlvl);
				if (group->busy) continue;
			}
			/* Otherwise just expand */
			if ((mNow / mStorage) > (eNow / eStorage))
				buildOrAssist(*group, BUILD_EPROVIDER, EMAKER|LAND);
			else
				buildOrAssist(*group, BUILD_MPROVIDER, MEXTRACTOR|LAND);
		}
	}

	if (activeGroups.size() < ai->cfgparser->getMaxWorkers() && 
	   (mexceeding || activeGroups.size() < ai->cfgparser->getMinWorkers())
	) ai->wishlist->push(BUILDER, HIGH);
	else if (activeGroups.size() < ai->cfgparser->getMaxWorkers())
		ai->wishlist->push(BUILDER, NORMAL);
}

bool CEconomy::taskInProgress(buildType bt) {
	std::map<int, CTaskHandler::BuildTask*>::iterator i;
	for (i = ai->tasks->activeBuildTasks.begin(); i != ai->tasks->activeBuildTasks.end(); i++) {
		if (i->second->bt == bt)
			return true;
	}
	return false;
}

void CEconomy::preventStalling() {
	/* If factorytask is on wait, unwait him */
	std::map<int, CTaskHandler::FactoryTask*>::iterator k;
	for (k = ai->tasks->activeFactoryTasks.begin(); k != ai->tasks->activeFactoryTasks.end(); k++)
		k->second->setWait(false);

	/* If we aren't stalling, return */
	if (!mstall && !estall)
		return;

	/* If we are only stalling energy, see if we can turn metalmakers off */
	std::map<int, bool>::iterator j;
	if ((estall && !mstall) || (eRequest && !mRequest)) {
		int success = 0;
		for (j = ai->unittable->metalMakers.begin(); j != ai->unittable->metalMakers.end(); j++) {
			if (j->second) {
				CUnit *unit = ai->unittable->getUnit(j->first);
				unit->setOnOff(false);
				j->second = false;
				success++;
			}
		}
		if (success > 0) {
			return;
		}
	}

	/* If we are only stalling metal, see if we can turn metalmakers on */
	if ((mstall && !estall) || (mRequest && !eRequest)) {
		int success = 0;
		for (j = ai->unittable->metalMakers.begin(); j != ai->unittable->metalMakers.end(); j++) {
			if (!j->second) {
				CUnit *unit = ai->unittable->getUnit(j->first);
				unit->setOnOff(true);
				j->second = true;
				success++;
			}
		}
		if (success > 0) {
			return;
		}
	}

	/* Stop all guarding workers */
	std::map<int,CTaskHandler::AssistTask*>::iterator i;
	for (i = ai->tasks->activeAssistTasks.begin(); i != ai->tasks->activeAssistTasks.end(); i++) {
		/* If the assisting group is moving, continue */
		if (i->second->isMoving)
			continue;

		/* Don't stop those workers that are trying to fix the problem */
		if (i->second->assist->t == BUILD) {
			CTaskHandler::BuildTask* build = dynamic_cast<CTaskHandler::BuildTask*>(i->second->assist);
			if ((mstall || mRequest) && build->bt == BUILD_MPROVIDER)
				continue;

			if ((estall || eRequest) && build->bt == BUILD_EPROVIDER)
				continue;
		}

		if (mstall && i->second->group->units.begin()->second->def->isCommander)
			continue;

		if (i->second->assist->t == BUILD || i->second->assist->t == FACTORY_BUILD)
			i->second->remove();
		return;
	}

	/* Wait all factories and their assisters */
	for (k = ai->tasks->activeFactoryTasks.begin(); k != ai->tasks->activeFactoryTasks.end(); k++)
		k->second->setWait(true);

}

void CEconomy::updateIncomes(int frame) {
	incomes++;

	mNowSummed    += ai->cb->GetMetal();
	eNowSummed    += ai->cb->GetEnergy();
	mIncomeSummed += ai->cb->GetMetalIncome();
	eIncomeSummed += ai->cb->GetEnergyIncome();
	mUsageSummed  += ai->cb->GetMetalUsage();
	eUsageSummed  += ai->cb->GetEnergyUsage();
	mStorage       = ai->cb->GetMetalStorage();
	eStorage       = ai->cb->GetEnergyStorage();

	mNow     = ai->cb->GetMetal();
	eNow     = ai->cb->GetEnergy();
	mIncome  = ai->cb->GetMetalIncome();
	eIncome  = ai->cb->GetEnergyIncome();
	mUsage   = alpha*(mUsageSummed / incomes)  + (1.0f-alpha)*(ai->cb->GetMetalUsage());
	eUsage   = beta *(eUsageSummed / incomes)  + (1.0f-beta) *(ai->cb->GetEnergyUsage());

	std::map<int, CUnit*>::iterator i;
	float mU = 0.0f, eU = 0.0f;
	for (i = ai->unittable->activeUnits.begin(); i != ai->unittable->activeUnits.end(); i++) {
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

	mstall     = (mNow < (mStorage*0.1f) && mUsage > mIncome) || mIncome < (mStart*2.0f);
	estall     = (eNow < (eStorage*0.1f) && eUsage > eIncome) || eIncome < (eStart*1.5f);

	mexceeding = (mNow > (mStorage*0.9f) && mUsage < mIncome);
	eexceeding = (eNow > (eStorage*0.9f) && eUsage < eIncome);

	mRequest   = (mNow < (mStorage*0.5f));
	eRequest   = (eNow < (eStorage*0.5f));

	state = ai->cfgparser->determineState(mIncome, eIncome);
}

ATask* CEconomy::canAssist(buildType t, CGroup &group) {
	std::map<int, CTaskHandler::BuildTask*>::iterator i;
	std::multimap<float, CTaskHandler::BuildTask*> suited;
	for (i = ai->tasks->activeBuildTasks.begin(); i != ai->tasks->activeBuildTasks.end(); i++) {
		CTaskHandler::BuildTask *buildtask = i->second;

		/* Don't build TECH1 mexes together */
		unsigned c = buildtask->toBuild->cats;
		if (c&MEXTRACTOR && c&TECH1)
			continue;

		/* Only build tasks we are interested in */
		float travelTime;
		if (buildtask->bt != t || !buildtask->assistable(group, travelTime))
			continue;

		
		suited.insert(std::pair<float, CTaskHandler::BuildTask*>(travelTime, buildtask));
	}

	/* There are no suited tasks that require assistance */
	if (suited.empty())
		return NULL;

	bool isCommander = group.units.begin()->second->def->isCommander;
	bool isToFar     = suited.begin()->first > 30*10;
	if (isCommander && isToFar)
		return NULL;

	return suited.begin()->second;
}

ATask* CEconomy::canAssistFactory(CGroup &group) {
	CUnit *unit = group.units.begin()->second;
	std::map<int, CTaskHandler::FactoryTask*>::iterator i;
	std::map<float, CTaskHandler::FactoryTask*> candidates;
	float3 pos = group.pos();
	for (i = ai->tasks->activeFactoryTasks.begin(); i != ai->tasks->activeFactoryTasks.end(); i++) {
		/* TODO: instead of euclid distance, use pathfinder distance */
		float dist = (pos - i->second->pos).Length2D();
		candidates[dist] = i->second;
	}

	if (candidates.empty())
		return NULL;

	if (unit->def->isCommander)
		return candidates.begin()->second;

	std::map<float, CTaskHandler::FactoryTask*>::iterator j;
	for (j = candidates.begin(); j != candidates.end(); j++) {
		if (!j->second->assistable(group))
			continue;
		return j->second;
	}

	return NULL;
}

bool CEconomy::canAffordToBuild(CGroup &group, UnitType *utToBuild) {
	/* NOTE: "Salary" is provided every 32 logical frames */
	float buildTime   = (utToBuild->def->buildTime / group.buildSpeed) * 32.0f;
	float mCost       = utToBuild->def->metalCost;
	float eCost       = utToBuild->def->energyCost;
	float mPrediction = (mIncome - mUsage - mCost/buildTime)*buildTime - mCost + mNow;
	float ePrediction = (eIncome - eUsage - eCost/buildTime)*buildTime - eCost + eNow;
	mRequest          = mPrediction < 0.0f;
	eRequest          = ePrediction < 0.0f;
	return (mPrediction >= 0.0f && ePrediction >= 0.0f);
}
