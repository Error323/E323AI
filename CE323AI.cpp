#include "CE323AI.h"

CE323AI::CE323AI() {
}

CE323AI::~CE323AI() {
	/* Print ScopedTimer times */
	std::string s = ScopedTimer::profile();
	printf("%s", s.c_str());

	/* close the logfile */
	ai->logger->close();

	delete ai->metalMap;
	delete ai->unitTable;
	delete ai->eco;
	delete ai->logger;
	delete ai->tasks;
	delete ai->threatMap;
	delete ai->pf;
	delete ai->intel;
	delete ai->military;
	delete ai->wl;
	delete ai;
}

void CE323AI::InitAI(IGlobalAICallback* callback, int team) {
	ai          = new AIClasses();
	ai->call    = callback->GetAICallback();
	ai->cheat   = callback->GetCheatInterface();

	/* Retrieve mapname, time and team info for the log file */
	std::string mapname = std::string(ai->call->GetMapName());
	mapname.resize(mapname.size() - 4);

	time_t now1;
	time(&now1);
	struct tm* now2 = localtime(&now1);

	sprintf(buf, "%s", LOG_FOLDER);
	ai->call->GetValue(AIVAL_LOCATE_FILE_W, buf);
	std::sprintf(
		buf, "%s%2.2d%2.2d%2.2d%2.2d%2.2d-%s-team-%d.log", 
		std::string(LOG_PATH).c_str(), 
		now2->tm_year + 1900, 
		now2->tm_mon + 1, 
		now2->tm_mday, 
		now2->tm_hour, 
		now2->tm_min, 
		mapname.c_str(), 
		team
	);
	ai->logger		= new std::ofstream(buf, std::ios::app);

	std::string version("*** " + AI_VERSION + " ***");
	LOGS(version.c_str());
	LOGS("*** " AI_CREDITS " ***");
	LOGS("*** " AI_NOTES " ***");
	LOGS(buf);
	LOGN(AI_VERSION);

	ai->metalMap	= new CMetalMap(ai);
	ai->unitTable	= new CUnitTable(ai);
	ai->eco     	= new CEconomy(ai);
	ai->wl          = new CWishList(ai);
	ai->tasks     	= new CTaskHandler(ai);
	ai->threatMap   = new CThreatMap(ai);
	ai->pf          = new CPathfinder(ai);
	ai->intel       = new CIntel(ai);
	ai->military    = new CMilitary(ai);

	LOG("\n\n\nBEGIN\n\n\n");
}


/************************
 * Unit related callins *
 ************************/

/* Called when units are spawned in a factory or when game starts */
void CE323AI::UnitCreated(int uid, int bid) {
	CUnit *unit    = ai->unitTable->requestUnit(uid, bid);

	sprintf(buf, "[CE323AI::UnitCreated]\t%s(%d) created", unit->def->humanName.c_str(), uid);
	LOGN(buf);

	unsigned int c = unit->type->cats;
	if (unit->def->isCommander)
		ai->eco->init(*unit);

	if (c&MEXTRACTOR)
		ai->metalMap->addUnit(*unit);
}

/* Called when units are finished in a factory and able to move */
void CE323AI::UnitFinished(int uid) {
	CUnit *unit = ai->unitTable->getUnit(uid);

	sprintf(buf, "[CE323AI::UnitFinished]\t%s(%d) finished", unit->def->humanName.c_str(), uid);
	LOGN(buf);

	unsigned int c = unit->type->cats;
	if (unit->builder > 0)
		ai->unitTable->builders[unit->builder] = true;

	/* Eco unit */
	if (!(c&ATTACKER) || c&COMMANDER)
		ai->eco->addUnit(*unit);
	/* Military unit */
	else
		ai->military->addUnit(*unit);

}

/* Called on a destroyed unit */
void CE323AI::UnitDestroyed(int uid, int attacker) {
	CUnit *unit = ai->unitTable->getUnit(uid);
	sprintf(buf, "[CE323AI::UnitDestroyed]\t%s(%d) destroyed", unit->def->humanName.c_str(), uid);
	LOGN(buf);
	unit->remove();
}

/* Called when unit is idle */
void CE323AI::UnitIdle(int uid) {
	CUnit *unit = ai->unitTable->getUnit(uid);
	sprintf(buf, "[CE323AI::UnitIdle]\t%s(%d) idling", unit->def->humanName.c_str(), uid);
	LOGN(buf);
}

/* Called when unit is damaged */
void CE323AI::UnitDamaged(int damaged, int attacker, float damage, float3 dir) {
}

/* Called on move fail e.g. can't reach point */
void CE323AI::UnitMoveFailed(int uid) {
	CUnit *unit = ai->unitTable->getUnit(uid);
	unit->moveRandom(100.0f);
	sprintf(buf, "[CE323AI::UnitMoveFailed]\t%s(%d) failed moving", unit->def->humanName.c_str(), uid);
	LOGN(buf);
}



/***********************
 * Enemy related callins *
 ***********************/

void CE323AI::EnemyEnterLOS(int enemy) { }

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
	LOGS("I don't like smalltalk, just lose plz xD");
}

int CE323AI::HandleEvent(int msg, const void* data) {
	const ChangeTeamEvent* cte = (const ChangeTeamEvent*) data;
	switch(msg) {
		case AI_EVENT_UNITGIVEN:
			/* Unit gained */
			if ((cte->newteam) == (ai->call->GetMyTeam())) {
				UnitFinished(cte->unit);
				sprintf(buf, "[CE323AI::HandleEvent]\tGiven/Gained unit(%d)", cte->unit);
				LOGN(buf);
			}
		break;

		case AI_EVENT_UNITCAPTURED:
			/* Unit lost */
			if ((cte->oldteam) == (ai->call->GetMyTeam())) {
				UnitDestroyed(cte->unit, 0);
				sprintf(buf, "[CE323AI::HandleEvent]\tCaptured/Lost unit(%d)", cte->unit);
				LOGN(buf);
			}
		break;
	}
	return 0;
}


/* Update AI per logical frame = 1/30 sec on gamespeed 1.0 */
void CE323AI::Update() {
	int frame = ai->call->GetCurrentFrame();

	/* Rotate through the different update events to distribute computations */
	switch(frame % 9) {
		case 0: { /* update threatmap */
			ScopedTimer t(std::string("threatmap"));
			ai->threatMap->update(frame);
		}
		break;

		case 1: { /* update pathfinder with threatmap */
			ScopedTimer t(std::string("pf-map"));
			ai->pf->updateMap(ai->threatMap->map);
		}
		break;

		case 2: { /* update the path itself of a group */
			ScopedTimer t(std::string("pf-grouppath"));
			ai->pf->updatePaths();
		}
		break;

		case 3: { /* update the groups following a path */
			ScopedTimer t(std::string("pf-followers"));
			ai->pf->updateFollowers();
		}
		break;

		case 4: { /* update enemy intel */
			ScopedTimer t(std::string("intel"));
			ai->intel->update(frame);
		}
		break;

		case 5: { /* update military */
			ScopedTimer t(std::string("military"));
			ai->military->update(frame);
		}
		break;

		case 6: { /* update incomes */
			ScopedTimer t(std::string("eco-incomes"));
			ai->eco->updateIncomes(frame);
		}
		break;

		case 7: { /* update economy */
			ScopedTimer t(std::string("eco-update"));
			ai->eco->update(frame);
		}
		break;

		case 8: { /* update taskhandler */
			ScopedTimer t(std::string("tasks"));
			ai->tasks->update();
		}
		break;

		default: return;
	}
}
