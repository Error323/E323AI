#include "CEconomy.h"

CEconomy::CEconomy(AIClasses *ai) {
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
	gameIdle[unit.key] = utCommander;
	mRequest = eRequest = false;

	factory = ai->unitTable->canBuild(utCommander, KBOT|TECH1);
	mex = ai->unitTable->canBuild(utCommander, MEXTRACTOR);
	UnitType *utWind  = ai->unitTable->canBuild(utCommander, EMAKER|WIND);
	utSolar = ai->unitTable->canBuild(utCommander, EMAKER);

	builder = ai->unitTable->canBuild(factory, BUILDER|MOBILE);

	float avgWind = (ai->call->GetMinWind() + ai->call->GetMaxWind()) / 2.0f;
	float windProf  = avgWind / utWind->cost;
	float solarProf = utSolar->energyMake / utSolar->cost;

	energyProvider = windProf > solarProf ? utWind : utSolar;
	sprintf(buf, "Energy provider: %s", energyProvider->def->humanName.c_str());
	LOGN(buf);
}

void CEconomy::addUnit(CUnit &unit) {
	unsigned c = unit.type->cats;
	if (c&FACTORY)
		gameFactories[unit.key] = false;

	else if (c&BUILDER && c&MOBILE)
		unit.moveForward(-70.0f);

	else if (c&MMAKER)
		gameMetalMakers[unit.key] = true;
}

void CEconomy::update(int frame) {
	/* If we are stalling, do something about it */
	preventStalling();

	/* Update tables */
	updateTables();

	/* Update idle units */
	std::map<int, UnitType*>::iterator i;
	for (i = gameIdle.begin(); i != gameIdle.end(); i++) {
		UnitType *ut     = i->second;
		unsigned int c   = ut->cats;
		float3       pos = ai->call->GetUnitPos(i->first);
		
		if (c&FACTORY) {
			/* There are no wishes */
			if (ai->wl->empty(i->first)) continue;

			UnitType *utToBuild = ai->wl->top(i->first);
			if (canAffordToBuild(i->first, utToBuild)) {
				ai->metaCmds->factoryBuild(i->first, utToBuild);
				gameFactoriesBuilding[i->first] = utToBuild;
				ai->wl->pop(i->first);
			}
			else gameFactoriesBuilding[i->first] = NULL;
		}

		else if (c&COMMANDER) {
			int fac;
			/* If we don't have enough metal income, build a mex */
			if ((mIncome - uMIncome) < 2.0f) {
				bool canBuildMex = ai->metalMap->buildMex(i->first, mex);
				if (!canBuildMex) {
					UnitType *mmaker = ai->unitTable->canBuild(ut, MMAKER);
					ai->metaCmds->build(i->first, mmaker, pos);
				}
			}
			/* If we don't have a factory, build one */
			else if (gameFactories.empty()) {
				if (canAffordToBuild(i->first, factory))
					ai->metaCmds->build(i->first, factory, pos);
			}
			/* If we don't have enough energy income, build a energy provider */
			else if (estall || eRequest) {
				if (canAffordToBuild(i->first, energyProvider)) {
					ai->metaCmds->build(i->first, energyProvider, pos);
					eRequest = false;
				}
				else if (energyProvider->id != utSolar->id) {
					if (canAffordToBuild(i->first, utSolar)) {
						ai->metaCmds->build(i->first, utSolar, pos);
						eRequest = false;
					}
				}
			}
			/* If we can afford to assist a factory, do so */
			else if (canAffordToAssistFactory(i->first,fac)) {
				ai->metaCmds->guard(i->first, fac);
			}
		}

		else if (c&BUILDER && c&MOBILE) {
			int fac;
			/* Increase eco income */
			if (stalling || mRequest || eRequest) {
				int toHelp = 0;
				if (mRequest || mstall) {
					bool canhelp = canHelp(BUILD_MMAKER, i->first, toHelp, mex);
					if (canhelp)
						ai->metaCmds->guard(i->first, toHelp);
					else  {
						bool canBuildMex = ai->metalMap->buildMex(i->first, mex);
						if (!canBuildMex) {
							UnitType *mmaker = ai->unitTable->canBuild(ut, MMAKER);
							ai->metaCmds->build(i->first, mmaker, pos);
						}
					}
					mRequest = false;
				}
				else if (eRequest || estall) {
					bool canhelp = canHelp(BUILD_EMAKER, i->first, toHelp, energyProvider);
 					//FIXME: segfaults sometimes, cause by toHelp being 0x0 ??
					if (canhelp)
						ai->metaCmds->guard(i->first, toHelp);
					else
						ai->metaCmds->build(i->first, energyProvider, pos);
					eRequest = false;
				}
			}
			/* If we can afford to assist a lab and it's close enough, do so */
			else if (canAffordToAssistFactory(i->first, fac)) {
				float3 facpos = ai->call->GetUnitPos(fac);
				float dist = (pos - facpos).Length2D();
				float travelTime = dist / (ut->def->speed/30.0f);
				float buildTime = builder->def->buildTime / (factory->def->buildSpeed/32.0f);
				if (travelTime <= buildTime)
					ai->metaCmds->guard(i->first, fac);
				else if (gameFactories.size() < 2 && exceeding){
					UnitType *lab = ai->unitTable->canBuild(ut, KBOT|TECH2);
					if (lab == NULL)
						lab = ai->unitTable->canBuild(ut, VEHICLE|TECH1);
					ai->metaCmds->build(i->first, lab, pos);
				}
			}
		}
	}

	if (gameBuilders.size() <= 1)
		ai->wl->push(BUILDER, HIGH);
	if (stalling || exceeding || mRequest)
		ai->wl->push(BUILDER, NORMAL);
}

void CEconomy::preventStalling() {
	std::map<int,int>::iterator i;
	std::map<int, bool>::iterator j;
	mstall = (mNow < 30.0f && mUsage > mIncome);
	estall = (eNow/eStorage < 0.1f && eUsage > eIncome);
	stalling = mstall || estall;
	exceeding = (eNow > eStorage*0.9f && eUsage < eIncome) || (mNow > mStorage*0.9f && mUsage < mIncome);

	/* Always remove all previous waiting factories */
	for (j = gameFactories.begin(); j != gameFactories.end(); j++) {
		if (!j->second) {
			ai->metaCmds->wait(j->first);
			j->second = true;
		}
	}

	/* If we aren't stalling, return */
	if (!stalling) return;

	/* If we are only stalling energy, see if we can turn metalmakers off */
	if (estall && !mstall && !gameMetalMakers.empty()) {
		for (j = gameMetalMakers.begin(); j != gameMetalMakers.end(); j++) {
			if (j->second) {
				ai->metaCmds->setOnOff(j->first, false);
				j->second = false;
				return;
			}
		}
	}
	/* If we are only stalling metal, see if we can turn metalmakers on */
	if (mstall && !estall && !gameMetalMakers.empty()) {
		for (j = gameMetalMakers.begin(); j != gameMetalMakers.end(); j++) {
			if (!j->second) {
				ai->metaCmds->setOnOff(j->first, true);
				j->second = true;
				return;
			}
		}
	}
	/* Stop all guarding workers */
	if (!gameGuarding.empty()) {
		for (i = gameGuarding.begin(); i != gameGuarding.end(); i++) {
			ai->metaCmds->stop(i->first);
			const UnitDef *guarder = ai->call->GetUnitDef(i->first);
			removeFromGuarding.push_back(i->first);
			gameIdle[i->first] = UT(guarder->id);
			return;
		}
	}
	/* As our last resort, put factories on wait */
	for (j = gameFactories.begin(); j != gameFactories.end(); j++) {
		ai->metaCmds->wait(j->first);
		j->second = false;
	}
}

bool CEconomy::canHelp(task t, int helper, int &unit, UnitType *utToBuild) {
	std::vector<int> busyUnits; 
	ai->tasks->getBuildTasks(t, busyUnits);
	UnitType *utHelper = UT(ai->call->GetUnitDef(helper)->id);

	if (busyUnits.empty())
		return false;
	else {
		float buildTime = utToBuild->def->buildTime / (utHelper->def->buildSpeed/32.0f);
		for (unsigned int uid = 0; uid < busyUnits.size(); uid++) {
			float3 posToHelp = ai->call->GetUnitPos(busyUnits[uid]);
			float3 posHelper = ai->call->GetUnitPos(helper);
			float pathLength = (posHelper - posToHelp).Length2D();
			float travelTime  = pathLength / (utHelper->def->speed/30.0f);
			if (travelTime <= buildTime && getGuardings(busyUnits[uid]) < 1) {
				/* Only if the worker itself isn't guarding */
				if (gameGuarding.find(busyUnits[uid]) == gameGuarding.end()) {
					unit = busyUnits[uid];
					return true;
				}
			}
		}
	}
	return false;
}

int CEconomy::getGuardings(int unit) {
	std::map<int,int>::iterator g;
	int guarders = 0;
	for (g = gameGuarding.begin(); g != gameGuarding.end(); g++) {
		if (g->second == unit)
			guarders++;
	}
	return guarders;
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
	eNow     = alpha*(eNowSummed / incomes)    + (1.0f-alpha)*(ai->call->GetEnergy());
	mIncome  = alpha*(mIncomeSummed / incomes) + (1.0f-alpha)*(ai->call->GetMetalIncome());
	eIncome  = alpha*(eIncomeSummed / incomes) + (1.0f-alpha)*(ai->call->GetEnergyIncome());
	mUsage   = alpha*(mUsageSummed / incomes)  + (1.0f-alpha)*(ai->call->GetMetalUsage());
	eUsage   = alpha*(eUsageSummed / incomes)  + (1.0f-alpha)*(ai->call->GetEnergyUsage());

	std::map<int, UnitType*>::iterator i;
	float mU = 0.0f, eU = 0.0f;
	for (i = ai->unitTable->gameAllUnits.begin(); i != ai->unitTable->gameAllUnits.end(); i++) {
		unsigned int c = i->second->cats;
		if (!(c&MMAKER) || !(c&EMAKER) || !(c&MEXTRACTOR)) {
			mU += i->second->metalMake;
			eU += i->second->energyMake;
		}
	}
	uMIncomeSummed += mU;
	uEIncomeSummed += eU;

	uMIncome = alpha*(uMIncomeSummed / incomes) + (1.0f-alpha)*mU;
	uEIncome = alpha*(uEIncomeSummed / incomes) + (1.0f-alpha)*eU;
}

void CEconomy::updateTables() {
	/* Remove previous guarders */
	for (unsigned int i = 0; i < removeFromGuarding.size(); i++)
		gameGuarding.erase(removeFromGuarding[i]);
	removeFromGuarding.clear();

	/* Remove previous idlers */
	for (unsigned int i = 0; i < removeFromIdle.size(); i++)
		gameIdle.erase(removeFromIdle[i]);
	removeFromIdle.clear();
}

bool CEconomy::canAffordToAssistFactory(int unit, int &fac) {
	if (gameFactories.empty()) return false;
	float3 upos = ai->call->GetUnitPos(unit);
	std::map<int,bool>::iterator i;
	float smallestDist = MAX_FLOAT;
	for (i = gameFactories.begin(); i != gameFactories.end(); i++) {
		float3 facpos = ai->call->GetUnitPos(i->first);
		float  dist = (facpos - upos).Length2D();
		if (dist <= smallestDist) {
			fac = i->first;
			smallestDist = dist;
		}
	}
	UnitType *building = gameFactoriesBuilding[fac];
	return (building == NULL || canAffordToBuild(unit, building));
}

bool CEconomy::canAffordToBuild(int unit, UnitType *utToBuild) {
	/* NOTE: "Salary" is provided every 32 logical frames */
	UnitType *builder = UT(ai->call->GetUnitDef(unit)->id);
	float buildTime   = (utToBuild->def->buildTime / (builder->def->buildSpeed/32.0f)) / 32.0f;
	float mCost       = utToBuild->def->metalCost;
	float eCost       = utToBuild->def->energyCost;
	float mPrediction = (mIncome-mUsage)*buildTime - mCost + mNow;
	float ePrediction = (eIncome-eUsage)*buildTime - eCost + eNow;
	if (mPrediction < 0.0f) mRequest = true;
	if (ePrediction < 0.0f) eRequest = true;
	return (mPrediction >= 0.0f &&  ePrediction >= 0.0f);
}

void CEconomy::removeIdleUnit(int unit) {
	removeFromIdle.push_back(unit);
}

void CEconomy::removeMyGuards(int unit) {
	std::map<int,int>::iterator i;
	for (i = gameGuarding.begin(); i != gameGuarding.end(); i++) {
		if (i->second == unit) {
			ai->metaCmds->stop(i->first);
			const UnitDef *guarder = ai->call->GetUnitDef(i->first);
			gameIdle[i->first] = UT(guarder->id);
			removeFromGuarding.push_back(i->first);
		}
	}
}
