#include "CAI.h"

#include "../AI/Wrappers/LegacyCpp/AIGlobalAI.h"

extern std::map<int, CAIGlobalAI*> myAIs;

std::vector<int> AIClasses::unitIDs;

AIClasses::AIClasses() {
	difficulty = DIFFICULTY_HARD;
	if (unitIDs.empty())
		unitIDs.resize(MAX_UNITS);
}

bool AIClasses::isMaster() {
	return (myAIs.begin()->first == team);
}
