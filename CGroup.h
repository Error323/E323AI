#ifndef CGROUP_H
#define CGROUP_H

#include <limits>
#include <map>

#include "headers/HEngine.h"
#include "ARegistrar.h"

class ATask;
class CUnit;
class AIClasses;
class UnitType;

struct TargetsFilter {
	unsigned int include, exclude;
		// unit category tags filter
	std::map<int, bool> *excludeId;
		// unit Ids to exclude
	int bestTarget; 
		// best target score; can be updated after passing to selectTarget()
	int candidatesLimit;
		// number of possible targets to consider
	float threatRadius;
		// radius to calculate area threat value
	float threatCeiling;
		// max threat threshold allowed at target position
	float scoreCeiling; 
		// max target score allowed; can be updated after passing to selectTarget()
	float threatFactor;
		// impacts heuristic formula which calculates target score
	float threatValue;
		// threat value at best target position; can be updated after passing to selectTarget()
	float damageFactor;
		// impacts heuristic formula which calculates target score
	float powerFactor;
		// impacts heuristic formula which calculates target score

	TargetsFilter() {
		reset();
	}	

	void reset() {
		excludeId = NULL;
		bestTarget = -1;
		candidatesLimit = std::numeric_limits<int>::max();
		include = std::numeric_limits<unsigned int>::max();
		exclude = 0;
		threatRadius = 0.0f;
		scoreCeiling = threatCeiling = std::numeric_limits<float>::max();
		threatFactor = 1.0f;
		threatValue = 0.0f;
		damageFactor = -50.0f;
		powerFactor = 0.0f; // don't care
	}
};

class CGroup: public ARegistrar {
	public:
		CGroup(AIClasses *ai): ARegistrar(counter, std::string("group")) {
			this->ai = ai;
			reset();
			counter++;
		}
		CGroup(): ARegistrar(counter, std::string("group")) {
			counter++;
		};

		~CGroup() {};

		/* Group counter */
		static int counter;
		/* Group category tags */
		unsigned int cats;
		/* Max tech level of all units in a group */
		unsigned int techlvl;
		/* Path type with the smallest slope */
		int pathType;
		/* Corresponding maxSlope */
		float maxSlope;
		/* The group strength */
		float strength;
		/* The group's moveSpeed */
		float speed;
		/* The group's buildSpeed */
		float buildSpeed;
		
		float cost;

		float costMetal;
		/* The group's footprint */
		int size;
		/* The group maxrange, buildrange */
		float range, buildRange, los;
		/* Is this group busy? */
		bool busy;
		/* The units <id, CUnit*> */
		std::map<int, CUnit*> units;
		/* Group personal bad targets (due to pathfinding issues usually) <id, frame> */
		std::map<int, int> badTargets;

		/* Reference to common AI struct */
		AIClasses *ai;

		/* Remove this group, preserve units */
		void remove();
		/* Overloaded */
		void remove(ARegistrar &unit);
		/* Reset for object re-usage */
		void reset();
		/* Reclaim an entity (unit, feature etc.) */
		void reclaim(int entity, bool feature = true);
		/* Rpair target unit */
		void repair(int target);
		/* Set this group to micro mode true/false */
		void micro(bool on);
		/* Add a unit to the group */
		void addUnit(CUnit &unit);
		/* Get the first unit of the group */
		CUnit* firstUnit();
		/* Merge another group with this group */
		void merge(CGroup &group);
		/* Enable abilities on units like cloaking */
		void abilities(bool on);
		/* See if the entire group is idle */
		bool isIdle();
		/* See if the group is microing */
		bool isMicroing();
		/* Get the position of the group center */
		float3 pos(bool force_valid = false);
		/* Group radius when units are placed within a square */
		float radius();

		void assist(ATask &task);
		
		void attack(int target, bool enqueue = false);
		
		void build(float3 &pos, UnitType *ut);
		
		void stop();
		
		void move(float3 &pos, bool enqueue = false);
		
		void guard(int target, bool enqueue = false);
		
		void wait();
		
		void unwait();
		/* Get the maximal lateral dispersion */
		int maxLength();
		/* Can group exists at this place? */
		bool canTouch(const float3&);
		/* Is position reachable by group */
		bool canReach(const float3&);
		/* Can group attack another unit? */
		bool canAttack(int uid);

		bool canAdd(CUnit *unit);

		bool canAssist(UnitType *type = NULL);
		
		bool canMerge(CGroup *group);
		/* Get area threat specific to current group */
		float getThreat(float3 &target, float radius = 0.0f);

		int selectTarget(std::vector<int> &targets, TargetsFilter &tf);
		
		int selectTarget(float search_radius, TargetsFilter &tf);
		
		bool addBadTarget(int id);
		/* Get a range usually used for scanning enemies while moving */
		float getScanRange();

		float getRange();

		CUnit* getWorstSlopeUnit() { return worstSlopeUnit; }

		CUnit* getWorstSpeedUnit() { return worstSpeedUnit; }

		/* Overloaded */
		RegistrarType regtype() { return REG_GROUP; }
		/* Output stream */
		friend std::ostream& operator<<(std::ostream &out, const CGroup &group);

	private:
		bool radiusUpdateRequired;
			// when "radius" needs to be recalculated
		float groupRadius;
			// group radius (when units clustered together)
		CUnit *worstSpeedUnit;
			// points to the slowest unit
		CUnit *worstSlopeUnit;
			// points to unit with worst slope
		/* Recalculate group properties based on new unit */
		void recalcProperties(CUnit *unit, bool reset = false);
		/* Implements rules of mering unit categories */
		void mergeCats(unsigned int);

		static bool canBeMerged(unsigned int bcats, unsigned int mcats);
};

#endif
