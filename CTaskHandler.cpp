#include "CTaskHandler.h"

#include <iostream>
#include <sstream>
#include <string>
#include <math.h>
#include <limits>

#include "CUnitTable.h"
#include "CWishList.h"
#include "CPathfinder.h"
#include "CUnit.h"
#include "CGroup.h"
#include "CEconomy.h"
#include "CConfigParser.h"
#include "CThreatMap.h"
#include "CScopedTimer.h"
#include "CDefenseMatrix.h"


CTaskHandler::CTaskHandler(AIClasses *ai): ARegistrar(500) {
	this->ai = ai;

	statsMaxActiveTasks = 0;
	statsMaxTasks = 0;
}

CTaskHandler::~CTaskHandler() {
	//LOG_II("CTaskHandler::Stats MaxActiveTasks = " << statsMaxActiveTasks)
	LOG_II("CTaskHandler::Stats MaxTasks = " << statsMaxTasks)

	std::list<ATask*>::iterator it;
	for (it = processQueue.begin(); it != processQueue.end(); ++it)
		delete *it;
}

void CTaskHandler::remove(ARegistrar &obj) {
	switch(obj.regtype()) {
		case ARegistrar::TASK: {
			ATask *task = dynamic_cast<ATask*>(&obj);
			LOG_II("CTaskHandler::remove " << (*task))
			for(std::list<CGroup*>::iterator it = task->groups.begin(); it != task->groups.end(); ++it) {
				CGroup *group = *it;
				group->unreg(*this);
				groupToTask.erase(group->key);
				if (task->isMoving)
					ai->pathfinder->remove(*group);
			}
			activeTasks[task->t].erase(task->key);
			obsoleteTasks.push(task);
			break;
		}
		case ARegistrar::GROUP: {
			CGroup *group = dynamic_cast<CGroup*>(&obj);
			LOG_II("CTaskHandler::remove " << (*group))
			groupToTask.erase(group->key);
			break;
		}
		default:
			assert(false);
	}
	
	obj.unreg(*this);
}

void CTaskHandler::update() {
	/* delete obsolete tasks from memory */
	while(!obsoleteTasks.empty()) {
		ATask *task = obsoleteTasks.top();
		processQueue.remove(task);
		obsoleteTasks.pop();
		// make sure task is really detached from groups
		assert(task->groups.size() == 0);
		delete task;
	}

	/* Begin task updates */
	if (!processQueue.empty()) {
		ATask *task;

		if (processQueue.size() == 1) {
			task = processQueue.front();
			if (task->active) task->update();
		}
		else {
			int c = 0; // active tasks counter
			std::list<ATask*>::iterator it = processQueue.begin();
			ATask *taskFirst = *it;
			
			do {
				task = *it;

				if (task->active) {
					task->update();
					c++;
				}
				
				++it;

				// imitate circle queue...
				processQueue.pop_front();
				processQueue.push_back(task);

				assert(it != processQueue.end());
			} while (c < MAX_TASKS_PER_UPDATE && (*it)->key != taskFirst->key);
		}

		statsMaxTasks = std::max<int>(statsMaxTasks, processQueue.size());
	}
}

float3 CTaskHandler::getPos(CGroup &group) {
	assert(groupToTask.find(group.key) != groupToTask.end());
	return groupToTask[group.key]->pos;
}

ATask* CTaskHandler::getTask(CGroup &group) {
	std::map<int, ATask*>::iterator i = groupToTask.find(group.key);
	if (i == groupToTask.end())
		return NULL;
	return i->second;
}

ATask* CTaskHandler::getTaskByTarget(int uid) {
	std::map<int, ATask*>::iterator i;
	for (i = activeTasks[TASK_ATTACK].begin(); i != activeTasks[TASK_ATTACK].end(); ++i) {
		if (((AttackTask*)i->second)->target == uid)
			return i->second;
	}
	return NULL;
}

bool CTaskHandler::addTask(ATask *task, ATask::NPriority p) {
	if (task == NULL)
		return false;
	
	assert(task->t != TASK_UNDEFINED);

	task->priority = p;

	std::list<CGroup*>::iterator it;

	// NOTE: this is required because otherwise memory used by task will never 
	// be freed
	task->reg(*this);
	processQueue.push_back(task);
	// NOTE: this is required because within ATask::onValidate() a path 
	// (or even paths) can be added, which in its turn calls 
	// CTaskHandler::getPos()
	for (it = task->groups.begin(); it != task->groups.end(); ++it) {
		(*it)->reg(*this);
		groupToTask[(*it)->key] = task;
	}
	
	LOG_II((*task))

	if (!task->onValidate()) {
		task->remove();
		return false;
	}
	
	// TODO: after MoveTask is implemented remove the code below
	for(it = task->groups.begin(); it != task->groups.end(); ++it) {
		CGroup *group = *it;
		if (task->isMoving && !ai->pathfinder->pathAssigned(*group)) {
			if(!ai->pathfinder->addGroup(*group)) {
				task->remove();
				return false;
			}
		}
	}	
		
	activeTasks[task->t][task->key] = task;

	task->active = true;

	return true;
}

void CTaskHandler::onEnemyDestroyed(int enemy, int attacker) {
	std::list<ATask*>::iterator it = processQueue.begin();
	ATask *task;
	while (it != processQueue.end()) {
		task = *it;	++it;
		if (task->active)
			task->onEnemyDestroyed(enemy, attacker);
	}
}

void CTaskHandler::onUnitDestroyed(int uid, int attacker) {
	std::list<ATask*>::iterator it = processQueue.begin();
	ATask *task;
	while (it != processQueue.end()) {
		task = *it;	++it;
		if (task->active)
			task->onUnitDestroyed(uid, attacker);
	}
}
