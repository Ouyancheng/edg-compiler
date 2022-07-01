/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2022 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

ifc_map_functions.h -- Function declarations for interacting with IFC types for
                       Microsoft modules.

** NOTICE: This file is produced by an external script. **

While EDG staff should update the generation script rather than manually
editing this file, customers are welcome to modify this file and create patches
as they see fit.

Please contact EDG Support if you would be interested in using, or learning
more about, the tool that generated this file.
*/

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

extern a_boolean is_supported_ifc_version(an_ifc_version major_version,
                                          an_ifc_version minor_version);

/*
Functions for interacting with IFC AccessSort sorts.
*/

extern a_const_char* str_for(an_ifc_access_sort universal);

extern an_ifc_encoded_access_sort to_encoded(an_ifc_module      *mod,
                                             an_ifc_access_sort universal);

extern a_boolean validate_sort(an_ifc_access_sort_0_33       versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_access_sort to_universal_sort(an_ifc_access_sort_0_33 versioned);

/*
Functions for interacting with IFC ArchitectureSort sorts.
*/

extern a_const_char* str_for(an_ifc_architecture_sort universal);

extern an_ifc_encoded_architecture_sort to_encoded(
                                           an_ifc_module            *mod,
                                           an_ifc_architecture_sort universal);

extern a_boolean validate_sort(an_ifc_architecture_sort_0_33 versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_architecture_sort to_universal_sort(
                                      an_ifc_architecture_sort_0_33 versioned);

/*
Functions for interacting with IFC AttrSort sorts.
*/

extern a_const_char* str_for(an_ifc_attr_sort universal);

extern an_ifc_encoded_attr_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_attr_sort universal);

extern a_boolean validate_sort(an_ifc_attr_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_attr_sort to_universal_sort(an_ifc_attr_sort_0_33 versioned);

/*
Functions for interacting with IFC CallingConventionSort sorts.
*/

extern a_const_char* str_for(an_ifc_calling_convention_sort universal);

extern an_ifc_encoded_calling_convention_sort to_encoded(
                                     an_ifc_module                  *mod,
                                     an_ifc_calling_convention_sort universal);

extern a_boolean validate_sort(an_ifc_calling_convention_sort_0_33 versioned,
                               const an_ifc_validation_trace       *parent);

extern an_ifc_calling_convention_sort to_universal_sort(
                                an_ifc_calling_convention_sort_0_33 versioned);

/*
Functions for interacting with IFC ChartSort sorts.
*/

extern a_const_char* str_for(an_ifc_chart_sort universal);

extern an_ifc_encoded_chart_sort to_encoded(an_ifc_module     *mod,
                                            an_ifc_chart_sort universal);

extern a_boolean validate_sort(an_ifc_chart_sort_0_33        versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_chart_sort to_universal_sort(an_ifc_chart_sort_0_33 versioned);

/*
Functions for interacting with IFC DeclSort sorts.
*/

extern a_const_char* str_for(an_ifc_decl_sort universal);

extern an_ifc_encoded_decl_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_decl_sort universal);

extern a_boolean validate_sort(an_ifc_decl_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_decl_sort to_universal_sort(an_ifc_decl_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_decl_sort_0_41         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_decl_sort to_universal_sort(an_ifc_decl_sort_0_41 versioned);

/*
Functions for interacting with IFC DelimiterSort sorts.
*/

extern a_const_char* str_for(an_ifc_delimiter_sort universal);

extern an_ifc_encoded_delimiter_sort to_encoded(
                                              an_ifc_module         *mod,
                                              an_ifc_delimiter_sort universal);

extern a_boolean validate_sort(an_ifc_delimiter_sort_0_33    versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_delimiter_sort to_universal_sort(
                                         an_ifc_delimiter_sort_0_33 versioned);

/*
Functions for interacting with IFC DyadicOperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_dyadic_operator_sort universal);

extern an_ifc_encoded_dyadic_operator_sort to_encoded(
                                        an_ifc_module               *mod,
                                        an_ifc_dyadic_operator_sort universal);

extern a_boolean validate_sort(an_ifc_dyadic_operator_sort_0_33 versioned,
                               const an_ifc_validation_trace    *parent);

extern an_ifc_dyadic_operator_sort to_universal_sort(
                                   an_ifc_dyadic_operator_sort_0_33 versioned);

/*
Functions for interacting with IFC ExpansionModeSort sorts.
*/

extern a_const_char* str_for(an_ifc_expansion_mode_sort universal);

extern an_ifc_encoded_expansion_mode_sort to_encoded(
                                         an_ifc_module              *mod,
                                         an_ifc_expansion_mode_sort universal);

extern a_boolean validate_sort(an_ifc_expansion_mode_sort_0_33 versioned,
                               const an_ifc_validation_trace   *parent);

extern an_ifc_expansion_mode_sort to_universal_sort(
                                    an_ifc_expansion_mode_sort_0_33 versioned);

/*
Functions for interacting with IFC ExprSort sorts.
*/

extern a_const_char* str_for(an_ifc_expr_sort universal);

extern an_ifc_encoded_expr_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_expr_sort universal);

extern a_boolean validate_sort(an_ifc_expr_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_expr_sort to_universal_sort(an_ifc_expr_sort_0_33 versioned);

/*
Functions for interacting with IFC FoldDirectionSort sorts.
*/

extern a_const_char* str_for(an_ifc_fold_direction_sort universal);

extern an_ifc_encoded_fold_direction_sort to_encoded(
                                         an_ifc_module              *mod,
                                         an_ifc_fold_direction_sort universal);

extern a_boolean validate_sort(an_ifc_fold_direction_sort_0_33 versioned,
                               const an_ifc_validation_trace   *parent);

extern an_ifc_fold_direction_sort to_universal_sort(
                                    an_ifc_fold_direction_sort_0_33 versioned);

/*
Functions for interacting with IFC FormSort sorts.
*/

extern a_const_char* str_for(an_ifc_form_sort universal);

extern an_ifc_encoded_form_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_form_sort universal);

extern a_boolean validate_sort(an_ifc_form_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_form_sort to_universal_sort(an_ifc_form_sort_0_33 versioned);

/*
Functions for interacting with IFC InitializerSort sorts.
*/

extern a_const_char* str_for(an_ifc_initializer_sort universal);

extern an_ifc_encoded_initializer_sort to_encoded(
                                            an_ifc_module           *mod,
                                            an_ifc_initializer_sort universal);

extern a_boolean validate_sort(an_ifc_initializer_sort_0_33  versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_initializer_sort to_universal_sort(
                                       an_ifc_initializer_sort_0_33 versioned);

/*
Functions for interacting with IFC KeywordSort sorts.
*/

extern a_const_char* str_for(an_ifc_keyword_sort universal);

extern an_ifc_encoded_keyword_sort to_encoded(an_ifc_module       *mod,
                                              an_ifc_keyword_sort universal);

extern a_boolean validate_sort(an_ifc_keyword_sort_0_33      versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_keyword_sort to_universal_sort(
                                           an_ifc_keyword_sort_0_33 versioned);

/*
Functions for interacting with IFC LabelSort sorts.
*/

extern a_const_char* str_for(an_ifc_label_sort universal);

extern an_ifc_encoded_label_sort to_encoded(an_ifc_module     *mod,
                                            an_ifc_label_sort universal);

extern a_boolean validate_sort(an_ifc_label_sort_0_33        versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_label_sort to_universal_sort(an_ifc_label_sort_0_33 versioned);

/*
Functions for interacting with IFC LitSort sorts.
*/

extern a_const_char* str_for(an_ifc_lit_sort universal);

extern an_ifc_encoded_lit_sort to_encoded(an_ifc_module   *mod,
                                          an_ifc_lit_sort universal);

extern a_boolean validate_sort(an_ifc_lit_sort_0_33          versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_lit_sort to_universal_sort(an_ifc_lit_sort_0_33 versioned);

/*
Functions for interacting with IFC MacroSort sorts.
*/

extern a_const_char* str_for(an_ifc_macro_sort universal);

extern an_ifc_encoded_macro_sort to_encoded(an_ifc_module     *mod,
                                            an_ifc_macro_sort universal);

extern a_boolean validate_sort(an_ifc_macro_sort_0_33        versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_macro_sort to_universal_sort(an_ifc_macro_sort_0_33 versioned);

/*
Functions for interacting with IFC MonadicOperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_monadic_operator_sort universal);

extern an_ifc_encoded_monadic_operator_sort to_encoded(
                                       an_ifc_module                *mod,
                                       an_ifc_monadic_operator_sort universal);

extern a_boolean validate_sort(an_ifc_monadic_operator_sort_0_33 versioned,
                               const an_ifc_validation_trace     *parent);

extern an_ifc_monadic_operator_sort to_universal_sort(
                                  an_ifc_monadic_operator_sort_0_33 versioned);

/*
Functions for interacting with IFC NameSort sorts.
*/

extern a_const_char* str_for(an_ifc_name_sort universal);

extern an_ifc_encoded_name_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_name_sort universal);

extern a_boolean validate_sort(an_ifc_name_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_name_sort to_universal_sort(an_ifc_name_sort_0_33 versioned);

/*
Functions for interacting with IFC NiladicOperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_niladic_operator_sort universal);

extern an_ifc_encoded_niladic_operator_sort to_encoded(
                                       an_ifc_module                *mod,
                                       an_ifc_niladic_operator_sort universal);

extern a_boolean validate_sort(an_ifc_niladic_operator_sort_0_33 versioned,
                               const an_ifc_validation_trace     *parent);

extern an_ifc_niladic_operator_sort to_universal_sort(
                                  an_ifc_niladic_operator_sort_0_33 versioned);

/*
Functions for interacting with IFC NoexceptSort sorts.
*/

extern a_const_char* str_for(an_ifc_noexcept_sort universal);

extern an_ifc_encoded_noexcept_sort to_encoded(an_ifc_module        *mod,
                                               an_ifc_noexcept_sort universal);

extern a_boolean validate_sort(an_ifc_noexcept_sort_0_33     versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_noexcept_sort to_universal_sort(
                                          an_ifc_noexcept_sort_0_33 versioned);

/*
Functions for interacting with IFC OperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_operator_sort universal);

extern an_ifc_encoded_operator_sort to_encoded(an_ifc_module        *mod,
                                               an_ifc_operator_sort universal);

extern a_boolean validate_sort(an_ifc_operator_sort_0_33     versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_operator_sort to_universal_sort(
                                          an_ifc_operator_sort_0_33 versioned);

/*
Functions for interacting with IFC ParameterSort sorts.
*/

extern a_const_char* str_for(an_ifc_parameter_sort universal);

extern an_ifc_encoded_parameter_sort to_encoded(
                                              an_ifc_module         *mod,
                                              an_ifc_parameter_sort universal);

extern a_boolean validate_sort(an_ifc_parameter_sort_0_33    versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_parameter_sort to_universal_sort(
                                         an_ifc_parameter_sort_0_33 versioned);

/*
Functions for interacting with IFC PointerDeclaratorSort sorts.
*/

extern a_const_char* str_for(an_ifc_pointer_declarator_sort universal);

extern an_ifc_encoded_pointer_declarator_sort to_encoded(
                                     an_ifc_module                  *mod,
                                     an_ifc_pointer_declarator_sort universal);

extern a_boolean validate_sort(an_ifc_pointer_declarator_sort_0_33 versioned,
                               const an_ifc_validation_trace       *parent);

extern an_ifc_pointer_declarator_sort to_universal_sort(
                                an_ifc_pointer_declarator_sort_0_33 versioned);

/*
Functions for interacting with IFC PragmaSort sorts.
*/

extern a_const_char* str_for(an_ifc_pragma_sort universal);

extern an_ifc_encoded_pragma_sort to_encoded(an_ifc_module      *mod,
                                             an_ifc_pragma_sort universal);

extern a_boolean validate_sort(an_ifc_pragma_sort_0_33       versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_pragma_sort to_universal_sort(an_ifc_pragma_sort_0_33 versioned);

/*
Functions for interacting with IFC ReadConversionSort sorts.
*/

extern a_const_char* str_for(an_ifc_read_conversion_sort universal);

extern an_ifc_encoded_read_conversion_sort to_encoded(
                                        an_ifc_module               *mod,
                                        an_ifc_read_conversion_sort universal);

extern a_boolean validate_sort(an_ifc_read_conversion_sort_0_33 versioned,
                               const an_ifc_validation_trace    *parent);

extern an_ifc_read_conversion_sort to_universal_sort(
                                   an_ifc_read_conversion_sort_0_33 versioned);

/*
Functions for interacting with IFC ReturnSort sorts.
*/

extern a_const_char* str_for(an_ifc_return_sort universal);

extern an_ifc_encoded_return_sort to_encoded(an_ifc_module      *mod,
                                             an_ifc_return_sort universal);

extern a_boolean validate_sort(an_ifc_return_sort_0_33       versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_return_sort to_universal_sort(an_ifc_return_sort_0_33 versioned);

/*
Functions for interacting with IFC SourceDirectiveSort sorts.
*/

extern a_const_char* str_for(an_ifc_source_directive_sort universal);

extern an_ifc_encoded_source_directive_sort to_encoded(
                                       an_ifc_module                *mod,
                                       an_ifc_source_directive_sort universal);

extern a_boolean validate_sort(an_ifc_source_directive_sort_0_33 versioned,
                               const an_ifc_validation_trace     *parent);

extern an_ifc_source_directive_sort to_universal_sort(
                                  an_ifc_source_directive_sort_0_33 versioned);

/*
Functions for interacting with IFC SourceIdentifierSort sorts.
*/

extern a_const_char* str_for(an_ifc_source_identifier_sort universal);

extern an_ifc_encoded_source_identifier_sort to_encoded(
                                      an_ifc_module                 *mod,
                                      an_ifc_source_identifier_sort universal);

extern a_boolean validate_sort(an_ifc_source_identifier_sort_0_33 versioned,
                               const an_ifc_validation_trace      *parent);

extern an_ifc_source_identifier_sort to_universal_sort(
                                 an_ifc_source_identifier_sort_0_33 versioned);

/*
Functions for interacting with IFC SourceKeywordSort sorts.
*/

extern a_const_char* str_for(an_ifc_source_keyword_sort universal);

extern an_ifc_encoded_source_keyword_sort to_encoded(
                                         an_ifc_module              *mod,
                                         an_ifc_source_keyword_sort universal);

extern a_boolean validate_sort(an_ifc_source_keyword_sort_0_33 versioned,
                               const an_ifc_validation_trace   *parent);

extern an_ifc_source_keyword_sort to_universal_sort(
                                    an_ifc_source_keyword_sort_0_33 versioned);

/*
Functions for interacting with IFC SourceLiteralSort sorts.
*/

extern a_const_char* str_for(an_ifc_source_literal_sort universal);

extern an_ifc_encoded_source_literal_sort to_encoded(
                                         an_ifc_module              *mod,
                                         an_ifc_source_literal_sort universal);

extern a_boolean validate_sort(an_ifc_source_literal_sort_0_33 versioned,
                               const an_ifc_validation_trace   *parent);

extern an_ifc_source_literal_sort to_universal_sort(
                                    an_ifc_source_literal_sort_0_33 versioned);

/*
Functions for interacting with IFC SourceOperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_source_operator_sort universal);

extern an_ifc_encoded_source_operator_sort to_encoded(
                                        an_ifc_module               *mod,
                                        an_ifc_source_operator_sort universal);

extern a_boolean validate_sort(an_ifc_source_operator_sort_0_33 versioned,
                               const an_ifc_validation_trace    *parent);

extern an_ifc_source_operator_sort to_universal_sort(
                                   an_ifc_source_operator_sort_0_33 versioned);

/*
Functions for interacting with IFC SourcePunctuatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_source_punctuator_sort universal);

extern an_ifc_encoded_source_punctuator_sort to_encoded(
                                      an_ifc_module                 *mod,
                                      an_ifc_source_punctuator_sort universal);

extern a_boolean validate_sort(an_ifc_source_punctuator_sort_0_33 versioned,
                               const an_ifc_validation_trace      *parent);

extern an_ifc_source_punctuator_sort to_universal_sort(
                                 an_ifc_source_punctuator_sort_0_33 versioned);

/*
Functions for interacting with IFC SpecializationSort sorts.
*/

extern a_const_char* str_for(an_ifc_specialization_sort universal);

extern an_ifc_encoded_specialization_sort to_encoded(
                                         an_ifc_module              *mod,
                                         an_ifc_specialization_sort universal);

extern a_boolean validate_sort(an_ifc_specialization_sort_0_33 versioned,
                               const an_ifc_validation_trace   *parent);

extern an_ifc_specialization_sort to_universal_sort(
                                    an_ifc_specialization_sort_0_33 versioned);

/*
Functions for interacting with IFC StmtSort sorts.
*/

extern a_const_char* str_for(an_ifc_stmt_sort universal);

extern an_ifc_encoded_stmt_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_stmt_sort universal);

extern a_boolean validate_sort(an_ifc_stmt_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_stmt_sort to_universal_sort(an_ifc_stmt_sort_0_33 versioned);

/*
Functions for interacting with IFC StorageInstructionOperatorSort sorts.
*/

extern a_const_char* str_for(
                           an_ifc_storage_instruction_operator_sort universal);

extern an_ifc_encoded_storage_instruction_operator_sort to_encoded(
                           an_ifc_module                            *mod,
                           an_ifc_storage_instruction_operator_sort universal);

extern a_boolean validate_sort(
                      an_ifc_storage_instruction_operator_sort_0_33 versioned,
                      const an_ifc_validation_trace                 *parent);

extern an_ifc_storage_instruction_operator_sort to_universal_sort(
                      an_ifc_storage_instruction_operator_sort_0_33 versioned);

/*
Functions for interacting with IFC StringSort sorts.
*/

extern a_const_char* str_for(an_ifc_string_sort universal);

extern an_ifc_encoded_string_sort to_encoded(an_ifc_module      *mod,
                                             an_ifc_string_sort universal);

extern a_boolean validate_sort(an_ifc_string_sort_0_33       versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_string_sort to_universal_sort(an_ifc_string_sort_0_33 versioned);

/*
Functions for interacting with IFC SyntaxSort sorts.
*/

extern a_const_char* str_for(an_ifc_syntax_sort universal);

extern an_ifc_encoded_syntax_sort to_encoded(an_ifc_module      *mod,
                                             an_ifc_syntax_sort universal);

extern a_boolean validate_sort(an_ifc_syntax_sort_0_33       versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_syntax_sort to_universal_sort(an_ifc_syntax_sort_0_33 versioned);

/*
Functions for interacting with IFC TriadicOperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_triadic_operator_sort universal);

extern an_ifc_encoded_triadic_operator_sort to_encoded(
                                       an_ifc_module                *mod,
                                       an_ifc_triadic_operator_sort universal);

extern a_boolean validate_sort(an_ifc_triadic_operator_sort_0_33 versioned,
                               const an_ifc_validation_trace     *parent);

extern an_ifc_triadic_operator_sort to_universal_sort(
                                  an_ifc_triadic_operator_sort_0_33 versioned);

/*
Functions for interacting with IFC TypeBasisSort sorts.
*/

extern a_const_char* str_for(an_ifc_type_basis_sort universal);

extern an_ifc_encoded_type_basis_sort to_encoded(
                                             an_ifc_module          *mod,
                                             an_ifc_type_basis_sort universal);

extern a_boolean validate_sort(an_ifc_type_basis_sort_0_33   versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_type_basis_sort to_universal_sort(
                                        an_ifc_type_basis_sort_0_33 versioned);

/*
Functions for interacting with IFC TypePrecisionSort sorts.
*/

extern a_const_char* str_for(an_ifc_type_precision_sort universal);

extern an_ifc_encoded_type_precision_sort to_encoded(
                                         an_ifc_module              *mod,
                                         an_ifc_type_precision_sort universal);

extern a_boolean validate_sort(an_ifc_type_precision_sort_0_33 versioned,
                               const an_ifc_validation_trace   *parent);

extern an_ifc_type_precision_sort to_universal_sort(
                                    an_ifc_type_precision_sort_0_33 versioned);

/*
Functions for interacting with IFC TypeSignSort sorts.
*/

extern a_const_char* str_for(an_ifc_type_sign_sort universal);

extern an_ifc_encoded_type_sign_sort to_encoded(
                                              an_ifc_module         *mod,
                                              an_ifc_type_sign_sort universal);

extern a_boolean validate_sort(an_ifc_type_sign_sort_0_33    versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_type_sign_sort to_universal_sort(
                                         an_ifc_type_sign_sort_0_33 versioned);

/*
Functions for interacting with IFC TypeSort sorts.
*/

extern a_const_char* str_for(an_ifc_type_sort universal);

extern an_ifc_encoded_type_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_type_sort universal);

extern a_boolean validate_sort(an_ifc_type_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_type_sort to_universal_sort(an_ifc_type_sort_0_33 versioned);

/*
Functions for interacting with IFC UnitSort sorts.
*/

extern a_const_char* str_for(an_ifc_unit_sort universal);

extern an_ifc_encoded_unit_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_unit_sort universal);

extern a_boolean validate_sort(an_ifc_unit_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_unit_sort to_universal_sort(an_ifc_unit_sort_0_33 versioned);

/*
Functions for interacting with IFC VariadicOperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_variadic_operator_sort universal);

extern an_ifc_encoded_variadic_operator_sort to_encoded(
                                      an_ifc_module                 *mod,
                                      an_ifc_variadic_operator_sort universal);

extern a_boolean validate_sort(an_ifc_variadic_operator_sort_0_33 versioned,
                               const an_ifc_validation_trace      *parent);

extern an_ifc_variadic_operator_sort to_universal_sort(
                                 an_ifc_variadic_operator_sort_0_33 versioned);

/*
Functions for interacting with IFC WordSort sorts.
*/

extern a_const_char* str_for(an_ifc_word_sort universal);

extern an_ifc_encoded_word_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_word_sort universal);

extern a_boolean validate_sort(an_ifc_word_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_word_sort to_universal_sort(an_ifc_word_sort_0_33 versioned);

/*
Functions for interacting with IFC AttrIndex indexes.
*/

extern an_ifc_attr_sort_0_33 attr_sort(an_ifc_attr_index_0_33 versioned);

extern uint32_t attr_value(an_ifc_attr_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_attr_index_0_33        versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_attr_index to_universal_index(an_ifc_module          *mod,
                                            an_ifc_attr_index_0_33 versioned);

extern an_ifc_encoded_attr_index to_encoded(an_ifc_module     *mod,
                                            an_ifc_attr_index universal);

extern a_boolean is_null_index(an_ifc_attr_index universal);

/*
Functions for interacting with IFC ChartIndex indexes.
*/

extern an_ifc_chart_sort_0_33 chart_sort(an_ifc_chart_index_0_33 versioned);

extern uint32_t chart_value(an_ifc_chart_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_chart_index_0_33       versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_chart_index to_universal_index(
                                            an_ifc_module           *mod,
                                            an_ifc_chart_index_0_33 versioned);

extern an_ifc_encoded_chart_index to_encoded(an_ifc_module      *mod,
                                             an_ifc_chart_index universal);

extern a_boolean is_null_index(an_ifc_chart_index universal);

/*
Functions for interacting with IFC DeclIndex indexes.
*/

extern an_ifc_decl_sort_0_33 decl_sort(an_ifc_decl_index_0_33 versioned);

extern uint32_t decl_value(an_ifc_decl_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_decl_index_0_33        versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_decl_index to_universal_index(an_ifc_module          *mod,
                                            an_ifc_decl_index_0_33 versioned);

extern an_ifc_decl_sort_0_41 decl_sort(an_ifc_decl_index_0_41 versioned);

extern uint32_t decl_value(an_ifc_decl_index_0_41 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_decl_index_0_41        versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_decl_index to_universal_index(an_ifc_module          *mod,
                                            an_ifc_decl_index_0_41 versioned);

extern an_ifc_encoded_decl_index to_encoded(an_ifc_module     *mod,
                                            an_ifc_decl_index universal);

extern a_boolean is_null_index(an_ifc_decl_index universal);

/*
Functions for interacting with IFC ExprIndex indexes.
*/

extern an_ifc_expr_sort_0_33 expr_sort(an_ifc_expr_index_0_33 versioned);

extern uint32_t expr_value(an_ifc_expr_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_expr_index_0_33        versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_expr_index to_universal_index(an_ifc_module          *mod,
                                            an_ifc_expr_index_0_33 versioned);

extern an_ifc_encoded_expr_index to_encoded(an_ifc_module     *mod,
                                            an_ifc_expr_index universal);

extern a_boolean is_null_index(an_ifc_expr_index universal);

/*
Functions for interacting with IFC FormIndex indexes.
*/

extern an_ifc_form_sort_0_33 form_sort(an_ifc_form_index_0_33 versioned);

extern uint32_t form_value(an_ifc_form_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_form_index_0_33        versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_form_index to_universal_index(an_ifc_module          *mod,
                                            an_ifc_form_index_0_33 versioned);

extern an_ifc_encoded_form_index to_encoded(an_ifc_module     *mod,
                                            an_ifc_form_index universal);

extern a_boolean is_null_index(an_ifc_form_index universal);

/*
Functions for interacting with IFC LitIndex indexes.
*/

extern an_ifc_lit_sort_0_33 lit_sort(an_ifc_lit_index_0_33 versioned);

extern uint32_t lit_value(an_ifc_lit_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_lit_index_0_33         versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_lit_index to_universal_index(an_ifc_module         *mod,
                                           an_ifc_lit_index_0_33 versioned);

extern an_ifc_encoded_lit_index to_encoded(an_ifc_module    *mod,
                                           an_ifc_lit_index universal);

extern a_boolean is_null_index(an_ifc_lit_index universal);

/*
Functions for interacting with IFC MacroIndex indexes.
*/

extern an_ifc_macro_sort_0_33 macro_sort(an_ifc_macro_index_0_33 versioned);

extern uint32_t macro_value(an_ifc_macro_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_macro_index_0_33       versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_macro_index to_universal_index(
                                            an_ifc_module           *mod,
                                            an_ifc_macro_index_0_33 versioned);

extern an_ifc_encoded_macro_index to_encoded(an_ifc_module      *mod,
                                             an_ifc_macro_index universal);

extern a_boolean is_null_index(an_ifc_macro_index universal);

/*
Functions for interacting with IFC NameIndex indexes.
*/

extern an_ifc_name_sort_0_33 name_sort(an_ifc_name_index_0_33 versioned);

extern uint32_t name_value(an_ifc_name_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_name_index_0_33        versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_name_index to_universal_index(an_ifc_module          *mod,
                                            an_ifc_name_index_0_33 versioned);

extern an_ifc_encoded_name_index to_encoded(an_ifc_module     *mod,
                                            an_ifc_name_index universal);

extern a_boolean is_null_index(an_ifc_name_index universal);

/*
Functions for interacting with IFC PragmaIndex indexes.
*/

extern an_ifc_pragma_sort_0_33 pragma_sort(an_ifc_pragma_index_0_33 versioned);

extern uint32_t pragma_value(an_ifc_pragma_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_pragma_index_0_33      versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_pragma_index to_universal_index(
                                           an_ifc_module            *mod,
                                           an_ifc_pragma_index_0_33 versioned);

extern an_ifc_encoded_pragma_index to_encoded(an_ifc_module       *mod,
                                              an_ifc_pragma_index universal);

extern a_boolean is_null_index(an_ifc_pragma_index universal);

/*
Functions for interacting with IFC StmtIndex indexes.
*/

extern an_ifc_stmt_sort_0_33 stmt_sort(an_ifc_stmt_index_0_33 versioned);

extern uint32_t stmt_value(an_ifc_stmt_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_stmt_index_0_33        versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_stmt_index to_universal_index(an_ifc_module          *mod,
                                            an_ifc_stmt_index_0_33 versioned);

extern an_ifc_encoded_stmt_index to_encoded(an_ifc_module     *mod,
                                            an_ifc_stmt_index universal);

extern a_boolean is_null_index(an_ifc_stmt_index universal);

/*
Functions for interacting with IFC StringIndex indexes.
*/

extern an_ifc_string_sort_0_33 string_sort(an_ifc_string_index_0_33 versioned);

extern uint32_t string_value(an_ifc_string_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_string_index_0_33      versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_string_index to_universal_index(
                                           an_ifc_module            *mod,
                                           an_ifc_string_index_0_33 versioned);

extern an_ifc_encoded_string_index to_encoded(an_ifc_module       *mod,
                                              an_ifc_string_index universal);

extern a_boolean is_null_index(an_ifc_string_index universal);

/*
Functions for interacting with IFC SyntaxIndex indexes.
*/

extern an_ifc_syntax_sort_0_33 syntax_sort(an_ifc_syntax_index_0_33 versioned);

extern uint32_t syntax_value(an_ifc_syntax_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_syntax_index_0_33      versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_syntax_index to_universal_index(
                                           an_ifc_module            *mod,
                                           an_ifc_syntax_index_0_33 versioned);

extern an_ifc_encoded_syntax_index to_encoded(an_ifc_module       *mod,
                                              an_ifc_syntax_index universal);

extern a_boolean is_null_index(an_ifc_syntax_index universal);

/*
Functions for interacting with IFC TypeIndex indexes.
*/

extern an_ifc_type_sort_0_33 type_sort(an_ifc_type_index_0_33 versioned);

extern uint32_t type_value(an_ifc_type_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_type_index_0_33        versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_type_index to_universal_index(an_ifc_module          *mod,
                                            an_ifc_type_index_0_33 versioned);

extern an_ifc_encoded_type_index to_encoded(an_ifc_module     *mod,
                                            an_ifc_type_index universal);

extern a_boolean is_null_index(an_ifc_type_index universal);

/*
Functions for interacting with IFC UnitIndex indexes.
*/

extern an_ifc_unit_sort_0_33 unit_sort(an_ifc_unit_index_0_33 versioned);

extern uint32_t unit_value(an_ifc_unit_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_unit_index_0_33        versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_unit_index to_universal_index(an_ifc_module          *mod,
                                            an_ifc_unit_index_0_33 versioned);

extern an_ifc_encoded_unit_index to_encoded(an_ifc_module     *mod,
                                            an_ifc_unit_index universal);

extern a_boolean is_null_index(an_ifc_unit_index universal);

/*
Functions for interacting with IFC DeclForeignIndex indexes.
*/

a_boolean validate_index(an_ifc_module                  *mod,
                         an_ifc_decl_foreign_index_0_33 versioned,
                         const an_ifc_validation_trace  *parent);

extern an_ifc_decl_index to_universal_index(
                                     an_ifc_module                  *mod,
                                     an_ifc_decl_foreign_index_0_33 versioned);

extern a_boolean validate_index(an_ifc_module                 *foreign_mod,
                                an_ifc_decl_foreign_index     universal,
                                const an_ifc_validation_trace *parent);

extern an_ifc_decl_index to_universal_index(
                                       an_ifc_module             *foreign_mod,
                                       an_ifc_decl_foreign_index universal);

/*
Functions for interacting with IFC BasicSpecifiersBitfield bitfields.
*/


constexpr an_ifc_basic_specifiers_bitfield_query operator|(
                             const an_ifc_basic_specifiers_bitfield_query &lhs,
                             const an_ifc_basic_specifiers_bitfield_query &rhs)
/*
Perform a bitor operation on the given arguments.
*/
{
  return (an_ifc_basic_specifiers_bitfield_query)(
                                               (uint64_t)lhs | (uint64_t)rhs);

}  /* operator| */


extern a_boolean is_null_bitfield(an_ifc_basic_specifiers_bitfield universal);

extern uint8_t to_bitmask_0_33(an_ifc_basic_specifiers_bitfield_query query);

template<an_ifc_basic_specifiers_bitfield_query a_Query>
a_boolean test_bitmask(const an_ifc_basic_specifiers_bitfield &universal)
/*
Given the universal representation of BasicSpecifiersBitfield, return TRUE if
the universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  result = universal.value & to_bitmask_0_33(a_Query);
  return result;
}  /* test_bitmask */

/*
Functions for interacting with IFC FunctionTraitsBitfield bitfields.
*/


constexpr an_ifc_function_traits_bitfield_query operator|(
                              const an_ifc_function_traits_bitfield_query &lhs,
                              const an_ifc_function_traits_bitfield_query &rhs)
/*
Perform a bitor operation on the given arguments.
*/
{
  return (an_ifc_function_traits_bitfield_query)(
                                               (uint64_t)lhs | (uint64_t)rhs);

}  /* operator| */


extern a_boolean is_null_bitfield(an_ifc_function_traits_bitfield universal);

extern uint16_t to_bitmask_0_33(an_ifc_function_traits_bitfield_query query);

template<an_ifc_function_traits_bitfield_query a_Query>
a_boolean test_bitmask(const an_ifc_function_traits_bitfield &universal)
/*
Given the universal representation of FunctionTraitsBitfield, return TRUE if
the universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  result = universal.value & to_bitmask_0_33(a_Query);
  return result;
}  /* test_bitmask */

/*
Functions for interacting with IFC FunctionTypeTraitsBitfield bitfields.
*/


constexpr an_ifc_function_type_traits_bitfield_query operator|(
                         const an_ifc_function_type_traits_bitfield_query &lhs,
                         const an_ifc_function_type_traits_bitfield_query &rhs)
/*
Perform a bitor operation on the given arguments.
*/
{
  return (an_ifc_function_type_traits_bitfield_query)(
                                               (uint64_t)lhs | (uint64_t)rhs);

}  /* operator| */


extern a_boolean is_null_bitfield(
                               an_ifc_function_type_traits_bitfield universal);

extern uint8_t to_bitmask_0_33(
                             an_ifc_function_type_traits_bitfield_query query);

template<an_ifc_function_type_traits_bitfield_query a_Query>
a_boolean test_bitmask(const an_ifc_function_type_traits_bitfield &universal)
/*
Given the universal representation of FunctionTypeTraitsBitfield, return TRUE
if the universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  result = universal.value & to_bitmask_0_33(a_Query);
  return result;
}  /* test_bitmask */

/*
Functions for interacting with IFC MsvcTraitsBitfield bitfields.
*/


constexpr an_ifc_msvc_traits_bitfield_query operator|(
                                  const an_ifc_msvc_traits_bitfield_query &lhs,
                                  const an_ifc_msvc_traits_bitfield_query &rhs)
/*
Perform a bitor operation on the given arguments.
*/
{
  return (an_ifc_msvc_traits_bitfield_query)((uint64_t)lhs | (uint64_t)rhs);

}  /* operator| */


extern a_boolean is_null_bitfield(an_ifc_msvc_traits_bitfield universal);

extern uint32_t to_bitmask_0_33(an_ifc_msvc_traits_bitfield_query query);

template<an_ifc_msvc_traits_bitfield_query a_Query>
a_boolean test_bitmask(const an_ifc_msvc_traits_bitfield &universal)
/*
Given the universal representation of MsvcTraitsBitfield, return TRUE if the
universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  result = universal.value & to_bitmask_0_33(a_Query);
  return result;
}  /* test_bitmask */

/*
Functions for interacting with IFC ObjectTraitsBitfield bitfields.
*/


constexpr an_ifc_object_traits_bitfield_query operator|(
                                const an_ifc_object_traits_bitfield_query &lhs,
                                const an_ifc_object_traits_bitfield_query &rhs)
/*
Perform a bitor operation on the given arguments.
*/
{
  return (an_ifc_object_traits_bitfield_query)((uint64_t)lhs | (uint64_t)rhs);

}  /* operator| */


extern a_boolean is_null_bitfield(an_ifc_object_traits_bitfield universal);

extern uint8_t to_bitmask_0_33(an_ifc_object_traits_bitfield_query query);

template<an_ifc_object_traits_bitfield_query a_Query>
a_boolean test_bitmask(const an_ifc_object_traits_bitfield &universal)
/*
Given the universal representation of ObjectTraitsBitfield, return TRUE if the
universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  result = universal.value & to_bitmask_0_33(a_Query);
  return result;
}  /* test_bitmask */

/*
Functions for interacting with IFC QualifierBitfield bitfields.
*/


constexpr an_ifc_qualifier_bitfield_query operator|(
                                    const an_ifc_qualifier_bitfield_query &lhs,
                                    const an_ifc_qualifier_bitfield_query &rhs)
/*
Perform a bitor operation on the given arguments.
*/
{
  return (an_ifc_qualifier_bitfield_query)((uint64_t)lhs | (uint64_t)rhs);

}  /* operator| */


extern a_boolean is_null_bitfield(an_ifc_qualifier_bitfield universal);

extern uint8_t to_bitmask_0_33(an_ifc_qualifier_bitfield_query query);

template<an_ifc_qualifier_bitfield_query a_Query>
a_boolean test_bitmask(const an_ifc_qualifier_bitfield &universal)
/*
Given the universal representation of QualifierBitfield, return TRUE if the
universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  result = universal.value & to_bitmask_0_33(a_Query);
  return result;
}  /* test_bitmask */

/*
Functions for interacting with IFC ReachablePropertiesBitfield bitfields.
*/


constexpr an_ifc_reachable_properties_bitfield_query operator|(
                         const an_ifc_reachable_properties_bitfield_query &lhs,
                         const an_ifc_reachable_properties_bitfield_query &rhs)
/*
Perform a bitor operation on the given arguments.
*/
{
  return (an_ifc_reachable_properties_bitfield_query)(
                                               (uint64_t)lhs | (uint64_t)rhs);

}  /* operator| */


extern a_boolean is_null_bitfield(
                               an_ifc_reachable_properties_bitfield universal);

extern uint8_t to_bitmask_0_33(
                             an_ifc_reachable_properties_bitfield_query query);

template<an_ifc_reachable_properties_bitfield_query a_Query>
a_boolean test_bitmask(const an_ifc_reachable_properties_bitfield &universal)
/*
Given the universal representation of ReachablePropertiesBitfield, return TRUE
if the universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  result = universal.value & to_bitmask_0_33(a_Query);
  return result;
}  /* test_bitmask */

/*
Functions for interacting with IFC ScopeTraitsBitfield bitfields.
*/


constexpr an_ifc_scope_traits_bitfield_query operator|(
                                 const an_ifc_scope_traits_bitfield_query &lhs,
                                 const an_ifc_scope_traits_bitfield_query &rhs)
/*
Perform a bitor operation on the given arguments.
*/
{
  return (an_ifc_scope_traits_bitfield_query)((uint64_t)lhs | (uint64_t)rhs);

}  /* operator| */


extern a_boolean is_null_bitfield(an_ifc_scope_traits_bitfield universal);

extern uint8_t to_bitmask_0_33(an_ifc_scope_traits_bitfield_query query);

template<an_ifc_scope_traits_bitfield_query a_Query>
a_boolean test_bitmask(const an_ifc_scope_traits_bitfield &universal)
/*
Given the universal representation of ScopeTraitsBitfield, return TRUE if the
universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  result = universal.value & to_bitmask_0_33(a_Query);
  return result;
}  /* test_bitmask */

/*
Functions for interacting with IFC OperatorCategory indexes.
*/

extern an_ifc_operator_sort_0_33 operator_sort(
                                      an_ifc_operator_category_0_33 versioned);

extern uint16_t operator_value(an_ifc_operator_category_0_33 versioned);

extern a_boolean validate_category(an_ifc_module                 *mod,
                                   an_ifc_operator_category_0_33 versioned,
                                   const an_ifc_validation_trace *parent);

extern an_ifc_operator_category to_universal_category(
                                      an_ifc_module                 *mod,
                                      an_ifc_operator_category_0_33 versioned);

/*
Functions for interacting with IFC SourceIdentifierCategory indexes.
*/

extern an_ifc_source_identifier_sort_0_33 source_identifier_sort(
                             an_ifc_source_identifier_category_0_33 versioned);

extern uint64_t source_identifier_value(
                             an_ifc_source_identifier_category_0_33 versioned);

extern a_boolean validate_category(
                             an_ifc_module                          *mod,
                             an_ifc_source_identifier_category_0_33 versioned,
                             const an_ifc_validation_trace          *parent);

extern an_ifc_source_identifier_category to_universal_category(
                             an_ifc_module                          *mod,
                             an_ifc_source_identifier_category_0_33 versioned);

/*
Functions for interacting with IFC SourceLiteralCategory indexes.
*/

extern an_ifc_source_literal_sort_0_33 source_literal_sort(
                                an_ifc_source_literal_category_0_33 versioned);

extern uint64_t source_literal_value(
                                an_ifc_source_literal_category_0_33 versioned);

extern a_boolean validate_category(
                                an_ifc_module                       *mod,
                                an_ifc_source_literal_category_0_33 versioned,
                                const an_ifc_validation_trace       *parent);

extern an_ifc_source_literal_category to_universal_category(
                                an_ifc_module                       *mod,
                                an_ifc_source_literal_category_0_33 versioned);

/*
Functions for interacting with IFC WordCategory indexes.
*/

extern an_ifc_word_sort_0_33 word_sort(an_ifc_word_category_0_33 versioned);

extern uint64_t word_value(an_ifc_word_category_0_33 versioned);

extern a_boolean validate_category(an_ifc_module                 *mod,
                                   an_ifc_word_category_0_33     versioned,
                                   const an_ifc_validation_trace *parent);

extern an_ifc_word_category to_universal_category(
                                          an_ifc_module             *mod,
                                          an_ifc_word_category_0_33 versioned);


template<typename a_Desired_type, typename an_ifc_Node_type>
inline void copy_ifc_field(a_Desired_type         *result,
                           const an_ifc_Node_type *node_start,
                           size_t                 offset,
                           size_t                 size)
/*
Given the starting position of a node's storage, the offset into the storage of
the field, and the field size, copy the field's bytes into result.

node_start should be a pointer to the byte array representing the node's
storage.  This will be added to the offset to get the start of field value's
bytes.  This function guarantees even if the byte position is not an aligned
representation of the desired type (a_Desired_type), so long as the result
pointer points to aligned storage, the result will be properly aligned.

This function is not a replacement for get_bytes and should NEVER be used with
byte arrays that do not have the host's endianness.  The byte array pointed to
by node_start should already have been (if necessary) realigned to match the
host's endianness before being passed to this function.

It is strongly advised not to directly use this function in the front end, and
instead to use a get_ifc_X function to get the field you want when working with
IFC data.  This function exists primarily as an implementation detail for code
generation to make use of (where code generation also handles additional
usage/sanity checks).
*/
{
  /* As the resulting pointer to the beginning of the field can be unaligned,
     casting to the desired type and dereferencing the byte array pointer is
     not an inherently safe operation.  While this is not (practically)
     problematic on x86, this can be an issue on some architectures that are
     more sensitive to pointer alignment.

     Thus, in the interest of portability, copy the bytes manually with memcpy
     into the aligned storage pointed to by result. */
  memcpy(result, (void*)((*node_start) + offset), size);
}  /* copy_ifc_field */


template<typename a_Desired_type, typename an_ifc_Node_type>
inline void copy_ifc_field(a_Desired_type         *result,
                           const an_ifc_Node_type *node_start,
                           size_t                 offset)
/*
Given the starting position of a node's storage, and the offset into the
storage of the field, copy the field's bytes into result.

node_start should be a pointer to the byte array representing the node's
storage.  This will be added to the offset to get the start of field value's
bytes.  This function guarantees even if the byte position is not an aligned
representation of the desired type (a_Desired_type), so long as the result
pointer points to aligned storage, the result will be properly aligned.

This function is not a replacement for get_bytes and should NEVER be used with
byte arrays that do not have the host's endianness.  The byte array pointed to
by node_start should already have been (if necessary) realigned to match the
host's endianness before being passed to this function.

It's additionally important when using this form of copy_ifc_field that the
result type's byte size corresponds exactly to the byte size of the encoded
field (otherwise buffer overflow is possible).  In terms of the IFC versioning
code, this means "versioned" types are the only types that should be used with
this function, "universal" representations are NOT safe.

It is strongly advised not to directly use this function in the front end, and
instead to use a get_ifc_X function to get the field you want when working with
IFC data.  This function exists primarily as an implementation detail for code
generation to make use of (where code generation also handles additional
usage/sanity checks).
*/
{
  copy_ifc_field(result, node_start, offset, /*size=*/sizeof(a_Desired_type));
}  /* copy_ifc_field */


template<typename an_ifc_Node_type>
extern an_ifc_Node_type* get(an_ifc_module    *mod,
                             an_ifc_Node_type *storage,
                             a_boolean        fill_storage = FALSE) = delete;


template<typename an_ifc_Node_type>
constexpr an_ifc_partition_kind get_ifc_partition_kind() = delete;

/*
Functions for interacting with IFC KeywordSyntax nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_keyword_syntax &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_keyword_syntax &universal);

extern a_boolean has_ifc_value(const an_ifc_keyword_syntax &universal);

extern an_ifc_keyword_sort get_ifc_value(
                                       const an_ifc_keyword_syntax &universal);

extern a_boolean validate(const an_ifc_keyword_syntax   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_keyword_syntax &universal, unsigned indent);

extern void db_node(const an_ifc_keyword_syntax &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC ModuleReference nodes.
*/

extern a_boolean has_ifc_owner(const an_ifc_module_reference &universal);

extern an_ifc_text_offset get_ifc_owner(
                                     const an_ifc_module_reference &universal);

extern a_boolean has_ifc_partition(const an_ifc_module_reference &universal);

extern an_ifc_text_offset get_ifc_partition(
                                     const an_ifc_module_reference &universal);

extern a_boolean validate(const an_ifc_module_reference &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_module_reference &universal, unsigned indent);

extern void db_node(const an_ifc_module_reference &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC NestableWord nodes.
*/

extern a_boolean has_ifc_category(const an_ifc_nestable_word &universal);

extern an_ifc_word_category get_ifc_category(
                                        const an_ifc_nestable_word &universal);

extern a_boolean has_ifc_index(const an_ifc_nestable_word &universal);

extern an_ifc_index get_ifc_index(const an_ifc_nestable_word &universal);

extern a_boolean has_ifc_locus(const an_ifc_nestable_word &universal);

extern an_ifc_source_location get_ifc_locus(
                                        const an_ifc_nestable_word &universal);

extern a_boolean has_ifc_sort(const an_ifc_nestable_word &universal);

extern an_ifc_word_sort get_ifc_sort(const an_ifc_nestable_word &universal);

extern a_boolean has_ifc_value(const an_ifc_nestable_word &universal);

extern an_ifc_u16 get_ifc_value(const an_ifc_nestable_word &universal);

extern a_boolean validate(const an_ifc_nestable_word    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_nestable_word &universal, unsigned indent);

extern void db_node(const an_ifc_nestable_word &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC NoexceptSpecification nodes.
*/

extern a_boolean has_ifc_sort(const an_ifc_noexcept_specification &universal);

extern an_ifc_noexcept_sort get_ifc_sort(
                               const an_ifc_noexcept_specification &universal);

extern a_boolean has_ifc_words(const an_ifc_noexcept_specification &universal);

extern an_ifc_sentence_index get_ifc_words(
                               const an_ifc_noexcept_specification &universal);

extern a_boolean validate(const an_ifc_noexcept_specification &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_noexcept_specification &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_noexcept_specification &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC ParameterizedEntity nodes.
*/

extern a_boolean has_ifc_attributes(
                                 const an_ifc_parameterized_entity &universal);

extern an_ifc_sentence_index get_ifc_attributes(
                                 const an_ifc_parameterized_entity &universal);

extern a_boolean has_ifc_body(const an_ifc_parameterized_entity &universal);

extern an_ifc_sentence_index get_ifc_body(
                                 const an_ifc_parameterized_entity &universal);

extern a_boolean has_ifc_decl(const an_ifc_parameterized_entity &universal);

extern an_ifc_decl_index get_ifc_decl(
                                 const an_ifc_parameterized_entity &universal);

extern a_boolean has_ifc_head(const an_ifc_parameterized_entity &universal);

extern an_ifc_sentence_index get_ifc_head(
                                 const an_ifc_parameterized_entity &universal);

extern a_boolean validate(const an_ifc_parameterized_entity &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_parameterized_entity &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_parameterized_entity &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC Sequence nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_sequence &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                             const an_ifc_sequence &universal);

extern a_boolean has_ifc_start(const an_ifc_sequence &universal);

extern an_ifc_index get_ifc_start(const an_ifc_sequence &universal);

extern a_boolean validate(const an_ifc_sequence         &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_sequence &universal, unsigned indent);

extern void db_node(const an_ifc_sequence &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC SourceLocation nodes.
*/

extern a_boolean has_ifc_column(const an_ifc_source_location &universal);

extern an_ifc_column get_ifc_column(const an_ifc_source_location &universal);

extern a_boolean has_ifc_line(const an_ifc_source_location &universal);

extern an_ifc_line_index get_ifc_line(const an_ifc_source_location &universal);

extern a_boolean validate(const an_ifc_source_location  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_source_location &universal, unsigned indent);

extern void db_node(const an_ifc_source_location &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC FileHeader nodes.
*/

extern a_boolean has_ifc_abi(const an_ifc_file_header &universal);

extern an_ifc_abi get_ifc_abi(const an_ifc_file_header &universal);

extern a_boolean has_ifc_arch(const an_ifc_file_header &universal);

extern an_ifc_architecture_sort get_ifc_arch(
                                          const an_ifc_file_header &universal);

extern a_boolean has_ifc_checksum(const an_ifc_file_header &universal);

extern an_ifc_sha256 get_ifc_checksum(const an_ifc_file_header &universal);

extern a_boolean has_ifc_dialect(const an_ifc_file_header &universal);

extern an_ifc_language_version get_ifc_dialect(
                                          const an_ifc_file_header &universal);

extern a_boolean has_ifc_global_scope(const an_ifc_file_header &universal);

extern an_ifc_scope_index get_ifc_global_scope(
                                          const an_ifc_file_header &universal);

extern a_boolean has_ifc_internal(const an_ifc_file_header &universal);

extern an_ifc_bool get_ifc_internal(const an_ifc_file_header &universal);

extern a_boolean has_ifc_major_version(const an_ifc_file_header &universal);

extern an_ifc_version get_ifc_major_version(
                                          const an_ifc_file_header &universal);

extern a_boolean has_ifc_minor_version(const an_ifc_file_header &universal);

extern an_ifc_version get_ifc_minor_version(
                                          const an_ifc_file_header &universal);

extern a_boolean has_ifc_partition_count(const an_ifc_file_header &universal);

extern an_ifc_cardinality get_ifc_partition_count(
                                          const an_ifc_file_header &universal);

extern a_boolean has_ifc_src_path(const an_ifc_file_header &universal);

extern an_ifc_text_offset get_ifc_src_path(
                                          const an_ifc_file_header &universal);

extern a_boolean has_ifc_string_table_bytes(
                                          const an_ifc_file_header &universal);

extern an_ifc_byte_offset get_ifc_string_table_bytes(
                                          const an_ifc_file_header &universal);

extern a_boolean has_ifc_string_table_size(
                                          const an_ifc_file_header &universal);

extern an_ifc_cardinality get_ifc_string_table_size(
                                          const an_ifc_file_header &universal);

extern a_boolean has_ifc_toc(const an_ifc_file_header &universal);

extern an_ifc_byte_offset get_ifc_toc(const an_ifc_file_header &universal);

extern a_boolean has_ifc_unit(const an_ifc_file_header &universal);

extern an_ifc_unit_index get_ifc_unit(const an_ifc_file_header &universal);

extern a_boolean validate(const an_ifc_file_header      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_file_header &universal, unsigned indent);

extern void db_node(const an_ifc_file_header &universal);
#endif /* DEBUG */

template<>
an_ifc_file_header_storage* get<an_ifc_file_header_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_file_header_storage *storage,
                                      a_boolean                  fill_storage);

/*
Functions for interacting with IFC Partition nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_partition &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                            const an_ifc_partition &universal);

extern a_boolean has_ifc_entry_size(const an_ifc_partition &universal);

extern an_ifc_entity_size get_ifc_entry_size(
                                            const an_ifc_partition &universal);

extern a_boolean has_ifc_name(const an_ifc_partition &universal);

extern an_ifc_text_offset get_ifc_name(const an_ifc_partition &universal);

extern a_boolean has_ifc_offset(const an_ifc_partition &universal);

extern an_ifc_byte_offset get_ifc_offset(const an_ifc_partition &universal);

extern a_boolean validate(const an_ifc_partition        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_partition &universal, unsigned indent);

extern void db_node(const an_ifc_partition &universal);
#endif /* DEBUG */

template<>
an_ifc_partition_storage* get<an_ifc_partition_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_partition_storage *storage,
                                        a_boolean                fill_storage);

/*
Functions for interacting with IFC AttrBasic nodes.
*/

extern a_boolean has_ifc_word(const an_ifc_attr_basic &universal);

extern an_ifc_nestable_word get_ifc_word(const an_ifc_attr_basic &universal);

extern a_boolean validate(const an_ifc_attr_basic       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_attr_basic &universal, unsigned indent);

extern void db_node(const an_ifc_attr_basic &universal);
#endif /* DEBUG */

template<>
an_ifc_attr_basic_storage* get<an_ifc_attr_basic_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_attr_basic_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_attr_basic_storage>()
/*
Return the corresponding partition kind for AttrBasic.
*/
{
  return ifc_pk_attr_basic;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC AttrCalled nodes.
*/

extern a_boolean has_ifc_arguments(const an_ifc_attr_called &universal);

extern an_ifc_attr_index get_ifc_arguments(
                                          const an_ifc_attr_called &universal);

extern a_boolean has_ifc_function(const an_ifc_attr_called &universal);

extern an_ifc_attr_index get_ifc_function(const an_ifc_attr_called &universal);

extern a_boolean validate(const an_ifc_attr_called      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_attr_called &universal, unsigned indent);

extern void db_node(const an_ifc_attr_called &universal);
#endif /* DEBUG */

template<>
an_ifc_attr_called_storage* get<an_ifc_attr_called_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_attr_called_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_attr_called_storage>()
/*
Return the corresponding partition kind for AttrCalled.
*/
{
  return ifc_pk_attr_called;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC AttrElaborated nodes.
*/

extern a_boolean has_ifc_expression(const an_ifc_attr_elaborated &universal);

extern an_ifc_expr_index get_ifc_expression(
                                      const an_ifc_attr_elaborated &universal);

extern a_boolean validate(const an_ifc_attr_elaborated  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_attr_elaborated &universal, unsigned indent);

extern void db_node(const an_ifc_attr_elaborated &universal);
#endif /* DEBUG */

template<>
an_ifc_attr_elaborated_storage* get<an_ifc_attr_elaborated_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_attr_elaborated_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_attr_elaborated_storage>()
/*
Return the corresponding partition kind for AttrElaborated.
*/
{
  return ifc_pk_attr_elaborated;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC AttrExpanded nodes.
*/

extern a_boolean has_ifc_operand(const an_ifc_attr_expanded &universal);

extern an_ifc_attr_index get_ifc_operand(
                                        const an_ifc_attr_expanded &universal);

extern a_boolean validate(const an_ifc_attr_expanded    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_attr_expanded &universal, unsigned indent);

extern void db_node(const an_ifc_attr_expanded &universal);
#endif /* DEBUG */

template<>
an_ifc_attr_expanded_storage* get<an_ifc_attr_expanded_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_attr_expanded_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_attr_expanded_storage>()
/*
Return the corresponding partition kind for AttrExpanded.
*/
{
  return ifc_pk_attr_expanded;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC AttrFactored nodes.
*/

extern a_boolean has_ifc_factor(const an_ifc_attr_factored &universal);

extern an_ifc_nestable_word get_ifc_factor(
                                        const an_ifc_attr_factored &universal);

extern a_boolean has_ifc_terms(const an_ifc_attr_factored &universal);

extern an_ifc_attr_index get_ifc_terms(const an_ifc_attr_factored &universal);

extern a_boolean validate(const an_ifc_attr_factored    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_attr_factored &universal, unsigned indent);

extern void db_node(const an_ifc_attr_factored &universal);
#endif /* DEBUG */

template<>
an_ifc_attr_factored_storage* get<an_ifc_attr_factored_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_attr_factored_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_attr_factored_storage>()
/*
Return the corresponding partition kind for AttrFactored.
*/
{
  return ifc_pk_attr_factored;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC AttrLabeled nodes.
*/

extern a_boolean has_ifc_attribute(const an_ifc_attr_labeled &universal);

extern an_ifc_attr_index get_ifc_attribute(
                                         const an_ifc_attr_labeled &universal);

extern a_boolean has_ifc_label(const an_ifc_attr_labeled &universal);

extern an_ifc_nestable_word get_ifc_label(
                                         const an_ifc_attr_labeled &universal);

extern a_boolean validate(const an_ifc_attr_labeled     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_attr_labeled &universal, unsigned indent);

extern void db_node(const an_ifc_attr_labeled &universal);
#endif /* DEBUG */

template<>
an_ifc_attr_labeled_storage* get<an_ifc_attr_labeled_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_attr_labeled_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_attr_labeled_storage>()
/*
Return the corresponding partition kind for AttrLabeled.
*/
{
  return ifc_pk_attr_labeled;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC AttrScoped nodes.
*/

extern a_boolean has_ifc_member(const an_ifc_attr_scoped &universal);

extern an_ifc_nestable_word get_ifc_member(
                                          const an_ifc_attr_scoped &universal);

extern a_boolean has_ifc_scope(const an_ifc_attr_scoped &universal);

extern an_ifc_nestable_word get_ifc_scope(const an_ifc_attr_scoped &universal);

extern a_boolean validate(const an_ifc_attr_scoped      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_attr_scoped &universal, unsigned indent);

extern void db_node(const an_ifc_attr_scoped &universal);
#endif /* DEBUG */

template<>
an_ifc_attr_scoped_storage* get<an_ifc_attr_scoped_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_attr_scoped_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_attr_scoped_storage>()
/*
Return the corresponding partition kind for AttrScoped.
*/
{
  return ifc_pk_attr_scoped;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC AttrTuple nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_attr_tuple &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                           const an_ifc_attr_tuple &universal);

extern a_boolean has_ifc_start(const an_ifc_attr_tuple &universal);

extern an_ifc_index get_ifc_start(const an_ifc_attr_tuple &universal);

extern a_boolean validate(const an_ifc_attr_tuple       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_attr_tuple &universal, unsigned indent);

extern void db_node(const an_ifc_attr_tuple &universal);
#endif /* DEBUG */

template<>
an_ifc_attr_tuple_storage* get<an_ifc_attr_tuple_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_attr_tuple_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_attr_tuple_storage>()
/*
Return the corresponding partition kind for AttrTuple.
*/
{
  return ifc_pk_attr_tuple;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ChartMultilevel nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_chart_multilevel &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                     const an_ifc_chart_multilevel &universal);

extern a_boolean has_ifc_start(const an_ifc_chart_multilevel &universal);

extern an_ifc_index get_ifc_start(const an_ifc_chart_multilevel &universal);

extern a_boolean validate(const an_ifc_chart_multilevel &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_chart_multilevel &universal, unsigned indent);

extern void db_node(const an_ifc_chart_multilevel &universal);
#endif /* DEBUG */

template<>
an_ifc_chart_multilevel_storage* get<an_ifc_chart_multilevel_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_chart_multilevel_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_chart_multilevel_storage>()
/*
Return the corresponding partition kind for ChartMultilevel.
*/
{
  return ifc_pk_chart_multilevel;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ChartUnilevel nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_chart_unilevel &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                       const an_ifc_chart_unilevel &universal);

extern a_boolean has_ifc_constraint(const an_ifc_chart_unilevel &universal);

extern an_ifc_expr_index get_ifc_constraint(
                                       const an_ifc_chart_unilevel &universal);

extern a_boolean has_ifc_start(const an_ifc_chart_unilevel &universal);

extern an_ifc_index get_ifc_start(const an_ifc_chart_unilevel &universal);

extern a_boolean validate(const an_ifc_chart_unilevel   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_chart_unilevel &universal, unsigned indent);

extern void db_node(const an_ifc_chart_unilevel &universal);
#endif /* DEBUG */

template<>
an_ifc_chart_unilevel_storage* get<an_ifc_chart_unilevel_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_chart_unilevel_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_chart_unilevel_storage>()
/*
Return the corresponding partition kind for ChartUnilevel.
*/
{
  return ifc_pk_chart_unilevel;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ConstF64 nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_const_f64 &universal);

extern an_ifc_ieeele_float get_ifc_value(const an_ifc_const_f64 &universal);

extern a_boolean validate(const an_ifc_const_f64        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_const_f64 &universal, unsigned indent);

extern void db_node(const an_ifc_const_f64 &universal);
#endif /* DEBUG */

template<>
an_ifc_const_f64_storage* get<an_ifc_const_f64_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_const_f64_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_const_f64_storage>()
/*
Return the corresponding partition kind for ConstF64.
*/
{
  return ifc_pk_const_f64;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ConstI64 nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_const_i64 &universal);

extern an_ifc_u64 get_ifc_value(const an_ifc_const_i64 &universal);

extern a_boolean validate(const an_ifc_const_i64        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_const_i64 &universal, unsigned indent);

extern void db_node(const an_ifc_const_i64 &universal);
#endif /* DEBUG */

template<>
an_ifc_const_i64_storage* get<an_ifc_const_i64_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_const_i64_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_const_i64_storage>()
/*
Return the corresponding partition kind for ConstI64.
*/
{
  return ifc_pk_const_i64;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ConstStr nodes.
*/

extern a_boolean has_ifc_length(const an_ifc_const_str &universal);

extern an_ifc_cardinality get_ifc_length(const an_ifc_const_str &universal);

extern a_boolean has_ifc_start(const an_ifc_const_str &universal);

extern an_ifc_text_offset get_ifc_start(const an_ifc_const_str &universal);

extern a_boolean has_ifc_suffix(const an_ifc_const_str &universal);

extern an_ifc_text_offset get_ifc_suffix(const an_ifc_const_str &universal);

extern a_boolean validate(const an_ifc_const_str        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_const_str &universal, unsigned indent);

extern void db_node(const an_ifc_const_str &universal);
#endif /* DEBUG */

template<>
an_ifc_const_str_storage* get<an_ifc_const_str_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_const_str_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_const_str_storage>()
/*
Return the corresponding partition kind for ConstStr.
*/
{
  return ifc_pk_const_str;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclAlias nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_alias &universal);

extern an_ifc_access_sort get_ifc_access(const an_ifc_decl_alias &universal);

extern a_boolean has_ifc_aliasee(const an_ifc_decl_alias &universal);

extern an_ifc_type_index get_ifc_aliasee(const an_ifc_decl_alias &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_alias &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                           const an_ifc_decl_alias &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_alias &universal);

extern an_ifc_source_location get_ifc_locus(
                                           const an_ifc_decl_alias &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_alias &universal);

extern an_ifc_text_offset get_ifc_name(const an_ifc_decl_alias &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_alias &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                           const an_ifc_decl_alias &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_alias &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_alias &universal);

extern a_boolean validate(const an_ifc_decl_alias       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_alias &universal, unsigned indent);

extern void db_node(const an_ifc_decl_alias &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_alias_storage* get<an_ifc_decl_alias_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_decl_alias_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_alias_storage>()
/*
Return the corresponding partition kind for DeclAlias.
*/
{
  return ifc_pk_decl_alias;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclBitfield nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_bitfield &universal);

extern an_ifc_access_sort get_ifc_access(
                                        const an_ifc_decl_bitfield &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_bitfield &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                        const an_ifc_decl_bitfield &universal);

extern a_boolean has_ifc_initializer(const an_ifc_decl_bitfield &universal);

extern an_ifc_expr_index get_ifc_initializer(
                                        const an_ifc_decl_bitfield &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_bitfield &universal);

extern an_ifc_source_location get_ifc_locus(
                                        const an_ifc_decl_bitfield &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_bitfield &universal);

extern an_ifc_text_offset get_ifc_name(const an_ifc_decl_bitfield &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_bitfield &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                        const an_ifc_decl_bitfield &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_bitfield &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                        const an_ifc_decl_bitfield &universal);

extern a_boolean has_ifc_traits(const an_ifc_decl_bitfield &universal);

extern an_ifc_object_traits_bitfield get_ifc_traits(
                                        const an_ifc_decl_bitfield &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_bitfield &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_bitfield &universal);

extern a_boolean has_ifc_width(const an_ifc_decl_bitfield &universal);

extern an_ifc_expr_index get_ifc_width(const an_ifc_decl_bitfield &universal);

extern a_boolean validate(const an_ifc_decl_bitfield    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_bitfield &universal, unsigned indent);

extern void db_node(const an_ifc_decl_bitfield &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_bitfield_storage* get<an_ifc_decl_bitfield_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_decl_bitfield_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_bitfield_storage>()
/*
Return the corresponding partition kind for DeclBitfield.
*/
{
  return ifc_pk_decl_bitfield;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclConcept nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_concept &universal);

extern an_ifc_access_sort get_ifc_access(const an_ifc_decl_concept &universal);

extern a_boolean has_ifc_body(const an_ifc_decl_concept &universal);

extern an_ifc_sentence_index get_ifc_body(
                                         const an_ifc_decl_concept &universal);

extern a_boolean has_ifc_chart(const an_ifc_decl_concept &universal);

extern an_ifc_chart_index get_ifc_chart(const an_ifc_decl_concept &universal);

extern a_boolean has_ifc_constraint(const an_ifc_decl_concept &universal);

extern an_ifc_expr_index get_ifc_constraint(
                                         const an_ifc_decl_concept &universal);

extern a_boolean has_ifc_head(const an_ifc_decl_concept &universal);

extern an_ifc_sentence_index get_ifc_head(
                                         const an_ifc_decl_concept &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_concept &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                         const an_ifc_decl_concept &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_concept &universal);

extern an_ifc_source_location get_ifc_locus(
                                         const an_ifc_decl_concept &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_concept &universal);

extern an_ifc_text_offset get_ifc_name(const an_ifc_decl_concept &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_concept &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                         const an_ifc_decl_concept &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_concept &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_concept &universal);

extern a_boolean has_ifc_unknown(const an_ifc_decl_concept &universal);

extern an_ifc_u16 get_ifc_unknown(const an_ifc_decl_concept &universal);

extern a_boolean validate(const an_ifc_decl_concept     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_concept &universal, unsigned indent);

extern void db_node(const an_ifc_decl_concept &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_concept_storage* get<an_ifc_decl_concept_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_decl_concept_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_concept_storage>()
/*
Return the corresponding partition kind for DeclConcept.
*/
{
  return ifc_pk_decl_concept;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclConstructor nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_constructor &universal);

extern an_ifc_access_sort get_ifc_access(
                                     const an_ifc_decl_constructor &universal);

extern a_boolean has_ifc_chart(const an_ifc_decl_constructor &universal);

extern an_ifc_chart_index get_ifc_chart(
                                     const an_ifc_decl_constructor &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_constructor &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                     const an_ifc_decl_constructor &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_constructor &universal);

extern an_ifc_source_location get_ifc_locus(
                                     const an_ifc_decl_constructor &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_constructor &universal);

extern an_ifc_name_index get_ifc_name(
                                     const an_ifc_decl_constructor &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_constructor &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                     const an_ifc_decl_constructor &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_constructor &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                     const an_ifc_decl_constructor &universal);

extern a_boolean has_ifc_traits(const an_ifc_decl_constructor &universal);

extern an_ifc_function_traits_bitfield get_ifc_traits(
                                     const an_ifc_decl_constructor &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_constructor &universal);

extern an_ifc_type_index get_ifc_type(
                                     const an_ifc_decl_constructor &universal);

extern a_boolean validate(const an_ifc_decl_constructor &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_constructor &universal, unsigned indent);

extern void db_node(const an_ifc_decl_constructor &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_constructor_storage* get<an_ifc_decl_constructor_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_decl_constructor_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_constructor_storage>()
/*
Return the corresponding partition kind for DeclConstructor.
*/
{
  return ifc_pk_decl_constructor;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclDeductionGuide nodes.
*/

extern a_boolean has_ifc_home_scope(
                                 const an_ifc_decl_deduction_guide &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                 const an_ifc_decl_deduction_guide &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_deduction_guide &universal);

extern an_ifc_source_location get_ifc_locus(
                                 const an_ifc_decl_deduction_guide &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_deduction_guide &universal);

extern an_ifc_text_offset get_ifc_name(
                                 const an_ifc_decl_deduction_guide &universal);

extern a_boolean has_ifc_source(const an_ifc_decl_deduction_guide &universal);

extern an_ifc_chart_index get_ifc_source(
                                 const an_ifc_decl_deduction_guide &universal);

extern a_boolean has_ifc_specifiers(
                                 const an_ifc_decl_deduction_guide &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                 const an_ifc_decl_deduction_guide &universal);

extern a_boolean has_ifc_target(const an_ifc_decl_deduction_guide &universal);

extern an_ifc_expr_index get_ifc_target(
                                 const an_ifc_decl_deduction_guide &universal);

extern a_boolean has_ifc_traits(const an_ifc_decl_deduction_guide &universal);

extern an_ifc_guide_traits_bitfield get_ifc_traits(
                                 const an_ifc_decl_deduction_guide &universal);

extern a_boolean validate(const an_ifc_decl_deduction_guide &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_deduction_guide &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_decl_deduction_guide &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_deduction_guide_storage* get<an_ifc_decl_deduction_guide_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_decl_deduction_guide_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_deduction_guide_storage>()
/*
Return the corresponding partition kind for DeclDeductionGuide.
*/
{
  return ifc_pk_decl_deduction_guide;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclDestructor nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_destructor &universal);

extern an_ifc_access_sort get_ifc_access(
                                      const an_ifc_decl_destructor &universal);

extern a_boolean has_ifc_convention(const an_ifc_decl_destructor &universal);

extern an_ifc_calling_convention_sort get_ifc_convention(
                                      const an_ifc_decl_destructor &universal);

extern a_boolean has_ifc_eh_spec(const an_ifc_decl_destructor &universal);

extern an_ifc_noexcept_specification get_ifc_eh_spec(
                                      const an_ifc_decl_destructor &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_destructor &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                      const an_ifc_decl_destructor &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_destructor &universal);

extern an_ifc_source_location get_ifc_locus(
                                      const an_ifc_decl_destructor &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_destructor &universal);

extern an_ifc_name_index get_ifc_name(const an_ifc_decl_destructor &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_destructor &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                      const an_ifc_decl_destructor &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_destructor &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                      const an_ifc_decl_destructor &universal);

extern a_boolean has_ifc_traits(const an_ifc_decl_destructor &universal);

extern an_ifc_function_traits_bitfield get_ifc_traits(
                                      const an_ifc_decl_destructor &universal);

extern a_boolean validate(const an_ifc_decl_destructor  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_destructor &universal, unsigned indent);

extern void db_node(const an_ifc_decl_destructor &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_destructor_storage* get<an_ifc_decl_destructor_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_decl_destructor_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_destructor_storage>()
/*
Return the corresponding partition kind for DeclDestructor.
*/
{
  return ifc_pk_decl_destructor;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclEnumeration nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_enumeration &universal);

extern an_ifc_access_sort get_ifc_access(
                                     const an_ifc_decl_enumeration &universal);

extern a_boolean has_ifc_alignment(const an_ifc_decl_enumeration &universal);

extern an_ifc_expr_index get_ifc_alignment(
                                     const an_ifc_decl_enumeration &universal);

extern a_boolean has_ifc_base(const an_ifc_decl_enumeration &universal);

extern an_ifc_type_index get_ifc_base(
                                     const an_ifc_decl_enumeration &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_enumeration &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                     const an_ifc_decl_enumeration &universal);

extern a_boolean has_ifc_initializer(const an_ifc_decl_enumeration &universal);

extern an_ifc_sequence get_ifc_initializer(
                                     const an_ifc_decl_enumeration &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_enumeration &universal);

extern an_ifc_source_location get_ifc_locus(
                                     const an_ifc_decl_enumeration &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_enumeration &universal);

extern an_ifc_text_offset get_ifc_name(
                                     const an_ifc_decl_enumeration &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_enumeration &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                     const an_ifc_decl_enumeration &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_enumeration &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                     const an_ifc_decl_enumeration &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_enumeration &universal);

extern an_ifc_type_index get_ifc_type(
                                     const an_ifc_decl_enumeration &universal);

extern a_boolean validate(const an_ifc_decl_enumeration &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_enumeration &universal, unsigned indent);

extern void db_node(const an_ifc_decl_enumeration &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_enumeration_storage* get<an_ifc_decl_enumeration_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_decl_enumeration_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_enumeration_storage>()
/*
Return the corresponding partition kind for DeclEnumeration.
*/
{
  return ifc_pk_decl_enum;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclEnumerator nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_enumerator &universal);

extern an_ifc_access_sort get_ifc_access(
                                      const an_ifc_decl_enumerator &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_enumerator &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                      const an_ifc_decl_enumerator &universal);

extern a_boolean has_ifc_initializer(const an_ifc_decl_enumerator &universal);

extern an_ifc_expr_index get_ifc_initializer(
                                      const an_ifc_decl_enumerator &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_enumerator &universal);

extern an_ifc_source_location get_ifc_locus(
                                      const an_ifc_decl_enumerator &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_enumerator &universal);

extern an_ifc_text_offset get_ifc_name(
                                      const an_ifc_decl_enumerator &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_enumerator &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                      const an_ifc_decl_enumerator &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_enumerator &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_enumerator &universal);

extern a_boolean validate(const an_ifc_decl_enumerator  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_enumerator &universal, unsigned indent);

extern void db_node(const an_ifc_decl_enumerator &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_enumerator_storage* get<an_ifc_decl_enumerator_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_decl_enumerator_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_enumerator_storage>()
/*
Return the corresponding partition kind for DeclEnumerator.
*/
{
  return ifc_pk_decl_enumerator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclExpansion nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_decl_expansion &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_decl_expansion &universal);

extern a_boolean has_ifc_operand(const an_ifc_decl_expansion &universal);

extern an_ifc_decl_index get_ifc_operand(
                                       const an_ifc_decl_expansion &universal);

extern a_boolean validate(const an_ifc_decl_expansion   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_expansion &universal, unsigned indent);

extern void db_node(const an_ifc_decl_expansion &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_expansion_storage* get<an_ifc_decl_expansion_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_decl_expansion_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_expansion_storage>()
/*
Return the corresponding partition kind for DeclExpansion.
*/
{
  return ifc_pk_decl_expansion;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclExplicitInstantiation nodes.
*/

extern a_boolean has_ifc_decl(
                          const an_ifc_decl_explicit_instantiation &universal);

extern an_ifc_decl_index get_ifc_decl(
                          const an_ifc_decl_explicit_instantiation &universal);

extern a_boolean has_ifc_form(
                          const an_ifc_decl_explicit_instantiation &universal);

extern an_ifc_form_spec_index get_ifc_form(
                          const an_ifc_decl_explicit_instantiation &universal);

extern a_boolean validate(const an_ifc_decl_explicit_instantiation &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_explicit_instantiation &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_decl_explicit_instantiation &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_explicit_instantiation_storage*
get<an_ifc_decl_explicit_instantiation_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_decl_explicit_instantiation_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_explicit_instantiation_storage>()
/*
Return the corresponding partition kind for DeclExplicitInstantiation.
*/
{
  return ifc_pk_decl_explicit_instantiation;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclExplicitSpecialization nodes.
*/

extern a_boolean has_ifc_decl(
                         const an_ifc_decl_explicit_specialization &universal);

extern an_ifc_decl_index get_ifc_decl(
                         const an_ifc_decl_explicit_specialization &universal);

extern a_boolean has_ifc_form(
                         const an_ifc_decl_explicit_specialization &universal);

extern an_ifc_form_spec_index get_ifc_form(
                         const an_ifc_decl_explicit_specialization &universal);

extern a_boolean validate(
                         const an_ifc_decl_explicit_specialization &universal,
                         const an_ifc_validation_trace             *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_explicit_specialization &universal,
                    unsigned                                  indent);

extern void db_node(const an_ifc_decl_explicit_specialization &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_explicit_specialization_storage*
get<an_ifc_decl_explicit_specialization_storage>(
                     an_ifc_module                               *mod,
                     an_ifc_decl_explicit_specialization_storage *storage,
                     a_boolean                                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_explicit_specialization_storage>()
/*
Return the corresponding partition kind for DeclExplicitSpecialization.
*/
{
  return ifc_pk_decl_explicit_specialization;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclField nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_field &universal);

extern an_ifc_access_sort get_ifc_access(const an_ifc_decl_field &universal);

extern a_boolean has_ifc_alignment(const an_ifc_decl_field &universal);

extern an_ifc_expr_index get_ifc_alignment(const an_ifc_decl_field &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_field &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                           const an_ifc_decl_field &universal);

extern a_boolean has_ifc_initializer(const an_ifc_decl_field &universal);

extern an_ifc_expr_index get_ifc_initializer(
                                           const an_ifc_decl_field &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_field &universal);

extern an_ifc_source_location get_ifc_locus(
                                           const an_ifc_decl_field &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_field &universal);

extern an_ifc_text_offset get_ifc_name(const an_ifc_decl_field &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_field &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                           const an_ifc_decl_field &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_field &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                           const an_ifc_decl_field &universal);

extern a_boolean has_ifc_traits(const an_ifc_decl_field &universal);

extern an_ifc_object_traits_bitfield get_ifc_traits(
                                           const an_ifc_decl_field &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_field &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_field &universal);

extern a_boolean validate(const an_ifc_decl_field       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_field &universal, unsigned indent);

extern void db_node(const an_ifc_decl_field &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_field_storage* get<an_ifc_decl_field_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_decl_field_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_field_storage>()
/*
Return the corresponding partition kind for DeclField.
*/
{
  return ifc_pk_decl_field;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclFriend nodes.
*/

extern a_boolean has_ifc_entity(const an_ifc_decl_friend &universal);

extern an_ifc_expr_index get_ifc_entity(const an_ifc_decl_friend &universal);

extern a_boolean validate(const an_ifc_decl_friend      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_friend &universal, unsigned indent);

extern void db_node(const an_ifc_decl_friend &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_friend_storage* get<an_ifc_decl_friend_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_decl_friend_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_friend_storage>()
/*
Return the corresponding partition kind for DeclFriend.
*/
{
  return ifc_pk_decl_friend;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclFunction nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_function &universal);

extern an_ifc_access_sort get_ifc_access(
                                        const an_ifc_decl_function &universal);

extern a_boolean has_ifc_chart(const an_ifc_decl_function &universal);

extern an_ifc_chart_index get_ifc_chart(const an_ifc_decl_function &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_function &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                        const an_ifc_decl_function &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_function &universal);

extern an_ifc_source_location get_ifc_locus(
                                        const an_ifc_decl_function &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_function &universal);

extern an_ifc_name_index get_ifc_name(const an_ifc_decl_function &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_function &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                        const an_ifc_decl_function &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_function &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                        const an_ifc_decl_function &universal);

extern a_boolean has_ifc_traits(const an_ifc_decl_function &universal);

extern an_ifc_function_traits_bitfield get_ifc_traits(
                                        const an_ifc_decl_function &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_function &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_function &universal);

extern a_boolean validate(const an_ifc_decl_function    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_function &universal, unsigned indent);

extern void db_node(const an_ifc_decl_function &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_function_storage* get<an_ifc_decl_function_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_decl_function_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_function_storage>()
/*
Return the corresponding partition kind for DeclFunction.
*/
{
  return ifc_pk_decl_function;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclInheritedConstructor nodes.
*/

extern a_boolean has_ifc_access(
                           const an_ifc_decl_inherited_constructor &universal);

extern an_ifc_access_sort get_ifc_access(
                           const an_ifc_decl_inherited_constructor &universal);

extern a_boolean has_ifc_base_ctor(
                           const an_ifc_decl_inherited_constructor &universal);

extern an_ifc_decl_index get_ifc_base_ctor(
                           const an_ifc_decl_inherited_constructor &universal);

extern a_boolean has_ifc_chart(
                           const an_ifc_decl_inherited_constructor &universal);

extern an_ifc_chart_index get_ifc_chart(
                           const an_ifc_decl_inherited_constructor &universal);

extern a_boolean has_ifc_home_scope(
                           const an_ifc_decl_inherited_constructor &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                           const an_ifc_decl_inherited_constructor &universal);

extern a_boolean has_ifc_locus(
                           const an_ifc_decl_inherited_constructor &universal);

extern an_ifc_source_location get_ifc_locus(
                           const an_ifc_decl_inherited_constructor &universal);

extern a_boolean has_ifc_name(
                           const an_ifc_decl_inherited_constructor &universal);

extern an_ifc_text_offset get_ifc_name(
                           const an_ifc_decl_inherited_constructor &universal);

extern a_boolean has_ifc_specifiers(
                           const an_ifc_decl_inherited_constructor &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                           const an_ifc_decl_inherited_constructor &universal);

extern a_boolean has_ifc_traits(
                           const an_ifc_decl_inherited_constructor &universal);

extern an_ifc_function_traits_bitfield get_ifc_traits(
                           const an_ifc_decl_inherited_constructor &universal);

extern a_boolean has_ifc_type(
                           const an_ifc_decl_inherited_constructor &universal);

extern an_ifc_type_index get_ifc_type(
                           const an_ifc_decl_inherited_constructor &universal);

extern a_boolean validate(const an_ifc_decl_inherited_constructor &universal,
                          const an_ifc_validation_trace           *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_inherited_constructor &universal,
                    unsigned                                indent);

extern void db_node(const an_ifc_decl_inherited_constructor &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_inherited_constructor_storage*
get<an_ifc_decl_inherited_constructor_storage>(
                       an_ifc_module                             *mod,
                       an_ifc_decl_inherited_constructor_storage *storage,
                       a_boolean                                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_inherited_constructor_storage>()
/*
Return the corresponding partition kind for DeclInheritedConstructor.
*/
{
  return ifc_pk_decl_inherited_constructor;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclIntrinsic nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_intrinsic &universal);

extern an_ifc_access_sort get_ifc_access(
                                       const an_ifc_decl_intrinsic &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_intrinsic &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                       const an_ifc_decl_intrinsic &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_intrinsic &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_decl_intrinsic &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_intrinsic &universal);

extern an_ifc_text_offset get_ifc_name(const an_ifc_decl_intrinsic &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_intrinsic &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                       const an_ifc_decl_intrinsic &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_intrinsic &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_intrinsic &universal);

extern a_boolean validate(const an_ifc_decl_intrinsic   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_intrinsic &universal, unsigned indent);

extern void db_node(const an_ifc_decl_intrinsic &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_intrinsic_storage* get<an_ifc_decl_intrinsic_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_decl_intrinsic_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_intrinsic_storage>()
/*
Return the corresponding partition kind for DeclIntrinsic.
*/
{
  return ifc_pk_decl_intrinsic;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclMethod nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_method &universal);

extern an_ifc_access_sort get_ifc_access(const an_ifc_decl_method &universal);

extern a_boolean has_ifc_chart(const an_ifc_decl_method &universal);

extern an_ifc_chart_index get_ifc_chart(const an_ifc_decl_method &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_method &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                          const an_ifc_decl_method &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_method &universal);

extern an_ifc_source_location get_ifc_locus(
                                          const an_ifc_decl_method &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_method &universal);

extern an_ifc_name_index get_ifc_name(const an_ifc_decl_method &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_method &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                          const an_ifc_decl_method &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_method &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                          const an_ifc_decl_method &universal);

extern a_boolean has_ifc_traits(const an_ifc_decl_method &universal);

extern an_ifc_function_traits_bitfield get_ifc_traits(
                                          const an_ifc_decl_method &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_method &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_method &universal);

extern a_boolean validate(const an_ifc_decl_method      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_method &universal, unsigned indent);

extern void db_node(const an_ifc_decl_method &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_method_storage* get<an_ifc_decl_method_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_decl_method_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_method_storage>()
/*
Return the corresponding partition kind for DeclMethod.
*/
{
  return ifc_pk_decl_method;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclOutputSegment nodes.
*/

extern a_boolean has_ifc_ID(const an_ifc_decl_output_segment &universal);

extern an_ifc_text_offset get_ifc_ID(
                                  const an_ifc_decl_output_segment &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_output_segment &universal);

extern an_ifc_text_offset get_ifc_name(
                                  const an_ifc_decl_output_segment &universal);

extern a_boolean has_ifc_traits(const an_ifc_decl_output_segment &universal);

extern an_ifc_segment_traits get_ifc_traits(
                                  const an_ifc_decl_output_segment &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_output_segment &universal);

extern an_ifc_segment_type get_ifc_type(
                                  const an_ifc_decl_output_segment &universal);

extern a_boolean validate(const an_ifc_decl_output_segment &universal,
                          const an_ifc_validation_trace    *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_output_segment &universal,
                    unsigned                         indent);

extern void db_node(const an_ifc_decl_output_segment &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_output_segment_storage* get<an_ifc_decl_output_segment_storage>(
                              an_ifc_module                      *mod,
                              an_ifc_decl_output_segment_storage *storage,
                              a_boolean                          fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_output_segment_storage>()
/*
Return the corresponding partition kind for DeclOutputSegment.
*/
{
  return ifc_pk_decl_segment;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclParameter nodes.
*/

extern a_boolean has_ifc_constraint(const an_ifc_decl_parameter &universal);

extern an_ifc_expr_index get_ifc_constraint(
                                       const an_ifc_decl_parameter &universal);

extern a_boolean has_ifc_initializer(const an_ifc_decl_parameter &universal);

extern an_ifc_expr_index get_ifc_initializer(
                                       const an_ifc_decl_parameter &universal);

extern a_boolean has_ifc_level(const an_ifc_decl_parameter &universal);

extern an_ifc_parameter_level get_ifc_level(
                                       const an_ifc_decl_parameter &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_parameter &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_decl_parameter &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_parameter &universal);

extern an_ifc_text_offset get_ifc_name(const an_ifc_decl_parameter &universal);

extern a_boolean has_ifc_pack(const an_ifc_decl_parameter &universal);

extern an_ifc_bool get_ifc_pack(const an_ifc_decl_parameter &universal);

extern a_boolean has_ifc_position(const an_ifc_decl_parameter &universal);

extern an_ifc_parameter_position get_ifc_position(
                                       const an_ifc_decl_parameter &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_parameter &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                       const an_ifc_decl_parameter &universal);

extern a_boolean has_ifc_sort(const an_ifc_decl_parameter &universal);

extern an_ifc_parameter_sort get_ifc_sort(
                                       const an_ifc_decl_parameter &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_parameter &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_parameter &universal);

extern a_boolean validate(const an_ifc_decl_parameter   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_parameter &universal, unsigned indent);

extern void db_node(const an_ifc_decl_parameter &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_parameter_storage* get<an_ifc_decl_parameter_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_decl_parameter_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_parameter_storage>()
/*
Return the corresponding partition kind for DeclParameter.
*/
{
  return ifc_pk_decl_parameter;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclPartialSpecialization nodes.
*/

extern a_boolean has_ifc_access(
                          const an_ifc_decl_partial_specialization &universal);

extern an_ifc_access_sort get_ifc_access(
                          const an_ifc_decl_partial_specialization &universal);

extern a_boolean has_ifc_chart(
                          const an_ifc_decl_partial_specialization &universal);

extern an_ifc_chart_index get_ifc_chart(
                          const an_ifc_decl_partial_specialization &universal);

extern a_boolean has_ifc_entity(
                          const an_ifc_decl_partial_specialization &universal);

extern an_ifc_parameterized_entity get_ifc_entity(
                          const an_ifc_decl_partial_specialization &universal);

extern a_boolean has_ifc_form(
                          const an_ifc_decl_partial_specialization &universal);

extern an_ifc_form_spec_index get_ifc_form(
                          const an_ifc_decl_partial_specialization &universal);

extern a_boolean has_ifc_home_scope(
                          const an_ifc_decl_partial_specialization &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                          const an_ifc_decl_partial_specialization &universal);

extern a_boolean has_ifc_locus(
                          const an_ifc_decl_partial_specialization &universal);

extern an_ifc_source_location get_ifc_locus(
                          const an_ifc_decl_partial_specialization &universal);

extern a_boolean has_ifc_name(
                          const an_ifc_decl_partial_specialization &universal);

extern an_ifc_name_index get_ifc_name(
                          const an_ifc_decl_partial_specialization &universal);

extern a_boolean has_ifc_properties(
                          const an_ifc_decl_partial_specialization &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                          const an_ifc_decl_partial_specialization &universal);

extern a_boolean has_ifc_specifiers(
                          const an_ifc_decl_partial_specialization &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                          const an_ifc_decl_partial_specialization &universal);

extern a_boolean validate(const an_ifc_decl_partial_specialization &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_partial_specialization &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_decl_partial_specialization &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_partial_specialization_storage*
get<an_ifc_decl_partial_specialization_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_decl_partial_specialization_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_partial_specialization_storage>()
/*
Return the corresponding partition kind for DeclPartialSpecialization.
*/
{
  return ifc_pk_decl_partial_specialization;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclProperty nodes.
*/

extern a_boolean has_ifc_getter(const an_ifc_decl_property &universal);

extern an_ifc_text_offset get_ifc_getter(
                                        const an_ifc_decl_property &universal);

extern a_boolean has_ifc_member(const an_ifc_decl_property &universal);

extern an_ifc_decl_index get_ifc_member(const an_ifc_decl_property &universal);

extern a_boolean has_ifc_setter(const an_ifc_decl_property &universal);

extern an_ifc_text_offset get_ifc_setter(
                                        const an_ifc_decl_property &universal);

extern a_boolean validate(const an_ifc_decl_property    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_property &universal, unsigned indent);

extern void db_node(const an_ifc_decl_property &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_property_storage* get<an_ifc_decl_property_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_decl_property_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_property_storage>()
/*
Return the corresponding partition kind for DeclProperty.
*/
{
  return ifc_pk_decl_property;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclReference nodes.
*/

extern a_boolean has_ifc_index(const an_ifc_decl_reference &universal);

extern an_ifc_decl_index get_ifc_index(const an_ifc_decl_reference &universal);

extern a_boolean has_ifc_local_index(const an_ifc_decl_reference &universal);

extern an_ifc_decl_foreign_index get_ifc_local_index(
                                       const an_ifc_decl_reference &universal);

extern a_boolean has_ifc_unit(const an_ifc_decl_reference &universal);

extern an_ifc_module_reference get_ifc_unit(
                                       const an_ifc_decl_reference &universal);

extern a_boolean validate(const an_ifc_decl_reference   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_reference &universal, unsigned indent);

extern void db_node(const an_ifc_decl_reference &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_reference_storage* get<an_ifc_decl_reference_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_decl_reference_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_reference_storage>()
/*
Return the corresponding partition kind for DeclReference.
*/
{
  return ifc_pk_decl_reference;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclScope nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_scope &universal);

extern an_ifc_access_sort get_ifc_access(const an_ifc_decl_scope &universal);

extern a_boolean has_ifc_alignment(const an_ifc_decl_scope &universal);

extern an_ifc_expr_index get_ifc_alignment(const an_ifc_decl_scope &universal);

extern a_boolean has_ifc_base(const an_ifc_decl_scope &universal);

extern an_ifc_type_index get_ifc_base(const an_ifc_decl_scope &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_scope &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                           const an_ifc_decl_scope &universal);

extern a_boolean has_ifc_initializer(const an_ifc_decl_scope &universal);

extern an_ifc_scope_index get_ifc_initializer(
                                           const an_ifc_decl_scope &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_scope &universal);

extern an_ifc_source_location get_ifc_locus(
                                           const an_ifc_decl_scope &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_scope &universal);

extern an_ifc_name_index get_ifc_name(const an_ifc_decl_scope &universal);

extern a_boolean has_ifc_pack_size(const an_ifc_decl_scope &universal);

extern an_ifc_pack_size get_ifc_pack_size(const an_ifc_decl_scope &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_scope &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                           const an_ifc_decl_scope &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_scope &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                           const an_ifc_decl_scope &universal);

extern a_boolean has_ifc_traits(const an_ifc_decl_scope &universal);

extern an_ifc_scope_traits_bitfield get_ifc_traits(
                                           const an_ifc_decl_scope &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_scope &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_scope &universal);

extern a_boolean validate(const an_ifc_decl_scope       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_scope &universal, unsigned indent);

extern void db_node(const an_ifc_decl_scope &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_scope_storage* get<an_ifc_decl_scope_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_decl_scope_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_scope_storage>()
/*
Return the corresponding partition kind for DeclScope.
*/
{
  return ifc_pk_decl_scope;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclSpecialization nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_decl_specialization &universal);

extern an_ifc_decl_index get_ifc_decl(
                                  const an_ifc_decl_specialization &universal);

extern a_boolean has_ifc_form(const an_ifc_decl_specialization &universal);

extern an_ifc_form_spec_index get_ifc_form(
                                  const an_ifc_decl_specialization &universal);

extern a_boolean has_ifc_home_scope(
                                  const an_ifc_decl_specialization &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                  const an_ifc_decl_specialization &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_specialization &universal);

extern an_ifc_source_location get_ifc_locus(
                                  const an_ifc_decl_specialization &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_specialization &universal);

extern an_ifc_name_index get_ifc_name(
                                  const an_ifc_decl_specialization &universal);

extern a_boolean has_ifc_sort(const an_ifc_decl_specialization &universal);

extern an_ifc_specialization_sort get_ifc_sort(
                                  const an_ifc_decl_specialization &universal);

extern a_boolean validate(const an_ifc_decl_specialization &universal,
                          const an_ifc_validation_trace    *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_specialization &universal,
                    unsigned                         indent);

extern void db_node(const an_ifc_decl_specialization &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_specialization_storage* get<an_ifc_decl_specialization_storage>(
                              an_ifc_module                      *mod,
                              an_ifc_decl_specialization_storage *storage,
                              a_boolean                          fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_specialization_storage>()
/*
Return the corresponding partition kind for DeclSpecialization.
*/
{
  return ifc_pk_decl_specialization;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclTemplate nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_template &universal);

extern an_ifc_access_sort get_ifc_access(
                                        const an_ifc_decl_template &universal);

extern a_boolean has_ifc_chart(const an_ifc_decl_template &universal);

extern an_ifc_chart_index get_ifc_chart(const an_ifc_decl_template &universal);

extern a_boolean has_ifc_entity(const an_ifc_decl_template &universal);

extern an_ifc_parameterized_entity get_ifc_entity(
                                        const an_ifc_decl_template &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_template &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                        const an_ifc_decl_template &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_template &universal);

extern an_ifc_source_location get_ifc_locus(
                                        const an_ifc_decl_template &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_template &universal);

extern an_ifc_name_index get_ifc_name(const an_ifc_decl_template &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_template &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                        const an_ifc_decl_template &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_template &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                        const an_ifc_decl_template &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_template &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_template &universal);

extern a_boolean validate(const an_ifc_decl_template    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_template &universal, unsigned indent);

extern void db_node(const an_ifc_decl_template &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_template_storage* get<an_ifc_decl_template_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_decl_template_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_template_storage>()
/*
Return the corresponding partition kind for DeclTemplate.
*/
{
  return ifc_pk_decl_template;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclTemploid nodes.
*/

extern a_boolean has_ifc_chart(const an_ifc_decl_temploid &universal);

extern an_ifc_chart_index get_ifc_chart(const an_ifc_decl_temploid &universal);

extern a_boolean has_ifc_entity(const an_ifc_decl_temploid &universal);

extern an_ifc_parameterized_entity get_ifc_entity(
                                        const an_ifc_decl_temploid &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_temploid &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                        const an_ifc_decl_temploid &universal);

extern a_boolean validate(const an_ifc_decl_temploid    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_temploid &universal, unsigned indent);

extern void db_node(const an_ifc_decl_temploid &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_temploid_storage* get<an_ifc_decl_temploid_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_decl_temploid_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_temploid_storage>()
/*
Return the corresponding partition kind for DeclTemploid.
*/
{
  return ifc_pk_decl_temploid;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclTuple nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_decl_tuple &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                           const an_ifc_decl_tuple &universal);

extern a_boolean has_ifc_start(const an_ifc_decl_tuple &universal);

extern an_ifc_index get_ifc_start(const an_ifc_decl_tuple &universal);

extern a_boolean validate(const an_ifc_decl_tuple       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_tuple &universal, unsigned indent);

extern void db_node(const an_ifc_decl_tuple &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_tuple_storage* get<an_ifc_decl_tuple_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_decl_tuple_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_tuple_storage>()
/*
Return the corresponding partition kind for DeclTuple.
*/
{
  return ifc_pk_decl_tuple;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclUsingDeclaration nodes.
*/

extern a_boolean has_ifc_access(
                               const an_ifc_decl_using_declaration &universal);

extern an_ifc_access_sort get_ifc_access(
                               const an_ifc_decl_using_declaration &universal);

extern a_boolean has_ifc_hidden(
                               const an_ifc_decl_using_declaration &universal);

extern an_ifc_bool get_ifc_hidden(
                               const an_ifc_decl_using_declaration &universal);

extern a_boolean has_ifc_home_scope(
                               const an_ifc_decl_using_declaration &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                               const an_ifc_decl_using_declaration &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_using_declaration &universal);

extern an_ifc_source_location get_ifc_locus(
                               const an_ifc_decl_using_declaration &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_using_declaration &universal);

extern an_ifc_text_offset get_ifc_name(
                               const an_ifc_decl_using_declaration &universal);

extern a_boolean has_ifc_name2(const an_ifc_decl_using_declaration &universal);

extern an_ifc_text_offset get_ifc_name2(
                               const an_ifc_decl_using_declaration &universal);

extern a_boolean has_ifc_parent(
                               const an_ifc_decl_using_declaration &universal);

extern an_ifc_expr_index get_ifc_parent(
                               const an_ifc_decl_using_declaration &universal);

extern a_boolean has_ifc_resolution(
                               const an_ifc_decl_using_declaration &universal);

extern an_ifc_decl_index get_ifc_resolution(
                               const an_ifc_decl_using_declaration &universal);

extern a_boolean has_ifc_specifiers(
                               const an_ifc_decl_using_declaration &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                               const an_ifc_decl_using_declaration &universal);

extern a_boolean validate(const an_ifc_decl_using_declaration &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_using_declaration &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_decl_using_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_using_declaration_storage*
get<an_ifc_decl_using_declaration_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_decl_using_declaration_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_using_declaration_storage>()
/*
Return the corresponding partition kind for DeclUsingDeclaration.
*/
{
  return ifc_pk_decl_using_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC DeclVariable nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_decl_variable &universal);

extern an_ifc_access_sort get_ifc_access(
                                        const an_ifc_decl_variable &universal);

extern a_boolean has_ifc_alignment(const an_ifc_decl_variable &universal);

extern an_ifc_expr_index get_ifc_alignment(
                                        const an_ifc_decl_variable &universal);

extern a_boolean has_ifc_home_scope(const an_ifc_decl_variable &universal);

extern an_ifc_decl_index get_ifc_home_scope(
                                        const an_ifc_decl_variable &universal);

extern a_boolean has_ifc_initializer(const an_ifc_decl_variable &universal);

extern an_ifc_expr_index get_ifc_initializer(
                                        const an_ifc_decl_variable &universal);

extern a_boolean has_ifc_locus(const an_ifc_decl_variable &universal);

extern an_ifc_source_location get_ifc_locus(
                                        const an_ifc_decl_variable &universal);

extern a_boolean has_ifc_name(const an_ifc_decl_variable &universal);

extern an_ifc_name_index get_ifc_name(const an_ifc_decl_variable &universal);

extern a_boolean has_ifc_properties(const an_ifc_decl_variable &universal);

extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                                        const an_ifc_decl_variable &universal);

extern a_boolean has_ifc_specifiers(const an_ifc_decl_variable &universal);

extern an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                        const an_ifc_decl_variable &universal);

extern a_boolean has_ifc_traits(const an_ifc_decl_variable &universal);

extern an_ifc_object_traits_bitfield get_ifc_traits(
                                        const an_ifc_decl_variable &universal);

extern a_boolean has_ifc_type(const an_ifc_decl_variable &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_decl_variable &universal);

extern a_boolean validate(const an_ifc_decl_variable    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_decl_variable &universal, unsigned indent);

extern void db_node(const an_ifc_decl_variable &universal);
#endif /* DEBUG */

template<>
an_ifc_decl_variable_storage* get<an_ifc_decl_variable_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_decl_variable_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_variable_storage>()
/*
Return the corresponding partition kind for DeclVariable.
*/
{
  return ifc_pk_decl_variable;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprAlignof nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_alignof &universal);

extern an_ifc_source_location get_ifc_locus(
                                         const an_ifc_expr_alignof &universal);

extern a_boolean has_ifc_operand(const an_ifc_expr_alignof &universal);

extern an_ifc_syntax_index get_ifc_operand(
                                         const an_ifc_expr_alignof &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_alignof &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_alignof &universal);

extern a_boolean validate(const an_ifc_expr_alignof     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_alignof &universal, unsigned indent);

extern void db_node(const an_ifc_expr_alignof &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_alignof_storage* get<an_ifc_expr_alignof_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_expr_alignof_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_alignof_storage>()
/*
Return the corresponding partition kind for ExprAlignof.
*/
{
  return ifc_pk_expr_alignof_type_id;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprArrayValue nodes.
*/

extern a_boolean has_ifc_element_type(
                                     const an_ifc_expr_array_value &universal);

extern an_ifc_type_index get_ifc_element_type(
                                     const an_ifc_expr_array_value &universal);

extern a_boolean has_ifc_elements(const an_ifc_expr_array_value &universal);

extern an_ifc_expr_index get_ifc_elements(
                                     const an_ifc_expr_array_value &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_array_value &universal);

extern an_ifc_source_location get_ifc_locus(
                                     const an_ifc_expr_array_value &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_array_value &universal);

extern an_ifc_type_index get_ifc_type(
                                     const an_ifc_expr_array_value &universal);

extern a_boolean validate(const an_ifc_expr_array_value &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_array_value &universal, unsigned indent);

extern void db_node(const an_ifc_expr_array_value &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_array_value_storage* get<an_ifc_expr_array_value_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_expr_array_value_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_array_value_storage>()
/*
Return the corresponding partition kind for ExprArrayValue.
*/
{
  return ifc_pk_expr_array_value;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprAssignInitializer nodes.
*/

extern a_boolean has_ifc_equal(
                              const an_ifc_expr_assign_initializer &universal);

extern an_ifc_source_location get_ifc_equal(
                              const an_ifc_expr_assign_initializer &universal);

extern a_boolean has_ifc_initializer(
                              const an_ifc_expr_assign_initializer &universal);

extern an_ifc_expr_index get_ifc_initializer(
                              const an_ifc_expr_assign_initializer &universal);

extern a_boolean validate(const an_ifc_expr_assign_initializer &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_assign_initializer &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_expr_assign_initializer &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_assign_initializer_storage*
get<an_ifc_expr_assign_initializer_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_expr_assign_initializer_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_assign_initializer_storage>()
/*
Return the corresponding partition kind for ExprAssignInitializer.
*/
{
  return ifc_pk_expr_assign_initializer;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprBinaryFold nodes.
*/

extern a_boolean has_ifc_associativity(
                                     const an_ifc_expr_binary_fold &universal);

extern an_ifc_associativity get_ifc_associativity(
                                     const an_ifc_expr_binary_fold &universal);

extern a_boolean has_ifc_left(const an_ifc_expr_binary_fold &universal);

extern an_ifc_expr_index get_ifc_left(
                                     const an_ifc_expr_binary_fold &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_binary_fold &universal);

extern an_ifc_source_location get_ifc_locus(
                                     const an_ifc_expr_binary_fold &universal);

extern a_boolean has_ifc_operation(const an_ifc_expr_binary_fold &universal);

extern an_ifc_dyadic_operator_sort get_ifc_operation(
                                     const an_ifc_expr_binary_fold &universal);

extern a_boolean has_ifc_right(const an_ifc_expr_binary_fold &universal);

extern an_ifc_expr_index get_ifc_right(
                                     const an_ifc_expr_binary_fold &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_binary_fold &universal);

extern an_ifc_type_index get_ifc_type(
                                     const an_ifc_expr_binary_fold &universal);

extern a_boolean validate(const an_ifc_expr_binary_fold &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_binary_fold &universal, unsigned indent);

extern void db_node(const an_ifc_expr_binary_fold &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_binary_fold_storage* get<an_ifc_expr_binary_fold_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_expr_binary_fold_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_binary_fold_storage>()
/*
Return the corresponding partition kind for ExprBinaryFold.
*/
{
  return ifc_pk_expr_binary_fold;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprCall nodes.
*/

extern a_boolean has_ifc_arguments(const an_ifc_expr_call &universal);

extern an_ifc_expr_index get_ifc_arguments(const an_ifc_expr_call &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_call &universal);

extern an_ifc_source_location get_ifc_locus(const an_ifc_expr_call &universal);

extern a_boolean has_ifc_operation(const an_ifc_expr_call &universal);

extern an_ifc_expr_index get_ifc_operation(const an_ifc_expr_call &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_call &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_call &universal);

extern a_boolean validate(const an_ifc_expr_call        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_call &universal, unsigned indent);

extern void db_node(const an_ifc_expr_call &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_call_storage* get<an_ifc_expr_call_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_expr_call_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_call_storage>()
/*
Return the corresponding partition kind for ExprCall.
*/
{
  return ifc_pk_expr_call;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprCast nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_cast &universal);

extern an_ifc_source_location get_ifc_locus(const an_ifc_expr_cast &universal);

extern a_boolean has_ifc_op(const an_ifc_expr_cast &universal);

extern an_ifc_dyadic_operator_sort get_ifc_op(
                                            const an_ifc_expr_cast &universal);

extern a_boolean has_ifc_source(const an_ifc_expr_cast &universal);

extern an_ifc_expr_index get_ifc_source(const an_ifc_expr_cast &universal);

extern a_boolean has_ifc_target(const an_ifc_expr_cast &universal);

extern an_ifc_type_index get_ifc_target(const an_ifc_expr_cast &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_cast &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_cast &universal);

extern a_boolean validate(const an_ifc_expr_cast        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_cast &universal, unsigned indent);

extern void db_node(const an_ifc_expr_cast &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_cast_storage* get<an_ifc_expr_cast_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_expr_cast_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_cast_storage>()
/*
Return the corresponding partition kind for ExprCast.
*/
{
  return ifc_pk_expr_cast;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprCompoundString nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_compound_string &universal);

extern an_ifc_source_location get_ifc_locus(
                                 const an_ifc_expr_compound_string &universal);

extern a_boolean has_ifc_prefix(const an_ifc_expr_compound_string &universal);

extern an_ifc_text_offset get_ifc_prefix(
                                 const an_ifc_expr_compound_string &universal);

extern a_boolean has_ifc_string(const an_ifc_expr_compound_string &universal);

extern an_ifc_expr_index get_ifc_string(
                                 const an_ifc_expr_compound_string &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_compound_string &universal);

extern an_ifc_type_index get_ifc_type(
                                 const an_ifc_expr_compound_string &universal);

extern a_boolean validate(const an_ifc_expr_compound_string &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_compound_string &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_expr_compound_string &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_compound_string_storage* get<an_ifc_expr_compound_string_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_expr_compound_string_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_compound_string_storage>()
/*
Return the corresponding partition kind for ExprCompoundString.
*/
{
  return ifc_pk_expr_compound_string;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprCondition nodes.
*/

extern a_boolean has_ifc_expr(const an_ifc_expr_condition &universal);

extern an_ifc_expr_index get_ifc_expr(const an_ifc_expr_condition &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_condition &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_expr_condition &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_condition &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_condition &universal);

extern a_boolean validate(const an_ifc_expr_condition   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_condition &universal, unsigned indent);

extern void db_node(const an_ifc_expr_condition &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_condition_storage* get<an_ifc_expr_condition_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_expr_condition_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_condition_storage>()
/*
Return the corresponding partition kind for ExprCondition.
*/
{
  return ifc_pk_expr_condition;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprDesignatedInitializer nodes.
*/

extern a_boolean has_ifc_initializer(
                          const an_ifc_expr_designated_initializer &universal);

extern an_ifc_expr_index get_ifc_initializer(
                          const an_ifc_expr_designated_initializer &universal);

extern a_boolean has_ifc_locus(
                          const an_ifc_expr_designated_initializer &universal);

extern an_ifc_source_location get_ifc_locus(
                          const an_ifc_expr_designated_initializer &universal);

extern a_boolean has_ifc_member(
                          const an_ifc_expr_designated_initializer &universal);

extern an_ifc_text_offset get_ifc_member(
                          const an_ifc_expr_designated_initializer &universal);

extern a_boolean has_ifc_type(
                          const an_ifc_expr_designated_initializer &universal);

extern an_ifc_type_index get_ifc_type(
                          const an_ifc_expr_designated_initializer &universal);

extern a_boolean validate(const an_ifc_expr_designated_initializer &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_designated_initializer &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_expr_designated_initializer &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_designated_initializer_storage*
get<an_ifc_expr_designated_initializer_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_expr_designated_initializer_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_designated_initializer_storage>()
/*
Return the corresponding partition kind for ExprDesignatedInitializer.
*/
{
  return ifc_pk_expr_designated_init;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprDestructorCall nodes.
*/

extern a_boolean has_ifc_cleanup(const an_ifc_expr_destructor_call &universal);

extern an_ifc_destructor_sort get_ifc_cleanup(
                                 const an_ifc_expr_destructor_call &universal);

extern a_boolean has_ifc_decltype_specifier(
                                 const an_ifc_expr_destructor_call &universal);

extern an_ifc_syntax_index get_ifc_decltype_specifier(
                                 const an_ifc_expr_destructor_call &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_destructor_call &universal);

extern an_ifc_source_location get_ifc_locus(
                                 const an_ifc_expr_destructor_call &universal);

extern a_boolean has_ifc_name(const an_ifc_expr_destructor_call &universal);

extern an_ifc_expr_index get_ifc_name(
                                 const an_ifc_expr_destructor_call &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_destructor_call &universal);

extern an_ifc_type_index get_ifc_type(
                                 const an_ifc_expr_destructor_call &universal);

extern a_boolean validate(const an_ifc_expr_destructor_call &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_destructor_call &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_expr_destructor_call &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_destructor_call_storage* get<an_ifc_expr_destructor_call_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_expr_destructor_call_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_destructor_call_storage>()
/*
Return the corresponding partition kind for ExprDestructorCall.
*/
{
  return ifc_pk_expr_destructor_call;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprDyad nodes.
*/

extern a_boolean has_ifc_argument_0(const an_ifc_expr_dyad &universal);

extern an_ifc_expr_index get_ifc_argument_0(const an_ifc_expr_dyad &universal);

extern a_boolean has_ifc_argument_1(const an_ifc_expr_dyad &universal);

extern an_ifc_expr_index get_ifc_argument_1(const an_ifc_expr_dyad &universal);

extern a_boolean has_ifc_assoc(const an_ifc_expr_dyad &universal);

extern an_ifc_dyadic_operator_sort get_ifc_assoc(
                                            const an_ifc_expr_dyad &universal);

extern a_boolean has_ifc_impl(const an_ifc_expr_dyad &universal);

extern an_ifc_decl_index get_ifc_impl(const an_ifc_expr_dyad &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_dyad &universal);

extern an_ifc_source_location get_ifc_locus(const an_ifc_expr_dyad &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_dyad &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_dyad &universal);

extern a_boolean validate(const an_ifc_expr_dyad        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_dyad &universal, unsigned indent);

extern void db_node(const an_ifc_expr_dyad &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_dyad_storage* get<an_ifc_expr_dyad_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_expr_dyad_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_dyad_storage>()
/*
Return the corresponding partition kind for ExprDyad.
*/
{
  return ifc_pk_expr_dyad;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprDynamicDispatch nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_dynamic_dispatch &universal);

extern an_ifc_source_location get_ifc_locus(
                                const an_ifc_expr_dynamic_dispatch &universal);

extern a_boolean has_ifc_pivot(const an_ifc_expr_dynamic_dispatch &universal);

extern an_ifc_expr_index get_ifc_pivot(
                                const an_ifc_expr_dynamic_dispatch &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_dynamic_dispatch &universal);

extern an_ifc_type_index get_ifc_type(
                                const an_ifc_expr_dynamic_dispatch &universal);

extern a_boolean validate(const an_ifc_expr_dynamic_dispatch &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_dynamic_dispatch &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_expr_dynamic_dispatch &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_dynamic_dispatch_storage*
get<an_ifc_expr_dynamic_dispatch_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_expr_dynamic_dispatch_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_dynamic_dispatch_storage>()
/*
Return the corresponding partition kind for ExprDynamicDispatch.
*/
{
  return ifc_pk_expr_dynamic_dispatch;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprEmpty nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_empty &universal);

extern an_ifc_source_location get_ifc_locus(
                                           const an_ifc_expr_empty &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_empty &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_empty &universal);

extern a_boolean validate(const an_ifc_expr_empty       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_empty &universal, unsigned indent);

extern void db_node(const an_ifc_expr_empty &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_empty_storage* get<an_ifc_expr_empty_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_expr_empty_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_empty_storage>()
/*
Return the corresponding partition kind for ExprEmpty.
*/
{
  return ifc_pk_expr_empty;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprExpansion nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_expansion &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_expr_expansion &universal);

extern a_boolean has_ifc_operand(const an_ifc_expr_expansion &universal);

extern an_ifc_expr_index get_ifc_operand(
                                       const an_ifc_expr_expansion &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_expansion &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_expansion &universal);

extern a_boolean validate(const an_ifc_expr_expansion   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_expansion &universal, unsigned indent);

extern void db_node(const an_ifc_expr_expansion &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_expansion_storage* get<an_ifc_expr_expansion_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_expr_expansion_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_expansion_storage>()
/*
Return the corresponding partition kind for ExprExpansion.
*/
{
  return ifc_pk_expr_expansion;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprExpressionList nodes.
*/

extern a_boolean has_ifc_contents(
                                 const an_ifc_expr_expression_list &universal);

extern an_ifc_expr_index get_ifc_contents(
                                 const an_ifc_expr_expression_list &universal);

extern a_boolean has_ifc_delimiter(
                                 const an_ifc_expr_expression_list &universal);

extern an_ifc_delimiter_sort get_ifc_delimiter(
                                 const an_ifc_expr_expression_list &universal);

extern a_boolean has_ifc_left(const an_ifc_expr_expression_list &universal);

extern an_ifc_source_location get_ifc_left(
                                 const an_ifc_expr_expression_list &universal);

extern a_boolean has_ifc_right(const an_ifc_expr_expression_list &universal);

extern an_ifc_source_location get_ifc_right(
                                 const an_ifc_expr_expression_list &universal);

extern a_boolean validate(const an_ifc_expr_expression_list &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_expression_list &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_expr_expression_list &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_expression_list_storage* get<an_ifc_expr_expression_list_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_expr_expression_list_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_expression_list_storage>()
/*
Return the corresponding partition kind for ExprExpressionList.
*/
{
  return ifc_pk_expr_expression_list;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprFunctionString nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_function_string &universal);

extern an_ifc_source_location get_ifc_locus(
                                 const an_ifc_expr_function_string &universal);

extern a_boolean has_ifc_macro(const an_ifc_expr_function_string &universal);

extern an_ifc_text_offset get_ifc_macro(
                                 const an_ifc_expr_function_string &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_function_string &universal);

extern an_ifc_type_index get_ifc_type(
                                 const an_ifc_expr_function_string &universal);

extern a_boolean validate(const an_ifc_expr_function_string &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_function_string &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_expr_function_string &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_function_string_storage* get<an_ifc_expr_function_string_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_expr_function_string_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_function_string_storage>()
/*
Return the corresponding partition kind for ExprFunctionString.
*/
{
  return ifc_pk_expr_function_string;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprHierarchyConversion nodes.
*/

extern a_boolean has_ifc_inheritance(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern an_ifc_expr_index get_ifc_inheritance(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern a_boolean has_ifc_locus(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern an_ifc_source_location get_ifc_locus(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern a_boolean has_ifc_op(const an_ifc_expr_hierarchy_conversion &universal);

extern an_ifc_dyadic_operator_sort get_ifc_op(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern a_boolean has_ifc_override(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern an_ifc_expr_index get_ifc_override(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern a_boolean has_ifc_source(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern an_ifc_expr_index get_ifc_source(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern a_boolean has_ifc_target(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern an_ifc_type_index get_ifc_target(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern a_boolean has_ifc_type(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern an_ifc_type_index get_ifc_type(
                            const an_ifc_expr_hierarchy_conversion &universal);

extern a_boolean validate(const an_ifc_expr_hierarchy_conversion &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_hierarchy_conversion &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_expr_hierarchy_conversion &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_hierarchy_conversion_storage*
get<an_ifc_expr_hierarchy_conversion_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_expr_hierarchy_conversion_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_hierarchy_conversion_storage>()
/*
Return the corresponding partition kind for ExprHierarchyConversion.
*/
{
  return ifc_pk_expr_hierarchy_conversion;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprInheritancePath nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_inheritance_path &universal);

extern an_ifc_source_location get_ifc_locus(
                                const an_ifc_expr_inheritance_path &universal);

extern a_boolean has_ifc_path(const an_ifc_expr_inheritance_path &universal);

extern an_ifc_expr_index get_ifc_path(
                                const an_ifc_expr_inheritance_path &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_inheritance_path &universal);

extern an_ifc_type_index get_ifc_type(
                                const an_ifc_expr_inheritance_path &universal);

extern a_boolean validate(const an_ifc_expr_inheritance_path &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_inheritance_path &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_expr_inheritance_path &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_inheritance_path_storage*
get<an_ifc_expr_inheritance_path_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_expr_inheritance_path_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_inheritance_path_storage>()
/*
Return the corresponding partition kind for ExprInheritancePath.
*/
{
  return ifc_pk_expr_inheritance_path;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprInitializer nodes.
*/

extern a_boolean has_ifc_expr(const an_ifc_expr_initializer &universal);

extern an_ifc_expr_index get_ifc_expr(
                                     const an_ifc_expr_initializer &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_initializer &universal);

extern an_ifc_source_location get_ifc_locus(
                                     const an_ifc_expr_initializer &universal);

extern a_boolean has_ifc_sort(const an_ifc_expr_initializer &universal);

extern an_ifc_initializer_sort get_ifc_sort(
                                     const an_ifc_expr_initializer &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_initializer &universal);

extern an_ifc_type_index get_ifc_type(
                                     const an_ifc_expr_initializer &universal);

extern a_boolean validate(const an_ifc_expr_initializer &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_initializer &universal, unsigned indent);

extern void db_node(const an_ifc_expr_initializer &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_initializer_storage* get<an_ifc_expr_initializer_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_expr_initializer_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_initializer_storage>()
/*
Return the corresponding partition kind for ExprInitializer.
*/
{
  return ifc_pk_expr_initializer;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprInitializerList nodes.
*/

extern a_boolean has_ifc_elements(
                                const an_ifc_expr_initializer_list &universal);

extern an_ifc_expr_index get_ifc_elements(
                                const an_ifc_expr_initializer_list &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_initializer_list &universal);

extern an_ifc_source_location get_ifc_locus(
                                const an_ifc_expr_initializer_list &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_initializer_list &universal);

extern an_ifc_type_index get_ifc_type(
                                const an_ifc_expr_initializer_list &universal);

extern a_boolean validate(const an_ifc_expr_initializer_list &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_initializer_list &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_expr_initializer_list &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_initializer_list_storage*
get<an_ifc_expr_initializer_list_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_expr_initializer_list_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_initializer_list_storage>()
/*
Return the corresponding partition kind for ExprInitializerList.
*/
{
  return ifc_pk_expr_initializer_list;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprLambda nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_expr_lambda &universal);

extern an_ifc_syntax_index get_ifc_body(const an_ifc_expr_lambda &universal);

extern a_boolean has_ifc_constraint(const an_ifc_expr_lambda &universal);

extern an_ifc_syntax_index get_ifc_constraint(
                                          const an_ifc_expr_lambda &universal);

extern a_boolean has_ifc_declarator(const an_ifc_expr_lambda &universal);

extern an_ifc_syntax_index get_ifc_declarator(
                                          const an_ifc_expr_lambda &universal);

extern a_boolean has_ifc_introducer(const an_ifc_expr_lambda &universal);

extern an_ifc_syntax_index get_ifc_introducer(
                                          const an_ifc_expr_lambda &universal);

extern a_boolean has_ifc_template_parameters(
                                          const an_ifc_expr_lambda &universal);

extern an_ifc_syntax_index get_ifc_template_parameters(
                                          const an_ifc_expr_lambda &universal);

extern a_boolean validate(const an_ifc_expr_lambda      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_lambda &universal, unsigned indent);

extern void db_node(const an_ifc_expr_lambda &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_lambda_storage* get<an_ifc_expr_lambda_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_expr_lambda_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_lambda_storage>()
/*
Return the corresponding partition kind for ExprLambda.
*/
{
  return ifc_pk_expr_lambda;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprLiteral nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_literal &universal);

extern an_ifc_source_location get_ifc_locus(
                                         const an_ifc_expr_literal &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_literal &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_literal &universal);

extern a_boolean has_ifc_value(const an_ifc_expr_literal &universal);

extern an_ifc_lit_index get_ifc_value(const an_ifc_expr_literal &universal);

extern a_boolean validate(const an_ifc_expr_literal     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_literal &universal, unsigned indent);

extern void db_node(const an_ifc_expr_literal &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_literal_storage* get<an_ifc_expr_literal_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_expr_literal_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_literal_storage>()
/*
Return the corresponding partition kind for ExprLiteral.
*/
{
  return ifc_pk_expr_literal;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprMemberAccess nodes.
*/

extern a_boolean has_ifc_enclosing(const an_ifc_expr_member_access &universal);

extern an_ifc_type_index get_ifc_enclosing(
                                   const an_ifc_expr_member_access &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_member_access &universal);

extern an_ifc_source_location get_ifc_locus(
                                   const an_ifc_expr_member_access &universal);

extern a_boolean has_ifc_name(const an_ifc_expr_member_access &universal);

extern an_ifc_text_offset get_ifc_name(
                                   const an_ifc_expr_member_access &universal);

extern a_boolean has_ifc_offset(const an_ifc_expr_member_access &universal);

extern an_ifc_expr_index get_ifc_offset(
                                   const an_ifc_expr_member_access &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_member_access &universal);

extern an_ifc_type_index get_ifc_type(
                                   const an_ifc_expr_member_access &universal);

extern a_boolean validate(const an_ifc_expr_member_access &universal,
                          const an_ifc_validation_trace   *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_member_access &universal,
                    unsigned                        indent);

extern void db_node(const an_ifc_expr_member_access &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_member_access_storage* get<an_ifc_expr_member_access_storage>(
                               an_ifc_module                     *mod,
                               an_ifc_expr_member_access_storage *storage,
                               a_boolean                         fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_member_access_storage>()
/*
Return the corresponding partition kind for ExprMemberAccess.
*/
{
  return ifc_pk_expr_member_access;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprMemberInitializer nodes.
*/

extern a_boolean has_ifc_base(const an_ifc_expr_member_initializer &universal);

extern an_ifc_type_index get_ifc_base(
                              const an_ifc_expr_member_initializer &universal);

extern a_boolean has_ifc_initializer(
                              const an_ifc_expr_member_initializer &universal);

extern an_ifc_expr_index get_ifc_initializer(
                              const an_ifc_expr_member_initializer &universal);

extern a_boolean has_ifc_locus(
                              const an_ifc_expr_member_initializer &universal);

extern an_ifc_source_location get_ifc_locus(
                              const an_ifc_expr_member_initializer &universal);

extern a_boolean has_ifc_member(
                              const an_ifc_expr_member_initializer &universal);

extern an_ifc_decl_index get_ifc_member(
                              const an_ifc_expr_member_initializer &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_member_initializer &universal);

extern an_ifc_type_index get_ifc_type(
                              const an_ifc_expr_member_initializer &universal);

extern a_boolean validate(const an_ifc_expr_member_initializer &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_member_initializer &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_expr_member_initializer &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_member_initializer_storage*
get<an_ifc_expr_member_initializer_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_expr_member_initializer_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_member_initializer_storage>()
/*
Return the corresponding partition kind for ExprMemberInitializer.
*/
{
  return ifc_pk_expr_member_initializer;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprMonad nodes.
*/

extern a_boolean has_ifc_argument(const an_ifc_expr_monad &universal);

extern an_ifc_expr_index get_ifc_argument(const an_ifc_expr_monad &universal);

extern a_boolean has_ifc_assoc(const an_ifc_expr_monad &universal);

extern an_ifc_monadic_operator_sort get_ifc_assoc(
                                           const an_ifc_expr_monad &universal);

extern a_boolean has_ifc_impl(const an_ifc_expr_monad &universal);

extern an_ifc_decl_index get_ifc_impl(const an_ifc_expr_monad &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_monad &universal);

extern an_ifc_source_location get_ifc_locus(
                                           const an_ifc_expr_monad &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_monad &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_monad &universal);

extern a_boolean validate(const an_ifc_expr_monad       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_monad &universal, unsigned indent);

extern void db_node(const an_ifc_expr_monad &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_monad_storage* get<an_ifc_expr_monad_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_expr_monad_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_monad_storage>()
/*
Return the corresponding partition kind for ExprMonad.
*/
{
  return ifc_pk_expr_monad;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprNamedDecl nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_named_decl &universal);

extern an_ifc_source_location get_ifc_locus(
                                      const an_ifc_expr_named_decl &universal);

extern a_boolean has_ifc_resolution(const an_ifc_expr_named_decl &universal);

extern an_ifc_decl_index get_ifc_resolution(
                                      const an_ifc_expr_named_decl &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_named_decl &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_named_decl &universal);

extern a_boolean validate(const an_ifc_expr_named_decl  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_named_decl &universal, unsigned indent);

extern void db_node(const an_ifc_expr_named_decl &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_named_decl_storage* get<an_ifc_expr_named_decl_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_expr_named_decl_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_named_decl_storage>()
/*
Return the corresponding partition kind for ExprNamedDecl.
*/
{
  return ifc_pk_expr_decl;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprNullptr nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_nullptr &universal);

extern an_ifc_source_location get_ifc_locus(
                                         const an_ifc_expr_nullptr &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_nullptr &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_nullptr &universal);

extern a_boolean validate(const an_ifc_expr_nullptr     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_nullptr &universal, unsigned indent);

extern void db_node(const an_ifc_expr_nullptr &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_nullptr_storage* get<an_ifc_expr_nullptr_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_expr_nullptr_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_nullptr_storage>()
/*
Return the corresponding partition kind for ExprNullptr.
*/
{
  return ifc_pk_expr_nullptr;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprPackedTemplateArguments nodes.
*/

extern a_boolean has_ifc_arguments(
                       const an_ifc_expr_packed_template_arguments &universal);

extern an_ifc_expr_index get_ifc_arguments(
                       const an_ifc_expr_packed_template_arguments &universal);

extern a_boolean has_ifc_locus(
                       const an_ifc_expr_packed_template_arguments &universal);

extern an_ifc_source_location get_ifc_locus(
                       const an_ifc_expr_packed_template_arguments &universal);

extern a_boolean has_ifc_type(
                       const an_ifc_expr_packed_template_arguments &universal);

extern an_ifc_type_index get_ifc_type(
                       const an_ifc_expr_packed_template_arguments &universal);

extern a_boolean validate(
                       const an_ifc_expr_packed_template_arguments &universal,
                       const an_ifc_validation_trace               *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_packed_template_arguments &universal,
                    unsigned                                    indent);

extern void db_node(const an_ifc_expr_packed_template_arguments &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_packed_template_arguments_storage*
get<an_ifc_expr_packed_template_arguments_storage>(
                   an_ifc_module                                 *mod,
                   an_ifc_expr_packed_template_arguments_storage *storage,
                   a_boolean                                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_packed_template_arguments_storage>()
/*
Return the corresponding partition kind for ExprPackedTemplateArguments.
*/
{
  return ifc_pk_expr_packed_template_arguments;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprPath nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_path &universal);

extern an_ifc_source_location get_ifc_locus(const an_ifc_expr_path &universal);

extern a_boolean has_ifc_member(const an_ifc_expr_path &universal);

extern an_ifc_expr_index get_ifc_member(const an_ifc_expr_path &universal);

extern a_boolean has_ifc_scope(const an_ifc_expr_path &universal);

extern an_ifc_expr_index get_ifc_scope(const an_ifc_expr_path &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_path &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_path &universal);

extern a_boolean validate(const an_ifc_expr_path        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_path &universal, unsigned indent);

extern void db_node(const an_ifc_expr_path &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_path_storage* get<an_ifc_expr_path_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_expr_path_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_path_storage>()
/*
Return the corresponding partition kind for ExprPath.
*/
{
  return ifc_pk_expr_path;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprPlaceholder nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_placeholder &universal);

extern an_ifc_source_location get_ifc_locus(
                                     const an_ifc_expr_placeholder &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_placeholder &universal);

extern an_ifc_type_index get_ifc_type(
                                     const an_ifc_expr_placeholder &universal);

extern a_boolean validate(const an_ifc_expr_placeholder &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_placeholder &universal, unsigned indent);

extern void db_node(const an_ifc_expr_placeholder &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_placeholder_storage* get<an_ifc_expr_placeholder_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_expr_placeholder_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_placeholder_storage>()
/*
Return the corresponding partition kind for ExprPlaceholder.
*/
{
  return ifc_pk_expr_placeholder;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprPointer nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_pointer &universal);

extern an_ifc_source_location get_ifc_locus(
                                         const an_ifc_expr_pointer &universal);

extern a_boolean validate(const an_ifc_expr_pointer     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_pointer &universal, unsigned indent);

extern void db_node(const an_ifc_expr_pointer &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_pointer_storage* get<an_ifc_expr_pointer_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_expr_pointer_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_pointer_storage>()
/*
Return the corresponding partition kind for ExprPointer.
*/
{
  return ifc_pk_expr_pointer;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprProductTypeValue nodes.
*/

extern a_boolean has_ifc_base_subobjects(
                              const an_ifc_expr_product_type_value &universal);

extern an_ifc_expr_index get_ifc_base_subobjects(
                              const an_ifc_expr_product_type_value &universal);

extern a_boolean has_ifc_class_decl(
                              const an_ifc_expr_product_type_value &universal);

extern an_ifc_type_index get_ifc_class_decl(
                              const an_ifc_expr_product_type_value &universal);

extern a_boolean has_ifc_locus(
                              const an_ifc_expr_product_type_value &universal);

extern an_ifc_source_location get_ifc_locus(
                              const an_ifc_expr_product_type_value &universal);

extern a_boolean has_ifc_members(
                              const an_ifc_expr_product_type_value &universal);

extern an_ifc_expr_index get_ifc_members(
                              const an_ifc_expr_product_type_value &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_product_type_value &universal);

extern an_ifc_type_index get_ifc_type(
                              const an_ifc_expr_product_type_value &universal);

extern a_boolean validate(const an_ifc_expr_product_type_value &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_product_type_value &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_expr_product_type_value &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_product_type_value_storage*
get<an_ifc_expr_product_type_value_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_expr_product_type_value_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_product_type_value_storage>()
/*
Return the corresponding partition kind for ExprProductTypeValue.
*/
{
  return ifc_pk_expr_product_type_value;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprPushState nodes.
*/

extern a_boolean has_ifc_ctor_call(const an_ifc_expr_push_state &universal);

extern an_ifc_expr_index get_ifc_ctor_call(
                                      const an_ifc_expr_push_state &universal);

extern a_boolean has_ifc_dtor_call(const an_ifc_expr_push_state &universal);

extern an_ifc_expr_index get_ifc_dtor_call(
                                      const an_ifc_expr_push_state &universal);

extern a_boolean has_ifc_flags(const an_ifc_expr_push_state &universal);

extern an_ifc_eh_flags get_ifc_flags(const an_ifc_expr_push_state &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_push_state &universal);

extern an_ifc_source_location get_ifc_locus(
                                      const an_ifc_expr_push_state &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_push_state &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_push_state &universal);

extern a_boolean validate(const an_ifc_expr_push_state  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_push_state &universal, unsigned indent);

extern void db_node(const an_ifc_expr_push_state &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_push_state_storage* get<an_ifc_expr_push_state_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_expr_push_state_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_push_state_storage>()
/*
Return the corresponding partition kind for ExprPushState.
*/
{
  return ifc_pk_expr_push_state;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprQualifiedName nodes.
*/

extern a_boolean has_ifc_elements(const an_ifc_expr_qualified_name &universal);

extern an_ifc_expr_index get_ifc_elements(
                                  const an_ifc_expr_qualified_name &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_qualified_name &universal);

extern an_ifc_source_location get_ifc_locus(
                                  const an_ifc_expr_qualified_name &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_qualified_name &universal);

extern an_ifc_type_index get_ifc_type(
                                  const an_ifc_expr_qualified_name &universal);

extern a_boolean has_ifc_typename_keyword(
                                  const an_ifc_expr_qualified_name &universal);

extern an_ifc_source_location get_ifc_typename_keyword(
                                  const an_ifc_expr_qualified_name &universal);

extern a_boolean validate(const an_ifc_expr_qualified_name &universal,
                          const an_ifc_validation_trace    *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_qualified_name &universal,
                    unsigned                         indent);

extern void db_node(const an_ifc_expr_qualified_name &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_qualified_name_storage* get<an_ifc_expr_qualified_name_storage>(
                              an_ifc_module                      *mod,
                              an_ifc_expr_qualified_name_storage *storage,
                              a_boolean                          fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_qualified_name_storage>()
/*
Return the corresponding partition kind for ExprQualifiedName.
*/
{
  return ifc_pk_expr_qualified_name;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprRead nodes.
*/

extern a_boolean has_ifc_address(const an_ifc_expr_read &universal);

extern an_ifc_expr_index get_ifc_address(const an_ifc_expr_read &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_read &universal);

extern an_ifc_source_location get_ifc_locus(const an_ifc_expr_read &universal);

extern a_boolean has_ifc_sort(const an_ifc_expr_read &universal);

extern an_ifc_read_conversion_sort get_ifc_sort(
                                            const an_ifc_expr_read &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_read &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_read &universal);

extern a_boolean validate(const an_ifc_expr_read        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_read &universal, unsigned indent);

extern void db_node(const an_ifc_expr_read &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_read_storage* get<an_ifc_expr_read_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_expr_read_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_read_storage>()
/*
Return the corresponding partition kind for ExprRead.
*/
{
  return ifc_pk_expr_read;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprRequires nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_expr_requires &universal);

extern an_ifc_syntax_index get_ifc_body(const an_ifc_expr_requires &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_requires &universal);

extern an_ifc_source_location get_ifc_locus(
                                        const an_ifc_expr_requires &universal);

extern a_boolean has_ifc_parameters(const an_ifc_expr_requires &universal);

extern an_ifc_syntax_index get_ifc_parameters(
                                        const an_ifc_expr_requires &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_requires &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_requires &universal);

extern a_boolean validate(const an_ifc_expr_requires    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_requires &universal, unsigned indent);

extern void db_node(const an_ifc_expr_requires &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_requires_storage* get<an_ifc_expr_requires_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_expr_requires_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_requires_storage>()
/*
Return the corresponding partition kind for ExprRequires.
*/
{
  return ifc_pk_expr_requires;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprSimpleIdentifier nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_simple_identifier &universal);

extern an_ifc_source_location get_ifc_locus(
                               const an_ifc_expr_simple_identifier &universal);

extern a_boolean has_ifc_name(const an_ifc_expr_simple_identifier &universal);

extern an_ifc_name_index get_ifc_name(
                               const an_ifc_expr_simple_identifier &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_simple_identifier &universal);

extern an_ifc_type_index get_ifc_type(
                               const an_ifc_expr_simple_identifier &universal);

extern a_boolean validate(const an_ifc_expr_simple_identifier &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_simple_identifier &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_expr_simple_identifier &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_simple_identifier_storage*
get<an_ifc_expr_simple_identifier_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_expr_simple_identifier_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_simple_identifier_storage>()
/*
Return the corresponding partition kind for ExprSimpleIdentifier.
*/
{
  return ifc_pk_expr_simple_identifier;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprSizeofType nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_sizeof_type &universal);

extern an_ifc_source_location get_ifc_locus(
                                     const an_ifc_expr_sizeof_type &universal);

extern a_boolean has_ifc_operand(const an_ifc_expr_sizeof_type &universal);

extern an_ifc_type_index get_ifc_operand(
                                     const an_ifc_expr_sizeof_type &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_sizeof_type &universal);

extern an_ifc_type_index get_ifc_type(
                                     const an_ifc_expr_sizeof_type &universal);

extern a_boolean validate(const an_ifc_expr_sizeof_type &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_sizeof_type &universal, unsigned indent);

extern void db_node(const an_ifc_expr_sizeof_type &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_sizeof_type_storage* get<an_ifc_expr_sizeof_type_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_expr_sizeof_type_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_sizeof_type_storage>()
/*
Return the corresponding partition kind for ExprSizeofType.
*/
{
  return ifc_pk_expr_sizeof_type;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprString nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_string &universal);

extern an_ifc_source_location get_ifc_locus(
                                          const an_ifc_expr_string &universal);

extern a_boolean has_ifc_string_index(const an_ifc_expr_string &universal);

extern an_ifc_string_index get_ifc_string_index(
                                          const an_ifc_expr_string &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_string &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_string &universal);

extern a_boolean validate(const an_ifc_expr_string      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_string &universal, unsigned indent);

extern void db_node(const an_ifc_expr_string &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_string_storage* get<an_ifc_expr_string_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_expr_string_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_string_storage>()
/*
Return the corresponding partition kind for ExprString.
*/
{
  return ifc_pk_expr_strings;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprStringSequence nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_string_sequence &universal);

extern an_ifc_source_location get_ifc_locus(
                                 const an_ifc_expr_string_sequence &universal);

extern a_boolean has_ifc_strings(const an_ifc_expr_string_sequence &universal);

extern an_ifc_expr_index get_ifc_strings(
                                 const an_ifc_expr_string_sequence &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_string_sequence &universal);

extern an_ifc_type_index get_ifc_type(
                                 const an_ifc_expr_string_sequence &universal);

extern a_boolean validate(const an_ifc_expr_string_sequence &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_string_sequence &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_expr_string_sequence &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_string_sequence_storage* get<an_ifc_expr_string_sequence_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_expr_string_sequence_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_string_sequence_storage>()
/*
Return the corresponding partition kind for ExprStringSequence.
*/
{
  return ifc_pk_expr_string_sequence;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprSubobjectValue nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_expr_subobject_value &universal);

extern an_ifc_expr_index get_ifc_value(
                                 const an_ifc_expr_subobject_value &universal);

extern a_boolean validate(const an_ifc_expr_subobject_value &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_subobject_value &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_expr_subobject_value &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_subobject_value_storage* get<an_ifc_expr_subobject_value_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_expr_subobject_value_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_subobject_value_storage>()
/*
Return the corresponding partition kind for ExprSubobjectValue.
*/
{
  return ifc_pk_expr_class_subobject_value;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprSumTypeValue nodes.
*/

extern a_boolean has_ifc_discriminant(
                                  const an_ifc_expr_sum_type_value &universal);

extern an_ifc_active_member get_ifc_discriminant(
                                  const an_ifc_expr_sum_type_value &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_sum_type_value &universal);

extern an_ifc_source_location get_ifc_locus(
                                  const an_ifc_expr_sum_type_value &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_sum_type_value &universal);

extern an_ifc_type_index get_ifc_type(
                                  const an_ifc_expr_sum_type_value &universal);

extern a_boolean has_ifc_value(const an_ifc_expr_sum_type_value &universal);

extern an_ifc_expr_index get_ifc_value(
                                  const an_ifc_expr_sum_type_value &universal);

extern a_boolean has_ifc_variant(const an_ifc_expr_sum_type_value &universal);

extern an_ifc_decl_index get_ifc_variant(
                                  const an_ifc_expr_sum_type_value &universal);

extern a_boolean validate(const an_ifc_expr_sum_type_value &universal,
                          const an_ifc_validation_trace    *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_sum_type_value &universal,
                    unsigned                         indent);

extern void db_node(const an_ifc_expr_sum_type_value &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_sum_type_value_storage* get<an_ifc_expr_sum_type_value_storage>(
                              an_ifc_module                      *mod,
                              an_ifc_expr_sum_type_value_storage *storage,
                              a_boolean                          fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_sum_type_value_storage>()
/*
Return the corresponding partition kind for ExprSumTypeValue.
*/
{
  return ifc_pk_expr_sum_type_value;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprSyntaxTree nodes.
*/

extern a_boolean has_ifc_syntax(const an_ifc_expr_syntax_tree &universal);

extern an_ifc_syntax_index get_ifc_syntax(
                                     const an_ifc_expr_syntax_tree &universal);

extern a_boolean validate(const an_ifc_expr_syntax_tree &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_syntax_tree &universal, unsigned indent);

extern void db_node(const an_ifc_expr_syntax_tree &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_syntax_tree_storage* get<an_ifc_expr_syntax_tree_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_expr_syntax_tree_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_syntax_tree_storage>()
/*
Return the corresponding partition kind for ExprSyntaxTree.
*/
{
  return ifc_pk_expr_syntax_tree;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprTemplateId nodes.
*/

extern a_boolean has_ifc_arguments(const an_ifc_expr_template_id &universal);

extern an_ifc_expr_index get_ifc_arguments(
                                     const an_ifc_expr_template_id &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_template_id &universal);

extern an_ifc_source_location get_ifc_locus(
                                     const an_ifc_expr_template_id &universal);

extern a_boolean has_ifc_primary(const an_ifc_expr_template_id &universal);

extern an_ifc_expr_index get_ifc_primary(
                                     const an_ifc_expr_template_id &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_template_id &universal);

extern an_ifc_type_index get_ifc_type(
                                     const an_ifc_expr_template_id &universal);

extern a_boolean validate(const an_ifc_expr_template_id &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_template_id &universal, unsigned indent);

extern void db_node(const an_ifc_expr_template_id &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_template_id_storage* get<an_ifc_expr_template_id_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_expr_template_id_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_template_id_storage>()
/*
Return the corresponding partition kind for ExprTemplateId.
*/
{
  return ifc_pk_expr_template_id;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprTemplateReference nodes.
*/

extern a_boolean has_ifc_arguments(
                              const an_ifc_expr_template_reference &universal);

extern an_ifc_expr_index get_ifc_arguments(
                              const an_ifc_expr_template_reference &universal);

extern a_boolean has_ifc_locus(
                              const an_ifc_expr_template_reference &universal);

extern an_ifc_source_location get_ifc_locus(
                              const an_ifc_expr_template_reference &universal);

extern a_boolean has_ifc_member_locus(
                              const an_ifc_expr_template_reference &universal);

extern an_ifc_source_location get_ifc_member_locus(
                              const an_ifc_expr_template_reference &universal);

extern a_boolean has_ifc_member_name(
                              const an_ifc_expr_template_reference &universal);

extern an_ifc_name_index get_ifc_member_name(
                              const an_ifc_expr_template_reference &universal);

extern a_boolean has_ifc_scope(
                              const an_ifc_expr_template_reference &universal);

extern an_ifc_type_index get_ifc_scope(
                              const an_ifc_expr_template_reference &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_template_reference &universal);

extern an_ifc_type_index get_ifc_type(
                              const an_ifc_expr_template_reference &universal);

extern a_boolean validate(const an_ifc_expr_template_reference &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_template_reference &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_expr_template_reference &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_template_reference_storage*
get<an_ifc_expr_template_reference_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_expr_template_reference_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_template_reference_storage>()
/*
Return the corresponding partition kind for ExprTemplateReference.
*/
{
  return ifc_pk_expr_template_reference;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprTemporary nodes.
*/

extern a_boolean has_ifc_id(const an_ifc_expr_temporary &universal);

extern an_ifc_unique_id get_ifc_id(const an_ifc_expr_temporary &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_temporary &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_expr_temporary &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_temporary &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_temporary &universal);

extern a_boolean validate(const an_ifc_expr_temporary   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_temporary &universal, unsigned indent);

extern void db_node(const an_ifc_expr_temporary &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_temporary_storage* get<an_ifc_expr_temporary_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_expr_temporary_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_temporary_storage>()
/*
Return the corresponding partition kind for ExprTemporary.
*/
{
  return ifc_pk_expr_temporary;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprThis nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_this &universal);

extern an_ifc_source_location get_ifc_locus(const an_ifc_expr_this &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_this &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_this &universal);

extern a_boolean validate(const an_ifc_expr_this        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_this &universal, unsigned indent);

extern void db_node(const an_ifc_expr_this &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_this_storage* get<an_ifc_expr_this_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_expr_this_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_this_storage>()
/*
Return the corresponding partition kind for ExprThis.
*/
{
  return ifc_pk_expr_this;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprTokens nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_tokens &universal);

extern an_ifc_source_location get_ifc_locus(
                                          const an_ifc_expr_tokens &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_tokens &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_tokens &universal);

extern a_boolean has_ifc_words(const an_ifc_expr_tokens &universal);

extern an_ifc_sentence_index get_ifc_words(
                                          const an_ifc_expr_tokens &universal);

extern a_boolean validate(const an_ifc_expr_tokens      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_tokens &universal, unsigned indent);

extern void db_node(const an_ifc_expr_tokens &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_tokens_storage* get<an_ifc_expr_tokens_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_expr_tokens_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_tokens_storage>()
/*
Return the corresponding partition kind for ExprTokens.
*/
{
  return ifc_pk_expr_tokens;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprTriad nodes.
*/

extern a_boolean has_ifc_argument_0(const an_ifc_expr_triad &universal);

extern an_ifc_expr_index get_ifc_argument_0(
                                           const an_ifc_expr_triad &universal);

extern a_boolean has_ifc_argument_1(const an_ifc_expr_triad &universal);

extern an_ifc_expr_index get_ifc_argument_1(
                                           const an_ifc_expr_triad &universal);

extern a_boolean has_ifc_argument_2(const an_ifc_expr_triad &universal);

extern an_ifc_expr_index get_ifc_argument_2(
                                           const an_ifc_expr_triad &universal);

extern a_boolean has_ifc_assoc(const an_ifc_expr_triad &universal);

extern an_ifc_triadic_operator_sort get_ifc_assoc(
                                           const an_ifc_expr_triad &universal);

extern a_boolean has_ifc_impl(const an_ifc_expr_triad &universal);

extern an_ifc_decl_index get_ifc_impl(const an_ifc_expr_triad &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_triad &universal);

extern an_ifc_source_location get_ifc_locus(
                                           const an_ifc_expr_triad &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_triad &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_triad &universal);

extern a_boolean validate(const an_ifc_expr_triad       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_triad &universal, unsigned indent);

extern void db_node(const an_ifc_expr_triad &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_triad_storage* get<an_ifc_expr_triad_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_expr_triad_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_triad_storage>()
/*
Return the corresponding partition kind for ExprTriad.
*/
{
  return ifc_pk_expr_triad;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprTuple nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_expr_tuple &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                           const an_ifc_expr_tuple &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_tuple &universal);

extern an_ifc_source_location get_ifc_locus(
                                           const an_ifc_expr_tuple &universal);

extern a_boolean has_ifc_start(const an_ifc_expr_tuple &universal);

extern an_ifc_index get_ifc_start(const an_ifc_expr_tuple &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_tuple &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_tuple &universal);

extern a_boolean validate(const an_ifc_expr_tuple       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_tuple &universal, unsigned indent);

extern void db_node(const an_ifc_expr_tuple &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_tuple_storage* get<an_ifc_expr_tuple_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_expr_tuple_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_tuple_storage>()
/*
Return the corresponding partition kind for ExprTuple.
*/
{
  return ifc_pk_expr_tuple;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprType nodes.
*/

extern a_boolean has_ifc_denotation(const an_ifc_expr_type &universal);

extern an_ifc_type_index get_ifc_denotation(const an_ifc_expr_type &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_type &universal);

extern an_ifc_source_location get_ifc_locus(const an_ifc_expr_type &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_type &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_type &universal);

extern a_boolean validate(const an_ifc_expr_type        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_type &universal, unsigned indent);

extern void db_node(const an_ifc_expr_type &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_type_storage* get<an_ifc_expr_type_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_expr_type_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_type_storage>()
/*
Return the corresponding partition kind for ExprType.
*/
{
  return ifc_pk_expr_type;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprTypeTraitIntrinsic nodes.
*/

extern a_boolean has_ifc_arguments(
                            const an_ifc_expr_type_trait_intrinsic &universal);

extern an_ifc_type_index get_ifc_arguments(
                            const an_ifc_expr_type_trait_intrinsic &universal);

extern a_boolean has_ifc_intrinsic(
                            const an_ifc_expr_type_trait_intrinsic &universal);

extern an_ifc_operator_category get_ifc_intrinsic(
                            const an_ifc_expr_type_trait_intrinsic &universal);

extern a_boolean has_ifc_locus(
                            const an_ifc_expr_type_trait_intrinsic &universal);

extern an_ifc_source_location get_ifc_locus(
                            const an_ifc_expr_type_trait_intrinsic &universal);

extern a_boolean has_ifc_type(
                            const an_ifc_expr_type_trait_intrinsic &universal);

extern an_ifc_type_index get_ifc_type(
                            const an_ifc_expr_type_trait_intrinsic &universal);

extern a_boolean validate(const an_ifc_expr_type_trait_intrinsic &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_type_trait_intrinsic &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_expr_type_trait_intrinsic &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_type_trait_intrinsic_storage*
get<an_ifc_expr_type_trait_intrinsic_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_expr_type_trait_intrinsic_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_type_trait_intrinsic_storage>()
/*
Return the corresponding partition kind for ExprTypeTraitIntrinsic.
*/
{
  return ifc_pk_expr_type_trait;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprTypeid nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_typeid &universal);

extern an_ifc_source_location get_ifc_locus(
                                          const an_ifc_expr_typeid &universal);

extern a_boolean has_ifc_operand(const an_ifc_expr_typeid &universal);

extern an_ifc_type_index get_ifc_operand(const an_ifc_expr_typeid &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_typeid &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_typeid &universal);

extern a_boolean validate(const an_ifc_expr_typeid      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_typeid &universal, unsigned indent);

extern void db_node(const an_ifc_expr_typeid &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_typeid_storage* get<an_ifc_expr_typeid_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_expr_typeid_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_typeid_storage>()
/*
Return the corresponding partition kind for ExprTypeid.
*/
{
  return ifc_pk_expr_typeid;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprUnaryFold nodes.
*/

extern a_boolean has_ifc_associativity(
                                      const an_ifc_expr_unary_fold &universal);

extern an_ifc_associativity get_ifc_associativity(
                                      const an_ifc_expr_unary_fold &universal);

extern a_boolean has_ifc_expr(const an_ifc_expr_unary_fold &universal);

extern an_ifc_expr_index get_ifc_expr(const an_ifc_expr_unary_fold &universal);

extern a_boolean has_ifc_locus(const an_ifc_expr_unary_fold &universal);

extern an_ifc_source_location get_ifc_locus(
                                      const an_ifc_expr_unary_fold &universal);

extern a_boolean has_ifc_operation(const an_ifc_expr_unary_fold &universal);

extern an_ifc_dyadic_operator_sort get_ifc_operation(
                                      const an_ifc_expr_unary_fold &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_unary_fold &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_expr_unary_fold &universal);

extern a_boolean validate(const an_ifc_expr_unary_fold  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_unary_fold &universal, unsigned indent);

extern void db_node(const an_ifc_expr_unary_fold &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_unary_fold_storage* get<an_ifc_expr_unary_fold_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_expr_unary_fold_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_unary_fold_storage>()
/*
Return the corresponding partition kind for ExprUnaryFold.
*/
{
  return ifc_pk_expr_unary_fold;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprUnqualifiedId nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_unqualified_id &universal);

extern an_ifc_source_location get_ifc_locus(
                                  const an_ifc_expr_unqualified_id &universal);

extern a_boolean has_ifc_name(const an_ifc_expr_unqualified_id &universal);

extern an_ifc_name_index get_ifc_name(
                                  const an_ifc_expr_unqualified_id &universal);

extern a_boolean has_ifc_resolution(
                                  const an_ifc_expr_unqualified_id &universal);

extern an_ifc_expr_index get_ifc_resolution(
                                  const an_ifc_expr_unqualified_id &universal);

extern a_boolean has_ifc_template_keyword(
                                  const an_ifc_expr_unqualified_id &universal);

extern an_ifc_source_location get_ifc_template_keyword(
                                  const an_ifc_expr_unqualified_id &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_unqualified_id &universal);

extern an_ifc_type_index get_ifc_type(
                                  const an_ifc_expr_unqualified_id &universal);

extern a_boolean validate(const an_ifc_expr_unqualified_id &universal,
                          const an_ifc_validation_trace    *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_unqualified_id &universal,
                    unsigned                         indent);

extern void db_node(const an_ifc_expr_unqualified_id &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_unqualified_id_storage* get<an_ifc_expr_unqualified_id_storage>(
                              an_ifc_module                      *mod,
                              an_ifc_expr_unqualified_id_storage *storage,
                              a_boolean                          fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_unqualified_id_storage>()
/*
Return the corresponding partition kind for ExprUnqualifiedId.
*/
{
  return ifc_pk_expr_unqualified_id;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprUnresolvedId nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_expr_unresolved_id &universal);

extern an_ifc_source_location get_ifc_locus(
                                   const an_ifc_expr_unresolved_id &universal);

extern a_boolean has_ifc_name(const an_ifc_expr_unresolved_id &universal);

extern an_ifc_name_index get_ifc_name(
                                   const an_ifc_expr_unresolved_id &universal);

extern a_boolean has_ifc_type(const an_ifc_expr_unresolved_id &universal);

extern an_ifc_type_index get_ifc_type(
                                   const an_ifc_expr_unresolved_id &universal);

extern a_boolean validate(const an_ifc_expr_unresolved_id &universal,
                          const an_ifc_validation_trace   *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_unresolved_id &universal,
                    unsigned                        indent);

extern void db_node(const an_ifc_expr_unresolved_id &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_unresolved_id_storage* get<an_ifc_expr_unresolved_id_storage>(
                               an_ifc_module                     *mod,
                               an_ifc_expr_unresolved_id_storage *storage,
                               a_boolean                         fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_unresolved_id_storage>()
/*
Return the corresponding partition kind for ExprUnresolvedId.
*/
{
  return ifc_pk_expr_unresolved;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ExprVirtualFunctionConversion nodes.
*/

extern a_boolean has_ifc_function(
                     const an_ifc_expr_virtual_function_conversion &universal);

extern an_ifc_decl_index get_ifc_function(
                     const an_ifc_expr_virtual_function_conversion &universal);

extern a_boolean has_ifc_locus(
                     const an_ifc_expr_virtual_function_conversion &universal);

extern an_ifc_source_location get_ifc_locus(
                     const an_ifc_expr_virtual_function_conversion &universal);

extern a_boolean has_ifc_type(
                     const an_ifc_expr_virtual_function_conversion &universal);

extern an_ifc_type_index get_ifc_type(
                     const an_ifc_expr_virtual_function_conversion &universal);

extern a_boolean validate(
                     const an_ifc_expr_virtual_function_conversion &universal,
                     const an_ifc_validation_trace                 *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_virtual_function_conversion &universal,
                    unsigned                                      indent);

extern void db_node(const an_ifc_expr_virtual_function_conversion &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_virtual_function_conversion_storage*
get<an_ifc_expr_virtual_function_conversion_storage>(
                 an_ifc_module                                   *mod,
                 an_ifc_expr_virtual_function_conversion_storage *storage,
                 a_boolean                                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_virtual_function_conversion_storage>()
/*
Return the corresponding partition kind for ExprVirtualFunctionConversion.
*/
{
  return ifc_pk_expr_virtual_function_conversion;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormCatenate nodes.
*/

extern a_boolean has_ifc_first(const an_ifc_form_catenate &universal);

extern an_ifc_form_index get_ifc_first(const an_ifc_form_catenate &universal);

extern a_boolean has_ifc_locus(const an_ifc_form_catenate &universal);

extern an_ifc_source_location get_ifc_locus(
                                        const an_ifc_form_catenate &universal);

extern a_boolean has_ifc_second(const an_ifc_form_catenate &universal);

extern an_ifc_form_index get_ifc_second(const an_ifc_form_catenate &universal);

extern a_boolean validate(const an_ifc_form_catenate    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_catenate &universal, unsigned indent);

extern void db_node(const an_ifc_form_catenate &universal);
#endif /* DEBUG */

template<>
an_ifc_form_catenate_storage* get<an_ifc_form_catenate_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_form_catenate_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_catenate_storage>()
/*
Return the corresponding partition kind for FormCatenate.
*/
{
  return ifc_pk_pp_catenate;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormCharacter nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_character &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_form_character &universal);

extern a_boolean has_ifc_spelling(const an_ifc_form_character &universal);

extern an_ifc_text_offset get_ifc_spelling(
                                       const an_ifc_form_character &universal);

extern a_boolean validate(const an_ifc_form_character   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_character &universal, unsigned indent);

extern void db_node(const an_ifc_form_character &universal);
#endif /* DEBUG */

template<>
an_ifc_form_character_storage* get<an_ifc_form_character_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_form_character_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_character_storage>()
/*
Return the corresponding partition kind for FormCharacter.
*/
{
  return ifc_pk_pp_char;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormHeader nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_header &universal);

extern an_ifc_source_location get_ifc_locus(
                                          const an_ifc_form_header &universal);

extern a_boolean has_ifc_spelling(const an_ifc_form_header &universal);

extern an_ifc_text_offset get_ifc_spelling(
                                          const an_ifc_form_header &universal);

extern a_boolean validate(const an_ifc_form_header      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_header &universal, unsigned indent);

extern void db_node(const an_ifc_form_header &universal);
#endif /* DEBUG */

template<>
an_ifc_form_header_storage* get<an_ifc_form_header_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_form_header_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_header_storage>()
/*
Return the corresponding partition kind for FormHeader.
*/
{
  return ifc_pk_pp_header;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormIdentifier nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_identifier &universal);

extern an_ifc_source_location get_ifc_locus(
                                      const an_ifc_form_identifier &universal);

extern a_boolean has_ifc_spelling(const an_ifc_form_identifier &universal);

extern an_ifc_text_offset get_ifc_spelling(
                                      const an_ifc_form_identifier &universal);

extern a_boolean validate(const an_ifc_form_identifier  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_identifier &universal, unsigned indent);

extern void db_node(const an_ifc_form_identifier &universal);
#endif /* DEBUG */

template<>
an_ifc_form_identifier_storage* get<an_ifc_form_identifier_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_form_identifier_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_identifier_storage>()
/*
Return the corresponding partition kind for FormIdentifier.
*/
{
  return ifc_pk_pp_ident;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormJunk nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_junk &universal);

extern an_ifc_source_location get_ifc_locus(const an_ifc_form_junk &universal);

extern a_boolean has_ifc_spelling(const an_ifc_form_junk &universal);

extern an_ifc_text_offset get_ifc_spelling(const an_ifc_form_junk &universal);

extern a_boolean validate(const an_ifc_form_junk        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_junk &universal, unsigned indent);

extern void db_node(const an_ifc_form_junk &universal);
#endif /* DEBUG */

template<>
an_ifc_form_junk_storage* get<an_ifc_form_junk_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_form_junk_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_junk_storage>()
/*
Return the corresponding partition kind for FormJunk.
*/
{
  return ifc_pk_pp_junk;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormKeyword nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_keyword &universal);

extern an_ifc_source_location get_ifc_locus(
                                         const an_ifc_form_keyword &universal);

extern a_boolean has_ifc_spelling(const an_ifc_form_keyword &universal);

extern an_ifc_text_offset get_ifc_spelling(
                                         const an_ifc_form_keyword &universal);

extern a_boolean validate(const an_ifc_form_keyword     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_keyword &universal, unsigned indent);

extern void db_node(const an_ifc_form_keyword &universal);
#endif /* DEBUG */

template<>
an_ifc_form_keyword_storage* get<an_ifc_form_keyword_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_form_keyword_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_keyword_storage>()
/*
Return the corresponding partition kind for FormKeyword.
*/
{
  return ifc_pk_pp_key;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormNumber nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_number &universal);

extern an_ifc_source_location get_ifc_locus(
                                          const an_ifc_form_number &universal);

extern a_boolean has_ifc_spelling(const an_ifc_form_number &universal);

extern an_ifc_text_offset get_ifc_spelling(
                                          const an_ifc_form_number &universal);

extern a_boolean validate(const an_ifc_form_number      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_number &universal, unsigned indent);

extern void db_node(const an_ifc_form_number &universal);
#endif /* DEBUG */

template<>
an_ifc_form_number_storage* get<an_ifc_form_number_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_form_number_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_number_storage>()
/*
Return the corresponding partition kind for FormNumber.
*/
{
  return ifc_pk_pp_num;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormOperator nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_operator &universal);

extern an_ifc_source_location get_ifc_locus(
                                        const an_ifc_form_operator &universal);

extern a_boolean has_ifc_op(const an_ifc_form_operator &universal);

extern an_ifc_form_operator_sort get_ifc_op(
                                        const an_ifc_form_operator &universal);

extern a_boolean has_ifc_spelling(const an_ifc_form_operator &universal);

extern an_ifc_text_offset get_ifc_spelling(
                                        const an_ifc_form_operator &universal);

extern a_boolean validate(const an_ifc_form_operator    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_operator &universal, unsigned indent);

extern void db_node(const an_ifc_form_operator &universal);
#endif /* DEBUG */

template<>
an_ifc_form_operator_storage* get<an_ifc_form_operator_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_form_operator_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_operator_storage>()
/*
Return the corresponding partition kind for FormOperator.
*/
{
  return ifc_pk_pp_op;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormParameter nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_parameter &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_form_parameter &universal);

extern a_boolean has_ifc_spelling(const an_ifc_form_parameter &universal);

extern an_ifc_text_offset get_ifc_spelling(
                                       const an_ifc_form_parameter &universal);

extern a_boolean validate(const an_ifc_form_parameter   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_parameter &universal, unsigned indent);

extern void db_node(const an_ifc_form_parameter &universal);
#endif /* DEBUG */

template<>
an_ifc_form_parameter_storage* get<an_ifc_form_parameter_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_form_parameter_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_parameter_storage>()
/*
Return the corresponding partition kind for FormParameter.
*/
{
  return ifc_pk_pp_param;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormParenthesized nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_parenthesized &universal);

extern an_ifc_source_location get_ifc_locus(
                                   const an_ifc_form_parenthesized &universal);

extern a_boolean has_ifc_operand(const an_ifc_form_parenthesized &universal);

extern an_ifc_form_index get_ifc_operand(
                                   const an_ifc_form_parenthesized &universal);

extern a_boolean validate(const an_ifc_form_parenthesized &universal,
                          const an_ifc_validation_trace   *parent);

#if DEBUG
extern void db_node(const an_ifc_form_parenthesized &universal,
                    unsigned                        indent);

extern void db_node(const an_ifc_form_parenthesized &universal);
#endif /* DEBUG */

template<>
an_ifc_form_parenthesized_storage* get<an_ifc_form_parenthesized_storage>(
                               an_ifc_module                     *mod,
                               an_ifc_form_parenthesized_storage *storage,
                               a_boolean                         fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_parenthesized_storage>()
/*
Return the corresponding partition kind for FormParenthesized.
*/
{
  return ifc_pk_pp_paren;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormPragma nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_pragma &universal);

extern an_ifc_source_location get_ifc_locus(
                                          const an_ifc_form_pragma &universal);

extern a_boolean has_ifc_operand(const an_ifc_form_pragma &universal);

extern an_ifc_form_index get_ifc_operand(const an_ifc_form_pragma &universal);

extern a_boolean validate(const an_ifc_form_pragma      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_pragma &universal, unsigned indent);

extern void db_node(const an_ifc_form_pragma &universal);
#endif /* DEBUG */

template<>
an_ifc_form_pragma_storage* get<an_ifc_form_pragma_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_form_pragma_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_pragma_storage>()
/*
Return the corresponding partition kind for FormPragma.
*/
{
  return ifc_pk_pp_pragma;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormSpec nodes.
*/

extern a_boolean has_ifc_arguments(const an_ifc_form_spec &universal);

extern an_ifc_expr_index get_ifc_arguments(const an_ifc_form_spec &universal);

extern a_boolean has_ifc_primary_template(const an_ifc_form_spec &universal);

extern an_ifc_decl_index get_ifc_primary_template(
                                            const an_ifc_form_spec &universal);

extern a_boolean validate(const an_ifc_form_spec        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_spec &universal, unsigned indent);

extern void db_node(const an_ifc_form_spec &universal);
#endif /* DEBUG */

template<>
an_ifc_form_spec_storage* get<an_ifc_form_spec_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_form_spec_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_spec_storage>()
/*
Return the corresponding partition kind for FormSpec.
*/
{
  return ifc_pk_form_spec;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormString nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_string &universal);

extern an_ifc_source_location get_ifc_locus(
                                          const an_ifc_form_string &universal);

extern a_boolean has_ifc_spelling(const an_ifc_form_string &universal);

extern an_ifc_text_offset get_ifc_spelling(
                                          const an_ifc_form_string &universal);

extern a_boolean validate(const an_ifc_form_string      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_string &universal, unsigned indent);

extern void db_node(const an_ifc_form_string &universal);
#endif /* DEBUG */

template<>
an_ifc_form_string_storage* get<an_ifc_form_string_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_form_string_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_string_storage>()
/*
Return the corresponding partition kind for FormString.
*/
{
  return ifc_pk_pp_string;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormStringize nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_stringize &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_form_stringize &universal);

extern a_boolean has_ifc_operand(const an_ifc_form_stringize &universal);

extern an_ifc_form_index get_ifc_operand(
                                       const an_ifc_form_stringize &universal);

extern a_boolean validate(const an_ifc_form_stringize   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_stringize &universal, unsigned indent);

extern void db_node(const an_ifc_form_stringize &universal);
#endif /* DEBUG */

template<>
an_ifc_form_stringize_storage* get<an_ifc_form_stringize_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_form_stringize_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_stringize_storage>()
/*
Return the corresponding partition kind for FormStringize.
*/
{
  return ifc_pk_pp_to_string;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormTuple nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_form_tuple &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                           const an_ifc_form_tuple &universal);

extern a_boolean has_ifc_start(const an_ifc_form_tuple &universal);

extern an_ifc_index get_ifc_start(const an_ifc_form_tuple &universal);

extern a_boolean validate(const an_ifc_form_tuple       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_tuple &universal, unsigned indent);

extern void db_node(const an_ifc_form_tuple &universal);
#endif /* DEBUG */

template<>
an_ifc_form_tuple_storage* get<an_ifc_form_tuple_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_form_tuple_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_tuple_storage>()
/*
Return the corresponding partition kind for FormTuple.
*/
{
  return ifc_pk_pp_tuple;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC FormWhitespace nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_form_whitespace &universal);

extern an_ifc_source_location get_ifc_locus(
                                      const an_ifc_form_whitespace &universal);

extern a_boolean validate(const an_ifc_form_whitespace  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_form_whitespace &universal, unsigned indent);

extern void db_node(const an_ifc_form_whitespace &universal);
#endif /* DEBUG */

template<>
an_ifc_form_whitespace_storage* get<an_ifc_form_whitespace_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_form_whitespace_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_form_whitespace_storage>()
/*
Return the corresponding partition kind for FormWhitespace.
*/
{
  return ifc_pk_pp_space;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC HeapAttr nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_heap_attr &universal);

extern an_ifc_attr_index get_ifc_value(const an_ifc_heap_attr &universal);

extern a_boolean validate(const an_ifc_heap_attr        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_heap_attr &universal, unsigned indent);

extern void db_node(const an_ifc_heap_attr &universal);
#endif /* DEBUG */

template<>
an_ifc_heap_attr_storage* get<an_ifc_heap_attr_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_heap_attr_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_heap_attr_storage>()
/*
Return the corresponding partition kind for HeapAttr.
*/
{
  return ifc_pk_heap_attr;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC HeapChart nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_heap_chart &universal);

extern an_ifc_chart_index get_ifc_value(const an_ifc_heap_chart &universal);

extern a_boolean validate(const an_ifc_heap_chart       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_heap_chart &universal, unsigned indent);

extern void db_node(const an_ifc_heap_chart &universal);
#endif /* DEBUG */

template<>
an_ifc_heap_chart_storage* get<an_ifc_heap_chart_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_heap_chart_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_heap_chart_storage>()
/*
Return the corresponding partition kind for HeapChart.
*/
{
  return ifc_pk_heap_chart;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC HeapDecl nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_heap_decl &universal);

extern an_ifc_decl_index get_ifc_value(const an_ifc_heap_decl &universal);

extern a_boolean validate(const an_ifc_heap_decl        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_heap_decl &universal, unsigned indent);

extern void db_node(const an_ifc_heap_decl &universal);
#endif /* DEBUG */

template<>
an_ifc_heap_decl_storage* get<an_ifc_heap_decl_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_heap_decl_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_heap_decl_storage>()
/*
Return the corresponding partition kind for HeapDecl.
*/
{
  return ifc_pk_heap_decl;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC HeapExpr nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_heap_expr &universal);

extern an_ifc_expr_index get_ifc_value(const an_ifc_heap_expr &universal);

extern a_boolean validate(const an_ifc_heap_expr        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_heap_expr &universal, unsigned indent);

extern void db_node(const an_ifc_heap_expr &universal);
#endif /* DEBUG */

template<>
an_ifc_heap_expr_storage* get<an_ifc_heap_expr_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_heap_expr_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_heap_expr_storage>()
/*
Return the corresponding partition kind for HeapExpr.
*/
{
  return ifc_pk_heap_expr;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC HeapForm nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_heap_form &universal);

extern an_ifc_form_index get_ifc_value(const an_ifc_heap_form &universal);

extern a_boolean validate(const an_ifc_heap_form        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_heap_form &universal, unsigned indent);

extern void db_node(const an_ifc_heap_form &universal);
#endif /* DEBUG */

template<>
an_ifc_heap_form_storage* get<an_ifc_heap_form_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_heap_form_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_heap_form_storage>()
/*
Return the corresponding partition kind for HeapForm.
*/
{
  return ifc_pk_heap_form;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC HeapPPForm nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_heap_pp_form &universal);

extern an_ifc_form_index get_ifc_value(const an_ifc_heap_pp_form &universal);

extern a_boolean validate(const an_ifc_heap_pp_form     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_heap_pp_form &universal, unsigned indent);

extern void db_node(const an_ifc_heap_pp_form &universal);
#endif /* DEBUG */

template<>
an_ifc_heap_pp_form_storage* get<an_ifc_heap_pp_form_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_heap_pp_form_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_heap_pp_form_storage>()
/*
Return the corresponding partition kind for HeapPPForm.
*/
{
  return ifc_pk_heap_pp;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC HeapStmt nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_heap_stmt &universal);

extern an_ifc_stmt_index get_ifc_value(const an_ifc_heap_stmt &universal);

extern a_boolean validate(const an_ifc_heap_stmt        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_heap_stmt &universal, unsigned indent);

extern void db_node(const an_ifc_heap_stmt &universal);
#endif /* DEBUG */

template<>
an_ifc_heap_stmt_storage* get<an_ifc_heap_stmt_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_heap_stmt_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_heap_stmt_storage>()
/*
Return the corresponding partition kind for HeapStmt.
*/
{
  return ifc_pk_heap_stmt;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC HeapSyntax nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_heap_syntax &universal);

extern an_ifc_syntax_index get_ifc_value(const an_ifc_heap_syntax &universal);

extern a_boolean validate(const an_ifc_heap_syntax      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_heap_syntax &universal, unsigned indent);

extern void db_node(const an_ifc_heap_syntax &universal);
#endif /* DEBUG */

template<>
an_ifc_heap_syntax_storage* get<an_ifc_heap_syntax_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_heap_syntax_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_heap_syntax_storage>()
/*
Return the corresponding partition kind for HeapSyntax.
*/
{
  return ifc_pk_heap_syn;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC HeapType nodes.
*/

extern a_boolean has_ifc_value(const an_ifc_heap_type &universal);

extern an_ifc_type_index get_ifc_value(const an_ifc_heap_type &universal);

extern a_boolean validate(const an_ifc_heap_type        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_heap_type &universal, unsigned indent);

extern void db_node(const an_ifc_heap_type &universal);
#endif /* DEBUG */

template<>
an_ifc_heap_type_storage* get<an_ifc_heap_type_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_heap_type_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_heap_type_storage>()
/*
Return the corresponding partition kind for HeapType.
*/
{
  return ifc_pk_heap_type;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC MacroFunctionLike nodes.
*/

extern a_boolean has_ifc_arity_variadic(
                                  const an_ifc_macro_function_like &universal);

extern an_ifc_variadic_arity get_ifc_arity_variadic(
                                  const an_ifc_macro_function_like &universal);

extern a_boolean has_ifc_body(const an_ifc_macro_function_like &universal);

extern an_ifc_form_index get_ifc_body(
                                  const an_ifc_macro_function_like &universal);

extern a_boolean has_ifc_locus(const an_ifc_macro_function_like &universal);

extern an_ifc_source_location get_ifc_locus(
                                  const an_ifc_macro_function_like &universal);

extern a_boolean has_ifc_name(const an_ifc_macro_function_like &universal);

extern an_ifc_text_offset get_ifc_name(
                                  const an_ifc_macro_function_like &universal);

extern a_boolean has_ifc_parameters(
                                  const an_ifc_macro_function_like &universal);

extern an_ifc_form_index get_ifc_parameters(
                                  const an_ifc_macro_function_like &universal);

extern a_boolean validate(const an_ifc_macro_function_like &universal,
                          const an_ifc_validation_trace    *parent);

#if DEBUG
extern void db_node(const an_ifc_macro_function_like &universal,
                    unsigned                         indent);

extern void db_node(const an_ifc_macro_function_like &universal);
#endif /* DEBUG */

template<>
an_ifc_macro_function_like_storage* get<an_ifc_macro_function_like_storage>(
                              an_ifc_module                      *mod,
                              an_ifc_macro_function_like_storage *storage,
                              a_boolean                          fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_macro_function_like_storage>()
/*
Return the corresponding partition kind for MacroFunctionLike.
*/
{
  return ifc_pk_macro_function_like;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC MacroObjectLike nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_macro_object_like &universal);

extern an_ifc_form_index get_ifc_body(
                                    const an_ifc_macro_object_like &universal);

extern a_boolean has_ifc_locus(const an_ifc_macro_object_like &universal);

extern an_ifc_source_location get_ifc_locus(
                                    const an_ifc_macro_object_like &universal);

extern a_boolean has_ifc_name(const an_ifc_macro_object_like &universal);

extern an_ifc_text_offset get_ifc_name(
                                    const an_ifc_macro_object_like &universal);

extern a_boolean validate(const an_ifc_macro_object_like &universal,
                          const an_ifc_validation_trace  *parent);

#if DEBUG
extern void db_node(const an_ifc_macro_object_like &universal,
                    unsigned                       indent);

extern void db_node(const an_ifc_macro_object_like &universal);
#endif /* DEBUG */

template<>
an_ifc_macro_object_like_storage* get<an_ifc_macro_object_like_storage>(
                                an_ifc_module                    *mod,
                                an_ifc_macro_object_like_storage *storage,
                                a_boolean                        fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_macro_object_like_storage>()
/*
Return the corresponding partition kind for MacroObjectLike.
*/
{
  return ifc_pk_macro_object_like;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ModuleExportReference nodes.
*/

extern a_boolean has_ifc_reference(
                              const an_ifc_module_export_reference &universal);

extern an_ifc_module_reference get_ifc_reference(
                              const an_ifc_module_export_reference &universal);

extern a_boolean validate(const an_ifc_module_export_reference &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_module_export_reference &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_module_export_reference &universal);
#endif /* DEBUG */

template<>
an_ifc_module_export_reference_storage*
get<an_ifc_module_export_reference_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_module_export_reference_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_module_export_reference_storage>()
/*
Return the corresponding partition kind for ModuleExportReference.
*/
{
  return ifc_pk_module_exported;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ModuleImportReference nodes.
*/

extern a_boolean has_ifc_reference(
                              const an_ifc_module_import_reference &universal);

extern an_ifc_module_reference get_ifc_reference(
                              const an_ifc_module_import_reference &universal);

extern a_boolean validate(const an_ifc_module_import_reference &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_module_import_reference &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_module_import_reference &universal);
#endif /* DEBUG */

template<>
an_ifc_module_import_reference_storage*
get<an_ifc_module_import_reference_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_module_import_reference_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_module_import_reference_storage>()
/*
Return the corresponding partition kind for ModuleImportReference.
*/
{
  return ifc_pk_module_imported;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC NameConversion nodes.
*/

extern a_boolean has_ifc_encoded(const an_ifc_name_conversion &universal);

extern an_ifc_text_offset get_ifc_encoded(
                                      const an_ifc_name_conversion &universal);

extern a_boolean has_ifc_target(const an_ifc_name_conversion &universal);

extern an_ifc_type_index get_ifc_target(
                                      const an_ifc_name_conversion &universal);

extern a_boolean validate(const an_ifc_name_conversion  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_name_conversion &universal, unsigned indent);

extern void db_node(const an_ifc_name_conversion &universal);
#endif /* DEBUG */

template<>
an_ifc_name_conversion_storage* get<an_ifc_name_conversion_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_name_conversion_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_name_conversion_storage>()
/*
Return the corresponding partition kind for NameConversion.
*/
{
  return ifc_pk_name_conversion;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC NameGuide nodes.
*/

extern a_boolean has_ifc_primary_template(const an_ifc_name_guide &universal);

extern an_ifc_decl_index get_ifc_primary_template(
                                           const an_ifc_name_guide &universal);

extern a_boolean validate(const an_ifc_name_guide       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_name_guide &universal, unsigned indent);

extern void db_node(const an_ifc_name_guide &universal);
#endif /* DEBUG */

template<>
an_ifc_name_guide_storage* get<an_ifc_name_guide_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_name_guide_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_name_guide_storage>()
/*
Return the corresponding partition kind for NameGuide.
*/
{
  return ifc_pk_name_guide;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC NameLiteral nodes.
*/

extern a_boolean has_ifc_encoded(const an_ifc_name_literal &universal);

extern an_ifc_text_offset get_ifc_encoded(
                                         const an_ifc_name_literal &universal);

extern a_boolean validate(const an_ifc_name_literal     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_name_literal &universal, unsigned indent);

extern void db_node(const an_ifc_name_literal &universal);
#endif /* DEBUG */

template<>
an_ifc_name_literal_storage* get<an_ifc_name_literal_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_name_literal_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_name_literal_storage>()
/*
Return the corresponding partition kind for NameLiteral.
*/
{
  return ifc_pk_name_literal;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC NameOperator nodes.
*/

extern a_boolean has_ifc_encoded(const an_ifc_name_operator &universal);

extern an_ifc_text_offset get_ifc_encoded(
                                        const an_ifc_name_operator &universal);

extern a_boolean has_ifc_operator(const an_ifc_name_operator &universal);

extern an_ifc_operator_category get_ifc_operator(
                                        const an_ifc_name_operator &universal);

extern a_boolean validate(const an_ifc_name_operator    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_name_operator &universal, unsigned indent);

extern void db_node(const an_ifc_name_operator &universal);
#endif /* DEBUG */

template<>
an_ifc_name_operator_storage* get<an_ifc_name_operator_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_name_operator_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_name_operator_storage>()
/*
Return the corresponding partition kind for NameOperator.
*/
{
  return ifc_pk_name_operator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC NameSourceFile nodes.
*/

extern a_boolean has_ifc_guard(const an_ifc_name_source_file &universal);

extern an_ifc_text_offset get_ifc_guard(
                                     const an_ifc_name_source_file &universal);

extern a_boolean has_ifc_path(const an_ifc_name_source_file &universal);

extern an_ifc_text_offset get_ifc_path(
                                     const an_ifc_name_source_file &universal);

extern a_boolean validate(const an_ifc_name_source_file &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_name_source_file &universal, unsigned indent);

extern void db_node(const an_ifc_name_source_file &universal);
#endif /* DEBUG */

template<>
an_ifc_name_source_file_storage* get<an_ifc_name_source_file_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_name_source_file_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_name_source_file_storage>()
/*
Return the corresponding partition kind for NameSourceFile.
*/
{
  return ifc_pk_name_source_file;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC NameSpecialization nodes.
*/

extern a_boolean has_ifc_arguments(
                                  const an_ifc_name_specialization &universal);

extern an_ifc_expr_index get_ifc_arguments(
                                  const an_ifc_name_specialization &universal);

extern a_boolean has_ifc_primary(const an_ifc_name_specialization &universal);

extern an_ifc_name_index get_ifc_primary(
                                  const an_ifc_name_specialization &universal);

extern a_boolean validate(const an_ifc_name_specialization &universal,
                          const an_ifc_validation_trace    *parent);

#if DEBUG
extern void db_node(const an_ifc_name_specialization &universal,
                    unsigned                         indent);

extern void db_node(const an_ifc_name_specialization &universal);
#endif /* DEBUG */

template<>
an_ifc_name_specialization_storage* get<an_ifc_name_specialization_storage>(
                              an_ifc_module                      *mod,
                              an_ifc_name_specialization_storage *storage,
                              a_boolean                          fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_name_specialization_storage>()
/*
Return the corresponding partition kind for NameSpecialization.
*/
{
  return ifc_pk_name_specialization;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC NameTemplate nodes.
*/

extern a_boolean has_ifc_name(const an_ifc_name_template &universal);

extern an_ifc_name_index get_ifc_name(const an_ifc_name_template &universal);

extern a_boolean validate(const an_ifc_name_template    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_name_template &universal, unsigned indent);

extern void db_node(const an_ifc_name_template &universal);
#endif /* DEBUG */

template<>
an_ifc_name_template_storage* get<an_ifc_name_template_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_name_template_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_name_template_storage>()
/*
Return the corresponding partition kind for NameTemplate.
*/
{
  return ifc_pk_name_template;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ScopeDescriptor nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_scope_descriptor &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                     const an_ifc_scope_descriptor &universal);

extern a_boolean has_ifc_start(const an_ifc_scope_descriptor &universal);

extern an_ifc_index get_ifc_start(const an_ifc_scope_descriptor &universal);

extern a_boolean validate(const an_ifc_scope_descriptor &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_scope_descriptor &universal, unsigned indent);

extern void db_node(const an_ifc_scope_descriptor &universal);
#endif /* DEBUG */

template<>
an_ifc_scope_descriptor_storage* get<an_ifc_scope_descriptor_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_scope_descriptor_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_scope_descriptor_storage>()
/*
Return the corresponding partition kind for ScopeDescriptor.
*/
{
  return ifc_pk_scope_desc;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC ScopeMember nodes.
*/

extern a_boolean has_ifc_index(const an_ifc_scope_member &universal);

extern an_ifc_decl_index get_ifc_index(const an_ifc_scope_member &universal);

extern a_boolean validate(const an_ifc_scope_member     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_scope_member &universal, unsigned indent);

extern void db_node(const an_ifc_scope_member &universal);
#endif /* DEBUG */

template<>
an_ifc_scope_member_storage* get<an_ifc_scope_member_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_scope_member_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_scope_member_storage>()
/*
Return the corresponding partition kind for ScopeMember.
*/
{
  return ifc_pk_scope_member;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SourceLine nodes.
*/

extern a_boolean has_ifc_file(const an_ifc_source_line &universal);

extern an_ifc_name_index get_ifc_file(const an_ifc_source_line &universal);

extern a_boolean has_ifc_line(const an_ifc_source_line &universal);

extern an_ifc_line_number get_ifc_line(const an_ifc_source_line &universal);

extern a_boolean validate(const an_ifc_source_line      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_source_line &universal, unsigned indent);

extern void db_node(const an_ifc_source_line &universal);
#endif /* DEBUG */

template<>
an_ifc_source_line_storage* get<an_ifc_source_line_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_source_line_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_source_line_storage>()
/*
Return the corresponding partition kind for SourceLine.
*/
{
  return ifc_pk_src_line;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SourceSentence nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_source_sentence &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                      const an_ifc_source_sentence &universal);

extern a_boolean has_ifc_locus(const an_ifc_source_sentence &universal);

extern an_ifc_source_location get_ifc_locus(
                                      const an_ifc_source_sentence &universal);

extern a_boolean has_ifc_start(const an_ifc_source_sentence &universal);

extern an_ifc_index get_ifc_start(const an_ifc_source_sentence &universal);

extern a_boolean validate(const an_ifc_source_sentence  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_source_sentence &universal, unsigned indent);

extern void db_node(const an_ifc_source_sentence &universal);
#endif /* DEBUG */

template<>
an_ifc_source_sentence_storage* get<an_ifc_source_sentence_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_source_sentence_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_source_sentence_storage>()
/*
Return the corresponding partition kind for SourceSentence.
*/
{
  return ifc_pk_src_sentence;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SourceWord nodes.
*/

extern a_boolean has_ifc_category(const an_ifc_source_word &universal);

extern an_ifc_word_category get_ifc_category(
                                          const an_ifc_source_word &universal);

extern a_boolean has_ifc_index(const an_ifc_source_word &universal);

extern an_ifc_index get_ifc_index(const an_ifc_source_word &universal);

extern a_boolean has_ifc_locus(const an_ifc_source_word &universal);

extern an_ifc_source_location get_ifc_locus(
                                          const an_ifc_source_word &universal);

extern a_boolean has_ifc_sort(const an_ifc_source_word &universal);

extern an_ifc_word_sort get_ifc_sort(const an_ifc_source_word &universal);

extern a_boolean has_ifc_value(const an_ifc_source_word &universal);

extern an_ifc_u16 get_ifc_value(const an_ifc_source_word &universal);

extern a_boolean validate(const an_ifc_source_word      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_source_word &universal, unsigned indent);

extern void db_node(const an_ifc_source_word &universal);
#endif /* DEBUG */

template<>
an_ifc_source_word_storage* get<an_ifc_source_word_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_source_word_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_source_word_storage>()
/*
Return the corresponding partition kind for SourceWord.
*/
{
  return ifc_pk_src_word;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtBlock nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_stmt_block &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                           const an_ifc_stmt_block &universal);

extern a_boolean has_ifc_start(const an_ifc_stmt_block &universal);

extern an_ifc_index get_ifc_start(const an_ifc_stmt_block &universal);

extern a_boolean validate(const an_ifc_stmt_block       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_block &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_block &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_block_storage* get<an_ifc_stmt_block_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_stmt_block_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_block_storage>()
/*
Return the corresponding partition kind for StmtBlock.
*/
{
  return ifc_pk_stmt_block;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtBreak nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_stmt_break &universal);

extern an_ifc_source_location get_ifc_locus(
                                           const an_ifc_stmt_break &universal);

extern a_boolean validate(const an_ifc_stmt_break       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_break &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_break &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_break_storage* get<an_ifc_stmt_break_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_stmt_break_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_break_storage>()
/*
Return the corresponding partition kind for StmtBreak.
*/
{
  return ifc_pk_stmt_break;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtCase nodes.
*/

extern a_boolean has_ifc_expr(const an_ifc_stmt_case &universal);

extern an_ifc_expr_index get_ifc_expr(const an_ifc_stmt_case &universal);

extern a_boolean has_ifc_locus(const an_ifc_stmt_case &universal);

extern an_ifc_source_location get_ifc_locus(const an_ifc_stmt_case &universal);

extern a_boolean validate(const an_ifc_stmt_case        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_case &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_case &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_case_storage* get<an_ifc_stmt_case_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_stmt_case_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_case_storage>()
/*
Return the corresponding partition kind for StmtCase.
*/
{
  return ifc_pk_stmt_case;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtContinue nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_stmt_continue &universal);

extern an_ifc_source_location get_ifc_locus(
                                        const an_ifc_stmt_continue &universal);

extern a_boolean validate(const an_ifc_stmt_continue    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_continue &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_continue &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_continue_storage* get<an_ifc_stmt_continue_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_stmt_continue_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_continue_storage>()
/*
Return the corresponding partition kind for StmtContinue.
*/
{
  return ifc_pk_stmt_continue;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtDefault nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_stmt_default &universal);

extern an_ifc_source_location get_ifc_locus(
                                         const an_ifc_stmt_default &universal);

extern a_boolean validate(const an_ifc_stmt_default     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_default &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_default &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_default_storage* get<an_ifc_stmt_default_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_stmt_default_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_default_storage>()
/*
Return the corresponding partition kind for StmtDefault.
*/
{
  return ifc_pk_stmt_default;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtDoWhile nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_stmt_do_while &universal);

extern an_ifc_stmt_index get_ifc_body(const an_ifc_stmt_do_while &universal);

extern a_boolean has_ifc_condition(const an_ifc_stmt_do_while &universal);

extern an_ifc_stmt_index get_ifc_condition(
                                        const an_ifc_stmt_do_while &universal);

extern a_boolean has_ifc_locus(const an_ifc_stmt_do_while &universal);

extern an_ifc_source_location get_ifc_locus(
                                        const an_ifc_stmt_do_while &universal);

extern a_boolean validate(const an_ifc_stmt_do_while    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_do_while &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_do_while &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_do_while_storage* get<an_ifc_stmt_do_while_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_stmt_do_while_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_do_while_storage>()
/*
Return the corresponding partition kind for StmtDoWhile.
*/
{
  return ifc_pk_stmt_do_while;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtEmpty nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_stmt_empty &universal);

extern an_ifc_source_location get_ifc_locus(
                                           const an_ifc_stmt_empty &universal);

extern a_boolean validate(const an_ifc_stmt_empty       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_empty &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_empty &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_empty_storage* get<an_ifc_stmt_empty_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_stmt_empty_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_empty_storage>()
/*
Return the corresponding partition kind for StmtEmpty.
*/
{
  return ifc_pk_stmt_empty;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtExpansion nodes.
*/

extern a_boolean has_ifc_operand(const an_ifc_stmt_expansion &universal);

extern an_ifc_stmt_index get_ifc_operand(
                                       const an_ifc_stmt_expansion &universal);

extern a_boolean validate(const an_ifc_stmt_expansion   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_expansion &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_expansion &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_expansion_storage* get<an_ifc_stmt_expansion_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_stmt_expansion_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_expansion_storage>()
/*
Return the corresponding partition kind for StmtExpansion.
*/
{
  return ifc_pk_stmt_expansion;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtExpression nodes.
*/

extern a_boolean has_ifc_expr(const an_ifc_stmt_expression &universal);

extern an_ifc_expr_index get_ifc_expr(const an_ifc_stmt_expression &universal);

extern a_boolean has_ifc_locus(const an_ifc_stmt_expression &universal);

extern an_ifc_source_location get_ifc_locus(
                                      const an_ifc_stmt_expression &universal);

extern a_boolean validate(const an_ifc_stmt_expression  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_expression &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_expression &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_expression_storage* get<an_ifc_stmt_expression_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_stmt_expression_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_expression_storage>()
/*
Return the corresponding partition kind for StmtExpression.
*/
{
  return ifc_pk_stmt_expression;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtFor nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_stmt_for &universal);

extern an_ifc_stmt_index get_ifc_body(const an_ifc_stmt_for &universal);

extern a_boolean has_ifc_condition(const an_ifc_stmt_for &universal);

extern an_ifc_stmt_index get_ifc_condition(const an_ifc_stmt_for &universal);

extern a_boolean has_ifc_continuation(const an_ifc_stmt_for &universal);

extern an_ifc_stmt_index get_ifc_continuation(
                                             const an_ifc_stmt_for &universal);

extern a_boolean has_ifc_initialization(const an_ifc_stmt_for &universal);

extern an_ifc_stmt_index get_ifc_initialization(
                                             const an_ifc_stmt_for &universal);

extern a_boolean has_ifc_locus(const an_ifc_stmt_for &universal);

extern an_ifc_source_location get_ifc_locus(const an_ifc_stmt_for &universal);

extern a_boolean validate(const an_ifc_stmt_for         &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_for &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_for &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_for_storage* get<an_ifc_stmt_for_storage>(
                                         an_ifc_module           *mod,
                                         an_ifc_stmt_for_storage *storage,
                                         a_boolean               fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_for_storage>()
/*
Return the corresponding partition kind for StmtFor.
*/
{
  return ifc_pk_stmt_for;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtIf nodes.
*/

extern a_boolean has_ifc_alternative(const an_ifc_stmt_if &universal);

extern an_ifc_stmt_index get_ifc_alternative(const an_ifc_stmt_if &universal);

extern a_boolean has_ifc_condition(const an_ifc_stmt_if &universal);

extern an_ifc_stmt_index get_ifc_condition(const an_ifc_stmt_if &universal);

extern a_boolean has_ifc_consequence(const an_ifc_stmt_if &universal);

extern an_ifc_stmt_index get_ifc_consequence(const an_ifc_stmt_if &universal);

extern a_boolean has_ifc_initialization(const an_ifc_stmt_if &universal);

extern an_ifc_stmt_index get_ifc_initialization(
                                              const an_ifc_stmt_if &universal);

extern a_boolean has_ifc_locus(const an_ifc_stmt_if &universal);

extern an_ifc_source_location get_ifc_locus(const an_ifc_stmt_if &universal);

extern a_boolean validate(const an_ifc_stmt_if          &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_if &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_if &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_if_storage* get<an_ifc_stmt_if_storage>(
                                          an_ifc_module          *mod,
                                          an_ifc_stmt_if_storage *storage,
                                          a_boolean              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_if_storage>()
/*
Return the corresponding partition kind for StmtIf.
*/
{
  return ifc_pk_stmt_if;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtReturn nodes.
*/

extern a_boolean has_ifc_expr(const an_ifc_stmt_return &universal);

extern an_ifc_expr_index get_ifc_expr(const an_ifc_stmt_return &universal);

extern a_boolean has_ifc_expression_type(const an_ifc_stmt_return &universal);

extern an_ifc_type_index get_ifc_expression_type(
                                          const an_ifc_stmt_return &universal);

extern a_boolean has_ifc_function_type(const an_ifc_stmt_return &universal);

extern an_ifc_type_index get_ifc_function_type(
                                          const an_ifc_stmt_return &universal);

extern a_boolean has_ifc_locus(const an_ifc_stmt_return &universal);

extern an_ifc_source_location get_ifc_locus(
                                          const an_ifc_stmt_return &universal);

extern a_boolean validate(const an_ifc_stmt_return      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_return &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_return &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_return_storage* get<an_ifc_stmt_return_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_stmt_return_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_return_storage>()
/*
Return the corresponding partition kind for StmtReturn.
*/
{
  return ifc_pk_stmt_return;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtSwitch nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_stmt_switch &universal);

extern an_ifc_stmt_index get_ifc_body(const an_ifc_stmt_switch &universal);

extern a_boolean has_ifc_condition(const an_ifc_stmt_switch &universal);

extern an_ifc_expr_index get_ifc_condition(
                                          const an_ifc_stmt_switch &universal);

extern a_boolean has_ifc_initialization(const an_ifc_stmt_switch &universal);

extern an_ifc_stmt_index get_ifc_initialization(
                                          const an_ifc_stmt_switch &universal);

extern a_boolean has_ifc_locus(const an_ifc_stmt_switch &universal);

extern an_ifc_source_location get_ifc_locus(
                                          const an_ifc_stmt_switch &universal);

extern a_boolean validate(const an_ifc_stmt_switch      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_switch &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_switch &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_switch_storage* get<an_ifc_stmt_switch_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_stmt_switch_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_switch_storage>()
/*
Return the corresponding partition kind for StmtSwitch.
*/
{
  return ifc_pk_stmt_switch;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtVariableDecl nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_stmt_variable_decl &universal);

extern an_ifc_decl_index get_ifc_decl(
                                   const an_ifc_stmt_variable_decl &universal);

extern a_boolean has_ifc_locus(const an_ifc_stmt_variable_decl &universal);

extern an_ifc_source_location get_ifc_locus(
                                   const an_ifc_stmt_variable_decl &universal);

extern a_boolean validate(const an_ifc_stmt_variable_decl &universal,
                          const an_ifc_validation_trace   *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_variable_decl &universal,
                    unsigned                        indent);

extern void db_node(const an_ifc_stmt_variable_decl &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_variable_decl_storage* get<an_ifc_stmt_variable_decl_storage>(
                               an_ifc_module                     *mod,
                               an_ifc_stmt_variable_decl_storage *storage,
                               a_boolean                         fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_variable_decl_storage>()
/*
Return the corresponding partition kind for StmtVariableDecl.
*/
{
  return ifc_pk_stmt_variable;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC StmtWhile nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_stmt_while &universal);

extern an_ifc_stmt_index get_ifc_body(const an_ifc_stmt_while &universal);

extern a_boolean has_ifc_condition(const an_ifc_stmt_while &universal);

extern an_ifc_stmt_index get_ifc_condition(const an_ifc_stmt_while &universal);

extern a_boolean has_ifc_locus(const an_ifc_stmt_while &universal);

extern an_ifc_source_location get_ifc_locus(
                                           const an_ifc_stmt_while &universal);

extern a_boolean validate(const an_ifc_stmt_while       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_while &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_while &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_while_storage* get<an_ifc_stmt_while_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_stmt_while_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_stmt_while_storage>()
/*
Return the corresponding partition kind for StmtWhile.
*/
{
  return ifc_pk_stmt_while;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxAccessSpecifier nodes.
*/

extern a_boolean has_ifc_access(
                              const an_ifc_syntax_access_specifier &universal);

extern an_ifc_keyword_syntax get_ifc_access(
                              const an_ifc_syntax_access_specifier &universal);

extern a_boolean has_ifc_comma(
                              const an_ifc_syntax_access_specifier &universal);

extern an_ifc_source_location get_ifc_comma(
                              const an_ifc_syntax_access_specifier &universal);

extern a_boolean has_ifc_designator(
                              const an_ifc_syntax_access_specifier &universal);

extern an_ifc_expr_index get_ifc_designator(
                              const an_ifc_syntax_access_specifier &universal);

extern a_boolean has_ifc_locus(
                              const an_ifc_syntax_access_specifier &universal);

extern an_ifc_source_location get_ifc_locus(
                              const an_ifc_syntax_access_specifier &universal);

extern a_boolean has_ifc_virtual_kw(
                              const an_ifc_syntax_access_specifier &universal);

extern an_ifc_source_location get_ifc_virtual_kw(
                              const an_ifc_syntax_access_specifier &universal);

extern a_boolean has_ifc_virtual_kw2(
                              const an_ifc_syntax_access_specifier &universal);

extern an_ifc_source_location get_ifc_virtual_kw2(
                              const an_ifc_syntax_access_specifier &universal);

extern a_boolean validate(const an_ifc_syntax_access_specifier &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_access_specifier &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_syntax_access_specifier &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_access_specifier_storage*
get<an_ifc_syntax_access_specifier_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_syntax_access_specifier_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_access_specifier_storage>()
/*
Return the corresponding partition kind for SyntaxAccessSpecifier.
*/
{
  return ifc_pk_syntax_access_specifier;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxAliasDeclaration nodes.
*/

extern a_boolean has_ifc_aliasee(
                             const an_ifc_syntax_alias_declaration &universal);

extern an_ifc_syntax_index get_ifc_aliasee(
                             const an_ifc_syntax_alias_declaration &universal);

extern a_boolean has_ifc_equal(
                             const an_ifc_syntax_alias_declaration &universal);

extern an_ifc_source_location get_ifc_equal(
                             const an_ifc_syntax_alias_declaration &universal);

extern a_boolean has_ifc_locus(
                             const an_ifc_syntax_alias_declaration &universal);

extern an_ifc_source_location get_ifc_locus(
                             const an_ifc_syntax_alias_declaration &universal);

extern a_boolean has_ifc_name(
                             const an_ifc_syntax_alias_declaration &universal);

extern an_ifc_expr_index get_ifc_name(
                             const an_ifc_syntax_alias_declaration &universal);

extern a_boolean has_ifc_semicolon(
                             const an_ifc_syntax_alias_declaration &universal);

extern an_ifc_source_location get_ifc_semicolon(
                             const an_ifc_syntax_alias_declaration &universal);

extern a_boolean validate(const an_ifc_syntax_alias_declaration &universal,
                          const an_ifc_validation_trace         *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_alias_declaration &universal,
                    unsigned                              indent);

extern void db_node(const an_ifc_syntax_alias_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_alias_declaration_storage*
get<an_ifc_syntax_alias_declaration_storage>(
                         an_ifc_module                           *mod,
                         an_ifc_syntax_alias_declaration_storage *storage,
                         a_boolean                               fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_alias_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxAliasDeclaration.
*/
{
  return ifc_pk_syntax_alias_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxAlignas nodes.
*/

extern a_boolean has_ifc_left_paren(const an_ifc_syntax_alignas &universal);

extern an_ifc_source_location get_ifc_left_paren(
                                       const an_ifc_syntax_alignas &universal);

extern a_boolean has_ifc_locus(const an_ifc_syntax_alignas &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_syntax_alignas &universal);

extern a_boolean has_ifc_operand(const an_ifc_syntax_alignas &universal);

extern an_ifc_syntax_index get_ifc_operand(
                                       const an_ifc_syntax_alignas &universal);

extern a_boolean has_ifc_right_paren(const an_ifc_syntax_alignas &universal);

extern an_ifc_source_location get_ifc_right_paren(
                                       const an_ifc_syntax_alignas &universal);

extern a_boolean validate(const an_ifc_syntax_alignas   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_alignas &universal, unsigned indent);

extern void db_node(const an_ifc_syntax_alignas &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_alignas_storage* get<an_ifc_syntax_alignas_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_syntax_alignas_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_alignas_storage>()
/*
Return the corresponding partition kind for SyntaxAlignas.
*/
{
  return ifc_pk_syntax_alignas;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxArrayDeclarator nodes.
*/

extern a_boolean has_ifc_bound(
                              const an_ifc_syntax_array_declarator &universal);

extern an_ifc_expr_index get_ifc_bound(
                              const an_ifc_syntax_array_declarator &universal);

extern a_boolean has_ifc_left_bracket(
                              const an_ifc_syntax_array_declarator &universal);

extern an_ifc_source_location get_ifc_left_bracket(
                              const an_ifc_syntax_array_declarator &universal);

extern a_boolean has_ifc_right_bracket(
                              const an_ifc_syntax_array_declarator &universal);

extern an_ifc_source_location get_ifc_right_bracket(
                              const an_ifc_syntax_array_declarator &universal);

extern a_boolean validate(const an_ifc_syntax_array_declarator &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_array_declarator &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_syntax_array_declarator &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_array_declarator_storage*
get<an_ifc_syntax_array_declarator_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_syntax_array_declarator_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_array_declarator_storage>()
/*
Return the corresponding partition kind for SyntaxArrayDeclarator.
*/
{
  return ifc_pk_syntax_array_declarator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxArrayIndex nodes.
*/

extern a_boolean has_ifc_array(const an_ifc_syntax_array_index &universal);

extern an_ifc_expr_index get_ifc_array(
                                   const an_ifc_syntax_array_index &universal);

extern a_boolean has_ifc_index(const an_ifc_syntax_array_index &universal);

extern an_ifc_expr_index get_ifc_index(
                                   const an_ifc_syntax_array_index &universal);

extern a_boolean has_ifc_left_bracket(
                                   const an_ifc_syntax_array_index &universal);

extern an_ifc_source_location get_ifc_left_bracket(
                                   const an_ifc_syntax_array_index &universal);

extern a_boolean has_ifc_right_bracket(
                                   const an_ifc_syntax_array_index &universal);

extern an_ifc_source_location get_ifc_right_bracket(
                                   const an_ifc_syntax_array_index &universal);

extern a_boolean validate(const an_ifc_syntax_array_index &universal,
                          const an_ifc_validation_trace   *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_array_index &universal,
                    unsigned                        indent);

extern void db_node(const an_ifc_syntax_array_index &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_array_index_storage* get<an_ifc_syntax_array_index_storage>(
                               an_ifc_module                     *mod,
                               an_ifc_syntax_array_index_storage *storage,
                               a_boolean                         fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_array_index_storage>()
/*
Return the corresponding partition kind for SyntaxArrayIndex.
*/
{
  return ifc_pk_syntax_array_index;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxArrayOrFunctionDeclarator nodes.
*/

extern a_boolean has_ifc_declarator(
                  const an_ifc_syntax_array_or_function_declarator &universal);

extern an_ifc_syntax_index get_ifc_declarator(
                  const an_ifc_syntax_array_or_function_declarator &universal);

extern a_boolean has_ifc_next(
                  const an_ifc_syntax_array_or_function_declarator &universal);

extern an_ifc_syntax_index get_ifc_next(
                  const an_ifc_syntax_array_or_function_declarator &universal);

extern a_boolean validate(
                  const an_ifc_syntax_array_or_function_declarator &universal,
                  const an_ifc_validation_trace                    *parent);

#if DEBUG
extern void db_node(
                  const an_ifc_syntax_array_or_function_declarator &universal,
                  unsigned                                         indent);

extern void db_node(
                  const an_ifc_syntax_array_or_function_declarator &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_array_or_function_declarator_storage*
get<an_ifc_syntax_array_or_function_declarator_storage>(
              an_ifc_module                                      *mod,
              an_ifc_syntax_array_or_function_declarator_storage *storage,
              a_boolean                                          fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_array_or_function_declarator_storage>()
/*
Return the corresponding partition kind for SyntaxArrayOrFunctionDeclarator.
*/
{
  return ifc_pk_syntax_array_or_function_declarator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxAsmStatement nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_syntax_asm_statement &universal);

extern an_ifc_source_location get_ifc_locus(
                                 const an_ifc_syntax_asm_statement &universal);

extern a_boolean has_ifc_tokens(const an_ifc_syntax_asm_statement &universal);

extern an_ifc_sentence_index get_ifc_tokens(
                                 const an_ifc_syntax_asm_statement &universal);

extern a_boolean validate(const an_ifc_syntax_asm_statement &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_asm_statement &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_syntax_asm_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_asm_statement_storage* get<an_ifc_syntax_asm_statement_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_syntax_asm_statement_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_asm_statement_storage>()
/*
Return the corresponding partition kind for SyntaxAsmStatement.
*/
{
  return ifc_pk_syntax_asm_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxAttribute nodes.
*/

extern a_boolean has_ifc_argument_clause(
                                     const an_ifc_syntax_attribute &universal);

extern an_ifc_syntax_index get_ifc_argument_clause(
                                     const an_ifc_syntax_attribute &universal);

extern a_boolean has_ifc_colons(const an_ifc_syntax_attribute &universal);

extern an_ifc_source_location get_ifc_colons(
                                     const an_ifc_syntax_attribute &universal);

extern a_boolean has_ifc_comma(const an_ifc_syntax_attribute &universal);

extern an_ifc_source_location get_ifc_comma(
                                     const an_ifc_syntax_attribute &universal);

extern a_boolean has_ifc_expander(const an_ifc_syntax_attribute &universal);

extern an_ifc_source_location get_ifc_expander(
                                     const an_ifc_syntax_attribute &universal);

extern a_boolean has_ifc_name(const an_ifc_syntax_attribute &universal);

extern an_ifc_expr_index get_ifc_name(
                                     const an_ifc_syntax_attribute &universal);

extern a_boolean has_ifc_scope(const an_ifc_syntax_attribute &universal);

extern an_ifc_expr_index get_ifc_scope(
                                     const an_ifc_syntax_attribute &universal);

extern a_boolean validate(const an_ifc_syntax_attribute &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_attribute &universal, unsigned indent);

extern void db_node(const an_ifc_syntax_attribute &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_attribute_storage* get<an_ifc_syntax_attribute_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_syntax_attribute_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attribute_storage>()
/*
Return the corresponding partition kind for SyntaxAttribute.
*/
{
  return ifc_pk_syntax_attribute;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxAttributeArgumentClause nodes.
*/

extern a_boolean has_ifc_left_paren(
                     const an_ifc_syntax_attribute_argument_clause &universal);

extern an_ifc_source_location get_ifc_left_paren(
                     const an_ifc_syntax_attribute_argument_clause &universal);

extern a_boolean has_ifc_right_paren(
                     const an_ifc_syntax_attribute_argument_clause &universal);

extern an_ifc_source_location get_ifc_right_paren(
                     const an_ifc_syntax_attribute_argument_clause &universal);

extern a_boolean has_ifc_tokens(
                     const an_ifc_syntax_attribute_argument_clause &universal);

extern an_ifc_sentence_index get_ifc_tokens(
                     const an_ifc_syntax_attribute_argument_clause &universal);

extern a_boolean validate(
                     const an_ifc_syntax_attribute_argument_clause &universal,
                     const an_ifc_validation_trace                 *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_attribute_argument_clause &universal,
                    unsigned                                      indent);

extern void db_node(const an_ifc_syntax_attribute_argument_clause &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_attribute_argument_clause_storage*
get<an_ifc_syntax_attribute_argument_clause_storage>(
                 an_ifc_module                                   *mod,
                 an_ifc_syntax_attribute_argument_clause_storage *storage,
                 a_boolean                                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attribute_argument_clause_storage>()
/*
Return the corresponding partition kind for SyntaxAttributeArgumentClause.
*/
{
  return ifc_pk_syntax_attribute_argument_clause;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxAttributeSpecifier nodes.
*/

extern a_boolean has_ifc_attributes(
                           const an_ifc_syntax_attribute_specifier &universal);

extern an_ifc_syntax_index get_ifc_attributes(
                           const an_ifc_syntax_attribute_specifier &universal);

extern a_boolean has_ifc_left_paren_1(
                           const an_ifc_syntax_attribute_specifier &universal);

extern an_ifc_source_location get_ifc_left_paren_1(
                           const an_ifc_syntax_attribute_specifier &universal);

extern a_boolean has_ifc_left_paren_2(
                           const an_ifc_syntax_attribute_specifier &universal);

extern an_ifc_source_location get_ifc_left_paren_2(
                           const an_ifc_syntax_attribute_specifier &universal);

extern a_boolean has_ifc_prefix(
                           const an_ifc_syntax_attribute_specifier &universal);

extern an_ifc_syntax_index get_ifc_prefix(
                           const an_ifc_syntax_attribute_specifier &universal);

extern a_boolean has_ifc_right_paren_1(
                           const an_ifc_syntax_attribute_specifier &universal);

extern an_ifc_source_location get_ifc_right_paren_1(
                           const an_ifc_syntax_attribute_specifier &universal);

extern a_boolean has_ifc_right_paren_2(
                           const an_ifc_syntax_attribute_specifier &universal);

extern an_ifc_source_location get_ifc_right_paren_2(
                           const an_ifc_syntax_attribute_specifier &universal);

extern a_boolean validate(const an_ifc_syntax_attribute_specifier &universal,
                          const an_ifc_validation_trace           *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_attribute_specifier &universal,
                    unsigned                                indent);

extern void db_node(const an_ifc_syntax_attribute_specifier &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_attribute_specifier_storage*
get<an_ifc_syntax_attribute_specifier_storage>(
                       an_ifc_module                             *mod,
                       an_ifc_syntax_attribute_specifier_storage *storage,
                       a_boolean                                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attribute_specifier_storage>()
/*
Return the corresponding partition kind for SyntaxAttributeSpecifier.
*/
{
  return ifc_pk_syntax_attribute_specifier;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxAttributeSpecifierSeq nodes.
*/

extern a_boolean has_ifc_attributes(
                       const an_ifc_syntax_attribute_specifier_seq &universal);

extern an_ifc_syntax_index get_ifc_attributes(
                       const an_ifc_syntax_attribute_specifier_seq &universal);

extern a_boolean validate(
                       const an_ifc_syntax_attribute_specifier_seq &universal,
                       const an_ifc_validation_trace               *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_attribute_specifier_seq &universal,
                    unsigned                                    indent);

extern void db_node(const an_ifc_syntax_attribute_specifier_seq &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_attribute_specifier_seq_storage*
get<an_ifc_syntax_attribute_specifier_seq_storage>(
                   an_ifc_module                                 *mod,
                   an_ifc_syntax_attribute_specifier_seq_storage *storage,
                   a_boolean                                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attribute_specifier_seq_storage>()
/*
Return the corresponding partition kind for SyntaxAttributeSpecifierSeq.
*/
{
  return ifc_pk_syntax_attribute_specifier_seq;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxAttributeUsingPrefix nodes.
*/

extern a_boolean has_ifc_locus(
                        const an_ifc_syntax_attribute_using_prefix &universal);

extern an_ifc_source_location get_ifc_locus(
                        const an_ifc_syntax_attribute_using_prefix &universal);

extern a_boolean has_ifc_scope(
                        const an_ifc_syntax_attribute_using_prefix &universal);

extern an_ifc_source_location get_ifc_scope(
                        const an_ifc_syntax_attribute_using_prefix &universal);

extern a_boolean validate(
                        const an_ifc_syntax_attribute_using_prefix &universal,
                        const an_ifc_validation_trace              *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_attribute_using_prefix &universal,
                    unsigned                                   indent);

extern void db_node(const an_ifc_syntax_attribute_using_prefix &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_attribute_using_prefix_storage*
get<an_ifc_syntax_attribute_using_prefix_storage>(
                    an_ifc_module                                *mod,
                    an_ifc_syntax_attribute_using_prefix_storage *storage,
                    a_boolean                                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attribute_using_prefix_storage>()
/*
Return the corresponding partition kind for SyntaxAttributeUsingPrefix.
*/
{
  return ifc_pk_syntax_attribute_using_prefix;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxAttributedDeclaration nodes.
*/

extern a_boolean has_ifc_attributes(
                        const an_ifc_syntax_attributed_declaration &universal);

extern an_ifc_syntax_index get_ifc_attributes(
                        const an_ifc_syntax_attributed_declaration &universal);

extern a_boolean has_ifc_decl(
                        const an_ifc_syntax_attributed_declaration &universal);

extern an_ifc_syntax_index get_ifc_decl(
                        const an_ifc_syntax_attributed_declaration &universal);

extern a_boolean has_ifc_locus(
                        const an_ifc_syntax_attributed_declaration &universal);

extern an_ifc_source_location get_ifc_locus(
                        const an_ifc_syntax_attributed_declaration &universal);

extern a_boolean validate(
                        const an_ifc_syntax_attributed_declaration &universal,
                        const an_ifc_validation_trace              *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_attributed_declaration &universal,
                    unsigned                                   indent);

extern void db_node(const an_ifc_syntax_attributed_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_attributed_declaration_storage*
get<an_ifc_syntax_attributed_declaration_storage>(
                    an_ifc_module                                *mod,
                    an_ifc_syntax_attributed_declaration_storage *storage,
                    a_boolean                                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attributed_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxAttributedDeclaration.
*/
{
  return ifc_pk_syntax_attributed_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxAttributedStatement nodes.
*/

extern a_boolean has_ifc_attributes(
                          const an_ifc_syntax_attributed_statement &universal);

extern an_ifc_syntax_index get_ifc_attributes(
                          const an_ifc_syntax_attributed_statement &universal);

extern a_boolean has_ifc_pragma(
                          const an_ifc_syntax_attributed_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                          const an_ifc_syntax_attributed_statement &universal);

extern a_boolean has_ifc_stmt(
                          const an_ifc_syntax_attributed_statement &universal);

extern an_ifc_syntax_index get_ifc_stmt(
                          const an_ifc_syntax_attributed_statement &universal);

extern a_boolean validate(const an_ifc_syntax_attributed_statement &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_attributed_statement &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_syntax_attributed_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_attributed_statement_storage*
get<an_ifc_syntax_attributed_statement_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_syntax_attributed_statement_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attributed_statement_storage>()
/*
Return the corresponding partition kind for SyntaxAttributedStatement.
*/
{
  return ifc_pk_syntax_attributed_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxBaseSpecifier nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_syntax_base_specifier &universal);

extern an_ifc_keyword_syntax get_ifc_access(
                                const an_ifc_syntax_base_specifier &universal);

extern a_boolean has_ifc_colon(const an_ifc_syntax_base_specifier &universal);

extern an_ifc_source_location get_ifc_colon(
                                const an_ifc_syntax_base_specifier &universal);

extern a_boolean validate(const an_ifc_syntax_base_specifier &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_base_specifier &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_syntax_base_specifier &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_base_specifier_storage*
get<an_ifc_syntax_base_specifier_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_syntax_base_specifier_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_base_specifier_storage>()
/*
Return the corresponding partition kind for SyntaxBaseSpecifier.
*/
{
  return ifc_pk_syntax_base_specifier;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxBaseSpecifierList nodes.
*/

extern a_boolean has_ifc_base_specifiers(
                           const an_ifc_syntax_base_specifier_list &universal);

extern an_ifc_syntax_index get_ifc_base_specifiers(
                           const an_ifc_syntax_base_specifier_list &universal);

extern a_boolean has_ifc_colon(
                           const an_ifc_syntax_base_specifier_list &universal);

extern an_ifc_source_location get_ifc_colon(
                           const an_ifc_syntax_base_specifier_list &universal);

extern a_boolean validate(const an_ifc_syntax_base_specifier_list &universal,
                          const an_ifc_validation_trace           *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_base_specifier_list &universal,
                    unsigned                                indent);

extern void db_node(const an_ifc_syntax_base_specifier_list &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_base_specifier_list_storage*
get<an_ifc_syntax_base_specifier_list_storage>(
                       an_ifc_module                             *mod,
                       an_ifc_syntax_base_specifier_list_storage *storage,
                       a_boolean                                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_base_specifier_list_storage>()
/*
Return the corresponding partition kind for SyntaxBaseSpecifierList.
*/
{
  return ifc_pk_syntax_base_specifier_list;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxBinaryFoldExpression nodes.
*/

extern a_boolean has_ifc_direction(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern an_ifc_fold_direction_sort get_ifc_direction(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern a_boolean has_ifc_dyad(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern an_ifc_dyadic_operator_sort get_ifc_dyad(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern a_boolean has_ifc_ellipsis(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern an_ifc_source_location get_ifc_ellipsis(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern a_boolean has_ifc_glyph_loci_1(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern an_ifc_source_location get_ifc_glyph_loci_1(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern a_boolean has_ifc_glyph_loci_2(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern an_ifc_source_location get_ifc_glyph_loci_2(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern a_boolean has_ifc_locus(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern an_ifc_source_location get_ifc_locus(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern a_boolean has_ifc_operand_1(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern an_ifc_expr_index get_ifc_operand_1(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern a_boolean has_ifc_operand_2(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern an_ifc_expr_index get_ifc_operand_2(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern a_boolean has_ifc_right_paren(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern an_ifc_source_location get_ifc_right_paren(
                        const an_ifc_syntax_binary_fold_expression &universal);

extern a_boolean validate(
                        const an_ifc_syntax_binary_fold_expression &universal,
                        const an_ifc_validation_trace              *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_binary_fold_expression &universal,
                    unsigned                                   indent);

extern void db_node(const an_ifc_syntax_binary_fold_expression &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_binary_fold_expression_storage*
get<an_ifc_syntax_binary_fold_expression_storage>(
                    an_ifc_module                                *mod,
                    an_ifc_syntax_binary_fold_expression_storage *storage,
                    a_boolean                                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_binary_fold_expression_storage>()
/*
Return the corresponding partition kind for SyntaxBinaryFoldExpression.
*/
{
  return ifc_pk_syntax_binary_fold_expression;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxBreakStatement nodes.
*/

extern a_boolean has_ifc_break(const an_ifc_syntax_break_statement &universal);

extern an_ifc_source_location get_ifc_break(
                               const an_ifc_syntax_break_statement &universal);

extern a_boolean has_ifc_semicolon(
                               const an_ifc_syntax_break_statement &universal);

extern an_ifc_source_location get_ifc_semicolon(
                               const an_ifc_syntax_break_statement &universal);

extern a_boolean validate(const an_ifc_syntax_break_statement &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_break_statement &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_syntax_break_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_break_statement_storage*
get<an_ifc_syntax_break_statement_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_syntax_break_statement_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_break_statement_storage>()
/*
Return the corresponding partition kind for SyntaxBreakStatement.
*/
{
  return ifc_pk_syntax_break_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxCaptureDefault nodes.
*/

extern a_boolean has_ifc_by_ref(
                               const an_ifc_syntax_capture_default &universal);

extern an_ifc_bool get_ifc_by_ref(
                               const an_ifc_syntax_capture_default &universal);

extern a_boolean has_ifc_comma(const an_ifc_syntax_capture_default &universal);

extern an_ifc_source_location get_ifc_comma(
                               const an_ifc_syntax_capture_default &universal);

extern a_boolean has_ifc_locus(const an_ifc_syntax_capture_default &universal);

extern an_ifc_source_location get_ifc_locus(
                               const an_ifc_syntax_capture_default &universal);

extern a_boolean validate(const an_ifc_syntax_capture_default &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_capture_default &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_syntax_capture_default &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_capture_default_storage*
get<an_ifc_syntax_capture_default_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_syntax_capture_default_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_capture_default_storage>()
/*
Return the corresponding partition kind for SyntaxCaptureDefault.
*/
{
  return ifc_pk_syntax_capture_default;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxClassSpecifier nodes.
*/

extern a_boolean has_ifc_bases(const an_ifc_syntax_class_specifier &universal);

extern an_ifc_syntax_index get_ifc_bases(
                               const an_ifc_syntax_class_specifier &universal);

extern a_boolean has_ifc_class_key(
                               const an_ifc_syntax_class_specifier &universal);

extern an_ifc_keyword_syntax get_ifc_class_key(
                               const an_ifc_syntax_class_specifier &universal);

extern a_boolean has_ifc_left_paren(
                               const an_ifc_syntax_class_specifier &universal);

extern an_ifc_syntax_index get_ifc_left_paren(
                               const an_ifc_syntax_class_specifier &universal);

extern a_boolean has_ifc_members(
                               const an_ifc_syntax_class_specifier &universal);

extern an_ifc_syntax_index get_ifc_members(
                               const an_ifc_syntax_class_specifier &universal);

extern a_boolean has_ifc_name(const an_ifc_syntax_class_specifier &universal);

extern an_ifc_expr_index get_ifc_name(
                               const an_ifc_syntax_class_specifier &universal);

extern a_boolean has_ifc_right_paren(
                               const an_ifc_syntax_class_specifier &universal);

extern an_ifc_syntax_index get_ifc_right_paren(
                               const an_ifc_syntax_class_specifier &universal);

extern a_boolean validate(const an_ifc_syntax_class_specifier &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_class_specifier &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_syntax_class_specifier &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_class_specifier_storage*
get<an_ifc_syntax_class_specifier_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_syntax_class_specifier_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_class_specifier_storage>()
/*
Return the corresponding partition kind for SyntaxClassSpecifier.
*/
{
  return ifc_pk_syntax_class_specifier;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxCompoundRequirement nodes.
*/

extern a_boolean has_ifc_condition(
                          const an_ifc_syntax_compound_requirement &universal);

extern an_ifc_expr_index get_ifc_condition(
                          const an_ifc_syntax_compound_requirement &universal);

extern a_boolean has_ifc_constraint(
                          const an_ifc_syntax_compound_requirement &universal);

extern an_ifc_expr_index get_ifc_constraint(
                          const an_ifc_syntax_compound_requirement &universal);

extern a_boolean has_ifc_locus(
                          const an_ifc_syntax_compound_requirement &universal);

extern an_ifc_source_location get_ifc_locus(
                          const an_ifc_syntax_compound_requirement &universal);

extern a_boolean has_ifc_noexcept_loc(
                          const an_ifc_syntax_compound_requirement &universal);

extern an_ifc_source_location get_ifc_noexcept_loc(
                          const an_ifc_syntax_compound_requirement &universal);

extern a_boolean has_ifc_right_curly(
                          const an_ifc_syntax_compound_requirement &universal);

extern an_ifc_source_location get_ifc_right_curly(
                          const an_ifc_syntax_compound_requirement &universal);

extern a_boolean validate(const an_ifc_syntax_compound_requirement &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_compound_requirement &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_syntax_compound_requirement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_compound_requirement_storage*
get<an_ifc_syntax_compound_requirement_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_syntax_compound_requirement_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_compound_requirement_storage>()
/*
Return the corresponding partition kind for SyntaxCompoundRequirement.
*/
{
  return ifc_pk_syntax_compound_requirement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxCompoundStatement nodes.
*/

extern a_boolean has_ifc_left_curly(
                            const an_ifc_syntax_compound_statement &universal);

extern an_ifc_source_location get_ifc_left_curly(
                            const an_ifc_syntax_compound_statement &universal);

extern a_boolean has_ifc_pragam(
                            const an_ifc_syntax_compound_statement &universal);

extern an_ifc_sentence_index get_ifc_pragam(
                            const an_ifc_syntax_compound_statement &universal);

extern a_boolean has_ifc_right_curly(
                            const an_ifc_syntax_compound_statement &universal);

extern an_ifc_source_location get_ifc_right_curly(
                            const an_ifc_syntax_compound_statement &universal);

extern a_boolean has_ifc_stmts(
                            const an_ifc_syntax_compound_statement &universal);

extern an_ifc_syntax_index get_ifc_stmts(
                            const an_ifc_syntax_compound_statement &universal);

extern a_boolean validate(const an_ifc_syntax_compound_statement &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_compound_statement &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_compound_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_compound_statement_storage*
get<an_ifc_syntax_compound_statement_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_compound_statement_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_compound_statement_storage>()
/*
Return the corresponding partition kind for SyntaxCompoundStatement.
*/
{
  return ifc_pk_syntax_compound_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxConceptDefinition nodes.
*/

extern a_boolean has_ifc_concept_keyword(
                            const an_ifc_syntax_concept_definition &universal);

extern an_ifc_source_location get_ifc_concept_keyword(
                            const an_ifc_syntax_concept_definition &universal);

extern a_boolean has_ifc_equal(
                            const an_ifc_syntax_concept_definition &universal);

extern an_ifc_source_location get_ifc_equal(
                            const an_ifc_syntax_concept_definition &universal);

extern a_boolean has_ifc_initializer(
                            const an_ifc_syntax_concept_definition &universal);

extern an_ifc_expr_index get_ifc_initializer(
                            const an_ifc_syntax_concept_definition &universal);

extern a_boolean has_ifc_locus(
                            const an_ifc_syntax_concept_definition &universal);

extern an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_concept_definition &universal);

extern a_boolean has_ifc_name(
                            const an_ifc_syntax_concept_definition &universal);

extern an_ifc_text_offset get_ifc_name(
                            const an_ifc_syntax_concept_definition &universal);

extern a_boolean has_ifc_parameters(
                            const an_ifc_syntax_concept_definition &universal);

extern an_ifc_syntax_index get_ifc_parameters(
                            const an_ifc_syntax_concept_definition &universal);

extern a_boolean has_ifc_semicolon(
                            const an_ifc_syntax_concept_definition &universal);

extern an_ifc_source_location get_ifc_semicolon(
                            const an_ifc_syntax_concept_definition &universal);

extern a_boolean validate(const an_ifc_syntax_concept_definition &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_concept_definition &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_concept_definition &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_concept_definition_storage*
get<an_ifc_syntax_concept_definition_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_concept_definition_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_concept_definition_storage>()
/*
Return the corresponding partition kind for SyntaxConceptDefinition.
*/
{
  return ifc_pk_syntax_concept_definition;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxConditionDeclaration nodes.
*/

extern a_boolean has_ifc_decl_specifier(
                         const an_ifc_syntax_condition_declaration &universal);

extern an_ifc_syntax_index get_ifc_decl_specifier(
                         const an_ifc_syntax_condition_declaration &universal);

extern a_boolean has_ifc_initializaerion(
                         const an_ifc_syntax_condition_declaration &universal);

extern an_ifc_syntax_index get_ifc_initializaerion(
                         const an_ifc_syntax_condition_declaration &universal);

extern a_boolean has_ifc_locus(
                         const an_ifc_syntax_condition_declaration &universal);

extern an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_condition_declaration &universal);

extern a_boolean validate(
                         const an_ifc_syntax_condition_declaration &universal,
                         const an_ifc_validation_trace             *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_condition_declaration &universal,
                    unsigned                                  indent);

extern void db_node(const an_ifc_syntax_condition_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_condition_declaration_storage*
get<an_ifc_syntax_condition_declaration_storage>(
                     an_ifc_module                               *mod,
                     an_ifc_syntax_condition_declaration_storage *storage,
                     a_boolean                                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_condition_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxConditionDeclaration.
*/
{
  return ifc_pk_syntax_condition_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxContinueStatement nodes.
*/

extern a_boolean has_ifc_continue(
                            const an_ifc_syntax_continue_statement &universal);

extern an_ifc_source_location get_ifc_continue(
                            const an_ifc_syntax_continue_statement &universal);

extern a_boolean has_ifc_semicolon(
                            const an_ifc_syntax_continue_statement &universal);

extern an_ifc_source_location get_ifc_semicolon(
                            const an_ifc_syntax_continue_statement &universal);

extern a_boolean validate(const an_ifc_syntax_continue_statement &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_continue_statement &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_continue_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_continue_statement_storage*
get<an_ifc_syntax_continue_statement_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_continue_statement_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_continue_statement_storage>()
/*
Return the corresponding partition kind for SyntaxContinueStatement.
*/
{
  return ifc_pk_syntax_continue_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxCtorInitializer nodes.
*/

extern a_boolean has_ifc_colon(
                              const an_ifc_syntax_ctor_initializer &universal);

extern an_ifc_source_location get_ifc_colon(
                              const an_ifc_syntax_ctor_initializer &universal);

extern a_boolean has_ifc_initializers(
                              const an_ifc_syntax_ctor_initializer &universal);

extern an_ifc_syntax_index get_ifc_initializers(
                              const an_ifc_syntax_ctor_initializer &universal);

extern a_boolean validate(const an_ifc_syntax_ctor_initializer &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_ctor_initializer &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_syntax_ctor_initializer &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_ctor_initializer_storage*
get<an_ifc_syntax_ctor_initializer_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_syntax_ctor_initializer_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_ctor_initializer_storage>()
/*
Return the corresponding partition kind for SyntaxCtorInitializer.
*/
{
  return ifc_pk_syntax_ctor_initializer;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxDeclSpecifierSeq nodes.
*/

extern a_boolean has_ifc_declspec(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern an_ifc_sentence_index get_ifc_declspec(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern a_boolean has_ifc_explicit_kw(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern an_ifc_syntax_index get_ifc_explicit_kw(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern a_boolean has_ifc_locus(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern a_boolean has_ifc_qualifiers(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern an_ifc_qualifier_bitfield get_ifc_qualifiers(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern a_boolean has_ifc_storage_class(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern an_ifc_storage_class get_ifc_storage_class(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern a_boolean has_ifc_type(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern an_ifc_type_index get_ifc_type(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern a_boolean has_ifc_type_name(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern an_ifc_syntax_index get_ifc_type_name(
                            const an_ifc_syntax_decl_specifier_seq &universal);

extern a_boolean validate(const an_ifc_syntax_decl_specifier_seq &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_decl_specifier_seq &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_decl_specifier_seq &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_decl_specifier_seq_storage*
get<an_ifc_syntax_decl_specifier_seq_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_decl_specifier_seq_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_decl_specifier_seq_storage>()
/*
Return the corresponding partition kind for SyntaxDeclSpecifierSeq.
*/
{
  return ifc_pk_syntax_decl_specifier_seq;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxDeclarationStatement nodes.
*/

extern a_boolean has_ifc_decl(
                         const an_ifc_syntax_declaration_statement &universal);

extern an_ifc_syntax_index get_ifc_decl(
                         const an_ifc_syntax_declaration_statement &universal);

extern a_boolean has_ifc_pragma(
                         const an_ifc_syntax_declaration_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                         const an_ifc_syntax_declaration_statement &universal);

extern a_boolean validate(
                         const an_ifc_syntax_declaration_statement &universal,
                         const an_ifc_validation_trace             *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_declaration_statement &universal,
                    unsigned                                  indent);

extern void db_node(const an_ifc_syntax_declaration_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_declaration_statement_storage*
get<an_ifc_syntax_declaration_statement_storage>(
                     an_ifc_module                               *mod,
                     an_ifc_syntax_declaration_statement_storage *storage,
                     a_boolean                                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_declaration_statement_storage>()
/*
Return the corresponding partition kind for SyntaxDeclarationStatement.
*/
{
  return ifc_pk_syntax_declaration_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxDeclarator nodes.
*/

extern a_boolean has_ifc_array_or_function(
                                    const an_ifc_syntax_declarator &universal);

extern an_ifc_syntax_index get_ifc_array_or_function(
                                    const an_ifc_syntax_declarator &universal);

extern a_boolean has_ifc_callable(const an_ifc_syntax_declarator &universal);

extern an_ifc_bool get_ifc_callable(const an_ifc_syntax_declarator &universal);

extern a_boolean has_ifc_convention(const an_ifc_syntax_declarator &universal);

extern an_ifc_calling_convention_sort get_ifc_convention(
                                    const an_ifc_syntax_declarator &universal);

extern a_boolean has_ifc_ellipsis(const an_ifc_syntax_declarator &universal);

extern an_ifc_source_location get_ifc_ellipsis(
                                    const an_ifc_syntax_declarator &universal);

extern a_boolean has_ifc_locus(const an_ifc_syntax_declarator &universal);

extern an_ifc_source_location get_ifc_locus(
                                    const an_ifc_syntax_declarator &universal);

extern a_boolean has_ifc_name(const an_ifc_syntax_declarator &universal);

extern an_ifc_expr_index get_ifc_name(
                                    const an_ifc_syntax_declarator &universal);

extern a_boolean has_ifc_parenthesized(
                                    const an_ifc_syntax_declarator &universal);

extern an_ifc_syntax_index get_ifc_parenthesized(
                                    const an_ifc_syntax_declarator &universal);

extern a_boolean has_ifc_pointer(const an_ifc_syntax_declarator &universal);

extern an_ifc_syntax_index get_ifc_pointer(
                                    const an_ifc_syntax_declarator &universal);

extern a_boolean has_ifc_qualifiers(const an_ifc_syntax_declarator &universal);

extern an_ifc_qualifier_bitfield get_ifc_qualifiers(
                                    const an_ifc_syntax_declarator &universal);

extern a_boolean has_ifc_trailing_target(
                                    const an_ifc_syntax_declarator &universal);

extern an_ifc_syntax_index get_ifc_trailing_target(
                                    const an_ifc_syntax_declarator &universal);

extern a_boolean has_ifc_virtual_specifiers(
                                    const an_ifc_syntax_declarator &universal);

extern an_ifc_syntax_index get_ifc_virtual_specifiers(
                                    const an_ifc_syntax_declarator &universal);

extern a_boolean validate(const an_ifc_syntax_declarator &universal,
                          const an_ifc_validation_trace  *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_declarator &universal,
                    unsigned                       indent);

extern void db_node(const an_ifc_syntax_declarator &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_declarator_storage* get<an_ifc_syntax_declarator_storage>(
                                an_ifc_module                    *mod,
                                an_ifc_syntax_declarator_storage *storage,
                                a_boolean                        fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_declarator_storage>()
/*
Return the corresponding partition kind for SyntaxDeclarator.
*/
{
  return ifc_pk_syntax_declarator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxDecltypeSpecifier nodes.
*/

extern a_boolean has_ifc_decltype_keyword(
                            const an_ifc_syntax_decltype_specifier &universal);

extern an_ifc_source_location get_ifc_decltype_keyword(
                            const an_ifc_syntax_decltype_specifier &universal);

extern a_boolean has_ifc_expr(
                            const an_ifc_syntax_decltype_specifier &universal);

extern an_ifc_expr_index get_ifc_expr(
                            const an_ifc_syntax_decltype_specifier &universal);

extern a_boolean has_ifc_left_paren(
                            const an_ifc_syntax_decltype_specifier &universal);

extern an_ifc_source_location get_ifc_left_paren(
                            const an_ifc_syntax_decltype_specifier &universal);

extern a_boolean has_ifc_right_paren(
                            const an_ifc_syntax_decltype_specifier &universal);

extern an_ifc_source_location get_ifc_right_paren(
                            const an_ifc_syntax_decltype_specifier &universal);

extern a_boolean validate(const an_ifc_syntax_decltype_specifier &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_decltype_specifier &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_decltype_specifier &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_decltype_specifier_storage*
get<an_ifc_syntax_decltype_specifier_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_decltype_specifier_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_decltype_specifier_storage>()
/*
Return the corresponding partition kind for SyntaxDecltypeSpecifier.
*/
{
  return ifc_pk_syntax_decltype_specifier;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxDoWhileStatement nodes.
*/

extern a_boolean has_ifc_body(
                            const an_ifc_syntax_do_while_statement &universal);

extern an_ifc_syntax_index get_ifc_body(
                            const an_ifc_syntax_do_while_statement &universal);

extern a_boolean has_ifc_condition(
                            const an_ifc_syntax_do_while_statement &universal);

extern an_ifc_expr_index get_ifc_condition(
                            const an_ifc_syntax_do_while_statement &universal);

extern a_boolean has_ifc_do(const an_ifc_syntax_do_while_statement &universal);

extern an_ifc_source_location get_ifc_do(
                            const an_ifc_syntax_do_while_statement &universal);

extern a_boolean has_ifc_pragma(
                            const an_ifc_syntax_do_while_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                            const an_ifc_syntax_do_while_statement &universal);

extern a_boolean has_ifc_semicolon(
                            const an_ifc_syntax_do_while_statement &universal);

extern an_ifc_source_location get_ifc_semicolon(
                            const an_ifc_syntax_do_while_statement &universal);

extern a_boolean has_ifc_while(
                            const an_ifc_syntax_do_while_statement &universal);

extern an_ifc_source_location get_ifc_while(
                            const an_ifc_syntax_do_while_statement &universal);

extern a_boolean validate(const an_ifc_syntax_do_while_statement &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_do_while_statement &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_do_while_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_do_while_statement_storage*
get<an_ifc_syntax_do_while_statement_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_do_while_statement_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_do_while_statement_storage>()
/*
Return the corresponding partition kind for SyntaxDoWhileStatement.
*/
{
  return ifc_pk_syntax_do_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxDynamicExceptionSpec nodes.
*/

extern a_boolean has_ifc_expander(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

extern an_ifc_source_location get_ifc_expander(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

extern a_boolean has_ifc_left_paren(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

extern an_ifc_source_location get_ifc_left_paren(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

extern a_boolean has_ifc_right_paren(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

extern an_ifc_source_location get_ifc_right_paren(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

extern a_boolean has_ifc_throw(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

extern an_ifc_source_location get_ifc_throw(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

extern a_boolean has_ifc_type_list(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

extern an_ifc_syntax_index get_ifc_type_list(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

extern a_boolean validate(
                        const an_ifc_syntax_dynamic_exception_spec &universal,
                        const an_ifc_validation_trace              *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_dynamic_exception_spec &universal,
                    unsigned                                   indent);

extern void db_node(const an_ifc_syntax_dynamic_exception_spec &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_dynamic_exception_spec_storage*
get<an_ifc_syntax_dynamic_exception_spec_storage>(
                    an_ifc_module                                *mod,
                    an_ifc_syntax_dynamic_exception_spec_storage *storage,
                    a_boolean                                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_dynamic_exception_spec_storage>()
/*
Return the corresponding partition kind for SyntaxDynamicExceptionSpec.
*/
{
  return ifc_pk_syntax_dynamic_exception_spec;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxEmptyStatement nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_syntax_empty_statement &universal);

extern an_ifc_source_location get_ifc_locus(
                               const an_ifc_syntax_empty_statement &universal);

extern a_boolean validate(const an_ifc_syntax_empty_statement &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_empty_statement &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_syntax_empty_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_empty_statement_storage*
get<an_ifc_syntax_empty_statement_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_syntax_empty_statement_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_empty_statement_storage>()
/*
Return the corresponding partition kind for SyntaxEmptyStatement.
*/
{
  return ifc_pk_syntax_empty_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxEnumSpecifier nodes.
*/

extern a_boolean has_ifc_base(const an_ifc_syntax_enum_specifier &universal);

extern an_ifc_syntax_index get_ifc_base(
                                const an_ifc_syntax_enum_specifier &universal);

extern a_boolean has_ifc_class_key(
                                const an_ifc_syntax_enum_specifier &universal);

extern an_ifc_keyword_syntax get_ifc_class_key(
                                const an_ifc_syntax_enum_specifier &universal);

extern a_boolean has_ifc_colon(const an_ifc_syntax_enum_specifier &universal);

extern an_ifc_source_location get_ifc_colon(
                                const an_ifc_syntax_enum_specifier &universal);

extern a_boolean has_ifc_enumerators(
                                const an_ifc_syntax_enum_specifier &universal);

extern an_ifc_syntax_index get_ifc_enumerators(
                                const an_ifc_syntax_enum_specifier &universal);

extern a_boolean has_ifc_left_brace(
                                const an_ifc_syntax_enum_specifier &universal);

extern an_ifc_source_location get_ifc_left_brace(
                                const an_ifc_syntax_enum_specifier &universal);

extern a_boolean has_ifc_locus(const an_ifc_syntax_enum_specifier &universal);

extern an_ifc_source_location get_ifc_locus(
                                const an_ifc_syntax_enum_specifier &universal);

extern a_boolean has_ifc_name(const an_ifc_syntax_enum_specifier &universal);

extern an_ifc_expr_index get_ifc_name(
                                const an_ifc_syntax_enum_specifier &universal);

extern a_boolean has_ifc_right_brace(
                                const an_ifc_syntax_enum_specifier &universal);

extern an_ifc_source_location get_ifc_right_brace(
                                const an_ifc_syntax_enum_specifier &universal);

extern a_boolean validate(const an_ifc_syntax_enum_specifier &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_enum_specifier &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_syntax_enum_specifier &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_enum_specifier_storage*
get<an_ifc_syntax_enum_specifier_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_syntax_enum_specifier_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_enum_specifier_storage>()
/*
Return the corresponding partition kind for SyntaxEnumSpecifier.
*/
{
  return ifc_pk_syntax_enum_specifier;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxEnumeratorDefinition nodes.
*/

extern a_boolean has_ifc_comma(
                         const an_ifc_syntax_enumerator_definition &universal);

extern an_ifc_source_location get_ifc_comma(
                         const an_ifc_syntax_enumerator_definition &universal);

extern a_boolean has_ifc_equal(
                         const an_ifc_syntax_enumerator_definition &universal);

extern an_ifc_source_location get_ifc_equal(
                         const an_ifc_syntax_enumerator_definition &universal);

extern a_boolean has_ifc_initializer(
                         const an_ifc_syntax_enumerator_definition &universal);

extern an_ifc_expr_index get_ifc_initializer(
                         const an_ifc_syntax_enumerator_definition &universal);

extern a_boolean has_ifc_locus(
                         const an_ifc_syntax_enumerator_definition &universal);

extern an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_enumerator_definition &universal);

extern a_boolean has_ifc_name(
                         const an_ifc_syntax_enumerator_definition &universal);

extern an_ifc_text_offset get_ifc_name(
                         const an_ifc_syntax_enumerator_definition &universal);

extern a_boolean validate(
                         const an_ifc_syntax_enumerator_definition &universal,
                         const an_ifc_validation_trace             *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_enumerator_definition &universal,
                    unsigned                                  indent);

extern void db_node(const an_ifc_syntax_enumerator_definition &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_enumerator_definition_storage*
get<an_ifc_syntax_enumerator_definition_storage>(
                     an_ifc_module                               *mod,
                     an_ifc_syntax_enumerator_definition_storage *storage,
                     a_boolean                                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_enumerator_definition_storage>()
/*
Return the corresponding partition kind for SyntaxEnumeratorDefinition.
*/
{
  return ifc_pk_syntax_enumerator_definition;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxExceptionDeclaration nodes.
*/

extern a_boolean has_ifc_declarator(
                         const an_ifc_syntax_exception_declaration &universal);

extern an_ifc_syntax_index get_ifc_declarator(
                         const an_ifc_syntax_exception_declaration &universal);

extern a_boolean has_ifc_ellipsis(
                         const an_ifc_syntax_exception_declaration &universal);

extern an_ifc_source_location get_ifc_ellipsis(
                         const an_ifc_syntax_exception_declaration &universal);

extern a_boolean has_ifc_locus(
                         const an_ifc_syntax_exception_declaration &universal);

extern an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_exception_declaration &universal);

extern a_boolean has_ifc_type_specifiers(
                         const an_ifc_syntax_exception_declaration &universal);

extern an_ifc_syntax_index get_ifc_type_specifiers(
                         const an_ifc_syntax_exception_declaration &universal);

extern a_boolean validate(
                         const an_ifc_syntax_exception_declaration &universal,
                         const an_ifc_validation_trace             *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_exception_declaration &universal,
                    unsigned                                  indent);

extern void db_node(const an_ifc_syntax_exception_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_exception_declaration_storage*
get<an_ifc_syntax_exception_declaration_storage>(
                     an_ifc_module                               *mod,
                     an_ifc_syntax_exception_declaration_storage *storage,
                     a_boolean                                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_exception_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxExceptionDeclaration.
*/
{
  return ifc_pk_syntax_exception_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxExplicitSpecifier nodes.
*/

extern a_boolean has_ifc_condition(
                            const an_ifc_syntax_explicit_specifier &universal);

extern an_ifc_expr_index get_ifc_condition(
                            const an_ifc_syntax_explicit_specifier &universal);

extern a_boolean has_ifc_left_paren(
                            const an_ifc_syntax_explicit_specifier &universal);

extern an_ifc_source_location get_ifc_left_paren(
                            const an_ifc_syntax_explicit_specifier &universal);

extern a_boolean has_ifc_locus(
                            const an_ifc_syntax_explicit_specifier &universal);

extern an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_explicit_specifier &universal);

extern a_boolean has_ifc_right_paren(
                            const an_ifc_syntax_explicit_specifier &universal);

extern an_ifc_source_location get_ifc_right_paren(
                            const an_ifc_syntax_explicit_specifier &universal);

extern a_boolean validate(const an_ifc_syntax_explicit_specifier &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_explicit_specifier &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_explicit_specifier &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_explicit_specifier_storage*
get<an_ifc_syntax_explicit_specifier_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_explicit_specifier_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_explicit_specifier_storage>()
/*
Return the corresponding partition kind for SyntaxExplicitSpecifier.
*/
{
  return ifc_pk_syntax_explicit_specifier;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxExpression nodes.
*/

extern a_boolean has_ifc_expression(const an_ifc_syntax_expression &universal);

extern an_ifc_expr_index get_ifc_expression(
                                    const an_ifc_syntax_expression &universal);

extern a_boolean validate(const an_ifc_syntax_expression &universal,
                          const an_ifc_validation_trace  *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_expression &universal,
                    unsigned                       indent);

extern void db_node(const an_ifc_syntax_expression &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_expression_storage* get<an_ifc_syntax_expression_storage>(
                                an_ifc_module                    *mod,
                                an_ifc_syntax_expression_storage *storage,
                                a_boolean                        fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_expression_storage>()
/*
Return the corresponding partition kind for SyntaxExpression.
*/
{
  return ifc_pk_syntax_expression;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxExpressionStatement nodes.
*/

extern a_boolean has_ifc_expr(
                          const an_ifc_syntax_expression_statement &universal);

extern an_ifc_expr_index get_ifc_expr(
                          const an_ifc_syntax_expression_statement &universal);

extern a_boolean has_ifc_pragma(
                          const an_ifc_syntax_expression_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                          const an_ifc_syntax_expression_statement &universal);

extern a_boolean has_ifc_semicolon(
                          const an_ifc_syntax_expression_statement &universal);

extern an_ifc_source_location get_ifc_semicolon(
                          const an_ifc_syntax_expression_statement &universal);

extern a_boolean validate(const an_ifc_syntax_expression_statement &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_expression_statement &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_syntax_expression_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_expression_statement_storage*
get<an_ifc_syntax_expression_statement_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_syntax_expression_statement_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_expression_statement_storage>()
/*
Return the corresponding partition kind for SyntaxExpressionStatement.
*/
{
  return ifc_pk_syntax_expression_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxForRangeDeclaration nodes.
*/

extern a_boolean has_ifc_declarator(
                         const an_ifc_syntax_for_range_declaration &universal);

extern an_ifc_syntax_index get_ifc_declarator(
                         const an_ifc_syntax_for_range_declaration &universal);

extern a_boolean has_ifc_specifiers(
                         const an_ifc_syntax_for_range_declaration &universal);

extern an_ifc_syntax_index get_ifc_specifiers(
                         const an_ifc_syntax_for_range_declaration &universal);

extern a_boolean validate(
                         const an_ifc_syntax_for_range_declaration &universal,
                         const an_ifc_validation_trace             *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_for_range_declaration &universal,
                    unsigned                                  indent);

extern void db_node(const an_ifc_syntax_for_range_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_for_range_declaration_storage*
get<an_ifc_syntax_for_range_declaration_storage>(
                     an_ifc_module                               *mod,
                     an_ifc_syntax_for_range_declaration_storage *storage,
                     a_boolean                                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_for_range_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxForRangeDeclaration.
*/
{
  return ifc_pk_syntax_for_range_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxForStatement nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_syntax_for_statement &universal);

extern an_ifc_syntax_index get_ifc_body(
                                 const an_ifc_syntax_for_statement &universal);

extern a_boolean has_ifc_condition(
                                 const an_ifc_syntax_for_statement &universal);

extern an_ifc_expr_index get_ifc_condition(
                                 const an_ifc_syntax_for_statement &universal);

extern a_boolean has_ifc_continuation(
                                 const an_ifc_syntax_for_statement &universal);

extern an_ifc_expr_index get_ifc_continuation(
                                 const an_ifc_syntax_for_statement &universal);

extern a_boolean has_ifc_for(const an_ifc_syntax_for_statement &universal);

extern an_ifc_source_location get_ifc_for(
                                 const an_ifc_syntax_for_statement &universal);

extern a_boolean has_ifc_initialization(
                                 const an_ifc_syntax_for_statement &universal);

extern an_ifc_syntax_index get_ifc_initialization(
                                 const an_ifc_syntax_for_statement &universal);

extern a_boolean has_ifc_left_paren(
                                 const an_ifc_syntax_for_statement &universal);

extern an_ifc_source_location get_ifc_left_paren(
                                 const an_ifc_syntax_for_statement &universal);

extern a_boolean has_ifc_pragma(const an_ifc_syntax_for_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                                 const an_ifc_syntax_for_statement &universal);

extern a_boolean has_ifc_right_paren(
                                 const an_ifc_syntax_for_statement &universal);

extern an_ifc_source_location get_ifc_right_paren(
                                 const an_ifc_syntax_for_statement &universal);

extern a_boolean has_ifc_semicolon(
                                 const an_ifc_syntax_for_statement &universal);

extern an_ifc_source_location get_ifc_semicolon(
                                 const an_ifc_syntax_for_statement &universal);

extern a_boolean validate(const an_ifc_syntax_for_statement &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_for_statement &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_syntax_for_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_for_statement_storage* get<an_ifc_syntax_for_statement_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_syntax_for_statement_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_for_statement_storage>()
/*
Return the corresponding partition kind for SyntaxForStatement.
*/
{
  return ifc_pk_syntax_for_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxFunctionBody nodes.
*/

extern a_boolean has_ifc_assign(const an_ifc_syntax_function_body &universal);

extern an_ifc_source_location get_ifc_assign(
                                 const an_ifc_syntax_function_body &universal);

extern a_boolean has_ifc_generate(
                                 const an_ifc_syntax_function_body &universal);

extern an_ifc_keyword_syntax get_ifc_generate(
                                 const an_ifc_syntax_function_body &universal);

extern a_boolean has_ifc_initializers(
                                 const an_ifc_syntax_function_body &universal);

extern an_ifc_syntax_index get_ifc_initializers(
                                 const an_ifc_syntax_function_body &universal);

extern a_boolean has_ifc_semicolon(
                                 const an_ifc_syntax_function_body &universal);

extern an_ifc_source_location get_ifc_semicolon(
                                 const an_ifc_syntax_function_body &universal);

extern a_boolean has_ifc_stmts(const an_ifc_syntax_function_body &universal);

extern an_ifc_syntax_index get_ifc_stmts(
                                 const an_ifc_syntax_function_body &universal);

extern a_boolean has_ifc_try_block(
                                 const an_ifc_syntax_function_body &universal);

extern an_ifc_syntax_index get_ifc_try_block(
                                 const an_ifc_syntax_function_body &universal);

extern a_boolean validate(const an_ifc_syntax_function_body &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_function_body &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_syntax_function_body &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_function_body_storage* get<an_ifc_syntax_function_body_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_syntax_function_body_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_function_body_storage>()
/*
Return the corresponding partition kind for SyntaxFunctionBody.
*/
{
  return ifc_pk_syntax_function_body;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxFunctionDeclarator nodes.
*/

extern a_boolean has_ifc_eh_spec(
                           const an_ifc_syntax_function_declarator &universal);

extern an_ifc_syntax_index get_ifc_eh_spec(
                           const an_ifc_syntax_function_declarator &universal);

extern a_boolean has_ifc_left_paren(
                           const an_ifc_syntax_function_declarator &universal);

extern an_ifc_source_location get_ifc_left_paren(
                           const an_ifc_syntax_function_declarator &universal);

extern a_boolean has_ifc_parameters(
                           const an_ifc_syntax_function_declarator &universal);

extern an_ifc_syntax_index get_ifc_parameters(
                           const an_ifc_syntax_function_declarator &universal);

extern a_boolean has_ifc_right_paren(
                           const an_ifc_syntax_function_declarator &universal);

extern an_ifc_source_location get_ifc_right_paren(
                           const an_ifc_syntax_function_declarator &universal);

extern a_boolean validate(const an_ifc_syntax_function_declarator &universal,
                          const an_ifc_validation_trace           *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_function_declarator &universal,
                    unsigned                                indent);

extern void db_node(const an_ifc_syntax_function_declarator &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_function_declarator_storage*
get<an_ifc_syntax_function_declarator_storage>(
                       an_ifc_module                             *mod,
                       an_ifc_syntax_function_declarator_storage *storage,
                       a_boolean                                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_function_declarator_storage>()
/*
Return the corresponding partition kind for SyntaxFunctionDeclarator.
*/
{
  return ifc_pk_syntax_function_declarator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxFunctionDefinition nodes.
*/

extern a_boolean has_ifc_assign(
                           const an_ifc_syntax_function_definition &universal);

extern an_ifc_source_location get_ifc_assign(
                           const an_ifc_syntax_function_definition &universal);

extern a_boolean has_ifc_initializers(
                           const an_ifc_syntax_function_definition &universal);

extern an_ifc_syntax_index get_ifc_initializers(
                           const an_ifc_syntax_function_definition &universal);

extern a_boolean has_ifc_semicolon(
                           const an_ifc_syntax_function_definition &universal);

extern an_ifc_source_location get_ifc_semicolon(
                           const an_ifc_syntax_function_definition &universal);

extern a_boolean has_ifc_stmts(
                           const an_ifc_syntax_function_definition &universal);

extern an_ifc_syntax_index get_ifc_stmts(
                           const an_ifc_syntax_function_definition &universal);

extern a_boolean has_ifc_synthesis(
                           const an_ifc_syntax_function_definition &universal);

extern an_ifc_keyword_syntax get_ifc_synthesis(
                           const an_ifc_syntax_function_definition &universal);

extern a_boolean has_ifc_try_block(
                           const an_ifc_syntax_function_definition &universal);

extern an_ifc_syntax_index get_ifc_try_block(
                           const an_ifc_syntax_function_definition &universal);

extern a_boolean validate(const an_ifc_syntax_function_definition &universal,
                          const an_ifc_validation_trace           *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_function_definition &universal,
                    unsigned                                indent);

extern void db_node(const an_ifc_syntax_function_definition &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_function_definition_storage*
get<an_ifc_syntax_function_definition_storage>(
                       an_ifc_module                             *mod,
                       an_ifc_syntax_function_definition_storage *storage,
                       a_boolean                                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_function_definition_storage>()
/*
Return the corresponding partition kind for SyntaxFunctionDefinition.
*/
{
  return ifc_pk_syntax_function_definition;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxFunctionTryBlock nodes.
*/

extern a_boolean has_ifc_body(
                            const an_ifc_syntax_function_try_block &universal);

extern an_ifc_syntax_index get_ifc_body(
                            const an_ifc_syntax_function_try_block &universal);

extern a_boolean has_ifc_handlers(
                            const an_ifc_syntax_function_try_block &universal);

extern an_ifc_syntax_index get_ifc_handlers(
                            const an_ifc_syntax_function_try_block &universal);

extern a_boolean has_ifc_initializers(
                            const an_ifc_syntax_function_try_block &universal);

extern an_ifc_syntax_index get_ifc_initializers(
                            const an_ifc_syntax_function_try_block &universal);

extern a_boolean validate(const an_ifc_syntax_function_try_block &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_function_try_block &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_function_try_block &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_function_try_block_storage*
get<an_ifc_syntax_function_try_block_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_function_try_block_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_function_try_block_storage>()
/*
Return the corresponding partition kind for SyntaxFunctionTryBlock.
*/
{
  return ifc_pk_syntax_function_try_block;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxGotoStatement nodes.
*/

extern a_boolean has_ifc_label(const an_ifc_syntax_goto_statement &universal);

extern an_ifc_source_location get_ifc_label(
                                const an_ifc_syntax_goto_statement &universal);

extern a_boolean has_ifc_locus(const an_ifc_syntax_goto_statement &universal);

extern an_ifc_source_location get_ifc_locus(
                                const an_ifc_syntax_goto_statement &universal);

extern a_boolean has_ifc_pragma(const an_ifc_syntax_goto_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                                const an_ifc_syntax_goto_statement &universal);

extern a_boolean has_ifc_semicolon(
                                const an_ifc_syntax_goto_statement &universal);

extern an_ifc_source_location get_ifc_semicolon(
                                const an_ifc_syntax_goto_statement &universal);

extern a_boolean has_ifc_target(const an_ifc_syntax_goto_statement &universal);

extern an_ifc_text_offset get_ifc_target(
                                const an_ifc_syntax_goto_statement &universal);

extern a_boolean validate(const an_ifc_syntax_goto_statement &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_goto_statement &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_syntax_goto_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_goto_statement_storage*
get<an_ifc_syntax_goto_statement_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_syntax_goto_statement_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_goto_statement_storage>()
/*
Return the corresponding partition kind for SyntaxGotoStatement.
*/
{
  return ifc_pk_syntax_goto_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxHandler nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_syntax_handler &universal);

extern an_ifc_syntax_index get_ifc_body(
                                       const an_ifc_syntax_handler &universal);

extern a_boolean has_ifc_catch(const an_ifc_syntax_handler &universal);

extern an_ifc_source_location get_ifc_catch(
                                       const an_ifc_syntax_handler &universal);

extern a_boolean has_ifc_exception(const an_ifc_syntax_handler &universal);

extern an_ifc_syntax_index get_ifc_exception(
                                       const an_ifc_syntax_handler &universal);

extern a_boolean has_ifc_left_paren(const an_ifc_syntax_handler &universal);

extern an_ifc_source_location get_ifc_left_paren(
                                       const an_ifc_syntax_handler &universal);

extern a_boolean has_ifc_pragma(const an_ifc_syntax_handler &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                                       const an_ifc_syntax_handler &universal);

extern a_boolean has_ifc_right_paren(const an_ifc_syntax_handler &universal);

extern an_ifc_source_location get_ifc_right_paren(
                                       const an_ifc_syntax_handler &universal);

extern a_boolean validate(const an_ifc_syntax_handler   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_handler &universal, unsigned indent);

extern void db_node(const an_ifc_syntax_handler &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_handler_storage* get<an_ifc_syntax_handler_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_syntax_handler_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_handler_storage>()
/*
Return the corresponding partition kind for SyntaxHandler.
*/
{
  return ifc_pk_syntax_handler;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxHandlerSeq nodes.
*/

extern a_boolean has_ifc_handlers(const an_ifc_syntax_handler_seq &universal);

extern an_ifc_syntax_index get_ifc_handlers(
                                   const an_ifc_syntax_handler_seq &universal);

extern a_boolean validate(const an_ifc_syntax_handler_seq &universal,
                          const an_ifc_validation_trace   *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_handler_seq &universal,
                    unsigned                        indent);

extern void db_node(const an_ifc_syntax_handler_seq &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_handler_seq_storage* get<an_ifc_syntax_handler_seq_storage>(
                               an_ifc_module                     *mod,
                               an_ifc_syntax_handler_seq_storage *storage,
                               a_boolean                         fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_handler_seq_storage>()
/*
Return the corresponding partition kind for SyntaxHandlerSeq.
*/
{
  return ifc_pk_syntax_handler_seq;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxIfStatement nodes.
*/

extern a_boolean has_ifc_alternative(
                                  const an_ifc_syntax_if_statement &universal);

extern an_ifc_syntax_index get_ifc_alternative(
                                  const an_ifc_syntax_if_statement &universal);

extern a_boolean has_ifc_condition(
                                  const an_ifc_syntax_if_statement &universal);

extern an_ifc_index get_ifc_condition(
                                  const an_ifc_syntax_if_statement &universal);

extern a_boolean has_ifc_consequence(
                                  const an_ifc_syntax_if_statement &universal);

extern an_ifc_syntax_index get_ifc_consequence(
                                  const an_ifc_syntax_if_statement &universal);

extern a_boolean has_ifc_constexpr(
                                  const an_ifc_syntax_if_statement &universal);

extern an_ifc_source_location get_ifc_constexpr(
                                  const an_ifc_syntax_if_statement &universal);

extern a_boolean has_ifc_else(const an_ifc_syntax_if_statement &universal);

extern an_ifc_source_location get_ifc_else(
                                  const an_ifc_syntax_if_statement &universal);

extern a_boolean has_ifc_if(const an_ifc_syntax_if_statement &universal);

extern an_ifc_source_location get_ifc_if(
                                  const an_ifc_syntax_if_statement &universal);

extern a_boolean has_ifc_initialization(
                                  const an_ifc_syntax_if_statement &universal);

extern an_ifc_syntax_index get_ifc_initialization(
                                  const an_ifc_syntax_if_statement &universal);

extern a_boolean has_ifc_pragma(const an_ifc_syntax_if_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                                  const an_ifc_syntax_if_statement &universal);

extern a_boolean validate(const an_ifc_syntax_if_statement &universal,
                          const an_ifc_validation_trace    *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_if_statement &universal,
                    unsigned                         indent);

extern void db_node(const an_ifc_syntax_if_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_if_statement_storage* get<an_ifc_syntax_if_statement_storage>(
                              an_ifc_module                      *mod,
                              an_ifc_syntax_if_statement_storage *storage,
                              a_boolean                          fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_if_statement_storage>()
/*
Return the corresponding partition kind for SyntaxIfStatement.
*/
{
  return ifc_pk_syntax_if_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxInitCapture nodes.
*/

extern a_boolean has_ifc_ampersand(
                                  const an_ifc_syntax_init_capture &universal);

extern an_ifc_source_location get_ifc_ampersand(
                                  const an_ifc_syntax_init_capture &universal);

extern a_boolean has_ifc_comma(const an_ifc_syntax_init_capture &universal);

extern an_ifc_source_location get_ifc_comma(
                                  const an_ifc_syntax_init_capture &universal);

extern a_boolean has_ifc_expander(const an_ifc_syntax_init_capture &universal);

extern an_ifc_source_location get_ifc_expander(
                                  const an_ifc_syntax_init_capture &universal);

extern a_boolean has_ifc_initializer(
                                  const an_ifc_syntax_init_capture &universal);

extern an_ifc_expr_index get_ifc_initializer(
                                  const an_ifc_syntax_init_capture &universal);

extern a_boolean has_ifc_name(const an_ifc_syntax_init_capture &universal);

extern an_ifc_expr_index get_ifc_name(
                                  const an_ifc_syntax_init_capture &universal);

extern a_boolean validate(const an_ifc_syntax_init_capture &universal,
                          const an_ifc_validation_trace    *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_init_capture &universal,
                    unsigned                         indent);

extern void db_node(const an_ifc_syntax_init_capture &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_init_capture_storage* get<an_ifc_syntax_init_capture_storage>(
                              an_ifc_module                      *mod,
                              an_ifc_syntax_init_capture_storage *storage,
                              a_boolean                          fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_init_capture_storage>()
/*
Return the corresponding partition kind for SyntaxInitCapture.
*/
{
  return ifc_pk_syntax_init_capture;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxInitDeclarator nodes.
*/

extern a_boolean has_ifc_comma(const an_ifc_syntax_init_declarator &universal);

extern an_ifc_source_location get_ifc_comma(
                               const an_ifc_syntax_init_declarator &universal);

extern a_boolean has_ifc_constraint(
                               const an_ifc_syntax_init_declarator &universal);

extern an_ifc_syntax_index get_ifc_constraint(
                               const an_ifc_syntax_init_declarator &universal);

extern a_boolean has_ifc_declarator(
                               const an_ifc_syntax_init_declarator &universal);

extern an_ifc_syntax_index get_ifc_declarator(
                               const an_ifc_syntax_init_declarator &universal);

extern a_boolean has_ifc_initializer(
                               const an_ifc_syntax_init_declarator &universal);

extern an_ifc_expr_index get_ifc_initializer(
                               const an_ifc_syntax_init_declarator &universal);

extern a_boolean validate(const an_ifc_syntax_init_declarator &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_init_declarator &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_syntax_init_declarator &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_init_declarator_storage*
get<an_ifc_syntax_init_declarator_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_syntax_init_declarator_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_init_declarator_storage>()
/*
Return the corresponding partition kind for SyntaxInitDeclarator.
*/
{
  return ifc_pk_syntax_init_declarator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxInitStatement nodes.
*/

extern a_boolean has_ifc_init(const an_ifc_syntax_init_statement &universal);

extern an_ifc_syntax_index get_ifc_init(
                                const an_ifc_syntax_init_statement &universal);

extern a_boolean has_ifc_pragma(const an_ifc_syntax_init_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                                const an_ifc_syntax_init_statement &universal);

extern a_boolean validate(const an_ifc_syntax_init_statement &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_init_statement &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_syntax_init_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_init_statement_storage*
get<an_ifc_syntax_init_statement_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_syntax_init_statement_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_init_statement_storage>()
/*
Return the corresponding partition kind for SyntaxInitStatement.
*/
{
  return ifc_pk_syntax_init_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxLabeledStatement nodes.
*/

extern a_boolean has_ifc_label(
                             const an_ifc_syntax_labeled_statement &universal);

extern an_ifc_expr_index get_ifc_label(
                             const an_ifc_syntax_labeled_statement &universal);

extern a_boolean has_ifc_locus(
                             const an_ifc_syntax_labeled_statement &universal);

extern an_ifc_keyword_sort get_ifc_locus(
                             const an_ifc_syntax_labeled_statement &universal);

extern a_boolean has_ifc_pragma(
                             const an_ifc_syntax_labeled_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                             const an_ifc_syntax_labeled_statement &universal);

extern a_boolean has_ifc_sort(
                             const an_ifc_syntax_labeled_statement &universal);

extern an_ifc_label_sort get_ifc_sort(
                             const an_ifc_syntax_labeled_statement &universal);

extern a_boolean has_ifc_stmt(
                             const an_ifc_syntax_labeled_statement &universal);

extern an_ifc_syntax_index get_ifc_stmt(
                             const an_ifc_syntax_labeled_statement &universal);

extern a_boolean validate(const an_ifc_syntax_labeled_statement &universal,
                          const an_ifc_validation_trace         *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_labeled_statement &universal,
                    unsigned                              indent);

extern void db_node(const an_ifc_syntax_labeled_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_labeled_statement_storage*
get<an_ifc_syntax_labeled_statement_storage>(
                         an_ifc_module                           *mod,
                         an_ifc_syntax_labeled_statement_storage *storage,
                         a_boolean                               fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_labeled_statement_storage>()
/*
Return the corresponding partition kind for SyntaxLabeledStatement.
*/
{
  return ifc_pk_syntax_labeled_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxLambdaDeclarator nodes.
*/

extern a_boolean has_ifc_eh_spec(
                             const an_ifc_syntax_lambda_declarator &universal);

extern an_ifc_syntax_index get_ifc_eh_spec(
                             const an_ifc_syntax_lambda_declarator &universal);

extern a_boolean has_ifc_expander(
                             const an_ifc_syntax_lambda_declarator &universal);

extern an_ifc_source_location get_ifc_expander(
                             const an_ifc_syntax_lambda_declarator &universal);

extern a_boolean has_ifc_left_paren(
                             const an_ifc_syntax_lambda_declarator &universal);

extern an_ifc_source_location get_ifc_left_paren(
                             const an_ifc_syntax_lambda_declarator &universal);

extern a_boolean has_ifc_modifier(
                             const an_ifc_syntax_lambda_declarator &universal);

extern an_ifc_keyword_sort get_ifc_modifier(
                             const an_ifc_syntax_lambda_declarator &universal);

extern a_boolean has_ifc_parameters(
                             const an_ifc_syntax_lambda_declarator &universal);

extern an_ifc_syntax_index get_ifc_parameters(
                             const an_ifc_syntax_lambda_declarator &universal);

extern a_boolean has_ifc_right_paren(
                             const an_ifc_syntax_lambda_declarator &universal);

extern an_ifc_source_location get_ifc_right_paren(
                             const an_ifc_syntax_lambda_declarator &universal);

extern a_boolean has_ifc_trailing_target(
                             const an_ifc_syntax_lambda_declarator &universal);

extern an_ifc_syntax_index get_ifc_trailing_target(
                             const an_ifc_syntax_lambda_declarator &universal);

extern a_boolean validate(const an_ifc_syntax_lambda_declarator &universal,
                          const an_ifc_validation_trace         *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_lambda_declarator &universal,
                    unsigned                              indent);

extern void db_node(const an_ifc_syntax_lambda_declarator &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_lambda_declarator_storage*
get<an_ifc_syntax_lambda_declarator_storage>(
                         an_ifc_module                           *mod,
                         an_ifc_syntax_lambda_declarator_storage *storage,
                         a_boolean                               fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_lambda_declarator_storage>()
/*
Return the corresponding partition kind for SyntaxLambdaDeclarator.
*/
{
  return ifc_pk_syntax_lambda_declarator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxLambdaIntroducer nodes.
*/

extern a_boolean has_ifc_captures(
                             const an_ifc_syntax_lambda_introducer &universal);

extern an_ifc_syntax_index get_ifc_captures(
                             const an_ifc_syntax_lambda_introducer &universal);

extern a_boolean has_ifc_left_bracket(
                             const an_ifc_syntax_lambda_introducer &universal);

extern an_ifc_source_location get_ifc_left_bracket(
                             const an_ifc_syntax_lambda_introducer &universal);

extern a_boolean has_ifc_right_bracket(
                             const an_ifc_syntax_lambda_introducer &universal);

extern an_ifc_source_location get_ifc_right_bracket(
                             const an_ifc_syntax_lambda_introducer &universal);

extern a_boolean validate(const an_ifc_syntax_lambda_introducer &universal,
                          const an_ifc_validation_trace         *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_lambda_introducer &universal,
                    unsigned                              indent);

extern void db_node(const an_ifc_syntax_lambda_introducer &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_lambda_introducer_storage*
get<an_ifc_syntax_lambda_introducer_storage>(
                         an_ifc_module                           *mod,
                         an_ifc_syntax_lambda_introducer_storage *storage,
                         a_boolean                               fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_lambda_introducer_storage>()
/*
Return the corresponding partition kind for SyntaxLambdaIntroducer.
*/
{
  return ifc_pk_syntax_lambda_introducer;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxMemInitializer nodes.
*/

extern a_boolean has_ifc_comma(const an_ifc_syntax_mem_initializer &universal);

extern an_ifc_source_location get_ifc_comma(
                               const an_ifc_syntax_mem_initializer &universal);

extern a_boolean has_ifc_expander(
                               const an_ifc_syntax_mem_initializer &universal);

extern an_ifc_source_location get_ifc_expander(
                               const an_ifc_syntax_mem_initializer &universal);

extern a_boolean has_ifc_initializer(
                               const an_ifc_syntax_mem_initializer &universal);

extern an_ifc_expr_index get_ifc_initializer(
                               const an_ifc_syntax_mem_initializer &universal);

extern a_boolean has_ifc_member(
                               const an_ifc_syntax_mem_initializer &universal);

extern an_ifc_expr_index get_ifc_member(
                               const an_ifc_syntax_mem_initializer &universal);

extern a_boolean validate(const an_ifc_syntax_mem_initializer &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_mem_initializer &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_syntax_mem_initializer &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_mem_initializer_storage*
get<an_ifc_syntax_mem_initializer_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_syntax_mem_initializer_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_mem_initializer_storage>()
/*
Return the corresponding partition kind for SyntaxMemInitializer.
*/
{
  return ifc_pk_syntax_mem_initializer;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxMemberDeclaration nodes.
*/

extern a_boolean has_ifc_decl_specifiers(
                            const an_ifc_syntax_member_declaration &universal);

extern an_ifc_syntax_index get_ifc_decl_specifiers(
                            const an_ifc_syntax_member_declaration &universal);

extern a_boolean has_ifc_declarations(
                            const an_ifc_syntax_member_declaration &universal);

extern an_ifc_syntax_index get_ifc_declarations(
                            const an_ifc_syntax_member_declaration &universal);

extern a_boolean has_ifc_semicolon(
                            const an_ifc_syntax_member_declaration &universal);

extern an_ifc_source_location get_ifc_semicolon(
                            const an_ifc_syntax_member_declaration &universal);

extern a_boolean validate(const an_ifc_syntax_member_declaration &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_member_declaration &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_member_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_member_declaration_storage*
get<an_ifc_syntax_member_declaration_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_member_declaration_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_member_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxMemberDeclaration.
*/
{
  return ifc_pk_syntax_member_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxMemberDeclarator nodes.
*/

extern a_boolean has_ifc_bitwidth(
                             const an_ifc_syntax_member_declarator &universal);

extern an_ifc_expr_index get_ifc_bitwidth(
                             const an_ifc_syntax_member_declarator &universal);

extern a_boolean has_ifc_colon(
                             const an_ifc_syntax_member_declarator &universal);

extern an_ifc_source_location get_ifc_colon(
                             const an_ifc_syntax_member_declarator &universal);

extern a_boolean has_ifc_comma(
                             const an_ifc_syntax_member_declarator &universal);

extern an_ifc_source_location get_ifc_comma(
                             const an_ifc_syntax_member_declarator &universal);

extern a_boolean has_ifc_constraint(
                             const an_ifc_syntax_member_declarator &universal);

extern an_ifc_syntax_index get_ifc_constraint(
                             const an_ifc_syntax_member_declarator &universal);

extern a_boolean has_ifc_declarator(
                             const an_ifc_syntax_member_declarator &universal);

extern an_ifc_syntax_index get_ifc_declarator(
                             const an_ifc_syntax_member_declarator &universal);

extern a_boolean has_ifc_initializer(
                             const an_ifc_syntax_member_declarator &universal);

extern an_ifc_expr_index get_ifc_initializer(
                             const an_ifc_syntax_member_declarator &universal);

extern a_boolean has_ifc_locus(
                             const an_ifc_syntax_member_declarator &universal);

extern an_ifc_source_location get_ifc_locus(
                             const an_ifc_syntax_member_declarator &universal);

extern a_boolean validate(const an_ifc_syntax_member_declarator &universal,
                          const an_ifc_validation_trace         *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_member_declarator &universal,
                    unsigned                              indent);

extern void db_node(const an_ifc_syntax_member_declarator &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_member_declarator_storage*
get<an_ifc_syntax_member_declarator_storage>(
                         an_ifc_module                           *mod,
                         an_ifc_syntax_member_declarator_storage *storage,
                         a_boolean                               fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_member_declarator_storage>()
/*
Return the corresponding partition kind for SyntaxMemberDeclarator.
*/
{
  return ifc_pk_syntax_member_declarator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxMemberFunctionDeclaration nodes.
*/

extern a_boolean has_ifc_definition(
                   const an_ifc_syntax_member_function_declaration &universal);

extern an_ifc_syntax_index get_ifc_definition(
                   const an_ifc_syntax_member_function_declaration &universal);

extern a_boolean validate(
                   const an_ifc_syntax_member_function_declaration &universal,
                   const an_ifc_validation_trace                   *parent);

#if DEBUG
extern void db_node(
                   const an_ifc_syntax_member_function_declaration &universal,
                   unsigned                                        indent);

extern void db_node(
                   const an_ifc_syntax_member_function_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_member_function_declaration_storage*
get<an_ifc_syntax_member_function_declaration_storage>(
               an_ifc_module                                     *mod,
               an_ifc_syntax_member_function_declaration_storage *storage,
               a_boolean                                         fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_member_function_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxMemberFunctionDeclaration.
*/
{
  return ifc_pk_syntax_member_function_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxMemberSpecification nodes.
*/

extern a_boolean has_ifc_member_declarations(
                          const an_ifc_syntax_member_specification &universal);

extern an_ifc_syntax_index get_ifc_member_declarations(
                          const an_ifc_syntax_member_specification &universal);

extern a_boolean validate(const an_ifc_syntax_member_specification &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_member_specification &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_syntax_member_specification &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_member_specification_storage*
get<an_ifc_syntax_member_specification_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_syntax_member_specification_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_member_specification_storage>()
/*
Return the corresponding partition kind for SyntaxMemberSpecification.
*/
{
  return ifc_pk_syntax_member_specification;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxNamespaceAliasDefinition nodes.
*/

extern a_boolean has_ifc_assign(
                    const an_ifc_syntax_namespace_alias_definition &universal);

extern an_ifc_source_location get_ifc_assign(
                    const an_ifc_syntax_namespace_alias_definition &universal);

extern a_boolean has_ifc_name(
                    const an_ifc_syntax_namespace_alias_definition &universal);

extern an_ifc_expr_index get_ifc_name(
                    const an_ifc_syntax_namespace_alias_definition &universal);

extern a_boolean has_ifc_namespace_kw(
                    const an_ifc_syntax_namespace_alias_definition &universal);

extern an_ifc_source_location get_ifc_namespace_kw(
                    const an_ifc_syntax_namespace_alias_definition &universal);

extern a_boolean has_ifc_semicolon(
                    const an_ifc_syntax_namespace_alias_definition &universal);

extern an_ifc_source_location get_ifc_semicolon(
                    const an_ifc_syntax_namespace_alias_definition &universal);

extern a_boolean has_ifc_target(
                    const an_ifc_syntax_namespace_alias_definition &universal);

extern an_ifc_expr_index get_ifc_target(
                    const an_ifc_syntax_namespace_alias_definition &universal);

extern a_boolean validate(
                    const an_ifc_syntax_namespace_alias_definition &universal,
                    const an_ifc_validation_trace                  *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_namespace_alias_definition &universal,
                    unsigned                                       indent);

extern void db_node(const an_ifc_syntax_namespace_alias_definition &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_namespace_alias_definition_storage*
get<an_ifc_syntax_namespace_alias_definition_storage>(
                an_ifc_module                                    *mod,
                an_ifc_syntax_namespace_alias_definition_storage *storage,
                a_boolean                                        fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_namespace_alias_definition_storage>()
/*
Return the corresponding partition kind for SyntaxNamespaceAliasDefinition.
*/
{
  return ifc_pk_syntax_namespace_alias_definition;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxNestedRequirement nodes.
*/

extern a_boolean has_ifc_condition(
                            const an_ifc_syntax_nested_requirement &universal);

extern an_ifc_expr_index get_ifc_condition(
                            const an_ifc_syntax_nested_requirement &universal);

extern a_boolean has_ifc_locus(
                            const an_ifc_syntax_nested_requirement &universal);

extern an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_nested_requirement &universal);

extern a_boolean validate(const an_ifc_syntax_nested_requirement &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_nested_requirement &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_nested_requirement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_nested_requirement_storage*
get<an_ifc_syntax_nested_requirement_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_nested_requirement_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_nested_requirement_storage>()
/*
Return the corresponding partition kind for SyntaxNestedRequirement.
*/
{
  return ifc_pk_syntax_nested_requirement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxNewDeclarator nodes.
*/

extern a_boolean has_ifc_declarator(
                                const an_ifc_syntax_new_declarator &universal);

extern an_ifc_syntax_index get_ifc_declarator(
                                const an_ifc_syntax_new_declarator &universal);

extern a_boolean validate(const an_ifc_syntax_new_declarator &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_new_declarator &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_syntax_new_declarator &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_new_declarator_storage*
get<an_ifc_syntax_new_declarator_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_syntax_new_declarator_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_new_declarator_storage>()
/*
Return the corresponding partition kind for SyntaxNewDeclarator.
*/
{
  return ifc_pk_syntax_new_declarator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxNoexceptSpecification nodes.
*/

extern a_boolean has_ifc_expr(
                        const an_ifc_syntax_noexcept_specification &universal);

extern an_ifc_syntax_index get_ifc_expr(
                        const an_ifc_syntax_noexcept_specification &universal);

extern a_boolean has_ifc_left_paren(
                        const an_ifc_syntax_noexcept_specification &universal);

extern an_ifc_source_location get_ifc_left_paren(
                        const an_ifc_syntax_noexcept_specification &universal);

extern a_boolean has_ifc_locus(
                        const an_ifc_syntax_noexcept_specification &universal);

extern an_ifc_source_location get_ifc_locus(
                        const an_ifc_syntax_noexcept_specification &universal);

extern a_boolean has_ifc_right_paren(
                        const an_ifc_syntax_noexcept_specification &universal);

extern an_ifc_source_location get_ifc_right_paren(
                        const an_ifc_syntax_noexcept_specification &universal);

extern a_boolean validate(
                        const an_ifc_syntax_noexcept_specification &universal,
                        const an_ifc_validation_trace              *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_noexcept_specification &universal,
                    unsigned                                   indent);

extern void db_node(const an_ifc_syntax_noexcept_specification &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_noexcept_specification_storage*
get<an_ifc_syntax_noexcept_specification_storage>(
                    an_ifc_module                                *mod,
                    an_ifc_syntax_noexcept_specification_storage *storage,
                    a_boolean                                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_noexcept_specification_storage>()
/*
Return the corresponding partition kind for SyntaxNoexceptSpecification.
*/
{
  return ifc_pk_syntax_noexcept_specification;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxNonTypeTemplateArgument nodes.
*/

extern a_boolean has_ifc_argument(
                    const an_ifc_syntax_non_type_template_argument &universal);

extern an_ifc_expr_index get_ifc_argument(
                    const an_ifc_syntax_non_type_template_argument &universal);

extern a_boolean has_ifc_comma(
                    const an_ifc_syntax_non_type_template_argument &universal);

extern an_ifc_source_location get_ifc_comma(
                    const an_ifc_syntax_non_type_template_argument &universal);

extern a_boolean has_ifc_ellipsis(
                    const an_ifc_syntax_non_type_template_argument &universal);

extern an_ifc_source_location get_ifc_ellipsis(
                    const an_ifc_syntax_non_type_template_argument &universal);

extern a_boolean validate(
                    const an_ifc_syntax_non_type_template_argument &universal,
                    const an_ifc_validation_trace                  *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_non_type_template_argument &universal,
                    unsigned                                       indent);

extern void db_node(const an_ifc_syntax_non_type_template_argument &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_non_type_template_argument_storage*
get<an_ifc_syntax_non_type_template_argument_storage>(
                an_ifc_module                                    *mod,
                an_ifc_syntax_non_type_template_argument_storage *storage,
                a_boolean                                        fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_non_type_template_argument_storage>()
/*
Return the corresponding partition kind for SyntaxNonTypeTemplateArgument.
*/
{
  return ifc_pk_syntax_non_type_template_argument;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxParameterDeclarator nodes.
*/

extern a_boolean has_ifc_decl_specifiers(
                          const an_ifc_syntax_parameter_declarator &universal);

extern an_ifc_syntax_index get_ifc_decl_specifiers(
                          const an_ifc_syntax_parameter_declarator &universal);

extern a_boolean has_ifc_declarator(
                          const an_ifc_syntax_parameter_declarator &universal);

extern an_ifc_syntax_index get_ifc_declarator(
                          const an_ifc_syntax_parameter_declarator &universal);

extern a_boolean has_ifc_default_expr(
                          const an_ifc_syntax_parameter_declarator &universal);

extern an_ifc_expr_index get_ifc_default_expr(
                          const an_ifc_syntax_parameter_declarator &universal);

extern a_boolean has_ifc_locus(
                          const an_ifc_syntax_parameter_declarator &universal);

extern an_ifc_source_location get_ifc_locus(
                          const an_ifc_syntax_parameter_declarator &universal);

extern a_boolean has_ifc_sort(
                          const an_ifc_syntax_parameter_declarator &universal);

extern an_ifc_parameter_sort get_ifc_sort(
                          const an_ifc_syntax_parameter_declarator &universal);

extern a_boolean validate(const an_ifc_syntax_parameter_declarator &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_parameter_declarator &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_syntax_parameter_declarator &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_parameter_declarator_storage*
get<an_ifc_syntax_parameter_declarator_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_syntax_parameter_declarator_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_parameter_declarator_storage>()
/*
Return the corresponding partition kind for SyntaxParameterDeclarator.
*/
{
  return ifc_pk_syntax_parameter_declarator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxPlaceholderTypeSpecifier nodes.
*/

extern a_boolean has_ifc_basis(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

extern an_ifc_type_basis_sort get_ifc_basis(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

extern a_boolean has_ifc_constraint(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

extern an_ifc_expr_index get_ifc_constraint(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

extern a_boolean has_ifc_keyword(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

extern an_ifc_source_location get_ifc_keyword(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

extern a_boolean has_ifc_locus(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

extern an_ifc_source_location get_ifc_locus(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

extern a_boolean validate(
                    const an_ifc_syntax_placeholder_type_specifier &universal,
                    const an_ifc_validation_trace                  *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_placeholder_type_specifier &universal,
                    unsigned                                       indent);

extern void db_node(const an_ifc_syntax_placeholder_type_specifier &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_placeholder_type_specifier_storage*
get<an_ifc_syntax_placeholder_type_specifier_storage>(
                an_ifc_module                                    *mod,
                an_ifc_syntax_placeholder_type_specifier_storage *storage,
                a_boolean                                        fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_placeholder_type_specifier_storage>()
/*
Return the corresponding partition kind for SyntaxPlaceholderTypeSpecifier.
*/
{
  return ifc_pk_syntax_placeholder_type_specifier;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxPointerDeclarator nodes.
*/

extern a_boolean has_ifc_callable(
                            const an_ifc_syntax_pointer_declarator &universal);

extern an_ifc_bool get_ifc_callable(
                            const an_ifc_syntax_pointer_declarator &universal);

extern a_boolean has_ifc_convention(
                            const an_ifc_syntax_pointer_declarator &universal);

extern an_ifc_calling_convention_sort get_ifc_convention(
                            const an_ifc_syntax_pointer_declarator &universal);

extern a_boolean has_ifc_locus(
                            const an_ifc_syntax_pointer_declarator &universal);

extern an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_pointer_declarator &universal);

extern a_boolean has_ifc_next(
                            const an_ifc_syntax_pointer_declarator &universal);

extern an_ifc_syntax_index get_ifc_next(
                            const an_ifc_syntax_pointer_declarator &universal);

extern a_boolean has_ifc_qualifiers(
                            const an_ifc_syntax_pointer_declarator &universal);

extern an_ifc_qualifier_bitfield get_ifc_qualifiers(
                            const an_ifc_syntax_pointer_declarator &universal);

extern a_boolean has_ifc_sort(
                            const an_ifc_syntax_pointer_declarator &universal);

extern an_ifc_pointer_declarator_sort get_ifc_sort(
                            const an_ifc_syntax_pointer_declarator &universal);

extern a_boolean has_ifc_whole(
                            const an_ifc_syntax_pointer_declarator &universal);

extern an_ifc_syntax_index get_ifc_whole(
                            const an_ifc_syntax_pointer_declarator &universal);

extern a_boolean validate(const an_ifc_syntax_pointer_declarator &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_pointer_declarator &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_pointer_declarator &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_pointer_declarator_storage*
get<an_ifc_syntax_pointer_declarator_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_pointer_declarator_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_pointer_declarator_storage>()
/*
Return the corresponding partition kind for SyntaxPointerDeclarator.
*/
{
  return ifc_pk_syntax_pointer_declarator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxRangeBasedForStatement nodes.
*/

extern a_boolean has_ifc_body(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern an_ifc_syntax_index get_ifc_body(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern a_boolean has_ifc_colon(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern an_ifc_source_location get_ifc_colon(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern a_boolean has_ifc_decl(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern an_ifc_syntax_index get_ifc_decl(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern a_boolean has_ifc_for(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern an_ifc_source_location get_ifc_for(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern a_boolean has_ifc_init(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern an_ifc_syntax_index get_ifc_init(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern a_boolean has_ifc_initializer(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern an_ifc_syntax_index get_ifc_initializer(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern a_boolean has_ifc_left_paren(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern an_ifc_source_location get_ifc_left_paren(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern a_boolean has_ifc_pragma(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern a_boolean has_ifc_right_paren(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern an_ifc_source_location get_ifc_right_paren(
                     const an_ifc_syntax_range_based_for_statement &universal);

extern a_boolean validate(
                     const an_ifc_syntax_range_based_for_statement &universal,
                     const an_ifc_validation_trace                 *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_range_based_for_statement &universal,
                    unsigned                                      indent);

extern void db_node(const an_ifc_syntax_range_based_for_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_range_based_for_statement_storage*
get<an_ifc_syntax_range_based_for_statement_storage>(
                 an_ifc_module                                   *mod,
                 an_ifc_syntax_range_based_for_statement_storage *storage,
                 a_boolean                                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_range_based_for_statement_storage>()
/*
Return the corresponding partition kind for SyntaxRangeBasedForStatement.
*/
{
  return ifc_pk_syntax_range_based_for_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxRequirementBody nodes.
*/

extern a_boolean has_ifc_locus(
                              const an_ifc_syntax_requirement_body &universal);

extern an_ifc_source_location get_ifc_locus(
                              const an_ifc_syntax_requirement_body &universal);

extern a_boolean has_ifc_requirements(
                              const an_ifc_syntax_requirement_body &universal);

extern an_ifc_syntax_index get_ifc_requirements(
                              const an_ifc_syntax_requirement_body &universal);

extern a_boolean has_ifc_right_curly(
                              const an_ifc_syntax_requirement_body &universal);

extern an_ifc_source_location get_ifc_right_curly(
                              const an_ifc_syntax_requirement_body &universal);

extern a_boolean validate(const an_ifc_syntax_requirement_body &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_requirement_body &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_syntax_requirement_body &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_requirement_body_storage*
get<an_ifc_syntax_requirement_body_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_syntax_requirement_body_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_requirement_body_storage>()
/*
Return the corresponding partition kind for SyntaxRequirementBody.
*/
{
  return ifc_pk_syntax_requirement_body;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxRequiresClause nodes.
*/

extern a_boolean has_ifc_condition(
                               const an_ifc_syntax_requires_clause &universal);

extern an_ifc_expr_index get_ifc_condition(
                               const an_ifc_syntax_requires_clause &universal);

extern a_boolean has_ifc_locus(const an_ifc_syntax_requires_clause &universal);

extern an_ifc_source_location get_ifc_locus(
                               const an_ifc_syntax_requires_clause &universal);

extern a_boolean validate(const an_ifc_syntax_requires_clause &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_requires_clause &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_syntax_requires_clause &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_requires_clause_storage*
get<an_ifc_syntax_requires_clause_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_syntax_requires_clause_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_requires_clause_storage>()
/*
Return the corresponding partition kind for SyntaxRequiresClause.
*/
{
  return ifc_pk_syntax_requires_clause;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxReturnStatement nodes.
*/

extern a_boolean has_ifc_expr(const an_ifc_syntax_return_statement &universal);

extern an_ifc_expr_index get_ifc_expr(
                              const an_ifc_syntax_return_statement &universal);

extern a_boolean has_ifc_pragma(
                              const an_ifc_syntax_return_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                              const an_ifc_syntax_return_statement &universal);

extern a_boolean has_ifc_return(
                              const an_ifc_syntax_return_statement &universal);

extern an_ifc_source_location get_ifc_return(
                              const an_ifc_syntax_return_statement &universal);

extern a_boolean has_ifc_semicolon(
                              const an_ifc_syntax_return_statement &universal);

extern an_ifc_source_location get_ifc_semicolon(
                              const an_ifc_syntax_return_statement &universal);

extern a_boolean has_ifc_sort(const an_ifc_syntax_return_statement &universal);

extern an_ifc_return_sort get_ifc_sort(
                              const an_ifc_syntax_return_statement &universal);

extern a_boolean validate(const an_ifc_syntax_return_statement &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_return_statement &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_syntax_return_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_return_statement_storage*
get<an_ifc_syntax_return_statement_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_syntax_return_statement_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_return_statement_storage>()
/*
Return the corresponding partition kind for SyntaxReturnStatement.
*/
{
  return ifc_pk_syntax_return_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxSEHExcept nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_syntax_seh_except &universal);

extern an_ifc_syntax_index get_ifc_body(
                                    const an_ifc_syntax_seh_except &universal);

extern a_boolean has_ifc_condition(const an_ifc_syntax_seh_except &universal);

extern an_ifc_expr_index get_ifc_condition(
                                    const an_ifc_syntax_seh_except &universal);

extern a_boolean has_ifc_except_kw(const an_ifc_syntax_seh_except &universal);

extern an_ifc_source_location get_ifc_except_kw(
                                    const an_ifc_syntax_seh_except &universal);

extern a_boolean has_ifc_left_paren(const an_ifc_syntax_seh_except &universal);

extern an_ifc_source_location get_ifc_left_paren(
                                    const an_ifc_syntax_seh_except &universal);

extern a_boolean has_ifc_right_paren(
                                    const an_ifc_syntax_seh_except &universal);

extern an_ifc_source_location get_ifc_right_paren(
                                    const an_ifc_syntax_seh_except &universal);

extern a_boolean validate(const an_ifc_syntax_seh_except &universal,
                          const an_ifc_validation_trace  *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_seh_except &universal,
                    unsigned                       indent);

extern void db_node(const an_ifc_syntax_seh_except &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_seh_except_storage* get<an_ifc_syntax_seh_except_storage>(
                                an_ifc_module                    *mod,
                                an_ifc_syntax_seh_except_storage *storage,
                                a_boolean                        fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_seh_except_storage>()
/*
Return the corresponding partition kind for SyntaxSEHExcept.
*/
{
  return ifc_pk_syntax_seh_except;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxSEHFinally nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_syntax_seh_finally &universal);

extern an_ifc_syntax_index get_ifc_body(
                                   const an_ifc_syntax_seh_finally &universal);

extern a_boolean has_ifc_finally_kw(
                                   const an_ifc_syntax_seh_finally &universal);

extern an_ifc_source_location get_ifc_finally_kw(
                                   const an_ifc_syntax_seh_finally &universal);

extern a_boolean validate(const an_ifc_syntax_seh_finally &universal,
                          const an_ifc_validation_trace   *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_seh_finally &universal,
                    unsigned                        indent);

extern void db_node(const an_ifc_syntax_seh_finally &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_seh_finally_storage* get<an_ifc_syntax_seh_finally_storage>(
                               an_ifc_module                     *mod,
                               an_ifc_syntax_seh_finally_storage *storage,
                               a_boolean                         fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_seh_finally_storage>()
/*
Return the corresponding partition kind for SyntaxSEHFinally.
*/
{
  return ifc_pk_syntax_seh_finally;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxSEHLeave nodes.
*/

extern a_boolean has_ifc_leave_kw(const an_ifc_syntax_seh_leave &universal);

extern an_ifc_source_location get_ifc_leave_kw(
                                     const an_ifc_syntax_seh_leave &universal);

extern a_boolean has_ifc_semicolon(const an_ifc_syntax_seh_leave &universal);

extern an_ifc_source_location get_ifc_semicolon(
                                     const an_ifc_syntax_seh_leave &universal);

extern a_boolean validate(const an_ifc_syntax_seh_leave &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_seh_leave &universal, unsigned indent);

extern void db_node(const an_ifc_syntax_seh_leave &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_seh_leave_storage* get<an_ifc_syntax_seh_leave_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_syntax_seh_leave_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_seh_leave_storage>()
/*
Return the corresponding partition kind for SyntaxSEHLeave.
*/
{
  return ifc_pk_syntax_seh_leave;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxSEHTry nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_syntax_seh_try &universal);

extern an_ifc_syntax_index get_ifc_body(
                                       const an_ifc_syntax_seh_try &universal);

extern a_boolean has_ifc_handler(const an_ifc_syntax_seh_try &universal);

extern an_ifc_syntax_index get_ifc_handler(
                                       const an_ifc_syntax_seh_try &universal);

extern a_boolean has_ifc_try_kw(const an_ifc_syntax_seh_try &universal);

extern an_ifc_source_location get_ifc_try_kw(
                                       const an_ifc_syntax_seh_try &universal);

extern a_boolean validate(const an_ifc_syntax_seh_try   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_seh_try &universal, unsigned indent);

extern void db_node(const an_ifc_syntax_seh_try &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_seh_try_storage* get<an_ifc_syntax_seh_try_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_syntax_seh_try_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_seh_try_storage>()
/*
Return the corresponding partition kind for SyntaxSEHTry.
*/
{
  return ifc_pk_syntax_seh_try;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxSimpleCapture nodes.
*/

extern a_boolean has_ifc_ampersand(
                                const an_ifc_syntax_simple_capture &universal);

extern an_ifc_source_location get_ifc_ampersand(
                                const an_ifc_syntax_simple_capture &universal);

extern a_boolean has_ifc_comma(const an_ifc_syntax_simple_capture &universal);

extern an_ifc_source_location get_ifc_comma(
                                const an_ifc_syntax_simple_capture &universal);

extern a_boolean has_ifc_expander(
                                const an_ifc_syntax_simple_capture &universal);

extern an_ifc_source_location get_ifc_expander(
                                const an_ifc_syntax_simple_capture &universal);

extern a_boolean has_ifc_name(const an_ifc_syntax_simple_capture &universal);

extern an_ifc_expr_index get_ifc_name(
                                const an_ifc_syntax_simple_capture &universal);

extern a_boolean validate(const an_ifc_syntax_simple_capture &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_simple_capture &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_syntax_simple_capture &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_simple_capture_storage*
get<an_ifc_syntax_simple_capture_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_syntax_simple_capture_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_simple_capture_storage>()
/*
Return the corresponding partition kind for SyntaxSimpleCapture.
*/
{
  return ifc_pk_syntax_simple_capture;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxSimpleDeclaration nodes.
*/

extern a_boolean has_ifc_decl_specifiers(
                            const an_ifc_syntax_simple_declaration &universal);

extern an_ifc_syntax_index get_ifc_decl_specifiers(
                            const an_ifc_syntax_simple_declaration &universal);

extern a_boolean has_ifc_declarators(
                            const an_ifc_syntax_simple_declaration &universal);

extern an_ifc_syntax_index get_ifc_declarators(
                            const an_ifc_syntax_simple_declaration &universal);

extern a_boolean has_ifc_locus(
                            const an_ifc_syntax_simple_declaration &universal);

extern an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_simple_declaration &universal);

extern a_boolean has_ifc_semicolon(
                            const an_ifc_syntax_simple_declaration &universal);

extern an_ifc_source_location get_ifc_semicolon(
                            const an_ifc_syntax_simple_declaration &universal);

extern a_boolean validate(const an_ifc_syntax_simple_declaration &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_simple_declaration &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_simple_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_simple_declaration_storage*
get<an_ifc_syntax_simple_declaration_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_simple_declaration_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_simple_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxSimpleDeclaration.
*/
{
  return ifc_pk_syntax_simple_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxSimpleRequirement nodes.
*/

extern a_boolean has_ifc_condition(
                            const an_ifc_syntax_simple_requirement &universal);

extern an_ifc_expr_index get_ifc_condition(
                            const an_ifc_syntax_simple_requirement &universal);

extern a_boolean has_ifc_locus(
                            const an_ifc_syntax_simple_requirement &universal);

extern an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_simple_requirement &universal);

extern a_boolean validate(const an_ifc_syntax_simple_requirement &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_simple_requirement &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_simple_requirement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_simple_requirement_storage*
get<an_ifc_syntax_simple_requirement_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_simple_requirement_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_simple_requirement_storage>()
/*
Return the corresponding partition kind for SyntaxSimpleRequirement.
*/
{
  return ifc_pk_syntax_simple_requirement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxSimpleTypeSpecifier nodes.
*/

extern a_boolean has_ifc_expr(
                         const an_ifc_syntax_simple_type_specifier &universal);

extern an_ifc_expr_index get_ifc_expr(
                         const an_ifc_syntax_simple_type_specifier &universal);

extern a_boolean has_ifc_locus(
                         const an_ifc_syntax_simple_type_specifier &universal);

extern an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_simple_type_specifier &universal);

extern a_boolean has_ifc_type(
                         const an_ifc_syntax_simple_type_specifier &universal);

extern an_ifc_type_index get_ifc_type(
                         const an_ifc_syntax_simple_type_specifier &universal);

extern a_boolean validate(
                         const an_ifc_syntax_simple_type_specifier &universal,
                         const an_ifc_validation_trace             *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_simple_type_specifier &universal,
                    unsigned                                  indent);

extern void db_node(const an_ifc_syntax_simple_type_specifier &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_simple_type_specifier_storage*
get<an_ifc_syntax_simple_type_specifier_storage>(
                     an_ifc_module                               *mod,
                     an_ifc_syntax_simple_type_specifier_storage *storage,
                     a_boolean                                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_simple_type_specifier_storage>()
/*
Return the corresponding partition kind for SyntaxSimpleTypeSpecifier.
*/
{
  return ifc_pk_syntax_simple_type_specifier;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxStatementSeq nodes.
*/

extern a_boolean has_ifc_stmts(const an_ifc_syntax_statement_seq &universal);

extern an_ifc_syntax_index get_ifc_stmts(
                                 const an_ifc_syntax_statement_seq &universal);

extern a_boolean validate(const an_ifc_syntax_statement_seq &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_statement_seq &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_syntax_statement_seq &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_statement_seq_storage* get<an_ifc_syntax_statement_seq_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_syntax_statement_seq_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_statement_seq_storage>()
/*
Return the corresponding partition kind for SyntaxStatementSeq.
*/
{
  return ifc_pk_syntax_statement_seq;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxStaticAssertDeclaration nodes.
*/

extern a_boolean has_ifc_comma(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern an_ifc_source_location get_ifc_comma(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern a_boolean has_ifc_condition(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern an_ifc_expr_index get_ifc_condition(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern a_boolean has_ifc_left_paren(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern an_ifc_source_location get_ifc_left_paren(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern a_boolean has_ifc_locus(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern an_ifc_source_location get_ifc_locus(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern a_boolean has_ifc_message(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern an_ifc_expr_index get_ifc_message(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern a_boolean has_ifc_right_paren(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern an_ifc_source_location get_ifc_right_paren(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern a_boolean has_ifc_semicolon(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern an_ifc_source_location get_ifc_semicolon(
                     const an_ifc_syntax_static_assert_declaration &universal);

extern a_boolean validate(
                     const an_ifc_syntax_static_assert_declaration &universal,
                     const an_ifc_validation_trace                 *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_static_assert_declaration &universal,
                    unsigned                                      indent);

extern void db_node(const an_ifc_syntax_static_assert_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_static_assert_declaration_storage*
get<an_ifc_syntax_static_assert_declaration_storage>(
                 an_ifc_module                                   *mod,
                 an_ifc_syntax_static_assert_declaration_storage *storage,
                 a_boolean                                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_static_assert_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxStaticAssertDeclaration.
*/
{
  return ifc_pk_syntax_static_assert_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxStructuredBindingDeclaration nodes.
*/

extern a_boolean has_ifc_initializer(
                const an_ifc_syntax_structured_binding_declaration &universal);

extern an_ifc_expr_index get_ifc_initializer(
                const an_ifc_syntax_structured_binding_declaration &universal);

extern a_boolean has_ifc_locus(
                const an_ifc_syntax_structured_binding_declaration &universal);

extern an_ifc_source_location get_ifc_locus(
                const an_ifc_syntax_structured_binding_declaration &universal);

extern a_boolean has_ifc_names(
                const an_ifc_syntax_structured_binding_declaration &universal);

extern an_ifc_syntax_index get_ifc_names(
                const an_ifc_syntax_structured_binding_declaration &universal);

extern a_boolean has_ifc_ref(
                const an_ifc_syntax_structured_binding_declaration &universal);

extern an_ifc_source_location get_ifc_ref(
                const an_ifc_syntax_structured_binding_declaration &universal);

extern a_boolean has_ifc_specifiers(
                const an_ifc_syntax_structured_binding_declaration &universal);

extern an_ifc_syntax_index get_ifc_specifiers(
                const an_ifc_syntax_structured_binding_declaration &universal);

extern a_boolean validate(
                const an_ifc_syntax_structured_binding_declaration &universal,
                const an_ifc_validation_trace                      *parent);

#if DEBUG
extern void db_node(
                const an_ifc_syntax_structured_binding_declaration &universal,
                unsigned                                           indent);

extern void db_node(
                const an_ifc_syntax_structured_binding_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_structured_binding_declaration_storage*
get<an_ifc_syntax_structured_binding_declaration_storage>(
            an_ifc_module                                        *mod,
            an_ifc_syntax_structured_binding_declaration_storage *storage,
            a_boolean                                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_structured_binding_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxStructuredBindingDeclaration.
*/
{
  return ifc_pk_syntax_structured_binding_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxStructuredBindingIdentifier nodes.
*/

extern a_boolean has_ifc_comma(
                 const an_ifc_syntax_structured_binding_identifier &universal);

extern an_ifc_source_location get_ifc_comma(
                 const an_ifc_syntax_structured_binding_identifier &universal);

extern a_boolean has_ifc_name(
                 const an_ifc_syntax_structured_binding_identifier &universal);

extern an_ifc_expr_index get_ifc_name(
                 const an_ifc_syntax_structured_binding_identifier &universal);

extern a_boolean validate(
                 const an_ifc_syntax_structured_binding_identifier &universal,
                 const an_ifc_validation_trace                     *parent);

#if DEBUG
extern void db_node(
                 const an_ifc_syntax_structured_binding_identifier &universal,
                 unsigned                                          indent);

extern void db_node(
                 const an_ifc_syntax_structured_binding_identifier &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_structured_binding_identifier_storage*
get<an_ifc_syntax_structured_binding_identifier_storage>(
             an_ifc_module                                       *mod,
             an_ifc_syntax_structured_binding_identifier_storage *storage,
             a_boolean                                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_structured_binding_identifier_storage>()
/*
Return the corresponding partition kind for SyntaxStructuredBindingIdentifier.
*/
{
  return ifc_pk_syntax_structured_binding_identifier;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxSuper nodes.
*/

extern a_boolean has_ifc_locus(const an_ifc_syntax_super &universal);

extern an_ifc_source_location get_ifc_locus(
                                         const an_ifc_syntax_super &universal);

extern a_boolean validate(const an_ifc_syntax_super     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_super &universal, unsigned indent);

extern void db_node(const an_ifc_syntax_super &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_super_storage* get<an_ifc_syntax_super_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_syntax_super_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_super_storage>()
/*
Return the corresponding partition kind for SyntaxSuper.
*/
{
  return ifc_pk_syntax_super;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxSwitchStatement nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_syntax_switch_statement &universal);

extern an_ifc_syntax_index get_ifc_body(
                              const an_ifc_syntax_switch_statement &universal);

extern a_boolean has_ifc_condition(
                              const an_ifc_syntax_switch_statement &universal);

extern an_ifc_syntax_index get_ifc_condition(
                              const an_ifc_syntax_switch_statement &universal);

extern a_boolean has_ifc_init(const an_ifc_syntax_switch_statement &universal);

extern an_ifc_syntax_index get_ifc_init(
                              const an_ifc_syntax_switch_statement &universal);

extern a_boolean has_ifc_pragma(
                              const an_ifc_syntax_switch_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                              const an_ifc_syntax_switch_statement &universal);

extern a_boolean has_ifc_switch(
                              const an_ifc_syntax_switch_statement &universal);

extern an_ifc_source_location get_ifc_switch(
                              const an_ifc_syntax_switch_statement &universal);

extern a_boolean validate(const an_ifc_syntax_switch_statement &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_switch_statement &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_syntax_switch_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_switch_statement_storage*
get<an_ifc_syntax_switch_statement_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_syntax_switch_statement_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_switch_statement_storage>()
/*
Return the corresponding partition kind for SyntaxSwitchStatement.
*/
{
  return ifc_pk_syntax_switch_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTemplateArgumentList nodes.
*/

extern a_boolean has_ifc_arguments(
                        const an_ifc_syntax_template_argument_list &universal);

extern an_ifc_syntax_index get_ifc_arguments(
                        const an_ifc_syntax_template_argument_list &universal);

extern a_boolean has_ifc_left_angle(
                        const an_ifc_syntax_template_argument_list &universal);

extern an_ifc_source_location get_ifc_left_angle(
                        const an_ifc_syntax_template_argument_list &universal);

extern a_boolean has_ifc_right_angle(
                        const an_ifc_syntax_template_argument_list &universal);

extern an_ifc_source_location get_ifc_right_angle(
                        const an_ifc_syntax_template_argument_list &universal);

extern a_boolean validate(
                        const an_ifc_syntax_template_argument_list &universal,
                        const an_ifc_validation_trace              *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_template_argument_list &universal,
                    unsigned                                   indent);

extern void db_node(const an_ifc_syntax_template_argument_list &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_template_argument_list_storage*
get<an_ifc_syntax_template_argument_list_storage>(
                    an_ifc_module                                *mod,
                    an_ifc_syntax_template_argument_list_storage *storage,
                    a_boolean                                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_template_argument_list_storage>()
/*
Return the corresponding partition kind for SyntaxTemplateArgumentList.
*/
{
  return ifc_pk_syntax_template_argument_list;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTemplateDeclaration nodes.
*/

extern a_boolean has_ifc_locus(
                          const an_ifc_syntax_template_declaration &universal);

extern an_ifc_source_location get_ifc_locus(
                          const an_ifc_syntax_template_declaration &universal);

extern a_boolean has_ifc_parameters(
                          const an_ifc_syntax_template_declaration &universal);

extern an_ifc_syntax_index get_ifc_parameters(
                          const an_ifc_syntax_template_declaration &universal);

extern a_boolean has_ifc_subject(
                          const an_ifc_syntax_template_declaration &universal);

extern an_ifc_syntax_index get_ifc_subject(
                          const an_ifc_syntax_template_declaration &universal);

extern a_boolean validate(const an_ifc_syntax_template_declaration &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_template_declaration &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_syntax_template_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_template_declaration_storage*
get<an_ifc_syntax_template_declaration_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_syntax_template_declaration_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_template_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxTemplateDeclaration.
*/
{
  return ifc_pk_syntax_template_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTemplateId nodes.
*/

extern a_boolean has_ifc_arguments(const an_ifc_syntax_template_id &universal);

extern an_ifc_syntax_index get_ifc_arguments(
                                   const an_ifc_syntax_template_id &universal);

extern a_boolean has_ifc_locus(const an_ifc_syntax_template_id &universal);

extern an_ifc_source_location get_ifc_locus(
                                   const an_ifc_syntax_template_id &universal);

extern a_boolean has_ifc_name(const an_ifc_syntax_template_id &universal);

extern an_ifc_syntax_index get_ifc_name(
                                   const an_ifc_syntax_template_id &universal);

extern a_boolean has_ifc_symbol(const an_ifc_syntax_template_id &universal);

extern an_ifc_expr_index get_ifc_symbol(
                                   const an_ifc_syntax_template_id &universal);

extern a_boolean has_ifc_template_kw(
                                   const an_ifc_syntax_template_id &universal);

extern an_ifc_source_location get_ifc_template_kw(
                                   const an_ifc_syntax_template_id &universal);

extern a_boolean validate(const an_ifc_syntax_template_id &universal,
                          const an_ifc_validation_trace   *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_template_id &universal,
                    unsigned                        indent);

extern void db_node(const an_ifc_syntax_template_id &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_template_id_storage* get<an_ifc_syntax_template_id_storage>(
                               an_ifc_module                     *mod,
                               an_ifc_syntax_template_id_storage *storage,
                               a_boolean                         fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_template_id_storage>()
/*
Return the corresponding partition kind for SyntaxTemplateId.
*/
{
  return ifc_pk_syntax_template_id;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTemplateParameterList nodes.
*/

extern a_boolean has_ifc_clause(
                       const an_ifc_syntax_template_parameter_list &universal);

extern an_ifc_syntax_index get_ifc_clause(
                       const an_ifc_syntax_template_parameter_list &universal);

extern a_boolean has_ifc_left_angle(
                       const an_ifc_syntax_template_parameter_list &universal);

extern an_ifc_source_location get_ifc_left_angle(
                       const an_ifc_syntax_template_parameter_list &universal);

extern a_boolean has_ifc_parameters(
                       const an_ifc_syntax_template_parameter_list &universal);

extern an_ifc_syntax_index get_ifc_parameters(
                       const an_ifc_syntax_template_parameter_list &universal);

extern a_boolean has_ifc_right_angle(
                       const an_ifc_syntax_template_parameter_list &universal);

extern an_ifc_source_location get_ifc_right_angle(
                       const an_ifc_syntax_template_parameter_list &universal);

extern a_boolean validate(
                       const an_ifc_syntax_template_parameter_list &universal,
                       const an_ifc_validation_trace               *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_template_parameter_list &universal,
                    unsigned                                    indent);

extern void db_node(const an_ifc_syntax_template_parameter_list &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_template_parameter_list_storage*
get<an_ifc_syntax_template_parameter_list_storage>(
                   an_ifc_module                                 *mod,
                   an_ifc_syntax_template_parameter_list_storage *storage,
                   a_boolean                                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_template_parameter_list_storage>()
/*
Return the corresponding partition kind for SyntaxTemplateParameterList.
*/
{
  return ifc_pk_syntax_template_parameter_list;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTemplateTemplateParameter nodes.
*/

extern a_boolean has_ifc_argument(
                   const an_ifc_syntax_template_template_parameter &universal);

extern an_ifc_syntax_index get_ifc_argument(
                   const an_ifc_syntax_template_template_parameter &universal);

extern a_boolean has_ifc_comma(
                   const an_ifc_syntax_template_template_parameter &universal);

extern an_ifc_source_location get_ifc_comma(
                   const an_ifc_syntax_template_template_parameter &universal);

extern a_boolean has_ifc_ellipsis(
                   const an_ifc_syntax_template_template_parameter &universal);

extern an_ifc_source_location get_ifc_ellipsis(
                   const an_ifc_syntax_template_template_parameter &universal);

extern a_boolean has_ifc_key(
                   const an_ifc_syntax_template_template_parameter &universal);

extern an_ifc_keyword_syntax get_ifc_key(
                   const an_ifc_syntax_template_template_parameter &universal);

extern a_boolean has_ifc_locus(
                   const an_ifc_syntax_template_template_parameter &universal);

extern an_ifc_source_location get_ifc_locus(
                   const an_ifc_syntax_template_template_parameter &universal);

extern a_boolean has_ifc_name(
                   const an_ifc_syntax_template_template_parameter &universal);

extern an_ifc_text_offset get_ifc_name(
                   const an_ifc_syntax_template_template_parameter &universal);

extern a_boolean has_ifc_parameters(
                   const an_ifc_syntax_template_template_parameter &universal);

extern an_ifc_syntax_index get_ifc_parameters(
                   const an_ifc_syntax_template_template_parameter &universal);

extern a_boolean validate(
                   const an_ifc_syntax_template_template_parameter &universal,
                   const an_ifc_validation_trace                   *parent);

#if DEBUG
extern void db_node(
                   const an_ifc_syntax_template_template_parameter &universal,
                   unsigned                                        indent);

extern void db_node(
                   const an_ifc_syntax_template_template_parameter &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_template_template_parameter_storage*
get<an_ifc_syntax_template_template_parameter_storage>(
               an_ifc_module                                     *mod,
               an_ifc_syntax_template_template_parameter_storage *storage,
               a_boolean                                         fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_template_template_parameter_storage>()
/*
Return the corresponding partition kind for SyntaxTemplateTemplateParameter.
*/
{
  return ifc_pk_syntax_template_template_parameter;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxThisCapture nodes.
*/

extern a_boolean has_ifc_asterisk(const an_ifc_syntax_this_capture &universal);

extern an_ifc_source_location get_ifc_asterisk(
                                  const an_ifc_syntax_this_capture &universal);

extern a_boolean has_ifc_comma(const an_ifc_syntax_this_capture &universal);

extern an_ifc_source_location get_ifc_comma(
                                  const an_ifc_syntax_this_capture &universal);

extern a_boolean has_ifc_locus(const an_ifc_syntax_this_capture &universal);

extern an_ifc_source_location get_ifc_locus(
                                  const an_ifc_syntax_this_capture &universal);

extern a_boolean validate(const an_ifc_syntax_this_capture &universal,
                          const an_ifc_validation_trace    *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_this_capture &universal,
                    unsigned                         indent);

extern void db_node(const an_ifc_syntax_this_capture &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_this_capture_storage* get<an_ifc_syntax_this_capture_storage>(
                              an_ifc_module                      *mod,
                              an_ifc_syntax_this_capture_storage *storage,
                              a_boolean                          fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_this_capture_storage>()
/*
Return the corresponding partition kind for SyntaxThisCapture.
*/
{
  return ifc_pk_syntax_this_capture;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTrailingReturnType nodes.
*/

extern a_boolean has_ifc_arrow(
                          const an_ifc_syntax_trailing_return_type &universal);

extern an_ifc_source_location get_ifc_arrow(
                          const an_ifc_syntax_trailing_return_type &universal);

extern a_boolean has_ifc_target(
                          const an_ifc_syntax_trailing_return_type &universal);

extern an_ifc_syntax_index get_ifc_target(
                          const an_ifc_syntax_trailing_return_type &universal);

extern a_boolean validate(const an_ifc_syntax_trailing_return_type &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_trailing_return_type &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_syntax_trailing_return_type &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_trailing_return_type_storage*
get<an_ifc_syntax_trailing_return_type_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_syntax_trailing_return_type_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_trailing_return_type_storage>()
/*
Return the corresponding partition kind for SyntaxTrailingReturnType.
*/
{
  return ifc_pk_syntax_trailing_return_type;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTryBlock nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_syntax_try_block &universal);

extern an_ifc_syntax_index get_ifc_body(
                                     const an_ifc_syntax_try_block &universal);

extern a_boolean has_ifc_handlers(const an_ifc_syntax_try_block &universal);

extern an_ifc_syntax_index get_ifc_handlers(
                                     const an_ifc_syntax_try_block &universal);

extern a_boolean has_ifc_pragma(const an_ifc_syntax_try_block &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                                     const an_ifc_syntax_try_block &universal);

extern a_boolean has_ifc_try(const an_ifc_syntax_try_block &universal);

extern an_ifc_source_location get_ifc_try(
                                     const an_ifc_syntax_try_block &universal);

extern a_boolean validate(const an_ifc_syntax_try_block &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_try_block &universal, unsigned indent);

extern void db_node(const an_ifc_syntax_try_block &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_try_block_storage* get<an_ifc_syntax_try_block_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_syntax_try_block_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_try_block_storage>()
/*
Return the corresponding partition kind for SyntaxTryBlock.
*/
{
  return ifc_pk_syntax_try_block;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTuple nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_syntax_tuple &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                         const an_ifc_syntax_tuple &universal);

extern a_boolean has_ifc_start(const an_ifc_syntax_tuple &universal);

extern an_ifc_index get_ifc_start(const an_ifc_syntax_tuple &universal);

extern a_boolean validate(const an_ifc_syntax_tuple     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_tuple &universal, unsigned indent);

extern void db_node(const an_ifc_syntax_tuple &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_tuple_storage* get<an_ifc_syntax_tuple_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_syntax_tuple_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_tuple_storage>()
/*
Return the corresponding partition kind for SyntaxTuple.
*/
{
  return ifc_pk_syntax_tuple;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTypeId nodes.
*/

extern a_boolean has_ifc_abstract_declarator(
                                       const an_ifc_syntax_type_id &universal);

extern an_ifc_syntax_index get_ifc_abstract_declarator(
                                       const an_ifc_syntax_type_id &universal);

extern a_boolean has_ifc_locus(const an_ifc_syntax_type_id &universal);

extern an_ifc_source_location get_ifc_locus(
                                       const an_ifc_syntax_type_id &universal);

extern a_boolean has_ifc_type_specifier(
                                       const an_ifc_syntax_type_id &universal);

extern an_ifc_syntax_index get_ifc_type_specifier(
                                       const an_ifc_syntax_type_id &universal);

extern a_boolean validate(const an_ifc_syntax_type_id   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_type_id &universal, unsigned indent);

extern void db_node(const an_ifc_syntax_type_id &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_type_id_storage* get<an_ifc_syntax_type_id_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_syntax_type_id_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_id_storage>()
/*
Return the corresponding partition kind for SyntaxTypeId.
*/
{
  return ifc_pk_syntax_type_id;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTypeIdListElement nodes.
*/

extern a_boolean has_ifc_ellipsis(
                          const an_ifc_syntax_type_id_list_element &universal);

extern an_ifc_source_location get_ifc_ellipsis(
                          const an_ifc_syntax_type_id_list_element &universal);

extern a_boolean has_ifc_type_id(
                          const an_ifc_syntax_type_id_list_element &universal);

extern an_ifc_syntax_index get_ifc_type_id(
                          const an_ifc_syntax_type_id_list_element &universal);

extern a_boolean validate(const an_ifc_syntax_type_id_list_element &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_type_id_list_element &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_syntax_type_id_list_element &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_type_id_list_element_storage*
get<an_ifc_syntax_type_id_list_element_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_syntax_type_id_list_element_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_id_list_element_storage>()
/*
Return the corresponding partition kind for SyntaxTypeIdListElement.
*/
{
  return ifc_pk_syntax_type_id_list_element;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTypeRequirement nodes.
*/

extern a_boolean has_ifc_locus(
                              const an_ifc_syntax_type_requirement &universal);

extern an_ifc_source_location get_ifc_locus(
                              const an_ifc_syntax_type_requirement &universal);

extern a_boolean has_ifc_type(const an_ifc_syntax_type_requirement &universal);

extern an_ifc_expr_index get_ifc_type(
                              const an_ifc_syntax_type_requirement &universal);

extern a_boolean validate(const an_ifc_syntax_type_requirement &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_type_requirement &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_syntax_type_requirement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_type_requirement_storage*
get<an_ifc_syntax_type_requirement_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_syntax_type_requirement_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_requirement_storage>()
/*
Return the corresponding partition kind for SyntaxTypeRequirement.
*/
{
  return ifc_pk_syntax_type_requirement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTypeSpecifierSeq nodes.
*/

extern a_boolean has_ifc_locus(
                            const an_ifc_syntax_type_specifier_seq &universal);

extern an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_type_specifier_seq &universal);

extern a_boolean has_ifc_qualifiers(
                            const an_ifc_syntax_type_specifier_seq &universal);

extern an_ifc_qualifier_bitfield get_ifc_qualifiers(
                            const an_ifc_syntax_type_specifier_seq &universal);

extern a_boolean has_ifc_type(
                            const an_ifc_syntax_type_specifier_seq &universal);

extern an_ifc_type_index get_ifc_type(
                            const an_ifc_syntax_type_specifier_seq &universal);

extern a_boolean has_ifc_type_name(
                            const an_ifc_syntax_type_specifier_seq &universal);

extern an_ifc_syntax_index get_ifc_type_name(
                            const an_ifc_syntax_type_specifier_seq &universal);

extern a_boolean has_ifc_unhashed(
                            const an_ifc_syntax_type_specifier_seq &universal);

extern an_ifc_bool get_ifc_unhashed(
                            const an_ifc_syntax_type_specifier_seq &universal);

extern a_boolean validate(const an_ifc_syntax_type_specifier_seq &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_type_specifier_seq &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_syntax_type_specifier_seq &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_type_specifier_seq_storage*
get<an_ifc_syntax_type_specifier_seq_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_syntax_type_specifier_seq_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_specifier_seq_storage>()
/*
Return the corresponding partition kind for SyntaxTypeSpecifierSeq.
*/
{
  return ifc_pk_syntax_type_specifier_seq;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTypeTemplateArgument nodes.
*/

extern a_boolean has_ifc_argument(
                        const an_ifc_syntax_type_template_argument &universal);

extern an_ifc_syntax_index get_ifc_argument(
                        const an_ifc_syntax_type_template_argument &universal);

extern a_boolean has_ifc_comma(
                        const an_ifc_syntax_type_template_argument &universal);

extern an_ifc_source_location get_ifc_comma(
                        const an_ifc_syntax_type_template_argument &universal);

extern a_boolean has_ifc_ellipsis(
                        const an_ifc_syntax_type_template_argument &universal);

extern an_ifc_source_location get_ifc_ellipsis(
                        const an_ifc_syntax_type_template_argument &universal);

extern a_boolean validate(
                        const an_ifc_syntax_type_template_argument &universal,
                        const an_ifc_validation_trace              *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_type_template_argument &universal,
                    unsigned                                   indent);

extern void db_node(const an_ifc_syntax_type_template_argument &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_type_template_argument_storage*
get<an_ifc_syntax_type_template_argument_storage>(
                    an_ifc_module                                *mod,
                    an_ifc_syntax_type_template_argument_storage *storage,
                    a_boolean                                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_template_argument_storage>()
/*
Return the corresponding partition kind for SyntaxTypeTemplateArgument.
*/
{
  return ifc_pk_syntax_type_template_argument;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTypeTemplateParameter nodes.
*/

extern a_boolean has_ifc_argument(
                       const an_ifc_syntax_type_template_parameter &universal);

extern an_ifc_syntax_index get_ifc_argument(
                       const an_ifc_syntax_type_template_parameter &universal);

extern a_boolean has_ifc_constraint(
                       const an_ifc_syntax_type_template_parameter &universal);

extern an_ifc_syntax_index get_ifc_constraint(
                       const an_ifc_syntax_type_template_parameter &universal);

extern a_boolean has_ifc_ellipsis(
                       const an_ifc_syntax_type_template_parameter &universal);

extern an_ifc_source_location get_ifc_ellipsis(
                       const an_ifc_syntax_type_template_parameter &universal);

extern a_boolean has_ifc_locus(
                       const an_ifc_syntax_type_template_parameter &universal);

extern an_ifc_source_location get_ifc_locus(
                       const an_ifc_syntax_type_template_parameter &universal);

extern a_boolean has_ifc_name(
                       const an_ifc_syntax_type_template_parameter &universal);

extern an_ifc_text_offset get_ifc_name(
                       const an_ifc_syntax_type_template_parameter &universal);

extern a_boolean validate(
                       const an_ifc_syntax_type_template_parameter &universal,
                       const an_ifc_validation_trace               *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_type_template_parameter &universal,
                    unsigned                                    indent);

extern void db_node(const an_ifc_syntax_type_template_parameter &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_type_template_parameter_storage*
get<an_ifc_syntax_type_template_parameter_storage>(
                   an_ifc_module                                 *mod,
                   an_ifc_syntax_type_template_parameter_storage *storage,
                   a_boolean                                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_template_parameter_storage>()
/*
Return the corresponding partition kind for SyntaxTypeTemplateParameter.
*/
{
  return ifc_pk_syntax_type_template_parameter;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxTypeTraitIntrinsic nodes.
*/

extern a_boolean has_ifc_arguments(
                          const an_ifc_syntax_type_trait_intrinsic &universal);

extern an_ifc_syntax_index get_ifc_arguments(
                          const an_ifc_syntax_type_trait_intrinsic &universal);

extern a_boolean has_ifc_intrinsic(
                          const an_ifc_syntax_type_trait_intrinsic &universal);

extern an_ifc_operator_category get_ifc_intrinsic(
                          const an_ifc_syntax_type_trait_intrinsic &universal);

extern a_boolean has_ifc_locus(
                          const an_ifc_syntax_type_trait_intrinsic &universal);

extern an_ifc_source_location get_ifc_locus(
                          const an_ifc_syntax_type_trait_intrinsic &universal);

extern a_boolean validate(const an_ifc_syntax_type_trait_intrinsic &universal,
                          const an_ifc_validation_trace            *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_type_trait_intrinsic &universal,
                    unsigned                                 indent);

extern void db_node(const an_ifc_syntax_type_trait_intrinsic &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_type_trait_intrinsic_storage*
get<an_ifc_syntax_type_trait_intrinsic_storage>(
                      an_ifc_module                              *mod,
                      an_ifc_syntax_type_trait_intrinsic_storage *storage,
                      a_boolean                                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_trait_intrinsic_storage>()
/*
Return the corresponding partition kind for SyntaxTypeTraitIntrinsic.
*/
{
  return ifc_pk_syntax_type_trait_intrinsic;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxUnaryFoldExpression nodes.
*/

extern a_boolean has_ifc_direction(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern an_ifc_fold_direction_sort get_ifc_direction(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern a_boolean has_ifc_dyad(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern an_ifc_dyadic_operator_sort get_ifc_dyad(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern a_boolean has_ifc_ellipsis(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern an_ifc_source_location get_ifc_ellipsis(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern a_boolean has_ifc_glyph_locus(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern an_ifc_source_location get_ifc_glyph_locus(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern a_boolean has_ifc_locus(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern a_boolean has_ifc_operand(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern an_ifc_expr_index get_ifc_operand(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern a_boolean has_ifc_right_paren(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern an_ifc_source_location get_ifc_right_paren(
                         const an_ifc_syntax_unary_fold_expression &universal);

extern a_boolean validate(
                         const an_ifc_syntax_unary_fold_expression &universal,
                         const an_ifc_validation_trace             *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_unary_fold_expression &universal,
                    unsigned                                  indent);

extern void db_node(const an_ifc_syntax_unary_fold_expression &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_unary_fold_expression_storage*
get<an_ifc_syntax_unary_fold_expression_storage>(
                     an_ifc_module                               *mod,
                     an_ifc_syntax_unary_fold_expression_storage *storage,
                     a_boolean                                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_unary_fold_expression_storage>()
/*
Return the corresponding partition kind for SyntaxUnaryFoldExpression.
*/
{
  return ifc_pk_syntax_unary_fold_expression;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxUsingDeclaration nodes.
*/

extern a_boolean has_ifc_declarators(
                             const an_ifc_syntax_using_declaration &universal);

extern an_ifc_syntax_index get_ifc_declarators(
                             const an_ifc_syntax_using_declaration &universal);

extern a_boolean has_ifc_keyword(
                             const an_ifc_syntax_using_declaration &universal);

extern an_ifc_source_location get_ifc_keyword(
                             const an_ifc_syntax_using_declaration &universal);

extern a_boolean has_ifc_semicolon(
                             const an_ifc_syntax_using_declaration &universal);

extern an_ifc_source_location get_ifc_semicolon(
                             const an_ifc_syntax_using_declaration &universal);

extern a_boolean validate(const an_ifc_syntax_using_declaration &universal,
                          const an_ifc_validation_trace         *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_using_declaration &universal,
                    unsigned                              indent);

extern void db_node(const an_ifc_syntax_using_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_using_declaration_storage*
get<an_ifc_syntax_using_declaration_storage>(
                         an_ifc_module                           *mod,
                         an_ifc_syntax_using_declaration_storage *storage,
                         a_boolean                               fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_using_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxUsingDeclaration.
*/
{
  return ifc_pk_syntax_using_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxUsingDeclarator nodes.
*/

extern a_boolean has_ifc_comma(
                              const an_ifc_syntax_using_declarator &universal);

extern an_ifc_source_location get_ifc_comma(
                              const an_ifc_syntax_using_declarator &universal);

extern a_boolean has_ifc_expander(
                              const an_ifc_syntax_using_declarator &universal);

extern an_ifc_source_location get_ifc_expander(
                              const an_ifc_syntax_using_declarator &universal);

extern a_boolean has_ifc_qualified_name(
                              const an_ifc_syntax_using_declarator &universal);

extern an_ifc_expr_index get_ifc_qualified_name(
                              const an_ifc_syntax_using_declarator &universal);

extern a_boolean has_ifc_typename_kw(
                              const an_ifc_syntax_using_declarator &universal);

extern an_ifc_source_location get_ifc_typename_kw(
                              const an_ifc_syntax_using_declarator &universal);

extern a_boolean validate(const an_ifc_syntax_using_declarator &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_using_declarator &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_syntax_using_declarator &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_using_declarator_storage*
get<an_ifc_syntax_using_declarator_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_syntax_using_declarator_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_using_declarator_storage>()
/*
Return the corresponding partition kind for SyntaxUsingDeclarator.
*/
{
  return ifc_pk_syntax_using_declarator;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxUsingDirective nodes.
*/

extern a_boolean has_ifc_namespace_kw(
                               const an_ifc_syntax_using_directive &universal);

extern an_ifc_source_location get_ifc_namespace_kw(
                               const an_ifc_syntax_using_directive &universal);

extern a_boolean has_ifc_qualified_name(
                               const an_ifc_syntax_using_directive &universal);

extern an_ifc_expr_index get_ifc_qualified_name(
                               const an_ifc_syntax_using_directive &universal);

extern a_boolean has_ifc_semicolon(
                               const an_ifc_syntax_using_directive &universal);

extern an_ifc_source_location get_ifc_semicolon(
                               const an_ifc_syntax_using_directive &universal);

extern a_boolean has_ifc_using_kw(
                               const an_ifc_syntax_using_directive &universal);

extern an_ifc_source_location get_ifc_using_kw(
                               const an_ifc_syntax_using_directive &universal);

extern a_boolean validate(const an_ifc_syntax_using_directive &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_using_directive &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_syntax_using_directive &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_using_directive_storage*
get<an_ifc_syntax_using_directive_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_syntax_using_directive_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_using_directive_storage>()
/*
Return the corresponding partition kind for SyntaxUsingDirective.
*/
{
  return ifc_pk_syntax_using_directive;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxUsingEnumDeclaration nodes.
*/

extern a_boolean has_ifc_enum_kw(
                        const an_ifc_syntax_using_enum_declaration &universal);

extern an_ifc_source_location get_ifc_enum_kw(
                        const an_ifc_syntax_using_enum_declaration &universal);

extern a_boolean has_ifc_name(
                        const an_ifc_syntax_using_enum_declaration &universal);

extern an_ifc_expr_index get_ifc_name(
                        const an_ifc_syntax_using_enum_declaration &universal);

extern a_boolean has_ifc_semicolon(
                        const an_ifc_syntax_using_enum_declaration &universal);

extern an_ifc_source_location get_ifc_semicolon(
                        const an_ifc_syntax_using_enum_declaration &universal);

extern a_boolean has_ifc_using_kw(
                        const an_ifc_syntax_using_enum_declaration &universal);

extern an_ifc_source_location get_ifc_using_kw(
                        const an_ifc_syntax_using_enum_declaration &universal);

extern a_boolean validate(
                        const an_ifc_syntax_using_enum_declaration &universal,
                        const an_ifc_validation_trace              *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_using_enum_declaration &universal,
                    unsigned                                   indent);

extern void db_node(const an_ifc_syntax_using_enum_declaration &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_using_enum_declaration_storage*
get<an_ifc_syntax_using_enum_declaration_storage>(
                    an_ifc_module                                *mod,
                    an_ifc_syntax_using_enum_declaration_storage *storage,
                    a_boolean                                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_using_enum_declaration_storage>()
/*
Return the corresponding partition kind for SyntaxUsingEnumDeclaration.
*/
{
  return ifc_pk_syntax_using_enum_declaration;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxVirtualSpecifierSeq nodes.
*/

extern a_boolean has_ifc_final_kw(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

extern an_ifc_source_location get_ifc_final_kw(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

extern a_boolean has_ifc_locus(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

extern an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

extern a_boolean has_ifc_override_kw(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

extern an_ifc_source_location get_ifc_override_kw(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

extern a_boolean has_ifc_pure(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

extern an_ifc_bool get_ifc_pure(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

extern a_boolean validate(
                         const an_ifc_syntax_virtual_specifier_seq &universal,
                         const an_ifc_validation_trace             *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_virtual_specifier_seq &universal,
                    unsigned                                  indent);

extern void db_node(const an_ifc_syntax_virtual_specifier_seq &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_virtual_specifier_seq_storage*
get<an_ifc_syntax_virtual_specifier_seq_storage>(
                     an_ifc_module                               *mod,
                     an_ifc_syntax_virtual_specifier_seq_storage *storage,
                     a_boolean                                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_virtual_specifier_seq_storage>()
/*
Return the corresponding partition kind for SyntaxVirtualSpecifierSeq.
*/
{
  return ifc_pk_syntax_virtual_specifier_seq;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC SyntaxWhileStatement nodes.
*/

extern a_boolean has_ifc_body(const an_ifc_syntax_while_statement &universal);

extern an_ifc_syntax_index get_ifc_body(
                               const an_ifc_syntax_while_statement &universal);

extern a_boolean has_ifc_condition(
                               const an_ifc_syntax_while_statement &universal);

extern an_ifc_expr_index get_ifc_condition(
                               const an_ifc_syntax_while_statement &universal);

extern a_boolean has_ifc_pragma(
                               const an_ifc_syntax_while_statement &universal);

extern an_ifc_sentence_index get_ifc_pragma(
                               const an_ifc_syntax_while_statement &universal);

extern a_boolean has_ifc_while(const an_ifc_syntax_while_statement &universal);

extern an_ifc_source_location get_ifc_while(
                               const an_ifc_syntax_while_statement &universal);

extern a_boolean validate(const an_ifc_syntax_while_statement &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_syntax_while_statement &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_syntax_while_statement &universal);
#endif /* DEBUG */

template<>
an_ifc_syntax_while_statement_storage*
get<an_ifc_syntax_while_statement_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_syntax_while_statement_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_while_statement_storage>()
/*
Return the corresponding partition kind for SyntaxWhileStatement.
*/
{
  return ifc_pk_syntax_while_statement;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitAliasTemplate nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_trait_alias_template &universal);

extern an_ifc_decl_index get_ifc_decl(
                                 const an_ifc_trait_alias_template &universal);

extern a_boolean has_ifc_encoded_decl(
                                 const an_ifc_trait_alias_template &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                 const an_ifc_trait_alias_template &universal);

extern a_boolean has_ifc_trait(const an_ifc_trait_alias_template &universal);

extern an_ifc_syntax_index get_ifc_trait(
                                 const an_ifc_trait_alias_template &universal);

extern a_boolean validate(const an_ifc_trait_alias_template &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_alias_template &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_trait_alias_template &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_alias_template_storage* get<an_ifc_trait_alias_template_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_trait_alias_template_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_alias_template_storage>()
/*
Return the corresponding partition kind for TraitAliasTemplate.
*/
{
  return ifc_pk_trait_alias_template;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitAttribute nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_trait_attribute &universal);

extern an_ifc_decl_index get_ifc_decl(const an_ifc_trait_attribute &universal);

extern a_boolean has_ifc_encoded_decl(const an_ifc_trait_attribute &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                      const an_ifc_trait_attribute &universal);

extern a_boolean has_ifc_trait(const an_ifc_trait_attribute &universal);

extern an_ifc_attr_index get_ifc_trait(
                                      const an_ifc_trait_attribute &universal);

extern a_boolean validate(const an_ifc_trait_attribute  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_attribute &universal, unsigned indent);

extern void db_node(const an_ifc_trait_attribute &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_attribute_storage* get<an_ifc_trait_attribute_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_trait_attribute_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_attribute_storage>()
/*
Return the corresponding partition kind for TraitAttribute.
*/
{
  return ifc_pk_trait_attribute;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitDeductionGuide nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_trait_deduction_guide &universal);

extern an_ifc_decl_index get_ifc_decl(
                                const an_ifc_trait_deduction_guide &universal);

extern a_boolean has_ifc_encoded_decl(
                                const an_ifc_trait_deduction_guide &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                const an_ifc_trait_deduction_guide &universal);

extern a_boolean has_ifc_trait(const an_ifc_trait_deduction_guide &universal);

extern an_ifc_decl_index get_ifc_trait(
                                const an_ifc_trait_deduction_guide &universal);

extern a_boolean validate(const an_ifc_trait_deduction_guide &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_deduction_guide &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_trait_deduction_guide &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_deduction_guide_storage*
get<an_ifc_trait_deduction_guide_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_trait_deduction_guide_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_deduction_guide_storage>()
/*
Return the corresponding partition kind for TraitDeductionGuide.
*/
{
  return ifc_pk_trait_deduction_guides;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitDeprecated nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_trait_deprecated &universal);

extern an_ifc_decl_index get_ifc_decl(
                                     const an_ifc_trait_deprecated &universal);

extern a_boolean has_ifc_encoded_decl(
                                     const an_ifc_trait_deprecated &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                     const an_ifc_trait_deprecated &universal);

extern a_boolean has_ifc_trait(const an_ifc_trait_deprecated &universal);

extern an_ifc_text_offset get_ifc_trait(
                                     const an_ifc_trait_deprecated &universal);

extern a_boolean validate(const an_ifc_trait_deprecated &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_deprecated &universal, unsigned indent);

extern void db_node(const an_ifc_trait_deprecated &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_deprecated_storage* get<an_ifc_trait_deprecated_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_trait_deprecated_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_deprecated_storage>()
/*
Return the corresponding partition kind for TraitDeprecated.
*/
{
  return ifc_pk_trait_deprecated;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitFriend nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_trait_friend &universal);

extern an_ifc_decl_index get_ifc_decl(const an_ifc_trait_friend &universal);

extern a_boolean has_ifc_encoded_decl(const an_ifc_trait_friend &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                         const an_ifc_trait_friend &universal);

extern a_boolean has_ifc_trait(const an_ifc_trait_friend &universal);

extern an_ifc_sequence get_ifc_trait(const an_ifc_trait_friend &universal);

extern a_boolean validate(const an_ifc_trait_friend     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_friend &universal, unsigned indent);

extern void db_node(const an_ifc_trait_friend &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_friend_storage* get<an_ifc_trait_friend_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_trait_friend_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_friend_storage>()
/*
Return the corresponding partition kind for TraitFriend.
*/
{
  return ifc_pk_trait_friend;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitFunctionDefinition nodes.
*/

extern a_boolean has_ifc_body(
                            const an_ifc_trait_function_definition &universal);

extern an_ifc_stmt_index get_ifc_body(
                            const an_ifc_trait_function_definition &universal);

extern a_boolean has_ifc_decl(
                            const an_ifc_trait_function_definition &universal);

extern an_ifc_decl_index get_ifc_decl(
                            const an_ifc_trait_function_definition &universal);

extern a_boolean has_ifc_encoded_decl(
                            const an_ifc_trait_function_definition &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                            const an_ifc_trait_function_definition &universal);

extern a_boolean has_ifc_initializers(
                            const an_ifc_trait_function_definition &universal);

extern an_ifc_expr_index get_ifc_initializers(
                            const an_ifc_trait_function_definition &universal);

extern a_boolean has_ifc_parameters(
                            const an_ifc_trait_function_definition &universal);

extern an_ifc_chart_index get_ifc_parameters(
                            const an_ifc_trait_function_definition &universal);

extern a_boolean validate(const an_ifc_trait_function_definition &universal,
                          const an_ifc_validation_trace          *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_function_definition &universal,
                    unsigned                               indent);

extern void db_node(const an_ifc_trait_function_definition &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_function_definition_storage*
get<an_ifc_trait_function_definition_storage>(
                        an_ifc_module                            *mod,
                        an_ifc_trait_function_definition_storage *storage,
                        a_boolean                                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_function_definition_storage>()
/*
Return the corresponding partition kind for TraitFunctionDefinition.
*/
{
  return ifc_pk_trait_mapping_expr;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitMsvcDeclAttrs nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_trait_msvc_decl_attrs &universal);

extern an_ifc_decl_index get_ifc_decl(
                                const an_ifc_trait_msvc_decl_attrs &universal);

extern a_boolean has_ifc_encoded_decl(
                                const an_ifc_trait_msvc_decl_attrs &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                const an_ifc_trait_msvc_decl_attrs &universal);

extern a_boolean has_ifc_trait(const an_ifc_trait_msvc_decl_attrs &universal);

extern an_ifc_attr_index get_ifc_trait(
                                const an_ifc_trait_msvc_decl_attrs &universal);

extern a_boolean validate(const an_ifc_trait_msvc_decl_attrs &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_msvc_decl_attrs &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_trait_msvc_decl_attrs &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_msvc_decl_attrs_storage*
get<an_ifc_trait_msvc_decl_attrs_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_trait_msvc_decl_attrs_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_msvc_decl_attrs_storage>()
/*
Return the corresponding partition kind for TraitMsvcDeclAttrs.
*/
{
  return ifc_pk_msvc_trait_decl_attrs;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitMsvcFuncParams nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_trait_msvc_func_params &universal);

extern an_ifc_decl_index get_ifc_decl(
                               const an_ifc_trait_msvc_func_params &universal);

extern a_boolean has_ifc_encoded_decl(
                               const an_ifc_trait_msvc_func_params &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                               const an_ifc_trait_msvc_func_params &universal);

extern a_boolean has_ifc_params(
                               const an_ifc_trait_msvc_func_params &universal);

extern an_ifc_chart_index get_ifc_params(
                               const an_ifc_trait_msvc_func_params &universal);

extern a_boolean validate(const an_ifc_trait_msvc_func_params &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_msvc_func_params &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_trait_msvc_func_params &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_msvc_func_params_storage*
get<an_ifc_trait_msvc_func_params_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_trait_msvc_func_params_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_msvc_func_params_storage>()
/*
Return the corresponding partition kind for TraitMsvcFuncParams.
*/
{
  return ifc_pk_msvc_trait_named_function_parameters;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitMsvcUuid nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_trait_msvc_uuid &universal);

extern an_ifc_decl_index get_ifc_decl(const an_ifc_trait_msvc_uuid &universal);

extern a_boolean has_ifc_encoded_decl(const an_ifc_trait_msvc_uuid &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                      const an_ifc_trait_msvc_uuid &universal);

extern a_boolean has_ifc_uuid(const an_ifc_trait_msvc_uuid &universal);

extern an_ifc_uuid get_ifc_uuid(const an_ifc_trait_msvc_uuid &universal);

extern a_boolean validate(const an_ifc_trait_msvc_uuid  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_msvc_uuid &universal, unsigned indent);

extern void db_node(const an_ifc_trait_msvc_uuid &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_msvc_uuid_storage* get<an_ifc_trait_msvc_uuid_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_trait_msvc_uuid_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_msvc_uuid_storage>()
/*
Return the corresponding partition kind for TraitMsvcUuid.
*/
{
  return ifc_pk_msvc_trait_uuid;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitMsvcVendorTrait nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_trait_msvc_vendor_trait &universal);

extern an_ifc_decl_index get_ifc_decl(
                              const an_ifc_trait_msvc_vendor_trait &universal);

extern a_boolean has_ifc_encoded_decl(
                              const an_ifc_trait_msvc_vendor_trait &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                              const an_ifc_trait_msvc_vendor_trait &universal);

extern a_boolean has_ifc_trait(
                              const an_ifc_trait_msvc_vendor_trait &universal);

extern an_ifc_msvc_traits_bitfield get_ifc_trait(
                              const an_ifc_trait_msvc_vendor_trait &universal);

extern a_boolean validate(const an_ifc_trait_msvc_vendor_trait &universal,
                          const an_ifc_validation_trace        *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_msvc_vendor_trait &universal,
                    unsigned                             indent);

extern void db_node(const an_ifc_trait_msvc_vendor_trait &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_msvc_vendor_trait_storage*
get<an_ifc_trait_msvc_vendor_trait_storage>(
                          an_ifc_module                          *mod,
                          an_ifc_trait_msvc_vendor_trait_storage *storage,
                          a_boolean                              fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_msvc_vendor_trait_storage>()
/*
Return the corresponding partition kind for TraitMsvcVendorTrait.
*/
{
  return ifc_pk_msvc_trait_vendor_traits;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitRequires nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_trait_requires &universal);

extern an_ifc_decl_index get_ifc_decl(const an_ifc_trait_requires &universal);

extern a_boolean has_ifc_encoded_decl(const an_ifc_trait_requires &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                       const an_ifc_trait_requires &universal);

extern a_boolean has_ifc_trait(const an_ifc_trait_requires &universal);

extern an_ifc_syntax_index get_ifc_trait(
                                       const an_ifc_trait_requires &universal);

extern a_boolean validate(const an_ifc_trait_requires   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_requires &universal, unsigned indent);

extern void db_node(const an_ifc_trait_requires &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_requires_storage* get<an_ifc_trait_requires_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_trait_requires_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_requires_storage>()
/*
Return the corresponding partition kind for TraitRequires.
*/
{
  return ifc_pk_trait_requires;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TraitSpecialization nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_trait_specialization &universal);

extern an_ifc_decl_index get_ifc_decl(
                                 const an_ifc_trait_specialization &universal);

extern a_boolean has_ifc_encoded_decl(
                                 const an_ifc_trait_specialization &universal);

extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                 const an_ifc_trait_specialization &universal);

extern a_boolean has_ifc_trait(const an_ifc_trait_specialization &universal);

extern an_ifc_sequence get_ifc_trait(
                                 const an_ifc_trait_specialization &universal);

extern a_boolean validate(const an_ifc_trait_specialization &universal,
                          const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_trait_specialization &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_trait_specialization &universal);
#endif /* DEBUG */

template<>
an_ifc_trait_specialization_storage* get<an_ifc_trait_specialization_storage>(
                             an_ifc_module                       *mod,
                             an_ifc_trait_specialization_storage *storage,
                             a_boolean                           fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_specialization_storage>()
/*
Return the corresponding partition kind for TraitSpecialization.
*/
{
  return ifc_pk_trait_specialization;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeArray nodes.
*/

extern a_boolean has_ifc_element(const an_ifc_type_array &universal);

extern an_ifc_type_index get_ifc_element(const an_ifc_type_array &universal);

extern a_boolean has_ifc_extent(const an_ifc_type_array &universal);

extern an_ifc_expr_index get_ifc_extent(const an_ifc_type_array &universal);

extern a_boolean validate(const an_ifc_type_array       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_array &universal, unsigned indent);

extern void db_node(const an_ifc_type_array &universal);
#endif /* DEBUG */

template<>
an_ifc_type_array_storage* get<an_ifc_type_array_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_type_array_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_array_storage>()
/*
Return the corresponding partition kind for TypeArray.
*/
{
  return ifc_pk_type_array;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeBase nodes.
*/

extern a_boolean has_ifc_access(const an_ifc_type_base &universal);

extern an_ifc_access_sort get_ifc_access(const an_ifc_type_base &universal);

extern a_boolean has_ifc_pack_expanded(const an_ifc_type_base &universal);

extern an_ifc_bool get_ifc_pack_expanded(const an_ifc_type_base &universal);

extern a_boolean has_ifc_shared(const an_ifc_type_base &universal);

extern an_ifc_bool get_ifc_shared(const an_ifc_type_base &universal);

extern a_boolean has_ifc_type(const an_ifc_type_base &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_type_base &universal);

extern a_boolean validate(const an_ifc_type_base        &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_base &universal, unsigned indent);

extern void db_node(const an_ifc_type_base &universal);
#endif /* DEBUG */

template<>
an_ifc_type_base_storage* get<an_ifc_type_base_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_type_base_storage *storage,
                                        a_boolean                fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_base_storage>()
/*
Return the corresponding partition kind for TypeBase.
*/
{
  return ifc_pk_type_base;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeDecltype nodes.
*/

extern a_boolean has_ifc_expr(const an_ifc_type_decltype &universal);

extern an_ifc_syntax_index get_ifc_expr(const an_ifc_type_decltype &universal);

extern a_boolean validate(const an_ifc_type_decltype    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_decltype &universal, unsigned indent);

extern void db_node(const an_ifc_type_decltype &universal);
#endif /* DEBUG */

template<>
an_ifc_type_decltype_storage* get<an_ifc_type_decltype_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_type_decltype_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_decltype_storage>()
/*
Return the corresponding partition kind for TypeDecltype.
*/
{
  return ifc_pk_type_decltype;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeDesignated nodes.
*/

extern a_boolean has_ifc_decl(const an_ifc_type_designated &universal);

extern an_ifc_decl_index get_ifc_decl(const an_ifc_type_designated &universal);

extern a_boolean validate(const an_ifc_type_designated  &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_designated &universal, unsigned indent);

extern void db_node(const an_ifc_type_designated &universal);
#endif /* DEBUG */

template<>
an_ifc_type_designated_storage* get<an_ifc_type_designated_storage>(
                                  an_ifc_module                  *mod,
                                  an_ifc_type_designated_storage *storage,
                                  a_boolean                      fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_designated_storage>()
/*
Return the corresponding partition kind for TypeDesignated.
*/
{
  return ifc_pk_type_designated;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeExpansion nodes.
*/

extern a_boolean has_ifc_mode(const an_ifc_type_expansion &universal);

extern an_ifc_expansion_mode_sort get_ifc_mode(
                                       const an_ifc_type_expansion &universal);

extern a_boolean has_ifc_pack(const an_ifc_type_expansion &universal);

extern an_ifc_type_index get_ifc_pack(const an_ifc_type_expansion &universal);

extern a_boolean validate(const an_ifc_type_expansion   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_expansion &universal, unsigned indent);

extern void db_node(const an_ifc_type_expansion &universal);
#endif /* DEBUG */

template<>
an_ifc_type_expansion_storage* get<an_ifc_type_expansion_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_type_expansion_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_expansion_storage>()
/*
Return the corresponding partition kind for TypeExpansion.
*/
{
  return ifc_pk_type_expansion;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeForall nodes.
*/

extern a_boolean has_ifc_chart(const an_ifc_type_forall &universal);

extern an_ifc_chart_index get_ifc_chart(const an_ifc_type_forall &universal);

extern a_boolean has_ifc_subject(const an_ifc_type_forall &universal);

extern an_ifc_type_index get_ifc_subject(const an_ifc_type_forall &universal);

extern a_boolean validate(const an_ifc_type_forall      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_forall &universal, unsigned indent);

extern void db_node(const an_ifc_type_forall &universal);
#endif /* DEBUG */

template<>
an_ifc_type_forall_storage* get<an_ifc_type_forall_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_type_forall_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_forall_storage>()
/*
Return the corresponding partition kind for TypeForall.
*/
{
  return ifc_pk_type_forall;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeFunction nodes.
*/

extern a_boolean has_ifc_convention(const an_ifc_type_function &universal);

extern an_ifc_calling_convention_sort get_ifc_convention(
                                        const an_ifc_type_function &universal);

extern a_boolean has_ifc_eh_spec(const an_ifc_type_function &universal);

extern an_ifc_noexcept_specification get_ifc_eh_spec(
                                        const an_ifc_type_function &universal);

extern a_boolean has_ifc_source(const an_ifc_type_function &universal);

extern an_ifc_type_index get_ifc_source(const an_ifc_type_function &universal);

extern a_boolean has_ifc_target(const an_ifc_type_function &universal);

extern an_ifc_type_index get_ifc_target(const an_ifc_type_function &universal);

extern a_boolean has_ifc_traits(const an_ifc_type_function &universal);

extern an_ifc_function_type_traits_bitfield get_ifc_traits(
                                        const an_ifc_type_function &universal);

extern a_boolean validate(const an_ifc_type_function    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_function &universal, unsigned indent);

extern void db_node(const an_ifc_type_function &universal);
#endif /* DEBUG */

template<>
an_ifc_type_function_storage* get<an_ifc_type_function_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_type_function_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_function_storage>()
/*
Return the corresponding partition kind for TypeFunction.
*/
{
  return ifc_pk_type_function;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeFundamental nodes.
*/

extern a_boolean has_ifc_basis(const an_ifc_type_fundamental &universal);

extern an_ifc_type_basis_sort get_ifc_basis(
                                     const an_ifc_type_fundamental &universal);

extern a_boolean has_ifc_precision(const an_ifc_type_fundamental &universal);

extern an_ifc_type_precision_sort get_ifc_precision(
                                     const an_ifc_type_fundamental &universal);

extern a_boolean has_ifc_sign(const an_ifc_type_fundamental &universal);

extern an_ifc_type_sign_sort get_ifc_sign(
                                     const an_ifc_type_fundamental &universal);

extern a_boolean validate(const an_ifc_type_fundamental &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_fundamental &universal, unsigned indent);

extern void db_node(const an_ifc_type_fundamental &universal);
#endif /* DEBUG */

template<>
an_ifc_type_fundamental_storage* get<an_ifc_type_fundamental_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_type_fundamental_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_fundamental_storage>()
/*
Return the corresponding partition kind for TypeFundamental.
*/
{
  return ifc_pk_type_fundamental;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeLvalueReference nodes.
*/

extern a_boolean has_ifc_referee(
                                const an_ifc_type_lvalue_reference &universal);

extern an_ifc_type_index get_ifc_referee(
                                const an_ifc_type_lvalue_reference &universal);

extern a_boolean validate(const an_ifc_type_lvalue_reference &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_type_lvalue_reference &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_type_lvalue_reference &universal);
#endif /* DEBUG */

template<>
an_ifc_type_lvalue_reference_storage*
get<an_ifc_type_lvalue_reference_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_type_lvalue_reference_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_lvalue_reference_storage>()
/*
Return the corresponding partition kind for TypeLvalueReference.
*/
{
  return ifc_pk_type_lvalue_reference;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeMethod nodes.
*/

extern a_boolean has_ifc_convention(const an_ifc_type_method &universal);

extern an_ifc_calling_convention_sort get_ifc_convention(
                                          const an_ifc_type_method &universal);

extern a_boolean has_ifc_eh_spec(const an_ifc_type_method &universal);

extern an_ifc_noexcept_specification get_ifc_eh_spec(
                                          const an_ifc_type_method &universal);

extern a_boolean has_ifc_scope(const an_ifc_type_method &universal);

extern an_ifc_type_index get_ifc_scope(const an_ifc_type_method &universal);

extern a_boolean has_ifc_source(const an_ifc_type_method &universal);

extern an_ifc_type_index get_ifc_source(const an_ifc_type_method &universal);

extern a_boolean has_ifc_target(const an_ifc_type_method &universal);

extern an_ifc_type_index get_ifc_target(const an_ifc_type_method &universal);

extern a_boolean has_ifc_traits(const an_ifc_type_method &universal);

extern an_ifc_function_type_traits_bitfield get_ifc_traits(
                                          const an_ifc_type_method &universal);

extern a_boolean validate(const an_ifc_type_method      &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_method &universal, unsigned indent);

extern void db_node(const an_ifc_type_method &universal);
#endif /* DEBUG */

template<>
an_ifc_type_method_storage* get<an_ifc_type_method_storage>(
                                      an_ifc_module              *mod,
                                      an_ifc_type_method_storage *storage,
                                      a_boolean                  fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_method_storage>()
/*
Return the corresponding partition kind for TypeMethod.
*/
{
  return ifc_pk_type_nonstatic_member_function;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypePlaceholder nodes.
*/

extern a_boolean has_ifc_basis(const an_ifc_type_placeholder &universal);

extern an_ifc_type_basis_sort get_ifc_basis(
                                     const an_ifc_type_placeholder &universal);

extern a_boolean has_ifc_constraint(const an_ifc_type_placeholder &universal);

extern an_ifc_expr_index get_ifc_constraint(
                                     const an_ifc_type_placeholder &universal);

extern a_boolean has_ifc_elaboration(const an_ifc_type_placeholder &universal);

extern an_ifc_type_index get_ifc_elaboration(
                                     const an_ifc_type_placeholder &universal);

extern a_boolean validate(const an_ifc_type_placeholder &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_placeholder &universal, unsigned indent);

extern void db_node(const an_ifc_type_placeholder &universal);
#endif /* DEBUG */

template<>
an_ifc_type_placeholder_storage* get<an_ifc_type_placeholder_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_type_placeholder_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_placeholder_storage>()
/*
Return the corresponding partition kind for TypePlaceholder.
*/
{
  return ifc_pk_type_placeholder;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypePointer nodes.
*/

extern a_boolean has_ifc_pointee(const an_ifc_type_pointer &universal);

extern an_ifc_type_index get_ifc_pointee(const an_ifc_type_pointer &universal);

extern a_boolean validate(const an_ifc_type_pointer     &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_pointer &universal, unsigned indent);

extern void db_node(const an_ifc_type_pointer &universal);
#endif /* DEBUG */

template<>
an_ifc_type_pointer_storage* get<an_ifc_type_pointer_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_type_pointer_storage *storage,
                                     a_boolean                   fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_pointer_storage>()
/*
Return the corresponding partition kind for TypePointer.
*/
{
  return ifc_pk_type_pointer;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypePointerToMember nodes.
*/

extern a_boolean has_ifc_member(
                               const an_ifc_type_pointer_to_member &universal);

extern an_ifc_type_index get_ifc_member(
                               const an_ifc_type_pointer_to_member &universal);

extern a_boolean has_ifc_scope(const an_ifc_type_pointer_to_member &universal);

extern an_ifc_type_index get_ifc_scope(
                               const an_ifc_type_pointer_to_member &universal);

extern a_boolean validate(const an_ifc_type_pointer_to_member &universal,
                          const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_type_pointer_to_member &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_type_pointer_to_member &universal);
#endif /* DEBUG */

template<>
an_ifc_type_pointer_to_member_storage*
get<an_ifc_type_pointer_to_member_storage>(
                           an_ifc_module                         *mod,
                           an_ifc_type_pointer_to_member_storage *storage,
                           a_boolean                             fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_pointer_to_member_storage>()
/*
Return the corresponding partition kind for TypePointerToMember.
*/
{
  return ifc_pk_type_pointer_to_member;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeQualified nodes.
*/

extern a_boolean has_ifc_qualifiers(const an_ifc_type_qualified &universal);

extern an_ifc_qualifier_bitfield get_ifc_qualifiers(
                                       const an_ifc_type_qualified &universal);

extern a_boolean has_ifc_unqualified(const an_ifc_type_qualified &universal);

extern an_ifc_type_index get_ifc_unqualified(
                                       const an_ifc_type_qualified &universal);

extern a_boolean validate(const an_ifc_type_qualified   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_qualified &universal, unsigned indent);

extern void db_node(const an_ifc_type_qualified &universal);
#endif /* DEBUG */

template<>
an_ifc_type_qualified_storage* get<an_ifc_type_qualified_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_type_qualified_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_qualified_storage>()
/*
Return the corresponding partition kind for TypeQualified.
*/
{
  return ifc_pk_type_qualified;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeRvalueReference nodes.
*/

extern a_boolean has_ifc_referee(
                                const an_ifc_type_rvalue_reference &universal);

extern an_ifc_type_index get_ifc_referee(
                                const an_ifc_type_rvalue_reference &universal);

extern a_boolean validate(const an_ifc_type_rvalue_reference &universal,
                          const an_ifc_validation_trace      *parent);

#if DEBUG
extern void db_node(const an_ifc_type_rvalue_reference &universal,
                    unsigned                           indent);

extern void db_node(const an_ifc_type_rvalue_reference &universal);
#endif /* DEBUG */

template<>
an_ifc_type_rvalue_reference_storage*
get<an_ifc_type_rvalue_reference_storage>(
                            an_ifc_module                        *mod,
                            an_ifc_type_rvalue_reference_storage *storage,
                            a_boolean                            fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_rvalue_reference_storage>()
/*
Return the corresponding partition kind for TypeRvalueReference.
*/
{
  return ifc_pk_type_rvalue_reference;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeSyntactic nodes.
*/

extern a_boolean has_ifc_expr(const an_ifc_type_syntactic &universal);

extern an_ifc_expr_index get_ifc_expr(const an_ifc_type_syntactic &universal);

extern a_boolean validate(const an_ifc_type_syntactic   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_syntactic &universal, unsigned indent);

extern void db_node(const an_ifc_type_syntactic &universal);
#endif /* DEBUG */

template<>
an_ifc_type_syntactic_storage* get<an_ifc_type_syntactic_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_type_syntactic_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_syntactic_storage>()
/*
Return the corresponding partition kind for TypeSyntactic.
*/
{
  return ifc_pk_type_syntactic;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeSyntaxTree nodes.
*/

extern a_boolean has_ifc_syntax(const an_ifc_type_syntax_tree &universal);

extern an_ifc_syntax_index get_ifc_syntax(
                                     const an_ifc_type_syntax_tree &universal);

extern a_boolean validate(const an_ifc_type_syntax_tree &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_syntax_tree &universal, unsigned indent);

extern void db_node(const an_ifc_type_syntax_tree &universal);
#endif /* DEBUG */

template<>
an_ifc_type_syntax_tree_storage* get<an_ifc_type_syntax_tree_storage>(
                                 an_ifc_module                   *mod,
                                 an_ifc_type_syntax_tree_storage *storage,
                                 a_boolean                       fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_syntax_tree_storage>()
/*
Return the corresponding partition kind for TypeSyntaxTree.
*/
{
  return ifc_pk_type_syntax_tree;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeTor nodes.
*/

extern a_boolean has_ifc_convention(const an_ifc_type_tor &universal);

extern an_ifc_calling_convention_sort get_ifc_convention(
                                             const an_ifc_type_tor &universal);

extern a_boolean has_ifc_eh_spec(const an_ifc_type_tor &universal);

extern an_ifc_noexcept_specification get_ifc_eh_spec(
                                             const an_ifc_type_tor &universal);

extern a_boolean has_ifc_source(const an_ifc_type_tor &universal);

extern an_ifc_type_index get_ifc_source(const an_ifc_type_tor &universal);

extern a_boolean validate(const an_ifc_type_tor         &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_tor &universal, unsigned indent);

extern void db_node(const an_ifc_type_tor &universal);
#endif /* DEBUG */

template<>
an_ifc_type_tor_storage* get<an_ifc_type_tor_storage>(
                                         an_ifc_module           *mod,
                                         an_ifc_type_tor_storage *storage,
                                         a_boolean               fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_tor_storage>()
/*
Return the corresponding partition kind for TypeTor.
*/
{
  return ifc_pk_type_tor;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeTuple nodes.
*/

extern a_boolean has_ifc_cardinality(const an_ifc_type_tuple &universal);

extern an_ifc_cardinality get_ifc_cardinality(
                                           const an_ifc_type_tuple &universal);

extern a_boolean has_ifc_start(const an_ifc_type_tuple &universal);

extern an_ifc_index get_ifc_start(const an_ifc_type_tuple &universal);

extern a_boolean validate(const an_ifc_type_tuple       &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_tuple &universal, unsigned indent);

extern void db_node(const an_ifc_type_tuple &universal);
#endif /* DEBUG */

template<>
an_ifc_type_tuple_storage* get<an_ifc_type_tuple_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_type_tuple_storage *storage,
                                       a_boolean                 fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_tuple_storage>()
/*
Return the corresponding partition kind for TypeTuple.
*/
{
  return ifc_pk_type_tuple;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeTypename nodes.
*/

extern a_boolean has_ifc_path(const an_ifc_type_typename &universal);

extern an_ifc_expr_index get_ifc_path(const an_ifc_type_typename &universal);

extern a_boolean validate(const an_ifc_type_typename    &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_typename &universal, unsigned indent);

extern void db_node(const an_ifc_type_typename &universal);
#endif /* DEBUG */

template<>
an_ifc_type_typename_storage* get<an_ifc_type_typename_storage>(
                                    an_ifc_module                *mod,
                                    an_ifc_type_typename_storage *storage,
                                    a_boolean                    fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_typename_storage>()
/*
Return the corresponding partition kind for TypeTypename.
*/
{
  return ifc_pk_type_typename;
}  /* get_ifc_partition_kind */

/*
Functions for interacting with IFC TypeUnaligned nodes.
*/

extern a_boolean has_ifc_type(const an_ifc_type_unaligned &universal);

extern an_ifc_type_index get_ifc_type(const an_ifc_type_unaligned &universal);

extern a_boolean validate(const an_ifc_type_unaligned   &universal,
                          const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_type_unaligned &universal, unsigned indent);

extern void db_node(const an_ifc_type_unaligned &universal);
#endif /* DEBUG */

template<>
an_ifc_type_unaligned_storage* get<an_ifc_type_unaligned_storage>(
                                   an_ifc_module                 *mod,
                                   an_ifc_type_unaligned_storage *storage,
                                   a_boolean                     fill_storage);

template<>
constexpr an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_type_unaligned_storage>()
/*
Return the corresponding partition kind for TypeUnaligned.
*/
{
  return ifc_pk_type_unaligned;
}  /* get_ifc_partition_kind */

extern an_ifc_entity_size_storage get_ifc_partition_element_size(
                                                   an_ifc_module         *mod,
                                                   an_ifc_partition_kind kind);

/*
Functions for converting IFC partition kinds to various IFC sorts.
*/

extern an_ifc_attr_sort to_attr_sort(an_ifc_partition_kind kind);

extern an_ifc_chart_sort to_chart_sort(an_ifc_partition_kind kind);

extern an_ifc_decl_sort to_decl_sort(an_ifc_partition_kind kind);

extern an_ifc_expr_sort to_expr_sort(an_ifc_partition_kind kind);

extern an_ifc_form_sort to_form_sort(an_ifc_partition_kind kind);

extern an_ifc_macro_sort to_macro_sort(an_ifc_partition_kind kind);

extern an_ifc_name_sort to_name_sort(an_ifc_partition_kind kind);

extern an_ifc_stmt_sort to_stmt_sort(an_ifc_partition_kind kind);

extern an_ifc_syntax_sort to_syntax_sort(an_ifc_partition_kind kind);

extern an_ifc_type_sort to_type_sort(an_ifc_partition_kind kind);

/*
Functions for converting various IFC sorts to IFC partition kinds.
*/

extern a_boolean has_partition_kind(an_ifc_attr_sort sort);

extern an_ifc_partition_kind to_partition_kind(an_ifc_attr_sort sort);

extern a_boolean has_partition_kind(an_ifc_chart_sort sort);

extern an_ifc_partition_kind to_partition_kind(an_ifc_chart_sort sort);

extern a_boolean has_partition_kind(an_ifc_decl_sort sort);

extern an_ifc_partition_kind to_partition_kind(an_ifc_decl_sort sort);

extern a_boolean has_partition_kind(an_ifc_expr_sort sort);

extern an_ifc_partition_kind to_partition_kind(an_ifc_expr_sort sort);

extern a_boolean has_partition_kind(an_ifc_form_sort sort);

extern an_ifc_partition_kind to_partition_kind(an_ifc_form_sort sort);

extern a_boolean has_partition_kind(an_ifc_macro_sort sort);

extern an_ifc_partition_kind to_partition_kind(an_ifc_macro_sort sort);

extern a_boolean has_partition_kind(an_ifc_name_sort sort);

extern an_ifc_partition_kind to_partition_kind(an_ifc_name_sort sort);

extern a_boolean has_partition_kind(an_ifc_stmt_sort sort);

extern an_ifc_partition_kind to_partition_kind(an_ifc_stmt_sort sort);

extern a_boolean has_partition_kind(an_ifc_syntax_sort sort);

extern an_ifc_partition_kind to_partition_kind(an_ifc_syntax_sort sort);

extern a_boolean has_partition_kind(an_ifc_type_sort sort);

extern an_ifc_partition_kind to_partition_kind(an_ifc_type_sort sort);

/*
Visitor functions for validating the nodes at a given index.
*/

extern a_boolean validate(an_ifc_decl_index idx);

/*
Visitor functions for retrieving access values from nodes on the DeclIndex.
*/

extern a_boolean has_ifc_access(an_ifc_decl_index idx);

extern an_ifc_access_sort get_ifc_access(an_ifc_decl_index idx);

/*
Visitor functions for retrieving home_scope values from nodes on the DeclIndex.
*/

extern a_boolean has_ifc_home_scope(an_ifc_decl_index idx);

extern an_ifc_decl_index get_ifc_home_scope(an_ifc_decl_index idx);

/*
Visitor functions for retrieving locus values from nodes on the DeclIndex.
*/

extern a_boolean has_ifc_locus(an_ifc_decl_index idx);

extern an_ifc_source_location get_ifc_locus(an_ifc_decl_index idx);

/*
Visitor functions for retrieving name values from nodes on the DeclIndex.
*/

extern a_boolean has_ifc_name(an_ifc_decl_index idx);

extern an_ifc_name_index get_ifc_name(an_ifc_decl_index idx);


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2022 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
