#include "CEconomy.h"

CEconomy::CEconomy(AIClasses *ai): ARegistrar(700) {
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
	const UnitDef *ud = ai->call->GetUnitDef(unit.key);
	UnitType *utCommander = UT(ud->id);

	UnitType *utWind  = ai->unitTable->canBuild(utCommander, EMAKER|WIND);
	UnitType *utSolar = ai->unitTable->canBuild(utCommander, EMAKER);

	float avgWind   = (ai->call->GetMinWind() + ai->call->GetMaxWind()) / 2.0f;
	float windProf  = avgWind / utWind->cost;
	float solarProf = utSolar->energyMake / utSolar->cost;
	mStart = utCommander->def->metalMake;
	eStart = utCommander->def->energyMake;

	energyProvider  = windProf > solarProf ? utWind : utSolar;
}
		
bool CEconomy::hasFinishedBuilding(CGroup &group) {
	std::map<int, CUnit*>::iterator i;
	for (i = group.units.begin(); i != group.units.end(); i++) {
		CUnit *unit = i->second;
		if (ai->unitTable->builders.find(unit->key) != ai->unitTable->builders.end() &&
			ai->unitTable->builders[unit->key]) {
			ai->unitTable->builders[unit->key] = false;
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
	sprintf(buf, "[CEconomy::remove]\tremove group(%d)", group.key);
	LOGN(buf);
	free.push(lookup[group.key]);
	lookup.erase(group.key);
	activeGroups.erase(group.key);

	std::list<ARegistrar*>::iterator i;
	for (i = records.begin(); i != records.end(); i++)
		(*i)->remove(group);
}

void CEconomy::addUnit(CUnit &unit) {
	sprintf(buf, "[CEconomy::addUnit]\tadded unit(%d):%s", unit.key, unit.def->humanName.c_str());
	LOGN(buf);
	unsigned c = unit.type->cats;
	if (c&FACTORY) {
		ai->tasks->addFactoryTask(unit);
		ai->unitTable->factories[unit.key] = true;
	}

	else if (c&BUILDER && c&MOBILE) {
		unit.moveForward(-100.0f);
		CGroup *group = requestGroup();
		group->addUnit(unit);
	}

	else if (c&MMAKER) {
		ai->unitTable->metalMakers[unit.key] = true;
	}
}

void CEconomy::buildMprovider(CGroup &group) {
	/* Handle metal income */
	CUnit *unit = group.units.begin()->second;
	ATask *task = canAssist(BUILD_MPROVIDER, group);
	float3 pos  = group.pos();
	if (task != NULL)
		ai->tasks->addAssistTask(*task, group);
	else  {
		float3 mexPos;
		bool canBuildMex = ai->metalMap->getMexSpot(group, mexPos);
		if (canBuildMex) {
			UnitType *mex = ai->unitTable->canBuild(unit->type, MEXTRACTOR);
			ai->tasks->addBuildTask(BUILD_MPROVIDER, mex, group, mexPos);
		}
		else {
			UnitType *mmaker = ai->unitTable->canBuild(unit->type, MMAKER);
			ai->tasks->addBuildTask(BUILD_MPROVIDER, mmaker, group, pos);
		}
	}
	mRequest = false;
}

void CEconomy::buildEprovider(CGroup &group) {
	/* Handle energy income */
	buildOrAssist(BUILD_EPROVIDER, energyProvider, group);
	eRequest = false;
}

void CEconomy::buildOrAssist(buildType bt, UnitType *ut,  CGroup &group) {
	ATask *task = canAssist(bt, group);
	float3 pos = group.pos();
	if (task != NULL)
		ai->tasks->addAssistTask(*task, group);
	else {
		if (bt == BUILD_EPROVIDER || bt == BUILD_MPROVIDER)
			ai->tasks->addBuildTask(bt, ut, group, pos);
		else if (canAffordToBuild(group, ut) && !taskInProgress(bt))
			ai->tasks->addBuildTask(bt, ut, group, pos);
	}
}

void CEconomy::update(int frame) {
	/* If we are stalling, do something about it */
	preventStalling();

	/* Update idle worker groups */
	std::map<int, CGroup*>::iterator i;
	for (i = activeGroups.begin(); i != activeGroups.end(); i++) {
		CGroup *group = i->second;
		if (group->busy) continue;

		CUnit *unit = group->units.begin()->second;
		float3 pos = group->pos();

		if (unit->def->isCommander) {
			/* If we are mstalling deal with it */
			if (mstall) {
				buildMprovider(*group);
				if (group->busy) continue;
			}
			/* If we are estalling deal with it */
			if (estall) {
				buildEprovider(*group);
				if (group->busy) continue;
			}
			/* If we don't have a factory, build one */
			if (ai->unitTable->factories.empty()) {
				UnitType *factory = ai->unitTable->canBuild(unit->type, KBOT|TECH1);
				buildOrAssist(BUILD_FACTORY, factory, *group);
				if (group->busy) continue;
			}
			ATask *task = NULL;
			/* If we can assist a lab and it's close enough, do so */
			if ((task = canAssistFactory(*group)) != NULL) {
				ai->tasks->addAssistTask(*task, *group);
				if (group->busy) continue;
			}
			/* See if we can build a new factory */
			UnitType *factory = ai->unitTable->canBuild(unit->type, AIR|TECH1);
			buildOrAssist(BUILD_FACTORY, factory, *group);
		}
		else {
			/* If we are mstalling deal with it */
			if (mstall) {
				buildMprovider(*group);
				if (group->busy) continue;
			}
			/* If we are estalling deal with it */
			if (estall) {
				buildEprovider(*group);
				if (group->busy) continue;
			}
			/* If we don't have a factory, build one */
			if (ai->unitTable->factories.empty()) {
				UnitType *factory = ai->unitTable->canBuild(unit->type, KBOT|TECH1);
				buildOrAssist(BUILD_FACTORY, factory, *group);
				if (group->busy) continue;
			}
			/* If we are overflowing energy build a estorage */
			if (eexceeding) {
				UnitType *storage = ai->unitTable->canBuild(unit->type, ESTORAGE);
				buildOrAssist(BUILD_ESTORAGE, storage, *group);
				if (group->busy) continue;
			}
			/* If we are overflowing metal build an mstorage */
			if (mexceeding) {
				UnitType *storage = ai->unitTable->canBuild(unit->type, MSTORAGE);
				buildOrAssist(BUILD_MSTORAGE, storage, *group);
				if (group->busy) continue;
			}
			/* If both requested, see what is required most */
			if (eRequest && mRequest) {
				if ((mNow / mStorage) > (eNow / eStorage))
					buildEprovider(*group);
				else
					buildMprovider(*group);
				if (group->busy) continue;
			}
			/* Else just provide that which is requested */
			if (eRequest) {
				buildEprovider(*group);
				if (group->busy) continue;
			}
			if (mRequest) {
				buildMprovider(*group);
				if (group->busy) continue;
			}
			ATask *task = NULL;
			/* If we can afford to assist a lab and it's close enough, do so */
			if ((task = canAssistFactory(*group)) != NULL) {
				ai->tasks->addAssistTask(*task, *group);
				if (group->busy) continue;
			}
			/* See if we can build a new factory */
			UnitType *factory = ai->unitTable->canBuild(unit->type, KBOT|TECH2);
			buildOrAssist(BUILD_FACTORY, factory, *group);
		}
	}

	if (mexceeding || activeGroups.size() < 2)
		ai->wl->push(BUILDER, HIGH);
	else
		ai->wl->push(BUILDER, NORMAL);
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
	if (estall && !mstall) {
		int success = 0;
		for (j = ai->unitTable->metalMakers.begin(); j != ai->unitTable->metalMakers.end(); j++) {
			if (j->second) {
				CUnit *unit = ai->unitTable->getUnit(j->first);
				unit->setOnOff(false);
				j->second = false;
				success++;
			}
		}
		if (success > 0) {
			sprintf(buf, "[CEconomy::preventStalling]\tturned %d metalmakers off\n", success);
			LOGN(buf);
			return;
		}
	}

	/* If we are only stalling metal, see if we can turn metalmakers on */
	if (mstall && !estall) {
		int success = 0;
		for (j = ai->unitTable->metalMakers.begin(); j != ai->unitTable->metalMakers.end(); j++) {
			if (!j->second) {
				CUnit *unit = ai->unitTable->getUnit(j->first);
				unit->setOnOff(true);
				j->second = true;
				success++;
			}
		}
		if (success > 0) {
			sprintf(buf, "[CEconomy::preventStalling]\tturned %d metalmakers on\n", success);
			LOGN(buf);
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

	mNowSummed    += ai->call->GetMetal();
	eNowSummed    += ai->call->GetEnergy();
	mIncomeSummed += ai->call->GetMetalIncome();
	eIncomeSummed += ai->call->GetEnergyIncome();
	mUsageSummed  += ai->call->GetMetalUsage();
	eUsageSummed  += ai->call->GetEnergyUsage();
	mStorage       = ai->call->GetMetalStorage();
	eStorage       = ai->call->GetEnergyStorage();

	mNow     = ai->call->GetMetal();
	eNow     = ai->call->GetEnergy();
	mIncome  = ai->call->GetMetalIncome();
	eIncome  = ai->call->GetEnergyIncome();
	mUsage   = alpha*(mUsageSummed / incomes)  + (1.0f-alpha)*(ai->call->GetMetalUsage());
	eUsage   = beta *(eUsageSummed / incomes)  + (1.0f-beta) *(ai->call->GetEnergyUsage());

	std::map<int, CUnit*>::iterator i;
	float mU = 0.0f, eU = 0.0f;
	for (i = ai->unitTable->activeUnits.begin(); i != ai->unitTable->activeUnits.end(); i++) {
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

/*
	printf("min(%0.2f)\tmout(%0.2f)\tmnow(%0.2f)\tmstor(%0.2f)\tmstall(%d)\tmreq(%d)\n",
			mIncome,
			mUsage,
			mNow,
			mStorage,
			mstall,
			mRequest
	);
	printf("ein(%0.2f)\teout(%0.2f)\tenow(%0.2f)\testor(%0.2f)\testall(%d)\tereq(%d)\n\n",
			eIncome,
			eUsage,
			eNow,
			eStorage,
			estall,
			eRequest
	);
*/


}

ATask* CEconomy::canAssist(buildType t, CGroup &group) {
	std::map<int, CTaskHandler::BuildTask*>::iterator i;
	std::map<float, CTaskHandler::BuildTask*> suited;
	std::map<float, CTaskHandler::BuildTask*>::iterator best;
	float3 pos = group.pos();
	for (i = ai->tasks->activeBuildTasks.begin(); i != ai->tasks->activeBuildTasks.end(); i++) {
		CTaskHandler::BuildTask *buildtask = i->second;

		/* Only build tasks we are interested in */
		if (buildtask->bt != t || !buildtask->assistable())
			continue;

		/* TODO: instead of euclid distance, use pathfinder distance */
		float3 grouppos   = buildtask->group->pos();
		float builderdist = (buildtask->pos - grouppos).Length2D();
		float dist        = (pos - buildtask->pos).Length2D() - builderdist;
		suited[dist]      = buildtask;
	}

	/* There are no suited tasks that require assistance */
	if (suited.empty())
		return NULL;

	/* See if we can get there in time */
	best = suited.begin();
	float buildTime  = (best->second->toBuild->def->buildTime / group.buildSpeed) * 32.0f;
	float travelTime = best->first / group.speed;
	if (travelTime < buildTime)
		return best->second;
	else
		return NULL;
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
		if (j->second->assisters.size() > 5)
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
	float mPrediction = (mIncome - mUsage)*buildTime - mCost + mNow;
	float ePrediction = (eIncome - eUsage)*buildTime - eCost + eNow;
	mRequest          = mPrediction < 0.0f;
	eRequest          = ePrediction < 0.0f;
	return (mPrediction >= 0.0f && ePrediction >= 0.0f);
}
