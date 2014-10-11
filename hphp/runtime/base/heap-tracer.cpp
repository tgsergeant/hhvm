#include "hphp/runtime/base/heap-tracer.h"
#include "hphp/runtime/vm/jit/translator-inline.h"

namespace HPHP {

TRACE_SET_MOD(tmp2);

StaticString s_globals("GLOBALS");

void p(std::queue<SearchNode> *q, SearchNode n) {
  q->push(n);
}

void HeapTracer::traceHeap(NodeHandleFunc nodeFun) {
  JIT::VMRegAnchor _;
  const ActRec* const fp = g_context->getFP();
  const TypedValue *const sp = (TypedValue *)g_context->getStack().top();
  int offset = (fp->m_func->unit() != nullptr)
               ? fp->unit()->offsetOf(g_context->getPC())
               : 0;
  //TRACE(1, g_context->getStack().toString(fp, offset));

  //Main source of roots
  traceStackFrame(fp, offset, sp);

  //Also use $GLOBALS
  auto globals = g_context->m_globalVarEnv->lookup(s_globals.get());
  if (globals != nullptr) {
    FTRACE(2, "Found globals\n");
    TypedValue nulltv;
    nulltv.m_type = DataType::KindOfUninit;
    SearchNode n = {*globals, nulltv};
    p(&m_searchQ, n);
  }

  FTRACE(2, "Found {} roots\n", m_searchQ.size());
  // Checks

  TypedValue cur;
  SearchNode n;

  while (!m_searchQ.empty()) {
    n = m_searchQ.front();
    cur = n.current;
    m_searchQ.pop();

    if (cur.m_type <= KindOfDouble) {
      continue; //Ignore primitive values.
    }

    if(isReachable(cur.m_data.pstr)) {
      continue;
    }

    markReachable(cur.m_data.pstr);

    //Handle this node
    nodeFun(n);

    //Find children, ma
    if(cur.m_type == DataType::KindOfArray) {
      TRACE(3, "Found array\n");
      if(cur.m_data.parr && cur.m_data.parr->kind() < ArrayData::ArrayKind::kNumKinds) {
        for(ArrayIter iter(cur.m_data.parr); iter; ++iter) {
          m_searchQ.push({*iter.first().asTypedValue(), cur});
          m_searchQ.push({*iter.second().asTypedValue(), cur});
        }
      } else {
        TRACE(3, "It was a fake\n");
      }
    }

    if(cur.m_type == DataType::KindOfRef) {
      TRACE(3, "Found a ref\n");
      m_searchQ.push({*cur.m_data.pref->tv(), cur});
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
        m_searchQ.push({*prop, cur});
      }

      //Then, if there are dynamic properties
      if (UNLIKELY(od->getAttribute(ObjectData::Attribute::HasDynPropArr))) {
        //The dynamic properties are implemented internally as an
        //array, so why not treat them in the same way?
        auto dynProps = od->dynPropArray();

        TypedValue dynTv;
        dynTv.m_data.parr = dynProps.get();
        dynTv.m_type = DataType::KindOfArray;
        m_searchQ.push({dynTv, cur});
      }
    }
  }

  TRACE(1, "Finished tracing\n");
  reset();
}


void HeapTracer::traceStackFrame(const ActRec *fp, int offset, const TypedValue *ftop) {
  const Func *func = fp->m_func;
  func->validate();
  std::string funcName(func->fullName()->data());

  TypedValue nulltv = {};
  nulltv.m_type = DataType::KindOfUninit;

  FTRACE(2, "Visiting: {} at offset {}\n", funcName.c_str(), fp->m_soff);


  if(fp->hasThis()) {
    TypedValue thisTv;
    thisTv.m_data.pobj = fp->getThis();
    thisTv.m_type = KindOfObject;
    p(&m_searchQ, {thisTv, nulltv});
  }

  const int numExtra = fp->numArgs() - fp->m_func->numNonVariadicParams();
  if (numExtra > 0) {
    for (int i = 0; i < numExtra; i++) {
      TypedValue *val = fp->getExtraArgs()->getExtraArg(i);
      if (val != nullptr) {
        p(&m_searchQ, {*val, nulltv});
        FTRACE(1, "Extra arg: {}\n", tname(val->m_type));
      }
    }
  }

  if (fp->hasVarEnv()) {
    auto table = fp->getVarEnv()->getTable();

    for (auto it = NameValueTable::Iterator(table);
         it.valid();
         it.next()) {
      auto tv = it.curVal();
      if(tv->m_type != KindOfNamedLocal) {
        FTRACE(1, "NVT elem: {} - {}\n", *it.curKey(), tname(tv->m_type));
        p(&m_searchQ, {*tv, nulltv});
      }
    }
  }

  //Check out the local variables
  TypedValue *tv = (TypedValue *)fp;
  tv --;
  if (func->numLocals() > 0) {
    int n = func->numLocals();
    for (int i = 0; i < n; i++) {
      p(&m_searchQ, {*tv, nulltv});
      tv --;
    }
  }

  //See if the stack holds any secrets
  const auto base = fp->resumed() ? Stack::resumableStackBase(fp)
                                  : Stack::frameStackBase(fp);
  if(ftop <= base) {
    visitStackElems(fp, ftop, offset,
        [&](const ActRec *ar) {
        std::string funcName(ar->m_func->fullName()->data());
        FTRACE(1, "HELP I FOUND AN ACTREC WAT DO: {}, {} at {}\n", ar, funcName.c_str(), ar->m_soff);
        //markStackFrame(ar,
        },
        [&](const TypedValue *tv) {
          FTRACE(2, "Found a typedvalue in the stack: {}\n", tv);
          p(&m_searchQ, {*tv, nulltv});
        }
    );
  }

  //Try and step down within the stack
  {
    Offset prevPc = 0;
    TypedValue *prevStackTop = nullptr;
    ActRec *prevFp = g_context->getPrevVMState(fp, &prevPc, &prevStackTop);
    if (prevFp != nullptr) {
      traceStackFrame(prevFp, prevPc, prevStackTop);
    }
  }
}

void HeapTracer::reset() {
  marked.clear();
}

void HeapTracer::markReachable(void *ptr) {
  marked.insert(ptr);
}

bool HeapTracer::isReachable(void *ptr) {
  return (marked.find(ptr) != marked.end());
}

}
