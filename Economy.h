#ifndef ECONOMY_H
#define ECONOMY_H

#include "E323AI.h"

class CEconomy {
	public:
		CEconomy(AIClasses *ai);
		~CEconomy(){};

		/* overal mNow averaged over 5 logical frames */
		float mNow;
		/* overal eNow averaged over 5 logical frames */
		float eNow;
		/* overal mIncome averaged over 5 logical frames */
		float mIncome;
		/* overal eIncome averaged over 5 logical frames */
		float eIncome;
		/* total units mIncome averaged over 5 logical frames */
		float uMIncome;
		/* total units eIncome averaged over 5 logical frames */
		float uEIncome;
		/* metal usage averaged over 5 logical frames */
		float mUsage;
		/* energy usage averaged over 5 logical frames */
		float eUsage;

		/* Initialize economy module */
		void init(int unit);

		/* Update the eco system */
		void update(int frame);

		/* Update averaged incomes */
		void updateIncomes(int N);

		/* Add a unittype to our wishlist */
		void addWish(UnitType *fac, UnitType *ut, buildPriority p);

		/* Put unit in a remove vector */
		void removeIdleUnit(int unit);

		/* Eco related units */
		std::map<int, UnitType*> gameFactories;
		std::map<int, UnitType*> gameBuilders;
		std::map<int, UnitType*> gameMetal;
		std::map<int, UnitType*> gameEnergy;
		std::map<int, UnitType*> gameIdle;
		std::map<int, int>       gameGuarding;

		/* To remove */
		std::vector<int> removeFromIdle;
		std::vector<int> removeFromGuarding;

	private:
		AIClasses *ai;

		struct Wish {
			buildPriority p;
			UnitType *ut;

			Wish(UnitType *ut, buildPriority p) {
				this->ut = ut;
				this->p  = p;
			}

			bool operator< (const Wish &w) const {
				return p < w.p;
			}
		};

		/* Can we afford to build this ? */
		bool canAffordToBuild(UnitType *builder, UnitType *toBuild);
		
		/* Holds wishlists, one for each factory type */
		std::map<int, std::priority_queue<Wish> > wishlist;

		/* buffer */
		char buf[1024];

		/* energy provider, factory, builder */
		UnitType *energyProvider, *factory, *builder;
};

#endif
