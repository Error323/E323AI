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
			// type of the task: BUILD, ASSIST, ATTACK, etc.
		static int counter;
			// task counter, used as task key; shared among all AI instances
		std::list<ATask*> assisters;
			// the assisters assisting this task
		CGroup *group;
			// the group involved; for Merge task it is master-group
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
		bool enemyScan();

		int lifeFrames() const;
		
		/* Task lifetime in sec */
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
		~CTaskHandler();

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
			/* overload */
			bool validate();
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
		};

		struct AttackTask: public ATask {
			AttackTask(AIClasses *_ai): ATask(_ai) {t = ATTACK;}
			
			bool urgent;
				// if task is urgent then disable enemy scanning while moving
			int target;
				// the target to attack
			std::string enemy;
				// enemy user name
			/* Update the attack task */
			void update();
			/* overload */
			bool validate();
		};

		struct MergeTask: public ATask {
			MergeTask(AIClasses *_ai): ATask(_ai) { t = MERGE; isRetreating = false; }

			bool isRetreating;
				// are groups retreating?
			float range;
				// the minimal range at which groups can merge
			std::map<int, CGroup*> groups;

			std::map<int, bool> mergable;

			bool reelectMasterGroup();
			/* Update the merge task */
			void update();
			/* overload */
			bool validate();
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
		bool addAttackTask(int target, CGroup &group, bool urgent = false);

		/* Add a fresh merge task */
		void addMergeTask(std::map<int,CGroup*> &groups);

		/* Add a fresh factory task */
		void addFactoryTask(CGroup &group);

		ATask* getTask(CGroup &group);

		ATask* getTaskByTarget(int);

		/* Get the group destination */
		float3 getPos(CGroup &group);

		/* Update call */
		void update();

	private:
		AIClasses *ai;

		/* The active tasks to update */
		std::map<int, ATask*> activeTasks;
		/* The group to task table */
		std::map<int, ATask*> groupToTask;

		int statsMaxTasks;
		int statsMaxActiveTasks;
};

#endif
