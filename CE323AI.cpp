#include "CE323AI.h"

#include "CAI.h"
#include "CConfigParser.h"
#include "GameMap.hpp"
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
#include "CGroup.h"
#include "CScopedTimer.h"
#include "CRNG.h"
#include "Util.hpp"
#include "ReusableObjectFactory.hpp"

int CE323AI::instances = 0;

void CE323AI::InitAI(IGlobalAICallback* callback, int team) {
	instances++;
	ai                = new AIClasses();
	ai->cb            = callback->GetAICallback();
	ai->cbc           = callback->GetCheatInterface();
	ai->team          = team;
	ai->logger        = new CLogger(ai, CLogger::LOG_SCREEN | CLogger::LOG_FILE);
	ai->cfgparser     = new CConfigParser(ai);
	ai->unittable     = new CUnitTable(ai);

	std::string configfile(ai->cb->GetModName());
	configfile = configfile.substr(0, configfile.size()-4) + "-config.cfg";
	ai->cfgparser->parseConfig(configfile);
	if (!ai->cfgparser->isUsable()) {
		ai->cb->SendTextMsg("No usable config file available for this Mod/Game.", 0);
		const std::string confFileLine = "A template can be found at: " + configfile;
		ai->cb->SendTextMsg(confFileLine.c_str(), 0);
		ai->cb->SendTextMsg("Shutting Down ...", 0);

		// we have to cleanup here, as ReleaseAI() will not be called
		// in case of an error in InitAI().
		delete ai->cfgparser;
		delete ai->logger;
		delete ai->unittable;
		delete ai;
		// this will kill this AI instance gracefully
		throw 33;
	}

	ai->gamemap       = new GameMap(ai);
	
	ai->economy       = new CEconomy(ai);
	ai->wishlist      = new CWishList(ai);
	ai->tasks         = new CTaskHandler(ai);
	ai->threatmap     = new CThreatMap(ai);
	ai->pathfinder    = new CPathfinder(ai);
	ai->intel         = new CIntel(ai);
	ai->military      = new CMilitary(ai);
	ai->defensematrix = new CDefenseMatrix(ai);

	/* Set the new graph stuff */
	ai->cb->DebugDrawerSetGraphPos(-0.4f, -0.4f);
	ai->cb->DebugDrawerSetGraphSize(0.8f, 0.6f);

	/*
	ai->uploader->AddString("aiversion", AI_VERSION_NR);
	ai->uploader->AddString("ainame",    AI_NAME);
	ai->uploader->AddString("modname",   ai->cb->GetModName());
	ai->uploader->AddString("mapname",   ai->cb->GetMapName());
	*/
}

void CE323AI::ReleaseAI() {
	instances--;
	if (instances == 0) {
		ReusableObjectFactory<CGroup>::Shutdown();
		ReusableObjectFactory<CUnit>::Shutdown();
	}

	delete ai->defensematrix;
	delete ai->military;
	delete ai->intel;
	delete ai->pathfinder;
	delete ai->threatmap;
	delete ai->tasks;
	delete ai->wishlist;
	delete ai->economy;
	delete ai->unittable;
	delete ai->gamemap;
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

	if (unit->def->isCommander && !ai->economy->isInitialized()) {
		ai->economy->init(*unit);
	}

	ai->economy->addUnitOnCreated(*unit);

	if (bid < 0)
		return; // unit was spawned from nowhere (e.g. commander)

	unsigned int c = unit->type->cats;
	if (c&MOBILE) {
		CUnit *builder = ai->unittable->getUnit(bid);
		// if builder is a mobile unit (like Consul in BA) then do not 
		// assign move command...
		if (builder->type->cats&STATIC) {
			// NOTE: factories should be already rotated in proper direction to prevent
			// units going outside the map
			if (c&AIR)
				unit->moveRandom(500.0f, true);
			else if (c&BUILDER)
				unit->moveForward(200.0f);
			else
				unit->moveForward(400.0f);
		}
	}
}

/* Called when units are finished in a factory and able to move */
void CE323AI::UnitFinished(int uid) {
	CUnit *unit = ai->unittable->getUnit(uid);
	
	if(!unit) {
		LOG_EE("CE323AI::UnitFinished unregistered Unit(" << uid << ")")
		return;
	}

	ai->unittable->unitsAliveTime[uid] = 0;
	ai->unittable->idle[uid] = true;

	LOG_II("CE323AI::UnitFinished " << (*unit))

	if (unit->builtBy >= 0)
		ai->unittable->builders[unit->builtBy] = true;

	/* Eco unit */
	if (unit->isEconomy())
		ai->economy->addUnitOnFinished(*unit);
	/* Military unit */
	else if(unit->type->cats&ATTACKER)
		ai->military->addUnit(*unit);
	else
		LOG_WW("CE323AI::UnitFinished invalid unit " << *unit)
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
	//LOG_II("CE323AI::UnitIdle " << (*unit))
	if(ai->unittable->unitsUnderPlayerControl.find(uid) != ai->unittable->unitsUnderPlayerControl.end()) {
		ai->unittable->unitsUnderPlayerControl.erase(uid);
		assert(unit->group == NULL);
		LOG_II("CE323AI::UnitControlledByAI " << (*unit))
		// re-assigning unit to appropriate group
		UnitFinished(uid);
		return;
	}
	ai->unittable->idle[uid] = true;
}

/* Called when unit is damaged */
void CE323AI::UnitDamaged(int damaged, int attacker, float damage, float3 dir) {
	// TODO: introduce quick imrovement for builders to reclaim their attacker
	// but next it should return to its current job, so we need to delay 
	// current task which is impossible while there is no task queue per unit
	// group (curently we have a single task per group)

	if (ai->cb->UnitBeingBuilt(damaged) || ai->cb->IsUnitParalyzed(damaged) || ai->cb->GetUnitHealth(damaged) < EPS) 
		return;
	if (attacker < 0)
		return;
	if (ai->cbc->GetUnitHealth(attacker) < EPS)
		return;

	CUnit* unit = ai->unittable->getUnit(damaged);
	if (!unit)
		return; // invalid unit
	if (!unit->group)
		return; // unit is not under AI control

	/*
	const unsigned int cats = unit->type->cats;
	if (cats&MOBILE) {
		bool attack = false;
		if (cats&(ATTACKER|BUILDER)) {
			const UnitDef *eud = ai->cbc->GetUnitDef(attacker);
			if (!eud)
				return;
			const unsigned int ecats = UC(eud->id);
			float3 epos = ai->cbc->GetUnitPos(attacker);
			float range = cats&BUILDER ? unit->group->buildRange: unit->group->range;
			if (unit->group->pos().distance2D(epos) < 1.1f*range) {
				// always strikeback if attacker is a scouter or less powerful
				if ((ecats&SCOUTER) || ai->threatmap->getThreat(epos, 0.0f) <= unit->group->strength) {
					ATask* task = ai->tasks->getTask(*unit->group);
					if (!task || (task->t != ATTACK && task->t != FACTORY_BUILD)) {
						if (task && task->active)
							task->remove();
						attack = true;
					}
				}
			}
		}
		
		if (attack) {
			ai->tasks->addAttackTask(attacker, *unit->group);
		} else {
			// TODO: run away or continue its task depending on group style
		}
	}
	*/
}

/* Called on move fail e.g. can't reach point */
void CE323AI::UnitMoveFailed(int uid) {
	/*
	CUnit *unit = ai->unittable->getUnit(uid);
	unit->moveRandom(50.0f);
	*/
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
				UnitCreated(cte->unit, -1);
				UnitFinished(cte->unit);
				
				// NOTE: getting "unit" for logging only
				CUnit *unit = ai->unittable->getUnit(cte->unit);

				LOG_II("CE323AI::UnitGiven " << (*unit))
			}
			break;

		case AI_EVENT_UNITCAPTURED:
			/* Unit lost */
			if ((cte->oldteam) == ai->team) {
				// NOTE: getting "unit" for logging only
				CUnit *unit = ai->unittable->getUnit(cte->unit);
				
				LOG_II("CE323AI::UnitCaptured " << (*unit))

				UnitDestroyed(cte->unit, 0);
			}
			break;

		case AI_EVENT_PLAYER_COMMAND:
			/* Player incoming command */
			const PlayerCommandEvent* pce = (const PlayerCommandEvent*) data;
			bool importantCommand = false;
			if(pce->command.id < 0)
				importantCommand = true;
			else
				switch(pce->command.id)
				{
					case CMD_MOVE:
					case CMD_PATROL:
					case CMD_FIGHT:
					case CMD_ATTACK:
					case CMD_AREA_ATTACK:
					case CMD_GUARD:
					case CMD_REPAIR:
					case CMD_LOAD_UNITS:
					case CMD_UNLOAD_UNITS:
					case CMD_UNLOAD_UNIT:
					case CMD_RECLAIM:
					case CMD_DGUN:
					case CMD_RESTORE:
					case CMD_RESURRECT:
					case CMD_CAPTURE:
						importantCommand = true;
						break;
				}

			if(importantCommand && !pce->units.empty()) {
				for(int i = 0; i < pce->units.size(); i++) {
					if(ai->unittable->unitsUnderPlayerControl.find(pce->units[i]) == ai->unittable->unitsUnderPlayerControl.end()) {
						// we have to remove unit from a group, but not 
						// to emulate unit death
						CUnit* unit = ai->unittable->getUnit(pce->units[i]);
						if(unit->group) {
							// remove unit from group so it will not receive 
							// AI commands anymore...
							unit->unreg(*unit->group); // this prevent a crash when unit destroyed in player mode
							unit->group->remove(*unit);
						}							
						// NOTE: i think the following two lines have almost
						// no sense because current AI design does not deal
						// with units not assigned to any group
						unit->micro(false);
						ai->unittable->idle[unit->key] = false; // because player controls it
						ai->unittable->unitsUnderPlayerControl[unit->key] = unit;
						LOG_II("CE323AI::UnitControlledByPlayer " << (*unit))
					}
				}
			}
			break;	
	}

	return 0;
}

/* Update AI per logical frame = 1/30 sec on gamespeed 1.0 */
void CE323AI::Update() {
	// NOTE: if AI is attached at game start Update() is called since 1st game frame.
	// By current time all player commanders are already spawned and that's good because 
	// we calculate number of enemies in CIntel::init()
	const int currentFrame = ai->cb->GetCurrentFrame();
	
	if (currentFrame < 0)
		return; // some shit happened with engine? (stolen from AAI)

	// NOTE: AI can be attached in mid-game state with /aicontrol command
	int localFrame;

	if (attachedAtFrame < 0) {
		attachedAtFrame = currentFrame - 1;
	}
	
	localFrame = currentFrame - attachedAtFrame;

	if(localFrame == 1)
		ai->intel->init();
	
	if(!ai->economy->isInitialized())
		return;

	// anyway show AI is loaded even if it is not playing actually...
	if (localFrame == (800 + (ai->team*200))) {
		LOG_SS("*** " << AI_VERSION << " ***");
		LOG_SS("*** " << AI_CREDITS << " ***");
		LOG_SS("*** " <<  AI_NOTES  << " ***");
	}

	/* Make sure we shift the multiplexer for each instance of E323AI */
	int aiframe = localFrame + ai->team;
	
	// Make sure we start playing since "eco-incomes" update
	if(!isRunning) {
		isRunning = aiframe % MULTIPLEXER == 0;
	}

	if(!isRunning)
		return;

	/* Rotate through the different update events to distribute computations */
	switch(aiframe % MULTIPLEXER) {
		case 0: { /* update incomes */
			ai->economy->updateIncomes();
		}
		break;

		case 1: { /* update threatmap */
			PROFILE(threatmap)
			ai->threatmap->update(localFrame);
		}
		break;

		case 2: { /* update the path itself of a group */
			PROFILE(A*)
			ai->pathfinder->updatePaths();
		}
		break;

		case 3: { /* update the groups following a path */
			PROFILE(following)
			ai->pathfinder->updateFollowers();
		}
		break;

		case 4: { /* update enemy intel */
			PROFILE(intel)
			ai->intel->update(localFrame);
		}
		break;

		case 5: { /* update defense matrix */
			PROFILE(defensematrix)
			ai->defensematrix->update();
		}

		case 6: { /* update military */
			PROFILE(military)
			ai->military->update(localFrame);
		}
		break;

		case 7: { /* update economy */
			PROFILE(economy)
			ai->economy->update(localFrame);
		}
		break;

		case 8: { /* update taskhandler */
			PROFILE(taskhandler)
			ai->tasks->update();
		}

		case 9: { /* update unit table */
			ai->unittable->update();
		}
		break;
	}
}
