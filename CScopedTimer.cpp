#include "CScopedTimer.h"

std::map<std::string, unsigned> CScopedTimer::times;
std::map<std::string, unsigned> CScopedTimer::counters;
std::map<std::string, unsigned> CScopedTimer::mintimes;
std::map<std::string, unsigned> CScopedTimer::maxtimes;
unsigned CScopedTimer::sum = 0;
