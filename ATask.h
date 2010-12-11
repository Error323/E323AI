#ifndef E323_ATASK_H
#define E323_ATASK_H

#include <list>
#include <iostream>

#include "CAI.h"
#include "ARegistrar.h"

enum TaskType {
	TASK_UNDEFINED,
	TASK_BUILD,
	TASK_ASSIST,
	TASK_ATTACK,
	TASK_MERGE,
	TASK_FACTORY,
	TASK_REPAIR,
	TASK_GUARD
};

class CGroup;

class ATask: public ARegistrar {

public:
	enum NPriority { LOW = 0, NORMAL, HIGH };

	ATask(AIClasses *_ai);
	~ATask() {}

	bool active;
		// task is active
	bool suspended;
		// task is suspended
	NPriority priority;
		// task priority
	int queueID;
		// queue ID this task belongs to
	int initFrame;
		// frame when task was initialized
	int validateInterval;
		// validate interval in frames; 0 means validation is OFF
	int nextValidateFrame;
		// next frame to execute task validation
	TaskType t;
		// type of the task: BUILD, ASSIST, ATTACK, etc.
	std::list<ATask*> assisters;
		// the assisters assisting this task
	std::list<CGroup*> groups;
		// groups involved
	//CGroup *group;
		// the group involved; for Merge task it is master-group
	bool isMoving;
		// determine if all groups in this task are moving or not
	float3 pos;
		// the position to navigate too
		// TODO: make it as method because for assisting task this position
		// may vary depending on master task 

	CGroup* firstGroup() const;
	/* Remove this task, unreg groups involved, and make them available again */
	virtual void remove();
	/* Overload */
	void remove(ARegistrar &group);
	/* Add a group to this task */
	void addGroup(CGroup &group);

	void removeGroup(CGroup &group);
	/* Scan and micro for resources */
	bool resourceScan();
	/* Scan and micro for damaged units */
	bool repairScan();
	/* Scan and micro for enemy targets */
	bool enemyScan(int& target);
	/* Task lifetime in frames */
	int lifeFrames() const;
	/* Task lifetime in sec */
	float lifeTime() const;
	/* Update this task */
	void update();

	bool urgent() { return priority == HIGH; }

	virtual void onUpdate() = 0;
	
	virtual bool onValidate() { return true; }

	virtual void toStream(std::ostream& out) const = 0;

	virtual void onEnemyDestroyed(int enemy, int attacker) {};

	virtual void onUnitDestroyed(int uid, int attacker) {};

	ARegistrar::NType regtype() const { return ARegistrar::TASK; }
	
	friend std::ostream& operator<<(std::ostream& out, const ATask& task);

protected:
	AIClasses *ai;

private:
	static int counter;
		// task counter, used as task key; shared among all AI instances
};

#endif
