#include "CAI.h"

#include "AIExport.h"

std::vector<int> AIClasses::unitIDs;

AIClasses::AIClasses() {
	difficulty = DIFFICULTY_HARD;
	if (unitIDs.empty())
		unitIDs.resize(MAX_UNITS);
}

bool AIClasses::isMaster() {
	return aiexport_isPrimaryInstance(skirmishAIId);
}
