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
  return msc.markHeap();
}

int64_t MarkSweepCollector::markHeap() {
  Stack& stack = g_context->getStack();
  TypedValue *current = (TypedValue *)stack.getStackHighAddress();

  std::queue<TypedValue> searchQ;

  while (current != (TypedValue *)stack.getStackLowAddress()) {
    searchQ.push(*current);
    current --;
  }

  TypedValue cur;
  VariableSerializer s(VariableSerializer::Type::VarDump);

  while (!searchQ.empty()) {
    cur = searchQ.front();
    searchQ.pop();

    if(isReachable(cur.m_data.pstr)) {
      continue;
    }

    if(cur.m_type == DataType::KindOfArray) {
      for(ArrayIter iter(cur.m_data.parr); iter; ++iter) {
        searchQ.push(*iter.first().asTypedValue());
        searchQ.push(*iter.second().asTypedValue());

        markReachable(cur.m_data.parr);
      }
    }

    if(cur.m_type == DataType::KindOfRef) {
      searchQ.push(*cur.m_data.pref->tv());
      markReachable(cur.m_data.pref);
    }

    if(cur.m_type == DataType::KindOfObject) {
      ObjectData *od = cur.m_data.pobj;

      auto const cls = od->getVMClass();
      const TypedValue *props = od->propVec();
      auto propCount = cls->numDeclProperties();

      //First go through all of the properties which are defined by the class
      for(auto i = 0; i < propCount; i++) {
        const TypedValue *prop = props + i;
        searchQ.push(*prop);
      }

      //Then, if there are dynamic properties
      if (UNLIKELY(od->getAttribute(ObjectData::Attribute::HasDynPropArr))) {
        //The dynamic properties are implemented internally as an
        //array, so why not treat them in the same way?
        auto dynProps = od->dynPropArray();

        TypedValue dynTv;
        dynTv.m_data.parr = dynProps.get();
        dynTv.m_type = DataType::KindOfArray;
        searchQ.push(dynTv);

      }
      markReachable(od);
    }

    if(cur.m_type == DataType::KindOfStaticString || cur.m_type == DataType::KindOfString) {
      markReachable(cur.m_data.pstr);
    }

    if(cur.m_type == DataType::KindOfResource) {
      std::cout << "Found a resource" << std::endl;
      std::cout << "Not currently implemented" << std::endl;
      markReachable(cur.m_data.pres);
    }
  }
  return marked.size();
}


void MarkSweepCollector::markReachable(void *ptr) {
  marked.insert(ptr);
}

bool MarkSweepCollector::isReachable(void *ptr) {
  return (marked.find(ptr) != marked.end());
}

///////////////////////////////////////////////////////////////////////////////

}
