#ifndef METACOMMANDS_H
#define METACOMMANDS_H

#include "E323AI.h"

class CMetaCommands {
	public:
		CMetaCommands(AIClasses *ai);
		~CMetaCommands();

		/* Move a unit forward by dist */
		void moveForward(int unit, float dist);

		/* Move beside base */
		void moveBesidesBase(int unit);

		/* Move unit with id uid to position pos */
		bool move(int uid, float3 &pos, bool enqueue = false);

		/* Let unit with id uid build a unit with a certain facing (NORTH,
		 * SOUTH, EAST, WEST) at position pos */
		bool build(int builder, UnitType *toBuild, float3 &pos);

		/* Build a unit in a certain factory */
		bool factoryBuild(int factory, UnitType *toBuild);

		/* Repair (or assist) a certain unit */
		bool repair(int unit, int target);

		/* Guard a certain unit */
		bool guard(int unit, int target, bool enqueue = true);

		/* Stop doing what you did */
		bool stop(int unit);

		/* Wait with what you are doing */
		bool wait(int unit);

		/* Get best facing */
		facing getBestFacing(float3 &pos);

		/* Get quadrant */
		quadrant getQuadrant(float3 &pos);

	private:
		AIClasses *ai;
		char buf[1024];

		Command createPosCommand(int cmd, float3 pos, float radius = -1.0f, facing f = NONE);
		Command createTargetCommand(int cmd, int target);
};

#endif
