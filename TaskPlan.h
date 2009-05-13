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
			MilitaryPlan(task t, int target) {
				this->t = t;
				this->target = target;
			}
		};
			

		void addMilitaryPlan(task t, int unitOrGroup, int target);

		void addBuildPlan(int unit, UnitType* toBuild);

		void updateBuildPlans(int unit);

		void getBuildTasks(task t, std::vector<int> &units);

		void getMilitaryTasks(task t, std::vector<int> &targets);

		std::map<int, BuildPlan*> buildplans;
		std::map<int, MilitaryPlan*> militaryplans;

	private:
		std::map<task, std::string> taskStr;
		int previousFrame;
		char buf[1024];
		AIClasses *ai;
};

#endif
