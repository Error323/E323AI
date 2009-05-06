#include "TaskPlan.h"

CTaskPlan::CTaskPlan(AIClasses *ai) {
	this->ai = ai;
	previousFrame = 0;

	taskStr[BUILD_MMAKER]   = std::string("build_mmaker");
	taskStr[BUILD_EMAKER]   = std::string("build_emaker");
	taskStr[BUILD_FACTORY]  = std::string("build_factory");
	taskStr[ASSIST_FACTORY] = std::string("assist_factory");
	
}

void CTaskPlan::addTaskPlan(int unit, task t, int eta) {
	const UnitDef *ud = UD(unit);
	
	taskplans[unit] = new Plan(t, eta);
	sprintf(buf,"TaskPlan <%s, %s, %d>", ud->humanName.c_str(), taskStr[t].c_str(), eta);
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
			sprintf(buf,"TaskPlan %s removed", taskStr[p->t].c_str());
			LOGN(buf);
		}
	}
	
	for (unsigned int i = 0; i < erase.size(); i++)
		taskplans.erase(erase[i]);
	previousFrame = frame;
}

void CTaskPlan::getTasks(task t, std::vector<int> &units) {
	std::map<int, Plan*>::iterator i;
	for (i = taskplans.begin(); i != taskplans.end(); i++) {
		Plan *p = i->second;
		int unit = i->first;
		if (p->t == t)
			units.push_back(unit);
	}
}
