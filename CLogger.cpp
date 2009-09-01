#include "CLogger.h"

#include "CE323AI.h"

CLogger::CLogger(AIClasses *_ai, unsigned lt): ai(_ai), logType(lt) {
	logLevels[ERROR]   = "(EE)";
	logLevels[WARNING] = "(WW)";
	logLevels[VERBOSE] = "(II)";

	if (lt & CLogger::LOG_FILE) {
		char fileName[255];
		getFileName(fileName);
		fs.open(fileName, std::fstream::out | std::fstream::trunc);
		if (fs.good()) {
			std::cout << "Logging to file: " << fileName << "\n";
		}
		else {
			logType -= CLogger::LOG_FILE;
		}
	}
	if (lt & CLogger::LOG_SCREEN) {
		std::cout << "Logging to screen:\n";
	}
}

CLogger::~CLogger() {
	if (logType & CLogger::LOG_FILE) {
		fs.close();
	}
}

void CLogger::log(logLevel level, std::string &msg) {
	int frame = ai->call->GetCurrentFrame();
	int sec   = (frame / 30) % 60;
	int min   = ((frame / 30) - sec) / 60;
	char time[10];
	sprintf(time, "[%2.2d:%2.2d] ", min, sec);
	std::string output(time);
	output += logLevels[level] + ": " + msg + "\n";

	if (logType & CLogger::LOG_FILE) {
		fs << output;
		fs.flush();
	}

	if (logType & CLogger::LOG_SCREEN) {
		std::cout << output;
	}
}

void CLogger::getFileName(char *fileName) {
	std::string mapname = std::string(ai->call->GetMapName());
	mapname.resize(mapname.size() - 4);

	time_t now1;
	time(&now1);
	struct tm* now2 = localtime(&now1);

	char buf[255];
	sprintf(buf, "%s", LOG_FOLDER);
	ai->call->GetValue(AIVAL_LOCATE_FILE_W, buf);
	std::sprintf(
		fileName, "%s%2.2d%2.2d%2.2d%2.2d%2.2d-%s-team-%d.log", 
		std::string(LOG_PATH).c_str(), 
		now2->tm_year + 1900, 
		now2->tm_mon + 1, 
		now2->tm_mday, 
		now2->tm_hour, 
		now2->tm_min, 
		mapname.c_str(), 
		ai->team
	);
}
