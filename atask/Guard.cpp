#include "Guard.h"

#include "../CGroup.h"

GuardTask::GuardTask(AIClasses *_ai, CGroup& group, CGroup& toGuard): ATask(_ai) {
	t = TASK_GUARD;
	pos = toGuard.pos(true);
	this->toGuard = &toGuard;

	addGroup(group);
}

void GuardTask::onUpdate() {
	// TODO:
}

bool GuardTask::onValidate() {
	// TODO:
	return true;
}

void GuardTask::toStream(std::ostream& out) const {
	// TODO:
}
