#include "hphp/runtime/base/heap-stats.h"
#include "hphp/util/trace.h"

namespace HPHP {

TRACE_SET_MOD(heapstats);

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

inline int getDataTypeIndexNoKill(DataType type) {
  switch (type) {
    case KindOfUninit       : return 0;
    case KindOfNull         : return 1;
    case KindOfBoolean      : return 2;
    case KindOfInt64        : return 3;
    case KindOfDouble       : return 4;
    case KindOfStaticString : return 5;
    case KindOfString       : return 6;
    case KindOfArray        : return 7;
    case KindOfObject       : return 8;
    case KindOfResource     : return 9;
    case KindOfRef          : return 10;
    case KindOfNamedLocal   : return 11;
    default                 : return -1;
  }
}

void HeapStats::trace_heap_stats() {
  HeapStatsEntry entry;

  //Initialise the arrays
  for(int i = 0; i < MaxNumDataTypes; i++) {
    entry.counts[i] = 0;
    entry.bytes[i] = 0;
  }

  tracer.traceHeap([&] (SearchNode n) {
      auto index = getDataTypeIndexNoKill(n.current.m_type);
      if (index >= 0) {
        entry.counts[index] += 1;
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
  FTRACE(2, "\n\nURL: {}\n", g_context->getRequestUrl());
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
