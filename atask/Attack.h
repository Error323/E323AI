#ifndef E323_TASKATTACK_H
#define E323_TASKATTACK_H

#include "../ATask.h"

struct AttackTask: public ATask {
	AttackTask(AIClasses *_ai): ATask(_ai) { t = TASK_ATTACK; }
	AttackTask(AIClasses *_ai, int target, CGroup& group);
	
	int target;
		// the target to attack
	int targetAlt;
		// alternative target from enemyScan() subroutine
	std::string enemy;
		// user name of target
	
	/* Update the attack task */
	void onUpdate();
	/* overload */
	bool onValidate();
	/* overload */
	void toStream(std::ostream& out) const;
	/* overload */
	void onEnemyDestroyed(int enemy, int attacker);
};

#endif
