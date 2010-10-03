#ifndef E323_TASKREPAIR_H
#define E323_TASKREPAIR_H

#include "../ATask.h"

class UnitType;

struct RepairTask: public ATask {
	RepairTask(AIClasses* _ai): ATask(_ai) { t = TASK_REPAIR; }
	RepairTask(AIClasses* _ai, int target, CGroup& group);

	/* build type to string */
	static std::map<buildType, std::string> buildStr;
	
	bool repairing;

	int target;

	/* overloaded */
	void onUpdate();
	/* overloaded */
	bool onValidate();
	/* overloaded */
	void toStream(std::ostream& out) const;
	/* overloaded */
	void onUnitDestroyed(int uid, int attacker);
};

#endif
