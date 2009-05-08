#include "Economy.h"

CEconomy::CEconomy(AIClasses *ai) {
	incomes = 0;
	mNow = mNowSummed = eNow = eNowSummed = 0.0f;
	mIncome = mIncomeSummed = eIncome = eIncomeSummed = 0.0f;
	uMIncome = uMIncomeSummed = uEIncome = uEIncomeSummed = 0.0f;
	mUsage = mUsageSummed = eUsage = eUsageSummed = 0.0f;
	mStorage = eStorage = 0.0f;
	this->ai = ai;
}

void CEconomy::init(int unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	UnitType *ut = UT(ud->id);
	gameIdle[unit] = ut;
	mRequest = eRequest = false;

	// XXX: temporary artillery kbot
	hammer = UT(69);

	/* XXX: temporary factory, kbot lab(82) */
	factory = UT(82);
	builder = UT(34);

	/* XXX: temporary, determine wind(172) or solar(136) energy */
	UnitType *wind  = UT(172);
	UnitType *solar = UT(136);
	
	float avgWind = (ai->call->GetMinWind() + ai->call->GetMaxWind()) / 2.0f;
	float windProf  = avgWind / wind->cost;
	float solarProf = solar->energyMake / solar->cost;

	energyProvider = windProf > solarProf ? wind : solar;
	sprintf(buf, "Energy provider: %s", energyProvider->def->humanName.c_str());
	LOGN(buf);
}

void CEconomy::update(int frame) {
	/* If we are stalling, stop guarding workers (1 per update call), if there
	 * are none, set a wait factories until we stall no more */
	preventStalling();

	/* Remove previous guarders */
	for (unsigned int i = 0; i < removeFromGuarding.size(); i++)
		gameGuarding.erase(removeFromGuarding[i]);
	removeFromGuarding.clear();

	/* Remove previous idlers */
	for (unsigned int i = 0; i < removeFromIdle.size(); i++)
		gameIdle.erase(removeFromIdle[i]);
	removeFromIdle.clear();

	/* Remove previous waiters */
	for (unsigned int i = 0; i < removeFromWaiting.size(); i++)
		gameWaiting.erase(removeFromWaiting[i]);
	removeFromWaiting.clear();

	/* Update idle units */
	std::map<int, UnitType*>::iterator i;
	for (i = gameIdle.begin(); i != gameIdle.end(); i++) {
		UnitType *ut     = i->second;
		unsigned int c   = ut->cats;
		int          u   = ut->id;
		float3       pos = ai->call->GetUnitPos(i->first);
		
		if (c&FACTORY) {
			std::map<int, std::priority_queue<Wish> >::iterator j;
			j = wishlist.find(u);
			/* There are no wishes */
			if (j == wishlist.end() || j->second.empty()) continue;
			const Wish *w = &(j->second.top());
			if (canAffordToBuild(ut, w->ut)) {
				ai->metaCmds->factoryBuild(i->first, w->ut);
				j->second.pop();
			}
		}

		else if (c&COMMANDER) {
			/* If we don't have enough metal income, build a mex */
			if ((mIncome - uMIncome) < 1.0f) {
				UnitType *mex = ai->unitTable->canBuild(ut, MEXTRACTOR);
				if (!ai->metalMap->buildMex(i->first, mex)) {
					UnitType *mmaker = ai->unitTable->canBuild(ut, MMAKER);
					ai->metaCmds->build(i->first, mmaker, pos);
				}
			}
			/* If we don't have enough energy income, build a energy provider */
			else if (eUsage > eIncome || eNow < eStorage/2.0f || eRequest) {
				if (canAffordToBuild(ut, energyProvider)) {
					ai->metaCmds->build(i->first, energyProvider, pos);
					eRequest = false;
				}
			}
			/* If we don't have a factory, build one */
			else if (gameFactories.empty()) {
				if (canAffordToBuild(ut, factory))
					ai->metaCmds->build(i->first, factory, pos);
			}
			/* If we can afford to assist a factory, do so */
			else if (canAffordToBuild(ut, builder)) {
				std::map<int,UnitType*>::iterator fac = gameFactories.begin();
				ai->metaCmds->guard(i->first, fac->first);
			}
		}

		else if (c&BUILDER && c&MOBILE) {
			/* Increase eco income */
			if (mIncome < mUsage || eIncome < eUsage || eRequest || mRequest) {
				int toHelp = 0;
				if (mRequest || mIncome < mUsage) {
					UnitType *mex = ai->unitTable->canBuild(ut, MEXTRACTOR);
					bool canhelp = canHelp(BUILD_MMAKER, i->first, toHelp, mex);
					if (canhelp)
						ai->metaCmds->guard(i->first, toHelp);
					else 
						if (!ai->metalMap->buildMex(i->first, mex)) {
							UnitType *mmaker = ai->unitTable->canBuild(ut, MMAKER);
							ai->metaCmds->build(i->first, mmaker, pos);
						}
					mRequest = false;
				}
				else if (eRequest || eIncome < eUsage) {
					bool canhelp = canHelp(BUILD_EMAKER, i->first, toHelp, energyProvider);
					if (canhelp)
						ai->metaCmds->guard(i->first, toHelp);
					else
						ai->metaCmds->build(i->first, energyProvider, pos);
					eRequest = false;
				}
			}
			/* If we can afford to assist a lab and it's close enough, do so */
			else if (canAffordToBuild(ut, builder)) {
				std::map<int,UnitType*>::iterator fac = gameFactories.begin();
				float3 facpos = ai->call->GetUnitPos(fac->first);
				float dist = ai->call->GetPathLength(pos, facpos, ut->def->movedata->pathType);
				float travelTime = dist / (ut->def->speed/30.0f);
				float buildTime = builder->def->buildTime / (factory->def->buildSpeed/32.0f);
				if (travelTime <= buildTime)
					ai->metaCmds->guard(i->first, fac->first);
			}
		}
	}

	if (!gameFactories.empty() && (mRequest || eRequest))
		if (gameIdle.size() <= 1)
			addWish(factory, builder, NORMAL);

	//XXX: Temporary
	addWish(factory, hammer, NORMAL);
}

void CEconomy::preventStalling() {
	bool stalling = (mNow < 30.0f && mUsage > mIncome) || (eNow < 30.0f && eUsage > eIncome);
	std::map<int,int>::iterator i;
	for (i = gameGuarding.begin(); i != gameGuarding.end(); i++) {
		if (stalling) {
			ai->metaCmds->stop(i->first);
			const UnitDef *guarder = ai->call->GetUnitDef(i->first);
			removeFromGuarding.push_back(i->first);
			gameIdle[i->first] = UT(guarder->id);
			break;
		}
	}
	std::map<int, UnitType*>::iterator j;
	for (j = gameFactories.begin(); j != gameFactories.end(); j++) {
		bool onWait = gameWaiting.find(j->first) != gameWaiting.end();
		if (stalling && !onWait) {
			if (gameGuarding.empty()) {
				ai->metaCmds->wait(j->first);
				gameWaiting[j->first] = 0x0;
			}
		}
		/* Remove previous waiting if we aren't stalling factories */
		else if (!stalling && onWait) {
			ai->metaCmds->wait(j->first);
			removeFromWaiting.push_back(j->first);
		}
	}
}

bool CEconomy::canHelp(task t, int helper, int &unit, UnitType *helpBuild) {
	std::vector<int> units; 
	ai->tasks->getTasks(t, units);
	UnitType *helperUnitType = UT(ai->call->GetUnitDef(helper)->id);
	if (t == BUILD_MMAKER)
		helpBuild = ai->unitTable->canBuild(helperUnitType, MEXTRACTOR);
	else
		helpBuild = energyProvider;

	if (units.empty())
		return false;
	else {
		float buildTime = helpBuild->def->buildTime / (helperUnitType->def->buildSpeed/32.0f);
		for (unsigned int uid = 0; uid < units.size(); uid++) {
			float3 posHelped = ai->call->GetUnitPos(units[uid]);
			float3 posHelper = ai->call->GetUnitPos(helper);
			const UnitDef *uud = ai->call->GetUnitDef(units[uid]);
			float pathLength  = ai->call->GetPathLength(posHelper, posHelped, helperUnitType->def->movedata->pathType);
			float travelTime  = pathLength / (helperUnitType->def->speed/30.0f);
			if (travelTime <= buildTime && getGuardings(units[uid]) < 2) {
				unit = units[uid];
				/* Only if the worker itself isn't guarding */
				if (gameGuarding.find(units[uid]) == gameGuarding.end())
					return true;
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

bool CEconomy::canAffordToBuild(UnitType *builder, UnitType *toBuild) {
	/* NOTE: "Salary" is provided every 32 logical frames */
	float buildTime   = (toBuild->def->buildTime / (builder->def->buildSpeed/32.0f)) / 32.0f;
	float mCost       = toBuild->def->metalCost;
	float eCost       = toBuild->def->energyCost;
	float mPrediction = (mIncome-mUsage)*buildTime - mCost + mNow;
	float ePrediction = (eIncome-eUsage)*buildTime - eCost + eNow;
	if (mPrediction < 0.0f) mRequest = true;
	if (ePrediction < 0.0f) eRequest = true;
	return (mPrediction >= 0.0f &&  ePrediction >= 0.0f);
}

void CEconomy::addWish(UnitType *fac, UnitType *ut, buildPriority p) {
	assert(ut->cats&MOBILE);

	std::map<int, std::priority_queue<Wish> >::iterator k;

	k = wishlist.find(fac->id);

	/* Initialize new priority queue for this factorytype */
	if (k == wishlist.end()) {
		std::priority_queue<Wish> pq;
		wishlist[fac->id] = pq;
	}

	/* If a certain unit is already in the top of our wishlist, don't add it */
	// TODO: calc howmuch factories of this type exist
	if (!wishlist[fac->id].empty()) {
		const Wish *w = &wishlist[fac->id].top();
		if (w->ut->id == ut->id || wishlist[fac->id].size() > 2)
			return;
	}
	wishlist[fac->id].push(Wish(ut, p));
}

void CEconomy::removeIdleUnit(int unit) {
	removeFromIdle.push_back(unit);
}

void CEconomy::removeMyGuards(int unit) {
	std::map<int,int>::iterator i;
	for (i = gameGuarding.begin(); i != gameGuarding.end(); i++) {
		if (i->second == unit)
			ai->metaCmds->stop(i->first);
	}
}

