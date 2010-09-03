#include "CLogger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>

#include "CAI.h"
#include "Util.hpp"

std::map<CLogger::logLevel, std::string> CLogger::logLevels;
std::map<CLogger::logLevel, std::string> CLogger::logDesc;

CLogger::CLogger(AIClasses *_ai, unsigned int lt, logLevel lf): ai(_ai), logType(lt), logFilter(lf) {
	if (logLevels.empty()) {
		logLevels[ERROR]   = "(EE)";
		logLevels[WARNING] = "(WW)";
		logLevels[VERBOSE] = "(II)";
	}

	if (logDesc.empty()) {
		logDesc[ERROR]   = logLevels[ERROR]   + " error, ";
		logDesc[WARNING] = logLevels[WARNING] + " warning, ";
		logDesc[VERBOSE] = logLevels[VERBOSE] + " informational";
	}

	if (lf == NONE) {
		ai->cb->SendTextMsg("Logging disabled", 0);
		return;
	}

	if (lt & CLogger::LOG_FILE) {
		std::string modShortName = std::string(ai->cb->GetModShortName());
		std::string mapName = std::string(ai->cb->GetMapName());

		util::SanitizeFileNameInPlace(modShortName);
		util::SanitizeFileNameInPlace(mapName);

		time_t now1;
		time(&now1);
		struct tm* now2 = localtime(&now1);

		char relFileName[2048];
		std::sprintf(
			relFileName, "%s%s-%s-%2.2d%2.2d%2.2d%2.2d%2.2d-team-%d.log", 
			LOG_FOLDER,
			modShortName.c_str(),
			mapName.c_str(), 
			now2->tm_year + 1900, 
			now2->tm_mon + 1, 
			now2->tm_mday, 
			now2->tm_hour, 
			now2->tm_min, 
			ai->team
		);
		
		fileName = util::GetAbsFileName(ai->cb, std::string(relFileName), false);
		ofs.open(fileName.c_str(), std::ios::app);
		if (ofs.good()) {
			std::cout << "Logging to file: " << fileName << "\n";
			ofs << "Version: " << AI_VERSION << "\n";
			ofs << "Developers: " << AI_CREDITS << "\n";
			ofs << "Markers: ";
			
			std::map<logLevel, std::string>::iterator i;
			for (i = logDesc.begin(); i != logDesc.end(); ++i)
				ofs << i->second;

			ofs << "\n\n";
			ofs.flush();
			ofs.close();
		}
		else {
			std::cout << "Logging to file: " << fileName << " failed!\n";
			logType -= CLogger::LOG_FILE;
		}
	}
	
	if (lt & CLogger::LOG_STDOUT) {
		std::cout << "Logging to screen...\n";
	}
	
	if (lt & CLogger::LOG_SPRING) {
		ai->cb->SendTextMsg("Logging to Spring...", 0);
	}
}

void CLogger::s(std::string msg) {
	ai->cb->SendTextMsg(msg.c_str(), 0);
}

void CLogger::log(logLevel level, std::string &msg) {
	if (level == NONE || level > logFilter)
		return;

	int frame = ai->cb->GetCurrentFrame();
	int sec   = (frame / 30) % 60;
	int min   = ((frame / 30) - sec) / 60;
	char time[10];
	sprintf(time, "[%2.2d:%2.2d] ", min, sec);
	std::string output(time);
	output += logLevels[level] + ": " + msg + "\n";

	if (logType & CLogger::LOG_FILE) {
		ofs.open(fileName.c_str(), std::ios::app);
		if (ofs.good()) {
			ofs << output;
			ofs.flush();
			ofs.close();
		}
	}

	if (logType & CLogger::LOG_STDOUT) {
		std::cout << output;
	}
	
	if ((logType & CLogger::LOG_SPRING) && level == ERROR) {
		ai->cb->SendTextMsg(msg.c_str(), 0);
	}
}
