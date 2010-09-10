#include "CAI.h"

#include "AIExport.h"

AIClasses::AIClasses() {
	difficulty = DIFFICULTY_HARD;
	unitIDs.resize(MAX_UNITS);
}

bool AIClasses::isMaster() {
	return aiexport_isPrimaryInstance(skirmishAIId);
}
