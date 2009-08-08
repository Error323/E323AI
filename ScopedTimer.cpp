#include "ScopedTimer.h"

std::map<std::string, unsigned> ScopedTimer::times;
std::map<std::string, unsigned> ScopedTimer::counters;
unsigned ScopedTimer::sum = 0;
