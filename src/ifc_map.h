/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2017-2020 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

ifc_map.h -- Mapping of IFC fields for Microsoft modules

*/

/*
It must be possible to include this file more than once, so it intentionally
does not have an include guard.
*/

/*
The Compiled Module Interface IFC Binary Format document describes the format
of a compiled Microsoft module.  This header file captures the layouts of
each of the entities contained therein, and is used to automatically create
types and/or functions that allow the front end to access various fields
from these entities.

When including this header file, the caller must provide macro definitions for
IFC_DECL_START, IFC_DECL_FIELD, and IFC_DECL_END (which are #undef'ed at the
end of this file).
*/

/* File_Header */
IFC_DECL_START(File_Header)
  IFC_DECL_FIELD(major_version, Version)
  IFC_DECL_FIELD(minor_version, Version)
  IFC_DECL_FIELD(abi, Abi)
  IFC_DECL_FIELD(arch, Architecture)
  IFC_DECL_FIELD(string_table_bytes, ByteOffset)
  IFC_DECL_FIELD(string_table_size, Cardinality)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(source, TextOffset)
  IFC_DECL_FIELD(global_scope, ScopeIndex)
  IFC_DECL_FIELD(toc, ByteOffset)
  IFC_DECL_FIELD(partition_count, Cardinality)
IFC_DECL_END(File_Header)

/* Partition */
IFC_DECL_START(Partition)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(offset, ByteOffset)
  IFC_DECL_FIELD(cardinality, Cardinality)
  IFC_DECL_FIELD(entry_size, EntitySize)
IFC_DECL_END(Partition)

/* Scope::Descriptor */
IFC_DECL_START(Scope_Descriptor)
  IFC_DECL_FIELD(start, Index)
  IFC_DECL_FIELD(cardinality, Cardinality)
IFC_DECL_END(Scope_Descriptor)

/* Scope::Member */
IFC_DECL_START(Scope_Member)
  IFC_DECL_FIELD(index, DeclIndex)
IFC_DECL_END(Scope_Member)

/* DeclSort::Enumerator */
IFC_DECL_START(DeclSort_Enumerator)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(specifier, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_Enumerator)

/* DeclSort::Variable */
IFC_DECL_START(DeclSort_Variable)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(alignment, Alignment)
  IFC_DECL_FIELD(traits, ObjectTraits)
  IFC_DECL_FIELD(specifier, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_Variable)

#if 0
/* DeclSort::FunctionParameter */
IFC_DECL_START(DeclSort_FunctionParameter)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(specifier, BasicSpecifiers)
IFC_DECL_END(DeclSort_FunctionParameter)
#endif /* 0 */

/* DeclSort::Field */
IFC_DECL_START(DeclSort_Field)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(alignment, Alignment)
  IFC_DECL_FIELD(traits, ObjectTraits)
  IFC_DECL_FIELD(specifier, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_Field)

/* DeclSort::Bitfield */
IFC_DECL_START(DeclSort_Bitfield)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(width, Cardinality)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(traits, ObjectTraits)
  IFC_DECL_FIELD(specifier, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_Bitfield)

/* DeclSort::Scope */
IFC_DECL_START(DeclSort_Scope)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(base, TypeIndex)
  IFC_DECL_FIELD(initializer, ScopeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(alignment, Alignment)
  IFC_DECL_FIELD(pack_size, PackSize)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(traits, ScopeTraits)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_Scope)

/* DeclSort::Enumeration */
IFC_DECL_START(DeclSort_Enumeration)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(base, TypeIndex)
  IFC_DECL_FIELD(initializer, Sequence)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(alignment, Alignment)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_Enumeration)

/* DeclSort::TypeAlias */
IFC_DECL_START(DeclSort_TypeAlias)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(initializer, TypeIndex)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_TypeAlias)

/* DeclSort::Intrinsic */
IFC_DECL_START(DeclSort_Intrinsic)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_Intrinsic)

/* DeclSort::Function */
IFC_DECL_START(DeclSort_Function)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(default_arguments, ExprIndex)
  IFC_DECL_FIELD(traits, FunctionTraits)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_Function)

/* DeclSort::Method */
IFC_DECL_START(DeclSort_Method)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(default_arguments, ExprIndex)
  IFC_DECL_FIELD(traits, FunctionTraits)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_Method)

/* DeclSort::Constructor */
IFC_DECL_START(DeclSort_Constructor)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(source, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(eh_spec, NoexceptSpecification)
  IFC_DECL_FIELD(default_arguments, ExprIndex)
  IFC_DECL_FIELD(traits, FunctionTraits)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(convention, CallingConvention)
IFC_DECL_END(DeclSort_Constructor)

/* DeclSort::Destructor */
IFC_DECL_START(DeclSort_Destructor)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(eh_spec, NoexceptSpecification)
  IFC_DECL_FIELD(traits, FunctionTraits)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(convention, CallingConvention)
IFC_DECL_END(DeclSort_Destructor)

#if 0
/* DeclSort::Reference */
IFC_DECL_START(DeclSort_Reference)
  IFC_DECL_FIELD(owner, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(local_index, DeclIndex)
IFC_DECL_END(DeclSort_Reference)

/* DeclSort::UsingDirective */
IFC_DECL_START(DeclSort_UsingDirective)
  IFC_DECL_FIELD(index, DeclIndex)
IFC_DECL_END(DeclSort_UsingDirective)
#endif /* 0 */

/* TypeSort::Fundamental */
IFC_DECL_START(TypeSort_Fundamental)
  IFC_DECL_FIELD(basis, TypeBasis)
  IFC_DECL_FIELD(precision, TypePrecision)
  IFC_DECL_FIELD(sign, TypeSign)
IFC_DECL_END(TypeSort_Fundamental)

/* TypeSort::Pointer */
IFC_DECL_START(TypeSort_Pointer)
  IFC_DECL_FIELD(pointee, TypeIndex)
IFC_DECL_END(TypeSort_Pointer)

/* TypeSort::PointerToMember */
IFC_DECL_START(TypeSort_PointerToMember)
  IFC_DECL_FIELD(scope, TypeIndex)
  IFC_DECL_FIELD(member, TypeIndex)
IFC_DECL_END(TypeSort_PointerToMember)

/* TypeSort::LvalueReference */
IFC_DECL_START(TypeSort_LvalueReference)
  IFC_DECL_FIELD(referee, TypeIndex)
IFC_DECL_END(TypeSort_LvalueReference)

/* TypeSort::RvalueReference */
IFC_DECL_START(TypeSort_RvalueReference)
  IFC_DECL_FIELD(referee, TypeIndex)
IFC_DECL_END(TypeSort_RvalueReference)

/* TypeSort::Designated */
IFC_DECL_START(TypeSort_Designated)
  IFC_DECL_FIELD(decl, DeclIndex)
IFC_DECL_END(TypeSort_Designated)

/* TypeSort::Function */
IFC_DECL_START(TypeSort_Function)
  IFC_DECL_FIELD(target, TypeIndex)
  IFC_DECL_FIELD(source, TypeIndex)
  IFC_DECL_FIELD(eh_spec, NoexceptSpecification)
  IFC_DECL_FIELD(convention, CallingConvention)
  IFC_DECL_FIELD(traits, FunctionTypeTraits)
IFC_DECL_END(TypeSort_Function)

#if 0
/* TypeSort::Method */
IFC_DECL_START(TypeSort_Method)
  IFC_DECL_FIELD(target, TypeIndex)
  IFC_DECL_FIELD(source, TypeIndex)
  IFC_DECL_FIELD(scope, TypeIndex)
  IFC_DECL_FIELD(eh_spec, NoexceptSpecification)
  IFC_DECL_FIELD(convention, CallingConvention)
  IFC_DECL_FIELD(traits, FunctionTypeTraits)
IFC_DECL_END(TypeSort_Method)
#endif /* 0 */

/* TypeSort::Array */
IFC_DECL_START(TypeSort_Array)
  IFC_DECL_FIELD(element, TypeIndex)
  IFC_DECL_FIELD(extent, ExprIndex)
IFC_DECL_END(TypeSort_Array)

#if 0
/* TypeSort::Typename */
IFC_DECL_START(TypeSort_Typename)
  IFC_DECL_FIELD(path, ExprIndex)
IFC_DECL_END(TypeSort_Typename)
#endif /* 0 */

/* TypeSort::Qualified */
IFC_DECL_START(TypeSort_Qualified)
  IFC_DECL_FIELD(unqualified, TypeIndex)
  IFC_DECL_FIELD(qualifiers, Qualifiers)
IFC_DECL_END(TypeSort_Qualified)

/* TypeSort::Base */
IFC_DECL_START(TypeSort_Base)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(shared, bool)
  IFC_DECL_FIELD(pack_expanded, bool)
IFC_DECL_END(TypeSort_Base)

/* TypeSort::Tuple */
IFC_DECL_START(TypeSort_Tuple)
  IFC_DECL_FIELD(start, Index)
  IFC_DECL_FIELD(cardinality, Cardinality)
IFC_DECL_END(TypeSort_Tuple)

/* Source::Line */
IFC_DECL_START(Source_Line)
  IFC_DECL_FIELD(file, NameIndex)
  IFC_DECL_FIELD(line, LineNumber)
IFC_DECL_END(Source_Line)

#if 0
/* ExprSort::Empty */
IFC_DECL_START(ExprSort_Empty)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_Empty)
#endif /* 0 */

/* ExprSort::Literal */
IFC_DECL_START(ExprSort_Literal)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(value, LitIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_Literal)

#if 0
/* ExprSort::Type */
IFC_DECL_START(ExprSort_Type)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_Type)

/* ExprSort::NamedDecl */
IFC_DECL_START(ExprSort_NamedDecl)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(resolution, DeclIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_NamedDecl)

/* ExprSort::Unresolved */
IFC_DECL_START(ExprSort_Unresolved)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_Unresolved)

/* ExprSort::TemplateId */
IFC_DECL_START(ExprSort_TemplateId)
  IFC_DECL_FIELD(primary, ExprIndex)
  IFC_DECL_FIELD(arguments, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_TemplateId)

/* ExprSort::Path */
IFC_DECL_START(ExprSort_Path)
  IFC_DECL_FIELD(scope, ExprIndex)
  IFC_DECL_FIELD(member, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_Path)

/* ExprSort::Read */
IFC_DECL_START(ExprSort_Read)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(address, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_Read)

/* ExprSort::Monad */
IFC_DECL_START(ExprSort_Monad)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(argument, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(opcat, OperatorCategory)
IFC_DECL_END(ExprSort_Monad)

/* ExprSort::Dyad */
IFC_DECL_START(ExprSort_Dyad)
  IFC_DECL_FIELD(type, TypeIndex)
  /* IFC_DECL_FIELD(arguments, ExprIndex[2])  re-written: */
  IFC_DECL_FIELD(arguments_0, ExprIndex)
  IFC_DECL_FIELD(arguments_1, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(opcat, OperatorCategory)
IFC_DECL_END(ExprSort_Dyad)

/* ExprSort::Triad */
IFC_DECL_START(ExprSort_Triad)
  IFC_DECL_FIELD(type, TypeIndex)
  /* IFC_DECL_FIELD(arguments, ExprIndex[3]) re-written: */
  IFC_DECL_FIELD(arguments_0, ExprIndex)
  IFC_DECL_FIELD(arguments_1, ExprIndex)
  IFC_DECL_FIELD(arguments_2, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(opcat, OperatorCategory)
IFC_DECL_END(ExprSort_Triad)

/* ExprSort::Tuple */
IFC_DECL_START(ExprSort_Tuple)
  IFC_DECL_FIELD(start, Index)
  IFC_DECL_FIELD(cardinality, Cardinality)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_Tuple)

/* ExprSort::Tokens */
IFC_DECL_START(ExprSort_Tokens)
  IFC_DECL_FIELD(tokens, SentenceOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_Tokens)

/* ExprSort::String */
IFC_DECL_START(ExprSort_String)
  IFC_DECL_FIELD(string_index, StringIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_String)

/* ExprSort::Temporary */
IFC_DECL_START(ExprSort_Temporary)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(id, UniqueID)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_Temporary)

/* ExprSort::Call */
IFC_DECL_START(ExprSort_Call)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(operation, ExprIndex)
  IFC_DECL_FIELD(arguments, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(opcat, OperatorCategory)
IFC_DECL_END(ExprSort_Call)

/* ExprSort::PushState */
IFC_DECL_START(ExprSort_PushState)
  IFC_DECL_FIELD(ctor_call, ExprIndex)
  IFC_DECL_FIELD(dtor_call, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(flags, EHFlags)
IFC_DECL_END(ExprSort_PushState)

/* ExprSort::TypeTraitIntrinsic */
IFC_DECL_START(ExprSort_TypeTraitIntrinsic)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(shape, TokenCategory)
  IFC_DECL_FIELD(kind, TokenCategory)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_TypeTraitIntrinsic)

/* ExprSort::MemberInitializer */
IFC_DECL_START(ExprSort_MemberInitializer)
  IFC_DECL_FIELD(member, DeclIndex)
  IFC_DECL_FIELD(base, TypeIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_MemberInitializer)

/* ExprSort::MemberAccess */
IFC_DECL_START(ExprSort_MemberAccess)
  IFC_DECL_FIELD(member, DeclIndex)
  IFC_DECL_FIELD(offset, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_MemberAccess)

/* ExprSort::InheritancePath */
IFC_DECL_START(ExprSort_InheritancePath)
  IFC_DECL_FIELD(path, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_InheritancePath)

/* ExprSort::TemplateReference */
IFC_DECL_START(ExprSort_TemplateReference)
  IFC_DECL_FIELD(scope, TypeIndex)
  IFC_DECL_FIELD(member, DeclIndex)
  IFC_DECL_FIELD(arguments, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_TemplateReference)
#endif /* 0 */

/* NameSort::SourceFile */
IFC_DECL_START(NameSort_SourceFile)
  IFC_DECL_FIELD(path, TextOffset)
  IFC_DECL_FIELD(guard, TextOffset)
IFC_DECL_END(NameSort_SourceFile)

/* NameSort::Operator */
IFC_DECL_START(NameSort_Operator)
  IFC_DECL_FIELD(encoded, TextOffset)
  IFC_DECL_FIELD(category, OperatorCategory)
IFC_DECL_END(NameSort_Operator)

/* NameSort::Conversion */
IFC_DECL_START(NameSort_Conversion)
  IFC_DECL_FIELD(target, TypeIndex)
  IFC_DECL_FIELD(syntax, SyntaxIndex)
  IFC_DECL_FIELD(encoded, TextOffset)
IFC_DECL_END(NameSort_Conversion)

/* NameSort::Literal */
IFC_DECL_START(NameSort_Literal)
  IFC_DECL_FIELD(encoded, TextOffset)
IFC_DECL_END(NameSort_Literal)

/* #undef macros that were set by the #includer. */
#undef IFC_DECL_START
#undef IFC_DECL_FIELD
#undef IFC_DECL_END


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
