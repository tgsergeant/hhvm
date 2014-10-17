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

  prepareSlabData();

  for(auto thing : slabLookup) {
    FTRACE(3, "{} -> {}\n", (void *)thing.first, (void *)m_slabs[thing.second].slab.base);
  }

  tracer.traceHeap([&] (const SearchNode n) {
      markReachable(n.current.m_data.pstr);
      });

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

void MarkSweepCollector::printStack() {
  JIT::VMRegAnchor _;
  const ActRec* const fp = g_context->getFP();
  int offset = (fp->m_func->unit() != nullptr)
               ? fp->unit()->offsetOf(g_context->getPC())
               : 0;
  TRACE(1, g_context->getStack().toString(fp, offset));
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

void MarkSweepCollector::markDestructable(ObjectData const *obj) {
  destructable.push_back(obj);
  FTRACE(3, "Marked {} as destructable\n", obj);
}

///////////////////////////////////////////////////////////////////////////////

}
