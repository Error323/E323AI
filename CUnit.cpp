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
		ARegistrar *regobj = *i; i++;
		// remove from CUnitTable, CGroup
		regobj->remove(reg);
	}
	//records.clear();
}

float3 CUnit::pos() {
	return ai->cb->GetUnitPos(key);
}

void CUnit::reset(int uid, int bid) {
	records.clear();
	this->key     = uid;
	this->def     = ai->cb->GetUnitDef(uid);
	this->type    = UT(def->id);
	this->builtBy = bid;
	this->waiting = false;
	this->microing= false;
	this->techlvl = 0;
	this->group = NULL;
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
	float3 upos = ai->cb->GetUnitPos(key);
	facing f = getBestFacing(upos);
	float3 pos(upos);
	switch(f) {
		case NORTH:
			pos.z -= dist;
		break;
		case SOUTH:
			pos.z += dist;
		break;
		case EAST:
			pos.x += dist;
		break;
		case WEST:
			pos.x -= dist;
		break;
		default: break;
	}
	return move(pos, enqueue);
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
	float3 newpos(rng.RandFloat(), 0.0f, rng.RandFloat());
	newpos.Normalize();
	newpos   *= radius;
	newpos.x += pos.x;
	newpos.y  = pos.y;
	newpos.z += pos.z;
	return move(newpos, enqueue);
}

bool CUnit::move(float3 &pos, bool enqueue) {
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

bool CUnit::build(UnitType *toBuild, float3 &pos) {
	int mindist = 8;
	if (toBuild->cats&FACTORY || toBuild->cats&EMAKER) {
		mindist = 10;
		if (toBuild->cats&VEHICLE || toBuild->cats&TECH3)
			mindist = 15;
	}
	else if(toBuild->cats&MEXTRACTOR)
		mindist = 0;

	float startRadius  = def->buildDistance*2.0f;
	facing f           = getBestFacing(pos);
	float3 start       = ai->cb->GetUnitPos(key);
	float3 goal        = ai->cb->ClosestBuildSite(toBuild->def, pos, startRadius, mindist, f);

	int i = 0;
	while (goal == ERRORVECTOR) {
		startRadius += def->buildDistance;
		goal = ai->cb->ClosestBuildSite(toBuild->def, pos, startRadius, mindist, f);
		i++;
		if (i > 15) {
			/* Building in this area seems impossible, relocate unit */
			moveRandom(startRadius);
			return false;
		}
	}

	Command c = createPosCommand(-(toBuild->id), goal, -1.0f, f);
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

bool CUnit::micro(bool on) {
	microing = on;
	return microing;
}

bool CUnit::isMicroing() {
	return microing;
}

bool CUnit::isOn() {
	return ai->cb->IsUnitActivated(key);
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

/* From KAIK */
bool CUnit::factoryBuild(UnitType *ut, bool enqueue) {
	Command c;
	if (enqueue)
		c.options |= SHIFT_KEY;
	c.id = -(ut->def->id);
	ai->cb->GiveOrder(key, &c);
	ai->unittable->idle[key] = false;
	return true;
}

/* From KAIK */
Command CUnit::createTargetCommand(int cmd, int target) {
	Command c;
	c.id = cmd;
	c.params.push_back(target);
	return c;
}

/* From KAIK */
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

quadrant CUnit::getQuadrant(float3 &pos) {
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

/* From KAIK */
facing CUnit::getBestFacing(float3 &pos) {
	int mapWidth = ai->cb->GetMapWidth() * 8;
	int mapHeight = ai->cb->GetMapHeight() * 8;
	quadrant mapQuadrant = getQuadrant(pos);
	facing f = NONE;

	switch (mapQuadrant) {
		case NORTH_WEST: {
			f = (mapHeight > mapWidth) ? SOUTH: EAST;
		} break;
		case NORTH_EAST: {
			f = (mapHeight > mapWidth) ? SOUTH: WEST;
		} break;
		case SOUTH_WEST: {
			f = (mapHeight > mapWidth) ? NORTH: EAST;
		} break;
		case SOUTH_EAST: {
			f = (mapHeight > mapWidth) ? NORTH: WEST;
		} break;
	}

	return f;
}

std::ostream& operator<<(std::ostream &out, const CUnit &unit) {
	out << unit.def->humanName << "(" << unit.key << ", " << unit.builtBy << ")";
	return out;
}
