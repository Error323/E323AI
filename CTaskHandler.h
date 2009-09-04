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
		ATask(): 
			ARegistrar(counter) {
			counter++;
			isMoving = true;
			pos = ZEROVECTOR;
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

		/* Reset this task for reuse */
		void reset();

		/* Is this task assistable */
		bool assistable();

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
			BuildTask(){}

			/* The build task */
			buildType bt;

			/* The UnitType to build */
			UnitType *toBuild;

			/* Update the build task, assumes 1 group on a task! */
			void update();

			bool assistable(CGroup &group);

			void reset(float3 &pos, buildType bt, UnitType *ut);
		};

		struct FactoryTask: public ATask {
			FactoryTask(){}

			CUnit *factory;

			/* Let the factorytask wait */
			bool wait;

			/* set the factorytask to wait including assisters */
			void setWait(bool wait);

			/* If a factory is idle, make sure it gets something to build */
			void update();

			bool assistable(CGroup &group);

			void reset(CUnit &factory);
		};

		struct AssistTask: public ATask {
			AssistTask(){}

			/* The (build)task to assist */
			ATask *assist;

			/* Update the assist task */
			void update();

			void remove();
			
			void reset(ATask &task);
		};

		struct AttackTask: public ATask {
			AttackTask(){}

			/* The target to attack */
			int target;

			/* Update the attack task */
			void update();

			std::string enemy;

			void reset(int target);
		};

		struct MergeTask: public ATask {
			MergeTask(){}

			/* The maximal range from the target when attacking */
			float range;

			std::vector<CGroup*> groups;

			/* Update the merge task */
			void update();

			void reset(std::vector<CGroup*> &groups);
		};

		/* The ATask container per task type */
		std::map<task, std::vector<ATask*> > tasks;

		/* The <task, taskid, vectorid> table */
		std::map<task, std::map<int, int> > lookup;

		/* The free slots per task type */
		std::map<task, std::stack<int> >    free;

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

		/* Request an unoccupied task */
		ATask* requestTask(task t);

		/* Calculate avg range and pos of groups */
		static void getGroupsPos(std::vector<CGroup*> &groups, float3 &pos);
};

#endif
