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

/**************************************************************/
/************************* ATASK ******************************/
/**************************************************************/
int ATask::counter = 0;

void ATask::remove() {
	LOG_II("ATask::remove " << (*this))

	// NOTE: removal order below is VERY important

	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); ++j)
		// remove current task from CTaskHandler, so it will mark this task 
		// to be killed on next update
		(*j)->remove(*this);

	// remove all assisting tasks...
	std::list<ATask*>::iterator i = assisters.begin();
	while(i != assisters.end()) {
		ATask *task = *i; ++i;
		task->remove();
	}
	assert(assisters.size() == 0);

	if (group) {
		// remove from group...
		group->unreg(*this);
		group->busy = false;
		group->unwait();
		group->micro(false);
		group->abilities(false);
		if (isMoving)
			group->stop();
		group = NULL;
	}

	active = false;
}

// called on Group removing
void ATask::remove(ARegistrar &group) {
	LOG_II("ATask::remove by " << (*(dynamic_cast<CGroup*>(&group))))

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

bool ATask::enemyScan() {
	bool scout = group->cats&SCOUTER;
	bool aircraft = group->cats&AIR;
	TargetsFilter tf;

	if (scout) {
		tf.threatCeiling = 1.1f;
		tf.threatRadius = 300.0f;
	}
	else {
		if (aircraft) {
			if (group->cats&ASSAULT)
				tf.threatCeiling = group->strength;
			else
				tf.threatCeiling = 1.1f;
			// TODO: replace with maneuvering radius?
			tf.threatRadius = 300.0f;
		}
		else {
			tf.exclude = SCOUTER;
			tf.threatFactor = 0.001f;
			tf.threatCeiling = group->strength;
			tf.threatRadius = 0.0f;
		}
	}

	// do not chase after aircraft by non-aircraft groups...
	if (!aircraft)
		tf.exclude |= AIR;

	int target = group->selectTarget(group->getScanRange(), tf);

	if (target >= 0) {
		group->attack(target);
		group->micro(true);
		if (scout)
			LOG_II("ATask::enemyScan scout " << (*group) << " is microing enemy target Unit(" << target << ")")
		else
			LOG_II("ATask::enemyScan engage " << (*group) << " is microing enemy target Unit(" << target << ")")
		return true;
	}

	return false;
}

bool ATask::resourceScan() {
	bool isFeature = true;
	int bestFeature = -1;
	float bestDist = std::numeric_limits<float>::max();
	// NOTE: do not use group->los because it is too small and do not 
	// correspond to real map units
	float radius = group->buildRange;
	float3 gpos = group->pos();

	assert(radius > EPS);

	// reclaim features when we can store metal only...
	if (!ai->economy->mexceeding) {
		const int numFeatures = ai->cb->GetFeatures(&ai->unitIDs[0], MAX_FEATURES, gpos, 1.5f * radius);
		for (int i = 0; i < numFeatures; i++) {
			const int uid = ai->unitIDs[i];
			const FeatureDef *fd = ai->cb->GetFeatureDef(uid);
			if (fd->metal > 0.0f) {
				float3 fpos = ai->cb->GetFeaturePos(uid);
				float dist = gpos.distance2D(fpos);
				if (dist < bestDist) {
					bestFeature = uid;
					bestDist = dist;
				}
			}
		}
	}

	// if there is no feature available then reclaim enemy unarmed building, 
	// hehe :)
	if (bestFeature == -1) {
		std::map<int, bool> occupied;
		TargetsFilter tf;
		tf.include = STATIC;
		tf.exclude = ATTACKER;
		tf.threatCeiling = 1.1f;
		tf.threatRadius = radius;
		
		bestFeature = group->selectTarget(radius, tf);
		
		isFeature = false;
	}			
	
	if (bestFeature != -1) {
		group->reclaim(bestFeature, isFeature);
		group->micro(true);
		LOG_II("ATask::resourceScan group " << (*group) << " is reclaiming")
		return true;
	}

	return false;
}

bool ATask::repairScan() {
	//PROFILE(tasks-repairscan)
	
	if (ai->economy->mstall || ai->economy->estall)
		return false;
	
	int bestUnit = -1;
	float bestScore = 0.0f;
	float radius = group->buildRange;
	float3 gpos = group->pos();

	const int numUnits = ai->cb->GetFriendlyUnits(&ai->unitIDs[0], gpos, 2.0f * radius, MAX_FEATURES);
	for (int i = 0; i < numUnits; i++) {
		const int uid = ai->unitIDs[i];
		
		if (ai->cb->UnitBeingBuilt(uid) || group->firstUnit()->key == uid)
			continue;
		
		const float healthDamage = ai->cb->GetUnitMaxHealth(uid) - ai->cb->GetUnitHealth(uid);
		if (healthDamage > EPS) {
			// TODO: somehow limit number of repairing builders per unit
			const UnitDef *ud = ai->cb->GetUnitDef(uid);
			const float score = healthDamage + (CUnit::isDefense(ud) ? 10000.0f: 0.0f) + (CUnit::isStatic(ud) ? 5000.0f: 0.0f);
			if (score > bestScore) {
				bestUnit = uid;
				bestScore = score;
			}
		}
	}

	if (bestUnit != -1) {
		group->repair(bestUnit);
		group->micro(true);
		LOG_II("ATask::repairScan group " << (*group) << " is repairing")
		return true;
	}

	return false;
}

int ATask::lifeFrames() const {
	return ai->cb->GetCurrentFrame() - bornFrame;
}

float ATask::lifeTime() const {
	return (float)(ai->cb->GetCurrentFrame() - bornFrame) / 30.0f;
}

void ATask::update() {
	if (!active) return;

	// validate task while moving to goal...
	if (validateInterval > 0 && isMoving) {
		int lifetime = lifeFrames();
		if (lifetime >= nextValidateFrame) {
			if (!validate())
				remove();
			else
				nextValidateFrame = lifetime + validateInterval;
		}
	}
}

std::ostream& operator<<(std::ostream &out, const ATask &atask) {
	std::stringstream ss;
	switch(atask.t) {
		case BUILD: {
			const CTaskHandler::BuildTask *task = dynamic_cast<const CTaskHandler::BuildTask*>(&atask);
			ss << "BuildTask(" << task->key << ") " << CTaskHandler::buildStr[task->bt];
			ss << "(" << task->toBuild->def->humanName << ") ETA(" << task->eta << ")";
			ss << " timer("<<task->lifeFrames()<<") "<<(*(task->group));
		} break;

		case ASSIST: {
			const CTaskHandler::AssistTask *task = dynamic_cast<const CTaskHandler::AssistTask*>(&atask);
			ss << "AssistTask(" << task->key << ") assisting Task(" << (task->assist->key) << ") ";
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
			for (i = task->groups.begin(); i != task->groups.end(); ++i) {
				ss << (*(i->second)) << " ";
			}
			ss << "}";
		} break;

		default: return out;
	}

	if (atask.t != ASSIST && atask.t != MERGE) {
		ss << " Assisters: amount(" << atask.assisters.size() << ") [";
		std::list<ATask*>::const_iterator i;
		for (i = atask.assisters.begin(); i != atask.assisters.end(); ++i)
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

	statsMaxTasks = 0;
	statsMaxActiveTasks = 0;

}

CTaskHandler::~CTaskHandler() {
	LOG_II("CTaskHandler::Stats MaxTasks = " << statsMaxTasks)
	LOG_II("CTaskHandler::Stats MaxActiveTasks = " << statsMaxActiveTasks)
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
	int numActiveTasks = 0;
	std::map<int, ATask*>::iterator i;
	for (i = activeTasks.begin(); i != activeTasks.end(); ++i) {
		if (i->second->active) {
			i->second->update();
			numActiveTasks++;
		}
	}

	statsMaxTasks = std::max<int>(statsMaxTasks, activeTasks.size());
	statsMaxActiveTasks = std::max<int>(statsMaxActiveTasks, numActiveTasks);
}

float3 CTaskHandler::getPos(CGroup &group) {
	return groupToTask[group.key]->pos;
}

ATask* CTaskHandler::getTask(CGroup &group) {
	std::map<int, ATask*>::iterator i = groupToTask.find(group.key);
	if (i == groupToTask.end())
		return NULL;
	return i->second;
}

ATask* CTaskHandler::getTaskByTarget(int uid) {
	std::map<int, AttackTask*>::iterator i;
	for (i = activeAttackTasks.begin(); i != activeAttackTasks.end(); ++i) {
		if (i->second->target == uid)
			return i->second;
	}
	return NULL;
}

/**************************************************************/
/************************* BUILD TASK *************************/
/**************************************************************/
void CTaskHandler::addBuildTask(buildType build, UnitType *toBuild, CGroup &group, float3 &pos) {
	if (ai->pathfinder->getClosestNode(pos, &group) == NULL)
		return;

	BuildTask *buildTask = new BuildTask(ai);
	buildTask->pos       = pos;
	buildTask->bt        = build;
	buildTask->toBuild   = toBuild;
	buildTask->eta       = int((ai->pathfinder->getETA(group, pos) + 100) * 1.3f);
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

bool CTaskHandler::BuildTask::validate() {
	if (toBuild->cats&MEXTRACTOR) {
		int numUnits = ai->cb->GetFriendlyUnits(&ai->unitIDs[0], pos, 1.1f * ai->cb->GetExtractorRadius());
		for (int i = 0; i < numUnits; i++) {
			const int uid = ai->unitIDs[i];
			const UnitDef *ud = ai->cb->GetUnitDef(uid);
			if (UC(ud->id)&MEXTRACTOR) {
				return false;
			}
		}
	}
	return true;
}

void CTaskHandler::BuildTask::update() {
	//PROFILE(tasks-build)
	
	ATask::update();

	if (!active) return;
	
	float3 gpos = group->pos();

	if (group->isMicroing()) {
		if (group->isIdle())
			group->micro(false); // if idle, our micro is done
		else
			return; // if microing, break
	}

	/* See if we can build yet */
	if (isMoving) {
		if (gpos.distance2D(pos) <= group->buildRange) {
			group->build(pos, toBuild);
			isMoving = false;
			ai->pathfinder->remove(*group);
		}
		/* See if we can suck wreckages */
		else if (!group->isMicroing()) {
			// TODO: increase ETA on success
			if (!resourceScan())
				repairScan();
		}
	}

	/* We are building or blocked */
	if (!isMoving) { 
		if (ai->economy->hasFinishedBuilding(*group))
			remove();
		else if (lifeFrames() > eta && !ai->economy->hasBegunBuilding(*group)) {
			LOG_WW("BuildTask::update assuming buildpos blocked for group " << (*group))
			remove();
		}
	}
}

bool CTaskHandler::BuildTask::assistable(CGroup &assister, float &travelTime) {
	if ((bt == BUILD_AG_DEFENSE && assisters.size() >= 2) || isMoving)
		return false;
	
	float buildSpeed = group->buildSpeed;
	std::list<ATask*>::iterator it;
	for (it = assisters.begin(); it != assisters.end(); ++it)
		buildSpeed += (*it)->group->buildSpeed;

	float3 gpos = group->pos();
	//float3 gpos = pos;
	float buildTime = (toBuild->def->buildTime / buildSpeed) * 32.0f;
	
	/* travelTime + 5 seconds to make it worth the trip */
	travelTime = ai->pathfinder->getETA(assister, gpos) + 30 * 5;	

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
	factoryTask->pos = group.firstUnit()->pos();
	// TODO: rethink this when implementing units like Consul
	// NOTE: set isMoveing to true to enable task validating
	factoryTask->isMoving = true;
	factoryTask->reg(*this); // register task in a task handler
	factoryTask->addGroup(group);
	factoryTask->validateInterval = 10 * 30;

	activeFactoryTasks[factoryTask->key] = factoryTask;
	activeTasks[factoryTask->key] = factoryTask;
	groupToTask[factoryTask->key] = factoryTask;

	LOG_II((*factoryTask))

	factoryTask->active = true;
}

bool CTaskHandler::FactoryTask::assistable(CGroup &assister) {
	if (assisters.size() >= std::min(ai->cfgparser->getState() * 2, FACTORY_ASSISTERS) || 
		!group->firstUnit()->def->canBeAssisted) {
		return false;
	}
	else {
		// SLOGIC: why we need to push a new builder? First of all, this breaks
		// builder limit set in config file. Secondly, assisting task involves
		// a builder which has nothing to do better. Right? So, commented out 
		// by now.
		//ai->wishlist->push(BUILDER, HIGH);
		return true;
	}
}

bool CTaskHandler::FactoryTask::validate() {
	int numUnits = ai->cb->GetFriendlyUnits(&ai->unitIDs[0], pos, 16.0f);
	if (numUnits > 0) {
		for (int i = 0; i < numUnits; i++) {
    		int uid = ai->unitIDs[i];
    		
    		if (group->firstUnit()->key == uid)
    			continue;
    		
    		if (!ai->cb->UnitBeingBuilt(uid)) {
    			CUnit *unit = ai->unittable->getUnit(uid);
    			if (unit) {
    				if (ai->unittable->canPerformTask(*unit))
    					// our unit stalled a factory
    					return false;
    			}
    			else
    				// some stupid allied unit stalled out factory
    				return false;
    		}
		}
	}
	return true;
}

void CTaskHandler::FactoryTask::update() {
	//PROFILE(tasks-factory)
	
	ATask::update();

	if (!active) return;

	std::map<int,CUnit*>::iterator i;
	CUnit *factory;
	
	for(i = group->units.begin(); i != group->units.end(); ++i) {
		factory = i->second;
		if (ai->unittable->idle[factory->key] && !ai->wishlist->empty(factory->key)) {
			Wish w = ai->wishlist->top(factory->key);
			ai->wishlist->pop(factory->key);
			if (factory->factoryBuild(w.ut)) {
				ai->unittable->unitsBuilding[factory->key] = w;
			}
		}
	}
}

void CTaskHandler::FactoryTask::setWait(bool on) {
	std::map<int,CUnit*>::iterator ui;
	std::list<ATask*>::iterator ti;
	CUnit *factory;

	for (ui = group->units.begin(); ui != group->units.end(); ++ui) {
		factory = ui->second;
		if(on)
			factory->wait();
		else
			factory->unwait();
	}

	for (ti = assisters.begin(); ti != assisters.end(); ++ti) {
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
	if (ai->pathfinder->getClosestNode(toAssist.pos, &group) == NULL)
		return;
	
	AssistTask *assistTask = new AssistTask(ai);
	assistTask->assist     = &toAssist;
	assistTask->pos        = toAssist.pos;
	assistTask->addGroup(group);
	assistTask->reg(*this); // register task in a task handler
	assistTask->validateInterval = 0;

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
	LOG_II("AssistTask::remove by Group(" << group.key << ")")
	
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
	//PROFILE(tasks-assist)
	
	ATask::update();

	if (!active) return;
	
	if (group->isMicroing() && group->isIdle())
		group->micro(false);

	if (isMoving) {
		pos = assist->pos; // because task target could be mobile

		float3 grouppos = group->pos();
		float3 dist = grouppos - pos;
		float range = group->getRange();

		if (dist.Length2D() <= range) {
			group->assist(*assist);
			ai->pathfinder->remove(*group);
			isMoving = false;
			group->micro(true);
		} 
	}

	if (!group->isMicroing()) {
		if (group->cats&BUILDER)
			resourceScan(); // builders should not be too aggressive
		else if (!(group->cats&AIR)) {
			enemyScan();
		}
	}
}

/**************************************************************/
/************************* ATTACK TASK ************************/
/**************************************************************/
bool CTaskHandler::addAttackTask(int target, CGroup &group, bool urgent) {
	const UnitDef *ud = ai->cbc->GetUnitDef(target);
	if (ud == NULL)
		return false;

	float3 pos = ai->cbc->GetUnitPos(target);
	if (ai->pathfinder->getClosestNode(pos, &group) == NULL)
		return false;

	AttackTask *attackTask = new AttackTask(ai);
	attackTask->target     = target;
	attackTask->pos        = pos;
	attackTask->urgent     = urgent;
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

	return attackTask->active;
}

bool CTaskHandler::AttackTask::validate() {
	bool scoutGroup = group->cats&SCOUTER;
	float targetDistance = pos.distance2D(group->pos());
	
	// don't chase scout groups too long if they are out of base...
	if (!scoutGroup && lifeTime() > 20.0f) {
		const UnitDef *eud = ai->cbc->GetUnitDef(target);
		if (eud) {
			const unsigned int ecats = UC(eud->id);
			if (ecats&SCOUTER && !ai->defensematrix->isPosInBounds(pos))
				return false; 
		}
	}
	
	if (targetDistance > 1000.0f)
		return true; // too far to panic

	if (ai->cbc->IsUnitCloaked(target))
		return false;
	
	bool isBaseThreat = ai->defensematrix->isPosInBounds(pos);

	// if there is no threat to our base then prevent useless attack for groups
	// which are not too cheap
	if (!isBaseThreat && (group->costMetal / group->units.size()) >= 100.0f) {
		float threatRange;
		if (scoutGroup)
			threatRange = 300.0f;
		else
			threatRange = 0.0f;

		if (group->getThreat(pos, threatRange) > group->strength)
			return false;
	}

	return true;
}

void CTaskHandler::AttackTask::update() {
	//PROFILE(tasks-attack)
	
	ATask::update();

	if (!active) return;

	if (group->isMicroing() && group->isIdle())
		group->micro(false);

	/* If the target is destroyed, remove the task, unreg groups */
	// TODO: move this check to validate() ?
	if (ai->cbc->GetUnitHealth(target) <= 0.0f) {
		remove();
		return;
	}

	if (isMoving) {
		/* Keep tracking the target */
		pos = ai->cbc->GetUnitPos(target);
	
		float range = group->getRange();
		float3 gpos = group->pos();

		/* See if we can attack our target already */
		if (gpos.distance2D(pos) <= range) {
			if (group->cats&BUILDER)
				group->reclaim(target);
			else
				group->attack(target);
			isMoving = false;
			ai->pathfinder->remove(*group);
			group->micro(true);
		}
	}
	
	/* See if we can attack a target we found on our path */
	if (!(group->isMicroing() || urgent)) {
		if (group->cats&BUILDER)
			resourceScan(); // builders should not be too aggressive
		else
			enemyScan();
	}
}

/**************************************************************/
/************************* MERGE TASK *************************/
/**************************************************************/
void CTaskHandler::addMergeTask(std::map<int,CGroup*> &groups) {
	//int masterGroup = -1;
	float range = 0.0f;
	std::map<int,CGroup*>::iterator j;
	float minSlope = std::numeric_limits<float>::max();
	float maxPower = std::numeric_limits<float>::min();
	
	MergeTask *mergeTask = new MergeTask(ai);
	mergeTask->groups = groups;
	mergeTask->pos = float3(0.0f, 0.0f, 0.0f);
	mergeTask->reg(*this); // register task in a task handler
	//mergeTask->validateInterval = 0;
	
	for (j = groups.begin(); j != groups.end(); ++j) {
		j->second->reg(*mergeTask);
		j->second->busy = true;
		j->second->micro(false);
		j->second->abilities(true);
		groupToTask[j->first] = mergeTask;
		range += j->second->radius();
		/*
		if (j->second->maxSlope < (minSlope + EPS) && maxPower < j->second->strength) {
			masterGroup = j->second->key;
			minSlope = j->second->maxSlope;
			maxPower = j->second->strength;
			mergeTask->pos = j->second->pos(true);
		}
		*/
	}
	
	unsigned int cats = groups.begin()->second->cats;
	if ((cats&AIR) && !(cats&ASSAULT)) {
		mergeTask->range = 600.0f;
	}
	else {
		mergeTask->range = range + groups.size() * FOOTPRINT2REAL;
	}

	LOG_II((*mergeTask))

	activeMergeTasks[mergeTask->key] = mergeTask;
	activeTasks[mergeTask->key] = mergeTask;
	
	/*
	for (j = groups.begin(); j != groups.end(); j++) {
		//if (j->second->key == masterGroup)
		//	continue; // this group is already in place
		if (!ai->pathfinder->addGroup(*(j->second))) {
			mergeTask->remove();
			break;
		}
	}
	
	if (j == groups.end())
		mergeTask->active = true;
	*/
	
	mergeTask->active = mergeTask->reelectMasterGroup();
	if (!mergeTask->active)
		mergeTask->remove();
}

void CTaskHandler::MergeTask::remove() {
	LOG_II("MergeTask::remove " << (*this))
	
	std::map<int, CGroup*>::iterator g;

	for (g = groups.begin(); g != groups.end(); ++g) {
		g->second->unreg(*this);
		g->second->busy = false;
		g->second->micro(false);
		g->second->abilities(false);
		ai->pathfinder->remove(*(g->second));
		// TODO: remove from taskhandler->groupToTask
	}
	
	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); ++j)
		// remove current task from CTaskHandler, so it will mark this task 
		// to be killed on next update
		(*j)->remove(*this);

	group = NULL;

	active = false;
}

// called on Group removing
void CTaskHandler::MergeTask::remove(ARegistrar &g) {
	bool isReelectionNeeded = (group && group->key == g.key);
	
	mergable.erase(g.key);
	groups.erase(g.key);
	g.unreg(*this);
	// TODO: remove from taskhandler->groupToTask

	if (isReelectionNeeded) {
		group = NULL;
		reelectMasterGroup();
	}
}
		
bool CTaskHandler::MergeTask::validate() {
	return reelectMasterGroup();
}

void CTaskHandler::MergeTask::update() {
	//PROFILE(tasks-merge)
	
	ATask::update();

	if (!active) return;

	/* See which groups can be merged already */
	std::map<int,CGroup*>::iterator it;
	for (it = groups.begin(); it != groups.end(); ++it) {
		CGroup *g = it->second;
		
		if (g->isMicroing())
			continue;
		
		if (pos.distance2D(g->pos()) < range) {
			mergable[g->key] = true;
			g->micro(true);
		}
	}
	
	/* We have at least two groups, now we can merge */
	if (mergable.size() >= 2) {
		std::vector<int> keys;
		std::map<int, bool>::iterator it;
		
		// get keys because while merging "mergable" is reducing...
		for (it = mergable.begin(); it != mergable.end(); ++it) {
			keys.push_back(it->first);
		}
		
		for (int i = 0; i < keys.size(); i++) {
			int key = keys[i];
			if (key != group->key) {
				LOG_II("MergeTask::update merging " << (*groups[key]) << " with " << (*group))
				// NOTE: group being merged is automatically removed
				group->merge(*groups[key]);
			}
		}
		
		assert(mergable.size() == 1);
		mergable.clear();
		group->micro(false);
	}

	// if only one (or none) group remains, merging is no longer possible,
	// remove the task, unreg groups...
	if (groups.size() <= 1)
		remove();
}

bool CTaskHandler::MergeTask::reelectMasterGroup() {
	bool reelect = true;
	
	if (groups.size() <= 1)
		return false;

	if (group && !isRetreating) {
		float threat = group->getThreat(pos, group->radius());
		// if threat is 2x smaller than group strength then do nothing
		if (threat <= EPS || (group->strength / threat) > 2.0f)
			reelect = false;
	}
	
	if (reelect) {
		float minThreat = std::numeric_limits<float>::max();
		float maxDistance = std::numeric_limits<float>::min();
		CGroup *bestGroup = NULL;
		std::map<int,CGroup*>::iterator it;

		for (it = groups.begin(); it != groups.end(); ++it) {
			CGroup *g = it->second;
			float3 gpos = g->pos();
			float threat = g->getThreat(gpos, g->radius());
			float distance = ai->defensematrix->distance2D(gpos);

			if (distance > maxDistance)
				maxDistance = distance;

			if (threat < minThreat) {
				bestGroup = g;
				minThreat = threat;
				isRetreating = (distance + EPS) < maxDistance;
			}
		}

		if (bestGroup && (group == NULL || group->key != bestGroup->key)) {
			group = bestGroup;
			pos = bestGroup->pos(true);
			for (it = groups.begin(); it != groups.end(); ++it) {
				CGroup *g = it->second;
				ai->pathfinder->remove(*g);
				if (!ai->pathfinder->addGroup(*g))
					return false;
			}
		}
	}
	
	return (group != NULL);
}
