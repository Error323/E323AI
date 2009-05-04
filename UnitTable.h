#ifndef UNIT_TABLE_H
#define UNIT_TABLE_H

#include "E323AI.h"

class CUnitTable {
	public:
		CUnitTable(AIClasses *ai);
		~CUnitTable() {};

		/* Total nr of units */
		int numUnits;

		/* Current units ingame, unit-ingame-id as key (See Global AI) */
		std::map<int, UnitType*> gameAllUnits;
		std::map<int, UnitType*> gameIdleMilitary;

		/* All units flattened in a map */
		std::map<int, UnitType> units;

		/* Special commander hook, since it's the first to spawn */
		UnitType *comm;

		/* Debugging functions */
		void debugCategories(UnitType *ut);
		void debugUnitDefs(UnitType *ut);
		void debugWeapons(UnitType *ut);

	private:
		AIClasses *ai;

		/* unitCategories in string format, see Defines.h */
		std::map<unitCategory, std::string> categories;

		/* Build the lists buildby and canbuild per unit */
		void buildTechTree();

		/* Categorize the units, see defines.h for categories */
		unsigned int categorizeUnit(UnitType *ut);

		/* Calculate the unit damage per second */
		float calcUnitDps(UnitType *ut);

		/* Determine wether a unit has antiair weapons */
		bool hasAntiAir(const std::vector<UnitDef::UnitDefWeapon> &weapons);

		/* Create a UnitType of ud and insert into units */
		UnitType* insertUnit(const UnitDef *ud);

		char buf[1024];
};

#endif
