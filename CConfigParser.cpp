#include "CConfigParser.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>

#include "CAI.h"
#include "CUnit.h"
#include "CUnitTable.h"

CConfigParser::CConfigParser(AIClasses *ai) {
	this->ai = ai;
	stateVariables["metalIncome"]       = 0;
	stateVariables["energyIncome"]      = 0;
	stateVariables["minWorkers"]        = 0;
	stateVariables["maxWorkers"]        = 0;
	stateVariables["minScouts"]         = 0;
	stateVariables["maxTechLevel"]      = 0;
	stateVariables["minGroupSizeTech1"] = 0;
	stateVariables["minGroupSizeTech2"] = 0;
	stateVariables["minGroupSizeTech3"] = 0;
	state = -1;
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
		LOG_II("CConfigParser::determineState activated state(" << state << ")")
	return state;
}

int CConfigParser::getState() { return state; }
int CConfigParser::getTotalStates()  { return states.size(); }
int CConfigParser::getMinWorkers()   { return states[state]["minWorkers"]; }
int CConfigParser::getMaxWorkers()   { return states[state]["maxWorkers"]; }
int CConfigParser::getMinScouts()    { return states[state]["minScouts"]; }

int CConfigParser::getMaxTechLevel() { 
	switch(states[state]["maxTechLevel"]) {
		case 1: return TECH1;
		case 2: return TECH2;
		case 3: return TECH3;
		default: return TECH1;
	}
}

int CConfigParser::getMinGroupSize(int techLevel) {
	switch (techLevel) {
		case TECH1: return states[state]["minGroupSizeTech1"];
		case TECH2: return states[state]["minGroupSizeTech2"];
		case TECH3: return states[state]["minGroupSizeTech3"];
		default: return 0;
	}
}

void CConfigParser::parseConfig(std::string filename) {
	filename = getAbsoluteFileName(filename);
	std::ifstream file(filename.c_str());
	unsigned linenr = 0;

	if (file.good() && file.is_open()) {
		std::vector<std::string> splitted;
		while(!file.eof()) {
			linenr++;
			std::string line;

			std::getline(file, line);
			line = line.substr(0, line.find('#')-1);
			removeWhiteSpace(line);

			if (line[0] == '#' || line.empty())
				continue;

			/* New state block */
			if (contains(line, '{')) {
				line.substr(0, line.size()-1);
				split(line, ':', splitted);
				state = atoi(splitted[1].c_str());
				std::map<std::string, int> curstate;
				states[state] = curstate;
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
		LOG_II("CConfigParser::parseConfig parsed "<<linenr<<" lines from " << filename)
		file.close();
	}
	else {
		LOG_EE("Could not open " << filename << " for parsing")
		ai->cb->SendTextMsg(std::string("Could not parse"+filename).c_str(), 0);
		throw(2);
	}
}

void CConfigParser::parseCategories(std::string filename, std::map<int, UnitType> &units) {
	filename = getAbsoluteFileName(filename);
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

			line = line.substr(0, line.find('#')-1);
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
		LOG_EE("Could not open " << filename << " for parsing")
		ai->cb->SendTextMsg(std::string("Could not parse"+filename).c_str(), 0);
		throw(2);
	}
	LOG_II("Parsed " << linenr << " lines from " << filename)
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

std::string CConfigParser::getAbsoluteFileName(std::string filename) {
	char buf[256];
	sprintf(
		buf, "%s%s", 
		CFG_FOLDER,
		filename.c_str()
	);
	ai->cb->GetValue(AIVAL_LOCATE_FILE_R, buf);
	filename = std::string(buf);
	return filename;
}

bool CConfigParser::contains(std::string &line, char c) {
	for ( int i = 0; i < line.length(); i++ ) {
		if (line[i] == c)
			return true;
	}
	return false;
}

void CConfigParser::removeWhiteSpace(std::string &line) {
	for ( int i = 0, j ; i < line.length( ) ; ++ i ) {
		if ( line [i] == ' ' || line[i] == '\t') {
			for ( j = i + 1; j < line.length ( ) ; ++j )
			{
				if ( line [j] != ' ' || line[i] == '\t')
				break ;
			}
			line = line.erase ( i, (j - i) );
		}
	}
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
