#include "CAI.h"

#include "../AI/Wrappers/LegacyCpp/AIGlobalAI.h"

extern std::map<int, CAIGlobalAI*> myAIs;

AIClasses::AIClasses() {
	unitIDs.resize(MAX_UNITS);
}

bool AIClasses::isMaster() {
	return (myAIs.begin()->first == team);
}
