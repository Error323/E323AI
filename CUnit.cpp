#include "CUnit.h"

#include "CAI.h"
#include "CUnitTable.h"
#include "CRNG.h"

void CUnit::remove() {
	remove(*this);
}

void CUnit::remove(ARegistrar &reg) {
	LOG_II("CUnit::remove " << (*this))
	
	std::list<ARegistrar*>::iterator i = records.begin();
	while(i != records.end()) {
		ARegistrar *regobj = *i; ++i;
		// remove from CUnitTable, CGroup
		regobj->remove(reg);
	}
	
	assert(records.empty());
	
	// TODO: remove next line when prev assertion is never raised
	records.clear();
}

void CUnit::reset(int uid, int bid) {
	records.clear();
	this->key     = uid;
	this->def     = ai->cb->GetUnitDef(uid);
	this->type    = UT(def->id);
	this->builtBy = bid;
	this->waiting = false;
	this->microing= false;
	this->techlvl = TECH1;
	this->group = NULL;
	this->aliveFrames = 0;
	this->microingFrames = 0;
}

bool CUnit::isEconomy() {
	static const unitCategory economic = 
		FACTORY|BUILDER|ASSISTER|RESURRECTOR|COMMANDER|MEXTRACTOR|MMAKER
		|EMAKER|MSTORAGE|ESTORAGE;
	return (type->cats&economic).any();
}

bool CUnit::reclaim(float3 pos, float radius) {
	Command c = createPosCommand(CMD_RECLAIM, pos, radius);
	if (c.id != 0) {
		ai->cb->GiveOrder(key, &c);
		ai->unittable->idle[key] = false;
		return true;
	}
	return false;
}

bool CUnit::reclaim(int target, bool enqueue) {
	Command c = createTargetCommand(CMD_RECLAIM, target);
	if (c.id != 0) {
		if (enqueue)
			c.options |= SHIFT_KEY;
		ai->cb->GiveOrder(key, &c);
		ai->unittable->idle[key] = false;
		return true;
	}
	return false;
}	

int CUnit::queueSize() {
	return ai->cb->GetCurrentUnitCommands(key)->size();
}

bool CUnit::attack(int target, bool enqueue) {
	Command c = createTargetCommand(CMD_ATTACK, target);
	if (c.id != 0) {
		if (enqueue)
			c.options |= SHIFT_KEY;
		ai->cb->GiveOrder(key, &c);
		ai->unittable->idle[key] = false;
		return true;
	}
	return false;
}

bool CUnit::moveForward(float dist, bool enqueue) {
	return move(getForwardPos(dist), enqueue);
}


bool CUnit::setOnOff(bool on) {
	Command c = createTargetCommand(CMD_ONOFF, on);

	if (c.id != 0) {
		ai->cb->GiveOrder(key, &c);
		return true;
	}
	
	return false;
}

bool CUnit::moveRandom(float radius, bool enqueue) {
	float3 pos = ai->cb->GetUnitPos(key);
	float3 newpos(rng.RandFloat(2.0f) - 1.0f, 0.0f, rng.RandFloat(2.0f) - 1.0f);
	newpos.Normalize();
	newpos   *= radius;
	newpos.x += pos.x;
	newpos.z += pos.z;
	newpos.y  = ai->cb->GetElevation(pos.x, pos.z);
	return move(newpos, enqueue);
}

bool CUnit::move(const float3 &pos, bool enqueue) {
	Command c = createPosCommand(CMD_MOVE, pos);
	
	if (c.id != 0) {
		if (enqueue)
			c.options |= SHIFT_KEY;
		ai->cb->GiveOrder(key, &c);
		ai->unittable->idle[key] = false;
		return true;
	}
	return false;
}

bool CUnit::guard(int target, bool enqueue) {
	Command c = createTargetCommand(CMD_GUARD, target);

	if (c.id != 0) {
		if (enqueue)
			c.options |= SHIFT_KEY;
		ai->cb->GiveOrder(key, &c);
		ai->unittable->idle[key] = false;
		return true;
	}
	return false;
}

bool CUnit::patrol(const float3& pos, bool enqueue) {
	Command c = createPosCommand(CMD_PATROL, pos);

	if (c.id != 0) {
		if (enqueue)
			c.options |= SHIFT_KEY;
		ai->cb->GiveOrder(key, &c);
		ai->unittable->idle[key] = false;
		return true;
	}
	return false;
}

bool CUnit::cloak(bool on) {
	Command c = createTargetCommand(CMD_CLOAK, on);

	if (c.id != 0) {
		ai->cb->GiveOrder(key, &c);
		return true;
	}
	return false;
}

bool CUnit::repair(int target) {
	Command c = createTargetCommand(CMD_REPAIR, target);

	if (c.id != 0) {
		ai->cb->GiveOrder(key, &c);
		ai->unittable->idle[key] = false;
		return true;
	}
	return false;
}

bool CUnit::build(UnitType* toBuild, const float3& pos) {
	bool staticBuilder = (type->cats&STATIC).any();
	int mindist = 8;
	
	// FIXME: remove hardcoding; we need analyzer of the largest footprint
	// per tech level
	if ((toBuild->cats&(FACTORY|EMAKER)).any()) {
		mindist = 10;
		if ((toBuild->cats&(TECH3|TECH4|TECH5|VEHICLE|NAVAL)).any())
			mindist = 15;
	}
	else if((toBuild->cats&MEXTRACTOR).any())
		mindist = 0;

	float startRadius  = staticBuilder ? def->buildDistance : def->buildDistance*2.0f;
	facing f           = getBestFacing(pos);
	float3 goal        = ai->cb->ClosestBuildSite(toBuild->def, pos, startRadius, mindist, f);

	if (goal.x < 0.0f) {
		if (staticBuilder)
			return false;
		
		int i = 0;
		while (goal.x < 0.0f) {
			startRadius += def->buildDistance;
			goal = ai->cb->ClosestBuildSite(toBuild->def, pos, startRadius, mindist, f);
			i++;
			if (i > 10) {
				/* Building in this area seems impossible, relocate unit */
				//moveRandom(startRadius);
				return false;
			}
		}
	}

	// NOTE: using of negative unit id is valid for Legacy C++ API only
	Command c = createPosCommand(-(toBuild->def->id), goal, -1.0f, f);
	if (c.id != 0) {
		ai->cb->GiveOrder(key, &c);
		ai->unittable->idle[key] = false;
		return true;
	}

	return false;
}

bool CUnit::stop() {
	Command c;
	c.id = CMD_STOP;
	ai->cb->GiveOrder(key, &c);
	return true;
}

bool CUnit::wait() {
	if (!waiting) {
		Command c;
		c.id = CMD_WAIT;
		ai->cb->GiveOrder(key, &c);
		waiting = true;
	}
	return waiting;
}

bool CUnit::unwait() {
	if (waiting) {
		Command c;
		c.id = CMD_WAIT;
		ai->cb->GiveOrder(key, &c);
		waiting = false;
	}
	return waiting;
}

bool CUnit::stockpile() {
	if (!type->def->stockpileWeaponDef)
		return false;

	Command c;
	c.id = CMD_STOCKPILE;
	ai->cb->GiveOrder(key, &c);
	return true;
}

int CUnit::getStockpileReady() {
	int result;
	ai->cb->GetProperty(key, AIVAL_STOCKPILED, &result);
	return result;
}
	
int CUnit::getStockpileQueued() {
	int result;
	ai->cb->GetProperty(key, AIVAL_STOCKPILE_QUED, &result);
	return result;
}

bool CUnit::micro(bool on) {
	if (on && !microing)
		microingFrames = 0;	
	microing = on; 
	return microing;
}

bool CUnit::factoryBuild(UnitType *ut, bool enqueue) {
	Command c;
	if (enqueue)
		c.options |= SHIFT_KEY;
	c.id = -(ut->def->id);
	ai->cb->GiveOrder(key, &c);
	ai->unittable->idle[key] = false;
	return true;
}

Command CUnit::createTargetCommand(int cmd, int target) {
	Command c;
	c.id = cmd;
	c.params.push_back(target);
	return c;
}

Command CUnit::createPosCommand(int cmd, float3 pos, float radius, facing f) {
	if (pos.x > ai->cb->GetMapWidth() * 8)
		pos.x = ai->cb->GetMapWidth() * 8;

	if (pos.z > ai->cb->GetMapHeight() * 8)
		pos.z = ai->cb->GetMapHeight() * 8;

	if (pos.x < 0)
		pos.x = 0;

	if (pos.y < 0)
		pos.y = 0;

	Command c;
	c.id = cmd;
	c.params.push_back(pos.x);
	c.params.push_back(pos.y);
	c.params.push_back(pos.z);

	if (f != NONE)
		c.params.push_back(f);

	if (radius > 0.0f)
		c.params.push_back(radius);
	return c;
}

quadrant CUnit::getQuadrant(const float3& pos) const {
	int mapWidth = ai->cb->GetMapWidth() * 8;
	int mapHeight = ai->cb->GetMapHeight() * 8;
	quadrant mapQuadrant = NORTH_EAST;

	if (pos.x < (mapWidth >> 1)) {
		/* left half of map */
		if (pos.z < (mapHeight >> 1)) {
			mapQuadrant = NORTH_WEST;
		} else {
			mapQuadrant = SOUTH_WEST;
		}
	}
	else {
		/* right half of map */
		if (pos.z < (mapHeight >> 1)) {
			mapQuadrant = NORTH_EAST;
		} else {
			mapQuadrant = SOUTH_EAST;
		}
	}
	return mapQuadrant;
}

facing CUnit::getBestFacing(const float3& pos) const {
	int mapWidth = ai->cb->GetMapWidth() * 8;
	int mapHeight = ai->cb->GetMapHeight() * 8;
	quadrant mapQuadrant = getQuadrant(pos);
	facing f = NONE;

	switch (mapQuadrant) {
		case NORTH_WEST:
			f = (mapHeight > mapWidth) ? SOUTH: EAST;
			break;
		case NORTH_EAST:
			f = (mapHeight > mapWidth) ? SOUTH: WEST;
			break;
		case SOUTH_WEST:
			f = (mapHeight > mapWidth) ? NORTH: EAST;
			break;
		case SOUTH_EAST:
			f = (mapHeight > mapWidth) ? NORTH: WEST;
			break;
	}

	return f;
}

float CUnit::getRange() const {
	if ((type->cats&BUILDER).any())
		return type->def->buildDistance;
	else if((type->cats&TRANSPORT).any())
		return type->def->loadingRadius;
	return ai->cb->GetUnitMaxRange(key);
}

bool CUnit::hasAntiAirWeapon(const std::vector<UnitDef::UnitDefWeapon>& weapons) {
	for (unsigned int i = 0; i < weapons.size(); i++) {
		const UnitDef::UnitDefWeapon *weapon = &weapons[i];
		if (weapon->def->tracks && !weapon->def->waterweapon 
		&& !weapon->def->stockpile && !weapon->def->canAttackGround)
			return true;
	}
	return false;
}

bool CUnit::hasNukeWeapon(const std::vector<UnitDef::UnitDefWeapon> &weapons) {
	for (unsigned int i = 0; i < weapons.size(); i++) {
		const UnitDef::UnitDefWeapon *weapon = &weapons[i];
		if (weapon->def->stockpile && weapon->def->range > 10000 
		/*&& weapon->def->fixedLauncher*/ && !weapon->def->paralyzer
		&& !weapon->def->interceptor && weapon->def->targetable
		&& weapon->def->damages.GetDefaultDamage() > 1000.0f)
			return true;
	}
	return false;
}

bool CUnit::hasParalyzerWeapon(const std::vector<UnitDef::UnitDefWeapon> &weapons) {
	for (unsigned int i = 0; i < weapons.size(); i++) {
		const UnitDef::UnitDefWeapon *weapon = &weapons[i];
		if (weapon->def->paralyzer)
			return true;
	}
	return false;
}

bool CUnit::hasInterceptorWeapon(const std::vector<UnitDef::UnitDefWeapon>& weapons) {
	for (unsigned int i = 0; i < weapons.size(); i++) {
		const UnitDef::UnitDefWeapon *weapon = &weapons[i];
		if (weapon->def->stockpile && weapon->def->interceptor
		/*&& weapon->def->fixedLauncher*/)
			return true;
	}
	return false;
}

bool CUnit::hasTorpedoWeapon(const std::vector<UnitDef::UnitDefWeapon>& weapons) {
	for (unsigned int i = 0; i < weapons.size(); i++) {
		const UnitDef::UnitDefWeapon *weapon = &weapons[i];
		if (weapon->def->submissile || weapon->def->waterweapon /*&& weapon->def->tracks*/)
			return true;
	}
	return false;
}

bool CUnit::hasShield(const std::vector<UnitDef::UnitDefWeapon>& weapons) {
	for (unsigned int i = 0; i < weapons.size(); i++) {
		const UnitDef::UnitDefWeapon* weapon = &weapons[i];
		if (weapon->def->isShield)
			return true;
	}
	return false;
}

float3 CUnit::getForwardPos(float distance) const {
	float3 result = pos();
	facing f = getBestFacing(result);
	
	switch(f) {
		case NORTH:
			result.z -= distance;
			break;
		case SOUTH:
			result.z += distance;
			break;
		case EAST:
			result.x += distance;
			break;
		case WEST:
			result.x -= distance;
			break;
		default: break;
	}
	
	result.y = ai->cb->GetElevation(result.x, result.z);

	return result;
}

std::ostream& operator<<(std::ostream &out, const CUnit &unit) {
	if (unit.def == NULL)
		out << "Unknown" << "(" << unit.key << ", " << unit.builtBy << ")";
	else
		out << unit.def->humanName << "(" << unit.key << ", " << unit.builtBy << ")";
	return out;
}
