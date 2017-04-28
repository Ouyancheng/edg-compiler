/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015-2017 Edison Design Group Inc.                   [_]          *
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

#include "templates.h"

/*
This file implements an interpreter for a subset of the unlowered IL produced
by the C++ front end.  Specifically, the subset corresponds to the constructs
allowed in a constant expression in C++14.


The Interpreter
---------------
The interpreter itself traverses the IL in typical "recursive descent" fashion.
The principal entry points are interpret_expr, interpret_constexpr_call,
interpret_dynamic_init, and interpret_constexpr_ctor.  These set up an
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
  init_flag
  type_ptr
  (data representation starts here)

Given a data pointer, traversing the prefix backwards provides the type of the
complete object (type_ptr), a byte (init_flag) that is zero until the object
is fully initialized, and, for class and array objects, bit sets indicating
whether a given offset in the data representation has been initialized.  Note
that only one bit is set per scalar entity.  For example, if integers are
represented using eight bytes and the leading entry of an array is initialized,
then bit 0 of init_bits[0] will be set, but bits 1 through 7 will remain
unchanged even though their corresponding bytes have valid values.

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

typedef unsigned int an_alloc_seq_number;

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

static unsigned long count_ones(unsigned long n)
/*
Return the number of trailing "ones" in the binary representation of n.
*/
{
  unsigned long r = 0;

  while (n != 0) {
    if (n&1) {
      ++r;
    }  /* if */
    n >>= 1;
  }  /* while */
  return r;
}  /* count_ones */

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
Structure describing a data map.  (Implemented has a hash table with linear
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
  a_byte_count  size = n_slots*sizeof(a_data_map_entry);

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
  a_byte_count      size = n_slots*sizeof(a_data_map_entry);
  unsigned long     mask_width = count_ones(mask);

  if (mask_width > MAX_WIDTH_REUSABLE_TABLE) {
    free_general(map->table, size);
  } else {
    map->table[0].ptr = (a_byte*)free_map_tables[mask_width];
    free_map_tables[mask_width] = map->table;
  }  /* if */
}  /* release_data_map_table */


/*
Structure describing a call context.
*/
typedef struct a_call_frame *a_call_frame_ptr;
typedef struct a_call_frame {
  a_call_frame_ptr
		parent;
			/* The frame in which this frame was created. */
  a_routine_ptr	routine;
			/* The routine being called. */
  a_source_position
		*position;
			/* The source position of the call. */
  a_byte	*result_storage;
			/* The storage in which returned expression results
			   should be placed. */
  a_byte	*complete_object;
			/* A pointer to the complete object in which
			   result_storage points. */
  a_bit_field	return_active:1;
			/* TRUE while backtracking from a return statement. */
  a_bit_field	break_active:1;
			/* TRUE while backtracking from a break statement. */
  a_bit_field	continue_active:1;
			/* TRUE while backtracking from a continue
			   statement. */
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
  a_byte_count  size = n_slots*sizeof(an_alloc_seq_number);

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
  a_byte_count      size = n_slots*sizeof(an_alloc_seq_number);
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
  a_byte_count         old_size = n_slots*sizeof(an_alloc_seq_number);
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
  while (table[idx] != alloc_seq) {                                          \
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
			/* Hash table mapping pointers into the IL onto
			   associated data. */
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
  a_storage_stack_state
		static_storage;
			/* Pointer to the storage stack state used to allocate
			   static storage-duration variables.  Initialized
			   when first needed. */
} an_interpreter_state;


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
  sss->top = sss->curr_block+3*ptr_size;
  sss->large_blocks = NULL;
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
  *(a_byte**)(sss->curr_block+2*ptr_size) = sss->top;
  next_block = *(a_byte**)(sss->curr_block+ptr_size);
  if (next_block == NULL) {
    /* The current block is the last block: Allocate a new one. */
    alloc_constexpr_stack_block(sss);
  } else {
    /* Reuse a previously-allocated block. */
    sss->curr_block = next_block;
    /* Leave space for the bookkeeping information (three pointers). */
    sss->top = sss->curr_block+3*ptr_size;
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
}


#define restore_storage_stack(ips, state)                                    \
{                                                                            \
  a_byte  *curr_large_blocks = (ips)->storage_stack.large_blocks;            \
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
#define get_int_val_from(bytes, tp, val, ovfl)                                \
  conv_integer_value_to_host_large_integer(                                   \
                          (an_integer_value *)(bytes),                        \
                          int_kind_is_signed[(tp)->variant.integer.int_kind], \
                          &(val), &(ovfl))


/*
Macros to push and pop call frames.
*/
#define push_call_frame(ips, p_frame, rp, pos, p_result, p_complete)         \
  {                                                                          \
    (p_frame)->parent = (ips)->curr_call_frame;                              \
    (p_frame)->routine = (rp);                                               \
    (p_frame)->position = (pos);                                             \
    (p_frame)->result_storage = (p_result);                                  \
    (p_frame)->complete_object = (p_complete);                               \
    (p_frame)->return_active = FALSE;                                        \
    (p_frame)->break_active = FALSE;                                         \
    (p_frame)->continue_active = FALSE;                                      \
    (ips)->curr_call_frame = (p_frame);                                      \
  }

#define pop_call_frame(ips)                                                  \
  ((ips)->curr_call_frame = (ips)->curr_call_frame->parent)


#if HOST_ALIGNMENT_REQUIRED == 1
#define HASH_PTR_SHIFT 0
#else /* HOST_ALIGNMENT_REQUIRED > 1 */
#if HOST_ALIGNMENT_REQUIRED == 2
#define HASH_PTR_SHIFT 1
#else /* HOST_ALIGNMENT_REQUIRED > 2 */
#if HOST_ALIGNMENT_REQUIRED == 4
#define HASH_PTR_SHIFT 2
#else /* HOST_ALIGNMENT_REQUIRED > 4 */
#if HOST_ALIGNMENT_REQUIRED == 8
#define HASH_PTR_SHIFT 3
#else /* HOST_ALIGNMENT_REQUIRED > 8 */
#if HOST_ALIGNMENT_REQUIRED == 16
#define HASH_PTR_SHIFT 4
#else /* HOST_ALIGNMENT_REQUIRED > 16 */
#if HOST_ALIGNMENT_REQUIRED == 32
#define HASH_PTR_SHIFT 5
#else /* HOST_ALIGNMENT_REQUIRED > 32 */
#define HASH_PTR_SHIFT 6
#endif /* == 32 */
#endif /* == 16 */
#endif /* == 8 */
#endif /* == 4 */
#endif /* == 2 */
#endif /* == 1 */

#define hash_ptr(ptr)                                                        \
   ((uintptr_t)ptr >> HASH_PTR_SHIFT)


/*
Macro to retrieve a pointer (dptr) associated with a pointer (iptr) from a
given data map.
*/
#define get_mapped_ptr(map, iptr, dptr)                                      \
{                                                                            \
  uintptr_t         hash = hash_ptr(iptr);                                   \
  a_map_index       mask = (map)->hash_mask;                                 \
  a_map_index       idx = hash & mask;                                       \
  a_data_map_entry  *table = (map)->table;                                   \
  for (;;) {                                                                 \
    a_byte  *tptr = table[idx].ptr;                                          \
    if (tptr == (a_byte*)(iptr)) {                                           \
      (dptr) = table[idx].data.ptr;                                          \
      break;                                                                 \
    } else if (tptr == NULL) {                                               \
      (dptr) = NULL;                                                         \
      break;                                                                 \
    }  /* if */                                                              \
    idx = (idx+1) & mask;                                                    \
  }  /* for */                                                               \
}


/*
Macro to retrieve a byte count (bcount) associated with a pointer (iptr)
from a given data map.
*/
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

void db_data_map(void  *map_ptr)
/*
Output some information about a data map's contents
*/
{
  a_data_map        *map = (a_data_map*)map_ptr;
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
              (a_map_index)hash_ptr(ptr) & mask, ptr);
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
  a_byte_count      old_size = n_slots*sizeof(a_data_map_entry);
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
                              a_data_map_entry  new_entry,
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
Useful constants.
*/
static an_integer_value
		zero_int;
static an_integer_value
		one_int;
static an_internal_float_value
		zero_flt[(int)fk_last];
static an_internal_float_value
		one_flt[(int)fk_last];
static a_boolean
		useful_constants_initialized;
			/* Flag indicating whether these constants have
			   been initialized yet. */


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
		length: 24;
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
#define MAX_ARRAY_LENGTH ((1<<24) - 1)
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

static void get_runtime_array_pos(a_constexpr_address  *cap,
                                  a_byte_count         elem_size,
                                  a_byte_count         *a_len,
                                  a_byte_count         *p_pos)
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
    /* Presumably a null pointer (an integer with a pointer type). */
    check_assertion(constant_is(con_addr, ck_integer));
    length = 0;
    pos = 0;
  } else if (elem_size == 0) {
    /* In some modes (e.g., GNU C), types can have size zero.  Any length and
       position is a-priori possible. */
    length = MAX_ARRAY_LENGTH;
    pos = 0;
  } else {
    switch(con_addr->variant.address.kind) {
      case abk_variable:
        tp = skip_typerefs(con_addr->variant.address.variant.variable->type);
        /* Ignore incomplete arrays, flexible arrays. */
        if (!tp->incomplete &&
            !(is_immediate_class_type(tp) &&
              tp->variant.class_struct_union.contains_flexible_array_member)) {
          length = (a_byte_count)tp->size/elem_size;
        } else {
          length = MAX_ARRAY_LENGTH;
        }  /* if */
        break;
      case abk_constant:
      case abk_temporary:
        cp = con_addr->variant.address.variant.constant;
        if (constant_is(cp, ck_string)) {
          length = (a_byte_count)cp->variant.string.length/elem_size;
        } else {
          length = (a_byte_count)skip_typerefs(cp->type)->size/elem_size;
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
    pos = (a_byte_count)con_addr->variant.address.offset / elem_size;
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
occurs.
*/
#define get_array_pos(ips, cap, elem_type, a_len, pos, e_size, p_result)     \
{                                                                            \
  if (is_runtime_data_address(cap)) {                                        \
    *(e_size) = (a_byte_count)elem_type->size;                               \
    get_runtime_array_pos(cap, *(e_size), a_len, pos);                       \
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



static void trim_bit_field(a_byte     *storage,
                           unsigned   length,
                           a_boolean  is_signed)
/*
The given storage is that for an integer value representing a bit field of the
given length (the bit field is signed if is_signed is TRUE).  Trim the value
representation to fit in the bit field length.
*/
{
  if (is_signed) {
    sign_extend_integer_value((an_integer_value*)storage, length);
  } else {
    a_boolean         ovflo;
    an_integer_value  mask = one_int;
    shift_left_integer_value(&mask, (int)length, &ovflo);
    subtract_integer_values(&mask, &one_int, /*is_signed=*/FALSE, &ovflo);
    and_integer_values((an_integer_value*)storage, &mask);
  }  /* if */
}  /* trim_bit_field */


#define trim_bit_field_if_needed(addr)                                        \
{                                                                             \
  if (is_bit_field_lvalue(addr)) {                                            \
    unsigned   length = (addr)->length;                                       \
    a_boolean  is_signed_field = (length & 1);                                \
    length = length/2;                                                        \
    trim_bit_field((addr)->address, length, is_signed_field);                 \
  }  /* if */                                                                 \
}


/*
Macro to initialize a constant address at addr referring to the complete
interpreter value at targ_addr (or a null pointer).
*/
#define clear_address(addr, targ_addr)                                   \
  memzero((char *)(addr) /*lint -e668*/, sizeof(a_constexpr_address));   \
  ((a_constexpr_address *)(addr))->address = (targ_addr);                \
  ((a_constexpr_address *)(addr))->complete_object = (targ_addr);
  

/*
Macro to initialize a constant address at addr referring to the function
denoted by the IL a_routine entry rout.
*/
#define make_function_address(addr, rout)                      \
  memzero((char *)(addr), sizeof(a_constexpr_address));        \
  ((a_constexpr_address *)(addr))->flags = CA_FUNCTION;        \
  ((a_constexpr_address *)(addr))->variant.routine = (rout);


/*
Macro to initialize a constant address at addr referring to the
(non-interpreter) constant address described by the ck_address constant con.
*/
#define clear_runtime_constant_address(addr, con)                   \
  memzero((char *)(addr), sizeof(a_constexpr_address));             \
  ((a_constexpr_address *)(addr))->flags = CA_RUNTIME_DATA_ADDRESS; \
  ((a_constexpr_address *)(addr))->variant.addr_con = (con);


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


static void init_interpreter_state(an_interpreter_state  *ips)
/*
Initialize the given interpreter state.
*/
{
  init_data_map(&ips->map, 3);
  init_constexpr_stack(&ips->storage_stack);
  init_live_set(&ips->live_set);
  add_to_live_set(&ips->live_set, 1);
  ips->curr_alloc_seq_number = 1;
  ips->curr_call_frame = NULL;
  ips->extension_state = NULL;
  ips->constants = NULL;
  clear_diag_list(&ips->diag_list);
  ips->position = null_source_position;
  ips->cost = 0;
  ips->static_storage_ready = FALSE;
  ips->side_effects_disabled = !relaxed_constexpr_enabled;
  ips->suspend_diag_list = FALSE;
  ips->input_error = FALSE;
  ips->call_seen = FALSE;
}  /* init_interpreter_state */


static void release_interpreter_state(an_interpreter_state  *ips)
/*
Release the storage allocated for the given interpreter state.
*/
{
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
  if (n_free_variant_path_entries != n_variant_path_entries) {
    reclaim_variant_path_entries();
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
      more_info_diagnostic(ec_constexpr_called_from, frame->position,
                           &ips->diag_list);
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
  a_byte_count  elem_size, pos;
  a_byte        *base_address;
  a_boolean     local_result = TRUE;

  if (is_array_element(addr)) {
    elem_size = value_bytes_for_type(ips, expr->type, &local_result);
    check_assertion(local_result);
    base_address = get_base_address(addr);
    pos = (a_byte_count)(addr->address - base_address) / elem_size;
    info_with_pos_num(ec_constexpr_access_one_past_array_end, &expr->position, 
                      pos, ips);
  } else {
    info_with_pos(ec_constexpr_access_past_object, &expr->position, ips);
  }  /* if */
}  /* info_one_past_end_of_array */


/*
Macro defining the largest allowed size of a type in the interpreter.
*/
#define MAX_CONSTEXPR_TYPE_SIZE ((a_byte_count)(1<<20))


static a_byte_count lay_out_class_type(an_interpreter_state  *ips,
                                       a_type_ptr            tp,
                                       a_boolean             *p_result);
static a_byte_count lay_out_union_type(an_interpreter_state  *ips,
                                       a_type_ptr            tp,
                                       a_boolean             *p_result);


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
      result = sizeof(an_internal_float_value);
      break;
    case tk_routine:
    case tk_pointer:
    case tk_nullptr:
      result = sizeof(a_constexpr_address);
      break;
    case tk_array:
      { a_targ_size_t  n_elems = 1;
        a_type_ptr     etp = tp;
        an_error_code  err_code = ec_no_error;
        do {
          if (etp->variant.array.is_variable_size_array) {
            err_code = ec_constexpr_vla;
            break;
          } else if (etp->variant.array.is_template_dependent_size_array) {
            err_code = ec_constexpr_type_invalid;
            break;
          } else if (etp->variant.array.variant.number_of_elements == 0 &&
                     !etp->variant.array.bound_is_zero) {
            err_code = ec_constexpr_access_to_runtime_storage;
            break;
          } else {
            n_elems *= etp->variant.array.variant.number_of_elements;
            etp = etp->variant.array.element_type;
            etp = skip_typerefs(etp);
          }  /* if */
        } while (etp->kind == (a_type_kind)tk_array);
        if (err_code == ec_no_error) {
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
            result = MAX_CONSTEXPR_TYPE_SIZE+1;
          } else {
            result *= (a_byte_count)n_elems;
          }  /* if */
        } else {
          a_source_position  *pos = &tp->source_corresp.decl_position;
#if DEBUG
          check_assertion(ips != NULL);
#endif /* DEBUG */
          if (pos->seq == 0) pos = &ips->position;
          info_with_pos(err_code, pos, ips);
          do_constexpr_fail(*p_result);
          result = 0;
        }  /* if */
      }  /* if */
      break;
    case tk_class:
    case tk_struct:
      get_mapped_byte_count(&persistent_map, tp, result);
      if (result == 0) {
        if (!tp->incomplete) {
          result = lay_out_class_type(ips, tp, p_result);
        } else {
          a_source_position  *pos = &tp->source_corresp.decl_position;
          info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
          do_constexpr_fail(*p_result);
        }  /* if */
      } else if (result > MAX_CONSTEXPR_TYPE_SIZE) {
        a_source_position  *pos = &tp->source_corresp.decl_position;
        info_with_pos_type(ec_constexpr_incomplete_type, pos, tp, ips);
        do_constexpr_fail(*p_result);
      }  /* if */
      break;
    case tk_union:
      get_mapped_byte_count(&persistent_map, tp, result);
      if (result == 0) {
        if (!tp->incomplete) {
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
#endif /* FIXED_POINT_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
    case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
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
    case tk_error:
      ips->input_error = TRUE;
      /*FALLTHROUGH*/
    case tk_template_param:
    case tk_unknown:
      /* Fail interpretation. */
      info_with_pos_type(ec_constexpr_type_invalid, &ips->position, tp, ips);
      do_constexpr_fail(*p_result);
      result = MAX_CONSTEXPR_TYPE_SIZE+1;
      do_constexpr_fail(*p_result);
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


static a_field_ptr next_alloc_field(a_field_ptr  field)
/*
Given a pointer to a field (or NULL), return a pointer to the first field at
or after the given field that is allocated as such in the interpreter.
Unnamed bit fields, for example, are not allocated, and are skipped by
initialization processing.  If there is no next such field, return NULL.  This
is similar to next_alloc_field except it also skips fields generated
by lowering.
*/
{
  for (; field != NULL; field = field->next) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Skip any property or event fields. */
    if (field->property_or_event_descr != NULL) continue;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Skip fields generated by lowering. */
    if (symbol_for(field) == NULL) continue;
    /* Unnamed bit fields are not initializable. */
    if (has_name(field) || !field->is_bit_field) break;
    /* Anonymous unions are also initializable in C++.  An extension allows
       anonymous parent objects in C too. */
    if (field->is_anonymous_parent_object) break;
  }  /* for */
  return field;
}  /* next_alloc_field */


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
    total_size += value_bytes_for_type(ips, fp->type, p_result);
    if (total_size > MAX_CONSTEXPR_TYPE_SIZE) {
      if (*p_result) {
        a_source_position  *pos = &tp->source_corresp.decl_position;
        if (pos->seq == 0) pos = &ips->position;
        info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
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
        a_source_position  *pos = &tp->source_corresp.decl_position;
        if (pos->seq == 0) pos = &ips->position;
        info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
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
            a_source_position  *pos = &tp->source_corresp.decl_position;
            if (pos->seq == 0) pos = &ips->position;
            info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
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
    a_source_position  *pos = &tp->source_corresp.decl_position;
    if (pos->seq == 0) pos = &ips->position;
    info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
    *p_result = TRUE;
    total_size = MAX_CONSTEXPR_TYPE_SIZE+1;
  }  /* if */
  map_byte_count(&persistent_map, tp, total_size);
done:
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
  if (parent_type->kind != (a_type_kind)tk_union) {
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
    if (offset-sub_offset < value_bytes_for_type(ips, last_bcp->type, &okay)) {
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
  *(void**)(subobj_ptr) = (void*)(bcp);

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
  static a_byte_count  alloc_num = 0;

  if (!map_ready) {
    init_data_map(&object_alloc_map, /*mask_width=*/5U);
    map_ready = TRUE;
  }  /* if */
  alloc_num += 1;
  if (alloc_num == object_alloc_to_intercept) {
    object_alloc_intercept();
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
Given a type utp and the size n_bytes required to represent its value, compute
the number of bytes needed as a bookkeeping prefix for a complete object of
that type.
*/
#define compute_prefix_size_for_type(utp, n_bytes, prefix_size)              \
{                                                                            \
  a_byte_count  bitmap_size;                                                 \
  if (is_immediate_class_type(utp) ||                                        \
      utp->kind == (a_type_kind)tk_array) {                                  \
    bitmap_size = (n_bytes+CHAR_BIT-1)/CHAR_BIT;                             \
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

/*
Mark the complete object at the given address as fully initialized.
*/
#define mark_complete_object_initialized(obj)                                \
  (*((a_byte*)obj-sizeof(a_type_ptr)-1) = 1)

#define mark_subobject_initialized(subobj, complete_obj)                     \
{                                                                            \
  a_byte        *start_byte = (complete_obj);                                \
  a_byte_count  off = (a_byte_count)((subobj)-start_byte);                   \
  a_byte_count  byte_pos = off/CHAR_BIT+sizeof(a_type_ptr)+2;                \
  a_byte_count  bit_pos = off%CHAR_BIT;                                      \
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
    a_byte_count  byte_pos = off/CHAR_BIT+sizeof(a_type_ptr)+2;
    a_byte_count  bit_pos = off%CHAR_BIT;
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
      *fp_value(subobj) = zero_flt[(int)tp->variant.float_kind];
      break;
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
        check_assertion(result);
        for (k = 0; k<n_elems; k += 1) {
          init_subobject_to_zero(ips, subobj, etp, complete_obj);
          subobj += elem_size;
        }  /* for */
      }
      break;
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
}  /* init_subobject_to_zero */

/*
Return TRUE if the given complete object is fully initialized.
*/
#define complete_object_is_initialized(complete_object)                      \
  (*((complete_object)-sizeof(a_type_ptr)-1) != 0)


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
  a_byte_count  byte_pos = off/CHAR_BIT+sizeof(a_type_ptr)+2;
  a_byte_count  bit_pos = off%CHAR_BIT;

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
    for (;;) {
      if (parent_type->kind == (a_type_kind)tk_array) {
        a_type_ptr    etp = skip_typerefs(
                                     parent_type->variant.array.element_type);
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
          parent_addr += idx1*esize;
        }  /* if */
      } else if (is_immediate_class_type(parent_type)) {
        a_field_ptr  fp1, fp2;
        a_base_class_ptr  bcp1, bcp2;
        a_byte_count      offset;
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


void db_object(a_byte      *addr,
               a_type_ptr  tp)
/*
Output the contents of the interpreted object of type tp stored at addr.
*/
{
  static int indent = 0;

  db_indent(indent);
  tp = skip_typerefs(tp);
  switch (tp->kind) {
    case tk_integer:
      { a_host_large_integer  val;
        a_boolean             ovflo;
        conv_integer_value_to_host_large_integer(
                  (an_integer_value *)addr, /*is_signed=*/TRUE, &val, &ovflo);
        (void)fprintf(f_debug, "%ld%s\n", (long)val,
                      ovflo ? " (overflow!)" : "");
      }
      break;
    case tk_float:
      (void)fprintf(f_debug, "%s\n",
                    fp_to_string(tp->variant.float_kind,
                                 (an_internal_float_value*)addr,
                                 /*pos_infinity=*/(a_boolean*)NULL,
                                 /*neg_infinity=*/(a_boolean*)NULL,
                                 /*not_a_number=*/(a_boolean*)NULL));
     
      break;
    case tk_pointer:
      { a_constexpr_address *cap = (a_constexpr_address*)addr;
        (void)fprintf(f_debug, "address %p:\n", cap->address);
        db_indent(indent+2);
        db_address_flags(cap->flags);
        (void)fprintf(f_debug, "\n");
        if (is_array_element(cap)) {
          db_indent(indent+2);
          (void)fprintf(f_debug, "length %u:\n", cap->length);
        }  /* if */
        db_indent(indent+2);
        (void)fprintf(f_debug, "alloc seq# %u:\n", cap->alloc_seq_number);
      }
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
          (void)fprintf(f_debug, "%u:\n", offset/e_bytes);
          db_object(addr+offset, etp);
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
          db_object(addr+offset, fp->type);
        }  /* for */
        /* Output the base class values. */
        for (; bcp != NULL; bcp = bcp->next) {
          if (bcp->direct && !bcp->is_virtual) {
            db_indent(indent);
            (void)fprintf(f_debug, "base ");
            db_type_name(bcp->type);
            get_mapped_byte_count(&persistent_map, bcp, offset);
            (void)fprintf(f_debug, " (offset %u)= \n", offset);
            db_object(addr+offset, bcp->type);
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
        if (fp == NULL) {
          (void)fprintf(f_debug, "no active field\n");
        } else {
          (void)fprintf(f_debug, "active field = %s\n",
                        db_name_str(&fp->source_corresp, iek_none));
          get_mapped_byte_count(&persistent_map, fp, offset);
          (void)fprintf(f_debug, " (offset %u)= \n", offset);
          db_object(addr+offset, fp->type);
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
}  /* db_object */


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

static void make_anon_union_path(a_symbol_ptr              au_sym,
                                 a_variant_path_entry_ptr  *p_last_entry,
                                 a_byte                    **p_addr)
/*
au_sym represents an anonymous union parent field being selected in an object
at address *p_addr.  Update *p_addr to point to the address of the anonymous
union parent object.  au_sym may itself be a member of anonymous enclosing
anonymous unions; if so, append to *p_last_entry a variant path corresponding
to those anonymous union objects.
*/
{
  a_variant_path_entry_ptr  vpep, last_entry;
  a_field_ptr               aufp = au_sym->variant.field.ptr;
  a_symbol_ptr              au_parent;
  a_byte_count              offset;

  au_parent = au_sym->variant.field.anonymous_parent_object;
  if (au_parent != NULL) {
    a_type_ptr  au_type = au_parent->variant.field.ptr->type;
    if (au_type->kind == (a_type_kind)tk_union &&
        !au_type->variant.class_struct_union.is_nonstd_anonymous_union_type) {
      /* aufp is not the top-most anonymous union.  Recurse to determine its
         parent's address, then append a variant entry to select it. */
      make_anon_union_path(au_parent, p_last_entry, p_addr);
    }  /* if */
  }  /* if */
  last_entry = *p_last_entry;
  vpep = alloc_variant_path_entry();
  vpep->next = NULL;
  last_entry->next = vpep;
  if (parent_class_of(aufp)->kind == (a_type_kind)tk_union) {
    /* aufp is a member of a union itself (always the case in recursive calls,
       but also if the original call is for a member of an ordinary union). */
    last_entry->field = aufp;
    last_entry->base_address = *p_addr;
  }  /* if */
  *p_last_entry = vpep;
  get_mapped_byte_count(&persistent_map, aufp, offset);
  *p_addr += offset;
}  /* make_anon_union_path */


static void add_to_variant_path(a_constexpr_address  *addr,
                                a_field_ptr          union_field,
                                a_type_ptr           top_type)
/*
The given field of a union object or subobject pointed to by addr is being
selected.  Add that field to the variant path associated with addr (and, if
this is the first field added to the path, also add a prefix field for array
element selections).  If union_field is a standard anonymous union field,
addr->address is adjusted to the innermost anonymous union parent and
additional variant path entries are added for nested anonymous unions if
needed.  top_type is the top-most class type in the selection: It may not
be a union type if union_field is an anonymous union field.
*/
{
  a_variant_path_entry_ptr  last_entry, vpep;
  a_symbol_ptr              au_parent;

  if (addr->flags & CA_VARIANT_PATH) {
    /* This entry already has a variant path: Find its end. */
    last_entry = addr->variant.variant_path->next;
    while (last_entry->next != NULL) {
      last_entry = last_entry->next;
    }  /* while */
  } else {
    /* No entries yet: Create a first entry to record an array base address if
       needed. */
    addr->variant.variant_path = alloc_variant_path_entry();
    last_entry = addr->variant.variant_path;
    last_entry->next = NULL;
    last_entry->field = NULL;
    last_entry->base_address = NULL;
    addr->flags |= CA_VARIANT_PATH;
  }  /* if */
  if (top_type->kind == (a_type_kind)tk_union) {
    /* An ordinary union member.  Just add a new entry. */
    vpep = alloc_variant_path_entry();
    vpep->next = NULL;
    last_entry->next = vpep;
    last_entry = vpep;
  }  /* if */
  au_parent = symbol_for(union_field)->variant.field.anonymous_parent_object;
  if (au_parent != NULL && symbol_is(au_parent, sk_field)) {
    a_type_ptr  au_type = au_parent->variant.field.ptr->type;
    if (au_type->kind == (a_type_kind)tk_union &&
        !au_type->variant.class_struct_union.is_nonstd_anonymous_union_type) {
      make_anon_union_path(au_parent, &last_entry, &addr->address);
    }  /* if */
  }  /* if */
  last_entry->field = union_field;
  last_entry->base_address = addr->address;
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
                                    a_boolean             release,
                                    a_source_position     *pos)
/*
The given address entry has its CA_VARIANT_PATH flag set.  Check that it points
to an object whose active variant subobjects match the recorded variant path
and return TRUE if that's the case.  Otherwise, return FALSE and record an
appropriate diagnostic.

If release is TRUE, release the variant path structures when the check is
completed.
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
  if (release) {
    release_variant_path(addr);
  }  /* if */
  return result;
}  /* check_variant_path */


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
  restore_storage_stack(ips, saved_stack_for_full_expr);                      \
}

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
                                   a_byte                *result_storage,
                                   a_byte                *complete_object,
                                   a_constexpr_address   *implied_src);


static a_boolean do_constexpr_dynamic_init(
                                      an_interpreter_state  *ips,
                                      a_dynamic_init_ptr    dip,
                                      a_source_position     *pos,
                                      a_byte                *result_storage,
                                      a_byte                *complete_object);


static a_boolean translate_il_address_offset(an_interpreter_state  *ips,
                                             a_constant_ptr        con,
                                             a_constexpr_address   *cap,
                                             a_type_ptr            obj_type)
/*
con is an address constant being translated to *cap, a representation in
interpreter storage of the address of the complete object of type obj_type.
If con represents the address of a subobject, update *cap accordingly.

Currently, the IL representation is insufficient to handle union members.
If con represents an address of a union subobject, interpretation will fail.
*/
{
  a_boolean         result = TRUE;
  a_type_ptr        subobj_type = skip_typerefs(con->type);
  a_targ_ptrdiff_t  t_offset, t_pos;
  a_byte_count      i_offset, i_size;

  subobj_type = skip_typerefs(subobj_type->variant.pointer.type);
  t_offset = con->variant.address.offset;
  for (;;) {
    if (t_offset == 0 && identical_types(subobj_type, obj_type)) {
      /* *cap is fully updated. */
      break;
    }  /* if */
    switch (obj_type->kind) {
      case tk_array:
        { cap->flags |= CA_ARRAY_ELEMENT;
          cap->length = obj_type->variant.array.variant.number_of_elements;
          if (is_variant_path(cap)) {
            cap->variant.variant_path->base_address = cap->address;
          } else {
            cap->variant.base_address = cap->address;
          }  /* if */
          obj_type = skip_typerefs(obj_type->variant.array.element_type);
          i_size = value_bytes_for_type(ips, obj_type, &result); 
          check_assertion(result);
          t_pos = t_offset/(a_targ_ptrdiff_t)obj_type->size;
          cap->address += i_size*(a_byte_count)t_pos;
          t_offset -= t_pos*obj_type->size;
          if ((a_byte_count)t_pos == cap->length) {
            /* One position past the end of the array.  This has to be the
               address of the corresponding element; not that of a subobject
               thereof. */
            cap->flags |= CA_CANNOT_DEREFERENCE;
            check_assertion(identical_types(subobj_type, obj_type));
          }  /* if */
        }
        break;
      case tk_class:
      case tk_struct:
        { /* Search fields and direct nonvirtual bases for the right offset. */
          a_field_ptr  fp = obj_type->variant.class_struct_union.field_list;
          fp = next_alloc_field(fp);
          for (; fp != NULL; fp = next_alloc_field(fp->next)) {
            a_type_ptr  ftp;
            if (t_offset < (a_targ_ptrdiff_t)fp->offset) continue;
            ftp = skip_typerefs(fp->type);
            if (t_offset < (a_targ_ptrdiff_t)(fp->offset+ftp->size)) {
              t_offset -= fp->offset;
              get_mapped_byte_count(&persistent_map, fp, i_offset);
              cap->address += i_offset;
              obj_type = ftp;
              break;
            }  /* if */
          }  /* for */
          if (fp == NULL) {
            a_base_class_ptr  bcp = base_classes_of(obj_type);
            for (; bcp != NULL; bcp = bcp->next) {
              if (!bcp->direct || bcp->is_virtual) continue;
              if (t_offset < (a_targ_ptrdiff_t)bcp->offset) continue;
              if (t_offset < (a_targ_ptrdiff_t)(bcp->offset+bcp->type->size)) {
                if (bcp->is_optimized_empty_base &&
                    !identical_types(bcp->type, obj_type)) {
                  /* Empty base classes can overlap with other base classes.
                     Skip this base if it is not the addressed subobject. */
                  continue;
                }  /* if */
                t_offset -= bcp->offset;
                get_mapped_byte_count(&persistent_map, bcp, i_offset);
                cap->address += i_offset;
                obj_type = bcp->type;
                break;
              }  /* if */
            }  /* for */
            if (bcp == NULL) {
              /* The search failed. */
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_bad_address, &ips->position, ips);
              goto done;
            }  /* if */
          }  /* if */
          cap->flags &= ~CA_ARRAY_ELEMENT;
        }
        break;
      case tk_union:
        { /* Look through the subobject path for this union (it must be
             present). */
          a_field_ptr           selected_field = NULL;
          a_subobject_path_ptr  path = con->variant.address.subobject_path;
          a_variant_path_entry_ptr  last_entry, vpep;
          for (; path != NULL; path = path->next) {
            if (path->kind == (an_il_entry_kind)iek_field) {
              a_type_ptr  tp = parent_class_of(path->variant.field);
              if (identical_types(obj_type, tp)) {
                selected_field = path->variant.field;
                break;
              }  /* if */
            }  /* if */
          }  /* for */
          check_assertion(selected_field != NULL);
          /* Update the variant path.  Do not use add_to_variant_path because
             it implicitly handles anonymous unions, whereas this process
             traverses them explicitly (we'd account for them twice). */
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
          vpep->field = selected_field;
          vpep->base_address = cap->address;
          last_entry->next = vpep;
          /* Adjust the remaining target offset to account for (normally a
             no-op for a union field) and increase the corresponding
             interpreter offset (not a no-op; i.e., i_offset is not zero
             because some space is used to store the active field). */
          t_offset -= selected_field->offset;
          get_mapped_byte_count(&persistent_map, selected_field, i_offset);
          cap->address += i_offset;
          obj_type = skip_typerefs(selected_field->type);
        }
        break;
      default:
        /* Scalars are sometimes treated as arrays of one element. */
        if (t_offset == (a_targ_ptrdiff_t)obj_type->size) {
          i_size = value_bytes_for_type(ips, obj_type, &result); 
          check_assertion(result);
          cap->address += i_size;
          cap->flags |= CA_CANNOT_DEREFERENCE;
        } else {
          do_constexpr_fail(result);
          info_with_pos(ec_constexpr_bad_address, &ips->position, ips);
        }  /* if */
        goto done;
    }  /* switch */
  }  /* for */
done:
  return result;
}  /* translate_il_address_offset */


static a_constant_ptr instantiate_member_constant(a_variable_ptr  vp)
/*
vp represents a variable that is expected to have a constant value, but with
no recorded initializer.  This can occur in GNU C++ mode with static data
members of class templates, whose initializers are instantiated on demand.  If
this is such a case, perform that instantiation and return the constant.
Otherwise, return an error constant.
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
  if (result == NULL) {
    result = alloc_error_constant();
  }  /* if */
  return result;
}  /* instantiate_member_constant */

/*
Macro to set result_storage from the value of the specified constant.
Duplicates some cases from extract_value_from_constant for performance
reasons.
*/
#define copy_val_from_constant(ips, con, result_storage, complete_object)     \
  (                                                                           \
    ((con)->kind == (a_constant_repr_kind)ck_integer &&                       \
     !(con)->implicit_cast) ?                                                 \
      (*(an_integer_value *)(result_storage) = (con)->variant.integer_value,  \
       TRUE):                                                                 \
    ((con)->kind == (a_constant_repr_kind)ck_float) ?                         \
      ((*fp_value(result_storage) = (con)->variant.float_value), TRUE) :      \
    /* else */                                                                \
      extract_value_from_constant(ips, con, result_storage, complete_object)  \
  )  /* copy_val_from_constant */


static a_boolean extract_value_from_constant(
                                       an_interpreter_state  *ips,
                                       a_constant_ptr        con,
                                       a_byte                *value,
                                       a_byte                *complete_object)
/*
Copy the value of con into the interpreter storage at value, converting
formats as necessary.  Return FALSE if the constant is an error constant.
*/
{
  a_boolean  result = TRUE;

  if (con->implicit_cast) {
    if (con->is_reinterpret_cast) {
      info_with_pos(ec_constexpr_reinterpret_cast, &ips->position, ips);
      do_constexpr_fail(result);
    } else if (con->expr != NULL && !constant_is(con, ck_integer)) {
      /* If the constant includes an implicit cast, evaluate the constant
         through the backing expression so that the cast is correctly
         applied. */
      do_constexpr_full_expression(
                              ips, con->expr, value, complete_object, result);
      goto done;
    }  /* if */
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
          *(an_integer_value *)value = con->variant.integer_value;
        } else {
          unexpected_condition();
        }  /* if */
      }
      break;
    case ck_float:
      *fp_value(value) = con->variant.float_value;
      break;
    case ck_address:
      if (con->variant.address.offset != 0 && con->expr != NULL) {
        /* To reconstruct the offset in interpreter storage, interpret the
           backing expression. */
        do_constexpr_full_expression(
                              ips, con->expr, value, complete_object, result);
      } else {
        a_type_ptr  obj_type = NULL;
        if (con->explicit_cast_applied) {
          a_type_ptr  ptr_type = skip_typerefs(con->type);
          if (ptr_type->kind != (a_type_kind)tk_pointer) {
            /* A reinterpret-like cast from pointer to integer. */
            check_assertion(con->orig_type != NULL);
            info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                &ips->position, con->orig_type, con->type,
                                ips);
            do_constexpr_fail(result);
            break;
          }  /* if */
        }  /* if */
        switch (con->variant.address.kind) {
          case abk_routine:
            { a_routine_ptr  rp = con->variant.address.variant.routine;
#if GNU_EXTENSIONS_ALLOWED
              if (rp->is_weak) {
                /* Weakly declared functions have no definite address (they
                   could have a null address). */
                a_source_position  *diag_pos;
                diag_pos = &con->source_corresp.decl_position;
                if (diag_pos->seq == 0) {
                  diag_pos = &ips->position;
                }  /* if */
                info_with_pos_sym(ec_constexpr_weak_address, diag_pos,
                                  symbol_for(rp), ips);
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
              if (vp->constant_valued || vp->is_constexpr) {
                a_byte      *var_bytes;
                a_type_ptr  vtp = skip_typerefs(vp->type);
                get_stack_bytes(ips, vp, var_bytes);
                if (var_bytes == NULL) {
                  alloc_static_object(ips, vtp, var_bytes, &result);
                  map_stack_bytes(ips, vp, var_bytes);
                  if (result) {
                    a_constant_ptr  cp = NULL;
                    if (vp->init_kind == (an_init_kind)initk_static) {
                      cp = vp->initializer.constant;
                    } else if (vp->init_kind == (an_init_kind)initk_dynamic) {
                      a_dynamic_init_ptr  dip = vp->initializer.dynamic;
                      result = do_constexpr_dynamic_init(
                                                     ips, dip, &ips->position,
                                                     var_bytes, var_bytes);
                    } else {
                      an_init_kind    init_kind;
                      an_initializer  *initializer;
                      get_variable_initializer(vp, (a_scope*)NULL, &init_kind,
                                               &initializer);
                      if (init_kind == (an_init_kind)initk_static) {
                        cp = initializer->constant;
                      } else if (init_kind == (an_init_kind)initk_dynamic) {
                        a_dynamic_init_ptr  dip = initializer->dynamic;
                        result = do_constexpr_dynamic_init(
                                                     ips, dip, &ips->position,
                                                     var_bytes, var_bytes);
                      } else {
                        /* In GNU C++ mode, the initializer may not be
                           instantiated yet. */
                        if (gpp_mode && vp->is_template_variable) {
                          cp = instantiate_member_constant(vp);
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
                    }  /* if */                     
                  }  /* if */
                  if (!result) break;
                  mark_complete_object_initialized(var_bytes);
                  /* Set up a reverse mapping so we can re-create a variable
                     address constant if the address (with potentially a
                     different offset) is returned from the interpreter. */
                  map_stack_bytes(ips, var_bytes, (a_byte*)con);
                }  /* if */
                clear_address(value, var_bytes);
                if (is_const_qualified_type(vp->type)) {
                  ((a_constexpr_address*)value)->flags |= CA_CONST_STORAGE;
                }  /* if */
                obj_type = vtp;
              } else {
#if GNU_EXTENSIONS_ALLOWED
                if (vp->is_weak) {
                  /* Weakly declared variables have no definite address (they
                     could have a null address). */
                  a_source_position  *diag_pos;
                  diag_pos = &con->source_corresp.decl_position;
                  if (diag_pos->seq == 0) {
                    diag_pos = &ips->position;
                  }  /* if */
                  info_with_pos_sym(ec_constexpr_weak_address, diag_pos,
                                    symbol_for(vp), ips);
                  do_constexpr_fail(result);
                  break;
                }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
                clear_runtime_constant_address(value, con);
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
                /* Record a two-way mapping to ensure we always use the same
                   storage, and that we reproduce the original constant if this
                   becomes part of the interpretation result. */
                map_stack_bytes(ips, cp, con_bytes);
                map_stack_bytes(ips, con_bytes, (a_byte*)con);
                if (result) {
                  result = extract_value_from_constant(ips, cp, con_bytes,
                                                       con_bytes);
                }  /* if */
                if (!result) break;
                mark_complete_object_initialized(con_bytes);
              }  /* if */
              clear_address(value, con_bytes);
              ((a_constexpr_address*)value)->flags |= CA_CONST_STORAGE;
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
      }  /* if */
      break;
    case ck_ptr_to_member:
      {
        a_base_class_ptr  bcp = con->variant.ptr_to_member.casting_base_class;
        a_constexpr_ptr_to_mem
                          *pm_value = (a_constexpr_ptr_to_mem*)value;
        a_byte_count      offset = 0;
        a_boolean         is_null;
        if (con->variant.ptr_to_member.is_function_ptr) {
          pm_value->variant.routine =
                                   con->variant.ptr_to_member.variant.routine;
          is_null = (pm_value->variant.routine == NULL);
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
            a_source_position  *diag_pos = &con->source_corresp.decl_position;
            if (diag_pos->seq == 0) {
              diag_pos = &ips->position;
            }  /* if */
            info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                diag_pos, con->orig_type, con->type, ips);
            do_constexpr_fail(result);
           }  /* if */
        }  /* if */
        if (bcp != NULL) {
          /* We cannot look up bcp's offset in the persistent map directly
             because it may be an indirect base class. */
          a_derivation_step_ptr  dsp = bcp->derivation->path;
          a_type_ptr             prev_type = dsp->base_class->type;
          /* Ensure the derived class has been laid out. */
          (void)f_value_bytes_for_type(ips, bcp->derived_class, &result);
          get_mapped_byte_count(&persistent_map, dsp->base_class, offset);
          for (dsp = dsp->next; dsp != NULL; dsp = dsp->next) {
            a_base_class_ptr  sbcp;
            a_byte_count      step;
            sbcp = find_direct_base_class_of(prev_type, dsp->base_class->type);
            prev_type = dsp->base_class->type;
            get_mapped_byte_count(&persistent_map, sbcp, step);
            offset += step;
          }  /* for */
        }  /* if */
        pm_value->this_class_adjustment = offset;
        pm_value->subtract_adjustment =
                                      con->variant.ptr_to_member.cast_to_base;
      }  /* if */
      break;
    case ck_dynamic_init:
      {
        result = do_constexpr_dynamic_init(ips, con->variant.dynamic_init,
                                           &con->source_corresp.decl_position,
                                           value, complete_object);
      }
      break;
    case ck_string:
      {
        a_type_ptr     tp = skip_typerefs(con->type);
        a_type_ptr     etp;
        a_targ_size_t  n_elems, k, char_size;
        a_byte_count   elem_size;
        a_const_char   *char_ptr;
        etp = skip_typerefs(tp->variant.array.element_type);
        char_size = etp->size;
        n_elems = tp->variant.array.variant.number_of_elements;
        elem_size = value_bytes_for_type(ips, etp, &result);
        char_ptr = con->variant.string.value;
        /* Map the interpreter storage for the string back to the constant
           entry so that that constant can, if needed, be retrieved by
           copy_interpreter_object_to_constant. */
        map_ptr(&ips->map, value, (a_byte*)con);
        for (k = 0; k<n_elems; k += 1) {
          unsigned long char_val = extract_character_from_string(
                                           char_ptr, (unsigned int)char_size);
          set_integer_value((an_integer_value*)value,
                            (a_host_large_integer)char_val);
          mark_subobject_initialized(value, complete_object);
          value += elem_size;
          char_ptr += char_size;
        }  /* for */
      }
      break;
    case ck_aggregate:
      {
        a_type_ptr  tp = skip_typerefs(con->type);
        if (tp->kind == (a_type_kind)tk_array) {
          a_targ_size_t   n_elems, k, repeat;
          a_byte_count    elem_size;
          a_constant_ptr  elem_con;
          a_type_ptr      etp = tp->variant.array.element_type;
          etp = skip_typerefs(etp);
          n_elems = tp->variant.array.variant.number_of_elements;
          elem_size = value_bytes_for_type(ips, etp, &result);
          if (!result) break;
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
                                     ips, elem_con, value, complete_object)) {
                do_constexpr_fail(result);
                break;
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
          }  /* for */
        } else if (tp->kind == (a_type_kind)tk_struct ||
                   tp->kind == (a_type_kind)tk_class) {
          a_field_ptr       fp = tp->variant.class_struct_union.field_list;
          a_base_class_ptr  bcp = base_classes_of(tp);
          a_constant_ptr    elem_con;
          elem_con = con->variant.aggregate.first_constant;
          /* Initialize base subobjects first. */
          for (;;) {
            a_byte_count  offset;
            while (bcp != NULL && !bcp->direct) {
              bcp = bcp->next;
            }  /* while */
            if (bcp == NULL) break;
            get_mapped_byte_count(&persistent_map, bcp, offset);
            if (elem_con != NULL &&
                elem_con->constant_for_base_class_from_constexpr_folding) {
              if (!copy_val_from_constant(
                              ips, elem_con, value+offset, complete_object)) {
                do_constexpr_fail(result);
                break;
              }  /* if */
              elem_con = elem_con->next;
            }  /* if */
            /* Record the derivation step (not really needed if this is for
               an assignment rather than for an initialization, but it is 
               easier -- and probably cheaper -- to do this
               indiscriminately). */
            record_subobject_derivation(value+offset, bcp);
            mark_subobject_initialized(value+offset, complete_object);
            bcp = bcp->next;
          }  /* for */
          for (;;) {
            a_byte_count  offset;
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
            get_mapped_byte_count(&persistent_map, fp, offset);
            if (elem_con == NULL) {
              /* No more initializers, but we have more fields.  Zero the
                 remainder of the class value. */
              a_type_ptr  ftp = skip_typerefs(fp->type);
              init_subobject_to_zero(ips, value+offset, ftp, complete_object);
            } else if (constant_is(elem_con, ck_designator) &&
                       elem_con->variant.designator.is_field_designator &&
                       !elem_con->variant.designator.is_generic) {
              /* A field designator. */
              fp = elem_con->variant.designator.variant.field;
              elem_con = elem_con->next;
              continue;
            } else if (!copy_val_from_constant(
                              ips, elem_con, value+offset, complete_object)) {
              do_constexpr_fail(result);
              break;
            } else {
              if (fp->is_bit_field) {
                /* Fit the value in the bit field width. */
                trim_bit_field(value+offset, fp->bit_size,
                               fp->bit_field_is_signed);
              }  /* if */
              mark_subobject_initialized(value+offset, complete_object);
              elem_con = elem_con->next;
            }  /* if */
            fp = fp->next;
          }  /* for */
        } else if (tp->kind == (a_type_kind)tk_union) {
          /* Initialize the first field (unless another field is
             designated). */
          a_field_ptr     fp;
          a_constant_ptr  elem_con;
          a_byte_count    offset;
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
              a_source_position  *diag_pos =
                                           &con->source_corresp.decl_position;
              if (diag_pos->seq == 0) {
                diag_pos = &ips->position;
              }  /* if */
              info_with_pos_sym(ec_constexpr_missing_initializer_for_field,
                                diag_pos, symbol_for(fp), ips);
              do_constexpr_fail(result);
            }  /* if */
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
          if (fp == NULL || elem_con->next != NULL) {
            /* Unions should have only one actual initializer constant
               (possibly following a designator).  This can happen with
               severe errors, however. */
            expect_error();
            do_constexpr_fail(result);
            break;
          }  /* if */
          get_mapped_byte_count(&persistent_map, fp, offset);
          if (!copy_val_from_constant(
                              ips, elem_con, value+offset, complete_object)) {
            do_constexpr_fail(result);
          } else {
            if (fp->is_bit_field) {
              /* Fit the value in the bit field width. */
              trim_bit_field(value+offset, fp->bit_size,
                             fp->bit_field_is_signed);
            }  /* if */
            /* Record the active field. */
            *(a_field_ptr*)value = fp;
            mark_subobject_initialized(value+offset, complete_object);
          }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
        } else if (tp->kind == (a_type_kind)tk_vector) {
          a_constant_ptr  elem_con;
          a_type_ptr      etp = skip_typerefs(tp->variant.vector.element_type);
          a_targ_size_t   k, n_elems = tp->size/etp->size;
          a_byte_count    elem_size = value_bytes_for_type(ips, etp, &result);
          if (!result) break;
          elem_con = con->variant.aggregate.first_constant;
          for (k = 0; k<n_elems;) {
            if (!copy_val_from_constant(
                                     ips, elem_con, value, complete_object)) {
              do_constexpr_fail(result);
              break;
            }  /* if */
            elem_con = elem_con->next;
            k += 1;
            value += elem_size;
            if (elem_con == NULL) {
              if (k<n_elems) {
                /* Not all elements are covered.  Zero the remainder. */
                memzero(value, size_t_arg((n_elems-k)*elem_size));
              }  /* if */
              break;
            }  /* if */
          }  /* for */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        } else {
          do_constexpr_fail(result);
        }  /* if */
      }
      break;
    case ck_init_repeat:
      {
        a_constant_ptr  elem_con = con->variant.init_repeat.constant;
        a_type_ptr      etp = skip_typerefs(elem_con->type);
        a_targ_size_t   n_elems, k;
        a_byte_count    elem_size;
        n_elems = con->variant.init_repeat.count;
        elem_size = value_bytes_for_type(ips, etp, &result);
        if (!result) break;
        for (k = 0; k<n_elems;) {
          mark_complete_class_object_if_needed(etp, value);
          if (!copy_val_from_constant(ips, elem_con, value, complete_object)) {
            do_constexpr_fail(result);
            break;
          }  /* if */
          mark_subobject_initialized(value, complete_object);
          k  += 1;
          value += elem_size;
        }  /* for */
      }
      break;
    case ck_void:
      /* void values have no representation: Nothing to do. */
      break;
    default:
      { a_source_position  *diag_pos = &con->source_corresp.decl_position;
        if (diag_pos->seq == 0) {
          diag_pos = &ips->position;
        }  /* if */
        info_with_pos(ec_constexpr_invalid_constant_kind, diag_pos, ips);
        do_constexpr_fail(result);
      }
  }  /* switch */
done:
  return result;
}  /* extract_value_from_constant */


static a_boolean constexpr_copy_object(an_interpreter_state  *ips,
                                       a_type_ptr            tp,
                                       a_byte                *src_bytes,
                                       a_byte                *dst_bytes,
                                       a_byte                *complete_obj)
/*
Copy an object of the given type from one interpreter storage location
(src_bytes) to another (dst_bytes).  complete_obj points to the complete
object of the destination (dst_bytes is within that object).
*/
{
  a_boolean     result = TRUE;
  a_byte_count  n_bytes = value_bytes_for_type(ips, tp, &result);

  if (result) {
    a_byte_count  k;
    (void)memcpy(dst_bytes, src_bytes, size_t_arg(n_bytes));
    /* Mark possible subobject starting positions as initialized. */
    for (k = 0; k<n_bytes; k += HOST_ALIGNMENT_REQUIRED) {
      mark_subobject_initialized(dst_bytes+k, complete_obj);  
    }  /* for */
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


static a_boolean do_array_constructor_copy(
                                       an_interpreter_state  *ips,
                                       a_dynamic_init_ptr    dip,
                                       a_source_position     *pos,
                                       a_byte                *result_storage,
                                       a_byte                *complete_object)
/*
The given dik_constructor dynamic initialization entry has its is_array_copy
flag set to TRUE.  Perform the array copy it represents (to storage indicated
by result_storage, part of the complete object represented by complete_object).
Return TRUE is successful.  Otherwise, return FALSE and update *ips
accordingly.
*/
{   
  a_boolean            result = TRUE;
  an_expr_node_ptr     array_expr = dip->variant.constructor.args;
  a_byte               *lvalue;
  a_constexpr_address  *src_addr;
  a_byte_count         n_lvalue_bytes;
  a_type_ptr           tp = skip_typerefs(array_expr->type), elem_type;

  check_assertion(array_expr->is_lvalue);
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
    a_targ_size_t   k, length;
    a_byte_count    elem_size;
    a_dynamic_init  dip_copy = *dip;
    dip_copy.variant.constructor.args = array_expr->next;
    dip_copy.variant.constructor.is_array_copy = FALSE;
    dip_copy.variant.constructor
                    .is_copy_constructor_with_implied_source = TRUE;
    check_assertion(tp->kind == (a_type_kind)tk_array);
    length = tp->variant.array.variant.number_of_elements;
    elem_type = skip_typerefs(tp->variant.array.element_type);
    elem_size = value_bytes_for_type(ips, elem_type, &result);
    if (!result) goto done;
    /* Decay src_addr from the address of the array to the address of its
       first element. */
    src_addr->flags |= CA_ARRAY_ELEMENT;
    src_addr->length = length;
    if (is_variant_path(src_addr)) {
      src_addr->variant.variant_path->base_address = src_addr->address;
    } else {
      src_addr->variant.base_address = src_addr->address;
    }  /* if */
    for (k = 0; k<length; ++k) {
      if (!do_constexpr_ctor(ips, &dip_copy, pos, result_storage+k*elem_size,
                             complete_object, src_addr)) {
        do_constexpr_fail(result);
        goto done;
      } else {
        src_addr->address += elem_size;
      }  /* if */
    }  /* for */
  }  /* if */
done:
  return result;
}  /* do_array_constructor_copy */


static a_boolean do_constexpr_dynamic_init(
                                        an_interpreter_state  *ips,
                                        a_dynamic_init_ptr    dip,
                                        a_source_position     *pos,
                                        a_byte                *result_storage,
                                        a_byte                *complete_object)
/*
Evaluate the given dynamic initialization for the given storage.
*/
{
  a_boolean  result = FALSE;

  if (dip->destructor != NULL) {
    info_with_pos(ec_constexpr_ctor_with_dtor, pos, ips);
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  switch (dip->kind) {
    case dik_nonconstant_aggregate:
      { a_constant_ptr  con = dip->variant.constant;
        a_type_ptr      con_type = skip_typerefs(con->type);
        a_byte          *this_bytes;
        if (is_immediate_class_type(con_type)) {
          /* Set up a "this" pointer in case we run into enk_param_ref nodes.
             It is associated with &ips->curr_call_frame. */
          a_type_ptr  this_type = make_pointer_type(con_type);
          alloc_complete_object(ips, sizeof(a_constexpr_address), this_type,
                               this_bytes);
          clear_address(this_bytes, result_storage);
          ((a_constexpr_address *)this_bytes)->complete_object =
                                                              complete_object;
          ((a_constexpr_address *)this_bytes)->alloc_seq_number =
                                                   ips->curr_alloc_seq_number;
          mark_complete_object_initialized(this_bytes);
          map_stack_bytes(ips, &ips->curr_call_frame, this_bytes);
        } else {
          this_bytes = NULL;
        }  /* if */
        result = copy_val_from_constant(ips, dip->variant.constant,
                                        result_storage, complete_object);
        if (this_bytes != NULL) {
          unmap_stack_bytes(ips, &ips->curr_call_frame);
        }  /* if */
      }
      break;
    case dik_constant:
      result = copy_val_from_constant(ips, dip->variant.constant,
                                      result_storage, complete_object);
      break;
    case dik_expression:
    case dik_class_result_via_ctor:
      result = do_constexpr_expression(ips, dip->variant.expression,
                                       result_storage, complete_object);
      break;
    case dik_constructor:
      if (dip->variant.constructor.is_array_copy) {
        result = do_array_constructor_copy(ips, dip, pos, result_storage,
                                           complete_object);
      } else {
        result = do_constexpr_ctor(ips, dip, pos, result_storage,
                                   complete_object, /*implied_src=*/NULL);
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
          result = do_constexpr_expression(ips, source_expr, result_storage,
                                           complete_object);
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
done:
  return result;
}  /* do_constexpr_dynamic_init */


static a_boolean do_constexpr_init_variable(an_interpreter_state   *ips,
                                            a_variable_ptr         vp,
                                            a_byte                 *storage,
                                            a_source_position      *pos)
/*
Evaluate the (dynamic) initializer of the given variable.  Return FALSE if an
error occurs.
*/
{
  a_boolean              result = TRUE;
  a_storage_stack_state  saved_stack_for_full_expr;
  a_dynamic_init_ptr     dip = vp->initializer.dynamic;

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
  if (do_constexpr_dynamic_init(ips, dip, pos, storage, storage)) {
    mark_complete_object_initialized(storage);
  } else {
    do_constexpr_fail(result);
  }  /* if */
  if (vp->extends_lifetime) {
    /* Release the ordinary storage stack blocks for this expression.
       The large blocks will be released by the call to
       restore_storage_stack below. */
    release_constexpr_stack(&ips->storage_stack);
  }  /* if */
  restore_storage_stack(ips, saved_stack_for_full_expr);
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

  if (cond_var_init == NULL && init == NULL) {
    expect_error()
    do_constexpr_fail(result);
  } else {
    save_storage_stack(ips, *vs_state);
    if (init != NULL) {
      if (init->kind == (a_statement_kind)stmk_decl) {
        /* Allocate storage for any variables. */
        an_il_entity_list_entry_ptr  p = init->variant.decl.entities;
        for (; p != NULL; p = p->next) {
          if (p->entity.kind == (a_byte_il_entry_kind)iek_variable) {
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
      restore_storage_stack(ips, *vs_state);
    }  /* if */
  }  /* if */
  return result;
}  /* do_constexpr_condition_alloc */


static void do_constexpr_condition_dealloc(an_interpreter_state   *ips,
                                           an_expr_node_ptr       expr,
                                           a_storage_stack_state  *vs_state)
/*
expr is an enk_condition node representing the condition expression of a
statement (i.e., the <expr> in "if (<expr>) ...", "switch (<expr>) ...", etc.).
Deallocate and unmap the associated variable.  Restore the storage state
recorded in *vs_state.
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
      /* Allocate storage for any variables. */
      an_il_entity_list_entry_ptr  p = init->variant.decl.entities;
      for (; p != NULL; p = p->next) {
        if (p->entity.kind == (a_byte_il_entry_kind)iek_variable) {
          a_variable_ptr  vp = (a_variable_ptr)p->entity.ptr;
          do_constexpr_unmap_variable(ips, vp);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  restore_storage_stack(ips, *vs_state);
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
          if (p->entity.kind == (a_byte_il_entry_kind)iek_variable) {
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
  if (result &&
      !do_constexpr_expression(ips, expr_to_evaluate, value, value)) {
    result = FALSE;
  }  /* if */
  release_address_structures(expr, expr_type, value);
  restore_storage_stack(ips, saved_stack_for_full_expr);
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
      /* Allocate storage for any variables. */
      an_il_entity_list_entry_ptr  p = init->variant.decl.entities;
      for (; p != NULL; p = p->next) {
        if (p->entity.kind == (a_byte_il_entry_kind)iek_variable) {
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
    }  /* if */
  }  /* if */
  if (result) {
    /* Interpret the statements in the block. */
    for (; result && stmt != NULL; stmt = stmt->next) {
      result = do_constexpr_statement(ips, stmt);
      if (ips->curr_call_frame->return_active ||
          ips->curr_call_frame->break_active ||
          ips->curr_call_frame->continue_active) {
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
    restore_storage_stack(ips, saved_stack);
  }  /* if */
  return result;
}  /* do_constexpr_block_statement */


static a_boolean do_constexpr_for_statement(an_interpreter_state  *ips,
                                            a_statement_ptr       stmt)
/*
Interpret the given for-statement.
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
  if (init != NULL && !do_constexpr_statement(ips, init)) {
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
      has_cond_var = (expr->kind == (an_expr_node_kind)enk_condition);
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
            } else if (ips->curr_call_frame->break_active) {
              /* Stop the loop (which completes the execution of the break
                 statement). */
              ips->curr_call_frame->break_active = FALSE;
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
      do_constexpr_condition_dealloc(ips, expr, &cond_saved_stack);
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
  restore_storage_stack(ips, saved_stack);
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
    dip = vp[k]->initializer.dynamic;
    if (!do_constexpr_dynamic_init(ips, dip, &stmt->position,
                                   var_storage[k], var_storage[k])) {
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
        if (expr != NULL) {
          get_int_val_from(expr_value, tp, bool_val, ovfl);
        } else {
          bool_val = TRUE;
          ovfl = FALSE;
        }  /* if */
        if (!ovfl && bool_val) {
          /* Initialize the iterator variable: */
          if (!do_constexpr_dynamic_init(ips, dip, &stmt->position,
                                         var_storage[0], var_storage[0])) {
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
            } else if (ips->curr_call_frame->break_active) {
              /* Break out of the loop (which completes the execution of
                 the break statement). */
              ips->curr_call_frame->break_active = FALSE;
              break;
            } else if (ips->curr_call_frame->continue_active) {
              /* Continue, but clear the continue_active flag since we've
                 reached the point of continuation. */
              ips->curr_call_frame->continue_active = FALSE;
            }  /* if */
            if (incr != NULL) {
              do_constexpr_full_expression(
                                   ips, incr, incr_value, incr_value, result);
              release_address_structures(incr, incr_type,
                                                  incr_value);
            }  /* if */
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
  restore_storage_stack(ips, saved_stack);
  return result;
}  /* do_constexpr_range_based_for_statement */


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
        restore_storage_stack(ips, saved_stack);
      }
      break;
    case stmk_if:
    case stmk_constexpr_if:
      {
        a_boolean             has_cond_var;
        a_host_large_integer  bool_val;
        a_statement_ptr       then_statement, else_statement;
        if (stmt->kind == (a_statement_kind)stmk_if) {
          then_statement = stmt->variant.if_stmt.then_statement;
          else_statement = stmt->variant.if_stmt.else_statement;
        } else {
          then_statement = stmt->variant.constexpr_if->then_statement;
          else_statement = stmt->variant.constexpr_if->else_statement;
        }  /* if */
        expr = stmt->expr;
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
        result = do_constexpr_condition(has_cond_var, ips, expr, tp,
                                        expr_value);
        if (result) {
          /* Evaluation of the test expression succeeded.  Get its value to
             see which dependent statement should be executed. */
          get_int_val_from(expr_value, tp, bool_val, ovfl);
          if (ovfl || bool_val) {
            /* Execute the "then" statement. */
            result = do_constexpr_statement(ips, then_statement);
          } else if (else_statement != NULL) {
            result = do_constexpr_statement(ips, else_statement);
          }  /* if */
        }  /* if */
        if (has_cond_var) {
          do_constexpr_condition_cleanup(ips, expr);
          do_constexpr_condition_dealloc(ips, expr, &saved_stack);
        }  /* if */
      }
      break;
    case stmk_while:
      {
        a_boolean             has_cond_var;
        a_host_large_integer  bool_val = FALSE;
        expr = stmt->expr;
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
                } else if (ips->curr_call_frame->break_active) {
                  /* Stop the loop (which completes the execution of the break
                     statement). */
                  ips->curr_call_frame->break_active = FALSE;
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
          do_constexpr_condition_dealloc(ips, expr, &saved_stack);
        }  /* if */
      }
      break;
    case stmk_goto:
      if (stmt->variant.label.ptr->break_label) {
        ips->curr_call_frame->break_active = TRUE;
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
        if (stmt->expr != NULL) {
          do_constexpr_full_expression(ips, stmt->expr, frame->result_storage,
                                       frame->complete_object, result);
        } else if (stmt->variant.return_dynamic_init != NULL) {
          /* Handle return_dynamic_init case. */
          result = do_constexpr_dynamic_init(ips,
                                             stmt->variant.return_dynamic_init,
                                             &stmt->position, 
                                             frame->result_storage,
                                             frame->result_storage);
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
      break;
    case stmk_block:
      { a_block_ptr  block = stmt->variant.block.extra_info;
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
          } else if (ips->curr_call_frame->break_active) {
            /* Break out of the loop (which completes the execution of the
               break statement). */
            ips->curr_call_frame->break_active = FALSE;
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
      result = do_constexpr_for_statement(ips, stmt);
      break;
    case stmk_range_based_for:
      result = do_constexpr_range_based_for_statement(ips, stmt);
      break;
    case stmk_switch_case:
      /* Nothing to do. */
      break;
    case stmk_switch:
      { a_statement_ptr          substmt;
        a_switch_case_entry_ptr  scep = stmt->variant.switch_stmt.extra_info
                                            ->sorted_cases;
        a_boolean                is_signed, has_cond_var;
        expr = stmt->expr;
        has_cond_var = (expr->kind == (an_expr_node_kind)enk_condition);
        if (has_cond_var &&
            !do_constexpr_condition_alloc(ips, expr, &saved_stack)) {
          break;
        }  /* if */
        /* The type of the test expression is known to be bool. */
        tp = skip_typerefs(expr->type);
        n_bytes = value_bytes_for_type(ips, tp, &result);
        alloc_complete_object(ips, n_bytes, tp, expr_value);
        result = do_constexpr_condition(has_cond_var, ips, expr, tp,
                                        expr_value);
        if (!result) {
          goto done_with_switch;
        }  /* if */
        is_signed = int_kind_is_signed[tp->variant.integer.int_kind];
        /* Search through the ordered list of case labels for the one selected
           by the switch expression. */
        for (; scep != NULL; scep = scep->next_on_sorted_list) {
          a_byte          *case_bytes;
          int             cmp;
          a_constant_ptr  case_con = scep->case_value;
          a_type_ptr      case_tp = skip_typerefs(case_con->type);
          alloc_complete_object(ips, n_bytes, case_tp, case_bytes);
          result = copy_val_from_constant(
                               ips, scep->case_value, case_bytes, case_bytes);
          if (!result) {
            goto done_with_switch;
          }  /* if */
          cmp = cmp_integer_values((an_integer_value*)expr_value, is_signed,
                                   (an_integer_value*)case_bytes, is_signed);
          if (cmp == 0) {
            /* We found the case entry. */
            break;
          } else if (cmp < 0) {
            /* There may be more entries, but they won't match. */
            scep = NULL;
            break;
#if GNU_EXTENSIONS_ALLOWED
          } else if (scep->range_end != NULL) {
            result = copy_val_from_constant(
                                ips, scep->range_end, case_bytes, case_bytes);
            if (!result) {
              goto done_with_switch;
            }  /* if */
            cmp = cmp_integer_values((an_integer_value*)expr_value, is_signed,
                                     (an_integer_value*)case_bytes, is_signed);
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
          if (ips->curr_call_frame->break_active) {
            /* Break out of the switch statements (which completes the
               execution of the break statement). */
            ips->curr_call_frame->break_active = FALSE;
            break;
          } else if (ips->curr_call_frame->return_active ||
                     ips->curr_call_frame->break_active ||
                     ips->curr_call_frame->continue_active) {
            /* Other branching statements end the execution of the switch, but
               they are not completed by the switch. */
            break;
          } else if (substmt->next != NULL) {
            /* The statement just interpreted is followed by another one.
               We'll interpret it next. */
            substmt = substmt->next;
          } else {
            /* There are no more statements in this sequence.  Move up to the
               parent sequence if appropriate. */
            for (;;) {
              substmt = substmt->parent;
              if (substmt == stmt) {
                /* We're flowing off the switch statement itself. */
                goto done_with_switch;
              } else if (substmt->next != NULL) {
                substmt = substmt->next;
                break;
              }  /* if */
              /* Continue up the parent chain. */
            }  /* for */
          }  /* if */
          result = do_constexpr_statement(ips, substmt);
          if (!result) {
            goto done_with_switch;
          }  /* if */
        }  /* for */
done_with_switch:
        if (has_cond_var) {
          do_constexpr_condition_cleanup(ips, expr);
          do_constexpr_condition_dealloc(ips, expr, &saved_stack);
        }  /* if */
      }
      break;
    case stmk_init:
      { a_dynamic_init_ptr     dip = stmt->variant.dynamic_init;
        a_variable_ptr         vp = dip->variable;
        a_byte                 *var_storage;
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
         initialized through stmk_init statements. */
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

#if BUILTIN_FUNCTIONS_ENABLED

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
        val = fp_is_negative(fpkind, fpval);
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
  n_bits = arg_tp->size*CHAR_BIT;
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
Evaluate the standard strlen function on the operand of type arg_tp stored in
arg_bytes.  Place the result in *result_storage.  Return FALSE if this fails
(because the entity pointed to is not a null-terminated string) and record a
potential diagnostic for the given expression node and interpreter state.
*/
{
  a_boolean  result = TRUE;

  if (arg_tp->kind == (a_type_kind)tk_pointer) {
    a_constexpr_address  *addr = (a_constexpr_address*)arg_bytes;
    a_type_ptr           tp = skip_typerefs(arg_tp->variant.pointer.type);
    if (addr->address == NULL) {
      do_constexpr_fail(result);
      info_with_pos(ec_constexpr_null_dereference, &call_node->position, ips);
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
  a_boolean         interpreted, err = FALSE, depends_on_fp_mode;
  an_expr_node_ptr  args = call_node->variant.operation.operands->next;
  a_byte            *arg1_bytes;

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
    case bfk_fabs:
    case bfk_fabsf:
    case bfk_fabsl:
      {
        interpreted = TRUE;
        if (args == NULL || args->next != NULL ||
            !is_real_floating_type(args->type)) {
          unexpected_condition();
        } else {
          a_type_ptr    tp = skip_typerefs(args->type);
          a_byte_count  n_bytes = value_bytes_for_type(ips, tp, p_result);
          alloc_complete_object(ips, n_bytes, tp, arg1_bytes);
          if (do_constexpr_expression(ips, args, arg1_bytes, arg1_bytes)) {
            a_float_kind  fk = tp->variant.float_kind;
            if (fp_is_negative(fk, fp_value(arg1_bytes))) {
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
        interpreted = TRUE;
        if (args == NULL || args->next != NULL ||
            !is_real_floating_type(args->type)) {
          unexpected_condition();
        } else {
          a_type_ptr    tp = skip_typerefs(args->type);
          a_byte_count  n_bytes = value_bytes_for_type(ips, tp, p_result);
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
    default:
      interpreted = FALSE;
  }  /* switch */
  return interpreted;
}  /* do_constexpr_builtin_function */

#endif /* BUILTIN_FUNCTIONS_ENABLED */

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
  a_boolean         result = TRUE;
  a_constexpr_ptr_to_mem
                    *pm_target = NULL;

  /* First determine the actual callee. */
  callee_node = call_node->variant.operation.operands;
  if (is_routine_node(callee_node)) {
    callee = node_routine(callee_node);
  } else if (node_operator_is(call_node, eok_dot_pm_call) ||
             node_operator_is(call_node, eok_points_to_pm_call)) {
    /* A call through a pointer-to-member function.  We'll determine the
       callee here, and adjust the "this" pointer later on. */
    a_type_ptr    pm_type = skip_typerefs(callee_node->type);
    a_byte        *pm_bytes;
    a_byte_count  n_pm_bytes = value_bytes_for_type(ips, pm_type, &result);
    alloc_complete_object(ips, n_pm_bytes, pm_type, pm_bytes);
    pm_target = (a_constexpr_ptr_to_mem*)pm_bytes;
    if (do_constexpr_expression(ips, callee_node, pm_bytes, pm_bytes)) {
      callee = pm_target->variant.routine;
      if (callee == NULL) {
        info_with_pos(ec_constexpr_null_callee, &callee_node->position, ips);
        do_constexpr_fail(result);
        goto done;
      }  /* if */
    } else {
      do_constexpr_fail(result);
      goto done;
    }  /* if */
  } else {
    /* An indirect call. */
    a_byte  *addr_bytes;
    alloc_stack_bytes(ips, sizeof(a_constexpr_address), addr_bytes);
    if (do_constexpr_expression( ips, callee_node, addr_bytes, addr_bytes)) {
      a_constexpr_address  *addr = (a_constexpr_address*)addr_bytes;
      if (is_function_address(addr)) {
        callee = addr->variant.routine;
        if (callee == NULL) {
          info_with_pos(ec_constexpr_null_callee, &callee_node->position, ips);
          do_constexpr_fail(result);
          goto done;
        }  /* if */
      } else if (addr->address == NULL) {
        info_with_pos(ec_constexpr_null_callee, &callee_node->position, ips);
        do_constexpr_fail(result);
        goto done;
      } else {
        unexpected_condition();
      }  /* if */
    } else {
      do_constexpr_fail(result);
      goto done;
    }  /* if */
  }  /* if */
  /* Now interpret the call if possible. */
#if BUILTIN_FUNCTIONS_ENABLED
  {
    a_routine_ptr  eff_callee = callee;
#if GNU_EXTENSIONS_ALLOWED
    if (eff_callee->implicit_alias && !eff_callee->defined &&
        gnu_routine_supp(eff_callee)->aliased_routine != NULL) {
      /* Some functions are implicitly aliased to a built-in function. */
      eff_callee = gnu_routine_supp(eff_callee)->aliased_routine;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    if (special_kind_is(eff_callee, sfk_none) &&
        eff_callee->variant.builtin_function_kind !=
                                          (a_builtin_function_kind)bfk_none &&
        do_constexpr_builtin_function(ips, eff_callee, call_node,
                                      result_storage, &result)) {
      goto done;
    } else if (!result) {
      goto done;
    }  /* if */
  }
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  if (!callee->is_constexpr) {
    info_with_pos_sym(ec_constexpr_call_to_nonconstexpr_function,
                      &callee_node->position, symbol_for(callee), ips);
    do_constexpr_fail(result);
    goto done;
  }  /* if */
  if (!callee->defined) {
    set_instance_required(symbol_for(callee), TRUE, SIR_CONSTANT_CONTEXT);
  }  /* if */
  if (callee->function_def_number == NULL_function_def_number) {
    info_with_pos_sym(ec_constexpr_function_undefined, &callee_node->position,
                      symbol_for(callee), ips);
    do_constexpr_fail(result);
  } else if (callee->is_prototype_instantiation) {
    info_with_pos_sym(ec_constexpr_call_not_interpretable,
                      &call_node->position, symbol_for(callee), ips);
    do_constexpr_fail(result);
  } else if (cost_exceeded(ips)) {
    more_info_diagnostic(ec_excessive_constexpr_complexity, &ips->position,
                         &ips->diag_list);
    do_constexpr_fail(result);
  } else {
    a_scope_ptr     callee_scope = scope_for_routine(callee);
    a_statement_ptr
                    block_stmt = callee_scope->assoc_block;
    a_call_frame    frame;
    a_variable_ptr  params = callee_scope->variant.routine.parameters,
                    param, this_var;
    a_byte_count    n_args = 0, n_params = 0;
    a_byte_count    *arg_size;
    a_byte          *arg_ptrs, **p_arg_ptr, *arg_sizes;
    an_alloc_seq_number
                    alloc_seq_number;
    unsigned long   up_front_cost;
    /* Don't attempt to interpret a non-constexpr function.  The flag
       scope->is_constexpr_routine is set at the end of a constexpr function
       definition, so this also prevents the interpretation of a function that
       is not fully parsed (e.g., requested due to a recursive call in a
       constexpr function). */
    if (!callee_scope->is_constexpr_routine) {
      do_constexpr_fail(result);
      info_with_pos_sym(ec_constexpr_call_not_interpretable,
                        &call_node->position, symbol_for(callee), ips);
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
    for (arg = callee_node->next; arg != NULL; arg = arg->next) {
      n_args += 1;
    }  /* for */
    alloc_stack_bytes(ips, n_args*sizeof(a_byte*), arg_ptrs);
    alloc_stack_bytes(ips, n_args*sizeof(a_byte_count), arg_sizes);
    /* Count the parameters (including "this") to make sure there are enough
       arguments for the parameters. */
    this_var = callee_scope->variant.routine.this_param_variable;
    if (this_var != NULL) n_params += 1;
    for (param = params; param != NULL; param = param->next) {
      n_params += 1;
    }  /* if */
    if (n_args < n_params) {
      info_with_pos(ec_too_few_arguments, &call_node->position, ips);
      do_constexpr_fail(result);
      goto done;
    }  /* if */
    /* Phase 1: Allocate and evaluate the arguments. */
    p_arg_ptr = (a_byte**)arg_ptrs;
    arg_size = (a_byte_count*)arg_sizes;
    arg = callee_node->next;
    if (this_var != NULL) {
      a_byte        *this_bytes;
      a_type_ptr    this_type = skip_typerefs(this_var->type);
      a_type_ptr    tp = skip_typerefs(arg->type);
      a_byte_count  this_n_bytes = sizeof(a_constexpr_address);
      do_host_alignment(this_n_bytes);
      *arg_size = this_n_bytes;
      arg_size += 1;
      this_n_bytes += sizeof(a_var_postfix);
      alloc_complete_object(ips, this_n_bytes, this_type, this_bytes);
      *p_arg_ptr = this_bytes;
      p_arg_ptr += 1;
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
        clear_address(this_bytes, class_bytes);
        ((a_constexpr_address *)this_bytes)->alloc_seq_number =
                                          ips->storage_stack.alloc_seq_number;
      }  /* if */
      mark_complete_object_initialized(this_bytes);
      if (pm_target != NULL &&
          !adjust_this_address(ips, (a_constexpr_address*)this_bytes,
                               pm_target, this_var->type, call_node)) {
        do_constexpr_fail(result);
        goto done;
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
      if (result) {
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
    /* Phase 2: Map the parameters to the arguments. */
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
    /* Run the function's top-level block statement. */
    if (block_stmt->kind != (a_statement_kind)stmk_block) {
      check_assertion(block_stmt->kind == (a_statement_kind)stmk_try_block);
      info_with_pos(ec_constexpr_try_block, &block_stmt->position, ips);
      do_constexpr_fail(result);
    } else {
      result = do_constexpr_block_statement(ips, block_stmt, callee_scope);
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
    ips->call_seen = TRUE;
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

  aufp = symbol_for(fp)->variant.field.anonymous_parent_object
                       ->variant.field.ptr;
  aufp_sym = symbol_for(aufp);
  get_mapped_byte_count(&persistent_map, aufp, offset);
  if (aufp_sym != NULL &&
      aufp_sym->variant.field.anonymous_parent_object != NULL) {
    /* aufp is not the top-most anonymous union.  Recurse to determine its
       offset, and then replace it by the top-most anonymous union. */
    offset += record_anon_union_active_field(&aufp, storage, complete_obj);
  }  /* if */
  mark_subobject_initialized(storage+offset, complete_obj);  
  *(a_field_ptr*)(storage+offset) = fp;
  *p_fp = aufp;
  return offset;
}  /* record_anon_union_active_field */


static a_boolean do_constexpr_ctor(an_interpreter_state  *ips,
                                   a_dynamic_init_ptr    dip,
                                   a_source_position     *pos,
                                   a_byte                *result_storage,
                                   a_byte                *complete_object,
                                   a_constexpr_address   *implied_src)
/*
Interpret the constructor call represented by the given dynamic initialization
entry.  Return TRUE if no error occurred; otherwise, return FALSE and update
*ips accordingly.  pos is the position of the call.  The object is constructed
at the location indicated by result_storage, which is within the given complete
object.  If implied_src is non-NULL, this is a copy/move constructor invocation
and the source object is stored at the location indicated by implied_src.

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
    set_instance_required(symbol_for(callee), TRUE, SIR_CONSTANT_CONTEXT);
  }  /* if */
  if (callee->function_def_number == NULL_function_def_number) {
    info_with_pos_sym(ec_constexpr_function_undefined, pos,
                      symbol_for(callee), ips);
    do_constexpr_fail(result);
  } else if (callee->is_prototype_instantiation) {
    info_with_pos_sym(ec_constexpr_call_not_interpretable, pos,
                      symbol_for(callee), ips);
    do_constexpr_fail(result);
  } else if (cost_exceeded(ips)) {
    more_info_diagnostic(ec_excessive_constexpr_complexity, &ips->position,
                         &ips->diag_list);
    do_constexpr_fail(result);
  } else {
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
    alloc_stack_bytes(ips, n_args*sizeof(a_byte*), arg_ptrs);
    alloc_stack_bytes(ips, n_args*sizeof(a_byte_count), arg_sizes);
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
    p_arg_ptr = (a_byte**)arg_ptrs+1;
    arg_size = (a_byte_count*)arg_sizes;
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
            arg->variant.operation.operands->kind ==
                                         (an_expr_node_kind)enk_reuse_value) {
          /* The interpreter does not generally handle enk_reuse_value nodes.
             There is only one standard use for them and that is in some
             invocations of the std::initializer_list constructor: That use is
             handled as a special case here. */
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
          arg2->address += arg2->length * elem_size;
          arg2->flags |= CA_CANNOT_DEREFERENCE;
        } else if (!do_constexpr_expression(ips, arg, arg_bytes, arg_bytes)) {
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
    if (implied_src != NULL) {
      a_byte_count  n_bytes = sizeof(a_constexpr_address);
      a_byte        *arg_bytes;
      do_host_alignment(n_bytes);
      *arg_size = n_bytes;
      n_bytes += sizeof(a_var_postfix);
      alloc_complete_object(ips, n_bytes, params->type, arg_bytes);
      *(a_constexpr_address*)arg_bytes = *implied_src;
      mark_complete_object_initialized(arg_bytes);
      *p_arg_ptr = arg_bytes;
    }  /* if */
    /* Phase 2: Map the parameters to the arguments. */
    /* First map the "this" pointer. */
    alloc_seq_number = ips->curr_alloc_seq_number++;
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
      a_type_ptr    this_type = skip_typerefs(this_var->type);
      a_byte_count  this_n_bytes = sizeof(a_constexpr_address);
      a_byte_count   with_postfix_bytes;
      a_var_postfix  *postfix;
      do_host_alignment(this_n_bytes);
      with_postfix_bytes = this_n_bytes+sizeof(a_var_postfix);
      alloc_complete_object(ips, with_postfix_bytes, this_type, this_bytes);
      clear_address(this_bytes, result_storage);
      ((a_constexpr_address *)this_bytes)->complete_object = complete_object;
      ((a_constexpr_address *)this_bytes)->alloc_seq_number = alloc_seq_number;
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
      a_byte_count        offset;
      a_dynamic_init_ptr  sub_dip;
      a_type_ptr          tp;
      a_boolean           record_param_ref = FALSE;
      if (ctor_init->kind == (a_constructor_init_kind)cik_field) {
        a_field_ptr  fp = ctor_init->variant.field;
        tp = skip_typerefs(fp->type);
        get_mapped_byte_count(&persistent_map, fp, offset);
        if (ctor_init->use_field_initializer) {
          sub_dip = fp->initializer;
          /* Field initializers may contain enk_param_ref nodes representing
             "this": We provide a mapping for those below. */
          record_param_ref = TRUE;
        } else {
          sub_dip = ctor_init->initializer;
        }  /* if */
        if (symbol_for(fp)->variant.field.anonymous_parent_object != NULL) {
          /* ctor-init may point directly to an anonymous union field.  In
             that case, the active fields of all intervening anonymous unions
             must be recorded, offset must be adjusted, and fp must be set to
             the top-level anonymous union parent field, so that it can be
             recorded as the active field in case class_type itself is a union
             (see below). */
          offset += record_anon_union_active_field(&fp, result_storage,
                                                   complete_object);
        }  /* if */
        if (class_type->kind == (a_type_kind)tk_union) {
          /* Record the active field for the enclosing union. */
          *(a_field_ptr*)result_storage = fp;
        }  /* if */
        if (tp->kind == (a_type_kind)tk_union) {
          /* For union subobjects, make sure the active field is cleared
             initially.  (It may never be changed if the union has no
             fields.) */
          *(void**)(result_storage+offset) = NULL;
        }  /* if */
      } else if (ctor_init->kind == (a_constructor_init_kind)cik_delegation) {
        result = do_constexpr_dynamic_init(ips, ctor_init->initializer, pos,
                                           result_storage, complete_object);
        break;
      } else {
        a_base_class_ptr  bcp = ctor_init->variant.base_class;
        tp = bcp->type;
        get_mapped_byte_count(&persistent_map, bcp, offset);
        /* Record the derivation step. */
        record_subobject_derivation(result_storage+offset, bcp);
        sub_dip = ctor_init->initializer;
      }  /* if */
      if (sub_dip->kind == (a_dynamic_init_kind)dik_bitwise_copy &&
          sub_dip->variant.bitwise_copy.source == NULL) {
        /* An implicit member copy in a copy constructor.  arg_ptrs[1] points
           to the first argument of the copy constructor, which is a reference
           to the copied object. */
        a_constexpr_address  *src_addr;
        src_addr = (a_constexpr_address*)((a_byte**)arg_ptrs)[1];
        if (is_runtime_data_address(src_addr)) {
          info_with_pos(ec_constexpr_access_to_runtime_storage,
                        &args->position, ips);
          do_constexpr_fail(result);
          break;
        } else {
          if (!constexpr_copy_object(ips, tp, src_addr->address+offset,
                                     result_storage+offset, complete_object)) {
            do_constexpr_fail(result);
            break;
          }  /* if */
        }  /* if */
      } else {
        if (sub_dip->kind == (a_dynamic_init_kind)dik_constructor &&
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
                          &args->position, ips);
            do_constexpr_fail(result);
            break;
          } else {
            a_constexpr_address  adjusted_src_addr;
            adjusted_src_addr = *src_addr;
            adjusted_src_addr.address += offset;
            if (!do_constexpr_ctor(ips, sub_dip,
                                   &callee->source_corresp.decl_position,
                                   result_storage+offset, complete_object,
                                   &adjusted_src_addr)) {
              do_constexpr_fail(result);
              break;
            } else {
              mark_subobject_initialized(result_storage+offset,
                                         complete_object);
            }  /* if */
          }  /* if */
        } else if (sub_dip->kind == (a_dynamic_init_kind)dik_zero ||
                   sub_dip->kind == (a_dynamic_init_kind)dik_none) {
          /* Just zero the storage (for the dik_zero case) and record the
             derivation structure (which is needed even for the dik_none
             case). */
          init_subobject_to_zero(ips, result_storage+offset, tp,
                                 complete_object);
        } else {
          if (record_param_ref) {
            /* Associate the "this" pointer value (arbitrarily) with
               &ips->curr_call_frame. */
            map_stack_bytes(ips, &ips->curr_call_frame, this_bytes);
          }  /* if */
          if (!do_constexpr_dynamic_init(
                                    ips, sub_dip,
                                    &callee->source_corresp.decl_position,
                                    result_storage+offset, complete_object)) {
            do_constexpr_fail(result);
            break;
          } else {
            mark_subobject_initialized(result_storage+offset, complete_object);
          }  /* if */
          if (record_param_ref) {
            unmap_stack_bytes(ips, &ips->curr_call_frame);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    /* Run the function's top-level block statement. */
    if (!result) {
      /* Something went wrong.  Don't perform additional interpretation. */
    } else if (block_stmt->kind != (a_statement_kind)stmk_block) {
      check_assertion(block_stmt->kind == (a_statement_kind)stmk_try_block);
      info_with_pos(ec_constexpr_try_block, &block_stmt->position, ips);
      unexpected_condition();
    } else {
      result = do_constexpr_block_statement(ips, block_stmt, callee_scope);
    }  /* if */
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
    }
    remove_from_live_set(&ips->live_set, alloc_seq_number);
    /* Reduce the cost of the call to just 1. */
    ips->cost -= up_front_cost-2;
    ips->call_seen = TRUE;
  }  /* if */
done:
  return result;
}  /* do_constexpr_ctor */


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
  return new_con;
}  /* make_interpreter_copy_of_constant */


static a_boolean get_value_from_address_constant(
                                               an_interpreter_state  *ips,
                                               a_constant_ptr        addr_con,
                                               a_byte                *value)
/*
If addr_con is the address of a constant, copy it into the interpreter
storage at value and return TRUE.  Otherwise, return FALSE.
*/
{
  a_constant_ptr val_con = local_constant();
  a_boolean      result;

  if (constant_value_at_address(addr_con, val_con)) {
    /* Copy the constant value. */
    result = copy_val_from_constant(ips, val_con, value, value);
  } else {
    result = FALSE;
  }  /* if */
  release_local_constant(&val_con);
  return result;
}  /* get_value_from_address_constant */


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
    default:
      do_constexpr_fail(result);
      info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                    &expr->position, ips);
  }  /* switch */
  return result;
}  /* do_constexpr_builtin_operation */


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
                                ips, cap->variant.addr_con, result_storage)) {
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
  } else if (!is_initialized(cap)) {
    do_constexpr_fail(result);
    info_with_pos(ec_object_not_initialized, &expr->position, ips);
  } else if (is_variant_path(cap) &&
             !check_variant_path(ips, cap, /*release=*/TRUE,
                                 &expr->position)) {
    /* An attempt to dereference an inactive variant path. */
    do_constexpr_fail(result);
  } else {
    result = TRUE;
    (void)memcpy(result_storage, value_bytes_at(cap), size_t_arg(n_bytes));
    if (result_storage == complete_object) {
      /* Mark the destination storage as fully initialized. */
      mark_complete_object_initialized(complete_object);
    }  /* if */
    if (is_immediate_class_type(tp) || tp->kind == (a_type_kind)tk_array) {
      mark_whole_subobject_initialized(ips, result_storage, tp,
                                       complete_object);
    } else if (tp->kind == (a_type_kind)tk_pointer) {
      /* If a pointer value is loaded from a glvalue, give the copy its own
         address structures (so the original will not be freed when the copy
         is freed). */
      copy_address_structures(result_storage);
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
value is the result of evaluating expr, which has the given type (after
skipping typerefs).  The expression is used as a boolean condition: If
that condition can be determined to be true or false, set *p_cond to that
condition and return TRUE.  Otherwise, return FALSE and record a diagnostic.
*/
{
  a_boolean  result = TRUE;

  if (tp->kind == (a_type_kind)tk_integer) {
    a_host_large_integer  bool_val;
    a_boolean             ovflo;
    get_int_val_from(value, tp, bool_val, ovflo);
    *p_cond = ovflo || bool_val;
  } else if (tp->kind == (a_type_kind)tk_pointer) {
    a_constexpr_address  *cap = (a_constexpr_address*)value;
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
  } else if (tp->kind == (a_type_kind)tk_float) {
    a_boolean  unord;
    if (fp_compare(tp->variant.float_kind,
                   fp_value(value),
                   &zero_flt[(int)tp->variant.float_kind],
                   &unord) == 0) {
      *p_cond = FALSE;
    } else {
      *p_cond = TRUE;
    }  /* if */
  } else if (tp->kind == (a_type_kind)tk_nullptr) {
    *p_cond = FALSE;
  } else if (tp->kind == (a_type_kind)tk_ptr_to_member) {
    a_constexpr_ptr_to_mem
                       *pm = (a_constexpr_ptr_to_mem*)value;
    if ((pm->is_ptr_to_mem_function ? (void*)pm->variant.routine
                                    : (void*)pm->variant.field)
                                                        == NULL) {
      *p_cond = FALSE;
    } else {
      *p_cond = TRUE;
    }  /* if */
  } else {
    *p_cond = TRUE;
    do_constexpr_fail(result);
    unexpected_condition();
  }  /* if */
  return result;
}
 

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
}  /* normalize_runtime_addresses_if_possible */


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
  } else if (dst_type->kind == (a_type_kind)tk_array) {
    a_type_ptr  etp = underlying_array_element_type(dst_type);
    etp = skip_typerefs(etp);
    if (identical_types_ignoring_qualifiers(src_type, etp) &&
        is_array_element(cap)) {
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
  a_byte_count         n_bytes = value_bytes_for_type(ips, tp, &result);
  an_integer_kind      int_kind;
  a_boolean            is_signed;
  a_host_large_integer host_int_val;

  if (!result) goto done;
  if (expr->do_not_interpret) {
    info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                  &expr->position, ips);
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
        a_type_ptr       opnd2_type;
        a_byte           *opnd2_value;
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
             cmp_integer_values((an_integer_value *)(val), is_signed,         \
                                &max_integer_value_of_kind[int_kind],         \
                                is_signed) > 0 ||                             \
             (is_signed &&                                                    \
              cmp_integer_values((an_integer_value *)(val), is_signed,        \
                                 &min_integer_value_of_kind[int_kind],        \
                                 is_signed) < 0)) {                           \
    do_constexpr_fail(result);                                                \
    info_with_pos_type(ec_constexpr_integer_overflow, &expr->position, tp,    \
                       ips);                                                  \
  }  /* if */                                                                 \
}  /* CHECK_int_range */

        opnd1 = expr->variant.operation.operands;
        opnd2 = opnd1->next;
        opnd1_type = skip_typerefs(opnd1->type);
        opnd_n_bytes = expr_result_size(ips, opnd1, opnd1_type, &result);
        alloc_complete_object(ips, opnd_n_bytes, opnd1_type, opnd1_value);
        if (result &&
            !do_constexpr_expression(ips, opnd1, opnd1_value, opnd1_value)) {
          do_constexpr_fail(result);
        }  /* if */
        if (result && opnd2 != NULL &&
            !node_operator_is(expr, eok_land) &&
            !node_operator_is(expr, eok_lor) &&
            !node_operator_is(expr, eok_question) &&
            !node_operator_is(expr, eok_comma) &&
            !node_operator_is(expr, eok_dot_static) &&
            !node_operator_is(expr, eok_points_to_static)) {
          /* Evaluate the second operand.  For short-circuiting operators,
             whether to evaluate the second operand will be decided below in
             the specific code for each such operator.  The comma operator can
             be handled similarly (although the evaluation is unconditional in
             that case); eok_dot_static and eok_points_to_static are equivalent
             to the comma operator in this respect. */
          opnd2_type = skip_typerefs(opnd2->type);
          opnd_n_bytes = expr_result_size(ips, opnd2, opnd2_type, &result);
          alloc_complete_object(ips, opnd_n_bytes, opnd2_type, opnd2_value);
          if (result &&
              !do_constexpr_expression(ips, opnd2, opnd2_value, opnd2_value)) {
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
              if (tp->kind == (a_type_kind)tk_integer) {
                /* Integers: Somewhat surprisingly, narrowing conversions are
                   valid here ("implementation-defined").  So we don't check
                   that the result is in range. */
                an_integer_value  *r_int = (an_integer_value *)result_storage;
                *r_int = *(an_integer_value *)opnd1_value;
                int_kind = tp->variant.integer.int_kind;
                if (int_kind_is_signed[int_kind]) {
                  if (int_kind_is_signed[
                                      opnd1_type->variant.integer.int_kind]) {
                    /* When converting a signed value to another signed value,
                       be sure to sign-extend the result (otherwise, widening
                       conversions would produce positive values from negative
                       operands. */
                    int  n_bits = (int)(opnd1_type->size*CHAR_BIT);
                    sign_extend_integer_value(r_int, n_bits);
                  }  /* if */
                } else {
                  /* Truncate the unsigned result, in case this is a narrowing
                     conversion. */
                  and_integer_values(r_int,
                                     &max_integer_value_of_kind[int_kind]);
                }  /* if */
              } else if (tp->kind == (a_type_kind)tk_float) {
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
              } else if (tp->kind == (a_type_kind)tk_pointer) {
                a_type_ptr  utp1 = skip_typerefs(tp->variant.pointer.type);
                a_type_ptr  utp2;
                utp2 = skip_typerefs(opnd1_type->variant.pointer.type);
                if (identical_types(utp1, utp2) ||
                    utp1->kind == (a_type_kind)tk_void) {
                  /* E.g., a conversion from X* to X const* or X* to void*. */
                  *(a_constexpr_address *)result_storage =
                                          *(a_constexpr_address *)opnd1_value;
                } else {
                  info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                      &expr->position, opnd1_type, tp, ips);
                  do_constexpr_fail(result);
                }  /* if */
              } else if (tp->kind == (a_type_kind)tk_void) {
                release_address_structures(opnd1, opnd1_type, opnd1_value);
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
            } else {
              do_constexpr_fail(result);
              info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                  &expr->position, opnd1_type, tp, ips);
            }  /* if */
            break;
          case eok_lvalue_cast:
          case eok_ref_cast:
          case eok_lvalue_adjust:
            /* If the type (other than qualification) doesn't change, this is
               is not a reinterpret-like cast and we can interpret the result.
               In the case of casting a function lvalue to a reference to
               function type, it is possible that the type of this node was
               later "decayed" to a pointer-to-function type (to match the
               expectations of a parent node); that case is valid, too. */
            if (expr->variant.operation.is_reinterpret_cast ||
                (!identical_types_ignoring_qualifiers(tp, opnd1_type) &&
                 !acceptable_lvalue_conversion(
                    ips, (a_constexpr_address*)opnd1_value, opnd1_type, tp))) {
              info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                  &expr->position, opnd1_type, tp, ips);
              do_constexpr_fail(result);
            } else {
              SET_result_val_from_operand_address(opnd1_value);
            }  /* if */
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
              if (bcp == NULL || bcp->is_virtual) {
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
                /* Temporarily clear the backing expression to avoid
                   maintaining it at this stage. */
                an_expr_node_ptr  backing_expr = addr_con->expr;
                addr_con->expr = NULL;
                fold_base_class_cast(
                    addr_con, bcp, btp, new_con,
                    /*check_cast_access=*/TRUE, /*check_ambiguity=*/TRUE,
                    expr->variant.operation.compiler_generated,
                    /*is_object_pointer=*/(result_addr->length != 0),
                    &nonconstant, &expr->position, &err_code);
                addr_con->expr = backing_expr;
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
              if (!result) break;
              if (constexpr_copy_object(ips, tp, opnd1_value+offset,
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
              } else if (src->address == NULL) {
                *(a_constexpr_address*)result_storage = *src;
              } else {
                a_base_class_ptr  bcp = *(a_base_class_ptr*)src->address;
                if (bcp != NULL && bcp->derived_class == tp) {
                  a_byte_count  offset;
                  get_mapped_byte_count(&persistent_map, bcp, offset);
                  src->address -= offset;
                  SET_result_val_from_operand_address(src);
                } else {
                  a_type_ptr  derived_class;
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
              } else {
                /* The somewhat unusual case of an array rvalue. */
                clear_address(result_addr, opnd1_value);
              }  /* if */
              result_addr->flags |= CA_ARRAY_ELEMENT;
              /* Check that the array length fits in interpreter limits. */
              length = opnd1_type->variant.array.variant.number_of_elements;
              if (length <= MAX_ARRAY_LENGTH) {
                result_addr->length = length;
                if (is_runtime_data_address(result_addr)) {
                  a_constant_ptr  orig_con = result_addr->variant.addr_con;
                  a_constant_ptr  new_con;
                  new_con = make_interpreter_copy_of_constant(ips, orig_con);
                  new_con->type = tp;
                  result_addr->variant.addr_con = new_con;
                } else if (is_variant_path(result_addr)) {
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
            info_with_pos(ec_constexpr_vacuous_dtor_call,
                          &expr->position, ips);
            do_constexpr_fail(result);
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
            } else if (opnd1_type->kind == (a_type_kind)tk_float) {
              err = FALSE;
              fp_negate(opnd1_type->variant.float_kind,
                        fp_value(opnd1_value), fp_value(result_storage),
                        &err, &depends_on_fp_mode);
              if (err) {
                /* fp_negate should never fail. */
                unexpected_condition();
              }  /* if */
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
            } else if (opnd1_type->kind == (a_type_kind)tk_float) {
              *fp_value(result_storage) = *fp_value(opnd1_value);
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
                    trim_bit_field_if_needed(cap);
                  }  /* if */
                } else if (tp->kind == (a_type_kind)tk_float) {
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
                  a_type_ptr           elem_type;
                  a_byte_count         elem_size;
                  ptr = (a_constexpr_address*)value_bytes_at(cap);
                  if (cannot_dereference(ptr)) {
                    /* Invalid pointer value. */
                    info_with_pos(ec_constexpr_invalid_pointer,
                                  &expr->position, ips);
                    do_constexpr_fail(result);
                    break;
                  }  /* if */
                  elem_type = skip_typerefs(opnd1_type->variant.pointer.type);
                  elem_size = value_bytes_for_type(ips, elem_type, &result);
                  ptr->address += elem_size;
                  if (!is_array_element(ptr)) {
                    /* The address of a non-array can be treated as a pointer
                       to an array of one element. */
                    ptr->flags |= CA_CANNOT_DEREFERENCE;
                  } else {
                    a_byte        *base_address;
                    base_address = get_base_address(ptr);
                    if (ptr->address == base_address + ptr->length*elem_size) {
                      /* We've reached "one past the end of the array". */
                      ptr->flags |= CA_CANNOT_DEREFERENCE;
                    }    /* if */
                  }  /* if */
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
                  trim_bit_field_if_needed(cap);
                } else if (tp->kind == (a_type_kind)tk_float) {
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
                  a_type_ptr           elem_type;
                  a_byte_count         elem_size;
                  ptr = (a_constexpr_address*)value_bytes_at(cap);
                  if (!is_array_element(ptr)) {
                    /* Not a pointer to an array element in interpreter
                       storage. */
                    if (!cannot_dereference(ptr)) {
                      info_with_pos(ec_constexpr_invalid_pointer,
                                    &expr->position, ips);
                      do_constexpr_fail(result);
                      break;
                    }  /* if */
                  } else {
                    if (ptr->address == get_base_address(ptr)) {
                      /* The pointer cannot point ahead of the array. */
                      info_with_pos(ec_constexpr_invalid_pointer,
                                    &expr->position, ips);
                      do_constexpr_fail(result);
                      break;
                    }  /* if */
                  }  /* if */
                  elem_type = skip_typerefs(opnd1_type->variant.pointer.type);
                  elem_size = value_bytes_for_type(ips, elem_type, &result);
                  ptr->address -= elem_size;
                  ptr->flags &= ~CA_CANNOT_DEREFERENCE;
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
              } else if (is_variant_path(cap) &&
                         !check_variant_path(ips, cap, /*release=*/TRUE,
                                             &expr->position)) {
                /* An attempt to dereference an inactive variant path. */
                do_constexpr_fail(result);
              } else if (is_const_storage(cap)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (!is_initialized(cap)) {
                do_constexpr_fail(result);
                info_with_pos(ec_object_not_initialized, &expr->position, ips);
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
                  trim_bit_field_if_needed(cap);
                }  /* if */
              } else if (tp->kind == (a_type_kind)tk_float) {
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
                a_type_ptr           elem_type;
                a_byte_count         elem_size;
                ptr = (a_constexpr_address*)value_bytes_at(cap);
                if (cannot_dereference(ptr)) {
                  /* Invalid pointer value. */
                  info_with_pos(ec_constexpr_invalid_pointer,
                                &expr->position, ips);
                  do_constexpr_fail(result);
                  break;
                }  /* if */
                elem_type = skip_typerefs(opnd1_type->variant.pointer.type);
                elem_size = value_bytes_for_type(ips, elem_type, &result);
                ptr->address += elem_size;
                if (!is_array_element(ptr)) {
                  /* The address of a non-array can be treated as a pointer to
                     an array of one element. */
                  ptr->flags |= CA_CANNOT_DEREFERENCE;
                } else {
                  a_byte  *base_address = get_base_address(ptr);
                  if (ptr->address == base_address + ptr->length*elem_size) {
                    /* We've reached "one past the end of the array". */
                    ptr->flags |= CA_CANNOT_DEREFERENCE;
                  }  /* if */
                }  /* if */
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
              } else if (is_variant_path(cap) &&
                         !check_variant_path(ips, cap, /*release=*/TRUE,
                                             &expr->position)) {
                /* An attempt to dereference an inactive variant path. */
                do_constexpr_fail(result);
              } else if (is_const_storage(cap)) {
                info_with_pos(ec_constexpr_modifying_const_storage,
                              &expr->position, ips);
                do_constexpr_fail(result);
              } else if (!is_initialized(cap)) {
                do_constexpr_fail(result);
                info_with_pos(ec_object_not_initialized, &expr->position, ips);
              } else if (tp->kind == (a_type_kind)tk_integer) {
                /* An integer. */
                an_integer_value  *ival = int_value_at(cap);
                int_kind = tp->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                subtract_mixed_signed_integer_values(
                                 ival, is_signed, &one_int, is_signed, &ovfl);
                CHECK_int_range(ival, tp);
                trim_bit_field_if_needed(cap);
              } else if (tp->kind == (a_type_kind)tk_float) {
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
                a_type_ptr           elem_type;
                a_byte_count         elem_size;
                ptr = (a_constexpr_address*)value_bytes_at(cap);
                if (!is_array_element(ptr)) {
                  /* Not a pointer to an array element in interpreter
                     storage. */
                  if (!cannot_dereference(ptr)) {
                    info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                                  &expr->position, ips);
                    do_constexpr_fail(result);
                  }  /* if */
                } else {
                  if (ptr->address == get_base_address(ptr)) {
                    /* The pointer cannot point ahead of the array. */
                    do_constexpr_fail(result);
                    info_with_pos(ec_constexpr_pointer_ahead_of_array,
                                  &expr->position, ips);
                    break;
                  }  /* if */
                }  /* if */
                elem_type = skip_typerefs(opnd1_type->variant.pointer.type);
                elem_size = value_bytes_for_type(ips, elem_type, &result);
                ptr->address -= elem_size;
                ptr->flags &= ~CA_CANNOT_DEREFERENCE;
              } else {
                /* Invalid type for prefix --. */
                unexpected_condition();
              }  /* if */
              if (result) {
                /* Return either the address or the value, as
                   appropriate. */
                SET_result_val_from_operand_address(cap);
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
            } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
              fp_add(tp->variant.float_kind,
                     fp_value(opnd1_value), fp_value(opnd2_value),
                     fp_value(result_storage), &err, &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
            } else {
              /* Other types. */
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
            } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
              fp_subtract(tp->variant.float_kind,
                          fp_value(opnd1_value), fp_value(opnd2_value),
                          fp_value(result_storage), &err,
                          &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
            } else {
              /* Other types. */
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
            } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
              fp_multiply(tp->variant.float_kind,
                          fp_value(opnd1_value), fp_value(opnd2_value),
                          fp_value(result_storage), &err,
                          &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
            } else {
              /* Other types. */
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
            } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
              fp_divide(tp->variant.float_kind,
                        fp_value(opnd1_value), fp_value(opnd2_value),
                        fp_value(result_storage), &err,
                        &depends_on_fp_mode);
              if (err) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
              }  /* if */
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
                                       (unsigned long)(pos+host_int_val),
                                       (unsigned long)len, ips);
                  } else {
                    info_with_pos(ec_constexpr_pointer_ahead_of_array,
                                  &expr->position, ips);
                  }  /* if */
                } else {
                  if (is_runtime_data_address(result_addr)) {
                    result_addr->variant.addr_con->variant.address.offset +=
                      host_int_val * elem_size;
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
                                       (unsigned long)(pos-host_int_val),
                                       (unsigned long)len, ips);
                  } else {
                    info_with_pos(ec_constexpr_pointer_ahead_of_array,
                                  &expr->position, ips);
                  }  /* if */
                } else {
                  if (is_runtime_data_address(result_addr)) {
                    result_addr->variant.addr_con->variant.address.offset -=
                      host_int_val * elem_size;
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
              } else if (is_array_element(addr1) && is_array_element(addr2) &&
                         get_base_address(addr1) == get_base_address(addr2)) {
                a_type_ptr    etp = opnd1_type->variant.pointer.type;
                a_byte_count  elem_size = value_bytes_for_type(
                                                           ips, etp, &result);
                if (!result) {
                  /* Nothing more to do. */
                } else if (elem_size == 0) {
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
              } else if (addr1->address == NULL && addr2->address == NULL) {
                /* Subtracting two null pointers produces zero. */
                *(an_integer_value *)result_storage = zero_int;
              } else {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_non_array_pointer_arithmetic,
                              &expr->position, ips);
              }  /* if */
            }
            break;
          case eok_shiftl:
            /* Check for a valid value of opnd2, which must be non-negative
               and less than the number of bits in opnd1. */
            int_kind = tp->variant.integer.int_kind;
            is_signed = int_kind_is_signed[int_kind];
            get_int_val_from(opnd2_value, opnd2_type, host_int_val, ovfl);
            if (ovfl) {
              do_constexpr_fail(result);
            } else if (host_int_val < 0 ||
                       host_int_val >=
                           (a_host_large_integer)(tp->size * CHAR_BIT)) {
              do_constexpr_fail(result);
            }  /* if */
            if (result) {
              shift_left_integer_value((an_integer_value *)opnd1_value,
                                       (int)host_int_val, &ovfl);
              CHECK_int_range(opnd1_value, opnd1_type);
              if (result) {
                *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              }  /* if */
            } else {
              info_with_pos(ec_integer_overflow, &expr->position, ips);
            }  /* if */
            break;
          case eok_shiftr:
            /* Check for a valid value of opnd2, which must be non-negative
               and less than the number of bits in opnd1. */
            int_kind = tp->variant.integer.int_kind;
            is_signed = int_kind_is_signed[int_kind];
            get_int_val_from(opnd2_value, opnd2_type, host_int_val, ovfl);
            if (ovfl) {
              do_constexpr_fail(result);
            } else if (host_int_val < 0 ||
                       host_int_val >=
                           (a_host_large_integer)(tp->size * CHAR_BIT)) {
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
              info_with_pos(ec_integer_overflow, &expr->position, ips);
            }  /* if */
            break;
          case eok_and:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              and_integer_values((an_integer_value*)result_storage,
                                 (an_integer_value*)opnd2_value);
            }  /* if */
            break;
          case eok_or:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              or_integer_values((an_integer_value*)result_storage,
                                (an_integer_value*)opnd2_value);
            }  /* if */
            break;
          case eok_xor:
            if (expr->variant.operation.type_kind == (a_type_kind)tk_integer) {
              *(an_integer_value *)result_storage =
                                             *(an_integer_value *)opnd1_value;
              xor_integer_values((an_integer_value*)result_storage,
                                 (an_integer_value*)opnd2_value);
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
            } else if (opnd1_type->kind == (a_type_kind)tk_float) {
              /* Floating-point operands. */
              if (fp_compare(opnd1_type->variant.float_kind,
                             fp_value(opnd1_value),
                             fp_value(opnd2_value),
                             &unord) == 0) {
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
                  if (eq_constants(ptr1->variant.addr_con,
                                   ptr2->variant.addr_con)) {
                    *(an_integer_value *)result_storage = one_int;
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
            } else if (opnd1_type->kind == (a_type_kind)tk_float) {
              /* Floating-point operands. */
              if (fp_compare(opnd1_type->variant.float_kind,
                             fp_value(opnd1_value),
                             fp_value(opnd2_value),
                             &unord) != 0) {
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
              if (compatible_address_kinds(ptr1, ptr2)) {
                if (is_function_address(ptr1) || is_function_address(ptr2)) {
                  if (is_function_address(ptr2) && is_function_address(ptr2) &&
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
                  if (!eq_constants(ptr1->variant.addr_con,
                                    ptr2->variant.addr_con)) {
                    *(an_integer_value *)result_storage = one_int;
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
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
          case eok_assign:
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
                          !check_variant_path(ips, dst, /*release=*/TRUE,
                                              &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else {
                /* Copy the value of the right operand to the indicated
                   address and return either the address or the value, as
                   appropriate. */
                a_byte  *dst_storage = value_bytes_at(dst);
                (void)memcpy(dst_storage, opnd2_value, size_t_arg(n_bytes));
                if (tp->kind == (a_type_kind)tk_pointer) {
                  /* Copying a pointer type.  Make sure its side structures,
                     if any, are not shared. */
                  copy_address_structures(dst_storage);
                } else {
                  trim_bit_field_if_needed(dst);
                }  /* if */
                if (expr->is_lvalue || expr->is_xvalue ||
                    is_function_address(dst)) {
                  /* The assignment produces an lvalue-like result. */
                  *(a_constexpr_address*)result_storage = *dst;
                } else {
                  /* The assignment produces an rvalue.  Copy the value once
                     more. */
                  (void)memcpy(result_storage, dst_storage,
                               size_t_arg(n_bytes));
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
                          !check_variant_path(ips, dst, /*release=*/TRUE,
                                              &expr->position))) {
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
                  trim_bit_field_if_needed(dst);
                  CHECK_int_range(int_value_at(dst), tp);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
                  an_internal_float_value  *dst_val = fp_value_at(dst);
                  fp_add(tp->variant.float_kind, dst_val,
                         fp_value(opnd2_value), dst_val, &err,
                         &depends_on_fp_mode);
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
                          !check_variant_path(ips, dst, /*release=*/TRUE,
                                              &expr->position))) {
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
                  trim_bit_field_if_needed(dst);
                  CHECK_int_range(int_value_at(dst), tp);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
                  an_internal_float_value  *dst_val = fp_value_at(dst);
                  fp_subtract(tp->variant.float_kind, dst_val,
                              fp_value(opnd2_value), dst_val, &err,
                              &depends_on_fp_mode);
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
                          !check_variant_path(ips, dst, /*release=*/TRUE,
                                              &expr->position))) {
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
                  trim_bit_field_if_needed(dst);
                  CHECK_int_range(int_value_at(dst), tp);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                } else if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_float) {
                  an_internal_float_value  *dst_val = fp_value_at(dst);
                  fp_multiply(tp->variant.float_kind, dst_val,
                              fp_value(opnd2_value), dst_val, &err,
                              &depends_on_fp_mode);
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
                          !check_variant_path(ips, dst, /*release=*/TRUE,
                                              &expr->position))) {
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
                  trim_bit_field_if_needed(dst);
                  CHECK_int_range(int_value_at(dst), tp);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
                  an_internal_float_value  *dst_val = fp_value_at(dst);
                  fp_divide(tp->variant.float_kind, dst_val,
                            fp_value(opnd2_value), dst_val, &err,
                            &depends_on_fp_mode);
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
                          !check_variant_path(ips, dst, /*release=*/TRUE,
                                              &expr->position))) {
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
                trim_bit_field_if_needed(dst);
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
                          !check_variant_path(ips, dst, /*release=*/TRUE,
                                              &expr->position))) {
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
                          (a_host_large_integer)(tp->size * CHAR_BIT)) {
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
                  trim_bit_field_if_needed(dst);
                  CHECK_int_range(int_value_at(dst), tp);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                }  /* if */
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
                          !check_variant_path(ips, dst, /*release=*/TRUE,
                                              &expr->position))) {
                /* Attempting to store into a non-active variant field. */
                do_constexpr_fail(result);
              } else if (ips->side_effects_disabled) {
                /* Side-effects (like assignments) are disabled. */
                do_constexpr_fail(result);
              } else {
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
                          (a_host_large_integer)(tp->size * CHAR_BIT)) {
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
                  trim_bit_field_if_needed(dst);
                  if (expr->is_lvalue || expr->is_xvalue) {
                    /* The assignment produces an lvalue-like result. */
                    *(a_constexpr_address*)result_storage = *dst;
                  } else {
                    /* The assignment produces an rvalue.  Copy the value. */
                    *(an_integer_value*)result_storage = *int_value_at(dst);
                  }  /* if */
                }  /* if */
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
                          !check_variant_path(ips, dst, /*release=*/TRUE,
                                              &expr->position))) {
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
                          !check_variant_path(ips, dst, /*release=*/TRUE,
                                              &expr->position))) {
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
                          !check_variant_path(ips, dst, /*release=*/TRUE,
                                              &expr->position))) {
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
                                      (unsigned long)(pos+host_int_val),
                                      (unsigned long)len, ips);
                      } else {
                        info_with_pos(ec_constexpr_pointer_ahead_of_array,
                                      &expr->position, ips);
                      }  /* if */
                    } else {
                      if (is_runtime_data_address(ptr_val)) {
                        ptr_val->variant.addr_con->variant.address.offset +=
                          host_int_val * elem_size;
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
                                      (unsigned long)(pos-host_int_val),
                                      (unsigned long)len, ips);
                    } else {
                      info_with_pos(ec_constexpr_pointer_ahead_of_array,
                                    &expr->position, ips);
                    }  /* if */
                  } else {
                    if (is_runtime_data_address(ptr_val)) {
                      ptr_val->variant.addr_con->variant.address.offset -=
                        host_int_val * elem_size;
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
            result = do_constexpr_expression(
                                 ips, opnd2, result_storage, complete_object);
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
                } else if (!is_array_element(&result_addr)) {
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
                                       (unsigned long)(pos+host_int_val),
                                       (unsigned long)len, ips);
                  } else {
                    if (is_runtime_data_address(&result_addr)) {
                      result_addr.variant.addr_con->variant.address.offset +=
                        host_int_val * elem_size;
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
              }  /* if */
              if (is_runtime_data_address(&result_addr)) {
                /* Attempt to compute a new offset for a run-time address
                   constant. */
                if (!(expr->is_lvalue || expr->is_xvalue) ||
                    field->is_bit_field) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else {
                  a_constant_ptr    new_con = local_constant(),
                                    addr_con = result_addr.variant.addr_con;
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
                  add_to_variant_path(&result_addr, field, opnd1_type);
                }  /* if */
                get_mapped_byte_count(&persistent_map, field, offset);
                result_addr.address += offset;
                result_addr.flags &= ~CA_ARRAY_ELEMENT;
                if (field->is_bit_field) {
                  /* Record that the lvalue is that of a bit field.  The
                     length and signedness of the field are encoded in
                     result_addr.length. */
                  result_addr.flags |= CA_BIT_FIELD;
                  result_addr.length = field->bit_size*2 + field->is_bit_field;
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
              result_addr = *(a_constexpr_address*)opnd1_value;
              pm_value = (a_constexpr_ptr_to_mem*)opnd2_value;
              field = pm_value->variant.field;
              if (field == NULL) {
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_null_ptr_to_member_data,
                              &expr->position, ips);
              } else if (is_runtime_data_address(&result_addr)) {
                if (!(expr->is_lvalue || expr->is_xvalue)) {
                  do_constexpr_fail(result);
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else {
                  a_constant_ptr    new_con = local_constant(),
                                    addr_con = result_addr.variant.addr_con;
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
                     the address is null.  That is needed to accomodate
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
                  add_to_variant_path(&result_addr, field, opnd1_type);
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
                                         field->is_bit_field;
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
          case eok_dot_static:
          case eok_points_to_static:
            result = do_constexpr_expression(
                                 ips, opnd2, result_storage, complete_object);
            break;
          case eok_question:
            { a_boolean  bool_val;
              if (!check_boolean_condition(ips, opnd1_value, opnd1, opnd1_type,
                                           &bool_val)) {
                do_constexpr_fail(result);
                break;
              }  /* if */
              if (!bool_val) {
                /* Evaluate the third operand. */
                opnd2 = opnd2->next;
              }  /* if */
              result = do_constexpr_expression(
                                 ips, opnd2, result_storage, complete_object);
            }
            break;
          case eok_call:
            /* Calls are handled separately.  We should not get here. */
            unexpected_condition();
            /*FALLTHROUGH*/
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
        if (tp->kind == (a_type_kind)tk_array &&
            (expr->is_lvalue || expr->is_xvalue)) {
          /* An array lvalue (normally: a string literal).  Allocate the
             string statically and return its address.  Make sure that
             multiple uses of the constant produce the same address. */
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
            if (!complete_object_is_initialized(var_bytes)) {
              info_with_pos(ec_object_not_initialized, &expr->position, ips);
              do_constexpr_fail(result);
            } else {
              (void)memcpy(result_storage, var_bytes, size_t_arg(n_bytes));
              if (tp->kind == (a_type_kind)tk_pointer) {
                /* Copying an address type.  Make sure its side structures, if
                   any, are not shared. */
                copy_address_structures(result_storage);
              } else if (is_immediate_class_type(tp) ||
                         tp->kind == (a_type_kind)tk_array) {
                /* Mark subobjects as initialized. */
                mark_whole_subobject_initialized(ips, result_storage, tp,
                                                 complete_object);
              }  /* if */
              if (complete_object == result_storage) {
                /* Mark the destination storage as fully initialized. */
                mark_complete_object_initialized(complete_object);
              }  /* if */
            }  /* if */
          } else {
            a_constant_ptr  con = var_constant_value(var);
            if (con != NULL) {
              result = copy_val_from_constant(ips, con, result_storage,
                                              result_storage);
            } else {
              if (var->is_this_parameter) {
                info_with_pos(ec_star_this_not_constant_valued,
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
            do_host_alignment(obj_size);
            clear_address(result_storage, var_bytes);
            /* Record the allocation sequence number for this variable in the
               address record. */
            cap->alloc_seq_number =
                     ((a_var_postfix*)(var_bytes+obj_size))->alloc_seq_number;
            if (is_const_qualified_type(var->type)) {
              cap->flags |= CA_CONST_STORAGE;
            }  /* if */
          } else {
            /* A reference to a run-time variable.  This may not be valid,
               but we cannot usually tell at this time. */
            a_constant_ptr  con;
            a_byte          *con_ptr;
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
            result = extract_value_from_constant(
                                    ips, con, result_storage, result_storage);
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
        if (!rp->is_prototype_instantiation) {
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
    case enk_temp_init:
      { a_dynamic_init_ptr     dip = expr->variant.init.dynamic_init;
        a_byte                 *tmp_bytes;
        an_alloc_seq_number    alloc_seq_number;
        a_byte_count           prefix_size;
        if (C_mode()) {
          info_with_pos(ec_constexpr_access_to_runtime_storage,
                        &expr->position, ips);
          do_constexpr_fail(result);
          break;
        }  /* if */
        if (expr->is_lvalue || expr->is_xvalue) {
          /* A glvalue temporary is expected.  I.e., the caller expects an
             interpreter address for the temporary object.  Allocate the
             storage for that object here. */
          a_constexpr_address  *cap;
          a_boolean            temp_lifetime = dip->has_temporary_lifetime;
          n_bytes = value_bytes_for_type(ips, tp, &result);
          if (!result) break;
          compute_prefix_size_for_type(tp, n_bytes, prefix_size);
          if (!temp_lifetime) {
            /* A lifetime-extended temporary.  Switch to the storage stack
               state that was saved at the time the stmk_init statement was
               started. */
            if (ips->extension_state != NULL) {
              alloc_bytes(ips->extension_state, n_bytes+prefix_size,
                          tmp_bytes);
              alloc_seq_number = ips->extension_state->alloc_seq_number;
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
          if (is_const_qualified_type(expr->type)) {
            cap->flags |= CA_CONST_STORAGE;
          }  /* if */
          if (tp->kind == (a_type_kind)tk_array) {
            /* We are referring to the array as a whole; not just one element
               of it.  Record the length in case it is needed later on. */
            cap->length = tp->variant.array.variant.number_of_elements;
          }  /* if */
          if (!temp_lifetime) {
            cap->flags |= CA_LIFETIME_EXTENDED;
	  }  /* if */
        } else {
          /* The consumer of the temporary expects an rvalue.  So we can
             evaluate the initialization directly into result_storage. */
          tmp_bytes = result_storage;
        }  /* if */
        if (dip->kind == (a_dynamic_init_kind)dik_zero &&
            dip->destructor == NULL) {
          init_subobject_to_zero(ips, tmp_bytes, tp, tmp_bytes);
        } else if (!do_constexpr_dynamic_init(
                           ips, dip, &expr->position, tmp_bytes, tmp_bytes)) {
          do_constexpr_fail(result);
        }  /* if */
        mark_complete_object_initialized(tmp_bytes);
      }
      break;
    case enk_object_lifetime:
      result = do_constexpr_expression(ips, expr->variant.object_lifetime.expr,
                                       result_storage, complete_object);
      break;
    case enk_typeid:
      if (expr->variant.typeid_info.expr == NULL &&
          (expr->is_lvalue || expr->is_xvalue)) {
        a_constant_ptr       cp = local_constant();
        a_constexpr_address  *cap = (a_constexpr_address*)result_storage;
        make_typeid_constant(expr->variant.typeid_info.type,
                             /*is_cli_typeid*/FALSE, cp);
        cp->next = ips->constants;
        ips->constants = cp;
        clear_runtime_constant_address(cap, cp);
      } else {
        /* Polymorphic typeid: not allowed in constant expressions. */
        info_with_pos(ec_constexpr_access_to_runtime_storage,
                      &expr->position, ips);
        do_constexpr_fail(result);
      }  /* if */
      break;
    case enk_param_ref:
      { a_byte  *this_bytes = NULL;
        if (ips->curr_call_frame != NULL &&
            expr->variant.param_ref.param_num == 0) {
          /* An entry representing "this" in a field initializer.  The code
             handling constructor calls (which initializes members based on
             field initializers when needed) associated the address of the
             "this" pointer variable for the constructor with
             &ips->curr_call_frame. */
          get_stack_bytes(ips, &ips->curr_call_frame, this_bytes);
        }  /* if */
        if (this_bytes != NULL) {
          (void)memcpy(result_storage, this_bytes, size_t_arg(n_bytes));
          copy_address_structures(result_storage);
          mark_complete_object_initialized(complete_object);
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
    default:
      do_constexpr_fail(result);
      info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                    &expr->position, ips);
  }  /* switch */
done:
  return result;
#undef set_result_from_opnd1
#undef within_int_range
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
  variant_path_entries = NULL;
  n_variant_path_entries = 0;
  free_variant_path_entries = NULL;
  n_free_variant_path_entries = 0;
  if (!useful_constants_initialized) {
    /* Initialize useful constants. */
    a_float_kind fk;
    a_boolean    dummy;
    set_integer_value(&zero_int, (a_host_large_integer)0);
    set_integer_value(&one_int, (a_host_large_integer)1);
    for (fk = (a_float_kind)fk_float; fk < (a_float_kind)fk_last; ++fk) {
      fp_host_large_integer_to_float(fk, (a_host_large_integer)0,
                                     &zero_flt[(int)fk], &dummy);
      fp_host_large_integer_to_float(fk, (a_host_large_integer)1,
                                     &one_flt[(int)fk], &dummy);
    }  /* for */
    useful_constants_initialized = TRUE;
  }  /* if */
}  /* initialize_interpreter_data */


static void translate_interpreter_offset(an_interpreter_state  *ips,
                                         a_constexpr_address   *cap,
                                         a_type_ptr            type,
                                         a_constant_ptr        con)
/*
cap represents an interpreter address pointing into interpreter storage for a
complete object of the given type.  Record in con->variant.address.offset
the corresponding target offset for a ck_address constant representing the
same address.  Also record the associated subobject path.
*/
{
  a_targ_ptrdiff_t  t_offset = 0; /* Total target offset. */
  a_byte            *address = cap->address;

  if (address != cap->complete_object) {
    a_byte                *parent_address = cap->complete_object;
    a_field_ptr           fp = NULL;
    a_base_class_ptr      bcp = NULL;
    a_subobject_path_ptr  path = NULL, *p_end_path = &path, path_entry;
    do {
      a_byte_count  i_offset;  /* Local interpreter offset. */
      path_entry = alloc_subobject_path();
      if (is_immediate_class_type(type)) {
        void  *ptr;
        find_subobject_for_interpreter_address(ips, cap, parent_address, type,
                                               &fp, &bcp);
        if (fp != NULL) {
          t_offset += fp->offset;
          type = skip_typerefs(fp->type);
          ptr = (void*)fp;
          path_entry->kind = (an_il_entry_kind)iek_field;
          path_entry->variant.field = fp;
        } else {
          check_assertion(bcp != NULL);
          t_offset += bcp->offset;
          type = skip_typerefs(bcp->type);
          ptr = (void*)bcp;
          path_entry->kind = (an_il_entry_kind)iek_base_class;
          path_entry->variant.base_class = bcp;
        }  /* if */
        get_mapped_byte_count(&persistent_map, ptr, i_offset);
      } else {
        i_offset = (a_byte_count)(address - parent_address);
        if (i_offset != 0) {
          a_byte_count  pos, elem_size;
          a_boolean     okay = TRUE;
          if (type->kind == (a_type_kind)tk_array) {
            type = skip_typerefs(type->variant.array.element_type);
          } else {
            /* Non-array objects are treated as arrays of one element in this
               context. */
          }  /* if */
          elem_size = value_bytes_for_type(ips, type, &okay);
          check_assertion(okay);
          pos = i_offset/elem_size;
          t_offset += pos*type->size;
          i_offset = pos*elem_size;
          path_entry->kind = (an_il_entry_kind)iek_constant;
          path_entry->variant.ptr_offset = (a_targ_ptrdiff_t)pos;
        }  /* if */
      }  /* if */
      parent_address += i_offset;
      *p_end_path = path_entry;
      p_end_path = &path_entry->next;
    } while (parent_address != address);
    con->variant.address.offset = t_offset;
    con->variant.address.subobject_path = path;
    con->implicit_cast = TRUE;
  }  /* if */
}  /* translate_interpreter_offset */


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
      con->null_pointer_constant_ruled_out = TRUE;
      break;
    case tk_float:
      set_constant_kind(con, (a_constant_repr_kind)ck_float);
      con->variant.float_value = *fp_value(object);
      break;
    case tk_pointer:
      { a_constexpr_address *cap = (a_constexpr_address *)object;
        if (is_runtime_data_address(cap)) {
          a_constant_ptr  rt_con = cap->variant.addr_con;
          /* Catch the case of a pointer or reference to a variable that is
             not constant-valued. */
          if (constant_is(rt_con, ck_address)) {
            if (rt_con->variant.address.kind ==
                                         (an_address_base_kind)abk_variable) {
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
            }  /* if */
          } else {
            /* Some "address" constants are integers cast to a pointer type. */
            check_assertion(constant_is(rt_con, ck_integer));
          }  /* if */
          /* Copy the run-time constant to result_con. */
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
          set_routine_address_constant(rp, con,
                                       /*set_address_taken_flag=*/TRUE);
          con->type = type;
          if (!identical_types(utp, rp->type)) {
            /* The pointer to function type was converted to a different
               pointer type (e.g., void*). */
            con->implicit_cast = TRUE;
          }  /* if */
        } else if (cap->address == NULL) {
          /* A NULL pointer (since it has a pointer type, it is not a null
             pointer constant). */
          set_constant_kind(con, (a_constant_repr_kind)ck_integer);
          con->implicit_cast = TRUE;
          con->null_pointer_constant_ruled_out = TRUE;
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
          set_constant_kind(con, (a_constant_repr_kind)ck_address);
          get_stack_bytes(ips, cap->complete_object, mptr);
          if (mptr != NULL) {
            /* Either a constant was already allocated for the pointed-to
               object or this address was created from an abk_variable entry
               (in which case, we must produce an address constant for that
               same variable). */
            a_constant_ptr  prev_con = (a_constant_ptr)mptr;
            a_type_ptr      top_type;
            if (!constant_is(prev_con, ck_address)) {
              /* Presumably this is a constant created for an abk_constant
                 address by the code below or it is a ck_string entry that
                 was loaded into interpreter storage (see
                 extract_value_from_constant). */
              cp = prev_con;
              top_type = cp->type;
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
              top_type = skip_typerefs(vp->type);
              cp = NULL;
            } else {
              cp = prev_con->variant.address.variant.constant;
              top_type = skip_typerefs(cp->type);
            }  /* if */
            if (cap->address != cap->complete_object) {
              translate_interpreter_offset(ips, cap, top_type, con);
            }  /* if */
          } else {
            /* Create an abk_constant or abk_temporary entry. */
            a_byte  *base_address;
            cp = alloc_constant((a_constant_repr_kind)ck_error);
            if (cap->length != 0 && !is_bit_field_lvalue(cap)) {
              /* If we're pointing at or into an array, a constant for the
                 whole array must be allocated. */
              a_type_ptr    atp = alloc_type((a_type_kind)tk_array);
              a_type_ptr    butp = skip_typerefs(utp);
              a_byte_count  offset;
              if (is_array_element(cap)) {
                base_address = get_base_address(cap);
              } else {
                base_address = cap->address;
              }  /* if */
              offset = (a_byte_count)(cap->address - base_address);
              if (!is_array_element(cap) ||
                  (butp->incomplete && butp->kind == (a_type_kind)tk_array)) {
                /* This can happen when binding a reference to an array with
                   no specified bound.  E.g.:
                     struct S { const int (&x)[]; };
                     constexpr S x = { { 37 } };
                   We'll produce a known bound below. */
                utp = butp->variant.array.element_type;
                butp = skip_typerefs(utp);
              }  /* if */
              if (offset != 0) {
                con->variant.address.offset =
                      butp->size *
                            (offset/value_bytes_for_type(ips, butp, &result));
              }  /* if */
              atp->variant.array.element_type = utp;
              atp->variant.array.variant.number_of_elements = cap->length;
              set_type_size(atp);
              utp = atp;
            } else {
              base_address = cap->address;
            }  /* if */
            /* Set up a reverse mapping, so other address constants into this
               object can use the same constant entry (see the case where mptr
               points to a non-ck_address entry above). */
            map_stack_bytes(ips, base_address, (a_byte*)cp);
            if (!copy_interpreter_object_to_constant(
                          ips, base_address, cap->complete_object, utp, cp)) {
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
              con->variant.address.kind = (an_address_base_kind)abk_constant;
            } else {
              con->variant.address.kind = (an_address_base_kind)abk_temporary;
              if (!(cap->flags & CA_LIFETIME_EXTENDED) ||
                  depth_innermost_function_scope != NO_SCOPE_DEPTH) {
                /* The address of a temporary results in a dangling pointer. */
                do_constexpr_fail(result);
                info_with_pos(ec_constexpr_expiring_temporary,
                              &ips->position, ips);
              }  /* if */
            }  /* if */
            con->variant.address.variant.constant = cp;
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
          con->variant.ptr_to_member.is_function_ptr = TRUE;
          con->variant.ptr_to_member.variant.routine =
                                                    pm_value->variant.routine;
        } else {
          con->variant.ptr_to_member.variant.field = pm_value->variant.field;
        }  /* if */
      }
      break;
    case tk_struct:
    case tk_class:
      { a_base_class_ptr  bcp = base_classes_of(type);
        a_field_ptr       fp = type->variant.class_struct_union.field_list;
        a_boolean         is_static_init_list;
        set_constant_kind(con, (a_constant_repr_kind)ck_aggregate);
        /* Add direct base sub-object constants first. */
        for (bcp = base_classes_of(type); bcp != NULL; bcp = bcp->next) {
          a_byte_count    offset;
          a_constant_ptr  cp;
          if (!bcp->direct || bcp->is_virtual) continue;
          get_mapped_byte_count(&persistent_map, bcp, offset);
          if (!subobject_is_initialized(object+offset, complete_object)) {
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
          cp->constant_for_base_class_from_constexpr_folding = TRUE;
          add_constant_to_aggregate(cp, con);
        }  /* for */
        if (!result) break;
        /* Now add the constants for initializable fields. */
        is_static_init_list = class_type_supp(type)->is_initializer_list &&
                              innermost_function_scope == NULL &&
                              !scope_stack_top().in_field_initializer;
        fp = next_alloc_field(fp);
        for (; fp != NULL; fp = next_alloc_field(fp->next)) {
          a_byte_count    offset;
          a_constant_ptr  cp;
          if (fp->compiler_generated && !fp->is_anonymous_parent_object) {
            /* Ignore fields generated by prelowering. */
            continue;
          }  /* if */
          get_mapped_byte_count(&persistent_map, fp, offset);
          if (!subobject_is_initialized(object+offset, complete_object)) {
            info_with_pos_sym(ec_field_subobject_not_initialized,
                              &ips->position, symbol_for(fp), ips);
            do_constexpr_fail(result);
            break;
          }  /* if */
          if (is_static_init_list &&
              skip_typerefs(fp->type)->kind == (a_type_kind)tk_pointer) {
            /* A static-lifetime initializer list.  Make sure the underlying
               array is treated as having a static lifetime also. */
            ((a_constexpr_address*)(object+offset))->flags |=
                                                         CA_LIFETIME_EXTENDED;
          }  /* if */
          cp = alloc_constant((a_constant_repr_kind)ck_error);
          if (!copy_interpreter_object_to_constant(
                         ips, object+offset, complete_object, fp->type, cp)) {
            do_constexpr_fail(result);
            break;
          }  /* if */
          add_constant_to_aggregate(cp, con);
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
        /* Retrieve the active field. */
        afp = (a_field_ptr)*(void**)object;
        if (afp == NULL) {
          /* This should only happen with unions that have no field (other
             than empty anonymous union parent objects), and therefore cannot
             have an active field. */
        } else {
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
              add_constant_to_aggregate(des_con, con);
            }  /* if */
            add_constant_to_aggregate(elem_con, con);
          }  /* if */
        }  /* if */
      }
      break;
    case tk_array:
      { a_type_ptr      etp = skip_typerefs(type->variant.array.element_type);
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
          add_constant_to_aggregate(elem_con, con);
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
          add_constant_to_aggregate(elem_con, con);
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
  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips);
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
                         a_boolean         force_prvalue,
                         a_constant_ptr    result_con,
                         a_diag_list_ptr   diag_list)
/*
Attempt to interpret the given expression.  If force_prvalue is TRUE and expr
is a glvalue, convert the glvalue result to a prvalue.  Return TRUE if
successful, and produce the resulting value in result_con.  Otherwise, return
FALSE, and record diagnostic info in *diag_list.
*/
{
  a_boolean             result = TRUE;
  an_interpreter_state  ips;
  a_byte                *result_storage;
  a_byte_count          n_bytes;
  a_type_ptr            result_type = skip_typerefs(expr->type);

  if (is_constant_node(expr)) {
    a_constant_ptr  expr_con = node_constant(expr);
    if (constant_is(expr_con, ck_template_param)) {
      /* Do not return a copy of a template-dependent constant since it
         requires substitution before deciding that it is an actual constant
         value.  (Also, it may have associated rescan info that would not be
         equivalent in the copy.) */
    } else {
      (void)copy_constant_full(expr_con, result_con,
                               CE_COPYING_FOR_CONSTEXPR_MASTER_EXPR);
      goto done;
    }  /* if */
  }  /* if */
  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips);
  ips.position = expr->position;
  n_bytes = expr_result_size(&ips, expr, result_type, &result); 
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
            info_with_pos(ec_constexpr_access_to_runtime_storage,
                          &expr->position, &ips);
            do_constexpr_fail(result);
          } else if (is_volatile_qualified_type(expr->type)) {
              do_constexpr_fail(result);
              info_with_pos(ec_constexpr_volatile_fetch, &expr->position,
                            &ips);
          } else {
            /* result_storage points to an interpreter address for the glvalue.
               Allocate a new object for the corresponding prvalue and perform
               the glvalue-to-prvalue conversion into it. */
            n_bytes = value_bytes_for_type(&ips, result_type, &result);
            check_assertion(result);
            alloc_complete_object(&ips, n_bytes, result_type, result_storage);
            result = do_glvalue_to_prvalue(&ips, expr, result_type, cap,
                                           n_bytes, result_storage,
                                           result_storage);
          }  /* if */
        } else {
          result_type = expr->is_xvalue ?
                                      make_rvalue_reference_type(expr->type) :
                                      make_reference_type(expr->type);
        }  /* if */
      }  /* if */
      if (!result) {
        /* Nothing more to do. */
      } else if (!copy_interpreter_object_to_constant(
                                         &ips, result_storage, result_storage,
                                         result_type, result_con)) {
        do_constexpr_fail(result);
      } else if (expr->next == NULL) {
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


a_boolean interpret_constexpr_call(an_expr_node_ptr  call_expr,
                                   a_constant_ptr    result_con,
                                   a_diag_list_ptr   diag_list)
/*
Attempt to interpret the call represented by call_expr.  Return TRUE if
successful, and produce the resulting value in result_con.  Otherwise,
return FALSE, and record diagnostic info in *diag_list.
*/
{
  a_boolean             result = TRUE;
  an_interpreter_state  ips;
  a_byte                *result_storage;
  a_byte_count          n_bytes;
  a_type_ptr            result_type = skip_typerefs(call_expr->type);

  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips);
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
    } else if (!copy_interpreter_object_to_constant(
                                         &ips, result_storage, result_storage,
                                         result_type, result_con)) {
      do_constexpr_fail(result);
    } else if (call_expr->next == NULL) {
      /* If call_expr is part of an expression list and followed by other
         expressions, do not record it as the backing expression since
         it could cause IL traversal problems later on. */
      result_con->expr = call_expr;
    }  /* if */
  }  /* if */
  *diag_list = ips.diag_list;
  release_interpreter_state(&ips);
  return result;
}  /* interpret_constexpr_call */


a_boolean interpret_dynamic_init(a_dynamic_init_ptr  dip,
                                 a_source_position   *pos,
                                 a_type_ptr          result_type,
                                 a_constant_ptr      result_con,
                                 a_diag_list_ptr     diag_list)
/*
Attempt to interpret the given dynamic initialization entry.  Return TRUE if
successful, and produce the resulting value (of the given type) in result_con.
Otherwise, return FALSE, and record diagnostic info in *diag_list.  pos is the
source position of the initialization.
*/
{
  a_boolean             result = TRUE;
  an_interpreter_state  ips;
  a_byte                *result_storage;
  a_byte_count          n_bytes;

  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips);
  ips.position = *pos;
  result_type = skip_typerefs(result_type);
  n_bytes = value_bytes_for_type(&ips, result_type, &result); 
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
    if (!do_constexpr_dynamic_init(&ips, dip, pos, result_storage,
                                   result_storage)) {
      if (ips.input_error) {
        /* Interpretation failed due to an error node in the IL.  Continue
           with an error constant, but treat interpretation as successful. */
        set_error_constant(result_con);
      } else {
        do_constexpr_fail(result);
      }  /* if */
    } else if (!copy_interpreter_object_to_constant(
                                         &ips, result_storage, result_storage,
                                         result_type, result_con)) {
      do_constexpr_fail(result);
    } else {
      if ((dip->kind == (a_dynamic_init_kind)dik_expression ||
           dip->kind == (a_dynamic_init_kind)dik_class_result_via_ctor) &&
          (curr_il_region_number == file_scope_region_number) ==
                                     in_file_scope(dip->variant.expression)) {
        result_con->expr = dip->variant.expression;
      }  /* if */
      if (dip->is_explicit_cast) {
        result_con->explicit_cast_applied = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  *diag_list = ips.diag_list;
  release_interpreter_state(&ips);
  return result;
}  /* interpret_dynamic_init */


a_boolean interpret_constexpr_ctor(a_dynamic_init_ptr  dip,
                                   a_constant_ptr      result_con,
                                   a_diag_list_ptr     diag_list)
/*
Attempt to interpret the constructor call represented by dip.  Return TRUE if
successful, and produce the resulting value in result_con.  Otherwise,
return FALSE, and record diagnostic info in *diag_list.
*/
{
  a_boolean             result = TRUE;
  a_routine_ptr         ctor;
  an_interpreter_state  ips;
  a_byte                *result_storage;
  a_byte_count          n_bytes;
  a_type_ptr            result_type;

  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips);
  ips.position = error_position;
  if (is_error_dynamic_init(dip)) {
    set_error_constant(result_con);
    goto done;
  } else {
    ctor = dip->variant.constructor.ptr;
  }  /* if */
  result_type = parent_class_of(ctor);
  n_bytes = value_bytes_for_type(&ips, result_type, &result); 
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
    if (!do_constexpr_ctor(&ips, dip, &error_position, result_storage,
                           result_storage, /*implied_src=*/NULL)) {
      if (ips.input_error) {
        /* Interpretation failed due to an error node in the IL.  Continue
           with an error constant, but treat interpretation as successful. */
        set_error_constant(result_con);
      } else {
        do_constexpr_fail(result);
      }  /* if */
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
        do_constexpr_fail(result);
      }  /* if */
      unmap_stack_bytes(&ips, result_storage);
      release_local_constant(&this_con);
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
      table_size = sizeof(an_alloc_seq_number)*(unsigned long)(1<<k);
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
      table_size = sizeof(an_alloc_seq_number)*(unsigned long)(1<<k);
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
  useful_constants_initialized = FALSE;
  free_stack_blocks = NULL;
  free_variant_path_entries = NULL;
#if DEBUG && TRACK_INTERPRETER_ALLOCATIONS
  object_alloc_to_intercept = 0;
#endif /* DEBUG && TRACK_INTERPRETER_ALLOCATIONS */
}  /* interpret_one_time_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
