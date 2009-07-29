#ifndef CGROUP_H
#define CGROUP_H

#include "CE323AI.h"
#include "ARegistrar.h"

/* NOTE: CGroup silently assumes that waterunits will not be merged with
 * non-water units as a group
 */
class CGroup: public ARegistrar {
	public:

		CGroup(AIClasses *ai, groupType type): ARegistrar(counter);
		CGroup(){};
		~CGroup(){};

		/* Group counter */
		static int counter;

		/* movetype, the movetype with the smallest slope */
		int moveType;

		/* corresponding maxSlope */
		float maxSlope;

		/* The group type */
		groupType type;

		/* The group strength */
		float strength;

		/* The group maxrange */
		float range;

		/* Is this group busy? */
		bool busy;

		/* Remove this group, preserve units */
		void remove();

		/* Overload */
		void remove(ARegistrar &unit);

		/* Reset for object re-usage */
		void reset(groupType type);

		/* The waiters <id,iswaiting> */
		std::map<int, bool> waiters;

		/* The units <id, CUnit*> */
		std::map<int, CUnit*> units;

		/* Add a unit to the group */
		void addUnit(CUnit &unit);

		/* Merge another group with this group */
		void merge(CGroup &group);

		/* Get the position of the group */
		float3 pos();

		/* Get the maximal lateral dispersion */
		int maxLength();

	private:
		AIClasses *ai;
		
		char buf[256];


};

#endif
