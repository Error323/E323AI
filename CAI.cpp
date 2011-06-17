#include "CAI.h"

#include "LegacyCpp/IGlobalAICallback.h"
#include "LegacyCpp/IAICallback.h"
#include "LegacyCpp/IAICheats.h"


std::vector<int> AIClasses::unitIDs;
std::map<int, AIClasses*> AIClasses::instances;
std::map<int, std::map<int, AIClasses*> > AIClasses::instancesByAllyTeam;

AIClasses::AIClasses(IGlobalAICallback* callback):difficulty(DIFFICULTY_HARD) {
	if (unitIDs.empty())
		unitIDs.resize(MAX_UNITS);

	cb = callback->GetAICallback();
	cbc = callback->GetCheatInterface();

	cbc->EnableCheatEvents(true);

	team = cb->GetMyTeam();
	allyTeam = cb->GetMyAllyTeam();

	updateAllyIndex();

	instances[team] = this;
	instancesByAllyTeam[allyTeam][team] = this;
}

AIClasses::~AIClasses() {
	instances.erase(team);
}

void AIClasses::updateAllyIndex() {
	allyIndex = 1;

	std::map<int, AIClasses*>::iterator it;
	for (it = instances.begin(); it != instances.end(); ++it) {
		if (it->first != team) {
			if (it->second->allyTeam == allyTeam)
				allyIndex++;
		}
	}
}
