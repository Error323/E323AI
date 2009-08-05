#include "CGroup.h"

int CGroup::counter = 0;

void CGroup::addUnit(CUnit &unit) {
	MoveData* md = ai->call->GetUnitDef(unit.key)->movedata;
	assert(md != NULL); /* this would be bad */

	if (md->maxSlope <= maxSlope) {
		moveType = md->pathType;
		maxSlope = md->maxSlope;
	}
		
	strength   += ai->call->GetUnitPower(unit.key);
	buildSpeed += unit.def->buildSpeed;
	range = std::max<float>(ai->call->GetUnitMaxRange(unit.key), range);
	buildRange = std::max<float>(unit.def->buildDistance, buildRange);
	speed = std::min<float>(ai->call->GetUnitSpeed(unit.key), speed);

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
	sprintf(buf, "[CGroup::remove]\tremove unit(%d)", unit.key);
	LOGN(buf);
	waiters.erase(unit.key);
	units.erase(unit.key);

	strength = buildSpeed = 0.0f;
	speed = MAX_FLOAT;
	MoveData* md;

	/* Recalculate range, strength and maxSlope of the group */
	range = 0.0f;
	maxSlope = 1.0f;
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		range       = std::max<float>(ai->call->GetUnitMaxRange(i->first), range);
		speed       = std::min<float>(ai->call->GetUnitSpeed(i->first), speed);
		strength   += ai->call->GetUnitPower(i->first);
		buildSpeed += i->second->def->buildSpeed;
		md          = ai->call->GetUnitDef(i->first)->movedata;
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
	speed      = MAX_FLOAT;
	buildSpeed = 0.0f;
	range      = 0.0f;
	buildRange = 0.0f;
	busy       = false;
	maxSlope   = 1.0f;
	units.clear();
	waiters.clear();
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
		pos += ai->call->GetUnitPos(i->first);

	pos /= units.size();

	return pos;
}

int CGroup::maxLength() {
	return units.size()*50;
}

void CGroup::assist(ATask &t) {
	switch(t.t) {
		case BUILD: {
			CTaskHandler::BuildTask *task = dynamic_cast<CTaskHandler::BuildTask*>(&t);
			CGroup *group = task->groups.begin()->second;
			CUnit  *unit  = group->units.begin()->second;
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
