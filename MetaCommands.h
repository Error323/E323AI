#ifndef METACOMMANDS_H
#define METACOMMANDS_H

#include "E323AI.h"

class CMetaCommands {
	public:
		CMetaCommands(AIClasses *ai);
		~CMetaCommands();

		/* Move unit with id uid to position pos */
		bool move(int uid, float3 &pos);

		/* Let unit with id uid build a unit with a certain facing (NORTH,
		 * SOUTH, EAST, WEST) at position pos */
		bool build(int uid, const UnitDef *ud, float3 &pos);

		bool factoryBuild(int factory, const UnitDef *ud);
		
	private:
		AIClasses *ai;
		char buf[1024];

		Command createPosCommand(int cmd, float3 pos, float radius = -1.0f, facing f = NONE);
		facing getBestFacing(float3 &pos);
};

#endif
