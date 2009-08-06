#ifndef SCOPEDTIMER_HDR
#define SCOPEDTIMER_HDR

#include <string>
#include <sstream>
#include <iomanip>
#include <map>

#include <SDL/SDL_timer.h>

#define MAX_STR_LENGTH 20

class ScopedTimer {
	public:
		ScopedTimer(const std::string& s): task(s) {
			if (times.find(task) == times.end())
				times[task] = 0;
			t1 = SDL_GetTicks();
		}

		~ScopedTimer() {
			t2           = SDL_GetTicks();
			t3           = t2 - t1;
			times[task] += t3;
			sum         += t3;
		}

		static std::string profile() {
			std::stringstream ss;
			ss << "\n[ScopedTimer] milliseconds\n";

			std::map<std::string, unsigned>::iterator i;
			for (i = times.begin(); i != times.end(); i++) {
				ss << "  " << i->first;
				for (unsigned j = i->first.size()+2; j < MAX_STR_LENGTH; j++)
				  ss << " ";
				ss << i->second << "\n";
			}

			ss << "\n[ScopedTimer] percentage\n";
			for (i = times.begin(); i != times.end(); i++) {
				int x = i->second / float(sum) * 10000;
				float pct = x / 100.0f;
				ss << "  " << i->first;
				for (unsigned j = i->first.size()+2; j < MAX_STR_LENGTH; j++)
				  ss << " ";
				ss << pct << "\n";
			}
			ss << "\n";
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

#endif
