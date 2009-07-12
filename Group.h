#ifndef MYGROUP_H
#define MYGROUP_H

#include "E323AI.h"

class CMyGroup {
	public:
		enum groupType{WORKER, SCOUT, ATTACKER};

		CMyGroup(AIClasses *ai, int id, groupType type);
		~CMyGroup(){};

		/* The group id */
		int id;

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

};

#endif
