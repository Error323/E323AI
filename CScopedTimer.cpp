#include "CScopedTimer.h"

std::map<unsigned int, std::map<std::string, unsigned int> > CScopedTimer::timings;
std::vector<std::string> CScopedTimer::tasks;
unsigned int CScopedTimer::counter = 1;
