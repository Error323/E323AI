#include "Merge.h"

#include "../CGroup.h"
#include "../CDefenseMatrix.h"
#include "../CPathfinder.h"

MergeTask::MergeTask(AIClasses *_ai, std::list<CGroup*>& groups): ATask(_ai) {
	t = TASK_MERGE;
	isRetreating = false;
	range = 0.0f;
	masterGroup = NULL;

	std::list<CGroup*>::iterator it;
	for (it = groups.begin(); it != groups.end(); ++it) {
		CGroup *group = *it;
		addGroup(*group);
		range += group->radius();
	}
	
	unsigned int cats = firstGroup()->cats;
	if ((cats&AIR) && !(cats&ASSAULT)) {
		// FIXME: prefer no hardcoding
		range = 600.0f;
	}
	else {
		range = range + groups.size() * FOOTPRINT2REAL;
	}
}

// called on Group removing
void MergeTask::remove(ARegistrar &group) {
	CGroup *g = dynamic_cast<CGroup*>(&group);
	
	assert(g != NULL);	

	bool isReelectionNeeded = (masterGroup && masterGroup->key == g->key);
	
	mergable.erase(g->key);
	removeGroup(*g);

	if (isReelectionNeeded) {
		masterGroup = NULL;
		reelectMasterGroup();
	}
}

bool MergeTask::onValidate() {
	return reelectMasterGroup();
}

void MergeTask::onUpdate() {
	/* See which groups can be merged already */
	std::list<CGroup*>::iterator it;
	for (it = groups.begin(); it != groups.end(); ++it) {
		CGroup *g = *it;
		
		if (g->isMicroing())
			continue;
		
		if (pos.distance2D(g->pos()) < range) {
			mergable[g->key] = g;
			g->micro(true);
		}
	}
	
	/* We have at least two groups, now we can merge */
	if (mergable.size() >= 2) {
		std::vector<int> keys;
		std::map<int, CGroup*>::iterator it;
		
		// get keys because while merging "mergable" is reducing...
		for (it = mergable.begin(); it != mergable.end(); ++it) {
			keys.push_back(it->first);
		}
		
		for (int i = 0; i < keys.size(); i++) {
			int key = keys[i];
			if (key != masterGroup->key) {
				CGroup *g = mergable[key];
				LOG_II("MergeTask::update merging " << (*g) << " with " << (*masterGroup))
				// NOTE: group being merged is automatically removed
				masterGroup->merge(*g);
			}
		}
		
		assert(mergable.size() == 1);
		mergable.clear();
		masterGroup->micro(false);
	}

	// if only one (or none) group remains, merging is no longer possible,
	// remove the task, unreg groups...
	if (groups.size() <= 1)
		ATask::remove();
}

bool MergeTask::reelectMasterGroup() {
	if (groups.size() <= 1)
		return false;

	bool reelect = true;

	if (masterGroup && !isRetreating) {
		float threat = masterGroup->getThreat(pos, masterGroup->radius());
		// if threat is 2x smaller than group strength then do nothing
		if (threat <= EPS || (masterGroup->strength / threat) > 2.0f)
			reelect = false;
	}
	
	if (reelect) {
		float minThreat = std::numeric_limits<float>::max();
		float maxDistance = std::numeric_limits<float>::min();
		CGroup *bestGroup = NULL;
		std::list<CGroup*>::iterator it;

		for (it = groups.begin(); it != groups.end(); ++it) {
			CGroup *g = *it;
			float3 gpos = g->pos();
			float threat = g->getThreat(gpos, g->radius());
			float distance = ai->defensematrix->distance2D(gpos);

			if (distance > maxDistance)
				maxDistance = distance;

			if (threat < minThreat) {
				bestGroup = g;
				minThreat = threat;
				isRetreating = (distance + EPS) < maxDistance;
			}
		}

		if (bestGroup && (masterGroup == NULL || masterGroup->key != bestGroup->key)) {
			masterGroup = bestGroup;
			pos = bestGroup->pos(true);
			for (it = groups.begin(); it != groups.end(); ++it) {
				CGroup *g = *it;
				ai->pathfinder->remove(*g);
				if (!ai->pathfinder->addGroup(*g))
					return false;
			}
		}
	}
	
	return (masterGroup != NULL);
}

void MergeTask::toStream(std::ostream& out) const {
	out << "MergeTask(" << key << ") range(" << range<<") pos(" << pos.x << ", " << pos.z << ") groups(" << groups.size() << ") { ";
	std::list<CGroup*>::const_iterator i;
	for (i = groups.begin(); i != groups.end(); ++i) {
		out << (*(*i)) << " ";
	}
	out << "}";
}
