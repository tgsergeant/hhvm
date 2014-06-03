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

#ifndef incl_HPHP_TRACING_COLLECTOR_IMPL_H_
#define incl_HPHP_TRACING_COLLECTOR_IMPL_H_

#include "hphp/runtime/base/tracing-collector.h"
#include "hphp/util/thread-local.h"
#include "hphp/runtime/base/base-includes.h"

#include <boost/container/set.hpp>

namespace HPHP {

///////////////////////////////////////////////////////////////////////////////

class MarkSweepCollector {
public:
  typedef ThreadLocalSingleton<MarkSweepCollector> TlsWrapper;
  static void Create(void *);
  static void Delete(MarkSweepCollector *);
  static void OnThreadExit(MarkSweepCollector *);

public:
  MarkSweepCollector() {}
  ~MarkSweepCollector() {}

  int64_t collect();

  /**
   * Mark an object as having a destructor that will need to
   * be called when it is collected.
   */
  void markDestructable(const ObjectData *obj);

private:
  int64_t markHeap();

  int sweepHeap() {return 1;}

  void markReachable(void *ptr);
  bool isReachable(void *ptr);

  std::unordered_set<const void *> marked;

  //A vector is probably not the best data structure for this,
  //depending on the size of the data.
  //Something that is segregated based on memory block will be
  //easier to handle at collection time.
  std::vector<const ObjectData *> destructable;
};

MarkSweepCollector &gc();

///////////////////////////////////////////////////////////////////////////////

}

#endif
