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
		~GameMap(){}

		/** @return float3, ZeroVector when there are no candidates */
		float3 GetClosestOpenMetalSpot(CGroup*);

		/** @return int, unit id of the upgradeable mex, -1 if there are no candidates */
		int GetClosestUpgradableMetalSpot(CGroup*);

		/** @return float, height variance */
		float GetHeightVariance() { return heightVariance; }

		/** @return float, amount of water in [0, 1] */
		float GetAmountOfWater() { return waterAmount; }

		/** @return float, amount of land in [0, 1] */
		float GetAmountOfLand() { return (1.0f - waterAmount); }


		bool HasGeoSpots() { return geospots.size() > 0; }
		bool HasMetalFeatures() { return metalfeatures.size() > 0; }
		bool HasEnergyFeatures() { return energyfeatures.size() > 0; }
		//bool HasMetalSpots() { return true; }

		bool IsKbotMap() { return heightVariance > KBOT_VEH_THRESHOLD; }
		bool IsVehicleMap() { return !IsKbotMap(); }
		bool IsHooverMap() { return waterAmount > 0.2f; }
		//bool IsMetalMap() { return true; }

		std::list<float3>& GetGeoSpots() { return geospots; }
		std::list<float3>& GetMetalFeatures() { return metalfeatures; }
		std::list<float3>& GetEnergyFeatures() { return energyfeatures; }
	
	private:
		float heightVariance;
		float waterAmount;

		static std::list<float3> geospots;
		static std::list<float3> metalfeatures;
		static std::list<float3> energyfeatures;
		static std::list<float3> metalspots;

		AIClasses* ai;

		void CalcMetalSpots();
		void CalcMapHeightFeatures();
		void CalcGeoSpots();
};

#endif
