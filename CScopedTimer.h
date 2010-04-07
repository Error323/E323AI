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

#define SPRING_PROFILER
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
#ifdef SPRING_PROFILER
		CScopedTimer(const std::string& s, IAICallback *_cb): cb(_cb), task(s) {
			if (!cb->IsDebugDrawerEnabled())
				return;

			if (std::find(tasks.begin(), tasks.end(), task) == tasks.end()) {
				tasknr = tasks.size();
				cb->SetDebugGraphLineLabel(tasknr, task.c_str());
				cb->SetDebugGraphLineColor(tasknr, colors[tasknr%8]);
				tasks.push_back(s);
			}

			t1 = SDL_GetTicks();
		}

		~CScopedTimer() {
			t2 = SDL_GetTicks();
			t3 = t2 - t1;
			cb->AddDebugGraphPoint(tasknr, cb->GetCurrentFrame(), t3);
		}
#else
		CScopedTimer(const std::string& s, IAICallback *_cb): task(s), cb(_cb) {
			if (std::find(tasks.begin(), tasks.end(), task) == tasks.end())
				tasks.push_back(s);

			float time = cb->GetCurrentFrame() / float(counter);
			if (time > TIME_INTERVAL)
				counter += int(floorf(time/TIME_INTERVAL));

			if (timings.find(counter) == timings.end()) {
				std::map<std::string, unsigned int> M;
				M[task] = 0;
				timings[counter] = M;
			}

			t1 = SDL_GetTicks();
		}

		~CScopedTimer() {
			t2 = SDL_GetTicks();
			t3 = t2 - t1;
			timings[counter][task] += t3;
		}
#endif

		static void toFile(const std::string& filename) {
			std::ofstream ofs;
			ofs.open(filename.c_str(), std::ios::trunc);
			if (ofs.good()) {
				// labels
				ofs << "time";
				for (unsigned int i = 0; i < tasks.size(); i++)
					ofs << "\t" << tasks[i];
				ofs << "\n";

				// data for gnuplot
				std::map<unsigned int, std::map<std::string, unsigned int> >::iterator i;
				for (i = timings.begin(); i != timings.end(); i++) {
					ofs << i->first;
					for (unsigned int j = 0; j < tasks.size(); j++) {
						ofs << "\t";
						if (i->second.find(tasks[j]) == i->second.end())
							ofs << 0;
						else
							ofs << i->second[tasks[j]];
					}
					ofs << "\n";
				}

				ofs.flush();
				ofs.close();
				std::cout << "Writing timings to: " << filename << " succeeded!\n";
			}
			else
				std::cout << "Writing timings to: " << filename << " failed!\n";
		}

		

	private:
		IAICallback *cb;
		const std::string task;
		unsigned t1, t2, t3, tasknr;

		static std::map<unsigned int, std::map<std::string, unsigned int> > timings;
		static std::vector<std::string> tasks;
		static unsigned int counter;
};

#endif
