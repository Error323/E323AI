#ifndef GAME_MAP
#define GAME_MAP

#include <list>

#include "System/float3.h"

// heightVariance(Altair_Crossing.smf)
#define KBOT_VEH_THRESHOLD 43.97f

class CGroup;
class AIClasses;

class GameMap {

public:
	GameMap(AIClasses*);

	static std::list<float3> metalspots;
	static std::list<float3> geospots;
	static std::list<float3> metalfeatures; // not used yet
	static std::list<float3> energyfeatures; // not used yet

	/** @return float, height variance */
	float GetHeightVariance() { return heightVariance; }
	/** @return float, amount of water in [0, 1] */
	float GetAmountOfWater() { return waterAmount; }
	/** @return float, amount of land in [0, 1] */
	float GetAmountOfLand() { return (1.0f - waterAmount); }

	bool HasGeoSpots() { return geospots.size() > 0; }
	bool HasMetalFeatures() { return metalfeatures.size() > 0; }
	bool HasEnergyFeatures() { return energyfeatures.size() > 0; }

	bool IsKbotMap() { return heightVariance > KBOT_VEH_THRESHOLD; }
	bool IsVehicleMap() { return !IsKbotMap(); }
	bool IsHooverMap() { return waterAmount > 0.2f; }
	bool IsWaterMap() { return waterAmount > 0.7f; }
	bool IsWaterFreeMap() { return waterAmount < 0.15f; }
	bool IsLandFreeMap() { return waterAmount > 0.9f; }
	bool IsMetalMap() { return metalCount > nonMetalCount && avgMetal > 80; }
	bool IsMetalFreeMap() { return metalCount == 0; }

protected:
    AIClasses* ai;

private:
	float heightVariance;
	float waterAmount;
	int metalCount;
	int nonMetalCount;
	int minMetal;
	int maxMetal;
	int avgMetal;
	bool debug;

	void CalcMetalSpots();
	void CalcGeoSpots();
	void CalcMapHeightFeatures();
};

#endif
