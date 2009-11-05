#ifndef CCONFIGPARSER_H
#define CCONFIGPARSER_H

#include <string>

#include "headers/Defines.h"

class CConfigParser {
	public:
		CConfigParser(AIClasses *ai);
		CConfigParser(){};

		int determineState(int metalIncome, int energyIncome);
		int getMinWorkers();
		int getMaxWorkers();
		int getMinScouts();
		int getMaxTechLevel();
		int getMinGroupSize(int techLevel);

		void parseConfig(std::string filename);
		void parseCategories(std::string filename, std::map<int, UnitType*> &units);

	private:
		struct State {
			int state;
			int metalIncome;
			int energyIncome;
			int minWorkers;
			int maxWorkers;
			int minScouts;
			int maxTechLevel;
			int minGroupSizeTech1;
			int minGroupSizeTech2;
			int minGroupSizeTech3;
		};

		AIClasses *ai;

		std::map<int, State*> states;
		std::map<std::string, int> stateVariables;

		std::string getAbsoluteFileName(std::string filename);
		void split(std::string &line, char c, std::vector<std::string> &splitted);
		bool contains(std::string &line, char c);
		void removeWhiteSpace(std::string &line);
		
};

#endif

