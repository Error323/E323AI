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

		/* add a unit to a certain group */
		void addToGroup(int unit, groupType type);

		/* remove a unit from its group, erase group when empty and not 
		 * current
		 */
		void removeFromGroup(int unit);

		std::map<int, CMyGroup> groups; /* <group id, CMyGroup> */
		std::map<int, int>      units;  /* <unit id, group id> */

	private:
		AIClasses *ai;

		int scoutGroup;
		int attackerGroup;

		/* Instantiates a new group */
		void createNewGroup(groupType type);

		/* Scout and annoy >:) */
		int selectHarrasTarget(int group);

		/* All targets in a certain order */
		int selectAttackTarget(int group);

		/* Subfunction for select*Target */
		int selectTarget(float3 &ourPos, std::vector<int> &targets,
						 std::vector<int> &occupied);

		/* Request a random unit for building */
		unsigned randomUnit();

		char buf[1024];
};

#endif
