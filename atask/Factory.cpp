#include "Factory.h"

#include "../CGroup.h"
#include "../CWishList.h"
#include "../CUnit.h"
#include "../CUnitTable.h"
#include "../CConfigParser.h"

FactoryTask::FactoryTask(AIClasses *_ai, CGroup& group): ATask(_ai) {
	t = TASK_FACTORY;
	// NOTE: currently if factories are joined into one group then assisters 
	// will assist the first factory only
	//factoryTask->pos = group.pos();
	pos = group.firstUnit()->pos();
	validateInterval = 10 * 30;
	addGroup(group);
}

bool FactoryTask::assistable(CGroup &assister) {
	CGroup *group = firstGroup();

	if (!group->firstUnit()->def->canBeAssisted)
		return false; // no physical ability
	if (assister.firstUnit()->type->cats&COMMANDER)
		return true; // commander must stay at the base
	if (assisters.size() >= std::min(ai->cfgparser->getState() * 2, FACTORY_ASSISTERS)) {
		if (assister.cats&AIR) {
			// try replacing existing assisters with aircraft assisters to free
			// factory exit...
			std::list<ATask*>::iterator it;
			for (it = assisters.begin(); it != assisters.end(); ++it) {
				ATask *task = *it;
				if (!(task->firstGroup()->cats&(AIR|COMMANDER))) {
					task->remove();
					return true;
				}
			}		
		}
		return false;
	}
	
	return true;
}

bool FactoryTask::onValidate() {
	int numUnits = ai->cb->GetFriendlyUnits(&ai->unitIDs[0], pos, 16.0f);
	if (numUnits > 0) {
		int factoryID = firstGroup()->firstUnit()->key;
		for (int i = 0; i < numUnits; i++) {
    		int uid = ai->unitIDs[i];
    		
    		if (factoryID == uid)
    			continue;
    		
    		if (!ai->cb->UnitBeingBuilt(uid)) {
    			CUnit *unit = ai->unittable->getUnit(uid);
    			if (unit) {
    				if (ai->unittable->canPerformTask(*unit))
    					// our unit stalled a factory
    					return false;
    			}
    			else
    				// some stupid allied unit stalled out factory
    				return false;
    		}
		}
	}
	return true;
}

void FactoryTask::onUpdate() {
	std::map<int, CUnit*>::iterator i;
	CGroup *group = firstGroup();
	CUnit *factory;
	
	for(i = group->units.begin(); i != group->units.end(); ++i) {
		factory = i->second;
		if (ai->unittable->idle[factory->key] && !ai->wishlist->empty(factory->key)) {
			Wish w = ai->wishlist->top(factory->key);
			ai->wishlist->pop(factory->key);
			if (factory->factoryBuild(w.ut)) {
				ai->unittable->unitsBuilding[factory->key] = w;
			}
		}
	}
}

void FactoryTask::setWait(bool on) {
	std::map<int, CUnit*>::iterator ui;
	std::list<ATask*>::iterator ti;
	CGroup *group = firstGroup();
	CUnit *factory;

	for (ui = group->units.begin(); ui != group->units.end(); ++ui) {
		factory = ui->second;
		if(on)
			factory->wait();
		else
			factory->unwait();
	}

	for (ti = assisters.begin(); ti != assisters.end(); ++ti) {
		if ((*ti)->isMoving) continue;
		if(on)
			(*ti)->firstGroup()->wait();
		else
			(*ti)->firstGroup()->unwait();
	}
}

void FactoryTask::toStream(std::ostream& out) const {
	out << "FactoryTask(" << key << ") ";
	CGroup *group = firstGroup();
	if (group)
		out << (*group);
}
