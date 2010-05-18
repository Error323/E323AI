#include "CGroup.h"

#include <sstream>
#include <iostream>
#include <string>

#include "CAI.h"
#include "CUnit.h"
#include "CUnitTable.h"
#include "CTaskHandler.h"
#include "CPathfinder.h"
#include "CDefenseMatrix.h"

int CGroup::counter = 0;

void CGroup::addUnit(CUnit &unit) {
	if (unit.group) {
		if (unit.group == this) {
			LOG_WW("CGroup::addUnit " << unit << " already registered in " << (*this))
			return; // already registered
		} else {
			// NOTE: unit can exist in one and only group
			//LOG_II("CGroup::addUnit " << unit << " still in " << (*(unit.group)))
			unit.group->remove(unit);
		}
	}
	
	assert(unit.group == NULL);
	
	units[unit.key] = &unit;
	unit.reg(*this);
	unit.group = this;
	
	recalcProperties(&unit);

	LOG_II("CGroup::addUnit " << unit << " to " << (*this))
	// TODO: if group is busy invoke new unit to community process?
}

void CGroup::remove() {
	LOG_II("CGroup::remove " << (*this))

	// NOTE: removal order below is important
		
	std::list<ARegistrar*>::iterator j = records.begin();
	while(j != records.end()) {
		ARegistrar *regobj = *j; j++;
		// remove from CEconomy, CPathfinder, ATask
		regobj->remove(*this);
	}
	
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		i->second->unreg(*this);
		i->second->group = NULL;
	}
	units.clear();
	badTargets.clear();

	assert(records.empty());
}

void CGroup::remove(ARegistrar &object) {
	CUnit *unit = dynamic_cast<CUnit*>(&object);
	
	LOG_II("CGroup::remove " << (*unit) << " from " << (*this))

	assert(units.find(unit->key) != units.end());
	
	unit->group = NULL;
	unit->unreg(*this);
	units.erase(unit->key);

	badTargets.clear();

	/* If no more units remain in this group, remove the group */
	if (units.empty()) {
		remove();
	} else {
		/* Recalculate properties of the current group */
		
		recalcProperties(NULL, true);
		std::map<int, CUnit*>::iterator i;
		for (i = units.begin(); i != units.end(); i++) {
			recalcProperties(i->second);
		}
	}
}

void CGroup::reclaim(int entity, bool feature) {
	float3 pos;
	
	if (feature) {
		pos = ai->cb->GetFeaturePos(entity);
		if (pos == ZeroVector)
			return;
	}
		
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		if (i->second->def->canReclaim) {
			if (feature)
				i->second->reclaim(pos, 16.0f);
			else
				i->second->reclaim(entity);
		}
	}
}

void CGroup::repair(int target) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		if (i->second->def->canRepair)
			i->second->repair(target);
	}
}

void CGroup::abilities(bool on) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		if (i->second->def->canCloak)
			i->second->cloak(on);
	}
}

void CGroup::micro(bool on) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->micro(on);
}

bool CGroup::isMicroing() {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		if (i->second->isMicroing())
			return true;
	}
	return false;
}

bool CGroup::isIdle() {
	bool idle = true;
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		if (!ai->unittable->idle[i->second->key]) {
			idle = false;
			break;
		}
	}
	return idle;
}

void CGroup::reset() {
	LOG_II("CGroup::reset " << (*this))
	assert(units.empty());
	recalcProperties(NULL, true);
	busy = false;
	micro(false);
	abilities(false);
	units.clear();
	records.clear();
	badTargets.clear();
}

void CGroup::recalcProperties(CUnit *unit, bool reset)
{
	if(reset) {
		strength   = 0.0f;
		speed      = std::numeric_limits<float>::max();
		size       = 0;
		buildSpeed = 0.0f;
		range      = 0.0f;
		buildRange = 0.0f;
		los        = 0.0f;
		maxSlope   = 1.0f;
		pathType   = -1; // emulate NONE
		techlvl    = TECH1;
		cats       = 0;
		groupRadius     = 0.0f;
		radiusUpdateRequied = false;
		cost       = 0.0f;
		costMetal  = 0.0f;
	}

	if(unit == NULL)
		return;

    if (unit->builtBy >= 0) {
		techlvl = std::max<int>(techlvl, unit->techlvl);
	}

	// NOTE: aircraft & static units do not have movedata
	MoveData *md = unit->def->movedata;
    if (md) {
		// select base path type with the lowerst slope, so pos(true) will
		// return valid postition for all units in a group...
		if (md->maxSlope <= maxSlope) {
			pathType = md->pathType;
			maxSlope = md->maxSlope;
		}
	}
		
	strength += unit->type->dps;
	buildSpeed += unit->def->buildSpeed;
	size += FOOTPRINT2REAL * std::max<int>(unit->def->xsize, unit->def->zsize);
	range = std::max<float>(ai->cb->GetUnitMaxRange(unit->key), range);
	buildRange = std::max<float>(unit->def->buildDistance, buildRange);
	speed = std::min<float>(ai->cb->GetUnitSpeed(unit->key), speed);
	los = std::max<float>(unit->def->losRadius, los);
	cost += unit->type->cost;
	costMetal += unit->type->costMetal;
	
	mergeCats(unit->type->cats);

	radiusUpdateRequied = true;
}

void CGroup::merge(CGroup &group) {
	std::map<int, CUnit*>::iterator i = group.units.begin();
	// NOTE: "group" will automatically be removed when last unit is transferred
	while(i != group.units.end()) {
		CUnit *unit = i->second; i++;
		assert(unit->group == &group);
		addUnit(*unit);
	}
}

float3 CGroup::pos(bool force_valid) {
	std::map<int, CUnit*>::iterator i;
	float3 pos(0.0f, 0.0f, 0.0f);

	for (i = units.begin(); i != units.end(); i++)
		pos += ai->cb->GetUnitPos(i->first);

	pos /= units.size();

	if (force_valid) {
		if (ai->pathfinder->isBlocked(pos.x, pos.z, pathType)) {
			float3 posValid = ai->pathfinder->getClosestPos(pos, this);
			if (posValid == ERRORVECTOR) {
				float bestDistance = std::numeric_limits<float>::max();
				for (i = units.begin(); i != units.end(); i++) {
					float3 pos2 = ai->cb->GetUnitPos(i->first);
					if (ai->pathfinder->isBlocked(pos2.x, pos2.z, pathType))
						pos2 = ai->pathfinder->getClosestPos(pos2, this);
					if (pos2 != ERRORVECTOR) {
						float distance = pos.distance2D(pos2);
						if (distance < bestDistance) {
							posValid = pos2;
							bestDistance = distance;
						}
					}

				}
			}
			return posValid;
		}
	}

	return pos;
}

float CGroup::radius() {
	if (radiusUpdateRequied) {
		int i;
		// get number of units per leg length in a square
		for(i = 1; units.size() > i * i; i++);
		// calculate length of leg of square
		float sqLeg = maxLength() * i / (float)units.size();
		sqLeg *= sqLeg;
		// calculate half of hypotenuse
		groupRadius = sqrt(sqLeg + sqLeg) / 2.0f;
		
		radiusUpdateRequied = false;
	}
	return groupRadius;
}

int CGroup::maxLength() {
	return size + units.size() * FOOTPRINT2REAL;
}

void CGroup::assist(ATask &t) {
	// t->addAssister(
	switch(t.t) {
		case BUILD: {
			CTaskHandler::BuildTask *task = dynamic_cast<CTaskHandler::BuildTask*>(&t);
			CUnit *unit = task->group->firstUnit();
			guard(unit->key);
			break;
		}

		case ATTACK: {
			// TODO: Calculate the flanking pos and attack from there
			CTaskHandler::AttackTask *task = dynamic_cast<CTaskHandler::AttackTask*>(&t);
			attack(task->target);
			break;
		}

		case FACTORY_BUILD: {
			CTaskHandler::FactoryTask *task = dynamic_cast<CTaskHandler::FactoryTask*>(&t);
			CUnit *unit = task->group->firstUnit();
			guard(unit->key);
			break;
		}

		default: return;
	}
}

void CGroup::move(float3 &pos, bool enqueue) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->move(pos, enqueue);
}

void CGroup::wait() {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->wait();
}

void CGroup::unwait() {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->unwait();
}

void CGroup::attack(int target, bool enqueue) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->attack(target, enqueue);
}

void CGroup::build(float3 &pos, UnitType *ut) {
	std::map<int, CUnit*>::iterator alpha, i;
	alpha = units.begin();
	if (alpha->second->build(ut, pos)) {
		for (i = ++alpha; i != units.end(); i++)
			i->second->guard(alpha->first);
	}
}

void CGroup::stop() {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->stop();
	ai->pathfinder->remove(*this);
}

void CGroup::guard(int target, bool enqueue) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->guard(target, enqueue);
}

bool CGroup::canTouch(const float3 &goal) {
	return !ai->pathfinder->isBlocked(goal.x, goal.z, pathType);
}

bool CGroup::canReach(const float3 &goal) {
	if (!canTouch(goal))
		return false;
	if (pathType < 0)
		return true;

	float3 gpos = pos(true);

	return ai->pathfinder->pathExists(*this, gpos, goal);
}

bool CGroup::canAttack(int uid) {
	if (!(cats&ATTACKER) && !firstUnit()->def->canReclaim)
		return false;
	
	const UnitDef *ud = ai->cbc->GetUnitDef(uid);
	
	if (ud == NULL || ai->cbc->IsUnitCloaked(uid))
		return false;
	
	const unsigned int ecats = UC(ud->id);
	float3 epos = ai->cbc->GetUnitPos(uid);
	
	if ((ecats&AIR) && !(cats&ANTIAIR))
		return false;
	/*
	if ((ecats&SUBMARINE) && !(cats&TORPEDO))
		return false;
	*/
	if (epos.y < 0.0f && (cats&(LAND|AIR)) /*&& !(cats&TORPEDO)*/)
		return false;
	
	if ((ecats&LAND) && pos().y < 0.0f)
		return false;

	// TODO: add more tweaks based on physical weapon possibilities

	return true;
}

bool CGroup::canAdd(CUnit *unit) {
	// TODO: ?
	return true;
}
		
bool CGroup::canMerge(CGroup *group) {
	static unsigned int nonMergableCats[] = {SEA, LAND, AIR, HOVER, ATTACKER, STATIC, MOBILE, BUILDER, SCOUTER};

	unsigned int c = cats&group->cats;
	
	if (c == 0)
		return false;
	
	for (int i = 0; i < sizeof(nonMergableCats) / sizeof(unsigned int); i++) {
		unsigned int tag = nonMergableCats[i];
		if ((cats&tag) && !(c&tag))
			return false;
	}

	if (!(cats&SCOUTER) && (group->cats&SCOUTER)) {
		// attempt to add scouter to non-scout group
		static unsigned int attackCats = ANTIAIR|ARTILLERY|SNIPER|ASSAULT;
		if (!(c&attackCats))
			return false;
	}

	// NOTE: aircraft units have more restricted merge rules
	// TODO: refactor with introducing Group behaviour property?
	if (cats&AIR) {
		if ((cats&ASSAULT) && !(c&ASSAULT))
			return false;
		if ((cats&ARTILLERY) && !(c&ARTILLERY))
			return false;
	}
	
	return true;
}

CUnit* CGroup::firstUnit() {
	if (units.empty())
		return NULL;
	return units.begin()->second;
}

void CGroup::mergeCats(unsigned int newcats) {
	if (cats == 0)
		cats = newcats;
	else {
		static unsigned int nonXorCats[] = {SEA, LAND, AIR, STATIC, MOBILE, SCOUTER};
		unsigned int oldcats = cats;
		cats |= newcats;
		for (int i = 0; i < sizeof(nonXorCats) / sizeof(unsigned int); i++) {
			unsigned int tag = nonXorCats[i];
			if (!(oldcats&tag) && (cats&tag))
				cats &= ~tag;
		}
	}
}

float CGroup::getThreat(float3 &target, float radius) {
	return ai->threatmap->getThreat(target, radius, this);
}

bool CGroup::addBadTarget(int id) {
	const UnitDef *ud = ai->cbc->GetUnitDef(id);
	if (ud == NULL)
		return false;
	
	LOG_WW("CGroup::addBadTarget " << ud->humanName << "(" << id << ") to " << (*this))
	
	const unsigned int ecats = UC(ud->id);
	if (ecats&STATIC)
		badTargets[id] = -1;
	else
		badTargets[id] = ai->cb->GetCurrentFrame();

	return true;
}

int CGroup::selectTarget(std::vector<int> &targets, TargetsFilter &tf) {
	bool scout = cats&SCOUTER;
	bool bomber = !scout && (cats&AIR) && (cats&ARTILLERY);
	int frame = ai->cb->GetCurrentFrame();
	float bestScore = tf.scoreCeiling;
	float unitDamageK;
	float3 gpos = pos();

	for (int i = 0; i < std::min<int>(targets.size(), tf.candidatesLimit); i++) {
		int t = targets[i];

		if (!canAttack(t) || (tf.excludeId && (*(tf.excludeId))[t]))
			continue;
		
		if (badTargets.size() > 0) {
			std::map<int, int>::iterator it = badTargets.find(t);
			if (it != badTargets.end()) {
				if (it->second < 0)
					continue; // permanent bad target
				if ((frame - it->second) < BAD_TARGET_TIMEOUT)
					continue; // temporary bad target
				else {
					badTargets.erase(it->first);
					LOG_II("CGroup::selectTarget bad target Unit(" << t << ") timed out for " << (*this))
				}
			}
		}
		
		const UnitDef *ud = ai->cbc->GetUnitDef(t);
		if (ud == NULL)
			continue;

		const unsigned int ecats = UC(ud->id);
		if (!(tf.include&ecats) || (tf.exclude&ecats))
			continue;
		
		float3 epos = ai->cbc->GetUnitPos(t);
		float threat = getThreat(epos, tf.threatRadius);
		if (threat > tf.threatCeiling)
			continue;
		
		float unitMaxHealth = ai->cbc->GetUnitMaxHealth(t);
		if (unitMaxHealth > EPS)
			unitDamageK = (unitMaxHealth - ai->cbc->GetUnitHealth(t)) / unitMaxHealth;
		else
			unitDamageK = 0.0f;
		
		float score = gpos.distance2D(epos);
		score += (tf.threatFactor * threat) - unitDamageK * 50.0f;
		// TODO: refactor so params below are moved into TargetFilter
		if (ai->defensematrix->isPosInBounds(epos))
			// boost in priority enemy at our base, even scout units
			score -= 1000.0f; // TODO: better change value to the length a group can pass for 1 min (40 sec?)?
		else if(!scout && ecats&SCOUTER) {
			// remote scouts are not interesting for engage groups
			score += 10000.0f;
		}
		
		if (bomber && (ecats&STATIC) && (ecats&ANTIAIR))
			score -= 500.0f;

		// do not allow land units chase after air units...
		if (!(cats&AIR) && (ecats&AIR))
			score += 3000.0f;

		if(score < tf.scoreCeiling) {
			tf.bestTarget = t;
			tf.scoreCeiling = score;
			tf.threatValue = threat;
		}
	}

	return tf.bestTarget;
}

int CGroup::selectTarget(float search_radius, TargetsFilter &tf) {
	int numEnemies = ai->cbc->GetEnemyUnits(&ai->unitIDs[0], pos(), search_radius, std::min<int>(MAX_ENEMIES, tf.candidatesLimit));
	if (numEnemies > 0) {
		tf.candidatesLimit = numEnemies;
		tf.bestTarget = selectTarget(ai->unitIDs, tf);
	}
	return tf.bestTarget;
}

float CGroup::getScanRange() {
	float result = radius();

	if (cats&STATIC)
		result += getRange();
	if (cats&BUILDER)
		result += buildRange * 1.5f;
	else if (cats&SNIPER)
		result += range * 1.05f;
	else if (cats&SCOUTER)
		result += range * 3.0f;
	else if (cats&ATTACKER)
		result += range * 1.4f;
	
	return result;
}

float CGroup::getRange() {
	if (cats&BUILDER)
		return buildRange;
	return range;
}

std::ostream& operator<<(std::ostream &out, const CGroup &group) {
	std::stringstream ss;
	ss << "Group(" << group.key << "):" << " range(" << group.range << "), buildRange(" << group.buildRange << "), los(" << group.los << "), speed(" << group.speed << "), strength(" << group.strength << "), amount(" << group.units.size() << ") [";
	std::map<int, CUnit*>::const_iterator i = group.units.begin();
	for (i = group.units.begin(); i != group.units.end(); i++) {
		ss << (*i->second) << ", ";
	}
	std::string s = ss.str();
	s = s.substr(0, s.size()-2);
	s += "]";
	out << s;
	return out;
}
