#ifndef SCOPEDTIMER_HDR
#define SCOPEDTIMER_HDR

#include <string>
#include <sstream>
#include <map>

#include <SDL/SDL_timer.h>

class ScopedTimer {
	public:
		ScopedTimer(const std::string& s): task(s), t1(SDL_GetTicks()) {
			if (times.find(task) == times.end())
				times[task] = 0;
		}

		~ScopedTimer() {
			t2           = SDL_GetTicks();
			t3           = t2 - t1;
			times[task] += t3;
			sum         += t3;
		}

		static std::string profile() {
			std::stringstream ss;
			ss << "[ScopedTimer] milliseconds\n";

			std::map<std::string, unsigned>::iterator i;
			for (i = times.begin(); i != times.end(); i++) {
				ss << "\t" << i->first << "\t" << i->second << "\n";
			}

			ss << "\n[ScopedTimer] percentage\n";
			for (i = times.begin(); i != times.end(); i++) {
				float pct = i->second / float(sum) * 100.0f;
				ss << "\t" << i->first << "\t" << pct << "\n";
			}

			return ss.str();
		}

		

	private:
		const std::string task;
		unsigned t1;
		unsigned t2;
		unsigned t3;

		static unsigned sum;
		static std::map<std::string, unsigned> times;
};

std::map<std::string, unsigned> ScopedTimer::times;
unsigned ScopedTimer::sum = 0;

#endif
