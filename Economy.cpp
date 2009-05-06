#include "Economy.h"

CEconomy::CEconomy(AIClasses *ai) {
	this->ai = ai;
}

void CEconomy::init(int unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	UnitType *ut = UT(ud->id);
	ai->eco->gameIdle[unit] = ut;
	updateIncomes(1);

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
	/* If we are stalling, stop guarding workers until we stall no more */
	std::map<int,int>::iterator g;
	for (g = gameGuarding.begin(); g != gameGuarding.end(); g++) {
		if ((mNow < 10.0f && mUsage > mIncome) || (eNow < 10.0f && eUsage > eIncome)) {
			ai->metaCmds->stop(g->first);
			const UnitDef *guarder = ai->call->GetUnitDef(g->first);
			removeFromGuarding.push_back(g->first);
		}
	}

	/* Update idle units */
	std::map<int, UnitType*>::iterator i;
	for (i = ai->eco->gameIdle.begin(); i != ai->eco->gameIdle.end(); i++) {
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
			ai->metaCmds->factoryBuild(i->first, w->ut);
			j->second.pop();
		}

		else if (c&COMMANDER) {
			/* If we don't have enough metal income, build a mex */
			if ((mIncome - uMIncome) < 1.0f) {
				UnitType *mex = ai->unitTable->canBuild(ut, MEXTRACTOR);
				if (canAffordToBuild(ut, mex))
					ai->metalMap->buildMex(i->first, mex);
			}
			/* If we don't have enough energy income, build a energy provider */
			else if (eUsage > eIncome || eNow < eStorage/2.0f) {
				if (canAffordToBuild(ut, energyProvider))
					ai->metaCmds->build(i->first, energyProvider, pos);
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
			/* If we can afford to assist a lab, do so */
			if (canAffordToBuild(ut, builder)) {
				std::map<int,UnitType*>::iterator fac = gameFactories.begin();
				ai->metaCmds->guard(i->first, fac->first);
			}
			/* Increase eco income */
			else if (mIncome < mUsage || eIncome < eUsage) {
				int toHelp = 0;
				if (mNow/mStorage < eNow/eStorage) {
					UnitType *mex = ai->unitTable->canBuild(ut, MEXTRACTOR);
					bool canhelp = canHelp(BUILD_MMAKER, i->first, toHelp, mex);
					if (canhelp)
						ai->metaCmds->guard(i->first, toHelp);
					else 
						ai->metalMap->buildMex(i->first, mex);
				}
				else {
					bool canhelp = canHelp(BUILD_EMAKER, i->first, toHelp, energyProvider);
					if (canhelp)
						ai->metaCmds->guard(i->first, toHelp);
					else
						ai->metaCmds->build(i->first, energyProvider, pos);
				}
			}
		}
	}
	
	/* If we can afford, create a new builder */
	if (!gameFactories.empty() && canAffordToBuild(factory, builder));
		addWish(factory, builder, NORMAL);

	/* Reset incomes */
	mNow = eNow = mIncome = eIncome = uMIncome = uEIncome = mUsage = eUsage = 0.0f;

	/* Remove */
	for (unsigned int i = 0; i < removeFromIdle.size(); i++)
		gameIdle.erase(removeFromIdle[i]);
	removeFromIdle.clear();
	/* Remove guarders */
	for (unsigned int i = 0; i < removeFromGuarding.size(); i++)
		gameGuarding.erase(removeFromGuarding[i]);
	removeFromGuarding.clear();
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

void CEconomy::updateIncomes(int N) {
	mNow    += ai->call->GetMetal() / N;
	eNow    += ai->call->GetEnergy() / N;
	mIncome += ai->call->GetMetalIncome() / N;
	eIncome += ai->call->GetEnergyIncome() / N;
	mUsage  += ai->call->GetMetalUsage() / N;
	eUsage  += ai->call->GetEnergyUsage() / N;
	mStorage = ai->call->GetMetalStorage();
	eStorage = ai->call->GetEnergyStorage();

	// TODO: subtract predicted usages using taskplanner

	std::map<int, UnitType*>::iterator i;
	float mU = 0.0f, eU = 0.0f;
	for (i = ai->unitTable->gameAllUnits.begin(); i != ai->unitTable->gameAllUnits.end(); i++) {
		unsigned int c = i->second->cats;
		if (!(c&MMAKER) || !(c&EMAKER) || !(c&MEXTRACTOR)) {
			mU += i->second->metalMake;
			eU += i->second->energyMake;
		}
	}

	uMIncome += mU / N;
	uEIncome += eU / N;
}

bool CEconomy::canAffordToBuild(UnitType *builder, UnitType *toBuild) {
	/* NOTE: "Salary" is provided every 32 logical frames */
	float buildTime   = (toBuild->def->buildTime / (builder->def->buildSpeed/32.0f)) / 32.0f;
	float mCost       = toBuild->def->metalCost;
	float eCost       = toBuild->def->energyCost;
	float mPrediction = (mIncome-mUsage)*buildTime - mCost + mNow;
	float ePrediction = (eIncome-eUsage)*buildTime - eCost + eNow;

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

	/* No more than 2 units in our priority queue */
	// TODO: calc howmuch factories of this type exist
	if (wishlist[fac->id].size() <= 2)
		wishlist[fac->id].push(Wish(ut, p));
}

void CEconomy::removeIdleUnit(int unit) {
	removeFromIdle.push_back(unit);
}
