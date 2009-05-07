#include "E323AI.h"

CE323AI::CE323AI() {
}

CE323AI::~CE323AI() {
	/* close the logfile */
	ai->logger->close();

	delete ai->metalMap;
	delete ai->unitTable;
	delete ai->metaCmds;
	delete ai->eco;
	delete ai->logger;
	delete ai->tasks;
	delete ai;
}

void CE323AI::InitAI(IGlobalAICallback* callback, int team) {
	ai          = new AIClasses();
	ai->call    = callback->GetAICallback();
	ai->cheat   = callback->GetCheatInterface();
	unitCreated = 0;

	/* Retrieve mapname, time and team info for the log file */
	std::string mapname = std::string(ai->call->GetMapName());
	mapname.resize(mapname.size() - 4);

	time_t now1;
	time(&now1);
	struct tm* now2 = localtime(&now1);

	std::sprintf(buf, "%s%s-%2.2d-%2.2d-%4.4d-%2.2d%2.2d-team(%d).log", 
		std::string(LOG_FOLDER).c_str(), mapname.c_str(), now2->tm_mon + 1, 
		now2->tm_mday, now2->tm_year + 1900, now2->tm_hour, now2->tm_min, team);

	printf("logfile @ %s\n", buf);

	ai->logger		= new std::ofstream(buf, std::ios::app);
	LOGN("v" << AI_VERSION);

	ai->metalMap	= new CMetalMap(ai);
	ai->unitTable	= new CUnitTable(ai);
	ai->metaCmds	= new CMetaCommands(ai);
	ai->eco     	= new CEconomy(ai);
	ai->tasks     	= new CTaskPlan(ai);

	ai->call->SendTextMsg("*** " AI_NOTES " ***", 0);
	ai->call->SendTextMsg("*** " AI_CREDITS " ***", 0);
	LOGN("");
	LOGN("");
	LOGN("*****************************************************************");
	LOGN("");
	LOGN("");
}


/************************
 * Unit related callins *
 ************************/

/* Called when units are spawned in a factory or when game starts */
void CE323AI::UnitCreated(int unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	UnitType *ut = UT(ud->id);
	unsigned int c = ut->cats;

	if (unitCreated == 1 && ud->isCommander)
		ai->eco->init(unit);

	if (c&MEXTRACTOR) {
		ai->metalMap->taken[unit] = ai->call->GetUnitPos(unit);
	}

	unitCreated++;
}

/* Called when units are finished in a factory and able to move */
void CE323AI::UnitFinished(int unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	UnitType* ut = UT(ud->id);
	unsigned int c = ut->cats;

	if (c&FACTORY) {
		ai->eco->gameFactories[unit] = ut;
		ai->eco->gameIdle[unit]           = ut;
		ai->tasks->updateBuildPlans(unit);
	}

	if (c&BUILDER && c&MOBILE) {
		ai->eco->gameBuilders[unit]  = ut;
	}

	if (c&MEXTRACTOR || c&MMAKER || c&MSTORAGE) {
		ai->eco->gameMetal[unit]     = ut;
		ai->tasks->updateBuildPlans(unit);
	}

	if (c&EMAKER || c& ESTORAGE) {
		ai->eco->gameEnergy[unit]    = ut;
		ai->tasks->updateBuildPlans(unit);
	}

	if (c&MOBILE) {
		float3 pos = ai->call->GetUnitPos(unit);
		int f = ai->metaCmds->getBestFacing(pos);
		float dist = (c&ATTACKER) ? 400.0f : 100.0f;
		switch(f) {
			case NORTH:
				pos.z -= dist;
			break;
			case SOUTH:
				pos.z += dist;
			break;
			case EAST:
				pos.x += dist;
			break;
			case WEST:
				pos.x -= dist;
			break;
		}
		ai->metaCmds->move(unit, pos);
	}

	ai->unitTable->gameAllUnits[unit] = ut;
}

/* Called on a destroyed unit, wanna take revenge? :P */
void CE323AI::UnitDestroyed(int unit, int attacker) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	UnitType* ut = UT(ud->id);
	unsigned int c = ut->cats;

	std::map<int, UnitType*> *map;
	std::map<int, UnitType*>::iterator i;

	if (c&FACTORY) {
		map = &(ai->eco->gameFactories);
	}

	if (c&BUILDER && c&MOBILE) {
		map = &(ai->eco->gameBuilders);
	}

	if (c&MEXTRACTOR || c&MMAKER || c&MSTORAGE) {
		map = &(ai->eco->gameMetal);
		if (c&MEXTRACTOR)
			ai->metalMap->taken.erase(unit);
	}

	if (c&EMAKER || c& ESTORAGE) {
		map = &(ai->eco->gameEnergy);
	}

	map->erase(unit);
	ai->eco->gameIdle.erase(unit);
	ai->eco->gameGuarding.erase(unit);
	ai->unitTable->gameAllUnits.erase(unit);
	ai->tasks->buildplans.erase(unit);
}

/* Called when unit is idle */
void CE323AI::UnitIdle(int unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	UnitType* ut = UT(ud->id);
	unsigned int c = ut->cats;

	if (c&BUILDER)
		ai->eco->gameIdle[unit] = ut;
}

/* Called when unit is damaged */
void CE323AI::UnitDamaged(int damaged, int attacker, float damage, float3 dir) {
}

/* Called on move fail e.g. can't reach point */
void CE323AI::UnitMoveFailed(int unit) {
}



/***********************
 * LOS related callins *
 ***********************/

void CE323AI::EnemyEnterLOS(int enemy) {
}

void CE323AI::EnemyLeaveLOS(int enemy) {
}

void CE323AI::EnemyEnterRadar(int enemy) {
}

void CE323AI::EnemyLeaveRadar(int enemy) {
}

void CE323AI::EnemyDestroyed(int enemy, int attacker) {
}

void CE323AI::EnemyDamaged(int damaged, int attacker, float damage, float3 dir) {
}


/****************
 * Misc callins *
 ****************/

void CE323AI::GotChatMsg(const char* msg, int player) {
}

int CE323AI::HandleEvent(int msg, const void* data) {
	return 0;
}


/* Update AI per logical frame = 1/30 sec on gamespeed 1.0 */
void CE323AI::Update() {
	int frame = ai->call->GetCurrentFrame();
	
	ai->eco->updateIncomes(frame);

	/* Rotate through the different update events to distribute computations */
	switch(frame % 4) {
		case 0: /* update threatmap */
		break;

		case 1: /* update enemy intel */
		break;

		case 2: /* update military */
		break;

		case 3: /* update economy */
			ai->eco->update(frame);
		break;
	}
}
