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

		/* Get the position of a group */
		float3 getGroupPos(int group);

		/* military units */
		std::map<int, bool>                 scouts;   /* <unit id, isharrasing> */
		std::map<int, std::map<int, bool> > groups;   /* <group id, <unit id, isactive> > */
		std::map<int, int>                  units;    /* <unit id, group id> */
		std::map<int, float>                strength; /* <group id, strength> */

	private:
		AIClasses *ai;

		UnitType *scout, *artie, *antiair, *factory;
		int currentGroup;
		float currentGroupStrength;

		/* Instantiates a new group */
		void createNewGroup();

		int selectHarrasTarget(int unit);

		int selectAttackTarget(int group);

		int selectTarget(float3 &ourPos, std::vector<int> &targets, std::vector<int> &occupied);

		/* Request a random unit for building */
		unsigned randomUnit();

		char buf[1024];
};

#endif
