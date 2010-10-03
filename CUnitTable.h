#ifndef CUNIT_TABLE_H
#define CUNIT_TABLE_H

#include <map>
#include <vector>
#include <stack>
#include <string>

#include "ARegistrar.h"
#include "headers/Defines.h"
#include "headers/HEngine.h"
#include "CWishList.h"

class CUnit;
class AIClasses;

struct UnitCategoryCompare {
	bool operator() (const unitCategory& a, const unitCategory& b) const {
		if (a.none() && b.any())
			return true;
		if (a.any() && b.none())
			return false;
		for (int bit = 0; bit < MAX_CATEGORIES; bit++) {
			if (a[bit]) {
				if (!b[bit])
					return true;
			}
			else if(b[bit])
				return false;
		}				
		return false;
	}
};

/* Unit wrapper struct */
struct UnitType {
	const UnitDef *def; /* As defined by spring */
	int techLevel;      /* By looking at the factory cost in which it can be build */
	float dps;          /* A `briljant' measurement for the power of a unit :P */
	float cost;         /* Cost defined in energy units */
	float costMetal;
	float energyMake;   /* Netto energy this unit provides */
	float metalMake;    /* Netto metal this unit provides */
	unitCategory cats;  /* Custom categories this unit belongs */
	std::map<int, UnitType*> buildBy;
	std::map<int, UnitType*> canBuild;

	inline int getID() const { def->id; }
};

class CUnitTable: public ARegistrar {
	
public:
	typedef std::map<unitCategory, std::string, UnitCategoryCompare> UnitCategory2StrMap;

	CUnitTable(AIClasses* ai);
	~CUnitTable();

	/* unitCategories in string format, see Defines.h */
	static std::map<unitCategory, std::string, UnitCategoryCompare> cat2str;
	static std::map<std::string, unitCategory> str2cat;
	/* Unit categories in a vector */
	static std::vector<unitCategory> cats;
	/* Total number of unit types */
	int numUnits;
	/* Max unit power */
	float maxUnitPower;
	/* All units flattened in a map <udef_id, unit_type> */
	std::map<int, UnitType> units;
	/* movetypes, used by pathfinder <move_type_id, move_data> */
	std::map<int, MoveData*> moveTypes;
	/* Ingame units, set in eco module */
	std::map<int, bool>         idle;
	std::map<int, bool>         builders; // <unit_id, job_is_finished>
	std::map<int, CUnit*>       metalMakers;
	std::map<int, CUnit*>       activeUnits;
	std::map<int, CUnit*>       factories;
	std::map<int, CUnit*>       defenses;
	std::map<int, CUnit*>       staticUnits;
	std::map<int, CUnit*>       staticEconomyUnits;
	std::map<int, CUnit*>       energyStorages;
	std::map<int, CUnit*>       unitsUnderPlayerControl;
	std::map<int, unitCategory> unitsUnderConstruction; // <unit_id, cats_from_wishlist>
	std::map<int, Wish>         unitsBuilding; // <builder_id, wish>
	

	static CUnit* getUnitByDef(std::map<int, CUnit*>& dic, const UnitDef* udef);
	
	static CUnit* getUnitByDef(std::map<int, CUnit*>& dic, int did);
	/* Returns a fresh CUnit instance */
	CUnit* requestUnit(int uid, int bid);
	/* Return unit by ingame id */
	CUnit* getUnit(int id);
	/* Called on each frame%MULTIPLEXER */
	void update();
	/* Overload */
	void remove(ARegistrar &unit);
	/* Returns a unittype with categories that ut can build */
	UnitType* canBuild(UnitType *ut, unitCategory categories);
	
	void getBuildables(UnitType *ut, unitCategory include, unitCategory exclude, std::multimap<float, UnitType*>& candidates);
	
	int factoryCount(unitCategory c);
	
	bool gotFactory(unitCategory c);
	
	int unitCount(unitCategory c);
	/* Select first unit type which matches requested cats */
	UnitType* getUnitTypeByCats(unitCategory c);
	
	int setOnOff(std::map<int, CUnit*>& list, bool value);
	/* Debugging functions */
	std::string debugCategories(UnitType* ut);
	
	std::string debugCategories(unitCategory categories);
	
	void debugUnitDefs(UnitType* ut);
	
	void debugWeapons(UnitType* ut);

protected:
	AIClasses* ai;

private:
	char buf[255];

	/* Build the lists buildby and canbuild per unit */
	void buildTechTree();
	/* Generate the categorizations config file */
	void generateCategorizationFile(std::string& fileName);
	/* Categorize the units, see defines.h for categories */
	unitCategory categorizeUnit(UnitType *ut);
	/* Calculate the unit damage per second */
	float calcUnitDps(UnitType *ut);
	/* Create a UnitType of ud and insert into units */
	UnitType* insertUnit(const UnitDef *ud);
};

#endif
