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

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

/*
 * Keep track of how large reference counts get.
 *
 */
void track_refcount(const void *address, int32_t value);

void track_refcount_release(const void *address);

void dump_refcount_survey();
}

#endif
