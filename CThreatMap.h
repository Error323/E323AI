#ifndef THREATMAP_H
#define THREATMAP_H

#include "headers/Defines.h"
#include "headers/HEngine.h"

class AIClasses;
class CGroup;

// TODO: convert into flags?
enum ThreatMapType {
	TMT_NONE = 0,
	TMT_AIR,
	TMT_SURFACE,
	TMT_UNDERWATER,
	TMT_LAST
};

class CThreatMap {

public:
	CThreatMap(AIClasses* ai);
	~CThreatMap();

	void update(int frame);
	float getThreat(float3 center, float radius, ThreatMapType type = TMT_SURFACE);
	float getThreat(const float3& center, float radius, CGroup* group);
	const float* getMap(ThreatMapType);
	bool switchDebugMode();
	bool isInBounds(int x, int z) { return (x >= 0 && z >= 0 && x < X && z < Z); }
	void checkInBounds(int& x, int& z);

protected:
    AIClasses* ai;

private:
	int X, Z;
		// width & height of threat map (in pathgraph resolution)
	int lastUpdateFrame;
		// not very helpful for now; will be really used when threatmap
		// is shared between allied AIs
	ThreatMapType drawMap;
		// current threat map to visualize
	std::map<ThreatMapType, float> maxPower;
		// max threat per map
	std::map<ThreatMapType, float*> maps;
		// threatmap data
#if !defined(BUILDING_AI_FOR_SPRING_0_81_2)
	std::map<ThreatMapType, int> handles;
		// for visual debugging purposes
#endif
	float gauss(float x, float sigma, float mu);

	void reset();

	void visualizeMap(ThreatMapType type = TMT_SURFACE);
};

#endif
