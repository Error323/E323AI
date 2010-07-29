#include "ATask.h"

#include "CGroup.h"
#include "CEconomy.h"
#include "CUnit.h"

int ATask::counter = 0;

ATask::ATask(AIClasses *_ai):ARegistrar(++counter, std::string("task")) {
	t = TASK_UNDEFINED;
	ai = _ai;
	active = false;
	suspended = false;
	isMoving = true;
	pos = ZEROVECTOR;
	initFrame = ai->cb->GetCurrentFrame();
	validateInterval = 5 * 30; // 5 sec by default
	nextValidateFrame = validateInterval;
	priority = 5;
	queueID = 0;
}

void ATask::remove() {
	LOG_II("ATask::remove " << (*this))

	// NOTE: removal order below is VERY important

	// remove current task from CTaskHandler, so it will mark this task 
	// to be killed on next update
	std::list<ARegistrar*>::iterator j = records.begin();
	while (j != records.end()) {
		ARegistrar *regobj = *j; ++j;
		regobj->remove(*this);
	}

	// remove all assisting tasks...
	std::list<ATask*>::iterator i = assisters.begin();
	while (i != assisters.end()) {
		ATask *task = *i; ++i;
		task->remove();
	}
	assert(assisters.size() == 0);

	// detach task from groups...
	std::list<CGroup*>::iterator itGroup = groups.begin();
	while(itGroup != groups.end()) {
		CGroup *g = *itGroup; ++itGroup;
		removeGroup(*g);
	}

	active = false;
}

// called on Group removing
void ATask::remove(ARegistrar &group) {
	CGroup *g = dynamic_cast<CGroup*>(&group);
	
	assert(g != NULL);
	
	removeGroup(*g);
	
	if (groups.empty()) {
		LOG_II("ATask::remove " << (*g))

		remove();
	}
}

CGroup* ATask::firstGroup() const {
	if (groups.empty())
		return NULL;
	return groups.front();
}

void ATask::addGroup(CGroup &g) {
	// FIXME: remove this when queue tasks will be supported
	assert(!g.busy);
	/*
	if (g->busy) {
		ATask *task = ai->tasks->getTask(g);
		assert(task != NULL && task != this);
		task->suspended = true;
		// TODO: nextTask = task;
	}
	*/
	g.reg(*this);
	g.busy = true;
	g.micro(false);
	g.abilities(true);
	
	groups.push_back(&g);
}

void ATask::removeGroup(CGroup &g) {
	g.unreg(*this);
	
	if (!suspended) {
		g.busy = false;
		g.unwait();
		g.micro(false);
		g.abilities(false);
		if (isMoving) g.stop();
	}

	groups.remove(&g);
}

bool ATask::enemyScan() {
	CGroup *group = firstGroup();
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
	CGroup *group = firstGroup();
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
	if (ai->economy->mstall || ai->economy->estall)
		return false;
	
	int bestUnit = -1;
	float bestScore = 0.0f;
	CGroup *group = firstGroup();
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
	return ai->cb->GetCurrentFrame() - initFrame;
}

float ATask::lifeTime() const {
	return (float)(ai->cb->GetCurrentFrame() - initFrame) / 30.0f;
}

void ATask::update() {
	if (!active) return;

	if (validateInterval > 0) {
		int lifetime = lifeFrames();
		if (lifetime >= nextValidateFrame) {
			if (!onValidate()) {
				remove();
				return;
			}
			else
				nextValidateFrame = lifetime + validateInterval;
		}
	}

	if (suspended) return;

	onUpdate();
}

std::ostream& operator<<(std::ostream &out, const ATask &atask) {
	atask.toStream(out);

	if (atask.assisters.size() > 0) {
		out << " Assisters: amount(" << atask.assisters.size() << ") [";
		std::list<ATask*>::const_iterator i;
		for (i = atask.assisters.begin(); i != atask.assisters.end(); ++i) {
			CGroup *group = (*i)->firstGroup();
			if (group)
				out << (*group);
		}
		out << "]";
	}
	
	return out;
}
