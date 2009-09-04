#include "CScopedTimer.h"

std::map<std::string, unsigned> CScopedTimer::times;
std::map<std::string, unsigned> CScopedTimer::counters;
unsigned CScopedTimer::sum = 0;
