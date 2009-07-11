#include "Group.h"

CGroup::CGroup(AIClasses *ai, int id) {
	this->ai = ai;
	this->id = id;
	strength = 0.0f;
}

void CGroup::add(int unit) {
	units[unit] = false;
	strength += ai->call->GetUnitPower(unit);
}

void CGroup::remove(int unit) {
	units.erase(unit);
	strength -= ai->call->GetUnitPower(unit);
}

void CGroup::merge(CGroup &group) {
	std::map<int, bool>::iterator i;
	for (i = group.units.begin(); i != group.units.end(); i++)
		units[i->first] = i->second;

	strength += group.strength;
}

float3 CGroup::pos() {
	std::map<int, bool>::iterator i;
	float3 pos(0.0f, 0.0f, 0.0f);

	for (i = units.begin(); i != units.end(); i++)
		pos += ai->call->GetUnitPos(i->first);

	pos /= units.size();

	return pos;
}
