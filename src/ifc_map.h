/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2017-2021 Edison Design Group Inc.                   [_]          *
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
end of this file).  In addition, if behavior differs when the entity in
question is known to be stored in a little-endian formet, IFC_LE_DECL_XXX
variants must also be defined.  If these variants are not defined, the regular
version is used.
*/
#ifndef IFC_DECL_START
#error IFC_DECL_START not defined
#endif /* ifndef IFC_DECL_START */

#ifndef IFC_DECL_FIELD
#error IFC_DECL_FIELD not defined
#endif /* ifndef IFC_DECL_FIELD */

#ifndef IFC_DECL_END
#error IFC_DECL_END not defined
#endif /* ifndef IFC_DECL_END */

#ifndef IFC_LE_DECL_START
#define IFC_LE_DECL_START(name) IFC_DECL_START(name)
#endif /* ifndef IFC_LE_DECL_START */

#ifndef IFC_LE_DECL_FIELD
#define IFC_LE_DECL_FIELD(field, type) IFC_DECL_FIELD(field, type)
#endif /* ifndef IFC_LE_DECL_FIELD */

#ifndef IFC_LE_DECL_END
#define IFC_LE_DECL_END(name) IFC_DECL_END(name)
#endif /* ifndef IFC_LE_DECL_END */

/* File_Header */
IFC_LE_DECL_START(File_Header)
  IFC_LE_DECL_FIELD(checksum, Checksum)
  IFC_LE_DECL_FIELD(major_version, Version)
  IFC_LE_DECL_FIELD(minor_version, Version)
  IFC_LE_DECL_FIELD(abi, Abi)
  IFC_LE_DECL_FIELD(arch, Architecture)
  IFC_LE_DECL_FIELD(dialect, LanguageVersion)
  IFC_LE_DECL_FIELD(string_table_bytes, ByteOffset)
  IFC_LE_DECL_FIELD(string_table_size, Cardinality)
  IFC_LE_DECL_FIELD(unit, UnitIndex)
  IFC_LE_DECL_FIELD(src_path, TextOffset)
  IFC_LE_DECL_FIELD(global_scope, ScopeIndex)
  IFC_LE_DECL_FIELD(toc, ByteOffset)
  IFC_LE_DECL_FIELD(partition_count, Cardinality)
  IFC_LE_DECL_FIELD(internal, bool)
IFC_LE_DECL_END(File_Header)

/* Partition */
IFC_LE_DECL_START(Partition)
  IFC_LE_DECL_FIELD(name, TextOffset)
  IFC_LE_DECL_FIELD(offset, ByteOffset)
  IFC_LE_DECL_FIELD(cardinality, Cardinality)
  IFC_LE_DECL_FIELD(entry_size, EntitySize)
IFC_LE_DECL_END(Partition)

/* Scope::Descriptor */
IFC_DECL_START(Scope_Descriptor)
  IFC_DECL_FIELD(start, Index)
  IFC_DECL_FIELD(cardinality, Cardinality)
IFC_DECL_END(Scope_Descriptor)

/* Scope::Member */
IFC_DECL_START(Scope_Member)
  IFC_DECL_FIELD(index, DeclIndex)
IFC_DECL_END(Scope_Member)

/* DeclSort::VendorExtension */
IFC_DECL_START(DeclSort_VendorExtension)
IFC_DECL_END(DeclSort_VendorExtension)

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
  IFC_DECL_FIELD(alignment, ExprIndex)
  IFC_DECL_FIELD(traits, ObjectTraits)
  IFC_DECL_FIELD(specifier, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_Variable)

/* DeclSort::Parameter */
IFC_DECL_START(DeclSort_Parameter)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(constraint, ExprIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(level, ParameterLevel)
  IFC_DECL_FIELD(position, ParameterPosition)
  IFC_DECL_FIELD(sort, ParameterSort)
  IFC_DECL_FIELD(properties, ReachableProperties)
  IFC_DECL_FIELD(pack, bool)
IFC_DECL_END(DeclSort_Parameter)

/* DeclSort::Field */
IFC_DECL_START(DeclSort_Field)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(alignment, ExprIndex)
  IFC_DECL_FIELD(traits, ObjectTraits)
  IFC_DECL_FIELD(specifier, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_Field)

/* DeclSort::Bitfield */
IFC_DECL_START(DeclSort_Bitfield)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(width, ExprIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(traits, ObjectTraits)
  IFC_DECL_FIELD(specifier, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_Bitfield)

/* DeclSort::Scope */
IFC_DECL_START(DeclSort_Scope)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(base, TypeIndex)
  IFC_DECL_FIELD(initializer, ScopeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(alignment, ExprIndex)
  IFC_DECL_FIELD(pack_size, PackSize)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(traits, ScopeTraits)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_Scope)

/* DeclSort::Enumeration */
IFC_DECL_START(DeclSort_Enumeration)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(base, TypeIndex)
  IFC_DECL_FIELD(initializer, Sequence)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(alignment, ExprIndex)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_Enumeration)

/* DeclSort::Alias */
IFC_DECL_START(DeclSort_Alias)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(aliasee, TypeIndex)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_TypeAlias)

/* DeclSort::Temploid */
IFC_DECL_START(DeclSort_Temploid)
  IFC_DECL_FIELD(entity, ParameterizedEntity)
  IFC_DECL_FIELD(chart, ChartIndex)
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_Temploid)

/* DeclSort::Template */
IFC_DECL_START(DeclSort_Template)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(chart, ChartIndex)
  IFC_DECL_FIELD(entity, ParameterizedEntity)
/* Spec omits this and it may be removed. */
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_Template)

/* DeclSort::PartialSpecialization */
IFC_DECL_START(DeclSort_PartialSpecialization)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(chart, ChartIndex)
  IFC_DECL_FIELD(entity, ParameterizedEntity)
  IFC_DECL_FIELD(form, Index)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_PartialSpecialization)

/* DeclSort::ExplicitSpecialization */
IFC_DECL_START(DeclSort_ExplicitSpecialization)
  IFC_DECL_FIELD(form, Index)
  IFC_DECL_FIELD(decl, DeclIndex)
IFC_DECL_END(DeclSort_ExplicitSpecialization)

/* DeclSort::ExplicitInstantiation */
IFC_DECL_START(DeclSort_ExplicitInstantiation)
  IFC_DECL_FIELD(form, Index)
  IFC_DECL_FIELD(decl, DeclIndex)
IFC_DECL_END(DeclSort_ExplicitInstantiation)

/* DeclSort::Concept */
IFC_DECL_START(DeclSort_Concept)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(chart, ChartIndex)
  IFC_DECL_FIELD(constraint, ExprIndex)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(head, SentenceIndex)
  IFC_DECL_FIELD(body, SentenceIndex)
IFC_DECL_END(DeclSort_Concept)

/* DeclSort::Function */
IFC_DECL_START(DeclSort_Function)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(chart, ChartIndex)
  IFC_DECL_FIELD(traits, FunctionTraits)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_Function)

/* DeclSort::Method */
IFC_DECL_START(DeclSort_Method)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(chart, ChartIndex)
  IFC_DECL_FIELD(traits, FunctionTraits)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_Method)

/* DeclSort::Constructor */
IFC_DECL_START(DeclSort_Constructor)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(source, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(eh_spec, NoexceptSpecification)
  IFC_DECL_FIELD(chart, ChartIndex) // ?
  IFC_DECL_FIELD(traits, FunctionTraits)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(convention, CallingConvention)
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_Constructor)

/* DeclSort::InheritedConstructor */
IFC_DECL_START(DeclSort_InheritedConstructor)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(source, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(eh_spec, NoexceptSpecification)
  IFC_DECL_FIELD(chart, ChartIndex)
  IFC_DECL_FIELD(traits, FunctionTraits)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(convention, CallingConvention)
  IFC_DECL_FIELD(__padding1__, u8)
  IFC_DECL_FIELD(__padding2__, u16)
  IFC_DECL_FIELD(base_ctor, DeclIndex)
IFC_DECL_END(DeclSort_InheritedConstructor)

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
  IFC_DECL_FIELD(properties, ReachableProperties)
IFC_DECL_END(DeclSort_Destructor)

/* DeclSort::Reference */
IFC_DECL_START(DeclSort_Reference)
  IFC_DECL_FIELD(unit, ModuleReference)
  IFC_DECL_FIELD(local_index, DeclIndex)
IFC_DECL_END(DeclSort_Reference)

/* DeclSort::UsingDeclaration */
IFC_DECL_START(DeclSort_UsingDeclaration)
  /* The IFC spec says this is a NameIndex, however, the contents of the IFC
     files indicate it's a TextOffset. */
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(resolution, DeclIndex)
  IFC_DECL_FIELD(parent, ExprIndex)
  IFC_DECL_FIELD(name2, TextOffset)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
  IFC_DECL_FIELD(hidden, bool)
IFC_DECL_END(DeclSort_UsingDeclaration)

/* DeclSort::UsingDirective */
IFC_DECL_START(DeclSort_UsingDirective)
IFC_DECL_END(DeclSort_UsingDirective)

/* DeclSort::Friend */
IFC_DECL_START(DeclSort_Friend)
  IFC_DECL_FIELD(entity, TypeIndex)
IFC_DECL_END(DeclSort_Friend)

/* DeclSort::Expansion */
IFC_DECL_START(DeclSort_Expansion)
  IFC_DECL_FIELD(operand, DeclIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(DeclSort_Expansion)

/* DeclSort::DeductionGuide */
IFC_DECL_START(DeclSort_DeductionGuide)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(source, ChartIndex)
  IFC_DECL_FIELD(target, ExprIndex)
  IFC_DECL_FIELD(traits, GuideTraits)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
IFC_DECL_END(DeclSort_DeductionGuide)

/* DeclSort::Barren */
IFC_DECL_START(DeclSort_Barren)
IFC_DECL_END(DeclSort_Barren)

/* DeclSort::Tuple */
IFC_DECL_START(DeclSort_Tuple)
  IFC_DECL_FIELD(start, Index)
  IFC_DECL_FIELD(cardinality, Cardinality)
IFC_DECL_END(DeclSort_Tuple)

/* DeclSort::SyntaxTree */
IFC_DECL_START(DeclSort_SyntaxTree)
IFC_DECL_END(DeclSort_SyntaxTree)

/* DeclSort::Intrinsic */
IFC_DECL_START(DeclSort_Intrinsic)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(home_scope, DeclIndex)
  IFC_DECL_FIELD(specifiers, BasicSpecifiers)
  IFC_DECL_FIELD(access, Access)
IFC_DECL_END(DeclSort_Intrinsic)

/* DeclSort::Property */
IFC_DECL_START(DeclSort_Property)
  IFC_DECL_FIELD(member, DeclIndex)
  IFC_DECL_FIELD(getter, TextOffset)
  IFC_DECL_FIELD(setter, TextOffset)
IFC_DECL_END(DeclSort_Property)

/* DeclSort::OutputSegment */
IFC_DECL_START(DeclSort_OutputSegment)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(ID, TextOffset)
  IFC_DECL_FIELD(traits, SegmentTraits)
  IFC_DECL_FIELD(type, SegmentType)
IFC_DECL_END(DeclSort_OutputSegment)

/* TypeSort::VendorExtension */
IFC_DECL_START(TypeSort_VendorExtension)
IFC_DECL_END(TypeSort_VendorExtension)

/* TypeSort::Fundamental */
IFC_DECL_START(TypeSort_Fundamental)
  IFC_DECL_FIELD(basis, TypeBasis)
  IFC_DECL_FIELD(precision, TypePrecision)
  IFC_DECL_FIELD(sign, TypeSign)
  IFC_DECL_FIELD(__padding__, u8)
IFC_DECL_END(TypeSort_Fundamental)

/* TypeSort::Designated */
IFC_DECL_START(TypeSort_Designated)
  IFC_DECL_FIELD(decl, DeclIndex)
IFC_DECL_END(TypeSort_Designated)

/* TypeSort::Deduced */
/* As of IFC version 0.30, this is no longer a valid type. */
IFC_DECL_START(TypeSort_Deduced)
  IFC_DECL_FIELD(return_type, TypeIndex)
IFC_DECL_END(TypeSort_Deduced)

/* TypeSort::Syntactic */
IFC_DECL_START(TypeSort_Syntactic)
  IFC_DECL_FIELD(expr, ExprIndex)
IFC_DECL_END(TypeSort_Syntactic)

/* TypeSort::Expansion */
IFC_DECL_START(TypeSort_Expansion)
  IFC_DECL_FIELD(pack, TypeIndex)
  IFC_DECL_FIELD(mode, ExpansionMode)
IFC_DECL_END(TypeSort_Expansion)

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

/* TypeSort::Function */
IFC_DECL_START(TypeSort_Function)
  IFC_DECL_FIELD(target, TypeIndex)
  IFC_DECL_FIELD(source, TypeIndex)
  IFC_DECL_FIELD(eh_spec, NoexceptSpecification)
  IFC_DECL_FIELD(convention, CallingConvention)
  IFC_DECL_FIELD(traits, FunctionTypeTraits)
IFC_DECL_END(TypeSort_Function)

/* TypeSort::Method */
IFC_DECL_START(TypeSort_Method)
  IFC_DECL_FIELD(target, TypeIndex)
  IFC_DECL_FIELD(source, TypeIndex)
  IFC_DECL_FIELD(scope, TypeIndex)
  IFC_DECL_FIELD(eh_spec, NoexceptSpecification)
  IFC_DECL_FIELD(convention, CallingConvention)
  IFC_DECL_FIELD(traits, FunctionTypeTraits)
IFC_DECL_END(TypeSort_Method)

/* TypeSort::Array */
IFC_DECL_START(TypeSort_Array)
  IFC_DECL_FIELD(element, TypeIndex)
  IFC_DECL_FIELD(extent, ExprIndex)
IFC_DECL_END(TypeSort_Array)

/* TypeSort::Typename */
IFC_DECL_START(TypeSort_Typename)
  IFC_DECL_FIELD(path, ExprIndex)
IFC_DECL_END(TypeSort_Typename)

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

/* TypeSort::Decltype */
IFC_DECL_START(TypeSort_Decltype)
  IFC_DECL_FIELD(expr, SyntaxIndex)
IFC_DECL_END(TypeSort_Decltype)

/* TypeSort::Placeholder */
IFC_DECL_START(TypeSort_Placeholder)
  IFC_DECL_FIELD(constraint, ExprIndex)
  IFC_DECL_FIELD(basis, TypeBasis)
  IFC_DECL_FIELD(elaboration, TypeIndex)
IFC_DECL_END(TypeSort_Placeholder)

/* TypeSort::Tuple */
IFC_DECL_START(TypeSort_Tuple)
  IFC_DECL_FIELD(start, Index)
  IFC_DECL_FIELD(cardinality, Cardinality)
IFC_DECL_END(TypeSort_Tuple)

/* TypeSort::Forall */
IFC_DECL_START(TypeSort_Forall)
  IFC_DECL_FIELD(chart, ChartIndex)
  IFC_DECL_FIELD(subject, TypeIndex)
IFC_DECL_END(TypeSort_Forall)

/* TypeSort::Unaligned */
IFC_DECL_START(TypeSort_Unaligned)
  IFC_DECL_FIELD(type, TypeIndex)
IFC_DECL_END(TypeSort_Unaligned)

/* TypeSort::SyntaxTree */
IFC_DECL_START(TypeSort_SyntaxTree)
  IFC_DECL_FIELD(syntax, SyntaxIndex)
IFC_DECL_END(TypeSort_SyntaxTree)

/* StmtSort::VendorExtension */
IFC_DECL_START(StmtSort_VendorExtension)
IFC_DECL_END(StmtSort_VendorExtension)

/* StmtSort::Empty */
IFC_DECL_START(StmtSort_Empty)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_Empty)

/* StmtSort::If */
IFC_DECL_START(StmtSort_If)
  IFC_DECL_FIELD(initialization, StmtIndex)
  IFC_DECL_FIELD(condition, StmtIndex)
  IFC_DECL_FIELD(consequence, StmtIndex)
  IFC_DECL_FIELD(alternative, StmtIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_If)

/* StmtSort::For */
IFC_DECL_START(StmtSort_For)
  IFC_DECL_FIELD(initialization, StmtIndex)
  IFC_DECL_FIELD(condition, StmtIndex)
  IFC_DECL_FIELD(continuation, StmtIndex)
  IFC_DECL_FIELD(body, StmtIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_For)

/* StmtSort::Case */
IFC_DECL_START(StmtSort_Case)
  IFC_DECL_FIELD(expr, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_Case)

/* StmtSort::While */
IFC_DECL_START(StmtSort_While)
  IFC_DECL_FIELD(condition, StmtIndex)
  IFC_DECL_FIELD(body, StmtIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_While)

/* StmtSort::Block */
IFC_DECL_START(StmtSort_Block)
  IFC_DECL_FIELD(start, Index)
  IFC_DECL_FIELD(cardinality, Cardinality)
IFC_DECL_END(StmtSort_Block)

/* StmtSort::Break */
IFC_DECL_START(StmtSort_Break)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_Break)

/* StmtSort::Switch */
IFC_DECL_START(StmtSort_Switch)
  IFC_DECL_FIELD(initialization, StmtIndex)
  IFC_DECL_FIELD(condition, ExprIndex)
  IFC_DECL_FIELD(body, StmtIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_Switch)

/* StmtSort::DoWhile */
IFC_DECL_START(StmtSort_DoWhile)
  IFC_DECL_FIELD(condition, StmtIndex)
  IFC_DECL_FIELD(body, StmtIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_DoWhile)

/* StmtSort::Default */
IFC_DECL_START(StmtSort_Default)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_Default)

/* StmtSort::Continue */
IFC_DECL_START(StmtSort_Continue)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_Continue)

/* StmtSort::Expression */
IFC_DECL_START(StmtSort_Expression)
  IFC_DECL_FIELD(expr, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_Expression)

/* StmtSort::Return */
IFC_DECL_START(StmtSort_Return)
  IFC_DECL_FIELD(expr, ExprIndex)
  IFC_DECL_FIELD(function_type, TypeIndex)
  IFC_DECL_FIELD(expression_type, TypeIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_Return)

/* StmtSort::VariableDecl */
IFC_DECL_START(StmtSort_VariableDecl)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(StmtSort_VariableDecl)

/* StmtSort::Expansion */
IFC_DECL_START(StmtSort_Expansion)
  IFC_DECL_FIELD(operand, StmtIndex)
IFC_DECL_END(StmtSort_Expansion)

/* StmtSort::SyntaxTree */
IFC_DECL_START(StmtSort_SyntaxTree)
IFC_DECL_END(StmtSort_SyntaxTree)

/* ExprSort::VendorExtension */
IFC_DECL_START(ExprSort_VendorExtension)
IFC_DECL_END(ExprSort_VendorExtension)

/* ExprSort::Empty */
IFC_DECL_START(ExprSort_Empty)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
IFC_DECL_END(ExprSort_Empty)

/* ExprSort::Literal */
IFC_DECL_START(ExprSort_Literal)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(value, LitIndex)
IFC_DECL_END(ExprSort_Literal)

/* ExprSort::Lambda */
IFC_DECL_START(ExprSort_Lambda)
  IFC_DECL_FIELD(introducer, SyntaxIndex)
  IFC_DECL_FIELD(template_parameters, SyntaxIndex)
  IFC_DECL_FIELD(declarator, SyntaxIndex)
  IFC_DECL_FIELD(constraint, SyntaxIndex)
  IFC_DECL_FIELD(body, SyntaxIndex)
IFC_DECL_END(ExprSort_Lambda)

/* ExprSort::Type */
IFC_DECL_START(ExprSort_Type)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(denotation, TypeIndex)
IFC_DECL_END(ExprSort_Type)

/* ExprSort::NamedDecl */
IFC_DECL_START(ExprSort_NamedDecl)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(resolution, DeclIndex)
IFC_DECL_END(ExprSort_NamedDecl)

/* ExprSort::UnresolvedId */
IFC_DECL_START(ExprSort_UnresolvedId)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(name, NameIndex)
IFC_DECL_END(ExprSort_UnresolvedId)

/* ExprSort::TemplateId */
IFC_DECL_START(ExprSort_TemplateId)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(primary, ExprIndex)
  IFC_DECL_FIELD(arguments, ExprIndex)
IFC_DECL_END(ExprSort_TemplateId)

/* ExprSort::UnqualifiedId */
IFC_DECL_START(ExprSort_UnqualifiedId)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(name, NameIndex)
  IFC_DECL_FIELD(resolution, ExprIndex)
  IFC_DECL_FIELD(template_keyword, SourceLocation)
IFC_DECL_END(ExprSort_UnqualifiedId)

/* ExprSort::SimpleIdentifier */
IFC_DECL_START(ExprSort_SimpleIdentifier)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(name, NameIndex)
IFC_DECL_END(ExprSort_SimpleIdentifier)

/* ExprSort::Pointer */
IFC_DECL_START(ExprSort_Pointer)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(ExprSort_Pointer)

/* ExprSort::QualifiedName */
IFC_DECL_START(ExprSort_QualifiedName)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(elements, ExprIndex)
  IFC_DECL_FIELD(typename_keyword, SourceLocation)
IFC_DECL_END(ExprSort_QualifiedName)

/* ExprSort::Path */
IFC_DECL_START(ExprSort_Path)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(scope, ExprIndex)
  IFC_DECL_FIELD(member, ExprIndex)
IFC_DECL_END(ExprSort_Path)

/* ExprSort::Read */
IFC_DECL_START(ExprSort_Read)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(address, ExprIndex)
  IFC_DECL_FIELD(sort, ReadConversionSort)
IFC_DECL_END(ExprSort_Read)

/* ExprSort::Monad */
IFC_DECL_START(ExprSort_Monad)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(argument, ExprIndex)
  IFC_DECL_FIELD(op, MonadicOperator)
IFC_DECL_END(ExprSort_Monad)

/* ExprSort::Dyad */
IFC_DECL_START(ExprSort_Dyad)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  /* IFC_DECL_FIELD(arguments[2], ExprIndex)  needs to be rewritten: */
  IFC_DECL_FIELD(arguments_0, ExprIndex)
  IFC_DECL_FIELD(arguments_1, ExprIndex)
  IFC_DECL_FIELD(op, DyadicOperator)
IFC_DECL_END(ExprSort_Dyad)

/* ExprSort::Triad */
IFC_DECL_START(ExprSort_Triad)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  /* IFC_DECL_FIELD(arguments[3], ExprIndex)  needs to be rewritten: */
  IFC_DECL_FIELD(arguments_0, ExprIndex)
  IFC_DECL_FIELD(arguments_1, ExprIndex)
  IFC_DECL_FIELD(arguments_2, ExprIndex)
  IFC_DECL_FIELD(op, TriadicOperator)
IFC_DECL_END(ExprSort_Triad)

/* ExprSort::String */
IFC_DECL_START(ExprSort_String)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(string_index, StringIndex)
IFC_DECL_END(ExprSort_String)

/* ExprSort::Temporary */
IFC_DECL_START(ExprSort_Temporary)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(id, UniqueID)
IFC_DECL_END(ExprSort_Temporary)

/* ExprSort::Call */
IFC_DECL_START(ExprSort_Call)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(operation, ExprIndex)
  IFC_DECL_FIELD(arguments, ExprIndex)
IFC_DECL_END(ExprSort_Call)

/* ExprSort::MemberInitializer */
IFC_DECL_START(ExprSort_MemberInitializer)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(member, DeclIndex)
  IFC_DECL_FIELD(base, TypeIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
IFC_DECL_END(ExprSort_MemberInitializer)

/* ExprSort::MemberAccess */
IFC_DECL_START(ExprSort_MemberAccess)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(member, DeclIndex)
  IFC_DECL_FIELD(offset, ExprIndex)
  IFC_DECL_FIELD(name, TextOffset)
IFC_DECL_END(ExprSort_MemberAccess)

/* ExprSort::InheritancePath */
IFC_DECL_START(ExprSort_InheritancePath)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(path, ExprIndex)
IFC_DECL_END(ExprSort_InheritancePath)

/* ExprSort::InitializerList */
IFC_DECL_START(ExprSort_InitializerList)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(elements, ExprIndex)
IFC_DECL_END(ExprSort_InitializerList)

/* ExprSort::Cast */
IFC_DECL_START(ExprSort_Cast)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(source, ExprIndex)
  IFC_DECL_FIELD(target, Index)
  IFC_DECL_FIELD(op, DyadicOperator)
IFC_DECL_END(ExprSort_Cast)

/* ExprSort::Condition */
IFC_DECL_START(ExprSort_Condition)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(expr, ExprIndex)
IFC_DECL_END(ExprSort_Condition)

/* ExprSort::ExpressionList */
IFC_DECL_START(ExprSort_ExpressionList)
  IFC_DECL_FIELD(left, SourceLocation)
  IFC_DECL_FIELD(right, SourceLocation)
  IFC_DECL_FIELD(contents, ExprIndex)
  IFC_DECL_FIELD(delimiter, DelimiterSort)
IFC_DECL_END(ExprSort_ExpressionList)

/* ExprSort::SizeofType */
IFC_DECL_START(ExprSort_SizeofType)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(operand, SyntaxIndex)
IFC_DECL_END(ExprSort_SizeofType)

/* ExprSort::Alignof */
IFC_DECL_START(ExprSort_Alignof)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(operand, SyntaxIndex)
IFC_DECL_END(ExprSort_Alignof)

/* ExprSort::New */
IFC_DECL_START(ExprSort_New)
  IFC_DECL_FIELD(double_colon, SourceLocation)
  IFC_DECL_FIELD(new_keyword, SourceLocation)
  IFC_DECL_FIELD(allocated_type, SyntaxIndex)
  IFC_DECL_FIELD(placement, ExprIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
IFC_DECL_END(ExprSort_New)

/* ExprSort::Delete */
IFC_DECL_START(ExprSort_Delete)
  IFC_DECL_FIELD(double_colon, SourceLocation)
  IFC_DECL_FIELD(delete_keyword, SourceLocation)
  IFC_DECL_FIELD(address, ExprIndex)
IFC_DECL_END(ExprSort_Delete)

/* ExprSort::Typeid */
IFC_DECL_START(ExprSort_Typeid)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(operand, TypeIndex)
IFC_DECL_END(ExprSort_Typeid)

/* ExprSort::DestructorCall */
IFC_DECL_START(ExprSort_DestructorCall)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(name, ExprIndex)
  IFC_DECL_FIELD(decltype_specifier, SyntaxIndex)
  IFC_DECL_FIELD(cleanup, DestructorSort)
IFC_DECL_END(ExprSort_DestructorCall)

/* ExprSort::SyntaxTree */
IFC_DECL_START(ExprSort_SyntaxTree)
  IFC_DECL_FIELD(syntax, SyntaxIndex)
IFC_DECL_END(ExprSort_SyntaxTree)

/* ExprSort::FunctionString */
IFC_DECL_START(ExprSort_FunctionString)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(macro, TextOffset)
IFC_DECL_END(ExprSort_FunctionString)

/* ExprSort::CompoundString */
IFC_DECL_START(ExprSort_CompoundString)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(prefix, TextOffset)
  IFC_DECL_FIELD(string, ExprIndex)
IFC_DECL_END(ExprSort_CompoundString)

/* ExprSort::StringSequence */
IFC_DECL_START(ExprSort_StringSequence)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(strings, ExprIndex)
IFC_DECL_END(ExprSort_StringSequence)

/* ExprSort::Initializer */
IFC_DECL_START(ExprSort_Initializer)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(expr, ExprIndex)
  IFC_DECL_FIELD(sort, InitializerSort)
IFC_DECL_END(ExprSort_Initializer)

/* ExprSort::Requires */
IFC_DECL_START(ExprSort_Requires)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(parameters, SyntaxIndex)
  IFC_DECL_FIELD(body, SyntaxIndex)
IFC_DECL_END(ExprSort_Requires)

/* ExprSort::UnaryFold */
IFC_DECL_START(ExprSort_UnaryFold)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(expr, ExprIndex)
  IFC_DECL_FIELD(operation, DyadicOperator)
  IFC_DECL_FIELD(associativity, Associativity)
IFC_DECL_END(ExprSort_UnaryFold)

/* ExprSort::BinaryFold */
IFC_DECL_START(ExprSort_BinaryFold)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(left, ExprIndex)
  IFC_DECL_FIELD(right, ExprIndex)
  IFC_DECL_FIELD(operation, DyadicOperator)
  IFC_DECL_FIELD(associativity, Associativity)
IFC_DECL_END(ExprSort_BinaryFold)

/* ExprSort::HierarchyConversion */
IFC_DECL_START(ExprSort_HierarchyConversion)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(source, ExprIndex)
  IFC_DECL_FIELD(target, TypeIndex)
  IFC_DECL_FIELD(inheritance, ExprIndex)
  IFC_DECL_FIELD(override, ExprIndex)
  IFC_DECL_FIELD(op, DyadicOperator)
IFC_DECL_END(ExprSort_HierarchyConversion)

/* ExprSort::ProductTypeValue */
IFC_DECL_START(ExprSort_ProductTypeValue)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(class_decl, DeclIndex)
  IFC_DECL_FIELD(members, ExprIndex)
  IFC_DECL_FIELD(base_subobjects, ExprIndex)
IFC_DECL_END(ExprSort_ProductTypeValue)

/* ExprSort::SumTypeValue */
IFC_DECL_START(ExprSort_SumTypeValue)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(variant, DeclIndex)
  IFC_DECL_FIELD(discriminant, ActiveMember)
  IFC_DECL_FIELD(value, ExprIndex)
IFC_DECL_END(ExprSort_SumTypeValue)

/* ExprSort::SubobjectValue */
IFC_DECL_START(ExprSort_SubobjectValue)
IFC_DECL_END(ExprSort_SubobjectValue)

/* ExprSort::ArrayValue */
IFC_DECL_START(ExprSort_ArrayValue)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(elements, ExprIndex)
  IFC_DECL_FIELD(element_type, TypeIndex)
IFC_DECL_END(ExprSort_ArrayValue)

/* ExprSort::DynamicDispatch */
IFC_DECL_START(ExprSort_DynamicDispatch)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(pivot, ExprIndex)
IFC_DECL_END(ExprSort_DynamicDispatch)

/* ExprSort::VirtualFunctionConversion */
IFC_DECL_START(ExprSort_VirtualFunctionConversion)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(function, DeclIndex)
IFC_DECL_END(ExprSort_VirtualFunctionConversion)

/* ExprSort::Placeholder */
IFC_DECL_START(ExprSort_Placeholder)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
IFC_DECL_END(ExprSort_Placeholder)

/* ExprSort::Expansion */
IFC_DECL_START(ExprSort_Expansion)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(operand, ExprIndex)
IFC_DECL_END(ExprSort_Expansion)

/* ExprSort::Generic */
IFC_DECL_START(ExprSort_Generic)
IFC_DECL_END(ExprSort_Generic)

/* ExprSort::Tuple */
IFC_DECL_START(ExprSort_Tuple)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(start, Index)
  IFC_DECL_FIELD(cardinality, Cardinality)
IFC_DECL_END(ExprSort_Tuple)

/* ExprSort::Nullptr */
IFC_DECL_START(ExprSort_Nullptr)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
IFC_DECL_END(ExprSort_Nullptr)

/* ExprSort::This */
IFC_DECL_START(ExprSort_This)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
IFC_DECL_END(ExprSort_This)

/* ExprSort::TemplateReference */
IFC_DECL_START(ExprSort_TemplateReference)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(member_name, NameIndex)
  IFC_DECL_FIELD(member_locus, SourceLocation)
  IFC_DECL_FIELD(scope, TypeIndex)
  IFC_DECL_FIELD(arguments, ExprIndex)
IFC_DECL_END(ExprSort_TemplateReference)

/* ExprSort::PushState */
IFC_DECL_START(ExprSort_PushState)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(ctor_call, ExprIndex)
  IFC_DECL_FIELD(dtor_call, ExprIndex)
  IFC_DECL_FIELD(flags, EHFlags)
IFC_DECL_END(ExprSort_PushState)

/* ExprSort::TypeTraitIntrinsic */
IFC_DECL_START(ExprSort_TypeTraitIntrinsic)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(arguments, TypeIndex)
  IFC_DECL_FIELD(intrinsic, Operator)
IFC_DECL_END(ExprSort_TypeTraitIntrinsic)

/* ExprSort::DesignatedInitializer */
IFC_DECL_START(ExprSort_DesignatedInitializer)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(member, TextOffset)
  IFC_DECL_FIELD(initializer, ExprIndex)
IFC_DECL_END(ExprSort_DesignatedInitializer)

/* ExprSort::PackedTemplateArguments */
IFC_DECL_START(ExprSort_PackedTemplateArguments)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(arguments, ExprIndex)
IFC_DECL_END(ExprSort_PackedTemplateArguments)

/* ExprSort::Tokens */
IFC_DECL_START(ExprSort_Tokens)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(words, SentenceIndex)
IFC_DECL_END(ExprSort_Tokens)

/* ExprSort::AssignInitializer */
IFC_DECL_START(ExprSort_AssignInitializer)
  IFC_DECL_FIELD(equal, SourceLocation)
  IFC_DECL_FIELD(initializer, ExprIndex)
IFC_DECL_END(ExprSort_AssignInitializer)

/* String::Literal */
IFC_DECL_START(String_Literal)
  IFC_DECL_FIELD(start, TextOffset)
  IFC_DECL_FIELD(length, Cardinality)
  IFC_DECL_FIELD(suffix, TextOffset)
IFC_DECL_END(String_Literal)

/* NameSort::Identifier */
/* No partition - value is an index into the string table. */

/* NameSort::Operator */
IFC_DECL_START(NameSort_Operator)
  IFC_DECL_FIELD(encoded, TextOffset)
  IFC_DECL_FIELD(op, Operator)
IFC_DECL_END(NameSort_Operator)

/* NameSort::Conversion */
IFC_DECL_START(NameSort_Conversion)
  IFC_DECL_FIELD(target, TypeIndex)
  IFC_DECL_FIELD(encoded, TextOffset)
IFC_DECL_END(NameSort_Conversion)

/* NameSort::Literal */
IFC_DECL_START(NameSort_Literal)
  IFC_DECL_FIELD(encoded, TextOffset)
IFC_DECL_END(NameSort_Literal)

/* NameSort::Template */
IFC_DECL_START(NameSort_Template)
  IFC_DECL_FIELD(name, NameIndex)
IFC_DECL_END(NameSort_Template)

/* NameSort::Specialization */
IFC_DECL_START(NameSort_Specialization)
  IFC_DECL_FIELD(primary, NameIndex)
  IFC_DECL_FIELD(arguments, ExprIndex)
IFC_DECL_END(NameSort_Specialization)

/* NameSort::SourceFile */
IFC_DECL_START(NameSort_SourceFile)
  IFC_DECL_FIELD(path, TextOffset)
  IFC_DECL_FIELD(guard, TextOffset)
IFC_DECL_END(NameSort_SourceFile)

/* NameSort::Guide */
IFC_DECL_START(NameSort_Guide)
  IFC_DECL_FIELD(primary_template, DeclIndex)
IFC_DECL_END(NameSort_Guide)

/* ChartSort::None */
IFC_DECL_START(ChartSort_None)
IFC_DECL_END(ChartSort_None)

/* ChartSort::Unilevel */
IFC_DECL_START(ChartSort_Unilevel)
  IFC_DECL_FIELD(start, Index)
  IFC_DECL_FIELD(cardinality, Cardinality)
  IFC_DECL_FIELD(constraint, ExprIndex)
IFC_DECL_END(ChartSort_Unilevel)

/* ChartSort::Multilevel */
IFC_DECL_START(ChartSort_Multilevel)
  IFC_DECL_FIELD(start, Index)
  IFC_DECL_FIELD(cardinality, Cardinality)
IFC_DECL_END(ChartSort_Multilevel)

/* SyntaxSort::VendorExtension */
IFC_DECL_START(SyntaxSort_VendorExtension)
IFC_DECL_END(SyntaxSort_VendorExtension)

/* SyntaxSort::SimpleTypeSpecifier */
IFC_DECL_START(SyntaxSort_SimpleTypeSpecifier)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(expr, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(SyntaxSort_SimpleTypeSpecifier)

/* SyntaxSort::DecltypeSpecifier */
IFC_DECL_START(SyntaxSort_DecltypeSpecifier)
  IFC_DECL_FIELD(expr, ExprIndex)
  IFC_DECL_FIELD(decltype_keyword, SourceLocation)
  IFC_DECL_FIELD(left_paren, SourceLocation)
  IFC_DECL_FIELD(right_paren, SourceLocation)
IFC_DECL_END(SyntaxSort_DecltypeSpecifier)

/* SyntaxSort::PlaceholderTypeSpecifier */
IFC_DECL_START(SyntaxSort_PlaceholderTypeSpecifier)
  /* The IFC spec doesn't define PlaceholderType anywhere. */
  /* IFC_DECL_FIELD(type, PlaceholderType) */
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(keyword, SourceLocation)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(SyntaxSort_PlaceholderTypeSpecifier)

/* SyntaxSort::TypeSpecifierSeq */
IFC_DECL_START(SyntaxSort_TypeSpecifierSeq)
  IFC_DECL_FIELD(type_name, SyntaxIndex)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(qualifiers, Qualifiers)
  IFC_DECL_FIELD(unshashed, bool)
IFC_DECL_END(SyntaxSort_TypeSpecifierSeq)

/* SyntaxSort::DeclSpecifierSeq */
IFC_DECL_START(SyntaxSort_DeclSpecifierSeq)
  IFC_DECL_FIELD(type, TypeIndex)
  IFC_DECL_FIELD(type_name, SyntaxIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  /* The IFC spec does not define StorageClass anywhere. */
  /* IFC_DECL_FIELD(storage_class, StorageClass) */
  IFC_DECL_FIELD(storage_class, u32)
  IFC_DECL_FIELD(declspec, SentenceIndex)
  IFC_DECL_FIELD(explicit_kw, SyntaxIndex)
  IFC_DECL_FIELD(qualifiers, Qualifiers)
IFC_DECL_END(SyntaxSort_DeclSpecifierSeq)

/* SyntaxSort::VirtualSpecifierSeq */
IFC_DECL_START(SyntaxSort_VirtualSpecifierSeq)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(final_kw, SourceLocation)
  IFC_DECL_FIELD(override_kw, SourceLocation)
  IFC_DECL_FIELD(pure, bool)
IFC_DECL_END(SyntaxSort_VirtualSpecifierSeq)

/* SyntaxSort::NoexceptSpecification */
IFC_DECL_START(SyntaxSort_NoexceptSpecification)
  IFC_DECL_FIELD(expr, SyntaxIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(left_paren, SourceLocation)
  IFC_DECL_FIELD(right_paren, SourceLocation)
IFC_DECL_END(SyntaxSort_NoexceptSpecification)

/* SyntaxSort::ExplicitSpecifier */
IFC_DECL_START(SyntaxSort_ExplicitSpecifier)
  IFC_DECL_FIELD(condition, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(left_paren, SourceLocation)
  IFC_DECL_FIELD(right_paren, SourceLocation)
IFC_DECL_END(SyntaxSort_ExplicitSpecifier)

/* SyntaxSort::EnumSpecifier */
IFC_DECL_START(SyntaxSort_EnumSpecifier)
  IFC_DECL_FIELD(name, ExprIndex)
  /* The IFC spec does not define KeywordSyntax anywhere. */
  /* IFC_DECL_FIELD(class_key, KeywordSyntax) */
  IFC_DECL_FIELD(class_key, SyntaxIndex)
  IFC_DECL_FIELD(enumerators, SyntaxIndex)
  IFC_DECL_FIELD(base, SyntaxIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(colon, SourceLocation)
  IFC_DECL_FIELD(left_brace, SourceLocation)
  IFC_DECL_FIELD(right_brace, SourceLocation)
IFC_DECL_END(SyntaxSort_EnumSpecifier)

/* SyntaxSort::EnumeratorDefinition */
IFC_DECL_START(SyntaxSort_EnumeratorDefinition)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(equal, SourceLocation)
  IFC_DECL_FIELD(comma, SourceLocation)
IFC_DECL_END(SyntaxSort_EnumeratorDefinition)

/* SyntaxSort::ClassSpecifier */
IFC_DECL_START(SyntaxSort_ClassSpecifier)
  IFC_DECL_FIELD(name, ExprIndex)
  /* The IFC spec does not define KeywordSyntax anywhere. */
  /* IFC_DECL_FIELD(class_key, KeywordSyntax) */
  IFC_DECL_FIELD(class_key, SyntaxIndex)
  IFC_DECL_FIELD(bases, SyntaxIndex)
  IFC_DECL_FIELD(members, SyntaxIndex)
  IFC_DECL_FIELD(left_paren, SyntaxIndex)
  IFC_DECL_FIELD(right_paren, SyntaxIndex)
IFC_DECL_END(SyntaxSort_ClassSpecifier)

/* SyntaxSort::MemberSpecification */
IFC_DECL_START(SyntaxSort_MemberSpecification)
  IFC_DECL_FIELD(member_declarations, SyntaxIndex)
IFC_DECL_END(SyntaxSort_MemberSpecification)

/* SyntaxSort::MemberDeclaration */
IFC_DECL_START(SyntaxSort_MemberDeclaration)
  IFC_DECL_FIELD(decl_specifiers, SyntaxIndex)
  IFC_DECL_FIELD(declarations, SyntaxIndex)
  IFC_DECL_FIELD(semicolon, SourceLocation)
IFC_DECL_END(SyntaxSort_MemberDeclaration)

/* SyntaxSort::MemberDeclarator */
IFC_DECL_START(SyntaxSort_MemberDeclarator)
  IFC_DECL_FIELD(declarator, SyntaxIndex)
  IFC_DECL_FIELD(constraint, SyntaxIndex)
  IFC_DECL_FIELD(bitwidth, ExprIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(colon, SourceLocation)
  IFC_DECL_FIELD(comma, SourceLocation)
IFC_DECL_END(SyntaxSort_MemberDeclarator)

/* SyntaxSort::AccessSpecifier */
IFC_DECL_START(SyntaxSort_AccessSpecifier)
  /* The IFC spec does not define KeywordSyntax anywhere. */
  /* IFC_DECL_FIELD(access, KeywordSyntax) */
  IFC_DECL_FIELD(access, SyntaxIndex)
  IFC_DECL_FIELD(colon, SourceLocation)
IFC_DECL_END(SyntaxSort_AccessSpecifier)

/* SyntaxSort::BaseSpecifierList */
IFC_DECL_START(SyntaxSort_BaseSpecifierList)
  IFC_DECL_FIELD(base_specifiers, SyntaxIndex)
  IFC_DECL_FIELD(colon, SourceLocation)
IFC_DECL_END(SyntaxSort_BaseSpecifierList)

/* SyntaxSort::BaseSpecifier */
IFC_DECL_START(SyntaxSort_BaseSpecifier)
  IFC_DECL_FIELD(designator, ExprIndex)
  /* The IFC spec does not define KeywordSyntax anywhere. */
  /* IFC_DECL_FIELD(access, KeywordSyntax) */
  IFC_DECL_FIELD(access, SyntaxIndex)
  IFC_DECL_FIELD(virtual_kw, SourceLocation)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(virtual_kw2, SourceLocation)
  IFC_DECL_FIELD(comma, SourceLocation)
IFC_DECL_END(SyntaxSort_BaseSpecifier)

/* SyntaxSort::TypeId */
IFC_DECL_START(SyntaxSort_TypeId)
  IFC_DECL_FIELD(type_specifier, SyntaxIndex)
  IFC_DECL_FIELD(abstract_declarator, SyntaxIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(SyntaxSort_TypeId)

/* SyntaxSort::TrailingReturnType */
IFC_DECL_START(SyntaxSort_TrailingReturnType)
  IFC_DECL_FIELD(target, SyntaxIndex)
  IFC_DECL_FIELD(arrow, SourceLocation)
IFC_DECL_END(SyntaxSort_TrailingReturnType)

/* SyntaxSort::Declarator */
IFC_DECL_START(SyntaxSort_Declarator)
  IFC_DECL_FIELD(pointer, SyntaxIndex)
  IFC_DECL_FIELD(parenthesized, SyntaxIndex)
  IFC_DECL_FIELD(array_or_function, SyntaxIndex)
  IFC_DECL_FIELD(trailing_target, SyntaxIndex)
  IFC_DECL_FIELD(virtual_specifiers, SyntaxIndex)
  IFC_DECL_FIELD(name, ExprIndex)
  IFC_DECL_FIELD(ellipsis, SourceLocation)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(qualifiers, Qualifiers)
  IFC_DECL_FIELD(convention, CallingConvention)
  IFC_DECL_FIELD(callable, bool)
IFC_DECL_END(SyntaxSort_Declarator)

/* SyntaxSort::PointerDeclarator */
IFC_DECL_START(SyntaxSort_PointerDeclarator)
  IFC_DECL_FIELD(whole, SyntaxIndex)
  IFC_DECL_FIELD(next, SyntaxIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  /* The IFC spec does not define PointerDeclaratorSort anywhere. */
  /* IFC_DECL_FIELD(sort, PointerDeclaratorSort) */
  IFC_DECL_FIELD(sort, u8)
  IFC_DECL_FIELD(qualifiers, Qualifiers)
  IFC_DECL_FIELD(convention, CallingConvention)
  IFC_DECL_FIELD(callable, bool)
IFC_DECL_END(SyntaxSort_PointerDeclarator)

/* SyntaxSort::ArrayDeclarator */
IFC_DECL_START(SyntaxSort_ArrayDeclarator)
  IFC_DECL_FIELD(bound, ExprIndex)
  IFC_DECL_FIELD(left_bracket, SourceLocation)
  IFC_DECL_FIELD(right_bracket, SourceLocation)
IFC_DECL_END(SyntaxSort_ArrayDeclarator)

/* SyntaxSort::FunctionDeclarator */
IFC_DECL_START(SyntaxSort_FunctionDeclarator)
  IFC_DECL_FIELD(parameters, SyntaxIndex)
  IFC_DECL_FIELD(eh_spec, SyntaxIndex)
  IFC_DECL_FIELD(left_paren, SourceLocation)
  IFC_DECL_FIELD(right_paren, SourceLocation)
  /* IFC files have the size of this structure as 44 bytes, but there are only
     24 bytes here.  Add 20 bytes of padding to get it to match. */
  IFC_DECL_FIELD(__pad1__, u32)
  IFC_DECL_FIELD(__pad2__, u32)
  IFC_DECL_FIELD(__pad3__, u32)
  IFC_DECL_FIELD(__pad4__, u32)
  IFC_DECL_FIELD(__pad5__, u32)
IFC_DECL_END(SyntaxSort_FunctionDeclarator)

/* SyntaxSort::ArrayOrFunctionDeclarator */
IFC_DECL_START(SyntaxSort_ArrayOrFunctionDeclarator)
  IFC_DECL_FIELD(declarator, SyntaxIndex)
  IFC_DECL_FIELD(next, SyntaxIndex)
IFC_DECL_END(SyntaxSort_ArrayOrFunctionDeclarator)

/* SyntaxSort::ParameterDeclarator */
IFC_DECL_START(SyntaxSort_ParameterDeclarator)
  IFC_DECL_FIELD(decl_specifiers, SyntaxIndex)
  IFC_DECL_FIELD(declarator, SyntaxIndex)
  IFC_DECL_FIELD(default_kw, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(sort, ParameterSort)
IFC_DECL_END(SyntaxSort_ParameterDeclarator)

/* SyntaxSort::InitDeclarator */
IFC_DECL_START(SyntaxSort_InitDeclarator)
  IFC_DECL_FIELD(declarator, SyntaxIndex)
  IFC_DECL_FIELD(constraint, SyntaxIndex)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(comma, SourceLocation)
IFC_DECL_END(SyntaxSort_InitDeclarator)

/* SyntaxSort::NewDeclarator */
IFC_DECL_START(SyntaxSort_NewDeclarator)
  IFC_DECL_FIELD(declarator, SyntaxIndex)
IFC_DECL_END(SyntaxSort_NewDeclarator)

/* SyntaxSort::SimpleDeclaration */
IFC_DECL_START(SyntaxSort_SimpleDeclaration)
  IFC_DECL_FIELD(decl_specifiers, SyntaxIndex)
  IFC_DECL_FIELD(declarators, SyntaxIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(semicolon, SourceLocation)
IFC_DECL_END(SyntaxSort_SimpleDeclaration)

/* SyntaxSort::ExceptionDeclaration */
IFC_DECL_START(SyntaxSort_ExceptionDeclaration)
  IFC_DECL_FIELD(type_specifiers, SyntaxIndex)
  IFC_DECL_FIELD(declarator, SyntaxIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(ellipsis, SourceLocation)
IFC_DECL_END(SyntaxSort_ExceptionDeclaration)

/* SyntaxSort::ConditionDeclaration */
IFC_DECL_START(SyntaxSort_ConditionDeclaration)
  IFC_DECL_FIELD(decl_specifier, SyntaxIndex)
  IFC_DECL_FIELD(initializaerion, SyntaxIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(SyntaxSort_ConditionDeclaration)

/* SyntaxSort::StaticAssertDeclaration */
IFC_DECL_START(SyntaxSort_StaticAssertDeclaration)
  IFC_DECL_FIELD(condition, ExprIndex)
  IFC_DECL_FIELD(message, ExprIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(left_paren, SourceLocation)
  IFC_DECL_FIELD(right_paren, SourceLocation)
  IFC_DECL_FIELD(semicolon, SourceLocation)
  IFC_DECL_FIELD(comma, SourceLocation)
IFC_DECL_END(SyntaxSort_StaticAssertDeclaration)

/* SyntaxSort::AliasDeclaration */
IFC_DECL_START(SyntaxSort_AliasDeclaration)
  IFC_DECL_FIELD(name, ExprIndex)
  IFC_DECL_FIELD(aliasee, SyntaxIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(equal, SourceLocation)
  IFC_DECL_FIELD(semicolon, SourceLocation)
IFC_DECL_END(SyntaxSort_AliasDeclaration)

/* SyntaxSort::ConceptDefinition */
IFC_DECL_START(SyntaxSort_ConceptDefinition)
  IFC_DECL_FIELD(parameters, SyntaxIndex)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(initializer, ExprIndex)
  IFC_DECL_FIELD(concept_keyword, SourceLocation)
  IFC_DECL_FIELD(equal, SourceLocation)
  IFC_DECL_FIELD(semicolon, SourceLocation)
IFC_DECL_END(SyntaxSort_ConceptDefinition)

/* SyntaxSort::CompoundStatement */
IFC_DECL_START(SyntaxSort_CompoundStatement)
IFC_DECL_END(SyntaxSort_CompoundStatement)

/* SyntaxSort::ReturnStatement */
IFC_DECL_START(SyntaxSort_ReturnStatement)
IFC_DECL_END(SyntaxSort_ReturnStatement)

/* SyntaxSort::IfStatement */
IFC_DECL_START(SyntaxSort_IfStatement)
IFC_DECL_END(SyntaxSort_IfStatement)

/* SyntaxSort::WhileStatement */
IFC_DECL_START(SyntaxSort_WhileStatement)
IFC_DECL_END(SyntaxSort_WhileStatement)

/* SyntaxSort::DoWhileStatement */
IFC_DECL_START(SyntaxSort_DoWhileStatement)
IFC_DECL_END(SyntaxSort_DoWhileStatement)

/* SyntaxSort::ForStatement */
IFC_DECL_START(SyntaxSort_ForStatement)
IFC_DECL_END(SyntaxSort_ForStatement)

/* SyntaxSort::InitStatement */
IFC_DECL_START(SyntaxSort_InitStatement)
IFC_DECL_END(SyntaxSort_InitStatement)

/* SyntaxSort::RangeBasedForStatement */
IFC_DECL_START(SyntaxSort_RangeBasedForStatement)
IFC_DECL_END(SyntaxSort_RangeBasedForStatement)

/* SyntaxSort::ForRangeDeclaration */
IFC_DECL_START(SyntaxSort_ForRangeDeclaration)
IFC_DECL_END(SyntaxSort_ForRangeDeclaration)

/* SyntaxSort::LabeledStatement */
IFC_DECL_START(SyntaxSort_LabeledStatement)
IFC_DECL_END(SyntaxSort_LabeledStatement)

/* SyntaxSort::BreakStatement */
IFC_DECL_START(SyntaxSort_BreakStatement)
IFC_DECL_END(SyntaxSort_BreakStatement)

/* SyntaxSort::ContinueStatement */
IFC_DECL_START(SyntaxSort_ContinueStatement)
IFC_DECL_END(SyntaxSort_ContinueStatement)

/* SyntaxSort::SwitchStatement */
IFC_DECL_START(SyntaxSort_SwitchStatement)
IFC_DECL_END(SyntaxSort_SwitchStatement)

/* SyntaxSort::GotoStatement */
IFC_DECL_START(SyntaxSort_GotoStatement)
IFC_DECL_END(SyntaxSort_GotoStatement)

/* SyntaxSort::DeclarationStatement */
IFC_DECL_START(SyntaxSort_DeclarationStatement)
IFC_DECL_END(SyntaxSort_DeclarationStatement)

/* SyntaxSort::ExpressionStatement */
IFC_DECL_START(SyntaxSort_ExpressionStatement)
IFC_DECL_END(SyntaxSort_ExpressionStatement)

/* SyntaxSort::TryBlock */
IFC_DECL_START(SyntaxSort_TryBlock)
IFC_DECL_END(SyntaxSort_TryBlock)

/* SyntaxSort::Handler */
IFC_DECL_START(SyntaxSort_Handler)
IFC_DECL_END(SyntaxSort_Handler)

/* SyntaxSort::HandlerSeq */
IFC_DECL_START(SyntaxSort_HandlerSeq)
IFC_DECL_END(SyntaxSort_HandlerSeq)

/* SyntaxSort::FunctionTryBlock */
IFC_DECL_START(SyntaxSort_FunctionTryBlock)
IFC_DECL_END(SyntaxSort_FunctionTryBlock)

/* SyntaxSort::TypeIdListElement */
IFC_DECL_START(SyntaxSort_TypeIdListElement)
IFC_DECL_END(SyntaxSort_TypeIdListElement)

/* SyntaxSort::DynamicExceptionSpec */
IFC_DECL_START(SyntaxSort_DynamicExceptionSpec)
IFC_DECL_END(SyntaxSort_DynamicExceptionSpec)

/* SyntaxSort::StatementSeq */
IFC_DECL_START(SyntaxSort_StatementSeq)
IFC_DECL_END(SyntaxSort_StatementSeq)

/* SyntaxSort::FunctionBody */
IFC_DECL_START(SyntaxSort_FunctionBody)
IFC_DECL_END(SyntaxSort_FunctionBody)

/* SyntaxSort::Expression */
IFC_DECL_START(SyntaxSort_Expression)
IFC_DECL_END(SyntaxSort_Expression)

/* SyntaxSort::FunctionDefinition */
IFC_DECL_START(SyntaxSort_FunctionDefinition)
IFC_DECL_END(SyntaxSort_FunctionDefinition)

/* SyntaxSort::MemberFunctionDeclaration */
IFC_DECL_START(SyntaxSort_MemberFunctionDeclaration)
IFC_DECL_END(SyntaxSort_MemberFunctionDeclaration)

/* SyntaxSort::TemplateDeclaration */
IFC_DECL_START(SyntaxSort_TemplateDeclaration)
IFC_DECL_END(SyntaxSort_TemplateDeclaration)

/* SyntaxSort::RequiresClause */
IFC_DECL_START(SyntaxSort_RequiresClause)
IFC_DECL_END(SyntaxSort_RequiresClause)

/* SyntaxSort::SimpleRequirement */
IFC_DECL_START(SyntaxSort_SimpleRequirement)
IFC_DECL_END(SyntaxSort_SimpleRequirement)

/* SyntaxSort::TypeRequirement */
IFC_DECL_START(SyntaxSort_TypeRequirement)
IFC_DECL_END(SyntaxSort_TypeRequirement)

/* SyntaxSort::CompoundRequirement */
IFC_DECL_START(SyntaxSort_CompoundRequirement)
IFC_DECL_END(SyntaxSort_CompoundRequirement)

/* SyntaxSort::NestedRequirement */
IFC_DECL_START(SyntaxSort_NestedRequirement)
IFC_DECL_END(SyntaxSort_NestedRequirement)

/* SyntaxSort::RequirementBody */
IFC_DECL_START(SyntaxSort_RequirementBody)
IFC_DECL_END(SyntaxSort_RequirementBody)

/* SyntaxSort::TypeTemplateParameter */
IFC_DECL_START(SyntaxSort_TypeTemplateParameter)
IFC_DECL_END(SyntaxSort_TypeTemplateParameter)

/* SyntaxSort::TemplateTemplateParameter */
IFC_DECL_START(SyntaxSort_TemplateTemplateParameter)
IFC_DECL_END(SyntaxSort_TemplateTemplateParameter)

/* SyntaxSort::TypeTemplateArgument */
IFC_DECL_START(SyntaxSort_TypeTemplateArgument)
IFC_DECL_END(SyntaxSort_TypeTemplateArgument)

/* SyntaxSort::NonTypeTemplateArgument */
IFC_DECL_START(SyntaxSort_NonTypeTemplateArgument)
IFC_DECL_END(SyntaxSort_NonTypeTemplateArgument)

/* SyntaxSort::TemplateParameterList */
IFC_DECL_START(SyntaxSort_TemplateParameterList)
IFC_DECL_END(SyntaxSort_TemplateParameterList)

/* SyntaxSort::TemplateArgumentList */
IFC_DECL_START(SyntaxSort_TemplateArgumentList)
IFC_DECL_END(SyntaxSort_TemplateArgumentList)

/* SyntaxSort::TemplateId */
IFC_DECL_START(SyntaxSort_TemplateId)
IFC_DECL_END(SyntaxSort_TemplateId)

/* SyntaxSort::MemInitializer */
IFC_DECL_START(SyntaxSort_MemInitializer)
IFC_DECL_END(SyntaxSort_MemInitializer)

/* SyntaxSort::CtorInitializer */
IFC_DECL_START(SyntaxSort_CtorInitializer)
IFC_DECL_END(SyntaxSort_CtorInitializer)

/* SyntaxSort::LambdaIntroducer */
IFC_DECL_START(SyntaxSort_LambdaIntroducer)
IFC_DECL_END(SyntaxSort_LambdaIntroducer)

/* SyntaxSort::LambdaDeclarator */
IFC_DECL_START(SyntaxSort_LambdaDeclarator)
IFC_DECL_END(SyntaxSort_LambdaDeclarator)

/* SyntaxSort::CaptureDefault */
IFC_DECL_START(SyntaxSort_CaptureDefault)
IFC_DECL_END(SyntaxSort_CaptureDefault)

/* SyntaxSort::SimpleCapture */
IFC_DECL_START(SyntaxSort_SimpleCapture)
IFC_DECL_END(SyntaxSort_SimpleCapture)

/* SyntaxSort::InitCapture */
IFC_DECL_START(SyntaxSort_InitCapture)
IFC_DECL_END(SyntaxSort_InitCapture)

/* SyntaxSort::ThisCapture */
IFC_DECL_START(SyntaxSort_ThisCapture)
IFC_DECL_END(SyntaxSort_ThisCapture)

/* SyntaxSort::AttributedStatement */
IFC_DECL_START(SyntaxSort_AttributedStatement)
IFC_DECL_END(SyntaxSort_AttributedStatement)

/* SyntaxSort::AttributedDeclaration */
IFC_DECL_START(SyntaxSort_AttributedDeclaration)
IFC_DECL_END(SyntaxSort_AttributedDeclaration)

/* SyntaxSort::AttributeSpecifierSeq */
IFC_DECL_START(SyntaxSort_AttributeSpecifierSeq)
IFC_DECL_END(SyntaxSort_AttributeSpecifierSeq)

/* SyntaxSort::AttributeSpecifier */
IFC_DECL_START(SyntaxSort_AttributeSpecifier)
IFC_DECL_END(SyntaxSort_AttributeSpecifier)

/* SyntaxSort::AttributeUsingPrefix */
IFC_DECL_START(SyntaxSort_AttributeUsingPrefix)
IFC_DECL_END(SyntaxSort_AttributeUsingPrefix)

/* SyntaxSort::Attribute */
IFC_DECL_START(SyntaxSort_Attribute)
IFC_DECL_END(SyntaxSort_Attribute)

/* SyntaxSort::AttributeArgumentClause */
IFC_DECL_START(SyntaxSort_AttributeArgumentClause)
IFC_DECL_END(SyntaxSort_AttributeArgumentClause)

/* SyntaxSort::Alignas */
IFC_DECL_START(SyntaxSort_Alignas)
IFC_DECL_END(SyntaxSort_Alignas)

/* SyntaxSort::UsingDeclaration */
IFC_DECL_START(SyntaxSort_UsingDeclaration)
IFC_DECL_END(SyntaxSort_UsingDeclaration)

/* SyntaxSort::UsingDeclarator */
IFC_DECL_START(SyntaxSort_UsingDeclarator)
IFC_DECL_END(SyntaxSort_UsingDeclarator)

/* SyntaxSort::UsingDirective */
IFC_DECL_START(SyntaxSort_UsingDirective)
IFC_DECL_END(SyntaxSort_UsingDirective)

/* SyntaxSort::ArrayIndex */
IFC_DECL_START(SyntaxSort_ArrayIndex)
IFC_DECL_END(SyntaxSort_ArrayIndex)

/* SyntaxSort::SEHTry */
IFC_DECL_START(SyntaxSort_SEHTry)
IFC_DECL_END(SyntaxSort_SEHTry)

/* SyntaxSort::SEHExcept */
IFC_DECL_START(SyntaxSort_SEHExcept)
IFC_DECL_END(SyntaxSort_SEHExcept)

/* SyntaxSort::SEHFinally */
IFC_DECL_START(SyntaxSort_SEHFinally)
IFC_DECL_END(SyntaxSort_SEHFinally)

/* SyntaxSort::SEHLeave */
IFC_DECL_START(SyntaxSort_SEHLeave)
IFC_DECL_END(SyntaxSort_SEHLeave)

/* SyntaxSort::TypeTraitIntrinsic */
IFC_DECL_START(SyntaxSort_TypeTraitIntrinsic)
IFC_DECL_END(SyntaxSort_TypeTraitIntrinsic)

/* SyntaxSort::Tuple */
IFC_DECL_START(SyntaxSort_Tuple)
IFC_DECL_END(SyntaxSort_Tuple)

/* SyntaxSort::AsmStatement */
IFC_DECL_START(SyntaxSort_AsmStatement)
IFC_DECL_END(SyntaxSort_AsmStatement)

/* SyntaxSort::NamespaceAliasDefinition */
IFC_DECL_START(SyntaxSort_NamespaceAliasDefinition)
IFC_DECL_END(SyntaxSort_NamespaceAliasDefinition)

/* SyntaxSort::Super */
IFC_DECL_START(SyntaxSort_Super)
IFC_DECL_END(SyntaxSort_Super)

/* SyntaxSort::UnaryFoldExpression */
IFC_DECL_START(SyntaxSort_UnaryFoldExpression)
IFC_DECL_END(SyntaxSort_UnaryFoldExpression)

/* SyntaxSort::BinaryFoldExpression */
IFC_DECL_START(SyntaxSort_BinaryFoldExpression)
IFC_DECL_END(SyntaxSort_BinaryFoldExpression)

/* SyntaxSort::EmptyStatement */
IFC_DECL_START(SyntaxSort_EmptyStatement)
IFC_DECL_END(SyntaxSort_EmptyStatement)

/* SyntaxSort::StructuredBindingDeclaration */
IFC_DECL_START(SyntaxSort_StructuredBindingDeclaration)
IFC_DECL_END(SyntaxSort_StructuredBindingDeclaration)

/* SyntaxSort::StructuredBindingIdentifier */
IFC_DECL_START(SyntaxSort_StructuredBindingIdentifier)
IFC_DECL_END(SyntaxSort_StructuredBindingIdentifier)

/* SyntaxSort::UsingEnumDeclaration */
IFC_DECL_START(SyntaxSort_UsingEnumDeclaration)
IFC_DECL_END(SyntaxSort_UsingEnumDeclaration)

/* MacroSort::ObjectLike */
IFC_DECL_START(MacroSort_ObjectLike)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(body, FormIndex)
IFC_DECL_END(MacroSort_ObjectLike)

/* MacroSort::FunctionLike */
IFC_DECL_START(MacroSort_FunctionLike)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(name, TextOffset)
  IFC_DECL_FIELD(parameters, FormIndex)
  IFC_DECL_FIELD(body, FormIndex)
  IFC_DECL_FIELD(arity_variadic, u32)
IFC_DECL_END(MacroSort_FunctionLike)

/* FormSort::Identifier */
IFC_DECL_START(FormSort_Identifier)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(spelling, TextOffset)
IFC_DECL_END(FormSort_Identifier)

/* FormSort::Number */
IFC_DECL_START(FormSort_Number)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(spelling, TextOffset)
IFC_DECL_END(FormSort_Number)

/* FormSort::Character */
IFC_DECL_START(FormSort_Character)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(spelling, TextOffset)
IFC_DECL_END(FormSort_Character)

/* FormSort::String */
IFC_DECL_START(FormSort_String)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(spelling, TextOffset)
IFC_DECL_END(FormSort_String)

/* FormSort::Operator */
IFC_DECL_START(FormSort_Operator)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(spelling, TextOffset)
  IFC_DECL_FIELD(op, FormOperator)
IFC_DECL_END(FormSort_Operator)

/* FormSort::Keyword */
IFC_DECL_START(FormSort_Keyword)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(spelling, TextOffset)
IFC_DECL_END(FormSort_Keyword)

/* FormSort::Whitespace */
IFC_DECL_START(FormSort_Whitespace)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(FormSort_Whitespace)

/* FormSort::Parameter */
IFC_DECL_START(FormSort_Parameter)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(spelling, TextOffset)
IFC_DECL_END(FormSort_Parameter)

/* FormSort::Stringize */
IFC_DECL_START(FormSort_Stringize)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(operand, FormIndex)
IFC_DECL_END(FormSort_Stringize)

/* FormSort::Catenate */
IFC_DECL_START(FormSort_Catenate)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(first, FormIndex)
  IFC_DECL_FIELD(second, FormIndex)
IFC_DECL_END(FormSort_Catenate)

/* FormSort::Pragma */
IFC_DECL_START(FormSort_Pragma)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(operand, FormIndex)
IFC_DECL_END(FormSort_Pragma)

/* FormSort::Header */
IFC_DECL_START(FormSort_Header)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(spelling, TextOffset)
IFC_DECL_END(FormSort_Header)

/* FormSort::Parenthesized */
IFC_DECL_START(FormSort_Parenthesized)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(operand, FormIndex)
IFC_DECL_END(FormSort_Parenthesized)

/* FormSort::Tuple */
IFC_DECL_START(FormSort_Tuple)
  IFC_DECL_FIELD(start, Index)
  IFC_DECL_FIELD(cardinality, Cardinality)
IFC_DECL_END(FormSort_Tuple)

/* FormSort::Junk */
IFC_DECL_START(FormSort_Junk)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(spelling, TextOffset)
IFC_DECL_END(FormSort_Junk)

/* Source::Line */
IFC_DECL_START(Source_Line)
  IFC_DECL_FIELD(file, NameIndex)
  IFC_DECL_FIELD(line, LineNumber)
IFC_DECL_END(Source_Line)

/* Trait::DeductionGuide */
IFC_DECL_START(Trait_DeductionGuide)
IFC_DECL_END(Trait_DeductionGuide)

/* Trait::Deprecated */
IFC_DECL_START(Trait_Deprecated)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(trait, u32)
IFC_DECL_END(Trait_Deprecated)

/* Trait::Specialization */
IFC_DECL_START(Trait_Specialization)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(trait, u32)
  IFC_DECL_FIELD(__padding__, u32)
IFC_DECL_END(Trait_Specialization)

/* Trait::Friend */
IFC_DECL_START(Trait_Friend)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(trait, u32)
  IFC_DECL_FIELD(__padding__, u32)
IFC_DECL_END(Trait_Friend)

/* Trait::FunctionDefinition */
IFC_DECL_START(Trait_FunctionDefinition)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(parameters, ChartIndex)
  IFC_DECL_FIELD(initializers, ExprIndex)
  IFC_DECL_FIELD(body, StmtIndex)
IFC_DECL_END(Trait_FunctionDefinition)

/* Trait::FunctionTemplate */
IFC_DECL_START(Trait_FunctionTemplate)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(trait, u32)
IFC_DECL_END(Trait_FunctionTemplate)

/* Trait::ClassTemplate */
IFC_DECL_START(Trait_ClassTemplate)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(trait, u32)
IFC_DECL_END(Trait_ClassTemplate)

/* Trait::AliasTemplate */
IFC_DECL_START(Trait_AliasTemplate)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(trait, u32)
IFC_DECL_END(Trait_AliasTemplate)

/* Trait::VariableTemplate */
IFC_DECL_START(Trait_VariableTemplate)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(trait, u32)
IFC_DECL_END(Trait_VariableTemplate)

/* Trait::MsvcVendorTrait */
IFC_DECL_START(Trait_MsvcVendorTrait)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(trait, MsvcTraits)
IFC_DECL_END(Trait_MsvcVendorTrait)

/* Trait::MsvcUuid */
IFC_DECL_START(Trait_MsvcUuid)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(uuid, u16)
IFC_DECL_END(Trait_MsvcUuid)

/* Trait::MsvcFuncParams */
IFC_DECL_START(Trait_MsvcFuncParams)
  IFC_DECL_FIELD(decl, DeclIndex)
  IFC_DECL_FIELD(params, ChartIndex)
IFC_DECL_END(Trait_MsvcFuncParams)

/* Sentences: Internal token stream from MSVC. */
IFC_DECL_START(Sentence)
  IFC_DECL_FIELD(start, WordIndex)
  IFC_DECL_FIELD(cardinality, Cardinality)
  IFC_DECL_FIELD(locus, SourceLocation)
IFC_DECL_END(Sentence)

/* Words: Tokens that build up a Sentence. */
IFC_DECL_START(Word)
  IFC_DECL_FIELD(locus, SourceLocation)
  IFC_DECL_FIELD(index, Index)
  IFC_DECL_FIELD(value, u16)
  IFC_DECL_FIELD(sort, WordSort)
  IFC_DECL_FIELD(__padding__, u8)
IFC_DECL_END(Word)

/* #undef macros that were set by the #includer. */
#undef IFC_DECL_START
#undef IFC_DECL_FIELD
#undef IFC_DECL_END
#undef IFC_LE_DECL_START
#undef IFC_LE_DECL_FIELD
#undef IFC_LE_DECL_END


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
