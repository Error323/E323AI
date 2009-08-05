#ifndef CECONOMY_H
#define CECONOMY_H

#include <map>
#include <vector>
#include <stack>

#include "CE323AI.h"
#include "ARegistrar.h"
#include "Defines.h"

class ATask;
class CGroup;

const float alpha = 0.001f;

class CEconomy: public ARegistrar {
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

		/* stalling vars, updated in updateIncomes() */
		bool mstall, estall, stalling, mexceeding, eexceeding, exceeding;

		/* Returns a fresh CGroup instance */
		CGroup* requestGroup();

		/* Overload */
		void remove(ARegistrar &group);

		/* Add a new unit */
		void addUnit(CUnit &unit);

		/* Initialize economy module */
		void init(CUnit &unit);

		/* Update the eco system */
		void update(int frame);

		/* Update averaged incomes */
		void updateIncomes(int frame);

		bool hasFinishedBuilding(CGroup &group);

	private:
		AIClasses *ai;

		/* The group container */
		std::vector<CGroup*> groups;

		/* The <unitid, vectoridx> table */
		std::map<int, int>  lookup;

		/* The free slots (CUnit instances that are zombie-ish) */
		std::stack<int>     free;

		/* Active groups ingame */
		std::map<int, CGroup*> activeGroups;

		/* buffer */
		char buf[1024];

		/* energy provider, factory, builder */
		UnitType *energyProvider, *factory, *builder, *mex, *utSolar;

		/* Altered by canAfford() */
		bool eRequest, mRequest;

		/* updateIncomes counter */
		unsigned int incomes;

		/* Can we afford to build this ? */
		bool canAffordToBuild(CGroup &group, UnitType *utToBuild);

		/* Can we afford to assist a factory ? */
		ATask* canAssistFactory(CGroup &group);

		/* See if we can help with a certain task */
		ATask* canAssist(buildType t, CGroup &group);

		/* Prevent stalling */
		void preventStalling();

};

#endif
