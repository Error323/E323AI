#ifndef MILITARY_H
#define MILITARY_H

#include "E323AI.h"

class CMilitary {
	public:
		CMilitary(AIClasses *ai);
		~CMilitary(){};

		/* initialize some unitids etc */
		void init(int unit);

		/* update callin */
		void update(int frame);

		/* add a unit to the current group */
		void addToGroup(int unit);

		/* remove a unit from its group, erase group when empty and not current */
		void removeFromGroup(int unit);

		/* military units */
		std::map<int, int> scouts;                  /* <unit id, unit id> */
		std::map<int, std::map<int, bool> > groups; /* <group id, <unit id, isactive> > */

	private:
		AIClasses *ai;

		UnitType *scout, *artie, *antiair, *factory;
		int currentGroup;
		float currentGroupStrength;

		/* Instantiates a new group */
		int createNewGroup();

		/* Request a random unit for building */
		UnitType* randomUnit();

		/* Get the position of a group */
		float3 getGroupPos(int group);
		
};

#endif
