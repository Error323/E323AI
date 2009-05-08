#ifndef MILITARY_H
#define MILITARY_H

#include "E323AI.h"

class CMilitary {
	public:
		CMilitary(AIClasses *ai);
		~CMilitary(){};

		void update(int frame);

	private:
		AIClasses *ai;
};

#endif
