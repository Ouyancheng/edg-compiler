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
#include "il.h"
#include "host_envir.h"
#include "cmd_line.h"

#if !STANDALONE_UTILITY_PROGRAM
#include "lexical.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if ASM_FUNCTION_ALLOWED
#include "asm_func.h"
#endif /* ASM_FUNCTION_ALLOWED */
#endif /* !STANDALONE_UTILITY_PROGRAM */


static char *error_text(an_error_code error_code)
/*
Return a pointer to the error text for the message identified by the given
error code.
*/
{
  char *m;

  switch (error_code) {
    case ec_last_line_incomplete:
      m = "last line of file ends without a newline";
      break;
    case ec_last_line_backslash:
      m = "last line of file ends with a backslash";
      break;
    case ec_include_recursion:
      m = "#include file \"%s\" includes itself";
      break;
    case ec_out_of_memory:
      m = "out of memory";
      break;
    case ec_source_file_could_not_be_opened:
      m = "could not open source file \"%s\"";
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
      m = "\"%s\" is not a file containing source text";
      break;
    case ec_illegal_source_file_name:
      m = "\"%s\" is not a valid source file name";
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
      m = "identifier \"%s\" is undefined";
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
      m = "incomplete type not allowed";
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
      m = "label \"%s\" has already been defined";
      break;
    case ec_label_never_defined:
      m = "label \"%s\" was referenced but not defined";
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
    case ec_type_must_be_compat_with_prev_def:
      m = "type must be compatible with previous declaration";
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
      m = "name conflicts with previously used external name \"%s\"";
      break;
    case ec_routine_definition_missing:
      m = "the definition for function \"%s\" is missing";
      break;
    case ec_unrecognized_pragma:
      m = "unrecognized #pragma";
      break;
    case ec_expr_not_scalar_or_void:
      m = "expression must have arithmetic, pointer, or void type";
      break;
    case ec_cannot_open_temp_file:
      m = "could not open temporary file \"%s\"";
      break;
    case ec_temp_file_dir_name_too_long:
      m = "name of directory for temporary files is too long (\"%s\")";
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
      m = "variable \"%s\" declared and never referenced";
      break;
    case ec_routine_declared_but_not_referenced:
      m = "routine \"%s\" declared and never referenced";
      break;
    case ec_label_declared_but_not_referenced:
      m = "label \"%s\" declared and never referenced";
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
      m = "parameter \"%s\" declared and never referenced";
      break;
    case ec_printf_arg_mismatch:
      m = "invalid argument type for format string conversion";
      break;
    case ec_empty_include_search_path:
      m = "could not open source file \"%s\" (no directories in search list)";
      break;
    case ec_cast_not_integral:
      m = "type of cast must be integral";
      break;
    case ec_cast_not_scalar:
      m = "type of cast must be arithmetic or pointer";
      break;
    case ec_initialization_not_reachable:
      m = "dynamic initialization in unreachable block";
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
      m = "variable \"%s\" declared with a never-completed type";
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
      m = "access control not specified (\"%s\" by default)";
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
      m = "member name is inaccessible";
      break;
    case ec_ambiguous_name:
      m = "member name is ambiguous";
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
      m = "name is not a member of a base class of \"%s\"";
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
      m = "qualfied name not equivalent to inherited member \"%s\"";
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
    case ec_id_has_same_name_as_class:
      m =
        "within a class, the class name may only be declared as a constructor";
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
    case ec_paren_initialization_not_allowed:
      m =
       "initialization with \"(...)\" is not allowed -- no constructor exists";
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
      m = "copy constructor for class \"%s\" is ambiguous";
      break;
    case ec_no_default_constructor:
      m = "no default constructor exists for class \"%s\"";
      break;
    case ec_not_a_field_or_base_class:
      m = "not a nonstatic data member or base class of class \"%s\"";
      break;
    case ec_indirect_nonvirtual_base_class_not_allowed:
      m = "indirect nonvirtual base class not allowed";
      break;
    case ec_no_constructor:
      m = "no constructor exists for class \"%s\"";
      break;
    case ec_bad_union_field:
      m = "invalid union member -- disallowed member function in class \"%s\"";
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
      m = "type does not match any instance of overloaded function \"%s\"";
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
      m = "redefinition of virtual function \"%s\" is ambiguous";
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
    case ec_inaccessible_constructor:
      m = "constructor \"%s\" is inaccessible";
      break;
    case ec_inaccessible_destructor:
      m = "destructor \"%s\" is inaccessible";
      break;
    case ec_direct_derivation_less_accessible:
      m =
       "direct path to base class \"%s\" gives less access than indirect path";
      break;
    case ec_missing_const_copy_constructor:
      m = "class \"%s\" has no copy constructor to copy a const object";
      break;
    case ec_definition_of_implicitly_declared_function:
      m = "defining an implicitly declared member function is not allowed";
      break;
    case ec_no_suitable_copy_constructor:
      m = "class \"%s\" has no suitable copy constructor";
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
      m =
      "more than one instance of overloaded function \"%s\" has \"C\" linkage";
      break;
    case ec_ambiguous_default_constructor:
      m = "more than one default constructor for class \"%s\"";
      break;
    case ec_temp_used_for_ref_init:
      m = "reference initialized to copy of initial value";
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
      m = "too many arguments for overloaded operator";
      break;
    case ec_too_few_args_for_operator:
      m = "too few arguments for overloaded operator";
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
    case ec_inaccessible_assignment_operator:
      m = "assignment operator \"%s\" is inaccessible";
      break;
    case ec_no_matching_operator_function:
      m = "none of the available operator functions matches these operands";
      break;
    case ec_ambiguous_operator_function:
      m = "more than one operator function matches these operands";
      break;
    case ec_inaccessible_conversion_function:
      m = "conversion function is inaccessible";
      break;
    case ec_bad_arg_type_for_operator_new:
      m = "operator new requires first argument of type \"size_t\"";
      break;
    case ec_bad_return_type_for_operator_new:
      m = "operator new requires return type of \"void*\"";
      break;
    case ec_bad_return_type_for_operator_delete:
      m = "operator delete requires return type of \"void\"";
      break;
    case ec_bad_first_arg_type_for_operator_delete:
      m = "operator delete requires first argument of type \"void*\"";
      break;
    case ec_bad_second_arg_type_for_operator_delete:
      m = "second argument of operator delete must be of type \"size_t\"";
      break;
    case ec_type_must_be_object_type:
      m = "type must be an object type";
      break;
    case ec_base_class_already_initialized:
      m = "base class \"%s\" has already been initialized";
      break;
    case ec_base_class_init_anachronism:
      m = "base class \"%s\" assumed (anachronism)";
      break;
    case ec_member_already_initialized:
      m = "member has already been initialized";
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


static void write_diagnostic(char              *msg1,
                             int               len1,
                             char              *msg2,
                             int               len2,
                             char              *msg3,
                             int               len3,
                             a_source_position *error_pos,
                             an_error_severity severity)
/*
Write out a diagnostic message with the given message string, position, and
severity.  If the error is severe, terminate the compilation.
The "message" to be put out is the concatenation of msg1, msg2, and msg3.
Each pointer can be NULL to indicate that that part of the message is
omitted.  For each part that is not omitted, the associated length (len1, len2,
or len3) gives the length in characters, or is -1 to indicate that the
string is null-terminated.
*/
{
  char          *severity_string, *file_name, *full_name;
  a_line_number line_number;
  a_boolean     at_end_of_source;
  a_boolean     capitalize_severity;
  a_boolean     column_needed;
#if !STANDALONE_UTILITY_PROGRAM
  a_boolean     source_text_needed = FALSE;
#endif /* !STANDALONE_UTILITY_PROGRAM */
  int           line_len = 0;
#define BUFFER_SIZE 200
  char          buffer[BUFFER_SIZE];

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
    if (msg2 != NULL || msg3 != NULL) {
      /* There's more than just msg1.  Try to put all three into
         "buffer", if there's room. */
      if (len1 < 0) len1 = (msg1 != NULL) ? strlen(msg1) : 0;
      if (len2 < 0) len2 = (msg2 != NULL) ? strlen(msg2) : 0;
      if (len3 < 0) len3 = (msg3 != NULL) ? strlen(msg3) : 0;
      if (len1 + len2 + len3 + 1 <= BUFFER_SIZE) {
        if (msg1 != NULL) memcpy(buffer, msg1, len1);
        if (msg2 != NULL) memcpy(buffer+len1, msg2, len2);
        if (msg3 != NULL) memcpy(buffer+len1+len2, msg3, len3);
        buffer[len1+len2+len3] = '\0';
        msg1 = buffer;
        len1 = len1 + len2 + len3;
        msg2 = msg3 = NULL;
        len2 = len3 = 0;
      }  /* if */
    }  /* if */
    write_message(msg1, len1, stderr, &line_len, /*wrap=*/TRUE);
    write_message(msg2, len2, stderr, &line_len, /*wrap=*/TRUE);
    write_message(msg3, len3, stderr, &line_len, /*wrap=*/TRUE);
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
      write_message(msg1, len1, f_raw_listing, &line_len, /*wrap=*/FALSE);
      write_message(msg2, len2, f_raw_listing, &line_len, /*wrap=*/FALSE);
      write_message(msg3, len3, f_raw_listing, &line_len, /*wrap=*/FALSE);
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
  write_diagnostic(error_message, -1, (char *)NULL, -1, (char *)NULL, -1,
                   &error_position, es_internal_error);
}  /* internal_error */
#endif /* CHECKING */


void str_command_line_error(char *error_message,
                            char *fill_in_string)
/*
Write a command-line error message with a fill-in string, and terminate the
compilation.
*/
{
  error_position.seq = 0;
  error_position.column = SP_COL_CMD_LINE;
  write_diagnostic(error_message, -1, fill_in_string, -1, (char *)NULL, -1,
                   &error_position, es_command_line_error);
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
                          an_error_severity severity,
                          char              *fill_in_string)
/*
Write out a diagnostic.  The error code is error_code, and the position
of the error is *error_pos.  severity gives the severity (e.g.,
es_warning), and fill_in_string, if non-NULL, provides text to replace a
%s in the error message text.
*/
{
  char *msg1 = NULL, *msg2 = NULL, *msg3 = NULL;
  int  len1  = -1,   len2  = -1,   len3  = -1;
  char *error_text_string, *percent_ptr;

  /* Get the error message text. */
  error_text_string = error_text(error_code);
  if (fill_in_string == NULL) {
    /* No fill-in, write the error as-is. */
    msg1 = error_text_string;
  } else {
    /* There is a fill-in string. */
    /* Find the position of "%s" in the text, then replace that with the
       fill-in string when writing it out. */
    percent_ptr = error_text_string;
    for (;;) {
      percent_ptr = strchr(percent_ptr, '%');
      if (percent_ptr == NULL || percent_ptr[1] == 's') break;
      percent_ptr++;
    }  /* for */
    if (percent_ptr != NULL) {
      /* Write the part before the "%s", the fill-in string,
         and the part after the "%s". */
      msg1 = error_text_string;
      len1 = percent_ptr - error_text_string;
      msg2 = fill_in_string;
      msg3 = percent_ptr+2;
    } else {
      /* If there is a fill-in string, but no "%s", put the fill-in
         string at the end of the message. */
      msg1 = error_text_string;
      msg2 = " -- ";
      msg3 = fill_in_string;
    }  /* if */
  }  /* if */
  write_diagnostic(msg1, len1, msg2, len2, msg3, len3, error_pos, severity);
}  /* diag_message */


void pos_st_remark(an_error_code     error_code,
                   a_source_position *error_pos,
                   char              *error_string)
/*
Report the indicated remark (with the indicated fill-in string) at the
indicated position.
*/
{
  diag_message(error_code, error_pos, es_remark, error_string);
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


void pos_st_warning(an_error_code     error_code,
                    a_source_position *error_pos,
                    char              *error_string)
/*
Report the indicated warning (with the indicated fill-in string) at the
indicated position.
*/
{
  diag_message(error_code, error_pos, es_warning, error_string);
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


void pos_st_error(an_error_code     error_code,
                  a_source_position *error_pos,
                  char              *error_string)
/*
Report the indicated error (with the indicated fill-in string) at the
indicated position.
*/
{
  diag_message(error_code, error_pos, es_error, error_string);
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
  diag_message(error_code, error_pos, es_catastrophe, error_string);
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


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
