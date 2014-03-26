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

#include "hphp/util/refcount-survey.h"
#include "hphp/runtime/base/thread-init-fini.h"

#include <boost/unordered_map.hpp>

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////
long sizecounts[32] = {0};

boost::unordered_map<const void *, int> live_values;

TRACE_SET_MOD(tmp1);

static InitFiniNode process_death (
  []{ dump_refcount_survey(); },
  InitFiniNode::When::ProcessExit
);

// This currently doesn't actually work. Need a new way to do this.
static InitFiniNode server_death (
  []{ dump_refcount_survey(); },
  InitFiniNode::When::ServerExit
);

void dump_refcount_survey() {
	TRACE(1, "---Refcount survey statistics---\n\n");
	long bitcounts[32] = {0};
	int bc;

	long total_bits = 0;
	long total_count = 0;

	TRACE(1, "Raw Sizes\n");
	for(int i = 0; i < 32; i++) {
		FTRACE(1, "{}: {}\n", i, sizecounts[i]);

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

	TRACE(1, "\nBits required\n");
	for(int i = 0; i < 32; i++) {
		FTRACE(1, "{}: {}\n", i, bitcounts[i]);
	}

	FTRACE(1, "Mean: {}\n", mean);
	FTRACE(1, "Median: {}\n", median);

}

void track_refcount_release(const void *address) {
	auto live_value = live_values.find(address);
	if(live_value != live_values.end()) {
		sizecounts[live_value->second] += 1;
		live_values.erase(live_value);
	}
}

void track_refcount_change(const void *address, int32_t value) {
	if (value > 31) {
		value = 31;
	}
	if (value < 0) {
		return;
	}

	auto live_value = live_values.find(address);
	if (live_value == live_values.end()) {
		live_values[address] = value;
	}
	else {
		if(live_value->second < value) {
			live_values[address] = value;
		}
	}
}

void track_refcount_operation(RefcountOperation op, const void *address, int32_t value) {
	switch(op) {
	case RC_RELEASE:
		track_refcount_release(address);
		break;
	case RC_DEC:
	case RC_INC:
	case RC_SET:
		track_refcount_change(address, value);
		break;
	}
}


}


