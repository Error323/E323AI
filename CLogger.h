#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <map>

class AIClasses;

class CLogger {
	public:
		enum type { LOG_FILE = (1<<0), LOG_STDOUT = (1<<1), LOG_SPRING = (1<<2) };
		enum logLevel { NONE = 0, ERROR, WARNING, VERBOSE };

		CLogger(AIClasses *_ai, unsigned int lt, logLevel lf = VERBOSE);
		~CLogger() {}

		/* Error logging */
		void e(std::string msg) { log(ERROR, msg); }

		/* Warning logging */
		void w(std::string msg) { log(WARNING, msg); }

		/* Verbose logging */
		void v(std::string msg) { log(VERBOSE, msg); }

		/* Log to spring */
		void s(std::string msg);

	private:
		std::string fileName;

		AIClasses *ai;

		/* Sum of log type flags */
		unsigned int logType;

		logLevel logFilter;

		/* File stream */
		std::ofstream ofs;

		/* logLevels to string */
		static std::map<logLevel, std::string> logLevels;
		static std::map<logLevel, std::string> logDesc;

		/* Perform logging @ defined logTypes */
		void log(logLevel level, std::string &msg);
};

#define LOG_EE(x)  {std::stringstream ss; ss << x; ai->logger->e(ss.str());}
#define LOG_WW(x)  {std::stringstream ss; ss << x; ai->logger->w(ss.str());}
#define LOG_II(x)  {std::stringstream ss; ss << x; ai->logger->v(ss.str());}
#define LOG_SS(x)  {std::stringstream ss; ss << x; ai->logger->s(ss.str());}

#endif
