#include "CGroup.h"

#include <sstream>
#include <iostream>
#include <string>
#include <limits>

#include "CAI.h"
#include "CUnit.h"
#include "CUnitTable.h"
#include "CTaskHandler.h"
#include "CPathfinder.h"

int CGroup::counter = 0;

void CGroup::addUnit(CUnit &unit) {

	if (unit.group) {
		if (unit.group == this) {
			LOG_EE("CGroup::addUnit " << unit << " already registered in " << (*this))
			return; // already registered
		} else {
			// NOTE: unit can only exist in one and only group
			LOG_EE("CGroup::addUnit " << unit << " still in group " << (*(unit.group)))
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
	
	assert(records.empty());

	// TODO: remove next line when prev assertion is never raised
	records.clear();
}

void CGroup::remove(ARegistrar &unit) {
	LOG_II("CGroup::remove Unit(" << unit.key << ") from " << (*this))

	assert(units.find(unit.key) != units.end());
	
	CUnit *unit2 = units[unit.key];
	unit2->group = NULL;
	unit2->unreg(*this);
	units.erase(unit.key);

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
		if (pos == ZEROVECTOR)
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
	recalcProperties(NULL, true);
	busy = false;
	micro(false);
	abilities(false);
	units.clear();
	records.clear();
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
		busy       = false;
		maxSlope   = 1.0f;
		moveType   = -1; // emulate NONE
		techlvl    = 1;
	}

	if(unit == NULL)
		return;

    if (unit->builtBy >= 0) {
		techlvl = std::max<int>(techlvl, unit->techlvl);
	}

	// NOTE: aircraft & static units do not have movedata
	MoveData *md = ai->cb->GetUnitDef(unit->key)->movedata;
    if (md) {
    	if (md->maxSlope <= maxSlope) {
			// TODO: rename moveType into pathType because this is not the same
			moveType = md->pathType;
			maxSlope = md->maxSlope;
		}
	}
		
	strength += ai->cb->GetUnitPower(unit->key);
	buildSpeed += unit->def->buildSpeed;
	size += FOOTPRINT2REAL * std::max<int>(unit->def->xsize, unit->def->zsize);
	range = std::max<float>(ai->cb->GetUnitMaxRange(unit->key), range);
	buildRange = std::max<float>(unit->def->buildDistance, buildRange);
	speed = std::min<float>(ai->cb->GetUnitSpeed(unit->key), speed);
	los = std::max<float>(unit->def->losRadius, los);
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

float3 CGroup::pos() {
	std::map<int, CUnit*>::iterator i;
	float3 pos(0.0f, 0.0f, 0.0f);

	for (i = units.begin(); i != units.end(); i++)
		pos += ai->cb->GetUnitPos(i->first);

	pos /= units.size();

	return pos;
}

int CGroup::maxLength() {
	return size + units.size() * FOOTPRINT2REAL;
}

void CGroup::assist(ATask &t) {
	switch(t.t) {
		case BUILD: {
			CTaskHandler::BuildTask *task = dynamic_cast<CTaskHandler::BuildTask*>(&t);
			CUnit *unit  = task->group->firstUnit();
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
			CUnit *unit  = task->group->firstUnit();
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

bool CGroup::canReach(float3 &pos) {
	// TODO: what movetype should we use?
	return true;
}

bool CGroup::canAttack(int uid) {
	// TODO: if at least one unit can shoot target then return true
	return true;
}

bool CGroup::canAdd(CUnit *unit) {
	// TODO:
	return true;
}
		
bool CGroup::canMerge(CGroup *group) {
	/* TODO: can't merge: 
	- static vs mobile
	- water with non-water
	- underwater with hovers?
	- builders with non-builders?
	- nukes with non-nukes
	- lrpc with non-lrpc?
	*/
	return true;
}

CUnit* CGroup::firstUnit() {
	if (units.empty())
		return NULL;
	return units.begin()->second;
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
