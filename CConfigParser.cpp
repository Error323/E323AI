#include "CConfigParser.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>

#include "CAI.h"
#include "CUnit.h"
#include "CUnitTable.h"
#include "Util.hpp"

CConfigParser::CConfigParser(AIClasses *ai) {
	this->ai = ai;
	loaded = false;
	templt = false;

	stateVariables["metalIncome"]       = 0;
	stateVariables["energyIncome"]      = 0;
	stateVariables["minWorkers"]        = 0;
	stateVariables["maxWorkers"]        = 99;
	stateVariables["minScouts"]         = 0;
	// NOTE: in config file techlevel is ordinary value, not a flag
	stateVariables["maxTechLevel"]      = MIN_TECHLEVEL;
	stateVariables["minGroupSizeTech1"] = 1;
	stateVariables["minGroupSizeTech2"] = 1;
	stateVariables["minGroupSizeTech3"] = 1;
	stateVariables["minGroupSizeTech4"] = 1;
	stateVariables["minGroupSizeTech5"] = 1;
	
	state = -1;
}

bool CConfigParser::fileExists(const std::string& filename) {
	return
		ai->cb->GetFileSize(util::GetAbsFileName(ai->cb, std::string(CFG_FOLDER) + filename, true).c_str()) > 0;
}

std::string CConfigParser::getFilename(unsigned int f) {
	static const char 
		ext[] =".cfg",
		cfg[] = "-config",
		cat[] = "-categorization";

	std::string result(ai->cb->GetModShortName());
	
	if (f&GET_VER) {
		result += "-";
		result += ai->cb->GetModVersion();
	}

	if (f&GET_CFG)
		result += cfg;
	else if (f&GET_CAT)
		result += cat;
	
	if (f&GET_TEAM) {
		char team[16];
		sprintf(team, "-%d", ai->team);
		result += team;
	}

	result += ext;

	util::SanitizeFileNameInPlace(result);
	
	return result;
}


int CConfigParser::determineState(int metalIncome, int energyIncome) {
	int previous = state;
	std::map<int, std::map<std::string, int> >::iterator i;
	for (i = states.begin(); i != states.end(); i++) {
		if (
			metalIncome >= i->second["metalIncome"] &&
			energyIncome >= i->second["energyIncome"]
		) state = i->first;
	}
	if (state != previous)
		LOG_II("CConfigParser::determineState(mIncome=" << metalIncome << ", eIncome=" << energyIncome << ") activated state(" << state << ")")
	return state;
}

int CConfigParser::getState() { return state; }
int CConfigParser::getTotalStates()  { return states.size(); }
int CConfigParser::getMinWorkers()   { return states[state]["minWorkers"]; }
int CConfigParser::getMaxWorkers()   { return states[state]["maxWorkers"]; }
int CConfigParser::getMinScouts()    { return states[state]["minScouts"]; }

int CConfigParser::getMaxTechLevel() {
	int result = std::min<int>(states[state]["maxTechLevel"], MAX_TECHLEVEL);
	if (result < MIN_TECHLEVEL) result = MIN_TECHLEVEL;
	return result;
}

int CConfigParser::getMinGroupSize(int techLevel) {
	switch (techLevel) {
		case TECH1: return states[state]["minGroupSizeTech1"];
		case TECH2: return states[state]["minGroupSizeTech2"];
		case TECH3: return states[state]["minGroupSizeTech3"];
		case TECH4: return states[state]["minGroupSizeTech4"];
		case TECH5: return states[state]["minGroupSizeTech5"];
		default: return 0;
	}
}

bool CConfigParser::parseConfig(std::string filename) {
	std::string dirfilename = util::GetAbsFileName(ai->cb, std::string(CFG_FOLDER)+filename, true);
	std::ifstream file(dirfilename.c_str());

	if (file.good() && file.is_open()) {
		unsigned int linenr = 0;
		std::vector<std::string> splitted;
		
		templt = false;
		
		while(!file.eof()) {
			linenr++;
			std::string line;

			std::getline(file, line);
			line = line.substr(0, line.find('#') - 1);
			util::RemoveWhiteSpaceInPlace(line);

			if (line.empty() || line[0] == '#')
				continue;

			if (line == "TEMPLATE") {
				templt = true;
				continue;
			}

			/* New state block */
			if (contains(line, '{')) {
				line.substr(0, line.size() - 1);
				split(line, ':', splitted);
				state = atoi(splitted[1].c_str());
				states[state] = stateVariables;
			}
			/* Close state block */
			else if (contains(line, '}')) {
				if (states[state].size() == stateVariables.size())
					LOG_II("CConfigParser::parseConfig State("<<state<<") parsed successfully")
				else
					LOG_EE("CConfigParser::parseConfig State("<<state<<") parsed unsuccessfully")
			}
			/* Add var to curState */
			else {
				split(line, ':', splitted);
				states[state][splitted[0]] = atoi(splitted[1].c_str());
			}
		}
		LOG_II("CConfigParser::parseConfig parsed " << linenr << " lines from " << dirfilename)
		file.close();
		loaded = true;
	}
	else {
		LOG_WW("Could not open " << dirfilename << " for parsing")

		std::string templatefile = util::GetAbsFileName(ai->cb, std::string(CFG_FOLDER)+std::string(CONFIG_TEMPLATE), true);
		std::ifstream ifs(templatefile.c_str(), std::ios::binary);

		std::string newfile = util::GetAbsFileName(ai->cb, std::string(CFG_FOLDER)+filename, false);
		std::ofstream ofs(newfile.c_str(), std::ios::binary);

		ofs << ifs.rdbuf();
		LOG_II("New configfile created from " << templatefile)
		templt = true;
		loaded = false;
	}
	return loaded;
}

bool CConfigParser::isUsable() const {
#ifdef DEBUG
	return loaded;
#else
	return loaded && !templt;
#endif
}

bool CConfigParser::parseCategories(std::string filename, std::map<int, UnitType> &units) {
	filename = util::GetAbsFileName(ai->cb, std::string(CFG_FOLDER)+filename, true);
	std::ifstream file(filename.c_str());
	unsigned linenr = 0;

	if (file.good() && file.is_open()) {
		while(!file.eof()) {
			linenr++;
			std::string line;
			std::vector<std::string> splitted;

			std::getline(file, line);

			if (line.empty() || line[0] == '#')
				continue;

			line = line.substr(0, line.find('#') - 1);
			split(line, ',', splitted);
			const UnitDef *ud = ai->cb->GetUnitDef(splitted[0].c_str());
			if (ud == NULL) {
				LOG_EE("Parsing config line: " << linenr << "\tunit `" << splitted[0] << "' is invalid")
				continue;
			}
			UnitType *ut = &units[ud->id];

			unsigned categories = 0;
			for (unsigned i = 1; i < splitted.size(); i++) {
				if (CUnitTable::str2cat.find(splitted[i]) == CUnitTable::str2cat.end()) {
					LOG_EE("Parsing config line: " << linenr << "\tcategory `" << splitted[i] << "' is invalid")
					continue;
				}
				categories |= CUnitTable::str2cat[splitted[i]];
			}

			if (categories == 0) {
				LOG_EE("Parsing config line: " << linenr << "\t" << ut->def->humanName << " is uncategorized, falling back to standard")
				continue;
			}

			ut->cats = categories;
		}
		file.close();
	}
	else {
		LOG_WW("Could not open " << filename << " for parsing")
		return false;
	}
	
	LOG_II("CConfigParser::parseCategories parsed "<<linenr<<" lines from " << filename)
	
	return true;
}

void CConfigParser::split(std::string &line, char c, std::vector<std::string> &splitted) {
	size_t begin = 0, end = 0;
	std::string substr;
	splitted.clear();
	while (true) {
		end = line.find(c, begin);
		if (end == std::string::npos)
			break;

		substr = line.substr(begin, end-begin);
		splitted.push_back(substr);
		begin  = end + 1;
	}
	/* Manually push the last */
	substr = line.substr(begin, line.size()-1);
	splitted.push_back(substr);
}

bool CConfigParser::contains(std::string &line, char c) {
	for ( int i = 0; i < line.length(); i++ ) {
		if (line[i] == c)
			return true;
	}
	return false;
}

void CConfigParser::debugConfig() {
	std::stringstream ss;
	std::map<int, std::map<std::string, int> >::iterator i;
	std::map<std::string, int>::iterator j;
	ss << "found " << states.size() << " states\n";
	for (i = states.begin(); i != states.end(); i++) {
		ss << "\tState("<<i->first<<"):\n";
		for (j = i->second.begin(); j != i->second.end(); j++)
			ss << "\t\t" << j->first << " = " << j->second << "\n";
	}
	LOG_II("CConfigParser::debugConfig " << ss.str())
}
