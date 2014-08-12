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

#include <iostream>
#include <string>

#include "hphp/runtime/base/tracing-collector-impl.h"

#include "hphp/runtime/base/variable-serializer.h"
#include "hphp/runtime/base/thread-info.h"
#include "hphp/runtime/vm/jit/translator-inline.h"

#include "hphp/util/trace.h"


namespace HPHP {

///////////////////////////////////////////////////////////////////////////////

TRACE_SET_MOD(tmp1);

// External API (tracing-collector.h)

int64_t tracingGCCollect() {
  return gc().collect();
}

void markDestructableObject(ObjectData const *obj) {
  gc().markDestructable(obj);
}

void requestGC() {
  ThreadInfo *info = ThreadInfo::s_threadInfo.getNoCheck();
  info->m_reqInjectionData.setGarbageCollectionFlag();
}


//Thread local singleton stuff

MarkSweepCollector::TlsWrapper tls;

MarkSweepCollector &gc() {
  return *MarkSweepCollector::TlsWrapper::getCheck();
}

void MarkSweepCollector::Create(void *storage) {
  new (storage) MarkSweepCollector();
}

void MarkSweepCollector::Delete(MarkSweepCollector *msc) {
  msc->~MarkSweepCollector();
}

void MarkSweepCollector::OnThreadExit(MarkSweepCollector *msc) {}


//Actual implementation

int64_t MarkSweepCollector::collect() {
  JIT::VMRegAnchor _;

  prepareSlabData();

  for(auto thing : slabLookup) {
    FTRACE(3, "{} -> {}\n", (void *)thing.first, (void *)m_slabs[thing.second].slab.base);
  }

  auto ret = markHeap();

  int reclaimed = 0;

  for(auto sd : m_slabs) {
    FTRACE(1, "Used: {}\n{}\n\n", (void *)sd.slab.base, sd.usedBlocks.to_string());
    //FTRACE(1, "Allocated: {}\n{}\n\n", (void *)sd.slab.base, sd.slab.allocatedBlocks.to_string());

    auto blocksToFree = ~ sd.usedBlocks & sd.slab.allocatedBlocks;

    //FTRACE(1, "To free:\n{}\n\n", blocksToFree.to_string());

    //Should remain disabled until we know that gc_enabled objects are not being collected elsewhere
    //(Otherwise they could end up on a freelist as well as in the block allocator)
    //MM().recycleMemory(sd.slab, blocksToFree);
    reclaimed += blocksToFree.count();
  }

  FTRACE(1, "Reclaimed {} blocks ({} kb)\n", reclaimed, reclaimed * 4);

  //Clear out the intermediate data we used
  cleanData();

  return 0;
}

void MarkSweepCollector::markStackFrame(const ActRec *fp, int offset, const TypedValue *ftop) {
  const Func *func = fp->m_func;
  func->validate();
  std::string funcName(func->fullName()->data());

  FTRACE(2, "Visiting: {} at offset {}\n", funcName.c_str(), fp->m_soff);


  if(fp->hasThis()) {
    TypedValue thisTv;
    thisTv.m_data.pobj = fp->getThis();
    thisTv.m_type = KindOfObject;
    m_searchQ.push(thisTv);
  }

  //Check out the local variables
  TypedValue *tv = (TypedValue *)fp;
  tv --;
  if (func->numLocals() > 0) {
    int n = func->numLocals();
    for (int i = 0; i < n; i++) {
      m_searchQ.push(*tv);
      tv --;
    }
  }

  //See if the stack holds any secrets
  visitStackElems(fp, ftop, offset,
      [&](const ActRec *ar) {
        std::string funcName(ar->m_func->fullName()->data());
        FTRACE(1, "HELP I FOUND AN ACTREC WAT DO: {}, {} at {}\n", ar, funcName.c_str(), ar->m_soff);
        //markStackFrame(ar, 
      },
      [&](const TypedValue *tv) {
        FTRACE(2, "Found a typedvalue in the stack: {}\n", tv);
        m_searchQ.push(*tv);
      }
  );

  //Try and step down within the stack
  {
    Offset prevPc = 0;
    TypedValue *prevStackTop = nullptr;
    ActRec *prevFp = g_context->getPrevVMState(fp, &prevPc, &prevStackTop);
    if (prevFp != nullptr) {
      markStackFrame(prevFp, prevPc, prevStackTop);
    }
  }
}

void MarkSweepCollector::printStack() {
  JIT::VMRegAnchor _;
  const ActRec* const fp = g_context->getFP();
  int offset = (fp->m_func->unit() != nullptr)
               ? fp->unit()->offsetOf(g_context->getPC())
               : 0;
  TRACE(1, g_context->getStack().toString(fp, offset));
}

StaticString s_globals("GLOBALS");

int64_t MarkSweepCollector::markHeap() {
  const ActRec* const fp = g_context->getFP();
  const TypedValue *const sp = (TypedValue *)g_context->getStack().top();
  int offset = (fp->m_func->unit() != nullptr)
               ? fp->unit()->offsetOf(g_context->getPC())
               : 0;
  TRACE(1, g_context->getStack().toString(fp, offset));

  //Main source of roots
  markStackFrame(fp, offset, sp);

  //Also use $GLOBALS
  auto globals = g_context->m_globalVarEnv->lookup(s_globals.get());
  if (globals != nullptr) {
    FTRACE(2, "Found globals\n");
    m_searchQ.push(*globals);
  }

  FTRACE(2, "Found {} roots\n", m_searchQ.size());
  // Checks

  TypedValue cur;
  VariableSerializer s(VariableSerializer::Type::VarDump);

  while (!m_searchQ.empty()) {
    cur = m_searchQ.front();
    m_searchQ.pop();

    if(isReachable(cur.m_data.pstr)) {
      continue;
    }

    if(cur.m_type == DataType::KindOfArray) {
      TRACE(3, "Found array\n");
      if(cur.m_data.parr) {
        for(ArrayIter iter(cur.m_data.parr); iter; ++iter) {
          m_searchQ.push(*iter.first().asTypedValue());
          m_searchQ.push(*iter.second().asTypedValue());

          markReachable(cur.m_data.parr);
        }
      } else {
        TRACE(3, "It was a fake\n");
      }
    }

    if(cur.m_type == DataType::KindOfRef) {
      TRACE(3, "Found a ref\n");
      m_searchQ.push(*cur.m_data.pref->tv());
      markReachable(cur.m_data.pref);
    }

    if(cur.m_type == DataType::KindOfObject) {
      TRACE(3, "Found an object\n");
      ObjectData *od = cur.m_data.pobj;
      if(!od) {
        TRACE(3, "It was a fake\n");
        continue;
      }

      auto const cls = od->getVMClass();
      const TypedValue *props = od->propVec();
      auto propCount = cls->numDeclProperties();

      //First go through all of the properties which are defined by the class
      for(auto i = 0; i < propCount; i++) {
        const TypedValue *prop = props + i;
        m_searchQ.push(*prop);
      }

      //Then, if there are dynamic properties
      if (UNLIKELY(od->getAttribute(ObjectData::Attribute::HasDynPropArr))) {
        //The dynamic properties are implemented internally as an
        //array, so why not treat them in the same way?
        auto dynProps = od->dynPropArray();

        TypedValue dynTv;
        dynTv.m_data.parr = dynProps.get();
        dynTv.m_type = DataType::KindOfArray;
        m_searchQ.push(dynTv);

      }
      markReachable(od);
    }

    if(cur.m_type == DataType::KindOfStaticString || cur.m_type == DataType::KindOfString) {
      TRACE(3, "Found a string\n");
      markReachable(cur.m_data.pstr);
    }

    if(cur.m_type == DataType::KindOfResource) {
      TRACE(3, "Found a resource\n(Not currently implemented)\n");
      markReachable(cur.m_data.pres);
    }
  }
  return 111;
}

void MarkSweepCollector::prepareSlabData() {
  const std::vector<MemoryManager::Slab> activeSlabs = MM().getActiveSlabs(true);
  const auto alignmentFactor = MemoryManager::kSlabSize / MemoryManager::kSlabAlignment;

  for(auto slab : activeSlabs) {
    int i = m_slabs.size();
    MarkSweepCollector::SlabData sd;
    sd.slab = slab;
    m_slabs.push_back(sd);

    slabLookup[sd.slab.base] = i;
  }
}

void MarkSweepCollector::cleanData() {
  m_slabs.clear();
  slabLookup.clear();
  marked.clear();
}

MarkSweepCollector::SlabData *MarkSweepCollector::getSlabData(void *ptr) {
  void *aligned = MemoryManager::slabAlignedPtr(ptr);
  auto ret = slabLookup.find(aligned);
  if(ret == slabLookup.end()) {
    return NULL;
  }
  return m_slabs.data() + ret->second;
}

void MarkSweepCollector::markReachable(void *ptr) {
  marked.insert(ptr);
  MarkSweepCollector::SlabData *sd = getSlabData(ptr);
  if(sd) {
    size_t blockId = MemoryManager::getBlockId(sd->slab, ptr);
    FTRACE(3, "Marked {} as reachable (block {})\n", ptr, blockId);

    sd->usedBlocks.set(blockId);
  }
}

bool MarkSweepCollector::isBlockReachable(void *ptr) {
  MarkSweepCollector::SlabData *sd = getSlabData(ptr);
  if(sd) {
    size_t blockId = MemoryManager::getBlockId(sd->slab, ptr);
    return sd->usedBlocks[blockId];
  }
  return false;
}

bool MarkSweepCollector::isReachable(void *ptr) {
  return (marked.find(ptr) != marked.end());
}

void MarkSweepCollector::markDestructable(ObjectData const *obj) {
  destructable.push_back(obj);
  FTRACE(3, "Marked {} as destructable\n", obj);
}

///////////////////////////////////////////////////////////////////////////////

}
