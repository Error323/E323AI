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

		/**
		 * Tries to load the config file from filename.
		 * @return true if file was loaded, false otherwise
		 */
		bool parseConfig(std::string filename);
		/**
		 * Indicates whether a config file was loaded
		 * and whether it is valid to be used.
		 * @return true if (loaded && (!template || DEBUG)), false otherwise
		 */
		bool isUsable() const;
		bool parseCategories(std::string filename, std::map<int, UnitType> &units);
		void debugConfig();

	private:
		std::map<int, std::map<std::string, int> > states;
		bool loaded;
		bool templt; // cause template is a reserved word

		AIClasses *ai;

		std::map<std::string, bool> stateVariables;
		int state;

		void split(std::string &line, char c, std::vector<std::string> &splitted);
		bool contains(std::string &line, char c);
};

#endif

