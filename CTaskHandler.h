#ifndef CTASKHANDLER_H
#define CTASKHANDLER_H

#include <vector>
#include <map>
#include <stack>

#include "CE323AI.h"
#include "ARegistrar.h"

enum task{BUILD, ASSIST, ATTACK, MERGE, FACTORY_BUILD};

class ATask: public ARegistrar {
	public:
		ATask(task _t, float3 &_pos): 
			ARegistrar(counter), t(_t), pos(_pos) {
			counter++;
			isMoving = true;
		}
		~ATask(){}

		static int counter;

		/* The task in {BUILD, ASSIST, ATTACK, MERGE, FACTORY_BUILD} */
		task t;

		/* The group(s) involved */
		std::map<int, CGroup*> groups;

		/* Determine if the group is moving or in the final stage */
		std::map<int, bool> moving;

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
		void reset(float3 &p);

		/* Update this task */
		virtual void update() = 0;
};

class CTaskHandler: public ARegistrar {
	public:
		CTaskHandler(AIClasses *ai);
		~CTaskHandler(){};

		struct BuildTask: public ATask {
			BuildTask(float3 &pos, buildType _bt, UnitType *_toBuild);

			/* The build task */
			buildType bt;

			/* The UnitType to build */
			UnitType *toBuild;

			/* Update the build task, assumes 1 group on a task! */
			void update();
		};

		struct FactoryTask: public ATask {
			FactoryTask(CUnit &unit);

			CUnit *factory;

			/* If a factory is idle, make sure it gets something to build */
			void update();
		};

		struct AssistTask: public ATask {
			AssistTask(ATask &task);

			/* The (build)task to assist */
			ATask *assist;

			/* Update the assist task */
			void update();
		};

		struct AttackTask: public ATask {
			AttackTask(int _target);

			/* The target to attack */
			int target;

			/* Update the attack task */
			void update();
		};

		struct MergeTask: public ATask {
			MergeTask(float3 &pos, float _range): 
				ATask(MERGE, pos), range(_range) {}

			/* The maximal range from the target when attacking */
			float range;

			/* Update the merge task */
			void update();
		};

		/* Controls which task may be updated (round robin-ish) */
		unsigned updateCount;

		/* The ATask container per task type */
		std::map<task, std::vector<ATask> > taskContainer;

		/* The <task, taskid, vectorid> table */
		std::map<task, std::map<int, int> > lookup;

		/* The free slots per task type */
		std::map<task, std::stack<int> >    free;

		/* The active tasks per type */
		std::map<int, BuildTask*> activeBuildTasks;
		std::map<int, AssistTask*> activeAssistTasks;
		std::map<int, AttackTask*> activeAttackTasks;
		std::map<int, MergeTask*> activeMergeTasks;
		std::map<int, FactoryTask*> activeFactoryTasks;

		/* Overload */
		void remove(ARegistrar &task);

		/* Add a fresh build task */
		void addBuildTask(buildType build, UnitType *toBuild, std::vector<CGroup*> &groups, float3 pos = float3(0.0f, 0.0f, 0.0f));

		/* Add a fresh assist task */
		void addAssistTask(ATask &task, std::vector<CGroup*> &groups);

		/* Add a fresh attack task */
		void addAttackTask(int target, std::vector<CGroup*> &groups);

		/* Add a fresh merge task */
		void addMergeTask(std::vector<CGroup*> &groups);

		/* Add a fresh factory task */
		void addFactoryTask(CUnit &factory);

		/* Update call */
		void update();

	private:
		AIClasses *ai;
		char buf[1024];

		/* Tasks to string */
		std::map<task, std::string> taskStr;

		/* The active tasks to update */
		std::map<int, ATask*>               activeTasks;

		/* Add a task */
		ATask* addTask(ATask &t, std::vector<CGroup*> &groups);

		/* Calculate avg range and pos of groups */
		void getGroupsRangeAndPos(std::vector<CGroup*> &groups, float &range, float3 &pos);
};

#endif
