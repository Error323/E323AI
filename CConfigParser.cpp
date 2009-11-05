#include "CConfigParser.h"

#include <iostream>
#include <fstream>

#include "CAI.h"

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
}

/*
int CConfigParser::determineState(int metalIncome, int energyIncome);
int CConfigParser::getMinWorkers();
int CConfigParser::getMaxWorkers();
int CConfigParser::getMinScouts();
int CConfigParser::getMaxTechLevel();
int CConfigParser::getMinGroupSize(int techLevel);
*/

void CConfigParser::parseConfig(std::string filename) {
	filename = getAbsoluteFileName(filename);
	std::ifstream file(fileName.c_str());
	unsigned linenr = 0;

	if (file.good() && file.is_open()) {
		while(!file.eof()) {
			linenr++;
			std::string line;
			std::vector<std::string> splitted;

			std::getline(file, line);
			line = line.substr(0, line.find('#')-1);
			removeWhiteSpace(line);

			if (line.empty() || !contains(line, '{') || line.size() < 2)
				continue;

			/* Entered new state block */
			State *state = new State();
			line.substr(0, line.size()-2);
			split(line, ':', splitted);
			state->state = atoi(line[1]);
			
			std::getline(file, line);
			linenr++;
			int statevars = 0;
			while(!contains(line, '}') && !file.eof()) {
				removeWhiteSpace(line);
				split(line, ':', splitted);
				if (stateVariables.find(splitted[0]) == stateVariables.end()) {
					/* Something went wrong */
					continue;
				}
				
				stateVariables[splitted[0]] = atoi(splitted[1]);
				std::getline(file, line);
				linenr++;
				statevars++;
			}
			if (statevars == stateVariables.size()) {
				parseState(state);
				states[state->state] = state;
			}
			else {
				/* Something went wrong */
			}
		}
	}
}
//void CConfigParser::parseCategories(std::string filename, std::map<int, UnitType*> &units);

void CConfigParser::parseState(State state) {
	std::map<std::string, int>::iterator i;
	for (i = stateVariables.begin(); i != stateVariables.end(); i++) {
		if (i->first == "metalIncome") state->metalIncome = i->second;
		if (i->first == "energyIncome") state->energyIncome = i->second;
		if (i->first == "minWorkers") state->minWorkers = i->second;
		if (i->first == "maxWorkers") state->maxWorkers = i->second;
		if (i->first == "minScouts") state->minScouts = i->second;
		if (i->first == "maxTechLevel") state->maxTechLevel = i->second;
		if (i->first == "minGroupSizeTech1") state->minGroupSizeTech1 = i->second;
		if (i->first == "minGroupSizeTech2") state->minGroupSizeTech2 = i->second;
		if (i->first == "minGroupSizeTech3") state->minGroupSizeTech3 = i->second;
	}
}

void CConfigParser::split(std::string &line, char c, std::vector<std::string> &splitted) {
	size_t begin = 0, end = 0;
	std::string substr;

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
	filename(buf);
	return filename;
}

bool CConfigParser::contains(std::string &line, char c) {
	if (line.empty()) return false;
	if (line[0] == c) return true;
	if (line.find(c) > 0) return true;
	return false;
}

void CConfigParser::removeWhiteSpace(std::string &line) {
	/* First remove frontal whitespaces */
	while(true) {
		if (line[0] != ' ' && line[0] != '\t')
			break;
		line = line.substr(1,line.size()-1);
	}
	
	/* Now remove other whitespaces */
	while(true) {
		bool b1 = false, b2 = false;
		int pos;

		pos = line.find(' ');
		b1 = pos == 0;
		if (b1) line.replace(pos, pos+1, "");

		pos = line.find('\t');
		b2 = pos == 0;
		if (b2) line.replace(pos, pos+1, "");

		if (b1 && b2)
			break;
	}
}
