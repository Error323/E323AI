#include "CTaskHandler.h"

#include <iostream>
#include <sstream>
#include <string>

#include "CAI.h"
#include "CMetalMap.h"
#include "CUnitTable.h"
#include "CWishList.h"
#include "CPathfinder.h"
#include "CUnit.h"
#include "CGroup.h"
#include "CEconomy.h"

/**************************************************************/
/************************* ATASK ******************************/
/**************************************************************/
int ATask::counter = 0;

void ATask::remove() {
	LOG_II("ATask::remove " << (*this))
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
	LOG_II("ATask::remove " << (*this))
	std::list<ATask*>::iterator i;
	for (i = assisters.begin(); i != assisters.end(); i++)
		(*i)->remove();
	
	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); j++)
		(*j)->remove(*this);
}

void ATask::addGroup(CGroup &g) {
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
			canAssist = (assisters.size() < 2);
		break;
		case ATTACK: case FACTORY_BUILD:
			canAssist = (assisters.size() < 5);
		break;
		case MERGE: case ASSIST:
			canAssist = false;
		break;
	}
	return canAssist;
}

std::ostream& operator<<(std::ostream &out, const ATask &atask) {
	std::stringstream ss;
	switch(atask.t) {
		case BUILD: {
			const CTaskHandler::BuildTask *task = dynamic_cast<const CTaskHandler::BuildTask*>(&atask);
			ss << "BuildTask(" << task->key << ") " << CTaskHandler::buildStr[task->bt];
			ss << "(" << task->toBuild->def->humanName << ") " << (*(task->group));
		} break;

		case ASSIST: {
			const CTaskHandler::AssistTask *task = dynamic_cast<const CTaskHandler::AssistTask*>(&atask);
			ss << "AssistTask(" << task->key << ") Assisting(" << (*task->assist) << ") ";
			ss << (*(task->group));
		} break;

		case ATTACK: {
			const CTaskHandler::AttackTask *task = dynamic_cast<const CTaskHandler::AttackTask*>(&atask);
			ss << "AttackTask(" << task->key << ") target("; ss << task->enemy << ") ";
			ss << (*(task->group));
		} break;

		case FACTORY_BUILD: {
			const CTaskHandler::FactoryTask *task = dynamic_cast<const CTaskHandler::FactoryTask*>(&atask);
			ss << "FactoryTask(" << task->key << ") " << (*(task->factory));
		} break;
		default: return out;
	}

	if (atask.t != ASSIST) {
		ss << " Assisters: amount(" << atask.assisters.size() << ") [";
		std::list<ATask*>::const_iterator i;
		for (i = atask.assisters.begin(); i != atask.assisters.end(); i++)
			ss << (*(*i)->group);
		ss << "] ";
	}

	std::string s = ss.str();
	out << s;
	return out;
}
/**************************************************************/
/************************* CTASKHANDLER ***********************/
/**************************************************************/
std::map<buildType, std::string> CTaskHandler::buildStr;

CTaskHandler::CTaskHandler(AIClasses *ai): ARegistrar(500) {
	this->ai = ai;

	taskStr[ASSIST]       = std::string("ASSIST");
	taskStr[BUILD]        = std::string("BUILD");
	taskStr[ATTACK]       = std::string("ATTACK");
	taskStr[MERGE]        = std::string("MERGE");
	taskStr[FACTORY_BUILD]= std::string("FACTORY_BUILD");

	CTaskHandler::buildStr[BUILD_MPROVIDER] = std::string("MPROVIDER");
	CTaskHandler::buildStr[BUILD_EPROVIDER] = std::string("EPROVIDER");
	CTaskHandler::buildStr[BUILD_AA_DEFENSE] = std::string("AA_DEFENSE");
	CTaskHandler::buildStr[BUILD_AG_DEFENSE] = std::string("AG_DEFENSE");
	CTaskHandler::buildStr[BUILD_FACTORY] = std::string("FACTORY");
	CTaskHandler::buildStr[BUILD_MSTORAGE] = std::string("MSTORAGE");
	CTaskHandler::buildStr[BUILD_ESTORAGE] = std::string("ESTORAGE");

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
		task = tasks[t][index];
		task->reset();
	}

	lookup[t][task->key] = index;
	task->reg(*this);
	activeTasks[task->key] = task;
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
	return groupToTask[group.key]->pos;
}

void CTaskHandler::removeTask(CGroup &group) {
	groupToTask[group.key]->remove();
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
	groupToTask[group.key] = task;
	LOG_II((*buildTask))
	if (!ai->pathfinder->addTask(*task))
		buildTask->remove();
}

void CTaskHandler::BuildTask::update() {
	float3 grouppos = group->pos();
	float3 dist = grouppos - pos;

	/* See if we can build yet */
	if (isMoving && dist.Length2D() <= group->buildRange) {
		group->build(pos, toBuild);
		ai->pathfinder->remove(*this);
		unreg(*(ai->pathfinder));
		isMoving = false;
	}

	/* We are building, lets see if it finished already */
	if (!isMoving && ai->economy->hasFinishedBuilding(*group)) {
		remove();
	}
}

bool CTaskHandler::BuildTask::assistable(CGroup &assister) {
	float buildSpeed = group->buildSpeed;
	std::list<ATask*>::iterator i;
	for (i = assisters.begin(); i != assisters.end(); i++)
		buildSpeed += (*i)->group->buildSpeed;

	float3 apos = assister.pos();
	float3 gpos = group->pos();
	float dist = (apos - gpos).Length2D() - assister.buildRange;
	float buildTime = (toBuild->def->buildTime / buildSpeed) * 32.0f;
	float travelTime = dist / assister.speed;

	return (buildTime > travelTime);
}

/**************************************************************/
/************************* FACTORY TASK ***********************/
/**************************************************************/
void CTaskHandler::FactoryTask::reset(CUnit &unit) {
	pos = unit.pos();
	factory = &unit;
	unit.reg(*this);
}

bool CTaskHandler::FactoryTask::assistable(CGroup &assister) {
	float buildSpeed = factory->def->buildSpeed;
	std::list<ATask*>::iterator i;
	for (i = assisters.begin(); i != assisters.end(); i++)
		buildSpeed += (*i)->group->buildSpeed;

	UnitType *toBuild = ai->unittable->factoriesBuilding[factory->key];
	float3 apos = assister.pos();
	float3 gpos = factory->pos();
	float dist = (apos - gpos).Length2D() - assister.buildRange;
	float buildTime = (toBuild->def->buildTime / buildSpeed) * 32.0f;
	float travelTime = dist / assister.speed;

	/* If a build takes more then 10 seconds, we can assist it */
	return (buildTime > travelTime);
}

void CTaskHandler::addFactoryTask(CUnit &factory) {
	ATask *task = requestTask(FACTORY_BUILD);
	FactoryTask *factoryTask = dynamic_cast<FactoryTask*>(task);
	factoryTask->reset(factory);
	activeFactoryTasks[task->key] = factoryTask;
	LOG_II((*factoryTask))
}

void CTaskHandler::FactoryTask::update() {
	if (ai->unittable->idle[factory->key] && !ai->wishlist->empty(factory->key)) {
		UnitType *ut = ai->wishlist->top(factory->key); ai->wishlist->pop(factory->key);
		factory->factoryBuild(ut);
		ai->unittable->factoriesBuilding[factory->key] = ut;
		ai->unittable->idle[factory->key] = false;
	}
}

void CTaskHandler::FactoryTask::setWait(bool on) {
	if (on) {
		factory->wait();
		std::list<ATask*>::iterator i;
		for (i = assisters.begin(); i != assisters.end(); i++) {
			if ((*i)->isMoving) continue;
			(*i)->group->wait();
		}
	}
	else {
		factory->unwait();
		std::list<ATask*>::iterator i;
		for (i = assisters.begin(); i != assisters.end(); i++) {
			if ((*i)->isMoving) continue;
			(*i)->group->unwait();
		}
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
	LOG_II("CGroup::remove " << (*this))
	group->unreg(*this);
	group->busy = false;
	group->stop();

	assist->assisters.remove(this);
	
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
	groupToTask[group.key] = task;
	LOG_II((*assistTask))
	if (!ai->pathfinder->addTask(*task))
		assistTask->remove();
}

void CTaskHandler::AssistTask::update() {
	float3 grouppos = group->pos();
	float3 dist = grouppos - pos;
	float range = (assist->t == ATTACK) ? group->range : group->buildRange;
	if (isMoving && dist.Length2D() <= range) {
		group->assist(*assist);
		ai->pathfinder->remove(*this);
		unreg(*(ai->pathfinder));
		isMoving = false;
	}
}

/**************************************************************/
/************************* ATTACK TASK ************************/
/**************************************************************/
void CTaskHandler::AttackTask::reset(int t) {
	target = t;
	pos = ai->cbc->GetUnitPos(t);
	enemy = ai->cbc->GetUnitDef(target)->humanName;
}

void CTaskHandler::addAttackTask(int target, CGroup &group) {
	ATask *task = requestTask(ATTACK);
	AttackTask *attackTask = dynamic_cast<AttackTask*>(task);
	attackTask->reset(target);
	attackTask->addGroup(group);
	activeAttackTasks[task->key] = attackTask;
	float3 gp = group.pos();
	groupToTask[group.key] = task;
	LOG_II((*attackTask))
	if (!ai->pathfinder->addTask(*task))
		attackTask->remove();

}

void CTaskHandler::AttackTask::update() {
	/* See if we can attack our target already */
	float3 grouppos = group->pos();
	float3 dist = grouppos - pos;
	if (isMoving && dist.Length2D() <= group->range) {
		group->attack(target);
		isMoving = false;
		ai->pathfinder->remove(*this);
		unreg(*(ai->pathfinder));
	}
	/* Keep tracking it */
	else pos = ai->cbc->GetUnitPos(target);

	/* If the target is destroyed, remove the task, unreg groups */
	if (ai->cbc->GetUnitHealth(target) <= 0.0f) 
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
		ai->pathfinder->addGroup(*groups[i], gp, task->pos);
		groupToTask[groups[i]->key] = task;
	}
}
		
void CTaskHandler::MergeTask::update() {
	std::vector<CGroup*> mergable;

	/* See which groups can be merged already */
	for (unsigned i = 0; i < groups.size(); i++) {
		CGroup *group = groups[i];
		float3 grouppos = group->pos();
		float3 dist = grouppos - pos;
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
