#ifndef E323_TASKBUILD_H
#define E323_TASKBUILD_H

#include "../ATask.h"

struct UnitType;

struct BuildTask: public ATask {
	BuildTask(AIClasses *_ai): ATask(_ai) { t = TASK_BUILD; }
	BuildTask(AIClasses *_ai, buildType build, UnitType *toBuild, CGroup &group, float3 &pos);

	/* Build type to string */
	static std::map<buildType, std::string> buildStr;
	
	bool building;
	/* The build task */
	buildType bt;
	/* The ETA in frames */
	unsigned int eta;
	/* The UnitType to build */
	UnitType* toBuild;

	/* overload */
	void onUpdate();
	/* overload */
	bool onValidate();
	/* overload */
	void toStream(std::ostream& out) const;
	/* overload */
	void onUnitDestroyed(int uid, int attacker);

	bool assistable(CGroup &group, float &travelTime) const;
};

#endif
