#ifndef CTASKHANDLER_H
#define CTASKHANDLER_H

#include <vector>
#include <map>
#include <stack>

#include "CAI.h"
#include "ARegistrar.h"
#include "headers/Defines.h"
#include "headers/HEngine.h"

class UnitType;
class AIClasses;
class CGroup;
class CUnit;

enum task{BUILD, ASSIST, ATTACK, MERGE, FACTORY_BUILD, REPAIR};

class ATask: public ARegistrar {
	public:
		ATask(AIClasses *ai): 
			ARegistrar(counter, std::string("task")) {
			this->ai = ai;
			active = false;
			counter++;
			isMoving = true;
			pos = ZEROVECTOR;
			group = NULL;
			bornFrame = ai->cb->GetCurrentFrame();
			validateInterval = 5 * 30;
			nextValidateFrame = validateInterval;
			
		}
		~ATask() {}

		bool active;
			// task is active
		int bornFrame;
			// frame when task was created
		int validateInterval;
			// validate interval in frames
		int nextValidateFrame; 
			// next frame to execute task validation
		task t;
			// type of the task: BUILD, ASSIST, ATTACK, MERGE, FACTORY_BUILD
		static int counter;
			// task counter, used as task key; shared among all AI instances
		std::list<ATask*> assisters;
			// the assisters assisting this task
		CGroup *group;
			// the group involved; for Merge task is null
		bool isMoving;
			// determine if all groups in this task are moving or not
		float3 pos;
			// the position to navigate too
			// TODO: make it as method because for assisting task this position
			// may vary depending on master task 

		/* Remove this task, unreg groups involved, and make them available
		   again */
		virtual void remove();

		/* Overload */
		void remove(ARegistrar &group);

		/* Add a group to this task */
		void addGroup(CGroup &group);

		/* Scan and micro for resources */
		bool resourceScan();

		/* Scan and micro for damaged units */
		bool repairScan();

		/* Scan and micro for enemy targets */
		bool enemyScan(bool scout);

		int lifeFrames() const;
		
		float lifeTime() const;

		/* Update this task */
		virtual void update();

		virtual bool validate() { return true; }

		RegistrarType regtype() { return REGT_TASK; } 
		
		friend std::ostream& operator<<(std::ostream &out, const ATask &task);

		AIClasses *ai;
		char buf[512];
};

class CTaskHandler: public ARegistrar {
	public:
		CTaskHandler(AIClasses *ai);
		~CTaskHandler() {};

		struct BuildTask: public ATask {
			BuildTask(AIClasses *_ai): ATask(_ai) {t = BUILD;}

			/* The build task */
			buildType bt;

			/* The ETA in frames */
			unsigned int eta;

			/* The UnitType to build */
			UnitType *toBuild;

			/* Update the build task, assumes 1 group on a task! */
			void update();

			bool validate();

			bool assistable(CGroup &group, float &travelTime);

			//void reset(float3 &pos, buildType bt, UnitType *ut);
		};

		struct FactoryTask: public ATask {
			FactoryTask(AIClasses *_ai): ATask(_ai){t = FACTORY_BUILD;}

			/* set the factorytask to wait including assisters */
			void setWait(bool wait);

			/* If a factory is idle, make sure it gets something to build */
			void update();

			bool assistable(CGroup &group);

			//void reset(CUnit &factory);
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
			
			//void reset(ATask &task);
		};

		struct AttackTask: public ATask {
			AttackTask(AIClasses *_ai): ATask(_ai) {t = ATTACK;}
			
			int target;
				// the target to attack
			std::string enemy;

			/* Update the attack task */
			void update();

			bool validate();

			//void reset(int target);
		};

		struct MergeTask: public ATask {
			MergeTask(AIClasses *_ai): ATask(_ai) {t = MERGE;}

			/* The minimal range at which groups can merge */
			float range;

			std::map<int, CGroup*> groups;

			/* Update the merge task */
			void update();

			/* overload */
			void remove();

			/* overload */
			void remove(ARegistrar& group);
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

		/* task type to string */
		static std::map<task, std::string> taskStr;

		/* Overload */
		void remove(ARegistrar &task);

		/* Add a fresh build task */
		void addBuildTask(buildType build, UnitType *toBuild, CGroup &group, float3 &pos);

		/* Add a fresh assist task */
		void addAssistTask(ATask &task, CGroup &group);

		/* Add a fresh attack task */
		void addAttackTask(int target, CGroup &group);

		/* Add a fresh merge task */
		void addMergeTask(std::map<int,CGroup*> &groups);

		/* Add a fresh factory task */
		void addFactoryTask(CGroup &group);

		ATask* getTask(CGroup &group);

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
