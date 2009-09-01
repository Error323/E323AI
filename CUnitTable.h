#ifndef CUNIT_TABLE_H
#define CUNIT_TABLE_H

#include <map>
#include <vector>
#include <stack>

#include "ARegistrar.h"
#include "CUnit.h"
#include "CE323AI.h"

class CUnitTable: public ARegistrar {
	public:
		CUnitTable(AIClasses *ai);
		~CUnitTable() {};

		/* Returns a fresh CUnit instance */
		CUnit* requestUnit(int uid, int bid);

		/* Return unit by ingame id */
		CUnit* getUnit(int id);

		/* Total nr of units */
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

		/* Special commander hook, since it's the first to spawn */
		UnitType *comm;

		/* Overload */
		void remove(ARegistrar &unit);

		/* Returns a unittype with categories that ut can build */
		UnitType* canBuild(UnitType *ut, unsigned int categories);

		/* Debugging functions */
		void debugCategories(UnitType *ut);
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

		/* unitCategories in string format, see Defines.h */
		std::map<unitCategory, std::string> cat2str;
		std::map<std::string, unitCategory> str2cat;

		/* Build the lists buildby and canbuild per unit */
		void buildTechTree();

		/* Generate the categorizations config file */
		void generateCategorizationFile(const char *fileName);

		/* Parse the saved unit categorizations */
		void parseCategorizations(const char *fileName);

		/* Split a string on a certain character */
		void split(std::string &line, char c, std::vector<std::string> &splitted);

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
