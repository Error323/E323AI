#ifndef SCOPEDTIMER_HDR
#define SCOPEDTIMER_HDR

#include <string>
#include <map>
#include <vector>
#include <algorithm>

#include "boost/thread.hpp"

#include "headers/Defines.h"
#include "headers/HAIInterface.h"
#include "headers/HEngine.h"

#define PROFILE(x) CScopedTimer t(std::string(#x), ai->cb);

// Time interval in logic frames
#define TIME_INTERVAL 1000

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
			initialized = true;
			
			if (std::find(tasks.begin(), tasks.end(), task) == tasks.end()) {
				taskIDs[task] = tasks.size();
#if !defined(BUILDING_AI_FOR_SPRING_0_81_2)
				cb->DebugDrawerSetGraphLineColor(taskIDs[task], colors[taskIDs[task] % 8]);
				cb->DebugDrawerSetGraphLineLabel(taskIDs[task], task.c_str());
#endif
				tasks.push_back(task);
				
				curTime[task] = cb->GetCurrentFrame();
				prevTime[task] = 0;
			}

			t1 = GetEngineRuntimeMSec();
		}

		~CScopedTimer() {
			t2 = GetEngineRuntimeMSec();

			if (!initialized)
				return;
#if !defined(BUILDING_AI_FOR_SPRING_0_81_2)
			unsigned int curFrame = cb->GetCurrentFrame();
			
			for (size_t i = 0; i < tasks.size(); i++) {
				const int taskID = taskIDs[tasks[i]];
				
				if (tasks[i] == task)
					prevTime[task] = t2 - t1;
				
				cb->DebugDrawerAddGraphPoint(taskID, curFrame, prevTime[tasks[i]]);

				if ((curFrame - curTime[tasks[i]]) >= TIME_INTERVAL)
					cb->DebugDrawerDelGraphPoints(taskID, 1);
			}
#endif
		}

		static unsigned int GetEngineRuntimeMSec() {
			boost::xtime t;
			boost::xtime_get(&t, boost::TIME_UTC);
			const unsigned int milliSeconds = t.sec * 1000 + (t.nsec / 1000000);
			return milliSeconds;
		}

	private:
		IAICallback *cb;
		const std::string task;
		unsigned int t1, t2;
		bool initialized;
		
		static std::vector<std::string> tasks;
		static std::map<std::string, int> taskIDs;
		static std::map<std::string, unsigned int> curTime, prevTime;
};

#endif
