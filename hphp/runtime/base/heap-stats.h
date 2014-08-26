#include "hphp/runtime/base/base-includes.h"
#include "hphp/util/thread-local.h"
#include "hphp/runtime/base/heap-tracer.h"

#include <vector>
#include <array>

#ifndef incl_HPHP_HEAP_STATS_H_
#define incl_HPHP_HEAP_STATS_H_

namespace HPHP {

void trace_heap_stats();

void heap_stats_request_end();

struct HeapStats;

HeapStats& threadHeapStats();

struct HeapStatsEntry {
  std::array<int, DataType::MaxNumDataTypes> counts; 
  std::array<long, DataType::MaxNumDataTypes> bytes; 
};

struct HeapStats {
  typedef ThreadLocalSingleton<HeapStats> TlsWrapper;
  static void Create(void* storage) {
    new (storage) HeapStats();
  }
  static void Delete(HeapStats* stats) {
    stats->~HeapStats();
  }
  static void OnThreadExit(HeapStats* stats) {
    stats->~HeapStats();
  }

  void trace_heap_stats();

  void request_end();
  
  std::vector<HeapStatsEntry> logs;

  HeapTracer tracer;
};

}

#endif
