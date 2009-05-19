#include "Military.h"

CMilitary::CMilitary(AIClasses *ai) {
	this->ai = ai;
	currentGroup = 0;
}

void CMilitary::init(int unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	UnitType *commander = UT(ud->id);

	factory = ai->unitTable->canBuild(commander, KBOT|TECH1);

	scout = ai->unitTable->canBuild(factory, ATTACKER|MOBILE|SCOUT);
	artie = ai->unitTable->canBuild(factory, ATTACKER|MOBILE|ARTILLERY);
	antiair = ai->unitTable->canBuild(factory, ATTACKER|MOBILE|ANTIAIR);
	createNewGroup();
}

void CMilitary::update(int frame) {
	if (scouts.size() < ai->intel->metalMakers.size())
		ai->eco->addWish(factory, scout, NORMAL);

	/* If we have a scout, harras the enemy */
	if (!scouts.empty()) {
		std::map<int, bool>::iterator s;
		int scout = -1;
		for (s = scouts.begin(); s != scouts.end(); s++)
			if (!s->second)
				scout = s->first;

		if (scout != -1) {
			for (unsigned i = 0; i < ai->intel->metalMakers.size(); i++) {
				int target      = ai->intel->metalMakers[i];
				float3 metalPos = ai->cheat->GetUnitPos(target);
				if (ai->threatMap->getThreat(metalPos, 10.0f) < 50.0f) {
					ai->tasks->addMilitaryPlan(HARRAS, scout, target);
					float3 start = ai->call->GetUnitPos(scout);
					ai->pf->addPath(scout, start, metalPos);
					scouts[scout] = true;
				}
			}
		}
	} else ai->eco->addWish(factory, scout, HIGH);
	
	/* If enemy offensive is within our perimeter, attack it */
	if (ai->intel->enemyInbound()) {
		/* Regroup our offensives such that we have enough power to engage */

		/* If we cannot get the power, put factories on HIGH priority for offensives */
	}

	/* Else pick a target, decide group power required and attack */
	else {
		/* Pick a target */
		int target;
		if (!ai->intel->factories.empty() || !ai->intel->energyMakers.empty()) {
			target = (ai->intel->factories.empty()) ? ai->intel->energyMakers[0] : ai->intel->factories[0];
			float3 goal = ai->cheat->GetUnitPos(target);
			float enemyStrength = ai->threatMap->getThreat(goal, 10.0f);

			/* If we can confront the enemy, do so */
			if (currentGroupStrength >= enemyStrength*1.2f && groups[currentGroup].size() >= 30) {
			//if (groups[currentGroup].size() >= 4) {
				/* Add the taskplan */
				ai->tasks->addMilitaryPlan(ATTACK, currentGroup, target);

				/* Bootstrap the path */
				float3 start = getGroupPos(currentGroup);
				ai->pf->addPath(currentGroup, start, goal); 

				/* Create new group */
				createNewGroup();
			}
		}
	}

	/* Always build some unit */
	ai->eco->addWish(factory, randomUnit(), NORMAL);
}

float3 CMilitary::getGroupPos(int group) {
	std::map<int, bool>::iterator i = groups[group].begin();
	return ai->call->GetUnitPos(i->first);
}

void CMilitary::createNewGroup() {
	currentGroup+=10;
	std::map<int, bool> G;
	groups[currentGroup] = G;
	currentGroupStrength = 0.0f;
	sprintf(buf, "[CMilitary::createNewGroup]\t New group(%d)", currentGroup);
	LOGN(buf);
}

void CMilitary::addToGroup(int unit) {
	groups[currentGroup][unit] = true;
	units[unit] = currentGroup;
	//currentGroupStrength += ai->call->GetUnitPower(unit) * ai->call->GetUnitMaxRange(unit);
	currentGroupStrength += ai->call->GetUnitPower(unit);
	sprintf(buf, "[CMilitary::addToGroup]\t Group(%d) adding %s new size %d", currentGroup, ai->call->GetUnitDef(unit)->name.c_str(), groups[currentGroup].size());
	LOGN(buf);
}

void CMilitary::removeFromGroup(int unit) {
	int group = units[unit];
	groups[group].erase(unit);
	units.erase(unit);
	if (groups[group].empty() && group != currentGroup) {
		groups.erase(group);
		ai->call->EraseGroup(group);
		ai->tasks->militaryplans.erase(group);
		ai->pf->removePath(group);
	}
}

UnitType* CMilitary::randomUnit() {
	int r = rng.RandInt(2);
	switch(r) {
		case 0: return antiair;
		case 1: return artie;
	}
	return NULL;
}
