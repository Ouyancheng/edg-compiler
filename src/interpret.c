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
The principal entry point is interpret_constexpr_call, which sets up an
"interpreter state" that is carried through the interpretation process.

An interpreter invocation can end for one of three reasons:
  (1) the call is completed with a valid result (normal case),
  (2) interpretation runs into an invalid operation (e.g., an attempt to
      read an uninitialized value), or
  (3) the cost of the interpretation is too high.

Regarding the latter reason, the interpreter tracks the sum of the number of
calls and the number of loop-back branches.  When that number reaches a certain
large value, the interpretation is deemed too expensive.


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
Storage can be associated with a particular IL entry through a data map (an
efficient pointer-to-pointer hash table).  An association is established by
invoking the macro map_ptr, and revoked by invoking unmap_ptr.  A mapping
can be retrieved with the macro get_mapped_ptr.

The interpreter state includes a data map for automatic variables and
temporaries; convenience macros map_stack_bytes, unmap_stack_bytes, and
get_stack_bytes can be used to manage that data map.

If a pointer is mapped multiple times (e.g., a local variable entry during a
recursive function invocation), the last mapping is returned by get_mapped_ptr
(or get_stack_bytes), and if that last mapping is "unmapped", the previous
mapping becomes available again.

Besides the data map associated with an interpreter state, another map is kept
that persists across interpreter invocations (static variable persistent_map).
This map, e.g., holds data layout information for associated with types and
fields (the data layout for the interpreter is different from that for the
target architecture).
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
    map->next_free = NUM_DATA_MAP_HASH_HEADERS;
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
  void
		*diagnostic;
			/* A pointer to a representation of a pending
			   diagnostic (presumably explaining why interpretation
			   failed to produce a constant result). */
  unsigned long	cost;
			/* An interpretation "cost" counter.  It counts the
			   number of calls and loop-back branches. */
  an_alloc_seq_number
		curr_alloc_seq_number;
			/* A sequence number counting the number of saved
			   storage stack states.  This is used to detect
			   dangling pointers. */
} an_interpreter_state;


#define cost_exceeded(ips)                                                   \
  (++(ips)->cost > 2000000)


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
  ips->diagnostic = NULL;
  ips->cost = 0;
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
#define push_call_frame(ips, p_frame, rp, p_result)                          \
  {                                                                          \
    (p_frame)->parent = (ips)->curr_call_frame;                              \
    (p_frame)->routine = (rp);                                               \
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
Find ptr in the overflow section of the given map and remove the associated
entry.
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
Structure describing the representation of an address in the interpreter.
*/
typedef struct a_constexpr_address {
  a_byte
		*address;
			/* The address in interpreter storage of the thing
			   pointed to, or NULL if is_runtime_data_address or
			   is_function_address are TRUE. */
#if 0
/* Not needed yet: eliminate to placate lint. */
  a_bit_field
		in_array:1;
			/* TRUE if this is a pointer to an array element
			   stored in interpreter storage. */
  a_bit_field
		is_function_address:1;
			/* TRUE if this is the address of a function. */
#endif /* 0 */
  a_bit_field
		is_runtime_data_address:1;
			/* TRUE if this is a data pointer that is constant at
			   run time, but not a pointer into interpreter
			   storage (i.e., a pointer to a static-duration
			   variable of some kind). */
#if 0
/* Not needed yet: eliminate to placate lint. */
  a_bit_field
		cannot_dereference:1;
			/* TRUE if this address cannot be dereferenced. */
  unsigned int
		length: 24;
			/* If in_array is TRUE, the number of
			   elements in the array. */
#endif /* 0 */  
  an_alloc_seq_number
		alloc_seq_number;
			/* The allocation sequence number of the storage
			   pointed to. */
  union {
#if 0    
    /* When in_array is TRUE: */
    a_byte
		*base_address;
			/* For an array element, the address of element #0. */
    /* When is_function_address is TRUE: */
    a_routine_ptr
		routine;
    			/* For addresses of functions. */
#endif /* 0 */    
    /* When is_runtime_data_address is TRUE: */
    a_constant_ptr
		addr_con;
			/* For constant addresses of run-time objects.  This
			   will always point to a constant acquired from
			   local_constant() and must be released when the
			   lvalue_to_rvalue conversion is applied or when
			   the expression is discarded. */
  } variant;
} a_constexpr_address;


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

#if 0
/* Not needed yet: eliminate to placate lint. */
/*
Convenience macro to get a pointer to the float value addressed by the
a_constexpr_address addr.
*/
#define float_value_at(addr) ((an_internal_float_value *)value_bytes_at(addr))
#endif /* 0 */

/*
Macro to initialize a constant address at addr referring to the interpreter
value at targ_addr.
*/
#define clear_address(addr, targ_addr)                    \
  memzero((char *)(addr), sizeof(a_constexpr_address));   \
  ((a_constexpr_address *)(addr))->address = (targ_addr);

#if 0
/* Not needed yet: eliminate so lint won't complain. */
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


/*
Macro to initialize a constant address at addr referring to the function
denoted by the IL a_routine entry rout.
*/
#define clear_function_address(addr, rout)                     \
  memzero((char *)(addr), sizeof(a_constexpr_address));        \
  ((a_constexpr_address *)(addr))->is_function_address = TRUE; \
  ((a_constexpr_address *)(addr))->variant.routine = (rout);
#endif /* 0 */


/*
Macro to initialize a constant address at addr referring to the
(non-interpreter) constant address described by the ck_address constant con.
*/
#define clear_runtime_constant_address(addr, con)                  \
  memzero((char *)(addr), sizeof(a_constexpr_address));            \
  ((a_constexpr_address *)(addr))->is_runtime_data_address = TRUE; \
  ((a_constexpr_address *)(addr))->variant.addr_con = (con);


typedef struct a_constexpr_ptr_to_mem_function {
  a_routine_ptr	member_function;
			/* The member function referred to. */
  a_byte_count
		this_class_adjustment;
			/* The adjustment needed to the "this" pointer. */
} a_constexpr_ptr_to_mem_function;


/*FIXME: delete when fields are used*/
/*lint -esym(754,a_constexpr_address::address)*/
/*lint -esym(754,a_constexpr_ptr_to_mem_function::member_function)*/
/*lint -esym(754,a_constexpr_ptr_to_mem_function::this_class_adjustment)*/



/*
Macro defining the largest allowed size of a type in the interpreter.
*/
#define MAX_CONSTEXPR_TYPE_SIZE ((a_byte_count)(1<<20))

#if /*FIXME: delete?*/0
/*
Macro producing TRUE if the given tk_pointer type is a pointer or reference to
a function type.
*/
#define ptr_or_ref_is_to_function(tp)                                        \
  (skip_typerefs(tp->variant.pointer.type)->kind == (a_type_kind)tk_routine)
#endif /*FIXME: delete?*/


/*
Macro producing TRUE if the given tk_ptr_to_member type is a pointer-to-member-
function type.
*/
#define ptr_to_mem_is_to_function(tp)                                        \
  (skip_typerefs(tp->variant.ptr_to_member.type)->kind ==                    \
                                                    (a_type_kind)tk_routine)

/*
Macro returning the number of bytes needed to represent a value of a given type
in interpreter storage.  In the case of a class type, the layout is computed if
needed.  Array or class types that are too large trigger an interpretation
failure (*ips is updated accordingly).
*/
#define value_bytes_for_type(ips, tp)                                        \
  ((tp)->kind == (a_type_kind)tk_integer ?                                   \
       sizeof(an_integer_value) :                                            \
   (tp)->kind == (a_type_kind)tk_float ?                                     \
       sizeof(an_internal_float_value) :                                     \
   /* else */                                                                \
    f_value_bytes_for_type((ips), (tp)))

/*
Macro returning the larger of two values.
*/
#define max(a, b) (((a) > (b))/*lint --e(506)*/ ? (a) : (b))


/*
Macro giving the number of bytes required for a scalar value.
*/
#define VALUE_BYTES_FOR_SCALAR                  \
  max(max(max(sizeof(an_integer_value),         \
              sizeof(an_internal_float_value)), \
          sizeof(a_constexpr_address)),         \
      sizeof(a_constexpr_ptr_to_mem_function))

static a_byte_count lay_out_class_type(an_interpreter_state  *ips,
                                       a_type_ptr  tp);
static a_byte_count lay_out_union_type(an_interpreter_state  *ips,
                                       a_type_ptr  tp);


/*ARGSUSED*/  /*FIXME:delete once errors are recorded. */
static a_byte_count f_value_bytes_for_type(an_interpreter_state  *ips,
                                           a_type_ptr            tp)
/*
Return the number of bytes needed to represent a value of the given type.
Always called through the macro value_bytes_for_type, which handles some common
type kinds.  If the number of bytes is too large, interpretation fails.
*/
{
  a_byte_count  result;

redo:
  switch (tp->kind) {
    case tk_integer:
      result = sizeof(an_integer_value);
      break;
    case tk_float:
      result = sizeof(an_internal_float_value);
      break;
    case tk_pointer:
      result = sizeof(a_constexpr_address);
      break;
    case tk_array:
      {
        a_targ_size_t  n_elems = num_array_elements(tp);
        a_type_ptr     etp = underlying_array_element_type(tp);
        result = value_bytes_for_type(ips, etp);
        etp = skip_typerefs(etp);
        if (MAX_CONSTEXPR_TYPE_SIZE/result < n_elems) {
          /* Too many elements. */
          /* FIXME: Interpretation failure. */
          unexpected_condition();
        } else {
          result *= (a_byte_count)n_elems;
        }  /* if */
      }
      break;
    case tk_class:
    case tk_struct:
      get_mapped_byte_count(&persistent_map, tp, result);
      if (result == 0) {
        result = lay_out_class_type(ips, tp);
      }  /* if */
      break;
    case tk_union:
      get_mapped_byte_count(&persistent_map, tp, result);
      if (result == 0) {
        result = lay_out_union_type(ips, tp);
      }  /* if */
      break;
    case tk_typeref:
      tp = tp->variant.typeref.type;
      goto redo;
    case tk_ptr_to_member:
      if (ptr_to_mem_is_to_function(tp)) {
        result = sizeof(a_constexpr_ptr_to_mem_function);
      } else {
        /* Pointer-to-data-member objects are represented as an "offset+1"
           value. */
        result = sizeof(a_byte_count);
      }  /* if */
      break;
    case tk_nullptr:
      result = 1;  /* FIXME? */
      break;
    case tk_error:
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:       /* All fixed-point types. */
#endif /* FIXED_POINT_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
    case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      /* FIXME: Interpretation failure. */
    case tk_void:
    case tk_routine:
    case tk_template_param:
    case tk_unknown:
    default:
      /* The GNU optimizer complains if result is not assigned a value on
         this branch. */
      result = (a_byte_count)tp->size;
      unexpected_condition();
  }  /* switch */
  return result;
}  /* f_value_bytes_for_type */


static a_byte_count lay_out_class_type(an_interpreter_state  *ips,
                                       a_type_ptr            tp)
/*
Compute and return the size of the given non-union class type.  Also record
offsets in any associated fields as well as any direct or virtual base classes.
If needed, this will recursively lay out types this class type is composed of.
ips is used to record an interpretation failure if the size exceeds the
interpreter's limits.
*/
{
  a_byte_count      total_size = 0;
  a_field_ptr       fp;
  a_base_class_ptr  bcp, bases = base_classes_of(tp);
  a_boolean         any_virtual_bases =
                      tp->variant.class_struct_union.any_virtual_base_classes;

  if (tp->variant.class_struct_union
                 .any_virtual_functions_including_in_base_classes ||
      any_virtual_bases) {
    /* Allocate storage to indicate which base in a hierarchy it is (NULL if
       it is a complete object). */
    total_size += sizeof(a_base_class_ptr);
  }  /* if */
  /* Allocate each proper field and record the field offsets. */
  fp = tp->variant.class_struct_union.field_list;
  for (; fp != NULL; fp = fp->next) {
    do_host_alignment(total_size);
    map_byte_count(&persistent_map, fp, total_size);
    total_size += value_bytes_for_type(ips, fp->type);
    if (total_size >= MAX_CONSTEXPR_TYPE_SIZE) {
      /* FIXME: error & saturate. */
      goto done;
    }  /* if */
  }  /* for */
  /* Allocate each direct, nonvirtual base class. */
  for (bcp = bases; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct && !bcp->is_virtual) {
      do_host_alignment(total_size);
      map_byte_count(&persistent_map, bcp, total_size);
      total_size += value_bytes_for_type(ips, bcp->type);
      if (total_size >= MAX_CONSTEXPR_TYPE_SIZE) {
        /* FIXME: error & saturate. */
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
        total_size += value_bytes_for_type(ips, bcp->type);
        if (total_size >= MAX_CONSTEXPR_TYPE_SIZE) {
          /* FIXME: error & saturate. */
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
                                       a_type_ptr            tp)
/*
Return the size that should be allocated for the given union type, and record
the offsets of its fields.  If needed, this will recursively lay out the types
of the fields.  ips is used to record an interpretation failure if the size
exceeds the interpreter's limits.
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
    a_byte_count  field_size = value_bytes_for_type(ips, fp->type);
    map_byte_count(&persistent_map, fp, prefix_size);
    if (field_size > max_field_size) max_field_size = field_size;
  }  /* for */
  total_size = prefix_size+max_field_size;
  if (total_size >= MAX_CONSTEXPR_TYPE_SIZE) {
    /* FIXME: error & saturate. */
    unexpected_condition();
  }  /* if */
  map_byte_count(&persistent_map, tp, total_size);
  return total_size;
}  /* lay_out_union_type */



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
  }  /* while */
}  /* db_call_stack */

#endif /* DEBUG */


/*
Macro to release a local constant captured by a glvalue or pointer
expression (i.e., an a_constexpr_address value).  expr is the expression
corresponding to the interpreter storage at value and tp is the type of
expr after applying skip_typerefs.
*/
#define release_local_constant_from_address(expr, tp, value)                  \
{                                                                             \
  if (((expr)->is_lvalue || (expr)->is_xvalue ||                              \
       (tp)->kind == (a_type_kind)tk_pointer) &&                              \
      ((a_constexpr_address *)(value))->is_runtime_data_address) {            \
    release_local_constant(&((a_constexpr_address *)(value))->                \
                                                           variant.addr_con); \
  }  /* if */                                                                 \
}  /* release_local_constant_from_address */


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


static a_boolean extract_value_from_constant(a_constant_ptr        con,
                                             a_byte                *value)
/*
Copy the value of con into the interpreter storage at value, converting
formats as necessary.  Return FALSE if the constant is an error constant.
*/
{
  a_boolean  result = TRUE;

  switch (con->kind) {
    case ck_error:
      result = FALSE;
      break;
    case ck_integer:
      *(an_integer_value *)value = con->variant.integer_value;
      break;
    case ck_float:
      *(an_internal_float_value *)value = con->variant.float_value;
      break;
    case ck_address:
      {
        /* Create an a_constexpr_address for the runtime constant, which
           requires a local constant. */
        a_constant_ptr addr_con = local_constant();
        copy_constant(con, addr_con);
        clear_runtime_constant_address(value, addr_con);
      }
      break;
    default:
      unexpected_condition();  /* FIXME: handle more kinds of constants. */
  }  /* switch */
  return result;
}  /* extract_value_from_constant */


/*
Macro to set result_storage from the value of the specified constant.
Duplicates some cases from extract_value_from_constant for performance
reasons.
*/
#define copy_val_from_constant(con, result_storage)                           \
  (                                                                           \
    ((con)->kind == (a_constant_repr_kind)ck_integer) ?                       \
      (*(an_integer_value *)(result_storage) = (con)->variant.integer_value,  \
       TRUE):                                                                 \
    ((con)->kind == (a_constant_repr_kind)ck_float) ?                         \
      ((*(an_internal_float_value *)(result_storage) =                        \
                                  (con)->variant.float_value), TRUE) :        \
    /* else */                                                                \
      extract_value_from_constant(con, result_storage)                        \
  )  /* copy_val_from_constant */


/*
Macro to interpret a full-expression.
*/
#define do_constexpr_full_expression(ips, expr, result_storage, result_flag)  \
  {                                                                           \
    a_storage_stack_state  saved_stack_for_full_expr;                         \
    save_storage_stack((ips), saved_stack_for_full_expr);                     \
    (result_flag) = do_constexpr_expression((ips), (expr), (result_storage)); \
    restore_storage_stack(ips, saved_stack_for_full_expr);                    \
  }

static a_boolean do_constexpr_expression(
                                       an_interpreter_state  *ips,
                                       an_expr_node_ptr      expr,
                                       a_byte                *result_storage);


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
    /* Allocate storage for variables, and map the variables to that
       storage.  Don't do this for parameter variables since they're
       already allocated and mapped. */
    a_variable_ptr  vp = scope->nonstatic_variables;
    if (vp != NULL) {
      save_storage_stack(ips, saved_stack);
      local_storage = TRUE;
      do {
        a_byte_count  n_bytes = value_bytes_for_type(ips, vp->type);
        a_byte        *var_storage;
        alloc_stack_bytes(ips, n_bytes, var_storage);
        /* Associate with the variable its value storage. */
        map_stack_bytes(ips, vp, var_storage);
        /* Also associate with the variable (somewhat arbitrarily, with its
           "storage_class" field) an allocation sequence number that may be
           used to detect leaks. */
        map_byte_count(&ips->map, &vp->storage_class,
                       ips->curr_alloc_seq_number);
        vp = vp->next;
      } while (vp != NULL);
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
    do {
      unmap_stack_bytes(ips, vp);
      unmap_ptr(&ips->map, &vp->storage_class);
      vp = vp->next;
    } while (vp != NULL);
    restore_storage_stack(ips, saved_stack);
  }  /* if */
  return result;
}  /* do_constexpr_block_statement */


static a_boolean do_constexpr_statement(an_interpreter_state  *ips,
                                        a_statement_ptr       stmt)
/*
Interpret the given statement.  Return TRUE if the statement was
successfully interpreted, FALSE otherwise.
*/
{
  a_boolean             result = TRUE;
  an_expr_node_ptr      expr;
  a_byte                expr_bytes[VALUE_BYTES_FOR_SCALAR];
  a_byte                *expr_value;
  a_storage_stack_state saved_stack;
  a_host_large_integer  bool_val;
  a_type_ptr            tp;
  a_boolean             ovfl;
  a_byte_count          n_bytes;

  switch (stmt->kind) {
    case stmk_expr:
      {
        expr = stmt->expr;
        tp = skip_typerefs(expr->type);
        n_bytes = value_bytes_for_type(ips, tp);
        save_storage_stack(ips, saved_stack);
        if (n_bytes > VALUE_BYTES_FOR_SCALAR &&
            !expr->is_lvalue && !expr->is_xvalue) {
          /* The value is larger than a scalar type, so allocate space for
             it on the stack. */
          alloc_stack_bytes(ips, n_bytes, expr_value);
        } else {
          expr_value = expr_bytes;
        }  /* if */
        result = do_constexpr_expression(ips, expr, expr_value);
        release_local_constant_from_address(expr, tp, expr_value);
        restore_storage_stack(ips, saved_stack);
      }
      break;
    case stmk_if:
      {
        /* The type of the test expression is known to be bool, which will
           fit within the expr_bytes array. */
        expr = stmt->expr;
        expr_value = expr_bytes;
        tp = skip_typerefs(expr->type);
        do_constexpr_full_expression(ips, expr, expr_value, result);
        release_local_constant_from_address(expr, tp, expr_value);
        if (result) {
          /* Evaluation of the test expression succeeded.  Get its value to
             see which dependent statement should be executed. */
          get_int_val_from(expr_value, tp, bool_val, ovfl);
          if (!ovfl && bool_val) {
            /* Execute the "then" statement. */
            result = do_constexpr_statement(
                                   ips, stmt->variant.if_stmt.then_statement);
          } else {
            result = do_constexpr_statement(
                                   ips, stmt->variant.if_stmt.else_statement);
          }  /* if */
        }  /* if */
      }
      break;
    case stmk_goto:
      if (stmt->variant.label.ptr->break_label) {
        ips->curr_call_frame->break_active = TRUE;
      } else if (stmt->variant.label.ptr->continue_label) {
        ips->curr_call_frame->continue_active = TRUE;
      } else {
        /* FIXME: record interpretation error. */
        result = FALSE;
      }  /* if */
      break;
    case stmk_label:
      /* Nothing to do. */
      break;
    case stmk_return:
      if (stmt->expr != NULL) {
        do_constexpr_full_expression(ips, stmt->expr,
                                     ips->curr_call_frame->result_storage,
                                     result);
      } else {
        /* Handle return_dynamic_init case. FIXME */
        unexpected_condition();
      }  /* if */
      ips->curr_call_frame->return_active = TRUE;
      break;
    case stmk_block:
      { a_block_ptr  block = stmt->variant.block.extra_info;
        result = do_constexpr_block_statement(ips, stmt, block->assoc_scope);
      }
      break;
    case stmk_for:
      {
        a_byte           incr_bytes[VALUE_BYTES_FOR_SCALAR];
        a_byte           *incr_value;
        an_expr_node_ptr incr;
        a_type_ptr       incr_type;
        expr = stmt->expr;
        if (expr != NULL) {
          /* The type of the test expression is known to be bool, which will
             fit within the expr_bytes array. */
          expr_value = expr_bytes;
          tp = skip_typerefs(expr->type);
        }  /* if */
        incr = stmt->variant.for_loop.extra_info->increment;
        if (incr != NULL) {
          incr_type = skip_typerefs(incr->type);
          n_bytes = value_bytes_for_type(ips, incr_type);
          save_storage_stack(ips, saved_stack);
          if (n_bytes > VALUE_BYTES_FOR_SCALAR &&
              !incr->is_lvalue && !incr->is_xvalue) {
            /* The result of the increment expression is larger than a scalar
               type, so allocate space for it on the stack. */
            alloc_stack_bytes(ips, n_bytes, incr_value);
          } else {
            incr_value = incr_bytes;
          }  /* if */
        }  /* if */
        /* Initialization is handled by an stmk_init in the containing block
           and not as part of the stmk_for processing. */
        do {
          /* Evaluate the test expression. */
          if (cost_exceeded(ips)) {
            result = FALSE;
            /* FIXME: record a diagnostic. */
          } else if (expr != NULL) {
            do_constexpr_full_expression(ips, expr, expr_value, result);
            release_local_constant_from_address(expr, tp, expr_value);
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
                                             ips,
                                             stmt->variant.for_loop.statement);
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
                  release_local_constant_from_address(incr, incr_type,
                                                      incr_value);
                }  /* if */
              }  /* if */
            }  /* if */
          }   /* if */
        } while (result && bool_val);
        if (incr != NULL) {
          restore_storage_stack(ips, saved_stack);
        }  /* if */
      }
      break;
    case stmk_init:
      { a_dynamic_init_ptr  dip = stmt->variant.dynamic_init;
        a_variable_ptr      vp = dip->variable;
        a_byte              *var_storage;
        get_stack_bytes(ips, vp, var_storage);
        if (var_storage != NULL) {
          /* Evaluate the initializer. */
          switch (dip->kind) {
            case dik_constant:
              result = copy_val_from_constant(dip->variant.constant,
                                              var_storage);
              break;
            case dik_expression:
              do_constexpr_full_expression(ips, dip->variant.expression,
                                           var_storage, result);
              break;
            case dik_class_result_via_ctor:
            case dik_constructor:
            case dik_nonconstant_aggregate:
            case dik_bitwise_copy:
              /* FIXME: NYI. */
              unexpected_condition();
              break;
            case dik_zero:
            case dik_none:
            default:
              unexpected_condition();
          }  /* switch */
        }  /* if */
      }
      break;
    case stmk_decl:
      /* Nothing to do; variables are handled when the scope is opened. */
      break;
    case stmk_empty:
      /* Nothing to do. */
      break;
    default:
      unexpected_condition();  /* FIXME: handle errors. */
  }  /* switch */
  return result;
}  /* do_constexpr_statement */


static a_boolean do_constexpr_call(an_interpreter_state  *ips,
                                   an_expr_node_ptr      call_node,
                                   a_byte                *result_storage)
/*
Interpret the given call node and place the result at the given storage.
Return TRUE if no error occurred; otherwise, return FALSE and update *ips
accordingly.
*/
{
  an_expr_node_ptr  callee_node, arg_nodes, routine_node, arg;
  a_routine_ptr     callee;
  a_memory_region_number
                    callee_region;
  a_boolean         result = TRUE;

  callee_node = call_node->variant.operation.operands;
  arg_nodes = callee_node->next;
  /* FIXME: eok_dot_static case may not be handled correctly by the following
     call. */
  callee = routine_and_node_from_function_expr(callee_node, &routine_node);
  if (callee == NULL) {
    /* FIXME: Interpret callee_node to get the callee. */
    unexpected_condition();
  }  /* if */
  /* Retrieve the routine scope, or issue an error. */
  callee_region = callee->assoc_scope;
  if (callee_region == NULL_region_number) {
    /* FIXME: error. */
    result = FALSE;
#if /*FIXME*/0
  } else if (ellipsis_case) {
    /* error. */
    result = FALSE;
#endif /* 0 */
  } else if (cost_exceeded(ips)) {
    /* FIXME: record an error. */
    result = FALSE;
  } else {
    a_scope_ptr     callee_scope = il_header.region_scope_entry[callee_region];
    a_storage_stack_state
                    saved_stack;
    a_statement_ptr
                    block_stmt = callee_scope->assoc_block;
    a_call_frame    frame;
    a_variable_ptr  param = callee_scope->variant.routine.parameters;
    /* Don't attempt to interpret a non-constexpr function.  The flag
       scope->is_constexpr_routine is set at the end of a constexpr function
       definition, so this also prevents the interpretation of a function that
       is not fully parsed (e.g., requested due to a recursive call in a
       constexpr function). */
    if (!callee_scope->is_constexpr_routine) {
      result = FALSE;
      /* FIXME: record an error. */
      goto done;
    }  /* if */
    /* Set up arguments, including "this" if applicable. */
    /* FIXME: handle "this". */
    save_storage_stack(ips, saved_stack);
    for (arg = arg_nodes; arg != NULL; arg = arg->next, param = param->next) {
      a_type_ptr    tp = skip_typerefs(arg->type);
      a_byte_count  n_bytes = value_bytes_for_type(ips, tp);
      a_byte        *arg_bytes;
      alloc_stack_bytes(ips, n_bytes, arg_bytes);
      if (!do_constexpr_expression(ips, arg, arg_bytes)) {
        /* Undo the mappings so far. */
        a_variable_ptr  up = callee_scope->variant.routine.parameters;
        for (; up != param; up = up->next) {
          unmap_stack_bytes(ips, up);
          unmap_ptr(&ips->map, &up->storage_class);
        }  /* for */
        result = FALSE;
        goto reclaim_arg_storage;
      }  /* if */
/* FIXME!  This mapping must be done after all the arguments have been
   evaluated because other arguments might refer to the same parameters in
   recursive calls. */
      map_stack_bytes(ips, param, arg_bytes);
      map_byte_count(&ips->map, &param->storage_class,
                     ips->curr_alloc_seq_number);
    }  /* for */
    /* Set up the call frame. */
    push_call_frame(ips, &frame, callee, result_storage);
    /* Run the function's top-level block statement. */
    if (block_stmt->kind != (a_statement_kind)stmk_block) {
      check_assertion(block_stmt->kind == (a_statement_kind)stmk_try_block);
      /* FIXME: should be an ordinary failure */
      unexpected_condition();
    } else {
      result = do_constexpr_block_statement(ips, block_stmt, callee_scope);
    }  /* if */
    pop_call_frame(ips);
    /* Release the storage and mappings of the parameters. */
    param = callee_scope->variant.routine.parameters;
    for (; param != NULL; param = param->next) {
      unmap_stack_bytes(ips, param);
      unmap_ptr(&ips->map, &param->storage_class);
    }  /* for */
reclaim_arg_storage:
    restore_storage_stack(ips, saved_stack);
    ips->cost += 1;
  }  /* if */
done:
  return result;
}  /* do_constexpr_call */


static a_boolean get_value_from_address_constant(a_constant_ptr addr_con,
                                                 a_byte         *value)
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
    result = copy_val_from_constant(val_con, value);
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
  a_byte_count         n_bytes = value_bytes_for_type(ips, tp);
  an_integer_kind      int_kind;
  a_boolean            is_signed;
  a_host_large_integer host_int_val;
  a_constant_ptr       con;

  switch (expr->kind) {
    case enk_operation:
      {
        /* An operation node.  Lvalue-to-rvalue conversions and casts are
           assumed to have already been applied to the operands, as
           required by the semantics of the operation. */
        an_expr_node_ptr opnd1;
        a_type_ptr       opnd1_type;
        a_byte           opnd1_bytes[VALUE_BYTES_FOR_SCALAR];
        a_byte           *opnd1_value;
        an_expr_node_ptr opnd2;
        a_type_ptr       opnd2_type;
        a_byte           opnd2_bytes[VALUE_BYTES_FOR_SCALAR];
        a_byte           *opnd2_value;
        a_boolean        ovfl;
        a_byte_count     opnd_n_bytes;

        if (is_call_node(expr)) {
          /* Call nodes are handled separately.  FIXME: Maybe this should not
             be the case. */
          result = do_constexpr_call(ips, expr, result_storage);
          goto done;
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
    if (expr->is_lvalue || expr->is_xvalue) {                                 \
      /* Copy the address. */                                                 \
      *(a_constexpr_address *)result_storage = *(a_constexpr_address *)(opnd);\
    } else {                                                                  \
      /* Do the lvalue-to-rvalue conversion into the result. */               \
      if (((a_constexpr_address *)(opnd))->is_runtime_data_address) {         \
        if (!get_value_from_address_constant(                                 \
                   ((a_constexpr_address *)(opnd))->variant.addr_con,         \
                   result_storage)) {                                         \
          /* Not a compile-time constant value. */                            \
          result = FALSE;                                                     \
          /* FIXME: record a diagnostic. */                                   \
        }  /* if */                                                           \
        /* Release the local constant acquired when this a_constexpr_address  \
           was created. */                                                    \
        release_local_constant(&((a_constexpr_address *)(opnd))->             \
                                                           variant.addr_con); \
      } else if (!in_live_set(&ips->live_set,                                 \
                              ((a_constexpr_address *)(opnd))                 \
                                                      ->alloc_seq_number)) {  \
        result = FALSE;                                                       \
        /* FIXME: record a diagnostic. */                                     \
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
#define check_int_range(val, tp, result, ovfl)                                \
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
  } /* if */                                                                  \
}  /* check_int_range */
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
#define check_int_range(val, tp, result, ovfl)                     \
  ((result) = (!ovfl &&                                            \
         cmp_integer_values((an_integer_value *)(val), is_signed,  \
                            &max_integer_value_of_kind[int_kind],  \
                            is_signed) <= 0 &&                     \
         (!is_signed ||                                            \
          cmp_integer_values((an_integer_value *)(val), is_signed, \
                             &min_integer_value_of_kind[int_kind], \
                             is_signed) >= 0)))
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

        opnd1 = expr->variant.operation.operands;
        opnd2 = opnd1->next;
        opnd1_type = skip_typerefs(opnd1->type);
        opnd_n_bytes = value_bytes_for_type(ips, opnd1_type);
        if (opnd_n_bytes > VALUE_BYTES_FOR_SCALAR &&
            !opnd1->is_lvalue && !opnd1->is_xvalue) {
          /* The value is larger than a scalar type, so allocate
             space for it on the stack. */
          alloc_stack_bytes(ips, opnd_n_bytes, opnd1_value);
        } else {
          opnd1_value = opnd1_bytes;
        }  /* if */
        result = do_constexpr_expression(ips, opnd1, opnd1_value);
        if (result && opnd2 != NULL &&
            !node_operator_is(expr, eok_land) &&
            !node_operator_is(expr, eok_lor) &&
            !node_operator_is(expr, eok_question)) {
          /* Evaluate the second operand.  For short-circuiting operators,
             whether to evaluate the second operand will be decided below
             in the specific code for each such operator. */
          opnd2_type = skip_typerefs(opnd2->type);
          opnd_n_bytes = value_bytes_for_type(ips, opnd2_type);
          if (opnd_n_bytes > VALUE_BYTES_FOR_SCALAR &&
              !opnd2->is_lvalue && !opnd2->is_xvalue) {
            /* The value may be larger than a scalar type, so allocate
               space for it on the stack. */
            alloc_stack_bytes(ips, opnd_n_bytes, opnd2_value);
          } else {
            opnd2_value = opnd2_bytes;
          }  /* if */
          result = do_constexpr_expression(ips, opnd2, opnd2_value);
        } else {
          opnd2_value = opnd2_bytes;
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
                  /* Integers: make sure the value fits in the target. */
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  ovfl = FALSE;
                  check_int_range(opnd1_value, tp, result, ovfl);
                  if (result) {
                    *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
                  } else {
                    /* FIXME: record an overflow diagnostic. */
                  }  /* if */
                } else {
                  unexpected_condition(); /* FIXME: implement non-ints. */
                }  /* if */
              } else {
                unexpected_condition();  /* FIXME: implement conversions. */
              }  /* if */
              break;
            case eok_bool_cast:
              if (opnd1_type->kind == (a_type_kind)tk_integer) {
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
              } else {
                /* FIXME: NYI, other source types. */
              }  /* if */
              break;
            case eok_pre_incr:
              if (((a_constexpr_address *)opnd1_value)->
                                                     is_runtime_data_address) {
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
                  int_kind = tp->variant.integer.int_kind;
                  is_signed = int_kind_is_signed[int_kind];
                  add_integer_values(int_value_at(opnd1_value), &one_int,
                                     is_signed, &ovfl);
                  check_int_range(int_value_at(opnd1_value), tp, result, ovfl);
                  if (!result) {
                    /* FIXME: record a diagnostic. */
                  }  /* if */
                }  /* if */
              } else if (tp->kind == (a_type_kind)tk_float) {
                /* FIXME: handle floating point value. */
                result = FALSE;
              } else if (tp->kind == (a_type_kind)tk_pointer) {
                /* FIXME: handle pointer value. */
                result = FALSE;
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
                check_int_range((an_integer_value*)(opnd1_value), tp, result,
                                ovfl);
                if (!result) {
                  /* FIXME: record a diagnostic. */
                }  /* if */
              } else {
                /* FIXME: Other type kinds NYI. */
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
                check_int_range((an_integer_value*)(opnd1_value), tp, result,
                                ovfl);
                if (!result) {
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
                check_int_range((an_integer_value*)(opnd1_value), tp, result,
                                ovfl);
                if (!result) {
                  /* FIXME: record a diagnostic. */
                }  /* if */
              } else {
                /* FIXME: Other type kinds NYI. */
              }  /* if */
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
                result = FALSE;
              }  /* if */
              if (result) {
                shift_left_integer_value((an_integer_value *)opnd1_value,
                                         (int)host_int_val, &ovfl);
                check_int_range(opnd1_value, opnd1_type, result, ovfl);
                if (result) {
                  *(an_integer_value *)result_storage =
                                              *(an_integer_value *)opnd1_value;
                } else {
                  /* FIXME: record a diagnostic for invalid result. */
                }  /* if */
              } else {
                /* FIXME: record a diagnostic for invalid opnd2. */
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
                /* FIXME: handle floating point value. */
                result = FALSE;
              } else if (opnd1_type->kind == (a_type_kind)tk_pointer) {
                /* FIXME: handle pointer value. */
                result = FALSE;
              } else {
                unexpected_condition();
              }  /* if */
              break;
            case eok_assign:
              if (((a_constexpr_address *)opnd1_value)->
                                                     is_runtime_data_address) {
                /* Cannot modify the value of an object whose lifetime began
                   outside the current evaluation. */
                /* FIXME: record a diagnostic. */
                result = FALSE;
              } else {
                /* Copy the value of the right operand to the indicated
                   address and return either the address or the value, as
                   appropriate. */
                (void)memcpy(value_bytes_at(opnd1_value), opnd2_value,
                             size_t_arg(n_bytes));
                set_result_val_from_operand_address(opnd1_value);
              }  /* if */
              break;
            case eok_call:
              result = do_constexpr_call(ips, expr, result_storage);
              break;
            default:
              unexpected_condition();  /* FIXME: handle errors. */
          }  /* switch */
        }  /* if */
      }
      break;
    case enk_constant:
      result = copy_val_from_constant(expr->variant.constant, result_storage);
      break;
    case enk_variable:
      {
        a_variable_ptr  var = expr->variant.variable;
        a_byte          *var_bytes;
        get_stack_bytes(ips, var, var_bytes);
        if (!expr->is_lvalue && !expr->is_xvalue) {
          /* A variable used as an rvalue; the result is its associated
             value bytes. */
          if (var_bytes != NULL) {
            /* This is a variable on the interpreter stack. */
            (void)memcpy(result_storage, var_bytes, size_t_arg(n_bytes));
          } else {
            con = var_constant_value(var);
            if (con != NULL) {
              result = copy_val_from_constant(con, result_storage);
            } else {
              /* FIXME: record a diagnostic. */
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
            con = local_constant();
            if (constant_glvalue_address(expr, con,
                                         /*address_escapes=*/FALSE)) {
              clear_runtime_constant_address(result_storage, con);
            } else {
              release_local_constant(&con);
              /* FIXME: record a diagnostic. */
              result = FALSE;
            }  /* if */
          }  /* if */
        }  /* if */
      }
      break;
    default:
      unexpected_condition();  /* FIXME: handle errors. */
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


a_boolean interpret_constexpr_call(an_expr_node_ptr      call_expr,
                                   a_constant_ptr        result_con)
/*
Attempt to interpret the call represented by call_expr.  Return TRUE if
successful, and produce the resulting the value in result_con.  Otherwise,
return FALSE.
*/
{
  a_boolean             result = FALSE;
  an_interpreter_state  ips;
  a_byte                result_bytes[VALUE_BYTES_FOR_SCALAR];
  a_byte                *result_storage;
  a_type_ptr            result_type = skip_typerefs(call_expr->type);

  if (trans_unit_initialization_needed) {
    initialize_interpreter_data();
    trans_unit_initialization_needed = FALSE;
  }  /* if */
  init_interpreter_state(&ips);
  if (result_type->kind == (a_type_kind)tk_integer) {
    clear_constant(result_con, (a_constant_repr_kind)ck_integer);
    result_storage = (a_byte*)&result_con->variant.integer_value;
  } else if (result_type->kind == (a_type_kind)tk_float) {
    clear_constant(result_con, (a_constant_repr_kind)ck_float);
    result_storage = (a_byte*)&result_con->variant.float_value;
  } else if (result_type->kind == (a_type_kind)tk_pointer) {
    /* A pointer or reference.  The result will be the an
       a_constexpr_address, which will be further handled after
       interpretation is finished. */
    result_storage = result_bytes;
  } else {
    /* FIXME: Handle other type kinds */
    result_storage = NULL;
    unexpected_condition();
  }  /* if */
  result_con->type = result_type;
  result = do_constexpr_call(&ips, call_expr, result_storage);
  if (result && result_type->kind == (a_type_kind)tk_pointer) {
    /* A constexpr function can return an address constant; if it returns
       an interpreter address, the invocation is non-constant. */
    a_constexpr_address *cap = (a_constexpr_address *)result_storage;
    if (cap->is_runtime_data_address) {
      /* Copy the address constant to result_con and release the local
         constant. */
      copy_constant(cap->variant.addr_con, result_con);
      release_local_constant(&cap->variant.addr_con);
    } else {
      /* The address designates an interpreter value, which will be a
         dangling pointer or reference and thus cannot be constant. */
      result = FALSE;
    }  /* if */
  }  /* if */
  release_interpreter_state(&ips);
  return result;
}  /* interpret_constexpr_call */


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
      pch_saved_var_array_elem(free_stack_blocks),
      pch_saved_var_array_elem(free_map_tables),
      pch_saved_var_array_elem(free_live_set_tables),
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
