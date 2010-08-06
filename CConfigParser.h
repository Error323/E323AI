#ifndef CCONFIGPARSER_H
#define CCONFIGPARSER_H

#include <string>
#include <map>
#include <vector>

#include "headers/Defines.h"

enum GetFilenameFlags {
	GET_CFG  = (1<<0),
	GET_CAT  = (1<<1),
	GET_VER  = (1<<2),
	GET_TEAM = (1<<3)
};

class UnitType;
class AIClasses;

class CConfigParser {
	public:
		CConfigParser(AIClasses *ai);
		~CConfigParser() {};

		std::string getFilename(unsigned int f);
		bool fileExists(const std::string& filename);

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

		int state;
		std::map<std::string, int> stateVariables;

		void split(std::string &line, char c, std::vector<std::string> &splitted);
		bool contains(std::string &line, char c);
};

#endif

