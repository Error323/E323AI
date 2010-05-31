#include "CLogger.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <ctime>

#include "CAI.h"
#include "Util.hpp"

CLogger::CLogger(AIClasses *_ai, unsigned lt): ai(_ai), logType(lt) {
	logLevels[ERROR]   = "(EE)";
	logLevels[WARNING] = "(WW)";
	logLevels[VERBOSE] = "(II)";

	logDesc[ERROR]   = logLevels[ERROR]   + " error, ";
	logDesc[WARNING] = logLevels[WARNING] + " warning, ";
	logDesc[VERBOSE] = logLevels[VERBOSE] + " informational";

	if (lt & CLogger::LOG_FILE) {
		std::string mapname = std::string(ai->cb->GetMapName());

		time_t now1;
		time(&now1);
		struct tm* now2 = localtime(&now1);

		char relFileName[2048];
		std::sprintf(
			relFileName, "%s%s-%2.2d%2.2d%2.2d%2.2d%2.2d-team-%d.log", 
			LOG_FOLDER,
			mapname.c_str(), 
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
			for (i = logDesc.begin(); i != logDesc.end(); i++)
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
		std::cout << "Logging to screen:\n";
	}
	
	if (lt & CLogger::LOG_SPRING) {
		ai->cb->SendTextMsg("Logging to spring", 0);
	}
}

void CLogger::s(std::string msg) {
	ai->cb->SendTextMsg(msg.c_str(), 0);
}

void CLogger::log(logLevel level, std::string &msg) {
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
