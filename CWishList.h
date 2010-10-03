#ifndef WISHLIST_H
#define WISHLIST_H

#include <vector>
#include <queue>
#include <map>
#include <algorithm>

#include "headers/Defines.h"

class AIClasses;
class UnitType;

struct Wish {
	enum NPriority { LOW = 0, NORMAL, HIGH };

	unitCategory goalCats;
	NPriority p;
	UnitType* ut;

	Wish() {
		goalCats.reset();
		p = NORMAL;
		ut = NULL;
	}
	Wish(UnitType *ut, NPriority p, unitCategory gcats) {
		this->ut = ut;
		this->p  = p;
		this->goalCats = gcats;
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
	void push(unitCategory categories, Wish::NPriority p);
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
