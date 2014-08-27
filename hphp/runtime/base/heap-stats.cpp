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
  TRACE(2, "\n\nNumber of objects\n");
  TRACE(2, "String,Array,Object,Resource,Ref\n");
  for (auto entry : logs) {
    FTRACE(2, "{},{},{},{},{}\n", entry.counts[6],
        entry.counts[7], entry.counts[8], entry.counts[9], entry.counts[10]);
  }

  TRACE(2, "\nLive bytes\n");
  TRACE(2, "String header,String data,Array header,Array data,Object header,Object data\n");
  for (auto entry : logs) {
    auto str_header = entry.counts[6] * sizeof(StringData);
    auto arr_header = entry.counts[7] * sizeof(ArrayData);
    auto obj_header = entry.counts[8] * sizeof(ObjectData);
    FTRACE(2, "{},{},{},{},{},{}\n",
        str_header,
        entry.bytes[6] - str_header,
        arr_header,
        entry.bytes[7] - arr_header,
        obj_header,
        entry.bytes[8] - obj_header
        );

  }
  logs.clear();
}

}
