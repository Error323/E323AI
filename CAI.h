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

/* Ensures single instantiation of classes and good reachability */
struct AIClasses {
	IAICallback    *cb;
	IAICheats      *cbc;
	CConfigParser  *cfgparser;
	CMetalMap      *metalmap;
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
};

#endif
