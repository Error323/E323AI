#include "CEconomy.h"

CEconomy::CEconomy(AIClasses *ai): ARegistrar(700) {
	this->ai = ai;
	incomes  = 0;
	mNow     = mNowSummed     = eNow     = eNowSummed     = 0.0f;
	mIncome  = mIncomeSummed  = eIncome  = eIncomeSummed  = 0.0f;
	uMIncome = uMIncomeSummed = uEIncome = uEIncomeSummed = 0.0f;
	mUsage   = mUsageSummed   = eUsage   = eUsageSummed   = 0.0f;
	mStorage = eStorage                                   = 0.0f;
}

void CEconomy::init(CUnit &unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit.key);
	UnitType *utCommander = UT(ud->id);
	mRequest = eRequest = false;

	factory = ai->unitTable->canBuild(utCommander, KBOT|TECH1);
	mex = ai->unitTable->canBuild(utCommander, MEXTRACTOR);
	UnitType *utWind  = ai->unitTable->canBuild(utCommander, EMAKER|WIND);
	utSolar = ai->unitTable->canBuild(utCommander, EMAKER);

	builder = ai->unitTable->canBuild(factory, BUILDER|MOBILE);

	float avgWind   = (ai->call->GetMinWind() + ai->call->GetMaxWind()) / 2.0f;
	float windProf  = avgWind / utWind->cost;
	float solarProf = utSolar->energyMake / utSolar->cost;

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
		unit.moveForward(-70.0f);
		CGroup *group = requestGroup();
		group->addUnit(unit);
	}

	else if (c&MMAKER) {
		ai->unitTable->metalMakers[unit.key] = false;
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
	mstall = false;
	stalling = (mstall || estall);
}

void CEconomy::buildEprovider(CGroup &group) {
	/* Handle energy income */
	buildOrAssist(BUILD_EPROVIDER, energyProvider, group);
	eRequest = false;
	estall = false;
	stalling = (mstall || estall);
}

void CEconomy::buildOrAssist(buildType bt, UnitType *ut,  CGroup &group) {
	ATask *task = canAssist(bt, group);
	float3 pos = group.pos();
	if (task != NULL)
		ai->tasks->addAssistTask(*task, group);
	else {
		UnitType *mex = ai->unitTable->canBuild(group.units.begin()->second->type, MEXTRACTOR);
		if (ut->id == energyProvider->id || ut->id == mex->id)
			ai->tasks->addBuildTask(bt, ut, group, pos);
		else if (canAffordToBuild(group, ut))
			ai->tasks->addBuildTask(bt, ut, group, pos);
	}
}

void CEconomy::update(int frame) {
	/* If we are stalling, do something about it */
	if (frame % 10 == 0) preventStalling();

	/* Update idle worker groups */
	std::map<int, CGroup*>::iterator i;
	for (i = activeGroups.begin(); i != activeGroups.end(); i++) {
		CGroup *group = i->second;
		if (group->busy) continue;

		CUnit *unit = group->units.begin()->second;
		float3 pos = group->pos();

		/* If we are stalling deal with it */
		if (stalling) {
			if (estall)
				buildEprovider(*group);
			else
				buildMprovider(*group);
		}
		/* If we don't have a factory, build one */
		else if (ai->unitTable->factories.empty()) {
			if (canAffordToBuild(*group, factory))
				ai->tasks->addBuildTask(BUILD_FACTORY, factory, *group, pos);
		}
		/* If we require more income deal with it */
		else if (mRequest || eRequest) {
			/* If both requested, see who requires most */
			if (eRequest && mRequest) {
				if ((mNow / mStorage) > (eNow / eStorage))
					buildEprovider(*group);
				else
					buildMprovider(*group);
			}
			/* Else just provide that which is requested */
			else {
				if (eRequest)
					buildEprovider(*group);
				else
					buildMprovider(*group);
			}
		}
		else {
			ATask *task;
			/* If we are overflowing build a storage */
			if (eexceeding && !taskInProgress(BUILD_ESTORAGE)) {
				UnitType *storage = ai->unitTable->canBuild(unit->type, ESTORAGE);
				buildOrAssist(BUILD_ESTORAGE, storage, *group);
			}
			else if (mexceeding && !taskInProgress(BUILD_MSTORAGE)) {
				UnitType *storage = ai->unitTable->canBuild(unit->type, MSTORAGE);
				buildOrAssist(BUILD_MSTORAGE, storage, *group);
			}
			/* If we can afford to assist a lab and it's close enough, do so */
			else if ((task = canAssistFactory(*group)) != NULL)
				ai->tasks->addAssistTask(*task, *group);
			/* See if we can build a new factory */
			else if (!taskInProgress(BUILD_FACTORY)) {
				UnitType *factory = ai->unitTable->canBuild(unit->type, KBOT|TECH2);
				buildOrAssist(BUILD_FACTORY, factory, *group);
			}
		}
	}

	if (activeGroups.size() <= 1)
		ai->wl->push(BUILDER, HIGH);

	if (mexceeding)
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
	std::map<int, bool>::iterator j;

	/* If we aren't stalling, return */
	if (!stalling)
		return;

	/* If we are only stalling energy, see if we can turn metalmakers off */
	if (estall) {
		for (j = ai->unitTable->metalMakers.begin(); j != ai->unitTable->metalMakers.end(); j++) {
			if (j->second) {
				CUnit *unit = ai->unitTable->getUnit(j->first);
				unit->setOnOff(false);
				j->second = false;
				if (!mstall)
					return;
			}
		}
	}

	/* If we are only stalling metal, see if we can turn metalmakers on */
	if (mstall) {
		for (j = ai->unitTable->metalMakers.begin(); j != ai->unitTable->metalMakers.end(); j++) {
			if (!j->second) {
				CUnit *unit = ai->unitTable->getUnit(j->first);
				unit->setOnOff(true);
				j->second = true;
				if (!estall)
					return;
			}
		}
	}

	/* Stop all guarding workers */
	std::map<int,CTaskHandler::AssistTask*>::iterator i;
	for (i = ai->tasks->activeAssistTasks.begin(); i != ai->tasks->activeAssistTasks.end(); i++) {
		/* If the assisting group isn't moving, but actually assisting make them stop */
		if (i->second->isMoving) 
			continue;

		i->second->remove();
		return;
	}
}

void CEconomy::updateIncomes(int frame) {
	/* incomes unreliable before this frame, bah */
	if (frame < 65) return;
	incomes++;

	mNowSummed    += ai->call->GetMetal();
	eNowSummed    += ai->call->GetEnergy();
	mIncomeSummed += ai->call->GetMetalIncome();
	eIncomeSummed += ai->call->GetEnergyIncome();
	mUsageSummed  += ai->call->GetMetalUsage();
	eUsageSummed  += ai->call->GetEnergyUsage();
	mStorage       = ai->call->GetMetalStorage();
	eStorage       = ai->call->GetEnergyStorage();

	mNow     = alpha*(mNowSummed / incomes)    + (1.0f-alpha)*(ai->call->GetMetal());
	eNow     = beta *(eNowSummed / incomes)    + (1.0f-beta) *(ai->call->GetEnergy());
	mIncome  = alpha*(mIncomeSummed / incomes) + (1.0f-alpha)*(ai->call->GetMetalIncome());
	eIncome  = beta *(eIncomeSummed / incomes) + (1.0f-beta) *(ai->call->GetEnergyIncome());
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

	mstall     = (mNow < (30.0f+mIncome) && mUsage > mIncome);
	estall     = (eNow < (3.0f*eIncome) && eUsage > eIncome);
	stalling   = (mstall || estall);

	eexceeding = (eNow > eStorage*0.9f);
	mexceeding = (mNow > mStorage*0.9f);
	exceeding  = (mexceeding || eexceeding);
	if (mUsage > mIncome && mNow < mStorage/6.0f) mRequest = true;
	if (eUsage > eIncome && eNow < eStorage/4.0f) eRequest = true;
	if (eexceeding) eRequest = false;
	if (mexceeding) mRequest = false;

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
		float builderdist = (buildtask->pos - buildtask->group->pos()).Length2D();
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
	float mPrediction = (mIncome-mUsage)*buildTime - mCost + mNow;
	float ePrediction = (eIncome-eUsage)*buildTime - eCost + eNow;
	if (mPrediction < 0.0f) mRequest = true;
	if (ePrediction < 0.0f) eRequest = true;
	return (mPrediction >= 0.0f && ePrediction >= 0.0f);
}
