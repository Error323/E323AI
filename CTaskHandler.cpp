#include "TaskHandler.h"

int ATask::counter = 0;

CTaskHandler::CTaskHandler(AIClasses *ai) {
	this->ai = ai;
	update   = 0;

	taskStr[ASSIST] = std::string("ASSIST");
	taskStr[BUILD]  = std::string("BUILD");
	taskStr[ATTACK] = std::string("ATTACK");
	taskStr[MERGE]  = std::string("MERGE");

	std::map<task, std::string>::iterator i;
	for (i = taskStr.begin(); i != taskStr.end(); i++) {
		std::vector<ATask> V;
		taskContainer[i->first] = V;

		std::map<int, int> M;
		lookup[i->first] = M;

		std::stack<int> S;
		free[i->first] = S;
	}
}

void CTaskHandler::remove(ARegistrar &task) {
	ATask *t = &(dynamic_cast<ATask>(task));
	free[t->task].push_back(t->key);
	lookup.erase(t->key);
	activeTasks.erase(t->key);
	switch(t->task) {
		case BUILD:  activeBuildTasks.erase(t->key);  break;
		case ASSIST: activeAssistTasks.erase(t->key); break;
		case ATTACK: activeAttackTasks.erase(t->key); break;
		case MERGE:  activeMergeTasks.erase(t->key);  break;

		default: return;
	}
}

void CTaskHandler::addBuildTask(float3 &pos, UnitType *toBuild, std::vector<CGroup*> &groups) {
	BuildTask bt(pos, toBuild);
	ATask *task = addTask(bt, groups);
	activeBuildTasks[task->key] = dynamic_cast<BuildTask*>(task);
}

void CTaskHandler::addAssistTask(float3 &pos, ATask &task, std::vector<CGroup*> &groups) {
	AssistTask at(pos, task);
	ATask *assistTask = addTask(at, groups);
	activeAssistTasks[task->key] = dynamic_cast<AssistTask*>(task);
}

void CTaskHandler::addAttackTask(int target, std::vector<CGroup*> &groups) {
	AttackTask at(target);
	ATask *task = addTask(at, groups);
	activeAttackTasks[task->key] = dynamic_cast<AttackTask*>(task);
}

void CTaskHandler::addMergeTask(std::vector<CGroup*> &groups) {
	float3 pos(0.0f, 0.0f, 0.0f);
	float range = 0.0f;
	for (unsigned i = 0; i < groups.size(); i++) {
		pos += groups[i]->pos();
		range += groups[i]->range();
	}
	pos   /= groups.size();
	range /= groups.size();
	
	MergeTask mt(pos, range);
	ATask *task = addTask(mt, groups);
	activeMergeTasks[task->key] = dynamic_cast<MergeTask*>(task);
}

void update() {
	std::map<int, ATask*>::iterator i;
	int tasknr = 0;
	for (i = activeTasks.begin(); i != activeTasks.end(); i++) {
		if (update % activeTasks.size() == tasknr)
			i->second->update();
		tasknr++;
	}
	update++;
}

ATask* addTask(ATask &at, std::vector<CGroup*> &groups) {
	int index   = 0;
	ATask *task = NULL;

	/* Create a new slot */
	if (free[at.type].empty()) {
		taskContainer[at.type].push_back(at);
		index = taskContainer[at.type].size() - 1;
		task  = &taskContainer[at.type][index];
	}
	/* Use top free slot */
	else {
		index = free[at.type].top(); free[at.type].pop();
		task  = &taskContainer[at.type][index];
		task->reset(at.pos);
	}
	lookup[at.type][task->key] = index;
	task->reg(*this);
	activeTasks[task->key] = task;
	for (unsigned i = 0; i < groups.size(); i++)
		task->addGroup(*groups[i]);
	ai->pf->addTask(*task);
	sprintf(buf, 
		"[CTaskHandler::addTask]\tTask %s(%d) created with %d groups",
		taskStr[task->t], 
		task->key, 
		groups.size()
	);
	LOGN(buf);
	return task;
}
