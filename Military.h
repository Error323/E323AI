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
		void addToGroup(int factory, int unit, groupType type);

		/* remove a unit from its group, erase group when empty and not 
		 * current
		 */
		void removeFromGroup(int factory, int unit);

		/* initializes groups[factory] subgroups */
		void initSubGroups(int factory);

		/* factory->group->unit <factory, <group, CMyGroup> > */
		std::map<int, std::map<int, CMyGroup> > groups;

		/* factory->unit->group <factory, <unit, group> > */
		std::map<int, std::map<int, int> >      units;

	private:
		AIClasses *ai;

		/* the current group per factory */
		std::map<int, int> attackGroup;

		/* the current scout per factory */
		std::map<int, int> scoutGroup;

		/* Instantiates a new group */
		void createNewGroup(groupType type, int factory);

		/* Scout and annoy >:) */
		int selectHarrasTarget(CMyGroup &G);

		/* All targets in a certain order */
		int selectAttackTarget(CMyGroup &G);

		/* Subfunction for select*Target */
		int selectTarget(float3 &ourPos, std::vector<int> &targets,
						 std::vector<int> &occupied);

		/* Group size (while respecting the enemy strength) */
		int groupSize;
		/* Minimal group size */
		int minGroupSize;
		/* Maximal group size */
		int maxGroupSize;

		/* Request a random unit for building */
		unsigned randomUnit();

		char buf[1024];
};

#endif
