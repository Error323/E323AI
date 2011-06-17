#include "Build.h"

#include "../CUnit.h"
#include "../CGroup.h"
#include "../CUnitTable.h"
#include "../CPathfinder.h"
#include "../CEconomy.h"
#include "../CTaskHandler.h"


std::map<buildType, std::string> BuildTask::buildStr;

BuildTask::BuildTask(AIClasses *_ai, buildType build, UnitType *toBuild, CGroup &group, float3 &pos): ATask(_ai) {
	if (buildStr.empty()) {
		buildStr[BUILD_MPROVIDER] = std::string("MPROVIDER");
		buildStr[BUILD_EPROVIDER] = std::string("EPROVIDER");
		buildStr[BUILD_AA_DEFENSE] = std::string("AA_DEFENSE");
		buildStr[BUILD_AG_DEFENSE] = std::string("AG_DEFENSE");
		buildStr[BUILD_UW_DEFENSE] = std::string("UW_DEFENSE");
		buildStr[BUILD_FACTORY] = std::string("FACTORY");
		buildStr[BUILD_MSTORAGE] = std::string("MSTORAGE");
		buildStr[BUILD_ESTORAGE] = std::string("ESTORAGE");
		buildStr[BUILD_MISC_DEFENSE] = std::string("MISC_DEFENSE");
		buildStr[BUILD_IMP_DEFENSE] = std::string("IMPORTANT_DEFENSE");
	}

	this->t       = TASK_BUILD;
	this->pos     = pos;
	this->bt      = build;
	this->toBuild = toBuild;

	float eta = ai->pathfinder->getETA(group, pos);
	if (eta == std::numeric_limits<float>::max())
		this->eta = 0;
	else
		this->eta = int((eta + 100.0f) * 1.2f);

	this->building = false;

	addGroup(group);
}

bool BuildTask::onValidate() {
	if (isMoving) {
		// decline task if metal spot is occupied by firendly unit
		if ((toBuild->cats&MEXTRACTOR).any()) {
			int numUnits = ai->cb->GetFriendlyUnits(&ai->unitIDs[0], pos, 1.1f * ai->cb->GetExtractorRadius());
			for (int i = 0; i < numUnits; i++) {
				const int uid = ai->unitIDs[i];
				const UnitDef *ud = ai->cb->GetUnitDef(uid);
				if ((UC(ud->id)&MEXTRACTOR).any()) {
					return false;
				}
			}
		}
	}
	else {
		CGroup *group = firstGroup();
		if (ai->economy->hasFinishedBuilding(*group))
			return false;
		if (lifeFrames() > eta && !ai->economy->hasBegunBuilding(*group)) {
			LOG_WW("BuildTask::update assuming buildpos blocked for group " << (*group))
			return false;
		}
	}

	return true;
}

void BuildTask::onUpdate() {
	CGroup *group = firstGroup();

	if (group->isMicroing()) {
		if (group->isIdle()) {
			CUnit* unit = group->firstUnit();
			group->micro(false); // if idle, our micro is done
			eta += unit->microingFrames;
			unit->microingFrames = 0; // reset
		}
		else
			return; // if microing, break
	}

	if (!building) {
		if (isMoving) {
			if (!resourceScan()) {
				const float3 gpos = group->pos();
				if (gpos.distance2D(pos) <= group->buildRange) {
					isMoving = false;
					ai->pathfinder->remove(*group);
				}
				else
					repairScan();
			}
		}

		if (!isMoving && !group->isMicroing()) {
			if (group->build(pos, toBuild)) {
				building = true;
				group->micro(true);
			}
		}
	}

	if (group->isIdle()) {
		// make builder react faster when job is finished
		if (!onValidate())
			remove();
	}
}

bool BuildTask::assistable(CGroup &assister, float &travelTime) const {
	if (!toBuild->def->canBeAssisted)
		return false;
	if (isMoving)
		return false;
	if (assisters.size() > 1 && (bt == BUILD_AG_DEFENSE || bt == BUILD_AA_DEFENSE || bt == BUILD_MISC_DEFENSE))
		return false;

	CGroup *group = firstGroup();

	float buildSpeed = group->buildSpeed;
	std::list<ATask*>::const_iterator it;
	for (it = assisters.begin(); it != assisters.end(); ++it)
		buildSpeed += (*it)->firstGroup()->buildSpeed;

	// NOTE: we should calculate distance to group position instead of target
	// position because build place can be reachable within build range only
	float3 gpos = group->pos();
	float buildTime = (toBuild->def->buildTime / buildSpeed) * 32.0f;

	/* travelTime + 5 seconds to make it worth the trip */
	travelTime = ai->pathfinder->getETA(assister, gpos) + 150.0f;

	return (buildTime > travelTime);
}

void BuildTask::onUnitDestroyed(int uid, int attacker) {
	if (ai->cb->UnitBeingBuilt(uid)) {
		CUnit* unit = ai->unittable->getUnit(uid);
		if (unit && unit->builtBy == firstGroup()->firstUnit()->key)
			remove();
	}
}

void BuildTask::toStream(std::ostream& out) const {
	out << "BuildTask(" << key << ") " << buildStr[bt];
	out << "(" << toBuild->def->humanName << ") ETA(" << eta << ")";
	out << " lifetime(" << lifeFrames() << ") ";
	CGroup *group = firstGroup();
	if (group)
		out << (*group);
}
