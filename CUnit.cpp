#include "CUnit.h"

CUnit::CUnit(AIClasses *ai, int uid, int builder) {
	this->ai  = ai;
	reset(uid, builder);
}

void CUnit::remove() {
	remove(*this);
}

void CUnit::remove(ARegistrar &reg) {
	std::list<ARegistrar*>::iterator i;
	for (i = registers.begin(); i != registers.end(); i++) {
		(*i)->remove(reg);
	}
}

void CUnit::reset(int uid, int builder) {
	registers.clear();
	this->key     = uid;
	this->def     = ai->call->GetUnitDef(uid);
	this->type    = UT(def->id);
	this->builder = builder;
}

bool CUnit::attack(int target) {
	Command c = createTargetCommand(CMD_ATTACK, target);
	if (c.id != 0) {
		ai->call->GiveOrder(key, &c);
		return true;
	}
	return false;
}

bool CUnit::moveForward(float dist) {
	float3 upos = ai->call->GetUnitPos(key);
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
	}
	return move(key, pos);
}


bool CUnit::setOnOff(bool on) {
	Command c = createTargetCommand(CMD_ONOFF, on);

	if (c.id != 0) {
		ai->call->GiveOrder(key, &c);
		return true;
	}
	
	return false;
}

bool CUnit::moveRandom(float radius, bool enqueue) {
	float3 pos = ai->call->GetUnitPos(key);
	float3 newpos(rng.RandFloat(), 0.0f, rng.RandFloat());
	newpos.Normalize();
	newpos   *= radius;
	newpos.x += pos.x;
	newpos.y  = pos.y;
	newpos.z += pos.z;
	return move(key, newpos, enqueue);
}

bool CUnit::move(float3 &pos, bool enqueue) {
	Command c = createPosCommand(CMD_MOVE, pos);
	
	if (c.id != 0) {
		if (enqueue)
			c.options |= SHIFT_KEY;
		ai->call->GiveOrder(key, &c);
		ai->eco->removeIdleUnit(key);
		return true;
	}
	sprintf(buf, "[CUnit::move]\t %s(%d) moves", ai->call->GetUnitDef(key)->humanName.c_str(), key);
	LOGN(buf);
	return false;
}

bool CUnit::guard(int target, bool enqueue) {
	Command c = createTargetCommand(CMD_GUARD, target);
	ai->eco->removeMyGuards(key);

	if (c.id != 0) {
		if (enqueue)
			c.options |= SHIFT_KEY;
		ai->call->GiveOrder(key, &c);
		ai->eco->gameGuarding[key] = target;
		ai->eco->removeIdleUnit(key);
		const UnitDef *u = ai->call->GetUnitDef(key);
		const UnitDef *t = ai->call->GetUnitDef(target);
		sprintf(buf, "[CUnit::guard]\t %s(%d) guards %s(%d)", u->humanName.c_str(), key, t->humanName.c_str(), target);
		LOGN(buf);
		return true;
	}
	return false;
}

bool CUnit::repair(int builder, int target) {
	Command c = createTargetCommand(CMD_REPAIR, target);

	if (c.id != 0) {
		ai->call->GiveOrder(builder, &c);
		ai->eco->removeIdleUnit(builder);
		const UnitDef *u = ai->call->GetUnitDef(builder);
		const UnitDef *t = ai->call->GetUnitDef(target);
		sprintf(buf, "[CUnit::repair]\t %s repairs %s", u->name.c_str(), t->name.c_str());
		LOGN(buf);
		return true;
	}
	return false;
}

bool CUnit::build(UnitType *toBuild, float3 &pos) {
	int mindist = 5;
	if (toBuild->cats&FACTORY || toBuild->cats&EMAKER)
		mindist = 10;
	else if(toBuild->cats&MEXTRACTOR)
		mindist = 0;

	const UnitDef *ud  = ai->call->GetUnitDef(key);
	float startRadius  = ud->buildDistance;
	facing f           = getBestFacing(pos);
	float3 start       = ai->call->GetUnitPos(key);
	float3 goal        = ai->call->ClosestBuildSite(toBuild->def, pos, startRadius, mindist, f);

	int i = 0;
	while (goal == ERRORVECTOR) {
		startRadius += ud->buildDistance;
		goal = ai->call->ClosestBuildSite(toBuild->def, pos, startRadius, mindist, f);
		sprintf(buf, "------%s(%d) trying to build %s iteration %d", ud->name.c_str(), key, toBuild->def->name.c_str(), i);
		LOGN(buf);
		i++;
		if (i > 10) {
			/* Building in this area seems impossible, relocate key */
			moveRandom(key, startRadius);
			return false;
		}
	}

	Command c = createPosCommand(-(toBuild->id), goal, -1.0f, f);
	if (c.id != 0) {
		ai->call->GiveOrder(builder, &c);
		ai->tasks->addBuildPlan(builder, toBuild);
		float dist = 100.0f;
		goal.y += 20;
		float3 arrow = goal;
		switch(f) {
			case NORTH:
				arrow.z -= dist;
			break;
			case SOUTH:
				arrow.z += dist;
			break;
			case EAST:
				arrow.x += dist;
			break;
			case WEST:
				arrow.x -= dist;
			break;
		}
		ai->eco->removeIdleUnit(builder);
		sprintf(buf, "[CUnit::build]\t %s(%d) builds %s", ud->humanName.c_str(), builder, toBuild->def->humanName.c_str());
		LOGN(buf);
		return true;
	}
	return false;
}

bool CUnit::stop() {
	Command c;
	c.id = CMD_STOP;
	ai->call->GiveOrder(key, &c);
	sprintf(buf, "[CUnit::stop]\t %s(%d) stopped", ai->call->GetUnitDef(key)->humanName.c_str(), key);
	LOGN(buf);
	return true;
}

bool CUnit::wait() {
	Command c;
	c.id = CMD_WAIT;
	ai->call->GiveOrder(key, &c);
	sprintf(buf, "[CUnit::wait]\t %s(%d) waited", ai->call->GetUnitDef(key)->humanName.c_str(), key);
	LOGN(buf);
	return true;
}

/* From KAIK */
bool CUnit::factoryBuild(UnitType *ut) {
	Command c;
	c.id = -(ut->def->id);
	ai->call->GiveOrder(key, &c);
	ai->eco->removeIdleUnit(key);
	const UnitDef *u = ai->call->GetUnitDef(key);
	sprintf(buf, "[CUnit::factoryBuild]\t %s(%d) builds %s", u->humanName.c_str(), key, ut->def->humanName.c_str());
	LOGN(buf);
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
	if (pos.x > ai->call->GetMapWidth() * 8)
		pos.x = ai->call->GetMapWidth() * 8;

	if (pos.z > ai->call->GetMapHeight() * 8)
		pos.z = ai->call->GetMapHeight() * 8;

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
	int mapWidth = ai->call->GetMapWidth() * 8;
	int mapHeight = ai->call->GetMapHeight() * 8;
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
	int mapWidth = ai->call->GetMapWidth() * 8;
	int mapHeight = ai->call->GetMapHeight() * 8;
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
