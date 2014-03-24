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

#include "hphp/runtime/base/countable.h"

///////////////////////////////////////////////////////////////////////////////
namespace HPHP {
long sizecounts[32] = {0};

void dump_refcount_survey() {
	std::cout << "Refcount survey statistics" << std::endl;
	long bitcounts[32] = {0};
	int bc;

	long total_bits = 0;
	long total_count = 0;

	std::cout << "Raw Sizes" << std::endl;
	for(int i = 0; i < 32; i++) {
		std::cout << i << ": " << sizecounts[i] << std::endl;

		bc = ceil(log2((double)i + 1));
		bitcounts[bc] += sizecounts[i];

		total_bits += bc * sizecounts[i];
		total_count += sizecounts[i];
	}

	double mean = (double)total_bits / total_count;

	long half = total_count / 2;
	long half_sum = 0;

	double median;

	for(int i = 0; i < 32; i++) {
		half_sum += bitcounts[i];
		if (half_sum > half) {
			long pre = half_sum - bitcounts[i];
			long diff = half - pre;
			double proportion = (double)diff / bitcounts[i];
			median = i - proportion;
			break;
		}
	}

	std::cout << "Bitcounts" << std::endl;
	for(int i = 0; i < 32; i++) {
		std::cout << i << ": " << bitcounts[i] << std::endl;
	}

	std::cout << "Mean: " << mean << std::endl;
	std::cout << "Median: " << median << std::endl;

}

void track_refcount(RefCount value) {
	if (value > 31) {
		value = 31;
	}
	sizecounts[value] += 1;
}

}
