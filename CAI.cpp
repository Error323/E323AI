#include "CAI.h"

#include "AIExport.h"

AIClasses::AIClasses() {
	unitIDs.resize(MAX_UNITS);
}

bool AIClasses::isMaster() {
	return aiexport_isPrimaryInstance(skirmishAIId);
}
