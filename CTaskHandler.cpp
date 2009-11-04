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
	group->unwait();

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

std::ostream& operator<<(std::ostream &out, const ATask &atask) {
	std::stringstream ss;
	switch(atask.t) {
		case BUILD: {
			const CTaskHandler::BuildTask *task = dynamic_cast<const CTaskHandler::BuildTask*>(&atask);
			ss << "BuildTask(" << task->key << ") " << CTaskHandler::buildStr[task->bt];
			ss << "(" << task->toBuild->def->humanName << ") ETA(" << task->eta << ")";
			ss << " timer("<<task->timer<<") "<<(*(task->group));
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

CTaskHandler::CTaskHandler(AIClasses *ai): ARegistrar(500, std::string("taskhandler")) {
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
}

void CTaskHandler::remove(ARegistrar &task) {
	ATask *t = dynamic_cast<ATask*>(&task);
	obsoleteTasks.push(t);
	LOG_II("CTaskHandler::remove " << (*t))
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

void CTaskHandler::getGroupsPos(std::vector<CGroup*> &groups, float3 &pos) {
	pos.x = pos.y = pos.z = 0.0f;
	for (unsigned i = 0; i < groups.size(); i++)
		pos += groups[i]->pos();
	pos /= groups.size();
}

void CTaskHandler::update() {
	/* delete obsolete tasks from memory */
	while(!obsoleteTasks.empty()) {
		ATask *t = obsoleteTasks.top();
		obsoleteTasks.pop();
		activeTasks.erase(t->key);
		delete t;
	}

	/* Begin task updates */
	std::map<int, ATask*>::iterator i;
	for (i = activeTasks.begin(); i != activeTasks.end(); i++) {
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
void CTaskHandler::addBuildTask(buildType build, UnitType *toBuild, CGroup &group, float3 &pos) {
	BuildTask *buildTask = new BuildTask(ai);
	buildTask->pos       = pos;
	buildTask->bt        = build;
	buildTask->toBuild   = toBuild;
	buildTask->eta       = int((ai->pathfinder->getETA(group, pos)+100)*1.3f);
	buildTask->reg(*this);
	buildTask->addGroup(group);

	activeBuildTasks[buildTask->key] = buildTask;
	activeTasks[buildTask->key] = buildTask;

	groupToTask[group.key] = buildTask;
	LOG_II((*buildTask))
	if (!ai->pathfinder->addTask(*buildTask))
		buildTask->remove();
}

void CTaskHandler::BuildTask::update() {
	float3 grouppos = group->pos();
	float3 dist = grouppos - pos;
	timer += MULTIPLEXER;

	/* See if we can build yet */
	if (isMoving && dist.Length2D() <= group->buildRange) {
		group->build(pos, toBuild);
		ai->pathfinder->remove(*this);
		unreg(*(ai->pathfinder));
		isMoving = false;
	}

	/* We are building or blocked */
	if (!isMoving) { 
		if (ai->economy->hasFinishedBuilding(*group))
			remove();
		else if (timer > eta && !ai->economy->hasBegunBuilding(*group)) {
			LOG_WW("BuildTask::update assuming buildpos blocked for group "<<(*group))
			remove();
		}
	}
}

bool CTaskHandler::BuildTask::assistable(CGroup &assister, float &travelTime) {
	float buildSpeed = group->buildSpeed;
	std::list<ATask*>::iterator i;
	for (i = assisters.begin(); i != assisters.end(); i++)
		buildSpeed += (*i)->group->buildSpeed;

	float3 gpos = group->pos();
	float buildTime = (toBuild->def->buildTime / buildSpeed) * 32.0f;
	travelTime = ai->pathfinder->getETA(assister, gpos);

	/* If a build takes more then 5 seconds after arrival, we can assist it */
	return ((buildTime+30*5) > travelTime);
}

/**************************************************************/
/************************* FACTORY TASK ***********************/
/**************************************************************/
void CTaskHandler::addFactoryTask(CUnit &factory) {
	FactoryTask *factoryTask = new FactoryTask(ai);
	factoryTask->pos         = factory.pos();
	factoryTask->factory     = &factory;
	factoryTask->reg(*this);

	activeFactoryTasks[factoryTask->key] = factoryTask;
	activeTasks[factoryTask->key] = factoryTask;

	factory.reg(*factoryTask);
	LOG_II((*factoryTask))
}

bool CTaskHandler::FactoryTask::assistable(CGroup &assister) {
	if (assisters.size() > 10) 
		return false;

	float buildSpeed = factory->def->buildSpeed;
	std::list<ATask*>::iterator i;
	for (i = assisters.begin(); i != assisters.end(); i++)
		buildSpeed += (*i)->group->buildSpeed;

	UnitType *toBuild = ai->unittable->factoriesBuilding[factory->key];
	float3 gpos = factory->pos();
	float buildTime = (toBuild->def->buildTime / buildSpeed) * 32.0f;
	float travelTime = ai->pathfinder->getETA(assister, gpos);

	return (buildTime > travelTime);
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
void CTaskHandler::addAssistTask(ATask &toAssist, CGroup &group) {
	AssistTask *assistTask = new AssistTask(ai);
	assistTask->assist     = &toAssist;
	assistTask->pos        = toAssist.pos;
	assistTask->addGroup(group);
	assistTask->reg(*this);

	toAssist.assisters.push_back(assistTask);
	activeAssistTasks[assistTask->key] = assistTask;
	activeTasks[assistTask->key] = assistTask;
	groupToTask[group.key] = assistTask;
	LOG_II((*assistTask))
	if (!ai->pathfinder->addTask(*assistTask))
		assistTask->remove();
}

void CTaskHandler::AssistTask::remove(ARegistrar &group) {
	LOG_II("AssistTask::remove " << (*this))

	assist->assisters.remove(this);
	
	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); j++)
		(*j)->remove(*this);
}

void CTaskHandler::AssistTask::remove() {
	LOG_II("AssistTask::remove " << (*this))
	group->unreg(*this);
	group->busy = false;
	group->unwait();

	assist->assisters.remove(this);
	
	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); j++)
		(*j)->remove(*this);
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
void CTaskHandler::addAttackTask(int target, CGroup &group) {
	const UnitDef *ud = ai->cbc->GetUnitDef(target);
	if (ud == NULL) return;

	AttackTask *attackTask = new AttackTask(ai);
	attackTask->target     = target;
	attackTask->pos        = ai->cbc->GetUnitPos(target);
	attackTask->enemy      = ud->humanName;
	attackTask->reg(*this);
	attackTask->addGroup(group);

	activeAttackTasks[attackTask->key] = attackTask;
	activeTasks[attackTask->key] = attackTask;
	groupToTask[group.key] = attackTask;
	LOG_II((*attackTask))
	if (!ai->pathfinder->addTask(*attackTask))
		attackTask->remove();
}

void CTaskHandler::AttackTask::update() {
	/* If the target is destroyed, remove the task, unreg groups */
	if (ai->cbc->GetUnitHealth(target) <= 0.0f) {
		remove();
		return;
	}

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
}

/**************************************************************/
/************************* MERGE TASK *************************/
/**************************************************************/
void CTaskHandler::addMergeTask(std::vector<CGroup*> &groups) {
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
