/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
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
#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Static variable set when a catastrophe occurs, to catch catastrophe loops.
*/
static a_boolean catastrophe_has_occurred;

/*
Constants, structures and static variables used to format diagnostic
messages.
*/

#define NORMAL_DIAG_INDENT 0;	/* The number of spaces to be indented prior
				   to conventional single message
				   diagnostics. */
#define INDENT_AMOUNT 10	/* Number of additional spaces at the start of
				   continuation lines. */
#define LIST_DIAG_INDENT 12	/* The number of spaces to be indented prior
				   to each additional message that is part of
				   a multiple message diagnostic, i.e. an
				   error diagnostic followed by a list of
				   entities.  For appearances, this value
				   should be greater than INDENT_AMOUNT. */

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

static a_source_file_ptr
		diag_primary_source_file;
				/* Pointer to the primary source file
				   associated with the diagnostic currently
				   being processed. */
				   
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
  dck_end_context		/* Like dck_end_list except that the source
				   line is not output. */
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
  int		length;		/* Current length of the message segment. */
  int		max_length;	/* Maximum string size that can be accommodated
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
Diagnostic message substitutions can be based upon strings, types, and symbols
passed to the appropriate diagnostic routines.  The following arrays of
pointers to these various substitution kinds are used to denote the
source of substitutions in message segments.  The sequence number in
message segment descriptor is used as an index into the appropriate array.
*/

#define MAX_ERR_SEG_KIND_PER_MSG 2
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
	error_message_head = NULL;
				/* Pointer to the first segment in the current
				   error message being formatted. */

static an_error_severity
		severity_for_error_code[(int)ec_last + 1];
				/* Array of error severities associated
				   with error codes.  Static initialization
				   results in the array being set to
				   es_default. */

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
static char	*error_source_line = NULL;
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
static char	*after_end_of_error_source_line = NULL;
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
		head_of_file_index_list /*= NULL*/;
				/* Pointer to the beginning of the list of
				   an_error_file_index entries.  Initialized
				   to NULL by error_init(). */
static an_error_file_index_ptr
		tail_of_file_index_list /*= NULL*/;
				/* Pointer to the tail of the list of
				   an_error_file_index entries.  Initialized
				   to NULL by error_init(). */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Structure used to maintain a record of diagnostic messages that have been
issued during prototype instantiations.
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


static char *error_text(an_error_code error_code)
/*
Return a pointer to the error text for the message identified by the given
error code.
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
  int	length_of_string;

  if (str != NULL) {
    length_of_string = strlen(str);
    /* If the string will not fit in the buffer, enlarge the buffer so that it
       will fit.  Allow for a terminating null character. */
    if (seg_ptr->max_length <= (seg_ptr->length + length_of_string)) {
      char	*new_buffer;
      sizeof_t	new_size;

      new_size = seg_ptr->max_length +
                 ((INCR_MSG_SEGMENT_SIZE > length_of_string)
                   ? INCR_MSG_SEGMENT_SIZE + 1 : length_of_string + 1);
      new_buffer = realloc_general(seg_ptr->segment,
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


static void put_str_to_curr_output_msg_segment(char *str)
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
The generated format is:

        <prefix_string>at line xxx of "file name"<suffix string>

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
      add_string_to_segment(prefix_string, seg_ptr);
      add_string_to_segment("at line ", seg_ptr);
#if CHECKING
      if (digits_to_represent((unsigned long)pos->seq)
                          >= BASE_MSG_SEGMENT_SIZE) {
        internal_error("form_source_position: buffer size too small");
      }  /* if */
#endif /* CHECKING */
      (void)sprintf(buffer, "%lu", (unsigned long)line_number);
      add_string_to_segment(&buffer[0], seg_ptr);
      /* Add the file name if needed. */
      if (strcmp(file_name, diag_file_name) != 0 &&
          strcmp(file_name, FILE_NAME_FOR_STDIN) != 0) {
        add_string_to_segment(" of \"", seg_ptr);
        add_string_to_segment(file_name, seg_ptr);
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
          tap = rp->template_arg_list;
          decl_info = tssp->variant.function.decl_cache.decl_info;
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
      parent_sym = (a_symbol_ptr)sym->parent.class_type->
                                                     source_corresp.assoc_info;
      if (template_sym != NULL) {
        check_assertion(template_sym->is_class_member);
        parent_template_sym = (a_symbol_ptr)template_sym->parent.class_type->
                                                     source_corresp.assoc_info;
      } else {
        /* No template symbol was provided by the caller.  If the parent class
           is a template instance, use the prototype instantiation as the
           template symbol. */
        parent_template_sym =
                            prototype_symbol_for_class(sym->parent.class_type);
      }  /* if */
      /* Only display the parent information if the parent class of the
         template is a prototype instantiation.  This suppresses the
         template argument information for the levels at which the
         template has been specialized.  This is also suppressed if the
         parent_sym is a prototype instantiation, to avoid output like
         "[with T=T]" */
      if (parent_template_sym != NULL &&
          is_prototype_instantiation_symbol(parent_template_sym) &&
          !is_prototype_instantiation_symbol(parent_sym)) {
        form_template_arg_info(parent_sym, parent_template_sym, seg_ptr,
                               any_args);
      }  /* if */
    }  /* if */
    if (tap != NULL) {
      /* Display the argument list for this entity. */
      check_assertion(decl_info != NULL);
      tpp = decl_info->parameters;
      for (; tap != NULL; tap = tap->next, tpp = tpp->next) {
        /* Display "parameter=value". */
        if (!*any_args) {
          /* This is the first argument displayed -- add the introduction
             string to the message. */
          add_string_to_segment(" [with ", seg_ptr);
          *any_args = TRUE;
        } else {
          /* This is not the first argument -- add "," separator. */
          add_string_to_segment(", ", seg_ptr);
        }  /* if */
        add_string_to_segment(tpp->param_symbol->header->identifier,
                              seg_ptr);
        add_string_to_segment("=", seg_ptr);
        form_a_template_arg(tap, &octl);
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
     parent_class = sym->parent.class_type;
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
  char				*entity_kind;
  a_boolean			force_function_params = FALSE;
  a_boolean			return_type_needed = TRUE;
  a_boolean			is_declaration_like = FALSE;
  a_symbol_ptr			corresp_template_sym = NULL;
  a_template_instance_ptr	tip = NULL;
  a_symbol_ptr			sym_to_display;

  curr_output_msg_segment = seg_ptr;
  /* Determine the fundamental symbol of this symbol. */
  fund_sym = fundamental_symbol_of(sym);
  switch (fund_sym->kind) {
    case sk_keyword:
      /* The name of a keyword is extracted from the token_names array, and
         is handled differently from other symbols. */
      if (! seg_ptr->variant.symbol.name_only) {
        add_string_to_segment("keyword ", seg_ptr);
      } /* if */
      add_string_to_segment("\"", seg_ptr);
      /* Use the name in the header. */
      add_string_to_segment(sym->header->identifier, seg_ptr);
      break;
    case sk_macro:
      entity_kind = "macro ";
      goto symbol_name;
    case sk_label:
      entity_kind = "label ";
      goto symbol_name;
    case sk_type:
      if (fund_sym->variant.type.ptr->kind == (a_type_kind)tk_template_param) {
        entity_kind = "template parameter ";
      } else {
        entity_kind = "type ";
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
            entity_kind = "union ";
          } else if (C_dialect == C_dialect_cplusplus) {
            entity_kind = "class ";
          } else {
            entity_kind = "struct ";
          }  /* if */
          if (distinct_template_signatures &&
              seg_ptr->variant.symbol.force_template_name_output) {
            /* If the class is a template instance, get the corresponding
               prototype symbol for display purposes. */
            corresp_template_sym =
              prototype_symbol_for_class(sym->variant.class_struct_union.type);
          }  /* if */
          goto symbol_name;
        }  /* if */
      }
      /*FALLTHROUGH*/
    case sk_class_template:
      if (sym->is_template_param) {
        entity_kind = "template template parameter ";
      } else if (sym->kind == (a_symbol_kind)sk_class_template &&
                 sym->variant.template_info->is_nonreal_member) {
        entity_kind = "template ";
      } else {
        entity_kind = "class template ";
      }  /* if */
      goto symbol_name;
    case sk_enum_tag:
      entity_kind = "enum ";
      goto symbol_name;
    case sk_parameter:
      entity_kind = "parameter ";
      type = fund_sym->variant.param_id->type;
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_variable:
      type = fund_sym->variant.variable.ptr->type;
      if (fund_sym->variant.variable.ptr->is_parameter) {
        entity_kind = "parameter ";
      } else if (fund_sym->variant.variable.ptr->is_handler_param) {
        entity_kind = "handler parameter ";
      } else {
        entity_kind = "variable ";
      }  /* if */
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_extern_variable:
      type = fund_sym->variant.extern_symbol_descr->type;
      entity_kind = "variable ";
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
        entity_kind = "nontype ";
      } else {
        entity_kind = "constant ";
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
      entity_kind = "function ";
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_extern_routine:
      type = fund_sym->variant.extern_symbol_descr->type;
      routine = fund_sym->variant.extern_symbol_descr->variant.routine.ptr;
      entity_kind = "function ";
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_overloaded_function:
      entity_kind = "overloaded function ";
      /* There is no specific type information available; this entity cannot
         be expressed as a declaration. */
      goto symbol_name;
    case sk_static_data_member:
      tip = fund_sym->variant.static_data_member.instance_ptr;
      type = fund_sym->variant.static_data_member.variable->type;
      entity_kind = "member ";
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
        entity_kind = "member ";
        is_declaration_like = TRUE;
      } else {
        entity_kind = "field ";
      }  /* if */
      goto symbol_name;
    case sk_namespace:
      entity_kind = "namespace ";
      goto symbol_name;
    case sk_undefined:
      entity_kind = "";
      goto symbol_name;
    case sk_function_template:
      entity_kind = "function template ";
      routine = fund_sym->variant.template_info->variant.function.routine;
      type = routine->type;
symbol_name:
      /* Add the entity kind if not specified as name only or full type for
         a declaration-like entity. */
      if (type == NULL) is_declaration_like = FALSE;
      if (! seg_ptr->variant.symbol.name_only &&
          ! (seg_ptr->variant.symbol.full_type && is_declaration_like) ) {
        add_string_to_segment(entity_kind, seg_ptr);
      } /* if */
      /* Add the beginning double quote. */
      add_string_to_segment("\"", seg_ptr);
      seg_ptr->first_quote = seg_ptr->segment + seg_ptr->length - 1;
      /* Check for special kinds of routines. */
      if (routine != NULL) {
        if (is_constructor_symbol(fund_sym) ||
            is_destructor_symbol(fund_sym) ||
            routine->special_kind == (a_special_function_kind)sfk_conversion) {
          /* The return type is not listed for constructors, destructors, and
             conversion functions. */
          return_type_needed = FALSE;
        }  /* if */
      }  /* if */
      /* Put out the first part of the type if needed, but not for
         constructors, destructors, and conversion functions (the return type
         is not listed for those). */
      if (type != NULL && seg_ptr->variant.symbol.full_type &&
          (routine == NULL || return_type_needed)) {
        form_type_first_part_simple(type,
                                    /*under_lhs_declarator=*/FALSE,
                                    /*need_trailing_space=*/TRUE,
                                    &octl);
      }  /* if */
      /* Put out the name, including the class qualifier if any.  For
         class members and ambiguous symbols always use the original
         symbol.  Otherwise, use the fundamental symbol. */
      { a_boolean	use_orig_sym;
        /* If a template symbol is being displayed, use the normal
           form_symbol_name routine.  If a template symbol is not
           being displayed, use a special routine that displays the
           corresponding prototype template in place of the actual
           parent class. */
        use_orig_sym = sym->is_class_member || sym->ambiguous;
        if (corresp_template_sym == NULL) {
          sym_to_display = use_orig_sym ? sym : fund_sym;
          form_symbol_name_for_error(sym_to_display, seg_ptr);
        } else {
          sym_to_display = corresp_template_sym;
          form_symbol_name(sym_to_display, &octl);
        }  /* if */
      }
      /* Put out the second part of the type if needed.  Don't put it
         out in name-only mode.  Do put it out in full-type mode, or
         if function parameters should be listed. */
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
        if (routine != NULL && !return_type_needed) {
          /* For constructors, destructors, and conversion functions,
             put out the function type but not the return type. */
          form_function_declarator(type, &octl);
        } else {
          /* Normal case -- put out the complete second part of the type. */
          form_type_second_part_simple(type, /*under_lhs_declarator=*/FALSE,
                                       &octl);
        }  /* if */
      }  /* if */
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
    check_assertion(sym->kind == (a_symbol_kind)sk_function_template ||
                    sym->kind == (a_symbol_kind)sk_class_template);
    check_assertion(ssep != NULL);
    if (ssep->template_arg_list != NULL) {
      add_string_to_segment(" based on template argument", seg_ptr);
      if (ssep->template_arg_list->next != NULL) {
        add_string_to_segment("s", seg_ptr);
      }  /* if */
      add_string_to_segment(" ", seg_ptr);
      form_template_args(ssep->template_arg_list, &octl);
    }  /* if */
  }  /* if */
  /* Add the declaration position as requested. */
  if (seg_ptr->variant.symbol.decl_pos) {
    form_source_position(&sym->decl_position, error_pos, " (declared ", ")",
                         "(at end of source)", seg_ptr);
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
      add_string_to_segment(" (", seg_ptr);
      /* This message code includes the explanatory text (e.g.,
         "from translation unit"). */
      add_string_to_segment(error_text(ec_from_trans_unit), seg_ptr);
      add_string_to_segment("\"", seg_ptr);
      add_string_to_segment(tup->source_file->file_name,
                            seg_ptr);
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
  a_msg_segment_ptr  curr_segment;	/* Pointer to the current segment. */
  char               *end_ptr;
  int                i;
  
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
      end_ptr = strchr(msg_ptr+1, '%');
      if (end_ptr == NULL) {
        /* This part is the end of the message template. */
        curr_segment->length = strlen(msg_ptr);
      } else {
        /* A substitution parameter has been found.  The length is the
           difference of the two pointers. */
        curr_segment->length = end_ptr - msg_ptr;
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

  p = strchr(error_text(error_code), '%');
  /* Ignore "%%"; it's not a real fill-in. */
  while (p != NULL && p[1] == '%') p = strchr(p+2, '%');
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
  int                     index, mid_index;
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
  if ((index = curr_file->next_index_entry) < 
                                  NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES) {
    /* Add the file index information into the next available table entry. */
    curr_file->line_number[index] = physical_line;
    curr_file->file_position[index] = file_pos;
    curr_file->next_index_entry++;
  } else {
    /* The index table is full.  Reorganize the table by compressing the
       first half of the table to cover a wider range of lines.  The line
       number gaps will be some integer multiple of
       INITIAL_PHYSICAL_LINE_COUNT_INCREMENT. The value of
       curr_file->physical_line_count_increment is incremented by
       INITIAL_PHYSICAL_LINE_COUNT_INCREMENT each time the table is
       filled. */
    mid_index = NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES / 2;
    spacing = curr_file->line_number[mid_index] / (a_line_number)mid_index;
    /* Eliminate the first entry in the top half of the table that is less
       than the value should be at the desired interval. */
    for (index = 0; index < mid_index; index++ ) {
      if (curr_file->line_number[index] < ((index + 1) * spacing)) {
        /* Eliminate this entry simply by breaking the loop. */
        break;
      }  /* if */
    }  /* for */
    /* Now shift all remaining entries in the table. */
    for (/* start with the index to be eliminated */;
         index < NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES - 1;
         index++ ) {
      curr_file->line_number[index] = curr_file->line_number[index + 1];
      curr_file->file_position[index] = curr_file->file_position[index + 1];
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
    for (index = 0;
         index < NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES; ++index) {
      fprintf(f_debug, "entry %d=%5lu\n", index,
              curr_file->line_number[index]);
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
  int                     index;

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
  for (index = 0; index < curr_file->next_index_entry; index++) {
    if (curr_file->line_number[index] > physical_line )  break;
  }  /* for */
  if (index == 0) {
    /* Desired position is earlier than any known position; start at the
       beginning of the file. */
    *seek_position = 0L;
    *starting_line = 1;
  } else {
    /* Return the last encountered "good" file position. */
    *seek_position = curr_file->file_position[index - 1];
    *starting_line = curr_file->line_number[index - 1];
  }  /* if */
}  /* optimum_file_start_position */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

static a_boolean can_locate_source_line(a_seq_number seq_number)
/*
Determine the actual file which contains the specified sequence number.  If
possible read the desired source line into the buffer pointed to by
error_source_line for later use by diagnostic output functions.
*/
{
  a_source_file_ptr src_file;
  a_line_number     physical_line, starting_line, skip_lines;
  long              seek_position;
  a_boolean         at_end_of_source;
  a_boolean         src_line_found = FALSE;
  FILE              *f_err_src_file;
  int               ch;
  register char     *loc_in_line;
  char              *after_end_of_error_source_line_minus_2;

  conv_seq_to_physical_file_and_line(seq_number, &src_file, &physical_line,
                                     &at_end_of_source);
  if (physical_line == 0 ||
      at_end_of_source ||
      strcmp(src_file->full_name, FILE_NAME_FOR_STDIN) == 0 ||
      head_of_file_index_list == NULL) {
    /* Either the file position is strange or unknown, we are at the end of
       the primary source file, the input is from stdin, or there is no
       file index information (for example, because we are currently in the
       back end).  The original source line cannot be recovered. */
    goto return_point;
  } else {
    /* Determine the optimum starting position in the file to read the desired
       source line. */
    optimum_file_start_position(src_file, physical_line, &seek_position,
                                &starting_line);
    /* Attempt to read the desired source line.  The source file should be
       readable unless it was deleted recently.  Fail softly if any problems
       arise. */
    if ((f_err_src_file = reopen_source_file(src_file->full_name)) != NULL) {
      if (seek_position != 0) {
        if (fseek(f_err_src_file, seek_position, SEEK_SET) != 0) {
          /* The seek failed; fail softly and assume the source line is
             not readable. */
          goto close_file;
        }  /* if */
      }  /* if */
      /* Skip over lines in the file to the position of the desired line. */
      for (skip_lines = physical_line - starting_line;
           skip_lines > 0;
           skip_lines--) {
        while ((ch = getc(f_err_src_file)) != '\n') {
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
        error_source_line = alloc_general(
                                  ERROR_SOURCE_LINE_INITIAL_ALLOCATION + 1);
        after_end_of_error_source_line = error_source_line +
                                  ERROR_SOURCE_LINE_INITIAL_ALLOCATION;
      }  /* if */
      loc_in_line = error_source_line;
      after_end_of_error_source_line_minus_2 = after_end_of_error_source_line -
                                               2;
      while ((ch = getc(f_err_src_file)) != '\n' &&
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
          new_error_source_line = realloc_general(error_source_line,
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
    }  /* if */
  }  /* if */
    
return_point:
  return src_line_found;
}  /* can_locate_source_line */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

/*
Size of the local buffer used to buffer characters going to stderr.
Should be comparable in size to a source line.  The actual array is
allocated with an extra element to be used to store a terminating
character.  This is used to avoid a purify/clcc bug that causes
purify to issue a spurious error.
*/
#define MAX_PUTCBUFFER_CHARS 100
#define PUTCBUFFER_ARRAY_SIZE (MAX_PUTCBUFFER_CHARS + 1)

static void flush_putcbuffer(char *putcbuffer,
                             int  *num_putcbuffer_chars)
/*
Flush characters out of the local buffer putcbuffer to stderr.
*num_putcbuffer_chars indicates how many characters there are in the buffer;
it is reset to 0.
*/
{
  /* The following assignment should not be necessary but is present
     to avoid Purify errors from versions of fprintf that look one
     character beyond the specified precision specification.
     Specifically, the clcc runtime does this. */
  putcbuffer[*num_putcbuffer_chars] = '\0';
  if (*num_putcbuffer_chars > 0) {
     fprintf(stderr, "%.*s", *num_putcbuffer_chars, putcbuffer);
     *num_putcbuffer_chars = 0;
  }  /* if */
}  /* flush_putcbuffer */


static void add_to_putcbuffer(char *putcbuffer,
                              int  *num_putcbuffer_chars,
                              char out_char)
/*
Add out_char to the local buffer putcbuffer.  Increment the number of
characters in the buffer, *num_putcbuffer_chars.  Flush the buffer
to stderr if it is full.
*/
{
  /* Flush the buffer if it is full. */
  if (*num_putcbuffer_chars == MAX_PUTCBUFFER_CHARS) {
    flush_putcbuffer(putcbuffer, num_putcbuffer_chars);
  }  /* if */
  /* Add the character to the buffer. */
  putcbuffer[*num_putcbuffer_chars] = out_char;
  (*num_putcbuffer_chars)++;
}  /* add_to_putcbuffer */


/*
Put out_char into a local buffer for later writing to stderr.  This is
done to avoid lots of costly system calls in the usual case that stderr
is unbuffered.
*/
#define putcb(out_char)                                               \
  add_to_putcbuffer(putcbuffer, &num_putcbuffer_chars, (out_char));

/*
Macro to write source line characters in the first pass, and spaces over
and the caret on the second pass.  Exits to "end_of_loop" upon finding the
column for the caret in the second pass.
*/
#define put_char(out_char)                                            \
{ if (pass_for_caret && curr_column >= source_pos->column) {          \
    goto end_of_loop;                                                 \
  } else {                                                            \
    if (/*lint --e(506)*/ !pass_for_caret || (out_char) == '\t') {    \
      putcb(out_char);                                                \
    } else {                                                          \
      putcb(' ');                                                     \
    }  /* if */                                                       \
    curr_column++;                                                    \
  }  /* if */                                                         \
}  /* put_char */


static void write_orig_source_line(a_source_position *source_pos)
/*
Write out the source line associated with the source position source_pos,
and place a caret under the proper column.  The position must be within
the current logical source line.  If the column position is zero, write
a blank line instead of the caret line.
*/
{
  a_seq_number            seq;
  an_orig_line_modif_ptr  line_olmp, olmp, olmp_next;
  char                    *line_start, *loc_in_line;
  a_column_number         curr_column;
  a_boolean               pass_for_caret;
  char                    ch;
  a_source_line_modif_ptr slmp;
  int                     i;
  char                    putcbuffer[PUTCBUFFER_ARRAY_SIZE];
  int                     num_putcbuffer_chars = 0;

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
    putcb(' ');
    putcb(' ');
    /* Perform any additional indentation needed (based on the category
       kind) */
    for (i = 0; i < diagnostic_indent; i++) {
      putcb(' ');
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
          /* This character of the source line may have been replaced by
             an attention character to indicate that some sort of source
             line modification starts here.  If so, go to the modification
             entry and get the original source line character. */
          if (ch == ATTENTION_MARKER) {
            slmp = nested_source_line_modif(loc_in_line);
            ch = slmp->orig_char;
          }  /* if */
          if (ch == LE_ESCAPE) {
            /* LE_NULL is handled below (it has an associated modification
               entry). */
            /* Exit on the newline at the end of the source line.  (If there
               wasn't one there originally, one has been added.) */
            check_assertion_str(loc_in_line[1] == LE_NEWLINE,
                                "write_orig_source_line: bad lexical escape");
            goto end_of_loop;
          }  /* if */
          put_char(ch);
          loc_in_line++;
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
      if (pass_for_caret) putcb('^');
    }  /* if */
    /* For both passes, end the output line. */
    putcb('\n');
    flush_putcbuffer(putcbuffer, &num_putcbuffer_chars);
    /* After the first pass (writing the source), go on to the second pass
       (writing the caret). */
  }  /* for */
}  /* write_orig_source_line */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

static void write_error_source_line(a_source_position *source_pos)
/*
Write out the source line associated with the source position source_pos,
and place a caret under the proper column.  The position has been determined
earlier to be in other than the current logical source line and the line
has been reread into the buffer pointed to by the static variable
error_source_line.  If the column position is zero, write a blank line
instead of the caret line.
*/
{
  char            *loc_in_line;
  char            ch;
  a_boolean       pass_for_caret;
  a_column_number curr_column;
  int             i;
  char            putcbuffer[PUTCBUFFER_ARRAY_SIZE];
  int             num_putcbuffer_chars = 0;

  /* Take two passes -- the first to write the source line, the second to
     write the caret.  Because of the presence of tabs in the source line,
     etc. it is hard to figure out where to place the caret without
     running through the characters again. */
  for (pass_for_caret = 0; pass_for_caret <= 1; pass_for_caret++) {
    /* Indent both the source line and the caret line.  This is done so
       that programs (like emacs) that read the error output will ignore
       these lines. */
    putcb(' ');
    putcb(' ');
    /* Perform any additional indentation needed (based on the category
       kind). */
    for (i = 0; i < diagnostic_indent; i++) {
      putcb(' ');
    }  /* for */
    /* On the caret pass, if the column number is zero (unknown), skip
       writing the spaces and caret and go right to the newline. */
    if (!pass_for_caret || source_pos->column != SP_COL_UNKNOWN) {
      loc_in_line = error_source_line;
      curr_column = 1;
      /* Process each individual character until the newline is found. */
      for (;;) {
        /* Exit on the newline at the end of the source line. */
        if ((ch = *loc_in_line++) == '\n') goto end_of_loop;
        put_char(ch);
      }  /* for */

end_of_loop:
      /* For the pass that writes the caret, write the caret at this point. */
      if (pass_for_caret) putcb('^');
    }  /* if */
    /* For both passes, end the output line. */
    putcb('\n');
    flush_putcbuffer(putcbuffer, &num_putcbuffer_chars);
    /* After the first pass (writing the source), go on to the second pass
       (writing the caret). */
  }  /* for */
}  /* write_error_source_line */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void write_message_part(char      *msg,
                               int       len,
                               FILE      *file,
                               int       *line_len,
                               a_boolean wrap,
                               a_boolean quoted_text,
                               a_boolean start_of_diagnostic)
/*
Write out a piece of an error message.  msg points to the
message (or is NULL if there is no message), and len is its length (or
-1 if the text is null-terminated).  The message is written to the file
indicated by file.  *line_len is incremented by the number of characters
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
    if (len < 0) len = strlen(msg);
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
          fputc(' ', file);
          (*line_len)++;
        }  /* for */
        *line_len += fprintf(file, "%.*s", chars_to_take, msg);
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
      (void)fputc('\n', file);
      for (*line_len = 0;
           *line_len < (INDENT_AMOUNT + diagnostic_indent);
           (*line_len)++) {
        (void)fputc(' ', file);
      }  /* for */
    }  /* while */
    /* Print the final piece of the text (in the usual case, this prints
       all of the text). */
    /* Print any "remembered" spaces from the last fragment. */
    for (; trailing_space_count > 0; trailing_space_count--) {
      fputc(' ', file);
      (*line_len)++;
    }  /* for */

    if (wrap) {
      /* Remove any trailing spaces from the final piece of text.  These
         will be added prior to the next piece of text if needed. */
      for (; len > 0 && msg[len - 1] == ' '; len--, trailing_space_count++) {}
    }  /* if */
    if (len > 0) {
      *line_len += fprintf(file, "%.*s", len, msg);
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


static void write_message(FILE      *file,
                          int       *line_len,
                          a_boolean wrap)
/*
Write each of the message segments chained from the static variable
error_message_head to the specified file.  *line_len is the current line
length and is incremented to reflect the number of characters added
to the current line.  If wrap is TRUE, the text will be wrapped to
successive additional lines as necessary.
*/
{
  a_msg_segment_ptr curr_seg;
  int               length;
  int               total_len;
  a_boolean         start_of_message = TRUE;

  for (curr_seg = error_message_head;
       curr_seg != NULL &&
         curr_seg->kind != (a_message_segment_kind)msk_last;
       curr_seg = curr_seg->next) {
    switch (curr_seg->kind) {
      case msk_error_text_part:
        write_message_part(curr_seg->variant.msg_part, curr_seg->length,
                           file, line_len, wrap, /*quoted_text=*/FALSE,
                           start_of_message);
        break;
      case msk_user_string:
        if (curr_seg->variant.string.quoted) {
          goto handle_embedded_quoted_text;
        }  /* if */
        write_message_part(error_msg_strings[curr_seg->sequence_no], -1, file,
                           line_len, wrap, /*quoted_text=*/FALSE,
                           start_of_message);
        break;
      case msk_source_position:
      case msk_type:
      case msk_symbol:
handle_embedded_quoted_text:
        if (curr_seg->first_quote == NULL) {
          write_message_part(curr_seg->segment, -1, file, line_len,
                             wrap, /*quoted_text=*/FALSE,
                             start_of_message);
        } else {
          /* This segment contains double quoted text which should not be
             broken across lines. */
          total_len = 0;
          if (curr_seg->segment != curr_seg->first_quote) {
            total_len = (curr_seg->first_quote - curr_seg->segment);
            write_message_part(curr_seg->segment, total_len, file,
                               line_len, wrap, /*quoted_text=*/FALSE,
                               start_of_message);
            start_of_message = FALSE;
          }  /* if */
          /* Output the quoted text as a single unit. */
          total_len += length = curr_seg->second_quote -
                                curr_seg->first_quote +1;
          write_message_part(curr_seg->first_quote, length, file, line_len,
                             wrap, /*quoted_text=*/TRUE,
                             start_of_message);
          start_of_message = FALSE;
          /* Check for any fragment following the quoted text. */
          if ((length = curr_seg->length - total_len) > 0) {
            write_message_part(curr_seg->second_quote + 1, length, file,
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
  putc('\n', file);
}  /* write_message */


static void write_position_and_severity(an_error_code     error_code,
                                        an_error_severity severity,
                                        a_source_position *error_pos,
                                        char              **file_name,
                                        a_line_number     *line_number,
                                        a_boolean         *src_text_needed,
                                        a_boolean         *in_curr_src_line,
                                        int               *line_len)
/*
Write the source position (file name and line number) and severity to
stderr.  Determine if the actual source line is available, either in the 
current source line or able to be reread from one of the source files.   If
the actual source line is not available, the column number is added into
the output.
*/
{
  char          *severity_string, *full_name;
  a_boolean	at_end_of_source;
  a_boolean     capitalize_severity;
  a_boolean     column_needed;
  a_boolean	local_display_error_number;

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
      *line_len += fprintf(stderr, "At end of source: ");
    } else {
      /* Normal line in file, not end of file. */
#if STANDALONE_UTILITY_PROGRAM
      /* In program-form C-generating back end, source lines are 
         never displayed. */
      column_needed = FALSE;
#else /* !STANDALONE_UTILITY_PROGRAM */
      column_needed = FALSE;
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
        if (can_locate_source_line(error_pos->seq)) {
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
      /* If the line is from stdin, do not display the file name. */
      if (strcmp(*file_name, FILE_NAME_FOR_STDIN) == 0) {
        *line_len += fprintf(stderr, "Line %lu", *line_number);
      } else {
        *line_len += fprintf(stderr, "\"");
        /* Don't convert '\' to '\\' in error message output.  The
           name should be displayed as written by the user.  This also
           prevents doubling of directory separators on Windows. */
        *line_len += write_file_name(*file_name, stderr,
                                     /*process_escapes=*/FALSE);
        *line_len += fprintf(stderr, "\", line %lu", *line_number);
      }  /* if */
      if (column_needed) {
        *line_len += fprintf(stderr, " (col. %d)", error_pos->column);
      }  /* if */
      *line_len += fprintf(stderr, ": ");
    }  /* if */
  }  /* if */
  /* Determine the appropriate severity string, and also count this
     diagnostic against the total for the severity. */
  switch (severity) {
    case es_remark:
      severity_string = "remark";
      total_remarks++;
      break;
    case es_warning:
      severity_string = "warning";
      total_warnings++;
      break;
    case es_discretionary_error:
    case es_error:
      if (local_display_error_number ||
          ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES) { /*lint !e506 !e774*/
        severity_string = "error";
      } else {
        severity_string = "";
      }  /* if */
      total_errors++;
      break;
    case es_catastrophe:
      severity_string = "catastrophic error";
      total_catastrophes++;
      break;
    case es_command_line_error:
      severity_string = "command-line error";
      total_catastrophes++;
      break;
    case es_internal_error:
      severity_string = "internal error";
      total_catastrophes++;
      break;
#if CHECKING
    case es_none:
    default:
      internal_error("write_position_and_severity: bad severity");
#endif /* CHECKING */
  }  /* switch */
  if (capitalize_severity && *severity_string != '\0') {
    /* Capitalize the first letter of the severity, because it's the first
       thing on the line. */
    *line_len += fprintf(stderr, "%c%s", toupper(*severity_string),
                                          severity_string+1);
  } else {
    *line_len += fprintf(stderr, "%s", severity_string);
  }  /* if */
  /* The error number may optionally be displayed based on a command
     line option. */
  if (local_display_error_number) {
    /* Display the error message number.  Append a -D suffix if the
       severity may be changed. */
    a_boolean	is_discretionary;
    is_discretionary = ((int)severity <= (int)es_discretionary_error);
    *line_len += fprintf(stderr, " #%d%s: ", (int)error_code,
                         is_discretionary ? "-D" : "");
  } else {
    *line_len += fprintf(stderr, ": ");
  }  /* if */
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
                        file_name, line_number, error_pos->column);
  }  /* if */
  /* For an internal error, the coded-form message indicates only that the
     error is catastrophic, so we add text to indicate that it is an
     internal error. */
  if (severity == es_internal_error) {
    fputs("(internal error) ", f_raw_listing);
  }  /* if */
  /* Put out the error message text. */
  line_len = 0;  /* Meaningless. */
  write_message(f_raw_listing, &line_len, /*wrap=*/FALSE);
}  /* write_diag_to_raw_listing */

#endif /* !STANDALONE_UTILITY_PROGRAM */

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
*/
{
		
  static char              *file_name;
  static a_line_number     line_number;
  static a_boolean         source_text_needed;
  static a_boolean         in_current_source_line;
  int                      line_len;

  if ((int)severity < (int)error_threshold) {
    /* Ignore the message if its severity is below the threshold. */
  } else {
    if (diag_kind == (a_diagnostic_category_kind)dck_list) {
      diagnostic_indent = LIST_DIAG_INDENT;
    } else if (diag_kind == (a_diagnostic_category_kind)dck_context_primary) {
      diagnostic_indent = INDENT_AMOUNT;
    } else {
      diagnostic_indent = NORMAL_DIAG_INDENT;
    }  /* if */
  
    if (diag_kind != dck_end_list && diag_kind != dck_end_context) {
      /* Perform any indentation needed (based on the category kind) */
      for (line_len = 0; line_len < diagnostic_indent; line_len++) {
        putc(' ', stderr);
      }  /* for */
    }  /* if */

    if (diag_kind == dck_standalone || diag_kind == dck_primary) {
      /* Collect and output error position and severity information. */
      write_position_and_severity(error_code, severity, error_pos, &file_name,
                                  &line_number,
                                  &source_text_needed,
                                  &in_current_source_line,
                                  &line_len);
    }  /* if */

    if (diag_kind != dck_end_list && diag_kind != dck_end_context) {
      /* There is a message to be formatted and written. */
      /* Put out the error message text to stderr. */
      write_message(stderr, &line_len, /*wrap=*/!brief_diagnostics && 
                                                !do_not_wrap_diagnostics);

#if !STANDALONE_UTILITY_PROGRAM
      /* The message is always output to stderr so that the user can see it.
         If raw-listing information has been requested, it is also output to
         the raw-listing file in coded form, for later incorporation into the
         listing.  */
      if (f_raw_listing != NULL) {
        write_diag_to_raw_listing(severity, file_name, line_number,
                                  error_pos, diag_kind);
      }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    }  /* if */

    if (diag_kind == dck_standalone || diag_kind == dck_end_list) {
#if !STANDALONE_UTILITY_PROGRAM
      if (source_text_needed && !brief_diagnostics &&
          (diag_kind == dck_standalone || diag_kind == dck_end_list)) {
        /* Write the source text line, with a caret pointing to the location
           of the error. */
        if (in_current_source_line) {
          /* Write the source line text from the curr_source_line buffer. */
          write_orig_source_line(error_pos);
        }  else {
          /* Write the source line text from the error_source_line buffer. */
          write_error_source_line(error_pos);
        }  /* if */
      }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    }  /* if */
    if ((diag_kind == dck_standalone || diag_kind == dck_end_list ||
         diag_kind == dck_end_context) &&
	!context_required && !brief_diagnostics) {
      /* Put out an extra space line after the error, for clarity.  The
         space is suppressed if a context message is to follow since the
         space should follow the context. */
      putc('\n', stderr);
    }  /* if */
  }  /* if */

  if ((diag_kind == dck_standalone || diag_kind == dck_end_list ||
       diag_kind == dck_end_context) && !context_required ) {
    /* Terminate the compilation for the more serious severities. */
    if (severity == es_catastrophe || severity == es_command_line_error ||
        severity == es_internal_error) {
      /* Force out the last line of the raw listing file. */
#if !STANDALONE_UTILITY_PROGRAM
      finish_raw_listing_file();
#endif /* !STANDALONE_UTILITY_PROGRAM */
      term_compilation(severity);
    }  /* if */
#if IL_SHOULD_BE_WRITTEN_TO_FILE && !STANDALONE_UTILITY_PROGRAM
    /* If there are any errors, suppress generation of the intermediate
       language file. */
    if (total_errors + total_catastrophes > 0) cancel_il_file();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && !STANDALONE_UTILITY_PROGRAM */
    /* Terminate the compilation if the error limit has been reached.  Note
       that remarks and warnings are never counted. */
    if (total_errors + total_catastrophes >= error_limit) {
#if !USING_DRIVER
      fprintf(stderr, "Error limit reached.\n");
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
DOES_NOT_RETURN internal_error(char *error_message)
/*
An internal error has occurred.  Write the given message and abort.
*/
{
  /* This variable does not have to be reset by fe_init. */
  static a_boolean internal_error_loop = FALSE;

  /* Make sure that if one internal error leads to another, we abort
     the compilation instead of looping. */
  if (internal_error_loop) {
    fprintf(stderr, "Internal error loop: %s\n", error_message);
    term_compilation(es_internal_error);
  }  /* if */
  internal_error_loop = TRUE;
  init_error_params();
  error_msg_strings[1] = error_message;
  construct_message_segments("%s");
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
  char	buffer[BUFFER_SIZE];
  int   max_filename_length = BUFFER_SIZE - 100;
  int	overflow;

  /* Make sure that formatting the internal error string won't overflow
     the buffer.  We subtract 100 from the buffer length to allow for
     other information that is included in the message.  If the filename
     is too long we print as many characters from the end of the string
     as possible because the characters at the beginning probably contain
     the directory portion of the name. */
  overflow = strlen(filename) - max_filename_length;
  if (overflow > 0) {
    filename += overflow;
  }  /* if */
  
  if (string1 == NULL) {
    sprintf(buffer, "assertion failed at: \"%s\", line %d\n",
            filename, line_number);
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
    sprintf(buffer, "assertion failed: %s%s%s (%s, line %d)\n", string1,
            separator, string2, filename, line_number);
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
} expected_error_record = { NULL, 0, NULL, NULL };

  
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
  error_position.seq = 0;
  error_position.column = SP_COL_CMD_LINE;
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
    new_severity = severity_for_error_code[(int)error_code];
    if (new_severity != es_default) *severity = new_severity;
  }  /* if */
}  /* check_for_overridden_severity */


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
  static an_error_severity  saved_severity = (an_error_severity)es_default;
  static an_error_severity  saved_error_threshold;
  an_error_severity	    error_threshold_to_use;

#if CHECKING
  /* The saved severity level should be es_default if and only if this is a
     diagnostic without extra message lines or if it is the first message
     with such extra lines. */
  if ((saved_severity == (an_error_severity)es_default) !=
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
    /* Check whether we are inside a "system" include file in which
       warnings should be suppressed. */
    error_threshold_to_use = error_threshold;
    { a_source_file_ptr	sfp;
      a_boolean		at_end_of_source;
      a_line_number	line_number;
      unsigned long	nesting_depth;
      sfp = source_file_for_seq((*error_pos)->seq, &line_number,
                                &at_end_of_source, &nesting_depth,
                               /*physical_line=*/FALSE);
      if (sfp != NULL && sfp->from_system_include_dir) {
        error_threshold_to_use = es_discretionary_error;
      }  /* if */
    }
    if (diag_kind != (a_diagnostic_category_kind)dck_standalone) {
      /* The principal message of a multiple message diagnostic.  Save the
         arguments for later calls. */
      copy_source_position(**error_pos, saved_error_position);
      saved_severity = *severity;
      saved_error_threshold = error_threshold_to_use;
    }  /* if */
  } else if (diag_kind == (a_diagnostic_category_kind)dck_list ||
             diag_kind == (a_diagnostic_category_kind)dck_end_list ||
             diag_kind == (a_diagnostic_category_kind)dck_end_context) {
    /* Reuse the error position and severity from the primary diagnostic. */
    *error_pos = &saved_error_position;
    *severity = saved_severity;
    error_threshold_to_use = saved_error_threshold;
#if CHECKING
    if (diag_kind == (a_diagnostic_category_kind)dck_end_list ||
        diag_kind == (a_diagnostic_category_kind)dck_end_context) {
      saved_severity = (an_error_severity)es_default;
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  /* Return FALSE if the current severity is below the threshold. */
  return ((int)*severity >= (int)error_threshold_to_use);
}  /* check_severity */


#if !STANDALONE_UTILITY_PROGRAM
static a_boolean include_in_context_output
			(a_scope_stack_entry_ptr ssep,
			 a_symbol_ptr	         *context_sym,
			 an_error_code		 *context_error_code,
			 a_boolean               add_detected_prefix)
/*
Return TRUE if this scope stack entry has context information that should
be processed, otherwise return FALSE.  When TRUE is returned *context_sym
is set to point to a symbol that provides the context information and
*context_error_code is set to the appropriate error code.  When
add_detected_prefix is TRUE, the error code returned will refer to
a message that includes the text (e.g., "detected during ") that is
used when only a single line of context information is being supplied.
When multiple context lines are being displayed, the "detected during"
message appears by itself on a separate line.
*/
{
  a_boolean	result = FALSE;
  a_symbol_ptr	sym;
  an_error_code error_code;

  if (ssep->exclude_from_context_output) {
    /* Don't include this scope in the context output. */
  } else if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
    /* Template instantiations (except for prototype instantiations)
       need additional context information. */
    sym = ssep->instance_sym;
    /* If the instance symbol is NULL use the template symbol instead. */
    if (ssep->in_prototype_instantiation) {
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
    } else if (sym->kind == (a_symbol_kind)sk_static_data_member) {
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
suppress duplicate diagnostics.
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
      found = TRUE;
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
    } else if (is_template_dependent_context()) {
      record_prototype_diagnostic(error_code, severity, error_pos);
    } else if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
      if (find_prototype_diagnostic(error_code, severity, error_pos)) {
        suppress_diagnostic = TRUE;
      }  /* if */
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


/* Forward declaration. */
static void diag_message(an_error_code              error_code,
                         a_source_position          *error_pos,
                         an_error_severity          severity,
                         a_diagnostic_category_kind diag_kind);


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
  error_msg_strings[1] = diag_primary_source_file->file_name;
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
es_warning)and diag_kind indicates if this is a single diagnostic message or
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
        fprintf(stderr, "Loop in catastrophic error processing.\n");
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
    };
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
          a_scope_stack_entry_ptr ssep = &scope_stack[sd];
          if (!include_in_context_output(ssep, &sym,
                                         &context_error_code,
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
	  error_msg_positions[1] = &ssep->source_position;
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
				     an_error_severity	severity)
/*
Given an error tag string, this routine looks up the error tag and updates
the table used to override the error severity of diagnostic messages.
If the tag cannot be found return TRUE, otherwise return FALSE.
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
    severity_for_error_code[(int)error_code] = severity;
  }  /* if */
  /* Return TRUE if the tag could not be found. */
  return etep_found == NULL;
}  /* set_severity_for_error_tag */


a_boolean set_severity_for_error_number(int		   error_number,
				        an_error_severity  severity)
/*
Given an error number, this routine updates the table used to override the
error severity of diagnostic messages. If the error number is out of range
return TRUE, otherwise return FALSE.
*/
{
  an_error_code			error_code;
  a_boolean			err;


  err = (error_number <= (int)ec_no_error || error_number >= (int)ec_last);
  if (!err) {
    error_code = (an_error_code)error_number;
    severity_for_error_code[(int)error_code] = severity;
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

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED

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

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */

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
#if GNU_EXTENSIONS_ALLOWED

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

#endif /* GNU_EXTENSIONS_ALLOWED */

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


void pos_stty_error(an_error_code     error_code,
                    a_source_position *error_pos,
                    char              *error_string,
                    a_type_ptr        type)
/*
Report the indicated error (with the indicated fill-in string) at the
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
    fprintf(stderr, text, primary_source_file_name, fill_in_str);
    fprintf(stderr, "\n");
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
  error_position.seq = 0;
  error_position.column = SP_COL_CMD_LINE;
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

#if !STANDALONE_UTILITY_PROGRAM

void error_one_time_init(void)
/*
Do one-time initialization of variables related to the error routines.
(Variables that need to be reinitialized with each new translation unit
are handled in error_init.)
*/
{
  /* Save variables from error.h and error.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(head_of_file_index_list),
      pch_saved_var_array_elem(tail_of_file_index_list),
      pch_saved_var_array_elem(error_position),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
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
  catastrophe_has_occurred = FALSE;
  clear_file_index_list();
  memzero((char *)recorded_diagnostic_table,
          sizeof(recorded_diagnostic_table));
}  /* error_init */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
