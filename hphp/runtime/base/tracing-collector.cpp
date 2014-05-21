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

#include <queue>
#include <iostream>

#include "hphp/runtime/base/tracing-collector.h"

namespace HPHP {

///////////////////////////////////////////////////////////////////////////////


int64_t tracingGCCollect() {
  MarkSweepCollector msc;
  msc.markHeap();
  return 110;
}

void MarkSweepCollector::markHeap() {
  Stack& stack = g_context->getStack();
  TypedValue *current = (TypedValue *)stack.getStackHighAddress();

  std::queue<TypedValue> searchQ;

  while (current != (TypedValue *)stack.getStackLowAddress()) {
    searchQ.push(*current);
    current --;
  }

  std::cout << "Found " << searchQ.size() << " roots" << std::endl;

  TypedValue cur;

  while (!searchQ.empty()) {
    cur = searchQ.front();

    if(cur.m_type == DataType::KindOfArray) {
      std::cout << "Found an array" << std::endl;

      for(ArrayIter iter(cur.m_data.parr); iter; ++iter) {
        searchQ.push(*iter.first().asTypedValue());
        searchQ.push(*iter.second().asTypedValue());
      }
    }



    searchQ.pop();
  }
}


void MarkSweepCollector::markReachable(void *ptr) {
  marked.insert(ptr);
}

bool MarkSweepCollector::isReachable(void *ptr) {
  return (marked.find(ptr) != marked.end());
}

///////////////////////////////////////////////////////////////////////////////

}
