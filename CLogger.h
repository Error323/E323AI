#ifndef LOGGER_HDR
#define LOGGER_HDR

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <map>

class AIClasses;

#define LOG_EE(x)  {std::stringstream ss; ss << x; ai->l->e(ss.str());}
#define LOG_WW(x)  {std::stringstream ss; ss << x; ai->l->w(ss.str());}
#define LOG_II(x)  {std::stringstream ss; ss << x; ai->l->v(ss.str());}

class CLogger {
	public:
		enum type{LOG_FILE = (1<<0), LOG_SCREEN = (1<<1)};

		CLogger(AIClasses *_ai, unsigned lt);
		~CLogger(){}

		/* Error logging */
		void e(std::string msg) { log(ERROR, msg); }

		/* Warning logging */
		void w(std::string msg) { log(WARNING, msg); }

		/* Verbose logging */
		void v(std::string msg) { log(VERBOSE, msg); }

	private:
		enum logLevel{ERROR, WARNING, VERBOSE};

		char fileName[255];

		AIClasses *ai;

		/* The log types */
		unsigned logType;

		/* File stream */
		std::ofstream ofs;

		/* logLevels to string */
		std::map<logLevel, std::string> logLevels;
		std::map<logLevel, std::string> logDesc;

		/* Perform logging @ defined logTypes */
		void log(logLevel level, std::string &msg);

		/* Get the filename for logging the fstream to */
		void getFileName(char *fileName);
};

#endif
