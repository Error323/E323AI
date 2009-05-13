#ifndef INTEL_H
#define INTEL_H

#include "E323AI.h"

class CIntel {
	public:
		CIntel(AIClasses *ai);
		~CIntel();

		bool hasAir;

		void update(int frame);

		std::vector<int> economy;
		std::vector<int> factories;
		std::vector<int> attackers;

	private:
		AIClasses *ai;

		int *units;
};

#endif
