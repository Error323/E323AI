#ifndef CAI_H
#define CAI_H

#include "headers/HAIInterface.h"
#include "headers/Defines.h"
#include "CLogger.h"

class CMetalMap;
class CUnitTable;
class CEconomy;
class CWishList;
class CTaskHandler;
class CThreatMap;
class CPathfinder;
class CIntel;
class CMilitary;

/* Ensures single instantiation of classes and good reachability */
struct AIClasses {
	IAICallback   *cb;
	IAICheats     *cbc;
	CMetalMap     *metalmap;
	CUnitTable    *unittable;
	CEconomy      *economy;
	CWishList     *wishlist;
	CTaskHandler  *tasks;
	CThreatMap    *threatmap;
	CPathfinder   *pathfinder;
	CIntel        *intel;
	CMilitary     *military;
	CLogger       *logger;
	int           team;
};

#endif
