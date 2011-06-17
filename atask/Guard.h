#ifndef E323_TASKGUARD_H
#define E323_TASKGUARD_H

#include "../ATask.h"

struct GuardTask: public ATask {
	GuardTask(AIClasses *_ai): ATask(_ai) { t = TASK_GUARD; }
	GuardTask(AIClasses *_ai, CGroup& group, CGroup& toGuard);

	CGroup *toGuard;

	/* Update the merge task */
	void onUpdate();
	/* overload */
	bool onValidate();
	/* overload */
	void toStream(std::ostream& out) const;
};		

#endif
