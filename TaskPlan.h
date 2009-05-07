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

		void addBuildPlan(int unit, UnitType* toBuild);

		void updateBuildPlans(int unit);

		void getTasks(task t, std::vector<int> &units);

		std::map<int, BuildPlan*> buildplans;

	private:
		std::map<task, std::string> taskStr;
		int previousFrame;
		char buf[1024];
		AIClasses *ai;
};

#endif
