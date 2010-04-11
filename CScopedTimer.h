#ifndef SCOPEDTIMER_HDR
#define SCOPEDTIMER_HDR

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <math.h>

#include <SDL_timer.h>

#include "headers/Defines.h"
#include "headers/HAIInterface.h"
#include "headers/HEngine.h"

#define PROFILE(x) CScopedTimer t(std::string(#x), ai->cb);

// Time interval in logic frames
#define TIME_INTERVAL 300

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
		CScopedTimer(const std::string& s, IAICallback *_cb): cb(_cb), task(s) {
			initialized = cb->IsDebugDrawerEnabled();
			if (!initialized)
				return;

			if (std::find(tasks.begin(), tasks.end(), task) == tasks.end()) {
				tasknrs[task] = tasks.size();
				cb->DebugDrawerSetGraphLineColor(tasknrs[task], colors[tasknrs[task]%8]);
				cb->DebugDrawerSetGraphLineLabel(tasknrs[task], task.c_str());
				tasks.push_back(task);
				counters[task] = 0;
			}

			counters[task]++;
			t1 = SDL_GetTicks();
		}

		~CScopedTimer() {
			t2 = SDL_GetTicks();

			if (!initialized)
				return;

			cb->DebugDrawerAddGraphPoint(tasknrs[task], cb->GetCurrentFrame(), (t2-t1));
			if (counters[task] >= 50)
				cb->DebugDrawerDelGraphPoints(tasknrs[task], 1);
		}

	private:
		IAICallback *cb;
		const std::string task;
		unsigned t1, t2;
		bool initialized;

		static std::vector<std::string> tasks;
		static std::map<std::string, int> tasknrs;
		static std::map<std::string, int> counters;
};

#endif
