#include "MetaCommands.h"

CMetaCommands::CMetaCommands(AIClasses *ai) {
	this->ai = ai;
}

CMetaCommands::~CMetaCommands() {}

bool CMetaCommands::move(int unit, float3 &pos) {
	Command c = createPosCommand(CMD_MOVE, pos);
	
	if (c.id != 0) {
		ai->call->GiveOrder(unit, &c);
		return true;
	}
	ai->eco->removeIdleUnit(unit);
	return false;
}

bool CMetaCommands::guard(int unit, int target) {
	Command c = createTargetCommand(CMD_GUARD, target);

	if (c.id != 0) {
		ai->call->GiveOrder(unit, &c);
		ai->eco->gameGuarding[unit] = target;
		ai->eco->removeIdleUnit(unit);
		const UnitDef *u = ai->call->GetUnitDef(unit);
		const UnitDef *t = ai->call->GetUnitDef(target);
		sprintf(buf, "[CMetaCommands::guard] %s guards %s", u->name.c_str(), t->name.c_str());
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
		sprintf(buf, "[CMetaCommands::repair] %s repairs %s", u->name.c_str(), t->name.c_str());
		LOGN(buf);
		return true;
	}
	return false;
}

bool CMetaCommands::build(int builder, UnitType *toBuild, float3 &pos) {
	const UnitDef *ud = ai->call->GetUnitDef(builder);
	float startRadius = ud->buildDistance;
	facing f           = getBestFacing(pos);
	float3 start       = ai->call->GetUnitPos(builder);
	float3 goal        = ai->call->ClosestBuildSite(toBuild->def, pos, startRadius, 7, f);

	while (goal == ERRORVECTOR) {
		startRadius += ud->buildDistance;
		goal = ai->call->ClosestBuildSite(toBuild->def, pos, startRadius, 7, f);
	}
	//ai->call->DrawUnit(toBuild->def->name.c_str(), goal, 0, 30*60*5, 0, true, true, f);

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
		sprintf(buf, "[CMetaCommands::build] %s builds %s", ud->name.c_str(), toBuild->def->name.c_str());
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
	return true;
}

bool CMetaCommands::wait(int unit) {
	assert(ai->call->GetUnitDef(unit) != NULL);
	Command c;
	c.id = CMD_WAIT;
	ai->call->GiveOrder(unit, &c);
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
	sprintf(buf, "[CMetaCommands::factoryBuild] %s builds %s", u->name.c_str(), ut->def->name.c_str());
	LOGN(buf);
	return true;
}

/* From KAIK */
Command CMetaCommands::createTargetCommand(int cmd, int target) {
	assert(ai->call->GetUnitDef(target) != NULL);

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
