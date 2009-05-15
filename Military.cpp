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
	/* If we have a scout, harras the enemy */
	if (!scouts.empty()) {
	}
	
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
			float enemyStrength = ai->threatMap->getThreat(goal, 100.0f);

			/* If we can confront the enemy, do so */
			if (groups[currentGroup].size() >= 2) {
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

	/* Always have enough scouts lying around */
	//if (scouts.size() < ai->intel->mobileBuilders.size())
	//	ai->eco->addWish(factory, scout, HIGH);
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
	currentGroupStrength += ai->call->GetUnitPower(unit) * ai->call->GetUnitMaxRange(unit);
	sprintf(buf, "[CMilitary::addToGroup]\t Group(%d) adding %s new size %d", currentGroup, ai->call->GetUnitDef(unit)->name.c_str(), groups[currentGroup].size());
	LOGN(buf);
}

void CMilitary::removeFromGroup(int unit) {
	int group = currentGroup;
	groups[group].erase(unit);
	if (groups[group].empty() && group != currentGroup) {
		groups.erase(group);
		ai->call->EraseGroup(group);
		ai->tasks->militaryplans.erase(group);
		ai->pf->removePath(group);
	}
}

UnitType* CMilitary::randomUnit() {
	int r = rng.RandInt(3);
	switch(r) {
		case 0: return scout;
		case 1: return artie;
		case 2: return antiair;
	}
	return NULL;
}
