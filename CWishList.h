#ifndef WISHLIST_H
#define WISHLIST_H

#include "CE323AI.h"

class CWishList {
	friend class CEconomy;

	public:
		CWishList(AIClasses *ai);
		~CWishList() {}

		/* Insert a unit in the wishlist, sorted by priority p */
		void push(unsigned int categories, buildPriority p);

		/* Remove the top unit from the wishlist */
		void pop(int factory);

		/* Is empty ? */
		bool empty(int factory);

		/* View the top unit from the wishlist */
		UnitType* top(int factory);

	private:
		struct Wish {
			buildPriority p;
			UnitType *ut;

			Wish(UnitType *ut, buildPriority p) {
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

		void unique(std::vector<Wish> &vector);

		std::map<int, std::vector<Wish> > wishlist;  /* <factory type, wishes> */

		AIClasses *ai;

		char buf[1024];
};

#endif
