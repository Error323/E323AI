#ifndef SCOPEDTIMER_HDR
#define SCOPEDTIMER_HDR

#include <string>
#include <SDL/SDL_timer.h>

class ScopedTimer {
	public:
		ScopedTimer(const std::string& s): task(s), t1(SDL_GetTicks()) {
		}

		~ScopedTimer() {
			t2 = SDL_GetTicks();
			t3 = t2 - t1;

			printf("%s: %u msecs\n", task.c_str(), t3);
		}

	private:
		const std::string task;
		unsigned int t1;
		unsigned int t2;
		unsigned int t3;
};

#endif
