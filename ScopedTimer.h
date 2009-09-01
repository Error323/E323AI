#ifndef SCOPEDTIMER_HDR
#define SCOPEDTIMER_HDR

#include <string>
#include <sstream>
#include <iomanip>
#include <map>

#include <SDL/SDL_timer.h>

#define MAX_STR_LENGTH 21

class ScopedTimer {
	public:
		ScopedTimer(const std::string& s): task(s) {
			if (times.find(task) == times.end()) {
				times[task] = 0;
				counters[task] = 0;
			}
			t1 = SDL_GetTicks();
		}

		~ScopedTimer() {
			t2           = SDL_GetTicks();
			t3           = t2 - t1;
			times[task] += t3;
			sum         += t3;
			counters[task]++;
		}

		static std::string profile() {
			std::stringstream ss;
			ss << "[ScopedTimer] milliseconds\n";
			for (int i = 0; i < MAX_STR_LENGTH; i++)
				ss << " ";
			ss << "PCT\tAVG\tTOT\n";

			std::map<std::string, unsigned>::iterator i;
			for (i = times.begin(); i != times.end(); i++) {
				int x = i->second / float(sum) * 10000;
				float pct = x / 100.0f;
				x = i->second / float(counters[i->first]) * 100;
				float avg = x / 100.0f;
				ss << "  " << i->first;
				for (unsigned j = i->first.size()+2; j < MAX_STR_LENGTH; j++)
				  ss << " ";
				ss << pct << "%\t" << avg << "\t" << i->second << "\t" << "\n";
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
		static std::map<std::string, unsigned> counters;
};

#endif
