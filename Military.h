#ifndef MILITARY_H
#define MILITARY_H

#include "E323AI.h"
#include "CGroup.h"

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

		/* remove a unit from its group, erase group when empty and not current
		 */
		void removeFromGroup(int unit);

		/* military units */
		std::map<int, CGroup> scouts; /* <group id, CGroup> */
		std::map<int, CGroup> groups; /* <group id, CGroup> */
		std::map<int, int>    units;  /* <unit id, group id> */

	private:
		AIClasses *ai;

		int currentGroup;
		int currentScout;

		/* Instantiates a new group */
		void createNewGroup(std::map<int, CGroup> &groups, int group);

		int selectHarrasTarget(int unit);

		int selectAttackTarget(int group);

		int selectTarget(float3 &ourPos, std::vector<int> &targets,
						 std::vector<int> &occupied);

		/* Request a random unit for building */
		unsigned randomUnit();

		char buf[1024];
};

#endif
