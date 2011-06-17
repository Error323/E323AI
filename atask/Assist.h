#ifndef E323_TASKASSIST_H
#define E323_TASKASSIST_H

#include "../ATask.h"

struct AssistTask: public ATask {
	AssistTask(AIClasses *_ai): ATask(_ai) { t = TASK_ASSIST; }
	AssistTask(AIClasses *_ai, ATask& toAssist, CGroup& group);

	bool assisting;
	/* Task to assist */
	ATask *assist;

	int targetAlt;

	/* overload */
	void onUpdate();
	/* overload */
	bool onValidate();
	/* overload */
	void remove();
	/* overload */
	void toStream(std::ostream& out) const;
};

#endif
