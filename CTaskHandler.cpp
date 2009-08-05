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

void ATask::reset() {
	records.clear();
	groups.clear();
	moving.clear();
	isMoving = true;
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
	free[t->t].push(t->key);
	lookup[t->t].erase(t->key);
	activeTasks.erase(t->key);
	switch(t->t) {
		case BUILD:         activeBuildTasks.erase(t->key);  break;
		case ASSIST:        activeAssistTasks.erase(t->key); break;
		case ATTACK:        activeAttackTasks.erase(t->key); break;
		case MERGE:         activeMergeTasks.erase(t->key);  break;
		case FACTORY_BUILD: activeFactoryTasks.erase(t->key); break;

		default: return;
	}
	sprintf(buf, 
		"[CTaskHandler::remove]\tTask %s",
		taskStr[t->t].c_str()
	);
	LOGN(buf);
}

void CTaskHandler::addBuildTask(buildType build, UnitType *toBuild, std::vector<CGroup*> &groups, float3 pos) {
	float range;
	if (pos == NULLVECTOR) 
		getGroupsRangeAndPos(groups, range, pos);

	ATask *task = requestTask(BUILD, groups);

	if (task == NULL) {
		task = new BuildTask(ai, pos, build, toBuild);
		BuildTask *buildTask = dynamic_cast<BuildTask*>(task);
		activeBuildTasks[task->key] = buildTask;
		activeTasks[task->key]      = task;
		tasks[BUILD].push_back(task);
	}
	else {
		BuildTask *buildTask        = dynamic_cast<BuildTask*>(task);
		buildTask->pos              = pos;
		buildTask->bt               = build;
		buildTask->toBuild          = toBuild;
		activeBuildTasks[task->key] = buildTask;
	}

	for (unsigned i = 0; i < groups.size(); i++)
		task->addGroup(*groups[i]);

	ai->pf->addTask(*task);
	task->reg(*this);
}

void CTaskHandler::addFactoryTask(CUnit &factory) {
}

void CTaskHandler::addAssistTask(ATask &task, std::vector<CGroup*> &groups) {
}

void CTaskHandler::addAttackTask(int target, std::vector<CGroup*> &groups) {
}

void CTaskHandler::addMergeTask(std::vector<CGroup*> &groups) {
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
	unsigned tasknr = 0;
	for (i = activeTasks.begin(); i != activeTasks.end(); i++) {
		if (updateCount % activeTasks.size() == tasknr)
			i->second->update();
		tasknr++;
	}
	updateCount++;
}

ATask* CTaskHandler::requestTask(task t, std::vector<CGroup*> &groups) {
	sprintf(buf, 
		"[CTaskHandler::requestTask]\tTask %s created with %d groups",
		taskStr[t].c_str(), 
		groups.size()
	);
	LOGN(buf);

	/* If there are no free tasks of this type, return NULL */
	if (free[t].empty()) 
		return NULL;

	int index   = free[t].top(); free[t].pop();
	ATask *task = tasks[t][index];
	task->reset();
	lookup[t][task->key] = index;
	task->reg(*this);
	activeTasks[task->key] = task;

	for (unsigned i = 0; i < groups.size(); i++)
		task->addGroup(*groups[i]);

	return task;
}

CTaskHandler::BuildTask::BuildTask(AIClasses *ai, float3 &pos, buildType _bt, UnitType *_toBuild):
	ATask(ai, BUILD, pos), bt(_bt), toBuild(_toBuild) {}

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
		if (!isMoving)
			if (ai->eco->hasFinishedBuilding(*group))
				hasFinished = true;
	}

	if (hasFinished)
		remove();
}

CTaskHandler::FactoryTask::FactoryTask(AIClasses *ai, CUnit &unit):
	ATask(ai, FACTORY_BUILD), factory(&unit) {
	this->pos = unit.pos();	
}
	

void CTaskHandler::FactoryTask::update() {
	if (!ai->unitTable->factories[factory->key] && !ai->wl->empty(factory->key)) {
		UnitType *ut = ai->wl->top(factory->key);
		factory->factoryBuild(ut);
		ai->unitTable->factoriesBuilding[factory->key] = ut;
		ai->unitTable->factories[factory->key] = true;
	}
}

CTaskHandler::AssistTask::AssistTask(AIClasses *ai, ATask &task):
	ATask(ai, ASSIST, task.pos), assist(&task) {

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

CTaskHandler::AttackTask::AttackTask(AIClasses *ai, int _target):
	ATask(ai, ATTACK), target(_target) {
	this->pos = ai->cheat->GetUnitPos(_target);
}

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

CTaskHandler::MergeTask::MergeTask(AIClasses *ai, float3 &pos, float _range):
	ATask(ai, MERGE, pos), range(_range) {}

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
