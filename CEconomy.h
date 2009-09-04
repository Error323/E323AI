#ifndef CECONOMY_H
#define CECONOMY_H

#include <map>
#include <vector>
#include <stack>

#include "ARegistrar.h"

#include "headers/Defines.h"

class ATask;
class CGroup;
class CUnit;
class AIClasses;
class UnitType;

const float alpha = 0.0f;
const float beta = 0.00f;

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
		float mUsage, mUsageSummed, mStart;
		/* energy usage averaged over 5 logical frames */
		float eUsage, eUsageSummed, eStart;
		/* metal storage */
		float mStorage;
		/* energy storage */
		float eStorage;

		/* stalling/exceeding vars, updated in updateIncomes() */
		bool mstall, estall, mexceeding, eexceeding;

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
		void updateIncomes(int frame = 100);

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

		/* energy provider, factory, builder */
		UnitType *energyProvider;

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

		void buildMprovider(CGroup &group);
		void buildEprovider(CGroup &group);
		void buildOrAssist(buildType bt, UnitType *ut,  CGroup &group);
		bool taskInProgress(buildType bt);

};

#endif
