/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

Throw processing for exception handling.

*/

#include <malloc.h>
#include "basics.h"
#include "config.h"
#include "eh.h"

/* Structure used to record information about blocks of memory handled
   by the EH memory management routines. */
typedef struct a_mem_block_descr *a_mem_block_descr_ptr;
typedef struct a_mem_block_descr {
  a_mem_block_descr_ptr
		next;
			/* The next stack entry. */
  void*		addr;
			/* Address of the block of memory. */
  a_sizeof_t	size;
			/* Size in bytes of the block of memory. */
  a_sizeof_t	used;
			/* Number of bytes used in the block. */
  a_byte_boolean
		dynamically_allocated;
			/* TRUE if the block of memory was dynamically
			   allocated.  The initial memory block is
			   statically allocated. */
} a_mem_block_descr;


/* Describes a single piece of memory allocated by the EH runtime. */
typedef struct a_mem_allocation *a_mem_allocation_ptr;
typedef struct a_mem_allocation {
  a_mem_allocation_ptr
		next;
			/* The next allocation entry. */
  a_sizeof_t	alloc_size;
			/* Size of the piece of memory.  This is the
			   allocated size including any space needed
			   for alignment not just the requested size. */
  void*		addr;
			/* Address of the memory allocated. */
  a_byte_boolean
		is_mem_block_descr_allocation;
			/* TRUE if this is a memory allocation done
			   to keep track of a memory block description
		           record. */
} a_mem_allocation;


static a_mem_block_descr_ptr
		curr_mem_block_descr = NULL;
			/* Pointer to the top of a stack of memory
			   blocks managed by the EH runtime. */

static a_mem_allocation_ptr
		mem_allocation_stack = NULL;
			/* Pointer to the top of a stack of memory
			   allocation entries. */

static a_mem_block_descr
		initial_mem_block_descr;
			/* Initial entry pointed to by the memory block
			   stack. */


static union {
  char		memory[EH_MEMORY_ALLOCATION_INCREMENT];
			/* The initial block of memory to be used.  This
			   avoids the need for the EH runtime to do
			   any dynamic memory allocation in most cases. */
  TYPE_WITH_MOST_STRICT_ALIGNMENT
		dummy;
			/* Used to ensure that the memory block is
			   guaranteed to be aligned on an appropriate
			   boundary. */
} initial_mem_block;

/* The number of bytes needed for a memory block description and any
   required alignment. */
#define NEEDED_FOR_MEM_BLOCK_DESCR \
  ((sizeof(a_mem_block_descr) < MOST_STRICT_ALIGNMENT) ?		\
     MOST_STRICT_ALIGNMENT :						\
     sizeof(a_mem_block_descr))


/* The number of bytes needed for a memory allocation structure and any
   required alignment. */
#define NEEDED_FOR_MEM_ALLOCATION_INFO \
  ((sizeof(a_mem_allocation) < MOST_STRICT_ALIGNMENT) ?			\
     MOST_STRICT_ALIGNMENT :						\
     sizeof(a_mem_allocation))

/* The number of bytes needed at the end of a memory block to record the
   information needed to allocate a new memory block. */
#define RESERVED_FOR_END_OF_MEM_BLOCK \
  (NEEDED_FOR_MEM_BLOCK_DESCR + NEEDED_FOR_MEM_ALLOCATION_INFO)


/* The number of bytes that must be added to the_ptr to obtain a value
   with suitable alignment.  the_ptr is actually an integer value that
   represents an offset from the base of a block of memory that is known
   to be appropriately aligned. */
#define increment_needed_for_alignment(the_ptr)				\
  (((the_ptr % MOST_STRICT_ALIGNMENT) == 0) ?	\
         0 :								\
         (MOST_STRICT_ALIGNMENT - (the_ptr % MOST_STRICT_ALIGNMENT)))


static void* eh_get_memory(a_sizeof_t	size)
/*
This is a low level routine that just gets a piece of dynamically
allocated memory from the system.   This must get the memory in
a means that will not result in an exception being thrown.
*/
{
  void*		mem_block;

  mem_block = malloc(size);
  /* If we can't get the memory we need, call the terminate routine. */
  if (mem_block == NULL) {
    __call_terminate();
  }  /* if */
  return mem_block;
}  /* eh_get_memory */


static void eh_free_memory(void* ptr)
/*
This is a low level routine that simply frees a piece of dynamically allocated
memory to the system.  This must free the memory in a means that will not
result in an exception being thrown.
*/
{
  free(ptr);
}  /* eh_free_memory */


static void mem_block_descr_init(a_mem_block_descr_ptr mbdp)
/*
Initialize the fields of a memory block description record.
*/
{
  mbdp->next = NULL;
  mbdp->addr = NULL;
  mbdp->size = 0;
  mbdp->used = 0;
  mbdp->dynamically_allocated = FALSE;
}  /* mem_block_descr_init */


static void init_eh_memory_management(void)
/*
Initialize the variables that keep track of memory used by the EH runtime.
*/
{
  /* Initialize the initial memory block description record. */
  mem_block_descr_init(&initial_mem_block_descr);
  initial_mem_block_descr.addr = initial_mem_block.memory;
  initial_mem_block_descr.size = EH_MEMORY_ALLOCATION_INCREMENT;
  initial_mem_block_descr.used = 0;
  initial_mem_block_descr.dynamically_allocated = FALSE;
  curr_mem_block_descr = &initial_mem_block_descr;
}  /* init_eh_memory_management */


/*
Return the address of the specified character position within the
current memory block.
*/
#define addr_in_mem_block(pos)						\
  (void *)((char *)curr_mem_block_descr->used)[pos]


static void* alloc_in_mem_block(a_sizeof_t	      size,
			        a_mem_allocation_ptr* map)
/*
Allocate a memory allocation record and the requested amount of space
in the current memory block.  There must be enough space for the allocation
to succeed and size must be a multiple of MOST_STRICT_ALIGNMENT.
*/
{
  void*			ptr;
  int			used;

  /* Get space from the memory block to store a new memory block description
     an a memory allocation record to describe it. */
  used = curr_mem_block_descr->used;
  *map = (a_mem_allocation_ptr)addr_in_mem_block(used);
  used += NEEDED_FOR_MEM_ALLOCATION_INFO;
  ptr = (void*)addr_in_mem_block(used);
  used += size;
  curr_mem_block_descr->used = used;
  /* Add this memory allocation record to the top of the stack. */
  (*map)->next = mem_allocation_stack;
  (*map)->addr = ptr;
  mem_allocation_stack = *map;
  /* Initialize the other fields of the memory allocation record. */
  (*map)->alloc_size = size;
  (*map)->is_mem_block_descr_allocation = FALSE;
  check_assertion(curr_mem_block_descr->used <= curr_mem_block_descr->used);
  check_assertion(size % MOST_STRICT_ALIGNMENT == 0);
  return ptr;
}  /* alloc_in_mem_block */


static void alloc_new_mem_block(a_sizeof_t	size)
/*
Allocate a new memory block of at least "size" bytes.  Actually,
it must also have enough space for an additional mem_block_descr entry
too.  We actually allocate "size + EH_MEM_ALLOCATION_INCREMENT" bytes since
we know that "size" bytes will immediately be consumed.
*/
{
  void*			mem_block;
  a_mem_allocation_ptr	map;
  a_mem_block_descr_ptr	mpdp;

  /* Adjust the requested size.  The adjusted size is a multiple of the
     memory allocation increment.  If (adjusted_size - size) >
     (memory_allocation_increment * .5) then we allocate an extra
     memory_allocation_increment bytes. */
  size =
      (size + EH_MEMORY_ALLOCATION_INCREMENT +
       (EH_MEMORY_ALLOCATION_INCREMENT >> 1)) % EH_MEMORY_ALLOCATION_INCREMENT;
  /* Get space from the memory block to store a new memory block description
     an a memory allocation record to describe it. */
  mpdp = (a_mem_block_descr_ptr)alloc_in_mem_block(NEEDED_FOR_MEM_BLOCK_DESCR,
						   &map);
  map->is_mem_block_descr_allocation = TRUE;
  mem_block = eh_get_memory(size);
  /* Add the new memory block description to the top of the stack. */
  mpdp->next = curr_mem_block_descr;
  curr_mem_block_descr = mpdp;
  /* Initialize the fields of the memory block descriptor. */
  mpdp->addr = mem_block;
  mpdp->size = size;
  mpdp->used = 0;
  mpdp->dynamically_allocated = TRUE;
}  /* alloc_new_mem_block */


static void* eh_alloc_on_stack(a_sizeof_t	size)
/*
Allocate a block of memory on the EH memory stack.
*/
{
  a_mem_allocation_ptr	map;
  int			needed_for_alignment;
  void*			ptr;
  a_sizeof_t		alloc_size;

  /* The memory management system is initialized the first time that
     this routine is called. */
  if (curr_mem_block_descr == NULL) {
    init_eh_memory_management();
  }  /* if */
  /* Determine the number of bytes that must be added to size to ensure
     that the resulting "used" value will be appropriately aligned. */
  needed_for_alignment = increment_needed_for_alignment(size);
  /* Make sure that the current memory block would have enough space
     leftover to allocate the requested space, plus the space needed for
     the memory allocation information plus a new memory block descriptor.
     If not, start the new memory block now. */
  alloc_size = size + needed_for_alignment;
  if ((alloc_size + NEEDED_FOR_MEM_ALLOCATION_INFO +
       RESERVED_FOR_END_OF_MEM_BLOCK) > curr_mem_block_descr->size) {
    alloc_new_mem_block(size);
  }  /* if */
  ptr = alloc_in_mem_block(alloc_size, &map);
  return ptr;
}  /* eh_alloc_on_stack */


static void free_in_mem_block(void*	ptr)
/*
Free a block of memory allocated in a memory block.
*/
{
  a_mem_allocation_ptr	map;
  int			used;

  map = mem_allocation_stack;
  check_assertion(map->addr == ptr);
  used = curr_mem_block_descr->used;
  used -= map->alloc_size;
  used -= NEEDED_FOR_MEM_ALLOCATION_INFO;
  curr_mem_block_descr->used = used;
}  /* free_in_mem_block */


static void eh_free_on_stack(void*	ptr)
/*
Free a piece of memory on the memory stack.  If a memory block becomes
empty then remove it from the stack.
*/
{
  /* Free the memory passed by the caller. */
  free_in_mem_block(ptr);
  /* Is the memory block now empty? */
  if (curr_mem_block_descr->used == 0) {
    a_mem_block_descr_ptr	mpdp_to_free;
    mpdp_to_free = curr_mem_block_descr;
    curr_mem_block_descr = mpdp_to_free->next;
    /* Free the memory block.  This is freed to the system -- not just to
       the memory stack like other kinds of memory. */
    eh_free_memory(mpdp_to_free->addr);
    /* Free the memory block description entry. */
    free_in_mem_block(mpdp_to_free);
  }  /* if */
}  /* eh_free_on_stack */


/* Determine whether two typeinfo entries refer to the same type.  They
   match if their pointers are the same or if the unique ID pointed to
   by the entries is the same (and nonzero). */
#define matching_typeinfo(type1, type2)					\
  ((type1) == (type2) || ((type1)->unique_id == (type2)->unique_id) &&  \
                          (type1)->unique_id != 0)

#if DEBUG
static void db_eh_region_descr(an_eh_region_descr_ptr  ehrdp)
/*
Print the contents of a region description entry.
*/
{
  if (ehrdp->flags) {
    fprintf(__f_debug, "  flags: ");
    if (ehrdp->flags & RDF_INDIRECT) fprintf(__f_debug, " indirect");
    if (ehrdp->flags & RDF_NEW_ALLOCATION) fprintf(__f_debug, " new");
  }  /* if */
  fprintf(__f_debug, "  destr/delete=%p\n",
          (void*)ehrdp->destructor_or_delete_routine);
#if 0
  if (ehrdp->array_size != 0) {
    fprintf(__f_debug, "  array_size=%ld\n", ehrdp->array_size);
  }  /* if */
#endif /* 0 */
  fprintf(__f_debug, "  handle=%d\n", ehrdp->handle);
  fprintf(__f_debug, "  prev region=%d\n", ehrdp->index_of_previous_region);
} /* db_eh_region_descr */
#endif /* DEBUG */


static void set_base_class_flags(a_typeinfo_ptr	class_typeinfo,
				 a_boolean	set_flag)
/*
Go through all of the base classes (direct and indirect) of the class
indicated by class_typeinfo and set the base class flags in the unique
ID.  This is used later to determine whether a catch clause refers to
a base class of the class being thrown.  set_flag is TRUE if the flags
are to be set and FALSE if they are to be cleared.

When the flags are set this routine detects ambiguous base classes and
sets the flags accordingly.
*/
{
  a_base_class_spec_ptr	bcsp = class_typeinfo->base_class_entries;

  if (bcsp != NULL) {
    /* A base class list is present. */
    a_boolean	done = FALSE;
    do {
      a_typeinfo_ptr	base_typeinfo = bcsp->typeinfo;
      a_unique_id	new_value;
      /* Set the flags for this base class and then call this routine
         recursively. */
      if (set_flag) {
        a_unique_id	old_value = *(base_typeinfo->unique_id);
        if (old_value == BCS_NO_FLAGS) new_value = bcsp->flags;
      } else {
        new_value = BCS_NO_FLAGS;
      }  /* if */
      *(base_typeinfo->unique_id) = new_value;
      if (base_typeinfo->base_class_entries != NULL) {
        /* This base class has its own bases.  Call this routine
	   recursively. */
        set_base_class_flags(base_typeinfo, set_flag);
      }  /* if */
      /* The last entry in the array will have the BCS_LAST flag set. */
      done = bcsp->flags & BCS_LAST;
      /* Advance the pointer to the next element in the array of base
         class specifications. */
      bcsp++;
    } while (!done);
  }  /* if */
}  /* set_base_class_flags */


static void cleanup(an_eh_stack_entry_ptr ehsep,
                      a_region_number       region)
/*
Do the cleanup operations required in the function described by ehsep.
The current region number within ehsep is designated by region.
*/
{
  for (;;) {
    an_eh_region_descr_ptr	ehrdp;
    an_object_ptr	        *obj_addr_array;
    an_object_ptr	        obj_addr;
    char			*temp_addr;
    a_region_descr_flag_set     flags;

    /* If the region number is the NULL region then there is no
       cleanup required in this function. */
    if (region == NULL_REGION_NUMBER) break;
    ehrdp = &ehsep->variant.function.regions[region];
    flags = ehrdp->flags;
    obj_addr_array = ehsep->variant.function.object_address_table;
    obj_addr = *(obj_addr_array + ehrdp->handle);
    if (flags & RDF_INDIRECT) {
      temp_addr = (char *)*(void**)obj_addr;
#if 0
      /* Need to add this parameter offset code. */
      temp_addr += ehrdp->indirect_offset;
#endif /* 0 */
      obj_addr = (void *)temp_addr;
    }  /* if */
#if DEBUG
    if (__debug_level >= 2) {
      fprintf(__f_debug, "Region: %d, descr address=%p\n", region,
              (void*)ehrdp);
      db_eh_region_descr(ehrdp);
      fprintf(__f_debug, "  object address=%p\n", (void*)obj_addr);
    }  /* if */
#endif /* DEBUG */
    /* Do the actual cleanup of the object. */
    if (!(flags & RDF_NEW_ALLOCATION)) {
      /* A normal (not a new allocation) region.  Call the destructor for
         the object. */
      a_destructor_ptr	dtor_ptr;
      dtor_ptr = (a_destructor_ptr)ehrdp->destructor_or_delete_routine;
      (dtor_ptr)(obj_addr, 2);
    } else {
      /* A new allocation region.  Call the delete operator to free the
         space. */
#if 0
      /* Handling of placement new? */
#endif /* 0 */
      a_delete_ptr	delete_ptr;
      delete_ptr = (a_delete_ptr)ehrdp->destructor_or_delete_routine;
      (delete_ptr)(obj_addr);
    }  /* if */
    region = ehrdp->index_of_previous_region;
  }  /* for */
}  /* cleanup */


static int check_catches(an_eh_stack_entry_ptr	ehsep,
                         a_typeinfo_ptr		typeinfo)
/*
Examine the catch information associated with a given try block and
determine whether any of the clauses match the object being thrown.
Returns 0 if no matching catch was found.  If a match is found
the position in the catch array is returned (actually, the array index
plus 1).
*/
{
  an_exception_type_specification_ptr	etsp;
  int					result = 0;
  int					index = 0;
  a_boolean				done = FALSE;

  etsp = ehsep->variant.try_block.catch_entries;
  do {
    a_boolean	match = FALSE;
    index++;
#if 0
    /* Pointer and reference handling needs to be added. */
#endif /* 0 */
    if (etsp->flags & ETS_IS_ELLIPSIS) {
      match = TRUE;
    } else if (matching_typeinfo(etsp->typeinfo, typeinfo)) {
      match = TRUE;
    } else if (*(etsp->typeinfo->unique_id) != BCS_AMBIGUOUS) {
      /* An ambiguous base class -- no match. */
    } else if (*(etsp->typeinfo->unique_id) != BCS_NO_FLAGS) {
      /* A base class of the class that was thrown. */
      match = TRUE;
#if 0
      /* Derived to base conversion needs to be done. */
#endif /* 0 */
    }  /* if */
    if (match) {
      result = index;
      break;
    }  /* if */
    done = etsp->flags & ETS_LAST;
    etsp++;
  } while (!done);
  return result;
}  /* check_catches */


/*
Temporary variables that hold the information about the thrown type.
This will be replaced with a stack of throw information.
*/
static a_typeinfo_ptr	thrown_typeinfo = NULL;
static a_boolean	thrown_is_pointer;
static int		throw_buffer[1024];
static a_boolean	throw_in_process = FALSE;


EXTERN_C int __throw(void)
/*
Process a throw.  This routine looks through the stack entries for
a try block with a catch that matches the type of the object thrown.
*/
{
  an_eh_stack_entry_ptr	ehsep;
  a_region_number		region = __eh_curr_region;
  an_eh_stack_entry_ptr	destination_ehsep = NULL;
  int				destination_catch_value;

#if DEBUG
  if (__debug_level >= 1) {
    fprintf(__f_debug, "__throw called\n");
  }  /* if */
#endif /* DEBUG */
  /* Set the base class flags for the thrown type. */
  /* Find the try block that can catch the object being thrown. */
#if 0
#else /* 0 */
  if (thrown_typeinfo != NULL) {
    /* Clear the base class flags from the previous throw.  This needs to
       be changed when stacked throws are implemented. */
    set_base_class_flags(thrown_typeinfo, /*set_flag=*/FALSE);
  }  /* if */
#endif /* 1 */
  set_base_class_flags(thrown_typeinfo, /*set_flag=*/TRUE);
  ehsep = __curr_eh_stack_entry;
  while (ehsep != NULL) {
    an_eh_stack_entry_kind	kind = ehsep->kind;
    if (kind == (an_eh_stack_entry_kind)ehsek_function) {
      /* Do nothing with function blocks at this time. */
    } else if (kind == (an_eh_stack_entry_kind)ehsek_try_block) {
      int result = check_catches(ehsep, thrown_typeinfo);
      if (result != 0) {
        destination_ehsep = ehsep;
        destination_catch_value = result;
        break;
      }  /* if */
    } else if (kind == (an_eh_stack_entry_kind)ehsek_throw_spec) {
#if 0
      /* Check for violations of throw specifications. */
#endif /* 0 */
    } else {
      unexpected_condition();
    }  /* if */
    ehsep = ehsep->next;
  }  /* while */

  if (destination_ehsep == NULL) {
    /* If no handler was found call the terminate function. */
   __call_terminate();
  }  /* if */

  ehsep = __curr_eh_stack_entry;
  while (ehsep != destination_ehsep) {
    an_eh_stack_entry_kind	kind = ehsep->kind;
    if (kind == (an_eh_stack_entry_kind)ehsek_function) {
#if DEBUG
       if (__debug_level >= 2) {
         fprintf(__f_debug, "Processing EH stack entry at %p\n",
                 (void *)ehsep);
       }  /* if */
#endif /* DEBUG */
      cleanup(ehsep, region);
      region = ehsep->variant.function.saved_region_number;
    } else if (kind == (an_eh_stack_entry_kind)ehsek_try_block) {
      /* A try block that is being skipped -- do nothing. */
    } else if (kind == (an_eh_stack_entry_kind)ehsek_throw_spec) {
      /* Do nothing. */
    } else {
      unexpected_condition();
    }  /* if */
    ehsep = ehsep->next;
  }  /* while */
  if (destination_ehsep != NULL) {
    __catch_clause_number = destination_catch_value;
    __curr_eh_stack_entry = destination_ehsep;
#if 0
    /* This will need to be modified for a more sophisticated memory
       management scheme.  Also, needs to handle derived to base
       conversions. */
#endif /* 0 */
    __caught_object_address = (void *)throw_buffer;
   longjmp(destination_ehsep->variant.try_block.setjmp_buffer, 1);
  }  /* if */
  return 0;
}  /* __throw */


EXTERN_C void* __throw_alloc(a_typeinfo_ptr	typeinfo,
			     a_sizeof_t		size,
			     a_boolean		is_pointer)
/*
Allocate space for the object to be thrown and save information about
the type being thrown.
*/
{
#if 0
  /* This is a temporary version that just saved the information in static
     variables.  The real version will push the information onto a
     throw stack. */
#endif /* 0 */
#if DEBUG
  if (throw_in_process) {
    fprintf(__f_debug, "Nested throw attempted.\n");
    abort();
  }  /* if */
#endif /* DEBUG */
  thrown_typeinfo = typeinfo;
  thrown_is_pointer = is_pointer;
  return (void *)throw_buffer;
}  /* __throw_alloc */


EXTERN_C void __free_thrown_object(void)
/*
Free the space used to make the copy of the thrown object.  Called at
the completion of a catch clause.
*/
{
#if 0
  /* To be added when throw stacking is added. */
#endif /* 0 */
}  /* __free_thrown_object */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
