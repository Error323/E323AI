#ifndef E323_CCOVERAGEHANDLER_H
#define E323_CCOVERAGEHANDLER_H

#include <string>
#include <map>

#include "CCoverageCell.h"

class ARegistrar;
class CUnit;

class CCoverageHandler: ARegistrar {

public:
	CCoverageHandler(AIClasses* _ai): ai(_ai) { visualizationEnabled = false; }
	~CCoverageHandler() {}
	
	/* Register unit */
	void addUnit(CUnit* unit);
	/* Get closest build site */
	float3 getNextClosestBuildSite(const CUnit* builder, UnitType* toBuild);
	/* Get the most valuable build site */
	float3 getNextImportantBuildSite(UnitType* toBuild);
	
	float3 getClosestDefendedPos(float3& pos) const;
	/* Not implemented */
	float3 getBestDefendedPos(float safetyLevel = 0.0f) const;
	/* Update call */
	void update();
	/* Overloaded */
	void remove(ARegistrar& obj);

	float getCoreRange(CCoverageCell::NType type, UnitType* ut);
	/* Get number of cores in a layer */
	int getLayerSize(CCoverageCell::NType layer);
	/* Get coverage type which can be based upon units of this type */
	CCoverageCell::NType getCoreType(const UnitType* ut) const;

	bool isUnitCovered(int uid, CCoverageCell::NType layer);

	ARegistrar::NType regtype() const { return ARegistrar::COVERAGE_HANDLER; }
		
	bool toggleVisualization();

protected:
	AIClasses* ai;

private:
	bool visualizationEnabled;
	CCoverageCell::NType visualizationLayer;
	std::map<CCoverageCell::NType, std::list<CCoverageCell*> > layers;
	std::map<CCoverageCell::NType, std::map<int, CCoverageCell*> > unitsCoveredBy;
	std::map<int, CCoverageCell*> coreUnits;
	std::map<int, int> unitsCoveredCount;

	/* Update position using k-means clusterization method */
	void updateBestBuildSite(UnitType* toBuild, float3& pos);
	/* Get list of units worth to cover by current layer */
	std::map<int, CUnit*>* getScanList(CCoverageCell::NType layer) const;

	void addUncoveredUnits(CCoverageCell* c);

	void visualizeLayer(CCoverageCell::NType layer);
};

#endif
