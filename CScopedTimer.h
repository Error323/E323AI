#ifndef SCOPEDTIMER_HDR
#define SCOPEDTIMER_HDR

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <math.h>

#define USE_SDL_TIMER

// Time interval in logic frames
#define TIME_INTERVAL 1800

#ifdef USE_SDL_TIMER
	#include <SDL_timer.h>
#else
	#include <time.h>
#endif

#define MAX_STR_LENGTH 30

#ifdef DEBUG
	#define PROFILE(x) CScopedTimer t(std::string(#x), ai->cb->GetCurrentFrame());
#else
	#define PROFILE(x)
#endif


class CScopedTimer {
	public:
		CScopedTimer(const std::string& s, unsigned int frames): task(s) {
			if (std::find(tasks.begin(), tasks.end(), task) == tasks.end())
				tasks.push_back(s);

			float time = frames / float(counter);
			if (time > TIME_INTERVAL)
				counter += int(floorf(time/TIME_INTERVAL));

			if (timings.find(counter) == timings.end()) {
				std::map<std::string, unsigned int> M;
				M[task] = 0;
				timings[counter] = M;
			}

#ifdef USE_SDL_TIMER
			t1 = SDL_GetTicks();
#else
			t1 = clock();
#endif
		}

		~CScopedTimer() {
#ifdef USE_SDL_TIMER
			t2 = SDL_GetTicks();
#else
			t2 = clock();
#endif
			t3 = t2 - t1;
			timings[counter][task] += t3;
		}

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
		const std::string task;
		unsigned t1, t2, t3;

		static std::map<unsigned int, std::map<std::string, unsigned int> > timings;
		static std::vector<std::string> tasks;
		static unsigned int counter;
};

#endif
