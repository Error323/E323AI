#ifndef CAI_H
#define CAI_H

#include "headers/HAIInterface.h"
#include "headers/Defines.h"
#include "CLogger.h"

class CConfigParser;
class CUnitTable;
class CEconomy;
class CWishList;
class CTaskHandler;
class CThreatMap;
class CPathfinder;
class CIntel;
class CMilitary;
class CDefenseMatrix;
class GameMap;
class CCoverageHandler;

/* Ensures single instantiation of classes and good reachability */
class AIClasses {

public:
	AIClasses(IGlobalAICallback* callback);
	~AIClasses();

	static std::map<int, AIClasses*> instances;
		// AI instances grouped by team ID
	static std::map<int, std::map<int, AIClasses*> > instancesByAllyTeam;
	    // AI instances grouped by ally team ID
	static std::vector<int> unitIDs;
		// temporary container for GetEnemyUnits(), GetFriendlyUnits() etc. results
	IAICallback*	cb;
	IAICheats*		cbc;
	CConfigParser*	cfgparser;
	GameMap*		gamemap;
	CUnitTable*		unittable;
	CEconomy*		economy;
	CWishList*		wishlist;
	CTaskHandler*	tasks;
	CThreatMap*		threatmap;
	CPathfinder*	pathfinder;
	CIntel*			intel;
	CMilitary*		military;
	CDefenseMatrix*	defensematrix;
	CLogger*		logger;
	CCoverageHandler*	coverage;
	int team;
		// team ID
	int allyTeam;
		// ally team ID
	int allyIndex;
		// team index within ally E323AIs (base = 1)
	difficultyLevel difficulty;
		// TODO: move to CIntel?

	bool isMaster() { return (instances.begin()->first == team); }
	bool isMasterAlly() { return instancesByAllyTeam[allyTeam].begin()->first == team; }
	bool isSole() { return instances.size() == 1; }
	void updateAllyIndex();
};

#endif
