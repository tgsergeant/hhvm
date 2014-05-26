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

#include "hphp/runtime/base/variable-serializer.h"

namespace HPHP {

///////////////////////////////////////////////////////////////////////////////


int64_t tracingGCCollect() {
  MarkSweepCollector msc;
  msc.markHeap();
  return 110;
}

#define cereal(x) std::cout << s.serialize(x, true).c_str() << std::endl

void MarkSweepCollector::markHeap() {
  Stack& stack = g_context->getStack();
  TypedValue *current = (TypedValue *)stack.getStackHighAddress();

  std::queue<TypedValue *> searchQ;

  while (current != (TypedValue *)stack.getStackLowAddress()) {
    searchQ.push(current);
    current --;
  }

  std::cout << "Found " << searchQ.size() << " roots" << std::endl;

  TypedValue *cur;
  VariableSerializer s(VariableSerializer::Type::VarDump);

  while (!searchQ.empty()) {
    cur = searchQ.front();

    if(cur->m_type == DataType::KindOfArray) {
      std::cout << "Found an array" << std::endl;
      cereal(Variant(cur->m_data.parr));

      for(ArrayIter iter(cur->m_data.parr); iter; ++iter) {
        std::cout << "---" << std::endl;
        cereal(iter.first());
        cereal(iter.second());
        TypedValue *second = iter.second().asTypedValue();
        searchQ.push(iter.first().asTypedValue());
        searchQ.push(second);
      }
    }

    if(cur->m_type == DataType::KindOfRef) {
      std::cout << "Found a ref" << std::endl;
      searchQ.push(cur->m_data.pref->tv());
      markReachable(cur);
    }

    if(cur->m_type == DataType::KindOfObject) {
      std::cout << "Found an object" << std::endl;
      //Difficult case =/
    }

    if(cur->m_type == DataType::KindOfStaticString) {
      std::cout << "Found a static string" << std::endl;
      markReachable(cur);
    }

    if(cur->m_type == DataType::KindOfString) {
      std::cout << "Found a string" << std::endl;
      markReachable(cur);
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
