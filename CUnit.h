#ifndef CUNIT_H
#define CUNIT_H

#include <map>
#include <iostream>

#include "ARegistrar.h"
#include "headers/HEngine.h"
#include "headers/Defines.h"

class AIClasses;
class UnitType;
class CGroup;

/* Building facings, NOTE: this order is important! */
enum facing{ SOUTH, EAST, NORTH, WEST, NONE };

/* Map quadrants */
enum quadrant { NORTH_WEST, NORTH_EAST, SOUTH_WEST, SOUTH_EAST };

class CUnit: public ARegistrar {
	public:
		CUnit(): ARegistrar() {
			def = NULL;
			type = NULL;
			builtBy = 0;
		}
		CUnit(AIClasses *ai, int uid, int bid): ARegistrar(uid) {
			this->ai = ai;
			reset(uid, bid);
		}
		~CUnit(){}

		const UnitDef *def;
		UnitType *type;
		int builtBy;
		int techlvl;
		CGroup *group; // group unit belongs to

		/* Remove the unit from everywhere registered */
		void remove();

		/* Overloaded */
		void remove(ARegistrar &reg);

		/* Reset this object */
		void reset(int uid, int bid);

		int queueSize();

		/* Determine if this unit belongs to economic tracker */
		bool isEconomy();

		/* Attack a unit */
		bool attack(int target, bool enqueue = false);

		/* Move a unit forward by dist */
		bool moveForward(float dist, bool enqueue = false);

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

		/* Cloak a unit */
		bool cloak(bool on);

		/* Guard a certain unit */
		bool guard(int target, bool enqueue = false);

		/* Stop doing what you did */
		bool stop();

		/* Wait with what you are doing */
		bool wait();

		bool reclaim(float3 pos, float radius);
		bool reclaim(int target, bool enqueue = false);

		/* Undo wait command */
		bool unwait();

		bool waiting;

		bool micro(bool on);

		bool isMicroing();

		bool isOn();

		float3 pos();

		/* Get best facing */
		facing getBestFacing(float3 &pos);

		/* Get quadrant */
		quadrant getQuadrant(float3 &pos);

		RegistrarType regtype() { return REG_UNIT; } 

		static bool isMilitary(const UnitDef* ud) { return !ud->weapons.empty(); }
		static bool isStatic(const UnitDef* ud) { return ud->speed < 0.0001f; }
		static bool isDefense(const UnitDef* ud) { return isStatic(ud) && isMilitary(ud); }

		/* output stream */
		friend std::ostream& operator<<(std::ostream &out, const CUnit &unit);

		AIClasses *ai;

	private:
		bool microing;

		Command createPosCommand(int cmd, float3 pos, float radius = -1.0f, facing f = NONE);
		Command createTargetCommand(int cmd, int target);
};

#endif
