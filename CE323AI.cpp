#include "CE323AI.h"

CE323AI::CE323AI() {
}

CE323AI::~CE323AI() {
	/* Print ScopedTimer times */
	ai->l->v(ScopedTimer::profile());

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
	delete ai->l;
	delete ai;
}

void CE323AI::InitAI(IGlobalAICallback* callback, int team) {
	ai          = new AIClasses();
	ai->call    = callback->GetAICallback();
	ai->cheat   = callback->GetCheatInterface();
	ai->team    = team;
	this->team  = team;

	/* Retrieve mapname, time and team info for the log file */
	std::string mapname = std::string(ai->call->GetMapName());
	mapname.resize(mapname.size() - 4);

	time_t now1;
	time(&now1);
	struct tm* now2 = localtime(&now1);

	std::string version("*** " + AI_VERSION + " ***");
	LOGS(version.c_str());
	LOGS("*** " AI_CREDITS " ***");
	LOGS("*** " AI_NOTES " ***");

	ai->metalMap	= new CMetalMap(ai);
	ai->unitTable	= new CUnitTable(ai);
	ai->eco     	= new CEconomy(ai);
	ai->wl          = new CWishList(ai);
	ai->tasks     	= new CTaskHandler(ai);
	ai->threatMap   = new CThreatMap(ai);
	ai->pf          = new CPathfinder(ai);
	ai->intel       = new CIntel(ai);
	ai->military    = new CMilitary(ai);
	ai->l           = new CLogger(ai, /*CLogger::LOG_SCREEN |*/ CLogger::LOG_FILE);
}


/************************
 * Unit related callins *
 ************************/

/* Called when units are spawned in a factory or when game starts */
void CE323AI::UnitCreated(int uid, int bid) {
	CUnit *unit    = ai->unitTable->requestUnit(uid, bid);

	LOG_II("CE323AI::UnitCreated " << (*unit))

	unsigned int c = unit->type->cats;
	if (unit->def->isCommander) {
		ai->eco->init(*unit);
		return;
	}

	if (c&MEXTRACTOR)
		ai->metalMap->addUnit(*unit);

	if (c&ATTACKER && c&MOBILE)
		unit->moveForward(400.0f);

	if (c&BUILDER && c&MOBILE)
		unit->moveForward(-100.0f);
}

/* Called when units are finished in a factory and able to move */
void CE323AI::UnitFinished(int uid) {
	CUnit *unit = ai->unitTable->getUnit(uid);
	ai->unitTable->idle[uid] = true;

	LOG_II("CE323AI::UnitFinished " << (*unit))

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
	LOG_II("CE323AI::UnitDestroyed " << (*unit))
	unit->remove();
}

/* Called when unit is idle */
void CE323AI::UnitIdle(int uid) {
	CUnit *unit = ai->unitTable->getUnit(uid);
	LOG_II("CE323AI::UnitIdle " << (*unit))
	ai->unitTable->idle[uid] = true;
}

/* Called when unit is damaged */
void CE323AI::UnitDamaged(int damaged, int attacker, float damage, float3 dir) {
}

/* Called on move fail e.g. can't reach point */
void CE323AI::UnitMoveFailed(int uid) {
	CUnit *unit = ai->unitTable->getUnit(uid);
	LOG_WW("CE323AI::UnitMoveFailed " << (*unit))
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
	CUnit *unit = ai->unitTable->getUnit(attacker);
	UnitType *enem = UT(enemy);
	LOG_II("CE323AI::EnemyDestroyed " << enem->def->humanName << "(" << enemy << ")" << (*unit))
}

void CE323AI::EnemyDamaged(int damaged, int attacker, float damage, float3 dir) {
}


/****************
 * Misc callins *
 ****************/

void CE323AI::GotChatMsg(const char* msg, int player) {
}

int CE323AI::HandleEvent(int msg, const void* data) {
	const ChangeTeamEvent* cte = (const ChangeTeamEvent*) data;
	switch(msg) {
		case AI_EVENT_UNITGIVEN:
			/* Unit gained */
			if ((cte->newteam) == (ai->call->GetMyTeam())) {
				UnitFinished(cte->unit);
				CUnit *unit = ai->unitTable->getUnit(cte->unit);

				LOG_II("CE323AI::UnitGiven " << (*unit))
			}
		break;

		case AI_EVENT_UNITCAPTURED:
			/* Unit lost */
			if ((cte->oldteam) == (ai->call->GetMyTeam())) {
				UnitDestroyed(cte->unit, 0);
				CUnit *unit = ai->unitTable->getUnit(cte->unit);

				LOG_II("CE323AI::UnitCaptured " << (*unit))
			}
		break;
	}
	return 0;
}


/* Update AI per logical frame = 1/30 sec on gamespeed 1.0 */
void CE323AI::Update() {
	int frame = ai->call->GetCurrentFrame()+team;
	/* Don't act before the 100th frame, messed up eco stuff :P */
	if (frame < 90) return;
	int groupsize = (frame / (30*60*2))+1;

	/* Rotate through the different update events to distribute computations */
	switch(frame % 9) {
		case 0: { /* update incomes */
			ScopedTimer t(std::string("eco-incomes"));
			ai->eco->updateIncomes();
		}
		break;

		case 1: { /* update threatmap */
			ScopedTimer t(std::string("threatmap"));
			ai->threatMap->update(frame);
		}
		break;

		case 2: { /* update pathfinder with threatmap */
			ScopedTimer t(std::string("pf-map"));
			ai->pf->updateMap(ai->threatMap->map);
		}
		break;

		case 3: { /* update the path itself of a group */
			ScopedTimer t(std::string("pf-grouppath"));
			ai->pf->updatePaths();
		}
		break;

		case 4: { /* update the groups following a path */
			ScopedTimer t(std::string("pf-followers"));
			ai->pf->updateFollowers();
		}
		break;

		case 5: { /* update enemy intel */
			ScopedTimer t(std::string("intel"));
			ai->intel->update(frame);
		}
		break;

		case 6: { /* update military */
			ScopedTimer t(std::string("military"));
			ai->military->update(groupsize);
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
