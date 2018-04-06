/*
 * Copyright (c) 1997, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_VM_INTERPRETER_INTERPRETERRUNTIME_HPP
#define SHARE_VM_INTERPRETER_INTERPRETERRUNTIME_HPP

#include "interpreter/bytecode.hpp"
#include "interpreter/linkResolver.hpp"
#include "memory/universe.hpp"
#include "oops/method.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/signature.hpp"
#include "runtime/thread.hpp"
#include "utilities/macros.hpp"

// The InterpreterRuntime is called by the interpreter for everything
// that cannot/should not be dealt with in assembly and needs C support.

class InterpreterRuntime: AllStatic {
  friend class BytecodeClosure; // for method and bcp
  friend class PrintingClosure; // for method and bcp

 private:
  // Helper class to access current interpreter state
  class LastFrameAccessor : public StackObj {
    frame _last_frame;
  public:
    LastFrameAccessor(JavaThread* thread) {
      assert(thread == Thread::current(), "sanity");
      _last_frame = thread->last_frame();
    }
    bool is_interpreted_frame() const              { return _last_frame.is_interpreted_frame(); }
    Method*   method() const                       { return _last_frame.interpreter_frame_method(); }
    address   bcp() const                          { return _last_frame.interpreter_frame_bcp(); }
    int       bci() const                          { return _last_frame.interpreter_frame_bci(); }
    address   mdp() const                          { return _last_frame.interpreter_frame_mdp(); }

    void      set_bcp(address bcp)                 { _last_frame.interpreter_frame_set_bcp(bcp); }
    void      set_mdp(address dp)                  { _last_frame.interpreter_frame_set_mdp(dp); }

    // pass method to avoid calling unsafe bcp_to_method (partial fix 4926272)
    Bytecodes::Code code() const                   { return Bytecodes::code_at(method(), bcp()); }

    Bytecode  bytecode() const                     { return Bytecode(method(), bcp()); }
    int get_index_u1(Bytecodes::Code bc) const     { return bytecode().get_index_u1(bc); }
    int get_index_u2(Bytecodes::Code bc) const     { return bytecode().get_index_u2(bc); }
    int get_index_u2_cpcache(Bytecodes::Code bc) const
                                                   { return bytecode().get_index_u2_cpcache(bc); }
    int get_index_u4(Bytecodes::Code bc) const     { return bytecode().get_index_u4(bc); }
    int number_of_dimensions() const               { return bcp()[3]; }
    ConstantPoolCacheEntry* cache_entry_at(int i) const
                                                   { return method()->constants()->cache()->entry_at(i); }
    ConstantPoolCacheEntry* cache_entry() const    { return cache_entry_at(Bytes::get_native_u2(bcp() + 1)); }

    oop callee_receiver(Symbol* signature) {
      return _last_frame.interpreter_callee_receiver(signature);
    }
    BasicObjectLock* monitor_begin() const {
      return _last_frame.interpreter_frame_monitor_begin();
    }
    BasicObjectLock* monitor_end() const {
      return _last_frame.interpreter_frame_monitor_end();
    }
    BasicObjectLock* next_monitor(BasicObjectLock* current) const {
      return _last_frame.next_monitor_in_interpreter_frame(current);
    }

    frame& get_frame()                             { return _last_frame; }
  };

  static void      set_bcp_and_mdp(address bcp, JavaThread*thread);
  static void      note_trap_inner(JavaThread* thread, int reason,
                                   const methodHandle& trap_method, int trap_bci, TRAPS);
  static void      note_trap(JavaThread *thread, int reason, TRAPS);
#ifdef CC_INTERP
  // Profile traps in C++ interpreter.
  static void      note_trap(JavaThread* thread, int reason, Method *method, int trap_bci);
#endif // CC_INTERP

  // Inner work method for Interpreter's frequency counter overflow.
  static nmethod* frequency_counter_overflow_inner(JavaThread* thread, address branch_bcp);

 public:
  // Constants
  static void    ldc           (JavaThread* thread, bool wide);
  static void    resolve_ldc   (JavaThread* thread, Bytecodes::Code bytecode);

  // Allocation
  static void    _new          (JavaThread* thread, ConstantPool* pool, int index);
  static void    newarray      (JavaThread* thread, BasicType type, jint size);
  static void    anewarray     (JavaThread* thread, ConstantPool* pool, int index, jint size);
  static void    multianewarray(JavaThread* thread, jint* first_size_address);
  static void    register_finalizer(JavaThread* thread, oopDesc* obj);
  static void    defaultvalue  (JavaThread* thread, ConstantPool* pool, int index);
  static int     withfield     (JavaThread* thread, ConstantPoolCache* cp_cache);
  static void    uninitialized_static_value_field(JavaThread* thread, oopDesc* mirror, int offset);
  static void    uninitialized_instance_value_field(JavaThread* thread, oopDesc* obj, int offset);
  static void    write_heap_copy (JavaThread* thread, oopDesc* value, int offset, oopDesc* rcv);
  static void    write_flattened_value(JavaThread* thread, oopDesc* value, int offset, oopDesc* rcv);
  static void    read_flattened_field(JavaThread* thread, oopDesc* value, int index, Klass* field_holder);

  // Value Buffers support
  static void    recycle_vtbuffer(void *alloc_ptr);
  static void    recycle_buffered_values(JavaThread* thread);
  static void    return_value(JavaThread* thread, oopDesc* obj);
  static void    return_value_step2(oopDesc* obj, void* alloc_ptr);
  static void    fix_frame_vt_alloc_ptr(JavaThread* thread);
  static void    value_heap_copy(JavaThread* thread, oopDesc* value);

  static void value_array_load(JavaThread* thread, arrayOopDesc* array, int index);
  static void value_array_store(JavaThread* thread, void* val, arrayOopDesc* array, int index);

  // Quicken instance-of and check-cast bytecodes
  static void    quicken_io_cc(JavaThread* thread);

  // Exceptions thrown by the interpreter
  static void    throw_AbstractMethodError(JavaThread* thread);
  static void    throw_IncompatibleClassChangeError(JavaThread* thread);
  static void    throw_StackOverflowError(JavaThread* thread);
  static void    throw_delayed_StackOverflowError(JavaThread* thread);
  static void    throw_ArrayIndexOutOfBoundsException(JavaThread* thread, char* name, jint index);
  static void    throw_ClassCastException(JavaThread* thread, oopDesc* obj);
  static void    create_exception(JavaThread* thread, char* name, char* message);
  static void    create_klass_exception(JavaThread* thread, char* name, oopDesc* obj);
  static address exception_handler_for_exception(JavaThread* thread, oopDesc* exception);
#if INCLUDE_JVMTI
  static void    member_name_arg_or_null(JavaThread* thread, address dmh, Method* m, address bcp);
#endif
  static void    throw_pending_exception(JavaThread* thread);

#ifdef CC_INTERP
  // Profile traps in C++ interpreter.
  static void    note_nullCheck_trap (JavaThread* thread, Method *method, int trap_bci);
  static void    note_div0Check_trap (JavaThread* thread, Method *method, int trap_bci);
  static void    note_rangeCheck_trap(JavaThread* thread, Method *method, int trap_bci);
  static void    note_classCheck_trap(JavaThread* thread, Method *method, int trap_bci);
  static void    note_arrayCheck_trap(JavaThread* thread, Method *method, int trap_bci);
  // A dummy for macros that shall not profile traps.
  static void    note_no_trap(JavaThread* thread, Method *method, int trap_bci) {}
#endif // CC_INTERP

  static void resolve_from_cache(JavaThread* thread, Bytecodes::Code bytecode);
 private:
  // Statics & fields
  static void resolve_get_put(JavaThread* thread, Bytecodes::Code bytecode);

  // Calls
  static void resolve_invoke(JavaThread* thread, Bytecodes::Code bytecode);
  static void resolve_invokehandle (JavaThread* thread);
  static void resolve_invokedynamic(JavaThread* thread);

 public:
  // Synchronization
  static void    monitorenter(JavaThread* thread, BasicObjectLock* elem);
  static void    monitorexit (JavaThread* thread, BasicObjectLock* elem);

  static void    throw_illegal_monitor_state_exception(JavaThread* thread);
  static void    new_illegal_monitor_state_exception(JavaThread* thread);

  // Breakpoints
  static void _breakpoint(JavaThread* thread, Method* method, address bcp);
  static Bytecodes::Code get_original_bytecode_at(JavaThread* thread, Method* method, address bcp);
  static void            set_original_bytecode_at(JavaThread* thread, Method* method, address bcp, Bytecodes::Code new_code);
  static bool is_breakpoint(JavaThread *thread) { return Bytecodes::code_or_bp_at(LastFrameAccessor(thread).bcp()) == Bytecodes::_breakpoint; }

  // Safepoints
  static void    at_safepoint(JavaThread* thread);

  // Debugger support
  static void post_field_access(JavaThread *thread, oopDesc* obj,
    ConstantPoolCacheEntry *cp_entry);
  static void post_field_modification(JavaThread *thread, oopDesc* obj,
    ConstantPoolCacheEntry *cp_entry, jvalue *value);
  static void post_method_entry(JavaThread *thread);
  static void post_method_exit (JavaThread *thread);
  static int  interpreter_contains(address pc);

  // Native signature handlers
  static void prepare_native_call(JavaThread* thread, Method* method);
  static address slow_signature_handler(JavaThread* thread,
                                        Method* method,
                                        intptr_t* from, intptr_t* to);

#if defined(IA32) || defined(AMD64) || defined(ARM)
  // Popframe support (only needed on x86, AMD64 and ARM)
  static void popframe_move_outgoing_args(JavaThread* thread, void* src_address, void* dest_address);
#endif

  // bytecode tracing is only used by the TraceBytecodes
  static intptr_t trace_bytecode(JavaThread* thread, intptr_t preserve_this_value, intptr_t tos, intptr_t tos2) PRODUCT_RETURN0;

  // Platform dependent stuff
#include CPU_HEADER(interpreterRT)

  // optional normalization of fingerprints to reduce the number of adapters
  static uint64_t normalize_fast_native_fingerprint(uint64_t fingerprint);

  // Interpreter's frequency counter overflow
  static nmethod* frequency_counter_overflow(JavaThread* thread, address branch_bcp);

  // Interpreter profiling support
  static jint    bcp_to_di(Method* method, address cur_bcp);
  static void    profile_method(JavaThread* thread);
  static void    update_mdp_for_ret(JavaThread* thread, int bci);
#ifdef ASSERT
  static void    verify_mdp(Method* method, address bcp, address mdp);
#endif // ASSERT
  static MethodCounters* build_method_counters(JavaThread* thread, Method* m);
};


class SignatureHandlerLibrary: public AllStatic {
 public:
  enum { buffer_size =  1*K }; // the size of the temporary code buffer
  enum { blob_size   = 32*K }; // the size of a handler code blob.

 private:
  static BufferBlob*              _handler_blob; // the current buffer blob containing the generated handlers
  static address                  _handler;      // next available address within _handler_blob;
  static GrowableArray<uint64_t>* _fingerprints; // the fingerprint collection
  static GrowableArray<address>*  _handlers;     // the corresponding handlers
  static address                  _buffer;       // the temporary code buffer

  static address set_handler_blob();
  static void initialize();
  static address set_handler(CodeBuffer* buffer);
  static void pd_set_handler(address handler);

 public:
  static void add(const methodHandle& method);
  static void add(uint64_t fingerprint, address handler);
};

#endif // SHARE_VM_INTERPRETER_INTERPRETERRUNTIME_HPP
