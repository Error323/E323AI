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
class float3;

const float alpha = 0.2f;
const float beta = 0.05f;

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
		
		/* State, a sort of measurement how far advanced we are */
		int state;

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

		/* See if this group has finished a building */
		bool hasFinishedBuilding(CGroup &group);

		/* See if this group begun building */
		bool hasBegunBuilding(CGroup &group);

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

		/* Altered by canAfford() */
		bool eRequest, mRequest;

		/* Is this a windmap ? */
		bool windmap;

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

		/* See what is best for our economy */
		void controlMetalMakers();

		/* build or assist on a certain task */
		void buildOrAssist(CGroup &group, buildType bt, unsigned include, unsigned exclude = 0);

		/* build or assist a new lab */
		void buildNewFactory(CGroup &group);

		/* See if a buildtask is in progress */
		bool taskInProgress(buildType bt);

};

#endif
