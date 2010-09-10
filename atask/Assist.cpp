#include "Assist.h"

#include "../CGroup.h"
#include "../CPathfinder.h"

AssistTask::AssistTask(AIClasses *_ai, ATask& toAssist, CGroup& group): ATask(_ai) {
	t = TASK_ASSIST;
	assist = &toAssist;
	toAssist.assisters.push_back(this);
	assisting = false;
	pos = toAssist.pos;
	validateInterval = 0;
	addGroup(group);
}

void AssistTask::remove() {
	LOG_II("AssistTask::remove " << (*this))
	
	// NOTE: we have to remove manually because assisting tasks are not 
	// completely built upon ARegistrar pattern
	assist->assisters.remove(this);

	ATask::remove();
}

void AssistTask::onUpdate() {
	CGroup *group = firstGroup();

	if (group->isMicroing() && group->isIdle())
		group->micro(false);

	if (!assisting) {
		if (isMoving) {
			pos = assist->pos; // because task target could be mobile

			float3 grouppos = group->pos();
			float3 dist = grouppos - pos;
			float range = group->getRange();

			if (dist.Length2D() <= range) {
				isMoving = false;
				ai->pathfinder->remove(*group);
			} 
		}

		if (!isMoving) {
			group->assist(*assist);
			group->micro(true);
			assisting = true;
		}
	}

	if (!group->isMicroing()) {
		if (group->cats&BUILDER)
			resourceScan(); // builders should not be too aggressive
		else if (!(group->cats&AIR)) {
			enemyScan();
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
