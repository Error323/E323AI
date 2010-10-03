#include "Assist.h"

#include "../CGroup.h"
#include "../CPathfinder.h"

AssistTask::AssistTask(AIClasses *_ai, ATask& toAssist, CGroup& group): ATask(_ai) {
	t = TASK_ASSIST;
	assist = &toAssist;
	toAssist.assisters.push_back(this);
	assisting = false;
	pos = toAssist.pos;
	targetAlt = -1;
	
	addGroup(group);
}

void AssistTask::remove() {
	LOG_II("AssistTask::remove " << (*this))
	
	// NOTE: we have to remove manually because assisting tasks are not 
	// completely built upon ARegistrar pattern
	assist->assisters.remove(this);

	ATask::remove();
}

bool AssistTask::onValidate() {
	if (targetAlt >= 0) {
		if (ai->cbc->IsUnitCloaked(targetAlt)) {
			firstGroup()->stop();
		}
	}

	return true;
}

void AssistTask::onUpdate() {
	CGroup *group = firstGroup();

	if (group->isMicroing() && group->isIdle()) {
		targetAlt = -1; // for sure
		group->micro(false);
	}

	if (!assisting) {
		if (isMoving) {
			/* Keep tracking the target */
			pos = assist->pos;

			float3 gpos = group->pos();
			float dist = gpos.distance2D(pos);
			float range = group->getRange();
			
			if (dist <= range) {
				bool canAssist = true;
				/*
				// for ground units prevent assisting across hill...
				if ((group->cats&AIR).none()) {
					dist = ai->pathfinder->getPathLength(*group, pos);
					canAssist = (dist <= range * 1.1f);
				}
				*/
				if (canAssist) {
					isMoving = false;
					ai->pathfinder->remove(*group);
				}
			} 
		}

		if (!isMoving) {
			group->assist(*assist);
			group->micro(true);
			assisting = true;
		}
	}

	if (!group->isMicroing()) {
		if ((group->cats&BUILDER).any())
			resourceScan(); // builders should not be too aggressive
		else if ((group->cats&AIR).none()) {
			enemyScan(targetAlt);
		}
	}
}

void AssistTask::toStream(std::ostream& out) const {
	out << "AssistTask(" << key << ") groups(" << groups.size() << ") { ";
	std::list<CGroup*>::const_iterator i;
	for (i = groups.begin(); i != groups.end(); ++i) {
		out << (*(*i)) << " ";
	}
	out << "} assisting " << (*assist);
}
