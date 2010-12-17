#ifndef CCONFIGPARSER_H
#define CCONFIGPARSER_H

#include <string>
#include <map>
#include <vector>

#include "headers/Defines.h"
#include "CUnitTable.h"

enum GetFilenameFlags {
	GET_CFG   = (1<<0),
	GET_CAT   = (1<<1),
	GET_VER   = (1<<2),
	GET_TEAM  = (1<<3),
	GET_PATCH = (1<<4)
};

class UnitType;
class AIClasses;

class CConfigParser {
	
public:
	CConfigParser(AIClasses* ai);
	~CConfigParser() {};

	std::string getFilename(unsigned int f);
	bool fileExists(const std::string& filename);

	int determineState(int metalIncome, int energyIncome);
	int getMinWorkers();
	int getMaxWorkers();
	int getMinScouts();
	int getMaxTechLevel();
	int getTotalStates();
	int getMinGroupSize(unitCategory techLevel);
	int getState();

	/**
	 * Tries to load the config file from filename.
	 * @return true if file was loaded, false otherwise
	 */
	bool parseConfig(std::string filename);
	/**
	 * Tries to load the categorization file from filename.
	 * @return true if file was loaded, false otherwise
	 */
	bool parseCategories(std::string filename, std::map<int, UnitType>& units, bool patchMode = false);
	/**
	 * Indicates whether a config file was loaded
	 * and whether it is valid to be used.
	 * @return true if (loaded && (!template || DEBUG)), false otherwise
	 */
	bool isUsable() const;
	void debugConfig();

protected:
	AIClasses* ai;

private:
	bool loaded;
	bool templt; // cause template is a reserved word
	int state;
	std::map<int, std::map<std::string, int> > states;
	std::map<std::string, int> stateVariables;
};

#endif

