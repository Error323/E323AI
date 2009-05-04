#ifndef ECONOMY_H
#define ECONOMY_H

#include "E323AI.h"

enum task {
	INCREASE_METAL,
	INCREASE_ENERGY,
	ASSIST_WORKER,
	ASSIST_LAB
};

class CTaskPlan {
	public:
		CTaskPlan(AIClasses *ai);
		~CEconomy(){};

		struct Plan {
			unsigned int eta;
			task t;
		}
}

#endif
