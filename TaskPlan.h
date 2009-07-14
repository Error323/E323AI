#ifndef TASKPLAN_H
#define TASKPLAN_H

#include "E323AI.h"

class CTaskPlan {
	public:
		CTaskPlan(AIClasses *ai);
		~CTaskPlan(){};

		struct BuildPlan {
			task t;
			UnitType *toBuild;

			BuildPlan(task t, UnitType *toBuild) {
				this->t       = t;
				this->toBuild = toBuild;
			}
		};

		struct MilitaryPlan {
			task t;
			int target;
			CMyGroup *G;
			MilitaryPlan(task t, int target, CMyGroup &G) {
				this->t      = t;
				this->target = target;
				this->G      = &G;
			}
		};
			

		void addMilitaryPlan(task t, CMyGroup &G, int target);

		void addBuildPlan(int unit, UnitType* toBuild);

		void updateBuildPlans(int unit);

		void updateMilitaryPlans();

		void getBuildTasks(task t, std::vector<int> &units);

		void getMilitaryTasks(task t, std::vector<int> &targets);

		int getTarget(int unitOrGroup);

		std::map<int, BuildPlan*> buildplans;
		std::map<int, MilitaryPlan*> militaryplans;

	private:
		std::map<task, std::string> taskStr;
		int previousFrame;
		char buf[1024];
		AIClasses *ai;
};

#endif
