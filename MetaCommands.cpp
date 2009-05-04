#include "MetaCommands.h"

CMetaCommands::CMetaCommands(AIClasses *ai) {
	this->ai = ai;
}

CMetaCommands::~CMetaCommands() {}

bool CMetaCommands::move(int uid, float3 &pos) {
	Command c = createPosCommand(CMD_MOVE, pos);
	
	if (c.id != 0) {
		ai->call->GiveOrder(uid, &c);
		return true;
	}
	return false;
}

bool CMetaCommands::build(int uid, const UnitDef *ud, float3 &pos) {
	facing f = getBestFacing(pos);
	//float3 realpos = ai->call->ClosestBuildSite(ud, pos, 5.0f, 2, f);
	Command c = createPosCommand(-(ud->id), pos, -1.0f, f);

	if (c.id != 0) {
		ai->call->GiveOrder(uid, &c);
		ai->eco->removeIdleUnit(uid);
		return true;
	}
	return false;
}

bool CMetaCommands::factoryBuild(int factory, const UnitDef *ud) {
	Command c;
	c.id = -(ud->id);
	ai->call->GiveOrder(factory, &c);
	ai->eco->removeIdleUnit(factory);
	return true;
}

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

facing CMetaCommands::getBestFacing(float3 &pos) {
	return NONE;
}
