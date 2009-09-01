#ifndef LOGGER_HDR
#define LOGGER_HDR

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <map>

class AIClasses;

class CLogger {
	public:
		enum type{LOG_FILE, LOG_SCREEN};

		CLogger(AIClasses *_ai, unsigned lt);
		~CLogger();

		/* Error logging */
		void e(std::string msg) { log(ERROR, msg); }

		/* Warning logging */
		void w(std::string msg) { log(WARNING, msg); }

		/* Verbose logging */
		void v(std::string msg) { log(VERBOSE, msg); }

	private:
		enum logLevel{ERROR, WARNING, VERBOSE};

		AIClasses *ai;

		/* The log types */
		unsigned logType;

		/* File stream */
		std::fstream fs;

		/* logLevels to string */
		std::map<logLevel, std::string> logLevels;

		/* Perform logging @ defined logTypes */
		void log(logLevel level, std::string &msg);

		/* Get the filename for logging the fstream to */
		void getFileName(char *fileName);
};

#endif
