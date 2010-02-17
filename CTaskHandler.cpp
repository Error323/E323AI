#include "CTaskHandler.h"

#include <iostream>
#include <sstream>
#include <string>
#include <math.h>

#include "CAI.h"
#include "CUnitTable.h"
#include "CWishList.h"
#include "CPathfinder.h"
#include "CUnit.h"
#include "CGroup.h"
#include "CEconomy.h"
#include "CConfigParser.h"
#include "CThreatMap.h"
#include "CScopedTimer.h"

/**************************************************************/
/************************* ATASK ******************************/
/**************************************************************/
int ATask::counter = 0;

void ATask::remove() {
	LOG_II("ATask::remove " << (*this))

	// NOTE: removal order below is VERY important

	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); j++)
		// remove current task from CTaskHandler, so it will mark this task 
		// to be killed on next update
		(*j)->remove(*this);

	// remove all assisting tasks...
	std::list<ATask*>::iterator i = assisters.begin();
	while(i != assisters.end()) {
		ATask *task = *i; i++;
		task->remove();
	}
	//assert(assisters.size() == 0);

	if (group) {
		// remove from group...
		group->unreg(*this);
		group->busy = false;
		group->unwait();
		group->micro(false);
		group->abilities(false);
		group = NULL;
	}

	active = false;
}

// called on Group removing
void ATask::remove(ARegistrar &group) {
	LOG_II("ATask::remove by group(" << (*(dynamic_cast<CGroup*>(&group))) << ")")

	// NOTE: experimental
	//group = NULL; // group is going to be inactive

	remove();
}

void ATask::addGroup(CGroup &g) {
	// make sure a task is active per only group
	assert(group == NULL);
	group = &g;
	group->reg(*this);
	group->busy = true;
	group->micro(false);
	group->abilities(true);
}

void ATask::enemyScan(bool scout) {
	CScopedTimer t(std::string("tasks-enemyscan"));
	std::multimap<float, int> candidates;
	float3 pos = group->pos();
	int enemyids[MAX_ENEMIES];
	int numEnemies = ai->cbc->GetEnemyUnits(&enemyids[0], pos, group->range, MAX_ENEMIES);
	for (int i = 0; i < numEnemies; i++) {
		const UnitDef *ud = ai->cbc->GetUnitDef(enemyids[i]);
		UnitType *ut = UT(ud->id);
		float3 epos = ai->cbc->GetUnitPos(enemyids[i]);
		float dist = (epos-pos).Length2D();
		if (!(ut->cats&AIR) && !(ut->cats&SCOUTER) && !(ai->cbc->IsUnitCloaked(enemyids[i])))
			candidates.insert(std::pair<float,int>(dist, enemyids[i]));
	}

	if (!candidates.empty()) {
		std::multimap<float,int>::iterator i = candidates.begin();
		float3 epos = ai->cbc->GetUnitPos(i->second);
		bool offensive = ai->threatmap->getThreat(epos, 400.0f) > 1.1f;
		if (scout && !offensive) {
			group->attack(i->second);
			group->micro(true);
			LOG_II("ATask::enemyScan scout " << (*group) << " is microing enemy targets")
		}
		else if (!scout) {
			group->attack(i->second);
			group->micro(true);
			LOG_II("ATask::enemyScan group " << (*group) << " is microing enemy targets")
		}
	}
}

void ATask::resourceScan() {
	CScopedTimer t(std::string("tasks-resourcescan"));
	/* Leave metal alone when we can't store it */
	if (ai->economy->mexceeding)
		return;

	float bestDist = std::numeric_limits<float>::max();
	int bestFeature = -1;
	float3 pos = group->pos();
	float radius = group->buildRange;
	const int numFeatures = ai->cb->GetFeatures(&ai->unitIDs[0], MAX_FEATURES, pos, radius);
	for (int i = 0; i < numFeatures; i++) {
		const FeatureDef *fd = ai->cb->GetFeatureDef(ai->unitIDs[i]);
		if (fd->metal > 0.0f) {
			float3 fpos = ai->cb->GetFeaturePos(ai->unitIDs[i]);
			float dist = (fpos - pos).Length2D();
			if (dist < bestDist) {
				bestFeature = ai->unitIDs[i];
				bestDist = dist;
			}
		}
	}

	if (bestFeature != -1) {
		group->reclaim(bestFeature);
		group->micro(true);
		LOG_II("ATask::resourceScan group " << (*group) << " is reclaiming")
	}
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
			ss << "AssistTask(" << task->key << ") assisting(" << (*task->assist) << ") ";
			ss << (*(task->group));
		} break;

		case ATTACK: {
			const CTaskHandler::AttackTask *task = dynamic_cast<const CTaskHandler::AttackTask*>(&atask);
			ss << "AttackTask(" << task->key << ") target(" << task->enemy << ") ";
			ss << (*(task->group));
		} break;

		case FACTORY_BUILD: {
			const CTaskHandler::FactoryTask *task = dynamic_cast<const CTaskHandler::FactoryTask*>(&atask);
			ss << "FactoryTask(" << task->key << ") ";
			ss << (*(task->group));
		} break;

		case MERGE: {
			const CTaskHandler::MergeTask *task = dynamic_cast<const CTaskHandler::MergeTask*>(&atask);
			ss << "MergeTask(" << task->key << ") " << task->groups.size() << " range("<<task->range<<") pos("<<task->pos.x<<", "<<task->pos.z<<") groups { ";
			std::map<int,CGroup*>::const_iterator i;
			for (i = task->groups.begin(); i != task->groups.end(); i++) {
				ss << (*(i->second)) << " ";
			}
			ss << "}";
		} break;

		default: return out;
	}

	if (atask.t != ASSIST && atask.t != MERGE) {
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
std::map<task, std::string> CTaskHandler::taskStr;

CTaskHandler::CTaskHandler(AIClasses *ai): ARegistrar(500, std::string("taskhandler")) {
	this->ai = ai;

	if (taskStr.empty()) {
		taskStr[ASSIST]        = std::string("ASSIST");
		taskStr[BUILD]         = std::string("BUILD");
		taskStr[ATTACK]        = std::string("ATTACK");
		taskStr[MERGE]         = std::string("MERGE");
		taskStr[FACTORY_BUILD] = std::string("FACTORY_BUILD");
	}

	if (buildStr.empty()) {
		buildStr[BUILD_MPROVIDER] = std::string("MPROVIDER");
		buildStr[BUILD_EPROVIDER] = std::string("EPROVIDER");
		buildStr[BUILD_AA_DEFENSE] = std::string("AA_DEFENSE");
		buildStr[BUILD_AG_DEFENSE] = std::string("AG_DEFENSE");
		buildStr[BUILD_FACTORY] = std::string("FACTORY");
		buildStr[BUILD_MSTORAGE] = std::string("MSTORAGE");
		buildStr[BUILD_ESTORAGE] = std::string("ESTORAGE");
	}
}

void CTaskHandler::remove(ARegistrar &task) {
	ATask *t = dynamic_cast<ATask*>(&task);
	
	LOG_II("CTaskHandler::remove " << (*t))
	
	obsoleteTasks.push(t);
	
	if (t->group)
		groupToTask.erase(t->group->key);
	
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
		// make sure task is really detached from group
		assert(t->group == NULL);
		delete t;
	}

	/* Begin task updates */
	std::map<int, ATask*>::iterator i;
	for (i = activeTasks.begin(); i != activeTasks.end(); i++) {
		if (i->second->active)
			i->second->update();
	}
}

float3 CTaskHandler::getPos(CGroup &group) {
	return groupToTask[group.key]->pos;
}

void CTaskHandler::removeTask(CGroup &group) {
	int key = group.key;
	groupToTask[key]->remove();
	groupToTask.erase(key);
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
	buildTask->reg(*this); // register task in a task handler
	buildTask->addGroup(group);

	activeBuildTasks[buildTask->key] = buildTask;
	activeTasks[buildTask->key] = buildTask;
	groupToTask[group.key] = buildTask;
	
	LOG_II((*buildTask))
	
	if (!ai->pathfinder->addGroup(group))
		buildTask->remove();
	else
		buildTask->active = true;
}

void CTaskHandler::BuildTask::update() {
	CScopedTimer t(std::string("tasks-build"));
	float3 grouppos = group->pos();
	float3 dist = grouppos - pos;
	timer += MULTIPLEXER;

	/* If idle, our micro is done */
	if (group->isMicroing() && group->isIdle())
		group->micro(false);

	/* See if we can build yet */
	if (isMoving && dist.Length2D() <= group->buildRange) {
		group->build(pos, toBuild);
		ai->pathfinder->remove(*group);
		isMoving = false;
	}
	/* See if we can suck wreckages */
	else if (isMoving && !group->isMicroing()) {
		resourceScan();
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
	if (bt == BUILD_AG_DEFENSE && assisters.size() >= 2) return false;
	float buildSpeed = group->buildSpeed;
	std::list<ATask*>::iterator i;
	for (i = assisters.begin(); i != assisters.end(); i++)
		buildSpeed += (*i)->group->buildSpeed;

	float3 gpos = group->pos();
	float buildTime = (toBuild->def->buildTime / buildSpeed) * 32.0f;
	travelTime = ai->pathfinder->getETA(assister, gpos);

	return (buildTime > travelTime);
}

/**************************************************************/
/************************* FACTORY TASK ***********************/
/**************************************************************/
void CTaskHandler::addFactoryTask(CGroup &group) {
	FactoryTask *factoryTask = new FactoryTask(ai);
	// NOTE: currently if factories are joined into one group then assisters 
	// will assist the first factory only
	//factoryTask->pos = group.pos();
	factoryTask->pos = group.units.begin()->second->pos();
	factoryTask->reg(*this); // register task in a task handler
	factoryTask->addGroup(group);

	activeFactoryTasks[factoryTask->key] = factoryTask;
	activeTasks[factoryTask->key] = factoryTask;
	groupToTask[factoryTask->key] = factoryTask;

	LOG_II((*factoryTask))

	factoryTask->active = true;
}

bool CTaskHandler::FactoryTask::assistable(CGroup &assister) {
	if (assisters.size() >= std::min(ai->cfgparser->getState() * 2, FACTORY_ASSISTERS) || 
		!group->units.begin()->second->def->canBeAssisted) {
		return false;
	}
	else {
		ai->wishlist->push(BUILDER, HIGH);
		return true;
	}

/*
	float3 gpos = factory->pos();
	float travelTime = ai->pathfinder->getETA(assister, gpos);

	bool canAssist = (travelTime < FACTORY_ASSISTERS-assisters.size());
	if (canAssist)
		ai->wishlist->push(BUILDER, HIGH);
	return canAssist;
*/
}

void CTaskHandler::FactoryTask::update() {
	CScopedTimer t(std::string("tasks-factory"));
	std::map<int,CUnit*>::iterator i;
	CUnit *factory;
	
	for(i = group->units.begin(); i != group->units.end(); i++) {
		factory = i->second;
		if (ai->unittable->idle[factory->key] && !ai->wishlist->empty(factory->key)) {
			UnitType *ut = ai->wishlist->top(factory->key); ai->wishlist->pop(factory->key);
			factory->factoryBuild(ut);
			ai->unittable->factoriesBuilding[factory->key] = ut;
			ai->unittable->idle[factory->key] = false;
		}
	}
}

void CTaskHandler::FactoryTask::setWait(bool on) {
	std::map<int,CUnit*>::iterator ui;
	std::list<ATask*>::iterator ti;
	CUnit *factory;

	for (ui = group->units.begin(); ui != group->units.end(); ui++) {
		factory = ui->second;
		if(on)
			factory->wait();
		else
			factory->unwait();
	}

	for (ti = assisters.begin(); ti != assisters.end(); ti++) {
		if ((*ti)->isMoving) continue;
		if(on)
			(*ti)->group->wait();
		else
			(*ti)->group->unwait();
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
	assistTask->reg(*this); // register task in a task handler

	toAssist.assisters.push_back(assistTask);

	activeAssistTasks[assistTask->key] = assistTask;
	activeTasks[assistTask->key] = assistTask;
	groupToTask[group.key] = assistTask;

	LOG_II((*assistTask))
	
	if (!ai->pathfinder->addGroup(group))
		assistTask->remove();
	else
		assistTask->active = true;
}

void CTaskHandler::AssistTask::remove(ARegistrar &group) {
	LOG_II("AssistTask::remove by group(" << (*(dynamic_cast<CGroup*>(&group))) << ")")
	
	//assert(this->group == &group);

	remove();
}

void CTaskHandler::AssistTask::remove() {
	LOG_II("AssistTask::remove " << (*this))
	
	// NOTE: we have to remove manually because assisting tasks are not 
	// completely built upon ARegistrar pattern
	assist->assisters.remove(this);

	ATask::remove();
}

void CTaskHandler::AssistTask::update() {
	CScopedTimer t(std::string("tasks-assist"));
	float3 grouppos = group->pos();
	float3 dist = grouppos - pos;
	float range = (assist->t == ATTACK) ? group->range : group->buildRange;
	if (assist->t == BUILD && group->isMicroing() && group->isIdle())
		group->micro(false);

	if (isMoving && dist.Length2D() <= range) {
		group->assist(*assist);
		ai->pathfinder->remove(*group);
		isMoving = false;
	}
	/* See if we can suck wreckages */
	else if (isMoving && assist->t == BUILD && !group->isMicroing()) {
		resourceScan();
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
	attackTask->reg(*this); // register task in a task handler
	attackTask->addGroup(group);

	activeAttackTasks[attackTask->key] = attackTask;
	activeTasks[attackTask->key] = attackTask;
	groupToTask[group.key] = attackTask;
	
	LOG_II((*attackTask))
	
	if (!ai->pathfinder->addGroup(group))
		attackTask->remove();
	else
		attackTask->active = true;
}

void CTaskHandler::AttackTask::update() {
	CScopedTimer t(std::string("tasks-attack"));
	if (group->isMicroing() && group->isIdle())
		group->micro(false);

	/* If the target is destroyed, remove the task, unreg groups */
	if (ai->cbc->GetUnitHealth(target) <= 0.0f) {
		ai->pathfinder->remove(*group);
		remove();
		return;
	}

	/* See if we can attack our target already */
	float3 grouppos = group->pos();
	float3 dist = grouppos - pos;
	if (isMoving && dist.Length2D() <= group->range) {
		group->attack(target);
		isMoving = false;
		ai->pathfinder->remove(*group);
	}
	/* See if we can attack a target we found on our path */
	else if (!group->isMicroing()) {
		if (group->units.begin()->second->type->cats&SCOUTER)
			enemyScan(true);
		else
			enemyScan(false);
	}
	/* Keep tracking the target */
	pos = ai->cbc->GetUnitPos(target);
}

/**************************************************************/
/************************* MERGE TASK *************************/
/**************************************************************/
void CTaskHandler::addMergeTask(std::map<int,CGroup*> &groups) {
	int i;
	int range = 0;
	int units = 0;
	std::map<int,CGroup*>::iterator j;
	float maxSlope = MAX_FLOAT;
	float sqLeg;
	
	MergeTask *mergeTask = new MergeTask(ai);
	mergeTask->groups = groups;
	mergeTask->pos = float3(0.0f, 0.0f, 0.0f);
	mergeTask->reg(*this); // register task in a task handler
	
	for (j = groups.begin(); j != groups.end(); j++) {
		j->second->reg(*mergeTask);
		j->second->busy = true;
		j->second->micro(false);
		j->second->abilities(true);
		groupToTask[j->first] = mergeTask;
		range += j->second->size;
		units += j->second->units.size();
		if (j->second->maxSlope < maxSlope) {
			maxSlope = j->second->maxSlope;
			mergeTask->pos = j->second->pos();
		}
	}
	
	// TODO: actually range should increase from the smallest to calculated 
	// here as long as more groups are joined
	for(i = 1; units > i * i; i++);
	i *= i;
	sqLeg = (float)range * i / units;
	sqLeg *= sqLeg;
	mergeTask->range = sqrt(sqLeg + sqLeg);
	//mergeTask->range = 200.0f;

	LOG_II((*mergeTask))

	activeMergeTasks[mergeTask->key] = mergeTask;
	activeTasks[mergeTask->key] = mergeTask;
	for (j = groups.begin(); j != groups.end(); j++) {
		if (!ai->pathfinder->addGroup(*(j->second))) {
			mergeTask->remove();
			break;
		}
	}

	if (j == groups.end())
		mergeTask->active = true;
}

void CTaskHandler::MergeTask::remove() {
	LOG_II("MergeTask::remove " << (*this))
	std::map<int,CGroup*>::iterator g;
	for (g = groups.begin(); g != groups.end(); g++) {
		g->second->unreg(*this);
		g->second->busy = false;
		g->second->micro(false);
		g->second->abilities(false);
		ai->pathfinder->remove(*(g->second));
	}
	
	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); j++)
		(*j)->remove(*this);

	active = false;
}

// called on Group removing
void CTaskHandler::MergeTask::remove(ARegistrar &group) {
	groups.erase(group.key);
	group.unreg(*this);
	// TODO: cleanup other data of group?
}
		
void CTaskHandler::MergeTask::update() {
	CScopedTimer t(std::string("tasks-merge"));
	std::vector<CGroup*> mergable;

	/* See which groups can be merged already */
	std::map<int,CGroup*>::iterator g;
	for (g = groups.begin(); g != groups.end(); g++) {
		CGroup *group = g->second;
		if (pos.distance2D(group->pos()) <= range) {
			mergable.push_back(group);
			//ai->pathfinder->remove(*group);
		}
	}
	
	/* We have at least two groups, now we can merge */
	if (mergable.size() >= 2) {
		CGroup *alpha = mergable[0];
		for (unsigned j = 1; j < mergable.size(); j++) {
			LOG_II("MergeTask::update merging " << (*mergable[j]) << " with " << (*alpha))
			alpha->merge(*mergable[j]);
		}
	}

	/* If only one (or none) group remains, merging is no longer possible,
	 * remove the task, unreg groups 
	 */
	if (groups.size() <= 1)
		remove();
}
