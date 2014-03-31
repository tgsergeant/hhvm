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

#ifndef incl_HPHP_REFCOUNT_SURVEY_IMPL_H_
#define incl_HPHP_REFCOUNT_SURVEY_IMPL_H_

#include "hphp/util/refcount-survey.h"

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////


struct RefcountSurvey;
RefcountSurvey &survey();

#define TIME_GRANULARITY 5000


struct RefcountSurvey {
	typedef ThreadLocalSingleton<RefcountSurvey> TlsWrapper;
	static void Create(void*);
	static void Delete(RefcountSurvey*);
	static void OnThreadExit(RefcountSurvey*);
public:
	void track_refcount_operation(RefcountOperation op, const void *address, int32_t value);
	void track_refcount_request_end();

private:
	long sizecounts[32] = {0};
	long total_ops = 0;

	boost::unordered_map<const void *, int> live_values;
	std::vector<int> release_times;

	void track_change(const void *address, int32_t value);
	/**
	 * Record that a address has been released:
	 *  - Remember the max value reached by this address
	 *  - Mark that a release happened in the current time slot.
	 */
	void track_release(const void *address);

	/**
	 * Return this thread to its initial state at the end of a request
	 */
	void reset();
};

}


#endif
