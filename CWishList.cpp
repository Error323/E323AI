#include "CWishList.h"

#include "CAI.h"
#include "CUnitTable.h"
#include "CUnit.h"
#include "CRNG.h"
#include "CConfigParser.h"
#include "CEconomy.h"

CWishList::CWishList(AIClasses *ai) {
	this->ai = ai;
}

void CWishList::push(unsigned categories, buildPriority p) {
	std::map<int, bool>::iterator itFac = ai->unittable->factories.begin();
	UnitType *fac;
	int max = ai->cfgparser->getTotalStates()+1 - ai->cfgparser->getState();
	for (;itFac != ai->unittable->factories.end(); itFac++) {
		fac = UT(ai->cb->GetUnitDef(itFac->first)->id);
		std::multimap<float, UnitType*> candidates;
		ai->unittable->getBuildables(fac, categories, 0, candidates);
		if (!candidates.empty()) { 
			/* Initialize new std::vector */
			if (wishlist.find(fac->id) == wishlist.end()) {
				std::vector<Wish> L;
				wishlist[fac->id] = L;
			}

			/* Determine which buildables we can afford */
			std::multimap<float, UnitType*>::iterator i = candidates.begin();
			int iterations = candidates.size() / max;
			bool affordable = false;
			while(iterations >= 0) {
				if (ai->economy->canAffordToBuild(fac, i->second))
					affordable = true;
				else
					break;

				if (i == --candidates.end())
					break;
				iterations--;
				i++;
			}
			
			wishlist[fac->id].push_back(Wish(i->second, p));
			unique(wishlist[fac->id]);
			std::stable_sort(wishlist[fac->id].begin(), wishlist[fac->id].end());
		}
		else {
			CUnit *unit = ai->unittable->getUnit(itFac->first);
			LOG_WW("CWishList::push failed for " << (*unit) << " categories: " << ai->unittable->debugCategories(categories))
		}
	}
}

UnitType* CWishList::top(int factory) {
	UnitType *fac = UT(ai->cb->GetUnitDef(factory)->id);
	Wish *w = &(wishlist[fac->id].front());
	return w->ut;
}

void CWishList::pop(int factory) {
	UnitType *fac = UT(ai->cb->GetUnitDef(factory)->id);
	wishlist[fac->id].erase(wishlist[fac->id].begin());
}

bool CWishList::empty(int factory) {
	std::map<int, std::vector<Wish> >::iterator itList;
	UnitType *fac = UT(ai->cb->GetUnitDef(factory)->id);
	itList = wishlist.find(fac->id);
	return itList == wishlist.end() || itList->second.empty();
}

void CWishList::unique(std::vector<Wish> &vector) {
	std::vector<Wish>::iterator i;
	Wish *w = &(vector.back());
	for (i = vector.begin(); i != --vector.end(); i++) {
		if (i->ut->id == w->ut->id) {
			i->p = (i->p > w->p) ? i->p : w->p;
			vector.pop_back();
			return;
		}
	}
}
