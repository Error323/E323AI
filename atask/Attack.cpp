#include "Attack.h"

#include "../CGroup.h"
#include "../CUnitTable.h"
#include "../CDefenseMatrix.h"
#include "../CPathfinder.h"

AttackTask::AttackTask(AIClasses *_ai, int target, CGroup &group, bool urgent): ATask(_ai) {
	const UnitDef *eud = ai->cbc->GetUnitDef(target);
	
	this->t      = TASK_ATTACK;
	this->target = target;
	this->pos    = ai->cbc->GetUnitPos(target);
	this->urgent = urgent;
	if (eud)
		this->enemy = eud->humanName;
	addGroup(group);
}

bool AttackTask::onValidate() {
	const UnitDef *eud = ai->cbc->GetUnitDef(target);
	if (eud == NULL)
		return false;
	
	if (!isMoving) {
		if (ai->cbc->IsUnitCloaked(target))
			return false;
		return true;
	}
	
	CGroup *group = firstGroup();
	bool scoutGroup = group->cats&SCOUTER;
	float targetDistance = pos.distance2D(group->pos());
	
	// don't chase scout groups too long if they are out of base...
	if (!scoutGroup && lifeTime() > 20.0f) {
		const unsigned int ecats = UC(eud->id);
		if (ecats&SCOUTER && !ai->defensematrix->isPosInBounds(pos))
			return false; 
	}
	
	if (targetDistance > 1000.0f)
		return true; // too far to panic

	if (ai->cbc->IsUnitCloaked(target))
		return false;
	
	bool isBaseThreat = ai->defensematrix->isPosInBounds(pos);

	// if there is no threat to our base then prevent useless attack for groups
	// which are not too cheap
	if (!isBaseThreat && (group->costMetal / group->units.size()) >= 100.0f) {
		float threatRange;
		if (scoutGroup)
			threatRange = 300.0f;
		else
			threatRange = 0.0f;

		if (group->getThreat(pos, threatRange) > group->strength)
			return false;
	}

	return true;
}

void AttackTask::onUpdate() {
	CGroup *group = firstGroup();

	if (group->isMicroing() && group->isIdle())
		group->micro(false);

	/* If the target is destroyed, remove the task, unreg groups */
	/*
	if (ai->cbc->GetUnitHealth(target) <= 0.0f) {
		remove();
		return;
	}
	*/

	if (isMoving) {
		/* Keep tracking the target */
		pos = ai->cbc->GetUnitPos(target);
	
		float range = group->getRange();
		float3 gpos = group->pos();

		/* See if we can attack our target already */
		if (gpos.distance2D(pos) <= range) {
			if (group->cats&BUILDER)
				group->reclaim(target);
			else
				group->attack(target);
			isMoving = false;
			ai->pathfinder->remove(*group);
			group->micro(true);
		}
	}
	
	/* See if we can attack a target we found on our path */
	if (!(group->isMicroing() || urgent)) {
		if (group->cats&BUILDER)
			resourceScan(); // builders should not be too aggressive
		else
			enemyScan();
	}
}

void AttackTask::toStream(std::ostream& out) const {
	out << "AttackTask(" << key << ") target(" << enemy << ") ";
	CGroup *group = firstGroup();
	if (group)
		out << (*group);
}

void AttackTask::onEnemyDestroyed(int enemy, int attacker) {
	if (target == enemy)
		remove();
}
