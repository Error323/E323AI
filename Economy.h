#ifndef ECONOMY_H
#define ECONOMY_H

#include "E323AI.h"

#define alpha 0.001f

class CEconomy {
	public:
		CEconomy(AIClasses *ai);
		~CEconomy(){};

		/* overal mNow averaged over 5 logical frames */
		float mNow, mNowSummed;
		/* overal eNow averaged over 5 logical frames */
		float eNow, eNowSummed;
		/* overal mIncome averaged over 5 logical frames */
		float mIncome, mIncomeSummed;
		/* overal eIncome averaged over 5 logical frames */
		float eIncome, eIncomeSummed;
		/* total units mIncome averaged over 5 logical frames */
		float uMIncome, uMIncomeSummed;
		/* total units eIncome averaged over 5 logical frames */
		float uEIncome, uEIncomeSummed;
		/* metal usage averaged over 5 logical frames */
		float mUsage, mUsageSummed;
		/* energy usage averaged over 5 logical frames */
		float eUsage, eUsageSummed;
		/* metal storage */
		float mStorage;
		/* energy storage */
		float eStorage;

		/* Initialize economy module */
		void init(int unit);

		/* Update the eco system */
		void update(int frame);

		/* Update averaged incomes */
		void updateIncomes(int frame);

		/* Add a unittype to our wishlist */
		void addWish(UnitType *fac, UnitType *ut, buildPriority p);

		/* Put unit in a remove vector */
		void removeIdleUnit(int unit);

		/* Remove guards of a unit */
		void removeMyGuards(int unit);

		/* Eco related units */
		std::map<int, UnitType*> gameFactories;
		std::map<int, UnitType*> gameBuilders;
		std::map<int, UnitType*> gameMetal;
		std::map<int, UnitType*> gameEnergy;
		std::map<int, UnitType*> gameIdle;
		std::map<int, UnitType*> gameWaiting;
		std::map<int, int>       gameGuarding;

		/* To remove */
		std::vector<int> removeFromIdle;
		std::vector<int> removeFromGuarding;
		std::vector<int> removeFromWaiting;

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

		/* Get the amount of guarding units guarding this unit */
		int getGuardings(int unit);

		/* See if we can help (guard) a unit with a certain task */
		bool canHelp(task t, int helper, int &unit, UnitType *helpBuild);

		/* Holds wishlists, one for each factory type */
		std::map<int, std::priority_queue<Wish> > wishlist;

		/* buffer */
		char buf[1024];

		/* energy provider, factory, builder */
		UnitType *energyProvider, *factory, *builder, *hammer;

		/* Altered by canAfford() */
		bool eRequest, mRequest;

		/* updateIncomes counter */
		unsigned int incomes;
};

#endif
