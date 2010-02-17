#ifndef SCOPEDTIMER_HDR
#define SCOPEDTIMER_HDR

#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include <limits>

#define USE_SDL_TIMER

#ifdef USE_SDL_TIMER
	#include <SDL_timer.h>
#else
	#include <time.h>
#endif

#define MAX_STR_LENGTH 30

#ifdef DEBUG
	#define PROFILE(x) CScopedTimer t(std::string((#x)));
#else
	#define PROFILE(x)
#endif


class CScopedTimer {
	public:
		CScopedTimer(const std::string& s): task(s) {
			if (times.find(task) == times.end()) {
				times[task] = 0;
				counters[task] = 0;
				maxtimes[task] = 0;
				mintimes[task] = std::numeric_limits<unsigned>::max();
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
			times[task] += t3;
			if (t3 > 0) // we are not interested in 0 timings
				mintimes[task] = std::min<unsigned>(mintimes[task], t3);
			maxtimes[task] = std::max<unsigned>(maxtimes[task], t3);
			sum += t3;
			counters[task]++;
		}

		static std::string profile() {
			std::stringstream ss;
			ss << "[CScopedTimer] milliseconds\n";
			for (int i = 0; i < MAX_STR_LENGTH; i++)
				ss << " ";
			ss << "PCT\tAVG\tMIN\tMAX\tTOT\n";

			std::map<std::string, unsigned>::iterator i;
			for (i = times.begin(); i != times.end(); i++) {
				int x = i->second / float(sum) * 10000;
				float pct = x / 100.0f;
				x = i->second / float(counters[i->first]) * 100;
				float avg = x / 100.0f;
				ss << "  " << i->first;
				for (unsigned j = i->first.size()+2; j < MAX_STR_LENGTH; j++)
					ss << " ";
#ifndef USE_SDL_TIMER
				if(mintimes[i->first] == std::numeric_limits<unsigned>::max())
					mintimes[i->first] = 0;
				mintimes[i->first] = unsigned(((float)mintimes[i->first] / CLOCKS_PER_SEC) * 1000.0f);
				maxtimes[i->first] = unsigned(((float)maxtimes[i->first] / CLOCKS_PER_SEC) * 1000.0f);
				i->second = unsigned(((float)i->second / CLOCKS_PER_SEC) * 1000.0f);
#endif
				ss << pct << "%\t" << avg << "\t" << mintimes[i->first] << "\t" << maxtimes[i->first] << "\t" << i->second << "\t" << "\n";
			}
			ss << "\n";
			return ss.str();
		}

		

	private:
		const std::string task;
		unsigned t1;
		unsigned t2;
		unsigned t3;

		static unsigned sum; // total time for all timers
		static std::map<std::string, unsigned> times;
		static std::map<std::string, unsigned> counters;
		static std::map<std::string, unsigned> mintimes,maxtimes;
};

#endif
