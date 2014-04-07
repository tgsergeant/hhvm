/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2014 Facebook, Inc. (http://www.facebook.com)     |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifndef incl_HPHP_HISTOGRAM_H_
#define incl_HPHP_HISTOGRAM_H_

#include <array>
#include <string>

namespace HPHP {

///////////////////////////////////////////////////////////////////////////////

template<size_t N>
class Histogram {
public:
	Histogram() {
		clear();
	}

	long get(int bucket) const {
		return data[bucket];
	}

	void add(int bucket, long amount) {
		data[bucket] += amount;
	}

	void incr(int bucket) {
		data[bucket] += 1;
	}

	void set(int bucket, long amount) {
		data[bucket] = amount;
	}

	void add(const Histogram<N> other) {
		for(int i = 0; i < N; i++) {
			data[i] += other.get(i);
		}
	}

	void clear() {
		for(int i = 0; i < N; i++) {
			data[i] = 0;
		}
	}

	size_t size() {
		return N;
	}

	std::string print(bool showZeros = false, int index_mult = 1) const {
		std::stringstream str;
		for(int i = 0; i < N; i++) {
			if(showZeros || data[i] != 0) {
				str << i * index_mult << "," << data[i] << '\n';
			}
		}
		return str.str();
	}

	long operator[](size_t pos) const {
		return data[pos];
	}

private:
	std::array<long, N> data;
};

///////////////////////////////////////////////////////////////////////////////

}


#endif
