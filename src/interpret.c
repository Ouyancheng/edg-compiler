/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015 Edison Design Group Inc.                        [_]          *
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

#include "pch.h"

#include "exprutil.h"

#include "folding.h"

/*
This file implements an interpreter for a subset of the unlowered IL produced
by the C++ front end.  Specifically, the subset corresponds to the constructs
allowed in a constant expression in C++14.


The Interpreter
---------------
The interpreter itself traverses the IL in typical "recursive descent" fashion.
The principal entry points are interpret_constexpr_call and
interpret_constexpr_ctor, which set up an "interpreter state" that is carried
through the interpretation process.

An interpreter invocation can end for one of three reasons:
  (1) the call is completed with a valid result (normal case),
  (2) interpretation runs into an invalid operation (e.g., an attempt to
      read an uninitialized value), or
  (3) the cost of the interpretation is too high.

Regarding the latter reason, the interpreter tracks the sum of the number of
calls and the number of loop-back branches.  When that number reaches a certain
large value, the interpretation is deemed too expensive.  Deeply nested
recursion is also limited.


Storage
-------
The interpreter manages two pools of storage.
  (1) Local variables and temporaries, and
  (2) Permanent data associated with declarative entities
      (e.g., field offsets).

The first kind of storage is allocated/deallocated in strict last-in/first-out
(LIFO) manner, and so is efficiently implemented using a storage stack.  It is
implemented here through a structure of type a_storage_stack_state, which
maintains a linked list of large blocks from which small chunks are allocated
/deallocated (adding a large block whenever the last added one is full).  To
avoid excessive waste, larger chunks (which should be uncommon) are not
allocated from the large blocks, but separately (through the front end's
normal memory allocator).

Every variable and temporary triggers an allocation, which is performed through
the macro alloc_stack_bytes.  Deallocation, on the other hand, is batched
"per scope" (for variables) or "per full expression" (for temporaries).  The
macros save_storage_stack and restore_storage_stack support this.

The second kind of storage persists across interpreter invocations.  It also
uses a storage stack (static variable persistent_data), although the ability to
efficiently deallocate is not exploited in that case.  The macro alloc_bytes
can be used for these allocations.


Mappings
--------
Storage can be associated with a particular address in IL memory through a
data map (an efficient pointer-to-pointer hash table).  An association is
established by invoking the macro map_ptr, and revoked by invoking unmap_ptr.
A mapping can be retrieved with the macro get_mapped_ptr.  The macros
map_byte_count and get_mapped_byte_count can similarly be used to associate an
unsigned integer with an IL address.

The interpreter state includes a data map for automatic variables and
temporaries; convenience macros map_stack_bytes, unmap_stack_bytes, and
get_stack_bytes can be used to manage that data map.  This map is also used
to map run-time namespace-scope variables to a_constant entries representing
their address (this is done by mapping the storage class field of the
variable).

If a pointer is mapped multiple times (e.g., a local variable entry during a
recursive function invocation), the last mapping is returned by get_mapped_ptr
(or get_stack_bytes), and if that last mapping is "unmapped", the previous
mapping becomes available again.

Besides the data map associated with an interpreter state, another map is kept
that persists across interpreter invocations (static variable persistent_map).
This map, e.g., holds data layout information for associated with types and
fields (the data layout for the interpreter is different from that for the
target architecture).


Object Layout
-------------
FIXME
The non-address scalar data members of stored objects are the value
representations used elsewhere in the front end (i.e., an_integer_value, etc.).
Bit fields occupy a whole integer value, but every "store" to a bit field is
appropriately trimmed.

Class types objects and subobjects start with an IL pointer (described below),
followed by storage for the fields, and that followed by storage for direct
base classes (in declaration order).  A derived-to-base class cast therefore
always corresponds to a positive offset of the "this" pointer, whereas a
base-to-derived class cast involves negative offset.

The storage for a union object starts with a pointer to an IL entry for a
field.  That pointer reflects which field is the active field in the union.

The storage of a non-union class type ("class" or "struct") starts with a
pointer to an IL entry for the type of the next-derived subobject, or NULL
for the most-derived subobject.  This is used to catch invalid base-to-derived
casts (or certain invalid accesses to a derived-object member through a
pointer-to-member value).

*/

typedef unsigned int a_byte_count;

typedef unsigned int an_alloc_seq_number;

/*
Macro defining the size of large blocks allocated for the storage stack.  These
large blocks are then parceled out in smaller chunks as requested through the
macro alloc_stack_bytes.  If an alloc_stack_bytes request is too large,
however, the storage is not carved from the large blocks; instead, it is
allocated from general memory.
*/
#define CONSTEXPR_STACK_BLOCK_SIZE (1<<16)

/*
Macro defining the largest chunk size to be carved from storage stack blocks
(see CONSTEXPR_STACK_BLOCK_SIZE  above).  Invocations of alloc_stack_bytes that
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
  a_map_index	next_index;
			/* The index of the next map entry with an identical
			   hash value. */
} a_data_map_entry;


/*
Structure describing a data map.
*/
typedef struct a_data_map {
  a_data_map_entry
		*table;
			/* Hash table mapping pointers into the IL onto
			   associated data. */
  a_map_index
		overflow_size;
			/* Number of overflow entries (used in case of hashing
			   collisions) in the hash table. */
  a_map_index
		next_free;
			/* Index of the next available map overflow entry. */
} a_data_map;


static a_data_map
		persistent_map;
			/* Map that persists across interpreter invocations.
			   In particular, its entries describe the layout of
			   data in the interpreter. */


/* Macro defining the number of entries in the hash table proper. */
#define NUM_DATA_MAP_HASH_HEADERS (1<<16)


static void init_map_free_list(a_data_map   *map,
                               a_map_index  first,
                               a_map_index  last)
/*
Establish a "free list" structure on the entries map->table[first] through
map->table[last].  I.e., each map->table[k].next_index points to the next,
except for the last entry whose next_index field is set to 0.
*/
{
  a_data_map_entry  *table = map->table;

  
  map->next_free = first;
  while (first != last) {
    table[first].next_index = first+1;
    first = first+1;
  }  /* while */
  table[last].next_index = 0;
}  /* init_map_free_list */


static a_byte	*free_map_tables;
			/* List of map tables available for reuse. */

static void init_data_map(a_data_map  *map)
/*
Initialize the given data map.
*/
{
  if (free_map_tables == NULL) {
    /* Initialize the data map fields. */
    a_byte_count  n_bytes;
    map->overflow_size = 100;
    n_bytes = (NUM_DATA_MAP_HASH_HEADERS+map->overflow_size)
                                         * sizeof(a_data_map_entry);
    map->table = (a_data_map_entry*)alloc_resizable_buffer(n_bytes);
  } else {
    /* Reuse a previously allocated table. */
    a_data_map  *self_map = (a_data_map*)free_map_tables;
    *map = *self_map;
    map->table = (a_data_map_entry*)free_map_tables;
    free_map_tables = (a_byte*)self_map->table;
  }  /* if */
  /* Clear the main map entries. */
  memzero((char*)map->table,
          size_t_arg(NUM_DATA_MAP_HASH_HEADERS*sizeof(a_data_map_entry)));
  init_map_free_list(map, NUM_DATA_MAP_HASH_HEADERS,
                     NUM_DATA_MAP_HASH_HEADERS+map->overflow_size-1);
}  /* init_data_map */


static void release_data_map_table(a_data_map  *map)
/*
Release the storage for the given map's table.
*/
{
  a_data_map  *self_map = (a_data_map*)map->table;

  /* Embed the map information into the first bytes of the table. */
  *self_map = *map;
  /* Prepend the new table to the "free tables" list. */
  self_map->table = (a_data_map_entry*)free_map_tables;
  free_map_tables = (a_byte*)self_map;
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
Type for the entries in a live set table.
*/
typedef struct a_live_set_entry {
  an_alloc_seq_number
		alloc_seq_number;
			/* An sequence number in the set. */
  a_live_set_index
		next_index;
			/* The index of the next set entry with an identical
			   hash value. */
} a_live_set_entry;


/* Macro defining the number of entries in the live set's hash table proper
   (i.e., not including overflow entries). */
#define NUM_LIVE_SET_HASH_HEADERS (1<<16)


/*
A hash table maintaining a set of "live" allocation sequence numbers (i.e., the
sequence numbers of allocations that have not been deallocated yet).
*/
typedef struct a_live_set {
  a_live_set_entry
		*table;
			/* The hash table proper. */
  a_live_set_index
		overflow_size;
			/* Number of overflow entries (used in case of hashing
			   collisions) in the hash table. */
  a_live_set_index
		next_free;
			/* Index of the next available overflow entry. */
} a_live_set;


static void init_live_set_free_list(a_live_set        *set,
                                    a_live_set_index  first,
                                    a_live_set_index  last)
/*
Establish a "free list" structure on the entries set->table[first] through
set->table[last].  I.e., each set->table[k].next_index points to the next,
except for the last entry whose next_index field is set to 0.
*/
{
  a_live_set_entry  *table = set->table;

  while (first != last) {
    table[first].next_index = first+1;
    first = first+1;
  }  /* while */
  table[last].next_index = 0;
}  /* init_live_set_free_list */


static a_byte	*free_live_set_tables;
			/* List of live set tables available for reuse. */

static void init_live_set(a_live_set  *set)
/*
Initialize the given live set.
*/
{
  if (free_live_set_tables == NULL) {
    /* Allocate a new table. */
    a_byte_count  n_bytes;
    set->overflow_size = 100;
    set->next_free = NUM_LIVE_SET_HASH_HEADERS;
    n_bytes = (NUM_LIVE_SET_HASH_HEADERS+set->overflow_size)
                                         * sizeof(a_live_set_entry);
    set->table = (a_live_set_entry*)alloc_resizable_buffer(n_bytes);
  } else {
    /* Reuse a previously allocated table. */
    a_live_set  *self_set = (a_live_set*)free_live_set_tables;
    *set = *self_set;
    set->table = (a_live_set_entry*)free_live_set_tables;
    free_live_set_tables = (a_byte*)self_set->table;
  }  /* if */
  /* Clear the main set entries. */
  memzero((char*)set->table,
          size_t_arg(NUM_LIVE_SET_HASH_HEADERS*sizeof(a_live_set_entry)));
  /* Establish the free-list structure for the overflow entries. */
  init_live_set_free_list(set, NUM_LIVE_SET_HASH_HEADERS,
                          NUM_LIVE_SET_HASH_HEADERS+set->overflow_size-1);
}  /* init_live_set */


static void release_live_set_table(a_live_set  *set)
/*
Release the storage for the given set's table.
*/
{
  a_live_set  *self_set = (a_live_set*)set->table;

  /* Embed the set information into the first bytes of the table. */
  *self_set = *set;
  /* Prepend the new table to the "free tables" list. */
  self_set->table = (a_live_set_entry*)free_live_set_tables;
  free_live_set_tables = (a_byte*)self_set;
}  /* release_live_set_table */


static void expand_live_set(a_live_set  *set)
/*
Double the size of the overflow area of the given live set.
*/
{
  a_byte_count  old_byte_size, new_byte_size;

  check_assertion(set->next_free == 0);
  old_byte_size = (NUM_LIVE_SET_HASH_HEADERS+set->overflow_size)
                                                    * sizeof(a_live_set_entry);
  new_byte_size = (NUM_LIVE_SET_HASH_HEADERS+2*set->overflow_size)
                                                    * sizeof(a_live_set_entry);
  set->table = (a_live_set_entry*)realloc_buffer((char*)set->table,
                                                 old_byte_size, new_byte_size);
  init_live_set_free_list(set, NUM_LIVE_SET_HASH_HEADERS+set->overflow_size,
                          NUM_LIVE_SET_HASH_HEADERS+2*set->overflow_size-1);
  set->overflow_size *= 2;
}  /* expand_live_set */


#define hash_alloc_seq_number(seq)                                           \
  ((seq) % NUM_LIVE_SET_HASH_HEADERS)


#define add_to_live_set(set, seq)                                            \
  { a_live_set_index  idx = hash_alloc_seq_number(seq);                      \
    a_live_set_entry  *table = (set)->table;                                 \
    an_alloc_seq_number  cached_seq_number = table[idx].alloc_seq_number;    \
    if (cached_seq_number != 0) {                                            \
      /* Move the existing entry to an overflow entry. */                    \
      a_live_set_index  new_index = (set)->next_free;                        \
      (set)->next_free = table[new_index].next_index;                        \
      if (new_index == 0) {                                                  \
        expand_live_set(set);                                                \
        new_index = (set)->next_free;                                        \
      }  /* if */                                                            \
      table[new_index].alloc_seq_number = cached_seq_number;                 \
      table[new_index].next_index = table[idx].next_index;                   \
      table[idx].next_index = new_index;                                     \
    }  /* if */                                                              \
    table[idx].alloc_seq_number = (seq);                                     \
  }


#define remove_from_live_set(set, seq)                                       \
  { a_live_set_index     idx = hash_alloc_seq_number(seq);                   \
    a_live_set_entry     *table = (set)->table;                              \
    an_alloc_seq_number  cached_seq_number = table[idx].alloc_seq_number;    \
    if (cached_seq_number == seq) {                                          \
      a_live_set_index  prev_index = table[idx].next_index;                  \
      if (prev_index == 0) {                                                 \
        /* Only one element in the bucket. */                                \
        table[idx].alloc_seq_number = 0;                                     \
      } else {                                                               \
        /* Move the first overflow entry to the main hash table. */          \
        table[idx].alloc_seq_number = table[prev_index].alloc_seq_number;    \
        table[idx].next_index = table[prev_index].next_index;                \
        /* Recycle the overflow entry. */                                    \
        table[prev_index].next_index = (set)->next_free;                     \
        (set)->next_free = prev_index;                                       \
      }  /* if */                                                            \
    } else {                                                                 \
      /* The stack allocation discipline make this is impossible.*/          \
      unexpected_condition_str("live_set id not found");                     \
    }  /* if */                                                              \
  }


/*
Return TRUE if the given allocation sequence number is in the given live set.
This macro is written with the assumption that in the vast majority of cases
the sequence number is present and it is in the main hash table (and not in
an overflow entry).
*/
#define in_live_set(set, seq)                                                \
  ((set)->table[hash_alloc_seq_number(seq)].alloc_seq_number == seq ?        \
    TRUE : f_in_live_set(set, seq))

static a_boolean f_in_live_set(a_live_set           *set,
                               an_alloc_seq_number  seq)
/*
Return TRUE if the given allocation sequence number is in the given live set.
Return FALSE otherwise.  This is normally always called through the macro
in_live_set.
*/
{
  a_boolean            result;
  an_alloc_seq_number  stored_seq;
  a_live_set_index     idx = hash_alloc_seq_number(seq);

  for (;;) {
    stored_seq = set->table[idx].alloc_seq_number;
    if (stored_seq == seq) {
      result = TRUE;
      break;
    } else if (stored_seq == 0) {
      result = FALSE;
      break;
    } else {
      idx = set->table[idx].next_index;
    }  /* if */
  }  /* for */
  return result;
}  /* f_in_live_set */

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
			/* A representation of a pending diagnostics
			   (presumably explaining why interpretation failed to
			   produce a constant result).  This diagnostic is not
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
  a_storage_stack_state
		static_storage;
			/* Pointer to the storage stack state used to allocate
			   static storage-duration variables.  Initialized
			   when first needed. */
} an_interpreter_state;


#define cost_exceeded(ips)                                                   \
  (++(ips)->cost > max_constexpr_call_cost)


static a_byte	*free_stack_blocks;
			/* List of free stack blocks available for reuse. */


static void init_constexpr_stack(a_storage_stack_state  *sss)
/*
Initialize stack storage for the given storage stack.
*/
{
  a_byte        *new_block;
  a_byte_count  ptr_size = sizeof(a_byte*);

  if (free_stack_blocks == NULL) {
    new_block = (a_byte*)alloc_fe(CONSTEXPR_STACK_BLOCK_SIZE);
  } else {
    new_block = free_stack_blocks;
    free_stack_blocks = *(a_byte**)(new_block+ptr_size);
  }  /* if */
  do_host_alignment(ptr_size);
  /* Link to the previous block (if any). */
  *(a_byte**)new_block = sss->curr_block;
  sss->curr_block = new_block;
  /* Set the next-block pointer to NULL. */
  *(a_byte**)(new_block+ptr_size) = NULL;
  /* Leave space for the bookkeeping information (three pointers). */
  sss->top = sss->curr_block+3*ptr_size;
  sss->alloc_seq_number = 1;
}  /* init_constexpr_stack */


static void release_constexpr_stack(a_storage_stack_state  *sss)
/*
Release the storage stack pointed to by ips for reuse.
*/
{
  a_byte  *first_block = sss->curr_block;

  if (first_block != NULL) {
    if (free_stack_blocks != NULL) {
      /* Some blocks are already on the "free blocks" list.  Append those free
         blocks to those about to be freed. */
      a_byte        *block, *next_block;
      a_byte_count  ptr_size = sizeof(a_byte*);
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


static void init_interpreter_state(an_interpreter_state  *ips)
/*
Initialize the given interpreter state.
*/
{
  init_data_map(&ips->map);
  init_constexpr_stack(&ips->storage_stack);
  init_live_set(&ips->live_set);
  ips->curr_alloc_seq_number = 1;
  ips->curr_call_frame = NULL;
  ips->extension_state = NULL;
  ips->constants = NULL;
  clear_diag_list(&ips->diag_list);
  ips->position = null_source_position;
  ips->cost = 0;
  ips->static_storage_ready = FALSE;
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
}  /* release_interpreter_state */


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
    init_constexpr_stack(sss);
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
  { if ((n_bytes) > CONSTEXPR_STACK_ALLOC_LIMIT) {                           \
      /* We'll allocate the bytes in a separate general allocation block. */ \
      a_byte        *large_block;                                            \
      a_byte_count  hdr_size = sizeof(a_large_block_header), block_size;     \
      do_host_alignment(hdr_size);                                           \
      block_size = hdr_size+n_bytes;                                         \
      large_block = (a_byte*)alloc_general(block_size);                      \
      ((a_large_block_header*)large_block)->prev_large_block =               \
                                          (sss)->large_blocks;               \
      ((a_large_block_header*)large_block)->block_size = block_size;         \
      (sss)->large_blocks = large_block;                                     \
      (storage_ptr) = large_block+hdr_size;                                  \
    } else {                                                                 \
      a_byte_count  size = n_bytes;                                          \
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
  alloc_bytes(&(ips)->storage_stack, (n_bytes), (storage_ptr))


/*
Macros to save and restore an allocation stack state.
*/
#define save_storage_stack(ips, state)                                       \
  {                                                                          \
    (state) = (ips)->storage_stack;                                          \
    (ips)->storage_stack.alloc_seq_number = ++(ips)->curr_alloc_seq_number;  \
    add_to_live_set(&(ips)->live_set, (ips)->curr_alloc_seq_number);         \
  }

#define restore_storage_stack(ips, state)                                    \
  {                                                                          \
    a_byte  *curr_large_blocks = (ips)->storage_stack.large_blocks,          \
            *saved_large_blocks = (state).large_blocks;                      \
    remove_from_live_set(&(ips)->live_set,                                   \
                         (ips)->storage_stack.alloc_seq_number);             \
    while (curr_large_blocks != saved_large_blocks) {                        \
      a_byte  *large_block = curr_large_blocks;                              \
      curr_large_blocks = ((a_large_block_header*)large_block)               \
                                                      ->prev_large_block;    \
      free_general(large_block,                                              \
                   ((a_large_block_header*)large_block)->block_size);        \
    }  /* while */                                                           \
    (ips)->storage_stack = (state);                                          \
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
#define push_call_frame(ips, p_frame, rp, pos, p_result)                     \
  {                                                                          \
    (p_frame)->parent = (ips)->curr_call_frame;                              \
    (p_frame)->routine = (rp);                                               \
    (p_frame)->position = (pos);                                             \
    (p_frame)->result_storage = (p_result);                                  \
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

#define hash_il_ptr(ptr)                                                     \
   (((uintptr_t)ptr >> HASH_PTR_SHIFT) % NUM_DATA_MAP_HASH_HEADERS)

static a_mapped_value find_overflow_entry(a_data_map    *map,
                                          a_byte        *ptr,
                                          a_byte_count  idx)
/*
Search map for an overflow entry mapping ptr, starting at the entry at the
given index.
*/
{
  a_mapped_value    result;
  a_data_map_entry  *table = map->table;

  for (;;) {
    if (table[idx].ptr == ptr) {
      /* We found the searched-for entry. */
      a_map_index  hash_idx = hash_il_ptr(ptr);
      result = table[idx].data;
      /* Make this the new principal entry (by swapping). */
      table[idx].ptr = table[hash_idx].ptr;
      table[idx].data = table[hash_idx].data;
      table[hash_idx].ptr = ptr;
      table[hash_idx].data = result;
      break;
    } else {
      idx = table[idx].next_index;
      if (idx == 0) {
        /* We've exhausted the list of entries. */
        memzero((char*)&result, sizeof(result));
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* find_overflow_entry */


/*
Macro to retrieve a pointer (dptr) associated with a pointer into the IL
(iptr) from a given data map.
*/
#define get_mapped_ptr(map, iptr, dptr)                                      \
  { a_map_index   idx = hash_il_ptr((a_byte*)(iptr));                        \
    a_byte        *cached_ptr = (map)->table[idx].ptr;                       \
    if (cached_ptr == (a_byte*)(iptr)) {                                     \
      (dptr) = (map)->table[idx].data.ptr;                                   \
    } else {                                                                 \
      a_map_index  next_index = (map)->table[idx].next_index;                \
      if (next_index != 0) {                                                 \
        (dptr) = find_overflow_entry((map), (a_byte*)(iptr), next_index).ptr;\
      } else {                                                               \
        (dptr) = 0;                                                          \
      }  /* if */                                                            \
    }  /* if */                                                              \
  }

/*
Macro to retrieve a byte count (bcount) associated with a pointer into the IL
(iptr) from a given data map.
*/
#define get_mapped_byte_count(map, iptr, bcount)                             \
  { a_map_index   idx = hash_il_ptr((a_byte*)(iptr));                        \
    a_byte        *cached_ptr = (map)->table[idx].ptr;                       \
    if (cached_ptr == (a_byte*)(iptr)) {                                     \
      (bcount) = (map)->table[idx].data.byte_count;                          \
    } else {                                                                 \
      a_map_index  next_index = (map)->table[idx].next_index;                \
      if (next_index != 0) {                                                 \
        (bcount) = find_overflow_entry((map), (a_byte*)(iptr), next_index)   \
                                                                 .byte_count;\
      } else {                                                               \
        (bcount) = 0;                                                        \
      }  /* if */                                                            \
    }  /* if */                                                              \
  }

/*
Convenience macro to retrieve a pointer (sptr) to the stack storage associated
with an IL pointer (iptr).  ips is the interpreter state managing the stack
storage.
*/
#define get_stack_bytes(ips, iptr, sptr)                                     \
  get_mapped_ptr(&(ips)->map, iptr, sptr)

static void expand_map(a_data_map  *map)
/*
Double the size of the overflow area of the given map.
*/
{
  a_byte_count  old_byte_size, new_byte_size;

  check_assertion(map->next_free == 0);
  old_byte_size = (NUM_DATA_MAP_HASH_HEADERS+map->overflow_size)
                                                    * sizeof(a_data_map_entry);
  new_byte_size = (NUM_DATA_MAP_HASH_HEADERS+2*map->overflow_size)
                                                    * sizeof(a_data_map_entry);
  map->table = (a_data_map_entry*)realloc_buffer((char*)map->table,
                                                 old_byte_size, new_byte_size);
  init_map_free_list(map, NUM_DATA_MAP_HASH_HEADERS+map->overflow_size,
                     NUM_DATA_MAP_HASH_HEADERS+2*map->overflow_size-1);
  map->overflow_size *= 2;
}  /* expand_map */

/*
Macro to add a (pointer, pointer) entry to a data map.
*/
#define map_ptr(map, iptr, dptr)                                             \
  { a_map_index       idx = hash_il_ptr(iptr);                               \
    a_data_map_entry  *table = (map)->table;                                 \
    a_byte            *cached_ptr = table[idx].ptr;                          \
    if (cached_ptr != NULL) {                                                \
      /* Move the existing entry to an overflow entry. */                    \
      a_map_index  new_index;                                                \
      if ((map)->next_free == 0) {                                           \
        expand_map(map);                                                     \
      }  /* if */                                                            \
      new_index = (map)->next_free;                                          \
      (map)->next_free = table[new_index].next_index;                        \
      table[new_index].ptr = cached_ptr;                                     \
      table[new_index].data = table[idx].data;                               \
      table[new_index].next_index = table[idx].next_index;                   \
      table[idx].next_index = new_index;                                     \
    }  /* if */                                                              \
    table[idx].ptr = (a_byte*)(iptr);                                        \
    table[idx].data.ptr = (dptr);                                            \
  }

/*
Macro to add a (pointer, byte-count) entry to a data map.
*/
#define map_byte_count(map, iptr, bcount)                                    \
  { a_map_index       idx = hash_il_ptr(iptr);                               \
    a_data_map_entry  *table = (map)->table;                                 \
    a_byte            *cached_ptr = table[idx].ptr;                          \
    if (cached_ptr != NULL) {                                                \
      /* Move the existing entry to an overflow entry. */                    \
      a_map_index  new_index;                                                \
      if ((map)->next_free == 0) {                                           \
        expand_map(map);                                                     \
      }  /* if */                                                            \
      new_index = (map)->next_free;                                          \
      (map)->next_free = table[new_index].next_index;                        \
      table[new_index].ptr = cached_ptr;                                     \
      table[new_index].data = table[idx].data;                               \
      table[new_index].next_index = table[idx].next_index;                   \
      table[idx].next_index = new_index;                                     \
    }  /* if */                                                              \
    table[idx].ptr = (a_byte*)(iptr);                                        \
    table[idx].data.byte_count = (bcount);                                   \
  }

/*
Convenience macro to map an IL pointer (iptr) to a pointer (sptr) to stack
storage associated.  ips is the interpreter state managing the stack storage.

If an existing mapping exists for iptr, the new mapping supersedes it (until
it is unmapped).
*/
#define map_stack_bytes(ips, iptr, sptr)                                     \
  map_ptr(&(ips)->map, iptr, sptr)


static void unmap_overflow_entry(a_data_map   *map,
                                 a_byte       *ptr,
                                 a_map_index  idx)
/*
Search for ptr in the overflow section of the given map and, if found, remove
the associated entry.
*/
{
  a_data_map_entry  *table = map->table;
  a_map_index       last_index = 0;

  for (;;) {
    if (table[idx].ptr == ptr) {
      /* We found the searched-for entry.  Unlink it. */
      if (last_index == 0) {
        last_index = hash_il_ptr(ptr);
      }  /* if */
      table[last_index].next_index = table[idx].next_index;
      /* Recycle the unlinked entry. */
      table[idx].next_index = map->next_free;
      map->next_free = idx;
      break;
    } else {
      last_index = idx;
      idx = table[idx].next_index;
      if (idx == 0) {
        /* We've exhausted the list of entries. */
        break;
      }  /* if */
    }  /* if */
  }  /* for */
}  /* unmap_overflow_entry */


/*
Macro to remove an entry associated with iptr from a given data map.
*/
#define unmap_ptr(map, iptr)                                                 \
  { a_byte_count      idx = hash_il_ptr(iptr);                               \
    a_data_map_entry  *table = (map)->table;                                 \
    a_byte            *cached_ptr = table[idx].ptr;                          \
    if (cached_ptr == (a_byte*)(iptr)) {                                     \
      if (table[idx].next_index == 0) {                                      \
        /* Only one element in the bucket. */                                \
        table[idx].ptr = NULL;                                               \
      } else {                                                               \
        /* Move the first overflow entry to the main hash table. */          \
        a_map_index  old_index = table[idx].next_index;                      \
        table[idx].ptr = table[old_index].ptr;                               \
        table[idx].data = table[old_index].data;                             \
        table[idx].next_index = table[old_index].next_index;                 \
        /* Recycle the overflow entry. */                                    \
        table[old_index].next_index = (map)->next_free;                      \
        (map)->next_free = old_index;                                        \
      }  /* if */                                                            \
    } else {                                                                 \
      unmap_overflow_entry(map, (a_byte*)(iptr), table[idx].next_index);     \
    }  /* if */                                                              \
  }


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
			/* Next entry on this path (or NULL if there is
			   none. */
  a_field_ptr	field;
			/* The field selected for this variant path, or NULL
			   if this entry represents the base address of an
			   indexed array. */
  a_byte	*base_address;
			/* The address of the variant (i.e., union) subobject
			   in interpreter storage, or, if active_field is NULL,
			   the base address of the indexed array. */
} a_variant_path_entry;

static a_variant_path_entry_ptr
		free_variant_path_entries;

static unsigned long
		n_variant_path_entries;

static a_variant_path_entry_ptr alloc_variant_path_entry(void)
/*
Return new variant path entry.
*/
{
  a_variant_path_entry_ptr vpep;

  if (free_variant_path_entries != NULL) {
    vpep = free_variant_path_entries;
    free_variant_path_entries = free_variant_path_entries->next;
  } else {
    vpep = alloc_fe_of_type(a_variant_path_entry);
    n_variant_path_entries += 1;
  }  /* if */
  return vpep;
}  /* alloc_variant_path_entry */


/*
A set of flags to describe special kinds of interpreter addresses.
*/
#define CA_RUNTIME_DATA_ADDRESS ((unsigned int)0x1)
		/* This flag indicates that the address is that of a run-time
		   entity (not a value known to the interpreter). */
#define CA_CANNOT_DEREFERENCE ((unsigned int)0x2)
		/* This flag indicates that the address cannot be dereferenced.
		   It is set in particular for pointers "on position past" the
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
		   therefore always for a bit field lvalue.) */
#define CA_SIGNED_BIT_FIELD ((unsigned int)0x20)
		/* This flag indicates that the address is that of a signed bit
		   field.  This flag is never set if the CA_BIT_FLAG is not
		   set. */
#define CA_FUNCTION ((unsigned int)0x40)
		/* This flag indicates that the address is that of a
		   function. */

/*
Structure describing the representation of an address in the interpreter.
(Addresses in the interpreter are used to represent pointers, references, and
lvalues.)
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
			/* If the CA_ARRAY_ELEMENT flag is set, the number of
			   elements in the array.  If the CA_BIT_FIELD flag is
			   set, the number of bits in the bit field designated
			   by this lvalue. */
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
} a_constexpr_address;


#define is_runtime_data_address(cap)                                         \
  ((((a_constexpr_address*)(cap))->flags & CA_RUNTIME_DATA_ADDRESS) != 0)

#define cannot_dereference(cap)                                              \
  ((((a_constexpr_address*)(cap))->flags & CA_CANNOT_DEREFERENCE) != 0)

#define is_variant_path(cap)                                                 \
  ((((a_constexpr_address*)(cap))->flags & CA_VARIANT_PATH) != 0)

#define is_array_element(cap)                                                \
  ((((a_constexpr_address*)(cap))->flags & CA_ARRAY_ELEMENT) != 0)

#define is_function_address(cap)                                             \
  ((((a_constexpr_address*)(cap))->flags & CA_FUNCTION) != 0)


#define get_base_address(cap)                                                \
  (is_variant_path(cap) ? (cap)->variant.variant_path->base_address          \
                        : (cap)->variant.base_address)

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
  if ((addr)->flags & CA_BIT_FIELD) {                                         \
    trim_bit_field((addr)->address, (addr)->length,                           \
                   ((addr)->flags & CA_SIGNED_BIT_FIELD) != 0);               \
  }  /* if */                                                                 \
}


/*
Macro to initialize a constant address at addr referring to the interpreter
value at targ_addr.
*/
#define clear_address(addr, targ_addr)                    \
  memzero((char *)(addr), sizeof(a_constexpr_address));   \
  ((a_constexpr_address *)(addr))->address = (targ_addr);

#if 0
/* FIXME -- Not needed yet: disabled so lint won't complain. */
/*
Macro to initialize a constant address at addr referring to the array
element at targ_addr, which is a member of the interpreter array of len
elements starting at base.
*/
#define clear_array_address(addr, targ_addr, len, base)           \
  clear_address((addr), targ_addr);                               \
  ((a_constexpr_address *)(addr))->is_array = TRUE;               \
  ((a_constexpr_address *)(addr))->length = (len);                \
  ((a_constexpr_address *)(addr))->variant.base_address = (base);
#endif /* 0 */


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


static void info_call_stack(an_interpreter_state  *ips)
/*
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
  more_info_diagnostic(err_code, pos, &ips->diag_list);
  info_call_stack(ips);
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
  more_info_type_diagnostic(err_code, pos, tp, &ips->diag_list);
  info_call_stack(ips);
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
  more_info_type2_diagnostic(err_code, pos, tp1, tp2, &ips->diag_list);
  info_call_stack(ips);
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
  more_info_num_diagnostic(err_code, pos, num, &ips->diag_list);
  info_call_stack(ips);
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
  more_info_num2_diagnostic(err_code, pos, num1, num2, &ips->diag_list);
  info_call_stack(ips);
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
  more_info_sym_diagnostic(err_code, pos, sym, &ips->diag_list);
  info_call_stack(ips);
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
  more_info_sym_type_diagnostic(err_code, pos, sym, type, &ips->diag_list);
  info_call_stack(ips);
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
  more_info_sym2_diagnostic(err_code, pos, sym1, sym2, &ips->diag_list);
  info_call_stack(ips);
}  /* info_with_pos_sym */


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

static void info_one_past_end_of_array(a_constexpr_address   *addr,
                                       an_expr_node_ptr      expr,
                                       an_interpreter_state  *ips)
/*
expr is an rvalue whose evaluation requires the indirection of addr, but it
turns out addr is pointing one position past an array.  Record diagnostic
information describing the problem.
*/
{
  a_byte_count  elem_size, pos;
  a_byte        *base_address;
  a_boolean     local_result = TRUE;

  elem_size = value_bytes_for_type(ips, expr->type, &local_result);
  check_assertion(local_result);
  base_address = get_base_address(addr);
  pos = (a_byte_count)(addr->address - base_address) / elem_size;
  info_with_pos_num(ec_constexpr_access_one_past_array_end, &expr->position, 
                    pos, ips);
}  /* info_one_past_end_of_array */


/*
Macro defining the largest allowed size of a type in the interpreter.
*/
#define MAX_CONSTEXPR_TYPE_SIZE ((a_byte_count)(1<<20))

/*
The interpreter's stack allocator is pretty efficient, but many "compact"
operands can be allocated even more efficiently on the call stack.  To manage
this, we create a union of the value types allocated on the stack, and macros
to declare and access a corresponding array of bytes.
*/
typedef union a_compact_value_sizing_model {
  an_integer_value	 iv;
	/*lint -esym(754, a_compact_value_sizing_model::iv)*/
  an_internal_float_value
			 ifv;
	/*lint -esym(754, a_compact_value_sizing_model::ifv)*/
  a_constexpr_address	 ca;
	/*lint -esym(754, a_compact_value_sizing_model::ca)*/
  a_constexpr_ptr_to_mem cptm;
	/*lint -esym(754, a_compact_value_sizing_model::cptm)*/
} a_compact_value_sizing_model;

#define is_compact_value_size(n)  (n <= sizeof(a_compact_value_sizing_model))

#ifdef __GNUC__
/* Use GCC attributes to control alignment. */
#define DECL_COMPACT_VALUE_BYTES(buf_name)                                   \
   __attribute((aligned(__alignof(a_compact_value_sizing_model))))           \
     a_byte buf_name[sizeof(a_compact_value_sizing_model)]

#define compact_value_bytes(buf_name) (buf_name)

#else /* !defined(__GNUC__) */
#ifdef _MSVC_VER
/* Use Microsoft __declspec to control alignment. */
#define DECL_COMPACT_VALUE_BYTES(buf_name)                                   \
   __declspec(align(__alignof(a_compact_value_sizing_model)))                \
     a_byte buf_name[sizeof(a_compact_value_sizing_model)]

#define compact_value_bytes(buf_name) (buf_name)

#else /* !defined(_MSC_VER) */
/* Use a union to align a byte buffer. */
typedef union a_compact_value {
  a_compact_value_sizing_model
		alignment_model;
	/*lint -esym(754, a_compact_value::alignment_model)*/
  a_byte	buf[sizeof(a_compact_value_sizing_model)];
} a_compact_value;

#define DECL_COMPACT_VALUE_BYTES(buf_name)                                   \
  a_compact_value buf_name

#define compact_value_bytes(buf_name) ((buf_name).buf)

#endif /* ifdef _MSC_VER */
#endif /* ifdef __GNUC__ */


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
      result = sizeof(a_constexpr_address);
      break;
    case tk_array:
      if (!tp->variant.array.is_variable_size_array) {
        a_targ_size_t  n_elems = num_array_elements(tp);
        a_type_ptr     etp = underlying_array_element_type(tp);
        etp = skip_typerefs(etp);
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
          *p_result = FALSE;
        } else {
          result *= (a_byte_count)n_elems;
        }  /* if */
      } else {
        a_source_position  *pos = &tp->source_corresp.decl_position;
#if DEBUG
        check_assertion(ips != NULL);
#endif /* DEBUG */
        if (pos->seq == 0) pos = &ips->position;
        info_with_pos(ec_constexpr_vla, pos, ips);
        *p_result = FALSE;
        result = 0;
      }  /* if */
      break;
    case tk_class:
    case tk_struct:
      get_mapped_byte_count(&persistent_map, tp, result);
      if (result == 0) {
        result = lay_out_class_type(ips, tp, p_result);
      }  /* if */
      break;
    case tk_union:
      get_mapped_byte_count(&persistent_map, tp, result);
      if (result == 0) {
        result = lay_out_union_type(ips, tp, p_result);
      }  /* if */
      break;
    case tk_typeref:
      tp = tp->variant.typeref.type;
      goto redo;
    case tk_ptr_to_member:
      result = sizeof(a_constexpr_ptr_to_mem);
      break;
    case tk_nullptr:
      result = 1;
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
          *p_result = FALSE;
        } else {
          result *= (a_byte_count)n_elems;
        }  /* if */
      }
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tk_error:
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
        *p_result = FALSE;
        result = 0;
      }
      break;
    case tk_template_param:
    case tk_unknown:
      /* Fail interpretation. */
      result = 0;
      *p_result = FALSE;
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
  for (fp = next_initializable_field(fp);
       fp != NULL;
       fp = next_initializable_field(fp->next)) {
    if (fp->compiler_generated && !fp->is_anonymous_parent_object) {
      /* Ignore fields generated by prelowering. */
      continue;
    }  /* if */
    do_host_alignment(total_size);
    map_byte_count(&persistent_map, fp, total_size);
    total_size += value_bytes_for_type(ips, fp->type, p_result);
    if (total_size >= MAX_CONSTEXPR_TYPE_SIZE) {
      a_source_position  *pos = &tp->source_corresp.decl_position;
      if (pos->seq == 0) pos = &ips->position;
      info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
      *p_result = FALSE;
      total_size = MAX_CONSTEXPR_TYPE_SIZE;
      goto done;
    }  /* if */
  }  /* for */
  /* Allocate each direct, nonvirtual base class. */
  for (bcp = bases; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct && !bcp->is_virtual) {
      do_host_alignment(total_size);
      map_byte_count(&persistent_map, bcp, total_size);
      total_size += value_bytes_for_type(ips, bcp->type, p_result);
      if (total_size >= MAX_CONSTEXPR_TYPE_SIZE) {
        a_source_position  *pos = &tp->source_corresp.decl_position;
        if (pos->seq == 0) pos = &ips->position;
        info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
        *p_result = FALSE;
        total_size = MAX_CONSTEXPR_TYPE_SIZE;
        goto done;
      }  /* if */
    }  /* if */
  }  /* for */
  if (any_virtual_bases) {
    /* Allocate virtual base classes. */
    for (bcp = bases; bcp != NULL; bcp = bcp->next) {
      if (bcp->direct && !bcp->is_virtual) {
        do_host_alignment(total_size);
        map_byte_count(&persistent_map, bcp, total_size);
        total_size += value_bytes_for_type(ips, bcp->type, p_result);
        if (total_size >= MAX_CONSTEXPR_TYPE_SIZE) {
          a_source_position  *pos = &tp->source_corresp.decl_position;
          if (pos->seq == 0) pos = &ips->position;
          info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
          *p_result = FALSE;
          total_size = MAX_CONSTEXPR_TYPE_SIZE;
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
    map_byte_count(&persistent_map, fp, prefix_size);
    if (field_size > max_field_size) max_field_size = field_size;
  }  /* for */
  total_size = prefix_size+max_field_size;
  if (total_size >= MAX_CONSTEXPR_TYPE_SIZE) {
    a_source_position  *pos = &tp->source_corresp.decl_position;
    if (pos->seq == 0) pos = &ips->position;
    info_with_pos_type(ec_constexpr_type_too_large, pos, tp, ips);
    *p_result = TRUE;
    total_size = MAX_CONSTEXPR_TYPE_SIZE;
  }  /* if */
  map_byte_count(&persistent_map, tp, total_size);
  return total_size;
}  /* lay_out_union_type */


#define record_subobject_derivation(subobj_ptr, bcp)                         \
  *(void**)(subobj_ptr) = (void*)(bcp);

/*
Class type subobjects record the type of the next-more-derived subobject, or
NULL for a most-derived object.  The following macro records that NULL (for
proper base subobjects, the next-more-derived type is recorded when the base
subobject is initializer).  For unions, the recorded pointer represents the
active field rather than a base class entry.  (This must therefore be
invoked before placing a result in the indicated storage, because that result
could set the active field.)
*/
#define record_complete_object(utp, storage_ptr)                             \
  if (is_immediate_class_type(utp)) {                                        \
    record_subobject_derivation(storage_ptr, NULL);                          \
  }  /* if */                                                                \

#define alloc_complete_object(ips, n_bytes, utp, storage_ptr)                \
  {                                                                          \
    alloc_stack_bytes(ips, n_bytes, storage_ptr);                            \
    record_complete_object(utp, storage_ptr);                                \
  }

#if DEBUG

uintptr_t db_hash_ptr(void  *ptr)
/*
Debug routine to compute a hash value from within a debugger.
*/
{
  return hash_il_ptr(ptr);
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
        (void)fprintf(f_debug, "address 0x%p:\n", cap->address);
        db_indent(indent+2);
        (void)fprintf(f_debug, "flags 0x%x:\n", cap->flags);
        if (is_array_element(cap)) {
          db_indent(indent+2);
          (void)fprintf(f_debug, "length %d:\n", cap->length);
        }  /* if */
        db_indent(indent+2);
        (void)fprintf(f_debug, "alloc seq# %d:\n", cap->alloc_seq_number);
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
          (void)fprintf(f_debug, "%d:\n", offset/e_bytes);
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
        for (fp = next_initializable_field(fp);
             fp != NULL;
             fp = next_initializable_field(fp->next)) {
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
interpreter state converted to an_interpreter_state* (to avoid having to
expose an_interpreter_state in outside this source file).
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

#endif /* DEBUG */

static a_boolean add_to_variant_path(a_constexpr_address  *addr,
                                     a_field_ptr          union_field)
/*
The given field of a union object or subobject pointed to by addr is being
selected.  Add that field to the variant path associated with addr (and, if
this is the first field added to the path, also add a prefix field for
array element selections).
*/
{
  a_variant_path_entry_ptr  *p_path_ptr;

  if (addr->flags & CA_VARIANT_PATH) {
    /* This entry already has a variant path: Find its end. */
    p_path_ptr = &addr->variant.variant_path->next;
    while (*p_path_ptr) {
      p_path_ptr = &(*p_path_ptr)->next;
    }  /* while */
  } else {
    /* No entries yet: Create a first entry to record an array base address if
       needed. */
    addr->variant.variant_path = alloc_variant_path_entry();
    p_path_ptr = &addr->variant.variant_path->next;
    addr->flags |= CA_VARIANT_PATH;
  }  /* if */
  /* Add the new entry. */
  *p_path_ptr = alloc_variant_path_entry();
  (*p_path_ptr)->next = NULL;
  (*p_path_ptr)->field = union_field;
  (*p_path_ptr)->base_address = addr->address;
  return TRUE;
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
    *p_vpep = alloc_variant_path_entry();
    **p_vpep = *vpep;
    p_vpep = &(*p_vpep)->next;
    vpep = vpep->next;
  } while (vpep != NULL);
}  /* copy_variant_path */


static void release_variant_path(a_constexpr_address  *addr)
/*
Release the variant path entries associated with the given address.  The caller
is responsible for ensuring that there are such entries.
*/
{
  a_variant_path_entry_ptr  entries, vpep;

  entries = addr->variant.variant_path;
  vpep = entries->next;
  while (vpep->next != NULL) {
    vpep = vpep->next;
  }  /* while */
  vpep->next = free_variant_path_entries;
  free_variant_path_entries = entries;
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
      result = FALSE;
      info_with_pos_sym2(ec_constexpr_union_field_inactive, pos,
                         symbol_for(selected_field), symbol_for(active_field),
                         ips);
      break;
    }  /* if */
    vpep = vpep->next;
  } while (vpep != NULL);
  if (release) {
    release_variant_path(addr);
  }  /* if */
  return result;
}  /* check_variant_path */


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
#define do_constexpr_full_expression(ips, expr, result_storage, result_flag)  \
  {                                                                           \
    a_storage_stack_state  saved_stack_for_full_expr;                         \
    save_storage_stack(ips, saved_stack_for_full_expr);                       \
    (result_flag) = do_constexpr_expression(ips, expr, result_storage);       \
    restore_storage_stack(ips, saved_stack_for_full_expr);                    \
  }

static a_boolean do_constexpr_expression(
                                       an_interpreter_state  *ips,
                                       an_expr_node_ptr      expr,
                                       a_byte                *result_storage);


static a_boolean do_constexpr_ctor(an_interpreter_state  *ips,
                                   a_dynamic_init_ptr    dip,
                                   a_source_position     *pos,
                                   a_byte                *result_storage);


static a_boolean do_constexpr_dynamic_init(
                                        an_interpreter_state  *ips,
                                        a_dynamic_init_ptr    dip,
                                        a_source_position     *pos,
                                        a_byte                *result_storage);


/*
Macro to set result_storage from the value of the specified constant.
Duplicates some cases from extract_value_from_constant for performance
reasons.
*/
#define copy_val_from_constant(ips, con, result_storage)                      \
  (                                                                           \
    ((con)->kind == (a_constant_repr_kind)ck_integer) ?                       \
      (*(an_integer_value *)(result_storage) = (con)->variant.integer_value,  \
       TRUE):                                                                 \
    ((con)->kind == (a_constant_repr_kind)ck_float) ?                         \
      ((*fp_value(result_storage) = (con)->variant.float_value), TRUE) :      \
    /* else */                                                                \
      extract_value_from_constant(ips, con, result_storage)                   \
  )  /* copy_val_from_constant */


static a_boolean extract_value_from_constant(an_interpreter_state  *ips,
                                             a_constant_ptr        con,
                                             a_byte                *value)
/*
Copy the value of con into the interpreter storage at value, converting
formats as necessary.  Return FALSE if the constant is an error constant.
*/
{
  a_boolean  result = TRUE;

  if (con->implicit_cast && con->expr != NULL) {
    /* If the constant includes an implicit cast, evaluate the constant
       through the backing expression so that the cast is correctly applied. */
    do_constexpr_full_expression(ips, con->expr, value, result);
    goto done;
  }  /* if */
  switch (con->kind) {
    case ck_error:
      result = FALSE;
      break;
    case ck_integer:
      *(an_integer_value *)value = con->variant.integer_value;
      break;
    case ck_float:
      *fp_value(value) = con->variant.float_value;
      break;
    case ck_address:
      switch (con->variant.address.kind) {
        case abk_routine:
          make_function_address(value, con->variant.address.variant.routine);
          break;
        case abk_variable:
          { /* Check if the variable has a constant value, and if so ensure
               it has a representation in static interpreter storage.
               Otherwise, create a run-time address. */
            a_variable_ptr  vp = con->variant.address.variant.variable;
            if (vp->constant_valued) {
              a_byte  *var_bytes;
              get_stack_bytes(ips, vp, var_bytes);
              if (var_bytes == NULL) {
                a_type_ptr    vtp = skip_typerefs(vp->type);
                a_byte_count  n_bytes;
                if (!ips->static_storage_ready) {
                  /* This is the first time we allocate static storage:
                     Initialize the associated static storage stack. */
                  init_constexpr_stack(&ips->static_storage);
                  ips->static_storage_ready = TRUE;
                }  /* if */
                n_bytes = value_bytes_for_type(ips, vtp, &result);
                if (result) {
                  a_constant_ptr  cp = NULL;
                  alloc_bytes(&ips->static_storage, n_bytes, var_bytes);
                  if (vp->init_kind == (an_init_kind)initk_static) {
                    cp = vp->initializer.constant;
                  } else if (vp->init_kind == (an_init_kind)initk_dynamic) {
                    cp = vp->initializer.dynamic->variant.constant;
                  } else {
                    unexpected_condition();
                  }  /* if */
                  result = extract_value_from_constant(ips, cp, var_bytes);
                }  /* if */
                if (!result) break;
                map_stack_bytes(ips, vp, var_bytes);
              }  /* if */
              clear_address(value, var_bytes);
            } else {
              clear_runtime_constant_address(value, con);
            }  /* if */
          }
          break;
        case abk_constant:
        case abk_temporary:
          {
            a_constant_ptr  cp = con->variant.address.variant.constant;
            a_byte          *con_bytes;
            get_stack_bytes(ips, cp, con_bytes);
            if (con_bytes == NULL) {
              a_type_ptr    ctp = skip_typerefs(cp->type);
              a_byte_count  n_bytes;
              if (!ips->static_storage_ready) {
                /* This is the first time we allocate static storage:
                   Initialize the associated static storage stack. */
                init_constexpr_stack(&ips->static_storage);
                ips->static_storage_ready = TRUE;
              }  /* if */
              n_bytes = value_bytes_for_type(ips, ctp, &result);
              if (result) {
                alloc_bytes(&ips->static_storage, n_bytes, con_bytes);
                result = extract_value_from_constant(ips, cp, con_bytes);
              }  /* if */
              if (!result) break;
              map_stack_bytes(ips, cp, con_bytes);
            }  /* if */
            clear_address(value, con_bytes);
          }
          break;
        default:
          { /* Create an a_constexpr_address for the runtime constant, which
               requires a local constant. */
            clear_runtime_constant_address(value, con);
          }
          break;
      }  /* switch */
      break;
    case ck_ptr_to_member:
      { a_base_class_ptr  bcp = con->variant.ptr_to_member.casting_base_class;
        a_constexpr_ptr_to_mem
                          *pm_value = (a_constexpr_ptr_to_mem*)value;
        a_byte_count      offset = 0;
        if (con->variant.ptr_to_member.is_function_ptr) {
          pm_value->variant.routine =
                                   con->variant.ptr_to_member.variant.routine;
          pm_value->is_ptr_to_mem_function = TRUE;
        } else {
          pm_value->variant.field = con->variant.ptr_to_member.variant.field;
          pm_value->is_ptr_to_mem_function = FALSE;
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
                                           value);
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
        elem_size = value_bytes_for_type(
                                ips, tp->variant.array.element_type, &result);
        char_ptr = con->variant.string.value;
        for (k = 0; k<n_elems; k += 1) {
          unsigned long char_val = extract_character_from_string(
                                           char_ptr, (unsigned int)char_size);
          set_integer_value((an_integer_value*)value,
                            (a_host_large_integer)char_val);
          value += elem_size;
        }  /* for */
      }
      break;
    case ck_aggregate:
      {
        a_type_ptr  tp = skip_typerefs(con->type);
        if (tp->kind == (a_type_kind)tk_array) {
          a_targ_size_t   n_elems, k;
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
              memzero(value, size_t_arg((n_elems-k)*elem_size));
              break;
            } else {
              record_complete_object(etp, value);
              if (!copy_val_from_constant(ips, elem_con, value)) {
                result = FALSE;
                break;
              }  /* if */
            }  /* if */
            elem_con = elem_con->next;
            k += 1;
            value += elem_size;
          }  /* for */
        } else if (tp->kind == (a_type_kind)tk_struct ||
                   tp->kind == (a_type_kind)tk_class) {
          a_field_ptr       fp = tp->variant.class_struct_union.field_list;
          a_base_class_ptr  bcp = base_classes_of(tp);
          a_constant_ptr    elem_con;
          elem_con = con->variant.aggregate.first_constant;
          if (bcp != NULL &&
              elem_con->constant_for_base_class_from_constexpr_folding) {
            for (;;) {
              a_byte_count  offset;
              while (!bcp->direct) {
                bcp = bcp->next;
              }  /* while */
              get_mapped_byte_count(&persistent_map, bcp, offset);
              if (!copy_val_from_constant(ips, elem_con, value+offset)) {
                result = FALSE;
                break;
              }  /* if */
              /* Record the derivation step (not really needed if this is for
                 an assignment rather than for an initialization, but it is 
                 easier -- and probably cheaper -- to do this
                 indiscriminately). */
              record_subobject_derivation(value+offset, bcp);
              bcp = bcp->next;
              elem_con = elem_con->next;
              if (bcp == NULL ||
                  !elem_con->constant_for_base_class_from_constexpr_folding) {
                break;
              }  /* if */
            }  /* for */
          }  /* if */
          for (;;) {
            a_byte_count  offset;
            fp = next_initializable_field(fp);
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
              a_byte_count  class_size = value_bytes_for_type(ips, tp,
                                                              &result);
              if (result) {
                memzero(value+offset, size_t_arg(class_size-offset));
              }  /* if */
              break;
            } else if (!copy_val_from_constant(ips, elem_con, value+offset)) {
              result = FALSE;
              break;
            } else if (fp->is_bit_field) {
              /* Fit the value in the bit field width. */
              trim_bit_field(value+offset, fp->bit_size,
                             fp->bit_field_is_signed);
            }  /* if */
            fp = fp->next;
            elem_con = elem_con->next;
          }  /* for */
        } else if (tp->kind == (a_type_kind)tk_union) {
          /* Initialize the first field (unless another field is
             designated). */
          a_field_ptr     fp;
          a_constant_ptr  elem_con;
          a_byte_count    offset;
          elem_con = con->variant.aggregate.first_constant;
          if (elem_con->kind == (a_constant_repr_kind)ck_designator) {
            fp = elem_con->variant.designator.field;
            elem_con = elem_con->next;
          } else {
            fp = tp->variant.class_struct_union.field_list;
            fp = next_initializable_field(fp);
          }  /* if */
          if (fp == NULL || elem_con == NULL || elem_con->next != NULL) {
            /* Unions should have only one actual initializer constant
               (possibly following a designator). */
            unexpected_condition();
          }  /* if */
          get_mapped_byte_count(&persistent_map, fp, offset);
          if (!copy_val_from_constant(ips, elem_con, value+offset)) {
            result = FALSE;
          } else {
            if (fp->is_bit_field) {
              /* Fit the value in the bit field width. */
              trim_bit_field(value+offset, fp->bit_size,
                             fp->bit_field_is_signed);
            }  /* if */
            /* Record the active field. */
            *(a_field_ptr*)value = fp;
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
            if (!copy_val_from_constant(ips, elem_con, value)) {
              result = FALSE;
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
          result = FALSE;
        }  /* if */
      }
      break;
    default:
      { a_source_position  *diag_pos = &con->source_corresp.decl_position;
        if (diag_pos->seq == 0) {
          diag_pos = &ips->position;
        }  /* if */
        info_with_pos(ec_constexpr_invalid_constant_kind, diag_pos, ips);
        result = FALSE;
      }
  }  /* switch */
done:
  return result;
}  /* extract_value_from_constant */


static a_boolean constexpr_copy_object(an_interpreter_state  *ips,
                                       a_type_ptr            tp,
                                       a_byte                *src_bytes,
                                       a_byte                *dst_bytes)
/*
Copy an object of the given type from one interpreter storage location
(src_bytes) to another (dst_bytes).
*/
{
  a_boolean     result = TRUE;
  a_byte_count  n_bytes = value_bytes_for_type(ips, tp, &result);

  if (result) {
    (void)memcpy(dst_bytes, src_bytes, size_t_arg(n_bytes));
    /* FIXME: Adjust addresses in object? */
  }  /* if */
  return result;
}  /* constexpr_copy_object */


static a_boolean do_constexpr_dynamic_init(
                                        an_interpreter_state  *ips,
                                        a_dynamic_init_ptr    dip,
                                        a_source_position     *pos,
                                        a_byte                *result_storage)
/*
Evaluate the given dynamic initialization for the given storage.
*/
{
  a_boolean  result = FALSE;

  switch (dip->kind) {
    case dik_constant:
    case dik_nonconstant_aggregate:
      result = copy_val_from_constant(ips, dip->variant.constant,
                                      result_storage);
      break;
    case dik_expression:
    case dik_class_result_via_ctor:
      result = do_constexpr_expression(ips, dip->variant.expression,
                                       result_storage);
      break;
    case dik_constructor:
      result = do_constexpr_ctor(ips, dip, pos, result_storage);
      break;
    case dik_bitwise_copy:
      { an_expr_node_ptr  source_expr = dip->variant.bitwise_copy.source;
        if (source_expr != NULL) {
          result = do_constexpr_expression(ips, source_expr, result_storage);
        } else {
          /* An implicit source: The caller should catch those cases. */
          unexpected_condition();
        }  /* if */
      }
      break;
    case dik_zero:
    case dik_none:
      /* Nothing to do, but check that there is no associated destructor. */
      if (dip->destructor != NULL) {
        info_with_pos(ec_constexpr_ctor_with_dtor, pos, ips);
        result = FALSE;
      } else {
        result = TRUE;
      }  /* if */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* do_constexpr_dynamic_init */


static a_boolean do_constexpr_statement(an_interpreter_state  *ips,
                                        a_statement_ptr       stmt);


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
       their corresponding stmk_init statement is interpreter), save the
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
        a_byte          *var_bytes;
        get_stack_bytes(ips, vp, var_bytes);
        release_variant_path_if_needed(var_bytes);
      }  /* if */
      unmap_stack_bytes(ips, vp);
      unmap_ptr(&ips->map, &vp->storage_class);
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
  a_storage_stack_state  saved_stack;
  a_for_loop_ptr         loop_info = stmt->variant.for_loop.extra_info;
  a_statement_ptr        init = loop_info->initialization;

  /* We may have to allocate storage for the increment expression result and/or
     variables declared in the for-init declaration.  Save the current storage
     state to enable deallocation when we're done. */
  save_storage_stack(ips, saved_stack);
  /* Run the initialization statement (if any). */
  if (init != NULL && !do_constexpr_statement(ips, init)) {
    result = FALSE;
  } else {
    an_expr_node_ptr  expr = stmt->expr, incr = loop_info->increment;
    a_byte            *expr_value, *incr_value;
    a_type_ptr        tp, incr_type;
    a_byte_count      n_bytes;
    a_boolean         ovfl;
    a_host_large_integer
                      bool_val;
    DECL_COMPACT_VALUE_BYTES(expr_bytes);
    DECL_COMPACT_VALUE_BYTES(incr_bytes);
    if (expr != NULL) {
      /* The type of the test expression is known to be bool, which will
         fit within the expr_bytes array. */
      expr_value = compact_value_bytes(expr_bytes);
      tp = skip_typerefs(expr->type);
    } else {
      /* Needed only to avoid spurious GNU compiler optimizer
         warnings. */
      expr_value = compact_value_bytes(expr_bytes);
      tp = NULL;
    }  /* if */
    incr = loop_info->increment;
    if (incr != NULL) {
      incr_type = skip_typerefs(incr->type);
      n_bytes = value_bytes_for_type(ips, incr_type, &result);
      if (!result) goto unmap_storage;
      if (!is_compact_value_size(n_bytes) &&
          !incr->is_lvalue && !incr->is_xvalue) {
        /* The result of the increment expression is larger than a scalar
           type, so allocate space for it on the stack. */
        alloc_stack_bytes(ips, n_bytes, incr_value);
      } else {
        incr_value = compact_value_bytes(incr_bytes);
      }  /* if */
      record_complete_object(incr_type, incr_value);
    } else {
      /* Needed only to avoid spurious GNU compiler optimizer warnings. */
      incr_type = NULL;
      incr_value = compact_value_bytes(incr_bytes);
    }  /* if */
    do {
      /* Evaluate the test expression. */
      if (cost_exceeded(ips)) {
        more_info_diagnostic(ec_excessive_constexpr_complexity, &ips->position,
                             &ips->diag_list);
        result = FALSE;
      } else if (expr != NULL) {
        do_constexpr_full_expression(ips, expr, expr_value, result);
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
          /* Execute the dependent statement. */
          result = do_constexpr_statement(
                                       ips, stmt->variant.for_loop.statement);
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
              do_constexpr_full_expression(ips, incr, incr_value, result);
              release_address_structures(incr, incr_type,
                                                  incr_value);
            }  /* if */
          }  /* if */
        }  /* if */
      }   /* if */
    } while (result && bool_val);
  }  /* if */
unmap_storage:
  { /* Unmap the local storage if necessary. */
    a_scope_ptr  init_scope = loop_info->for_init_scope;
    if (init_scope != NULL) {
      a_variable_ptr  vp = init_scope->nonstatic_variables;
      if (vp != NULL) {
        do {
          unmap_stack_bytes(ips, vp);
          unmap_ptr(&ips->map, &vp->storage_class);
          vp = vp->next;
        } while (vp != NULL);
      }  /* if */
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
  for (k = 0; k<4; ++k) {
    a_type_ptr    vtp = skip_typerefs(vp[k]->type);
    a_byte_count  n_bytes = value_bytes_for_type(ips, vtp, &result);
    alloc_complete_object(ips, n_bytes, vtp, var_storage[k]);
    /* Associate with the variable its value storage. */
    map_stack_bytes(ips, vp[k], var_storage[k]);
    /* Also associate with the variable (somewhat arbitrarily, with its
       "storage_class" field) an allocation sequence number that may be
       used to detect leaks. */
    map_byte_count(&ips->map, &vp[k]->storage_class,
                   ips->curr_alloc_seq_number);
  }  /* for */
  if (!result) goto unmap_storage;
  /* Initialize the range and its delimiters: */
  for (k = 1; k<4; ++k) {
    dip = vp[k]->initializer.dynamic;
    if (!do_constexpr_dynamic_init(ips, dip, &stmt->position,
                                   var_storage[k])) {
      result = FALSE;
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
    DECL_COMPACT_VALUE_BYTES(expr_bytes);
    DECL_COMPACT_VALUE_BYTES(incr_bytes);
    expr_value = compact_value_bytes(expr_bytes);
    n_bytes = value_bytes_for_type(ips, incr_type, &result);
    if (!is_compact_value_size(n_bytes) &&
        !incr->is_lvalue && !incr->is_xvalue) {
      /* The result of the increment expression is larger than a scalar type,
         so allocate space for it on the stack. */
      alloc_stack_bytes(ips, n_bytes, incr_value);
    } else {
      incr_value = compact_value_bytes(incr_bytes);
    }  /* if */
    record_complete_object(incr_type, incr_value);
    if (!result) goto unmap_storage;
    dip = vp[0]->initializer.dynamic;
    do {
      /* Evaluate the test expression. */
      if (cost_exceeded(ips)) {
        more_info_diagnostic(ec_excessive_constexpr_complexity, &ips->position,
                             &ips->diag_list);
        result = FALSE;
      } else {
        do_constexpr_full_expression(ips, expr, expr_value, result);
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
                                         var_storage[0])) {
            result = FALSE;
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
              do_constexpr_full_expression(ips, incr, incr_value, result);
              release_address_structures(incr, incr_type,
                                                  incr_value);
            }  /* if */
          }  /* if */
        }  /* if */
      }   /* if */
    } while (result && bool_val);
  }  /* if */
unmap_storage:
  /* Release and unmap the local storage. */
  for (k = 4; k--;) {
    unmap_stack_bytes(ips, vp[k]);
    unmap_ptr(&ips->map, &vp[k]->storage_class);
  }  /* if */
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
  a_host_large_integer  bool_val;
  a_type_ptr            tp;
  a_boolean             ovfl;
  DECL_COMPACT_VALUE_BYTES(expr_bytes);

  switch (stmt->kind) {
    case stmk_expr:
      {
        a_byte_count  n_bytes;
        expr = stmt->expr;
        tp = skip_typerefs(expr->type);
        n_bytes = value_bytes_for_type(ips, tp, &result);
        save_storage_stack(ips, saved_stack);
        if (!is_compact_value_size(n_bytes) &&
            !expr->is_lvalue && !expr->is_xvalue) {
          /* The value is larger than a scalar type, so allocate space for
             it on the stack. */
          alloc_stack_bytes(ips, n_bytes, expr_value);
        } else {
          expr_value = compact_value_bytes(expr_bytes);
        }  /* if */
        record_complete_object(tp, expr_value);
        if (!result) {
          /* Stop interpretation. */
        } else if (!do_constexpr_expression(ips, expr, expr_value)) {
          result = FALSE;
        } else {
          release_address_structures(expr, tp, expr_value);
        }  /* if */
        restore_storage_stack(ips, saved_stack);
      }
      break;
    case stmk_if:
      {
        /* The type of the test expression is known to be bool, which will
           fit within the expr_bytes buffer. */
        expr = stmt->expr;
        expr_value = compact_value_bytes(expr_bytes);
        tp = skip_typerefs(expr->type);
        do_constexpr_full_expression(ips, expr, expr_value, result);
        release_address_structures(expr, tp, expr_value);
        if (result) {
          /* Evaluation of the test expression succeeded.  Get its value to
             see which dependent statement should be executed. */
          get_int_val_from(expr_value, tp, bool_val, ovfl);
          if (ovfl || bool_val) {
            /* Execute the "then" statement. */
            result = do_constexpr_statement(
                                   ips, stmt->variant.if_stmt.then_statement);
          } else if (stmt->variant.if_stmt.else_statement != NULL) {
            result = do_constexpr_statement(
                                   ips, stmt->variant.if_stmt.else_statement);
          }  /* if */
        }  /* if */
      }
      break;
    case stmk_while:
      {
        expr = stmt->expr;
        /* The type of the test expression is known to be bool, which will
           fit within the expr_bytes array. */
        expr_value = compact_value_bytes(expr_bytes);
        tp = skip_typerefs(expr->type);
        do {
          /* Evaluate the test expression. */
          if (cost_exceeded(ips)) {
            more_info_diagnostic(ec_excessive_constexpr_complexity,
                                 &ips->position, &ips->diag_list);
            result = FALSE;
          } else {
            do_constexpr_full_expression(ips, expr, expr_value, result);
            release_address_structures(expr, tp, expr_value);
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
              }  /* if */
            }  /* if */
          }   /* if */
        } while (result && bool_val);
      }
      break;
    case stmk_goto:
      if (stmt->variant.label.ptr->break_label) {
        ips->curr_call_frame->break_active = TRUE;
      } else if (stmt->variant.label.ptr->continue_label) {
        ips->curr_call_frame->continue_active = TRUE;
      } else {
        info_with_pos(ec_constexpr_goto, &stmt->position, ips);
        result = FALSE;
      }  /* if */
      break;
    case stmk_label:
      /* Nothing to do. */
      break;
    case stmk_return:
      { a_call_frame_ptr  frame = ips->curr_call_frame;
        if (stmt->expr != NULL) {
          do_constexpr_full_expression(ips, stmt->expr, frame->result_storage,
                                       result);
        } else if (stmt->variant.return_dynamic_init != NULL) {
          /* Handle return_dynamic_init case. */
          result = do_constexpr_dynamic_init(ips,
                                             stmt->variant.return_dynamic_init,
                                             &stmt->position, 
                                             frame->result_storage);
        } else {
          /* Return without a value. */
          a_type_ptr  fn_type = frame->routine->type;
          fn_type = skip_typerefs(fn_type);
          if (!is_void_type(fn_type->variant.routine.return_type)) {
            info_with_pos(ec_constexpr_missing_return_value, &stmt->position,
                          ips);
            result = FALSE;
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
        expr = stmt->expr;
        /* The type of the test expression is known to be bool, which will
           fit within the expr_bytes array. */
        expr_value = compact_value_bytes(expr_bytes);
        tp = skip_typerefs(expr->type);
        do {
          /* Execute the dependent statement. */
          result = do_constexpr_statement(ips, stmt->variant.loop_statement);
          if (result) {
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
          }  /* if */
          /* Evaluate the test expression. */
          if (cost_exceeded(ips)) {
            more_info_diagnostic(ec_excessive_constexpr_complexity,
                                 &ips->position, &ips->diag_list);
            result = FALSE;
          } else {
            do_constexpr_full_expression(ips, expr, expr_value, result);
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
        a_boolean                is_signed;
        expr = stmt->expr;
        /* The type of the switch expression is known to be integral, which
           will fit within the expr_bytes array. */
        expr_value = compact_value_bytes(expr_bytes);
        tp = skip_typerefs(expr->type);
        is_signed = int_kind_is_signed[tp->variant.integer.int_kind];
        do_constexpr_full_expression(ips, expr, expr_value, result);
        release_address_structures(expr, tp, expr_value);
        /* Search through the ordered list of case labels for the one selected
           by the switch expression. */
        for (; scep != NULL; scep = scep->next_on_sorted_list) {
          DECL_COMPACT_VALUE_BYTES(case_buffer);
          a_byte  *case_bytes = compact_value_bytes(case_buffer);
          int cmp;
          result = copy_val_from_constant(ips, scep->case_value, case_bytes);
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
            result = copy_val_from_constant(ips, scep->range_end, case_bytes);
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
          } else if (!result) {
            /* Some interpretation error occurred. */
            goto done_with_switch;
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
        }  /* for */
      }
done_with_switch:
      break;
    case stmk_init:
      { a_dynamic_init_ptr     dip = stmt->variant.dynamic_init;
        a_variable_ptr         vp = dip->variable;
        a_type_ptr             vtp = skip_typerefs(vp->type);
        a_byte                 *var_storage;
        a_byte_count           n_bytes;
        a_storage_stack_state  saved_stack_for_full_expr;
        /* Allocate storage for the variable. */
        n_bytes = value_bytes_for_type(ips, vtp, &result);
        alloc_complete_object(ips, n_bytes, vtp, var_storage);
        /* Associate with the variable its value storage. */
        map_stack_bytes(ips, vp, var_storage);
        /* Also associate with the variable (somewhat arbitrarily, with its
           "storage_class" field) an allocation sequence number that may be
           used to detect leaks. */
        map_byte_count(&ips->map, &vp->storage_class,
                       ips->storage_stack.alloc_seq_number);
        /* Evaluate the initializer. */
        save_storage_stack((ips), saved_stack_for_full_expr);
        if (vp->extends_lifetime) {
          /* Start a new stack for temporaries in this expression, but keep a
             pointer to the original stack to allocate the lifetime-extended
             temporary. */
          init_constexpr_stack(&ips->storage_stack);
          ips->storage_stack.alloc_seq_number = ips->curr_alloc_seq_number;
          ips->extension_state = &saved_stack_for_full_expr;
        }  /* if */
        result = do_constexpr_dynamic_init(ips, dip, &stmt->position,
                                           var_storage);
        if (vp->extends_lifetime) {
          /* Release the ordinary storage stack blocks for this expression.
             The large blocks will be release by the call to
             restore_storage_stack below. */
          release_constexpr_stack(&ips->storage_stack);
        }  /* if */
        restore_storage_stack(ips, saved_stack_for_full_expr);
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
      result = FALSE;
      break;
    default:
      info_with_pos(ec_constexpr_statement_cannot_be_interpreted,
                    &stmt->position, ips);
      result = FALSE;
  }  /* switch */
  return result;
}  /* do_constexpr_statement */


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
        result = FALSE;
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
                                   a_byte                *result_storage)
/*
Interpret the given call node and place the result at the given storage.
Return TRUE if no error occurred; otherwise, return FALSE and update *ips
accordingly.
*/
{
  an_expr_node_ptr  callee_node, arg;
  a_routine_ptr     callee = NULL;
  a_boolean         result = TRUE;
  a_constexpr_ptr_to_mem
                    *pm_target = NULL;
  DECL_COMPACT_VALUE_BYTES(pm_buf);

  callee_node = call_node->variant.operation.operands;
  if (is_routine_node(callee_node)) {
    callee = node_routine(callee_node);
  } else if (node_operator_is(call_node, eok_dot_pm_call) ||
             node_operator_is(call_node, eok_points_to_pm_call)) {
    /* A call through a pointer-to-member function.  We'll determine the
       callee here, and adjust the "this" pointer later on. */
    a_byte  *pm_bytes = compact_value_bytes(pm_buf);
    pm_target = (a_constexpr_ptr_to_mem*)pm_bytes;
    if (do_constexpr_expression(ips, callee_node, pm_bytes)) {
      callee = pm_target->variant.routine;
      if (callee == NULL) {
        info_with_pos(ec_constexpr_null_callee, &callee_node->position, ips);
        result = FALSE;
        goto done;
      }  /* if */
    } else {
      result = FALSE;
      goto done;
    }  /* if */
  } else {
    a_constexpr_address  addr;
    if (do_constexpr_expression(ips, callee_node, (a_byte*)&addr)) {
      if (is_function_address(&addr)) {
        callee = addr.variant.routine;
        if (callee == NULL) {
          info_with_pos(ec_constexpr_null_callee, &callee_node->position, ips);
          result = FALSE;
          goto done;
        }  /* if */
      } else if (addr.address == NULL) {
        info_with_pos(ec_constexpr_null_callee, &callee_node->position, ips);
        result = FALSE;
        goto done;
      } else {
        unexpected_condition();
      }  /* if */
    } else {
      result = FALSE;
      goto done;
    }  /* if */
  }  /* if */
  /* Retrieve the routine scope, or issue an error. */
  if (!callee->is_constexpr) {
    info_with_pos_sym(ec_constexpr_call_to_nonconstexpr_function,
                      &callee_node->position, symbol_for(callee), ips);
    result = FALSE;
  } else if (callee->function_def_number == NULL_function_def_number) {
    info_with_pos_sym(ec_constexpr_function_undefined, &callee_node->position,
                      symbol_for(callee), ips);
    result = FALSE;
  } else if (cost_exceeded(ips)) {
    more_info_diagnostic(ec_excessive_constexpr_complexity, &ips->position,
                         &ips->diag_list);
    result = FALSE;
  } else {
    a_scope_ptr     callee_scope = scope_for_routine(callee);
    a_statement_ptr
                    block_stmt = callee_scope->assoc_block;
    a_call_frame    frame;
    a_variable_ptr  params = callee_scope->variant.routine.parameters,
                    param, this_var;
    a_byte_count    n_args = 0;
    a_byte          *arg_ptrs, **p_arg_ptr;
    an_alloc_seq_number
                    alloc_seq_number;
    unsigned long   up_front_cost;
    DECL_COMPACT_VALUE_BYTES(this_buf);
    /* Don't attempt to interpret a non-constexpr function.  The flag
       scope->is_constexpr_routine is set at the end of a constexpr function
       definition, so this also prevents the interpretation of a function that
       is not fully parsed (e.g., requested due to a recursive call in a
       constexpr function). */
    if (!callee_scope->is_constexpr_routine) {
      result = FALSE;
      info_with_pos_sym(ec_constexpr_call_not_interpretable,
                        &call_node->position, symbol_for(callee), ips);
      goto done;
    }  /* if */
    /* Account a relatively high cost for the call up-front, to limit the
       overall call depth.  When the call returns, that cost will be reduced
       to just "one". */
    up_front_cost = max_constexpr_call_cost/max_constexpr_call_depth+1;
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
       the second phase. */
    for (arg = callee_node; arg != NULL; arg = arg->next) {
      n_args += 1;
    }  /* for */
    alloc_stack_bytes(ips, n_args*sizeof(a_byte*), arg_ptrs);
    /* Phase 1: Allocate and evaluate the arguments. */
    p_arg_ptr = (a_byte**)arg_ptrs;
    arg = callee_node->next;
    this_var = callee_scope->variant.routine.this_param_variable;
    if (this_var != NULL) {
      a_byte      *this_bytes = compact_value_bytes(this_buf);
      a_type_ptr  tp = skip_typerefs(arg->type);
      *p_arg_ptr = this_bytes;
      p_arg_ptr += 1;
      if (arg->is_lvalue || arg->is_xvalue ||
          tp->kind == (a_type_kind)tk_pointer) {
        /* The usual case: An address is produced. */
        if (!do_constexpr_expression(ips, arg, this_bytes)) {
          result = FALSE;
          goto done;
        }  /* if */
      } else {
        /* The call is on a class rvalue.  E.g., "X().f();". */
        a_byte_count  n_bytes = value_bytes_for_type(ips, tp, &result);
        a_byte        *class_bytes;
        if (!result) goto done;
        alloc_complete_object(ips, n_bytes, tp, class_bytes);
        if (!do_constexpr_expression(ips, arg, class_bytes)) {
          result = FALSE;
          goto done;
        }  /* if */
        /* Store the address of the class in *this_bytes. */
        clear_address(this_bytes, class_bytes);
      }  /* if */
      if (pm_target != NULL &&
          !adjust_this_address(ips, (a_constexpr_address*)this_bytes,
                               pm_target, this_var->type, call_node)) {
        result = FALSE;
        goto done;
      }  /* if */
      arg = arg->next;
    }  /* if */
    for (; arg != NULL; arg = arg->next) {
      a_type_ptr    tp = skip_typerefs(arg->type);
      a_byte_count  n_bytes = value_bytes_for_type(ips, tp, &result);
      a_byte        *arg_bytes;
      a_boolean     restore_lvalue = FALSE, restore_xvalue = FALSE;
      alloc_complete_object(ips, n_bytes, tp, arg_bytes);
      *p_arg_ptr = arg_bytes;
      p_arg_ptr += 1;
      if (result) {
        if (!(tp->kind == (a_type_kind)tk_pointer &&
              tp->variant.pointer.is_reference)) {
          /* When a class-type argument is pass by-value via a copy constructor
             call, the argument is left as an lvalue.  Temporarily set it back
             to an rvalue. */
          if (arg->is_lvalue) {
            restore_lvalue = TRUE;
            arg->is_lvalue = FALSE;
          } else if (arg->is_xvalue) {
            restore_xvalue = TRUE;
            arg->is_xvalue = FALSE;
          }  /* if */
        }  /* if */
        if (!do_constexpr_expression(ips, arg, arg_bytes)) {
          result = FALSE;
        }  /* if */
        if (restore_lvalue) {
          arg->is_lvalue = TRUE;
        } else if (restore_xvalue) {
          arg->is_xvalue = TRUE;
        }  /* if */
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
    if (this_var != NULL) {
      map_stack_bytes(ips, this_var, *p_arg_ptr);
      map_byte_count(&ips->map, &this_var->storage_class, alloc_seq_number);
      p_arg_ptr += 1;
    }  /* if */
    for (param = params; param != NULL; param = param->next) {
      map_stack_bytes(ips, param, *p_arg_ptr);
      map_byte_count(&ips->map, &param->storage_class, alloc_seq_number);
      p_arg_ptr += 1;
    }  /* for */
    /* Set up the call frame. */
    push_call_frame(ips, &frame, callee, &call_node->position, result_storage);
    /* Run the function's top-level block statement. */
    if (block_stmt->kind != (a_statement_kind)stmk_block) {
      check_assertion(block_stmt->kind == (a_statement_kind)stmk_try_block);
      info_with_pos(ec_constexpr_try_block, &block_stmt->position, ips);
      result = FALSE;
    } else {
      result = do_constexpr_block_statement(ips, block_stmt, callee_scope);
    }  /* if */
    /* Release any address structures, if needed. */
    p_arg_ptr = (a_byte**)arg_ptrs;
    for (arg = callee_node->next; arg != NULL; arg = arg->next) {
      a_type_ptr  tp = skip_typerefs(arg->type);
      release_address_structures(arg, tp, *p_arg_ptr);
      p_arg_ptr += 1;
    }  /* for */
    pop_call_frame(ips);
    /* Release mappings of the parameters. */
    param = callee_scope->variant.routine.parameters;
    for (; param != NULL; param = param->next) {
      unmap_stack_bytes(ips, param);
      unmap_ptr(&ips->map, &param->storage_class);
    }  /* for */
    /* Reduce the cost of the call to just 1. */
    ips->cost -= up_front_cost-1;
  }  /* if */
done:
  return result;
}  /* do_constexpr_call */


static a_byte_count record_anon_union_active_field(a_field_ptr  *p_fp,
                                                   a_byte       *storage)
/*
*p_fp is a field in an anonymous union.  storage points to the representation
of the object enclosing the one or more anonymous union parent objects of
*p_fp.  Set the active field in every anonymous union parent object, update
*p_fp to point to the top-most anonymous parent object, and return the offset
of the original *p_fp field in the representation of the returned *p_fp field.
*/
{
  a_field_ptr   fp = *p_fp, aufp;
  a_byte_count  offset;

  aufp = symbol_for(fp)->variant.field.anonymous_parent_object
                       ->variant.field.ptr;
  get_mapped_byte_count(&persistent_map, aufp, offset);
  if (symbol_for(aufp)->variant.field.anonymous_parent_object != NULL) {
    /* aufp is not the top-most anonymous union.  Recurse to determine its
       offset, and then replace it by the top-most anonymous union. */
    offset += record_anon_union_active_field(&aufp, storage);
  }  /* if */
  *(a_field_ptr*)(storage+offset) = fp;
  *p_fp = aufp;
  return offset;
}  /* record_anon_union_active_field */


static a_boolean do_constexpr_ctor(an_interpreter_state  *ips,
                                   a_dynamic_init_ptr    dip,
                                   a_source_position     *pos,
                                   a_byte                *result_storage)
/*
Interpret the constructor call represented by the given dynamic initialization
entry.  Return TRUE if no error occurred; otherwise, return FALSE and update
*ips accordingly.  pos is the position of the call.

This is similar to do_constexpr_call, but the call has a different
representation, and mem-initializers must be interpreter prior to interpreting
the body of the (constructor) function proper.
*/
{
  a_routine_ptr     callee = dip->variant.constructor.ptr;
  a_boolean         result = TRUE;

  /* Retrieve the routine scope, or issue an error. */
  if (callee->function_def_number == NULL_function_def_number) {
    info_with_pos_sym(ec_constexpr_function_undefined, pos,
                      symbol_for(callee), ips);
    result = FALSE;
  } else if (cost_exceeded(ips)) {
    more_info_diagnostic(ec_excessive_constexpr_complexity, &ips->position,
                         &ips->diag_list);
    result = FALSE;
  } else {
    a_scope_ptr       callee_scope = scope_for_routine(callee);
    a_statement_ptr   block_stmt = callee_scope->assoc_block;
    a_call_frame      frame;
    an_expr_node_ptr  args = dip->variant.constructor.args, arg;
    a_variable_ptr    params = callee_scope->variant.routine.parameters,
                      param, this_var;
    a_constructor_init_ptr
                      ctor_init;
    a_byte_count      n_args = 0;
    a_byte            *arg_ptrs, **p_arg_ptr;
    an_alloc_seq_number
                      alloc_seq_number;
    a_type_ptr        class_type = parent_class_of(callee);
    a_class_symbol_supplement_ptr
                      cssp;
    unsigned long     up_front_cost;
    /* Don't attempt to interpret a non-constexpr function.  The flag
       scope->is_constexpr_routine is set at the end of a constexpr function
       definition, so this also prevents the interpretation of a function that
       is not fully parsed (e.g., requested due to a recursive call in a
       constexpr function). */
    if (!callee_scope->is_constexpr_routine) {
      result = FALSE;
      info_with_pos_sym(ec_constexpr_call_not_interpretable, pos,
                        symbol_for(callee), ips);
      goto done;
    }  /* if */
    /* If the constructor has an associated nontrivial destructor, don't
       attempt interpretation either since the lifetime won't be right. */
    cssp = class_symbol_supp(symbol_for(class_type));
    if (has_nontrivial_destructor(cssp)) {
      result = FALSE;
      info_with_pos(ec_constexpr_ctor_with_dtor, pos, ips);
      goto done;
    }  /* if */
    /* Account a relatively high cost for the call up-front, to limit the
       overall call depth.  When the call returns, that cost will be reduced
       to just "one". */
    up_front_cost = max_constexpr_call_cost/max_constexpr_call_depth+1;
    ips->cost += up_front_cost;
    /* Set up arguments, starting with "this" if applicable. */
    /* This process must happen in two phases.  First, the arguments must be
       allocated and evaluated.  Only then can we map parameter variables onto
       the allocated arguments.  We cannot do the two in a single loop because
       of recursive calls.  (See do_constexpr_call for details.) */
    for (arg = args; arg != NULL; arg = arg->next) {
      n_args += 1;
    }  /* for */
    alloc_stack_bytes(ips, n_args*sizeof(a_byte*), arg_ptrs);
    /* Phase 1: Allocate and evaluate the arguments. */
    p_arg_ptr = (a_byte**)arg_ptrs;
    for (arg = args; arg != NULL; arg = arg->next) {
      a_type_ptr    tp = skip_typerefs(arg->type);
      a_byte_count  n_bytes = value_bytes_for_type(ips, tp, &result);
      a_byte        *arg_bytes;
      a_boolean     restore_lvalue = FALSE, restore_xvalue = FALSE;
      alloc_complete_object(ips, n_bytes, tp, arg_bytes);
      *p_arg_ptr = arg_bytes;
      p_arg_ptr += 1;
      if (result) {
        if (!(tp->kind == (a_type_kind)tk_pointer &&
              tp->variant.pointer.is_reference)) {
          /* When a class-type argument is pass by-value via a copy constructor
             call, the argument is left as an lvalue.  Temporarily set it back
             to an rvalue. */
          if (arg->is_lvalue) {
            restore_lvalue = TRUE;
            arg->is_lvalue = FALSE;
          } else if (arg->is_xvalue) {
            restore_xvalue = TRUE;
            arg->is_xvalue = FALSE;
          }  /* if */
        }  /* if */
        if (!do_constexpr_expression(ips, arg, arg_bytes)) {
          result = FALSE;
        }  /* if */
        if (restore_lvalue) {
          arg->is_lvalue = TRUE;
        } else if (restore_xvalue) {
          arg->is_xvalue = TRUE;
        }  /* if */
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
    this_var = callee_scope->variant.routine.this_param_variable;
    if (this_var != NULL) {
      map_stack_bytes(ips, this_var, result_storage);
      map_byte_count(&ips->map, &this_var->storage_class, alloc_seq_number);
    } else {
      /* A constructor should always have a "this" parameter. */
      unexpected_condition();
    }  /* if */
    p_arg_ptr = (a_byte**)arg_ptrs;
    for (param = params; param != NULL; param = param->next) {
      map_stack_bytes(ips, param, *p_arg_ptr);
      map_byte_count(&ips->map, &param->storage_class, alloc_seq_number);
      p_arg_ptr += 1;
    }  /* for */
    if (dip->variant.constructor.value_initialization) {
      /* If this is for value initialization, clear the storage first.
         Do not, however, override the first word (which may record the
         inheritance hierarchy). */
      a_byte_count  n_class_bytes;
      a_boolean     dummy = TRUE;
      n_class_bytes = f_value_bytes_for_type(ips, class_type, &dummy);
      memzero(result_storage+sizeof(void*),
              size_t_arg(n_class_bytes-sizeof(void*)));
    }  /* if */
    /* Set up the call frame. */
    push_call_frame(ips, &frame, callee, pos, result_storage);
    /* Run the constructor initializers. */
    ctor_init = callee_scope->variant.routine.constructor_inits;
    for (; ctor_init != NULL; ctor_init = ctor_init->next) {
      a_byte_count        offset;
      a_dynamic_init_ptr  sub_dip;
      a_type_ptr          tp;
      if (ctor_init->kind == (a_constructor_init_kind)cik_field) {
        a_field_ptr  fp = ctor_init->variant.field;
        tp = skip_typerefs(fp->type);
        get_mapped_byte_count(&persistent_map, fp, offset);
        if (ctor_init->use_field_initializer) {
          sub_dip = fp->initializer;
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
          offset += record_anon_union_active_field(&fp, result_storage);
        }  /* if */
        if (class_type->kind == (a_type_kind)tk_union) {
          /* Record the active field for the enclosing union. */
          *(a_field_ptr*)result_storage = fp;
        }  /* if */
      } else if (ctor_init->kind == (a_constructor_init_kind)cik_delegation) {
        result = do_constexpr_dynamic_init(ips, ctor_init->initializer, pos,
                                           result_storage);
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
        /* An implicit member copy in a copy constructor.  arg_ptrs[0] points
           to the first argument of the copy constructor, which is a reference
           to the copied object. */
        a_constexpr_address  *src_addr;
        src_addr = (a_constexpr_address*)((a_byte**)arg_ptrs)[0];
        if (is_runtime_data_address(src_addr)) {
          /* Cannot modify the value of an object whose lifetime began
             outside the current evaluation. */
          info_with_pos(ec_constexpr_access_to_runtime_storage,
                        &args->position, ips);
          result = FALSE;
          break;
        } else {
           if (constexpr_copy_object(ips, tp, src_addr->address+offset,
                                     result_storage+offset)) {
             result = FALSE;
             break;
           }  /* if */
        }  /* if */
      } else if (!do_constexpr_dynamic_init(
                                        ips, sub_dip,
                                        &callee->source_corresp.decl_position,
                                        result_storage+offset)) {
        result = FALSE;
        break;
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
    p_arg_ptr = (a_byte**)arg_ptrs;
    for (arg = args; arg != NULL; arg = arg->next) {
      a_type_ptr  tp = skip_typerefs(arg->type);
      release_address_structures(arg, tp, *p_arg_ptr);
      p_arg_ptr += 1;
    }  /* for */
    pop_call_frame(ips);
    /* Release the storage and mappings of the parameters. */
    param = callee_scope->variant.routine.parameters;
    for (; param != NULL; param = param->next) {
      unmap_stack_bytes(ips, param);
      unmap_ptr(&ips->map, &param->storage_class);
    }  /* for */
    /* Reduce the cost of the call to just 1. */
    ips->cost -= up_front_cost-1;
  }  /* if */
done:
  return result;
}  /* do_constexpr_ctor */


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

  if (constant_value_at_address(addr_con,
                                /*a_constexpr_evaluation_block=*/NULL,
                                val_con)) {
    /* Copy the constant value. */
    result = copy_val_from_constant(ips, val_con, value);
  } else {
    result = FALSE;
  }  /* if */
  release_local_constant(&val_con);
  return result;
}  /* get_value_from_address_constant */


static a_boolean do_constexpr_expression(an_interpreter_state  *ips,
                                         an_expr_node_ptr      orig_expr,
                                         a_byte                *result_storage)
/*
Interpret the given expression in the given interpreter context.  If
successful return TRUE and store the result at *result_storage.  Otherwise,
return FALSE and update *ips accordingly.  A glvalue result is represented
as an a_constexpr_address value, so result_storage must be at least large
enough for that type; otherwise, it need only be large enough for the type
of the prvalue result.
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
        DECL_COMPACT_VALUE_BYTES(opnd1_bytes);
        DECL_COMPACT_VALUE_BYTES(opnd2_bytes);

        if (is_call_node(expr)) {
          /* Call nodes are handled separately because their operands are set
             up a little differently. */
          result = do_constexpr_call(ips, expr, result_storage);
          goto done;
        } else if (node_operator_is(expr, eok_parens) ||
                   node_operator_is(expr, eok_class_rvalue_adjust)) {
          /* These are pass-through operators for prvalues.  So we cannot just
             copy the operand, since it could invalidate internal addresses.
             Instead, the operand must be evaluated directly into the final
             result storage. */
          result = do_constexpr_expression(
                       ips, expr->variant.operation.operands, result_storage);
        }  /* if */
/*
Macro to set result_storage from either the address in opnd or the value
to which that address points, depending on whether the result is a glvalue
or a prvalue.  This is used for operations that produce glvalues but may
incorporate an implicit lvalue-to-rvalue conversion, i.e., "rvalueable"
nodes.
*/
#define set_result_val_from_operand_address(opnd)                             \
  {                                                                           \
    if (expr->is_lvalue || expr->is_xvalue || is_function_address(opnd)) {    \
      /* Copy the address. */                                                 \
      *(a_constexpr_address*)result_storage = *(a_constexpr_address*)(opnd);  \
    } else {                                                                  \
      /* Do the lvalue-to-rvalue conversion into the result. */               \
      if (cannot_dereference(opnd)) {                                         \
        /* This address cannot be dereferenced. */                            \
        result = FALSE;                                                       \
        info_one_past_end_of_array((a_constexpr_address*)opnd, expr, ips);    \
      } else if (is_runtime_data_address(opnd)) {                             \
        if (!get_value_from_address_constant(                                 \
                   ips,                                                       \
                   ((a_constexpr_address*)(opnd))->variant.addr_con,          \
                   result_storage)) {                                         \
          /* Not a compile-time constant value. */                            \
          result = FALSE;                                                     \
          info_with_pos(ec_constexpr_access_to_runtime_storage,               \
                        &expr->position, ips);                                \
        }  /* if */                                                           \
      } else if (!in_live_set(&ips->live_set,                                 \
                              ((a_constexpr_address*)(opnd))                  \
                                                      ->alloc_seq_number)) {  \
        /* An attempt to access storage that has expired. */                  \
        result = FALSE;                                                       \
        info_with_pos(ec_constexpr_access_to_expired_storage, &expr->position,\
                      ips);                                                   \
      } else if (is_variant_path(opnd) &&                                     \
                 !check_variant_path(ips, (a_constexpr_address*)opnd,         \
                                     /*release=*/TRUE, &expr->position)) {    \
        /* An attempt to dereference an inactive variant path. */             \
        result = FALSE;                                                       \
      } else if (((a_constexpr_address*)(opnd))->address == NULL) {           \
        result = FALSE;                                                       \
        info_with_pos(ec_constexpr_null_dereference, &expr->position, ips);   \
      } else {                                                                \
        (void)memcpy(result_storage, value_bytes_at(opnd),                    \
                     size_t_arg(n_bytes));                                    \
      }  /* if */                                                             \
    }  /* if */                                                               \
  }  /* set_result_val_from_operand_address */

/*
Macro that sets result to TRUE or FALSE depending on whether the integer
result of an operation (in val) is within the range representable by its
type.  This includes checking the value of ovfl set by the operation.
*/
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
#define check_int_range(val, tp, result, ovfl, pos, ips)                      \
{                                                                             \
  if (!ovfl) {                                                                \
    get_int_val_from((val), (tp), host_int_val, ovfl);                        \
    (result) = (!ovfl &&                                                      \
                host_int_val <=                                               \
                 (a_host_large_integer)max_integer_value_of_kind[int_kind] && \
                (!is_signed ||                                                \
                 host_int_val >=                                              \
                 (a_host_large_integer)min_integer_value_of_kind[int_kind])); \
  } else {                                                                    \
    (result) = FALSE;                                                         \
  }  /* if */                                                                 \
  if (!(result)) {                                                            \
    info_with_pos_type(ec_constexpr_integer_overflow, pos, tp, ips);          \
  }  /* if */                                                                 \
}  /* check_int_range */
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
#define check_int_range(val, tp, result, ovfl, pos, ips)                      \
{                                                                             \
  ((result) = (!ovfl &&                                                       \
         cmp_integer_values((an_integer_value *)(val), is_signed,             \
                            &max_integer_value_of_kind[int_kind],             \
                            is_signed) <= 0 &&                                \
         (!is_signed ||                                                       \
          cmp_integer_values((an_integer_value *)(val), is_signed,            \
                             &min_integer_value_of_kind[int_kind],            \
                             is_signed) >= 0)));                              \
  if (!(result)) {                                                            \
    info_with_pos_type(ec_constexpr_integer_overflow, pos, tp, ips);          \
  }  /* if */                                                                 \
}  /* check_int_range */
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

        opnd1 = expr->variant.operation.operands;
        opnd2 = opnd1->next;
        opnd1_type = skip_typerefs(opnd1->type);
        opnd_n_bytes = value_bytes_for_type(ips, opnd1_type, &result);
        if (!is_compact_value_size(opnd_n_bytes) &&
            !opnd1->is_lvalue && !opnd1->is_xvalue) {
          /* The value is larger than a scalar type, so allocate
             space for it on the stack. */
          alloc_stack_bytes(ips, opnd_n_bytes, opnd1_value);
        } else {
          opnd1_value = compact_value_bytes(opnd1_bytes);
        }  /* if */
        record_complete_object(opnd1_type, opnd1_value);
        if (result && !do_constexpr_expression(ips, opnd1, opnd1_value)) {
          result = FALSE;
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
          opnd_n_bytes = value_bytes_for_type(ips, opnd2_type, &result);
          if (!is_compact_value_size(opnd_n_bytes) &&
              !opnd2->is_lvalue && !opnd2->is_xvalue) {
            /* The value may be larger than a scalar type, so allocate
               space for it on the stack. */
            alloc_stack_bytes(ips, opnd_n_bytes, opnd2_value);
          } else {
            opnd2_value = compact_value_bytes(opnd2_bytes);
          }  /* if */
          record_complete_object(opnd2_type, opnd2_value);
          if (result && !do_constexpr_expression(ips, opnd2, opnd2_value)) {
            result = FALSE;
          }  /* if */
        } else {
          opnd2_value = compact_value_bytes(opnd2_bytes);
          opnd2_type = NULL;
        }  /* if */
        if (result) {
          /* The operand(s) were evaluated successfully.  Process the
             operation. */
          switch (expr->variant.operation.kind) {
            case eok_address_of:
            case eok_reference_to:
              /* The result is an a_constexpr_address designating the
                 object, and the operand is already an a_constexpr_address
                 (glvalue or temporary), so just copy the operand. */
              *(a_constexpr_address *)result_storage =
                                           *(a_constexpr_address *)opnd1_value;
              break;
            case eok_indirect:
            case eok_ref_indirect:
              /* The result is either a copy of the operand (which is an
                 a_constexpr_address) if the result is a glvalue or the
                 value to which the address points for a prvalue. */
              set_result_val_from_operand_address(opnd1_value);
              break;
            case eok_cast:
              if (tp->kind == opnd1_type->kind) {
                /* The type kinds are the same, so the representation is
                   the same, and we can just copy the opnd1 value. */
                if (tp->kind == (a_type_kind)tk_integer) {
                  /* Integers: Somewhat surprisingly, narrowing conversions are
                     valid here ("implementation-defined").  So we don't check
                     that the result is in range. */
                  *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
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
                    result = FALSE;
                  }  /* if */
                } else if (tp->kind == (a_type_kind)tk_pointer) {
                  a_type_ptr  utp1 = skip_typerefs(tp->variant.pointer.type);
                  a_type_ptr  utp2;
                  utp2 = skip_typerefs(opnd1_type->variant.pointer.type);
                  if (identical_types(utp1, utp2)) {
                    /* E.g., a conversion from X* to X const*. */
                    *(a_constexpr_address *)result_storage =
                                           *(a_constexpr_address *)opnd1_value;
                  } else {
                    info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                        &expr->position, opnd1_type, tp, ips);
                    result = FALSE;
                  }  /* if */
                } else if (tp->kind == (a_type_kind)tk_void) {
                  release_address_structures(opnd1, opnd1_type, opnd1_value);
                } else {
                  info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                      &expr->position, opnd1_type, tp, ips);
                  result = FALSE;
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
                  result = FALSE;
                  info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                      &expr->position, opnd1_type, tp, ips);
                }  /* if */
              } else if (opnd1_type->kind == (a_type_kind)tk_float &&
                         tp->kind == (a_type_kind)tk_integer) {
                unexpected_condition(); /* FIXME NYI */
              } else {
                result = FALSE;
                info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                    &expr->position, opnd1_type, tp, ips);
              }  /* if */
              break;
            case eok_lvalue_cast:
            case eok_ref_cast:
            case eok_lvalue_adjust:
              if (tp != opnd1_type) {
                info_with_pos_type2(ec_constexpr_invalid_type_conversion,
                                    &expr->position, opnd1_type, tp, ips);
                result = FALSE;
              } else {
                set_result_val_from_operand_address(opnd1_value);
              }  /* if */
              break;
            case eok_base_class_cast:
              if (tp->kind == (a_type_kind)tk_pointer ||
                  opnd1->is_lvalue || opnd1->is_xvalue) {
                /* An address adjustment. */
                a_constexpr_address  *result_addr =
                                         (a_constexpr_address*)result_storage;
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
                get_mapped_byte_count(&persistent_map, bcp, offset);
                *result_addr = *(a_constexpr_address *)opnd1_value;
                result_addr->address += offset;
                result_addr->flags &= ~CA_ARRAY_ELEMENT;
              } else {
                /* FIXME: NYI, slicing. */
              }  /* if */
              break;
            case eok_derived_class_cast:
              { a_constexpr_address  *src = (a_constexpr_address*)opnd1_value;
                a_base_class_ptr     bcp = *(a_base_class_ptr*)src->address;
                if (bcp != NULL && bcp->type == tp) {
                  a_byte_count  offset;
                  a_constexpr_address  *dst =
                                (a_constexpr_address*)result_storage;
                  get_mapped_byte_count(&persistent_map, bcp, offset);
                  *dst = *src;
                  dst->address -= offset;
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
                  result = FALSE;
                  info_with_pos_type(ec_constexpr_bad_derived_class_cast,
                                     &expr->position, derived_class, ips);
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
                dtp = opnd1_type->variant.ptr_to_member
                                         .class_of_which_a_member;
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
              if (opnd1_type->kind == (a_type_kind)tk_integer) {
                int_kind = opnd1_type->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                if (cmp_integer_values((an_integer_value *)opnd1_value,
                                       is_signed,
                                       (an_integer_value *)&zero_int,
                                       is_signed) != 0) {
                  *(an_integer_value *)result_storage = one_int;
                } else {
                  *(an_integer_value *)result_storage = zero_int;
                }  /* if */
              } else {
                unexpected_condition();  /* FIXME: NYI, other source types. */
              }  /* if */
              break;
            case eok_array_to_pointer:
              /* The actual address is unchanged, but record the array
                 characteristics. */
              { a_constexpr_address  *result_addr =
                                         (a_constexpr_address*)result_storage;
                a_targ_size_t        length;
                *result_addr = *(a_constexpr_address *)opnd1_value;
                result_addr->flags |= CA_ARRAY_ELEMENT;
                /* Check the array length fits in interpreter limits. */
                length = opnd1_type->variant.array.variant.number_of_elements;
                if (length <= MAX_ARRAY_LENGTH) {
                  result_addr->length = length;
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
              /* This operator has no effect. */
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
                negate_integer_value((an_integer_value *)result_storage,
                                     &ovfl);
                check_int_range((an_integer_value *)result_storage, tp, result,
                                ovfl, &expr->position, ips);
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
                check_int_range((an_integer_value *)result_storage, tp, result,
                                ovfl, &expr->position, ips);
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
                check_int_range((an_integer_value *)result_storage, tp, result,
                                ovfl, &expr->position, ips);
              } else {
                /* The complement operator only applies to integer types. */
                unexpected_condition();
              }  /* if */
              break;
            case eok_not:
              if (opnd1_type->kind == (a_type_kind)tk_integer) {
                a_host_large_integer  bool_val;
                int_kind = opnd1_type->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                get_int_val_from(opnd1_value, opnd1_type, bool_val, ovfl);
                if (ovfl || bool_val) {
                  *(an_integer_value *)result_storage = zero_int;
                } else {
                  *(an_integer_value *)result_storage = one_int;
                }  /* if */
              } else {
                unexpected_condition();  /* FIXME: NYI, other source types. */
              }  /* if */
              break;
            case eok_post_incr:
              if (is_runtime_data_address(opnd1_value)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
                result = FALSE;
              } else {
                /* Return a copy of the value stored at the operand address. */
                set_result_val_from_operand_address(opnd1_value);
                /* Now increment the original value. */
                if (!result) {
                  /* Something was wrong with the operand address. */
                } else if (tp->kind == (a_type_kind)tk_integer) {
                  /* An integral type. */
                  an_integer_value  *ival = int_value_at(opnd1_value);
                  if (tp->variant.integer.bool_type) {
                    /* Incrementing a bool variable sets it to TRUE. */
                    *ival = one_int;
                  } else {
                    /* An integer. */
                    int_kind = tp->variant.integer.int_kind;
                    is_signed = int_kind_is_signed[int_kind];
                    add_integer_values(ival, &one_int, is_signed, &ovfl);
                    check_int_range(ival, tp, result, ovfl, &expr->position,
                                    ips);
                  }  /* if */
                } else if (tp->kind == (a_type_kind)tk_float) {
                  /* A floating-point type. */
                  fp_add(tp->variant.float_kind,
                         fp_value_at(opnd1_value),
                         &one_flt[(int)tp->variant.float_kind],
                         fp_value_at(opnd1_value), &err, &depends_on_fp_mode);
                  if (err) {
                    result = FALSE;
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  }  /* if */
                } else if (tp->kind == (a_type_kind)tk_pointer) {
                  /* A pointer. */
                  a_constexpr_address  *ptr;
                  ptr = (a_constexpr_address*)value_bytes_at(opnd1_value);
                  if (!is_array_element(ptr) || cannot_dereference(ptr)) {
                    /* Not a pointer to an array element in interpreter
                       storage. */
                    info_with_pos(ec_constexpr_invalid_pointer,
                                  &expr->position, ips);
                    result = FALSE;
                  } else {
                    a_type_ptr    elem_type;
                    a_byte_count  elem_size;
                    a_byte        *base_address;
                    elem_type =
                              skip_typerefs(opnd1->type->variant.pointer.type);
                    elem_size = value_bytes_for_type(ips, elem_type, &result);
                    ptr->address += elem_size;
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
            case eok_post_decr:
              if (is_runtime_data_address(opnd1_value)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                info_with_pos(ec_constexpr_access_to_runtime_storage,
                              &expr->position, ips);
                result = FALSE;
              } else {
                /* Return a copy of the value stored at the operand address. */
                set_result_val_from_operand_address(opnd1_value);
                /* Now decrement the original value. */
                if (!result) {
                  /* Something was wrong with the operand address. */
                } else if (tp->kind == (a_type_kind)tk_integer) {
                  /* An integer. */
                  an_integer_value  *ival = int_value_at(opnd1_value);
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  subtract_mixed_signed_integer_values(
                                 ival, is_signed, &one_int, is_signed, &ovfl);
                  check_int_range(ival, tp, result, ovfl, &expr->position,
                                  ips);
                } else if (tp->kind == (a_type_kind)tk_float) {
                  /* A floating-point type. */
                  fp_subtract(tp->variant.float_kind,
                              fp_value_at(opnd1_value),
                              &one_flt[(int)tp->variant.float_kind],
                              fp_value_at(opnd1_value), &err,
                              &depends_on_fp_mode);
                  if (err) {
                    result = FALSE;
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                  }  /* if */
                } else if (tp->kind == (a_type_kind)tk_pointer) {
                  /* A pointer. */
                  a_constexpr_address  *ptr;
                  ptr = (a_constexpr_address*)value_bytes_at(opnd1_value);
                  if (!is_array_element(ptr)) {
                    /* Not a pointer to an array element in interpreter
                       storage. */
                    info_with_pos(ec_constexpr_fp_error, &expr->position, ips);
                    result = FALSE;
                  } else {
                    a_type_ptr    elem_type;
                    a_byte_count  elem_size;
                    a_byte        *base_address;
                    elem_type =
                              skip_typerefs(opnd1->type->variant.pointer.type);
                    elem_size = value_bytes_for_type(ips, elem_type, &result);
                    base_address = get_base_address(ptr);
                    if (ptr->address == base_address) {
                      /* The pointer cannot point ahead of the array. */
                      info_with_pos(ec_constexpr_invalid_pointer,
                                    &expr->position, ips);
                      result = FALSE;
                    } else {
                      if (ptr->address == base_address+ptr->length*elem_size) {
                        /* We were "one past the end of the array", but that
                           will no longer be true. */
                        ptr->flags &= ~CA_CANNOT_DEREFERENCE;
                      }  /* if */
                      ptr->address -= elem_size;
                    }  /* if */
                  }  /* if */
                } else {
                  /* Invalid type for prefix --. */
                  unexpected_condition();
                }  /* if */
              }  /* if */
              break;
            case eok_pre_incr:
              if (is_runtime_data_address(opnd1_value)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                /* FIXME: record a diagnostic. */
                result = FALSE;
              } else if (tp->kind == (a_type_kind)tk_integer) {
                /* An integral type. */
                if (tp->variant.integer.bool_type) {
                  /* Incrementing a bool variable sets it to TRUE. */
                  *(an_integer_value *)value_bytes_at(opnd1_value) = one_int;
                } else {
                  /* An integer. */
                  an_integer_value  *ival = int_value_at(opnd1_value);
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  add_integer_values(ival, &one_int, is_signed, &ovfl);
                  check_int_range(ival, tp, result, ovfl, &expr->position,
                                  ips);
                }  /* if */
              } else if (tp->kind == (a_type_kind)tk_float) {
                /* A floating-point type. */
                fp_add(tp->variant.float_kind,
                       fp_value_at(opnd1_value),
                       &one_flt[(int)tp->variant.float_kind],
                       fp_value_at(opnd1_value), &err, &depends_on_fp_mode);
                if (err) {
                  result = FALSE;
                  /* FIXME: record a diagnostic. */
                }  /* if */
              } else if (tp->kind == (a_type_kind)tk_pointer) {
                /* A pointer. */
                a_constexpr_address  *ptr;
                ptr = (a_constexpr_address*)value_bytes_at(opnd1_value);
                if (!is_array_element(ptr) || cannot_dereference(ptr)) {
                  /* Not a pointer to an array element in interpreter
                     storage. */
                  /* FIXME: record a diagnostic. */
                  result = FALSE;
                } else {
                  a_type_ptr    elem_type;
                  a_byte_count  elem_size;
                  a_byte        *base_address;
                  elem_type = skip_typerefs(opnd1->type->variant.pointer.type);
                  elem_size = value_bytes_for_type(ips, elem_type, &result);
                  ptr->address += elem_size;
                  base_address = get_base_address(ptr);
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
                set_result_val_from_operand_address(opnd1_value);
              }  /* if */
              break;
            case eok_pre_decr:
              if (is_runtime_data_address(opnd1_value)) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                /* FIXME: record a diagnostic. */
                result = FALSE;
              } else if (tp->kind == (a_type_kind)tk_integer) {
                /* An integer. */
                  an_integer_value  *ival = int_value_at(opnd1_value);
                int_kind = tp->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                subtract_mixed_signed_integer_values(
                                 ival, is_signed, &one_int, is_signed, &ovfl);
                check_int_range(ival, tp, result, ovfl, &expr->position, ips);
              } else if (tp->kind == (a_type_kind)tk_float) {
                /* A floating-point type. */
                fp_subtract(tp->variant.float_kind,
                            fp_value_at(opnd1_value),
                            &one_flt[(int)tp->variant.float_kind],
                            fp_value_at(opnd1_value), &err,
                            &depends_on_fp_mode);
                if (err) {
                  result = FALSE;
                  /* FIXME: record a diagnostic. */
                }  /* if */
              } else if (tp->kind == (a_type_kind)tk_pointer) {
                /* A pointer. */
                a_constexpr_address  *ptr;
                ptr = (a_constexpr_address*)value_bytes_at(opnd1_value);
                if (!is_array_element(ptr)) {
                  /* Not a pointer to an array element in interpreter
                     storage. */
                  /* FIXME: record a diagnostic. */
                  result = FALSE;
                } else {
                  a_type_ptr    elem_type;
                  a_byte_count  elem_size;
                  a_byte        *base_address;
                  elem_type = skip_typerefs(opnd1->type->variant.pointer.type);
                  elem_size = value_bytes_for_type(ips, elem_type, &result);
                  base_address = get_base_address(ptr);
                  if (ptr->address == base_address) {
                    /* The pointer can point ahead of the array. */
                    /* FIXME: record a diagnostic. */
                    result = FALSE;
                  } else {
                    if (ptr->address == base_address + ptr->length*elem_size) {
                      /* We were "one past the end of the array", but that will
                         no longer be true. */
                      ptr->flags &= ~CA_CANNOT_DEREFERENCE;
                    }  /* if */
                    ptr->address -= elem_size;
                  }  /* if */
                }  /* if */
              } else {
                /* Invalid type for prefix --. */
                unexpected_condition();
              }  /* if */
              if (result) {
                /* Return either the address or the value, as
                   appropriate. */
                set_result_val_from_operand_address(opnd1_value);
              }  /* if */
              break;
            case eok_add:
              if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
                int_kind = tp->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                add_integer_values((an_integer_value*)result_storage,
                                   (an_integer_value*)opnd2_value,
                                   is_signed, &ovfl);
                check_int_range((an_integer_value*)(result_storage), tp,
                                result, ovfl, &expr->position, ips);
              } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
                fp_add(tp->variant.float_kind,
                       fp_value(opnd1_value), fp_value(opnd2_value),
                       fp_value(result_storage), &err, &depends_on_fp_mode);
                if (err) {
                  result = FALSE;
                  /* FIXME: record a diagnostic. */
                }  /* if */
              } else {
                /* FIXME: Other type kinds NYI. */
                unexpected_condition();
              }  /* if */
              break;
            case eok_subtract:
              if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
                int_kind = tp->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                subtract_integer_values((an_integer_value*)result_storage,
                                        (an_integer_value*)opnd2_value,
                                        is_signed, &ovfl);
                check_int_range((an_integer_value*)(result_storage), tp,
                                result, ovfl, &expr->position, ips);
              } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
                fp_subtract(tp->variant.float_kind,
                            fp_value(opnd1_value), fp_value(opnd2_value),
                            fp_value(result_storage), &err,
                            &depends_on_fp_mode);
                if (err) {
                  result = FALSE;
                  /* FIXME: record a diagnostic. */
                }  /* if */
              } else {
                /* FIXME: Other type kinds NYI. */
              }  /* if */
              break;
            case eok_multiply:
              if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
                int_kind = tp->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                multiply_integer_values((an_integer_value*)result_storage,
                                        (an_integer_value*)opnd2_value,
                                        is_signed, &ovfl);
                check_int_range((an_integer_value*)(result_storage), tp,
                                result, ovfl, &expr->position, ips);
              } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
                fp_multiply(tp->variant.float_kind,
                            fp_value(opnd1_value), fp_value(opnd2_value),
                            fp_value(result_storage), &err,
                            &depends_on_fp_mode);
                if (err) {
                  result = FALSE;
                  /* FIXME: record a diagnostic. */
                }  /* if */
              } else {
                /* FIXME: Other type kinds NYI. */
              }  /* if */
              break;
            case eok_divide:
              if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
                int_kind = tp->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                divide_integer_values((an_integer_value*)result_storage,
                                      (an_integer_value*)opnd2_value,
                                      is_signed, &ovfl);
                if (ovfl) {
                  /* FIXME: record a diagnostic. */
                }  /* if */
              } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
                fp_divide(tp->variant.float_kind,
                          fp_value(opnd1_value), fp_value(opnd2_value),
                          fp_value(result_storage), &err,
                          &depends_on_fp_mode);
                if (err) {
                  result = FALSE;
                  /* FIXME: record a diagnostic. */
                }  /* if */
              } else {
                /* FIXME: Other type kinds NYI. */
              }  /* if */
              break;
            case eok_remainder:
              if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
                int_kind = tp->variant.integer.int_kind;
                is_signed = int_kind_is_signed[int_kind];
                remainder_integer_values((an_integer_value*)result_storage,
                                         (an_integer_value*)opnd2_value,
                                         is_signed, &ovfl);
                if (ovfl) {
                  /* FIXME: record a diagnostic. */
                }  /* if */
              } else {
                /* FIXME: Other type kinds NYI. */
              }  /* if */
              break;
            case eok_padd:
              /* Pointer + integer or integer + pointer. */
              { a_constexpr_address  *result_addr;
                a_type_ptr           elem_type;
                result_addr = (a_constexpr_address*)result_storage;
                if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                  get_int_val_from(opnd2_value, opnd2_type, host_int_val,
                                   ovfl);
                  *result_addr = *(a_constexpr_address *)opnd1_value;
                  elem_type = skip_typerefs(opnd1->type->variant.pointer.type);
                } else {
                  get_int_val_from(opnd1_value, opnd1_type, host_int_val,
                                   ovfl);
                  *result_addr = *(a_constexpr_address *)opnd2_value;
                  elem_type = skip_typerefs(opnd1->type->variant.pointer.type);
                }  /* if */
                if (ovfl) {
                  result = FALSE;  /* FIXME: diagnostic */
                } else {
                  if (host_int_val == 0) {
                    /* Leave the address unchanged. */
                  } else if (!is_array_element(result_addr)) {
                    result = FALSE;  /* FIXME: diagnostic */
                  } else {
                    a_byte_count  elem_size, pos, len;
                    a_byte        *base_address;
                    elem_size = value_bytes_for_type(ips, elem_type, &result);
                    if (!result) break;
                    len = result_addr->length;
                    base_address = get_base_address(result_addr);
                    pos = (a_byte_count)(result_addr->address - base_address)
                                        / elem_size;
                    if (host_int_val > 0 ?
                                        (len-pos < (a_byte_count)host_int_val)
                                      : (pos < (a_byte_count)-host_int_val)) {
                      /* Out of bounds. */
                      result = FALSE;  /* FIXME: diagnostic */
                    } else {
                      result_addr->address +=
                        host_int_val
                              * value_bytes_for_type(ips, elem_type, &result);
                      if (pos+host_int_val == len) {
                        result_addr->flags |= CA_CANNOT_DEREFERENCE;
                      } else {
                        result_addr->flags &= ~CA_CANNOT_DEREFERENCE;
                      }  /* if */
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
                  get_int_val_from(opnd2_value, opnd2_type, host_int_val,
                                   ovfl);
                  *result_addr = *(a_constexpr_address *)opnd1_value;
                  elem_type = skip_typerefs(opnd1->type->variant.pointer.type);
                } else {
                  get_int_val_from(opnd1_value, opnd1_type, host_int_val,
                                   ovfl);
                  *result_addr = *(a_constexpr_address *)opnd2_value;
                  elem_type = skip_typerefs(opnd1->type->variant.pointer.type);
                }  /* if */
                if (ovfl) {
                  result = FALSE;  /* FIXME: diagnostic */
                } else {
                  if (host_int_val == 0) {
                    /* Leave the address unchanged. */
                  } else if (!is_array_element(result_addr)) {
                    result = FALSE;  /* FIXME: diagnostic */
                  } else {
                    a_byte_count  elem_size, pos, len;
                    a_byte        *base_address;
                    elem_size = value_bytes_for_type(ips, elem_type, &result);
                    if (!result) break;
                    len = result_addr->length;
                    base_address = get_base_address(result_addr);
                    pos = (a_byte_count)(result_addr->address - base_address)
                                        / elem_size;
                    if (host_int_val > 0 ?
                                  (pos < (a_byte_count)host_int_val)
                                : (len-pos < (a_byte_count)-host_int_val)) {
                      /* Out of bounds. */
                      result = FALSE;  /* FIXME: diagnostic */
                    } else {
                      result_addr->address -=
                        host_int_val
                              * value_bytes_for_type(ips, elem_type, &result);
                      if (pos - host_int_val == len) {
                        result_addr->flags |= CA_CANNOT_DEREFERENCE;
                      } else {
                        result_addr->flags &= ~CA_CANNOT_DEREFERENCE;
                      }  /* if */
                    }  /* if */
                  }  /* if */
                }  /* if */
              }
              break;
            case eok_pdiff:
              { a_constexpr_address  *addr1, *addr2;
                addr1 = (a_constexpr_address*)opnd1_value;
                addr2 = (a_constexpr_address*)opnd2_value;
                if (is_array_element(addr1) && is_array_element(addr2) &&
                    get_base_address(addr1) == get_base_address(addr2)) {
                  a_type_ptr    etp = opnd1_type->variant.array.element_type;
                  a_byte_count  elem_size = value_bytes_for_type(
                                                           ips, etp, &result);
                  if (!result) {
                    /* Nothing more to do. */
                  } else if (elem_size == 0) {
                    result = FALSE;  /* FIXME: diagnostic */ 
                  } else {
                    set_integer_value(
                        (an_integer_value*)result_storage,
                        (a_host_large_integer)(addr1->address - addr2->address)
                           / (a_host_large_integer)elem_size);
                    int_kind = tp->variant.integer.int_kind;
                    is_signed = int_kind_is_signed[int_kind];
                    ovfl = FALSE;
                    check_int_range((an_integer_value*)result_storage, tp,
                                    result, ovfl, &expr->position, ips);
                  }  /* if */
                } else {
                  result = FALSE;  /* FIXME: diagnostic */
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
                result = FALSE;
              } else if (host_int_val < 0 ||
                         host_int_val >=
                            (a_host_large_integer)(tp->size * targ_char_bit)) {
                /* FIXME: record a diagnostic for invalid result. */
                result = FALSE;
              }  /* if */
              if (result) {
                shift_left_integer_value((an_integer_value *)opnd1_value,
                                         (int)host_int_val, &ovfl);
                check_int_range(opnd1_value, opnd1_type, result, ovfl,
                                &expr->position, ips);
                if (result) {
                  *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
                }  /* if */
              } else {
                /* FIXME: record a diagnostic for invalid opnd2. */
              }  /* if */
              break;
            case eok_shiftr:
              /* Check for a valid value of opnd2, which must be non-negative
                 and less than the number of bits in opnd1. */
              int_kind = tp->variant.integer.int_kind;
              is_signed = int_kind_is_signed[int_kind];
              get_int_val_from(opnd2_value, opnd2_type, host_int_val, ovfl);
              if (ovfl) {
                result = FALSE;
              } else if (host_int_val < 0 ||
                         host_int_val >=
                            (a_host_large_integer)(tp->size * targ_char_bit)) {
                result = FALSE;
              }  /* if */
              if (result) {
                shift_right_integer_value((an_integer_value *)opnd1_value,
                                          (int)host_int_val, is_signed,
                                          targ_right_shift_is_arithmetic);
                check_int_range(opnd1_value, opnd1_type, result, ovfl,
                                &expr->position, ips);
                if (result) {
                  *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
                }  /* if */
              } else {
                /* FIXME: record a diagnostic for invalid opnd2. */
              }  /* if */
              break;
            case eok_and:
              if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
                and_integer_values((an_integer_value*)result_storage,
                                   (an_integer_value*)opnd2_value);
              }  /* if */
              break;
            case eok_or:
              if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
                *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
                or_integer_values((an_integer_value*)result_storage,
                                  (an_integer_value*)opnd2_value);
              }  /* if */
              break;
            case eok_xor:
              if (expr->variant.operation.type_kind ==
                                                    (a_type_kind)tk_integer) {
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
                /* FIXME: handle NaNs (unord == TRUE)? */
              } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                /* Pointer operands. */
                a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
                a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
                if (is_runtime_data_address(ptr1) ==
                                               is_runtime_data_address(ptr2)) {
                  if (!is_runtime_data_address(ptr1)) {
                    if (ptr1->address == ptr2->address) {
                      *(an_integer_value *)result_storage = one_int;
                    } else {
                      *(an_integer_value *)result_storage = zero_int;
                    }  /* if */
                  } else {
                    result = FALSE;
                  }  /* if */
                } else {
                  result = FALSE;
                }  /* if */
                release_variant_path_if_needed(ptr1);
                release_variant_path_if_needed(ptr2);
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
                /* FIXME: handle NaNs (unord == TRUE)? */
              } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                /* Pointer operands. */
                a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
                a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
                if (is_runtime_data_address(ptr1) ==
                                               is_runtime_data_address(ptr2)) {
                  if (!is_runtime_data_address(ptr1)) {
                    if (ptr1->address != ptr2->address) {
                      *(an_integer_value *)result_storage = one_int;
                    } else {
                      *(an_integer_value *)result_storage = zero_int;
                    }  /* if */
                  } else {
                    result = FALSE;
                  }  /* if */
                } else {
                  result = FALSE;
                }  /* if */
                release_variant_path_if_needed(ptr1);
                release_variant_path_if_needed(ptr2);
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
                /* FIXME: handle NaNs (unord == TRUE)? */
              } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                /* Pointer operands. */
                a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
                a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
                if (is_runtime_data_address(ptr1) ==
                                               is_runtime_data_address(ptr2)) {
                  if (!is_runtime_data_address(ptr1)) {
                    if (ptr1->address < ptr2->address) {
                      *(an_integer_value *)result_storage = one_int;
                    } else {
                      *(an_integer_value *)result_storage = zero_int;
                    }  /* if */
                  } else {
                    result = FALSE;
                  }  /* if */
                } else {
                  result = FALSE;
                }  /* if */
                release_variant_path_if_needed(ptr1);
                release_variant_path_if_needed(ptr2);
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
                /* FIXME: handle NaNs (unord == TRUE)? */
              } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                /* Pointer operands. */
                a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
                a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
                if (is_runtime_data_address(ptr1) ==
                                               is_runtime_data_address(ptr2)) {
                  if (!is_runtime_data_address(ptr1)) {
                    if (ptr1->address > ptr2->address) {
                      *(an_integer_value *)result_storage = one_int;
                    } else {
                      *(an_integer_value *)result_storage = zero_int;
                    }  /* if */
                  } else {
                    result = FALSE;
                  }  /* if */
                } else {
                  result = FALSE;
                }  /* if */
                release_variant_path_if_needed(ptr1);
                release_variant_path_if_needed(ptr2);
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
                /* FIXME: handle NaNs (unord == TRUE)? */
              } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                /* Pointer operands. */
                a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
                a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
                if (is_runtime_data_address(ptr1) ==
                                               is_runtime_data_address(ptr2)) {
                  if (!is_runtime_data_address(ptr1)) {
                    if (ptr1->address <= ptr2->address) {
                      *(an_integer_value *)result_storage = one_int;
                    } else {
                      *(an_integer_value *)result_storage = zero_int;
                    }  /* if */
                  } else {
                    result = FALSE;
                  }  /* if */
                } else {
                  result = FALSE;
                }  /* if */
                release_variant_path_if_needed(ptr1);
                release_variant_path_if_needed(ptr2);
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
                /* FIXME: handle NaNs (unord == TRUE)? */
              } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                /* Pointer operands. */
                a_constexpr_address  *ptr1 = (a_constexpr_address*)opnd1_value;
                a_constexpr_address  *ptr2 = (a_constexpr_address*)opnd2_value;
                if (is_runtime_data_address(ptr1) ==
                                               is_runtime_data_address(ptr2)) {
                  if (!is_runtime_data_address(ptr1)) {
                    if (ptr1->address >= ptr2->address) {
                      *(an_integer_value *)result_storage = one_int;
                    } else {
                      *(an_integer_value *)result_storage = zero_int;
                    }  /* if */
                  } else {
                    result = FALSE;
                  }  /* if */
                } else {
                  result = FALSE;
                }  /* if */
                release_variant_path_if_needed(ptr1);
                release_variant_path_if_needed(ptr2);
              } else {
                unexpected_condition();
              }  /* if */
              break;
            case eok_assign:
              { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
                if (cannot_dereference(dst)) {
                  /* Storing one position past the end of an array. */
                  result = FALSE;
                  info_one_past_end_of_array(dst, expr, ips);
                } else if (is_runtime_data_address(dst)) {
                  /* Cannot modify the value of an object whose lifetime began
                     outside the current evaluation. */
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (!in_live_set(&ips->live_set,
                                        dst->alloc_seq_number)) {
                  /* Attempting to store into expired storage. */
                  info_with_pos(ec_constexpr_access_to_expired_storage,
                                &expr->position, ips);
                  result = FALSE;
                } else if (is_variant_path(dst) &&
                           !check_variant_path(ips, dst, /*release=*/TRUE,
                                               &expr->position)) {
                  /* Attempting to store into a non-active variant field. */
                  result = FALSE;
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
                  *(a_constexpr_address *)result_storage = *dst;
                }  /* if */
              }
              break;
            case eok_add_assign:
              { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
                if (cannot_dereference(dst)) {
                  /* Storing one position past the end of an array. */
                  result = FALSE;
                  info_one_past_end_of_array(dst, expr, ips);
                } else if (is_runtime_data_address(dst)) {
                  /* Cannot modify the value of an object whose lifetime began
                     outside the current evaluation. */
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (!in_live_set(&ips->live_set,
                                        dst->alloc_seq_number)) {
                  /* Attempting to store into expired storage. */
                  info_with_pos(ec_constexpr_access_to_expired_storage,
                                &expr->position, ips);
                  result = FALSE;
                } else if (is_variant_path(dst) &&
                           !check_variant_path(ips, dst, /*release=*/TRUE,
                                               &expr->position)) {
                  /* Attempting to store into a non-active variant field. */
                  result = FALSE;
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
                    check_int_range(int_value_at(dst), tp, result, ovfl,
                                    &expr->position, ips);
                  } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
                    an_internal_float_value  *dst_val = fp_value_at(dst);
                    fp_add(tp->variant.float_kind, dst_val,
                           fp_value(opnd2_value), dst_val, &err,
                           &depends_on_fp_mode);
                    if (err) {
                      result = FALSE;
                      info_with_pos(ec_constexpr_fp_error,
                                    &expr->position, ips);
                    }  /* if */
                  } else {
                    /* FIXME: Other type kinds NYI. */
                    unexpected_condition();
                  }  /* if */
                  *(a_constexpr_address *)result_storage = *dst;
                }  /* if */
              }
              break;
            case eok_subtract_assign:
              { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
                if (cannot_dereference(dst)) {
                  /* Storing one position past the end of an array. */
                  result = FALSE;
                  info_one_past_end_of_array(dst, expr, ips);
                } else if (is_runtime_data_address(dst)) {
                  /* Cannot modify the value of an object whose lifetime began
                     outside the current evaluation. */
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (!in_live_set(&ips->live_set,
                                        dst->alloc_seq_number)) {
                  /* Attempting to store into expired storage. */
                  info_with_pos(ec_constexpr_access_to_expired_storage,
                                &expr->position, ips);
                  result = FALSE;
                } else if (is_variant_path(dst) &&
                           !check_variant_path(ips, dst, /*release=*/TRUE,
                                               &expr->position)) {
                  /* Attempting to store into a non-active variant field. */
                  result = FALSE;
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
                    check_int_range(int_value_at(dst), tp, result, ovfl,
                                    &expr->position, ips);
                  } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
                    an_internal_float_value  *dst_val = fp_value_at(dst);
                    fp_subtract(tp->variant.float_kind, dst_val,
                                fp_value(opnd2_value), dst_val, &err,
                                &depends_on_fp_mode);
                    if (err) {
                      result = FALSE;
                      info_with_pos(ec_constexpr_fp_error,
                                    &expr->position, ips);
                    }  /* if */
                  } else {
                    /* FIXME: Other type kinds NYI. */
                    unexpected_condition();
                  }  /* if */
                  *(a_constexpr_address *)result_storage = *dst;
                }  /* if */
              }
              break;
            case eok_multiply_assign:
              { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
                if (cannot_dereference(dst)) {
                  /* Storing one position past the end of an array. */
                  result = FALSE;
                  info_one_past_end_of_array(dst, expr, ips);
                } else if (is_runtime_data_address(dst)) {
                  /* Cannot modify the value of an object whose lifetime began
                     outside the current evaluation. */
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (!in_live_set(&ips->live_set,
                                        dst->alloc_seq_number)) {
                  /* Attempting to store into expired storage. */
                  info_with_pos(ec_constexpr_access_to_expired_storage,
                                &expr->position, ips);
                  result = FALSE;
                } else if (is_variant_path(dst) &&
                           !check_variant_path(ips, dst, /*release=*/TRUE,
                                               &expr->position)) {
                  /* Attempting to store into a non-active variant field. */
                  result = FALSE;
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
                    check_int_range(int_value_at(dst), tp, result, ovfl,
                                    &expr->position, ips);
                  } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
                    an_internal_float_value  *dst_val = fp_value_at(dst);
                    fp_multiply(tp->variant.float_kind, dst_val,
                                fp_value(opnd2_value), dst_val, &err,
                                &depends_on_fp_mode);
                    if (err) {
                      result = FALSE;
                      info_with_pos(ec_constexpr_fp_error,
                                    &expr->position, ips);
                    }  /* if */
                  } else {
                    /* FIXME: Other type kinds NYI. */
                    unexpected_condition();
                  }  /* if */
                  *(a_constexpr_address *)result_storage = *dst;
                }  /* if */
              }
              break;
            case eok_divide_assign:
              { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
                if (cannot_dereference(dst)) {
                  /* Storing one position past the end of an array. */
                  result = FALSE;
                  info_one_past_end_of_array(dst, expr, ips);
                } else if (is_runtime_data_address(dst)) {
                  /* Cannot modify the value of an object whose lifetime began
                     outside the current evaluation. */
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (!in_live_set(&ips->live_set,
                                        dst->alloc_seq_number)) {
                  /* Attempting to store into expired storage. */
                  info_with_pos(ec_constexpr_access_to_expired_storage,
                                &expr->position, ips);
                  result = FALSE;
                } else if (is_variant_path(dst) &&
                           !check_variant_path(ips, dst, /*release=*/TRUE,
                                               &expr->position)) {
                  /* Attempting to store into a non-active variant field. */
                  result = FALSE;
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
                    check_int_range(int_value_at(dst), tp, result, ovfl,
                                    &expr->position, ips);
                  } else if (expr->variant.operation.type_kind ==
                                                      (a_type_kind)tk_float) {
                    an_internal_float_value  *dst_val = fp_value_at(dst);
                    fp_divide(tp->variant.float_kind, dst_val,
                              fp_value(opnd2_value), dst_val, &err,
                              &depends_on_fp_mode);
                    if (err) {
                      result = FALSE;
                      info_with_pos(ec_constexpr_fp_error,
                                    &expr->position, ips);
                    }  /* if */
                  } else {
                    /* FIXME: Other type kinds NYI. */
                    unexpected_condition();
                  }  /* if */
                  *(a_constexpr_address *)result_storage = *dst;
                }  /* if */
              }
              break;
            case eok_remainder_assign:
              { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
                if (cannot_dereference(dst)) {
                  /* Storing one position past the end of an array. */
                  result = FALSE;
                  info_one_past_end_of_array(dst, expr, ips);
                } else if (is_runtime_data_address(dst)) {
                  /* Cannot modify the value of an object whose lifetime began
                     outside the current evaluation. */
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (!in_live_set(&ips->live_set,
                                        dst->alloc_seq_number)) {
                  /* Attempting to store into expired storage. */
                  info_with_pos(ec_constexpr_access_to_expired_storage,
                                &expr->position, ips);
                  result = FALSE;
                } else if (is_variant_path(dst) &&
                           !check_variant_path(ips, dst, /*release=*/TRUE,
                                               &expr->position)) {
                  /* Attempting to store into a non-active variant field. */
                  result = FALSE;
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
                  check_int_range(int_value_at(dst), tp, result, ovfl,
                                  &expr->position, ips);
                  *(a_constexpr_address *)result_storage = *dst;
                } else {
                  /* FIXME: Other type kinds NYI. */
                  unexpected_condition();
                }  /* if */
              }
              break;
            case eok_shiftl_assign:
              { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
                if (cannot_dereference(dst)) {
                  /* Storing one position past the end of an array. */
                  result = FALSE;
                  info_one_past_end_of_array(dst, expr, ips);
                } else if (is_runtime_data_address(dst)) {
                  /* Cannot modify the value of an object whose lifetime began
                     outside the current evaluation. */
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (!in_live_set(&ips->live_set,
                                        dst->alloc_seq_number)) {
                  /* Attempting to store into expired storage. */
                  info_with_pos(ec_constexpr_access_to_expired_storage,
                                &expr->position, ips);
                  result = FALSE;
                } else if (is_variant_path(dst) &&
                           !check_variant_path(ips, dst, /*release=*/TRUE,
                                               &expr->position)) {
                  /* Attempting to store into a non-active variant field. */
                  result = FALSE;
                } else {
                  /* Shift the bits stored in the first operand left by the
                     number of bits indicated by the second operand.  Return
                     the left operand (as an lvalue). */
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  get_int_val_from(opnd2_value, opnd2_type, host_int_val,
                                   ovfl);
                  if (ovfl) {
                    result = FALSE;
                  } else if (host_int_val < 0 ||
                             host_int_val >=
                            (a_host_large_integer)(tp->size * targ_char_bit)) {
                    result = FALSE;
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
                    check_int_range(int_value_at(dst), tp, result, ovfl,
                                    &expr->position, ips);
                    *(a_constexpr_address *)result_storage = *dst;
                  }  /* if */
                }  /* if */
              }
              break;
            case eok_shiftr_assign:
              { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
                if (cannot_dereference(dst)) {
                  /* Storing one position past the end of an array. */
                  result = FALSE;
                  info_one_past_end_of_array(dst, expr, ips);
                } else if (is_runtime_data_address(dst)) {
                  /* Cannot modify the value of an object whose lifetime began
                     outside the current evaluation. */
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (!in_live_set(&ips->live_set,
                                        dst->alloc_seq_number)) {
                  /* Attempting to store into expired storage. */
                  info_with_pos(ec_constexpr_access_to_expired_storage,
                                &expr->position, ips);
                  result = FALSE;
                } else if (is_variant_path(dst) &&
                           !check_variant_path(ips, dst, /*release=*/TRUE,
                                               &expr->position)) {
                  /* Attempting to store into a non-active variant field. */
                  result = FALSE;
                } else {
                  /* Shift the bits stored in the first operand left by the
                     number of bits indicated by the second operand.  Return
                     the left operand (as an lvalue). */
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  get_int_val_from(opnd2_value, opnd2_type, host_int_val,
                                   ovfl);
                  if (ovfl) {
                    result = FALSE;
                  } else if (host_int_val < 0 ||
                             host_int_val >=
                            (a_host_large_integer)(tp->size * targ_char_bit)) {
                    result = FALSE;
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
                    check_int_range(int_value_at(dst), tp, result, ovfl,
                                    &expr->position, ips);
                    trim_bit_field_if_needed(dst);
                    *(a_constexpr_address *)result_storage = *dst;
                  }  /* if */
                }  /* if */
              }
              break;
            case eok_and_assign:
              { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
                if (cannot_dereference(dst)) {
                  /* Storing one position past the end of an array. */
                  result = FALSE;
                  info_one_past_end_of_array(dst, expr, ips);
                } else if (is_runtime_data_address(dst)) {
                  /* Cannot modify the value of an object whose lifetime began
                     outside the current evaluation. */
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (!in_live_set(&ips->live_set,
                                        dst->alloc_seq_number)) {
                  /* Attempting to store into expired storage. */
                  info_with_pos(ec_constexpr_access_to_expired_storage,
                                &expr->position, ips);
                  result = FALSE;
                } else if (is_variant_path(dst) &&
                           !check_variant_path(ips, dst, /*release=*/TRUE,
                                               &expr->position)) {
                  /* Attempting to store into a non-active variant field. */
                  result = FALSE;
                } else {
                  /* Bitwise "and" the value stored in the left operand with
                     the value of the right operand and leave the result in the
                     left operand.  Return the left operand (as an lvalue). */
                  and_integer_values(int_value_at(dst),
                                     (an_integer_value*)opnd2_value);
                  *(a_constexpr_address *)result_storage = *dst;
                }  /* if */
              }
              break;
            case eok_or_assign:
              { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
                if (cannot_dereference(dst)) {
                  /* Storing one position past the end of an array. */
                  result = FALSE;
                  info_one_past_end_of_array(dst, expr, ips);
                } else if (is_runtime_data_address(dst)) {
                  /* Cannot modify the value of an object whose lifetime began
                     outside the current evaluation. */
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (!in_live_set(&ips->live_set,
                                        dst->alloc_seq_number)) {
                  /* Attempting to store into expired storage. */
                  info_with_pos(ec_constexpr_access_to_expired_storage,
                                &expr->position, ips);
                  result = FALSE;
                } else if (is_variant_path(dst) &&
                           !check_variant_path(ips, dst, /*release=*/TRUE,
                                               &expr->position)) {
                  /* Attempting to store into a non-active variant field. */
                  result = FALSE;
                } else {
                  /* Bitwise "or" the value stored in the left operand with
                     the value of the right operand and leave the result in the
                     left operand.  Return the left operand (as an lvalue). */
                  or_integer_values(int_value_at(dst),
                                    (an_integer_value*)opnd2_value);
                  *(a_constexpr_address *)result_storage = *dst;
                }  /* if */
              }
              break;
            case eok_xor_assign:
              { a_constexpr_address  *dst = (a_constexpr_address*)opnd1_value;
                if (cannot_dereference(dst)) {
                  /* Storing one position past the end of an array. */
                  result = FALSE;
                  info_one_past_end_of_array(dst, expr, ips);
                } else if (is_runtime_data_address(dst)) {
                  /* Cannot modify the value of an object whose lifetime began
                     outside the current evaluation. */
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (!in_live_set(&ips->live_set,
                                        dst->alloc_seq_number)) {
                  /* Attempting to store into expired storage. */
                  info_with_pos(ec_constexpr_access_to_expired_storage,
                                &expr->position, ips);
                  result = FALSE;
                } else if (is_variant_path(dst) &&
                           !check_variant_path(ips, dst, /*release=*/TRUE,
                                               &expr->position)) {
                  /* Attempting to store into a non-active variant field. */
                  result = FALSE;
                } else {
                  /* Bitwise "xor" the value stored in the left operand with
                     the value of the right operand and leave the result in the
                     left operand.  Return the left operand (as an lvalue). */
                  xor_integer_values(int_value_at(dst),
                                     (an_integer_value*)opnd2_value);
                  *(a_constexpr_address *)result_storage = *dst;
                }  /* if */
              }
              break;
            case eok_padd_assign:
              /* FIXME: NYI. */
              unexpected_condition();
              break;
            case eok_psubtract_assign:
              /* FIXME: NYI. */
              unexpected_condition();
              break;
            case eok_land:
              { a_boolean             logical_and_result;
                a_host_large_integer  bool_val;
                if (opnd1_type->kind == (a_type_kind)tk_integer) {
                  int_kind = opnd1_type->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  get_int_val_from(opnd1_value, opnd1_type, bool_val, ovfl);
                  logical_and_result = ovfl || bool_val;
                } else {
                  /* FIXME: NYI, other source types. */
                  logical_and_result = FALSE;
                  result = FALSE;
                  unexpected_condition();
                }  /* if */
                if (!logical_and_result) {
                  /* Short-circuit the second operand evaluation. */
                } else {
                  /* Evaluate the second operand. */
                  opnd2_type = skip_typerefs(opnd2->type);
                  opnd_n_bytes = value_bytes_for_type(ips, opnd2_type,
                                                      &result);
                  if (!is_compact_value_size(opnd_n_bytes) &&
                      !opnd2->is_lvalue && !opnd2->is_xvalue) {
                    /* The value may be larger than a scalar type, so allocate
                       space for it on the stack. */
                    alloc_stack_bytes(ips, opnd_n_bytes, opnd2_value);
                  } else {
                    opnd2_value = compact_value_bytes(opnd2_bytes);
                  }  /* if */
                  record_complete_object(opnd2_type, opnd2_value);
                  if (result &&
                      !do_constexpr_expression(ips, opnd2, opnd2_value)) {
                    result = FALSE;
                  }  /* if */
                  if (!result) {
                    /* Interpretation of the second operand failed. */
                  } else if (opnd2_type->kind == (a_type_kind)tk_integer) {
                    int_kind = opnd2_type->variant.integer.int_kind;
                    is_signed = int_kind_is_signed[int_kind];
                    get_int_val_from(opnd2_value, opnd2_type, bool_val, ovfl);
                    logical_and_result = ovfl || bool_val;
                  } else {
                    /* FIXME: NYI, other source types. */
                    unexpected_condition();
                  }  /* if */
                }  /* if */
                if (result) {
                  if (logical_and_result) {
                    *(an_integer_value *)result_storage = one_int;
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
                  }  /* if */
                }  /* if */
              }
              break;
            case eok_lor:
              { a_boolean             logical_or_result;
                a_host_large_integer  bool_val;
                if (opnd1_type->kind == (a_type_kind)tk_integer) {
                  int_kind = opnd1_type->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  get_int_val_from(opnd1_value, opnd1_type, bool_val, ovfl);
                  logical_or_result = ovfl || bool_val;
                } else {
                  /* FIXME: NYI, other source types. */
                  logical_or_result = TRUE;
                  result = FALSE;
                  unexpected_condition();
                }  /* if */
                if (logical_or_result) {
                  /* Short-circuit the second operand evaluation. */
                } else {
                  /* Evaluate the second operand. */
                  opnd2_type = skip_typerefs(opnd2->type);
                  opnd_n_bytes = value_bytes_for_type(ips, opnd2_type,
                                                      &result);
                  if (!is_compact_value_size(opnd_n_bytes) &&
                      !opnd2->is_lvalue && !opnd2->is_xvalue) {
                    /* The value may be larger than a scalar type, so allocate
                       space for it on the stack. */
                    alloc_stack_bytes(ips, opnd_n_bytes, opnd2_value);
                  } else {
                    opnd2_value = compact_value_bytes(opnd2_bytes);
                  }  /* if */
                  record_complete_object(opnd2_type, opnd2_value);
                  if (result &&
                      !do_constexpr_expression(ips, opnd2, opnd2_value)) {
                    result = FALSE;
                  }  /* if */
                  if (!result) {
                    /* Interpretation of the second operand failed. */
                  } else if (opnd2_type->kind == (a_type_kind)tk_integer) {
                    int_kind = opnd2_type->variant.integer.int_kind;
                    is_signed = int_kind_is_signed[int_kind];
                    get_int_val_from(opnd2_value, opnd2_type, bool_val, ovfl);
                    logical_or_result = ovfl || bool_val;
                  } else {
                    /* FIXME: NYI, other source types. */
                    unexpected_condition();
                  }  /* if */
                }  /* if */
                if (result) {
                  if (logical_or_result) {
                    *(an_integer_value *)result_storage = one_int;
                  } else {
                    *(an_integer_value *)result_storage = zero_int;
                  }  /* if */
                }  /* if */
              }
              break;
            case eok_comma:
              result = do_constexpr_expression(ips, opnd2, result_storage);
              break;
            case eok_subscript:
              /* Pointer + integer or integer + pointer. */
              { a_constexpr_address  result_addr;
                a_type_ptr           elem_type;
                /* Place the pointer in result_addr and the integer in
                   host_int_val. */
                if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                  get_int_val_from(opnd2_value, opnd2_type, host_int_val,
                                   ovfl);
                  result_addr = *(a_constexpr_address *)opnd1_value;
                  elem_type = skip_typerefs(opnd1->type->variant.pointer.type);
                } else {
                  get_int_val_from(opnd1_value, opnd1_type, host_int_val,
                                   ovfl);
                  result_addr = *(a_constexpr_address *)opnd2_value;
                  elem_type = skip_typerefs(opnd1->type->variant.pointer.type);
                }  /* if */
                /* Carefully add the two, if appropriate. */
                if (ovfl) {
                  result = FALSE;
                  info_with_pos(ec_integer_overflow, &expr->position, ips);
                } else {
                  if (host_int_val == 0) {
                    /* Leave the address unchanged. */
                    set_result_val_from_operand_address(&result_addr);
                  } else if (!is_array_element(&result_addr)) {
                    result = FALSE;
                    info_with_pos(ec_constexpr_non_array_subscript,
                                  &expr->position, ips);
                  } else {
                    a_byte_count  elem_size, pos, len;
                    a_byte        *base_address;
                    elem_size = value_bytes_for_type(ips, elem_type, &result);
                    if (!result) break;
                    len = result_addr.length;
                    base_address = get_base_address(&result_addr);
                    pos = (a_byte_count)(result_addr.address - base_address)
                                        / elem_size;
                    if (host_int_val > 0 ?
                                        (len-pos < (a_byte_count)host_int_val)
                                      : (pos < (a_byte_count)-host_int_val)) {
                      /* Out of bounds. */
                      result = FALSE;
                      info_with_pos_num2(
                                      ec_constexpr_out_of_bounds_array_access,
                                      &expr->position,
                                      (unsigned long)(pos+host_int_val),
                                      (unsigned long)len, ips);
                    } else {
                      result_addr.address +=
                        host_int_val
                              * value_bytes_for_type(ips, elem_type, &result);
                      if (!result) break;
                      if (pos+host_int_val == len) {
                        result_addr.flags |= CA_CANNOT_DEREFERENCE;
                      } else {
                        result_addr.flags &= ~CA_CANNOT_DEREFERENCE;
                      }  /* if */
                      set_result_val_from_operand_address(&result_addr);
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
                result_addr = *(a_constexpr_address*)opnd1_value;
                if (opnd1_type->kind == (a_type_kind)tk_union &&
                    !is_runtime_data_address(&result_addr) &&
                    !add_to_variant_path(&result_addr, field)) {
                  /* We should not return from the failure of adding a variant
                     path entry. */
                  unexpected_condition();
                } else {
                  get_mapped_byte_count(&persistent_map, field, offset);
                  result_addr.address += offset;
                  result_addr.flags &= ~CA_ARRAY_ELEMENT;
                  if (field->is_bit_field) {
                    if (field->bit_field_is_signed) {
                      result_addr.flags |= (CA_BIT_FIELD |
                                            CA_SIGNED_BIT_FIELD);
                    } else {
                      result_addr.flags |= CA_BIT_FIELD;
                    }  /* if */
                    result_addr.length = field->bit_size;
                  }  /* if */
                  set_result_val_from_operand_address(&result_addr);
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
                if (is_runtime_data_address(&result_addr)) {
                  result = FALSE;
                  info_with_pos(ec_constexpr_access_to_runtime_storage,
                                &expr->position, ips);
                } else if (field == NULL) {
                  result = FALSE;
                  info_with_pos(ec_constexpr_null_ptr_to_member_data,
                                &expr->position, ips);
                } else if (opnd1_type->kind == (a_type_kind)tk_union &&
                           !add_to_variant_path(&result_addr, field)) {
                  /* We should not return from the failure of adding a variant
                     path entry. */
                  unexpected_condition();
                } else {
                  if (!adjust_this_address(ips, &result_addr, pm_value,
                                           opnd1_type, expr)) {
                    result = FALSE;
                  } else {
                    get_mapped_byte_count(&persistent_map, field, offset);
                    result_addr.address += offset;
                    result_addr.flags &= ~CA_ARRAY_ELEMENT;
                    if (field->is_bit_field) {
                      if (field->bit_field_is_signed) {
                        result_addr.flags |= (CA_BIT_FIELD |
                                              CA_SIGNED_BIT_FIELD);
                      } else {
                        result_addr.flags |= CA_BIT_FIELD;
                      }  /* if */
                      result_addr.length = field->bit_size;
                    }  /* if */
                  }  /* if */
                  set_result_val_from_operand_address(&result_addr);
                }  /* if */
              }
              break;
            case eok_dot_static:
            case eok_points_to_static:
              result = do_constexpr_expression(ips, opnd2, result_storage);
              break;
            case eok_question:
              { a_host_large_integer  bool_val;
                if (opnd1_type->kind == (a_type_kind)tk_integer) {
                  int_kind = opnd1_type->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  get_int_val_from(opnd1_value, opnd1_type, bool_val, ovfl);
                  if (!(ovfl || bool_val)) {
                    /* Evaluate the third operand. */
                    opnd2 = opnd2->next;
                  }  /* if */
                } else {
                  unexpected_condition();
                }  /* if */
                result = do_constexpr_expression(ips, opnd2, result_storage);
              }
              break;
            case eok_call:
              /* Calls are handled separately.  We should not get here. */
              unexpected_condition();
              /*FALLTHROUGH*/
            default:
              result = FALSE;
              info_with_pos(ec_constexpr_expression_cannot_be_interpreted,
                            &expr->position, ips);
          }  /* switch */
        }  /* if */
      }
      break;
    case enk_constant:
      {
        a_constant_ptr  con = node_constant(expr);
        a_byte          *con_bytes;
        if (tp->kind == (a_type_kind)tk_array &&
            (expr->is_lvalue || expr->is_xvalue)) {
          /* An array lvalue (normally: a string literal).  Allocate the
             string statically and return its address. */
          a_byte_count  na_bytes = f_value_bytes_for_type(ips, tp, &result);
          if (!result) break;
          if (!ips->static_storage_ready) {
            /* This is the first time we allocate static storage: Initialize
               the associated static storage stack. */
            init_constexpr_stack(&ips->static_storage);
            ips->static_storage_ready = TRUE;
          }  /* if */
          alloc_bytes(&ips->static_storage, na_bytes, con_bytes);
          clear_address(result_storage, con_bytes);
        } else {
          con_bytes = result_storage;
        }  /* if */
        result = copy_val_from_constant(ips, con, con_bytes);
      }
      break;
    case enk_variable:
      {
        a_variable_ptr  var = node_variable(expr);
        a_byte          *var_bytes;
        get_stack_bytes(ips, var, var_bytes);
        if (!expr->is_lvalue && !expr->is_xvalue) {
          /* A variable used as an rvalue; the result is its associated
             value bytes. */
          if (var_bytes != NULL) {
            /* This is a variable on the interpreter stack. */
            (void)memcpy(result_storage, var_bytes, size_t_arg(n_bytes));
            if (tp->kind == (a_type_kind)tk_pointer) {
              /* Copying an address type.  Make sure its side structures, if
                 any, are not shared. */
              copy_address_structures(result_storage);
            }  /* if */
          } else {
            a_constant_ptr  con = var_constant_value(var);
            if (con != NULL) {
              result = copy_val_from_constant(ips, con, result_storage);
            } else {
              info_with_pos_sym(ec_variable_not_constant_valued,
                                &expr->position, symbol_for(var), ips);
              result = FALSE;
            }  /* if */
          }  /* if */
        } else {
          /* A variable used as a glvalue; the result is its address. */
          if (var_bytes != NULL) {
            a_constexpr_address
                            *p_address = (a_constexpr_address*)result_storage;
            clear_address(result_storage, var_bytes);
            /* Record the allocation sequence number for this variable in the
               address record. */
            get_mapped_byte_count(&ips->map, &var->storage_class,
                                  p_address->alloc_seq_number);
          } else {
            a_constant_ptr  con;
            a_byte          *con_ptr;
            get_mapped_ptr(&ips->map, &var->storage_class, con_ptr);
            con = (a_constant_ptr)con_ptr;
            if (con == NULL) {
              con = local_constant();
              map_ptr(&ips->map, &var->storage_class, (a_byte*)con);
              con->next = ips->constants;
              ips->constants = con;
            }  /* if */
            if (constant_glvalue_address(expr, con,
                                         /*address_escapes=*/FALSE)) {
              if (!extract_value_from_constant(ips, con, result_storage)) {
                /* The address of a run-time variable. */
                clear_runtime_constant_address(result_storage, con);
              }  /* if */
            } else {
              info_with_pos_sym(ec_variable_not_constant_addressed,
                                &expr->position, symbol_for(var), ips);
              result = FALSE;
            }  /* if */
          }  /* if */
        }  /* if */
      }
      break;
    case enk_field:
      /* Nothing to do at this point.  Specific parent operators (like
         eok_dot_field) know what to do with this kind of node. */
      break;
    case enk_routine:
      make_function_address(result_storage, node_routine(expr));
      break;
    case enk_temp_init:
      { a_dynamic_init_ptr     dip = expr->variant.init.dynamic_init;
        a_byte                 *tmp_bytes;
        an_alloc_seq_number    alloc_seq_number;
        if (expr->is_lvalue || expr->is_xvalue) {
          /* An glvalue temporary is expected.  I.e., the caller expects an
             interpreter address for the temporary object.  Allocate the
             storage for that object here. */
          n_bytes = value_bytes_for_type(ips, tp, &result);
          if (!result) break;
          if (!dip->has_temporary_lifetime && ips->extension_state != NULL) {
            /* A life-time extended temporary.  Switch to the storage stack
               state was saved at the time the stmk_init statement was
               started. */
            /* If we're processing the initializer of a static-lifetime
               variable.  E.g.,
                 constexpr std::initializer_list<int> x = { 1, 2 };
               there is no extended-lifetime storage.  Instead, the result
               will be stored in IL, which is persistent across interpreter
               invocations. */
            alloc_bytes(ips->extension_state, n_bytes, tmp_bytes);
            alloc_seq_number = ips->extension_state->alloc_seq_number;
            ips->extension_state = NULL;
          } else {
            alloc_stack_bytes(ips, n_bytes, tmp_bytes);
            alloc_seq_number = ips->storage_stack.alloc_seq_number;
          }  /* if */
          record_complete_object(tp, tmp_bytes);
        } else {
          /* The consumer of the temporary expects an rvalue.  So we can
             evaluate the initialization directly into result_storage. */
          tmp_bytes = result_storage;
          alloc_seq_number = 0;
        }  /* if */
        if (!do_constexpr_dynamic_init(ips, dip, &expr->position, tmp_bytes)) {
          result = FALSE;
        }  /* if */
        if (expr->is_lvalue || expr->is_xvalue) {
          a_constexpr_address
                            *p_address = (a_constexpr_address*)result_storage;
          clear_address(p_address, tmp_bytes);
          /* Record the allocation sequence number for this temporary in the
             address record. */ 
          p_address->alloc_seq_number = alloc_seq_number;
        }  /* if */
      }
      break;
    default:
      result = FALSE;
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
  init_data_map(&persistent_map);
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


static a_boolean copy_interpreter_object_to_constant(
                                                an_interpreter_state  *ips,
                                                a_byte                *object,
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
/* FIXME: replace alloc_constant by local_constant? */
  a_boolean  result = TRUE;

  clear_constant(con, (a_constant_repr_kind)ck_error);
  con->type = type;
  con->is_result_of_constexpr_call = TRUE;
  type = skip_typerefs(type);
  switch (type->kind) {
    case tk_integer:
      set_constant_kind(con, (a_constant_repr_kind)ck_integer);
      con->variant.integer_value = *(an_integer_value *)object;
      break;
    case tk_float:
      set_constant_kind(con, (a_constant_repr_kind)ck_float);
      con->variant.float_value = *fp_value(object);
      break;
    case tk_pointer:
      { a_constexpr_address *cap = (a_constexpr_address *)object;
        if (is_runtime_data_address(cap)) {
          /* Copy the address constant to result_con and release the local
             constant. */
          copy_constant(cap->variant.addr_con, con);
        } else if (cap->address == NULL) {
          /* A NULL pointer constant. */
          set_constant_kind(con, (a_constant_repr_kind)ck_integer);
        } else if (cap->alloc_seq_number > 1) {
          /* The address designates an interpreter value that is already
             deallocated, and thus cannot be constant. */
          result = FALSE;
          info_with_pos(ec_constexpr_interpreter_address, &ips->position, ips);
        } else {
          /* Create an abk_constant or abk_temporary entry. */
          a_constant_ptr  cp = alloc_constant((a_constant_repr_kind)ck_error);
          a_type_ptr      utp = skip_typerefs(type->variant.pointer.type);
          set_constant_kind(con, (a_constant_repr_kind)ck_address);
          if (is_array_element(cap)) {
            /* If we're pointing into an array, a constant for the whole array
               must be allocated. */
            a_type_ptr    atp = alloc_type((a_type_kind)tk_array);
            a_byte_count  offset = cap->address - get_base_address(cap);
            if (offset != 0) {
              a_byte_count  n_bytes = value_bytes_for_type(ips, utp, &result);
              con->variant.address.offset = utp->size * (offset/n_bytes);
            }  /* if */
            atp->variant.array.element_type = utp;
            atp->variant.array.variant.number_of_elements = cap->length;
            set_type_size(atp);
            utp = atp;
          }  /* if */
          if (!copy_interpreter_object_to_constant(
                                                ips, cap->address, utp, cp)) {
            result = FALSE;
            break;
          }  /* if */
          if (utp->kind == (a_type_kind)tk_array ||
              is_immediate_class_type(utp)) {
            con->variant.address.kind = (an_address_base_kind)abk_constant;
          } else {
            con->variant.address.kind = (an_address_base_kind)abk_temporary;
          }  /* if */
          con->variant.address.variant.constant = cp;
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
        set_constant_kind(con, (a_constant_repr_kind)ck_aggregate);
        /* Add direct base sub-object constants first. */
        for (bcp = base_classes_of(type); bcp != NULL; bcp = bcp->next) {
          a_byte_count    offset;
          a_constant_ptr  cp;
          if (!bcp->direct || bcp->is_virtual) continue;
          cp = alloc_constant((a_constant_repr_kind)ck_error);
          get_mapped_byte_count(&persistent_map, bcp, offset);
          if (!copy_interpreter_object_to_constant(
                                         ips, object+offset, bcp->type, cp)) {
            result = FALSE;
            break;
          }  /* if */
          cp->constant_for_base_class_from_constexpr_folding = TRUE;
          add_constant_to_aggregate(cp, con);
        }  /* for */
        if (!result) break;
        /* Now add the constants for initializable fields. */
        fp = next_initializable_field(fp);
        for (; fp != NULL; fp = next_initializable_field(fp->next)) {
          a_byte_count    offset;
          a_constant_ptr  cp;
          if (fp->compiler_generated && !fp->is_anonymous_parent_object) {
            /* Ignore fields generated by prelowering. */
            continue;
          }  /* if */
          get_mapped_byte_count(&persistent_map, fp, offset);
          cp = alloc_constant((a_constant_repr_kind)ck_error);
          if (!copy_interpreter_object_to_constant(
                                          ips, object+offset, fp->type, cp)) {
            result = FALSE;
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
        fp = next_initializable_field(fp);
        /* Retrieve the active field. */
        afp = (a_field_ptr)*(void**)object;
        if (afp == NULL) {
          /* This should only happen with unions that have no field (and
             therefore cannot have an active field). */
          check_assertion(fp == NULL);
        } else {
          a_constant_ptr  elem_con, des_con;
          a_byte_count    offset;
          elem_con = alloc_constant((a_constant_repr_kind)ck_error);
          get_mapped_byte_count(&persistent_map, afp, offset);
          if (!copy_interpreter_object_to_constant(
                                    ips, object+offset, afp->type, elem_con)) {
            result = FALSE;
          } else {
            if (fp != afp) {
              /* Add a designator for the active field. */
              des_con = alloc_constant((a_constant_repr_kind)ck_designator);
              des_con->variant.designator.field = afp;
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
                                               ips, sub_obj, etp, elem_con)) {
            result = FALSE;
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
                                               ips, sub_obj, etp, elem_con)) {
            result = FALSE;
            break;
          }  /* if */
          add_constant_to_aggregate(elem_con, con);
        }  /* for */
      }
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tk_void:
      set_constant_kind(con, (a_constant_repr_kind)ck_void);
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* copy_interpreter_object_to_constant */


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
  n_bytes = value_bytes_for_type(&ips, result_type, &result); 
  if (!result) {
    /* Nothing more to be done. */
  } else {
    alloc_complete_object(&ips, n_bytes, result_type, result_storage);
    result_con->type = result_type;
    if (!do_constexpr_call(&ips, call_expr, result_storage)) {
      result = FALSE;
    } else if (!copy_interpreter_object_to_constant(
                             &ips, result_storage, result_type, result_con)) {
      result = FALSE;
    }  /* if */
  }  /* if */
  *diag_list = ips.diag_list;
  release_interpreter_state(&ips);
#if CHECKING
  /* Check that all variant path entries have been freed. */
  { unsigned long             n_freed = 0;
    a_variant_path_entry_ptr  vpep = free_variant_path_entries;
    for (; vpep != NULL; vpep = vpep->next) ++n_freed;
    check_assertion_str(n_freed == n_variant_path_entries,
                        "Not all variant path entries freed");
  }
#endif /* CHECKING */
  return result;
}  /* interpret_constexpr_call */


a_boolean interpret_constexpr_ctor(a_dynamic_init_ptr  dip,
                                   a_constant_ptr      result_con)
/*
Attempt to interpret the constructor call represented by dip.  Return TRUE if
successful, and produce the resulting value in result_con.  Otherwise,
return FALSE.
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
    if (ctor == NULL) {
      expect_error();
      result = FALSE;
      goto done;
    } else if (!ctor->is_constexpr) {
      info_with_pos_sym(ec_constexpr_call_to_nonconstexpr_function,
                        &error_position, symbol_for(ctor), &ips);
      result = FALSE;
      goto done;
    }  /* if */
  }  /* if */
  result_type = parent_class_of(ctor);
  n_bytes = value_bytes_for_type(&ips, result_type, &result); 
  alloc_complete_object(&ips, n_bytes, result_type, result_storage);
  if (result) {
    if (!do_constexpr_ctor(&ips, dip, &error_position, result_storage)) {
      result = FALSE;
    } else if (!copy_interpreter_object_to_constant(
                             &ips, result_storage, result_type, result_con)) {
      result = FALSE;
    }  /* if */
  }  /* if */
  release_interpreter_state(&ips);
#if CHECKING
  /* Check that all variant path entries have been freed. */
  { unsigned long             n_freed = 0;
    a_variant_path_entry_ptr  vpep = free_variant_path_entries;
    for (; vpep != NULL; vpep = vpep->next) ++n_freed;
    check_assertion_str(n_freed == n_variant_path_entries,
                        "Not all variant path entries freed");
  }
#endif /* CHECKING */
done:
  return result;
}  /* interpret_constexpr_ctor */


void interpret_trans_unit_init(void)
/*
Initialize static variables related to the interpreter.  These are variables
that need initialization for every (primary and secondary) translation unit.
*/
{
  trans_unit_initialization_needed = TRUE;
}  /* interpret_trans_unit_init */


void interpret_one_time_init(void)
/*
One-time initialization for interpret.c static variables.
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(persistent_data),
      pch_saved_var_array_elem(persistent_map),
      pch_saved_var_array_elem(free_stack_blocks),
      pch_saved_var_array_elem(free_map_tables),
      pch_saved_var_array_elem(free_live_set_tables),
      pch_saved_var_array_elem(free_variant_path_entries),
      pch_saved_var_array_elem(n_variant_path_entries),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Static variables in interpret.c. */
  register_trans_unit_variable(persistent_data);
  register_trans_unit_variable(persistent_map);
  useful_constants_initialized = FALSE;
}  /* interpret_one_time_init */
/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
