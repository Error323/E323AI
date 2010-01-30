#include "CE323AI.h"

#include "CAI.h"
#include "CConfigParser.h"
#include "CMetalMap.h"
#include "CUnitTable.h"
#include "CEconomy.h"
#include "CWishList.h"
#include "CTaskHandler.h"
#include "CThreatMap.h"
#include "CPathfinder.h"
#include "CIntel.h"
#include "CMilitary.h"
#include "CDefenseMatrix.h"
#include "CUnit.h"
#include "CScopedTimer.h"

void CE323AI::InitAI(IGlobalAICallback* callback, int team) {
	ai                = new AIClasses();
	ai->cb            = callback->GetAICallback();
	ai->cbc           = callback->GetCheatInterface();
	ai->team          = team;
	ai->logger        = new CLogger(ai, CLogger::LOG_SCREEN | CLogger::LOG_FILE);
	ai->cfgparser     = new CConfigParser(ai);
	ai->metalmap      = new CMetalMap(ai);
	ai->unittable     = new CUnitTable(ai);
	ai->economy       = new CEconomy(ai);
	ai->wishlist      = new CWishList(ai);
	ai->tasks         = new CTaskHandler(ai);
	ai->threatmap     = new CThreatMap(ai);
	ai->pathfinder    = new CPathfinder(ai);
	ai->intel         = new CIntel(ai);
	ai->military      = new CMilitary(ai);
	ai->defensematrix = new CDefenseMatrix(ai);

	std::string configfile(ai->cb->GetModName());
	configfile = configfile.substr(0, configfile.size()-4) + "-config.cfg";
	if (!ai->cfgparser->parseConfig(configfile))
		ai->cfgparser->parseConfig(configfile);
}


void CE323AI::ReleaseAI() {
	LOG_II(CScopedTimer::profile())

	delete ai->defensematrix;
	delete ai->military;
	delete ai->intel;
	delete ai->pathfinder;
	delete ai->threatmap;
	delete ai->tasks;
	delete ai->wishlist;
	delete ai->economy;
	delete ai->unittable;
	delete ai->metalmap;
	delete ai->cfgparser;
	delete ai->logger;
	delete ai;
}

/************************
 * Unit related callins *
 ************************/

/* Called when units are spawned in a factory or when game starts */
void CE323AI::UnitCreated(int uid, int bid) {
	CUnit *unit = ai->unittable->requestUnit(uid, bid);

	LOG_II("CE323AI::UnitCreated " << (*unit))

	unsigned int c = unit->type->cats;
	if (unit->def->isCommander) {
		ai->economy->init(*unit);
		return;
	}

	if (c&MEXTRACTOR)
		ai->metalmap->addUnit(*unit);

	if (c&ATTACKER && c&MOBILE)
		unit->moveForward(400.0f);

	if (c&BUILDER && c&MOBILE)
		unit->moveForward(200.0f);
}

/* Called when units are finished in a factory and able to move */
void CE323AI::UnitFinished(int uid) {
	CUnit *unit = ai->unittable->getUnit(uid);
	ai->unittable->unitsAliveTime[uid] = 0;
	ai->unittable->idle[uid] = true;

	LOG_II("CE323AI::UnitFinished " << (*unit))

	unsigned int c = unit->type->cats;
	if (unit->builder > 0)
		ai->unittable->builders[unit->builder] = true;

	/* Eco unit */
	if (!(c&ATTACKER) || c&COMMANDER)
		ai->economy->addUnit(*unit);
	/* Military unit */
	else
		ai->military->addUnit(*unit);

}

/* Called on a destroyed unit */
void CE323AI::UnitDestroyed(int uid, int attacker) {
	CUnit *unit = ai->unittable->getUnit(uid);
	LOG_II("CE323AI::UnitDestroyed " << (*unit))
	unit->remove();
}

/* Called when unit is idle */
void CE323AI::UnitIdle(int uid) {
	CUnit *unit = ai->unittable->getUnit(uid);
	LOG_II("CE323AI::UnitIdle " << (*unit))
	ai->unittable->idle[uid] = true;
}

/* Called when unit is damaged */
void CE323AI::UnitDamaged(int damaged, int attacker, float damage, float3 dir) {

}

/* Called on move fail e.g. can't reach point */
void CE323AI::UnitMoveFailed(int uid) {
	//CUnit *unit = ai->unittable->getUnit(uid);
	//unit->moveRandom(50.0f);
}



/***********************
 * Enemy related callins *
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
	const ChangeTeamEvent* cte = (const ChangeTeamEvent*) data;
	switch(msg) {
		case AI_EVENT_UNITGIVEN:
			/* Unit gained */
			if ((cte->newteam) == ai->team) {
				UnitFinished(cte->unit);
				CUnit *unit = ai->unittable->getUnit(cte->unit);

				LOG_II("CE323AI::UnitGiven " << (*unit))
			}
		break;

		case AI_EVENT_UNITCAPTURED:
			/* Unit lost */
			if ((cte->oldteam) == ai->team) {
				UnitDestroyed(cte->unit, 0);
				CUnit *unit = ai->unittable->getUnit(cte->unit);

				LOG_II("CE323AI::UnitCaptured " << (*unit))
			}
		break;
	}
	return 0;
}


/* Update AI per logical frame = 1/30 sec on gamespeed 1.0 */
void CE323AI::Update() {
	/* Make sure we shift the multiplexer for each instance of E323AI */
	int frame = ai->cb->GetCurrentFrame();

	/* Don't act before the 100th frame, messed up eco stuff -_- */
	if (frame < 100) {
		if (frame == 1)
			ai->intel->init();
		return;
	}

	else if (frame == (800+(ai->team*200))) {
		LOG_SS("*** " << AI_VERSION << " ***");
		LOG_SS("*** " << AI_CREDITS << " ***");
		LOG_SS("*** " <<  AI_NOTES  << " ***");
	}

	/* Rotate through the different update events to distribute computations */
	frame += ai->team;
	switch(frame % MULTIPLEXER) {
		case 0: { /* update incomes */
			CScopedTimer t(std::string("eco-incomes"));
			ai->economy->updateIncomes();
		}
		break;

		case 1: { /* update threatmap */
			CScopedTimer t(std::string("threatmap"));
			ai->threatmap->update(frame);
		}
		break;

		case 2: { /* update the path itself of a group */
			CScopedTimer t(std::string("pf-grouppath"));
			ai->pathfinder->updatePaths();
		}
		break;

		case 3: { /* update the groups following a path */
			CScopedTimer t(std::string("pf-followers"));
			ai->pathfinder->updateFollowers();
		}
		break;

		case 4: { /* update enemy intel */
			CScopedTimer t(std::string("intel"));
			ai->intel->update(frame);
		}
		break;

		case 5: { /* update defense matrix */
			CScopedTimer t(std::string("defensematrix"));
			ai->defensematrix->update();
		}

		case 6: { /* update military */
			CScopedTimer t(std::string("military"));
			ai->military->update(frame);
		}
		break;

		case 7: { /* update economy */
			CScopedTimer t(std::string("eco-update"));
			ai->economy->update(frame);
		}
		break;

		case 8: { /* update taskhandler */
			CScopedTimer t(std::string("tasks"));
			ai->tasks->update();
		}

		case 9: { /* update unit table */
			CScopedTimer t(std::string("unittable"));
			ai->unittable->update();
		}
		break;
	}
}
