#ifndef E323AIGROUP_H
#define E323AIGROUP_H

#include "E323AI.h"

/* NOTE: CMyGroup silently assumes that waterunits will not be merged with
 * non-water units as a group 
 */
class CMyGroup {
	public:

		CMyGroup(AIClasses *ai, groupType type);
		CMyGroup(){};
		~CMyGroup(){};

		/* Group counter */
		static int counter;

		/* The group id */
		int id;

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

		/* The group unit <id,iswaiting> */
		std::map<int, bool> units;

		/* Add a unit to the group */
		void add(int unit);

		/* Remove a unit from the group */
		void remove(int unit);

		/* Merge another group with this group */
		void merge(CMyGroup &group);

		/* Get the position of the group */
		float3 pos();

	private:
		AIClasses *ai;
		
		char buf[1024];


};

#endif
