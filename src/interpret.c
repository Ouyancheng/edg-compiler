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

/*
This file implements an interpreter for a subset of the unlowered IL produced
by the C++ front end.  Specifically, the subset corresponds to the constructs
allowed a constant expressions in C++14.


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
uses a storage stack, although the ability to efficiently deallocate is not
exploited in that case.  The macro alloc_bytes can be used for these
allocations.


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

FIXME: Add note about second "persistent" data map when it's introduced.
*/

typedef unsigned int a_byte_count;

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
} a_storage_stack_state;


typedef struct a_large_block_header {
  a_byte	*prev_large_block;
			/* Pointer to the previously allocated large block (or
			   NULL if none). */
  a_byte_count	block_size;
			/* Size of this block. */
} a_large_block_header;


/*
Type to use to index into the data map.
*/
typedef unsigned int a_map_index;

/*
Structure mapping pointers in the IL to associated data in the interpreter.
(E.g., to map a variable to its associated storage, or a type to its associated
layout data.)
*/
typedef struct a_data_map_entry {
  a_byte	*il_ptr;
			/* The pointer into the IL mapped by this entry.
			   (A "key" in the hash table.) */
  a_byte	*data_ptr;
			/* The pointer to associated data mapped by this entry.
			   (A "value" in the hash table.) */
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
  /* Prepend the new table of the "free tables" list. */
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
} a_call_frame;


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
			   number of calls, and loop-back branches. */
} an_interpreter_state;


#define cost_exceeded(ips)                                                   \
  ((ips)->cost > 1000000)


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
  ips->diagnostic = NULL;
  ips->cost = 0;
}  /* init_interpreter_state */


static void release_interpreter_state(an_interpreter_state  *ips)
/*
Release the storage allocate for the given interpreter state.
*/
{
  release_constexpr_stack(&ips->storage_stack);
  release_data_map_table(&ips->map);
  ips->map.table = NULL;
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
  ((state) = (ips)->storage_stack)

#define restore_storage_stack(ips, state)                                    \
  {                                                                          \
    a_byte  *curr_large_blocks = (ips)->storage_stack.large_blocks,          \
            *saved_large_blocks = (state).large_blocks;                      \
    while (curr_large_blocks != saved_large_blocks) {                        \
      a_byte  *large_block = curr_large_blocks;                              \
      curr_large_blocks = ((a_large_block_header*)large_block)               \
                                                      ->prev_large_block;    \
      free_general(large_block,                                              \
                   ((a_large_block_header*)large_block)->block_size);        \
    }  /* while */                                                           \
    (ips)->storage_stack = (state);                                          \
  }


/*
Macros to push and pop call frames.
*/
#define push_call_frame(ips, p_frame, rp, p_result)                          \
  {                                                                          \
    (p_frame)->parent = (ips)->curr_call_frame;                        \
    (p_frame)->routine = (rp);                                               \
    (p_frame)->result_storage = (p_result);                                  \
    (ips)->curr_call_frame = (p_frame);                                      \
  }

#define pop_call_frame(ips)                                                  \
  ((ips)->curr_call_frame = (ips)->curr_call_frame->parent)


#if HOST_ALIGNMENT_REQUIRED == 1
#define HASH_PTR_SHIFT 0
#else
#if HOST_ALIGNMENT_REQUIRED == 2
#define HASH_PTR_SHIFT 1
#else
#if HOST_ALIGNMENT_REQUIRED == 4
#define HASH_PTR_SHIFT 2
#else
#if HOST_ALIGNMENT_REQUIRED == 8
#define HASH_PTR_SHIFT 3
#else
#if HOST_ALIGNMENT_REQUIRED == 16
#define HASH_PTR_SHIFT 4
#else
#if HOST_ALIGNMENT_REQUIRED == 32
#define HASH_PTR_SHIFT 5
#else
#define HASH_PTR_SHIFT 6
#endif /* == 32 */
#endif /* == 16 */
#endif /* == 8 */
#endif /* == 4 */
#endif /* == 2 */
#endif /* == 1 */

#define hash_il_ptr(ptr)                                                     \
   (((uintptr_t)ptr >> HASH_PTR_SHIFT) % NUM_DATA_MAP_HASH_HEADERS)

static a_byte* find_overflow_entry(a_data_map    *map,
                                   a_byte        *il_ptr,
                                   a_byte_count  idx)
/*
Search map for an overflow entry mapping il_ptr, starting at the entry at the
given index.
*/
{
  a_byte            *result;
  a_data_map_entry  *table = map->table;

  for (;;) {
    if (table[idx].il_ptr == il_ptr) {
      /* We found the searched-for entry. */
      a_map_index  hash_idx = hash_il_ptr(il_ptr);
      result = table[idx].data_ptr;
      /* Make this the new principal entry (by swapping). */
      table[idx].il_ptr = table[hash_idx].il_ptr;
      table[idx].data_ptr = table[hash_idx].data_ptr;
      table[hash_idx].il_ptr = il_ptr;
      table[hash_idx].data_ptr = result;
      break;
    } else {
      idx = table[idx].next_index;
      if (idx == 0) {
        /* We've exhausted the list of entries. */
        result = NULL;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* find_overflow_entry */


/*
Macro to retrieve a pointer (dptr) associated with an pointer into the IL
(iptr) from a given data map.
*/
#define get_mapped_ptr(map, iptr, dptr)                                      \
  { a_byte_count  idx = hash_il_ptr((a_byte*)(iptr));                        \
    a_byte        *cached_ptr = (map)->table[idx].il_ptr;                    \
    if (cached_ptr == (a_byte*)(iptr)) {                                     \
      (dptr) = (map)->table[idx].data_ptr;                                   \
    } else {                                                                 \
      a_map_index  next_index = (map)->table[idx].next_index;                \
      if (next_index != 0) {                                                 \
        (dptr) = find_overflow_entry((map), (a_byte*)(iptr), next_index);    \
      } else {                                                               \
        (dptr) = 0;                                                          \
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
Macro to add an entry to a data map.
*/
#define map_ptr(map, iptr, dptr)                                             \
  { a_byte_count      idx = hash_il_ptr(iptr);                               \
    a_data_map_entry  *table = (map)->table;                                 \
    a_byte            *cached_ptr = table[idx].il_ptr;                       \
    if (cached_ptr != NULL) {                                                \
      /* Move the existing entry to an overflow entry. */                    \
      a_map_index  new_index;                                                \
      if ((map)->next_free == 0) {                                           \
        expand_map(map);                                                     \
      }  /* if */                                                            \
      new_index = (map)->next_free;                                          \
      table[new_index].il_ptr = cached_ptr;                                  \
      table[new_index].data_ptr = table[idx].data_ptr;                       \
      table[new_index].next_index = table[idx].next_index;                   \
      table[idx].next_index = new_index;                                     \
    }  /* if */                                                              \
    table[idx].il_ptr = (a_byte*)(iptr);                                     \
    table[idx].data_ptr = (dptr);                                            \
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
                                 a_byte       *il_ptr,
                                 a_map_index  idx)
/*
Find il_ptr in the overflow section of the given map and remove the associated
entry.
*/
{
  a_data_map_entry  *table = map->table;
  a_map_index       last_index = 0;

  for (;;) {
    if (table[idx].il_ptr == il_ptr) {
      /* We found the searched-for entry.  Unlink it. */
      if (last_index == 0) {
        last_index = hash_il_ptr(il_ptr);
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
    a_byte            *cached_ptr = table[idx].il_ptr;                       \
    if (cached_ptr == (a_byte*)(iptr)) {                                     \
      if (table[idx].next_index == 0) {                                      \
        /* Only one element in the bucket. */                                \
        table[idx].il_ptr = NULL;                                            \
      } else {                                                               \
        /* Move the first overflow entry to the main hash table. */          \
        a_map_index  old_index = table[idx].next_index;                      \
        table[idx].il_ptr = table[old_index].il_ptr;                         \
        table[idx].data_ptr = table[old_index].data_ptr;                     \
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


typedef struct a_constexpr_data_address {
  a_byte
		*address;
			/* The address in interpreter storage of the thing
			   pointed to, or NULL if is_runtime_constant is
			   TRUE. */
#if /*FIXME: enable when used*/0
  a_bit_field
		is_array:1;
			/* TRUE if this is a pointer to an
			   array element. */
  a_bit_field
		is_runtime_constant:1;
			/* TRUE if this is a pointer that is constant
			   at run time, but not a pointer into interpreter
			   storage.  Normally, a pointer to a static-duration
			   variable of some kind. */
  a_bit_field
		cannot_dereference:1;
			/* TRUE if this address cannot be dereferenced. */
  unsigned int
		length: 24;
			/* If is_array is TRUE, the number of
			   elements in the array. */
  union {
    a_byte
		*base_address;
			/* For an array, the address of element #0. */
    a_type_ptr
		complete_class;
			/* For a class type, the complete class type
			   to use for polymorphic dispatch. */
    a_constant_ptr
		runtime_constant;
			/* For constant addresses of run-time entities. */
  } variant;
#endif /*0*/
} a_constexpr_data_address;


typedef struct a_constexpr_ptr_to_mem_function {
  a_routine_ptr	member_function;
			/* The member function referred to. */
  a_byte_count
		this_class_adjustment;
			/* The adjustment needed to the "this" pointer. */
} a_constexpr_ptr_to_mem_function;


/*FIXME: delete when fields are used*/
/*lint -esym(754,a_constexpr_data_address::address)*/
/*lint -esym(754,a_constexpr_ptr_to_mem_function::member_function)*/
/*lint -esym(754,a_constexpr_ptr_to_mem_function::this_class_adjustment)*/



/*
Macro defining the largest allowed size of a type in the interpreter.
*/
#define MAX_CONSTEXPR_TYPE_SIZE ((a_byte_count)(1<<20))


/*
Macro producing TRUE if the given tk_pointer type is a pointer or reference to
a function type.
*/
#define ptr_or_ref_is_to_function(tp)                                        \
  (skip_typerefs(tp->variant.pointer.type)->kind == (a_type_kind)tk_routine)


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
       f_value_bytes_for_type(ips, tp))


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
      if (ptr_or_ref_is_to_function(tp)) {
        result = sizeof(a_routine_ptr);
      } else {
        result = sizeof(a_constexpr_data_address);
      }  /* if */
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
      /* FIXME */
      break;
    case tk_union:
      /* FIXME */
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
      unexpected_condition();
  }  /* switch */
  return 0;
}  /* f_value_bytes_for_type */

#if DEBUG

uintptr_t db_hash_ptr(void  *ptr)
/*
Debug routine to compute a hash value from within a debugger.
*/
{
  return hash_il_ptr(ptr);
}  /* db_hash_ptr */


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



static a_boolean do_constexpr_statement(an_interpreter_state  *ips,
                                        a_statement_ptr       stmt);

static a_boolean do_constexpr_expression(
                                       an_interpreter_state  *ips,
                                       an_expr_node_ptr      expr,
                                       a_byte                *result_storage);

/*
Macro to interpret a full-expression.
*/
#define do_constexpr_full_expression(ips, expr, result_storage, result_flag) \
  {                                                                          \
    a_storage_stack_state  saved_stack_for_full_expr;                        \
    save_storage_stack(ips, saved_stack_for_full_expr);                      \
    (result_flag) = do_constexpr_expression(ips, expr, result_storage);      \
    restore_storage_stack(ips, saved_stack_for_full_expr);                   \
  }


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

  if (scope != NULL) {
    /* Allocate storage for variables, and map the variables to that
       storage.  Don't do this for parameter variables since they're
       already allocated and mapped. */
    a_variable_ptr  vp = scope->nonstatic_variables;
    for (; vp != NULL; vp = vp->next) {
      a_byte_count  n_bytes = value_bytes_for_type(ips, vp->type);
      a_byte        *var_storage;
      if (!local_storage) {
        save_storage_stack(ips, saved_stack);
        local_storage = TRUE;
      }  /* if */
      alloc_stack_bytes(ips, n_bytes, var_storage);
      map_stack_bytes(ips, vp, var_storage);
    }  /* for */
  }  /* if */
  /* Interpret the statements in the block. */
  for (; stmt != NULL; stmt = stmt->next) {
    if (!do_constexpr_statement(ips, stmt)) {
      result = FALSE;
      break;
    }  /* if */
  }  /* for */
  /* Release and unmap the local storage if necessary. */
  if (local_storage) {
    a_variable_ptr  vp = scope->nonstatic_variables;
    for (; vp != NULL; vp = vp->next) {
      unmap_stack_bytes(ips, vp);
    }  /* for */
    restore_storage_stack(ips, saved_stack);
  }  /* if */
  return result;
}  /* do_constexpr_block_statement */


static a_boolean do_constexpr_statement(an_interpreter_state  *ips,
                                        a_statement_ptr       stmt)
/*
Interpret the given statement.  Return FALSE if no more statements should be
interpreted for the current call frame (either because of an error, or because
stmt is a return statement).  Otherwise, return TRUE.
*/
{
  a_boolean  result;

  switch (stmt->kind) {
    case stmk_block:
      { a_block_ptr  block = stmt->variant.block.extra_info;
        result = do_constexpr_block_statement(ips, stmt, block->assoc_scope);
      }
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
      result = FALSE;
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

  ips->cost += 1;
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
    unexpected_condition();
#if /*FIXME*/0
  } else if (ellipsis_case) {
    /* error. */
#endif /* 0 */
  } else if (cost_exceeded(ips)) {
    /* FIXME: error. */
    unexpected_condition();
  } else {
    a_scope_ptr     callee_scope = il_header.region_scope_entry[callee_region];
    a_storage_stack_state
                    saved_stack;
    a_statement_ptr
                    block_stmt = callee_scope->assoc_block;
    a_call_frame    frame;
    a_variable_ptr  param = callee_scope->variant.routine.parameters;
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
        for (; up != param; up = up->next) unmap_stack_bytes(ips, up);
        goto reclaim_arg_storage;
      }  /* if */
      map_stack_bytes(ips, param, arg_bytes);
    }  /* for */
    /* Set up the call frame. */
    push_call_frame(ips, &frame, callee, result_storage);
    /* Run the function's top-level block statement. */
    if (block_stmt->kind != (a_statement_kind)stmk_block) {
      check_assertion(block_stmt->kind == (a_statement_kind)stmk_try_block);
      /* FIXME: should be an ordinary failure */
      unexpected_condition();
    } else {
      (void)do_constexpr_block_statement(ips, block_stmt, callee_scope);
    }  /* if */
    pop_call_frame(ips);
    /* Release the storage and mappings of the parameters. */
    param = callee_scope->variant.routine.parameters;
    for (; param != NULL && param->is_parameter; param = param->next) {
      unmap_stack_bytes(ips, param);
    }  /* for */
reclaim_arg_storage:
    restore_storage_stack(ips, saved_stack);
  }  /* if */
  return ips->diagnostic == NULL;
}  /* do_constexpr_call */


static a_boolean do_constexpr_expression(an_interpreter_state  *ips,
                                         an_expr_node_ptr      expr,
                                         a_byte                *result_storage)
/*
Interpret the given expression in the given interpreter context.  If
successful return TRUE and store the result at *result_bytes.  Otherwise,
return FALSE and update the *ips accordingly.
*/
{
  a_boolean  result = TRUE;

  switch (expr->kind) {
    case enk_operation:
      {
        switch (expr->variant.operation.kind) {
          case eok_assign:
            /* FIXME */
            break;
          default:
            unexpected_condition();  /* FIXME: handle errors. */
        }  /* switch */
      }
      break;
    case enk_constant:
      { a_constant_ptr  con = expr->variant.constant;
        switch (con->kind) {
          case ck_integer:
            *(an_integer_value*)result_storage = con->variant.integer_value;
            break;
          default:
            unexpected_condition();  /* FIXME: handle errors. */
        }  /* switch */
      }
      break;
    case enk_variable:
      if (!expr->is_lvalue && !expr->is_xvalue) {
        /* A variable used as an rvalue: Copy its associated value bytes. */
        a_variable_ptr  var = expr->variant.variable;
        a_type_ptr      tp = skip_typerefs(expr->type);
        a_byte_count    n_bytes = value_bytes_for_type(ips, tp);
        a_byte          *var_bytes;
        get_stack_bytes(ips, var, var_bytes);
        if (var_bytes != NULL) {
          (void)memcpy(result_storage, var_bytes, size_t_arg(n_bytes));
        } else {
           /* FIXME: handle constant-valued variable that aren't mapped
              during interpretation (e.g., a namespace-scope constexpr
              variable). */
           unexpected_condition();
        }  /* if */
      } else {
        unexpected_condition();  /* FIXME: handle lvalue/xvalue. */
      }  /* if */
      break;
    default:
      unexpected_condition();  /* FIXME: handle errors. */
  }  /* switch */
  return result;
}  /* do_constexpr_expression */


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
  a_byte                *result_storage;
  a_type_ptr            result_type = skip_typerefs(call_expr->type);

  init_interpreter_state(&ips);
  if (result_type->kind == (a_type_kind)tk_integer) {
    clear_constant(result_con, (a_constant_repr_kind)ck_integer);
    result_storage = (a_byte*)&result_con->variant.integer_value;
  } else if (result_type->kind == (a_type_kind)tk_float) {
    clear_constant(result_con, (a_constant_repr_kind)ck_float);
    result_storage = (a_byte*)&result_con->variant.float_value;
  } else {
    /* FIXME: Handle other type kinds */
    unexpected_condition();
  }  /* if */
  result_con->type = result_type;
  result = do_constexpr_call(&ips, call_expr, result_storage);
  release_interpreter_state(&ips);
  return result;
}  /* interpret_constexpr_call */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
