#include "CGroup.h"

int CGroup::counter = 0;

CGroup::CGroup(AIClasses *ai) {
	this->ai = ai;
	reset();
	counter++;
}

void CGroup::addUnit(CUnit &unit) {
	MoveData* md = ai->call->GetUnitDef(unit.key)->movedata;
	assert(md != NULL); /* this would be bad */

	if (md->maxSlope <= maxSlope) {
		moveType = md->pathType;
		maxSlope = md->maxSlope;
	}
		
	strength += ai->call->GetUnitPower(unit.key);
	range = std::max<float>(ai->call->GetUnitMaxRange(unit.key), range);

	waiters[unit.key] = false;
	units[unit.key]   = &unit;
	unit.reg(*this);
}

void CGroup::remove() {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->unreg(*this);
	
	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); j++)
		(*j)->remove(*this);
}

void CGroup::remove(ARegistrar &unit) {
	waiters.erase(unit.key);
	units.erase(unit.key);

	strength = 0.0f;
	MoveData* md;

	/* Recalculate range, strength and maxSlope of the group */
	range = 0.0f;
	maxSlope = 1.0f;
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		range     = std::max<float>(ai->call->GetUnitMaxRange(i->first), range);
		strength += ai->call->GetUnitPower(i->first);
		md        = ai->call->GetUnitDef(i->first)->movedata;
		if (md->maxSlope <= maxSlope) {
			moveType = md->pathType;
			maxSlope = md->maxSlope;
		}
	}

	/* If no more units remain in this group, remove the group */
	if (units.empty()) {
		std::list<ARegistrar*>::iterator i;
		for (i = records.begin(); i != records.end(); i++)
			(*i)->remove(*this);
	}
}

void CGroup::reset() {
	strength   = 0.0f;
	range      = 0.0f;
	busy       = false;
	maxSlope   = 1.0f;
	units.clear();
	waiters.clear();
	LOGN(buf);
}

void CGroup::merge(CGroup &group) {
	std::map<int, bool>::iterator i;
	for (i = group.units.begin(); i != group.units.end(); i++) {
		CUnit *unit = ai->unitTable->getUnit(i->first);
		unit->stop();
		addUnit(*unit);
	}
	group.remove();
}

float3 CGroup::pos() {
	std::map<int, bool>::iterator i;
	float3 pos(0.0f, 0.0f, 0.0f);

	for (i = units.begin(); i != units.end(); i++)
		pos += ai->call->GetUnitPos(i->first);

	pos /= units.size();

	return pos;
}

int CGroup::maxLength() {
	return units.size()*50;
}
