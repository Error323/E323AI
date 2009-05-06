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
	Command c;
	facing f;
	float pathLength, travelTime, buildTime, startRadius = 0.0f;
	float3 start, goal;
	const UnitDef *ud;
	int eta;
	task t;

	ud          = ai->call->GetUnitDef(builder);
	start       = ai->call->GetUnitPos(builder);
	f           = getBestFacing(pos);
	goal        = ai->call->ClosestBuildSite(toBuild->def, pos, startRadius, 5, f);
	while (goal == ERRORVECTOR) {
		startRadius += 100.0f;
		goal = ai->call->ClosestBuildSite(toBuild->def, pos, startRadius, 5, f);
	}
	pathLength  = ai->call->GetPathLength(start, goal, ud->movedata->pathType);
	pathLength -= std::min<float>(ud->buildDistance, pathLength);
	travelTime  = pathLength / (ud->speed/30.0f);
	buildTime   = toBuild->def->buildTime / (ud->buildSpeed/32.0f);
	eta         = travelTime + buildTime;

	if (toBuild->cats&FACTORY)
		t = BUILD_FACTORY;
	else if(toBuild->cats&MEXTRACTOR || toBuild->cats&MMAKER)
		t = BUILD_MMAKER;
	else if(toBuild->cats&EMAKER)
		t = BUILD_EMAKER;
	
	c = createPosCommand(-(toBuild->id), goal, -1.0f, f);
	if (c.id != 0) {
		ai->call->GiveOrder(builder, &c);
		ai->tasks->addTaskPlan(builder, t, eta);
		ai->call->CreateLineFigure(goal, float3(goal.x,goal.y+100, goal.z), 20, 1, eta, 0);
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

/* From KAIK */
facing CMetaCommands::getBestFacing(float3 &pos) {
	/* Introduce some diversity */
	int mapWidth = ai->call->GetMapWidth() * 8;
	int mapHeight = ai->call->GetMapHeight() * 8;
	quadrant mapQuadrant = NORTH_EAST;
	facing f = NONE;

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

	switch (mapQuadrant) {
		case NORTH_WEST: {
			f = (mapHeight > mapWidth) ? SOUTH: EAST;
		} break;
		case NORTH_EAST: {
			f = (mapHeight > mapWidth) ? SOUTH: WEST;
		} break;
		case SOUTH_WEST: {
			f = (mapHeight > mapWidth) ? NORTH: WEST;
		} break;
		case SOUTH_EAST: {
			f = (mapHeight > mapWidth) ? NORTH: EAST;
		} break;
	}

	return f;
}
