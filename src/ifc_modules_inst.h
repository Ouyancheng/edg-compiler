/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2023 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

ifc_modules_inst.h -- Explicit template instantiations needed at link time by
                      ifc_modules.c and ifc_map_functions.c.

** NOTICE: This file is produced by an external script. **

While EDG staff should update the generation script rather than manually
editing this file, customers are welcome to modify this file and create patches
as they see fit.

Please contact EDG Support if you would be interested in using, or learning
more about, the tool that generated this file.
*/

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


/*
Explicit instantiations of functions for AttrIndex.
*/
INST_PARTITION_ALL(an_ifc_attr_index)


/*
Explicit instantiations of functions for ChartIndex.
*/
INST_PARTITION_ALL(an_ifc_chart_index)


/*
Explicit instantiations of functions for DeclIndex.
*/
INST_PARTITION_ALL(an_ifc_decl_index)


/*
Explicit instantiations of functions for ExprIndex.
*/
INST_PARTITION_ALL(an_ifc_expr_index)


/*
Explicit instantiations of functions for FormIndex.
*/
INST_PARTITION_ALL(an_ifc_form_index)


/*
Explicit instantiations of functions for MacroIndex.
*/
INST_PARTITION_ALL(an_ifc_macro_index)


/*
Explicit instantiations of functions for NameIndex.
*/
INST_PARTITION_ALL(an_ifc_name_index)


/*
Explicit instantiations of functions for StmtIndex.
*/
INST_PARTITION_ALL(an_ifc_stmt_index)


/*
Explicit instantiations of functions for SyntaxIndex.
*/
INST_PARTITION_ALL(an_ifc_syntax_index)


/*
Explicit instantiations of functions for TypeIndex.
*/
INST_PARTITION_ALL(an_ifc_type_index)


/*
Explicit instantiations of functions for AttrBasic.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_attr_basic, an_ifc_attr_index)


/*
Explicit instantiations of functions for AttrCalled.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_attr_called, an_ifc_attr_index)


/*
Explicit instantiations of functions for AttrElaborated.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_attr_elaborated, an_ifc_attr_index)


/*
Explicit instantiations of functions for AttrExpanded.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_attr_expanded, an_ifc_attr_index)


/*
Explicit instantiations of functions for AttrFactored.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_attr_factored, an_ifc_attr_index)


/*
Explicit instantiations of functions for AttrLabeled.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_attr_labeled, an_ifc_attr_index)


/*
Explicit instantiations of functions for AttrScoped.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_attr_scoped, an_ifc_attr_index)


/*
Explicit instantiations of functions for AttrTuple.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_attr_tuple, an_ifc_attr_index)


/*
Explicit instantiations of functions for ChartMultilevel.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_chart_multilevel, an_ifc_chart_index)


/*
Explicit instantiations of functions for ChartUnilevel.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_chart_unilevel, an_ifc_chart_index)


/*
Explicit instantiations of functions for DeclAlias.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_alias, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclBitfield.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_bitfield, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclConcept.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_concept, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclConstructor.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_constructor, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclDeductionGuide.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_deduction_guide, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclDestructor.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_destructor, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclEnumeration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_enumeration, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclEnumerator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_enumerator, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclExpansion.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_expansion, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclExplicitInstantiation.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_explicit_instantiation, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclExplicitSpecialization.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_explicit_specialization,
                        an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclField.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_field, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclFriend.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_friend, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclFunction.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_function, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclInheritedConstructor.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_inherited_constructor, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclIntrinsic.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_intrinsic, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclMethod.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_method, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclOutputSegment.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_output_segment, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclParameter.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_parameter, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclPartialSpecialization.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_partial_specialization, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclProperty.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_property, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclReference.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_reference, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclScope.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_scope, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclSpecialization.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_specialization, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclTemplate.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_template, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclTemploid.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_temploid, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclTuple.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_tuple, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclUsingDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_using_declaration, an_ifc_decl_index)


/*
Explicit instantiations of functions for DeclVariable.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_decl_variable, an_ifc_decl_index)


/*
Explicit instantiations of functions for ExprAlignof.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_alignof, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprArrayValue.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_array_value, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprAssignInitializer.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_assign_initializer, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprBinaryFold.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_binary_fold, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprCall.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_call, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprCast.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_cast, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprCompoundString.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_compound_string, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprCondition.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_condition, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprDesignatedInitializer.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_designated_initializer, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprDestructorCall.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_destructor_call, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprDyad.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_dyad, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprDynamicDispatch.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_dynamic_dispatch, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprEmpty.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_empty, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprExpansion.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_expansion, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprExpressionList.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_expression_list, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprFunctionString.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_function_string, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprHierarchyConversion.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_hierarchy_conversion, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprInheritancePath.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_inheritance_path, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprInitializer.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_initializer, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprInitializerList.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_initializer_list, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprLabel.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_label, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprLambda.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_lambda, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprLiteral.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_literal, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprMemberAccess.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_member_access, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprMemberInitializer.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_member_initializer, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprMonad.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_monad, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprNamedDecl.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_named_decl, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprNullptr.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_nullptr, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprPackedTemplateArguments.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_packed_template_arguments,
                        an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprPath.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_path, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprPlaceholder.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_placeholder, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprPointer.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_pointer, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprProductTypeValue.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_product_type_value, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprPushState.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_push_state, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprQualifiedName.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_qualified_name, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprRead.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_read, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprRequires.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_requires, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprSimpleIdentifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_simple_identifier, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprSizeofType.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_sizeof_type, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprString.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_string, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprStringSequence.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_string_sequence, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprSubobjectValue.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_subobject_value, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprSumTypeValue.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_sum_type_value, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprSyntaxTree.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_syntax_tree, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprTemplateId.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_template_id, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprTemplateReference.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_template_reference, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprTemporary.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_temporary, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprThis.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_this, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprTokens.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_tokens, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprTriad.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_triad, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprTuple.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_tuple, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprType.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_type, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprTypeTraitIntrinsic.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_type_trait_intrinsic, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprTypeid.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_typeid, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprUnaryFold.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_unary_fold, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprUnqualifiedId.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_unqualified_id, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprUnresolvedId.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_unresolved_id, an_ifc_expr_index)


/*
Explicit instantiations of functions for ExprVirtualFunctionConversion.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_expr_virtual_function_conversion,
                        an_ifc_expr_index)


/*
Explicit instantiations of functions for FormCatenate.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_catenate, an_ifc_form_index)


/*
Explicit instantiations of functions for FormCharacter.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_character, an_ifc_form_index)


/*
Explicit instantiations of functions for FormHeader.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_header, an_ifc_form_index)


/*
Explicit instantiations of functions for FormIdentifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_identifier, an_ifc_form_index)


/*
Explicit instantiations of functions for FormJunk.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_junk, an_ifc_form_index)


/*
Explicit instantiations of functions for FormKeyword.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_keyword, an_ifc_form_index)


/*
Explicit instantiations of functions for FormNumber.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_number, an_ifc_form_index)


/*
Explicit instantiations of functions for FormOperator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_operator, an_ifc_form_index)


/*
Explicit instantiations of functions for FormParameter.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_parameter, an_ifc_form_index)


/*
Explicit instantiations of functions for FormParenthesized.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_parenthesized, an_ifc_form_index)


/*
Explicit instantiations of functions for FormPragma.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_pragma, an_ifc_form_index)


/*
Explicit instantiations of functions for FormString.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_string, an_ifc_form_index)


/*
Explicit instantiations of functions for FormStringize.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_stringize, an_ifc_form_index)


/*
Explicit instantiations of functions for FormTuple.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_tuple, an_ifc_form_index)


/*
Explicit instantiations of functions for FormWhitespace.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_form_whitespace, an_ifc_form_index)


/*
Explicit instantiations of functions for MacroFunctionLike.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_macro_function_like, an_ifc_macro_index)


/*
Explicit instantiations of functions for MacroObjectLike.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_macro_object_like, an_ifc_macro_index)


/*
Explicit instantiations of functions for NameConversion.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_name_conversion, an_ifc_name_index)


/*
Explicit instantiations of functions for NameGuide.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_name_guide, an_ifc_name_index)


/*
Explicit instantiations of functions for NameLiteral.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_name_literal, an_ifc_name_index)


/*
Explicit instantiations of functions for NameOperator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_name_operator, an_ifc_name_index)


/*
Explicit instantiations of functions for NameSourceFile.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_name_source_file, an_ifc_name_index)


/*
Explicit instantiations of functions for NameSpecialization.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_name_specialization, an_ifc_name_index)


/*
Explicit instantiations of functions for NameTemplate.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_name_template, an_ifc_name_index)


/*
Explicit instantiations of functions for StmtBlock.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_block, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtBreak.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_break, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtCase.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_case, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtContinue.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_continue, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtDecl.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_decl, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtDefault.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_default, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtDoWhile.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_do_while, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtEmpty.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_empty, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtExpansion.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_expansion, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtExpression.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_expression, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtFor.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_for, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtGoto.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_goto, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtHandler.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_handler, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtIf.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_if, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtLabeled.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_labeled, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtReturn.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_return, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtSwitch.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_switch, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtVariableDecl.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_variable_decl, an_ifc_stmt_index)


/*
Explicit instantiations of functions for StmtWhile.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_stmt_while, an_ifc_stmt_index)


/*
Explicit instantiations of functions for SyntaxAccessSpecifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_access_specifier, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxAliasDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_alias_declaration, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxAlignas.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_alignas, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxArrayDeclarator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_array_declarator, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxArrayIndex.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_array_index, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxArrayOrFunctionDeclarator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_array_or_function_declarator,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxAsmStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_asm_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxAttribute.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_attribute, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxAttributeArgumentClause.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_attribute_argument_clause,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxAttributeSpecifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_attribute_specifier,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxAttributeSpecifierSeq.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_attribute_specifier_seq,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxAttributeUsingPrefix.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_attribute_using_prefix,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxAttributedDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_attributed_declaration,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxAttributedStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_attributed_statement,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxBaseSpecifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_base_specifier, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxBaseSpecifierList.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_base_specifier_list,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxBinaryFoldExpression.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_binary_fold_expression,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxBreakStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_break_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxCaptureDefault.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_capture_default, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxClassSpecifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_class_specifier, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxCompoundRequirement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_compound_requirement,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxCompoundStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_compound_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxConceptDefinition.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_concept_definition, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxConditionDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_condition_declaration,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxContinueStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_continue_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxCtorInitializer.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_ctor_initializer, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxDeclSpecifierSeq.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_decl_specifier_seq, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxDeclarationStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_declaration_statement,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxDeclarator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_declarator, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxDecltypeSpecifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_decltype_specifier, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxDoWhileStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_do_while_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxDynamicExceptionSpec.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_dynamic_exception_spec,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxEmptyStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_empty_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxEnumSpecifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_enum_specifier, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxEnumeratorDefinition.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_enumerator_definition,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxExceptionDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_exception_declaration,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxExplicitSpecifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_explicit_specifier, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxExpression.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_expression, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxExpressionStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_expression_statement,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxForRangeDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_for_range_declaration,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxForStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_for_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxFunctionBody.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_function_body, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxFunctionDeclarator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_function_declarator,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxFunctionDefinition.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_function_definition,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxFunctionTryBlock.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_function_try_block, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxGotoStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_goto_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxHandler.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_handler, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxHandlerSeq.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_handler_seq, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxIfStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_if_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxInitCapture.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_init_capture, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxInitDeclarator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_init_declarator, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxInitStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_init_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxLabeledStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_labeled_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxLambdaDeclarator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_lambda_declarator, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxLambdaIntroducer.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_lambda_introducer, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxMemInitializer.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_mem_initializer, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxMemberDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_member_declaration, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxMemberDeclarator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_member_declarator, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxMemberFunctionDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_member_function_declaration,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxMemberSpecification.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_member_specification,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxNamespaceAliasDefinition.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_namespace_alias_definition,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxNestedRequirement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_nested_requirement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxNewDeclarator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_new_declarator, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxNoexceptSpecification.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_noexcept_specification,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxNonTypeTemplateArgument.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_non_type_template_argument,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxParameterDeclarator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_parameter_declarator,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxPlaceholderTypeSpecifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_placeholder_type_specifier,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxPointerDeclarator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_pointer_declarator, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxRangeBasedForStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_range_based_for_statement,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxRequirementBody.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_requirement_body, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxRequiresClause.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_requires_clause, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxReturnStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_return_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxSEHExcept.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_seh_except, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxSEHFinally.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_seh_finally, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxSEHLeave.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_seh_leave, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxSEHTry.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_seh_try, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxSimpleCapture.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_simple_capture, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxSimpleDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_simple_declaration, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxSimpleRequirement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_simple_requirement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxSimpleTypeSpecifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_simple_type_specifier,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxStatementSeq.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_statement_seq, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxStaticAssertDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_static_assert_declaration,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxStructuredBindingDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_structured_binding_declaration,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxStructuredBindingIdentifier.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_structured_binding_identifier,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxSuper.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_super, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxSwitchStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_switch_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTemplateArgumentList.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_template_argument_list,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTemplateDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_template_declaration,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTemplateId.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_template_id, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTemplateParameterList.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_template_parameter_list,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTemplateTemplateParameter.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_template_template_parameter,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxThisCapture.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_this_capture, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTrailingReturnType.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_trailing_return_type,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTryBlock.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_try_block, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTuple.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_tuple, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTypeId.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_type_id, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTypeIdListElement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_type_id_list_element,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTypeRequirement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_type_requirement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTypeSpecifierSeq.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_type_specifier_seq, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTypeTemplateArgument.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_type_template_argument,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTypeTemplateParameter.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_type_template_parameter,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxTypeTraitIntrinsic.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_type_trait_intrinsic,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxUnaryFoldExpression.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_unary_fold_expression,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxUsingDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_using_declaration, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxUsingDeclarator.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_using_declarator, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxUsingDirective.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_using_directive, an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxUsingEnumDeclaration.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_using_enum_declaration,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxVirtualSpecifierSeq.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_virtual_specifier_seq,
                        an_ifc_syntax_index)


/*
Explicit instantiations of functions for SyntaxWhileStatement.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_syntax_while_statement, an_ifc_syntax_index)


/*
Explicit instantiations of functions for TypeArray.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_array, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeBase.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_base, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeDecltype.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_decltype, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeDesignated.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_designated, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeExpansion.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_expansion, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeForall.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_forall, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeFunction.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_function, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeFundamental.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_fundamental, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeLvalueReference.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_lvalue_reference, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeMethod.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_method, an_ifc_type_index)


/*
Explicit instantiations of functions for TypePlaceholder.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_placeholder, an_ifc_type_index)


/*
Explicit instantiations of functions for TypePointer.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_pointer, an_ifc_type_index)


/*
Explicit instantiations of functions for TypePointerToMember.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_pointer_to_member, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeQualified.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_qualified, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeRvalueReference.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_rvalue_reference, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeSyntactic.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_syntactic, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeSyntaxTree.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_syntax_tree, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeTor.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_tor, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeTuple.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_tuple, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeTypename.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_typename, an_ifc_type_index)


/*
Explicit instantiations of functions for TypeUnaligned.
*/
INST_CONSTRUCT_NODE_ALL(an_ifc_type_unaligned, an_ifc_type_index)


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2023 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
