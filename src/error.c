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
#include "error.h"
#include "host_envir.h"
#include "cmd_line.h"
#include "mem_manage.h"

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
   independently of a front end (e.g. with a standalone IL display utility. */

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
  msk_last			/* Termination of the current message
				   being formatted.  This should be the last
				   message segment kind. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte a_message_segment_kind;

typedef struct msg_segment *msg_segment_ptr;
typedef struct msg_segment {
  msg_segment_ptr
		next;		/* Pointer to the next message segment. */
  char		*segment;	/* Pointer to the message segment buffer. */
  int		length;		/* Current length of the message segment. */
  int		max_length;	/* Maximum string size that can be accommodated
				   in the message segment buffer. */
  short		sequence;	/* Sequence number of the user string, type or
				   symbol name in the error message.  This
				   field is meaningless for kind ==
				   msk_error_text_part. */
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
    } symbol;
  } variant;
} msg_segment;




#define MAX_ERR_SEG_KIND_PER_MSG 2
				/* The maximum number of error message
				   arguments of any message segment kind. */
/*
Diagnostic message substitutions can be based upon strings, types, and symbols
passed to the appropriate diagnostic routines.  The following arrays of
pointers to these various substitution kinds are used to denote the
source of substitutions in message segments.  The sequence number in
message segment descriptor is used as an index into the appropriate array.
*/
static char *	error_msg_strings[MAX_ERR_SEG_KIND_PER_MSG + 1];
				/* Array of pointers to the strings to be
				   inserted into diagnostic messages. */
static a_type_ptr
		error_msg_types[MAX_ERR_SEG_KIND_PER_MSG + 1];
				/* Array of pointers to the types to be
				   used for substitutions in diagnostic
				   messages. */
#if !STANDALONE_UTILITY_PROGRAM
static a_symbol_ptr
		error_msg_syms[MAX_ERR_SEG_KIND_PER_MSG + 1];
				/* Array of pointers to the symbols to be
				   used for substitutions in diagnostic
				   messages. */
#endif /* !STANDALONE_UTILITY_PROGRAM */
static msg_segment_ptr
	error_message_head = NULL;
				/* Pointer to the first segment in the current
				   error message being formatted. */


static void form_param_list(a_routine_type_supplement_ptr suppl_ptr,
                            msg_segment_ptr               seg_ptr);


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
      m = "nested comment not allowed";
      break;
    case ec_bad_use_of_sharp:
      m = "\"#\" not allowed here";
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
      m = "directive not allowed -- an #else has already appeared";
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
      m = "operands are incompatible";
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
      m = "function call not allowed in a constant expression";
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
      m = "a declaration here may only declare a parameter";
      break;
    case ec_dupl_param_name:
      m = "duplicate parameter name";
      break;
    case ec_id_already_declared:
      m = "name has already been declared in the current scope";
      break;
    case ec_nonstd_forward_def_enum:
      m = "forward-defined enum type is nonstandard";
      break;
    case ec_struct_too_large:
      if (C_dialect == C_dialect_cplusplus) {
        m = "class is too large";
      } else {
        m = "struct or union is too large";
      }  /* if */
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
    case ec_label_already_defined:
      m = "label %n has already been defined";
      break;
    case ec_label_never_defined:
      m = "label %n was referenced but not defined";
      break;
    case ec_continue_must_be_in_loop:
      m = "a continue statement may only be used within a loop";
      break;
    case ec_break_must_be_in_loop_or_switch:
      m = "a break statement may only be used within a loop or switch";
      break;
    case ec_no_value_returned_in_non_void_function:
      m = "a non-void function should return a value";
      break;
    case ec_value_returned_in_void_function:
      m = "a void function may not return a value";
      break;
    case ec_cast_not_scalar_or_void:
      m = "type of cast must be arithmetic, pointer, or void";
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
    case ec_expr_not_ptr_to_struct_or_union:
      if (C_dialect == C_dialect_cplusplus) {
        m = "expression must have pointer-to-class type";
      } else {
        m = "expression must have pointer-to-struct-or-union type";
      }  /* if */
      break;
    case ec_exp_field_name:
      if (C_dialect == C_dialect_cplusplus) {
        m = "expected a member name";
      } else {
        m = "expected a field name";
      }  /* if */
      break;
    case ec_not_a_member:
      if (C_dialect == C_dialect_cplusplus) {
        m = "no such member in this class";
      } else {
        m = "no such field in this struct or union";
      }  /* if */
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
    case ec_too_many_memory_regions:
      m = "program too large to compile (too many functions)";
      break;
    case ec_bad_initializer_type:
      m = "incorrect initial value type";
      break;
    case ec_cannot_initialize:
      m = "this entity may not be initialized";
      break;
    case ec_too_many_initializer_values:
      m = "too many initializer values";
      break;
    case ec_not_compatible_with_previous_decl:
      m = "this declaration is incompatible with previous declaration";
      break;
    case ec_already_initialized:
      m = "this variable has already been initialized";
      break;
    case ec_bad_file_scope_storage_class:
      m = "a file-scope declaration may not have this storage class";
      break;
    case ec_typedef_cannot_be_param_name:
      if (C_dialect == C_dialect_cplusplus) {
        m = "a type name may not be redeclared as a parameter";
      } else {
        m = "a typedef name may not be redeclared as a parameter";
      }  /* if */
      break;
    case ec_non_zero_int_conv_to_pointer:
      m = "conversion of non-zero integer to pointer";
      break;
    case ec_expr_not_struct_or_union:
      if (C_dialect == C_dialect_cplusplus) {
        m = "expression must have class type";
      } else {
        m = "expression must have struct or union type";
      }  /* if */
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
      m = "declaration is incompatible with previous use of same name";
      break;
    case ec_external_name_clash:
      m = "name conflicts with previously used external name %sq";
      break;
    case ec_routine_definition_missing:
      m = "function %n was referenced but not defined";
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
      m = "argument is incompatible with its prototype";
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
    case ec_variable_declared_but_not_referenced:
      m = "variable %n declared and never referenced";
      break;
    case ec_routine_declared_but_not_referenced:
      m = "routine %n declared and never referenced";
      break;
    case ec_label_declared_but_not_referenced:
      m = "label %n declared and never referenced";
      break;
    case ec_pcc_address_of_array:
      m = "\"&\" applied to an array has no effect";
      break;
    case ec_mod_by_zero:
      m = "right operand of \"%\" is zero";
      break;
    case ec_old_style_incompatible_param:
      m = "argument is incompatible with formal parameter";
      break;
    case ec_parameter_declared_but_not_referenced:
      m = "parameter %n declared and never referenced";
      break;
    case ec_printf_arg_mismatch:
      m = "invalid argument type for format string conversion";
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
#if ASM_STATEMENT_ALLOWED
    case ec_exp_asm_string:
      m = "expected an asm string";
      break;
#endif /* ASM_STATEMENT_ALLOWED */
#if ASM_FUNCTION_ALLOWED
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
#endif /* ASM_FUNCTION_ALLOWED */
    case ec_file_delete_error:
      m = "error while deleting %s file";
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
      m = "variable %sq declared with a never-completed type";
      break;
    case ec_boolean_controlling_expr_is_constant:
      m = "controlling expression is constant";
      break;
    case ec_switch_selector_expr_is_constant:
      m = "selector expression is constant";
      break;
    case ec_address_of_void:
      m = "taking the address of something of type void is not allowed";
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
      m = "qualified name is not in the left operand's class or base classes";
      break;
    case ec_member_ref_requires_object:
      m = "a nonstatic member reference must be relative to a specific object";
      break;
    case ec_nonstatic_member_def_not_allowed:
      m = "a nonstatic data member may not be defined outside its class";
      break;
    case ec_redefinition_not_allowed:
      m = "redefinition of this object is not allowed";
      break;
    case ec_static_member_in_local_class:
      m = "static data member is not allowed in a local class";
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
      m = "reference-type object requires an initializer";
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
      m = "invalid redeclaration of type name";
      break;
    case ec_missing_initializer_on_const:
      m = "initializer for const variable is missing";
      break;
    case ec_this_used_incorrectly:
      m = "\"this\" may only be used inside a nonstatic member function";
      break;
    case ec_constant_value_not_known:
      m = "constant value is not known";
      break;
    case ec_missing_type_specifier:
      m = "explicit type specifier is missing";
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
      m = "inheritance of %sq is ambiguous";
      break;
    case ec_old_style_parameter_list:
      m = "old-style parameter list";
      break;
    case ec_declaration_after_statements:
      m = "declaration may not appear after executable statement in block";
      break;
    case ec_inaccessible_base_class:
      m = "base class is inaccessible";
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
    case ec_not_equivalent_to_inherited_member:
      m = "qualified name not equivalent to inherited member %sq";
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
    case ec_unary_colon_colon_in_declarator:
      m = "unary \"::\" is not allowed on a name in a declarator";
      break;
    case ec_name_not_found_in_file_scope:
      m = "no such name declared in the file scope";
      break;
    case ec_qualified_name_not_allowed:
      m = "qualified name is not allowed";
      break;
    case ec_null_reference:
      m = "NULL reference is not allowed";
      break;
    case ec_brace_initialization_not_allowed:
      m = "initialization with \"{...}\" is not allowed for this object";
      break;
    case ec_ambiguous_base_class:
      m = "base class is ambiguous";
      break;
    case ec_ambiguous_derived_class:
      m = "derived class contains more than one instance of this class";
      break;
    case ec_derived_class_from_virtual_base:
      m = "derived class has this class as a virtual base class";
      break;
    case ec_no_matching_constructor:
      m = "none of the available constructors matches this argument list";
      break;
    case ec_ambiguous_copy_constructor:
      m = "copy constructor for class %t is ambiguous";
      break;
    case ec_no_default_constructor:
      m = "no default constructor exists for class %t";
      break;
    case ec_not_a_field_or_base_class:
      m = "not a nonstatic data member or base class of class %t";
      break;
    case ec_indirect_nonvirtual_base_class_not_allowed:
      m = "indirect nonvirtual base class not allowed";
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
      m = "cannot determine which instance of overloaded function is intended";
      break;
    case ec_bound_function_must_be_called:
      m =
         "a pointer to a bound function may only be used to call the function";
      break;
    case ec_duplicate_typedef:
      m = "typedef name has already been declared (with same type)";
      break;
    case ec_function_redefinition:
      m = "this function has already been defined";
      break;
    case ec_overloaded_function_incompatible_type:
      m = "type does not match any instance of overloaded function %sq";
      break;
    case ec_no_matching_function:
      m = "no instance of this overloaded function matches this argument list";
      break;
    case ec_type_def_not_allowed_in_func_type_decl:
      m = "type definition not allowed in function return type declaration";
      break;
    case ec_default_arg_not_at_end:
      m = "default argument not at end of parameter list";
      break;
    case ec_default_arg_already_defined:
      m = "redefinition of default argument";
      break;
    case ec_ambiguous_overloaded_function:
      m =
       "more than one overloaded function instance matches this argument list";
      break;
    case ec_ambiguous_constructor:
      m = "more than one constructor matches this argument list";
      break;
    case ec_bad_default_arg_type:
      m = "default argument expression is incompatible with parameter";
      break;
    case ec_return_type_cannot_distinguish_functions:
      m = "cannot overload functions distinguished by return type alone";
      break;
    case ec_no_conversion_constructor:
      m = "no appropriate constructor or conversion function exists";
      break;
    case ec_function_qualifier_not_allowed:
      m = "const or volatile qualifier on this function is not allowed";
      break;
    case ec_bad_virtual_decl:
      m = "only nonstatic member functions may be declared virtual";
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
      m = "redefinition of virtual function %n is ambiguous";
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
      m = "linkage specification is incompatible with previous declaration";
      break;
    case ec_overloaded_function_linkage:
      m = "more than one instance of overloaded function %n has \"C\" linkage";
      break;
    case ec_ambiguous_default_constructor:
      m = "more than one default constructor for class %t";
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
    case ec_ambiguous_conversion_constructor:
      m = "more than one constructor or conversion function applies";
      break;
    case ec_no_matching_operator_function:
      m = "none of the available operator functions matches these operands";
      break;
    case ec_ambiguous_operator_function:
      m = "more than one operator function matches these operands";
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
      m = "base class %t assumed (anachronism)";
      break;
    case ec_member_already_initialized:
      m = "member %n has already been initialized";
      break;
    case ec_missing_base_class_or_member_name:
      m = "name of member or base class is missing";
      break;
    case ec_assignment_to_this:
      m = "assignment to \"this\" (anachronism)";
      break;
    case ec_overload_ignored:
      m = "\"overload\" ignored (anachronism)";
      break;
    case ec_anon_union_member_access:
      m = "invalid anonymous union -- nonpublic member not allowed";
      break;
    case ec_anon_union_member_function:
      m = "invalid anonymous union -- member function not allowed";
      break;
    case ec_anon_union_storage_class:
      m = "global anonymous union must be declared static";
      break;
    case ec_missing_initializer_on_field:
      m = "no initializer provided for %n";
      break;
    case ec_cannot_initialize_field:
      m = "compiler-generated constructor cannot initialize %n";
      break;
    case ec_uninitialized_const_member:
      if (C_dialect == C_dialect_cplusplus) {
        m = "variable contains uninitialized const member";
      } else {
        m = "variable contains uninitialized const field";
      }  /* if */
      break;
    case ec_uninitialized_ref_member:
      m = "variable contains uninitialized reference member";
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
    case ec_virtual_function_in_union:
      m = "virtual member function not allowed in a union";
      break;
    case ec_static_member_in_union:
      m = "static data member not allowed in a union";
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
      m =
       "none of the available operator new() functions matches these operands";
      break;
    case ec_delete_already_declared:
      m = "operator delete() may not be overloaded";
      break;
    case ec_no_match_for_addr_of_overloaded_function:
      m = "no instance of overloaded function %n matches the required type";
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
      m = "\"main\" may not be called or have its address taken";
      break;
    case ec_initializer_not_allowed_on_array_new:
      m = "a new-initializer may not be specified for an array";
      break;
    case ec_member_function_redeclaration:
      m = "member function may not be redeclared outside its class";
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
      m = "access adjustment not allowed -- mixed accessibility for %n";
      break;
    case ec_missing_user_defined_assignment_for_copy:
      m = "implicit generation of %nf is not allowed";
      break;
    case ec_nonstd_array_cast:
      m = "cast to array type is nonstandard (treated as cast to %t)";
      break;
    case ec_virtual_new_or_delete_not_allowed:
      m = "operator %s() may not be declared virtual";
      break;
    case ec_class_with_op_new_but_no_op_delete:
      m = "class %n has an operator new() but no operator delete()";
      break;
    case ec_class_with_op_delete_but_no_op_new:
      m = "class %n has an operator delete() but no operator new()";
      break;
    case ec_class_with_virtual_func_but_nonvirtual_dtor:
      m = "class %n has virtual functions but destructor is nonvirtual";
      break;
    case ec_no_access_to_constructors:
      m = "there is no access to the constructors for class %n";
      break;
    case ec_nonstd_member_function_redeclaration:
      m = "redeclaring a member function is nonstandard";
      break;
    case ec_static_main:
      m = "\"main()\" may not be declared static";
      break;
    case ec_inline_main:
      m = "\"main()\" may not be declared inline";
      break;
    case ec_class_and_member_function_name_conflict:
      m =
       "member function with the same name as its class must be a constructor";
      break;
    case ec_nested_class_anachronism:
      m = "using nested class %n (anachronism)";
      break;
    case ec_too_many_params_for_destructor:
      m = "a destructor may not have parameters";
      break;
    case ec_bad_constructor_param:
      m =
      "a constructor parameter may not have the type of the constructed class";
      break;
    case ec_incomplete_return_type_not_allowed:
      m = "a function with an incomplete return type may not be called";
      break;
    case ec_protected_access_problem:
      m =
      "protected member %n is not accessible through this pointer or object";
      break;
    case ec_param_not_allowed:
      m = "a parameter is not allowed";
      break;
    case ec_unimplemented_keyword:
      m = "%n is reserved for future use as a keyword";
      break;
    /* +++ -- For ease of finding the insert point for new diagnostics. */
    case ec_no_error:
    default:
#if CHECKING
      internal_error("error_text: unknown error code");
#else
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

static void add_string_to_segment(char		  *str,
				  msg_segment_ptr seg_ptr)
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
      seg_ptr->segment    = new_buffer;
      seg_ptr->max_length = new_size - 1;
    }  /* if */
    (void)strcpy((char *)(seg_ptr->segment + seg_ptr->length), str);
    seg_ptr->length += length_of_string;
  }  /* if */
}  /* add_string_to_segment */


static void form_int_kind_name(an_integer_kind kind,
                               msg_segment_ptr seg_ptr)
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
#if CHECKING
    default:
      internal_error("form_int_kind_name: bad integer kind");
#endif /* CHECKING */
  }  /* switch */
  add_string_to_segment(s, seg_ptr);
}  /* form_int_kind_name */


static void form_float_kind_name(a_float_kind    kind,
                                 msg_segment_ptr seg_ptr)
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


static void form_type_qualifier(a_type_ptr      type,
                                msg_segment_ptr seg_ptr)
/*
Add a type qualifier to the type string being formatted at the position
indicated by type_string_ptr.  type_string_ptr is then incremented by
the length of the type qualifier added.
*/
{
  a_boolean is_const = FALSE,
            is_volatile = FALSE;

  for (;
       type->kind == (a_type_kind)tk_typeref;
       type = type->variant.typeref.type) {
    if (type->variant.typeref.is_const) is_const = TRUE;
    if (type->variant.typeref.is_volatile) is_volatile = TRUE;
  }  /* for */
  if (is_const) add_string_to_segment("const ", seg_ptr);
  if (is_volatile) add_string_to_segment("volatile ", seg_ptr);
}  /* form_type_qualifier */


static void form_type_specifier(a_type_ptr      type,
                                msg_segment_ptr seg_ptr)
/*
Add the type specifier to the type string being formed.
*/
{
  char	*s = NULL;

  switch (type->kind) {
    case tk_error:
      s = "<error type>";
      break;
    case tk_unknown:
      s = "<unknown type>";
      break;
    case tk_void:
      s = "void";
      break;
    case tk_integer:
      if (type->variant.integer.enum_type) {
        s = "enum ";
        goto do_tag_name;
      }  /* if */
      if (type->variant.integer.explicitly_signed) {
        add_string_to_segment("signed ", seg_ptr);
      }  /* if */
      form_int_kind_name(type->variant.integer.int_kind, seg_ptr);
      break;
    case tk_float:
      form_float_kind_name(type->variant.float_kind, seg_ptr);
      break;
    case tk_struct:
      if (C_dialect != C_dialect_cplusplus) s = "struct ";
      goto do_tag_name;
    case tk_union:
      if (C_dialect != C_dialect_cplusplus) s = "union ";
    case tk_class:
do_tag_name:
      add_string_to_segment(s, seg_ptr);
      if (type->source_corresp.name != NULL) {
        s = type->source_corresp.name;
      } else {
        /* Have an unnamed class. */
        s = "<unnamed>";
      }  /* if */
      break;
    case tk_typeref:
      /* Look at each level of typeref.  If one with a name is found, print
         the name.  Otherwise, when we reach a non-typeref, print that.
         Note that type qualifiers are unimportant as far as the code here. */
      do {
        if (type->source_corresp.name != NULL) {
          /* Named typeref (i.e., a typedef).  Print the name. */
          add_string_to_segment(type->source_corresp.name, seg_ptr);
          goto typeref_done;
        }  /* if */
        type = type->variant.typeref.type;
      } while (type->kind == (a_type_kind)tk_typeref);
      form_type_specifier(type, seg_ptr);
typeref_done:
      break;
      /* Note that certain type kinds are handled by form_type_first_part
         and form_type_second_part and shouldn't get here. */
    default:
      internal_error("form_type_specifier: bad type specifier kind");
  }  /* switch */
  if (s != NULL) add_string_to_segment(s, seg_ptr);
}  /* form_type_specifier */


static void form_class_name(a_type_ptr      type,
                            msg_segment_ptr seg_ptr)
/*
Add the class name of the specified type followed by "::" to the message
segment being constructed at "seg_ptr".  Use "<unnamed>" if the class
has no user name.
*/
{
  char 	*s;

  if (type != NULL && C_dialect == C_dialect_cplusplus) {
    if (type->source_corresp.name != NULL) {
      s = type->source_corresp.name;
    } else {
      s = "<unnamed>";
    }  /* if */
    add_string_to_segment(s, seg_ptr);
    add_string_to_segment("::", seg_ptr);
  }  /* if */
}  /* form_class_name */


static void form_type_first_part(a_type_ptr      type,
                                 a_boolean       need_parens,
                                 msg_segment_ptr seg_ptr)
/*
Add the first of possibly two parts of a type reference.
*/
{
  a_type_ptr local_type;

  /* For the pointer case, ignore any typerefs that provide qualifiers
     on the indirection. */
  if (is_pointer_or_reference_type(type)) {
    local_type = skip_typerefs(type)->variant.pointer.type;
    /* Recursive call to print out any lower indirections. */
    form_type_first_part(local_type, /*need_parens=*/FALSE, seg_ptr);
    /* Print out the star for this indirection. */
    if (skip_typerefs(type)->variant.pointer.is_reference) {
      /* This is a C++ reference type */
      add_string_to_segment("&", seg_ptr);
    } else {
      add_string_to_segment("*", seg_ptr);
    }  /* if */
    form_type_qualifier(type, seg_ptr);
  } else if (type->kind == (a_type_kind)tk_array) {
    form_type_first_part(type->variant.array.element_type,
                         /*need_parens=*/TRUE, seg_ptr);
  } else if (type->kind == (a_type_kind)tk_ptr_to_member) {
    /* C*++ pointer to member type */
    form_type_first_part(type->variant.ptr_to_member.type,
                         /*needs_parens=*/TRUE, seg_ptr);
    form_class_name(type->variant.ptr_to_member.class_of_which_a_member,
                    seg_ptr);
    add_string_to_segment("*", seg_ptr);
  } else if (type->kind == (a_type_kind)tk_routine) {
    form_type_first_part(type->variant.routine.return_type,
                         /*need_parens=*/FALSE, seg_ptr);
  } else {
    form_type_qualifier(type, seg_ptr);
    form_type_specifier(type, seg_ptr);
  }  /* if */
  if (need_parens) {
    add_string_to_segment("(", seg_ptr);
  }  /* if */
}  /* form_type_first_part */


static void form_type_second_part(a_type_ptr      type,
                                  a_boolean       need_parens,
                                  msg_segment_ptr seg_ptr)
/*
Add the second part of a type reference to the type string being formed.
If it's a pointer, just continue to look for the base type.  If it's an
array, print out the dimension information.
*/
{
  a_type_ptr local_type;

  /* For the pointer case, ignore any typerefs that provide qualifiers
     on the indirection. */
  if (is_pointer_or_reference_type(type)) {
    local_type = skip_typerefs(type);
    if (need_parens) {
      add_string_to_segment(")", seg_ptr);
    }  /* if */
    form_type_second_part(local_type->variant.pointer.type,
                          /*need_parens=*/FALSE, seg_ptr);
  } else if (type->kind == (a_type_kind)tk_array) {
    if (need_parens) {
      add_string_to_segment(")", seg_ptr);
    }  /* if */
    if (type->variant.array.number_of_elements == 0) {
      add_string_to_segment("[]", seg_ptr);
    } else {
      char	buffer[BASE_MSG_SEGMENT_SIZE];
#if CHECKING
      if (digits_to_represent(
                    (unsigned long)type->variant.array.number_of_elements)
                            >= BASE_MSG_SEGMENT_SIZE) {
        internal_error
              ("form_type_second_part: tk_array: buffer size too small");
      }  /* if */
#endif /* CHECKING */
      (void)sprintf(buffer, "[%lu]",
                    (unsigned long)type->variant.array.number_of_elements);
      add_string_to_segment(&buffer[0], seg_ptr);
    }  /* if */
    form_type_second_part(type->variant.array.element_type,
                          /*need_parens=*/TRUE, seg_ptr);
  } else if (type->kind == (a_type_kind)tk_ptr_to_member) {
    /* C*++ pointer to member type */
    if (need_parens) {
      add_string_to_segment(")", seg_ptr);
    }  /* if */
    form_type_second_part(type->variant.ptr_to_member.type,
                          /*needs_parens=*/TRUE, seg_ptr);
  } else if (type->kind == (a_type_kind)tk_routine) {
    if (need_parens) {
      add_string_to_segment(")", seg_ptr);
    }  /* if */
    form_param_list(type->variant.routine.extra_info, seg_ptr);
    form_type_second_part(type->variant.routine.return_type,
                          /*need_parens=*/FALSE, seg_ptr);
  }  /* if */
}  /* form_type_second_part */


static void form_param_list(a_routine_type_supplement_ptr suppl_ptr,
                            msg_segment_ptr               seg_ptr)
/*
Add the parameter list of a function to the type string being formatted.
*/
{
  a_param_type_ptr	param_ptr;
  a_boolean		has_ellipsis;

  add_string_to_segment("(", seg_ptr);
  if (suppl_ptr->prototyped) {
    has_ellipsis = suppl_ptr->has_ellipsis;
    for (param_ptr = suppl_ptr->param_type_list;
         param_ptr != NULL;
         param_ptr = param_ptr->next) {
      form_type_first_part(param_ptr->type, /*need_parens=*/FALSE, seg_ptr);
      form_type_second_part(param_ptr->type, /*need_parens=*/FALSE, seg_ptr);
      if (param_ptr->next != NULL || has_ellipsis) {
        add_string_to_segment(", ", seg_ptr);
      }  /* if */
    }  /* for */
    if (has_ellipsis) {
      add_string_to_segment("...", seg_ptr);
    }  /* if */
  }  /* if */
  add_string_to_segment(")", seg_ptr);
}  /* form_param_list */


static void form_type_summary(a_type_ptr      tp,
                              msg_segment_ptr seg_ptr)
/*
Format a string that represents the type pointed to by "tp" into the message
segment described by "seg_ptr".
*/
{
  form_type_first_part(tp, /*need_parens=*/FALSE, seg_ptr);
  form_type_second_part(tp, /*need_parens=*/FALSE, seg_ptr);
}  /* summarize_type */

#if !STANDALONE_UTILITY_PROGRAM

static void form_decl_position(a_symbol_ptr    sym,
                               msg_segment_ptr seg_ptr)
/*
Format the declaration position for the specified symbol in the message
segment described by seg_ptr.  The generated format is:

        (declared at line xxx of "yyyyyy.c")
*/
{
  char		*file_name, *full_name;
  char		buffer[BASE_MSG_SEGMENT_SIZE];
  a_line_number line_number;
  a_boolean	at_end_of_source;

  if (sym->decl_position.seq != 0) {
    /* Have a valid source position. */
    conv_seq_to_file_and_line(sym->decl_position.seq, &file_name, &full_name,
                              &line_number, &at_end_of_source);
    if (at_end_of_source) {
      add_string_to_segment(" (at end of source: \"", seg_ptr);
    } else {
      add_string_to_segment(" (declared at line ", seg_ptr);
#if CHECKING
      if (digits_to_represent((unsigned long)sym->decl_position.seq)
                          >= BASE_MSG_SEGMENT_SIZE) {
        internal_error("form_bound: buffer size too small");
      }  /* if */
#endif /* CHECKING */
      (void)sprintf(buffer, "%ld", (unsigned long)line_number);
      add_string_to_segment(&buffer[0], seg_ptr);
      add_string_to_segment(" of \"", seg_ptr);
      add_string_to_segment(file_name, seg_ptr);
      add_string_to_segment("\")", seg_ptr);
    }  /* if */
  }  /* if */
}  /* form_decl_position */


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


static void form_symbol_name(a_symbol_ptr    sym,
                             msg_segment_ptr seg_ptr)
/*
Format the name of the symbol pointed to by "sym" in the message segment
described by "seg_ptr".  Type information is based on the fundamental symbol
and the name is that of "sym".
*/
{
  a_type_ptr	type = NULL;
  a_routine_ptr	routine = NULL;	
  a_symbol_ptr  fund_sym;	/* Pointer to the fundamental symbol of
				   argument "sym" if it exists.  Otherwise,
				   the value will be that of "sym". */
  a_boolean	is_routine = FALSE;
  a_boolean	is_constructor = FALSE;
  a_boolean	is_destructor = FALSE;
  a_boolean	is_overloaded = FALSE;
  a_boolean	is_conversion = FALSE;

  /* Determine the fundamental symbol of this symbol. */
  fund_sym = fundamental_symbol_of(sym);
  add_string_to_segment("\"", seg_ptr);
  switch (fund_sym->kind) {
    case sk_keyword:
      add_string_to_segment(token_names[(int)fund_sym->variant.keyword_token],
                            seg_ptr);
      break;

    case sk_macro:
    case sk_label:
    case sk_type:
    case sk_class_or_struct_tag:
    case sk_union_tag:
    case sk_enum_tag:
      goto symbol_name;

    case sk_variable:
      type = fund_sym->variant.variable->type;
      goto symbol_name;

    case sk_extern_variable:
      type = fund_sym->variant.extern_symbol_descr->type;
      goto symbol_name;

    case sk_constant:
      type = fund_sym->variant.constant->type;
      goto symbol_name;

    case sk_routine:
    case sk_member_function:
      type = routine_symbol_type(fund_sym);
      routine = fund_sym->variant.routine;
      is_routine = TRUE;
      goto symbol_name;

    case sk_extern_routine:
      type = fund_sym->variant.extern_symbol_descr->type;
      routine = fund_sym->variant.extern_symbol_descr->variant.routine;
      is_routine = TRUE;
      goto symbol_name;

    case sk_overloaded_function:
      is_routine = TRUE;
      goto symbol_name;

    case sk_static_data_member:
      type = fund_sym->variant.variable->type;
      goto symbol_name;

    case sk_field:
      type = fund_sym->variant.field.ptr->type;

symbol_name:
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
        add_string_to_segment(" ", seg_ptr);
      }  /* if */
      form_class_name(sym->class_of_which_a_member, seg_ptr);
      if (is_conversion) {
        /* This is a conversion function; form the name as "operator type". */
        add_string_to_segment("operator ", seg_ptr);
        form_type_first_part(type, /*need_parens=*/FALSE, seg_ptr);
      }  else {
        /* Use the name in the header. */
        add_string_to_segment(sym->header->identifier, seg_ptr);
      }  /* if */
      if (type != NULL &&
          (seg_ptr->variant.symbol.full_type || is_overloaded) ) {
        form_type_second_part(type, /*need_parens=*/FALSE, seg_ptr);
      } else if (is_routine && (! seg_ptr->variant.symbol.name_only)) {
        add_string_to_segment("()", seg_ptr);
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
  add_string_to_segment("\"", seg_ptr);

  /* Add the declaration position as requested. */
  if (seg_ptr->variant.symbol.decl_pos) {
    form_decl_position(sym, seg_ptr);
  }  /* if */
}  /* form_symbol_name */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static msg_segment_ptr new_message_segment(void)
/*
Allocate and initialize the fixed part a new message segment.
*/
{
  msg_segment_ptr	msg;

  msg = (msg_segment_ptr)alloc_general(sizeof(msg_segment));
  msg->next       = NULL;
  msg->segment    = NULL;
  msg->length     = 0;
  msg->max_length = 0;
  msg->sequence   = 1;
  return msg;
}  /* new_message_segment */


static void construct_message_segments(char *msg_ptr)
/*
Scan the message template pointed to by msg_ptr and construct the message
segment list.  The global variable error_message_head points to the first
segment descriptor.  Parameter substitutions are specified in the message
template beginning with a "%".  Accepted substitution designations are:

	s[q]x		- user provided string insertion.
	tx		- type insertion in double quotes.
        n[f|o][d]x	- symbol name insertion in double quotes.

where "x" is an optional number in the range of 1 to MAX_ERR_SEG_KIND_PER_MSG
(defaulted to 1) that indicates which of multiple types, strings, or
symbols substitutions to be used.

String inserts may have an optional "q" modifier which specifies that the
string is to be enclosed in quotes.

Symbol name expansions may have one of the mutually exclusive optional
modifiers:

	f	- full object, complete type and object name.
	o	- name or qualified name only.

Symbol name expansions may have a declaration position modifier"d" which
requests that the declaration position of the symbol be added at the end
of the expansion.

The linked list of message segments needed for the text and parameter
substitutions to form the desired diagnostic message is constructed.

NOTE:  Symbol name insertion is not available if STANDALONE_UTILITY_PROGRAM
       is defined.  The symbol table and token names no longer exist.
*/
{
  msg_segment_ptr
		curr_segment;		/* Pointer to the current segment. */
  char		*end_ptr;
  int           i;
  
  /* Establish the message segment descriptor for the first segment. */
  curr_segment = error_message_head;
  if (curr_segment == NULL) {
    /* For the very first time, this will be NULL. */
    curr_segment = error_message_head = new_message_segment();
  }  /* if */
  curr_segment->length = 0;
  curr_segment->sequence = 1;

  while (*msg_ptr != '\0') {
    switch (*msg_ptr) {
      case '%':
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
            }
            goto check_for_seq_number;
          case 't':
            curr_segment->kind = (a_message_segment_kind)msk_type;
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
            msg_ptr++;
            if (*msg_ptr == 'f') {
              /* Display complete type and object name. */
              curr_segment->variant.symbol.full_type = TRUE;
              msg_ptr++;
            } else if (*msg_ptr == 'o') {
              curr_segment->variant.symbol.name_only = TRUE;
              msg_ptr++;
            }  /* if */
            if (*msg_ptr == 'd') {
              curr_segment->variant.symbol.decl_pos = TRUE;
              msg_ptr++;
            }  /* if */
#endif /* STANDALONE_UTILITY_PROGRAM */
check_for_seq_number:
            curr_segment->sequence = 1;
            if (isdigit(*msg_ptr)) {
              i = toascii(*msg_ptr) - toascii('0');
              if (i > 0 && i <= INCR_MSG_SEGMENT_SIZE) {
                curr_segment->sequence = i;
                msg_ptr++;
              }  /* if */
            }  /* if */
            break;
#if CHECKING
          default:
            internal_error(
         "construct_message_segments: unknown message substitution parameter");
#endif /* CHECKING */
        }  /* switch */
        break;

      default:
#if STANDALONE_UTILITY_PROGRAM
text_segment:
#endif /* STANDALONE_UTILITY_PROGRAM */
        /* This is the first character of a text segment. */
        curr_segment->kind = (a_message_segment_kind)msk_error_text_part;
        curr_segment->variant.msg_part = msg_ptr;
        end_ptr = strchr(msg_ptr, '%');
        if (end_ptr == NULL) {
          /* This part is the end of the message template. */
          curr_segment->length = strlen(msg_ptr);
        } else {
          /* A substitution parameter has been found.  The length is the
	     difference of the two pointers. */
          curr_segment->length = end_ptr - msg_ptr;
        }  /* if */
        msg_ptr += curr_segment->length;
    }  /* switch */

    /* Prepare for the next message segment. */
    if (curr_segment->next == NULL) {
      /* Reached the current end of the chain; add another segment. */
      curr_segment->next = new_message_segment();
    }  /* if */
    curr_segment = curr_segment->next;
    curr_segment->length = 0;
    curr_segment->sequence = 1;
  }  /* while */

  /* Having reached the end of the diagnostic message template, terminate
     the message segment chain by setting the current segment kind to
     msk_last. */
  curr_segment->kind = (a_message_segment_kind)msk_last;
}  /* construct_message_segments */

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
    if (!pass_for_caret || (out_char) == '\t') {                      \
      putc((out_char), stderr);                                       \
    } else {                                                          \
      putc(' ', stderr);                                              \
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
    fputs("  ", stderr);
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
      if (pass_for_caret) putc('^', stderr);
    }  /* if */
    /* For both passes, end the output line. */
    putc('\n', stderr);
    /* After the first pass (writing the source), go on to the second pass
       (writing the caret). */
  }  /* for */
}  /* write_orig_source_line */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void write_message(char      *msg,
                          int       len,
                          FILE      *file,
                          int       *line_len,
                          a_boolean wrap)
/*
Write out a piece of an error message.  msg points to the
message (or is NULL if there is no message), and len is its length (or
-1 if the text is null-terminated).  The message is written to the file
indicated by file.  *line_len is incremented by the number of characters
written.  If wrap is TRUE, the text will be wrapped to successive
additional lines as necessary and *line_len will be set to the number
of characters written on the final line.
*/
{
#define INDENT_AMOUNT 10 /* Number of spaces at the start of continuation
                            lines. */
  int chars_to_take, chars_that_will_fit_on_line;

  if (msg != NULL) {
    if (len < 0) len = strlen(msg);
    while (wrap && 
           (chars_that_will_fit_on_line = 
            MAX_ERROR_OUTPUT_LINE_LENGTH - *line_len) < len) {
      /* The text is too long to fit on one line.  Write part of it,
         and continue on the next line. */
      if (chars_that_will_fit_on_line < 0) chars_that_will_fit_on_line = 0;
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
      if (chars_to_take == 0 && *line_len <= INDENT_AMOUNT) {
        chars_to_take = chars_that_will_fit_on_line;
      }  /* if */
      /* Print the characters that will fit on the current line. */
      if (chars_to_take > 0) {
        *line_len += fprintf(file, "%.*s", chars_to_take, msg);
        msg += chars_to_take;
        len -= chars_to_take;
      }  /* if */
      /* Skip over any blanks at the start of the remaining text of the
         message. */
      while (len > 0 && *msg == ' ') {
        msg++;
        len--;
      }  /* while */
      /* Start a new line and indent. */
      (void)fputc('\n', file);
      for (*line_len = 0; *line_len < INDENT_AMOUNT; (*line_len)++) {
        (void)fputc(' ', file);
      }  /* for */
    }  /* while */
    /* Print the final piece of the text (in the usual case, this prints
       all of the text). */
    if (len > 0) {
      *line_len += fprintf(file, "%.*s", len, msg);
    }  /* if */
  }  /* if */
}  /* write_message */


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
    error_msg_syms[i] = NULL;
#endif /* !STANDALONE_UTILITY_PROGRAM */
  }  /* for */
}  /* init_error_params */


static void write_diagnostic(a_source_position *error_pos,
                             an_error_severity severity)
/*
Write out a diagnostic message with the given message string, position, and
severity.  If the error is severe, terminate the compilation.
The "message" to be put out is the concatenation of the linked list of
message segments pointed to by the global variable error_msg_head.
*/
{
  char            *severity_string, *file_name, *full_name;
  a_line_number   line_number;
  a_boolean       at_end_of_source;
  a_boolean       capitalize_severity;
  a_boolean       column_needed;
#if !STANDALONE_UTILITY_PROGRAM
  a_boolean       source_text_needed = FALSE;
#endif /* !STANDALONE_UTILITY_PROGRAM */
  int             line_len = 0;
  msg_segment_ptr curr_seg;

  if ((int)severity < (int)error_threshold) {
    /* Ignore the message if its severity is below the threshold. */
  } else {
    capitalize_severity = FALSE;
    /* Determine the source position (file, line number). */
    if (error_pos->seq == 0) {
      /* Error position is in the command line or in initialization. */
      /* No position indication is written. */
      capitalize_severity = TRUE;
    } else {
      /* Get the file name and line number associated with the sequence
         number. */
      conv_seq_to_file_and_line(error_pos->seq, &file_name, &full_name,
                                &line_number, &at_end_of_source);
      if (at_end_of_source) {
        /* After end of source. */
        line_len += fprintf(stderr, "At end of source: ");
      } else {
        /* Normal line in file, not end of file. */
#if STANDALONE_UTILITY_PROGRAM
        /* In program-form C-generating back end, source lines are 
           never displayed. */
        column_needed = FALSE;
#else /* !STANDALONE_UTILITY_PROGRAM */
        /* If the line is the current one, print it and a caret indicating
           the position. */
        if (error_pos->seq >= curr_seq_number) {
          /* The sequence number falls within the sequence numbers for the
             current logical source line (it can't be past the current
             line). */
          column_needed = FALSE;
          source_text_needed = TRUE;
        } else {
          /* The sequence number is not in the current logical source line. */
          column_needed = (error_pos->column != 0);
        }  /* if */
#endif /* STANDALONE_UTILITY_PROGRAM */
        /* Print the file and line number, with a column number if the
           position could not be indicated via a caret pointing to the
           source of the current line. */
        /* If the line is from stdin, do not display the file name. */
        if (strcmp(file_name, FILE_NAME_FOR_STDIN) == 0) {
          line_len += fprintf(stderr, "Line %lu", line_number);
        } else {
          line_len += fprintf(stderr, "\"%s\", line %lu", file_name,
                                                          line_number);
        }  /* if */
        if (column_needed) {
          line_len += fprintf(stderr, " (col. %d)", error_pos->column);
        }  /* if */
        line_len += fprintf(stderr, ": ");
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
        severity_string = "";
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
        internal_error("write_diagnostic: bad severity");
#endif /* CHECKING */
    }  /* switch */
    if (capitalize_severity && *severity_string != '\0') {
      /* Capitalize the first letter of the severity, because it's the first
         thing on the line. */
      line_len += fprintf(stderr, "%c%s", toupper(*severity_string),
                                          severity_string+1);
    } else {
      line_len += fprintf(stderr, "%s", severity_string);
    }  /* if */

    /* Put out the error message text.  Try to collect the three parts
       into a single message if possible, as that will improve the
       appearance of the output of the line-wrapping algorithm in
       write_message (if the text is not consolidated, lines may be
       broken at the text-piece boundaries, which can be, for example,
       between a quote and a file name). */

    for (curr_seg = error_message_head;
         curr_seg->kind != (a_message_segment_kind)msk_last;
         curr_seg = curr_seg->next) {
      switch (curr_seg->kind) {
        case msk_error_text_part:
          write_message(curr_seg->variant.msg_part, curr_seg->length, stderr,
                        &line_len, /*wrap=*/TRUE);
          break;
        case msk_user_string:
          { char  *msg;

            if (curr_seg->variant.string.quoted) {
              msg = curr_seg->segment;
            } else {
              msg = error_msg_strings[curr_seg->sequence];
            }  /* if */
            write_message(msg, -1, stderr, &line_len, /*wrap=*/TRUE);
          }
          break;
        case msk_type:
        case msk_symbol:
          write_message(curr_seg->segment, -1, stderr, &line_len,
                        /*wrap=*/TRUE);
          break;
      }  /* switch */
    }  /* for */
    putc('\n', stderr);
#if !STANDALONE_UTILITY_PROGRAM
    if (source_text_needed) {
      /* Write the source text line, with a caret pointing to the location
         of the error. */
      write_orig_source_line(error_pos);
    }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    /* Put out an extra space line after the error, for clarity. */
    putc('\n', stderr);

    /* The message is always output to stderr so that the user can see it.
       If raw-listing information has been requested, it is also output to
       the raw-listing file in coded form, for later incorporation into the
       listing.  The coded form output line has the form

       S "file-name" line-number column-number message-text

       where "S" is R for remark, W for warning, E for error, and 
       C for catastrophe, command-line error, or internal error. */
    if (f_raw_listing != NULL) {
      /* Start with the severity code character. */
      switch (severity) {
        case es_remark:
          (void)fputc('R', f_raw_listing);
          break;
        case es_warning:
          (void)fputc('W', f_raw_listing);
          break;
        case es_error:
          (void)fputc('E', f_raw_listing);
          break;
        case es_catastrophe:
        case es_command_line_error:
        case es_internal_error:
          (void)fputc('C', f_raw_listing);
          break;
#if CHECKING
        case es_none:
        default:
          internal_error("write_diagnostic: bad severity (2)");
#endif /* CHECKING */
      }  /* switch */
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
      for (curr_seg = error_message_head;
           curr_seg->kind != (a_message_segment_kind)msk_last;
           curr_seg = curr_seg->next) {
        switch (curr_seg->kind) {
          case msk_error_text_part:
            write_message(curr_seg->variant.msg_part, curr_seg->length,
                          f_raw_listing, &line_len, /*wrap=*/FALSE);
            break;
          case msk_user_string:
            { char  *msg;

              if (curr_seg->variant.string.quoted) {
                msg = curr_seg->segment;
              } else {
                msg = error_msg_strings[curr_seg->sequence];
              }  /* if */
              write_message(msg, -1, f_raw_listing, &line_len, /*wrap=*/FALSE);
            }
            break;
          case msk_type:
          case msk_symbol:
            write_message(curr_seg->segment, -1, f_raw_listing,
                          &line_len, /*wrap=*/FALSE);
            break;
        }  /* switch */
      }  /* for */
      putc('\n', f_raw_listing);
    }  /* if */
  }  /* if */

  /* Terminate the compilation for the more serious severities. */
  if (severity == es_catastrophe || severity == es_command_line_error ||
      severity == es_internal_error) {
    term_compilation(severity);
  }  /* if */
#if IL_SHOULD_BE_WRITTEN_TO_FILE && !STANDALONE_UTILITY_PROGRAM
  /* If there are any errors, suppress generation of the intermediate language
     file. */
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
#else
    if (f_raw_listing != NULL) {
      fprintf(f_raw_listing, "C \"\" 0 0 error limit reached\n");
    }  /* if */
#endif /* !USING_DRIVER */
    term_compilation(es_catastrophe);
  }  /* if */
}  /* write_diagnostic */


#if CHECKING
void internal_error(char *error_message)
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
  write_diagnostic(&error_position, es_internal_error);
}  /* internal_error */
#endif /* CHECKING */


void str_command_line_error(char *error_message,
                            char *concat_string)
/*
Write a command-line error message concatenated with concat_string, and
terminate the compilation.
*/
{
  error_position.seq = 0;
  error_position.column = SP_COL_CMD_LINE;
  init_error_params();
  error_msg_strings[1] = error_message;
  error_msg_strings[2] = concat_string;
  construct_message_segments("%s1%s2");

  write_diagnostic(&error_position, es_command_line_error);
}  /* str_command_line_error */


void command_line_error(char *error_message)
/*
Write a command-line error message, and terminate the compilation.
*/
{
  str_command_line_error(error_message, "");
}  /* command_line_error */


static void diag_message (an_error_code     error_code,
                          a_source_position *error_pos,
                          an_error_severity severity)
/*
Construct a diagnostic message.  The error code is error_code, and the
position of the error is *error_pos.  severity gives the severity (e.g.,
es_warning).  The linked list of message segments that comprise the 
diagnostic is based on the error message template associated with error_code.
After constructing the segment list and doing any required expansions, the
diagnostic is written.
*/
{
  char		  *error_text_template;
  msg_segment_ptr curr_seg;

  /* Get the error message text (template). */
  error_text_template = error_text(error_code);
  construct_message_segments(error_text_template);

  /* Walk through the message segments and complete any required 
     expansion. */
  for (curr_seg = error_message_head;
       curr_seg->kind != (a_message_segment_kind)msk_last;
       curr_seg = curr_seg->next ) {
    switch (curr_seg->kind) {
      /* No processing is needed for msk_error_text_part. */

      case msk_user_string:
#if CHECKING
        if (error_msg_strings[curr_seg->sequence] == NULL) {
          internal_error("diag_message: missing string substitution");
        }  /* if */
#endif /* CHECKING */
        if (curr_seg->variant.string.quoted) {
          /* Rebuild the user string surrounded by double quotes. */
          add_string_to_segment("\"", curr_seg);
          add_string_to_segment(error_msg_strings[curr_seg->sequence],
                                curr_seg);
          add_string_to_segment("\"", curr_seg);
        }  /* if */
        break;
        
      case msk_type:
#if CHECKING
        if (error_msg_types[curr_seg->sequence] == NULL) {
          internal_error("diag_message: missing type substitution");
        }  /* if */
#endif /* CHECKING */
        form_type_summary(error_msg_types[curr_seg->sequence], curr_seg);
        break;
      case msk_symbol:
#if !STANDALONE_UTILITY_PROGRAM
#if CHECKING
        if (error_msg_syms[curr_seg->sequence] == NULL) {
          internal_error("diag_message: missing symbol substitution");
        }  /* if */
#endif /* CHECKING */
        form_symbol_name(error_msg_syms[curr_seg->sequence], curr_seg);
#endif /* !STANDALONE_UTILITY_PROGRAM */
        break;
    }  /* switch */
  }  /* for */

  write_diagnostic(error_pos, severity);
}  /* diag_message */


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
  diag_message(error_code, error_pos, es_remark);
}  /* pos_st_remark */


void pos_remark(an_error_code     error_code,
                a_source_position *error_pos)
/*
Report the indicated remark at the indicated position.
*/
{
  pos_st_remark(error_code, error_pos, (char *)NULL);
}  /* pos_remark */


void str_remark(an_error_code error_code,
                char          *error_string)
/*
Report the indicated remark (with the indicated fill-in string) at the
position indicated by error_position.
*/
{
  pos_st_remark(error_code, &error_position, error_string);
}  /* str_remark */


void remark(an_error_code error_code)
/*
Report the indicated remark at the position indicated by error_position.
*/
{
  pos_st_remark(error_code, &error_position, (char *)NULL);
}  /* remark */


void pos_ty_remark(an_error_code     error_code,
                   a_source_position *error_pos,
                   struct a_type     *type)
/*
Report the indicated remark (with the indicated type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, es_remark);
}  /* pos_ty_remark */


void type_remark(an_error_code error_code,
                 struct a_type *type)
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
                   struct a_symbol   *symbol)
/*
Report the indicated remark (with the indicated symbol) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_remark);
}  /* pos_sy_remark */


void sym_remark(an_error_code   error_code,
                struct a_symbol *symbol)
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
  diag_message(error_code, error_pos, es_warning);
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
                    struct a_type     *type)
/*
Report the indicated warning (with the indicated type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, es_warning);
}  /* pos_ty_warning */


void type_warning(an_error_code error_code,
                  struct a_type *type)
/*
Report the indicated warning (with the indicated type) at the position
indicated by error_position.
*/
{
  pos_ty_warning(error_code, &error_position, type);
}  /* type_warning */

#if !STANDALONE_UTILITY_PROGRAM

void pos_sy_warning(an_error_code     error_code,
                    a_source_position *error_pos,
                    struct a_symbol   *symbol)
/*
Report the indicated warning (with the indicated symbol) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_warning);
}  /* pos_sy_warning */


void sym_warning(an_error_code   error_code,
                 struct a_symbol *symbol)
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
  diag_message(error_code, error_pos, es_error);
}  /* pos_st_error */


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
                  struct a_type     *type)
/*
Report the indicated error (with the indicated type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, es_error);
}  /* pos_ty_error */


void type_error(an_error_code error_code,
                struct a_type *type)
/*
Report the indicated error (with the indicated type) at the position
indicated by error_position.
*/
{
  pos_ty_error(error_code, &error_position, type);
}  /* type_error */

#if !STANDALONE_UTILITY_PROGRAM

void pos_sy_error(an_error_code     error_code,
                  a_source_position *error_pos,
                  struct a_symbol   *symbol)
/*
Report the indicated error (with the indicated symbol) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_error);
}  /* pos_sy_error */


void sym_error(an_error_code   error_code,
               struct a_symbol *symbol)
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


void pos_st_catastrophe(an_error_code     error_code,
                        a_source_position *error_pos,
                        char              *error_string)
/*
Report the indicated catastrophic error (with the indicated fill-in string)
at the indicated position, and then terminate the compilation.
*/
{
  init_error_params();
  error_msg_strings[1] = error_string;
  diag_message(error_code, error_pos, es_catastrophe);
}  /* pos_st_catastrophe */


void str_catastrophe(an_error_code error_code,
                     char          *error_string)
/*
Report the indicated catastrophe (with the indicated fill-in string) at the
position indicated by error_position, and then terminate the compilation.
*/
{
  pos_st_catastrophe(error_code, &error_position, error_string);
}  /* str_catastrophe */


void catastrophe(an_error_code error_code)
/*
Report the indicated catastrophe at the position indicated by error_position,
and then terminate the compilation.
*/
{
  pos_st_catastrophe(error_code, &error_position, (char *)NULL);
}  /* catastrophe */


void pos_ty_catastrophe(an_error_code     error_code,
                        a_source_position *error_pos,
                        struct a_type     *type)
/*
Report the indicated catastrophe (with the indicated type) at the
indicated position.
*/
{
  init_error_params();
  error_msg_types[1] = type;
  diag_message(error_code, error_pos, es_catastrophe);
}  /* pos_ty_catastrophe */


void type_catastrophe(an_error_code error_code,
                      struct a_type *type)
/*
Report the indicated catastrophe (with the indicated type) at the position
indicated by error_position.
*/
{
  pos_ty_catastrophe(error_code, &error_position, type);
}  /* type_catastrophe */

#if !STANDALONE_UTILITY_PROGRAM

void pos_sy_catastrophe(an_error_code     error_code,
                        a_source_position *error_pos,
                        struct a_symbol   *symbol)
/*
Report the indicated catastrophe (with the indicated symbol) at the
indicated position.
*/
{
  init_error_params();
  error_msg_syms[1] = symbol;
  diag_message(error_code, error_pos, es_catastrophe);
}  /* pos_sy_catastrophe */


void sym_catastrophe(an_error_code   error_code,
                     struct a_symbol *symbol)
/*
Report the indicated catastrophe (with the indicated symbol) at the position
indicated by error_position.
*/
{
  pos_sy_catastrophe(error_code, &error_position, symbol);
}  /* sym_catastrophe */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
