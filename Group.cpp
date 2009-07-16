#include "Group.h"

int CMyGroup::counter = 0;

CMyGroup::CMyGroup(AIClasses *ai, groupType type) {
	this->ai   = ai;
	this->type = type;
	this->id   = counter;

	strength   = 0.0f;
	range      = 0.0f;
	busy       = false;
	maxSlope   = 1.0f;
	sprintf(buf, "[CMyGroup::CMyGroup]\ttype(%s) id(%d)", 
		type == G_SCOUT ? "SCOUT" : "ATTACK",
		id
	); 
	LOGN(buf);

	counter++;
}

void CMyGroup::add(int unit) {
	MoveData* md = ai->call->GetUnitDef(unit)->movedata;
	assert(md != NULL); /* this would be bad */

	if (md->maxSlope <= maxSlope) {
		moveType = md->pathType;
		maxSlope = md->maxSlope;
	}
		
	units[unit] = false;
	strength += ai->call->GetUnitPower(unit);
	range = std::max<float>(ai->call->GetUnitMaxRange(unit), range);
}

void CMyGroup::remove(int unit) {
	units.erase(unit);
	strength = 0.0f;
	MoveData* md = ai->call->GetUnitDef(unit)->movedata;

	/* Recalculate range, strength and maxSlope of the group */
	range = 0.0f;
	maxSlope = 1.0f;
	std::map<int, bool>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		range     = std::max<float>(ai->call->GetUnitMaxRange(i->first), range);
		strength += ai->call->GetUnitPower(i->first);
		if (md->maxSlope <= maxSlope) {
			moveType = md->pathType;
			maxSlope = md->maxSlope;
		}
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
