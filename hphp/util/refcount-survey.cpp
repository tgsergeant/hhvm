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
#include "folly/Synchronized.h"
#include "folly/RWSpinLock.h"

#include <array>

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

TRACE_SET_MOD(tmp1);

static InitFiniNode process_death (
  []{ dump_refcount_survey(); },
  InitFiniNode::When::ProcessExit
);

//Global variable to track the total reference sizes
folly::Synchronized<std::array<long, 31>, folly::RWSpinLock> global_counts;

//Need to initialise it before we do anything
//We don't actually do anything with this object
RefcountSurvey::TlsWrapper tls;

// Return the survey object for the current thread
RefcountSurvey &survey() { return *RefcountSurvey::TlsWrapper::getCheck(); }

void track_refcount_operation(RefcountOperation op, const void *address, int32_t value) {
	survey().track_refcount_operation(op, address, value);
}

void dump_refcount_survey() {
	survey().dump_refcount_survey();
}

void dump_global_survey() {
	SYNCHRONIZED(global_counts) {
		TRACE(1, "---Request ended. Total counts so far---\n\n");
		long bitcounts[32] = {0};
		int bc;

		long total_bits = 0;
		long total_count = 0;

		TRACE(1, "Raw Sizes\n");
		for(int i = 0; i < 32; i++) {
			if (global_counts[i] > 0) {
				FTRACE(1, "{}: {}\n", i, global_counts[i]);
			}


			bc = ceil(log2((double)i + 1));
			bitcounts[bc] += global_counts[i];

			total_bits += bc * global_counts[i];
			total_count += global_counts[i];
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
			if(bitcounts[i] > 0) {
				FTRACE(1, "{}: {}\n", i, bitcounts[i]);
			}
		}

		FTRACE(1, "Mean: {}\n", mean);
		FTRACE(1, "Median: {}\n", median);
	}

}

void RefcountSurvey::Create(void *storage) {
	new (storage) RefcountSurvey();
}

void RefcountSurvey::Delete(RefcountSurvey *survey) {
	survey->~RefcountSurvey();
}

void RefcountSurvey::OnThreadExit(RefcountSurvey *survey) {
	TRACE(1, "~~~Survey thread died~~~");
	survey->~RefcountSurvey();
}

void RefcountSurvey::dump_refcount_survey() {
	SYNCHRONIZED(global_counts) {
		for (int i = 0; i < 32; i++) {
			global_counts[i] += sizecounts[i];
			sizecounts[i] = 0;
		}
	}
	dump_global_survey();
}

void RefcountSurvey::track_release(const void *address) {
	auto live_value = live_values.find(address);
	if(live_value != live_values.end()) {
		sizecounts[live_value->second] += 1;
		live_values.erase(live_value);
	}
}

void RefcountSurvey::track_change(const void *address, int32_t value) {
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

void RefcountSurvey::track_refcount_operation(RefcountOperation op, const void *address, int32_t value) {
	switch(op) {
	case RC_RELEASE:
		track_release(address);
		break;
	case RC_DEC:
	case RC_INC:
	case RC_SET:
		track_change(address, value);
		break;
	}
}


}


