#include "CCoverageHandler.h"

#include "ReusableObjectFactory.hpp"
#include "CUnitTable.h"
#include "CConfigParser.h"


void CCoverageHandler::update() {
	std::map<CCoverageCell::NType, std::list<CCoverageCell*> >::iterator itLayer;
	std::list<CCoverageCell*>::iterator itCell;
	std::list<CUnit*> uncovered;

	for (itLayer = layers.begin(); itLayer != layers.end(); ++itLayer) {
		uncovered.clear();
		
		for (itCell = itLayer->second.begin(); itCell != itLayer->second.end(); ++itCell)
			(*itCell)->update(uncovered);
		
		if (!uncovered.empty()) {
			std::list<CUnit*>::iterator itUnit;
			std::map<int, CCoverageCell*>* coveredUnits = &unitsCoveredBy[itLayer->first];
			
			for (itUnit = uncovered.begin(); itUnit != uncovered.end(); ++itUnit) {
				CUnit* unit = *itUnit;
				coveredUnits->erase(unit->key);
				assert(unitsCoveredCount[unit->key] > 0);
				if(--unitsCoveredCount[unit->key] == 0) {
					unit->unreg(*this); // no need to track this unit anymore
				}
				addUnit(unit);
			}

			// re-assign uncovered units to existing cores...
			for (itUnit = uncovered.begin(); itUnit != uncovered.end(); ++itUnit) {
				addUnit(*itUnit);
			}
		}
	}

	if (visualizationEnabled) {
		visualizeLayer(visualizationLayer);
	}
}

void CCoverageHandler::addUnit(CUnit* unit) {
	LOG_II("CCoverageHandler::addUnit " << (*unit))

	CCoverageCell::NType coreType = getCoreType(unit->type);
	
	if (coreType != CCoverageCell::UNDEFINED) {
		if (coreUnits.find(unit->key) == coreUnits.end()) {
			// register new core...
			CCoverageCell* c = ReusableObjectFactory<CCoverageCell>::Instance();
			c->ai = ai;
			c->type = coreType;
			c->setCore(unit);
			c->reg(*this);
			layers[c->type].push_back(c);
			coreUnits[unit->key] = c;
		
			addUncoveredUnits(c);

			LOG_II((*c))
		}
	}

	// NOTE: only static units can be registered within coverage cells
	if ((unit->type->cats&STATIC).any()) {
		const float3 pos = unit->pos();
		std::map<CCoverageCell::NType, std::list<CCoverageCell*> >::iterator itLayer;
		std::list<CCoverageCell*>::iterator itCell;
		for (itLayer = layers.begin(); itLayer != layers.end(); ++itLayer) {
			CCoverageCell::NType layer = itLayer->first;

			if (layer == coreType)
				continue; // this unit represents a core within current layer

			if (unitsCoveredBy[layer].find(unit->key) != unitsCoveredBy[layer].end())
				continue; // unit already covered by current layer
			
			std::map<int, CUnit*>* validUnits = getScanList(layer);
			
			if (validUnits == NULL)
				continue;
			
			// OPTIMIZE: replace this check with category tags?
			if (validUnits->find(unit->key) == validUnits->end())
				continue; // this unit can't be registered in current layer

			// detect to which core this unit belongs...
			for (itCell = itLayer->second.begin(); itCell != itLayer->second.end(); ++itCell) {
				CCoverageCell *c = *itCell;
				if (c->isInRange(pos)) {
					if (c->addUnit(unit)) {
						LOG_II("CCoverageHandler::addUnit " << (*unit) << " covered by " << (*c))
						unitsCoveredBy[layer][unit->key] = c;
						unitsCoveredCount[unit->key]++;
						if (unitsCoveredCount[unit->key] == 1) {
							unit->reg(*this);
						}
						break;
					}
				}
			}
		}
	}
}

CCoverageCell::NType CCoverageHandler::getCoreType(UnitType* ut) const {
	const unitCategory cats = ut->cats;
	
	// NOTE: core unit should never belong to different types of layers
	// simultaneously

	if ((cats&NANOTOWER).any())
		return CCoverageCell::BUILD_ASSISTER;
	if ((cats&EBOOSTER).any())
		return CCoverageCell::ECONOMY_BOOSTER;
	
	// FIXME: though mobile defense can be passed below it is not supported
	if ((cats&DEFENSE).any()) {
		if ((cats&JAMMER).any())
			return CCoverageCell::DEFENSE_JAMMER;
		if ((cats&ANTINUKE).any())
			return CCoverageCell::DEFENSE_ANTINUKE;
		if ((cats&SHIELD).any())
			return CCoverageCell::DEFENSE_SHIELD;
		if ((cats&ANTIAIR).any())
			return CCoverageCell::DEFENSE_ANTIAIR;
		if ((cats&ATTACKER).any())
			return CCoverageCell::DEFENSE_GROUND;
	}
	
	return CCoverageCell::UNDEFINED;
}

std::map<int, CUnit*>* CCoverageHandler::getScanList(CCoverageCell::NType layer) const {
	switch (layer) {
		case CCoverageCell::DEFENSE_GROUND:
		case CCoverageCell::DEFENSE_ANTIAIR:
		case CCoverageCell::DEFENSE_ANTINUKE:
		case CCoverageCell::DEFENSE_SHIELD:
		case CCoverageCell::DEFENSE_JAMMER:
			return &ai->unittable->staticUnits;
		case CCoverageCell::BUILD_ASSISTER:
			return &ai->unittable->factories; // TODO: +defenses?
		case CCoverageCell::ECONOMY_BOOSTER:
	    	return &ai->unittable->staticEconomyUnits;
		default:
			return NULL;
	}
}

float3 CCoverageHandler::getNextBuildSite(UnitType* toBuild, const float3& pos) {
	float3 goal = ERRORVECTOR;
	
	CCoverageCell::NType layer = getCoreType(toBuild);
	if (layer == CCoverageCell::UNDEFINED)
		return goal;

	std::map<int, CUnit*>* scanList = getScanList(layer);
	if (scanList == NULL || scanList->empty())
		return goal;

	float minDistance = std::numeric_limits<float>::max();
	std::map<int, CCoverageCell*>* coveredUnits = &(unitsCoveredBy[layer]);
	
	for (std::map<int, CUnit*>::iterator it = scanList->begin(); it != scanList->end(); ++it) {
		CUnit* unit = it->second;
		if (getCoreType(unit->type) == layer)
			continue;
		if (coveredUnits->find(unit->key) == coveredUnits->end()) {
			float3 upos = unit->pos();
			// NOTE: i would use getETA but this is a great CPU hit
			float distance = upos.distance2D(pos);
			if (distance < minDistance) {
				minDistance = distance;
				goal = upos;
			}
		}
	}

	updateBestBuildSite(toBuild, goal);

	return goal;
}

float3 CCoverageHandler::getNextBuildSite(UnitType* toBuild) {
	float3 goal = ERRORVECTOR;

	CCoverageCell::NType layer = getCoreType(toBuild);
	if (layer == CCoverageCell::UNDEFINED)
		return goal;

	std::map<int, CUnit*>* scanList = getScanList(layer);
	if (scanList == NULL || scanList->empty())
		return goal;

	float maxCost = std::numeric_limits<float>::min();
	CUnit* bestUnit = NULL;
	std::map<int, CCoverageCell*>* coveredUnits = &unitsCoveredBy[layer];
	
	for (std::map<int, CUnit*>::iterator it = ai->unittable->staticUnits.begin(); it != ai->unittable->staticUnits.end(); ++it)	{
		CUnit* unit = it->second;
		if (getCoreType(unit->type) == layer)
			continue;
		if (coveredUnits->find(unit->key) == coveredUnits->end()) {
			if (maxCost < unit->type->cost) {
				maxCost = unit->type->cost;
				bestUnit = unit;
			}
		}
	}

	if (bestUnit) {
		goal = bestUnit->pos();
		updateBestBuildSite(toBuild, goal);
	}

	return goal;
}

void CCoverageHandler::updateBestBuildSite(UnitType* toBuild, float3& pos) {
	if (pos == ERRORVECTOR)
		return;

	CCoverageCell::NType layer = getCoreType(toBuild);
	float range = getCoreRange(layer, toBuild);
	float3 oldPos, basePos;
	
	basePos = pos;

	do {
		oldPos = pos; pos = ZeroVector;
		int numAppended = 0;
		int numUnits = ai->cb->GetFriendlyUnits(&ai->unitIDs[0], oldPos, range);
		
		for (int i = 0; i < numUnits; i++) {
			const int uid = ai->unitIDs[i];
			const UnitDef* ud = ai->cb->GetUnitDef(uid);
			
			if (ud == NULL)
				continue;
			
			UnitType* ut = UT(ud->id);
			bool append = ((ut->cats&STATIC).any() && getCoreType(ut) != layer);

			if (append) {
				CUnit* unit = ai->unittable->getUnit(uid);
				if (unit)
					append = (unitsCoveredBy[layer].find(uid) == unitsCoveredBy[layer].end());
				else
					append = true; // allied unit
			
				if (append) {
					pos += ai->cb->GetUnitPos(uid);
					numAppended++;
				}
			}
		}

		if (numAppended == 0) {
			pos = oldPos;
			break;
		}
		
		pos /= numAppended;

		if (basePos.distance2D(pos) > range) {
			// center has moved too far from base position => break
			pos = oldPos;
			break;
		}			
	} while (pos.distance2D(oldPos) > FOOTPRINT2REAL);

	pos.y = ai->cb->GetElevation(pos.x, pos.z);
}

int CCoverageHandler::getLayerSize(CCoverageCell::NType layer) {
	return layers[layer].size();
}

float3 CCoverageHandler::getClosestDefendedPos(float3& pos) const {
	float3 bestPos = ERRORVECTOR;
	float minDistance = std::numeric_limits<float>::max();
	std::list<CCoverageCell*>::const_iterator itCell;
	std::map<CCoverageCell::NType, std::list<CCoverageCell*> >::const_iterator itLayer;
	for (itLayer = layers.begin(); itLayer != layers.end(); ++itLayer) {
		for (itCell = itLayer->second.begin(); itCell != itLayer->second.end(); ++itCell) {
			float distance = pos.distance2D((*itCell)->getCenter());
			if (distance < minDistance) {
				minDistance = distance;
				bestPos = (*itCell)->getCenter();
			}
		}
	}
	return bestPos;
}

float3 CCoverageHandler::getBestDefendedPos(float safetyLevel) const {
	return ZeroVector;
}
		
/*
bool CCoverageHandler::isPosInBounds(float3& pos) const {
}

float CCoverageHandler::distance2D(float3& pos) const {
	
}
*/

void CCoverageHandler::remove(ARegistrar& obj) {
	switch(obj.regtype()) {
		case ARegistrar::UNIT: {
			LOG_II("CCoverageHandler::remove Unit(" << obj.key << ")")
			assert(unitsCoveredCount[obj.key] > 0);
			int left = unitsCoveredCount[obj.key];
			std::map<CCoverageCell::NType, std::map<int, CCoverageCell*> >::iterator it;
			// remove unit from all layers...
			for (it = unitsCoveredBy.begin(); it != unitsCoveredBy.end(); ++it) {
				left -= it->second.erase(obj.key);
			}
			assert(left == 0);
			unitsCoveredCount[obj.key] = 0;
			obj.unreg(*this);
			break;
		}
		case ARegistrar::COVERAGE_CELL: {
			CCoverageCell* c = dynamic_cast<CCoverageCell*>(&obj);
			CCoverageCell::NType layer = c->type;

			LOG_II("CCoverageHandler::remove " << (*c))

			std::list<CUnit*> uncoveredUnits;
			if (c->units.size() > 0) {
				// remember uncovered units...
				std::map<int, CUnit*>::iterator it;
				std::map<int, CCoverageCell*>* coveredUnits = &unitsCoveredBy[layer];
				for (it = c->units.begin(); it != c->units.end(); ++it) {
					uncoveredUnits.push_back(it->second);
					coveredUnits->erase(it->first);
					assert(unitsCoveredCount[it->first] > 0);
					if(--unitsCoveredCount[it->first] == 0)
						it->second->unreg(*this); // no need to track unit anymore
				}
			}
			
			c->unreg(*this);
			layers[layer].remove(c);
			assert(c->getCore() != NULL);
			coreUnits.erase(c->getCore()->key);
			ReusableObjectFactory<CCoverageCell>::Release(c);
			
			if (!(uncoveredUnits.empty() || layers[layer].empty())) {
				// re-assign uncovered units to existing cores of current layer...
				for (std::list<CUnit*>::iterator itUnit = uncoveredUnits.begin(); itUnit != uncoveredUnits.end(); ++itUnit) {
					addUnit(*itUnit);
				}
			}	 
			
			break;
		}
		default:
			assert(false);
	}
}

float CCoverageHandler::getCoreRange(CCoverageCell::NType type, UnitType* ut) {
	float result = 0.0f;

	if (ut == NULL)
		return result;

	switch (type) {
		case CCoverageCell::DEFENSE_GROUND:
		case CCoverageCell::DEFENSE_ANTIAIR:
		case CCoverageCell::DEFENSE_ANTINUKE:
			result = ut->def->maxWeaponRange;
			break;
		case CCoverageCell::DEFENSE_SHIELD:
			for (int i = 0; i < ut->def->weapons.size(); i++) {
				const WeaponDef* wdef = ut->def->weapons[i].def;
				if(wdef->isShield) {
					result = wdef->shieldRadius;
				}
			}
			break;
		case CCoverageCell::ECONOMY_BOOSTER:
			// TODO: get the real effective range
			result = 200.0f;
			break;
		case CCoverageCell::DEFENSE_JAMMER:
			result = ut->def->jammerRadius;
			break;
		case CCoverageCell::BUILD_ASSISTER:
			result = ut->def->buildDistance;
			break;
	}

	switch (type) {
		case CCoverageCell::DEFENSE_GROUND:
		case CCoverageCell::DEFENSE_ANTIAIR:
			switch (ai->difficulty) {
				case DIFFICULTY_EASY:
					result *= 2.0f;
					break;
				case DIFFICULTY_NORMAL:
					result *= (1.5f - 0.5f * (ai->cfgparser->getMaxTechLevel() / MAX_TECHLEVEL));
					break;
				case DIFFICULTY_HARD:
					result *= (0.8f - 0.3f * (ai->cfgparser->getMaxTechLevel() / MAX_TECHLEVEL));
					break;
			}
			break;
		case CCoverageCell::DEFENSE_JAMMER:
		case CCoverageCell::DEFENSE_ANTINUKE:
		case CCoverageCell::ECONOMY_BOOSTER:
			result *= 0.95f;	
		default:
			return result;
	}
	
	return result;
}

void CCoverageHandler::addUncoveredUnits(CCoverageCell* c) {
	float range = c->getRange();
	float3 pos = c->getCenter();
	std::map<int, CUnit*>* units = getScanList(c->type);
	std::map<int, CCoverageCell*>* coveredUnits = &unitsCoveredBy[c->type];
	
	if (units == NULL)
		return;

	// register uncovered units in a cell...
	for (std::map<int, CUnit*>::iterator it = units->begin(); it != units->end(); ++it) {
		if (coveredUnits->find(it->first) == coveredUnits->end()) {
			// NOTE: due to optimization purposes we do not use c->isInRange() here
			if (pos.distance2D(ai->cb->GetUnitPos(it->first)) <= range) {
				if (c->addUnit(it->second)) {
					(*coveredUnits)[it->first] = c;
					unitsCoveredCount[it->first]++;
					if (unitsCoveredCount[it->first] == 1) {
						it->second->reg(*this);
					}
				}
			}
		}
	}
}

bool CCoverageHandler::isUnitCovered(int uid, CCoverageCell::NType layer) {
	return (unitsCoveredBy[layer].find(uid) != unitsCoveredBy[layer].end());
}

bool CCoverageHandler::toggleVisualization() {
	visualizationEnabled = !visualizationEnabled;
	
	if (visualizationEnabled) {
		// NOTE: to enable visualization at least one unit should be selected
		if (ai->cb->GetSelectedUnits(&ai->unitIDs[0], 1) > 0) {
			CUnit* unit = ai->unittable->getUnit(ai->unitIDs[0]);
			if (unit) {
				visualizationLayer = getCoreType(unit->type);
				if (visualizationLayer != CCoverageCell::UNDEFINED)
					return true;
			}
		}
		visualizationEnabled = false;
	}	
	return visualizationEnabled;
}

void CCoverageHandler::visualizeLayer(CCoverageCell::NType layer) {
	static const int figureID = 13;
	int i = 0;
	std::list<CCoverageCell*>* l = &layers[layer];
	const float size = l->size();
	for (std::list<CCoverageCell*>::iterator it = l->begin(); it != l->end(); ++it, i++) {
		CCoverageCell* c = *it;
		float3 p0(c->getCenter());
		p0.y = ai->cb->GetElevation(p0.x, p0.z) + 10.0f;
		for (std::map<int, CUnit*>::iterator itUnit = c->units.begin(); itUnit != c->units.end(); ++itUnit) {
			float3 p2 = itUnit->second->pos();
			ai->cb->CreateLineFigure(p0, p2, 5.0f, 0, MULTIPLEXER, figureID);
		}
		ai->cb->SetFigureColor(figureID, 0.0f, 0.0f, i/size, 1.0f);
	}
}
