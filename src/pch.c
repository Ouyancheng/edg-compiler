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

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "pch.h"
#include "decls.h"
#include "statements.h"
#include "symbol_ref.h"
#include "macro.h"

/* Declaration needed because of forward reference. */
static void pch_one_time_init(void);


#define PCH_ID_STRING_LENGTH 128
			/* Maximum length of the PCH id string. */

static char	pch_id_string[PCH_ID_STRING_LENGTH];
			/* Buffer used to store the PCH id string. */

static sizeof_t	pch_id_string_length;
			/* The actual length of the PCH id string (including
                           the trailing null character). */

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

static FILE	*f_pch_input;
			/* File from which the precompiled header information
			   is being read. */

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

static int	num_of_saved_variable_lists = 0;
			/* Number of entries in the saved variable array list
			   that have been used. */

static an_error_code
		mismatch_reason;
			/* An error code that specifies why a given
			   precompiled header file could not be used. */

static an_il_header
		il_header_from_pch;
			/* Copy of the IL header from the compilation that
			   generated the PCH file. */

static a_seq_number
		saved_curr_seq_number;
			/* Saved value of curr_seq_number, used to fix up
			   the source file sequence number information. */

static a_mem_alloc_history_ptr
		new_alloc_history;
			/* The memory allocation history information
			   read from the precompiled header file. */

static a_mem_alloc_history_number
		new_alloc_history_entries = 0;
			/* Number of entries in new_alloc_history. */

static a_text_buffer_ptr
		file_name_text_buffer;
			/* A buffer used to construct PCH file names. */

/*
Macro to write a value to the PCH output file.
*/
#define pch_write_value(value)						\
  (void)fwrite((a_stdio_arg)&(value), sizeof(value), 1, f_pch_output)


static void bad_pch_file(void)
/*
Called when a read operation on a PCH file fails.  Issue a catastrophic
error.
*/
{
  /* The error position is reset to prevent the error from being issued
     with respect to a particular source line.  This is important because
     the state information used to file source lines, etc. may be in an
     indeterminate state. */
  error_position = null_source_position;
  catastrophe(ec_bad_pch_file);
}  /* bad_pch_file */


static void pch_write_error(void)
/*
Called when a write operation on a PCH file fails.  Issue a catastrophic
error.
*/
{
  error_position = null_source_position;
  str_catastrophe(ec_file_write_error, "PCH");
}  /* pch_write_error */


/*
Macro to read a value from the PCH input file.
*/
#define pch_read_value(value)						\
  if (fread((a_stdio_arg)&(value), sizeof((value)), 1, f_pch_input) != 1) { \
    bad_pch_file();							\
  }  /* if */


/*
Macro to perform an fread with an error check.
*/
#define fread_with_check(value, length, file)				\
  if (fread((a_stdio_arg)(value), size_t_arg((length)), 1, (file)) != 1) { \
    bad_pch_file();							\
  }  /* if */

/*
Macro to perform an fread and return an error status.  Return TRUE if the
read succeeded.
*/
#define fread_with_status(value, length, file)				\
  (fread((a_stdio_arg)(value), size_t_arg((length)), 1, (file)) == 1)

/*
Macro to perform an fwrite with an error check.
*/
#define fwrite_with_check(value, length, file)				\
  if (fwrite((a_stdio_arg)(value), size_t_arg((length)), 1, (file)) != 1) { \
    pch_write_error();							\
  }  /* if */

/*
Macro to do an fseek on the output file with an error check.
*/
#define fseek_with_check(file, pos, mode)			\
  if (fseek((file), (long)(pos), (mode)) != 0) {			\
    pch_write_error();							\
  }  /* if */


#if DEBUG
static long	num_pch_events_allocated;
#endif /* DEBUG */

#if CHECKING
/* Enumeration of sections of the PCH file.  This is used to make sure that
   the file is positioned at the correct location before a section of the
   file is read. */
typedef enum /* a_pch_file_section */ {
  pfs_cmd_line_events,
  pfs_other_events,
  pfs_include_file_info,
  pfs_mem_alloc_info,
  pfs_saved_variables,
  pfs_memory_regions,
  pfs_last		/* Must be last. */
} a_pch_file_section;

#if DEBUG
static char	*file_section_names[(int)pfs_last + 1] =
{
  "cmd_line_events",
  "other_events",
  "include_file_info",
  "mem_alloc_info",
  "saved_variables",
  "memory_regions",
  "last"
};
#endif /* DEBUG */


static void write_file_section_id(a_pch_file_section section)
/*
Write the file section ID to the PCH file.
*/
{
  pch_write_value(section);
}  /* write_file_section_id */


static void check_file_section_id(a_pch_file_section section)
/*
Read a file section ID from the PCH file and compare it with the expected
value passed by the caller.
*/
{
  a_pch_file_section	section_in_file;

  pch_read_value(section_in_file);
#if DEBUG
  if (section_in_file != section) {
    fprintf(f_debug, "Incorrect file section ID: expected %d, got %d\n",
            section, section_in_file);
    fprintf(f_debug, "  (expected name: %s, got name: %s\n",
            file_section_names[(int)section],
            file_section_names[(int)section_in_file]);
  }  /* if */
#endif /* DEBUG */
  check_assertion_str2(section_in_file == section,
                       "check_file_section_id:",
                       "incorrect file section encountered");
}  /* check_file_section_id */
#else /* !CHECKING */
/*
When not generating checking code, these functions are replaced with NULL
macros.
*/
#define write_file_section_id(value) /* Nothing. */
#define check_file_section_id(value) /* Nothing. */
#endif /* CHECKING */

			
#define PCH_BUFFER_INITIAL_ALLOCATION 2048
#define PCH_BUFFER_INCREMENTAL_ALLOCATION 1024
			/* Initial and incremental allocation sizes for
			   pch_buffer.  The initial allocation
			   should be such that almost all cases can be
			   accepted (so that the realloc is hardly ever
			   needed). */

/*
Dynamically allocated buffer used to contain string that are used
during precompiled header prefix comparisons.
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


#define FILE_NAME_BUFFER_INCREMENTAL_ALLOCATION 1024
			/* Initial and incremental allocation sizes for
			   file_name_buffer.  The initial allocation
			   should be such that almost all cases can be
			   accepted (so that the realloc is hardly ever
			   needed). */

typedef struct a_file_name_buffer *a_file_name_buffer_ptr;
typedef struct a_file_name_buffer {
  char		*name;
			/* Pointer to a buffer containing the file name. */
  sizeof_t	size;
			/* Allocated size of the buffer pointed to by
			   file_name. */
} a_file_name_buffer;

static a_file_name_buffer
		file_name_buffer;
			/* Buffer use to temporarily record file names. */


static void expand_file_name_buffer(a_file_name_buffer_ptr fnbp,
                                    sizeof_t		   size_needed)
/*
Expand the file_name_buffer by reallocating it, so that its total
size is at least size_needed.  Called by ensure_file_name_buffer_space.
*/
{
  sizeof_t new_size;

  new_size = fnbp->size + FILE_NAME_BUFFER_INCREMENTAL_ALLOCATION;
  if (new_size < size_needed) new_size  = size_needed;
  fnbp->name = realloc_general(fnbp->name, fnbp->size, new_size);
  fnbp->size = new_size;
}  /* expand_file_name_buffer */


/*
Ensure that file_name_buffer has at least size_needed bytes in it.
If not, expand file_name_buffer by reallocating it.
*/
#define ensure_file_name_buffer_space(fnb, size_needed)                 \
{ if ((fnb).size < (size_needed)) {                    			  \
    expand_file_name_buffer(&(fnb), (sizeof_t)(size_needed));              \
  }  /* if */                                                          \
}  /* ensure_file_name_buffer_space */


static char *build_pch_file_name(char	*file_name)
/*
Concatenate the PCH directory with the specified file name.  Uses a
local file name buffer for storage.  Return a pointer to the name.  If no
directory name is being used, a pointer to the original name is returned.
*/
{
  char	*result;

  if (pch_dir_name == NULL || is_absolute_file_name(file_name)) {
    result = file_name;
  } else {
    if (file_name_text_buffer == NULL) {
      /* Allocate this buffer the first time it is needed. */
      file_name_text_buffer = alloc_text_buffer(256);
    }  /* if */
    (void)combine_dir_and_file_name(pch_dir_name, file_name,
                                    file_name_text_buffer);
    result = file_name_text_buffer->buffer;
  }  /* if */
  return result;
}  /* build_pch_file_name */


static void initialize_pch_id_string(void)
/*
Create the string that is used to identify a flag as a precompiled header
associated with this compiler version.
*/
{
  char		*format_string = "EDG C/C++ version %s (%s %s)\n";
  check_assertion_str2((sizeof_t)(strlen(format_string) +
                       strlen(VERSION_NUMBER) +
                       strlen(build_date) +
                       strlen(build_time)) <= (sizeof_t)(PCH_ID_STRING_LENGTH),
                       "initialize_pch_id_string:", "PCH ID string too long");
  sprintf(pch_id_string, format_string, VERSION_NUMBER, build_date,
          build_time);
  pch_id_string_length = strlen(pch_id_string) + 1;
}  /* initialize_pch_id_string */


static a_pch_event_ptr alloc_pch_event(a_pch_event_kind kind)
/*
Allocate and initialize a precompiled header event record.
*/
{
  a_pch_event_ptr pep;

  /* Allocate a new entry. */
  pep = (a_pch_event_ptr)alloc_general(sizeof(a_pch_event));
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
    default:
      unexpected_condition();
  }  /* switch */
  pep->value = NULL;
  pep->position = null_source_position;
  pep->match_found = FALSE;
  return pep;
}  /* alloc_pch_event */


void add_pch_event(a_pch_event_kind	kind,
		   a_pp_directive_kind	ppd_kind,
		   char			*value,
		   a_source_position	*position,
		   a_line_number	actual_line)
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
    /* Copy the value string. */
    pep->value = (char *)alloc_general((sizeof_t)(strlen(value) + 1));
    (void)strcpy(pep->value, value);
  }  /* if */
  pep->position = *position;
  /* Replace the sequence number with the actual file line number. */
  pep->position.seq = actual_line;
  /* Add this entry to the list. */
  if (pch_event_list_head == NULL) pch_event_list_head = pep;
  if (pch_event_list_tail != NULL) pch_event_list_tail->next = pep;
  pch_event_list_tail = pep;
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("pch_event")) {
    fprintf(f_debug, "Added PCH event: %s, value=%s, line %lu, col %d\n",
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
				char			*opt_arg)
/*
Add a precompiled header event to the list of events associated with
the command line.
*/
{
  a_pch_event_ptr	pep;

  db_enter(4, "add_command_line_pch_event");
  check_assertion_str2(kind == pchek_command_line,
                       "add_command_line_pch_event:",
                       "invalid PCH event kind");
  pep = alloc_pch_event(kind);
  pep->variant.cl_option.kind = opt_kind;
  pep->variant.cl_option.opt_value = opt_value;
  if (opt_arg != NULL) {
    /* Command line events are reused for multiple source files so
       the value string must be allocated in general memory. */
    pep->value = (char *)alloc_general((sizeof_t)(strlen(opt_arg) + 1));
    (void)strcpy(pep->value, opt_arg);
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


#if DEBUG
static void db_pch_event(a_pch_event_ptr pep)
/*
Display a PCH event for debugging purposes.
*/
{
  fprintf(f_debug, "Event kind: %s", pch_event_kind_names[(int)pep->kind]);
  switch (pep->kind) {
    case pchek_command_line:
      fprintf(f_debug, ", option kind: %d", pep->variant.cl_option.kind);
      fprintf(f_debug, ", option value: %s",
              pep->variant.cl_option.opt_value ? "TRUE" : "FALSE");
      break;
    case pchek_pp_directive:
      fprintf(f_debug, ", ppd_kind: %s",
              pp_directive_kind_names[(int)pep->variant.ppd_kind]);
      break;
    default:
      unexpected_condition();
  }  /* switch */
  fprintf(f_debug, ", value: %s", pep->value == NULL ? "(NULL)" : pep->value);
  fprintf(f_debug, ", seq: %lu, column: %lu\n", pep->position.seq,
          (unsigned long)pep->position.column);
}  /* db_pch_event */
#endif /* DEBUG */


static void find_last_event_to_use(void)
/*
Once the end of the file prefix has been found, this routine determines the
last usable event in the list.  The last usable event is the last
zero-level (i.e., not within a preprocessing if directive) event that
precedes the header stop position.
*/
{
  a_pch_event_ptr	last_if_event = NULL;
  a_pch_event_ptr	pep = pch_event_list_head;
  a_pch_event_ptr	last_event_to_use = NULL;
  int			nesting_level = 0;

  for (; pep != NULL; pep = pep->next) {
    if (pep->kind == pchek_pp_directive) {
      a_pp_directive_kind	ppd_kind = pep->variant.ppd_kind;
      if (ppd_kind == ppd_if ||
          ppd_kind == ppd_ifdef ||
          ppd_kind == ppd_ifndef) {
        /* This is an if directive, save a pointer to the last if event
           and increment the if nesting level. */
        pep->last_if_event = last_if_event;
        last_if_event = pep;
        nesting_level++;
      } else if (ppd_kind == ppd_endif) {
        /* On an endif, decrement the if nesting level.  If there is
           an if/endif mismatch, don't try to do any further PCH
           processing. */
        if (nesting_level == 0) {
          /* A nesting error occurred.  Don't do any PCH processing.
             An error will be diagnosed when the file is compiled. */
          abandon_pch_processing();
          break;
        } else {
          nesting_level--;
          if (last_if_event != NULL) {
            last_if_event = last_if_event->last_if_event;
          }  /* if */
        }  /* if */
      }  /* if */
      if (nesting_level == 0 && 
          (ppd_kind == ppd_include ||
           ppd_kind == ppd_define ||
           ppd_kind == ppd_pragma ||
           ppd_kind == ppd_endif)) {
        /* Update the pointer to the last zero level event.  When we've
           scanned the whole event list, this will point to the last event
           to be included in the prefix. */
        last_event_to_use = pep;
      }  /* if */
    }  /* if */
  }  /* for */
  if (last_event_to_use != NULL) {
    /* Discard any events that follow the new last one. */
    last_event_to_use->next = NULL;
    pch_event_list_tail = last_event_to_use;
    header_stop_source_position = last_event_to_use->position;
  } else {
    /* We did not find an eligible event.  Suppress any PCH processing. */
    abandon_pch_processing();
  }  /* if */
}  /* find_last_event_to_use */


static void build_prefix_information(void)
/*
Do an initial scan of the primary source file to build the file prefix
information.
*/
{
  a_source_position	saved_pos_curr_token;
  a_source_position	saved_error_position;

  saved_pos_curr_token = pos_curr_token;
  saved_error_position = error_position;
  /* Set the flag that indicate that we are build the file prefix
     information.  This affects the way in which preprocessing directives
     are handled and the way end-of-file is processed. */
  building_pch_prefix = TRUE;
  /* If any preinclude files were specified, create an event for them. */
  if (preinclude_file_list != NULL || macro_preinclude_file_list) {
    create_preinclude_pch_event();
  }  /* if */
  /* Simply do a get_token call.  This will return the first token
     of the file that is not a comment or a preprocessing directive.
     Because the prefix information includes only preprocessing directives,
     they will have all been seen by the time the first token is returned.
     The actual work to build the prefix information is done by special
     processing in preproc.c that is enabled when building_pch_prefix is
     TRUE. */
  (void)get_token();
  if (curr_ise != NULL) {
    /* If we didn't reach the end of the source file, pop the input stack
       and close the primary input file. */
    pop_input_stack();
  }  /* if */
  /* Go through the event list and find the last eligible event. */
  find_last_event_to_use();
  /* Reset the state information maintained by the lexical routines. */
  lexical_reset();
  /* Update curr_char_loc to point to the end of the current line.  This
     will force the next token to begin on a new line. */
  building_pch_prefix = FALSE;
  pos_curr_token = saved_pos_curr_token;
  error_position = saved_error_position;
}  /* build_prefix_information */


void process_prefix_pragma_hdrstop(void)
/*
Do processing needed when a pragma hdrstop is found while doing the
prefix scan.
*/
{
  pragma_hdrstop_found = TRUE;
}  /* process_prefix_pragma_hdrstop */


static void open_pch_output_file(void)
/*
Create or truncate the precompiled header file.
*/
{
  a_boolean	cannot_open;
  a_boolean	bad_name;
  char		*file_name;

  if (create_precompiled_header) {
    file_name = pch_output_file_name;
  } else {
    file_name = derived_name(primary_source_file_name, PCH_FILE_SUFFIX);
  }  /* if */
  pch_file_name = build_pch_file_name(file_name);
  if (is_regular_file(pch_file_name)) {
    /* Delete the file before writing it.  This way, if someone already
       has the file open for reading, we won't be overwriting the
       same file that they are reading. */
    delete_file(pch_file_name);
  }  /* if */
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


static a_boolean open_pch_input_file(char *file_name)
/*
Open the PCH input file.  Return TRUE if the file could be opened.
If the file cannot be opened, and the name was explicitly specified by the
user, then issue an error.
*/
{
  f_pch_input = open_input_file(file_name, /*binary_file=*/TRUE);
  if (f_pch_input == NULL && !automatic_pch_processing) {
    /* Only issue an error if the input file was explicitly specified. */
    str_command_line_error(ec_cl_cannot_open_pch_input_file,
                           file_name);
  }  /* if */
  return f_pch_input != NULL;
}  /* open_pch_input_file */


static void remove_pch_input_file(void)
/*
The current input file is not usable for some reason, so remove it.
This may be done because the include files used by the PCH have
changed.
*/
{
  db_enter(3, "remove_pch_input_file");
#if DEBUG
  if (debug_level >= 3) {
    fprintf(f_debug, "Removing PCH file: %s\n", pch_input_file_name);
  }  /* if */
#endif /* DEBUG */
  /* First close the file. */
  if (f_pch_input != NULL) (void)fclose(f_pch_input);
  f_pch_input = NULL;
  delete_file(pch_input_file_name);
  db_exit();
}  /* remove_pch_input_file */


static void remove_assoc_pch_file_if_not_being_used(void)
/*
See if the PCH file being used (if any) is associated with the
file currently being compiled.  If not, remove the associated file.
*/
{
  char		*assoc_pch_file_name;
  a_boolean	remove_file = FALSE;

  db_enter(3, "remove_assoc_pch_file_if_not_being_used");
  /* Append the PCH file prefix to the primary source file base name. */
  assoc_pch_file_name = derived_name(primary_source_file_name,
                                     PCH_FILE_SUFFIX);
  /* Add the PCH directory name. */
  assoc_pch_file_name = build_pch_file_name(assoc_pch_file_name);
  if (!is_regular_file(assoc_pch_file_name)) {
    /* The file does not exist -- nothing to do. */
  } else if (!using_a_pch_file) {
     /* We're not using a PCH file, remove the old one. */
     remove_file = TRUE;
  } else if (compare_file_names(assoc_pch_file_name,
                                pch_input_file_name) != 0) {
    /* The PCH file in use is not associated with this file -- remove the
       associated file. */
    remove_file = TRUE;
  }  /* if */
  if (remove_file) {
#if DEBUG
    if (debug_level >= 3) {
      fprintf(f_debug, "Removing PCH file: %s\n", assoc_pch_file_name);
    }  /* if */
#endif /* DEBUG */
    delete_file(assoc_pch_file_name);
  }  /* if */
  db_exit();
}  /* remove_assoc_pch_file_if_not_being_used */


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
    fwrite_with_check(str, length, f_pch_output);
  } else {
    /* The string pointer is null.  Represent this as a zero length
       string. */
    length = 0;
    pch_write_value(length);
  }  /* if */
}  /* pch_write_string */


static char *pch_read_string(void)
/*
Read a string from the PCH input file.  In the
file, the string consists of a length followed by the actual
characters.  A pointer to the pch_buffer containing the string
is returned.
*/
{
  sizeof_t	length;
  pch_read_value(length);
  ensure_pch_buffer_space(length);
  if (length == 0) {
    /* The original pointer was NULL.  Return a NULL string. */
    pch_buffer[0] = '\0';
  } else {
    /* Read the string.  The length includes the null terminator. */
    fread_with_check(pch_buffer, length, f_pch_input);
  }  /* if */
  return pch_buffer;
}  /* pch_read_string */


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


static a_boolean read_pch_event(a_pch_event_ptr pep)
/*
Read a single PCH event from the PCH input file.  Note that the
string pointer returned points into the pch_buffer (i.e., it exists
only for a short time).  Return TRUE if an event is successfully
read.  Return FALSE when the pchek_none marker at the end of the list
is encountered.
*/
{
  a_boolean		result = FALSE;

  pch_read_value(pep->kind);
  if (pep->kind == pchek_none) {
    /* This is the end of the list. */
  } else {
    result = TRUE;
    switch (pep->kind) {
      case pchek_command_line:
        pch_read_value(pep->variant.cl_option.kind);
        pch_read_value(pep->variant.cl_option.opt_value);
        break;
      case pchek_pp_directive:
        pch_read_value(pep->variant.ppd_kind);
        break;
      default:
        unexpected_condition();
    }  /* switch */
    pep->value = pch_read_string();
    pch_read_value(pep->position);
  }  /* if */
  return result;
}  /* read_pch_event */


static a_boolean equivalent_pch_events(a_pch_event_ptr pep1,
                                       a_pch_event_ptr pep2)
/*
Return TRUE if two PCH events are equivalent.
*/
{
  a_boolean	result = FALSE;
  a_boolean	is_include = FALSE;

  if (pep1->kind == pep2->kind) {
    switch (pep1->kind) {
      case pchek_command_line:
        if (pep1->variant.cl_option.kind == pep2->variant.cl_option.kind) {
          result = pep1->variant.cl_option.opt_value ==
                   pep2->variant.cl_option.opt_value;
        }  /* if */
        break;
      case pchek_pp_directive:
        result = pep1->variant.ppd_kind == pep2->variant.ppd_kind;
        is_include = pep1->variant.ppd_kind == ppd_include;
        break;
      default:
        unexpected_condition();
    }  /* switch */
    /* If the events are equal so far, compare the value strings. */
    if (result) {
      if ((pep1->value == NULL || *(pep1->value) == '\0') &&
          (pep2->value == NULL || *(pep2->value) == '\0')) {
        /* Both value strings are empty so are equivalent.  Leave result
           set to TRUE. */
      } else {
        if (!is_include) {
          result = strcmp(pep1->value, pep2->value) == 0;
        } else {
          /* For include directives, compare the values as file names. */
          /* First make sure the include kind matches (i.e., <...> vs.
             "...".  This is not done in Microsoft bugs mode. */
          result = microsoft_bugs || pep1->value[0] == pep2->value[0];
          if (result) {
            /* Now compare the actual file names. */
            result = f_compare_file_names(pep1->value, pep2->value,
                                          /*ignore_delimiters=*/TRUE,
                                          /*is_partial_file_name=*/TRUE) == 0;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Comparing PCH event: ");
    db_pch_event(pep1);
    fprintf(f_debug, "  with PCH event: ");
    db_pch_event(pep2);
    fprintf(f_debug, "  Equivalent: %s\n", result ? "TRUE" : "FALSE");
  }  /* if */
#endif /* DEBUG */
  return result;
}  /* equivalent_pch_events */


static void write_list_of_file_timestamps(a_source_file_ptr sfp)
/*
Go through a list of source file entries and write the file name and
timestamp to the PCH output file.  Do a recursive call to process any
child files encountered.
*/
{
  db_enter(5, "write_list_of_file_timestamps");
  for (; sfp != NULL; sfp = sfp->next) {
    time_t	mod_time;
    /* Only do this for include files, not for the primary source file
       or for the source file entry associated with a primary source
       file from which precompiled header information has been restored. */
    if (sfp->is_include_file) {
      (void)get_file_modification_time(sfp->full_name, &mod_time);
      pch_write_string(sfp->full_name);
      pch_write_value(mod_time);
#if DEBUG
      if (debug_level >= 5) {
        fprintf(f_debug, "Writing file timestamp for %s, time is %ld\n",
                sfp->full_name, (long)mod_time);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
    if (sfp->first_child_file != NULL) {
      write_list_of_file_timestamps(sfp->first_child_file);
    }  /* if */
  }  /* for */
  db_exit();
}  /* write_list_of_file_timestamps */


static void write_include_file_timestamps(void)
/*
Write a list of include file names and their associated modification times.
In order for this PCH to be used later, none of the includes files may have
changed.
*/
{
  write_list_of_file_timestamps(il_header.primary_source_file);
  /* Write a NULL string to mark the end of the list. */
  pch_write_string((char *)NULL);
}  /* write_include_file_timestamps */


static void write_saved_variables(void)
/*
Save the contents of the variables as specified by the saved
variable lists.
*/
{
  int				i;
  a_pch_saved_variable_ptr	psvp;

  db_enter(4, "write_saved_variables");
  for (i = 0; i < num_of_saved_variable_lists; ++i) {
    for (psvp = saved_variable_array_list[i];
         psvp->var_address != NULL;
         psvp++) {
      a_void_ptr	address = psvp->var_address;
      /* If the indirect flag is set, get the address stored at the
         specified address. */
      if (psvp->indirect) address = *(a_void_ptr*)address;
#if DEBUG
      if (debug_level >= 5) {
        fprintf(f_debug, "Saving %5lu bytes at %p, variable %s %s\n",
                (unsigned long)psvp->var_size, address,
                psvp->var_name == NULL
                  ? "(name not available)"
                  : psvp->var_name,
                psvp->indirect ? "(indirect)" : "");
      }  /* if */
#endif /* DEBUG */
      fwrite_with_check(address, psvp->var_size, f_pch_output);
    }  /* for */
  }  /* for */
  db_exit();
}  /* write_saved_variables */


static void read_saved_variables(void)
/*
Save the contents of the variables as specified by the saved
variable lists.
*/
{
  int				i;
  a_pch_saved_variable_ptr	psvp;

  db_enter(4, "read_saved_variables");
  check_file_section_id(pfs_saved_variables);
  for (i = 0; i < num_of_saved_variable_lists; ++i) {
    for (psvp = saved_variable_array_list[i];
         psvp->var_address != NULL;
         psvp++) {
      a_void_ptr	address = psvp->var_address;
      /* If the indirect flag is set, get the address stored at the
         specified address. */
      if (psvp->indirect) address = *(a_void_ptr*)address;
#if DEBUG
      if (debug_level >= 5) {
        fprintf(f_debug, "Restoring %5lu bytes at %p, variable %s %s\n",
                (unsigned long)psvp->var_size, address,
                psvp->var_name == NULL
                  ? "(name not available)"
                  : psvp->var_name,
                psvp->indirect ? "(indirect)" : "");
      }  /* if */
#endif /* DEBUG */
      fread_with_check(address, psvp->var_size, f_pch_input);
    }  /* for */
  }  /* for */
  db_exit();
}  /* read_saved_variables */


#if USE_MMAP_FOR_MEMORY_REGIONS
static
a_boolean address_can_be_mapped(a_void_ptr	addr,
		                sizeof_t	size)
/*
Check whether the specified address can be used for a memory
mapping operation.  It does this by trying to map a piece of the
IL file to a particular address.  If it succeeds, the mapping is
undone so that the address is available for establishing the real mapping.
*/
{
  a_void_ptr	new_addr;
  a_boolean	successful;

  /* The purpose of this call is simply to establish some kind of
     mapping at the desired address.  The mapping will never be
     used, so it doesn't matter whether or not there is anything at
     that location in the file. */
  new_addr = map_input_file_to_region(f_pch_input, 0, size, addr);
  successful = new_addr != NULL;
  if (successful) unmap_memory(addr, size);
  return successful;
}  /* address_can_be_mapped */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */


static void write_mem_alloc_history(void)
/*
Write the memory allocation history information to the PCH output
file.
*/
{
  db_enter(4, "write_mem_alloc_history");
  pch_write_value(size_of_mem_alloc_history);
  pch_write_value(mem_alloc_history_entries_used);
  fwrite_with_check(mem_alloc_history,
                    sizeof(a_mem_alloc_history) *
                                            mem_alloc_history_entries_used,
                    f_pch_output);
  db_exit();
}  /* write_mem_alloc_history */


static a_boolean read_mem_alloc_history(void)
/*
Read the memory allocation history information from the PCH input file and
restore the memory allocation state.  First, make sure that any memory 
already allocated by the current process matches the corresponding
entries read from the file.  If so, duplicate the remaining entries in the
allocation list so that we know we have reserved the memory needed to
restore the memory regions.
*/
{
  a_boolean			successful = TRUE;
  a_mem_alloc_history_number	new_size;
  a_mem_alloc_history_number	n;
  sizeof_t			bytes_in_new_alloc_history;

  db_enter(4, "read_mem_alloc_history");
  check_file_section_id(pfs_mem_alloc_info);
  /* Read the memory allocation history information.  Read it into
     a separate area so that it can be compared with the existing
     information. */
  pch_read_value(new_size);
  pch_read_value(new_alloc_history_entries);
  bytes_in_new_alloc_history = new_alloc_history_entries *
                                                 sizeof(a_mem_alloc_history);
  new_alloc_history = (a_mem_alloc_history_ptr)alloc_general
                             (bytes_in_new_alloc_history);
  fread_with_check(new_alloc_history,
                   bytes_in_new_alloc_history,
                   f_pch_input);
  /* Make sure the entries for the current compilation match the initial
     entries read from the file. */
  for (n = 0; n < mem_alloc_history_entries_used; ++n) {
    if (!equivalent_mem_alloc_history(mem_alloc_history[n],
                                      new_alloc_history[n])) {
      successful = FALSE;
      break;
    }  /* if */
  }  /* for */
  if (successful) {
    /* The memory allocations that have been done so far are compatible
       with those done in the original compilation.  Perform the
       remaining allocations needed to read in the memory regions. */
    for (; n < new_alloc_history_entries; ++n) {
#if USE_MMAP_FOR_MEMORY_REGIONS
      /* Don't actually allocate the memory, just make sure the address
         space is available for mapping. */
      if (!address_can_be_mapped(new_alloc_history[n].addr,
                                 new_alloc_history[n].size)) {
        successful = FALSE;
        break;
      }  /* if */
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
      /* Allocate the needed block. */
      (void)alloc_new_mem_block(new_alloc_history[n].size);
      if (!equivalent_mem_alloc_history(mem_alloc_history[n],
                                        new_alloc_history[n])) {
        successful = FALSE;
        break;
      }  /* if */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
    }  /* for */
#if 0
    /* How should this memory be freed if a failure occurred during
       allocation? */
#endif /* 0 */
  }  /* if */
  if (!successful) {
    mismatch_reason = ec_memory_mismatch;
    if (automatic_pch_processing && verbose_pch_messages) {
      pos_st_warning(mismatch_reason, &null_source_position,
                     pch_input_file_name);
    }  /* if */
  }  /* if */
  db_exit();
  return successful;
}  /* read_mem_alloc_history */


#if USE_MMAP_FOR_MEMORY_REGIONS
static void write_memory_used_for_memory_regions(void)
/*
Write, to the PCH output file, the contents of the memory that has been
allocated for memory region purposes.  This routine is used when the
memory region information will be accessed using mmap by the consumer of
the PCH file.  The memory is written this way, instead of as individual
memory regions, to avoid the need to write each region at a file offset
that is a multiple of the host page size.
*/
{
  int		i;

  for (i = 0; i < num_of_mem_alloc_history_entries; ++i) {
    a_mem_alloc_history_ptr	mahp = &mem_alloc_history[i];
    (void)seek_to_page_alignment(f_pch_output);
    fwrite_with_check(mahp->addr, mahp->size, f_pch_output);
  }  /* for */
}  /* write_memory_used_for_memory_regions */

#else /* !USE_MMAP_FOR_MEMORY_REGIONS */

static void write_a_memory_region(a_memory_region_number number)
/*
Write the blocks comprising a single memory region to the PCH output
file.  The header information is written out as part of the memory
block.  In order to use the precompiled header, we first guarantee that
the memory blocks used for memory region storage have been allocated
in exactly the same manner as that in which they were created.
*/
{
  a_mem_block_header_ptr	mbhp = mem_region_table[number];
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Writing memory region %d\n", number);
  }  /* if */
#endif /* DEBUG */
  while (mbhp != NULL) {
    sizeof_t	size;
    sizeof_t	region_size;
    size = mbhp->next_avail_in_block - (char *)mbhp;
    region_size = mbhp->after_end_of_block - (char *)mbhp;
    pch_write_value(size);
    pch_write_value(region_size);
    pch_write_value(mbhp);
    fwrite_with_check(mbhp, size, f_pch_output);
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Writing %lu bytes from %p\n", (unsigned long)size,
              mbhp);
    }  /* if */
#endif /* DEBUG */
    mbhp = mbhp->next;
  }  /* while */
}  /* write_a_memory_region */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */


#if USE_MMAP_FOR_MEMORY_REGIONS

static void read_memory_used_for_memory_regions(void)
/*
Read, from the PCH input file, the contents of the memory that has been
allocated for memory region purposes.  This routine is used when the
memory region information will be accessed using mmap by the consumer of
the PCH file.
*/
{
  int		i;
  a_void_ptr	addr;
  sizeof_t	offset;


  /* The memory regions that have already been allocated should be
     unmapped so that the address space is available to be remapped. */
  free_mapped_mem_blocks();
  /* Get the current input file position. */
  offset = ftell(f_pch_input);
  for (i = 0; i < new_alloc_history_entries; ++i) {
    a_mem_alloc_history_ptr	mahp = &new_alloc_history[i];
    offset = do_page_alignment(offset);
    addr = map_input_file_to_region(f_pch_input, offset,
                                    mahp->size, mahp->addr);
    offset += mahp->size;
    if (addr == NULL) {
      /* We were unable to get the desired mapped memory.  This is
         something we can't recover from. */
      error_position = null_source_position;
      catastrophe(ec_unable_to_get_mapped_memory);
    }  /* if */
    /* Create a memory allocation history entry for this block. */
    record_mapped_mem_block(mahp->addr, mahp->size);
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "Mapped bytes from %p for %lu bytes from PCH\n",
              mahp->addr, (unsigned long)mahp->size);
    }  /* if */
#endif /* DEBUG */
  }  /* for */
}  /* read_memory_used_for_memory_regions */

#else /* !USE_MMAP_FOR_MEMORY_REGIONS */

static void read_a_memory_region(a_memory_region_number number)
/*
Read the blocks comprising a single memory region from the PCH input
file.  See write_a_memory_region for more information.
*/
{
  a_mem_block_header_ptr	mbhp = mem_region_table[number];

#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Reading memory region %d\n", number);
  }  /* if */
#endif /* DEBUG */
  for (;;) {
    sizeof_t	size;
    sizeof_t	region_size;
    pch_read_value(size);
    pch_read_value(region_size);
    pch_read_value(mbhp);
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Reading %lu bytes into %p\n", (unsigned long)size,
              mbhp);
    }  /* if */
#endif /* DEBUG */
    fread_with_check((a_stdio_arg)mbhp, size, f_pch_input);
    /* See if this is the last block in the memory region. */
    if (mbhp->next == NULL) break;
  }  /* for */
}  /* read_a_memory_region */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */


static void write_memory_regions(void)
/*
Write the memory region information to the PCH output file.  This includes
header information about the memory regions such as the memory_region_table.
*/
{
  a_memory_region_number	mem_regions_used;
  db_enter(4, "write_memory_regions");
  /* Write a copy of the IL header. */
  pch_write_value(il_header);
  mem_regions_used = highest_used_region_number + 1;
  /* Write the memory region table and the region_scope_entry table from
     the IL header.  Note that index_for_il_file is not written. */
  pch_write_value(highest_used_region_number);
  fwrite_with_check(mem_region_table,
                    sizeof(a_mem_block_header_ptr) * mem_regions_used,
                    f_pch_output);
  fwrite_with_check(il_header.region_scope_entry,
                    sizeof(a_mem_block_header_ptr) * mem_regions_used,
                    f_pch_output);
#if DEBUG
  /* Write the allocated_in_region information. */
  fwrite_with_check(allocated_in_region,
                    sizeof(unsigned long) * mem_regions_used,
                    f_pch_output);
#endif /* DEBUG */
#if USE_MMAP_FOR_MEMORY_REGIONS
  /* When using memory mapping, instead of writing the memory regions one
     at a time, we write the entire memory blocks that contain the
     memory regions.  File sections that are to be accessed later via
     mmap must be written at file offsets that are multiples of the host
     page size.  Writing out individual memory regions in this way
     wastes too much space for alignment. */
  write_memory_used_for_memory_regions();
#else /* USE_MMAP_FOR_MEMORY_REGIONS */
  /* Write the actual memory regions to the PCH file. */
  {
    a_memory_region_number	n;
    for (n = 0; n < mem_regions_used; ++n) {
      write_a_memory_region(n);
    }  /* for */
  }
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  db_exit();
}  /* write_memory_regions */


static void read_memory_regions(void)
/*
Read the memory region information from the PCH output file.  This includes
header information about the memory regions such as the memory_region_table.
*/
{
  a_memory_region_number	mem_regions_used;

  db_enter(4, "read_memory_regions");
  check_file_section_id(pfs_memory_regions);
  /* Read the copy of the IL header. */
  pch_read_value(il_header_from_pch);
  /* Read the memory region table and the region_scope_entry table from
     the IL header.  Note that index_for_il_file is not written. */
  pch_read_value(highest_used_region_number);
  /* Make sure that the tables allocated to store the memory region
     information are large enough. */
  ensure_mem_region_table_space(highest_used_region_number);
  mem_regions_used = highest_used_region_number + 1;
  fread_with_check(mem_region_table,
                  sizeof(a_mem_block_header_ptr) * mem_regions_used,
                  f_pch_input);
  fread_with_check(il_header.region_scope_entry,
                   sizeof(a_scope_ptr) * mem_regions_used,
                   f_pch_input);
#if DEBUG
  /* Read the allocated_in_region information. */
  fread_with_check(allocated_in_region,
                   sizeof(unsigned long) * mem_regions_used,
                   f_pch_input);
#endif /* DEBUG */
#if USE_MMAP_FOR_MEMORY_REGIONS
  /* Read the blocks of memory used for memory region storage.  See
     write_memory_regions for more information. */
  read_memory_used_for_memory_regions();
#else /* USE_MMAP_FOR_MEMORY_REGIONS */
  /* Read the actual memory regions from the PCH file. */
  {
    a_memory_region_number	n;
    for (n = 0; n < mem_regions_used; ++n) {
      if (mem_region_table[n] != NULL) {
        /* A NULL memory region table entry will not have any entries written
           to the file.  Don't try to read the memory region in this case. */
        read_a_memory_region(n);
      }  /* if */
    }  /* for */
  }
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  db_exit();
}  /* read_memory_regions */


static void write_precompiled_header_file(void)
/*
Create a precompiled header file for the compilation up to the
current point.
*/
{
  a_boolean	is_complete = FALSE;
  long		flag_position;
  open_pch_output_file();
  pch_message(ec_creating_pch, pch_file_name);
#if DEBUG
  if (debug_level >= 3) {
    a_pch_event_ptr	pep;
    fprintf(f_debug, "Events to be recorded in %s:\n", pch_file_name);
    for (pep = pch_event_list_head; pep != NULL; pep = pep->next) {
      db_pch_event(pep);
    }  /* for */
  }  /* if */
#endif /* DEBUG */
  /* Write the string that identifies this file as a precompiled header
     file. */
  fwrite_with_check(pch_id_string, pch_id_string_length, f_pch_output);
  /* Write a FALSE to the file, this will later be changed to TRUE after
     the file has been completely written.  This is done to prevent a
     partially written PCH file from being used. */
  flag_position = ftell(f_pch_output);
  pch_write_value(is_complete);
  /* Current directory name. */
  pch_write_string(current_directory_name);
  /* The directory name associated with the primary source file. */
  pch_write_string(directory_of(primary_source_file_name));
  /* Write the event list that will be used for PCH file matching. */
  write_file_section_id(pfs_cmd_line_events);
  write_pch_events(pch_cmd_line_event_list_head);
  write_file_section_id(pfs_other_events);
  write_pch_events(pch_event_list_head);
  /* Write dependency checking information. */
  /* Include file names and timestamps. */
  write_file_section_id(pfs_include_file_info);
  write_include_file_timestamps();
  /* Write the memory allocation history information. */
  write_file_section_id(pfs_mem_alloc_info);
  write_mem_alloc_history();
  /* Write the compilation state to be restored. */
  write_file_section_id(pfs_saved_variables);
  write_saved_variables();
  /* Write the memory region information. */
  write_file_section_id(pfs_memory_regions);
  write_memory_regions();
  /* Write the flag that indicates that the PCH file is now complete. */
  fseek_with_check(f_pch_output, flag_position, SEEK_SET);
  is_complete = TRUE;
  pch_write_value(is_complete);
  (void)fclose(f_pch_output);
}  /* write_precompiled_header_file */


#if DEBUG
static void db_cannot_generate_reason(char *str)
/*
Display a debugging message explaining why a precompiled header file
can't be generated.
*/
{
  if (debug_level >= 2) {
    fprintf(f_debug, "Cannot generate precompiled header: %s\n", str);
  }  /* if */
}  /* db_cannot_generate_reason */
#else /* !DEBUG */
#define db_cannot_generate_reason(str) /* Nothing */
#endif /* !DEBUG */



void generate_precompiled_header(void)
/*
Processing has reached the "header stop" point.  Check for conditions that
would prevent generation of a precompiled header file, and if none exists,
write out the precompiled header file.
*/
{
  db_enter(2, "generate_precompiled_header");
  check_assertion(header_stop_position_pending);
  if (cannot_create_pch_file) {
    /* Some condition was encountered that makes creation of a precompiled
       header impossible. */
    db_cannot_generate_reason("cannot_create_pch_file is set");
#if !USE_MMAP_FOR_MEMORY_REGIONS
    if (exhausted_preallocated_memory) {
      char	size_string[20];
      sizeof_t	size_needed;
      /* Compute the amount of preallocated memory needed in K (1024) byte
         units. */
      size_needed = HOST_ALLOCATION_INCREMENT * total_mem_blocks_allocated;
      size_needed = (size_needed / 1024) + 1;
      /* Convert the size needed to a string. */
      (void)sprintf(size_string, "%luK", (unsigned long)size_needed);
      str_warning(ec_not_enough_preallocated_memory, size_string);
    } else if (large_mem_block_needed) {
      pos_warning(ec_program_entity_too_large_for_pch,
                     &large_mem_block_error_pos);
    }  /* if */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
  } else if (!next_token_is_top_level_decl_start) {
    /* We are in the middle of a construct.  We must be between top level
       declarations in order to generate a precompiled header. */
    db_cannot_generate_reason("not between top level declarations");
  } else if (depth_scope_stack != DEPTH_OF_FILE_SCOPE) {
    /* Don't save the header files if we are not currently at file scope. */
    db_cannot_generate_reason("not at file scope");
  } else if (macro_depth != 0 || pp_if_stack_depth != -1) {
    /* Nor if we are in the midst of a macro definition or a #if construct. */
    db_cannot_generate_reason("in a macro or #if");
  } else if (total_errors > 0) {
    /* Nor if there have been errors. */
    db_cannot_generate_reason("there have been errors");
  } else if (scope_stack[DEPTH_OF_FILE_SCOPE].name_linkage_is_explicit) {
    /* Nor if we are in the middle of a linkage specifier block. */
    db_cannot_generate_reason("in a linkage block");
  } else {
    /* The state justifies creating a precompiled header. */
    check_assertion(curr_il_region_number == FILE_SCOPE_REGION_NUMBER);
    check_assertion(depth_stmt_stack == -1);
    /* Be sure that the overhead in generating a precompiled header is
       justified "quantitatively". */
    if (decl_seq_counter < PCH_DECL_SEQ_THRESHOLD) {
      /* There haven't been enough declarations to justify writing out and
         restoring the header information. */
      db_cannot_generate_reason("too few declarations");
    } else {
      /* Okay -- go ahead and do it. */
      write_precompiled_header_file();
    }  /* if */
  }  /* if */
  db_exit();
}  /* generate_precompiled_header */


void header_stop_no_longer_pending(void)
/*
This routine is called when we are no longer generating information that
may potentially be part of a precompiled header.  We may have just generated
a precompiled header file, or we may have determined that generating one is
not possible.  This routine goes back of the memory regions that have already
been completed and calls check_for_done_with_memory_region so that the IL
file can be written (if needed) and the memory freed (if appropriate).
*/
{
  a_memory_region_number	n;

  db_enter(3, "header_stop_no_longer_pending");
  header_stop_position_pending = FALSE;
  /* Loop through the memory regions.  Skip the front end and file scope
     memory regions. */
  for (n = FILE_SCOPE_REGION_NUMBER + 1;
       n <= highest_used_region_number; ++n) {
    if (mem_region_table[n] == NULL) {
      /* This memory region has already been freed. */
    } else {
      a_scope_ptr		sp;
      sp = il_header.region_scope_entry[n];
      if (sp->depth_in_scope_stack != NO_SCOPE_DEPTH) {
        /* The scope is still active and will be processed when it is
           popped off of the scope stack. */
      } else {
        /* The scope is no longer active. */
        check_for_done_with_memory_region(n);
      }  /* if */
    }  /* if */
  }  /* for */
  free_unused_pch_memory();
  db_exit();
}  /* header_stop_no_longer_pending */


static a_boolean id_string_matches(void)
/*
Make sure that the ID string in the candidate PCH file matches the current
version.  This routine also makes sure that the is_complete flag in the
header has been set indicating that the PCH file was successfully
written.
*/
{
  a_boolean	match = FALSE;
  a_boolean	is_complete;

  /* We don't use fread_with_check here because we want to handle
     read errors more gracefully.  After all, we don't yet know
     that is actually a PCH written by this compiler. */
  if (!fread_with_status(pch_buffer, size_t_arg(pch_id_string_length),
                         f_pch_input)) {
    /* The read failed -- the file must contain something unexpected. */
  } else {
    /* The read succeeded, see if the ID string matches. */
    if (strcmp(pch_buffer, pch_id_string) == 0) {
      match = TRUE;
    }  /* if */
  }  /* if */
  if (!match) {
    mismatch_reason = ec_invalid_pch_file;
  }  /* if */
  if (!fread_with_status(&is_complete, sizeof(is_complete), f_pch_input)) {
    /* The read failed - the file must not be complete. */
    is_complete = FALSE;
  }  /* if */
  return match && is_complete;
}  /* id_string_matches */


static a_boolean curr_dir_matches(void)
/*
Return TRUE if the next string in the candidate PCH file matches
the current directory.
*/
{
  char		*ptr;
  a_boolean	result;
  ptr = pch_read_string();
  result = strcmp(ptr, current_directory_name) == 0;
  if (!result) {
    mismatch_reason = ec_pch_curr_directory_changed;
  }  /* if */
  ptr = pch_read_string();
  if (result) {
    /* If the current directory matches, check the directory associated
       with the primary source file. */
    result = compare_dir_names(ptr,
                               directory_of(primary_source_file_name),
                               /*is_partial_file_name=*/FALSE) == 0;
    if (!result) {
      /* A mismatch of the primary source file is diagnosed as a command
	 line option mismatch. */
      mismatch_reason = ec_pch_cmd_line_option_mismatch;
    }  /* if */
  }  /* if */
  return result;
}  /* curr_dir_matches */


static a_boolean include_files_have_not_changed(void)
/*
Read the include file timestamp information from the PCH input file
and make the modification times match the current values for the files.
*/
{
  a_boolean	match = TRUE;

  check_file_section_id(pfs_include_file_info);
  for (;;) {
    char	*file_name;
    time_t	time_from_file;
    time_t	curr_time;
    /* Read the file name. */
    file_name = pch_read_string();
    /* A null string marks the end of the list. */
    if (*file_name == '\0') break;
    /* Read the modification time. */
    pch_read_value(time_from_file);
    if (!get_file_modification_time(file_name, &curr_time) ||
        time_from_file != curr_time) {
      /* Either the file does not exist or the modification time has changed.
         Note that we require the times to be identical, so even if the include
         file seems to be older than the last one we still consider it to be
         a change. */
      mismatch_reason = ec_pch_header_files_have_changed;
      match = FALSE;
      break;
    }  /* if */
  }  /* for */
  if (!match && automatic_pch_processing) {
    /* The include files are obsolete.  Remove the file. */
    remove_pch_input_file();
  }  /* if */
  return match;
}  /* include_files_have_not_changed */


static a_boolean cmd_line_events_match(void)
/*
Compare the command line event list of the current file with the
command line events in the candidate PCH file.  Return TRUE if they
all match.
*/
{
  a_pch_event_ptr	pep;
  a_pch_event		event;
  a_boolean		match = TRUE;

  db_enter(4, "cmd_line_events_match");
  check_file_section_id(pfs_cmd_line_events);
  /* Loop through the command line events until one that does not match
     is found. */
  for (pep = pch_cmd_line_event_list_head; pep != NULL; pep = pep->next) {
    if (!read_pch_event(&event) || !equivalent_pch_events(pep, &event)) {
      /* Either the event list of the file has ended, or the events are
         not equivalent. */
      match = FALSE;
      break;
    }  /* if */
  }  /* for */
  /* There should be no more events in the PCH input file.  If there are
     any, then there is a mismatch. */
  if (match && read_pch_event(&event)) {
    match = FALSE;
  }  /* if */
  if (!match) {
    mismatch_reason = ec_pch_cmd_line_option_mismatch;
  }  /* if */
  db_exit();
  return match;
}  /* cmd_line_events_match */


static a_pch_event_ptr compare_event_lists(void)
/*
Compare the event list of the current file with the list in the candidate
pch file.  Return a pointer to the last matching event.
*/
{
  a_pch_event_ptr	pep;
  a_pch_event_ptr	last_matching_event = NULL;
  a_pch_event_ptr	pos_in_event_list;
  a_pch_event		event;
  a_boolean		match;
  
  db_enter(4, "compare_event_lists");
  check_file_section_id(pfs_other_events);
  /* Clear the match_found flags in the event list for the current file. */
  for (pep = pch_event_list_head; pep != NULL; pep = pep->next) {
    pep->match_found = FALSE;
  }  /* for */
  pos_in_event_list = pch_event_list_head;
  /* Read each of the events from the candidate file until a mismatch is
     found. */
  match = TRUE;
  while (read_pch_event(&event)) {
    a_boolean	is_define = FALSE;
    a_boolean	event_matches = FALSE;
#if DEBUG
    if (debug_level >= 4) {
      db_pch_event(&event);
      if (pos_in_event_list == NULL) {
        fprintf(f_debug, "Candidate event list longer than current file\n");
      }  /* if */
    }  /* if */
#endif /* DEBUG */
    if (pos_in_event_list == NULL) {
      /* We've reached the end of the event list for this file but there
         are still more events in the candidate file.  Terminate the
         scan of this candidate file. */
      match = FALSE;
      break;
    }  /* if */
    switch (event.kind) {
      case pchek_command_line:
        unexpected_condition();
        break;
      case pchek_pp_directive:
        is_define = event.variant.ppd_kind == ppd_define;
        break;
      default:
        unexpected_condition();
    }  /* switch */
    if (is_define) {
      /* Look for a matching #define in a sequence of defines. */
      pep = pos_in_event_list;
      while (pep != NULL && pep->kind == pchek_pp_directive &&
             pep->variant.ppd_kind == ppd_define) {
        if (pep->match_found) {
          /* A match has already been found for this define.  It can't
             be used again. */
        } else {
          if (equivalent_pch_events(pep, &event)) {
            /* This event matches on of the defines.  Set the flag that
               indicates that this match has been used and exit the loop. */
            pep->match_found = TRUE;
            event_matches = TRUE;
            break;
          }  /* if */
        }  /* if */
        pep = pep->next;
      }  /* while */
    } else {
      /* This is not a #define, this event must match exactly.  Before
         checking, see if we need to skip over one or more #define events
         that have already been matched. */
      pep = pos_in_event_list;
      while (pep != NULL && pep->kind == pchek_pp_directive &&
             pep->variant.ppd_kind == ppd_define && pep->match_found) {
        pep = pep->next;
      }  /* while */
      /* The events must match exactly. */
      if (pep != NULL && equivalent_pch_events(pep, &event)) {
        event_matches = TRUE;
        last_matching_event = pep;
        pos_in_event_list = pep->next;
      }  /* if */
    }  /* if */
    if (!event_matches) {
      /* If this event doesn't match, then terminate the scan of this
         candidate file. */
      match = FALSE;
      break;
    }  /* if */
  }  /* while */
  /* Skip past any matched #define entries that were not followed
     by some other directive. */
  pep = pos_in_event_list;
  while (pep != NULL && pep->kind == pchek_pp_directive &&
         pep->variant.ppd_kind == ppd_define && pep->match_found) {
    last_matching_event = pep;
    pep = pep->next;
  }  /* while */
#if 0
  /* This test only applies if you can't both create and use
     PCH files in the same compilation. */
  if (match && pragma_hdrstop_found) {
    /* If the file contains a pragma hdrstop, don't accept an existing
       file whose prefix falls short of the hdrstop. */
    if (last_matching_event != pch_event_list_tail) {
      match = FALSE;
    }  /* if */
  }  /* if */
#endif /* 0 */
  /* If the candidate file doesn't match, return a NULL to the caller. */
  if (!match) {
    last_matching_event = NULL;
    mismatch_reason = ec_pch_file_prefix_mismatch;
  }  /* if */
  db_exit();
  return last_matching_event;
}  /* compare_event_lists */


static a_pch_event_ptr pch_is_applicable(void)
/*
Compares the precompiled header referred to by the f_pch_input
pointer with the current event list to see if it matches.  If
it so, it checks the include file timestamps to see if the files
are up to date.  If the PCH can be used, a pointer to the last
matching event is returned.
*/
{
  a_pch_event_ptr	last_matching_event = NULL;

  db_enter(3, "pch_is_applicable");
  mismatch_reason = ec_no_error;
  if (id_string_matches() && curr_dir_matches()) {
    /* This is a valid precompiled header -- see if the event lists match. */
    if (cmd_line_events_match()) {
      last_matching_event = compare_event_lists();
      if (last_matching_event != NULL) {
        if (include_files_have_not_changed()) {
          /* The include files have not changed.  We can use this PCH. */
        } else {
          /* This PCH cannot be used because the include files have changed. */
          last_matching_event = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return last_matching_event;
}  /* pch_is_applicable */


static a_boolean find_applicable_pch(void)
/*
Compare the prefix information for this file with the prefix
information for the other precompiled headers in the current
directory.  Return TRUE if an applicable PCH was found.
*/
{
  a_boolean		first;
  char			*file_name;
  a_source_position	best_result_so_far;
  a_boolean		is_applicable;
  a_boolean		result = FALSE;
  a_boolean		skip_remaining_entries = FALSE;

  db_enter(3, "find_applicable_pch");
#if DEBUG
  if (debug_level >= 4) {
    a_pch_event_ptr	pep;
    fprintf(f_debug, "Event list of this file:\n");
    for (pep = pch_event_list_head; pep != NULL; pep = pep->next) {
      db_pch_event(pep);
    }  /* for */
  }  /* if */
#endif /* DEBUG */
  best_result_so_far = null_source_position;
  for (first = TRUE;;first = FALSE) {
    a_pch_event_ptr	last_matching_event;
    file_name = get_file_name_from_dir(first, pch_dir_name, PCH_FILE_SUFFIX,
                                       current_directory_name);
    /* A NULL pointer indicates there are no more matching file names. */
    if (file_name == NULL) break;
    /* If we've found an optimal PCH file, don't bother looking at more
       entries.  We still read the directory entries so that the
       directory will be closed after all entries have been read. */
    if (skip_remaining_entries) continue;
    /* Append the PCH directory name to the file name. */
    file_name = build_pch_file_name(file_name);
    /* The open routine will also make sure that it is a regular file. */
    if (!open_pch_input_file(file_name)) continue;
    pch_input_file_name = file_name;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Checking %s for applicability\n", file_name);
    }  /* if */
#endif /* DEBUG */
    /* See if this PCH file can be used. */
    last_matching_event = pch_is_applicable();
    if (f_pch_input != NULL) (void)fclose(f_pch_input);
    is_applicable = last_matching_event != NULL;
    if (is_applicable) result = TRUE;
#if DEBUG
    if (debug_level >= 1) {
      fprintf(f_debug, "PCH file %s, applicable: %s",
              file_name, is_applicable ? "TRUE" : "FALSE");
      if (is_applicable) {
        fprintf(f_debug, ", seq: %lu, column: %lu\n",
                (unsigned long)last_matching_event->position.seq,
                (unsigned long)last_matching_event->position.column);
      } else {
        fprintf(f_debug, "\n");
        if (db_active) {
          pos_st_warning(mismatch_reason, &null_source_position, file_name);
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* DEBUG */
    if (is_applicable) {
      /* See if this precompiled header matches an event later in the source
         file than the previous best. */
      if (cmp_source_positions(last_matching_event->position,
                               best_result_so_far) >= 0) {
        sizeof_t	file_name_length = strlen(file_name);
        best_result_so_far = last_matching_event->position;
        /* Make sure that the file name buffer is large enough to hold
           the new file name. */
        ensure_file_name_buffer_space(file_name_buffer, file_name_length+1);
        (void)strcpy(file_name_buffer.name, file_name);
        /* If this file provides all possible events, don't bother
           inspecting any other files. */
        skip_remaining_entries = last_matching_event == pch_event_list_tail;
      }  /* if */
    } else {
      if (verbose_pch_messages) {
        /* Issue a warning that the precompiled header cannot be used. */
        pos_st_warning(mismatch_reason, &null_source_position, file_name);
      }  /* if */
    }  /* if */
  }  /* for */
  if (result) {
    /* Save a copy of the precompiled header file name to be used. */
    pch_input_file_name = 
           (char *)alloc_general((sizeof_t)strlen(file_name_buffer.name) + 1);
    (void)strcpy(pch_input_file_name, file_name_buffer.name);
  }  /* if */
  db_exit();
  return result;
}  /* find_applicable_pch */


static void pch_fixup_part_1(void)
/*
This routine is called after restoring the memory region information
from the PCH file.  It updates the IL header (which is not restored
from the PCH file) to reflect the information loaded from the file.
*/
{
  a_source_file_ptr	orig_sfp;

  db_enter(3, "pch_fixup_part_1");
  orig_sfp = il_header_from_pch.primary_source_file;
  /* Make the source file pointer for the file that created the
     precompiled header file the first child file of the new source
     file.  This allows diagnostics that reference lines that come
     from the precompiled header to work properly. */
  il_header.primary_source_file = orig_sfp;
  il_header.primary_scope = il_header_from_pch.primary_scope;
  il_header.main_routine = il_header_from_pch.main_routine;
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  il_header.scope_orphaned_list_headers =
                              il_header_from_pch.scope_orphaned_list_headers;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_MACROS_IN_IL
  il_header.macros = il_header_from_pch.macros;
#endif /* RECORD_MACROS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
  il_header.number_of_external_nonclass_template_entities =
              il_header_from_pch.number_of_external_nonclass_template_entities;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* Rebuild the trans_unit_for_scope table.  This is done by calling
     take_next_scope_number the appropriate number of times. */
  {
    a_scope_number	number_of_scopes = next_scope_number;
    for (next_scope_number = 0; next_scope_number < number_of_scopes;) {
      (void)take_next_scope_number();
    }  /* for */
  }
  /* Clear the stop tokens array that was restored. */
  clear_stop_tokens();
  db_exit();
}  /* pch_fixup_part_1 */


void pch_fixup_part_2(void)
/*
This routine is called when we have reached the point in the current source
file at which we make the transition from information supplied by the PCH
to information generated as a result of this compilation.  This routine
updates the source file pointer in the IL header to reflect the information
loaded from the PCH file.  This needs to be done in both part 1 and 2
of the fixup because a new source file entry for the primary source file
is created when the primary source file is reopened between the two fixups.

*/
{
  a_source_file_ptr	sfp;
  a_source_file_ptr	orig_sfp;

  db_enter(3, "pch_fixup_part_2");
  building_pch_prefix = FALSE;
  next_event_resumes_compilation = FALSE;
  sfp = il_header.primary_source_file;
  orig_sfp = il_header_from_pch.primary_source_file;
  /* Make the source file pointer for the file that created the
     precompiled header file the first child file of the new source
     file.  This allows diagnostics that reference lines that come
     from the precompiled header to work properly. */
  sfp->first_child_file = orig_sfp;
  sfp->last_child_file = orig_sfp;
  sfp->first_seq_number = 1;
  /* Set the ending sequence number for what was the primary source
     file when the precompiled header was generated. */
  orig_sfp->last_seq_number = saved_curr_seq_number;
  db_exit();
}  /* pch_fixup_part_2 */


static void restore_precompiled_header_information(void)
/*
Reload the compiler state information so that a precompiled header file
may be used.
*/
{
  a_boolean			can_use_pch = TRUE;
  a_pch_event_ptr		last_event_from_pch;

  db_enter(2, "restore_precompiled_header_information");
  if (!automatic_pch_processing) {
    sizeof_t	size;
    char	*name_with_dir;
    /* In non-automatic mode, the input file name will not yet have
       had the PCH directory name added.  Do it now. */
    pch_input_file_name = build_pch_file_name(pch_input_file_name);
    /* Make a copy of the name in general memory. */
    size = strlen(pch_input_file_name) + 1;
    name_with_dir = strcpy((char *)alloc_general(size), pch_input_file_name);
    pch_input_file_name = name_with_dir;
  }  /* if */
  if (open_pch_input_file(pch_input_file_name)) {
    /* Make sure the PCH can still be used.  Also make sure that
       the memory configuration needed by the PCH is compatible with
       what we can allocate. */
    last_event_from_pch = pch_is_applicable();
#if EDG_WIN32 && USE_MMAP_FOR_MEMORY_REGIONS
    /* Open the file a second time in a way that it can be used for
       file mapping purposes. */
    open_mapped_input_file(pch_input_file_name);
#endif /* EDG_WIN32  && USE_MMAP_FOR_MEMORY_REGIONS */
    if (last_event_from_pch != NULL && read_mem_alloc_history()) {
      /* Everything is OK. */
      pos_of_last_event_from_pch = last_event_from_pch->position;
    } else {
      /* The file is not applicable for some reason. */
      can_use_pch = FALSE;
      if (!automatic_pch_processing) {
        /* Issue a warning that the precompiled header cannot be used. */
        pos_st_warning(mismatch_reason, &null_source_position,
                       pch_input_file_name);
      } else {
        /* Something must have changed since was last read the PCH file.
           Silently suppress use of the PCH file. */
      }  /* if */
    }  /* if */
  }  /* if */
  if (can_use_pch) {
    pch_message(ec_using_pch, pch_input_file_name);
    using_a_pch_file = TRUE;
    read_saved_variables();
    read_memory_regions();
#if EDG_WIN32 && USE_MMAP_FOR_MEMORY_REGIONS
    close_mapped_input_file();
#endif /* EDG_WIN32  && USE_MMAP_FOR_MEMORY_REGIONS */
    if (new_alloc_history != NULL) {
      /* Free the new allocation history information. */
      free_general((a_void_ptr)new_alloc_history,
                   (sizeof_t)(new_alloc_history_entries *
                                             sizeof(a_mem_alloc_history)));
    }  /* if */
    /* Save the sequence number as of this point.  seq_number_last_read is
       used rather than curr_seq_number because in some cases curr_seq_number
       may not have been updated to reflect the latest seq_number read. */
    saved_curr_seq_number = seq_number_last_read;
    /* Update the IL header to reflect the information in the PCH file. */
    pch_fixup_part_1();
  }  /* if */
  if (f_pch_input != NULL) (void)fclose(f_pch_input);
  db_exit();
}  /* restore_precompiled_header_information */


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
  a_boolean	applicable_pch_found = FALSE;

  db_enter(2, "precompiled_header_processing");
  /* We have not encountered a condition that would prevent us from
     using a precompiled header. */
  build_prefix_information();
  if (cannot_do_pch_processing) {
    /* Something happened that makes it impossible to generate or
       use a precompiled header. */
  } else {
    if (automatic_pch_processing) {
      applicable_pch_found = find_applicable_pch();
    }  /* if */
    if (use_precompiled_header ||
        (automatic_pch_processing && applicable_pch_found)) {
      restore_precompiled_header_information();
    }  /* if */
    if (automatic_pch_processing) {
      /* If we are not using the PCH file associated with this source file
         (if one exists), then remove it.  It must be obsolete for some
         reason. */
      remove_assoc_pch_file_if_not_being_used();
    }  /* if */
    /* See if we can create a precompiled header file. */
    if (automatic_pch_processing || create_precompiled_header) {
      if (cmp_source_positions(header_stop_source_position,
                               null_source_position) != 0) {
        /* Only generate one if a header stop position was found. */
        if (!using_a_pch_file ||
            (cmp_source_positions(header_stop_source_position,
                                  pos_of_last_event_from_pch) > 0)) {
          /* If we are also using a precompiled header file, make sure that
             the new header stop position is beyond what is being obtained from
             the PCH input file. */
          header_stop_position_pending = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (!header_stop_position_pending) {
      /* We are not attempting to generate a precompiled header.
         Call header_stop_no_longer_pending to process any memory regions
         that may have been restored from a PCH (if one is being used).
         If a PCH is not being used, this will free any specially allocated
         PCH memory. */
      header_stop_no_longer_pending();
    }  /* if */
  }  /* if */
  db_exit();
}  /* precompiled_header_processing */


void register_pch_saved_variables(a_pch_saved_variable array[])
/*
Save the address of a list of variables to be saved when a PCH
file is created and restored when a PCH file is used.
*/
{
  check_assertion_str2
            (num_of_saved_variable_lists < MAX_NUMBER_OF_SAVED_VARIABLE_LISTS,
             "register_pch_saved_variables:",
             "too many saved variable lists");
  saved_variable_array_list[num_of_saved_variable_lists++] = array;
}  /* register_pch_saved_variables */


#if DEBUG
unsigned long db_show_pch_space_used(unsigned long grand_total)
/*
Show space used by the PCH routines.  This is called by
the symbol table space used routine.  The space used by the PCH
routines is reported as part of the symbol table memory used.
*/
{
  unsigned long	num;
  unsigned long	size;
  unsigned long	total;

  db_space_used("PCH events", num_pch_events_allocated, a_pch_event);
  return grand_total;
}  /* db_show_template_space_used */
#endif /* DEBUG */


void pch_init(void)
/*
Initialize variables used by the precompiled header routines.
*/
{
  static a_boolean one_time_init_done = FALSE;
  db_enter(4, "pch_init");
#if DEBUG
  check_assertion(strcmp(pch_event_kind_names[(int)pchek_last], "last") == 0);
#endif /* DEBUG */
  if (!one_time_init_done) {
    pch_one_time_init();
    one_time_init_done = TRUE;
  }  /* if */
  initialize_pch_id_string();
  cannot_do_pch_processing = FALSE;
  cannot_create_pch_file = FALSE;
  pch_event_list_head = NULL;
  pch_event_list_tail = NULL;
  building_pch_prefix = FALSE;
  header_stop_source_position = null_source_position;
  header_stop_position_pending = FALSE;
  next_event_resumes_compilation = FALSE;
  generate_pch_on_return_to_primary_source_file = FALSE;
  pragma_hdrstop_found = FALSE;
  pos_of_last_event_from_pch = null_source_position;
  using_a_pch_file = FALSE;
  file_name_text_buffer = NULL;
#if DEBUG
  num_pch_events_allocated = 0;
#endif /* DEBUG */
  /* Check for conditions that make it impossible to do precompiled header
     processing. */
  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) == 0) {
    /* PCH processing must be able to restart the scan of the primary
       source file.  This can't be done with standard input, so we have
       to suppress PCH processing. */
    abandon_pch_processing();
  }  /* if */

  db_exit();
}  /* pch_init */


static void pch_one_time_init(void)
{
  pch_buffer = (char *)alloc_general(PCH_BUFFER_INITIAL_ALLOCATION);
  size_pch_buffer = PCH_BUFFER_INITIAL_ALLOCATION;
  /* Do initial allocation of the file name buffer. */
  ensure_file_name_buffer_space(file_name_buffer, 1);
}  /* pch_one_time_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
