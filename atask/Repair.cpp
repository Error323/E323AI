#include "Repair.h"

#include "../CGroup.h"
#include "../CPathfinder.h"

RepairTask::RepairTask(AIClasses* _ai, int target, CGroup& group): ATask(_ai) {
	this->t = TASK_REPAIR;
	this->target = target;
	pos = ai->cb->GetUnitPos(target);
	repairing = false;
		
	addGroup(group);
}

void RepairTask::onUpdate() {
	CGroup* group = firstGroup();

	if (group->isMicroing() && group->isIdle()) {
		group->micro(false);
	}

	if (!repairing) {
		if (isMoving) {
			/* Keep tracking the target */
			pos = ai->cb->GetUnitPos(target);

			float3 gpos = group->pos();
			float dist = gpos.distance2D(pos);
			float range = group->getRange();
			
			if (dist <= range) {
				bool canRepair = true;
				/*
				// for ground units prevent assisting across hill...
				if ((group->cats&AIR).none()) {
					dist = ai->pathfinder->getPathLength(*group, pos);
					canRepair = (dist <= range * 1.1f);
				}
				*/
				if (canRepair) {
					isMoving = false;
					ai->pathfinder->remove(*group);
				}
			} 
		}

		if (!isMoving) {
			group->repair(target);
			group->micro(true);
			repairing = true;
		}
	}

	if (!group->isMicroing() && !urgent()) {
		resourceScan();
	}
}

bool RepairTask::onValidate() {
	float health = ai->cb->GetUnitHealth(target);
	if (health < EPS)
		return false;
	if (health < ai->cb->GetUnitMaxHealth(target))
		return true;
	return false;
}

void RepairTask::onUnitDestroyed(int uid, int attacker) {
	if (uid == target)
		remove();
}

void RepairTask::toStream(std::ostream& out) const {
	out << "RepairTask(" << key << ") target(" << target << ") ";
	CGroup* group = firstGroup();
	if (group)
		out << (*group);
}
