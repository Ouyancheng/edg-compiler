/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

pch.c -- Precompiled header processing.

*/

#include "basics.h"
#include "pch.h"
#include "mem_manage.h"
#include "version.h"

#define PCH_ID_STRING_LENGTH 128
			/* Maximum length of the PCH id string. */

static char	pch_id_string[PCH_ID_STRING_LENGTH];
			/* Buffer used to store the PCH id string. */

static a_void_ptr
		pch_memory_block;
			/* The block of memory reserved for precompiled
			   header processing. */

static sizeof_t
		pch_mem_allocated;
			/* The amount of pch_memory_block that has been
			   allocated. */

static a_pch_event_ptr
		pch_event_list_head;
			/* List of precompiled header events for the
			   current primary input file. */

static a_pch_event_ptr
		pch_event_list_tail;
			/* Pointer to the end of the list of precompiled
                           header events for the current primary input file. */

#if DEBUG
static long	num_pch_events_allocated;
#endif /* DEBUG */

			
static void initialize_pch_id_string(void)
/*
Create the string that is used to identify a flag as a precompiled header
associated with this compiler version.
*/
{
  char		*format_string = "EDG C/C++ version %s (%s %s)";
  check_assertion_str2(strlen(format_string) +
                       strlen(VERSION_NUMBER) +
                       strlen(build_date) +
                       strlen(build_time) <= PCH_ID_STRING_LENGTH,
                       "initialize_pch_id_string:", "PCH ID string too long");
  sprintf(pch_id_string, format_string, VERSION_NUMBER, build_date,
          build_time);
}  /* initialize_pch_id_string */


static void alloc_pch_memory_block(void)
/*
Allocate the block of memory reserved for precompiled header processing.
This memory must be allocated before any of the memory region memory
is allocated.  A fixed size block is reserved so that we can ensure
that precompiled header processing will use a fixed amount of memory thus
allowing us to reload the memory region information from the precompiled
header file into the same range of addresses used when the precompiled
header was generated.
*/
{
  pch_memory_block = (a_void_ptr)alloc_general(MEM_ALLOCATED_FOR_PCH_ANALYSIS);
  pch_mem_allocated = 0;
}  /* alloc_pch_memory_block */


static a_void_ptr alloc_pch_memory(sizeof_t size)
/*
Allocate "size" bytes of memory in the PCH memory area and return
a pointer to the allocated memory.  If there is not sufficient
memory in the PCH memory area, allocate the memory from general
memory and indicate that precompiled header processing cannot be
done for this file.  This is done instead of simply returning a
NULL pointer so that the callers of this routine do not have to
worry about the prospect of running out of memory.
*/
{
  a_void_ptr	ptr;

  /* Round up the size if necessary to preserve alignment.  Note that
     aside from keeping the data correctly aligned, this also keeps the
     next available address properly aligned. */
  do_host_alignment(size);
  if (size > (MEM_ALLOCATED_FOR_PCH_ANALYSIS - pch_mem_allocated)) {
    /* Not enough memory left in the PCH memory block, use general
       memory. */
    ptr = (a_void_ptr)alloc_general(size);
    /* Indicate that we can't generate/use precompiled header information
       because we exhausted the PCH memory block. */
    abandon_pch_processing();
  } else {
    /* Allocate "size" bytes from the PCH memory block. */
    ptr = ((char *)pch_memory_block) + pch_mem_allocated;
    pch_mem_allocated += size;
  }  /* if */
  return ptr;
}  /* alloc_pch_memory */


static a_pch_event_ptr alloc_pch_event(void)
/*
Allocate and initialize a precompiled header event record.
*/
{
  a_pch_event_ptr pep;

  /* Allocate a new entry. */
#if 0
  /* Special handling needed for command line events? */
#endif /* 0 */
  pep = (a_pch_event_ptr)alloc_pch_memory(sizeof(a_pch_event));
#if DEBUG
  num_pch_events_allocated++;
#endif /* DEBUG */
  pep->next = NULL;
  pep->kind = pchek_unknown;
  pep->ppd_kind = ppd_not_valid;
  pep->value = NULL;
  pep->position = null_source_position;
  return pep;
}  /* alloc_pch_event */


void add_pch_event(a_pch_event_kind	kind,
		   a_pp_directive_kind	ppd_kind,
		   char			*value,
		   a_source_position	*position)
/*
Add a precompiled header event record to the list of events for the current
file.
*/
{
  a_pch_event_ptr	pep;

  db_enter(4, "add_pch_event");
  pep = alloc_pch_event();
  pep->kind = kind;
  pep->ppd_kind = ppd_kind;
  if (value != NULL) {
    /* Copy the value string into PCH memory. */
    pep->value = (char *)alloc_pch_memory((sizeof_t)(strlen(value) + 1));
    (void)strcpy(pep->value, value);
  }  /* if */
  pep->position = *position;
  /* Add this entry to the list. */
  if (pch_event_list_head == NULL) pch_event_list_head = pep;
  if (pch_event_list_tail != NULL) pch_event_list_tail->next = pep;
  pch_event_list_tail = pep;
#if DEBUG
  if (debug_level >= 0) {
    fprintf(f_debug, "Added PCH event: %s, %s, value=%s, line %0d, col %0d\n",
            pch_event_kind_names[(int)pep->kind],
            pp_directive_kind_names[(int)pep->ppd_kind],
            pep->value == NULL ? "(NULL)" : pep->value,
            pep->position.seq, pep->position.column);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* add_pch_event */


static void build_prefix_information(void)
/*
Do an initial scan of the primary source file to build the file prefix
information.
*/
{
  an_input_stack_entry	saved_input_stack_entry;

  /* Set the flag that indicate that we are build the file prefix
     information.  This affects the way in which preprocessing directives
     are handled and the way end-of-file is processed. */
  building_pch_prefix = TRUE;
  /* Simply do a get_token call.  This will return the first token
     of the file that is not a comment or a preprocessing directive.
     Because the prefix information includes only preprocessing directives,
     they will have all been seen by the time the first token is returned.
     The actual work to build the prefix information is done by special
     processing in preproc.c that is enabled when building_pch_prefix is
     TRUE. */
  if (get_token() != tok_end_of_source) {
    /* If we didn't reach the end of the source file, pop the input stack
       and close the primary input file. */
    pop_input_stack();
  }  /* if */
  /* Reset the state information maintained by the lexical routines. */
  lexical_reset();
  /* Update curr_char_loc to point to the end of the current line.  This
     will force the next token to begin on a new line. */
  building_pch_prefix = FALSE;
}  /* build_prefix_information */


void precompiled_header_processing(void)
/*
This is the main routine responsible for precompiled header processing.
It is called after the primary source file has been opened but before any
tokens have been fetched.  First, the file "prefix" is scanned.  The prefix
is the sequence of preprocessing directives that precede the first declaration
of the file.  When determining whether an existing precompiled header may
be used to replace part of this compilation, the prefix will be compared
with the prefix of candidate precompiled header files to see if they
are applicable.  If this compilation is to generate a precompiled header
file, the prefix information will be saved in the file so that it may
be used as part of the applicability check in subsequent compilations.
*/
{
  build_prefix_information();
}  /* precompiled_header_processing */


void pch_init(void)
/*
Initialize variables used by the precompiled header routines.
*/
{
  db_enter(4, "pch_init");
  alloc_pch_memory_block();
  initialize_pch_id_string();
  cannot_do_pch_processing = FALSE;
  pch_event_list_head = NULL;
  pch_event_list_tail = NULL;
  building_pch_prefix = FALSE;
#if DEBUG
  num_pch_events_allocated = 0;
#endif /* DEBUG */
  db_exit();
}  /* pch_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
