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

#include "basics.h"
#include "eh.h"


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
#endif
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
    index++;
#if 0
    /* Pointer and reference handling needs to be added. */
    /* Base class handling needs to be added. */
#endif /* 0 */
    if (matching_typeinfo(etsp->typeinfo, typeinfo)) {
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
static a_typeinfo_ptr	thrown_typeinfo;
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
  /* Find the try block that can catch the object being thrown. */
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
    __default_terminate_routine();
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

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
