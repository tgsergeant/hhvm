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
#ifndef incl_HPHP_HHIR_OPT_H_
#define incl_HPHP_HHIR_OPT_H_

#include "hphp/runtime/vm/jit/frame-state.h"
#include "hphp/runtime/vm/jit/types.h"

namespace HPHP {  namespace JIT {

//////////////////////////////////////////////////////////////////////

class IRBuilder;
class IRUnit;
class IRInstruction;

//////////////////////////////////////////////////////////////////////

/*
 * The main optimization passes, in the order they run.
 */
void optimizeRefcounts(IRUnit&, FrameState&&);
void optimizePredictions(IRUnit&);
void optimizeJumps(IRUnit&);
void eliminateDeadCode(IRUnit&);

/*
 * Run all the optimization passes.
 */
void optimize(IRUnit& unit, IRBuilder& builder, TransKind kind);

//////////////////////////////////////////////////////////////////////

}}

#endif
