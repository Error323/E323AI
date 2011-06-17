#ifndef UTIL_H
#define UTIL_H

#include <algorithm>
#include <list>
#include <set>
#include <string>
#include <vector>
#include <map>

namespace springLegacyAI {
	class IAICallback;
}

namespace util {
	std::string GetAbsFileName(IAICallback*, const std::string&, bool readonly = false);
	void SanitizeFileNameInPlace(std::string&);

	void StringToLowerInPlace(std::string&);
	std::string StringToLower(std::string);
	std::string StringStripSpaces(const std::string&);
	/**
	 * Removes all white space chars in the supplied string.
	 * @param s to clean string
	 */
	void RemoveWhiteSpaceInPlace(std::string& s);
	std::string IntToString(int i, const std::string& format = "%i");
	void StringSplit(const std::string& src, char splitter, std::vector<std::string>& dest, bool empty = true);
	bool StringContainsChar(const std::string& src, const char c);

	float WeightedAverage(std::list<float>&, std::list<float>&);

	float GaussDens(float, float mu = 0.0f, float sigma = 1.0f);

	unsigned int CountOneBits(unsigned int n);

	/**
	 * Determines if A is a binary subset of B
	 *
	 * @param unsigned, binary mask A
	 * @param unsigned, binary mask B
	 * @return bool, A \subseteq B
	 */
	bool IsBinarySubset(unsigned A, unsigned B);

	template<class T>
	std::set<T> IntersectSets(const std::set<T>& s1, const std::set<T>& s2) {
		std::set<T> r;
		typename std::set<T>::const_iterator sit;

		for (sit = s1.begin(); sit != s1.end(); sit++) {
			if (s2.find(*sit) != s2.end()) {
				r.insert(*sit);
			}
		}

		return r;
	}

	template<class T>
	std::set<T> UnionSets(const std::set<T>& s1, const std::set<T>& s2) {
		std::set<T> r;

		r.insert(s1.begin(), s1.end());
		r.insert(s2.begin(), s2.end());

		return r;
	}

	template <class Key, class T>
	void GetShuffledKeys(std::vector<Key> &out, std::map<Key, T> &in) {
		typename std::map<Key, T>::iterator it;
		for (it = in.begin(); it != in.end(); it++) {
			out.push_back(it->first);
		}
		std::random_shuffle(out.begin(), out.end());
	}
}

#endif
