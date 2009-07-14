#ifndef E323AIGROUP_H
#define E323AIGROUP_H

#include "E323AI.h"

class CMyGroup {
	public:

		CMyGroup(AIClasses *ai, groupType type);
		CMyGroup(){};
		~CMyGroup(){};

		/* Group counter */
		static int counter;

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
		
		char buf[1024];


};

#endif
