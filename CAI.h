#ifndef CAI_H
#define CAI_H

#include "headers/HAIInterface.h"
#include "headers/Defines.h"
#include "CLogger.h"

class CConfigParser;
class CMetalMap;
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

/* Ensures single instantiation of classes and good reachability */
class AIClasses {

public:
	AIClasses() { unitIDs.resize(MAX_UNITS); }

	IAICallback    *cb;
	IAICheats      *cbc;
	CConfigParser  *cfgparser;
	CMetalMap      *metalmap;
	GameMap        *gamemap;
	CUnitTable     *unittable;
	CEconomy       *economy;
	CWishList      *wishlist;
	CTaskHandler   *tasks;
	CThreatMap     *threatmap;
	CPathfinder    *pathfinder;
	CIntel         *intel;
	CMilitary      *military;
	CDefenseMatrix *defensematrix;
	CLogger        *logger;
	int            team;
	std::vector<int> unitIDs; // temporary container for GetEnemyUnits(), GetFriendlyUnits() etc. results
};

#endif
