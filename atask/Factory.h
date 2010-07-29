#ifndef E323_TASKFACTORY_H
#define E323_TASKFACTORY_H

#include "../ATask.h"

struct FactoryTask: public ATask {
	FactoryTask(AIClasses *_ai): ATask(_ai) { t = TASK_FACTORY; }
	FactoryTask(AIClasses *_ai, CGroup& group);

	/* overload */
	void onUpdate();
	/* overload */
	bool onValidate();
	/* overload */
	void toStream(std::ostream& out) const;
	/* set the factorytask to wait including assisters */
	void setWait(bool wait);

	bool assistable(CGroup& group);
};

#endif
