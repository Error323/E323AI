#include "CCataloguer.h"

#include <stdexcept>

#include "CUnit.h"


void CCataloguer::registerMatcher(const CategoryMatcher& m) {
	if (cache.find(m) == cache.end()) {
		cache[m];
	}
}

void CCataloguer::addUnit(UnitType* type, int id) {
	const unitCategory cats = type->cats;

	for (CacheMapIt it = cache.begin(); it != cache.end(); ++it) {
		if (it->first.test(cats)) {
			it->second[id] = type;
		}
	}
}

void CCataloguer::addUnit(CUnit* unit) {
	addUnit(unit->type, unit->key);
}

void CCataloguer::removeUnit(int id) {
	for (CacheMapIt it = cache.begin(); it != cache.end(); ++it) {
		it->second.erase(id);
	}
}

std::map<int, UnitType*>& CCataloguer::getUnits(const CategoryMatcher& m) {
	CacheMapIt it = cache.find(m);
	if (it != cache.end())
		return it->second;
	throw std::runtime_error("No unit found for category");
}

std::map<int, UnitType*>* CCataloguer::getUnits(const unitCategory& inc, const unitCategory& exc) {
	CacheMapIt it;
	CategoryMatcher m(inc, exc);

	it = cache.find(m);

	if (it == cache.end())
		return NULL;
	return &it->second;
}
