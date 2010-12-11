#ifndef E323_CCATALOGUER_H
#define E323_CCATALOGUER_H

#include <map>

#include "headers/Defines.h"
#include "CUnitTable.h"

class CUnit;

struct CategoryMatcher {
	CategoryMatcher(const unitCategory& inc, const unitCategory& exc = 0):include(inc),exclude(exc) {}
	
	bool operator < (const CategoryMatcher& other) const {
		UnitCategoryCompare cmp;
		if (cmp(include, other.include))
			return true;
		if (include == other.include)
			return cmp(exclude, other.exclude);
		return false;
	}

	bool test(const unitCategory& other) const {
		return (include&other).any() && (exclude&other).none();
	}

	unitCategory include;
	unitCategory exclude;
};

class CCataloguer {

public:
	void registerMatcher(const CategoryMatcher& m);
	void registerMatcher(const unitCategory& inc, const unitCategory& exc = 0) { registerMatcher(CategoryMatcher(inc, exc)); }
	void addUnit(UnitType* type, int id);
	void addUnit(CUnit* unit);
	void removeUnit(int id);
	std::map<int, UnitType*>& getUnits(const CategoryMatcher& m);
	std::map<int, UnitType*>* getUnits(const unitCategory& inc, const unitCategory& exc = 0);
	int size() const { return cache.size(); }

private:
	typedef std::map<CategoryMatcher, std::map<int, UnitType*> > CacheMap;
	typedef CacheMap::iterator CacheMapIt;
	
	CacheMap cache;
};

#endif
