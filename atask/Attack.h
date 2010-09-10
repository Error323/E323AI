#ifndef E323_TASKATTACK_H
#define E323_TASKATTACK_H

#include "../ATask.h"

struct AttackTask: public ATask {
	AttackTask(AIClasses *_ai): ATask(_ai) { t = TASK_ATTACK; }
	AttackTask(AIClasses *_ai, int target, CGroup &group, bool urgent = false);
	
	bool urgent;
		// if task is urgent then enemy scanning is disabled while moving
	int target;
		// the target to attack
	std::string enemy;
		// user name of target
	
	/* Update the attack task */
	void onUpdate();
	/* overload */
	bool onValidate();
	/* overload */
	void toStream(std::ostream& out) const;

	void onEnemyDestroyed(int enemy, int attacker);
};

#endif
