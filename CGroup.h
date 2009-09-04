#ifndef CGROUP_H
#define CGROUP_H

#include <map>

#include "headers/HEngine.h"
#include "ARegistrar.h"

class ATask;
class CUnit;
class AIClasses;
class UnitType;

/* NOTE: CGroup silently assumes that waterunits will not be merged with
 * non-water units as a group, aswell as builders with attackers
 */
class CGroup: public ARegistrar {
	public:

		CGroup(AIClasses *ai): ARegistrar(counter) {
			this->ai = ai;
			reset();
			counter++;
		}
		CGroup(){};
		~CGroup(){};

		/* Group counter */
		static int counter;

		/* movetype, the movetype with the smallest slope */
		int moveType;

		/* corresponding maxSlope */
		float maxSlope;

		/* The group strength */
		float strength;

		/* The group's moveSpeed */
		float speed;

		/* The group's buildSpeed */
		float buildSpeed;

		/* The group maxrange */
		float range, buildRange;

		/* Is this group busy? */
		bool busy;

		/* Remove this group, preserve units */
		void remove();

		/* Overload */
		void remove(ARegistrar &unit);

		/* Reset for object re-usage */
		void reset();

		/* The waiters <id,iswaiting> */
		std::map<int, bool> waiters;

		/* The units <id, CUnit*> */
		std::map<int, CUnit*> units;

		/* Add a unit to the group */
		void addUnit(CUnit &unit);

		/* Merge another group with this group */
		void merge(CGroup &group);

		/* See if the entire group is idle */
		bool isIdle();

		/* Get the position of the group */
		float3 pos();

		void assist(ATask &task);
		void attack(int target);
		void build(float3 &pos, UnitType *ut);
		void stop();
		void move(float3 &pos, bool enqueue = false);
		void guard(int target, bool enqueue = false);
		void wait();

		/* Get the maximal lateral dispersion */
		int maxLength();

		/* output stream */
		friend std::ostream& operator<<(std::ostream &out, const CGroup &group);

	private:
		AIClasses *ai;
};

#endif
