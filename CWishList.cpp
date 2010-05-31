#include "CWishList.h"

#include "CAI.h"
#include "CUnitTable.h"
#include "CUnit.h"
#include "CConfigParser.h"
#include "CEconomy.h"

CWishList::CWishList(AIClasses *ai) {
	this->ai = ai;
	maxWishlistSize = 0;
}

CWishList::~CWishList() {
	LOG_II("CWishList::Stats MaxWishListSize = " << maxWishlistSize)
}

void CWishList::push(unsigned categories, buildPriority p) {
	std::map<int, CUnit*>::iterator itFac = ai->unittable->factories.begin();
	UnitType *fac;
	for (;itFac != ai->unittable->factories.end(); ++itFac) {
		fac = itFac->second->type;
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
			int iterations = candidates.size();
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
			
			wishlist[fac->id].push_back(Wish(i->second, p, categories));
			unique(wishlist[fac->id]);
			std::stable_sort(wishlist[fac->id].begin(), wishlist[fac->id].end());
		}
		else {
			CUnit *unit = ai->unittable->getUnit(itFac->first);
			LOG_WW("CWishList::push failed for " << (*unit) << " categories: " << ai->unittable->debugCategories(categories))
		}
	}

	maxWishlistSize = std::max<int>(maxWishlistSize, wishlist.size());
}

Wish CWishList::top(int factory) {
	const UnitDef *udef = ai->cb->GetUnitDef(factory);
	return wishlist[udef->id].front();
}

void CWishList::pop(int factory) {
	const UnitDef *udef = ai->cb->GetUnitDef(factory);
	wishlist[udef->id].erase(wishlist[udef->id].begin());
}

bool CWishList::empty(int factory) {
	std::map<int, std::vector<Wish> >::iterator itList;
	const UnitDef *udef = ai->cb->GetUnitDef(factory);
	itList = wishlist.find(udef->id);
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
