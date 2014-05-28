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

#include "hphp/runtime/base/base-includes.h"
#include <boost/container/set.hpp>

namespace HPHP {

///////////////////////////////////////////////////////////////////////////////

int64_t tracingGCCollect();

class MarkSweepCollector {
public: 
  MarkSweepCollector() {}

  int64_t markHeap();

  int sweepHeap() {return 1;}

private:
  void markReachable(void *ptr);
  bool isReachable(void *ptr);

  std::unordered_set<const void *> marked;

};

///////////////////////////////////////////////////////////////////////////////

}
