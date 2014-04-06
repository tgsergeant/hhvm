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


enum RefcountOperation {
	RC_INC,
	RC_DEC,
	RC_RELEASE,
	RC_SET,
	RC_ALLOC
};

/*
 * Main helper method. Passes the data through to the ThreadLocalSingleton,
 * which then chooses how to proceed based on the operation
 */
void track_refcount_operation(RefcountOperation op, const void *address, int32_t value = -1);
/*
 * Mark the current request as ended. Print results, add to global counters
 * and clear everything out in preparation for the next request
 */
void track_refcount_request_end();

}


#endif
