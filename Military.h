#ifndef MILITARY_H
#define MILITARY_H

#include "E323AI.h"

class CMilitary {
	public:
		CMilitary(AIClasses *ai);
		~CMilitary(){};

		void init(int unit);

		void update(int frame);

		/* military units */
		std::map<int, int> scouts;
		std::map<int, int> artillery;
		std::map<int, int> antiairs;
		std::map<int, int> groups;

	private:
		AIClasses *ai;

		UnitType *scout, *artie, *antiair, *factory;

};

#endif
