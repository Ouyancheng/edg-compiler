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

static a_pch_event_ptr
		pch_cmd_line_event_list_head;
			/* List of precompiled header events associated with
			   the command line. */

static a_pch_event_ptr
		pch_cmd_line_event_list_tail;
			/* Pointer to the end of the list of precompiled
                           header events associated with the command line. */

static char	*pch_file_name;
			/* Name of the precompiled header file being written
			   or read. */

static FILE	*f_pch_output;
			/* File to which the precompiled header information
			   is being written. */


#define MAX_NUMBER_OF_SAVED_VARIABLE_LISTS 64
			/* The number of saved variable lists that can be
			   used.  One saved variable list entry will be used
			   for each compiled source file containing variables
			   to be saved. */

static a_pch_saved_variable_ptr
		saved_variable_array_list[MAX_NUMBER_OF_SAVED_VARIABLE_LISTS];
			/* Array of pointers to arrays of saved variable
			   lists.  Each element points to an array of
			   saved variable entries. */

static int	num_of_saved_variable_lists;
			/* Number of entries in the saved variable array list
			   that have been used. */

#if DEBUG
static long	num_pch_events_allocated;
#endif /* DEBUG */

			
#define PCH_BUFFER_INITIAL_ALLOCATION 2048
#define PCH_BUFFER_INCREMENTAL_ALLOCATION 1024
			/* Initial and incremental allocation sizes for
			   pch_buffer.  The initial allocation
			   should be such that almost all cases can be
			   accepted (so that the realloc is hardly ever
			   needed). */

/*
Dynamically allocated buffer used to contain string that are used
during precompiled header prefix comparisions.
*/
static char	*pch_buffer = NULL;
			/* Not allocated on a per-file basis. */

static sizeof_t	size_pch_buffer;
			/* Current size of pch_buffer. */


static void expand_pch_buffer(sizeof_t size_needed)
/*
Expand the pch_buffer by reallocating it, so that its total
size is at least size_needed.  Called by ensure_pch_buffer_space.
*/
{
  sizeof_t new_size;

  new_size = size_pch_buffer +
             PCH_BUFFER_INCREMENTAL_ALLOCATION;
  if (new_size < size_needed) new_size  = size_needed;
  pch_buffer = realloc_general(pch_buffer, size_pch_buffer, new_size);
  size_pch_buffer = new_size;
}  /* expand_pch_buffer */


/*
Ensure that pch_buffer has at least size_needed bytes in it.
If not, expand pch_buffer by reallocating it.
*/
#define ensure_pch_buffer_space(size_needed)                 \
{ if (size_pch_buffer < size_needed) {                       \
    expand_pch_buffer((sizeof_t)(size_needed));              \
  }  /* if */                                                          \
}  /* ensure_pch_buffer_space */


static void initialize_pch_id_string(void)
/*
Create the string that is used to identify a flag as a precompiled header
associated with this compiler version.
*/
{
  char		*format_string = "EDG C/C++ version %s (%s %s)\n";
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


static a_pch_event_ptr alloc_pch_event(a_pch_event_kind kind)
/*
Allocate and initialize a precompiled header event record.
*/
{
  a_pch_event_ptr pep;

  /* Allocate a new entry. */
  if (kind == pchek_command_line) {
    /* Command line events are reused for multiple source files so
       must be allocated in general memory. */
    pep = (a_pch_event_ptr)alloc_general(sizeof(a_pch_event));
  } else {
    pep = (a_pch_event_ptr)alloc_pch_memory(sizeof(a_pch_event));
  }  /* if */
#if DEBUG
  num_pch_events_allocated++;
#endif /* DEBUG */
  pep->next = NULL;
  pep->kind = kind;
  switch (kind) {
    case pchek_command_line:
      pep->variant.cl_option.kind = optk_none;
      pep->variant.cl_option.opt_value = FALSE;
      break;
    case pchek_pp_directive:
      pep->variant.ppd_kind = ppd_not_valid;
      break;
    case pchek_sequence_marker:
      break;
    default:
      unexpected_condition();
  }  /* switch */
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
  pep = alloc_pch_event(kind);
  if (kind == pchek_pp_directive) {
    pep->variant.ppd_kind = ppd_kind;
  }  /* if */
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
  if (debug_level >= 4) {
    fprintf(f_debug, "Added PCH event: %s, value=%s, line %0ld, col %0d\n",
            pch_event_kind_names[(int)pep->kind],
            pep->value == NULL ? "(NULL)" : pep->value,
            pep->position.seq, pep->position.column);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* add_pch_event */


void add_command_line_pch_event(a_pch_event_kind	kind,
                                an_option_kind		opt_kind,
				a_boolean		opt_value,
				char			*optarg)
/*
Add a precompiled header event to the list of events associated with
the command line.
*/
{
  a_pch_event_ptr	pep;

  db_enter(4, "add_command_line_pch_event");
  check_assertion_str2(kind == pchek_command_line,
                       "add_command_line_pch_event:",
                       "invalid pch event kind");
  pep = alloc_pch_event(kind);
  pep->variant.cl_option.kind = opt_kind;
  pep->variant.cl_option.opt_value = opt_value;
  if (optarg != NULL) {
    /* Command line events are reused for multiple source files so
       the value string must be allocated in general memory. */
    pep->value = (char *)alloc_general((sizeof_t)(strlen(optarg) + 1));
    (void)strcpy(pep->value, optarg);
  }  /* if */
  /* Add this entry to the list. */
  if (pch_cmd_line_event_list_head == NULL) {
    pch_cmd_line_event_list_head = pep;
  }  /* if */
  if (pch_cmd_line_event_list_tail != NULL) {
    pch_cmd_line_event_list_tail->next = pep;
  }  /* if */
  pch_cmd_line_event_list_tail = pep;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Added PCH event: %s, value=%s\n",
            pch_event_kind_names[(int)pep->kind],
            pep->value == NULL ? "(NULL)" : pep->value);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* add_command_line_pch_event */


static void build_prefix_information(void)
/*
Do an initial scan of the primary source file to build the file prefix
information.
*/
{
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
  header_stop_source_position = pos_curr_token;
  /* Reset the state information maintained by the lexical routines. */
  lexical_reset();
  /* Update curr_char_loc to point to the end of the current line.  This
     will force the next token to begin on a new line. */
  building_pch_prefix = FALSE;
}  /* build_prefix_information */


#if 0
static void compare_prefix_with_existing_headers(void)
/*
Compare the prefix information for this file with the prefix
information for the other precompiled headers in the current
directory.
*/
{
  a_boolean	first;
  char		*file_name;

  db_enter(3, "compare_prefix_with_existing_headers");
  for (first = TRUE;
       (file_name = get_file_name_from_curr_dir(first)) == NULL;
       first = FALSE) {
#if 0
    /* Make sure this is a regular file with a PCH suffix. */
#endif /* 0 */
    pch_input_file = fopen(file_name, "rb");
  }  /* for */
  db_exit();
}  /* compare_prefix_with_existing_headers */
#endif


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
#if 0
  compare_prefix_with_existing_headers();
#endif
#if 0
  write_precompiled_header_file();
#endif
}  /* precompiled_header_processing */


static void open_pch_output_file(void)
/*
Create or truncate the precompiled header file.
*/
{
  a_boolean	cannot_open;
  a_boolean	bad_name;

  pch_file_name = derived_name(primary_source_file_name, PCH_FILE_SUFFIX);
  f_pch_output = open_output_file(pch_file_name, /*binary_file=*/TRUE,
                                  /*update_mode=*/FALSE,
                                  &cannot_open, &bad_name);
  if (bad_name) {
    str_command_line_error(ec_cl_invalid_pch_output_file, pch_file_name);
  } else if (cannot_open) {
    str_command_line_error(ec_cl_cannot_open_pch_output_file,
                           pch_file_name);
  }  /* if */
}  /* open_pch_output_file */


/*
Macro to write a value to the PCH output file.
*/
#define pch_write_value(value)						\
  (void)fwrite(&value, sizeof(value), 1, f_pch_output);


static void pch_write_string(char	*str)
/*
Write a null terminated character string to the PCH output file.  The
string is written as a length followed by the characters of the string.
Both the length and the actual string include the null terminator.
*/
{
  sizeof_t	length;
  if (str != NULL) {
    length = strlen(str) + 1;
    pch_write_value(length);
    (void)fwrite(str, length, 1, f_pch_output);
  } else {
    /* The string pointer is null.  Represent this as a zero length
       string. */
    length = 0;
    pch_write_value(length);
  }  /* if */
}  /* pch_write_string */


static void write_pch_events(a_pch_event_ptr list)
/*
Write a list of precompiled header events to the PCH output file.
*/
{
  a_pch_event_ptr	pep;
  a_pch_event_kind	dummy_pchek;

  for (pep = list; pep != NULL; pep = pep->next) {
    check_assertion(pep->kind != pchek_none);
    pch_write_value(pep->kind);
    switch (pep->kind) {
      case pchek_command_line:
        pch_write_value(pep->variant.cl_option.kind);
        pch_write_value(pep->variant.cl_option.opt_value);
        break;
      case pchek_pp_directive:
        pch_write_value(pep->variant.ppd_kind);
        break;
      case pchek_sequence_marker:
        break;
      default:
        unexpected_condition();
    }  /* switch */
    pch_write_string(pep->value);
    pch_write_value(pep->position);
  }  /* for */
  /* An event kind of "none" terminates the list. */
  dummy_pchek = pchek_none;
  pch_write_value(dummy_pchek);
}  /* write_pch_events */


void write_precompiled_header_file(void)
/*
Create a precompiled header file for the compilation up to the
current point.
*/
{
  open_pch_output_file();
  /* Write the string that identifies this file as a precompiled header
     file. */
  (void)fputs(pch_id_string, f_pch_output);
  write_pch_events(pch_cmd_line_event_list_head);
  write_pch_events(pch_event_list_head);
  (void)fclose(f_pch_output);
}  /* write_precompiled_header_file */


void register_pch_saved_variables(a_pch_saved_variable array[])
/*
*/
{
  check_assertion_str2
            (num_of_saved_variable_lists < MAX_NUMBER_OF_SAVED_VARIABLE_LISTS,
             "register_pch_saved_variables:",
             "too many saved variable lists");
  saved_variable_array_list[num_of_saved_variable_lists++] = array;
}  /* register_pch_saved_variables */


void pch_init(void)
/*
Initialize variables used by the precompiled header routines.
*/
{
  db_enter(4, "pch_init");
#if DEBUG
  check_assertion(strcmp(pch_event_kind_names[(int)pchek_last], "last") == 0);
#endif /* DEBUG */
  if (pch_buffer == NULL) {
    pch_buffer = (char *)alloc_general(PCH_BUFFER_INITIAL_ALLOCATION);
    size_pch_buffer = PCH_BUFFER_INITIAL_ALLOCATION;
  }  /* if */
  alloc_pch_memory_block();
  initialize_pch_id_string();
  cannot_do_pch_processing = FALSE;
  pch_event_list_head = NULL;
  pch_event_list_tail = NULL;
  building_pch_prefix = FALSE;
  header_stop_source_position = null_source_position;
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
