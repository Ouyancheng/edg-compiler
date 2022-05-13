/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

interpret.c -- IL interpreter for constexpr functions

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "interpret.h"

#include "class_decl.h"

#include "exprutil.h"

#include "folding.h"

#include "func_def.h"

#if DO_IL_LOWERING
#include "lower_il.h"
#endif /* DO_IL_LOWERING */

#include "templates.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
This file implements an interpreter for a subset of the unlowered IL produced
by the C++ front end.  Specifically, the subset corresponds to the constructs
allowed in a constant expression in C++14.


The Interpreter
---------------
The interpreter itself traverses the IL in typical "recursive descent" fashion.
The principal entry points are interpret_expr, interpret_constexpr_call,
interpret_dynamic_init_full, and interpret_constexpr_ctor.  These set up an
"interpreter state" that is carried through the interpretation process (this
state includes local allocations and mappings, the call stack, diagnostic
records, etc.).

An interpreter invocation can end for one of three reasons:
  (1) the call is completed with a valid result (normal case),
  (2) interpretation runs into an invalid operation (e.g., an attempt to
      read an uninitialized value), or
  (3) the cost of the interpretation is too high.

Regarding the latter reason, the interpreter tracks the sum of twice the number
of calls and the number of loop-back branches.  When that number reaches a
certain large value, the interpretation is deemed too expensive.  Deeply nested
recursion is also limited.


Storage
-------
The interpreter manages several pools of storage:
  (1) automatic variables and temporaries;
  (2) static data storage;
  (3) permanent data associated with declarative entities
      (e.g., field offsets); and
  (4) maps (see below).

The first kind of storage is allocated/deallocated in strict last-in/first-out
(LIFO) manner, and so is efficiently implemented using a storage stack.  It is
implemented here through a structure of type a_storage_stack_state, which
maintains a linked list of large blocks from which small chunks are allocated
/deallocated (adding a large block whenever the last added one is full).  To
avoid excessive waste, larger chunks (which should be uncommon) are not
allocated from the large blocks, but separately (through the front end's
normal memory allocator).

Almost every variable and temporary triggers an allocation performed through
the macro alloc_stack_bytes.  Deallocation, on the other hand, is batched
"per scope" (for variables) or "per full expression" (for temporaries).  The
macros save_storage_stack and restore_storage_stack support this.

A wrinkle in this mechanism are temporaries whose lifetime is extended because
they are bound to a reference.  This is handled by starting a new storage stack
when the reference is encountered while recording the original stack in the
interpreter state (see an_interpreter_state::extension_state).  When the
temporary to be bound is encountered, it is allocated in the original stack,
which ensures it will live as long as the reference.

The second kind of storage (static data) is similar to the first in that it is
associated with a specific interpreter invocation, but it persists until the
end of that invocation.  This is used, e.g., for string literal storage.  It
also uses the a_storage_stack_state structure, but is only initialized if it
is actually needed during interpretation (see the fields static_storage_ready
and static_storage in an_interpreter_state).  See also alloc_static_bytes.

The third kind of storage persists across interpreter invocations.  It also
uses a storage stack (see the static variable persistent_data), although the
ability to efficiently deallocate is not exploited in that case.  The macro
alloc_bytes can be used for these allocations.

The fourth kind of storage is that managed by maps (see below).  This uses a
separate allocation strategy.


Mappings
--------
The interpreter makes use of an efficient hash table data structure that maps
pointers (the "key") to either pointers or byte counts (the "value").  The
type describing such a "map" is a_data_map.  A new (key, value) pair can be
added with macro map_ptr or map_byte_count.  A mapping can then be retrieved
with get_mapped_ptr or get_mapped_byte_count.  A recorded mapping for a given
key can be revoked with unmap_ptr.  (Most of the time, the key is a pointer
into IL.)

The interpreter state includes a data map for automatic variables and
temporaries; convenience macros map_stack_bytes, unmap_stack_bytes, and
get_stack_bytes can be used to manage that data map.  For example, if a
stmk_init statement entry is interpreted, storage is allocated for the variable
and the pointer to the associated a_variable entry is mapped to that storage
address.

This map tracking automatic variables and temporaries is also used to map
run-time namespace-scope variables to a_constant entries representing their
address (this is done by mapping a_variable::initializer).

If a pointer to be mapped may already be in a map, use map_or_replace_ptr
(or replace_mapped_ptr) instead of map_ptr.  It will replace the mapping
recorded in the data map.

Besides the data map associated with an interpreter state, another map is kept
that persists across interpreter invocations (static variable persistent_map).
This map, e.g., holds data layout information associated with types and fields
(the data layout for the interpreter is different from that for the target
architecture).


Object Layout
-------------
A complete object (i.e., an object that is not a subobject of another object)
stored in a storage stack consists of a "prefix" followed by the representation
of the value of that object.  The prefix has the following components in order:

  init_bits[n]
  init_bits[n-1]
  ...
  init_bits[0]
  object_flags
  type_ptr
  (data representation starts here)

Given a data pointer, traversing the prefix backwards provides the type of the
complete object (type_ptr), a byte (object_flags) that tracks some properties
of the complete object (see below), and, for class and array objects, bit sets
indicating whether a given offset in the data representation has been
initialized.  Note that only one bit is set per scalar entity.  For example,
if integers are represented using eight bytes and the leading entry of an
array is initialized, then bit 0 of init_bits[0] will be set, but bits 1
through 7 will remain unchanged even though their corresponding bytes have
valid values.  Currently, object_flags has two flags:
  0x01: Set when the complete object has been initialized (mainly used
        for non-aggregate objects).
  0x02: Set when the complete object has been dynamically allocated.
        In that case, the recorded type (type_ptr) of the complete object
        is actually the element type.

The representation of variables (including parameters) also has a "postfix"
block that includes (a) an allocation sequence number that describes its
lifetime and (b) a pointer to any storage previously associated with that
variable (for recursive calls).

Integer and floating-point values are stored using their IL representations 
(i.e., an_integer_value and an_internal_float_value).  Bit fields occupy a
whole integer value, but every "store" to a bit field is appropriately trimmed.

Class type objects and subobjects start with an IL pointer (described below),
followed by storage for the fields, storage for nonvirtual direct base classes,
and finally storage for virtual base classes (all in declaration order).
A derived-to-base class cast therefore always corresponds to a positive offset
of the "this" pointer, whereas a base-to-derived class cast involves negative
offset.

For objects of union type, the leading pointer points to the a_field
corresponding to the "active field" (attempting to read a non-active field
results in interpretation failure).  

The storage of a non-union class type ("class" or "struct") starts with a
pointer to an IL entry for the corresponding direct base class entry of the
next-derived subobject, or NULL for the most-derived subobject.  This is used
to catch invalid base-to-derived casts (or certain invalid accesses to a
derived-object member through a pointer-to-member value).

Pointers (and references, lvalues, and xvalues) are represented with type
a_constexpr_address.  Often, all that is needed is a pointer into interpreter
storage.  However, there are many variations:

  - pointers to array elements, for which bounds must be maintained;
  - pointers to items in unions, for which the access path (see type
    a_variant_path_entry) must be maintained so that it can be checked
    against active fields when the address is read from;
  - pointers to functions or entities not known at compile time;
  - etc.

In addition, when comparing pointers using, e.g., the '<' operator, the
interpreter must ensure that the pointers point to the same complete
object.  a_constexpr_address therefore includes a pointer to the complete
object it points to (assuming it is a pointer to interpreter storage).

When reading through a_constexpr_address, the front end must ensure that the
storage it points to is still valid.  To this end, every variable and temporary
allocated in the interpreter has an associated allocation sequence number, and
that number is recorded when an address for that entity is formed (for a
pointer, lvalue, or xvalue).  When an allocation sequence number is created, it
is also recorded in a hash table described by a_live_set (see field live_set in
an_interpreter_state); it is removed when the associated storage is reclaimed.
Every read through a_constexpr_address therefore just has to check that the
recorded allocation sequence number is still in the live table (if not,
interpretation fails).

The a_constexpr_address representation is sufficient to re-create the complete
"symbolic path" needed to obtain it from a complete object.  E.g., if the
address is in an object x, perhaps an expression like "x.y[3].z" is needed to
produce that address: The ".y[3].z" path can be reconstructed and this may be
needed when comparing pointers or when translating interpreter addresses back
to a_constant/ck_address entries.

Pointer-to-member values are represented by the a_constexpr_ptr_to_mem type.


Local Macro Names
-----------------
The implementation of the interpreter makes liberal use of macros.  In a few
cases, the macros are specific to a local function context and they make use
of local variables not passed through parameters.  Such local macros are named
with a capitalized leading word (a visual reminder that their effect is broader
than may seem at first).  See, e.g., SET_result_val_from_operand_address in
do_constexpr_expression.
*/


typedef unsigned int a_byte_count;

/*
Macro defining the largest allowed size of a type in the interpreter.
*/
#define MAX_CONSTEXPR_TYPE_SIZE ((a_byte_count)(1<<20))

/*
Macros that control the maximum length of an array.  The length of arrays is
sometimes stored in a bit field (see a_constexpr_address).  So, the width of
that field is defined here.
*/
#define ARRAY_LENGTH_WIDTH 24
#define MAX_ARRAY_LENGTH ((1<<ARRAY_LENGTH_WIDTH) - 1)

/*
Macro defining the maximum size in bytes of a block allocated to represent
a dynamic memory allocation.
*/
#define MAX_CONSTEXPR_DYN_ALLOC_SIZE ((a_byte_count)(1<<28))

typedef unsigned int an_alloc_seq_number;
			/* An integral type used to number allocations and
			   track whether an allocation is still "live" (via
			   a_live_set -- see below).  A zero value corresponds
			   to static storage. */

/*
Macro to set the flag indicating that interpretation has failed.  In DEBUG
configurations, a breakpoint on constexpr_fail_intercept is useful to find
where interpretation fails. 
*/
#if DEBUG && !defined(_lint)
#define do_constexpr_fail(flag) (constexpr_fail_intercept(), ((flag) = FALSE))

static void constexpr_fail_intercept(void)
/*
This function exists solely to intercept interpretation failure in a debugger.
*/
{
}  /* constexpr_fail_intercept */

#else /* !(DEBUG  && !defined(_lint)) */
#define do_constexpr_fail(flag) ((flag) = FALSE)
#endif /* DEBUG && !defined(_lint) */


/*
Macro defining the size of large blocks allocated for a storage stack.  These
large blocks are then parceled out in smaller chunks as requested through the
macro alloc_bytes.  If an alloc_bytes request is too large, however, the
storage is not carved from the large blocks; instead, it is allocated from
general memory.
*/
#define CONSTEXPR_STACK_BLOCK_SIZE (1<<16)

/*
Macro defining the largest chunk size to be carved from storage stack blocks
(see CONSTEXPR_STACK_BLOCK_SIZE above).  Invocations of alloc_bytes that
request larger chunks are handled in terms of individual calls to alloc_general
(and are freed by a call to free_general).
*/
#define CONSTEXPR_STACK_ALLOC_LIMIT (1<<10)

/*
The complete object flag values.
*/
#define COMPLETE_OBJ_INITIALIZED ((a_byte)0x01)
#define COMPLETE_OBJ_DYN_ALLOC   ((a_byte)0x02)


#define set_complete_obj_flag(obj, flag)                                     \
  (*((a_byte*)obj-sizeof(a_type_ptr)-1) |= flag)

#define clear_complete_obj_flag(obj, flag)                                   \
  (*((a_byte*)obj-sizeof(a_type_ptr)-1) &= ~flag)

#define complete_obj_flag(obj, flag)                                         \
  ((*((a_byte*)(obj)-sizeof(a_type_ptr)-1) & flag) != 0)


/*
Structure describing a destruction to be performed.  A storage stack state
points to a list of these.
*/
typedef struct a_constexpr_destruction {
  struct a_constexpr_destruction
		*next;
			/* Next destruction to perform, or NULL if this is the
			   last destruction on the list. */
  a_dynamic_init_ptr
		dip;
			/* An entry describing the destructor to execute. */
  a_byte	*sub_obj;
			/* Address of the subobject to destroy (could be a
			   complete object address). */
  a_byte	*complete_obj;
			/* Address of the complete object to destroy. */
  a_source_position
		*pos;
			/* Position for diagnostics. */
}  a_constexpr_destruction;


/*
Structure describing a dynamic allocation during constexpr evaluation.  These
are maintained on a list of "live" allocations (deallocation removes them from
the list).  This structure is placed at the top of a block that also includes
the allocated object.
*/
typedef struct a_constexpr_allocation *a_constexpr_allocation_ptr;
typedef struct a_constexpr_allocation {
  a_constexpr_allocation_ptr
		next, prev;
			/* Next and previous allocation on the "live" list. */
  a_type_ptr
		elem_type;
			/* The type of the elements being allocated. */
  a_source_position
		pos;	/* Position in the source code where the allocation
			   was requested. */
  an_alloc_seq_number
                alloc_seq_number;
			/* The sequence number associated with this
			   allocation. */
  a_byte_count
		total_size;
			/* The total size of this block (used when freeing
			   the block). */
  a_byte_count
		prefix_size;
			/* The distance between the start of the block (i.e.,
			   the address of this object) and the stored object
			   past its bookkeeping preamble. */
  a_byte_count
		length;
			/* The number of items allocated in the block. */
} a_constexpr_allocation;


/*
Structure describing a state of the allocation stack.  States can be saved to
be restored later (effectively "popping" the allocation stack).
*/
typedef struct a_storage_stack_state {
  a_byte	*top;
			/* A pointer to the next byte available for allocation
			   in the interpreter's storage stack. */
  a_byte	*curr_block;
			/* The current block in the interpreter's storage
			   stack.  The start of a block has (in order): the
			   previous block, the next block, and the top of the
			   stack when this block last became non-current. */
  a_byte	*large_blocks;
			/* A pointer to the last allocated large block (or NULL
			   if there is none). */
  an_alloc_seq_number
		alloc_seq_number;
			/* A sequence number used to detect leaks. */
  a_constexpr_destruction
		*destructions;
			/* Destructions to perform when this state is
			   popped. */
} a_storage_stack_state;


typedef struct a_large_block_header {
  a_byte	*prev_large_block;
			/* Pointer to the previously allocated large block (or
			   NULL if none). */
  a_byte_count	block_size;
			/* Size of this block. */
  an_alloc_seq_number
		alloc_seq_number;
			/* A sequence number used to manage deallocation. */
} a_large_block_header;


static a_storage_stack_state
		persistent_data;
			/* Data that persists across interpreter invocations.
			   In particular, data describing the layout of data
			   in the interpreter. */

/*
Type to use to index into a data map.
*/
typedef unsigned int a_map_index;


/*
Union for the kinds of values that a data map maps to.
*/
typedef union a_mapped_value {
  a_byte	*ptr;
			/* A pointer. */
  a_byte_count
		byte_count;
			/* A byte count. */
} a_mapped_value;


/*
Structure mapping pointers in the IL to associated data in the interpreter.
(E.g., to map a variable to its associated storage, or a type to its associated
layout data.)
*/
typedef struct a_data_map_entry {
  a_byte	*ptr;
			/* A pointer mapped by this entry.  (A "key" in the
			   hash table.) */
  a_mapped_value
		data;
			/* A value associated with ptr. */
} a_data_map_entry;


/*
Structure describing a data map.  (Implemented as a hash table with linear
probing.)
*/
typedef struct a_data_map {
  a_data_map_entry
		*table;
			/* Hash table mapping pointers into the IL onto
			   associated data. */
  a_map_index
		hash_mask;
			/* The mask to apply to the hash value before indexing
			   in the table.  This mask is increased as the table
			   grows. */
  a_map_index
		n_elements;
			/* The number of elements stored in the table. */
} a_data_map;


static a_data_map
		persistent_map;
			/* Map that persists across interpreter invocations.
			   In particular, its entries describe the layout of
			   data in the interpreter. */

#define MAX_WIDTH_REUSABLE_TABLE 10
static a_data_map_entry
		*free_map_tables[MAX_WIDTH_REUSABLE_TABLE+1];
			/* A array of pointers to map tables available for
			   reuse.  free_map_table[n] points to a list of tables
			   allocated for 1<<n entries.  Larger tables use
			   alloc_general and free_general. */


static void init_data_map(a_data_map    *map,
                          unsigned int  mask_width)
/*
Initialize the given data map.
*/
{
  unsigned      n_slots = (1<<mask_width);
  a_byte_count  size = (a_byte_count)(n_slots*sizeof(a_data_map_entry));

  if (mask_width > MAX_WIDTH_REUSABLE_TABLE) {
    map->table = (a_data_map_entry*)alloc_general(size);
  } else if (free_map_tables[mask_width] != NULL) {
    map->table = free_map_tables[mask_width];
    free_map_tables[mask_width] =
                          (a_data_map_entry*)free_map_tables[mask_width]->ptr;
  } else {
    map->table = (a_data_map_entry*)alloc_fe(size);
  }  /* if */
  memzero((char*)map->table, size_t_arg(size));
  map->hash_mask = n_slots-1;
  map->n_elements = 0;
}  /* init_data_map */


static void release_data_map_table(a_data_map  *map)
/*
Release the storage for the given map's table.
*/
{
  a_map_index       mask = map->hash_mask;
  a_map_index       n_slots = mask+1;
  a_byte_count      size = (a_byte_count)(n_slots*sizeof(a_data_map_entry));
  unsigned long     mask_width = count_ones(mask);

  if (mask_width > MAX_WIDTH_REUSABLE_TABLE) {
    free_general(map->table, size);
  } else {
    map->table[0].ptr = (a_byte*)free_map_tables[mask_width];
    free_map_tables[mask_width] = map->table;
  }  /* if */
}  /* release_data_map_table */


/*
Structure describing a call context or a GNU statement expression evaluation
context.
*/
typedef struct a_call_frame *a_call_frame_ptr;
typedef struct a_call_frame {
  a_call_frame_ptr
		parent;
			/* The frame in which this frame was created. */
  a_routine_ptr	routine;
			/* The routine being called or NULL for a GNU
			   statement expression. */
  union {
    /* When routine != NULL: */
    a_source_position
		*position;
			/* The source position of the call. */
#if GNU_EXTENSIONS_ALLOWED
    /* When routine == NULL: */
    an_expr_node_ptr
		expr;	/* The statement expression associated with this
			   frame. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } variant;
  a_byte	*result_storage;
			/* The storage in which returned expression results
			   should be placed. */
  a_byte	*complete_object;
			/* A pointer to the complete object in which
			   result_storage points. */
  a_bit_field	return_active:1;
			/* TRUE while backtracking from a return statement. */
  a_bit_field	loop_break_active:1;
			/* TRUE while backtracking from a break statement
			   that applies to a loop statement. */
  a_bit_field	continue_active:1;
			/* TRUE while backtracking from a continue
			   statement. */
  a_bit_field	switch_break_active:1;
			/* TRUE while backtracking from a break statement
			   that applies to a switch statement. */
} a_call_frame;



/*
Type to use to index into a live set.
*/
typedef unsigned int a_live_set_index;


/*
A hash table maintaining a set of "live" allocation sequence numbers (i.e., the
sequence numbers of allocations that have not been deallocated yet).  Unlike
most hash tables this table only holds "keys", not associated "values".
*/
typedef struct a_live_set {
  an_alloc_seq_number
		*table;
			/* The hash table proper. */
  a_live_set_index
		hash_mask;
			/* The mask to apply to the hash value before indexing
			   in the table.  This mask is increased as the table
			   grows. */
  a_live_set_index
		n_elements;
			/* The number of elements stored in the table. */
} a_live_set;

static an_alloc_seq_number
		*free_live_set_tables[MAX_WIDTH_REUSABLE_TABLE+1];
			/* A array of pointers to live set tables available for
			   reuse.  free_live_set_table[n] points to a list of
			   tables allocated for 1<<n entries.  Larger tables
			   use alloc_general and free_general.*/


static void init_live_set(a_live_set  *set)
/*
Initialize the given live set.
*/
{
  unsigned      mask_width = 3, n_slots = (1<<mask_width);
  a_byte_count  size = (a_byte_count)(n_slots*sizeof(an_alloc_seq_number));

  if (free_live_set_tables[mask_width] != NULL) {
    set->table = free_live_set_tables[mask_width];
    free_live_set_tables[mask_width] =
                     *(an_alloc_seq_number**)free_live_set_tables[mask_width];
  } else {
    set->table = (an_alloc_seq_number*)alloc_fe(size);
  }  /* if */
  memzero((char*)set->table, size_t_arg(size));
  set->hash_mask = n_slots-1;
  set->n_elements = 0;
}  /* init_live_set */


static void release_live_set_table(a_live_set  *set)
/*
Release the storage for the given set's table.
*/
{
  a_live_set_index  mask = set->hash_mask;
  a_live_set_index  n_slots = mask+1;
  a_byte_count      size = (a_byte_count)(n_slots*sizeof(an_alloc_seq_number));
  unsigned long     mask_width = count_ones(mask);

  if (mask_width > MAX_WIDTH_REUSABLE_TABLE) {
    free_general(set->table, size);
  } else {
    *(an_alloc_seq_number**)set->table = free_live_set_tables[mask_width];
    free_live_set_tables[mask_width] = set->table;
  }  /* if */
}  /* release_live_set_table */


#define hash_alloc_seq_number(seq)                                           \
  (seq)


static void expand_live_set(a_live_set  *set)
/*
Double the number of entries in the given set.  This requires rehashing.
*/
{
  an_alloc_seq_number  *new_table, *old_table = set->table;
  a_live_set_index     mask = set->hash_mask;
  a_live_set_index     k, n_slots = mask+1;
  a_byte_count         old_size = (a_byte_count)
                                         (n_slots*sizeof(an_alloc_seq_number));
  a_byte_count         new_size = 2*old_size;
  unsigned long        new_width = count_ones(mask)+1, old_width;

  if (new_width > MAX_WIDTH_REUSABLE_TABLE) {
    new_table = (an_alloc_seq_number*)alloc_general(new_size);
  } else if (free_live_set_tables[new_width] != NULL) {
    new_table = free_live_set_tables[new_width];
    free_live_set_tables[new_width] =
                      *(an_alloc_seq_number**)free_live_set_tables[new_width];
  } else {
    new_table = (an_alloc_seq_number*)alloc_fe(new_size);
  }  /* if */
  memzero((char*)new_table, size_t_arg(new_size));
  mask = mask*2+1;
  for (k = 0; k<n_slots; ++k) {
    an_alloc_seq_number  seq = old_table[k];
    if (seq != 0) {
      a_live_set_index  idx = hash_alloc_seq_number(seq) & mask;
      while (new_table[idx] != 0) {
        idx = (idx+1) & mask;
      }  /* while */
      new_table[idx] = old_table[k];
    }  /* if */
  }  /* for */
  set->table = new_table;
  set->hash_mask = mask;
  old_width = new_width-1;
  if (old_width > MAX_WIDTH_REUSABLE_TABLE) {
    free_general(old_table, old_size);
  } else {
    *(an_alloc_seq_number**)old_table = free_live_set_tables[old_width];
    free_live_set_tables[old_width] = old_table;
  }  /* if */
}  /* expand_live_set */


/*
Macro to add an allocation sequence number to a live set.  (It may not be in
the set already.)
*/
#define add_to_live_set(set, alloc_seq)                                      \
{                                                                            \
  uintptr_t            hash = hash_alloc_seq_number(alloc_seq);              \
  a_live_set_index     mask = (set)->hash_mask;                              \
  a_live_set_index     idx = hash & mask;                                    \
  an_alloc_seq_number  *table = (set)->table;                                \
  if (table[idx] == 0) {                                                     \
    table[idx] = (alloc_seq);                                                \
  } else {                                                                   \
    set_colliding_seq(set, alloc_seq, idx);                                  \
  }  /* if */                                                                \
  (set)->n_elements += 1;                                                    \
  if ((set)->n_elements*2 > mask) {                                          \
    expand_live_set(set);                                                    \
  }  /* if */                                                                \
}


static void set_colliding_seq(a_live_set           *set,
                              an_alloc_seq_number  seq,
                              a_live_set_index     idx)
/*
The given allocation sequence number collides with an existing entry in the
given set.  Move the existing entry to the next available spot and place seq
at idx.
*/
{
  a_live_set_index     mask = set->hash_mask;
  an_alloc_seq_number  *table = set->table;
  an_alloc_seq_number  saved_seq;

  /* Place the new number at idx, and move the existing entry to the next
     available spot. */
  saved_seq = table[idx];
  table[idx] = seq;
  for (;;) {
    idx = (idx+1) & mask;
    if (table[idx] == 0) {
      table[idx] = saved_seq;
      break;
    }  /* if */
  }  /* for */
}  /* set_colliding_seq */


#define remove_from_live_set(set, alloc_seq)                                 \
{                                                                            \
  uintptr_t            hash = hash_alloc_seq_number(alloc_seq);              \
  a_live_set_index     mask = (set)->hash_mask;                              \
  a_live_set_index     idx = hash & mask;                                    \
  an_alloc_seq_number  *table = (set)->table;                                \
  /* Find the item to delete (we're assuming it exists). */                  \
  while (table[idx] != (alloc_seq)) {                                        \
    idx = (idx+1) & mask;                                                    \
  }  /* while */                                                             \
  table[idx] = 0;                                                            \
  /* If the next slot is empty, we're done.  Otherwise, we may have to */    \
  /* move another element into the emptied slot. */                          \
  if (table[(idx+1) & mask] != 0) {                                          \
    check_deleted_live_set_slot(set, idx);                                   \
  }  /* if */                                                                \
  (set)->n_elements -= 1;                                                    \
}


static void check_deleted_live_set_slot(a_live_set        *set,
                                        a_live_set_index  idx0)
/*
Slot idx has been cleared in the given set (i.e., set->table[idx].ptr has been
set to zero).  The next slot is not empty.  There may therefore exist entries
that are associated with that slot (i.e., have the same hash index).  This
function makes sure that such entries can be found, by moving up entries as
needed.

This corresponds to Algorithm R in section 6.4 of volume 3 of Donald E. Knuth's
"The Art of Computer Programming" (Sorting and Searching -- Second Edition),
with the assumption that step R1 has already been performed (idx0 is "j") and
we know that the subsequent slot is not empty.
*/
{
  an_alloc_seq_number  *table = set->table;
  a_live_set_index     mask = set->hash_mask;
  a_live_set_index     idx, ridx;
  an_alloc_seq_number  rseq;
  
  idx = (idx0+1) & mask;
  rseq = table[idx];
  for (;;) {
    for (;;) {
      ridx = hash_alloc_seq_number(rseq) & mask;
      /* See if we can move the entry at idx to idx0.  ridx is its "ideal"
         slot: the place from where probing will start.  So we cannot move it
         ahead of there.  I.e., if idx0 lies outside [ridx, idx-1] (considering
         "wrap-around"), do not move the entry and try the next entry
         instead. */
      if ((ridx <= idx0 && idx0 < idx) ||
          (idx0 >= ridx && idx < ridx) ||
          (idx0 < idx && idx < ridx)) {
        /* idx0 is in [ridx, idx-1]: Move the entry. */
        break;
      } else {
        idx = (idx+1) & mask;
        rseq = table[idx];
        if (rseq == 0) goto done;
      }  /* if */
    }  /* for */
    table[idx0] = table[idx];
    table[idx] = 0;
    idx0 = idx;
    idx = (idx0+1) & mask;
    rseq = table[idx];
    if (rseq == 0) goto done;
  }  /* for */
done:;
}  /* check_deleted_live_set_slot */


/*
Return TRUE if the given allocation sequence number is in the given live set.
This macro is written with the assumption that in the vast majority of cases
the sequence number is present and it needs no probing to be found.
*/
#define in_live_set(set, seq)                                                \
  ((set)->table[hash_alloc_seq_number(seq) & (set)->hash_mask] == seq ?      \
     TRUE : f_in_live_set(set, seq))


static a_boolean f_in_live_set(a_live_set           *set,
                               an_alloc_seq_number  seq)
/*
Return TRUE if the given allocation sequence number is in the given live set.
Return FALSE otherwise.  This is normally always called through the macro
in_live_set.
*/
{
  a_boolean  result;

  if (seq == 0) {
    /* Sequence number zero (static storage) is always "live". */
    result = TRUE;
  } else {
    a_live_set_index     mask = set->hash_mask;
    a_live_set_index     idx = hash_alloc_seq_number(seq) & mask;
    an_alloc_seq_number  *table = set->table;
    for (;;) {
      an_alloc_seq_number  tseq = table[idx];
      if (tseq == seq) {
        result = TRUE;
        break;
      } else if (tseq == 0) {
        result = FALSE;
        break;
      }  /* if */
      idx = (idx+1) & mask;
    }  /* for */
  }  /* if */
  return result;
}  /* f_in_live_set */


/*
Structure describing the information following the value bytes for a variable
object: its allocation sequence number and its prior mapping.
*/
typedef struct a_var_postfix {
  an_alloc_seq_number
		alloc_seq_number;
			/* The allocation sequence number for the variable
			   object's storage. */
  a_byte
		*prev_storage;
			/* The storage previously associated with this
			   variable. */
} a_var_postfix;


/*
Structure maintaining data about the IL interpreter across a complete
interpretation of a constexpr function and its callees.
*/
typedef struct an_interpreter_state {
  a_data_map
		map;
			/* A hash table mapping pointers into the IL onto
			   associated data.  This hash table contains several
			   categories of key value pairs:
			   - Variable (including "this" and parameter
			     variables) ptr to storage bytes ptr.
			   - Current call frame ptr to this bytes ptr.
			   - Constant ptr to constant bytes ptr.
			   - Initializer ptr to constant bytes ptr.
			   - Initializer expr ptr to constant ptr.
			   - Reused init ptr to bytes ptr.
			   - Complete object ptr - NATURALIZABLE_KEY_OFFSET to
			     argument static_storage indicating the object can
			     be naturalized. */
  a_storage_stack_state
		storage_stack;
			/* The current state of the storage stack. */
  a_live_set
		live_set;
			/* The set of allocation sequence numbers that are
			   still "live". */
  a_call_frame_ptr
		curr_call_frame;
			/* The currently active call. */
  a_storage_stack_state
		*extension_state;
			/* Pointer to the storage stack state from which a
			   temporary with extended lifetime should be
			   allocated. */
  a_constant_ptr
		constants;
			/* A list of local constants allocated for the
			   interpreter (to be released when interpretation is
			   done). */
  a_diag_list
		diag_list;
			/* A representation of pending diagnostics (presumably
			   explaining why interpretation failed to produce a
			   constant result).  These diagnostics are not
			   necessarily emitted (we may be in a SFINAE context,
			   or in an initialization context that permits both
			   constant and non-constant initializers). */
  a_source_position
		position;
			/* The position of the expression where interpretation
			   starts. */
  unsigned long	cost;
			/* An interpretation "cost" counter.  It counts the
			   number of calls and loop-back branches. */
  an_alloc_seq_number
		curr_alloc_seq_number;
			/* A sequence number counting the number of saved
			   storage stack states.  This is used to detect
			   dangling pointers. */
  a_bit_field
		is_constant_evaluated:1;
			/* The value returned by std::is_constant_evaluated in
			   this interpretation. */
  a_bit_field
		is_variable_initializer:1;
			/* This interpreter invocation is to attempt to fold a
			   variable initializer. */
  a_bit_field
		allow_reinterpret_cast:1;
			/* TRUE if reinterpret_cast should be permitted when
			   possible.  This is used to optimize certain variable
			   initializations, which can then be implemented as
			   static initialization instead of dynamic
			   initialization. */
  a_bit_field
		static_storage_ready:1;
			/* TRUE if static_storage has been initialized. */
  a_bit_field
		side_effects_disabled:1;
			/* TRUE if side-effects (specifically: assignments and
			   increments/decrements) are disabled.  Any such side-
			   effects cause interpretation failure.  Used for
			   C++11 constexpr evaluation and to implement
			   __builtin_constant_p (a GCC extension). */
  a_bit_field
		suspend_diag_list:1;
			/* TRUE if diagnostic records should not be added to
			   diag_list.  Used to implement __builtin_constant_p
			   (a GCC extension). */
  a_bit_field
		input_error:1;
			/* TRUE if interpretation failed because an error
			   entry was encountered in the IL. */
  a_bit_field
		call_seen:1;
			/* TRUE if a call was interpreted. */
  a_bit_field
		permit_address_of_local_temporary:1;
			/* TRUE if copy_interpreter_object_to_constant should
			   permit the creation of an address of a local
			   temporary object (used for local static
			   initializer_list objects). */
  a_bit_field
		permit_null_pointer_offsets:1;
			/* TRUE if adding an offset to a null pointer is
			   permitted in a constant expression.  This is TRUE
			   in GNU C++ mode and within an __INTADDR__
			   construct. */
  a_bit_field
		static_lifetime_init:1;
			/* TRUE when interpreting the initializer for a static
			   lifetime variable. */
  a_bit_field
		disallow_mutable_field_load:1;
			/* TRUE if extract_value_from_constant should not
			   permit access to a mutable field. */
  a_bit_field
		allow_consteval_routine_node:1;
			/* TRUE if an enk_routine node for a consteval function
			   is permitted outside the immediate target operand of
			   a call (i.e., something like "(1, f)()" where f
			   designates a consteval function; if it were "f()"
			   it would always be allowed). */
 a_bit_field
		report_started:1;
			/* TRUE if a call to std::__report_constexpr_value was
			   already evaluated in this invocation of the
			   interpreter. */
  a_storage_stack_state
		static_storage;
			/* Pointer to the storage stack state used to allocate
			   static storage-duration variables.  Initialized
			   when first needed. */
  a_constexpr_allocation_ptr
		dyn_allocations;
			/* Pointer to a doubly-linked list of allocations
			   performed during the evaluation. */
  a_source_position
		*srcloc_builtin_pos;
			/* The current source location used for
			   std::source_location. */
} an_interpreter_state;


static unsigned long
		n_active_interpreter_states;
			/* The number of interpreter states that have been
			   initialized but not released. */

#define active_alloc_seq(ips)  ((ips)->storage_stack.alloc_seq_number)

#define cost_exceeded(ips)                                                   \
  (++(ips)->cost > max_cost_constexpr_call)


static a_byte	*free_stack_blocks;
			/* List of free stack blocks available for reuse. */


static void alloc_constexpr_stack_block(a_storage_stack_state  *sss)
/*
Initialize stack storage for the given storage stack.
*/
{
  a_byte        *new_block;
  a_byte_count  ptr_size = sizeof(a_byte*);

  do_host_alignment(ptr_size);
  if (free_stack_blocks == NULL) {
    new_block = (a_byte*)alloc_fe(CONSTEXPR_STACK_BLOCK_SIZE);
  } else {
    new_block = free_stack_blocks;
    free_stack_blocks = *(a_byte**)(new_block+ptr_size);
  }  /* if */
  /* Link to the previous block (if any). */
  *(a_byte**)new_block = sss->curr_block;
  sss->curr_block = new_block;
  /* Set the next-block pointer to NULL. */
  *(a_byte**)(new_block+ptr_size) = NULL;
  /* Leave space for the bookkeeping information (three pointers). */
  sss->top = sss->curr_block+3*ptr_size; /*lint !e679*/
  sss->large_blocks = NULL;
  sss->destructions = NULL;
}  /* alloc_constexpr_stack_block */


/*
Initialize the given storage stack.
*/
#define init_constexpr_stack(sss)                                            \
{                                                                            \
  alloc_constexpr_stack_block(sss);                                          \
  (sss)->alloc_seq_number = 1;                                               \
}



static void release_constexpr_stack(a_storage_stack_state  *sss)
/*
Release the storage stack pointed to by sss for reuse.
*/
{
  a_byte  *first_block = sss->curr_block;

  if (first_block != NULL) {
    if (free_stack_blocks != NULL) {
      /* Some blocks are already on the "free blocks" list.  Append those free
         blocks to those about to be freed. */
      a_byte        *block, *next_block;
      a_byte_count  ptr_size = sizeof(a_byte*);
      do_host_alignment(ptr_size);
      for (block = first_block; ; block = next_block) {
        next_block = *(a_byte**)(block+ptr_size);
        if (next_block == NULL) break;
      }  /* for */
      *(a_byte**)(block+ptr_size) = free_stack_blocks;
    }  /* if */
    free_stack_blocks = first_block;
    sss->curr_block = NULL;
  }  /* if */
}  /* release_constexpr_stack */

#if DEBUG

void db_live_set(void  *interpreter_state)
/*
Output the current live set record in ips.
*/
{
  an_interpreter_state  *ips = (an_interpreter_state*)interpreter_state;
  an_alloc_seq_number   num = 1;

  (void)fprintf(f_debug, "live set:");
  while (num <= ips->curr_alloc_seq_number+1) {
    if (in_live_set(&ips->live_set, num)) {
      (void)fprintf(f_debug, "  %lu", (unsigned long)num);
    }  /* if */
    num += 1;
  }  /* while */
  (void)fprintf(f_debug, "\n");
}  /* db_live_set */

#endif /* DEBUG */

/*
Macro producing the number of stack storage bytes left in the current block.
*/
#define stack_bytes_left(sss)                                                \
  ((a_byte_count)(CONSTEXPR_STACK_BLOCK_SIZE -                               \
                                          ((sss)->top - (sss)->curr_block)))


static void add_storage_stack_block(a_storage_stack_state  *sss)
/*
Add a block of storage (to parcel out) to the given storage stack.
*/
{
  a_byte_count  ptr_size = sizeof(a_byte*);
  a_byte        *next_block;

  do_host_alignment(ptr_size);
  /* Record the current stack top in the bookkeeping section. */
  *(a_byte**)(sss->curr_block+2*ptr_size) = sss->top; /*lint !e679*/
  next_block = *(a_byte**)(sss->curr_block+ptr_size);
  if (next_block == NULL) {
    /* The current block is the last block: Allocate a new one. */
    alloc_constexpr_stack_block(sss);
  } else {
    /* Reuse a previously-allocated block. */
    sss->curr_block = next_block;
    /* Leave space for the bookkeeping information (three pointers). */
    sss->top = sss->curr_block+3*ptr_size; /*lint !e679*/
  }  /* if */
}  /* add_storage_stack_block */


/*
Macro to allocate bytes in stack storage.  Deallocation is handled by restoring
a previously saved stack state.
*/
#define alloc_bytes(sss, n_bytes, storage_ptr)                               \
  { if ((n_bytes) > CONSTEXPR_STACK_ALLOC_LIMIT /*lint -e506*/) {            \
      /* We'll allocate the bytes in a separate general allocation block. */ \
      a_byte        *large_block;                                            \
      a_byte_count  hdr_size = sizeof(a_large_block_header), block_size;     \
      do_host_alignment(hdr_size);                                           \
      block_size = hdr_size+(n_bytes);                                       \
      large_block = (a_byte*)alloc_general(block_size);                      \
      ((a_large_block_header*)large_block)->prev_large_block =               \
                                                        (sss)->large_blocks; \
      ((a_large_block_header*)large_block)->block_size = block_size;         \
      ((a_large_block_header*)large_block)->alloc_seq_number =               \
                                                    (sss)->alloc_seq_number; \
      (sss)->large_blocks = large_block;                                     \
      (storage_ptr) = large_block+hdr_size;                                  \
    } else {                                                                 \
      a_byte_count  size = (n_bytes);                                        \
      do_host_alignment(size);                                               \
      if (size > stack_bytes_left(sss)) {                                    \
        add_storage_stack_block(sss);                                        \
      }  /* if */                                                            \
      (storage_ptr) = (sss)->top;                                            \
      (sss)->top += size;                                                    \
    }  /* if */                                                              \
  }


/*
Convenience macro to allocate bytes in the storage stack of an interpreter
state.
*/
#define alloc_stack_bytes(ips, n_bytes, storage_ptr)                         \
  alloc_bytes(&(ips)->storage_stack, n_bytes, storage_ptr)


/*
Convenience macro to allocate bytes in the static storage area of an
interpreter state.
*/
#define alloc_static_bytes(ips, n_bytes, storage_ptr)                        \
{                                                                            \
  if (!(ips)->static_storage_ready) {                                        \
    /* This is the first time we allocate static storage: Initialize */      \
    /* the associated static storage stack. */                               \
    alloc_constexpr_stack_block(&(ips)->static_storage);                     \
    (ips)->static_storage_ready = TRUE;                                      \
    (ips)->static_storage.alloc_seq_number = 0;                              \
  }  /* if */                                                                \
  alloc_bytes(&(ips)->static_storage, n_bytes, storage_ptr);                 \
}

/*
Macros to save and restore an allocation stack state.
*/
#define save_storage_stack(ips, state)                                       \
{                                                                            \
  (state) = (ips)->storage_stack;                                            \
  (ips)->storage_stack.alloc_seq_number = ++(ips)->curr_alloc_seq_number;    \
  add_to_live_set(&(ips)->live_set, (ips)->curr_alloc_seq_number);           \
  (ips)->storage_stack.destructions = NULL;                                  \
}


#define restore_storage_stack(ips, state, result_flag)                       \
{                                                                            \
  a_byte  *curr_large_blocks;                                                \
  if ((ips)->storage_stack.destructions != NULL && (result_flag)) {          \
    (result_flag) = perform_destructions(ips);                               \
  }  /* if */                                                                \
  curr_large_blocks = (ips)->storage_stack.large_blocks;                     \
  remove_from_live_set(&(ips)->live_set,                                     \
                       (ips)->storage_stack.alloc_seq_number);               \
  (ips)->storage_stack = (state);                                            \
  if (curr_large_blocks != NULL &&                                           \
      curr_large_blocks != (state).large_blocks) {                           \
    /* Delete large blocks no longer in the live set. */                     \
    do {                                                                     \
      a_byte  *large_block = curr_large_blocks;                              \
      an_alloc_seq_number  seq = ((a_large_block_header*)large_block)        \
                                                         ->alloc_seq_number; \
      if (in_live_set(&(ips)->live_set, seq)) break;                         \
      curr_large_blocks = ((a_large_block_header*)large_block)               \
                                                      ->prev_large_block;    \
      free_general(large_block,                                              \
                   ((a_large_block_header*)large_block)->block_size);        \
    } while (curr_large_blocks != NULL);                                     \
    (ips)->storage_stack.large_blocks = curr_large_blocks;                   \
  }  /* if */                                                                \
}


#if defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ < 5
/*
Some versions of GCC 4.x issue spurious "uninitialized" diagnostics when the
optimizer is enabled (on code where unneeded initialization is undesirable
because of performance concerns).
*/
#define init_storage_stack_state_to_silence_GCC(sss)                         \
  ((sss).top = (sss).curr_block = (sss).large_blocks = NULL,                 \
   (sss).destructions = NULL,                                                \
   (sss).alloc_seq_number = 0)
#else /* !defined(__GNUC__) && ... */
#define init_storage_stack_state_to_silence_GCC(sss) /* Nothing */
#endif /* defined(__GNUC__) && ... */


/*
Convenience macro to extract a host large integer from the integer value at
bytes of type tp, which must be a tk_integer type.  The result is left in
val.  ovfl is set to TRUE if the value is too large or small for a host
large integer.
*/
#define get_int_val_from(bytes, tp, val, ovfl)                               \
  conv_integer_value_to_host_large_integer((an_integer_value *)(bytes),      \
                                           int_type_is_signed(tp),           \
                                           &(val), &(ovfl))


/*
Macros to push and pop call frames.
*/
#define push_call_frame(ips, p_frame, rp, pos, p_result, p_complete)         \
  {                                                                          \
    (p_frame)->parent = (ips)->curr_call_frame;                              \
    (p_frame)->routine = (rp);                                               \
    (p_frame)->variant.position = (pos);                                     \
    (p_frame)->result_storage = (p_result);                                  \
    (p_frame)->complete_object = (p_complete);                               \
    (p_frame)->return_active = FALSE;                                        \
    (p_frame)->loop_break_active = FALSE;                                    \
    (p_frame)->continue_active = FALSE;                                      \
    (p_frame)->switch_break_active = FALSE;                                  \
    (ips)->curr_call_frame = (p_frame);                                      \
    (ips)->call_seen = TRUE;                                                 \
  }

#define pop_call_frame(ips)                                                  \
  ((ips)->curr_call_frame = (ips)->curr_call_frame->parent)

#if GNU_EXTENSIONS_ALLOWED
/*
Macro to push a GNU statement expressions frame (which is a special kind of
call frame).
*/
#define push_stmt_expr(ips, p_frame, stmt_expr, p_result, p_complete)        \
  {                                                                          \
    (p_frame)->parent = (ips)->curr_call_frame;                              \
    (p_frame)->routine = NULL;                                               \
    (p_frame)->variant.expr = (stmt_expr);                                   \
    (p_frame)->result_storage = (p_result);                                  \
    (p_frame)->complete_object = (p_complete);                               \
    (p_frame)->return_active = FALSE;                                        \
    (p_frame)->loop_break_active = FALSE;                                    \
    (p_frame)->continue_active = FALSE;                                      \
    (p_frame)->switch_break_active = FALSE;                                  \
    (ips)->curr_call_frame = (p_frame);                                      \
    (ips)->call_seen = FALSE;                                                \
  }
#endif /* GNU_EXTENSIONS_ALLOWED */


/*
Macro to retrieve a pointer (dptr) associated with a pointer (iptr) from a
given data map.
*/
/*lint -emacro(578,get_mapped_ptr)*/
#define get_mapped_ptr(map, iptr, dptr)                                      \
{                                                                            \
  uintptr_t         hash = hash_ptr(iptr);                                   \
  a_map_index       msk = (map)->hash_mask;                                  \
  a_map_index       i = hash & msk;                                          \
  a_data_map_entry  *tbl = (map)->table;                                     \
  for (;;) {                                                                 \
    a_byte  *tptr = tbl[i].ptr;                                              \
    if (tptr == (a_byte*)(iptr)) {                                           \
      (dptr) = tbl[i].data.ptr;                                              \
      break;                                                                 \
    } else if (tptr == NULL) {                                               \
      (dptr) = NULL;                                                         \
      break;                                                                 \
    }  /* if */                                                              \
    i = (i+1) & msk;                                                         \
  }  /* for */                                                               \
}


/*
Macro to retrieve a byte count (bcount) associated with a pointer (iptr)
from a given data map.
*/
/*lint -emacro(578,get_mapped_byte_count)*/
#define get_mapped_byte_count(map, iptr, bcount)                             \
{                                                                            \
  uintptr_t         hash = hash_ptr(iptr);                                   \
  a_map_index       mask = (map)->hash_mask;                                 \
  a_map_index       idx = hash & mask;                                       \
  a_data_map_entry  *table = (map)->table;                                   \
  for (;;) {                                                                 \
    a_byte  *tptr = table[idx].ptr;                                          \
    if (tptr == (a_byte*)(iptr)) {                                           \
      (bcount) = table[idx].data.byte_count;                                 \
      break;                                                                 \
    } else if (tptr == NULL) {                                               \
      (bcount) = 0;                                                          \
      break;                                                                 \
    }  /* if */                                                              \
    idx = (idx+1) & mask;                                                    \
  }  /* for */                                                               \
}


/*
Convenience macro to retrieve a pointer (sptr) to the stack storage associated
with an IL pointer (iptr).  ips is the interpreter state managing the stack
storage.
*/
#define get_stack_bytes(ips, iptr, sptr)                                     \
  get_mapped_ptr(&(ips)->map, iptr, sptr)

#if DEBUG

void db_data_map(void  *map_address)
/*
Output some information about a data map's contents
*/
{
  a_data_map        *map = (a_data_map*)map_address;
  a_data_map_entry  *table = map->table;
  a_map_index       mask = map->hash_mask;
  a_map_index       k, n_slots = mask+1;

  for (k = 0; k<n_slots; ++k) {
    a_byte  *ptr = table[k].ptr;
    fprintf(f_debug, "[%2u] ", k);
    if (ptr == NULL) {
      fprintf(f_debug, "(empty)\n");
    } else {
      fprintf(f_debug, "h = %2u  %p\n",
              (a_map_index)hash_ptr(ptr) & mask, (a_void_ptr)ptr);
    }  /* if */
  }  /* for */
}  /* db_data_map */

#endif /* DEBUG */

static void expand_ptr_map(a_data_map  *map)
/*
Double the number of entries in the given map.  This requires rehashing.
*/
{
  a_data_map_entry  *new_table, *old_table = map->table;
  a_map_index       mask = map->hash_mask;
  a_map_index       k, n_slots = mask+1;
  a_byte_count      old_size =
                              (a_byte_count)(n_slots*sizeof(a_data_map_entry));
  a_byte_count      new_size = 2*old_size;
  unsigned long     new_width = count_ones(mask)+1, old_width;

  if (new_width > MAX_WIDTH_REUSABLE_TABLE) {
    new_table = (a_data_map_entry*)alloc_general(new_size);
  } else if (free_map_tables[new_width] != NULL) {
    new_table = free_map_tables[new_width];
    free_map_tables[new_width] =
                           (a_data_map_entry*)free_map_tables[new_width]->ptr;
  } else {
    new_table = (a_data_map_entry*)alloc_fe(new_size);
  }  /* if */
  memzero((char*)new_table, size_t_arg(new_size));
  mask = mask*2+1;
  for (k = 0; k<n_slots; ++k) {
    a_byte  *ptr = old_table[k].ptr;
    if (ptr != NULL) {
      a_map_index  idx = hash_ptr(ptr) & mask;
      while (new_table[idx].ptr != NULL) {
        idx = (idx+1) & mask;
      }  /* while */
      new_table[idx] = old_table[k];
    }  /* if */
  }  /* for */
  map->table = new_table;
  map->hash_mask = mask;
  old_width = new_width-1;
  if (old_width > MAX_WIDTH_REUSABLE_TABLE) {
    free_general(old_table, old_size);
  } else {
    old_table[0].ptr = (a_byte*)free_map_tables[old_width];
    free_map_tables[old_width] = old_table;
  }  /* if */
}  /* expand_ptr_map */


#ifdef TRACE_INTERPRETER_MAP

static a_byte
		*traced_iptr = NULL;
			/* Pointer that is checked for mapping activity.
			   Intended to be set from within a debugger and
			   watched by setting a breakpoint on function
			   interpreter_map_intercept. */

static void interpreter_map_intercept(a_const_char  *msg)
/*
Function called when a map key equal to traced_iptr is entered into or removed
from a map.
*/
{
#if DEBUG
  fprintf(f_debug, "\nMap activity for %p: %s\n", traced_iptr, msg);
#endif /* DEBUG */
}  /* interpreter_map_intercept */

#define check_traced_iptr(ptr, msg)                                          \
  if ((a_byte*)(ptr) == traced_iptr) interpreter_map_intercept(msg);

#else /* TRACE_INTERPRETER_MAP  */

#define check_traced_iptr(ptr, msg) /* Nothing */

#endif /* TRACE_INTERPRETER_MAP */


/*
Macro to add a (pointer, pointer) entry to a data map.  The key pointer (iptr)
may not be in the map already.
*/
#define map_ptr(map, iptr, dptr)                                             \
{                                                                            \
  uintptr_t    hash = hash_ptr(iptr);                                        \
  a_map_index  mask = (map)->hash_mask;                                      \
  a_map_index  idx = hash & mask;                                            \
  a_data_map_entry  *table = (map)->table;                                   \
  check_traced_iptr(iptr, "mapped");                                         \
  if (table[idx].ptr == NULL) {                                              \
    table[idx].ptr = (a_byte*)(iptr);                                        \
    table[idx].data.ptr = (dptr);                                            \
  } else {                                                                   \
    a_data_map_entry  entry;                                                 \
    entry.ptr = (a_byte*)(iptr);                                             \
    entry.data.ptr = (dptr);                                                 \
    map_colliding_ptr(map, entry, idx);                                      \
  }  /* if */                                                                \
  (map)->n_elements += 1;                                                    \
  if ((map)->n_elements*2 > mask) {                                          \
    expand_ptr_map(map);                                                     \
  }  /* if */                                                                \
}


#define map_or_replace_ptr(map, iptr, dptr, old_dptr)                        \
{                                                                            \
  uintptr_t         hash = hash_ptr(iptr);                                   \
  a_map_index       mask = (map)->hash_mask;                                 \
  a_map_index       idx = hash & mask, idx0 = idx;                           \
  a_data_map_entry  *table = (map)->table;                                   \
  a_byte            *ptr = table[idx].ptr;                                   \
  check_traced_iptr(iptr, "mapped or replaced");                             \
  if (ptr == NULL) {                                                         \
    table[idx].ptr = (a_byte*)(iptr);                                        \
    table[idx].data.ptr = (dptr);                                            \
    (map)->n_elements += 1;                                                  \
    if ((map)->n_elements*2 > mask) {                                        \
      expand_ptr_map(map);                                                   \
    }  /* if */                                                              \
    (old_dptr) = NULL;                                                       \
  } else {                                                                   \
    for (;;) {                                                               \
      if (ptr == (a_byte*)(iptr)) {                                          \
        (old_dptr) = table[idx].data.ptr;                                    \
        table[idx].data.ptr = (dptr);                                        \
        break;                                                               \
      } else {                                                               \
        idx = (idx+1) & mask;                                                \
        ptr = table[idx].ptr;                                                \
        if (ptr == NULL) {                                                   \
          table[idx] = table[idx0];                                          \
          table[idx].ptr = (a_byte*)(iptr);                                  \
          table[idx].data.ptr = (dptr);                                      \
          (map)->n_elements += 1;                                            \
          if ((map)->n_elements*2 > mask) {                                  \
            expand_ptr_map(map);                                             \
          }  /* if */                                                        \
          (old_dptr) = NULL;                                                 \
          break;                                                             \
        }  /* if */                                                          \
      }  /* if */                                                            \
    }  /* for */                                                             \
  }  /* if */                                                                \
}


/*
Macro to replace the pointer associated with iptr (which is known to be in
the table).
*/
#define replace_mapped_ptr(map, iptr, dptr)                                  \
{                                                                            \
  uintptr_t         hash = hash_ptr(iptr);                                   \
  a_map_index       mask = (map)->hash_mask;                                 \
  a_map_index       idx = hash & mask;                                       \
  a_data_map_entry  *table = (map)->table;                                   \
  a_byte            *ptr = table[idx].ptr;                                   \
  check_traced_iptr(iptr, "replaced");                                       \
  for (;;) {                                                                 \
    if (ptr == (a_byte*)(iptr)) {                                            \
      table[idx].data.ptr = (dptr);                                          \
      break;                                                                 \
    } else {                                                                 \
      idx = (idx+1) & mask;                                                  \
      ptr = table[idx].ptr;                                                  \
    }  /* if */                                                              \
  }  /* for */                                                               \
}


/*
Macro to add a (pointer, byte-count) entry to a data map.
*/
#define map_byte_count(map, iptr, bcount)                                    \
{                                                                            \
  uintptr_t    hash = hash_ptr(iptr);                                        \
  a_map_index  mask = (map)->hash_mask;                                      \
  a_map_index  idx = hash & mask;                                            \
  a_data_map_entry  *table = (map)->table;                                   \
  a_byte            *cached_ptr = table[idx].ptr;                            \
  check_traced_iptr(iptr, "mapped (bytecount)");                             \
  if (cached_ptr == NULL) {                                                  \
    table[idx].ptr = (a_byte*)(iptr);                                        \
    table[idx].data.byte_count = (bcount);                                   \
  } else {                                                                   \
    a_data_map_entry  entry;                                                 \
    entry.ptr = (a_byte*)(iptr);                                             \
    entry.data.byte_count = (bcount);                                        \
    map_colliding_ptr(map, entry, idx);                                      \
  }  /* if */                                                                \
  (map)->n_elements += 1;                                                    \
  if ((map)->n_elements*2 > mask) {                                          \
    expand_ptr_map(map);                                                     \
  }  /* if */                                                                \
}

static void map_colliding_ptr(a_data_map        *map,
                              a_data_map_entry  new_entry/*lint !e1746*/,
                              a_map_index       idx)
/*
The given map entry collides with an existing entry in the given map.  Move
the existing entry to the next free entry, and record new_entry at the given
location.
*/
{
  a_map_index       mask = map->hash_mask;
  a_data_map_entry  *table = map->table;
  a_data_map_entry  saved_entry;

#if EXPENSIVE_CHECKING
  { a_byte  *dptr;
    get_mapped_ptr(map, new_entry.ptr, dptr)
    if (dptr != NULL) {
      unexpected_condition_str("duplicate map key in interpreter");
    }  /* if */
  }
#endif /* EXPENSIVE_CHECKING */
  /* Place the new mapping at idx, and move the existing mapping to the
     next available spot. */
  saved_entry = table[idx];
  table[idx] = new_entry;
  for (;;) {
    idx = (idx+1) & mask;
    if (table[idx].ptr == NULL) {
      table[idx] = saved_entry;
      break;
    }  /* if */
  }  /* for */
}  /* map_colliding_ptr */


#define unmap_ptr(map, iptr)                                                 \
{                                                                            \
  uintptr_t         hash = hash_ptr(iptr);                                   \
  a_map_index       mask = (map)->hash_mask;                                 \
  a_map_index       idx = hash & mask;                                       \
  a_data_map_entry  *table = (map)->table;                                   \
  check_traced_iptr(iptr, "UNmapped");                                       \
  /* Find the item to delete (we're assuming it exists). */                  \
  while (table[idx].ptr != (a_byte*)iptr) {                                  \
    idx = (idx+1) & mask;                                                    \
  }  /* while */                                                             \
  table[idx].ptr = NULL;                                                     \
  /* If the next slot is empty, we're done.  Otherwise, we may have to */    \
  /* move another element into the emptied slot. */                          \
  if (table[(idx+1) & mask].ptr != NULL) {                                   \
    check_deleted_data_map_slot(map, idx);                                   \
  }  /* if */                                                                \
  (map)->n_elements -= 1;                                                    \
}


static void check_deleted_data_map_slot(a_data_map   *map,
                                        a_map_index  idx0)
/*
Slot idx has been cleared in the given map (i.e., map->table[idx].ptr has been
set to NULL).  The next slot is not empty.  There may therefore exist entries
that are associated with that slot (i.e., have the same hash index).  This
function makes sure that such entries can be found, by moving up entries as
needed.

This corresponds to Algorithm R in section 6.4 of volume 3 of Donald E. Knuth's
"The Art of Computer Programming" (Sorting and Searching -- Second Edition),
with the assumption that step R1 has already been performed (idx0 is "j") and
we know that the subsequent slot is not empty.
*/
{
  a_data_map_entry  *table = map->table;
  a_map_index       mask = map->hash_mask;
  a_map_index       idx, ridx;
  a_byte            *rptr;
  
  idx = (idx0+1) & mask;
  rptr = table[idx].ptr;
  for (;;) {
    for (;;) {
      ridx = hash_ptr(rptr) & mask;
      /* See if we can move the entry at idx to idx0.  ridx is its "ideal"
         slot: the place from where probing will start.  So we cannot move it
         ahead of there.  I.e., if idx0 lies outside [ridx, idx-1] (considering
         "wrap-around"), do not move the entry and try the next entry
         instead. */
      if ((ridx <= idx0 && idx0 < idx) ||
          (idx0 >= ridx && idx < ridx) ||
          (idx0 < idx && idx < ridx)) {
        /* idx0 is in [ridx, idx-1]: Move the entry. */
        break;
      } else {
        idx = (idx+1) & mask;
        rptr = table[idx].ptr;
        if (rptr == NULL) goto done;
      }  /* if */
    }  /* for */
    table[idx0] = table[idx];
    table[idx].ptr = NULL;
    idx0 = idx;
    idx = (idx0+1) & mask;
    rptr = table[idx].ptr;
    if (rptr == NULL) goto done;
  }  /* for */
done:;
}  /* check_deleted_data_map_slot */


/*
Convenience macro to map an IL pointer (iptr) to a pointer (sptr) to stack
storage associated.  ips is the interpreter state managing the stack storage.

If an existing mapping exists for iptr, the new mapping supersedes it (until
it is unmapped).
*/
#define map_stack_bytes(ips, iptr, sptr)                                     \
  map_ptr(&(ips)->map, iptr, sptr)


/*
Convenience macro to unmap the pointer to stack storage associated with an
IL pointer (iptr).  ips is the interpreter state managing the stack storage.

If an older mapping exists for iptr, that mapping becomes active again.
*/
#define unmap_stack_bytes(ips, iptr)                                         \
  unmap_ptr(&(ips)->map, iptr)


/*
Useful constants and types.
*/
static an_integer_value
		zero_int;
static an_integer_value
		one_int;
static an_internal_float_value
		zero_flt[(int)fk_last];
static an_internal_float_value
		one_flt[(int)fk_last];
static a_type_ptr
		generic_ptr_type;
			/* Type (void*) used in some cases where a pointer type
			   is needed, but the specific type is unimportant. */
static a_boolean
		useful_constants_initialized;
			/* Flag indicating whether these constants and types
			   have been initialized yet. */


/*
Structure describing a variant path in a subobject.

An address of a subobject of a union can be formed even if that subobject is
not currently active in the union.  Only at the point of dereference must the
requirement that the subobject is active be enforced.  To achieve this, address
manipulations in unions maintain a "variant path" that can be checked at the
point of dereference.  The first entry on the path describes the base address
of an array: The entry can be ignored if the address does not have the
CA_ARRAY_ELEMENT flag set.  Every element after that represents a selected
variant (from outer selection to inner selection).  For example:

  struct S {
    int i;
    union U {
      struct X {
        union V {
          int i;
          char y[3];
        } v;
      } x;
    } u[4];
  } s;

The a_constexpr_address entry representing &s.u[2].x.v.y[1] will have both the
CA_VARIANT_PATH and CA_ARRAY_ELEMENT flags set (the latter flag is for the y[1]
part; not the u[2] part since the address is not of the s.u[2] element
specifically).  The address entry will point to a list of three variant path
entries.  The first entry will record the base address of s.u[2].x.v.y.  The
second will point to the address of the s.u[2] subobject and to the IL entry
for its x field.  The third entry will point to the address of the s.u[2].x.v
subobject and to the IL entry for its y field.  Dereferencing the address will
check that the two unions' active fields correspond to those recorded in the
path (if they don't, interpretation fails).
*/
typedef struct a_variant_path_entry *a_variant_path_entry_ptr;
typedef struct a_variant_path_entry {
  a_variant_path_entry_ptr
		next;
			/* For entries on a variant path, the next entry on
			   that path (or NULL if there is none).  Otherwise,
			   the entry is on the free entries list and this field
			   points to the next free entry (or NULL if there is
			   none). */
  a_variant_path_entry_ptr
		next_allocated;
			/* The next entry on the list of all allocated variant
			   path entries (NULL if it's the last entry). */
  a_field_ptr	field;
			/* The field selected for this variant path, or NULL
			   if this entry represents the base address of an
			   indexed array. */
  a_byte	*base_address;
			/* The address of the variant (i.e., union) subobject
			   in interpreter storage, or, if field is NULL, the
			   base address of the indexed array. */
} a_variant_path_entry;

static a_variant_path_entry_ptr
		variant_path_entries;
			/* A list of all allocated variant path entries,
			   linked through the next_allocated pointers. */

static unsigned long
		n_variant_path_entries;
			/* The number of entries on the variant_path_entries
			   list. */

static a_variant_path_entry_ptr
		free_variant_path_entries;
			/* A list of variant path entries available for reuse,
			   linked through the next pointers. */

static unsigned long
		n_free_variant_path_entries;
			/* The number of entries on the
			   free_variant_path_entries list. */

static a_variant_path_entry_ptr alloc_variant_path_entry(void)
/*
Return a new variant path entry.
*/
{
  a_variant_path_entry_ptr vpep;

  if (free_variant_path_entries != NULL) {
    vpep = free_variant_path_entries;
    free_variant_path_entries = free_variant_path_entries->next;
    n_free_variant_path_entries -= 1;
  } else {
    vpep = alloc_fe_of_type(a_variant_path_entry);
    vpep->next_allocated = variant_path_entries;
    variant_path_entries = vpep;
    n_variant_path_entries += 1;
  }  /* if */
  return vpep;
}  /* alloc_variant_path_entry */


static void reclaim_variant_path_entries(void)
/*
The caller has determined that not all variant path entries were reclaimed at
the end of interpretation.  Move all allocated entries back onto the free list.
*/
{
  a_variant_path_entry_ptr  vpep = variant_path_entries;

  check_assertion(n_free_variant_path_entries < n_variant_path_entries);
  while (vpep->next_allocated != NULL) {
    vpep->next = vpep->next_allocated;
    vpep = vpep->next_allocated;
  }  /* while */
  vpep->next = NULL;
  free_variant_path_entries = variant_path_entries;
  n_free_variant_path_entries = n_variant_path_entries;
}  /* reclaim_variant_path_entries */


/*
A set of flags to describe special interpreter address attributes.
*/
#define CA_RUNTIME_DATA_ADDRESS ((unsigned int)0x1)
		/* This flag indicates that the address is that of a run-time
		   entity (not a value known to the interpreter). */
#define CA_CANNOT_DEREFERENCE ((unsigned int)0x2)
		/* This flag indicates that the address cannot be dereferenced.
		   It is set in particular for pointers "one position past" the
		   end of an array. */
#define CA_VARIANT_PATH ((unsigned int)0x4)
		/* This flag indicates that the formation of the address
		   included the selection of at least one union field.  Such
		   selections must be checked for validity when the address is
		   dereferenced. */
#define CA_ARRAY_ELEMENT ((unsigned int)0x8)
		/* This flag indicates that the address is that of an array
		   element.  Such an address is subject to pointer
		   arithmetic (which requires bounds checking). */
#define CA_BIT_FIELD ((unsigned int)0x10)
		/* This flag indicates that the address is that of a bit field.
		   (Pointers and references to bit fields are invalid.  This is
		   therefore always for a bit field lvalue.)  Whether the bit
		   field is signed is encoded in the "length" field. */
#define CA_FUNCTION ((unsigned int)0x20)
		/* This flag indicates that the address is that of a
		   function. */
#define CA_CONST_STORAGE ((unsigned int)0x40)
		/* This flag indicates that the address is that of const
		   storage. */
#define CA_LIFETIME_EXTENDED ((unsigned int)0x80)
		/* This flag indicates that the address is that of a lifetime-
		   extended temporary. */

/*
Structure describing the representation of an address in the interpreter.
(Addresses in the interpreter are used to represent pointers, references, and
glvalues.)
*/
typedef struct a_constexpr_address {
  a_byte
		*address;
			/* The address in interpreter storage of the thing
			   pointed to, or NULL if is_runtime_data_address or
			   is_function_address are TRUE. */
  unsigned int	flags:8;
			/* Flags describing properties of this address.
			   See the CA_... macros above. */
  unsigned int
		length: ARRAY_LENGTH_WIDTH;
			/* If the CA_ARRAY_ELEMENT flag is set or for an array
		           lvalue, the number of elements in the array.  If the
			   CA_BIT_FIELD flag is set, twice the number of bits
			   in the bit field designated by this lvalue, plus one
			   if the bit field is signed.  If the flag
			   CA_RUNTIME_DATA_ADDRESS is set, a value of 1
			   indicates that a field selection was applied to that
			   address and so it should be assumed to point to an
			   object even if it is a null address (used to
			   support classic implementations of "offsetof").
			   Otherwise, zero. */
  an_alloc_seq_number
		alloc_seq_number;
			/* The allocation sequence number of the storage
			   pointed to. */
  union {
    /* When (flags & CA_ARRAY_ELEMENT) != 0 and
            (flags & CA_VARIANT_PATH) == 0: */
    a_byte
		*base_address;
			/* For an array element, the address of element #0. */
    /* When (flags & CA_FUNCTION) != 0: */
    a_routine_ptr
		routine;
    			/* For addresses of functions. */
    /* When (flags & CA_RUNTIME_DATA_ADDRESS) != 0: */
    a_constant_ptr
		addr_con;
			/* For constant addresses of run-time objects. */
    /* When (flags & CA_VARIANT_PATH) != 0: */
    a_variant_path_entry_ptr
		variant_path;
			/* For addresses into variant subobjects, the recorded
			   path of the subobject.  The path is checked against
			   active fields at the point of dereference. */
  } variant;
  a_byte
		*complete_object;
			/* Pointer to the complete object into which this
			   address is pointing.  This is needed to validate
			   pointer comparisons (p < q, etc.). */
} a_constexpr_address;


#define is_runtime_data_address(cap)                                         \
  ((((a_constexpr_address*)(cap))->flags & CA_RUNTIME_DATA_ADDRESS) != 0)

#define cannot_dereference(cap)                                              \
  ((((a_constexpr_address*)(cap))->flags & CA_CANNOT_DEREFERENCE) != 0)

#define is_variant_path(cap)                                                 \
  ((((a_constexpr_address*)(cap))->flags & CA_VARIANT_PATH) != 0)

#define is_array_element(cap)                                                \
  ((((a_constexpr_address*)(cap))->flags & CA_ARRAY_ELEMENT) != 0)

#define is_bit_field_lvalue(cap)                                             \
  ((((a_constexpr_address*)(cap))->flags & CA_BIT_FIELD) != 0)

#define is_function_address(cap)                                             \
  ((((a_constexpr_address*)(cap))->flags & CA_FUNCTION) != 0)

#define is_const_storage(cap)                                                \
  ((((a_constexpr_address*)(cap))->flags & CA_CONST_STORAGE) != 0)


#define get_base_address(cap)                                                \
  (is_variant_path(cap) ? (cap)->variant.variant_path->base_address          \
                        : (cap)->variant.base_address)

static void get_runtime_array_pos(an_interpreter_state  *ips,
                                  a_constexpr_address   *cap,
                                  a_byte_count          elem_size,
                                  a_byte_count          *a_len,
                                  a_byte_count          *p_pos)
/*
cap represents a run-time address constant or null pointer and elem_size the
size of the element type being addressed.  For null pointers, set *a_len and
*p_pos to zero.  Otherwise, return in *a_len the number of objects pointed to
if known (the length of an array or one for a non-array object); if unknown,
return MAX_ARRAY_LENGTH.  Return in *p_pos the "array" position being
addressed (with non-array objects treated as arrays of one element).
*/
{
  a_constant_ptr  con_addr = cap->variant.addr_con;
  a_byte_count    length, pos;
  a_type_ptr      tp;
  a_constant_ptr  cp;

  if (!constant_is(con_addr, ck_address)) {
    check_assertion(constant_is(con_addr, ck_integer));
    if (cmp_integer_values(&con_addr->variant.integer_value,
                           /*op_1_signed=*/FALSE,
                           (an_integer_value *)&zero_int,
                           /*op_2_signed=*/FALSE) == 0 &&
        !ips->permit_null_pointer_offsets) {
      /* A null pointer value in a context that does not permit null pointer
         offsets in constant-expressions. */
      length = 0;
      pos = 0;
    } else {
      /* A run-time address formed by casting an arbitrary non-zero integer
         to a pointer type (e.g., "(char*)0x1234").  Any length and position
         is a-priori possible. */
      length = MAX_ARRAY_LENGTH;
      pos = 0;
    }  /* if */
  } else if (elem_size == 0) {
    /* In some modes (e.g., GNU C), types can have size zero.  Any length and
       position is a-priori possible. */
    length = MAX_ARRAY_LENGTH;
    pos = 0;
  } else {
    a_boolean  use_subobject_path = FALSE;
    switch(con_addr->variant.address.kind) {
      case abk_variable:
        use_subobject_path = TRUE;
        break;
      case abk_constant:
      case abk_temporary:
        cp = con_addr->variant.address.variant.constant;
        if (constant_is(cp, ck_string)) {
          length = (a_byte_count)cp->variant.string.length/elem_size;
        } else {
          use_subobject_path = TRUE;
        }  /* if */
        break;
      case abk_uuidof:
        tp = type_pointed_to(con_addr->type);
        length = (a_byte_count)skip_typerefs(tp)->size/elem_size;
        break;
      case abk_typeid:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case abk_cli_typeid:
      case abk_cli_array:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* The object is std::type_info or a class derived from it, or a
           handle to a C++/CLI System::String or System::Array.  Therefore, we
           don't really know the actual size. */
        length = MAX_ARRAY_LENGTH;
        break;
      case abk_routine:
      case abk_label:
      default:
        length = 0;
        unexpected_condition();
    }  /* switch */
    if (use_subobject_path) {
      a_subobject_path_ptr  spp = con_addr->variant.address.subobject_path;
      a_type_ptr            atype = NULL;
      a_boolean             field_seen = FALSE;
      pos = 0;
      /* Look through the subobject path to find the type of the addressed
         sub-object (except for array elements) or the position of array
         elements. */
      for (; spp != NULL; spp = spp->next) {
        if (spp->is_offset) {
          pos = (a_byte_count)spp->variant.ptr_offset;
        } else if (spp->is_base_class) {
          atype = spp->variant.base_class->type;
          pos = 0;
        } else {
          atype = spp->variant.field->type;
          pos = 0;
          field_seen = TRUE;
        }  /* if */
      }  /* for */
      if (atype == NULL) {
        /* The subobject path doesn't designate a field or class subobject.
           So we are dealing with a top-level array or the address of an
           object treated as an array of one element. */
        if (address_base_is(con_addr, abk_variable)) {
          atype = con_addr->variant.address.variant.variable->type;
        } else {
          atype = con_addr->variant.address.variant.constant->type;
        }  /* if */
      }  /* if */
      if (type_is(atype, tk_array)) {
        if (has_unknown_specified_bound(atype) ||
            (!field_seen && array_type_has_no_bound(atype))) {
          /* The bound is not known (e.g., "extern int x[];" or a VLA. */
          length = MAX_ARRAY_LENGTH;
        } else {
          /* A known bound or a flexible array.  The latter is treated as a
             zero-length array in this context. */
          length = (a_byte_count)atype->variant.array
                                       .variant.number_of_elements;
        }  /* if */
      } else {
        length = 1;
      }  /* if */
    } else {
      pos = (a_byte_count)con_addr->variant.address.offset / elem_size;
    }  /* if */
  }  /* if */
  *a_len = length;
  *p_pos = pos;
}  /* get_runtime_array_pos */


/*
Given a data address cap pointing to an object of type elem_type, treat it as
the address of an element of an array (an array of length one if it's not
actually an array element).  Produce in *a_len, *pos, and *e_size,
respectively, the type of the element, the number of elements in the array,
the element position in the array, and the size of an element in the array.
This macro applies to both interpreter addresses and run-time addresses, but
in the case of run-time addresses the array length is set to MAX_ARRAY_LENGTH
if the actual length cannot be determined.  Set *p_result to FALSE if an error
occurs.  (Note that in GNU modes, elem_type can be tk_void because GCC
sometimes permits pointer arithmetic on void* pointers and treats then as
pointing to byte arrays.)
*/
#define get_array_pos(ips, cap, elem_type, a_len, pos, e_size, p_result)     \
{                                                                            \
  if (is_runtime_data_address(cap)) {                                        \
    *(e_size) = type_is(elem_type, tk_void) ? 1 :                            \
                                              (a_byte_count)elem_type->size; \
    get_runtime_array_pos(ips, cap, *(e_size), a_len, pos);                  \
  } else {                                                                   \
    *(e_size) = value_bytes_for_type(ips, elem_type, p_result);              \
    if (*p_result) {                                                         \
      if (is_array_element(cap)) {                                           \
        *(a_len) = (cap)->length;                                            \
        *(pos) = (a_byte_count)((cap)->address - get_base_address(cap));     \
        *(pos) /= *(e_size);                                                 \
      } else {                                                               \
        *(a_len) = 1;                                                        \
        *(pos) = cannot_dereference(cap) ? 1: 0;                             \
      }  /* if */                                                            \
    } else {                                                                 \
      *(a_len) = 0;                                                          \
      *(pos) = 0;                                                            \
    }  /* if */                                                              \
  }  /* if */                                                                \
}

/*
Convenience macro to get a pointer to the value addressed by the
a_constexpr_address addr.
*/
#define value_bytes_at(addr) (((a_constexpr_address *)(addr))->address)


/*
Convenience macro to get a pointer to the integer value addressed by the
a_constexpr_address addr.
*/
#define int_value_at(addr) ((an_integer_value *)value_bytes_at(addr))


/*
Convenience macro to cast an opaque pointer to a pointer to a floating-point
value.
*/
#define fp_value(ptr) ((an_internal_float_value *)(ptr))


/*
Convenience macro to get a pointer to the floating-point value addressed by
the a_constexpr_address addr.
*/
#define fp_value_at(addr) (fp_value(value_bytes_at(addr)))


#if C99_IL_EXTENSIONS_SUPPORTED
/*
Convenience macro to cast an opaque pointer to a pointer to a complex
floating-point value.
*/
#define cx_value(ptr) ((an_internal_complex_value *)(ptr))


/*
Convenience macro to get a pointer to the complex floating-point value
addressed by the a_constexpr_address addr.
*/
#define cx_value_at(addr) (cx_value(value_bytes_at(addr)))
#endif /* C99_IL_EXTENSIONS_SUPPORTED */


/*
Convenience macro to get the position of a constant.
 */
#define constant_pos(cp, ips) ((cp)->source_corresp.decl_position.seq != 0 ?  \
                               &(cp)->source_corresp.decl_position :          \
                               &(ips)->position)

/*
Convenience macro to get the position of a type.
 */
#define type_pos(tp, ips) ((tp)->source_corresp.decl_position.seq != 0 ?  \
                           &(tp)->source_corresp.decl_position :          \
                           &(ips)->position)


static void trim_bit_field(a_byte      *storage,
                           unsigned    length,
                           a_boolean   is_signed,
                           a_type_ptr  bftp)
/*
The given storage is that for an integer value representing a bit field of the
given length (the bit field is signed if is_signed is TRUE).  Trim the value
representation to fit in the bit field length.  bftp is the underlying type
(i.e., after skip_typerefs) of the bit field.
*/
{
  an_integer_value  *field_value = (an_integer_value*)storage;

  if (bftp->variant.integer.bool_type) {
    /* Boolean values aren't really "trimmed": They're just normalized to zero
       or one. */
    if (cmp_integer_values(field_value, /*op_1_signed=*/FALSE,
                           &zero_int, /*op_2_signed=*/FALSE) != 0) {
      *field_value = one_int;
    } else {
      *field_value = zero_int;
    }  /* if */
  } else if (is_signed) {
    sign_extend_integer_value(field_value, length);
  } else {
    a_boolean         ovflo;
    an_integer_value  mask = one_int;
    shift_left_integer_value(&mask, (int)length, &ovflo);
    subtract_integer_values(&mask, &one_int, /*is_signed=*/FALSE, &ovflo);
    and_integer_values(field_value, &mask);
  }  /* if */
}  /* trim_bit_field */


#define trim_bit_field_if_needed(addr, bit_field_tp)                          \
{                                                                             \
  if (is_bit_field_lvalue(addr)) {                                            \
    unsigned   length = (addr)->length;                                       \
    a_boolean  is_signed_field = (length & 1);                                \
    length = length/2;                                                        \
    trim_bit_field((addr)->address, length, is_signed_field, bit_field_tp);   \
  }  /* if */                                                                 \
}


/*
Macro to initialize a constant address at addr referring to the complete
interpreter value at targ_addr (or a null pointer).
*/
#define clear_address(addr, targ_addr)                                   \
  memzero((char *)(addr) /*lint -e668*/, sizeof(a_constexpr_address));   \
  ((a_constexpr_address *)(addr))->address = (targ_addr);                \
  ((a_constexpr_address *)(addr))->complete_object = (targ_addr)


static inline void set_active_address(an_interpreter_state  *ips,
                                      a_constexpr_address   *addr,
                                      a_byte                *value,
                                      a_byte                *compl_obj)
/*
Set the given interpreter address to point to value, with the current state's
active allocation sequence number.  compl_obj is the address of the associated
complete object.
*/
{
  addr->address = value;
  addr->flags = 0;
  addr->length = 0;
  addr->alloc_seq_number = active_alloc_seq(ips);
  addr->complete_object = compl_obj;
}  /* set_active_address */

/*
Macro to initialize a constant address at addr referring to the function
denoted by the IL a_routine entry rout.
*/
#define make_function_address(addr, rout)                      \
  memzero((char *)(addr), sizeof(a_constexpr_address));        \
  ((a_constexpr_address *)(addr))->flags = CA_FUNCTION;        \
  ((a_constexpr_address *)(addr))->variant.routine = (rout)


/*
Macro to initialize a constant address at addr referring to the
(non-interpreter) constant address described by the ck_address constant con.
*/
#define clear_runtime_constant_address(addr, con)                   \
  memzero((char *)(addr), sizeof(a_constexpr_address));             \
  ((a_constexpr_address *)(addr))->flags = CA_RUNTIME_DATA_ADDRESS; \
  ((a_constexpr_address *)(addr))->variant.addr_con = (con)


typedef struct a_constexpr_ptr_to_mem {
  a_bit_field
		is_ptr_to_mem_function:1;
			/* TRUE if this is a pointer to member function. */
  a_bit_field
		subtract_adjustment:1;
			/* TRUE if this_class_adjustment must be subtracted
			   from the "this" pointer. */
  a_byte_count
		this_class_adjustment;
			/* The adjustment needed to the "this" pointer. */
  union {
    /* When is_ptr_to_mem_function is FALSE. */
    a_field_ptr
		field;
			/* Field referred to by the pointer-to-member. */
    /* When is_ptr_to_mem_function is TRUE. */
    a_routine_ptr
		routine;
			/* Routine referred to by the pointer-to-member. */
  } variant;
} a_constexpr_ptr_to_mem;


static void init_interpreter_state(an_interpreter_state  *ips,
                                   a_boolean             is_constant_evaluated)
/*
Initialize the given interpreter state.  is_constant_evaluated determines the
result of calls to std::is_constant_evaluated().
*/
{
  init_data_map(&ips->map, 3);
  init_constexpr_stack(&ips->storage_stack);
  init_live_set(&ips->live_set);
  add_to_live_set(&ips->live_set, 1);
  ips->curr_call_frame = NULL;
  ips->extension_state = NULL;
  ips->constants = NULL;
  clear_diag_list(&ips->diag_list);
  ips->position = null_source_position;
  ips->cost = 0;
  ips->curr_alloc_seq_number = 1;
  ips->is_constant_evaluated = is_constant_evaluated;
  ips->is_variable_initializer = FALSE;
  ips->allow_reinterpret_cast = FALSE;
  ips->static_storage_ready = FALSE;
  ips->side_effects_disabled = !relaxed_constexpr_enabled;
  ips->suspend_diag_list = FALSE;
  ips->input_error = FALSE;
  ips->call_seen = FALSE;
  ips->permit_address_of_local_temporary = FALSE;
  ips->permit_null_pointer_offsets = (gpp_mode && !clang_mode) ||
                                     microsoft_mode;
  ips->static_lifetime_init = FALSE;
  ips->disallow_mutable_field_load = FALSE;
  ips->allow_consteval_routine_node = FALSE;
  ips->report_started = FALSE;
  ips->dyn_allocations = NULL;
  ips->srcloc_builtin_pos = NULL;
  n_active_interpreter_states += 1;
}  /* init_interpreter_state */


static void release_interpreter_state(an_interpreter_state  *ips)
/*
Release the storage allocated for the given interpreter state.
*/
{
  n_active_interpreter_states -= 1;
  release_constexpr_stack(&ips->storage_stack);
  release_data_map_table(&ips->map);
  ips->map.table = NULL;
  release_live_set_table(&ips->live_set);
  ips->live_set.table = NULL;
  { a_constant_ptr  cp = ips->constants;
    while (cp != NULL) {
      a_constant_ptr  next_cp = cp->next;
      release_local_constant(&cp);
      cp = next_cp;
    }  /* while */
  }
  if (ips->static_storage_ready) {
    release_constexpr_stack(&ips->static_storage);
  }  /* if */
  if (n_free_variant_path_entries != n_variant_path_entries &&
      n_active_interpreter_states == 0) {
    reclaim_variant_path_entries();
  }  /* if */
  if (ips->dyn_allocations != NULL) {
    a_constexpr_allocation_ptr
            alloc = ips->dyn_allocations, next_alloc;
    do {
      next_alloc = alloc->next;
      free_for_interpreter((void*)alloc, (sizeof_t)alloc->total_size);
      alloc = next_alloc;
    } while (alloc != NULL);
  }  /* if */
  if (ips->report_started) {
    fprintf(f_error, "\n%s\n", error_text(ec_constexpr_end_report));
  }  /* if */
}  /* release_interpreter_state */


static void info_call_stack(an_interpreter_state  *ips)
/*
Record diagnostic entries (on the list pointed to by ips->diag_list) describing
the interpreter's current call stack.
*/
{
  a_call_frame_ptr  frame = ((an_interpreter_state*)ips)->curr_call_frame;

  if (frame != NULL) {
    for (; frame->parent != NULL; frame = frame->parent) {
      a_routine_ptr  rp = frame->routine;
      if (rp == NULL) {
        /* Ignore frames not associated with an actual call (this can happen
           with GNU statement expressions). */
        continue;
      }  /* if */
      if (is_module_imported(rp)) {
        /* Ordinarily, we just issue a note pointing to the call site.
           However, for call sites imported from modules, we have no access to
           the source code and so it is useful to actually name the calling
           function. */
        more_info_sym_diagnostic(ec_constexpr_called_from_rout,
                                 frame->variant.position, symbol_for(rp),
                                 &ips->diag_list);
      } else {
        more_info_diagnostic(ec_constexpr_called_from, frame->variant.position,
                             &ips->diag_list);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* info_call_stack */


static void info_with_pos(an_error_code         err_code,
                          a_source_position     *pos,
                          an_interpreter_state  *ips)
/*
Record the given error code at the given position as a diagnostic annotation
for interpretation failure.  Also record annotations describing the call
stack.
*/
{
  if (!ips->suspend_diag_list) {
    more_info_diagnostic(err_code, pos, &ips->diag_list);
    info_call_stack(ips);
  }  /* if */
}  /* info_with_pos */


static void info_with_pos_type(an_error_code         err_code,
                               a_source_position     *pos,
                               a_type_ptr            tp,
                               an_interpreter_state  *ips)
/*
Record the given error code at the given position as a diagnostic annotation
for interpretation failure.  Also record annotations describing the call
stack.  Use the given type to replace fill-ins.
*/
{
  if (!ips->suspend_diag_list) {
    more_info_type_diagnostic(err_code, pos, tp, &ips->diag_list);
    info_call_stack(ips);
  }  /* if */
}  /* info_with_pos_type */


static void info_with_pos_type2(an_error_code         err_code,
                                a_source_position     *pos,
                                a_type_ptr            tp1,
                                a_type_ptr            tp2,
                                an_interpreter_state  *ips)
/*
Record the given error code at the given position as a diagnostic annotation
for interpretation failure.  Also record annotations describing the call
stack.  Use the given types to replace fill-ins.
*/
{
  if (!ips->suspend_diag_list) {
    more_info_type2_diagnostic(err_code, pos, tp1, tp2, &ips->diag_list);
    info_call_stack(ips);
  }  /* if */
}  /* info_with_pos_type2 */


static void info_with_pos_num(an_error_code         err_code,
                              a_source_position     *pos,
                              uint32_t              num,
                              an_interpreter_state  *ips)
/*
Record the given error code at the given position as a diagnostic annotation
for interpretation failure.  Also record annotations describing the call
stack.
*/
{
  if (!ips->suspend_diag_list) {
    more_info_num_diagnostic(err_code, pos, num, &ips->diag_list);
    info_call_stack(ips);
  }  /* if */
}  /* info_with_pos_num */


static void info_with_pos_num2(an_error_code         err_code,
                               a_source_position     *pos,
                               uint32_t              num1,
                               uint32_t              num2,
                               an_interpreter_state  *ips)
/*
Record the given error code at the given position as a diagnostic annotation
for interpretation failure.  Also record annotations describing the call
stack.
*/
{
  if (!ips->suspend_diag_list) {
    more_info_num2_diagnostic(err_code, pos, num1, num2, &ips->diag_list);
    info_call_stack(ips);
  }  /* if */
}  /* info_with_pos_num2 */


static void info_with_pos_sym(an_error_code         err_code,
                              a_source_position     *pos,
                              a_symbol_ptr          sym,
                              an_interpreter_state  *ips)
/*
Record the given error code at the given position as a diagnostic annotation
for interpretation failure.  Use sym for placeholder substitution in the
diagnostic string.  Also record annotations describing the call stack.
*/
{
  if (!ips->suspend_diag_list) {
    more_info_sym_diagnostic(err_code, pos, sym, &ips->diag_list);
    info_call_stack(ips);
  }  /* if */
}  /* info_with_pos_sym */


static void info_with_pos_sym_type(an_error_code         err_code,
                                   a_source_position     *pos,
                                   a_symbol_ptr          sym,
                                   a_type_ptr            type,
                                   an_interpreter_state  *ips)
/*
Record the given error code at the given position as a diagnostic annotation
for interpretation failure.  Use sym and type for placeholder substitution in
the diagnostic string.  Also record annotations describing the call stack.
*/
{
  if (!ips->suspend_diag_list) {
    more_info_sym_type_diagnostic(err_code, pos, sym, type, &ips->diag_list);
    info_call_stack(ips);
  }  /* if */
}  /* info_with_pos_sym_type */


static void info_with_pos_sym2(an_error_code         err_code,
                               a_source_position     *pos,
                               a_symbol_ptr          sym1,
                               a_symbol_ptr          sym2,
                               an_interpreter_state  *ips)
/*
Record the given error code at the given position as a diagnostic annotation
for interpretation failure.  Use sym1 and sym2 for placeholder substitution in
the diagnostic string.  Also record annotations describing the call stack.
*/
{
  if (!ips->suspend_diag_list) {
    more_info_sym2_diagnostic(err_code, pos, sym1, sym2, &ips->diag_list);
    info_call_stack(ips);
  }  /* if */
}  /* info_with_pos_sym2 */


static a_byte_count f_value_bytes_for_type(an_interpreter_state  *ips,
                                           a_type_ptr            tp,
                                           a_boolean             *p_result);


/*
Macro returning the number of bytes needed to represent a value of a given type
in interpreter storage.  In the case of a class type, the layout is computed if
needed.  Array or class types that are too large trigger an interpretation
failure (*ips is updated accordingly).
*/
#define value_bytes_for_type(ips, tp, result_flag)                           \
  ((tp)->kind == (a_type_kind)tk_integer ?                                   \
       sizeof(an_integer_value) :                                            \
   (tp)->kind == (a_type_kind)tk_float ?                                     \
       sizeof(an_internal_float_value) :                                     \
   /* else */                                                                \
    f_value_bytes_for_type(ips, tp, result_flag))

/*
Macro returning the number of bytes needed to represent the result of an
expression.  For glvalues, it is the size of a constexpr address.  For prvalues
it is the number of bytes needed to represent a value of the given type (tp) in
interpreter storage.
*/
#define expr_result_size(ips, expr, tp, p_result)                            \
  (((expr)->is_lvalue || (expr)->is_xvalue) ?                                \
      sizeof(a_constexpr_address) :                                          \
      value_bytes_for_type(ips, tp, p_result))


static void info_one_past_end_of_array(a_constexpr_address   *addr,
                                       an_expr_node_ptr      expr,
                                       an_interpreter_state  *ips)
/*
expr is an rvalue whose evaluation requires the indirection of addr, but it
turns out addr is pointing one position past the end of an array or a single
object treated as an array of one element.  Record diagnostic information
describing the problem.
*/
{
  if (is_array_element(addr)) {
    info_with_pos_num(ec_constexpr_access_one_past_array_end, &expr->position, 
                      addr->length, ips);
  } else {
    info_with_pos(ec_constexpr_access_past_object, &expr->position, ips);
  }  /* if */
}  /* info_one_past_end_of_array */


static a_byte_count lay_out_class_type(an_interpreter_state  *ips,
                                       a_type_ptr            tp,
                                       a_boolean             *p_result);
static a_byte_count lay_out_union_type(an_interpreter_state  *ips,
                                       a_type_ptr            tp,
                                       a_boolean             *p_result);

static an_error_code get_element_and_type_from_array(a_type_ptr    array_type,
                                                     a_type_ptr    *elem_type,
                                                     a_targ_size_t *n_elems)
/*
Helper routine to return (in *elem_type) the underlying element type as well
as (in *n_elems) the number of total elements in a potentially-multi-
dimensional array type (array_type).  Returns an appropriate error code if
the array type is not suitable for constexpr evaluation or ec_no_error.
*/
{
  an_error_code  result = ec_no_error;
  a_type_ptr     etp = array_type;

  *n_elems = 1;
  do {
    if (etp->variant.array.is_variable_size_array) {
      result = ec_constexpr_vla;
      break;
    } else if (etp->variant.array.is_template_dependent_size_array) {
      result = ec_constexpr_dependent_array_size;
      break;
    } else if (etp->variant.array.variant.number_of_elements == 0 &&
               !etp->variant.array.bound_is_zero) {
      result = ec_constexpr_access_to_runtime_storage;
      break;
    } else {
      *n_elems *= etp->variant.array.variant.number_of_elements;
      etp = etp->variant.array.element_type;
      etp = skip_typerefs(etp);
    }  /* if */
  } while (etp->kind == (a_type_kind)tk_array);
  *elem_type = etp;
  return result;
}  /* get_element_and_type_from_array */


static a_field_ptr next_alloc_field(a_field_ptr  field)
/*
Given a pointer to a field (or NULL), return a pointer to the first field at
or after the given field that is allocated as such in the interpreter.
Unnamed bit fields, for example, are not allocated, and are skipped by
initialization processing.  If there is no next such field, return NULL.
*/
{
  for (; field != NULL; field = field->next) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Skip any property or event fields. */
    if (field->property_or_event_descr != NULL) continue;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Skip fields generated by lowering. */
    if (symbol_for(field) == NULL && !field->is_captured_this) continue;
    /* Unnamed bit fields are not initializable. */
    if (has_name(field) || !field->is_bit_field) break;
    /* Anonymous unions are also initializable in C++.  An extension allows
       anonymous parent objects in C too. */
    if (field->is_anonymous_parent_object) break;
  }  /* for */
  return field;
}  /* next_alloc_field */


static a_boolean has_dependent_layout(a_type_ptr  class_type)
/*
Return TRUE if the given class type has a template-dependent layout within the
interpreter.
*/
{
  a_boolean         result = FALSE;
  a_base_class_ptr  bcp = base_classes_of(class_type);

  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    if (has_dependent_layout(bcp->type)) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  if (!result) {
    a_field_ptr  fp = class_type->variant.class_struct_union.field_list;
    fp = next_alloc_field(fp);
    for (; fp != NULL; fp = next_alloc_field(fp->next)) {
      a_type_ptr  tp = skip_typerefs(skip_array_types(fp->type));
      if (is_immediate_class_type(fp->type)) {
        if (has_dependent_layout(fp->type)) {
          result = TRUE;
          break;
        }  /* if */
      } else if (is_template_dependent_type(tp)) {
        result = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* has_dependent_layout */


static a_byte_count f_value_bytes_for_type(an_interpreter_state  *ips,
                                           a_type_ptr            tp,
                                           a_boolean             *p_result)
/*
Return the number of bytes needed to represent a value of the given type.
Always called through the macro value_bytes_for_type, which handles some common
type kinds.  If the number of bytes is too large, interpretation fails, and
*p_result is set to FALSE.
*/
{
  a_byte_count  result;

redo:
  switch (tp->kind) {
    case tk_void:
      result = 0;
      break;
    case tk_integer:
      result = sizeof(an_integer_value);
      break;
    case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      result = sizeof(an_internal_float_value);
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
      result = sizeof(an_internal_complex_value);
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_routine:
    case tk_pointer:
    case tk_nullptr:
      result = sizeof(a_constexpr_address);
      break;
    case tk_array:
      { a_targ_size_t  n_elems;
        a_type_ptr     etp;
        an_error_code  err_code = ec_no_error;
        err_code = get_element_and_type_from_array(tp, &etp, &n_elems);
        if (err_code == ec_no_error) {
          result = value_bytes_for_type(ips, etp, p_result);
          if (!*p_result) {
            /* Interpretation failure. */
          } else if (n_elems > MAX_CONSTEXPR_TYPE_SIZE/result ||
                     n_elems > MAX_ARRAY_LENGTH) {
            /* Too many elements. */
#if DEBUG
            check_assertion(ips != NULL);
#endif /* DEBUG */
            info_with_pos_type(ec_constexpr_type_too_large, type_pos(tp, ips),
                               tp, ips);
            do_constexpr_fail(*p_result);
            result = MAX_CONSTEXPR_TYPE_SIZE+1;
          } else {
            result *= (a_byte_count)n_elems;
          }  /* if */
        } else {
#if DEBUG
          check_assertion(ips != NULL);
#endif /* DEBUG */
          info_with_pos(err_code, type_pos(tp, ips), ips);
          do_constexpr_fail(*p_result);
          result = MAX_CONSTEXPR_TYPE_SIZE+1;
        }  /* if */
      }  /* if */
      break;
    case tk_class:
    case tk_struct:
      get_mapped_byte_count(&persistent_map, tp, result);
      if (result == 0) {
#if DEBUG
        check_assertion(ips != NULL);
#endif /* DEBUG */
        if (tp->variant.class_struct_union.is_nonreal_class &&
            has_dependent_layout(tp)) {
          /* A nonreal class type.  Normally, we should not attempt to handle
             any nonreal class types, but due to a current limitation of the
             front end some class types are currently marked as nonreal even
             though they are not really dependent.  For example, the closure
             type in:
               template<int = []{ return 1; }()>  int f();
             The has_dependent_layout condition allows us to nonetheless
             handle such cases. */
          info_with_pos_type(ec_constexpr_type_invalid, &ips->position, tp,
                             ips);
          do_constexpr_fail(*p_result);
          result = MAX_CONSTEXPR_TYPE_SIZE+1;
        } else if (!tp->incomplete) {
          result = lay_out_class_type(ips, tp, p_result);
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (tp == type_of_guid) {
          /* Although _GUID is a predefined incomplete type, some lvalues can
             have a _GUID type and do_constexpr_expression therefore expects
             to be able to determine its size.  Since _GUID is implemented as
             four integer values of various sizes, we compute a matching
             size. */
          result = sizeof(a_type_ptr);
          do_host_alignment(result);
          result += 4*sizeof(an_integer_value);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else {
          info_with_pos_type(ec_constexpr_type_too_large, type_pos(tp, ips),
                             tp, ips);
          do_constexpr_fail(*p_result);
        }  /* if */
      } else if (result > MAX_CONSTEXPR_TYPE_SIZE) {
        a_source_position  *pos = &tp->source_corresp.decl_position;
#if DEBUG
        check_assertion(ips != NULL);
#endif /* DEBUG */
        info_with_pos_type(ec_constexpr_incomplete_type, pos, tp, ips);
        do_constexpr_fail(*p_result);
      }  /* if */
      break;
    case tk_union:
      get_mapped_byte_count(&persistent_map, tp, result);
#if DEBUG
      check_assertion(ips != NULL);
#endif /* DEBUG */
      if (result == 0) {
        if (tp->variant.class_struct_union.is_nonreal_class &&
            has_dependent_layout(tp)) {
          info_with_pos_type(ec_constexpr_type_invalid, &ips->position, tp,
                             ips);
          do_constexpr_fail(*p_result);
          result = MAX_CONSTEXPR_TYPE_SIZE+1;
        } else if (!tp->incomplete) {
          result = lay_out_union_type(ips, tp, p_result);
        } else {
          a_source_position  *pos = &tp->source_corresp.decl_position;
          info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
          do_constexpr_fail(*p_result);
        }  /* if */
      } else if (result > MAX_CONSTEXPR_TYPE_SIZE) {
        a_source_position  *pos = &tp->source_corresp.decl_position;
        info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
        do_constexpr_fail(*p_result);
      }  /* if */
      break;
    case tk_typeref:
      tp = tp->variant.typeref.type;
      goto redo;
    case tk_ptr_to_member:
      result = sizeof(a_constexpr_ptr_to_mem);
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      { a_type_ptr     etp = skip_typerefs(tp->variant.vector.element_type);
        a_targ_size_t  n_elems = tp->size/etp->size;
        result = value_bytes_for_type(ips, etp, p_result);
        if (!*p_result) {
          /* Interpretation failure. */
        } else if (n_elems > MAX_CONSTEXPR_TYPE_SIZE/result ||
                   n_elems > MAX_ARRAY_LENGTH) {
          /* Too many elements. */
          a_source_position  *pos = &tp->source_corresp.decl_position;
#if DEBUG
          check_assertion(ips != NULL);
#endif /* DEBUG */
          if (pos->seq == 0) pos = &ips->position;
          info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
          do_constexpr_fail(*p_result);
        } else {
          result *= (a_byte_count)n_elems;
        }  /* if */
      }
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:       /* All fixed-point types. */
      /* These types are not supported by the interpreter. */
      { a_source_position  *pos = &tp->source_corresp.decl_position;
#if DEBUG
        check_assertion(ips != NULL);
#endif /* DEBUG */
        if (pos->seq == 0) pos = &ips->position;
        info_with_pos_type(ec_constexpr_type_invalid, pos, tp, ips);
        do_constexpr_fail(*p_result);
        result = MAX_CONSTEXPR_TYPE_SIZE+1;
      }
      break;
#endif /* FIXED_POINT_ALLOWED */
    case tk_error:
#if DEBUG
      check_assertion(ips != NULL);
#endif /* DEBUG */
      ips->input_error = TRUE;
      FALLTHROUGH
    case tk_template_param:
    case tk_unknown:
      /* Fail interpretation. */
      info_with_pos_type(ec_constexpr_type_invalid, &ips->position, tp, ips);
      do_constexpr_fail(*p_result);
      result = MAX_CONSTEXPR_TYPE_SIZE+1;
      break;
    default:
      /* These types should never be encountered by the interpreter. */
      /* The GNU optimizer complains if result is not assigned a value on
         this branch. */
      result = 0;
      unexpected_condition();
  }  /* switch */
  return result;
}  /* f_value_bytes_for_type */


static a_byte_count lay_out_class_type(an_interpreter_state  *ips,
                                       a_type_ptr            tp,
                                       a_boolean             *p_result)
/*
Compute and return the size of the given non-union class type.  Also record
offsets in any associated fields as well as any direct or virtual base classes.
If needed, this will recursively lay out types this class type is composed of.
ips is used to record an interpretation failure if the size exceeds the
interpreter's limits; in that case, *p_result is set to FALSE.
*/
{
  a_byte_count      total_size = 0;
  a_field_ptr       fp;
  a_base_class_ptr  bcp, bases = base_classes_of(tp);
  a_boolean         any_virtual_bases =
                      tp->variant.class_struct_union.any_virtual_base_classes;

  /* Allocate a pointer recording the next derivation step (NULL for a
     complete class). */
  total_size += sizeof(a_type_ptr);
  /* Allocate each proper field and record the field offsets. */
  fp = tp->variant.class_struct_union.field_list;
  for (fp = next_alloc_field(fp);
       fp != NULL;
       fp = next_alloc_field(fp->next)) {
    if (fp->compiler_generated && !fp->is_anonymous_parent_object) {
      /* Ignore fields generated by prelowering. */
      continue;
    }  /* if */
    do_host_alignment(total_size);
    map_byte_count(&persistent_map, fp, total_size);
    if (tp->variant.class_struct_union.contains_flexible_array_member &&
        type_is(fp->type, tk_array) && (gnu_mode && !clang_mode)) {
      /* A flexible array member.  GCC allows those in constant expressions,
         but Clang does not.  Treat this as a zero-length field. */
      continue;
    }  /* if */
    total_size += value_bytes_for_type(ips, fp->type, p_result);
    if (total_size > MAX_CONSTEXPR_TYPE_SIZE) {
      if (*p_result) {
        info_with_pos_type(ec_constexpr_type_too_large, type_pos(tp, ips), tp,
                            ips);
        do_constexpr_fail(*p_result);
      }  /* if */
      total_size = MAX_CONSTEXPR_TYPE_SIZE+1;
      goto done;
    }  /* if */
  }  /* for */
  /* Allocate each direct, nonvirtual base class. */
  for (bcp = bases; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct && !bcp->is_virtual) {
      a_byte_count  size_without_virtual_bases;
      a_type_ptr    btp = bcp->type;
      do_host_alignment(total_size);
      map_byte_count(&persistent_map, bcp, total_size);
      (void)value_bytes_for_type(ips, btp, p_result);
      if (!*p_result) {
        total_size = MAX_CONSTEXPR_TYPE_SIZE+1;
        goto done;
      }  /* if */
      get_mapped_byte_count(&persistent_map,
                            &btp->variant.class_struct_union.extra_info,
                            size_without_virtual_bases);
      total_size += size_without_virtual_bases;
      if (total_size > MAX_CONSTEXPR_TYPE_SIZE) {
        info_with_pos_type(ec_constexpr_type_too_large, type_pos(tp, ips), tp,
                           ips);
        do_constexpr_fail(*p_result);
        total_size = MAX_CONSTEXPR_TYPE_SIZE+1;
        goto done;
      }  /* if */
    }  /* if */
  }  /* for */
  /* Associate the size without the virtual bases with the "extra_info"
     field. */
  map_byte_count(&persistent_map, &tp->variant.class_struct_union.extra_info,
                 total_size);
  if (any_virtual_bases) {
    /* Allocate virtual base classes. */
    for (bcp = bases; bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual) {
        do_host_alignment(total_size);
        map_byte_count(&persistent_map, bcp, total_size);
        total_size += value_bytes_for_type(ips, bcp->type, p_result);
        if (total_size > MAX_CONSTEXPR_TYPE_SIZE) {
          if (*p_result) {
            info_with_pos_type(ec_constexpr_type_too_large, type_pos(tp, ips),
                               tp, ips);
            do_constexpr_fail(*p_result);
          }  /* if */
          total_size = MAX_CONSTEXPR_TYPE_SIZE+1;
          goto done;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
done:
  map_byte_count(&persistent_map, tp, total_size);
  return total_size;
}  /* lay_out_class_type */


static a_byte_count lay_out_union_type(an_interpreter_state  *ips,
                                       a_type_ptr            tp,
                                       a_boolean             *p_result)
/*
Return the size that should be allocated for the given union type, and record
the offsets of its fields.  If needed, this will recursively lay out the types
of the fields.  ips is used to record an interpretation failure if the size
exceeds the interpreter's limits; in that case, *p_result is set to FALSE.
*/
{
  a_byte_count      prefix_size = 0, max_field_size = 0, total_size;
  a_field_ptr       fp;

  /* Determine the size of the prefix indicating which field is active (NULL if
     none). */
  prefix_size += sizeof(a_field_ptr);
  do_host_alignment(prefix_size);
  /* Determine the size of the largest field and record the field offsets. */
  fp = tp->variant.class_struct_union.field_list;
  for (; fp != NULL; fp = fp->next) {
    a_byte_count  field_size = value_bytes_for_type(ips, fp->type, p_result);
    if (!*p_result) {
      total_size = MAX_CONSTEXPR_TYPE_SIZE+1;
      goto done;
    }  /* if */
    map_byte_count(&persistent_map, fp, prefix_size);
    if (field_size > max_field_size) max_field_size = field_size;
  }  /* for */
  total_size = prefix_size+max_field_size;
  if (total_size >= MAX_CONSTEXPR_TYPE_SIZE) {
    info_with_pos_type(ec_constexpr_type_too_large, type_pos(tp, ips), tp,
                       ips);
    *p_result = TRUE;
    total_size = MAX_CONSTEXPR_TYPE_SIZE+1;
  }  /* if */
done:
  map_byte_count(&persistent_map, tp, total_size);
  return total_size;
}  /* lay_out_union_type */


static void find_subobject_for_interpreter_address(
                                        an_interpreter_state  *ips,
                                        a_constexpr_address   *cap,
                                        a_byte                *parent_address,
                                        a_type_ptr            parent_type,
                                        a_field_ptr           *p_field,
                                        a_base_class_ptr      *p_bcp)
/*
cap represents an interpreter address pointing into interpreter storage for an
object X of type parent_type stored at parent_address (not necessarily a
complete object).  Return the direct subobject of X that cap points to (or
"into") via *p_field and *p_bcp (for field subobjects *p_bcp is set to NULL;
for base class subobjects *p_field is set to NULL).  If cap points "one past
the end" of a field subobject, that field is returned.
*/
{
  if (!type_is(parent_type, tk_union)) {
    /* Search among base classes and fields for the one that covers the offset
       of the given address.  We search through direct subobjects in allocation
       order. */
    a_field_ptr       fp = parent_type->variant.class_struct_union.field_list;
    a_field_ptr       last_fp = next_alloc_field(fp);
    a_base_class_ptr  bcp, last_bcp;
    a_boolean         okay = TRUE, check_virtual_bases = FALSE;
    a_byte_count      offset = (a_byte_count)(cap->address - parent_address),
                      sub_offset, type_size;
    sub_offset = sizeof(a_type_ptr);
    do_host_alignment(sub_offset);
    /* First search through the fields. */
    if (last_fp == NULL) {
      /* There are no allocated fields: Look among the base classes. */
      goto search_base_subobjects;
    }  /* if */
    for (fp = next_alloc_field(last_fp->next);
         fp != NULL;
         last_fp = fp, fp = next_alloc_field(fp->next)) {
      get_mapped_byte_count(&persistent_map, fp, sub_offset);
      if (offset < sub_offset ||
          (offset == sub_offset && cannot_dereference(cap))) {
        *p_field = last_fp;
        *p_bcp = NULL;
        goto done;
      }  /* if */
    }  /* for */
    type_size = value_bytes_for_type(ips, last_fp->type, &okay);
    if (offset-sub_offset < type_size ||
        (offset-sub_offset == type_size && cannot_dereference(cap))) {
      check_assertion(okay);
      *p_field = last_fp;
      *p_bcp = NULL;
      goto done;
    }  /* if */
search_base_subobjects:
    /* Next search through direct base classes.  (If we got here, there must be
       some).  Start with nonvirtual bases. */
    bcp = base_classes_of(parent_type);
    check_assertion(bcp != NULL);
    last_bcp = NULL;
    for (; bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual) {
        check_virtual_bases = TRUE;
      } else if (bcp->direct) {
        get_mapped_byte_count(&persistent_map, bcp, sub_offset);
        if (last_bcp != NULL) {
          if (offset < sub_offset) {
            *p_field = NULL;
            *p_bcp = last_bcp;
            goto done;
          }  /* if */
        }  /* if */
        last_bcp = bcp;
      }  /* if */
    }  /* for */
    if (check_virtual_bases) {
      for (bcp = base_classes_of(parent_type); bcp != NULL; bcp = bcp->next) {
        if (bcp->is_virtual) {
          get_mapped_byte_count(&persistent_map, bcp, sub_offset);
          if (last_bcp != NULL) {
            if (offset < sub_offset) {
              *p_field = NULL;
              *p_bcp = last_bcp;
              goto done;
            }  /* if */
          }  /* if */
          last_bcp = bcp;
        }  /* if */
      }  /* for */
    }  /* if */
    type_size = value_bytes_for_type(ips, last_bcp->type, &okay);
    if (offset-sub_offset < type_size ||
        (offset-sub_offset == type_size && cannot_dereference(cap))) {
      check_assertion(okay);
      *p_field = NULL;
      *p_bcp = last_bcp;
      goto done;
    }  /* if */
  } else {
    /* If the parent type is a union, a variant path must be available.  Just
       return the entry on the variant path with a field whose base address
       is parent_address. */
    a_variant_path_entry_ptr  vpep;
    check_assertion(is_variant_path(cap));
    vpep = cap->variant.variant_path->next;
    for (; vpep != NULL; vpep = vpep->next) {
      a_field_ptr  fp = vpep->field;
      if (fp != NULL && vpep->base_address == parent_address) {
        *p_field = fp;
        *p_bcp = NULL;
        goto done;
      }  /* if */
    }  /* for */
  }  /* if */
  /* We should never get here (our search should always be successful). */
  unexpected_condition();
done:;
}  /* find_subobject_for_interpreter_address */


#define record_subobject_derivation(subobj_ptr, bcp)                         \
  (*(void**)(subobj_ptr) = (void*)(bcp))

/*
Class type subobjects record their relationship to the next-more-derived
subobject with a base class entry pointer, or NULL for a most-derived object.
The following macro records that NULL (for proper base subobjects, the base
class entry pointer is recorded when the base subobject is initialized).  For
unions, the recorded pointer represents the active field rather than a base
class entry.  (This must therefore be invoked before placing a result in the
indicated storage, because that result could set the active field.)
*/
#define mark_complete_class_object_if_needed(utp, storage_ptr)               \
  if (is_immediate_class_type(utp)) {                                        \
    record_subobject_derivation(storage_ptr, NULL);                          \
  }  /* if */                                                                \

#if DEBUG && TRACK_INTERPRETER_ALLOCATIONS

static a_data_map
		object_alloc_map;
			/* Map tracking allocations of complete objects. */

static a_byte_count
		object_alloc_to_intercept;
			/* The allocation number to intercept (0 if none). */


static void object_alloc_intercept(void)
/*
Called if the object indicated by object_alloc_to_intercept is allocated.
Place a breakpoint on this function to find out when a certain complete object
is allocated.
*/
{
  fprintf(f_debug, "Allocated interpreter object #%u\n",
          object_alloc_to_intercept);
}  /* object_alloc_intercept */


static void track_complete_object_alloc(a_byte  *ptr)
/*
Record the given pointer to a just-allocated complete object in a map along
with a unique integer value.  That unique value can be retrieved by calling
db_object_alloc_num and can be assigned to object_alloc_to_intercept when the
front end starts up to find where the object is originally allocated.
*/
{
  static a_boolean     map_ready = FALSE;
  static a_byte_count  alloc_num = 0, prev_num;

  if (!map_ready) {
    init_data_map(&object_alloc_map, /*mask_width=*/5U);
    map_ready = TRUE;
  }  /* if */
  alloc_num += 1;
  if (alloc_num == object_alloc_to_intercept) {
    object_alloc_intercept();
  }  /* if */
  get_mapped_byte_count(&object_alloc_map, ptr, prev_num);
  if (prev_num != 0) {
    unmap_ptr(&object_alloc_map, ptr);
  }  /* if */
  map_byte_count(&object_alloc_map, ptr, alloc_num);
}  /* track_complete_object_alloc */


unsigned long db_object_alloc_num(a_byte  *ptr)
/*
Return the unique integer value associated with ptr by
track_complete_object_alloc.
*/
{
  a_byte_count  alloc_num;
  get_mapped_byte_count(&object_alloc_map, ptr, alloc_num);
  return (unsigned long)alloc_num;
}  /* db_object_alloc_num */

#else /* !(DEBUG && TRACK_INTERPRETER_ALLOCATIONS) */
#define track_complete_object_alloc(ptr)  /*Nothing*/
#endif /* DEBUG && TRACK_INTERPRETER_ALLOCATIONS */


/*
Record the type of a complete object in its prefix.
*/
#define record_complete_object_type(utp, data_ptr)                           \
  track_complete_object_alloc(data_ptr);                                     \
  (*(a_type_ptr*)(data_ptr-sizeof(a_type_ptr)) = (utp))

/*
Retrieve the type of a complete object in its prefix.
*/
#define complete_object_type(data_ptr)                                       \
  (*(a_type_ptr*)(data_ptr-sizeof(a_type_ptr)))

/*
Compute the number of bytes needed to hold an initialization bitmap for a
given number of bytes.
*/
#define compute_bitmap_size(n_bytes)                                         \
  (((n_bytes)+CHAR_BIT-1)/CHAR_BIT)


/*
Given a type utp and the size n_bytes required to represent its value, compute
the number of bytes needed as a bookkeeping prefix for a complete object of
that type.
*/
#define compute_prefix_size_for_type(utp, n_bytes, prefix_size)              \
{                                                                            \
  a_byte_count  bitmap_size;                                                 \
  if (is_immediate_class_type(utp) ||                                        \
      utp->kind == (a_type_kind)tk_array) {                                  \
    bitmap_size = compute_bitmap_size(n_bytes);                              \
  } else {                                                                   \
    bitmap_size = 0;                                                         \
  }  /* if */                                                                \
  prefix_size = 1+sizeof(a_type_ptr)+bitmap_size;                            \
  do_host_alignment(prefix_size);                                            \
}


#if DEBUG
#define debug_scramble(ptr, n_bytes) (void)memset(ptr, 0xdb, n_bytes)
#else /* !DEBUG */
#define debug_scramble(ptr, n_bytes) /*nothing*/
#endif /* DEBUG */



/*
Allocate a complete object of type utp and size n_bytes in the interpreter's
storage stack, including prefix storage to keep bookkeeping information.
Initialize the prefix and mark the object as being complete (see
mark_complete_class_object_if_needed above).  n_bytes must be positive.
storage_ptr is set to the data portion of the allocated storage (i.e., the
first byte after the prefix).
*/
#define alloc_complete_object(ips, n_bytes, utp, storage_ptr)                \
{                                                                            \
  a_byte_count  total_size, prefix_size;                                     \
  a_byte        *ptr, *data_ptr;                                             \
  compute_prefix_size_for_type(utp, n_bytes, prefix_size);                   \
  total_size = prefix_size+n_bytes;                                          \
  alloc_stack_bytes(ips, total_size, ptr);                                   \
  memzero((char*)ptr, size_t_arg(prefix_size-sizeof(a_type_ptr)));           \
  data_ptr = ptr+prefix_size;                                                \
  debug_scramble(data_ptr, n_bytes);                                         \
  record_complete_object_type(utp, data_ptr);                                \
  (storage_ptr) = data_ptr;                                                  \
  mark_complete_class_object_if_needed(utp, data_ptr);                       \
}

/*
Allocate a complete object of type utp in the interpreter's static storage
area.  The static storage is zeroed.
*/
#define alloc_static_object(ips, utp, storage_ptr, p_result)                 \
{                                                                            \
  a_byte_count  data_size, total_size, prefix_size;                          \
  a_byte        *ptr, *data_ptr;                                             \
  data_size = value_bytes_for_type(ips, utp, p_result);                      \
  if (*p_result) {                                                           \
    compute_prefix_size_for_type(utp, data_size, prefix_size);               \
    do_host_alignment(data_size);                                            \
    total_size = prefix_size+data_size+sizeof(a_var_postfix);                \
    alloc_static_bytes(ips, total_size, ptr);                                \
    memzero((char*)ptr, size_t_arg(total_size));                             \
    data_ptr = ptr+prefix_size;                                              \
    ((a_var_postfix*)(data_ptr+data_size))->alloc_seq_number = 0;            \
    record_complete_object_type(utp, data_ptr);                              \
    (storage_ptr) = data_ptr;                                                \
    mark_complete_class_object_if_needed(utp, data_ptr);                     \
  }  /* if */                                                                \
}

#if BUILTIN_FUNCTIONS_ENABLED

/*
The naturalizable functions below while not specifically tied to builtin
functions are currently only used by builtin functions.  Thus, only include
them when builtin functions are enabled.
*/

/*
The offset from the complete object pointer.
*/
#define NATURALIZABLE_KEY_OFFSET   ((a_byte)0x01)


static void mark_naturalizable_object(an_interpreter_state  *ips,
                                      a_byte                *storage_ptr)
/*
Mark the given interpreter storage as eligible for naturalization into a
runtime constant.
*/
{
  map_ptr(&ips->map, storage_ptr - NATURALIZABLE_KEY_OFFSET,
          (a_byte*)&ips->static_storage);
}  /* mark_naturalizable_object */


static void alloc_naturalizable_object(an_interpreter_state  *ips,
                                       a_type_ptr            ty_ptr,
                                       a_byte                **storage_ptr,
                                       a_boolean             *p_result)
/*
Allocate a complete object of type ty_ptr in the interpreter's storage that's
eligible for naturalization into a runtime constant.  The storage is zeroed.
If a problem occurs during allocation, *p_result is set to FALSE.
*/
{
  alloc_static_object(ips, ty_ptr, *storage_ptr, p_result);
  if (*p_result) {
    mark_naturalizable_object(ips, *storage_ptr);
  }  /* if */
}  /* alloc_naturalizable_object */


static a_boolean is_naturalizable_object(
                                        an_interpreter_state  *ips,
                                        a_byte                *complete_object)
/*
Return TRUE if the given complete object can be naturalized, otherwise return
FALSE.
*/
{
  a_byte  *mapped_bytes = NULL;

  get_mapped_ptr(&ips->map, complete_object - NATURALIZABLE_KEY_OFFSET,
                 mapped_bytes);
  return mapped_bytes == (a_byte*)&ips->static_storage;
}  /* is_naturalizable_object */

#endif /* BUILTIN_FUNCTIONS_ENABLED */

/*
Get the position of the bit representing whether a given byte position is
initialized.
*/
#define get_init_bit_pos(offset, byte_pos, bit_pos)                          \
  { byte_pos = (offset)/CHAR_BIT + sizeof(a_type_ptr)+2;                     \
    bit_pos = (offset)%CHAR_BIT; }

/*
Mark a subobject within a given complete object as initialized.
*/
#define mark_subobject_initialized(subobj, complete_obj)                     \
{                                                                            \
  a_byte        *start_byte = (complete_obj);                                \
  a_byte_count  byte_pos, bit_pos, off = (a_byte_count)((subobj)-start_byte);\
  get_init_bit_pos(off, byte_pos, bit_pos);                                  \
  start_byte[-(int)byte_pos] |= (a_byte)(1<<bit_pos);                        \
}


static void mark_whole_subobject_initialized(
                                          an_interpreter_state  *ips,
                                          a_byte                *subobj,
                                          a_type_ptr            tp,
                                          a_byte                *complete_obj)
/*
If tp is a scalar type, this routine does the same work as
mark_subobject_initialized.  If tp is a class or array type, it marks the
indicated subobject and all its subobject as initialized.
*/
{
  if (is_immediate_class_type(tp) || tp->kind == (a_type_kind)tk_array) {
    a_boolean     result = TRUE;
    a_byte_count  n_bytes = value_bytes_for_type(ips, tp, &result);
    a_byte_count  off = (a_byte_count)(subobj - complete_obj);
    a_byte_count  byte_pos, bit_pos;
    get_init_bit_pos(off, byte_pos, bit_pos);
    while (n_bytes != 0) {
      if (bit_pos == 0 && n_bytes >= CHAR_BIT) {
        /* Mark a whole byte at a time. */
        complete_obj[-(int)byte_pos] = ~(a_byte)0;
        byte_pos += 1;
        n_bytes -= CHAR_BIT;
      } else {
        complete_obj[-(int)byte_pos] |= (a_byte)(1<<bit_pos);
        bit_pos += 1;
        if (bit_pos == CHAR_BIT) {
          bit_pos = 0;
          byte_pos += 1;
        }  /* if */
        n_bytes -= 1;
      }  /* if */
    }  /* while */
  } else {
    mark_subobject_initialized(subobj, complete_obj);
  }  /* if */
}  /* mark_whole_subobject_initialized */


/*
Mark a subobject within a given complete object as uninitialized.
*/
#define mark_subobject_uninitialized(subobj, complete_obj)                   \
{                                                                            \
  a_byte        *start_byte = (complete_obj);                                \
  a_byte_count  byte_pos, bit_pos, off = (a_byte_count)((subobj)-start_byte);\
  get_init_bit_pos(off, byte_pos, bit_pos);                                  \
  start_byte[-(int)byte_pos] &= ~(a_byte)(1<<bit_pos);                       \
}


static a_boolean mark_whole_subobject_uninitialized(
                                          an_interpreter_state  *ips,
                                          a_byte                *subobj,
                                          a_type_ptr            tp,
                                          a_byte                *complete_obj)
/*
If tp is a scalar type, this routine does the same work as
mark_subobject_uninitialized.  If tp is a class or array type, it marks the
indicated subobject and all its subobjects as uninitialized.  Return TRUE if
successful, FALSE if not (e.g., if the subobject type included a member with
an error type).
*/
{
  a_boolean  result = TRUE;

  if (is_immediate_class_type(tp) || tp->kind == (a_type_kind)tk_array) {
    a_byte_count  n_bytes = value_bytes_for_type(ips, tp, &result);
    if (result) {
      a_byte_count  off = (a_byte_count)(subobj - complete_obj);
      a_byte_count  byte_pos, bit_pos;
      get_init_bit_pos(off, byte_pos, bit_pos);
      while (n_bytes != 0) {
        if (bit_pos == 0 && n_bytes >= CHAR_BIT) {
          /* Mark a whole byte at a time. */
          complete_obj[-(int)byte_pos] = (a_byte)0;
          byte_pos += 1;
          n_bytes -= CHAR_BIT;
        } else {
          complete_obj[-(int)byte_pos] &= ~(a_byte)(1<<bit_pos);
          bit_pos += 1;
          if (bit_pos == CHAR_BIT) {
            bit_pos = 0;
            byte_pos += 1;
          }  /* if */
          n_bytes -= 1;
        }  /* if */
      }  /* while */
    }  /* if */
  } else {
    mark_subobject_uninitialized(subobj, complete_obj);
  }  /* if */
  return result;
}  /* mark_whole_subobject_uninitialized */


static void init_subobject_to_zero(an_interpreter_state  *ips,
                                   a_byte                *subobj,
                                   a_type_ptr            tp,
                                   a_byte                *complete_obj)
/*
Initialize to zero the given subobject of the given type (the subobject is
within the given complete object).
*/
{
  switch (tp->kind) {
    case tk_integer:
      *((an_integer_value*)subobj) = zero_int;
      break;
    case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      *fp_value(subobj) = zero_flt[(int)tp->variant.float_kind];
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
      cx_value(subobj)->real = zero_flt[(int)tp->variant.float_kind];
      cx_value(subobj)->imag = zero_flt[(int)tp->variant.float_kind];
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_pointer:
    case tk_nullptr:
      clear_address(subobj, (a_byte*)0);
      break;
    case tk_array:
      { a_type_ptr     etp = skip_typerefs(tp->variant.array.element_type);
        a_targ_size_t  n_elems, k;
        a_byte_count   elem_size;
        a_boolean      result = TRUE;
        n_elems = tp->variant.array.variant.number_of_elements;
        elem_size = value_bytes_for_type(ips, etp, &result);
        if (!result) goto done;
        for (k = 0; k<n_elems; k += 1) {
          init_subobject_to_zero(ips, subobj, etp, complete_obj);
          subobj += elem_size;
        }  /* for */
      }
      /* Bypass the code that would mark the object pointed to by "subobject"
         as initialized: */
      goto done;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      { a_type_ptr     etp = skip_typerefs(tp->variant.vector.element_type);
        a_targ_size_t  k, n_elems = tp->size/etp->size;
        a_boolean      result = TRUE;
        a_byte_count   elem_size = value_bytes_for_type(ips, etp, &result);
        check_assertion(result);
        for (k = 0; k<n_elems; k += 1) {
          init_subobject_to_zero(ips, subobj, etp, complete_obj);
          subobj += elem_size;
        }  /* for */
      }
      /* Bypass the code that would mark the object pointed to by "subobject"
         as initialized: */
      goto done;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tk_class:
    case tk_struct:
      { /* Initialize fields and bases. */
        a_base_class_ptr  bcp = base_classes_of(tp);
        a_field_ptr       fp = tp->variant.class_struct_union.field_list;
        fp = next_alloc_field(fp);
        for (; fp != NULL; fp = next_alloc_field(fp->next)) {
          a_type_ptr    ftp = skip_typerefs(fp->type);
          a_byte_count  offset;
          get_mapped_byte_count(&persistent_map, fp, offset);
          init_subobject_to_zero(ips, subobj+offset, ftp, complete_obj);
        }  /* for */
        for (; bcp != NULL; bcp = bcp->next) {
          if (bcp->direct || bcp->is_virtual) {
            a_byte_count  offset;
            get_mapped_byte_count(&persistent_map, bcp, offset);
            init_subobject_to_zero(ips, subobj+offset, bcp->type,
                                   complete_obj);
            record_subobject_derivation(subobj+offset, bcp);
          }  /* if */
        }  /* for */
      }
      break;
    case tk_union:
      { /* Initialize the first field (if any). */
        a_field_ptr  fp = tp->variant.class_struct_union.field_list;
        fp = next_alloc_field(fp);
        if (fp != NULL) {
          a_byte_count  offset;
          a_type_ptr    ftp = skip_typerefs(fp->type);
          get_mapped_byte_count(&persistent_map, fp, offset);
          init_subobject_to_zero(ips, subobj+offset, ftp, complete_obj);
          /* Record the active field. */
          *(a_field_ptr*)subobj = fp;
        } else {
          *(a_field_ptr*)subobj = NULL;
        }  /* if */
      }
      break;
    case tk_ptr_to_member:
      { /* Initialize the first field (if any). */
        a_constexpr_ptr_to_mem  *pm_value = (a_constexpr_ptr_to_mem*)subobj;
        a_type_ptr              mem_type = tp->variant.ptr_to_member.type;
        mem_type = skip_typerefs(mem_type);
        if (mem_type->kind == (a_type_kind)tk_routine) {
          pm_value->is_ptr_to_mem_function = TRUE;
          pm_value->variant.routine = NULL;
        } else {
          pm_value->is_ptr_to_mem_function = FALSE;
          pm_value->variant.field = NULL;
        }  /* if */
        pm_value->subtract_adjustment = FALSE;
        pm_value->this_class_adjustment = 0;
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
  mark_subobject_initialized(subobj, complete_obj);
done:;
}  /* init_subobject_to_zero */

/*
Return TRUE if the given complete object is fully initialized.
*/
#define complete_object_is_initialized(complete_object)                      \
  complete_obj_flag(complete_object, COMPLETE_OBJ_INITIALIZED)


/*
Produce TRUE if the object pointed into by "cap" is initialized.
*/
#define is_initialized(cap)                                                  \
  (complete_object_is_initialized((cap)->complete_object) ||                 \
   subobject_is_initialized((cap)->address, (cap)->complete_object))


static a_boolean subobject_is_initialized(a_byte  *address,
                                          a_byte  *complete_object)
/*
Return TRUE if the subobject pointed to by address (part of the given complete
object) is initialized.
*/
{
  a_byte_count  off = (a_byte_count)(address-complete_object);
  a_byte_count  byte_pos, bit_pos;

  get_init_bit_pos(off, byte_pos, bit_pos);
  return (complete_object[-(int)byte_pos] & (a_byte)(1<<bit_pos)) != 0;
}  /* subobject_is_initialized */


static a_boolean addresses_are_comparable(an_interpreter_state  *ips,
                                          a_constexpr_address   *cap1,
                                          a_constexpr_address   *cap2)
/*
Return TRUE if the given addresses can be compared using relational operators.
The caller is responsible for ensuring that these are addresses pointing to
interpreter storage.
*/
{
  a_boolean  comparable;

  if (cap1->complete_object != cap2->complete_object) {
    /* Addresses into distinct objects are never comparable. */
    comparable = FALSE;
  } else if (cap1->address == cap2->address) {
    /* Equal addresses are always comparable. */
    comparable = TRUE;
  } else if (cap1->address == NULL || cap2->address == NULL) {
    /* A null pointer cannot be compared to a non-null pointer. */
    comparable = FALSE;
  } else {
    a_byte*     parent_addr = cap1->complete_object;
    a_type_ptr  parent_type = complete_object_type(parent_addr);
    a_boolean   check_dyn_alloc = TRUE;
    for (;;) {
      a_type_ptr  etp = NULL;
      if (parent_type->kind == (a_type_kind)tk_array) {
        etp = skip_typerefs(parent_type->variant.array.element_type);
      } else if (complete_obj_flag(parent_addr, COMPLETE_OBJ_DYN_ALLOC) &&
                 check_dyn_alloc) {
        /* If the complete object was dynamically allocated, the recorded
           complete object is actually the allocated element type.  Treat it
           as an array on this first iteration. */
        etp = parent_type;
        check_dyn_alloc = FALSE;
      }  /* if */
      if (etp != NULL) {
        /* The case of an array, or of a dynamically-allocated object (always
           treated as an array the first time around). */
        a_byte_count  esize, idx1, idx2;
        a_boolean     dummy_result = TRUE;
        esize = value_bytes_for_type(ips, etp, &dummy_result);
        idx1 = ((a_byte_count)(cap1->address-parent_addr))/esize;
        idx2 = ((a_byte_count)(cap2->address-parent_addr))/esize;
        if (idx1 != idx2) {
          /* Pointers to or into different elements of the same array are
             comparable. */
          comparable = TRUE;
          break;
        } else {
          /* Continue the check with the common element. */
          parent_type = etp;
          parent_addr += idx1*esize; /*lint !e679*/
        }  /* if */
      } else if (is_immediate_class_type(parent_type)) {
        a_field_ptr  fp1, fp2;
        a_base_class_ptr  bcp1, bcp2;
        a_byte_count      offset;
        if (cap1->address == parent_addr || cap2->address == parent_addr) {
          /* At least one of the addresses is that of a class object and the
             other is pointing at or within that same object. */
          comparable = TRUE;
          break;
        }  /* if */
        find_subobject_for_interpreter_address(ips, cap1, parent_addr,
                                               parent_type, &fp1, &bcp1);
        find_subobject_for_interpreter_address(ips, cap2, parent_addr,
                                               parent_type, &fp2, &bcp2);
        if (bcp1 != bcp2) {
          /* At least one address is in a base class subobject, and the other
             is not in that same subobject.  The addresses are not
             comparable. */
          comparable = FALSE;
          break;
        } else if (bcp1 != NULL) {
          /* Both addresses point to or into the same base class subobject:
             Continue the check within that. */
          parent_type = bcp1->type;
          get_mapped_byte_count(&persistent_map, bcp1, offset);
          parent_addr += offset;
        } else if (fp1 != fp2) {
          /* Addresses to or into different fields can be compared if and only
             if the fields have the same accessibility and are not members of a
             union. */
          comparable =
                   fp1->source_corresp.access == fp2->source_corresp.access &&
                   parent_type->kind != (a_type_kind)tk_union;
          break;
        } else {
          /* Both addresses point to or into the same field subobject: Continue
             the check within that. */
          parent_type = skip_typerefs(fp1->type);
          get_mapped_byte_count(&persistent_map, fp1, offset);
          parent_addr += offset;
        }  /* if */
      }  else {
        /* This can happen when comparing a pointer to a scalar object, and
           one "past the end" of that object. */
        comparable = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return comparable;
}  /* addresses_are_comparable */

#if DEBUG

uintptr_t db_hash_ptr(void  *ptr)
/*
Debug routine to compute a hash value from within a debugger.
*/
{
  return hash_ptr(ptr);
}  /* db_hash_ptr */


a_host_large_integer db_int_val(a_byte  *val_bytes)
/*
Return the int value stored at val_bytes.  Output a diagnostic in case of
overflow.
*/
{
  a_host_large_integer  val;
  a_boolean             ovflo;

  conv_integer_value_to_host_large_integer(
                            (an_integer_value *)val_bytes, /*is_signed=*/TRUE,
                            &val, &ovflo);
  if (ovflo) (void)fprintf(f_debug, "overflow!\n");
  return val;
}  /* db_int_val */


static void db_address_flags(unsigned int  flags)
/*
Output the given a_constexpr_address flags as human-readable text.
*/
{
  if (flags & CA_RUNTIME_DATA_ADDRESS) {
    (void)fprintf(f_debug, "runtime-data ");
  }  /* if */
  if (flags & CA_CANNOT_DEREFERENCE) {
    (void)fprintf(f_debug, "cannot-deref ");
  }  /* if */
  if (flags & CA_VARIANT_PATH) {
    (void)fprintf(f_debug, "variant-path ");
  }  /* if */
  if (flags & CA_ARRAY_ELEMENT) {
    (void)fprintf(f_debug, "array-elem ");
  }  /* if */
  if (flags & CA_BIT_FIELD) {
    (void)fprintf(f_debug, "bit-field ");
  }  /* if */
  if (flags & CA_FUNCTION) {
    (void)fprintf(f_debug, "func ");
  }  /* if */
  if (flags & CA_CONST_STORAGE) {
    (void)fprintf(f_debug, "const ");
  }  /* if */
  if (flags & CA_LIFETIME_EXTENDED) {
    (void)fprintf(f_debug, "lifetime-extended ");
  }  /* if */
  if (flags == 0) {
    (void)fprintf(f_debug, "no flags ");
  }  /* if */
}  /* db_address_flags */


static void db_variant_path(a_variant_path_entry_ptr  path)
/*
Display the indicated variant path.
*/
{
  int  n = 0;
  for (; path != NULL; path = path->next) {
    if (path->field != NULL) {
      fprintf(f_debug, " ->");
      db_name(&path->field->source_corresp);
    } else {
      fprintf(f_debug, "(no field)");
    }  /* if */
    n += 1;
  }  /* for */
  fprintf(f_debug, " (%d entries)", n);
}  /* db_variant_path */


static void db_addr(a_constexpr_address  *cap,
                    int                  indent)
/*
Output the given interpreter address.  Indent the output with the given number
of whitespace characters.
*/
{
  (void)fprintf(f_debug, "address %p:\n", (a_void_ptr)cap->address);
  db_indent(indent+2);
  db_address_flags(cap->flags);
  (void)fprintf(f_debug, "\n");
  if (is_array_element(cap)) {
    db_indent(indent+2);
    (void)fprintf(f_debug, "length %u:\n", cap->length);
  }  /* if */
  if (is_variant_path(cap)) {
    db_indent(indent+2);
    (void)fprintf(f_debug, "variant:");
    db_variant_path(cap->variant.variant_path);
    (void)fprintf(f_debug, "\n");
  }  /* if */
  db_indent(indent+2);
  (void)fprintf(f_debug, "alloc seq# %u:\n", cap->alloc_seq_number);
}  /* db_addr */


static void db_object(a_byte      *addr,
                      a_type_ptr  tp,
                      a_byte      *complete_object)
/*
Output the contents of the interpreted object of type tp stored at addr.
*/
{
  a_boolean  not_initialized = FALSE;
  static int indent = 0;

  db_indent(indent);
  if (complete_object == NULL) {
    (void)fprintf(f_debug,
                  "Cannot output value when complete_object is NULL.\n");
    goto done;
  }  /* if */
  tp = skip_typerefs(tp);
  if (addr != complete_object &&
      !subobject_is_initialized(addr, complete_object)) {
    not_initialized = TRUE;
  }  /* if */
  switch (tp->kind) {
    case tk_void:
      (void)fprintf(f_debug, "(void value)\n");
      break;
    case tk_integer:
      { a_host_large_integer  val;
        a_boolean             ovflo;
        conv_integer_value_to_host_large_integer(
                  (an_integer_value *)addr,
                  is_signed_integral_type(tp)/*lint !e2666*/, &val, &ovflo);
        (void)fprintf(f_debug, "%ld%s\n", (long)val,
                      ovflo ? " (overflow!)" : "");
      }
      break;
    case tk_float:
      (void)fprintf(f_debug, "%s\n",
                    fp_to_string(tp->variant.float_kind, fp_value(addr),
                                 /*pos_infinity=*/(a_boolean*)NULL,
                                 /*neg_infinity=*/(a_boolean*)NULL,
                                 /*not_a_number=*/(a_boolean*)NULL));
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
      (void)fprintf(f_debug, "%si\n",
                    fp_to_string(tp->variant.float_kind, fp_value(addr),
                                 /*pos_infinity=*/(a_boolean*)NULL,
                                 /*neg_infinity=*/(a_boolean*)NULL,
                                 /*not_a_number=*/(a_boolean*)NULL));
      break;
    case tk_complex:
      (void)fprintf(f_debug, "%s + ",
                    fp_to_string(tp->variant.float_kind, &cx_value(addr)->real,
                                 /*pos_infinity=*/(a_boolean*)NULL,
                                 /*neg_infinity=*/(a_boolean*)NULL,
                                 /*not_a_number=*/(a_boolean*)NULL));
      (void)fprintf(f_debug, "%si\n",
                    fp_to_string(tp->variant.float_kind, &cx_value(addr)->imag,
                                 /*pos_infinity=*/(a_boolean*)NULL,
                                 /*neg_infinity=*/(a_boolean*)NULL,
                                 /*not_a_number=*/(a_boolean*)NULL));
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_pointer:
      db_addr((a_constexpr_address*)addr, indent);
      break;
    case tk_array:
      { a_type_ptr    etp = skip_typerefs(tp->variant.array.element_type);
        a_boolean     dummy = TRUE;
        a_byte_count  n_bytes = value_bytes_for_type(
                                    (an_interpreter_state*)NULL, tp, &dummy);
        a_byte_count  e_bytes = value_bytes_for_type(
                                    (an_interpreter_state*)NULL, etp, &dummy);
        a_byte_count  offset;
        (void)fprintf(f_debug, "[\n");
        indent += 2;
        for (offset = 0; offset < n_bytes; offset += e_bytes) {
          db_indent(indent-2);
          (void)fprintf(f_debug, "%u:\n", offset/e_bytes);
          db_object(addr+offset, etp, complete_object);
        }  /* for */
        indent -= 2;
        db_indent(indent);
        (void)fprintf(f_debug, "]\n");
      }
      break;
    case tk_struct:
    case tk_class:
      {
        a_field_ptr       fp = tp->variant.class_struct_union.field_list;
        a_base_class_ptr  bcp = base_classes_of(tp);
        a_byte_count      offset;
        (void)fprintf(f_debug, "{\n");
        indent += 2;
        /* Output the field values. */
        for (fp = next_alloc_field(fp);
             fp != NULL;
             fp = next_alloc_field(fp->next)) {
          if (fp->compiler_generated && !fp->is_anonymous_parent_object) {
            /* Ignore fields generated by prelowering. */
            continue;
          }  /* if */
          db_indent(indent);
          (void)fprintf(f_debug, "field ");
          db_name(&fp->source_corresp);
          get_mapped_byte_count(&persistent_map, fp, offset);
          (void)fprintf(f_debug, " (offset %u)= \n", offset);
          db_object(addr+offset, fp->type, complete_object);
        }  /* for */
        /* Output the base class values. */
        for (; bcp != NULL; bcp = bcp->next) {
          if (bcp->direct && !bcp->is_virtual) {
            db_indent(indent);
            (void)fprintf(f_debug, "base ");
            db_type_name(bcp->type);
            get_mapped_byte_count(&persistent_map, bcp, offset);
            (void)fprintf(f_debug, " (offset %u)= \n", offset);
            if ((*(a_base_class_ptr*)(addr+offset))->type != bcp->type) {
              db_indent(indent);
              (void)fprintf(f_debug, " (BAD DERIVED PTR %p)\n",
                            (void*)*(a_type_ptr*)(addr+offset));
            }  /* if */
            db_object(addr+offset, bcp->type, complete_object);
          }  /* if */
        }  /* for */
        indent -= 2;
        db_indent(indent);
        (void)fprintf(f_debug, "}\n");
      }
      break;
    case tk_union:
      {
        a_field_ptr   fp;
        a_byte_count  offset;
        (void)fprintf(f_debug, "{\n");
        indent += 2;
        /* Output the active field and its value. */
        fp = *(a_field_ptr*)addr;
        if (!subobject_is_initialized(addr, complete_object)) {
          db_indent(indent);
          (void)fprintf(f_debug, "active field not initialized\n");
        } else if (fp == NULL) {
          db_indent(indent);
          (void)fprintf(f_debug, "no active field\n");
        } else {
          db_indent(indent);
          (void)fprintf(f_debug, "active field = %s\n",
                        db_name_str(&fp->source_corresp, iek_none));
          get_mapped_byte_count(&persistent_map, fp, offset);
          (void)fprintf(f_debug, " (offset %u)= \n", offset);
          db_object(addr+offset, fp->type, complete_object);
        }  /* if */
        indent -= 2;
        db_indent(indent);
        (void)fprintf(f_debug, "}\n");
      }
      break;
    default:
      (void)fprintf(f_debug, "db_object: unimplemented type:");
      db_type_name(tp);
      (void)fprintf(f_debug, "\n");
      break;
  }  /* switch */
  if (not_initialized) {
    db_indent(indent);
    (void)fprintf(f_debug, "[NOINIT]\n");
  }  /* if */
done:;
}  /* db_object */


void db_complete_object(a_byte  *obj)
/*
Output the contents of the given complete object.
*/
{
  a_type_ptr  tp = complete_object_type(obj);

  (void)fprintf(f_debug, ">> %s initialized:\n",
                complete_object_is_initialized(obj) ? "Completely"
                                                    : "Not completely");
  if (is_scalar_type(tp)) {
    db_type(tp);
    (void)fprintf(f_debug, "= ");
  } else {
    db_type_name(tp);
  }  /* if */
  db_object(obj, tp, obj);
}  /* db_complete_object */



void db_call_stack(void  *ips)
/*
Output a summary of the interpreted call stack.  ips is a pointer to an
interpreter state (passed as a void* to avoid having to expose
an_interpreter_state outside this source file).
*/
{
  unsigned          num = 0;
  a_call_frame_ptr  frame = ((an_interpreter_state*)ips)->curr_call_frame;

  while (frame != NULL) {
    (void)fprintf(f_debug, "%4u: ", num++);
    db_scp((char*)frame->routine);
    frame = frame->parent;
  }  /* while */
}  /* db_call_stack */


a_byte* db_stack_storage(void  *ptr,
                         void  *ips)
/*
Return a pointer to the stack storage associated with the given pointer.
*/
{
  a_byte  *bytes;

  get_stack_bytes((an_interpreter_state*)ips, ptr, bytes);
  return bytes;
}  /* db_stack_storage */

#endif /* DEBUG */
#if EXPENSIVE_CHECKING

static void check_no_variant_path_cycle(a_variant_path_entry_ptr  path)
/*
Trigger an internal error if the given path contains a cycle.
*/
{
  if (path != NULL) {
    a_variant_path_entry_ptr  ahead = path->next;
    a_variant_path_entry_ptr  it = path;
    for (;;) {
      if (ahead == NULL) break;
      if (it != ahead) {
        ahead = ahead->next;
        if (ahead == NULL) break;
        if (it != ahead) {
          ahead = ahead->next;
          it = it->next;
          continue;
        }  /* if */
      }  /* if */
      /* The "ahead" pointer has caught up with the "path" pointer.  There is
         therefore a cycle. */
      unexpected_condition_str("variant path loop");
    }  /* for */
  }  /* if */
}  /* check_no_variant_path_cycle */

#endif /* EXPENSIVE_CHECKING */

/*
Mark the complete object at the given address as fully initialized.
*/
#define mark_complete_object_initialized(obj)                                \
  set_complete_obj_flag(obj, COMPLETE_OBJ_INITIALIZED)

/*
Mark the complete object at the given address as not fully initialized.
*/
#define unmark_complete_object_initialized(obj)                              \
  clear_complete_obj_flag(obj, COMPLETE_OBJ_INITIALIZED)


static a_boolean add_to_variant_path(
                                   a_constexpr_address  *addr,
                                   a_field_ptr          union_field,
                                   a_type_ptr           top_type,
                                   a_boolean            for_ctor_init = FALSE)
/*
The given field is being selected in the given type, from an object or
subobject stored at the given interpreter address.  The type is a union type
or the field is a member of an anonymous union or struct.  If needed, update
the variant path of addr to reflect all union selections involved (potentially
this can include the selection from top_type itself and any number of nested
anonymous unions).  Also, adjust addr->address to the address of the innermost
anonymous union or struct object.  If for_ctor_init is TRUE, this selection is
for a ctor-initializer: In that context, selection from anonymous structs (a
nonstandard extension) is implicit.  If for_ctor_init is FALSE, this selection
is for a field selection in an expression context, where anonymous struct field
selections are represented explicitly, and thus do not have to be handled here.
This routine currently handles at most 30 selection steps (i.e., at most 29
anonymous nested types): If more are needed, FALSE is returned and the caller
is responsible for recording a failing evaluation.
*/
{
#define MAX_LEVELS 30
  a_boolean                 result = TRUE;
  a_variant_path_entry_ptr  path = NULL, *p_last = &path;
  a_symbol_ptr              au_parent;
  a_field_ptr               fields[MAX_LEVELS];
  int32_t                   n = 0;
  a_type_ptr                curr_type = top_type;
  a_byte_count              total_offset = 0;

  fields[n++] = union_field;
  /* Collect the (potential) chain of anonymous parent fields: */
  au_parent = symbol_for(union_field)->variant.field.anonymous_parent_object;
  while (au_parent != NULL && symbol_is(au_parent, sk_field)) {
    /* The field is a member of an anonymous union or struct. */
    if (!for_ctor_init &&
        !type_is(skip_typerefs(au_parent->variant.field.ptr->type),
                 tk_union)) {
      /* For member selection in expression contexts, anonymous structs are
         explicitly represented in the IL.  Do not re-apply the corresponding
         adjustments here. */
      break;
    }  /* if */
    fields[n++] = au_parent->variant.field.ptr;
    if (n == MAX_LEVELS) {
      result = FALSE;
      goto done;
    }  /* if */
    au_parent = au_parent->variant.field.anonymous_parent_object;
  }  /* while */
  /* Traverse the fields from outer-to-inner, updating the path and the
     offset. */
  for (;;) {
    a_field_ptr   fp = fields[--n];
    a_byte_count  offset;
    if (type_is(curr_type, tk_union)) {
      *p_last = alloc_variant_path_entry();
      (*p_last)->next = NULL;
      (*p_last)->field = fp;
      (*p_last)->base_address = addr->address+total_offset;
      p_last = &(*p_last)->next;
    }  /* if */
    if (n == 0) break;
    get_mapped_byte_count(&persistent_map, fp, offset);
    total_offset += offset;
    curr_type = skip_typerefs(fp->type);
  }  /* while */
  addr->address += total_offset;
  if (path != NULL) {
    /* There are actual variant selections.  Record them. */
    a_variant_path_entry_ptr  last_entry;
    if (addr->flags & CA_VARIANT_PATH) {
      /* This entry already has a variant path: Find its end. */
      last_entry = addr->variant.variant_path->next;
      while (last_entry->next != NULL) {
        last_entry = last_entry->next;
      }  /* while */
    } else {
      /* No entries yet: Create a first entry to record an array base address
         if needed. */
      addr->variant.variant_path = alloc_variant_path_entry();
      last_entry = addr->variant.variant_path;
      last_entry->next = NULL;
      last_entry->field = NULL;
      last_entry->base_address = NULL;
      addr->flags |= CA_VARIANT_PATH;
    }  /* if */
    last_entry->next = path;
  }  /* if */
#if EXPENSIVE_CHECKING
  if (is_variant_path(addr)) {
    check_no_variant_path_cycle(addr->variant.variant_path);
  }  /* if */
#endif /* EXPENSIVE_CHECKING */
done:
  return result;
#undef MAX_LEVELS
}  /* add_to_variant_path */


static void copy_variant_path(a_constexpr_address  *addr)
/*
Replace the variant path pointed to by addr by a copy of that same path.
(This is used to avoid sharing paths in cases where one will be cleaned up
soon but the other must persist.  E.g., this happens after copying a variable
(whose variant path must persist) to temporary expression storage.
*/
{
  a_variant_path_entry_ptr  vpep, *p_vpep;

#if EXPENSIVE_CHECKING
  check_no_variant_path_cycle(addr->variant.variant_path);
#endif /* EXPENSIVE_CHECKING */
  p_vpep = &addr->variant.variant_path;
  vpep = *p_vpep;
  do {
    a_variant_path_entry_ptr  new_entry = alloc_variant_path_entry();
    *p_vpep = new_entry;
    new_entry->field = vpep->field;
    new_entry->base_address = vpep->base_address;
    p_vpep = &new_entry->next;
    vpep = vpep->next;
  } while (vpep != NULL);
  *p_vpep = NULL;
}  /* copy_variant_path */


static void release_variant_path(a_constexpr_address  *addr)
/*
Release the variant path entries associated with the given address.  The caller
is responsible for ensuring that there are such entries.
*/
{
  a_variant_path_entry_ptr  entries, vpep;
  unsigned long             n_freed = 2;

  entries = addr->variant.variant_path;
  /* There are always at least two entries on a variant path (the first of
     which is a placeholder entry to potentially hold the base address of an
     array). */
  vpep = entries->next;
  while (vpep->next != NULL) {
    vpep = vpep->next;
    n_freed += 1;
  }  /* while */
  vpep->next = free_variant_path_entries;
  free_variant_path_entries = entries;
  n_free_variant_path_entries += n_freed;
  addr->flags &= ~CA_VARIANT_PATH;
  addr->variant.base_address = entries->base_address;
}  /* release_variant_path */


static a_boolean check_variant_path(an_interpreter_state  *ips,
                                    a_constexpr_address   *addr,
                                    a_source_position     *pos)
/*
The given address entry has its CA_VARIANT_PATH flag set.  Check that it points
to an object whose active variant subobjects match the recorded variant path
and return TRUE if that's the case.  Otherwise, return FALSE and record an
appropriate diagnostic.

Release the variant path structures when the check is completed.
*/
{
  a_boolean  result = TRUE;
  a_variant_path_entry_ptr
             vpep = addr->variant.variant_path->next;

  do {
    a_field_ptr  active_field = *(a_field_ptr*)vpep->base_address;
    a_field_ptr  selected_field = vpep->field;
    if (selected_field != active_field) {
      do_constexpr_fail(result);
      if (active_field != NULL) {
        info_with_pos_sym2(ec_constexpr_union_field_inactive, pos,
                           symbol_for(selected_field),
                           symbol_for(active_field), ips);
      } else {
        info_with_pos_sym(ec_constexpr_no_active_union_field, pos,
                          symbol_for(selected_field), ips);
      }  /* if */
      break;
    }  /* if */
    vpep = vpep->next;
  } while (vpep != NULL);
  release_variant_path(addr);
  return result;
}  /* check_variant_path */


static a_boolean check_variant_assign(an_interpreter_state  *ips,
                                      a_constexpr_address   *addr,
                                      a_source_position     *pos)
/*
The given address entry has its CA_VARIANT_PATH flag set and an assignment is
being made through it.  In pre-C++20 modes, check that the variant path matches
the active variant subobjects and return TRUE if that's the case, or FALSE
otherwise (and record an appropriate diagnostic).  In C++20 modes, if the
variant path does not match the variant subobjects, mark those subobjects as
uninitialized and activate the subobject along the variant path instead; return
TRUE in that case.
*/
{
  a_boolean  result = TRUE, activation_mode = FALSE, strict = !cpp20_mode;
  a_variant_path_entry_ptr
             vpep = addr->variant.variant_path->next;

  if (!subobject_is_initialized(addr->address, addr->complete_object)) {
    if (strict) {
      /* Before C++20, this was always an error. */
      if (complete_object_is_initialized(addr->complete_object)) {
        /* The object was previously initialized.  So this is just a case of
           an active field having been deactivated.  A better diagnostic will
           be produced below. */
      } else {
        do_constexpr_fail(result);
        info_with_pos(ec_object_not_initialized, pos, ips);
        goto done;
      }  /* if */
    } else {
      /* Enter "activation mode": Automatically set the active member to the
         one assigned to (possibly at multiple levels). */
      activation_mode = TRUE;
    }  /* if */
  }  /* if */
  do {
    a_field_ptr  *p_active_field = (a_field_ptr*)vpep->base_address;
    a_field_ptr  active_field = *p_active_field;
    a_field_ptr  selected_field = vpep->field;
    if (!activation_mode) {
      if (selected_field != active_field) {
        if (strict) {
          /* Before C++20, this was always an error. */
          do_constexpr_fail(result);
          if (active_field != NULL) {
            info_with_pos_sym2(ec_constexpr_union_field_inactive, pos,
                               symbol_for(selected_field),
                               symbol_for(active_field), ips);
          } else {
            info_with_pos_sym(ec_constexpr_no_active_union_field, pos,
                              symbol_for(selected_field), ips);
          }  /* if */
          break;
        } else {
          /* Enter "activation mode": Automatically set the active member to
             the one assigned to (possibly at multiple levels).  Mark the
             currently active subobject as uninitialized. */
          activation_mode = TRUE;
          if (active_field != NULL) {
            a_type_ptr  tp = parent_class_of(active_field);
            if (!mark_whole_subobject_uninitialized(
                        ips, vpep->base_address, tp, addr->complete_object)) {
              result = FALSE;
              goto done;
            }  /* if */
            unmark_complete_object_initialized(addr->complete_object);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (activation_mode) {
      /* Activation mode: Set the active field to the selected field. */
      mark_subobject_initialized((a_byte*)p_active_field,
                                 addr->complete_object);
      *p_active_field = selected_field;
    }  /* if */
    vpep = vpep->next;
  } while (vpep != NULL);
  if (activation_mode) {
    /* The assignment that is about to take place "initializes" the
       corresponding subobject. */
    mark_subobject_initialized(addr->address, addr->complete_object);
    /* However, that may have left other subobjects uninitialized.  For
       example:
         union U {
           struct S {
             short h;
             union V {
               int i;
               unsigned u;
             } v;
           } s;
           double f;
         };
         constexpr int g() {
           U u = { .f = 1.0 };
           u.s.v.u = 42;  // Initialized v but leaves h uninitialized.
           return u.s.h;
         }
    */
    unmark_complete_object_initialized(addr->complete_object);
  }  /* if */
done:
  return result;
}  /* check_variant_assign */


/*
Macro to call release_variant_path if a given address (possibly passed via a
pointer to bytes representing that address) holds a variant path.
*/
#define release_variant_path_if_needed(value)                                 \
{                                                                             \
  if (is_variant_path((a_constexpr_address*)(value))) {                       \
    release_variant_path((a_constexpr_address*)(value));                      \
  }  /* if */                                                                 \
}  /* release_variant_path_if_needed */


/*
Macro to release structures allocated for the representation of the result of
a glvalue or pointer expression (i.e., an a_constexpr_address value).  expr is
the expression corresponding to the interpreter storage at value and tp is
the type of expr after applying skip_typerefs.
*/
#define release_address_structures(expr, tp, value)                           \
{                                                                             \
  if (((expr)->is_lvalue || (expr)->is_xvalue ||                              \
       (tp)->kind == (a_type_kind)tk_pointer)) {                              \
    release_variant_path_if_needed(value);                                    \
  }  /* if */                                                                 \
}  /* release_address_structures */


/*
For an interpreter value that is known to be an address, copy the associated
structures (e.g., any variant path) so that they won't be shared with the
address they were shallowly copied from.
*/
#define copy_address_structures(value)                                        \
{                                                                             \
  a_constexpr_address  *addr = (a_constexpr_address *)(value);                \
  if (is_variant_path(addr)) {                                                \
    copy_variant_path(addr);                                                  \
  }  /* if */                                                                 \
}  /* copy_address_structures */


static a_boolean is_integer_address(a_constexpr_address  *cap,
                                    an_integer_value     *val)
/*
Return TRUE if the given address represents a null address or a run-time
address represented by a ck_integer constant.  If val is non-null, set *val to
the corresponding integer value (zero for a null address).
*/
{
  a_boolean  result;

  if (is_runtime_data_address(cap)) {
    a_constant_ptr  cp = cap->variant.addr_con;
    if (constant_is(cp, ck_integer)) {
      result = TRUE;
      if (val != NULL) *val = cp->variant.integer_value;
    } else {
      result = FALSE;
    }  /* if */
  } else if (is_function_address(cap)) {
    result = cap->variant.routine == NULL;
    if (result && val != NULL) *val = zero_int;
  } else {
    result = cap->address == NULL;
    if (result && val != NULL) *val = zero_int;
  }
  return result;
}  /* is_integer_address */


static a_boolean do_constexpr_statement(an_interpreter_state  *ips,
                                        a_statement_ptr       stmt);

static a_boolean do_constexpr_expression(
                                       an_interpreter_state  *ips,
                                       an_expr_node_ptr      expr,
                                       a_byte                *result_storage,
                                       a_byte                *complete_object);


static a_boolean do_constexpr_ctor(an_interpreter_state  *ips,
                                   a_dynamic_init_ptr    dip,
                                   a_source_position     *pos,
                                   a_constexpr_address   *cap,
                                   a_constexpr_address   *implied_src);


static a_boolean do_constexpr_dtor(an_interpreter_state  *ips,
                                   a_routine_ptr         callee,
                                   a_source_position     *pos,
                                   a_byte                *result_storage,
                                   a_byte                *complete_object,
                                   a_boolean             nonvirtual = FALSE);

static a_boolean do_constexpr_dynamic_init(
                                   an_interpreter_state  *ips,
                                   a_dynamic_init_ptr    dip,
                                   a_source_position     *pos,
                                   a_constexpr_address   *dst_addr,
                                   a_constexpr_address   *implied_src = NULL);


static a_boolean perform_destructions(an_interpreter_state  *ips)
/*
Perform the destructions for the current storage stack.
*/
{
  a_boolean  result = TRUE;
  a_constexpr_destruction
             *dlist = ips->storage_stack.destructions;

  do {
    if (!do_constexpr_dtor(ips, dlist->dip->destructor, dlist->pos,
                           dlist->sub_obj, dlist->complete_obj)) {
      result = FALSE;
      break;
    }  /* if */
    dlist = dlist->next;
  } while (dlist != NULL);
  return result;
}  /* perform_destructions */


/*
Macro to interpret a full-expression.
*/
#define do_constexpr_full_expression(                                         \
                    ips, expr, result_storage, complete_object, result_flag)  \
{                                                                             \
  a_storage_stack_state  saved_stack_for_full_expr;                           \
  save_storage_stack(ips, saved_stack_for_full_expr);                         \
  (result_flag) = do_constexpr_expression(                                    \
                                ips, expr, result_storage, complete_object);  \
  restore_storage_stack(ips, saved_stack_for_full_expr, result_flag);         \
}


static a_boolean translate_il_address_offset(an_interpreter_state  *ips,
                                             a_constant_ptr        con,
                                             a_constexpr_address   *cap,
                                             a_type_ptr            obj_type)
/*
con is an address constant being translated to *cap, a representation in
interpreter storage of the address of the complete object of type obj_type.
If con represents the address of a subobject, update *cap accordingly: That
can result in modifications of the address, but also the flags and the
variant path.
*/
{
  a_boolean             result = TRUE;
  a_subobject_path_ptr  spp = con->variant.address.subobject_path;
  a_byte_count          i_offset;

  for (; spp != NULL; spp = spp->next) {
    if (spp->is_offset) {
      a_byte_count  array_size, elem_size;
      array_size = value_bytes_for_type(ips, obj_type, &result); 
      if (type_is(obj_type, tk_array)) {
        do {
          obj_type = skip_typerefs(obj_type->variant.array.element_type);
        } while (type_is(obj_type, tk_array));
        elem_size = value_bytes_for_type(ips, obj_type, &result); 
        cap->flags |= CA_ARRAY_ELEMENT;
        cap->length = array_size/elem_size;
        if (is_variant_path(cap)) {
          cap->variant.variant_path->base_address = cap->address;
        } else {
          cap->variant.base_address = cap->address;
        }  /* if */
      } else {
        elem_size = array_size;
      }  /* if */
      check_assertion(result);
      i_offset = (a_byte_count)spp->variant.ptr_offset*elem_size;
      if (array_size == i_offset) {
        /* This is a "one past the end" pointer. */
        cap->flags |= CA_CANNOT_DEREFERENCE;
        check_assertion(spp->next == NULL);
      }  /* if */
    } else if (spp->is_base_class) {
      a_base_class_ptr  base_class = spp->variant.base_class;
      /* base_class is not necessarily a direct base class, but we only have
         offset information for direct base classes.  So we use the derivation
         path for base_class to compute the total offset. */
      a_derivation_step_ptr  dsp = base_class->derivation->path;
      i_offset = 0;
      for (; dsp != NULL; dsp = dsp->next) {
        a_base_class_ptr  bcp, bcp_step = dsp->base_class;
        a_byte_count  incr;
        if (!bcp_step->direct) {
          for (bcp = base_classes_of(obj_type); bcp != NULL; bcp = bcp->next) {
            if (bcp->direct && bcp->type == bcp_step->type) {
              bcp_step = bcp;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        get_mapped_byte_count(&persistent_map, bcp_step, incr);
        i_offset += incr;
        obj_type = bcp_step->type;
      }  /* for */
      cap->flags &= ~CA_ARRAY_ELEMENT;
    } else {
      a_field_ptr  fp = spp->variant.field;
      if (type_is(obj_type, tk_union)) {
        /* Update the variant path.  Do not use add_to_variant_path because
           it implicitly handles anonymous unions, whereas this process
           traverses them explicitly (we'd account for them twice). */
        a_variant_path_entry_ptr  last_entry, vpep;
        if (cap->flags & CA_VARIANT_PATH) {
          /* This entry already has a variant path: Find its end. */
          last_entry = cap->variant.variant_path->next;
          while (last_entry->next != NULL) {
            last_entry = last_entry->next;
          }  /* while */
        } else {
          /* No entries yet: Create a first entry to record an array base
             address if needed. */
          last_entry = alloc_variant_path_entry();
          last_entry->field = NULL;
          last_entry->base_address = is_array_element(cap) ?
                                             cap->variant.base_address : NULL;
          cap->variant.variant_path = last_entry;
          cap->flags |= CA_VARIANT_PATH;
        }  /* if */
        vpep = alloc_variant_path_entry();
        vpep->next = NULL;
        vpep->field = fp;
        vpep->base_address = cap->address;
        last_entry->next = vpep;
      }  /* if */
      get_mapped_byte_count(&persistent_map, fp, i_offset);
      obj_type = skip_typerefs(fp->type);
      cap->flags &= ~CA_ARRAY_ELEMENT;
    }  /* if */
    cap->address += i_offset;
  }  /* for */
  if (type_is(obj_type, tk_array)) {
    /* con points to an array as a whole or to just its first element.
       Set the CA_ARRAY_ELEMENT flag in the latter case. */
    a_type_ptr  con_addr_type = type_pointed_to(con->type);
    if (!identical_types(obj_type, con_addr_type)) {
      cap->flags |= CA_ARRAY_ELEMENT;
      cap->length =
             (unsigned int)obj_type->variant.array.variant.number_of_elements;
      if (is_variant_path(cap)) {
        cap->variant.variant_path->base_address = cap->address;
      } else {
        cap->variant.base_address = cap->address;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* translate_il_address_offset */


static a_constant_ptr instantiate_member_constant(a_variable_ptr  vp)
/*
vp represents a variable that is expected to have a constant value, but with
no recorded initializer.  This can occur in GNU C++ mode with static data
members of class templates, whose initializers are instantiated on demand.  If
this is such a case, perform that instantiation and return the constant.
Otherwise, return NULL.
*/
{
  a_constant_ptr  result = NULL;

  if (vp->initializer_in_class && vp->is_template_variable) {
    /* An uninstantiated in-class initializer. */
    ensure_inclass_static_member_constant_initializer_is_scanned(vp);
    if (vp->init_kind == (an_init_kind)initk_static) {
      result = vp->initializer.constant;
    }  /* if */
  }  /* if */
  return result;
}  /* instantiate_member_constant */


static a_byte* set_up_param_ref_for_this_ptr(
                                       an_interpreter_state  *ips,
                                       a_byte                *object,
                                       a_byte                *complete_object)
/*
Set up a "this" pointer in case we run into enk_param_ref nodes.  It is
associated with &ips->curr_call_frame.  The *this object is stored at the
address indicated by object and complete_object.  This mechanism is also used
to retrieve the "this" pointer value that is associated with the implicit
source address of certain nested lambda captures.
*/
{
  a_byte         *this_bytes;
  a_byte_count   this_n_bytes = sizeof(a_constexpr_address);
  a_byte_count   with_postfix_bytes;
  a_var_postfix  *postfix;

  do_host_alignment(this_n_bytes);
  with_postfix_bytes = this_n_bytes+sizeof(a_var_postfix);
  alloc_complete_object(ips, with_postfix_bytes, generic_ptr_type, this_bytes);
  clear_address(this_bytes, object);
  ((a_constexpr_address *)this_bytes)->complete_object = complete_object;
  ((a_constexpr_address *)this_bytes)->alloc_seq_number =
                                                   ips->curr_alloc_seq_number;
  mark_complete_object_initialized(this_bytes);
  postfix = (a_var_postfix*)(this_bytes+this_n_bytes);
  postfix->alloc_seq_number = ips->curr_alloc_seq_number;
  map_or_replace_ptr(&ips->map, &ips->curr_call_frame, this_bytes,
                     postfix->prev_storage);
  return this_bytes;
}  /* set_up_param_ref_for_this_ptr */


static void unmap_param_ref_for_this_ptr(an_interpreter_state  *ips,
                                         a_byte                *this_bytes)
/*
Remove a binding of enk_param nodes for "this" pointers previously set up by a
call to set_up_param_ref_for_this_ptr.  Restore any earlier binding if needed.
*/
{
  a_var_postfix  *postfix;

  postfix = (a_var_postfix*)(this_bytes+sizeof(a_constexpr_address));
  if (postfix->prev_storage == NULL) {
    unmap_ptr(&ips->map, &ips->curr_call_frame);
  } else {
    replace_mapped_ptr(&ips->map, &ips->curr_call_frame,
                       postfix->prev_storage);
  }  /* if */
}  /* unmap_param_ref_for_this_ptr */


static a_byte_count compute_interpreter_base_offset(a_base_class_ptr  bcp,
                                                    a_base_class_ptr  *p_dbcp)
/*
Return the offset of the given base class as laid out by the interpreter.
This function works even for indirect base classes (for direct base classes,
a single lookup in the persistent_map is sufficient).  If p_dbcp is non-NULL,
set *p_dbcp to the direct base class for the last step of the derivation path
(if bcp is a direct base, it's bcp itself).
*/
{
  a_byte_count           offset;
  a_derivation_step_ptr  dsp = bcp->derivation->path;
  a_type_ptr             prev_type = dsp->base_class->type;

  get_mapped_byte_count(&persistent_map, dsp->base_class, offset);
  for (dsp = dsp->next; dsp != NULL; dsp = dsp->next) {
    a_byte_count      step;
    bcp = find_direct_base_class_of(prev_type, dsp->base_class->type);
    prev_type = dsp->base_class->type;
    get_mapped_byte_count(&persistent_map, bcp, step);
    offset += step;
  }  /* for */
  if (p_dbcp != NULL) *p_dbcp = bcp;
  return offset;
}  /* compute_interpreter_base_offset */


static a_boolean extract_value_from_constant(
                                    an_interpreter_state  *ips,
                                    a_constant_ptr        con,
                                    a_byte                *value,
                                    a_byte                *complete_object,
                                    a_constexpr_address   *implied_src = NULL);

static inline a_boolean copy_val_from_constant(
                                   an_interpreter_state  *ips,
                                   a_constant_ptr        con,
                                   a_byte                *value,
                                   a_byte                *complete_object,
                                   a_constexpr_address   *implied_src = NULL)
/*
Set *value from the value of the specified constant.  Duplicates some cases
from extract_value_from_constant for performance reasons.  See
extract_value_from_constant for the meaning of the parameters.
*/
{
  a_boolean  result;

  if (constant_is(con, ck_integer) && !con->implicit_cast) {
    if (con->is_generic_initializer) {
      do_constexpr_fail(result);
    } else {
      *(an_integer_value *)value = con->variant.integer_value;
      result = TRUE;
    }  /* if */
  } else if (constant_is(con, ck_float)) {
    if (con->is_generic_initializer) {
      do_constexpr_fail(result);
    } else {
      *fp_value(value) = (con)->variant.float_value;
      result = TRUE;
    }  /* if */
  } else {
    result = extract_value_from_constant(ips, con,
                                         value, complete_object,
                                         implied_src);
  }  /* if */
  return result;
}  /* copy_val_from_constant */


static a_boolean extract_value_from_constant(
                                    an_interpreter_state  *ips,
                                    a_constant_ptr        con,
                                    a_byte                *value,
                                    a_byte                *complete_object,
                  /* Defaulted: */  a_constexpr_address   *implied_src)
/*
Copy the value of con into the interpreter storage at value, converting
formats as necessary.  Return FALSE if the constant is an error constant.  If
implied_src is non-NULL, this is being invoked from the evaluation of a copy
or move constructor and the source object is stored at the location indicated
by implied_src.
*/
{
  a_boolean  result = TRUE;

  if (con->implicit_cast) {
    if (con->is_reinterpret_cast &&
        !(ips->allow_reinterpret_cast && constant_is(con, ck_integer) &&
          is_pointer_type(con->type))) {
      info_with_pos(ec_constexpr_reinterpret_cast, &ips->position, ips);
      do_constexpr_fail(result);
      goto done;
    } else if (con->expr != NULL && !con->is_reinterpret_like_cast &&
               !con->is_result_of_constexpr_call &&
               !(constant_is(con, ck_integer) ||
                 (constant_is(con, ck_address) &&
                  con->variant.address.kind ==
                                      (an_address_base_kind)abk_temporary))) {
      /* If the constant includes a conversion, evaluate the constant through
         the backing expression so that the conversion is correctly applied.
         Do not attempt this if a reinterpret-like cast is involved (which we
         might not be able to interpret) or if we ended up with an integer
         value (which we can just load here).  ck_address/abk_temporary entries
         should generally be treated as run-time address constants if they
         refer to a mutable temporary: Evaluating the underlying enk_temp_init
         node would make the storage subject to mutation during this evaluation
         and that is not permitted. */
      if (!do_constexpr_expression(ips, con->expr, value, complete_object)) {
        result = FALSE;
      }  /* if */
      goto done;
    }  /* if */
  } else if (con->is_generic_initializer) {
    /* A constant from a template-dependent context.  Don't attempt to extract
       it because it might not match the type of the destination object. */
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  switch (con->kind) {
    case ck_error:
      ips->input_error = TRUE;
      do_constexpr_fail(result);
      break;
    case ck_integer:
      { a_type_ptr  tp = skip_typerefs(con->type);
        if (tp->kind == (a_type_kind)tk_pointer ||
            tp->kind == (a_type_kind)tk_nullptr) {
          /* Various expressions for null pointer constants are expressed as
             ck_integer constants.  They're handled as run-time data constants
             in the interpreter. */
          clear_runtime_constant_address(value, con);
        } else if (tp->kind == (a_type_kind)tk_integer) {
          *(an_integer_value*)value = con->variant.integer_value;
        } else {
          unexpected_condition();
        }  /* if */
      }
      break;
    case ck_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      *fp_value(value) = con->variant.float_value;
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_complex:
      *cx_value(value) = *con->variant.complex_value;
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case ck_address:
      if (con->variant.address.kind == (an_address_base_kind)abk_temporary) {
        a_constant_ptr  cp = con->variant.address.variant.constant;
        if (!is_const_qualified_type(cp->type)) {
          /* Do not load mutable temporaries (they're runtime entities even
             when referred to by "constant" ck_address entries). */
          clear_runtime_constant_address(value, con);
          break;
        }  /* if */
      }  /* if */
      {
        a_type_ptr  obj_type = NULL;
        if (con->is_reinterpret_like_cast) {
          /* A reinterpret-like cast (e.g., from pointer to integer). */
          /* Reconstruct the original type for the recorded diagnostic. */
          a_type_ptr  orig_type;
          switch (con->variant.address.kind) {
            case abk_routine:
              orig_type = con->variant.address.variant.routine->type;
              break;
            case abk_variable:
              orig_type = con->variant.address.variant.variable->type;
              break;
            case abk_constant:
            case abk_temporary:
              orig_type = con->variant.address.variant.constant->type;
              break;
            case abk_typeid:
              orig_type = typeid_constant_type(/*is_cli_typeid=*/FALSE);
              break;
#if MICROSOFT_EXTENSIONS_ALLOWED
            case abk_uuidof:
              orig_type = make_qualified_type(type_of_guid,
                                              (a_type_qualifier_set)TQ_CONST);
              break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            case abk_label:
              orig_type = con->type;
              break;
            default:
              orig_type = con->type;
              unexpected_condition();
          }  /* switch */
          if (is_any_reference_type(orig_type)) {
            orig_type = type_pointed_to(orig_type);
          } else if (is_array_type(orig_type)) {
            orig_type = array_element_type(orig_type);
          }  /* if */
          orig_type = make_pointer_type(orig_type);
          info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                              &ips->position, orig_type, con->type, ips);
          do_constexpr_fail(result);
          break;
        }  /* if */
        switch (con->variant.address.kind) {
          case abk_routine:
            { a_routine_ptr  rp = con->variant.address.variant.routine;
#if GNU_EXTENSIONS_ALLOWED
              if (rp->is_weak) {
                /* Weakly declared functions have no definite address (they
                   could have a null address). */
                info_with_pos_sym(ec_constexpr_weak_address,
                                  constant_pos(con, ips), symbol_for(rp), ips);
                do_constexpr_fail(result);
                break;
              }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
              make_function_address(value, rp);
            }
            break;
          case abk_variable:
            { /* Check if the variable has a constant value, and if so ensure
                 it has a representation in static interpreter storage.
                 Otherwise, create a run-time address. */
              a_variable_ptr  vp = con->variant.address.variant.variable;
              a_type_ptr      vtp = skip_typerefs(vp->type);
              if (vp->constant_valued || vp->is_constexpr) {
                a_byte  *var_bytes;
                get_stack_bytes(ips, vp, var_bytes);
                if (var_bytes == NULL) {
                  a_boolean  no_reverse_map = FALSE;
                  alloc_static_object(ips, vtp, var_bytes, &result);
                  map_stack_bytes(ips, vp, var_bytes);
                  if (result) {
                    a_constant_ptr  cp = NULL;
                    if (vp->init_kind == initk_static) {
                      cp = vp->initializer.constant;
                    } else if (vp->init_kind == initk_dynamic ||
                               vp->init_kind == initk_module) {
                      a_dynamic_init_ptr  dip = vp->initializer.dynamic;
                      if (dyn_init_is(dip, dik_constant) ||
                          (dyn_init_is(dip, dik_module) &&
                           dip->variant.constant.ptr != NULL)) {
                        cp = dip->variant.constant.ptr;
                      } else {
                        a_constexpr_address  var_addr;
                        clear_address(&var_addr, var_bytes);
                        var_addr.alloc_seq_number = 0;
                        var_addr.complete_object = var_bytes;
                        result = do_constexpr_dynamic_init(
                                         ips, dip, &ips->position, &var_addr);
                      }  /* if */
                    } else {
                      an_init_kind    init_kind;
                      an_initializer  *initializer;
                      get_variable_initializer(vp, (a_scope*)NULL, &init_kind,
                                               &initializer);
                      if (init_kind == (an_init_kind)initk_static) {
                        cp = initializer->constant;
                      } else if (init_kind == (an_init_kind)initk_dynamic) {
                        a_dynamic_init_ptr  dip = initializer->dynamic;
                        if (dyn_init_is(dip, dik_constant)) {
                          cp = dip->variant.constant.ptr;
                        } else {
                          a_constexpr_address  var_addr;
                          clear_address(&var_addr, var_bytes);
                          var_addr.alloc_seq_number = 0;
                          var_addr.complete_object = var_bytes;
                          result = do_constexpr_dynamic_init(
                                         ips, dip, &ips->position, &var_addr);
                        }  /* if */
                      } else {
                        /* In GNU C++ mode, the initializer may not be
                           instantiated yet. */
                        if (gpp_mode && vp->is_template_variable) {
                          cp = instantiate_member_constant(vp);
                          if (cp == NULL) {
                            do_constexpr_fail(result);
                            break;
                          }  /* if */
                        }  /* if */
                        if (cp == NULL) {
                          /* We may get here if a variable's initializer
                             refers to the variable itself.  Create a run-time
                             address. */
                          clear_runtime_constant_address(value, con);
                          break;
                        }  /* if */
                      }  /* if */
                    }  /* if */
                    if (cp != NULL) {
                      result = extract_value_from_constant(
                                               ips, cp, var_bytes, var_bytes);
                      if (constant_is(cp, ck_string)) {
                        /* The ck_string case already installed a reverse map
                           from var_bytes to cp. */
                        if (type_is(vtp, tk_array)) {
                          /* An array initialized with a string: var_bytes is
                             the variable storage, not the string storage.
                             Undo the previous reverse map. */
                          unmap_stack_bytes(ips, var_bytes);
                        } else {
                          /* Presumably a pointer or reference to the string.
                             var_bytes is the string storage itself.  Do not
                             map it back to the variable. */
                          no_reverse_map = TRUE;
                        }  /* if */
                      }  /* if */
                    }  /* if */
                  }  /* if */
                  if (!result) break;
                  if (ips->disallow_mutable_field_load &&
                      is_immediate_class_type(vtp) &&
                      vtp->variant.class_struct_union.any_mutable_member) {
                    /* A class value with a mutable member cannot be fully
                       loaded and thus is not completely initialized. */
                  } else {
                    /* Disable spurious GCC warning about writing one byte to a
                       zero sized region at the -O3 optimization level. */
BEGIN_DISABLE_GCC_WARNING_STR_OVERFLOW
                      mark_complete_object_initialized(var_bytes);
END_DISABLE_GCC_WARNING_STR_OVERFLOW
                  }  /* if */
                  if (!no_reverse_map) {
                    /* Set up a reverse mapping so we can re-create a variable
                       address constant if the address (with potentially a
                       different offset) is returned from the interpreter. */
                    map_stack_bytes(ips, var_bytes, (a_byte*)con);
                  }  /* if */
                }  /* if */
                clear_address(value, var_bytes);
                if (is_const_qualified_type(vp->type)) {
                  ((a_constexpr_address*)value)->flags |= CA_CONST_STORAGE;
                }  /* if */
                obj_type = vtp;
              } else {
                clear_runtime_constant_address(value, con);
                if (type_is(vtp, tk_array) &&
                    (con->variant.address.subobject_path != NULL ||
                     is_pointer_type(con->type))) {
                  ((a_constexpr_address*)value)->flags |= CA_ARRAY_ELEMENT;
                }  /* if */
#if GNU_EXTENSIONS_ALLOWED
                if (vp->is_weak) {
                  /* Weakly declared variables have no definite address (they
                     could have a null address). */
                  info_with_pos_sym(ec_constexpr_weak_address,
                                    constant_pos(con, ips), symbol_for(vp),
                                    ips);
                  do_constexpr_fail(result);
                  break;
                }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
              }  /* if */
            }
            break;
          case abk_constant:
          case abk_temporary:
            {
              a_constant_ptr  cp = con->variant.address.variant.constant;
              a_byte          *con_bytes;
              a_type_ptr      ctp = skip_typerefs(cp->type);
              get_stack_bytes(ips, cp, con_bytes);
              if (con_bytes == NULL) {
                alloc_static_object(ips, ctp, con_bytes, &result);
                if (result) {
                  result = extract_value_from_constant(ips, cp, con_bytes,
                                                       con_bytes);
                }  /* if */
                if (!result) break;
#if BUILTIN_FUNCTIONS_ENABLED
                if (cp->is_naturalized) {
                  mark_naturalizable_object(ips, con_bytes);
                }  /* if */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
                mark_complete_object_initialized(con_bytes);
                /* Record a two-way mapping to ensure we always use the same
                   storage, and that we reproduce the original constant if this
                   becomes part of the interpretation result. */
                map_stack_bytes(ips, cp, con_bytes);
                if (!constant_is(cp, ck_string)) {
                  /* String entries are already themselves mapped. */
                  map_stack_bytes(ips, con_bytes, (a_byte*)con);
                }  /* if */
              }  /* if */
              clear_address(value, con_bytes);
              ((a_constexpr_address*)value)->flags |= CA_CONST_STORAGE;
              ((a_constexpr_address*)value)->alloc_seq_number = 0;
              obj_type = ctp;
            }
            break;
          default:
            { /* Create an a_constexpr_address for the runtime constant. */
              clear_runtime_constant_address(value, con);
            }
            break;
        }  /* switch */
        if (obj_type != NULL &&
            (con->implicit_cast || con->variant.address.offset != 0)) {
          a_constexpr_address  *cap = (a_constexpr_address*)value;
          result = translate_il_address_offset(ips, con, cap, obj_type);
        } else {
          check_assertion(con->variant.address.offset == 0 ||
                          is_runtime_data_address(value));
        }  /* if */
      }
      break;
    case ck_ptr_to_member:
      {
        a_base_class_ptr  bcp = con->variant.ptr_to_member.casting_base_class;
        a_constexpr_ptr_to_mem
                          *pm_value = (a_constexpr_ptr_to_mem*)value;
        a_byte_count      offset = 0;
        a_boolean         is_null;
        if (con->variant.ptr_to_member.is_function_ptr) {
          a_routine_ptr  rp = con->variant.ptr_to_member.variant.routine;
          if (rp != NULL && rp->is_consteval &&
              !ips->allow_consteval_routine_node) {
            /* Don't treat a reference to a consteval function as a constant
               unless a constant is really needed.  That keeps the enk_routine
               node in the expression tree so invalid uses can be diagnosed. */
            info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                          constant_pos(con, ips), ips);
            do_constexpr_fail(result);
            break;
          }  /* if */
          pm_value->variant.routine = rp;
          is_null = (rp == NULL);
          pm_value->is_ptr_to_mem_function = TRUE;
        } else {
          pm_value->variant.field = con->variant.ptr_to_member.variant.field;
          is_null = (pm_value->variant.field == NULL);
          pm_value->is_ptr_to_mem_function = FALSE;
        }  /* if */
        if (con->orig_type != NULL && !is_null) {
          /* The pointer-to-member was converted.  Check that the conversion
             is a standard conversion. */
          a_type_ptr  src_mem_type = pm_member_type(con->orig_type);
          a_type_ptr  dst_mem_type = pm_member_type(con->type);
          a_boolean   qualifiers_added;
          if (!is_ptr_to_member_type(con->type) ||
              !member_types_correspond(
                                   dst_mem_type, src_mem_type,
                                   con->variant.ptr_to_member.is_function_ptr,
                                   /*allow_qualifier_or_eh_mismatch=*/TRUE,
                                   &qualifiers_added)) {
            info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                constant_pos(con, ips), con->orig_type,
                                con->type, ips);
            do_constexpr_fail(result);
            break;
           }  /* if */
        }  /* if */
        if (bcp != NULL) {
          /* We cannot look up bcp's offset in the persistent map directly
             because it may be an indirect base class. */
          /* Ensure the derived class has been laid out. */
          (void)f_value_bytes_for_type(ips, bcp->derived_class, &result);
          offset = compute_interpreter_base_offset(
                                                bcp, (a_base_class_ptr*)NULL);
        }  /* if */
        pm_value->this_class_adjustment = offset;
        pm_value->subtract_adjustment =
                                      con->variant.ptr_to_member.cast_to_base;
      }  /* if */
      break;
    case ck_dynamic_init:
      {
        a_constexpr_address  dst_addr;
        set_active_address(ips, &dst_addr, value, complete_object);
        result = do_constexpr_dynamic_init(ips, con->variant.dynamic_init.ptr,
                                           &con->source_corresp.decl_position,
                                           &dst_addr, implied_src);
      }
      break;
    case ck_string:
      {
        a_type_ptr     tp = skip_typerefs(con->type);
        a_type_ptr     etp;
        a_targ_size_t  n_elems, n_con_elems, k, char_size;
        a_byte_count   elem_size;
        a_const_char   *char_ptr;
        a_call_frame_ptr
                       curr_call_frame;
        etp = skip_typerefs(tp->variant.array.element_type);
        char_size = etp->size;
        n_elems = tp->variant.array.variant.number_of_elements;
        n_con_elems = con->variant.string.length / char_size;
        elem_size = value_bytes_for_type(ips, etp, &result);
        char_ptr = con->variant.string.value;
        /* Map the interpreter storage for the string back to the constant
           entry so that that constant can, if needed, be retrieved by
           copy_interpreter_object_to_constant.  For the rare case of a
           string literal used as an rvalue, this is not valid (since an
           rvalue cannot be reused), but we cannot tell here that the constant
           is used as an rvalue: See the handling of enk_constant nodes for
           the compensating code. */
        map_stack_bytes(ips, value, (a_byte*)con);
        curr_call_frame = ips->curr_call_frame;
        if (curr_call_frame != NULL && !in_file_scope(con) &&
            (innermost_function_scope == NULL ||
             innermost_function_scope->variant.routine.ptr !=
                                                  curr_call_frame->routine)) {
          /* The ck_string entry is allocated in a function-scope memory
             region for a function scope that is different from the scope in
             which the result will be needed.  Enter a map entry indicating
             that the string needs to be copied (to avoid memory region issues)
             if it ends up in the final result. */
          a_byte  *placeholder;
          get_stack_bytes(ips, &con->variant.string.value, placeholder);
          if (placeholder == NULL) {
            /* The value associated with &con->variant.string.value doesn't
               matter.  We use con for expediency. */
            map_stack_bytes(ips, &con->variant.string.value, (a_byte*)con);
          }  /* if */
        }  /* if */
        for (k = 0; k<n_elems; k += 1) {
          if (k >= n_con_elems) {
            /* Not all elements are covered.  Zero the remainder. */
            set_integer_value((an_integer_value*)value,
                              (a_host_large_integer)0);
          } else {
            unsigned long char_val = extract_character_from_string(
                                           char_ptr, (unsigned int)char_size);
            set_integer_value((an_integer_value*)value,
                              (a_host_large_integer)char_val);
            if (int_type_is_signed(etp)) {
              sign_extend_integer_value((an_integer_value*)value,
                                        (int)(etp->size * targ_char_bit));
            }  /* if */
            char_ptr += char_size;
          }  /* if */
          mark_subobject_initialized(value, complete_object);
          value += elem_size;
        }  /* for */
      }
      break;
    case ck_aggregate:
      {
        a_type_ptr  tp = skip_typerefs(con->type);
        a_byte      *saved_implied_src_address = NULL;
        if (type_is(tp, tk_array)) {
          a_targ_size_t   n_elems, k, repeat;
          a_byte_count    elem_size;
          a_constant_ptr  elem_con;
          a_type_ptr      etp = tp->variant.array.element_type;
          etp = skip_typerefs(etp);
          n_elems = tp->variant.array.variant.number_of_elements;
          elem_size = value_bytes_for_type(ips, etp, &result);
          if (!result) break;
          if (con->uses_designated_initializers) {
            /* Designated initializers might leave "holes" in the destination
               object, but those should be zero-initialized. */
            init_subobject_to_zero(ips, value, tp, complete_object);
          }  /* if */
          if (implied_src != NULL) {
            saved_implied_src_address = implied_src->address;
          }  /* if */
          elem_con = con->variant.aggregate.first_constant;
          for (k = 0; k<n_elems;) {
            if (elem_con == NULL) {
              /* Not all elements are covered.  Zero the remainder. */
              init_subobject_to_zero(ips, value, etp, complete_object);
              repeat = 1;
            } else if (constant_is(elem_con, ck_designator) &&
                       !elem_con->variant.designator.is_field_designator &&
                       !elem_con->variant.designator.is_generic) {
              /* An array designator. */
              k = elem_con->variant.designator.variant.array_element;
              elem_con = elem_con->next;
              continue;
            } else {
              mark_complete_class_object_if_needed(etp, value);
              if (!copy_val_from_constant(
                        ips, elem_con, value, complete_object, implied_src)) {
                do_constexpr_fail(result);
                break;
              } else if (constant_is(elem_con, ck_string) &&
                         etp->kind == (a_type_kind)tk_array) {
                /* The ck_string contents were copied to a separate array.
                   Undo the mapping of the array storage to the ck_string
                   entry since it is not a persistent association. */
                unmap_stack_bytes(ips, value);
              }  /* if */
              if (constant_is(elem_con, ck_init_repeat)) {
                repeat = elem_con->variant.init_repeat.count;
              } else {
                repeat = 1;
              }  /* if */
              elem_con = elem_con->next;
              mark_subobject_initialized(value, complete_object);
            }  /* if */
            k += repeat;
            value += repeat*elem_size;
            if (implied_src != NULL) implied_src->address += repeat*elem_size;
          }  /* for */
          if (implied_src != NULL) {
            implied_src->address = saved_implied_src_address;
          }  /* if */
        } else if (tp->kind == (a_type_kind)tk_struct ||
                   tp->kind == (a_type_kind)tk_class) {
          a_field_ptr       fp = tp->variant.class_struct_union.field_list;
          a_base_class_ptr  bcp = base_classes_of(tp);
          a_constant_ptr    elem_con;
          if (con->uses_designated_initializers) {
            /* Designated initializers might leave "holes" in the destination
               object, but those should be zero-initialized. */
            init_subobject_to_zero(ips, value, tp, complete_object);
          }  /* if */
          elem_con = con->variant.aggregate.first_constant;
          /* Initialize base subobjects first. */
          for (;;) {
            a_byte_count  offset;
            while (bcp != NULL && !bcp->direct) {
              bcp = bcp->next;
            }  /* while */
            if (bcp == NULL) break;
            get_mapped_byte_count(&persistent_map, bcp, offset);
            /* Record the derivation step. */
            record_subobject_derivation(value+offset, bcp);
            if (elem_con != NULL) {
              if (elem_con->type == tp) {
                /* This can happen with aggregates created in generic contexts,
                   where braces cannot be matched to destination types and so
                   element constants get the same type as their enclosing
                   aggregate.  Although the type itself may be nondependent,
                   no interpretation is needed in these dependent contexts. */
                info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                              &ips->position, ips);
                do_constexpr_fail(result);
                goto done;
              }  /* if */
              if (!copy_val_from_constant(
                              ips, elem_con, value+offset, complete_object)) {
                do_constexpr_fail(result);
                break;
              }  /* if */
              elem_con = elem_con->next;
            } else {
              /* Either a trivial subobject or an error.  Just initialize
                 the subobject to zero. */
              init_subobject_to_zero(ips, value+offset, bcp->type,
                                   complete_object);
            }  /* if */
            mark_subobject_initialized(value+offset, complete_object);
            bcp = bcp->next;
          }  /* for */
          if (!result) break;
          for (;;) {
            a_byte_count  offset;
            a_type_ptr  ftp;
            fp = next_alloc_field(fp);
            if (fp == NULL) {
              /* All fields are initialized: We're done. */
              break;
            }  /* if */
            if (fp->compiler_generated && !fp->is_anonymous_parent_object) {
              /* Ignore fields generated by prelowering. */
              fp = fp->next;
              continue;
            }  /* if */
            if (fp->is_mutable && ips->disallow_mutable_field_load) {
              /* Mutable fields cannot be loaded.  Just skip them for now.
                 An attempt to read this later on will fail since the
                 corresponding subobject won't be initialized (and it cannot
                 be initialized later because the object is const). */
              fp = fp->next;
              if (elem_con != NULL) elem_con = elem_con->next;
              continue;
            }  /* if */
            get_mapped_byte_count(&persistent_map, fp, offset);
            ftp = skip_typerefs(fp->type);
            if (elem_con == NULL) {
              /* No more initializers, but we have more fields.  Zero the
                 remainder of the class value. */
              init_subobject_to_zero(ips, value+offset, ftp, complete_object);
            } else if (constant_is(elem_con, ck_designator) &&
                       elem_con->variant.designator.is_field_designator &&
                       !elem_con->variant.designator.is_generic) {
              /* A field designator. */
              fp = elem_con->variant.designator.variant.field;
              elem_con = elem_con->next;
              continue;
            } else {
              a_byte  *this_bytes = NULL, *dst_bytes = value+offset;
              if (elem_con->implicit_aggr_element &&
                  con->variant.aggregate.has_dynamic_init_component &&
                  class_type_supp(tp)->anonymous_union_kind ==
                                          (an_anonymous_union_kind)auk_none) {
                /* This could involve a default member initializer using a
                   "this" pointer.  That pointer refers to the current class:
                   Ensure a mapping is set up for that. */
                this_bytes = set_up_param_ref_for_this_ptr(
                                                 ips, value, complete_object);
              }  /* if */
              mark_complete_class_object_if_needed(ftp, dst_bytes);
              if (elem_con->type == tp) {
                /* This can happen with aggregates created in generic contexts,
                   where braces cannot be matched to destination types and so
                   element constants get the same type as their enclosing
                   aggregate.  Although the type itself may be nondependent,
                   no interpretation is needed in these dependent contexts. */
                info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                              &ips->position, ips);
                do_constexpr_fail(result);
                goto done;
              }  /* if */
              if (!copy_val_from_constant(
                                 ips, elem_con, dst_bytes, complete_object)) {
                do_constexpr_fail(result);
              } else {
                if (fp->is_bit_field) {
                  /* Fit the value in the bit field width. */
                  trim_bit_field(dst_bytes, fp->bit_size,
                                 fp->bit_field_is_signed, ftp);
                } else if (constant_is(elem_con, ck_string) &&
                           ftp->kind == (a_type_kind)tk_array) {
                  /* The ck_string contents were copied to a separate array.
                     Undo the mapping of the array storage to the ck_string
                     entry since it is not a persistent association. */
                  unmap_stack_bytes(ips, dst_bytes);
                }  /* if */
                mark_subobject_initialized(dst_bytes, complete_object);
                elem_con = elem_con->next;
              }  /* if */
              if (this_bytes != NULL) {
                /* Unmap "this" (possibly restoring a previously active
                   mapping). */
                unmap_param_ref_for_this_ptr(ips, this_bytes);
              }  /* if */
              if (!result) break;
            }  /* if */
            fp = fp->next;
          }  /* for */
          mark_subobject_initialized(value, complete_object);
        } else if (tp->kind == (a_type_kind)tk_union) {
          /* Initialize the first field (unless another field is
             designated). */
          a_field_ptr     fp;
          a_constant_ptr  elem_con;
          a_byte_count    offset;
          a_byte          *this_bytes, *dst_bytes;
          elem_con = con->variant.aggregate.first_constant;
          if (elem_con == NULL) {
            fp = tp->variant.class_struct_union.field_list;
            fp = next_alloc_field(fp);
            if (fp == NULL) {
              /* An empty union: Just clear the "active field". */
              *(a_field_ptr*)value = NULL;
            } else if (con->explicit_braces_on_aggregate) {
              /* A value-initialized union (e.g., "U x{};").  Initialize the
                 first field to zero. */
              a_type_ptr  ftp = skip_typerefs(fp->type);
              get_mapped_byte_count(&persistent_map, fp, offset);
              init_subobject_to_zero(ips, value+offset, ftp, complete_object);
              /* Record the active field. */
              *(a_field_ptr*)value = fp;
            } else {
              info_with_pos_sym(ec_constexpr_missing_initializer_for_field,
                                constant_pos(con, ips), symbol_for(fp), ips);
              do_constexpr_fail(result);
            }  /* if */
            mark_subobject_initialized(value, complete_object);
            break;
          } else if (constant_is(elem_con, ck_designator) &&
                     elem_con->variant.designator.is_field_designator &&
                     !elem_con->variant.designator.is_generic) {
            /* A field designator. */
            fp = elem_con->variant.designator.variant.field;
            elem_con = elem_con->next;
          } else {
            fp = tp->variant.class_struct_union.field_list;
            fp = next_alloc_field(fp);
          }  /* if */
          if (fp == NULL || elem_con == NULL || elem_con->next != NULL) {
            /* Unions with multiple initializers are currently not interpreted
               (that can happen in error cases or with C designators; either
               way, successful interpretation is not required). */
            info_with_pos(ec_constexpr_multiple_union_initializers,
                          constant_pos(con, ips), ips);
            do_constexpr_fail(result);
            break;
          }  /* if */
          this_bytes = NULL;
          if (elem_con->implicit_aggr_element &&
              con->variant.aggregate.has_dynamic_init_component &&
              class_type_supp(tp)->anonymous_union_kind ==
                                          (an_anonymous_union_kind)auk_none) {
            /* This could involve a default member initializer using a "this"
               pointer.  That pointer refers to the current class: Ensure a
               mapping is set up for that. */
            this_bytes = set_up_param_ref_for_this_ptr(
                                                 ips, value, complete_object);
          }  /* if */
          get_mapped_byte_count(&persistent_map, fp, offset);
          dst_bytes = value+offset;
          if (!copy_val_from_constant(
                              ips, elem_con, dst_bytes, complete_object)) {
            do_constexpr_fail(result);
          } else {
            if (fp->is_bit_field) {
              /* Fit the value in the bit field width. */
              a_type_ptr  bftp = skip_typerefs(fp->type);
              trim_bit_field(dst_bytes, fp->bit_size, fp->bit_field_is_signed,
                             bftp);
            } else if (constant_is(elem_con, ck_string) &&
                       skip_typerefs(fp->type)->kind ==
                                                      (a_type_kind)tk_array) {
              /* The ck_string contents were copied to a separate array.
                 Undo the mapping of the array storage to the ck_string
                 entry since it is not a persistent association. */
              unmap_stack_bytes(ips, dst_bytes);
            }  /* if */
            /* Record the active field. */
            *(a_field_ptr*)value = fp;
            mark_subobject_initialized(dst_bytes, complete_object);
          }  /* if */
          mark_subobject_initialized(value, complete_object);
          if (this_bytes != NULL) {
            /* Unmap "this" (possibly restoring a previously active
               mapping). */
            unmap_param_ref_for_this_ptr(ips, this_bytes);
          }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
        } else if (tp->kind == (a_type_kind)tk_vector) {
          a_constant_ptr  elem_con;
          a_type_ptr      etp = skip_typerefs(tp->variant.vector.element_type);
          a_targ_size_t   k, n_elems = tp->size/etp->size;
          a_byte_count    elem_size = value_bytes_for_type(ips, etp, &result);
          if (!result) break;
          elem_con = con->variant.aggregate.first_constant;
          for (k = 0; k<n_elems; k += 1) {
            if (elem_con == NULL) {
              /* Not all elements are covered.  Zero the remainder. */
              memzero(value, size_t_arg((n_elems-k)*elem_size));
              break;
            }  /* if */
            if (!copy_val_from_constant(
                                     ips, elem_con, value, complete_object)) {
              do_constexpr_fail(result);
              break;
            }  /* if */
            elem_con = elem_con->next;
            value += elem_size;
          }  /* for */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
        } else if (tp->kind == (a_type_kind)tk_complex) {
          a_constant_ptr  elem_con;
          a_targ_size_t   k, n_elems = 2;
          a_byte_count    elem_size = sizeof(an_internal_float_value);
          elem_con = con->variant.aggregate.first_constant;
          for (k = 0; k<n_elems; k += 1) {
            if (elem_con == NULL) {
              /* Not all elements are covered.  Zero the remainder. */
              memzero(value, size_t_arg((n_elems-k)*elem_size));
              break;
            }  /* if */
            if (!copy_val_from_constant(
                                     ips, elem_con, value, complete_object)) {
              do_constexpr_fail(result);
              break;
            }  /* if */
            elem_con = elem_con->next;
            value += elem_size;
          }  /* for */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        } else {
          do_constexpr_fail(result);
        }  /* if */
      }
      break;
    case ck_init_repeat:
      {
        a_constant_ptr  elem_con = con->variant.init_repeat.constant;
        if (constant_is(elem_con, ck_dynamic_init) &&
            dyn_init_is(elem_con->variant.dynamic_init.ptr, dik_constructor) &&
            elem_con->variant.dynamic_init.ptr
                    ->variant.constructor.is_array_copy) {
          /* A ck_init_repeat on top of a special dik_constructor entry that
             represents copying an array through repeated constructor calls.
             The whole copy (including the iteration for each element of the
             array) will be handled by the interpretation of the dynamic
             initializer entry. */
          a_constexpr_address  dst_addr;
          set_active_address(ips, &dst_addr, value, complete_object);
          if (!do_constexpr_dynamic_init(
                                      ips, elem_con->variant.dynamic_init.ptr,
                                      &elem_con->source_corresp.decl_position,
                                      &dst_addr)) {
            do_constexpr_fail(result);
          }  /* if */
        } else {
          a_type_ptr      etp = skip_typerefs(elem_con->type);
          a_targ_size_t   n_elems, k;
          a_byte_count    elem_size;
          a_byte          *saved_implied_src_address = NULL;
          n_elems = con->variant.init_repeat.count;
          elem_size = value_bytes_for_type(ips, etp, &result);
          if (!result) break;
          if (implied_src != NULL) {
            saved_implied_src_address = implied_src->address;
          }  /* if */
          for (k = 0; k<n_elems;) {
            mark_complete_class_object_if_needed(etp, value);
            if (!copy_val_from_constant(ips, elem_con, value,
                                        complete_object, implied_src)) {
              do_constexpr_fail(result);
              break;
            }  /* if */
            mark_subobject_initialized(value, complete_object);
            k  += 1;
            value += elem_size;
            if (implied_src != NULL) implied_src->address += elem_size;
          }  /* for */
          if (implied_src != NULL) {
            implied_src->address = saved_implied_src_address;
          }  /* if */
        }  /* if */
      }
      break;
    case ck_void:
      /* void values have no representation: Nothing to do. */
      break;
    default:
      { info_with_pos(ec_constexpr_invalid_constant_kind,
                      constant_pos(con, ips), ips);
        do_constexpr_fail(result);
      }
  }  /* switch */
done:
  return result;
}  /* extract_value_from_constant */


static a_boolean constexpr_copy_object_init_bits(
                                       an_interpreter_state  *ips,
                                       a_type_ptr            tp,
                                       a_source_position     *pos,
                                       a_byte                *src_bytes,
                                       a_byte                *complete_src,
                                       a_byte                *dst_bytes,
                                       a_byte                *complete_dst)
/*
Copy the initialization bitmap of the subobject of type tp located at src_bytes
(with complete object located at complete_src) to the bitmap for the subobject
located at dst_bytes/complete_dst.  Return FALSE and record a diagnostic for
any subobject that is not initialized (the diagnostic is associated with pos).
*/
{
  a_boolean  result = TRUE;

  if (subobject_is_initialized(src_bytes, complete_src)) {
    mark_subobject_initialized(dst_bytes, complete_dst);  
  } else if (type_is(tp, tk_union)) {
    /* An empty union is always considered initialized.  Any other
       uninitialized union is an error. */
    if (!tp->variant.class_struct_union.is_empty_class) {
      do_constexpr_fail(result);
      info_with_pos(ec_object_not_initialized, pos, ips);
    }  /* if */
    goto done;
  } else if ((type_is(tp, tk_struct) || type_is(tp, tk_class)) &&
             (tp->variant.class_struct_union.no_proper_data || cpp20_mode)) {
    /* Empty struct/class type objects are always considered "initialized":
       We check "no_proper_data" rather than "is_empty_class" here because the
       decision should be made independently for subobjects (i.e., a derived
       class that adds no data can be copied if it has no metadata and its
       subobjects are initialized).  Also, in C++20, a struct/class type object
       is not considered uninitialized if each subobject has been given a value
       (e.g., via assignment); in such cases, the whole class might not have
       been marked as initialized. */
    if (tp->variant.class_struct_union.is_empty_class) {
      goto done;
    }  /* if */
  } else {
    do_constexpr_fail(result);
    info_with_pos(ec_object_not_initialized, pos, ips);
    goto done;
  }  /* if */
  switch (tp->kind) {
    case tk_array:
      { /* Recursively handle each array element. */
        a_type_ptr      etp = skip_typerefs(tp->variant.array.element_type);
        a_targ_size_t   k, n_elems;
        a_byte_count    elem_size = value_bytes_for_type(ips, etp, &result);
        n_elems = tp->variant.array.variant.number_of_elements;
        for (k = 0; k<n_elems; ++k){
          if (!constexpr_copy_object_init_bits(ips, etp, pos,
                                               src_bytes, complete_src,
                                               dst_bytes, complete_dst)) {
            result = FALSE;
            break;
          }  /* if */
          src_bytes += elem_size;
          dst_bytes += elem_size;
        }  /* for */
      }
      break;
    case tk_struct:
    case tk_class:
      { /* Recursively handle fields and bases. */
        a_base_class_ptr  bcp = base_classes_of(tp);
        a_field_ptr       fp = tp->variant.class_struct_union.field_list;
        fp = next_alloc_field(fp);
        for (; fp != NULL; fp = next_alloc_field(fp->next)) {
          a_type_ptr    ftp = skip_typerefs(fp->type);
          a_byte_count  offset;
          get_mapped_byte_count(&persistent_map, fp, offset);
          if (!constexpr_copy_object_init_bits(
                                            ips, ftp, pos,
                                            src_bytes+offset, complete_src,
                                            dst_bytes+offset, complete_dst)) {
            result = FALSE;
            break;
          }  /* if */
        }  /* for */
        for (; bcp != NULL; bcp = bcp->next) {
          if (bcp->direct || bcp->is_virtual) {
            a_byte_count  offset;
            get_mapped_byte_count(&persistent_map, bcp, offset);
            if (!constexpr_copy_object_init_bits(
                                            ips, bcp->type, pos,
                                            src_bytes+offset, complete_src,
                                            dst_bytes+offset, complete_dst)) {
              result = FALSE;
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      }
      break;
    case tk_union:
      { /* Recursively handle the active field (if any). */
        a_field_ptr  fp = *(a_field_ptr*)src_bytes;
        if (fp != NULL) {
          a_type_ptr    ftp = skip_typerefs(fp->type);
          a_byte_count  offset;
          get_mapped_byte_count(&persistent_map, fp, offset);
          if (!constexpr_copy_object_init_bits(
                                            ips, ftp, pos,
                                            src_bytes+offset, complete_src,
                                            dst_bytes+offset, complete_dst)) {
            result = FALSE;
            break;
          }  /* if */
        }  /* if */
      }
      break;
    default:
      /* Nothing more to do. */
      break;
  }  /* switch */
done:
  return result;
}  /* constexpr_copy_object_init_bits */


static a_boolean constexpr_copy_object(an_interpreter_state  *ips,
                                       a_type_ptr            tp,
                                       a_source_position     *pos,
                                       a_byte                *src_bytes,
                                       a_byte                *complete_src,
                                       a_byte                *dst_bytes,
                                       a_byte                *complete_dst)
/*
Copy an object of the given type from one interpreter storage location
(src_bytes, complete_src) to another (dst_bytes, complete_dst).  Record a
diagnostic (associated with pos) if attempting to copy an uninitialized
subobject.
*/
{
  a_boolean     result = TRUE;
  a_byte_count  n_bytes = value_bytes_for_type(ips, tp, &result);

  if (result) {
    if (constexpr_copy_object_init_bits(ips, tp, pos,
                                        src_bytes, complete_src,
                                        dst_bytes, complete_dst)) {
      (void)memcpy(dst_bytes, src_bytes, size_t_arg(n_bytes));
    } else {
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* constexpr_copy_object */


static a_byte* do_constexpr_alloc_variable(an_interpreter_state  *ips,
                                           a_variable_ptr        vp,
                                           a_boolean             *p_result)
/*
Allocate storage for the given variable and map the variable entry to that
storage.  This includes bookkeeping for lifetime management (allocation
sequence numbers) and for restoring a previous mapping when this mapping will
be removed.
*/
{
  a_boolean     result = TRUE;
  a_type_ptr    vtp = skip_typerefs(vp->type);
  a_byte_count  n_bytes = value_bytes_for_type(ips, vtp, &result);
  a_byte        *var_storage;

  if (result) {
    /* Allocate the variable storage and store the allocation sequence number
       and previous storage pointer (if any) right after the object. */
    a_byte_count   with_postfix_bytes;
    a_var_postfix  *postfix;
    do_host_alignment(n_bytes);
    with_postfix_bytes = n_bytes+sizeof(a_var_postfix);
    alloc_complete_object(ips, with_postfix_bytes, vtp, var_storage);
    postfix = (a_var_postfix*)(var_storage+n_bytes);
    postfix->alloc_seq_number = ips->storage_stack.alloc_seq_number;
    map_or_replace_ptr(&ips->map, vp, var_storage, postfix->prev_storage);
  } else {
    /* The size of the variable is unknown or too large. */
    *p_result = FALSE;
    var_storage = NULL;
  }  /* if */
  return var_storage;
}  /* do_constexpr_alloc_variable */


static void do_constexpr_unmap_variable(an_interpreter_state  *ips,
                                        a_variable_ptr        vp)
/*
Remove the mapping of vp to its associated storage and restore the previous
mapping if there was one.
*/
{
  a_byte  *var_storage;

  get_mapped_ptr(&ips->map, vp, var_storage);
  if (var_storage != NULL) {
    a_boolean      result = TRUE;
    a_type_ptr     vtp = skip_typerefs(vp->type);
    a_byte_count   n_bytes = value_bytes_for_type(ips, vtp, &result);
    a_var_postfix  *postfix;
    do_host_alignment(n_bytes);
    postfix = (a_var_postfix*)(var_storage+n_bytes);
    if (postfix->prev_storage == NULL) {
      unmap_ptr(&ips->map, vp);
    } else {
      replace_mapped_ptr(&ips->map, vp, postfix->prev_storage);
    }  /* if */
  } else {
    /* No associated storage: This can happen in error cases. */
  }  /* if */
}  /* do_constexpr_unmap_variable */


static a_boolean do_array_constructor_copy(an_interpreter_state  *ips,
                                           a_dynamic_init_ptr    dip,
                                           a_source_position     *pos,
                                           a_constexpr_address   *cap)
/*
The given dik_constructor dynamic initialization entry has its is_array_copy
flag set to TRUE.  Perform the array copy it represents (to storage indicated
by cap).  Return TRUE if successful.  Otherwise, return FALSE and update *ips
accordingly.  Associate diagnostics with the given position.
*/
{   
  a_boolean            result = TRUE, clear_lvalue = FALSE;
  an_expr_node_ptr     array_expr = dip->variant.constructor.args;
  a_byte               *lvalue;
  a_constexpr_address  *src_addr;
  a_byte_count         n_lvalue_bytes;
  a_type_ptr           tp = skip_typerefs(array_expr->type), elem_type;

  if (!array_expr->is_lvalue && !array_expr->is_xvalue) {
    /* The array to copy may be an rvalue temporary.  By setting the is_lvalue
       flag, the temporary is still created, but do_constexpr_expression will
       return the address of it instead. */
    array_expr->is_lvalue = TRUE;
    clear_lvalue = TRUE;
  }  /* if */
  n_lvalue_bytes = expr_result_size(ips, array_expr, tp, &result); 
  if (!result) goto done;
  alloc_complete_object(ips, n_lvalue_bytes, tp, lvalue);
  src_addr = (a_constexpr_address*)lvalue;
  if (!do_constexpr_expression(ips, array_expr, lvalue, lvalue)) {
    do_constexpr_fail(result);
  } else if (is_runtime_data_address(src_addr)) {
    info_with_pos(ec_constexpr_access_to_runtime_storage, pos, ips);
    do_constexpr_fail(result);
  } else {
    /* Perform the copy by creating an "implied-source" dynamic initializer
       from the given *dip entry, and interpreting it for every element of
       the array. */
    a_targ_size_t   k, length = 1;
    a_byte_count    elem_size;
    a_dynamic_init  dip_copy = *dip;
    dip_copy.variant.constructor.args = array_expr->next;
    dip_copy.variant.constructor.is_array_copy = FALSE;
    dip_copy.variant.constructor
                    .is_copy_constructor_with_implied_source = TRUE;
    check_assertion(tp->kind == (a_type_kind)tk_array);
    elem_type = tp;
    do {
      a_targ_size_t  dim = elem_type->variant.array.variant.number_of_elements;
      if (dim == 0 && !elem_type->variant.array.bound_is_zero) {
        info_with_pos_type(ec_constexpr_type_invalid, &ips->position, tp, ips);
        do_constexpr_fail(result);
        goto done;
      }  /* if */
      length *= dim;
      elem_type = skip_typerefs(elem_type->variant.array.element_type);
    } while (elem_type->kind == (a_type_kind)tk_array);
    elem_size = value_bytes_for_type(ips, elem_type, &result);
    if (!result) goto done;
    /* Decay src_addr from the address of the array to the address of its
       first element. */
    src_addr->flags |= CA_ARRAY_ELEMENT;
    src_addr->length = (unsigned int)length;
    if (is_variant_path(src_addr)) {
      src_addr->variant.variant_path->base_address = src_addr->address;
    } else {
      src_addr->variant.base_address = src_addr->address;
    }  /* if */
    a_constexpr_address  dst_addr = *cap;
    for (k = 0; k<length; ++k) {
      if (!do_constexpr_ctor(ips, &dip_copy, pos, &dst_addr, src_addr)) {
        do_constexpr_fail(result);
        goto done;
      } else {
        src_addr->address += elem_size;
        dst_addr.address += elem_size;
      }  /* if */
    }  /* for */
  }  /* if */
done:
  if (clear_lvalue) {
    array_expr->is_lvalue = FALSE;
  }  /* if */
  return result;
}  /* do_array_constructor_copy */


/* Defined later in this file. */
static a_boolean do_constexpr_lambda(an_interpreter_state *ips,
                                     a_dynamic_init_ptr   dip,
                                     a_source_position    *pos,
                                     a_byte               *result_storage,
                                     a_byte               *complete_object);


static a_boolean do_constexpr_dynamic_init(
                                        an_interpreter_state  *ips,
                                        a_dynamic_init_ptr    dip,
                                        a_source_position     *pos,
                                        a_constexpr_address   *dst_addr,
                      /* Defaulted: */  a_constexpr_address   *implied_src)
/*
Evaluate the given dynamic initialization of the storage described by dst_addr.
ips is the interpreter state and pos the default position for diagnostics.  If
implied_src is non-NULL, this is being invoked from the evaluation of a copy
or move constructor and the source object is stored at the location indicated
by implied_src.
*/
{
  a_boolean  result = FALSE;

  switch (dip->kind) {
    case dik_constant:
    case dik_nonconstant_aggregate:
      result = copy_val_from_constant(ips, dip->variant.constant.ptr,
                                      dst_addr->address,
                                      dst_addr->complete_object, implied_src);
      break;
    case dik_lambda:
      if (constexpr_lambdas_enabled) {
        result = do_constexpr_lambda(ips, dip, pos, dst_addr->address,
                                      dst_addr->complete_object);
      } else {
        info_with_pos(ec_lambda_not_constant_expr, pos, ips);
        do_constexpr_fail(result);
      }  /* if */
      break;
    case dik_module:
      if (dip->variant.constant.ptr == NULL) {
        /* The initializer exists in another TU, but isn't available to us. */
        do_constexpr_fail(result);
      } else {
        result = copy_val_from_constant(ips, dip->variant.constant.ptr,
                                        dst_addr->address,
                                        dst_addr->complete_object,
                                        implied_src);
      }  /* if */
      break;
    case dik_expression:
    case dik_class_result_via_ctor:
      result = do_constexpr_expression(ips, dip->variant.expression,
                                       dst_addr->address,
                                       dst_addr->complete_object);
      break;
    case dik_constructor:
      if (dip->variant.constructor.is_array_copy) {
        result = do_array_constructor_copy(ips, dip, pos, dst_addr);
      } else {
        result = do_constexpr_ctor(ips, dip, pos, dst_addr, implied_src);
      }  /* if */
      break;
    case dik_bitwise_copy:
      { an_expr_node_ptr  source_expr = dip->variant.bitwise_copy.source;
        if (source_expr != NULL) {
          a_type_ptr  tp = skip_typerefs(source_expr->type);
          a_boolean   restore_lvalue = FALSE;
          if (tp->kind == (a_type_kind)tk_array && source_expr->is_lvalue) {
            restore_lvalue = TRUE;
            source_expr->is_lvalue = FALSE;
          }  /* if */
          result = do_constexpr_expression(ips, source_expr, dst_addr->address,
                                           dst_addr->complete_object);
          if (restore_lvalue) source_expr->is_lvalue = TRUE;
        } else {
          /* An implicit source: The caller should catch those cases. */
          unexpected_condition();
        }  /* if */
      }
      break;
    case dik_zero:
    case dik_none:
      /* Nothing to do. */
      result = TRUE;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (dip->is_reused_value && result) {
    /* Record the location of a value to reuse. */
    a_byte  *discard;
    map_or_replace_ptr(&ips->map, dip, dst_addr->address, discard);
    *(a_byte**)&discard = discard;
                        /* To avoid spurious warnings from certain
                           compilers and tools. */
  }  /* if */
  return result;
}  /* do_constexpr_dynamic_init */


static a_boolean register_destruction(an_interpreter_state  *ips,
                                      a_dynamic_init_ptr    dip,
                                      a_byte                *sub_obj,
                                      a_byte                *complete_obj,
                                      a_source_position     *pos)
/*
Register a destruction to be performed when the current storage stack state
is released.
*/
{
  a_boolean  result = TRUE;

  if (!constexpr_dynamic_alloc_enabled) {
    info_with_pos(ec_constexpr_ctor_with_dtor, pos, ips);
    do_constexpr_fail(result);
  } else {
    /* Register the destruction in the current storage stack state. */
    a_byte                   *d_bytes;
    a_constexpr_destruction  *destruction;
    alloc_stack_bytes(ips, sizeof(a_constexpr_destruction), d_bytes);
    destruction = (a_constexpr_destruction*)d_bytes;
    destruction->next = ips->storage_stack.destructions;
    destruction->dip = dip;
    destruction->sub_obj = sub_obj;
    destruction->complete_obj = complete_obj;
    destruction->pos = pos;
    ips->storage_stack.destructions = destruction;
  }  /* if */
  return result;
}  /* register_destruction */


static a_boolean register_extended_destruction(
                                          an_interpreter_state  *ips,
                                          a_dynamic_init_ptr    dip,
                                          a_byte                *sub_obj,
                                          a_byte                *complete_obj,
                                          a_source_position     *pos)
/*
Register a destruction to be performed when the "extension" storage stack
state is released.
*/
{
  a_boolean  result = TRUE;

  if (!constexpr_dynamic_alloc_enabled) {
    info_with_pos(ec_constexpr_ctor_with_dtor, pos, ips);
    do_constexpr_fail(result);
  } else {
    /* Register the destruction in the extension storage stack state. */
    a_byte                   *d_bytes;
    a_constexpr_destruction  *destruction;
    alloc_bytes(ips->extension_state, sizeof(a_constexpr_destruction),
                d_bytes);
    destruction = (a_constexpr_destruction*)d_bytes;
    destruction->next = ips->extension_state->destructions;
    destruction->dip = dip;
    destruction->sub_obj = sub_obj;
    destruction->complete_obj = complete_obj;
    destruction->pos = pos;
    ips->extension_state->destructions = destruction;
  }  /* if */
  return result;
}  /* register_extended_destruction */


static a_boolean do_constexpr_init_variable(an_interpreter_state   *ips,
                                            a_variable_ptr         vp,
                                            a_byte                 *storage,
                                            a_source_position      *pos)
/*
Evaluate the (dynamic) initializer of the given variable.  Return FALSE if an
error occurs and use pos as the default position for recorded diagnostics.
If storage is non-NULL, it points to the storage occupied by the variable;
otherwise, this routine will look up that storage in ips->map.
*/
{
  a_boolean              result = TRUE;
  a_storage_stack_state  saved_stack_for_full_expr;
  a_dynamic_init_ptr     dip = vp->initializer.dynamic;
  a_type_ptr             tp = skip_typerefs(vp->type);

  if (dip == NULL) {
    /* This can happen in error cases. */
    ips->input_error = TRUE;
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  save_storage_stack(ips, saved_stack_for_full_expr);
  if (vp->extends_lifetime) {
    /* Start a new stack for temporaries in this expression, but keep a
       pointer to the original stack to allocate the lifetime-extended
       temporary. */
    init_constexpr_stack(&ips->storage_stack);
    ips->storage_stack.alloc_seq_number = ips->curr_alloc_seq_number;
    ips->extension_state = &saved_stack_for_full_expr;
  }  /* if */
  if (storage == NULL) {
    get_stack_bytes(ips, vp, storage);
  }  /* if */
  if (dyn_init_is(dip, dik_zero)) {
    init_subobject_to_zero(ips, storage, tp, storage);
  } else {
    a_boolean            saved_is_constant_evaluated =
                                                   ips->is_constant_evaluated;
    a_constexpr_address  dst_addr;
    set_active_address(ips, &dst_addr, storage, storage);
    if (vp->init_kind == (an_init_kind)initk_static ||
        (type_is(tp, tk_integer) && is_const_qualified_type(vp->type))) {
      ips->is_constant_evaluated = TRUE;
    }  /* if */
    if (do_constexpr_dynamic_init(ips, dip, pos, &dst_addr)) {
      if (!is_immediate_class_type(tp) && !type_is(tp, tk_array)) {
        mark_complete_object_initialized(storage);
      }  /* if */
    } else {
      do_constexpr_fail(result);
    }  /* if */
    ips->is_constant_evaluated = saved_is_constant_evaluated;
  }  /* if */
  if (vp->extends_lifetime) {
    /* Release the ordinary storage stack blocks for this expression.
       The large blocks will be released by the call to
       restore_storage_stack below. */
    release_constexpr_stack(&ips->storage_stack);
  }  /* if */
  restore_storage_stack(ips, saved_stack_for_full_expr, result);
  if (result && dip->destructor != NULL) {
    result = register_destruction(ips, dip, storage, storage, pos);
  }  /* if */
done:
  return result;
}  /* do_constexpr_init_variable */


static a_boolean do_constexpr_condition_alloc(
                                            an_interpreter_state   *ips,
                                            an_expr_node_ptr       expr,
                                            a_storage_stack_state  *vs_state)
/*
expr is an enk_condition node representing the condition expression of a
statement (i.e., the <expr> in "if (<expr>) ...", "switch (<expr>) ...", etc.).
Allocate and map the associated variable (it will be initialized by a call to
do_constexpr_condition).  Save the previous storage state in *vs_state if
successful.
*/
{
  a_boolean                   result = TRUE;
  a_condition_supplement_ptr  csp = expr->variant.condition;
  a_statement_ptr             init = csp->initialization;
  a_dynamic_init_ptr          cond_var_init = csp->dynamic_init;

  save_storage_stack(ips, *vs_state);
  if (init != NULL) {
    if (init->kind == (a_statement_kind)stmk_decl) {
      /* Allocate storage for any variables. */
      an_il_entity_list_entry_ptr  p = init->variant.decl.entities;
      for (; p != NULL; p = p->next) {
        if (p->entity.kind == iek_variable) {
          a_variable_ptr  vp = (a_variable_ptr)p->entity.ptr;
          (void)do_constexpr_alloc_variable(ips, vp, &result);
          if (!result) break;
        }  /* if */
      }  /* for */
    } else if (init->kind == (a_statement_kind)stmk_expr) {
      /* Nothing to allocate just now. */
    } else {
      unexpected_condition();
    }  /* if */
  }  /* if */
  if (result && cond_var_init != NULL) {
    a_variable_ptr  cond_var = cond_var_init->variable;
    (void)do_constexpr_alloc_variable(ips, cond_var, &result);
  }  /* if */
  if (!result) {
    restore_storage_stack(ips, *vs_state, result);
  }  /* if */
  return result;
}  /* do_constexpr_condition_alloc */


static void do_constexpr_condition_dealloc(an_interpreter_state   *ips,
                                           an_expr_node_ptr       expr,
                                           a_storage_stack_state  *vs_state,
                                           a_boolean              *p_result)
/*
expr is an enk_condition node representing the condition expression of a
statement (i.e., the <expr> in "if (<expr>) ...", "switch (<expr>) ...", etc.).
Deallocate and unmap all associated variables.  Restore the storage state
recorded in *vs_state.  Set *p_result to FALSE if this fails (due to a failing
destructor evaluation).
*/
{
  a_condition_supplement_ptr  csp = expr->variant.condition;
  a_statement_ptr             init = csp->initialization;
  a_dynamic_init_ptr          cond_var_init = csp->dynamic_init;

  if (cond_var_init != NULL) {
    a_variable_ptr  cond_var = cond_var_init->variable;
    do_constexpr_unmap_variable(ips, cond_var);
  }  /* if */
  if (init != NULL) {
    if (init->kind == (a_statement_kind)stmk_decl) {
      /* Unmap storage for all declared variables. */
      an_il_entity_list_entry_ptr  p = init->variant.decl.entities;
      for (; p != NULL; p = p->next) {
        if (p->entity.kind == iek_variable) {
          a_variable_ptr  vp = (a_variable_ptr)p->entity.ptr;
          do_constexpr_unmap_variable(ips, vp);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  /* Reclaim the storage of the variables that were unmapped above. */
  restore_storage_stack(ips, *vs_state, *p_result);
}  /* do_constexpr_condition_dealloc */


static a_boolean do_constexpr_condition(a_boolean             has_cond_var,
                                        an_interpreter_state  *ips,
                                        an_expr_node_ptr      expr,
                                        a_type_ptr            expr_type,
                                        a_byte                *value)
/*
Evaluate a condition expression (expr) of a statement (i.e., the <expr> in
"if (<expr>) ...", "switch (<expr>) ...", etc.) and place the result in the
storage pointed to by value.  If has_cond_var is TRUE, expr is an enk_condition
node (such nodes are not handled by do_constexpr_expression).  expr_type is
skip_typerefs(expr->type).
*/
{
  a_boolean              result = TRUE;
  a_storage_stack_state  saved_stack_for_full_expr;
  an_expr_node_ptr       expr_to_evaluate;

  save_storage_stack((ips), saved_stack_for_full_expr);
  if (has_cond_var) {
    a_condition_supplement_ptr  csp = expr->variant.condition;
    a_dynamic_init_ptr          cond_var_init = csp->dynamic_init;
    a_statement_ptr             init = csp->initialization;
    if (init != NULL) {
      /* A C++17-style initializer.  E.g., "if (int x = f(); x+1) ...". */
      if (init->kind == (a_statement_kind)stmk_decl) {
        /* Evaluate the initializer of each variable. */
        an_il_entity_list_entry_ptr  p = init->variant.decl.entities;
        for (; p != NULL; p = p->next) {
          if (p->entity.kind == iek_variable) {
            a_variable_ptr  vp = (a_variable_ptr)p->entity.ptr;
            if (!do_constexpr_init_variable(
                                         ips, vp, (a_byte*)NULL,
                                         &vp->source_corresp.decl_position)) {
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      } else if (init->kind == (a_statement_kind)stmk_expr) {
        result = do_constexpr_statement(ips, init);
      } else {
        unexpected_condition();
      }  /* if */
    }  /* if */
    if (cond_var_init != NULL && result) {
      a_variable_ptr  cond_var = cond_var_init->variable;
      if (!do_constexpr_init_variable(
                                  ips, cond_var, (a_byte*)NULL,
                                   &cond_var->source_corresp.decl_position)) {
        /* Zero the variable, so do_constexpr_condition_cleanup does not
           attempt to access uninitialized storage. */
        a_type_ptr    vtp = skip_typerefs(cond_var->type);
        a_boolean     local_result = TRUE;
        a_byte_count  n_bytes = value_bytes_for_type(ips, vtp, &local_result);
        a_byte        *var_bytes;
        check_assertion(local_result);
        get_stack_bytes(ips, cond_var, var_bytes);
        memzero(var_bytes, size_t_arg(n_bytes));
      }  /* if */
    }  /* if */
    expr_to_evaluate = csp->expr;
  } else {
    result = TRUE;
    expr_to_evaluate = expr;
  }  /* if */
  if (result) {
    if (do_constexpr_expression(ips, expr_to_evaluate, value, value)) {
      release_address_structures(expr, expr_type, value);
      restore_storage_stack(ips, saved_stack_for_full_expr, result);
    } else {
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* do_constexpr_condition */


static void do_constexpr_condition_cleanup(an_interpreter_state   *ips,
                                           an_expr_node_ptr       expr)
/*
expr is an enk_condition node representing the condition expression of a
statement (i.e., the <expr> in "if (<expr>) ...", "switch (<expr>) ...", etc.).
Clean up the variable values (currently, that means disposing of the "variant
path" structures).  This does not deallocate or unmap the variables (since for
loop constructs they may be needed again).
*/
{
  a_condition_supplement_ptr  csp = expr->variant.condition;
  a_statement_ptr             init = csp->initialization;
  a_dynamic_init_ptr          cond_var_init = csp->dynamic_init;

  if (cond_var_init != NULL) {
    a_variable_ptr  cond_var = cond_var_init->variable;
    if (skip_typerefs(cond_var->type)->kind == (a_type_kind)tk_pointer) {
      a_byte               *var_bytes;
      a_constexpr_address  *cap;
      get_stack_bytes(ips, cond_var, var_bytes);
      cap = (a_constexpr_address*)var_bytes;
      release_variant_path_if_needed(cap);
    }  /* if */
  }  /* if */
  if (init != NULL) {
    if (init->kind == (a_statement_kind)stmk_decl) {
      /* Clean up side structures for any declared variables. */
      an_il_entity_list_entry_ptr  p = init->variant.decl.entities;
      for (; p != NULL; p = p->next) {
        if (p->entity.kind == iek_variable) {
          a_variable_ptr  vp = (a_variable_ptr)p->entity.ptr;
          if (skip_typerefs(vp->type)->kind == (a_type_kind)tk_pointer) {
            a_byte               *var_bytes;
            a_constexpr_address  *cap;
            get_stack_bytes(ips, vp, var_bytes);
            cap = (a_constexpr_address*)var_bytes;
            release_variant_path_if_needed(cap);
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* do_constexpr_condition_cleanup */


static a_boolean init_static_variables(an_interpreter_state  *ips,
                                       a_scope_ptr           scope)
/*
Initialize a limited set of local static variables.  This function should only
be called for the local static variables associated with constructs like
"__func__".
*/
{
  a_boolean       result = TRUE;
  a_variable_ptr  vp = scope->variables;

  for (; vp != NULL; vp = vp->next) {
    a_byte  *var_bytes;
    get_stack_bytes(ips, vp, var_bytes);
    if (var_bytes == NULL) {
      a_type_ptr  tp = vp->type;
      if (!vp->compiler_generated ||
          !is_const_qualified_type(tp) ||
          is_volatile_qualified_type(tp) ||
          vp->init_kind != (an_init_kind)initk_static ||
          vp->initializer.constant == NULL) {
        /* This is not a static variable of interest. */
        continue;
      }  /* if */
      tp = skip_typerefs(tp);
      alloc_static_object(ips, tp, var_bytes, &result);
      if (!result) break;
      map_stack_bytes(ips, vp, var_bytes);
      result = extract_value_from_constant(ips, vp->initializer.constant,
                                           var_bytes, var_bytes);
      if (!result) break;
    }  /* if */
  }  /* for */
  return result;
}  /* init_static_variables */


static a_boolean do_constexpr_block_statement(an_interpreter_state  *ips,
                                              a_statement_ptr       block_stmt,
                                              a_scope_ptr           scope)
/*
Interpret the given block statement and its associated scope (if any).
*/
{
  a_boolean              result = TRUE, local_storage = FALSE;
  a_storage_stack_state  saved_stack;
  a_statement_ptr        stmt = block_stmt->variant.block.statements;

  init_storage_stack_state_to_silence_GCC(saved_stack);
  if (scope != NULL) {
    /* If local variables are going to be allocated (which will be done when
       their corresponding stmk_init statement is interpreted), save the
       current allocation state so we can efficiently deallocate those
       variables below.  Also do this for the top-level block of a statement
       because any parameters have already been associated with the allocation
       sequence number about to be recorded. */
    a_variable_ptr  vp = scope->nonstatic_variables;
    if (vp != NULL || scope_is(scope, sck_function)) {
      save_storage_stack(ips, saved_stack);
      local_storage = TRUE;
      for (; vp != NULL; vp = vp->next) {
        /* Ordinarily, local variables are allocated and initialized when
           interpreting their associated stmk_init entry.  In C++20, however,
           uninitialized variables are permitted in constexpr expressions
           (via changes introduced by P1331R2). */
        if (vp->init_kind == (an_init_kind)initk_none) {
          (void)do_constexpr_alloc_variable(ips, vp, &result);
        }  /* if */
      }  /* for */
    }  /* if */
    if (scope->variables != NULL) {
      /* There are local static variables.  That is normally not possible in
         constexpr functions, but the implied static variables for __func__
         and similar constructs are permitted. */
      if (!init_static_variables(ips, scope)) {
        result = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (result) {
    /* Interpret the statements in the block. */
    for (; result && stmt != NULL; stmt = stmt->next) {
      result = do_constexpr_statement(ips, stmt);
      if (ips->curr_call_frame->return_active ||
          ips->curr_call_frame->loop_break_active ||
          ips->curr_call_frame->continue_active ||
          ips->curr_call_frame->switch_break_active) {
        /* A branching statement ends execution for this block. */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  /* Release and unmap the local storage if necessary. */
  if (local_storage) {
    a_variable_ptr  vp = scope->nonstatic_variables;
    for (; vp != NULL; vp = vp->next) {
      if (skip_typerefs(vp->type)->kind == (a_type_kind)tk_pointer) {
        a_byte  *var_bytes;
        get_stack_bytes(ips, vp, var_bytes);
        if (var_bytes != NULL && result) {
          /* If interpretation failed, the initialization state of var_bytes
             is uncertain.  Rely on reclaim_variant_path_entries to recover
             entries in that case. */
          release_variant_path_if_needed(var_bytes);
        }  /* if */
      }  /* if */
      do_constexpr_unmap_variable(ips, vp);
    }  /* for */
    restore_storage_stack(ips, saved_stack, result);
  }  /* if */
  return result;
}  /* do_constexpr_block_statement */


static a_boolean do_constexpr_for_statement(an_interpreter_state  *ips,
                                            a_statement_ptr       stmt,
                                            a_boolean             do_continue)
/*
Interpret the given for-statement.  If do_continue is TRUE, we have jumped
into the body of the loop and encountered a "continue" statement: Skip the
initialization and execute the increment before the main iteration.
*/
{
  a_boolean              result = TRUE;
  a_storage_stack_state  saved_stack, cond_saved_stack;
  a_for_loop_ptr         loop_info = stmt->variant.for_loop.extra_info;
  a_statement_ptr        init = loop_info->initialization;

  /* We may have to allocate storage for the increment expression result and/or
     variables declared in the for-init declaration.  Save the current storage
     state to enable deallocation when we're done. */
  save_storage_stack(ips, saved_stack);
  /* Run the initialization statement (if any). */
  if (init != NULL && !do_continue && !do_constexpr_statement(ips, init)) {
    do_constexpr_fail(result);
  } else {
    an_expr_node_ptr  expr = stmt->expr, incr = loop_info->increment;
    a_byte            *expr_value, *incr_value;
    a_type_ptr        tp, incr_type;
    a_byte_count      n_bytes;
    a_boolean         ovfl, has_cond_var;
    a_host_large_integer
                      bool_val = FALSE;
    if (expr != NULL) {
      /* Allocate storage for the test expression result (a boolean). */
      tp = skip_typerefs(expr->type);
      n_bytes = expr_result_size(ips, expr, tp, &result);
      alloc_complete_object(ips, n_bytes, tp, expr_value);
      /* Check if we have to allocate a condition variable. */
      has_cond_var = node_is(expr, enk_condition);
      if (has_cond_var &&
          !do_constexpr_condition_alloc(ips, expr, &cond_saved_stack)) {
        do_constexpr_fail(result);
        has_cond_var = FALSE;
        goto unmap_storage;
      }  /* if */
    } else {
      has_cond_var = FALSE;
      /* Needed only to avoid spurious GNU compiler optimizer
         warnings. */
      tp = NULL;
      expr_value = NULL;
    }  /* if */
    incr = loop_info->increment;
    if (incr != NULL) {
      incr_type = skip_typerefs(incr->type);
      n_bytes = expr_result_size(ips, incr, incr_type, &result);
      if (!result) goto unmap_storage;
      alloc_complete_object(ips, n_bytes, incr_type, incr_value);
      if (do_continue) {
        /* This function was called to implement a "continue" statement after
           having jumped into the loop body (through a switch statement). The
           loop increment has to be executed first. */
        do_constexpr_full_expression(
                                   ips, incr, incr_value, incr_value, result);
        if (result) {
          release_address_structures(incr, incr_type, incr_value);
        } else {
          /* The initialization state of incr_value is uncertain.
             Rely on reclaim_variant_path_entries to recover entries
             in that case. */
        }  /* if */
      }  /* if */
    } else {
      /* To avoid spurious warnings from certain tools. */
      incr_type = NULL;
      incr_value = expr_value;
    }  /* if */
    do {
      /* Evaluate the test expression. */
      if (cost_exceeded(ips)) {
        more_info_diagnostic(ec_excessive_constexpr_complexity, &ips->position,
                             &ips->diag_list);
        do_constexpr_fail(result);
      } else if (expr != NULL) {
        result = do_constexpr_condition(has_cond_var, ips, expr, tp,
                                        expr_value);
        ips->cost += 1;
      }  /* if */
      if (result) {
        /* Evaluation of the test expression succeeded.  Get its value
           to see if the dependent statement should be executed. */
        if (expr != NULL) {
          get_int_val_from(expr_value, tp, bool_val, ovfl);
        } else {
          bool_val = TRUE;
          ovfl = FALSE;
        }  /* if */
        if (!ovfl && bool_val) {
          /* Execute the dependent statement. */
          result = do_constexpr_statement(
                                       ips, stmt->variant.for_loop.statement);
          if (result) {
            /* Execution of the dependent statement succeeded, so evaluate
               the increment expression (if any) unless we hit a branching
               statement. */
            if (ips->curr_call_frame->return_active) {
              /* Stop the loop (leave the flag active since we may have to
                 break out of other constructs). */
              bool_val = FALSE;
            } else if (ips->curr_call_frame->loop_break_active) {
              /* Stop the loop (which completes the execution of the break
                 statement). */
              ips->curr_call_frame->loop_break_active = FALSE;
              bool_val = FALSE;
            } else {
              if (ips->curr_call_frame->continue_active) {
                /* Continue, but clear the continue_active flag since we've
                   reached the point of continuation. */
                ips->curr_call_frame->continue_active = FALSE;
              }  /* if */
              if (incr != NULL) {
                do_constexpr_full_expression(
                                   ips, incr, incr_value, incr_value, result);
                if (result) {
                  release_address_structures(incr, incr_type, incr_value);
                } else {
                  /* The initialization state of incr_value is uncertain.
                     Rely on reclaim_variant_path_entries to recover entries
                     in that case. */
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }   /* if */
      if (has_cond_var) {
        do_constexpr_condition_cleanup(ips, expr);
      }  /* if */
    } while (result && bool_val);
unmap_storage:
    if (has_cond_var) {
      do_constexpr_condition_dealloc(ips, expr, &cond_saved_stack, &result);
    }  /* if */
  }  /* if */
  { /* Unmap the local storage if necessary. */
    a_scope_ptr  init_scope = loop_info->for_init_scope;
    if (init_scope != NULL) {
      a_variable_ptr  vp = init_scope->nonstatic_variables;
      while (vp != NULL) {
        do_constexpr_unmap_variable(ips, vp);
        vp = vp->next;
      }  /* while */
    }  /* if */
  }
  restore_storage_stack(ips, saved_stack, result);
  return result;
}  /* do_constexpr_for_statement */


static a_boolean do_constexpr_range_based_for_statement(
                                                   an_interpreter_state  *ips,
                                                   a_statement_ptr       stmt)
/*
Interpret the given range-based for-statement.
*/
{
  a_boolean              result = TRUE;
  a_storage_stack_state  saved_stack;
  a_range_based_for_loop_ptr
                         loop_info =
                                stmt->variant.range_based_for_loop.extra_info;
  a_variable_ptr         vp[4];
  a_byte                 *var_storage[4];
  int                    k;
  a_dynamic_init_ptr     dip;
  save_storage_stack(ips, saved_stack);
  /* Acquire storage for the iteration variables. */
  vp[0] = loop_info->iterator;
  vp[1] = loop_info->range;
  vp[2] = loop_info->begin;
  vp[3] = loop_info->end;
  if (vp[0] == NULL || vp[1] == NULL || vp[2] == NULL || vp[3] == NULL) {
    /* This can only occur in error cases. */
    expect_error();
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  for (k = 0; k<4; ++k) {
    var_storage[k] = do_constexpr_alloc_variable(ips, vp[k], &result);
    if (result) mark_complete_object_initialized(var_storage[k]);
  }  /* for */
  if (!result) goto unmap_storage;
  /* Initialize the range and its delimiters: */
  for (k = 1; k<4; ++k) {
    a_constexpr_address  var_addr;
    set_active_address(ips, &var_addr, var_storage[k], var_storage[k]);
    dip = vp[k]->initializer.dynamic;
    if (!do_constexpr_dynamic_init(ips, dip, &stmt->position, &var_addr)) {
      do_constexpr_fail(result);
      break;
    }  /* if */
  }  /* for */
  if (result) {
    an_expr_node_ptr  expr = loop_info->ne_call_expr,
                      incr = loop_info->incr_call_expr;
    a_byte            *expr_value, *incr_value;
    a_type_ptr        tp = skip_typerefs(expr->type),
                      incr_type = skip_typerefs(incr->type);
    a_byte_count      n_bytes;
    a_boolean         ovfl;
    a_host_large_integer
                      bool_val;
    /* Allocate storage for the loop-test result (a boolean) and the
       incrementation result. */
    n_bytes = expr_result_size(ips, expr, tp, &result);
    alloc_complete_object(ips, n_bytes, tp, expr_value);
    n_bytes = expr_result_size(ips, incr, incr_type, &result);
    alloc_complete_object(ips, n_bytes, incr_type, incr_value);
    if (vp[0]->init_kind != (an_init_kind)initk_dynamic ||
        vp[0]->initializer.dynamic == NULL) {
      /* This is possible in some error situations. */
      do_constexpr_fail(result);
      expect_error();
    } else {
      dip = vp[0]->initializer.dynamic;
    }  /* if */
    if (!result) goto unmap_storage;
    do {
      /* Evaluate the test expression. */
      if (cost_exceeded(ips)) {
        more_info_diagnostic(ec_excessive_constexpr_complexity, &ips->position,
                             &ips->diag_list);
        do_constexpr_fail(result);
      } else {
        do_constexpr_full_expression(
                                   ips, expr, expr_value, expr_value, result);
        release_address_structures(expr, tp, expr_value);
        ips->cost += 1;
      }  /* if */
      if (result) {
        /* Evaluation of the test expression succeeded.  Get its value
           to see if the dependent statement should be executed. */
        get_int_val_from(expr_value, tp, bool_val, ovfl);
        if (!ovfl && bool_val) {
          /* Initialize the iterator variable: */
          a_constexpr_address  var_addr;
          set_active_address(ips, &var_addr, var_storage[0], var_storage[0]);
          if (!do_constexpr_dynamic_init(ips, dip, &stmt->position,
                                         &var_addr)) {
            do_constexpr_fail(result);
            break;
          }  /* if */
          /* Execute the dependent statement. */
          result = do_constexpr_statement(
                           ips, stmt->variant.range_based_for_loop.statement);
          if (result) {
            /* Execution of the dependent statement succeeded, so evaluate
               the increment expression (if any) unless we hit a branching
               statement. */
            if (ips->curr_call_frame->return_active) {
              /* Break out of the loop (leave the flag active since we may
                 have to break out of other constructs). */
              break;
            } else if (ips->curr_call_frame->loop_break_active) {
              /* Break out of the loop (which completes the execution of
                 the break statement). */
              ips->curr_call_frame->loop_break_active = FALSE;
              break;
            } else if (ips->curr_call_frame->continue_active) {
              /* Continue, but clear the continue_active flag since we've
                 reached the point of continuation. */
              ips->curr_call_frame->continue_active = FALSE;
            }  /* if */
            do_constexpr_full_expression(
                                   ips, incr, incr_value, incr_value, result);
            release_address_structures(incr, incr_type, incr_value);
          }  /* if */
        }  /* if */
      }   /* if */
    } while (result && bool_val);
  }  /* if */
unmap_storage:
  /* Unmap the local storage. */
  for (k = 4; k--;) {
    do_constexpr_unmap_variable(ips, vp[k]);
  }  /* if */
done:
  restore_storage_stack(ips, saved_stack, result);
  return result;
}  /* do_constexpr_range_based_for_statement */


static a_boolean do_constexpr_switch(an_interpreter_state  *ips,
                                     a_statement_ptr       stmt)
/*
Interpret the given switch statement.  Return TRUE if the statement was
successfully interpreted, FALSE otherwise.
*/
{
  a_boolean                result = TRUE;
  an_expr_node_ptr         expr = stmt->expr;
  a_byte                   *expr_value, *case_value;
  a_storage_stack_state    saved_stack;
  a_type_ptr               tp;
  a_byte_count             n_bytes;
  a_statement_ptr          substmt;
  a_switch_case_entry_ptr  scep = stmt->variant.switch_stmt.extra_info
                                      ->sorted_cases;
  a_boolean                is_signed, has_cond_var;

  has_cond_var = node_is(expr, enk_condition);
  if (has_cond_var &&
      !do_constexpr_condition_alloc(ips, expr, &saved_stack)) {
    has_cond_var = FALSE;
    goto done_with_switch;
  }  /* if */
  tp = skip_typerefs(expr->type);
  n_bytes = value_bytes_for_type(ips, tp, &result);
  alloc_complete_object(ips, n_bytes, tp, expr_value);
  result = do_constexpr_condition(has_cond_var, ips, expr, tp,
                                  expr_value);
  if (!result) {
    goto done_with_switch;
  }  /* if */
  is_signed = int_type_is_signed(tp);
  /* Search through the ordered list of case labels for the one selected
     by the switch expression. */
  alloc_complete_object(ips, n_bytes, tp, case_value);
  for (; scep != NULL; scep = scep->next_on_sorted_list) {
    int             cmp;
    a_constant_ptr  case_con = scep->case_value;
    result = copy_val_from_constant(ips, case_con, case_value, case_value);
    if (!result) {
      goto done_with_switch;
    }  /* if */
    cmp = cmp_integer_values((an_integer_value*)expr_value, is_signed,
                             (an_integer_value*)case_value, is_signed);
    if (cmp == 0) {
      /* We found the case entry. */
      break;
    } else if (cmp < 0) {
      /* There may be more entries, but they won't match. */
      scep = NULL; /*lint !e850*/
      break;
#if GNU_EXTENSIONS_ALLOWED
    } else if (scep->range_end != NULL) {
      result = copy_val_from_constant(
                                ips, scep->range_end, case_value, case_value);
      if (!result) {
        goto done_with_switch;
      }  /* if */
      cmp = cmp_integer_values((an_integer_value*)expr_value, is_signed,
                               (an_integer_value*)case_value, is_signed);
      if (cmp <= 0) {
        /* We're in the range. */
        break;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* for */
  if (scep == NULL) {
    /* No case found: Use the default (if any). */
    scep = stmt->variant.switch_stmt.extra_info->default_case;
    if (scep == NULL) {
      /* No default. */
      goto done_with_switch;
    }  /* if */
  }  /* if */
  substmt = scep->stmt;
  /* Continue execution at the labeled statement.  Note that we don't
     have to activate intervening block scopes because they cannot
     contain variable declarations (errors are issued for case labels
     that enable branching past an initialized variable, and declaring
     uninitialized variables is not allowed in constexpr function
     definitions). */
  for (;;) {
    /* Move to the next statement.  The first time around, this means
       moving past the selected switch-case statement. */
    if (ips->curr_call_frame->switch_break_active) {
      /* Break out of the switch statements (which completes the
         execution of the break statement). */
      ips->curr_call_frame->switch_break_active = FALSE;
      break;
    } else if (ips->curr_call_frame->loop_break_active) {
      /* This is usually a break out of a loop enclosing the switch statement.
         However, if we switched into the body of an inner loop, it could also
         be a break out of that loop.  Traverse the parent statements to see
         which case it is. */
      for (;;) {
        substmt = substmt->parent;
        if (substmt == stmt) {
          /* We're breaking out of the switch statement itself. */
          goto done_with_switch;
        } else if (substmt->kind == (a_statement_kind)stmk_while ||
                   substmt->kind == (a_statement_kind)stmk_end_test_while ||
                   substmt->kind == (a_statement_kind)stmk_for) {
          /* We're breaking out of an inner loop.  This completes the
             execution of the break statement. */
          ips->curr_call_frame->loop_break_active = FALSE;
          break;
        }  /* if */
      }  /* for */
    } else if (ips->curr_call_frame->continue_active) {
      /* This is similar to the loop_break_active case, except the loop is
         continued. */
      for (;;) {
        substmt = substmt->parent;
        if (substmt == stmt) {
          /* We're continuing out of the switch statement itself. */
          goto done_with_switch;
        } else if (substmt->kind == (a_statement_kind)stmk_while ||
                   substmt->kind == (a_statement_kind)stmk_end_test_while ||
                   substmt->kind == (a_statement_kind)stmk_for) {
          /* We're continuing an inner loop.  This completes the
             execution of the continue statement. */
          ips->curr_call_frame->continue_active = FALSE;
          if (substmt->kind == (a_statement_kind)stmk_for) {
            result = do_constexpr_for_statement(ips, substmt,
                                                /*do_continue=*/TRUE);
          } else {
            result = !do_constexpr_statement(ips, substmt);
          }  /* if */
          if (!result) {
            goto done_with_switch;
          }  /* if */
          break;
        }  /* if */
      }  /* for */
    } else if (ips->curr_call_frame->return_active) {
      /* Return statements end the execution of the switch, but
         they are not completed by the switch. */
      break;
    }  /* if */
    if (substmt->next != NULL) {
      /* The statement just interpreted is followed by another one.
         We'll interpret it next. */
      substmt = substmt->next;
    } else {
      /* There are no more statements in this sequence.  Move up to the
         parent sequence if appropriate. */
      for (;;) {
        a_statement_ptr  next_stmt;
        substmt = substmt->parent;
        if (substmt == stmt) {
          /* We're flowing off the switch statement itself. */
          goto done_with_switch;
        } else if (substmt->kind == (a_statement_kind)stmk_while) {
          /* We jumped into a simple while loop.  Continue the loop. */
          break;
        } else if (substmt->kind == (a_statement_kind)stmk_end_test_while) {
          /* We jumped into a do...while loop.  Evaluate the loop condition to
             decide whether the loop should be continued.  Since the condition
             is an integral value (bool), we can reuse the case_value
             storage. */
          a_host_large_integer  bool_val;
          a_boolean             ovfl;
          expr = substmt->expr;
          tp = skip_typerefs(expr->type);
          do_constexpr_full_expression(
                                   ips, expr, case_value, case_value, result);
          release_address_structures(expr, tp, case_value);
          if (!result) {
            goto done_with_switch;
          }  /* if */
          get_int_val_from(case_value, tp, bool_val, ovfl);
          if (ovfl || bool_val) {
            /* Continue the loop. */
            break;
          } else {
            /* Keep looking for the next statement to execute. */
          }  /* if */
        } else if (substmt->kind == (a_statement_kind)stmk_for) {
          /* We jumped into a "for" loop.  Continue the loop, but skip
             its initialization. */
          if (!do_constexpr_for_statement(ips, substmt,
                                          /*do_continue=*/TRUE)) {
            result = FALSE;
            goto done_with_switch;
          }  /* if */
        }  /* if */
        next_stmt = substmt->next;
        /* Skip over label statements because some compiler-generated labels
           do not have the right parent statement for our purposes. */
        while (next_stmt != NULL &&
               next_stmt->kind == (a_statement_kind)stmk_label) {
          next_stmt = next_stmt->next;
        }  /* if */
        if (next_stmt != NULL) {
          substmt = next_stmt;
          break;
        }  /* if */
        /* Continue up the parent chain. */
      }  /* for */
    }  /* if */
    if (!do_constexpr_statement(ips, substmt)) {
      result = FALSE;
      break;
    }  /* if */
  }  /* for */
done_with_switch:
  if (has_cond_var) {
    do_constexpr_condition_cleanup(ips, expr);
    do_constexpr_condition_dealloc(ips, expr, &saved_stack, &result);
  }  /* if */
  return result;
}  /* do_constexpr_switch */


static a_boolean do_constexpr_statement(an_interpreter_state  *ips,
                                        a_statement_ptr       stmt)
/*
Interpret the given statement.  Return TRUE if the statement was
successfully interpreted, FALSE otherwise.
*/
{
  a_boolean             result = TRUE;
  an_expr_node_ptr      expr;
  a_byte                *expr_value;
  a_storage_stack_state saved_stack;
  a_type_ptr            tp;
  a_boolean             ovfl;
  a_byte_count          n_bytes;

  switch (stmt->kind) {
    case stmk_expr:
      {
        expr = stmt->expr;
        tp = skip_typerefs(expr->type);
        n_bytes = expr_result_size(ips, expr, tp, &result);
        save_storage_stack(ips, saved_stack);
        alloc_complete_object(ips, n_bytes, tp, expr_value);
        if (!result) {
          /* Stop interpretation. */
        } else if (!do_constexpr_expression(
                                         ips, expr, expr_value, expr_value)) {
          do_constexpr_fail(result);
        } else {
          release_address_structures(expr, tp, expr_value);
        }  /* if */
        restore_storage_stack(ips, saved_stack, result);
      }
      break;
    case stmk_if:
    case stmk_constexpr_if:
    case stmk_if_consteval:
    case stmk_if_not_consteval:
      {
        a_boolean             has_cond_var,
                              saved_allow_consteval_routine_node = FALSE;
        a_host_large_integer  bool_val;
        a_statement_ptr       then_statement, else_statement;
        if (stmt->kind == (a_statement_kind)stmk_constexpr_if) {
          then_statement = stmt->variant.constexpr_if->then_statement;
          else_statement = stmt->variant.constexpr_if->else_statement;
        } else {
          then_statement = stmt->variant.if_stmt.then_statement;
          else_statement = stmt->variant.if_stmt.else_statement;
        }  /* if */
        expr = stmt->expr;
        if (expr == NULL) {
          /* "if consteval ..." or "if not consteval". */
          if (!ips->is_constant_evaluated) {
            do_constexpr_fail(result);
          } else {
            if (stmt->kind == (a_statement_kind)stmk_if_consteval) {
              bool_val = 1;
              saved_allow_consteval_routine_node =
                                            ips->allow_consteval_routine_node;
              ips->allow_consteval_routine_node = TRUE;
            } else {
              bool_val = 0;
            }  /* if */
          }  /* if */
          has_cond_var = FALSE;
        } else {
          /* Check if we have to allocate a condition variable. */
          has_cond_var = (expr->kind == (an_expr_node_kind)enk_condition);
          if (has_cond_var &&
              !do_constexpr_condition_alloc(ips, expr, &saved_stack)) {
            do_constexpr_fail(result);
            break;
          }  /* if */
          /* The type of the test expression is known to be bool. */
          tp = skip_typerefs(expr->type);
          n_bytes = value_bytes_for_type(ips, tp, &result);
          alloc_complete_object(ips, n_bytes, tp, expr_value);
          if (do_constexpr_condition(has_cond_var, ips, expr, tp,
                                     expr_value)) {
            /* Evaluation of the test expression succeeded.  Get its value to
               see which dependent statement should be executed. */
            get_int_val_from(expr_value, tp, bool_val, ovfl);
            if (ovfl) bool_val = TRUE;
          } else {
            result = FALSE;
          }  /* if */
        }  /* if */
        if (result) {
          if (bool_val) {
            /* Execute the "then" statement. */
            result = do_constexpr_statement(ips, then_statement);
          } else if (else_statement != NULL) {
            result = do_constexpr_statement(ips, else_statement);
          }  /* if */
        }  /* if */
        if (has_cond_var) {
          do_constexpr_condition_cleanup(ips, expr);
          do_constexpr_condition_dealloc(ips, expr, &saved_stack, &result);
        } else if (expr == NULL) {
          ips->allow_consteval_routine_node =
                                           saved_allow_consteval_routine_node;
        }  /* if */
      }
      break;
    case stmk_while:
      {
        a_boolean             has_cond_var;
        a_host_large_integer  bool_val = FALSE;
        expr = stmt->expr;
        /* Check if we have to allocate a condition variable. */
        has_cond_var = node_is(expr, enk_condition);
        if (has_cond_var &&
            !do_constexpr_condition_alloc(ips, expr, &saved_stack)) {
          do_constexpr_fail(result);
          break;
        }  /* if */
        /* The type of the test expression is known to be bool. */
        tp = skip_typerefs(expr->type);
        n_bytes = value_bytes_for_type(ips, tp, &result);
        alloc_complete_object(ips, n_bytes, tp, expr_value);
        do {
          /* Evaluate the test expression. */
          if (cost_exceeded(ips)) {
            more_info_diagnostic(ec_excessive_constexpr_complexity,
                                 &ips->position, &ips->diag_list);
            do_constexpr_fail(result);
          } else {
            result = do_constexpr_condition(has_cond_var, ips, expr, tp,
                                            expr_value);
            ips->cost += 1;
          }  /* if */
          if (result) {
            /* Evaluation of the test expression succeeded.  Get its value
               to see if the dependent statement should be executed. */
            get_int_val_from(expr_value, tp, bool_val, ovfl);
            if (!ovfl && bool_val) {
              /* Execute the dependent statement. */
              result = do_constexpr_statement(ips,
                                              stmt->variant.loop_statement);
              if (result) {
                /* Execution of the dependent statement succeeded.  Check for
                   a pending branching statement. */
                if (ips->curr_call_frame->return_active) {
                  /* Stop the loop (leave the flag active since we may have to
                     break out of other constructs). */
                  bool_val = FALSE;
                } else if (ips->curr_call_frame->loop_break_active) {
                  /* Stop the loop (which completes the execution of the break
                     statement). */
                  ips->curr_call_frame->loop_break_active = FALSE;
                  bool_val = FALSE;
                } else if (ips->curr_call_frame->continue_active) {
                  /* Continue, but clear the continue_active flag since we've
                     reached the point of continuation. */
                  ips->curr_call_frame->continue_active = FALSE;
                }  /* if */
              }  /* if */
            }  /* if */
          }   /* if */
          if (has_cond_var) {
            do_constexpr_condition_cleanup(ips, expr);
          }  /* if */
        } while (result && bool_val);
        if (has_cond_var) {
          do_constexpr_condition_dealloc(ips, expr, &saved_stack, &result);
        }  /* if */
      }
      break;
    case stmk_goto:
      /* An actual "goto" statement is not valid.  However, stmk_goto
         statements generated for other branch statements ("break" and
         "continue") are okay.  For those, we activate a flag in the
         interpreter state depending on the kind of label we are branching to.
         These flags are consulted to determine the control flow during loop
         and switch statements. */
      if (stmt->variant.label.ptr->switch_break_label) {
        ips->curr_call_frame->switch_break_active = TRUE;
      } else if (stmt->variant.label.ptr->break_label) {
        ips->curr_call_frame->loop_break_active = TRUE;
      } else if (stmt->variant.label.ptr->continue_label) {
        ips->curr_call_frame->continue_active = TRUE;
      } else {
        info_with_pos(ec_constexpr_goto, &stmt->position, ips);
        do_constexpr_fail(result);
      }  /* if */
      break;
    case stmk_label:
      /* Nothing to do. */
      break;
    case stmk_return:
      { a_call_frame_ptr  frame = ips->curr_call_frame;
        a_byte            *result_storage, *complete_obj;
        /* Skip GNU statement expression frames. */
        while (frame->routine == NULL) {
          frame = frame->parent;
          if (frame == NULL) {
            info_with_pos(ec_branch_out_of_constant, &stmt->position, ips);
            do_constexpr_fail(result);
            goto done_with_return_statement;
          }  /* if */
        }  /* while */
        result_storage = frame->result_storage;
        complete_obj = frame->complete_object;
        if (stmt->expr != NULL) {
          do_constexpr_full_expression(ips, stmt->expr, result_storage,
                                       complete_obj, result);
        } else if (stmt->variant.return_dynamic_init != NULL) {
          /* Handle return_dynamic_init case. */
          a_dynamic_init_ptr  dip = stmt->variant.return_dynamic_init;
          if (dyn_init_is(dip, dik_zero)) {
            a_type_ptr  fn_type = frame->routine->type;
            fn_type = skip_typerefs(fn_type);
            tp = skip_typerefs(fn_type->variant.routine.return_type);
            init_subobject_to_zero(ips, result_storage, tp, complete_obj);
          } else {
            a_constexpr_address  dst_addr;
            set_active_address(ips, &dst_addr, result_storage, complete_obj);
            result = do_constexpr_dynamic_init(ips, dip, &stmt->position, 
                                               &dst_addr);
          }  /* if */
        } else {
          /* Return without a value. */
          a_type_ptr  fn_type = frame->routine->type;
          fn_type = skip_typerefs(fn_type);
          if (!is_void_type(fn_type->variant.routine.return_type)) {
            a_source_position  *pos = &stmt->position;
            if (pos->seq == 0) {
              a_statement_ptr  parent_stmt = stmt->parent;
              for (;;) {
                if (parent_stmt->kind == (a_statement_kind)stmk_block) {
                  if (parent_stmt->variant.block.extra_info
                                 ->final_position.seq != 0) {
                    pos = &parent_stmt->variant.block.extra_info
                                      ->final_position;
                    break;
                  }  /* if */
                }  /* if */
                parent_stmt = parent_stmt->parent;
                check_assertion(parent_stmt != NULL);
              }  /* if */
            }  /* if */
            info_with_pos(ec_constexpr_missing_return_value, pos, ips);
            do_constexpr_fail(result);
          }  /* if */
        }  /* if */
        frame->return_active = TRUE;
      }
done_with_return_statement:
      break;
#if GNU_EXTENSIONS_ALLOWED
    case stmk_stmt_expr_result:
      { a_call_frame_ptr  frame = ips->curr_call_frame;
        a_byte            *result_storage = frame->result_storage;
        a_byte            *complete_obj = frame->complete_object;
        if (stmt->expr != NULL) {
          do_constexpr_full_expression(ips, stmt->expr, result_storage,
                                       complete_obj, result);
        } else if (stmt->variant.stmt_expr_result.dynamic_init != NULL) {
          /* Handle return_dynamic_init case. */
          a_dynamic_init_ptr  dip;
          dip = stmt->variant.stmt_expr_result.dynamic_init;
          if (dyn_init_is(dip, dik_zero)) {
            tp = skip_typerefs(frame->variant.expr->type);
            init_subobject_to_zero(ips, result_storage, tp, complete_obj);
          } else {
            a_constexpr_address  dst_addr;
            set_active_address(ips, &dst_addr, result_storage, complete_obj);
            result = do_constexpr_dynamic_init(ips, dip, &stmt->position, 
                                               &dst_addr);
          }  /* if */
        }  /* if */
      }
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    case stmk_block:
      { a_block_ptr  block = stmt->variant.block.extra_info;
        result = do_constexpr_block_statement(ips, stmt, block->assoc_scope);
      }
      break;
    case stmk_try_block:
      { a_block_ptr  block;
        stmt = stmt->variant.try_block->statement;
        block = stmt->variant.block.extra_info;
        result = do_constexpr_block_statement(ips, stmt, block->assoc_scope);
      }
      break;
    case stmk_end_test_while:
      {
        a_host_large_integer  bool_val;
        expr = stmt->expr;
        /* The type of the test expression is known to be bool. */
        tp = skip_typerefs(expr->type);
        n_bytes = value_bytes_for_type(ips, tp, &result);
        alloc_complete_object(ips, n_bytes, tp, expr_value);
        do {
          /* Execute the dependent statement. */
          result = do_constexpr_statement(ips, stmt->variant.loop_statement);
          if (!result) break;
          /* Execution of the dependent statement succeeded.  Check for a
             pending branching statement. */
          if (ips->curr_call_frame->return_active) {
            /* Break out of the loop (leave the flag active since we may
               have to break out of other constructs). */
            break;
          } else if (ips->curr_call_frame->loop_break_active) {
            /* Break out of the loop (which completes the execution of the
               break statement). */
            ips->curr_call_frame->loop_break_active = FALSE;
            break;
          } else if (ips->curr_call_frame->continue_active) {
            /* Continue, but clear the continue_active flag since we've
               reached the point of continuation. */
            ips->curr_call_frame->continue_active = FALSE;
          }  /* if */
          /* Evaluate the test expression. */
          if (cost_exceeded(ips)) {
            more_info_diagnostic(ec_excessive_constexpr_complexity,
                                 &ips->position, &ips->diag_list);
            do_constexpr_fail(result);
          } else {
            do_constexpr_full_expression(
                                   ips, expr, expr_value, expr_value, result);
            release_address_structures(expr, tp, expr_value);
            ips->cost += 1;
          }  /* if */
          if (result) {
            /* Evaluation of the test expression succeeded.  Get its value
               to see if the dependent statement should be repeated. */
            get_int_val_from(expr_value, tp, bool_val, ovfl);
          }   /* if */
        } while (result && bool_val);
      }
      break;
    case stmk_for:
      result = do_constexpr_for_statement(ips, stmt, /*do_continue=*/FALSE);
      break;
    case stmk_range_based_for:
      result = do_constexpr_range_based_for_statement(ips, stmt);
      break;
    case stmk_switch_case:
      /* Nothing to do. */
      break;
    case stmk_switch:
      result = do_constexpr_switch(ips, stmt);
      break;
    case stmk_init:
      { a_dynamic_init_ptr  dip = stmt->variant.dynamic_init;
        a_variable_ptr      vp = dip->variable;
        a_byte              *var_storage;
        /* Allocate and bind storage for the variable. */
        var_storage = do_constexpr_alloc_variable(ips, vp, &result);
        if (!result) break;
        /* Evaluate the initializer. */
        result = do_constexpr_init_variable(
                                       ips, vp, var_storage, &stmt->position);
      }
      break;
    case stmk_decl:
      /* Nothing to do; variables are allocated when the scope is opened and
         initialized through stmk_init statements.  However, static-storage
         variables should not be permitted. */
      if (stmt->variant.decl.has_static_or_thread_variable) {
        info_with_pos(ec_constexpr_local_static, &stmt->position, ips);
        do_constexpr_fail(result);
      }  /* if */
      break;
    case stmk_empty:
      /* Nothing to do. */
      break;
    case stmk_set_vla_size:
    case stmk_vla_decl:
      info_with_pos(ec_constexpr_vla, &stmt->position, ips);
      do_constexpr_fail(result);
      break;
    default:
      info_with_pos(ec_constexpr_statement_cannot_be_interpreted,
                    &stmt->position, ips);
      do_constexpr_fail(result);
  }  /* switch */
  return result;
}  /* do_constexpr_statement */


static void warn_about_is_constant_evaluated(a_routine_ptr         callee,
                                             an_expr_node_ptr      call_node)
/*
call_node describes a call to callee, which is std::is_constant_evaluated() or
__builtin_is_constant_evaluated(), in a context where it would always produce
a true value.  Issue a warning if justified.
*/
{
  if ((innermost_function_scope == NULL ||
       !current_routine_entry()->is_consteval) &&
      !scope_stack_top().is_rescan) {
    pos_st_warning(ec_is_constant_evaluated_in_constant_expression,
                   &call_node->position,
                   unmangled_name_of(&callee->source_corresp));

  }  /* if */
}  /* warn_about_is_constant_evaluated */


static a_boolean translate_interpreter_object_to_target_bytes(
                                    an_interpreter_state  *ips,
                                    a_type_ptr            type,
                                    a_byte                *src_storage,
                                    a_byte                *src_complete_object,
                                    a_byte                *dest_storage,
                                    a_byte                *dest_bitmap,
                                    an_expr_node_ptr      expr)
/*
Translate the interpreter object at src_storage, whose type is "type", into
the target layout, starting at dest_storage.  Set the appropriate bits in
dest_bitmap to indicate which bits in dest_storage have been written to
(and are therefore considered to be initialized).  src_storage is encompassed
by src_complete_object.  Returns TRUE if there are no errors; otherwise emits a
diagnostic associated with expr->position.  Used in the implementation of
__builtin_bit_cast.
*/
{
  a_boolean result = TRUE;

  if (is_volatile_qualified_type(type)) {
    info_with_pos_type(ec_volatile_type_not_allowed, &expr->position, type,
                       ips);
    do_constexpr_fail(result);
  } else if (targ_char_bit != CHAR_BIT &&
             (targ_char_bit > CHAR_BIT) ? (targ_char_bit % CHAR_BIT) != 0
                                        : (CHAR_BIT % targ_char_bit) != 0) {
    info_with_pos(ec_cannot_interpret_target_bits, &expr->position, ips);
    do_constexpr_fail(result);
  } else {
    a_type_ptr  tp = skip_typerefs(type);
    a_byte      all_bits_on = ~(a_byte)0;
    switch (tp->kind) {
      case tk_error:
        ips->input_error = TRUE;
        FALLTHROUGH
      case tk_pointer:
      case tk_nullptr:
      case tk_union:
      case tk_ptr_to_member:
      case tk_routine:
        /* These are explicitly forbidden for a constexpr bit_cast. */
        info_with_pos_type(ec_invalid_bit_cast_type, &expr->position, tp, ips);
        do_constexpr_fail(result);
        break;
      case tk_integer:
        { an_integer_value     int_val, byte_val;
          a_boolean            ovfl;
          a_host_large_integer byte;
          int                  bit_shift;
          /* Store the resulting value in target layout, marking each
             byte in the result as initialized. */
          (void)memcpy((a_byte*)&int_val, src_storage, sizeof(int_val));
          for (unsigned int i = 0; i < tp->size; i++) {
            bit_shift = (int)(host_little_endian ? i : ((tp->size - 1) - i));
            bit_shift *= CHAR_BIT;
            /* The following code effectively does:
                   byte = (int_val & (0xff << bit_shift)) >> bit_shift;
               (assuming all_bits_on == 0xff). */
            set_unsigned_integer_value(&byte_val,
                                       (a_host_large_integer)all_bits_on);
            shift_left_integer_value(&byte_val, bit_shift, &ovfl);
            check_assertion(!ovfl);
            and_integer_values(&byte_val, &int_val);
            shift_right_integer_value(&byte_val, bit_shift,
                                      /*is_signed=*/FALSE,
                                      /*sign_extend=*/FALSE);
            conv_integer_value_to_host_large_integer(&byte_val,
                                                     /*is_signed=*/FALSE,
                                                     &byte, &ovfl);
            check_assertion(!ovfl);
            *dest_storage++ = (a_byte)byte;
            *dest_bitmap++ = all_bits_on;
          }  /* for */
        }
        break;
      case tk_float:
        /* Floating-point values are stored internally in a buffer large enough
           to hold the largest floating-point type for the target, but only
           the required bytes for the specific floating-point type are used.
           So, e.g., a float will only use four bytes (and the value is already
           in target layout). */
        for (unsigned int i = 0; i < tp->size; i++) {
          *dest_storage++ = *src_storage++;
          *dest_bitmap++ = all_bits_on;
        }  /* for */
        break;
      case tk_array:
        /* An array; step through each underlying element. */
        { a_targ_size_t  n_elems;
          a_type_ptr     etp;
          an_error_code  err_code = ec_no_error;
          err_code = get_element_and_type_from_array(tp, &etp, &n_elems);
          if (err_code == ec_no_error) {
            a_byte_count etp_n_bytes = value_bytes_for_type(ips, etp, &result);
            if (result) {
              check_assertion(n_elems < MAX_ARRAY_LENGTH);
              for (; n_elems > 0; n_elems--) {
                if (!translate_interpreter_object_to_target_bytes(
                                           ips, etp,
                                           src_storage, src_complete_object,
                                           dest_storage, dest_bitmap, expr)) {
                  do_constexpr_fail(result);
                  break;
                }  /* if */
                /* Move to the next element (both source and destination). */
                src_storage += etp_n_bytes;
                dest_storage += etp->size;
                dest_bitmap += etp->size;
              }  /* for */
            }  /* if */
          } else {
            info_with_pos(err_code, &expr->position, ips);
            do_constexpr_fail(result);
          }  /* if */
        }
        break;
      case tk_class:
      case tk_struct:
        /* Visit each subobject of the class separately. */
        { a_byte_count offset;
          /* This is called to ensure that the class has been laid out. */
          (void)f_value_bytes_for_type(ips, tp, &result);
          /* Visit subobjects.  Note that the order in which the subobjects
             are visited is immaterial. */
          if (result) {
            a_field_ptr fp = tp->variant.class_struct_union.field_list;
            for (fp = next_alloc_field(fp);
                 fp != NULL;
                 fp = next_alloc_field(fp->next)) {
              if (fp->compiler_generated && !fp->is_anonymous_parent_object) {
                /* Ignore fields generated by prelowering. */
                continue;
              }  /* if */
              if (fp->is_bit_field) {
                /* For now, don't allow bitfields. */
                info_with_pos_type(ec_bitfields_not_allowed, type_pos(tp, ips),
                                   tp, ips);
                do_constexpr_fail(result);
                break;
              }  /* if */
              if (is_reference_type(fp->type)) {
                /* Non-static data members with reference type are not
                   allowed. */
                info_with_pos_type(ec_reference_type_not_allowed,
                                   type_pos(tp, ips), tp, ips);
                do_constexpr_fail(result);
                break;
              }  /* if */
              get_mapped_byte_count(&persistent_map, fp, offset);
              if (!translate_interpreter_object_to_target_bytes(
                          ips, skip_typerefs(fp->type),
                          src_storage + offset, src_complete_object,
                          dest_storage + fp->offset, dest_bitmap + fp->offset,
                          expr)) {
                do_constexpr_fail(result);
                break;
              }  /* if */
            }  /* for */
          }  /* if */
          if (result) {
            a_base_class_ptr  bcp;
            /* Visit base classes (direct and virtual). */
            for (bcp = base_classes_of(tp); bcp != NULL; bcp = bcp->next) {
              get_mapped_byte_count(&persistent_map, bcp, offset);
              if (!translate_interpreter_object_to_target_bytes(
                     ips, skip_typerefs(bcp->type),
                     src_storage + offset, src_complete_object,
                     dest_storage + bcp->offset, dest_bitmap + bcp->offset,
                     expr)) {
                do_constexpr_fail(result);
                break;
              }  /* if */
            }  /* for */
          }  /* if */
        }
        break;
#if GNU_VECTOR_TYPES_ALLOWED
      case tk_vector:
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
      case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if C99_IL_EXTENSIONS_SUPPORTED
      case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        /* These should eventually be supported but aren't yet. */
        info_with_pos_type(ec_unsupported_type_for_bit_cast, &expr->position,
                           tp, ips);
        do_constexpr_fail(result);
        break;
#if FIXED_POINT_ALLOWED
      case tk_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
      case tk_void:
      case tk_typeref:
      case tk_template_param:
      case tk_unknown:
      default:
        /* These types should not be encountered here. */
        do_constexpr_fail(result);
        unexpected_condition();
    }  /* switch */
  }  /* if */
  return result;
}  /* translate_interpreter_object_to_target_bytes */

#if BUILTIN_FUNCTIONS_ENABLED
#if C99_IL_EXTENSIONS_SUPPORTED && TARG_HAS_IEEE_FLOATING_POINT

static a_boolean do_constexpr_builtin_fptest(
                                      a_routine_ptr            callee,
                                      a_float_kind             fpkind,
                                      an_internal_float_value  *fpval,
                                      a_byte                   *result_storage)
/*
callee is a GNU floating-point test function to which the floating-point value
fpval of the given kind is passed.  Place in *result_storage the result of the
test function (a boolean integer value) and return TRUE, or, if the test
cannot be evaluated, return FALSE.
*/
{
  a_boolean  val = FALSE, result = TRUE;

  switch (callee->variant.builtin_function_kind) {
    case bfk_isnan:
    case bfk_isnanf:
    case bfk_isnanl:
      val = fp_is_nan(fpval, fpkind);
      break;
    case bfk_isinf:
    case bfk_isinff:
    case bfk_isinfl:
      val = fp_is_infinity(fpval, fpkind);
      break;
    case bfk_isfinite:
      val = !fp_is_infinity(fpval, fpkind) && !fp_is_nan(fpval, fpkind);
      break;
    case bfk_isnormal:
      { a_boolean  unknown_result;
        val = fp_is_normalized(fpval, fpkind, &unknown_result);
        if (unknown_result) {
          do_constexpr_fail(result);
        }  /* if */
      }
      break;
    case bfk_signbit:
    case bfk_signbitf:
    case bfk_signbitl:
      if (fp_is_nan(fpval, fpkind)) {
        /* We don't currently attempt to determine the sign bit of a NaN
           value.  (This matches Clang but not GCC.) */
        do_constexpr_fail(result);
      } else {
        val = fp_signbit(fpkind, fpval);
      }  /* if */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (result) {
    *(an_integer_value *)result_storage = val ? one_int : zero_int;
  }  /* if */
  return result;
}  /* do_constexpr_builtin_fptest */

#endif /* C99_IL_EXTENSIONS_SUPPORTED && TARG_HAS_IEEE_FLOATING_POINT */

static a_boolean do_constexpr_builtin_bitcount(a_routine_ptr  callee,
                                               a_byte         *arg_bytes,
                                               a_type_ptr     arg_tp,
                                               a_byte         *result_storage)
/*
Evaluate the builtin bit counting function indicated by callee on the operand
of type arg_tp stored in arg_bytes.  Place the result in *result_storage.
This function currently always returns TRUE.
*/
{
  a_builtin_function_kind  bfk;
  a_targ_size_t            k, n_bits, count = 0;
  an_integer_value         arg;

  bfk = (a_builtin_function_kind)callee->variant.builtin_function_kind;
  arg = *(an_integer_value*)arg_bytes;
  check_assertion(arg_tp->kind == (a_type_kind)tk_integer);
  n_bits = arg_tp->size*targ_char_bit;
  for (k = 0; k < n_bits; ++k) {
    a_boolean         bit, ovflo;
    an_integer_value  mask = one_int;
    shift_left_integer_value(&mask, (int)k, &ovflo);
    check_assertion(!ovflo);
    and_integer_values(&mask, &arg);
    bit = cmp_integer_values(&mask, /*op_1_signed=*/FALSE,
                             &zero_int, /*op_2_signed=*/FALSE) != 0;
    switch (bfk) {
      case bfk_ffs:
      case bfk_ffsl:
#if LONG_LONG_ALLOWED
      case bfk_ffsll:
#endif /* LONG_LONG_ALLOWED */
        /* Index of the least significant 1-bit. */
        if (bit) {
          count = k+1;
          goto count_done;
        }  /* if */
        break;
      case bfk_clz:
      case bfk_clzl:
#if LONG_LONG_ALLOWED
      case bfk_clzll:
#endif /* LONG_LONG_ALLOWED */
        /* Count of leading zeros. */
        count = bit ? 0 : count+1;
        break;
      case bfk_ctz:
      case bfk_ctzl:
#if LONG_LONG_ALLOWED
      case bfk_ctzll:
#endif /* LONG_LONG_ALLOWED */
        /* Count of trailing zeros. */
        if (bit) {
          goto count_done;
        } else {
          ++count;
        }  /* if */
        break;
      case bfk_popcount:
      case bfk_popcountl:
#if LONG_LONG_ALLOWED
      case bfk_popcountll:
#endif /* LONG_LONG_ALLOWED */
        /* Count of ones. */
        if (bit) count += 1;
        break;
      case bfk_parity:
      case bfk_parityl:
#if LONG_LONG_ALLOWED
      case bfk_parityll:
#endif /* LONG_LONG_ALLOWED */
        /* Count of ones. */
        if (bit) count = (count+1) & 1;
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* for */
count_done:
  set_integer_value((an_integer_value*)result_storage,
                    (a_host_large_integer)count);
  return TRUE;
}  /* do_constexpr_builtin_bitcount */


static a_boolean do_constexpr_builtin_strlen(
                                        an_interpreter_state  *ips,
                                        a_byte                *arg_bytes,
                                        a_type_ptr            arg_tp,
                                        an_expr_node_ptr      call_node,
                                        a_byte                *result_storage)
/*
Evaluate the standard strlen/wcslen function on the operand of type arg_tp
stored in arg_bytes.  Place the result in *result_storage.  Return FALSE if
this fails (because the entity pointed to is not a null-terminated string) and
record a potential diagnostic for the given expression node and interpreter
state.
*/
{
  a_boolean  result = TRUE;

  if (arg_tp->kind == (a_type_kind)tk_pointer) {
    a_constexpr_address  *addr = (a_constexpr_address*)arg_bytes;
    a_type_ptr           tp = skip_typerefs(arg_tp->variant.pointer.type);
    if (addr->address == NULL) {
      do_constexpr_fail(result);
      info_with_pos((is_runtime_data_address(addr) &&
                     constant_is(addr->variant.addr_con, ck_integer)) ?
                                        ec_constexpr_null_dereference :
                                        ec_constexpr_access_to_runtime_storage,
                    &call_node->variant.operation.operands->next->position,
                    ips);
    } else if (is_array_element(addr) && tp->kind == (a_type_kind)tk_integer) {
      an_integer_value  *ptr = (an_integer_value*)addr->address;
      a_byte_count  elem_size, pos, max_len, len = 0;
      get_array_pos(ips, addr, tp, &max_len, &pos, &elem_size, &result);
      if (result) {
        max_len -= pos;
        while (cmp_integer_values(ptr, /*op_1_signed=*/FALSE,
                                  (an_integer_value *)&zero_int,
                                  /*op_2_signed=*/FALSE) != 0) {
          len += 1;
          ptr += 1;
          if (len == max_len) {
            an_expr_node_ptr  arg = call_node->variant.operation.operands;
            arg = arg->next;
            do_constexpr_fail(result);
            info_with_pos(ec_constexpr_string_not_null_terminated,
                          &arg->position, ips);
            break;
          }  /* if */
        }  /* while */
        if (result) {
          set_integer_value((an_integer_value*)result_storage,
                            (a_host_large_integer)len);
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  return result;
}  /* do_constexpr_builtin_strlen */


static a_boolean do_constexpr_builtin_strchr(
                                      an_interpreter_state    *ips,
                                      a_byte                  *arg1_bytes,
                                      a_type_ptr              arg1_tp,
                                      a_byte                  *arg2_bytes,
                                      a_type_ptr              arg2_tp,
                                      a_byte                  *arg3_bytes,
                                      a_type_ptr              arg3_tp,
                                      an_expr_node_ptr        call_node,
                                      a_byte                  *result_storage)
/*
Evaluate the strchr/memchr/wcschr/wmemchr family of builtin functions.
argX_bytes and argX_tp specify the operands and their types (arg3_bytes and
arg3_tp are NULL in cases where a count argument is not specified).  Place the
result in *result_storage.  Return FALSE if this fails (e.g., because the
entity pointed to is not a null-terminated string) and record a potential
diagnostic for the given expression node and interpreter state.
*/
{
  a_boolean  result = TRUE;

  if (arg1_tp->kind == (a_type_kind)tk_pointer) {
    a_constexpr_address  *addr = (a_constexpr_address*)arg1_bytes;
    a_type_ptr           tp = skip_typerefs(arg1_tp->variant.pointer.type);
    if (addr->address == NULL) {
      do_constexpr_fail(result);
      info_with_pos((is_runtime_data_address(addr) &&
                     constant_is(addr->variant.addr_con, ck_integer)) ?
                                        ec_constexpr_null_dereference :
                                        ec_constexpr_access_to_runtime_storage,
                    &call_node->variant.operation.operands->next->position,
                    ips);
    } else if (is_array_element(addr)) {
      an_integer_value  *ptr = (an_integer_value*)addr->address;
      an_integer_value  len, max_len, *eff_max;
      a_byte_count  elem_size, pos, max;
      if (tp == void_type()) {
        /* In the memchr case, the argument is "void *"; treat as "char *". */
        tp = integer_type(plain_char_int_kind);
      } else {
        check_assertion(tp->kind == (a_type_kind)tk_integer);
      }  /* if */
      get_array_pos(ips, addr, tp, &max, &pos, &elem_size, &result);
      if (result) {
        a_boolean     check_for_read_past_operand = FALSE;
        an_error_code error_code = ec_no_error;
        check_assertion(f_skip_typerefs(arg2_tp)->kind ==
                                                      (a_type_kind)tk_integer);
        max -= pos;
        set_integer_value(&max_len, (a_host_large_integer)max);
        set_integer_value(&len, (a_host_large_integer)0);
        /* Use the size of the array as a count (but see below). */
        eff_max = &max_len;
        if (arg3_bytes != NULL) {
          check_assertion(arg3_tp != NULL &&
                          f_skip_typerefs(arg3_tp)->kind ==
                                                      (a_type_kind)tk_integer);
          if (cmp_integer_values(&max_len, /*op_1_signed=*/FALSE,
                                 (an_integer_value *)arg3_bytes,
                                 /*op_2_signed=*/FALSE) >= 0) {
            /* The user-specified count is less than the array count so use
               that. */
            eff_max = (an_integer_value *)arg3_bytes;
          } else {
            /* The user-specified count is more than the array count, so if
               the item isn't found, we've gone past the array. */
            check_for_read_past_operand = TRUE;
          }  /* if */
        }  /* if */
        for (;cmp_integer_values(&len, /*op_1_signed=*/FALSE, eff_max,
                                 /*op_2_signed=*/FALSE);
             ptr++, incr_integer_value(&len)) {
          if (cmp_integer_values(ptr, /*op_1_signed=*/FALSE,
                                 (an_integer_value *)arg2_bytes,
                                 /*op_2_signed=*/FALSE) == 0) {
            /* The item has been found. */
            goto return_result;
          }  /* if */
          if (arg3_bytes == NULL) {
            if (cmp_integer_values(ptr, /*op_1_signed=*/FALSE,
                                   (an_integer_value *)&zero_int,
                                   /*op_2_signed=*/FALSE) == 0) {
              /* In the string case, we've found the end of the string, so the
                 item was not found (but this does not result in an error). */
              ptr = NULL;
              goto return_result;
            }  /* if */
          }  /* if */
        }  /* for */
        /* The entire object has been exhausted without finding a match.  In
           the string case, this is an error (as an upper bound on the string
           size was reached), in the memchr/wmemchr cases, this simply
           returns NULL, unless we ran off the end of the object. */
        ptr = NULL;
        if (arg3_bytes == NULL) {
          error_code = ec_constexpr_string_not_null_terminated;
        } else if (check_for_read_past_operand) {
          error_code = ec_attempt_to_read_past_end_of_object;
        }  /* if */
        if (error_code) {
          an_expr_node_ptr  arg = call_node->variant.operation.operands;
          arg = arg->next;
          do_constexpr_fail(result);
          info_with_pos(error_code, &arg->position, ips);
        }  /* if */
return_result:
        if (result) {
          if (ptr == NULL) {
            clear_address(addr, (a_byte*)NULL);
          } else {
            addr->address = (a_byte*)ptr;
          }  /* if */
          *(a_constexpr_address*)result_storage = *(a_constexpr_address*)addr;
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  return result;
}  /* do_constexpr_builtin_strchr */


static inline a_byte* base_address_of(a_constexpr_address   *cap)
/*
If the given address is that of an array element return the corresponding array
base address.  Otherwise, just return cap->address.
*/
{
  return is_array_element(cap) ? get_base_address(cap) : cap->address;
}  /* base_address_of */


static a_type_ptr obj_type_at_address(an_interpreter_state  *ips,
                                      a_constexpr_address   *cap)
/*
If the cap points to an object or subobject, return the type of that object.
Otherwise, return NULL.
*/
{
  a_type_ptr  tp = NULL;

  if (cap->address != NULL && !cannot_dereference(cap)) {
    a_byte  *addr = base_address_of(cap),
            *paddr = cap->complete_object;
    tp = complete_object_type(paddr);
    while (paddr != addr) {
      if (type_is(tp, tk_array)) {
        a_byte_count  esize, idx;
        a_boolean     result = TRUE;
        do {
          tp = skip_typerefs(tp->variant.array.element_type);
        } while (type_is(tp, tk_array));
        esize = value_bytes_for_type(ips, tp, &result);
        check_assertion(result);
        if (esize != 0) {
          idx = ((a_byte_count)(addr-paddr))/esize;
          paddr += (a_byte_count)(idx*esize);
        }  /* if */
      } else if (is_immediate_class_type(tp)) {
        a_field_ptr       fp;
        a_base_class_ptr  bcp;
        a_byte_count      offset;
        a_byte            *subobj_entry;
        find_subobject_for_interpreter_address(ips, cap, paddr, tp, &fp, &bcp);
        if (fp != NULL) {
          tp = skip_typerefs(fp->type);
          subobj_entry = (a_byte*)fp;
        } else {
          tp = bcp->type;
          subobj_entry = (a_byte*)bcp;
        }  /* if */
        get_mapped_byte_count(&persistent_map, subobj_entry, offset);
        paddr += offset;
      } else {
        /* A scalar type.  We shouldn't get here because interpreter addresses
           can only point to the start of a scalar object or one past the end
           of one. */
        unexpected_condition();
      }  /* if */
    }  /* while */
  }  /* if */
  return tp;
}  /* obj_type_at_address */


static a_boolean do_constexpr_memcpy(an_interpreter_state  *ips,
                                     a_boolean             is_move,
                                     a_constexpr_address   *src_cap,
                                     a_constexpr_address   *dst_cap,
                                     a_byte_count          n_target_bytes,
                                     an_expr_node_ptr      call_node)
/*
Interpret a memcpy-like intrinsic (__builtin_memcpy, __builtin_memmove,
__builtin_wmemcpy, or __builtin_wmemmove).  is_move is TRUE if this is a
memmove/wmemmove operation.  src_cap and dst_cap are the source and destination
addresses, respectively.  n_target_bytes is the number of bytes that must be
copied in the target architecture model.  call_node is the node representing
the invocation of the intrinsic.  Note that this routine does not create a
return value (unlike the actual intrinsic, which returns the destination
address).
*/
{
  a_boolean   result = TRUE;
  a_type_ptr  src_tp = obj_type_at_address(ips, src_cap),
              dst_tp = obj_type_at_address(ips, dst_cap);

  if (n_target_bytes == 0) {
    /* This call is essentially a no-op. */
  } else if (src_tp != NULL && dst_tp != NULL) {
    /* The source and destination point to objects. */
    a_type_ptr  src_etp = skip_typerefs(skip_array_types(src_tp)),
                dst_etp = skip_typerefs(skip_array_types(dst_tp));
    /* First check that the actual objects being copied are IL-identical and
       trivially copyable, and that only whole objects of nonzero length are
       copied. */
    if (!il_identical_types(src_etp, dst_etp)) {
      info_with_pos_type2(ec_constexpr_memcpy_distinct_types,
                         &call_node->position, src_etp, dst_etp, ips);
      do_constexpr_fail(result);
      goto done;
    } else if (is_immediate_class_type(src_etp) &&
               !is_trivially_copyable_type(src_etp)) {
      info_with_pos_type(ec_constexpr_memcpy_nontrivial_type,
                         &call_node->position, src_etp, ips);
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    a_byte_count  n_elems = src_etp->size == 0 ? MAX_ARRAY_LENGTH+1
                                               : n_target_bytes/src_etp->size;
    if (n_target_bytes-n_elems*src_etp->size != 0) {
      info_with_pos(ec_constexpr_memcpy_partial_object, &call_node->position,
                    ips);
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    /* Now check that the source and destination are large enough for the
       copy. */
    a_byte       *src_base = base_address_of(src_cap), *src = src_cap->address,
                 *dst_base = base_address_of(dst_cap), *dst = dst_cap->address;
    a_byte_count esize = value_bytes_for_type(ips, src_etp, &result),
                 src_base_length = (a_byte_count)num_array_elements(src_tp),
                 dst_base_length = (a_byte_count)num_array_elements(dst_tp),
                 k;
    if (n_elems > src_base_length || n_elems > dst_base_length ||
        n_elems > MAX_ARRAY_LENGTH ||
        (src_base_length-n_elems)*esize < (a_byte_count)(src-src_base) ||
        (dst_base_length-n_elems)*esize < (a_byte_count)(dst-dst_base)) {
      info_with_pos(ec_constexpr_memcpy_overflow, &call_node->position, ips);
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    a_byte_count  n_bytes = n_elems*esize;
    if (!is_move && src_base == dst_base &&
        ((src < dst && src+n_bytes > dst) ||
         (dst < src && dst+n_bytes < src))) {
      info_with_pos(ec_constexpr_memcpy_overlap, &call_node->position, ips);
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    (void)memmove(dst, src, n_bytes);
    a_byte  *complete_dst = dst_cap->complete_object;
    for (k = 0; k < n_elems; ++k, dst += esize) {
      mark_whole_subobject_initialized(ips, dst, dst_etp, complete_dst);
    }  /* for */
  } else {
    info_with_pos(ec_constexpr_memcpy_operand_not_object,
                  &call_node->position, ips);
    do_constexpr_fail(result);
  }  /* if */
done:
  return result;
}  /* do_constexpr_memcpy */


static a_boolean do_constexpr_builtin_strcmp(
                                      an_interpreter_state    *ips,
                                      a_boolean               is_memcmp,
                                      a_byte                  *arg1_bytes,
                                      a_type_ptr              arg1_tp,
                                      a_byte                  *arg2_bytes,
                                      a_type_ptr              arg2_tp,
                                      a_byte                  *arg3_bytes,
                                      a_type_ptr              arg3_tp,
                                      an_expr_node_ptr        call_node,
                                      a_byte                  *result_storage)
/*
Evaluate the strcmp/memcmp/wcscmp/wcsncmp/wmemcmp/strncmp family of builtin
functions.  If is_memcmp is TRUE, the builtin is either memcmp or wmemcmp (and
a NULL character doesn't terminate the comparison).  argX_bytes and argX_tp
specify the operands and their types (arg3_bytes and arg3_tp are NULL in cases
where a count argument is not specified).  Place the result in *result_storage.
Return FALSE if this fails (e.g., because the entity pointed to is not a
null-terminated string) and record a potential diagnostic for the given
expression node and interpreter state.
*/
{
  a_boolean             result = TRUE;
  a_constexpr_address   *addr1 = (a_constexpr_address*)arg1_bytes;
  a_constexpr_address   *addr2 = (a_constexpr_address*)arg2_bytes;
  a_type_ptr            tp;
  int                   ret_val = 0;
  a_host_large_integer  length_val = MAX_CONSTEXPR_TYPE_SIZE;

  check_assertion(type_is(arg1_tp, tk_pointer) &&
                  type_is(arg2_tp, tk_pointer));
  if (arg3_bytes != NULL) {
    /* If the "length" argument is zero, do nothing.  In particular, skip
       the check for null pointer arguments. */
    a_boolean  ovflo;
    check_assertion(arg3_tp != NULL &&
                    type_is(skip_typerefs(arg3_tp), tk_integer));
    conv_integer_value_to_host_large_integer(
                              (an_integer_value *)arg3_bytes,
                              is_signed_integral_type(arg3_tp)/*lint !e2666*/,
                              &length_val, &ovflo);
                                             
    if (!ovflo && length_val == 0) {
      goto return_result;
    }  /* if */
  }  /* if */
  tp = skip_typerefs(arg1_tp->variant.pointer.type);
  if (addr1->address == NULL) {
    do_constexpr_fail(result);
    info_with_pos((is_runtime_data_address(addr1) &&
                   constant_is(addr1->variant.addr_con, ck_integer)) ?
                                      ec_constexpr_null_dereference :
                                      ec_constexpr_access_to_runtime_storage,
                  &call_node->variant.operation.operands->next->position,
                  ips);
  } else if (addr2->address == NULL) {
    do_constexpr_fail(result);
    info_with_pos((is_runtime_data_address(addr2) &&
                   constant_is(addr2->variant.addr_con, ck_integer)) ?
                                      ec_constexpr_null_dereference :
                                      ec_constexpr_access_to_runtime_storage,
                &call_node->variant.operation.operands->next->next->position,
                ips);
  } else if (gpp_mode && !clang_mode &&
             addr1->address == addr2->address) {
    check_assertion(addr1->complete_object == addr2->complete_object);
    /* GCC appears to return 0 in cases where the two pointers refer to the
       same object (regardless of the number of bytes to compare). */
    goto return_result;
  } else if (!type_is(tp, tk_void)) {
    /* String or character array comparison. */
    an_integer_value  *ptr1 = (an_integer_value*)addr1->address;
    an_integer_value  *ptr2 = (an_integer_value*)addr2->address;
    an_integer_value  len, max_len, *eff_max;
    a_byte_count      elem_size1, pos1, max1 = MAX_CONSTEXPR_TYPE_SIZE;
    a_byte_count      elem_size2, pos2, max2 = MAX_CONSTEXPR_TYPE_SIZE;
    a_boolean         check_for_read_past_operand = FALSE;
    check_assertion(type_is(tp, tk_integer));
    if (is_array_element(addr1)) {
      get_array_pos(ips, addr1, tp, &max1, &pos1, &elem_size1, &result);
      max1 -= pos1;
    }  /* if */
    if (is_array_element(addr2)) {
      get_array_pos(ips, addr2, tp, &max2, &pos2, &elem_size2, &result);
      max2 -= pos2;
    }  /* if */
    if (!result) goto done;
    /* Use the minimum of the two object sizes for the loop below. */
    set_integer_value(&max_len,
                      (a_host_large_integer)(max1 > max2) ? max2 : max1);
    eff_max = &max_len;
    if (arg3_bytes != NULL) {
      check_assertion(arg3_tp != NULL &&
                      type_is(skip_typerefs(arg3_tp), tk_integer));
      if (cmp_integer_values(&max_len, /*op_1_signed=*/FALSE,
                             (an_integer_value *)arg3_bytes,
                             /*op_2_signed=*/FALSE) >= 0) {
        /* The user-specified count is less than the array count so use
           that. */
        eff_max = (an_integer_value *)arg3_bytes;
      } else {
        /* The user-specified count is more than the array count, so if
           a difference isn't found, we've gone past the array. */
        check_for_read_past_operand = TRUE;
      }  /* if */
    }  /* if */
    for (set_integer_value(&len, (a_host_large_integer)0);
         cmp_integer_values(&len, /*op_1_signed=*/FALSE, eff_max,
                            /*op_2_signed=*/FALSE);
         ptr1++, ptr2++, incr_integer_value(&len)) {
      ret_val = cmp_integer_values(ptr1, /*op_1_signed=*/FALSE, ptr2,
                                   /*op_2_signed=*/FALSE);
      if (ret_val != 0) {
        /* The comparison is finished. */
        goto return_result;
      }  /* if */
      /* *ptr1 and *ptr2 have the same value. */
      if (!is_memcmp &&
          cmp_integer_values(ptr1, /*op_1_signed=*/FALSE,
                             (an_integer_value *)&zero_int,
                             /*op_2_signed=*/FALSE) == 0) {
        /* In the string case, both strings have reached the null terminating
           character (and ret_val == 0). */
        goto return_result;
      }  /* if */
    }  /* for */
    /* No difference has been found. */
    if (check_for_read_past_operand) {
      /* If the user-specified length is greater than the sizes of the
         objects, this is an attempt to read past the end of the object. */
      an_expr_node_ptr  arg = call_node->variant.operation.operands;
      arg = arg->next;
      if (max2 < max1) {
        arg = arg->next;
      }  /* if */
      do_constexpr_fail(result);
      info_with_pos(ec_attempt_to_read_past_end_of_object, &arg->position,
                    ips);
      goto done;
    }  /* if */
  } else {
    /* Memory comparison (memcmp).  The general case cannot be evaluated
       because the byte count on the target machine layout usually differs
       from that of the interpreter layout.  However, Clang and GCC appear
       to handle this for the very specific case of comparing the bytes of
       two integer objects (or arrays thereof). */
    a_storage_stack_state  tmp_storage;
    a_byte                 *targ_repr1, *targ_repr2, *targ_map;
    a_type_ptr             obj_tp1, obj_tp2, utp1, utp2;
    a_boolean              equal_not_okay = FALSE;
    a_byte_count           size1, size2;
    save_storage_stack(ips, tmp_storage);
    check_assertion(is_memcmp);
    obj_tp1 = obj_type_at_address(ips, addr1);
    size1 = (a_byte_count)size_of_type(obj_tp1);
    utp1 = skip_typerefs(skip_array_types(obj_tp1));
    obj_tp2 = obj_type_at_address(ips, addr2);
    size2 = (a_byte_count)size_of_type(obj_tp2);
    utp2 = skip_typerefs(skip_array_types(obj_tp2));
    if (!type_is(utp1, tk_integer) || !type_is(utp2, tk_integer)) {
      info_with_pos(ec_invalid_constexpr_memcmp, &call_node->position, ips);
      do_constexpr_fail(result);
    }  /* if */
    alloc_stack_bytes(ips, size1, targ_repr1);
    alloc_stack_bytes(ips, size1, targ_map);
    if (!translate_interpreter_object_to_target_bytes(
                 ips, obj_tp1, base_address_of(addr1), addr1->complete_object,
                 targ_repr1, targ_map, call_node)) {
      unexpected_condition();
    }  /* if */
    alloc_stack_bytes(ips, size2, targ_repr2);
    alloc_stack_bytes(ips, size2, targ_map);
    if (!translate_interpreter_object_to_target_bytes(
                 ips, obj_tp2, base_address_of(addr2), addr2->complete_object,
                 targ_repr2, targ_map, call_node)) {
      unexpected_condition();
    }  /* if */
    /* Adjust for the offset into either array. */
    if (is_array_element(addr1)) {
      a_byte_count  max1, pos1, elem_size1;
      get_array_pos(ips, addr1, utp1, &max1, &pos1, &elem_size1, &result);
      size1 -= pos1*(a_byte_count)utp1->size;
      targ_repr1 += pos1*utp1->size;
    }  /* if */
    if (is_array_element(addr2)) {
      a_byte_count  max2, pos2, elem_size2;
      get_array_pos(ips, addr2, utp2, &max2, &pos2, &elem_size2, &result);
      size2 -= pos2*(a_byte_count)utp2->size;
      targ_repr2 += pos2*utp2->size;
    }  /* if */
    if (result) {
      /* Trim the length being compared if needed, to avoid reading
         uninitialized bytes.  However, if we needed to trim, an "equal" result
         (i.e., zero) implies that the untrimmed comparison would have read
         uninitialized memory, which isn't valid in a constant evaluation. */
      if ((a_byte_count)length_val > size1) {
        length_val = size1;
        equal_not_okay = TRUE;
      }  /* if */
      if ((a_byte_count)length_val > size2) {
        length_val = size2;
        equal_not_okay = TRUE;
      }  /* if */
      ret_val = memcmp(targ_repr1, targ_repr2, size_t_arg(length_val));
      if (equal_not_okay && ret_val == 0) {
        /* An attempt to read uninitialized memory. */
        an_expr_node_ptr  arg = call_node->variant.operation.operands;
        arg = arg->next;
        if (size2 < size1) {
          arg = arg->next;
        }  /* if */
        do_constexpr_fail(result);
        info_with_pos(ec_attempt_to_read_past_end_of_object, &arg->position,
                      ips);
      }  /* if */
    }  /* if */
    restore_storage_stack(ips, tmp_storage, result);
  }  /* if */
return_result:
  if (result) {
    set_integer_value((an_integer_value*)result_storage,
                      (a_host_large_integer)ret_val);
  }  /* if */
done:
  return result;
}  /* do_constexpr_builtin_strcmp */


static a_boolean within_int_bounds(an_integer_value *input_int,
                                   a_boolean        input_signed,
                                   an_integer_kind  dest_kind,
                                   a_boolean        dest_signed)
/*
Given an (possibly signed) integer value, check to see if it fits within the
(possibly signed) destination integer kind.  Return TRUE if the input integer
can be converted without loss, otherwise return FALSE.
*/
{
  a_boolean over = cmp_integer_values(input_int, input_signed,
                                      &max_integer_value_of_kind[dest_kind],
                                      dest_signed) > 0;
  a_boolean under = cmp_integer_values(input_int, input_signed,
                                       &min_integer_value_of_kind[dest_kind],
                                       dest_signed) < 0;

  return !over && !under;
}


template<typename a_Host_integer_type>
static a_boolean safely_set_host_integer_value(
                                          a_type_ptr           result_type,
                                          a_byte               *result_storage,
                                          a_Host_integer_type  value)
/*
Given the result type and the associated storage for the result, convert and
store the given host value.  Return TRUE if successfully converted and stored
without any integer bounding issues; otherwise, return FALSE.
*/
{
  /* As all integers types are currently stored by the interpreter using the
     same representation, assume there's no overflow or underflow, and store
     the integer in the "universal representation". */
  set_host_integer_value((an_integer_value*)result_storage, value);
  /* Then compute the signs and the destination int type. */
  a_boolean        input_value_signed = a_Host_integer_type(-1) <
                                                        a_Host_integer_type(0);
  a_boolean        result_value_signed = is_signed_integral_type(result_type);
  an_integer_kind  int_kind = result_type->variant.integer.int_kind;

  /* Using the aforementioned information, check that the completed operation
     wasn't lossy. */
  return within_int_bounds((an_integer_value*)result_storage,
                           input_value_signed, int_kind, result_value_signed);
}  /* safely_set_host_integer_value */


static void do_constexpr_write_source_column(
                                          an_interpreter_state *ips,
                                          a_source_position    *use_pos,
                                          a_type_ptr           result_type,
                                          a_byte               *result_storage,
                                          a_boolean            *p_result)
/*
Given the result type and the associated storage and interpreter state for the
result, convert and store the column number associated with the given position
(use_pos).  If any problems are encountered, the pointee of p_result will be
set to FALSE.
*/
{
#if EXPENSIVE_CHECKING
  check_assertion(is_integral_type(result_type));
#endif /* EXPENSIVE_CHECKING */
  if (!safely_set_host_integer_value(result_type, result_storage,
                                     use_pos->column)) {
    info_with_pos_type(ec_srcloc_column_bounds, use_pos, result_type, ips);
    do_constexpr_fail(*p_result);
  }  /* if */
}  /* do_constexpr_write_source_column */


static void do_constexpr_write_source_line(
                                          an_interpreter_state *ips,
                                          a_source_position    *use_pos,
                                          a_type_ptr           result_type,
                                          a_byte               *result_storage,
                                          a_boolean            *p_result)
/*
Given the result type and the associated storage and interpreter state for the
result, convert and store the line number associated with the given position
(use_pos).  If any problems are encountered, the pointee of p_result will be
set to FALSE.
*/
{
  a_line_number  line_number;
  a_boolean      at_end_of_source;
  a_const_char   *file_name, *full_name;

  (void)conv_seq_to_file_and_line(use_pos->seq, &file_name, &full_name,
                                  &line_number, &at_end_of_source);
#if EXPENSIVE_CHECKING
  check_assertion(is_integral_type(result_type));
#endif /* EXPENSIVE_CHECKING */
  if (!safely_set_host_integer_value(result_type, result_storage,
                                     line_number)) {
    info_with_pos_type(ec_srcloc_line_bounds, use_pos, result_type, ips);
    do_constexpr_fail(*p_result);
  }  /* if */
}  /* do_constexpr_write_source_line */


static void do_constexpr_write_cstring(an_interpreter_state *ips,
                                       a_const_char         *result_string,
                                       a_byte               *result_storage,
                                       a_boolean            *p_result)
/*
Given the associated storage and interpreter state for the resulting c-string,
convert and store the given string (result_string).  If any problems are
encountered, the pointee of p_result will be set to FALSE.
*/
{
  a_constant_ptr  cp;
  a_type_ptr      type;
  a_byte_count    length, k;
  a_byte          *string_bytes;
  a_boolean       result = TRUE;

  /* Obtain a shareable ck_string constant for the name. */
  cp = shareable_fs_string_constant(result_string);
  type = cp->type;
  length = (a_byte_count)cp->variant.string.length;
  /* Do we have an interpreter version of the string already? */
  get_stack_bytes(ips, cp->variant.string.value, string_bytes);
  if (string_bytes == NULL) {
    /* First time seeing this string; allocate it in static storage. */
    alloc_static_object(ips, type, string_bytes, &result);
    if (result) {
      /* Copy the string from host format to interpreter format (i.e.,
         an integer for each character). */
      a_type_ptr     etp = skip_typerefs(type->variant.array.element_type);
      a_byte_count   elem_size = value_bytes_for_type(ips, etp, &result);
      a_targ_size_t  char_size = etp->size;
      a_const_char   *char_ptr = cp->variant.string.value;
      a_byte         *elem = string_bytes;

      if (result) {
        for (k = 0; k<length; ++k, elem += elem_size) {
          unsigned long char_val = extract_character_from_string(
                                                      char_ptr,
                                                      (unsigned int)char_size);
          set_integer_value((an_integer_value*)elem,
                            (a_host_large_integer)char_val);
          char_ptr += char_size;
          mark_subobject_initialized(elem, string_bytes);
        }  /* for */
        mark_complete_object_initialized(string_bytes);
        /* Create a mapping to the string so the original IL constant
           can be found. */
        map_stack_bytes(ips, string_bytes, (a_byte*)cp);
      }  /* if */
    }  /* if */
  }  /* if */
  if (result) {
    /* Return the result. */
    clear_address(result_storage, string_bytes);
    ((a_constexpr_address*)result_storage)->flags |=
                                           CA_CONST_STORAGE | CA_ARRAY_ELEMENT;
    ((a_constexpr_address*)result_storage)->length = length;
  } else {
    *p_result = FALSE;
  }  /* if */
}  /* do_constexpr_write_cstring */


static void do_constexpr_write_source_file(
                                          an_interpreter_state *ips,
                                          a_source_position    *use_pos,
                                          a_byte               *result_storage,
                                          a_boolean            *p_result)
/*
Given the associated storage and interpreter state for the resulting c-string,
convert and store the source file name associated with the given position
(use_pos).  If any problems are encountered, the pointee of p_result will be
set to FALSE.
*/
{
  a_line_number  line_number;
  a_boolean      at_end_of_source;
  a_const_char   *file_name, *full_name;

  (void)conv_seq_to_file_and_line(use_pos->seq, &file_name,
                                  &full_name, &line_number,
                                  &at_end_of_source);
  do_constexpr_write_cstring(ips, file_name, result_storage, p_result);
}  /* do_constexpr_write_source_line */


static void do_constexpr_write_source_function(
                                          an_interpreter_state *ips,
                                          a_source_position    *use_pos,
                                          a_byte               *result_storage,
                                          a_boolean            *p_result)
/*
Given the associated storage and interpreter state for the resulting c-string,
convert and store the source function name associated with the given position
(use_pos).  When the given position doesn't have an associated function, an
empty c-string is instead stored.  If any problems are encountered, the pointee
of p_result will be set to FALSE.
*/
{
  /* Use the same string as if using __func__. */
  a_const_char  *func_name = get_string_for_function_name(
                                                      tok_func_name,
                                                      /*include_quote=*/FALSE);

  do_constexpr_write_cstring(ips, func_name, result_storage, p_result);
}  /* do_constexpr_write_source_function */


static a_boolean do_constexpr_builtin_source_pos_func(
                                          an_interpreter_state *ips,
                                          a_routine_ptr        callee,
                                          a_byte               *result_storage,
                                          a_boolean            *p_result)
/*
If possible, fold the source location builtin (i.e., __builtin_COLUMN,
__builtin_LINE, __builtin_FILE, __builtin_FUNCTION) into the appropriate
constant.  Generally, these calls are easily folded, but if they occur
in a default argument list of a consteval function or a default member
initializer, they are not folded here.  For default arguments, the folding
occurs in i_copy_expr_tree when the expression is being copied, and for
default member initializers, lowering does the work.
*/
{
  a_boolean  result = TRUE;

  if ((expr_stack != NULL &&
       expr_stack->is_default_arg_expression &&
       !expr_stack->consteval_call_need_not_fold) ||
      scope_stack_top().in_field_initializer) {
    /* As mentioned above, uses in default arguments of consteval functions
       and default member initializers are deferred. */
    do_constexpr_fail(result);
  } else {
    /* Determine the appropriate source position for this invocation and
       convert it to the appropriate integer (COLUMN, LINE) or string (FILE,
       FUNCTION). */
    a_source_position  *use_pos;

    if (ips->curr_call_frame == NULL ||
        ips->curr_call_frame->routine == NULL) {
      /* This is the usual case: The __builtin_... is folded right away when
         it is scanned, or when it is copied from a default argument.  In that
         case error_position ought to be the position of the call. */
      use_pos = &error_position;
    } else {
      /* If the __builtin_... was called in a default member initializer, the
         interpreter doesn't get to it until a constructor "uses" that
         initializer.  Get the position of the constructor from the call
         stack. */
      use_pos = &ips->curr_call_frame->routine->source_corresp.decl_position;
    }  /* if */
    switch (callee->variant.builtin_function_kind) {
      case bfk_COLUMN:
        { an_integer_kind col_int_kind = (an_integer_kind)ik_unsigned_int;

          do_constexpr_write_source_column(ips, use_pos,
                                           integer_type(col_int_kind),
                                           result_storage, p_result);
        }
        break;
      case bfk_LINE:
        { an_integer_kind line_int_kind = (an_integer_kind)ik_unsigned_long;

          do_constexpr_write_source_line(ips, use_pos,
                                         integer_type(line_int_kind),
                                         result_storage, p_result);
        }
        break;
      case bfk_FILE:
        do_constexpr_write_source_file(ips, use_pos, result_storage, p_result);
        if (p_result) {
          mark_complete_object_initialized(result_storage);
        }  /* if */
        break;
      case bfk_FUNCTION:
        do_constexpr_write_source_function(ips, use_pos, result_storage,
                                           p_result);
        if (p_result) {
          mark_complete_object_initialized(result_storage);
        }  /* if */
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* if */
  return result;
}  /* do_constexpr_builtin_source_pos_func */


static a_boolean do_constexpr_builtin_function(
                                        an_interpreter_state  *ips,
                                        a_routine_ptr         callee,
                                        an_expr_node_ptr      call_node,
                                        a_byte                *result_storage,
                                        a_boolean             *p_result)
/*
call_node represents a call to the given callee, which is a builtin function.
If the call belongs to the class of builtin functions that can sometimes be
folded (i.e., it is effectively "constexpr"), return TRUE; otherwise, return
FALSE.  If TRUE if returned, but folding was not successful, *p_result is set
to FALSE and the reason for the failure is recorded in *ips.
*/
{
  a_boolean         interpreted;
#if C99_IL_EXTENSIONS_SUPPORTED
  a_boolean         err = FALSE, depends_on_fp_mode;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  a_boolean         has_count = FALSE, is_memcmp = FALSE;
  an_expr_node_ptr  args = call_node->variant.operation.operands->next;
  an_expr_node_ptr  args2, args3;
  a_byte            *arg1_bytes, *arg2_bytes, *arg3_bytes;

  ips->cost += 1;
  switch (callee->variant.builtin_function_kind) {
    case bfk_constant_p:
      if (ips->curr_call_frame == NULL) {
        /* Do not attempt interpreting __builtin_constant_p as an argument to
           a top-level call, because we don't want to fold such a call
           permanently.  E.g.:
             constexpr int g(int x) { return __builtin_constant_p(x); }
             constexpr int v = g(3);
           While generating the IL for the call __builtin_constant_p(x), we
           don't want to interpret the call (which would yield a false value
           since x is not a constant at that point) because that would freeze
           the result for any invocation of g.  However, during the evaluation
           of g(3), we do want to interpret __builtin_constant_p(x) and that
           will yield a true value since x is the known constant 3 at that
           point. */
        interpreted = FALSE;
      } else {
        interpreted = TRUE;
        if (args == NULL || args->next != NULL) {
          /* A malformed __builtin_constant_p construct.  Treat that as not
             being constant. */
          *(an_integer_value*)result_storage = zero_int;
        } else {
          /* Put the interpreter in "no side-effects" mode and interpret the
             argument.  If successful, the argument is considered "constant"
             (i.e., __builtin_constant_p produces a true value).  If
             unsuccessful, we can still continue interpretation because no
             side-effects took place. */
          a_boolean     saved_side_effects_disabled, saved_suspend_diag_list;
          a_type_ptr    arg_type = skip_typerefs(args->type);
          a_byte_count  n_bytes = expr_result_size(ips, args, arg_type,
                                                   p_result);
          if (!*p_result) break;
          saved_side_effects_disabled = ips->side_effects_disabled;
          ips->side_effects_disabled = TRUE;
          saved_suspend_diag_list = ips->suspend_diag_list;
          ips->suspend_diag_list = TRUE;
          alloc_complete_object(ips, n_bytes, arg_type, arg1_bytes);
          if (do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes)) {
            *(an_integer_value*)result_storage = one_int;
          } else {
            *(an_integer_value*)result_storage = zero_int;
          }  /* if */
          ips->side_effects_disabled = saved_side_effects_disabled;
          ips->suspend_diag_list = saved_suspend_diag_list;
        }  /* if */
      }  /* if */
      break;
   case bfk_abs:
      {
        interpreted = TRUE;
        if (args == NULL || args->next != NULL) {
          unexpected_condition();
        } else {
          a_type_ptr    tp = skip_typerefs(args->type);
          a_byte_count  n_bytes = value_bytes_for_type(ips, tp, p_result);
          if (!*p_result) break;
          check_assertion(tp->kind == (a_type_kind)tk_integer);
          alloc_complete_object(ips, n_bytes, tp, arg1_bytes);
          if (!do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes)) {
            do_constexpr_fail(*p_result);
          } else {
            an_integer_kind  int_kind = tp->variant.integer.int_kind;
            *(an_integer_value*)result_storage =
                                               *(an_integer_value*)arg1_bytes;
            if (int_kind_is_signed[int_kind] &&
                cmp_integer_values((an_integer_value *)result_storage,
                                   /*is_signed=*/TRUE,
                                   (an_integer_value *)&zero_int,
                                   /*is_signed=*/TRUE) < 0) {
              a_boolean  ovfl;
              negate_integer_value((an_integer_value *)result_storage, &ovfl);
              if (ovfl ||
                  cmp_integer_values((an_integer_value *)result_storage,
                                     /*is_signed=*/TRUE,
                                     &max_integer_value_of_kind[int_kind],
                                     /*is_signed=*/TRUE) > 0) {
                do_constexpr_fail(*p_result);
                info_with_pos_type(ec_constexpr_integer_overflow,
                                   &call_node->position, tp, ips);
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case bfk_fabs:
    case bfk_fabsf:
    case bfk_fabsl:
      {
        interpreted = TRUE;
        if (args == NULL || args->next != NULL) {
          unexpected_condition();
        } else {
          a_type_ptr    tp = skip_typerefs(args->type);
          a_byte_count  n_bytes = value_bytes_for_type(ips, tp, p_result);
          if (!*p_result) break;
          check_assertion(is_real_floating_type(tp));
          alloc_complete_object(ips, n_bytes, tp, arg1_bytes);
          if (do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes)) {
            a_float_kind  fk = tp->variant.float_kind;
            if (fp_signbit(fk, fp_value(arg1_bytes))) {
              fp_negate(fk, fp_value(arg1_bytes), fp_value(result_storage),
                        &err, &depends_on_fp_mode);
              check_assertion(!err);
            } else {
              *fp_value(result_storage) = *fp_value(arg1_bytes);
            }  /* if */
          } else {
            do_constexpr_fail(*p_result);
          }  /* if */
        }  /* if */
      }
      break;
    case bfk_isnan:
    case bfk_isnanf:
    case bfk_isnanl:
    case bfk_isinf:
    case bfk_isinff:
    case bfk_isinfl:
    case bfk_isfinite:
    case bfk_isnormal:
    case bfk_signbit:
    case bfk_signbitf:
    case bfk_signbitl:
      {
#if TARG_HAS_IEEE_FLOATING_POINT
        interpreted = TRUE;
        if (args == NULL || args->next != NULL) {
          unexpected_condition();
        } else {
          a_type_ptr    tp = skip_typerefs(args->type);
          a_byte_count  n_bytes = value_bytes_for_type(ips, tp, p_result);
          if (!*p_result) break;
          check_assertion(is_real_floating_type(tp));
          alloc_complete_object(ips, n_bytes, tp, arg1_bytes);
          if (do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes)) {
            a_float_kind  fk = tp->variant.float_kind;
            if (!do_constexpr_builtin_fptest(callee, fk, fp_value(arg1_bytes),
                                             result_storage)) {
              info_with_pos(ec_constexpr_fp_error, &call_node->position, ips);
              do_constexpr_fail(*p_result);
            }  /* if */
          } else {
            do_constexpr_fail(*p_result);
          }  /* if */
        }  /* if */
#else /* !TARG_HAS_IEEE_FLOATING_POINT */
        interpreted = FALSE;
        do_constexpr_fail(*p_result);
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
      }
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case bfk_ceil:
    case bfk_ceilf:
    case bfk_ceill:
      /* Compute "ceiling" of the argument. */
      {
        interpreted = TRUE;
        if (args == NULL || args->next != NULL) {
          unexpected_condition();
        } else {
          a_type_ptr    tp = skip_typerefs(args->type);
          a_byte_count  n_bytes = value_bytes_for_type(ips, tp, p_result);
          if (!*p_result) break;
          check_assertion(is_real_floating_type(tp));
          alloc_complete_object(ips, n_bytes, tp, arg1_bytes);
          if (do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes)) {
            fp_ceil(tp->variant.float_kind, fp_value(arg1_bytes),
                    fp_value(result_storage), &err);
            if (err) do_constexpr_fail(*p_result);
          } else {
            do_constexpr_fail(*p_result);
          }  /* if */
        }  /* if */
      }
      break;
    case bfk_ffs:
    case bfk_ffsl:
    case bfk_clz:
    case bfk_clzl:
    case bfk_ctz:
    case bfk_ctzl:
    case bfk_popcount:
    case bfk_popcountl:
    case bfk_parity:
    case bfk_parityl:
#if LONG_LONG_ALLOWED
    case bfk_ffsll:
    case bfk_clzll:
    case bfk_ctzll:
    case bfk_popcountll:
    case bfk_parityll:
#endif /* LONG_LONG_ALLOWED */
      {
        interpreted = TRUE;
        if (args == NULL || args->next != NULL) {
          unexpected_condition();
        } else {
          a_type_ptr    tp = skip_typerefs(args->type);
          a_byte_count  n_bytes = value_bytes_for_type(ips, tp, p_result);
          if (!*p_result) break;
          alloc_complete_object(ips, n_bytes, tp, arg1_bytes);
          if (!do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes) ||
              !do_constexpr_builtin_bitcount(
                                    callee, arg1_bytes, tp, result_storage)) {
            do_constexpr_fail(*p_result);
          }  /* if */
        }  /* if */
      }
      break;
    case bfk_strlen:
    case bfk_wcslen:
    case bufk_u8strlen:
      {
        interpreted = TRUE;
        if (args == NULL || args->next != NULL) {
          unexpected_condition();
        } else {
          a_type_ptr    tp = skip_typerefs(args->type);
          a_byte_count  n_bytes = value_bytes_for_type(ips, tp, p_result);
          if (!*p_result) break;
          alloc_complete_object(ips, n_bytes, tp, arg1_bytes);
          if (!do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes) ||
              !do_constexpr_builtin_strlen(ips, arg1_bytes, tp, call_node,
                                           result_storage)) {
            do_constexpr_fail(*p_result);
          }  /* if */
        }  /* if */
      }
      break;
    case bfk_memchr:
    case bfk_wmemchr:
    case bfk_char_memchr:
    case bufk_u8memchr:
      has_count = TRUE;
      FALLTHROUGH
    case bfk_strchr:
    case bfk_wcschr:
      {
        interpreted = TRUE;
        if (args == NULL || args->next == NULL ||
            (has_count ?
                   args->next->next == NULL || args->next->next->next != NULL :
                   args->next->next != NULL)) {
          unexpected_condition();
        } else {
          /* Process the first argument. */
          a_type_ptr    arg2_tp, arg3_tp, arg1_tp = skip_typerefs(args->type);
          a_byte_count  n_bytes = value_bytes_for_type(ips, arg1_tp, p_result);
          if (!*p_result) break;
          alloc_complete_object(ips, n_bytes, arg1_tp, arg1_bytes);
          /* Process the second argument. */
          args2 = args->next;
          arg2_tp = skip_typerefs(args2->type);
          n_bytes = value_bytes_for_type(ips, arg2_tp, p_result);
          if (!*p_result) break;
          alloc_complete_object(ips, n_bytes, arg2_tp, arg2_bytes);
          if (has_count) {
            /* Process optional count argument. */
            args3 = args2->next;
            arg3_tp = skip_typerefs(args3->type);
            n_bytes = value_bytes_for_type(ips, arg3_tp, p_result);
            if (!*p_result) break;
            alloc_complete_object(ips, n_bytes, arg3_tp, arg3_bytes);
          } else {
            args3 = NULL;
            arg3_bytes = NULL;
            arg3_tp = NULL;
          }  /* if */
          if (!do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes) ||
              !do_constexpr_expression(ips, args2, arg2_bytes, arg2_bytes) ||
              (has_count &&
               !do_constexpr_expression(ips, args3, arg3_bytes, arg3_bytes)) ||
              !do_constexpr_builtin_strchr(ips,
                                           arg1_bytes, arg1_tp,
                                           arg2_bytes, arg2_tp,
                                           arg3_bytes, arg3_tp,
                                           call_node, result_storage)) {
            do_constexpr_fail(*p_result);
          }  /* if */
        }  /* if */
      }
      break;
    case bfk_memcmp:
    case bfk_wmemcmp:
    case bufk_u8memcmp:
      is_memcmp = TRUE;
      FALLTHROUGH
    case bfk_strncmp:
    case bfk_wcsncmp:
      has_count = TRUE;
      FALLTHROUGH
    case bfk_strcmp:
    case bfk_wcscmp:
      {
        interpreted = TRUE;
        if (args == NULL || args->next == NULL || 
            (has_count ?
                   args->next->next == NULL || args->next->next->next != NULL :
                   args->next->next != NULL)) {
          unexpected_condition();
        } else {
          /* Process the first argument. */
          a_type_ptr    arg2_tp, arg3_tp, arg1_tp = skip_typerefs(args->type);
          a_byte_count  n_bytes = value_bytes_for_type(ips, arg1_tp, p_result);
          if (!*p_result) break;
          alloc_complete_object(ips, n_bytes, arg1_tp, arg1_bytes);
          /* Process the second argument. */
          args2 = args->next;
          arg2_tp = skip_typerefs(args2->type);
          n_bytes = value_bytes_for_type(ips, arg2_tp, p_result);
          if (!*p_result) break;
          alloc_complete_object(ips, n_bytes, arg2_tp, arg2_bytes);
          if (has_count) {
            /* Process optional count argument. */
            args3 = args2->next;
            arg3_tp = skip_typerefs(args3->type);
            n_bytes = value_bytes_for_type(ips, arg3_tp, p_result);
            if (!*p_result) break;
            alloc_complete_object(ips, n_bytes, arg3_tp, arg3_bytes);
          } else {
            args3 = NULL;
            arg3_bytes = NULL;
            arg3_tp = NULL;
          }  /* if */
          if (!do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes) ||
              !do_constexpr_expression(ips, args2, arg2_bytes, arg2_bytes) ||
              (has_count &&
               !do_constexpr_expression(ips, args3, arg3_bytes, arg3_bytes)) ||
              !do_constexpr_builtin_strcmp(ips, is_memcmp,
                                           arg1_bytes, arg1_tp,
                                           arg2_bytes, arg2_tp,
                                           arg3_bytes, arg3_tp,
                                           call_node, result_storage)) {
            do_constexpr_fail(*p_result);
          }  /* if */
        }  /* if */
      }
      break;
    case bfk_memcpy:
    case bfk_memmove:
    case bfk_wmemcpy:
    case bfk_wmemmove:
      {
        interpreted = TRUE;
        if (args == NULL || args->next == NULL || 
            args->next->next == NULL || args->next->next->next != NULL) {
          unexpected_condition();
        } else {
          a_type_ptr  arg1_tp = skip_typerefs(args->type);
          alloc_complete_object(ips, sizeof(a_constexpr_address), arg1_tp,
                                arg1_bytes);
          args2 = args->next;
          a_type_ptr  arg2_tp = skip_typerefs(args2->type);
          alloc_complete_object(ips, sizeof(a_constexpr_address), arg2_tp,
                                arg2_bytes);
          args3 = args2->next;
          a_type_ptr  arg3_tp = skip_typerefs(args3->type);
          alloc_complete_object(ips, sizeof(an_integer_value), arg3_tp,
                                arg3_bytes);
          if (!do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes) ||
              !do_constexpr_expression(ips, args2, arg2_bytes, arg2_bytes) ||
              !do_constexpr_expression(ips, args3, arg3_bytes, arg3_bytes)) {
            do_constexpr_fail(*p_result);
            break;
          } else {
            a_boolean             ovflo, is_move = FALSE, is_wide = FALSE;
            a_host_large_integer  length_val = MAX_CONSTEXPR_TYPE_SIZE;
            conv_integer_value_to_host_large_integer(
                              (an_integer_value *)arg3_bytes,
                              is_signed_integral_type(arg3_tp)/*lint !e2666*/,
                              &length_val, &ovflo);
            switch (callee->variant.builtin_function_kind) {
              case bfk_memcpy:                                   break;
              case bfk_memmove:  is_move = TRUE;                 break;
              case bfk_wmemcpy:                  is_wide = TRUE; break;
              case bfk_wmemmove: is_move = TRUE; is_wide = TRUE; break;
              default: unexpected_condition();
            }  /* switch */
            if (ovflo || length_val > MAX_CONSTEXPR_TYPE_SIZE) {
              length_val = MAX_CONSTEXPR_TYPE_SIZE+1;
            }  /* if */
            if (is_wide) {
              length_val *= wchar_t_type()->size;
            }  /* if */
            if (!do_constexpr_memcpy(ips, is_move,
                                     (a_constexpr_address*)arg2_bytes,
                                     (a_constexpr_address*)arg1_bytes,
                                     (a_byte_count)length_val, call_node)) {
              do_constexpr_fail(*p_result);
            } else {
              *(a_constexpr_address*)result_storage =
                                            *(a_constexpr_address*)arg1_bytes;
            }  /* if */
          }  /* if */
        }  /* if */
      }
      break;
    case bfk_bswap16:
    case bfk_bswap32:
    case bfk_bswap64:
      {
        interpreted = TRUE;
        if (args == NULL || args->next != NULL) {
          unexpected_condition();
        } else if (targ_char_bit != 8) {
          info_with_pos(ec_cannot_interpret_target_bits,
                        &call_node->position, ips);
          do_constexpr_fail(*p_result);
        } else {
          unsigned int  bytes = 0;
          a_type_ptr    tp = skip_typerefs(args->type);
          a_byte_count  n_bytes = value_bytes_for_type(ips, tp, p_result);
          if (!*p_result) break;
          switch (callee->variant.builtin_function_kind) {
            case bfk_bswap16:
              bytes = 2;
              break;
            case bfk_bswap32:
              bytes = 4;
              break;
            case bfk_bswap64:
              bytes = 8;
              break;
            default:
              unexpected_condition();
          }  /* switch */
          check_assertion(type_is(tp, tk_integer));
          alloc_complete_object(ips, n_bytes, tp, arg1_bytes);
          if (!do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes) ||
              !swap_bytes_in_unsigned_integer(bytes,
                                         (an_integer_value *)arg1_bytes,
                                         (an_integer_value *)result_storage)) {
            do_constexpr_fail(*p_result);
          }  /* if */
        }  /* if */
      }
      break;
    case bfk_expect:
    case bfk_expect_with_probability:
      {
        interpreted = TRUE;
        /* Evaluate the first argument instead of the call.  (The second
           argument -- and third for __builtin_expect_with_probability -- are
           ignored.)  */
        if (ips->curr_call_frame == NULL && !ips->is_constant_evaluated) {
          /* Do not fold the invocation if it is at the top level, since a
             back end will want to see the intrinsic invocation to decide
             branch prediction hints. */
          do_constexpr_fail(*p_result);
          break;
        }  /* if */
        if (args == NULL || args->next == NULL ||
            (callee->variant.builtin_function_kind ==
                         (a_builtin_function_kind)bfk_expect_with_probability ?
              args->next->next == NULL || args->next->next->next != NULL :
              args->next->next != NULL)) {
          unexpected_condition();
        } else if (!do_constexpr_expression(ips, args, result_storage,
                                            result_storage)) {
          do_constexpr_fail(*p_result);
        }  /* if */
      }
      break;
    case bufk_launder:
      {
        interpreted = TRUE;
        /* Evaluate the single argument instead of the call. */
        if (args == NULL || args->next != NULL) {
          unexpected_condition();
        } else if (!do_constexpr_expression(ips, args, result_storage,
                                            result_storage)) {
          do_constexpr_fail(*p_result);
        }  /* if */
      }
      break;
    case bfk_is_constant_evaluated:
      {
        interpreted = TRUE;
        /* Return a true value if ips->is_constant_evaluated is TRUE, or
           fail interpretation otherwise. */
        if (args != NULL) {
          unexpected_condition();
        } else if (ips->is_constant_evaluated) {
          *(an_integer_value*)result_storage = one_int;
          if (ips->curr_call_frame == NULL &&
              !ips->is_variable_initializer) {
            warn_about_is_constant_evaluated(callee, call_node);
          }  /* if */
        } else {
          do_constexpr_fail(*p_result);
        }  /* if */
      }
      break;
    case bfk_zero_non_value_bits:
      interpreted = TRUE;
      /* Generally, __builtin_zero_non_value_bits is a no-op in the
         interpreter.  Its function is to zero padding bits, but the
         interpreter doesn't model padding bits (except during a
         __builtin_bit_cast operation but that doesn't survive after the
         operation is completed).  Fail if the address points to a
         runtime data address or the argument is incorrect. */
      if (args == NULL || args->next != NULL ||
          !is_pointer_type(args->type) ||
          is_void_type(type_pointed_to(args->type))) {
        /* Invalid argument (error has already been given). */
        do_constexpr_fail(*p_result);
      } else {
        a_type_ptr    tp = skip_typerefs(args->type);
        a_byte_count  n_bytes = value_bytes_for_type(ips, tp, p_result);
        if (!*p_result) {
          do_constexpr_fail(*p_result);
        } else {
          alloc_complete_object(ips, n_bytes, tp, arg1_bytes);
          if (!do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes)) {
            /* Argument did not have a constexpr value. */
            do_constexpr_fail(*p_result);
          } else {
            a_constexpr_address  *addr = (a_constexpr_address*)arg1_bytes;
            if (is_runtime_data_address(addr)) {
              /* Address must be known at constexpr time. */
              info_with_pos(ec_constexpr_access_to_runtime_storage,
                            &args->position, ips);
              do_constexpr_fail(*p_result);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      break;
    case bufk_source_location:
      {
        interpreted = FALSE;
        if (expr_stack != NULL && expr_stack->is_default_arg_expression) {
          do_constexpr_fail(*p_result);
        } else {
          a_gnu_source_location_type_info interp_inf;

          /* Load the type information, if the type is invalid, silently fail
             the interpretation (this has already been diagnosed). */
          interp_inf = gnu_source_location_impl();
          if (is_error_type(interp_inf.impl_type)) {
            do_constexpr_fail(*p_result);
          } else {
            a_byte  *obj_storage;

            interpreted = TRUE;
            /* Allocate the source location __impl object. */
            alloc_naturalizable_object(ips, interp_inf.impl_type, &obj_storage,
                                       p_result);
            if (*p_result) {
              a_source_position *use_pos = ips->srcloc_builtin_pos;

              /* This can occur when the builtin is used raw, fallback to the
                 error position. */
              if (use_pos == NULL) {
                use_pos = &error_position;
              }  /* if */

              /* Populate the source location __impl object with the values for
                 use_pos.  Note that the fields (and their associated types)
                 are guaranteed to have been validated upon use of the
                 builtin. */
              a_field_ptr   file_name_fp = interp_inf.file_field;
              a_byte_count  file_name_f_offset;
              get_mapped_byte_count(&persistent_map, file_name_fp,
                                  file_name_f_offset);
              a_byte        *file_name_f_bytes = obj_storage +
                                                            file_name_f_offset;
              do_constexpr_write_source_file(ips, use_pos, file_name_f_bytes,
                                             p_result);
              mark_subobject_initialized(file_name_f_bytes, obj_storage);

              a_field_ptr   function_name_fp = interp_inf.function_field;
              a_byte_count  function_name_f_offset;
              get_mapped_byte_count(&persistent_map, function_name_fp,
                                    function_name_f_offset);
              a_byte        *function_name_f_bytes = obj_storage +
                                                        function_name_f_offset;
              do_constexpr_write_source_function(ips, use_pos,
                                                 function_name_f_bytes,
                                                 p_result);
              mark_subobject_initialized(function_name_f_bytes, obj_storage);

              a_field_ptr   line_fp = interp_inf.line_field;
              a_byte_count  line_f_offset;
              get_mapped_byte_count(&persistent_map, line_fp, line_f_offset);
              a_byte        *line_f_bytes = obj_storage + line_f_offset;
              do_constexpr_write_source_line(ips, use_pos, line_fp->type,
                                             line_f_bytes, p_result);
              mark_subobject_initialized(line_f_bytes, obj_storage);

              a_field_ptr   column_fp = interp_inf.column_field;
              a_byte_count  column_f_offset;
              get_mapped_byte_count(&persistent_map, column_fp,
                                    column_f_offset);
              a_byte        *column_f_bytes = obj_storage + column_f_offset;
              do_constexpr_write_source_column(ips, use_pos, column_fp->type,
                                               column_f_bytes, p_result);
              mark_subobject_initialized(column_f_bytes, obj_storage);

              /* Update the pointer to point to the allocated object. */
              clear_address(result_storage, obj_storage);
              mark_complete_object_initialized(result_storage);
            }  /* if */
          }  /* if */
        }  /* if */
      }
      break;
    case bfk_COLUMN:
    case bfk_LINE:
    case bfk_FILE:
    case bfk_FUNCTION:
      /* Handle source location intrinsics. */
      interpreted = FALSE;
      if (args != NULL) {
        unexpected_condition();
      } else {
        if (do_constexpr_builtin_source_pos_func(ips, callee, result_storage,
                                                 p_result)) {
          interpreted = TRUE;
        } else {
          do_constexpr_fail(*p_result);
        }  /* if */
      }  /* if */
      break;
    case bfk_assume:
      /* The argument to __builtin_assume is not evaluated. */
      if (args == NULL || args->next != NULL) {
        unexpected_condition();
      } else {
        interpreted = TRUE;
        if (ips->curr_call_frame == NULL &&
            !ips->is_constant_evaluated) {
          /* Do not fold a __builtin_assume call inside a function unless a
             constant-expression is really needed.  Otherwise, the
             __builtin_assume call might become invisible to the back end. */
          do_constexpr_fail(*p_result);
        }  /* if */
      }  /* if */
      break;
    default:
      interpreted = FALSE;
  }  /* switch */
  return interpreted;
}  /* do_constexpr_builtin_function */

#endif /* BUILTIN_FUNCTIONS_ENABLED */

static a_boolean run_function_body(an_interpreter_state  *ips,
                                   a_scope_ptr           callee_scope)
/*
The given scope is a function scope.  Execute its associated compound
statement.
*/
{
  a_statement_ptr  block_stmt = callee_scope->assoc_block;

  if (block_stmt->kind == (a_statement_kind)stmk_try_block) {
    block_stmt = block_stmt->variant.try_block->statement;
  }  /* if */
  return do_constexpr_block_statement(ips, block_stmt, callee_scope);
}  /* run_function_body */


static a_boolean do_constexpr_std_is_constant_evaluated(
                                   an_interpreter_state        *ips,
                                   a_routine_ptr               callee,
                                   ARG_UNUSED an_expr_node_ptr call_node,
                                   ARG_UNUSED a_byte           **p_arg_bytes,
                                   a_byte                      *result_storage,
                                   ARG_UNUSED a_byte           *complete_obj)
/*
Return TRUE and set *result_storage to "true" if ips->is_constant_evaluated is
TRUE.  Otherwise result FALSE.
*/
{
  a_boolean result = TRUE;

  if (ips->is_constant_evaluated) {
    *(an_integer_value*)result_storage = one_int;
    if (ips->curr_call_frame->parent == NULL &&
        !ips->is_variable_initializer) {
      warn_about_is_constant_evaluated(callee, call_node);
    }  /* if */
  } else {
    do_constexpr_fail(result);
  }  /* if */
  return result;
}  /* do_constexpr_std_is_constant_evaluated */


static void report_leftover_allocations(an_interpreter_state  *ips)
/*
Interpretation is mostly completed and the caller has determined that some
dynamic allocations have not been freed.  Record a diagnostic that explains
the missing allocation.
*/
{
  uint32_t  count = 0;
  a_constexpr_allocation_ptr
            alloc = ips->dyn_allocations;

  for (; alloc != NULL; alloc = alloc->next) ++count;
  if (count > 0) {
    info_with_pos_num(ec_constexpr_leftover_allocations,
                      &ips->position, count, ips);
    info_with_pos(ec_constexpr_allocation_pos, &ips->dyn_allocations->pos,
                  ips);
  }  /* if */
}  /* report_leftover_allocations */


static a_constexpr_allocation_ptr do_constexpr_dynamic_alloc(
                                           an_interpreter_state  *ips,
                                           a_type_ptr            elem_tp,
                                           a_byte_count          alloc_length,
                                           a_source_position     *diag_pos,
                                           a_constexpr_address   *cap,
                                           a_byte_count          *p_elem_size)
/*
Allocate alloc_length consecutive objects of type elem_tp on the interpreter's
dynamic allocation heap and place the result in *cap.  Return a pointer to the
complete allocation structure if successful, and NULL otherwise (in which case,
a diagnostic is registered in *ips for the given position).  If successful,
also return the interpreter size of the allocated elements in *p_elem_size.
*/
{
  a_boolean            result = TRUE;
  a_type_ptr           orig_elem_tp = skip_typerefs(elem_tp);
  a_byte_count         header_size, prefix_size, bitmap_size, elem_size,
                       total_size, orig_alloc_length = alloc_length;
  a_byte               *block;
  an_alloc_seq_number  alloc_seq_number;
  a_constexpr_allocation_ptr
                       allocation = NULL;

  elem_tp = orig_elem_tp;
  if (type_is(elem_tp, tk_array)) {
    /* Adjust the number of elements for the array type. */
    do {
      a_targ_size_t  dim;
      if (elem_tp->variant.array.is_template_dependent_size_array) {
        info_with_pos(ec_constexpr_dependent_array_size, diag_pos, ips);
        do_constexpr_fail(result);
        goto done;
      }  /* if */
      dim = elem_tp->variant.array.variant.number_of_elements;
      if (dim == 0) {
        if (elem_tp->incomplete) {
          info_with_pos(ec_constexpr_access_to_runtime_storage, diag_pos, ips);
          do_constexpr_fail(result);
          goto done;
        }  /* if */
      } else if ((a_byte_count)(MAX_CONSTEXPR_DYN_ALLOC_SIZE/dim) <
                                                               alloc_length) {
        info_with_pos(ec_constexpr_allocation_too_large, diag_pos, ips);
        do_constexpr_fail(result);
        goto done;
      }  /* if */
      alloc_length = (a_byte_count)(alloc_length*dim);
      elem_tp = skip_typerefs(elem_tp->variant.array.element_type);
    } while (type_is(elem_tp, tk_array));
  }  /* if */
  elem_size = value_bytes_for_type(ips, elem_tp, &result); 
  if (!result) goto done;
  if (elem_size != 0 &&
      MAX_CONSTEXPR_DYN_ALLOC_SIZE/elem_size < alloc_length) {
    info_with_pos(ec_constexpr_allocation_too_large, diag_pos, ips);
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  total_size = alloc_length*elem_size;
  do_host_alignment(total_size);
  prefix_size = sizeof(a_constexpr_allocation);
  do_host_alignment(prefix_size);
  header_size = prefix_size;
  /* Add the size of the bookkeeping prefix as if this were an array (see also
     compute_prefix_size_for_type). */
  bitmap_size = compute_bitmap_size(total_size);
  prefix_size += bitmap_size+1+sizeof(a_type_ptr);
  do_host_alignment(prefix_size);
  total_size += prefix_size;
  block = (a_byte*)malloc_for_interpreter(total_size);
  if (block == NULL) {
    info_with_pos(ec_constexpr_allocation_too_large, diag_pos, ips);
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  /* Clear the bookkeeping prefix. */
  memzero(block+header_size, prefix_size-header_size);
  allocation = (a_constexpr_allocation_ptr)block;
  alloc_seq_number = ++ips->curr_alloc_seq_number;
  allocation->alloc_seq_number = alloc_seq_number;
  add_to_live_set(&ips->live_set, alloc_seq_number);
  allocation->elem_type = elem_tp;
  allocation->pos = *diag_pos;
  allocation->total_size = total_size;
  allocation->prefix_size = prefix_size;
  allocation->length = alloc_length;
  allocation->next = ips->dyn_allocations;
  allocation->prev = NULL;
  if (ips->dyn_allocations != NULL) {
    ips->dyn_allocations->prev = allocation;
  }  /* if */
  ips->dyn_allocations = allocation;
  clear_address(cap, block+prefix_size);
  cap->variant.base_address = cap->address;
  record_complete_object_type(orig_elem_tp, cap->complete_object);
  if (alloc_length != 1) {
    cap->flags |= CA_ARRAY_ELEMENT;
    cap->length = orig_alloc_length;
    if (alloc_length == 0) {
      cap->flags |= CA_CANNOT_DEREFERENCE;
    }  /* if */
  }  /* if */
  cap->alloc_seq_number = alloc_seq_number;
  *p_elem_size = elem_size;
  *(cap->complete_object-sizeof(a_type_ptr)-1) |= COMPLETE_OBJ_DYN_ALLOC;
done:
  return allocation;
}  /* do_constexpr_dynamic_alloc */


static a_boolean do_constexpr_std_allocator_allocate(
                                        an_interpreter_state  *ips,
                                        a_routine_ptr         callee,
                                        an_expr_node_ptr      call_node,
                                        a_byte                **p_arg_bytes,
                                        a_byte                *result_storage,
                                        ARG_UNUSED a_byte     *complete_obj)
/*
Interpret a call (represented by call_node) to std::allocator<T>::allocate
(represented by callee).  This ignores the actual definition of that function
(which is likely not constexpr-friendly) and instead allocates storage in the
interpreter's domain (updating *ips as needed).  *p_arg_bytes points to the
already-evaluated argument of the call.  result_storage/complete_obj indicates
where the result should be stored.
*/
{
  a_boolean             result = TRUE, ovflo;
  an_expr_node_ptr      callee_node, size_arg;
  a_type_ptr            size_tp, allocator_tp;
  a_template_arg_ptr    tap;
  a_host_large_integer  alloc_length;
  a_byte_count          elem_size;

  callee_node = call_node->variant.operation.operands;
  if (callee_node == NULL || callee_node->next == NULL ||
      callee_node->next->next == NULL) {
    do_constexpr_fail(result);
    info_with_pos_sym_type(ec_constexpr_invalid_intrinsic_signature,
                           &call_node->position, symbol_for(callee),
                           callee->type, ips);
    goto done;
  }  /* if */
  /* Skip the callee and selector nodes. */
  size_arg = callee_node->next->next;
  size_tp = skip_typerefs(size_arg->type);
  if (size_tp->kind != (a_type_kind)tk_integer) {
    do_constexpr_fail(result);
    info_with_pos_sym_type(ec_constexpr_invalid_intrinsic_signature,
                           &call_node->position, symbol_for(callee),
                           callee->type, ips);
    goto done;
  }  /* if */
  get_int_val_from(p_arg_bytes[1], size_tp, alloc_length, ovflo);
  if (ovflo || alloc_length < 0 || alloc_length > MAX_ARRAY_LENGTH) {
    info_with_pos_num(ec_constexpr_alloc_too_large, &call_node->position,
                      (a_byte_count)alloc_length, ips);
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  check_assertion(callee->source_corresp.is_class_member);
  allocator_tp = parent_class_of(callee);
  tap = class_type_supp(allocator_tp)->template_arg_list;
  if (tap == NULL || tap->kind != (a_templ_arg_kind)tak_type) {
    do_constexpr_fail(result);
    info_with_pos_sym_type(ec_constexpr_invalid_intrinsic_signature,
                           &call_node->position, symbol_for(callee),
                           callee->type, ips);
    goto done;
  }  /* if */
  if (do_constexpr_dynamic_alloc(ips, tap->variant.type,
                                 (a_byte_count)alloc_length,
                                 &call_node->position,
                                 (a_constexpr_address*)result_storage,
                                 &elem_size) == NULL) {
    do_constexpr_fail(result);
    goto done;
  }  /* if */
done:
  return result;
}  /* do_constexpr_std_allocator_allocate */


static a_constexpr_allocation_ptr find_constexpr_allocation(
                                             an_interpreter_state  *ips,
                                             a_byte                *obj_bytes,
                                             a_source_position     *diag_pos)
/*
obj_bytes is presumed to be a pointer to the top-level object allocated with
do_constexpr_dynamic_alloc.  Find the associated allocation and return it.  If
there is none, record a diagnostic in ips for the given position and return
NULL.
*/
{
  a_constexpr_allocation  *allocation;

  for (allocation = ips->dyn_allocations;
       allocation != NULL;
       allocation = allocation->next) {
    if (obj_bytes == (a_byte*)allocation+allocation->prefix_size) {
      break;
    }  /* if */
  }  /* for */
  if (allocation == NULL) {
    /* The pointer argument doesn't appear to correspond to an earlier
       allocation. */
    info_with_pos(ec_constexpr_bad_deallocation, diag_pos, ips);
  }  /* if */
  return allocation;
}  /* find_constexpr_allocation */


static void free_allocation(an_interpreter_state    *ips,
                            a_constexpr_allocation  *allocation)
/*
Release the given allocation.
*/
{
  remove_from_live_set(&ips->live_set, allocation->alloc_seq_number);
  if (allocation->prev == NULL) {
    ips->dyn_allocations = allocation->next;
  } else {
    allocation->prev->next = allocation->next;
  }  /* if */
  if (allocation->next != NULL) {
    allocation->next->prev = allocation->prev;
  }  /* if */
  free_for_interpreter((a_byte*)allocation, (sizeof_t)allocation->total_size);
}  /* free_constexpr_allocation */


static a_boolean do_constexpr_std_allocator_deallocate(
                                        an_interpreter_state  *ips,
                                        a_routine_ptr         callee,
                                        an_expr_node_ptr      call_node,
                                        a_byte                **p_arg_bytes,
                                        ARG_UNUSED a_byte     *result_storage,
                                        ARG_UNUSED a_byte     *complete_obj)
/*
Interpret a call (represented by call_node) to std::allocator<T>::deallocate
(represented by callee).  This ignores the actual definition of that function
(which is likely not constexpr-friendly) and instead deallocates storage in the
interpreter's domain (updating *ips as needed).  *p_arg_bytes points to the
already-evaluated arguments of the call.
*/
{
  a_boolean             result = TRUE;
  an_expr_node_ptr      callee_node, ptr_arg, size_arg;
  a_type_ptr            ptr_tp, size_tp, elem_tp, allocator_tp, result_type;
  a_template_arg_ptr    tap;
  a_host_large_integer  alloc_length;
  a_boolean             ovflo;
  a_byte_count          elem_size, total_size, orig_data_size;
  a_constexpr_address   *cap;
  a_constexpr_allocation_ptr
                        allocation;

  callee_node = call_node->variant.operation.operands;
  if (callee_node == NULL || callee_node->next == NULL ||
      callee_node->next->next == NULL ||
      callee_node->next->next->next == NULL) {
    /* Three "arguments": (1) the selector (allocator), (2) the pointer to
       allocator, and (3) the size to deallocate. */
    goto done;
  }  /* if */
  ptr_arg = callee_node->next->next;
  ptr_tp = skip_typerefs(ptr_arg->type);
  if (!type_is(ptr_tp, tk_pointer)) {
    do_constexpr_fail(result);
    info_with_pos_sym_type(ec_constexpr_invalid_intrinsic_signature,
                           &call_node->position, symbol_for(callee),
                           callee->type, ips);
    goto done;
  }  /* if */
  cap = (a_constexpr_address*)p_arg_bytes[1];
  allocation = find_constexpr_allocation(ips, cap->address,
                                         &call_node->position);
  if (allocation == NULL) {
    result = FALSE;
    goto done;
  }  /* if */
  size_arg = ptr_arg->next;
  size_tp = skip_typerefs(size_arg->type);
  result_type = skip_typerefs(call_node->type);
  if (!type_is(size_tp, tk_integer) || !type_is(result_type, tk_void)) {
    do_constexpr_fail(result);
    info_with_pos_sym_type(ec_constexpr_invalid_intrinsic_signature,
                           &call_node->position, symbol_for(callee),
                           callee->type, ips);
    goto done;
  }  /* if */
  get_int_val_from(p_arg_bytes[2], size_tp, alloc_length, ovflo);
  if (ovflo || alloc_length < 0 || alloc_length > MAX_ARRAY_LENGTH) {
    info_with_pos_num(ec_constexpr_alloc_too_large, &call_node->position,
                      (a_byte_count)alloc_length, ips);
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  check_assertion(callee->source_corresp.is_class_member);
  allocator_tp = parent_class_of(callee);
  tap = class_type_supp(allocator_tp)->template_arg_list;
  if (tap == NULL || tap->kind != (a_templ_arg_kind)tak_type) {
    do_constexpr_fail(result);
    info_with_pos_sym_type(ec_constexpr_invalid_intrinsic_signature,
                           &call_node->position, symbol_for(callee),
                           callee->type, ips);
    goto done;
  }  /* if */
  elem_tp = skip_typerefs(tap->variant.type);
  if (!same_entities(allocation->elem_type, elem_tp)) {
    info_with_pos_type2(ec_constexpr_bad_deallocation_type,
                        &call_node->position, elem_tp, allocation->elem_type,
                        ips);
    info_with_pos(ec_constexpr_allocation_pos, &allocation->pos, ips);
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  elem_tp = skip_typerefs(elem_tp);
  elem_size = value_bytes_for_type(ips, elem_tp, &result); 
  if (!result) goto done;
  if (elem_size != 0 &&
      MAX_CONSTEXPR_DYN_ALLOC_SIZE/elem_size < (a_byte_count)alloc_length) {
    info_with_pos(ec_constexpr_allocation_too_large, &call_node->position,
                  ips);
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  total_size = (a_byte_count)(alloc_length*elem_size);
  do_host_alignment(total_size);
  orig_data_size = allocation->total_size - allocation->prefix_size;
  if (total_size != orig_data_size) {
    info_with_pos_num2(ec_constexpr_bad_deallocation_size,
                       &call_node->position, (a_byte_count)alloc_length,
                       elem_size == 0 ? 0 : orig_data_size/elem_size, ips);
    info_with_pos(ec_constexpr_allocation_pos, &allocation->pos, ips);
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  free_allocation(ips, allocation);
done:
  return result;
}  /* do_constexpr_std_allocator_deallocate */


static a_byte
		*valid_placement_new_address;
			/* An address for which a placement-new is
			   acceptable. */

static a_type_ptr
		valid_placement_new_type;
			/* The type of the object stored at the
			   valid_placement_new_address.  NULL if no address
			   is acceptable. */


static a_boolean do_constexpr_std_construct_at(
                                   an_interpreter_state        *ips,
                                   a_routine_ptr               callee,
                                   ARG_UNUSED an_expr_node_ptr call_node,
                                   a_byte                      **p_arg_bytes,
                                   ARG_UNUSED a_byte           *result_storage,
                                   ARG_UNUSED a_byte           *complete_obj)
/*
Execute a call to:
	std::construct_at<T>(T* location, Args&&... args)
See do_constexpr_std_allocator_allocate for the meaning of the parameters.
*/
{
  /* The definition of construct_at relies on "placement new", which is usually
     a problem for constexpr evaluation because placement-new takes a void*,
     which we don't know to be valid in general.  However, in this case we know
     it's valid because it comes from the parameter "location" of the right
     type.  So we temporarily enable placement-new here, and record the address
     at which the object will be constructed.  do_constexpr_new then makes use
     of that information. */
  a_boolean            result;
  a_template_arg_ptr   tap = callee->template_arg_list;
  a_constexpr_address  *cap = (a_constexpr_address*)p_arg_bytes[0];

  check_assertion(valid_placement_new_type == NULL &&
                  tap != NULL && tap->kind == (a_templ_arg_kind)tak_type);
  if (is_variant_path(cap) &&
      !check_variant_assign(ips, cap, &call_node->position)) {
    /* Invalid attempt to store into a non-active variant field. */
    result = FALSE;
  } else {
    valid_placement_new_address = cap->address;
    valid_placement_new_type = skip_typerefs(tap->variant.type);
    result = run_function_body(ips, scope_for_routine(callee));
    valid_placement_new_type = NULL;
  }  /* if */
  return result;
}  /* do_constexpr_std_allocator_construct_at */


static a_boolean do_constexpr_std_destroy_at(
                                   an_interpreter_state        *ips,
                                   a_routine_ptr               callee,
                                   ARG_UNUSED an_expr_node_ptr call_node,
                                   ARG_UNUSED a_byte           **p_arg_bytes,
                                   ARG_UNUSED a_byte           *result_storage,
                                   ARG_UNUSED a_byte           *complete_obj)
/*
Execute a call to std::destroy_at.  See do_constexpr_std_allocator_allocate
for the meaning of the parameters.
*/
{
  a_boolean  result = TRUE;

  result = run_function_body(ips, scope_for_routine(callee));
  return result;
}  /* do_constexpr_std_destroy_at */


static a_boolean do_constexpr_std_report_constexpr_value(
                                   an_interpreter_state        *ips,
                                   a_routine_ptr               callee,
                                   ARG_UNUSED an_expr_node_ptr call_node,
                                   a_byte                      **p_arg_bytes,
                                   ARG_UNUSED a_byte           *result_storage,
                                   ARG_UNUSED a_byte           *complete_obj)
/*
call_node represents a call to std::__report_constexpr_value, which is an
overloaded function with one of the following signatures:

  void __report_constexpr_value(<integer-type>);
  void __report_constexpr_value(const char*);
  void __report_constexpr_value(const char*, int length);

If the first alternative is called, print out the given value in decimal form.
If the second alternative is called, print out the null-terminated string value
pointed to by the first argument.  The third alternative is like the second,
except no more than length characters are output.  Return FALSE in case of a
serious issue.  

See do_constexpr_std_allocator_allocate for the meaning of the parameters.
*/
{
  a_boolean         result = TRUE;
  a_type_ptr        rtp = skip_typerefs(callee->type);
  a_param_type_ptr  ptp = function_type_params(rtp);

  if (!ips->is_constant_evaluated) {
    /* Don't generate output if a constant is not needed, but fail evaluation
       in that case.  That avoids repeated output due to the front end trying
       to fold the same sub-expression multiple times (when it doesn't really
       need to). */
    result = FALSE;
    goto done;
  }  /* if */
  if (!ips->report_started) {
    a_const_char   *full_name, *diag_file_name;
    a_line_number  line_number;
    a_boolean      at_end_of_source;
    (void)conv_seq_to_file_and_line(ips->position.seq, &diag_file_name,
                                    &full_name, &line_number,
                                    &at_end_of_source);
    fprintf(f_error, "\n%s\n", error_text(ec_constexpr_begin_report));
    if (line_number != 0) {
      fprintf(f_error, "%s%lu%s%s\n",
              error_text(ec_at_line), (unsigned long)line_number,
              error_text(ec_of), diag_file_name);
    }  /* if */
    ips->report_started = TRUE;
  }  /* if */
  if (is_integral_type(ptp->type)) {
    a_boolean             is_signed = is_signed_integral_type(ptp->type);
    a_host_large_integer  val;
    a_boolean             ovflo;
    a_byte                *val_bytes = p_arg_bytes[0];
    conv_integer_value_to_host_large_integer(
                     (an_integer_value *)val_bytes, is_signed, &val, &ovflo);
    if (ovflo) {
      fprintf(f_error, "(overflow)");
    } else if (is_signed) {
      fprintf(f_error, "%lld", (long long)val);
    } else {
      fprintf(f_error, "%llu", (unsigned long long)(long long)val);
    }  /* if */
  } else {
    /* A sequence of characters.  The first argument is a pointer to the
       string.  There may be a second argument indicating the number of
       characters to output. */
    a_constexpr_address  *cap = (a_constexpr_address*)p_arg_bytes[0];
    if (is_array_element(cap) && !cannot_dereference(cap)) {
      an_integer_value      *p_val = (an_integer_value*)cap->address;
      a_type_ptr            elem_type = type_pointed_to(ptp->type);
      a_byte_count          elem_size, pos, len;
      a_host_large_integer  char_val;
      a_boolean             ovflo;
      elem_type = skip_typerefs(elem_type);
      get_array_pos(ips, cap, elem_type, &len, &pos, &elem_size,
                    &result);
      if (ptp->next != NULL) {
        /* There is a second parameter indicating the number of characters
           to output. */
        an_integer_value      *p_len = (an_integer_value*)p_arg_bytes[1];
        a_host_large_integer  max_len;
        conv_integer_value_to_host_large_integer(
                     p_len, /*is_signed=*/TRUE, &max_len, &ovflo);
        if (ovflo) {
          /* Ignore the parameter. */
        } else if (max_len < 0) {
          len = 0;
        } else if ((a_byte_count)max_len < len-pos) {
          len = (a_byte_count)(max_len-pos);
        }  /* if */
      }  /* if */
      while (pos<len) {
        conv_integer_value_to_host_large_integer(
                p_val, is_signed_integral_type(elem_type)/*lint !e2666*/,
                &char_val, &ovflo);
        if (char_val == 0) break;
        fprintf(f_error, "%c", (char)char_val);
        ++pos;
        ++p_val;
      }  /* while */
    } else {
      fprintf(f_error, "(invalid string pointer)");
    }  /* if */
  }  /* if */
done:
  return result;
}  /* do_constexpr_std_report_constexpr_value */


typedef a_boolean (*an_intrinsic_evaluator)(
                                        an_interpreter_state  *ips,
                                        a_routine_ptr         callee,
                                        an_expr_node_ptr      call_node,
                                        a_byte                **p_arg_bytes,
                                        a_byte                *result_storage,
                                        a_byte                *complete_obj);


static a_boolean do_constexpr_intrinsic_call(
                                        an_interpreter_state  *ips,
                                        a_routine_ptr         callee,
                                        an_expr_node_ptr      call_node,
                                        a_byte                **p_arg_bytes,
                                        a_byte                *result_storage,
                                        a_byte                *complete_obj)
/*
Evaluate the call represented by call_node, which is a call to the routine
represented by callee and that routine is handled in a special way by the
interpreter.  The arguments to the call have already been evaluated and their
interpreter representations are pointed to by p_arg_bytes[0 .. N-1] (where N
is the number of arguments).  The result of the call will be placed in storage
pointed to by result_storage, which is part of the complete object pointed to
by complete_obj.

The caller has already pushed a call frame and is responsible for popping that
frame when the call has completed.
*/
{
  a_boolean               result = TRUE;
  an_intrinsic_evaluator  evaluator;

  /* Dispatch the call to the appropriate implementation. */
  switch (callee->number.constexpr_intrinsic) {
    case cit_std_is_constant_evaluated:
      evaluator = do_constexpr_std_is_constant_evaluated;
      break;
    case cit_std_allocator_allocate:
      evaluator = do_constexpr_std_allocator_allocate;
      break;
    case cit_std_allocator_deallocate:
      evaluator = do_constexpr_std_allocator_deallocate;
      break;
    case cit_std_construct_at:
      evaluator = do_constexpr_std_construct_at;
      break;
    case cit_std_destroy_at:
      evaluator = do_constexpr_std_destroy_at;
      break;
    case cit_std_report_constexpr_value:
      evaluator = do_constexpr_std_report_constexpr_value;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  result = evaluator(ips, callee, call_node, p_arg_bytes,
                     result_storage, complete_obj);
  return result;
}  /* do_constexpr_intrinsic_call */


static a_boolean adjust_this_address(an_interpreter_state    *ips,
                                     a_constexpr_address     *this_addr,
                                     a_constexpr_ptr_to_mem  *pm_value,
                                     a_type_ptr              selector_type,
                                     an_expr_node_ptr        expr)
/*
this_addr is the address of an object which is accessed (via a pointer or
lvalue of type selector_type) through the given pointer-to-member value.
Adjust the address if needed.  If the adjustment is invalid (because the
complete object does not contain the accessed subobject) return FALSE and
record a diagnostic in the interpreter state (using the position of expr).
*/
{
  a_boolean  result = TRUE;

  if (pm_value->this_class_adjustment != 0) {
    if (pm_value->subtract_adjustment) {
      /* A derived-member access.  Check that it is valid. */
      a_base_class_ptr  bcp = *(a_base_class_ptr*)this_addr->address;
      a_type_ptr        derived_class;
      a_symbol_ptr      mem_sym;
      if (pm_value->is_ptr_to_mem_function) {
        mem_sym = symbol_for(pm_value->variant.routine);
      } else {
        mem_sym = symbol_for(pm_value->variant.field);
      }  /* if */
      if (bcp == NULL) {
        if (selector_type->kind == (a_type_kind)tk_pointer) {
          derived_class =
              skip_typerefs(selector_type->variant.pointer.type);
        } else {
          derived_class = selector_type;
        }  /* if */
      } else {
        a_derivation_step_ptr  dsp = bcp->derivation->path;
        a_type_ptr             ptp;
        ptp = sym_parent_class(mem_sym);
        derived_class = bcp->derived_class;
        if (derived_class == ptp) {
          derived_class = NULL;
        } else {
          for (; dsp->base_class != bcp; dsp = dsp->next) {
            if (ptp == skip_typerefs(dsp->base_class->type)) {
              /* A derived subobject contains the field. */
              derived_class = NULL;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
      if (derived_class != NULL) {
        /* An invalid access. */
        do_constexpr_fail(result);
        info_with_pos_sym_type(ec_constexpr_invalid_pm_access, &expr->position,
                               mem_sym, derived_class, ips);
      }  /* if */
      this_addr->address -= pm_value->this_class_adjustment;
    } else {
      /* A base-class member access: Type checking in the front end ensures
         that it is valid. */
      this_addr->address += pm_value->this_class_adjustment;
    }  /* if */
  }  /* if */
  return result;
}  /* adjust_this_address */


static a_boolean eval_selector_arg(an_interpreter_state  *ips,
                                   an_expr_node_ptr      arg,
                                   a_type_ptr            tp,
                                   a_byte                *this_bytes)
/*
Evaluate the given expression (of the given type, excluding typerefs) to
produce a "this" pointer value.  The result is stored at the location indicated
by this_bytes.
*/
{
  a_boolean  result = TRUE;

  if (arg->is_lvalue || arg->is_xvalue ||
      tp->kind == (a_type_kind)tk_pointer) {
    /* The usual case: An address is produced. */
    if (!do_constexpr_expression(ips, arg, this_bytes, this_bytes)) {
      do_constexpr_fail(result);
      goto done;
    }  /* if */
  } else {
    /* The call is on a class rvalue.  E.g., "X().f();". */
    a_byte_count  n_bytes = value_bytes_for_type(ips, tp, &result);
    a_byte        *class_bytes;
    if (!result) goto done;
    alloc_complete_object(ips, n_bytes, tp, class_bytes);
    if (!do_constexpr_expression(ips, arg, class_bytes, class_bytes)) {
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    mark_complete_object_initialized(class_bytes);
    /* Store the address of the class in *this_bytes. */
    set_active_address(ips, (a_constexpr_address*)this_bytes,
                       class_bytes, class_bytes);
  }  /* if */
  mark_complete_object_initialized(this_bytes);
done:
  return result;
}  /* eval_selector_arg */


static a_boolean adjust_virtual_callee(a_routine_ptr         *p_callee,
                                       a_byte                **p_this_arg,
                                       a_byte_count          *p_retval_offset)
/*
The front end has evaluated the arguments to a virtual call with the
statically-resolved callee indicated by *p_callee.  Update *p_callee to be the
overriding virtual function in the subobject referred to by *p_this_arg and
update *p_this_arg accordingly (i.e., to refer to the subobject associated with
the overriding member).  If the overriding function has a covariant return type
with respect to the statically-resolved callee, add to *p_retval_offset the
adjustment that will have to be made to the address returned by the call (to
translate the dynamically returned address back to the statically resolved
type).  Returns FALSE if *p_callee is not a constant expression (for example,
if *p_this_arg is not statically initialized.)
*/
{
  a_constexpr_address  **p_this_val = (a_constexpr_address**)p_this_arg,
                       *this_val = *p_this_val;
  a_byte               *subobj = this_val->address,
                       *complete_obj = this_val->complete_object;
  a_boolean            result = TRUE;

  if (is_runtime_data_address(this_val)) {
    result = FALSE;
  } else if (subobj == complete_obj) {
    /* We're already in the most-derived class: No adjustment is needed. */
  } else if (!subobject_is_initialized(subobj, complete_obj)) {
    /* The current subobject is not initialized (presumably because the
       constructor has not completely run yet, or because the object has
       been partially destroyed). */
  } else {
    a_routine_ptr     callee = *p_callee;
    do {
      a_base_class_ptr  bcp = *(a_base_class_ptr*)subobj;
      a_byte_count      offset;
      an_overriding_virtual_function_ptr
                        ovfp;
      if (bcp == NULL) {
        /* A most derived subobject (i.e., a field or array element).  We are
           done. */
        break;
      }  /* if */
      check_assertion(!bcp->is_pack_expansion);
      ovfp = bcp->variant.overriding_virtual_functions;
      for (; ovfp != NULL; ovfp = ovfp->next) {
        if (ovfp->primary_function == callee) {
          a_base_class_ptr  ret_base = ovfp->return_adjustment_base_class;
          a_byte_count      retval_offset_step;
          if (ret_base != NULL) {
            /* A covariant return override.  The return value will need
               adjustment. */
            get_mapped_byte_count(&persistent_map, bcp, retval_offset_step);
            *p_retval_offset += retval_offset_step;
          }  /* if */
          callee = ovfp->overriding_function;
          break;
        }  /* if */
      }  /* for */
      get_mapped_byte_count(&persistent_map, bcp, offset);
      subobj -= offset;
      if (!subobject_is_initialized(subobj, complete_obj)) {
        /* The next-enclosing subobject is not constructed.  Do not dispatch
           from it. */
        break;
      }  /* if */
    } while (subobj != complete_obj);
    *p_callee = callee;
    this_val->address = subobj;
  }  /* if */
  return result;
}  /* adjust_virtual_callee */


static a_routine_ptr eval_constexpr_callee(
                         an_interpreter_state    *ips,
                         an_expr_node_ptr        call_node,
                         a_constexpr_ptr_to_mem  **p_pm_target,
                         a_byte                  **p_pre_evaluated_this_bytes)
/*
Interpret the given call node until the call target is determined, except for
the effects of virtual dispatch.  If successful, return the IL entry for the
callee; otherwise return NULL and update *ips with the reason for the failure.
If the call is through a pointer-to-member-function, allocate and store in
*p_pm_target the computed value of that pointer-to-member-function;
furthermore, if the "this" value had to be computed, allocate and store that
in *p_pre_evaluated_this_bytes (needed because in "(f().*pm())()" f() must be
evaluated before pm() in C++17 mode).
*/
{
  a_routine_ptr     callee = NULL;
  an_expr_node_ptr  callee_node = call_node->variant.operation.operands;

  if (is_routine_node(callee_node)) {
    callee = node_routine(callee_node);
  } else if (node_operator_is(call_node, eok_dot_pm_call) ||
             node_operator_is(call_node, eok_points_to_pm_call)) {
    /* A call through a pointer-to-member function.  We'll determine the
       callee here, and adjust the "this" pointer later on. */
    a_type_ptr    pm_type = skip_typerefs(callee_node->type);
    a_byte        *pm_bytes;
    a_boolean     result = TRUE;
    a_byte_count  n_pm_bytes = value_bytes_for_type(ips, pm_type, &result);
    alloc_complete_object(ips, n_pm_bytes, pm_type, pm_bytes);
    *p_pm_target = (a_constexpr_ptr_to_mem*)pm_bytes;
    if (call_node->variant.operation.eval_left_to_right) {
      /* As of C++17, in a call like (f().*pm())() the sub-expression f() must
         be evaluated before the sub-expression pm().  We therefore evaluate
         that expression now, and copy the result (which is a constexpr address
         and therefore safe to copy) to the expected location of the "this"
         pointer value later on. */
      an_expr_node_ptr  selector_arg = callee_node->next;
      a_type_ptr        tp = skip_typerefs(selector_arg->type);
      a_byte_count      this_n_bytes = sizeof(a_constexpr_address);
      do_host_alignment(this_n_bytes);
      alloc_complete_object(ips, this_n_bytes, generic_ptr_type,
                            *p_pre_evaluated_this_bytes);
      if (!eval_selector_arg(ips, selector_arg, tp,
                             *p_pre_evaluated_this_bytes)) {
        goto done;
      }  /* if */
    }  /* if */
    if (do_constexpr_expression(ips, callee_node, pm_bytes, pm_bytes)) {
      callee = (*p_pm_target)->variant.routine;
      if (callee == NULL) {
        info_with_pos(ec_constexpr_null_callee, &callee_node->position, ips);
      }  /* if */
    }  /* if */
  } else {
    /* An indirect call. */
    a_byte  *addr_bytes;
    alloc_complete_object(ips, sizeof(a_constexpr_address), generic_ptr_type,
                          addr_bytes);
    if (do_constexpr_expression(ips, callee_node, addr_bytes, addr_bytes)) {
      a_constexpr_address  *addr = (a_constexpr_address*)addr_bytes;
      if (is_function_address(addr)) {
        callee = addr->variant.routine;
        if (callee == NULL) {
          info_with_pos(ec_constexpr_null_callee, &callee_node->position, ips);
        }  /* if */
      } else if (addr->address == NULL) {
        info_with_pos(ec_constexpr_null_callee, &callee_node->position, ips);
      } else {
        unexpected_condition();
      }  /* if */
    }  /* if */
  }  /* if */
done:
  return callee;
}  /* eval_constexpr_callee */


static a_boolean do_constexpr_call(an_interpreter_state  *ips,
                                   an_expr_node_ptr      call_node,
                                   a_byte                *result_storage,
                                   a_byte                *complete_object)
/*
Interpret the given call node and place the result at the given storage (which
is part of the given complete object).  Return TRUE if no error occurred;
otherwise, return FALSE and update *ips accordingly.
*/
{
  an_expr_node_ptr  callee_node, arg;
  a_routine_ptr     callee = NULL;
  a_boolean         result = TRUE, lambda_entry_case;
  a_constexpr_ptr_to_mem
                    *pm_target = NULL;
  a_byte            *pre_evaluated_this_bytes = NULL;
  a_boolean          is_member_call = !node_operator_is(call_node, eok_call);

  /* First determine the actual callee. */
  callee = eval_constexpr_callee(ips, call_node, &pm_target,
                                &pre_evaluated_this_bytes);
  if (callee == NULL) {
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  /* Now interpret the call if possible. */
  /* First check the case of a built-in function that is handled specially by
     the interpreter. */
  {
#if GNU_EXTENSIONS_ALLOWED || BUILTIN_FUNCTIONS_ENABLED
    a_routine_ptr  eff_callee = callee;
#endif /* GNU_EXTENSIONS_ALLOWED || BUILTIN_FUNCTIONS_ENABLED */
#if GNU_EXTENSIONS_ALLOWED
    if (eff_callee->implicit_alias && !eff_callee->defined &&
        gnu_routine_supp(eff_callee)->aliased_routine != NULL) {
      /* Some functions are implicitly aliased to a built-in function. */
      eff_callee = gnu_routine_supp(eff_callee)->aliased_routine;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if BUILTIN_FUNCTIONS_ENABLED
    if (special_kind_is(eff_callee, sfk_none) &&
        eff_callee->variant.builtin_function_kind !=
                                          (a_builtin_function_kind)bfk_none &&
        do_constexpr_builtin_function(ips, eff_callee, call_node,
                                      result_storage, &result)) {
      ips->call_seen = TRUE;
      goto done;
    } else if (!result) {
      goto done;
    }  /* if */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  }
  if (special_kind_is(callee, sfk_destructor)) {
    /* An explicit call to a destructor is handled separately (among other
       things, it may require destroying subobjects). */
    an_expr_node_ptr     selector_arg = call_node->variant.operation.operands
                                                 ->next;
    a_type_ptr           tp = skip_typerefs(selector_arg->type);
    a_byte_count         this_n_bytes = sizeof(a_constexpr_address);
    a_constexpr_address  *cap;
    do_host_alignment(this_n_bytes);
    alloc_complete_object(ips, this_n_bytes, generic_ptr_type,
                          pre_evaluated_this_bytes);
    if (!eval_selector_arg(ips, selector_arg, tp,
                           pre_evaluated_this_bytes)) {
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    cap = (a_constexpr_address*)pre_evaluated_this_bytes;
    if (is_runtime_data_address(cap)) {
      info_with_pos(ec_constexpr_access_to_runtime_storage,
                    &selector_arg->position, ips);
      do_constexpr_fail(result);
      goto done;
    } else {
      result = do_constexpr_dtor(
                               ips, callee, &call_node->position,
                               cap->address, cap->complete_object,
                               !call_node->variant.operation.is_virtual_call);
    }  /* if */
    mark_subobject_uninitialized(cap->address, cap->complete_object);
    unmark_complete_object_initialized(cap->complete_object);
    goto done;
  } else if (special_kind_is(callee, sfk_lambda_entry_point)) {
    /* Use the real call operator instead of the entry point. */
    callee = callee->variant.lambda_call_operator;
    lambda_entry_case = TRUE;
  } else {
    lambda_entry_case = FALSE;
  }  /* if */
  if (callee->is_prototype_instantiation && !callee->is_lambda_body) {
    /* It's generally not worth attempting to evaluate a call to a prototype
       instantiation (it's not needed, and in most cases we'll run into a
       dependent construct that cannot be evaluated anyway).  However, we make
       an exception for lambdas in template-dependent contexts: They're always
       marked as prototype instantiations, but we might like to evaluate
       something like "[]{ return 42; }()" at compile time nonetheless. */
    info_with_pos_sym(ec_constexpr_call_not_interpretable,
                      &call_node->position, symbol_for(callee), ips);
    do_constexpr_fail(result);
  } else if (cost_exceeded(ips)) {
    more_info_diagnostic(ec_excessive_constexpr_complexity, &ips->position,
                         &ips->diag_list);
    do_constexpr_fail(result);
  } else {
    a_scope_ptr      callee_scope;
    a_call_frame     frame;
    a_variable_ptr   params, param, this_var;
    a_byte_count     n_args = 0, n_params, retval_offset = 0;
    a_byte_count     *arg_size;
    a_byte           *arg_ptrs, **p_arg_ptr, *arg_sizes, *closure_ptr = NULL;
    an_alloc_seq_number
                     alloc_seq_number;
    unsigned long    up_front_cost;
    a_boolean        eval_right_to_left =
                              call_node->variant.operation.eval_right_to_left;
    /* Account a relatively high cost for the call up-front, to limit the
       overall call depth.  When the call returns, that cost will be
       reduced. */
    up_front_cost = max_cost_constexpr_call/max_depth_constexpr_call+1;
    ips->cost += up_front_cost;
    /* Set up arguments, starting with "this" if applicable. */
    /* This process must happen in two phases.  First, the arguments must be
       allocated and evaluated.  Only then can we map parameter variables onto
       the allocated arguments.  We cannot do the two in a single loop because
       of recursive calls.  E.g.:
          constexpr void f(int p, int q) {
            ...
            f(x, p+q);
          }
       Here, if we remap p to the evaluation of "x" early, the subsequent
       evaluation of "p+q" will likely be wrong.  In support of the two-phase
       approach, we first allocate a buffer to keep pointers to each argument
       location determined in the first phase, so that we can remap them in
       the second phase.  We also allocate an additional buffer to keep track
       of the sizes of the argument objects, so we can quickly get to their
       variable postfix in the second phase. */
    callee_node = call_node->variant.operation.operands;
    for (arg = callee_node->next; arg != NULL; arg = arg->next) {
      n_args += 1;
    }  /* for */
    alloc_stack_bytes(ips, (a_byte_count)(n_args*sizeof(a_byte*)), arg_ptrs);
    alloc_stack_bytes(ips, (a_byte_count)(n_args*sizeof(a_byte_count)),
                                                                    arg_sizes);
    /* Phase 1: Allocate and evaluate the arguments. */
    p_arg_ptr = (a_byte**)arg_ptrs;
    arg_size = (a_byte_count*)arg_sizes;
    arg = callee_node->next;
    /* Evaluate the "this" pointer if needed. */
    if (is_member_call) {
      a_byte        *this_bytes;
      a_type_ptr    tp = skip_typerefs(arg->type);
      a_byte_count  this_n_bytes = sizeof(a_constexpr_address);
      do_host_alignment(this_n_bytes);
      *arg_size = this_n_bytes;
      arg_size += 1;
      this_n_bytes += sizeof(a_var_postfix);
      alloc_complete_object(ips, this_n_bytes, generic_ptr_type, this_bytes);
      *p_arg_ptr = this_bytes;
      p_arg_ptr += 1;
      if (eval_right_to_left) {
        /* Don't evaluate the operation just yet. */
      } else {
        if (pre_evaluated_this_bytes != NULL) {
          /* The selector argument was evaluated before the pointer-to-member
             expression above.  Just copy the resulting "this" value. */
          (void)memcpy(this_bytes, pre_evaluated_this_bytes,
                       sizeof(a_constexpr_address));
          mark_complete_object_initialized(this_bytes);
        } else if (!eval_selector_arg(ips, arg, tp, this_bytes)) {
          do_constexpr_fail(result);
          goto done;
        }  /* if */
        if (pm_target != NULL) {
          if (!adjust_this_address(ips, (a_constexpr_address*)this_bytes,
                                   pm_target, tp, call_node)) {
            do_constexpr_fail(result);
            goto done;
          }  /* if */
        }  /* if */
      }  /* if */
      arg = arg->next;
    }  /* if */
    for (; arg != NULL; arg = arg->next) {
      a_type_ptr    tp = skip_typerefs(arg->type);
      a_byte_count  n_bytes;
      a_byte        *arg_bytes;
      a_boolean     restore_lvalue = FALSE, restore_xvalue = FALSE;
      if (!(tp->kind == (a_type_kind)tk_pointer &&
            tp->variant.pointer.is_reference)) {
        /* When a class-type argument is passed by-value via a copy
           constructor call, the argument is left as an lvalue.  Temporarily
           set it back to an rvalue. */
        if (arg->is_lvalue) {
          restore_lvalue = TRUE;
          arg->is_lvalue = FALSE;
        } else if (arg->is_xvalue) {
          restore_xvalue = TRUE;
          arg->is_xvalue = FALSE;
        }  /* if */
      }  /* if */
      n_bytes = expr_result_size(ips, arg, tp, &result);
      do_host_alignment(n_bytes);
      *arg_size = n_bytes;
      arg_size += 1;
      n_bytes += sizeof(a_var_postfix);
      alloc_complete_object(ips, n_bytes, tp, arg_bytes);
      *p_arg_ptr = arg_bytes;
      p_arg_ptr += 1;
      /* Evaluate the argument, unless it is the first argument in a call for
         a right-to-left operator (which can only happen here if this is not a
         member call). */
      if (result &&
          (!eval_right_to_left || is_member_call || arg->next == NULL)) {
        Value_saver<a_source_position*>  src_pointer(&ips->srcloc_builtin_pos);

        /* Since non-default argument cases are folded as they're parsed, only
           the default argument case needs handled. */
        if (ips->srcloc_builtin_pos == NULL && arg->generated_default_arg) {
          ips->srcloc_builtin_pos = &call_node->position;
        }  /* if */
        if (!do_constexpr_expression(ips, arg, arg_bytes, arg_bytes)) {
          do_constexpr_fail(result);
        }  /* if */
        mark_complete_object_initialized(arg_bytes);
      }  /* if */
      if (restore_lvalue) {
        arg->is_lvalue = TRUE;
      } else if (restore_xvalue) {
        arg->is_xvalue = TRUE;
      }  /* if */
      if (!result) {
        goto done;
      }  /* if */
    }  /* for */
    if (eval_right_to_left) {
      /* Evaluate the first operand now that the second operand has been
         evaluated (the first operand was allocated but not evaluated
         earlier on). */
      a_byte            *arg_bytes = *(a_byte**)arg_ptrs;
      an_expr_node_ptr  first_arg = callee_node->next;
      a_type_ptr        tp = skip_typerefs(first_arg->type);
      if (is_member_call) {
        if (!eval_selector_arg(ips, first_arg, tp, arg_bytes) ||
            (pm_target != NULL &&
             !adjust_this_address(ips, (a_constexpr_address*)arg_bytes,
                                  pm_target, tp, call_node))) {
          do_constexpr_fail(result);
          goto done;
        }  /* if */
      } else {
        a_boolean  restore_lvalue = FALSE, restore_xvalue = FALSE;
        if (!(tp->kind == (a_type_kind)tk_pointer &&
              tp->variant.pointer.is_reference)) {
          /* When a class-type argument is passed by-value via a copy
             constructor call, the argument is left as an lvalue.  Temporarily
             set it back to an rvalue. */
          if (first_arg->is_lvalue) {
            restore_lvalue = TRUE;
            first_arg->is_lvalue = FALSE;
          } else if (first_arg->is_xvalue) {
            restore_xvalue = TRUE;
            first_arg->is_xvalue = FALSE;
          }  /* if */
        }  /* if */
        if (!do_constexpr_expression(ips, first_arg, arg_bytes, arg_bytes)) {
          do_constexpr_fail(result);
        } else {
          mark_complete_object_initialized(arg_bytes);
        }  /* if */
        if (restore_lvalue) {
          first_arg->is_lvalue = TRUE;
        } else if (restore_xvalue) {
          first_arg->is_xvalue = TRUE;
        }  /* if */
        if (!result) {
          goto done;
        }  /* if */
      }  /* if */
    }  /* if */
    /* If the function is virtual, we can now determine the actual callee. */
    if (callee->is_virtual &&
        (call_node->variant.operation.is_virtual_call || pm_target != NULL) &&
        !adjust_virtual_callee(&callee, (a_byte**)arg_ptrs, &retval_offset)) {
      info_with_pos(ec_constexpr_access_to_runtime_storage,
                    &callee_node->position, ips);
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    if (!callee->is_constexpr) {
      info_with_pos_sym(ec_constexpr_call_to_nonconstexpr_function,
                        &callee_node->position, symbol_for(callee), ips);
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    if (!callee->defined) {
      /* Attempt to instantiate the callee if possible. */
      if (callee->routine_fixup != NULL) {
        /* Nontemplate friends defined in class templates require special
           handling. */
        add_to_deferred_friend_function_fixup_list(callee->routine_fixup);
      } else if (load_routine_definition_from_module(callee)) {
        /* The definition was stored in a module file. */
      } else {
        set_instance_required(symbol_for(callee), TRUE, SIR_CONSTANT_CONTEXT);
        if (!callee->defined && callee->is_defaulted) {
          force_definition_of_compiler_generated_routine(callee);
        }  /* if */
      }  /* if */
    }  /* if */
    if (callee->function_def_number == NULL_function_def_number) {
      info_with_pos_sym(ec_constexpr_function_undefined,
                        &callee_node->position, symbol_for(callee), ips);
      do_constexpr_fail(result);
      if (callee->is_deleted && ips->is_constant_evaluated) {
        /* An error has presumably been issued earlier (use of a deleted
           function).  Treat this as an input error to avoid extraneous
           diagnostics. */
        expect_error();
        ips->input_error = TRUE;
      }  /* if */
      goto done;
    }  /* if */
    /* Don't attempt to interpret a non-constexpr function.  The flag
       scope->is_constexpr_routine is set at the end of a constexpr function
       definition, so this also prevents the interpretation of a function that
       is not fully parsed (e.g., requested due to a recursive call in a
       constexpr function). */
    callee_scope = scope_for_routine(callee);
    if (!callee_scope->is_constexpr_routine) {
      info_with_pos_sym(ec_constexpr_call_not_interpretable,
                        &call_node->position, symbol_for(callee), ips);
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    /* Phase 2: Map the parameters to the arguments. */
    /* Count the parameters (including "this") to make sure there are enough
       arguments for the parameters.  However, if we are calling the lambda
       call operator through the entry point returned by the closure's
       conversion function, ignore the "this" parameter. */
    if (lambda_entry_case) {
      this_var = NULL;
    } else {
      this_var = callee_scope->variant.routine.this_param_variable;
    }  /* if */
    params = callee_scope->variant.routine.parameters;
    if (is_member_call) {
      /* If there is a "this" parameter, count an extra parameter. */
      n_params = 1;
    } else {
      n_params = 0;
    }  /* if */
    for (param = params; param != NULL; param = param->next) {
      n_params += 1;
    }  /* if */
    if (n_args < n_params) {
      info_with_pos(ec_too_few_arguments, &call_node->position, ips);
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    /* Associate with the parameter variables the allocation sequence number
       that is about to be created for the top-level block (since the
       parameters technically expire when that block expires, even though in
       our implementation they are allocated in the caller's context). */
    alloc_seq_number = ips->curr_alloc_seq_number+1;
    p_arg_ptr = (a_byte**)arg_ptrs;
    arg_size = (a_byte_count*)arg_sizes;
    if (this_var != NULL) {
      a_byte         *arg_bytes = *p_arg_ptr;
      a_var_postfix  *postfix = (a_var_postfix*)(arg_bytes+*arg_size);
      postfix->alloc_seq_number = alloc_seq_number;
      map_or_replace_ptr(&ips->map, this_var, arg_bytes,
                         postfix->prev_storage);
      if (callee->is_lambda_body) {
        /* If this lambda contains a nested lambda that captures a capture
           of this lambda, it will search ips->map for &ips->curr_call_frame
           to find this lambda's closure object.  Make sure it will be
           found.  See also set_up_param_ref_for_this_ptr. */
        a_constexpr_address  *this_addr = (a_constexpr_address*)arg_bytes;
        if (!is_runtime_data_address(this_addr)) {
          closure_ptr = set_up_param_ref_for_this_ptr(
                         ips, this_addr->address, this_addr->complete_object);
        }  /* if */
      }  /* if */
      p_arg_ptr += 1;
      arg_size += 1;
    }  /* if */
    for (param = params; param != NULL; param = param->next) {
      a_byte         *arg_bytes = *p_arg_ptr;
      a_var_postfix  *postfix = (a_var_postfix*)(arg_bytes+*arg_size);
      postfix->alloc_seq_number = alloc_seq_number;
      map_or_replace_ptr(&ips->map, param, arg_bytes, postfix->prev_storage);
      p_arg_ptr += 1;
      arg_size += 1;
    }  /* for */
    /* Set up the call frame. */
    push_call_frame(ips, &frame, callee, &call_node->position,
                    result_storage, complete_object);
    if (callee->is_constexpr_intrinsic) {
      /* A standard library function or member function that the front end has
         marked as "constexpr-intrinsic", which means we should implement its
         semantics without regard for the actual definition.  For example,
         this could be a call to std::is_constant_evaluated. */
      result = do_constexpr_intrinsic_call(
                                   ips, callee, call_node, (a_byte**)arg_ptrs,
                                   result_storage, complete_object);
    } else {
      /* Run the function's top-level block statement. */
      result = run_function_body(ips, callee_scope);
    }  /* if */
    if (result) {
      if (retval_offset != 0) {
        /* A virtual call dispatching to an overriding function with a
           covariant return type.  The return value is an address that must
           be updated to match the static type of the expression. */
        ((a_constexpr_address*)result_storage)->address += retval_offset;
      }  /* if */
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if BACK_END_IS_CP_GEN_BE
      callee->evaluated_in_interpreter = TRUE;
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    }  /* if */
    if (closure_ptr != NULL) {
      unmap_param_ref_for_this_ptr(ips, closure_ptr);
    }  /* if */
    /* Release any address structures, if needed. */
    p_arg_ptr = (a_byte**)arg_ptrs;
    for (arg = callee_node->next; arg != NULL; arg = arg->next) {
      a_type_ptr  tp = skip_typerefs(arg->type);
      if ((arg->is_lvalue || arg->is_xvalue) &&
          !(tp->kind == (a_type_kind)tk_pointer &&
            tp->variant.pointer.is_reference)) {
          /* When a class-type argument is passed by-value via a copy
             constructor call, the argument is left as an lvalue.  However,
             such cases aren't passed via an address above. */
      } else {
        release_address_structures(arg, tp, *p_arg_ptr);
      }  /* if */
      p_arg_ptr += 1;
    }  /* for */
    pop_call_frame(ips);
    /* Release mappings of the parameters. */
    p_arg_ptr = (a_byte**)arg_ptrs;
    arg_size = (a_byte_count*)arg_sizes;
    if (this_var != NULL) {
      a_byte         *arg_bytes = *p_arg_ptr;
      a_var_postfix  *postfix = (a_var_postfix*)(arg_bytes+*arg_size);
      if (postfix->prev_storage == NULL) {
        unmap_ptr(&ips->map, this_var);
      } else {
        replace_mapped_ptr(&ips->map, this_var, postfix->prev_storage);
      }  /* if */
      p_arg_ptr += 1;
      arg_size += 1;
    }  /* if */
    for (param = params; param != NULL; param = param->next) {
      a_byte         *arg_bytes = *p_arg_ptr;
      a_var_postfix  *postfix = (a_var_postfix*)(arg_bytes+*arg_size);
      if (postfix->prev_storage == NULL) {
        unmap_ptr(&ips->map, param);
      } else {
        replace_mapped_ptr(&ips->map, param, postfix->prev_storage);
      }  /* if */
      p_arg_ptr += 1;
      arg_size += 1;
    }  /* for */
    /* Reduce the cost of the call to just 2. */
    ips->cost -= up_front_cost-2;
  }  /* if */
done:
  return result;
}  /* do_constexpr_call */


static a_byte_count record_anon_union_active_field(a_field_ptr  *p_fp,
                                                   a_byte       *storage,
                                                   a_byte       *complete_obj)
/*
*p_fp is a field in an anonymous union.  storage points to the representation
of the object enclosing the one or more anonymous union parent objects of
*p_fp.  Set the active field in every anonymous union parent object, update
*p_fp to point to the top-most anonymous parent object, and return the offset
of the original *p_fp field in the representation of the returned *p_fp field.
*/
{
  a_field_ptr   fp = *p_fp, aufp;
  a_symbol_ptr  aufp_sym;
  a_byte_count  offset;

  aufp_sym = symbol_for(fp)->variant.field.anonymous_parent_object;
  if (symbol_is(aufp_sym, sk_field)) {
    a_type_ptr  au_type;
    aufp = aufp_sym->variant.field.ptr;
    au_type = skip_typerefs(aufp->type);
    get_mapped_byte_count(&persistent_map, aufp, offset);
    if (aufp_sym->variant.field.anonymous_parent_object != NULL) {
      /* aufp is not the top-most anonymous union.  Recurse to determine its
         offset, and then replace it by the top-most anonymous union. */
      offset += record_anon_union_active_field(&aufp, storage, complete_obj);
    }  /* if */
    mark_subobject_initialized(storage+offset, complete_obj);  
    if (type_is(au_type, tk_union)) {
      *(a_field_ptr*)(storage+offset) = fp;
    } else {
      /* Nonstandard anonymous "unions" can be "anonymous structs". */
      record_subobject_derivation(storage+offset, NULL);
    }  /* if */
    *p_fp = aufp;
  } else {
    offset = 0;
  }  /* if */
  return offset;
}  /* record_anon_union_active_field */


static a_boolean anon_union_field_is_active_field(a_field_ptr          fp,
                                                  a_constexpr_address  *cap)
/*
fp is a variant field of a type X and cap points to an object x of type X.
Return TRUE if fp is active in x.  Currently, this function only handles up
to 10 nested anonymous unions.
*/
{
  a_boolean    result = TRUE;
#define MAX_AU_DEPTH 10
  a_field_ptr  au_parent[MAX_AU_DEPTH+1];
  int          n = 1;

  au_parent[0] = fp;
  /* Record the sequence of anonymous union parent fields starting from the
     innermost one. */
  for (; n<MAX_AU_DEPTH; ++n) {
    a_symbol_ptr  aufp_sym;
    aufp_sym = symbol_for(fp)->variant.field.anonymous_parent_object;
    if (aufp_sym != NULL && symbol_is(aufp_sym, sk_field)) {
      fp = aufp_sym->variant.field.ptr;
      au_parent[n] = fp;
    } else {
      break;
    }  /* if */
  }  /* for */
  if (n == MAX_AU_DEPTH) {
    result = FALSE;
  } else {
    /* Now check in reverse order that each field up to the original is
       active. */
    a_byte        *address = cap->address;
    a_byte_count  offset;
    while (--n > 0) {
      get_mapped_byte_count(&persistent_map, au_parent[n], offset);
      address += offset;
      if (*(a_field_ptr*)address != au_parent[n-1]) {
        result = FALSE;
        break;
      }  /* if */
    }  /* while */
  }  /* if */
#undef MAX_AU_DEPTH
  return result;
}  /* anon_union_field_is_active_field */


static a_boolean do_constexpr_ctor(an_interpreter_state  *ips,
                                   a_dynamic_init_ptr    dip,
                                   a_source_position     *pos,
                                   a_constexpr_address   *cap,
                                   a_constexpr_address   *implied_src)
/*
Interpret the constructor call represented by the given dynamic initialization
entry.  Return TRUE if no error occurred; otherwise, return FALSE and update
*ips accordingly.  pos is the position of the call.  The object is constructed
at the location indicated by result_storage, which is within the given complete
object whose allocation sequence number is alloc_seq.  If implied_src is
non-NULL, this is a copy/move constructor invocation and the source object is
stored at the location indicated by implied_src.

This is similar to do_constexpr_call, but the call has a different
representation, and mem-initializers must be interpreted prior to interpreting
the body of the (constructor) function proper.
*/
{
  a_routine_ptr     callee = dip->variant.constructor.ptr;
  a_boolean         result = TRUE;

  /* Retrieve the routine scope, or issue an error. */
  if (callee == NULL) {
    info_with_pos(ec_constexpr_expression_cannot_be_interpreted, pos, ips);
    do_constexpr_fail(result);
    goto done;
  } else if (!callee->is_constexpr) {
    info_with_pos_sym(ec_constexpr_call_to_nonconstexpr_function,
                      pos, symbol_for(callee), ips);
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  if (!callee->defined) {
    if (load_routine_definition_from_module(callee)) {
      /* The definition was stored in a module file. */
    } else {
      set_instance_required(symbol_for(callee), TRUE, SIR_CONSTANT_CONTEXT);
    }  /* if */
  }  /* if */
  if (callee->function_def_number == NULL_function_def_number) {
    info_with_pos_sym(ec_constexpr_function_undefined, pos,
                      symbol_for(callee), ips);
    do_constexpr_fail(result);
    if (callee->is_deleted && ips->is_constant_evaluated) {
      /* An error has presumably been issued earlier (use of a deleted
         function).  Treat this as an input error to avoid extraneous
         diagnostics. */
      expect_error();
      ips->input_error = TRUE;
    }  /* if */
  } else if (callee->is_prototype_instantiation) {
    info_with_pos_sym(ec_constexpr_call_not_interpretable, pos,
                      symbol_for(callee), ips);
    do_constexpr_fail(result);
  } else if (cost_exceeded(ips)) {
    more_info_diagnostic(ec_excessive_constexpr_complexity, &ips->position,
                         &ips->diag_list);
    do_constexpr_fail(result);
  } else {
    a_byte               *result_storage = cap->address;
    a_byte               *complete_object = cap->complete_object;
    a_scope_ptr          callee_scope = scope_for_routine(callee);
    a_statement_ptr      block_stmt = callee_scope->assoc_block;
    a_call_frame         frame;
    an_expr_node_ptr     args = dip->variant.constructor.args, arg;
    a_variable_ptr       params = callee_scope->variant.routine.parameters,
                         param, this_var;
    a_constructor_init_ptr
                         ctor_init;
    a_byte_count         n_args = 1, n_params = 1;
    a_byte_count         *arg_size;
    a_byte               *arg_ptrs, **p_arg_ptr, *this_bytes, *arg_sizes;
    an_alloc_seq_number  alloc_seq_number;
    a_type_ptr           class_type = parent_class_of(callee);
    unsigned long        up_front_cost;
    /* Don't attempt to interpret a non-constexpr function.  The flag
       scope->is_constexpr_routine is set at the end of a constexpr function
       definition, so this also prevents the interpretation of a function that
       is not fully parsed (e.g., requested due to a recursive call in a
       constexpr function). */
    if (!callee_scope->is_constexpr_routine) {
      do_constexpr_fail(result);
      info_with_pos_sym(ec_constexpr_call_not_interpretable, pos,
                        symbol_for(callee), ips);
      goto done;
    }  /* if */
    if (class_type->variant.class_struct_union.any_virtual_base_classes) {
      do_constexpr_fail(result);
      info_with_pos_type(ec_constexpr_object_with_virtual_base, pos,
                         class_type, ips);
      goto done;
    }  /* if */
    /* Account a relatively high cost for the call up-front, to limit the
       overall call depth.  When the call returns, that cost will be
       reduced. */
    up_front_cost = max_cost_constexpr_call/max_depth_constexpr_call+1;
    ips->cost += up_front_cost;
    /* Set up arguments, starting with "this" if applicable. */
    /* This process must happen in two phases.  First, the arguments must be
       allocated and evaluated.  Only then can we map parameter variables onto
       the allocated arguments.  We cannot do the two in a single loop because
       of recursive calls.  (See do_constexpr_call for details.)  n_args was
       initialized to 1, because constructors always have a "this" pointer. */
    for (arg = args; arg != NULL; arg = arg->next) {
      n_args += 1;
    }  /* for */
    if (implied_src != NULL) n_args += 1;
    alloc_stack_bytes(ips, (a_byte_count)(n_args*sizeof(a_byte*)), arg_ptrs);
    alloc_stack_bytes(ips, (a_byte_count)(n_args*sizeof(a_byte_count)),
                                                                    arg_sizes);
    /* Count the parameters (including "this") to make sure there are enough
       arguments for the parameters. */
    for (param = params; param != NULL; param = param->next) {
      n_params += 1;
    }  /* if */
    if (n_args < n_params) {
      info_with_pos(ec_too_few_arguments, pos, ips);
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    /* Phase 1: Allocate and evaluate the arguments. */
    /* The "+1" below is to account for the "this" pointer (handled later). */
    p_arg_ptr = (a_byte**)arg_ptrs+1;
    arg_size = (a_byte_count*)arg_sizes;
    if (implied_src != NULL &&
        dip->variant.constructor.is_copy_constructor_with_implied_source) {
      /* The first non-this argument is implicit (i.e., not represented in
         the IL).  Since it always corresponds to the reference parameter of
         a copy/move constructor, we know it is a constexpr address. */
      a_byte_count  n_bytes = sizeof(a_constexpr_address);
      a_byte        *arg_bytes;
      do_host_alignment(n_bytes);
      *arg_size = n_bytes;
      arg_size += 1;
      n_bytes += sizeof(a_var_postfix);
      alloc_complete_object(ips, n_bytes, params->type, arg_bytes);
      *(a_constexpr_address*)arg_bytes = *implied_src;
      mark_complete_object_initialized(arg_bytes);
      *p_arg_ptr = arg_bytes;
      p_arg_ptr += 1;
    }  /* if */
    for (arg = args; arg != NULL; arg = arg->next) {
      a_type_ptr    tp = skip_typerefs(arg->type);
      a_byte_count  n_bytes;
      a_byte        *arg_bytes;
      a_boolean     restore_lvalue = FALSE, restore_xvalue = FALSE;
      if (!(tp->kind == (a_type_kind)tk_pointer &&
            tp->variant.pointer.is_reference)) {
        /* When a class-type argument is passed by-value via a copy
           constructor call, the argument is left as an lvalue.  Temporarily
           set it back to an rvalue. */
        if (arg->is_lvalue) {
          restore_lvalue = TRUE;
          arg->is_lvalue = FALSE;
        } else if (arg->is_xvalue) {
          restore_xvalue = TRUE;
          arg->is_xvalue = FALSE;
        }  /* if */
      }  /* if */
      n_bytes = expr_result_size(ips, arg, tp, &result);
      do_host_alignment(n_bytes);
      *arg_size = n_bytes;
      arg_size += 1;
      n_bytes += sizeof(a_var_postfix);
      alloc_complete_object(ips, n_bytes, tp, arg_bytes);
      *p_arg_ptr = arg_bytes;
      p_arg_ptr += 1;
      if (result) {
        if (arg == args->next &&
            class_type_supp(class_type)->is_initializer_list &&
            is_operation_node(arg) && node_operator_is(arg, eok_padd) &&
            node_is(arg->variant.operation.operands, enk_reuse_value)) {
          /* There is only one standard use for enk_reuse_value nodes and that
             is in some invocations of the std::initializer_list constructor:
             That use is handled as a special case here.  (Other uses occur in
             nonstandard extensions, and we approximate their behavior in
             general expression interpretation.) */
          /* The first argument is the address of an array (and has been
             evaluated already).  The second argument represents the address
             one position past the end of that array. */
          a_constexpr_address  *arg1, *arg2;
          a_type_ptr           elem_type;
          a_byte_count         elem_size;
          elem_type = skip_typerefs(tp->variant.pointer.type);
          elem_size = value_bytes_for_type(ips, elem_type, &result);
          arg1 = (a_constexpr_address*)p_arg_ptr[-2];
          arg2 = (a_constexpr_address*)p_arg_ptr[-1];
          *arg2 = *arg1;
          check_assertion(is_array_element(arg2));
          arg2->address += arg2->length * elem_size; /*lint !e679*/
          arg2->flags |= CA_CANNOT_DEREFERENCE;
        } else if (!do_constexpr_expression(ips, arg, arg_bytes, arg_bytes)) {
          do_constexpr_fail(result);
        }  /* if */
        if (!is_immediate_class_type(tp) && !type_is(tp, tk_array)) {
          mark_complete_object_initialized(arg_bytes);
        }  /* if */
      }  /* if */
      if (restore_lvalue) {
        arg->is_lvalue = TRUE;
      } else if (restore_xvalue) {
        arg->is_xvalue = TRUE;
      }  /* if */
      if (!result) {
        goto done;
      }  /* if */
    }  /* for */
    /* Phase 2: Map the parameters to the arguments. */
    /* First map the "this" pointer. */
    this_var = callee_scope->variant.routine.this_param_variable;
    if (this_var == NULL) {
      /* A constructor should always have a "this" parameter, but in some
         error cases, it may not have been created. */
      expect_error();
      do_constexpr_fail(result);
      goto done;
    } else {
      /* This is similar to do_constexpr_alloc_variable, except for the
         allocation sequence number value. */
      a_type_ptr     this_type = skip_typerefs(this_var->type);
      a_byte_count   this_n_bytes = sizeof(a_constexpr_address);
      a_byte_count   with_postfix_bytes;
      a_var_postfix  *postfix;
      do_host_alignment(this_n_bytes);
      alloc_seq_number = ips->curr_alloc_seq_number++;
      add_to_live_set(&ips->live_set, alloc_seq_number);
      with_postfix_bytes = this_n_bytes+sizeof(a_var_postfix);
      alloc_complete_object(ips, with_postfix_bytes, this_type, this_bytes);
      *(a_constexpr_address*)this_bytes = *cap;
      mark_complete_object_initialized(this_bytes);
      postfix = (a_var_postfix*)(this_bytes+this_n_bytes);
      postfix->alloc_seq_number = alloc_seq_number;
      map_or_replace_ptr(&ips->map, this_var, this_bytes,
                         postfix->prev_storage);
    }  /* if */
    /* Associate with the parameter variables a new allocation number.  For
       ordinary calls, we just use the allocation number about to be created
       for the function scope, but for constructors that is not an option
       because constructor initializers must first be evaluated. */
    alloc_seq_number += 1;
    add_to_live_set(&ips->live_set, alloc_seq_number);
    p_arg_ptr = (a_byte**)arg_ptrs+1;
    arg_size = (a_byte_count*)arg_sizes;
    for (param = params; param != NULL; param = param->next) {
      a_byte         *arg_bytes = *p_arg_ptr;
      a_var_postfix  *postfix = (a_var_postfix*)(arg_bytes+*arg_size);
      postfix->alloc_seq_number = alloc_seq_number;
      map_or_replace_ptr(&ips->map, param, arg_bytes, postfix->prev_storage);
      p_arg_ptr += 1;
      arg_size += 1;
    }  /* for */
    if (dip->variant.constructor.value_initialization) {
      /* If this is for value initialization, clear the storage first.
         Do not, however, overwrite the first word (which may record the
         inheritance hierarchy). */
      a_byte_count  n_class_bytes;
      a_boolean     dummy = TRUE;
      n_class_bytes = f_value_bytes_for_type(ips, class_type, &dummy);
      memzero(result_storage+sizeof(void*),
              size_t_arg(n_class_bytes-sizeof(void*)));
    }  /* if */
    /* Set up the call frame. */
    push_call_frame(ips, &frame, callee, pos, result_storage, complete_object);
    /* Run the constructor initializers. */
    ctor_init = callee_scope->variant.routine.constructor_inits;
    for (; ctor_init != NULL; ctor_init = ctor_init->next) {
      a_byte_count         offset;
      a_constexpr_address  dst_addr = *cap;
      a_dynamic_init_ptr   sub_dip;
      a_type_ptr           tp;
      a_boolean            record_param_ref = FALSE;
      if (ctor_init->kind == (a_constructor_init_kind)cik_field) {
        a_field_ptr  fp = ctor_init->variant.field;
        tp = skip_typerefs(fp->type);
        if (ctor_init->use_field_initializer) {
          sub_dip = fp->initializer;
          /* Field initializers may contain enk_param_ref nodes representing
             "this": We provide a mapping for those below. */
          record_param_ref = TRUE;
        } else {
          sub_dip = ctor_init->initializer;
        }  /* if */
        if (symbol_for(fp) != NULL &&
            symbol_for(fp)->variant.field.anonymous_parent_object != NULL) {
          /* ctor-init may point directly to an anonymous union field.  In
             that case, the active fields of all intervening anonymous unions
             must be recorded, offset must be adjusted, and fp must be set to
             the top-level anonymous union parent field, so that it can be
             recorded as the active field in case class_type itself is a union
             (see below). */
          if ((dyn_init_is(sub_dip, dik_bitwise_copy) &&
               sub_dip->variant.bitwise_copy.source == NULL) ||
              (dyn_init_is(sub_dip, dik_constructor) &&
               sub_dip->variant.constructor
                               .is_copy_constructor_with_implied_source)) {
            /* An implied-source copy.  All the variant members will be listed,
               but only the one corresponding to the active member of the
               implied source should be copied. */
            a_constexpr_address  *src_addr;
            src_addr = (a_constexpr_address*)((a_byte**)arg_ptrs)[1];
            if (is_runtime_data_address(src_addr)) {
              info_with_pos(ec_constexpr_access_to_runtime_storage,
                            args != NULL ? &args->position : pos, ips);
              do_constexpr_fail(result);
              break;
            } else if (!anon_union_field_is_active_field(fp, src_addr)) {
              /* fp does not designate the active field in the source.  So
                 don't attempt to copy it. */
              continue;
            }  /* if */
          }  /* if */
          a_field_ptr   orig_fp = fp;
          /* Activate any needed anonymous union fields. */
          (void)record_anon_union_active_field(&fp, result_storage,
                                               complete_object);
          /* fp now points to the outermost anonymous parent object.  If
             class_type is a union, that is used below to set the active
             field of that union. */
          /* Update dst_addr for any anonymous union/structs (including the
             variant path). */
          if (!add_to_variant_path(&dst_addr, orig_fp, class_type,
                                   /*for_ctor_init=*/TRUE)) {
            info_with_pos(ec_constexpr_too_many_nested_anonymous_types, pos, 
                          ips);
            do_constexpr_fail(result);
            break;
          }  /* if */
          /* The call to add_to_variant_path adjusted dst_addr to point to
             the innermost anonymous union enclosing fp. */
          get_mapped_byte_count(&persistent_map, orig_fp, offset);
          dst_addr.address += offset;
          offset = dst_addr.address - cap->address;
        } else {
          get_mapped_byte_count(&persistent_map, fp, offset);
          dst_addr.address += offset;
        }  /* if */
        if (type_is(class_type, tk_union)) {
          /* Record the active field for the enclosing union. */
          *(a_field_ptr*)result_storage = fp;
        }  /* if */
        mark_complete_class_object_if_needed(tp, dst_addr.address);
      } else if (ctor_init->kind == (a_constructor_init_kind)cik_delegation) {
        result = do_constexpr_dynamic_init(ips, ctor_init->initializer, pos,
                                           cap);
        break;
      } else {
        a_base_class_ptr  bcp = ctor_init->variant.base_class;
        tp = bcp->type;
        if (bcp->direct) {
          get_mapped_byte_count(&persistent_map, bcp, offset);
        } else {
          /* Inherited constructors may involve indirect base classes. */
          offset = compute_interpreter_base_offset(bcp, &bcp);
        }  /* if */
        dst_addr.address += offset;
        /* Record the derivation step. */
        record_subobject_derivation(dst_addr.address, bcp);
        sub_dip = ctor_init->initializer;
      }  /* if */
      if (dyn_init_is(sub_dip, dik_bitwise_copy) &&
          sub_dip->variant.bitwise_copy.source == NULL) {
        /* An implicit member copy in a copy constructor.  arg_ptrs[1] points
           to the first argument of the copy constructor, which is a reference
           to the copied object. */
        a_constexpr_address  *src_addr;
        src_addr = (a_constexpr_address*)((a_byte**)arg_ptrs)[1];
        if (is_runtime_data_address(src_addr)) {
          info_with_pos(ec_constexpr_access_to_runtime_storage,
                        args != NULL ? &args->position : pos, ips);
          do_constexpr_fail(result);
          break;
        } else {
          if (!constexpr_copy_object(ips, tp,
                                     args != NULL ? &args->position : pos,
                                     src_addr->address+offset,
                                     src_addr->complete_object,
                                     dst_addr.address, complete_object)) {
            do_constexpr_fail(result);
            break;
          }  /* if */
        }  /* if */
      } else {
        if (dyn_init_is(sub_dip, dik_constructor) &&
            sub_dip->variant.constructor
                            .is_copy_constructor_with_implied_source) {
          /* Constructor invocations for the mem-initializers of copy
             constructors don't always have an explicit source expression.
             Pass the source location of the top-level call through to the
             subobject constructor (adjusted for the offset). */
          a_constexpr_address  *src_addr;
          src_addr = (a_constexpr_address*)((a_byte**)arg_ptrs)[1];
          if (is_runtime_data_address(src_addr)) {
            info_with_pos(ec_constexpr_access_to_runtime_storage,
                          args != NULL ? &args->position : pos, ips);
            do_constexpr_fail(result);
            break;
          } else {
            a_constexpr_address  adjusted_src_addr;
            adjusted_src_addr = *src_addr;
            adjusted_src_addr.address += offset;
            if (!do_constexpr_ctor(ips, sub_dip,
                                   &callee->source_corresp.decl_position,
                                   &dst_addr, &adjusted_src_addr)) {
              do_constexpr_fail(result);
              break;
            } else {
              mark_subobject_initialized(dst_addr.address, complete_object);
            }  /* if */
          }  /* if */
        } else if (dyn_init_is(sub_dip, dik_zero) ||
                   dyn_init_is(sub_dip, dik_none)) {
          /* Just zero the storage (for the dik_zero case) and record the
             derivation structure (which is needed even for the dik_none
             case). */
          init_subobject_to_zero(ips, dst_addr.address, tp, complete_object);
        } else {
          a_byte               *prev_this_bytes = NULL;
          a_constexpr_address  *src_addr = NULL;
          if (dyn_init_is(sub_dip, dik_nonconstant_aggregate) &&
              n_params == 2 && callee->compiler_generated) {
            /* Implicit copies might rely on an "implied source"
               representation.  Pass the current source object address down
               in case it is needed. */
            src_addr = (a_constexpr_address*)((a_byte**)arg_ptrs)[1];
            src_addr->address += offset;
          }  /* if */
          if (record_param_ref) {
            /* Associate the "this" pointer value (arbitrarily) with
               &ips->curr_call_frame. */
            map_or_replace_ptr(&ips->map, &ips->curr_call_frame, this_bytes,
                               prev_this_bytes);
          }  /* if */
          if (!do_constexpr_dynamic_init(ips, sub_dip,
                                         &callee->source_corresp.decl_position,
                                         &dst_addr, src_addr)) {
            do_constexpr_fail(result);
            break;
          } else {
            mark_subobject_initialized(dst_addr.address, complete_object);
          }  /* if */
          if (src_addr != NULL) src_addr->address -= offset;
          if (record_param_ref) {
            if (prev_this_bytes == NULL) {
              unmap_ptr(&ips->map, &ips->curr_call_frame);
            } else {
              replace_mapped_ptr(&ips->map, &ips->curr_call_frame,
                                 prev_this_bytes);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    mark_subobject_initialized(result_storage, complete_object);
    /* Run the function's top-level block statement. */
    if (!result) {
      /* Something went wrong.  Don't perform additional interpretation. */
    } else {
      if (block_stmt->kind == (a_statement_kind)stmk_try_block) {
        block_stmt = block_stmt->variant.try_block->statement;
      }  /* if */
      result = do_constexpr_block_statement(ips, block_stmt, callee_scope);
    }  /* if */
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if BACK_END_IS_CP_GEN_BE
    if (result) callee->evaluated_in_interpreter = TRUE;
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    /* Release any address structures, if needed. */
    p_arg_ptr = (a_byte**)arg_ptrs+1;
    for (arg = args; arg != NULL; arg = arg->next) {
      a_type_ptr  tp = skip_typerefs(arg->type);
      if ((arg->is_lvalue || arg->is_xvalue) &&
          !(tp->kind == (a_type_kind)tk_pointer &&
            tp->variant.pointer.is_reference)) {
          /* When a class-type argument is passed by-value via a copy
             constructor call, the argument is left as an lvalue.  However,
             such cases aren't passed via an address above. */
      } else {
        release_address_structures(arg, tp, *p_arg_ptr);
      }  /* if */
      p_arg_ptr += 1;
    }  /* for */
    pop_call_frame(ips);
    /* Unmap the parameters. */
    p_arg_ptr = (a_byte**)arg_ptrs+1;
    arg_size = (a_byte_count*)arg_sizes;
    for (param = params; param != NULL; param = param->next) {
      a_byte         *arg_bytes = *p_arg_ptr;
      a_var_postfix  *postfix = (a_var_postfix*)(arg_bytes+*arg_size);
      if (postfix->prev_storage == NULL) {
        unmap_ptr(&ips->map, param);
      } else {
        replace_mapped_ptr(&ips->map, param, postfix->prev_storage);
      }  /* if */
      p_arg_ptr += 1;
      arg_size += 1;
    }  /* for */
    { /* Unmap the "this" parameter. */
      a_var_postfix  *postfix;
      postfix = (a_var_postfix*)(this_bytes+sizeof(a_constexpr_address));
      if (postfix->prev_storage == NULL) {
        unmap_ptr(&ips->map, this_var);
      } else {
        replace_mapped_ptr(&ips->map, this_var, postfix->prev_storage);
      }  /* if */
      remove_from_live_set(&ips->live_set, alloc_seq_number-1);
    }
    remove_from_live_set(&ips->live_set, alloc_seq_number);
    /* Reduce the cost of the call to just 2. */
    ips->cost -= up_front_cost-2;
  }  /* if */
done:
  return result;
}  /* do_constexpr_ctor */


static a_boolean do_constexpr_dtor(an_interpreter_state  *ips,
                                   a_routine_ptr         callee,
                                   a_source_position     *pos,
                                   a_byte                *result_storage,
                                   a_byte                *complete_object,
                                   a_boolean             nonvirtual)
/*
Interpret a call to the given destructor (callee).  Return TRUE if no error
occurred; otherwise, return FALSE and update *ips accordingly.  pos is the
position of the call.  The object being destroyed is at the location indicated
by result_storage, which is within the given complete object.  If nonvirtual
(defaulted to FALSE) is TRUE, no virtual dispatch is performed for a virtual
destructor.

This is similar to do_constexpr_ctor.
*/
{
  a_boolean  result = TRUE;

  /* Retrieve the routine scope, or issue an error. */
  if (callee == NULL) {
    unexpected_condition();
  } else if (!callee->is_constexpr) {
    info_with_pos_sym(ec_constexpr_call_to_nonconstexpr_function,
                      pos, symbol_for(callee), ips);
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  if (!callee->defined) {
    set_instance_required(symbol_for(callee), TRUE, SIR_CONSTANT_CONTEXT);
  }  /* if */
  if (callee->function_def_number == NULL_function_def_number) {
    info_with_pos_sym(ec_constexpr_function_undefined, pos,
                      symbol_for(callee), ips);
    do_constexpr_fail(result);
    if (callee->is_deleted && ips->is_constant_evaluated) {
      /* An error has presumably been issued earlier (use of a deleted
         function).  Treat this as an input error to avoid extraneous
         diagnostics. */
      expect_error();
      ips->input_error = TRUE;
    }  /* if */
  } else if (callee->is_prototype_instantiation) {
    info_with_pos_sym(ec_constexpr_call_not_interpretable, pos,
                      symbol_for(callee), ips);
    do_constexpr_fail(result);
  } else if (cost_exceeded(ips)) {
    more_info_diagnostic(ec_excessive_constexpr_complexity, &ips->position,
                         &ips->diag_list);
    do_constexpr_fail(result);
  } else {
    a_scope_ptr             callee_scope;
    a_statement_ptr         block_stmt;
    a_call_frame            frame;
    a_variable_ptr          this_var;
    a_constructor_init_ptr  dtor_init;
    a_byte                  *this_bytes;
    an_alloc_seq_number     alloc_seq_number;
    a_type_ptr              class_type;
    unsigned long           up_front_cost;
    a_byte_count            retval_offset = 0;
    a_byte_count            this_n_bytes = sizeof(a_constexpr_address);
    a_byte_count            with_postfix_bytes;
    /* Allocate the "this" parameter. */
    /* This is similar to do_constexpr_alloc_variable, except for the
       allocation sequence number value. */
    alloc_seq_number = ips->curr_alloc_seq_number++;
    add_to_live_set(&ips->live_set, alloc_seq_number);
    do_host_alignment(this_n_bytes);
    with_postfix_bytes = this_n_bytes+sizeof(a_var_postfix);
    alloc_complete_object(ips, with_postfix_bytes, generic_ptr_type,
                          this_bytes);
    clear_address(this_bytes, result_storage);
    ((a_constexpr_address*)this_bytes)->complete_object = complete_object;
    ((a_constexpr_address*)this_bytes)->alloc_seq_number = alloc_seq_number;
    mark_complete_object_initialized(this_bytes);
    /* If this is a virtual destructor call, adjust the callee. */
    if (callee->is_virtual && !nonvirtual) {
      if (!adjust_virtual_callee(&callee, &this_bytes, &retval_offset)) {
        info_with_pos(ec_constexpr_access_to_runtime_storage, pos, ips);
        do_constexpr_fail(result);
        goto done;
      } else {
        result_storage = ((a_constexpr_address*)this_bytes)->address;
      }  /* if */
    }  /* if */
    callee_scope = scope_for_routine(callee);
    block_stmt = callee_scope->assoc_block;
    class_type = parent_class_of(callee);
    /* Don't attempt to interpret a non-constexpr function.  The flag
       scope->is_constexpr_routine is set at the end of a constexpr function
       definition, so this also prevents the interpretation of a function that
       is not fully parsed (e.g., requested due to a recursive call in a
       constexpr function). */
    if (!callee_scope->is_constexpr_routine) {
      do_constexpr_fail(result);
      info_with_pos_sym(ec_constexpr_call_not_interpretable, pos,
                        symbol_for(callee), ips);
      goto done;
    }  /* if */
    /* Account a relatively high cost for the call up-front, to limit the
       overall call depth.  When the call returns, that cost will be
       reduced. */
    up_front_cost = max_cost_constexpr_call/max_depth_constexpr_call+1;
    ips->cost += up_front_cost;
    /* Phase 1: Allocate and evaluate the "this" argument. */
    /* Create the "this" argument and map it to the "this" parameter
       variable. */
    this_var = callee_scope->variant.routine.this_param_variable;
    if (this_var == NULL) {
      /* A destructor should always have a "this" parameter, but in some
         error cases, it may not have been created. */
      expect_error();
      do_constexpr_fail(result);
      goto done;
    } else {
      a_var_postfix  *postfix;
      postfix = (a_var_postfix*)(this_bytes+this_n_bytes);
      postfix->alloc_seq_number = alloc_seq_number;
      map_or_replace_ptr(&ips->map, this_var, this_bytes,
                         postfix->prev_storage);
    }  /* if */
    /* Set up the call frame. */
    /*lint -e{733}*/
    push_call_frame(ips, &frame, callee, pos, result_storage, complete_object);
    /* Run the function's top-level block statement. */
    if (!result) {
      /* Something went wrong.  Don't perform additional interpretation. */
    } else {
      if (block_stmt->kind == (a_statement_kind)stmk_try_block) {
        block_stmt = block_stmt->variant.try_block->statement;
      }  /* if */
      result = do_constexpr_block_statement(ips, block_stmt, callee_scope);
    }  /* if */
    /* Run the "constructor initializers" that describe subobject
       destructions. */
    unmark_complete_object_initialized(complete_object);
    dtor_init = callee_scope->variant.routine.constructor_inits;
    for (; dtor_init != NULL; dtor_init = dtor_init->next) {
      a_byte_count        offset;
      a_dynamic_init_ptr  sub_dip;
      if (dtor_init->kind == (a_constructor_init_kind)cik_field) {
        a_field_ptr  fp = dtor_init->variant.field;
        get_mapped_byte_count(&persistent_map, fp, offset);
        sub_dip = dtor_init->initializer;
        if (type_is(class_type, tk_union)) {
          /* Clear the active field for the enclosing union. */
          *(a_field_ptr*)result_storage = NULL;
        }  /* if */
      } else {
        a_base_class_ptr  bcp = dtor_init->variant.base_class;
        get_mapped_byte_count(&persistent_map, bcp, offset);
        sub_dip = dtor_init->initializer;
      }  /* if */
      /* Clear the derivation/active-field state. */
      *(void**)(result_storage+offset) = NULL;
      if (!do_constexpr_dtor(ips, sub_dip->destructor, pos,
                             result_storage+offset, complete_object,
                             /*nonvirtual=*/TRUE)) {
        do_constexpr_fail(result);
        break;
      } else {
        mark_subobject_uninitialized(result_storage+offset, complete_object);
      }  /* if */
    }  /* for */
    if (!mark_whole_subobject_uninitialized(ips, result_storage, class_type,
                                            complete_object)) {
      result = FALSE;
      goto done;
    }  /* if */
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if BACK_END_IS_CP_GEN_BE
    if (result) callee->evaluated_in_interpreter = TRUE;
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    pop_call_frame(ips);
    { /* Unmap the "this" parameter. */
      a_var_postfix  *postfix;
      postfix = (a_var_postfix*)(this_bytes+sizeof(a_constexpr_address));
      if (postfix->prev_storage == NULL) {
        unmap_ptr(&ips->map, this_var);
      } else {
        replace_mapped_ptr(&ips->map, this_var, postfix->prev_storage);
      }  /* if */
      remove_from_live_set(&ips->live_set, alloc_seq_number);
    }
    /* Reduce the cost of the call to just 2. */
    ips->cost -= up_front_cost-2;
    ips->call_seen = TRUE;
  }  /* if */
done:
  return result;
}  /* do_constexpr_dtor */


static a_constant_ptr make_interpreter_copy_of_constant(
                                                   an_interpreter_state  *ips,
                                                   a_constant_ptr        con)
/*
Copy the given constant to a new ("local") entry and record it on a list
pointed to by ips->constants.
*/
{
  a_constant_ptr  new_con = local_constant();

  *new_con = *con;
  new_con->next = ips->constants;
  ips->constants = new_con;
  if (constant_is(new_con, ck_address) &&
      new_con->variant.address.subobject_path != NULL) {
    new_con->variant.address.subobject_path =
                 copy_subobject_path(new_con->variant.address.subobject_path);
  }  /* if */
  return new_con;
}  /* make_interpreter_copy_of_constant */


static a_boolean do_constexpr_offsetof(an_interpreter_state  *ips,
                                       an_expr_node_ptr      expr,
                                       a_byte                *result_storage,
                                       ARG_UNUSED a_byte     *complete_object)
/*
Evaluate, if possible, the given __builtin_offsetof expression (whose second
operand should be a chain of subscript and field selection nodes applied to a
constant null pointer).  If successful, return TRUE and store the result in
*result_storage.  Otherwise, return FALSE and record a diagnostic in *ips.
*/
{
  a_boolean         result = TRUE;
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  an_integer_value  offset_val;
  a_byte            *index_val = NULL;

  set_integer_value(&offset_val, (a_host_large_integer)0);
  while (!is_constant_node(arg2)) {
    an_expr_node_ptr  opnd;
    a_boolean         ovflo;
    check_assertion(is_operation_node(arg2));
    opnd = arg2->variant.operation.operands;
    if (node_operator_is(arg2, eok_subscript)) {
      /* A subscript: Add the index scaled by the element size. */
      an_expr_node_ptr  index_expr = opnd->next;
      a_type_ptr        etp = skip_typerefs(arg2->type);
      an_integer_value  size_val;
      if (index_val == NULL) {
        a_type_ptr  itp = skip_typerefs(index_expr->type);
        alloc_complete_object(ips, sizeof(an_integer_value), itp, index_val);
      }  /* if */
      if (!do_constexpr_expression(ips, index_expr, index_val, index_val)) {
        do_constexpr_fail(result);
        break;
      }  /* if */
      set_integer_value(&size_val, (a_host_large_integer)etp->size);
      multiply_integer_values(&size_val, (an_integer_value*)index_val,
                              /*is_signed=*/TRUE, &ovflo);
      if (ovflo) {
        do_constexpr_fail(result);
        break;
      }  /* if */
      add_integer_values(&offset_val, (an_integer_value*)&size_val,
                         /*is_signed=*/TRUE, &ovflo);
      if (ovflo) {
        do_constexpr_fail(result);
        break;
      }  /* if */
      arg2 = opnd;
    } else if (node_operator_is(arg2, eok_dot_field)) {
      /* A field selection: Add the field offset. */
      an_expr_node_ptr  field_expr = opnd->next;
      a_field_ptr       field = node_field(field_expr);
      an_integer_value  field_val;
      set_integer_value(&field_val, (a_host_large_integer)field->offset);
      add_integer_values(&offset_val, &field_val, /*is_signed=*/TRUE, &ovflo);
      if (ovflo) {
        do_constexpr_fail(result);
        break;
      }  /* if */
    } else if (node_operator_is(arg2, eok_array_to_pointer) ||
               node_operator_is(arg2, eok_indirect)) {
      /* These nodes are expected "glue" nodes. */
    } else {
      /* An unexpected node: Abandon evaluation. */
      do_constexpr_fail(result);
      break;
    }  /* if */
    arg2 = opnd;
  }  /* while */
  if (!result) {
    info_with_pos(ec_cannot_evaluate_builtin_offsetof,
                  &arg2->position, ips);
  } else {
    *(an_integer_value*)result_storage = offset_val;
  }  /* if */
  return result;
}  /* do_constexpr_offsetof */


static a_boolean do_constexpr_intaddr(an_interpreter_state  *ips,
                                      an_expr_node_ptr      expr,
                                      a_byte                *result_storage,
                                      ARG_UNUSED a_byte     *complete_object)
/*
Evaluate, if possible, the given __INTADDR__ expression.  If successful,
return TRUE and store the (integer) result in *result_storage.  Otherwise,
return FALSE and record a diagnostic in *ips.
*/
{
  a_boolean         result = TRUE, saved_permit_null_pointer_offsets ;
  an_expr_node_ptr  opnd1 = expr->variant.builtin_operation.operands;
  a_type_ptr        tp = skip_typerefs(opnd1->type);
  a_byte_count      n_bytes = expr_result_size(ips, opnd1, tp, &result);
  a_byte            *opnd_bytes;

  if (!result) goto done;
  saved_permit_null_pointer_offsets = ips->permit_null_pointer_offsets;
  ips->permit_null_pointer_offsets = TRUE;
  do_host_alignment(n_bytes);
  alloc_complete_object(ips, n_bytes, tp, opnd_bytes);
  if (do_constexpr_expression(ips, opnd1, opnd_bytes, opnd_bytes)) {
    if (type_is(tp, tk_pointer)) {
      /* Do not accept run-time constants that aren't based on a null
         pointer address. */
      a_constexpr_address  *cap = (a_constexpr_address*)opnd_bytes;
      if (!is_runtime_data_address(cap) ||
          !constant_is(cap->variant.addr_con, ck_integer)) {
        info_with_pos(ec_invalid_intaddr_address, &opnd1->position, ips);
        do_constexpr_fail(result);
      } else {
        *(an_integer_value*)result_storage = cap->variant.addr_con
                                                ->variant.integer_value;
      }  /* if */
    } else if (type_is(tp, tk_integer)) {
      *(an_integer_value*)result_storage = *(an_integer_value*)opnd_bytes;
    } else {
      result = FALSE;
      expect_error();
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  ips->permit_null_pointer_offsets = saved_permit_null_pointer_offsets;
done:
  return result;
}  /* do_constexpr_intaddr */


static a_boolean translate_target_bytes_to_interpreter_object(
                                   an_interpreter_state  *ips,
                                   a_type_ptr            type,
                                   a_byte                *src_storage,
                                   a_byte                *src_bitmap,
                                   a_byte                *dest_storage,
                                   a_byte                *dest_complete_object)
/*
Translate the bits stored in target layout at src_storage as an object of
the specified type into the equivalent interpreter object layout at
dest_storage.  src_bitmap is a parallel array to src_storage and indicates
which bits have been previously initialized.  dest_storage is a subobject
of dest_complete_object.  Report an error and return FALSE if the translation
cannot be performed.  Used in the implementation of __builtin_bit_cast.
*/
{
  a_boolean  result = TRUE, initialized = TRUE;
  a_byte     *orig_dest_storage = dest_storage;
  a_type_ptr tp = NULL;

  if (is_volatile_qualified_type(type)) {
    info_with_pos_type(ec_volatile_type_not_allowed, &ips->position, type,
                       ips);
    do_constexpr_fail(result);
  } else {
    a_byte  all_bits_on = ~(a_byte)0;
    tp = skip_typerefs(type);
    switch (tp->kind) {
      case tk_error:
        ips->input_error = TRUE;
        FALLTHROUGH
      case tk_pointer:
      case tk_nullptr:
      case tk_union:
      case tk_ptr_to_member:
      case tk_routine:
        /* These are explicitly forbidden for a constexpr bit_cast. */
        info_with_pos_type(ec_invalid_bit_cast_type, &ips->position, tp, ips);
        do_constexpr_fail(result);
        break;
      case tk_integer:
        { an_integer_value int_val, byte_val;
          a_boolean        ovfl;
          a_byte           byte;
          int              bit_shift;
          /* For every byte in the target representation of the integer,
             if initialized, use it to re-construct the actual integer
             value. */
          set_unsigned_integer_value(&int_val, 0);
          for (unsigned int i = 0; i < tp->size; i++) {
            if (*src_bitmap++ != all_bits_on) {
              initialized = FALSE;
            }  /* if */
            byte = *src_storage++;
            bit_shift = (int)(host_little_endian ? i : ((tp->size - 1) - i));
            bit_shift *= CHAR_BIT;
            /* The code below effectively does this:
                 int_val |= byte << bit_shift;
            */
            set_unsigned_integer_value(&byte_val, (a_host_large_integer)byte);
            shift_left_integer_value(&byte_val, bit_shift, &ovfl);
            if (ovfl) {
              info_with_pos_type(ec_constexpr_integer_overflow, &ips->position,
                                 tp, ips);
              do_constexpr_fail(result);
              break;
            }  /* if */
            or_integer_values(&int_val, &byte_val);
          }  /* for */
          if (int_type_is_signed(tp)) {
            sign_extend_integer_value(&int_val,
                                      (int)(tp->size * targ_char_bit));
          }  /* if */
          (void)memcpy(dest_storage, (a_byte*)&int_val, sizeof(int_val));
        }
        break;
      case tk_float:
        for (unsigned int i = 0; i < tp->size; i++) {
          if (*src_bitmap++ != all_bits_on) {
            initialized = FALSE;
          }  /* if */
          *dest_storage++ = *src_storage++;
        }  /* for */
        break;
      case tk_array:
        /* An array; step through each underlying element. */
        { a_targ_size_t  n_elems;
          a_type_ptr     etp;
          an_error_code  err_code = ec_no_error;
          err_code = get_element_and_type_from_array(tp, &etp, &n_elems);
          if (err_code == ec_no_error) {
            a_byte_count etp_n_bytes = value_bytes_for_type(ips, etp, &result);
            if (result) {
              check_assertion(n_elems < MAX_ARRAY_LENGTH);
              for (; n_elems > 0; n_elems--) {
                if (!translate_target_bytes_to_interpreter_object(ips, etp,
                                                       src_storage,
                                                       src_bitmap,
                                                       dest_storage,
                                                       dest_complete_object)) {
                  do_constexpr_fail(result);
                  break;
                }  /* if */
                if (!subobject_is_initialized(dest_storage,
                                              dest_complete_object)) {
                  initialized = FALSE;
                }  /* if */
                /* Move to the next element (both source and destination). */
                src_storage += etp->size;
                src_bitmap += etp->size;
                dest_storage += etp_n_bytes;
              }  /* for */
            }  /* if */
          } else {
            info_with_pos(err_code, &ips->position, ips);
            do_constexpr_fail(result);
          }  /* if */
        }
        break;
      case tk_class:
      case tk_struct:
        /* Visit each subobject of the class separately. */
        { a_byte_count offset;
          /* This is called to ensure that the class has been laid out. */
          (void)f_value_bytes_for_type(ips, tp, &result);
          /* Visit subobjects.  Note that the order in which the subobjects
             are visited is immaterial. */
          if (result) {
            a_field_ptr fp = tp->variant.class_struct_union.field_list;
            for (fp = next_alloc_field(fp);
                 fp != NULL;
                 fp = next_alloc_field(fp->next)) {
              if (fp->compiler_generated && !fp->is_anonymous_parent_object) {
                /* Ignore fields generated by prelowering. */
                continue;
              }  /* if */
              if (fp->is_bit_field) {
                /* For now, don't allow bitfields. */
                info_with_pos_type(ec_bitfields_not_allowed, type_pos(tp, ips),
                                   tp, ips);
                do_constexpr_fail(result);
                break;
              }  /* if */
              if (is_reference_type(fp->type)) {
                /* Non-static data members with reference type are not
                   allowed. */
                info_with_pos_type(ec_reference_type_not_allowed,
                                   type_pos(tp, ips), tp, ips);
                do_constexpr_fail(result);
                break;
              }  /* if */
              get_mapped_byte_count(&persistent_map, fp, offset);
              if (!translate_target_bytes_to_interpreter_object(ips,
                                                   skip_typerefs(fp->type),
                                                   src_storage + fp->offset,
                                                   src_bitmap + fp->offset,
                                                   dest_storage + offset,
                                                   dest_complete_object)) {
                do_constexpr_fail(result);
                break;
              }  /* if */
              if (!subobject_is_initialized(dest_storage + offset,
                                            dest_complete_object)) {
                initialized = FALSE;
              }  /* if */
            }  /* for */
          }  /* if */
          if (result) {
            a_base_class_ptr  bcp;
            /* Visit base classes (direct and virtual). */
            for (bcp = base_classes_of(tp); bcp != NULL; bcp = bcp->next) {
              get_mapped_byte_count(&persistent_map, bcp, offset);
              if (!translate_target_bytes_to_interpreter_object(ips,
                                                   skip_typerefs(bcp->type),
                                                   src_storage + bcp->offset,
                                                   src_bitmap + bcp->offset,
                                                   dest_storage + offset,
                                                   dest_complete_object)) {
                do_constexpr_fail(result);
                break;
              }  /* if */
              if (!subobject_is_initialized(dest_storage + offset,
                                            dest_complete_object)) {
                initialized = FALSE;
              }  /* if */
            }  /* for */
          }  /* if */
        }
        break;
#if GNU_VECTOR_TYPES_ALLOWED
      case tk_vector:
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
      case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if C99_IL_EXTENSIONS_SUPPORTED
      case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        /* These should eventually be supported but aren't yet. */
        info_with_pos_type(ec_unsupported_type_for_bit_cast, &ips->position,
                           tp, ips);
        do_constexpr_fail(result);
        break;
#if FIXED_POINT_ALLOWED
      case tk_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
      case tk_void:
      case tk_typeref:
      case tk_template_param:
      case tk_unknown:
      default:
        /* These types should not be encountered here. */
        do_constexpr_fail(result);
        unexpected_condition();
    }  /* switch */
  }  /* if */
  if (result && initialized) {
    /* Mark this subobject as initialized. */
    mark_subobject_initialized(orig_dest_storage, dest_complete_object);
    if (orig_dest_storage == dest_complete_object) {
      /* Mark the destination storage as fully initialized. */
      mark_complete_object_initialized(dest_complete_object);
    }  /* if */
    if (tp != NULL && 
        (is_immediate_class_type(tp) || tp->kind == (a_type_kind)tk_array)) {
      /* Mark the subobject as fully initialized. */
      mark_whole_subobject_initialized(ips, orig_dest_storage, tp,
                                       dest_complete_object);
    }  /* if */
  }  /* if */
  return result;
}  /* translate_target_bytes_to_interpreter_object */


static a_boolean do_constexpr_builtin_bit_cast(
                                        an_interpreter_state  *ips,
                                        an_expr_node_ptr      expr,
                                        a_byte                *result_storage,
                                        a_byte                *complete_object)
/*
Evaluate, if possible, the given __builtin_bit_cast expression.  If successful,
return TRUE and store the result in *result_storage (which is a subobject of
complete_object).  Otherwise, return FALSE and record a diagnostic in *ips.
*/
{
  a_boolean         result = TRUE;
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands, object;
  a_type_ptr        src_type, dst_type;
  a_byte_count      n_bytes;
  a_targ_size_t     type_size;
  a_byte            *src_result_storage, *target_result_storage;
  a_byte            *target_result_bitmap;

  check_assertion(arg1 != NULL && arg1->next != NULL &&
                  node_is(arg1, enk_type_operand));
  object = arg1->next;
  dst_type = skip_typerefs(arg1->variant.type_operand.type);
  src_type = skip_typerefs(object->type);
  type_size = size_of_type(src_type);
  check_assertion(type_size == size_of_type(dst_type));
  n_bytes = expr_result_size(ips, object, src_type, &result);
  if (result) {
    /* Interpret the source of the bit_cast into src_result_storage (which
       is in "interpreter" object format). */
    do_host_alignment(n_bytes);
    alloc_complete_object(ips, n_bytes, src_type, src_result_storage);
    if (!do_constexpr_expression(ips, object, src_result_storage,
                                 src_result_storage)) {
      result = FALSE;
    }  /* if */
  }  /* if */
  if (result) {
    /* Now transform the interpreter-formatted object format into the
       target representation. */
    alloc_stack_bytes(ips, (a_byte_count)type_size, target_result_storage);
    alloc_stack_bytes(ips, (a_byte_count)type_size, target_result_bitmap);
    memzero(target_result_storage, type_size);
    memzero((char*)target_result_bitmap, type_size);
    if (!translate_interpreter_object_to_target_bytes(ips, src_type,
                                                      src_result_storage,
                                                      src_result_storage,
                                                      target_result_storage,
                                                      target_result_bitmap,
                                                      expr)) {
      result = FALSE;
    }  /* if */
    /* Translate the target layout back to an interpreter object. */
    if (result &&
        !translate_target_bytes_to_interpreter_object(ips, dst_type,
                                                      target_result_storage,
                                                      target_result_bitmap,
                                                      result_storage,
                                                      complete_object)) {
      result = FALSE;
    }  /* if */
    if (result) {
      mark_whole_subobject_initialized(
                             ips, result_storage, dst_type, complete_object);
    }  /* if */
  }  /* if */
  return result;
}  /* do_constexpr_builtin_bit_cast */


static a_boolean do_constexpr_is_pointer_interconvertible_with_class(
                                        an_interpreter_state  *ips,
                                        an_expr_node_ptr      expr,
                                        a_byte                *result_storage,
                                        a_byte                *complete_object)
/*
Evaluate, if possible, the given __is_pointer_interconvertible_with_class (MS)
or __builtin_is_pointer_interconvertible_with_class (GCC) expression.  If
successful, return TRUE and store the result in *result_storage (which is a
subobject of complete_object).  Otherwise, return FALSE (a diagnostic will be
generated by the caller).
*/
{
  a_boolean         result = TRUE;
  an_expr_node_ptr  args = expr->variant.builtin_operation.operands;
  a_builtin_operation_kind
                    op = expr->variant.builtin_operation.kind;
  a_type_ptr        tp;
  an_expr_node_ptr  pm;

  /* Some type checking was already performed by the front end. */
  if (op == bok_is_pointer_interconvertible_with_class) {
    check_assertion(args != NULL && args->next != NULL &&
                    args->next->next == NULL &&
                    args->kind == (an_expr_node_kind)enk_type_operand);
    tp = skip_typerefs(args->variant.type_operand.type);
    pm = args->next;
  } else {
    check_assertion(op == bok_builtin_is_pointer_interconvertible_with_class &&
                    args != NULL && args->next == NULL);
    pm = args;
    tp = skip_typerefs(pm->type);
    if (type_is(tp, tk_ptr_to_member)) {
      tp = tp->variant.ptr_to_member.class_of_which_a_member;
    } else {
      result = FALSE;
    }  /* if */
  }  /* if */
  a_type_ptr        pm_type = skip_typerefs(pm->type);
  a_byte_count      n_bytes = value_bytes_for_type(ips, pm_type, &result);
  if (result &&
      is_class_struct_union_type(tp) &&
      type_is(pm_type, tk_ptr_to_member)) {
    a_byte *arg_bytes;
    alloc_complete_object(ips, n_bytes, tp, arg_bytes);
    if (!do_constexpr_expression(ips, pm, arg_bytes, arg_bytes)) {
      /* The argument did not have a constexpr value. */
      result = FALSE;
    } else {
      a_constexpr_ptr_to_mem  *pm_value;
      pm_value = (a_constexpr_ptr_to_mem*)arg_bytes;
      if (pm_value->is_ptr_to_mem_function ||
          pm_value->variant.field == NULL ||
          pm_value->variant.field->offset != 0 ||
          !class_symbol_supp(symbol_for(tp))->standard_layout) {
        /* Non-standard-layout classes and pointer-to-member functions
           elicit a "false" result.  If a field is designated but its
           offset is not zero, its address is not "interconvertible"
           with that of its parent object. */
        *(an_integer_value*)result_storage = zero_int;
      } else {
        *(an_integer_value*)result_storage = one_int;
      }  /* if */
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* do_constexpr_is_pointer_interconvertible_with_class */


static a_boolean do_constexpr_is_corresponding_member(
                                        an_interpreter_state  *ips,
                                        an_expr_node_ptr      expr,
                                        a_byte                *result_storage,
                                        a_byte                *complete_object)
/*
Evaluate, if possible, the given __is_corresponding_member (MS) or
__builtin_is_corresponding_member (GCC) expression.  If successful, return TRUE
and store the result in *result_storage (which is a subobject of
complete_object).  Otherwise, return FALSE (the caller will generate a
diagnostic).
*/
{
  a_boolean         result = TRUE;
  an_expr_node_ptr  pm_args, arg = expr->variant.builtin_operation.operands;
  a_type_ptr        tp1, tp2;
  a_builtin_operation_kind
                    op = expr->variant.builtin_operation.kind;

  /* Some type checking was already performed by the front end. */
  check_assertion(arg != NULL && arg->next != NULL);
  if (op == bok_is_corresponding_member) {
    /* Four arguments, first two are types, next two are pointer-to-members. */
    check_assertion(arg->next->next != NULL &&
                    arg->next->next->next != NULL &&
                    arg->next->next->next->next == NULL &&
                    arg->kind == (an_expr_node_kind)enk_type_operand &&
                    arg->next->kind == (an_expr_node_kind)enk_type_operand);
    tp1 = skip_typerefs(arg->variant.type_operand.type),
    tp2 = skip_typerefs(arg->next->variant.type_operand.type);
    pm_args = arg->next->next;
  } else {
    /* Two pointer-to-member arguments. */
    check_assertion(op == bok_builtin_is_corresponding_member &&
                    arg->next->next == NULL);
    pm_args = arg;
  }  /* if */
  an_expr_node_ptr  pm1 = pm_args,
                    pm2 = pm1->next;
  a_type_ptr        pm_type1 = skip_typerefs(pm1->type),
                    pm_type2 = skip_typerefs(pm2->type);
  a_byte_count      n_bytes = value_bytes_for_type(ips, pm_type1, &result);
  if (op == bok_builtin_is_corresponding_member) {
    /* Get class types from the pointer-to-member arguments. */
    if (type_is(pm_type1, tk_ptr_to_member)) {
      tp1 = pm_type1->variant.ptr_to_member.class_of_which_a_member;
    } else {
      result = FALSE;
    }  /* if */
    if (type_is(pm_type2, tk_ptr_to_member)) {
      tp2 = pm_type2->variant.ptr_to_member.class_of_which_a_member;
    } else {
      result = FALSE;
    }  /* if */
  }  /* if */
  if (result &&
      is_class_struct_union_type(tp1) &&
      is_class_struct_union_type(tp2) &&
      type_is(pm_type1, tk_ptr_to_member) &&
      type_is(pm_type2, tk_ptr_to_member)) {
    a_byte *pm1_bytes, *pm2_bytes;
    alloc_complete_object(ips, n_bytes, pm_type1, pm1_bytes);
    alloc_complete_object(ips, n_bytes, pm_type2, pm2_bytes);
    if (!do_constexpr_expression(ips, pm1, pm1_bytes, pm1_bytes) ||
        !do_constexpr_expression(ips, pm2, pm2_bytes, pm2_bytes)) {
      /* The arguments did not have a constexpr value. */
      result = FALSE;
    } else {
      a_constexpr_ptr_to_mem  *pm_value1, *pm_value2;
      pm_value1 = (a_constexpr_ptr_to_mem*)pm1_bytes;
      pm_value2 = (a_constexpr_ptr_to_mem*)pm2_bytes;
      if (pm_value1->is_ptr_to_mem_function ||
          pm_value2->is_ptr_to_mem_function ||
          pm_value1->variant.field == NULL ||
          pm_value2->variant.field == NULL ||
          !class_symbol_supp(symbol_for(tp1))->standard_layout ||
          !class_symbol_supp(symbol_for(tp2))->standard_layout ||
          pm_value1->variant.field->offset !=
                                            pm_value2->variant.field->offset ||
          pm_value1->variant.field->offset >=
                                     common_initial_sequence_limit(tp1, tp2)) {
        /* Non-standard-layout classes and pointer-to-member functions
           elicit a "false" result.  Members "correspond" if they are within
           the "common initial sequence" and have the same offset. */
        *(an_integer_value*)result_storage = zero_int;
      } else {
        *(an_integer_value*)result_storage = one_int;
      }  /* if */
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* do_constexpr_is_corresponding_member */


static a_boolean do_constexpr_builtin_operation(
                                       an_interpreter_state  *ips,
                                       an_expr_node_ptr      orig_expr,
                                       a_byte                *result_storage,
                                       a_byte                *complete_object)
/*
Interpret the given builtin expression in the given interpreter context.  If
successful return TRUE and store the result at *result_storage (which is
storage within the given complete object).  Otherwise, return FALSE and update
*ips accordingly.
*/
{
  a_boolean         result = TRUE;
  an_expr_node_ptr  expr = skip_parens(orig_expr);

  switch (expr->variant.builtin_operation.kind) {
    case bok_offsetof:
      if (!do_constexpr_offsetof(ips, expr,
                                 result_storage, complete_object)) {
        do_constexpr_fail(result);
      }  /* if */
      break;
    case bok_intaddr:
      if (!do_constexpr_intaddr(ips, expr, result_storage, complete_object)) {
        do_constexpr_fail(result);
      }  /* if */
      break;
    case bok_builtin_addressof:
      { an_expr_node_ptr  opnd1 = expr->variant.builtin_operation.operands;
        if (opnd1->is_lvalue || opnd1->is_xvalue) {
          if (!do_constexpr_expression(ips, opnd1, result_storage,
                                       complete_object)) {
            do_constexpr_fail(result);
          }  /* if */
        } else {
          unexpected_condition();
        }  /* if */
      }
      break;
    case bok_builtin_bit_cast:
      if (!do_constexpr_builtin_bit_cast(ips, expr, result_storage,
                                         complete_object)) {
        do_constexpr_fail(result);
      }  /* if */
      break;
    case bok_is_pointer_interconvertible_with_class:
    case bok_builtin_is_pointer_interconvertible_with_class:
      if (!do_constexpr_is_pointer_interconvertible_with_class(ips, expr,
                                            result_storage, complete_object)) {
        do_constexpr_fail(result);
      }  /* if */
      break;
    case bok_is_corresponding_member:
    case bok_builtin_is_corresponding_member:
      if (!do_constexpr_is_corresponding_member(ips, expr, result_storage,
                                                complete_object)) {
        do_constexpr_fail(result);
      }  /* if */
      break;
    default:
      do_constexpr_fail(result);
      info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                    &expr->position, ips);
  }  /* switch */
  return result;
}  /* do_constexpr_builtin_operation */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void load_uuid_string_into_class_object(a_const_char  *uuid_str,
                                               a_type_ptr    dest_tp,
                                               a_byte        *result_storage,
                                               a_byte        *complete_object)
/*
A UUID type has the form:

  struct _GUID {
    unsigned char Data1;
    int Data2;
    int Data3;
    char Data4[8];
  };

and the given string has the form "HHHHHHHH-HHHH-HHHH-HHHH-HHHHHHHHHHHH" where
the Hs are hexadecimal digits.  Normally, result_storage/complete_object
designates an object of that type, and this function sets the first three
data members to the integer value of the first three sequences of digits,
respectively.  The remaining eight bytes are then set to the values indicated
by the remaining pairs of digits (over two sequences).  I.e., a total of 11
values are transferred.

However, MSVC appears to allow quite a bit of variance in the destination type 
(e.g., fields can be dropped and types can be changed).  We approximate that
behavior by transferring the values as long as we find a sequence of integral
fields.  E.g., if the type were:

  struct _FakeGUID {
    unsigned char bytes[10];
  };

then we load the 10 bytes with the first ten values that would normally be
transferred (retaining only the least significant bytes) and discard the
last (eleventh) value.
*/
{
  a_field_ptr           fp = dest_tp->variant.class_struct_union.field_list;
  a_type_ptr            ftp;
  int                   i = 0, k, n, n_digits;
  an_integer_value      *int_storage;
  a_byte_count          offset;
  a_boolean             load_bytes = FALSE;
  a_host_large_integer  host_val;

  fp = next_alloc_field(fp);
  for (; fp != NULL; fp = next_alloc_field(fp->next)) {
    get_mapped_byte_count(&persistent_map, fp, offset);
    int_storage = (an_integer_value*)(result_storage+offset);
    /* Only load values into integers or arrays of integers. */
    ftp = skip_typerefs(fp->type);
    if (type_is(ftp, tk_array)) {
      n = (int)num_array_elements(ftp);
      ftp = underlying_array_element_type(ftp);
      ftp = skip_typerefs(ftp);
    } else {
      n = 1;
    }  /* if */
    if (!is_integral_type(ftp)) break;
    /* If this is an array, loop through every element. */
    for (k = 0; k<n && i<11; ++k, ++i) {
      host_val = 0;
      n_digits = 0;
      /* Accumulate a value from a hexadecimal digit string.  Stop after two
         digits if we're loading bytes. */
      while (isxdigit(*uuid_str)) {
        host_val = host_val*16 + hexvalue(*uuid_str);
        ++uuid_str;
        ++n_digits;
        if (n_digits == 2 && load_bytes) break;
      }  /* if */
      set_integer_value(int_storage, host_val);
      mark_subobject_initialized((a_byte*)int_storage, complete_object);
      if (i == 2) {
        /* After loading three integer values, load pairs of digits only. */
        load_bytes = TRUE;
      }  /* if */
      while (!isxdigit(*uuid_str)) {
        if (*uuid_str == '\0') goto done;
        ++uuid_str;
      }  /* while */
      ++int_storage;
    }  /* for */
  }  /* for */
done:;
}  /* load_uuid_string_into_class_object */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean get_value_from_address_constant(
                                       an_interpreter_state  *ips,
                                       a_constant_ptr        addr_con,
                                       ARG_UNUSED a_type_ptr tp,
                                       a_byte                *result_storage,
                                       a_byte                *complete_object)
/*
If addr_con is the address of a constant, copy the constant value into the
interpreter storage designated by result_storage and complete_object (that
location expects a value of type tp); return TRUE if successful.  Otherwise,
return FALSE.
*/
{
  a_boolean  result;

  if (!constant_is(addr_con, ck_address)) {
    result = FALSE;
  } else {
    a_constant_ptr  val_con = local_constant();
    if (constant_value_at_address(addr_con, val_con)) {
      /* Copy the constant value. */
      result = copy_val_from_constant(ips, val_con,
                                      result_storage, complete_object);
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (addr_con->variant.address.kind ==
                                           (an_address_base_kind)abk_uuidof &&
               is_immediate_class_type(tp)) {
      /* Obtain the UUID string associated with the addr_con entry (via the
         type carrying that UUID), and decode it into a sequence of integer
         values. */
      a_type_ptr  uuid_tp = addr_con->variant.address.variant.type;
      if (uuid_tp == NULL) {
        result = FALSE;
      } else {
        a_const_char  *uuid_str = uuid_string_of_type(uuid_tp);
        if (uuid_str == NULL) {
          result = FALSE;
        } else {
          load_uuid_string_into_class_object(uuid_str, tp,
                                             result_storage, complete_object);
          result = TRUE;
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (is_immediate_class_type(tp) &&
               tp->variant.class_struct_union.is_empty_class &&
               is_trivially_copy_constructible_type(tp)) {
      /* An empty class type object with trivial copy semantics is considered
         "constant". */
      init_subobject_to_zero(ips, result_storage, tp, complete_object);
      result = TRUE;
    } else {
      result = FALSE;
    }  /* if */
    release_local_constant(&val_con);
  }  /* if */
  return result;
}  /* get_value_from_address_constant */


static a_boolean do_glvalue_to_prvalue(an_interpreter_state  *ips,
                                       an_expr_node_ptr      expr,
                                       a_type_ptr            tp,
                                       a_constexpr_address   *cap,
                                       a_byte_count          n_bytes,
                                       a_byte                *result_storage,
                                       a_byte                *complete_object)
/*
Load a value of type tp (interpreter size n_bytes) from the address described
by *cap into the storage pointed to by result_storage (complete_object points
to the enclosing complete object).  expr is the expression that requires this
"load" operation.  It is usually a prvalue, but it may be a glvalue whose
conversion to an rvalue is forced externally.
*/
{
  a_boolean  result;

  if (cannot_dereference(cap)) {
    /* This address cannot be dereferenced. */
    do_constexpr_fail(result);
    info_one_past_end_of_array(cap, expr, ips);
  } else if (is_runtime_data_address(cap)) {
    if (!get_value_from_address_constant(
           ips, cap->variant.addr_con, tp, result_storage, complete_object)) {
      /* Not a compile-time constant value. */
      do_constexpr_fail(result);
      info_with_pos(ec_constexpr_access_to_runtime_storage,
                    &expr->position, ips);
    } else {
      result = TRUE;
    }  /* if */
  } else if (!in_live_set(&ips->live_set, cap->alloc_seq_number)) {
    /* An attempt to access storage that has expired. */
    do_constexpr_fail(result);
    info_with_pos(ec_constexpr_access_to_expired_storage, &expr->position,
                  ips);
  } else if (cap->address == NULL) {
    do_constexpr_fail(result);
    info_with_pos(ec_constexpr_null_dereference, &expr->position, ips);
  } else if (expr->volatile_fetch) {
    do_constexpr_fail(result);
    info_with_pos(ec_constexpr_volatile_fetch, &expr->position, ips);
  } else if (!is_initialized(cap) && !is_immediate_class_type(tp)) {
    do_constexpr_fail(result);
    info_with_pos(ec_object_not_initialized, &expr->position, ips);
  } else if (is_variant_path(cap) &&
             !check_variant_path(ips, cap, &expr->position)) {
    /* An attempt to dereference an inactive variant path. */
    do_constexpr_fail(result);
  } else {
    if (is_immediate_class_type(tp) || type_is(tp, tk_array)) {
      result = constexpr_copy_object(ips, tp, &expr->position,
                                     cap->address, cap->complete_object,
                                     result_storage, complete_object);
    } else {
      result = TRUE;
      (void)memcpy(result_storage, value_bytes_at(cap), size_t_arg(n_bytes));
      if (type_is(tp, tk_pointer)) {
        /* If a pointer value is loaded from a glvalue, give the copy its own
           address structures (so the original will not be freed when the copy
           is freed). */
        copy_address_structures(result_storage);
      }  /* if */
    }  /* if */
    if (result_storage == complete_object) {
      /* Mark the destination storage as fully initialized. */
      mark_complete_object_initialized(complete_object);
    }  /* if */
  }  /* if */
  return result;
}  /* do_glvalue_to_prvalue */


static a_boolean check_boolean_condition(an_interpreter_state  *ips,
                                         a_byte                *value,
                                         an_expr_node_ptr      expr,
                                         a_type_ptr            tp,
                                         a_boolean             *p_cond)
/*
value is the result (or a sub-result in vector cases) of evaluating expr, which
has the given type (after skipping typerefs).  The expression is used as a
boolean condition: If that condition can be determined to be true or false, set
*p_cond to that condition and return TRUE.  Otherwise, return FALSE and record
a diagnostic.
*/
{
  a_boolean  result = TRUE;

  switch (tp->kind) {
    case tk_integer:
      { a_host_large_integer  bool_val;
        a_boolean             ovflo;
        get_int_val_from(value, tp, bool_val, ovflo);
        *p_cond = ovflo || bool_val;
      }
      break;
    case tk_pointer:
      { a_constexpr_address  *cap = (a_constexpr_address*)value;
        if (is_runtime_data_address(cap)) {
          a_constant_ptr  addr_con = cap->variant.addr_con;
          if (constant_is(addr_con, ck_integer)) {
            a_boolean  is_signed = FALSE;
            if (cmp_integer_values(&addr_con->variant.integer_value, is_signed,
                                   &zero_int, is_signed) != 0) {
              *p_cond = TRUE;
            } else {
              *p_cond = FALSE;
            }  /* if */
          } else {
            if (constant_is(addr_con, ck_address)) {
              /* Check for the case of a (non-weak) variable address: That is
                 a known true value. */
              if (addr_con->variant.address.kind ==
                                         (an_address_base_kind)abk_variable) {
                a_variable_ptr  vp = addr_con->variant.address
                                              .variant.variable;
                if (vp != NULL if_gnu_allowed(&& !vp->is_weak)) {
                  *p_cond = TRUE;
                  break;
                }  /* if */
              }  /* if */
            }  /* if */
            *p_cond = FALSE;
            do_constexpr_fail(result);
            info_with_pos(ec_constexpr_access_to_runtime_storage,
                          &expr->position, ips);
          }  /* if */
        } else if (is_function_address(cap) || cap->address != NULL) {
          *p_cond = TRUE;
        } else {
          *p_cond = FALSE;
        }  /* if */
      }
      break;
    case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      { a_boolean  unord;
        if (fp_compare(tp->variant.float_kind,
                       fp_value(value),
                       &zero_flt[(int)tp->variant.float_kind],
                       &unord) == 0) {
          *p_cond = FALSE;
        } else {
          *p_cond = TRUE;
        }  /* if */
      }
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
      { a_boolean  unord;
        if (fp_compare(tp->variant.float_kind,
                       &cx_value(value)->real,
                       &zero_flt[(int)tp->variant.float_kind],
                       &unord) == 0 &&
            fp_compare(tp->variant.float_kind,
                       &cx_value(value)->imag,
                       &zero_flt[(int)tp->variant.float_kind],
                       &unord) == 0) {
          *p_cond = FALSE;
        } else {
          *p_cond = TRUE;
        }  /* if */
      }
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_nullptr:
      *p_cond = FALSE;
      break;
    case tk_ptr_to_member:
      { a_constexpr_ptr_to_mem  *pm = (a_constexpr_ptr_to_mem*)value;
        if ((pm->is_ptr_to_mem_function ? (void*)pm->variant.routine
                                        : (void*)pm->variant.field) == NULL) {
          *p_cond = FALSE;
        } else {
          *p_cond = TRUE;
        }  /* if */
      }
      break;
    default:
      *p_cond = TRUE;
      do_constexpr_fail(result);
  }  /* switch */
  return result;
}  /* check_boolean_condition */
 

static a_boolean normalize_runtime_address_if_possible(
                                                    a_constexpr_address *ptr1,
                                                    a_constexpr_address *ptr2)
/*
One of ptr1 and ptr1 is a run-time address and the other is not.  If the
run-time address has an associated zero ck_integer constant, replace it by an
equivalent interpreter address and return TRUE.  Otherwise, return FALSE.
This is used to compare pointer values (null pointer values in particular).
*/
{
  a_boolean             compat = FALSE, ovfl;
  a_host_large_integer  val;

  if (is_runtime_data_address(ptr1)) {
    a_constant_ptr  cp = ptr1->variant.addr_con;
    if (constant_is(cp, ck_integer)) {
      conv_integer_value_to_host_large_integer(&cp->variant.integer_value,
                                               /*is_signed=*/FALSE, &val,
                                               &ovfl);
      if (!ovfl && val == 0) {
        clear_address(ptr1, (a_byte*)0);
        compat = TRUE;
      }  /* if */
    }  /* if */
  } else if (is_runtime_data_address(ptr2)) {
    a_constant_ptr  cp = ptr2->variant.addr_con;
    if (constant_is(cp, ck_integer)) {
      conv_integer_value_to_host_large_integer(&cp->variant.integer_value,
                                               /*is_signed=*/FALSE, &val,
                                               &ovfl);
      if (!ovfl && val == 0) {
        clear_address(ptr2, (a_byte*)0);
        compat = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return compat;
}  /* normalize_runtime_address_if_possible */


#define compatible_address_kinds(addr1, addr2)                               \
  (is_runtime_data_address(ptr1) == is_runtime_data_address(ptr2) ||         \
   normalize_runtime_address_if_possible(ptr1, ptr2))


static a_boolean acceptable_lvalue_conversion(an_interpreter_state  *ips,
                                              a_constexpr_address   *cap,
                                              a_type_ptr            src_type,
                                              a_type_ptr            dst_type)
/*
Return TRUE if converting the given lvalue of type src_type to dst_type using
an eok_lvalue_cast or eok_lvalue_adjust node is acceptable in a constant-
expression.  The caller has already determined that the types are different
(ignoring qualifiers) and top-level typerefs have been stripped from both
types.  *cap may be updated to reflect an extended lifetime in the given
interpreter context.
*/
{
  a_boolean  valid = FALSE;

  if (src_type->kind == (a_type_kind)tk_routine &&
      dst_type->kind == (a_type_kind)tk_pointer &&
      skip_typerefs(dst_type->variant.pointer.type) == src_type) {
    /* Function pointer decay is okay. */
    valid = TRUE;
  } else if (dst_type->kind == (a_type_kind)tk_array &&
             is_array_element(cap) &&
             !dst_type->variant.array.is_template_dependent_size_array &&
             !dst_type->variant.array.is_variable_size_array) {
    a_type_ptr  etp = skip_typerefs(dst_type->variant.array.element_type);
    if (dst_type->variant.array.variant.number_of_elements == cap->length &&
        identical_types_ignoring_qualifiers(src_type, etp)) {
      /* The front end produces IL like the following:
          [lvalue] operator: lvalue adjust, result type: array [1] of const int
            [lvalue] operator: *, result type: const int
              operator: array-decay, result type: ptr to const int
                constant: value = {}
         when binding a prvalue array to a reference.  In cases like these,
         also mark the prvalue referred to as having an extended lifetime if
         we are not in a function scope. */
      valid = TRUE;
      if (ips->curr_call_frame == NULL) {
        cap->flags |= CA_LIFETIME_EXTENDED;
      }  /* if */
      cap->flags &= ~CA_ARRAY_ELEMENT;
    }  /* if */
  }  /* if */
  return valid;
}  /* acceptable_lvalue_conversion */


static a_boolean do_constexpr_bound_expr(
                                       an_interpreter_state  *ips,
                                       an_expr_node_ptr      var_expr,
                                       a_byte                *result_storage,
                                       a_byte                *complete_object)
/*
The given expression is an enk_variable node for a variable with an
initk_binding initializer.  Interpret that initializer.  If successful return
TRUE and store the result at *result_storage (which is storage within the
given complete object).  Otherwise, return FALSE and update *ips accordingly.
*/
{
  a_boolean         result = TRUE;
  a_variable_ptr    var = node_variable(var_expr);
  an_expr_node_ptr  bound_expr = var->initializer.bound_expr;

  if (var_expr->is_lvalue || var_expr->is_xvalue) {
    if (!do_constexpr_expression(ips, bound_expr,
                                 result_storage, complete_object)) {
      do_constexpr_fail(result);
    }  /* if */
  } else {
    a_type_ptr  tp = skip_typerefs(var_expr->type);
    a_byte      *lvalue;
    alloc_complete_object(ips, sizeof(a_constexpr_address), tp, lvalue);
    if (do_constexpr_expression(ips, bound_expr, lvalue, lvalue)) {
      a_byte_count  n_bytes = value_bytes_for_type(ips, tp, &result);
      if (result &&
          !do_glvalue_to_prvalue(ips, var_expr, tp,
                                 (a_constexpr_address*)lvalue,
                                 n_bytes, result_storage, complete_object)) {
        do_constexpr_fail(result);
      }  /* if */
    } else {
      do_constexpr_fail(result);
    }  /* if */
  }  /* if */
  return result;
}  /* do_constexpr_bound_expr */


static a_boolean do_constexpr_lambda(an_interpreter_state *ips,
                                     a_dynamic_init_ptr   dip,
                                     a_source_position    *pos,
                                     a_byte               *result_storage,
                                     a_byte               *complete_object)
/*
Interpret dip, which must be a dik_lambda initializer.  Return TRUE if no error
occurred; otherwise, return FALSE and update *ips accordingly.  The resulting
closure object is placed at the location indicated by result_storage, which
is within the given complete_object.
*/
{
  a_lambda_ptr          lambda;
  a_lambda_capture_ptr  cap;
  a_constant_ptr        cp;
  a_constant_ptr        field_con;
  a_boolean             result = TRUE;

  check_assertion(dyn_init_is(dip, dik_lambda));
  lambda = dip->variant.constant.lambda;
  if (!dip->variant.constant.non_constant) {
    /* Simple constant initializer. */
    result = copy_val_from_constant(ips, dip->variant.constant.ptr,
                                    result_storage, complete_object);
  } else {
    cp = dip->variant.constant.ptr;
    /* Initialize each field of the closure object from the corresponding
       capture. */
    for (cap = lambda->capture_list,
                              field_con = cp->variant.aggregate.first_constant;
         result && cap != NULL && field_con != NULL;
         cap = cap->next, field_con = field_con->next) {
      a_field_ptr         fp = cap->closure_field;
      a_byte_count        field_offset;
      a_byte              *dst_bytes;
      a_dynamic_init_ptr  sub_dip;
      a_type_ptr          ftp = skip_typerefs(fp->type);
      /* Determine the offset of this field within the closure object's
         storage. */
      get_mapped_byte_count(&persistent_map, fp, field_offset);
      dst_bytes = result_storage+field_offset;
      mark_complete_class_object_if_needed(ftp, dst_bytes);
      if (cap->is_init_capture) {
        /* Interpret the initializer for the capture. */
        sub_dip = cap->captured.initializer;
        if (dyn_init_is(sub_dip, dik_zero) || dyn_init_is(sub_dip, dik_none)) {
          /* Just zero the storage (for the dik_zero case) and record the
             derivation structure (which is needed even for the dik_none
             case). */
          init_subobject_to_zero(ips, dst_bytes, ftp, complete_object);
        } else {
          a_constexpr_address  dst_addr;
          set_active_address(ips, &dst_addr, dst_bytes, complete_object);
          if (do_constexpr_dynamic_init(ips, sub_dip, pos, &dst_addr)) {
            mark_subobject_initialized(dst_bytes, complete_object);
          } else {
            result = FALSE;
          }  /* if */
        }  /* if */
      } else if (cap->captured.variable == NULL ||
                 (cap->capture_info.source_closure_field != NULL &&
                  !cap->captured.variable->is_this_parameter)) {
        /* This is a capture of "this" or "*this" in a field initializer or a
           capture of an enclosing lambda's capture. */
        check_assertion(constant_is(field_con, ck_dynamic_init));
        sub_dip = field_con->variant.dynamic_init.ptr;
        if (dyn_init_is(sub_dip, dik_bitwise_copy) &&
            sub_dip->variant.bitwise_copy.source == NULL) {
          /* An implicit bitwise copy from a field of the closure associated
             with the enclosing call (which is of a lambda call operator).
             The "this" value was recorded using a call to
             set_up_param_ref_for_this_ptr. */
          a_field_ptr  src_fp = cap->capture_info.source_closure_field;
          a_byte       *this_bytes, *src_bytes;
          a_constexpr_address
                       *src_addr;
          check_assertion(src_fp != NULL);
          get_mapped_byte_count(&persistent_map, src_fp, field_offset);
          get_stack_bytes(ips, &ips->curr_call_frame, this_bytes);
          if (this_bytes == NULL) {
            do_constexpr_fail(result);
            info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                          pos, ips);
            break;
          }  /* if */
          src_addr = (a_constexpr_address*)this_bytes;
          src_bytes = src_addr->address+field_offset;
          if (!constexpr_copy_object(ips, src_fp->type, pos,
                                     src_bytes, src_addr->complete_object,
                                     dst_bytes, complete_object)) {
            result = FALSE;
          } else {
            mark_complete_class_object_if_needed(src_fp->type, dst_bytes);
          }  /* if */
        } else {
          a_constexpr_address  dst_addr;
          set_active_address(ips, &dst_addr, dst_bytes, complete_object);
          if (do_constexpr_dynamic_init(ips, sub_dip, pos, &dst_addr)) {
            mark_subobject_initialized(dst_bytes, complete_object);
            mark_complete_class_object_if_needed(ftp, dst_bytes);
          } else {
            result = FALSE;
          }  /* if */
        }  /* if */
      } else {
        /* An ordinary simple capture. */
        a_byte              *var_storage;
        a_variable_ptr      vp = cap->captured.variable;
        a_type_ptr          vtp = vp->type, uvtp = skip_typerefs(vtp);
        a_boolean           ref_case = FALSE;
        check_assertion(constant_is(field_con, ck_dynamic_init));
        sub_dip = field_con->variant.dynamic_init.ptr;
        get_stack_bytes(ips, vp, var_storage);
        if (type_is(uvtp, tk_pointer)) {
          if (uvtp->variant.pointer.is_reference ||
              (vp->is_this_parameter && !cap->capture_by_reference)) {
            /* For a reference variable, capture the referenced value.
               Similarly for [*this] capture. */
              vtp = uvtp->variant.pointer.type;
              uvtp = skip_typerefs(vtp);
              ref_case = TRUE;
          }  /* if */
        }  /* if */
        /* Get the value of the captured variable. */
        if (var_storage != NULL) {
          /* We already have the variable's value in interpreter storage. */
        } else if (cap->capture_by_reference) {
          /* We are not capturing the variable's value, only its address. */
        } else if (is_volatile_qualified_type(vtp)) {
          /* Capturing a volatile variable prevents the lambda from being
             used in a constant expression. */
          info_with_pos(ec_constexpr_volatile_fetch, pos, ips);
          do_constexpr_fail(result);
        } else {
          /* This is the first interpreter reference to the variable's
             value, so copy it into the associated interpreter storage. */
          a_constant_ptr var_con = var_constant_value(vp);
          if (var_con != NULL) {
            var_storage = do_constexpr_alloc_variable(ips, vp, &result);
            if (result) {
              result = copy_val_from_constant(ips, var_con, var_storage,
                                              var_storage);
            }  /* if */
          } else {
            /* The variable does not have a constant value. Report the
               appropriate error. */
            if (vp->is_this_parameter) {
              info_with_pos(ec_star_this_not_constant_valued, pos,
                            ips);
            } else if (symbol_for(vp) == NULL) {
              /* This can happen with synthesized variables such as the one
                 generated for __func__. */
              info_with_pos(ec_constexpr_access_to_runtime_storage,
                            pos, ips);
            } else {
              info_with_pos_sym(ec_variable_not_constant_valued,
                                pos, symbol_for(vp), ips);
            }  /* if */
            do_constexpr_fail(result);
          }  /* if */
        }  /* if */
        /* If the capture has a constant value, copy it into the closure
           object field. */
        if (result) {
          a_constexpr_address  var_addr;
          if (ref_case && !cap->capture_by_reference) {
            /* When capturing a reference variable by value, the referenced
               object must be copied instead. */
            var_addr = *(a_constexpr_address*)var_storage;
            if (is_runtime_data_address(&var_addr)) {
              info_with_pos(ec_constexpr_access_to_runtime_storage,
                            pos, ips);
              do_constexpr_fail(result);
              break;
            } else if (!is_function_address(&var_addr) &&
                       !is_initialized(&var_addr)) {
              info_with_pos(ec_object_not_initialized, pos, ips);
              do_constexpr_fail(result);
              break;
            }  /* if */
            var_storage = var_addr.address;
          } else {
            clear_address(&var_addr, var_storage);
          }  /* if */
          if (dyn_init_is(sub_dip, dik_bitwise_copy)) {
            check_assertion(sub_dip->variant.bitwise_copy.source == NULL);
            if (!constexpr_copy_object(ips, ftp, pos,
                                       var_storage, var_addr.complete_object,
                                       dst_bytes, complete_object)) {
              do_constexpr_fail(result);
            }  /* if */
          } else if (dyn_init_is(sub_dip, dik_constructor)) {
            a_constexpr_address  dst_addr;
            set_active_address(ips, &dst_addr, dst_bytes, complete_object);
            if (!do_constexpr_ctor(ips, sub_dip, pos, &dst_addr, &var_addr)) {
              do_constexpr_fail(result);
            }  /* if */
          } else if (dyn_init_is(sub_dip, dik_expression)) {
            if (!do_constexpr_expression(ips, sub_dip->variant.expression,
                                         dst_bytes, complete_object)) {
              do_constexpr_fail(result);
            }  /* if */
          } else if (dyn_init_is(sub_dip, dik_none) ||
                     (dyn_init_is(sub_dip, dik_constant) &&
                      is_error_constant(sub_dip->variant.constant.ptr))) {
            /* This can happen in error cases. */
            expect_error();
            ips->input_error = TRUE;
            do_constexpr_fail(result);
          } else {
            unexpected_condition();
          }  /* if */
          if (result) {
            mark_subobject_initialized(dst_bytes, complete_object);
            mark_complete_class_object_if_needed(ftp, dst_bytes);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    /* Make sure we either bailed out because of an error or handled all
       the captured values. */
    check_assertion(!result || (cap == NULL && field_con == NULL));
  }  /* if */
  mark_subobject_initialized(result_storage, complete_object);
  return result;
}  /* do_constexpr_lambda */


a_subobject_path_ptr* last_subobject_path_link(a_constant_ptr  con)
/*
Return a pointer to the last "link" pointer of the subobject path of con (which
must be a ck_address entry).
*/
{
  a_subobject_path_ptr  *p_link;

  check_assertion(constant_is(con, ck_address));
  p_link = &con->variant.address.subobject_path;
  while (*p_link != NULL) {
    p_link = &(*p_link)->next;
  }  /* while */
  return p_link;
}  /* last_subobject_path_link */


a_subobject_path_ptr get_trailing_subobject_path_entry(
                                                a_constant_ptr  con,
                                                a_boolean       is_offset,
                                                a_boolean       is_base_class)
/*
con is a ck_address entry and either is_offset or is_base_class is TRUE.  If
the last entry on the subobject path for the given constant matches the given
flags, return that last entry.  Otherwise append an entry matching the given
flags and return that newly allocated entry.
*/
{
  a_subobject_path_ptr  *end_path;

  check_assertion(constant_is(con, ck_address) &&
                  (is_offset ? !is_base_class : is_base_class));
  end_path = &con->variant.address.subobject_path;
  for (; *end_path != NULL; end_path = &(*end_path)->next) {
    if ((*end_path)->next == NULL &&
        (is_offset ? (*end_path)->is_offset : (*end_path)->is_base_class)) {
      /* The path already ends in the right kind of entry. */
      break;
    }  /* if */
  }  /* for */
  if (*end_path == NULL) {
    *end_path = alloc_subobject_path();
    if (is_offset) {
      (*end_path)->is_offset = TRUE;
      (*end_path)->variant.ptr_offset = 0;
    } else {
      (*end_path)->is_base_class = TRUE;
      (*end_path)->variant.base_class = NULL;
    }  /* if */
  }  /* if */
  return *end_path;
}  /* get_trailing_subobject_path_entry */


static a_boolean offset_runtime_address(an_interpreter_state  *ips,
                                        a_source_position     *diag_pos,
                                        a_constexpr_address   *cap,
                                        a_host_large_integer  count,
                                        a_byte_count          elem_size,
                                        a_boolean             subtract)
/*
Offset the given address constant by count*elem_size (if subtract is FALSE) or
-count*elem_size (if subtract is TRUE).  That address constant is usually a
ck_address entry, but it also can be a ck_integer entry (e.g., resulting from
a construct like "(char*)0x1234").  In case of overflow, record a diagnostic
for the given position and return FALSE.  Otherwise, return TRUE.
*/
{
  an_integer_value  delta, tmp;
  a_boolean         ovflo;
  a_constant_ptr    addr_con = cap->variant.addr_con;

  /* Make a copy of the constant since we're about to modify it. */
  addr_con = make_interpreter_copy_of_constant(ips, addr_con);
  cap->variant.addr_con = addr_con;
  set_integer_value(&delta, count);
  set_unsigned_integer_value(&tmp, (a_host_large_unsigned)elem_size);
  multiply_integer_values(&delta, &tmp, /*is_signed=*/TRUE, &ovflo);
  if (ovflo) {
    /* Nothing more to do. */
  } else if (constant_is(addr_con, ck_address)) {
    a_subobject_path_ptr  spp;
    spp = get_trailing_subobject_path_entry(addr_con, /*is_offset=*/TRUE,
                                            /*is_base_class=*/FALSE);
    spp->variant.ptr_offset += subtract ? -count : count;
    set_integer_value(&tmp,
                      (a_host_large_integer)addr_con->variant.address.offset);
    if (subtract) {
      subtract_integer_values(&tmp, &delta, /*is_signed=*/TRUE, &ovflo);
    } else {
      add_integer_values(&tmp, &delta, /*is_signed=*/TRUE, &ovflo);
    }  /* if */
    if (!ovflo) {
      addr_con->variant.address.offset =
                      value_of_integer_value(&tmp, /*is_signed=*/TRUE, &ovflo);
    }  /* if */
  } else {
    an_integer_value  *addr_val = &addr_con->variant.integer_value;
    a_boolean         addr_is_signed;
    check_assertion(constant_is(addr_con, ck_integer));
    addr_is_signed = int_constant_is_signed(addr_con);
    if (subtract) {
      subtract_mixed_signed_integer_values(addr_val, addr_is_signed,
                                           &delta, /*op_2_signed=*/TRUE,
                                           &ovflo);
    } else {
      add_mixed_signed_integer_values(addr_val, addr_is_signed,
                                      &delta, /*op_2_signed=*/TRUE,
                                      &ovflo);
    }  /* if */
  }  /* if */
  if (ovflo) {
    info_with_pos_type(ec_constexpr_integer_overflow, diag_pos, addr_con->type,
                       ips);
  }  /* if */
  return !ovflo;
}  /* offset_runtime_address */


static a_type_ptr most_derived_object_type(a_constexpr_address *cap,
                                           a_type_ptr          type)
/*
cap is an address pointing to an object of static class type "type".  Return
that object's dynamic type.
*/
{
  a_byte  *subobj = cap->address, *complete_obj = cap->complete_object;

  for (;;) {
    a_base_class_ptr  bcp = *(a_base_class_ptr*)subobj;
    if (bcp == NULL) {
      break;
    } else {
      a_byte_count  offset;
      /* Determine the next-more-derived subobject. */
      get_mapped_byte_count(&persistent_map, bcp, offset);
      subobj -= offset;
      type = bcp->type;
      if (!subobject_is_initialized(subobj, complete_obj)) {
        /* The next-more-derived subobject is not constructed: So we're
           done. */
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return type;
}  /* most_derived_object_type */


static a_type_ptr address_con_complete_object_type(a_constant_ptr  addr_con)
/*
Return the type of the complete object on which the given address constant is
based, or NULL if such an object is not unambiguously defined (e.g., if it's
the address of a routine).
*/
{
  a_type_ptr  type;

  switch (addr_con->variant.address.kind) {
    case abk_variable:
      type = addr_con->variant.address.variant.variable->type;
      break;
    case abk_constant:
    case abk_temporary:
      type = addr_con->variant.address.variant.constant->type;
      break;
    default:
      type = NULL;
  }  /* switch */
  return type;
}  /* address_con_complete_object_type */


static a_boolean do_constexpr_typeid(an_interpreter_state  *ips,
                                     an_expr_node_ptr      expr,
                                     a_byte                *result_storage,
                                     a_byte                *complete_object)
/*
Interpret the given typeid(...) expression and place the result (a reference
to a std::type_info object) at *result_storage (which is storage within the
given complete object). 
*/
{
  a_boolean         result = TRUE;
  a_type_ptr        type = NULL;
  an_expr_node_ptr  opnd = expr->variant.typeid_info.expr;

  if (opnd == NULL && (expr->is_lvalue || expr->is_xvalue)) {
    /* A non-polymorphic typeid construct. */
    type = expr->variant.typeid_info.type;
  } else {
    /* Polymorphic typeid: not allowed in constant expressions prior to
       C++20. */
    if (opnd != NULL && (opnd->is_lvalue || opnd->is_xvalue) &&
        constexpr_virtual_enabled) {
      a_constexpr_address  *opnd_addr;
      /* First evaluate the operand, which should produce an address for the
         object whose dynamic type we must identify. */
      result = do_constexpr_expression(ips, opnd, result_storage,
                                       complete_object);
      if (!result) goto done;
      opnd_addr = (a_constexpr_address*)result_storage;
      /* Now determine the dynamic type of this object. */
      if (is_runtime_data_address(opnd_addr)) {
        /* The address is represented as an IL constant.  Follow the subobject
           path ignoring base-class casts (since we want the most-derived
           object address).  If that doesn't produce a type (because there is
           no path or the path only consists of base class casts), determine
           the type of the complete object. */
        a_constant_ptr  addr_con = opnd_addr->variant.addr_con;
        if (constant_is(addr_con, ck_address)) {
          a_subobject_path_ptr  path;
          path = addr_con->variant.address.subobject_path;
          for (; path != NULL; path = path->next) {
            if (path->is_offset) {
              check_assertion(type != NULL);
              type = array_element_type(type);
            } else if (!path->is_base_class) {
              type = path->variant.field->type;
            }  /* if */
          }  /* if */
          if (type == NULL) {
            type = address_con_complete_object_type(addr_con);
          }  /* if */
        }  /* if */
      } else {
        /* The interpreter representation includes embedded base class
           pointers that can be traversed to find the dynamic type of the
           object being referred to. */
        type = skip_typerefs(expr->variant.typeid_info.type);
        type = most_derived_object_type(opnd_addr, type);
      }  /* if */
    }  /* if */
  }  /* if */
  if (type != NULL) {
    a_constexpr_address  *cap = (a_constexpr_address*)result_storage;
    a_constant_ptr       cp = local_constant();
    make_typeid_constant(type, /*is_cli_typeid*/FALSE, cp);
    cp->next = ips->constants;
    ips->constants = cp;
    clear_runtime_constant_address(cap, cp);
  } else {
    info_with_pos(ec_constexpr_access_to_runtime_storage,
                  &expr->position, ips);
    do_constexpr_fail(result);
  }  /* if */
done:
  return result;
}  /* do_constexpr_typeid */


static a_base_class_ptr  find_base_in_type(a_type_ptr  dtp,
                                           a_type_ptr  btp)
/*
Find a base class entry of dtp with associated type btp and return it if found.
Otherwise, return NULL.

This is similar to find_base_class_of, but more efficient because no special
cases are considered.
*/
{
  a_base_class_ptr  bcp = base_classes_of(dtp);

  for (; bcp != NULL; bcp = bcp->next) {
    if (same_entities(bcp->type, btp)) break;
  }  /* for */
  return bcp;
}  /* find_base_in_type */


static void remove_trailing_subobject_path_entry(a_constant_ptr  con)
/*
The given entry has kind ck_address and points to a non-empty subobject path.
Remove the last entry on that path.
*/
{
  a_subobject_path_ptr  *p_spp = &con->variant.address.subobject_path;

  while ((*p_spp)->next != NULL) {
    p_spp = &(*p_spp)->next;
  }  /* while */
  *p_spp = NULL;
}  /* remove_trailing_subobject_path_entry */


static void adjust_constexpr_address_for_base_class(
                                            a_constexpr_address  *cap,
                                            a_base_class_ptr     baseward_bcp)
/*
cap points to an interpreter address of a class type object of type
baseward_bcp->derived_class.  Adjust the address so it points to the
base subobject corresponding to baseward_bcp.
*/
{
  a_derivation_step_ptr  dsp = baseward_bcp->derivation->path;
  a_byte                 *subobj = cap->address;
  a_type_ptr             subobj_type = baseward_bcp->derived_class;

  for (; dsp != NULL; dsp = dsp->next) {
    a_byte_count  offset;
    a_base_class_ptr  bcp;
    bcp = find_direct_base_class_of(subobj_type, dsp->base_class->type);
    get_mapped_byte_count(&persistent_map, bcp, offset);
    subobj += offset;
    subobj_type = bcp->type;
  }  /* for */
  cap->address = subobj;
  cap->flags &= ~CA_ARRAY_ELEMENT;
}  /* adjust_constexpr_address_for_base_class */


static a_boolean do_constexpr_dynamic_cast(
                                       an_interpreter_state  *ips,
                                       an_expr_node_ptr      expr,
                                       a_type_ptr            opnd_type,
                                       a_byte                *opnd_value,
                                       a_byte                *result_storage)
/*
Evaluate the dynamic_cast operation represented by expr.  Its operand (of type
opnd_type) has already been evaluated and the result of that evaluation is
opnd_value.  Store the result at *result_storage.

A dynamic_cast operation considers a number of strategies to produce a result:
  - Given a null pointer, it preserves that null pointer.
  - If the destination type represents a derived-to-base cast, the cast is
    essentially a static_cast (the front end represents it as a static_cast
    in some, but not all, configurations).
  - If the destination type is void*, a pointer to the most-derived object
    is produced.
  - If the destination type represents a base-to-derived cast that can be
    dynamically validated (i.e., we're not casting past the most-derived type
    of the object involved), then that cast is performed.
  - If the previous strategies failed, an attempt is made to perform a
    derived-to-base cast from the most-derived object.
This function implements those strategies both for objects stored in
interpreter storage and for "run-time objects" that have a constant address
represented by an entry of type a_constant (ck_address or ck_integer).
*/
{
  a_boolean  result = TRUE;

  if (constexpr_virtual_enabled) {
    a_constexpr_address  *opnd_addr = (a_constexpr_address*)opnd_value;
    a_constexpr_address  *result_addr = (a_constexpr_address*)result_storage;
    a_boolean            pointer_case = FALSE;
    a_type_ptr           tp = skip_typerefs(expr->type);
    if (type_is(opnd_type, tk_pointer)) {
      opnd_type = skip_typerefs(opnd_type->variant.pointer.type);
      pointer_case = TRUE;
    } else {
      an_expr_node_ptr  opnd = expr->variant.operation.operands;
      if (!opnd->is_lvalue) {
        info_with_pos(ec_bad_ref_dynamic_cast_operand, &opnd->position, ips);
        do_constexpr_fail(result);
        goto done;
      }  /* if */
    }  /* if */
    if (type_is(tp, tk_pointer)) {
      tp = skip_typerefs(tp->variant.pointer.type);
    }  /* if */
    if (same_entities(tp, opnd_type)) {
      /* The type is already as requested. */
      *result_addr = *opnd_addr;
      goto done;
    }  /* if */
    if (is_runtime_data_address(opnd_addr)) {
      /* The cast is applied to an IL constant. */
      a_constant_ptr  addr_con = opnd_addr->variant.addr_con;
      if (constant_is(addr_con, ck_address)) {
        /* We must compute a "dynamic_cast" applied to a subobject described
           by a ck_address IL constant.  Such an IL entry represents the
           address of a "complete object" adjusted for (a) field selections,
           (b) array element selections, and (c) derived-to-base casts.
           In that context, a dynamic_cast operation amounts to "undoing" some
           trailing (c) cases and possibly adding a few.  Fortunately, the
           transformations are recorded on the ck_address entry's "subobject
           path".  So we can traverse that to find which trailing segment
           should be undone (and compute the corresponding offset
           adjustment). */
        a_constant_ptr        new_con;
        a_subobject_path_ptr  spp;
        a_base_class_ptr      prev_bcp, baseward_bcp;
        if (!type_is(tp, tk_void) &&
            (baseward_bcp = find_base_in_type(opnd_type, tp)) != NULL) {
          /* The dynamic cast is actually a (static) derived-to-base cast. */
          if (baseward_bcp->ambiguous) {
            do_constexpr_fail(result);
            info_with_pos_type(ec_ambiguous_base_class, &expr->position,
                               baseward_bcp->type, ips);
            goto done;
          } else if (!is_accessible_base_class(baseward_bcp)) {
            do_constexpr_fail(result);
            info_with_pos_type(ec_inaccessible_base_class, &expr->position,
                               baseward_bcp->type, ips);
            goto done;
          } else {
            /* Make a copy of the address constant and its subobject path
               since we may potentially add entries to the path. */
            new_con = make_interpreter_copy_of_constant(ips, addr_con);
            spp = get_trailing_subobject_path_entry(
                        new_con, /*is_offset=*/FALSE, /*is_base_class=*/TRUE);
            prev_bcp = spp->variant.base_class;
            if (prev_bcp != NULL) {
              /* Undo the previous cast and find the corresponding baseward
                 base class in the most-derived type. */
              new_con->variant.address.offset -= prev_bcp->offset;
              if (baseward_bcp->direct) {
                /* This can only happen if baseward_bcp == prev_bcp, but we
                   we previously checked for the equal types case. */
                unexpected_condition();
              } else {
                a_base_class_ptr  disambiguator;
                disambiguator = find_disambiguator(prev_bcp, baseward_bcp);
                baseward_bcp = corresponding_base_class(
                                                      baseward_bcp,
                                                      prev_bcp->derived_class,
                                                      disambiguator);
              }  /* if */
            }  /* if */
            spp->variant.base_class = baseward_bcp;
            new_con->variant.address.offset += baseward_bcp->offset;
          }  /* if */
        } else {
          a_base_class_ptr  new_bcp = NULL;
          /* A base-to-derived cast or a "sideways" cast. */
          /* Make a copy of the address constant and its subobject path since
             we may potentially add entries to the path. */
          new_con = make_interpreter_copy_of_constant(ips, addr_con);
          spp = get_trailing_subobject_path_entry(
                        new_con, /*is_offset=*/FALSE, /*is_base_class=*/TRUE);
          prev_bcp = spp->variant.base_class;
          if (prev_bcp == NULL) {
            /* We are already at the most derived class.  Since we already
               checked for the equal types case, this must be an attempt to
               cast past the most-derived class type. */
            if (pointer_case) {
              if (type_is(tp, tk_void)) {
                remove_trailing_subobject_path_entry(new_con);
                clear_runtime_constant_address(result_storage, new_con);
              } else {
                clear_address(result_storage, NULL);
              }  /* if */
            } else {
              do_constexpr_fail(result);
              info_with_pos_type2(ec_constexpr_invalid_dynamic_cast,
                                  &expr->position, tp, opnd_type, ips);
            }  /* if */
            goto done;
          } else {
            a_base_class_derivation_ptr  bcdp = prev_bcp->derivation;
            /* Undo the previous cast offset. */
            new_con->variant.address.offset -= prev_bcp->offset;
            if (type_is(tp, tk_void) ||
                same_entities(tp, prev_bcp->derived_class)) {
              /* A cast to the most-derived class (either by casting to void*
                 or because the destination type happens to match the most-
                 derived class type). */
              remove_trailing_subobject_path_entry(new_con);
              clear_runtime_constant_address(result_storage, new_con);
              goto done;
            }  /* if */
            /* First try to find the destination base subobject on the
               derivation paths. */
            while (bcdp != NULL) {
              a_derivation_step_ptr  dsp = bcdp->path;
              bcdp = bcdp->next;
              for (; dsp != NULL; dsp = dsp->next) {
                if (same_entities(dsp->base_class->type, tp)) {
                  if (new_bcp == NULL) {
                    new_bcp = dsp->base_class;
                  } else if (dsp->base_class != new_bcp) {
                    /* The subobject on the derivation path is not unambiguous.
                       Move on to the final strategy. */
                    new_bcp = NULL;
                    bcdp = NULL;
                  }  /* if */
                  break;
                }  /* if */
              }  /* for */
            }  /* for */
            /* If the former strategy did not work, start from the most derived
               type and look for a matching unambiguous public subobject. */
            if (new_bcp == NULL) {
              new_bcp = find_base_in_type(prev_bcp->derived_class, tp);
              if (new_bcp != NULL) {
                if (new_bcp->ambiguous) {
                  if (pointer_case) {
                    clear_address(result_storage, NULL);
                  } else {
                    do_constexpr_fail(result);
                    info_with_pos_type(ec_ambiguous_base_class,
                                       &expr->position, new_bcp->type, ips);
                  }  /* if */
                  goto done;
                } else if (!is_accessible_base_class(new_bcp)) {
                  if (pointer_case) {
                    clear_address(result_storage, NULL);
                  } else {
                    do_constexpr_fail(result);
                    info_with_pos_type(ec_inaccessible_base_class,
                                       &expr->position, new_bcp->type, ips);
                  }  /* if */
                  goto done;
                }  /* if */
              } else {
                if (pointer_case) {
                  clear_address(result_storage, NULL);
                } else {
                  do_constexpr_fail(result);
                  info_with_pos_type2(ec_constexpr_invalid_dynamic_cast,
                                      &expr->position, tp,
                                      prev_bcp->derived_class, ips);
                }  /* if */
                goto done;
              }  /* if */
            }  /* if */
            spp->variant.base_class = new_bcp;
            new_con->variant.address.offset += new_bcp->offset;
          }  /* if */
        }  /* if */
        clear_runtime_constant_address(result_storage, new_con);
      } else if (constant_is(addr_con, ck_integer)) {
        /* Possibly a null pointer. */
        if (!pointer_case ||
            cmp_integer_values(&addr_con->variant.integer_value,
                               /*op_1_signed=*/FALSE,
                               (an_integer_value *)&zero_int,
                               /*op_2_signed=*/FALSE) != 0) {
          /* Either a reference cast (which cannot handle a null address, or
             not an actual null pointer (e.g., a non-zero integer cast to a
             pointer type). */
          info_with_pos(ec_constexpr_access_to_runtime_storage,
                        &expr->position, ips);
          do_constexpr_fail(result);
        } else {
          /* Create a null pointer result. */
          clear_address(result_addr, (a_byte*)0);
        }  /* if */
      } else {
        unexpected_condition();
      }  /* if */
    } else {
      /* Perform the dynamic_cast operation on the address of an object in
         interpreter storage. */
      a_base_class_ptr  bcp, baseward_bcp;
      a_type_ptr        dtp;
      a_byte            *subobj = opnd_addr->address;
      /* Check for the null address case first. */
      if (subobj == NULL) {
        /* Casting a null pointer value results in a null pointer value. */
        if (pointer_case) {
          clear_address(result_addr, (a_byte*)0);
        } else {
          info_with_pos(ec_constexpr_access_to_runtime_storage,
                        &expr->position, ips);
          do_constexpr_fail(result);
        }  /* if */
        goto done;
      }  /* if */
      if (type_is(tp, tk_void) &&
          (baseward_bcp = find_base_in_type(opnd_type, tp))!= NULL) {
        /* The dynamic cast is actually a (static) derived-to-base cast. */
        if (baseward_bcp->ambiguous) {
          do_constexpr_fail(result);
          info_with_pos_type(ec_ambiguous_base_class, &expr->position,
                             baseward_bcp->type, ips);
        } else if (!is_accessible_base_class(baseward_bcp)) {
          do_constexpr_fail(result);
          info_with_pos_type(ec_inaccessible_base_class, &expr->position,
                             baseward_bcp->type, ips);
        } else {
          *result_addr = *opnd_addr;
          adjust_constexpr_address_for_base_class(result_addr, baseward_bcp);
        }  /* if */
        goto done;
      }  /* if */
      /* Explore a base-to-derived cast. */
      *result_addr = *opnd_addr;
      dtp = opnd_type;
      bcp = *(a_base_class_ptr*)subobj;
      while (bcp != NULL) {
        a_byte_count  offset;
        get_mapped_byte_count(&persistent_map, bcp, offset);
        result_addr->address -= offset;
        dtp = bcp->derived_class;
        if (same_entities(dtp, tp)) {
          /* We found a derived subobject of the right type.  Since types with
             virtual bases cannot be literal types (and thus cannot be stored
             in interpreter memory), there is no concern about this being
             ambiguous. */
          goto done;
        }  /* if */
        if (is_initialized(result_addr)) {
          bcp = *(a_base_class_ptr*)result_addr->address;
        } else {
          /* The next derived subobject is not constructed yet (e.g., because
             we're still evaluating its constructor).  Do not search further
             down. */
          bcp = NULL;
        }  /* if */
      }  /* while */
      if (type_is(tp, tk_void)) {
        /* result_addr now points to the most-derived object, which is exactly
           what dynamic_cast<void*>(...) must do. */
        goto done;
      }  /* if */
      /* We've reached the most derived type (dtp) without encountering the
         destination type.  This might still be a "sideways" cast. */
      baseward_bcp = find_base_in_type(dtp, tp);
      if (baseward_bcp != NULL) {
        /* There is a subobject of the right kind: Verify that it is
           unambiguous and accessible. */
        if (baseward_bcp->ambiguous) {
          if (pointer_case) {
            clear_address(result_storage, NULL);
          } else {
            do_constexpr_fail(result);
            info_with_pos_type(ec_ambiguous_base_class,
                               &expr->position, tp, ips);
          }  /* if */
          goto done;
        } else if (!is_accessible_base_class(baseward_bcp)) {
          if (pointer_case) {
            clear_address(result_storage, NULL);
          } else {
            do_constexpr_fail(result);
            info_with_pos_type(ec_inaccessible_base_class,
                               &expr->position, tp, ips);
          }  /* if */
          goto done;
        }  /* if */
      } else {
        /* There is no subobject of the right type. */
        if (pointer_case) {
          clear_address(result_storage, NULL);
        } else {
          do_constexpr_fail(result);
          info_with_pos_type2(ec_constexpr_invalid_dynamic_cast,
                              &expr->position, tp, dtp, ips);
        }  /* if */
        goto done;
      }  /* if */
      adjust_constexpr_address_for_base_class(result_addr, baseward_bcp);
    }  /* if */
  } else {
    /* Prior to C++20, dynamic_cast expressions never produced core constant
       expressions. */
    do_constexpr_fail(result);
    info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                  &expr->position, ips);
  }  /* if */
done:;
  return result;
}  /* do_constexpr_dynamic_cast */


static a_boolean do_constexpr_new(an_interpreter_state  *ips,
                                  an_expr_node_ptr      expr,
                                  a_byte                *result_storage,
                                  a_byte                *complete_object)
/*
Evaluate the given new-expression.
*/
{
  a_boolean                    result = TRUE;
  a_new_delete_supplement_ptr  ndsp = expr->variant.new_delete;
  a_byte_count                 alloc_length, elem_size, orig_alloc_length;
  a_constexpr_address          *cap;
  a_type_ptr                   type = skip_typerefs(ndsp->type), elem_type;
  an_expr_node_ptr             length_expr = ndsp->number_of_elements;
  a_constexpr_allocation_ptr   allocation;

  if (!ips->is_constant_evaluated || !constexpr_dynamic_alloc_enabled) {
    /* Don't attempt to evaluate a new-expression if a constant result is not
       needed, because it could be somewhat expensive. */
    do_constexpr_fail(result);
    info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                  &expr->position, ips);
    goto done;
  }  /* if */
  if (length_expr == NULL) {
    /* No declarator of the form [<run-time length>]. */
    if (type_is(type, tk_array)) {
      alloc_length = (a_byte_count)type->variant.array
                                        .variant.number_of_elements;
      elem_type = skip_typerefs(type->variant.array.element_type);
    } else {
      alloc_length = 1;
      elem_type = type;
    }  /* if */
  } else {
    a_byte_count          opnd_n_bytes;
    a_type_ptr            length_tp = skip_typerefs(length_expr->type);
    a_byte                *length_bytes;
    a_boolean             ovflo;
    a_host_large_integer  length;
    check_assertion(type_is(length_tp, tk_integer));
    opnd_n_bytes = expr_result_size(ips, length_expr, length_tp, &result);
    if (!result) goto done;
    alloc_complete_object(ips, opnd_n_bytes, length_tp, length_bytes);
    if (!do_constexpr_expression(ips, length_expr,
                                 length_bytes, length_bytes)) {
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    get_int_val_from(length_bytes, length_tp, length, ovflo);
    if (ovflo || length < 0 || length > MAX_ARRAY_LENGTH) {
      info_with_pos_num(ec_constexpr_alloc_too_large, &length_expr->position,
                        (a_byte_count)length, ips);
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    alloc_length = (a_byte_count)length;
    check_assertion(type_is(type, tk_array));
    elem_type = skip_typerefs(type->variant.array.element_type);
  }  /* if */
  orig_alloc_length = alloc_length;
  cap = (a_constexpr_address*)result_storage;
  if (ndsp->routine != NULL && ndsp->routine->source_corresp.is_class_member) {
    /* A class-specific new-expression cannot be evaluated as a constant. */
    info_with_pos(ec_constexpr_class_specific_new, &expr->position, ips);
    do_constexpr_fail(result);
    goto done;
  } else if (ndsp->placement_new) {
    /* A placement new-expression cannot be evaluated as a constant, except
       if it is the new-expression implementing std::construct_at.  In the
       latter case, we will have interpreted the call to std::construct_at
       already and we will have recorded the specific address and type that
       the placement-new can access. */
    an_expr_node_ptr  ptr_expr = ndsp->arg;
    a_type_ptr        ptr_tp = skip_typerefs(ptr_expr->type);
    if (type != valid_placement_new_type) {
      /* The new-expression does not allocate a type deduced for an enclosing
         std::construct_at<T> call. */
      do_constexpr_fail(result);
    } else if (ptr_expr == NULL || ptr_expr->next != NULL ||
               !type_is(ptr_tp, tk_pointer) ||
               !is_void_type(ptr_tp->variant.pointer.type)) {
      /* The placement new isn't for an "operator new" with a single placement
         parameter of type "void*". */
      do_constexpr_fail(result);
    } else {
      /* Evaluate the address at which to place the object and ensure it's
         what was passed to an enclosing "std::construct_at<T>" call. */
      /* Allow recursive construct_at invocations from this point on. */
      a_byte  *expected_address = valid_placement_new_address;
      valid_placement_new_type = NULL;
      if (!do_constexpr_expression(ips, ptr_expr,
                                   result_storage, complete_object)) {
        result = FALSE;
        goto done;
      }  /* if */
      if (cap->address != expected_address) {
        /* The placement new address is not the one that came though the
           std::construct_at parameter.  Presumably that is a consequence of
           an incorrect std::construct_at definition. */
        do_constexpr_fail(result);
      }  /* if */
    }  /* if */
    if (!result) {
      info_with_pos(ec_constexpr_placement_new, &expr->position, ips);
      goto done;
    }  /* if */
    if (type_is(elem_type, tk_array)) {
      do {
        alloc_length = (a_byte_count)
           (alloc_length*elem_type->variant.array.variant.number_of_elements);
        elem_type = skip_typerefs(elem_type->variant.array.element_type);
      } while (type_is(elem_type, tk_array));
    }  /* if */
    elem_size = value_bytes_for_type(ips, elem_type, &result); 
  } else {
    allocation = do_constexpr_dynamic_alloc(ips, elem_type, alloc_length,
                                            &expr->position, cap, &elem_size);
    if (allocation == NULL) {
      result = FALSE;
      goto done;
    }  /* if */
    elem_type = allocation->elem_type;
    alloc_length = allocation->length;
  }  /* if */
  if (ndsp->dynamic_init != NULL) {
    /* Initialize each element. */
    a_dynamic_init_ptr  dip = ndsp->dynamic_init;
    int                 k = 0;
    a_byte              *elem = cap->address,
                        *complete_obj = cap->complete_object;
    if (dyn_init_is(dip, dik_constant) ||
        dyn_init_is(dip, dik_nonconstant_aggregate)) {
      a_constant_ptr  init_cp = dip->variant.constant.ptr;
      if (constant_is(init_cp, ck_aggregate) ||
          constant_is(init_cp, ck_string)) {
        a_type_ptr  init_tp = skip_typerefs(init_cp->type);
        if (type_is(init_tp, tk_array)) {
          a_constant_ptr  elem_cp;
          /* In some cases the initializer is a braced list whose number of
             elements is potentially larger than the number of elements
             allocated.  For example, "new int[n]{1, 2, 3}" where n evaluates
             to 2.  Check for that (non-constant) case. */
          if (length_expr != NULL) {
            a_targ_size_t  n_init_elems = 0;
            if (constant_is(init_cp, ck_aggregate)) {
              a_constant_ptr  cp = init_cp->variant.aggregate.first_constant;
              for (; cp != NULL; cp = cp->next) ++n_init_elems;
            } else {
              /* ck_string case. */
              n_init_elems = init_cp->variant.string.length;
            }  /* if */
            if (n_init_elems > orig_alloc_length) {
              info_with_pos_num(ec_constexpr_alloc_too_small,
                                &length_expr->position,
                                (a_byte_count)orig_alloc_length, ips);
              do_constexpr_fail(result);
              goto done;
            }  /* if */
          }  /* if */
          /* Initialize element-by-element. */
          if (constant_is(init_cp, ck_aggregate)) {
            /* Aggregate list case. */
            elem_cp = init_cp->variant.aggregate.first_constant;
            for (; k<(int)alloc_length; ++k, elem += elem_size) {
              if (elem_cp == NULL) {
                init_subobject_to_zero(ips, elem, elem_type, complete_obj);
                continue;
              } else if (constant_is(elem_cp, ck_init_repeat) &&
                  elem_cp->variant.init_repeat.count == 0) {
                /* A ck_init_repeat entry with zero count indicates that the
                   remainder of the array should be filled with that
                   initializer. */
                if (!extract_value_from_constant(
                                   ips, elem_cp->variant.init_repeat.constant,
                                   elem, complete_obj)) {
                  result = FALSE;
                  goto done;
                }  /* if */
              } else {
                /* A normal array element value to evaluate. */
                if (!extract_value_from_constant(ips, elem_cp, elem,
                                                 complete_obj)) {
                  result = FALSE;
                  goto done;
                }  /* if */
                elem_cp = elem_cp->next;
              }  /* if */
              mark_subobject_initialized(elem, complete_obj);
            }  /* for */
          } else {
            /* String literal case. */
            a_type_ptr     etp = skip_typerefs(
                                         init_tp->variant.array.element_type);
            a_targ_size_t  char_size = etp->size;
            a_targ_size_t  n_con_elems = init_cp->variant.string.length
                                                                  / char_size;
            a_const_char   *char_ptr = init_cp->variant.string.value;
            for (; k<(int)alloc_length; ++k, elem += elem_size) {
              if (k >= (int)n_con_elems) {
                /* Not all elements are covered.  Zero the remainder. */
                set_integer_value((an_integer_value*)elem,
                                  (a_host_large_integer)0);
              } else {
                unsigned long char_val = extract_character_from_string(
                                           char_ptr, (unsigned int)char_size);
                set_integer_value((an_integer_value*)elem,
                                  (a_host_large_integer)char_val);
                char_ptr += char_size;
              }  /* if */
              mark_subobject_initialized(elem, complete_obj);
            }  /* for */
          }  /* if */
          mark_complete_object_initialized(complete_obj);
          goto done;
        }  /* if */
      }  /* if */
    }  /* if */
    /* The initializer is not an array aggregate initializer. */
    for (; k<(int)orig_alloc_length; ++k, elem += elem_size) {
      /* Initialize each element individually.  Handle dik_zero entries
         separately since do_constexpr_dynamic_init doesn't have the type
         information for such entries. */
      if (dyn_init_is(dip, dik_zero)) {
        init_subobject_to_zero(ips, elem, elem_type, complete_obj);
      } else {
        a_constexpr_address  dst_addr;
        clear_address(&dst_addr, elem);
        dst_addr.alloc_seq_number = cap->alloc_seq_number;
        dst_addr.complete_object = complete_obj;
        if (!do_constexpr_dynamic_init(ips, dip, &expr->position, &dst_addr)) {
          result = FALSE;
          break;
        }  /* if */
      }  /* if */
      mark_subobject_initialized(elem, complete_obj);
      mark_complete_class_object_if_needed(elem_type, complete_obj);
    }  /* for */
    mark_complete_object_initialized(complete_obj);
  }  /* if */
done:
  return result;
}  /* do_constexpr_new */


static a_boolean do_constexpr_delete(an_interpreter_state  *ips,
                                     an_expr_node_ptr      expr)
/*
Evaluate the given delete-expression.
*/
{
  a_boolean                    result = TRUE;
  a_new_delete_supplement_ptr  ndsp = expr->variant.new_delete;
  a_byte_count                 opnd_n_bytes;
  an_expr_node_ptr             ptr_expr = ndsp->arg;
  a_type_ptr                   ptr_tp = skip_typerefs(ptr_expr->type);
  a_byte                       *ptr_bytes, *obj_bytes;
  a_constexpr_address          *cap;
  a_constexpr_allocation       *allocation;

  if (!ips->is_constant_evaluated || !constexpr_dynamic_alloc_enabled ||
      !type_is(ptr_tp, tk_pointer)) {
    /* Don't attempt to evaluate a delete-expression if a constant result is
       not needed, because the corresponding new-expression would not have
       been evaluated anyway.  In some situations the expression type may not
       actually be a pointer (e.g., in templates it might be an unknown
       type). */
    do_constexpr_fail(result);
    info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                  &expr->position, ips);
    goto done;
  }  /* if */
  opnd_n_bytes = expr_result_size(ips, ptr_expr, ptr_tp, &result);
  if (!result) goto done;
  alloc_complete_object(ips, opnd_n_bytes, ptr_tp, ptr_bytes);
  if (!do_constexpr_expression(ips, ptr_expr, ptr_bytes, ptr_bytes)) {
    result = FALSE;
    goto done;
  }  /* if */
  cap = (a_constexpr_address*)ptr_bytes;
  if (is_runtime_data_address(cap)) {
    if (is_null_pointer_value(cap->variant.addr_con)) {
      /* Deleting a null pointer has no effect. */
      goto done;
    } else {
      /* Search the allocations with a null pointer.  That will trigger the
         appropriate diagnostic. */
      obj_bytes = NULL;
    }  /* if */
  } else if (cap->address == NULL) {
    /* Deleting a null pointer has no effect. */
    goto done;
  } else if (ndsp->array_delete) {
    obj_bytes = cap->address;
  } else {
    /* For the non-array form of "delete ptr" find the most derived object
       pointed to. */
    a_byte  *complete_obj = cap->complete_object;
    obj_bytes = cap->address;
    if (obj_bytes != complete_obj) {
      a_base_class_ptr  bcp = *(a_base_class_ptr*)obj_bytes;
      if (ndsp->dynamic_init == NULL ||
          ndsp->dynamic_init->destructor == NULL ||
          !ndsp->dynamic_init->destructor->is_virtual) {
        info_with_pos(ec_constexpr_nonvirtual_subobject_delete,
                      &expr->position, ips);
        do_constexpr_fail(result);
        goto done;
      }  /* if */
      for (;;) {
        if (bcp == NULL) {
          /* This is the most-derived class in the object layout. */
          break;
        } else {
          a_byte_count  offset;
          /* Determine the next-more-derived subobject. */
          get_mapped_byte_count(&persistent_map, bcp, offset);
          obj_bytes -= offset;
          if (!subobject_is_initialized(obj_bytes, complete_obj)) {
            /* The next-more-derived subobject is not constructed: So we're
               done. */
            break;
          }  /* if */
        }  /* if */
        bcp = *(a_base_class_ptr*)obj_bytes;
      }  /* for */
    }  /* if */
  }  /* if */
  allocation = find_constexpr_allocation(ips, obj_bytes, &expr->position);
  if (allocation == NULL) {
    result = FALSE;
    goto done;
  } else {
    /* Perform necessary destructions and mark subobject uninitialized. */
    a_dynamic_init_ptr  dip = ndsp->dynamic_init;
    a_byte_count        k = 0, length = allocation->length, elem_size;
    a_byte              *arr = (a_byte*)allocation+allocation->prefix_size,
                        *elem = arr;
    a_type_ptr          elem_type = skip_typerefs(allocation->elem_type);
    elem_size = value_bytes_for_type(ips, elem_type, &result);
    if (!result) goto done;
    unmark_complete_object_initialized(arr);
    for (; k<length; ++k, elem += elem_size) {
      if (dip != NULL) {
        /* Run the destructor.  For the non-array case, this might be a virtual
           call: Make sure we start from the original address since
           do_constexpr_dtor will need it for the virtual function dispatch. */
        a_byte  *obj = ndsp->array_delete ? elem : cap->address;
        if (!do_constexpr_dtor(ips, dip->destructor, &expr->position,
                               obj, arr)) {
          result = FALSE;
          goto done;
        }  /* if */
        mark_subobject_uninitialized(elem, arr);
      } else if (!mark_whole_subobject_uninitialized(
                                                 ips, elem, elem_type, arr)) {
        result = FALSE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  free_allocation(ips, allocation);
done:
  return result;
}  /* do_constexpr_delete */

#if GNU_VECTOR_TYPES_ALLOWED

static a_boolean do_constexpr_vector_binary_op(
                                       an_interpreter_state  *ips,
                                       an_expr_node_ptr      expr,
                                       a_type_ptr            tp,
                                       a_byte                *src1,
                                       a_byte                *src2,
                                       a_byte                *dst,
                                       a_byte                *complete_object)
/*
The given expression (with type tp after dropping qualifiers) is a vector
operation applied to two operands (stored at src1 and src2, respectively).
Compute the result and store it at dst (which is part of the complete object
pointed to by complete_object).
*/
{
  a_boolean        result = TRUE, is_integer, is_signed = FALSE;
  a_type_ptr       etp = skip_typerefs(tp->variant.vector.element_type);
  a_targ_size_t    k, n_elems = tp->size/etp->size;
  a_byte_count     elem_size = value_bytes_for_type(ips, etp, &result);
  an_integer_kind  int_kind = ik_none;
  
  is_integer = type_is(etp, tk_integer);
  if (is_integer) {
    int_kind = etp->variant.integer.int_kind;
    is_signed = int_kind_is_signed[int_kind];
  }  /* if */
  check_assertion(result && (is_integer || type_is(etp, tk_float)));
  for (k = 0; k<n_elems; ++k) {
    if (is_integer) {
      a_boolean  ovflo = FALSE;
      switch (expr->variant.operation.kind) {
        case eok_add:
          *(an_integer_value*)dst = *(an_integer_value*)src1;
          add_integer_values((an_integer_value*)dst, (an_integer_value*)src2,
                             is_signed, &ovflo);
          break;
        case eok_subtract:
          *(an_integer_value*)dst = *(an_integer_value*)src1;
          subtract_integer_values((an_integer_value*)dst, 
                                  (an_integer_value*)src2,
                                  is_signed, &ovflo);
          break;
        case eok_multiply:
          *(an_integer_value*)dst = *(an_integer_value*)src1;
          multiply_integer_values((an_integer_value*)dst,
                                  (an_integer_value*)src2,
                                  is_signed, &ovflo);
          break;
        case eok_divide:
          *(an_integer_value*)dst = *(an_integer_value*)src1;
          divide_integer_values((an_integer_value*)dst,
                                (an_integer_value*)src2,
                                is_signed, &ovflo);
          break;
        case eok_remainder:
          *(an_integer_value*)dst = *(an_integer_value*)src1;
          remainder_integer_values((an_integer_value*)dst,
                                   (an_integer_value*)src2,
                                   is_signed, &ovflo);
          break;
        case eok_shiftl:
        case eok_shiftr:
          { a_host_large_integer  host_int_val;
            an_expr_node_ptr      opnd1 = expr->variant.operation.operands,
                                  opnd2 = opnd1->next;
            a_type_ptr            tp2 = skip_typerefs(opnd2->type);
            get_int_val_from(src2, tp2, host_int_val, ovflo);
            if (ovflo) {
              do_constexpr_fail(result);
              info_with_pos(ec_integer_overflow, &expr->position, ips);
              break;
            } else if (host_int_val < 0) {
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_negative_shift, &expr->position, ips);
              break;
            } else if (host_int_val >=
                            (a_host_large_integer)(tp->size * targ_char_bit)) {
              do_constexpr_fail(result);
              info_with_pos_num(ec_constexpr_shift_excess, &expr->position,
                                (uint32_t)host_int_val, ips);
              break;
            }  /* if */
            *(an_integer_value*)dst = *(an_integer_value*)src1;
            if (node_operator_is(expr, eok_shiftl)) {
              if (is_signed) {
                if (sign_of(*(an_integer_value*)dst)) {
                  info_with_pos(ec_constexpr_shift_negative_value,
                                &expr->position, ips);
                  do_constexpr_fail(result);
                  break;
                }  /* if */
              }  /* if */
              shift_left_integer_value((an_integer_value*)dst,
                                       (int)host_int_val, &ovflo);
            } else {
              shift_right_integer_value((an_integer_value*)dst,
                                        (int)host_int_val, is_signed,
                                        targ_right_shift_is_arithmetic);
            }  /* if */
          }
          break;
        case eok_and:
          *(an_integer_value*)dst = *(an_integer_value*)src1;
          and_integer_values((an_integer_value*)dst, (an_integer_value*)src2);
          break;
        case eok_or:
          *(an_integer_value*)dst = *(an_integer_value*)src1;
          or_integer_values((an_integer_value*)dst, (an_integer_value*)src2);
          break;
        case eok_xor:
          *(an_integer_value*)dst = *(an_integer_value*)src1;
          xor_integer_values((an_integer_value*)dst, (an_integer_value*)src2);
          break;
        default:
          unexpected_condition();
      }  /* switch */
      if (is_signed) {
        sign_extend_integer_value((an_integer_value*)dst,
                                  (int)(etp->size * targ_char_bit));
      } else {
        and_integer_values((an_integer_value*)dst,
                           &max_integer_value_of_kind[int_kind]);
      }  /* if */
    } else {
      a_boolean  err = FALSE, depends_on_fp_mode;
      switch (expr->variant.operation.kind) {
        case eok_add:
          fp_add(etp->variant.float_kind,
                 fp_value(src1), fp_value(src1), fp_value(dst),
                 &err, &depends_on_fp_mode);
          break;
        case eok_subtract:
          fp_subtract(etp->variant.float_kind,
                      fp_value(src1), fp_value(src1), fp_value(dst),
                      &err, &depends_on_fp_mode);
          break;
        case eok_multiply:
          fp_multiply(etp->variant.float_kind,
                      fp_value(src1), fp_value(src1), fp_value(dst),
                      &err, &depends_on_fp_mode);
          break;
        case eok_divide:
          fp_divide(etp->variant.float_kind,
                    fp_value(src1), fp_value(src1), fp_value(dst),
                    &err, &depends_on_fp_mode);
          break;
        default:
          unexpected_condition();
      }  /* switch */
      if (err) {
        do_constexpr_fail(result);
        info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
        goto done;
      }  /* if */
    }  /* if */
    mark_subobject_initialized(dst, complete_object);
    src1 += elem_size;
    src2 += elem_size;
    dst += elem_size;
  }  /* for */
done:
  return result;
}  /* do_constexpr_vector_binary_op */

#endif /* GNU_VECTOR_TYPES_ALLOWED */

static a_boolean reinterpret_runtime_address(a_byte      *addr_opnd,
                                             a_type_ptr  dtype)
/*
addr_opnd points to an entry of type a_constexpr_address and is the operand of
a reinterpret_cast operation with the given destination type.  Return TRUE if
the constant type can be reinterpreted to this type and perform the
transformation of the type accordingly.
*/
{
  a_boolean  result = FALSE;

  if (is_runtime_data_address(addr_opnd)) {
    a_constexpr_address  *cap = (a_constexpr_address*)addr_opnd;
    a_constant_ptr       con = cap->variant.addr_con, next_con = con->next;
    an_error_code        err_code;
    a_boolean            did_not_fold;
    type_change_constant_full(con, dtype,
                              /*is_implicit_cast=*/FALSE,
                              /*constant_context=*/TRUE,
                              /*evaluated_context=*/TRUE,
                              /*fold_constant_addr_exprs=*/TRUE,
                              /*is_cli_attr_arg_expression=*/FALSE,
                              /*check_cast_access=*/FALSE,
                              /*check_ambiguity=*/FALSE,
                              /*is_reinterpret_cast=*/TRUE,
                              /*maintain_expression=*/FALSE,
                              &did_not_fold, &err_code, &null_source_position);
    /* Restore the "next" pointer, which may have been cleared by the call to
       type_change_constant_full. */
    con->next = next_con;
    result = !did_not_fold && err_code == ec_no_error;
  }  /* if */
  return result;
}  /* reinterpret_runtime_address */


/*lint -efunc(2704,*do_constexpr_expression)*/
static a_boolean do_constexpr_expression(
                                       an_interpreter_state  *ips,
                                       an_expr_node_ptr      orig_expr,
                                       a_byte                *result_storage,
                                       a_byte                *complete_object)
/*
Interpret the given expression in the given interpreter context.  If
successful return TRUE and store the result at *result_storage (which is
storage within the given complete object).  Otherwise, return FALSE and update 
*ips accordingly.  A glvalue result is represented as an a_constexpr_address
value, so result_storage must be at least large enough for that type;
otherwise, it need only be large enough for the type of the prvalue result.
*/
{
  a_boolean            result = TRUE;
  an_expr_node_ptr     expr = skip_parens(orig_expr);
  a_type_ptr           tp = skip_typerefs(expr->type);
  a_byte_count         n_bytes;
  an_integer_kind      int_kind;
  a_boolean            is_signed;
  a_host_large_integer host_int_val;

  if (type_is(tp, tk_template_param) || expr->do_not_interpret) {
    info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                  &expr->position, ips);
    do_constexpr_fail(result);
    goto done;
  } else if (type_is(tp, tk_error)) {
    expect_error();
    ips->input_error = TRUE;
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  switch (expr->kind) {
    case enk_operation:
      {
        /* An operation node.  Lvalue-to-rvalue conversions and casts are
           assumed to have already been applied to the operands, as
           required by the semantics of the operation. */
        an_expr_node_ptr opnd1;
        a_type_ptr       opnd1_type;
        a_byte           *opnd1_value;
        an_expr_node_ptr opnd2;
        a_type_ptr       opnd2_type = NULL;
        a_byte           *opnd2_value = NULL;
        a_boolean        ovfl, err, depends_on_fp_mode, unord;
        a_byte_count     opnd_n_bytes;

        if (is_call_node(expr)) {
          /* Call nodes are handled separately because their operands are set
             up a little differently. */
          result = do_constexpr_call(
                                  ips, expr, result_storage, complete_object);
          goto done;
        } else if (node_operator_is(expr, eok_class_rvalue_adjust)) {
          /* This is a pass-through operator for prvalues.  So we cannot just
             copy the operand, since it could invalidate internal addresses.
             Instead, the operand must be evaluated directly into the final
             result storage. */
          result = do_constexpr_expression(
                                        ips, expr->variant.operation.operands,
                                        result_storage, complete_object);
          goto done;
        }  /* if */
/*
Macro to set result_storage from either the address in opnd or the value
to which that address points, depending on whether the result is a glvalue
or a prvalue.  This is used for operations that produce glvalues but may
incorporate an implicit lvalue-to-rvalue conversion, i.e., "rvalueable"
nodes.
*/
#define SET_result_val_from_operand_address(opnd)                             \
  {                                                                           \
    if (expr->is_lvalue || expr->is_xvalue || is_function_address(opnd)) {    \
      /* Copy the address. */                                                 \
      *(a_constexpr_address*)result_storage = *(a_constexpr_address*)(opnd);  \
    } else {                                                                  \
      /* Do the lvalue-to-rvalue conversion into the result. */               \
      n_bytes = value_bytes_for_type(ips, tp, &result);                       \
      result = do_glvalue_to_prvalue(ips, expr, tp,                           \
                                     (a_constexpr_address*)(opnd), n_bytes,   \
                                     result_storage, complete_object);        \
    }  /* if */                                                               \
  }  /* SET_result_val_from_operand_address */

/*
If is_signed is TRUE, the following macro sets result to FALSE if the integer
result of an operation (in val) is within the range representable by its kind
(int_kind).  This includes checking the value of the flag ovfl set by the
operation.  If is_signed is FALSE, the macro just clears any bits not used by
the value representation of the integer value.
*/
#define CHECK_int_range(val, tp)                                              \
{                                                                             \
  if (!is_signed) {                                                           \
    and_integer_values((an_integer_value*)(val),                              \
                       &max_integer_value_of_kind[int_kind]);                 \
  } else if (ovfl ||                                                          \
             cmp_integer_values((an_integer_value*)(val), is_signed,          \
                                &max_integer_value_of_kind[int_kind],         \
                                is_signed) > 0 ||                             \
             cmp_integer_values((an_integer_value*)(val), is_signed,          \
                                &min_integer_value_of_kind[int_kind],         \
                                is_signed) < 0) {                             \
    do_constexpr_fail(result);                                                \
    info_with_pos_type(ec_constexpr_integer_overflow, &expr->position, tp,    \
                       ips);                                                  \
  }  /* if */                                                                 \
}  /* CHECK_int_range */

        opnd1 = expr->variant.operation.operands;
        opnd2 = opnd1->next;
        opnd1_type = skip_typerefs(opnd1->type);
        opnd_n_bytes = expr_result_size(ips, opnd1, opnd1_type, &result);
        if (!result) break;
        alloc_complete_object(ips, opnd_n_bytes, opnd1_type, opnd1_value);
        /* Evaluate the first operand, unless this is an operation that
           requires the second operand to be evaluated first or expr is an
           operator that sometimes does not evaluate its first operand. */
        if (node_operator_is(expr, eok_comma) ||
            node_operator_is(expr, eok_dot_static) ||
            node_operator_is(expr, eok_points_to_static)) {
          /* To avoid spurious warnings from certain tools. */
          opnd2_value = opnd1_value;
          opnd2_type = opnd1_type;
        } else if (!expr->variant.operation.eval_right_to_left &&
                   !do_constexpr_expression(ips, opnd1,
                                            opnd1_value, opnd1_value)) {
          do_constexpr_fail(result);
        } else if (opnd2 != NULL &&
                   !node_is(opnd2, enk_field) &&
                   !node_operator_is(expr, eok_land) &&
                   !node_operator_is(expr, eok_lor) &&
                   !node_operator_is(expr, eok_question) &&
                   !node_operator_is(expr, eok_dot_vacuous_destructor_call) &&
                   !node_operator_is(expr,
                                     eok_points_to_vacuous_destructor_call)) {
          /* Evaluate the second operand.  For short-circuiting operators,
             whether to evaluate the second operand will be decided below in
             the specific code for each such operator.  The comma operator can
             be handled similarly (although the evaluation is unconditional in
             that case); eok_dot_static and eok_points_to_static are equivalent
             to the comma operator in this respect.  If this is an operation
             that requires the second operand to be evaluated first, the
             evaluation of the first operand is performed here also.  If the
             second operand is an enk_field node, do not "evaluate" it (it
             would do nothing, and determining the corresponding type size
             could trigger a spurious failure (the size is not needed for
             glvalue cases). */
          opnd2_type = skip_typerefs(opnd2->type);
          opnd_n_bytes = expr_result_size(ips, opnd2, opnd2_type, &result);
          if (!result) break;
          alloc_complete_object(ips, opnd_n_bytes, opnd2_type, opnd2_value);
          if (!do_constexpr_expression(ips, opnd2, opnd2_value, opnd2_value)) {
            do_constexpr_fail(result);
          } else if (expr->variant.operation.eval_right_to_left &&
                     !do_constexpr_expression(ips, opnd1,
                                              opnd1_value, opnd1_value)) {
            do_constexpr_fail(result);
          }  /* if */
        } else {
          /* To avoid spurious warnings from certain tools. */
          opnd2_value = opnd1_value;
          opnd2_type = opnd1_type;
        }  /* if */
        if (!result) break;
        /* The operand(s) were evaluated successfully.  Process the
           operation. */
        switch (expr->variant.operation.kind) {
          case eok_address_of:
          case eok_reference_to:
            /* The result is an a_constexpr_address designating the object,
               and the operand is already an a_constexpr_address (glvalue or
               temporary), so just copy the operand.  An exception exists:
               eok_reference_to can be applied to a class prvalue. */
            if (opnd1->is_lvalue || opnd1->is_xvalue) {
              *(a_constexpr_address *)result_storage =
                                          *(a_constexpr_address *)opnd1_value;
              if (is_runtime_data_address(result_storage)) {
                a_constant_ptr  rt_con =
                    ((a_constexpr_address *)result_storage)->variant.addr_con;
                rt_con->type = expr->type;
              }  /* if */
            } else {
              /* Return the address of the (class) prvalue. */
              clear_address(result_storage, opnd1_value);
              ((a_constexpr_address *)result_storage)->alloc_seq_number =
                                          ips->storage_stack.alloc_seq_number;
            }  /* if */
            break;
          case eok_indirect:
          case eok_ref_indirect:
            /* The result is either a copy of the operand (which is an
               a_constexpr_address) if the result is a glvalue or the
               value to which the address points for a prvalue. */
            SET_result_val_from_operand_address(opnd1_value);
            break;
          case eok_cast:
            if (tp->kind == opnd1_type->kind) {
              /* The type kinds are the same, so the representation is
                 the same, and we can just copy the opnd1 value. */
              if (type_is(tp, tk_integer)) {
                /* Integers: Somewhat surprisingly, narrowing conversions are
                   valid here ("implementation-defined").  So we don't check
                   that the result is in range. */
                an_integer_value  *r_int = (an_integer_value *)result_storage;
                *r_int = *(an_integer_value *)opnd1_value;
                int_kind = tp->variant.integer.int_kind;
                if (int_kind_is_signed[int_kind]) {
                  if (tp->size == opnd1_type->size ||
                      int_type_is_signed(opnd1_type)) {
                    /* When converting a signed value to another signed value,
                       be sure to sign-extend the result (otherwise, widening
                       conversions would produce positive values from negative
                       operands.  This is also needed when converting an
                       unsigned value to a signed value of the same size,
                       in case the host representation includes additional bits
                       (e.g., (int)(unsigned)-1 must be negative). */
                    int  n_bits = (int)(opnd1_type->size*targ_char_bit);
                    sign_extend_integer_value(r_int, n_bits);
                  }  /* if */
                } else {
                  /* Truncate the unsigned result, in case this is a narrowing
                     conversion. */
                  and_integer_values(r_int,
                                     &max_integer_value_of_kind[int_kind]);
                }  /* if */
              } else if (type_is_float_like(tp)) {
                a_boolean  depends_of_fp_mode;
                fp_change_kind(fp_value(opnd1_value),
                               opnd1_type->variant.float_kind,
                               fp_value(result_storage),
                               tp->variant.float_kind,
                               &err, &depends_of_fp_mode);
                if (err) {
                  info_with_pos(ec_constexpr_fp_conversion_failed,
                                &expr->position, ips);
                  do_constexpr_fail(result);
                }  /* if */
              } else if (type_is(tp, tk_pointer)) {
                /* E.g., a conversion from X* to X const* or X* to void*. */
                a_type_ptr  utp1 = skip_typerefs(tp->variant.pointer.type);
                a_type_ptr  utp2;
                utp2 = skip_typerefs(opnd1_type->variant.pointer.type);
                if (type_is(utp1, tk_error) || type_is(utp2, tk_error)) {
                  do_constexpr_fail(result);
                  ips->input_error = TRUE;
                  break;
                }  /* if */
                if (expr->variant.operation.is_reinterpret_cast ||
                    expr->variant.operation.is_reinterpret_like_cast ||
                    (type_is(utp2, tk_void) && !type_is(utp1, tk_void))) {
                  /* Usually an error, but in some cases we permit reinterpret-
                     casting a run-time address (because that sometimes allows
                     dynamic initialization to become static). */
                  if (ips->allow_reinterpret_cast &&
                      (expr->variant.operation.is_reinterpret_cast ||
                       expr->variant.operation.is_reinterpret_like_cast) &&
		      reinterpret_runtime_address(opnd1_value, tp)) {
                    *(a_constexpr_address*)result_storage =
                                           *(a_constexpr_address*)opnd1_value;
                  } else if (has_gnu_source_location_impl_type() &&
                             type_is(utp2, tk_void) &&
                             utp1 == gnu_source_location_impl_type()) {
                    /* This is okay, GCC allows conversion from the source
                       location __impl type to "void*". */
                  } else {
                    info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                        &expr->position, opnd1_type, tp, ips);
                    do_constexpr_fail(result);
                    break;
                  }  /* if */
                }  /* if */
                *(a_constexpr_address*)result_storage =
                                           *(a_constexpr_address*)opnd1_value;
              } else if (type_is(tp, tk_ptr_to_member)) {
                if (!expr->variant.operation.is_reinterpret_cast) {
                  *(a_constexpr_ptr_to_mem*)result_storage =
                                        *(a_constexpr_ptr_to_mem*)opnd1_value;
                } else {
                  info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                      &expr->position, opnd1_type, tp, ips);
                  do_constexpr_fail(result);
                }  /* if */
              } else if (type_is(tp, tk_void)) {
                release_address_structures(opnd1, opnd1_type, opnd1_value);
#if C99_IL_EXTENSIONS_SUPPORTED
              } else if (tp->kind == (a_type_kind)tk_complex) {
                a_boolean  depends_of_fp_mode;
                fp_change_kind(&cx_value(opnd1_value)->real,
                               opnd1_type->variant.float_kind,
                               &cx_value(result_storage)->real,
                               tp->variant.float_kind,
                               &err, &depends_of_fp_mode);
                if (!err) {
                  fp_change_kind(&cx_value(opnd1_value)->imag,
                                 opnd1_type->variant.float_kind,
                                 &cx_value(result_storage)->imag,
                                 tp->variant.float_kind,
                                 &err, &depends_of_fp_mode);
                }  /* if */
                if (err) {
                  info_with_pos(ec_constexpr_fp_conversion_failed,
                                &expr->position, ips);
                  do_constexpr_fail(result);
                }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              } else {
                info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                    &expr->position, opnd1_type, tp, ips);
                do_constexpr_fail(result);
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_integer &&
                       tp->kind == (a_type_kind)tk_float) {
              /* Integer-to-floating-point conversion. */
              int_kind = opnd1_type->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              conv_integer_value_to_float((an_integer_value*)opnd1_value,
                                          is_signed,
                                          fp_value(result_storage),
                                          tp->variant.float_kind,
                                          &err);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                    &expr->position, opnd1_type, tp, ips);
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_float &&
                       tp->kind == (a_type_kind)tk_integer) {
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              if (conv_float_value_to_int_value(
                                            fp_value(opnd1_value),
                                            opnd1_type->variant.float_kind,
                                            (an_integer_value*)result_storage,
                                            is_signed, &depends_on_fp_mode)) {
                ovfl = FALSE;
                CHECK_int_range((an_integer_value*)(result_storage), tp);
              } else {
                do_constexpr_fail(result);
                info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                    &expr->position, opnd1_type, tp, ips);
              }  /* if */
            } else if (tp->kind == (a_type_kind)tk_pointer &&
                       (opnd1_type->kind == (a_type_kind)tk_nullptr ||
                        (opnd1_type->kind == (a_type_kind)tk_integer &&
                         cmp_integer_values((an_integer_value *)opnd1_value,
                                            /*op_1_signed=*/FALSE,
                                            (an_integer_value *)&zero_int,
                                            /*op_2_signed=*/FALSE) == 0))) {
              /* A null pointer. */
              clear_address(result_storage, (a_byte*)0);
            } else if (tp->kind == (a_type_kind)tk_ptr_to_member &&
                       (opnd1_type->kind == (a_type_kind)tk_nullptr ||
                        (opnd1_type->kind == (a_type_kind)tk_integer &&
                         cmp_integer_values((an_integer_value *)opnd1_value,
                                            /*op_1_signed=*/FALSE,
                                            (an_integer_value *)&zero_int,
                                            /*op_2_signed=*/FALSE) == 0))) {
              /* A null pointer-to-member. */
              a_constexpr_ptr_to_mem
                                *pm = (a_constexpr_ptr_to_mem*)result_storage;
              pm->subtract_adjustment = 0;
              pm->this_class_adjustment = 0;
              if (is_function_type(tp->variant.ptr_to_member.type)) {
                pm->is_ptr_to_mem_function = TRUE;
                pm->variant.routine = NULL;
              } else {
                pm->is_ptr_to_mem_function = FALSE;
                pm->variant.field = NULL;
              }  /* if */
            } else if (tp->kind == (a_type_kind)tk_void) {
              /* Conversion to void.  No result. */
              release_address_structures(opnd1, opnd1_type, opnd1_value);
#if C99_IL_EXTENSIONS_SUPPORTED
            } else if (tp->kind == (a_type_kind)tk_complex &&
                       opnd1_type->kind == (a_type_kind)tk_float) {
              /* Convert real floating-point to complex floating-point. */
              cx_value(result_storage)->real = *fp_value(opnd1_value);
              cx_value(result_storage)->imag =
                                        zero_flt[(int)tp->variant.float_kind];
            } else if (tp->kind == (a_type_kind)tk_complex &&
                       opnd1_type->kind == (a_type_kind)tk_imaginary) {
              /* Convert imaginary floating-point to complex floating-point. */
              cx_value(result_storage)->real =
                                        zero_flt[(int)tp->variant.float_kind];
              cx_value(result_storage)->imag = *fp_value(opnd1_value);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            } else if (tp->kind == (a_type_kind)tk_integer &&
                       opnd1_type->kind == (a_type_kind)tk_pointer &&
                       ips->permit_null_pointer_offsets &&
                       is_integer_address((a_constexpr_address*)opnd1_value,
                                          (an_integer_value*)result_storage)) {
              /* These kinds of casts are generally invalid, but are sometimes
                 needed to permit traditional offsetof implementations (e.g.,
                 in GCC and MSVC modes). */
            } else {
              do_constexpr_fail(result);
              info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                  &expr->position, opnd1_type, tp, ips);
            }  /* if */
            break;
          case eok_lvalue_cast:
          case eok_ref_cast:
          case eok_lvalue_adjust:
            { a_boolean  valid_cast;
              if (expr->variant.operation.is_reinterpret_cast) {
                /* reinterpret_cast expressions are generally invalid, but GCC
                   and MSVC appear to allow them on null-based addresses to
                   permit traditional offsetof implementations. */
                a_constexpr_address  *cap = (a_constexpr_address*)opnd1_value;
                if (((gpp_mode && !clang_mode) || microsoft_mode) &&
                    is_integer_address(cap, (an_integer_value*)NULL)) {
                  valid_cast = TRUE;
                } else if (ips->allow_reinterpret_cast &&
                           is_runtime_data_address(cap)) {
                  /* Allow reinterpret_cast on run-time data addresses.  This
                     allows some variable initializers that otherwise would
                     require "dynamic initialization" to be implemented as
                     "static initialization". */
                  valid_cast = TRUE;
                } else {
                  valid_cast = FALSE;
                }  /* if */
              } else {
                /* If the type (other than qualification) doesn't change, this
                   is not a reinterpret-like cast and we can interpret the
                   result.  In the case of casting a function lvalue to a
                   reference to function type, it is possible that the type of
                   this node was later "decayed" to a pointer-to-function type
                   (to match the expectations of a parent node); that case is
                   valid, too. */
                if (!identical_types_ignoring_qualifiers(tp, opnd1_type) &&
                    !acceptable_lvalue_conversion(
                                       ips, (a_constexpr_address*)opnd1_value,
                                       opnd1_type, tp)) {
                  valid_cast = FALSE;
                } else {
                  valid_cast = TRUE;
                }  /* if */
              }  /* fi */
              if (valid_cast) {
                SET_result_val_from_operand_address(opnd1_value);
              } else {
                info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                    &expr->position, opnd1_type, tp, ips);
                do_constexpr_fail(result);
              }  /* if */
            }
            break;
          case eok_base_class_cast:
            if (tp->kind == (a_type_kind)tk_pointer ||
                opnd1->is_lvalue || opnd1->is_xvalue) {
              /* An address adjustment. */
              a_constexpr_address  *result_addr =
                                         (a_constexpr_address*)opnd1_value;
              a_type_ptr           dtp, btp;
              a_base_class_ptr     bcp;
              a_byte_count         offset;
              if (tp->kind == (a_type_kind)tk_pointer) {
                dtp = skip_typerefs(opnd1_type->variant.pointer.type);
                btp = skip_typerefs(tp->variant.pointer.type);
              } else {
                dtp = opnd1_type;
                btp = tp;
              }  /* if */
              bcp = find_direct_base_class_of(dtp, btp);
              if (bcp == NULL) {
                /* bcp can be NULL for indirect virtual base classes (only). */
                do_constexpr_fail(result);
                info_with_pos_type(ec_constexpr_virtual_base, &expr->position,
                                   btp, ips);
              } else if (is_runtime_data_address(result_addr)) {
                /* Attempt a "symbolic" derived-to-base cast using the
                   constant folding routines. */
                a_constant_ptr  new_con = local_constant(),
                                addr_con = result_addr->variant.addr_con;
                a_boolean       nonconstant;
                an_error_code   err_code;
                /* Create a copy of the address constant because we might
                   change its subobject path. */
                addr_con = make_interpreter_copy_of_constant(ips, addr_con);
                addr_con->expr = NULL;
                fold_base_class_cast(
                    addr_con, bcp, btp, new_con, /*check_cast_access=*/FALSE,
                    /*check_ambiguity=*/FALSE, expr->compiler_generated,
                    /*is_object_pointer=*/(result_addr->length != 0) ||
                                          expr->variant.operation
                                               .implicit_in_member_naming,
                    /*omit_back_expr=*/TRUE,
                    &nonconstant, &expr->position, &err_code);
                if (nonconstant || err_code != ec_no_error) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else {
                  result_addr->variant.addr_con =
                              make_interpreter_copy_of_constant(ips, new_con);
                }  /* if */
                release_local_constant(&new_con);
                if (!result) break;
              } else if (result_addr->address == NULL) {
                /* No adjustment needed. */
              } else {
                get_mapped_byte_count(&persistent_map, bcp, offset);
                result_addr->address += offset;
                result_addr->flags &= ~CA_ARRAY_ELEMENT;
              }  /* if */
              if (tp->kind == (a_type_kind)tk_pointer) {
                *(a_constexpr_address*)result_storage = *result_addr;
              } else {
                /* The node may be rvalued, in which case slicing is
                   involved. */
                SET_result_val_from_operand_address(result_addr);
              }  /* if */
            } else {
              /* Slicing. */
              a_base_class_ptr     bcp;
              a_byte_count         offset;
              bcp = find_direct_base_class_of(opnd1_type, tp);
              get_mapped_byte_count(&persistent_map, bcp, offset);
              if (constexpr_copy_object(ips, tp, &expr->position,
                                        opnd1_value+offset, opnd1_value,
                                        result_storage, complete_object)) {
                record_subobject_derivation(result_storage, NULL);
              } else {
                do_constexpr_fail(result);
              }  /* if */
            }  /* if */
            break;
          case eok_derived_class_cast:
            { a_constexpr_address  *src = (a_constexpr_address*)opnd1_value;
              if (is_runtime_data_address(src)) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (!subobject_is_initialized(src->address,
                                                   src->complete_object)) {
                do_constexpr_fail(result);
                info_with_pos(ec_object_not_initialized, &opnd1->position,
                              ips);
              } else if (src->address == NULL) {
                *(a_constexpr_address*)result_storage = *src;
              } else {
                a_base_class_ptr  bcp = *(a_base_class_ptr*)src->address;
                a_type_ptr        derived_class;
                if (bcp != NULL) {
                  derived_class = bcp->derived_class;
                } else {
                  if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                    derived_class =
                              skip_typerefs(opnd1_type->variant.pointer.type);
                  } else {
                    derived_class = opnd1_type;
                  }  /* if */
                }  /* if */
                if (bcp != NULL && bcp->derived_class == derived_class) {
                  a_byte_count  offset;
                  get_mapped_byte_count(&persistent_map, bcp, offset);
                  src->address -= offset;
                  if (opnd1->is_lvalue || opnd1->is_xvalue) {
                    /* Presumably a cast to a reference. */
                    SET_result_val_from_operand_address(src);
                  } else {
                    /* A pointer-to-pointer cast. */
                    *(a_constexpr_address*)result_storage = *src;
                  }  /* if */
                } else {
                  do_constexpr_fail(result);
                  info_with_pos_type(ec_constexpr_bad_derived_class_cast,
                                     &expr->position, derived_class, ips);
                }  /* if */
              }  /* if */
            }
            break;
          case eok_pm_base_class_cast:
            /* Casting from X D::* to X B::*.  Since the same member is
               referred to, but B is at a positive offset from D, the offset
               stored in the pointer-to-member value must decrease. */
            { a_type_ptr           dtp, btp;
              a_base_class_ptr     bcp;
              a_byte_count         offset;
              a_constexpr_ptr_to_mem
                                   *pm_src, *pm_dst;
              pm_src = (a_constexpr_ptr_to_mem*)opnd1_value;
              pm_dst = (a_constexpr_ptr_to_mem*)result_storage;
              btp = tp->variant.ptr_to_member.class_of_which_a_member;
              btp = skip_typerefs(btp);
              dtp = opnd1_type->variant.ptr_to_member.class_of_which_a_member;
              dtp = skip_typerefs(dtp);
              bcp = find_direct_base_class_of(dtp, btp);
              get_mapped_byte_count(&persistent_map, bcp, offset);
              *pm_dst = *pm_src;
              if (pm_dst->subtract_adjustment) {
                pm_dst->this_class_adjustment += offset;
              } else {
                if (offset > pm_dst->this_class_adjustment) {
                  /* The adjustment changes direction. */
                  pm_dst->subtract_adjustment = TRUE;
                  pm_dst->this_class_adjustment =
                                       offset - pm_dst->this_class_adjustment;
                } else {
                  pm_dst->this_class_adjustment -= offset;
                }  /* if */
              }  /* if */
            }
            break;
          case eok_pm_derived_class_cast:
            /* Casting from X B::* to X D::*.  Since the same member is
               referred to, but B is at a positive offset from D, the offset
               stored in the pointer-to-member value must increase. */
            { a_type_ptr           dtp, btp;
              a_base_class_ptr     bcp;
              a_byte_count         offset;
              a_constexpr_ptr_to_mem
                                   *pm_src, *pm_dst;
              pm_src = (a_constexpr_ptr_to_mem*)opnd1_value;
              pm_dst = (a_constexpr_ptr_to_mem*)result_storage;
              btp = opnd1_type->variant.ptr_to_member
                                       .class_of_which_a_member;
              btp = skip_typerefs(btp);
              dtp = tp->variant.ptr_to_member.class_of_which_a_member;
              dtp = skip_typerefs(dtp);
              bcp = find_direct_base_class_of(dtp, btp);
              /* Ensure the derived class (and the base class) has been laid
                 out. */
              (void)f_value_bytes_for_type(ips, dtp, &result);
              if (!result) break;
              get_mapped_byte_count(&persistent_map, bcp, offset);
              *pm_dst = *pm_src;
              if (!pm_dst->subtract_adjustment) {
                pm_dst->this_class_adjustment += offset;
              } else {
                if (offset > pm_dst->this_class_adjustment) {
                  /* The adjustment changes direction. */
                  pm_dst->subtract_adjustment = FALSE;
                  pm_dst->this_class_adjustment =
                                       offset - pm_dst->this_class_adjustment;
                } else {
                  pm_dst->this_class_adjustment -= offset;
                }  /* if */
              }  /* if */
            }
            break;
          case eok_bool_cast:
            { a_boolean  bool_val;
              if (check_boolean_condition(ips, opnd1_value, opnd1, opnd1_type,
                                          &bool_val)) {
                *(an_integer_value *)result_storage = bool_val ? one_int
                                                               : zero_int;
              } else {
                do_constexpr_fail(result);
              }  /* if */
            }
            break;
          case eok_array_to_pointer:
            /* Usually, the operand is an lvalue and therefore we already have
               an address.  The resulting address is unchanged, but record the
               array characteristics.  (With C++11 initializers, array rvalues
               are possible too.) */
            { a_constexpr_address  *result_addr =
                                         (a_constexpr_address*)result_storage;
              a_targ_size_t        length;
              if (opnd1->is_lvalue || opnd1->is_xvalue) {
                *result_addr = *(a_constexpr_address *)opnd1_value;
               if (is_runtime_data_address(result_addr)) {
                  a_constant_ptr  orig_con = result_addr->variant.addr_con;
                  a_constant_ptr  new_con;
                  new_con = make_interpreter_copy_of_constant(ips, orig_con);
                  new_con->type = tp;
                  result_addr->variant.addr_con = new_con;
                  result_addr->flags |= CA_ARRAY_ELEMENT;
                  break;
                } else if (cannot_dereference(result_addr)) {
                  do_constexpr_fail(result);
                  info_one_past_end_of_array(result_addr, expr, ips);
                }  /* if */
              } else {
                /* The somewhat unusual case of an array rvalue. */
                clear_address(result_addr, opnd1_value);
                result_addr->alloc_seq_number = ips->curr_alloc_seq_number;
              }  /* if */
              result_addr->flags |= CA_ARRAY_ELEMENT;
              /* Check that the array length fits in interpreter limits. */
              length = opnd1_type->variant.array.variant.number_of_elements;
              if (length <= MAX_ARRAY_LENGTH) {
                result_addr->length = (unsigned int)length;
                if (is_variant_path(result_addr)) {
                  result_addr->variant.variant_path->base_address =
                                                         result_addr->address;
                } else {
                  result_addr->variant.base_address = result_addr->address;
                }  /* if */
              } else {
                /* Interpretation would have failed when the array size
                   was determined. */
                unexpected_condition();
              }  /* if */
            }
            break;
          case eok_dot_vacuous_destructor_call:
          case eok_points_to_vacuous_destructor_call:
            if (constexpr_dynamic_alloc_enabled) {
              /* Pseudo-destructors are permitted in constant expressions. */
              a_byte      *obj = NULL, *complete_obj;
              a_type_ptr  obj_type = NULL;
              if (type_is(opnd1_type, tk_pointer) || opnd1->is_lvalue ||
                  opnd1->is_xvalue) {
                /* The operand is an address. */
                a_constexpr_address  *cap = (a_constexpr_address*)opnd1_value;
                if (!is_runtime_data_address(cap)) {
                  complete_obj = cap->complete_object;
                  if (is_array_element(cap) || obj != complete_obj ||
                      is_immediate_class_type(opnd1_type) ||
                      type_is(opnd1_type, tk_array)) {
                    /* Not a complete object of scalar type.  Prepare to mark
                       as uninitialized the subobject (and any of its
                       substructure). */
                    obj = cap->address;
                    if (type_is(opnd1_type, tk_pointer)) {
                      obj_type = skip_typerefs(
                                            opnd1_type->variant.pointer.type);
                    } else {
                      obj_type = opnd1_type;
                    }  /* if */
                  }  /* if */
                } else {
                  /* An rvalue pseudo-destruction. */
                  if (is_immediate_class_type(opnd1_type) ||
                      type_is(opnd1_type, tk_array)) {
                    /* Not a scalar rvalue: Prepare to mark its substructure
                       as uninitialized. */
                    obj = opnd1_value;
                    obj_type = opnd1_type;
                  }  /* if */
                  complete_obj = opnd1_value;
                }  /* if */
                if (obj != NULL) {
                  /* Mark the substructure as uninitialized. */
                  if (!mark_whole_subobject_uninitialized(ips, obj, obj_type,
                                                          complete_obj)) {
                    result = FALSE;
                    break;
                  }  /* if */
                }  /* if */
                unmark_complete_object_initialized(complete_obj);
              }  /* if */
            } else {
              a_type_ptr  otp;
              if (node_operator_is(expr, eok_dot_vacuous_destructor_call)) {
                otp = opnd1_type;
              } else {
                otp = skip_typerefs(opnd1_type->variant.pointer.type);
              }  /* if */
              if (!is_immediate_class_type(otp)) {
                info_with_pos(ec_constexpr_vacuous_dtor_call,
                              &expr->position, ips);
              } else {
                info_with_pos(ec_constexpr_explicit_dtor_call,
                              &expr->position, ips);
              }  /* if */
              do_constexpr_fail(result);
            }  /* if */
            break;
#if MICROSOFT_EXTENSIONS_ALLOWED
          case eok_assume:
            /* This operator has no effect. */
            break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          case eok_negate:
            if (opnd1_type->kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              negate_integer_value((an_integer_value *)result_storage, &ovfl);
              CHECK_int_range((an_integer_value *)result_storage, tp);
            } else if (type_is_float_like(opnd1_type)) {
              err = FALSE;
              fp_negate(opnd1_type->variant.float_kind,
                        fp_value(opnd1_value), fp_value(result_storage),
                        &err, &depends_on_fp_mode);
              /* fp_negate should never fail. */
              check_assertion(!err);
#if C99_IL_EXTENSIONS_SUPPORTED
            } else if (tp->kind == (a_type_kind)tk_complex) {
              err = FALSE;
              cx_negate(opnd1_type->variant.float_kind,
                        cx_value(opnd1_value), cx_value(result_storage),
                        &err, &depends_on_fp_mode);
              /* fp_negate should never fail. */
              check_assertion(!err);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            } else {
              unexpected_condition();
            }  /* if */
            break;
          case eok_unary_plus:
            if (opnd1_type->kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              ovfl = FALSE;
              CHECK_int_range((an_integer_value *)result_storage, tp);
            } else if (type_is_float_like(opnd1_type)) {
              *fp_value(result_storage) = *fp_value(opnd1_value);
#if C99_IL_EXTENSIONS_SUPPORTED
            } else if (opnd1_type->kind == (a_type_kind)tk_complex) {
              *cx_value(result_storage) = *cx_value(opnd1_value);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
              *(a_constexpr_address *)result_storage =
                                          *(a_constexpr_address *)opnd1_value;
            } else {
              unexpected_condition();
            }  /* if */
            break;
          case eok_complement:
            if (opnd1_type->kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              ovfl = FALSE;
              complement_integer_value((an_integer_value *)result_storage);
              CHECK_int_range((an_integer_value *)result_storage, tp);
            } else {
              /* The complement operator only applies to integer types. */
              unexpected_condition();
            }  /* if */
            break;
          case eok_not:
            { a_boolean  bool_val;
              if (check_boolean_condition(ips, opnd1_value, opnd1, opnd1_type,
                                           &bool_val)) {
                if (bool_val) {
                  *(an_integer_value *)result_storage = zero_int;
                } else {
                  *(an_integer_value *)result_storage = one_int;
                }  /* if */
              } else {
                do_constexpr_fail(result);
              }  /* if */
            }
            break;
#if GNU_VECTOR_TYPES_ALLOWED
          case eok_vector_not:
            { a_byte  *src = opnd1_value, *dst = result_storage;
              a_type  *setp = skip_typerefs(opnd1_type
                                               ->variant.vector.element_type),
                      *detp = skip_typerefs(tp->variant.vector.element_type);
              int     k, len = tp->size/detp->size;
              a_byte_count
                      sstep = value_bytes_for_type(ips, setp, &result),
                      dstep = value_bytes_for_type(ips, detp, &result);
              for (k = 0; k<len; ++k) {
                a_boolean  bool_val;
                if (check_boolean_condition(ips, src, expr, setp, &bool_val)) {
                  if (bool_val) {
                    *(an_integer_value *)dst = zero_int;
                  } else {
                    *(an_integer_value *)dst = one_int;
                  }  /* if */
                } else {
                  do_constexpr_fail(result);
                  break;
                }  /* if */
                src += sstep;
                dst += dstep;
              }  /* for */
            }
            break;
          case eok_vector_fill:
            /* Copy the operand to every slot of the result. */
            { a_type_ptr  etp = skip_typerefs(tp->variant.vector.element_type);
              int         k, len = tp->size/etp->size;
              a_byte  *dst = result_storage;
              for (k = 0; k<len; ++k) {
                (void)memcpy(dst, opnd1_value, opnd_n_bytes);
                dst += opnd_n_bytes;
              }  /* for */
            }
            break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
          case eok_xconj:
            { err = FALSE;
              fp_negate(opnd1_type->variant.float_kind,
                        &cx_value(opnd1_value)->imag,
                        &cx_value(result_storage)->imag,
                        &err, &depends_on_fp_mode);
              /* fp_negate should never fail. */
              check_assertion(!err);
            }
            break;
          case eok_real_part:
            if (opnd1->is_lvalue || opnd1->is_xvalue) {
              a_constexpr_address  *src = (a_constexpr_address*)opnd1_value;
              if (is_runtime_data_address(src)) {
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (src->address == NULL) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (expr->is_lvalue || expr->is_xvalue) {
                /* Maintain the lvalue. */
                a_constexpr_address  *dst =
                                         (a_constexpr_address*)result_storage;
                *dst = *src;
                dst->address =
                   (a_byte*)&((an_internal_complex_value*)dst->address)->real;
              } else {
                /* Copy the real component as an rvalue. */
                *fp_value(result_storage) = cx_value_at(opnd1_value)->real;
              }  /* if */
            } else {
              /* __real applied to an rvalue. */
              *fp_value(result_storage) = cx_value(opnd1_value)->real;
            }  /* if */
            break;
          case eok_imag_part:
            if (opnd1->is_lvalue || opnd1->is_xvalue) {
              a_constexpr_address  *src = (a_constexpr_address*)opnd1_value;
              if (is_runtime_data_address(src)) {
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (src->address == NULL) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (expr->is_lvalue || expr->is_xvalue) {
                /* Maintain the lvalue. */
                a_constexpr_address  *dst =
                                         (a_constexpr_address*)result_storage;
                *dst = *src;
                dst->address =
                   (a_byte*)&((an_internal_complex_value*)dst->address)->imag;
              } else {
                /* Copy the real component as an rvalue. */
                *fp_value(result_storage) = cx_value_at(opnd1_value)->imag;
              }  /* if */
            } else {
              /* __real applied to an rvalue. */
              *fp_value(result_storage) = cx_value(opnd1_value)->imag;
            }  /* if */
            break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          case eok_post_incr:
            { a_constexpr_address  *cap = (a_constexpr_address*)opnd1_value;
              if (is_runtime_data_address(cap)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (cap->address == NULL) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else if (is_const_storage(cap)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else {
                /* Return a copy of the value stored at the operand address. */
                SET_result_val_from_operand_address(cap);
                /* Now increment the original value. */
                if (!result) {
                  /* Something was wrong with the operand address. */
                } else if (tp->kind == (a_type_kind)tk_integer) {
                  /* An integral type. */
                  an_integer_value  *ival = int_value_at(cap);
                  if (tp->variant.integer.bool_type) {
                    /* Incrementing a bool variable sets it to TRUE. */
                    *ival = one_int;
                  } else {
                    /* An integer. */
                    int_kind = tp->variant.integer.int_kind;
                    is_signed = int_kind_is_signed[int_kind];
                    add_integer_values(ival, &one_int, is_signed, &ovfl);
                    CHECK_int_range(ival, tp);
                    trim_bit_field_if_needed(cap, tp);
                  }  /* if */
                } else if (type_is_float_like(tp)) {
                  /* A floating-point type. */
                  fp_add(tp->variant.float_kind,
                         fp_value_at(cap),
                         &one_flt[(int)tp->variant.float_kind],
                         fp_value_at(cap), &err, &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  }  /* if */
                } else if (tp->kind == (a_type_kind)tk_pointer) {
                  /* A pointer. */
                  a_constexpr_address  *ptr;
                  ptr = (a_constexpr_address*)value_bytes_at(cap);
                  if (is_function_address(ptr)) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                  &expr->position, ips);
                  } else if (ptr->address == NULL &&
                             (!is_runtime_data_address(ptr) ||
                              constant_is(ptr->variant.addr_con,
                                          ck_integer))) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_invalid_null_ptr_operation,
                                  &expr->position, ips);
                  } else {
                    a_byte_count  elem_size, pos, len;
                    a_type_ptr    elem_type;
                    elem_type =
                              skip_typerefs(opnd1_type->variant.pointer.type);
                    get_array_pos(ips, ptr, elem_type, &len, &pos,
                                  &elem_size, &result);
                    if (!is_array_element(ptr) && cannot_dereference(ptr)) {
                      /* Non-arrays are treated as arrays of length one. */
                      do_constexpr_fail(result);
                      info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                    &expr->position, ips);
                    }  /* if */
                    if (!result) break;
                    if (len == pos) {
                      /* Out of bounds. */
                      do_constexpr_fail(result);
                      info_with_pos_num2(
                                       ec_constexpr_out_of_bounds_array_access,
                                       &expr->position, (unsigned long)(pos+1),
                                       (unsigned long)len, ips);
                    } else {
                      if (is_runtime_data_address(ptr)) {
                        if (!offset_runtime_address(
                                             ips, &expr->position, ptr,
                                             (a_host_large_integer)1,
                                             elem_size, /*subtract=*/FALSE)) {
                          do_constexpr_fail(result);
                        }  /* if */
                      } else {
                        ptr->address += elem_size;
                      }  /* if */
                      if (pos+1 == len) {
                        ptr->flags |= CA_CANNOT_DEREFERENCE;
                      }  /* if */
                    }  /* if */
                  }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
                } else if (tp->kind == (a_type_kind)tk_complex) {
                  /* A complex floating-point type. */
                  fp_add(tp->variant.float_kind, &cx_value_at(cap)->real,
                         &one_flt[(int)tp->variant.float_kind],
                         &cx_value_at(cap)->real, &err, &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
                } else {
                  /* Invalid type for postfix ++. */
                  unexpected_condition();
                }  /* if */
              }  /* if */
              break;
            }
          case eok_post_decr:
            { a_constexpr_address  *cap = (a_constexpr_address*)opnd1_value;
              if (is_runtime_data_address(cap)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (cap->address == NULL) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else if (is_const_storage(cap)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else {
                /* Return a copy of the value stored at the operand address. */
                SET_result_val_from_operand_address(cap);
                /* Now decrement the original value. */
                if (!result) {
                  /* Something was wrong with the operand address. */
                } else if (tp->kind == (a_type_kind)tk_integer) {
                  /* An integer. */
                  an_integer_value  *ival = int_value_at(cap);
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  subtract_mixed_signed_integer_values(
                                 ival, is_signed, &one_int, is_signed, &ovfl);
                  CHECK_int_range(ival, tp);
                  trim_bit_field_if_needed(cap, tp);
                } else if (type_is_float_like(tp)) {
                  /* A floating-point type. */
                  fp_subtract(tp->variant.float_kind,
                              fp_value_at(cap),
                              &one_flt[(int)tp->variant.float_kind],
                              fp_value_at(cap), &err,
                              &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  }  /* if */
                } else if (tp->kind == (a_type_kind)tk_pointer) {
                  /* A pointer. */
                  a_constexpr_address  *ptr;
                  ptr = (a_constexpr_address*)value_bytes_at(cap);
                  if (is_function_address(ptr)) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                  &expr->position, ips);
                  } else if (ptr->address == NULL &&
                             (!is_runtime_data_address(ptr) ||
                              constant_is(ptr->variant.addr_con,
                                          ck_integer))) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_invalid_null_ptr_operation,
                                  &expr->position, ips);
                  } else {
                    a_byte_count  elem_size, pos, len;
                    a_type_ptr    elem_type;
                    elem_type =
                              skip_typerefs(opnd1_type->variant.pointer.type);
                    get_array_pos(ips, ptr, elem_type, &len, &pos,
                                  &elem_size, &result);
                    if (!is_array_element(ptr) && !cannot_dereference(ptr)) {
                      do_constexpr_fail(result);
                      info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                    &expr->position, ips);
                    }  /* if */
                    if (!result) break;
                    if (pos < 1) {
                      /* Out of bounds. */
                      do_constexpr_fail(result);
                      info_with_pos(ec_constexpr_pointer_ahead_of_array,
                                    &expr->position, ips);
                    } else {
                      if (is_runtime_data_address(ptr)) {
                        if (!offset_runtime_address(
                                              ips, &expr->position, ptr,
                                               (a_host_large_integer)1,
                                              elem_size, /*subtract=*/TRUE)) {
                          do_constexpr_fail(result);
                        }  /* if */
                      } else {
                        ptr->address -= elem_size;
                      }  /* if */
                      if (pos == len) {
                        ptr->flags &= ~CA_CANNOT_DEREFERENCE;
                      }  /* if */
                    }  /* if */
                  }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
                } else if (tp->kind == (a_type_kind)tk_complex) {
                  /* A complex floating-point type. */
                  fp_subtract(tp->variant.float_kind, &cx_value_at(cap)->real,
                              &one_flt[(int)tp->variant.float_kind],
                              &cx_value_at(cap)->real,
                              &err, &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
                } else {
                  /* Invalid type for prefix --. */
                  unexpected_condition();
                }  /* if */
              }  /* if */
            }
            break;
          case eok_pre_incr:
            { a_constexpr_address  *cap = (a_constexpr_address*)opnd1_value;
              if (is_runtime_data_address(cap)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (cap->address == NULL) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else if (!is_initialized(cap)) {
                do_constexpr_fail(result);
                info_with_pos(ec_object_not_initialized, &expr->position, ips);
              } else if (is_variant_path(cap) &&
                         !check_variant_path(ips, cap, &expr->position)) {
                /* An attempt to dereference an inactive variant path. */
                do_constexpr_fail(result);
              } else if (is_const_storage(cap)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (tp->kind == (a_type_kind)tk_integer) {
                /* An integral type. */
                if (tp->variant.integer.bool_type) {
                  /* Incrementing a bool variable sets it to TRUE. */
                  *(an_integer_value *)value_bytes_at(cap) = one_int;
                } else {
                  /* An integer. */
                  an_integer_value  *ival = int_value_at(cap);
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  add_integer_values(ival, &one_int, is_signed, &ovfl);
                  CHECK_int_range(ival, tp);
                  trim_bit_field_if_needed(cap, tp);
                }  /* if */
              } else if (type_is_float_like(tp)) {
                /* A floating-point type. */
                fp_add(tp->variant.float_kind,
                       fp_value_at(cap),
                       &one_flt[(int)tp->variant.float_kind],
                       fp_value_at(cap), &err, &depends_on_fp_mode);
                if (err) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                }  /* if */
              } else if (tp->kind == (a_type_kind)tk_pointer) {
                /* A pointer. */
                a_constexpr_address  *ptr;
                ptr = (a_constexpr_address*)value_bytes_at(cap);
                if (is_function_address(ptr)) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                &expr->position, ips);
                } else if (ptr->address == NULL &&
                           (!is_runtime_data_address(ptr) ||
                            constant_is(ptr->variant.addr_con, ck_integer))) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_invalid_null_ptr_operation,
                                &expr->position, ips);
                } else {
                  a_byte_count  elem_size, pos, len;
                  a_type_ptr    elem_type;
                  elem_type = skip_typerefs(opnd1_type->variant.pointer.type);
                  get_array_pos(ips, ptr, elem_type, &len, &pos,
                                &elem_size, &result);
                  if (!is_array_element(ptr) && cannot_dereference(ptr)) {
                    /* Non-arrays are treated as arrays of length one. */
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                  &expr->position, ips);
                  }  /* if */
                  if (!result) break;
                  if (len == pos) {
                    /* Out of bounds. */
                    do_constexpr_fail(result);
                    info_with_pos_num2(ec_constexpr_out_of_bounds_array_access,
                                       &expr->position, (unsigned long)(pos+1),
                                       (unsigned long)len, ips);
                  } else {
                    if (is_runtime_data_address(ptr)) {
                      if (!offset_runtime_address(
                                             ips, &expr->position, ptr,
                                             (a_host_large_integer)1,
                                             elem_size, /*subtract=*/FALSE)) {
                        do_constexpr_fail(result);
                      }  /* if */
                    } else {
                      ptr->address += elem_size;
                    }  /* if */
                    if (pos+1 == len) {
                      ptr->flags |= CA_CANNOT_DEREFERENCE;
                    }  /* if */
                  }  /* if */
                }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
              } else if (tp->kind == (a_type_kind)tk_complex) {
                fp_add(tp->variant.float_kind,
                       fp_value_at(cap),
                       &one_flt[(int)tp->variant.float_kind],
                       fp_value_at(cap), &err, &depends_on_fp_mode);
                if (err) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              } else {
                /* Invalid type for prefix ++. */
                unexpected_condition();
              }  /* if */
              if (result) {
                /* Return either the address or the value, as
                   appropriate. */
                SET_result_val_from_operand_address(cap);
              }  /* if */
            }
            break;
          case eok_pre_decr:
            { a_constexpr_address  *cap = (a_constexpr_address*)opnd1_value;
              a_constexpr_address  orig_address = *cap;
              copy_address_structures(&orig_address);
              if (is_runtime_data_address(cap)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (cap->address == NULL) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else if (!is_initialized(cap)) {
                do_constexpr_fail(result);
                info_with_pos(ec_object_not_initialized, &expr->position, ips);
              } else if (is_variant_path(cap) &&
                         !check_variant_path(ips, cap, &expr->position)) {
                /* An attempt to dereference an inactive variant path. */
                do_constexpr_fail(result);
              } else if (is_const_storage(cap)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (tp->kind == (a_type_kind)tk_integer) {
                /* An integer. */
                an_integer_value  *ival = int_value_at(cap);
                int_kind = tp->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                subtract_mixed_signed_integer_values(
                                 ival, is_signed, &one_int, is_signed, &ovfl);
                CHECK_int_range(ival, tp);
                trim_bit_field_if_needed(cap, tp);
              } else if (type_is_float_like(tp)) {
                /* A floating-point type. */
                fp_subtract(tp->variant.float_kind,
                            fp_value_at(cap),
                            &one_flt[(int)tp->variant.float_kind],
                            fp_value_at(cap), &err,
                            &depends_on_fp_mode);
                if (err) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                }  /* if */
              } else if (tp->kind == (a_type_kind)tk_pointer) {
                /* A pointer. */
                a_constexpr_address  *ptr;
                ptr = (a_constexpr_address*)value_bytes_at(cap);
                if (is_function_address(ptr)) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                &expr->position, ips);
                } else if (ptr->address == NULL &&
                           (!is_runtime_data_address(ptr) ||
                            constant_is(ptr->variant.addr_con, ck_integer))) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_invalid_null_ptr_operation,
                                &expr->position, ips);
                } else {
                  a_byte_count  elem_size, pos, len;
                  a_type_ptr    elem_type;
                  elem_type =
                            skip_typerefs(opnd1_type->variant.pointer.type);
                  get_array_pos(ips, ptr, elem_type, &len, &pos,
                                &elem_size, &result);
                  if (!is_array_element(ptr) && !cannot_dereference(ptr)) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                  &expr->position, ips);
                  }  /* if */
                  if (!result) break;
                  if (pos < 1) {
                    /* Out of bounds. */
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_pointer_ahead_of_array,
                                  &expr->position, ips);
                  } else {
                    if (is_runtime_data_address(ptr)) {
                      if (!offset_runtime_address(
                                              ips, &expr->position, ptr,
                                              (a_host_large_integer)1,
                                              elem_size, /*subtract=*/TRUE)) {
                        do_constexpr_fail(result);
                      }  /* if */
                    } else {
                      ptr->address -= elem_size;
                    }  /* if */
                    if (pos == len) {
                      ptr->flags &= ~CA_CANNOT_DEREFERENCE;
                    }  /* if */
                  }  /* if */
                }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
              } else if (tp->kind == (a_type_kind)tk_complex) {
                fp_subtract(tp->variant.float_kind, &cx_value_at(cap)->real,
                            &one_flt[(int)tp->variant.float_kind],
                            &cx_value_at(cap)->real,
                            &err, &depends_on_fp_mode);
                if (err) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              } else {
                /* Invalid type for prefix --. */
                unexpected_condition();
              }  /* if */
              if (result) {
                /* Return either the address or the value, as
                   appropriate. */
                SET_result_val_from_operand_address(&orig_address);
              }  /* if */
            }
            break;
          case eok_add:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              add_integer_values((an_integer_value*)result_storage,
                                 (an_integer_value*)opnd2_value,
                                 is_signed, &ovfl);
              CHECK_int_range((an_integer_value*)(result_storage), tp);
            } else if (type_kind_is_float_like(
                                         expr->variant.operation.type_kind)) {
              fp_add(tp->variant.float_kind,
                     fp_value(opnd1_value), fp_value(opnd2_value),
                     fp_value(result_storage), &err, &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
            } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_complex) {
              cx_add(tp->variant.float_kind,
                     cx_value(opnd1_value), cx_value(opnd2_value),
                     cx_value(result_storage), &err, &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if GNU_VECTOR_TYPES_ALLOWED
            } else if (expr->variant.operation.type_kind ==
                                                     (a_type_kind)tk_vector) {
              if (!do_constexpr_vector_binary_op(
                       ips, expr, tp, opnd1_value, opnd2_value,
                                         result_storage, complete_object)) {
                result = FALSE;
              }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
            } else {
              /* Other types. */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                            &expr->position, ips);
            }  /* if */
            break;
          case eok_subtract:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              subtract_integer_values((an_integer_value*)result_storage,
                                      (an_integer_value*)opnd2_value,
                                      is_signed, &ovfl);
              CHECK_int_range((an_integer_value*)(result_storage), tp);
            } else if (type_kind_is_float_like(
                                         expr->variant.operation.type_kind)) {
              fp_subtract(tp->variant.float_kind,
                          fp_value(opnd1_value), fp_value(opnd2_value),
                          fp_value(result_storage), &err,
                          &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
            } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_complex) {
              cx_subtract(tp->variant.float_kind,
                          cx_value(opnd1_value), cx_value(opnd2_value),
                          cx_value(result_storage), &err, &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if GNU_VECTOR_TYPES_ALLOWED
            } else if (expr->variant.operation.type_kind ==
                                                     (a_type_kind)tk_vector) {
              if (!do_constexpr_vector_binary_op(
                       ips, expr, tp, opnd1_value, opnd2_value,
                                         result_storage, complete_object)) {
                result = FALSE;
              }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
            } else {
              /* Other types. */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                            &expr->position, ips);
            }  /* if */
            break;
          case eok_multiply:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              multiply_integer_values((an_integer_value*)result_storage,
                                      (an_integer_value*)opnd2_value,
                                      is_signed, &ovfl);
              CHECK_int_range((an_integer_value*)(result_storage), tp);
            } else if (type_kind_is_float_like(
                                         expr->variant.operation.type_kind)) {
              fp_multiply(tp->variant.float_kind,
                          fp_value(opnd1_value), fp_value(opnd2_value),
                          fp_value(result_storage), &err,
                          &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
            } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_complex) {
              cx_multiply(tp->variant.float_kind,
                          cx_value(opnd1_value), cx_value(opnd2_value),
                          cx_value(result_storage), &err, &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            } else {
              /* Other types. */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                            &expr->position, ips);
            }  /* if */
            break;
          case eok_divide:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              divide_integer_values((an_integer_value*)result_storage,
                                    (an_integer_value*)opnd2_value,
                                    is_signed, &ovfl);
              if (ovfl) {
                do_constexpr_fail(result);
                info_with_pos(ec_integer_overflow, &expr->position, ips);
              }  /* if */
            } else if (type_kind_is_float_like(
                                         expr->variant.operation.type_kind)) {
              fp_divide(tp->variant.float_kind,
                        fp_value(opnd1_value), fp_value(opnd2_value),
                        fp_value(result_storage), &err,
                        &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
            } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_complex) {
              cx_divide(tp->variant.float_kind,
                        cx_value(opnd1_value), cx_value(opnd2_value),
                        cx_value(result_storage), &err, &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            } else {
              /* Other types. */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                            &expr->position, ips);
            }  /* if */
            break;
          case eok_remainder:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              remainder_integer_values((an_integer_value*)result_storage,
                                       (an_integer_value*)opnd2_value,
                                       is_signed, &ovfl);
              if (ovfl) {
                do_constexpr_fail(result);
                info_with_pos(ec_integer_overflow, &expr->position, ips);
              }  /* if */
            } else {
              unexpected_condition();
            }  /* if */
            break;
#if C99_IL_EXTENSIONS_SUPPORTED
          case eok_jmultiply:
            fp_multiply(tp->variant.float_kind,
                        fp_value(opnd1_value), fp_value(opnd2_value),
                        fp_value(result_storage), &err,
                        &depends_on_fp_mode);
            if (err) {
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
            } else {
              fp_negate(opnd1_type->variant.float_kind,
                        fp_value(result_storage), fp_value(result_storage),
                        &err, &depends_on_fp_mode);
              /* fp_negate should never fail. */
              check_assertion(!err);
            }  /* if */
            break;
          case eok_jdivide:
            fp_divide(tp->variant.float_kind,
                      fp_value(opnd1_value), fp_value(opnd2_value),
                      fp_value(result_storage), &err,
                      &depends_on_fp_mode);
            if (err) {
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
            } else {
              fp_negate(opnd1_type->variant.float_kind,
                        fp_value(result_storage), fp_value(result_storage),
                        &err, &depends_on_fp_mode);
              /* fp_negate should never fail. */
              check_assertion(!err);
            }  /* if */
            break;
          case eok_fjsubtract:
            fp_negate(opnd2_type->variant.float_kind,
                      fp_value(opnd2_value), fp_value(opnd2_value),
                      &err, &depends_on_fp_mode);
            FALLTHROUGH
          case eok_fjadd:
            cx_value(result_storage)->real = *fp_value(opnd1_value);
            cx_value(result_storage)->imag = *fp_value(opnd2_value);
            break;
          case eok_jfsubtract:
            fp_negate(opnd2_type->variant.float_kind,
                      fp_value(opnd2_value), fp_value(opnd2_value),
                      &err, &depends_on_fp_mode);
            FALLTHROUGH
          case eok_jfadd:
            cx_value(result_storage)->real = *fp_value(opnd2_value);
            cx_value(result_storage)->imag = *fp_value(opnd1_value);
            break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          case eok_padd:
            /* Pointer + integer or integer + pointer. */
            { a_constexpr_address  *result_addr;
              a_type_ptr           elem_type;
              result_addr = (a_constexpr_address*)result_storage;
              if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                get_int_val_from(opnd2_value, opnd2_type, host_int_val, ovfl);
                *result_addr = *(a_constexpr_address *)opnd1_value;
                elem_type = skip_typerefs(opnd1_type->variant.pointer.type);
              } else {
                get_int_val_from(opnd1_value, opnd1_type, host_int_val, ovfl);
                *result_addr = *(a_constexpr_address *)opnd2_value;
                elem_type = skip_typerefs(opnd2_type->variant.pointer.type);
              }  /* if */
              if (ovfl) {
                do_constexpr_fail(result);
                info_with_pos(ec_integer_overflow, &expr->position, ips);
              } else if (host_int_val == 0) {
                /* Leave the address unchanged. */
              } else if (is_function_address(result_addr)) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                              &expr->position, ips);
              } else if (result_addr->address == NULL &&
                         (!is_runtime_data_address(result_addr) ||
                          constant_is(result_addr->variant.addr_con,
                                      ck_integer))) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_invalid_null_ptr_operation,
                              &expr->position, ips);
              } else {
                a_byte_count  elem_size, pos, len;
                get_array_pos(ips, result_addr, elem_type, &len, &pos,
                              &elem_size, &result);
                if (!is_array_element(result_addr) &&
                    host_int_val !=
                                 (cannot_dereference(result_addr) ? -1 : 1)) {
                  /* Non-arrays are treated as arrays of length one. */
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                &expr->position, ips);
                }  /* if */
                if (!result) break;
                if (host_int_val > 0 ? (len-pos < (a_byte_count)host_int_val)
                                     : (pos < (a_byte_count)-host_int_val)) {
                  /* Out of bounds. */
                  do_constexpr_fail(result);
                  if (host_int_val > 0) {
                    info_with_pos_num2(ec_constexpr_out_of_bounds_array_access,
                                       &expr->position,
                                       (uint32_t)(pos+host_int_val),
                                       (uint32_t)len, ips);
                  } else {
                    info_with_pos(ec_constexpr_pointer_ahead_of_array,
                                  &expr->position, ips);
                  }  /* if */
                } else {
                  if (is_runtime_data_address(result_addr)) {
                    if (!offset_runtime_address(
                               ips, &expr->position, result_addr,
                               host_int_val, elem_size, /*subtract=*/FALSE)) {
                      do_constexpr_fail(result);
                    }  /* if */
                  } else {
                    result_addr->address += host_int_val * elem_size;
                  }  /* if */
                  if (pos+host_int_val == len) {
                    result_addr->flags |= CA_CANNOT_DEREFERENCE;
                  } else {
                    result_addr->flags &= ~CA_CANNOT_DEREFERENCE;
                  }  /* if */
                }  /* if */
              }  /* if */
            }
            break;
          case eok_psubtract:
            /* Pointer - integer or integer - pointer. */
            { a_constexpr_address  *result_addr;
              a_type_ptr           elem_type;
              result_addr = (a_constexpr_address*)result_storage;
              if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                get_int_val_from(opnd2_value, opnd2_type, host_int_val, ovfl);
                *result_addr = *(a_constexpr_address *)opnd1_value;
                elem_type = skip_typerefs(opnd1_type->variant.pointer.type);
              } else {
                get_int_val_from(opnd1_value, opnd1_type, host_int_val, ovfl);
                *result_addr = *(a_constexpr_address *)opnd2_value;
                elem_type = skip_typerefs(opnd2_type->variant.pointer.type);
              }  /* if */
              if (ovfl) {
                do_constexpr_fail(result);
                info_with_pos(ec_integer_overflow, &expr->position, ips);
              } else if (host_int_val == 0) {
                /* Leave the address unchanged. */
              } else if (is_function_address(result_addr)) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                              &expr->position, ips);
              } else if (result_addr->address == NULL &&
                         (!is_runtime_data_address(result_addr) ||
                          constant_is(result_addr->variant.addr_con,
                                      ck_integer))) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_invalid_null_ptr_operation,
                              &expr->position, ips);
              } else {
                a_byte_count  elem_size, pos, len;
                get_array_pos(ips, result_addr, elem_type, &len, &pos,
                              &elem_size, &result);
                if (!is_array_element(result_addr) &&
                    host_int_val !=
                                 (cannot_dereference(result_addr) ? 1 : -1)) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                &expr->position, ips);
                }  /* if */
                if (!result) break;
                if (host_int_val > 0 ?
                                    (pos < (a_byte_count)host_int_val)
                                  : (len-pos < (a_byte_count)-host_int_val)) {
                  /* Out of bounds. */
                  do_constexpr_fail(result);
                  if (host_int_val < 0) {
                    info_with_pos_num2(ec_constexpr_out_of_bounds_array_access,
                                       &expr->position,
                                       (uint32_t)(pos-host_int_val),
                                       (uint32_t)len, ips);
                  } else {
                    info_with_pos(ec_constexpr_pointer_ahead_of_array,
                                  &expr->position, ips);
                  }  /* if */
                } else {
                  if (is_runtime_data_address(result_addr)) {
                    if (!offset_runtime_address(
                               ips, &expr->position, result_addr,
                               host_int_val, elem_size, /*subtract=*/TRUE)) {
                      do_constexpr_fail(result);
                    }  /* if */
                  } else {
                    result_addr->address -= host_int_val * elem_size;
                  }  /* if */
                  if (pos - host_int_val == len) {
                    result_addr->flags |= CA_CANNOT_DEREFERENCE;
                  } else {
                    result_addr->flags &= ~CA_CANNOT_DEREFERENCE;
                  }  /* if */
                }  /* if */
              }  /* if */
            }
            break;
          case eok_pdiff:
            { a_constexpr_address  *addr1, *addr2;
              addr1 = (a_constexpr_address*)opnd1_value;
              addr2 = (a_constexpr_address*)opnd2_value;
              if (is_runtime_data_address(addr1) !=
                                             is_runtime_data_address(addr2)) {
                info_with_pos(ec_constexpr_invalid_pdiff, 
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_runtime_data_address(addr1)) {
                a_constant_ptr     diff_con = local_constant();
                a_boolean          did_not_fold;
                an_error_code      err_code;
                an_error_severity  sev;
                clear_constant(diff_con, (a_constant_repr_kind)ck_integer);
                diff_con->type = tp;
                do_pdiff(addr1->variant.addr_con, addr2->variant.addr_con,
                         diff_con, &did_not_fold, &err_code, &sev);
                if (did_not_fold) {
                  do_constexpr_fail(result);
                  info_with_pos(err_code, &expr->position, ips);
                } else {
                  result = copy_val_from_constant(
                               ips, diff_con, result_storage, result_storage);
                }  /* if */
                release_local_constant(&diff_con);
              } else {
                a_type_ptr    etp = opnd1_type->variant.pointer.type;
                a_byte_count  elem_size;
                etp = skip_typerefs(etp);
                elem_size = value_bytes_for_type(ips, etp, &result);
                if (!result) break;
                if (is_array_element(addr1) && is_array_element(addr2) &&
                    get_base_address(addr1) == get_base_address(addr2)) {
                  if (elem_size == 0) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_divide_by_zero, &expr->position, ips);
                  } else {
                    set_integer_value(
                       (an_integer_value*)result_storage,
                       (a_host_large_integer)(addr1->address - addr2->address)
                          / (a_host_large_integer)elem_size);
                    int_kind = tp->variant.integer.int_kind;
                    is_signed = int_kind_is_signed[int_kind];
                    ovfl = FALSE;
                    CHECK_int_range((an_integer_value*)result_storage, tp);
                  }  /* if */
                } else if (addr1->address == addr2->address) {
                  /* Subtracting two equal pointers produces zero (also true
                     for NULL pointers). */
                  *(an_integer_value *)result_storage = zero_int;
                } else if (cannot_dereference(addr1) &&
                           addr1->address-elem_size == addr2->address) {
                  /* Something like (&x+1) - &x for a non-array variable x. */
                  set_integer_value((an_integer_value*)result_storage,
                                    (a_host_large_integer)1);
                } else if (cannot_dereference(addr2) &&
                           addr2->address-elem_size == addr1->address) {
                  /* Something like &x - &(x+1) for a non-array variable x. */
                  set_integer_value((an_integer_value*)result_storage,
                                    (a_host_large_integer)-1);
                } else {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                &expr->position, ips);
                }  /* if */
              }  /* if */
            }
            break;
          case eok_shiftl:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              /* Check for a valid value of opnd2, which must be non-negative
                 and less than the number of bits in opnd1. */
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              get_int_val_from(opnd2_value, opnd2_type, host_int_val, ovfl);
              if (ovfl) {
                do_constexpr_fail(result);
              } else if (host_int_val < 0 ||
                         host_int_val >=
                            (a_host_large_integer)(tp->size * targ_char_bit)) {
                do_constexpr_fail(result);
              }  /* if */
              if (result) {
                /* Range checking for the "shift left" operator applied to a
                   signed value is a little peculiar.  Shifting negative values
                   has undefined behavior (which must be caught during constant
                   evaluation).  Shifting non-negative values is valid if the
                   result of the shift operation fits in the corresponding
                   unsigned type. */
                if (is_signed) {
                  if (sign_of(*(an_integer_value*)opnd1_value)) {
                    info_with_pos(ec_constexpr_shift_negative_value,
                                  &expr->position, ips);
                    do_constexpr_fail(result);
                    break;
                  }  /* if */
                }  /* if */
                shift_left_integer_value((an_integer_value*)opnd1_value,
                                         (int)host_int_val, &ovfl);
                if (is_signed) {
                  if (ovfl ||
                      cmp_integer_values((an_integer_value*)opnd1_value,
                                         /*op1_is_signed=*/FALSE,
                                         &max_integer_value_of_kind[
                                              unsigned_int_kind_of[int_kind]],
                                         /*op2_is_signed=*/FALSE) > 0) {
                    do_constexpr_fail(result);
                    info_with_pos_type(ec_constexpr_integer_overflow,
                                       &expr->position, opnd1_type, ips);
                    break;
                  }  /* if */
                  /* Sign-extend the result. */
                  sign_extend_integer_value((an_integer_value*)opnd1_value,
                                            (int)(tp->size * targ_char_bit));
                } else {
                  /* Discard overflowing bit. */
                  and_integer_values((an_integer_value*)opnd1_value,
                                     &max_integer_value_of_kind[int_kind]);
                }  /* if */
                *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              } else {
                info_with_pos(ec_integer_overflow, &expr->position, ips);
              }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
            } else if (expr->variant.operation.type_kind ==
                                                     (a_type_kind)tk_vector) {
              if (!do_constexpr_vector_binary_op(
                       ips, expr, tp, opnd1_value, opnd2_value,
                                         result_storage, complete_object)) {
                result = FALSE;
              }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
            } else {
              /* Other types. */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                            &expr->position, ips);
            }  /* if */
            break;
          case eok_shiftr:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              /* Check for a valid value of opnd2, which must be non-negative
                 and less than the number of bits in opnd1. */
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              get_int_val_from(opnd2_value, opnd2_type, host_int_val, ovfl);
              if (ovfl) {
                do_constexpr_fail(result);
              } else if (host_int_val < 0 ||
                         host_int_val >=
                           (a_host_large_integer)(tp->size * targ_char_bit)) {
                do_constexpr_fail(result);
              }  /* if */
              if (result) {
                shift_right_integer_value((an_integer_value *)opnd1_value,
                                          (int)host_int_val, is_signed,
                                          targ_right_shift_is_arithmetic);
                CHECK_int_range(opnd1_value, opnd1_type);
                if (result) {
                  *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
                }  /* if */
              } else {
                do_constexpr_fail(result);
                info_with_pos(ec_integer_overflow, &expr->position, ips);
              }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
            } else if (expr->variant.operation.type_kind ==
                                                     (a_type_kind)tk_vector) {
              if (!do_constexpr_vector_binary_op(
                       ips, expr, tp, opnd1_value, opnd2_value,
                                         result_storage, complete_object)) {
                result = FALSE;
              }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
            } else {
              /* Other types. */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                            &expr->position, ips);
            }  /* if */
            break;
          case eok_and:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              and_integer_values((an_integer_value*)result_storage,
                                 (an_integer_value*)opnd2_value);
#if GNU_VECTOR_TYPES_ALLOWED
            } else if (expr->variant.operation.type_kind ==
                                                     (a_type_kind)tk_vector) {
              if (!do_constexpr_vector_binary_op(
                       ips, expr, tp, opnd1_value, opnd2_value,
                                         result_storage, complete_object)) {
                result = FALSE;
              }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
            } else {
              /* Other types. */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                            &expr->position, ips);
            }  /* if */
            break;
          case eok_or:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              or_integer_values((an_integer_value*)result_storage,
                                (an_integer_value*)opnd2_value);
#if GNU_VECTOR_TYPES_ALLOWED
            } else if (expr->variant.operation.type_kind ==
                                                     (a_type_kind)tk_vector) {
              if (!do_constexpr_vector_binary_op(
                       ips, expr, tp, opnd1_value, opnd2_value,
                                         result_storage, complete_object)) {
                result = FALSE;
              }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
            } else {
              /* Other types. */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                            &expr->position, ips);
            }  /* if */
            break;
          case eok_xor:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              xor_integer_values((an_integer_value*)result_storage,
                                 (an_integer_value*)opnd2_value);
#if GNU_VECTOR_TYPES_ALLOWED
            } else if (expr->variant.operation.type_kind ==
                                                     (a_type_kind)tk_vector) {
              if (!do_constexpr_vector_binary_op(
                       ips, expr, tp, opnd1_value, opnd2_value,
                                         result_storage, complete_object)) {
                result = FALSE;
              }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
            } else {
              /* Other types. */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                            &expr->position, ips);
            }  /* if */
            break;
          case eok_eq:
            if (opnd1_type->kind == (a_type_kind)tk_integer) {
              /* Integral operands. */
              int_kind = opnd1_type->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              if (cmp_integer_values((an_integer_value *)opnd1_value,
                                     is_signed,
                                     (an_integer_value *)opnd2_value,
                                     is_signed) == 0) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
            } else if (type_is_float_like(opnd1_type)) {
              /* Floating-point operands. */
              if (fp_compare(opnd1_type->variant.float_kind,
                             fp_value(opnd1_value),
                             fp_value(opnd2_value),
                             &unord) == 0 && !unord) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
              /* Pointer operands. */
              a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
              a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
              if (compatible_address_kinds(ptr1, ptr2)) {
                if (is_function_address(ptr1) || is_function_address(ptr2)) {
                  if (is_function_address(ptr1) && is_function_address(ptr2) &&
                      ptr1->variant.routine == ptr2->variant.routine) {
                    *(an_integer_value *)result_storage = one_int;
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
                  }  /* if */
                } else if (!is_runtime_data_address(ptr1)) {
                  if (ptr1->address == ptr2->address &&
                      ptr1->complete_object == ptr2->complete_object) {
                    *(an_integer_value *)result_storage = one_int;
                  } else if (((ptr1->address == ptr1->complete_object &&
                               cannot_dereference(ptr2)) ||
                              (ptr2->address == ptr2->complete_object &&
                               cannot_dereference(ptr1))) &&
                             ptr1->complete_object != ptr2->complete_object) {
                    info_with_pos(ec_constexpr_equality_past_the_end_address,
                                  &expr->position, ips);
                    do_constexpr_fail(result);
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
                  }  /* if */
                } else {
                  /* Two runtime data pointers. */
                  int  cmp;
                  if (compare_address_constants(ptr1->variant.addr_con,
                                                ptr2->variant.addr_con,
                                                &cmp)) {
                    *(an_integer_value *)result_storage = cmp ? zero_int:
                                                                one_int;
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
                  }  /* if */
                }  /* if */
              }  /* if */
              release_variant_path_if_needed(ptr1);
              release_variant_path_if_needed(ptr2);
            } else if (opnd1_type->kind == (a_type_kind)tk_ptr_to_member) {
              a_constexpr_ptr_to_mem  *pm1, *pm2;
              pm1 = (a_constexpr_ptr_to_mem*)opnd1_value;
              pm2 = (a_constexpr_ptr_to_mem*)opnd2_value;
              if (pm1->subtract_adjustment != pm2->subtract_adjustment ||
                  pm1->this_class_adjustment != pm2->this_class_adjustment) {
                *(an_integer_value *)result_storage = zero_int;
              } else if (pm1->is_ptr_to_mem_function) {
                if (pm2->is_ptr_to_mem_function &&
                    pm1->variant.routine == pm2->variant.routine) {
                  *(an_integer_value *)result_storage = one_int;
                } else {
                  *(an_integer_value *)result_storage = zero_int;
                }  /* if */
              } else {
                if (!pm2->is_ptr_to_mem_function &&
                    pm1->variant.field == pm2->variant.field) {
                  *(an_integer_value *)result_storage = one_int;
                } else {
                  *(an_integer_value *)result_storage = zero_int;
                }  /* if */
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_nullptr) {
              /* Two nullptr values always compare equal. */
              *(an_integer_value *)result_storage = one_int;
#if C99_IL_EXTENSIONS_SUPPORTED
            } else if (tp->kind == (a_type_kind)tk_complex) {
              /* A complex floating-point type. */
              *(an_integer_value *)result_storage =
                 cx_equal(tp->variant.float_kind,
                          cx_value(opnd1_value), cx_value(opnd2_value)) ?
                                                           one_int : zero_int;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            } else {
              unexpected_condition();
            }  /* if */
            break;
          case eok_ne:
            if (opnd1_type->kind == (a_type_kind)tk_integer) {
              /* Integral operands. */
              int_kind = opnd1_type->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              if (cmp_integer_values((an_integer_value *)opnd1_value,
                                     is_signed,
                                     (an_integer_value *)opnd2_value,
                                     is_signed) != 0) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
            } else if (type_is_float_like(opnd1_type)) {
              /* Floating-point operands. */
              if (fp_compare(opnd1_type->variant.float_kind,
                             fp_value(opnd1_value),
                             fp_value(opnd2_value),
                             &unord) != 0 || unord) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
              /* Pointer operands. */
              a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
              a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
              if (compatible_address_kinds(ptr1, ptr2)) {
                if (is_function_address(ptr1) || is_function_address(ptr2)) {
                  if (is_function_address(ptr1) && is_function_address(ptr2) &&
                      ptr1->variant.routine == ptr2->variant.routine) {
                    *(an_integer_value *)result_storage = zero_int;
                  } else {
                    *(an_integer_value *)result_storage = one_int;
                  }  /* if */
                } else if (!is_runtime_data_address(ptr1)) {
                  if (ptr1->address == ptr2->address &&
                      ptr1->complete_object == ptr2->complete_object) {
                    *(an_integer_value *)result_storage = zero_int;
                  } else if (((ptr1->address == ptr1->complete_object &&
                               cannot_dereference(ptr2)) ||
                              (ptr2->address == ptr2->complete_object &&
                               cannot_dereference(ptr1))) &&
                             ptr1->complete_object != ptr2->complete_object) {
                    info_with_pos(ec_constexpr_equality_past_the_end_address,
                                  &expr->position, ips);
                    do_constexpr_fail(result);
                  } else {
                    *(an_integer_value *)result_storage = one_int;
                  }  /* if */
                } else {
                  /* Two runtime data pointers. */
                  int  cmp;
                  if (compare_address_constants(ptr1->variant.addr_con,
                                                ptr2->variant.addr_con,
                                                &cmp)) {
                    *(an_integer_value *)result_storage = cmp ? one_int:
                                                                zero_int;
                  } else {
                    *(an_integer_value *)result_storage = one_int;
                  }  /* if */
                }  /* if */
              } else {
                do_constexpr_fail(result);
              }  /* if */
              release_variant_path_if_needed(ptr1);
              release_variant_path_if_needed(ptr2);
            } else if (opnd1_type->kind == (a_type_kind)tk_ptr_to_member) {
              a_constexpr_ptr_to_mem  *pm1, *pm2;
              pm1 = (a_constexpr_ptr_to_mem*)opnd1_value;
              pm2 = (a_constexpr_ptr_to_mem*)opnd2_value;
              if (pm1->subtract_adjustment != pm2->subtract_adjustment ||
                  pm1->this_class_adjustment != pm2->this_class_adjustment) {
                *(an_integer_value *)result_storage = one_int;
              } else if (pm1->is_ptr_to_mem_function) {
                if (pm2->is_ptr_to_mem_function &&
                    pm1->variant.routine == pm2->variant.routine) {
                  *(an_integer_value *)result_storage = zero_int;
                } else {
                  *(an_integer_value *)result_storage = one_int;
                }  /* if */
              } else {
                if (!pm2->is_ptr_to_mem_function &&
                    pm1->variant.field == pm2->variant.field) {
                  *(an_integer_value *)result_storage = zero_int;
                } else {
                  *(an_integer_value *)result_storage = one_int;
                }  /* if */
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_nullptr) {
              /* Two nullptr values always compare equal. */
              *(an_integer_value *)result_storage = zero_int;
#if C99_IL_EXTENSIONS_SUPPORTED
            } else if (tp->kind == (a_type_kind)tk_complex) {
              /* A complex floating-point type. */
              *(an_integer_value *)result_storage =
                 cx_equal(tp->variant.float_kind,
                          cx_value(opnd1_value), cx_value(opnd2_value)) ?
                                                           zero_int : one_int;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            } else {
              unexpected_condition();
            }  /* if */
            break;
          case eok_lt:
            if (opnd1_type->kind == (a_type_kind)tk_integer) {
              /* Integral operands. */
              int_kind = opnd1_type->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              if (cmp_integer_values((an_integer_value *)opnd1_value,
                                     is_signed,
                                     (an_integer_value *)opnd2_value,
                                     is_signed) < 0) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_float) {
              /* Floating-point operands. */
              if (fp_compare(opnd1_type->variant.float_kind,
                             fp_value(opnd1_value),
                             fp_value(opnd2_value),
                             &unord) < 0) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
              if (unord) {
                /* The floating-point values are not comparable. */
                info_with_pos(ec_constexpr_fp_values_not_comparable,
                              &expr->position, ips);
                do_constexpr_fail(result);
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
              /* Pointer operands. */
              a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
              a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
              if (is_runtime_data_address(ptr1) ==
                                              is_runtime_data_address(ptr2)) {
                if (!is_runtime_data_address(ptr1)) {
                  if (!addresses_are_comparable(ips, ptr1, ptr2)) {
                    info_with_pos(ec_constexpr_pointers_not_comparable,
                                  &expr->position, ips);
                    do_constexpr_fail(result);
                  } else if (ptr1->address < ptr2->address) {
                    *(an_integer_value *)result_storage = one_int;
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
                  }  /* if */
                } else {
                  do_constexpr_fail(result);
                }  /* if */
              } else {
                do_constexpr_fail(result);
              }  /* if */
              release_variant_path_if_needed(ptr1);
              release_variant_path_if_needed(ptr2);
            } else if (opnd1_type->kind == (a_type_kind)tk_nullptr) {
              /* Two nullptr values always compare equal. */
              *(an_integer_value *)result_storage = zero_int;
            } else {
              unexpected_condition();
            }  /* if */
            break;
          case eok_gt:
            if (opnd1_type->kind == (a_type_kind)tk_integer) {
              /* Integral operands. */
              int_kind = opnd1_type->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              if (cmp_integer_values((an_integer_value *)opnd1_value,
                                     is_signed,
                                     (an_integer_value *)opnd2_value,
                                     is_signed) > 0) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_float) {
              /* Floating-point operands. */
              if (fp_compare(opnd1_type->variant.float_kind,
                             fp_value(opnd1_value),
                             fp_value(opnd2_value),
                             &unord) > 0) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
              if (unord) {
                /* The floating-point values are not comparable. */
                info_with_pos(ec_constexpr_fp_values_not_comparable,
                              &expr->position, ips);
                do_constexpr_fail(result);
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
              /* Pointer operands. */
              a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
              a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
              if (is_runtime_data_address(ptr1) ==
                                              is_runtime_data_address(ptr2)) {
                if (!is_runtime_data_address(ptr1)) {
                  if (!addresses_are_comparable(ips, ptr1, ptr2)) {
                    info_with_pos(ec_constexpr_pointers_not_comparable,
                                  &expr->position, ips);
                    do_constexpr_fail(result);
                  } else if (ptr1->address > ptr2->address) {
                    *(an_integer_value *)result_storage = one_int;
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
                  }  /* if */
                } else {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                }  /* if */
              } else {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              }  /* if */
              release_variant_path_if_needed(ptr1);
              release_variant_path_if_needed(ptr2);
            } else if (opnd1_type->kind == (a_type_kind)tk_nullptr) {
              /* Two nullptr values always compare equal. */
              *(an_integer_value *)result_storage = zero_int;
            } else {
              unexpected_condition();
            }  /* if */
            break;
          case eok_le:
            if (opnd1_type->kind == (a_type_kind)tk_integer) {
              /* Integral operands. */
              int_kind = opnd1_type->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              if (cmp_integer_values((an_integer_value *)opnd1_value,
                                     is_signed,
                                     (an_integer_value *)opnd2_value,
                                     is_signed) <= 0) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_float) {
              /* Floating-point operands. */
              if (fp_compare(opnd1_type->variant.float_kind,
                             fp_value(opnd1_value),
                             fp_value(opnd2_value),
                             &unord) <= 0) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
              if (unord) {
                /* The floating-point values are not comparable. */
                info_with_pos(ec_constexpr_fp_values_not_comparable,
                              &expr->position, ips);
                do_constexpr_fail(result);
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
              /* Pointer operands. */
              a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
              a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
              if (is_runtime_data_address(ptr1) ==
                                              is_runtime_data_address(ptr2)) {
                if (!is_runtime_data_address(ptr1)) {
                  if (!addresses_are_comparable(ips, ptr1, ptr2)) {
                    info_with_pos(ec_constexpr_pointers_not_comparable,
                                  &expr->position, ips);
                    do_constexpr_fail(result);
                  } else if (ptr1->address <= ptr2->address) {
                    *(an_integer_value *)result_storage = one_int;
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
                  }  /* if */
                } else {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                }  /* if */
              } else {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              }  /* if */
              release_variant_path_if_needed(ptr1);
              release_variant_path_if_needed(ptr2);
            } else if (opnd1_type->kind == (a_type_kind)tk_nullptr) {
              /* Two nullptr values always compare equal. */
              *(an_integer_value *)result_storage = one_int;
            } else {
              unexpected_condition();
            }  /* if */
            break;
          case eok_ge:
            if (opnd1_type->kind == (a_type_kind)tk_integer) {
              /* Integral operands. */
              int_kind = opnd1_type->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              if (cmp_integer_values((an_integer_value *)opnd1_value,
                                     is_signed,
                                     (an_integer_value *)opnd2_value,
                                     is_signed) >= 0) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_float) {
              /* Floating-point operands. */
              if (fp_compare(opnd1_type->variant.float_kind,
                             fp_value(opnd1_value),
                             fp_value(opnd2_value),
                             &unord) >= 0) {
                *(an_integer_value *)result_storage = one_int;
              } else {
                *(an_integer_value *)result_storage = zero_int;
              }  /* if */
              if (unord) {
                /* The floating-point values are not comparable. */
                info_with_pos(ec_constexpr_fp_values_not_comparable,
                              &expr->position, ips);
                do_constexpr_fail(result);
              }  /* if */
            } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
              /* Pointer operands. */
              a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
              a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
              if (is_runtime_data_address(ptr1) ==
                                              is_runtime_data_address(ptr2)) {
                if (!is_runtime_data_address(ptr1)) {
                  if (!addresses_are_comparable(ips, ptr1, ptr2)) {
                    info_with_pos(ec_constexpr_pointers_not_comparable,
                                  &expr->position, ips);
                    do_constexpr_fail(result);
                  } else if (ptr1->address >= ptr2->address) {
                    *(an_integer_value *)result_storage = one_int;
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
                  }  /* if */
                } else {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                }  /* if */
              } else {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              }  /* if */
              release_variant_path_if_needed(ptr1);
              release_variant_path_if_needed(ptr2);
            } else if (opnd1_type->kind == (a_type_kind)tk_nullptr) {
              /* Two nullptr values always compare equal. */
              *(an_integer_value *)result_storage = one_int;
            } else {
              unexpected_condition();
            }  /* if */
            break;
          case eok_spaceship:
            {
              a_constant_ptr  result_con = NULL;
              if (strong_ordering_equal == NULL) {
                initialize_ordering_constants();
              }  /* if */
              if (opnd1_type->kind == (a_type_kind)tk_integer) {
                /* Integral operands. */
                int  cmp;
                int_kind = opnd1_type->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                cmp = cmp_integer_values((an_integer_value *)opnd1_value,
                                         is_signed,
                                         (an_integer_value *)opnd2_value,
                                         is_signed);
                if (cmp == 0) {
                  result_con = strong_ordering_equal;
                } else if (cmp == 1) {
                  result_con = strong_ordering_greater;
                } else if (cmp == -1) {
                  result_con = strong_ordering_less;
                } else {
                  unexpected_condition();
                }  /* if */
              } else if (opnd1_type->kind == (a_type_kind)tk_float) {
                /* Floating-point operands. */
                int  cmp;
                cmp = fp_compare(opnd1_type->variant.float_kind,
                                 fp_value(opnd1_value),
                                 fp_value(opnd2_value),
                                 &unord);
                if (unord) {
                  /* The floating-point values are not comparable. */
                  result_con = partial_ordering_unordered;
                } else if (cmp == 0) {
                  result_con = partial_ordering_equivalent;
                } else if (cmp == 1) {
                  result_con = partial_ordering_greater;
                } else if (cmp == -1) {
                  result_con = partial_ordering_less;
                } else {
                  unexpected_condition();
                }  /* if */
              } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                /* Pointer operands. */
                a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
                a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
                if (!compatible_address_kinds(ptr1, ptr2)) {
                  info_with_pos(ec_constexpr_pointers_not_comparable,
                                &expr->position, ips);
                  do_constexpr_fail(result);
                } else if (is_function_address(ptr1) ||
                           is_function_address(ptr2)) {
                  if (is_function_address(ptr1) && is_function_address(ptr2)) {
                    if (ptr1->variant.routine == ptr2->variant.routine) {
                      result_con = strong_equality_equal;
                    } else {
                      result_con = strong_equality_nonequal;
                    }  /* if */
                  } else {
                    info_with_pos(ec_constexpr_pointers_not_comparable,
                                  &expr->position, ips);
                    do_constexpr_fail(result);
                  }  /* if */
                } else if (is_runtime_data_address(ptr1) ==
                                              is_runtime_data_address(ptr2)) {
                  if (!is_runtime_data_address(ptr1)) {
                    if (ptr1->address == ptr2->address) {
                      result_con = strong_ordering_equal;
                    } else if (addresses_are_comparable(ips, ptr1, ptr2)) {
                      if (ptr1->address < ptr2->address) {
                        result_con = strong_ordering_less;
                      } else {
                        result_con = strong_ordering_greater;
                      }  /* if */
                    } else {
                      info_with_pos(ec_constexpr_pointers_not_comparable,
                                    &expr->position, ips);
                      do_constexpr_fail(result);
                    }  /* if */
                  } else {
                    /* The addresses are represented using a_constant
                       entries. */
                    int  cmp;
                    if (compare_address_constants(ptr1->variant.addr_con,
                                                  ptr2->variant.addr_con,
                                                  &cmp)) {
                      if (cmp == 0) {
                        result_con = strong_ordering_equal;
                      } else if (cmp == 1) {
                        result_con = strong_ordering_greater;
                      } else if (cmp == -1) {
                        result_con = strong_ordering_less;
                      } else {
                        unexpected_condition();
                      }  /* if */
                    } else {
                      info_with_pos(ec_constexpr_pointers_not_comparable,
                                    &expr->position, ips);
                      do_constexpr_fail(result);
                    }  /* if */
                  }  /* if */
                } else {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                }  /* if */
                release_variant_path_if_needed(ptr1);
                release_variant_path_if_needed(ptr2);
              } else if (opnd1_type->kind == (a_type_kind)tk_ptr_to_member) {
                /* For pointer-to-members, <=> behaves like !=. */
                a_constexpr_ptr_to_mem  *pm1, *pm2;
                pm1 = (a_constexpr_ptr_to_mem*)opnd1_value;
                pm2 = (a_constexpr_ptr_to_mem*)opnd2_value;
                if (pm1->subtract_adjustment != pm2->subtract_adjustment ||
                    pm1->this_class_adjustment != pm2->this_class_adjustment) {
                  result_con = strong_equality_nonequal;
                } else if (pm1->is_ptr_to_mem_function) {
                  if (pm2->is_ptr_to_mem_function &&
                      pm1->variant.routine == pm2->variant.routine) {
                    result_con = strong_equality_equal;
                  } else {
                    result_con = strong_equality_nonequal;
                  }  /* if */
                } else {
                  if (!pm2->is_ptr_to_mem_function &&
                      pm1->variant.field == pm2->variant.field) {
                    result_con = strong_equality_equal;
                  } else {
                    result_con = strong_equality_nonequal;
                  }  /* if */
                }  /* if */
              } else if (opnd1_type->kind == (a_type_kind)tk_nullptr) {
                /* Two nullptr values always compare equal. */
                result_con = strong_equality_equal;
              } else {
                unexpected_condition();
              }  /* if */
              if (result) {
                result = copy_val_from_constant(ips, result_con,
                                                result_storage,
                                                complete_object);
              }  /* if */
            }
            break;
          case eok_assign:
            { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
              a_boolean            lhs_initialized;
              if (cannot_dereference(dst)) {
                /* Storing one position past the end of an array. */
                do_constexpr_fail(result);
                info_one_past_end_of_array(dst, expr, ips);
              } else if (is_runtime_data_address(dst)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (dst->address == NULL) {
                /* An attempt to write through a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (!in_live_set(&ips->live_set, dst->alloc_seq_number)) {
                /* Attempting to store into expired storage. */
                info_with_pos(ec_constexpr_access_to_expired_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_const_storage(dst)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (!(lhs_initialized = is_initialized(dst)) &&
                         !cpp20_mode) {
                /* Prior to C++20 (i.e., the changes of P1331R2), assigning to
                   uninitialized storage was invalid. */
                do_constexpr_fail(result);
                info_with_pos(ec_object_not_initialized, &opnd1->position,
                              ips);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else {
                /* Copy the value of the right operand to the indicated
                   address and return either the address or the value, as
                   appropriate. */
                a_byte     *dst_storage = value_bytes_at(dst);
                a_boolean  copy_subobjects = is_immediate_class_type(tp) ||
                                             type_is(tp, tk_array);
                if (is_variant_path(dst)) {
                  /* Assignment may require setting a new active field. */
                  if (!check_variant_assign(ips, dst, &expr->position)) {
                    /* Invalid attempt to store into a non-active variant
                       field. */
                    do_constexpr_fail(result);
                  }  /* if */
                  /* The variant path is no longer needed after this. */
                  release_variant_path(dst);
                }  /* if */
                n_bytes = value_bytes_for_type(ips, tp, &result);
                if (copy_subobjects) {
                  if (!constexpr_copy_object(
                                         ips, tp, &expr->position,
                                         opnd2_value, opnd2_value,
                                         dst_storage, dst->complete_object)) {
                    result = FALSE;
                    break;
                  }  /* if */
                } else {
                  (void)memcpy(dst_storage, opnd2_value, size_t_arg(n_bytes));
                  if (type_is(tp, tk_pointer)) {
                    /* Copying a pointer type.  Make sure its side structures,
                       if any, are not shared. */
                    copy_address_structures(dst_storage);
                  } else {
                    trim_bit_field_if_needed(dst, tp);
                  }  /* if */
                }  /* if */
                if (expr->is_lvalue || expr->is_xvalue ||
                    is_function_address(dst)) {
                  /* The assignment produces an lvalue-like result. */
                  *(a_constexpr_address*)result_storage = *dst;
                } else {
                  /* The assignment produces an rvalue.  Copy the value once
                     more. */
                  if (copy_subobjects) {
                    if (!constexpr_copy_object(
                                         ips, tp, &expr->position,
                                         dst_storage, dst->complete_object,
                                         result_storage, complete_object)) {
                      result = FALSE;
                      break;
                    }  /* if */
                  } else {
                    (void)memcpy(result_storage, dst_storage,
                                 size_t_arg(n_bytes));
                  }  /* if */
                  mark_whole_subobject_initialized(ips, result_storage, tp,
                                                   complete_object);
                }  /* if */
                if (!lhs_initialized) {
                  mark_whole_subobject_initialized(ips, dst_storage, tp,
                                                   dst->complete_object);
                  if (dst_storage == dst->complete_object) {
                    mark_complete_object_initialized(dst_storage);
                  }  /* if */
                }  /* if */
              }  /* if */
            }
            break;
          case eok_add_assign:
            { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
              if (cannot_dereference(dst)) {
                /* Storing one position past the end of an array. */
                do_constexpr_fail(result);
                info_one_past_end_of_array(dst, expr, ips);
              } else if (is_runtime_data_address(dst)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (dst->address == NULL) {
                /* An attempt to write through a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (!in_live_set(&ips->live_set, dst->alloc_seq_number)) {
                /* Attempting to store into expired storage. */
                info_with_pos(ec_constexpr_access_to_expired_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_const_storage(dst)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_variant_path(dst) &&
                         (!is_initialized(dst) ||
                          !check_variant_path(ips, dst, &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else {
                /* Add the value of the right operand to the value stored at
                   the left operand and return the left operand (as an
                   lvalue). */
                if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  add_integer_values(int_value_at(dst),
                                     (an_integer_value*)opnd2_value,
                                     is_signed, &ovfl);
                  trim_bit_field_if_needed(dst, tp);
                  CHECK_int_range(int_value_at(dst), tp);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                } else if (type_kind_is_float_like(
                                         expr->variant.operation.type_kind) &&
                           expr->variant.operation.type_kind == tp->kind) {
                  /* Floating-point += floating-point.  The operation type
                     is that of the second operand. */
                  an_internal_float_value  *dst_val = fp_value_at(dst);
                  an_internal_float_value  tmp;
                  a_boolean                need_tmp;
                  need_tmp = opnd1_type->variant.float_kind !=
                                               opnd2_type->variant.float_kind;
                  err = FALSE;
                  if (need_tmp) {
                    fp_change_kind(dst_val, opnd1_type->variant.float_kind,
                                   &tmp, opnd2_type->variant.float_kind, &err,
                                   &depends_on_fp_mode);
                    dst_val = &tmp;
                  }  /* if */
                  if (!err) {
                    fp_add(opnd2_type->variant.float_kind, dst_val,
                           fp_value(opnd2_value), dst_val, &err,
                           &depends_on_fp_mode);
                  }  /* if */
                  if (need_tmp && !err) {
                    dst_val = fp_value_at(dst);
                    fp_change_kind(&tmp, opnd2_type->variant.float_kind,
                                   dst_val, opnd1_type->variant.float_kind,
                                   &err, &depends_on_fp_mode);
                  }  /* if */
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  } else if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *fp_value(result_storage) = *fp_value_at(dst);
                  }  /* if */
                } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float &&
                           tp->kind == (a_type_kind)tk_integer) {
                  /* Integer += floating-point. */
                  an_internal_float_value  dst_val;
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  conv_integer_value_to_float(int_value_at(dst),
                                              is_signed,
                                              &dst_val,
                                              opnd2_type->variant.float_kind,
                                              &err);
                  fp_add(opnd2_type->variant.float_kind, &dst_val,
                         fp_value(opnd2_value), &dst_val, &err,
                         &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                    break;
                  } else if (conv_float_value_to_int_value(
                                            &dst_val,
                                            opnd2_type->variant.float_kind,
                                            int_value_at(dst),
                                            is_signed, &depends_on_fp_mode)) {
                    ovfl = FALSE;
                    CHECK_int_range(int_value_at(dst), tp);
                  } else {
                    do_constexpr_fail(result);
                    info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                        &expr->position, opnd1_type, tp, ips);
                    break;
                  }  /* if */
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
                } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_complex &&
                           tp->kind == (a_type_kind)tk_complex) {
                  /* Complex += complex. */
                  an_internal_complex_value  *dst_val = cx_value_at(dst);
                  cx_add(tp->variant.float_kind, dst_val,
                         cx_value(opnd2_value), dst_val, &err,
                         &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  } else if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *cx_value(result_storage) = *cx_value_at(dst);
                  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
                } else {
                  /* Other types. */
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                                &expr->position, ips);
                }  /* if */
              }  /* if */
            }
            break;
          case eok_subtract_assign:
            { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
              if (cannot_dereference(dst)) {
                /* Storing one position past the end of an array. */
                do_constexpr_fail(result);
                info_one_past_end_of_array(dst, expr, ips);
              } else if (is_runtime_data_address(dst)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (dst->address == NULL) {
                /* An attempt to write through a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (!in_live_set(&ips->live_set, dst->alloc_seq_number)) {
                /* Attempting to store into expired storage. */
                info_with_pos(ec_constexpr_access_to_expired_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_const_storage(dst)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_variant_path(dst) &&
                         (!is_initialized(dst) ||
                          !check_variant_path(ips, dst, &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else {
                /* Subtract the value of the right operand from the value
                   stored at the left operand and return the left operand
                   (as an lvalue). */
                if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  subtract_integer_values(int_value_at(dst),
                                          (an_integer_value*)opnd2_value,
                                          is_signed, &ovfl);
                  trim_bit_field_if_needed(dst, tp);
                  CHECK_int_range(int_value_at(dst), tp);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                } else if (type_kind_is_float_like(
                                         expr->variant.operation.type_kind) &&
                           expr->variant.operation.type_kind == tp->kind) {
                  /* Floating-point -= floating-point.  The operation type
                     is that of the second operand. */
                  an_internal_float_value  *dst_val = fp_value_at(dst);
                  an_internal_float_value  tmp;
                  a_boolean                need_tmp;
                  need_tmp = opnd1_type->variant.float_kind !=
                                               opnd2_type->variant.float_kind;
                  err = FALSE;
                  if (need_tmp) {
                    fp_change_kind(dst_val, opnd1_type->variant.float_kind,
                                   &tmp, opnd2_type->variant.float_kind, &err,
                                   &depends_on_fp_mode);
                    dst_val = &tmp;
                  }  /* if */
                  if (!err) {
                    fp_subtract(opnd2_type->variant.float_kind, dst_val,
                                fp_value(opnd2_value), dst_val, &err,
                                &depends_on_fp_mode);
                  }  /* if */
                  if (need_tmp && !err) {
                    dst_val = fp_value_at(dst);
                    fp_change_kind(&tmp, opnd2_type->variant.float_kind,
                                   dst_val, opnd1_type->variant.float_kind,
                                   &err, &depends_on_fp_mode);
                  }  /* if */
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  } else if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *fp_value(result_storage) = *fp_value_at(dst);
                  }  /* if */
                } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float &&
                           tp->kind == (a_type_kind)tk_integer) {
                  /* Integer -= floating-point. */
                  an_internal_float_value  dst_val;
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  conv_integer_value_to_float(int_value_at(dst),
                                              is_signed,
                                              &dst_val,
                                              opnd2_type->variant.float_kind,
                                              &err);
                  fp_subtract(opnd2_type->variant.float_kind, &dst_val,
                              fp_value(opnd2_value), &dst_val, &err,
                              &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                    break;
                  } else if (conv_float_value_to_int_value(
                                            &dst_val,
                                            opnd2_type->variant.float_kind,
                                            int_value_at(dst),
                                            is_signed, &depends_on_fp_mode)) {
                    ovfl = FALSE;
                    CHECK_int_range(int_value_at(dst), tp);
                  } else {
                    do_constexpr_fail(result);
                    info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                        &expr->position, opnd1_type, tp, ips);
                    break;
                  }  /* if */
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
                } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_complex &&
                           tp->kind == (a_type_kind)tk_complex) {
                  /* Complex -= complex. */
                  an_internal_complex_value  *dst_val = cx_value_at(dst);
                  cx_subtract(tp->variant.float_kind, dst_val,
                              cx_value(opnd2_value), dst_val, &err,
                              &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  } else if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *cx_value(result_storage) = *cx_value_at(dst);
                  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
                } else {
                  /* Other types. */
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                                &expr->position, ips);
                }  /* if */
              }  /* if */
            }
            break;
          case eok_multiply_assign:
            { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
              if (cannot_dereference(dst)) {
                /* Storing one position past the end of an array. */
                do_constexpr_fail(result);
                info_one_past_end_of_array(dst, expr, ips);
              } else if (is_runtime_data_address(dst)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (dst->address == NULL) {
                /* An attempt to write through a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (!in_live_set(&ips->live_set, dst->alloc_seq_number)) {
                /* Attempting to store into expired storage. */
                info_with_pos(ec_constexpr_access_to_expired_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_const_storage(dst)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_variant_path(dst) &&
                         (!is_initialized(dst) ||
                          !check_variant_path(ips, dst, &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else {
                /* Multiply the value stored in the left operand with the
                   value of the right operand and leave the result in the
                   left operand.  Return the left operand (as an lvalue). */
                if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  multiply_integer_values(int_value_at(dst),
                                          (an_integer_value*)opnd2_value,
                                          is_signed, &ovfl);
                  trim_bit_field_if_needed(dst, tp);
                  CHECK_int_range(int_value_at(dst), tp);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float &&
                           tp->kind == (a_type_kind)tk_float) {
                  /* Floating-point *= floating-point.  The operation type
                     is that of the second operand. */
                  an_internal_float_value  *dst_val = fp_value_at(dst);
                  an_internal_float_value  tmp;
                  a_boolean                need_tmp;
                  need_tmp = opnd1_type->variant.float_kind !=
                                               opnd2_type->variant.float_kind;
                  err = FALSE;
                  if (need_tmp) {
                    fp_change_kind(dst_val, opnd1_type->variant.float_kind,
                                   &tmp, opnd2_type->variant.float_kind, &err,
                                   &depends_on_fp_mode);
                    dst_val = &tmp;
                  }  /* if */
                  if (!err) {
                    fp_multiply(opnd2_type->variant.float_kind, dst_val,
                                fp_value(opnd2_value), dst_val, &err,
                                &depends_on_fp_mode);
                  }  /* if */
                  if (need_tmp && !err) {
                    dst_val = fp_value_at(dst);
                    fp_change_kind(&tmp, opnd2_type->variant.float_kind,
                                   dst_val, opnd1_type->variant.float_kind,
                                   &err, &depends_on_fp_mode);
                  }  /* if */
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  } else if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *fp_value(result_storage) = *fp_value_at(dst);
                  }  /* if */
                } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float &&
                           tp->kind == (a_type_kind)tk_integer) {
                  /* Integer *= floating-point. */
                  an_internal_float_value  dst_val;
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  conv_integer_value_to_float(int_value_at(dst),
                                              is_signed,
                                              &dst_val,
                                              opnd2_type->variant.float_kind,
                                              &err);
                  fp_multiply(opnd2_type->variant.float_kind, &dst_val,
                              fp_value(opnd2_value), &dst_val, &err,
                              &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                    break;
                  } else if (conv_float_value_to_int_value(
                                            &dst_val,
                                            opnd2_type->variant.float_kind,
                                            int_value_at(dst),
                                            is_signed, &depends_on_fp_mode)) {
                    ovfl = FALSE;
                    CHECK_int_range(int_value_at(dst), tp);
                  } else {
                    do_constexpr_fail(result);
                    info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                        &expr->position, opnd1_type, tp, ips);
                    break;
                  }  /* if */
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
                } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_complex &&
                           tp->kind == (a_type_kind)tk_complex) {
                  /* Complex *= complex. */
                  an_internal_complex_value  *dst_val = cx_value_at(dst);
                  cx_multiply(tp->variant.float_kind, dst_val,
                              cx_value(opnd2_value), dst_val, &err,
                              &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  } else if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *cx_value(result_storage) = *cx_value_at(dst);
                  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
                } else {
                  /* Other types. */
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                                &expr->position, ips);
                }  /* if */
              }  /* if */
            }
            break;
          case eok_divide_assign:
            { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
              if (cannot_dereference(dst)) {
                /* Storing one position past the end of an array. */
                do_constexpr_fail(result);
                info_one_past_end_of_array(dst, expr, ips);
              } else if (is_runtime_data_address(dst)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (dst->address == NULL) {
                /* An attempt to write through a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (!in_live_set(&ips->live_set, dst->alloc_seq_number)) {
                /* Attempting to store into expired storage. */
                info_with_pos(ec_constexpr_access_to_expired_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_variant_path(dst) &&
                         (!is_initialized(dst) ||
                          !check_variant_path(ips, dst, &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (is_const_storage(dst)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else {
                /* Divide the value stored in the left operand with the
                   value of the right operand and leave the result in the
                   left operand.  Return the left operand (as an lvalue). */
                if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  divide_integer_values(int_value_at(dst),
                                        (an_integer_value*)opnd2_value,
                                        is_signed, &ovfl);
                  trim_bit_field_if_needed(dst, tp);
                  CHECK_int_range(int_value_at(dst), tp);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float &&
                           tp->kind == (a_type_kind)tk_float) {
                  /* Floating-point /= floating-point.  The operation type
                     is that of the second operand. */
                  an_internal_float_value  *dst_val = fp_value_at(dst);
                  an_internal_float_value  tmp;
                  a_boolean                need_tmp;
                  need_tmp = opnd1_type->variant.float_kind !=
                                               opnd2_type->variant.float_kind;
                  err = FALSE;
                  if (need_tmp) {
                    fp_change_kind(dst_val, opnd1_type->variant.float_kind,
                                   &tmp, opnd2_type->variant.float_kind, &err,
                                   &depends_on_fp_mode);
                    dst_val = &tmp;
                  }  /* if */
                  if (!err) {
                    fp_divide(opnd2_type->variant.float_kind, dst_val,
                              fp_value(opnd2_value), dst_val, &err,
                              &depends_on_fp_mode);
                  }  /* if */
                  if (need_tmp && !err) {
                    dst_val = fp_value_at(dst);
                    fp_change_kind(&tmp, opnd2_type->variant.float_kind,
                                   dst_val, opnd1_type->variant.float_kind,
                                   &err, &depends_on_fp_mode);
                  }  /* if */
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  } else if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *fp_value(result_storage) = *fp_value_at(dst);
                  }  /* if */
                } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float &&
                           tp->kind == (a_type_kind)tk_integer) {
                  /* Integer /= floating-point. */
                  an_internal_float_value  dst_val;
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  conv_integer_value_to_float(int_value_at(dst),
                                              is_signed,
                                              &dst_val,
                                              opnd2_type->variant.float_kind,
                                              &err);
                  fp_divide(opnd2_type->variant.float_kind, &dst_val,
                            fp_value(opnd2_value), &dst_val, &err,
                            &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                    break;
                  } else if (conv_float_value_to_int_value(
                                            &dst_val,
                                            opnd2_type->variant.float_kind,
                                            int_value_at(dst),
                                            is_signed, &depends_on_fp_mode)) {
                    ovfl = FALSE;
                    CHECK_int_range(int_value_at(dst), tp);
                  } else {
                    do_constexpr_fail(result);
                    info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                        &expr->position, opnd1_type, tp, ips);
                    break;
                  }  /* if */
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
                } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_complex &&
                           tp->kind == (a_type_kind)tk_complex) {
                  /* Complex /= complex. */
                  an_internal_complex_value  *dst_val = cx_value_at(dst);
                  cx_divide(tp->variant.float_kind, dst_val,
                            cx_value(opnd2_value), dst_val, &err,
                            &depends_on_fp_mode);
                  if (err) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  } else if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *cx_value(result_storage) = *cx_value_at(dst);
                  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
                } else {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                                &expr->position, ips);
                }  /* if */
              }  /* if */
            }
            break;
          case eok_remainder_assign:
            { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
              if (cannot_dereference(dst)) {
                /* Storing one position past the end of an array. */
                do_constexpr_fail(result);
                info_one_past_end_of_array(dst, expr, ips);
              } else if (is_runtime_data_address(dst)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (dst->address == NULL) {
                /* An attempt to write through a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (!in_live_set(&ips->live_set, dst->alloc_seq_number)) {
                /* Attempting to store into expired storage. */
                info_with_pos(ec_constexpr_access_to_expired_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_const_storage(dst)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_variant_path(dst) &&
                         (!is_initialized(dst) ||
                          !check_variant_path(ips, dst, &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                /* Compute the remainder of the value stored in the left
                   operand when divided by the value of the right operand
                   and store the result in the left operand.  Return the
                   left operand (as an lvalue). */
                int_kind = tp->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                remainder_integer_values(int_value_at(dst),
                                         (an_integer_value*)opnd2_value,
                                         is_signed, &ovfl);
                trim_bit_field_if_needed(dst, tp);
                CHECK_int_range(int_value_at(dst), tp);
                if (expr->is_lvalue || expr->is_xvalue) {
                  /* The assignment produces an lvalue-like result. */
                  *(a_constexpr_address*)result_storage = *dst;
                } else {
                  /* The assignment produces an rvalue.  Copy the value. */
                  *(an_integer_value*)result_storage = *int_value_at(dst);
                }  /* if */
              } else {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                              &expr->position, ips);
              }  /* if */
            }
            break;
          case eok_shiftl_assign:
            { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
              if (cannot_dereference(dst)) {
                /* Storing one position past the end of an array. */
                do_constexpr_fail(result);
                info_one_past_end_of_array(dst, expr, ips);
              } else if (is_runtime_data_address(dst)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (dst->address == NULL) {
                /* An attempt to write through a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (!in_live_set(&ips->live_set, dst->alloc_seq_number)) {
                /* Attempting to store into expired storage. */
                info_with_pos(ec_constexpr_access_to_expired_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_variant_path(dst) &&
                         (!is_initialized(dst) ||
                          !check_variant_path(ips, dst, &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (is_const_storage(dst)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                /* Shift the bits stored in the first operand left by the
                   number of bits indicated by the second operand.  Return
                   the left operand (as an lvalue). */
                int_kind = tp->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                get_int_val_from(opnd2_value, opnd2_type, host_int_val,
                                 ovfl);
                if (ovfl) {
                  do_constexpr_fail(result);
                } else if (host_int_val < 0 ||
                           host_int_val >=
                          (a_host_large_integer)(tp->size * targ_char_bit)) {
                  do_constexpr_fail(result);
                  if (host_int_val < 0) {
                    info_with_pos(ec_constexpr_negative_shift,
                                  &expr->position, ips);
                  } else {
                    info_with_pos_num(ec_constexpr_shift_excess,
                                      &expr->position,
                                      (uint32_t)host_int_val, ips);
                  }  /* if */
                }  /* if */
                if (result) {
                  shift_left_integer_value(int_value_at(dst),
                                           (int)host_int_val, &ovfl);
                  trim_bit_field_if_needed(dst, tp);
                  CHECK_int_range(int_value_at(dst), tp);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                }  /* if */
              } else {
                /* Other types. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                              &expr->position, ips);
              }  /* if */
            }
            break;
          case eok_shiftr_assign:
            { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
              if (cannot_dereference(dst)) {
                /* Storing one position past the end of an array. */
                do_constexpr_fail(result);
                info_one_past_end_of_array(dst, expr, ips);
              } else if (is_runtime_data_address(dst)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (dst->address == NULL) {
                /* An attempt to write through a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (!in_live_set(&ips->live_set, dst->alloc_seq_number)) {
                /* Attempting to store into expired storage. */
                info_with_pos(ec_constexpr_access_to_expired_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_const_storage(dst)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_variant_path(dst) &&
                         (!is_initialized(dst) ||
                          !check_variant_path(ips, dst, &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                /* Shift the bits stored in the first operand left by the
                   number of bits indicated by the second operand.  Return
                   the left operand (as an lvalue). */
                int_kind = tp->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                get_int_val_from(opnd2_value, opnd2_type, host_int_val,
                                 ovfl);
                if (ovfl) {
                  do_constexpr_fail(result);
                } else if (host_int_val < 0 ||
                           host_int_val >=
                          (a_host_large_integer)(tp->size * targ_char_bit)) {
                  do_constexpr_fail(result);
                  if (host_int_val < 0) {
                    info_with_pos(ec_constexpr_negative_shift,
                                  &expr->position, ips);
                  } else {
                    info_with_pos_num(ec_constexpr_shift_excess,
                                      &expr->position,
                                      (uint32_t)host_int_val, ips);
                  }  /* if */
                }  /* if */
                if (result) {
                  shift_right_integer_value(int_value_at(dst),
                                            (int)host_int_val, is_signed,
                                            targ_right_shift_is_arithmetic);
                  CHECK_int_range(int_value_at(dst), tp);
                  trim_bit_field_if_needed(dst, tp);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                }  /* if */
              } else {
                /* Other types. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                              &expr->position, ips);
              }  /* if */
            }
            break;
          case eok_and_assign:
            { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
              if (cannot_dereference(dst)) {
                /* Storing one position past the end of an array. */
                do_constexpr_fail(result);
                info_one_past_end_of_array(dst, expr, ips);
              } else if (is_runtime_data_address(dst)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (dst->address == NULL) {
                /* An attempt to write through a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (!in_live_set(&ips->live_set, dst->alloc_seq_number)) {
                /* Attempting to store into expired storage. */
                info_with_pos(ec_constexpr_access_to_expired_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_const_storage(dst)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_variant_path(dst) &&
                         (!is_initialized(dst) ||
                          !check_variant_path(ips, dst, &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else {
                /* Bitwise "and" the value stored in the left operand with
                   the value of the right operand and leave the result in the
                   left operand.  Return the left operand (as an lvalue). */
                and_integer_values(int_value_at(dst),
                                   (an_integer_value*)opnd2_value);
                trim_bit_field_if_needed(dst, tp);
                if (expr->is_lvalue || expr->is_xvalue) {
                  /* The assignment produces an lvalue-like result. */
                  *(a_constexpr_address*)result_storage = *dst;
                } else {
                  /* The assignment produces an rvalue.  Copy the value. */
                  *(an_integer_value*)result_storage = *int_value_at(dst);
                }  /* if */
              }  /* if */
            }
            break;
          case eok_or_assign:
            { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
              if (cannot_dereference(dst)) {
                /* Storing one position past the end of an array. */
                do_constexpr_fail(result);
                info_one_past_end_of_array(dst, expr, ips);
              } else if (is_runtime_data_address(dst)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (dst->address == NULL) {
                /* An attempt to write through a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (!in_live_set(&ips->live_set, dst->alloc_seq_number)) {
                /* Attempting to store into expired storage. */
                info_with_pos(ec_constexpr_access_to_expired_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_const_storage(dst)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_variant_path(dst) &&
                         (!is_initialized(dst) ||
                          !check_variant_path(ips, dst, &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else {
                /* Bitwise "or" the value stored in the left operand with
                   the value of the right operand and leave the result in the
                   left operand.  Return the left operand (as an lvalue). */
                or_integer_values(int_value_at(dst),
                                  (an_integer_value*)opnd2_value);
                trim_bit_field_if_needed(dst, tp);
                if (expr->is_lvalue || expr->is_xvalue) {
                  /* The assignment produces an lvalue-like result. */
                  *(a_constexpr_address*)result_storage = *dst;
                } else {
                  /* The assignment produces an rvalue.  Copy the value. */
                  *(an_integer_value*)result_storage = *int_value_at(dst);
                }  /* if */
              }  /* if */
            }
            break;
          case eok_xor_assign:
            { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
              if (cannot_dereference(dst)) {
                /* Storing one position past the end of an array. */
                do_constexpr_fail(result);
                info_one_past_end_of_array(dst, expr, ips);
              } else if (is_runtime_data_address(dst)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (dst->address == NULL) {
                /* An attempt to write through a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else if (!in_live_set(&ips->live_set, dst->alloc_seq_number)) {
                /* Attempting to store into expired storage. */
                info_with_pos(ec_constexpr_access_to_expired_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_const_storage(dst)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (is_variant_path(dst) &&
                         (!is_initialized(dst) ||
                          !check_variant_path(ips, dst, &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else {
                /* Bitwise "xor" the value stored in the left operand with
                   the value of the right operand and leave the result in the
                   left operand.  Return the left operand (as an lvalue). */
                xor_integer_values(int_value_at(dst),
                                   (an_integer_value*)opnd2_value);
                trim_bit_field_if_needed(dst, tp);
                if (expr->is_lvalue || expr->is_xvalue) {
                  /* The assignment produces an lvalue-like result. */
                  *(a_constexpr_address*)result_storage = *dst;
                } else {
                  /* The assignment produces an rvalue.  Copy the value. */
                  *(an_integer_value*)result_storage = *int_value_at(dst);
                }  /* if */
              }  /* if */
            }
            break;
          case eok_padd_assign:
            {
              if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
                break;
              } else if (is_runtime_data_address(opnd1_value)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else if (is_const_storage(opnd1_value)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (opnd1_type->kind == (a_type_kind)tk_integer) {
                /* Only possible with "bool_value += ptr_value". */
                a_host_large_integer  bool_val;
                check_assertion(is_bool_type(opnd1_type));
                is_signed = FALSE;
                get_int_val_from(int_value_at(opnd1_value), opnd1_type,
                                 bool_val, ovfl);
                if (bool_val == 0) {
                  a_constexpr_address  *cap =
                                            (a_constexpr_address*)opnd2_value;
                  if (is_runtime_data_address(cap)) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_access_to_runtime_storage,
                                  &expr->position, ips);
                    break;
                  } else if (is_function_address(cap) ||
                             cap->address != NULL) {
                    bool_val = 1;
                  }  /* if */
                }  /* if */
                *int_value_at(opnd1_value) = bool_val ? one_int : zero_int;
              } else {
                /* ptr_lvalue += integer_rvalue. */
                a_constexpr_address
                                  *dst = (a_constexpr_address*)result_storage;
                a_type_ptr        elem_type;
                elem_type = skip_typerefs(opnd1_type->variant.pointer.type);
                *dst = *(a_constexpr_address*)opnd1_value;
                get_int_val_from(opnd2_value, opnd2_type, host_int_val, ovfl);
                if (ovfl) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_integer_overflow, &expr->position, ips);
                } else {
                  a_constexpr_address
                                *ptr_val = (a_constexpr_address*)dst->address;
                  if (host_int_val == 0) {
                    /* Leave the address unchanged. */
                  } else if (!is_array_element(ptr_val)) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                  &expr->position, ips);
                  } else {
                    a_byte_count  elem_size, pos, len;
                    get_array_pos(ips, ptr_val, elem_type, &len, &pos,
                                  &elem_size, &result);
                    if (!result) break;
                    if (host_int_val > 0 ?
                                        (len-pos < (a_byte_count)host_int_val)
                                      : (pos < (a_byte_count)-host_int_val)) {
                      /* Out of bounds. */
                      do_constexpr_fail(result);
                      if (host_int_val > 0) {
                        info_with_pos_num2(
                                      ec_constexpr_out_of_bounds_array_access,
                                      &expr->position,
                                      (uint32_t)(pos+host_int_val),
                                      (uint32_t)len, ips);
                      } else {
                        info_with_pos(ec_constexpr_pointer_ahead_of_array,
                                      &expr->position, ips);
                      }  /* if */
                    } else {
                      if (is_runtime_data_address(ptr_val)) {
                        if (!offset_runtime_address(
                               ips, &expr->position, ptr_val,
                               host_int_val, elem_size, /*subtract=*/FALSE)) {
                          do_constexpr_fail(result);
                        }  /* if */
                      } else {
                        ptr_val->address += host_int_val * elem_size;
                      }  /* if */
                      if (pos+host_int_val == len) {
                        ptr_val->flags |= CA_CANNOT_DEREFERENCE;
                      } else {
                        ptr_val->flags &= ~CA_CANNOT_DEREFERENCE;
                      }  /* if */
                    }  /* if */
                  }  /* if */
                }  /* if */
              }  /* if */
              *(a_constexpr_address *)result_storage =
                                           *(a_constexpr_address*)opnd1_value;
            }
            break;
          case eok_psubtract_assign:
            /* ptr_lvalue -= integer_rvalue. */
            if (ips->side_effects_disabled) {
              /* Side-effects (like assignments) are disabled. */
              do_constexpr_fail(result);
              break;
            } else if (is_const_storage(opnd1_value)) {
              info_with_pos(ec_constexpr_modifying_const_storage,
                            &expr->position, ips);
              do_constexpr_fail(result);
            } else {
              a_constexpr_address  *dst = (a_constexpr_address*)result_storage;
              a_type_ptr           elem_type;
              elem_type = skip_typerefs(opnd1_type->variant.pointer.type);
              *dst = *(a_constexpr_address*)opnd1_value;
              if (is_runtime_data_address(dst)) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
                break;
              }  /* if */
              get_int_val_from(opnd2_value, opnd2_type, host_int_val, ovfl);
              if (ovfl) {
                do_constexpr_fail(result);
                info_with_pos(ec_integer_overflow, &expr->position, ips);
              } else {
                a_constexpr_address
                                *ptr_val = (a_constexpr_address*)dst->address;
                if (host_int_val == 0) {
                  /* Leave the address unchanged. */
                } else if (!is_array_element(ptr_val)) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                &expr->position, ips);
                } else {
                  a_byte_count  elem_size, pos, len;
                  elem_size = value_bytes_for_type(ips, elem_type, &result);
                  get_array_pos(ips, ptr_val, elem_type, &len, &pos,
                                &elem_size, &result);
                  if (!result) break;
                  if (host_int_val > 0 ?
                                    (pos < (a_byte_count)host_int_val)
                                  : (len-pos < (a_byte_count)-host_int_val)) {
                    /* Out of bounds. */
                    do_constexpr_fail(result);
                    if (host_int_val < 0) {
                      info_with_pos_num2(
                                      ec_constexpr_out_of_bounds_array_access,
                                      &expr->position,
                                      (uint32_t)(pos-host_int_val),
                                      (uint32_t)len, ips);
                    } else {
                      info_with_pos(ec_constexpr_pointer_ahead_of_array,
                                    &expr->position, ips);
                    }  /* if */
                  } else {
                    if (is_runtime_data_address(ptr_val)) {
                      if (!offset_runtime_address(
                               ips, &expr->position, ptr_val,
                               host_int_val, elem_size, /*subtract=*/TRUE)) {
                        do_constexpr_fail(result);
                      }  /* if */
                    } else {
                      ptr_val->address -= host_int_val * elem_size;
                    }  /* if */
                    if (pos - host_int_val == len) {
                      ptr_val->flags |= CA_CANNOT_DEREFERENCE;
                    } else {
                      ptr_val->flags &= ~CA_CANNOT_DEREFERENCE;
                    }  /* if */
                  }  /* if */
                }  /* if */
              }  /* if */
            }  /* if */
            *(a_constexpr_address *)result_storage =
                                           *(a_constexpr_address*)opnd1_value;
            break;
          case eok_land:
            { a_boolean  logical_and_result;
              if (check_boolean_condition(ips, opnd1_value, opnd1, opnd1_type,
                                          &logical_and_result)) {
                if (!logical_and_result) {
                  /* Short-circuit the second operand evaluation. */
                } else {
                  /* Evaluate the second operand. */
                  opnd2_type = skip_typerefs(opnd2->type);
                  opnd_n_bytes = expr_result_size(ips, opnd2, opnd2_type,
                                                  &result);
                  alloc_complete_object(ips, opnd_n_bytes, opnd2_type,
                                        opnd2_value);
                  if (result && !do_constexpr_expression(
                                      ips, opnd2, opnd2_value, opnd2_value)) {
                    do_constexpr_fail(result);
                    break;
                  }  /* if */
                  result = check_boolean_condition(
                                          ips, opnd2_value, opnd2, opnd2_type,
                                          &logical_and_result);
                }  /* if */
                if (result) {
                  if (logical_and_result) {
                    *(an_integer_value *)result_storage = one_int;
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
                  }  /* if */
                }  /* if */
              } else {
                do_constexpr_fail(result);
              }  /* if */
            }
            break;
          case eok_lor:
            { a_boolean  logical_or_result;
              if (check_boolean_condition(ips, opnd1_value, opnd1, opnd1_type,
                                          &logical_or_result)) {
                if (logical_or_result) {
                  /* Short-circuit the second operand evaluation. */
                } else {
                  /* Evaluate the second operand. */
                  opnd2_type = skip_typerefs(opnd2->type);
                  opnd_n_bytes = expr_result_size(ips, opnd2, opnd2_type,
                                                  &result);
                  alloc_complete_object(ips, opnd_n_bytes, opnd2_type,
                                        opnd2_value);
                  if (result && !do_constexpr_expression(
                                      ips, opnd2, opnd2_value, opnd2_value)) {
                    do_constexpr_fail(result);
                    break;
                  }  /* if */
                  result = check_boolean_condition(
                                          ips, opnd2_value, opnd2, opnd2_type,
                                          &logical_or_result);
                }  /* if */
                if (result) {
                  if (logical_or_result) {
                    *(an_integer_value *)result_storage = one_int;
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
                  }  /* if */
                }  /* if */
              } else {
                do_constexpr_fail(result);
              }  /* if */
            }
            break;
          case eok_comma:
          case eok_dot_static:
          case eok_points_to_static:
            { a_boolean  restore_lvalue = FALSE, restore_xvalue = FALSE;
              if (gpp_mode && !is_template_dependent_context() &&
                  !node_has_side_effects(opnd1, (a_boolean*)NULL)) {
                /* In some cases, GCC does not appear to evaluate the first
                   operand for these operators.  It is not clear exactly when
                   this happens, but "!node_has_side_effects" is a close
                   approximation. */
              } else {
                /* Explicitly evaluate the first operand (since that was not
                   done earlier for these operators. */
                if (!do_constexpr_expression(ips, opnd1,
                                             opnd1_value, opnd1_value)) {
                  result = FALSE;
                  break;
                }  /* if */
              }  /* if */
              if (!expr->is_lvalue && !expr->is_xvalue) {
                /* The caller might have "rvalued" expr, but that didn't
                   propagate to the operands.  Temporarily enable that
                   propagation. */
                if (opnd2->is_lvalue) {
                  opnd2->is_lvalue = FALSE;
                  restore_lvalue = TRUE;
                }  /* if */
                if (opnd2->is_xvalue) {
                  opnd2->is_xvalue = FALSE;
                  restore_xvalue = TRUE;
                }  /* if */
              }  /* if */
              result = do_constexpr_expression(
                                 ips, opnd2, result_storage, complete_object);
              if (restore_xvalue) opnd2->is_xvalue = TRUE;
              if (restore_lvalue) opnd2->is_lvalue = TRUE;
            }
            break;
          case eok_subscript:
            /* Pointer + integer or integer + pointer. */
            { a_constexpr_address  result_addr;
              a_type_ptr           elem_type;
              /* Place the pointer in result_addr and the integer in
                 host_int_val. */
              if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                get_int_val_from(opnd2_value, opnd2_type, host_int_val, ovfl);
                result_addr = *(a_constexpr_address *)opnd1_value;
                elem_type = skip_typerefs(opnd1_type->variant.pointer.type);
              } else {
                get_int_val_from(opnd1_value, opnd1_type, host_int_val, ovfl);
                result_addr = *(a_constexpr_address *)opnd2_value;
                elem_type = skip_typerefs(opnd2_type->variant.pointer.type);
              }  /* if */
              /* Carefully add the two, if appropriate. */
              if (ovfl) {
                do_constexpr_fail(result);
                info_with_pos(ec_integer_overflow, &expr->position, ips);
              } else if (is_function_address(&result_addr)) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_non_array_subscript,
                              &expr->position, ips);
              } else if (result_addr.address == NULL &&
                         !is_runtime_data_address(&result_addr)) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_invalid_null_ptr_operation,
                              &expr->position, ips);
              } else {
                if (host_int_val == 0) {
                  /* Leave the address unchanged. */
                  SET_result_val_from_operand_address(&result_addr);
                } else if (!is_array_element(&result_addr) &&
                           !is_runtime_data_address(&result_addr)) {
                  if (host_int_val ==
                                (cannot_dereference(&result_addr) ? -1 : 1)) {
                    /* A non-array element is treated as an array of one
                       element. */
                    a_byte_count  elem_size;
                    elem_size = value_bytes_for_type(ips, elem_type, &result);
                    result_addr.address += host_int_val * elem_size;
                    result_addr.flags ^= CA_CANNOT_DEREFERENCE;
                    SET_result_val_from_operand_address(&result_addr);
                  } else {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_non_array_subscript,
                                  &expr->position, ips);
                  }  /* if */
                } else {
                  a_byte_count  elem_size, pos, len;
                  get_array_pos(ips, &result_addr, elem_type, &len, &pos,
                                &elem_size, &result);
                  if (!result) break;
                  if (host_int_val > 0 ? (len-pos < (a_byte_count)host_int_val)
                                       : (pos < (a_byte_count)-host_int_val)) {
                    /* Out of bounds. */
                    do_constexpr_fail(result);
                    info_with_pos_num2(ec_constexpr_out_of_bounds_array_access,
                                       &expr->position,
                                       (uint32_t)(pos+host_int_val),
                                       (uint32_t)len, ips);
                  } else {
                    if (is_runtime_data_address(&result_addr)) {
                      if (!offset_runtime_address(
                               ips, &expr->position, &result_addr,
                               host_int_val, elem_size, /*subtract=*/FALSE)) {
                        do_constexpr_fail(result);
                      }  /* if */
                    } else {
                      result_addr.address += host_int_val * elem_size;
                    }  /* if */
                    if (pos+host_int_val == len) {
                      result_addr.flags |= CA_CANNOT_DEREFERENCE;
                    } else {
                      result_addr.flags &= ~CA_CANNOT_DEREFERENCE;
                    }  /* if */
                    SET_result_val_from_operand_address(&result_addr);
                  }  /* if */
                }  /* if */
              }  /* if */
            }
            break;
#if GNU_VECTOR_TYPES_ALLOWED
          case eok_vector_subscript:
            /* The first operand is a vector (lvalue or rvalue), and the
               second operand is an integer. */
            { int           len = opnd1_type->size/tp->size;
              a_type_ptr    etp;
              a_byte_count  esize;
              check_assertion(type_is(opnd1_type, tk_vector) &&
                              type_is(opnd2_type, tk_integer));
              get_int_val_from(opnd2_value, opnd2_type, host_int_val, ovfl);
              if (host_int_val < 0 || (int)host_int_val >= len) {
                do_constexpr_fail(result);
                info_with_pos_num2(ec_constexpr_out_of_bounds_array_access,
                                   &expr->position, (uint32_t)host_int_val,
                                   (uint32_t)len, ips);
                break;
              }  /* if */
              etp = skip_typerefs(opnd1_type->variant.vector.element_type);
              esize = value_bytes_for_type(ips, etp, &result);
              if (!result) break;
              if (opnd1->is_lvalue) {
                a_constexpr_address
                             result_addr = *(a_constexpr_address*)opnd1_value;
                result_addr.address += host_int_val*esize;
                result_addr.flags &= ~CA_ARRAY_ELEMENT;
                SET_result_val_from_operand_address(&result_addr);
              } else {
                (void)memcpy(result_storage, opnd1_value+host_int_val*esize,
                             size_t_arg(esize));
              }  /* if */
            }
            break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
          case eok_dot_field:
          case eok_points_to_field:
            { a_constexpr_address  result_addr;
              a_field_ptr          field = node_field(opnd2);
              a_byte_count         offset;
              if (opnd1->is_lvalue || opnd1->is_xvalue ||
                  opnd1_type->kind == (a_type_kind)tk_pointer) {
                /* The first operand is already an address. */
                if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                  opnd1_type = skip_typerefs(opnd1_type->variant.pointer.type);
                }  /* if */
                result_addr = *(a_constexpr_address*)opnd1_value;
              } else {
                /* The first operand is a class rvalue: Create an address for
                   it. */
                mark_complete_object_initialized(opnd1_value);
                clear_address(&result_addr, opnd1_value);
                result_addr.alloc_seq_number =
                                          ips->storage_stack.alloc_seq_number;
              }  /* if */
              if (is_runtime_data_address(&result_addr)) {
                /* Attempt to compute a new offset for a run-time address
                   constant. */
                a_constant_ptr  addr_con = result_addr.variant.addr_con;
                if (constant_is(addr_con, ck_address) &&
                    addr_con->variant.address.kind ==
                                         (an_address_base_kind)abk_variable) {
                  a_variable_ptr
                              vp = addr_con->variant.address.variant.variable;
                  if (addr_con->variant.address.offset >=
                            (a_targ_ptrdiff_t)skip_typerefs(vp->type)->size) {
                    /* An attempt to select a field outside the variable. */
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_access_past_object,
                                  &expr->position, ips);
                    break;
                  }  /* if */
                }  /* if */
                if (!(expr->is_lvalue || expr->is_xvalue) ||
                    field->is_bit_field) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else {
                  a_constant_ptr    new_con = local_constant();
                  an_expr_node_ptr  backing_expr = addr_con->expr;
                  /* Temporarily clear the backing expression to avoid
                     maintaining it at this stage. */
                  addr_con->expr = NULL;
                  if (!fold_field_selection(
                                  addr_con, field,
                                  make_reference_type(expr->type), new_con)) {
                    unexpected_condition();
                  }  /* if */
                  addr_con->expr = backing_expr;
                  clear_runtime_constant_address(result_storage, new_con);
                  new_con->next = ips->constants;
                  ips->constants = new_con;
                  /* Set the "length" field of the result to one to indicate
                     that this should be treated as an object address even if
                     the address is null.  That is needed to accommodate
                     traditional "offsetof" implementations. */
                  ((a_constexpr_address*)result_storage)->length = 1;
                }  /* if */
              } else if (result_addr.address == NULL) {
                /* An attempt to offset a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else {
                if (parent_class_of(field)->kind == (a_type_kind)tk_union) {
                  if (!add_to_variant_path(&result_addr, field, opnd1_type)) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_too_many_nested_anonymous_types,
                                  &expr->position, ips);
                    break;
                  }  /* if */
                }  /* if */
                get_mapped_byte_count(&persistent_map, field, offset);
                result_addr.address += offset;
                result_addr.flags &= ~CA_ARRAY_ELEMENT;
                if (field->is_bit_field) {
                  /* Record that the lvalue is that of a bit field.  The
                     length and signedness of the field are encoded in
                     result_addr.length. */
                  result_addr.flags |= CA_BIT_FIELD;
                  result_addr.length = field->bit_size*2 +
                                       field->bit_field_is_signed;
                }  /* if */
                if (field->is_mutable) {
                  result_addr.flags &= ~CA_CONST_STORAGE;
                } else if (is_const_qualified_type(field->type)) {
                  result_addr.flags |= CA_CONST_STORAGE;
                }  /* if */
                SET_result_val_from_operand_address(&result_addr);
              }  /* if */
            }
            break;
          case eok_pm_field:
          case eok_pm_points_to_field:
            { /* Accessing a field through a pointer-to-member is almost
                 identical to accessing it directly (see above), except for
                 the possibility of a null pointer-to-member and the
                 potential need for a "this" adjustment. */
              a_constexpr_address     result_addr;
              a_field_ptr             field;
              a_byte_count            offset;
              a_constexpr_ptr_to_mem  *pm_value;
              if (opnd1->is_lvalue || opnd1->is_xvalue ||
                  opnd1_type->kind == (a_type_kind)tk_pointer) {
                /* The first operand is already an address. */
                if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                  opnd1_type = skip_typerefs(opnd1_type->variant.pointer.type);
                }  /* if */
                result_addr = *(a_constexpr_address*)opnd1_value;
              } else {
                /* The first operand is a class rvalue: Create an address for
                   it. */
                mark_complete_object_initialized(opnd1_value);
                clear_address(&result_addr, opnd1_value);
                result_addr.alloc_seq_number =
                                          ips->storage_stack.alloc_seq_number;
              }  /* if */
              pm_value = (a_constexpr_ptr_to_mem*)opnd2_value;
              field = pm_value->variant.field;
              if (field == NULL) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_ptr_to_member_data,
                              &expr->position, ips);
              } else if (is_runtime_data_address(&result_addr)) {
                a_constant_ptr  addr_con = result_addr.variant.addr_con;
                if (constant_is(addr_con, ck_address) &&
                    addr_con->variant.address.kind ==
                                         (an_address_base_kind)abk_variable) {
                  a_variable_ptr
                              vp = addr_con->variant.address.variant.variable;
                  if (addr_con->variant.address.offset >=
                            (a_targ_ptrdiff_t)skip_typerefs(vp->type)->size) {
                    /* An attempt to select a field outside the variable. */
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_access_past_object,
                                  &expr->position, ips);
                    break;
                  }  /* if */
                }  /* if */
                if (!(expr->is_lvalue || expr->is_xvalue)) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else {
                  a_constant_ptr    new_con = local_constant();
                  /* Temporarily clear the backing expression to avoid
                     maintaining it at this stage. */
                  an_expr_node_ptr  backing_expr = addr_con->expr;
                  addr_con->expr = NULL;
                  if (!fold_field_selection(
                                  addr_con, field,
                                  make_reference_type(expr->type), new_con)) {
                    unexpected_condition();
                  }  /* if */
                  addr_con->expr = backing_expr;
                  clear_runtime_constant_address(result_storage, new_con);
                  new_con->next = ips->constants;
                  ips->constants = new_con;
                  /* Set the "length" field of the result to one to indicate
                     that this should be treated as an object address even if
                     the address is null.  That is needed to accommodate
                     traditional "offsetof" implementations. */
                  ((a_constexpr_address*)result_storage)->length = 1;
                }  /* if */
              } else if (result_addr.address == NULL) {
                /* An attempt to offset a null pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_dereference, &expr->position,
                              ips);
              } else {
                if (parent_class_of(field)->kind == (a_type_kind)tk_union) {
                  if (!add_to_variant_path(&result_addr, field, opnd1_type)) {
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_too_many_nested_anonymous_types,
                                  &expr->position, ips);
                    break;
                  }  /* if */
                }  /* if */
                if (!adjust_this_address(ips, &result_addr, pm_value,
                                         opnd1_type, expr)) {
                  do_constexpr_fail(result);
                  break;
                } else {
                  get_mapped_byte_count(&persistent_map, field, offset);
                  result_addr.address += offset;
                  result_addr.flags &= ~CA_ARRAY_ELEMENT;
                  if (field->is_bit_field) {
                    /* Record that the lvalue is that of a bit field.  The
                       length and signedness of the field are encoded in
                       result_addr.length. */
                    result_addr.flags |= CA_BIT_FIELD;
                    result_addr.length = field->bit_size*2 +
                                         field->bit_field_is_signed;
                  }  /* if */
                }  /* if */
                if (field->is_mutable) {
                  result_addr.flags &= ~CA_CONST_STORAGE;
                } else if (is_const_qualified_type(field->type)) {
                  result_addr.flags |= CA_CONST_STORAGE;
                }  /* if */
                SET_result_val_from_operand_address(&result_addr);
              }  /* if */
            }
            break;
          case eok_question:
            { a_boolean  bool_val, restore_lvalue, restore_xvalue,
                         expr_is_prvalue = !(expr->is_lvalue ||
                                             expr->is_xvalue);
              if (!check_boolean_condition(ips, opnd1_value, opnd1, opnd1_type,
                                           &bool_val)) {
                do_constexpr_fail(result);
                break;
              }  /* if */
              if (!bool_val) {
                /* Evaluate the third operand. */
                opnd2 = opnd2->next;
              }  /* if */
              /* The caller might have "rvalued" expr, but that didn't
                 propagate to the operands.  Temporarily enable that
                 propagation. */
              restore_lvalue = FALSE;
              restore_xvalue = FALSE;
              if (opnd2->is_lvalue && expr_is_prvalue) {
                opnd2->is_lvalue = FALSE;
                restore_lvalue = TRUE;
              }  /* if */
              if (opnd2->is_xvalue && expr_is_prvalue) {
                opnd2->is_xvalue = FALSE;
                restore_xvalue = TRUE;
              }  /* if */
              result = do_constexpr_expression(
                                 ips, opnd2, result_storage, complete_object);
              if (restore_xvalue) opnd2->is_xvalue = TRUE;
              if (restore_lvalue) opnd2->is_lvalue = TRUE;
            }
            break;
          case eok_dynamic_cast:
          case eok_ref_dynamic_cast:
            result = do_constexpr_dynamic_cast(
                                           ips, expr, opnd1_type, opnd1_value,
                                           result_storage);
            break;
          case eok_call:
            /* Calls are handled separately.  We should not get here. */
            unexpected_condition();
            FALLTHROUGH
          default:
            do_constexpr_fail(result);
            info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                          &expr->position, ips);
        }  /* switch */
      }
      break;
    case enk_constant:
      {
        a_constant_ptr  con = node_constant(expr);
        a_byte          *con_bytes;
        if (type_is(tp, tk_array) && (expr->is_lvalue || expr->is_xvalue) &&
            !is_any_reference_type(con->type)) {
          /* An array glvalue (most commonly a string literal).  Allocate the
             array statically and return its address.  Make sure that multiple
             uses of the constant produce the same address. */
          get_mapped_ptr(&ips->map, con, con_bytes);
          if (con_bytes == NULL) {
            alloc_static_object(ips, tp, con_bytes, &result);
            if (result) {
              mark_complete_object_initialized(con_bytes);
              map_ptr(&ips->map, con, con_bytes);
              result = copy_val_from_constant(ips, con, con_bytes, con_bytes);
            }  /* if */
          }  /* if */
          clear_address(result_storage, con_bytes);
          ((a_constexpr_address*)result_storage)->flags |= CA_CONST_STORAGE;
        } else {
          con_bytes = result_storage;
          result = copy_val_from_constant(ips, con, con_bytes,
                                          complete_object);
          if (constant_is(con, ck_string)) {
            /* An rvalue string constant is rare, but possible with structured
               bindings:
                 auto [sb] = "";
               This initializes the unnamed array variable from an rvalue
               string constant.  copy_val_from_constant will have mapped
               con_bytes to con assuming it could reuse those bytes in the
               future, but that is not the case for an rvalue: Undo the
               mapping. */
            unmap_stack_bytes(ips, con_bytes);
          }  /* if */
        }  /* if */
      }
      break;
    case enk_variable:
      {
        a_variable_ptr  var = node_variable(expr);
        a_byte          *var_bytes;
        if (var->init_kind == (an_init_kind)initk_binding) {
          /* This variable is an alias for an lvalue expression. */
          if (!do_constexpr_bound_expr(ips, expr, result_storage,
                                       complete_object)) {
            result = FALSE;
          }  /* if */
          break;
        }  /* if */
        get_stack_bytes(ips, var, var_bytes);
        if (!expr->is_lvalue && !expr->is_xvalue) {
          /* A variable used as an rvalue; the result is its associated
             value bytes. */
          if (is_volatile_qualified_type(var->type)) {
            info_with_pos(ec_constexpr_volatile_fetch, &expr->position, ips);
            do_constexpr_fail(result);
          } else if (var_bytes != NULL) {
            /* This is a variable on the interpreter stack. */
            if (is_immediate_class_type(tp) || type_is(tp, tk_array)) {
              result = constexpr_copy_object(ips, tp, &expr->position,
                                             var_bytes, var_bytes,
                                             result_storage, complete_object);
            } else {
              if (!complete_object_is_initialized(var_bytes)) {
                info_with_pos(ec_object_not_initialized, &expr->position, ips);
                do_constexpr_fail(result);
              }  /* if */
              n_bytes = value_bytes_for_type(ips, tp, &result);
              if (result) {
                (void)memcpy(result_storage, var_bytes, size_t_arg(n_bytes));
                if (type_is(tp, tk_pointer)) {
                  /* Copying an address type.  Make sure its side structures,
                     if any, are not shared. */
                  copy_address_structures(result_storage);
                }  /* if */
              }  /* if */
            }  /* if */
            if (result_storage == complete_object) {
              /* Mark the destination storage as fully initialized. */
              mark_complete_object_initialized(complete_object);
            }  /* if */
          } else {
            /* This variable is not allocated in the interpreter.  See if it
               is a constant-valued variable. */
            a_constant_ptr  con = var_constant_value(var);
            if (con != NULL) {
              a_boolean  saved_flag = ips->disallow_mutable_field_load;
              ips->disallow_mutable_field_load = TRUE;
              result = copy_val_from_constant(ips, con, result_storage,
                                              result_storage);
              ips->disallow_mutable_field_load = saved_flag;
            } else if (is_immediate_class_type(tp) &&
                       tp->variant.class_struct_union.is_empty_class &&
                       is_trivially_copyable_type(tp) &&
                       !(ms_version_is(<1914) || gpp_version_is(<50000) ||
                         clang_version_is(< 30600))) {
              /* An empty class type object with trivial copy semantics is
                 considered "constant". */
              mark_whole_subobject_initialized(ips, result_storage, tp,
                                               complete_object);
              init_subobject_to_zero(ips, result_storage, tp, complete_object);
              if (complete_object == result_storage) {
                /* Mark the destination storage as fully initialized. */
                mark_complete_object_initialized(complete_object);
              }  /* if */
            } else {
              if (var->is_this_parameter) {
                info_with_pos(ec_star_this_not_constant_valued,
                              &expr->position, ips);
              } else if (symbol_for(var) == NULL) {
                /* This can happen with synthesized variables such as the one
                   generated for __func__. */
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
              } else {
                info_with_pos_sym(ec_variable_not_constant_valued,
                                  &expr->position, symbol_for(var), ips);
              }  /* if */
              do_constexpr_fail(result);
            }  /* if */
          }  /* if */
        } else {
          /* A variable used as a glvalue; the result is its address. */
          if (var_bytes != NULL) {
            a_constexpr_address  *cap = (a_constexpr_address*)result_storage;
            a_byte_count         obj_size;
            obj_size = value_bytes_for_type(ips, tp, &result);
            if (result) {
              do_host_alignment(obj_size);
              clear_address(result_storage, var_bytes);
              /* Record the allocation sequence number for this variable in
                 the address record. */
              cap->alloc_seq_number =
                     ((a_var_postfix*)(var_bytes+obj_size))->alloc_seq_number;
              if (is_const_qualified_type(var->type)) {
                cap->flags |= CA_CONST_STORAGE;
              }  /* if */
            }  /* if */
          } else {
            /* A reference to a run-time variable.  This may not be valid,
               but we cannot usually tell at this time. */
            a_constant_ptr  con;
            a_byte          *con_ptr;
            a_boolean       saved_flag;
#if GNU_EXTENSIONS_ALLOWED
            if (var->is_weak) {
              /* Weakly declared variables have no definite address (they
                 could have a null address). */
              info_with_pos_sym(ec_constexpr_weak_address, &expr->position,
                                symbol_for(var), ips);
              do_constexpr_fail(result);
              break;
            }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
            if (type_is(tp, tk_array) && tp->incomplete) {
              /* The variable was an array of unknown bound when parsed.
                 However, in the current evaluation context its bound may now
                 be known.  For example:
                   extern const int arr[];
                   constexpr auto p = arr;
                   constexpr int f(int i) { return p[i]; }  // (X)
                   constexpr int arr[] = { 1, 2, 3 };
                   constexpr int x = f(2);  // (Y)
                 At point (X), the bound of arr was unknown, but by the time
                 it is evaluated (point (Y)), the type has been completed.
                 Update the node type so a parent node can correctly record
                 the array bound.  */
              a_type_ptr  vtp = skip_typerefs(var->type);
              if (type_is(vtp, tk_array) && !vtp->incomplete) {
                expr->type = var->type;
              }  /* if */
            }  /* if */
            get_mapped_ptr(&ips->map, &var->initializer, con_ptr);
            con = (a_constant_ptr)con_ptr;
            if (con == NULL) {
              /* No address constant is associated with this variable yet.
                 Create one and associate it with the variable so it can be
                 reused in the future. */
              con = local_constant();
              clear_constant(con, (a_constant_repr_kind)ck_address);
              con->next = ips->constants;
              ips->constants = con;
              con->variant.address.kind = (an_address_base_kind)abk_variable;
              con->variant.address.variant.variable = var;
              con->type = make_reference_type(var->type);
              map_ptr(&ips->map, &var->initializer, (a_byte*)con);
            }  /* if */
            saved_flag = ips->disallow_mutable_field_load;
            ips->disallow_mutable_field_load = TRUE;
            result = extract_value_from_constant(
                                    ips, con, result_storage, result_storage);
            ips->disallow_mutable_field_load = saved_flag;
          }  /* if */
        }  /* if */
      }
      break;
    case enk_field:
      /* Nothing to do at this point.  Specific parent operators (like
         eok_dot_field) know what to do with this kind of node. */
      break;
    case enk_routine:
      { a_routine_ptr  rp = node_routine(expr);
        /* Don't treat a reference to a consteval function as a constant
           unless a constant is really needed.  That keeps the enk_routine
           node in the expression tree so invalid uses can be diagnosed. */
        if (rp != NULL &&
            !rp->is_prototype_instantiation &&
            !(rp->is_consteval && !ips->allow_consteval_routine_node)) {
#if GNU_EXTENSIONS_ALLOWED
          if (rp->is_weak) {
            /* Weakly declared functions have no definite address (they could
               have a null address). */
            info_with_pos_sym(ec_constexpr_weak_address, &expr->position,
                              symbol_for(rp), ips);
            do_constexpr_fail(result);
            break;
          }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
          make_function_address(result_storage, node_routine(expr));
        } else {
          info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                        &expr->position, ips);
          do_constexpr_fail(result);
        }  /* if */
      }
      break;
    case enk_lambda:
    case enk_temp_init:
      { a_dynamic_init_ptr   dip;
        a_byte               *tmp_bytes, *tmp_complete_obj;
        an_alloc_seq_number  alloc_seq_number;
        a_byte_count         prefix_size;
        a_boolean            temp_lifetime;
        if (C_mode()) {
          info_with_pos(ec_constexpr_access_to_runtime_storage,
                        &expr->position, ips);
          do_constexpr_fail(result);
          break;
        }  /* if */
        dip = expr->variant.init.dynamic_init;
        n_bytes = value_bytes_for_type(ips, tp, &result);
        if (expr->is_lvalue || expr->is_xvalue) {
          /* A glvalue temporary is expected.  I.e., the caller expects an
             interpreter address for the temporary object.  Allocate the
             storage for that object here. */
          a_constexpr_address  *cap;
          if (!result) break;
          compute_prefix_size_for_type(tp, n_bytes, prefix_size);
          temp_lifetime = dip->has_temporary_lifetime;
          if (!temp_lifetime) {
            /* A lifetime-extended temporary.  Switch to the storage stack
               state that was saved at the time the stmk_init statement was
               started. */
            if (ips->extension_state != NULL) {
              alloc_bytes(ips->extension_state, n_bytes+prefix_size,
                          tmp_bytes);
              alloc_seq_number = ips->extension_state->alloc_seq_number;
              if (dip->destructor != NULL &&
                  !register_extended_destruction(ips, dip,
                                                 tmp_bytes+prefix_size,
                                                 tmp_bytes+prefix_size,
                                                 &expr->position)) {
                break;
              }  /* if */
              ips->extension_state = NULL;
            } else {
              /* If we're processing the initializer of a static-lifetime
                 variable, e.g.,
                   constexpr std::initializer_list<int> x = { 1, 2 };
                 there is no extended-lifetime storage.  Instead, the result
                 will eventually be stored in IL, which is persistent across
                 interpreter invocations. */
              alloc_static_bytes(ips, n_bytes+prefix_size, tmp_bytes);
              alloc_seq_number = 0;
            }  /* if */
          } else {
            alloc_stack_bytes(ips, n_bytes+prefix_size, tmp_bytes);
            alloc_seq_number = ips->storage_stack.alloc_seq_number;
          }  /* if */
          memzero(tmp_bytes, size_t_arg(prefix_size-sizeof(a_type_ptr)));
          tmp_bytes += prefix_size;
          record_complete_object_type(tp, tmp_bytes);
          mark_complete_class_object_if_needed(tp, tmp_bytes);
          cap = (a_constexpr_address*)result_storage;
          clear_address(cap, tmp_bytes);
          /* Record the allocation sequence number for this temporary in the
             address record. */ 
          cap->alloc_seq_number = alloc_seq_number;
          if (!temp_lifetime) {
            cap->flags |= CA_LIFETIME_EXTENDED;
	  }  /* if */
          if (is_const_qualified_type(expr->type)) {
            cap->flags |= CA_CONST_STORAGE;
          }  /* if */
          if (type_is(tp, tk_array)) {
            /* We are referring to the array as a whole; not just one element
               of it.  Record the length in case it is needed later on. */
            cap->length =
                    (unsigned int)tp->variant.array.variant.number_of_elements;
          }  /* if */
          tmp_complete_obj = tmp_bytes;
        } else {
          /* The consumer of the temporary expects an rvalue.  So we can
             evaluate the initialization directly into result_storage. */
          tmp_bytes = result_storage;
          tmp_complete_obj = complete_object;
          temp_lifetime = TRUE;
          alloc_seq_number = ips->storage_stack.alloc_seq_number;
        }  /* if */
        if (dyn_init_is(dip, dik_zero)) {
          init_subobject_to_zero(ips, tmp_bytes, tp, tmp_complete_obj);
        } else {
          a_constexpr_address  dst_addr;
          clear_address(&dst_addr, tmp_bytes);
          dst_addr.alloc_seq_number = alloc_seq_number;
          dst_addr.complete_object = tmp_complete_obj;
          if (!do_constexpr_dynamic_init(ips, dip, &expr->position,
                                         &dst_addr)) {
            do_constexpr_fail(result);
          }  /* if */
        }  /* if */
        if (expr->is_lvalue || expr->is_xvalue) {
          if (!is_immediate_class_type(tp) && !type_is(tp, tk_array)) {
            /* Make sure that scalar-like types are marked as initialized.
               Aggregate types are marked member-by-member as they are
               initialized. */
            mark_complete_object_initialized(tmp_bytes);
          }  /* if */
        } else {
          mark_subobject_initialized(tmp_bytes, tmp_complete_obj);
        }  /* if */
        if (result && dip->destructor != NULL && temp_lifetime) {
          result = register_destruction(ips, dip, tmp_bytes, tmp_complete_obj,
                                        &expr->position);
        }  /* if */
      }
      break;
    case enk_new_delete:
      if (expr->variant.new_delete->is_new) {
        result = do_constexpr_new(ips, expr, result_storage, complete_object);
      } else {
        result = do_constexpr_delete(ips, expr);
      }  /* if */
      break;
    case enk_object_lifetime:
      result = do_constexpr_expression(ips, expr->variant.object_lifetime.expr,
                                       result_storage, complete_object);
      break;
    case enk_typeid:
      result = do_constexpr_typeid(ips, expr, result_storage, complete_object);
      break;
    case enk_param_ref:
      { a_byte  *this_bytes = NULL;
        if (expr->variant.param_ref.param_num == 0) {
          /* An entry representing "this" in a field initializer.  The code
             handling constructor calls (which initializes members based on
             field initializers when needed) associated the address of the
             "this" pointer variable for the constructor with
             &ips->curr_call_frame. */
          get_stack_bytes(ips, &ips->curr_call_frame, this_bytes);
        }  /* if */
        if (this_bytes != NULL) {
          n_bytes = value_bytes_for_type(ips, tp, &result);
          (void)memcpy(result_storage, this_bytes, size_t_arg(n_bytes));
          copy_address_structures(result_storage);
          if (result_storage == complete_object) {
            mark_complete_object_initialized(complete_object);
          } else {
            mark_subobject_initialized(result_storage, complete_object);
          }  /* if */
        } else {
          do_constexpr_fail(result);
          info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                        &expr->position, ips);
        }  /* if */
      }
      break;
    case enk_builtin_operation:
      result = do_constexpr_builtin_operation(ips, expr, result_storage,
                                              complete_object);
      break;
#if GNU_EXTENSIONS_ALLOWED
    case enk_statement:
      { a_call_frame     frame;
        a_statement_ptr  stmt = expr->variant.statement;
        /*lint -e{733}*/
        push_stmt_expr(ips, &frame, expr, result_storage, complete_object);
        result = do_constexpr_block_statement(
                      ips, stmt, stmt->variant.block.extra_info->assoc_scope);
        if (frame.parent == NULL &&
            (frame.return_active || frame.loop_break_active ||
             frame.continue_active || frame.switch_break_active)) {
          /* A branch is still active, but we're no longer in a statement
             context.  That is not valid. */
          info_with_pos(ec_branch_out_of_constant, &expr->position, ips);
          do_constexpr_fail(result);
        }  /* if */
        pop_call_frame(ips);
      }
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    case enk_initializer:
      { a_constexpr_address  dst_addr;
        set_active_address(ips, &dst_addr, result_storage, complete_object);
        if (!do_constexpr_dynamic_init(ips, expr->variant.initializer.dyn_init,
                                       &expr->position, &dst_addr)) {
          result = FALSE;
        }  /* if */
      }
      break;
    case enk_concept_id:
      { a_boolean  fatal = FALSE;
        if (concept_id_value(expr, &fatal)) {
          *(an_integer_value *)result_storage = one_int;
        } else {
          *(an_integer_value *)result_storage = zero_int;
        }  /* if */
        if (fatal) do_constexpr_fail(result);
      }
      break;
    case enk_requires:
      { a_subst_pairs_array  no_subst_pairs;
        if (is_template_dependent_context() && !scope_stack_top().is_rescan &&
            expr_is_instantiation_dependent(expr)) {
          /* We may get here when evaluating constant-expressions in dependent
             contexts.  Calling requires_expr_satisfied on a dependent
             requires expression can trigger a hard error and should thus not
             be attempted here. */
          do_constexpr_fail(result);
        } else {
          if (requires_expr_satisfied(expr, no_subst_pairs)) {
            *(an_integer_value *)result_storage = one_int;
          } else {
            *(an_integer_value *)result_storage = zero_int;
          }  /* if */
        }  /* if */
      }
      break;
    case enk_reuse_value:
      /* A reuse of a value that was evaluated earlier in this same expression
         tree via a dynamic-init entry.  At the time, the location of the
         result was stored in ips->map.  This case is currently only
         encountered in some GNU extensions (the occurrences in standard code
         are handled elsewhere). */
      { a_byte  *bytes;
        get_mapped_ptr(&ips->map, expr->variant.reused_value_init, bytes);
        if (bytes != NULL) {
          /* Retrieve the location of the original evaluation and copy the
             representation of that original evaluation.  This is fine, except
             that any self-referential addresses will keep referring to the
             original value.  That is often okay, except if the address is
             explicitly compared.  For now, we leave that approximation since
             it only affects rare, nonstandard cases. */
          n_bytes = value_bytes_for_type(ips, tp, &result);
          (void)memcpy(result_storage, bytes, size_t_arg(n_bytes));
        } else {
          do_constexpr_fail(result);
          info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                        &expr->position, ips);
        }  /* if */
      }
      break;
#if BUILTIN_FUNCTIONS_ENABLED
    case enk_builtin_choose_expr:
      { an_expr_node_ptr  active_expr = expr->variant.builtin_choose_expr
                                                     .operands->next;
        if (!expr->variant.builtin_choose_expr.choose_first) {
          active_expr = active_expr->next;
        }  /* if */
        result = do_constexpr_expression(ips, active_expr, result_storage,
                                         complete_object);
      }
      break;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
    case enk_error:
      ips->input_error = TRUE;
      FALLTHROUGH
    default:
      do_constexpr_fail(result);
      info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                    &expr->position, ips);
  }  /* switch */
done:
  return result;
#undef SET_result_val_from_operand_address
#undef CHECK_int_range
}  /* do_constexpr_expression */


static a_boolean
		trans_unit_initialization_needed;
			/* Flag indicating whether persistent_data and
			   persistent_storage have been initialized for this
			   translation unit. */


static void initialize_interpreter_data(void)
/*
Perform various initializations (both per-translation-unit and one-time)
that are needed for the operation of the interpreter.
*/
{
  init_constexpr_stack(&persistent_data);
  init_data_map(&persistent_map, 10);
  n_active_interpreter_states = 0;
  variant_path_entries = NULL;
  n_variant_path_entries = 0;
  free_variant_path_entries = NULL;
  n_free_variant_path_entries = 0;
  valid_placement_new_address = NULL;
  valid_placement_new_type = NULL;
  if (!useful_constants_initialized) {
    /* Initialize useful constants. */
    a_byte       fk;
    a_boolean    dummy;
    set_integer_value(&zero_int, (a_host_large_integer)0);
    set_integer_value(&one_int, (a_host_large_integer)1);
    for (fk = fk_float; fk < fk_last; ++fk) {
      fp_host_large_integer_to_float((a_float_kind)fk, (a_host_large_integer)0,
                                     &zero_flt[fk], &dummy);
      fp_host_large_integer_to_float((a_float_kind)fk, (a_host_large_integer)1,
                                     &one_flt[fk], &dummy);
    }  /* for */
    generic_ptr_type = make_pointer_type(void_type());
    useful_constants_initialized = TRUE;
  }  /* if */
}  /* initialize_interpreter_data */


static void translate_interpreter_offset(an_interpreter_state  *ips,
                                         a_constexpr_address   *cap,
                                         a_constant_ptr        con)
/*
cap represents an interpreter address pointing into interpreter storage.
Record in con->variant.address.offset the corresponding target offset for a
ck_address constant representing the same address.  Also record the associated
subobject path.
*/
{
  a_targ_ptrdiff_t  t_offset = 0; /* Total target offset. */
  a_byte            *address = cap->address;

  if (address != cap->complete_object) {
    a_byte                *parent_address = cap->complete_object;
    a_field_ptr           fp = NULL;
    a_base_class_ptr      bcp = NULL;
    a_subobject_path_ptr  path = NULL, end_path = NULL, path_entry;
    a_type_ptr            type = complete_object_type(parent_address);

    if (cannot_dereference(cap)) {
      a_boolean     result = TRUE;
      a_byte_count  n_bytes = value_bytes_for_type(ips, type, &result);
      check_assertion(result);
      if ((a_byte_count)(address - parent_address) == n_bytes) {
        a_type_ptr  tpt = type_pointed_to(con->type);
        if (skip_typerefs(tpt) == type) {
          /* The address points one position past the top-level variable and
             therefore not "into" the variable. */
          con->variant.address.offset = type->size;
          path = alloc_subobject_path();
          path->is_offset = TRUE;
          path->variant.ptr_offset = 1;
          con->variant.address.subobject_path = path;
          goto done;
        }  /* if */
      }  /* if */
    }  /* if */
    do {
      a_byte_count  i_offset;  /* Local interpreter offset. */
      if (is_immediate_class_type(type)) {
        void  *ptr;
        find_subobject_for_interpreter_address(ips, cap, parent_address, type,
                                               &fp, &bcp);
        /* Add a new subobject path entry if needed. */
        if (fp != NULL || (end_path == NULL || !end_path->is_base_class)) {
          /* For a field selection, we always create a new entry on the path.
             For a base selection, we only create one if the previous entry
             wasn't itself for a base selection. */
          path_entry = alloc_subobject_path();
          if (path == NULL) {
            path = path_entry;
          } else {
            check_assertion(end_path != NULL);
            end_path->next = path_entry;
          }  /* if */
          end_path = path_entry;
        } else {
          path_entry = NULL;
        }  /* if */
        if (fp != NULL) {
          t_offset += fp->offset;
          type = skip_typerefs(fp->type);
          ptr = (void*)fp;
          end_path->variant.field = fp;
        } else {
          check_assertion(bcp != NULL);
          t_offset += bcp->offset;
          type = bcp->type;
          ptr = (void*)bcp;
          end_path->is_base_class = TRUE;
          if (path_entry != NULL) {
            /* A new subobject path entry was allocated, which means this is
               the first derived-to-base step. */
            end_path->variant.base_class = bcp;
          } else {
            /* Not the first derived-to-base step: Find the base corresponding
               to bcp in the most-derived-class. */
            a_base_class_ptr  prev_full_bcp = end_path->variant.base_class,
                              full_bcp = base_classes_of(
                                                prev_full_bcp->derived_class);
            if (bcp->is_virtual) {
              /* For virtual base classes, look for a virtual base with a
                 matching type. */
              for (; full_bcp != NULL; full_bcp = full_bcp->next) {
                if (full_bcp->type == bcp->type && full_bcp->is_virtual) {
                  break;
                }  /* if */
              }  /* for */
            } else {
              /* For nonvirtual base classes, a matching type is not
                 sufficient: Make sure its parent base class is
                 prev_full_bcp. */
              for (; full_bcp != NULL; full_bcp = full_bcp->next) {
                if (full_bcp->type == bcp->type && !full_bcp->is_virtual) {
                  a_derivation_step_ptr  dsp = full_bcp->derivation->path;
                  /* Since this is not the first derivation step nor a virtual
                     derivation step, the path must be more than one step. */
                  check_assertion(dsp->next != NULL);
                  /* Find the second-to-last entry, which should match
                     prev_full_bcp. */
                  while (dsp->next->next != NULL) {
                    dsp = dsp->next;
                  }  /* if */
                  if (dsp->base_class == prev_full_bcp) {
                    break;
                  }  /* if */
                }  /* if */
              }  /* for */
            }  /* if */
            end_path->variant.base_class = full_bcp;
          }  /* if */
        }  /* if */
        get_mapped_byte_count(&persistent_map, ptr, i_offset);
      } else {
        i_offset = (a_byte_count)(address - parent_address);
        if (i_offset != 0) {
          a_byte_count  pos, elem_size;
          a_boolean     okay = TRUE;
          /* The last entry on the path shouldn't represent a pointer offset
             since otherwise either i_offset should be zero, or the remaining
             offset would be the result of pointing into a class type. */
          check_assertion(end_path == NULL || !end_path->is_offset);
          path_entry = alloc_subobject_path();
          if (path == NULL) {
            path = path_entry;
          } else {
            check_assertion(end_path != NULL);
            end_path->next = path_entry;
          }  /* if */
          end_path = path_entry;
          path_entry->is_offset = TRUE;
          while (type->kind == (a_type_kind)tk_array) {
            type = skip_typerefs(type->variant.array.element_type);
          }  /* while */
          elem_size = value_bytes_for_type(ips, type, &okay);
          check_assertion(okay);
          pos = i_offset/elem_size;
          path_entry->variant.ptr_offset = (a_targ_ptrdiff_t)pos;
          t_offset += pos*type->size;
          i_offset = pos*elem_size;
        }  /* if */
      }  /* if */
      parent_address += i_offset;
    } while (parent_address != address);
    con->variant.address.offset = t_offset;
    con->variant.address.subobject_path = path;
    con->implicit_cast = TRUE;
  }  /* if */
done:;
}  /* translate_interpreter_offset */


static a_byte_count interpreter_base_offset_of(a_base_class_ptr  bcp)
/*
Return the interpreter offset with the derived class for the given base.
*/
{
  a_byte_count  result;

  if (bcp->is_virtual || bcp->direct) {
    get_mapped_byte_count(&persistent_map, bcp, result);
  } else {
    a_derivation_step_ptr  step = bcp->derivation->path;
    a_type_ptr             tp = step->base_class->type;
    get_mapped_byte_count(&persistent_map, step->base_class, result);
    for (step = step->next; step != NULL; step = step->next) {
      a_byte_count  offset;
      bcp = find_base_in_type(tp, step->base_class->type);
      get_mapped_byte_count(&persistent_map, bcp, offset);
      result += offset;
      tp = step->base_class->type;
    }  /* for */
  }  /* if */
  return result;
}  /* interpreter_base_offset_of */


static a_boolean copy_interpreter_object_to_constant(
                                       an_interpreter_state  *ips,
                                       a_byte                *object,
                                       a_byte                *complete_object,
                                       a_type_ptr            type,
                                       a_constant_ptr        con);


static a_boolean alloc_const_for_object(
                                        an_interpreter_state  *ips,
                                        a_constexpr_address   *cap,
                                        a_constant_ptr        *result)
/*
Allocate a new constant at the current memory region for the interpreter object
pointed to by cap, for the interpreter state ips.  Resulted is updated to point
to the allocated constant. Return TRUE when the allocation operation is
successful, FALSE otherwise.
*/
{
  a_boolean       success = TRUE;
  a_constant_ptr  cp;

  /* Set up a reverse mapping, so other address constants into this
     object can use the same constant entry (see the case where mptr
     points to a non-ck_address entry above). */
  cp = *result = alloc_constant((a_constant_repr_kind)ck_error);
  map_stack_bytes(ips, cap->complete_object, (a_byte*)cp);
  /* Create an IL representation of the pointed-to-object.  In some
     cases, we may be pointing to a subobject; the representation is
     still needed for the complete object, however. */
  { a_type_ptr  otp = complete_object_type(cap->complete_object);
    if (is_const_storage(cap)) {
      otp = make_qualified_type(otp, (a_type_qualifier_set)TQ_CONST);
    }  /* if */
    if (!copy_interpreter_object_to_constant(
                               ips, cap->complete_object, cap->complete_object,
                               otp, cp)) {
      success = FALSE;
    }  /* if */
  }
  return success;
}  /* alloc_const_for_object */


static a_boolean copy_interpreter_object_to_constant(
                                       an_interpreter_state  *ips,
                                       a_byte                *object,
                                       a_byte                *complete_object,
                                       a_type_ptr            type,
                                       a_constant_ptr        con)
/*
The storage pointed to by object holds a representation of a value of the given
type produced by the interpreter.  Create in *con a constant representing
that same value.  Return FALSE if this cannot be done (e.g., because the object
represents an address of interpreter storage) and record a corresponding
diagnostic in *ips.
*/
{
  a_boolean  result = TRUE;

  clear_constant(con, (a_constant_repr_kind)ck_error);
  con->type = type;
  type = skip_typerefs(type);
  if (ips->call_seen ||
      (is_immediate_class_type(type) &&
       type->variant.class_struct_union.any_virtual_functions)) {
    /* If an actual constexpr call is involved, record that in the constant.
       If no call is involved, but the type has a virtual function, treat it
       as if the construction really did involve a call; lowering counts on
       the presence of this flag to add dynamic dispatch data. */
    con->is_result_of_constexpr_call = TRUE;
  }  /* if */
  switch (type->kind) {
    case tk_integer:
      set_constant_kind(con, (a_constant_repr_kind)ck_integer);
      con->variant.integer_value = *(an_integer_value *)object;
      /* In C, any constant expression with value zero can produce a "null
         pointer constant".  In C++, only integer literals of value zero can
         do so. */
      con->null_pointer_constant_ruled_out = ips->call_seen || !C_mode();
      break;
    case tk_float:
      set_constant_kind(con, (a_constant_repr_kind)ck_float);
      con->variant.float_value = *fp_value(object);
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
      set_constant_kind(con, (a_constant_repr_kind)ck_imaginary);
      con->variant.float_value = *fp_value(object);
      break;
    case tk_complex:
      set_constant_kind(con, (a_constant_repr_kind)ck_complex);
      *con->variant.complex_value = *cx_value(object);
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_pointer:
      { a_constexpr_address *cap = (a_constexpr_address *)object;
        if (is_runtime_data_address(cap)) {
          a_constant_ptr  rt_con = cap->variant.addr_con;
          /* Catch the case of a pointer or reference to a variable that is
             not constant-valued. */
          if (constant_is(rt_con, ck_address)) {
            an_address_base_kind  abk = rt_con->variant.address.kind;
            if (abk == (an_address_base_kind)abk_variable) {
              a_variable_ptr  vp = rt_con->variant.address.variant.variable;
              if (!variable_has_constant_address(vp)) {
                a_symbol_ptr  var_sym = symbol_for(vp);
                do_constexpr_fail(result);
                if (var_sym == NULL) {
                  /* A variable with no associated symbol (likely an anonymous
                     union parent object). */
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &vp->source_corresp.decl_position, ips);
                
                } else {
                  info_with_pos_sym(ec_variable_not_constant_addressed,
                                    &ips->position, var_sym, ips);
                }  /* if */
                break;
              }  /* if */
            } else if (abk == (an_address_base_kind)abk_routine) {
              a_routine_ptr  rp = rt_con->variant.address.variant.routine;
              if (rp->is_consteval) {
                info_with_pos_sym(ec_address_of_consteval_function,
                                  &ips->position, symbol_for(rp), ips);
                do_constexpr_fail(result);
              }  /* if */
            } else if (abk == (an_address_base_kind)abk_label) {
              a_label_ptr  lp = rt_con->variant.address.variant.label;
              info_with_pos(ec_constexpr_access_to_runtime_storage,
                            &lp->source_corresp.decl_position, ips);
              do_constexpr_fail(result);
            }  /* if */
          } else {
            /* Some "address" constants are integers cast to a pointer type. */
            check_assertion(constant_is(rt_con, ck_integer));
          }  /* if */
          /* Copy the run-time constant to con. */
          (void)copy_constant_full(rt_con, con,
                                   CE_COPYING_FOR_CONSTEXPR_MASTER_EXPR);
          con->type = type;
          if (type->variant.pointer.is_reference ||
              is_array_element(cap)) {
            /* The pointer-reinterpreted-as-reference case is marked as a kind
               of implicit_cast.  That ensures, e.g., that calling
               add_reference_indirection later on will not discard the constant
               and reduce it to just the variable reference (which would lose
               position information and render incorrectly in the
               C++-generating back end).  Similarly, array decay cases are
               marked as "implicit_cast" to avoid generating an extra cast in
               the C-generating back end (and, apparently, doing otherwise
               complicates certain traditional back ends). */
            con->implicit_cast = TRUE;
          }  /* if */
        } else if (is_function_address(cap)) {
          a_type_ptr     utp = type->variant.pointer.type;
          a_routine_ptr  rp = cap->variant.routine;
          if (rp->is_consteval) {
            info_with_pos_sym(ec_address_of_consteval_function,
                              &ips->position, symbol_for(rp), ips);
            do_constexpr_fail(result);
          } else {
            set_routine_address_constant(rp, con,
                                         /*set_address_taken_flag=*/TRUE);
            con->type = type;
            if (!identical_types(utp, rp->type)) {
              /* The pointer to function type was converted to a different
                 pointer type (e.g., void*). */
              con->implicit_cast = TRUE;
            }  /* if */
          }  /* if */
        } else if (cap->address == NULL) {
          /* A NULL pointer (since it has a pointer type, it is not a null
             pointer constant). */
          set_constant_kind(con, (a_constant_repr_kind)ck_integer);
          con->implicit_cast = TRUE;
          /* In C, any constant expression with value zero can produce a "null
             pointer constant".  In C++, only integer literals of value zero
             can do so. */
          con->null_pointer_constant_ruled_out = ips->call_seen || !C_mode();
        } else if (cap->alloc_seq_number > 1) {
          /* The address designates an interpreter value that is already
             deallocated, and thus cannot be constant. */
          do_constexpr_fail(result);
          info_with_pos(ec_constexpr_interpreter_address, &ips->position, ips);
        } else {
          /* Check if this address is already mapped to a constant. */
          a_type_ptr      utp = type->variant.pointer.type;
          a_byte          *mptr;
          a_constant_ptr  cp;
          a_variable_ptr  vp = NULL;
          a_boolean       is_subobj_addr, permit_local_temp;
          is_subobj_addr = cap->complete_object != cap->address ||
                           is_array_element(cap);
          permit_local_temp = ips->permit_address_of_local_temporary;
          if (permit_local_temp) {
            ips->permit_address_of_local_temporary = FALSE;
          }  /* if */
          set_constant_kind(con, (a_constant_repr_kind)ck_address);
          get_stack_bytes(ips, cap->complete_object, mptr);
          if (mptr != NULL) {
            /* Either a constant was already allocated for the pointed-to
               object or this address was created from an abk_variable entry
               (in which case, we must produce an address constant for that
               same variable). */
            a_constant_ptr  prev_con = (a_constant_ptr)mptr;
            if (!constant_is(prev_con, ck_address)) {
              /* Presumably this is a constant created for an abk_constant
                 address by the code below or it is a ck_string entry that
                 was loaded into interpreter storage (see
                 extract_value_from_constant).  In the latter case make a
                 copy of the string constant to avoid memory region issues. */
              cp = prev_con;
            } else if (prev_con->variant.address.kind ==
                                         (an_address_base_kind)abk_variable) {
              vp = prev_con->variant.address.variant.variable;
              if (vp == NULL) {
                /* This can happen when interpreting a dynamic initialization
                   entry that isn't associated with a variable.  Currently,
                   our IL cannot represent the folded result. */
                do_constexpr_fail(result);
                break;
              }  /* if */
              cp = NULL;
            } else {
              cp = prev_con->variant.address.variant.constant;
            }  /* if */
          } else {
            /* Create an abk_constant or abk_temporary entry. */
            a_byte  *base_address;
            if (is_const_qualified_type(utp)) {
              cap->flags |= CA_CONST_STORAGE;
            }  /* if */
            if (cap->length != 0 && !is_bit_field_lvalue(cap)) {
              /* If we're pointing at or into an array, a constant for the
                 whole array will be produced, but we may have to compute
                 the offset into that array. */
              a_type_ptr    butp = skip_typerefs(utp);
              a_byte_count  offset;
              if (is_array_element(cap)) {
                base_address = get_base_address(cap);
              } else {
                base_address = cap->address;
              }  /* if */
              offset = (a_byte_count)(cap->address - base_address);
              if (offset != 0) {
                if (!is_array_element(cap) ||
                    (butp->incomplete && type_is(butp, tk_array))) {
                  /* This can happen when binding a reference to an array with
                     no specified bound.  E.g.:
                       struct S { const int (&x)[]; };
                       constexpr S x = { { 37 } };
                     We'll produce a known bound below. */
                  utp = butp->variant.array.element_type;
                  butp = skip_typerefs(utp);
                }  /* if */
                con->variant.address.offset =
                      butp->size *
                            (offset/value_bytes_for_type(ips, butp, &result));
              }  /* if */
            } else {
              base_address = cap->address;
            }  /* if */
            if (!alloc_const_for_object(ips, cap, &cp)) {
              do_constexpr_fail(result);
              break;
            }  /* if */
          }  /* if */
          if (is_array_element(cap)) {
            /* Taking the address of an array implies a pointer-to-array
               type, but we're really creating a pointer to the first
               element of the array.  Record the presence of an implicit
               cast (otherwise, lowering may sometimes restore the pointer-
               to-array type, which in turn can lead to invalid C code
               generated by the C-generating back end). */
            con->implicit_cast = TRUE;
          } else if (type->variant.pointer.is_reference) {
            /* The pointer-reinterpreted-as-reference case is also marked as a
               kind of implicit_cast.  That ensures, e.g., that calling
               add_reference_indirection later on will not discard the
               constant and reduce it to just the variable reference (which
               would lose position information and render incorrectly in the
               the C++-generating back end). */
            con->implicit_cast = TRUE;
          }  /* if */
          if (vp != NULL) {
            if (var_has_static_storage_duration(vp)) {
              con->variant.address.kind = (an_address_base_kind)abk_variable;
              con->variant.address.variant.variable = vp;
            } else {
              /* Not a variable with static storage duration (e.g., a thread-
                 local variable). */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_access_to_runtime_storage,
                            &ips->position, ips);
            }  /* if */
          } else {
            if (constant_is(cp, ck_string)) {
              if (ips->call_seen) {
                /* If the ck_string constant was allocated in a different
                   memory region, we recorded &cp-variant.string.value in the
                   interpretation map.  In that case, a copy should be made to
                   avoid memory region violations. */
                a_byte  *placeholder;
                get_stack_bytes(ips, &cp->variant.string.value, placeholder);
                if (placeholder != NULL) {
                    /* Copy the string entry, but mark it as being the result
                       of a constant-expression evaluation, to distinguish it
                       from an actual string literal. */
                  cp = alloc_unshared_constant(cp);
                  cp->is_result_of_constexpr_call = TRUE;
#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
                  cp->variant.string.sequence_number = 0;
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
                }  /* if */
              }  /* if */
              con->variant.address.kind = (an_address_base_kind)abk_constant;
            } else if (!ips->is_constant_evaluated) {
              /* If we're not in a "manifest-constant" context, do not produce
                 a ck_address/abk_temporary entry.  If the temporary has a
                 mutable type, we will not load its value in a later evaluation
                 but we may have to do so if an enclosing context is in fact a
                 "manifest constant".  For example:
                     constexpr int g(int &r) { r *= 2; return r; }
                     struct S { int &&rr; int i; };
                     constexpr S s = { 42, g(s.rr) };
                 Here, if we folded the binding of 42 to s.rr into a ck_address
                 constant, we'd fail to later evaluate accesses to s.rr when
                 evaluating the braced initializer as a whole.  So, by leaving
                 the associated enk_temporary in the IL, we enable the
                 possibility of a successful interpretation at a higher level.
              */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_access_to_runtime_storage,
                            &ips->position, ips);
            } else if (ips->static_lifetime_init && cp->is_compound_literal) {
              /* This can occur when taking the address of a compound literal
                 (e.g., the decay of a compound literal array). */
              con->variant.address.kind = (an_address_base_kind)abk_constant;
            } else {
              con->variant.address.kind = (an_address_base_kind)abk_temporary;
              if (!((cap->flags & CA_LIFETIME_EXTENDED) ||
                    cap->alloc_seq_number == 0) ||
                  (!ips->static_lifetime_init && !permit_local_temp)) {
#if BUILTIN_FUNCTIONS_ENABLED
                /* If the complete object is a naturalizable, it can be
                   promoted from a temporary. */
                if (is_naturalizable_object(ips, cap->complete_object)) {
#if EXPENSIVE_CHECKING
                  /* The allocation sequence number should always represent
                     static storage (i.e., be 0). */
                  check_assertion(cap->alloc_seq_number == 0);
#endif /* EXPENSIVE_CHECKING */
                  con->variant.address.kind =
                                            (an_address_base_kind)abk_constant;
                  /* If the current memory region isn't the file scope region
                     the allocated constant may be freed prematurely.  To avoid
                     this, switch to the file scope region, and recreate the
                     constant. */
                  if (curr_il_region_number != file_scope_region_number) {
                    a_memory_region_number  region_to_switch_back_to;

                    switch_to_file_scope_region(&region_to_switch_back_to);
                    /* Unmap the constant, and replace it with one in the file
                       scope memory region. */
                    unmap_stack_bytes(ips, cap->complete_object);
                    a_boolean alloced = alloc_const_for_object(ips, cap, &cp);
                    switch_back_to_original_region(region_to_switch_back_to);
                    /* Only break after restoring the memory region. */
                    if (!alloced) {
                      do_constexpr_fail(result);
                      break;
                    }  /* if */
                  }  /* if */
                  cp->is_naturalized = TRUE;
                } else
#endif /* BUILTIN_FUNCTIONS_ENABLED */
                /* Do not add code here. */
                {
                  /* The address of a temporary results in a dangling
                     pointer. */
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_expiring_temporary,
                                &ips->position, ips);
                }  /* if */
              }  /* if */
            }  /* if */
            con->variant.address.variant.constant = cp;
          }  /* if */
          if (is_subobj_addr) {
            translate_interpreter_offset(ips, cap, con);
          }  /* if */
        }  /* if */
        if (is_variant_path(cap)) {
          release_variant_path(cap);
        }  /* if */
      }
      break;
    case tk_ptr_to_member:
      {
        a_constexpr_ptr_to_mem  *pm_value = (a_constexpr_ptr_to_mem*)object;
        set_constant_kind(con, (a_constant_repr_kind)ck_ptr_to_member);
        if (pm_value->is_ptr_to_mem_function) {
          a_routine_ptr  rp = pm_value->variant.routine;
          if (rp != NULL && rp->is_consteval) {
            info_with_pos_sym(ec_address_of_consteval_function,
                              &ips->position, symbol_for(rp), ips);
            do_constexpr_fail(result);
          } else {
            con->variant.ptr_to_member.is_function_ptr = TRUE;
            con->variant.ptr_to_member.variant.routine = rp;
          }  /* if */
        } else {
          con->variant.ptr_to_member.variant.field = pm_value->variant.field;
        }  /* if */
        if (pm_value->this_class_adjustment != 0) {
          a_type_ptr        dtype = pm_value->is_ptr_to_mem_function ?
                                  parent_class_of(pm_value->variant.routine) :
                                  parent_class_of(pm_value->variant.field);
          a_base_class_ptr  bcp = base_classes_of(dtype);
          con->variant.ptr_to_member.cast_to_base =
                                                pm_value->subtract_adjustment;
          for (; bcp != NULL; bcp = bcp->next) {
            if (interpreter_base_offset_of(bcp) ==
                                            pm_value->this_class_adjustment) {
              con->variant.ptr_to_member.casting_base_class = bcp;
              con->implicit_cast = TRUE;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
      }
      break;
    case tk_struct:
    case tk_class:
      if (type->variant.class_struct_union.is_nonreal_class) {
        /* We may get this far when interpreting a lambda expression that is
           nonreal only because it appears in a template-dependent context.
           Interpreting such lambdas as an intermediate result is fine because
           it can produce a nondependent value, but if it is the top-level
           expression, we might create an odd constant value whose
           representation is nondependent but whose type is nonreal (that,
           e.g., causes problems in lowering/mangling). */
        info_with_pos_type(ec_constexpr_type_invalid, &ips->position, type,
                           ips);
        do_constexpr_fail(result);
      } else {
        a_base_class_ptr  bcp;
        a_field_ptr       fp = type->variant.class_struct_union.field_list;
        a_boolean         is_static_init_list;
        set_constant_kind(con, (a_constant_repr_kind)ck_aggregate);
        /* Add direct base sub-object constants first. */
        for (bcp = base_classes_of(type); bcp != NULL; bcp = bcp->next) {
          a_byte_count    offset;
          a_constant_ptr  cp;
          if (!bcp->direct || bcp->is_virtual) continue;
          get_mapped_byte_count(&persistent_map, bcp, offset);
          if (!subobject_is_initialized(object+offset, complete_object) &&
              !bcp->type->variant.class_struct_union.is_empty_class) {
            info_with_pos_type(ec_base_subobject_not_initialized,
                               &ips->position, bcp->type, ips);
            do_constexpr_fail(result);
            break;
          }  /* if */
          cp = alloc_constant((a_constant_repr_kind)ck_error);
          if (!copy_interpreter_object_to_constant(
                        ips, object+offset, complete_object, bcp->type, cp)) {
            do_constexpr_fail(result);
            break;
          }  /* if */
          cp->constant_for_base_class = TRUE;
          cp->constant_for_base_class_from_constexpr_folding = TRUE;
          add_constant_to_aggregate(cp, con, bcp, (a_field_ptr)NULL);
        }  /* for */
        if (!result) break;
        /* Now add the constants for initializable fields. */
        is_static_init_list = class_type_supp(type)->is_initializer_list &&
                              (ips->static_lifetime_init ||
                               ips->permit_address_of_local_temporary) &&
                              !scope_stack_top().in_field_initializer;
        fp = next_alloc_field(fp);
        for (; fp != NULL; fp = next_alloc_field(fp->next)) {
          a_byte_count    offset;
          a_constant_ptr  cp;
          a_type_ptr      ftp;
          if (fp->compiler_generated && !fp->is_anonymous_parent_object) {
            /* Ignore fields generated by prelowering. */
            continue;
          }  /* if */
          get_mapped_byte_count(&persistent_map, fp, offset);
          ftp = skip_typerefs(fp->type);
          if (!subobject_is_initialized(object+offset, complete_object) &&
              !(is_immediate_class_type(ftp) &&
                ftp->variant.class_struct_union.is_empty_class) &&
              !(type_is(ftp, tk_array) && has_any_zero_bound(ftp))) {
            info_with_pos_sym(ec_field_subobject_not_initialized,
                              &ips->position, symbol_for(fp), ips);
            do_constexpr_fail(result);
            break;
          }  /* if */
          if (is_static_init_list && type_is(ftp, tk_pointer)) {
            /* A static-lifetime initializer list.  Make sure the underlying
               array is treated as having a static lifetime also. */
            ((a_constexpr_address*)(object+offset))->flags |=
                                                         CA_LIFETIME_EXTENDED;
            /* The call to copy_interpreter_object below will clear the
               permit_address_of_local_temporary flag.  However, if this is
               the second field of an initializer list (which may be a pointer
               in some implementation, like Microsoft's), the flag should be
               true for that field too. */
            ips->permit_address_of_local_temporary = TRUE;
          }  /* if */
          cp = alloc_constant((a_constant_repr_kind)ck_error);
          if (!copy_interpreter_object_to_constant(
                              ips, object+offset, complete_object, ftp, cp)) {
            result = FALSE;
            break;
          }  /* if */
          add_constant_to_aggregate(cp, con, (a_base_class_ptr)NULL, fp);
        }  /* for */
      }
      break;
    case tk_union:
      /* The resulting constant is a ck_aggregate entry containing an
         optional designator followed by a constant value.  The designator
         is added only if the active field is not the first initializable
         field. */
      { a_field_ptr  fp, afp;
        set_constant_kind(con, (a_constant_repr_kind)ck_aggregate);
        fp = type->variant.class_struct_union.field_list,
        fp = next_alloc_field(fp);
        if (fp == NULL || (afp = (a_field_ptr)*(void**)object) == NULL) {
          /* This should only happen with unions that have no field (other
             than empty anonymous union parent objects), and therefore cannot
             have an active field. */
        } else {
          /* afp describes the active field. */
          a_constant_ptr  elem_con, des_con;
          a_byte_count    offset;
          elem_con = alloc_constant((a_constant_repr_kind)ck_error);
          get_mapped_byte_count(&persistent_map, afp, offset);
          if (!copy_interpreter_object_to_constant(
                                          ips, object+offset, complete_object,
                                          afp->type, elem_con)) {
            do_constexpr_fail(result);
          } else {
            if (fp != afp) {
              /* Add a designator for the active field. */
              des_con = alloc_constant((a_constant_repr_kind)ck_designator);
              des_con->variant.designator.is_field_designator = TRUE;
              des_con->variant.designator.variant.field = afp;
              add_constant_to_aggregate(des_con, con, (a_base_class_ptr)NULL,
                                        (a_field_ptr)NULL);
            }  /* if */
            add_constant_to_aggregate(elem_con, con, (a_base_class_ptr)NULL,
                                      (a_field_ptr)NULL);
          }  /* if */
        }  /* if */
      }
      break;
    case tk_array:
      { a_type_ptr      etp = skip_typerefs(type->variant.array.element_type);
        a_targ_size_t   k, n_elems;
        a_byte_count    elem_size = value_bytes_for_type(ips, etp, &result);
        a_byte          *sub_obj = object;
        n_elems = type->variant.array.variant.number_of_elements;
        if (!result) break;
        set_constant_kind(con, (a_constant_repr_kind)ck_aggregate);
        for (k = 0; k<n_elems; k += 1, sub_obj += elem_size) {
          a_constant_ptr  elem_con;
          elem_con = alloc_constant((a_constant_repr_kind)ck_error);
          if (!copy_interpreter_object_to_constant(
                              ips, sub_obj, complete_object, etp, elem_con)) {
            do_constexpr_fail(result);
            break;
          }  /* if */
          add_constant_to_aggregate(elem_con, con, (a_base_class_ptr)NULL,
                                    (a_field_ptr)NULL);
        }  /* for */
      }
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      { a_type_ptr      etp = skip_typerefs(type->variant.vector.element_type);
        a_targ_size_t   k, n_elems = type->size/etp->size;
        a_byte_count    elem_size = value_bytes_for_type(ips, etp, &result);
        a_byte          *sub_obj = object;
        if (!result) break;
        set_constant_kind(con, (a_constant_repr_kind)ck_aggregate);
        for (k = 0; k<n_elems; k += 1, sub_obj += elem_size) {
          a_constant_ptr  elem_con;
          elem_con = alloc_constant((a_constant_repr_kind)ck_error);
          if (!copy_interpreter_object_to_constant(
                              ips, sub_obj, complete_object, etp, elem_con)) {
            do_constexpr_fail(result);
            break;
          }  /* if */
          add_constant_to_aggregate(elem_con, con, (a_base_class_ptr)NULL,
                                    (a_field_ptr)NULL);
        }  /* for */
      }
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tk_nullptr:
      set_constant_kind(con, (a_constant_repr_kind)ck_integer);
      con->variant.integer_value = zero_int;
      con->implicit_cast = TRUE;
      break;
    case tk_void:
      set_constant_kind(con, (a_constant_repr_kind)ck_void);
      break;
    case tk_routine:
      { a_constexpr_address *cap = (a_constexpr_address *)object;
        check_assertion(is_function_address(cap));
        set_routine_address_constant(cap->variant.routine, con,
                                     /*set_address_taken_flag=*/TRUE);
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* copy_interpreter_object_to_constant */


a_boolean is_core_constant_expr(an_expr_node_ptr  expr,
                                a_diag_list_ptr   diag_list)
/*
Return TRUE if the given expression is a "core constant expression".  
Otherwise, return FALSE, and record diagnostic info in *diag_list.
This is very similar to interpret_expr below, but the result of the evaluation
in terms of interpreter values is not copied back to an IL representation (that
copy would diagnose cases that are core constant expressions but not "constant
expressions").
*/
{
  a_boolean             result = TRUE;
  an_interpreter_state  ips;
  a_byte                *result_storage;
  a_byte_count          n_bytes;
  a_type_ptr            result_type = skip_typerefs(expr->type);

  if (is_constant_node(expr)) {
    /* The expression is already a constant. */
    goto done;
  }  /* if */
  if (!in_front_end
#if DO_IL_LOWERING
      || il_lowering_underway
#endif /* DO_IL_LOWERING */
                             ) {
    result = FALSE;
    goto done;
  }  /* if */
  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips, /*is_constant_evaluated=*/FALSE);
  ips.position = expr->position;
  n_bytes = expr_result_size(&ips, expr, result_type, &result); 
  if (!result) {
    if (ips.input_error) {
      /* Interpretation failed due to an error node in the IL.  Continue
         with an error constant, but treat interpretation as successful. */
      result = TRUE;
    }  /* if */
    /* Nothing more to be done. */
  } else {
    alloc_complete_object(&ips, n_bytes, result_type, result_storage);
    if (!do_constexpr_expression(&ips, expr, result_storage, result_storage)) {
      if (ips.input_error) {
        /* Interpretation failed due to an error node in the IL.  Continue
           with an error constant, but treat interpretation as successful. */
      } else {
        do_constexpr_fail(result);
      }  /* if */
    }  /* if */
  }  /* if */
  *diag_list = ips.diag_list;
  release_interpreter_state(&ips);
done:
  return result;
}  /* is_core_constant_expr */


a_boolean interpret_expr(an_expr_node_ptr  expr,
                         a_boolean         is_constant_evaluated,
                         a_boolean         force_prvalue,
                         a_constant_ptr    result_con,
                         a_diag_list_ptr   diag_list)
/*
Attempt to interpret the given expression.  If force_prvalue is TRUE and expr
is a glvalue, convert the glvalue result to a prvalue.  Return TRUE if
successful, and produce the resulting value in result_con.  Otherwise, return
FALSE, and record diagnostic info in *diag_list.  is_constant_evaluated
indicates the value produced by std::is_constant_evaluated().
*/
{
  a_boolean             result = TRUE;
  an_interpreter_state  ips;
  a_byte                *result_storage;
  a_byte_count          n_bytes;
  a_type_ptr            result_type = expr->type,
                        val_type = skip_typerefs(result_type);

  if (is_constant_node(expr)) {
    a_constant_ptr  expr_con = node_constant(expr);
    if (constant_is(expr_con, ck_template_param)) {
      /* Do not return a copy of a template-dependent constant since it
         requires substitution before deciding that it is an actual constant
         value.  (Also, it may have associated rescan info that would not be
         equivalent in the copy.) */
    } else if (constant_is(expr_con, ck_ptr_to_member) &&
               expr_con->variant.ptr_to_member.is_function_ptr &&
               expr_con->variant.ptr_to_member.variant.routine->is_consteval) {
      /* Don't treat a reference to a consteval function as a constant unless
         a constant is really needed.  That keeps the enk_routine node in the
         expression tree so invalid uses can be diagnosed. */
    } else {
      (void)copy_constant_full(expr_con, result_con,
                               CE_COPYING_FOR_CONSTEXPR_MASTER_EXPR);
      goto done;
    }  /* if */
  }  /* if */
  if (!in_front_end
#if DO_IL_LOWERING
      || il_lowering_underway
#endif /* DO_IL_LOWERING */
                             ) {
    result = FALSE;
    goto done;
  }  /* if */
  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips, is_constant_evaluated);
  ips.position = expr->position;
  n_bytes = expr_result_size(&ips, expr, val_type, &result); 
  if (!result) {
    if (ips.input_error) {
      /* Interpretation failed due to an error node in the IL.  Continue
         with an error constant, but treat interpretation as successful. */
      set_error_constant(result_con);
      result = TRUE;
    }  /* if */
    /* Nothing more to be done. */
  } else {
    alloc_complete_object(&ips, n_bytes, val_type, result_storage);
    if (!do_constexpr_expression(&ips, expr, result_storage, result_storage)) {
      if (ips.input_error) {
        /* Interpretation failed due to an error node in the IL.  Continue
           with an error constant, but treat interpretation as successful. */
        set_error_constant(result_con);
      } else {
        do_constexpr_fail(result);
      }  /* if */
    } else {
      if (expr->is_lvalue || expr->is_xvalue) {
        a_constexpr_address  *cap = (a_constexpr_address*)result_storage;
        if (force_prvalue) {
          if (is_runtime_data_address(cap)) {
            if (is_immediate_class_type(val_type) &&
                val_type->variant.class_struct_union.is_empty_class &&
                is_trivially_copy_constructible_type(val_type)) {
              /* An empty class with no actual data to copy: Just allocate
                 an empty object. */
              n_bytes = value_bytes_for_type(&ips, val_type, &result);
              check_assertion(result);
              alloc_complete_object(&ips, n_bytes, val_type,
                                    result_storage);
              init_subobject_to_zero(&ips, result_storage, val_type,
                                     result_storage);
            } else {
              info_with_pos(ec_constexpr_access_to_runtime_storage,
                            &expr->position, &ips);
              do_constexpr_fail(result);
            }  /* if */
          } else if (is_volatile_qualified_type(expr->type)) {
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_volatile_fetch, &expr->position,
                            &ips);
          } else {
            /* result_storage points to an interpreter address for the glvalue.
               Allocate a new object for the corresponding prvalue and perform
               the glvalue-to-prvalue conversion into it. */
            n_bytes = value_bytes_for_type(&ips, val_type, &result);
            check_assertion(result);
            alloc_complete_object(&ips, n_bytes, val_type, result_storage);
            result = do_glvalue_to_prvalue(&ips, expr, val_type, cap,
                                           n_bytes, result_storage,
                                           result_storage);
          }  /* if */
        } else {
          val_type = expr->is_xvalue ?
                                     make_rvalue_reference_type(result_type) :
                                     make_reference_type(result_type);
          result_type = val_type;
        }  /* if */
      }  /* if */
      if (!result) {
        /* Nothing more to do. */
      } else if (ips.storage_stack.destructions != NULL &&
                 ((!node_is(expr, enk_object_lifetime) &&
                   !is_constant_evaluated) ||
                  !perform_destructions(&ips))) {
        /* If there are pending destructions, but this node is not an
           enk_object_lifetime entry, expr doesn't represent a full expression
           and therefore we shouldn't attempt to evaluate the destruction of
           temporaries yet.  If we do have a full expression, ensure that the
           destruction interpretation succeeds. */
        result = FALSE;
      } else if (ips.dyn_allocations != NULL) {
        /* Leftover dynamic allocations are always invalid in this case. */
        report_leftover_allocations(&ips);
        do_constexpr_fail(result);
      } else if (!copy_interpreter_object_to_constant(
                                         &ips, result_storage, result_storage,
                                         result_type, result_con)) {
        do_constexpr_fail(result);
      } else if (expr->next == NULL &&
                 (expr_stack == NULL ||
                  curr_expr_kind_is_one_in_which_const_exprs_are_recorded())) {
        /* If expr is part of an expression list and followed by other
           expressions, do not record it as the backing expression since
           it could cause IL traversal problems later on. */
        result_con->expr = expr;
      }  /* if */
    }  /* if */
  }  /* if */
  *diag_list = ips.diag_list;
  release_interpreter_state(&ips);
done:
  return result;
}  /* interpret_expr */


a_routine_ptr get_constexpr_callee(an_expr_node_ptr  call_expr,
                                   a_diag_list_ptr   diag_list)
/*
Attempt to interpret the call represented by call_expr until the actual callee
is determined (except for virtual dispatch).  If successful, return the IL
entry for that callee; otherwise, return NULL and record diagnostic info in
*diag_list.  The interpretation is done assuming the callee is a consteval
function (i.e., std::is_constant_evaluated() produces TRUE).
*/
{
  a_routine_ptr           callee = NULL;
  an_interpreter_state    ips;
  a_constexpr_ptr_to_mem  *pm_target = NULL;
  a_byte                  *pre_evaluated_this_bytes = NULL;

  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips, /*is_constant_evaluated=*/TRUE);
  ips.allow_consteval_routine_node = TRUE;
  ips.position = call_expr->position;
  callee = eval_constexpr_callee(&ips, call_expr, &pm_target,
                                 &pre_evaluated_this_bytes);
  *diag_list = ips.diag_list;
  release_interpreter_state(&ips);
  return callee;
}  /* get_constexpr_callee */


a_boolean interpret_constexpr_call(an_expr_node_ptr  call_expr,
                                   a_boolean         is_constant_evaluated,
                                   a_constant_ptr    result_con,
                                   a_diag_list_ptr   diag_list)
/*
Attempt to interpret the call represented by call_expr.  Return TRUE if
successful, and produce the resulting value in result_con.  Otherwise, return
FALSE, and record diagnostic info in *diag_list.  is_constant_evaluated
indicates the value produced by std::is_constant_evaluated() (currently, this
can only be TRUE if the called function is "consteval").
*/
{
  a_boolean             result = TRUE;
  an_interpreter_state  ips;
  a_byte                *result_storage;
  a_byte_count          n_bytes;
  a_type_ptr            result_type = skip_typerefs(call_expr->type);

  if (!in_front_end
#if DO_IL_LOWERING
      || il_lowering_underway
#endif /* DO_IL_LOWERING */
                             ) {
    result = FALSE;
    goto done;
  }  /* if */
  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips, is_constant_evaluated);
  if (is_constant_evaluated) {
    ips.allow_consteval_routine_node = TRUE;
  }  /* if */
  ips.position = call_expr->position;
  n_bytes = expr_result_size(&ips, call_expr, result_type, &result); 
  if (!result) {
    if (ips.input_error) {
      /* Interpretation failed due to an error node in the IL.  Continue
         with an error constant, but treat interpretation as successful. */
      set_error_constant(result_con);
      result = TRUE;
    }  /* if */
    /* Nothing more to be done. */
  } else {
    alloc_complete_object(&ips, n_bytes, result_type, result_storage);
    result_con->type = result_type;
    if (!do_constexpr_call(&ips, call_expr, result_storage, result_storage)) {
      if (ips.input_error) {
        /* Interpretation failed due to an error node in the IL.  Continue
           with an error constant, but treat interpretation as successful. */
        set_error_constant(result_con);
      } else {
        do_constexpr_fail(result);
      }  /* if */
    } else if (ips.storage_stack.destructions != NULL ||
               (!is_constant_evaluated &&
                is_immediate_class_type(result_type) &&
                has_nontrivial_destructor(
                               class_symbol_supp(symbol_for(result_type))))) {
      /* Since we're just interpreting a call node, this isn't a full
         expression and we cannot evaluate the destruction of temporaries.
         (An exception are invocations of consteval functions, but the
         temporaries for those were handled by do_constexpr_call.  If there
         are destructions left, they are not part of the consteval invocation
         proper.) */
      result = FALSE;
    } else if (ips.dyn_allocations != NULL) {
      /* Leftover dynamic allocations are always invalid in this case. */
      report_leftover_allocations(&ips);
      do_constexpr_fail(result);
    } else if (!copy_interpreter_object_to_constant(
                                         &ips, result_storage, result_storage,
                                         result_type, result_con)) {
      do_constexpr_fail(result);
    } else if (call_expr->next == NULL &&
               (expr_stack == NULL ||
                curr_expr_kind_is_one_in_which_const_exprs_are_recorded())) {
      /* If call_expr is part of an expression list and followed by other
         expressions, do not record it as the backing expression since
         it could cause IL traversal problems later on. */
      result_con->expr = call_expr;
    }  /* if */
  }  /* if */
  *diag_list = ips.diag_list;
  release_interpreter_state(&ips);
done:
  return result;
}  /* interpret_constexpr_call */


static a_boolean interpret_dynamic_sub_initializers(
                                      a_constant_ptr   aggr_con,
                                      a_boolean        allow_reinterpret_cast,
                                      a_diag_list_ptr  diag_list)
/*
aggr_con is a ck_aggregate constant that might directly or indirectly contain
ck_dynamic_init elements.  Attempt to interpret those elements to produce an
aggregate with no dynamic components and return TRUE if successful.  If not
successful, some of the dynamic components may nonetheless have been folded.
See interpret_dynamic_init_full for the meaning of the diag_list and
allow_reinterpret_cast parameters.
*/
{
  a_boolean       result = TRUE;
  a_constant_ptr  elem_con;

  check_assertion(constant_is(aggr_con, ck_aggregate));
  elem_con = aggr_con->variant.aggregate.first_constant;
  for (; elem_con != NULL; elem_con = elem_con->next) {
    if (constant_is(elem_con, ck_dynamic_init)) {
      a_constant_ptr  cp = local_constant();
      if (!interpret_dynamic_init_full(elem_con->variant.dynamic_init.ptr,
                                       &elem_con->source_corresp.decl_position,
                                       elem_con->type,
                                       /*is_constant_evaluated=*/TRUE, cp,
                                       diag_list, allow_reinterpret_cast)) {
        result = FALSE;
        release_local_constant(&cp);
        break;
      } else {
        a_constant_ptr  saved_next = elem_con->next;
        *elem_con = *cp;
        elem_con->next = saved_next;
        release_local_constant(&cp);
      }  /* if */
    } else if (constant_is(elem_con, ck_aggregate) &&
               elem_con->variant.aggregate.has_dynamic_init_component) {
      if (!interpret_dynamic_sub_initializers(
                               elem_con, allow_reinterpret_cast, diag_list)) {
        result = FALSE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  if (result) {
    aggr_con->variant.aggregate.has_dynamic_init_component = FALSE;
  }  /* if */
  return result;
}  /* interpret_dynamic_sub_initializers */


a_boolean interpret_dynamic_init_full(
                                   a_dynamic_init_ptr  dip,
                                   a_source_position   *pos,
                                   a_type_ptr          result_type,
                                   a_boolean           is_constant_evaluated,
                                   a_constant_ptr      result_con,
                                   a_diag_list_ptr     diag_list,
                                   a_boolean           allow_reinterpret_cast)
/*
Attempt to interpret the given dynamic initialization entry.  Return TRUE if
successful, and produce the resulting value (of the given type) in result_con.
Otherwise, return FALSE, and record diagnostic info in *diag_list.  pos is the
source position of the initialization.  is_constant_evaluated indicates the
value produced by std::is_constant_evaluated().  allow_reinterpret_cast is TRUE
if the caller has determined that reinterpret_cast expressions can be folded
(when FALSE, this function can still determine that they can be folded).
*/
{
  a_boolean             result = TRUE;
  an_interpreter_state  ips;
  a_byte                *result_storage;

  if (!in_front_end
#if DO_IL_LOWERING
      || il_lowering_underway
#endif /* DO_IL_LOWERING */
                             ) {
    result = FALSE;
    goto done;
  }  /* if */
  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips, is_constant_evaluated);
  ips.allow_reinterpret_cast = allow_reinterpret_cast;
  ips.position = *pos;
  result_type = skip_typerefs(result_type);
  if (dip->variable != NULL) {
    /* An initializer might refer to the variable it initializes.  E.g.:
          constexpr int * const x[2] = { 0, x[0] };
       That requires the variable to be associated with its interpreter
       representation. */
    a_variable_ptr  vp = dip->variable;
    result_storage = do_constexpr_alloc_variable(&ips, vp, &result);
    if (var_has_static_storage_duration(vp)) {
      ips.static_lifetime_init = TRUE;
      if (!vp->is_constexpr && !vp->declared_constinit) {
        /* For non-constexpr variables, allow some reinterpret_cast constructs
           in "constant expressions".  That causes us to sometimes promote to
           "static initialization" what would otherwise be a "dynamic
           initialization".  For example:
               struct X { int x; };
               static union {
                 char buf[sizeof(X)];
                 int aligner;
               } u;
               X &r = reinterpret_cast<X&>(u.buf); // (1)
           Initialization (1) will be treated as a static initialization.
           In this context, "constinit" variables are treated as "constexpr"
           variables: We don't want to accept "constant-initialized" constinit
           variables whose initializers aren't really constant expressions (in
           the standard sense). */
        ips.allow_reinterpret_cast = TRUE;
      }  /* if */
    }  /* if */
    ips.is_variable_initializer = TRUE;
  } else {
    a_byte_count  n_bytes;
    n_bytes = value_bytes_for_type(&ips, result_type, &result); 
    if (result) {
      alloc_complete_object(&ips, n_bytes, result_type, result_storage);
    }  /* if */
  }  /* if */
  if (!result) {
    if (ips.input_error) {
      /* Interpretation failed due to an error node in the IL.  Continue
         with an error constant, but treat interpretation as successful. */
      set_error_constant(result_con);
      result = TRUE;
    } else if (is_constant_evaluated &&
               dyn_init_is(dip, dik_nonconstant_aggregate)) {
      /* A failure at this point is most likely due to an attempt to fold an
         initializer for an object that's too large for the interpreter.  E.g.:
             struct S { int i, x[10000000]; } = { f(), {} };
         It might still be possible to fold the sub-initializers for such a
         case, however, thereby producing a simple aggregate constant. */
      result = interpret_dynamic_sub_initializers(dip->variant.constant.ptr,
                                                  ips.allow_reinterpret_cast,
                                                  diag_list);
      (void)copy_constant_full(dip->variant.constant.ptr, result_con,
                               CE_COPYING_FOR_CONSTEXPR_MASTER_EXPR);
    }  /* if */
    /* Nothing more to be done. */
  } else {
    result_con->type = result_type;
    if (dyn_init_is(dip, dik_zero)) {
      init_subobject_to_zero(&ips, result_storage, result_type,
                             result_storage);
    } else {
      a_constexpr_address  dst_addr;
      clear_address(&dst_addr, result_storage);
      dst_addr.alloc_seq_number = 1;
      dst_addr.complete_object = result_storage;
      if (!do_constexpr_dynamic_init(&ips, dip, pos, &dst_addr)) {
        if (ips.input_error) {
          /* Interpretation failed due to an error node in the IL.  Continue
             with an error constant, but treat interpretation as successful. */
          set_error_constant(result_con);
        } else {
          do_constexpr_fail(result);
        }  /* if */
      }  /* if */
    }  /* if */
    if (result && !ips.input_error) {
      /* Map the result address (which is the "this" pointer) to a ck_address
         constant, so that copy_interpreter_object_to_constant can turn that
         address back into a ck_address constant entry if needed. */
      a_boolean       class_case = is_immediate_class_type(result_type);
      a_constant_ptr  this_con;
      if (class_case) {
        this_con = local_constant();
        clear_constant(this_con, (a_constant_repr_kind)ck_address);
        this_con->variant.address.kind = (an_address_base_kind)abk_variable;
        if (dip->variable != NULL) {
          this_con->variant.address.variant.variable = dip->variable;
        }  /* if */
        map_stack_bytes(&ips, result_storage, (a_byte*)this_con);
      }  /* if */
      if (!copy_interpreter_object_to_constant(
                                         &ips, result_storage, result_storage,
                                         result_type, result_con)) {
        result = FALSE;
      } else if (ips.storage_stack.destructions != NULL &&
                 (!is_constant_evaluated || !perform_destructions(&ips))) {
        /* If there are pending destructions, but this initialization is not a
           full-expression, we shouldn't attempt to evaluate the destruction of
           temporaries yet.  When is_constant_evaluated is FALSE, folding is
           not required and so we just continue as if it is not a full-
           expression context.  is_constant_evaluated is TRUE in full-
           expression contexts only, and therefore it is safe to attempt the
           destruction of temporaries in that case. */
        result = FALSE;
      } else if (dip->destructor != NULL &&
                 !do_constexpr_dtor(&ips, dip->destructor, pos,
                                    result_storage, result_storage)) {
        if (dip->variable != NULL && dip->variable->declared_constinit &&
            ips.dyn_allocations == NULL && !dyn_init_is(dip, dik_constant) &&
            !dyn_init_is(dip, dik_zero) && !dyn_init_is(dip, dik_none)) {
          /* A constinit variable with nonconstant destruction is acceptable:
             A dynamic initializer entry is still required in that case, but
             we must ensure that it represents constant initialization (i.e.,
             a dik_constant entry). */
          dip->kind = (a_dynamic_init_kind)dik_constant;
          dip->variant.constant.ptr = alloc_unshared_constant(result_con);
          dip->variant.constant.lambda = NULL;
          dip->variant.constant.non_constant = FALSE;
        }  /* if */
        result = FALSE;
      } else if (ips.dyn_allocations != NULL) {
        /* Leftover dynamic allocations are always invalid in this case. */
        report_leftover_allocations(&ips);
        do_constexpr_fail(result);
      } else {
        /* Record a backing expression.  If it is already an expression, we can
           just point straight to that expression in most cases.  An exception
           occurs if dip represents the result of a class rvalue question mark
           or a comma operator, because the expression tree may then point
           back to dip (which therefore cannot be dropped).  For other cases,
           create an enk_initializer node to represent the initialization as
           an expression. */
        if ((dyn_init_is(dip, dik_expression) ||
             dyn_init_is(dip, dik_class_result_via_ctor)) &&
            !(dip->is_result_for_class_rvalue_question_mark ||
              dip->is_result_for_comma_operator) &&
            (curr_il_region_number == file_scope_region_number) ==
                                     in_file_scope(dip->variant.expression)) {
          result_con->expr = dip->variant.expression;
        } else {
          result_con->expr =
                          alloc_expr_node((an_expr_node_kind)enk_initializer);
          result_con->expr->type = result_type;
          result_con->expr->position = *pos;
          result_con->expr->variant.initializer.dyn_init = dip;
        }  /* if */
        /* Transfer some properties from the dynamic init entry to the constant
           representation. */
        if (dip->is_explicit_cast) {
          result_con->explicit_cast_applied = TRUE;
        }  /* if */
      }  /* if */
      if (class_case) {
        unmap_stack_bytes(&ips, result_storage);
        release_local_constant(&this_con);
      }  /* if */
    }  /* if */
  }  /* if */
  *diag_list = ips.diag_list;
  release_interpreter_state(&ips);
done:
  return result;
}  /* interpret_dynamic_init_full */


a_boolean interpret_constexpr_ctor(a_dynamic_init_ptr  dip,
                                   a_boolean           is_constant_evaluated,
                                   a_source_position   *pos,
                                   a_constant_ptr      result_con,
                                   a_diag_list_ptr     diag_list)
/*
Attempt to interpret the constructor call represented by dip.  Return TRUE if
successful, and produce the resulting value in result_con.  Otherwise, return
FALSE, and record diagnostic info in *diag_list.  is_constant_evaluated
indicates the value produced by std::is_constant_evaluated().  pos is the
position associated with the call.
*/
{
  a_boolean             result = TRUE;
  a_routine_ptr         ctor;
  an_interpreter_state  ips;
  a_byte                *result_storage;
  a_byte_count          n_bytes;
  a_type_ptr            result_type;

  if (!in_front_end
#if DO_IL_LOWERING
      || il_lowering_underway
#endif /* DO_IL_LOWERING */
                             ) {
    result = FALSE;
    goto done;
  }  /* if */
  if (is_error_dynamic_init(dip)) {
    set_error_constant(result_con);
    goto done;
  }  /* if */
  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips, is_constant_evaluated);
  if (is_constant_evaluated) {
    ips.allow_consteval_routine_node = TRUE;
  }  /* if */
  ips.position = *pos;
  if (dip->variant.constructor.is_copy_constructor_with_implied_source) {
    /* An implied source is never constant. */
    more_info_diagnostic(ec_constexpr_implied_source_nonconstant, pos,
                         &ips.diag_list);
    do_constexpr_fail(result);
  }  /* if */
  if (result) {
    ctor = dip->variant.constructor.ptr;
    result_type = parent_class_of(ctor);
    n_bytes = value_bytes_for_type(&ips, result_type, &result); 
  }  /* if */
  if (!result) {
    if (ips.input_error) {
      /* Interpretation failed due to an error node in the IL.  Continue
         with an error constant, but treat interpretation as successful. */
      set_error_constant(result_con);
      result = TRUE;
    }  /* if */
    /* Nothing more to be done. */
  } else {
    if (dip->static_temp &&
        is_immediate_class_type(result_type) &&
        class_type_supp(result_type)->is_initializer_list) {
      /* When constructing a static std::initializer_list temporary object,
         the embedded pointer can point to a temporary if it is "local"
         (because it will be a static array). */
      ips.permit_address_of_local_temporary = TRUE;
    }  /* if */
    alloc_complete_object(&ips, n_bytes, result_type, result_storage);
    a_constexpr_address  dst_addr;
    clear_address(&dst_addr, result_storage);
    dst_addr.alloc_seq_number = 1;
    dst_addr.complete_object = result_storage;
    if (!do_constexpr_ctor(&ips, dip, pos, &dst_addr, /*implied_src=*/NULL)) {
      if (ips.input_error) {
        /* Interpretation failed due to an error node in the IL.  Continue
           with an error constant, but treat interpretation as successful. */
        set_error_constant(result_con);
      } else {
        do_constexpr_fail(result);
      }  /* if */
    } else if (ips.storage_stack.destructions != NULL &&
               (!is_constant_evaluated || !perform_destructions(&ips))) {
      /* If there are pending destructions, but this initialization is not a
         full-expression, we shouldn't attempt to evaluate the destruction of
         temporaries yet.  When is_constant_evaluated is FALSE, folding is not
         required and so we just continue as if it is not a full-expression
         context.  is_constant_evaluated is TRUE in full-expression contexts
         only, and therefore it is safe to attempt the destruction of
         temporaries in that case. */
      result = FALSE;
    } else if (ips.dyn_allocations != NULL) {
      /* Leftover dynamic allocations are always invalid in this case. */
      report_leftover_allocations(&ips);
      do_constexpr_fail(result);
    } else {
      /* Map the result address (which is the "this" pointer) to a ck_address
         constant, so that copy_interpreter_object_to_constant can turn that
         address back into a ck_address constant entry if needed. */
      a_constant_ptr  this_con = local_constant();
      clear_constant(this_con, (a_constant_repr_kind)ck_address);
      this_con->variant.address.kind = (an_address_base_kind)abk_variable;
      if (dip->variable != NULL) {
        this_con->variant.address.variant.variable = dip->variable;
      }  /* if */
      map_stack_bytes(&ips, result_storage, (a_byte*)this_con);
      if (!copy_interpreter_object_to_constant(
             &ips, result_storage, result_storage, result_type, result_con)) {
        result = FALSE;
      }  /* if */
      unmap_stack_bytes(&ips, result_storage);
      release_local_constant(&this_con);
    }  /* if */
    if (ips.storage_stack.destructions != NULL &&
        (!is_constant_evaluated || !perform_destructions(&ips))) {
      /* If there are pending destructions, but this initialization is not a
         full-expression, we shouldn't attempt to evaluate the destruction of
         temporaries yet.  When is_constant_evaluated is FALSE, folding is not
         required and so we just continue as if it is not a full-expression
         context.  is_constant_evaluated is TRUE in full-expression contexts
         only, and therefore it is safe to attempt the destruction of
         temporaries in that case. */
      result = FALSE;
    } else if (result && ips.dyn_allocations != NULL) {
      report_leftover_allocations(&ips);
      do_constexpr_fail(result);
    }  /* if */
  }  /* if */
  *diag_list = ips.diag_list;
  release_interpreter_state(&ips);
done:
  return result;
}  /* interpret_constexpr_ctor */


void clean_up_interpreter(void)
/*
Release the persistent storage stack and the persistent map (making that
storage available for another compilation, if any).
*/
{
  if (!trans_unit_initialization_needed) {
    a_byte  *large_blocks = persistent_data.large_blocks;
    /* First release any large blocks. */
    while (large_blocks != NULL) {
      a_byte  *large_block = large_blocks;
      large_blocks = ((a_large_block_header*)large_block)->prev_large_block;
      free_general(large_block,
                 ((a_large_block_header*)large_block)->block_size);
    }  /* while */
    persistent_data.large_blocks = NULL;
    /* Now release the storage stack itself. */
    release_constexpr_stack(&persistent_data);
    /* Release the persistent map. */
    release_data_map_table(&persistent_map);
    /* Clear pointer to stack blocks (they are allocated in front end memory,
       which is about to be reclaimed). */
    free_stack_blocks = NULL;
  }  /* if */
}  /* clean_up_interpreter */


void register_constexpr_intrinsic(a_constexpr_intrinsic  tag,
                                  a_routine_ptr          rp)
/*
Mark the given routine as a "constexpr intrinsic" (i.e., a function that is
treated specially when invoked at compile time) and associate it with the given
tag in the persistent map.  (An example of such a function is
std::is_constant_evaluated.)
*/
{
  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  rp->is_constexpr_intrinsic = TRUE;
  rp->number.constexpr_intrinsic = (int32_t)tag;
}  /* register_constexpr_intrinsic */

#if DEBUG

unsigned long db_show_interpret_fe_space_used(unsigned long  grand_total)
/*
Display memory use for entities in front end memory in this file (interpret.c).
*/
{
  int            k;
  unsigned long  num, size, total;

  /* Report map tables: */
  for (k = 0; k < MAX_WIDTH_REUSABLE_TABLE; ++k) {
    if (free_map_tables[k] != NULL) {
      char              name[40];
      a_data_map_entry  *table = free_map_tables[k];
      unsigned long     cnt = 1, table_size;
      while (table->ptr != NULL) {
        cnt += 1;
        table = (a_data_map_entry*)table->ptr;
      }  /* if */
      sprintf(name, "data map table width %d", k);
      table_size = sizeof(an_alloc_seq_number)*((sizeof_t)1<<k);
      db_space_used_nontype(name, cnt, table_size);
    }  /* if */
  }  /* for */
  /* Report live set tables: */
  for (k = 0; k < MAX_WIDTH_REUSABLE_TABLE; ++k) {
    if (free_live_set_tables[k] != NULL) {
      char                 name[40];
      an_alloc_seq_number  *table = free_live_set_tables[k];
      unsigned long        cnt = 1, table_size;
      while (*(an_alloc_seq_number**)table != NULL) {
        cnt += 1;
        table = *(an_alloc_seq_number**)table;
      }  /* if */
      sprintf(name, "live set table width %d", k);
      table_size = sizeof(an_alloc_seq_number)*((sizeof_t)1<<k);
      db_space_used_nontype(name, cnt, table_size);
    }  /* if */
  }  /* for */
  return grand_total;
}  /* db_show_interpret_fe_space_used */

#endif /* DEBUG */

void interpret_trans_unit_init(void)
/*
Initialize static variables related to the interpreter.  These are variables
that need initialization for every (primary and secondary) translation unit.
*/
{
  trans_unit_initialization_needed = TRUE;
}  /* interpret_trans_unit_init */


void interpret_init(void)
/*
Initialize static variables that need to be reset for every compilation.
*/
{
  memzero((char*)free_map_tables, sizeof(free_map_tables));
  memzero((char*)free_live_set_tables, sizeof(free_live_set_tables));
  useful_constants_initialized = FALSE;
}  /* interpret_init */


void interpret_one_time_init(void)
/*
One-time initialization for interpret.c static variables.
*/
{
  /* Static variables in interpret.c. */
  register_trans_unit_variable(trans_unit_initialization_needed);
  register_trans_unit_variable(persistent_data);
  register_trans_unit_variable(persistent_map);
  register_trans_unit_variable(variant_path_entries);
  register_trans_unit_variable(free_variant_path_entries);
  register_trans_unit_variable(n_variant_path_entries);
  register_trans_unit_variable(n_free_variant_path_entries);
  register_trans_unit_variable(valid_placement_new_address);
  register_trans_unit_variable(valid_placement_new_type);
  useful_constants_initialized = FALSE;
  free_stack_blocks = NULL;
  free_variant_path_entries = NULL;
#if DEBUG && TRACK_INTERPRETER_ALLOCATIONS
  object_alloc_to_intercept = 0;
#endif /* DEBUG && TRACK_INTERPRETER_ALLOCATIONS */
}  /* interpret_one_time_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
