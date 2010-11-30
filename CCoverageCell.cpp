#include "CCoverageCell.h"

#include "CAI.h"
#include "CUnitTable.h"
#include "CCoverageHandler.h"


std::map<CCoverageCell::NType, std::string> CCoverageCell::type2str;

bool CCoverageCell::isInRange(const float3& pos)  const {
	return getCenter().distance2D(pos) <= range;
}

void CCoverageCell::update(std::list<CUnit*>& uncovered) {
	if (isVacant())	return;

	float newRange = ai->coverage->getCoreRange(type, unit->type);
	if (newRange < range) {
		const float3 center = getCenter();
		for (std::map<int, CUnit*>::iterator it = units.begin(); it != units.end(); ) {
			const float3 pos = it->second->pos();
			if (center.distance2D(pos) > newRange) {
				uncovered.push_back(it->second);
				it->second->unreg(*this);
				units.erase(it++);
			}
			else
				++it;
		}

		range = newRange;
	}

	// TODO: if core is mobile then update it when position has changed
}

void CCoverageCell::setCore(CUnit* u) {
	assert(unit == NULL);
	
	u->reg(*this);

	unit = u;
	range = ai->coverage->getCoreRange(type, unit->type);
}

void CCoverageCell::remove() {
	LOG_II("CCoverageCell::remove " << (*this))

	// remove this object from CCoverageHandler...
	for (std::list<ARegistrar*>::iterator it = records.begin(); it != records.end(); ) {
		(*(it++))->remove(*this);
	}
	
	assert(records.empty());

	if (unit)
		unit->unreg(*this);

	if (!units.empty()) {
		for (std::map<int, CUnit*>::iterator it = units.begin(); it != units.end(); ++it) {
			it->second->unreg(*this);
		}
		units.clear();
	}

	unit = NULL;
	range = 0.0f;
}

void CCoverageCell::remove(ARegistrar& u) {
	if (unit->key == u.key)
		remove();
	else {
		units.erase(u.key);
		u.unreg(*this);
	}
}

bool CCoverageCell::addUnit(CUnit* u) {
	if (unit && unit->key == u->key)
		return false; // do not add itself
	if (ai->coverage->getCoreType(u->type) == type)
		return false; // do not cover neighbour cores
	assert(units.find(u->key) == units.end());
	units[u->key] = u;
	u->reg(*this);
	return true;
}

std::ostream& operator<<(std::ostream& out, const CCoverageCell& obj) {
	std::stringstream ss;
	
	if (obj.unit)
		ss << "CoverageCell(" << obj.unit->def->humanName;
	else
		ss << "CoverageCell(Unknown";
	
	ss << "):" << " type(" << obj.type2str[obj.type] << "), range(" << obj.range << "), amount(" << obj.units.size() << ")";

	std::string s = ss.str();
	
	out << s;
	
	return out;
}
