#ifndef SCOPEDTIMER_H
#define SCOPEDTIMER_H

#include <string>
#include <map>
#include <vector>
#include <algorithm>

#include "boost/thread.hpp"

#include "headers/Defines.h"
#include "headers/HAIInterface.h"
#include "headers/HEngine.h"

#define PROFILE(x) CScopedTimer t(std::string(#x), ai->cb);

// Time interval in logic frames (1 min)
#define TIME_INTERVAL 1800

static const float3 colors[] = {
	float3(1.0f, 0.0f, 0.0f),
	float3(0.0f, 1.0f, 0.0f),
	float3(0.0f, 0.0f, 1.0f),
	float3(1.0f, 1.0f, 0.0f),
	float3(0.0f, 1.0f, 1.0f),
	float3(1.0f, 0.0f, 1.0f),
	float3(0.0f, 0.0f, 0.0f),
	float3(1.0f, 1.0f, 1.0f)
};

class CScopedTimer {
	
public:
	CScopedTimer(const std::string& s, IAICallback *_cb);
	~CScopedTimer();

	static unsigned int GetEngineRuntimeMSec() {
		boost::xtime t;
		boost::xtime_get(&t, boost::TIME_UTC);
		const unsigned int milliSeconds = t.sec * 1000 + (t.nsec / 1000000);
		return milliSeconds;
	}

private:
	static std::vector<std::string> tasks;
	static std::map<std::string, int> taskIDs;
	static std::map<std::string, unsigned int> curTime, prevTime;
	
	IAICallback *cb;
	bool initialized;
	unsigned int t1, t2;
	const std::string task;
};

#endif
