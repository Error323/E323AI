#include "CGroup.h"

#include <sstream>
#include <iostream>
#include <string>

#include "CAI.h"
#include "CUnit.h"
#include "CUnitTable.h"
#include "CTaskHandler.h"

int CGroup::counter = 0;

void CGroup::addUnit(CUnit &unit) {
	LOG_II("CGroup::add " << unit)
	MoveData* md = ai->cb->GetUnitDef(unit.key)->movedata;

	if (md->maxSlope <= maxSlope) {
		moveType = md->pathType;
		maxSlope = md->maxSlope;
	}
		
	strength   += ai->cb->GetUnitPower(unit.key);
	buildSpeed += unit.def->buildSpeed;
	range = std::max<float>(ai->cb->GetUnitMaxRange(unit.key)*0.8f, range);
	buildRange = std::max<float>(unit.def->buildDistance*1.5f, buildRange);
	speed = std::min<float>(ai->cb->GetUnitSpeed(unit.key), speed);

	units[unit.key]   = &unit;
	unit.reg(*this);
}

void CGroup::remove() {
	LOG_II("CGroup::remove " << (*this))

	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->unreg(*this);
	
	std::list<ARegistrar*>::iterator j;
	for (j = records.begin(); j != records.end(); j++)
		(*j)->remove(*this);
}

void CGroup::remove(ARegistrar &unit) {
	LOG_II("CGroup::remove unit(" << unit.key << ")")
	units.erase(unit.key);

	strength = buildSpeed = 0.0f;
	speed = MAX_FLOAT;
	MoveData* md;

	/* Recalculate range, strength and maxSlope of the group */
	range = 0.0f;
	maxSlope = 1.0f;
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		range       = std::max<float>(ai->cb->GetUnitMaxRange(i->first)*0.9f, range);
		speed       = std::min<float>(ai->cb->GetUnitSpeed(i->first), speed);
		strength   += ai->cb->GetUnitPower(i->first);
		buildSpeed += i->second->def->buildSpeed;
		md          = ai->cb->GetUnitDef(i->first)->movedata;
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

bool CGroup::isIdle() {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		if (ai->unittable->idle[i->second->key])
			return true;
	}
	return false;
}

void CGroup::reset() {
	strength   = 0.0f;
	speed      = MAX_FLOAT;
	buildSpeed = 0.0f;
	range      = 0.0f;
	buildRange = 0.0f;
	busy       = false;
	maxSlope   = 1.0f;
	units.clear();
}

void CGroup::merge(CGroup &group) {
	std::map<int, CUnit*>::iterator i;
	for (i = group.units.begin(); i != group.units.end(); i++) {
		CUnit *unit = i->second;
		unit->stop();
		addUnit(*unit);
	}
	group.remove();
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
	return units.size()*40;
}

void CGroup::assist(ATask &t) {
	switch(t.t) {
		case BUILD: {
			CTaskHandler::BuildTask *task = dynamic_cast<CTaskHandler::BuildTask*>(&t);
			CUnit  *unit  = task->group->units.begin()->second;
			guard(unit->key);
			break;
		}

		case ATTACK: {
			//TODO: Calculate the flanking pos and attack from there
			CTaskHandler::AttackTask *task = dynamic_cast<CTaskHandler::AttackTask*>(&t);
			attack(task->target);
			break;
		}

		case FACTORY_BUILD: {
			CTaskHandler::FactoryTask *task = dynamic_cast<CTaskHandler::FactoryTask*>(&t);
			guard(task->factory->key);
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

void CGroup::attack(int target) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->attack(target);
}

void CGroup::build(float3 &pos, UnitType *ut) {
	std::map<int, CUnit*>::iterator alpha, i;
	alpha = units.begin();
	alpha->second->build(ut, pos);
	for (i = ++alpha; i != units.end(); i++)
		i->second->guard(alpha->first);
}

void CGroup::stop() {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->stop();
}

void CGroup::guard(int target, bool enqueue) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->guard(target, enqueue);
}

std::ostream& operator<<(std::ostream &out, const CGroup &group) {
	std::stringstream ss;
	ss << "Group(" << group.key << "):" << " amount(" << group.units.size() << ") [";
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
