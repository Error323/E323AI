#include "Military.h"

CMilitary::CMilitary(AIClasses *ai) {
	this->ai = ai;
}

void CMilitary::init(int unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	UnitType *commander = UT(ud->id);

	factory = ai->unitTable->canBuild(commander, KBOT|TECH1);

	scout = ai->unitTable->canBuild(factory, ATTACKER|MOBILE|SCOUT);
	artie = ai->unitTable->canBuild(factory, ATTACKER|MOBILE|ARTILLERY);
	antiair = ai->unitTable->canBuild(factory, ATTACKER|MOBILE|ANTIAIR);
	currentGroup = createNewGroup();
}

void CMilitary::update(int frame) {
	/* Always have some scouts lying around */
	if (scouts.size() < 2) ai->eco->addWish(factory, scout, HIGH);

}

int CMilitary::createNewGroup() {
	int group = ai->call->CreateGroup();
	std::map<int, bool> G;
	groups[group] = G;
	currentGroupStrength = 0.0f;
	return group;
}

void CMilitary::addToGroup(int unit) {
	groups[currentGroup][unit] = true;
	currentGroupStrength += ai->call->GetUnitPower(unit) * ai->call->GetUnitMaxRange(unit);
}

void CMilitary::removeFromGroup(int unit) {
	int group = ai->call->GetUnitGroup(unit);
	groups[group].erase(unit);
	if (groups[group].empty() && group != currentGroup) {
		groups.erase(group);
		ai->call->EraseGroup(group);
	}
}
