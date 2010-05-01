#include "MathUtil.h"

#include "headers/Defines.h"

#include <math.h>

#if defined(_MSC_VER)

float round(float val)
{
#if defined(__cplusplus)
    return floor(val + 0.5f);
#else
	return floorf(val + 0.5f);
#endif
}

double round(double val)
{
	return floor(val + 0.5);
}

#endif
