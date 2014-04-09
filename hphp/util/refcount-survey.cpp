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

#include "hphp/util/refcount-survey-impl.h"
#include "hphp/runtime/base/thread-init-fini.h"
#include "folly/Synchronized.h"
#include "folly/RWSpinLock.h"

#include <array>

namespace HPHP {

///////////////////////////////////////////////////////////////////////////////

TRACE_SET_MOD(rcsurvey);

//Global variable to track the total reference sizes
folly::Synchronized<Histogram<32>, folly::RWSpinLock> global_refcount_sizes;

folly::Synchronized<Histogram<129>, folly::RWSpinLock> global_object_sizes;

//Need to initialise it before we do anything
//We don't actually do anything with this object
RefcountSurvey::TlsWrapper tls;

// Return the survey object for the current thread
RefcountSurvey &survey() { return *RefcountSurvey::TlsWrapper::getCheck(); }

void track_refcount_operation(RefcountOperation op, const void *address, int32_t value) {
	survey().track_refcount_operation(op, address, value);
}

void track_refcount_request_end() {
	survey().track_refcount_request_end();
}

void dump_global_survey() {
	SYNCHRONIZED_CONST(global_refcount_sizes) {

		long bitcounts[32] = {0};
		int bc;

		long total_bits = 0;
		long total_count = 0;

		TRACE(1, "\n\nRaw Refcount Sizes (across all requests)\n");
		TRACE(1, "Size,Frequency\n");
		TRACE(1, global_refcount_sizes.print());
		for(int i = 0; i < 32; i++) {
			bc = ceil(log2((double)i + 1));
			bitcounts[bc] += global_refcount_sizes[i];

			total_bits += bc * global_refcount_sizes[i];
			total_count += global_refcount_sizes[i];
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

		TRACE(1, "\n\nRefcount Bits required\n");
		TRACE(1, "Bits,Frequency\n");
		for(int i = 0; i < 32; i++) {
			if(bitcounts[i] > 0) {
				FTRACE(1, "{},{}\n", i, bitcounts[i]);
			}
		}
		TRACE(1, "\n");
		FTRACE(1, "Mean: {}\n", mean);
		FTRACE(1, "Median: {}\n", median);
	}

	SYNCHRONIZED_CONST(global_object_sizes) {
		TRACE(1, "\n\nObject sizes (across all requests)\n");
		TRACE(1, "Size,Frequency\n");
		long total_bytes = 0;
		long total_objects = 0;
		for(int i = 0; i <= 128; i++) {
			if(global_object_sizes[i] != 0) {
				FTRACE(2, "{},{}\n", i << 4, global_object_sizes[i]);
				total_bytes += (i << 4) * global_object_sizes[i];
				total_objects += global_object_sizes[i];
			}
		}
		TRACE(2, "\nTotal Objects,Total Bytes,Average Bytes\n");
		FTRACE(2, "{},{},{:.1f}\n\n", total_objects, total_bytes, (double)total_bytes / total_objects);
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

void RefcountSurvey::track_refcount_request_end() {
	//Processing that needs to be done before we print results
	SYNCHRONIZED(global_refcount_sizes) {
		global_refcount_sizes.add(refcount_sizes);
	}
	SYNCHRONIZED(global_object_sizes) {
		global_object_sizes.add(object_sizes);
	}
	for(auto obj : live_values) {
		increment_lifetime_bucket(obj.second.allocation_time);
	}

	// Then print the results (but only if the request was something interesting)
	if(total_ops > 15000) {
		TRACE(1, "\n-----Start of report-----\n");

		auto const stats = MM().getStats();
		TRACE(1, "\n\nGeneral memory statistics\n");
		FTRACE(1, "Peak usage,\t\t{}\n", stats.peakUsage);
		FTRACE(1, "Peak allocation,\t{}\n", stats.peakAlloc);
		FTRACE(1, "Total allocation,\t{}\n", stats.totalAlloc);

		dump_global_survey();

		TRACE(2, "\n\nMemory activity over time\nTimeslot,Releases,Allocations,Allocation Size\n");
		for(int i = 0; i < timed_activity.size(); i++) {
			auto r = timed_activity[i];
			FTRACE(2, "{},{},{},{}\n", i + 1, r.deallocations, r.allocations, r.allocations_size);
		}
		FTRACE(2, "{},{},,\n", timed_activity.size() + 1, live_values.size());


		TRACE(2, "\n\nObject lifetimes\n");
		TRACE(2, "Time bucket,Frequency\n");
		TRACE(2, object_lifetimes.print(false, LIFETIME_GRANULARITY));
		FTRACE(2, "\nTotal operations: {}\n", total_ops);

		TRACE(3, "\n\nRequest Object Sizes\n");
		TRACE(3, "Size,Frequency\n");
		TRACE(3, object_sizes.print(false, 16));


		TRACE(1, "\n-----End of report-----\n\n");
	}

	reset();
}


void RefcountSurvey::track_release(const void *address) {
	auto live_value = live_values.find(address);
	if(live_value != live_values.end()) {
		refcount_sizes.incr(live_value->second.max_refcount);
		increment_lifetime_bucket(live_value->second.allocation_time);
		live_values.erase(live_value);
	}
	get_current_bucket()->deallocations += 1;
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
		live_values[address] = ObjectLifetimeData();
		live_values[address].allocation_time = total_ops;
		live_values[address].max_refcount = value;
	}
	else {
		if(live_value->second.max_refcount < value) {
			live_values[address].max_refcount = value;
		}
	}
}

void RefcountSurvey::track_refcount_operation(RefcountOperation op, const void *address, int32_t value) {
	total_ops ++;

	switch(op) {
	case RC_RELEASE:
		track_release(address);
		break;
	case RC_DEC:
	case RC_INC:
	case RC_SET:
		track_change(address, value);
		break;
	case RC_ALLOC:
		track_alloc(address, value);
		break;
	}
}

void RefcountSurvey::track_alloc(const void *address, int32_t value) {
	TimeDeltaActivity *bucket = get_current_bucket();
	bucket->allocations += 1;
	bucket->allocations_size += value;

	int size;
	if(value < 2048) {
		size = ((value + 8) & ~15) >> 4;
	} else {
		size = 128;
	}
	object_sizes.incr(size);
}

void RefcountSurvey::increment_lifetime_bucket(long allocation_time) {
	long lifetime = total_ops - allocation_time;
	int lifetime_bucket = (int)((double)lifetime / LIFETIME_GRANULARITY) + 1;
	if(lifetime_bucket > object_lifetimes.size()) {
		lifetime_bucket = object_lifetimes.size() - 1;
	}
	object_lifetimes.incr(lifetime_bucket);
}

void RefcountSurvey::reset() {
	refcount_sizes.clear();
	object_sizes.clear();
	live_values.clear();
	object_lifetimes.clear();
	timed_activity.clear();
	total_ops = 0;
}

TimeDeltaActivity *RefcountSurvey::get_current_bucket() {
	int bucket = total_ops / TIME_GRANULARITY;
	if(timed_activity.size() <= bucket) {
		timed_activity.push_back(TimeDeltaActivity());
	}
	return &timed_activity[bucket];
}

///////////////////////////////////////////////////////////////////////////////

}

