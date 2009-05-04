#include "Economy.h"

CEconomy::CEconomy(AIClasses *ai) {
	this->ai = ai;
}

void CEconomy::init(int unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	UnitType *ut = UT(ud->id);
	ai->eco->gameIdle[unit] = ut;
	updateIncomes(1);
}

void CEconomy::update(int frame) {
	// Als we stallen zet dan een worker op stop

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
			ai->metaCmds->factoryBuild(i->first, w->ut->def);
			j->second.pop();
		}

		else if (c&COMMANDER) {
			if ((mIncome - uMIncome) < 8.0f) {
				UnitType *mex = ai->unitTable->canBuild(ut, MEXTRACTOR);
				float3 goal   = ai->metalMap->buildMex(i->first, mex);
				float path    = ai->call->GetPathLength(pos, goal, ut->def->movedata->pathType);
				path -= std::min<float>(ut->def->buildDistance, path);
				float travelTime = path / (ut->def->speed/30.0f);
				float buildTime  = mex->def->buildTime / (ut->def->buildSpeed/32.0f);
				int eta = travelTime + buildTime;
				sprintf(buf,"turnRate(%0.2f), path(%0.2f), speed(%0.2f), buildTime(%0.2f), buildSpeed(%0.2f)", ut->def->turnRate, path, ut->def->speed, mex->def->buildTime, ut->def->buildSpeed);
				LOGN(buf);
				ai->tasks->addTaskPlan(i->first, BUILD_MMAKER, eta);
			}
			// If mNow - uMIncome < 2: increase m income
			// If eNow - uEIncome < 40: increase energy income
			// Als we nog geen factory hebben en we kunnen hem betalen, bouw deze
			// Anders zorgen we ervoor dat we deze kunnen betalen
			
			// als we genoeg eco hebben om een lab te assisten doe dat dan
			// Anders breid eco uit
		}

		else if (c&BUILDER && c&MOBILE) {
			// Als ons inkomen > uitgaven zdd onze buildpower erbij kan, assist een lab 
			// Anders:
			//  - als er geen builder bezig is met het uitbreidden van de eco, breid eco uit
			//  - als er wel een builder bezig is, kijk of je kan paralellizeren of helpen
		}
	}
	
	// Als ons inkomen groter is dan uitgaven en er is geen builder bezig met
    // uitbreiden bouw worker priority: HIGH

	/* Reset incomes */
	mNow = eNow = mIncome = eIncome = uMIncome = uEIncome = 0.0f;

	/* Remove from gameIdle */
	for (unsigned int i = 0; i < removeFromIdle.size(); i++)
		gameIdle.erase(removeFromIdle[i]);
	removeFromIdle.clear();
}

void CEconomy::updateIncomes(int N) {
	mNow    += ai->call->GetMetal() / N;
	eNow    += ai->call->GetEnergy() / N;
	mIncome += ai->call->GetMetalIncome() / N;
	eIncome += ai->call->GetEnergyIncome() / N;

	std::map<int, UnitType*>::iterator i;
	float mU = 0.0f, eU = 0.0f;
	for (i = ai->unitTable->gameAllUnits.begin(); i != ai->unitTable->gameAllUnits.end(); i++) {
		unsigned int c = i->second->cats;
		const UnitDef *ud = i->second->def;
		if (!(c&MMAKER) || !(c&EMAKER) || !(c&MEXTRACTOR)) {
			mU += ud->metalMake;
			eU += ud->energyMake;
		}
	}

	uMIncome += mU / N;
	uEIncome += mU / N;
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

	wishlist[fac->id].push(Wish(ut, p));
}

void CEconomy::removeIdleUnit(int unit) {
	removeFromIdle.push_back(unit);
}
