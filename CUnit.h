#ifndef CUNIT_H
#define CUNIT_H

#include <map>
#include <iostream>

#include "ARegistrar.h"
#include "headers/HEngine.h"

class AIClasses;
class UnitType;

/* Building facings, NOTE: this order is important! */
enum facing{SOUTH, EAST, NORTH, WEST, NONE};

/* Map quadrants */
enum quadrant {NORTH_WEST, NORTH_EAST, SOUTH_WEST, SOUTH_EAST};

class CUnit: public ARegistrar {
	public:
		CUnit(): ARegistrar() {}
		CUnit(AIClasses *ai, int uid, int bid): ARegistrar(uid) {
			this->ai = ai;
			reset(uid, bid);
		}
		~CUnit(){}

		const UnitDef *def;
		UnitType *type;
		int   builder;

		/* Remove the unit from everywhere registered */
		void remove();

		/* Overloaded */
		void remove(ARegistrar &reg);

		/* Reset this object */
		void reset(int uid, int bid);

		int queueSize();

		/* Attack a unit */
		bool attack(int target);

		/* Move a unit forward by dist */
		bool moveForward(float dist, bool enqueue = true);

		/* Move random */
		bool moveRandom(float radius, bool enqueue = false);

		/* Move unit with id uid to position pos */
		bool move(float3 &pos, bool enqueue = false);

		/* Set a unit (e.g. mmaker) on or off */
		bool setOnOff(bool on);

		/* Let unit with id uid build a unit with a certain facing (NORTH,
		 * SOUTH, EAST, WEST) at position pos 
		 */
		bool build(UnitType *toBuild, float3 &pos);

		/* Build a unit in a certain factory */
		bool factoryBuild(UnitType *toBuild, bool enqueue = false);

		/* Repair (or assist) a certain unit */
		bool repair(int target);

		/* Guard a certain unit */
		bool guard(int target, bool enqueue = true);

		/* Stop doing what you did */
		bool stop();

		/* Wait with what you are doing */
		bool wait();

		/* Undo wait command */
		bool unwait();

		bool waiting;

		float3 pos();

		/* Get best facing */
		facing getBestFacing(float3 &pos);

		/* Get quadrant */
		quadrant getQuadrant(float3 &pos);

		/* output stream */
		friend std::ostream& operator<<(std::ostream &out, const CUnit &unit);

	private:
		AIClasses *ai;

		Command createPosCommand(int cmd, float3 pos, float radius = -1.0f, facing f = NONE);
		Command createTargetCommand(int cmd, int target);
};

#endif
