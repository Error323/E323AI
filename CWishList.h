#ifndef WISHLIST_H
#define WISHLIST_H

#include <vector>
#include <queue>
#include <map>
#include <algorithm>

#include "headers/Defines.h"

class AIClasses;
struct UnitType;

struct Wish {
	enum NPriority { LOW = 0, NORMAL, HIGH };

	unitCategory goalCats;
	NPriority p;
	UnitType* ut;

	Wish():p(NORMAL),ut(NULL) {}
	Wish(UnitType* ut, NPriority p, unitCategory gcats):goalCats(gcats) {
		this->ut = ut;
		this->p  = p;
	}

	bool operator< (const Wish &w) const {
		return p > w.p;
	}

	bool operator== (const Wish &w) const {
		return p == w.p;
	}
};

class CWishList {
	
public:
	CWishList(AIClasses *ai);
	~CWishList();

	/* Insert a unit in the wishlist, sorted by priority p */
	void push(unitCategory include, unitCategory exclude = 0, Wish::NPriority p = Wish::NORMAL);
	/* Remove the top unit from the wishlist */
	void pop(int factory);
	/* Is empty ? */
	bool empty(int factory);
	/* View the top unit from the wishlist */
	Wish top(int factory);

private:
	AIClasses* ai;
	
	int maxWishlistSize;
	
	std::map<int, std::vector<Wish> > wishlist; /* <factory_def_id, wish> */
	
	void unique(std::vector<Wish>& vector);
};

#endif
