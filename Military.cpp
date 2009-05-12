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
}

void CMilitary::update(int frame) {
	/* If we don't have scouts, build one with high priority */
	if (scouts.empty())
		ai->eco->addWish(factory, scout, HIGH);

	/* Harras the enemy in a safe manner */
	else {
	}

	/* If the enemy is building offensives, build the counter unit priority NORMAL*/

	/* Else build scouts priority NORMAL */
}
