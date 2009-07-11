#ifndef GROUP_H
#define GROUP_H

#include "E323AI.h"

class CGroup {
	public:
		CGroup(AIClasses *ai, int id);
		~CGroup(){};

		/* The group id */
		int id;

		/* The group strength */
		float strength;

		/* The group unit <id,iswaiting> */
		std::map<int, bool> units;

		/* Add a unit to the group */
		void add(int unit);

		/* Remove a unit from the group */
		void remove(int unit);

		/* Merge another group with this group */
		void merge(CGroup &group);

		/* Get the position of the group */
		float3 pos();

	private:
		AIClasses *ai;

};

#endif
