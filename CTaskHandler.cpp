#include "CTaskHandler.h"

/**************************************************************/
/************************* ATASK ******************************/
/**************************************************************/
int ATask::counter = 0;

void ATask::remove() {
	group->unreg(*this);
	group->busy = false;
	group->stop();

	std::list<ATask*>::iterator i;
	for (i = assisters.begin(); i != assisters.end(); i++)
		(*i)->remove();
	
	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); j++)
		(*j)->remove(*this);
}

void ATask::remove(ARegistrar &group) {
	sprintf(buf, "[ATask::remove]\tremove group(%d)", group.key);
	LOGN(buf);
	std::list<ARegistrar*>::iterator i;
	for (i = records.begin(); i != records.end(); i++)
		(*i)->remove(*this);
}

void ATask::addGroup(CGroup &g) {
	sprintf(buf, "[ATask::addGroup]\tadd group(%d)", g.key);
	LOGN(buf);
	this->group = &g;
	group->reg(*this);
	group->busy = true;
}

void ATask::reset() {
	records.clear();
	assisters.clear();
	isMoving = true;
}

bool ATask::assistable() {
	bool canAssist = false;
	switch(t) {
		case BUILD:
			canAssist = (assisters.size() <= 1);
		break;
		case ATTACK: case FACTORY_BUILD:
			canAssist = true;
		break;
		case MERGE: case ASSIST:
			canAssist = false;
		break;
	}
	return canAssist;
}
/**************************************************************/
/************************* CTASKHANDLER ***********************/
/**************************************************************/
CTaskHandler::CTaskHandler(AIClasses *ai): ARegistrar(500) {
	this->ai = ai;

	taskStr[ASSIST]       = std::string("ASSIST");
	taskStr[BUILD]        = std::string("BUILD");
	taskStr[ATTACK]       = std::string("ATTACK");
	taskStr[MERGE]        = std::string("MERGE");
	taskStr[FACTORY_BUILD]= std::string("FACTORY_BUILD");

	std::map<task, std::string>::iterator i;
	for (i = taskStr.begin(); i != taskStr.end(); i++) {
		std::vector<ATask*> V;
		tasks[i->first] = V;

		std::map<int, int> M;
		lookup[i->first] = M;

		std::stack<int> S;
		free[i->first] = S;
	}
}

void CTaskHandler::remove(ARegistrar &task) {
	ATask *t = dynamic_cast<ATask*>(&task);
	free[t->t].push(lookup[t->t][t->key]);
	lookup[t->t].erase(t->key);
	activeTasks.erase(t->key);
	switch(t->t) {
		case BUILD:
			activeBuildTasks.erase(t->key);
		break;
		case ASSIST:
			activeAssistTasks.erase(t->key); 
		break;
		case ATTACK:
			activeAttackTasks.erase(t->key); 
		break;
		case MERGE:
			activeMergeTasks.erase(t->key);
		break;
		case FACTORY_BUILD:
			activeFactoryTasks.erase(t->key);
		break;

		default: return;
	}
	sprintf(buf, 
		"[CTaskHandler::remove]\tremove task %s(%d)",
		taskStr[t->t].c_str(),
		t->key
	);
	LOGN(buf);
}

ATask* CTaskHandler::requestTask(task t) {
	ATask *task = NULL;
	int index   = -1;
	if (free[t].empty()) {
		switch(t) {
			case BUILD:
				task = new BuildTask();
			break;
			case ASSIST:
				task = new AssistTask();
			break;
			case ATTACK:
				task = new AttackTask();
			break;
			case MERGE:
				task = new MergeTask();
			break;
			case FACTORY_BUILD:
				task = new FactoryTask();
			break;
		}

		task->t  = t;
		task->ai = this->ai;
		tasks[t].push_back(task);
		index = tasks[t].size()-1;
	}
	else {
		index = free[t].top();
		free[t].pop();
	}

	task = tasks[t][index];
	lookup[t][task->key] = index;
	task->reset();
	task->reg(*this);
	activeTasks[task->key] = task;

	sprintf(buf, "[CTaskHandler::requestTask]\ttask %s(%d) created", taskStr[t].c_str(), task->key);
	LOGN(buf);
	return task;
}

void CTaskHandler::getGroupsPos(std::vector<CGroup*> &groups, float3 &pos) {
	pos.x = pos.y = pos.z = 0.0f;
	for (unsigned i = 0; i < groups.size(); i++)
		pos += groups[i]->pos();
	pos /= groups.size();
}

void CTaskHandler::update() {
	std::map<int, ATask*>::iterator i;
	for (i = activeTasks.begin(); i != activeTasks.end(); i++) {
		assert(i->second != NULL);
		i->second->update();
	}
}

float3 CTaskHandler::getPos(CGroup &group) {
	assert(groupToTask[group.key] != NULL);
	return groupToTask[group.key]->pos;
}

/**************************************************************/
/************************* BUILD TASK *************************/
/**************************************************************/
void CTaskHandler::BuildTask::reset(float3 &p, buildType b, UnitType *ut) {
	pos     = p;
	bt      = b;
	toBuild = ut;
}

void CTaskHandler::addBuildTask(buildType build, UnitType *toBuild, CGroup &group, float3 &pos) {
	ATask *task = requestTask(BUILD);
	BuildTask *buildTask = dynamic_cast<BuildTask*>(task);
	buildTask->reset(pos, build, toBuild);
	buildTask->addGroup(group);
	activeBuildTasks[task->key] = buildTask;
	float3 gp = group.pos();
	ai->pf->addGroup(group, gp, task->pos);
	groupToTask[group.key] = task;
}

void CTaskHandler::BuildTask::update() {
	bool hasFinished = false;
	float3 dist = group->pos() - pos;

	/* See if we can build yet */
	if (isMoving && dist.Length2D() <= group->buildRange) {
		group->build(pos, toBuild);
		ai->pf->remove(*group);
		isMoving = false;
	}

	/* We are building, lets see if it finished already */
	if (!isMoving)
		if (ai->eco->hasFinishedBuilding(*group))
			hasFinished = true;

	/* This task is finished, remove it */
	if (hasFinished)
		remove();
}

/**************************************************************/
/************************* FACTORY TASK ***********************/
/**************************************************************/
void CTaskHandler::FactoryTask::reset(CUnit &unit) {
	pos = unit.pos();
	factory = &unit;
	unit.reg(*this);
}

void CTaskHandler::addFactoryTask(CUnit &factory) {
	ATask *task = requestTask(FACTORY_BUILD);
	FactoryTask *factoryTask = dynamic_cast<FactoryTask*>(task);
	factoryTask->reset(factory);
	activeFactoryTasks[task->key] = factoryTask;
}

void CTaskHandler::FactoryTask::update() {
	if (!ai->unitTable->factories[factory->key] && !ai->wl->empty(factory->key)) {
		UnitType *ut = ai->wl->top(factory->key); ai->wl->pop(factory->key);
		factory->factoryBuild(ut);
		ai->unitTable->factoriesBuilding[factory->key] = ut;
		ai->unitTable->factories[factory->key] = true;
	}
	else if (ai->unitTable->builders.find(factory->key) != ai->unitTable->builders.end() &&
		ai->unitTable->builders[factory->key]) {
		ai->unitTable->factories[factory->key] = false;
		ai->unitTable->builders[factory->key] = true;
	}
}


/**************************************************************/
/************************* ASSIST TASK ************************/
/**************************************************************/
void CTaskHandler::AssistTask::reset(ATask &task) {
	assist = &task;
	pos    = task.pos;
	task.assisters.push_back(this);
}

void CTaskHandler::AssistTask::remove() {
	group->unreg(*this);
	group->busy = false;
	group->stop();

	assist->assisters.remove(this);

	std::list<ATask*>::iterator i;
	for (i = assisters.begin(); i != assisters.end(); i++)
		(*i)->remove();
	
	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); j++)
		(*j)->remove(*this);
}

void CTaskHandler::addAssistTask(ATask &toAssist, CGroup &group) {
	ATask *task = requestTask(ASSIST);
	AssistTask *assistTask = dynamic_cast<AssistTask*>(task);
	assistTask->reset(toAssist);
	assistTask->addGroup(group);
	activeAssistTasks[task->key] = assistTask;
	float3 gp = group.pos();
	ai->pf->addGroup(group, gp, toAssist.pos);
	groupToTask[group.key] = task;
}

void CTaskHandler::AssistTask::update() {
	float3 dist = group->pos() - pos;
	if (isMoving && dist.Length2D() <= group->buildRange) {
		group->assist(*assist);
		ai->pf->remove(*group);
		isMoving = false;
	}
}

/**************************************************************/
/************************* ATTACK TASK ************************/
/**************************************************************/
void CTaskHandler::AttackTask::reset(int t) {
	target = t;
	pos = ai->cheat->GetUnitPos(t);
}

void CTaskHandler::addAttackTask(int target, CGroup &group) {
	ATask *task = requestTask(ATTACK);
	AttackTask *attackTask = dynamic_cast<AttackTask*>(task);
	attackTask->reset(target);
	attackTask->addGroup(group);
	activeAttackTasks[task->key] = attackTask;
	float3 gp = group.pos();
	ai->pf->addGroup(group, gp, task->pos);
	groupToTask[group.key] = task;
}

void CTaskHandler::AttackTask::update() {
	/* See if we can attack our target already */
	float3 dist = group->pos() - pos;
	if (isMoving && dist.Length2D() <= group->range) {
		group->attack(target);
		isMoving = false;
		ai->pf->remove(*group);
	}
	/* Keep tracking it */
	else pos = ai->cheat->GetUnitPos(target);

	/* If the target is destroyed, remove the task, unreg groups */
	if (ai->cheat->GetUnitPos(target) == NULLVECTOR) 
		remove();
}

/**************************************************************/
/************************* MERGE TASK *************************/
/**************************************************************/
void CTaskHandler::MergeTask::reset(std::vector<CGroup*> &groups) {
	CTaskHandler::getGroupsPos(groups, pos);
	range = 0.0f;
	this->groups = groups;
}

void CTaskHandler::addMergeTask(std::vector<CGroup*> &groups) {
	ATask *task = requestTask(MERGE);
	MergeTask *mergeTask = dynamic_cast<MergeTask*>(task);
	mergeTask->reset(groups);
	activeMergeTasks[task->key] = mergeTask;
	for (unsigned i = 0; i < groups.size(); i++) {
		float3 gp = groups[i]->pos();
		ai->pf->addGroup(*groups[i], gp, task->pos);
		groupToTask[groups[i]->key] = task;
	}
}
		
void CTaskHandler::MergeTask::update() {
	std::vector<CGroup*> mergable;

	/* See which groups can be merged already */
	for (unsigned i = 0; i < groups.size(); i++) {
		CGroup *group = groups[i];
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

	/* If only one (or none) group remains, merging is no longer possible,
	 * remove the task, unreg groups 
	 */
	if (groups.size() <= 1)
		remove();
}
