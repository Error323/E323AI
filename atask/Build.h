#ifndef E323_TASKBUILD_H
#define E323_TASKBUILD_H

#include "../ATask.h"

class UnitType;

struct BuildTask: public ATask {
	BuildTask(AIClasses *_ai): ATask(_ai) { t = TASK_BUILD; }
	BuildTask(AIClasses *_ai, buildType build, UnitType *toBuild, CGroup &group, float3 &pos);

	/* build type to string */
	static std::map<buildType, std::string> buildStr;
	/* The build task */
	buildType bt;
	/* The ETA in frames */
	unsigned int eta;
	/* The UnitType to build */
	UnitType *toBuild;

	/* overload */
	void onUpdate();
	/* overload */
	bool onValidate();
	/* overload */
	void toStream(std::ostream& out) const;

	bool assistable(CGroup &group, float &travelTime) const;
};

#endif
