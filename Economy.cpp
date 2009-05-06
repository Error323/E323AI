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
	/* If we are stalling, stop busy workers until we stall no more */
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
			if ((mIncome - uMIncome) < 2.0f) {
				UnitType *mex = ai->unitTable->canBuild(ut, MEXTRACTOR);
				if (canAffordToBuild(ut, mex))
					ai->metalMap->buildMex(i->first, mex);
			}
			/* If we don't have enough energy income, build a energy provider */
			else if (eIncome < 90.0f) {
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
			/* If we can afford to assist a lab, do it */
			if (canAffordToBuild(ut, builder)) {
				std::map<int,UnitType*>::iterator fac = gameFactories.begin();
				ai->metaCmds->guard(i->first, fac->first);
			}
			/* Increase metal income */
			else if (mIncome < mUsage) {
				/* If no builder is increasing metal income, do it */
				UnitType *mex = ai->unitTable->canBuild(ut, MEXTRACTOR);
				ai->metalMap->buildMex(i->first, mex);
				
				/* Else see if we can help him or increase metal ourselves */
			}
			/* Increase energy income */
			else if (eIncome < eUsage || eNow < ai->call->GetEnergyStorage()/2.0f) {
				/* If no builder is increasing energy income, do it */
				ai->metaCmds->build(i->first, energyProvider, pos);
				
				/* Else see if we can help him or increase energy ourselves */
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
	for (unsigned int i = 0; i < removeFromGuarding.size(); i++)
		gameGuarding.erase(removeFromGuarding[i]);
	removeFromGuarding.clear();
}

void CEconomy::updateIncomes(int N) {
	mNow    += ai->call->GetMetal() / N;
	eNow    += ai->call->GetEnergy() / N;
	mIncome += ai->call->GetMetalIncome() / N;
	eIncome += ai->call->GetEnergyIncome() / N;
	mUsage  += ai->call->GetMetalUsage() / N;
	eUsage  += ai->call->GetEnergyUsage() / N;

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

	sprintf(buf, "%s can afford to build %s if %0.2f >= 0 and %0.2f >= 0", builder->def->name.c_str(), toBuild->def->name.c_str(), mPrediction, ePrediction);
	if (mPrediction >= 0.0f &&  ePrediction >= 0.0f) {
		LOGN(buf);
		return true;
	}
	return false;
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

	/* No more than 4 units in our priority queue */
	// TODO: calc howmuch factories of this type exist
	if (wishlist[fac->id].size() <= 4)
		wishlist[fac->id].push(Wish(ut, p));
}

void CEconomy::removeIdleUnit(int unit) {
	removeFromIdle.push_back(unit);
}
