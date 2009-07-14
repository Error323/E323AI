#include "Group.h"

CMyGroup::CMyGroup(AIClasses *ai, int id, groupType type) {
	this->ai   = ai;
	this->id   = id;
	this->type = type;

	strength   = 0.0f;
	range      = 0.0f;
	busy       = false;
}

void CMyGroup::add(int unit) {
	units[unit] = false;
	strength += ai->call->GetUnitPower(unit);
	range = std::max<float>(ai->call->GetUnitMaxRange(unit), range);
}

void CMyGroup::remove(int unit) {
	units.erase(unit);
	strength = 0.0f;

	/* Recalculate range and strength of the group */
	std::map<int, bool>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		range     = std::max<float>(ai->call->GetUnitMaxRange(i->first), range);
		strength += ai->call->GetUnitPower(i->first);
	}
}

void CMyGroup::merge(CMyGroup &group) {
	std::map<int, bool>::iterator i;
	for (i = group.units.begin(); i != group.units.end(); i++)
		units[i->first] = i->second;

	strength += group.strength;
}

float3 CMyGroup::pos() {
	std::map<int, bool>::iterator i;
	float3 pos(0.0f, 0.0f, 0.0f);

	for (i = units.begin(); i != units.end(); i++)
		pos += ai->call->GetUnitPos(i->first);

	pos /= units.size();

	return pos;
}