#include "WishList.h"

CWishList::CWishList(AIClasses *ai) {
	this->ai = ai;
}

void CWishList::push(unsigned categories, buildPriority p) {
	std::map<int, bool>::iterator itFac = ai->eco->gameFactories.begin();
	UnitType *ut = NULL;
	UnitType *fac;
	for (;itFac != ai->eco->gameFactories.end(); itFac++) {
		fac = UT(ai->call->GetUnitDef(itFac->first)->id);
		ut = ai->unitTable->canBuild(fac, categories);
		if (ut != NULL) break;
	}

	if (ut == NULL) return;

	/* Initialize new std::vector */
	if (wishlist.find(fac->id) == wishlist.end()) {
		std::vector<Wish> L;
		wishlist[fac->id] = L;
	}

	/* pushback, uniqify, stable sort */
	wishlist[fac->id].push_back(Wish(ut, p));
	unique(wishlist[fac->id]);
	std::stable_sort(wishlist[fac->id].begin(), wishlist[fac->id].end());
}

UnitType* CWishList::top(int factory) {
	UnitType *fac = UT(ai->call->GetUnitDef(factory)->id);
	Wish *w = &(wishlist[fac->id].front());
	return w->ut;
}

void CWishList::pop(int factory) {
	UnitType *fac = UT(ai->call->GetUnitDef(factory)->id);
	wishlist[fac->id].erase(wishlist[fac->id].begin());
}

bool CWishList::empty(int factory) {
	std::map<int, std::vector<Wish> >::iterator itList;
	UnitType *fac = UT(ai->call->GetUnitDef(factory)->id);
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
