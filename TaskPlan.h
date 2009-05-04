#ifndef TASKPLAN_H
#define TASKPLAN_H

#include "E323AI.h"

class CTaskPlan {
	public:
		CTaskPlan(AIClasses *ai);
		~CTaskPlan(){};

		struct Plan {
			int eta;
			task t;

			Plan(task t, int eta) {
				this->t   = t;
				this->eta = eta;
			}
		};

		void addTaskPlan(int unit, task t, int eta);

		void update(int frame);


	private:
		std::map<int, Plan*> taskplans;
		std::map<task, char*> taskStr;
		int previousFrame;
		char buf[1024];
		AIClasses *ai;
};

#endif
