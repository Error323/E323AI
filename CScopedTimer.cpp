#include "CScopedTimer.h"

std::map<std::string, int> CScopedTimer::tasknrs;
std::vector<std::string> CScopedTimer::tasks;

unsigned int CScopedTimer::counter = 0;
