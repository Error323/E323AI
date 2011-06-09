#include "CE323AI.h"

#include <string>
#include <algorithm>
#include <cctype>

#include "CAI.h"
#include "CRNG.h"
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
#include "Util.hpp"
#include "CCoverageHandler.h"
#include "ReusableObjectFactory.hpp"

namespace springLegacyAI {
	class IGlobalAICallback;
}
#include "LegacyCpp/IAICallback.h"


CE323AI::CE323AI() {
	isRunning = false;
	attachedAtFrame = -1;
}

void CE323AI::InitAI(IGlobalAICallback* callback, int team) {
	static const char
		optionDifficulty[] = "difficulty",
		optionLoggingLevel[] = "logging";
  
	CLogger::logLevel loggingLevel = CLogger::VERBOSE;

	ai = new AIClasses(callback);

	std::map<std::string, std::string> options = ai->cb->GetMyOptionValues();
	if (options.find(optionDifficulty) != options.end()) {
		ai->difficulty = static_cast<difficultyLevel>(atoi(options[optionDifficulty].c_str()));
	}
	if (options.find(optionLoggingLevel) != options.end()) {
		loggingLevel = static_cast<CLogger::logLevel>(atoi(options[optionLoggingLevel].c_str()));
	}

	ai->logger = new CLogger(ai, /*CLogger::LOG_STDOUT |*/ CLogger::LOG_FILE, loggingLevel);

	LOG_II("CE323AI::InitAI allyIndex = " << ai->allyIndex)

	ai->cfgparser = new CConfigParser(ai);
	ai->unittable = new CUnitTable(ai);

	std::string configfile = ai->cfgparser->getFilename(GET_CFG);
	ai->cfgparser->parseConfig(configfile);
	if (ai->cfgparser->isUsable()) {
		// try loading overload file...
		configfile = ai->cfgparser->getFilename(GET_CFG|GET_VER);
		if (ai->cfgparser->fileExists(configfile))
			ai->cfgparser->parseConfig(configfile);
	}
	if (!ai->cfgparser->isUsable()) {
		ai->cb->SendTextMsg("No usable config file available for this Mod/Game.", 0);
		const std::string confFileLine = "A template can be found at: " + configfile;
		ai->cb->SendTextMsg(confFileLine.c_str(), 0);
		ai->cb->SendTextMsg("Shutting down...", 0);

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
	ai->coverage      = new CCoverageHandler(ai);

#if !defined(BUILDING_AI_FOR_SPRING_0_81_2)	
	/* Set the new graph stuff */
	ai->cb->DebugDrawerSetGraphPos(-0.4f, -0.4f);
	ai->cb->DebugDrawerSetGraphSize(0.8f, 0.6f);
#endif
}

void CE323AI::ReleaseAI() {
	
	if (ai->isSole()) {
		ReusableObjectFactory<CGroup>::Shutdown();
		ReusableObjectFactory<CUnit>::Shutdown();
		ReusableObjectFactory<CCoverageCell>::Shutdown();
	}

	delete ai->coverage;
	delete ai->defensematrix;
	delete ai->military;
	delete ai->intel;
	delete ai->pathfinder;
	delete ai->threatmap;
	delete ai->tasks;
	delete ai->wishlist;
	delete ai->economy;
	delete ai->gamemap;
	delete ai->unittable;
	delete ai->cfgparser;
	delete ai->logger;
	delete ai;
}

/************************
 * Unit related callins *
 ************************/

/* Called when units are spawned in a factory or when game starts */
void CE323AI::UnitCreated(int uid, int bid) {
	CUnit* unit = ai->unittable->requestUnit(uid, bid);
	
	LOG_II("CE323AI::UnitCreated " << (*unit))

	if ((unit->type->cats&COMMANDER).any() && !ai->economy->isInitialized()) {
		ai->economy->init(*unit);
	}

	// HACK: for metal extractors & geoplants only
	ai->economy->addUnitOnCreated(*unit);

	ai->coverage->addUnit(unit);

	if (bid < 0)
		return; // unit was spawned from nowhere (e.g. commander), or given by another player

	unitCategory c = unit->type->cats;
	if ((c&MOBILE).any()) {
		CUnit* builder = ai->unittable->getUnit(bid);
		// if builder is a mobile unit (like Consul in BA) then do not 
		// assign move command...
		if ((builder->type->cats&STATIC).any()) {
			// NOTE: factories should be already rotated in proper direction 
			// to prevent units going outside the map
			if ((c&AIR).any()) {
				if ((c&ANTIAIR).any())
					unit->guard(bid, true);
				else
					unit->moveRandom(450.0f, true);
			}
			else if ((c&BUILDER).any())
				unit->moveForward(200.0f);
			else
				unit->moveForward(400.0f);
		}
	}
	else {
		if ((c&NANOTOWER).any()) {
			unit->patrol(unit->getForwardPos(100.0f), true);
		}
	}

	// TODO: check if UnitIdle for factory/builder is called after 
	// UnitCreated then we do not need "unitsUnderConstruction" map
	std::map<int, Wish>::iterator it = ai->unittable->unitsBuilding.find(bid);
	if (it != ai->unittable->unitsBuilding.end())
		ai->unittable->unitsUnderConstruction[uid] = it->second.goalCats;
	else	
		ai->unittable->unitsUnderConstruction[uid] = 0;
}

/* Called when units are finished in a factory and able to move */
void CE323AI::UnitFinished(int uid) {
	CUnit* unit = ai->unittable->getUnit(uid);
	
	if(!unit) {
		const UnitDef *ud = ai->cb->GetUnitDef(uid);
		LOG_EE("CE323AI::UnitFinished unregistered " << (ud ? ud->humanName : std::string("UnknownUnit")) << "(" << uid << ")")
		return;
	}
	else
		LOG_II("CE323AI::UnitFinished " << (*unit))

	// NOTE: commanders and static units should start actions earlier than
	// usual units
	if (unit->builtBy == -1 || (unit->type->cats&STATIC).any())
		unit->aliveFrames = IDLE_UNIT_TIMEOUT;
	else
		unit->aliveFrames = 0; // reset time at which unit was building
			
	ai->unittable->idle[uid] = true;

	if (unit->builtBy >= 0) {
		// mark builder has finished its job
		ai->unittable->builders[unit->builtBy] = true;
	}

	if (unit->isEconomy()) {
		ai->economy->addUnitOnFinished(*unit);
	}
	else if(!ai->military->addUnit(*unit)) {
		LOG_WW("CE323AI::UnitFinished unit " << (*unit) << " is NOT under AI control")
	}
	
	// NOTE: very important to place this line AFTER registering a unit in
	// either economy or military blocks
	ai->unittable->unitsUnderConstruction.erase(uid);
}

/* Called on a destroyed unit */
void CE323AI::UnitDestroyed(int uid, int attacker) {
	ai->tasks->onUnitDestroyed(uid, attacker);

	CUnit *unit = ai->unittable->getUnit(uid);
	if (unit) {
		LOG_II("CE323AI::UnitDestroyed " << (*unit))
		unit->remove();
	}
}

/* Called when unit is idle */
void CE323AI::UnitIdle(int uid) {
	CUnit* unit = ai->unittable->getUnit(uid);
	
	if (unit == NULL) {
		const UnitDef *ud = ai->cb->GetUnitDef(uid);
		LOG_EE("CE323AI::UnitIdle unregistered " << (ud ? ud->humanName : std::string("UnknownUnit")) << "(" << uid << ")")
		return;
	}

	if (ai->unittable->unitsUnderPlayerControl.find(uid) != ai->unittable->unitsUnderPlayerControl.end()) {
		ai->unittable->unitsUnderPlayerControl.erase(uid);
		assert(unit->group == NULL);
		LOG_II("CE323AI::UnitIdle " << (*unit) << " is under AI control again")
		// re-assign unit to appropriate group
		UnitFinished(uid);
		return;
	}
	
	ai->unittable->idle[uid] = true;
	
	if ((unit->type->cats&(BUILDER|FACTORY)).any())
		ai->unittable->unitsBuilding.erase(uid);
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
	if (unit == NULL)
		return; // invalid unit
	if (unit->group == NULL)
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

/* Called on move fail e.g. can't reach the point */
void CE323AI::UnitMoveFailed(int uid) {
	CUnit* unit = ai->unittable->getUnit(uid);
	if (unit && (unit->type->cats&(LAND|SEA)).any()) {
		// if unit is inside a factory then force moving...
		float3 pos = ai->cb->GetUnitPos(unit->key);
		std::map<int, CUnit*>::iterator it;
		for (it = ai->unittable->factories.begin(); it != ai->unittable->factories.end(); ++it) {
			float distance = ai->cb->GetUnitPos(it->first).distance2D(pos);
			if (distance < 16.0) {
				unit->moveForward(200.0f);
				if (!unit->canPerformTasks())
					unit->aliveFrames = 0; // prolong idle timeout
			}
		}
	}
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

void CE323AI::EnemyCreated(int enemy) {
	ai->intel->onEnemyCreated(enemy);
}

void CE323AI::EnemyDestroyed(int enemy, int attacker) {
	ai->military->onEnemyDestroyed(enemy, attacker);
	ai->tasks->onEnemyDestroyed(enemy, attacker);
	ai->intel->onEnemyDestroyed(enemy, attacker);
}

void CE323AI::EnemyDamaged(int damaged, int attacker, float damage, float3 dir) {
}


/****************
 * Misc callins *
 ****************/

void CE323AI::GotChatMsg(const char* msg, int player) {
	static const char 
		cmdPrefix[] = "!e323ai",
		modTM[] = "threatmap",
		modMil[] = "military",
		modEco[] = "economy",
		modPF[] = "pathfinder",
		modPG[] = "pathgraph",
		modDM[] = "defensematrix",
		modCL[] = "coverage";

	// NOTE: accept AI commands from spectators only
	if (ai->cb->GetPlayerTeam(player) >= 0)
		return;
	
	std::string line(msg);
	std::transform(line.begin(), line.end(), line.begin(), ::tolower);

	if (line.find(cmdPrefix) == 0) {
		bool isDebugOn = false;
		size_t pos = line.find_first_not_of(' ', sizeof(cmdPrefix) - 1);
		if (pos == std::string::npos) {
			if (ai->isMaster()) {
				ai->cb->SendTextMsg("Usage: !e323ai <module>", 0);
				ai->cb->SendTextMsg("where", 0);
				ai->cb->SendTextMsg("\t<module> ::= pathfinder|pathgraph|military|threatmap|defensematrix|coverage", 0);
				ai->cb->SendTextMsg("\t<module> ::= pf|pg|mil|tm|dm|cl", 0);
			}
			return;
		}
		
		std::string cmd = line.substr(pos);
		if (cmd == modTM || cmd == "tm") {
			isDebugOn = ai->threatmap->switchDebugMode();
			cmd.assign(modTM);
		}
		else if (cmd == modMil || cmd == "mil") {
			isDebugOn = ai->military->switchDebugMode();
			cmd.assign(modMil);
		}
		else if (cmd == modPF || cmd == "pf") {
			isDebugOn = ai->pathfinder->switchDebugMode(false);
			cmd.assign(modPF);
		}
		else if (cmd == modPG || cmd == "pg") {
			isDebugOn = ai->pathfinder->switchDebugMode(true);
			cmd.assign(modPG);
		}
		else if (cmd == modDM || cmd == "dm") {
			isDebugOn = ai->defensematrix->switchDebugMode();
			cmd.assign(modDM);
		}
		else if (cmd == modCL || cmd == "cl") {
			isDebugOn = ai->coverage->toggleVisualization();
			cmd.assign(modCL);
		}
		else {
			if (cmd == "unit") {
				// dump selected unit info...
				line.clear();

				if (ai->cb->GetSelectedUnits(&ai->unitIDs[0], 1) > 0) {
					std::stringstream buffer;
					CUnit* unit = ai->unittable->getUnit(ai->unitIDs[0]);
					if (unit) {
						buffer << ".id = " << unit->key << "\n";
						buffer << ".humanName = " << unit->def->humanName << "\n";
						buffer << ".isMicroing = " << unit->isMicroing() << "\n";
						buffer << ".microingFrames = " << unit->microingFrames << "\n";
						buffer << ".aliveFrames = " << unit->aliveFrames << "\n";
						buffer << ".waiting = " << unit->waiting << "\n";
						buffer << ".idle = " << ai->unittable->idle[unit->key] << "\n";
						
						ai->cb->SendTextMsg(buffer.str().c_str(), 0);
					}
				}
			}
			else if (cmd == "tmv") {
				// dump threat value at mouse cursor...
				float value = 0.0f;
				float3 pos = ai->cb->GetMousePos();
				CUnit* unit = NULL;				
				if (ai->cb->GetSelectedUnits(&ai->unitIDs[0], 1) > 0) {
					unit = ai->unittable->getUnit(ai->unitIDs[0]);
					if (unit && unit->group) {
						value = ai->threatmap->getThreat(pos, 0.0f, unit->group);
					}
				}
				else {
					value = ai->threatmap->getThreat(pos, 0.0f);
				}
				
				std::stringstream buffer;
				buffer << "Threat value";
				if (unit)
					buffer << " for " << unit->def->humanName;
				buffer << " at position (" << pos.x << "," << pos.z << ") is " << value;
				
				ai->cb->SendTextMsg(buffer.str().c_str(), 0);				
			}
			else {
				line.assign("Module \"" + cmd + "\" is unknown or unsupported for visual debugging");
				
			}

			if (!line.empty())
				ai->cb->SendTextMsg(line.c_str(), 0);
			
			return;
		}
		
		line.assign("Debug mode is switched ");
		if (isDebugOn)
			line += "ON";
		else
			line += "OFF";
		line += " for \"" + cmd + "\" module";

		ai->cb->SendTextMsg(line.c_str(), 0);
	}
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
			else {
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
			}

			if(importantCommand && !pce->units.empty()) {
				for(int i = 0; i < pce->units.size(); i++) {
					const int uid = pce->units[i];
					if(ai->unittable->unitsUnderPlayerControl.find(uid) == ai->unittable->unitsUnderPlayerControl.end()) {
						// we have to remove unit from a group, but not 
						// to emulate unit death
						CUnit* unit = ai->unittable->getUnit(uid);
						
						if (unit == NULL)
							continue;
						
						// remove unit from group so it will not receive 
						// AI commands anymore...
						if(unit->group) {
							unit->group->remove(*unit);
						}							
						
						unit->micro(false);
						ai->unittable->idle[uid] = false; // because player controls it
						ai->unittable->unitsUnderPlayerControl[uid] = unit;
						
						LOG_II("CE323AI::PlayerCommand " << (*unit) << " is under human control")
					}
				}
			}
			break;	
	}

	return 0;
}

/* Update AI per logical frame = 1/30 sec on gamespeed 1.0 */
void CE323AI::Update() {
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
	if (localFrame == 800 && ai->isMaster()) {
		LOG_SS("*** " << AI_VERSION << " ***");
		LOG_SS("*** " << AI_CREDITS << " ***");
		LOG_SS("*** " <<  AI_NOTES  << " ***");
	}

	/* Make sure we shift the multiplexer for each instance of E323AI */
	int aiframe = localFrame + ai->team;
	
	// Make sure we start playing from "eco-incomes" update
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
			ai->coverage->update();
		}

		case 6: { /* update military */
			PROFILE(military)
			ai->military->update(localFrame);
		}
		break;

		case 7: { /* update economy */
			PROFILE(economy)
			ai->economy->update();
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
