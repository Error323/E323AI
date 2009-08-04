#ifndef INTEL_H
#define INTEL_H

#include <vector>

#include "CE323AI.h"

class CIntel {
	public:
		CIntel(AIClasses *ai);
		~CIntel();

		bool hasAir;

		void update(int frame);
		bool enemyInbound();

		std::vector<int> factories;
		std::vector<int> attackers;
		std::vector<int> mobileBuilders;
		std::vector<int> metalMakers;
		std::vector<int> energyMakers;

	private:
		AIClasses *ai;

		int *units;
};

#endif
