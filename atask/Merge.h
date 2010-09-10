#ifndef E323_TASKMERGE_H
#define E323_TASKMERGE_H

#include "../ATask.h"

struct MergeTask: public ATask {
	MergeTask(AIClasses *_ai): ATask(_ai) { t = TASK_MERGE; }
	MergeTask(AIClasses *_ai, std::list<CGroup*>& groups);
	
	bool isRetreating;
		// are groups retreating?
	float range;
		// the minimal range at which groups can merge
	std::map<int, CGroup*> mergable;
		// groups ready to merge <group_id, group>
	CGroup *masterGroup;
		// group which position is used as meeting point
	
	bool reelectMasterGroup();
	/* overload */
	void onUpdate();
	/* overload */
	bool onValidate();
	/* overload */
	void remove(ARegistrar &group);
	/* overload */
	void toStream(std::ostream& out) const;
};

#endif
