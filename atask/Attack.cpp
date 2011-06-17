#include "Attack.h"

#include "../CGroup.h"
#include "../CUnitTable.h"
#include "../CDefenseMatrix.h"
#include "../CPathfinder.h"

AttackTask::AttackTask(AIClasses *_ai, int target, CGroup &group): ATask(_ai) {
	const UnitDef *eud = ai->cbc->GetUnitDef(target);
	
	this->t = TASK_ATTACK;
	this->target = target;
	this->pos    = ai->cbc->GetUnitPos(target);
	if (eud)
		this->enemy = eud->humanName;
	targetAlt = -1;

	addGroup(group);
}

bool AttackTask::onValidate() {
	CGroup *group = firstGroup();

	if (targetAlt >= 0) {
		if (ai->cbc->IsUnitCloaked(targetAlt) || !group->canAttack(targetAlt)) {
			group->stop();
		}
	}
	
	const UnitDef *eud = ai->cbc->GetUnitDef(target);
	if (eud == NULL)
		return false;
	
	if (!isMoving) {
		if (ai->cbc->IsUnitCloaked(target))
			return false;
		return true;
	}
	
	if (!group->canAttack(target))
		return false;

	// don't chase scout groups too long if they are out of base...
	bool scoutGroup = (group->cats&SCOUTER).any();
	if (!scoutGroup && lifeTime() > 20.0f) {
		const unitCategory ecats = UC(eud->id);
		if ((ecats&SCOUTER).any() && !ai->defensematrix->isPosInBounds(pos))
			return false; 
	}
	
	float targetDistance = pos.distance2D(group->pos());
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

	if (group->isMicroing() && group->isIdle()) {
		targetAlt = -1; // for sure
		group->micro(false);
	}

	if (isMoving) {
		/* Keep tracking the target */
		pos = ai->cbc->GetUnitPos(target);
	
		float3 gpos = group->pos();
		float dist = gpos.distance2D(pos);
		float range = group->getRange();

		/* See if we can attack our target already */
		if (dist <= range) {
			bool canAttack = true;

			/*
			// for ground units prevent shooting across hill...
			if ((group->cats&AIR).none()) {
				// FIXME: improve
				dist = ai->pathfinder->getPathLength(*group, pos);
				canAttack = (dist <= range * 1.1f);
			}
			*/

			if (canAttack) {
				if ((group->cats&BUILDER).any())
					group->reclaim(target);
				else
					group->attack(target);
				isMoving = false;
				ai->pathfinder->remove(*group);
				group->micro(true);
			}
		}
	}
	
	/* See if we can attack a target we found on our path */
	if (!(group->isMicroing() || urgent())) {
		if ((group->cats&BUILDER).any())
			resourceScan(); // builders should not be too aggressive
		else
			enemyScan(targetAlt);
	}
}

void AttackTask::toStream(std::ostream& out) const {
	out << "AttackTask(" << key << ") target(" << enemy << ") ";
	CGroup *group = firstGroup();
	if (group)
		out << (*group);
}

void AttackTask::onEnemyDestroyed(int enemy, int attacker) {
	if (target == enemy) {
		LOG_II("AttackTask::onEnemyDestroyed")
		remove();
	}
}
