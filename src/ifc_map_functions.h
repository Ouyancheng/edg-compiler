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

extern a_boolean is_known_sort(an_ifc_access_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_access_sort_0_33       versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_access_sort to_universal_sort(an_ifc_access_sort_0_33 versioned);

/*
Functions for interacting with IFC ArchitectureSort sorts.
*/

extern a_const_char* str_for(an_ifc_architecture_sort universal);

extern an_ifc_encoded_architecture_sort to_encoded(
                                           an_ifc_module            *mod,
                                           an_ifc_architecture_sort universal);

extern a_boolean is_known_sort(an_ifc_architecture_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_architecture_sort_0_33 versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_architecture_sort to_universal_sort(
                                      an_ifc_architecture_sort_0_33 versioned);

/*
Functions for interacting with IFC AttrSort sorts.
*/

extern a_const_char* str_for(an_ifc_attr_sort universal);

extern an_ifc_encoded_attr_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_attr_sort universal);

extern a_boolean is_known_sort(an_ifc_attr_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_attr_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_attr_sort to_universal_sort(an_ifc_attr_sort_0_33 versioned);

/*
Functions for interacting with IFC CallingConventionSort sorts.
*/

extern a_const_char* str_for(an_ifc_calling_convention_sort universal);

extern an_ifc_encoded_calling_convention_sort to_encoded(
                                     an_ifc_module                  *mod,
                                     an_ifc_calling_convention_sort universal);

extern a_boolean is_known_sort(an_ifc_calling_convention_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                       *mod,
                               an_ifc_calling_convention_sort_0_33 versioned,
                               const an_ifc_validation_trace       *parent);

extern an_ifc_calling_convention_sort to_universal_sort(
                                an_ifc_calling_convention_sort_0_33 versioned);

/*
Functions for interacting with IFC ChartSort sorts.
*/

extern a_const_char* str_for(an_ifc_chart_sort universal);

extern an_ifc_encoded_chart_sort to_encoded(an_ifc_module     *mod,
                                            an_ifc_chart_sort universal);

extern a_boolean is_known_sort(an_ifc_chart_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_chart_sort_0_33        versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_chart_sort to_universal_sort(an_ifc_chart_sort_0_33 versioned);

/*
Functions for interacting with IFC DeclSort sorts.
*/

extern a_const_char* str_for(an_ifc_decl_sort universal);

extern an_ifc_encoded_decl_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_decl_sort universal);

extern a_boolean is_known_sort(an_ifc_decl_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_decl_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_decl_sort to_universal_sort(an_ifc_decl_sort_0_33 versioned);

extern a_boolean is_known_sort(an_ifc_decl_sort_0_41 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_decl_sort_0_41         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_decl_sort to_universal_sort(an_ifc_decl_sort_0_41 versioned);

/*
Functions for interacting with IFC DelimiterSort sorts.
*/

extern a_const_char* str_for(an_ifc_delimiter_sort universal);

extern an_ifc_encoded_delimiter_sort to_encoded(
                                              an_ifc_module         *mod,
                                              an_ifc_delimiter_sort universal);

extern a_boolean is_known_sort(an_ifc_delimiter_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_delimiter_sort_0_33    versioned,
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

extern a_boolean is_known_sort(an_ifc_dyadic_operator_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                    *mod,
                               an_ifc_dyadic_operator_sort_0_33 versioned,
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

extern a_boolean is_known_sort(an_ifc_expansion_mode_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                   *mod,
                               an_ifc_expansion_mode_sort_0_33 versioned,
                               const an_ifc_validation_trace   *parent);

extern an_ifc_expansion_mode_sort to_universal_sort(
                                    an_ifc_expansion_mode_sort_0_33 versioned);

/*
Functions for interacting with IFC ExprSort sorts.
*/

extern a_const_char* str_for(an_ifc_expr_sort universal);

extern an_ifc_encoded_expr_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_expr_sort universal);

extern a_boolean is_known_sort(an_ifc_expr_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_expr_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_expr_sort to_universal_sort(an_ifc_expr_sort_0_33 versioned);

extern a_boolean is_known_sort(an_ifc_expr_sort_0_42 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_expr_sort_0_42         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_expr_sort to_universal_sort(an_ifc_expr_sort_0_42 versioned);

/*
Functions for interacting with IFC FoldDirectionSort sorts.
*/

extern a_const_char* str_for(an_ifc_fold_direction_sort universal);

extern an_ifc_encoded_fold_direction_sort to_encoded(
                                         an_ifc_module              *mod,
                                         an_ifc_fold_direction_sort universal);

extern a_boolean is_known_sort(an_ifc_fold_direction_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                   *mod,
                               an_ifc_fold_direction_sort_0_33 versioned,
                               const an_ifc_validation_trace   *parent);

extern an_ifc_fold_direction_sort to_universal_sort(
                                    an_ifc_fold_direction_sort_0_33 versioned);

/*
Functions for interacting with IFC FormSort sorts.
*/

extern a_const_char* str_for(an_ifc_form_sort universal);

extern an_ifc_encoded_form_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_form_sort universal);

extern a_boolean is_known_sort(an_ifc_form_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_form_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_form_sort to_universal_sort(an_ifc_form_sort_0_33 versioned);

/*
Functions for interacting with IFC InitializerSort sorts.
*/

extern a_const_char* str_for(an_ifc_initializer_sort universal);

extern an_ifc_encoded_initializer_sort to_encoded(
                                            an_ifc_module           *mod,
                                            an_ifc_initializer_sort universal);

extern a_boolean is_known_sort(an_ifc_initializer_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_initializer_sort_0_33  versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_initializer_sort to_universal_sort(
                                       an_ifc_initializer_sort_0_33 versioned);

/*
Functions for interacting with IFC KeywordSort sorts.
*/

extern a_const_char* str_for(an_ifc_keyword_sort universal);

extern an_ifc_encoded_keyword_sort to_encoded(an_ifc_module       *mod,
                                              an_ifc_keyword_sort universal);

extern a_boolean is_known_sort(an_ifc_keyword_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_keyword_sort_0_33      versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_keyword_sort to_universal_sort(
                                           an_ifc_keyword_sort_0_33 versioned);

/*
Functions for interacting with IFC LabelSort sorts.
*/

extern a_const_char* str_for(an_ifc_label_sort universal);

extern an_ifc_encoded_label_sort to_encoded(an_ifc_module     *mod,
                                            an_ifc_label_sort universal);

extern a_boolean is_known_sort(an_ifc_label_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_label_sort_0_33        versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_label_sort to_universal_sort(an_ifc_label_sort_0_33 versioned);

/*
Functions for interacting with IFC LitSort sorts.
*/

extern a_const_char* str_for(an_ifc_lit_sort universal);

extern an_ifc_encoded_lit_sort to_encoded(an_ifc_module   *mod,
                                          an_ifc_lit_sort universal);

extern a_boolean is_known_sort(an_ifc_lit_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_lit_sort_0_33          versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_lit_sort to_universal_sort(an_ifc_lit_sort_0_33 versioned);

/*
Functions for interacting with IFC MacroSort sorts.
*/

extern a_const_char* str_for(an_ifc_macro_sort universal);

extern an_ifc_encoded_macro_sort to_encoded(an_ifc_module     *mod,
                                            an_ifc_macro_sort universal);

extern a_boolean is_known_sort(an_ifc_macro_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_macro_sort_0_33        versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_macro_sort to_universal_sort(an_ifc_macro_sort_0_33 versioned);

/*
Functions for interacting with IFC MonadicOperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_monadic_operator_sort universal);

extern an_ifc_encoded_monadic_operator_sort to_encoded(
                                       an_ifc_module                *mod,
                                       an_ifc_monadic_operator_sort universal);

extern a_boolean is_known_sort(an_ifc_monadic_operator_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                     *mod,
                               an_ifc_monadic_operator_sort_0_33 versioned,
                               const an_ifc_validation_trace     *parent);

extern an_ifc_monadic_operator_sort to_universal_sort(
                                  an_ifc_monadic_operator_sort_0_33 versioned);

/*
Functions for interacting with IFC NameSort sorts.
*/

extern a_const_char* str_for(an_ifc_name_sort universal);

extern an_ifc_encoded_name_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_name_sort universal);

extern a_boolean is_known_sort(an_ifc_name_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_name_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_name_sort to_universal_sort(an_ifc_name_sort_0_33 versioned);

/*
Functions for interacting with IFC NiladicOperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_niladic_operator_sort universal);

extern an_ifc_encoded_niladic_operator_sort to_encoded(
                                       an_ifc_module                *mod,
                                       an_ifc_niladic_operator_sort universal);

extern a_boolean is_known_sort(an_ifc_niladic_operator_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                     *mod,
                               an_ifc_niladic_operator_sort_0_33 versioned,
                               const an_ifc_validation_trace     *parent);

extern an_ifc_niladic_operator_sort to_universal_sort(
                                  an_ifc_niladic_operator_sort_0_33 versioned);

/*
Functions for interacting with IFC NoexceptSort sorts.
*/

extern a_const_char* str_for(an_ifc_noexcept_sort universal);

extern an_ifc_encoded_noexcept_sort to_encoded(an_ifc_module        *mod,
                                               an_ifc_noexcept_sort universal);

extern a_boolean is_known_sort(an_ifc_noexcept_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_noexcept_sort_0_33     versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_noexcept_sort to_universal_sort(
                                          an_ifc_noexcept_sort_0_33 versioned);

/*
Functions for interacting with IFC OperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_operator_sort universal);

extern an_ifc_encoded_operator_sort to_encoded(an_ifc_module        *mod,
                                               an_ifc_operator_sort universal);

extern a_boolean is_known_sort(an_ifc_operator_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_operator_sort_0_33     versioned,
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

extern a_boolean is_known_sort(an_ifc_parameter_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_parameter_sort_0_33    versioned,
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

extern a_boolean is_known_sort(an_ifc_pointer_declarator_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                       *mod,
                               an_ifc_pointer_declarator_sort_0_33 versioned,
                               const an_ifc_validation_trace       *parent);

extern an_ifc_pointer_declarator_sort to_universal_sort(
                                an_ifc_pointer_declarator_sort_0_33 versioned);

/*
Functions for interacting with IFC PragmaSort sorts.
*/

extern a_const_char* str_for(an_ifc_pragma_sort universal);

extern an_ifc_encoded_pragma_sort to_encoded(an_ifc_module      *mod,
                                             an_ifc_pragma_sort universal);

extern a_boolean is_known_sort(an_ifc_pragma_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_pragma_sort_0_33       versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_pragma_sort to_universal_sort(an_ifc_pragma_sort_0_33 versioned);

/*
Functions for interacting with IFC ReadConversionSort sorts.
*/

extern a_const_char* str_for(an_ifc_read_conversion_sort universal);

extern an_ifc_encoded_read_conversion_sort to_encoded(
                                        an_ifc_module               *mod,
                                        an_ifc_read_conversion_sort universal);

extern a_boolean is_known_sort(an_ifc_read_conversion_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                    *mod,
                               an_ifc_read_conversion_sort_0_33 versioned,
                               const an_ifc_validation_trace    *parent);

extern an_ifc_read_conversion_sort to_universal_sort(
                                   an_ifc_read_conversion_sort_0_33 versioned);

/*
Functions for interacting with IFC ReturnSort sorts.
*/

extern a_const_char* str_for(an_ifc_return_sort universal);

extern an_ifc_encoded_return_sort to_encoded(an_ifc_module      *mod,
                                             an_ifc_return_sort universal);

extern a_boolean is_known_sort(an_ifc_return_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_return_sort_0_33       versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_return_sort to_universal_sort(an_ifc_return_sort_0_33 versioned);

/*
Functions for interacting with IFC SourceDirectiveSort sorts.
*/

extern a_const_char* str_for(an_ifc_source_directive_sort universal);

extern an_ifc_encoded_source_directive_sort to_encoded(
                                       an_ifc_module                *mod,
                                       an_ifc_source_directive_sort universal);

extern a_boolean is_known_sort(an_ifc_source_directive_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                     *mod,
                               an_ifc_source_directive_sort_0_33 versioned,
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

extern a_boolean is_known_sort(an_ifc_source_identifier_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                      *mod,
                               an_ifc_source_identifier_sort_0_33 versioned,
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

extern a_boolean is_known_sort(an_ifc_source_keyword_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                   *mod,
                               an_ifc_source_keyword_sort_0_33 versioned,
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

extern a_boolean is_known_sort(an_ifc_source_literal_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                   *mod,
                               an_ifc_source_literal_sort_0_33 versioned,
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

extern a_boolean is_known_sort(an_ifc_source_operator_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                    *mod,
                               an_ifc_source_operator_sort_0_33 versioned,
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

extern a_boolean is_known_sort(an_ifc_source_punctuator_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                      *mod,
                               an_ifc_source_punctuator_sort_0_33 versioned,
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

extern a_boolean is_known_sort(an_ifc_specialization_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                   *mod,
                               an_ifc_specialization_sort_0_33 versioned,
                               const an_ifc_validation_trace   *parent);

extern an_ifc_specialization_sort to_universal_sort(
                                    an_ifc_specialization_sort_0_33 versioned);

/*
Functions for interacting with IFC StmtSort sorts.
*/

extern a_const_char* str_for(an_ifc_stmt_sort universal);

extern an_ifc_encoded_stmt_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_stmt_sort universal);

extern a_boolean is_known_sort(an_ifc_stmt_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_stmt_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_stmt_sort to_universal_sort(an_ifc_stmt_sort_0_33 versioned);

extern a_boolean is_known_sort(an_ifc_stmt_sort_0_42 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_stmt_sort_0_42         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_stmt_sort to_universal_sort(an_ifc_stmt_sort_0_42 versioned);

/*
Functions for interacting with IFC StorageInstructionOperatorSort sorts.
*/

extern a_const_char* str_for(
                           an_ifc_storage_instruction_operator_sort universal);

extern an_ifc_encoded_storage_instruction_operator_sort to_encoded(
                           an_ifc_module                            *mod,
                           an_ifc_storage_instruction_operator_sort universal);

extern a_boolean is_known_sort(
                      an_ifc_storage_instruction_operator_sort_0_33 versioned);

extern a_boolean validate_sort(
                      an_ifc_module                                 *mod,
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

extern a_boolean is_known_sort(an_ifc_string_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_string_sort_0_33       versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_string_sort to_universal_sort(an_ifc_string_sort_0_33 versioned);

/*
Functions for interacting with IFC SyntaxSort sorts.
*/

extern a_const_char* str_for(an_ifc_syntax_sort universal);

extern an_ifc_encoded_syntax_sort to_encoded(an_ifc_module      *mod,
                                             an_ifc_syntax_sort universal);

extern a_boolean is_known_sort(an_ifc_syntax_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_syntax_sort_0_33       versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_syntax_sort to_universal_sort(an_ifc_syntax_sort_0_33 versioned);

/*
Functions for interacting with IFC TriadicOperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_triadic_operator_sort universal);

extern an_ifc_encoded_triadic_operator_sort to_encoded(
                                       an_ifc_module                *mod,
                                       an_ifc_triadic_operator_sort universal);

extern a_boolean is_known_sort(an_ifc_triadic_operator_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                     *mod,
                               an_ifc_triadic_operator_sort_0_33 versioned,
                               const an_ifc_validation_trace     *parent);

extern an_ifc_triadic_operator_sort to_universal_sort(
                                  an_ifc_triadic_operator_sort_0_33 versioned);

extern a_boolean is_known_sort(an_ifc_triadic_operator_sort_0_42 versioned);

extern a_boolean validate_sort(an_ifc_module                     *mod,
                               an_ifc_triadic_operator_sort_0_42 versioned,
                               const an_ifc_validation_trace     *parent);

extern an_ifc_triadic_operator_sort to_universal_sort(
                                  an_ifc_triadic_operator_sort_0_42 versioned);

/*
Functions for interacting with IFC TypeBasisSort sorts.
*/

extern a_const_char* str_for(an_ifc_type_basis_sort universal);

extern an_ifc_encoded_type_basis_sort to_encoded(
                                             an_ifc_module          *mod,
                                             an_ifc_type_basis_sort universal);

extern a_boolean is_known_sort(an_ifc_type_basis_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_type_basis_sort_0_33   versioned,
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

extern a_boolean is_known_sort(an_ifc_type_precision_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                   *mod,
                               an_ifc_type_precision_sort_0_33 versioned,
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

extern a_boolean is_known_sort(an_ifc_type_sign_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_type_sign_sort_0_33    versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_type_sign_sort to_universal_sort(
                                         an_ifc_type_sign_sort_0_33 versioned);

/*
Functions for interacting with IFC TypeSort sorts.
*/

extern a_const_char* str_for(an_ifc_type_sort universal);

extern an_ifc_encoded_type_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_type_sort universal);

extern a_boolean is_known_sort(an_ifc_type_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_type_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_type_sort to_universal_sort(an_ifc_type_sort_0_33 versioned);

/*
Functions for interacting with IFC UnitSort sorts.
*/

extern a_const_char* str_for(an_ifc_unit_sort universal);

extern an_ifc_encoded_unit_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_unit_sort universal);

extern a_boolean is_known_sort(an_ifc_unit_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_unit_sort_0_33         versioned,
                               const an_ifc_validation_trace *parent);

extern an_ifc_unit_sort to_universal_sort(an_ifc_unit_sort_0_33 versioned);

/*
Functions for interacting with IFC VariadicOperatorSort sorts.
*/

extern a_const_char* str_for(an_ifc_variadic_operator_sort universal);

extern an_ifc_encoded_variadic_operator_sort to_encoded(
                                      an_ifc_module                 *mod,
                                      an_ifc_variadic_operator_sort universal);

extern a_boolean is_known_sort(an_ifc_variadic_operator_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                      *mod,
                               an_ifc_variadic_operator_sort_0_33 versioned,
                               const an_ifc_validation_trace      *parent);

extern an_ifc_variadic_operator_sort to_universal_sort(
                                 an_ifc_variadic_operator_sort_0_33 versioned);

/*
Functions for interacting with IFC WordSort sorts.
*/

extern a_const_char* str_for(an_ifc_word_sort universal);

extern an_ifc_encoded_word_sort to_encoded(an_ifc_module    *mod,
                                           an_ifc_word_sort universal);

extern a_boolean is_known_sort(an_ifc_word_sort_0_33 versioned);

extern a_boolean validate_sort(an_ifc_module                 *mod,
                               an_ifc_word_sort_0_33         versioned,
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

extern an_ifc_expr_sort_0_42 expr_sort(an_ifc_expr_index_0_42 versioned);

extern uint32_t expr_value(an_ifc_expr_index_0_42 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_expr_index_0_42        versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_expr_index to_universal_index(an_ifc_module          *mod,
                                            an_ifc_expr_index_0_42 versioned);

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

extern an_ifc_stmt_sort_0_42 stmt_sort(an_ifc_stmt_index_0_42 versioned);

extern uint32_t stmt_value(an_ifc_stmt_index_0_42 versioned);

extern a_boolean validate_index(an_ifc_module                 *mod,
                                an_ifc_stmt_index_0_42        versioned,
                                const an_ifc_validation_trace *parent);

extern an_ifc_stmt_index to_universal_index(an_ifc_module          *mod,
                                            an_ifc_stmt_index_0_42 versioned);

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
inline a_boolean test_bitmask(
                             const an_ifc_basic_specifiers_bitfield &universal)
/*
Given the universal representation of BasicSpecifiersBitfield, return TRUE if
the universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  uint8_t mask = to_bitmask_0_33(a_Query);
  uint8_t test_value = universal.value & mask;;

  result = test_value == mask;
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
inline a_boolean test_bitmask(const an_ifc_function_traits_bitfield &universal)
/*
Given the universal representation of FunctionTraitsBitfield, return TRUE if
the universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  uint16_t mask = to_bitmask_0_33(a_Query);
  uint16_t test_value = universal.value & mask;;

  result = test_value == mask;
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
inline a_boolean test_bitmask(
                         const an_ifc_function_type_traits_bitfield &universal)
/*
Given the universal representation of FunctionTypeTraitsBitfield, return TRUE
if the universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  uint8_t mask = to_bitmask_0_33(a_Query);
  uint8_t test_value = universal.value & mask;;

  result = test_value == mask;
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
inline a_boolean test_bitmask(const an_ifc_msvc_traits_bitfield &universal)
/*
Given the universal representation of MsvcTraitsBitfield, return TRUE if the
universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  uint32_t mask = to_bitmask_0_33(a_Query);
  uint32_t test_value = universal.value & mask;;

  result = test_value == mask;
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
inline a_boolean test_bitmask(const an_ifc_object_traits_bitfield &universal)
/*
Given the universal representation of ObjectTraitsBitfield, return TRUE if the
universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  uint8_t mask = to_bitmask_0_33(a_Query);
  uint8_t test_value = universal.value & mask;;

  result = test_value == mask;
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
inline a_boolean test_bitmask(const an_ifc_qualifier_bitfield &universal)
/*
Given the universal representation of QualifierBitfield, return TRUE if the
universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  uint8_t mask = to_bitmask_0_33(a_Query);
  uint8_t test_value = universal.value & mask;;

  result = test_value == mask;
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
inline a_boolean test_bitmask(
                         const an_ifc_reachable_properties_bitfield &universal)
/*
Given the universal representation of ReachablePropertiesBitfield, return TRUE
if the universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  uint8_t mask = to_bitmask_0_33(a_Query);
  uint8_t test_value = universal.value & mask;;

  result = test_value == mask;
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
inline a_boolean test_bitmask(const an_ifc_scope_traits_bitfield &universal)
/*
Given the universal representation of ScopeTraitsBitfield, return TRUE if the
universal bitmask specified by a_Query matches; otherwise, return FALSE.
*/
{
  a_boolean result;

  uint8_t mask = to_bitmask_0_33(a_Query);
  uint8_t test_value = universal.value & mask;;

  result = test_value == mask;
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

extern an_ifc_operator_sort_0_33 operator_sort(
                                      an_ifc_operator_category_0_42 versioned);

extern uint16_t operator_value(an_ifc_operator_category_0_42 versioned);

extern a_boolean validate_category(an_ifc_module                 *mod,
                                   an_ifc_operator_category_0_42 versioned,
                                   const an_ifc_validation_trace *parent);

extern an_ifc_operator_category to_universal_category(
                                      an_ifc_module                 *mod,
                                      an_ifc_operator_category_0_42 versioned);

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
storage.  This will be added to the offset to get the start of the field
value's bytes.  This function guarantees even if the byte position is not an
aligned representation of the desired type (a_Desired_type), so long as the
result pointer points to aligned storage, the result will be properly aligned.

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
extern an_ifc_Node_type* get(
                          an_ifc_module    *mod,
                          an_ifc_Node_type *storage,
                          a_boolean        fill_storage = FALSE) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_partition_kind get_ifc_partition_kind() DELETED_FN_DEF


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_access_metadata;


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclAlias nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_alias> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclBitfield nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_bitfield> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclConcept nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_concept> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclConstructor nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_constructor> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclDestructor nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_destructor> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclEnumeration nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_enumeration> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclEnumerator nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_enumerator> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclField nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_field> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclFunction nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_function> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclInheritedConstructor nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_inherited_constructor> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclIntrinsic nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_intrinsic> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclMethod nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_method> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclPartialSpecialization nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_partial_specialization> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclScope nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_scope> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclTemplate nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_template> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclUsingDeclaration nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_using_declaration> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclVariable nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_decl_variable> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAccessSpecifier nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_syntax_access_specifier> {
  using return_type = an_ifc_keyword_syntax;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxBaseSpecifier nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_syntax_base_specifier> {
  using return_type = an_ifc_keyword_syntax;
};  /* an_ifc_access_metadata */


/*
The IFC access field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeBase nodes.
*/
template<>
struct an_ifc_access_metadata<an_ifc_type_base> {
  using return_type = an_ifc_access_sort;
};  /* an_ifc_access_metadata */


/*
The IFC aliasee field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_aliasee_metadata;


/*
The IFC aliasee field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclAlias nodes.
*/
template<>
struct an_ifc_aliasee_metadata<an_ifc_decl_alias> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_aliasee_metadata */


/*
The IFC aliasee field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAliasDeclaration nodes.
*/
template<>
struct an_ifc_aliasee_metadata<an_ifc_syntax_alias_declaration> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_aliasee_metadata */


/*
The IFC alternative field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_alternative_metadata;


/*
The IFC alternative field has multiple return types.  This type is a metadata
type that allows resolution of the return type for StmtIf nodes.
*/
template<>
struct an_ifc_alternative_metadata<an_ifc_stmt_if> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_alternative_metadata */


/*
The IFC alternative field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxIfStatement nodes.
*/
template<>
struct an_ifc_alternative_metadata<an_ifc_syntax_if_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_alternative_metadata */


/*
The IFC argument field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_argument_metadata;


/*
The IFC argument field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMonad nodes.
*/
template<>
struct an_ifc_argument_metadata<an_ifc_expr_monad> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_argument_metadata */


/*
The IFC argument field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxNonTypeTemplateArgument
nodes.
*/
template<>
struct an_ifc_argument_metadata<an_ifc_syntax_non_type_template_argument> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_argument_metadata */


/*
The IFC argument field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTemplateTemplateParameter
nodes.
*/
template<>
struct an_ifc_argument_metadata<an_ifc_syntax_template_template_parameter> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_argument_metadata */


/*
The IFC argument field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTypeTemplateArgument nodes.
*/
template<>
struct an_ifc_argument_metadata<an_ifc_syntax_type_template_argument> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_argument_metadata */


/*
The IFC argument field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTypeTemplateParameter
nodes.
*/
template<>
struct an_ifc_argument_metadata<an_ifc_syntax_type_template_parameter> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_argument_metadata */


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_arguments_metadata;


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of the return type for AttrCalled nodes.
*/
template<>
struct an_ifc_arguments_metadata<an_ifc_attr_called> {
  using return_type = an_ifc_attr_index;
};  /* an_ifc_arguments_metadata */


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprCall nodes.
*/
template<>
struct an_ifc_arguments_metadata<an_ifc_expr_call> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_arguments_metadata */


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprPackedTemplateArguments
nodes.
*/
template<>
struct an_ifc_arguments_metadata<an_ifc_expr_packed_template_arguments> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_arguments_metadata */


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprTemplateId nodes.
*/
template<>
struct an_ifc_arguments_metadata<an_ifc_expr_template_id> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_arguments_metadata */


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprTemplateReference nodes.
*/
template<>
struct an_ifc_arguments_metadata<an_ifc_expr_template_reference> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_arguments_metadata */


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprTypeTraitIntrinsic
nodes.
*/
template<>
struct an_ifc_arguments_metadata<an_ifc_expr_type_trait_intrinsic> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_arguments_metadata */


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of the return type for FormSpec nodes.
*/
template<>
struct an_ifc_arguments_metadata<an_ifc_form_spec> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_arguments_metadata */


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of the return type for NameSpecialization nodes.
*/
template<>
struct an_ifc_arguments_metadata<an_ifc_name_specialization> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_arguments_metadata */


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxTemplateArgumentList
nodes.
*/
template<>
struct an_ifc_arguments_metadata<an_ifc_syntax_template_argument_list> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_arguments_metadata */


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxTemplateId nodes.
*/
template<>
struct an_ifc_arguments_metadata<an_ifc_syntax_template_id> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_arguments_metadata */


/*
The IFC arguments field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxTypeTraitIntrinsic
nodes.
*/
template<>
struct an_ifc_arguments_metadata<an_ifc_syntax_type_trait_intrinsic> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_arguments_metadata */


/*
The IFC assoc field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_assoc_metadata;


/*
The IFC assoc field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprDyad nodes.
*/
template<>
struct an_ifc_assoc_metadata<an_ifc_expr_dyad> {
  using return_type = an_ifc_dyadic_operator_sort;
};  /* an_ifc_assoc_metadata */


/*
The IFC assoc field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMonad nodes.
*/
template<>
struct an_ifc_assoc_metadata<an_ifc_expr_monad> {
  using return_type = an_ifc_monadic_operator_sort;
};  /* an_ifc_assoc_metadata */


/*
The IFC assoc field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTriad nodes.
*/
template<>
struct an_ifc_assoc_metadata<an_ifc_expr_triad> {
  using return_type = an_ifc_triadic_operator_sort;
};  /* an_ifc_assoc_metadata */


/*
The IFC attributes field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_attributes_metadata;


/*
The IFC attributes field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ParameterizedEntity nodes.
*/
template<>
struct an_ifc_attributes_metadata<an_ifc_parameterized_entity> {
  using return_type = an_ifc_sentence_index;
};  /* an_ifc_attributes_metadata */


/*
The IFC attributes field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxAttributeSpecifier
nodes.
*/
template<>
struct an_ifc_attributes_metadata<an_ifc_syntax_attribute_specifier> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_attributes_metadata */


/*
The IFC attributes field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxAttributeSpecifierSeq
nodes.
*/
template<>
struct an_ifc_attributes_metadata<an_ifc_syntax_attribute_specifier_seq> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_attributes_metadata */


/*
The IFC attributes field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxAttributedDeclaration
nodes.
*/
template<>
struct an_ifc_attributes_metadata<an_ifc_syntax_attributed_declaration> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_attributes_metadata */


/*
The IFC attributes field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxAttributedStatement
nodes.
*/
template<>
struct an_ifc_attributes_metadata<an_ifc_syntax_attributed_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_attributes_metadata */


/*
The IFC base field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_base_metadata;


/*
The IFC base field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclEnumeration nodes.
*/
template<>
struct an_ifc_base_metadata<an_ifc_decl_enumeration> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_base_metadata */


/*
The IFC base field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclScope nodes.
*/
template<>
struct an_ifc_base_metadata<an_ifc_decl_scope> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_base_metadata */


/*
The IFC base field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMemberInitializer nodes.
*/
template<>
struct an_ifc_base_metadata<an_ifc_expr_member_initializer> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_base_metadata */


/*
The IFC base field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxEnumSpecifier nodes.
*/
template<>
struct an_ifc_base_metadata<an_ifc_syntax_enum_specifier> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_base_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_body_metadata;


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclConcept nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_decl_concept> {
  using return_type = an_ifc_sentence_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprLambda nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_expr_lambda> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprRequires nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_expr_requires> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for MacroFunctionLike nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_macro_function_like> {
  using return_type = an_ifc_form_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for MacroObjectLike nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_macro_object_like> {
  using return_type = an_ifc_form_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ParameterizedEntity nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_parameterized_entity> {
  using return_type = an_ifc_sentence_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtDoWhile nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_stmt_do_while> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtFor nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_stmt_for> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtHandler nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_stmt_handler> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtSwitch nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_stmt_switch> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtWhile nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_stmt_while> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxDoWhileStatement nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_syntax_do_while_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxForStatement nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_syntax_for_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxFunctionTryBlock nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_syntax_function_try_block> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxHandler nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_syntax_handler> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxRangeBasedForStatement
nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_syntax_range_based_for_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxSEHExcept nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_syntax_seh_except> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxSEHFinally nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_syntax_seh_finally> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxSEHTry nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_syntax_seh_try> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxSwitchStatement nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_syntax_switch_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTryBlock nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_syntax_try_block> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxWhileStatement nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_syntax_while_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_body_metadata */


/*
The IFC body field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitFunctionDefinition nodes.
*/
template<>
struct an_ifc_body_metadata<an_ifc_trait_function_definition> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_body_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_condition_metadata;


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for StmtDoWhile nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_stmt_do_while> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for StmtFor nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_stmt_for> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for StmtIf nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_stmt_if> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for StmtSwitch nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_stmt_switch> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for StmtWhile nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_stmt_while> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxCompoundRequirement
nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_compound_requirement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxDoWhileStatement
nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_do_while_statement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxExplicitSpecifier
nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_explicit_specifier> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxForStatement nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_for_statement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxIfStatement nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_if_statement> {
  using return_type = an_ifc_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxNestedRequirement
nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_nested_requirement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxRequiresClause nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_requires_clause> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxSEHExcept nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_seh_except> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxSimpleRequirement
nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_simple_requirement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for
SyntaxStaticAssertDeclaration nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_static_assert_declaration> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxSwitchStatement nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_switch_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_condition_metadata */


/*
The IFC condition field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxWhileStatement nodes.
*/
template<>
struct an_ifc_condition_metadata<an_ifc_syntax_while_statement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_condition_metadata */


/*
The IFC consequence field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_consequence_metadata;


/*
The IFC consequence field has multiple return types.  This type is a metadata
type that allows resolution of the return type for StmtIf nodes.
*/
template<>
struct an_ifc_consequence_metadata<an_ifc_stmt_if> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_consequence_metadata */


/*
The IFC consequence field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxIfStatement nodes.
*/
template<>
struct an_ifc_consequence_metadata<an_ifc_syntax_if_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_consequence_metadata */


/*
The IFC constraint field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_constraint_metadata;


/*
The IFC constraint field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ChartUnilevel nodes.
*/
template<>
struct an_ifc_constraint_metadata<an_ifc_chart_unilevel> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_constraint_metadata */


/*
The IFC constraint field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclConcept nodes.
*/
template<>
struct an_ifc_constraint_metadata<an_ifc_decl_concept> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_constraint_metadata */


/*
The IFC constraint field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclParameter nodes.
*/
template<>
struct an_ifc_constraint_metadata<an_ifc_decl_parameter> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_constraint_metadata */


/*
The IFC constraint field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprLambda nodes.
*/
template<>
struct an_ifc_constraint_metadata<an_ifc_expr_lambda> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_constraint_metadata */


/*
The IFC constraint field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxCompoundRequirement
nodes.
*/
template<>
struct an_ifc_constraint_metadata<an_ifc_syntax_compound_requirement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_constraint_metadata */


/*
The IFC constraint field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxInitDeclarator nodes.
*/
template<>
struct an_ifc_constraint_metadata<an_ifc_syntax_init_declarator> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_constraint_metadata */


/*
The IFC constraint field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxMemberDeclarator
nodes.
*/
template<>
struct an_ifc_constraint_metadata<an_ifc_syntax_member_declarator> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_constraint_metadata */


/*
The IFC constraint field has multiple return types.  This type is a metadata
type that allows resolution of the return type for
SyntaxPlaceholderTypeSpecifier nodes.
*/
template<>
struct an_ifc_constraint_metadata<an_ifc_syntax_placeholder_type_specifier> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_constraint_metadata */


/*
The IFC constraint field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxTypeTemplateParameter
nodes.
*/
template<>
struct an_ifc_constraint_metadata<an_ifc_syntax_type_template_parameter> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_constraint_metadata */


/*
The IFC constraint field has multiple return types.  This type is a metadata
type that allows resolution of the return type for TypePlaceholder nodes.
*/
template<>
struct an_ifc_constraint_metadata<an_ifc_type_placeholder> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_constraint_metadata */


/*
The IFC continuation field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_continuation_metadata;


/*
The IFC continuation field has multiple return types.  This type is a metadata
type that allows resolution of the return type for StmtFor nodes.
*/
template<>
struct an_ifc_continuation_metadata<an_ifc_stmt_for> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_continuation_metadata */


/*
The IFC continuation field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxForStatement nodes.
*/
template<>
struct an_ifc_continuation_metadata<an_ifc_syntax_for_statement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_continuation_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_decl_metadata;


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclExplicitInstantiation nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_decl_explicit_instantiation> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclExplicitSpecialization nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_decl_explicit_specialization> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclSpecialization nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_decl_specialization> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ParameterizedEntity nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_parameterized_entity> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtDecl nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_stmt_decl> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtVariableDecl nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_stmt_variable_decl> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAttributedDeclaration
nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_syntax_attributed_declaration> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxDeclarationStatement nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_syntax_declaration_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxRangeBasedForStatement
nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_syntax_range_based_for_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitAliasTemplate nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_alias_template> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitAttribute nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_attribute> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitDeductionGuide nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_deduction_guide> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitDeprecated nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_deprecated> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitFriend nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_friend> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitFunctionDefinition nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_function_definition> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitMsvcDeclAttrs nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_msvc_decl_attrs> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitMsvcFuncParams nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_msvc_func_params> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitMsvcUuid nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_msvc_uuid> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitMsvcVendorTrait nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_msvc_vendor_trait> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitRequires nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_requires> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitSpecialization nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_trait_specialization> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC decl field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeDesignated nodes.
*/
template<>
struct an_ifc_decl_metadata<an_ifc_type_designated> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_decl_metadata */


/*
The IFC eh_spec field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_eh_spec_metadata;


/*
The IFC eh_spec field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclDestructor nodes.
*/
template<>
struct an_ifc_eh_spec_metadata<an_ifc_decl_destructor> {
  using return_type = an_ifc_noexcept_specification;
};  /* an_ifc_eh_spec_metadata */


/*
The IFC eh_spec field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxFunctionDeclarator nodes.
*/
template<>
struct an_ifc_eh_spec_metadata<an_ifc_syntax_function_declarator> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_eh_spec_metadata */


/*
The IFC eh_spec field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxLambdaDeclarator nodes.
*/
template<>
struct an_ifc_eh_spec_metadata<an_ifc_syntax_lambda_declarator> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_eh_spec_metadata */


/*
The IFC eh_spec field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeFunction nodes.
*/
template<>
struct an_ifc_eh_spec_metadata<an_ifc_type_function> {
  using return_type = an_ifc_noexcept_specification;
};  /* an_ifc_eh_spec_metadata */


/*
The IFC eh_spec field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeMethod nodes.
*/
template<>
struct an_ifc_eh_spec_metadata<an_ifc_type_method> {
  using return_type = an_ifc_noexcept_specification;
};  /* an_ifc_eh_spec_metadata */


/*
The IFC eh_spec field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeTor nodes.
*/
template<>
struct an_ifc_eh_spec_metadata<an_ifc_type_tor> {
  using return_type = an_ifc_noexcept_specification;
};  /* an_ifc_eh_spec_metadata */


/*
The IFC entity field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_entity_metadata;


/*
The IFC entity field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclFriend nodes.
*/
template<>
struct an_ifc_entity_metadata<an_ifc_decl_friend> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_entity_metadata */


/*
The IFC entity field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclPartialSpecialization nodes.
*/
template<>
struct an_ifc_entity_metadata<an_ifc_decl_partial_specialization> {
  using return_type = an_ifc_parameterized_entity;
};  /* an_ifc_entity_metadata */


/*
The IFC entity field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclTemplate nodes.
*/
template<>
struct an_ifc_entity_metadata<an_ifc_decl_template> {
  using return_type = an_ifc_parameterized_entity;
};  /* an_ifc_entity_metadata */


/*
The IFC entity field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclTemploid nodes.
*/
template<>
struct an_ifc_entity_metadata<an_ifc_decl_temploid> {
  using return_type = an_ifc_parameterized_entity;
};  /* an_ifc_entity_metadata */


/*
The IFC exception field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_exception_metadata;


/*
The IFC exception field has multiple return types.  This type is a metadata
type that allows resolution of the return type for StmtHandler nodes.
*/
template<>
struct an_ifc_exception_metadata<an_ifc_stmt_handler> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_exception_metadata */


/*
The IFC exception field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxHandler nodes.
*/
template<>
struct an_ifc_exception_metadata<an_ifc_syntax_handler> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_exception_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_expr_metadata;


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCondition nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_expr_condition> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprInitializer nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_expr_initializer> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprUnaryFold nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_expr_unary_fold> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtCase nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_stmt_case> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtExpression nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_stmt_expression> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtReturn nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_stmt_return> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxDecltypeSpecifier nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_syntax_decltype_specifier> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxExpressionStatement nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_syntax_expression_statement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxNoexceptSpecification
nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_syntax_noexcept_specification> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxReturnStatement nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_syntax_return_statement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxSimpleTypeSpecifier nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_syntax_simple_type_specifier> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeDecltype nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_type_decltype> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_expr_metadata */


/*
The IFC expr field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeSyntactic nodes.
*/
template<>
struct an_ifc_expr_metadata<an_ifc_type_syntactic> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_expr_metadata */


/*
The IFC function field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_function_metadata;


/*
The IFC function field has multiple return types.  This type is a metadata type
that allows resolution of the return type for AttrCalled nodes.
*/
template<>
struct an_ifc_function_metadata<an_ifc_attr_called> {
  using return_type = an_ifc_attr_index;
};  /* an_ifc_function_metadata */


/*
The IFC function field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprVirtualFunctionConversion
nodes.
*/
template<>
struct an_ifc_function_metadata<an_ifc_expr_virtual_function_conversion> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_function_metadata */


/*
The IFC index field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_index_metadata;


/*
The IFC index field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclReference nodes.
*/
template<>
struct an_ifc_index_metadata<an_ifc_decl_reference> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_index_metadata */


/*
The IFC index field has multiple return types.  This type is a metadata type
that allows resolution of the return type for NestableWord nodes.
*/
template<>
struct an_ifc_index_metadata<an_ifc_nestable_word> {
  using return_type = an_ifc_index;
};  /* an_ifc_index_metadata */


/*
The IFC index field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ScopeMember nodes.
*/
template<>
struct an_ifc_index_metadata<an_ifc_scope_member> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_index_metadata */


/*
The IFC index field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SourceWord nodes.
*/
template<>
struct an_ifc_index_metadata<an_ifc_source_word> {
  using return_type = an_ifc_index;
};  /* an_ifc_index_metadata */


/*
The IFC index field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxArrayIndex nodes.
*/
template<>
struct an_ifc_index_metadata<an_ifc_syntax_array_index> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_index_metadata */


/*
The IFC initialization field has multiple return types.  This type is a
metadata type that allows resolution of those return types based on the
respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_initialization_metadata;


/*
The IFC initialization field has multiple return types.  This type is a
metadata type that allows resolution of the return type for StmtFor nodes.
*/
template<>
struct an_ifc_initialization_metadata<an_ifc_stmt_for> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_initialization_metadata */


/*
The IFC initialization field has multiple return types.  This type is a
metadata type that allows resolution of the return type for StmtIf nodes.
*/
template<>
struct an_ifc_initialization_metadata<an_ifc_stmt_if> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_initialization_metadata */


/*
The IFC initialization field has multiple return types.  This type is a
metadata type that allows resolution of the return type for StmtSwitch nodes.
*/
template<>
struct an_ifc_initialization_metadata<an_ifc_stmt_switch> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_initialization_metadata */


/*
The IFC initialization field has multiple return types.  This type is a
metadata type that allows resolution of the return type for SyntaxForStatement
nodes.
*/
template<>
struct an_ifc_initialization_metadata<an_ifc_syntax_for_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_initialization_metadata */


/*
The IFC initialization field has multiple return types.  This type is a
metadata type that allows resolution of the return type for SyntaxIfStatement
nodes.
*/
template<>
struct an_ifc_initialization_metadata<an_ifc_syntax_if_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_initialization_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_initializer_metadata;


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclBitfield nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_decl_bitfield> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclEnumeration nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_decl_enumeration> {
  using return_type = an_ifc_sequence;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclEnumerator nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_decl_enumerator> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclField nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_decl_field> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclParameter nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_decl_parameter> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclScope nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_decl_scope> {
  using return_type = an_ifc_scope_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclVariable nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_decl_variable> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprAssignInitializer nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_expr_assign_initializer> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprDesignatedInitializer
nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_expr_designated_initializer> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprMemberInitializer nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_expr_member_initializer> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxConceptDefinition
nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_syntax_concept_definition> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxEnumeratorDefinition
nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_syntax_enumerator_definition> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxInitCapture nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_syntax_init_capture> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxInitDeclarator nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_syntax_init_declarator> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxMemInitializer nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_syntax_mem_initializer> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxMemberDeclarator
nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_syntax_member_declarator> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxRangeBasedForStatement
nodes.
*/
template<>
struct an_ifc_initializer_metadata<an_ifc_syntax_range_based_for_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializer field has multiple return types.  This type is a metadata
type that allows resolution of the return type for
SyntaxStructuredBindingDeclaration nodes.
*/
template<>
struct an_ifc_initializer_metadata<
                                an_ifc_syntax_structured_binding_declaration> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializer_metadata */


/*
The IFC initializers field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_initializers_metadata;


/*
The IFC initializers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxCtorInitializer nodes.
*/
template<>
struct an_ifc_initializers_metadata<an_ifc_syntax_ctor_initializer> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_initializers_metadata */


/*
The IFC initializers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxFunctionBody nodes.
*/
template<>
struct an_ifc_initializers_metadata<an_ifc_syntax_function_body> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_initializers_metadata */


/*
The IFC initializers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxFunctionDefinition
nodes.
*/
template<>
struct an_ifc_initializers_metadata<an_ifc_syntax_function_definition> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_initializers_metadata */


/*
The IFC initializers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxFunctionTryBlock
nodes.
*/
template<>
struct an_ifc_initializers_metadata<an_ifc_syntax_function_try_block> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_initializers_metadata */


/*
The IFC initializers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for TraitFunctionDefinition
nodes.
*/
template<>
struct an_ifc_initializers_metadata<an_ifc_trait_function_definition> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_initializers_metadata */


/*
The IFC label field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_label_metadata;


/*
The IFC label field has multiple return types.  This type is a metadata type
that allows resolution of the return type for AttrLabeled nodes.
*/
template<>
struct an_ifc_label_metadata<an_ifc_attr_labeled> {
  using return_type = an_ifc_nestable_word;
};  /* an_ifc_label_metadata */


/*
The IFC label field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtLabeled nodes.
*/
template<>
struct an_ifc_label_metadata<an_ifc_stmt_labeled> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_label_metadata */


/*
The IFC label field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxGotoStatement nodes.
*/
template<>
struct an_ifc_label_metadata<an_ifc_syntax_goto_statement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_label_metadata */


/*
The IFC label field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxLabeledStatement nodes.
*/
template<>
struct an_ifc_label_metadata<an_ifc_syntax_labeled_statement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_label_metadata */


/*
The IFC left field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_left_metadata;


/*
The IFC left field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprBinaryFold nodes.
*/
template<>
struct an_ifc_left_metadata<an_ifc_expr_binary_fold> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_left_metadata */


/*
The IFC left field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprExpressionList nodes.
*/
template<>
struct an_ifc_left_metadata<an_ifc_expr_expression_list> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_left_paren_metadata;


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxAlignas nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_alignas> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for
SyntaxAttributeArgumentClause nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_attribute_argument_clause> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxClassSpecifier nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_class_specifier> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxDecltypeSpecifier
nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_decltype_specifier> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxDynamicExceptionSpec
nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_dynamic_exception_spec> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxExplicitSpecifier
nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_explicit_specifier> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxForStatement nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_for_statement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxFunctionDeclarator
nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_function_declarator> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxHandler nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_handler> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxLambdaDeclarator
nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_lambda_declarator> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxNoexceptSpecification
nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_noexcept_specification> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxRangeBasedForStatement
nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_range_based_for_statement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxSEHExcept nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_seh_except> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC left_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for
SyntaxStaticAssertDeclaration nodes.
*/
template<>
struct an_ifc_left_paren_metadata<an_ifc_syntax_static_assert_declaration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_left_paren_metadata */


/*
The IFC line field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_line_metadata;


/*
The IFC line field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SourceLine nodes.
*/
template<>
struct an_ifc_line_metadata<an_ifc_source_line> {
  using return_type = an_ifc_line_number;
};  /* an_ifc_line_metadata */


/*
The IFC line field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SourceLocation nodes.
*/
template<>
struct an_ifc_line_metadata<an_ifc_source_location> {
  using return_type = an_ifc_line_index;
};  /* an_ifc_line_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_locus_metadata;


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclAlias nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_alias> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclBitfield nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_bitfield> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclConcept nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_concept> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclConstructor nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_constructor> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclDeductionGuide nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_deduction_guide> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclDestructor nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_destructor> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclEnumeration nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_enumeration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclEnumerator nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_enumerator> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclExpansion nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_expansion> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclField nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_field> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclFunction nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_function> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclInheritedConstructor nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_inherited_constructor> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclIntrinsic nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_intrinsic> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclMethod nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_method> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclParameter nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_parameter> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclPartialSpecialization nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_partial_specialization> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclScope nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_scope> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclSpecialization nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_specialization> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclTemplate nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_template> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclUsingDeclaration nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_using_declaration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclVariable nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_decl_variable> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprAlignof nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_alignof> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprArrayValue nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_array_value> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprBinaryFold nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_binary_fold> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCall nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_call> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCast nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_cast> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCompoundString nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_compound_string> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCondition nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_condition> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprDesignatedInitializer nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_designated_initializer> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprDestructorCall nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_destructor_call> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprDyad nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_dyad> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprDynamicDispatch nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_dynamic_dispatch> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprEmpty nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_empty> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprExpansion nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_expansion> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprFunctionString nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_function_string> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprHierarchyConversion nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_hierarchy_conversion> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprInheritancePath nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_inheritance_path> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprInitializer nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_initializer> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprInitializerList nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_initializer_list> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprLabel nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_label> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprLiteral nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_literal> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMemberAccess nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_member_access> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMemberInitializer nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_member_initializer> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMonad nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_monad> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprNamedDecl nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_named_decl> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprNullptr nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_nullptr> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprPackedTemplateArguments
nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_packed_template_arguments> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprPath nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_path> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprPlaceholder nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_placeholder> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprPointer nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_pointer> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprProductTypeValue nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_product_type_value> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprPushState nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_push_state> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprQualifiedName nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_qualified_name> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprRead nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_read> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprRequires nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_requires> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprSimpleIdentifier nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_simple_identifier> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprSizeofType nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_sizeof_type> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprString nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_string> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprStringSequence nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_string_sequence> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprSumTypeValue nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_sum_type_value> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTemplateId nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_template_id> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTemplateReference nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_template_reference> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTemporary nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_temporary> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprThis nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_this> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTokens nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_tokens> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTriad nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_triad> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTuple nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_tuple> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprType nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_type> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTypeTraitIntrinsic nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_type_trait_intrinsic> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTypeid nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_typeid> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprUnaryFold nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_unary_fold> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprUnqualifiedId nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_unqualified_id> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprUnresolvedId nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_unresolved_id> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprVirtualFunctionConversion
nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_expr_virtual_function_conversion> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormCatenate nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_catenate> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormCharacter nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_character> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormHeader nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_header> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormIdentifier nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_identifier> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormJunk nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_junk> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormKeyword nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_keyword> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormNumber nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_number> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormOperator nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_operator> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormParameter nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_parameter> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormParenthesized nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_parenthesized> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormPragma nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_pragma> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormString nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_string> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormStringize nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_stringize> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormWhitespace nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_form_whitespace> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for KeywordSyntax nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_keyword_syntax> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for MacroFunctionLike nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_macro_function_like> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for MacroObjectLike nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_macro_object_like> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for NestableWord nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_nestable_word> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SourceSentence nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_source_sentence> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SourceWord nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_source_word> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtBlock nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_block> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtBreak nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_break> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtCase nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_case> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtContinue nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_continue> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtDecl nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_decl> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtDefault nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_default> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtDoWhile nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_do_while> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtEmpty nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_empty> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtExpansion nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_expansion> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtExpression nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_expression> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtFor nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_for> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtGoto nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_goto> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtHandler nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_handler> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtIf nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_if> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtLabeled nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_labeled> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtReturn nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_return> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtSwitch nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_switch> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtVariableDecl nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_variable_decl> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtWhile nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_stmt_while> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAccessSpecifier nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_access_specifier> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAliasDeclaration nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_alias_declaration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAlignas nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_alignas> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAsmStatement nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_asm_statement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAttributeUsingPrefix nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_attribute_using_prefix> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAttributedDeclaration
nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_attributed_declaration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxBinaryFoldExpression nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_binary_fold_expression> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxCaptureDefault nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_capture_default> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxCompoundRequirement nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_compound_requirement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxConceptDefinition nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_concept_definition> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxConditionDeclaration nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_condition_declaration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxDeclSpecifierSeq nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_decl_specifier_seq> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxDeclarator nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_declarator> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxEmptyStatement nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_empty_statement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxEnumSpecifier nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_enum_specifier> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxEnumeratorDefinition nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_enumerator_definition> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxExceptionDeclaration nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_exception_declaration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxExplicitSpecifier nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_explicit_specifier> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxGotoStatement nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_goto_statement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxLabeledStatement nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_labeled_statement> {
  using return_type = an_ifc_keyword_sort;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxMemberDeclarator nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_member_declarator> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxNestedRequirement nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_nested_requirement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxNoexceptSpecification
nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_noexcept_specification> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxParameterDeclarator nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_parameter_declarator> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxPlaceholderTypeSpecifier
nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_placeholder_type_specifier> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxPointerDeclarator nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_pointer_declarator> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxRequirementBody nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_requirement_body> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxRequiresClause nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_requires_clause> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxSimpleDeclaration nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_simple_declaration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxSimpleRequirement nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_simple_requirement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxSimpleTypeSpecifier nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_simple_type_specifier> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxStaticAssertDeclaration
nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_static_assert_declaration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for
SyntaxStructuredBindingDeclaration nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_structured_binding_declaration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxSuper nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_super> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTemplateDeclaration nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_template_declaration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTemplateId nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_template_id> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTemplateTemplateParameter
nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_template_template_parameter> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxThisCapture nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_this_capture> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTypeId nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_type_id> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTypeRequirement nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_type_requirement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTypeSpecifierSeq nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_type_specifier_seq> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTypeTemplateParameter
nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_type_template_parameter> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTypeTraitIntrinsic nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_type_trait_intrinsic> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxUnaryFoldExpression nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_unary_fold_expression> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC locus field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxVirtualSpecifierSeq nodes.
*/
template<>
struct an_ifc_locus_metadata<an_ifc_syntax_virtual_specifier_seq> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_locus_metadata */


/*
The IFC member field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_member_metadata;


/*
The IFC member field has multiple return types.  This type is a metadata type
that allows resolution of the return type for AttrScoped nodes.
*/
template<>
struct an_ifc_member_metadata<an_ifc_attr_scoped> {
  using return_type = an_ifc_nestable_word;
};  /* an_ifc_member_metadata */


/*
The IFC member field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclProperty nodes.
*/
template<>
struct an_ifc_member_metadata<an_ifc_decl_property> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_member_metadata */


/*
The IFC member field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprDesignatedInitializer nodes.
*/
template<>
struct an_ifc_member_metadata<an_ifc_expr_designated_initializer> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_member_metadata */


/*
The IFC member field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMemberInitializer nodes.
*/
template<>
struct an_ifc_member_metadata<an_ifc_expr_member_initializer> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_member_metadata */


/*
The IFC member field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprPath nodes.
*/
template<>
struct an_ifc_member_metadata<an_ifc_expr_path> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_member_metadata */


/*
The IFC member field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxMemInitializer nodes.
*/
template<>
struct an_ifc_member_metadata<an_ifc_syntax_mem_initializer> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_member_metadata */


/*
The IFC member field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypePointerToMember nodes.
*/
template<>
struct an_ifc_member_metadata<an_ifc_type_pointer_to_member> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_member_metadata */


/*
The IFC members field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_members_metadata;


/*
The IFC members field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprProductTypeValue nodes.
*/
template<>
struct an_ifc_members_metadata<an_ifc_expr_product_type_value> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_members_metadata */


/*
The IFC members field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxClassSpecifier nodes.
*/
template<>
struct an_ifc_members_metadata<an_ifc_syntax_class_specifier> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_members_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_name_metadata;


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclAlias nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_alias> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclBitfield nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_bitfield> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclConcept nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_concept> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclConstructor nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_constructor> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclDeductionGuide nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_deduction_guide> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclDestructor nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_destructor> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclEnumeration nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_enumeration> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclEnumerator nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_enumerator> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclField nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_field> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclFunction nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_function> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclInheritedConstructor nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_inherited_constructor> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclIntrinsic nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_intrinsic> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclMethod nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_method> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclOutputSegment nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_output_segment> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclParameter nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_parameter> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclPartialSpecialization nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_partial_specialization> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclScope nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_scope> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclSpecialization nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_specialization> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclTemplate nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_template> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclUsingDeclaration nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_using_declaration> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclVariable nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_decl_variable> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprDestructorCall nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_expr_destructor_call> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMemberAccess nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_expr_member_access> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprSimpleIdentifier nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_expr_simple_identifier> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprUnqualifiedId nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_expr_unqualified_id> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprUnresolvedId nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_expr_unresolved_id> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for MacroFunctionLike nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_macro_function_like> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for MacroObjectLike nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_macro_object_like> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for NameTemplate nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_name_template> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for Partition nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_partition> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAliasDeclaration nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_alias_declaration> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAttribute nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_attribute> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxClassSpecifier nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_class_specifier> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxConceptDefinition nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_concept_definition> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxDeclarator nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_declarator> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxEnumSpecifier nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_enum_specifier> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxEnumeratorDefinition nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_enumerator_definition> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxInitCapture nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_init_capture> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxNamespaceAliasDefinition
nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_namespace_alias_definition> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxSimpleCapture nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_simple_capture> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxStructuredBindingIdentifier
nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_structured_binding_identifier> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTemplateId nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_template_id> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTemplateTemplateParameter
nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_template_template_parameter> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTypeTemplateParameter
nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_type_template_parameter> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_name_metadata */


/*
The IFC name field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxUsingEnumDeclaration nodes.
*/
template<>
struct an_ifc_name_metadata<an_ifc_syntax_using_enum_declaration> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_name_metadata */


/*
The IFC offset field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_offset_metadata;


/*
The IFC offset field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMemberAccess nodes.
*/
template<>
struct an_ifc_offset_metadata<an_ifc_expr_member_access> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_offset_metadata */


/*
The IFC offset field has multiple return types.  This type is a metadata type
that allows resolution of the return type for Partition nodes.
*/
template<>
struct an_ifc_offset_metadata<an_ifc_partition> {
  using return_type = an_ifc_byte_offset;
};  /* an_ifc_offset_metadata */


/*
The IFC op field has multiple return types.  This type is a metadata type that
allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_op_metadata;


/*
The IFC op field has multiple return types.  This type is a metadata type that
allows resolution of the return type for ExprCast nodes.
*/
template<>
struct an_ifc_op_metadata<an_ifc_expr_cast> {
  using return_type = an_ifc_dyadic_operator_sort;
};  /* an_ifc_op_metadata */


/*
The IFC op field has multiple return types.  This type is a metadata type that
allows resolution of the return type for ExprHierarchyConversion nodes.
*/
template<>
struct an_ifc_op_metadata<an_ifc_expr_hierarchy_conversion> {
  using return_type = an_ifc_dyadic_operator_sort;
};  /* an_ifc_op_metadata */


/*
The IFC op field has multiple return types.  This type is a metadata type that
allows resolution of the return type for FormOperator nodes.
*/
template<>
struct an_ifc_op_metadata<an_ifc_form_operator> {
  using return_type = an_ifc_form_operator_sort;
};  /* an_ifc_op_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_operand_metadata;


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for AttrExpanded nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_attr_expanded> {
  using return_type = an_ifc_attr_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclExpansion nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_decl_expansion> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprAlignof nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_expr_alignof> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprExpansion nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_expr_expansion> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprSizeofType nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_expr_sizeof_type> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTypeid nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_expr_typeid> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormParenthesized nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_form_parenthesized> {
  using return_type = an_ifc_form_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormPragma nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_form_pragma> {
  using return_type = an_ifc_form_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormStringize nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_form_stringize> {
  using return_type = an_ifc_form_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtExpansion nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_stmt_expansion> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAlignas nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_syntax_alignas> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operand field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxUnaryFoldExpression nodes.
*/
template<>
struct an_ifc_operand_metadata<an_ifc_syntax_unary_fold_expression> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_operand_metadata */


/*
The IFC operation field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_operation_metadata;


/*
The IFC operation field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprBinaryFold nodes.
*/
template<>
struct an_ifc_operation_metadata<an_ifc_expr_binary_fold> {
  using return_type = an_ifc_dyadic_operator_sort;
};  /* an_ifc_operation_metadata */


/*
The IFC operation field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprCall nodes.
*/
template<>
struct an_ifc_operation_metadata<an_ifc_expr_call> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_operation_metadata */


/*
The IFC operation field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprUnaryFold nodes.
*/
template<>
struct an_ifc_operation_metadata<an_ifc_expr_unary_fold> {
  using return_type = an_ifc_dyadic_operator_sort;
};  /* an_ifc_operation_metadata */


/*
The IFC pack field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_pack_metadata;


/*
The IFC pack field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclParameter nodes.
*/
template<>
struct an_ifc_pack_metadata<an_ifc_decl_parameter> {
  using return_type = an_ifc_bool;
};  /* an_ifc_pack_metadata */


/*
The IFC pack field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeExpansion nodes.
*/
template<>
struct an_ifc_pack_metadata<an_ifc_type_expansion> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_pack_metadata */


/*
The IFC parameters field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_parameters_metadata;


/*
The IFC parameters field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprRequires nodes.
*/
template<>
struct an_ifc_parameters_metadata<an_ifc_expr_requires> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_parameters_metadata */


/*
The IFC parameters field has multiple return types.  This type is a metadata
type that allows resolution of the return type for MacroFunctionLike nodes.
*/
template<>
struct an_ifc_parameters_metadata<an_ifc_macro_function_like> {
  using return_type = an_ifc_form_index;
};  /* an_ifc_parameters_metadata */


/*
The IFC parameters field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxConceptDefinition
nodes.
*/
template<>
struct an_ifc_parameters_metadata<an_ifc_syntax_concept_definition> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_parameters_metadata */


/*
The IFC parameters field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxFunctionDeclarator
nodes.
*/
template<>
struct an_ifc_parameters_metadata<an_ifc_syntax_function_declarator> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_parameters_metadata */


/*
The IFC parameters field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxLambdaDeclarator
nodes.
*/
template<>
struct an_ifc_parameters_metadata<an_ifc_syntax_lambda_declarator> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_parameters_metadata */


/*
The IFC parameters field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxTemplateDeclaration
nodes.
*/
template<>
struct an_ifc_parameters_metadata<an_ifc_syntax_template_declaration> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_parameters_metadata */


/*
The IFC parameters field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxTemplateParameterList
nodes.
*/
template<>
struct an_ifc_parameters_metadata<an_ifc_syntax_template_parameter_list> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_parameters_metadata */


/*
The IFC parameters field has multiple return types.  This type is a metadata
type that allows resolution of the return type for
SyntaxTemplateTemplateParameter nodes.
*/
template<>
struct an_ifc_parameters_metadata<an_ifc_syntax_template_template_parameter> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_parameters_metadata */


/*
The IFC parameters field has multiple return types.  This type is a metadata
type that allows resolution of the return type for TraitFunctionDefinition
nodes.
*/
template<>
struct an_ifc_parameters_metadata<an_ifc_trait_function_definition> {
  using return_type = an_ifc_chart_index;
};  /* an_ifc_parameters_metadata */


/*
The IFC path field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_path_metadata;


/*
The IFC path field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprInheritancePath nodes.
*/
template<>
struct an_ifc_path_metadata<an_ifc_expr_inheritance_path> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_path_metadata */


/*
The IFC path field has multiple return types.  This type is a metadata type
that allows resolution of the return type for NameSourceFile nodes.
*/
template<>
struct an_ifc_path_metadata<an_ifc_name_source_file> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_path_metadata */


/*
The IFC path field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeTypename nodes.
*/
template<>
struct an_ifc_path_metadata<an_ifc_type_typename> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_path_metadata */


/*
The IFC prefix field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_prefix_metadata;


/*
The IFC prefix field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCompoundString nodes.
*/
template<>
struct an_ifc_prefix_metadata<an_ifc_expr_compound_string> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_prefix_metadata */


/*
The IFC prefix field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAttributeSpecifier nodes.
*/
template<>
struct an_ifc_prefix_metadata<an_ifc_syntax_attribute_specifier> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_prefix_metadata */


/*
The IFC primary field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_primary_metadata;


/*
The IFC primary field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTemplateId nodes.
*/
template<>
struct an_ifc_primary_metadata<an_ifc_expr_template_id> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_primary_metadata */


/*
The IFC primary field has multiple return types.  This type is a metadata type
that allows resolution of the return type for NameSpecialization nodes.
*/
template<>
struct an_ifc_primary_metadata<an_ifc_name_specialization> {
  using return_type = an_ifc_name_index;
};  /* an_ifc_primary_metadata */


/*
The IFC resolution field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_resolution_metadata;


/*
The IFC resolution field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclUsingDeclaration nodes.
*/
template<>
struct an_ifc_resolution_metadata<an_ifc_decl_using_declaration> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_resolution_metadata */


/*
The IFC resolution field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprNamedDecl nodes.
*/
template<>
struct an_ifc_resolution_metadata<an_ifc_expr_named_decl> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_resolution_metadata */


/*
The IFC resolution field has multiple return types.  This type is a metadata
type that allows resolution of the return type for ExprUnqualifiedId nodes.
*/
template<>
struct an_ifc_resolution_metadata<an_ifc_expr_unqualified_id> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_resolution_metadata */


/*
The IFC right field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_right_metadata;


/*
The IFC right field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprBinaryFold nodes.
*/
template<>
struct an_ifc_right_metadata<an_ifc_expr_binary_fold> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_right_metadata */


/*
The IFC right field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprExpressionList nodes.
*/
template<>
struct an_ifc_right_metadata<an_ifc_expr_expression_list> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_right_paren_metadata;


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxAlignas nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_alignas> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for
SyntaxAttributeArgumentClause nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_attribute_argument_clause> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxBinaryFoldExpression
nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_binary_fold_expression> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxClassSpecifier nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_class_specifier> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxDecltypeSpecifier
nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_decltype_specifier> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxDynamicExceptionSpec
nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_dynamic_exception_spec> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxExplicitSpecifier
nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_explicit_specifier> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxForStatement nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_for_statement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxFunctionDeclarator
nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_function_declarator> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxHandler nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_handler> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxLambdaDeclarator
nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_lambda_declarator> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxNoexceptSpecification
nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_noexcept_specification> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxRangeBasedForStatement
nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_range_based_for_statement> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxSEHExcept nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_seh_except> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for
SyntaxStaticAssertDeclaration nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_static_assert_declaration> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC right_paren field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxUnaryFoldExpression
nodes.
*/
template<>
struct an_ifc_right_paren_metadata<an_ifc_syntax_unary_fold_expression> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_right_paren_metadata */


/*
The IFC scope field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_scope_metadata;


/*
The IFC scope field has multiple return types.  This type is a metadata type
that allows resolution of the return type for AttrScoped nodes.
*/
template<>
struct an_ifc_scope_metadata<an_ifc_attr_scoped> {
  using return_type = an_ifc_nestable_word;
};  /* an_ifc_scope_metadata */


/*
The IFC scope field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprPath nodes.
*/
template<>
struct an_ifc_scope_metadata<an_ifc_expr_path> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_scope_metadata */


/*
The IFC scope field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTemplateReference nodes.
*/
template<>
struct an_ifc_scope_metadata<an_ifc_expr_template_reference> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_scope_metadata */


/*
The IFC scope field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAttribute nodes.
*/
template<>
struct an_ifc_scope_metadata<an_ifc_syntax_attribute> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_scope_metadata */


/*
The IFC scope field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAttributeUsingPrefix nodes.
*/
template<>
struct an_ifc_scope_metadata<an_ifc_syntax_attribute_using_prefix> {
  using return_type = an_ifc_source_location;
};  /* an_ifc_scope_metadata */


/*
The IFC scope field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeMethod nodes.
*/
template<>
struct an_ifc_scope_metadata<an_ifc_type_method> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_scope_metadata */


/*
The IFC scope field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypePointerToMember nodes.
*/
template<>
struct an_ifc_scope_metadata<an_ifc_type_pointer_to_member> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_scope_metadata */


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_sort_metadata;


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclParameter nodes.
*/
template<>
struct an_ifc_sort_metadata<an_ifc_decl_parameter> {
  using return_type = an_ifc_parameter_sort;
};  /* an_ifc_sort_metadata */


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclSpecialization nodes.
*/
template<>
struct an_ifc_sort_metadata<an_ifc_decl_specialization> {
  using return_type = an_ifc_specialization_sort;
};  /* an_ifc_sort_metadata */


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprInitializer nodes.
*/
template<>
struct an_ifc_sort_metadata<an_ifc_expr_initializer> {
  using return_type = an_ifc_initializer_sort;
};  /* an_ifc_sort_metadata */


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprRead nodes.
*/
template<>
struct an_ifc_sort_metadata<an_ifc_expr_read> {
  using return_type = an_ifc_read_conversion_sort;
};  /* an_ifc_sort_metadata */


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of the return type for NestableWord nodes.
*/
template<>
struct an_ifc_sort_metadata<an_ifc_nestable_word> {
  using return_type = an_ifc_word_sort;
};  /* an_ifc_sort_metadata */


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of the return type for NoexceptSpecification nodes.
*/
template<>
struct an_ifc_sort_metadata<an_ifc_noexcept_specification> {
  using return_type = an_ifc_noexcept_sort;
};  /* an_ifc_sort_metadata */


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SourceWord nodes.
*/
template<>
struct an_ifc_sort_metadata<an_ifc_source_word> {
  using return_type = an_ifc_word_sort;
};  /* an_ifc_sort_metadata */


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxLabeledStatement nodes.
*/
template<>
struct an_ifc_sort_metadata<an_ifc_syntax_labeled_statement> {
  using return_type = an_ifc_label_sort;
};  /* an_ifc_sort_metadata */


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxParameterDeclarator nodes.
*/
template<>
struct an_ifc_sort_metadata<an_ifc_syntax_parameter_declarator> {
  using return_type = an_ifc_parameter_sort;
};  /* an_ifc_sort_metadata */


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxPointerDeclarator nodes.
*/
template<>
struct an_ifc_sort_metadata<an_ifc_syntax_pointer_declarator> {
  using return_type = an_ifc_pointer_declarator_sort;
};  /* an_ifc_sort_metadata */


/*
The IFC sort field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxReturnStatement nodes.
*/
template<>
struct an_ifc_sort_metadata<an_ifc_syntax_return_statement> {
  using return_type = an_ifc_return_sort;
};  /* an_ifc_sort_metadata */


/*
The IFC source field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_source_metadata;


/*
The IFC source field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclDeductionGuide nodes.
*/
template<>
struct an_ifc_source_metadata<an_ifc_decl_deduction_guide> {
  using return_type = an_ifc_chart_index;
};  /* an_ifc_source_metadata */


/*
The IFC source field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCast nodes.
*/
template<>
struct an_ifc_source_metadata<an_ifc_expr_cast> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_source_metadata */


/*
The IFC source field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprHierarchyConversion nodes.
*/
template<>
struct an_ifc_source_metadata<an_ifc_expr_hierarchy_conversion> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_source_metadata */


/*
The IFC source field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeFunction nodes.
*/
template<>
struct an_ifc_source_metadata<an_ifc_type_function> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_source_metadata */


/*
The IFC source field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeMethod nodes.
*/
template<>
struct an_ifc_source_metadata<an_ifc_type_method> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_source_metadata */


/*
The IFC source field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeTor nodes.
*/
template<>
struct an_ifc_source_metadata<an_ifc_type_tor> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_source_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of those return types based on the respective node
type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_specifiers_metadata;


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclAlias nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_alias> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclBitfield nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_bitfield> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclConcept nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_concept> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclConstructor nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_constructor> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclDeductionGuide nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_deduction_guide> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclDestructor nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_destructor> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclEnumeration nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_enumeration> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclEnumerator nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_enumerator> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclField nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_field> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclFunction nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_function> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclInheritedConstructor
nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_inherited_constructor> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclIntrinsic nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_intrinsic> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclMethod nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_method> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclPartialSpecialization
nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_partial_specialization> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclScope nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_scope> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclTemplate nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_template> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclUsingDeclaration nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_using_declaration> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for DeclVariable nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_decl_variable> {
  using return_type = an_ifc_basic_specifiers_bitfield;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for SyntaxForRangeDeclaration
nodes.
*/
template<>
struct an_ifc_specifiers_metadata<an_ifc_syntax_for_range_declaration> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_specifiers_metadata */


/*
The IFC specifiers field has multiple return types.  This type is a metadata
type that allows resolution of the return type for
SyntaxStructuredBindingDeclaration nodes.
*/
template<>
struct an_ifc_specifiers_metadata<
                                an_ifc_syntax_structured_binding_declaration> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_specifiers_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_start_metadata;


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for AttrTuple nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_attr_tuple> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ChartMultilevel nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_chart_multilevel> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ChartUnilevel nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_chart_unilevel> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ConstStr nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_const_str> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclTuple nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_decl_tuple> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTuple nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_expr_tuple> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FormTuple nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_form_tuple> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ScopeDescriptor nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_scope_descriptor> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for Sequence nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_sequence> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SourceSentence nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_source_sentence> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtBlock nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_stmt_block> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTuple nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_syntax_tuple> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC start field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeTuple nodes.
*/
template<>
struct an_ifc_start_metadata<an_ifc_type_tuple> {
  using return_type = an_ifc_index;
};  /* an_ifc_start_metadata */


/*
The IFC stmt field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_stmt_metadata;


/*
The IFC stmt field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtLabeled nodes.
*/
template<>
struct an_ifc_stmt_metadata<an_ifc_stmt_labeled> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_stmt_metadata */


/*
The IFC stmt field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxAttributedStatement nodes.
*/
template<>
struct an_ifc_stmt_metadata<an_ifc_syntax_attributed_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_stmt_metadata */


/*
The IFC stmt field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxLabeledStatement nodes.
*/
template<>
struct an_ifc_stmt_metadata<an_ifc_syntax_labeled_statement> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_stmt_metadata */


/*
The IFC subject field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_subject_metadata;


/*
The IFC subject field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTemplateDeclaration nodes.
*/
template<>
struct an_ifc_subject_metadata<an_ifc_syntax_template_declaration> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_subject_metadata */


/*
The IFC subject field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeForall nodes.
*/
template<>
struct an_ifc_subject_metadata<an_ifc_type_forall> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_subject_metadata */


/*
The IFC target field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_target_metadata;


/*
The IFC target field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclDeductionGuide nodes.
*/
template<>
struct an_ifc_target_metadata<an_ifc_decl_deduction_guide> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_target_metadata */


/*
The IFC target field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCast nodes.
*/
template<>
struct an_ifc_target_metadata<an_ifc_expr_cast> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_target_metadata */


/*
The IFC target field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprHierarchyConversion nodes.
*/
template<>
struct an_ifc_target_metadata<an_ifc_expr_hierarchy_conversion> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_target_metadata */


/*
The IFC target field has multiple return types.  This type is a metadata type
that allows resolution of the return type for NameConversion nodes.
*/
template<>
struct an_ifc_target_metadata<an_ifc_name_conversion> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_target_metadata */


/*
The IFC target field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtGoto nodes.
*/
template<>
struct an_ifc_target_metadata<an_ifc_stmt_goto> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_target_metadata */


/*
The IFC target field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxGotoStatement nodes.
*/
template<>
struct an_ifc_target_metadata<an_ifc_syntax_goto_statement> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_target_metadata */


/*
The IFC target field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxNamespaceAliasDefinition
nodes.
*/
template<>
struct an_ifc_target_metadata<an_ifc_syntax_namespace_alias_definition> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_target_metadata */


/*
The IFC target field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTrailingReturnType nodes.
*/
template<>
struct an_ifc_target_metadata<an_ifc_syntax_trailing_return_type> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_target_metadata */


/*
The IFC target field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeFunction nodes.
*/
template<>
struct an_ifc_target_metadata<an_ifc_type_function> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_target_metadata */


/*
The IFC target field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeMethod nodes.
*/
template<>
struct an_ifc_target_metadata<an_ifc_type_method> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_target_metadata */


/*
The IFC trait field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_trait_metadata;


/*
The IFC trait field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitAliasTemplate nodes.
*/
template<>
struct an_ifc_trait_metadata<an_ifc_trait_alias_template> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_trait_metadata */


/*
The IFC trait field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitAttribute nodes.
*/
template<>
struct an_ifc_trait_metadata<an_ifc_trait_attribute> {
  using return_type = an_ifc_attr_index;
};  /* an_ifc_trait_metadata */


/*
The IFC trait field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitDeductionGuide nodes.
*/
template<>
struct an_ifc_trait_metadata<an_ifc_trait_deduction_guide> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_trait_metadata */


/*
The IFC trait field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitDeprecated nodes.
*/
template<>
struct an_ifc_trait_metadata<an_ifc_trait_deprecated> {
  using return_type = an_ifc_text_offset;
};  /* an_ifc_trait_metadata */


/*
The IFC trait field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitFriend nodes.
*/
template<>
struct an_ifc_trait_metadata<an_ifc_trait_friend> {
  using return_type = an_ifc_sequence;
};  /* an_ifc_trait_metadata */


/*
The IFC trait field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitMsvcDeclAttrs nodes.
*/
template<>
struct an_ifc_trait_metadata<an_ifc_trait_msvc_decl_attrs> {
  using return_type = an_ifc_attr_index;
};  /* an_ifc_trait_metadata */


/*
The IFC trait field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitMsvcVendorTrait nodes.
*/
template<>
struct an_ifc_trait_metadata<an_ifc_trait_msvc_vendor_trait> {
  using return_type = an_ifc_msvc_traits_bitfield;
};  /* an_ifc_trait_metadata */


/*
The IFC trait field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitRequires nodes.
*/
template<>
struct an_ifc_trait_metadata<an_ifc_trait_requires> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_trait_metadata */


/*
The IFC trait field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TraitSpecialization nodes.
*/
template<>
struct an_ifc_trait_metadata<an_ifc_trait_specialization> {
  using return_type = an_ifc_sequence;
};  /* an_ifc_trait_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_traits_metadata;


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclBitfield nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_decl_bitfield> {
  using return_type = an_ifc_object_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclConstructor nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_decl_constructor> {
  using return_type = an_ifc_function_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclDeductionGuide nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_decl_deduction_guide> {
  using return_type = an_ifc_guide_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclDestructor nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_decl_destructor> {
  using return_type = an_ifc_function_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclField nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_decl_field> {
  using return_type = an_ifc_object_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclFunction nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_decl_function> {
  using return_type = an_ifc_function_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclInheritedConstructor nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_decl_inherited_constructor> {
  using return_type = an_ifc_function_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclMethod nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_decl_method> {
  using return_type = an_ifc_function_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclOutputSegment nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_decl_output_segment> {
  using return_type = an_ifc_segment_traits;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclScope nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_decl_scope> {
  using return_type = an_ifc_scope_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclVariable nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_decl_variable> {
  using return_type = an_ifc_object_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxFunctionDeclarator nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_syntax_function_declarator> {
  using return_type = an_ifc_function_type_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeFunction nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_type_function> {
  using return_type = an_ifc_function_type_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC traits field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeMethod nodes.
*/
template<>
struct an_ifc_traits_metadata<an_ifc_type_method> {
  using return_type = an_ifc_function_type_traits_bitfield;
};  /* an_ifc_traits_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_type_metadata;


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclAlias nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_alias> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclBitfield nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_bitfield> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclConcept nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_concept> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclConstructor nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_constructor> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclEnumeration nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_enumeration> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclEnumerator nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_enumerator> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclField nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_field> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclFunction nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_function> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclInheritedConstructor nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_inherited_constructor> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclIntrinsic nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_intrinsic> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclMethod nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_method> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclOutputSegment nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_output_segment> {
  using return_type = an_ifc_segment_type;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclParameter nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_parameter> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclScope nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_scope> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclTemplate nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_template> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclVariable nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_decl_variable> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprAlignof nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_alignof> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprArrayValue nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_array_value> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprBinaryFold nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_binary_fold> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCall nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_call> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCast nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_cast> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCompoundString nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_compound_string> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprCondition nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_condition> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprDesignatedInitializer nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_designated_initializer> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprDestructorCall nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_destructor_call> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprDyad nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_dyad> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprDynamicDispatch nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_dynamic_dispatch> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprEmpty nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_empty> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprExpansion nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_expansion> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprFunctionString nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_function_string> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprHierarchyConversion nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_hierarchy_conversion> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprInheritancePath nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_inheritance_path> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprInitializer nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_initializer> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprInitializerList nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_initializer_list> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprLabel nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_label> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprLiteral nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_literal> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMemberAccess nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_member_access> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMemberInitializer nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_member_initializer> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprMonad nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_monad> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprNamedDecl nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_named_decl> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprNullptr nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_nullptr> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprPackedTemplateArguments
nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_packed_template_arguments> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprPath nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_path> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprPlaceholder nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_placeholder> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprProductTypeValue nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_product_type_value> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprPushState nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_push_state> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprQualifiedName nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_qualified_name> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprRead nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_read> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprRequires nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_requires> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprSimpleIdentifier nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_simple_identifier> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprSizeofType nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_sizeof_type> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprString nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_string> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprStringSequence nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_string_sequence> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprSumTypeValue nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_sum_type_value> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTemplateId nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_template_id> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTemplateReference nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_template_reference> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTemporary nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_temporary> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprThis nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_this> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTokens nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_tokens> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTriad nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_triad> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTuple nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_tuple> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprType nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_type> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTypeTraitIntrinsic nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_type_trait_intrinsic> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprTypeid nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_typeid> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprUnaryFold nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_unary_fold> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprUnqualifiedId nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_unqualified_id> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprUnresolvedId nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_unresolved_id> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprVirtualFunctionConversion
nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_expr_virtual_function_conversion> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtLabeled nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_stmt_labeled> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for StmtReturn nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_stmt_return> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxDeclSpecifierSeq nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_syntax_decl_specifier_seq> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxSimpleTypeSpecifier nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_syntax_simple_type_specifier> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTypeRequirement nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_syntax_type_requirement> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SyntaxTypeSpecifierSeq nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_syntax_type_specifier_seq> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeBase nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_type_base> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC type field has multiple return types.  This type is a metadata type
that allows resolution of the return type for TypeUnaligned nodes.
*/
template<>
struct an_ifc_type_metadata<an_ifc_type_unaligned> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_type_metadata */


/*
The IFC unit field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_unit_metadata;


/*
The IFC unit field has multiple return types.  This type is a metadata type
that allows resolution of the return type for DeclReference nodes.
*/
template<>
struct an_ifc_unit_metadata<an_ifc_decl_reference> {
  using return_type = an_ifc_module_reference;
};  /* an_ifc_unit_metadata */


/*
The IFC unit field has multiple return types.  This type is a metadata type
that allows resolution of the return type for FileHeader nodes.
*/
template<>
struct an_ifc_unit_metadata<an_ifc_file_header> {
  using return_type = an_ifc_unit_index;
};  /* an_ifc_unit_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of those return types based on the respective node type.
*/
template<typename an_ifc_Node_type>
struct an_ifc_value_metadata;


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ConstF64 nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_const_f64> {
  using return_type = an_ifc_ieeele_float;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ConstI64 nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_const_i64> {
  using return_type = an_ifc_u64;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprLiteral nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_expr_literal> {
  using return_type = an_ifc_lit_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprSubobjectValue nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_expr_subobject_value> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for ExprSumTypeValue nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_expr_sum_type_value> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for HeapAttr nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_heap_attr> {
  using return_type = an_ifc_attr_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for HeapChart nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_heap_chart> {
  using return_type = an_ifc_chart_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for HeapDecl nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_heap_decl> {
  using return_type = an_ifc_decl_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for HeapExpr nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_heap_expr> {
  using return_type = an_ifc_expr_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for HeapForm nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_heap_form> {
  using return_type = an_ifc_form_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for HeapPPForm nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_heap_pp_form> {
  using return_type = an_ifc_form_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for HeapStmt nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_heap_stmt> {
  using return_type = an_ifc_stmt_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for HeapSyntax nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_heap_syntax> {
  using return_type = an_ifc_syntax_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for HeapType nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_heap_type> {
  using return_type = an_ifc_type_index;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for KeywordSyntax nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_keyword_syntax> {
  using return_type = an_ifc_keyword_sort;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for NestableWord nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_nestable_word> {
  using return_type = an_ifc_u16;
};  /* an_ifc_value_metadata */


/*
The IFC value field has multiple return types.  This type is a metadata type
that allows resolution of the return type for SourceWord nodes.
*/
template<>
struct an_ifc_value_metadata<an_ifc_source_word> {
  using return_type = an_ifc_u16;
};  /* an_ifc_value_metadata */


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_ID(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_ID(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_abi(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_abi get_ifc_abi(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_abstract_declarator(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_abstract_declarator(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_access(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_access_metadata<an_ifc_Node_type>::return_type
get_ifc_access(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_address(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_address(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_aliasee(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_aliasee_metadata<an_ifc_Node_type>::return_type
get_ifc_aliasee(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_alignment(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_alignment(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_alternative(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_alternative_metadata<an_ifc_Node_type>::return_type
get_ifc_alternative(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_ampersand(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_ampersand(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_arch(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_architecture_sort get_ifc_arch(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_argument(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_argument_metadata<an_ifc_Node_type>::return_type
get_ifc_argument(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_argument_0(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_argument_0(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_argument_1(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_argument_1(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_argument_2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_argument_2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_argument_clause(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_argument_clause(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_arguments(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_arguments_metadata<an_ifc_Node_type>::return_type
get_ifc_arguments(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_arity_variadic(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_variadic_arity get_ifc_arity_variadic(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_array(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_array(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_array_or_function(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_array_or_function(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_arrow(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_arrow(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_assign(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_assign(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_assoc(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_assoc_metadata<an_ifc_Node_type>::return_type
get_ifc_assoc(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_associativity(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_associativity get_ifc_associativity(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_asterisk(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_asterisk(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_attribute(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_attr_index get_ifc_attribute(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_attributes(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_attributes_metadata<an_ifc_Node_type>::return_type
get_ifc_attributes(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_base(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_base_metadata<an_ifc_Node_type>::return_type
get_ifc_base(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_base_ctor(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_decl_index get_ifc_base_ctor(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_base_specifiers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_base_specifiers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_base_subobjects(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_base_subobjects(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_bases(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_bases(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_basis(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_basis_sort get_ifc_basis(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_bitwidth(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_bitwidth(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_body(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_body_metadata<an_ifc_Node_type>::return_type
get_ifc_body(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_bound(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_bound(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_break(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_break(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_by_ref(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_bool get_ifc_by_ref(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_callable(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_bool get_ifc_callable(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_captures(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_captures(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_cardinality(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_cardinality get_ifc_cardinality(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_catch(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_catch(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_category(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_word_category get_ifc_category(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_chart(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_chart_index get_ifc_chart(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_checksum(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_sha256 get_ifc_checksum(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_class_decl(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_index get_ifc_class_decl(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_class_key(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_keyword_syntax get_ifc_class_key(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_clause(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_clause(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_cleanup(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_destructor_sort get_ifc_cleanup(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_colon(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_colon(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_colons(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_colons(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_column(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_column get_ifc_column(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_comma(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_comma(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_concept_keyword(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_concept_keyword(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_condition(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_condition_metadata<an_ifc_Node_type>::return_type
get_ifc_condition(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_consequence(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_consequence_metadata<an_ifc_Node_type>::return_type
get_ifc_consequence(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_constexpr(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_constexpr(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_constraint(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_constraint_metadata<an_ifc_Node_type>::return_type
get_ifc_constraint(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_contents(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_contents(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_continuation(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_continuation_metadata<an_ifc_Node_type>::return_type
get_ifc_continuation(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_continue(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_continue(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_convention(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_calling_convention_sort get_ifc_convention(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_ctor_call(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_ctor_call(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_decl(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_decl_metadata<an_ifc_Node_type>::return_type
get_ifc_decl(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_decl_specifier(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_decl_specifier(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_decl_specifiers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_decl_specifiers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_declarations(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_declarations(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_declarator(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_declarator(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_declarators(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_declarators(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_declspec(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_sentence_index get_ifc_declspec(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_decltype_keyword(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_decltype_keyword(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_decltype_specifier(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_decltype_specifier(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_default_expr(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_default_expr(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_definition(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_definition(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_delimiter(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_delimiter_sort get_ifc_delimiter(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_denotation(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_index get_ifc_denotation(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_designator(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_designator(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_dialect(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_language_version get_ifc_dialect(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_direction(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_fold_direction_sort get_ifc_direction(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_discriminant(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_active_member get_ifc_discriminant(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_do(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_do(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_dtor_call(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_dtor_call(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_dyad(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_dyadic_operator_sort get_ifc_dyad(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_eh_spec(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_eh_spec_metadata<an_ifc_Node_type>::return_type
get_ifc_eh_spec(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_elaboration(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_index get_ifc_elaboration(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_element(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_index get_ifc_element(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_element_type(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_index get_ifc_element_type(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_elements(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_elements(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_ellipsis(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_ellipsis(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_else(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_else(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_enclosing(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_index get_ifc_enclosing(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_encoded(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_encoded(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_encoded_decl(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_encoded_decl_index get_ifc_encoded_decl(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_entity(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_entity_metadata<an_ifc_Node_type>::return_type
get_ifc_entity(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_entry_size(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_entity_size get_ifc_entry_size(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_enum_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_enum_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_enumerators(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_enumerators(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_equal(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_equal(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_except_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_except_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_exception(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_exception_metadata<an_ifc_Node_type>::return_type
get_ifc_exception(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_expander(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_expander(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_explicit_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_explicit_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_expr(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_expr_metadata<an_ifc_Node_type>::return_type
get_ifc_expr(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_expression(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_expression(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_extent(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_extent(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_factor(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_nestable_word get_ifc_factor(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_file(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_name_index get_ifc_file(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_final_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_final_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_finally_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_finally_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_first(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_form_index get_ifc_first(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_flags(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_eh_flags get_ifc_flags(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_for(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_for(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_form(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_form_spec_index get_ifc_form(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_function(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_function_metadata<an_ifc_Node_type>::return_type
get_ifc_function(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_function_type(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_index get_ifc_function_type(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_generate(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_keyword_syntax get_ifc_generate(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_getter(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_getter(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_global_scope(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_scope_index get_ifc_global_scope(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_glyph_loci_1(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_glyph_loci_1(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_glyph_loci_2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_glyph_loci_2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_glyph_locus(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_glyph_locus(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_guard(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_guard(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_handler(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_handler(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_handlers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_handlers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_head(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_sentence_index get_ifc_head(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_hidden(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_bool get_ifc_hidden(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_home_scope(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_decl_index get_ifc_home_scope(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_id(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_unique_id get_ifc_id(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_if(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_if(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_impl(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_decl_index get_ifc_impl(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_index(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_index_metadata<an_ifc_Node_type>::return_type
get_ifc_index(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_inheritance(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_inheritance(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_init(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_init(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_initializaerion(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_initializaerion(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_initialization(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_initialization_metadata<an_ifc_Node_type>::return_type
get_ifc_initialization(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_initializer(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_initializer_metadata<an_ifc_Node_type>::return_type
get_ifc_initializer(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_initializers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_initializers_metadata<an_ifc_Node_type>::return_type
get_ifc_initializers(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_internal(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_bool get_ifc_internal(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_intrinsic(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_operator_category get_ifc_intrinsic(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_introducer(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_introducer(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_key(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_keyword_syntax get_ifc_key(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_keyword(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_keyword(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_label(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_label_metadata<an_ifc_Node_type>::return_type
get_ifc_label(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_leave_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_leave_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_left(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_left_metadata<an_ifc_Node_type>::return_type
get_ifc_left(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_left_angle(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_left_angle(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_left_brace(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_left_brace(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_left_bracket(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_left_bracket(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_left_curly(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_left_curly(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_left_paren(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_left_paren_metadata<an_ifc_Node_type>::return_type
get_ifc_left_paren(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_left_paren_1(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_left_paren_1(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_left_paren_2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_left_paren_2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_length(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_cardinality get_ifc_length(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_level(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_parameter_level get_ifc_level(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_line(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_line_metadata<an_ifc_Node_type>::return_type
get_ifc_line(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_local_index(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_decl_foreign_index get_ifc_local_index(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_locus(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_locus_metadata<an_ifc_Node_type>::return_type
get_ifc_locus(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_macro(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_macro(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_major_version(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_version get_ifc_major_version(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_member(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_member_metadata<an_ifc_Node_type>::return_type
get_ifc_member(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_member_declarations(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_member_declarations(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_member_locus(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_member_locus(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_member_name(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_name_index get_ifc_member_name(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_members(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_members_metadata<an_ifc_Node_type>::return_type
get_ifc_members(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_message(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_message(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_minor_version(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_version get_ifc_minor_version(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_mode(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expansion_mode_sort get_ifc_mode(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_modifier(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_keyword_sort get_ifc_modifier(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_name(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_name_metadata<an_ifc_Node_type>::return_type
get_ifc_name(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_name2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_name2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_names(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_names(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_namespace_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_namespace_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_next(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_next(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_noexcept_loc(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_noexcept_loc(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_offset(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_offset_metadata<an_ifc_Node_type>::return_type
get_ifc_offset(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_op(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_op_metadata<an_ifc_Node_type>::return_type get_ifc_op(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_operand(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_operand_metadata<an_ifc_Node_type>::return_type
get_ifc_operand(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_operand_1(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_operand_1(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_operand_2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_operand_2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_operation(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_operation_metadata<an_ifc_Node_type>::return_type
get_ifc_operation(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_operator(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_operator_category get_ifc_operator(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_override(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_override(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_override_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_override_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_owner(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_owner(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_pack(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_pack_metadata<an_ifc_Node_type>::return_type
get_ifc_pack(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_pack_expanded(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_bool get_ifc_pack_expanded(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_pack_size(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_pack_size get_ifc_pack_size(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_parameters(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_parameters_metadata<an_ifc_Node_type>::return_type
get_ifc_parameters(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_params(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_chart_index get_ifc_params(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_parent(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_parent(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_parenthesized(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_parenthesized(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_partition(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_partition(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_partition_count(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_cardinality get_ifc_partition_count(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_path(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_path_metadata<an_ifc_Node_type>::return_type
get_ifc_path(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_pivot(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_pivot(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_pointee(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_index get_ifc_pointee(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_pointer(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_pointer(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_position(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_parameter_position get_ifc_position(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_pragam(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_sentence_index get_ifc_pragam(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_pragma(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_sentence_index get_ifc_pragma(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_precision(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_precision_sort get_ifc_precision(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_prefix(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_prefix_metadata<an_ifc_Node_type>::return_type
get_ifc_prefix(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_primary(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_primary_metadata<an_ifc_Node_type>::return_type
get_ifc_primary(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_primary_template(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_decl_index get_ifc_primary_template(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_properties(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_reachable_properties_bitfield get_ifc_properties(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_pure(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_bool get_ifc_pure(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_qualified_name(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_qualified_name(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_qualifiers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_qualifier_bitfield get_ifc_qualifiers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_ref(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_ref(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_referee(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_index get_ifc_referee(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_reference(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_module_reference get_ifc_reference(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_requirements(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_requirements(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_resolution(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_resolution_metadata<an_ifc_Node_type>::return_type
get_ifc_resolution(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_return(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_return(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_right(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_right_metadata<an_ifc_Node_type>::return_type
get_ifc_right(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_right_angle(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_right_angle(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_right_brace(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_right_brace(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_right_bracket(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_right_bracket(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_right_curly(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_right_curly(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_right_paren(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_right_paren_metadata<an_ifc_Node_type>::return_type
get_ifc_right_paren(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_right_paren_1(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_right_paren_1(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_right_paren_2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_right_paren_2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_scope(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_scope_metadata<an_ifc_Node_type>::return_type
get_ifc_scope(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_second(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_form_index get_ifc_second(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_semicolon(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_semicolon(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_setter(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_setter(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_shared(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_bool get_ifc_shared(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_sign(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_sign_sort get_ifc_sign(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_sort(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_sort_metadata<an_ifc_Node_type>::return_type
get_ifc_sort(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_source(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_source_metadata<an_ifc_Node_type>::return_type
get_ifc_source(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_specifiers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_specifiers_metadata<an_ifc_Node_type>::return_type
get_ifc_specifiers(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_spelling(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_spelling(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_src_path(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_src_path(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_start(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_start_metadata<an_ifc_Node_type>::return_type
get_ifc_start(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_stmt(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_stmt_metadata<an_ifc_Node_type>::return_type
get_ifc_stmt(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_stmts(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_stmts(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_storage_class(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_storage_class get_ifc_storage_class(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_string(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_string(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_string_index(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_string_index get_ifc_string_index(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_string_table_bytes(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_byte_offset get_ifc_string_table_bytes(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_string_table_size(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_cardinality get_ifc_string_table_size(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_strings(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_strings(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_subject(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_subject_metadata<an_ifc_Node_type>::return_type
get_ifc_subject(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_suffix(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_text_offset get_ifc_suffix(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_switch(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_switch(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_symbol(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_symbol(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_syntax(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_syntax(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_synthesis(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_keyword_syntax get_ifc_synthesis(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_target(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_target_metadata<an_ifc_Node_type>::return_type
get_ifc_target(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_template_keyword(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_template_keyword(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_template_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_template_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_template_parameters(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_template_parameters(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_terms(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_attr_index get_ifc_terms(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_throw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_throw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_toc(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_byte_offset get_ifc_toc(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_tokens(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_sentence_index get_ifc_tokens(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_trailing_target(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_trailing_target(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_trait(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_trait_metadata<an_ifc_Node_type>::return_type
get_ifc_trait(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_traits(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_traits_metadata<an_ifc_Node_type>::return_type
get_ifc_traits(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_try(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_try(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_try_block(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_try_block(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_try_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_try_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_type(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_type_metadata<an_ifc_Node_type>::return_type
get_ifc_type(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_type_id(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_type_id(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_type_list(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_type_list(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_type_name(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_type_name(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_type_specifier(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_type_specifier(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_type_specifiers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_type_specifiers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_typename_keyword(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_typename_keyword(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_typename_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_typename_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_unhashed(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_bool get_ifc_unhashed(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_unit(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_unit_metadata<an_ifc_Node_type>::return_type
get_ifc_unit(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_unknown(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_u16 get_ifc_unknown(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_unqualified(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_type_index get_ifc_unqualified(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_using_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_using_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_uuid(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_uuid get_ifc_uuid(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_value(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern typename an_ifc_value_metadata<an_ifc_Node_type>::return_type
get_ifc_value(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_variant(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_decl_index get_ifc_variant(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_virtual_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_virtual_kw(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_virtual_kw2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_virtual_kw2(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_virtual_specifiers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_virtual_specifiers(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_while(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_source_location get_ifc_while(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_whole(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_syntax_index get_ifc_whole(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_width(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_expr_index get_ifc_width(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_word(const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_nestable_word get_ifc_word(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean has_ifc_words(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern an_ifc_sentence_index get_ifc_words(
                              const an_ifc_Node_type &universal) DELETED_FN_DEF


template<typename an_ifc_Node_type>
extern a_boolean validate(
                       const an_ifc_Node_type        &universal,
                       const an_ifc_validation_trace *parent) DELETED_FN_DEF

/*
Functions for interacting with IFC KeywordSyntax nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_keyword_syntax &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_keyword_syntax &universal);

template<>
a_boolean has_ifc_value(const an_ifc_keyword_syntax &universal);

template<>
an_ifc_keyword_sort get_ifc_value(const an_ifc_keyword_syntax &universal);

template<>
a_boolean validate(const an_ifc_keyword_syntax   &universal,
                   const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_keyword_syntax &universal, unsigned indent);

extern void db_node(const an_ifc_keyword_syntax &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC ModuleReference nodes.
*/

template<>
a_boolean has_ifc_owner(const an_ifc_module_reference &universal);

template<>
an_ifc_text_offset get_ifc_owner(const an_ifc_module_reference &universal);

template<>
a_boolean has_ifc_partition(const an_ifc_module_reference &universal);

template<>
an_ifc_text_offset get_ifc_partition(const an_ifc_module_reference &universal);

template<>
a_boolean validate(const an_ifc_module_reference &universal,
                   const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_module_reference &universal, unsigned indent);

extern void db_node(const an_ifc_module_reference &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC NestableWord nodes.
*/

template<>
a_boolean has_ifc_category(const an_ifc_nestable_word &universal);

template<>
an_ifc_word_category get_ifc_category(const an_ifc_nestable_word &universal);

template<>
a_boolean has_ifc_index(const an_ifc_nestable_word &universal);

template<>
an_ifc_index get_ifc_index(const an_ifc_nestable_word &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_nestable_word &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_nestable_word &universal);

template<>
a_boolean has_ifc_sort(const an_ifc_nestable_word &universal);

template<>
an_ifc_word_sort get_ifc_sort(const an_ifc_nestable_word &universal);

template<>
a_boolean has_ifc_value(const an_ifc_nestable_word &universal);

template<>
an_ifc_u16 get_ifc_value(const an_ifc_nestable_word &universal);

template<>
a_boolean validate(const an_ifc_nestable_word    &universal,
                   const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_nestable_word &universal, unsigned indent);

extern void db_node(const an_ifc_nestable_word &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC NoexceptSpecification nodes.
*/

template<>
a_boolean has_ifc_sort(const an_ifc_noexcept_specification &universal);

template<>
an_ifc_noexcept_sort get_ifc_sort(
                               const an_ifc_noexcept_specification &universal);

template<>
a_boolean has_ifc_words(const an_ifc_noexcept_specification &universal);

template<>
an_ifc_sentence_index get_ifc_words(
                               const an_ifc_noexcept_specification &universal);

template<>
a_boolean validate(const an_ifc_noexcept_specification &universal,
                   const an_ifc_validation_trace       *parent);

#if DEBUG
extern void db_node(const an_ifc_noexcept_specification &universal,
                    unsigned                            indent);

extern void db_node(const an_ifc_noexcept_specification &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC ParameterizedEntity nodes.
*/

template<>
a_boolean has_ifc_attributes(const an_ifc_parameterized_entity &universal);

template<>
an_ifc_sentence_index get_ifc_attributes(
                                 const an_ifc_parameterized_entity &universal);

template<>
a_boolean has_ifc_body(const an_ifc_parameterized_entity &universal);

template<>
an_ifc_sentence_index get_ifc_body(
                                 const an_ifc_parameterized_entity &universal);

template<>
a_boolean has_ifc_decl(const an_ifc_parameterized_entity &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_parameterized_entity &universal);

template<>
a_boolean has_ifc_head(const an_ifc_parameterized_entity &universal);

template<>
an_ifc_sentence_index get_ifc_head(
                                 const an_ifc_parameterized_entity &universal);

template<>
a_boolean validate(const an_ifc_parameterized_entity &universal,
                   const an_ifc_validation_trace     *parent);

#if DEBUG
extern void db_node(const an_ifc_parameterized_entity &universal,
                    unsigned                          indent);

extern void db_node(const an_ifc_parameterized_entity &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC Sequence nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_sequence &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(const an_ifc_sequence &universal);

template<>
a_boolean has_ifc_start(const an_ifc_sequence &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_sequence &universal);

template<>
a_boolean validate(const an_ifc_sequence         &universal,
                   const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_sequence &universal, unsigned indent);

extern void db_node(const an_ifc_sequence &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC SourceLocation nodes.
*/

template<>
a_boolean has_ifc_column(const an_ifc_source_location &universal);

template<>
an_ifc_column get_ifc_column(const an_ifc_source_location &universal);

template<>
a_boolean has_ifc_line(const an_ifc_source_location &universal);

template<>
an_ifc_line_index get_ifc_line(const an_ifc_source_location &universal);

template<>
a_boolean validate(const an_ifc_source_location  &universal,
                   const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_source_location &universal, unsigned indent);

extern void db_node(const an_ifc_source_location &universal);
#endif /* DEBUG */

/*
Functions for interacting with IFC FileHeader nodes.
*/

template<>
a_boolean has_ifc_abi(const an_ifc_file_header &universal);

template<>
an_ifc_abi get_ifc_abi(const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_arch(const an_ifc_file_header &universal);

template<>
an_ifc_architecture_sort get_ifc_arch(const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_checksum(const an_ifc_file_header &universal);

template<>
an_ifc_sha256 get_ifc_checksum(const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_dialect(const an_ifc_file_header &universal);

template<>
an_ifc_language_version get_ifc_dialect(const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_global_scope(const an_ifc_file_header &universal);

template<>
an_ifc_scope_index get_ifc_global_scope(const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_internal(const an_ifc_file_header &universal);

template<>
an_ifc_bool get_ifc_internal(const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_major_version(const an_ifc_file_header &universal);

template<>
an_ifc_version get_ifc_major_version(const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_minor_version(const an_ifc_file_header &universal);

template<>
an_ifc_version get_ifc_minor_version(const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_partition_count(const an_ifc_file_header &universal);

template<>
an_ifc_cardinality get_ifc_partition_count(
                                          const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_src_path(const an_ifc_file_header &universal);

template<>
an_ifc_text_offset get_ifc_src_path(const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_string_table_bytes(const an_ifc_file_header &universal);

template<>
an_ifc_byte_offset get_ifc_string_table_bytes(
                                          const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_string_table_size(const an_ifc_file_header &universal);

template<>
an_ifc_cardinality get_ifc_string_table_size(
                                          const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_toc(const an_ifc_file_header &universal);

template<>
an_ifc_byte_offset get_ifc_toc(const an_ifc_file_header &universal);

template<>
a_boolean has_ifc_unit(const an_ifc_file_header &universal);

template<>
an_ifc_unit_index get_ifc_unit(const an_ifc_file_header &universal);

template<>
a_boolean validate(const an_ifc_file_header      &universal,
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

template<>
a_boolean has_ifc_cardinality(const an_ifc_partition &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(const an_ifc_partition &universal);

template<>
a_boolean has_ifc_entry_size(const an_ifc_partition &universal);

template<>
an_ifc_entity_size get_ifc_entry_size(const an_ifc_partition &universal);

template<>
a_boolean has_ifc_name(const an_ifc_partition &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_partition &universal);

template<>
a_boolean has_ifc_offset(const an_ifc_partition &universal);

template<>
an_ifc_byte_offset get_ifc_offset(const an_ifc_partition &universal);

template<>
a_boolean validate(const an_ifc_partition        &universal,
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

template<>
a_boolean has_ifc_word(const an_ifc_attr_basic &universal);

template<>
an_ifc_nestable_word get_ifc_word(const an_ifc_attr_basic &universal);

template<>
a_boolean validate(const an_ifc_attr_basic       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_attr_basic>();

/*
Functions for interacting with IFC AttrCalled nodes.
*/

template<>
a_boolean has_ifc_arguments(const an_ifc_attr_called &universal);

template<>
an_ifc_attr_index get_ifc_arguments(const an_ifc_attr_called &universal);

template<>
a_boolean has_ifc_function(const an_ifc_attr_called &universal);

template<>
an_ifc_attr_index get_ifc_function(const an_ifc_attr_called &universal);

template<>
a_boolean validate(const an_ifc_attr_called      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_attr_called>();

/*
Functions for interacting with IFC AttrElaborated nodes.
*/

template<>
a_boolean has_ifc_expression(const an_ifc_attr_elaborated &universal);

template<>
an_ifc_expr_index get_ifc_expression(const an_ifc_attr_elaborated &universal);

template<>
a_boolean validate(const an_ifc_attr_elaborated  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_attr_elaborated>();

/*
Functions for interacting with IFC AttrExpanded nodes.
*/

template<>
a_boolean has_ifc_operand(const an_ifc_attr_expanded &universal);

template<>
an_ifc_attr_index get_ifc_operand(const an_ifc_attr_expanded &universal);

template<>
a_boolean validate(const an_ifc_attr_expanded    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_attr_expanded>();

/*
Functions for interacting with IFC AttrFactored nodes.
*/

template<>
a_boolean has_ifc_factor(const an_ifc_attr_factored &universal);

template<>
an_ifc_nestable_word get_ifc_factor(const an_ifc_attr_factored &universal);

template<>
a_boolean has_ifc_terms(const an_ifc_attr_factored &universal);

template<>
an_ifc_attr_index get_ifc_terms(const an_ifc_attr_factored &universal);

template<>
a_boolean validate(const an_ifc_attr_factored    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_attr_factored>();

/*
Functions for interacting with IFC AttrLabeled nodes.
*/

template<>
a_boolean has_ifc_attribute(const an_ifc_attr_labeled &universal);

template<>
an_ifc_attr_index get_ifc_attribute(const an_ifc_attr_labeled &universal);

template<>
a_boolean has_ifc_label(const an_ifc_attr_labeled &universal);

template<>
an_ifc_nestable_word get_ifc_label(const an_ifc_attr_labeled &universal);

template<>
a_boolean validate(const an_ifc_attr_labeled     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_attr_labeled>();

/*
Functions for interacting with IFC AttrScoped nodes.
*/

template<>
a_boolean has_ifc_member(const an_ifc_attr_scoped &universal);

template<>
an_ifc_nestable_word get_ifc_member(const an_ifc_attr_scoped &universal);

template<>
a_boolean has_ifc_scope(const an_ifc_attr_scoped &universal);

template<>
an_ifc_nestable_word get_ifc_scope(const an_ifc_attr_scoped &universal);

template<>
a_boolean validate(const an_ifc_attr_scoped      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_attr_scoped>();

/*
Functions for interacting with IFC AttrTuple nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_attr_tuple &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(const an_ifc_attr_tuple &universal);

template<>
a_boolean has_ifc_start(const an_ifc_attr_tuple &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_attr_tuple &universal);

template<>
a_boolean validate(const an_ifc_attr_tuple       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_attr_tuple>();

/*
Functions for interacting with IFC ChartMultilevel nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_chart_multilevel &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(
                                     const an_ifc_chart_multilevel &universal);

template<>
a_boolean has_ifc_start(const an_ifc_chart_multilevel &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_chart_multilevel &universal);

template<>
a_boolean validate(const an_ifc_chart_multilevel &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_chart_multilevel>();

/*
Functions for interacting with IFC ChartUnilevel nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_chart_unilevel &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(const an_ifc_chart_unilevel &universal);

template<>
a_boolean has_ifc_constraint(const an_ifc_chart_unilevel &universal);

template<>
an_ifc_expr_index get_ifc_constraint(const an_ifc_chart_unilevel &universal);

template<>
a_boolean has_ifc_start(const an_ifc_chart_unilevel &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_chart_unilevel &universal);

template<>
a_boolean validate(const an_ifc_chart_unilevel   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_chart_unilevel>();

/*
Functions for interacting with IFC ConstF64 nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_const_f64 &universal);

template<>
an_ifc_ieeele_float get_ifc_value(const an_ifc_const_f64 &universal);

template<>
a_boolean validate(const an_ifc_const_f64        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_const_f64>();

/*
Functions for interacting with IFC ConstI64 nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_const_i64 &universal);

template<>
an_ifc_u64 get_ifc_value(const an_ifc_const_i64 &universal);

template<>
a_boolean validate(const an_ifc_const_i64        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_const_i64>();

/*
Functions for interacting with IFC ConstStr nodes.
*/

template<>
a_boolean has_ifc_length(const an_ifc_const_str &universal);

template<>
an_ifc_cardinality get_ifc_length(const an_ifc_const_str &universal);

template<>
a_boolean has_ifc_start(const an_ifc_const_str &universal);

template<>
an_ifc_text_offset get_ifc_start(const an_ifc_const_str &universal);

template<>
a_boolean has_ifc_suffix(const an_ifc_const_str &universal);

template<>
an_ifc_text_offset get_ifc_suffix(const an_ifc_const_str &universal);

template<>
a_boolean validate(const an_ifc_const_str        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_const_str>();

/*
Functions for interacting with IFC DeclAlias nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_alias &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_alias &universal);

template<>
a_boolean has_ifc_aliasee(const an_ifc_decl_alias &universal);

template<>
an_ifc_type_index get_ifc_aliasee(const an_ifc_decl_alias &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_alias &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_alias &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_alias &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_alias &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_alias &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_decl_alias &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_alias &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                           const an_ifc_decl_alias &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_alias &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_alias &universal);

template<>
a_boolean validate(const an_ifc_decl_alias       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_alias>();

/*
Functions for interacting with IFC DeclBitfield nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_bitfield &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_bitfield &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_bitfield &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_bitfield &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_decl_bitfield &universal);

template<>
an_ifc_expr_index get_ifc_initializer(const an_ifc_decl_bitfield &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_bitfield &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_bitfield &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_bitfield &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_decl_bitfield &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_bitfield &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                        const an_ifc_decl_bitfield &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_bitfield &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                        const an_ifc_decl_bitfield &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_decl_bitfield &universal);

template<>
an_ifc_object_traits_bitfield get_ifc_traits(
                                        const an_ifc_decl_bitfield &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_bitfield &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_bitfield &universal);

template<>
a_boolean has_ifc_width(const an_ifc_decl_bitfield &universal);

template<>
an_ifc_expr_index get_ifc_width(const an_ifc_decl_bitfield &universal);

template<>
a_boolean validate(const an_ifc_decl_bitfield    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_bitfield>();

/*
Functions for interacting with IFC DeclConcept nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_concept &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_concept &universal);

template<>
a_boolean has_ifc_body(const an_ifc_decl_concept &universal);

template<>
an_ifc_sentence_index get_ifc_body(const an_ifc_decl_concept &universal);

template<>
a_boolean has_ifc_chart(const an_ifc_decl_concept &universal);

template<>
an_ifc_chart_index get_ifc_chart(const an_ifc_decl_concept &universal);

template<>
a_boolean has_ifc_constraint(const an_ifc_decl_concept &universal);

template<>
an_ifc_expr_index get_ifc_constraint(const an_ifc_decl_concept &universal);

template<>
a_boolean has_ifc_head(const an_ifc_decl_concept &universal);

template<>
an_ifc_sentence_index get_ifc_head(const an_ifc_decl_concept &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_concept &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_concept &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_concept &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_concept &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_concept &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_decl_concept &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_concept &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                         const an_ifc_decl_concept &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_concept &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_concept &universal);

template<>
a_boolean has_ifc_unknown(const an_ifc_decl_concept &universal);

template<>
an_ifc_u16 get_ifc_unknown(const an_ifc_decl_concept &universal);

template<>
a_boolean validate(const an_ifc_decl_concept     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_concept>();

/*
Functions for interacting with IFC DeclConstructor nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_constructor &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_constructor &universal);

template<>
a_boolean has_ifc_chart(const an_ifc_decl_constructor &universal);

template<>
an_ifc_chart_index get_ifc_chart(const an_ifc_decl_constructor &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_constructor &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_constructor &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_constructor &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_constructor &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_constructor &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_decl_constructor &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_constructor &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                     const an_ifc_decl_constructor &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_constructor &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                     const an_ifc_decl_constructor &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_decl_constructor &universal);

template<>
an_ifc_function_traits_bitfield get_ifc_traits(
                                     const an_ifc_decl_constructor &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_constructor &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_constructor &universal);

template<>
a_boolean validate(const an_ifc_decl_constructor &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_constructor>();

/*
Functions for interacting with IFC DeclDeductionGuide nodes.
*/

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_deduction_guide &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(
                                 const an_ifc_decl_deduction_guide &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_deduction_guide &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                 const an_ifc_decl_deduction_guide &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_deduction_guide &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_decl_deduction_guide &universal);

template<>
a_boolean has_ifc_source(const an_ifc_decl_deduction_guide &universal);

template<>
an_ifc_chart_index get_ifc_source(
                                 const an_ifc_decl_deduction_guide &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_deduction_guide &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                 const an_ifc_decl_deduction_guide &universal);

template<>
a_boolean has_ifc_target(const an_ifc_decl_deduction_guide &universal);

template<>
an_ifc_expr_index get_ifc_target(const an_ifc_decl_deduction_guide &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_decl_deduction_guide &universal);

template<>
an_ifc_guide_traits_bitfield get_ifc_traits(
                                 const an_ifc_decl_deduction_guide &universal);

template<>
a_boolean validate(const an_ifc_decl_deduction_guide &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_deduction_guide>();

/*
Functions for interacting with IFC DeclDestructor nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_destructor &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_destructor &universal);

template<>
a_boolean has_ifc_convention(const an_ifc_decl_destructor &universal);

template<>
an_ifc_calling_convention_sort get_ifc_convention(
                                      const an_ifc_decl_destructor &universal);

template<>
a_boolean has_ifc_eh_spec(const an_ifc_decl_destructor &universal);

template<>
an_ifc_noexcept_specification get_ifc_eh_spec(
                                      const an_ifc_decl_destructor &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_destructor &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_destructor &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_destructor &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_destructor &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_destructor &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_decl_destructor &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_destructor &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                      const an_ifc_decl_destructor &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_destructor &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                      const an_ifc_decl_destructor &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_decl_destructor &universal);

template<>
an_ifc_function_traits_bitfield get_ifc_traits(
                                      const an_ifc_decl_destructor &universal);

template<>
a_boolean validate(const an_ifc_decl_destructor  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_destructor>();

/*
Functions for interacting with IFC DeclEnumeration nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_enumeration &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_enumeration &universal);

template<>
a_boolean has_ifc_alignment(const an_ifc_decl_enumeration &universal);

template<>
an_ifc_expr_index get_ifc_alignment(const an_ifc_decl_enumeration &universal);

template<>
a_boolean has_ifc_base(const an_ifc_decl_enumeration &universal);

template<>
an_ifc_type_index get_ifc_base(const an_ifc_decl_enumeration &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_enumeration &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_enumeration &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_decl_enumeration &universal);

template<>
an_ifc_sequence get_ifc_initializer(const an_ifc_decl_enumeration &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_enumeration &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_enumeration &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_enumeration &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_decl_enumeration &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_enumeration &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                     const an_ifc_decl_enumeration &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_enumeration &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                     const an_ifc_decl_enumeration &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_enumeration &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_enumeration &universal);

template<>
a_boolean validate(const an_ifc_decl_enumeration &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_enumeration>();

/*
Functions for interacting with IFC DeclEnumerator nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_enumerator &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_enumerator &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_enumerator &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_enumerator &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_decl_enumerator &universal);

template<>
an_ifc_expr_index get_ifc_initializer(const an_ifc_decl_enumerator &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_enumerator &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_enumerator &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_enumerator &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_decl_enumerator &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_enumerator &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                      const an_ifc_decl_enumerator &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_enumerator &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_enumerator &universal);

template<>
a_boolean validate(const an_ifc_decl_enumerator  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_enumerator>();

/*
Functions for interacting with IFC DeclExpansion nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_decl_expansion &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_expansion &universal);

template<>
a_boolean has_ifc_operand(const an_ifc_decl_expansion &universal);

template<>
an_ifc_decl_index get_ifc_operand(const an_ifc_decl_expansion &universal);

template<>
a_boolean validate(const an_ifc_decl_expansion   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_expansion>();

/*
Functions for interacting with IFC DeclExplicitInstantiation nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_decl_explicit_instantiation &universal);

template<>
an_ifc_decl_index get_ifc_decl(
                          const an_ifc_decl_explicit_instantiation &universal);

template<>
a_boolean has_ifc_form(const an_ifc_decl_explicit_instantiation &universal);

template<>
an_ifc_form_spec_index get_ifc_form(
                          const an_ifc_decl_explicit_instantiation &universal);

template<>
a_boolean validate(const an_ifc_decl_explicit_instantiation &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_explicit_instantiation>();

/*
Functions for interacting with IFC DeclExplicitSpecialization nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_decl_explicit_specialization &universal);

template<>
an_ifc_decl_index get_ifc_decl(
                         const an_ifc_decl_explicit_specialization &universal);

template<>
a_boolean has_ifc_form(const an_ifc_decl_explicit_specialization &universal);

template<>
an_ifc_form_spec_index get_ifc_form(
                         const an_ifc_decl_explicit_specialization &universal);

template<>
a_boolean validate(const an_ifc_decl_explicit_specialization &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_explicit_specialization>();

/*
Functions for interacting with IFC DeclField nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_field &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_field &universal);

template<>
a_boolean has_ifc_alignment(const an_ifc_decl_field &universal);

template<>
an_ifc_expr_index get_ifc_alignment(const an_ifc_decl_field &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_field &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_field &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_decl_field &universal);

template<>
an_ifc_expr_index get_ifc_initializer(const an_ifc_decl_field &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_field &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_field &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_field &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_decl_field &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_field &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                           const an_ifc_decl_field &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_field &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                           const an_ifc_decl_field &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_decl_field &universal);

template<>
an_ifc_object_traits_bitfield get_ifc_traits(
                                           const an_ifc_decl_field &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_field &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_field &universal);

template<>
a_boolean validate(const an_ifc_decl_field       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_field>();

/*
Functions for interacting with IFC DeclFriend nodes.
*/

template<>
a_boolean has_ifc_entity(const an_ifc_decl_friend &universal);

template<>
an_ifc_expr_index get_ifc_entity(const an_ifc_decl_friend &universal);

template<>
a_boolean validate(const an_ifc_decl_friend      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_friend>();

/*
Functions for interacting with IFC DeclFunction nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_function &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_function &universal);

template<>
a_boolean has_ifc_chart(const an_ifc_decl_function &universal);

template<>
an_ifc_chart_index get_ifc_chart(const an_ifc_decl_function &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_function &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_function &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_function &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_function &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_function &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_decl_function &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_function &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                        const an_ifc_decl_function &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_function &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                        const an_ifc_decl_function &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_decl_function &universal);

template<>
an_ifc_function_traits_bitfield get_ifc_traits(
                                        const an_ifc_decl_function &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_function &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_function &universal);

template<>
a_boolean validate(const an_ifc_decl_function    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_function>();

/*
Functions for interacting with IFC DeclInheritedConstructor nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_inherited_constructor &universal);

template<>
an_ifc_access_sort get_ifc_access(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
a_boolean has_ifc_base_ctor(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
an_ifc_decl_index get_ifc_base_ctor(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
a_boolean has_ifc_chart(const an_ifc_decl_inherited_constructor &universal);

template<>
an_ifc_chart_index get_ifc_chart(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
a_boolean has_ifc_home_scope(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_inherited_constructor &universal);

template<>
an_ifc_source_location get_ifc_locus(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_inherited_constructor &universal);

template<>
an_ifc_text_offset get_ifc_name(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
a_boolean has_ifc_specifiers(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_decl_inherited_constructor &universal);

template<>
an_ifc_function_traits_bitfield get_ifc_traits(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_inherited_constructor &universal);

template<>
an_ifc_type_index get_ifc_type(
                           const an_ifc_decl_inherited_constructor &universal);

template<>
a_boolean validate(const an_ifc_decl_inherited_constructor &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_inherited_constructor>();

/*
Functions for interacting with IFC DeclIntrinsic nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_intrinsic &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_intrinsic &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_intrinsic &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_intrinsic &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_intrinsic &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_intrinsic &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_intrinsic &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_decl_intrinsic &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_intrinsic &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                       const an_ifc_decl_intrinsic &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_intrinsic &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_intrinsic &universal);

template<>
a_boolean validate(const an_ifc_decl_intrinsic   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_intrinsic>();

/*
Functions for interacting with IFC DeclMethod nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_method &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_method &universal);

template<>
a_boolean has_ifc_chart(const an_ifc_decl_method &universal);

template<>
an_ifc_chart_index get_ifc_chart(const an_ifc_decl_method &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_method &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_method &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_method &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_method &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_method &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_decl_method &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_method &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                          const an_ifc_decl_method &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_method &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                          const an_ifc_decl_method &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_decl_method &universal);

template<>
an_ifc_function_traits_bitfield get_ifc_traits(
                                          const an_ifc_decl_method &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_method &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_method &universal);

template<>
a_boolean validate(const an_ifc_decl_method      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_method>();

/*
Functions for interacting with IFC DeclOutputSegment nodes.
*/

template<>
a_boolean has_ifc_ID(const an_ifc_decl_output_segment &universal);

template<>
an_ifc_text_offset get_ifc_ID(const an_ifc_decl_output_segment &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_output_segment &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_decl_output_segment &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_decl_output_segment &universal);

template<>
an_ifc_segment_traits get_ifc_traits(
                                  const an_ifc_decl_output_segment &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_output_segment &universal);

template<>
an_ifc_segment_type get_ifc_type(const an_ifc_decl_output_segment &universal);

template<>
a_boolean validate(const an_ifc_decl_output_segment &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_output_segment>();

/*
Functions for interacting with IFC DeclParameter nodes.
*/

template<>
a_boolean has_ifc_constraint(const an_ifc_decl_parameter &universal);

template<>
an_ifc_expr_index get_ifc_constraint(const an_ifc_decl_parameter &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_decl_parameter &universal);

template<>
an_ifc_expr_index get_ifc_initializer(const an_ifc_decl_parameter &universal);

template<>
a_boolean has_ifc_level(const an_ifc_decl_parameter &universal);

template<>
an_ifc_parameter_level get_ifc_level(const an_ifc_decl_parameter &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_parameter &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_parameter &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_parameter &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_decl_parameter &universal);

template<>
a_boolean has_ifc_pack(const an_ifc_decl_parameter &universal);

template<>
an_ifc_bool get_ifc_pack(const an_ifc_decl_parameter &universal);

template<>
a_boolean has_ifc_position(const an_ifc_decl_parameter &universal);

template<>
an_ifc_parameter_position get_ifc_position(
                                       const an_ifc_decl_parameter &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_parameter &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                       const an_ifc_decl_parameter &universal);

template<>
a_boolean has_ifc_sort(const an_ifc_decl_parameter &universal);

template<>
an_ifc_parameter_sort get_ifc_sort(const an_ifc_decl_parameter &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_parameter &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_parameter &universal);

template<>
a_boolean validate(const an_ifc_decl_parameter   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_parameter>();

/*
Functions for interacting with IFC DeclPartialSpecialization nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_partial_specialization &universal);

template<>
an_ifc_access_sort get_ifc_access(
                          const an_ifc_decl_partial_specialization &universal);

template<>
a_boolean has_ifc_chart(const an_ifc_decl_partial_specialization &universal);

template<>
an_ifc_chart_index get_ifc_chart(
                          const an_ifc_decl_partial_specialization &universal);

template<>
a_boolean has_ifc_entity(const an_ifc_decl_partial_specialization &universal);

template<>
an_ifc_parameterized_entity get_ifc_entity(
                          const an_ifc_decl_partial_specialization &universal);

template<>
a_boolean has_ifc_form(const an_ifc_decl_partial_specialization &universal);

template<>
an_ifc_form_spec_index get_ifc_form(
                          const an_ifc_decl_partial_specialization &universal);

template<>
a_boolean has_ifc_home_scope(
                          const an_ifc_decl_partial_specialization &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(
                          const an_ifc_decl_partial_specialization &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_partial_specialization &universal);

template<>
an_ifc_source_location get_ifc_locus(
                          const an_ifc_decl_partial_specialization &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_partial_specialization &universal);

template<>
an_ifc_name_index get_ifc_name(
                          const an_ifc_decl_partial_specialization &universal);

template<>
a_boolean has_ifc_properties(
                          const an_ifc_decl_partial_specialization &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                          const an_ifc_decl_partial_specialization &universal);

template<>
a_boolean has_ifc_specifiers(
                          const an_ifc_decl_partial_specialization &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                          const an_ifc_decl_partial_specialization &universal);

template<>
a_boolean validate(const an_ifc_decl_partial_specialization &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_decl_partial_specialization>();

/*
Functions for interacting with IFC DeclProperty nodes.
*/

template<>
a_boolean has_ifc_getter(const an_ifc_decl_property &universal);

template<>
an_ifc_text_offset get_ifc_getter(const an_ifc_decl_property &universal);

template<>
a_boolean has_ifc_member(const an_ifc_decl_property &universal);

template<>
an_ifc_decl_index get_ifc_member(const an_ifc_decl_property &universal);

template<>
a_boolean has_ifc_setter(const an_ifc_decl_property &universal);

template<>
an_ifc_text_offset get_ifc_setter(const an_ifc_decl_property &universal);

template<>
a_boolean validate(const an_ifc_decl_property    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_property>();

/*
Functions for interacting with IFC DeclReference nodes.
*/

template<>
a_boolean has_ifc_index(const an_ifc_decl_reference &universal);

template<>
an_ifc_decl_index get_ifc_index(const an_ifc_decl_reference &universal);

template<>
a_boolean has_ifc_local_index(const an_ifc_decl_reference &universal);

template<>
an_ifc_decl_foreign_index get_ifc_local_index(
                                       const an_ifc_decl_reference &universal);

template<>
a_boolean has_ifc_unit(const an_ifc_decl_reference &universal);

template<>
an_ifc_module_reference get_ifc_unit(const an_ifc_decl_reference &universal);

template<>
a_boolean validate(const an_ifc_decl_reference   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_reference>();

/*
Functions for interacting with IFC DeclScope nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_scope &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_scope &universal);

template<>
a_boolean has_ifc_alignment(const an_ifc_decl_scope &universal);

template<>
an_ifc_expr_index get_ifc_alignment(const an_ifc_decl_scope &universal);

template<>
a_boolean has_ifc_base(const an_ifc_decl_scope &universal);

template<>
an_ifc_type_index get_ifc_base(const an_ifc_decl_scope &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_scope &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_scope &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_decl_scope &universal);

template<>
an_ifc_scope_index get_ifc_initializer(const an_ifc_decl_scope &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_scope &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_scope &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_scope &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_decl_scope &universal);

template<>
a_boolean has_ifc_pack_size(const an_ifc_decl_scope &universal);

template<>
an_ifc_pack_size get_ifc_pack_size(const an_ifc_decl_scope &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_scope &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                           const an_ifc_decl_scope &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_scope &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                           const an_ifc_decl_scope &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_decl_scope &universal);

template<>
an_ifc_scope_traits_bitfield get_ifc_traits(
                                           const an_ifc_decl_scope &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_scope &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_scope &universal);

template<>
a_boolean validate(const an_ifc_decl_scope       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_scope>();

/*
Functions for interacting with IFC DeclSpecialization nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_decl_specialization &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_decl_specialization &universal);

template<>
a_boolean has_ifc_form(const an_ifc_decl_specialization &universal);

template<>
an_ifc_form_spec_index get_ifc_form(
                                  const an_ifc_decl_specialization &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_specialization &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(
                                  const an_ifc_decl_specialization &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_specialization &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                  const an_ifc_decl_specialization &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_specialization &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_decl_specialization &universal);

template<>
a_boolean has_ifc_sort(const an_ifc_decl_specialization &universal);

template<>
an_ifc_specialization_sort get_ifc_sort(
                                  const an_ifc_decl_specialization &universal);

template<>
a_boolean validate(const an_ifc_decl_specialization &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_specialization>();

/*
Functions for interacting with IFC DeclTemplate nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_template &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_template &universal);

template<>
a_boolean has_ifc_chart(const an_ifc_decl_template &universal);

template<>
an_ifc_chart_index get_ifc_chart(const an_ifc_decl_template &universal);

template<>
a_boolean has_ifc_entity(const an_ifc_decl_template &universal);

template<>
an_ifc_parameterized_entity get_ifc_entity(
                                        const an_ifc_decl_template &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_template &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_template &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_template &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_template &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_template &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_decl_template &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_template &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                        const an_ifc_decl_template &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_template &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                        const an_ifc_decl_template &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_template &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_template &universal);

template<>
a_boolean validate(const an_ifc_decl_template    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_template>();

/*
Functions for interacting with IFC DeclTemploid nodes.
*/

template<>
a_boolean has_ifc_chart(const an_ifc_decl_temploid &universal);

template<>
an_ifc_chart_index get_ifc_chart(const an_ifc_decl_temploid &universal);

template<>
a_boolean has_ifc_entity(const an_ifc_decl_temploid &universal);

template<>
an_ifc_parameterized_entity get_ifc_entity(
                                        const an_ifc_decl_temploid &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_temploid &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                        const an_ifc_decl_temploid &universal);

template<>
a_boolean validate(const an_ifc_decl_temploid    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_temploid>();

/*
Functions for interacting with IFC DeclTuple nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_decl_tuple &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(const an_ifc_decl_tuple &universal);

template<>
a_boolean has_ifc_start(const an_ifc_decl_tuple &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_decl_tuple &universal);

template<>
a_boolean validate(const an_ifc_decl_tuple       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_tuple>();

/*
Functions for interacting with IFC DeclUsingDeclaration nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_using_declaration &universal);

template<>
an_ifc_access_sort get_ifc_access(
                               const an_ifc_decl_using_declaration &universal);

template<>
a_boolean has_ifc_hidden(const an_ifc_decl_using_declaration &universal);

template<>
an_ifc_bool get_ifc_hidden(const an_ifc_decl_using_declaration &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_using_declaration &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(
                               const an_ifc_decl_using_declaration &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_using_declaration &universal);

template<>
an_ifc_source_location get_ifc_locus(
                               const an_ifc_decl_using_declaration &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_using_declaration &universal);

template<>
an_ifc_text_offset get_ifc_name(
                               const an_ifc_decl_using_declaration &universal);

template<>
a_boolean has_ifc_name2(const an_ifc_decl_using_declaration &universal);

template<>
an_ifc_text_offset get_ifc_name2(
                               const an_ifc_decl_using_declaration &universal);

template<>
a_boolean has_ifc_parent(const an_ifc_decl_using_declaration &universal);

template<>
an_ifc_expr_index get_ifc_parent(
                               const an_ifc_decl_using_declaration &universal);

template<>
a_boolean has_ifc_resolution(const an_ifc_decl_using_declaration &universal);

template<>
an_ifc_decl_index get_ifc_resolution(
                               const an_ifc_decl_using_declaration &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_using_declaration &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                               const an_ifc_decl_using_declaration &universal);

template<>
a_boolean validate(const an_ifc_decl_using_declaration &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_using_declaration>();

/*
Functions for interacting with IFC DeclVariable nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_decl_variable &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_decl_variable &universal);

template<>
a_boolean has_ifc_alignment(const an_ifc_decl_variable &universal);

template<>
an_ifc_expr_index get_ifc_alignment(const an_ifc_decl_variable &universal);

template<>
a_boolean has_ifc_home_scope(const an_ifc_decl_variable &universal);

template<>
an_ifc_decl_index get_ifc_home_scope(const an_ifc_decl_variable &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_decl_variable &universal);

template<>
an_ifc_expr_index get_ifc_initializer(const an_ifc_decl_variable &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_decl_variable &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_decl_variable &universal);

template<>
a_boolean has_ifc_name(const an_ifc_decl_variable &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_decl_variable &universal);

template<>
a_boolean has_ifc_properties(const an_ifc_decl_variable &universal);

template<>
an_ifc_reachable_properties_bitfield get_ifc_properties(
                                        const an_ifc_decl_variable &universal);

template<>
a_boolean has_ifc_specifiers(const an_ifc_decl_variable &universal);

template<>
an_ifc_basic_specifiers_bitfield get_ifc_specifiers(
                                        const an_ifc_decl_variable &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_decl_variable &universal);

template<>
an_ifc_object_traits_bitfield get_ifc_traits(
                                        const an_ifc_decl_variable &universal);

template<>
a_boolean has_ifc_type(const an_ifc_decl_variable &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_decl_variable &universal);

template<>
a_boolean validate(const an_ifc_decl_variable    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_decl_variable>();

/*
Functions for interacting with IFC ExprAlignof nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_alignof &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_alignof &universal);

template<>
a_boolean has_ifc_operand(const an_ifc_expr_alignof &universal);

template<>
an_ifc_syntax_index get_ifc_operand(const an_ifc_expr_alignof &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_alignof &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_alignof &universal);

template<>
a_boolean validate(const an_ifc_expr_alignof     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_alignof>();

/*
Functions for interacting with IFC ExprArrayValue nodes.
*/

template<>
a_boolean has_ifc_element_type(const an_ifc_expr_array_value &universal);

template<>
an_ifc_type_index get_ifc_element_type(
                                     const an_ifc_expr_array_value &universal);

template<>
a_boolean has_ifc_elements(const an_ifc_expr_array_value &universal);

template<>
an_ifc_expr_index get_ifc_elements(const an_ifc_expr_array_value &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_array_value &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_array_value &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_array_value &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_array_value &universal);

template<>
a_boolean validate(const an_ifc_expr_array_value &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_array_value>();

/*
Functions for interacting with IFC ExprAssignInitializer nodes.
*/

template<>
a_boolean has_ifc_equal(const an_ifc_expr_assign_initializer &universal);

template<>
an_ifc_source_location get_ifc_equal(
                              const an_ifc_expr_assign_initializer &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_expr_assign_initializer &universal);

template<>
an_ifc_expr_index get_ifc_initializer(
                              const an_ifc_expr_assign_initializer &universal);

template<>
a_boolean validate(const an_ifc_expr_assign_initializer &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_assign_initializer>();

/*
Functions for interacting with IFC ExprBinaryFold nodes.
*/

template<>
a_boolean has_ifc_associativity(const an_ifc_expr_binary_fold &universal);

template<>
an_ifc_associativity get_ifc_associativity(
                                     const an_ifc_expr_binary_fold &universal);

template<>
a_boolean has_ifc_left(const an_ifc_expr_binary_fold &universal);

template<>
an_ifc_expr_index get_ifc_left(const an_ifc_expr_binary_fold &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_binary_fold &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_binary_fold &universal);

template<>
a_boolean has_ifc_operation(const an_ifc_expr_binary_fold &universal);

template<>
an_ifc_dyadic_operator_sort get_ifc_operation(
                                     const an_ifc_expr_binary_fold &universal);

template<>
a_boolean has_ifc_right(const an_ifc_expr_binary_fold &universal);

template<>
an_ifc_expr_index get_ifc_right(const an_ifc_expr_binary_fold &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_binary_fold &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_binary_fold &universal);

template<>
a_boolean validate(const an_ifc_expr_binary_fold &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_binary_fold>();

/*
Functions for interacting with IFC ExprCall nodes.
*/

template<>
a_boolean has_ifc_arguments(const an_ifc_expr_call &universal);

template<>
an_ifc_expr_index get_ifc_arguments(const an_ifc_expr_call &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_call &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_call &universal);

template<>
a_boolean has_ifc_operation(const an_ifc_expr_call &universal);

template<>
an_ifc_expr_index get_ifc_operation(const an_ifc_expr_call &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_call &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_call &universal);

template<>
a_boolean validate(const an_ifc_expr_call        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_call>();

/*
Functions for interacting with IFC ExprCast nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_cast &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_cast &universal);

template<>
a_boolean has_ifc_op(const an_ifc_expr_cast &universal);

template<>
an_ifc_dyadic_operator_sort get_ifc_op(const an_ifc_expr_cast &universal);

template<>
a_boolean has_ifc_source(const an_ifc_expr_cast &universal);

template<>
an_ifc_expr_index get_ifc_source(const an_ifc_expr_cast &universal);

template<>
a_boolean has_ifc_target(const an_ifc_expr_cast &universal);

template<>
an_ifc_type_index get_ifc_target(const an_ifc_expr_cast &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_cast &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_cast &universal);

template<>
a_boolean validate(const an_ifc_expr_cast        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_cast>();

/*
Functions for interacting with IFC ExprCompoundString nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_compound_string &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                 const an_ifc_expr_compound_string &universal);

template<>
a_boolean has_ifc_prefix(const an_ifc_expr_compound_string &universal);

template<>
an_ifc_text_offset get_ifc_prefix(
                                 const an_ifc_expr_compound_string &universal);

template<>
a_boolean has_ifc_string(const an_ifc_expr_compound_string &universal);

template<>
an_ifc_expr_index get_ifc_string(const an_ifc_expr_compound_string &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_compound_string &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_compound_string &universal);

template<>
a_boolean validate(const an_ifc_expr_compound_string &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_compound_string>();

/*
Functions for interacting with IFC ExprCondition nodes.
*/

template<>
a_boolean has_ifc_expr(const an_ifc_expr_condition &universal);

template<>
an_ifc_expr_index get_ifc_expr(const an_ifc_expr_condition &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_condition &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_condition &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_condition &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_condition &universal);

template<>
a_boolean validate(const an_ifc_expr_condition   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_condition>();

/*
Functions for interacting with IFC ExprDesignatedInitializer nodes.
*/

template<>
a_boolean has_ifc_initializer(
                          const an_ifc_expr_designated_initializer &universal);

template<>
an_ifc_expr_index get_ifc_initializer(
                          const an_ifc_expr_designated_initializer &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_designated_initializer &universal);

template<>
an_ifc_source_location get_ifc_locus(
                          const an_ifc_expr_designated_initializer &universal);

template<>
a_boolean has_ifc_member(const an_ifc_expr_designated_initializer &universal);

template<>
an_ifc_text_offset get_ifc_member(
                          const an_ifc_expr_designated_initializer &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_designated_initializer &universal);

template<>
an_ifc_type_index get_ifc_type(
                          const an_ifc_expr_designated_initializer &universal);

template<>
a_boolean validate(const an_ifc_expr_designated_initializer &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_designated_initializer>();

/*
Functions for interacting with IFC ExprDestructorCall nodes.
*/

template<>
a_boolean has_ifc_cleanup(const an_ifc_expr_destructor_call &universal);

template<>
an_ifc_destructor_sort get_ifc_cleanup(
                                 const an_ifc_expr_destructor_call &universal);

template<>
a_boolean has_ifc_decltype_specifier(
                                 const an_ifc_expr_destructor_call &universal);

template<>
an_ifc_syntax_index get_ifc_decltype_specifier(
                                 const an_ifc_expr_destructor_call &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_destructor_call &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                 const an_ifc_expr_destructor_call &universal);

template<>
a_boolean has_ifc_name(const an_ifc_expr_destructor_call &universal);

template<>
an_ifc_expr_index get_ifc_name(const an_ifc_expr_destructor_call &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_destructor_call &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_destructor_call &universal);

template<>
a_boolean validate(const an_ifc_expr_destructor_call &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_destructor_call>();

/*
Functions for interacting with IFC ExprDyad nodes.
*/

template<>
a_boolean has_ifc_argument_0(const an_ifc_expr_dyad &universal);

template<>
an_ifc_expr_index get_ifc_argument_0(const an_ifc_expr_dyad &universal);

template<>
a_boolean has_ifc_argument_1(const an_ifc_expr_dyad &universal);

template<>
an_ifc_expr_index get_ifc_argument_1(const an_ifc_expr_dyad &universal);

template<>
a_boolean has_ifc_assoc(const an_ifc_expr_dyad &universal);

template<>
an_ifc_dyadic_operator_sort get_ifc_assoc(const an_ifc_expr_dyad &universal);

template<>
a_boolean has_ifc_impl(const an_ifc_expr_dyad &universal);

template<>
an_ifc_decl_index get_ifc_impl(const an_ifc_expr_dyad &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_dyad &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_dyad &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_dyad &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_dyad &universal);

template<>
a_boolean validate(const an_ifc_expr_dyad        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_dyad>();

/*
Functions for interacting with IFC ExprDynamicDispatch nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_dynamic_dispatch &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                const an_ifc_expr_dynamic_dispatch &universal);

template<>
a_boolean has_ifc_pivot(const an_ifc_expr_dynamic_dispatch &universal);

template<>
an_ifc_expr_index get_ifc_pivot(const an_ifc_expr_dynamic_dispatch &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_dynamic_dispatch &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_dynamic_dispatch &universal);

template<>
a_boolean validate(const an_ifc_expr_dynamic_dispatch &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_dynamic_dispatch>();

/*
Functions for interacting with IFC ExprEmpty nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_empty &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_empty &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_empty &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_empty &universal);

template<>
a_boolean validate(const an_ifc_expr_empty       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_empty>();

/*
Functions for interacting with IFC ExprExpansion nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_expansion &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_expansion &universal);

template<>
a_boolean has_ifc_operand(const an_ifc_expr_expansion &universal);

template<>
an_ifc_expr_index get_ifc_operand(const an_ifc_expr_expansion &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_expansion &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_expansion &universal);

template<>
a_boolean validate(const an_ifc_expr_expansion   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_expansion>();

/*
Functions for interacting with IFC ExprExpressionList nodes.
*/

template<>
a_boolean has_ifc_contents(const an_ifc_expr_expression_list &universal);

template<>
an_ifc_expr_index get_ifc_contents(
                                 const an_ifc_expr_expression_list &universal);

template<>
a_boolean has_ifc_delimiter(const an_ifc_expr_expression_list &universal);

template<>
an_ifc_delimiter_sort get_ifc_delimiter(
                                 const an_ifc_expr_expression_list &universal);

template<>
a_boolean has_ifc_left(const an_ifc_expr_expression_list &universal);

template<>
an_ifc_source_location get_ifc_left(
                                 const an_ifc_expr_expression_list &universal);

template<>
a_boolean has_ifc_right(const an_ifc_expr_expression_list &universal);

template<>
an_ifc_source_location get_ifc_right(
                                 const an_ifc_expr_expression_list &universal);

template<>
a_boolean validate(const an_ifc_expr_expression_list &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_expression_list>();

/*
Functions for interacting with IFC ExprFunctionString nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_function_string &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                 const an_ifc_expr_function_string &universal);

template<>
a_boolean has_ifc_macro(const an_ifc_expr_function_string &universal);

template<>
an_ifc_text_offset get_ifc_macro(const an_ifc_expr_function_string &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_function_string &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_function_string &universal);

template<>
a_boolean validate(const an_ifc_expr_function_string &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_function_string>();

/*
Functions for interacting with IFC ExprHierarchyConversion nodes.
*/

template<>
a_boolean has_ifc_inheritance(
                            const an_ifc_expr_hierarchy_conversion &universal);

template<>
an_ifc_expr_index get_ifc_inheritance(
                            const an_ifc_expr_hierarchy_conversion &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_hierarchy_conversion &universal);

template<>
an_ifc_source_location get_ifc_locus(
                            const an_ifc_expr_hierarchy_conversion &universal);

template<>
a_boolean has_ifc_op(const an_ifc_expr_hierarchy_conversion &universal);

template<>
an_ifc_dyadic_operator_sort get_ifc_op(
                            const an_ifc_expr_hierarchy_conversion &universal);

template<>
a_boolean has_ifc_override(const an_ifc_expr_hierarchy_conversion &universal);

template<>
an_ifc_expr_index get_ifc_override(
                            const an_ifc_expr_hierarchy_conversion &universal);

template<>
a_boolean has_ifc_source(const an_ifc_expr_hierarchy_conversion &universal);

template<>
an_ifc_expr_index get_ifc_source(
                            const an_ifc_expr_hierarchy_conversion &universal);

template<>
a_boolean has_ifc_target(const an_ifc_expr_hierarchy_conversion &universal);

template<>
an_ifc_type_index get_ifc_target(
                            const an_ifc_expr_hierarchy_conversion &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_hierarchy_conversion &universal);

template<>
an_ifc_type_index get_ifc_type(
                            const an_ifc_expr_hierarchy_conversion &universal);

template<>
a_boolean validate(const an_ifc_expr_hierarchy_conversion &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_hierarchy_conversion>();

/*
Functions for interacting with IFC ExprInheritancePath nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_inheritance_path &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                const an_ifc_expr_inheritance_path &universal);

template<>
a_boolean has_ifc_path(const an_ifc_expr_inheritance_path &universal);

template<>
an_ifc_expr_index get_ifc_path(const an_ifc_expr_inheritance_path &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_inheritance_path &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_inheritance_path &universal);

template<>
a_boolean validate(const an_ifc_expr_inheritance_path &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_inheritance_path>();

/*
Functions for interacting with IFC ExprInitializer nodes.
*/

template<>
a_boolean has_ifc_expr(const an_ifc_expr_initializer &universal);

template<>
an_ifc_expr_index get_ifc_expr(const an_ifc_expr_initializer &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_initializer &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_initializer &universal);

template<>
a_boolean has_ifc_sort(const an_ifc_expr_initializer &universal);

template<>
an_ifc_initializer_sort get_ifc_sort(const an_ifc_expr_initializer &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_initializer &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_initializer &universal);

template<>
a_boolean validate(const an_ifc_expr_initializer &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_initializer>();

/*
Functions for interacting with IFC ExprInitializerList nodes.
*/

template<>
a_boolean has_ifc_elements(const an_ifc_expr_initializer_list &universal);

template<>
an_ifc_expr_index get_ifc_elements(
                                const an_ifc_expr_initializer_list &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_initializer_list &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                const an_ifc_expr_initializer_list &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_initializer_list &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_initializer_list &universal);

template<>
a_boolean validate(const an_ifc_expr_initializer_list &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_initializer_list>();

/*
Functions for interacting with IFC ExprLabel nodes.
*/

template<>
a_boolean has_ifc_designator(const an_ifc_expr_label &universal);

template<>
an_ifc_expr_index get_ifc_designator(const an_ifc_expr_label &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_label &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_label &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_label &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_label &universal);

template<>
a_boolean validate(const an_ifc_expr_label       &universal,
                   const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_expr_label &universal, unsigned indent);

extern void db_node(const an_ifc_expr_label &universal);
#endif /* DEBUG */

template<>
an_ifc_expr_label_storage* get<an_ifc_expr_label_storage>(
                                       an_ifc_module             *mod,
                                       an_ifc_expr_label_storage *storage,
                                       a_boolean                 fill_storage);

template<>
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_label>();

/*
Functions for interacting with IFC ExprLambda nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_expr_lambda &universal);

template<>
an_ifc_syntax_index get_ifc_body(const an_ifc_expr_lambda &universal);

template<>
a_boolean has_ifc_constraint(const an_ifc_expr_lambda &universal);

template<>
an_ifc_syntax_index get_ifc_constraint(const an_ifc_expr_lambda &universal);

template<>
a_boolean has_ifc_declarator(const an_ifc_expr_lambda &universal);

template<>
an_ifc_syntax_index get_ifc_declarator(const an_ifc_expr_lambda &universal);

template<>
a_boolean has_ifc_introducer(const an_ifc_expr_lambda &universal);

template<>
an_ifc_syntax_index get_ifc_introducer(const an_ifc_expr_lambda &universal);

template<>
a_boolean has_ifc_template_parameters(const an_ifc_expr_lambda &universal);

template<>
an_ifc_syntax_index get_ifc_template_parameters(
                                          const an_ifc_expr_lambda &universal);

template<>
a_boolean validate(const an_ifc_expr_lambda      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_lambda>();

/*
Functions for interacting with IFC ExprLiteral nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_literal &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_literal &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_literal &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_literal &universal);

template<>
a_boolean has_ifc_value(const an_ifc_expr_literal &universal);

template<>
an_ifc_lit_index get_ifc_value(const an_ifc_expr_literal &universal);

template<>
a_boolean validate(const an_ifc_expr_literal     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_literal>();

/*
Functions for interacting with IFC ExprMemberAccess nodes.
*/

template<>
a_boolean has_ifc_enclosing(const an_ifc_expr_member_access &universal);

template<>
an_ifc_type_index get_ifc_enclosing(
                                   const an_ifc_expr_member_access &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_member_access &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                   const an_ifc_expr_member_access &universal);

template<>
a_boolean has_ifc_name(const an_ifc_expr_member_access &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_expr_member_access &universal);

template<>
a_boolean has_ifc_offset(const an_ifc_expr_member_access &universal);

template<>
an_ifc_expr_index get_ifc_offset(const an_ifc_expr_member_access &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_member_access &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_member_access &universal);

template<>
a_boolean validate(const an_ifc_expr_member_access &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_member_access>();

/*
Functions for interacting with IFC ExprMemberInitializer nodes.
*/

template<>
a_boolean has_ifc_base(const an_ifc_expr_member_initializer &universal);

template<>
an_ifc_type_index get_ifc_base(
                              const an_ifc_expr_member_initializer &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_expr_member_initializer &universal);

template<>
an_ifc_expr_index get_ifc_initializer(
                              const an_ifc_expr_member_initializer &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_member_initializer &universal);

template<>
an_ifc_source_location get_ifc_locus(
                              const an_ifc_expr_member_initializer &universal);

template<>
a_boolean has_ifc_member(const an_ifc_expr_member_initializer &universal);

template<>
an_ifc_decl_index get_ifc_member(
                              const an_ifc_expr_member_initializer &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_member_initializer &universal);

template<>
an_ifc_type_index get_ifc_type(
                              const an_ifc_expr_member_initializer &universal);

template<>
a_boolean validate(const an_ifc_expr_member_initializer &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_member_initializer>();

/*
Functions for interacting with IFC ExprMonad nodes.
*/

template<>
a_boolean has_ifc_argument(const an_ifc_expr_monad &universal);

template<>
an_ifc_expr_index get_ifc_argument(const an_ifc_expr_monad &universal);

template<>
a_boolean has_ifc_assoc(const an_ifc_expr_monad &universal);

template<>
an_ifc_monadic_operator_sort get_ifc_assoc(const an_ifc_expr_monad &universal);

template<>
a_boolean has_ifc_impl(const an_ifc_expr_monad &universal);

template<>
an_ifc_decl_index get_ifc_impl(const an_ifc_expr_monad &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_monad &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_monad &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_monad &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_monad &universal);

template<>
a_boolean validate(const an_ifc_expr_monad       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_monad>();

/*
Functions for interacting with IFC ExprNamedDecl nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_named_decl &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_named_decl &universal);

template<>
a_boolean has_ifc_resolution(const an_ifc_expr_named_decl &universal);

template<>
an_ifc_decl_index get_ifc_resolution(const an_ifc_expr_named_decl &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_named_decl &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_named_decl &universal);

template<>
a_boolean validate(const an_ifc_expr_named_decl  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_named_decl>();

/*
Functions for interacting with IFC ExprNullptr nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_nullptr &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_nullptr &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_nullptr &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_nullptr &universal);

template<>
a_boolean validate(const an_ifc_expr_nullptr     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_nullptr>();

/*
Functions for interacting with IFC ExprPackedTemplateArguments nodes.
*/

template<>
a_boolean has_ifc_arguments(
                       const an_ifc_expr_packed_template_arguments &universal);

template<>
an_ifc_expr_index get_ifc_arguments(
                       const an_ifc_expr_packed_template_arguments &universal);

template<>
a_boolean has_ifc_locus(
                       const an_ifc_expr_packed_template_arguments &universal);

template<>
an_ifc_source_location get_ifc_locus(
                       const an_ifc_expr_packed_template_arguments &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_packed_template_arguments &universal);

template<>
an_ifc_type_index get_ifc_type(
                       const an_ifc_expr_packed_template_arguments &universal);

template<>
a_boolean validate(const an_ifc_expr_packed_template_arguments &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_packed_template_arguments>();

/*
Functions for interacting with IFC ExprPath nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_path &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_path &universal);

template<>
a_boolean has_ifc_member(const an_ifc_expr_path &universal);

template<>
an_ifc_expr_index get_ifc_member(const an_ifc_expr_path &universal);

template<>
a_boolean has_ifc_scope(const an_ifc_expr_path &universal);

template<>
an_ifc_expr_index get_ifc_scope(const an_ifc_expr_path &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_path &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_path &universal);

template<>
a_boolean validate(const an_ifc_expr_path        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_path>();

/*
Functions for interacting with IFC ExprPlaceholder nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_placeholder &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_placeholder &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_placeholder &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_placeholder &universal);

template<>
a_boolean validate(const an_ifc_expr_placeholder &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_placeholder>();

/*
Functions for interacting with IFC ExprPointer nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_pointer &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_pointer &universal);

template<>
a_boolean validate(const an_ifc_expr_pointer     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_pointer>();

/*
Functions for interacting with IFC ExprProductTypeValue nodes.
*/

template<>
a_boolean has_ifc_base_subobjects(
                              const an_ifc_expr_product_type_value &universal);

template<>
an_ifc_expr_index get_ifc_base_subobjects(
                              const an_ifc_expr_product_type_value &universal);

template<>
a_boolean has_ifc_class_decl(const an_ifc_expr_product_type_value &universal);

template<>
an_ifc_type_index get_ifc_class_decl(
                              const an_ifc_expr_product_type_value &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_product_type_value &universal);

template<>
an_ifc_source_location get_ifc_locus(
                              const an_ifc_expr_product_type_value &universal);

template<>
a_boolean has_ifc_members(const an_ifc_expr_product_type_value &universal);

template<>
an_ifc_expr_index get_ifc_members(
                              const an_ifc_expr_product_type_value &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_product_type_value &universal);

template<>
an_ifc_type_index get_ifc_type(
                              const an_ifc_expr_product_type_value &universal);

template<>
a_boolean validate(const an_ifc_expr_product_type_value &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_product_type_value>();

/*
Functions for interacting with IFC ExprPushState nodes.
*/

template<>
a_boolean has_ifc_ctor_call(const an_ifc_expr_push_state &universal);

template<>
an_ifc_expr_index get_ifc_ctor_call(const an_ifc_expr_push_state &universal);

template<>
a_boolean has_ifc_dtor_call(const an_ifc_expr_push_state &universal);

template<>
an_ifc_expr_index get_ifc_dtor_call(const an_ifc_expr_push_state &universal);

template<>
a_boolean has_ifc_flags(const an_ifc_expr_push_state &universal);

template<>
an_ifc_eh_flags get_ifc_flags(const an_ifc_expr_push_state &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_push_state &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_push_state &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_push_state &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_push_state &universal);

template<>
a_boolean validate(const an_ifc_expr_push_state  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_push_state>();

/*
Functions for interacting with IFC ExprQualifiedName nodes.
*/

template<>
a_boolean has_ifc_elements(const an_ifc_expr_qualified_name &universal);

template<>
an_ifc_expr_index get_ifc_elements(
                                  const an_ifc_expr_qualified_name &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_qualified_name &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                  const an_ifc_expr_qualified_name &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_qualified_name &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_qualified_name &universal);

template<>
a_boolean has_ifc_typename_keyword(
                                  const an_ifc_expr_qualified_name &universal);

template<>
an_ifc_source_location get_ifc_typename_keyword(
                                  const an_ifc_expr_qualified_name &universal);

template<>
a_boolean validate(const an_ifc_expr_qualified_name &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_qualified_name>();

/*
Functions for interacting with IFC ExprRead nodes.
*/

template<>
a_boolean has_ifc_address(const an_ifc_expr_read &universal);

template<>
an_ifc_expr_index get_ifc_address(const an_ifc_expr_read &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_read &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_read &universal);

template<>
a_boolean has_ifc_sort(const an_ifc_expr_read &universal);

template<>
an_ifc_read_conversion_sort get_ifc_sort(const an_ifc_expr_read &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_read &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_read &universal);

template<>
a_boolean validate(const an_ifc_expr_read        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_read>();

/*
Functions for interacting with IFC ExprRequires nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_expr_requires &universal);

template<>
an_ifc_syntax_index get_ifc_body(const an_ifc_expr_requires &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_requires &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_requires &universal);

template<>
a_boolean has_ifc_parameters(const an_ifc_expr_requires &universal);

template<>
an_ifc_syntax_index get_ifc_parameters(const an_ifc_expr_requires &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_requires &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_requires &universal);

template<>
a_boolean validate(const an_ifc_expr_requires    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_requires>();

/*
Functions for interacting with IFC ExprSimpleIdentifier nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_simple_identifier &universal);

template<>
an_ifc_source_location get_ifc_locus(
                               const an_ifc_expr_simple_identifier &universal);

template<>
a_boolean has_ifc_name(const an_ifc_expr_simple_identifier &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_expr_simple_identifier &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_simple_identifier &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_simple_identifier &universal);

template<>
a_boolean validate(const an_ifc_expr_simple_identifier &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_simple_identifier>();

/*
Functions for interacting with IFC ExprSizeofType nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_sizeof_type &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_sizeof_type &universal);

template<>
a_boolean has_ifc_operand(const an_ifc_expr_sizeof_type &universal);

template<>
an_ifc_type_index get_ifc_operand(const an_ifc_expr_sizeof_type &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_sizeof_type &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_sizeof_type &universal);

template<>
a_boolean validate(const an_ifc_expr_sizeof_type &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_sizeof_type>();

/*
Functions for interacting with IFC ExprString nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_string &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_string &universal);

template<>
a_boolean has_ifc_string_index(const an_ifc_expr_string &universal);

template<>
an_ifc_string_index get_ifc_string_index(const an_ifc_expr_string &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_string &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_string &universal);

template<>
a_boolean validate(const an_ifc_expr_string      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_string>();

/*
Functions for interacting with IFC ExprStringSequence nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_string_sequence &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                 const an_ifc_expr_string_sequence &universal);

template<>
a_boolean has_ifc_strings(const an_ifc_expr_string_sequence &universal);

template<>
an_ifc_expr_index get_ifc_strings(
                                 const an_ifc_expr_string_sequence &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_string_sequence &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_string_sequence &universal);

template<>
a_boolean validate(const an_ifc_expr_string_sequence &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_string_sequence>();

/*
Functions for interacting with IFC ExprSubobjectValue nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_expr_subobject_value &universal);

template<>
an_ifc_expr_index get_ifc_value(const an_ifc_expr_subobject_value &universal);

template<>
a_boolean validate(const an_ifc_expr_subobject_value &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_subobject_value>();

/*
Functions for interacting with IFC ExprSumTypeValue nodes.
*/

template<>
a_boolean has_ifc_discriminant(const an_ifc_expr_sum_type_value &universal);

template<>
an_ifc_active_member get_ifc_discriminant(
                                  const an_ifc_expr_sum_type_value &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_sum_type_value &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                  const an_ifc_expr_sum_type_value &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_sum_type_value &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_sum_type_value &universal);

template<>
a_boolean has_ifc_value(const an_ifc_expr_sum_type_value &universal);

template<>
an_ifc_expr_index get_ifc_value(const an_ifc_expr_sum_type_value &universal);

template<>
a_boolean has_ifc_variant(const an_ifc_expr_sum_type_value &universal);

template<>
an_ifc_decl_index get_ifc_variant(const an_ifc_expr_sum_type_value &universal);

template<>
a_boolean validate(const an_ifc_expr_sum_type_value &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_sum_type_value>();

/*
Functions for interacting with IFC ExprSyntaxTree nodes.
*/

template<>
a_boolean has_ifc_syntax(const an_ifc_expr_syntax_tree &universal);

template<>
an_ifc_syntax_index get_ifc_syntax(const an_ifc_expr_syntax_tree &universal);

template<>
a_boolean validate(const an_ifc_expr_syntax_tree &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_syntax_tree>();

/*
Functions for interacting with IFC ExprTemplateId nodes.
*/

template<>
a_boolean has_ifc_arguments(const an_ifc_expr_template_id &universal);

template<>
an_ifc_expr_index get_ifc_arguments(const an_ifc_expr_template_id &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_template_id &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_template_id &universal);

template<>
a_boolean has_ifc_primary(const an_ifc_expr_template_id &universal);

template<>
an_ifc_expr_index get_ifc_primary(const an_ifc_expr_template_id &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_template_id &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_template_id &universal);

template<>
a_boolean validate(const an_ifc_expr_template_id &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_template_id>();

/*
Functions for interacting with IFC ExprTemplateReference nodes.
*/

template<>
a_boolean has_ifc_arguments(const an_ifc_expr_template_reference &universal);

template<>
an_ifc_expr_index get_ifc_arguments(
                              const an_ifc_expr_template_reference &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_template_reference &universal);

template<>
an_ifc_source_location get_ifc_locus(
                              const an_ifc_expr_template_reference &universal);

template<>
a_boolean has_ifc_member_locus(
                              const an_ifc_expr_template_reference &universal);

template<>
an_ifc_source_location get_ifc_member_locus(
                              const an_ifc_expr_template_reference &universal);

template<>
a_boolean has_ifc_member_name(const an_ifc_expr_template_reference &universal);

template<>
an_ifc_name_index get_ifc_member_name(
                              const an_ifc_expr_template_reference &universal);

template<>
a_boolean has_ifc_scope(const an_ifc_expr_template_reference &universal);

template<>
an_ifc_type_index get_ifc_scope(
                              const an_ifc_expr_template_reference &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_template_reference &universal);

template<>
an_ifc_type_index get_ifc_type(
                              const an_ifc_expr_template_reference &universal);

template<>
a_boolean validate(const an_ifc_expr_template_reference &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_template_reference>();

/*
Functions for interacting with IFC ExprTemporary nodes.
*/

template<>
a_boolean has_ifc_id(const an_ifc_expr_temporary &universal);

template<>
an_ifc_unique_id get_ifc_id(const an_ifc_expr_temporary &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_temporary &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_temporary &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_temporary &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_temporary &universal);

template<>
a_boolean validate(const an_ifc_expr_temporary   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_temporary>();

/*
Functions for interacting with IFC ExprThis nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_this &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_this &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_this &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_this &universal);

template<>
a_boolean validate(const an_ifc_expr_this        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_this>();

/*
Functions for interacting with IFC ExprTokens nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_tokens &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_tokens &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_tokens &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_tokens &universal);

template<>
a_boolean has_ifc_words(const an_ifc_expr_tokens &universal);

template<>
an_ifc_sentence_index get_ifc_words(const an_ifc_expr_tokens &universal);

template<>
a_boolean validate(const an_ifc_expr_tokens      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_tokens>();

/*
Functions for interacting with IFC ExprTriad nodes.
*/

template<>
a_boolean has_ifc_argument_0(const an_ifc_expr_triad &universal);

template<>
an_ifc_expr_index get_ifc_argument_0(const an_ifc_expr_triad &universal);

template<>
a_boolean has_ifc_argument_1(const an_ifc_expr_triad &universal);

template<>
an_ifc_expr_index get_ifc_argument_1(const an_ifc_expr_triad &universal);

template<>
a_boolean has_ifc_argument_2(const an_ifc_expr_triad &universal);

template<>
an_ifc_expr_index get_ifc_argument_2(const an_ifc_expr_triad &universal);

template<>
a_boolean has_ifc_assoc(const an_ifc_expr_triad &universal);

template<>
an_ifc_triadic_operator_sort get_ifc_assoc(const an_ifc_expr_triad &universal);

template<>
a_boolean has_ifc_impl(const an_ifc_expr_triad &universal);

template<>
an_ifc_decl_index get_ifc_impl(const an_ifc_expr_triad &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_triad &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_triad &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_triad &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_triad &universal);

template<>
a_boolean validate(const an_ifc_expr_triad       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_triad>();

/*
Functions for interacting with IFC ExprTuple nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_expr_tuple &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(const an_ifc_expr_tuple &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_tuple &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_tuple &universal);

template<>
a_boolean has_ifc_start(const an_ifc_expr_tuple &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_expr_tuple &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_tuple &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_tuple &universal);

template<>
a_boolean validate(const an_ifc_expr_tuple       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_tuple>();

/*
Functions for interacting with IFC ExprType nodes.
*/

template<>
a_boolean has_ifc_denotation(const an_ifc_expr_type &universal);

template<>
an_ifc_type_index get_ifc_denotation(const an_ifc_expr_type &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_type &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_type &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_type &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_type &universal);

template<>
a_boolean validate(const an_ifc_expr_type        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_type>();

/*
Functions for interacting with IFC ExprTypeTraitIntrinsic nodes.
*/

template<>
a_boolean has_ifc_arguments(const an_ifc_expr_type_trait_intrinsic &universal);

template<>
an_ifc_type_index get_ifc_arguments(
                            const an_ifc_expr_type_trait_intrinsic &universal);

template<>
a_boolean has_ifc_intrinsic(const an_ifc_expr_type_trait_intrinsic &universal);

template<>
an_ifc_operator_category get_ifc_intrinsic(
                            const an_ifc_expr_type_trait_intrinsic &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_type_trait_intrinsic &universal);

template<>
an_ifc_source_location get_ifc_locus(
                            const an_ifc_expr_type_trait_intrinsic &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_type_trait_intrinsic &universal);

template<>
an_ifc_type_index get_ifc_type(
                            const an_ifc_expr_type_trait_intrinsic &universal);

template<>
a_boolean validate(const an_ifc_expr_type_trait_intrinsic &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_type_trait_intrinsic>();

/*
Functions for interacting with IFC ExprTypeid nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_typeid &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_typeid &universal);

template<>
a_boolean has_ifc_operand(const an_ifc_expr_typeid &universal);

template<>
an_ifc_type_index get_ifc_operand(const an_ifc_expr_typeid &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_typeid &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_typeid &universal);

template<>
a_boolean validate(const an_ifc_expr_typeid      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_typeid>();

/*
Functions for interacting with IFC ExprUnaryFold nodes.
*/

template<>
a_boolean has_ifc_associativity(const an_ifc_expr_unary_fold &universal);

template<>
an_ifc_associativity get_ifc_associativity(
                                      const an_ifc_expr_unary_fold &universal);

template<>
a_boolean has_ifc_expr(const an_ifc_expr_unary_fold &universal);

template<>
an_ifc_expr_index get_ifc_expr(const an_ifc_expr_unary_fold &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_expr_unary_fold &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_expr_unary_fold &universal);

template<>
a_boolean has_ifc_operation(const an_ifc_expr_unary_fold &universal);

template<>
an_ifc_dyadic_operator_sort get_ifc_operation(
                                      const an_ifc_expr_unary_fold &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_unary_fold &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_unary_fold &universal);

template<>
a_boolean validate(const an_ifc_expr_unary_fold  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_unary_fold>();

/*
Functions for interacting with IFC ExprUnqualifiedId nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_unqualified_id &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                  const an_ifc_expr_unqualified_id &universal);

template<>
a_boolean has_ifc_name(const an_ifc_expr_unqualified_id &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_expr_unqualified_id &universal);

template<>
a_boolean has_ifc_resolution(const an_ifc_expr_unqualified_id &universal);

template<>
an_ifc_expr_index get_ifc_resolution(
                                  const an_ifc_expr_unqualified_id &universal);

template<>
a_boolean has_ifc_template_keyword(
                                  const an_ifc_expr_unqualified_id &universal);

template<>
an_ifc_source_location get_ifc_template_keyword(
                                  const an_ifc_expr_unqualified_id &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_unqualified_id &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_unqualified_id &universal);

template<>
a_boolean validate(const an_ifc_expr_unqualified_id &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_unqualified_id>();

/*
Functions for interacting with IFC ExprUnresolvedId nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_expr_unresolved_id &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                   const an_ifc_expr_unresolved_id &universal);

template<>
a_boolean has_ifc_name(const an_ifc_expr_unresolved_id &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_expr_unresolved_id &universal);

template<>
a_boolean has_ifc_type(const an_ifc_expr_unresolved_id &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_expr_unresolved_id &universal);

template<>
a_boolean validate(const an_ifc_expr_unresolved_id &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_expr_unresolved_id>();

/*
Functions for interacting with IFC ExprVirtualFunctionConversion nodes.
*/

template<>
a_boolean has_ifc_function(
                     const an_ifc_expr_virtual_function_conversion &universal);

template<>
an_ifc_decl_index get_ifc_function(
                     const an_ifc_expr_virtual_function_conversion &universal);

template<>
a_boolean has_ifc_locus(
                     const an_ifc_expr_virtual_function_conversion &universal);

template<>
an_ifc_source_location get_ifc_locus(
                     const an_ifc_expr_virtual_function_conversion &universal);

template<>
a_boolean has_ifc_type(
                     const an_ifc_expr_virtual_function_conversion &universal);

template<>
an_ifc_type_index get_ifc_type(
                     const an_ifc_expr_virtual_function_conversion &universal);

template<>
a_boolean validate(const an_ifc_expr_virtual_function_conversion &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_expr_virtual_function_conversion>();

/*
Functions for interacting with IFC FormCatenate nodes.
*/

template<>
a_boolean has_ifc_first(const an_ifc_form_catenate &universal);

template<>
an_ifc_form_index get_ifc_first(const an_ifc_form_catenate &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_form_catenate &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_catenate &universal);

template<>
a_boolean has_ifc_second(const an_ifc_form_catenate &universal);

template<>
an_ifc_form_index get_ifc_second(const an_ifc_form_catenate &universal);

template<>
a_boolean validate(const an_ifc_form_catenate    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_catenate>();

/*
Functions for interacting with IFC FormCharacter nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_character &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_character &universal);

template<>
a_boolean has_ifc_spelling(const an_ifc_form_character &universal);

template<>
an_ifc_text_offset get_ifc_spelling(const an_ifc_form_character &universal);

template<>
a_boolean validate(const an_ifc_form_character   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_character>();

/*
Functions for interacting with IFC FormHeader nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_header &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_header &universal);

template<>
a_boolean has_ifc_spelling(const an_ifc_form_header &universal);

template<>
an_ifc_text_offset get_ifc_spelling(const an_ifc_form_header &universal);

template<>
a_boolean validate(const an_ifc_form_header      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_header>();

/*
Functions for interacting with IFC FormIdentifier nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_identifier &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_identifier &universal);

template<>
a_boolean has_ifc_spelling(const an_ifc_form_identifier &universal);

template<>
an_ifc_text_offset get_ifc_spelling(const an_ifc_form_identifier &universal);

template<>
a_boolean validate(const an_ifc_form_identifier  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_identifier>();

/*
Functions for interacting with IFC FormJunk nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_junk &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_junk &universal);

template<>
a_boolean has_ifc_spelling(const an_ifc_form_junk &universal);

template<>
an_ifc_text_offset get_ifc_spelling(const an_ifc_form_junk &universal);

template<>
a_boolean validate(const an_ifc_form_junk        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_junk>();

/*
Functions for interacting with IFC FormKeyword nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_keyword &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_keyword &universal);

template<>
a_boolean has_ifc_spelling(const an_ifc_form_keyword &universal);

template<>
an_ifc_text_offset get_ifc_spelling(const an_ifc_form_keyword &universal);

template<>
a_boolean validate(const an_ifc_form_keyword     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_keyword>();

/*
Functions for interacting with IFC FormNumber nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_number &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_number &universal);

template<>
a_boolean has_ifc_spelling(const an_ifc_form_number &universal);

template<>
an_ifc_text_offset get_ifc_spelling(const an_ifc_form_number &universal);

template<>
a_boolean validate(const an_ifc_form_number      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_number>();

/*
Functions for interacting with IFC FormOperator nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_operator &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_operator &universal);

template<>
a_boolean has_ifc_op(const an_ifc_form_operator &universal);

template<>
an_ifc_form_operator_sort get_ifc_op(const an_ifc_form_operator &universal);

template<>
a_boolean has_ifc_spelling(const an_ifc_form_operator &universal);

template<>
an_ifc_text_offset get_ifc_spelling(const an_ifc_form_operator &universal);

template<>
a_boolean validate(const an_ifc_form_operator    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_operator>();

/*
Functions for interacting with IFC FormParameter nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_parameter &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_parameter &universal);

template<>
a_boolean has_ifc_spelling(const an_ifc_form_parameter &universal);

template<>
an_ifc_text_offset get_ifc_spelling(const an_ifc_form_parameter &universal);

template<>
a_boolean validate(const an_ifc_form_parameter   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_parameter>();

/*
Functions for interacting with IFC FormParenthesized nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_parenthesized &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                   const an_ifc_form_parenthesized &universal);

template<>
a_boolean has_ifc_operand(const an_ifc_form_parenthesized &universal);

template<>
an_ifc_form_index get_ifc_operand(const an_ifc_form_parenthesized &universal);

template<>
a_boolean validate(const an_ifc_form_parenthesized &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_parenthesized>();

/*
Functions for interacting with IFC FormPragma nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_pragma &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_pragma &universal);

template<>
a_boolean has_ifc_operand(const an_ifc_form_pragma &universal);

template<>
an_ifc_form_index get_ifc_operand(const an_ifc_form_pragma &universal);

template<>
a_boolean validate(const an_ifc_form_pragma      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_pragma>();

/*
Functions for interacting with IFC FormSpec nodes.
*/

template<>
a_boolean has_ifc_arguments(const an_ifc_form_spec &universal);

template<>
an_ifc_expr_index get_ifc_arguments(const an_ifc_form_spec &universal);

template<>
a_boolean has_ifc_primary_template(const an_ifc_form_spec &universal);

template<>
an_ifc_decl_index get_ifc_primary_template(const an_ifc_form_spec &universal);

template<>
a_boolean validate(const an_ifc_form_spec        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_spec>();

/*
Functions for interacting with IFC FormString nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_string &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_string &universal);

template<>
a_boolean has_ifc_spelling(const an_ifc_form_string &universal);

template<>
an_ifc_text_offset get_ifc_spelling(const an_ifc_form_string &universal);

template<>
a_boolean validate(const an_ifc_form_string      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_string>();

/*
Functions for interacting with IFC FormStringize nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_stringize &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_stringize &universal);

template<>
a_boolean has_ifc_operand(const an_ifc_form_stringize &universal);

template<>
an_ifc_form_index get_ifc_operand(const an_ifc_form_stringize &universal);

template<>
a_boolean validate(const an_ifc_form_stringize   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_stringize>();

/*
Functions for interacting with IFC FormTuple nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_form_tuple &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(const an_ifc_form_tuple &universal);

template<>
a_boolean has_ifc_start(const an_ifc_form_tuple &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_form_tuple &universal);

template<>
a_boolean validate(const an_ifc_form_tuple       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_tuple>();

/*
Functions for interacting with IFC FormWhitespace nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_form_whitespace &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_form_whitespace &universal);

template<>
a_boolean validate(const an_ifc_form_whitespace  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_form_whitespace>();

/*
Functions for interacting with IFC HeapAttr nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_heap_attr &universal);

template<>
an_ifc_attr_index get_ifc_value(const an_ifc_heap_attr &universal);

template<>
a_boolean validate(const an_ifc_heap_attr        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_heap_attr>();

/*
Functions for interacting with IFC HeapChart nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_heap_chart &universal);

template<>
an_ifc_chart_index get_ifc_value(const an_ifc_heap_chart &universal);

template<>
a_boolean validate(const an_ifc_heap_chart       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_heap_chart>();

/*
Functions for interacting with IFC HeapDecl nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_heap_decl &universal);

template<>
an_ifc_decl_index get_ifc_value(const an_ifc_heap_decl &universal);

template<>
a_boolean validate(const an_ifc_heap_decl        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_heap_decl>();

/*
Functions for interacting with IFC HeapExpr nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_heap_expr &universal);

template<>
an_ifc_expr_index get_ifc_value(const an_ifc_heap_expr &universal);

template<>
a_boolean validate(const an_ifc_heap_expr        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_heap_expr>();

/*
Functions for interacting with IFC HeapForm nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_heap_form &universal);

template<>
an_ifc_form_index get_ifc_value(const an_ifc_heap_form &universal);

template<>
a_boolean validate(const an_ifc_heap_form        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_heap_form>();

/*
Functions for interacting with IFC HeapPPForm nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_heap_pp_form &universal);

template<>
an_ifc_form_index get_ifc_value(const an_ifc_heap_pp_form &universal);

template<>
a_boolean validate(const an_ifc_heap_pp_form     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_heap_pp_form>();

/*
Functions for interacting with IFC HeapStmt nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_heap_stmt &universal);

template<>
an_ifc_stmt_index get_ifc_value(const an_ifc_heap_stmt &universal);

template<>
a_boolean validate(const an_ifc_heap_stmt        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_heap_stmt>();

/*
Functions for interacting with IFC HeapSyntax nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_heap_syntax &universal);

template<>
an_ifc_syntax_index get_ifc_value(const an_ifc_heap_syntax &universal);

template<>
a_boolean validate(const an_ifc_heap_syntax      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_heap_syntax>();

/*
Functions for interacting with IFC HeapType nodes.
*/

template<>
a_boolean has_ifc_value(const an_ifc_heap_type &universal);

template<>
an_ifc_type_index get_ifc_value(const an_ifc_heap_type &universal);

template<>
a_boolean validate(const an_ifc_heap_type        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_heap_type>();

/*
Functions for interacting with IFC MacroFunctionLike nodes.
*/

template<>
a_boolean has_ifc_arity_variadic(const an_ifc_macro_function_like &universal);

template<>
an_ifc_variadic_arity get_ifc_arity_variadic(
                                  const an_ifc_macro_function_like &universal);

template<>
a_boolean has_ifc_body(const an_ifc_macro_function_like &universal);

template<>
an_ifc_form_index get_ifc_body(const an_ifc_macro_function_like &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_macro_function_like &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                  const an_ifc_macro_function_like &universal);

template<>
a_boolean has_ifc_name(const an_ifc_macro_function_like &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_macro_function_like &universal);

template<>
a_boolean has_ifc_parameters(const an_ifc_macro_function_like &universal);

template<>
an_ifc_form_index get_ifc_parameters(
                                  const an_ifc_macro_function_like &universal);

template<>
a_boolean validate(const an_ifc_macro_function_like &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_macro_function_like>();

/*
Functions for interacting with IFC MacroObjectLike nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_macro_object_like &universal);

template<>
an_ifc_form_index get_ifc_body(const an_ifc_macro_object_like &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_macro_object_like &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                    const an_ifc_macro_object_like &universal);

template<>
a_boolean has_ifc_name(const an_ifc_macro_object_like &universal);

template<>
an_ifc_text_offset get_ifc_name(const an_ifc_macro_object_like &universal);

template<>
a_boolean validate(const an_ifc_macro_object_like &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_macro_object_like>();

/*
Functions for interacting with IFC ModuleExportReference nodes.
*/

template<>
a_boolean has_ifc_reference(const an_ifc_module_export_reference &universal);

template<>
an_ifc_module_reference get_ifc_reference(
                              const an_ifc_module_export_reference &universal);

template<>
a_boolean validate(const an_ifc_module_export_reference &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_module_export_reference>();

/*
Functions for interacting with IFC ModuleImportReference nodes.
*/

template<>
a_boolean has_ifc_reference(const an_ifc_module_import_reference &universal);

template<>
an_ifc_module_reference get_ifc_reference(
                              const an_ifc_module_import_reference &universal);

template<>
a_boolean validate(const an_ifc_module_import_reference &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_module_import_reference>();

/*
Functions for interacting with IFC NameConversion nodes.
*/

template<>
a_boolean has_ifc_encoded(const an_ifc_name_conversion &universal);

template<>
an_ifc_text_offset get_ifc_encoded(const an_ifc_name_conversion &universal);

template<>
a_boolean has_ifc_target(const an_ifc_name_conversion &universal);

template<>
an_ifc_type_index get_ifc_target(const an_ifc_name_conversion &universal);

template<>
a_boolean validate(const an_ifc_name_conversion  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_name_conversion>();

/*
Functions for interacting with IFC NameGuide nodes.
*/

template<>
a_boolean has_ifc_primary_template(const an_ifc_name_guide &universal);

template<>
an_ifc_decl_index get_ifc_primary_template(const an_ifc_name_guide &universal);

template<>
a_boolean validate(const an_ifc_name_guide       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_name_guide>();

/*
Functions for interacting with IFC NameLiteral nodes.
*/

template<>
a_boolean has_ifc_encoded(const an_ifc_name_literal &universal);

template<>
an_ifc_text_offset get_ifc_encoded(const an_ifc_name_literal &universal);

template<>
a_boolean validate(const an_ifc_name_literal     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_name_literal>();

/*
Functions for interacting with IFC NameOperator nodes.
*/

template<>
a_boolean has_ifc_encoded(const an_ifc_name_operator &universal);

template<>
an_ifc_text_offset get_ifc_encoded(const an_ifc_name_operator &universal);

template<>
a_boolean has_ifc_operator(const an_ifc_name_operator &universal);

template<>
an_ifc_operator_category get_ifc_operator(
                                        const an_ifc_name_operator &universal);

template<>
a_boolean validate(const an_ifc_name_operator    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_name_operator>();

/*
Functions for interacting with IFC NameSourceFile nodes.
*/

template<>
a_boolean has_ifc_guard(const an_ifc_name_source_file &universal);

template<>
an_ifc_text_offset get_ifc_guard(const an_ifc_name_source_file &universal);

template<>
a_boolean has_ifc_path(const an_ifc_name_source_file &universal);

template<>
an_ifc_text_offset get_ifc_path(const an_ifc_name_source_file &universal);

template<>
a_boolean validate(const an_ifc_name_source_file &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_name_source_file>();

/*
Functions for interacting with IFC NameSpecialization nodes.
*/

template<>
a_boolean has_ifc_arguments(const an_ifc_name_specialization &universal);

template<>
an_ifc_expr_index get_ifc_arguments(
                                  const an_ifc_name_specialization &universal);

template<>
a_boolean has_ifc_primary(const an_ifc_name_specialization &universal);

template<>
an_ifc_name_index get_ifc_primary(const an_ifc_name_specialization &universal);

template<>
a_boolean validate(const an_ifc_name_specialization &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_name_specialization>();

/*
Functions for interacting with IFC NameTemplate nodes.
*/

template<>
a_boolean has_ifc_name(const an_ifc_name_template &universal);

template<>
an_ifc_name_index get_ifc_name(const an_ifc_name_template &universal);

template<>
a_boolean validate(const an_ifc_name_template    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_name_template>();

/*
Functions for interacting with IFC ScopeDescriptor nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_scope_descriptor &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(
                                     const an_ifc_scope_descriptor &universal);

template<>
a_boolean has_ifc_start(const an_ifc_scope_descriptor &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_scope_descriptor &universal);

template<>
a_boolean validate(const an_ifc_scope_descriptor &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_scope_descriptor>();

/*
Functions for interacting with IFC ScopeMember nodes.
*/

template<>
a_boolean has_ifc_index(const an_ifc_scope_member &universal);

template<>
an_ifc_decl_index get_ifc_index(const an_ifc_scope_member &universal);

template<>
a_boolean validate(const an_ifc_scope_member     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_scope_member>();

/*
Functions for interacting with IFC SourceLine nodes.
*/

template<>
a_boolean has_ifc_file(const an_ifc_source_line &universal);

template<>
an_ifc_name_index get_ifc_file(const an_ifc_source_line &universal);

template<>
a_boolean has_ifc_line(const an_ifc_source_line &universal);

template<>
an_ifc_line_number get_ifc_line(const an_ifc_source_line &universal);

template<>
a_boolean validate(const an_ifc_source_line      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_source_line>();

/*
Functions for interacting with IFC SourceSentence nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_source_sentence &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(
                                      const an_ifc_source_sentence &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_source_sentence &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_source_sentence &universal);

template<>
a_boolean has_ifc_start(const an_ifc_source_sentence &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_source_sentence &universal);

template<>
a_boolean validate(const an_ifc_source_sentence  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_source_sentence>();

/*
Functions for interacting with IFC SourceWord nodes.
*/

template<>
a_boolean has_ifc_category(const an_ifc_source_word &universal);

template<>
an_ifc_word_category get_ifc_category(const an_ifc_source_word &universal);

template<>
a_boolean has_ifc_index(const an_ifc_source_word &universal);

template<>
an_ifc_index get_ifc_index(const an_ifc_source_word &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_source_word &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_source_word &universal);

template<>
a_boolean has_ifc_sort(const an_ifc_source_word &universal);

template<>
an_ifc_word_sort get_ifc_sort(const an_ifc_source_word &universal);

template<>
a_boolean has_ifc_value(const an_ifc_source_word &universal);

template<>
an_ifc_u16 get_ifc_value(const an_ifc_source_word &universal);

template<>
a_boolean validate(const an_ifc_source_word      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_source_word>();

/*
Functions for interacting with IFC StmtBlock nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_stmt_block &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(const an_ifc_stmt_block &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_block &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_block &universal);

template<>
a_boolean has_ifc_start(const an_ifc_stmt_block &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_stmt_block &universal);

template<>
a_boolean validate(const an_ifc_stmt_block       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_block>();

/*
Functions for interacting with IFC StmtBreak nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_break &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_break &universal);

template<>
a_boolean validate(const an_ifc_stmt_break       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_break>();

/*
Functions for interacting with IFC StmtCase nodes.
*/

template<>
a_boolean has_ifc_expr(const an_ifc_stmt_case &universal);

template<>
an_ifc_expr_index get_ifc_expr(const an_ifc_stmt_case &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_case &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_case &universal);

template<>
a_boolean validate(const an_ifc_stmt_case        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_case>();

/*
Functions for interacting with IFC StmtContinue nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_continue &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_continue &universal);

template<>
a_boolean validate(const an_ifc_stmt_continue    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_continue>();

/*
Functions for interacting with IFC StmtDecl nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_stmt_decl &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_stmt_decl &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_decl &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_decl &universal);

template<>
a_boolean validate(const an_ifc_stmt_decl        &universal,
                   const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_decl &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_decl &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_decl_storage* get<an_ifc_stmt_decl_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_stmt_decl_storage *storage,
                                        a_boolean                fill_storage);

template<>
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_decl>();

/*
Functions for interacting with IFC StmtDefault nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_default &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_default &universal);

template<>
a_boolean validate(const an_ifc_stmt_default     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_default>();

/*
Functions for interacting with IFC StmtDoWhile nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_stmt_do_while &universal);

template<>
an_ifc_stmt_index get_ifc_body(const an_ifc_stmt_do_while &universal);

template<>
a_boolean has_ifc_condition(const an_ifc_stmt_do_while &universal);

template<>
an_ifc_stmt_index get_ifc_condition(const an_ifc_stmt_do_while &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_do_while &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_do_while &universal);

template<>
a_boolean validate(const an_ifc_stmt_do_while    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_do_while>();

/*
Functions for interacting with IFC StmtEmpty nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_empty &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_empty &universal);

template<>
a_boolean validate(const an_ifc_stmt_empty       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_empty>();

/*
Functions for interacting with IFC StmtExpansion nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_expansion &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_expansion &universal);

template<>
a_boolean has_ifc_operand(const an_ifc_stmt_expansion &universal);

template<>
an_ifc_stmt_index get_ifc_operand(const an_ifc_stmt_expansion &universal);

template<>
a_boolean validate(const an_ifc_stmt_expansion   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_expansion>();

/*
Functions for interacting with IFC StmtExpression nodes.
*/

template<>
a_boolean has_ifc_expr(const an_ifc_stmt_expression &universal);

template<>
an_ifc_expr_index get_ifc_expr(const an_ifc_stmt_expression &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_expression &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_expression &universal);

template<>
a_boolean validate(const an_ifc_stmt_expression  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_expression>();

/*
Functions for interacting with IFC StmtFor nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_stmt_for &universal);

template<>
an_ifc_stmt_index get_ifc_body(const an_ifc_stmt_for &universal);

template<>
a_boolean has_ifc_condition(const an_ifc_stmt_for &universal);

template<>
an_ifc_stmt_index get_ifc_condition(const an_ifc_stmt_for &universal);

template<>
a_boolean has_ifc_continuation(const an_ifc_stmt_for &universal);

template<>
an_ifc_stmt_index get_ifc_continuation(const an_ifc_stmt_for &universal);

template<>
a_boolean has_ifc_initialization(const an_ifc_stmt_for &universal);

template<>
an_ifc_stmt_index get_ifc_initialization(const an_ifc_stmt_for &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_for &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_for &universal);

template<>
a_boolean validate(const an_ifc_stmt_for         &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_for>();

/*
Functions for interacting with IFC StmtGoto nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_goto &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_goto &universal);

template<>
a_boolean has_ifc_target(const an_ifc_stmt_goto &universal);

template<>
an_ifc_expr_index get_ifc_target(const an_ifc_stmt_goto &universal);

template<>
a_boolean validate(const an_ifc_stmt_goto        &universal,
                   const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_goto &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_goto &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_goto_storage* get<an_ifc_stmt_goto_storage>(
                                        an_ifc_module            *mod,
                                        an_ifc_stmt_goto_storage *storage,
                                        a_boolean                fill_storage);

template<>
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_goto>();

/*
Functions for interacting with IFC StmtHandler nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_stmt_handler &universal);

template<>
an_ifc_stmt_index get_ifc_body(const an_ifc_stmt_handler &universal);

template<>
a_boolean has_ifc_exception(const an_ifc_stmt_handler &universal);

template<>
an_ifc_decl_index get_ifc_exception(const an_ifc_stmt_handler &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_handler &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_handler &universal);

template<>
a_boolean validate(const an_ifc_stmt_handler     &universal,
                   const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_handler &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_handler &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_handler_storage* get<an_ifc_stmt_handler_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_stmt_handler_storage *storage,
                                     a_boolean                   fill_storage);

template<>
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_handler>();

/*
Functions for interacting with IFC StmtIf nodes.
*/

template<>
a_boolean has_ifc_alternative(const an_ifc_stmt_if &universal);

template<>
an_ifc_stmt_index get_ifc_alternative(const an_ifc_stmt_if &universal);

template<>
a_boolean has_ifc_condition(const an_ifc_stmt_if &universal);

template<>
an_ifc_stmt_index get_ifc_condition(const an_ifc_stmt_if &universal);

template<>
a_boolean has_ifc_consequence(const an_ifc_stmt_if &universal);

template<>
an_ifc_stmt_index get_ifc_consequence(const an_ifc_stmt_if &universal);

template<>
a_boolean has_ifc_initialization(const an_ifc_stmt_if &universal);

template<>
an_ifc_stmt_index get_ifc_initialization(const an_ifc_stmt_if &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_if &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_if &universal);

template<>
a_boolean validate(const an_ifc_stmt_if          &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_if>();

/*
Functions for interacting with IFC StmtLabeled nodes.
*/

template<>
a_boolean has_ifc_label(const an_ifc_stmt_labeled &universal);

template<>
an_ifc_expr_index get_ifc_label(const an_ifc_stmt_labeled &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_labeled &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_labeled &universal);

template<>
a_boolean has_ifc_stmt(const an_ifc_stmt_labeled &universal);

template<>
an_ifc_stmt_index get_ifc_stmt(const an_ifc_stmt_labeled &universal);

template<>
a_boolean has_ifc_type(const an_ifc_stmt_labeled &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_stmt_labeled &universal);

template<>
a_boolean validate(const an_ifc_stmt_labeled     &universal,
                   const an_ifc_validation_trace *parent);

#if DEBUG
extern void db_node(const an_ifc_stmt_labeled &universal, unsigned indent);

extern void db_node(const an_ifc_stmt_labeled &universal);
#endif /* DEBUG */

template<>
an_ifc_stmt_labeled_storage* get<an_ifc_stmt_labeled_storage>(
                                     an_ifc_module               *mod,
                                     an_ifc_stmt_labeled_storage *storage,
                                     a_boolean                   fill_storage);

template<>
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_labeled>();

/*
Functions for interacting with IFC StmtReturn nodes.
*/

template<>
a_boolean has_ifc_expr(const an_ifc_stmt_return &universal);

template<>
an_ifc_expr_index get_ifc_expr(const an_ifc_stmt_return &universal);

template<>
a_boolean has_ifc_function_type(const an_ifc_stmt_return &universal);

template<>
an_ifc_type_index get_ifc_function_type(const an_ifc_stmt_return &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_return &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_return &universal);

template<>
a_boolean has_ifc_type(const an_ifc_stmt_return &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_stmt_return &universal);

template<>
a_boolean validate(const an_ifc_stmt_return      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_return>();

/*
Functions for interacting with IFC StmtSwitch nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_stmt_switch &universal);

template<>
an_ifc_stmt_index get_ifc_body(const an_ifc_stmt_switch &universal);

template<>
a_boolean has_ifc_condition(const an_ifc_stmt_switch &universal);

template<>
an_ifc_expr_index get_ifc_condition(const an_ifc_stmt_switch &universal);

template<>
a_boolean has_ifc_initialization(const an_ifc_stmt_switch &universal);

template<>
an_ifc_stmt_index get_ifc_initialization(const an_ifc_stmt_switch &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_switch &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_switch &universal);

template<>
a_boolean validate(const an_ifc_stmt_switch      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_switch>();

/*
Functions for interacting with IFC StmtVariableDecl nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_stmt_variable_decl &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_stmt_variable_decl &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_variable_decl &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                   const an_ifc_stmt_variable_decl &universal);

template<>
a_boolean validate(const an_ifc_stmt_variable_decl &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_variable_decl>();

/*
Functions for interacting with IFC StmtWhile nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_stmt_while &universal);

template<>
an_ifc_stmt_index get_ifc_body(const an_ifc_stmt_while &universal);

template<>
a_boolean has_ifc_condition(const an_ifc_stmt_while &universal);

template<>
an_ifc_stmt_index get_ifc_condition(const an_ifc_stmt_while &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_stmt_while &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_stmt_while &universal);

template<>
a_boolean validate(const an_ifc_stmt_while       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_stmt_while>();

/*
Functions for interacting with IFC SyntaxAccessSpecifier nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_syntax_access_specifier &universal);

template<>
an_ifc_keyword_syntax get_ifc_access(
                              const an_ifc_syntax_access_specifier &universal);

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_access_specifier &universal);

template<>
an_ifc_source_location get_ifc_comma(
                              const an_ifc_syntax_access_specifier &universal);

template<>
a_boolean has_ifc_designator(const an_ifc_syntax_access_specifier &universal);

template<>
an_ifc_expr_index get_ifc_designator(
                              const an_ifc_syntax_access_specifier &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_access_specifier &universal);

template<>
an_ifc_source_location get_ifc_locus(
                              const an_ifc_syntax_access_specifier &universal);

template<>
a_boolean has_ifc_virtual_kw(const an_ifc_syntax_access_specifier &universal);

template<>
an_ifc_source_location get_ifc_virtual_kw(
                              const an_ifc_syntax_access_specifier &universal);

template<>
a_boolean has_ifc_virtual_kw2(const an_ifc_syntax_access_specifier &universal);

template<>
an_ifc_source_location get_ifc_virtual_kw2(
                              const an_ifc_syntax_access_specifier &universal);

template<>
a_boolean validate(const an_ifc_syntax_access_specifier &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_access_specifier>();

/*
Functions for interacting with IFC SyntaxAliasDeclaration nodes.
*/

template<>
a_boolean has_ifc_aliasee(const an_ifc_syntax_alias_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_aliasee(
                             const an_ifc_syntax_alias_declaration &universal);

template<>
a_boolean has_ifc_equal(const an_ifc_syntax_alias_declaration &universal);

template<>
an_ifc_source_location get_ifc_equal(
                             const an_ifc_syntax_alias_declaration &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_alias_declaration &universal);

template<>
an_ifc_source_location get_ifc_locus(
                             const an_ifc_syntax_alias_declaration &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_alias_declaration &universal);

template<>
an_ifc_expr_index get_ifc_name(
                             const an_ifc_syntax_alias_declaration &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_alias_declaration &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                             const an_ifc_syntax_alias_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_alias_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_alias_declaration>();

/*
Functions for interacting with IFC SyntaxAlignas nodes.
*/

template<>
a_boolean has_ifc_left_paren(const an_ifc_syntax_alignas &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                                       const an_ifc_syntax_alignas &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_alignas &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_syntax_alignas &universal);

template<>
a_boolean has_ifc_operand(const an_ifc_syntax_alignas &universal);

template<>
an_ifc_syntax_index get_ifc_operand(const an_ifc_syntax_alignas &universal);

template<>
a_boolean has_ifc_right_paren(const an_ifc_syntax_alignas &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                                       const an_ifc_syntax_alignas &universal);

template<>
a_boolean validate(const an_ifc_syntax_alignas   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_alignas>();

/*
Functions for interacting with IFC SyntaxArrayDeclarator nodes.
*/

template<>
a_boolean has_ifc_bound(const an_ifc_syntax_array_declarator &universal);

template<>
an_ifc_expr_index get_ifc_bound(
                              const an_ifc_syntax_array_declarator &universal);

template<>
a_boolean has_ifc_left_bracket(
                              const an_ifc_syntax_array_declarator &universal);

template<>
an_ifc_source_location get_ifc_left_bracket(
                              const an_ifc_syntax_array_declarator &universal);

template<>
a_boolean has_ifc_right_bracket(
                              const an_ifc_syntax_array_declarator &universal);

template<>
an_ifc_source_location get_ifc_right_bracket(
                              const an_ifc_syntax_array_declarator &universal);

template<>
a_boolean validate(const an_ifc_syntax_array_declarator &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_array_declarator>();

/*
Functions for interacting with IFC SyntaxArrayIndex nodes.
*/

template<>
a_boolean has_ifc_array(const an_ifc_syntax_array_index &universal);

template<>
an_ifc_expr_index get_ifc_array(const an_ifc_syntax_array_index &universal);

template<>
a_boolean has_ifc_index(const an_ifc_syntax_array_index &universal);

template<>
an_ifc_expr_index get_ifc_index(const an_ifc_syntax_array_index &universal);

template<>
a_boolean has_ifc_left_bracket(const an_ifc_syntax_array_index &universal);

template<>
an_ifc_source_location get_ifc_left_bracket(
                                   const an_ifc_syntax_array_index &universal);

template<>
a_boolean has_ifc_right_bracket(const an_ifc_syntax_array_index &universal);

template<>
an_ifc_source_location get_ifc_right_bracket(
                                   const an_ifc_syntax_array_index &universal);

template<>
a_boolean validate(const an_ifc_syntax_array_index &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_array_index>();

/*
Functions for interacting with IFC SyntaxArrayOrFunctionDeclarator nodes.
*/

template<>
a_boolean has_ifc_declarator(
                  const an_ifc_syntax_array_or_function_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_declarator(
                  const an_ifc_syntax_array_or_function_declarator &universal);

template<>
a_boolean has_ifc_next(
                  const an_ifc_syntax_array_or_function_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_next(
                  const an_ifc_syntax_array_or_function_declarator &universal);

template<>
a_boolean validate(
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_array_or_function_declarator>();

/*
Functions for interacting with IFC SyntaxAsmStatement nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_asm_statement &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                 const an_ifc_syntax_asm_statement &universal);

template<>
a_boolean has_ifc_tokens(const an_ifc_syntax_asm_statement &universal);

template<>
an_ifc_sentence_index get_ifc_tokens(
                                 const an_ifc_syntax_asm_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_asm_statement &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_asm_statement>();

/*
Functions for interacting with IFC SyntaxAttribute nodes.
*/

template<>
a_boolean has_ifc_argument_clause(const an_ifc_syntax_attribute &universal);

template<>
an_ifc_syntax_index get_ifc_argument_clause(
                                     const an_ifc_syntax_attribute &universal);

template<>
a_boolean has_ifc_colons(const an_ifc_syntax_attribute &universal);

template<>
an_ifc_source_location get_ifc_colons(
                                     const an_ifc_syntax_attribute &universal);

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_attribute &universal);

template<>
an_ifc_source_location get_ifc_comma(const an_ifc_syntax_attribute &universal);

template<>
a_boolean has_ifc_expander(const an_ifc_syntax_attribute &universal);

template<>
an_ifc_source_location get_ifc_expander(
                                     const an_ifc_syntax_attribute &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_attribute &universal);

template<>
an_ifc_expr_index get_ifc_name(const an_ifc_syntax_attribute &universal);

template<>
a_boolean has_ifc_scope(const an_ifc_syntax_attribute &universal);

template<>
an_ifc_expr_index get_ifc_scope(const an_ifc_syntax_attribute &universal);

template<>
a_boolean validate(const an_ifc_syntax_attribute &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_attribute>();

/*
Functions for interacting with IFC SyntaxAttributeArgumentClause nodes.
*/

template<>
a_boolean has_ifc_left_paren(
                     const an_ifc_syntax_attribute_argument_clause &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                     const an_ifc_syntax_attribute_argument_clause &universal);

template<>
a_boolean has_ifc_right_paren(
                     const an_ifc_syntax_attribute_argument_clause &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                     const an_ifc_syntax_attribute_argument_clause &universal);

template<>
a_boolean has_ifc_tokens(
                     const an_ifc_syntax_attribute_argument_clause &universal);

template<>
an_ifc_sentence_index get_ifc_tokens(
                     const an_ifc_syntax_attribute_argument_clause &universal);

template<>
a_boolean validate(const an_ifc_syntax_attribute_argument_clause &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attribute_argument_clause>();

/*
Functions for interacting with IFC SyntaxAttributeSpecifier nodes.
*/

template<>
a_boolean has_ifc_attributes(
                           const an_ifc_syntax_attribute_specifier &universal);

template<>
an_ifc_syntax_index get_ifc_attributes(
                           const an_ifc_syntax_attribute_specifier &universal);

template<>
a_boolean has_ifc_left_paren_1(
                           const an_ifc_syntax_attribute_specifier &universal);

template<>
an_ifc_source_location get_ifc_left_paren_1(
                           const an_ifc_syntax_attribute_specifier &universal);

template<>
a_boolean has_ifc_left_paren_2(
                           const an_ifc_syntax_attribute_specifier &universal);

template<>
an_ifc_source_location get_ifc_left_paren_2(
                           const an_ifc_syntax_attribute_specifier &universal);

template<>
a_boolean has_ifc_prefix(const an_ifc_syntax_attribute_specifier &universal);

template<>
an_ifc_syntax_index get_ifc_prefix(
                           const an_ifc_syntax_attribute_specifier &universal);

template<>
a_boolean has_ifc_right_paren_1(
                           const an_ifc_syntax_attribute_specifier &universal);

template<>
an_ifc_source_location get_ifc_right_paren_1(
                           const an_ifc_syntax_attribute_specifier &universal);

template<>
a_boolean has_ifc_right_paren_2(
                           const an_ifc_syntax_attribute_specifier &universal);

template<>
an_ifc_source_location get_ifc_right_paren_2(
                           const an_ifc_syntax_attribute_specifier &universal);

template<>
a_boolean validate(const an_ifc_syntax_attribute_specifier &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attribute_specifier>();

/*
Functions for interacting with IFC SyntaxAttributeSpecifierSeq nodes.
*/

template<>
a_boolean has_ifc_attributes(
                       const an_ifc_syntax_attribute_specifier_seq &universal);

template<>
an_ifc_syntax_index get_ifc_attributes(
                       const an_ifc_syntax_attribute_specifier_seq &universal);

template<>
a_boolean validate(const an_ifc_syntax_attribute_specifier_seq &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attribute_specifier_seq>();

/*
Functions for interacting with IFC SyntaxAttributeUsingPrefix nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_attribute_using_prefix &universal);

template<>
an_ifc_source_location get_ifc_locus(
                        const an_ifc_syntax_attribute_using_prefix &universal);

template<>
a_boolean has_ifc_scope(const an_ifc_syntax_attribute_using_prefix &universal);

template<>
an_ifc_source_location get_ifc_scope(
                        const an_ifc_syntax_attribute_using_prefix &universal);

template<>
a_boolean validate(const an_ifc_syntax_attribute_using_prefix &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attribute_using_prefix>();

/*
Functions for interacting with IFC SyntaxAttributedDeclaration nodes.
*/

template<>
a_boolean has_ifc_attributes(
                        const an_ifc_syntax_attributed_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_attributes(
                        const an_ifc_syntax_attributed_declaration &universal);

template<>
a_boolean has_ifc_decl(const an_ifc_syntax_attributed_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_decl(
                        const an_ifc_syntax_attributed_declaration &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_attributed_declaration &universal);

template<>
an_ifc_source_location get_ifc_locus(
                        const an_ifc_syntax_attributed_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_attributed_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attributed_declaration>();

/*
Functions for interacting with IFC SyntaxAttributedStatement nodes.
*/

template<>
a_boolean has_ifc_attributes(
                          const an_ifc_syntax_attributed_statement &universal);

template<>
an_ifc_syntax_index get_ifc_attributes(
                          const an_ifc_syntax_attributed_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_attributed_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                          const an_ifc_syntax_attributed_statement &universal);

template<>
a_boolean has_ifc_stmt(const an_ifc_syntax_attributed_statement &universal);

template<>
an_ifc_syntax_index get_ifc_stmt(
                          const an_ifc_syntax_attributed_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_attributed_statement &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_attributed_statement>();

/*
Functions for interacting with IFC SyntaxBaseSpecifier nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_syntax_base_specifier &universal);

template<>
an_ifc_keyword_syntax get_ifc_access(
                                const an_ifc_syntax_base_specifier &universal);

template<>
a_boolean has_ifc_colon(const an_ifc_syntax_base_specifier &universal);

template<>
an_ifc_source_location get_ifc_colon(
                                const an_ifc_syntax_base_specifier &universal);

template<>
a_boolean validate(const an_ifc_syntax_base_specifier &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_base_specifier>();

/*
Functions for interacting with IFC SyntaxBaseSpecifierList nodes.
*/

template<>
a_boolean has_ifc_base_specifiers(
                           const an_ifc_syntax_base_specifier_list &universal);

template<>
an_ifc_syntax_index get_ifc_base_specifiers(
                           const an_ifc_syntax_base_specifier_list &universal);

template<>
a_boolean has_ifc_colon(const an_ifc_syntax_base_specifier_list &universal);

template<>
an_ifc_source_location get_ifc_colon(
                           const an_ifc_syntax_base_specifier_list &universal);

template<>
a_boolean validate(const an_ifc_syntax_base_specifier_list &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_base_specifier_list>();

/*
Functions for interacting with IFC SyntaxBinaryFoldExpression nodes.
*/

template<>
a_boolean has_ifc_direction(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
an_ifc_fold_direction_sort get_ifc_direction(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
a_boolean has_ifc_dyad(const an_ifc_syntax_binary_fold_expression &universal);

template<>
an_ifc_dyadic_operator_sort get_ifc_dyad(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
a_boolean has_ifc_ellipsis(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
an_ifc_source_location get_ifc_ellipsis(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
a_boolean has_ifc_glyph_loci_1(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
an_ifc_source_location get_ifc_glyph_loci_1(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
a_boolean has_ifc_glyph_loci_2(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
an_ifc_source_location get_ifc_glyph_loci_2(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_binary_fold_expression &universal);

template<>
an_ifc_source_location get_ifc_locus(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
a_boolean has_ifc_operand_1(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
an_ifc_expr_index get_ifc_operand_1(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
a_boolean has_ifc_operand_2(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
an_ifc_expr_index get_ifc_operand_2(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
a_boolean has_ifc_right_paren(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                        const an_ifc_syntax_binary_fold_expression &universal);

template<>
a_boolean validate(const an_ifc_syntax_binary_fold_expression &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_binary_fold_expression>();

/*
Functions for interacting with IFC SyntaxBreakStatement nodes.
*/

template<>
a_boolean has_ifc_break(const an_ifc_syntax_break_statement &universal);

template<>
an_ifc_source_location get_ifc_break(
                               const an_ifc_syntax_break_statement &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_break_statement &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                               const an_ifc_syntax_break_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_break_statement &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_break_statement>();

/*
Functions for interacting with IFC SyntaxCaptureDefault nodes.
*/

template<>
a_boolean has_ifc_by_ref(const an_ifc_syntax_capture_default &universal);

template<>
an_ifc_bool get_ifc_by_ref(const an_ifc_syntax_capture_default &universal);

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_capture_default &universal);

template<>
an_ifc_source_location get_ifc_comma(
                               const an_ifc_syntax_capture_default &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_capture_default &universal);

template<>
an_ifc_source_location get_ifc_locus(
                               const an_ifc_syntax_capture_default &universal);

template<>
a_boolean validate(const an_ifc_syntax_capture_default &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_capture_default>();

/*
Functions for interacting with IFC SyntaxClassSpecifier nodes.
*/

template<>
a_boolean has_ifc_bases(const an_ifc_syntax_class_specifier &universal);

template<>
an_ifc_syntax_index get_ifc_bases(
                               const an_ifc_syntax_class_specifier &universal);

template<>
a_boolean has_ifc_class_key(const an_ifc_syntax_class_specifier &universal);

template<>
an_ifc_keyword_syntax get_ifc_class_key(
                               const an_ifc_syntax_class_specifier &universal);

template<>
a_boolean has_ifc_left_paren(const an_ifc_syntax_class_specifier &universal);

template<>
an_ifc_syntax_index get_ifc_left_paren(
                               const an_ifc_syntax_class_specifier &universal);

template<>
a_boolean has_ifc_members(const an_ifc_syntax_class_specifier &universal);

template<>
an_ifc_syntax_index get_ifc_members(
                               const an_ifc_syntax_class_specifier &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_class_specifier &universal);

template<>
an_ifc_expr_index get_ifc_name(const an_ifc_syntax_class_specifier &universal);

template<>
a_boolean has_ifc_right_paren(const an_ifc_syntax_class_specifier &universal);

template<>
an_ifc_syntax_index get_ifc_right_paren(
                               const an_ifc_syntax_class_specifier &universal);

template<>
a_boolean validate(const an_ifc_syntax_class_specifier &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_class_specifier>();

/*
Functions for interacting with IFC SyntaxCompoundRequirement nodes.
*/

template<>
a_boolean has_ifc_condition(
                          const an_ifc_syntax_compound_requirement &universal);

template<>
an_ifc_expr_index get_ifc_condition(
                          const an_ifc_syntax_compound_requirement &universal);

template<>
a_boolean has_ifc_constraint(
                          const an_ifc_syntax_compound_requirement &universal);

template<>
an_ifc_expr_index get_ifc_constraint(
                          const an_ifc_syntax_compound_requirement &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_compound_requirement &universal);

template<>
an_ifc_source_location get_ifc_locus(
                          const an_ifc_syntax_compound_requirement &universal);

template<>
a_boolean has_ifc_noexcept_loc(
                          const an_ifc_syntax_compound_requirement &universal);

template<>
an_ifc_source_location get_ifc_noexcept_loc(
                          const an_ifc_syntax_compound_requirement &universal);

template<>
a_boolean has_ifc_right_curly(
                          const an_ifc_syntax_compound_requirement &universal);

template<>
an_ifc_source_location get_ifc_right_curly(
                          const an_ifc_syntax_compound_requirement &universal);

template<>
a_boolean validate(const an_ifc_syntax_compound_requirement &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_compound_requirement>();

/*
Functions for interacting with IFC SyntaxCompoundStatement nodes.
*/

template<>
a_boolean has_ifc_left_curly(
                            const an_ifc_syntax_compound_statement &universal);

template<>
an_ifc_source_location get_ifc_left_curly(
                            const an_ifc_syntax_compound_statement &universal);

template<>
a_boolean has_ifc_pragam(const an_ifc_syntax_compound_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragam(
                            const an_ifc_syntax_compound_statement &universal);

template<>
a_boolean has_ifc_right_curly(
                            const an_ifc_syntax_compound_statement &universal);

template<>
an_ifc_source_location get_ifc_right_curly(
                            const an_ifc_syntax_compound_statement &universal);

template<>
a_boolean has_ifc_stmts(const an_ifc_syntax_compound_statement &universal);

template<>
an_ifc_syntax_index get_ifc_stmts(
                            const an_ifc_syntax_compound_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_compound_statement &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_compound_statement>();

/*
Functions for interacting with IFC SyntaxConceptDefinition nodes.
*/

template<>
a_boolean has_ifc_concept_keyword(
                            const an_ifc_syntax_concept_definition &universal);

template<>
an_ifc_source_location get_ifc_concept_keyword(
                            const an_ifc_syntax_concept_definition &universal);

template<>
a_boolean has_ifc_equal(const an_ifc_syntax_concept_definition &universal);

template<>
an_ifc_source_location get_ifc_equal(
                            const an_ifc_syntax_concept_definition &universal);

template<>
a_boolean has_ifc_initializer(
                            const an_ifc_syntax_concept_definition &universal);

template<>
an_ifc_expr_index get_ifc_initializer(
                            const an_ifc_syntax_concept_definition &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_concept_definition &universal);

template<>
an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_concept_definition &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_concept_definition &universal);

template<>
an_ifc_text_offset get_ifc_name(
                            const an_ifc_syntax_concept_definition &universal);

template<>
a_boolean has_ifc_parameters(
                            const an_ifc_syntax_concept_definition &universal);

template<>
an_ifc_syntax_index get_ifc_parameters(
                            const an_ifc_syntax_concept_definition &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_concept_definition &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                            const an_ifc_syntax_concept_definition &universal);

template<>
a_boolean validate(const an_ifc_syntax_concept_definition &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_concept_definition>();

/*
Functions for interacting with IFC SyntaxConditionDeclaration nodes.
*/

template<>
a_boolean has_ifc_decl_specifier(
                         const an_ifc_syntax_condition_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_decl_specifier(
                         const an_ifc_syntax_condition_declaration &universal);

template<>
a_boolean has_ifc_initializaerion(
                         const an_ifc_syntax_condition_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_initializaerion(
                         const an_ifc_syntax_condition_declaration &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_condition_declaration &universal);

template<>
an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_condition_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_condition_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_condition_declaration>();

/*
Functions for interacting with IFC SyntaxContinueStatement nodes.
*/

template<>
a_boolean has_ifc_continue(const an_ifc_syntax_continue_statement &universal);

template<>
an_ifc_source_location get_ifc_continue(
                            const an_ifc_syntax_continue_statement &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_continue_statement &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                            const an_ifc_syntax_continue_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_continue_statement &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_continue_statement>();

/*
Functions for interacting with IFC SyntaxCtorInitializer nodes.
*/

template<>
a_boolean has_ifc_colon(const an_ifc_syntax_ctor_initializer &universal);

template<>
an_ifc_source_location get_ifc_colon(
                              const an_ifc_syntax_ctor_initializer &universal);

template<>
a_boolean has_ifc_initializers(
                              const an_ifc_syntax_ctor_initializer &universal);

template<>
an_ifc_syntax_index get_ifc_initializers(
                              const an_ifc_syntax_ctor_initializer &universal);

template<>
a_boolean validate(const an_ifc_syntax_ctor_initializer &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_ctor_initializer>();

/*
Functions for interacting with IFC SyntaxDeclSpecifierSeq nodes.
*/

template<>
a_boolean has_ifc_declspec(const an_ifc_syntax_decl_specifier_seq &universal);

template<>
an_ifc_sentence_index get_ifc_declspec(
                            const an_ifc_syntax_decl_specifier_seq &universal);

template<>
a_boolean has_ifc_explicit_kw(
                            const an_ifc_syntax_decl_specifier_seq &universal);

template<>
an_ifc_syntax_index get_ifc_explicit_kw(
                            const an_ifc_syntax_decl_specifier_seq &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_decl_specifier_seq &universal);

template<>
an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_decl_specifier_seq &universal);

template<>
a_boolean has_ifc_qualifiers(
                            const an_ifc_syntax_decl_specifier_seq &universal);

template<>
an_ifc_qualifier_bitfield get_ifc_qualifiers(
                            const an_ifc_syntax_decl_specifier_seq &universal);

template<>
a_boolean has_ifc_storage_class(
                            const an_ifc_syntax_decl_specifier_seq &universal);

template<>
an_ifc_storage_class get_ifc_storage_class(
                            const an_ifc_syntax_decl_specifier_seq &universal);

template<>
a_boolean has_ifc_type(const an_ifc_syntax_decl_specifier_seq &universal);

template<>
an_ifc_type_index get_ifc_type(
                            const an_ifc_syntax_decl_specifier_seq &universal);

template<>
a_boolean has_ifc_type_name(const an_ifc_syntax_decl_specifier_seq &universal);

template<>
an_ifc_syntax_index get_ifc_type_name(
                            const an_ifc_syntax_decl_specifier_seq &universal);

template<>
a_boolean validate(const an_ifc_syntax_decl_specifier_seq &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_decl_specifier_seq>();

/*
Functions for interacting with IFC SyntaxDeclarationStatement nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_syntax_declaration_statement &universal);

template<>
an_ifc_syntax_index get_ifc_decl(
                         const an_ifc_syntax_declaration_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_declaration_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                         const an_ifc_syntax_declaration_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_declaration_statement &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_declaration_statement>();

/*
Functions for interacting with IFC SyntaxDeclarator nodes.
*/

template<>
a_boolean has_ifc_array_or_function(const an_ifc_syntax_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_array_or_function(
                                    const an_ifc_syntax_declarator &universal);

template<>
a_boolean has_ifc_callable(const an_ifc_syntax_declarator &universal);

template<>
an_ifc_bool get_ifc_callable(const an_ifc_syntax_declarator &universal);

template<>
a_boolean has_ifc_convention(const an_ifc_syntax_declarator &universal);

template<>
an_ifc_calling_convention_sort get_ifc_convention(
                                    const an_ifc_syntax_declarator &universal);

template<>
a_boolean has_ifc_ellipsis(const an_ifc_syntax_declarator &universal);

template<>
an_ifc_source_location get_ifc_ellipsis(
                                    const an_ifc_syntax_declarator &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_declarator &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                    const an_ifc_syntax_declarator &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_declarator &universal);

template<>
an_ifc_expr_index get_ifc_name(const an_ifc_syntax_declarator &universal);

template<>
a_boolean has_ifc_parenthesized(const an_ifc_syntax_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_parenthesized(
                                    const an_ifc_syntax_declarator &universal);

template<>
a_boolean has_ifc_pointer(const an_ifc_syntax_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_pointer(const an_ifc_syntax_declarator &universal);

template<>
a_boolean has_ifc_qualifiers(const an_ifc_syntax_declarator &universal);

template<>
an_ifc_qualifier_bitfield get_ifc_qualifiers(
                                    const an_ifc_syntax_declarator &universal);

template<>
a_boolean has_ifc_trailing_target(const an_ifc_syntax_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_trailing_target(
                                    const an_ifc_syntax_declarator &universal);

template<>
a_boolean has_ifc_virtual_specifiers(
                                    const an_ifc_syntax_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_virtual_specifiers(
                                    const an_ifc_syntax_declarator &universal);

template<>
a_boolean validate(const an_ifc_syntax_declarator &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_declarator>();

/*
Functions for interacting with IFC SyntaxDecltypeSpecifier nodes.
*/

template<>
a_boolean has_ifc_decltype_keyword(
                            const an_ifc_syntax_decltype_specifier &universal);

template<>
an_ifc_source_location get_ifc_decltype_keyword(
                            const an_ifc_syntax_decltype_specifier &universal);

template<>
a_boolean has_ifc_expr(const an_ifc_syntax_decltype_specifier &universal);

template<>
an_ifc_expr_index get_ifc_expr(
                            const an_ifc_syntax_decltype_specifier &universal);

template<>
a_boolean has_ifc_left_paren(
                            const an_ifc_syntax_decltype_specifier &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                            const an_ifc_syntax_decltype_specifier &universal);

template<>
a_boolean has_ifc_right_paren(
                            const an_ifc_syntax_decltype_specifier &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                            const an_ifc_syntax_decltype_specifier &universal);

template<>
a_boolean validate(const an_ifc_syntax_decltype_specifier &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_decltype_specifier>();

/*
Functions for interacting with IFC SyntaxDoWhileStatement nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_syntax_do_while_statement &universal);

template<>
an_ifc_syntax_index get_ifc_body(
                            const an_ifc_syntax_do_while_statement &universal);

template<>
a_boolean has_ifc_condition(const an_ifc_syntax_do_while_statement &universal);

template<>
an_ifc_expr_index get_ifc_condition(
                            const an_ifc_syntax_do_while_statement &universal);

template<>
a_boolean has_ifc_do(const an_ifc_syntax_do_while_statement &universal);

template<>
an_ifc_source_location get_ifc_do(
                            const an_ifc_syntax_do_while_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_do_while_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                            const an_ifc_syntax_do_while_statement &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_do_while_statement &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                            const an_ifc_syntax_do_while_statement &universal);

template<>
a_boolean has_ifc_while(const an_ifc_syntax_do_while_statement &universal);

template<>
an_ifc_source_location get_ifc_while(
                            const an_ifc_syntax_do_while_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_do_while_statement &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_do_while_statement>();

/*
Functions for interacting with IFC SyntaxDynamicExceptionSpec nodes.
*/

template<>
a_boolean has_ifc_expander(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

template<>
an_ifc_source_location get_ifc_expander(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

template<>
a_boolean has_ifc_left_paren(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

template<>
a_boolean has_ifc_right_paren(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

template<>
a_boolean has_ifc_throw(const an_ifc_syntax_dynamic_exception_spec &universal);

template<>
an_ifc_source_location get_ifc_throw(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

template<>
a_boolean has_ifc_type_list(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

template<>
an_ifc_syntax_index get_ifc_type_list(
                        const an_ifc_syntax_dynamic_exception_spec &universal);

template<>
a_boolean validate(const an_ifc_syntax_dynamic_exception_spec &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_dynamic_exception_spec>();

/*
Functions for interacting with IFC SyntaxEmptyStatement nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_empty_statement &universal);

template<>
an_ifc_source_location get_ifc_locus(
                               const an_ifc_syntax_empty_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_empty_statement &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_empty_statement>();

/*
Functions for interacting with IFC SyntaxEnumSpecifier nodes.
*/

template<>
a_boolean has_ifc_base(const an_ifc_syntax_enum_specifier &universal);

template<>
an_ifc_syntax_index get_ifc_base(
                                const an_ifc_syntax_enum_specifier &universal);

template<>
a_boolean has_ifc_class_key(const an_ifc_syntax_enum_specifier &universal);

template<>
an_ifc_keyword_syntax get_ifc_class_key(
                                const an_ifc_syntax_enum_specifier &universal);

template<>
a_boolean has_ifc_colon(const an_ifc_syntax_enum_specifier &universal);

template<>
an_ifc_source_location get_ifc_colon(
                                const an_ifc_syntax_enum_specifier &universal);

template<>
a_boolean has_ifc_enumerators(const an_ifc_syntax_enum_specifier &universal);

template<>
an_ifc_syntax_index get_ifc_enumerators(
                                const an_ifc_syntax_enum_specifier &universal);

template<>
a_boolean has_ifc_left_brace(const an_ifc_syntax_enum_specifier &universal);

template<>
an_ifc_source_location get_ifc_left_brace(
                                const an_ifc_syntax_enum_specifier &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_enum_specifier &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                const an_ifc_syntax_enum_specifier &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_enum_specifier &universal);

template<>
an_ifc_expr_index get_ifc_name(const an_ifc_syntax_enum_specifier &universal);

template<>
a_boolean has_ifc_right_brace(const an_ifc_syntax_enum_specifier &universal);

template<>
an_ifc_source_location get_ifc_right_brace(
                                const an_ifc_syntax_enum_specifier &universal);

template<>
a_boolean validate(const an_ifc_syntax_enum_specifier &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_enum_specifier>();

/*
Functions for interacting with IFC SyntaxEnumeratorDefinition nodes.
*/

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_enumerator_definition &universal);

template<>
an_ifc_source_location get_ifc_comma(
                         const an_ifc_syntax_enumerator_definition &universal);

template<>
a_boolean has_ifc_equal(const an_ifc_syntax_enumerator_definition &universal);

template<>
an_ifc_source_location get_ifc_equal(
                         const an_ifc_syntax_enumerator_definition &universal);

template<>
a_boolean has_ifc_initializer(
                         const an_ifc_syntax_enumerator_definition &universal);

template<>
an_ifc_expr_index get_ifc_initializer(
                         const an_ifc_syntax_enumerator_definition &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_enumerator_definition &universal);

template<>
an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_enumerator_definition &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_enumerator_definition &universal);

template<>
an_ifc_text_offset get_ifc_name(
                         const an_ifc_syntax_enumerator_definition &universal);

template<>
a_boolean validate(const an_ifc_syntax_enumerator_definition &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_enumerator_definition>();

/*
Functions for interacting with IFC SyntaxExceptionDeclaration nodes.
*/

template<>
a_boolean has_ifc_declarator(
                         const an_ifc_syntax_exception_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_declarator(
                         const an_ifc_syntax_exception_declaration &universal);

template<>
a_boolean has_ifc_ellipsis(
                         const an_ifc_syntax_exception_declaration &universal);

template<>
an_ifc_source_location get_ifc_ellipsis(
                         const an_ifc_syntax_exception_declaration &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_exception_declaration &universal);

template<>
an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_exception_declaration &universal);

template<>
a_boolean has_ifc_type_specifiers(
                         const an_ifc_syntax_exception_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_type_specifiers(
                         const an_ifc_syntax_exception_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_exception_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_exception_declaration>();

/*
Functions for interacting with IFC SyntaxExplicitSpecifier nodes.
*/

template<>
a_boolean has_ifc_condition(const an_ifc_syntax_explicit_specifier &universal);

template<>
an_ifc_expr_index get_ifc_condition(
                            const an_ifc_syntax_explicit_specifier &universal);

template<>
a_boolean has_ifc_left_paren(
                            const an_ifc_syntax_explicit_specifier &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                            const an_ifc_syntax_explicit_specifier &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_explicit_specifier &universal);

template<>
an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_explicit_specifier &universal);

template<>
a_boolean has_ifc_right_paren(
                            const an_ifc_syntax_explicit_specifier &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                            const an_ifc_syntax_explicit_specifier &universal);

template<>
a_boolean validate(const an_ifc_syntax_explicit_specifier &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_explicit_specifier>();

/*
Functions for interacting with IFC SyntaxExpression nodes.
*/

template<>
a_boolean has_ifc_expression(const an_ifc_syntax_expression &universal);

template<>
an_ifc_expr_index get_ifc_expression(
                                    const an_ifc_syntax_expression &universal);

template<>
a_boolean validate(const an_ifc_syntax_expression &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_expression>();

/*
Functions for interacting with IFC SyntaxExpressionStatement nodes.
*/

template<>
a_boolean has_ifc_expr(const an_ifc_syntax_expression_statement &universal);

template<>
an_ifc_expr_index get_ifc_expr(
                          const an_ifc_syntax_expression_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_expression_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                          const an_ifc_syntax_expression_statement &universal);

template<>
a_boolean has_ifc_semicolon(
                          const an_ifc_syntax_expression_statement &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                          const an_ifc_syntax_expression_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_expression_statement &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_expression_statement>();

/*
Functions for interacting with IFC SyntaxForRangeDeclaration nodes.
*/

template<>
a_boolean has_ifc_declarator(
                         const an_ifc_syntax_for_range_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_declarator(
                         const an_ifc_syntax_for_range_declaration &universal);

template<>
a_boolean has_ifc_specifiers(
                         const an_ifc_syntax_for_range_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_specifiers(
                         const an_ifc_syntax_for_range_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_for_range_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_for_range_declaration>();

/*
Functions for interacting with IFC SyntaxForStatement nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_syntax_for_statement &universal);

template<>
an_ifc_syntax_index get_ifc_body(const an_ifc_syntax_for_statement &universal);

template<>
a_boolean has_ifc_condition(const an_ifc_syntax_for_statement &universal);

template<>
an_ifc_expr_index get_ifc_condition(
                                 const an_ifc_syntax_for_statement &universal);

template<>
a_boolean has_ifc_continuation(const an_ifc_syntax_for_statement &universal);

template<>
an_ifc_expr_index get_ifc_continuation(
                                 const an_ifc_syntax_for_statement &universal);

template<>
a_boolean has_ifc_for(const an_ifc_syntax_for_statement &universal);

template<>
an_ifc_source_location get_ifc_for(
                                 const an_ifc_syntax_for_statement &universal);

template<>
a_boolean has_ifc_initialization(const an_ifc_syntax_for_statement &universal);

template<>
an_ifc_syntax_index get_ifc_initialization(
                                 const an_ifc_syntax_for_statement &universal);

template<>
a_boolean has_ifc_left_paren(const an_ifc_syntax_for_statement &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                                 const an_ifc_syntax_for_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_for_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                                 const an_ifc_syntax_for_statement &universal);

template<>
a_boolean has_ifc_right_paren(const an_ifc_syntax_for_statement &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                                 const an_ifc_syntax_for_statement &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_for_statement &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                                 const an_ifc_syntax_for_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_for_statement &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_for_statement>();

/*
Functions for interacting with IFC SyntaxFunctionBody nodes.
*/

template<>
a_boolean has_ifc_assign(const an_ifc_syntax_function_body &universal);

template<>
an_ifc_source_location get_ifc_assign(
                                 const an_ifc_syntax_function_body &universal);

template<>
a_boolean has_ifc_generate(const an_ifc_syntax_function_body &universal);

template<>
an_ifc_keyword_syntax get_ifc_generate(
                                 const an_ifc_syntax_function_body &universal);

template<>
a_boolean has_ifc_initializers(const an_ifc_syntax_function_body &universal);

template<>
an_ifc_syntax_index get_ifc_initializers(
                                 const an_ifc_syntax_function_body &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_function_body &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                                 const an_ifc_syntax_function_body &universal);

template<>
a_boolean has_ifc_stmts(const an_ifc_syntax_function_body &universal);

template<>
an_ifc_syntax_index get_ifc_stmts(
                                 const an_ifc_syntax_function_body &universal);

template<>
a_boolean has_ifc_try_block(const an_ifc_syntax_function_body &universal);

template<>
an_ifc_syntax_index get_ifc_try_block(
                                 const an_ifc_syntax_function_body &universal);

template<>
a_boolean validate(const an_ifc_syntax_function_body &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_function_body>();

/*
Functions for interacting with IFC SyntaxFunctionDeclarator nodes.
*/

template<>
a_boolean has_ifc_eh_spec(const an_ifc_syntax_function_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_eh_spec(
                           const an_ifc_syntax_function_declarator &universal);

template<>
a_boolean has_ifc_ellipsis(const an_ifc_syntax_function_declarator &universal);

template<>
an_ifc_source_location get_ifc_ellipsis(
                           const an_ifc_syntax_function_declarator &universal);

template<>
a_boolean has_ifc_left_paren(
                           const an_ifc_syntax_function_declarator &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                           const an_ifc_syntax_function_declarator &universal);

template<>
a_boolean has_ifc_parameters(
                           const an_ifc_syntax_function_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_parameters(
                           const an_ifc_syntax_function_declarator &universal);

template<>
a_boolean has_ifc_ref(const an_ifc_syntax_function_declarator &universal);

template<>
an_ifc_source_location get_ifc_ref(
                           const an_ifc_syntax_function_declarator &universal);

template<>
a_boolean has_ifc_right_paren(
                           const an_ifc_syntax_function_declarator &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                           const an_ifc_syntax_function_declarator &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_syntax_function_declarator &universal);

template<>
an_ifc_function_type_traits_bitfield get_ifc_traits(
                           const an_ifc_syntax_function_declarator &universal);

template<>
a_boolean validate(const an_ifc_syntax_function_declarator &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_function_declarator>();

/*
Functions for interacting with IFC SyntaxFunctionDefinition nodes.
*/

template<>
a_boolean has_ifc_assign(const an_ifc_syntax_function_definition &universal);

template<>
an_ifc_source_location get_ifc_assign(
                           const an_ifc_syntax_function_definition &universal);

template<>
a_boolean has_ifc_initializers(
                           const an_ifc_syntax_function_definition &universal);

template<>
an_ifc_syntax_index get_ifc_initializers(
                           const an_ifc_syntax_function_definition &universal);

template<>
a_boolean has_ifc_semicolon(
                           const an_ifc_syntax_function_definition &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                           const an_ifc_syntax_function_definition &universal);

template<>
a_boolean has_ifc_stmts(const an_ifc_syntax_function_definition &universal);

template<>
an_ifc_syntax_index get_ifc_stmts(
                           const an_ifc_syntax_function_definition &universal);

template<>
a_boolean has_ifc_synthesis(
                           const an_ifc_syntax_function_definition &universal);

template<>
an_ifc_keyword_syntax get_ifc_synthesis(
                           const an_ifc_syntax_function_definition &universal);

template<>
a_boolean has_ifc_try_block(
                           const an_ifc_syntax_function_definition &universal);

template<>
an_ifc_syntax_index get_ifc_try_block(
                           const an_ifc_syntax_function_definition &universal);

template<>
a_boolean validate(const an_ifc_syntax_function_definition &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_function_definition>();

/*
Functions for interacting with IFC SyntaxFunctionTryBlock nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_syntax_function_try_block &universal);

template<>
an_ifc_syntax_index get_ifc_body(
                            const an_ifc_syntax_function_try_block &universal);

template<>
a_boolean has_ifc_handlers(const an_ifc_syntax_function_try_block &universal);

template<>
an_ifc_syntax_index get_ifc_handlers(
                            const an_ifc_syntax_function_try_block &universal);

template<>
a_boolean has_ifc_initializers(
                            const an_ifc_syntax_function_try_block &universal);

template<>
an_ifc_syntax_index get_ifc_initializers(
                            const an_ifc_syntax_function_try_block &universal);

template<>
a_boolean validate(const an_ifc_syntax_function_try_block &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_function_try_block>();

/*
Functions for interacting with IFC SyntaxGotoStatement nodes.
*/

template<>
a_boolean has_ifc_label(const an_ifc_syntax_goto_statement &universal);

template<>
an_ifc_source_location get_ifc_label(
                                const an_ifc_syntax_goto_statement &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_goto_statement &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                const an_ifc_syntax_goto_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_goto_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                                const an_ifc_syntax_goto_statement &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_goto_statement &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                                const an_ifc_syntax_goto_statement &universal);

template<>
a_boolean has_ifc_target(const an_ifc_syntax_goto_statement &universal);

template<>
an_ifc_text_offset get_ifc_target(
                                const an_ifc_syntax_goto_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_goto_statement &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_goto_statement>();

/*
Functions for interacting with IFC SyntaxHandler nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_syntax_handler &universal);

template<>
an_ifc_syntax_index get_ifc_body(const an_ifc_syntax_handler &universal);

template<>
a_boolean has_ifc_catch(const an_ifc_syntax_handler &universal);

template<>
an_ifc_source_location get_ifc_catch(const an_ifc_syntax_handler &universal);

template<>
a_boolean has_ifc_exception(const an_ifc_syntax_handler &universal);

template<>
an_ifc_syntax_index get_ifc_exception(const an_ifc_syntax_handler &universal);

template<>
a_boolean has_ifc_left_paren(const an_ifc_syntax_handler &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                                       const an_ifc_syntax_handler &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_handler &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(const an_ifc_syntax_handler &universal);

template<>
a_boolean has_ifc_right_paren(const an_ifc_syntax_handler &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                                       const an_ifc_syntax_handler &universal);

template<>
a_boolean validate(const an_ifc_syntax_handler   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_handler>();

/*
Functions for interacting with IFC SyntaxHandlerSeq nodes.
*/

template<>
a_boolean has_ifc_handlers(const an_ifc_syntax_handler_seq &universal);

template<>
an_ifc_syntax_index get_ifc_handlers(
                                   const an_ifc_syntax_handler_seq &universal);

template<>
a_boolean validate(const an_ifc_syntax_handler_seq &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_handler_seq>();

/*
Functions for interacting with IFC SyntaxIfStatement nodes.
*/

template<>
a_boolean has_ifc_alternative(const an_ifc_syntax_if_statement &universal);

template<>
an_ifc_syntax_index get_ifc_alternative(
                                  const an_ifc_syntax_if_statement &universal);

template<>
a_boolean has_ifc_condition(const an_ifc_syntax_if_statement &universal);

template<>
an_ifc_index get_ifc_condition(const an_ifc_syntax_if_statement &universal);

template<>
a_boolean has_ifc_consequence(const an_ifc_syntax_if_statement &universal);

template<>
an_ifc_syntax_index get_ifc_consequence(
                                  const an_ifc_syntax_if_statement &universal);

template<>
a_boolean has_ifc_constexpr(const an_ifc_syntax_if_statement &universal);

template<>
an_ifc_source_location get_ifc_constexpr(
                                  const an_ifc_syntax_if_statement &universal);

template<>
a_boolean has_ifc_else(const an_ifc_syntax_if_statement &universal);

template<>
an_ifc_source_location get_ifc_else(
                                  const an_ifc_syntax_if_statement &universal);

template<>
a_boolean has_ifc_if(const an_ifc_syntax_if_statement &universal);

template<>
an_ifc_source_location get_ifc_if(const an_ifc_syntax_if_statement &universal);

template<>
a_boolean has_ifc_initialization(const an_ifc_syntax_if_statement &universal);

template<>
an_ifc_syntax_index get_ifc_initialization(
                                  const an_ifc_syntax_if_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_if_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                                  const an_ifc_syntax_if_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_if_statement &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_if_statement>();

/*
Functions for interacting with IFC SyntaxInitCapture nodes.
*/

template<>
a_boolean has_ifc_ampersand(const an_ifc_syntax_init_capture &universal);

template<>
an_ifc_source_location get_ifc_ampersand(
                                  const an_ifc_syntax_init_capture &universal);

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_init_capture &universal);

template<>
an_ifc_source_location get_ifc_comma(
                                  const an_ifc_syntax_init_capture &universal);

template<>
a_boolean has_ifc_expander(const an_ifc_syntax_init_capture &universal);

template<>
an_ifc_source_location get_ifc_expander(
                                  const an_ifc_syntax_init_capture &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_syntax_init_capture &universal);

template<>
an_ifc_expr_index get_ifc_initializer(
                                  const an_ifc_syntax_init_capture &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_init_capture &universal);

template<>
an_ifc_expr_index get_ifc_name(const an_ifc_syntax_init_capture &universal);

template<>
a_boolean validate(const an_ifc_syntax_init_capture &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_init_capture>();

/*
Functions for interacting with IFC SyntaxInitDeclarator nodes.
*/

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_init_declarator &universal);

template<>
an_ifc_source_location get_ifc_comma(
                               const an_ifc_syntax_init_declarator &universal);

template<>
a_boolean has_ifc_constraint(const an_ifc_syntax_init_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_constraint(
                               const an_ifc_syntax_init_declarator &universal);

template<>
a_boolean has_ifc_declarator(const an_ifc_syntax_init_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_declarator(
                               const an_ifc_syntax_init_declarator &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_syntax_init_declarator &universal);

template<>
an_ifc_expr_index get_ifc_initializer(
                               const an_ifc_syntax_init_declarator &universal);

template<>
a_boolean validate(const an_ifc_syntax_init_declarator &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_init_declarator>();

/*
Functions for interacting with IFC SyntaxInitStatement nodes.
*/

template<>
a_boolean has_ifc_init(const an_ifc_syntax_init_statement &universal);

template<>
an_ifc_syntax_index get_ifc_init(
                                const an_ifc_syntax_init_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_init_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                                const an_ifc_syntax_init_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_init_statement &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_init_statement>();

/*
Functions for interacting with IFC SyntaxLabeledStatement nodes.
*/

template<>
a_boolean has_ifc_label(const an_ifc_syntax_labeled_statement &universal);

template<>
an_ifc_expr_index get_ifc_label(
                             const an_ifc_syntax_labeled_statement &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_labeled_statement &universal);

template<>
an_ifc_keyword_sort get_ifc_locus(
                             const an_ifc_syntax_labeled_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_labeled_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                             const an_ifc_syntax_labeled_statement &universal);

template<>
a_boolean has_ifc_sort(const an_ifc_syntax_labeled_statement &universal);

template<>
an_ifc_label_sort get_ifc_sort(
                             const an_ifc_syntax_labeled_statement &universal);

template<>
a_boolean has_ifc_stmt(const an_ifc_syntax_labeled_statement &universal);

template<>
an_ifc_syntax_index get_ifc_stmt(
                             const an_ifc_syntax_labeled_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_labeled_statement &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_labeled_statement>();

/*
Functions for interacting with IFC SyntaxLambdaDeclarator nodes.
*/

template<>
a_boolean has_ifc_eh_spec(const an_ifc_syntax_lambda_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_eh_spec(
                             const an_ifc_syntax_lambda_declarator &universal);

template<>
a_boolean has_ifc_expander(const an_ifc_syntax_lambda_declarator &universal);

template<>
an_ifc_source_location get_ifc_expander(
                             const an_ifc_syntax_lambda_declarator &universal);

template<>
a_boolean has_ifc_left_paren(const an_ifc_syntax_lambda_declarator &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                             const an_ifc_syntax_lambda_declarator &universal);

template<>
a_boolean has_ifc_modifier(const an_ifc_syntax_lambda_declarator &universal);

template<>
an_ifc_keyword_sort get_ifc_modifier(
                             const an_ifc_syntax_lambda_declarator &universal);

template<>
a_boolean has_ifc_parameters(const an_ifc_syntax_lambda_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_parameters(
                             const an_ifc_syntax_lambda_declarator &universal);

template<>
a_boolean has_ifc_right_paren(
                             const an_ifc_syntax_lambda_declarator &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                             const an_ifc_syntax_lambda_declarator &universal);

template<>
a_boolean has_ifc_trailing_target(
                             const an_ifc_syntax_lambda_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_trailing_target(
                             const an_ifc_syntax_lambda_declarator &universal);

template<>
a_boolean validate(const an_ifc_syntax_lambda_declarator &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_lambda_declarator>();

/*
Functions for interacting with IFC SyntaxLambdaIntroducer nodes.
*/

template<>
a_boolean has_ifc_captures(const an_ifc_syntax_lambda_introducer &universal);

template<>
an_ifc_syntax_index get_ifc_captures(
                             const an_ifc_syntax_lambda_introducer &universal);

template<>
a_boolean has_ifc_left_bracket(
                             const an_ifc_syntax_lambda_introducer &universal);

template<>
an_ifc_source_location get_ifc_left_bracket(
                             const an_ifc_syntax_lambda_introducer &universal);

template<>
a_boolean has_ifc_right_bracket(
                             const an_ifc_syntax_lambda_introducer &universal);

template<>
an_ifc_source_location get_ifc_right_bracket(
                             const an_ifc_syntax_lambda_introducer &universal);

template<>
a_boolean validate(const an_ifc_syntax_lambda_introducer &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_lambda_introducer>();

/*
Functions for interacting with IFC SyntaxMemInitializer nodes.
*/

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_mem_initializer &universal);

template<>
an_ifc_source_location get_ifc_comma(
                               const an_ifc_syntax_mem_initializer &universal);

template<>
a_boolean has_ifc_expander(const an_ifc_syntax_mem_initializer &universal);

template<>
an_ifc_source_location get_ifc_expander(
                               const an_ifc_syntax_mem_initializer &universal);

template<>
a_boolean has_ifc_initializer(const an_ifc_syntax_mem_initializer &universal);

template<>
an_ifc_expr_index get_ifc_initializer(
                               const an_ifc_syntax_mem_initializer &universal);

template<>
a_boolean has_ifc_member(const an_ifc_syntax_mem_initializer &universal);

template<>
an_ifc_expr_index get_ifc_member(
                               const an_ifc_syntax_mem_initializer &universal);

template<>
a_boolean validate(const an_ifc_syntax_mem_initializer &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_mem_initializer>();

/*
Functions for interacting with IFC SyntaxMemberDeclaration nodes.
*/

template<>
a_boolean has_ifc_decl_specifiers(
                            const an_ifc_syntax_member_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_decl_specifiers(
                            const an_ifc_syntax_member_declaration &universal);

template<>
a_boolean has_ifc_declarations(
                            const an_ifc_syntax_member_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_declarations(
                            const an_ifc_syntax_member_declaration &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_member_declaration &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                            const an_ifc_syntax_member_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_member_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_member_declaration>();

/*
Functions for interacting with IFC SyntaxMemberDeclarator nodes.
*/

template<>
a_boolean has_ifc_bitwidth(const an_ifc_syntax_member_declarator &universal);

template<>
an_ifc_expr_index get_ifc_bitwidth(
                             const an_ifc_syntax_member_declarator &universal);

template<>
a_boolean has_ifc_colon(const an_ifc_syntax_member_declarator &universal);

template<>
an_ifc_source_location get_ifc_colon(
                             const an_ifc_syntax_member_declarator &universal);

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_member_declarator &universal);

template<>
an_ifc_source_location get_ifc_comma(
                             const an_ifc_syntax_member_declarator &universal);

template<>
a_boolean has_ifc_constraint(const an_ifc_syntax_member_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_constraint(
                             const an_ifc_syntax_member_declarator &universal);

template<>
a_boolean has_ifc_declarator(const an_ifc_syntax_member_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_declarator(
                             const an_ifc_syntax_member_declarator &universal);

template<>
a_boolean has_ifc_initializer(
                             const an_ifc_syntax_member_declarator &universal);

template<>
an_ifc_expr_index get_ifc_initializer(
                             const an_ifc_syntax_member_declarator &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_member_declarator &universal);

template<>
an_ifc_source_location get_ifc_locus(
                             const an_ifc_syntax_member_declarator &universal);

template<>
a_boolean validate(const an_ifc_syntax_member_declarator &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_member_declarator>();

/*
Functions for interacting with IFC SyntaxMemberFunctionDeclaration nodes.
*/

template<>
a_boolean has_ifc_definition(
                   const an_ifc_syntax_member_function_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_definition(
                   const an_ifc_syntax_member_function_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_member_function_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_member_function_declaration>();

/*
Functions for interacting with IFC SyntaxMemberSpecification nodes.
*/

template<>
a_boolean has_ifc_member_declarations(
                          const an_ifc_syntax_member_specification &universal);

template<>
an_ifc_syntax_index get_ifc_member_declarations(
                          const an_ifc_syntax_member_specification &universal);

template<>
a_boolean validate(const an_ifc_syntax_member_specification &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_member_specification>();

/*
Functions for interacting with IFC SyntaxNamespaceAliasDefinition nodes.
*/

template<>
a_boolean has_ifc_assign(
                    const an_ifc_syntax_namespace_alias_definition &universal);

template<>
an_ifc_source_location get_ifc_assign(
                    const an_ifc_syntax_namespace_alias_definition &universal);

template<>
a_boolean has_ifc_name(
                    const an_ifc_syntax_namespace_alias_definition &universal);

template<>
an_ifc_expr_index get_ifc_name(
                    const an_ifc_syntax_namespace_alias_definition &universal);

template<>
a_boolean has_ifc_namespace_kw(
                    const an_ifc_syntax_namespace_alias_definition &universal);

template<>
an_ifc_source_location get_ifc_namespace_kw(
                    const an_ifc_syntax_namespace_alias_definition &universal);

template<>
a_boolean has_ifc_semicolon(
                    const an_ifc_syntax_namespace_alias_definition &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                    const an_ifc_syntax_namespace_alias_definition &universal);

template<>
a_boolean has_ifc_target(
                    const an_ifc_syntax_namespace_alias_definition &universal);

template<>
an_ifc_expr_index get_ifc_target(
                    const an_ifc_syntax_namespace_alias_definition &universal);

template<>
a_boolean validate(const an_ifc_syntax_namespace_alias_definition &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_namespace_alias_definition>();

/*
Functions for interacting with IFC SyntaxNestedRequirement nodes.
*/

template<>
a_boolean has_ifc_condition(const an_ifc_syntax_nested_requirement &universal);

template<>
an_ifc_expr_index get_ifc_condition(
                            const an_ifc_syntax_nested_requirement &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_nested_requirement &universal);

template<>
an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_nested_requirement &universal);

template<>
a_boolean validate(const an_ifc_syntax_nested_requirement &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_nested_requirement>();

/*
Functions for interacting with IFC SyntaxNewDeclarator nodes.
*/

template<>
a_boolean has_ifc_declarator(const an_ifc_syntax_new_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_declarator(
                                const an_ifc_syntax_new_declarator &universal);

template<>
a_boolean validate(const an_ifc_syntax_new_declarator &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_new_declarator>();

/*
Functions for interacting with IFC SyntaxNoexceptSpecification nodes.
*/

template<>
a_boolean has_ifc_expr(const an_ifc_syntax_noexcept_specification &universal);

template<>
an_ifc_syntax_index get_ifc_expr(
                        const an_ifc_syntax_noexcept_specification &universal);

template<>
a_boolean has_ifc_left_paren(
                        const an_ifc_syntax_noexcept_specification &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                        const an_ifc_syntax_noexcept_specification &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_noexcept_specification &universal);

template<>
an_ifc_source_location get_ifc_locus(
                        const an_ifc_syntax_noexcept_specification &universal);

template<>
a_boolean has_ifc_right_paren(
                        const an_ifc_syntax_noexcept_specification &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                        const an_ifc_syntax_noexcept_specification &universal);

template<>
a_boolean validate(const an_ifc_syntax_noexcept_specification &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_noexcept_specification>();

/*
Functions for interacting with IFC SyntaxNonTypeTemplateArgument nodes.
*/

template<>
a_boolean has_ifc_argument(
                    const an_ifc_syntax_non_type_template_argument &universal);

template<>
an_ifc_expr_index get_ifc_argument(
                    const an_ifc_syntax_non_type_template_argument &universal);

template<>
a_boolean has_ifc_comma(
                    const an_ifc_syntax_non_type_template_argument &universal);

template<>
an_ifc_source_location get_ifc_comma(
                    const an_ifc_syntax_non_type_template_argument &universal);

template<>
a_boolean has_ifc_ellipsis(
                    const an_ifc_syntax_non_type_template_argument &universal);

template<>
an_ifc_source_location get_ifc_ellipsis(
                    const an_ifc_syntax_non_type_template_argument &universal);

template<>
a_boolean validate(const an_ifc_syntax_non_type_template_argument &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_non_type_template_argument>();

/*
Functions for interacting with IFC SyntaxParameterDeclarator nodes.
*/

template<>
a_boolean has_ifc_decl_specifiers(
                          const an_ifc_syntax_parameter_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_decl_specifiers(
                          const an_ifc_syntax_parameter_declarator &universal);

template<>
a_boolean has_ifc_declarator(
                          const an_ifc_syntax_parameter_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_declarator(
                          const an_ifc_syntax_parameter_declarator &universal);

template<>
a_boolean has_ifc_default_expr(
                          const an_ifc_syntax_parameter_declarator &universal);

template<>
an_ifc_expr_index get_ifc_default_expr(
                          const an_ifc_syntax_parameter_declarator &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_parameter_declarator &universal);

template<>
an_ifc_source_location get_ifc_locus(
                          const an_ifc_syntax_parameter_declarator &universal);

template<>
a_boolean has_ifc_sort(const an_ifc_syntax_parameter_declarator &universal);

template<>
an_ifc_parameter_sort get_ifc_sort(
                          const an_ifc_syntax_parameter_declarator &universal);

template<>
a_boolean validate(const an_ifc_syntax_parameter_declarator &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_parameter_declarator>();

/*
Functions for interacting with IFC SyntaxPlaceholderTypeSpecifier nodes.
*/

template<>
a_boolean has_ifc_basis(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

template<>
an_ifc_type_basis_sort get_ifc_basis(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

template<>
a_boolean has_ifc_constraint(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

template<>
an_ifc_expr_index get_ifc_constraint(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

template<>
a_boolean has_ifc_keyword(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

template<>
an_ifc_source_location get_ifc_keyword(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

template<>
a_boolean has_ifc_locus(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

template<>
an_ifc_source_location get_ifc_locus(
                    const an_ifc_syntax_placeholder_type_specifier &universal);

template<>
a_boolean validate(const an_ifc_syntax_placeholder_type_specifier &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_placeholder_type_specifier>();

/*
Functions for interacting with IFC SyntaxPointerDeclarator nodes.
*/

template<>
a_boolean has_ifc_callable(const an_ifc_syntax_pointer_declarator &universal);

template<>
an_ifc_bool get_ifc_callable(
                            const an_ifc_syntax_pointer_declarator &universal);

template<>
a_boolean has_ifc_convention(
                            const an_ifc_syntax_pointer_declarator &universal);

template<>
an_ifc_calling_convention_sort get_ifc_convention(
                            const an_ifc_syntax_pointer_declarator &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_pointer_declarator &universal);

template<>
an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_pointer_declarator &universal);

template<>
a_boolean has_ifc_next(const an_ifc_syntax_pointer_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_next(
                            const an_ifc_syntax_pointer_declarator &universal);

template<>
a_boolean has_ifc_qualifiers(
                            const an_ifc_syntax_pointer_declarator &universal);

template<>
an_ifc_qualifier_bitfield get_ifc_qualifiers(
                            const an_ifc_syntax_pointer_declarator &universal);

template<>
a_boolean has_ifc_sort(const an_ifc_syntax_pointer_declarator &universal);

template<>
an_ifc_pointer_declarator_sort get_ifc_sort(
                            const an_ifc_syntax_pointer_declarator &universal);

template<>
a_boolean has_ifc_whole(const an_ifc_syntax_pointer_declarator &universal);

template<>
an_ifc_syntax_index get_ifc_whole(
                            const an_ifc_syntax_pointer_declarator &universal);

template<>
a_boolean validate(const an_ifc_syntax_pointer_declarator &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_pointer_declarator>();

/*
Functions for interacting with IFC SyntaxRangeBasedForStatement nodes.
*/

template<>
a_boolean has_ifc_body(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
an_ifc_syntax_index get_ifc_body(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
a_boolean has_ifc_colon(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
an_ifc_source_location get_ifc_colon(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
a_boolean has_ifc_decl(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
an_ifc_syntax_index get_ifc_decl(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
a_boolean has_ifc_for(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
an_ifc_source_location get_ifc_for(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
a_boolean has_ifc_init(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
an_ifc_syntax_index get_ifc_init(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
a_boolean has_ifc_initializer(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
an_ifc_syntax_index get_ifc_initializer(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
a_boolean has_ifc_left_paren(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
a_boolean has_ifc_pragma(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
a_boolean has_ifc_right_paren(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                     const an_ifc_syntax_range_based_for_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_range_based_for_statement &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_range_based_for_statement>();

/*
Functions for interacting with IFC SyntaxRequirementBody nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_requirement_body &universal);

template<>
an_ifc_source_location get_ifc_locus(
                              const an_ifc_syntax_requirement_body &universal);

template<>
a_boolean has_ifc_requirements(
                              const an_ifc_syntax_requirement_body &universal);

template<>
an_ifc_syntax_index get_ifc_requirements(
                              const an_ifc_syntax_requirement_body &universal);

template<>
a_boolean has_ifc_right_curly(const an_ifc_syntax_requirement_body &universal);

template<>
an_ifc_source_location get_ifc_right_curly(
                              const an_ifc_syntax_requirement_body &universal);

template<>
a_boolean validate(const an_ifc_syntax_requirement_body &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_requirement_body>();

/*
Functions for interacting with IFC SyntaxRequiresClause nodes.
*/

template<>
a_boolean has_ifc_condition(const an_ifc_syntax_requires_clause &universal);

template<>
an_ifc_expr_index get_ifc_condition(
                               const an_ifc_syntax_requires_clause &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_requires_clause &universal);

template<>
an_ifc_source_location get_ifc_locus(
                               const an_ifc_syntax_requires_clause &universal);

template<>
a_boolean validate(const an_ifc_syntax_requires_clause &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_requires_clause>();

/*
Functions for interacting with IFC SyntaxReturnStatement nodes.
*/

template<>
a_boolean has_ifc_expr(const an_ifc_syntax_return_statement &universal);

template<>
an_ifc_expr_index get_ifc_expr(
                              const an_ifc_syntax_return_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_return_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                              const an_ifc_syntax_return_statement &universal);

template<>
a_boolean has_ifc_return(const an_ifc_syntax_return_statement &universal);

template<>
an_ifc_source_location get_ifc_return(
                              const an_ifc_syntax_return_statement &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_return_statement &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                              const an_ifc_syntax_return_statement &universal);

template<>
a_boolean has_ifc_sort(const an_ifc_syntax_return_statement &universal);

template<>
an_ifc_return_sort get_ifc_sort(
                              const an_ifc_syntax_return_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_return_statement &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_return_statement>();

/*
Functions for interacting with IFC SyntaxSEHExcept nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_syntax_seh_except &universal);

template<>
an_ifc_syntax_index get_ifc_body(const an_ifc_syntax_seh_except &universal);

template<>
a_boolean has_ifc_condition(const an_ifc_syntax_seh_except &universal);

template<>
an_ifc_expr_index get_ifc_condition(const an_ifc_syntax_seh_except &universal);

template<>
a_boolean has_ifc_except_kw(const an_ifc_syntax_seh_except &universal);

template<>
an_ifc_source_location get_ifc_except_kw(
                                    const an_ifc_syntax_seh_except &universal);

template<>
a_boolean has_ifc_left_paren(const an_ifc_syntax_seh_except &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                                    const an_ifc_syntax_seh_except &universal);

template<>
a_boolean has_ifc_right_paren(const an_ifc_syntax_seh_except &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                                    const an_ifc_syntax_seh_except &universal);

template<>
a_boolean validate(const an_ifc_syntax_seh_except &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_seh_except>();

/*
Functions for interacting with IFC SyntaxSEHFinally nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_syntax_seh_finally &universal);

template<>
an_ifc_syntax_index get_ifc_body(const an_ifc_syntax_seh_finally &universal);

template<>
a_boolean has_ifc_finally_kw(const an_ifc_syntax_seh_finally &universal);

template<>
an_ifc_source_location get_ifc_finally_kw(
                                   const an_ifc_syntax_seh_finally &universal);

template<>
a_boolean validate(const an_ifc_syntax_seh_finally &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_seh_finally>();

/*
Functions for interacting with IFC SyntaxSEHLeave nodes.
*/

template<>
a_boolean has_ifc_leave_kw(const an_ifc_syntax_seh_leave &universal);

template<>
an_ifc_source_location get_ifc_leave_kw(
                                     const an_ifc_syntax_seh_leave &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_seh_leave &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                                     const an_ifc_syntax_seh_leave &universal);

template<>
a_boolean validate(const an_ifc_syntax_seh_leave &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_seh_leave>();

/*
Functions for interacting with IFC SyntaxSEHTry nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_syntax_seh_try &universal);

template<>
an_ifc_syntax_index get_ifc_body(const an_ifc_syntax_seh_try &universal);

template<>
a_boolean has_ifc_handler(const an_ifc_syntax_seh_try &universal);

template<>
an_ifc_syntax_index get_ifc_handler(const an_ifc_syntax_seh_try &universal);

template<>
a_boolean has_ifc_try_kw(const an_ifc_syntax_seh_try &universal);

template<>
an_ifc_source_location get_ifc_try_kw(const an_ifc_syntax_seh_try &universal);

template<>
a_boolean validate(const an_ifc_syntax_seh_try   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_seh_try>();

/*
Functions for interacting with IFC SyntaxSimpleCapture nodes.
*/

template<>
a_boolean has_ifc_ampersand(const an_ifc_syntax_simple_capture &universal);

template<>
an_ifc_source_location get_ifc_ampersand(
                                const an_ifc_syntax_simple_capture &universal);

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_simple_capture &universal);

template<>
an_ifc_source_location get_ifc_comma(
                                const an_ifc_syntax_simple_capture &universal);

template<>
a_boolean has_ifc_expander(const an_ifc_syntax_simple_capture &universal);

template<>
an_ifc_source_location get_ifc_expander(
                                const an_ifc_syntax_simple_capture &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_simple_capture &universal);

template<>
an_ifc_expr_index get_ifc_name(const an_ifc_syntax_simple_capture &universal);

template<>
a_boolean validate(const an_ifc_syntax_simple_capture &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_simple_capture>();

/*
Functions for interacting with IFC SyntaxSimpleDeclaration nodes.
*/

template<>
a_boolean has_ifc_decl_specifiers(
                            const an_ifc_syntax_simple_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_decl_specifiers(
                            const an_ifc_syntax_simple_declaration &universal);

template<>
a_boolean has_ifc_declarators(
                            const an_ifc_syntax_simple_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_declarators(
                            const an_ifc_syntax_simple_declaration &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_simple_declaration &universal);

template<>
an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_simple_declaration &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_simple_declaration &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                            const an_ifc_syntax_simple_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_simple_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_simple_declaration>();

/*
Functions for interacting with IFC SyntaxSimpleRequirement nodes.
*/

template<>
a_boolean has_ifc_condition(const an_ifc_syntax_simple_requirement &universal);

template<>
an_ifc_expr_index get_ifc_condition(
                            const an_ifc_syntax_simple_requirement &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_simple_requirement &universal);

template<>
an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_simple_requirement &universal);

template<>
a_boolean validate(const an_ifc_syntax_simple_requirement &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_simple_requirement>();

/*
Functions for interacting with IFC SyntaxSimpleTypeSpecifier nodes.
*/

template<>
a_boolean has_ifc_expr(const an_ifc_syntax_simple_type_specifier &universal);

template<>
an_ifc_expr_index get_ifc_expr(
                         const an_ifc_syntax_simple_type_specifier &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_simple_type_specifier &universal);

template<>
an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_simple_type_specifier &universal);

template<>
a_boolean has_ifc_type(const an_ifc_syntax_simple_type_specifier &universal);

template<>
an_ifc_type_index get_ifc_type(
                         const an_ifc_syntax_simple_type_specifier &universal);

template<>
a_boolean validate(const an_ifc_syntax_simple_type_specifier &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_simple_type_specifier>();

/*
Functions for interacting with IFC SyntaxStatementSeq nodes.
*/

template<>
a_boolean has_ifc_stmts(const an_ifc_syntax_statement_seq &universal);

template<>
an_ifc_syntax_index get_ifc_stmts(
                                 const an_ifc_syntax_statement_seq &universal);

template<>
a_boolean validate(const an_ifc_syntax_statement_seq &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_statement_seq>();

/*
Functions for interacting with IFC SyntaxStaticAssertDeclaration nodes.
*/

template<>
a_boolean has_ifc_comma(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
an_ifc_source_location get_ifc_comma(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
a_boolean has_ifc_condition(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
an_ifc_expr_index get_ifc_condition(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
a_boolean has_ifc_left_paren(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
an_ifc_source_location get_ifc_left_paren(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
a_boolean has_ifc_locus(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
an_ifc_source_location get_ifc_locus(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
a_boolean has_ifc_message(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
an_ifc_expr_index get_ifc_message(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
a_boolean has_ifc_right_paren(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
a_boolean has_ifc_semicolon(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                     const an_ifc_syntax_static_assert_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_static_assert_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_static_assert_declaration>();

/*
Functions for interacting with IFC SyntaxStructuredBindingDeclaration nodes.
*/

template<>
a_boolean has_ifc_initializer(
                const an_ifc_syntax_structured_binding_declaration &universal);

template<>
an_ifc_expr_index get_ifc_initializer(
                const an_ifc_syntax_structured_binding_declaration &universal);

template<>
a_boolean has_ifc_locus(
                const an_ifc_syntax_structured_binding_declaration &universal);

template<>
an_ifc_source_location get_ifc_locus(
                const an_ifc_syntax_structured_binding_declaration &universal);

template<>
a_boolean has_ifc_names(
                const an_ifc_syntax_structured_binding_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_names(
                const an_ifc_syntax_structured_binding_declaration &universal);

template<>
a_boolean has_ifc_ref(
                const an_ifc_syntax_structured_binding_declaration &universal);

template<>
an_ifc_source_location get_ifc_ref(
                const an_ifc_syntax_structured_binding_declaration &universal);

template<>
a_boolean has_ifc_specifiers(
                const an_ifc_syntax_structured_binding_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_specifiers(
                const an_ifc_syntax_structured_binding_declaration &universal);

template<>
a_boolean validate(
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_structured_binding_declaration>();

/*
Functions for interacting with IFC SyntaxStructuredBindingIdentifier nodes.
*/

template<>
a_boolean has_ifc_comma(
                 const an_ifc_syntax_structured_binding_identifier &universal);

template<>
an_ifc_source_location get_ifc_comma(
                 const an_ifc_syntax_structured_binding_identifier &universal);

template<>
a_boolean has_ifc_name(
                 const an_ifc_syntax_structured_binding_identifier &universal);

template<>
an_ifc_expr_index get_ifc_name(
                 const an_ifc_syntax_structured_binding_identifier &universal);

template<>
a_boolean validate(
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_structured_binding_identifier>();

/*
Functions for interacting with IFC SyntaxSuper nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_super &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_syntax_super &universal);

template<>
a_boolean validate(const an_ifc_syntax_super     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_super>();

/*
Functions for interacting with IFC SyntaxSwitchStatement nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_syntax_switch_statement &universal);

template<>
an_ifc_syntax_index get_ifc_body(
                              const an_ifc_syntax_switch_statement &universal);

template<>
a_boolean has_ifc_condition(const an_ifc_syntax_switch_statement &universal);

template<>
an_ifc_syntax_index get_ifc_condition(
                              const an_ifc_syntax_switch_statement &universal);

template<>
a_boolean has_ifc_init(const an_ifc_syntax_switch_statement &universal);

template<>
an_ifc_syntax_index get_ifc_init(
                              const an_ifc_syntax_switch_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_switch_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                              const an_ifc_syntax_switch_statement &universal);

template<>
a_boolean has_ifc_switch(const an_ifc_syntax_switch_statement &universal);

template<>
an_ifc_source_location get_ifc_switch(
                              const an_ifc_syntax_switch_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_switch_statement &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_switch_statement>();

/*
Functions for interacting with IFC SyntaxTemplateArgumentList nodes.
*/

template<>
a_boolean has_ifc_arguments(
                        const an_ifc_syntax_template_argument_list &universal);

template<>
an_ifc_syntax_index get_ifc_arguments(
                        const an_ifc_syntax_template_argument_list &universal);

template<>
a_boolean has_ifc_left_angle(
                        const an_ifc_syntax_template_argument_list &universal);

template<>
an_ifc_source_location get_ifc_left_angle(
                        const an_ifc_syntax_template_argument_list &universal);

template<>
a_boolean has_ifc_right_angle(
                        const an_ifc_syntax_template_argument_list &universal);

template<>
an_ifc_source_location get_ifc_right_angle(
                        const an_ifc_syntax_template_argument_list &universal);

template<>
a_boolean validate(const an_ifc_syntax_template_argument_list &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_template_argument_list>();

/*
Functions for interacting with IFC SyntaxTemplateDeclaration nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_template_declaration &universal);

template<>
an_ifc_source_location get_ifc_locus(
                          const an_ifc_syntax_template_declaration &universal);

template<>
a_boolean has_ifc_parameters(
                          const an_ifc_syntax_template_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_parameters(
                          const an_ifc_syntax_template_declaration &universal);

template<>
a_boolean has_ifc_subject(const an_ifc_syntax_template_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_subject(
                          const an_ifc_syntax_template_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_template_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_template_declaration>();

/*
Functions for interacting with IFC SyntaxTemplateId nodes.
*/

template<>
a_boolean has_ifc_arguments(const an_ifc_syntax_template_id &universal);

template<>
an_ifc_syntax_index get_ifc_arguments(
                                   const an_ifc_syntax_template_id &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_template_id &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                   const an_ifc_syntax_template_id &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_template_id &universal);

template<>
an_ifc_syntax_index get_ifc_name(const an_ifc_syntax_template_id &universal);

template<>
a_boolean has_ifc_symbol(const an_ifc_syntax_template_id &universal);

template<>
an_ifc_expr_index get_ifc_symbol(const an_ifc_syntax_template_id &universal);

template<>
a_boolean has_ifc_template_kw(const an_ifc_syntax_template_id &universal);

template<>
an_ifc_source_location get_ifc_template_kw(
                                   const an_ifc_syntax_template_id &universal);

template<>
a_boolean validate(const an_ifc_syntax_template_id &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_template_id>();

/*
Functions for interacting with IFC SyntaxTemplateParameterList nodes.
*/

template<>
a_boolean has_ifc_clause(
                       const an_ifc_syntax_template_parameter_list &universal);

template<>
an_ifc_syntax_index get_ifc_clause(
                       const an_ifc_syntax_template_parameter_list &universal);

template<>
a_boolean has_ifc_left_angle(
                       const an_ifc_syntax_template_parameter_list &universal);

template<>
an_ifc_source_location get_ifc_left_angle(
                       const an_ifc_syntax_template_parameter_list &universal);

template<>
a_boolean has_ifc_parameters(
                       const an_ifc_syntax_template_parameter_list &universal);

template<>
an_ifc_syntax_index get_ifc_parameters(
                       const an_ifc_syntax_template_parameter_list &universal);

template<>
a_boolean has_ifc_right_angle(
                       const an_ifc_syntax_template_parameter_list &universal);

template<>
an_ifc_source_location get_ifc_right_angle(
                       const an_ifc_syntax_template_parameter_list &universal);

template<>
a_boolean validate(const an_ifc_syntax_template_parameter_list &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_template_parameter_list>();

/*
Functions for interacting with IFC SyntaxTemplateTemplateParameter nodes.
*/

template<>
a_boolean has_ifc_argument(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
an_ifc_syntax_index get_ifc_argument(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
a_boolean has_ifc_comma(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
an_ifc_source_location get_ifc_comma(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
a_boolean has_ifc_ellipsis(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
an_ifc_source_location get_ifc_ellipsis(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
a_boolean has_ifc_key(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
an_ifc_keyword_syntax get_ifc_key(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
a_boolean has_ifc_locus(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
an_ifc_source_location get_ifc_locus(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
a_boolean has_ifc_name(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
an_ifc_text_offset get_ifc_name(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
a_boolean has_ifc_parameters(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
an_ifc_syntax_index get_ifc_parameters(
                   const an_ifc_syntax_template_template_parameter &universal);

template<>
a_boolean validate(const an_ifc_syntax_template_template_parameter &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_template_template_parameter>();

/*
Functions for interacting with IFC SyntaxThisCapture nodes.
*/

template<>
a_boolean has_ifc_asterisk(const an_ifc_syntax_this_capture &universal);

template<>
an_ifc_source_location get_ifc_asterisk(
                                  const an_ifc_syntax_this_capture &universal);

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_this_capture &universal);

template<>
an_ifc_source_location get_ifc_comma(
                                  const an_ifc_syntax_this_capture &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_this_capture &universal);

template<>
an_ifc_source_location get_ifc_locus(
                                  const an_ifc_syntax_this_capture &universal);

template<>
a_boolean validate(const an_ifc_syntax_this_capture &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_this_capture>();

/*
Functions for interacting with IFC SyntaxTrailingReturnType nodes.
*/

template<>
a_boolean has_ifc_arrow(const an_ifc_syntax_trailing_return_type &universal);

template<>
an_ifc_source_location get_ifc_arrow(
                          const an_ifc_syntax_trailing_return_type &universal);

template<>
a_boolean has_ifc_target(const an_ifc_syntax_trailing_return_type &universal);

template<>
an_ifc_syntax_index get_ifc_target(
                          const an_ifc_syntax_trailing_return_type &universal);

template<>
a_boolean validate(const an_ifc_syntax_trailing_return_type &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_trailing_return_type>();

/*
Functions for interacting with IFC SyntaxTryBlock nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_syntax_try_block &universal);

template<>
an_ifc_syntax_index get_ifc_body(const an_ifc_syntax_try_block &universal);

template<>
a_boolean has_ifc_handlers(const an_ifc_syntax_try_block &universal);

template<>
an_ifc_syntax_index get_ifc_handlers(const an_ifc_syntax_try_block &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_try_block &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(const an_ifc_syntax_try_block &universal);

template<>
a_boolean has_ifc_try(const an_ifc_syntax_try_block &universal);

template<>
an_ifc_source_location get_ifc_try(const an_ifc_syntax_try_block &universal);

template<>
a_boolean validate(const an_ifc_syntax_try_block &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_try_block>();

/*
Functions for interacting with IFC SyntaxTuple nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_syntax_tuple &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(const an_ifc_syntax_tuple &universal);

template<>
a_boolean has_ifc_start(const an_ifc_syntax_tuple &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_syntax_tuple &universal);

template<>
a_boolean validate(const an_ifc_syntax_tuple     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_tuple>();

/*
Functions for interacting with IFC SyntaxTypeId nodes.
*/

template<>
a_boolean has_ifc_abstract_declarator(const an_ifc_syntax_type_id &universal);

template<>
an_ifc_syntax_index get_ifc_abstract_declarator(
                                       const an_ifc_syntax_type_id &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_type_id &universal);

template<>
an_ifc_source_location get_ifc_locus(const an_ifc_syntax_type_id &universal);

template<>
a_boolean has_ifc_type_specifier(const an_ifc_syntax_type_id &universal);

template<>
an_ifc_syntax_index get_ifc_type_specifier(
                                       const an_ifc_syntax_type_id &universal);

template<>
a_boolean validate(const an_ifc_syntax_type_id   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_type_id>();

/*
Functions for interacting with IFC SyntaxTypeIdListElement nodes.
*/

template<>
a_boolean has_ifc_ellipsis(
                          const an_ifc_syntax_type_id_list_element &universal);

template<>
an_ifc_source_location get_ifc_ellipsis(
                          const an_ifc_syntax_type_id_list_element &universal);

template<>
a_boolean has_ifc_type_id(const an_ifc_syntax_type_id_list_element &universal);

template<>
an_ifc_syntax_index get_ifc_type_id(
                          const an_ifc_syntax_type_id_list_element &universal);

template<>
a_boolean validate(const an_ifc_syntax_type_id_list_element &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_id_list_element>();

/*
Functions for interacting with IFC SyntaxTypeRequirement nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_type_requirement &universal);

template<>
an_ifc_source_location get_ifc_locus(
                              const an_ifc_syntax_type_requirement &universal);

template<>
a_boolean has_ifc_type(const an_ifc_syntax_type_requirement &universal);

template<>
an_ifc_expr_index get_ifc_type(
                              const an_ifc_syntax_type_requirement &universal);

template<>
a_boolean validate(const an_ifc_syntax_type_requirement &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_type_requirement>();

/*
Functions for interacting with IFC SyntaxTypeSpecifierSeq nodes.
*/

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_type_specifier_seq &universal);

template<>
an_ifc_source_location get_ifc_locus(
                            const an_ifc_syntax_type_specifier_seq &universal);

template<>
a_boolean has_ifc_qualifiers(
                            const an_ifc_syntax_type_specifier_seq &universal);

template<>
an_ifc_qualifier_bitfield get_ifc_qualifiers(
                            const an_ifc_syntax_type_specifier_seq &universal);

template<>
a_boolean has_ifc_type(const an_ifc_syntax_type_specifier_seq &universal);

template<>
an_ifc_type_index get_ifc_type(
                            const an_ifc_syntax_type_specifier_seq &universal);

template<>
a_boolean has_ifc_type_name(const an_ifc_syntax_type_specifier_seq &universal);

template<>
an_ifc_syntax_index get_ifc_type_name(
                            const an_ifc_syntax_type_specifier_seq &universal);

template<>
a_boolean has_ifc_unhashed(const an_ifc_syntax_type_specifier_seq &universal);

template<>
an_ifc_bool get_ifc_unhashed(
                            const an_ifc_syntax_type_specifier_seq &universal);

template<>
a_boolean validate(const an_ifc_syntax_type_specifier_seq &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_specifier_seq>();

/*
Functions for interacting with IFC SyntaxTypeTemplateArgument nodes.
*/

template<>
a_boolean has_ifc_argument(
                        const an_ifc_syntax_type_template_argument &universal);

template<>
an_ifc_syntax_index get_ifc_argument(
                        const an_ifc_syntax_type_template_argument &universal);

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_type_template_argument &universal);

template<>
an_ifc_source_location get_ifc_comma(
                        const an_ifc_syntax_type_template_argument &universal);

template<>
a_boolean has_ifc_ellipsis(
                        const an_ifc_syntax_type_template_argument &universal);

template<>
an_ifc_source_location get_ifc_ellipsis(
                        const an_ifc_syntax_type_template_argument &universal);

template<>
a_boolean validate(const an_ifc_syntax_type_template_argument &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_template_argument>();

/*
Functions for interacting with IFC SyntaxTypeTemplateParameter nodes.
*/

template<>
a_boolean has_ifc_argument(
                       const an_ifc_syntax_type_template_parameter &universal);

template<>
an_ifc_syntax_index get_ifc_argument(
                       const an_ifc_syntax_type_template_parameter &universal);

template<>
a_boolean has_ifc_constraint(
                       const an_ifc_syntax_type_template_parameter &universal);

template<>
an_ifc_syntax_index get_ifc_constraint(
                       const an_ifc_syntax_type_template_parameter &universal);

template<>
a_boolean has_ifc_ellipsis(
                       const an_ifc_syntax_type_template_parameter &universal);

template<>
an_ifc_source_location get_ifc_ellipsis(
                       const an_ifc_syntax_type_template_parameter &universal);

template<>
a_boolean has_ifc_locus(
                       const an_ifc_syntax_type_template_parameter &universal);

template<>
an_ifc_source_location get_ifc_locus(
                       const an_ifc_syntax_type_template_parameter &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_type_template_parameter &universal);

template<>
an_ifc_text_offset get_ifc_name(
                       const an_ifc_syntax_type_template_parameter &universal);

template<>
a_boolean validate(const an_ifc_syntax_type_template_parameter &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_template_parameter>();

/*
Functions for interacting with IFC SyntaxTypeTraitIntrinsic nodes.
*/

template<>
a_boolean has_ifc_arguments(
                          const an_ifc_syntax_type_trait_intrinsic &universal);

template<>
an_ifc_syntax_index get_ifc_arguments(
                          const an_ifc_syntax_type_trait_intrinsic &universal);

template<>
a_boolean has_ifc_intrinsic(
                          const an_ifc_syntax_type_trait_intrinsic &universal);

template<>
an_ifc_operator_category get_ifc_intrinsic(
                          const an_ifc_syntax_type_trait_intrinsic &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_type_trait_intrinsic &universal);

template<>
an_ifc_source_location get_ifc_locus(
                          const an_ifc_syntax_type_trait_intrinsic &universal);

template<>
a_boolean validate(const an_ifc_syntax_type_trait_intrinsic &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_type_trait_intrinsic>();

/*
Functions for interacting with IFC SyntaxUnaryFoldExpression nodes.
*/

template<>
a_boolean has_ifc_direction(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
an_ifc_fold_direction_sort get_ifc_direction(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
a_boolean has_ifc_dyad(const an_ifc_syntax_unary_fold_expression &universal);

template<>
an_ifc_dyadic_operator_sort get_ifc_dyad(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
a_boolean has_ifc_ellipsis(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
an_ifc_source_location get_ifc_ellipsis(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
a_boolean has_ifc_glyph_locus(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
an_ifc_source_location get_ifc_glyph_locus(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_unary_fold_expression &universal);

template<>
an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
a_boolean has_ifc_operand(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
an_ifc_expr_index get_ifc_operand(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
a_boolean has_ifc_right_paren(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
an_ifc_source_location get_ifc_right_paren(
                         const an_ifc_syntax_unary_fold_expression &universal);

template<>
a_boolean validate(const an_ifc_syntax_unary_fold_expression &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_unary_fold_expression>();

/*
Functions for interacting with IFC SyntaxUsingDeclaration nodes.
*/

template<>
a_boolean has_ifc_declarators(
                             const an_ifc_syntax_using_declaration &universal);

template<>
an_ifc_syntax_index get_ifc_declarators(
                             const an_ifc_syntax_using_declaration &universal);

template<>
a_boolean has_ifc_keyword(const an_ifc_syntax_using_declaration &universal);

template<>
an_ifc_source_location get_ifc_keyword(
                             const an_ifc_syntax_using_declaration &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_using_declaration &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                             const an_ifc_syntax_using_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_using_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_using_declaration>();

/*
Functions for interacting with IFC SyntaxUsingDeclarator nodes.
*/

template<>
a_boolean has_ifc_comma(const an_ifc_syntax_using_declarator &universal);

template<>
an_ifc_source_location get_ifc_comma(
                              const an_ifc_syntax_using_declarator &universal);

template<>
a_boolean has_ifc_expander(const an_ifc_syntax_using_declarator &universal);

template<>
an_ifc_source_location get_ifc_expander(
                              const an_ifc_syntax_using_declarator &universal);

template<>
a_boolean has_ifc_qualified_name(
                              const an_ifc_syntax_using_declarator &universal);

template<>
an_ifc_expr_index get_ifc_qualified_name(
                              const an_ifc_syntax_using_declarator &universal);

template<>
a_boolean has_ifc_typename_kw(const an_ifc_syntax_using_declarator &universal);

template<>
an_ifc_source_location get_ifc_typename_kw(
                              const an_ifc_syntax_using_declarator &universal);

template<>
a_boolean validate(const an_ifc_syntax_using_declarator &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_using_declarator>();

/*
Functions for interacting with IFC SyntaxUsingDirective nodes.
*/

template<>
a_boolean has_ifc_namespace_kw(const an_ifc_syntax_using_directive &universal);

template<>
an_ifc_source_location get_ifc_namespace_kw(
                               const an_ifc_syntax_using_directive &universal);

template<>
a_boolean has_ifc_qualified_name(
                               const an_ifc_syntax_using_directive &universal);

template<>
an_ifc_expr_index get_ifc_qualified_name(
                               const an_ifc_syntax_using_directive &universal);

template<>
a_boolean has_ifc_semicolon(const an_ifc_syntax_using_directive &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                               const an_ifc_syntax_using_directive &universal);

template<>
a_boolean has_ifc_using_kw(const an_ifc_syntax_using_directive &universal);

template<>
an_ifc_source_location get_ifc_using_kw(
                               const an_ifc_syntax_using_directive &universal);

template<>
a_boolean validate(const an_ifc_syntax_using_directive &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_using_directive>();

/*
Functions for interacting with IFC SyntaxUsingEnumDeclaration nodes.
*/

template<>
a_boolean has_ifc_enum_kw(
                        const an_ifc_syntax_using_enum_declaration &universal);

template<>
an_ifc_source_location get_ifc_enum_kw(
                        const an_ifc_syntax_using_enum_declaration &universal);

template<>
a_boolean has_ifc_name(const an_ifc_syntax_using_enum_declaration &universal);

template<>
an_ifc_expr_index get_ifc_name(
                        const an_ifc_syntax_using_enum_declaration &universal);

template<>
a_boolean has_ifc_semicolon(
                        const an_ifc_syntax_using_enum_declaration &universal);

template<>
an_ifc_source_location get_ifc_semicolon(
                        const an_ifc_syntax_using_enum_declaration &universal);

template<>
a_boolean has_ifc_using_kw(
                        const an_ifc_syntax_using_enum_declaration &universal);

template<>
an_ifc_source_location get_ifc_using_kw(
                        const an_ifc_syntax_using_enum_declaration &universal);

template<>
a_boolean validate(const an_ifc_syntax_using_enum_declaration &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_using_enum_declaration>();

/*
Functions for interacting with IFC SyntaxVirtualSpecifierSeq nodes.
*/

template<>
a_boolean has_ifc_final_kw(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

template<>
an_ifc_source_location get_ifc_final_kw(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

template<>
a_boolean has_ifc_locus(const an_ifc_syntax_virtual_specifier_seq &universal);

template<>
an_ifc_source_location get_ifc_locus(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

template<>
a_boolean has_ifc_override_kw(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

template<>
an_ifc_source_location get_ifc_override_kw(
                         const an_ifc_syntax_virtual_specifier_seq &universal);

template<>
a_boolean has_ifc_pure(const an_ifc_syntax_virtual_specifier_seq &universal);

template<>
an_ifc_bool get_ifc_pure(const an_ifc_syntax_virtual_specifier_seq &universal);

template<>
a_boolean validate(const an_ifc_syntax_virtual_specifier_seq &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_syntax_virtual_specifier_seq>();

/*
Functions for interacting with IFC SyntaxWhileStatement nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_syntax_while_statement &universal);

template<>
an_ifc_syntax_index get_ifc_body(
                               const an_ifc_syntax_while_statement &universal);

template<>
a_boolean has_ifc_condition(const an_ifc_syntax_while_statement &universal);

template<>
an_ifc_expr_index get_ifc_condition(
                               const an_ifc_syntax_while_statement &universal);

template<>
a_boolean has_ifc_pragma(const an_ifc_syntax_while_statement &universal);

template<>
an_ifc_sentence_index get_ifc_pragma(
                               const an_ifc_syntax_while_statement &universal);

template<>
a_boolean has_ifc_while(const an_ifc_syntax_while_statement &universal);

template<>
an_ifc_source_location get_ifc_while(
                               const an_ifc_syntax_while_statement &universal);

template<>
a_boolean validate(const an_ifc_syntax_while_statement &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_syntax_while_statement>();

/*
Functions for interacting with IFC TraitAliasTemplate nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_trait_alias_template &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_trait_alias_template &universal);

template<>
a_boolean has_ifc_encoded_decl(const an_ifc_trait_alias_template &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                 const an_ifc_trait_alias_template &universal);

template<>
a_boolean has_ifc_trait(const an_ifc_trait_alias_template &universal);

template<>
an_ifc_syntax_index get_ifc_trait(
                                 const an_ifc_trait_alias_template &universal);

template<>
a_boolean validate(const an_ifc_trait_alias_template &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_trait_alias_template>();

/*
Functions for interacting with IFC TraitAttribute nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_trait_attribute &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_trait_attribute &universal);

template<>
a_boolean has_ifc_encoded_decl(const an_ifc_trait_attribute &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                      const an_ifc_trait_attribute &universal);

template<>
a_boolean has_ifc_trait(const an_ifc_trait_attribute &universal);

template<>
an_ifc_attr_index get_ifc_trait(const an_ifc_trait_attribute &universal);

template<>
a_boolean validate(const an_ifc_trait_attribute  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_trait_attribute>();

/*
Functions for interacting with IFC TraitDeductionGuide nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_trait_deduction_guide &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_trait_deduction_guide &universal);

template<>
a_boolean has_ifc_encoded_decl(const an_ifc_trait_deduction_guide &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                const an_ifc_trait_deduction_guide &universal);

template<>
a_boolean has_ifc_trait(const an_ifc_trait_deduction_guide &universal);

template<>
an_ifc_decl_index get_ifc_trait(const an_ifc_trait_deduction_guide &universal);

template<>
a_boolean validate(const an_ifc_trait_deduction_guide &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_trait_deduction_guide>();

/*
Functions for interacting with IFC TraitDeprecated nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_trait_deprecated &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_trait_deprecated &universal);

template<>
a_boolean has_ifc_encoded_decl(const an_ifc_trait_deprecated &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                     const an_ifc_trait_deprecated &universal);

template<>
a_boolean has_ifc_trait(const an_ifc_trait_deprecated &universal);

template<>
an_ifc_text_offset get_ifc_trait(const an_ifc_trait_deprecated &universal);

template<>
a_boolean validate(const an_ifc_trait_deprecated &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_trait_deprecated>();

/*
Functions for interacting with IFC TraitFriend nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_trait_friend &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_trait_friend &universal);

template<>
a_boolean has_ifc_encoded_decl(const an_ifc_trait_friend &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                         const an_ifc_trait_friend &universal);

template<>
a_boolean has_ifc_trait(const an_ifc_trait_friend &universal);

template<>
an_ifc_sequence get_ifc_trait(const an_ifc_trait_friend &universal);

template<>
a_boolean validate(const an_ifc_trait_friend     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_trait_friend>();

/*
Functions for interacting with IFC TraitFunctionDefinition nodes.
*/

template<>
a_boolean has_ifc_body(const an_ifc_trait_function_definition &universal);

template<>
an_ifc_stmt_index get_ifc_body(
                            const an_ifc_trait_function_definition &universal);

template<>
a_boolean has_ifc_decl(const an_ifc_trait_function_definition &universal);

template<>
an_ifc_decl_index get_ifc_decl(
                            const an_ifc_trait_function_definition &universal);

template<>
a_boolean has_ifc_encoded_decl(
                            const an_ifc_trait_function_definition &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                            const an_ifc_trait_function_definition &universal);

template<>
a_boolean has_ifc_initializers(
                            const an_ifc_trait_function_definition &universal);

template<>
an_ifc_expr_index get_ifc_initializers(
                            const an_ifc_trait_function_definition &universal);

template<>
a_boolean has_ifc_parameters(
                            const an_ifc_trait_function_definition &universal);

template<>
an_ifc_chart_index get_ifc_parameters(
                            const an_ifc_trait_function_definition &universal);

template<>
a_boolean validate(const an_ifc_trait_function_definition &universal,
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
an_ifc_partition_kind
get_ifc_partition_kind<an_ifc_trait_function_definition>();

/*
Functions for interacting with IFC TraitMsvcDeclAttrs nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_trait_msvc_decl_attrs &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_trait_msvc_decl_attrs &universal);

template<>
a_boolean has_ifc_encoded_decl(const an_ifc_trait_msvc_decl_attrs &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                const an_ifc_trait_msvc_decl_attrs &universal);

template<>
a_boolean has_ifc_trait(const an_ifc_trait_msvc_decl_attrs &universal);

template<>
an_ifc_attr_index get_ifc_trait(const an_ifc_trait_msvc_decl_attrs &universal);

template<>
a_boolean validate(const an_ifc_trait_msvc_decl_attrs &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_trait_msvc_decl_attrs>();

/*
Functions for interacting with IFC TraitMsvcFuncParams nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_trait_msvc_func_params &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_trait_msvc_func_params &universal);

template<>
a_boolean has_ifc_encoded_decl(const an_ifc_trait_msvc_func_params &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                               const an_ifc_trait_msvc_func_params &universal);

template<>
a_boolean has_ifc_params(const an_ifc_trait_msvc_func_params &universal);

template<>
an_ifc_chart_index get_ifc_params(
                               const an_ifc_trait_msvc_func_params &universal);

template<>
a_boolean validate(const an_ifc_trait_msvc_func_params &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_trait_msvc_func_params>();

/*
Functions for interacting with IFC TraitMsvcUuid nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_trait_msvc_uuid &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_trait_msvc_uuid &universal);

template<>
a_boolean has_ifc_encoded_decl(const an_ifc_trait_msvc_uuid &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                      const an_ifc_trait_msvc_uuid &universal);

template<>
a_boolean has_ifc_uuid(const an_ifc_trait_msvc_uuid &universal);

template<>
an_ifc_uuid get_ifc_uuid(const an_ifc_trait_msvc_uuid &universal);

template<>
a_boolean validate(const an_ifc_trait_msvc_uuid  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_trait_msvc_uuid>();

/*
Functions for interacting with IFC TraitMsvcVendorTrait nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_trait_msvc_vendor_trait &universal);

template<>
an_ifc_decl_index get_ifc_decl(
                              const an_ifc_trait_msvc_vendor_trait &universal);

template<>
a_boolean has_ifc_encoded_decl(
                              const an_ifc_trait_msvc_vendor_trait &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                              const an_ifc_trait_msvc_vendor_trait &universal);

template<>
a_boolean has_ifc_trait(const an_ifc_trait_msvc_vendor_trait &universal);

template<>
an_ifc_msvc_traits_bitfield get_ifc_trait(
                              const an_ifc_trait_msvc_vendor_trait &universal);

template<>
a_boolean validate(const an_ifc_trait_msvc_vendor_trait &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_trait_msvc_vendor_trait>();

/*
Functions for interacting with IFC TraitRequires nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_trait_requires &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_trait_requires &universal);

template<>
a_boolean has_ifc_encoded_decl(const an_ifc_trait_requires &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                       const an_ifc_trait_requires &universal);

template<>
a_boolean has_ifc_trait(const an_ifc_trait_requires &universal);

template<>
an_ifc_syntax_index get_ifc_trait(const an_ifc_trait_requires &universal);

template<>
a_boolean validate(const an_ifc_trait_requires   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_trait_requires>();

/*
Functions for interacting with IFC TraitSpecialization nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_trait_specialization &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_trait_specialization &universal);

template<>
a_boolean has_ifc_encoded_decl(const an_ifc_trait_specialization &universal);

template<>
an_ifc_encoded_decl_index get_ifc_encoded_decl(
                                 const an_ifc_trait_specialization &universal);

template<>
a_boolean has_ifc_trait(const an_ifc_trait_specialization &universal);

template<>
an_ifc_sequence get_ifc_trait(const an_ifc_trait_specialization &universal);

template<>
a_boolean validate(const an_ifc_trait_specialization &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_trait_specialization>();

/*
Functions for interacting with IFC TypeArray nodes.
*/

template<>
a_boolean has_ifc_element(const an_ifc_type_array &universal);

template<>
an_ifc_type_index get_ifc_element(const an_ifc_type_array &universal);

template<>
a_boolean has_ifc_extent(const an_ifc_type_array &universal);

template<>
an_ifc_expr_index get_ifc_extent(const an_ifc_type_array &universal);

template<>
a_boolean validate(const an_ifc_type_array       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_array>();

/*
Functions for interacting with IFC TypeBase nodes.
*/

template<>
a_boolean has_ifc_access(const an_ifc_type_base &universal);

template<>
an_ifc_access_sort get_ifc_access(const an_ifc_type_base &universal);

template<>
a_boolean has_ifc_pack_expanded(const an_ifc_type_base &universal);

template<>
an_ifc_bool get_ifc_pack_expanded(const an_ifc_type_base &universal);

template<>
a_boolean has_ifc_shared(const an_ifc_type_base &universal);

template<>
an_ifc_bool get_ifc_shared(const an_ifc_type_base &universal);

template<>
a_boolean has_ifc_type(const an_ifc_type_base &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_type_base &universal);

template<>
a_boolean validate(const an_ifc_type_base        &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_base>();

/*
Functions for interacting with IFC TypeDecltype nodes.
*/

template<>
a_boolean has_ifc_expr(const an_ifc_type_decltype &universal);

template<>
an_ifc_syntax_index get_ifc_expr(const an_ifc_type_decltype &universal);

template<>
a_boolean validate(const an_ifc_type_decltype    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_decltype>();

/*
Functions for interacting with IFC TypeDesignated nodes.
*/

template<>
a_boolean has_ifc_decl(const an_ifc_type_designated &universal);

template<>
an_ifc_decl_index get_ifc_decl(const an_ifc_type_designated &universal);

template<>
a_boolean validate(const an_ifc_type_designated  &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_designated>();

/*
Functions for interacting with IFC TypeExpansion nodes.
*/

template<>
a_boolean has_ifc_mode(const an_ifc_type_expansion &universal);

template<>
an_ifc_expansion_mode_sort get_ifc_mode(
                                       const an_ifc_type_expansion &universal);

template<>
a_boolean has_ifc_pack(const an_ifc_type_expansion &universal);

template<>
an_ifc_type_index get_ifc_pack(const an_ifc_type_expansion &universal);

template<>
a_boolean validate(const an_ifc_type_expansion   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_expansion>();

/*
Functions for interacting with IFC TypeForall nodes.
*/

template<>
a_boolean has_ifc_chart(const an_ifc_type_forall &universal);

template<>
an_ifc_chart_index get_ifc_chart(const an_ifc_type_forall &universal);

template<>
a_boolean has_ifc_subject(const an_ifc_type_forall &universal);

template<>
an_ifc_type_index get_ifc_subject(const an_ifc_type_forall &universal);

template<>
a_boolean validate(const an_ifc_type_forall      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_forall>();

/*
Functions for interacting with IFC TypeFunction nodes.
*/

template<>
a_boolean has_ifc_convention(const an_ifc_type_function &universal);

template<>
an_ifc_calling_convention_sort get_ifc_convention(
                                        const an_ifc_type_function &universal);

template<>
a_boolean has_ifc_eh_spec(const an_ifc_type_function &universal);

template<>
an_ifc_noexcept_specification get_ifc_eh_spec(
                                        const an_ifc_type_function &universal);

template<>
a_boolean has_ifc_source(const an_ifc_type_function &universal);

template<>
an_ifc_type_index get_ifc_source(const an_ifc_type_function &universal);

template<>
a_boolean has_ifc_target(const an_ifc_type_function &universal);

template<>
an_ifc_type_index get_ifc_target(const an_ifc_type_function &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_type_function &universal);

template<>
an_ifc_function_type_traits_bitfield get_ifc_traits(
                                        const an_ifc_type_function &universal);

template<>
a_boolean validate(const an_ifc_type_function    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_function>();

/*
Functions for interacting with IFC TypeFundamental nodes.
*/

template<>
a_boolean has_ifc_basis(const an_ifc_type_fundamental &universal);

template<>
an_ifc_type_basis_sort get_ifc_basis(const an_ifc_type_fundamental &universal);

template<>
a_boolean has_ifc_precision(const an_ifc_type_fundamental &universal);

template<>
an_ifc_type_precision_sort get_ifc_precision(
                                     const an_ifc_type_fundamental &universal);

template<>
a_boolean has_ifc_sign(const an_ifc_type_fundamental &universal);

template<>
an_ifc_type_sign_sort get_ifc_sign(const an_ifc_type_fundamental &universal);

template<>
a_boolean validate(const an_ifc_type_fundamental &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_fundamental>();

/*
Functions for interacting with IFC TypeLvalueReference nodes.
*/

template<>
a_boolean has_ifc_referee(const an_ifc_type_lvalue_reference &universal);

template<>
an_ifc_type_index get_ifc_referee(
                                const an_ifc_type_lvalue_reference &universal);

template<>
a_boolean validate(const an_ifc_type_lvalue_reference &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_lvalue_reference>();

/*
Functions for interacting with IFC TypeMethod nodes.
*/

template<>
a_boolean has_ifc_convention(const an_ifc_type_method &universal);

template<>
an_ifc_calling_convention_sort get_ifc_convention(
                                          const an_ifc_type_method &universal);

template<>
a_boolean has_ifc_eh_spec(const an_ifc_type_method &universal);

template<>
an_ifc_noexcept_specification get_ifc_eh_spec(
                                          const an_ifc_type_method &universal);

template<>
a_boolean has_ifc_scope(const an_ifc_type_method &universal);

template<>
an_ifc_type_index get_ifc_scope(const an_ifc_type_method &universal);

template<>
a_boolean has_ifc_source(const an_ifc_type_method &universal);

template<>
an_ifc_type_index get_ifc_source(const an_ifc_type_method &universal);

template<>
a_boolean has_ifc_target(const an_ifc_type_method &universal);

template<>
an_ifc_type_index get_ifc_target(const an_ifc_type_method &universal);

template<>
a_boolean has_ifc_traits(const an_ifc_type_method &universal);

template<>
an_ifc_function_type_traits_bitfield get_ifc_traits(
                                          const an_ifc_type_method &universal);

template<>
a_boolean validate(const an_ifc_type_method      &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_method>();

/*
Functions for interacting with IFC TypePlaceholder nodes.
*/

template<>
a_boolean has_ifc_basis(const an_ifc_type_placeholder &universal);

template<>
an_ifc_type_basis_sort get_ifc_basis(const an_ifc_type_placeholder &universal);

template<>
a_boolean has_ifc_constraint(const an_ifc_type_placeholder &universal);

template<>
an_ifc_expr_index get_ifc_constraint(const an_ifc_type_placeholder &universal);

template<>
a_boolean has_ifc_elaboration(const an_ifc_type_placeholder &universal);

template<>
an_ifc_type_index get_ifc_elaboration(
                                     const an_ifc_type_placeholder &universal);

template<>
a_boolean validate(const an_ifc_type_placeholder &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_placeholder>();

/*
Functions for interacting with IFC TypePointer nodes.
*/

template<>
a_boolean has_ifc_pointee(const an_ifc_type_pointer &universal);

template<>
an_ifc_type_index get_ifc_pointee(const an_ifc_type_pointer &universal);

template<>
a_boolean validate(const an_ifc_type_pointer     &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_pointer>();

/*
Functions for interacting with IFC TypePointerToMember nodes.
*/

template<>
a_boolean has_ifc_member(const an_ifc_type_pointer_to_member &universal);

template<>
an_ifc_type_index get_ifc_member(
                               const an_ifc_type_pointer_to_member &universal);

template<>
a_boolean has_ifc_scope(const an_ifc_type_pointer_to_member &universal);

template<>
an_ifc_type_index get_ifc_scope(
                               const an_ifc_type_pointer_to_member &universal);

template<>
a_boolean validate(const an_ifc_type_pointer_to_member &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_pointer_to_member>();

/*
Functions for interacting with IFC TypeQualified nodes.
*/

template<>
a_boolean has_ifc_qualifiers(const an_ifc_type_qualified &universal);

template<>
an_ifc_qualifier_bitfield get_ifc_qualifiers(
                                       const an_ifc_type_qualified &universal);

template<>
a_boolean has_ifc_unqualified(const an_ifc_type_qualified &universal);

template<>
an_ifc_type_index get_ifc_unqualified(const an_ifc_type_qualified &universal);

template<>
a_boolean validate(const an_ifc_type_qualified   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_qualified>();

/*
Functions for interacting with IFC TypeRvalueReference nodes.
*/

template<>
a_boolean has_ifc_referee(const an_ifc_type_rvalue_reference &universal);

template<>
an_ifc_type_index get_ifc_referee(
                                const an_ifc_type_rvalue_reference &universal);

template<>
a_boolean validate(const an_ifc_type_rvalue_reference &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_rvalue_reference>();

/*
Functions for interacting with IFC TypeSyntactic nodes.
*/

template<>
a_boolean has_ifc_expr(const an_ifc_type_syntactic &universal);

template<>
an_ifc_expr_index get_ifc_expr(const an_ifc_type_syntactic &universal);

template<>
a_boolean validate(const an_ifc_type_syntactic   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_syntactic>();

/*
Functions for interacting with IFC TypeSyntaxTree nodes.
*/

template<>
a_boolean has_ifc_syntax(const an_ifc_type_syntax_tree &universal);

template<>
an_ifc_syntax_index get_ifc_syntax(const an_ifc_type_syntax_tree &universal);

template<>
a_boolean validate(const an_ifc_type_syntax_tree &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_syntax_tree>();

/*
Functions for interacting with IFC TypeTor nodes.
*/

template<>
a_boolean has_ifc_convention(const an_ifc_type_tor &universal);

template<>
an_ifc_calling_convention_sort get_ifc_convention(
                                             const an_ifc_type_tor &universal);

template<>
a_boolean has_ifc_eh_spec(const an_ifc_type_tor &universal);

template<>
an_ifc_noexcept_specification get_ifc_eh_spec(
                                             const an_ifc_type_tor &universal);

template<>
a_boolean has_ifc_source(const an_ifc_type_tor &universal);

template<>
an_ifc_type_index get_ifc_source(const an_ifc_type_tor &universal);

template<>
a_boolean validate(const an_ifc_type_tor         &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_tor>();

/*
Functions for interacting with IFC TypeTuple nodes.
*/

template<>
a_boolean has_ifc_cardinality(const an_ifc_type_tuple &universal);

template<>
an_ifc_cardinality get_ifc_cardinality(const an_ifc_type_tuple &universal);

template<>
a_boolean has_ifc_start(const an_ifc_type_tuple &universal);

template<>
an_ifc_index get_ifc_start(const an_ifc_type_tuple &universal);

template<>
a_boolean validate(const an_ifc_type_tuple       &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_tuple>();

/*
Functions for interacting with IFC TypeTypename nodes.
*/

template<>
a_boolean has_ifc_path(const an_ifc_type_typename &universal);

template<>
an_ifc_expr_index get_ifc_path(const an_ifc_type_typename &universal);

template<>
a_boolean validate(const an_ifc_type_typename    &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_typename>();

/*
Functions for interacting with IFC TypeUnaligned nodes.
*/

template<>
a_boolean has_ifc_type(const an_ifc_type_unaligned &universal);

template<>
an_ifc_type_index get_ifc_type(const an_ifc_type_unaligned &universal);

template<>
a_boolean validate(const an_ifc_type_unaligned   &universal,
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
an_ifc_partition_kind get_ifc_partition_kind<an_ifc_type_unaligned>();

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

extern a_boolean validate(an_ifc_expr_index idx);

/*
Visitor functions for retrieving access values from nodes on the DeclIndex.
*/

a_boolean has_ifc_access(an_ifc_decl_index idx);

extern an_ifc_access_sort get_ifc_access(an_ifc_decl_index idx);

/*
Visitor functions for retrieving home_scope values from nodes on the DeclIndex.
*/

a_boolean has_ifc_home_scope(an_ifc_decl_index idx);

extern an_ifc_decl_index get_ifc_home_scope(an_ifc_decl_index idx);

/*
Visitor functions for retrieving locus values from nodes on the DeclIndex.
*/

a_boolean has_ifc_locus(an_ifc_decl_index idx);

extern an_ifc_source_location get_ifc_locus(an_ifc_decl_index idx);

/*
Visitor functions for retrieving name values from nodes on the DeclIndex.
*/

a_boolean has_ifc_name(an_ifc_decl_index idx);

extern an_ifc_name_index get_ifc_name(an_ifc_decl_index idx);

/*
Visitor functions for retrieving locus values from nodes on the ExprIndex.
*/

a_boolean has_ifc_locus(an_ifc_expr_index idx);

extern an_ifc_source_location get_ifc_locus(an_ifc_expr_index idx);

/*
Visitor functions for printing diagnostic textual representations on a given
index.
*/

extern void db_node_at_idx(an_ifc_attr_index idx);

extern void db_node_at_idx(an_ifc_chart_index idx);

extern void db_node_at_idx(an_ifc_decl_index idx);

extern void db_node_at_idx(an_ifc_expr_index idx);

extern void db_node_at_idx(an_ifc_form_index idx);

extern void db_node_at_idx(an_ifc_macro_index idx);

extern void db_node_at_idx(an_ifc_name_index idx);

extern void db_node_at_idx(an_ifc_stmt_index idx);

extern void db_node_at_idx(an_ifc_syntax_index idx);

extern void db_node_at_idx(an_ifc_type_index idx);


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
