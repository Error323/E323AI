#ifndef CUNIT_TABLE_H
#define CUNIT_TABLE_H

#include <map>
#include <vector>
#include <stack>
#include <string>

#include "ARegistrar.h"
#include "headers/Defines.h"
#include "headers/HEngine.h"

class CUnit;
class AIClasses;

/* Unit wrapper struct */
class UnitType {
public:
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

class CUnitTable: public ARegistrar {
	public:
		CUnitTable(AIClasses *ai);
		~CUnitTable();


		/* Returns a fresh CUnit instance */
		CUnit* requestUnit(int uid, int bid);

		/* Return unit by ingame id */
		CUnit* getUnit(int id);

		/* Total number of unit types (definitions) */
		int numUnits;

		/* All units flattened in a map */
		std::map<int, UnitType>   units;

		/* Unit categories in vector */
		std::vector<unitCategory> cats;

		/* movetypes, used by pathfinder */
		std::map<int, MoveData*>  moveTypes;

		/* Ingame units, set in eco module */
		std::map<int, bool>       idle;
		std::map<int, bool>       builders;
		std::map<int, bool>       metalMakers;
		std::map<int, UnitType*>  factoriesBuilding;
		std::map<int, CUnit*>     activeUnits;
		std::map<int, bool>       factories;
		std::map<int, CUnit*>     defenses;
		std::map<int, CUnit*>     energyStorages;
		std::map<int, int>        unitsAliveTime;

		/* unitCategories in string format, see Defines.h */
		static std::map<unitCategory, std::string> cat2str;
		static std::map<std::string, unitCategory> str2cat;


		/* Special commander hook, since it's the first to spawn */
		UnitType *comm;

		/* Determine if alive long enough */
		bool canPerformTask(CUnit &unit);

		void update();

		/* Overload */
		void remove(ARegistrar &unit);

		/* Returns a unittype with categories that ut can build */
		UnitType* canBuild(UnitType *ut, unsigned int categories);
		void getBuildables(UnitType *ut, unsigned i, unsigned e, std::multimap<float, UnitType*> &candidates);
		bool gotFactory(unsigned c);

		/* Debugging functions */
		std::string debugCategories(UnitType *ut);
		std::string debugCategories(unsigned categories);
		void debugUnitDefs(UnitType *ut);
		void debugWeapons(UnitType *ut);

	private:
		AIClasses *ai;

		char buf[255];

		/* The unit container */
		std::vector<CUnit*> ingameUnits;

		/* The <unitid, vectoridx> table */
		std::map<int, int> lookup;

		/* The free slots (CUnit instances that are zombie-ish) */
		std::stack<int>    free;

		/* Build the lists buildby and canbuild per unit */
		void buildTechTree();

		/* Generate the categorizations config file */
		void generateCategorizationFile(const char *fileName);

		/* Categorize the units, see defines.h for categories */
		unsigned int categorizeUnit(UnitType *ut);

		/* Calculate the unit damage per second */
		float calcUnitDps(UnitType *ut);

		/* Determine wether a unit has antiair weapons */
		bool hasAntiAir(const std::vector<UnitDef::UnitDefWeapon> &weapons);

		/* Create a UnitType of ud and insert into units */
		UnitType* insertUnit(const UnitDef *ud);
};

#endif
