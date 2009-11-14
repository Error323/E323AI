#ifndef CCONFIGPARSER_H
#define CCONFIGPARSER_H

#include <string>
#include <map>
#include <vector>

#include "headers/Defines.h"

class UnitType;
class AIClasses;

class CConfigParser {
	public:
		CConfigParser(AIClasses *ai);
		~CConfigParser(){};

		int determineState(int metalIncome, int energyIncome);
		int getMinWorkers();
		int getMaxWorkers();
		int getMinScouts();
		int getMaxTechLevel();
		int getTotalStates();
		int getMinGroupSize(int techLevel);
		int getState();

		bool parseConfig(std::string filename);
		bool parseCategories(std::string filename, std::map<int, UnitType> &units);
		void debugConfig();
		std::string getAbsoluteFileName(std::string filename, bool readonly = true);

	private:
		std::map<int, std::map<std::string, int> > states;

		AIClasses *ai;

		std::map<std::string, bool> stateVariables;
		int state;

		void split(std::string &line, char c, std::vector<std::string> &splitted);
		bool contains(std::string &line, char c);
		void removeWhiteSpace(std::string &line);
		
};

#endif

