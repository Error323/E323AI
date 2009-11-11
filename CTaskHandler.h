#ifndef CTASKHANDLER_H
#define CTASKHANDLER_H

#include <vector>
#include <map>
#include <stack>

#include "ARegistrar.h"
#include "headers/Defines.h"
#include "headers/HEngine.h"

class UnitType;
class AIClasses;
class CGroup;
class CUnit;

enum task{BUILD, ASSIST, ATTACK, MERGE, FACTORY_BUILD};

class ATask: public ARegistrar {
	public:
		ATask(AIClasses *ai): 
			ARegistrar(counter, std::string("task")) {
			counter++;
			isMoving = true;
			pos = ZEROVECTOR;
			this->ai = ai;
		}
		~ATask(){}

		/* The task in {BUILD, ASSIST, ATTACK, MERGE, FACTORY_BUILD} */
		task t;

		/* Task counter */
		static int counter;

		/* The assisters assisting this task */
		std::list<ATask*> assisters;

		/* The group(s) involved */
		CGroup *group;

		/* Determine if all groups in this task are moving or not */
		bool isMoving;

		/* The position to navigate too */
		float3 pos;

		/* Remove this task, unreg groups involved, and make them available
		 * again 
		 */
		void remove();

		/* Overload */
		void remove(ARegistrar &group);

		/* Add a group to this task */
		void addGroup(CGroup &group);

		/* Scan and micro for resources */
		void resourceScan();

		/* Scan and micro for enemy targets */
		void enemyScan(bool scout);

		/* Update this task */
		virtual void update() = 0;

		AIClasses *ai;
		char buf[512];
		friend std::ostream& operator<<(std::ostream &out, const ATask &task);
};

class CTaskHandler: public ARegistrar {
	public:
		CTaskHandler(AIClasses *ai);
		~CTaskHandler(){};

		struct BuildTask: public ATask {
			BuildTask(AIClasses *_ai): ATask(_ai){t = BUILD; timer = 0;}

			/* The build task */
			buildType bt;

			/* The ETA in frames */
			unsigned eta;

			/* Time this buildtask has been active */
			unsigned timer;

			/* The UnitType to build */
			UnitType *toBuild;

			/* Update the build task, assumes 1 group on a task! */
			void update();

			bool assistable(CGroup &group, float &travelTime);

			void reset(float3 &pos, buildType bt, UnitType *ut);
		};

		struct FactoryTask: public ATask {
			FactoryTask(AIClasses *_ai): ATask(_ai){t = FACTORY_BUILD;}

			CUnit *factory;

			/* set the factorytask to wait including assisters */
			void setWait(bool wait);

			/* If a factory is idle, make sure it gets something to build */
			void update();

			bool assistable(CGroup &group);

			void reset(CUnit &factory);
		};

		struct AssistTask: public ATask {
			AssistTask(AIClasses *_ai): ATask(_ai) {t = ASSIST;}

			/* The (build)task to assist */
			ATask *assist;

			/* Update the assist task */
			void update();

			/* overload */
			void remove();

			/* overload */
			void remove(ARegistrar& group);
			
			void reset(ATask &task);
		};

		struct AttackTask: public ATask {
			AttackTask(AIClasses *_ai): ATask(_ai) {t = ATTACK;}

			/* The target to attack */
			int target;

			/* Update the attack task */
			void update();

			std::string enemy;

			void reset(int target);
		};

		struct MergeTask: public ATask {
			MergeTask(AIClasses *_ai): ATask(_ai) {t = MERGE;}

			/* The maximal range from the target when attacking */
			float range;

			std::vector<CGroup*> groups;

			/* Update the merge task */
			void update();

			void reset(std::vector<CGroup*> &groups);
		};

		/* The -to be removed- tasks */
		std::stack<ATask*> obsoleteTasks;

		/* The active tasks per type */
		std::map<int, BuildTask*>   activeBuildTasks;
		std::map<int, AssistTask*>  activeAssistTasks;
		std::map<int, AttackTask*>  activeAttackTasks;
		std::map<int, MergeTask*>   activeMergeTasks;
		std::map<int, FactoryTask*> activeFactoryTasks;

		/* build type to string */
		static std::map<buildType, std::string> buildStr;

		/* Tasks to string */
		std::map<task, std::string> taskStr;

		/* Overload */
		void remove(ARegistrar &task);

		/* Add a fresh build task */
		void addBuildTask(buildType build, UnitType *toBuild, CGroup &group, float3 &pos);

		/* Add a fresh assist task */
		void addAssistTask(ATask &task, CGroup &group);

		/* Add a fresh attack task */
		void addAttackTask(int target, CGroup &group);

		/* Add a fresh merge task */
		void addMergeTask(std::vector<CGroup*> &groups);

		/* Add a fresh factory task */
		void addFactoryTask(CUnit &factory);

		/* Remove a task */
		void removeTask(CGroup &group);

		/* Get the group destination */
		float3 getPos(CGroup &group);

		/* Update call */
		void update();

	private:
		AIClasses *ai;
		char buf[1024];

		/* The active tasks to update */
		std::map<int, ATask*> activeTasks;

		/* The group to task table */
		std::map<int, ATask*> groupToTask;

		/* Calculate avg range and pos of groups */
		static void getGroupsPos(std::vector<CGroup*> &groups, float3 &pos);
};

#endif
