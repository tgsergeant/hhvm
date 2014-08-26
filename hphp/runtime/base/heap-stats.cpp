#include "hphp/runtime/base/heap-stats.h"
#include "hphp/util/trace.h"

namespace HPHP {

TRACE_SET_MOD(tmp3);

HeapStats::TlsWrapper thing;

void trace_heap_stats() {
  threadHeapStats().trace_heap_stats();
}

void heap_stats_request_end() {
  threadHeapStats().request_end();
}

HeapStats& threadHeapStats() {
  return *HeapStats::TlsWrapper::getCheck();
}


void HeapStats::trace_heap_stats() {
  HeapStatsEntry entry;

  //Initialise the arrays
  for(int i = 0; i < MaxNumDataTypes; i++) {
    entry.counts[i] = 0;
    entry.bytes[i] = 0;
  }

  tracer.traceHeap([&] (SearchNode n) {
      if (0 <= n.current.m_type && n.current.m_type < MaxNumDataTypes) {
        entry.counts[getDataTypeIndex(n.current.m_type)] += 1;
        auto index = getDataTypeIndex(n.current.m_type);
        if (n.current.m_type == KindOfString) {
          entry.bytes[index] += sizeof(StringData) + n.current.m_data.pstr->capacity();
        }
        else if (n.current.m_type == KindOfObject) {
          entry.bytes[index] 
            += ObjectData::sizeForNProps(n.current.m_data.pobj->getVMClass()->numDeclProperties());
        }
        else if (n.current.m_type == KindOfRef) {
          entry.bytes[index] += sizeof(RefData);
        } else if (n.current.m_type == KindOfResource) {
          entry.bytes[index] += sizeof(ResourceData);
        }
        else if (n.current.m_type == KindOfArray) {
          auto ad = n.current.m_data.parr;
          long bytes = 0;
          if (ad->kind() == ArrayData::kMixedKind) {
            bytes = MixedArray::MixedSize(ad);
          } else if (ad->kind() == ArrayData::kPackedKind) {
            bytes = PackedArray::PackedSize(ad);
          }
          entry.bytes[index] += bytes;
        }
      }
  });

  logs.push_back(entry);

}

void HeapStats::request_end() {
  TRACE(2, "Number of objects\n");
  TRACE(2, "String,Array,Object,Resource,Ref\n");
  for (auto entry : logs) {
    FTRACE(2, "{},{},{},{},{}\n", entry.counts[6],
        entry.counts[7], entry.counts[8], entry.counts[9], entry.counts[10]);
  }

  TRACE(2, "\nLive bytes\n");
  for (auto entry : logs) {
    FTRACE(2, "{},{},{},{},{}\n", entry.bytes[6],
        entry.bytes[7], entry.bytes[8], entry.bytes[9], entry.bytes[10]);
  }
}

}
