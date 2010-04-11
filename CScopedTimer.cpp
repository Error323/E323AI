#include "CScopedTimer.h"

std::map<std::string, int> CScopedTimer::taskIDs;
std::map<std::string, unsigned int> CScopedTimer::curTime;
std::map<std::string, unsigned int> CScopedTimer::prevTime;
std::vector<std::string> CScopedTimer::tasks;
