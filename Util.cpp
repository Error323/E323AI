#include <cassert>
#include <cstring>
#include <cmath>
#include <algorithm>

#include "ExternalAI/IAICallback.h"
#include "./Util.hpp"

namespace util {
	std::string GetAbsFileName(IAICallback* cb, const std::string& relFileName, bool readonly) {
		char        dst[1024] = {0};
		const char* src       = relFileName.c_str();
		const int   len       = relFileName.size();

		// last char ('\0') in dst
		// should not be overwritten
		assert(len < (1024 - 1));
		memcpy(dst, src, len);

		// get the absolute path to the file
		// (and create folders along the way)
		if (readonly) {
			cb->GetValue(AIVAL_LOCATE_FILE_R, dst);
		} else {
			cb->GetValue(AIVAL_LOCATE_FILE_W, dst);
		}

		return (std::string(dst));
	}


	void StringToLowerInPlace(std::string& s) {
		std::transform(s.begin(), s.end(), s.begin(), (int (*)(int))tolower);
	}
	std::string StringToLower(std::string s) {
		StringToLowerInPlace(s);
		return s;
	}

	std::string StringStripSpaces(const std::string& s1) {
		std::string s2; s2.reserve(s1.size());

		for (std::string::const_iterator it = s1.begin(); it != s1.end(); it++) {
			if (!isspace(*it)) {
				s2.push_back(*it);
			}
		}

		return s2;
	}


	float WeightedAverage(std::list<float>& V, std::list<float>& W) {
		float wavg = 0.0f;
		std::list<float>::const_iterator v, w;
		for (w = W.begin(), v = V.begin(); v != V.end() && w != W.end(); w++, v++)
			wavg += ((*w) * (*v));

		return wavg;
	}


	bool IsBinarySubset(unsigned A, unsigned B) {
		unsigned cA     = CountOneBits(A);
		unsigned cAandB = CountOneBits(A&B);

		return (cA == cAandB);
	}

	unsigned int CountOneBits(unsigned int n) {
		const int S[] = {1, 2, 4, 8, 16};
		const int B[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF};
		int c = n;
		c = ((c >> S[0]) & B[0]) + (c & B[0]);
		c = ((c >> S[1]) & B[1]) + (c & B[1]);
		c = ((c >> S[2]) & B[2]) + (c & B[2]);
		c = ((c >> S[3]) & B[3]) + (c & B[3]);
		c = ((c >> S[4]) & B[4]) + (c & B[4]);
		return c;
	}

	float GaussDens(float x, float mu, float sigma) {
		const float a = 1.0f / (sigma * std::sqrt(2.0f * M_PI));
		const float b = std::exp(-(((x - mu) * (x - mu)) / (2.0f * sigma * sigma)));
		return (a * b);
	}
}
