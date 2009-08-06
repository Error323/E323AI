#include "ScopedTimer.h"

std::map<std::string, unsigned> ScopedTimer::times;
unsigned ScopedTimer::sum = 0;
