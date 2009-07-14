#include "MetaCommands.h"

CMetaCommands::CMetaCommands(AIClasses *ai) {
	this->ai = ai;
}

CMetaCommands::~CMetaCommands() {}

void CMetaCommands::moveGroup(int group, float3 &pos, bool enqueue) {
	std::map<int, bool>::iterator i;
	std::map<int, bool> *G = &(ai->military->groups[group].units);
	for (i = G->begin(); i != G->end(); i++)
		move(i->first, pos, enqueue);
}

void CMetaCommands::attackGroup(int group, int target) {
	std::map<int, bool>::iterator i;
	std::map<int, bool> *G = &(ai->military->groups[group].units);
	for (i = G->begin(); i != G->end(); i++)
		attack(i->first, target);
}

bool CMetaCommands::attack(int unit, int target) {
	Command c = createTargetCommand(CMD_ATTACK, target);
	if (c.id != 0) {
		ai->call->GiveOrder(unit, &c);
		return true;
	}
	return false;
}

bool CMetaCommands::moveForward(int unit, float dist) {
	float3 upos = ai->call->GetUnitPos(unit);
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
	return move(unit, pos);
}


bool CMetaCommands::setOnOff(int unit, bool on) {
	Command c = createTargetCommand(CMD_ONOFF, on);

	if (c.id != 0) {
		ai->call->GiveOrder(unit, &c);
		return true;
	}
	
	return false;
}

bool CMetaCommands::moveRandom(int unit, float radius, bool enqueue) {
	float3 pos = ai->call->GetUnitPos(unit);
	float3 newpos(rng.RandFloat(), 0.0f, rng.RandFloat());
	newpos.Normalize();
	newpos   *= radius;
	newpos.x += pos.x;
	newpos.y  = pos.y;
	newpos.z += pos.z;
	return move(unit, newpos, enqueue);
}

bool CMetaCommands::move(int unit, float3 &pos, bool enqueue) {
	Command c = createPosCommand(CMD_MOVE, pos);
	
	if (c.id != 0) {
		if (enqueue)
			c.options |= SHIFT_KEY;
		ai->call->GiveOrder(unit, &c);
		ai->eco->removeIdleUnit(unit);
		return true;
	}
	sprintf(buf, "[CMetaCommands::move]\t %s(%d) moves", ai->call->GetUnitDef(unit)->humanName.c_str(), unit);
	LOGN(buf);
	return false;
}

bool CMetaCommands::guard(int unit, int target, bool enqueue) {
	Command c = createTargetCommand(CMD_GUARD, target);
	ai->eco->removeMyGuards(unit);

	if (c.id != 0) {
		if (enqueue)
			c.options |= SHIFT_KEY;
		ai->call->GiveOrder(unit, &c);
		ai->eco->gameGuarding[unit] = target;
		ai->eco->removeIdleUnit(unit);
		const UnitDef *u = ai->call->GetUnitDef(unit);
		const UnitDef *t = ai->call->GetUnitDef(target);
		sprintf(buf, "[CMetaCommands::guard]\t %s(%d) guards %s(%d)", u->humanName.c_str(), unit, t->humanName.c_str(), target);
		LOGN(buf);
		return true;
	}
	return false;
}

bool CMetaCommands::repair(int builder, int target) {
	Command c = createTargetCommand(CMD_REPAIR, target);

	if (c.id != 0) {
		ai->call->GiveOrder(builder, &c);
		ai->eco->removeIdleUnit(builder);
		const UnitDef *u = ai->call->GetUnitDef(builder);
		const UnitDef *t = ai->call->GetUnitDef(target);
		sprintf(buf, "[CMetaCommands::repair]\t %s repairs %s", u->name.c_str(), t->name.c_str());
		LOGN(buf);
		return true;
	}
	return false;
}

bool CMetaCommands::build(int builder, UnitType *toBuild, float3 &pos) {
	int mindist = 5;
	if (toBuild->cats&FACTORY || toBuild->cats&EMAKER)
		mindist = 10;
	else if(toBuild->cats&MEXTRACTOR)
		mindist = 0;

	const UnitDef *ud  = ai->call->GetUnitDef(builder);
	float startRadius  = ud->buildDistance;
	facing f           = getBestFacing(pos);
	float3 start       = ai->call->GetUnitPos(builder);
	float3 goal        = ai->call->ClosestBuildSite(toBuild->def, pos, startRadius, mindist, f);

	int i = 0;
	while (goal == ERRORVECTOR) {
		startRadius += ud->buildDistance;
		goal = ai->call->ClosestBuildSite(toBuild->def, pos, startRadius, mindist, f);
		sprintf(buf, "------%s(%d) trying to build %s iteration %d", ud->name.c_str(), builder, toBuild->def->name.c_str(), i);
		LOGN(buf);
		i++;
		if (i > 10) {
			/* Building in this area seems impossible, relocate builder */
			moveRandom(builder, startRadius);
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
		sprintf(buf, "[CMetaCommands::build]\t %s(%d) builds %s", ud->humanName.c_str(), builder, toBuild->def->humanName.c_str());
		LOGN(buf);
		return true;
	}
	return false;
}

bool CMetaCommands::stop(int unit) {
	assert(ai->call->GetUnitDef(unit) != NULL);
	Command c;
	c.id = CMD_STOP;
	ai->call->GiveOrder(unit, &c);
	sprintf(buf, "[CMetaCommands::stop]\t %s(%d) stopped", ai->call->GetUnitDef(unit)->humanName.c_str(), unit);
	LOGN(buf);
	return true;
}

bool CMetaCommands::wait(int unit) {
	assert(ai->call->GetUnitDef(unit) != NULL);
	Command c;
	c.id = CMD_WAIT;
	ai->call->GiveOrder(unit, &c);
	sprintf(buf, "[CMetaCommands::wait]\t %s(%d) waited", ai->call->GetUnitDef(unit)->humanName.c_str(), unit);
	LOGN(buf);
	return true;
}

/* From KAIK */
bool CMetaCommands::factoryBuild(int factory, UnitType *ut) {
	assert(ai->call->GetUnitDef(factory) != NULL);
	Command c;
	c.id = -(ut->def->id);
	ai->call->GiveOrder(factory, &c);
	ai->eco->removeIdleUnit(factory);
	const UnitDef *u = ai->call->GetUnitDef(factory);
	sprintf(buf, "[CMetaCommands::factoryBuild]\t %s(%d) builds %s", u->humanName.c_str(), factory, ut->def->humanName.c_str());
	LOGN(buf);
	return true;
}

/* From KAIK */
Command CMetaCommands::createTargetCommand(int cmd, int target) {
	Command c;
	c.id = cmd;
	c.params.push_back(target);
	return c;
}

/* From KAIK */
Command CMetaCommands::createPosCommand(int cmd, float3 pos, float radius, facing f) {
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

quadrant CMetaCommands::getQuadrant(float3 &pos) {
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
facing CMetaCommands::getBestFacing(float3 &pos) {
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
