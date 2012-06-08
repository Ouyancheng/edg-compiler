/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

error.c -- Error reporting routines.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "err_data.h"
#include "templates.h"
#if !STANDALONE_UTILITY_PROGRAM
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#include "pch.h"
#if MACRO_INVOCATION_TREE_IN_IL
#include "macro.h"
#endif /* MACRO_INVOCATION_TREE_IN_IL */
#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Static variable set when a catastrophe occurs, to catch catastrophe loops.
*/
static a_boolean catastrophe_has_occurred;

/*
Constants, structures and static variables used to format diagnostic
messages.
*/

#define NORMAL_DIAG_INDENT 0	/* The number of spaces to be indented prior
				   to conventional single message
				   diagnostics. */
#define SOURCE_INDENT 2		/* The number of spaces by which source lines
				   are indented. */
#define INDENT_AMOUNT 10	/* Number of additional spaces at the start of
				   continuation lines. */
#define LIST_DIAG_INDENT 12	/* The number of spaces to be indented prior
				   to each additional message that is part of
				   a multiple message diagnostic, i.e. an
				   error diagnostic followed by a list of
				   entities.  For appearances, this value
				   should be greater than INDENT_AMOUNT. */
#define MACRO_CONTEXT_INDENT 1	/* The number of spaces to indent macro
				   context lines.  For best appearances, this
				   value should be greater than
				   NORMAL_DIAG_INDENT and less than
				   SOURCE_INDENT. */

static int	diagnostic_indent;
				/* Typically all diagnostic messages will
				   begin in the first column of a line and
				   subsequent continuation lines would be
				   indented.  With diagnostics involving
				   multiple messages or entity names, this
				   static variable will be adjusted for the
				   start of each additional message.  See
				   NORMAL_DIAG_INDENT and LIST_DIAG_INDENT
				   above. */
static a_boolean
		context_required = FALSE;
				/* TRUE if context information (such as
				   information about templates currently
				   being instantiated) is required after
				   an error message is issued. */

#if !STANDALONE_UTILITY_PROGRAM

static a_source_file_ptr
		diag_primary_source_file;
				/* Pointer to the primary source file
				   associated with the diagnostic currently
				   being processed. */

#endif /* !STANDALONE_UTILITY_PROGRAM */
				   
static a_text_buffer_ptr
		write_diagnostic_buffer;
				/* A text buffer used by write_diagnostic to
				   accumulate the entire contents of a
				   diagnostic message (so it can be written
				   in one atomic operation).  The text buffer
				   is not null-terminated during the
				   construction of the diagnostic. */

static a_text_buffer_ptr
		write_message_buffer;
				/* A text buffer used by write_message. */

static int      write_diagnostic_recursion_level;
				/* An indication of the recursion level
				   of write_diagnostic. */

/*
Category codes for the various parts of the processing for multi-line
diagnostics:
*/
typedef enum a_diagnostic_category_kind_tag {
  dck_standalone,		/* The solitary diagnostic for an error
				   position, to be formatted with source
				   file, line number and source line, if
				   available. */
  dck_primary,			/* The beginning or primary message of a
				   multi-message diagnostic.  The source file
				   and line number are printed with
				   this message.  The source line, if
				   available, will be printed following
				   the list of associated messages. */
  dck_list,			/* Additional message in a multi-message
				   diagnostic.  This message will typically
                                   be indented relative to its associated
				   primary message.  Source file information
				   is suppressed. */
  dck_end_list,			/* Signifies the end of a list of messages and
				   that source line, if available, should be
				   outputted.  There is no actual diagnostic
				   text associated with this category. */
  dck_context_primary,		/* The beginning or primary message of a
				   multi-line diagnostic that specifies
				   error context information.  This is similar
				   to dck_primary except that the source
				   line, location, and severity are not
				   printed (because they were printed
				   as part of the original message). */
  dck_end_context,		/* Like dck_end_list except that the source
				   line is not output. */
  dck_macro_context		/* A line in the macro context stack trace. */
} a_diagnostic_category_kind;

#define BASE_MSG_SEGMENT_SIZE 100
				/* The starting length of a formatted
				   message segment. */
#define INCR_MSG_SEGMENT_SIZE BASE_MSG_SEGMENT_SIZE
				/* The increment size to be used to lengthen
				   a message seqment. */

/*
An error message being formed is represented by a linked list of message
segment descriptors, one for each part of the error message text or fill-in.
*/
enum a_message_segment_kind_tag {
/* Kind of error message segment (e.g. part of text, symbol name, or type).
*/
  msk_error_text_part,		/* Textual part of an error message. */
  msk_user_string,		/* User provided string insert. */
  msk_type,			/* Type to be expanded in the message */
  msk_symbol,			/* Symbol name to be expanded in the
				   message at this point. */
  msk_source_position,		/* Source position to be inserted in the
				   message. */
  msk_last			/* Termination of the current message
				   being formatted.  This should be the last
				   message segment kind. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte a_message_segment_kind;

typedef struct a_msg_segment *a_msg_segment_ptr;
typedef struct a_msg_segment {
  a_msg_segment_ptr
		next;		/* Pointer to the next message segment. */
  char		*segment;	/* Pointer to the message segment buffer. */
  char		*first_quote;	/* Pointer to the first double quote in the 
				   segment.  NULL if none. */
  char		*second_quote;	/* Pointer to the second double quote in the 
				   segment.  NULL if none. */
  uint32_t	length;		/* Current length of the message segment. */
  uint32_t	max_length;	/* Maximum string size that can be accommodated
				   in the message segment buffer. */
  short		sequence_no;	/* Sequence number of the user string, type,
				   source position, or symbol name in the
				   error message.  This field is meaningless
				   for kind == msk_error_text_part. */
  a_message_segment_kind
		kind;		/* The kind of this message segment. */
  union {
    /* When kind == msk_error_text_part: */
    char 	*msg_part;	/* Pointer into the error message text to the
				   start of this portion of the error.  The
				   length specifies the exact number of 
				   characters since this portion may not have
				   a NULL character terminator. */
    /* When kind == msk_user_string:
				   The pointer to the user string is in
				   error_msg_strings[]. */
    struct {
      a_byte_boolean
		quoted;		/* True if the user specified string is to
				   be outputted in double quotes.  When TRUE,
				   the string is constructed in the message
				   seqment buffer. */
    } string;
    /* When kind == msk_type: no variant
				   The pointer to the type is in
				   error_msg_types[]. */
    /* When kind == msk_source_position: no variant
				   The pointer to the position is in
				   error_msg_positions[]. */
    /* When kind == msk_symbol:    The pointer to the symbol is in
				   error_msg_syms[]. */
    struct {
      a_byte_boolean
		full_type;	/* True if the symbol should be expanded
				   into an object (type and name). */
      a_byte_boolean
		name_only;	/* True if only the symbol name is needed. */
      a_byte_boolean
		force_function_params;
				/* True if parameters of a function should be
				   listed even if it is not overloaded. */
      a_byte_boolean
		force_template_name_output;
				/* True if the "A<T> [with T=int]" style
 				   of output should be used for classes
				   when distinct template signatures are
				   being used. */
      a_byte_boolean
		decl_pos;	/* True if the declaration position is
				   to be generated. */
      a_byte_boolean
		template_args;	/* True if the template arguments are to be
				   displayed; the pointer to the scope is in
				   error_msg_scopes[]; for sk_function_template
				   only. */
      a_byte_boolean
		trans_unit;	/* True if the translation unit should be
				   displayed under certain circumstances.
				   In the primary translation unit, it is
				   displayed for secondary translation units.
				   In secondary translation units, it is
				   displayed for all translation units. */
    } symbol;
  } variant;
} a_msg_segment;

/*
Describe a label fill-in entry.  Label fill-in entries are used to specify
at run time which of two different strings (as specified by error codes)
should be filled in based upon the value of a variable.  Useful in cases
where an error message should differ depending on a particular mode.
*/
typedef struct a_label_fill_in_entry {
  char          *label;         /* The name of the "label" that is used in
                                   error message text (i.e., %[label]).  This
                                   string is used only for matching purposes
                                   and doesn't necessarily have to appear
                                   in the substituted string. */
  a_boolean     *test;          /* A pointer to a boolean variable whose
                                   value at run time is used to decide which
                                   error code below is substituted. */
  an_error_code true_value,
                false_value;    /* Error codes representing strings to be used
                                   in the "TRUE" and "FALSE" cases.  Note that
                                   these error codes should not themselves
                                   contain any fill-ins. */
} a_label_fill_in_entry;

/* Define the label fill-ins: */
static a_label_fill_in_entry label_fill_ins[] = {
  { "managed",       &use_cppcli_fill_ins, ec_managed,      ec_winrt },
  { "C++/CLI",       &use_cppcli_fill_ins, ec_cppcli,       ec_cppcx_mapping },
  { "default",       &use_cppcli_fill_ins, ec_default,      ec_cli_mapping },
  { "cli::array",    &use_cppcli_fill_ins, ec_cli_array,    ec_platform_array},
  { "C++/CLI array", &use_cppcli_fill_ins, ec_cppcli_array, ec_cppcx_array },
  { "System",        &use_cppcli_fill_ins, ec_system,       ec_platform },
  { "gcnew",         &use_cppcli_fill_ins, ec_gcnew,        ec_ref_new },
  { NULL,            (a_boolean*)NULL,     ec_no_error,     ec_no_error },
};


static a_label_fill_in_entry *get_label_fill_in_entry(char   *label,
                                                      size_t length)
/*
Return a pointer to the label fill-in entry that matches the specified
label, with the specified length.  An assertion failure occurs if the
label fill-in entry is not found.
*/
{
  a_label_fill_in_entry *lfie;

  for (lfie = label_fill_ins; lfie->label != NULL; lfie++) {
    if (strncmp(lfie->label, label, length) == 0) break;
  }  /* for */
#if CHECKING
  if (lfie->label == NULL) {
#if DEBUG
    char *label_copy = alloc_fe((sizeof_t)length+1);
    strncpy(label_copy, label, length);
    label_copy[length] = '\0';
    fprintf(f_debug, "missing fill-in label: %s\n", label_copy);
#endif /* DEBUG */
    unexpected_condition_str(
                            "get_label_fill_in_entry: no label fill-in found");
  }  /* if */
#endif /* CHECKING */
  return lfie;
}  /* get_label_fill_in_entry */

#if EXPENSIVE_CHECKING && !STANDALONE_UTILITY_PROGRAM

static void verify_label_fill_in_entries(void)
/*
Check the label fill-ins of each error message to ensure that the label
fill-in entries are valid.
*/
{
  int error_code;
  char *ptr, *end_label;

  for (error_code = 0; error_code < ec_last; error_code++) {
    ptr = error_text((an_error_code)error_code);
    while (ptr != NULL) {
      ptr = mbc_strchr(ptr, '%');
      if (ptr == NULL || *ptr == '\0') {
        break;
      } else {
        ptr++;
        if (*ptr == '\0') {
          break;
        } else if (*ptr == '[') {
          end_label = mbc_strchr(ptr+1, ']');
          check_assertion(end_label != NULL);
          (void)get_label_fill_in_entry(ptr+1, end_label-ptr-1);
          ptr = end_label+1;
        }  /* if */
      }  /* if */
    }  /* while */
  }  /* for */
}  /* verify_label_fill_in_entries */

#endif /* EXPENSIVE_CHECKING && !STANDALONE_UTILITY_PROGRAM */

/*
Diagnostic message substitutions can be based upon strings, types, and symbols
passed to the appropriate diagnostic routines.  The following arrays of
pointers to these various substitution kinds are used to denote the
source of substitutions in message segments.  The sequence number in
message segment descriptor is used as an index into the appropriate array.
*/

#define MAX_ERR_SEG_KIND_PER_MSG 3
				/* The maximum number of error message
				   arguments of any message segment kind. */

static char *	error_msg_strings[MAX_ERR_SEG_KIND_PER_MSG + 1];
				/* Array of pointers to the strings to be
				   inserted into diagnostic messages. */
static a_type_ptr
		error_msg_types[MAX_ERR_SEG_KIND_PER_MSG + 1];
				/* Array of pointers to the types to be
				   used for substitutions in diagnostic
				   messages. */
#if !STANDALONE_UTILITY_PROGRAM
static a_source_position_ptr
		error_msg_positions[MAX_ERR_SEG_KIND_PER_MSG + 1];
				/* Array of source positions to be used
				   for insertion into diagnostic messages. */
static a_symbol_ptr
		error_msg_syms[MAX_ERR_SEG_KIND_PER_MSG + 1];
				/* Array of pointers to the symbols to be
				   used for substitutions in diagnostic
				   messages. */
static a_scope_stack_entry_ptr
		error_msg_scopes[MAX_ERR_SEG_KIND_PER_MSG + 1];
				/* Array of pointers to scope stack entries
				   to be used in diagnostic messages. */
#endif /* !STANDALONE_UTILITY_PROGRAM */

static a_msg_segment_ptr
	error_message_head;
				/* Pointer to the first segment in the current
				   error message being formatted. */

static an_error_severity
		default_severity_for_error_code[(int)ec_last + 1];
				/* Array of error severities associated
				   with error codes.  The default table
				   contains values set from the
				   command-line. */

static an_error_severity
		current_severity_for_error_code[(int)ec_last + 1];
				/* Array of error severities associated
				   with error codes.  The current table
				   contains values set from the command-line
				   or by pragmas. */

static a_byte_boolean
		once_flag_for_error_code[(int)ec_last + 1];
				/* Array indicating whether the "once" flag
				   is set for a given error code.  This
				   flag indicates that a non-error
				   diagnostic should be issued only once. */

static a_byte_boolean
		diagnostic_issued_for_error_code[(int)ec_last + 1];
				/* Array indicating whether a diagnostic
				   has been issued for a given error code. */

static an_il_to_str_output_control_block
		octl;	/* Output control block for interface to il_to_str
			   routines. */


#if !STANDALONE_UTILITY_PROGRAM
/*
Variables pertaining to a line of source that must be reread for output in
a diagnostic.  Most diagnostics are issued for the current logical source
line.  Occasionally a diagnostic will refer to a source line that is not in
the current logical source line (a line read earlier).  The buffer pointed
to by error_source_line will hold such a source line that has been reread.
*/
static char	*error_source_line;
			/* Characters of the source line being reread for
			   diagnostic generation, ended by both a newline and
			   a null.  Space is dynamically allocated, and its
			   upper bound is given by
			   after_end_of_error_source_line. */
#define ERROR_SOURCE_LINE_INITIAL_ALLOCATION 200
#define ERROR_SOURCE_LINE_INCREMENTAL_ALLOCATION 1000
			/* Initial and incremental allocation sizes for
			   error_source_line.  The initial allocation should be
			   such that almost all cases can be accepted (so that
			   the realloc is hardly ever needed). */
static char	*after_end_of_error_source_line;
			/* Address past the last element of error_source_line,
			   as an aid to checking for overflow, etc.  A variable
			   because error_source_line line can be reallocated
			   larger if needed. */

/*
Data structures and variables used to index into source files to locate
a needed source line for a diagnostic.  For each source file, an index
table is created and updated as the file is read.
*/
#define INITIAL_PHYSICAL_LINE_COUNT_INCREMENT 100
				/* Constant value specifying the starting
				   interval at which physical line positions
				   will be recorded in the error_file_index
				   entry. */
#define NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES 10
				/* Number of physical line indexes maintained
		       		   for each source file. */

typedef struct an_error_file_index *an_error_file_index_ptr;
typedef struct an_error_file_index {
  a_source_file_ptr
		source_file;	/* Pointer to the IL source file entry
				   associated with this physical line index
				   table. */
  an_error_file_index_ptr
		previous;	/* Pointer to the previous an_error_file_index
				   entry in the doubly linked list. */
  an_error_file_index_ptr
		next;		/* Pointer to the next an_error_file_index
				   entry in the doubly linked list. */
  short		next_index_entry;
				/* Index of the next available entry in the
				   line_number and file_position arrays. */
  a_line_number line_number[NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES];
				/* Physical line number of the file that
				   begins at the associated file position. */
  long		file_position[NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES];
				/* File position that is the beginning of
				   the associated physical line and used 
				   to fseek() into the file. */
  long		physical_line_count_increment;
				/* Value specifying the interval at which
				   physical line positions will be recorded
				   in the error_file_index entry.  Initially
				   set to INITIAL_PHYSICAL_LINE_COUNT_INCREMENT
				   but may be updated later if the file is
				   large. */
} an_error_file_index;

static an_error_file_index_ptr
		head_of_file_index_list;
				/* Pointer to the beginning of the list of
				   an_error_file_index entries.  Initialized
				   to NULL by error_init(). */
static an_error_file_index_ptr
		tail_of_file_index_list;
				/* Pointer to the tail of the list of
				   an_error_file_index entries.  Initialized
				   to NULL by error_init(). */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Structure used to maintain a record of diagnostic messages that have been
issued during prototype instantiations.  This is used to suppress
diagnostics during actual instantiations if the diagnostic was issued
during the prototype instantiation.
*/
typedef struct a_recorded_diagnostic *a_recorded_diagnostic_ptr;
typedef struct a_recorded_diagnostic {
  a_recorded_diagnostic_ptr
		next;
			/* Pointer to the next entry in the current bucket.
			   NULL for the last entry in the bucket. */
  an_error_code	error_code;
			/* The error code of the diagnostic. */
  an_error_severity
		severity;
			/* The severity of the message. */
  a_source_position
		error_pos;
			/* The position associated with the message. */
  a_scope_number
		scope_of_prev_check;
			/* The scope number at the point of the last
			   suppressed diagnostic. */
  unsigned int	number_of_times_suppressed;
			/* Number of times the message was suppressed in
			   scope_of_prev_check. */
} a_recorded_diagnostic;


#define RECORDED_DIAG_TABLE_SIZE 983
			/* The number of buckets in the recorded diagnostic
			   table.  This number should be prime. */

static a_recorded_diagnostic_ptr
		recorded_diagnostic_table[RECORDED_DIAG_TABLE_SIZE];
			/* The top level array used for the hash table used
			   to find previously issued diagnostics. */


static a_msg_segment_ptr curr_output_msg_segment;
			/* The current output message segment used by
			   put_str_to_curr_output_msg_segment. */


char *error_text(an_error_code error_code)
/*
Return a pointer to the error text for the message identified by the given
error code.  Note that if this routine is modified to get the text from
some other source (e.g., a file) it should be copied into memory in such
a way that it will not be invalidated by subsequent calls of this routine.
This routine is called numerous times for each diagnostic, so it is important
that it be fast.  If a file is used, all of the messages should probably
be read into memory so that an array of strings can still be used here.
*/
{
  check_assertion_str2((int)error_code < (int)ec_last,
                       "error_text: ", "invalid error code");
  return (message_text[(int)error_code]);
}  /* error_text */


static void add_string_to_segment(char              *str,
                                  a_msg_segment_ptr seg_ptr)
/*
Add the specified string to the end of the message segment described by the
message segment descriptor pointed to by seg_ptr.  If the specified string
would overflow the existing string buffer, allocate a new buffer that is
at least INCR_MSG_SEGMENT_SIZE larger and copy the existing string to that
new buffer before adding the string.  Allow room for a NULL character at the
end of the buffer.
*/
{
  uint32_t	length_of_string;

  if (str != NULL) {
    length_of_string = (uint32_t)strlen(str);
    /* If the string will not fit in the buffer, enlarge the buffer so that it
       will fit.  Allow for a terminating null character. */
    if (seg_ptr->max_length <= (seg_ptr->length + length_of_string)) {
      char	*new_buffer;
      sizeof_t	new_size;

      new_size = seg_ptr->max_length +
                 ((INCR_MSG_SEGMENT_SIZE > length_of_string)
                   ? INCR_MSG_SEGMENT_SIZE + 1 : length_of_string + 1);
      new_buffer = realloc_buffer(seg_ptr->segment,
                                  (sizeof_t)(seg_ptr->max_length + 1),
                                   new_size);
      /* Since first_quote and second_quote, if non-NULL, point into the
         segment that's being replaced, they have to be modified to point
         into the new chunk of memory. */
      if (seg_ptr->first_quote != NULL) {
        seg_ptr->first_quote =
                  new_buffer + (seg_ptr->first_quote - seg_ptr->segment);
      }  /* if */
      if (seg_ptr->second_quote != NULL) {
        seg_ptr->second_quote =
                  new_buffer + (seg_ptr->second_quote - seg_ptr->segment);
      }  /* if */
      /* Now we can go ahead and reset the segment pointer. */
      seg_ptr->segment    = new_buffer;
      seg_ptr->max_length = (int)(new_size - 1);
    }  /* if */
    (void)strcpy((char *)(seg_ptr->segment + seg_ptr->length), str);
    seg_ptr->length += length_of_string;
  }  /* if */
}  /* add_string_to_segment */


static a_msg_segment_ptr new_message_segment(void)
/*
Allocate and initialize the fixed part a new message segment.
*/
{
  a_msg_segment_ptr msg;

  msg = (a_msg_segment_ptr)alloc_general(sizeof(a_msg_segment));
  msg->next         = NULL;
  msg->segment      = NULL;
  msg->first_quote  = NULL;
  msg->second_quote = NULL;
  msg->length       = 0;
  msg->max_length   = 0;
  msg->sequence_no  = 1;
  return msg;
}  /* new_message_segment */


static a_msg_segment_ptr establish_first_segment(void)
/*
Initialize the first message segment in the linked list of message segments
pointed to by the static variable error_message_head.  If error_message_head
is NULL, allocate a message seqment.
*/
{
  register a_msg_segment_ptr
		first_segment;

  first_segment = error_message_head;
  if (first_segment == NULL) {
    /* Only on the very first time, will this be NULL. */
    first_segment = error_message_head = new_message_segment();
  }  /* if */
  first_segment->length = 0;
  first_segment->sequence_no = 1;
  return first_segment;
}  /* establish_first_segment */


/*ARGSUSED*/ /* local_octl is not used. */
static void put_str_to_curr_output_msg_segment(
			char					*str,
			an_il_to_str_output_control_block_ptr	local_octl)
/*
Output the indicated string to the current output message segment.  This is
used as an output routine when using the il_to_str routines.
*/
{
  add_string_to_segment(str, curr_output_msg_segment);
}  /* put_str_to_curr_output_msg_segment */


static void set_up_output_control_block(void)
/*
Set up the output control block octl so that the il_to_str routines can be
called.
*/
{
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = put_str_to_curr_output_msg_segment;
  octl.gen_pcc_code = (C_dialect == C_dialect_pcc);
  /* For diagnostics in C99 mode we want to see "_Bool" rather "bool" or the
     type underlying _Bool. */
  octl.render_c99_bool = c99_mode;
  octl.remove_template_typedefs = !display_template_typedefs_in_diagnostics;
}  /* set_up_output_control_block */


static void form_type_summary(a_type_ptr        tp,
                              a_msg_segment_ptr seg_ptr)
/*
Format a string that represents the type pointed to by tp into the message
segment described by *seg_ptr.  The type description is enclosed in quotes.
*/
{
  curr_output_msg_segment = seg_ptr;
  add_string_to_segment("\"", seg_ptr);
  seg_ptr->first_quote = seg_ptr->segment + seg_ptr->length - 1;
  form_type(tp, &octl);
  add_string_to_segment("\"", seg_ptr);
  seg_ptr->second_quote = seg_ptr->segment + seg_ptr->length - 1;
}  /* summarize_type */


char *format_type_string(a_type_ptr tp,
                         sizeof_t   *len_ptr)
/*
A NULL terminated character string representation of the type pointed to
by tp is formatted into the first segment of the error diagnostic segment
list (pointed to by the static variable error_message_head).  The address
of the string created is returned and the length of the string is passed 
to the caller by *len_ptr.  Note that the string length does not include
the terminating NULL character.  The caller should make a copy of the
string immediately into whichever memory region is appropriate.
*/
{
  a_msg_segment_ptr curr_segment;

  curr_segment = establish_first_segment();
  /* Make certain that there is a string buffer and that it contains an
     empty string. */
  add_string_to_segment("", curr_segment);
  curr_output_msg_segment = curr_segment;
  /* Set up for use of the il_to_str routines. */
  set_up_output_control_block();
  form_type(tp, &octl);
  /* Provide the length of the string and the address of the string
     buffer to the caller. */
  *len_ptr = curr_segment->length;
  return curr_segment->segment;
}  /* format_type_string */

#if !STANDALONE_UTILITY_PROGRAM
#if CHECKING

static sizeof_t digits_to_represent(unsigned long value)
/*
Return the number of digits needed for the decimal representation of value,
e.g., 1297 --> 4.
*/
{
  sizeof_t ndigits = 1;

  while (value > 9) {
    value /= 10;
    ndigits++;
  }  /* while */
  return ndigits;
}  /* digits_to_represent */

#endif /* CHECKING */

static void form_source_position(a_source_position   *pos,
                                 a_source_position   *error_pos,
			         char		     *prefix_string,
			         char		     *suffix_string,
                                 char		     *end_of_source_string,
                                 a_msg_segment_ptr   seg_ptr)
/*
Format a source position in the message segment described by seg_ptr.
The generated format is one of:

        <prefix_string>at line xxx of "file name"<suffix string>
        <prefix_string>in "file name"<suffix string>

depending on whether or not the line number is zero (e.g., for assemblies).

If the file is stdin or the file name is identical to that of the error
position of the diagnostic message being composed, the file name is not
emitted as part of this declaration position.  error_pos represents the
source position of the diagnostic being formed and is used  to eliminate
redundant file names in a diagnostic.*/
{
  char		*file_name, *full_name, *diag_file_name;
  char		buffer[BASE_MSG_SEGMENT_SIZE];
  a_line_number line_number;
  a_boolean	at_end_of_source;

  diag_file_name = "";
  if (error_pos->seq != 0) {
    /* Have a valid diagnostic source position. */
    conv_seq_to_file_and_line(error_pos->seq, &diag_file_name, &full_name,
                              &line_number, &at_end_of_source);
    if (at_end_of_source) diag_file_name = "";
  }  /* if */
  if (pos->seq != 0) {
    /* Have a valid source position. */
    conv_seq_to_file_and_line(pos->seq, &file_name, &full_name,
                              &line_number, &at_end_of_source);
    if (at_end_of_source) {
      add_string_to_segment(end_of_source_string, seg_ptr);
    } else {
      a_boolean file_name_needed = strcmp(file_name, diag_file_name) != 0 &&
                                   strcmp(file_name, FILE_NAME_FOR_STDIN) != 0;
      add_string_to_segment(prefix_string, seg_ptr);
      if (line_number == SP_LINE_UNKNOWN) {
        /* No line number (e.g., C++/CLI assemblies). */
        if (file_name_needed) {
          add_string_to_segment(error_text(ec_in), seg_ptr);
        }  /* if */
      } else {
        /* Emit the line number. */
        add_string_to_segment(error_text(ec_at_line), seg_ptr);
#if CHECKING
        if (digits_to_represent((unsigned long)pos->seq)
                            >= BASE_MSG_SEGMENT_SIZE) {
          internal_error("form_source_position: buffer size too small");
        }  /* if */
#endif /* CHECKING */
        (void)sprintf(buffer, "%lu", (unsigned long)line_number);
        add_string_to_segment(&buffer[0], seg_ptr);
      }  /* if */
      /* Add the file name if needed. */
      if (file_name_needed) {
        char *formatted_file_name;
        if (line_number != SP_LINE_UNKNOWN) {
          add_string_to_segment(error_text(ec_of), seg_ptr);
        }  /* if */
        add_string_to_segment("\"", seg_ptr);
        formatted_file_name = format_file_name(file_name);
        add_string_to_segment(formatted_file_name, seg_ptr);
        add_string_to_segment("\"", seg_ptr);
      }  /* if */
      add_string_to_segment(suffix_string, seg_ptr);
    }  /* if */
  }  /* if */
}  /* form_source_position */


static void form_function_template_param_list(a_symbol_ptr	sym,
                                   	      a_msg_segment_ptr	seg_ptr)
/*
Display the parameter list of the function template specified by sym.
*/
{
  a_template_symbol_supplement_ptr	tssp;
  a_template_param_ptr			tpp;
  a_template_decl_info_ptr		decl_info;

  check_assertion(sym->kind == (a_symbol_kind)sk_function_template);
  tssp = sym->variant.template_info;
  /* Only display the parameter list if the function template makes use
     of template parameters that are not part of the function signature. */
  if (tssp->variant.function.template_param_not_in_function_type) {
    decl_info = tssp->variant.function.decl_cache.decl_info;
    tpp = decl_info->parameters;
    if (tpp != NULL) {
      add_string_to_segment("<", seg_ptr);
      for (; tpp != NULL; tpp = tpp->next) {
        add_string_to_segment(tpp->param_symbol->header->identifier, seg_ptr);
        if (tpp->is_pack) add_string_to_segment("...", seg_ptr);
        if (tpp->next != NULL) add_string_to_segment(",", seg_ptr);
      }  /* for */
      add_string_to_segment(">", seg_ptr);
    }  /* if */
  }  /* if */
}  /* form_function_template_param_list */


static a_symbol_ptr prototype_symbol_for_class(a_type_ptr class_type)
/*
If class_type is an instance of a class template or nested class of a class
template, but not a specialization, return a pointer to the prototype
instantiation of the associated template.
*/
{
  a_symbol_ptr			result_sym = NULL;
  a_class_symbol_supplement_ptr	cssp;

  if (class_type->variant.class_struct_union.is_template_class &&
      !class_type->variant.class_struct_union.is_specialized) {
    cssp = symbol_supplement_for_class(class_type);
    /* For instances of a class template, display the symbol for
       the prototype instantiation.  This may not be set yet for
       an incomplete template class.  If it is not set, get the
       prototype instantiation from the class template. */
    result_sym = cssp->corresp_prototype_sym;
    if (result_sym == NULL) {
      check_assertion(cssp->class_template != NULL);
      result_sym = prototype_template_of(cssp->class_template)->
                                variant.template_info->
                                variant.class_template.prototype_instantiation;
    }  /* if */
  }  /* if */
  return result_sym;
}  /* prototype_symbol_for_class */


static void form_template_arg_info(a_symbol_ptr			sym,
				   a_symbol_ptr			template_sym,
                                   a_msg_segment_ptr		seg_ptr,
				   a_boolean			*p_any_args)
/*
sym is an entity that was to be displayed in an error fill-in.  If
template_sym is non-NULL (when called at the top level) it points to a
template of which sym is an instance, and which was actually displayed
instead of sym.  This routine displays the value of any template
parameters of the entity referred to by sym and its parent classes.
If template_sym is NULL, sym is a nontemplate entity, but may be a
member (such as a nonstatic data member) of a template class.  This
routine displays the value of any template parameters referred to by
the parent classes of sym.  *p_any_args is set to TRUE if an argument
has been displayed.  It is a NULL pointer when called at the outermost
level.
*/
{
  a_boolean				sym_is_specialized = FALSE;
  a_template_arg_ptr			tap = NULL;
  a_template_param_ptr			tpp;
  a_template_decl_info_ptr		decl_info = NULL;
  a_boolean				*any_args;
  a_boolean				any_args_value;

  if (p_any_args == NULL) {
    /* This is a top-level call.  Set any_args to point to a local variable
       that will contain the status. */
    any_args = &any_args_value;
    any_args_value = FALSE;
  } else {
    /* This is not a top-level call, use the pointer passed by the caller. */
    any_args = p_any_args;
  }  /* if */
  if (template_sym != NULL) {
    /* Get the template argument list and the is_specialized flag for the
       entity. */
    a_template_symbol_supplement_ptr	tssp;
    tssp = template_supplement_for_symbol(template_sym);
    switch (sym->kind) {
      case sk_class_or_struct_tag:
      case sk_union_tag:
        {
          a_type_ptr	tp = sym->variant.class_struct_union.type;
          sym_is_specialized = tp->variant.class_struct_union.is_specialized;
          tap = templ_arg_list_for_class(tp);
          decl_info = tssp->cache.decl_info;
        }
        break;
      case sk_routine:
      case sk_member_function:
        {
          a_routine_ptr	rp = sym->variant.routine.ptr;
          if (!rp->is_prototype_instantiation) {
            tap = rp->template_arg_list;
            decl_info = tssp->variant.function.decl_cache.decl_info;
          }  /* if */
        }
        break;
      case sk_static_data_member:
        {
          decl_info = tssp->cache.decl_info;
        }
        break;
      default:
        unexpected_condition_str2("form_template_arg_info:",
                                  "unexpected symbol kind");
        break;
    }  /* switch */
  }  /* if */
  if (sym_is_specialized) {
    /* There is no information to be displayed for fully specialized
       instances. */
  } else {
    /* Display the template argument information for the parent classes,
       then display the template arguments for this entity. */
    if (sym->is_class_member) {
      a_symbol_ptr		parent_sym;
      a_symbol_ptr		parent_template_sym;
      parent_sym = symbol_for(sym_parent_class(sym));
      if (template_sym != NULL) {
        check_assertion(template_sym->is_class_member);
        parent_template_sym = symbol_for(sym_parent_class(template_sym));
      } else {
        /* No template symbol was provided by the caller.  If the parent class
           is a template instance, use the prototype instantiation as the
           template symbol. */
        parent_template_sym =
                            prototype_symbol_for_class(sym_parent_class(sym));
      }  /* if */
      /* Only display the parent information if the parent class of the
         template is a prototype instantiation.  This suppresses the
         template argument information for the levels at which the
         template has been specialized.  This is also suppressed if the
         parent_sym is a prototype instantiation, to avoid output like
         "[with T=T]" */
      if (parent_template_sym != NULL &&
          is_prototype_instantiation_or_cli_generic(parent_template_sym) &&
          !is_prototype_instantiation_symbol(parent_sym)) {
        form_template_arg_info(parent_sym, parent_template_sym, seg_ptr,
                               any_args);
      }  /* if */
    }  /* if */
    if (tap != NULL) {
      /* Display the argument list for this entity.  Don't use the standard
         template argument traversal routines because they hide the existence
         of parameter packs and we want parameter packs to be displayed in
         the diagnostic. */
      check_assertion(decl_info != NULL);
      tpp = decl_info->parameters;
      for (; tpp != NULL; tpp = tpp->next) {
        /* Display "parameter=value". */
        if (!*any_args) {
          /* This is the first argument displayed -- add the introduction
             string to the message. */
          add_string_to_segment(" [", seg_ptr);
          add_string_to_segment(error_text(ec_with), seg_ptr);
          *any_args = TRUE;
        } else {
          /* This is not the first argument -- add "," separator. */
          add_string_to_segment(", ", seg_ptr);
        }  /* if */
        add_string_to_segment(tpp->param_symbol->header->identifier,
                              seg_ptr);
        add_string_to_segment("=", seg_ptr);
        check_assertion(tap != NULL);
        if (is_start_of_pack_expansion_templ_arg(tap)) {
          a_boolean   first_pack_arg = TRUE;
          /* This template parameter maps to zero or more template arguments
             in a parameter pack. */
          add_string_to_segment("<", seg_ptr);
          for (tap = tap->next;
               (tap != NULL &&
                !is_start_of_pack_expansion_templ_arg(tap) &&
                tap->is_pack_element);
               tap = tap->next) {
            /* Add a "," separator after the first argument. */
            if (first_pack_arg) {
              first_pack_arg = FALSE;
            } else {
              add_string_to_segment(", ", seg_ptr);
            }  /* if */
            form_a_template_arg(tap, &octl);
          }  /* for */
          add_string_to_segment(">", seg_ptr);
        } else {
          form_a_template_arg(tap, &octl);
          tap = tap->next;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  if (p_any_args == NULL && *any_args) {
    /* One or more arguments were displayed.  Terminate the list. */
    add_string_to_segment("]", seg_ptr);
  }  /* if */
}  /* form_template_arg_info */


static void form_symbol_name_for_error(
				a_symbol_ptr		sym,
                                a_msg_segment_ptr	seg_ptr)
/*
Display the name of the symbol specified by sym.  If sym is a member
of a template class, or a class nested within a template class, display
the prototype instantiation name instead of the normal parent class name.
The template argument values will then be added to the diagnostic later.
*/
{
  a_symbol_ptr	prototype_sym = NULL;
  a_type_ptr	parent_class = NULL;

  if (distinct_template_signatures && sym->is_class_member) {
     /* If the symbol is a member of a class, get the parent class and
        determine whether it is a template instance. */
     parent_class = sym_parent_class(sym);
     prototype_sym = prototype_symbol_for_class(parent_class);
  }  /* if */
  if (prototype_sym != NULL) {
    form_symbol_name(prototype_sym, &octl);
    add_string_to_segment("::", seg_ptr);
    form_optionally_qualified_symbol_name(sym, &octl,
                                          /*suppress_qualifier=*/TRUE);
  } else {
    form_symbol_name(sym, &octl);
  }  /* if */
}  /* form_symbol_name_for_error */


static void form_symbol_summary(a_symbol_ptr        sym,
                                a_source_position   *error_pos,
                                a_msg_segment_ptr   seg_ptr)
/*
Format the name of the symbol pointed to by sym in the message segment
described by *seg_ptr.  Type information is based on the fundamental symbol
and the name is that of sym.  error_pos represents the source position of
the diagnostic being formed and is used when formatting the symbol source
declaration position to eliminate redundant file names in a diagnostic.
*/
{
  a_type_ptr	type = NULL;
  a_routine_ptr	routine = NULL;	
  a_symbol_ptr  fund_sym;	/* Pointer to the fundamental symbol of
				   argument sym if it exists.  Otherwise,
				   the value will be that of sym. */
  an_error_code			entity_kind;
  a_boolean			force_function_params = FALSE;
  a_boolean			force_return_type = FALSE;
  a_boolean			return_type_needed = TRUE;
  a_boolean			is_declaration_like = FALSE;
  a_symbol_ptr			corresp_template_sym = NULL;
  a_template_instance_ptr	tip = NULL;
  a_symbol_ptr			sym_to_display;
  a_boolean			saved_remove_template_typedefs;

  curr_output_msg_segment = seg_ptr;
  /* Determine the fundamental symbol of this symbol. */
  fund_sym = fundamental_symbol_of(sym);
  switch (fund_sym->kind) {
    case sk_keyword:
      /* The name of a keyword is extracted from the token_names array, and
         is handled differently from other symbols. */
      if (! seg_ptr->variant.symbol.name_only) {
        add_string_to_segment(error_text(ec_keyword), seg_ptr);
        add_string_to_segment(" ", seg_ptr);
      } /* if */
      add_string_to_segment("\"", seg_ptr);
      /* Use the name in the header. */
      add_string_to_segment(sym->header->identifier, seg_ptr);
      break;
    case sk_macro:
      entity_kind = ec_macro;
      goto symbol_name;
    case sk_label:
      entity_kind = ec_label;
      goto symbol_name;
    case sk_type:
      if (fund_sym->variant.type.ptr->kind == (a_type_kind)tk_template_param) {
        entity_kind = ec_template_parameter;
      } else {
        entity_kind = ec_type;
      }  /* if */
      goto symbol_name;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      { if (C_dialect == C_dialect_cplusplus &&
            is_prototype_instantiation_symbol(fund_sym)) {
          /* This is a symbol for a prototype instantiation of a class
             template.  It is preferable to display "class template X<T>"
             instead of "class X<T>", so fall through to code for
             sk_class_template. */
        } else {
          if (fund_sym->kind == (a_symbol_kind)sk_union_tag) {
            entity_kind = ec_union;
          } else if (C_mode()) {
            entity_kind = ec_struct;
#if MICROSOFT_EXTENSIONS_ALLOWED
          } else if (fund_sym->variant.class_struct_union.type
                             ->variant.class_struct_union.is_interface) {
            entity_kind = ec_microsoft_interface;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          } else {
            entity_kind = ec_class;
          }  /* if */
          if (distinct_template_signatures &&
              seg_ptr->variant.symbol.force_template_name_output) {
            /* If the class is a template instance, get the corresponding
               prototype symbol for display purposes. */
            corresp_template_sym = prototype_symbol_for_class(
                                    fund_sym->variant.class_struct_union.type);
          }  /* if */
          goto symbol_name;
        }  /* if */
      }
      /*FALLTHROUGH*/
    case sk_class_template:
      if (sym->is_template_param) {
        entity_kind = ec_template_template_parameter;
      } else if (sym->kind == (a_symbol_kind)sk_class_template &&
                 sym->variant.template_info->is_nonreal_member) {
        entity_kind = ec_template;
      } else if (sym->kind == (a_symbol_kind)sk_class_template &&
                 sym->variant.template_info->
                                    variant.class_template.is_alias_template) {
        entity_kind = ec_alias_template;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (sym->kind == (a_symbol_kind)sk_class_template &&
                 sym->variant.template_info->is_generic) {
        entity_kind = ec_generic_class;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        entity_kind = ec_class_template;
      }  /* if */
      goto symbol_name;
    case sk_enum_tag:
      entity_kind = ec_enum;
      goto symbol_name;
    case sk_parameter:
      entity_kind = ec_parameter;
      type = fund_sym->variant.param_id->type;
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_variable:
      type = fund_sym->variant.variable.ptr->type;
      if (fund_sym->variant.variable.ptr->is_parameter) {
        entity_kind = ec_parameter;
      } else if (fund_sym->variant.variable.ptr->is_handler_param) {
        entity_kind = ec_handler_parameter;
      } else {
        entity_kind = ec_variable;
      }  /* if */
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_extern_variable:
      type = fund_sym->variant.extern_symbol_descr->type;
      entity_kind = ec_variable;
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_constant:
      type = fund_sym->variant.constant->type;
      if (type->kind == (a_type_kind)tk_template_param &&
          type->variant.template_param.kind ==
                   (a_template_param_type_kind)tptk_unknown) {
        /* If the constant is a proxy or nonreal class member then use an
           entity kind of "nontype" to indicate that this is a generic
           nontype entity and not actually a constant. */
        entity_kind = ec_nontype;
      } else {
        entity_kind = ec_constant;
      }  /* if */
      goto symbol_name;
    case sk_routine:
    case sk_member_function:
      tip = fund_sym->variant.routine.instance_ptr;
      if (tip != NULL && distinct_template_signatures) {
        /* When a template function, or member function of a template class
           is displayed, it is done by displaying the template itself
           (or the member function of the prototype instantiation) and the
           template arguments used for each template parameter list.  Get
           the template symbol to be displayed. */
        /* Get the type of the template itself. */
        a_symbol_ptr				template_sym;
        a_template_symbol_supplement_ptr	tssp;
        template_sym = fund_sym->variant.routine.instance_ptr->template_sym;
        if (is_template_symbol(template_sym)) {
          template_sym = prototype_template_of(template_sym);
        }  /* if */
        tssp = template_supplement_for_symbol(template_sym);
        corresp_template_sym = template_sym;
        if (template_sym->kind == (a_symbol_kind)sk_function_template) {
          type = tssp->variant.function.routine->type;
        } else {
          type = routine_symbol_type(template_sym);
        }  /* if */
      } else {
        type = routine_symbol_type(fund_sym);
      }  /* if */
      routine = fund_sym->variant.routine.ptr;
      entity_kind = ec_function;
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_extern_routine:
      type = fund_sym->variant.extern_symbol_descr->type;
      routine = fund_sym->variant.extern_symbol_descr->variant.routine.ptr;
      entity_kind = ec_function;
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_overloaded_function:
      entity_kind = ec_overloaded_function;
      /* There is no specific type information available; this entity cannot
         be expressed as a declaration. */
      goto symbol_name;
    case sk_static_data_member:
      tip = fund_sym->variant.static_data_member.instance_ptr;
      type = fund_sym->variant.static_data_member.variable->type;
      entity_kind = ec_member;
      is_declaration_like = TRUE;
      if (tip != NULL && distinct_template_signatures) {
        /* When a static data member of a template class is displayed, it
           is done by displaying the static data member of the prototype
           instantiation and the template arguments used for each template
           parameter list.  Get the template symbol to be displayed. */
        corresp_template_sym = tip->template_sym;
      }  /* if */
      goto symbol_name;
    case sk_field:
      type = fund_sym->variant.field.ptr->type;
      if (C_dialect == C_dialect_cplusplus) {
        entity_kind = ec_member;
        is_declaration_like = TRUE;
      } else {
        entity_kind = ec_field;
      }  /* if */
      goto symbol_name;
    case sk_namespace:
      entity_kind = ec_namespace;
      goto symbol_name;
#if NAMED_REGISTERS_ALLOWED
    case sk_named_register:
      entity_kind = ec_named_register;
      goto symbol_name;
#endif /* NAMED_REGISTERS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
    case sk_named_address_space:
      entity_kind = ec_named_address_space;
      goto symbol_name;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sk_property_set:
      if (fund_sym->variant.property_info->properties != NULL &&
          fund_sym->variant.property_info->properties->next != NULL) {
        entity_kind = ec_property_set;
      } else {
        entity_kind = ec_property;
      }  /* if */
      goto symbol_name;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case sk_undefined:
      entity_kind = ec_no_error;
      goto symbol_name;
    case sk_function_template:
      entity_kind = ec_function_template;
      routine = fund_sym->variant.template_info->variant.function.routine;
      type = routine->type;
      /* Function templates can differ only by return type, so include the
         return type when also displaying the parameter types. */
      force_return_type = seg_ptr->variant.symbol.force_function_params;
symbol_name:
      /* Add the entity kind if not specified as name only or full type for
         a declaration-like entity. */
      if (type == NULL) is_declaration_like = FALSE;
      if (! seg_ptr->variant.symbol.name_only &&
          ! (seg_ptr->variant.symbol.full_type && is_declaration_like) ) {
        if (entity_kind != ec_no_error) {
          add_string_to_segment(error_text(entity_kind), seg_ptr);
          add_string_to_segment(" ", seg_ptr);
        }  /* if */
      } /* if */
      /* Add the beginning double quote. */
      add_string_to_segment("\"", seg_ptr);
      seg_ptr->first_quote = seg_ptr->segment + seg_ptr->length - 1;
      /* Check for special kinds of routines. */
      if (routine != NULL) {
        if (is_constructor_symbol(fund_sym) ||
#if MICROSOFT_EXTENSIONS_ALLOWED
            is_static_constructor_symbol(fund_sym) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            is_destructor_symbol(fund_sym) ||
            routine->special_kind == (a_special_function_kind)sfk_conversion) {
          /* The return type is not listed for constructors, destructors, and
             conversion functions. */
          return_type_needed = FALSE;
        }  /* if */
      }  /* if */
      /* Determine the symbol to be displayed.  For class members and
         ambiguous symbols always use the original symbol.  Otherwise, use
         the fundamental symbol. */
      { a_boolean	use_orig_sym;
        use_orig_sym = sym->is_class_member || sym->ambiguous;
        if (corresp_template_sym == NULL) {
          sym_to_display = use_orig_sym ? sym : fund_sym;
        } else {
          sym_to_display = corresp_template_sym;
        }  /* if */
      }
      /* When outputting a type based on a template definition, don't remove
         member typedefs. */
      saved_remove_template_typedefs = octl.remove_template_typedefs;
      octl.remove_template_typedefs = corresp_template_sym == NULL;
      if (routine != NULL) {
        if (seg_ptr->variant.symbol.force_function_params) {
          /* "%np" was specified for this fill-in. */
          force_function_params = TRUE;
        } else if (fund_sym->kind != (a_symbol_kind)sk_function_template &&
                   routine->template_arg_list != NULL) {
          /* Always put out the parameter list for functions that are
             instances of function templates. */
          force_function_params = TRUE;
        } else if (sym_to_display->overload_set_member) {
          /* Always put out the param list for overloaded functions. */
          force_function_params = TRUE;
        } else if (sym_to_display == corresp_template_sym &&
                   fund_sym->overload_set_member) {
          /* If a function is member of an instance of a template class, it
             may be that the template symbol is not part of an overload
             set and the member of the instantiated class is (e.g.,
             constructors and assignment operators, since the implicitly
             declared forms may bring an overload set into existence).  Treat
             this case as overloaded, too. */
          force_function_params = TRUE;
        }  /* if */
      }  /* if */
      /* Put out the first part of the type if needed, but not for
         constructors, destructors, and conversion functions (the return type
         is not listed for those). */
      if (type != NULL &&
          (seg_ptr->variant.symbol.full_type || force_return_type) &&
          (routine == NULL || return_type_needed)) {
        form_type_first_part_simple(type,
                                    /*under_lhs_declarator=*/FALSE,
                                    /*need_trailing_space=*/TRUE,
                                    &octl);
      }  /* if */
      /* Put out the name, including the class qualifier if any.
         If a template symbol is being displayed, use the normal
         form_symbol_name routine.  If a template symbol is not
         being displayed, use a special routine that displays the
         corresponding prototype template in place of the actual
         parent class. */
      if (corresp_template_sym == NULL) {
        form_symbol_name_for_error(sym_to_display, seg_ptr);
      } else {
        form_symbol_name(sym_to_display, &octl);
      }  /* if */
      /* Put out the second part of the type if needed.  Don't put it
         out in name-only mode.  Do put it out in full-type mode, or
         if function parameters should be listed. */
      if (type != NULL &&
          !seg_ptr->variant.symbol.name_only &&
          (seg_ptr->variant.symbol.full_type || force_function_params) ) {
        if (sym_to_display->kind == (a_symbol_kind)sk_function_template &&
            distinct_template_signatures) {
          /* If the symbol being displayed is a function template, display
             the template's parameter list in the form of an explicit
             function template parameter list. */
          form_function_template_param_list(sym_to_display, seg_ptr);
        }  /* if */
        if ((routine != NULL && !return_type_needed) ||
            (!seg_ptr->variant.symbol.full_type && !force_return_type)) {
          /* For constructors, destructors, and conversion functions,
             put out the function type but not the return type. */
          form_function_declarator(type, &octl);
        } else {
          /* Normal case -- put out the complete second part of the type. */
          form_type_second_part_simple(type, /*under_lhs_declarator=*/FALSE,
                                       &octl);
        }  /* if */
      }  /* if */
      /* Restore the typedef removal state before outputting the template
         argument values. */
      octl.remove_template_typedefs = saved_remove_template_typedefs;
      if (distinct_template_signatures) {
        /* Display the template argument information, if any. */
        a_symbol_ptr	templ_arg_sym;
        /* If a template name was displayed above, use the fundamental
           symbol to produce the appropriate template arguments.  Otherwise,
           use the symbol whose name was output above. */
        if (corresp_template_sym != NULL) {
          templ_arg_sym = fund_sym;
        } else {
          templ_arg_sym = sym_to_display;
        }  /* if */
        form_template_arg_info(templ_arg_sym, corresp_template_sym, seg_ptr,
                              (a_boolean*)NULL);
      }  /* if */
      break;
#if CHECKING
    case sk_projection:
    case sk_namespace_projection:
      /* Cannot have a projection of a projection symbol.  This is an
         error. */
      internal_error("form_symbol_summary: projection of projection kind");
      break;
    default:
      internal_error("form_symbol_summary: unsupported symbol kind");
#endif /* CHECKING */
  }  /* switch */
  /* Add the closing double quote mark. */
  add_string_to_segment("\"", seg_ptr);
  seg_ptr->second_quote = seg_ptr->segment + seg_ptr->length - 1;
  /* If the name is based on template arguments, add a message to that
     effect. */
  if (seg_ptr->variant.symbol.template_args) {
    a_scope_stack_entry_ptr  ssep = error_msg_scopes[seg_ptr->sequence_no];
    a_template_arg_ptr       tap;
    check_assertion(sym->kind == (a_symbol_kind)sk_function_template ||
                    sym->kind == (a_symbol_kind)sk_class_template);
    check_assertion(ssep != NULL);
    begin_template_arg_list_traversal_simple(ssep->template_arg_list, &tap);
    if (tap != NULL) {
      add_string_to_segment(" ", seg_ptr);
      advance_to_next_template_arg_simple(&tap);
      if (tap != NULL) {
        add_string_to_segment(error_text(ec_based_on_template_arguments),
                              seg_ptr);
      } else {
        add_string_to_segment(error_text(ec_based_on_template_argument),
                              seg_ptr);
      }  /* if */
      add_string_to_segment(" ", seg_ptr);
      form_template_args(ssep->template_arg_list, &octl);
    }  /* if */
  }  /* if */
  /* Add the declaration position as requested. */
  if (seg_ptr->variant.symbol.decl_pos) {
    if (routine != NULL && routine->compiler_generated &&
        !(routine->is_lambda_body && sym->decl_position.seq != 0)) {
      /* For compiler-generated routines referred to by name in diagnostics a
         declaration position is usually not helpful (e.g., for a generated
         constructor it ends up being the position of the class).  Lambda
         expressions, however, look sufficiently like the operator() they
         generate that the expression position can be reported as the position
         at which the corresponding operator() is declared. */
      add_string_to_segment(error_text(ec_declared_implicitly), seg_ptr);
    } else {
      form_source_position(&sym->decl_position, error_pos,
                           error_text(ec_declared_prefix), ")",
                           error_text(ec_at_end_of_source), seg_ptr);
    }  /* if */
  }  /* if */
  /* Add the translation unit associated with the symbol. */
  if (seg_ptr->variant.symbol.trans_unit) {
    a_boolean			add_trans_unit = FALSE;
    a_translation_unit_ptr	tup = NULL;
    if (sym->decl_scope == NO_SCOPE_NUMBER) {
      /* No translation unit is available for a symbol with no scope. */
    } else {
      tup = trans_unit_for_symbol(sym);
      if (is_primary_translation_unit) {
        /* In primary translation units,  only include the translation unit
           for symbols from secondary translation units. */
        add_trans_unit = tup != curr_translation_unit;
      } else {
        /* In secondary translation units, include the translation unit for
           all symbols. */
        add_trans_unit = TRUE;
      }  /* if */
    }  /* if */
    if (add_trans_unit) {
      /* Add the translation unit to the message. */
      char *formatted_file_name;
      add_string_to_segment(" (", seg_ptr);
      /* This message code includes the explanatory text (e.g.,
         "from translation unit"). */
      add_string_to_segment(error_text(ec_from_trans_unit), seg_ptr);
      add_string_to_segment("\"", seg_ptr);
      formatted_file_name = format_file_name(tup->source_file->file_name);
      add_string_to_segment(formatted_file_name, seg_ptr);
      add_string_to_segment("\"", seg_ptr);
      add_string_to_segment(")", seg_ptr);
    }  /* if */
  }  /* if */
}  /* form_symbol_summary */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void construct_message_segments(char *msg_ptr)
/*
Scan the message template pointed to by msg_ptr and construct the message
segment list.  The static variable error_message_head points to the first
segment descriptor.  Parameter substitutions are specified in the message
template beginning with a "%".  Accepted substitution designations are:

	s[q]x		- user provided string insertion.
	tx		- type insertion in double quotes.
        n[f|o|a][d]x	- symbol name insertion in double quotes.
        p		- insert a source position.
        %		- insert a percent sign.
        \[label\]	- choose one of two fill-ins depending on the value
			  of a variable (e.g., "%[C++/CLI]" might display as
			  "C++/CLI" or "C++/CX").

where "x" is an optional number in the range of 1 to MAX_ERR_SEG_KIND_PER_MSG
(defaulted to 1) that indicates which of multiple types, strings, or
symbols substitutions to be used.

String inserts may have an optional "q" modifier which specifies that the
string is to be enclosed in quotes.

Symbol name expansions may have one of the mutually exclusive optional
modifiers:

	f	- full object, complete type and object name.
	o	- name or qualified name only.
	a	- name or qualified name followed by template argument list

Symbol name expansions may have a declaration position modifier "d" which
requests that the declaration position of the symbol be added at the end
of the expansion.

The linked list of message segments needed for the text and parameter
substitutions to form the desired diagnostic message is constructed.

NOTE:  Symbol name insertion is not available if STANDALONE_UTILITY_PROGRAM
       is defined.  The symbol table and token names no longer exist.
*/
{
  a_msg_segment_ptr     curr_segment;	/* Pointer to the current segment. */
  char                  *end_ptr, *end_label;
  int                   i;
  a_label_fill_in_entry *lfie;
  
  /* Establish the message segment descriptor for the first segment. */
  curr_segment = establish_first_segment();
  while (*msg_ptr != '\0') {
    curr_segment->first_quote = NULL;
    curr_segment->second_quote = NULL;
    if (*msg_ptr == '%') {
      /* This is the beginning of a parameter substitution descriptor. */
      msg_ptr++;
      switch (*msg_ptr) {
        case 's':
          curr_segment->kind = (a_message_segment_kind)msk_user_string;
          curr_segment->variant.string.quoted = FALSE;
          msg_ptr++;
          if (*msg_ptr == 'q') {
            curr_segment->variant.string.quoted = TRUE;
            msg_ptr++;
          }  /* if */
          goto check_for_seq_number;
        case 't':
          curr_segment->kind = (a_message_segment_kind)msk_type;
          msg_ptr++;
          goto check_for_seq_number;
        case 'p':
          curr_segment->kind = (a_message_segment_kind)msk_source_position;
          msg_ptr++;
          goto check_for_seq_number;
        case 'n':
#if STANDALONE_UTILITY_PROGRAM
          /* Treat this %n as a continuation of the message template.
             No symbol name expansion is possible. */
          msg_ptr--;
          goto text_segment;
#else /* !STANDALONE_UTILITY_PROGRAM */
          /* This is a symbol name insertion point. */
          curr_segment->kind = (a_message_segment_kind)msk_symbol;
          curr_segment->variant.symbol.full_type = FALSE;
          curr_segment->variant.symbol.name_only = FALSE;
          curr_segment->variant.symbol.force_function_params = FALSE;
          curr_segment->variant.symbol.force_template_name_output = FALSE;
          curr_segment->variant.symbol.decl_pos = FALSE;
          curr_segment->variant.symbol.template_args = FALSE;
          curr_segment->variant.symbol.trans_unit = FALSE;
          msg_ptr++;
          /* Check for formatting options. */
          if (*msg_ptr == 'f') {
            /* Display complete type and object name. */
            curr_segment->variant.symbol.full_type = TRUE;
            msg_ptr++;
          } else if (*msg_ptr == 'o') {
            /* Display only the entity name. */
            curr_segment->variant.symbol.name_only = TRUE;
            msg_ptr++;
          } else if (*msg_ptr == 'p') {
            /* Display function parameters with the name. */
            curr_segment->variant.symbol.force_function_params = TRUE;
            msg_ptr++;
          } else if (*msg_ptr == 't') {
            /* Use the special template formatting for classes.  This
               also implies the 'f' option. */
            curr_segment->variant.symbol.force_template_name_output = TRUE;
            curr_segment->variant.symbol.full_type = TRUE;
            msg_ptr++;
          } else if (*msg_ptr == 'a') {
            /* Display the entity name along with associated template
               arguments. */
            curr_segment->variant.symbol.name_only = TRUE;
            curr_segment->variant.symbol.template_args = TRUE;
            msg_ptr++;
          }  /* if */
          if (*msg_ptr == 'd') {
            /* Display the declaration position following the entity name. */
            curr_segment->variant.symbol.decl_pos = TRUE;
            msg_ptr++;
          }  /* if */
          if (*msg_ptr == 'T') {
            /* Display the translation unit under certain conditions. */
            curr_segment->variant.symbol.trans_unit = TRUE;
            msg_ptr++;
          }  /* if */
#endif /* STANDALONE_UTILITY_PROGRAM */
check_for_seq_number:
          curr_segment->sequence_no = 1;
          if (isdigit((unsigned char)*msg_ptr)) {
            i = *msg_ptr - '0';
            if (i > 0 && i <= MAX_ERR_SEG_KIND_PER_MSG) {
              curr_segment->sequence_no = i;
              msg_ptr++;
            }  /* if */
          }  /* if */
          break;
        case '[':
          /* A label fill-in.  Use the string to find a matching label fill-in
             entry, then choose either the TRUE or FALSE error code message
             text as a replacement. */
          end_label = mbc_strchr(msg_ptr+1, ']');
          check_assertion(end_label != NULL);
          lfie = get_label_fill_in_entry(msg_ptr+1, end_label-msg_ptr-1);
          curr_segment->kind = (a_message_segment_kind)msk_error_text_part;
          curr_segment->variant.msg_part = error_text(*(lfie->test) ?
                                                           lfie->true_value :
                                                           lfie->false_value);
          curr_segment->length =
                              (uint32_t)strlen(curr_segment->variant.msg_part);
          msg_ptr = end_label+1;
          break;
        case '%':
          /* The string "%%" is used to insert a single "%" in the output. */
          goto text_segment;
#if CHECKING
        default:
          internal_error(
         "construct_message_segments: unknown message substitution parameter");
#endif /* CHECKING */
      }  /* switch */
    } else {
text_segment:
      /* This is the first character of a text segment. */
      curr_segment->kind = (a_message_segment_kind)msk_error_text_part;
      curr_segment->variant.msg_part = msg_ptr;
      /* Skip the first character when looking for a percent sign.  The
         first character may actually be a percent sign when the
         original message contained a "%%" used to insert a single
         "%" in the output. */
      end_ptr = mbc_strchr(msg_ptr+1, '%');
      if (end_ptr == NULL) {
        /* This part is the end of the message template. */
        curr_segment->length = (uint32_t)strlen(msg_ptr);
      } else {
        /* A substitution parameter has been found.  The length is the
           difference of the two pointers. */
        curr_segment->length = (uint32_t)(end_ptr - msg_ptr);
      }  /* if */
      msg_ptr += curr_segment->length;
    }  /* if */

    /* Prepare for the next message segment. */
    if (curr_segment->next == NULL) {
      /* Reached the current end of the chain; add another segment. */
      curr_segment->next = new_message_segment();
    }  /* if */
    curr_segment = curr_segment->next;
    curr_segment->length = 0;
    curr_segment->sequence_no = 1;
  }  /* while */

  /* Having reached the end of the diagnostic message template, terminate
     the message segment chain by setting the current segment kind to
     msk_last. */
  curr_segment->kind = (a_message_segment_kind)msk_last;
}  /* construct_message_segments */


static a_boolean message_has_fill_in(an_error_code error_code)
/*
Return TRUE if the message text for the indicated error code has at least
one error fill-in.
*/
{
  char *p;

  p = mbc_strchr(error_text(error_code), '%');
  /* Ignore "%%"; it's not a real fill-in. */
  while (p != NULL && p[1] == '%') p = mbc_strchr(p+2, '%');
  return (p != NULL);
}  /* message_has_fill_in */

#if !STANDALONE_UTILITY_PROGRAM

void clear_file_index_list(void)
/*
Clear the file index list.
*/
{
  head_of_file_index_list = NULL;
  tail_of_file_index_list = NULL;
}  /* clear_file_index_list */

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if !STANDALONE_UTILITY_PROGRAM

a_line_number initialize_file_index(a_source_file_ptr src_file)
/*
Create and initialize an_error_file_index entry for the source file IL
entry specified by src_file.  The newly created an_error_file_index is placed
at the head of the list pointed to by the static variable 
head_of_file_index_list.  Return the physical line number at which the first
index entry should be made.
*/
{
  an_error_file_index_ptr new_file;

  /* Allocate the error file index entry in the front end memory region. */
  new_file = (an_error_file_index_ptr)alloc_fe(sizeof(an_error_file_index));
  new_file->source_file = src_file;
  new_file->next_index_entry = 0;
  new_file->physical_line_count_increment =
					 INITIAL_PHYSICAL_LINE_COUNT_INCREMENT;
  /* Add the new entry at the head of the list. */
  new_file->previous = NULL;
  if ((new_file->next = head_of_file_index_list) == NULL) {
    /* This is for the primary source file. */
    tail_of_file_index_list = new_file;
  } else {
    /* Update the backward link. */
    head_of_file_index_list->previous = new_file;
  }  /* if */
  head_of_file_index_list = new_file;
  /* Return the physical line number that the first index entry should be
     made. */
  return INITIAL_PHYSICAL_LINE_COUNT_INCREMENT;
}  /* initialize_file_index */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

a_line_number update_file_index(a_source_file_ptr src_file,
                                a_line_number     physical_line,
                                long              file_pos)
/*
Add the file index specified by the physical_line and corresponding file
position (file_pos) to the an_error_file_index entry for the file represented
by src_file.  The physical line number that the next index entry should be
made is returned.
*/
{
  an_error_file_index_ptr curr_file;
  int                     idx, mid_idx;
  unsigned long           spacing;

  /* Typically the current file being read will be at the head of the list
     of an_error_file_index entries. */
  if ((curr_file = head_of_file_index_list)->source_file != src_file) {
    /* Since files are added to the beginning of this list as they are opened
       and the current file is not at the head of the list, the files
       included by the current file (precede the current file on the list)
       are no longer open.  Move these entries in front of the current file
       to the end of the list.  It is better to have any performance cost of
       file lookup associated with diagnostic generation, if needed. */
    for(curr_file = curr_file->next;
        curr_file != NULL;
        curr_file = curr_file->next) {
      /* Check if this is the entry needed. */
      if (curr_file->source_file == src_file) break;
    }  /* for */
#if CHECKING
    if (curr_file == NULL) {
#if DEBUG
      if (debug_level > 0) {
        (void)fprintf(f_debug,
                      "Missing file index entry for source file \"%s\"\n", 
                      src_file->full_name);
      }  /* if */
#endif /* DEBUG */
      internal_error("update_file_index: missing file index entry");
    }  /* if */
#endif /* CHECKING */
    /* Move the current file to the top of the list. */
    tail_of_file_index_list->next = head_of_file_index_list;
    head_of_file_index_list->previous = tail_of_file_index_list;
    /* Now have a circular list; cut where needed. */
    (tail_of_file_index_list = curr_file->previous)->next = NULL;
    (head_of_file_index_list = curr_file)->previous = NULL;
  }  /* if */
  /* Make the new entry. */
  if ((idx = curr_file->next_index_entry) < 
                                  NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES) {
    /* Add the file index information into the next available table entry. */
    curr_file->line_number[idx] = physical_line;
    curr_file->file_position[idx] = file_pos;
    curr_file->next_index_entry++;
  } else {
    /* The index table is full.  Reorganize the table by compressing the
       first half of the table to cover a wider range of lines.  The line
       number gaps will be some integer multiple of
       INITIAL_PHYSICAL_LINE_COUNT_INCREMENT. The value of
       curr_file->physical_line_count_increment is incremented by
       INITIAL_PHYSICAL_LINE_COUNT_INCREMENT each time the table is
       filled. */
    mid_idx = NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES / 2;
    spacing = curr_file->line_number[mid_idx] / (a_line_number)mid_idx;
    /* Eliminate the first entry in the top half of the table that is less
       than the value should be at the desired interval. */
    for (idx = 0; idx < mid_idx; idx++ ) {
      if (curr_file->line_number[idx] < ((idx + 1) * spacing)) {
        /* Eliminate this entry simply by breaking the loop. */
        break;
      }  /* if */
    }  /* for */
    /* Now shift all remaining entries in the table. */
    for (/* start with the index to be eliminated */;
         idx < NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES - 1;
         idx++ ) {
      curr_file->line_number[idx] = curr_file->line_number[idx + 1];
      curr_file->file_position[idx] = curr_file->file_position[idx + 1];
    }  /* for */
    /* Add the new entry at the end of the table. */
    curr_file->line_number[NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES - 1] =
                                                               physical_line;
    curr_file->file_position[NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES - 1] =
                                                              file_pos;
    /* Increments the physical line count increment value so that the
       additional entries that are added to the list will be spaced further
       apart. */
    curr_file->physical_line_count_increment +=
				 INITIAL_PHYSICAL_LINE_COUNT_INCREMENT;
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Updated error file index entries:\n");
    for (idx = 0;
         idx < NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES; ++idx) {
      fprintf(f_debug, "entry %d=%5lu\n", idx,
              curr_file->line_number[idx]);
    }  /* for */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  /* Return the physical line number at which the next entry should be made. */
  return physical_line + curr_file->physical_line_count_increment;
}  /* update_file_index */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

static void optimum_file_start_position(a_source_file_ptr src_file,
                                        a_line_number     physical_line,
                                        long              *seek_position,
                                        a_line_number     *starting_line)
/*
Given the source file specified by the IL source file entry pointer
src_file and the desired physical line number in that file, determine the
best position in the file to begin reading source lines.  The worst case
is from the beginning of the file, but if we have built a source line
index for the file as we were reading it, there may be a position in the
file which is closer to the desired line.
*/
{
  an_error_file_index_ptr curr_file;
  int                     idx;

  /* Locate the file index entry for the IL file entry specified by
      src_file. */
  for (curr_file = head_of_file_index_list;
       curr_file != NULL;
       curr_file = curr_file->next) {
    if (curr_file->source_file == src_file) break;
  }  /* for */
#if CHECKING
  if (curr_file == NULL) {
#if DEBUG
    if (debug_level > 0) {
      (void)fprintf(f_debug,
                    "Missing file index entry for source file \"%s\"\n", 
                    src_file->full_name);
    }  /* if */
#endif /* DEBUG */
    internal_error("optimum_file_start_position: missing file index entry");
  }  /* if */
#endif /* CHECKING */

  /* Find the index of the first entry greater than the specified physical
     line. */
  for (idx = 0; idx < curr_file->next_index_entry; idx++) {
    if (curr_file->line_number[idx] > physical_line )  break;
  }  /* for */
  if (idx == 0) {
    /* Desired position is earlier than any known position; start at the
       beginning of the file. */
    *seek_position = 0L;
    *starting_line = 1;
  } else {
    /* Return the last encountered "good" file position. */
    *seek_position = curr_file->file_position[idx - 1];
    *starting_line = curr_file->line_number[idx - 1];
  }  /* if */
}  /* optimum_file_start_position */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

static FILE	*f_err_src_file;
			/* File variable used to fetch the source line from
			   the source file. */

static a_boolean can_locate_source_line(
                                    a_seq_number          seq_number,
                                    a_unicode_source_kind *unicode_source_kind)
/*
Determine the actual file which contains the specified sequence number.  If
possible read the desired source line into the buffer pointed to by
error_source_line for later use by diagnostic output functions.
*unicode_source_kind is set to indicate the kind of Unicode encoding
form for the file, or usk_none if the file is not Unicode.
*/
{
  a_source_file_ptr src_file;
  a_line_number     physical_line, starting_line, skip_lines;
  long              seek_position;
  a_boolean         at_end_of_source;
  a_boolean         src_line_found = FALSE;
  int               ch;
  char              *loc_in_line;
  char              *after_end_of_error_source_line_minus_2;
#if UNICODE_SOURCE_SUPPORTED
  a_getc_source_state
                    source_state;
#endif /* UNICODE_SOURCE_SUPPORTED */

  *unicode_source_kind = usk_none;
  conv_seq_to_physical_file_and_line(seq_number, &src_file, &physical_line,
                                     &at_end_of_source);
  if (physical_line == 0 ||
      at_end_of_source ||
      strcmp(src_file->full_name, FILE_NAME_FOR_STDIN) == 0 ||
#if MICROSOFT_EXTENSIONS_ALLOWED
      src_file->is_assembly_file ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      head_of_file_index_list == NULL) {
    /* Either the file position is strange or unknown, we are at the end of
       the primary source file, the input is from stdin, the input is
       from a C++/CLI assembly file, or there is no file index information (for
       example, because we are currently in the back end).  The original source
       line cannot be recovered. */
    goto return_point;
  } else {
    /* Determine the optimum starting position in the file to read the desired
       source line. */
    optimum_file_start_position(src_file, physical_line, &seek_position,
                                &starting_line);
    /* Attempt to read the desired source line.  The source file should be
       readable unless it was deleted recently.  Fail softly if any problems
       arise. */
    if ((f_err_src_file = reopen_source_file(src_file->full_name,
                                             unicode_source_kind)) != NULL) {
      if (seek_position != 0) {
        if (fseek(f_err_src_file, seek_position, SEEK_SET) != 0) {
          /* The seek failed; fail softly and assume the source line is
             not readable. */
          goto close_file;
        }  /* if */
      }  /* if */
#if UNICODE_SOURCE_SUPPORTED
      clear_getc_source_state(&source_state, *unicode_source_kind);
#endif /* UNICODE_SOURCE_SUPPORTED */
      /* Skip over lines in the file to the position of the desired line. */
      for (skip_lines = physical_line - starting_line;
           skip_lines > 0;
           skip_lines--) {
        while ((ch = getc_source(f_err_src_file, source_state)) != '\n') {
          /* If the file has been changed under us, fail softly and assume
             the source line is not readable. */
          if (ch == EOF) goto close_file;
        }  /* while */
      }  /* for */
      /* Now positioned to read the actual source line desired.  Check if the
         error_source_line_buffer has been allocated.  This check may seem
         wasteful here, but it will only be done when a source line other
         than the current source line is needed, typically on a warning.
         The same error_source_line buffer will be used over multiple
         compilations. */
      if (error_source_line == NULL) {
        error_source_line = alloc_resizable_buffer(
                                  ERROR_SOURCE_LINE_INITIAL_ALLOCATION + 1);
        after_end_of_error_source_line = error_source_line +
                                  ERROR_SOURCE_LINE_INITIAL_ALLOCATION;
      }  /* if */
      loc_in_line = error_source_line;
      after_end_of_error_source_line_minus_2 = after_end_of_error_source_line -
                                               2;
      while ((ch = getc_source(f_err_src_file, source_state)) != '\n' &&
             ch != EOF) {
        if (loc_in_line == after_end_of_error_source_line_minus_2) {
          /* The buffer is not large enough for the current line. */
          sizeof_t  curr_length, old_size, new_size;
          char      *new_error_source_line;

          curr_length = loc_in_line - error_source_line;
          old_size = after_end_of_error_source_line - error_source_line;
          /* Increase the size of the error_source_line buffer. */
          new_size = old_size + ERROR_SOURCE_LINE_INCREMENTAL_ALLOCATION;
          /* As with the curr_source_line, add one more byte than required,
             so that a pointer past the end will not have the same address
             as a pointer to the next object. */
          new_error_source_line = realloc_buffer(error_source_line,
                                                  (sizeof_t)(old_size + 1),
                                                  (sizeof_t)(new_size + 1));
          /* Adjust the pointers to the old error_source_line */
          error_source_line = new_error_source_line;
          after_end_of_error_source_line = error_source_line + new_size;
          loc_in_line = error_source_line + curr_length;
          after_end_of_error_source_line_minus_2 =
                                     after_end_of_error_source_line - 2;
        }  /* if */
        /* Change a null character to a space (which is also what is done
           when the line is read initially). */
        if (ch == '\0') ch = ' ';
        /* Add the character to the buffer. */
        *loc_in_line++ = (char)ch;
      }  /* while */
      /* Add a trailing newline and null. */
      *loc_in_line++ = '\n';
      *loc_in_line = '\0';
      src_line_found = TRUE;

close_file:
      (void)fclose(f_err_src_file);
      f_err_src_file = NULL;
    }  /* if */
  }  /* if */
    
return_point:
  return src_line_found;
}  /* can_locate_source_line */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Put out_char into a local (a_text_buffer) buffer for later writing.  This is
done to avoid lots of costly system calls in the usual case that f_error
points to stderr, and stderr is unbuffered, as well as to emit an entire
diagnostic in a single system call.  This minimizes the chances of diagnostic
messages being intermixed when multiple compilations are performed in
parallel.
*/

#define putcb(out_char, buffer)                                       \
  add_char_to_text_buffer((buffer), (out_char));

/*
Shorthand for cases where an output character (out_char) is being written
to the write_diagnostic_buffer.
*/

#define putcwdb(out_char)                                             \
  putcb((out_char), write_diagnostic_buffer);

#if !STANDALONE_UTILITY_PROGRAM

/*
Macro to write source line characters in the first pass, and spaces over
and the caret on the second pass.  Exits to "end_of_loop" upon finding the
column for the caret in the second pass.
*/
#define put_char(out_char)                                            \
{ if (pass_for_caret && curr_column >= source_pos->column) {          \
    goto end_of_loop;                                                 \
  } else {                                                            \
    if ((out_char != '\r') &&                                         \
        (/*lint --e(506,845)*/ !pass_for_caret || (out_char) == '\t')) {  \
      putcwdb(out_char);                                              \
    } else {                                                          \
      putcwdb(' ');                                                   \
    }  /* if */                                                       \
    curr_column++;                                                    \
  }  /* if */                                                         \
}  /* put_char */


/*
Put out a character at *loc_in_line to the error output buffer, and
advance loc_in_line.  On the pass_for_caret, outputs a blank instead of
the character.  Handles multibyte characters appropriately.  ch is
the character to be used for *loc_in_line; it is different from what
is stored there when an ATTENTION_MARKER is being replaced by the
original character at that position.
*/
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED

#if UNICODE_SOURCE_SUPPORTED
#define and_unicode_source_kind_ne_usk_none(ukind) \
  && ((ukind) != usk_none)
#else /* !UNICODE_SOURCE_SUPPORTED */
#define and_unicode_source_kind_ne_usk_none(ukind) /* Nothing */
#endif /* UNICODE_SOURCE_SUPPORTED */

#define put_char_from_line(ch, ukind) \
{ put_char(ch); \
  if (multibyte_chars_in_source_enabled \
      and_unicode_source_kind_ne_usk_none(ukind)) { \
    int  numch; \
    char orig_ch = *loc_in_line; \
    *loc_in_line = (ch); \
    numch = mbc_length_simple(loc_in_line) - 1; \
    *loc_in_line = orig_ch; \
    while (numch-- > 0) { \
      loc_in_line++; \
      if (!pass_for_caret) { \
         putcwdb(*loc_in_line); \
      } \
    }  /* while */ \
  }  /* if */ \
  loc_in_line++; \
}  /* put_char_from_line */
#else /* !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
#define put_char_from_line(ch, ukind) \
{ put_char(ch); \
  loc_in_line++; \
}  /* put_char_from_line */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */


static void write_orig_source_line(a_source_position *source_pos)
/*
Write out the source line associated with the source position source_pos,
and place a caret under the proper column.  The position must be within
the current logical source line.  If the column position is zero, write
a blank line instead of the caret line.  All diagnostics generated by this
routine (or routines it calls) are placed in write_diagnostic_buffer for
later output as appropriate.
*/
{
  a_seq_number            seq;
  an_orig_line_modif_ptr  line_olmp, olmp, olmp_next;
  char                    *line_start, *loc_in_line;
  a_column_number         curr_column;
  int                     pass_for_caret;
  char                    ch;
  a_source_line_modif_ptr slmp;
  int                     i;

  /* Start by finding the right line.  The logical source line originally
     came from one or more physical lines ended by "\"s (line splices).
     Find the location of the start of text for the physical line
     containing the desired source position. */
  line_start = curr_source_line;
  seq = curr_seq_number;
  line_olmp = orig_line_modif_list;
  while (seq != source_pos->seq) {
    /* Need to advance to the next physical line.  Find the next line splice
       modification in the list of modifications. */
    for (;; line_olmp = line_olmp->next) {
#if CHECKING
      if (line_olmp == NULL) {
        internal_error("write_orig_source_line: could not find line");
      }  /* if */
#endif /* CHECKING */
      if (line_olmp->kind == olm_line_splice ||
          line_olmp->kind == olm_multiline_string_splice) break;
    }  /* for */
    seq++;
    line_start = line_olmp->line_loc;
    /* Skip the inserted \n of a multiline-string splice. */
    if (line_olmp->kind == olm_multiline_string_splice) line_start += 2;
    line_olmp = line_olmp->next;
  }  /* while */
  /* Found the proper physical line. */
  /* Take two passes -- the first to write the source line, the second to
     write the caret.  Because of the presence of tabs in the source line,
     etc. it is hard to figure out where to place the caret without
     running through the data structure again. */
  for (pass_for_caret = 0; pass_for_caret <= 1; pass_for_caret++) {
    /* Indent both the source line and the caret line.  This is done so
       that programs (like emacs) that read the error output will ignore
       these lines. */
    for (i = 0; i < SOURCE_INDENT; ++i) {
      putcwdb(' ');
    }  /* for */
    /* Perform any additional indentation needed (based on the category
       kind) */
    for (i = 0; i < diagnostic_indent; i++) {
      putcwdb(' ');
    }  /* for */
    /* On the caret pass, if the column number is zero (unknown), skip
       writing the spaces and caret and go right to the newline. */
    if (!pass_for_caret || source_pos->column != SP_COL_UNKNOWN) {
      loc_in_line = line_start;
      curr_column = 1;
      for (olmp = line_olmp; /*Exited by goto*/; olmp = olmp->next) {
        /* Print the characters of the current piece of curr_source_line, up
           to the next modification.  This is done a character at a time
           so that tabs can be processed specially and so that on the
           second pass the spaces/caret can be output. */
        while (olmp == NULL || loc_in_line != olmp->line_loc) {
          ch = *loc_in_line;
          if (ch == LE_ESCAPE) {
            /* LE_NULL is handled below (it has an associated modification
               entry). */
            /* Exit on the newline at the end of the source line.  (If there
               wasn't one there originally, one has been added.) */
            check_assertion_str(loc_in_line[1] == LE_NEWLINE,
                                "write_orig_source_line: bad lexical escape");
            goto end_of_loop;
          }  /* if */
          /* This character of the source line may have been replaced by
             an attention character to indicate that some sort of source
             line modification starts here.  If so, go to the modification
             entry and get the original source line character. */
          if (ch == ATTENTION_MARKER) {
            slmp = nested_source_line_modif(loc_in_line);
            ch = slmp->orig_char;
          }  /* if */
          /* Put out the character, which is possibly a multibyte
             character, and advance loc_in_line to after the character. */
          put_char_from_line(ch, curr_file_unicode_source_kind);
        }  /* while */
        /* Dump the characters for the modification. */
        switch ((int)olmp->kind) {
          case olm_trigraph:
            put_char('?');
            put_char('?');
            put_char(olmp->variant.trigraph_orig_char);
            loc_in_line++;
            /* If the trigraph is "? ? /", which turns into "\", and it's at
	       the end of a line, the "\" will indicate a line splice.  In
	       that case, the "\" for the line splice should not be put out.
	       (Note that the added space in this comment is to avoid
	       complaints about trigraphs while compiling this code.) */
            olmp_next = olmp->next;
            if (olmp_next != NULL && olmp_next->kind == olm_line_splice &&
                olmp_next->line_loc == olmp->line_loc) {
              /* Exit the loop because the line splice marks the end of
                 the physical line. */
              goto end_of_loop;
            }  /* if */
            break;
          case olm_line_splice:
            put_char('\\');
            /*FALLTHROUGH*/
          case olm_multiline_string_splice:
            /* Exit the loop since this line splice marks the end of the
               physical line. */
            goto end_of_loop;
          case olm_null:
            /* Null (zero) character in source line. */
            put_char(' ');
            loc_in_line += LE_ESCAPE_LEN;
            break;
#if CHECKING
          default:
            internal_error(
                          "write_orig_source_line: bad orig_modif_list entry");
#endif /* CHECKING */
        }  /* switch */
      }  /* for */
end_of_loop:
      /* For the pass that writes the caret, write the caret at this point. */
      if (pass_for_caret) putcwdb('^');
    }  /* if */
    /* For both passes, end the output line. */
    putcwdb('\n');
    /* After the first pass (writing the source), go on to the second pass
       (writing the caret). */
  }  /* for */
}  /* write_orig_source_line */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

#if !UNICODE_SOURCE_SUPPORTED
/*ARGSUSED*/  /* <-- unicode source_kind is not used in that case. */
#endif /* !UNICODE_SOURCE_SUPPORTED */
static void write_error_source_line(a_source_position     *source_pos,
                                    a_unicode_source_kind unicode_source_kind)
/*
Write out the source line associated with the source position source_pos,
and place a caret under the proper column.  The position has been determined
earlier to be in other than the current logical source line and the line
has been reread into the buffer pointed to by the static variable
error_source_line.  If the column position is zero, write a blank line
instead of the caret line.  unicode_source_kind indicates the kind of Unicode
encoding form for the file, or usk_none if the file is not Unicode.
All diagnostics generated by this routine (or routines it calls) are placed in
write_diagnostic_buffer for later output as appropriate.
*/
{
  char            *loc_in_line;
  char            ch;
  int             pass_for_caret;
  a_column_number curr_column;
  int             i;

  /* Take two passes -- the first to write the source line, the second to
     write the caret.  Because of the presence of tabs in the source line,
     etc. it is hard to figure out where to place the caret without
     running through the characters again. */
  for (pass_for_caret = 0; pass_for_caret <= 1; pass_for_caret++) {
    /* Indent both the source line and the caret line.  This is done so
       that programs (like emacs) that read the error output will ignore
       these lines. */
    for (i = 0; i < SOURCE_INDENT; ++i) {
      putcwdb(' ');
    }  /* for */
    /* Perform any additional indentation needed (based on the category
       kind). */
    for (i = 0; i < diagnostic_indent; i++) {
      putcwdb(' ');
    }  /* for */
    /* On the caret pass, if the column number is zero (unknown), skip
       writing the spaces and caret and go right to the newline. */
    if (!pass_for_caret || source_pos->column != SP_COL_UNKNOWN) {
      loc_in_line = error_source_line;
      curr_column = 1;
      /* Process each individual character until the newline is found. */
      for (;;) {
        /* Exit on the newline or carriage return/newline at the end of the
           source line. */
        if ((ch = *loc_in_line) == '\n' ||
            (ch == '\r' && loc_in_line[1] == '\n')) goto end_of_loop;
        put_char_from_line(ch, unicode_source_kind);
      }  /* for */

end_of_loop:
      /* For the pass that writes the caret, write the caret at this point. */
      if (pass_for_caret) putcwdb('^');
    }  /* if */
    /* For both passes, end the output line. */
    putcwdb('\n');
    /* After the first pass (writing the source), go on to the second pass
       (writing the caret). */
  }  /* for */
}  /* write_error_source_line */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void write_message_part(char              *msg,
                               int32_t           len,
                               a_text_buffer_ptr buffer,
                               int               *line_len,
                               a_boolean         wrap,
                               a_boolean         quoted_text,
                               a_boolean         start_of_diagnostic)
/*
Write a piece of an error message to buffer.  msg points to the
message (or is NULL if there is no message), and len is its length (or
-1 if the text is null-terminated).  The message is written to the text buffer
indicated by buffer.  *line_len is incremented by the number of characters
written.  If wrap is TRUE, the text will be wrapped to successive
additional lines as necessary and *line_len will be set to the number
of characters written on the final line.  If quoted_text is TRUE, the
msg consists solely of a double quoted string that should not be broken
if possible.

When text as allowed to be wrapped to the next line, trailing spaces on
each message fragment are not printed; but the number of these blanks is
remembered and used, if needed, for spacing before the next fragment.  The
boolean start_of_diagnostic indicates the beginning of a complete diagnostic.
Any trailing spaces not printed at the end of the previous diagnostic will
be forgotten.
*/
{
  int		chars_to_take,
		chars_that_will_fit_on_line;
  static int	trailing_space_count;

  if (start_of_diagnostic) trailing_space_count = 0;
  if (msg != NULL) {
    if (len < 0) len = (int32_t)strlen(msg);
    while (wrap && 
           (chars_that_will_fit_on_line = MAX_ERROR_OUTPUT_LINE_LENGTH -
                                  *line_len - trailing_space_count) < len) {
      /* The text is too long to fit on one line.  Write part of it,
         and continue on the next line. */
      if (chars_that_will_fit_on_line < 0) chars_that_will_fit_on_line = 0;
      /* Check that any quoted text that will not fit on this line
         can be put on the next line without being broken. */
      if (quoted_text &&
          len <= MAX_ERROR_OUTPUT_LINE_LENGTH - INDENT_AMOUNT - 
                                                diagnostic_indent ) {
        /* Quoted text will fit nicely on the next line. */
        goto start_line_and_indent;
      }  /* if */
      /*lint --e{850} chars_to_take modified in loop */
      for (chars_to_take = chars_that_will_fit_on_line;
           chars_to_take > 0;
           chars_to_take--) {
        /* Try to break the text at a blank. */
        if (msg[chars_to_take-1] == ' ') {
          /* Get to before the beginning of a string of blanks. */
          do {} while (--chars_to_take > 0 && msg[chars_to_take-1] == ' ');
          break;
        }  /* if */
      }  /* for */
      /* Make sure we're making progress; avoid getting hung up on one
         long piece of text with no blanks. */
      if (chars_to_take == 0 &&
          *line_len <= (INDENT_AMOUNT + diagnostic_indent)) {
        chars_to_take = chars_that_will_fit_on_line;
      }  /* if */
      /* Print the characters that will fit on the current line. */
      if (chars_to_take > 0) {
        /* Print any "remembered" spaces from the last fragment. */
        for (; trailing_space_count > 0; trailing_space_count--) {
          putcb(' ', buffer);
          (*line_len)++;
        }  /* for */
        if (chars_to_take > len) chars_to_take = len;
        *line_len += add_to_text_buffer(buffer, msg, (sizeof_t)chars_to_take);
        msg += chars_to_take;
        len -= chars_to_take;
      }  /* if */
start_line_and_indent:
      /* Skip over any blanks at the start of the remaining text of the
         message and discard any spaces from the previous message fragment. */
      trailing_space_count = 0;
      while (len > 0 && *msg == ' ') {
        msg++;
        len--;
      }  /* while */
      /* Start a new line and indent. */
      putcb('\n', buffer);
      for (*line_len = 0;
           *line_len < (INDENT_AMOUNT + diagnostic_indent);
           (*line_len)++) {
        putcb(' ', buffer);
      }  /* for */
    }  /* while */
    /* Print the final piece of the text (in the usual case, this prints
       all of the text). */
    /* Print any "remembered" spaces from the last fragment. */
    for (; trailing_space_count > 0; trailing_space_count--) {
      putcb(' ', buffer);
      (*line_len)++;
    }  /* for */

    if (wrap) {
      /* Remove any trailing spaces from the final piece of text.  These
         will be added prior to the next piece of text if needed. */
      for (; len > 0 && msg[len - 1] == ' '; len--, trailing_space_count++) {}
    }  /* if */
    if (len > 0) {
      *line_len += add_to_text_buffer(buffer, msg,
                                      strlen(msg) > (sizeof_t)len ? 
                                                                (sizeof_t)len :
                                                                strlen(msg));
    }  /* if */
  }  /* if */
}  /* write_message_part */


static void init_error_params(void)
/*
Initialize the array of user string, types and symbols to be inserted into
a diagnostic message.
*/
{
  int i;

  /* Initialize the message substitution kind array. */
  for (i = 1; i <= MAX_ERR_SEG_KIND_PER_MSG; i++) {
    error_msg_strings[i] = NULL;
    error_msg_types[i] = NULL;
#if !STANDALONE_UTILITY_PROGRAM
    error_msg_positions[i] = NULL;
    error_msg_syms[i] = NULL;
    error_msg_scopes[i] = NULL;
#endif /* !STANDALONE_UTILITY_PROGRAM */
  }  /* for */
  /* Set up for use of the il_to_str routines. */
  set_up_output_control_block();
}  /* init_error_params */


static void write_message(a_text_buffer_ptr buffer,
                          int               *line_len,
                          a_boolean         wrap)
/*
Write each of the message segments chained from the static variable
error_message_head to the specified text buffer.  *line_len is the current line
length and is incremented to reflect the number of characters added
to the current line.  If wrap is TRUE, the text will be wrapped to
successive additional lines as necessary.
*/
{
  a_msg_segment_ptr curr_seg;
  int32_t           length;
  int32_t           total_len;
  a_boolean         start_of_message = TRUE;

  for (curr_seg = error_message_head;
       curr_seg != NULL &&
         curr_seg->kind != (a_message_segment_kind)msk_last;
       curr_seg = curr_seg->next) {
    switch (curr_seg->kind) {
      case msk_error_text_part:
        write_message_part(curr_seg->variant.msg_part, curr_seg->length,
                           buffer, line_len, wrap, /*quoted_text=*/FALSE,
                           start_of_message);
        break;
      case msk_user_string:
        if (curr_seg->variant.string.quoted) {
          goto handle_embedded_quoted_text;
        }  /* if */
        write_message_part(error_msg_strings[curr_seg->sequence_no], -1,
                           buffer, line_len, wrap, /*quoted_text=*/FALSE,
                           start_of_message);
        break;
      case msk_source_position:
      case msk_type:
      case msk_symbol:
handle_embedded_quoted_text:
        if (curr_seg->first_quote == NULL) {
          write_message_part(curr_seg->segment, -1, buffer, line_len,
                             wrap, /*quoted_text=*/FALSE,
                             start_of_message);
        } else {
          /* This segment contains double quoted text which should not be
             broken across lines. */
          total_len = 0;
          if (curr_seg->segment != curr_seg->first_quote) {
            total_len = (int32_t)(curr_seg->first_quote - curr_seg->segment);
            write_message_part(curr_seg->segment, total_len, buffer,
                               line_len, wrap, /*quoted_text=*/FALSE,
                               start_of_message);
            start_of_message = FALSE;
          }  /* if */
          /* Output the quoted text as a single unit. */
          total_len += length = (int32_t)(curr_seg->second_quote -
                                          curr_seg->first_quote + 1);
          write_message_part(curr_seg->first_quote, length, buffer, line_len,
                             wrap, /*quoted_text=*/TRUE,
                             start_of_message);
          start_of_message = FALSE;
          /* Check for any fragment following the quoted text. */
          if ((length = curr_seg->length - total_len) > 0) {
            write_message_part(curr_seg->second_quote + 1, length, buffer,
                               line_len, wrap, /*quoted_text=*/FALSE,
                               start_of_message);
          }  /* if */
        }  /* if */
        break;
      default:
        unexpected_condition_str("write_message: bad message kind");
    }  /* switch */
    start_of_message = FALSE;
  }  /* for */
  putcb('\n', buffer);
}  /* write_message */


static void write_position(char              *file_name,
                           a_line_number     line_number,
                           a_column_number   column_number,
                           int               *line_len)
/*
Write the source position (filename and line number -- when non-zero) to the
write_diagnostic_buffer text buffer.  If column_number is not SP_COL_UNKNOWN,
the column number is added into the output.
*/
{
  char              number_buffer[50];
  char              *error_text_string;
  a_text_buffer_ptr buffer = write_diagnostic_buffer;

  /* Print the file and line number, with a column number if it is not
     SP_COL_UNKINOWN. */
  /* If the line is from stdin, do not display the file name. */
  if (strcmp(file_name, FILE_NAME_FOR_STDIN) == 0) {
    (void)sprintf(number_buffer, "%lu", line_number);
    error_text_string = error_text(ec_Line);
    *line_len += add_string_to_text_buffer(buffer, error_text_string);
    *line_len += add_string_to_text_buffer(buffer, " ");
    *line_len += add_string_to_text_buffer(buffer, number_buffer);
  } else {
    *line_len += add_string_to_text_buffer(buffer, "\"");
    /* Don't convert '\' to '\\' in error message output.  The
       name should be displayed as written by the user.  This also
       prevents doubling of directory separators on Windows. */
    *line_len += write_file_name_to_text_buffer(file_name, buffer,
                                          /*process_escapes=*/FALSE,
                                          /*escape_nonprintable_chars=*/FALSE);
    *line_len += add_string_to_text_buffer(buffer, "\"");
    if (line_number != SP_LINE_UNKNOWN) {
      (void)sprintf(number_buffer, "%lu", line_number);
      error_text_string = error_text(ec_line);
      *line_len += add_string_to_text_buffer(buffer, ", ");
      *line_len += add_string_to_text_buffer(buffer, error_text_string);
      *line_len += add_string_to_text_buffer(buffer, " ");
      *line_len += add_string_to_text_buffer(buffer, number_buffer);
    }  /* if */
  }  /* if */
  if (column_number != SP_COL_UNKNOWN) {
    (void)sprintf(number_buffer, "%d", column_number);
    error_text_string = error_text(ec_col);
    *line_len += add_string_to_text_buffer(buffer, " (");
    *line_len += add_string_to_text_buffer(buffer, error_text_string);
    *line_len += add_string_to_text_buffer(buffer, " ");
    *line_len += add_string_to_text_buffer(buffer, number_buffer);
    *line_len += add_string_to_text_buffer(buffer, ")");
  }  /* if */
}  /* write_position */


static void write_position_and_severity(
                                    an_error_code         error_code,
                                    an_error_severity     severity,
                                    a_source_position     *error_pos,
                                    char                  **file_name,
                                    a_line_number         *line_number,
                                    a_boolean             *src_text_needed,
                                    a_unicode_source_kind *unicode_source_kind,
                                    a_boolean             *in_curr_src_line,
                                    int                   *line_len)
/*
Write the source position (file name and line number) and severity to the
write_diagnostic_buffer text buffer.  Determine if the actual source line is
available, either in the current source line or able to be reread from one of
the source files.   If the actual source line is not available, the column
number is added into the output.
*/
{
  char          *full_name, *error_text_string;
  a_boolean	at_end_of_source;
  a_boolean     capitalize_severity;
  a_boolean     column_needed;
  a_boolean	local_display_error_number;
  an_error_code	severity_code;

#if STANDALONE_UTILITY_PROGRAM
  local_display_error_number = FALSE;
#else /* !STANDALONE_UTILITY_PROGRAM */
  /* Determine whether the error number should be displayed for this
     diagnostic.  Internal errors don't have error numbers.  If the
     caller passes the value ec_no_error, the error number display is
     suppressed. */
  local_display_error_number = display_error_number && 
                               error_code != ec_no_error;
#endif /* STANDALONE_UTILITY_PROGRAM */
  capitalize_severity = FALSE;
  *src_text_needed = FALSE;
  *unicode_source_kind = usk_none;
  *in_curr_src_line = FALSE;
  /* Determine the source position (file, line number). */
  if (error_pos->seq == 0) {
    /* Error position is in the command line or in initialization. */
    /* No position indication is written. */
    capitalize_severity = TRUE;
  } else {
    /* Get the file name and line number associated with the sequence
       number. */
    conv_seq_to_file_and_line(error_pos->seq, file_name, &full_name,
                              line_number, &at_end_of_source);
    if (at_end_of_source) {
      /* After end of source. */
      error_text_string = error_text(ec_at_end_of_source2);
      *line_len += add_string_to_text_buffer(write_diagnostic_buffer,
                                             error_text_string);
      *line_len += add_string_to_text_buffer(write_diagnostic_buffer, ": ");
    } else {
      /* Normal line in file, not end of file. */
#if STANDALONE_UTILITY_PROGRAM
      /* In program-form C-generating back end, source lines are 
         never displayed. */
      column_needed = FALSE;
#else /* !STANDALONE_UTILITY_PROGRAM */
      column_needed = brief_diagnostics &&
		      /*lint --e(506)*/COLUMN_NUMBER_IN_BRIEF_DIAGNOSTICS;
      /* If the line is the current one, print it and a caret indicating
         the position. */
      if (error_pos->seq >= curr_seq_number) {
        /* The sequence number falls within the sequence numbers for the
           current logical source line (it can't be past the current
           line). */
        *src_text_needed = TRUE;
        *in_curr_src_line = TRUE;
      } else {
        /* The sequence number is not in the current logical source line.
           Try to relocate the source line in the known source files. */
        if (can_locate_source_line(error_pos->seq, unicode_source_kind)) {
          /* The source line has been read into the error_source_line
             buffer. */
          *src_text_needed = TRUE;
        } else {
          /* The source line could not be reread.  Print column if it is
             nonzero. */
          column_needed = (error_pos->column != 0);
        }  /* if */
      }  /* if */
#endif /* STANDALONE_UTILITY_PROGRAM */
      /* Print the file and line number, with a column number if the
         position could not be indicated via a caret pointing to the
         source of the current line. */
      write_position(*file_name, *line_number,
                     column_needed ? error_pos->column : SP_COL_UNKNOWN,
                     line_len);
      *line_len += add_string_to_text_buffer(write_diagnostic_buffer, ": ");
    }  /* if */
  }  /* if */
  /* Determine the appropriate severity string, and also count this
     diagnostic against the total for the severity. */
  switch (severity) {
    case es_remark:
      severity_code = capitalize_severity ? ec_Remark : ec_remark;
      total_remarks++;
      break;
    case es_warning:
      severity_code = capitalize_severity ? ec_Warning : ec_warning;
      total_warnings++;
      break;
    case es_discretionary_error:
    case es_error:
      if (local_display_error_number ||
          ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES) { /*lint !e506 !e774*/
        severity_code = capitalize_severity ? ec_Error : ec_error;
      } else {
        severity_code = ec_no_error;
      }  /* if */
      total_errors++;
      break;
    case es_catastrophe:
      severity_code = capitalize_severity ? ec_Catastrophic_error
                                          : ec_catastrophic_error;
      total_catastrophes++;
      break;
    case es_command_line_error:
      severity_code = capitalize_severity ? ec_Command_line_error
                                          : ec_command_line_error;
      total_catastrophes++;
      break;
    case es_internal_error:
      severity_code = capitalize_severity ? ec_Internal_error
                                          : ec_internal_error;
      total_catastrophes++;
      break;
#if CHECKING
    case es_none:
    default:
      internal_error("write_position_and_severity: bad severity");
#endif /* CHECKING */
  }  /* switch */
  if (severity_code != ec_no_error) {
    error_text_string = error_text(severity_code);
    *line_len += add_string_to_text_buffer(write_diagnostic_buffer,
                                           error_text_string);
  }  /* if */
  /* The error number may optionally be displayed based on a command
     line option. */
  if (local_display_error_number) {
    /* Display the error message number.  Append a -D suffix if the
       severity may be changed. */
    a_boolean is_discretionary;
    char      number_buffer[50];
    (void)sprintf(number_buffer, "%d", (int)error_code);
    is_discretionary = ((int)severity <= (int)es_discretionary_error);
    error_text_string = error_text(is_discretionary ? ec_discretionary_suffix
                                                : ec_non_discretionary_suffix);
    *line_len += add_string_to_text_buffer(write_diagnostic_buffer, " #");
    *line_len += add_string_to_text_buffer(write_diagnostic_buffer,
                                           number_buffer);
    *line_len += add_string_to_text_buffer(write_diagnostic_buffer,
                                           error_text_string);
  }  /* if */
  *line_len += add_string_to_text_buffer(write_diagnostic_buffer, ": ");
}  /* write_position_and_severity */

#if !STANDALONE_UTILITY_PROGRAM

static void write_diag_to_raw_listing(an_error_severity          severity,
                                      char                       *file_name,
                                      a_line_number              line_number,
                                      a_source_position          *error_pos,
                                      a_diagnostic_category_kind diag_kind)
/*
If raw-listing information has been requested, the diagnostic message
is also output to the raw-listing file in coded form, for later
incorporation into the listing.  The coded form output line has the form:

  S "file-name" line-number column-number message-text

where "S" is R for remark, W for warning, E for error, and C for
catastrophe, command-line error, or internal error.  If the diagnostic
message is an additional message (dck_list), the coded severity is
in lower case.
*/
{
  int    line_len;
  char   severity_char;

  /* Start with the severity code character. */
  switch (severity) {
    case es_remark:
      severity_char = 'R';
      break;
    case es_warning:
      severity_char = 'W';
      break;
    case es_discretionary_error:
    case es_error:
      severity_char = 'E';
      break;
    case es_catastrophe:
    case es_command_line_error:
    case es_internal_error:
      severity_char = 'C';
      break;
#if CHECKING
    case es_none:
    default:
      internal_error("write_diag_to_raw_listing: bad severity");
#endif /* CHECKING */
  }  /* switch */
  if (diag_kind == dck_list || diag_kind == dck_context_primary) {
     severity_char = tolower(severity_char);
  }  /* if */
  (void)putc(severity_char, f_raw_listing);
  (void)fputc(' ', f_raw_listing);
  /* Determine the source position (file, line number). */
  if (error_pos->seq == 0) {
    /* Error position is in the command line or in initialization. */
    fputs("\"\" 0 0 ", f_raw_listing);
  } else {
    /* Normal line in file, or end of source.  Note that
       conv_seq_to_file_and_line has returned the position of the
       last line of the primary source file for the end-of-source case. */
    fprintf(f_raw_listing, "\"%s\" %lu %d ",
            format_file_name(file_name), line_number, error_pos->column);
  }  /* if */
  /* For an internal error, the coded-form message indicates only that the
     error is catastrophic, so we add text to indicate that it is an
     internal error. */
  if (severity == es_internal_error) {
    fputs("(internal error) ", f_raw_listing);
  }  /* if */
  /* Put out the error message text. */
  line_len = 0;  /* Meaningless. */
  if (write_message_buffer == NULL) {
    /* Allocate a buffer if we haven't yet already. */
    write_message_buffer = alloc_text_buffer(256);
  }  /* if */
  /* Start at the beginning of the buffer. */
  reset_text_buffer(write_message_buffer);
  write_message(write_message_buffer, &line_len, /*wrap=*/FALSE);
  /* Null terminate the buffer and write it. */
  putcb('\0', write_message_buffer);
  fputs(write_message_buffer->buffer, f_raw_listing);
}  /* write_diag_to_raw_listing */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Forward declaration. */
static void diag_message(an_error_code              error_code,
                         a_source_position          *error_pos,
                         an_error_severity          severity,
                         a_diagnostic_category_kind diag_kind);


static void write_diagnostic(an_error_code              error_code,
                             a_source_position          *error_pos,
                             an_error_severity          severity,
                             a_diagnostic_category_kind diag_kind)
/*
Write out a diagnostic message with the given message string, position, and
severity.  If the error is severe, terminate the compilation.
The message to be written is the concatenation of the linked list of
message segments pointed to by the static variable error_message_head.
The diagnostic category is specified by diag_kind.  The error position and
severity will be valid only on single (stand alone) diagnostics or the 
primary message of a multiple message diagnostic.  These values will
be preserved in static variables for use on subsequent calls to process
additional messages in a multiple message diagnostic.

The entire diagnostic is accumulated in the write_diagnostic_buffer text buffer
and emitted with a single system call.  This atomic operation reduces the
likelihood of diagnostic messages becoming intermingled during a parallel
compilation.
*/
{
		
  static char                   *file_name;
  static a_line_number          line_number;
  static a_boolean              source_text_needed;
  static a_unicode_source_kind  unicode_source_kind;
  static a_boolean              in_current_source_line;
  int                           line_len;
  a_source_position             local_pos;
#if FULLY_RESOLVED_MACRO_POSITIONS
  char                          *full_name;
  a_boolean                     at_end_of_source;
  int                           save_diagnostic_indent;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if MACRO_INVOCATION_TREE_IN_IL
  static a_source_position      full_pos;
  a_macro_invocation_record_ptr mirp = NULL;
#endif /* MACRO_INVOCATION_TREE_IN_IL */

  if (write_diagnostic_recursion_level == 0) {
    /* Allocate the diagnostic buffer if this is our first time. */
    if (write_diagnostic_buffer == NULL) {
      write_diagnostic_buffer = alloc_text_buffer(1024);
    }  /* if */
    /* Start at the beginning of the buffer. */
    reset_text_buffer(write_diagnostic_buffer);
  } else {
    /* Append error message to the existing text for this diagnostic. */
  }  /* if */
  write_diagnostic_recursion_level++;
#if FULLY_RESOLVED_MACRO_POSITIONS
  if (error_pos->orig_seq != 0 &&
      (macro_positions_in_diagnostics ||
       error_pos->orig_seq >= error_pos->seq)) {
    /* Use the original position of the text for the first part of the
       message (i.e., if the text is in a macro expansion, the position will
       indicate the macro definition or macro argument from which the text
       was copied).  We use the normal position if the original position is
       in a command-line or predefined macro (orig_seq == 0), and if
       macro_positions_in_diagnostics is FALSE, we only use the original
       position if it is in a macro argument (a reference to a macro definition
       will necessarily have orig_seq < seq). */
    local_pos.seq = error_pos->orig_seq;
    local_pos.column = error_pos->orig_column;
  } else {
    local_pos = *error_pos;
  }  /* if */
#else /* !FULLY_RESOLVED_MACRO_POSITIONS */
  local_pos = *error_pos;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  if ((int)severity < (int)error_threshold) {
    /* Ignore the message if its severity is below the threshold. */
  } else {
    if (diag_kind == (a_diagnostic_category_kind)dck_list) {
      diagnostic_indent = LIST_DIAG_INDENT;
    } else if (diag_kind == (a_diagnostic_category_kind)dck_context_primary) {
      diagnostic_indent = INDENT_AMOUNT;
    } else if (diag_kind == (a_diagnostic_category_kind)dck_macro_context) {
      diagnostic_indent = MACRO_CONTEXT_INDENT;
    } else {
      diagnostic_indent = NORMAL_DIAG_INDENT;
    }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
    save_diagnostic_indent = diagnostic_indent;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

    if (diag_kind != dck_end_list && diag_kind != dck_end_context) {
      /* Perform any indentation needed (based on the category kind) */
      for (line_len = 0; line_len < diagnostic_indent; line_len++) {
        putcwdb(' ');
      }  /* for */
    }  /* if */

    if (diag_kind == dck_standalone || diag_kind == dck_primary) {
      /* Collect and output error position and severity information. */
      write_position_and_severity(error_code, severity, &local_pos, &file_name,
                                  &line_number,
                                  &source_text_needed,
                                  &unicode_source_kind,
                                  &in_current_source_line,
                                  &line_len);
#if FULLY_RESOLVED_MACRO_POSITIONS
      if (local_pos.seq != error_pos->seq ||
          local_pos.column != error_pos->column) {
        /* The values of file_name and line_number set by
           write_position_and_severity were based on the original position
           in error_pos.  Those are possibly not suitable for use by
           write_diag_to_raw_listing below, as they may reflect a macro
           definition line, making it impossible to associate the error with
           the section of code where it occurred.  We therefore reset those
           values to reflect the normal position in error_pos. */
        conv_seq_to_file_and_line(error_pos->seq, &file_name, &full_name,
                                  &line_number, &at_end_of_source);
      }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
    }  /* if */

    if (diag_kind != dck_end_list && diag_kind != dck_end_context) {
      /* There is a message to be formatted and written. */
      /* Add the error message text to the text buffer. */
      write_message(write_diagnostic_buffer, &line_len,
                    /*wrap=*/!brief_diagnostics && !do_not_wrap_diagnostics);

#if !STANDALONE_UTILITY_PROGRAM
      /* The message is always output to f_error so that the user can see it.
         If raw-listing information has been requested, it is also output to
         the raw-listing file in coded form, for later incorporation into the
         listing.  */
      if (f_raw_listing != NULL && diag_kind != dck_macro_context) {
        /* We will use the normal position, not the original position, for
           the raw listing, because otherwise there is no way to figure out
           where in the source code the error originated. */
        /* This diagnostic is not added to the accumulated diagnostic in
           write_diagnostic_buffer; it is handled separately. */
        write_diag_to_raw_listing(severity, file_name, line_number,
                                  error_pos, diag_kind);
      }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    }  /* if */

    if (diag_kind == dck_standalone || diag_kind == dck_end_list) {
#if !STANDALONE_UTILITY_PROGRAM
      if (source_text_needed && !brief_diagnostics) {
        /* Write the source text line, with a caret pointing to the location
           of the error. */
        if (in_current_source_line) {
          /* Write the source line text from the curr_source_line buffer. */
          write_orig_source_line(&local_pos);
        }  else {
          /* Write the source line text from the error_source_line buffer. */
          write_error_source_line(&local_pos, unicode_source_kind);
        }  /* if */
#if MACRO_INVOCATION_TREE_IN_IL
        if (error_pos->macro_context != NO_PARENT_MACRO_INVOCATION &&
            macro_positions_in_diagnostics) {
          /* Print a trace of the macro invocation stack in effect at
             error_pos.  Note that the last (bottommost) stack frame is
             omitted in this trace because it will refer to the same position
             as the normal position in error_pos, which will be printed
             below before the source line. */
          int                             stack_depth = 0;
          int                             i;
          a_boolean                       frames_omitted_msg_printed = FALSE;
          a_source_position               save_error_pos = *error_pos;

          for (mirp =
                    macro_invocation_record_at_index(error_pos->macro_context);
               mirp != NULL &&
                        mirp->parent_macro_index != NO_PARENT_MACRO_INVOCATION;
               mirp =
                  macro_invocation_record_at_index(mirp->parent_macro_index)) {
            ++stack_depth;
          }  /* for */
          mirp = macro_invocation_record_at_index(error_pos->macro_context);
          for (i = 0; i < stack_depth; ++i) {
            if (i < 5 || i >= stack_depth - 4) {
              init_error_params();
              if (mirp->assoc_macro != NULL) {
                error_msg_strings[1] = mirp->assoc_macro->source_corresp.name;
              } else {
                error_msg_strings[1] = error_text(ec_name_of_unknown_macro);
              }  /* if */
              copy_simple_position_to_full_position(mirp->start, full_pos);
              error_msg_positions[1] = &full_pos;
              diag_message(ec_in_expansion_of_macro, &null_source_position,
                           severity, dck_macro_context);
            } else if (!frames_omitted_msg_printed) {
              static char buffer[20];
              init_error_params();
              (void)sprintf(buffer, "%d", stack_depth - 9);
              error_msg_strings[1] = buffer;
              diag_message(ec_macro_context_lines_skipped, error_pos,
                           severity, dck_macro_context);
              frames_omitted_msg_printed = TRUE;
            }  /* if */
            mirp = macro_invocation_record_at_index(mirp->parent_macro_index);
          }  /* for */
          *error_pos = save_error_pos;
        }  /* if */
#endif /* MACRO_INVOCATION_TREE_IN_IL */
      }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
      if (macro_positions_in_diagnostics) {
        a_boolean             need_generic_introducer;
        a_unicode_source_kind macro_unicode_source_kind;
        if (local_pos.seq != error_pos->seq) {
          /* The original source line printed above was a #define, so we will
             try to display the source line containing the macro invocation.
             (This code relies on the results of the call to
             conv_seq_to_file_and_line above.) */
          if (error_pos->seq == 0 || at_end_of_source) {
            /* This should never happen, but just in case... */
            source_text_needed = FALSE;
          } else {
            source_text_needed = source_text_needed && !brief_diagnostics;
            if (source_text_needed && error_pos->seq < curr_seq_number) {
              /* Not in current source line -- see if we can print it. */
              source_text_needed = can_locate_source_line(
                                                   error_pos->seq,
                                                   &macro_unicode_source_kind);
            }  /* if */
          }  /* if */
          need_generic_introducer = TRUE;
        } else {
          /* Either there was no macro invocation involved in the position or
             the original position designated a macro argument; in either
             case, there will be no second source line. */
          source_text_needed = FALSE;
          need_generic_introducer = (local_pos.column != error_pos->column);
        }  /* if */
#if MACRO_INVOCATION_TREE_IN_IL
        if (mirp != NULL) {
          /* We still need to print the last line of the stack trace.  That
             will either stand alone or be the introducer for the source line,
             if one is to be printed, so we do not need a generic introducer,
             regardless of the calculations above. */
          need_generic_introducer = FALSE;
          init_error_params();
          if (mirp->assoc_macro != NULL) {
            error_msg_strings[1] = mirp->assoc_macro->source_corresp.name;
          } else {
            error_msg_strings[1] = error_text(ec_name_of_unknown_macro);
          }  /* if */
          error_msg_strings[2] = source_text_needed ? ":" : ".";
          copy_simple_position_to_full_position(mirp->start, full_pos);
          error_msg_positions[1] = &full_pos;
          diag_message(ec_in_expansion_of_macro_last, &null_source_position,
                       severity, dck_macro_context);
        }  /* if */
#endif /* MACRO_INVOCATION_TREE_IN_IL */
        if (need_generic_introducer) {
          /* There was no stack trace, so we don't know the name of the
             macro involved -- use a more generic message. */
          char  *gen_text = error_text(ec_in_macro_expansion_at);
          for (line_len = 0; line_len < MACRO_CONTEXT_INDENT; ++line_len) {
            putcwdb(' ');
          }  /* for */
          line_len += add_string_to_text_buffer(write_diagnostic_buffer,
                                                gen_text);
          write_position(file_name, line_number,
                         source_text_needed ? SP_COL_UNKNOWN :
                         error_pos->column, &line_len);
          putcwdb(source_text_needed ? ':' : '.');
          putcwdb('\n');
        }  /* if */
        if (source_text_needed) {
          diagnostic_indent = save_diagnostic_indent;
          if (error_pos->seq >= curr_seq_number) {
            /* In current source line. */
            write_orig_source_line(error_pos);
          } else {
            /* Text is in the error source line. */
            write_error_source_line(error_pos, macro_unicode_source_kind);
          }  /* if */
        }  /* if */
      }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    }  /* if */
    if ((diag_kind == dck_standalone || diag_kind == dck_end_list ||
         diag_kind == dck_end_context) &&
	!context_required && !brief_diagnostics) {
      /* Put out an extra space line after the error, for clarity.  The
         space is suppressed if a context message is to follow since the
         space should follow the context. */
      putcwdb('\n');
    }  /* if */
  }  /* if */

  write_diagnostic_recursion_level--;
  if (write_diagnostic_recursion_level == 0) {
    /* We've accumulated the entire diagnostic -- null terminate it and
       write it to f_error, flushing after we're done. */
    putcwdb('\0');
    fputs(write_diagnostic_buffer->buffer, f_error);
    (void)fflush(f_error);
  }  /* if */
  if ((diag_kind == dck_standalone || diag_kind == dck_end_list ||
       diag_kind == dck_end_context) && !context_required ) {
#if IL_SHOULD_BE_WRITTEN_TO_FILE && !STANDALONE_UTILITY_PROGRAM
    /* If there are any errors, suppress generation of the intermediate
       language file. */
    if (total_errors + total_catastrophes > 0) cancel_il_file();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && !STANDALONE_UTILITY_PROGRAM */
    /* Terminate the compilation for the more serious severities. */
    if (severity == es_catastrophe || severity == es_command_line_error ||
        severity == es_internal_error) {
      /* Force out the last line of the raw listing file. */
#if !STANDALONE_UTILITY_PROGRAM
      finish_raw_listing_file();
#endif /* !STANDALONE_UTILITY_PROGRAM */
      term_compilation(severity);
    }  /* if */
    /* Terminate the compilation if the error limit has been reached.  Note
       that remarks and warnings are never counted. */
    if (total_errors + total_catastrophes >= error_limit) {
#if !USING_DRIVER
      fprintf(f_error, "%s\n", error_text(ec_error_limit_reached));
#endif /* !USING_DRIVER */
#if !STANDALONE_UTILITY_PROGRAM
      if (f_raw_listing != NULL) {
        fprintf(f_raw_listing, "C \"\" 0 0 error limit reached\n");
      }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
      /* Force out the last line of the raw listing file. */
#if !STANDALONE_UTILITY_PROGRAM
      finish_raw_listing_file();
#endif /* !STANDALONE_UTILITY_PROGRAM */
      term_compilation(es_catastrophe);
    }  /* if */
  }  /* if */
}  /* write_diagnostic */


#if CHECKING
static a_boolean internal_error_loop;
			/* Set to TRUE once an internal error has been
			   detected.  Used to detect a loop in internal
			   error processing. */

DOES_NOT_RETURN internal_error(char *error_message)
/*
An internal error has occurred.  Write the given message and abort.
*/
{
  /* Make sure that if one internal error leads to another, we abort
     the compilation instead of looping. */
  if (internal_error_loop) {
    fprintf(f_error, "%s: %s\n", error_text(ec_internal_error_loop),
            error_message);
    term_compilation(es_internal_error);
  }  /* if */
  internal_error_loop = TRUE;
  init_error_params();
  error_msg_strings[1] = error_message;
  construct_message_segments("%s");
  if (write_diagnostic_recursion_level > 0) {
    /* We're in the midst of reporting an error.  Flush the current
       diagnostic and reset the recursion level so the internal error
       diagnostic will start anew. */
    putcwdb('\0');
    fputs(write_diagnostic_buffer->buffer, f_error);
    (void)fflush(f_error);
    write_diagnostic_recursion_level = 0;
  }  /* if */
  write_diagnostic(ec_no_error, &error_position, es_internal_error,
                   dck_standalone);
#ifdef __GNUC__
  /* Avoid gcc warning.  write_diagnostic does not return in this case. */
  exit_compilation(es_internal_error);
#endif /* __GNUC__ */
}  /* internal_error */


DOES_NOT_RETURN assertion_failed(char	*filename,
		                 int	 line_number,
				 char   *string1,
				 char   *string2)
/*
An assertion has failed.  Abort the compilation.
*/
{
#define BUFFER_SIZE 512
  char		buffer[BUFFER_SIZE];
  int		max_filename_length = BUFFER_SIZE - 100;
  char		line_number_buffer[32];
  int32_t	overflow;

  /* Make sure that formatting the internal error string won't overflow
     the buffer.  We subtract 100 from the buffer length to allow for
     other information that is included in the message.  If the filename
     is too long we print as many characters from the end of the string
     as possible because the characters at the beginning probably contain
     the directory portion of the name. */
  overflow = (int32_t)(strlen(filename) - max_filename_length);
  if (overflow > 0) {
    filename += overflow;
  }  /* if */
  if (suppress_assertion_line_number) {
    (void)strcpy(line_number_buffer, "<suppressed>");
  } else {
    sprintf(line_number_buffer, "%d", line_number);
  }  /* if */
  if (string1 == NULL) {
    sprintf(buffer, "assertion failed at: \"%s\", line %s\n",
            filename, line_number_buffer);
  } else {
    /* Print the two strings.  Only separate them by a blank if the second
       string is not null. */
    char	*separator;
    if (string2 == NULL || strlen(string2) == 0) {
      separator = "";
      if (string2 == NULL) string2 = "";
    } else {
      separator = " ";
    }  /* if */
    sprintf(buffer, "assertion failed: %s%s%s (%s, line %s)\n", string1,
            separator, string2, filename, line_number_buffer);
  }  /* if */
  internal_error(buffer);
}  /* assertion_failed */


/*
Structure to record a pending assertion.  If necessary, the recorded entities
will be passed to assertion_failed at a later time.
*/
static struct {
  char *filename;
  int  line_number;
  char *string1;
  char *string2;
} expected_error_record;

  
void record_expected_error(char *filename,
                           int  line_number,
                           char *string1,
                           char *string2)
/*
Record a pending assertion.  This routine may be called in a situation that
is expected to be the result of processing invalid source code but where a
diagnostic has not yet been issued.  The location (and associated message)
of that situation is recorded and then later checked by check_expected_errors.
Only the first instance of such a situation is recorded; subsequent calls
have no effect.
*/
{
  check_assertion(filename != NULL);
  if (expected_error_record.filename == NULL) {
    /* No expected error has been recorded yet. */
    expected_error_record.filename = filename;
    expected_error_record.line_number = line_number;
    expected_error_record.string1 = string1;
    expected_error_record.string2 = string2;
  }  /* if */
}  /* record_expected_error */


void check_expected_errors(void)
/*
If expected_error was called, check that errors have been issued.  Otherwise,
abort the compilation with the information recorded in the first call to
expected_error.
*/
{
  if (expected_error_record.filename != NULL && total_errors == 0) {
    assertion_failed(expected_error_record.filename,
                     expected_error_record.line_number,
                     expected_error_record.string1,
                     expected_error_record.string2);
  }  /* if */
}  /* check_expected_errors */

#endif /* CHECKING */


DOES_NOT_RETURN str_command_line_error(an_error_code error_code,
                                       char          *concat_string)
/*
Write a command-line error message concatenated with concat_string, and
terminate the compilation.
*/
{
  set_position_to(error_position, 0, SP_COL_CMD_LINE);
  init_error_params();
  error_msg_strings[1] = error_text(error_code);
  error_msg_strings[2] = concat_string;
  construct_message_segments("%s1%s2");

  write_diagnostic(error_code, &error_position, es_command_line_error,
                   dck_standalone);
#ifdef __GNUC__
  /* Avoid gcc warning.  write_diagnostic does not return in this case. */
  exit_compilation(es_internal_error);
#endif /* __GNUC__ */
}  /* str_command_line_error */


DOES_NOT_RETURN command_line_error(an_error_code error_code)
/*
Write a command-line error message, and terminate the compilation.
*/
{
  str_command_line_error(error_code, "");
}  /* command_line_error */

#if CHECKING

/*ARGSUSED*/ /* <-- because "error_code" is not used in some versions. */
static void check_if_fill_in_used(enum a_message_segment_kind_tag kind,
                                  int                             seq_no,
                                  an_error_code                   error_code)
/*
As a sanity check, report any fill-in that has not been incorporated into
the diagnostic being formed.  kind denotes which of string, type, or symbol
fill-in kind is to be checked; seq_no specifies the sequence number of
that fill-in kind.  error_code is provided for debugging information.
*/
{
  a_msg_segment_ptr  curr_seg;
#if DEBUG
  char		   *s;
#endif /* DEBUG */

  for (curr_seg = error_message_head;
       curr_seg != NULL && curr_seg->kind != (a_message_segment_kind)msk_last;
       curr_seg = curr_seg->next ) {
    if (curr_seg->kind == (a_message_segment_kind)kind && 
        curr_seg->sequence_no == seq_no) {
      /* The fill-in to be checked has been used. */
      goto return_point;
    }  /* if */
  }  /* for */
  /* Having scanned the complete list of message segments, this fill-in
     obviously has not been used. */
#if DEBUG
  switch (kind) {
    case msk_user_string:
      s = "string %s";
      break;
    case msk_type:
      s = "type %t";
      break;
    case msk_symbol:
      s = "symbol %n";
      break;
    default:
      s = "";
  }  /* switch */
  if (debug_level > 0) {
    (void)fprintf(f_debug, "Provided diagnostic fill-in %s%d was not used.\n",
                  s, seq_no);
    (void)fprintf(f_debug,
                  "  error message = \"%s\"\n", error_text(error_code));
  }  /* if */
#endif /* DEBUG */
  internal_error(
           "check_if_fill_in_used: provided diagnostic fill-in was not used");
return_point:;
}  /* check_if_fill_in_used */

#endif /* CHECKING */

static void check_for_overridden_severity(an_error_code     error_code,
					  an_error_severity *severity)
/*
Determine whether this error code has should have its severity
overridden by a value specified on the command line.  Diagnostics
may have their severity increased or decreased using this mechanism,
but diagnostics with a severity greater than es_discretionary_error
may not have their severity altered.
*/
{
  if ((int)*severity <= (int)es_discretionary_error) {
    an_error_severity	new_severity;
    new_severity = current_severity_for_error_code[(int)error_code];
    if (new_severity != es_default) *severity = new_severity;
  }  /* if */
}  /* check_for_overridden_severity */


static an_error_severity  cs_saved_severity;
			/* The saved severity used by check_severity.
			   This is a file-scope static so that it can
			   be cleared during initialization of the
			   front end. */

				
static a_boolean check_severity(an_error_code		   error_code,
                                a_source_position          **error_pos,
                                an_error_severity          *severity,
                                a_diagnostic_category_kind diag_kind)
/*
Determine whether this message should have its severity overridden by a
value specified on the command line.  Compare the resulting error severity
with the threshold setting to see if this diagnostic should be issued.
If this is a multi-message diagnostic, it may be necessary to save the
current source position and severity or restore the previously saved settings.
*/
{
  static a_source_position  saved_error_position;
  static an_error_severity  saved_error_threshold;
  an_error_severity	    error_threshold_to_use = es_none;

#if CHECKING
  /* The saved severity level should be es_default if and only if this is a
     diagnostic without extra message lines or if it is the first message
     with such extra lines. */
  if (diag_kind != dck_macro_context &&
      (cs_saved_severity == (an_error_severity)es_default) !=
      (diag_kind == (a_diagnostic_category_kind)dck_standalone ||
       diag_kind == (a_diagnostic_category_kind)dck_primary ||
       diag_kind == (a_diagnostic_category_kind)dck_context_primary)) {
    internal_error("check_severity: bad saved severity");
  }  /* if */
#endif /* CHECKING */
  if (diag_kind == (a_diagnostic_category_kind)dck_standalone ||
      diag_kind == (a_diagnostic_category_kind)dck_primary ||
      diag_kind == (a_diagnostic_category_kind)dck_context_primary) {
    /* A standalone message or the start of a list. */
    check_for_overridden_severity(error_code, severity);
    error_threshold_to_use = error_threshold;
    if ((int)*severity >= (int)error_threshold) {
      /* Check whether we are inside a "system" include file in which
         warnings should be suppressed.  This test is only done if the message
         would be issued based on the current threshold. */
      if (seq_is_in_system_header((*error_pos)->seq)) {
        error_threshold_to_use = es_error;
#if !STANDALONE_UTILITY_PROGRAM
      } else if (curr_command_line_macro_def != NULL) {
        /* We are processing a command-line macro definition.  Warnings
           detected during this process are ignored. */
        error_threshold_to_use = es_discretionary_error;
#endif /* !STANDALONE_UTILITY_PROGRAM */
      }  /* if */
    }
    if (diag_kind != (a_diagnostic_category_kind)dck_standalone) {
      /* The principal message of a multiple message diagnostic.  Save the
         arguments for later calls. */
      copy_source_position(**error_pos, saved_error_position);
      cs_saved_severity = *severity;
    }  /* if */
    saved_error_threshold = error_threshold_to_use;
  } else if (diag_kind == (a_diagnostic_category_kind)dck_list ||
             diag_kind == (a_diagnostic_category_kind)dck_end_list ||
             diag_kind == (a_diagnostic_category_kind)dck_end_context) {
    /* Reuse the error position and severity from the primary diagnostic. */
    *error_pos = &saved_error_position;
    *severity = cs_saved_severity;
    error_threshold_to_use = saved_error_threshold;
#if CHECKING
    if (diag_kind == (a_diagnostic_category_kind)dck_end_list ||
        diag_kind == (a_diagnostic_category_kind)dck_end_context) {
      cs_saved_severity = (an_error_severity)es_default;
    }  /* if */
#endif /* CHECKING */
  } else {
    /* The only remaining diagnostic kind is for macro context lines, which
       are issued directly from write_diagnostic and thus should not change
       either the current or saved position and severity. */
    check_assertion(diag_kind == dck_macro_context);
    error_threshold_to_use = saved_error_threshold;
  }  /* if */
  check_assertion((int)error_threshold_to_use != (int)es_none);
  /* Return FALSE if the current severity is below the threshold. */
  return ((int)*severity >= (int)error_threshold_to_use);
}  /* check_severity */


a_boolean is_effective_error(an_error_code	error_code,
                             an_error_severity	severity)
/*
Determine the severity at which a diagnostic specified by error_code and
severity would be issued.  Return TRUE if the diagnostic would be issued
at a severity of discretionary error or above.
*/
{
  a_boolean	result;

  check_for_overridden_severity(error_code, &severity);
  result = (int)severity >= (int)es_discretionary_error;
  return result;
}  /* is_effective_error */


#if !STANDALONE_UTILITY_PROGRAM

static a_source_position *context_position_for_instantiation(
				a_symbol_ptr		sym,
				a_source_position	*scope_stack_pos)
/*
Determine the position to be used as the point of instantiation of a
template for diagnostic purposes.  This is normally the scope_stack_pos,
but if that position represents the end-of-source position, then use the
position of the first reference of the instance specified by "sym".
*/
{
  char		*file_name;
  char		*full_name;
  a_line_number	line_number;
  a_boolean	at_end_of_source;

  a_source_position	*result_pos = scope_stack_pos;

  conv_seq_to_file_and_line(scope_stack_pos->seq, &file_name, &full_name,
                            &line_number, &at_end_of_source);
  if (at_end_of_source) {
    a_template_instance_ptr	tip = NULL;
    /* Get the template instance (if any) associated with this symbol. */
    if (sym->kind == (a_symbol_kind)sk_static_data_member) {
      tip = sym->variant.static_data_member.instance_ptr;
    } else if (is_function_symbol(sym)) {
      tip = sym->variant.routine.instance_ptr;
    }  /* if */
    if (tip != NULL) {
      /* If the symbol has a position of first reference, use that. */
      if (tip->pos_of_first_reference.seq != 0) {
        result_pos = &tip->pos_of_first_reference;
      }  /* if */
    }  /* if */
  }  /* if */
  return result_pos;
}  /* context_position_for_instantiation */


static a_boolean include_in_context_output(
			 a_scope_stack_entry_ptr ssep,
			 a_symbol_ptr	         *context_sym,
			 an_error_code		 *context_error_code,
			 a_source_position	 *context_source_pos,
			 a_boolean               add_detected_prefix)
/*
Return TRUE if this scope stack entry has context information that should
be processed, otherwise return FALSE.  When TRUE is returned *context_sym
is set to point to a symbol that provides the context information,
*context_error_code is set to the appropriate error code, and
context_source_pos (if not NULL) is set to the position to be reported.  When
add_detected_prefix is TRUE, the error code returned will refer to
a message that includes the text (e.g., "detected during ") that is
used when only a single line of context information is being supplied.
When multiple context lines are being displayed, the "detected during"
message appears by itself on a separate line.
*/
{
  a_boolean		result = FALSE;
  a_symbol_ptr		sym;
  an_error_code		error_code;
  a_source_position	*pos = NULL;

  if (ssep->exclude_from_context_output) {
    /* Don't include this scope in the context output. */
  } else if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
    /* Template instantiations (except for prototype instantiations)
       need additional context information. */
    sym = ssep->instance_sym;
    /* If the instance symbol is NULL use the template symbol instead. */
    if (ssep->in_prototype_instantiation ||
        ssep->in_generic_definition) {
      /* Prototype instantiations are excluded from the context output. */
    } else if (sym == NULL) {
      sym = ssep->template_sym;
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        error_code = add_detected_prefix ? 
                          ec_det_during_template_function_declaration_context :
                          ec_template_function_declaration_context;
      } else if (sym->kind == (a_symbol_kind)sk_class_template) {
        error_code = add_detected_prefix ?
                           ec_det_during_template_class_argument_list_context :
                           ec_template_class_argument_list_context;
      } else {
        unexpected_condition();
      }  /* if */
      result = TRUE;
    } else {
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        result = TRUE;
        error_code = add_detected_prefix ?
                      ec_det_during_implicit_static_data_member_definition :
                      ec_implicit_static_data_member_definition;
      } else {
        error_code = add_detected_prefix ?
                               ec_det_during_template_instantiation_context :
                               ec_template_instantiation_context;
        result = TRUE;
      }  /* if */
      /* Determine the position to be used as the location of this
         instantiation. */
      pos = context_position_for_instantiation(sym, &ssep->source_position);
    }  /* if */
  } else if (ssep->kind == (a_scope_kind)sck_function) {
    /* Compiler generated functions need additional information. */
    if (ssep->assoc_routine->compiler_generated) {
      sym = (a_symbol_ptr)ssep->assoc_routine->source_corresp.assoc_info;
      result = TRUE;
      error_code = add_detected_prefix ?
                            ec_det_during_compiler_generated_function_context :
                            ec_compiler_generated_function_context;
    }  /* if */
  }  /* if */
  if (result) {
    *context_sym = sym;
    *context_error_code = error_code;
    /* If some other position was determined above, use that.  Otherwise,
       use the position from the scope stack entry. */
    if (context_source_pos != NULL) {
      *context_source_pos = pos != NULL ? *pos : ssep->source_position;
    }  /* if */
  }  /* if */
#if CHECKING
  if (result && sym == NULL) {
    internal_error("include_in_context_output: no sym for context info");
  }  /* if */
#endif /* CHECKING */
  return result;
} /* include_in_context_output */


static int bucket_for_diag(an_error_code		error_code,
			   an_error_severity	severity,
			   a_source_position	*error_pos)
/*
Compute the hash table bucket to be used for this diagnostic.
*/
{
  unsigned long	value;
  int		bucket;

  value = (unsigned long)error_code;
  value *= ((unsigned long)severity + 1);
  value *= ((unsigned long)error_pos->seq + 1);
  value *= ((unsigned long)error_pos->column + 1);
  bucket = (int)(value % RECORDED_DIAG_TABLE_SIZE);
  return bucket;
}  /* bucket_for_diag */


void record_prototype_diagnostic(
				an_error_code		error_code,
				an_error_severity	severity,
				a_source_position	*error_pos)
/*
This diagnostic is being issued for a prototype instantiation.
Make a record of the diagnostic so that we can find it later to
suppress duplicate diagnostics.  This is also used to record information
about diagnostics issued during disambiguation.
*/
{
  int				bucket;
  a_recorded_diagnostic_ptr	rdp;

  bucket = bucket_for_diag(error_code, severity, error_pos);
  rdp = (a_recorded_diagnostic_ptr)alloc_fe(sizeof(a_recorded_diagnostic));
  rdp->error_code = error_code;
  rdp->severity = severity;
  rdp->error_pos = *error_pos;
  rdp->next = recorded_diagnostic_table[bucket];
#if CHECKING
  rdp->scope_of_prev_check = NO_SCOPE_NUMBER;
  rdp->number_of_times_suppressed = 0;
#endif /* CHECKING */
  recorded_diagnostic_table[bucket] = rdp;
}  /* record_prototype_diagnostic */


a_boolean find_prototype_diagnostic(
				an_error_code		error_code,
				an_error_severity	severity,
				a_source_position	*error_pos)
/*
This diagnostic is being issued for a real instantiation.  Check
whether a matching diagnostic was issued during a prototype instantiation.
Return TRUE if one is found.
*/
{
  a_boolean			found = FALSE;
  int				bucket;
  a_recorded_diagnostic_ptr	rdp;

  bucket = bucket_for_diag(error_code, severity, error_pos);
  rdp = recorded_diagnostic_table[bucket];
  for (; rdp != NULL; rdp = rdp->next) {
    if (rdp->error_code == error_code &&
        rdp->severity == severity &&
        rdp->error_pos.seq == error_pos->seq &&
        rdp->error_pos.column == error_pos->column) {
      a_scope_number curr_scope = scope_stack_top().number;
      found = TRUE;
      /* Check whether a given diagnostic is suppressed a large number of
         times from the same scope.  This is used to prevent an infinite
         loop if there is an error recovery problem.  If the same diagnostic
         is issued many times, discontinue the suppression so that the error
         limit will be reached. */
      if (rdp->scope_of_prev_check == curr_scope) {
        if (++(rdp->number_of_times_suppressed) > error_limit) found = FALSE;
      } else {
        /* A different scope.  Reset the count. */
        rdp->scope_of_prev_check = curr_scope;
        rdp->number_of_times_suppressed = 0;
      }  /* if */
      break;
    }  /* if */
  }  /* for */
  return found;
}  /* find_prototype_diagnostic */


static a_boolean diagnostic_already_issued_for_prototype(
				an_error_code			error_code,
				an_error_severity		severity,
				a_source_position		*error_pos,
                                a_diagnostic_category_kind	diag_kind)
/*
This routine is used to prevent duplication of diagnostics in
templates.  When a diagnostic is issued during a prototype instantiation
a record is kept based on the error code, severity, and position.  If
a matching diagnostic is issued during a real instantiation, it is
suppressed.

Return TRUE if the diagnostic should be suppressed.
*/
{
  a_boolean		suppress_diagnostic = FALSE;
  static a_boolean	saved_suppress_diagnostic;

  if (diag_kind == (a_diagnostic_category_kind)dck_standalone ||
      diag_kind == (a_diagnostic_category_kind)dck_primary ||
      diag_kind == (a_diagnostic_category_kind)dck_context_primary) {
    /* A standalone message or the start of a list. */
    if (depth_scope_stack == NO_SCOPE_DEPTH) {
      /* The scope stack is empty, don't check further (probably a
         command-line error. */
    } else if (find_prototype_diagnostic(error_code, severity, error_pos)) {
      suppress_diagnostic = TRUE;
    } else if (is_template_dependent_context() ||
#if MICROSOFT_EXTENSIONS_ALLOWED
               scope_stack_top().in_generic_definition ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
               scope_stack_top().in_disambiguation) {
      record_prototype_diagnostic(error_code, severity, error_pos);
    }  /* if */
    /* Save the result of this check and reuse it for any subordinate
       messages (list elements, etc.) that may follow. */
    saved_suppress_diagnostic = suppress_diagnostic;
  } else if (diag_kind == (a_diagnostic_category_kind)dck_list ||
             diag_kind == (a_diagnostic_category_kind)dck_end_list ||
             diag_kind == (a_diagnostic_category_kind)dck_end_context) {
    /* Reuse the saved result from the primary diagnostic or the start of the
       list. */
    suppress_diagnostic = saved_suppress_diagnostic;
  }  /* if */
  return suppress_diagnostic;
}  /* diagnostic_already_issued_for_prototype */


static a_boolean diagnostic_already_issued_for_diag_once(
					an_error_code		error_code,
					an_error_severity	severity)
/*
This routine records the fact that a given diagnostic has been issued and
also checks whether this occurrence of the diagnostic should be suppressed
because of the use of the "once" diagnostic control.  Return TRUE if the
diagnostic should be suppressed.
*/
{
  a_boolean		result = FALSE;

  if ((int)severity <= (int)es_warning &&
      once_flag_for_error_code[(int)error_code]) {
    result = diagnostic_issued_for_error_code[(int)error_code];
  }  /* if */
  diagnostic_issued_for_error_code[(int)error_code] = TRUE;
  return result;
}  /* diagnostic_already_issued_for_diag_once */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Forward declaration. */
static DOES_NOT_RETURN error_code_errno_catastrophe(
                                      an_error_code error_code,
                                      an_error_code error_code2,
                                      int           errno_value);


static void file_open_error_full(an_error_severity	severity,
				 an_error_code		file_kind,
                                 char			*file_name,
				 an_open_file_result	*open_result,
				 a_source_position	*error_pos)
/*
Write an error message about opening the file named file_name.  file_kind is
an error code for a message that describes the kind of file being opened.
open_result is the entry that describes the kind of failure, which was
returned by the file open routine.
*/
{
  char				*reason = NULL;
  an_open_file_result_set	flags = open_result->flags;
  an_error_code			error_code;
  a_source_position		local_error_pos = *error_pos;

  /* Determine if a open failure reason should be displayed, and if so
     what it should be. */
  if ((flags & OFR_NOT_FOUND) != 0) {
    /* Do not display a reason if the file was not found. */
  } else if ((flags & OFR_CANNOT_OPEN) != 0) {
    /* Use strerror to determine the reason based on the errno value. */
    reason = strerror(open_result->errno_value);
  } else if ((flags & OFR_NOT_REGULAR) != 0) {
    reason = error_text(ec_not_regular);
  } else if ((flags & OFR_IS_DIRECTORY) != 0) {
    reason = error_text(ec_is_directory);
  } else if ((flags & OFR_BAD_NAME) != 0) {
    reason = error_text(ec_illegal_file_name);
  }  /* if */
  error_code = reason == NULL ? ec_cannot_open_file
                              : ec_cannot_open_file_reason;
  init_error_params();
  error_msg_strings[1] = error_text(file_kind);
  error_msg_strings[2] = file_name;
  if (reason != NULL) {
    error_msg_strings[3] = reason;
  }  /* if */
  /* If the error is to be issued as a command-line error, provide an
     appropriate source position. */
  if (severity == (an_error_severity)es_command_line_error) {
    set_position_to(local_error_pos, 0, SP_COL_CMD_LINE);
  }  /* if */

  diag_message(error_code, &local_error_pos, severity, dck_standalone);
}  /* file_open_error_full */


void file_open_error(an_error_severity		severity,
		     an_error_code		file_kind,
                     char			*file_name,
		     an_open_file_result	*open_result)
/*
Write an error message about opening the file named file_name.  file_kind is
an error code for a message that describes the kind of file being opened.
open_result is the entry returned by the file open routine that describes
the kind of failure.
*/
{
  file_open_error_full(severity, file_kind, file_name, open_result,
                       &error_position);
}  /* file_open_error */


DOES_NOT_RETURN output_file_open_error(a_boolean         bad_name,
                                       an_error_code     file_kind,
                                       char              *file_name,
                                       an_error_severity severity)
/*
Write an error message about opening the output file named file_name,
and terminate the compilation.  This routine is used in contexts in which
a file open result is not available.   file_kind is an error code for a
message that describes the kind of file being opened.  Normally a "cannot open
file" error is issued, but if bad_name is TRUE an "illegal file name"
error is issued instead.
*/
{
  an_open_file_result	open_result;

  clear_open_file_result(&open_result);
  if (bad_name) open_result.flags |= OFR_BAD_NAME;
  /* If the error is to be issued as a command-line error, provide an
     appropriate source position. */
  if (severity == (an_error_severity)es_command_line_error) {
    set_position_to(error_position, 0, SP_COL_CMD_LINE);
  }  /* if */
  file_open_error(severity, file_kind, file_name, &open_result);
#ifdef __GNUC__
  /* Avoid gcc warning.  file_open_error does not return in this case. */
  exit_compilation(es_internal_error);
#endif /* __GNUC__ */
}  /* output_file_open_error */


DOES_NOT_RETURN file_write_error(an_error_code	file_kind,
				 int		errno_value)
/*
Issue an error that a write to the file specified by file_kind failed.
errno_value provides information about the cause of the failure.
*/
{
  if (errno_value == 0) {
    pos_st_catastrophe(ec_file_write_error, &error_position,
                       error_text(file_kind));
  } else {
    error_code_errno_catastrophe(ec_file_write_error_errno,
                                 file_kind, errno_value);
  }  /* if */
}  /* file_write_error */


static a_boolean open_error_should_be_issued(
					an_open_file_flag_set	open_flags,
					an_open_file_result	*open_result,
					an_error_severity	*severity)
/*
The open of a file failed with open_result.  Based on open_flags, determine
whether an error should be issued or a NULL file pointer should be returned
to the caller.  Return TRUE if an error should be issued.  Set severity
to the error severity of the error to be issued.
*/
{
  a_boolean	issue_error = FALSE;

  if ((open_result->flags & OFR_NOT_FOUND) != 0 &&
      (open_flags & OFF_OKAY_IF_NOT_FOUND) == 0) {
    issue_error = TRUE;
  } else if ((open_result->flags & OFR_CANNOT_OPEN) != 0 &&
             (open_flags & OFF_OKAY_IF_CANNOT_OPEN) == 0) {
    issue_error = TRUE;
  } else if ((open_result->flags & OFR_NOT_REGULAR) != 0 &&
             (open_flags & OFF_OKAY_IF_NOT_REGULAR) == 0) {
    issue_error = TRUE;
  } else if ((open_result->flags & OFR_IS_DIRECTORY) != 0 &&
             (open_flags & OFF_OKAY_IF_DIRECTORY) == 0) {
    issue_error = TRUE;
  } else if ((open_result->flags & OFR_BAD_NAME) != 0) {
    issue_error = TRUE;
#if DEBUG
  } else if ((open_flags & OFF_FORCE_ERROR) != 0) {
    /* If this debug flag is specified, force an error to be issued. */
    issue_error = TRUE;
#endif /* DEBUG */
  }  /* if */
  *severity = (open_flags & OFF_COMMAND_LINE) != 0 ? es_command_line_error
                                                   : es_catastrophe;
  return issue_error;
}  /* open_error_should_be_issued */


FILE *open_source_file_with_error_handling(
				char			*file_name,
				an_open_file_flag_set	open_flags,
				an_open_file_result	*open_result,
				a_unicode_source_kind	*unicode_source_kind)
/*
Open the given file as a source input file, and return a pointer to the
file, or NULL if the file cannot be opened (and no error is issued).
open_flags indicates the cases in which NULL should be returned and
the cases in which an error should be issued.  *unicode_source_kind is set
to indicate the Unicode encoding form for the file, or usk_none if the file
is not Unicode.
*/
{
  FILE			*file;
  an_error_severity	severity;

  file = open_source_file(file_name, open_result, unicode_source_kind);
#if DEBUG
  /* Pretend the open failed if OFF_FORCE_ERROR is specified. */
  if ((open_flags & OFF_FORCE_ERROR) != 0) {
    if (file != NULL) (void)fclose(file);
    file = NULL;
  }  /* if */
#endif /* DEBUG */
  if (file == NULL &&
      open_error_should_be_issued(open_flags, open_result, &severity)) {
    /* Note that file_open_error does not return when called from here. */
    file_open_error(severity, ec_source, file_name, open_result);
  }  /* if */
  return file;
}  /* open_source_file_with_error_handling */


void close_output_file_with_error_handling(FILE			**f_output,
					   an_error_code	file_kind)
/*
Check for errors in writing an output file and close it.  Issue a diagnostic
if an error was detected while the file was being closed.  The file variable
passed by the caller is cleared.  file_kind is an error code for a message
that describes the kind of file being closed.
*/
{
  if (*f_output != NULL) {
    int		errno_value;
    /* Make a copy of the file variable and clear the copy from the caller. */
    FILE	*f_temp = *f_output;
    *f_output = NULL;
    if (close_output_file(f_temp, &errno_value)) {
      file_write_error(file_kind, errno_value);
    }  /* if */
  }  /* if */
}  /* close_output_file_with_error_handling */


FILE *fopen_with_error(char			*file_name,
		       char			*mode,
		       an_open_file_flag_set	open_flags,
		       an_error_code		file_kind)
/*
Open the given file_name using mode as the open mode.  Return the file
pointer, or NULL if the file cannot be opened (and no error is issued).
open_flags indicates the cases in which NULL should be returned and
the cases in which an error should be issued.  file_kind is the error
code for the description of the file to be used if an error is issued.
*/
{
  FILE			*file;
  an_open_file_result	open_result;
  an_error_severity	severity;

  file = fopen_with_result(file_name, mode, &open_result);
#if DEBUG
  /* Pretend the open failed if OFF_FORCE_ERROR is specified. */
  if ((open_flags & OFF_FORCE_ERROR) != 0) {
    if (file != NULL) (void)fclose(file);
    file = NULL;
  }  /* if */
#endif /* DEBUG */
  if (file == NULL &&
      open_error_should_be_issued(open_flags, &open_result, &severity)) {
    /* Note that file_open_error does not return when called from here. */
    file_open_error(severity, file_kind, file_name, &open_result);
  }  /* if */
  return file;
}  /* fopen_with_error */


FILE *open_output_file_with_error_handling(
					char			*file_name,
					a_boolean		binary_file,
					a_boolean		update_mode,
					an_open_file_flag_set	open_flags,
					an_error_code		file_kind)
/*
Open the given file_name as an output file.  binary_file is TRUE if
the file should be opened as a binary file instead of a text file.
update_mode is TRUE if the file should be opened in update mode so it
can be read as well as written.  Return the file pointer, or NULL if
the file cannot be opened (and no error is issued).  open_flags
indicates the cases in which NULL should be returned and the cases
in which an error should be issued.  file_kind is the error code for
the description of the file to be used if an error is issued.
*/
{
  FILE			*file;
  an_open_file_result	open_result;
  an_error_severity	severity;

  file = open_output_file(file_name, binary_file, update_mode, &open_result);
#if DEBUG
  /* Pretend the open failed if OFF_FORCE_ERROR is specified. */
  if ((open_flags & OFF_FORCE_ERROR) != 0) {
    if (file != NULL) (void)fclose(file);
    file = NULL;
  }  /* if */
#endif /* DEBUG */
  if (file == NULL &&
      open_error_should_be_issued(open_flags, &open_result, &severity)) {
    /* Note that file_open_error does not return when called from here. */
    file_open_error(severity, file_kind, file_name, &open_result);
  }  /* if */
  return file;
}  /* open_output_file_with_error_handling */


FILE *open_input_file_with_error_handling(
				char			*file_name,
				a_boolean		binary_file,
				an_open_file_flag_set	open_flags,
				an_error_code		file_kind)
/*
Open the given file_name as an input file.  Return the file pointer, or
NULL if the file cannot be opened (and no error is issued).  binary_file
is TRUE if the file should be opened as a binary file instead of a
text file.  open_flags indicates the cases in which NULL should be
returned and the cases in which an error should be issued.  file_kind
is the error code for the description of the file to be used if an
error is issued.
*/
{
  FILE			*file;
  an_open_file_result	open_result;
  an_error_severity	severity;

  file = open_input_file(file_name, binary_file, &open_result);
#if DEBUG
  /* Pretend the open failed if OFF_FORCE_ERROR is specified. */
  if ((open_flags & OFF_FORCE_ERROR) != 0) {
    if (file != NULL) (void)fclose(file);
    file = NULL;
  }  /* if */
#endif /* DEBUG */
  if (file == NULL &&
      open_error_should_be_issued(open_flags, &open_result, &severity)) {
    /* Note that file_open_error does not return when called from here. */
    file_open_error(severity, file_kind, file_name, &open_result);
  }  /* if */
  return file;
}  /* open_input_file_with_error_handling */


#if !STANDALONE_UTILITY_PROGRAM

static void display_trans_unit_context(
			a_source_position	*error_pos,
			an_error_severity	severity,
			a_boolean		add_detected_prefix)
/*
This routine is called when the diagnostic that is being issued is for
a translation unit other than the primary one.  Output a context message
that indicates the translation unit that is associated with the message.

error_pos and severity are the position and severity of the original message.

When add_detected_prefix is TRUE, the error code returned will refer to
a message that includes the text (e.g., "detected during ") that is
used when only a single line of context information is being supplied.
When multiple context lines are being displayed, the "detected during"
message appears by itself on a separate line.
*/
{
  an_error_code			context_error_code;
  a_diagnostic_category_kind	context_diag_kind;

  /* If only one line of context is being issued, then it is
     considered the "primary" context line.  Otherwise a header
     was issued above and the context lines are handled as list
     elements. */
  if (add_detected_prefix) {
    context_diag_kind = dck_context_primary;
    context_error_code = ec_det_during_compilation_of_secondary_trans_unit;
  } else {
    context_diag_kind = dck_list;
    context_error_code = ec_compilation_of_secondary_trans_unit_context;
  }  /* if */
  init_error_params();
  error_msg_strings[1] = format_file_name(diag_primary_source_file->file_name);
  diag_message(context_error_code, error_pos, severity, context_diag_kind);
}  /* display_trans_unit_context */


static a_boolean in_secondary_translation_unit(a_source_position *pos)
/*
Determine whether "pos" represents a position in a secondary translation
unit.  If so, set a global variable to indicate the source file that is
associated with the translation unit that it is in.
*/
{
  a_boolean	result;

  if (!in_front_end) {
    /* The translation unit data structure cannot be used after the front
       end has completed. */
    result = FALSE;
  } else if (translation_units == NULL ||
             translation_units->next == NULL) {
    /* Optimize the case where there is only one translation unit. */
    result = FALSE;
  } else {
    /* Get the primary source file associated with this position. */
    diag_primary_source_file = primary_source_file_for_seq(pos->seq);
    /* It is a secondary translation unit if it is not the first entry
       on the list. */
    result = diag_primary_source_file != NULL &&
             diag_primary_source_file != translation_units->source_file;
  }  /* if */
  return result;
}  /* in_secondary_translation_unit */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void diag_message(an_error_code              error_code,
                         a_source_position          *error_pos,
                         an_error_severity          severity,
                         a_diagnostic_category_kind diag_kind)
/*
Construct and write a diagnostic message.  The error code is error_code, and
the position of the error is *error_pos.  severity gives the severity (e.g.,
es_warning) and diag_kind indicates if this is a single diagnostic message or
one message in a related list of messages.  The linked list of message
segments that comprise the diagnostic is based on the error message
template associated with error_code.  After constructing the segment list
and doing any required expansions, the diagnostic is written.
*/
{
  a_msg_segment_ptr  curr_seg;
  char               *msg_template;
  a_boolean	     diag_should_be_issued;
#if CHECKING
  int                i;
#endif /* CHECKING */

  diag_should_be_issued = check_severity(error_code, &error_pos,
                                         &severity, diag_kind);
#if !STANDALONE_UTILITY_PROGRAM
  if (diag_should_be_issued) {
    /* Determine whether this diagnostic should not be issued because
       of the use of the "once" diagnostic control. */
    diag_should_be_issued = !diagnostic_already_issued_for_diag_once(
                                                         error_code, severity);
  }  /* if */
  if (diag_should_be_issued) {
    /* Suppress the diagnostic if it has already been issued during the
       prototype instantiation. */
    diag_should_be_issued =
                !diagnostic_already_issued_for_prototype(error_code, severity,
                                                         error_pos, diag_kind);
  }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
  if (diag_should_be_issued) {
#if !STANDALONE_UTILITY_PROGRAM
    if (curr_command_line_macro_def != NULL) {
      /* An error occurred while scanning a command-line macro definition.
         Ignore the original error and issue a general error indicating
         that the macro definition is invalid. */
      str_command_line_error(ec_bad_cmd_line_macro,
                             curr_command_line_macro_def);
    }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    if (severity == es_catastrophe &&
        (diag_kind == dck_standalone || diag_kind == dck_primary)) {
      /* Make sure that if catastrophic error leads to another, we abort
         the compilation instead of looping. */
      if (catastrophe_has_occurred) {
        fprintf(f_error, "%s\n", error_text(ec_catastrophic_error_loop));
        term_compilation(es_catastrophe);
      }  /* if */
      catastrophe_has_occurred = TRUE;
    }  /* if */
    /* Get the error message text (template) and construct the message
       segment list. */
    if (diag_kind != dck_end_list && diag_kind != dck_end_context) {
      msg_template = error_text(error_code);
    } else {
      msg_template = "";
    }  /* if */
    construct_message_segments(msg_template);

    /* Walk through the message segments and complete any required 
       expansion. */
    for (curr_seg = error_message_head;
         curr_seg != NULL &&
            curr_seg->kind != (a_message_segment_kind)msk_last;
         curr_seg = curr_seg->next ) {
      switch (curr_seg->kind) {
        case msk_error_text_part:
          /* No processing needed. */
          break;
        case msk_user_string:
#if CHECKING
          if (error_msg_strings[curr_seg->sequence_no] == NULL) {
            internal_error(
                  "diag_message: missing string substitution");
          }  /* if */
#endif /* CHECKING */
          if (curr_seg->variant.string.quoted) {
            /* Rebuild the user string surrounded by double quotes. */
            add_string_to_segment("\"", curr_seg);
            curr_seg->first_quote = curr_seg->segment + curr_seg->length - 1;
            add_string_to_segment(error_msg_strings[curr_seg->sequence_no],
                                  curr_seg);
            add_string_to_segment("\"", curr_seg);
            curr_seg->second_quote = curr_seg->segment + curr_seg->length - 1;
          }  /* if */
          break;
        case msk_type:
#if CHECKING
          if (error_msg_types[curr_seg->sequence_no] == NULL) {
            internal_error("diag_message: missing type substitution");
          }  /* if */
#endif /* CHECKING */
          form_type_summary(error_msg_types[curr_seg->sequence_no], curr_seg);
          break;
        case msk_source_position:
#if !STANDALONE_UTILITY_PROGRAM
#if CHECKING
          if (error_msg_positions[curr_seg->sequence_no] == NULL) {
            internal_error("diag_message: missing position substitution");
          }  /* if */
#endif /* CHECKING */
          form_source_position(error_msg_positions[curr_seg->sequence_no],
                               error_pos, "", "", "", curr_seg);
#endif /* !STANDALONE_UTILITY_PROGRAM */
          break;
        case msk_symbol:
#if !STANDALONE_UTILITY_PROGRAM
#if CHECKING
          if (error_msg_syms[curr_seg->sequence_no] == NULL) {
            internal_error("diag_message: missing symbol substitution");
          }  /* if */
#endif /* CHECKING */
          form_symbol_summary(error_msg_syms[curr_seg->sequence_no],
                              error_pos, curr_seg);
#endif /* !STANDALONE_UTILITY_PROGRAM */
          break;
        default:
          unexpected_condition_str("diag_message: bad message kind");
      }  /* switch */
    }  /* for */
#if CHECKING
    for (i = 1; i <= MAX_ERR_SEG_KIND_PER_MSG; i++) {
      if (error_msg_strings[i] != NULL) {
        check_if_fill_in_used(msk_user_string, i, error_code);
      }  /* if */
      if (error_msg_types[i] != NULL) {
        check_if_fill_in_used(msk_type, i, error_code);
      }  /* if */
#if !STANDALONE_UTILITY_PROGRAM
      if (error_msg_syms[i] != NULL) {
        check_if_fill_in_used(msk_symbol, i, error_code);
      }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    }  /* for */
#endif /* CHECKING */
#if STANDALONE_UTILITY_PROGRAM
    write_diagnostic(error_code, error_pos, severity, diag_kind);
#else /* !STANDALONE_UTILITY_PROGRAM */
    /* Certain conditions, such as errors that occur while instantiating
       template classes and functions, require additional context information
       to be supplied after the message is printed.  The context is printed
       following standalone messages and after the end of a list of messages.
       The processing is done in two phases.  First, we determine whether
       any context information is required.  This is needed because the
       processing of the initial message is handled slightly differently
       if context is to follow (for example, the error limit check is not
       done until the end of the context information is printed).  Once we
       know whether context is required, the original message is issued.
       Then we issue and context-related messages. */
    if ((diag_kind != dck_standalone && diag_kind != dck_end_list) ||
        (severity == es_catastrophe &&
         !display_error_context_on_catastrophe)) {
      /* The context display processing is only required after standalone and
         end-list messages.  The context can optionally be suppressed for
         catastrophic errors. */
      write_diagnostic(error_code, error_pos, severity, diag_kind);
    } else {
      int		num_of_contexts = 0;
      a_symbol_ptr	sym;
      an_error_code	context_error_code;
      a_boolean		pos_is_in_secondary_trans_unit;
      /* Check whether we need to supply additional context information. */
      a_scope_depth	sd;
      for (sd = depth_scope_stack; sd > DEPTH_OF_FILE_SCOPE; --sd) {
        if (include_in_context_output(&scope_stack[sd], &sym,
                                      &context_error_code,
                                      (a_source_position*)NULL,
				      /*add_detected_prefix=*/FALSE)) {
          num_of_contexts++;
        }  /* if */
      }  /* for */
      /* If we are in a secondary translation unit, we need a context line
         to specify the translation unit name. */
      pos_is_in_secondary_trans_unit =
                                      in_secondary_translation_unit(error_pos);
      if (pos_is_in_secondary_trans_unit) num_of_contexts++;
      /* Issue the original message. */
      context_required = num_of_contexts > 0;
      write_diagnostic(error_code, error_pos, severity, diag_kind);
      context_required = FALSE;
      /* Loop through the scope stack and output context information. */
      if (num_of_contexts > 0) {
        a_diagnostic_category_kind	context_diag_kind;
        int				contexts_to_include;
	int				contexts_processed = 0;
        int				contexts_skipped = 0;
        a_boolean			limit_context;
        /* Check whether we should limit the number of context lines emitted.
           Ignore the limit if we are just above it. */
        limit_context = context_limit > 0 &&
                        num_of_contexts > (context_limit + 1);
        contexts_to_include = context_limit / 2;
        if (num_of_contexts != 1) {
          /* If there is more than one line of context we output an
	     initial header line. */
          init_error_params();
          diag_message(ec_template_detected_during_header, error_pos,
                       severity, dck_context_primary);
        }  /* if */
        for (sd = depth_scope_stack; sd > DEPTH_OF_FILE_SCOPE; --sd) {
          a_scope_stack_entry_ptr	ssep = &scope_stack[sd];
          static a_source_position	context_source_pos;
          if (!include_in_context_output(ssep, &sym,
                                         &context_error_code,
                                         &context_source_pos,
					 /*add_detected_prefix=*/
					     num_of_contexts == 1)) continue;
          contexts_processed++;
          /* If we are limiting the number of context lines, see if this is
             an entry that should be excluded. */
          if (limit_context &&
              contexts_processed > contexts_to_include &&
              contexts_processed <= (num_of_contexts - contexts_to_include)) {
            contexts_skipped++;
            continue;
          }  /* if */
          /* When resuming the display of contexts, indicate the number of
             entries not shown. */
          if (contexts_skipped > 0) {
            static char	buffer[50];
            init_error_params();
            (void)sprintf(buffer, "%d", contexts_skipped);
            error_msg_strings[1] = buffer;
            diag_message(ec_context_lines_skipped,
                         error_pos, severity, dck_list);
            contexts_skipped = 0;
          }  /* if */
          /* If only one line of context is being issued, then it is
	     considered the "primary" context line.  Otherwise a header
	     was issued above and the context lines are handled as list
	     elements. */
          if (num_of_contexts == 1) {
 	    context_diag_kind = dck_context_primary;
          } else {
 	    context_diag_kind = dck_list;
          }  /* if */
          init_error_params();
          error_msg_syms[1] = sym;
	  error_msg_positions[1] = &context_source_pos;
          error_msg_scopes[1] = ssep;
          diag_message(context_error_code,
                       error_pos, severity, context_diag_kind);
        }  /* for */
        if (pos_is_in_secondary_trans_unit) {
          display_trans_unit_context(error_pos, severity,
                                     /*add_detected_prefix=*/
                                                         num_of_contexts == 1);
        }  /* if */
        /* Issue an "end context" message to indicate that all of the
           context information has been supplied. */
        init_error_params();
        context_required = FALSE;
        diag_message(ec_no_error, (a_source_position *)NULL, es_none,
                     dck_end_context);
      }  /* if */
    }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
  }  /* if */
}  /* diag_message */

#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
BEGIN_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */

static int compare_tag_info(a_const_void_ptr arg1,
                            a_const_void_ptr arg2)
/*
Function called by bsearch to compare two error_tag_entry records based on
the tag.
*/
{
  an_error_tag_entry_ptr	eip1;
  an_error_tag_entry_ptr	eip2;

  eip1 = (an_error_tag_entry_ptr)arg1;
  eip2 = (an_error_tag_entry_ptr)arg2;
  return strcmp(eip1->tag, eip2->tag);
}  /* compare_tag_info */

#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
END_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */

a_boolean set_severity_for_error_tag(char		*tag,
				     an_error_severity	severity,
				     a_boolean		make_default)
/*
Given an error tag string, this routine looks up the error tag and
updates the table used to override the error severity of diagnostic
messages.  If the tag cannot be found return TRUE, otherwise return
FALSE. make_default is TRUE when this is called for a value set on
the command line or as part of the initial front end configuration.
This causes both the current and default tables to be updated.  For
other calls, only the current table is updated.  If the severity is
"es_default" the severity from the default table is used to reset the
value in the current table.
*/
{
  an_error_tag_entry	        ete_to_find;
  an_error_tag_entry_ptr	etep_found;
  an_error_code			error_code;

  ete_to_find.tag = tag;
  /* Look up the enumeration code in the error_info table. */
  etep_found = (an_error_tag_entry_ptr)
                    bsearch((a_bsearch_arg_type)&ete_to_find,
                            (a_bsearch_arg_type)error_tags,
                            size_t_arg(NUMBER_OF_ERROR_TAGS),
                            sizeof(an_error_tag_entry),
                            compare_tag_info);
  if (etep_found != NULL) {
    error_code = etep_found->code;
    (void)set_severity_for_error_number((int)error_code,
                                        severity, make_default);
  }  /* if */
  /* Return TRUE if the tag could not be found. */
  return etep_found == NULL;
}  /* set_severity_for_error_tag */


a_boolean set_severity_for_error_number(int		   error_number,
				        an_error_severity  severity,
				        a_boolean	   make_default)
/*
Given an error number, this routine updates the table used to override
the error severity of diagnostic messages. If the error number is out
of range return TRUE, otherwise return FALSE.  make_default is TRUE
when this is called for a value set on the command line or as part of
the initial front end configuration.  This causes both the current and
default tables to be updated.  For other calls, only the current table
is updated.  If the severity is "es_default" the severity from the default
table is used to reset the value in the current table.
*/
{
  a_boolean			err;


  err = (error_number <= (int)ec_no_error || error_number >= (int)ec_last);
  if (!err) {
    if (severity == es_default) {
      /* Restore the severity from the default table. */
      current_severity_for_error_code[error_number] =
                                 default_severity_for_error_code[error_number];
    } else if (severity == es_once) {
      /* Set the flag indicating that a given non-error diagnostic
         should be issued only once. */
      once_flag_for_error_code[error_number] = TRUE;
    } else {
      current_severity_for_error_code[error_number] = severity;
      if (make_default) {
        default_severity_for_error_code[error_number] = severity;
      }  /* if */
    }  /* if */
  }  /* if */
  return err;
}  /* set_severity_for_error_number */


void pos_st_diagnostic(an_error_severity error_severity,
                       an_error_code     error_code,
                       a_source_position *error_pos,
                       char              *error_string)
/*
Report the indicated diagnostic message (with the indicated fill-in string)
at the indicated position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  diag_message(error_code, error_pos, error_severity, dck_standalone);
}  /* pos_st_diagnostic */


void pos_diagnostic(an_error_severity  error_severity,
                    an_error_code      error_code,
                    a_source_position  *error_pos)
/*
Report the indicated diagnostic at the indicated position.
*/
{
  pos_st_diagnostic(error_severity, error_code, error_pos, (char *)NULL);
}  /* pos_diagnostic */


void diagnostic(an_error_severity    error_severity,
                an_error_code        error_code)
/*
Report the indicated diagnostic at the position indicated by error_position.
*/
{
  pos_st_diagnostic(error_severity, error_code, &error_position, (char *)NULL);
}  /* diagnostic */


void pos_ty_diagnostic(an_error_severity  error_severity,
                       an_error_code      error_code,
                       a_source_position  *error_pos,
                       a_type_ptr         type)
/*
Report the indicated diagnostic (with the indicated type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, error_severity, dck_standalone);
}  /* pos_ty_diagnostic */


void pos_ty2_diagnostic(an_error_severity  error_severity,
                        an_error_code      error_code,
                        a_source_position  *error_pos,
                        a_type_ptr         type1,
                        a_type_ptr         type2)
/*
Report the indicated diagnostic (with the two indicated types) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type1;
  error_msg_types[2] = type2;
  diag_message(error_code, error_pos, error_severity, dck_standalone);
}  /* pos_ty2_diagnostic */


void type_diagnostic(an_error_severity  error_severity,
                     an_error_code      error_code,
                     a_type_ptr         type)
/*
Report the indicated diagnostic (with the indicated type) at the position
indicated by error_position.
*/
{
  pos_ty_diagnostic(error_severity, error_code, &error_position, type);
}  /* type_diagnostic */


#if !STANDALONE_UTILITY_PROGRAM

void pos_sy_diagnostic(an_error_severity  error_severity,
                       an_error_code      error_code,
                       a_source_position  *error_pos,
                       a_symbol_ptr       symbol)
/*
Report the indicated diagnostic (with the indicated symbol) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, error_severity, dck_standalone);
}  /* pos_sy_diagnostic */


void pos2_diagnostic(an_error_severity  error_severity,
                     an_error_code      error_code,
                     a_source_position  *error_pos,
                     a_source_position  *other_pos)
/*
Report the indicated diagnostic at the indicated position.  A second position
is also provided.
*/
{
  init_error_params();
  error_msg_positions[1] = other_pos;
  diag_message(error_code, error_pos, error_severity, dck_standalone);
}  /* pos2_diagnostic */


void pos2_sy_diagnostic(an_error_severity  error_severity,
                        an_error_code      error_code,
                        a_source_position  *error_pos,
                        a_source_position  *other_pos,
                        a_symbol_ptr       symbol)
/*
Report the indicated diagnostic (with the indicated symbol) at the
indicated position, a second position is also provided.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  error_msg_positions[1] = other_pos;
  diag_message(error_code, error_pos, error_severity, dck_standalone);
}  /* pos2_sy_diagnostic */

#if MICROSOFT_EXTENSIONS_ALLOWED

void pos2_ty_diagnostic(an_error_severity  error_severity,
                        an_error_code      error_code,
                        a_source_position  *error_pos,
                        a_source_position  *other_pos,
                        a_type_ptr         type)
/*
Report the indicated diagnostic (with the indicated type) at the
indicated position, a second position is also provided.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  error_msg_positions[1] = other_pos;
  diag_message(error_code, error_pos, error_severity, dck_standalone);
}  /* pos2_ty_diagnostic */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void pos_sy2_diagnostic(an_error_severity  error_severity,
                        an_error_code      error_code,
                        a_source_position  *error_pos,
                        a_symbol_ptr       symbol1,
                        a_symbol_ptr       symbol2)
/*
Report the indicated diagnostic (with the indicated symbols) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol1;
  error_msg_syms[2] = symbol2;
  diag_message(error_code, error_pos, error_severity, dck_standalone);
}  /* pos_sy_diagnostic */


void sym_diagnostic(an_error_severity  error_severity,
                    an_error_code      error_code,
                    a_symbol_ptr       symbol)
/*
Report the indicated diagnostic (with the indicated symbol) at the position
indicated by error_position.
*/
{
  pos_sy_diagnostic(error_severity, error_code, &error_position, symbol);
}  /* sym_diagnostic */


void pos_syty_diagnostic(an_error_severity  error_severity,
                         an_error_code      error_code,
                         a_source_position  *error_pos,
                         a_symbol_ptr       symbol,
                         a_type_ptr         type)
/*
Report the indicated diagnostic (with the indicated symbol and type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, error_severity, dck_standalone);
}  /* pos_syty_diagnostic */


void pos_stsy_diagnostic(an_error_severity  error_severity,
                         an_error_code      error_code,
                         a_source_position  *error_pos,
                         char               *error_string,
                         a_symbol_ptr       symbol)
/*
Report the indicated diagnostic (with the indicated fill-in string and symbol)
at the indicated position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, error_severity, dck_standalone);
}  /* pos_stsy_diagnostic */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void pos_st_remark(an_error_code     error_code,
                   a_source_position *error_pos,
                   char              *error_string)
/*
Report the indicated remark (with the indicated fill-in string) at the
indicated position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  diag_message(error_code, error_pos, es_remark, dck_standalone);
}  /* pos_st_remark */


void pos_remark(an_error_code     error_code,
                a_source_position *error_pos)
/*
Report the indicated remark at the indicated position.
*/
{
  pos_st_remark(error_code, error_pos, (char *)NULL);
}  /* pos_remark */


void remark(an_error_code error_code)
/*
Report the indicated remark at the position indicated by error_position.
*/
{
  pos_st_remark(error_code, &error_position, (char *)NULL);
}  /* remark */


void pos_ty_remark(an_error_code     error_code,
                   a_source_position *error_pos,
                   a_type_ptr        type)
/*
Report the indicated remark (with the indicated type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, es_remark, dck_standalone);
}  /* pos_ty_remark */


#if 0
/* These routines are not currently used by the compiler. */

void pos_ty2_remark(an_error_code     error_code,
                    a_source_position *error_pos,
                    a_type_ptr        type1,
                    a_type_ptr        type2)
/*
Report the indicated remark (with the two indicated types) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type1;
  error_msg_types[2] = type2;
  diag_message(error_code, error_pos, es_remark, dck_standalone);
}  /* pos_ty2_remark */


void type_remark(an_error_code error_code,
                 a_type_ptr    type)
/*
Report the indicated remark (with the indicated type) at the position
indicated by error_position.
*/
{
  pos_ty_remark(error_code, &error_position, type);
}  /* type_remark */

#endif /* 0 */

#if !STANDALONE_UTILITY_PROGRAM

void pos_sy_remark(an_error_code     error_code,
                   a_source_position *error_pos,
                   a_symbol_ptr      symbol)
/*
Report the indicated remark (with the indicated symbol) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_remark, dck_standalone);
}  /* pos_sy_remark */


void sym_remark(an_error_code error_code,
                a_symbol_ptr  symbol)
/*
Report the indicated remark (with the indicated symbol) at the position
indicated by error_position.
*/
{
  pos_sy_remark(error_code, &error_position, symbol);
}  /* sym_remark */


void pos_stsy_remark(an_error_code     error_code,
                     a_source_position *error_pos,
                     char              *error_string,
                     a_symbol_ptr      symbol)
/*
Report the indicated remark (with the indicated fill-in string and symbol)
at the indicated position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_remark, dck_standalone);
}  /* pos_stsy_remark */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void pos_st_warning(an_error_code     error_code,
                    a_source_position *error_pos,
                    char              *error_string)
/*
Report the indicated warning (with the indicated fill-in string) at the
indicated position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  diag_message(error_code, error_pos, es_warning, dck_standalone);
}  /* pos_st_warning */


void pos_warning(an_error_code     error_code,
                 a_source_position *error_pos)
/*
Report the indicated warning at the indicated position.
*/
{
  pos_st_warning(error_code, error_pos, (char *)NULL);
}  /* pos_warning */


void str_warning(an_error_code error_code,
               char          *error_string)
/*
Report the indicated warning (with the indicated fill-in string) at the
position indicated by error_position.
*/
{
  pos_st_warning(error_code, &error_position, error_string);
}  /* str_warning */

#if MICROSOFT_EXTENSIONS_ALLOWED

void pos_st2_warning(an_error_code     error_code,
                     a_source_position *error_pos,
                     char              *error_string1,
                     char              *error_string2)
/*
Report the indicated warning (with the indicated fill-in strings) at the
position indicated by error_position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string1;
  error_msg_strings[2] = error_string2;
  diag_message(error_code, error_pos, es_warning, dck_standalone);
}  /* pos_str2_warning */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void warning(an_error_code error_code)
/*
Report the indicated warning at the position indicated by error_position.
*/
{
  pos_st_warning(error_code, &error_position, (char *)NULL);
}  /* warning */


void pos_ty_warning(an_error_code     error_code,
                    a_source_position *error_pos,
                    a_type_ptr        type)
/*
Report the indicated warning (with the indicated type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, es_warning, dck_standalone);
}  /* pos_ty_warning */

void pos_ty2_warning(an_error_code     error_code,
                     a_source_position *error_pos,
                     a_type_ptr        type1,
                     a_type_ptr        type2)
/*
Report the indicated warning (with the two indicated types) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type1;
  error_msg_types[2] = type2;
  diag_message(error_code, error_pos, es_warning, dck_standalone);
}  /* pos_ty2_warning */

void pos_opt_ty2_warning(an_error_code     error_code,
                         a_source_position *error_pos,
                         a_type_ptr        type1,
                         a_type_ptr        type2)
/*
Report the indicated warning (with the two indicated types) at the
indicated position.  If the error message has no fill-ins, do not
put the types in the message.
*/
{
  init_error_params();
  /* See if the error message contains a fill-in for a type.  If so,
     put out the types. */
  if (message_has_fill_in(error_code)) {
    error_msg_types[1] = type1;
    error_msg_types[2] = type2;
  }  /* if */
  diag_message(error_code, error_pos, es_warning, dck_standalone);
}  /* pos_opt_ty2_warning */


void type_warning(an_error_code error_code,
                  a_type_ptr    type)
/*
Report the indicated warning (with the indicated type) at the position
indicated by error_position.
*/
{
  pos_ty_warning(error_code, &error_position, type);
}  /* type_warning */

#if !STANDALONE_UTILITY_PROGRAM

void pos_syty_warning(an_error_code     error_code,
                      a_source_position *error_pos,
                      a_symbol_ptr      symbol,
                      a_type_ptr        type)
/*
Report the indicated warning (with the indicated symbol and type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, es_warning, dck_standalone);
}  /* pos_syty_warning */


void pos_sy_warning(an_error_code     error_code,
                    a_source_position *error_pos,
                    a_symbol_ptr      symbol)
/*
Report the indicated warning (with the indicated symbol) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_warning, dck_standalone);
}  /* pos_sy_warning */


void sym_warning(an_error_code error_code,
                 a_symbol_ptr  symbol)
/*
Report the indicated warning (with the indicated symbol) at the position
indicated by error_position.
*/
{
  pos_sy_warning(error_code, &error_position, symbol);
}  /* sym_warning */


void pos_stsy_warning(an_error_code     error_code,
                      a_source_position *error_pos,
                      char              *error_string,
                      a_symbol_ptr      symbol)
/*
Report the indicated warning (with the indicated fill-in string and symbol)
at the indicated position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_warning, dck_standalone);
}  /* pos_stsy_warning */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void pos_stty_warning(an_error_code     error_code,
                      a_source_position *error_pos,
                      char              *error_string,
                      a_type_ptr        type)
/*
Report the indicated warning (with the indicated fill-in string and type) at
the indicated position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, es_warning, dck_standalone);
}  /* pos_stty_warning */


void pos_st_error(an_error_code     error_code,
                  a_source_position *error_pos,
                  char              *error_string)
/*
Report the indicated error (with the indicated fill-in string) at the
indicated position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  diag_message(error_code, error_pos, es_error, dck_standalone);
}  /* pos_st_error */


void pos_st2_error(an_error_code     error_code,
                   a_source_position *error_pos,
                   char              *error_string1,
                   char              *error_string2)
/*
Report the indicated error (with the indicated fill-in strings) at the
indicated position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string1;
  error_msg_strings[2] = error_string2;
  diag_message(error_code, error_pos, es_error, dck_standalone);
}  /* pos_st2_error */


void pos_stty_error(an_error_code     error_code,
                    a_source_position *error_pos,
                    char              *error_string,
                    a_type_ptr        type)
/*
Report the indicated error (with the indicated fill-in string and type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, es_error, dck_standalone);
}  /* pos_stty_error */


void pos_error(an_error_code     error_code,
               a_source_position *error_pos)
/*
Report the indicated error at the indicated position.
*/
{
  pos_st_error(error_code, error_pos, (char *)NULL);
}  /* pos_error */


void str_error(an_error_code error_code,
               char          *error_string)
/*
Report the indicated error (with the indicated fill-in string) at the
position indicated by error_position.
*/
{
  pos_st_error(error_code, &error_position, error_string);
}  /* str_error */


void error(an_error_code error_code)
/*
Report the indicated error at the position indicated by error_position.
*/
{
  pos_st_error(error_code, &error_position, (char *)NULL);
}  /* error */


void pos_ty_error(an_error_code     error_code,
                  a_source_position *error_pos,
                  a_type_ptr        type)
/*
Report the indicated error (with the indicated type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, es_error, dck_standalone);
}  /* pos_ty_error */


void pos_ty2_error(an_error_code     error_code,
                   a_source_position *error_pos,
                   a_type_ptr        type1,
                   a_type_ptr        type2)
/*
Report the indicated error (with the two indicated types) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type1;
  error_msg_types[2] = type2;
  diag_message(error_code, error_pos, es_error, dck_standalone);
}  /* pos_ty2_error */

#if MICROSOFT_EXTENSIONS_ALLOWED

void pos_ty3_error(an_error_code     error_code,
                   a_source_position *error_pos,
                   a_type_ptr        type1,
                   a_type_ptr        type2,
                   a_type_ptr        type3)
/*
Report the indicated error (with the three indicated types) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type1;
  error_msg_types[2] = type2;
  error_msg_types[3] = type3;
  diag_message(error_code, error_pos, es_error, dck_standalone);
}  /* pos_ty3_error */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void pos_opt_ty2_error(an_error_code     error_code,
                       a_source_position *error_pos,
                       a_type_ptr        type1,
                       a_type_ptr        type2)
/*
Report the indicated error (with the two indicated types) at the
indicated position.  If the error message has no fill-ins, do not
put the types in the message.
*/
{
  init_error_params();
  /* See if the error message contains a fill-in for a type.  If so,
     put out the types. */
  if (message_has_fill_in(error_code)) {
    error_msg_types[1] = type1;
    error_msg_types[2] = type2;
  }  /* if */
  diag_message(error_code, error_pos, es_error, dck_standalone);
}  /* pos_opt_ty2_error */


void type_error(an_error_code error_code,
                a_type_ptr    type)
/*
Report the indicated error (with the indicated type) at the position
indicated by error_position.
*/
{
  pos_ty_error(error_code, &error_position, type);
}  /* type_error */

#if !STANDALONE_UTILITY_PROGRAM

void pos_stsy_error(an_error_code     error_code,
                    a_source_position *error_pos,
                    char              *error_string,
                    a_symbol_ptr      symbol)
/*
Report the indicated error (with the indicated fill-in string) at the
indicated position.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_error, dck_standalone);
}  /* pos_stsy_error */


void pos_sy_error(an_error_code     error_code,
                  a_source_position *error_pos,
                  a_symbol_ptr      symbol)
/*
Report the indicated error (with the indicated symbol) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_error, dck_standalone);
}  /* pos_sy_error */


void pos_sy2_error(an_error_code     error_code,
                   a_source_position *error_pos,
                   struct a_symbol   *symbol1,
                   struct a_symbol   *symbol2)
/*
Report the indicated error (with the indicated symbols) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol1;
  error_msg_syms[2] = symbol2;
  diag_message(error_code, error_pos, es_error, dck_standalone);
}  /* pos_sy2_error */


void pos_syty_error(an_error_code     error_code,
                    a_source_position *error_pos,
                    a_symbol_ptr      symbol,
                    a_type_ptr        type)
/*
Report the indicated error (with the indicated symbol and type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_error, dck_standalone);
}  /* pos_syty_error */


void sym_error(an_error_code error_code,
               a_symbol_ptr  symbol)
/*
Report the indicated error (with the indicated symbol) at the position
indicated by error_position.
*/
{
  pos_sy_error(error_code, &error_position, symbol);
}  /* sym_error */

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if !STANDALONE_UTILITY_PROGRAM
void syntax_error(an_error_code error_code)
/*
Report the indicated error at the position indicated by error_position,
then get and throw away tokens until a token is read that is in the set
of stop tokens.  This routine is called to report and recover from syntax
errors.
*/
{
  /* Report the error. */
  error(error_code);

  /* Flush tokens until something in the stop token set turns up. */
  flush_tokens();
}  /* syntax_error */
#endif /* !STANDALONE_UTILITY_PROGRAM */


/*lint -esym(759,pos_st_catastrophe)*/
/*lint -esym(765,pos_st_catastrophe)*/
DOES_NOT_RETURN pos_st_catastrophe(an_error_code     error_code,
                                   a_source_position *error_pos,
                                   char              *error_string)
/*
Report the indicated catastrophic error (with the indicated fill-in string)
at the indicated position, and then terminate the compilation.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  diag_message(error_code, error_pos, es_catastrophe, dck_standalone);
#ifdef __GNUC__
  /* Avoid gcc warning.  diag_message does not return in this case. */
  exit_compilation(es_internal_error);
#endif /* __GNUC__ */
}  /* pos_st_catastrophe */


DOES_NOT_RETURN str_catastrophe(an_error_code error_code,
                                char          *error_string)
/*
Report the indicated catastrophe (with the indicated fill-in string) at the
position indicated by error_position, and then terminate the compilation.
*/
{
  pos_st_catastrophe(error_code, &error_position, error_string);
}  /* str_catastrophe */


DOES_NOT_RETURN pos_str2_catastrophe(an_error_code     error_code,
                                     char              *error_string1,
                                     char              *error_string2,
				     a_source_position *error_pos)
/*
Report the indicated catastrophe (with the indicated fill-in strings) at the
indicated error_position, and then terminate the compilation.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string1;
  error_msg_strings[2] = error_string2;
  diag_message(error_code, error_pos, es_catastrophe, dck_standalone);
#ifdef __GNUC__
  /* Avoid gcc warning.  diag_message does not return in this case. */
  exit_compilation(es_internal_error);
#endif /* __GNUC__ */
}  /* pos_str2_catastrophe */

#if EDG_WIN32
#if CPPCLI_ENABLING_POSSIBLE
#if !STANDALONE_UTILITY_PROGRAM

DOES_NOT_RETURN win32_catastrophe(an_ms_dword   error_code,
                                  char          *error_string)
/*
When a WIN32 API fails, issue a diagnostic that describes the failure.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  error_msg_strings[2] = win32_error_to_str(error_code);
  diag_message(ec_win32_api_error, &error_position,
               es_catastrophe, dck_standalone);
#ifdef __GNUC__
  /* Avoid gcc warning.  diag_message does not return in this case. */
  exit_compilation(es_internal_error);
#endif /* __GNUC__ */
}  /* win32_catastrophe */


DOES_NOT_RETURN hresult_catastrophe(char *error_string)
/*
When a random COM API (or other API that hopefully uses ISetErrorInfo) fails,
this produces a diagnostic that describes the failure.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  error_msg_strings[2] = com_error_to_str();
  diag_message(ec_win32_api_error, &error_position,
               es_catastrophe, dck_standalone);
#ifdef __GNUC__
  /* Avoid gcc warning.  diag_message does not return in this case. */
  exit_compilation(es_internal_error);
#endif /* __GNUC__ */
}  /* hresult_catastrophe */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* CPPCLI_ENABLING_POSSIBLE */
#endif /* EDG_WIN32 */

DOES_NOT_RETURN str_errno_catastrophe(an_error_code error_code,
                                      char          *error_string,
                                      int           errno_value)
/*
Report the indicated catastrophe with the fill-in string error_string and
with errno converted to a string fill-in at the position indicated by
error_position, and then terminate the compilation.
*/
{
  pos_str2_catastrophe(error_code, error_string,
                       strerror(errno_value), &error_position);
}  /* str_errno_catastrophe */


static DOES_NOT_RETURN error_code_errno_catastrophe(
                                      an_error_code error_code,
                                      an_error_code error_code2,
                                      int           errno_value)
/*
Report the indicated catastrophe with the indicated fill-in strings
and with errno converted to a string fill-in at the position indicated by
error_position, and then terminate the compilation.
*/
{
  pos_str2_catastrophe(error_code, error_text(error_code2),
                       strerror(errno_value), &error_position);
}  /* error_code_errno_catastrophe */


DOES_NOT_RETURN catastrophe(an_error_code error_code)
/*
Report the indicated catastrophe at the position indicated by error_position,
and then terminate the compilation.
*/
{
  pos_st_catastrophe(error_code, &error_position, (char *)NULL);
}  /* catastrophe */


/* The following routines are used to construct multiple message
   diagnostics with various fill-ins. */
void pos_start_diagnostic(an_error_severity  error_severity,
                          an_error_code      error_code,
                          a_source_position  *error_pos)
/*
Begin a multiple message diagnostic with the specified severity, error code,
and source position.
*/
{
  init_error_params();
  diag_message(error_code, error_pos, error_severity, dck_primary);
}  /* pos_start_diagnostic */


void pos_ty_start_diagnostic(an_error_severity  error_severity,
                             an_error_code      error_code,
                             a_source_position *error_pos,
                             struct a_type     *type)
/*
Begin a multiple message diagnostic with the specified severity, error code,
source position, and type fill-in.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, error_severity, dck_primary);
}  /* pos_ty_start_diagnostic */


void pos_start_error(an_error_code     error_code,
                     a_source_position *error_pos)
/*
Begin a multiple message error with the specified error code and source
position.
*/
{
  init_error_params();
  diag_message(error_code, error_pos, es_error, dck_primary);
}  /* pos_start_error */


void pos_st_start_error(an_error_code     error_code,
                        a_source_position *error_pos,
                        char              *error_string)
/*
Begin a multiple message error with the specified error code, source
position and string fill-in.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  diag_message(error_code, error_pos, es_error, dck_primary);
}  /* pos_st_start_error */


void pos_ty_start_error(an_error_code     error_code,
                        a_source_position *error_pos,
                        a_type_ptr        type)
/*
Begin a multiple message error with the specified error code, source
position and type fill-in.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, es_error, dck_primary);
}  /* pos_ty_start_error */


void pos_ty2_start_error(an_error_code     error_code,
                         a_source_position *error_pos,
                         a_type_ptr        type1,
                         a_type_ptr        type2)
/*
Begin a multiple message error with the specified error code, source
position, and 2 types as fill-ins.
*/
{
  init_error_params();
  error_msg_types[1] = type1;
  error_msg_types[2] = type2;
  diag_message(error_code, error_pos, es_error, dck_primary);
}  /* pos_ty2_start_error */


void ty_add_diag_info(an_error_code error_code,
                      a_type_ptr    type)

/*
Add the specified diagnostic message with the type substitution to the
multiple message diagnostic being processed.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  diag_message(error_code, (a_source_position *)NULL, es_none, dck_list);
}  /* ty_add_diag_info */


void str_add_diag_info(an_error_code error_code,
                       char          *error_string)
/*
Add the specified diagnostic message with the string substitution to the
multiple message diagnostic being processed.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  diag_message(error_code, (a_source_position *)NULL, es_none, dck_list);
}  /* str_add_diag_info */


void add_diag_info(an_error_code error_code)
/*
Add the specified diagnostic message to the multiple message diagnostic
being processed.
*/
{
  init_error_params();
  diag_message(error_code, (a_source_position *)NULL, es_none, dck_list);
}  /* add_diag_info */

#if !STANDALONE_UTILITY_PROGRAM

void add_diag_info_with_pos_insert(an_error_code      error_code,
                                   a_source_position  *pos)
/*
Add the specified diagnostic message to the multiple message diagnostic
being processed.  pos refers to a source position that will be embedded
in the message text using the "%p" convention.
*/
{
  init_error_params();
  error_msg_positions[1] = pos;
  diag_message(error_code, (a_source_position *)NULL, es_none, dck_list);
}  /* add_diag_info_with_pos_insert */


void pos_sy_start_diagnostic(an_error_severity  error_severity,
                             an_error_code      error_code,
                             a_source_position  *error_pos,
                             a_symbol_ptr       symbol)
/*
Begin a multiple message diagnostic with the specified severity, error code,
source position, and symbol fill-in.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, error_severity, dck_primary);
}  /* pos_sy_start_diagnostic */


void pos_sy_start_error(an_error_code     error_code,
                        a_source_position *error_pos,
                        a_symbol_ptr      symbol)
/*
Begin a multiple message error with the specified error code, source
position and symbol fill-in.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_error, dck_primary);
}  /* pos_sy_start_error */


void pos_stsy_start_error(an_error_code     error_code,
                          a_source_position *error_pos,
                          char              *error_string,
                          a_symbol_ptr      symbol)
/*
Begin a multiple message error with the specified error code, source
position and symbol fill-in.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_error, dck_primary);
}  /* pos_stsy_start_error */

#if 0
/* This routine is not currently used by the compiler. */

void pos_sy_start_warning(an_error_code     error_code,
                          a_source_position *error_pos,
                          a_symbol_ptr      symbol)
/*
Begin a multiple message warning with the specified error code, source
position and symbol fill-in.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_warning, dck_primary);
}  /* pos_sy_start_warning */

#endif /* 0 */

void pos_sy2_warning(an_error_code     error_code,
                     a_source_position *error_pos,
                     struct a_symbol   *symbol1,
                     struct a_symbol   *symbol2)
/*
Report the indicated warning (with the indicated symbols) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol1;
  error_msg_syms[2] = symbol2;
  diag_message(error_code, error_pos, es_warning, dck_standalone);
}  /* pos_sy2_warning */


void sym_add_diag_info(an_error_code error_code,
                       a_symbol_ptr  symbol)
/*
Add the specified diagnostic message with the symbol substitution to the
multiple message diagnostic being processed.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, (a_source_position *)NULL , es_none, dck_list);
}  /* sym_add_diag_info */


void pch_message(an_error_code error_code,
		 char	    *fill_in_str)
/*
Display a message of the form:

source_file: creating precompiled header file "file".

This is used to display the messages that indicate a precompiled header is
being created or used.  The text from the error message file must supply
two string fill-ins for the source file name and PCH file name.
*/
{
  char	*text;

  if (!suppress_pch_messages) {
    text = error_text(error_code);
    fprintf(f_error, text, primary_source_file_name, fill_in_str);
    fprintf(f_error, "\n");
  }  /* if */
}  /* pch_message */


void embedded_cplusplus_noncompliance_diagnostic(a_source_position  *error_pos,
                                                 an_error_code      error_code)
/*
Issue a discretionary error for use of a feature that does not belong to the
"Embedded C++" subset.  *error_pos is the source position with which the
diagnostic is associated; error_code indicates the message to be issued.
*/
{
  an_error_severity  severity;

  /* Implementations may elect to hard-code a reduction in the severity of
     of the diagnostics.  Otherwise, users can control it from the command
     line (e.g., --diag_warning=ec_not_part_of_embedded_cplusplus). */
  severity = es_discretionary_error;
  pos_diagnostic(severity, error_code, error_pos);
}  /* embedded_cplusplus_noncompliance_diagnostic */

void diag_pragma(a_pending_pragma_ptr	ppp)
/*
The routine called when a "diag_xxx" pragma is encountered.  The form of
the pragma is

	#pragma diag_xxx [=] arg, arg, arg

where "arg" is either an error number or an error tag.
*/
{
  a_pragma_kind_description_ptr	pkdp = ppp->descr_ptr;
  a_pragma_kind			kind = pkdp->kind;
  an_error_severity		severity;
  a_boolean			error_in_pragma = FALSE;

  /* Convert the pragma kind into an error severity. */
  switch (kind) {
    case pk_diag_suppress: severity = es_none;                break;
    case pk_diag_remark:   severity = es_remark;              break;
    case pk_diag_warning:  severity = es_warning;             break;
    case pk_diag_error:    severity = es_discretionary_error; break;
    case pk_diag_once:     severity = es_once;                break;
    case pk_diag_default:  severity = es_default;             break;
    default: unexpected_condition();
  }  /* switch */
  begin_rescan_of_pragma_tokens(ppp);
  /* Bypass the optional "=". */
  if (curr_token == tok_assign) (void)get_token();
  do {
    a_boolean			err = FALSE;
    if (curr_token == tok_int_constant) {
      /* The argument is an integer, which is expected to be an error
         number. */
      a_host_large_integer	error_number;
      error_number = value_of_integer_constant(&const_for_curr_token, &err);
      if (!err) {
        /* The routine will return TRUE if the number is invalid. */
        err = set_severity_for_error_number((int)error_number, severity,
                                            /*make_default=*/FALSE);
      }  /* if */
      if (err) {
        pos_warning(ec_invalid_error_number, &pos_curr_token);
      }  /* if */
    } else if (curr_token == tok_identifier) {
      /* The argument is an identifier, which is expected to name an error
         tag. */
      char	*error_tag;
      error_tag = locator_for_curr_id.symbol_header->identifier;
      /* The routine will return TRUE if the tag is invalid. */
      err = set_severity_for_error_tag(error_tag, severity,
                                       /*make_default=*/FALSE);
      if (err) {
        pos_warning(ec_invalid_error_tag, &pos_curr_token);
      }  /* if */
    } else {
      /* Not an error number or an error tag. */
      pos_warning(ec_exp_error_argument, &pos_curr_token);
    }  /* if */
    /* Bypass the token just processed. */
    (void)get_token();
    if (curr_token != tok_comma && curr_token != tok_end_of_source) {
      pos_warning(ec_exp_comma, &pos_curr_token);
      error_in_pragma = TRUE;
    }  /* if */
  } while (loop_token(tok_comma));
  /* Stop rescanning tokens from the pragma token cache. */
  wrapup_rescan_of_pragma_tokens(error_in_pragma);
}  /* diag_pragma */


#endif /* !STANDALONE_UTILITY_PROGRAM */

void end_error(void)
/*
Complete the multiple message diagnostic currently being processed.
*/
{
  init_error_params();
  diag_message(ec_no_error, (a_source_position *)NULL, es_none, dck_end_list);
}  /* end_error */

void start_command_line_error(an_error_code      error_code,
			      char		 *error_string)
/*
Begin a multiple message command line error.
*/
{
  init_error_params();
  set_position_to(error_position, 0, SP_COL_CMD_LINE);
  error_msg_strings[1] = error_string;
  diag_message(error_code, &error_position, es_command_line_error,
               dck_primary);
}  /* start_command_line_error */


DOES_NOT_RETURN end_command_line_error(void)
/*
Complete the multiple command line error being processed.
*/
{
  init_error_params();
  diag_message(ec_no_error, (a_source_position *)NULL, es_none, dck_end_list);
#ifdef __GNUC__
  /* Avoid gcc warning.  diag_message does not return in this case. */
  exit_compilation(es_internal_error);
#endif /* __GNUC__ */
}  /* end_command_line_error */


void error_early_init(void)
/*
Do initialization that needs to be done very early, specifically before command
line processing is done.
*/
{
#if CHECKING
  internal_error_loop = FALSE;
#endif /* CHECKING */
  /* The initialization of f_error is also done in cfe.c, but is done here
     also so that it will be reset if the front end is reinitialized. */
  f_error = stderr;
#if DEBUG
  f_debug = stderr;
#endif /* DEBUG */
  write_diagnostic_buffer = NULL;
  write_message_buffer = NULL;
  write_diagnostic_recursion_level = 0;
  catastrophe_has_occurred = FALSE;
  error_threshold = es_warning;
  cs_saved_severity = (an_error_severity)es_default;
  error_limit = 100;
  context_limit = DEFAULT_CONTEXT_LIMIT;
  strict_ansi_error_severity = es_warning;
  strict_ansi_discretionary_severity = es_warning;
  /* These are initialized here and also during per-compilation
     initialization. */
  total_remarks = total_warnings = total_errors = total_catastrophes = 0;
  anachronism_error_severity
#if DEFAULT_ALLOW_ANACHRONISMS
                             = es_warning;
#else /* DEFAULT_ALLOW_ANACHRONISMS */
                             = es_error;
#endif /* DEFAULT_ALLOW_ANACHRONISMS */
  brief_diagnostics = DEFAULT_BRIEF_DIAGNOSTICS;
  do_not_wrap_diagnostics = FALSE;
  display_error_context_on_catastrophe =
                                  DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE;
  display_template_typedefs_in_diagnostics =
                              DEFAULT_DISPLAY_TEMPLATE_TYPEDEFS_IN_DIAGNOSTICS;
#if FULLY_RESOLVED_MACRO_POSITIONS
  macro_positions_in_diagnostics = DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  error_message_head = NULL;
  /* Zeroing this array causes it to be set to es_default. */
  memzero((a_void_ptr)default_severity_for_error_code,
           sizeof(default_severity_for_error_code));
  /* Zeroing this array causes it to be set to es_default. */
  memzero((a_void_ptr)current_severity_for_error_code,
           sizeof(current_severity_for_error_code));
  memzero((a_void_ptr)once_flag_for_error_code,
           sizeof(once_flag_for_error_code));
#if !STANDALONE_UTILITY_PROGRAM
  error_source_line = NULL;
  after_end_of_error_source_line = NULL;
  f_err_src_file = NULL;
#endif /* !STANDALONE_UTILITY_PROGRAM */
}  /* error_early_init */


void error_one_time_init(void)
/*
Do one-time initialization of variables related to the error routines.
(Variables that need to be reinitialized with each new translation unit
are handled in error_init.)
*/
{
#if !STANDALONE_UTILITY_PROGRAM
  /* Save variables from error.h and error.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(head_of_file_index_list),
      pch_saved_var_array_elem(tail_of_file_index_list),
      pch_saved_var_array_elem(error_position),
      pch_array_saved_var_array_elem(default_severity_for_error_code),
      pch_array_saved_var_array_elem(current_severity_for_error_code),
      pch_array_saved_var_array_elem(once_flag_for_error_code),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
#if EXPENSIVE_CHECKING
  verify_label_fill_in_entries();
#endif /* EXPENSIVE_CHECKING */
#endif /* !STANDALONE_UTILITY_PROGRAM */
}  /* error_one_time_init */


void error_trans_unit_init(void)
/*
Initialize variables that are specific to a given translation unit.
*/
{
}  /* error_trans_unit_init */


void error_init(void)
/*
Perform any initializations necessary for error.c functions at the beginning
of each compilation.
*/
{
  total_remarks = total_warnings = total_errors = total_catastrophes = 0;
  memzero((char *)recorded_diagnostic_table,
          sizeof(recorded_diagnostic_table));
  memzero((a_void_ptr)diagnostic_issued_for_error_code,
           sizeof(diagnostic_issued_for_error_code));
#if !STANDALONE_UTILITY_PROGRAM
  clear_file_index_list();
#endif /* !STANDALONE_UTILITY_PROGRAM */
#if CHECKING
  expected_error_record.filename = NULL;
  expected_error_record.line_number = 0;
  expected_error_record.string1 = NULL;
  expected_error_record.string2 = NULL;
#endif /* CHECKING */
  write_diagnostic_recursion_level = 0;
}  /* error_init */

#if !STANDALONE_UTILITY_PROGRAM

#if MAKE_FRONT_END_CALLABLE

void error_cleanup(void)
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason.  It performs any cleanup operations
required.  In particular, it closes any files that may have been open at
the point at which the compilation was terminated.
*/
{
  close_file_if_open(&f_err_src_file);
  /* Reset f_error so that an internal error during initialization will
     be directed to stderr, not wherever the previous compilation directed
     error output. */
  if (f_error != stderr
#if DIRECT_ERROR_OUTPUT_TO_STDOUT
                        && f_error != stdout
#endif /* DIRECT_ERROR_OUTPUT_TO_STDOUT */
                                            ) {
    close_file_if_open(&f_error);
  }  /* if */
  f_error = stderr;
#if DEBUG
  f_debug = stderr;
#endif /* DEBUG */
}  /* error_cleanup */

#endif /* MAKE_FRONT_END_CALLABLE */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
