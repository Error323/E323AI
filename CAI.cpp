#include "CAI.h"

#include "../AI/Wrappers/LegacyCpp/AIGlobalAI.h"

extern std::map<int, CAIGlobalAI*> myAIs;

AIClasses::AIClasses() {
	difficulty = DIFFICULTY_HARD;
	unitIDs.resize(MAX_UNITS);
}

bool AIClasses::isMaster() {
	return (myAIs.begin()->first == team);
}
