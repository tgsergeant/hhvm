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
#include "hphp/runtime/base/apc-array.h"

#include "folly/Bits.h"

#include "hphp/runtime/base/apc-handle.h"
#include "hphp/runtime/base/apc-handle-defs.h"
#include "hphp/runtime/base/apc-typed-value.h"
#include "hphp/runtime/base/apc-string.h"
#include "hphp/runtime/base/apc-local-array.h"
#include "hphp/runtime/base/apc-local-array-defs.h"
#include "hphp/runtime/base/array-iterator.h"
#include "hphp/runtime/base/mixed-array-defs.h"
#include "hphp/runtime/ext/ext_apc.h"

namespace HPHP {

///////////////////////////////////////////////////////////////////////////////

namespace {
inline
bool chekVisited(PointerSet& visited, void* pvar) {
  if (visited.find(pvar) != visited.end()) {
    return true;
  }
  visited.insert(pvar);
  return false;
}
}

APCHandle* APCArray::MakeShared(ArrayData* arr,
                                size_t& size,
                                bool inner,
                                bool unserializeObj,
                                bool createIfSer) {
  bool serialize = false;
  bool uncounted = true;
  if (apcExtension::OptimizeSerialization) {
    if (!inner) {
      // at the top level need to check for circular references and
      // uncounted arrays.
      // If there is "circularity" we serialize and so we never re-enter.
      // If it is uncounted we also never re-enter as the creation of
      // uncounted arrays never creates APC objects.
      // If we are *not* uncounted though it may still be the case that a
      // nested array can be uncounted (thus the 'else' part)
      PointerSet visited;
      visited.insert(reinterpret_cast<void*>(arr));
      traverseDataRecursive(arr,
          [&](const Variant& var) {
            if (var.isReferenced()) {
              Variant *pvar = var.getRefData();
              if (chekVisited(visited, pvar)) {
                serialize = true;
                uncounted = false;
                return true;
              }
            }

            DataType type = var.getType();
            if (type == KindOfObject) {
              uncounted = false;
              auto data = var.getObjectData();
              if (chekVisited(visited, reinterpret_cast<void*>(data))) {
                serialize = true;
                return true;
              }
            } else if (type == KindOfResource) {
              serialize = true;
              uncounted = false;
              return true;
            }
            return false;
          }
      );
    } else {
      // if it is *not* an outermost array it means that the
      // container did not have any circular references neither was a
      // serializable object.
      // It also means the container cannot be an uncounted array.
      // We can still be an uncounted inner array though
      traverseDataRecursive(arr,
          [&](const Variant& var) {
            DataType type = var.getType();
            if (type == KindOfObject || type == KindOfResource) {
              uncounted = false;
              return true;
            }
            return false;
          }
      );
    }
    uncounted = uncounted && !arr->empty();
  } else {
    uncounted = false;
    if (!inner) {
      // only need to call traverseData() on the toplevel array
      DataWalker walker(DataWalker::LookupFeature::HasObjectOrResource);
      DataWalker::DataFeature features = walker.traverseData(arr);
      serialize = features.isCircular() || features.hasCollection();
      uncounted = !features.hasObjectOrResource() && !arr->empty();
    }
  }

  if (serialize) {
    // collection call into this to see if they can get some optimized
    // array form (uncounted or apc). When that is not the case there
    // is no point to serialize because the collection itself will
    // serialize
    if (!createIfSer) return nullptr;
    String s = apc_serialize(arr);
    APCHandle* handle = APCString::MakeShared(KindOfArray, s.get(), size);
    handle->setSerializedArray();
    handle->mustCache();
    return handle;
  }

  if (uncounted && apcExtension::UseUncounted) {
    size = getMemSize(arr) + sizeof(APCTypedValue);
    return APCTypedValue::MakeSharedArray(arr);
  }

  if (arr->isVectorData()) {
    return APCArray::MakePackedShared(arr, size, unserializeObj);
  }

  return APCArray::MakeShared(arr, size, unserializeObj);
}

APCHandle* APCArray::MakeShared() {
  void* p = malloc(sizeof(APCArray));
  APCArray* arr = new (p) APCArray(static_cast<size_t>(0));
  return arr->getHandle();
}

APCHandle* APCArray::MakeShared(ArrayData* arr,
                                size_t& size,
                                bool unserializeObj) {
  auto num = arr->size();
  auto cap = num > 2 ? folly::nextPowTwo(num) : 2;

  size = sizeof(APCArray) + sizeof(int) * cap + sizeof(Bucket) * num;
  void* p = malloc(size);
  APCArray* ret = new (p) APCArray(static_cast<unsigned int>(cap));

  for (int i = 0; i < cap; i++) ret->hash()[i] = -1;

  try {
    for (ArrayIter it(arr); !it.end(); it.next()) {
      size_t s = 0;
      auto key = APCHandle::Create(it.first(), s, false, true,
                                   unserializeObj);
      size += s;
      s = 0;
      auto val = APCHandle::Create(it.secondRef(), s, false, true,
                                   unserializeObj);
      size += s;
      if (val->shouldCache()) {
        ret->mustCache();
      }
      ret->add(key, val);
    }
  } catch (...) {
    delete ret;
    throw;
  }

  return ret->getHandle();
}

APCHandle* APCArray::MakePackedShared(ArrayData* arr,
                                      size_t& size,
                                      bool unserializeObj) {
  size_t num_elems = arr->size();
  size = sizeof(APCArray) + sizeof(APCHandle*) * num_elems;
  void* p = malloc(size);
  auto ret = new (p) APCArray(static_cast<size_t>(num_elems));

  try {
    size_t i = 0;
    for (ArrayIter it(arr); !it.end(); it.next()) {
      size_t s = 0;
      APCHandle* val = APCHandle::Create(it.secondRef(),
                                         s, false, true,
                                         unserializeObj);
      size += s;
      if (val->shouldCache()) {
        ret->mustCache();
      }
      ret->vals()[i++] = val;
    }
    assert(i == num_elems);
  } catch (...) {
    delete ret;
    throw;
  }

  return ret->getHandle();
}

Variant APCArray::MakeArray(APCHandle* handle) {
  if (handle->isUncounted()) {
    return APCTypedValue::fromHandle(handle)->getArrayData();
  } else if (handle->isSerializedArray()) {
    StringData* serArr = APCString::fromHandle(handle)->getStringData();
    return apc_unserialize(serArr->data(), serArr->size());
  }
  return APCLocalArray::Make(APCArray::fromHandle(handle))->asArrayData();
}

void APCArray::Delete(APCHandle* handle) {
  assert(!handle->isUncounted());
  handle->isSerializedArray() ? delete APCString::fromHandle(handle)
                              : delete APCArray::fromHandle(handle);
}

APCArray::~APCArray() {
  if (isPacked()) {
    APCHandle** v = vals();
    for (size_t i = 0, n = m_size; i < n; i++) {
      v[i]->unreferenceRoot(0);
    }
  } else {
    Bucket* bks = buckets();
    for (int i = 0; i < m.m_num; i++) {
      bks[i].key->unreferenceRoot(0);
      bks[i].val->unreferenceRoot(0);
    }
  }
}

void APCArray::add(APCHandle *key, APCHandle *val) {
  int pos = m.m_num;
  // NOTE: no check on duplication because we assume the original array has no
  // duplication
  Bucket* bucket = buckets() + pos;
  bucket->key = key;
  bucket->val = val;
  m.m_num++;
  int hash_pos;
  if (!IS_REFCOUNTED_TYPE(key->getType())) {
    APCTypedValue *k = APCTypedValue::fromHandle(key);
    hash_pos = (key->is(KindOfInt64) ?
        k->getInt64() : k->getStringData()->hash()) & m.m_capacity_mask;
  } else {
    assert(key->is(KindOfString));
    StringData* elKey;
    if (key->isUncounted()) {
      auto* k = APCTypedValue::fromHandle(key);
      elKey = k->getStringData();
    } else {
      auto* k = APCString::fromHandle(key);
      elKey = k->getStringData();
    }
    hash_pos = elKey->hash() & m.m_capacity_mask;
  }

  int& hp = hash()[hash_pos];
  bucket->next = hp;
  hp = pos;
}

ssize_t APCArray::indexOf(const StringData* key) const {
  strhash_t h = key->hash();
  ssize_t bucket = hash()[h & m.m_capacity_mask];
  Bucket* b = buckets();
  while (bucket != -1) {
    if (!IS_REFCOUNTED_TYPE(b[bucket].key->getType())) {
      APCTypedValue *k = APCTypedValue::fromHandle(b[bucket].key);
      if (!b[bucket].key->is(KindOfInt64) &&
          key->same(k->getStringData())) {
        return bucket;
      }
    } else {
      assert(b[bucket].key->is(KindOfString));
      StringData* elKey;
      if (b[bucket].key->isUncounted()) {
        auto* k = APCTypedValue::fromHandle(b[bucket].key);
        elKey = k->getStringData();
      } else {
        auto* k = APCString::fromHandle(b[bucket].key);
        elKey = k->getStringData();
      }
      if (key->same(elKey)) {
        return bucket;
      }
    }
    bucket = b[bucket].next;
  }
  return -1;
}

ssize_t APCArray::indexOf(int64_t key) const {
  ssize_t bucket = hash()[key & m.m_capacity_mask];
  Bucket* b = buckets();
  while (bucket != -1) {
    if (b[bucket].key->is(KindOfInt64) &&
        key == APCTypedValue::fromHandle(b[bucket].key)->getInt64()) {
      return bucket;
    }
    bucket = b[bucket].next;
  }
  return -1;
}

///////////////////////////////////////////////////////////////////////////////
}
