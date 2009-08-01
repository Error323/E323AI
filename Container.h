#ifndef CONTAINERS_H
#define CONTAINERS_H

class IAICallback;
class IAICheats;
class CMetalMap;
class CUnitTable;
class CEconomy;
class CWishList;
class CTaskHandler;
class CThreatMap;
class CPathfinder;
class CIntel;
class CMilitary;
class CMyGroup;

/* Ensures single instantiation of classes and good reachability */
struct AIClasses {
	IAICallback   *call;
	IAICheats     *cheat;
	CMetalMap     *metalMap;
	CUnitTable    *unitTable;
	CEconomy      *eco;
	CWishList     *wl;
	CTaskHandler  *tasks;
	CThreatMap    *threatMap;
	CPathfinder   *pf;
	CIntel        *intel;
	CMilitary     *military;
	std::ofstream *logger;
};

/* Unit wrapper struct */
struct UnitType {
	const UnitDef *def;                 /* As defined by spring */
	int id;                             /* Overloading the UnitDef id */
	int techLevel;                      /* By looking at the factory cost in which it can be build */
	float dps;                          /* A `briljant' measurement for the power of a unit :P */
	float cost;                         /* Cost defined in energy units */
	float energyMake;                   /* Netto energy this unit provides */
	float metalMake;                    /* Netto metal this unit provides */
	unsigned int cats;                  /* Categories this unit belongs, Categories @ Defines.h */
	std::map<int, UnitType*> buildBy;
	std::map<int, UnitType*> canBuild;
};

#endif
