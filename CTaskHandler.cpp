#include "CTaskHandler.h"

int ATask::counter = 0;

void ATask::remove() {
	std::map<int, CGroup*>::iterator i;
	for (i = groups.begin(); i != groups.end(); i++) {
		i->second->unreg(*this);
		i->second->busy = false;
		i->second->stop();
	}
	
	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); j++)
		(*j)->remove(*this);
}

void ATask::remove(ARegistrar &group) {
	groups.erase(group.key);
	moving.erase(group.key);
	
	if (groups.empty()) {
		std::list<ARegistrar*>::iterator i;
		for (i = records.begin(); i != records.end(); i++)
			(*i)->remove(*this);
	}
}

void ATask::addGroup(CGroup &group) {
	groups[group.key] = &group;
	group.reg(*this);
	group.busy = true;
	moving[group.key] = true;
}

void ATask::reset(float3 &p) {
	records.clear();
	groups.clear();
	moving.clear();
	pos = p;
}

CTaskHandler::CTaskHandler(AIClasses *ai): ARegistrar(500) {
	this->ai    = ai;
	updateCount = 0;

	taskStr[ASSIST] = std::string("ASSIST");
	taskStr[BUILD]  = std::string("BUILD");
	taskStr[ATTACK] = std::string("ATTACK");
	taskStr[MERGE]  = std::string("MERGE");
	taskStr[FACTORY_BUILD]= std::string("FACTORY_BUILD");

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
	ATask *t = dynamic_cast<ATask*>(&task);
	free[t->t].push(t->key);
	lookup[t->t].erase(t->key);
	activeTasks.erase(t->key);
	switch(t->t) {
		case BUILD:  activeBuildTasks.erase(t->key);  break;
		case ASSIST: activeAssistTasks.erase(t->key); break;
		case ATTACK: activeAttackTasks.erase(t->key); break;
		case MERGE:  activeMergeTasks.erase(t->key);  break;
		case FACTORY: activeFactoryTasks.erase(t->key); break;

		default: return;
	}
}

void CTaskHandler::addBuildTask(buildType build, UnitType *toBuild, std::vector<CGroup*> &groups, float3 pos) {
	float range;
	if (pos == NULLVECTOR) getGroupsRangeAndPos(groups, range, pos);

	BuildTask bt(pos, build, toBuild);
	ATask *task = addTask(bt, groups);
	activeBuildTasks[task->key] = dynamic_cast<BuildTask*>(task);
	ai->pf->addTask(*task);
}

void CTaskHandler::addFactoryTask(CUnit &factory) {
	std::vector<CGroup*> groups;
	FactoryTask ft(factory);
	ATask *task = addTask(ft, groups);
	activeFactoryTasks[task->key] = dynamic_cast<FactoryTask*>(task);
}

void CTaskHandler::addAssistTask(ATask &task, std::vector<CGroup*> &groups) {
	AssistTask at(task);
	ATask *assistTask = addTask(at, groups);
	activeAssistTasks[task.key] = dynamic_cast<AssistTask*>(assistTask);
	ai->pf->addTask(*assistTask);
}

void CTaskHandler::addAttackTask(int target, std::vector<CGroup*> &groups) {
	AttackTask at(target);
	ATask *task = addTask(at, groups);
	activeAttackTasks[task->key] = dynamic_cast<AttackTask*>(task);
	ai->pf->addTask(*task);
}

void CTaskHandler::addMergeTask(std::vector<CGroup*> &groups) {
	float3 pos; float range;
	getGroupsRangeAndPos(groups, range, pos);
	MergeTask mt(pos, range);
	ATask *task = addTask(mt, groups);
	activeMergeTasks[task->key] = dynamic_cast<MergeTask*>(task);
	ai->pf->addTask(*task);
}
		
void CTaskHandler::getGroupsRangeAndPos(std::vector<CGroup*> &groups, float &range, float3 &pos) {
	range = pos.x = pos.y = pos.z = 0.0f;
	for (unsigned i = 0; i < groups.size(); i++) {
		pos += groups[i]->pos();
		range += groups[i]->range;
	}
	pos   /= groups.size();
	range /= groups.size();
}

void CTaskHandler::update() {
	std::map<int, ATask*>::iterator i;
	int tasknr = 0;
	for (i = activeTasks.begin(); i != activeTasks.end(); i++) {
		if (updateCount % activeTasks.size() == tasknr)
			i->second->update();
		tasknr++;
	}
	updateCount++;
}

ATask* CTaskHandler::addTask(ATask &at, std::vector<CGroup*> &groups) {
	int index   = 0;
	ATask *task = NULL;

	/* Create a new slot */
	if (free[at.t].empty()) {
		taskContainer[at.t].push_back(at);
		index = taskContainer[at.t].size() - 1;
		task  = &taskContainer[at.t][index];
	}
	/* Use top free slot */
	else {
		index = free[at.t].top(); free[at.t].pop();
		task  = &taskContainer[at.t][index];
		task->reset(at.pos);
	}
	lookup[at.t][task->key] = index;
	task->reg(*this);
	activeTasks[task->key] = task;
	for (unsigned i = 0; i < groups.size(); i++)
		task->addGroup(*groups[i]);
	sprintf(buf, 
		"[CTaskHandler::addTask]\tTask %s(%d) created with %d groups",
		taskStr[task->t], 
		task->key, 
		groups.size()
	);
	LOGN(buf);
	return task;
}

void CTaskHandler::BuildTask::update() {
	std::map<int, CGroup*>::iterator i;
	bool hasFinished = false;
	for (i = groups.begin(); i != groups.end(); i++) {
		CGroup *group = i->second;
		float3 dist = group->pos() - pos;
		if (moving[group->key] && dist.Length2D() <= group->range) {
			group->build(pos, toBuild);
			moving[group->key] = false;
			ai->pf->remove(*group);
			isMoving = false;
		}

		/* We are building, lets see if it finished already */
		if (!moving[group->key])
			if (ai->eco->hasFinishedBuilding(*group))
				hasFinished = true;
	}

	if (hasFinished)
		remove();
}

void CTaskHandler::FactoryTask::FactoryTask(CUnit &unit):
	ATask(FACTORY_BUILD, unit.pos()), factory(&unit) {}
	

void CTaskHandler::FactoryTask::update() {
	if (!ai->unitTable->factories[factory->key] && !ai->wl->empty(factory)) {
		UnitType *ut = ai->wl->top(factory);
		factory->factoryBuild(ut);
		ai->unitTable->factoriesBuilding[factory->key] = ut;
		ai->unitTable->factories[factory->key] = true;
	}
}

void CTaskHandler::AssistTask::AssistTask(ATask &task):
	ATask(ASSIST, task.pos), assist(&task) {

	/* This will ensure that when the original task finishes (is
	 * removed) it also calls this removal, lovely isn't it :)
	 */
	task.reg(*this);
}
	

void CTaskHandler::AssistTask::update() {
	std::map<int, CGroup*>::iterator i;
	for (i = groups.begin(); i != groups.end(); i++) {
		CGroup *group = i->second;
		float3 dist = group->pos() - pos;
		if (moving[group->key] && dist.Length2D() <= group->range) {
			group->assist(*assist);
			moving[group->key] = false;
			ai->pf->remove(*group);
			isMoving = false;
		}
	}
}

void CTaskHandler::AttackTask::AttackTask(int _target):
	ATask(ATTACK, ai->cheat->GetUnitPos(_target), target(_target)) {}

void CTaskHandler::AttackTask::update() {
	std::map<int, CGroup*>::iterator i;

	/* See if we can attack our target already */
	for (i = groups.begin(); i != groups.end(); i++) {
		CGroup *group = i->second;
		float3 dist = group->pos() - pos;
		if (moving[group->key] && dist.Length2D() <= group->range) {
			group->attack(target);
			moving[group->key] = false;
			ai->pf->remove(*group);
		}
		else pos = ai->cheat->GetUnitPos(target);
	}

	/* If the target is destroyed, remove the task, unreg groups */
	if (ai->cheat->GetUnitPos(target) == NULLVECTOR) 
		remove();
}

void CTaskHandler::MergeTask::update() {
	std::map<int, CGroup*>::iterator i;
	std::vector<CGroup*> mergable;

	/* See which groups can be merged already */
	for (i = groups.begin(); i != groups.end(); i++) {
		CGroup *group = i->second;
		float3 dist = group->pos() - pos;
		if (dist.Length2D() <= range)
			mergable.push_back(group);
	}

	/* We have atleast two groups, now we can merge */
	if (mergable.size() >= 2) {
		CGroup *alpha = mergable[0];
		for (unsigned j = 1; j < mergable.size(); j++)
			alpha->merge(*mergable[j]);
	}

	/* If only one group remains, merging is no longer possible,
	 * remove the task, unreg groups 
	 */
	if (groups.size() <= 1) 
		remove();
}
