/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

error.c -- Error reporting routines.

*/

#include "basics.h"
#include "target.h"
#include "error.h"
#include "host_envir.h"
#include "cmd_line.h"
#include "mem_manage.h"
#include "float_pt.h"
#include "const_ints.h"
#include "il.h"
#if !STANDALONE_UTILITY_PROGRAM
#include "symbol_tbl.h"
#include "lexical.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if ASM_FUNCTION_ALLOWED
#include "asm_func.h"
#endif /* ASM_FUNCTION_ALLOWED */

#else /* STANDALONE_UTILITY_PROGRAM */

/* Many support functions and macros that are generally available in the
   front end are duplicated here so that error.c can be compiled
   independently of a front end (e.g. with a standalone IL display
   utility). */

/* Macro to strip tk_typeref entries from a type. */
#define skip_typerefs(tp)                                             \
  ((tp)->kind != (a_type_kind)tk_typeref ? (tp) : local_skip_typerefs(tp))

static a_type_ptr local_skip_typerefs(a_type_ptr type_ptr)
/*
Strip any typeref entries off the given type to get to the real type, and
return a pointer to that.  Note that the typeref may have some type
qualifiers (const, volatile), and they will be dropped here.  Therefore,
this routine should not be used when checking type qualifiers.  Note
that ordinarily this routine should not be called directly; use the macro
"skip_typerefs".
*/
{
  while (type_ptr->kind == (a_type_kind)tk_typeref) {
    type_ptr = type_ptr->variant.typeref.type;
#if CHECKING
    if (type_ptr == NULL) {
      internal_error("local_skip_typerefs: NULL referenced type");
    }  /* if */
#endif /* CHECKING */
  }  /* while */
  return(type_ptr);
}  /* local_skip_typerefs */
#endif /* !STANDALONE_UTILITY_PROGRAM */

#define is_pointer_or_reference_type(tp) \
	(skip_typerefs(tp)->kind == (a_type_kind)tk_pointer)

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
				   
/*
Diagnostics messages being generated can be one of several category of
messages.
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
		decl_pos;	/* True if the declaration position is
				   to be generated. */
      a_byte_boolean
		template_args;	/* True if the template arguments are to be
				   displayed; the pointer to the scope is in
				   error_msg_scopes[]; for sk_function_template
				   only. */
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
static a_source_position_ptr
		error_msg_positions[MAX_ERR_SEG_KIND_PER_MSG + 1];
				/* Array of source positions to be used
				   for insertion into diagnostic messages. */
#if !STANDALONE_UTILITY_PROGRAM
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
#define INITIAL_PHYSICAL_LINE_COUNT_INCREMENT 100;
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

static void form_param_list(a_routine_type_supplement_ptr   suppl_ptr,
                            a_msg_segment_ptr               seg_ptr);


static char *error_text(an_error_code error_code)
/*
Return a pointer to the error text for the message identified by the given
error code.
*/
{  char *m;

  switch (error_code) {
    case ec_last_line_incomplete:
      m = "last line of file ends without a newline";
      break;
    case ec_last_line_backslash:
      m = "last line of file ends with a backslash";
      break;
    case ec_include_recursion:
      m = "#include file %sq includes itself";
      break;
    case ec_out_of_memory:
      m = "out of memory";
      break;
    case ec_source_file_could_not_be_opened:
      m = "could not open source file %sq";
      break;
    case ec_comment_unclosed_at_eof:
      m = "comment unclosed at end of file";
      break;
    case ec_bad_token:
      m = "unrecognized token";
      break;
    case ec_unclosed_string:
      m = "missing closing quote";
      break;
    case ec_nested_comment:
      m = "nested comment is not allowed";
      break;
    case ec_bad_use_of_sharp:
      m = "\"#\" not expected here";
      break;
    case ec_bad_pp_directive_keyword:
      m = "unrecognized preprocessing directive";
      break;
    case ec_end_of_flush:
      m = "parsing restarts here after previous syntax error";
      break;
    case ec_exp_file_name:
      m = "expected a file name";
      break;
    case ec_extra_text_in_pp_directive:
      m = "extra text after expected end of preprocessing directive";
      break;
    case ec_source_file_has_bad_format:
      m = "%sq is not a file containing source text";
      break;
    case ec_illegal_source_file_name:
      m = "%sq is not a valid source file name";
      break;
    case ec_exp_rbracket:
      m = "expected a \"]\"";
      break;
    case ec_exp_rparen:
      m = "expected a \")\"";
      break;
    case ec_extra_chars_on_number:
      m = "extra text after expected end of number";
      break;
    case ec_undefined_identifier:
      m = "identifier %sq is undefined";
      break;
    case ec_useless_type_qualifiers:
      m = "type qualifiers are meaningless in this declaration";
      break;
    case ec_bad_hex_digit:
      m = "invalid hexadecimal number";
      break;
    case ec_integer_too_large:
      m = "integer constant is too large";
      break;
    case ec_bad_octal_digit:
      m = "invalid octal digit";
      break;
    case ec_zero_length_string:
      m = "quoted string should contain at least one character";
      break;
    case ec_too_many_characters:
      m = "too many characters in character constant";
      break;
    case ec_bad_character_value:
      m = "character value is out of range";
      break;
    case ec_expr_not_constant:
      m = "expression must have a constant value";
      break;
    case ec_exp_primary_expr:
      m = "expected an expression";
      break;
    case ec_bad_float_value:
      m = "floating constant is out of range";
      break;
    case ec_expr_not_integral:
      m = "expression must have integral type";
      break;
    case ec_expr_not_arithmetic:
      m = "expression must have arithmetic type";
      break;
    case ec_exp_line_number:
      m = "expected a line number";
      break;
    case ec_bad_line_number:
      m = "invalid line number";
      break;
    case ec_error_directive:
      m = "#error directive: %s";
      break;
    case ec_missing_pp_if:
      m = "the #if for this directive is missing";
      break;
    case ec_missing_endif:
      m = "the #endif for this directive is missing";
      break;
    case ec_pp_else_already_appeared:
      m = "directive is not allowed -- an #else has already appeared";
      break;
    case ec_divide_by_zero:
      m = "division by zero";
      break;
    case ec_exp_identifier:
      m = "expected an identifier";
      break;
    case ec_expr_not_scalar:
      m = "expression must have arithmetic or pointer type";
      break;
    case ec_incompatible_operands:
      m = "operand types are incompatible (%t1 and %t2)";
      break;
    case ec_expr_not_integral_or_pointer:
      m = "expression must have integral or pointer type";
      break;
    case ec_expr_not_pointer:
      m = "expression must have pointer type";
      break;
    case ec_cannot_undef_predef_macro:
      m = "#undef may not be used on this predefined name";
      break;
    case ec_cannot_redef_predef_macro:
      m = "this predefined name may not be redefined";
      break;
    case ec_bad_macro_redef:
      m = "macro redefined differently";
      break;
    case ec_mixed_function_object_pointers:
      m = "cast between pointer-to-object and pointer-to-function";
      break;
    case ec_duplicate_macro_param_name:
      m = "duplicate macro parameter name";
      break;
    case ec_paste_cannot_be_first:
      m = "\"##\" may not be first in a macro definition";
      break;
    case ec_paste_cannot_be_last:
      m = "\"##\" may not be last in a macro definition";
      break;
    case ec_exp_macro_param:
      m = "expected a macro parameter name";
      break;
    case ec_exp_colon:
      m = "expected a \":\"";
      break;
    case ec_too_few_macro_args:
      m = "too few arguments in macro invocation";
      break;
    case ec_too_many_macro_args:
      m = "too many arguments in macro invocation";
      break;
    case ec_sizeof_function:
      m = "operand of sizeof may not be a function";
      break;
    case ec_bad_constant_operator:
      m = "this operator is not allowed in a constant expression";
      break;
    case ec_bad_pp_operator:
      m = "this operator is not allowed in a preprocessing expression";
      break;
    case ec_bad_constant_function_call:
      m = "function call is not allowed in a constant expression";
      break;
    case ec_bad_integral_operator:
      m = "this operator is not allowed in an integral constant expression";
      break;
    case ec_integer_overflow:
      m = "integer operation result is out of range";
      break;
    case ec_negative_shift_count:
      m = "shift count is negative";
      break;
    case ec_shift_count_too_large:
      m = "shift count is too large";
      break;
    case ec_useless_decl:
      m = "declaration does not declare anything";
      break;
    case ec_exp_semicolon:
      m = "expected a \";\"";
      break;
    case ec_enum_value_out_of_int_range:
      m = "enumeration value is out of \"int\" range";
      break;
    case ec_exp_rbrace:
      m = "expected a \"}\"";
      break;
    case ec_integer_sign_change:
      m = "integer conversion resulted in a change of sign";
      break;
    case ec_integer_truncated:
      m = "integer conversion resulted in truncation";
      break;
    case ec_incomplete_type_not_allowed:
      m = "incomplete type is not allowed";
      break;
    case ec_sizeof_bit_field:
      m = "operand of sizeof may not be a bit field";
      break;
    case ec_address_of_constant:
      m = "operand of \"&\" may not be a constant";
      break;
    case ec_init_constant_address_of_non_static:
      m = "operand of \"&\" in an initializer must be static";
      break;
    case ec_bad_address_of_operand:
      m = "invalid operand of \"&\"";
      break;
    case ec_bad_indirection_operand:
      m = "operand of \"*\" must be a pointer";
      break;
    case ec_empty_macro_argument:
      m = "argument to macro is empty";
      break;
    case ec_missing_decl_specifiers:
      m = "this declaration has no storage class or type specifier";
      break;
    case ec_initializer_in_param:
      m = "a parameter declaration may not have an initializer";
      break;
    case ec_exp_type_specifier:
      m = "expected a type specifier";
      break;
    case ec_storage_class_not_allowed:
      m = "a storage class may not be specified here";
      break;
    case ec_mult_storage_classes:
      m = "more than one storage class may not be specified";
      break;
    case ec_storage_class_not_first:
      m = "storage class is not first";
      break;
    case ec_dupl_type_qualifier:
      m = "type qualifier specified more than once";
      break;
    case ec_bad_combination_of_type_specifiers:
      m = "invalid combination of type specifiers";
      break;
    case ec_bad_param_storage_class:
      m = "invalid storage class for a parameter";
      break;
    case ec_bad_function_storage_class:
      m = "invalid storage class for a function";
      break;
    case ec_type_specifier_not_allowed:
      m = "a type specifier may not be used here";
      break;
    case ec_array_of_function:
      m = "array of functions is not allowed";
      break;
    case ec_array_of_void:
      m = "array of void is not allowed";
      break;
    case ec_function_returning_function:
      m = "function returning function is not allowed";
      break;
    case ec_function_returning_array:
      m = "function returning array is not allowed";
      break;
    case ec_param_id_list_needs_function_def:
      m =
        "identifier-list parameters may only be used in a function definition";
      break;
    case ec_function_type_must_come_from_declarator:
      m = "function type may not come from a typedef";
      break;
    case ec_array_size_must_be_positive:
      m = "the size of an array must be greater than zero";
      break;
    case ec_array_size_too_large:
      m = "array is too large";
      break;
    case ec_empty_translation_unit:
      m = "a translation unit must contain at least one declaration";
      break;
    case ec_bad_function_return_type:
      m = "a function may not return a value of this type";
      break;
    case ec_bad_array_element_type:
      m = "an array may not have elements of this type";
      break;
    case ec_decl_should_be_of_param:
      m = "a declaration here must declare a parameter";
      break;
    case ec_dupl_param_name:
      m = "duplicate parameter name";
      break;
    case ec_id_already_declared:
      m = "%sq has already been declared in the current scope";
      break;
    case ec_nonstd_forward_def_enum:
      m = "forward-defined enum type is nonstandard";
      break;
    case ec_class_too_large:
      m = "class is too large";
      break;
    case ec_struct_too_large:
      m = "struct or union is too large";
      break;
    case ec_bad_bit_field_size:
      m = "invalid size for bit field";
      break;
    case ec_bad_bit_field_type:
      m = "invalid type for a bit field";
      break;
    case ec_zero_length_bit_field_must_be_unnamed:
      m = "zero-length bit field must be unnamed";
      break;
    case ec_signed_one_bit_field:
      m = "signed bit field of length 1";
      break;
    case ec_expr_not_ptr_to_function:
      m = "expression must have (pointer-to-) function type";
      break;
    case ec_exp_definition_of_tag:
      m = "expected either a definition or a tag name";
      break;
    case ec_code_is_unreachable:
      m = "statement is unreachable";
      break;
    case ec_exp_while:
      m = "expected \"while\"";
      break;
    case ec_nonstd_default_arg:
      m = "this use of a default argument is nonstandard";
      break;
    case ec_never_defined:
      m = "%n was referenced but not defined";
      break;
    case ec_continue_must_be_in_loop:
      m = "a continue statement may only be used within a loop";
      break;
    case ec_break_must_be_in_loop_or_switch:
      m = "a break statement may only be used within a loop or switch";
      break;
    case ec_no_value_returned_in_non_void_function:
      m = "non-void %nd should return a value";
      break;
    case ec_value_returned_in_void_function:
      m = "a void function may not return a value";
      break;
    case ec_cast_to_bad_type:
      m = "cast to type %t is not allowed";
      break;
    case ec_bad_return_value_type:
      m = "return value type does not match the function type";
      break;
    case ec_case_label_must_be_in_switch:
      m = "a case label may only be used within a switch";
      break;
    case ec_default_label_must_be_in_switch:
      m = "a default label may only be used within a switch";
      break;
    case ec_case_label_appears_more_than_once:
      m = "case label value has already appeared in this switch";
      break;
    case ec_default_label_appears_more_than_once:
      m = "default label has already appeared in this switch";
      break;
    case ec_exp_lparen:
      m = "expected a \"(\"";
      break;
    case ec_expr_not_an_lvalue:
      m = "expression must be an lvalue";
      break;
    case ec_exp_statement:
      m = "expected a statement";
      break;
    case ec_loop_not_reachable:
      m = "loop is not reachable from preceding code";
      break;
    case ec_block_scope_function_must_be_extern:
      m = "a block-scope function may only have extern storage class";
      break;
    case ec_exp_lbrace:
      m = "expected a \"{\"";
      break;
    case ec_expr_not_ptr_to_class:
      m = "expression must have pointer-to-class type";
      break;
    case ec_expr_not_ptr_to_struct_or_union:
      m = "expression must have pointer-to-struct-or-union type";
      break;
    case ec_exp_member_name:
      m = "expected a member name";
      break;
    case ec_exp_field_name:
      m = "expected a field name";
      break;
    case ec_not_a_member:
      m = "%n has no member %sq";
      break;
    case ec_not_a_field:
      m = "%n has no field %sq";
      break;
    case ec_expr_not_a_modifiable_lvalue:
      m = "expression must be a modifiable lvalue";
      break;
    case ec_address_of_register_variable:
      m = "taking the address of a register variable is not allowed";
      break;
    case ec_address_of_bit_field:
      m = "taking the address of a bit field is not allowed";
      break;
    case ec_too_many_arguments:
      m = "too many arguments in function call";
      break;
    case ec_all_proto_params_must_be_named:
      m = "unnamed prototyped parameters not allowed when body is present";
      break;
    case ec_expr_not_pointer_to_object:
      m = "expression must have pointer-to-object type";
      break;
    case ec_program_too_large:
      m = "program too large or complicated to compile";
      break;
    case ec_bad_initializer_type:
      m =
      "a value of type %t1 cannot be used to initialize an entity of type %t2";
      break;
    case ec_cannot_initialize:
      m = "%n may not be initialized";
      break;
    case ec_too_many_initializer_values:
      m = "too many initializer values";
      break;
    case ec_not_compatible_with_previous_decl:
      m = "declaration is incompatible with %nfd";
      break;
    case ec_already_initialized:
      m = "%n has already been initialized";
      break;
    case ec_bad_file_scope_storage_class:
      m = "a global-scope declaration may not have this storage class";
      break;
    case ec_type_cannot_be_param_name:
      m = "a type name may not be redeclared as a parameter";
      break;
    case ec_typedef_cannot_be_param_name:
      m = "a typedef name may not be redeclared as a parameter";
      break;
    case ec_non_zero_int_conv_to_pointer:
      m = "conversion of nonzero integer to pointer";
      break;
    case ec_expr_not_class:
      m = "expression must have class type";
      break;
    case ec_expr_not_struct_or_union:
      m = "expression must have struct or union type";
      break;
    case ec_old_fashioned_assignment_operator:
      m = "old-fashioned assignment operator";
      break;
    case ec_old_fashioned_initializer:
      m = "old-fashioned initializer";
      break;
    case ec_expr_not_integral_constant:
      m = "expression must be an integral constant expression";
      break;
    case ec_expr_not_an_lvalue_or_function_designator:
      m = "expression must be an lvalue or a function designator";
      break;
    case ec_decl_incompatible_with_previous_use:
      m = "declaration is incompatible with previous %nod";
      break;
    case ec_external_name_clash:
      m = "name conflicts with previously used external name %sq";
      break;
    case ec_unrecognized_pragma:
      m = "unrecognized #pragma";
      break;
    case ec_expr_not_scalar_or_void:
      m = "expression must have arithmetic, pointer, or void type";
      break;
    case ec_cannot_open_temp_file:
      m = "could not open temporary file %sq";
      break;
    case ec_temp_file_dir_name_too_long:
      m = "name of directory for temporary files is too long (%sq)";
      break;
    case ec_too_few_arguments:
      m = "too few arguments in function call";
      break;
    case ec_bad_float_constant:
      m = "invalid floating constant";
      break;
    case ec_incompatible_param:
      m = "argument of type %t1 is incompatible with parameter of type %t2";
      break;
    case ec_function_type_not_allowed:
      m = "a function type is not allowed here";
      break;
    case ec_exp_declaration:
      m = "expected a declaration";
      break;
    case ec_pointer_outside_base_object:
      m = "pointer points outside of underlying object";
      break;
    case ec_bad_cast:
      m = "invalid type conversion";
      break;
    case ec_linkage_conflict:
      m = "external/internal linkage conflict with previous declaration";
      break;
    case ec_float_to_integer_conversion:
      m = "floating-point value does not fit in required integral type";
      break;
    case ec_expr_has_no_effect:
      m = "expression has no effect";
      break;
    case ec_subscript_out_of_range:
      m = "subscript out of range";
      break;
    case ec_constant_string_subscript_out_of_range:
      m = "constant string subscript out of range";
      break;
    case ec_declared_but_not_referenced:
      m = "%n was declared but never referenced";
      break;
    case ec_pcc_address_of_array:
      m = "\"&\" applied to an array has no effect";
      break;
    case ec_mod_by_zero:
      m = "right operand of \"%%\" is zero";
      break;
    case ec_old_style_incompatible_param:
      m = "argument is incompatible with formal parameter";
      break;
    case ec_printf_arg_mismatch:
      m =
        "argument is incompatible with corresponding format string conversion";
      break;
    case ec_empty_include_search_path:
      m = "could not open source file %sq (no directories in search list)";
      break;
    case ec_cast_not_integral:
      m = "type of cast must be integral";
      break;
    case ec_cast_not_scalar:
      m = "type of cast must be arithmetic or pointer";
      break;
    case ec_initialization_not_reachable:
      m = "dynamic initialization in unreachable code";
      break;
    case ec_unsigned_compare_with_zero:
      m = "pointless comparison of unsigned integer with zero";
      break;
    case ec_assign_where_compare_meant:
      m = "possible use of \"=\" where \"==\" was intended";
      break;
    case ec_mixed_enum_type:
      m = "enumerated type mixed with another type";
      break;
    case ec_file_write_error:
      m = "error while writing %s file";
      break;
    case ec_bad_il_file:
      m = "invalid intermediate language file";
      break;
    case ec_cast_to_qualified_type:
      m = "type qualifier is meaningless on cast type";
      break;
    case ec_unrecognized_char_escape:
      m = "unrecognized character escape sequence";
      break;
    case ec_undefined_preproc_id:
      m = "zero used for undefined preprocessing identifier";
      break;
    case ec_exp_asm_string:
      m = "expected an asm string";
      break;
    case ec_asm_func_must_be_prototyped:
      m = "an asm function must be prototyped";
      break;
    case ec_bad_asm_func_ellipsis:
      m = "an asm function may not have an ellipsis";
      break;
    case ec_asm_with_non_function:
      m = "asm may only be used to declare a function";
      break;
    case ec_asm_func_has_storage_class:
      m = "an asm function may not have a storage class";
      break;
    case ec_bad_asm_func_return_size:
      m = "asm return value size does not match function return type";
      break;
    case ec_bad_asm_func_param_size:
      m = "asm parameter size does not match function parameter size";
      break;
    case ec_exp_percent:
      m = "expected a \"%\"";
      break;
    case ec_asm_specifier_conflicts_with_prev:
      m = "invalid combination of asm control specifiers";
      break;
    case ec_extra_text_on_asm_control_line:
      m = "extra text after expected end of asm control line";
      break;
    case ec_exp_asm_control_specifier:
      m = "expected an asm control specifier";
      break;
    case ec_asm_name_already_defined:
      m = "this asm name is already defined";
      break;
    case ec_bad_asm_reg_name:
      m = "invalid register name";
      break;
    case ec_asm_param_may_not_be_void:
      m = "an asm parameter may not have void type";
      break;
    case ec_exp_asm_type:
      m = "expected an asm type specification";
      break;
    case ec_bad_asm_type_specification:
      m = "invalid asm type specification";
      break;
    case ec_bad_asm_type_width:
      m = "invalid asm type width";
      break;
    case ec_bad_asm_constant:
      m = "invalid asm constant";
      break;
    case ec_bad_asm_temp_type:
      m = "an asm temporary may not have this type";
      break;
    case ec_cannot_ref_untyped_asm_param:
      m = "this parameter may not be referenced because it has no type";
      break;
    case ec_cannot_ref_void_asm_return:
      m = "the return value may not be referenced because its type is void";
      break;
    case ec_bad_asm_reg_spec:
      m = "invalid register specifier";
      break;
    case ec_asm_leaf_has_no_expansion_lines:
      m = "an expansion leaf must have at least one expansion line";
      break;
    case ec_cannot_ref_untyped_asm_return:
      m = "the return value may not be referenced because it has no type";
      break;
    case ec_bad_asm_return_type:
      m = "the return value may not have this asm type";
      break;
    case ec_file_delete_error:
      m = "error while deleting file %sq";
      break;
    case ec_integer_to_float_conversion:
      m = "integral value does not fit in required floating-point type";
      break;
    case ec_float_to_float_conversion:
      m = "floating-point value does not fit in required floating-point type";
      break;
    case ec_bad_float_operation_result:
      m = "floating-point operation result is out of range";
      break;
    case ec_implicit_func_decl:
      m = "function declared implicitly";
      break;
    case ec_too_few_printf_args:
      m = "the format string requires additional arguments";
      break;
    case ec_too_many_printf_args:
      m = "the format string ends before this argument";
      break;
    case ec_bad_printf_format_string:
      m = "invalid format string conversion";
      break;
    case ec_macro_recursion:
      m = "macro recursion";
      break;
    case ec_nonstd_extra_comma:
      m = "extra final comma is nonstandard";
      break;
    case ec_enum_bit_field_too_small:
      m = "bit field cannot contain all values of the enumerated type";
      break;
    case ec_nonstd_bit_field_type:
      m = "nonstandard type for a bit field";
      break;
    case ec_decl_in_prototype_scope:
      m = "declaration is not visible outside of function";
      break;
    case ec_decl_of_void_ignored:
      m = "old-fashioned typedef of \"void\" ignored";
      break;
    case ec_old_fashioned_field_selection:
      m = "left operand is not a struct or union containing this field";
      break;
    case ec_old_fashioned_ptr_field_selection:
      m = "pointer does not point to struct or union containing this field";
      break;
    case ec_var_retained_incomp_type:
      m = "variable %sq was declared with a never-completed type";
      break;
    case ec_boolean_controlling_expr_is_constant:
      m = "controlling expression is constant";
      break;
    case ec_switch_selector_expr_is_constant:
      m = "selector expression is constant";
      break;
    case ec_bad_param_specifier:
      m = "invalid specifier on a parameter";
      break;
    case ec_bad_specifier_outside_class_decl:
      m = "invalid specifier outside a class declaration";
      break;
    case ec_dupl_decl_specifier:
      m = "duplicate specifier in declaration";
      break;
    case ec_base_class_not_allowed_for_union:
      m = "a union is not allowed to have a base class";
      break;
    case ec_access_already_specified:
      m = "multiple access control specifiers are not allowed";
      break;
    case ec_missing_class_definition:
      m = "class or struct definition is missing";
      break;
    case ec_name_not_member_of_class_or_base_classes:
      m = "qualified name is not a member of class %t or its base classes";
      break;
    case ec_member_ref_requires_object:
      m = "a nonstatic member reference must be relative to a specific object";
      break;
    case ec_nonstatic_member_def_not_allowed:
      m = "a nonstatic data member may not be defined outside its class";
      break;
    case ec_already_defined:
      m = "%n has already been defined";
      break;
    case ec_pointer_to_reference:
      m = "pointer to reference is not allowed";
      break;
    case ec_reference_to_reference:
      m = "reference to reference is not allowed";
      break;
    case ec_reference_to_void:
      m = "reference to void is not allowed";
      break;
    case ec_array_of_reference:
      m = "array of reference is not allowed";
      break;
    case ec_missing_initializer_on_reference:
      m = "reference %n requires an initializer";
      break;
    case ec_exp_comma:
      m = "expected a \",\"";
      break;
    case ec_type_identifier_not_allowed:
      m = "type name is not allowed";
      break;
    case ec_type_definition_not_allowed:
      m = "type definition is not allowed";
      break;
    case ec_bad_type_name_redeclaration:
      m = "invalid redeclaration of type name %sq";
      break;
    case ec_missing_initializer_on_const:
      m = "const %n requires an initializer";
      break;
    case ec_this_used_incorrectly:
      m = "\"this\" may only be used inside a nonstatic member function";
      break;
    case ec_constant_value_not_known:
      m = "constant value is not known";
      break;
    case ec_missing_type_specifier:
      m = "explicit type is missing (\"int\" assumed)";
      break;
    case ec_missing_access_specifier:
      m = "access control not specified (%sq by default)";
      break;
    case ec_not_a_class_or_struct_name:
      m = "not a class or struct name";
      break;
    case ec_dupl_base_class_name:
      m = "duplicate base class name";
      break;
    case ec_bad_base_class:
      m = "invalid base class";
      break;
    case ec_no_access_to_name:
      m = "%n is inaccessible";
      break;
    case ec_ambiguous_name:
      m = "%no is ambiguous";
      break;
    case ec_old_style_parameter_list:
      m = "old-style parameter list (anachronism)";
      break;
    case ec_declaration_after_statements:
      m = "declaration may not appear after executable statement in block";
      break;
    case ec_inaccessible_base_class:
      m = "base class %t is inaccessible";
      break;
    case ec_not_a_base_class_member:
      m = "name is not a member of a base class of %sq";
      break;
    case ec_access_adjustment_in_private_section:
      m = "access adjustment in a \"private\" section is not allowed";
      break;
    case ec_increasing_access_not_allowed:
      m = "increasing an inherited member's access is not allowed";
      break;
    case ec_restricting_access_not_allowed:
      m = "restricting an inherited member's access is not allowed";
      break;
    case ec_improperly_terminated_macro_call:
      m = "improperly terminated macro invocation";
      break;
    case ec_bad_access_decl_name_is_hidden:
      m = "invalid access declaration -- %no1 is hidden by %no2";
      break;
    case ec_id_must_be_class_name:
      m = "name followed by \"::\" must be a class name";
      break;
    case ec_bad_friend_decl:
      m = "invalid friend declaration";
      break;
    case ec_value_returned_in_constructor:
      m = "a constructor or destructor may not return a value";
      break;
    case ec_bad_destructor_decl:
      m = "invalid destructor declaration";
      break;
    case ec_class_and_member_name_conflict:
      m = "invalid declaration of a member with the same name as its class";
      break;
    case ec_global_qualifier_not_allowed:
      m = "global-scope qualifier (leading \"::\") is not allowed";
      break;
    case ec_name_not_found_in_file_scope:
      m = "the global scope has no %sq";
      break;
    case ec_qualified_name_not_allowed:
      m = "qualified name is not allowed";
      break;
    case ec_null_reference:
      m = "NULL reference is not allowed";
      break;
    case ec_brace_initialization_not_allowed:
      m = "initialization with \"{...}\" is not allowed for object of type %t";
      break;
    case ec_ambiguous_base_class:
      m = "base class %t is ambiguous";
      break;
    case ec_ambiguous_derived_class:
      m = "derived class %t1 contains more than one instance of class %t2";
      break;
    case ec_derived_class_from_virtual_base:
      m = "derived class %t1 has class %t2 as a virtual base class";
      break;
    case ec_no_matching_constructor:
      m = "no instance of constructor %no matches the argument list";
      break;
    case ec_ambiguous_copy_constructor:
      m = "copy constructor for class %t is ambiguous";
      break;
    case ec_no_default_constructor:
      m = "no default constructor exists for class %t";
      break;
    case ec_not_a_field_or_base_class:
      m = "%sq is not a nonstatic data member or base class of class %t";
      break;
    case ec_indirect_nonvirtual_base_class_not_allowed:
      m = "indirect nonvirtual base class is not allowed";
      break;
    case ec_bad_union_field:
      m = "invalid union member -- class %t has a disallowed member function";
      break;
    case ec_overloaded_function_types_too_similar:
      m = "cannot overload functions -- parameter types are too similar";
      break;
    case ec_bad_rvalue_array:
      m = "invalid use of non-lvalue array";
      break;
    case ec_exp_operator:
      m = "expected an operator";
      break;
    case ec_inherited_member_not_allowed:
      m = "inherited member is not allowed";
      break;
    case ec_indeterminate_overloaded_function:
      m = "cannot determine which instance of %n is intended";
      break;
    case ec_bound_function_must_be_called:
      m =
         "a pointer to a bound function may only be used to call the function";
      break;
    case ec_duplicate_typedef:
      m = "typedef name has already been declared (with same type)";
      break;
    case ec_function_redefinition:
      m = "%n has already been defined";
      break;
    case ec_overloaded_function_incompatible_type:
      m = "type does not match any instance of %n";
      break;
    case ec_no_matching_function:
      m = "no instance of %n matches the argument list";
      break;
    case ec_type_def_not_allowed_in_func_type_decl:
      m = "type definition is not allowed in function return type declaration";
      break;
    case ec_default_arg_not_at_end:
      m = "default argument not at end of parameter list";
      break;
    case ec_default_arg_already_defined:
      m = "redefinition of default argument";
      break;
    case ec_ambiguous_overloaded_function:
      m = "more than one instance of %n matches the argument list:";
      break;
    case ec_ambiguous_constructor:
      m =
        "more than one instance of constructor %no matches the argument list:";
      break;
    case ec_bad_default_arg_type:
      m =
     "default argument of type %t1 is incompatible with parameter of type %t2";
      break;
    case ec_return_type_cannot_distinguish_functions:
      m = "cannot overload functions distinguished by return type alone";
      break;
    case ec_no_user_defined_conversion:
      m = "no suitable user-defined conversion from %t1 to %t2 exists";
      break;
    case ec_function_qualifier_not_allowed:
      m = "const or volatile qualifier on this function is not allowed";
      break;
    case ec_virtual_static_not_allowed:
      m = "only nonstatic member functions may be virtual";
      break;
    case ec_unqual_function_with_qual_object:
      m = "function may not be called for const- or volatile-qualified object";
      break;
    case ec_too_many_virtual_functions:
      m = "program too large to compile (too many virtual functions)";
      break;
    case ec_bad_return_type_on_virtual_function_override:
      m = "type differs from base class virtual function by return type alone";
      break;
    case ec_ambiguous_virtual_function_override:
      m = "override of virtual %n is ambiguous";
      break;
    case ec_pure_specifier_on_nonvirtual_function:
      m = "pure specifier (\"= 0\") allowed only on virtual functions";
      break;
    case ec_bad_pure_specifier:
      m = "badly-formed pure specifier (only \"= 0\" is allowed)";
      break;
    case ec_bad_data_member_initialization:
      m = "data member initializer is not allowed";
      break;
    case ec_abstract_class_object_not_allowed:
      m = "object of abstract class type is not allowed";
      break;
    case ec_function_returning_abstract_class:
      m = "function returning abstract class is not allowed";
      break;
    case ec_duplicate_friend_decl:
      m = "duplicate friend declaration";
      break;
    case ec_inline_and_nonfunction:
      m = "inline specifier allowed on function declarations only";
      break;
    case ec_inline_not_allowed:
      m = "\"inline\" is not allowed";
      break;
    case ec_bad_storage_class_with_inline:
      m = "invalid storage class for an inline function";
      break;
    case ec_bad_member_storage_class:
      m = "invalid storage class for a class member";
      break;
    case ec_local_class_function_def_missing:
      m = "member function of local class -- definition is required";
      break;
    case ec_inaccessible_special_function:
      m = "%nf is inaccessible";
      break;
    case ec_direct_derivation_less_accessible:
      m = "direct path to base class %t gives less access than indirect path";
      break;
    case ec_missing_const_copy_constructor:
      m = "class %t has no copy constructor to copy a const object";
      break;
    case ec_definition_of_implicitly_declared_function:
      m = "defining an implicitly declared member function is not allowed";
      break;
    case ec_no_suitable_copy_constructor:
      m = "class %t has no suitable copy constructor";
      break;
    case ec_linkage_specifier_not_allowed:
      m = "linkage specification is not allowed";
      break;
    case ec_bad_linkage_specifier:
      m = "unknown external linkage specification";
      break;
    case ec_incompatible_linkage_specifier:
      m = "linkage specification is incompatible with previous %nod";
      break;
    case ec_overloaded_function_linkage:
      m = "more than one instance of %n has \"C\" linkage";
      break;
    case ec_ambiguous_default_constructor:
      m = "class %t has more than one default constructor";
      break;
    case ec_temp_used_for_ref_init:
      m = "value copied to temporary, reference to temporary used";
      break;
    case ec_nonmember_operator_not_allowed:
      m = "\"operator%s\" must be a member function";
      break;
    case ec_static_member_operator_not_allowed:
      m = "operator may not be a static member function";
      break;
    case ec_too_many_args_for_conversion:
      m = "no arguments allowed on user-defined conversion";
      break;
    case ec_too_many_args_for_operator:
      m = "too many arguments for operator function";
      break;
    case ec_too_few_args_for_operator:
      m = "too few arguments for operator function";
      break;
    case ec_no_args_with_class_type:
      m = "nonmember operator requires an argument with class type";
      break;
    case ec_default_arg_expr_not_allowed:
      m = "default argument is not allowed";
      break;
    case ec_ambiguous_user_defined_conversion:
      m = "more than one user-defined conversion from %t1 to %t2 applies:";
      break;
    case ec_no_matching_operator_function:
      m = "none of the available operator functions matches these operands";
      break;
    case ec_ambiguous_operator_function:
      m = "more than one operator %sq matches these operands:";
      break;
    case ec_bad_arg_type_for_operator_new:
      m = "operator new() requires first argument of type \"size_t\"";
      break;
    case ec_bad_return_type_for_op_new:
      m = "operator new() requires return type of \"void *\"";
      break;
    case ec_bad_return_type_for_op_delete:
      m = "operator delete() requires return type of \"void\"";
      break;
    case ec_bad_first_arg_type_for_operator_delete:
      m = "operator delete() requires first argument of type \"void *\"";
      break;
    case ec_bad_second_arg_type_for_operator_delete:
      m = "second argument of operator delete() must be of type \"size_t\"";
      break;
    case ec_type_must_be_object_type:
      m = "type must be an object type";
      break;
    case ec_base_class_already_initialized:
      m = "base class %t has already been initialized";
      break;
    case ec_base_class_init_anachronism:
      m = "base class name required -- %t assumed (anachronism)";
      break;
    case ec_member_already_initialized:
      m = "%n has already been initialized";
      break;
    case ec_missing_base_class_or_member_name:
      m = "name of member or base class is missing";
      break;
    case ec_assignment_to_this:
      m = "assignment to \"this\" (anachronism)";
      break;
    case ec_overload_anachronism:
      m = "\"overload\" keyword used (anachronism)";
      break;
    case ec_anon_union_member_access:
      m = "invalid anonymous union -- nonpublic member is not allowed";
      break;
    case ec_anon_union_member_function:
      m = "invalid anonymous union -- member function is not allowed";
      break;
    case ec_anon_union_storage_class:
      m = "global anonymous union must be declared static";
      break;
    case ec_missing_initializer_on_fields:
      m = "%nf provides no initializer for:";
      break;
    case ec_cannot_initialize_fields:
      m = "implicitly generated constructor for class %t cannot initialize:";
      break;
    case ec_no_ctor_but_const_or_ref_member:
      m = "%n defines no constructor to initialize the following:";
      break;
    case ec_var_with_uninitialized_member:
      m = "%n has an uninitialized const or reference member";
      break;
    case ec_var_with_uninitialized_field:
      m = "%n has an uninitialized const field";
      break;
    case ec_missing_const_assignment_operator:
      m = "class %t has no assignment operator to copy a const object";
      break;
    case ec_no_suitable_assignment_operator:
      m = "class %t has no suitable assignment operator";
      break;
    case ec_ambiguous_assignment_operator:
      m = "ambiguous default assignment operator for class %t";
      break;
    case ec_const_volatile_not_allowed:
      m = "const or volatile qualifier is not allowed";
      break;
    case ec_missing_typedef_name:
      m = "declaration requires a typedef name";
      break;
    case ec_missing_object_name:
      m = "declaration requires an object name";
      break;
    case ec_virtual_not_allowed:
      m = "\"virtual\" is not allowed";
      break;
    case ec_static_not_allowed:
      m = "\"static\" is not allowed";
      break;
    case ec_bound_function_cast_anachronism:
      m = "cast of bound function to normal function pointer (anachronism)";
      break;
    case ec_expr_not_ptr_to_member:
      m = "expression must have pointer-to-member type";
      break;
    case ec_extra_semicolon:
      m = "extra \";\" ignored";
      break;
    case ec_nonstd_const_member:
      m = "declaring a member constant is nonstandard";
      break;
    case ec_delete_of_const_pointer:
      m = "a pointer to const may not be deleted";
      break;
    case ec_no_matching_new_function:
      m = "no instance of overloaded %no matches these operands";
      break;
    case ec_delete_already_declared:
      m = "operator delete() may not be overloaded";
      break;
    case ec_no_match_for_addr_of_overloaded_function:
      m = "no instance of %n matches the required type";
      break;
    case ec_delete_count_anachronism:
      m = "delete array size expression ignored (anachronism)";
      break;
    case ec_bad_return_type_for_op_arrow:
      m = "operator->() requires pointer-to-class return type";
      break;
    case ec_cast_to_abstract_class:
      m = "a cast to an abstract class is not allowed";
      break;
    case ec_bad_use_of_main:
      m = "function \"main\" may not be called or have its address taken";
      break;
    case ec_initializer_not_allowed_on_array_new:
      m = "a new-initializer may not be specified for an array";
      break;
    case ec_member_function_redecl_outside_class:
      m = "a member function may not be redeclared outside its class";
      break;
    case ec_ptr_to_incomplete_class_type_not_allowed:
      m = "pointer to incomplete class type is not allowed";
      break;
    case ec_ref_to_nested_function_var:
      m = "reference to local variable of enclosing function is not allowed";
      break;
    case ec_single_arg_postfix_incr_decr_anachronism:
      m = "single-argument function used for postfix %sq (anachronism)";
      break;
    case ec_bad_access_adjustment_with_overloading:
      m = "access adjustment is not allowed -- mixed accessibility for %n";
      break;
    case ec_bad_default_assignment:
      m = "implicitly generated assignment operator cannot copy:";
      break;
    case ec_nonstd_array_cast:
      m = "cast to array type is nonstandard (treated as cast to %t)";
      break;
    case ec_class_with_op_new_but_no_op_delete:
      m = "%n has an operator new() but no operator delete()";
      break;
    case ec_class_with_op_delete_but_no_op_new:
      m = "%n has an operator delete() but no operator new()";
      break;
    case ec_base_class_with_nonvirtual_dtor:
      m = "destructor for base class %t is not virtual";
      break;
    case ec_no_access_to_constructors:
      m = "%n has no accessible constructors";
      break;
    case ec_member_function_redeclaration:
      m = "%n has already been declared";
      break;
    case ec_inline_main:
      m = "function \"main\" may not be declared inline";
      break;
    case ec_class_and_member_function_name_conflict:
      m =
       "member function with the same name as its class must be a constructor";
      break;
    case ec_nested_class_anachronism:
      m = "using nested %n (anachronism)";
      break;
    case ec_too_many_params_for_destructor:
      m = "a destructor may not have parameters";
      break;
    case ec_bad_constructor_param:
      m = "copy constructor for class %t may not have a parameter of type %t";
      break;
    case ec_incomplete_function_return_type:
      m = "function return type is incomplete";
      break;
    case ec_protected_access_problem:
      m = "protected %n is not accessible through a %t pointer or object";
      break;
    case ec_param_not_allowed:
      m = "a parameter is not allowed";
      break;
    case ec_asm_not_allowed:
      m = "an \"asm\" declaration is not allowed at this point";
      break;
    case ec_no_conversion_function:
      m = "no suitable conversion function from %t1 to %t2 exists";
      break;
    case ec_delete_of_incomplete_class:
      m = "delete of pointer to incomplete class";
      break;
    case ec_no_constructor_for_conversion:
      m = "no suitable constructor exists to convert from %t1 to %t2";
      break;
    case ec_ambiguous_constructor_for_conversion:
      m = "more than one constructor applies to convert from %t1 to %t2:";
      break;
    case ec_ambiguous_conversion_function:
      m = "more than one conversion function from %t1 to %t2 applies:";
      break;
    case ec_ambiguous_conversion_to_builtin:
      m =
       "more than one conversion function from %t to a built-in type applies:";
      break;
    case ec_const_member:
      m = "const %n";
      break;
    case ec_reference_member:
      m = "reference %n";
      break;
    case ec_ambiguous_function_add_on:
      m = "%n";
      break;
    case ec_builtin_operator_add_on:
      m = "built-in operator %sq";
      break;
    case ec_ambiguous_by_inheritance_add_on:
      m = "%n (ambiguous by inheritance)";
      break;
    case ec_addr_of_constructor_or_destructor:
      m = "a constructor or destructor may not have its address taken";
      break;
    case ec_dollar_used_in_identifier:
      m = "dollar sign (\"$\") used in identifier";
      break;
    case ec_nonconst_ref_init_anachronism:
      m = 
    "temporary used for initial value of reference to non-const (anachronism)";
      break;
    case ec_qualifier_in_member_declaration:
      m = "qualified name is not allowed in member declaration";
      break;
    case ec_mixed_enum_type_anachronism:
      m = "enumerated type mixed with another type (anachronism)";
      break;
    case ec_new_array_size_must_be_nonnegative:
      m = "the size of an array in \"new\" must be non-negative";
      break;
    case ec_return_ref_init_requires_temp:
      m = "returning reference to local temporary";
      break;
    case ec_cfront_nonconst_ref_init:
      m = "const qualifier dropped in initializing reference to non-const";
      break;
    case ec_enum_not_allowed:
      m = "\"enum\" declaration is not allowed";
      break;
    case ec_qualifier_dropped_in_ref_init:
      m = "initial value of reference has excess const/volatile qualifiers";
      break;
    case ec_bad_nonconst_ref_init:
      m = "initial value of reference to non-const has incorrect type";
      break;
    case ec_delete_of_function_pointer:
      m = "a pointer to function may not be deleted";
      break;
    case ec_bad_conversion_function_decl:
      m = "conversion function must be a nonstatic member function";
      break;
    case ec_nonglobal_template_declaration:
      m = "nonglobal template declaration is not allowed";
      break;
    case ec_exp_lt:
      m = "expected a \"<\"";
      break;
    case ec_exp_gt:
      m = "expected a \">\"";
      break;
    case ec_missing_template_param:
      m = "template parameter declaration is missing";
      break;
    case ec_missing_template_arg_list:
      m = "argument list for %nf is missing";
      break;
    case ec_too_few_template_args:
      m = "too few arguments for %nf";
      break;
    case ec_too_many_template_args:
      m = "too many arguments for %nf";
      break;
    case ec_not_a_type_arg:
      m = "template parameter for a function template must be a type";
      break;
    case ec_not_used_in_template_function_params:
      m = "%n1 is not used in declaring the argument types of %n2";
      break;
    case ec_cfront_multiple_nested_types:
      m = "two nested types have the same name: %no1 and %nod2 (cfront compatibility)";
      break;
    case ec_cfront_global_defined_after_nested_type:
      m = "global %no1 was declared after nested %nod2 (cfront compatibility)";
      break;
    case ec_template_param_declared_but_not_referenced:
      m = "template parameter %no was declared but never referenced";
      break;
    case ec_ambiguous_ptr_to_overloaded_function:
      m = "more than one instance of %n matches the required type";
      break;
    case ec_nonstd_long_long:
      m = "the type \"long long\" is nonstandard";
      break;
    case ec_nonstd_friend_decl:
      m = "omission of \"%s\" is nonstandard";
      break;
    case ec_return_type_on_conversion_function:
      m = "return type may not be specified on a conversion function";
      break;
    case ec_template_detected_during_header:
      m = "detected during:";
      break;
    case ec_template_instantiation_context:
      m = "%sinstantiation of %nf %p";
      break;
    case ec_compiler_generated_function_context:
      m = "%simplicit generation of %nf %p";
      break;
    case ec_runaway_recursive_instantiation:
      m = "excessive recursion at instantiation of %n";
      break;
    case ec_bad_template_declaration:
      m = "\"%s\" is not a function or static data member";
      break;
    case ec_bad_nontype_template_arg:
      m =
    "argument of type %t1 is incompatible with template parameter of type %t2";
      break;
    case ec_init_needing_temp_not_allowed:
      m = "initialization requiring a temporary or conversion is not allowed";
      break;
    case ec_decl_hides_function_parameter:
      m = "declaration of %sq hides function parameter";
      break;
    case ec_nonconst_ref_init_from_rvalue:
      m = "initial value of reference to non-const must be an lvalue";
      break;
    case ec_implicit_static_data_member_definition:
      m = "%simplicit definition of %nf %p";
      break;
    case ec_template_not_allowed:
      m = "\"template\" is not allowed";
      break;
    case ec_not_a_class_template:
      m = "%t is not a class template";
      break;
    case ec_static_data_member_anon_union:
      m = "static data member may not be an anonymous union";
      break;
    case ec_function_template_named_main:
      m = "\"main\" is not a valid name for a function template";
      break;
    case ec_union_nonunion_mismatch:
      m = "invalid reference to %n (union/nonunion mismatch)";
      break;
    case ec_local_type_in_template_arg:
      m = "a template argument may not reference a local type";
      break;
    case ec_tag_kind_incompatible_with_declaration:
      m = "tag kind of %s is incompatible with declaration of %nfd";
      break;
    case ec_name_not_tag_in_file_scope:
      m = "the global scope has no tag named %sq";
      break;
    case ec_not_a_tag_member:
      m = "%n has no tag member named %sq";
      break;
    case ec_ptr_to_member_typedef:
      m = "member function typedef (allowed for cfront compatibility)";
      break;
    case ec_bad_use_of_ptr_to_member_typedef:
      m = "%n may be used only in pointer-to-member declaration";
      break;
#ifdef REMOVED
    case ec_empty_initializer_list:
      m = "empty initializer list is nonstandard";
      break;
#endif /* REMOVED */
    case ec_nonexternal_entity_in_template_arg:
      m = "a template argument may not reference a non-external entity";
      break;
    case ec_id_must_be_class_or_type_name:
      m = "name followed by \"::~\" must be a class name or a type name";
      break;
    case ec_destructor_name_mismatch:
      m = "destructor name does not match name of class %t";
      break;
    case ec_destructor_type_mismatch:
      m = "type used as destructor name does not match type %t";
      break;
    case ec_called_function_redeclared_inline:
      m = "%n may not be redeclared \"inline\" after being called";
      break;
    case ec_vacuous_destructor_name_mismatch:
      m = "destructor name does not match left operand of \"->\" or \".\"";
      break;
    case ec_bad_storage_class_on_template_decl:
      m = "invalid storage class for a template declaration";
      break;
    case ec_no_access_to_type_cfront_mode:
      m = "%n is an inaccessible type (allowed for cfront compatibility)";
      break;
    case ec_return_type_not_allowed:
      m = "a return type is not allowed";
      break;
    case ec_invalid_instantiation_pragma_argument:
      m = "invalid instantiation pragma argument";
      break;
    case ec_not_instantiatable_entity:
      m = "%nf is not an entity that can be instantiated";
      break;
    case ec_compiler_generated_function_cannot_be_instantiated:
      m = "compiler generated function %n cannot be instantiated";
      break;
    case ec_inline_function_cannot_be_instantiated:
      m = "inline function %n cannot be instantiated";
      break;
    case ec_pure_virtual_function_cannot_be_instantiated:
      m = "pure virtual function %n cannot be instantiated";
      break;
    case ec_instantiation_requested_no_definition_supplied:
      m = "%n cannot be instantiated -- no template definition was supplied";
      break;
    case ec_instantiation_requested_and_specific_definition:
      m =
        "%n cannot be instantiated -- a specific definition has been supplied";
      break;
    case ec_no_constructor:
      m = "class %t has no constructor";
      break;
    case ec_template_param_only_used_in_default_args:
      m = "%n1 must be used in a parameter without a default value in %n2";
      break;
    case ec_no_match_for_type_of_overloaded_function:
      m = "no instance of %n matches the specified type";
      break;
    case ec_nonstd_void_param_list:
      m = "declaring a void parameter list with a typedef is nonstandard";
      break;
    case ec_cfront_name_lookup_bug:
      m = "global %n used instead of %n2 (cfront compatibility)";
      break;
    case ec_redeclaration_of_template_param_name:
      m = "template parameter %sq may not be redeclared in this scope";
      break;
    case ec_decl_hides_template_parameter:
      m = "declaration of %sq hides template parameter";
      break;
    case ec_must_be_prototype_instantiation:
      m = "template argument list must match the parameter list";
      break;
    case ec_conversion_to_type_not_allowed:
      m = "conversion function to convert from %t1 to %t2 is not allowed";
      break;
    case ec_bad_extra_arg_for_postfix_operator:
      m = "extra argument of postfix \"operator%s\" must be of type \"int\"";
      break;
    case ec_function_type_required:
      m = "an operator name must be declared as a function";
      break;
    case ec_operator_name_not_allowed:
      m = "operator name is not allowed";
      break;
    case ec_specific_def_must_be_global:
      m = "class template specific definition not at global scope";
      break;
    case ec_nonstd_member_function_address:
      m = "nonstandard form for taking the address of a member function";
      break;
    case ec_too_few_template_params:
      m = "too few template parameters -- does not match previous declaration";
      break;
    case ec_too_many_template_params:
      m =
         "too many template parameters -- does not match previous declaration";
      break;
    case ec_template_operator_delete:
      m = "function template for operator delete() is not allowed";
      break;
    case ec_class_template_same_name_as_templ_param:
      m = "class template and template parameter may not have the same name";
      break;
    case ec_bad_constructor_name:
      m = "%no cannot be used to designate constructor for %n2";
      break;
    case ec_unnamed_type_in_template_arg:
      m = "a template argument may not reference an unnamed type";
      break;
    case ec_enum_type_not_allowed:
      m = "enumerated type is not allowed";
      break;
    case ec_qualified_reference_type:
      m = "type qualifier on a reference type is meaningless";
      break;
    case ec_incompatible_assignment_operands:
      m = "a value of type %t1 cannot be assigned to an entity of type %t2";
      break;
    case ec_unsigned_compare_with_negative:
      m = "pointless comparison of unsigned integer with a negative constant";
      break;
    case ec_converting_to_incomplete_class:
      m = "cannot convert to incomplete class %t";
      break;
    case ec_missing_initializer_on_unnamed_const:
      m = "const object requires an initializer";
      break;
    case ec_unnamed_object_with_uninitialized_field:
      m = "object has an uninitialized const or reference member";
      break;
    case ec_nonstd_pp_directive:
      m = "nonstandard preprocessing directive";
      break;
    case ec_unexpected_template_arg_list:
      m = "%n may not have a template argument list";
      break;
    case ec_missing_initializer_list:
      m = "initialization with \"{...}\" expected for aggregate object";
      break;
    case ec_incompatible_ptr_to_member_selection_operands:
      m =
      "pointer-to-member selection class types are incompatible (%t1 and %t2)";
      break;
    case ec_self_friendship:
      m = "pointless friend declaration";
      break;
    case ec_period_used_as_qualifier:
      m = "\".\" used in place of \"::\" to form a qualified name (cfront anachronism)";
      break;
    case ec_const_function_anachronism:
      m = "non-const function called for const object (cfront anachronism)";
      break;
    case ec_dependent_stmt_is_declaration:
      m = "a dependent statement may not be a declaration";
      break;
    case ec_void_param_not_allowed:
      m = "a parameter may not have void type";
      break;
    case ec_template_function_declaration_context:
      m = "%sinstantiation of %na %p";
      break;
    case ec_template_class_argument_list_context:
      m = "%sprocessing of template argument list for %na %p";
      break;
    case ec_bad_templ_arg_expr_operator:
      m = "this operator is not allowed in a template argument expression";
      break;
    case ec_missing_handler:
      m = "try block requires at least one handler";
      break;
    case ec_missing_exception_declaration:
      m = "handler requires an exception declaration";
      break;
    case ec_masked_by_default_handler:
      m = "handler is masked by default handler";
      break;
    case ec_masked_by_handler:
      m = "handler is masked by previous handler for type %t";
      break;
    case ec_local_type_used_in_exception:
      m = "use of a local type to specify an exception";
      break;
    case ec_redundant_throw_type:
      m = "redundant type in throw specification";
      break;
    case ec_incompatible_throw_specification:
      m = "throw specification is incompatible with that of previous %nd%s";
      break;
    case ec_previously_empty_throw_list:
      m = "previously specified: no exceptions will be thrown";
      break;
    case ec_previously_omitted_throw_type:
      m = "previously omitted: %t";
      break;
    case ec_previously_included_throw_type:
      m = "previously specified but omitted here: %t";
      break;
    case ec_no_exception_support:
      m = "support for exception handling is disabled";
      break;
    case ec_omitted_throw_specification:
      m = "omission of throw specification is incompatible with previous %nd";
      break;
    case ec_cannot_create_instantiation_information_file:
      m = "could not create instantiation information file %sq";
      break;
    case ec_non_arith_operation_in_templ_arg:
      m = "non-arithmetic operation not allowed in nontype template argument";
      break;
    case ec_local_type_in_nonlocal_var:
      m = "use of a local type to declare a nonlocal variable";
      break;
    case ec_local_type_in_function:
      m = "use of a local type to declare a function";
      break;
    case ec_branch_past_initialization:
      m = "transfer of control bypasses initialization of:";
      break;
    case ec_name_at_decl_position:
      m = "%nd";
      break;
    case ec_branch_into_handler:
      m = "transfer of control into an exception handler";
      break;
    case ec_used_before_set:
      m = "%n is used before its value is set";
      break;
    case ec_set_but_not_used:
      m = "%n was set but never used";
      break;
    case ec_bad_scope_for_definition:
      m = "%n cannot be defined in the current scope";
      break;
    case ec_throw_specification_not_allowed:
      m = "throw specification is not allowed";
      break;
    case ec_template_and_instance_linkage_conflict:
      m = "external/internal linkage conflict for %nfd";
      break;
    case ec_conversion_function_not_usable:
      m = "%nf will not be called for implicit or explicit conversions";
      break;
    case ec_tag_kind_incompatible_with_template_parameter:
      m = "tag kind of %s is incompatible with template parameter of type %t";
      break;
    case ec_template_operator_new:
      m = "function template for operator new(size_t) is not allowed";
      break;
    case ec_bad_access_decl_ambiguous_name:
      m = "invalid access declaration -- inherited name %sq is ambiguous";
      break;
    case ec_bad_member_type_in_ptr_to_member:
      m = "pointer to member of type %t is not allowed";
      break;
    case ec_ellipsis_on_operator_function:
      m = "ellipsis is not allowed in operator function parameter list";
      break;
    case ec_unimplemented_keyword:
      m = "%no is reserved for future use as a keyword";
      break;
    case ec_cl_invalid_macro_definition:
      m = "invalid macro definition: ";
      break;
    case ec_cl_invalid_macro_undefinition:
      m = "invalid macro undefinition: ";
      break;
    case ec_cl_invalid_preprocessor_output_file:
      m = "invalid preprocessor output file ";
      break;
    case ec_cl_cannot_open_preprocessor_output_file:
      m = "cannot open preprocessor output file ";
      break;
    case ec_cl_il_file_must_be_specified:
      m = "IL file name must be specified if input is ";
      break;
    case ec_cl_invalid_il_output_file:
      m = "invalid IL output file ";
      break;
    case ec_cl_cannot_open_il_output_file:
      m = "cannot open IL output file ";
      break;
    case ec_cl_invalid_C_output_file:
      m = "invalid C output file ";
      break;
    case ec_cl_cannot_open_C_output_file:
      m = "cannot open C output file ";
      break;
    case ec_cl_error_in_debug_option_argument:
      m = "error in debug option argument";
      break;
    case ec_cl_invalid_option:
      m = "invalid option: ";
      break;
    case ec_cl_back_end_requires_il_file:
      m = "back end requires name of IL file";
      break;
    case ec_cl_could_not_open_il_file:
      m = "could not open IL file ";
      break;
    case ec_cl_invalid_number:
      m = "invalid number: ";
      break;
    case ec_cl_incorrect_host_id:
      m = "incorrect host CPU id";
      break;
    case ec_cl_invalid_instantiation_mode:
      m = "invalid instantiation mode: ";
      break;
    case ec_cl_missing_include_directory:
      m = "missing include file directory name";
      break;
    case ec_cl_invalid_error_limit:
      m = "invalid error limit: ";
      break;
    case ec_cl_invalid_raw_listing_output_file:
      m = "invalid raw-listing output file ";
      break;
    case ec_cl_cannot_open_raw_listing_output_file:
      m = "cannot open raw-listing output file ";
      break;
    case ec_cl_invalid_xref_output_file:
      m = "invalid cross-reference output file ";
      break;
    case ec_cl_cannot_open_xref_output_file:
      m = "cannot open cross-reference output file ";
      break;
    case ec_cl_invalid_error_output_file:
      m = "invalid error output file ";
      break;
    case ec_cl_cannot_open_error_output_file:
      m = "cannot open error output file ";
      break;
    case ec_cl_vtbl_option_only_in_cplusplus:
      m =
      "virtual function tables can only be suppressed (-V) when compiling C++";
      break;
    case ec_cl_anachronism_option_only_in_cplusplus:
      m = "anachronism option (-O) can be used only when compiling C++";
      break;
    case ec_cl_instantiation_option_only_in_cplusplus:
      m = "instantiation mode (-t) can be used only when compiling C++";
      break;
    case ec_cl_auto_instantiation_option_only_in_cplusplus:
      m =
       "automatic instantiation mode (-T) can be used only when compiling C++";
      break;
    case ec_cl_implicit_inclusion_option_only_in_cplusplus:
      m =
   "implicit template inclusion mode (-B) can be used only when compiling C++";
      break;
    case ec_cl_exceptions_option_only_in_cplusplus:
      m = "exception handling option (-x) can be used only when compiling C++";
      break;
    case ec_cl_strict_ansi_incompatible_with_pcc:
      m = "strict ANSI mode is incompatible with K&R mode";
      break;
    case ec_cl_strict_ansi_incompatible_with_cfront:
      m = "strict ANSI mode is incompatible with cfront mode";
      break;
    case ec_cl_missing_source_file_name:
      m = "missing source file name";
      break;
    case ec_cl_output_file_incompatible_with_multiple_inputs:
      m =
        "output files may not be specified when compiling several input files";
      break;
    case ec_cl_too_many_arguments:
      m = "too many arguments on command line";
      break;
    case ec_cl_no_output_file_needed:
      m = "-o was specified, but no output file is needed";
      break;
    case ec_cl_il_display_requires_il_file_name:
      m = "IL display requires name of IL file";
      break;
    case ec_void_template_parameter:
      m = "a template parameter may not have void type";
      break;
    case ec_too_many_unused_instantiations:
      m =
        "excessive recursive instantiation of %n due to instantiate-all mode";
      break;
    case ec_cl_strict_ansi_incompatible_with_anachronisms:
      m = "strict ANSI mode is incompatible with allowing anachronisms";
      break;
    case ec_void_throw:
      m = "a throw expression may not have void type";
      break;
    case ec_cl_tim_local_conflicts_with_auto_instantiation:
      m = "-tlocal mode is incompatible with automatic instantiation";
      break;
    case ec_abstract_class_param_type:
      m = "parameter of abstract class type is not allowed";
      break;
    case ec_array_of_abstract_class:
      m = "array of abstract class is not allowed";
      break;
    case ec_float_template_parameter:
      m = "floating-point template parameter is nonstandard";
      break;
    case ec_pragma_must_precede_declaration:
      m = "this pragma must immediately precede a declaration";
      break;
    case ec_pragma_must_precede_statement:
      m = "this pragma must immediately precede a statement"; 
      break;
      /* +++ -- For ease of finding the insert point for new diagnostics. */
    case ec_no_error:
    default:
#if CHECKING
      internal_error("error_text: unknown error code");
#else /* CHECKING */
      m = "unknown error";
#endif /* CHECKING */
  }  /* switch */
  return (m);
}  /* error_text */

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


static void form_int_kind_name(an_integer_kind   kind,
                               a_msg_segment_ptr seg_ptr)
/*
Add the name of the integer type to the message segment string being formatted.
*/
{
  char *s;

  switch (kind) {
    case ik_char:           s = "char";             break;
    case ik_signed_char:    s = "signed char";      break;
    case ik_unsigned_char:  s = "unsigned char";    break;
    case ik_short:          s = "short";            break;
    case ik_unsigned_short: s = "unsigned short";   break;
    case ik_int:            s = "int";              break;
    case ik_unsigned_int:   s = "unsigned int";     break;
    case ik_long:           s = "long";             break;
    case ik_unsigned_long:  s = "unsigned long";    break;
#if LONG_LONG_ALLOWED
    case ik_long_long:      s = "long long";        break;
    case ik_unsigned_long_long:
                            s = "unsigned long long";
                                                    break;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
    default:
      internal_error("form_int_kind_name: bad integer kind");
#endif /* CHECKING */
  }  /* switch */
  add_string_to_segment(s, seg_ptr);
}  /* form_int_kind_name */


static void form_float_kind_name(a_float_kind      kind,
                                 a_msg_segment_ptr seg_ptr)
/*
Add the name of the floating point type to the type string being formatted.
*/
{
  char *s;

  switch (kind) {
    case fk_float:       s = "float";             break;
    case fk_double:      s = "double";            break;
    case fk_long_double: s = "long double";       break;
#if CHECKING
    default:
      internal_error("form_float_kind|name: bad float kind");
#endif /* CHECKING */
  }  /* switch */
  add_string_to_segment(s, seg_ptr);
}  /* form_float_kind_name */


static void form_type_qualifier(a_type_ptr        tp,
                                a_boolean         postpositional,
                                a_msg_segment_ptr seg_ptr)
/*
Add a type qualifier to the type string being formatted at the position
indicated by type_string_ptr.  type_string_ptr is then incremented by
the length of the type qualifier added.
*/
{
  a_boolean is_const = FALSE,
            is_volatile = FALSE;

  for (; tp->kind == (a_type_kind)tk_typeref; tp = tp->variant.typeref.type) {
    if (tp->variant.typeref.is_const || tp->variant.typeref.is_volatile) {
      if (tp->variant.typeref.is_const) is_const = TRUE;
      if (tp->variant.typeref.is_volatile) is_volatile = TRUE;
    } else {
      /* A typedef name.  Stop here. */
      break;
    }  /* if */
  }  /* for */
  if (is_const || is_volatile) {
    if (postpositional) add_string_to_segment(" ", seg_ptr);
    if (is_const) {
      add_string_to_segment("const", seg_ptr);
      if (is_volatile) add_string_to_segment(" ", seg_ptr);
    }  /* if */
    if (is_volatile) add_string_to_segment("volatile", seg_ptr);
    if (!postpositional) add_string_to_segment(" ", seg_ptr);
  }  /* if */
}  /* form_type_qualifier */


/* Forward declarations for recursive calls. */
static void form_constant(a_constant_ptr     cp,
                          a_msg_segment_ptr  seg_ptr);
static void form_class_qualifier(a_type_ptr        type,
                                 a_msg_segment_ptr seg_ptr);
static void form_type_name(a_type_ptr        type,
                           a_msg_segment_ptr seg_ptr);
static void form_type(a_type_ptr        type,
                      a_boolean         need_parens,
                      a_msg_segment_ptr seg_ptr);


static void form_template_param_constant_expr(an_expr_node_ptr   node,
                                              a_msg_segment_ptr  seg_ptr)
/*
Add a string representing a template param constant expression to a string
being formed.
*/
{
  an_expr_node_ptr         op1, op2;
  an_expr_operator_kind    op_kind;
  char                     *s;
  a_source_correspondence  *scp;

  check_assertion(node != NULL);
  switch (node->kind) {
    case enk_operation:
      op1 = node->variant.operation.operands;
      op2 = op1->next;
      op_kind = node->variant.operation.kind;
      switch (op_kind) {
        case eok_iadd:
        case eok_fadd:
        case eok_padd:         s = "+";  break;
        case eok_inegate:
        case eok_fnegate:
        case eok_isubtract:
        case eok_fsubtract:
        case eok_psubtract:    s = "-";  break;
        case eok_imultiply:
        case eok_fmultiply:    s = "*";  break;
        case eok_idivide:
        case eok_fdivide:      s = "/";  break;
        default:               s = NULL;
      }  /* switch */
      if (s == NULL ||
          op1->kind == (an_expr_node_kind)enk_operation ||
          (op2 != NULL && op2->kind == (an_expr_node_kind)enk_operation)) {
        /* Only display a limited set of operators and only simple unary or
           binary expressions (op-leaf or leaf-op-leaf). */
        add_string_to_segment("<expression>", seg_ptr);
      } else if (op2 == NULL) {
        /* Unary operation. */
        check_assertion(op_kind == (an_expr_operator_kind)eok_inegate);
        add_string_to_segment("-", seg_ptr);
        form_template_param_constant_expr(op1, seg_ptr);
      } else {
        /* Binary operation. */
        check_assertion(op2->next == NULL);
        form_template_param_constant_expr(op1, seg_ptr);
        add_string_to_segment(s, seg_ptr);
        form_template_param_constant_expr(op2, seg_ptr);
      }  /* if */
      break;
    case enk_constant:
      form_constant(node->variant.constant, seg_ptr);
      break;
    case enk_variable_address:
      scp = &node->variant.variable->source_corresp;
      goto display_var_or_routine_address;
    case enk_routine_address:
      scp = &node->variant.routine->source_corresp;
display_var_or_routine_address:
      add_string_to_segment("&", seg_ptr);
      form_class_qualifier(scp->class_of_which_a_member, seg_ptr);
      add_string_to_segment(scp->name, seg_ptr);
      break;
    case enk_error:
      add_string_to_segment("<error>", seg_ptr);
      break;
#if CHECKING
    default:
      internal_error("form_template_param_constant_expr: bad expr kind");
#endif /* CHECKING */
  }  /* switch */
}  /* form_template_param_constant_expr */


static void form_constant(a_constant_ptr     cp,
                          a_msg_segment_ptr  seg_ptr)
/*
Add a string representing a constant value to a string being formed.
*/
{
#define LOCAL_BUFFER_LEN 30
  char                     buffer[LOCAL_BUFFER_LEN], *p_char;
  a_source_correspondence  *scp;
  int                      i;
  a_targ_size_t            count;
  a_float_kind             fkind;

  if (cp->implicit_cast) {
    /* If the constant is implicitly cast, put out a cast. */
    add_string_to_segment("(", seg_ptr);
    form_type(cp->type, /*need_parens=*/FALSE, seg_ptr);
    add_string_to_segment(")", seg_ptr);
  }  /* if */
  switch (cp->kind) {
    case ck_integer:
      add_string_to_segment(str_for_integer_constant(cp), seg_ptr);
      break;
    case ck_string:
      /* Opening quote. */
      buffer[0] = '"';
      i = 1;
      /* Use the indicated character count to cycle through the string
         constant, since there may be embedded NULLs. */
      count = cp->variant.string.length;
      for (p_char = cp->variant.string.value; --count > 0; ++p_char) {
        if (count == 0 && *p_char == 0) {
          /* This is the terminating NULL in the string -- we don't want to
             display it. */
          break;
        } else if (*p_char == '"') {
          buffer[i++] = '\\';
          buffer[i++] = '"';
        } else if (isprint((unsigned char)*p_char)) {
          buffer[i++] = *p_char;
        } else {
          /* A nonprintable character.  Use the language defined escape
             sequence if appropriate, and otherwise an octal value. */
          char c;

          switch (*p_char) {
            case TARG_ALERT_CHAR:       c = 'a'; break;
            case TARG_BACKSPACE_CHAR:   c = 'b'; break;
            case TARG_FORM_FEED_CHAR:   c = 'f'; break;
            case TARG_NEWLINE_CHAR:     c = 'n'; break;
            case TARG_CARR_RETURN_CHAR: c = 'r'; break;
            case TARG_HORIZ_TAB_CHAR:   c = 't'; break;
            case TARG_VERT_TAB_CHAR:    c = 'v'; break;
            /* Default case:  no escape sequence is defined. */
            default:                    c = 0;
          }  /* switch */
          buffer[i++] = '\\';
          if (c != 0) {
            /* Print the escaped value. */
            buffer[i++] = c;
          } else {
            /* Print non-printable character in octal form.  Truncate
               to right number of bits to avoid problems with signed chars. */
            sprintf(&buffer[i], "%03o",
                    (unsigned int)(*p_char & ((1<<targ_char_bit)-1)));
            i += 3;
          }  /* if */
        }  /* if */
        /* We'll only put out part of the string if it's too long. */
        if (i > LOCAL_BUFFER_LEN-10 && count > 3) {
          sprintf(&buffer[i], "...");
          i += 3;
          break;
        }  /* if */
      }  /* for */
      /* Terminating quote plus NULL character. */
      buffer[i++] = '"';
      buffer[i] = 0;
      add_string_to_segment(buffer, seg_ptr);
      break;
    case ck_float:
      fkind = skip_typerefs(cp->type)->variant.float_kind;
      add_string_to_segment(fp_to_string(fkind, &cp->variant.float_value),
                            seg_ptr);
      break;
    case ck_address:
      if (cp->variant.address.kind == (an_address_base_kind)abk_constant) {
        form_constant(cp->variant.address.variant.constant, seg_ptr);
      } else {
        add_string_to_segment("&", seg_ptr);
        if (cp->variant.address.kind == (an_address_base_kind)abk_routine) {
          scp = &cp->variant.address.variant.routine->source_corresp;
#if CHECKING
        } else if (cp->variant.address.kind !=
                                       (an_address_base_kind)abk_variable) {
          internal_error("form_constant: bad address constant kind");
#endif /* CHECKING */
        } else {
          scp = &cp->variant.address.variant.variable->source_corresp;
        }  /* if */
        form_class_qualifier(scp->class_of_which_a_member, seg_ptr);
        add_string_to_segment(scp->name, seg_ptr);
      }  /* if */
      break;
    case ck_ptr_to_member:
      /* C++ pointer-to-member. */
      scp = NULL;
      if (cp->variant.ptr_to_member.is_function_ptr) {
        a_routine_ptr rp = cp->variant.ptr_to_member.variant.routine;
        if (rp != NULL) scp = &rp->source_corresp;
      } else {
        a_field_ptr fp = cp->variant.ptr_to_member.variant.field;
        if (fp != NULL) scp = &fp->source_corresp;
      }  /* if */
      if (scp == NULL) {
        /* NULL pointer-to-member constant.  Note that implicit_cast will
           be set, so the type will have been printed out above. */
        add_string_to_segment("0", seg_ptr);
      } else {
        add_string_to_segment("&", seg_ptr);
        form_class_qualifier(scp->class_of_which_a_member, seg_ptr);
        add_string_to_segment(scp->name, seg_ptr);
      }  /* if */
      break;
    case ck_template_param:
      switch (cp->variant.template_param.kind) {
        case tpck_member:
          check_assertion(cp->source_corresp.class_of_which_a_member != NULL);
          form_class_qualifier(cp->source_corresp.class_of_which_a_member,
                               seg_ptr);
          /* Fall through to display the rest of the qualified name. */
        case tpck_param:
          add_string_to_segment(cp->source_corresp.name, seg_ptr);
          break;
        case tpck_expression:
          form_template_param_constant_expr(cp->variant.template_param.
                                                                variant.expr,
                                            seg_ptr);
          break;
#if CHECKING
        default:
          internal_error("form_constant: bad template param constant kind");
#else /* CHECKING */
          add_string_to_segment("<unknown constant>", seg_ptr);
#endif /* CHECKING */
      }  /* switch */
      break;
    case ck_error:
      add_string_to_segment("<error constant>", seg_ptr);
      break;
    case ck_aggregate:
    default:
#if CHECKING
      internal_error("form_constant: bad constant kind");
#else /* CHECKING */
      add_string_to_segment("<unknown constant>", seg_ptr);
#endif /* CHECKING */
  }  /* switch */
#undef LOCAL_BUFFER_LEN
}  /* form_constant */


static void form_type_specifier(a_type_ptr        type,
                                a_msg_segment_ptr seg_ptr)
/*
Add the type specifier to the type string being formed.
*/
{
  char	*s = NULL;
  char	*tag_kind = NULL;

  switch (type->kind) {
    case tk_error:
      s = "<error type>";
      break;
    case tk_unknown:
      s = "<unknown>";
      break;
    case tk_void:
      s = "void";
      break;
    case tk_integer:
      if (type->variant.integer.enum_type) {
        tag_kind = "enum ";
        goto do_tag_name;
      }  /* if */
      form_int_kind_name(type->variant.integer.int_kind, seg_ptr);
      break;
    case tk_float:
      form_float_kind_name(type->variant.float_kind, seg_ptr);
      break;
    case tk_struct:
      tag_kind = "struct ";
      goto do_tag_name;
    case tk_union:
      tag_kind = "union ";
      goto do_tag_name;
    case tk_class:
      tag_kind = "class ";
do_tag_name:
      /* For C++, do not generate the tag kind for named types. */
      if (type->source_corresp.name == NULL ||
          C_dialect != C_dialect_cplusplus) {
        add_string_to_segment(tag_kind, seg_ptr);
      }  /* if */
      form_class_qualifier(type->source_corresp.class_of_which_a_member,
                           seg_ptr);
      form_type_name(type, seg_ptr);
      break;
    case tk_typeref:
      /* Assume that the caller has stripped off type qualifiers -- this
         tk_typeref represents a typedef name. */
      check_assertion(!type->variant.typeref.is_const &&
                      !type->variant.typeref.is_volatile);
      form_class_qualifier(type->source_corresp.class_of_which_a_member,
                           seg_ptr);
      form_type_name(type, seg_ptr);
      break;
    case tk_template_param:
      form_class_qualifier(type->source_corresp.class_of_which_a_member,
                           seg_ptr);
      form_type_name(type, seg_ptr);
      break;
#if CHECKING
    default:
      /* Note that certain type kinds are handled by form_type_first_part
         and form_type_second_part and shouldn't get here. */
      internal_error("form_type_specifier: bad type specifier kind");
#endif /* CHECKING */
  }  /* switch */
  if (s != NULL) add_string_to_segment(s, seg_ptr);
}  /* form_type_specifier */


static a_type_ptr unqualified_display_type(a_type_ptr type)
/*
Drop type qualifiers but not typedefs from the indicated type and return
what's left.  This is used to remove type qualifiers to get to the underlying
type for display purposes.
*/
{
  /* Drop type qualifiers but stop on a typedef. */
  while (type->kind == (a_type_kind)tk_typeref &&
         type->source_corresp.name == NULL) {
    type = type->variant.typeref.type;
  }  /* while */
  return type;
}  /* unqualified_display_type */


static void form_type_first_part(a_type_ptr        type,
                                 a_boolean         need_parens,
                                 a_msg_segment_ptr seg_ptr)
/*
Add the first of possibly two parts of a type reference.
*/
{
  a_type_ptr local_type, unqualified_type;

  /* Drop type qualifiers but not typedefs. */
  unqualified_type = unqualified_display_type(type);
  if (unqualified_type->kind == (a_type_kind)tk_pointer) {
    /* Pointer or reference type. */
    local_type = unqualified_type->variant.pointer.type;
    /* Recursive call to print out any lower indirections. */
    form_type_first_part(local_type,
                         /*need_parens=*/
                            !is_pointer_or_reference_type(local_type) &&
                            skip_typerefs(local_type)->kind !=
                                       (a_type_kind)tk_ptr_to_member,
                         seg_ptr);
    if (skip_typerefs(local_type)->kind == (a_type_kind)tk_ptr_to_member) {
      add_string_to_segment(" ", seg_ptr);
    }  /* if */
    /* Print out the star for this indirection. */
    if (unqualified_type->variant.pointer.is_reference) {
      /* This is a C++ reference type */
      add_string_to_segment("&", seg_ptr);
    } else {
      add_string_to_segment("*", seg_ptr);
    }  /* if */
    form_type_qualifier(type, /*postpositional=*/TRUE, seg_ptr);
    if (need_parens) add_string_to_segment("(", seg_ptr);
  } else if (unqualified_type->kind == (a_type_kind)tk_array) {
    /* Array type. */
    local_type = unqualified_type->variant.array.element_type;
    form_type_first_part(local_type,
                         /*need_parens=*/
                         (local_type->kind != (a_type_kind)tk_array &&
                          !is_pointer_or_reference_type(local_type)),
                         seg_ptr);
    if (need_parens) add_string_to_segment("(", seg_ptr);
  } else if (unqualified_type->kind == (a_type_kind)tk_ptr_to_member) {
    /* C++ pointer to member type. */
    form_type_first_part(unqualified_type->variant.ptr_to_member.type,
                         /*needs_parens=*/TRUE, seg_ptr);
    form_class_qualifier(unqualified_type->
                            variant.ptr_to_member.class_of_which_a_member,
                         seg_ptr);
    add_string_to_segment("*", seg_ptr);
    form_type_qualifier(type, /*postpositional=*/TRUE, seg_ptr);
    if (need_parens) add_string_to_segment("(", seg_ptr);
  } else if (unqualified_type->kind == (a_type_kind)tk_routine) {
    /* Function type. */
    local_type = unqualified_type->variant.routine.return_type;
    form_type_first_part(local_type,
                         /*need_parens=*/
                         ! is_pointer_or_reference_type(local_type),
                         seg_ptr);
    if (need_parens) add_string_to_segment("(", seg_ptr);
  } else {
    form_type_qualifier(type, /*postpositional=*/FALSE, seg_ptr);
    form_type_specifier(unqualified_type, seg_ptr);
    if (need_parens) add_string_to_segment(" ", seg_ptr);
  }  /* if */
}  /* form_type_first_part */


static void form_type_second_part(a_type_ptr        type,
                                  a_boolean         need_parens,
                                  a_msg_segment_ptr seg_ptr)
/*
Add the second part of a type reference to the type string being formed.
If it's a pointer, just continue to look for the base type.  If it's an
array, print out the dimension information.
*/
{
  a_type_ptr local_type;

  /* Drop type qualifiers but not typedefs. */
  type = unqualified_display_type(type);
  if (type->kind == (a_type_kind)tk_pointer) {
    local_type = type->variant.pointer.type;
    if (need_parens) add_string_to_segment(")", seg_ptr);
    form_type_second_part(local_type,  /*need_parens=*/
                          !is_pointer_or_reference_type(local_type),
                          seg_ptr);
  } else if (type->kind == (a_type_kind)tk_array) {
    /* Array type. */
    if (need_parens) add_string_to_segment(")", seg_ptr);
    if (type->variant.array.is_variable_size_array) {
#if 0
      /* THIS IS TEMPORARY AND SHOULD BE IMPROVED. */
#endif /* if 0 */
      add_string_to_segment("[<expr>]", seg_ptr);
    } else if (type->variant.array.variant.number_of_elements == 0) {
      add_string_to_segment("[]", seg_ptr);
    } else {
      char	buffer[BASE_MSG_SEGMENT_SIZE];
#if CHECKING
      if (digits_to_represent(
                (unsigned long)type->variant.array.variant.number_of_elements)
                            >= BASE_MSG_SEGMENT_SIZE) {
        internal_error
              ("form_type_second_part: tk_array: buffer size too small");
      }  /* if */
#endif /* CHECKING */
      (void)sprintf(buffer, "[%lu]",
                    (unsigned long)type->
                                     variant.array.variant.number_of_elements);
      add_string_to_segment(buffer, seg_ptr);
    }  /* if */
    local_type = type->variant.array.element_type;
    form_type_second_part(local_type,
                          /*need_parens=*/
                          (local_type->kind != (a_type_kind)tk_array &&
                           !is_pointer_or_reference_type(local_type)),
                          seg_ptr);
  } else if (type->kind == (a_type_kind)tk_ptr_to_member) {
    /* C++ pointer to member type. */
    if (need_parens) add_string_to_segment(")", seg_ptr);
    form_type_second_part(type->variant.ptr_to_member.type,
                          /*needs_parens=*/TRUE, seg_ptr);
  } else if (type->kind == (a_type_kind)tk_routine) {
    /* Function type. */
    local_type = type->variant.routine.return_type;
    if (need_parens) add_string_to_segment(")", seg_ptr);
    form_param_list(type->variant.routine.extra_info, seg_ptr);
    form_type_second_part(local_type,
                          /*need_parens=*/
                          !is_pointer_or_reference_type(local_type),
                          seg_ptr);
  }  /* if */
}  /* form_type_second_part */


static void form_type(a_type_ptr        type,
                      a_boolean         need_parens,
                      a_msg_segment_ptr seg_ptr)
/*
Add a type to the string being formatted.
*/
{
  form_type_first_part(type, need_parens, seg_ptr);
  form_type_second_part(type, need_parens, seg_ptr);
}  /* form_type */


static void form_implicit_this_qualifiers(a_type_ptr        implicit_this_type,
                                          a_msg_segment_ptr seg_ptr)
/*
Add any qualifiers associated with the implicit this parameter to
the type string being formatted.
*/
{
  a_type_ptr	type = skip_typerefs(implicit_this_type);
  if (type->kind == (a_type_kind)tk_pointer &&
      (type = type->variant.pointer.type)->kind == (a_type_kind)tk_typeref) {
    if (type->variant.typeref.is_const) {
      add_string_to_segment(" const", seg_ptr);
    }  /* if */
    if (type->variant.typeref.is_volatile) {
      add_string_to_segment(" volatile", seg_ptr);
    }  /* if */
  }  /* if */
}  /* form_implicit_this_qualifiers */


static void form_param_list(a_routine_type_supplement_ptr suppl_ptr,
                            a_msg_segment_ptr             seg_ptr)
/*
Add the parameter list of a function to the type string being formatted.
*/
{
  a_param_type_ptr	param_ptr;
  a_type_ptr		type;
  a_boolean		has_ellipsis;

  add_string_to_segment("(", seg_ptr);
  if (suppl_ptr->prototyped || suppl_ptr->old_style_params_scanned) {
    has_ellipsis = suppl_ptr->has_ellipsis;
    for (param_ptr = suppl_ptr->param_type_list;
         param_ptr != NULL;
         param_ptr = param_ptr->next) {
      form_type(param_ptr->type, /*need_parens=*/FALSE, seg_ptr);
      if (param_ptr->next != NULL || has_ellipsis) {
        add_string_to_segment(", ", seg_ptr);
      }  /* if */
    }  /* for */
    if (has_ellipsis) {
      add_string_to_segment("...", seg_ptr);
    }  /* if */
  }  /* if */
  add_string_to_segment(")", seg_ptr);
#ifdef CFE
  /* Check if this is a "const" or "volatile" member function by looking at
     the type of the implicit "this" parameter. */
  if ((type = suppl_ptr->implicit_this_param_type) != NULL) {
    form_implicit_this_qualifiers(type, seg_ptr);
  }  /* if */
#endif /* ifdef CFE */
}  /* form_param_list */


static void form_template_args(a_template_arg_ptr  template_arg,
                               a_msg_segment_ptr   seg_ptr)
/*
Add to the message string a comma separated list of template arguments,
surrounded by "<" and ">".
*/
{
  if (template_arg != NULL) {
    /* One or more template arguments.  First put out the "<", then loop
       through the arguments. */
    add_string_to_segment("<", seg_ptr);
    for (;;) {
      if (template_arg->is_type) {
        /* Type argument. */
        form_type(template_arg->variant.type, /*need_parens=*/FALSE, seg_ptr);
      } else {
        /* Constant argument */
        form_constant(template_arg->variant.constant, seg_ptr);
      }  /* if */
      /* Advance to the next argument.  If it's NULL, put out the terminating
         ">"; otherwise, put out a comma and continue looping. */
      template_arg = template_arg->next;
      if (template_arg == NULL) {
        add_string_to_segment(">", seg_ptr);
        break;
      } else {
        add_string_to_segment(",", seg_ptr);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* form_template_args */


static void form_type_name(a_type_ptr        type,
                           a_msg_segment_ptr seg_ptr)
/*
Add the name of a type to the message segment being constructed
at *seg_ptr.  Use "<unnamed>" if the type has no user name.  This is
used for classes and enums.
*/
{
  char                         *s;
  a_class_type_supplement_ptr  ctsp;

  if (type->source_corresp.name != NULL) {
    s = type->source_corresp.name;
  } else {
    s = "<unnamed>";
  }  /* if */
  add_string_to_segment(s, seg_ptr);
  /* If a class is an instantiation of a class template, put out the
     template arguments. */
  /* We do not use is_immediate_class_type here to avoid standalone
     program problems. */
  if (type->kind == (a_type_kind)tk_class  ||
      type->kind == (a_type_kind)tk_struct ||
      type->kind == (a_type_kind)tk_union) {
    ctsp = type->variant.class_struct_union.extra_info;
    if (ctsp != NULL) form_template_args(ctsp->template_arg_list, seg_ptr);
  }  /* if */
}  /* form_type_name */


static void form_class_qualifier(a_type_ptr        type,
                                 a_msg_segment_ptr seg_ptr)
/*
Add the class name of the specified type followed by "::" to the message
segment being constructed at *seg_ptr.
*/
{
  if (type != NULL && C_dialect == C_dialect_cplusplus) {
    /* Check for nested classes. */
    if (type->source_corresp.class_of_which_a_member != NULL) {
      form_class_qualifier(type->source_corresp.class_of_which_a_member,
                           seg_ptr);
    }  /* if */
    form_type_name(type, seg_ptr);
    add_string_to_segment("::", seg_ptr);
  }  /* if */
}  /* form_class_qualifier */


static void form_type_summary(a_type_ptr        tp,
                              a_msg_segment_ptr seg_ptr)
/*
Format a string that represents the type pointed to by tp into the message
segment described by *seg_ptr.
*/
{
  add_string_to_segment("\"", seg_ptr);
  seg_ptr->first_quote = seg_ptr->segment + seg_ptr->length - 1;
  form_type(tp, /*need_parens=*/FALSE, seg_ptr);
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
  form_type(tp, /*need_parens=*/FALSE, curr_segment);
  /* Provide the length of the string and the address of the string
     buffer to the caller. */
  *len_ptr = curr_segment->length;
  return curr_segment->segment;
}  /* format_type_string */

#if !STANDALONE_UTILITY_PROGRAM

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
        internal_error("form_bound: buffer size too small");
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


static a_boolean is_overloaded_function(a_symbol_ptr sym)
/*
Run through the linked list of active and inactive symbols chained from the
symbol header of the specified symbol looking for that symbol.  If found, the
symbol cannot be an overloaded function.  An overloaded function will be
represented in these lists of symbols as a symbol of sk_overloaded_function
kind with this specific function symbol being part of the list chained from
that overloaded function symbol entry.
*/
{
  a_boolean	is_overloaded = TRUE;	/* Assume and try to disprove. */
  a_symbol_ptr	wrk_sym;

  /* Check through the current list of symbols. */
  for (wrk_sym = sym->header->symbol;
       wrk_sym != NULL;
       wrk_sym = wrk_sym->next) {
    if (wrk_sym == sym) {
      /* The symbol is in the header's linked list of symbols and therefore
         is not an overloaded function. */
      is_overloaded = FALSE;
      goto return_point;
    }  /* if */
  }  /* for */
  /* Check through the inactive symbol list. */
  for (wrk_sym = sym->header->inactive_symbols;
       wrk_sym != NULL;
       wrk_sym = wrk_sym->next) {
    if (wrk_sym == sym) {
      /* The symbol is in the header's linked list of inactive symbols and
         therefore is not an overloaded function. */
      is_overloaded = FALSE;
      goto return_point;
    }  /* if */
  }  /* for */
return_point:
  return is_overloaded;
}  /* is_overloaded_function */


static void form_symbol_name(a_symbol_ptr        sym,
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
  char		*entity_kind;
  a_boolean	is_constructor = FALSE;
  a_boolean	is_destructor = FALSE;
  a_boolean	is_overloaded = FALSE;
  a_boolean	is_conversion = FALSE;
  a_boolean	is_declaration_like = FALSE;

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
      if (fund_sym->variant.type->kind == (a_type_kind)tk_template_param) {
        entity_kind = "template parameter ";
      } else {
        entity_kind = "type ";
      }  /* if */
      goto symbol_name;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      if (C_dialect == C_dialect_cplusplus &&
          fund_sym->variant.class_struct_union.extra_info->is_nonreal_class) {
        /* This is a symbol for a prototype instantiation of a class template.
           It is preferable to display "class template X<T>" instead of
           "class X<T>", so fall through to code for sk_class_template. */
      } else {
        if (fund_sym->kind == (a_symbol_kind)sk_union_tag) {
          entity_kind = "union ";
        } else if (C_dialect == C_dialect_cplusplus) {
          entity_kind = "class ";
        } else {
          entity_kind = "struct ";
        }  /* if */
        goto symbol_name;
      }  /* if */
    case sk_class_template:
      entity_kind = "class template ";
      goto symbol_name;
    case sk_enum_tag:
      entity_kind = "enum ";
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
      entity_kind = "constant ";
      goto symbol_name;
    case sk_routine:
    case sk_member_function:
      type = routine_symbol_type(fund_sym);
      routine = fund_sym->variant.routine.ptr;
      entity_kind = "function ";
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_extern_routine:
      type = fund_sym->variant.extern_symbol_descr->type;
      routine = fund_sym->variant.extern_symbol_descr->variant.routine;
      entity_kind = "function ";
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_overloaded_function:
      entity_kind = "overloaded function ";
      /* There is no specific type information available; this entity cannot
         be expressed as a declaration. */
      goto symbol_name;
    case sk_static_data_member:
      type = fund_sym->variant.static_data_member.variable->type;
      entity_kind = "member ";
      is_declaration_like = TRUE;
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
    case sk_function_template:
      entity_kind = "function template ";
      routine = fund_sym->variant.template_info->variant.function.routine;
      type = routine->type;
      goto symbol_name;
symbol_name:
      /* Add the entity kind if not specified as name only or full type for
         a declaration like entity. */
      if (type == NULL) is_declaration_like = FALSE;
      if (! seg_ptr->variant.symbol.name_only &&
          ! (seg_ptr->variant.symbol.full_type && is_declaration_like) ) {
        add_string_to_segment(entity_kind, seg_ptr);
      } /* if */
      /* Add the beginning double quote. */
      add_string_to_segment("\"", seg_ptr);
      seg_ptr->first_quote = seg_ptr->segment + seg_ptr->length - 1;
      /* Check if this is a C++ constructor, destructor or conversion 
         routine. */
      if (routine != NULL) {
        is_constructor = is_constructor_symbol(fund_sym);
        is_destructor = is_destructor_symbol(fund_sym);
        is_overloaded = is_overloaded_function(fund_sym);
        is_conversion = routine->special_kind ==
                          (a_special_function_kind)sfk_conversion;
      }  /* if */
      if (seg_ptr->variant.symbol.full_type && 
          type != NULL &&
          ! is_constructor &&
          ! is_destructor &&
          ! is_conversion ) {
        form_type_first_part(type, /*need_parens=*/FALSE, seg_ptr);
        if (seg_ptr->segment[seg_ptr->length - 1] != ' ' &&
            seg_ptr->segment[seg_ptr->length - 1] != '*' &&
            seg_ptr->segment[seg_ptr->length - 1] != '(') {
          add_string_to_segment(" ", seg_ptr);
        }  /* if */
      }  /* if */
      form_class_qualifier(sym->class_of_which_a_member, seg_ptr);
      if (sym->kind == (a_symbol_kind)sk_class_or_struct_tag ||
          sym->kind == (a_symbol_kind)sk_union_tag) {
        /* Put out name plus template args where appropriate. */
        form_type_name(sym->variant.class_struct_union.type, seg_ptr);
      } else {
        /* Use the name in the header. */
        add_string_to_segment(sym->header->identifier, seg_ptr);
      }  /* if */
      if (type != NULL &&
          !seg_ptr->variant.symbol.name_only &&
          (seg_ptr->variant.symbol.full_type || is_overloaded) ) {
        if (is_conversion ||
            (C_dialect != C_dialect_cplusplus && is_function_type(type) &&
             !type->variant.routine.extra_info->prototyped)) {
          /* This is either a conversion function for which there is no
             parameter list or in C-mode a function with an old-style
             declaration, in which case the param type list is accidental. */
          add_string_to_segment("()", seg_ptr);
          if (is_conversion) {
            /* For conversion functions, add any qualifiers that may be
               present. */
            a_type_ptr	this_param_type = type->variant.routine.extra_info->
                                                      implicit_this_param_type;
            if (this_param_type != NULL) {
              form_implicit_this_qualifiers(this_param_type, seg_ptr);
            }  /* if */
          }  /* if */
        }  else {
          form_type_second_part(type, /*need_parens=*/FALSE, seg_ptr);
        }  /* if */
      }  /* if */
      break;
#if CHECKING
    case sk_projection:
      /* Cannot have a projection of a projection symbol.  This is an
         error. */
      internal_error("form_symbol_name: projection of projection kind");
      break;
    default:
      internal_error("form_symbol_name: unsupported symbol kind");
#endif /* CHECKING */
  }  /* switch */
  /* Add the closing double quote mark. */
  add_string_to_segment("\"", seg_ptr);
  seg_ptr->second_quote = seg_ptr->segment + seg_ptr->length - 1;

  if (seg_ptr->variant.symbol.template_args) {
    a_scope_stack_entry_ptr  ssep = error_msg_scopes[seg_ptr->sequence_no];
    check_assertion(sym->kind == (a_symbol_kind)sk_function_template ||
                    sym->kind == (a_symbol_kind)sk_class_template);
    check_assertion(ssep != NULL && ssep->template_arg_list != NULL);
    add_string_to_segment(" based on template argument", seg_ptr);
    if (ssep->template_arg_list->next != NULL) {
      add_string_to_segment("s", seg_ptr);
    }  /* if */
    add_string_to_segment(" ", seg_ptr);

    form_template_args(ssep->template_arg_list, seg_ptr);
  }  /* if */
  /* Add the declaration position as requested. */
  if (seg_ptr->variant.symbol.decl_pos) {
    form_source_position(&sym->decl_position, error_pos, " (declared ", ")",
                         "(at end of source)", seg_ptr);
  }  /* if */
}  /* form_symbol_name */

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

Symbol name expansions may have a declaration position modifier"d" which
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
          curr_segment->variant.symbol.decl_pos = FALSE;
          curr_segment->variant.symbol.template_args = FALSE;
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
#endif /* STANDALONE_UTILITY_PROGRAM */
check_for_seq_number:
          curr_segment->sequence_no = 1;
          if (isdigit((unsigned char)*msg_ptr)) {
            i = (unsigned)*msg_ptr - (unsigned)'0';
            if (i > 0 && i <= INCR_MSG_SEGMENT_SIZE) {
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

void error_init(void)
/*
Perform any initializations necessary for error.c functions at the beginning
of each compilation.
*/
{
  clear_file_index_list();
}  /* error_init */

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
    spacing = curr_file->line_number[mid_index] / mid_index;
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
      fprintf(f_debug, "entry %0d=%5lu\n", index,
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
  char              ch;
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
        /* Add the character to the buffer. */
        *loc_in_line++ = ch;
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
    if (!pass_for_caret || (out_char) == '\t') {                      \
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
      if (line_olmp->kind == olm_line_splice) break;
    }  /* for */
    seq++;
    line_start = line_olmp->line_loc;
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
          /* Exit on the newline at the end of the source line. */
          if (ch == '\n') goto end_of_loop;
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
            /* If the trigraph is "??/", which turns into "\", and it's at the
               end of a line, the "\" will indicate a line splice.  In that
               case, the "\" for the line splice should not be put out. */
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
            /* Exit the loop since this line splice marks the end of the
               physical line. */
            goto end_of_loop;
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
       kind) */
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
    error_msg_positions[i] = NULL;
#if !STANDALONE_UTILITY_PROGRAM
    error_msg_syms[i] = NULL;
    error_msg_scopes[i] = NULL;
#endif /* !STANDALONE_UTILITY_PROGRAM */
  }  /* for */
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
    }  /* switch */
    start_of_message = FALSE;
  }  /* for */
  putc('\n', file);
}  /* write_message */


static void write_position_and_severity(an_error_severity severity,
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
        *line_len += fprintf(stderr, "\"%s\", line %lu", *file_name,
                                                          *line_number);
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
      severity_string = "remark: ";
      total_remarks++;
      break;
    case es_warning:
      severity_string = "warning: ";
      total_warnings++;
      break;
    case es_error:
#if ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES
      severity_string = "error: ";
#else /* ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES */
      severity_string = "";
#endif /* ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES */
      total_errors++;
      break;
    case es_catastrophe:
      severity_string = "catastrophic error: ";
      total_catastrophes++;
      break;
    case es_command_line_error:
      severity_string = "command-line error: ";
      total_catastrophes++;
      break;
    case es_internal_error:
      severity_string = "internal error: ";
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
}  /* write_position_and_severity */


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


static void write_diagnostic(a_source_position          *error_pos,
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
      write_position_and_severity(severity, error_pos, &file_name,
                                  &line_number,
                                  &source_text_needed,
                                  &in_current_source_line,
                                  &line_len);
    }  /* if */

    if (diag_kind != dck_end_list && diag_kind != dck_end_context) {
      /* There is a message to be formatted and written. */
      /* Put out the error message text to stderr. */
      write_message(stderr, &line_len, /*wrap=*/TRUE);

      /* The message is always output to stderr so that the user can see it.
         If raw-listing information has been requested, it is also output to
         the raw-listing file in coded form, for later incorporation into the
         listing.  */
      if (f_raw_listing != NULL) {
        write_diag_to_raw_listing(severity, file_name, line_number,
                                  error_pos, diag_kind);
      }  /* if */
    }  /* if */

    if (diag_kind == dck_standalone || diag_kind == dck_end_list) {
#if !STANDALONE_UTILITY_PROGRAM
      if (source_text_needed && 
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
         diag_kind == dck_end_context) && !context_required) {
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
      term_compilation(severity);
    }  /* if */
#if IL_SHOULD_BE_WRITTEN_TO_FILE && !STANDALONE_UTILITY_PROGRAM
    /* If there are any errors, suppress generation of the intermediate
       language file. */
    if (total_errors + total_catastrophes > 0) cancel_il_file();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && !STANDALONE_UTILITY_PROGRAM */
#if ASM_FUNCTION_ALLOWED && !STANDALONE_UTILITY_PROGRAM
    /* If there are any errors, suppress generation of the asm configuration
       file. */
    if (total_errors + total_catastrophes > 0) cancel_asm_config_file();
#endif /* ASM_FUNCTION_ALLOWED && !STANDALONE_UTILITY_PROGRAM */
    /* Terminate the compilation if the error limit has been reached.  Note
       that remarks and warnings are never counted. */
    if (total_errors + total_catastrophes >= error_limit) {
#if !USING_DRIVER
      fprintf(stderr, "Error limit reached.\n");
#else /* !USING_DRIVER */
      if (f_raw_listing != NULL) {
        fprintf(f_raw_listing, "C \"\" 0 0 error limit reached\n");
      }  /* if */
#endif /* !USING_DRIVER */
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
  write_diagnostic(&error_position, es_internal_error, dck_standalone);
#ifdef __GNUC__
  /* Avoid gcc warning.  write_diagnostic does not return in this case. */
  exit_compilation(es_internal_error);
#endif /* __GNUC__ */
}  /* internal_error */


DOES_NOT_RETURN assertion_failed(char	*filename,
		                 int	 line_number,
				 char   *string)
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
  
  if (string == NULL) {
    sprintf(buffer, "assertion failed at: \"%s\", line %0d\n",
            filename, line_number);
  } else {
    sprintf(buffer, "assertion failed: %s (%s, line %0d)\n", string,
            filename, line_number);
  }  /* if */
  internal_error(buffer);
}  /* assertion_failed */

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

  write_diagnostic(&error_position, es_command_line_error, dck_standalone);
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


static a_boolean check_severity(a_source_position          **error_pos,
                                an_error_severity          *severity,
                                a_diagnostic_category_kind diag_kind)
/*
Compare the current error severity with the threshold setting to see if
this diagnostic should be issued.  If this is a multi-message diagnostic,
it may be necessary to save the current source position and severity or
restore the previously saved settings.
*/
{
  static a_source_position  saved_error_position;
  static an_error_severity  saved_severity = (an_error_severity)es_none;

#if CHECKING
  /* The saved severity level should be es_none if and only if this is a
     diagnostic without extra message lines or if it is the first message
     with such extra lines. */
  if ((saved_severity == (an_error_severity)es_none) !=
      (diag_kind == (a_diagnostic_category_kind)dck_standalone ||
       diag_kind == (a_diagnostic_category_kind)dck_primary ||
       diag_kind == (a_diagnostic_category_kind)dck_context_primary)) {
    internal_error("check_severity: bad saved severity");
  }  /* if */
#endif /* CHECKING */
  if (diag_kind == (a_diagnostic_category_kind)dck_standalone) {
    /* Just use the severity and error position specified. */
  } else if (diag_kind == (a_diagnostic_category_kind)dck_primary ||
             diag_kind == (a_diagnostic_category_kind)dck_context_primary) {
    /* The principal message of a multiple message diagnostic.  Save the
       arguments for later calls. */
    copy_source_position(**error_pos, saved_error_position);
    saved_severity = *severity;
  } else if (diag_kind == (a_diagnostic_category_kind)dck_list ||
             diag_kind == (a_diagnostic_category_kind)dck_end_list ||
             diag_kind == (a_diagnostic_category_kind)dck_end_context) {
    /* Reuse the error position and severity from the primary diagnostic. */
    *error_pos = &saved_error_position;
    *severity = saved_severity;
#if CHECKING
    if (diag_kind == (a_diagnostic_category_kind)dck_end_list ||
        diag_kind == (a_diagnostic_category_kind)dck_end_context) {
      saved_severity = (an_error_severity)es_none;
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  /* Return FALSE if the current severity is below the threshold. */
  return ((int)*severity >= (int)error_threshold);
}  /* check_severity */


#if !STANDALONE_UTILITY_PROGRAM
static a_boolean include_in_context_output
			(a_scope_stack_entry_ptr ssep,
			 a_symbol_ptr	         *context_sym,
			 an_error_code		 *context_error_code)
/*
Return TRUE if this scope stack entry has context information that should
be processed, otherwise return FALSE.  When TRUE is returned *context_sym
is set to point to a symbol that provides the context information and
*context_error_code is set to the appropriate error code.
*/
{
  a_boolean	result = FALSE;
  a_symbol_ptr	sym;
  an_error_code error_code;

  if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
    /* Template instantiations (except for prototype instantiations)
       need additional context information. */
    sym = ssep->instance_sym;
    /* If the instance symbol is NULL use the template symbol instead. */
    if (sym == NULL) {
      sym = ssep->template_sym;
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        error_code = ec_template_function_declaration_context;
      } else if (sym->kind == (a_symbol_kind)sk_class_template) {
        error_code = ec_template_class_argument_list_context;
      } else {
        unexpected_condition();
      }  /* if */
      result = TRUE;
    } else if (sym->kind == (a_symbol_kind)sk_static_data_member) {
      result = TRUE;
      error_code = ec_implicit_static_data_member_definition;
    } else {
      error_code = ec_template_instantiation_context;
      if (is_class_symbol(sym)) {
        result = !sym->variant.class_struct_union.extra_info->is_nonreal_class;
      } else {
        result = TRUE;
      }  /* if */
    }  /* if */
  } else if (ssep->kind == (a_scope_kind)sck_function) {
    /* Compiler generated functions need additional information. */
    if (ssep->assoc_routine->compiler_generated) {
      sym = (a_symbol_ptr)ssep->assoc_routine->source_corresp.assoc_info;
      result = TRUE;
      error_code = ec_compiler_generated_function_context;
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

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void diag_message (an_error_code              error_code,
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
#if CHECKING
  int                i;
#endif /* CHECKING */
  /* This variable does not have to be reset by fe_init. */
  static a_boolean   catastrophe_loop = FALSE;

  if (check_severity(&error_pos, &severity, diag_kind)) {
    if (severity == es_catastrophe &&
        (diag_kind == dck_standalone || diag_kind == dck_primary)) {
      /* Make sure that if catastrophic error leads to another, we abort
         the compilation instead of looping. */
      if (catastrophe_loop) {
        fprintf(stderr, "Loop in catastrophic error processing.\n");
        term_compilation(es_catastrophe);
      }  /* if */
      catastrophe_loop = TRUE;
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
        /* No processing is needed for msk_error_text_part. */
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
          form_symbol_name(error_msg_syms[curr_seg->sequence_no],
                           error_pos, curr_seg);
#endif /* !STANDALONE_UTILITY_PROGRAM */
          break;
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
    write_diagnostic(error_pos, severity, diag_kind);
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
    if (diag_kind != dck_standalone && diag_kind != dck_end_list) {
      /* The context display processing is only required after standalone and
         end-list messages. */
      write_diagnostic(error_pos, severity, diag_kind);
    } else {
      int		num_of_contexts = 0;
      a_symbol_ptr	sym;
      an_error_code	context_error_code;
      /* Check whether we need to supply additional context information. */
      a_scope_depth	sd;
      for (sd = depth_scope_stack; sd > DEPTH_OF_FILE_SCOPE; --sd) {
        if (include_in_context_output(&scope_stack[sd], &sym,
                                      &context_error_code)) {
          num_of_contexts++;
        }  /* if */
      }  /* for */
      /* Issue the original message. */
      context_required = num_of_contexts > 0;
      write_diagnostic(error_pos, severity, diag_kind);
      context_required = FALSE;
      /* Loop through the scope stack and output context information. */
      if (num_of_contexts > 0) {
        a_diagnostic_category_kind	context_diag_kind;
        char				*prefix_string;
        if (num_of_contexts != 1) {
          /* If there is more than one line of context we output an
	     initial header line. */
          init_error_params();
          diag_message(ec_template_detected_during_header, &error_position,
                       severity, dck_context_primary);
        }  /* if */
        for (sd = depth_scope_stack; sd > DEPTH_OF_FILE_SCOPE; --sd) {
          a_scope_stack_entry_ptr ssep = &scope_stack[sd];
          a_symbol_ptr		  sym;
          if (!include_in_context_output(ssep, &sym,
                                         &context_error_code)) continue;
          /* If only one line of context is being issued, then it is
	     considered the "primary" context line and is prefixed with
	     the string "detected during".  Otherwise a header was issued
	     above and the context lines are handled as list elements. */
          if (num_of_contexts == 1) {
 	    context_diag_kind = dck_context_primary;
	    prefix_string = "detected during ";
          } else {
 	    context_diag_kind = dck_list;
	    prefix_string = "";
          }  /* if */
          init_error_params();
          error_msg_syms[1] = sym;
	  error_msg_strings[1] = prefix_string;
	  error_msg_positions[1] = &ssep->source_position;
          error_msg_scopes[1] = ssep;
          diag_message(context_error_code,
                       &error_position, severity, context_diag_kind);
        }  /* for */
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

#if 0
/* This routine is not currently used by the compiler. */

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
#endif /* 0 */


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
}  /* str_add_diag_info */


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
}  /* str_add_diag_info */

#if !STANDALONE_UTILITY_PROGRAM

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
}  /* pos_sy_start_error */


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


#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
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
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */


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

#endif /* !STANDALONE_UTILITY_PROGRAM */

void end_error(void)
/*
Complete the multiple message diagnostic currently being processed.
*/
{
  init_error_params();
  diag_message(ec_no_error, (a_source_position *)NULL, es_none, dck_end_list);
}  /* end_error */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
