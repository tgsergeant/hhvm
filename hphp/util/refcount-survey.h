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

#ifndef incl_HPHP_REFCOUNT_SURVEY_H_
#define incl_HPHP_REFCOUNT_SURVEY_H_

#include <cstdlib>
#include <boost/unordered_map.hpp>

#include "hphp/util/thread-local.h"


namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

/*
 * Keep track of how large reference counts get.
 *
 */
enum RefcountOperation {
	RC_INC,
	RC_DEC,
	RC_RELEASE,
	RC_SET
};

struct RefcountSurvey;
RefcountSurvey &survey();


struct RefcountSurvey {
	typedef ThreadLocalSingleton<RefcountSurvey> TlsWrapper;
	static void Create(void*);
	static void Delete(RefcountSurvey*);
	static void OnThreadExit(RefcountSurvey*);
public:
	void track_refcount_operation(RefcountOperation op, const void *address, int32_t value);
	void dump_refcount_survey();

private:
	long sizecounts[32] = {0};

	boost::unordered_map<const void *, int> live_values;

	void track_change(const void *address, int32_t value);
	void track_release(const void *address);
};

void track_refcount_operation(RefcountOperation op, const void *address, int32_t value = -1);

void dump_refcount_survey();


}


#endif
