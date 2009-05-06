#include "TaskPlan.h"

CTaskPlan::CTaskPlan(AIClasses *ai) {
	this->ai = ai;
	previousFrame = 0;

	taskStr[BUILD_MMAKER]   = "build_mmaker";
	taskStr[BUILD_EMAKER]   = "build_emaker";
	taskStr[BUILD_FACTORY]  = "build_factory";
	taskStr[ASSIST_FACTORY] = "assist_factory";
	
}

void CTaskPlan::addTaskPlan(int unit, task t, int eta) {
	const UnitDef *ud = UD(unit);
	
	taskplans[unit] = new Plan(t, eta);
	sprintf(buf,"TaskPlan <%s, %s, %d>", ud->humanName.c_str(), taskStr[t], eta);
	LOGS(buf);
	LOGN(buf);
}

void CTaskPlan::update(int frame) {
	std::map<int, Plan*>::iterator i;
	std::vector<int> erase;

	int diff = frame - previousFrame;

	for (i = taskplans.begin(); i != taskplans.end(); i++) {
		Plan *p = i->second;
		p->eta -= diff;

		if (p->eta <= 0) {
			erase.push_back(i->first);
			sprintf(buf,"TaskPlan %s removed", taskStr[p->t]);
			LOGS(buf);
			LOGN(buf);
		}
	}
	
	for (unsigned int i = 0; i < erase.size(); i++)
		taskplans.erase(erase[i]);
	previousFrame = frame;
}
