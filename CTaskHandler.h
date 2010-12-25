#ifndef E323_TASKHANDLER_H
#define E323_TASKHANDLER_H

#include <vector>
#include <map>
#include <stack>

#include "atask/Factory.h"
#include "atask/Build.h"
#include "atask/Attack.h"
#include "atask/Assist.h"
#include "atask/Merge.h"
#include "atask/Guard.h"
#include "atask/Repair.h"

struct UnitType;
class AIClasses;
class CGroup;
class CUnit;

class CTaskHandler: public ARegistrar {

public:
	CTaskHandler(AIClasses *ai);
	~CTaskHandler();

	/* The active tasks per type */
	std::map<TaskType, std::map<int, ATask*> > activeTasks;

	/* Overload */
	void remove(ARegistrar &task);

	bool addTask(ATask* task, ATask::NPriority p = ATask::NORMAL);

	/* Get active task of group */
	ATask* getTask(const CGroup& group);

	ATask* getTaskByTarget(int);
	/* Get the group destination */
	float3 getPos(const CGroup& group);
	/* Update call */
	void update();
	/* Propagate "EnemyDestroyed" event to tasks */
	void onEnemyDestroyed(int enemy, int attacker);
	/* Propagate "UnitDestroyed" event to tasks */
	void onUnitDestroyed(int uid, int attacker);

private:
	AIClasses *ai;
	/* The -to be removed- tasks */
	std::stack<ATask*> obsoleteTasks;
	/* The group to task table */
	std::map<int, ATask*> groupToTask;
	/* Task queues <queue_id, tasks> */
	std::map<int, std::list<ATask*> > taskQueues;
	/* Plain queue containing every task to be processed */
	std::list<ATask*> processQueue;

	int statsMaxActiveTasks;
	int statsMaxTasks;
};

#endif
