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

ifc_modules.h -- Declarations relating to ifc_modules.c (having to do with
                 Microsoft IFC modules).

*/

/* Avoid including these declarations more than once: */
#ifndef IFC_MODULES_H
#define IFC_MODULES_H 1

#if !STANDALONE_UTILITY_PROGRAM

#include "util.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

typedef struct a_tmpl_decl_state *a_tmpl_decl_state_ptr;

/* FIXME: Temporarily disable "not referenced" warnings until completed. */
/*lint -save -e755 -e758 -e768 -e769*/

/*lint -e1751*/
namespace {
/*
Magic numbers that identify the beginning of an IFC file.  Declared outside of
MICROSOFT_EXTENSIONS_ALLOWED to facilitate identifying the kind of a mismatched
module file.
*/
constexpr a_byte ifc_magic_numbers[] = { 0x54, 0x51, 0x45, 0x1A };
}  /* namespace */

#if MICROSOFT_EXTENSIONS_ALLOWED

/* These types are described by the IFC document. */

/*
For enumeration types that have associated enumerator constants, an opaque
declaration is provided first to keep a compact overview that can more
conveniently be compared to the lists in the IFC document.  The complete
definition follows later on.
*/

/* 32-bit types: */
typedef uint32_t ifc_Index_type;

enum ifc_ActiveMember : uint32_t {};
enum ifc_AttrIndex : ifc_Index_type {};
enum ifc_ByteOffset : uint32_t {};
enum ifc_Cardinality : uint32_t {};
enum ifc_ChartIndex : ifc_Index_type {};
enum ifc_Column : uint32_t {};
enum ifc_DeclIndex : ifc_Index_type {};
enum ifc_EntitySize : uint32_t {};
enum ifc_ExprIndex : ifc_Index_type {};
enum ifc_FormIndex : ifc_Index_type {};
enum ifc_FormSpecIndex : ifc_Index_type {};
enum ifc_Index : ifc_Index_type {};
enum ifc_LanguageVersion : uint32_t {};
enum ifc_LineIndex : ifc_Index_type {};
enum ifc_LineNumber : uint32_t {};
enum ifc_LitIndex : ifc_Index_type {};
enum ifc_MacroIndex : ifc_Index_type {};
enum ifc_MsvcTraits : uint32_t;  /* Defined below. */
enum ifc_NameIndex : ifc_Index_type {};
enum ifc_ParameterLevel : uint32_t {};
enum ifc_ParameterPosition : uint32_t {};
enum ifc_PragmaIndex : ifc_Index_type {};
enum ifc_ScopeIndex : ifc_Index_type {};
enum ifc_SegmentTraits : uint32_t {};
enum ifc_SegmentType : uint32_t {};
enum ifc_SentenceIndex : ifc_Index_type {};
enum ifc_StmtIndex : ifc_Index_type {};
enum ifc_StringIndex : ifc_Index_type {};
enum ifc_SyntaxIndex : ifc_Index_type {};
enum ifc_TextOffset : uint32_t {};
enum ifc_TypeIndex : ifc_Index_type {};
enum ifc_UniqueID : uint32_t {};
enum ifc_UnitIndex : ifc_Index_type {};
enum ifc_WordIndex : ifc_Index_type {};

/* 16-bit types: */
typedef uint16_t ifc_Operator_type;

enum ifc_DyadicOperator : ifc_Operator_type;  /* Defined below. */
enum ifc_EHFlags : uint16_t {};
enum ifc_FormOperator : ifc_Operator_type {};
enum ifc_FunctionTraits : uint16_t;  /* Defined below. */
enum ifc_MonadicOperator : ifc_Operator_type;  /* Defined below. */
enum ifc_NiladicOperator : ifc_Operator_type;  /* Defined below. */
enum ifc_Operator : uint16_t {};
enum ifc_PackSize : uint16_t {};
enum ifc_SourceDirective : uint16_t;  /* Defined below. */
enum ifc_SourceIdentifier : uint16_t;  /* Defined below. */
enum ifc_SourceKeyword : uint16_t;  /* Defined below. */
enum ifc_SourceLiteral : uint16_t;  /* Defined below. */
enum ifc_SourceOperator : uint16_t;  /* Defined below. */
enum ifc_SourcePunctuator : uint16_t;  /* Defined below. */
enum ifc_StorageOperator : ifc_Operator_type;  /* Defined below. */
enum ifc_TriadicOperator : ifc_Operator_type;  /* Defined below. */
enum ifc_VariadicOperator : ifc_Operator_type;  /* Defined below. */

/* 8-bit types: */
typedef uint8_t ifc_Sort_type;

enum ifc_Abi : uint8_t {};
enum ifc_Access : uint8_t;  /* Defined below. */
enum ifc_Architecture : uint8_t;  /* Defined below. */
enum ifc_Associativity : uint8_t {};
enum ifc_BasicSpecifiers : uint8_t;  /* Defined below. */
enum ifc_CallingConvention : uint8_t;  /* Defined below. */
enum ifc_DelimiterSort : ifc_Sort_type;  /* Defined below. */
enum ifc_DestructorSort : ifc_Sort_type {};
enum ifc_ExpansionMode : uint8_t;  /* Defined below. */
enum ifc_FunctionTypeTraits : uint8_t;  /* Defined below. */
enum ifc_GuideTraits : uint8_t;  /* Defined below. */
enum ifc_InitializerSort : ifc_Sort_type;  /* Defined below. */
enum ifc_NoexceptSort : ifc_Sort_type;  /* Defined below. */
enum ifc_ObjectTraits : uint8_t;  /* Defined below. */
enum ifc_ParameterSort : ifc_Sort_type;  /* Defined below. */
enum ifc_PointerDeclaratorSort : ifc_Sort_type;  /* Defined below. */
enum ifc_Qualifiers : uint8_t;  /* Defined below. */
enum ifc_ReachableProperties : uint8_t;  /* Defined below. */
enum ifc_ReadConversionSort : ifc_Sort_type;  /* Defined below. */
enum ifc_ScopeTraits : uint8_t;  /* Defined below. */
enum ifc_SyntaxSort : ifc_Sort_type;  /* Defined below. */
enum ifc_TypeBasis : uint8_t;  /* Defined below. */
enum ifc_TypePrecision : uint8_t;  /* Defined below. */
enum ifc_TypeSign : uint8_t;  /* Defined below. */
enum ifc_Version : uint8_t {};
enum ifc_WordSort : ifc_Sort_type;  /* Defined below. */

/* Embedded tags: */
enum ifc_AttrSort : ifc_Sort_type;  /* Defined below. */
enum ifc_ChartSort : ifc_Sort_type;  /* Defined below. */
enum ifc_DeclSort : ifc_Sort_type;  /* Defined below. */
enum ifc_ExprSort : ifc_Sort_type;  /* Defined below. */
enum ifc_FormSort : ifc_Sort_type;  /* Defined below. */
enum ifc_KeywordSort : ifc_Sort_type; /* Defined below. */
enum ifc_LiteralSort : ifc_Sort_type;  /* Defined below. */
enum ifc_MacroSort : ifc_Sort_type;  /* Defined below. */
enum ifc_NameSort : ifc_Sort_type;  /* Defined below. */
enum ifc_OperatorSort : ifc_Sort_type;  /* Defined below. */
enum ifc_PragmaSort : ifc_Sort_type;  /* Defined below. */
enum ifc_StmtSort : ifc_Sort_type;  /* Defined below. */
enum ifc_StringSort : ifc_Sort_type;  /* Defined below. */
enum ifc_TypeSort : ifc_Sort_type;  /* Defined below. */
enum ifc_UnitSort : ifc_Sort_type;  /* Defined below. */

/* Some IFC fields have fundamental types. */
typedef uint8_t  ifc_bool;
typedef uint8_t  ifc_u8;
typedef uint16_t ifc_u16;
typedef uint32_t ifc_u32;

/* SHA256 checksum. */
typedef uint8_t sha256_t[32];
typedef sha256_t ifc_Checksum;

/*
Define some IFC structures that are nested inside other IFC structures.  These
are handled specially here (rather than with the ifc_map.h automated method)
because the nesting can create padding issues on some architectures.
FIXME: See if these can be handled "automatically" as well.
*/
struct ifc_Sequence {
  ifc_Index	start;
  ifc_Cardinality
		cardinality;
};  /* ifc_Sequence */

typedef uint64_t a_module_ref_key;

struct ifc_ModuleReference {
  ifc_TextOffset
		owner;
  ifc_TextOffset
		partition;
  inline a_module_ref_key as_key() const;
};  /* ifc_ModuleReference */


inline a_module_ref_key ifc_ModuleReference::as_key() const
/*
Convert to a key for hashing purposes.
*/
{
  static_assert(sizeof(a_module_ref_key) >= sizeof(*this),
                "Key is not large enough");
  return (((a_module_ref_key)partition) << (sizeof(owner) * CHAR_BIT)) |
                                                      (a_module_ref_key)owner;
}  /* as_key */


struct ifc_SourceLocation {
  ifc_LineIndex	line;
  ifc_Column	column;
};  /* ifc_SourceLocation */

struct ifc_NoexceptSpecification {
  ifc_SentenceIndex
		words;
  ifc_NoexceptSort
		sort;
  uint8_t       __padding__[3];
};  /* ifc_NoexceptSpecification */

struct ifc_ParameterizedEntity {
  ifc_DeclIndex decl;
  ifc_SentenceIndex
		head;
  ifc_SentenceIndex
		body;
  ifc_SentenceIndex
		attributes;
};

struct ifc_KeywordSyntax {
  ifc_SourceLocation locus;
  ifc_KeywordSort    value;
};

/* Words: MSVC "Tokens" (primarily used by Sentences).

   Make sure to update Word (in ifc_map.h) and translate_word (in
   ifc_modules.c) for any changes made here. */
struct ifc_NestableWord {
  ifc_SourceLocation
		locus;
  ifc_Index
		index;
  uint16_t
		value;
  ifc_WordSort
		sort;
};  /* ifc_NestableWord */

/*
Create structures for each of the IFC entities by setting the IFC_DECL macros
appropriately and including ifc_map.h.  The net result is something like:

  struct an_ifc_foo {
    ifc_field1_type field1;
    ifc_field2_type field2;
    ...
  };
*/

/*lint -estring(823,IFC_DECL_START)*/
/*lint -estring(823,IFC_DECL_FIELD)*/
/*lint -estring(823,IFC_DECL_END)*/
/*lint -estring(823,IFC_LE_DECL_START)*/
/*lint -estring(823,IFC_LE_DECL_FIELD)*/
/*lint -estring(823,IFC_LE_DECL_END)*/
#define IFC_DECL_START(name) \
  struct concat(an_ifc_, name) {
#define IFC_DECL_FIELD(field, type) \
    concat(ifc_, type)  field;
#define IFC_DECL_END(name) \
  };  /* #name */

#include "ifc_map.h"

enum ifc_DelimiterSort : uint8_t {
  ifc_Delimiter_Unknown = 0,
  ifc_Delimiter_Brace = 1,
  ifc_Delimiter_Parenthesis = 2
};

/*lint -save -e835*/ /* Allow 1 << 0 in the code below. */
/* Enumeration for Architectures. */
enum ifc_Architecture : uint8_t {
  ifc_Architecture_Unknown        = 0,
  ifc_Architecture_X86            = 0x01,
  ifc_Architecture_X64            = 0x02,
  ifc_Architecture_ARM32          = 0x03,
  ifc_Architecture_ARM64          = 0x04,
  ifc_Architecture_HybridX86ARM64 = 0x05,
};

/* Enumeration for Qualifiers. */
enum ifc_Qualifiers : uint8_t {
  ifc_Qualifier_None     = 0,  
  ifc_Qualifier_Const    = 1 << 0,
  ifc_Qualifier_Volatile = 1 << 1,
  ifc_Qualifier_Restrict = 1 << 2,
};

/* Enumeration for ReachableProperties. */
enum ifc_ReachableProperties : uint8_t {
  ifc_ReachableProperties_None             = 0,
                         /* Nothing beyond name, type, scope. */
  ifc_ReachableProperties_Initializer      = 1 << 0,
                         /* IPR-initializer exported. */
  ifc_ReachableProperties_DefaultArguments = 1 << 1,
                         /* Function or template default arguments exported. */
  ifc_ReachableProperties_Attributes       = 1 << 2,
                         /* Standard attributes exported. */
  ifc_ReachableProperties_All              = 0xFF,
                         /* Everything. */
};

/* Enumeration for Access specifiers. */
enum ifc_Access : uint8_t {
  ifc_Access_None,      /* No access specifier. */
  ifc_Access_Private,   /* "private" for scope member. */
  ifc_Access_Protected, /* "protected" for scope member. */
  ifc_Access_Public,    /* "public" for scope member. */
};

/* Enumeration for BasicSpecifiers. */
enum ifc_BasicSpecifiers : uint8_t {
  ifc_BasicSpecifiers_Cxx               = 0,      /* C++ language linkage */
  ifc_BasicSpecifiers_C                 = 1 << 0, /* C language linkage */
  ifc_BasicSpecifiers_Internal          = 1 << 1, /* Internal linkage */
  ifc_BasicSpecifiers_Vague             = 1 << 2, /* Vague linkage, e.g.
                                                     COMDAT, still external */
  ifc_BasicSpecifiers_External          = 1 << 3, /* External linkage */
  ifc_BasicSpecifiers_Deprecated        = 1 << 4, /* [[deprecated("bar")]] */
  ifc_BasicSpecifiers_InitializedInClass= 1 << 5, /* Defined or initialized in
                                                     class */
  ifc_BasicSpecifiers_NonExported       = 1 << 6, /* Not explicitly exported */
  ifc_BasicSpecifiers_IsMemberOfGlobalModules
                                        = 1 << 7, /* Member of the global
                                                     module */
};


inline a_boolean is_from_gmf(ifc_BasicSpecifiers specifier)
/*
Return TRUE if the specifier indicates the associated entity came from the
global module fragment, FALSE otherwise.
*/
{
  a_boolean result;
  /* Lambda closure class members are marked as being members of the GMF
     without actually being members of the GMF.  More generally, if something
     is the member of a class then it's reasonable to expect that we can treat
     it as not a member of the GMF and allow the containing class' membership
     in the GMF to cover it, if necessary. */
  result = ((specifier & ifc_BasicSpecifiers_IsMemberOfGlobalModules) != 0) &&
           ((specifier & ifc_BasicSpecifiers_InitializedInClass) == 0);
  return result;
}  /* is_from_gmf */


/* Enumeration for ObjectTraits. */
enum ifc_ObjectTraits : uint8_t {
  ifc_ObjectTraits_None        = 0,
  ifc_ObjectTraits_Constexpr   = 1 << 0,
  ifc_ObjectTraits_Mutable     = 1 << 1,
  ifc_ObjectTraits_ThreadLocal = 1 << 2,
  ifc_ObjectTraits_Inline      = 1 << 3,
  ifc_ObjectTraits_InitializerExported
                               = 1 << 4,
  ifc_ObjectTraits_Vendor      = 1 << 7
};

/* Enumeration for MsvcTraits. */
enum ifc_MsvcTraits : uint32_t {
  ifc_MsvcTraits_None            = 0,
  ifc_MsvcTraits_ForceInline     = 1 << 0,
  ifc_MsvcTraits_Naked           = 1 << 1,
  ifc_MsvcTraits_NoAlias         = 1 << 2,
  ifc_MsvcTraits_NoInline        = 1 << 3,
  ifc_MsvcTraits_Restrict        = 1 << 4,
  ifc_MsvcTraits_SafeBuffers     = 1 << 5,
  ifc_MsvcTraits_DllExport       = 1 << 6,
  ifc_MsvcTraits_DllImport       = 1 << 7,
  ifc_MsvcTraits_CodeSegment     = 1 << 8,
  ifc_MsvcTraits_Novtable        = 1 << 9,
  ifc_MsvcTraits_IntrinsicType   = 1 << 10,
  ifc_MsvcTraits_EmptyBases      = 1 << 11,
  ifc_MsvcTraits_Process         = 1 << 12,
  ifc_MsvcTraits_Allocate        = 1 << 13,
  ifc_MsvcTraits_SelectAny       = 1 << 14,
  ifc_MsvcTraits_Comdat          = 1 << 15,
  ifc_MsvcTraits_Uuid            = 1 << 16,
};

/* Enumeration for FunctionTraits. */
enum ifc_FunctionTraits : uint16_t {
  ifc_FunctionTraits_None         = 0,
  ifc_FunctionTraits_Inline       = 1 << 0,
  ifc_FunctionTraits_Constexpr    = 1 << 1,
  ifc_FunctionTraits_Explicit     = 1 << 2,
  ifc_FunctionTraits_Virtual      = 1 << 3,
  ifc_FunctionTraits_NoReturn     = 1 << 4,
  ifc_FunctionTraits_PureVirtual  = 1 << 5,
  ifc_FunctionTraits_HiddenFriend = 1 << 6,
  ifc_FunctionTraits_Defaulted    = 1 << 7,
  ifc_FunctionTraits_Deleted      = 1 << 8,
  ifc_FunctionTraits_Constrained  = 1 << 9,
  ifc_FunctionTraits_Immediate    = 1 << 10,
  ifc_FunctionTraits_Vendor       = 1 << 15,
};

/* Enumeration for FunctionTypeTraits. */
enum ifc_FunctionTypeTraits : uint8_t {
  ifc_FunctionTypeTraits_None     = 0,
  ifc_FunctionTypeTraits_Const    = 1 << 0,
  ifc_FunctionTypeTraits_Volatile = 1 << 1,
  ifc_FunctionTypeTraits_Lvalue   = 1 << 2,
  ifc_FunctionTypeTraits_Rvalue   = 1 << 3,
};

/* Enumeration for GuideTraits. */
enum ifc_GuideTraits : uint8_t {
  ifc_GuideTraits_Nothing         = 0,
  ifc_GuideTraits_Explicit        = 1 << 0,
};

/* Enumeration for CallingConventions. */
enum ifc_CallingConvention : uint8_t {
  ifc_CallingConvention_Cdecl,
  ifc_CallingConvention_Fast,
  ifc_CallingConvention_Std,
  ifc_CallingConvention_This,
  ifc_CallingConvention_Clr,
  ifc_CallingConvention_Vector,
  ifc_CallingConvention_Eabi,
};

/* Enumeration for ExpansionModes. */
enum ifc_ExpansionMode : uint8_t {
  ifc_ExpansionMode_Full,
  ifc_ExpansionMode_Partial,
};

/* Enumeration for NoexceptSpecification. */
enum ifc_NoexceptSort : ifc_Sort_type {
  ifc_NoexceptSort_None,
  ifc_NoexceptSort_False,
  ifc_NoexceptSort_True,
  ifc_NoexceptSort_Expression,
  ifc_NoexceptSort_Inferred,
  ifc_NoexceptSort_Unenforced,
};

/* Enumeration for ScopeTraits. */
enum ifc_ScopeTraits : uint8_t {
  ifc_ScopeTraits_None          = 0,
  ifc_ScopeTraits_Unnamed       = 1 << 0,
  ifc_ScopeTraits_Inline        = 1 << 1,
  ifc_ScopeTraits_InitializerExported
                                = 1 << 2,
  ifc_ScopeTraits_ClosureType   = 1 << 3,
  ifc_ScopeTraits_Vendor        = 1 << 7,
};
/*lint -restore*/

/* Macros used to access UnitIndex::tag and UnitIndex::value. */
#define unit_tag(unit) ((ifc_UnitSort)((unit) & 0x00000007))
#define unit_value(unit) ((ifc_Index)((unit) >> 3))

/* Enumeration for UnitSort (i.e., types of modules). */
enum ifc_UnitSort : ifc_Sort_type {
  ifc_UnitSort_Source,
  ifc_UnitSort_Primary,
  ifc_UnitSort_Partition,
  ifc_UnitSort_Header,
  ifc_UnitSort_ExportedTU,
};

/* Enumeration for ParameterSort (i.e., types of parameters). */
enum ifc_ParameterSort : ifc_Sort_type {
  ifc_ParameterSort_Object,           /* Function parameter. */
  ifc_ParameterSort_Type,             /* Type template parameter. */
  ifc_ParameterSort_NonType,          /* Non-type template parameter. */
  /* The IFC spec is missing this, and it's scheduled for removal. */
  ifc_ParameterSort_Placeholder,      /* "auto" template parameter. */
  ifc_ParameterSort_Template,         /* Template template parameter. */
};

/* The IFC spec is missing this - details provided via email correspondence. */
/* Enumerator for PointerDeclaratorSort (i.e., types of pointer declarators).*/
enum ifc_PointerDeclaratorSort : ifc_Sort_type {
  ifc_PointerDeclaratorSort_None,
  ifc_PointerDeclaratorSort_Pointer,            /* T* */
  ifc_PointerDeclaratorSort_LvalueReference,    /* T& */
  ifc_PointerDeclaratorSort_RvalueReference,    /* T&& */
  ifc_PointerDeclaratorSort_PointerToMember,    /* T X::* */
};

/* Macros used to access TypeIndex::tag and TypeIndex::value. */
#define type_tag(type) ((ifc_TypeSort)((type) & 0x0000001F))
#define type_value(type) ((ifc_Index)((type) >> 5))

/* Enumeration for TypeSort (i.e., types of types). */
enum ifc_TypeSort : ifc_Sort_type {
  ifc_TypeSort_VendorExtension,
  ifc_TypeSort_Fundamental,
  ifc_TypeSort_Designated,
  ifc_TypeSort_Tor,
  ifc_TypeSort_Syntactic,
  ifc_TypeSort_Expansion,
  ifc_TypeSort_Pointer,
  ifc_TypeSort_PointerToMember,
  ifc_TypeSort_LvalueReference,
  ifc_TypeSort_RvalueReference,
  ifc_TypeSort_Function,
  ifc_TypeSort_Method,
  ifc_TypeSort_Array,
  ifc_TypeSort_Typename,
  ifc_TypeSort_Qualified,
  ifc_TypeSort_Base,
  ifc_TypeSort_Decltype,
  ifc_TypeSort_Placeholder,
  ifc_TypeSort_Tuple,
  ifc_TypeSort_Forall,
  ifc_TypeSort_Unaligned,
  ifc_TypeSort_SyntaxTree,
  /* Must be last. */
  ifc_TypeSort_Last
};

/* Macros used to access StmtIndex::tag and StmtIndex::value. */
#define stmt_tag(stmt) ((ifc_StmtSort)((stmt) & 0x0000001F))
#define stmt_value(stmt) ((ifc_Index)((stmt) >> 5))

/* Enumeration for StmtSort (i.e., types of statements). */
enum ifc_StmtSort : ifc_Sort_type {
  ifc_StmtSort_VendorExtension,
  ifc_StmtSort_Empty,
  ifc_StmtSort_If,
  ifc_StmtSort_For,
  ifc_StmtSort_Case,
  ifc_StmtSort_While,
  ifc_StmtSort_Block,
  ifc_StmtSort_Break,
  ifc_StmtSort_Switch,
  ifc_StmtSort_DoWhile,
  ifc_StmtSort_Default,
  ifc_StmtSort_Continue,
  ifc_StmtSort_Expression,
  ifc_StmtSort_Return,
  ifc_StmtSort_VariableDecl,
  ifc_StmtSort_Expansion,
  ifc_StmtSort_SyntaxTree,
  /* Must be last. */
  ifc_StmtSort_Last
};

/* Macros used to access ExprIndex::tag and ExprIndex::value. */
#define expr_tag(expr) ((ifc_ExprSort)((expr) & 0x0000003F))
#define expr_value(expr) ((ifc_Index)((expr) >> 6))

/* Enumeration for ExprSort (i.e., types of expressions). */
enum ifc_ExprSort : ifc_Sort_type {
  ifc_ExprSort_VendorExtension,
  ifc_ExprSort_Empty,
  ifc_ExprSort_Literal,
  ifc_ExprSort_Lambda,
  ifc_ExprSort_Type,
  ifc_ExprSort_NamedDecl,
  ifc_ExprSort_UnresolvedId,
  ifc_ExprSort_TemplateId,
  ifc_ExprSort_UnqualifiedId,
  ifc_ExprSort_SimpleIdentifier,
  ifc_ExprSort_Pointer,
  ifc_ExprSort_QualifiedName,
  ifc_ExprSort_Path,
  ifc_ExprSort_Read,
  ifc_ExprSort_Monad,
  ifc_ExprSort_Dyad,
  ifc_ExprSort_Triad,
  ifc_ExprSort_String,
  ifc_ExprSort_Temporary,
  ifc_ExprSort_Call,
  ifc_ExprSort_MemberInitializer,
  ifc_ExprSort_MemberAccess,
  ifc_ExprSort_InheritancePath,
  ifc_ExprSort_InitializerList,
  ifc_ExprSort_Cast,
  ifc_ExprSort_Condition,
  ifc_ExprSort_ExpressionList,
  ifc_ExprSort_SizeofType,
  ifc_ExprSort_Alignof,
  ifc_ExprSort_New,
  ifc_ExprSort_Delete,
  ifc_ExprSort_Typeid,
  ifc_ExprSort_DestructorCall,
  ifc_ExprSort_SyntaxTree,
  ifc_ExprSort_FunctionString,
  ifc_ExprSort_CompoundString,
  ifc_ExprSort_StringSequence,
  ifc_ExprSort_Initializer,
  ifc_ExprSort_Requires,
  ifc_ExprSort_UnaryFold,
  ifc_ExprSort_BinaryFold,
  ifc_ExprSort_HierarchyConversion,
  ifc_ExprSort_ProductTypeValue,
  ifc_ExprSort_SumTypeValue,
  ifc_ExprSort_SubobjectValue,
  ifc_ExprSort_ArrayValue,
  ifc_ExprSort_DynamicDispatch,
  ifc_ExprSort_VirtualFunctionConversion,
  ifc_ExprSort_Placeholder,
  ifc_ExprSort_Expansion,
  ifc_ExprSort_Generic,
  ifc_ExprSort_Tuple,
  ifc_ExprSort_Nullptr,
  ifc_ExprSort_This,
  ifc_ExprSort_TemplateReference,
  ifc_ExprSort_PushState,
  ifc_ExprSort_TypeTraitIntrinsic,
  ifc_ExprSort_DesignatedInitializer,
  ifc_ExprSort_PackedTemplateArguments,
  ifc_ExprSort_Tokens,
  ifc_ExprSort_AssignInitializer,
  /* Must be last. */
  ifc_ExprSort_Last
};

/* Enumeration for ReadConversionSort (i.e., kinds of read conversions). */
enum ifc_ReadConversionSort : ifc_Sort_type {
  ifc_ReadConversionSort_Identity,
  ifc_ReadConversionSort_Indirection,
  ifc_ReadConversionSort_Dereference,
  ifc_ReadConversionSort_LvalueToRvalue,
  ifc_ReadConversionSort_IntegralConversion,
};

/* Enumeration for InitializerSort (i.e., kinds of initialization) */
enum ifc_InitializerSort : ifc_Sort_type {
  ifc_InitializerSort_Unknown,
  ifc_InitializerSort_Direct,
  ifc_InitializerSort_Copy,
};

/* Macros used to access StringIndex::tag and StringIndex::value. */
/* Spec says StringSort has width 4 bits, IFC files suggest it's 3 bits. */
#define str_tag(str) ((ifc_StringSort)((str) & 0x00000007))
#define str_value(str) ((ifc_Index)((str) >> 3))

/* Enumeration for StringSort (i.e., kinds of strings). */
enum ifc_StringSort : ifc_Sort_type {
  ifc_StringSort_Ordinary,
  ifc_StringSort_UTF8,
  ifc_StringSort_Char16,
  ifc_StringSort_Char32,
  ifc_StringSort_Wide,
};

/* Enumeration for TypeBasis (i.e., kinds of fundamental types). */
enum ifc_TypeBasis : uint8_t {
  ifc_TypeBasis_Void,
  ifc_TypeBasis_Bool,
  ifc_TypeBasis_Char,
  ifc_TypeBasis_Wchar_t,
  ifc_TypeBasis_Int,
  ifc_TypeBasis_Float,
  ifc_TypeBasis_Double,
  ifc_TypeBasis_Nullptr,
  ifc_TypeBasis_Ellipsis,
  ifc_TypeBasis_SegmentType,
  ifc_TypeBasis_Class,
  ifc_TypeBasis_Struct,
  ifc_TypeBasis_Union,
  ifc_TypeBasis_Enum,
  ifc_TypeBasis_Typename,
  ifc_TypeBasis_Namespace,
  ifc_TypeBasis_Interface,
  ifc_TypeBasis_Function,
  ifc_TypeBasis_Empty,
  ifc_TypeBasis_VariableTemplate,
  ifc_TypeBasis_Auto,
  ifc_TypeBasis_DecltypeAuto,
};

/* Enumeration for TypePrecision (i.e., sizes of fundamental types). */
enum ifc_TypePrecision : uint8_t {
  ifc_TypePrecision_Default,
  ifc_TypePrecision_Short,
  ifc_TypePrecision_Long,
  /* The IFC spec is missing this, but IFC files indicate otherwise. */
  ifc_TypePrecision_Bit8,
  ifc_TypePrecision_Bit16,
  ifc_TypePrecision_Bit32,
  ifc_TypePrecision_Bit64,
  ifc_TypePrecision_Bit128,
};

/* Enumeration for TypeSign (i.e., sign of fundamental types). */
enum ifc_TypeSign : uint8_t {
  ifc_TypeSign_Plain,
  ifc_TypeSign_Signed,
  ifc_TypeSign_Unsigned,
};

/* Macros used to access DeclIndex::tag and DeclIndex::value. */
#define decl_tag(decl) ((ifc_DeclSort)((decl) & 0x0000001F))
#define decl_value(decl) ((ifc_Index)((decl) >> 5))
#define make_decl_index(tag, idx) \
  ((ifc_DeclIndex)(((idx) << 5) | ((tag) & 0x0000001F)))

/* Enumeration for DeclSort (i.e., types of declarations). */
enum ifc_DeclSort : ifc_Sort_type {
  ifc_DeclSort_VendorExtension,
  ifc_DeclSort_Enumerator,
  ifc_DeclSort_Variable,
  ifc_DeclSort_Parameter,
  ifc_DeclSort_Field,
  ifc_DeclSort_Bitfield,
  ifc_DeclSort_Scope,
  ifc_DeclSort_Enumeration,
  ifc_DeclSort_Alias,
  ifc_DeclSort_Temploid,
  ifc_DeclSort_Template,
  ifc_DeclSort_PartialSpecialization,
  ifc_DeclSort_ExplicitSpecialization,
  ifc_DeclSort_ExplicitInstantiation,
  ifc_DeclSort_Concept,
  ifc_DeclSort_Function,
  ifc_DeclSort_Method,
  ifc_DeclSort_Constructor,
  ifc_DeclSort_InheritedConstructor,
  ifc_DeclSort_Destructor,
  ifc_DeclSort_Reference,
  ifc_DeclSort_UsingDeclaration,
  ifc_DeclSort_UsingDirective,
  ifc_DeclSort_Friend,
  ifc_DeclSort_Expansion,
  ifc_DeclSort_DeductionGuide,
  ifc_DeclSort_Barren,
  ifc_DeclSort_Tuple,
  ifc_DeclSort_SyntaxTree,
  ifc_DeclSort_Intrinsic,
  ifc_DeclSort_Property,
  ifc_DeclSort_OutputSegment,
  /* Must be last. */
  ifc_DeclSort_Last
};

/* Macros used to access NameIndex::tag and NameIndex::value. */
#define name_tag(name) ((ifc_NameSort)((name) & 0x00000007))
#define name_value(name) ((ifc_Index)((name) >> 3))

/* Enumeration for NameSort (i.e., types of names). */
enum ifc_NameSort : ifc_Sort_type {
  ifc_NameSort_Identifier,
  ifc_NameSort_Operator,
  ifc_NameSort_Conversion,
  ifc_NameSort_Literal,
  ifc_NameSort_Template,
  ifc_NameSort_Specialization,
  ifc_NameSort_SourceFile,
  ifc_NameSort_Guide,
  /* Must be last. */
  ifc_NameSort_Last
};

/* Macros used to access LitIndex::tag and LitIndex::index. */
#define literal_tag(litindex) ((ifc_LiteralSort)((litindex) & 0x00000003))
#define literal_index(litindex) ((ifc_Index)((litindex) >> 2))

/* Enumeration for LiteralSort (i.e., types of literals). */
enum ifc_LiteralSort : ifc_Sort_type {
  ifc_LiteralSort_Immediate,
  ifc_LiteralSort_Integer,
  ifc_LiteralSort_FloatingPoint,
};

/* Macros used to access Operator::tag and Operator::index. */
#define operator_tag(op) ((ifc_OperatorSort)((op) & 0x0000000F))
#define operator_index(op) ((uint16_t)((op) >> 4))

/* Enumeration for OperatorSort (i.e., types of operators). */
enum ifc_OperatorSort : ifc_Sort_type {
  ifc_OperatorSort_Niladic,
  ifc_OperatorSort_Monadic,
  ifc_OperatorSort_Dyadic,
  ifc_OperatorSort_Triadic,
  ifc_OperatorSort_Storage = 0x0E,
  ifc_OperatorSort_Variadic,
};

/* Enumeration of niladic operators (operators accepting no arguments). */
enum ifc_NiladicOperator : ifc_Operator_type {
  ifc_NiladicOperator_Unknown,
  ifc_NiladicOperator_Phantom,
  ifc_NiladicOperator_Constant,
  ifc_NiladicOperator_Nil,
  /* MSVC-specific. */
  ifc_NiladicOperator_Msvc = 0x400,
  ifc_NiladicOperator_MsvcConstantObject,
  ifc_NiladicOperator_MsvcLambda,
};

/* Enumeration of monadic operators (operators accepting one argument). */
enum ifc_MonadicOperator : ifc_Operator_type {
  ifc_MonadicOperator_Unknown,
  ifc_MonadicOperator_Plus,
  ifc_MonadicOperator_Negate,
  ifc_MonadicOperator_Deref,
  ifc_MonadicOperator_Address,
  ifc_MonadicOperator_Complement,
  ifc_MonadicOperator_Not,
  ifc_MonadicOperator_PreIncrement,
  ifc_MonadicOperator_PreDecrement,
  ifc_MonadicOperator_PostIncrement,
  ifc_MonadicOperator_PostDecrement,
  ifc_MonadicOperator_Truncate,
  ifc_MonadicOperator_Ceil,
  ifc_MonadicOperator_Floor,
  ifc_MonadicOperator_Paren,
  ifc_MonadicOperator_Brace,
  ifc_MonadicOperator_Alignas,
  ifc_MonadicOperator_Alignof,
  ifc_MonadicOperator_Sizeof,
  ifc_MonadicOperator_Cardinality,
  ifc_MonadicOperator_Typeid,
  ifc_MonadicOperator_Noexcept,
  ifc_MonadicOperator_Requires,
  ifc_MonadicOperator_CoReturn,
  ifc_MonadicOperator_Await,
  ifc_MonadicOperator_Yield,
  ifc_MonadicOperator_Throw,
  ifc_MonadicOperator_New,
  ifc_MonadicOperator_Delete,
  ifc_MonadicOperator_DeleteArray,
  ifc_MonadicOperator_Expand,
  ifc_MonadicOperator_Read,
  ifc_MonadicOperator_Materialize,
  ifc_MonadicOperator_PseudoDtorCall,
  /* MSVC-specific. */
  ifc_MonadicOperator_Msvc = 0x400,
  ifc_MonadicOperator_MsvcAssume,
  ifc_MonadicOperator_MsvcAlignof,
  ifc_MonadicOperator_MsvcUuidof,
  ifc_MonadicOperator_MsvcIsClass,
  ifc_MonadicOperator_MsvcIsUnion,
  ifc_MonadicOperator_MsvcIsEnum,
  ifc_MonadicOperator_MsvcIsPolymorphic,
  ifc_MonadicOperator_MsvcIsEmpty,
  ifc_MonadicOperator_MsvcIsTriviallyCopyConstructible,
  ifc_MonadicOperator_MsvcIsTriviallyCopyAssignable,
  ifc_MonadicOperator_MsvcIsTriviallyDestructible,
  ifc_MonadicOperator_MsvcHasVirtualDestructor,
  ifc_MonadicOperator_MsvcIsNothrowCopyConstructible,
  ifc_MonadicOperator_MsvcIsNothrowCopyAssignable,
  ifc_MonadicOperator_MsvcIsPod,
  ifc_MonadicOperator_MsvcIsAbstract,
  ifc_MonadicOperator_MsvcIsTrivial,
  ifc_MonadicOperator_MsvcIsTriviallyCopyable,
  ifc_MonadicOperator_MsvcIsStandardLayout,
  ifc_MonadicOperator_MsvcIsLiteralType,
  ifc_MonadicOperator_MsvcIsTriviallyMoveConstructible,
  ifc_MonadicOperator_MsvcHasTrivialMoveAssign,
  ifc_MonadicOperator_MsvcIsTriviallyMoveAssignable,
  ifc_MonadicOperator_MsvcIsNothrowMoveAssignable,
  ifc_MonadicOperator_MsvcUnderlyingType,
  ifc_MonadicOperator_MsvcIsDestructible,
  ifc_MonadicOperator_MsvcIsNothrowDestructible,
  ifc_MonadicOperator_MsvcHasUniqueObjectRepresentations,
  ifc_MonadicOperator_MsvcIsAggregate,
  ifc_MonadicOperator_MsvcBuiltinAddressOf,
  ifc_MonadicOperator_MsvcIsRefClass,
  ifc_MonadicOperator_MsvcIsValueClass,
  ifc_MonadicOperator_MsvcIsSimpleValueClass,
  ifc_MonadicOperator_MsvcIsInterfaceClass,
  ifc_MonadicOperator_MsvcIsDelegate,
  ifc_MonadicOperator_MsvcIsFinal,
  ifc_MonadicOperator_MsvcIsSealed,
  ifc_MonadicOperator_MsvcHasFinalizer,
  ifc_MonadicOperator_MsvcHasCopy,
  ifc_MonadicOperator_MsvcHasAssign,
  ifc_MonadicOperator_MsvcHasUserDestructor,
  ifc_MonadicOperator_MsvcConfusion = 0xFE0,
  ifc_MonadicOperator_MsvcConfusedExpand,
};

/* Enumeration of dyadic operators (operators accepting two arguments). */
enum ifc_DyadicOperator : ifc_Operator_type {
  ifc_DyadicOperator_Unknown,
  ifc_DyadicOperator_Plus,
  ifc_DyadicOperator_Minus,
  ifc_DyadicOperator_Mult,
  ifc_DyadicOperator_Slash,
  ifc_DyadicOperator_Modulo,
  ifc_DyadicOperator_Remainder,
  ifc_DyadicOperator_Bitand,
  ifc_DyadicOperator_Bitor,
  ifc_DyadicOperator_Bitxor,
  ifc_DyadicOperator_Lshift,
  ifc_DyadicOperator_Rshift,
  ifc_DyadicOperator_Equal,
  ifc_DyadicOperator_NotEqual,
  ifc_DyadicOperator_Less,
  ifc_DyadicOperator_LessEqual,
  ifc_DyadicOperator_Greater,
  ifc_DyadicOperator_GreaterEqual,
  ifc_DyadicOperator_Compare,
  ifc_DyadicOperator_LogicAnd,
  ifc_DyadicOperator_LogicOr,
  ifc_DyadicOperator_Assign,
  ifc_DyadicOperator_PlusAssign,
  ifc_DyadicOperator_MinusAssign,
  ifc_DyadicOperator_MultAssign,
  ifc_DyadicOperator_SlashAssign,
  ifc_DyadicOperator_ModuloAssign,
  ifc_DyadicOperator_BitandAssign,
  ifc_DyadicOperator_BitorAssign,
  ifc_DyadicOperator_BitxorAssign,
  ifc_DyadicOperator_LshiftAssign,
  ifc_DyadicOperator_RshiftAssign,
  ifc_DyadicOperator_Comma,
  ifc_DyadicOperator_Dot,
  ifc_DyadicOperator_Arrow,
  ifc_DyadicOperator_DotStar,
  ifc_DyadicOperator_ArrowStar,
  ifc_DyadicOperator_Curry,
  ifc_DyadicOperator_Apply,
  ifc_DyadicOperator_Index,
  ifc_DyadicOperator_DefaultAt,
  ifc_DyadicOperator_New,
  ifc_DyadicOperator_NewArray,
  ifc_DyadicOperator_Destruct,
  ifc_DyadicOperator_DestructAt,
  ifc_DyadicOperator_Cleanup,
  ifc_DyadicOperator_Qualification,
  ifc_DyadicOperator_Promote,
  ifc_DyadicOperator_Demote,
  ifc_DyadicOperator_Coerce,
  ifc_DyadicOperator_Rewrite,
  ifc_DyadicOperator_Bless,
  ifc_DyadicOperator_Cast,
  ifc_DyadicOperator_ExplicitConversion,
  ifc_DyadicOperator_ReinterpretCast,
  ifc_DyadicOperator_StaticCast,
  ifc_DyadicOperator_ConstCast,
  ifc_DyadicOperator_DynamicCast,
  ifc_DyadicOperator_Narrow,
  ifc_DyadicOperator_Widen,
  ifc_DyadicOperator_Pretend,
  ifc_DyadicOperator_Closure,
  ifc_DyadicOperator_ZeroInitialize,
  ifc_DyadicOperator_ClearStorage,
  /* MSVC-specific. */
  ifc_DyadicOperator_Msvc = 0x400,
  ifc_DyadicOperator_MsvcTryCast,
  ifc_DyadicOperator_MsvcCurry,
  ifc_DyadicOperator_MsvcVirtualCurry,
  ifc_DyadicOperator_MsvcAlign,
  ifc_DyadicOperator_MsvcBitSpan,
  ifc_DyadicOperator_MsvcBitfieldAccess,
  ifc_DyadicOperator_MsvcObscureBitfieldAccess,
  ifc_DyadicOperator_MsvcInitialize,
  ifc_DyadicOperator_MsvcBuiltinOffsetOf,
  ifc_DyadicOperator_MsvcIsBaseOf,
  ifc_DyadicOperator_MsvcIsConvertibleTo,
  ifc_DyadicOperator_MsvcIsTriviallyAssignable,
  ifc_DyadicOperator_MsvcIsNothrowAssignable,
  ifc_DyadicOperator_MsvcIsAssignable,
  ifc_DyadicOperator_MsvcIsAssignableNocheck,
  ifc_DyadicOperator_MsvcBuiltinBitCast,
  ifc_DyadicOperator_MsvcBuiltinIsLayoutCompatible,
  ifc_DyadicOperator_MsvcBuiltinIsPointerInterconvertibleBaseOf,
  ifc_DyadicOperator_MsvcBuiltinIsPointerInterconvertibleWithClass,
  ifc_DyadicOperator_MsvcBuiltinIsCorrespondingMember,
  ifc_DyadicOperator_MsvcIntrinsic,
};

/* Enumeration of triadic operators (operators accepting three arguments). */
enum ifc_TriadicOperator : ifc_Operator_type {
  ifc_TriadicOperator_Unknown,
  ifc_TriadicOperator_Choice,
  ifc_TriadicOperator_ConstructAt,
  ifc_TriadicOperator_Initialize,
  /* MSVC-specific. */
  ifc_TriadicOperator_Msvc = 0x400,
};

/* Enumeration of storage operators (operators for storage [de]allocation). */
enum ifc_StorageOperator : ifc_Operator_type {
  ifc_StorageOperator_Unknown,
  ifc_StorageOperator_AllocateSingle,
  ifc_StorageOperator_AllocateArray,
  ifc_StorageOperator_DeallocateSingle,
  ifc_StorageOperator_DeallocateArray,
  /* MSVC-specific. */
  ifc_StorageOperator_Msvc = 0x7DE,
};

/* Enumeration of variadic operators (operators accepting any number of
   arguments). */
enum ifc_VariadicOperator : ifc_Operator_type {
  ifc_VariadicOperator_Unknown,
  ifc_VariadicOperator_Collection,
  ifc_VariadicOperator_Sequence,
  /* MSVC-specific. */
  ifc_VariadicOperator_Msvc = 0x400,
  ifc_VariadicOperator_MsvcHasTrivialConstructor,
  ifc_VariadicOperator_MsvcIsConstructible,
  ifc_VariadicOperator_MsvcIsNothrowConstructible,
  ifc_VariadicOperator_MsvcIsTriviallyConstructible,
};

/* Macros used to access ChartIndex::tag and ChartIndex::value. */
#define chart_tag(chart) ((ifc_ChartSort)((chart) & 0x00000003))
#define chart_value(chart) ((ifc_Index)((chart) >> 2))

/* Enumeration for ChartSort (i.e., kinds of charts). */
enum ifc_ChartSort : ifc_Sort_type {
  ifc_ChartSort_None,
  ifc_ChartSort_Unilevel,
  ifc_ChartSort_Multilevel,
  /* Must be last. */
  ifc_ChartSort_Last
};

/* Macros used to access AttrIndex::tag and AttrIndex::value. */
#define attr_tag(attr) ((ifc_AttrSort)((attr) & 0x0000000F))
#define attr_value(attr) ((ifc_Index)((attr) >> 4))

/* Enumeration for AttrSort (i.e., kinds of attributes). */
enum ifc_AttrSort : ifc_Sort_type {
  ifc_AttrSort_Nothing,
  ifc_AttrSort_Basic,
  ifc_AttrSort_Scoped,
  ifc_AttrSort_Labeled,
  ifc_AttrSort_Called,
  ifc_AttrSort_Expanded,
  ifc_AttrSort_Factored,
  ifc_AttrSort_Elaborated,
  ifc_AttrSort_Tuple,
  /* Must be last. */
  ifc_AttrSort_Last
};

/* Enumeration for WordSort (i.e., kinds of words). */
enum ifc_WordSort : ifc_Sort_type {
  ifc_WordSort_Unknown,
  ifc_WordSort_Directive,
  ifc_WordSort_Punctuator,
  ifc_WordSort_Literal,
  ifc_WordSort_Operator,
  ifc_WordSort_Keyword,
  ifc_WordSort_Identifier,
};

/* Enumeration for SourceDirective (i.e., types of directives) */
enum ifc_SourceDirective : uint16_t {
  /* MSVC-specific */
  ifc_SourceDirective_Msvc = 0x1FFF,
  ifc_SourceDirective_MsvcPragmaPush,
  ifc_SourceDirective_MsvcPragmaPop,
  ifc_SourceDirective_MsvcDirectiveStart,
  ifc_SourceDirective_MsvcDirectiveEnd,
  ifc_SourceDirective_MsvcPragmaAllocText,
  ifc_SourceDirective_MsvcPragmaAutoInline,
  ifc_SourceDirective_MsvcPragmaBssSeg,
  ifc_SourceDirective_MsvcPragmaCheckStack,
  ifc_SourceDirective_MsvcPragmaCodeSeg,
  ifc_SourceDirective_MsvcPragmaComment,
  ifc_SourceDirective_MsvcPragmaComponent,
  ifc_SourceDirective_MsvcPragmaConform,
  ifc_SourceDirective_MsvcPragmaConstSeg,
  ifc_SourceDirective_MsvcPragmaDataSeg,
  ifc_SourceDirective_MsvcPragmaDeprecated,
  ifc_SourceDirective_MsvcPragmaDetectMismatch,
  ifc_SourceDirective_MsvcPragmaEndregion,
  ifc_SourceDirective_MsvcPragmaExecutionCharacterSet,
  ifc_SourceDirective_MsvcPragmaFenvAccess,
  ifc_SourceDirective_MsvcPragmaFileHash,
  ifc_SourceDirective_MsvcPragmaFloatControl,
  ifc_SourceDirective_MsvcPragmaFpContract,
  ifc_SourceDirective_MsvcPragmaFunction,
  ifc_SourceDirective_MsvcPragmaBGI,
  ifc_SourceDirective_MsvcPragmaIdent,
  ifc_SourceDirective_MsvcPragmaImplementationKey,
  ifc_SourceDirective_MsvcPragmaIncludeAlias,
  ifc_SourceDirective_MsvcPragmaInitSeq,
  ifc_SourceDirective_MsvcPragmaInlineDepth,
  ifc_SourceDirective_MsvcPragmaInlineRecursion,
  ifc_SourceDirective_MsvcPragmaIntrinsic,
  ifc_SourceDirective_MsvcPragmaLoop,
  ifc_SourceDirective_MsvcPragmaMakePublic,
  ifc_SourceDirective_MsvcPragmaManaged,
  ifc_SourceDirective_MsvcPragmaMessage,
  ifc_SourceDirective_MsvcPragmaOMP,
  ifc_SourceDirective_MsvcPragmaOptimize,
  ifc_SourceDirective_MsvcPragmaPack,
  ifc_SourceDirective_MsvcPragmaPointerToMembers,
  ifc_SourceDirective_MsvcPragmaPopMacro,
  ifc_SourceDirective_MsvcPragmaPrefast,
  ifc_SourceDirective_MsvcPragmaPushMacro,
  ifc_SourceDirective_MsvcPragmaRegion,
  ifc_SourceDirective_MsvcPragmaRuntimeChecks,
  ifc_SourceDirective_MsvcPragmaSameSeg,
  ifc_SourceDirective_MsvcPragmaSection,
  ifc_SourceDirective_MsvcPragmaSegment,
  ifc_SourceDirective_MsvcPragmaSetlocale,
  ifc_SourceDirective_MsvcPragmaStartMapRegion,
  ifc_SourceDirective_MsvcPragmaStopMapRegion,
  ifc_SourceDirective_MsvcPragmaStrictGSCheck,
  ifc_SourceDirective_MsvcPragmaSystemHeader,
  ifc_SourceDirective_MsvcPragmaUnmanaged,
  ifc_SourceDirective_MsvcPragmaVtordisp,
  ifc_SourceDirective_MsvcPragmaWarning,
  ifc_SourceDirective_MsvcPragmaP0include,
  ifc_SourceDirective_MsvcPragmaP0line,
};

/* Enumeration for SourcePunctuator (i.e., types of punctuation) */
enum ifc_SourcePunctuator : uint16_t {
  ifc_SourcePunctuator_Unknown,
  ifc_SourcePunctuator_LeftParenthesis,
  ifc_SourcePunctuator_RightParenthesis,
  ifc_SourcePunctuator_LeftBracket,
  ifc_SourcePunctuator_RightBracket,
  ifc_SourcePunctuator_LeftBrace,
  ifc_SourcePunctuator_RightBrace,
  ifc_SourcePunctuator_Colon,
  ifc_SourcePunctuator_Question,
  ifc_SourcePunctuator_Semicolon,
  ifc_SourcePunctuator_ColonColon,
  /* MSVC-specific */
  ifc_SourcePunctuator_Msvc = 0x1FFF,
  ifc_SourcePunctuator_MsvcZeroWidthSpace,
  ifc_SourcePunctuator_MsvcEndOfPhrase,
  ifc_SourcePunctuator_MsvcFullStop,
  ifc_SourcePunctuator_MsvcNestedTemplateStart,
  ifc_SourcePunctuator_MsvcDefaultArgumentStart,
  ifc_SourcePunctuator_MsvcAlignasEdictStart,
  ifc_SourcePunctuator_MsvcDefaultInitStart,
};

/* Enumeration for SourceLiteral (i.e., types of literals) */
enum ifc_SourceLiteral : uint16_t {
  ifc_SourceLiteral_Unknown,
  ifc_SourceLiteral_Scalar,
  ifc_SourceLiteral_String,
  ifc_SourceLiteral_DefinedString,
  /* MSVC-specific */
  ifc_SourceLiteral_Msvc = 0x1FFF,
  ifc_SourceLiteral_MsvcFunctionNameMacro,
  ifc_SourceLiteral_MsvcStringPrefixMacro,
  ifc_SourceLiteral_MsvcBinding,
};

/* Enumeration for SourceOperator (i.e., types of operators) */
enum ifc_SourceOperator : uint16_t {
  ifc_SourceOperator_Unknown,
  ifc_SourceOperator_Equal,
  ifc_SourceOperator_Comma,
  ifc_SourceOperator_Exclaim,
  ifc_SourceOperator_Plus,
  ifc_SourceOperator_Dash,
  ifc_SourceOperator_Star,
  ifc_SourceOperator_Slash,
  ifc_SourceOperator_Percent,
  ifc_SourceOperator_LeftChevron,
  ifc_SourceOperator_RightChevron,
  ifc_SourceOperator_Tilde,
  ifc_SourceOperator_Caret,
  ifc_SourceOperator_Bar,
  ifc_SourceOperator_Ampersand,
  ifc_SourceOperator_PlusPlus,
  ifc_SourceOperator_DashDash,
  ifc_SourceOperator_Less,
  ifc_SourceOperator_LessEqual,
  ifc_SourceOperator_Greater,
  ifc_SourceOperator_GreaterEqual,
  ifc_SourceOperator_EqualEqual,
  ifc_SourceOperator_ExclaimEqual,
  ifc_SourceOperator_Diamond,
  ifc_SourceOperator_PlusEqual,
  ifc_SourceOperator_DashEqual,
  ifc_SourceOperator_StarEqual,
  ifc_SourceOperator_SlashEqual,
  ifc_SourceOperator_PercentEqual,
  ifc_SourceOperator_AmpersandEqual,
  ifc_SourceOperator_BarEqual,
  ifc_SourceOperator_CaretEqual,
  ifc_SourceOperator_LeftChevronEqual,
  ifc_SourceOperator_RightChevronEqual,
  ifc_SourceOperator_AmpersandAmpersand,
  ifc_SourceOperator_BarBar,
  ifc_SourceOperator_Ellipsis,
  ifc_SourceOperator_Dot,
  ifc_SourceOperator_Arrow,
  ifc_SourceOperator_DotStar,
  ifc_SourceOperator_ArrowStar,
};

/* Enumeration for SourceKeyword (i.e., types of keywords) */
enum ifc_SourceKeyword : uint16_t {
  ifc_SourceKeyword_Unknown,
  ifc_SourceKeyword_Alignas,
  ifc_SourceKeyword_Alignof,
  ifc_SourceKeyword_Asm,
  ifc_SourceKeyword_Auto,
  ifc_SourceKeyword_Bool,
  ifc_SourceKeyword_Break,
  ifc_SourceKeyword_Case,
  ifc_SourceKeyword_Catch,
  ifc_SourceKeyword_Char,
  ifc_SourceKeyword_Char8T,
  ifc_SourceKeyword_Char16T,
  ifc_SourceKeyword_Char32T,
  ifc_SourceKeyword_Class,
  ifc_SourceKeyword_Concept,
  ifc_SourceKeyword_Const,
  ifc_SourceKeyword_Consteval,
  ifc_SourceKeyword_Constexpr,
  ifc_SourceKeyword_Constinit,
  ifc_SourceKeyword_ConstCast,
  ifc_SourceKeyword_Continue,
  ifc_SourceKeyword_CoAwait,
  ifc_SourceKeyword_CoReturn,
  ifc_SourceKeyword_CoYield,
  ifc_SourceKeyword_Decltype,
  ifc_SourceKeyword_Default,
  ifc_SourceKeyword_Delete,
  ifc_SourceKeyword_Do,
  ifc_SourceKeyword_Double,
  ifc_SourceKeyword_DynamicCast,
  ifc_SourceKeyword_Else,
  ifc_SourceKeyword_Enum,
  ifc_SourceKeyword_Explicit,
  ifc_SourceKeyword_Export,
  ifc_SourceKeyword_Extern,
  ifc_SourceKeyword_False,
  ifc_SourceKeyword_Float,
  ifc_SourceKeyword_For,
  ifc_SourceKeyword_Friend,
  ifc_SourceKeyword_Generic,
  ifc_SourceKeyword_Goto,
  ifc_SourceKeyword_If,
  ifc_SourceKeyword_Inline,
  ifc_SourceKeyword_Int,
  ifc_SourceKeyword_Long,
  ifc_SourceKeyword_Mutable,
  ifc_SourceKeyword_Namespace,
  ifc_SourceKeyword_New,
  ifc_SourceKeyword_Noexcept,
  ifc_SourceKeyword_Nullptr,
  ifc_SourceKeyword_Operator,
  ifc_SourceKeyword_Pragma,
  ifc_SourceKeyword_Private,
  ifc_SourceKeyword_Protected,
  ifc_SourceKeyword_Public,
  ifc_SourceKeyword_Register,
  ifc_SourceKeyword_ReinterpretCast,
  ifc_SourceKeyword_Requires,
  ifc_SourceKeyword_Restrict,
  ifc_SourceKeyword_Return,
  ifc_SourceKeyword_Short,
  ifc_SourceKeyword_Signed,
  ifc_SourceKeyword_Sizeof,
  ifc_SourceKeyword_Static,
  ifc_SourceKeyword_StaticAssert,
  ifc_SourceKeyword_StaticCast,
  ifc_SourceKeyword_Struct,
  ifc_SourceKeyword_Switch,
  ifc_SourceKeyword_Template,
  ifc_SourceKeyword_This,
  ifc_SourceKeyword_ThreadLocal,
  ifc_SourceKeyword_Throw,
  ifc_SourceKeyword_True,
  ifc_SourceKeyword_Try,
  ifc_SourceKeyword_Typedef,
  ifc_SourceKeyword_Typeid,
  ifc_SourceKeyword_Typename,
  ifc_SourceKeyword_Union,
  ifc_SourceKeyword_Unsigned,
  ifc_SourceKeyword_Using,
  ifc_SourceKeyword_Virtual,
  ifc_SourceKeyword_Void,
  ifc_SourceKeyword_Volatile,
  ifc_SourceKeyword_WcharT,
  ifc_SourceKeyword_While,
  /* MSVC-specific */
  ifc_SourceKeyword_Msvc = 0x1FFF,
  ifc_SourceKeyword_MsvcAsm,
  ifc_SourceKeyword_MsvcAssume,
  ifc_SourceKeyword_MsvcAlignof,
  ifc_SourceKeyword_MsvcBased,
  ifc_SourceKeyword_MsvcCdecl,
  ifc_SourceKeyword_MsvcClrcall,
  ifc_SourceKeyword_MsvcDeclspec,
  ifc_SourceKeyword_MsvcEabi,
  ifc_SourceKeyword_MsvcEvent,
  ifc_SourceKeyword_MsvcSehExcept,
  ifc_SourceKeyword_MsvcFastcall,
  ifc_SourceKeyword_MsvcSehFinally,
  ifc_SourceKeyword_MsvcForceinline,
  ifc_SourceKeyword_MsvcHook,
  ifc_SourceKeyword_MsvcIdentifier,
  ifc_SourceKeyword_MsvcIfExists,
  ifc_SourceKeyword_MsvcIfNotExists,
  ifc_SourceKeyword_MsvcInt8,
  ifc_SourceKeyword_MsvcInt16,
  ifc_SourceKeyword_MsvcInt32,
  ifc_SourceKeyword_MsvcInt64,
  ifc_SourceKeyword_MsvcInt128,
  ifc_SourceKeyword_MsvcInterface,
  ifc_SourceKeyword_MsvcLeave,
  ifc_SourceKeyword_MsvcMultipleInheritance,
  ifc_SourceKeyword_MsvcNullptr,
  ifc_SourceKeyword_MsvcNovtordisp,
  ifc_SourceKeyword_MsvcPragma,
  ifc_SourceKeyword_MsvcPtr32,
  ifc_SourceKeyword_MsvcPtr64,
  ifc_SourceKeyword_MsvcRestrict,
  ifc_SourceKeyword_MsvcSingleInheritance,
  ifc_SourceKeyword_MsvcSptr,
  ifc_SourceKeyword_MsvcStdcall,
  ifc_SourceKeyword_MsvcSuper,
  ifc_SourceKeyword_MsvcThiscall,
  ifc_SourceKeyword_MsvcSehTry,
  ifc_SourceKeyword_MsvcUptr,
  ifc_SourceKeyword_MsvcUuidof,
  ifc_SourceKeyword_MsvcUnaligned,
  ifc_SourceKeyword_MsvcUnhook,
  ifc_SourceKeyword_MsvcVectorcall,
  ifc_SourceKeyword_MsvcVirtualInheritance,
  ifc_SourceKeyword_MsvcW64,
  ifc_SourceKeyword_MsvcIsClass,
  ifc_SourceKeyword_MsvcIsUnion,
  ifc_SourceKeyword_MsvcIsEnum,
  ifc_SourceKeyword_MsvcIsPolymorphic,
  ifc_SourceKeyword_MsvcIsEmpty,
  ifc_SourceKeyword_MsvcHasTrivialConstructor,
  ifc_SourceKeyword_MsvcIsTriviallyConstructible,
  ifc_SourceKeyword_MsvcIsTriviallyCopyConstructible,
  ifc_SourceKeyword_MsvcIsTriviallyCopyAssignable,
  ifc_SourceKeyword_MsvcIsTriviallyDestructible,
  ifc_SourceKeyword_MsvcHasVirtualDestructor,
  ifc_SourceKeyword_MsvcIsNothrowConstructible,
  ifc_SourceKeyword_MsvcIsNothrowCopyConstructible,
  ifc_SourceKeyword_MsvcIsNothrowCopyAssignable,
  ifc_SourceKeyword_MsvcIsPod,
  ifc_SourceKeyword_MsvcIsAbstract,
  ifc_SourceKeyword_MsvcIsBaseOf,
  ifc_SourceKeyword_MsvcIsConvertibleto,
  ifc_SourceKeyword_MsvcIsTrivial,
  ifc_SourceKeyword_MsvcIsTriviallyCopyable,
  ifc_SourceKeyword_MsvcIsStandardLayout,
  ifc_SourceKeyword_MsvcIsLiteralType,
  ifc_SourceKeyword_MsvcIsTriviallyMoveConstructible,
  ifc_SourceKeyword_MsvcHasTrivialMoveAssign,
  ifc_SourceKeyword_MsvcIsTriviallyMoveAssignable,
  ifc_SourceKeyword_MsvcIsNothrowMoveAssignable,
  ifc_SourceKeyword_MsvcIsConstructible,
  ifc_SourceKeyword_MsvcUnderlyingType,
  ifc_SourceKeyword_MsvcIsTriviallyAssignable,
  ifc_SourceKeyword_MsvcIsNothrowAssignable,
  ifc_SourceKeyword_MsvcIsDestructible,
  ifc_SourceKeyword_MsvcIsNothrowDestructible,
  ifc_SourceKeyword_MsvcIsAssignable,
  ifc_SourceKeyword_MsvcIsAssignableNoCheck,
  ifc_SourceKeyword_MsvcHasUniqueObjectRepresentations,
  ifc_SourceKeyword_MsvcIsAggregate,
  ifc_SourceKeyword_MsvcBuiltinAddressOf,
  ifc_SourceKeyword_MsvcBuiltinOffsetOf,
  ifc_SourceKeyword_MsvcBuiltinBitCast,
  ifc_SourceKeyword_MsvcBuiltinIsLayoutCompatible,
  ifc_SourceKeyword_MsvcBuiltinIsPointerInterconvertibleBaseOf,
  ifc_SourceKeyword_MsvcBuiltinIsPointerInterconvertibleWithClass,
  ifc_SourceKeyword_MsvcBuiltinIsCorrespondingMember,
  ifc_SourceKeyword_MsvcIsRefClass,
  ifc_SourceKeyword_MsvcIsValueClass,
  ifc_SourceKeyword_MsvcIsSimpleValueClass,
  ifc_SourceKeyword_MsvcIsInterfaceClass,
  ifc_SourceKeyword_MsvcIsDelegate,
  ifc_SourceKeyword_MsvcIsFinal,
  ifc_SourceKeyword_MsvcIsSealed,
  ifc_SourceKeyword_MsvcHasFinalizer,
  ifc_SourceKeyword_MsvcHasCopy,
  ifc_SourceKeyword_MsvcHasAssign,
  ifc_SourceKeyword_MsvcHasUserDestructor,
  ifc_SourceKeyword_MsvcPackCardinality,
  ifc_SourceKeyword_MsvcConfusedSizeof,
  ifc_SourceKeyword_MsvcConfusedalignas,
};

/* Enumeration for SourceIdentifier (i.e., types of identifiers) */
enum ifc_SourceIdentifier : uint16_t {
  ifc_SourceIdentifier_Plain,
  /* MSVC-specific */
  ifc_SourceIdentifier_Msvc = 0x1FFF,
  ifc_SourceIdentifier_MsvcBuiltinHugeVal,
  ifc_SourceIdentifier_MsvcBuiltinHugeValf,
  ifc_SourceIdentifier_MsvcBuiltinNan,
  ifc_SourceIdentifier_MsvcBuiltinNanf,
  ifc_SourceIdentifier_MsvcBuiltinNans,
  ifc_SourceIdentifier_MsvcBuiltinNansf,
};

/* Macros used to access SyntaxIndex::tag and SyntaxIndex::value. */
#define syntax_tag(syn) ((ifc_SyntaxSort)((syn) & 0x0000007F))
#define syntax_value(syn) ((ifc_Index)((syn) >> 7))

/* Enumeration for SyntaxSort (i.e., kinds of syntax). */
enum ifc_SyntaxSort : ifc_Sort_type {
  ifc_SyntaxSort_VendorExtension,
             /* Vendor-specific extension for syntax. */
  ifc_SyntaxSort_SimpleTypeSpecifier,
             /* A simple type-specifier (i.e. no declarator) */
  ifc_SyntaxSort_DecltypeSpecifier,
             /* A decltype-specifier - 'decltype(expression)' */
  ifc_SyntaxSort_PlaceholderTypeSpecifier,
  ifc_SyntaxSort_TypeSpecifierSeq,
             /* A type-specifier-seq - part of a type-id */
  ifc_SyntaxSort_DeclSpecifierSeq,
             /* A decl-specifier-seq - part of a declarator */
  ifc_SyntaxSort_VirtualSpecifierSeq,
             /* A virtual-specifier-seq (includes pure-specifier) */
  ifc_SyntaxSort_NoexceptSpecification,
             /* A noexcept-specification */
  ifc_SyntaxSort_ExplicitSpecifier,
             /* An explicit-specifier */
  ifc_SyntaxSort_EnumSpecifier,
             /* An enum-specifier */
  ifc_SyntaxSort_EnumeratorDefinition,
             /* An enumerator-definition */
  ifc_SyntaxSort_ClassSpecifier,
             /* A class-specifier */
  ifc_SyntaxSort_MemberSpecification,
             /* A member-specification */
  ifc_SyntaxSort_MemberDeclaration,
             /* A member-declaration */
  ifc_SyntaxSort_MemberDeclarator,
             /* A member-declarator */
  ifc_SyntaxSort_AccessSpecifier,
             /* An access-specifier */
  ifc_SyntaxSort_BaseSpecifierList,
             /* A base-specifier-list */
  ifc_SyntaxSort_BaseSpecifier,
             /* A base-specifier */
  ifc_SyntaxSort_TypeId,
             /* A complete type used as part of an expression */
  ifc_SyntaxSort_TrailingReturnType,
             /* a trailing return type: '-> T' */
  ifc_SyntaxSort_Declarator,
            /* A declarator: i.e. something that has not (yet) been resolved */
  ifc_SyntaxSort_PointerDeclarator,
             /* A sub-declarator for a pointer: '*D' */
  ifc_SyntaxSort_ArrayDeclarator,
             /* A sub-declarator for an array: 'D[e]' */
  ifc_SyntaxSort_FunctionDeclarator,
             /* A sub-declarator for a function: 'D(T1, T2, T3) <stuff>' */
  ifc_SyntaxSort_ArrayOrFunctionDeclarator,
             /* Either an array or a function sub-declarator */
  ifc_SyntaxSort_ParameterDeclarator,
             /* A function parameter declaration */
  ifc_SyntaxSort_InitDeclarator,
             /* A declaration with an initializer */
  ifc_SyntaxSort_NewDeclarator,
             /* A new declarator (used in new expressions) */
  ifc_SyntaxSort_SimpleDeclaration,
             /* A simple-declaration */
  ifc_SyntaxSort_ExceptionDeclaration,
             /* An exception-declaration */
  ifc_SyntaxSort_ConditionDeclaration,
             /* A declaration within if or switch statement */
  ifc_SyntaxSort_StaticAssertDeclaration,
             /* A static_assert-declaration */
  ifc_SyntaxSort_AliasDeclaration,
             /* An alias-declaration */
  ifc_SyntaxSort_ConceptDefinition,
             /* A concept-definition */
  ifc_SyntaxSort_CompoundStatement,
             /* A compound statement */
  ifc_SyntaxSort_ReturnStatement,
             /* A return statement */
  ifc_SyntaxSort_IfStatement,
             /* An if statement */
  ifc_SyntaxSort_WhileStatement,
             /* A while statement */
  ifc_SyntaxSort_DoWhileStatement,
             /* A do-while statement */
  ifc_SyntaxSort_ForStatement,
             /* A for statement */
  ifc_SyntaxSort_InitStatement,
             /* An init-statement */
  ifc_SyntaxSort_RangeBasedForStatement,
             /* A range-based for statement */
  ifc_SyntaxSort_ForRangeDeclaration,
             /* A for-range-declaration */
  ifc_SyntaxSort_LabeledStatement,
             /* A labeled statement */
  ifc_SyntaxSort_BreakStatement,
             /* A break statement */
  ifc_SyntaxSort_ContinueStatement,
             /* A continue statement */
  ifc_SyntaxSort_SwitchStatement,
             /* A switch statement */
  ifc_SyntaxSort_GotoStatement,
             /* A goto statement */
  ifc_SyntaxSort_DeclarationStatement,
             /* A declaration statement */
  ifc_SyntaxSort_ExpressionStatement,
             /* An expression statement */
  ifc_SyntaxSort_TryBlock,
             /* A try block */
  ifc_SyntaxSort_Handler,
             /* A catch handler */
  ifc_SyntaxSort_HandlerSeq,
             /* A sequence of catch handlers */
  ifc_SyntaxSort_FunctionTryBlock,
             /* A function try block */
  ifc_SyntaxSort_TypeIdListElement,
             /* a type-id-list element */
  ifc_SyntaxSort_DynamicExceptionSpec,
             /* A dynamic exception specification */
  ifc_SyntaxSort_StatementSeq,
             /* A sequence of statements */
  ifc_SyntaxSort_FunctionBody,
             /* The body of a function */
  ifc_SyntaxSort_Expression,
             /* A wrapper around an ExprSort node */
  ifc_SyntaxSort_FunctionDefinition,
             /* A function-definition */
  ifc_SyntaxSort_MemberFunctionDeclaration,
             /* A member function declaration */
  ifc_SyntaxSort_TemplateDeclaration,
             /* A template head definition */
  ifc_SyntaxSort_RequiresClause,
             /* A requires clause */
  ifc_SyntaxSort_SimpleRequirement,
             /* A simple requirement */
  ifc_SyntaxSort_TypeRequirement,
             /* A type requirement */
  ifc_SyntaxSort_CompoundRequirement,
             /* A compound requirement */
  ifc_SyntaxSort_NestedRequirement,
             /* A nested requirement */
  ifc_SyntaxSort_RequirementBody,
             /* A requirement body */
  ifc_SyntaxSort_TypeTemplateParameter,
             /* A type template-parameter */
  ifc_SyntaxSort_TemplateTemplateParameter,
             /* A template template-parameter */
  ifc_SyntaxSort_TypeTemplateArgument,
             /* A type template-argument */
  ifc_SyntaxSort_NonTypeTemplateArgument,
             /* A non-type template-argument */
  ifc_SyntaxSort_TemplateParameterList,
             /* A template parameter list */
  ifc_SyntaxSort_TemplateArgumentList,
             /* A template argument list */
  ifc_SyntaxSort_TemplateId,
             /* A template-id */
  ifc_SyntaxSort_MemInitializer,
             /* A mem-initializer */
  ifc_SyntaxSort_CtorInitializer,
             /* A ctor-initializer */
  ifc_SyntaxSort_LambdaIntroducer,
             /* A lambda-introducer */
  ifc_SyntaxSort_LambdaDeclarator,
             /* A lambda-declarator */
  ifc_SyntaxSort_CaptureDefault,
             /* A capture-default */
  ifc_SyntaxSort_SimpleCapture,
             /* A simple-capture */
  ifc_SyntaxSort_InitCapture,
             /* An init-capture */
  ifc_SyntaxSort_ThisCapture,
             /* A this-capture */
  ifc_SyntaxSort_AttributedStatement,
             /* An attributed statement */
  ifc_SyntaxSort_AttributedDeclaration,
             /* An attributed declaration */
  ifc_SyntaxSort_AttributeSpecifierSeq,
             /* An attribute-specifier-seq */
  ifc_SyntaxSort_AttributeSpecifier,
             /* An attribute-specifier */
  ifc_SyntaxSort_AttributeUsingPrefix,
             /* An attribute-using-prefix */
  ifc_SyntaxSort_Attribute,
             /* An attribute */
  ifc_SyntaxSort_AttributeArgumentClause,
             /* An attribute-argument-clause */
  ifc_SyntaxSort_Alignas,
             /* An alignas( expression ) */
  ifc_SyntaxSort_UsingDeclaration,
             /* A using-declaration */
  ifc_SyntaxSort_UsingDeclarator,
             /* A using-declarator */
  ifc_SyntaxSort_UsingDirective,
             /* A using-directive */
  ifc_SyntaxSort_ArrayIndex,
             /* An array index */
  ifc_SyntaxSort_SEHTry,
             /* An SEH try-block */
  ifc_SyntaxSort_SEHExcept,
             /* An SEH except-block */
  ifc_SyntaxSort_SEHFinally,
             /* An SEH finally-block */
  ifc_SyntaxSort_SEHLeave,
             /* An SEH leave */
  ifc_SyntaxSort_TypeTraitIntrinsic,
             /* A type trait intrinsic */
  ifc_SyntaxSort_Tuple,
             /* A sequence of zero or more syntactic elements */
  ifc_SyntaxSort_AsmStatement,
             /* An __asm statement, */
  ifc_SyntaxSort_NamespaceAliasDefinition,
             /* A namespace-alias-definition */
  ifc_SyntaxSort_Super,
             /* The '__super' keyword in a qualified id */
  ifc_SyntaxSort_UnaryFoldExpression,
             /* A unary fold expression */
  ifc_SyntaxSort_BinaryFoldExpression,
             /* A binary fold expression */
  ifc_SyntaxSort_EmptyStatement,
             /* An empty statement: ';' */
  ifc_SyntaxSort_StructuredBindingDeclaration,
             /* A structured binding */
  ifc_SyntaxSort_StructuredBindingIdentifier,
             /* A structured binding identifier */
  ifc_SyntaxSort_UsingEnumDeclaration,

  /* Must be last. */
  ifc_SyntaxSort_Last
};

/* Macros used to access PragmaIndex::tag and PragmaIndex::index. */
#define pragma_tag(pragma) ((ifc_PragmaSort)((pragma) & 0x00000001))
#define pragma_index(pragma) ((ifc_Index)((pragma) >> 1))

/* Enumeration for PragmaSort (i.e., kinds of pragmas). */
enum ifc_PragmaSort : ifc_Sort_type {
  ifc_PragmaSort_VendorExtension
};

/* Macros used to access MacroIndex::tag and MacroIndex::index. */
#define macro_tag(macro) ((ifc_MacroSort)((macro) & 0x00000001))
#define macro_index(macro) ((ifc_Index)((macro) >> 1))

/* Enumeration for MacroSort (i.e., kinds of macros). */
enum ifc_MacroSort : ifc_Sort_type {
  ifc_MacroSort_ObjectLike,
  ifc_MacroSort_FunctionLike,
};

/* Macros used to access FormIndex::tag and FormIndex::index. */
#define form_tag(form) ((ifc_FormSort)((form) & 0x0000000F))
#define form_index(form) ((ifc_Index)((form) >> 4))

/* Enumeration for FormSort (i.e., kinds of forms). */
enum ifc_FormSort : ifc_Sort_type {
  ifc_FormSort_Identifier,
  ifc_FormSort_Number,
  ifc_FormSort_Character,
  ifc_FormSort_String,
  ifc_FormSort_Operator,
  ifc_FormSort_Keyword,
  ifc_FormSort_Whitespace,
  ifc_FormSort_Parameter,
  ifc_FormSort_Stringize,
  ifc_FormSort_Catenate,
  ifc_FormSort_Pragma,
  ifc_FormSort_Header,
  ifc_FormSort_Parenthesized,
  ifc_FormSort_Tuple,
  ifc_FormSort_Junk,
  /* Must be last. */
  ifc_FormSort_Last
};


/* Enumeration for KeywordSort (i.e., kinds of keywords). */
enum ifc_KeywordSort : ifc_Sort_type {
  ifc_KeywordSort_Nothing,
  ifc_KeywordSort_Class,
  ifc_KeywordSort_Struct,
  ifc_KeywordSort_Union,
  ifc_KeywordSort_Public,
  ifc_KeywordSort_Protected,
  ifc_KeywordSort_Private,
  ifc_KeywordSort_Default,
  ifc_KeywordSort_Delete,
  ifc_KeywordSort_Mutable,
  ifc_KeywordSort_Constexpr,
  ifc_KeywordSort_Consteval,
  ifc_KeywordSort_Typename,
};


/* Macros used to access FormOperator::tag and FormOperator::value. */
#define form_op_tag(form_op) ((ifc_WordSort)((form_op) & 0x00000007))
#define form_op_value(form_op) ((ifc_Index)((form_op) >> 3))
/* Note: No enumeration here as these index into WordSorts.
   Valid WordSorts here are WordSort::Operator and WordSort::Punctuator.
*/

/*
An enumeration of IFC partitions.  Note that generally the order of
partitions doesn't matter, but in cases where a "tag" is used to identify
a particular set of partitions, group those together in such a way that the
tag can be added to a base value to get the proper partition.  This is
essential when using get_tag_from_partition.
*/
/*lint -save -e488 -e641*/
enum an_ifc_partition_kind : uint32_t {
  ifc_none,
  /* Group all DeclIndex::tag partitions together. */
  ifc_decl_start,
  ifc_decl_vendor_extension = ifc_decl_start + ifc_DeclSort_VendorExtension,
  ifc_decl_enumerator = ifc_decl_start + ifc_DeclSort_Enumerator,
  ifc_decl_variable = ifc_decl_start + ifc_DeclSort_Variable,
  ifc_decl_parameter = ifc_decl_start + ifc_DeclSort_Parameter,
  ifc_decl_field = ifc_decl_start + ifc_DeclSort_Field,
  ifc_decl_bitfield = ifc_decl_start + ifc_DeclSort_Bitfield,
  ifc_decl_scope = ifc_decl_start + ifc_DeclSort_Scope,
  ifc_decl_enumeration = ifc_decl_start + ifc_DeclSort_Enumeration,
  ifc_decl_alias = ifc_decl_start + ifc_DeclSort_Alias,
  ifc_decl_temploid = ifc_decl_start + ifc_DeclSort_Temploid,
  ifc_decl_template = ifc_decl_start + ifc_DeclSort_Template,
  ifc_decl_partial_specialization = ifc_decl_start +
                                            ifc_DeclSort_PartialSpecialization,
  ifc_decl_explicit_specialization = ifc_decl_start +
                                           ifc_DeclSort_ExplicitSpecialization,
  ifc_decl_explicit_instantiation = ifc_decl_start +
                                            ifc_DeclSort_ExplicitInstantiation,
  ifc_decl_concept = ifc_decl_start + ifc_DeclSort_Concept,
  ifc_decl_function = ifc_decl_start + ifc_DeclSort_Function,
  ifc_decl_method = ifc_decl_start + ifc_DeclSort_Method,
  ifc_decl_constructor = ifc_decl_start + ifc_DeclSort_Constructor,
  ifc_decl_inh_ctor = ifc_decl_start + ifc_DeclSort_InheritedConstructor,
  ifc_decl_destructor = ifc_decl_start + ifc_DeclSort_Destructor,
  ifc_decl_reference = ifc_decl_start + ifc_DeclSort_Reference,
  ifc_decl_using_declaration = ifc_decl_start + ifc_DeclSort_UsingDeclaration,
  ifc_decl_using_directive = ifc_decl_start + ifc_DeclSort_UsingDirective,
  ifc_decl_friend = ifc_decl_start + ifc_DeclSort_Friend,
  ifc_decl_expansion = ifc_decl_start + ifc_DeclSort_Expansion,
  ifc_decl_deduction_guide = ifc_decl_start + ifc_DeclSort_DeductionGuide,
  ifc_decl_barren = ifc_decl_start + ifc_DeclSort_Barren,
  ifc_decl_tuple = ifc_decl_start + ifc_DeclSort_Tuple,
  ifc_decl_syntax_tree = ifc_decl_start + ifc_DeclSort_SyntaxTree,
  ifc_decl_intrinsic = ifc_decl_start + ifc_DeclSort_Intrinsic,
  ifc_decl_property = ifc_decl_start + ifc_DeclSort_Property,
  ifc_decl_segment = ifc_decl_start + ifc_DeclSort_OutputSegment,
  ifc_decl_end = ifc_decl_start + (ifc_DeclSort_Last-1),
  /* Group all TypeIndex::tag partitions together. */
  ifc_type_start,
  ifc_type_vendor_extension = ifc_type_start + ifc_TypeSort_VendorExtension,
  ifc_type_fundamental = ifc_type_start + ifc_TypeSort_Fundamental,
  ifc_type_designated = ifc_type_start + ifc_TypeSort_Designated,
  ifc_type_tor = ifc_type_start + ifc_TypeSort_Tor,
  ifc_type_syntactic = ifc_type_start + ifc_TypeSort_Syntactic,
  ifc_type_expansion = ifc_type_start + ifc_TypeSort_Expansion,
  ifc_type_pointer = ifc_type_start + ifc_TypeSort_Pointer,
  ifc_type_pointer_to_member = ifc_type_start + ifc_TypeSort_PointerToMember,
  ifc_type_lvalue_reference = ifc_type_start + ifc_TypeSort_LvalueReference,
  ifc_type_rvalue_reference = ifc_type_start + ifc_TypeSort_RvalueReference,
  ifc_type_function = ifc_type_start + ifc_TypeSort_Function,
  ifc_type_method = ifc_type_start + ifc_TypeSort_Method,
  ifc_type_array = ifc_type_start + ifc_TypeSort_Array,
  ifc_type_typename = ifc_type_start + ifc_TypeSort_Typename,
  ifc_type_qualified = ifc_type_start + ifc_TypeSort_Qualified,
  ifc_type_base = ifc_type_start + ifc_TypeSort_Base,
  ifc_type_decltype = ifc_type_start + ifc_TypeSort_Decltype,
  ifc_type_placeholder = ifc_type_start + ifc_TypeSort_Placeholder,
  ifc_type_tuple = ifc_type_start + ifc_TypeSort_Tuple,
  ifc_type_forall = ifc_type_start + ifc_TypeSort_Forall,
  ifc_type_unaligned = ifc_type_start + ifc_TypeSort_Unaligned,
  ifc_type_syntax_tree = ifc_type_start + ifc_TypeSort_SyntaxTree,
  ifc_type_end = ifc_type_start + (ifc_TypeSort_Last-1),
  /* Group all NameIndex::tag partitions together. */
  ifc_name_start,
  ifc_name_identifier = ifc_name_start + ifc_NameSort_Identifier,
  ifc_name_operator = ifc_name_start + ifc_NameSort_Operator,
  ifc_name_conversion = ifc_name_start + ifc_NameSort_Conversion,
  ifc_name_literal = ifc_name_start + ifc_NameSort_Literal,
  ifc_name_template = ifc_name_start + ifc_NameSort_Template,
  ifc_name_specialization = ifc_name_start + ifc_NameSort_Specialization,
  ifc_name_source_file = ifc_name_start + ifc_NameSort_SourceFile,
  ifc_name_guide = ifc_name_start + ifc_NameSort_Guide,
  ifc_name_end = ifc_name_start + (ifc_NameSort_Last-1),
  /* Group all ExprIndex::tag partitions together. */
  ifc_expr_start,
  ifc_expr_vendor_extension = ifc_expr_start + ifc_ExprSort_VendorExtension,
  ifc_expr_empty = ifc_expr_start + ifc_ExprSort_Empty,
  ifc_expr_literal = ifc_expr_start + ifc_ExprSort_Literal,
  ifc_expr_lambda = ifc_expr_start + ifc_ExprSort_Lambda,
  ifc_expr_type = ifc_expr_start + ifc_ExprSort_Type,
  ifc_expr_decl = ifc_expr_start + ifc_ExprSort_NamedDecl,
  ifc_expr_unresolved_id = ifc_expr_start + ifc_ExprSort_UnresolvedId,
  ifc_expr_template_id = ifc_expr_start + ifc_ExprSort_TemplateId,
  ifc_expr_unqualified_id = ifc_expr_start + ifc_ExprSort_UnqualifiedId,
  ifc_expr_simple_identifier = ifc_expr_start + ifc_ExprSort_SimpleIdentifier,
  ifc_expr_pointer = ifc_expr_start + ifc_ExprSort_Pointer,
  ifc_expr_qualified_name = ifc_expr_start + ifc_ExprSort_QualifiedName,
  ifc_expr_path = ifc_expr_start + ifc_ExprSort_Path,
  ifc_expr_read = ifc_expr_start + ifc_ExprSort_Read,
  ifc_expr_monad = ifc_expr_start + ifc_ExprSort_Monad,
  ifc_expr_dyad = ifc_expr_start + ifc_ExprSort_Dyad,
  ifc_expr_triad = ifc_expr_start + ifc_ExprSort_Triad,
  ifc_expr_string = ifc_expr_start + ifc_ExprSort_String,
  ifc_expr_temporary = ifc_expr_start + ifc_ExprSort_Temporary,
  ifc_expr_call = ifc_expr_start + ifc_ExprSort_Call,
  ifc_expr_member_initializer = ifc_expr_start +
                                                ifc_ExprSort_MemberInitializer,
  ifc_expr_member_access = ifc_expr_start + ifc_ExprSort_MemberAccess,
  ifc_expr_inheritance_path = ifc_expr_start + ifc_ExprSort_InheritancePath,
  ifc_expr_initializer_list = ifc_expr_start + ifc_ExprSort_InitializerList,
  ifc_expr_cast = ifc_expr_start + ifc_ExprSort_Cast,
  ifc_expr_condition = ifc_expr_start + ifc_ExprSort_Condition,
  ifc_expr_expression_list = ifc_expr_start + ifc_ExprSort_ExpressionList,
  ifc_expr_sizeof_type = ifc_expr_start + ifc_ExprSort_SizeofType,
  ifc_expr_alignof_type = ifc_expr_start + ifc_ExprSort_Alignof,
  ifc_expr_new = ifc_expr_start + ifc_ExprSort_New,
  ifc_expr_delete = ifc_expr_start + ifc_ExprSort_Delete,
  ifc_expr_typeid = ifc_expr_start + ifc_ExprSort_Typeid,
  ifc_expr_destructor_call = ifc_expr_start + ifc_ExprSort_DestructorCall,
  ifc_expr_syntax_tree = ifc_expr_start + ifc_ExprSort_SyntaxTree,
  ifc_expr_function_string = ifc_expr_start + ifc_ExprSort_FunctionString,
  ifc_expr_compound_string = ifc_expr_start + ifc_ExprSort_CompoundString,
  ifc_expr_string_sequence = ifc_expr_start + ifc_ExprSort_StringSequence,
  ifc_expr_initializer = ifc_expr_start + ifc_ExprSort_Initializer,
  ifc_expr_requires = ifc_expr_start + ifc_ExprSort_Requires,
  ifc_expr_unaryfold = ifc_expr_start + ifc_ExprSort_UnaryFold,
  ifc_expr_binaryfold = ifc_expr_start + ifc_ExprSort_BinaryFold,
  ifc_expr_hierarchy_conversion = ifc_expr_start +
                                              ifc_ExprSort_HierarchyConversion,
  ifc_expr_product = ifc_expr_start + ifc_ExprSort_ProductTypeValue,
  ifc_expr_sum = ifc_expr_start + ifc_ExprSort_SumTypeValue,
  ifc_expr_subobject = ifc_expr_start + ifc_ExprSort_SubobjectValue,
  ifc_expr_array = ifc_expr_start + ifc_ExprSort_ArrayValue,
  ifc_expr_dynamic_dispatch = ifc_expr_start + ifc_ExprSort_DynamicDispatch,
  ifc_expr_virtual_function = ifc_expr_start +
                                        ifc_ExprSort_VirtualFunctionConversion,
  ifc_expr_placeholder = ifc_expr_start + ifc_ExprSort_Placeholder,
  ifc_expr_expansion = ifc_expr_start + ifc_ExprSort_Expansion,
  ifc_expr_generic = ifc_expr_start + ifc_ExprSort_Generic,
  ifc_expr_tuple = ifc_expr_start + ifc_ExprSort_Tuple,
  ifc_expr_nullptr = ifc_expr_start + ifc_ExprSort_Nullptr,
  ifc_expr_this = ifc_expr_start + ifc_ExprSort_This,
  ifc_expr_template_reference = ifc_expr_start +
                                                ifc_ExprSort_TemplateReference,
  ifc_expr_push_state = ifc_expr_start + ifc_ExprSort_PushState,
  ifc_expr_type_trait = ifc_expr_start + ifc_ExprSort_TypeTraitIntrinsic,
  ifc_expr_des_init = ifc_expr_start + ifc_ExprSort_DesignatedInitializer,
  ifc_expr_packed_template_arguments = ifc_expr_start +
                                          ifc_ExprSort_PackedTemplateArguments,
  ifc_expr_tokens = ifc_expr_start + ifc_ExprSort_Tokens,
  ifc_expr_assign_initializer = ifc_expr_start +
                                                ifc_ExprSort_AssignInitializer,
  ifc_expr_end = ifc_expr_start + (ifc_ExprSort_Last-1),
  /* Group all StmtIndex::Tag partitions together. */
  ifc_stmt_start,
  ifc_stmt_vendor_extension = ifc_stmt_start + ifc_StmtSort_VendorExtension,
  ifc_stmt_empty = ifc_stmt_start + ifc_StmtSort_Empty,
  ifc_stmt_if = ifc_stmt_start + ifc_StmtSort_If,
  ifc_stmt_for = ifc_stmt_start + ifc_StmtSort_For,
  ifc_stmt_case = ifc_stmt_start + ifc_StmtSort_Case,
  ifc_stmt_while = ifc_stmt_start + ifc_StmtSort_While,
  ifc_stmt_block = ifc_stmt_start + ifc_StmtSort_Block,
  ifc_stmt_break = ifc_stmt_start + ifc_StmtSort_Break,
  ifc_stmt_switch = ifc_stmt_start + ifc_StmtSort_Switch,
  ifc_stmt_do_while = ifc_stmt_start + ifc_StmtSort_DoWhile,
  ifc_stmt_default = ifc_stmt_start + ifc_StmtSort_Default,
  ifc_stmt_continue = ifc_stmt_start + ifc_StmtSort_Continue,
  ifc_stmt_expression = ifc_stmt_start + ifc_StmtSort_Expression,
  ifc_stmt_return = ifc_stmt_start + ifc_StmtSort_Return,
  ifc_stmt_variable = ifc_stmt_start + ifc_StmtSort_VariableDecl,
  ifc_stmt_expansion = ifc_stmt_start + ifc_StmtSort_Expansion,
  ifc_stmt_syntax_tree = ifc_stmt_start + ifc_StmtSort_SyntaxTree,
  ifc_stmt_end = ifc_stmt_start + (ifc_StmtSort_Last-1),
  /* Group all ChartIndex::Tag partitions together. */
  ifc_chart_start,
  ifc_chart_none = ifc_chart_start + ifc_ChartSort_None,
  ifc_chart_unilevel = ifc_chart_start + ifc_ChartSort_Unilevel,
  ifc_chart_multilevel = ifc_chart_start + ifc_ChartSort_Multilevel,
  ifc_chart_end = ifc_chart_start + (ifc_ChartSort_Last-1),
  /* Group all AttrIndex::Tag partitions together. */
  ifc_attr_start,
  ifc_attr_nothing = ifc_attr_start + ifc_AttrSort_Nothing,
  ifc_attr_basic = ifc_attr_start + ifc_AttrSort_Basic,
  ifc_attr_scoped = ifc_attr_start + ifc_AttrSort_Scoped,
  ifc_attr_labeled = ifc_attr_start + ifc_AttrSort_Labeled,
  ifc_attr_called = ifc_attr_start + ifc_AttrSort_Called,
  ifc_attr_expanded = ifc_attr_start + ifc_AttrSort_Expanded,
  ifc_attr_factored = ifc_attr_start + ifc_AttrSort_Factored,
  ifc_attr_elaborated = ifc_attr_start + ifc_AttrSort_Elaborated,
  ifc_attr_tuple = ifc_attr_start + ifc_AttrSort_Tuple,
  ifc_attr_end = ifc_attr_start + (ifc_AttrSort_Last-1),
  /* Group all SyntaxIndex::Tag partitions together. */
  ifc_syntax_start,
  ifc_syntax_vendor_extension = ifc_syntax_start +
                                                ifc_SyntaxSort_VendorExtension,
  ifc_syntax_simple_type_specifier = ifc_syntax_start +
                                            ifc_SyntaxSort_SimpleTypeSpecifier,
  ifc_syntax_decltype_specifier = ifc_syntax_start +
                                              ifc_SyntaxSort_DecltypeSpecifier,
  ifc_syntax_placeholder_type_specifier = ifc_syntax_start +
                                       ifc_SyntaxSort_PlaceholderTypeSpecifier,
  ifc_syntax_type_specifier_seq = ifc_syntax_start +
                                               ifc_SyntaxSort_TypeSpecifierSeq,
  ifc_syntax_decl_specifier_seq = ifc_syntax_start +
                                               ifc_SyntaxSort_DeclSpecifierSeq,
  ifc_syntax_virtual_specifier_seq = ifc_syntax_start +
                                            ifc_SyntaxSort_VirtualSpecifierSeq,
  ifc_syntax_noexcept_specification = ifc_syntax_start +
                                          ifc_SyntaxSort_NoexceptSpecification,
  ifc_syntax_explicit_specifier = ifc_syntax_start +
                                              ifc_SyntaxSort_ExplicitSpecifier,
  ifc_syntax_enum_specifier = ifc_syntax_start + ifc_SyntaxSort_EnumSpecifier,
  ifc_syntax_enumerator_definition = ifc_syntax_start +
                                           ifc_SyntaxSort_EnumeratorDefinition,
  ifc_syntax_class_specifier = ifc_syntax_start +
                                                 ifc_SyntaxSort_ClassSpecifier,
  ifc_syntax_member_specification = ifc_syntax_start +
                                            ifc_SyntaxSort_MemberSpecification,
  ifc_syntax_member_declaration = ifc_syntax_start +
                                              ifc_SyntaxSort_MemberDeclaration,
  ifc_syntax_member_declarator = ifc_syntax_start +
                                               ifc_SyntaxSort_MemberDeclarator,
  ifc_syntax_access_specifier = ifc_syntax_start +
                                                ifc_SyntaxSort_AccessSpecifier,
  ifc_syntax_base_specifier_list = ifc_syntax_start +
                                              ifc_SyntaxSort_BaseSpecifierList,
  ifc_syntax_base_specifier = ifc_syntax_start + ifc_SyntaxSort_BaseSpecifier,
  ifc_syntax_type_id = ifc_syntax_start + ifc_SyntaxSort_TypeId,
  ifc_syntax_trailing_return_type = ifc_syntax_start +
                                             ifc_SyntaxSort_TrailingReturnType,
  ifc_syntax_declarator = ifc_syntax_start + ifc_SyntaxSort_Declarator,
  ifc_syntax_pointer_declarator = ifc_syntax_start +
                                              ifc_SyntaxSort_PointerDeclarator,
  ifc_syntax_array_declarator = ifc_syntax_start +
                                                ifc_SyntaxSort_ArrayDeclarator,
  ifc_syntax_function_declarator = ifc_syntax_start +
                                             ifc_SyntaxSort_FunctionDeclarator,
  ifc_syntax_array_or_function_declarator = ifc_syntax_start +
                                      ifc_SyntaxSort_ArrayOrFunctionDeclarator,
  ifc_syntax_parameter_declarator = ifc_syntax_start +
                                            ifc_SyntaxSort_ParameterDeclarator,
  ifc_syntax_init_declarator = ifc_syntax_start +
                                                 ifc_SyntaxSort_InitDeclarator,
  ifc_syntax_new_declarator = ifc_syntax_start + ifc_SyntaxSort_NewDeclarator,
  ifc_syntax_simple_declaration = ifc_syntax_start +
                                              ifc_SyntaxSort_SimpleDeclaration,
  ifc_syntax_exception_declaration = ifc_syntax_start +
                                           ifc_SyntaxSort_ExceptionDeclaration,
  ifc_syntax_condition_declaration = ifc_syntax_start +
                                           ifc_SyntaxSort_ConditionDeclaration,
  ifc_syntax_static_assert_declaration = ifc_syntax_start +
                                        ifc_SyntaxSort_StaticAssertDeclaration,
  ifc_syntax_alias_declaration = ifc_syntax_start +
                                               ifc_SyntaxSort_AliasDeclaration,
  ifc_syntax_concept_definition = ifc_syntax_start +
                                              ifc_SyntaxSort_ConceptDefinition,
  ifc_syntax_compound_statement = ifc_syntax_start +
                                              ifc_SyntaxSort_CompoundStatement,
  ifc_syntax_return_statement = ifc_syntax_start +
                                                ifc_SyntaxSort_ReturnStatement,
  ifc_syntax_if_statement = ifc_syntax_start + ifc_SyntaxSort_IfStatement,
  ifc_syntax_while_statement = ifc_syntax_start +
                                                 ifc_SyntaxSort_WhileStatement,
  ifc_syntax_do_statement = ifc_syntax_start + ifc_SyntaxSort_DoWhileStatement,
  ifc_syntax_for_statement = ifc_syntax_start + ifc_SyntaxSort_ForStatement,
  ifc_syntax_init_statement = ifc_syntax_start + ifc_SyntaxSort_InitStatement,
  ifc_syntax_range_based_for_statement = ifc_syntax_start +
                                         ifc_SyntaxSort_RangeBasedForStatement,
  ifc_syntax_for_range_declaration = ifc_syntax_start +
                                            ifc_SyntaxSort_ForRangeDeclaration,
  ifc_syntax_labeled_statement = ifc_syntax_start +
                                               ifc_SyntaxSort_LabeledStatement,
  ifc_syntax_break_statement = ifc_syntax_start +
                                                 ifc_SyntaxSort_BreakStatement,
  ifc_syntax_continue_statement = ifc_syntax_start +
                                              ifc_SyntaxSort_ContinueStatement,
  ifc_syntax_switch_statement = ifc_syntax_start +
                                                ifc_SyntaxSort_SwitchStatement,
  ifc_syntax_goto_statement = ifc_syntax_start + ifc_SyntaxSort_GotoStatement,
  ifc_syntax_declaration_statement = ifc_syntax_start +
                                           ifc_SyntaxSort_DeclarationStatement,
  ifc_syntax_expression_statement = ifc_syntax_start +
                                            ifc_SyntaxSort_ExpressionStatement,
  ifc_syntax_try_block = ifc_syntax_start + ifc_SyntaxSort_TryBlock,
  ifc_syntax_handler = ifc_syntax_start + ifc_SyntaxSort_Handler,
  ifc_syntax_handler_seq = ifc_syntax_start + ifc_SyntaxSort_HandlerSeq,
  ifc_syntax_function_try_block = ifc_syntax_start +
                                               ifc_SyntaxSort_FunctionTryBlock,
  ifc_syntax_type_id_list_element = ifc_syntax_start +
                                              ifc_SyntaxSort_TypeIdListElement,
  ifc_syntax_dynamic_exception_spec = ifc_syntax_start +
                                           ifc_SyntaxSort_DynamicExceptionSpec,
  ifc_syntax_statement_seq = ifc_syntax_start + ifc_SyntaxSort_StatementSeq,
  ifc_syntax_function_body = ifc_syntax_start + ifc_SyntaxSort_FunctionBody,
  ifc_syntax_expression = ifc_syntax_start + ifc_SyntaxSort_Expression,
  ifc_syntax_function_definition = ifc_syntax_start +
                                             ifc_SyntaxSort_FunctionDefinition,
  ifc_syntax_member_function_declaration = ifc_syntax_start +
                                      ifc_SyntaxSort_MemberFunctionDeclaration,
  ifc_syntax_template_declaration = ifc_syntax_start +
                                            ifc_SyntaxSort_TemplateDeclaration,
  ifc_syntax_requires_clause = ifc_syntax_start +
                                                 ifc_SyntaxSort_RequiresClause,
  ifc_syntax_simple_requirement = ifc_syntax_start +
                                              ifc_SyntaxSort_SimpleRequirement,
  ifc_syntax_type_requirement = ifc_syntax_start +
                                                ifc_SyntaxSort_TypeRequirement,
  ifc_syntax_compound_requirement = ifc_syntax_start +
                                            ifc_SyntaxSort_CompoundRequirement,
  ifc_syntax_nested_requirement = ifc_syntax_start +
                                              ifc_SyntaxSort_NestedRequirement,
  ifc_syntax_requirement_body = ifc_syntax_start +
                                                ifc_SyntaxSort_RequirementBody,
  ifc_syntax_type_template_parameter = ifc_syntax_start +
                                          ifc_SyntaxSort_TypeTemplateParameter,
  ifc_syntax_template_template_parameter = ifc_syntax_start +
                                      ifc_SyntaxSort_TemplateTemplateParameter,
  ifc_syntax_type_template_argument = ifc_syntax_start +
                                           ifc_SyntaxSort_TypeTemplateArgument,
  ifc_syntax_non_type_template_argument = ifc_syntax_start +
                                        ifc_SyntaxSort_NonTypeTemplateArgument,
  ifc_syntax_template_parameter_list = ifc_syntax_start +
                                          ifc_SyntaxSort_TemplateParameterList,
  ifc_syntax_template_argument_list = ifc_syntax_start +
                                           ifc_SyntaxSort_TemplateArgumentList,
  ifc_syntax_template_id = ifc_syntax_start + ifc_SyntaxSort_TemplateId,
  ifc_syntax_mem_initializer = ifc_syntax_start +
                                                 ifc_SyntaxSort_MemInitializer,
  ifc_syntax_ctor_initializer = ifc_syntax_start +
                                                ifc_SyntaxSort_CtorInitializer,
  ifc_syntax_lambda_introducer = ifc_syntax_start +
                                               ifc_SyntaxSort_LambdaIntroducer,
  ifc_syntax_lambda_declarator = ifc_syntax_start +
                                               ifc_SyntaxSort_LambdaDeclarator,
  ifc_syntax_capture_default = ifc_syntax_start +
                                                 ifc_SyntaxSort_CaptureDefault,
  ifc_syntax_simple_capture = ifc_syntax_start +
                                                  ifc_SyntaxSort_SimpleCapture,
  ifc_syntax_init_capture = ifc_syntax_start + ifc_SyntaxSort_InitCapture,
  ifc_syntax_this_capture = ifc_syntax_start + ifc_SyntaxSort_ThisCapture,
  ifc_syntax_attributed_statement = ifc_syntax_start +
                                            ifc_SyntaxSort_AttributedStatement,
  ifc_syntax_attributed_declaration = ifc_syntax_start +
                                          ifc_SyntaxSort_AttributedDeclaration,
  ifc_syntax_attribute_specifier_seq = ifc_syntax_start +
                                          ifc_SyntaxSort_AttributeSpecifierSeq,
  ifc_syntax_attribute_specifier = ifc_syntax_start +
                                             ifc_SyntaxSort_AttributeSpecifier,
  ifc_syntax_attribute_using_prefix = ifc_syntax_start +
                                           ifc_SyntaxSort_AttributeUsingPrefix,
  ifc_syntax_attribute = ifc_syntax_start + ifc_SyntaxSort_Attribute,
  ifc_syntax_attribute_argument_clause = ifc_syntax_start +
                                        ifc_SyntaxSort_AttributeArgumentClause,
  ifc_syntax_alignas = ifc_syntax_start + ifc_SyntaxSort_Alignas,
  ifc_syntax_using_declaration = ifc_syntax_start +
                                               ifc_SyntaxSort_UsingDeclaration,
  ifc_syntax_using_declarator = ifc_syntax_start +
                                                ifc_SyntaxSort_UsingDeclarator,
  ifc_syntax_using_directive = ifc_syntax_start +
                                                 ifc_SyntaxSort_UsingDirective,
  ifc_syntax_array_index = ifc_syntax_start + ifc_SyntaxSort_ArrayIndex,
  ifc_syntax_seh_try = ifc_syntax_start + ifc_SyntaxSort_SEHTry,
  ifc_syntax_seh_except = ifc_syntax_start + ifc_SyntaxSort_SEHExcept,
  ifc_syntax_seh_finally = ifc_syntax_start + ifc_SyntaxSort_SEHFinally,
  ifc_syntax_seh_leave = ifc_syntax_start + ifc_SyntaxSort_SEHLeave,
  ifc_syntax_type_trait_intrinsic = ifc_syntax_start +
                                             ifc_SyntaxSort_TypeTraitIntrinsic,
  ifc_syntax_tuple = ifc_syntax_start + ifc_SyntaxSort_Tuple,
  ifc_syntax_asm_statement = ifc_syntax_start + ifc_SyntaxSort_AsmStatement,
  ifc_syntax_namespace_alias_definition = ifc_syntax_start +
                                       ifc_SyntaxSort_NamespaceAliasDefinition,
  ifc_syntax_super = ifc_syntax_start + ifc_SyntaxSort_Super,
  ifc_syntax_unary_fold_expression = ifc_syntax_start +
                                            ifc_SyntaxSort_UnaryFoldExpression,
  ifc_syntax_binary_fold_expression = ifc_syntax_start +
                                           ifc_SyntaxSort_BinaryFoldExpression,
  ifc_syntax_empty_statement = ifc_syntax_start +
                                                 ifc_SyntaxSort_EmptyStatement,
  ifc_syntax_structured_binding_declaration = ifc_syntax_start +
                                   ifc_SyntaxSort_StructuredBindingDeclaration,
  ifc_syntax_structured_binding_identifier = ifc_syntax_start +
                                    ifc_SyntaxSort_StructuredBindingIdentifier,
  ifc_syntax_using_enum_decl = ifc_syntax_start +
                                           ifc_SyntaxSort_UsingEnumDeclaration,
  ifc_syntax_end = ifc_syntax_start + (ifc_SyntaxSort_Last-1),
  /* Group all MacroIndex::Tag partitions together. */
  ifc_macro_obj_like,
  ifc_macro_func_like,
  /* Group all FormIndex::Tag partitions together. */
  ifc_form_start,
  ifc_form_ident = ifc_form_start + ifc_FormSort_Identifier,
  ifc_form_number = ifc_form_start + ifc_FormSort_Number,
  ifc_form_char = ifc_form_start + ifc_FormSort_Character,
  ifc_form_string = ifc_form_start + ifc_FormSort_String,
  ifc_form_operator = ifc_form_start + ifc_FormSort_Operator,
  ifc_form_keyword = ifc_form_start + ifc_FormSort_Keyword,
  ifc_form_whitespace = ifc_form_start + ifc_FormSort_Whitespace,
  ifc_form_param = ifc_form_start + ifc_FormSort_Parameter,
  ifc_form_stringize = ifc_form_start + ifc_FormSort_Stringize,
  ifc_form_catenate = ifc_form_start + ifc_FormSort_Catenate,
  ifc_form_pragma = ifc_form_start + ifc_FormSort_Pragma,
  ifc_form_header = ifc_form_start + ifc_FormSort_Header,
  ifc_form_parenthesized = ifc_form_start + ifc_FormSort_Parenthesized,
  ifc_form_tuple = ifc_form_start + ifc_FormSort_Tuple,
  ifc_form_junk = ifc_form_start + ifc_FormSort_Junk,
  ifc_form_end = ifc_form_start + (ifc_FormSort_Last-1),
  /* No particular grouping. */
  ifc_cmd_line,
  ifc_const_f64,
  ifc_const_i64,
  ifc_const_str,
  ifc_form_spec,
  ifc_heap_attr,
  ifc_heap_chart,
  ifc_heap_decl,
  ifc_heap_expr,
  ifc_heap_form,
  ifc_heap_pp,
  ifc_heap_stmt,
  ifc_heap_syn,
  ifc_heap_type,
  ifc_msvc_trait_code_segment,
  ifc_msvc_trait_codegen_expr_trees,
  ifc_msvc_trait_decl_attrs,
  ifc_msvc_trait_entity_init_locus,
  ifc_msvc_trait_impl_pragmas,
  ifc_msvc_trait_named_func_params,
  ifc_msvc_trait_spec_encodings,
  ifc_msvc_trait_suppressed_warnings,
  ifc_msvc_trait_templ_templ_param_classes,
  ifc_msvc_trait_uuid,
  ifc_msvc_trait_vendor_traits,
  ifc_module_exported,
  ifc_module_imported,
  ifc_pragma_state,
  ifc_pragma_vendorext,
  ifc_scope_desc,
  ifc_scope_member,
  ifc_sentence,
  ifc_src_line,
  ifc_trait_alias_template,
  ifc_trait_attribute,
  ifc_trait_deduction_guides,
  ifc_trait_deprecated,
  ifc_trait_friend,
  ifc_trait_function_definition,
  ifc_trait_requires,
  ifc_trait_specialization,
  ifc_word,
  ifc_last
};  /* an_ifc_partition_kind */
/*lint -restore*/


/*
A method for mapping partition names to an_ifc_partition_kind values.  Used
when reading an IFC file to identify which partitions are which.  The list
should be sorted by the partition name.
*/
struct an_ifc_partition_map {
  a_const_char	*name;	/* The name of an IFC partition. */
  an_ifc_partition_kind
		kind;	/* The internal representation of the partition. */
};  /* an_ifc_partition_map */

/*lint -save -e641*/
EXTERN an_ifc_partition_map ifc_partition_map[(int)ifc_last+1]
#if VAR_INITIALIZERS
= {
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.code-segment",        ifc_msvc_trait_code_segment },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.codegen-expression-trees",
                                       ifc_msvc_trait_codegen_expr_trees },
  { ".msvc.trait.decl-attrs",          ifc_msvc_trait_decl_attrs },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.entity-initializer-locus", ifc_msvc_trait_entity_init_locus },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.impl-pragmas",        ifc_msvc_trait_impl_pragmas },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.named-function-parameters",
                                       ifc_msvc_trait_named_func_params },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.specialization-encodings",
                                       ifc_msvc_trait_spec_encodings },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.suppressed-warnings", ifc_msvc_trait_suppressed_warnings },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.template-template-parameter-classes",
                                    ifc_msvc_trait_templ_templ_param_classes },
  { ".msvc.trait.uuid",                ifc_msvc_trait_uuid },
  { ".msvc.trait.vendor-traits",       ifc_msvc_trait_vendor_traits },
  { "attr.basic",                      ifc_attr_basic },
  { "attr.called",                     ifc_attr_called },
  { "attr.elaborated",                 ifc_attr_elaborated },
  { "attr.expanded",                   ifc_attr_expanded },
  { "attr.factored",                   ifc_attr_factored },
  { "attr.labeled",                    ifc_attr_labeled },
  { "attr.scoped",                     ifc_attr_scoped },
  { "attr.tuple",                      ifc_attr_tuple },
  { "chart.multilevel",                ifc_chart_multilevel },
  { "chart.none",                      ifc_chart_none },
  { "chart.unilevel",                  ifc_chart_unilevel },
  /* Not mentioned in spec.  Found in IFC files. */
  { "command_line",                    ifc_cmd_line },
  { "const.f64",                       ifc_const_f64 },
  { "const.i64",                       ifc_const_i64 },
  { "const.str",                       ifc_const_str },
  { "decl.alias",                      ifc_decl_alias },
  { "decl.barren",                     ifc_decl_barren },
  { "decl.bitfield",                   ifc_decl_bitfield },
  { "decl.concept",                    ifc_decl_concept },
  { "decl.constructor",                ifc_decl_constructor },
  { "decl.deduction-guide",            ifc_decl_deduction_guide },
  { "decl.destructor",                 ifc_decl_destructor },
  { "decl.enum",                       ifc_decl_enumeration },
  { "decl.enumerator",                 ifc_decl_enumerator },
  { "decl.expansion",                  ifc_decl_expansion },
  { "decl.explicit-instantiation",     ifc_decl_explicit_instantiation },
  { "decl.explicit-specialization",    ifc_decl_explicit_specialization },
  { "decl.field",                      ifc_decl_field },
  { "decl.friend",                     ifc_decl_friend },
  { "decl.function",                   ifc_decl_function },
  { "decl.inherited-constructor",      ifc_decl_inh_ctor },
  { "decl.intrinsic",                  ifc_decl_intrinsic },
  { "decl.method",                     ifc_decl_method },
  { "decl.parameter",                  ifc_decl_parameter },
  { "decl.partial-specialization",     ifc_decl_partial_specialization },
  { "decl.property",                   ifc_decl_property },
  { "decl.reference",                  ifc_decl_reference },
  { "decl.scope",                      ifc_decl_scope },
  { "decl.segment",                    ifc_decl_segment },
  { "decl.syntax-tree",                ifc_decl_syntax_tree },
  { "decl.template",                   ifc_decl_template },
  { "decl.temploid",                   ifc_decl_temploid },
  { "decl.tuple",                      ifc_decl_tuple },
  { "decl.using-declaration",          ifc_decl_using_declaration },
  { "decl.using-directive",            ifc_decl_using_directive },
  { "decl.variable",                   ifc_decl_variable },
  { "decl.vendor-extension",           ifc_decl_vendor_extension },
  { "expr.alignof-type-id",            ifc_expr_alignof_type },
  { "expr.array-value",                ifc_expr_array },
  { "expr.assign-initializer",         ifc_expr_assign_initializer },
  { "expr.binary-fold",                ifc_expr_binaryfold },
  { "expr.call",                       ifc_expr_call },
  { "expr.cast",                       ifc_expr_cast },
  { "expr.class-subobject-value",      ifc_expr_subobject },
  { "expr.compound-string",            ifc_expr_compound_string },
  { "expr.condition",                  ifc_expr_condition },
  { "expr.decl",                       ifc_expr_decl },
  { "expr.delete",                     ifc_expr_delete },
  { "expr.designated-init",            ifc_expr_des_init },
  { "expr.destructor-call",            ifc_expr_destructor_call },
  { "expr.dyad",                       ifc_expr_dyad },
  { "expr.dynamic-dispatch",           ifc_expr_dynamic_dispatch },
  { "expr.empty",                      ifc_expr_empty },
  { "expr.expansion",                  ifc_expr_expansion },
  { "expr.expression-list",            ifc_expr_expression_list },
  { "expr.function-string",            ifc_expr_function_string },
  { "expr.generic",                    ifc_expr_generic },
  { "expr.hierarchy-conversion",       ifc_expr_hierarchy_conversion },
  { "expr.inheritance-path",           ifc_expr_inheritance_path },
  { "expr.initializer",                ifc_expr_initializer },
  { "expr.initializer-list",           ifc_expr_initializer_list },
  { "expr.lambda",                     ifc_expr_lambda },
  { "expr.literal",                    ifc_expr_literal },
  { "expr.member-access",              ifc_expr_member_access },
  { "expr.member-initializer",         ifc_expr_member_initializer },
  { "expr.monad",                      ifc_expr_monad },
  { "expr.new",                        ifc_expr_new },
  { "expr.nullptr",                    ifc_expr_nullptr },
  { "expr.packed-template-arguments",  ifc_expr_packed_template_arguments },
  { "expr.path",                       ifc_expr_path },
  { "expr.placeholder",                ifc_expr_placeholder },
  { "expr.pointer",                    ifc_expr_pointer },
  { "expr.product-type-value",         ifc_expr_product },
  { "expr.push-state",                 ifc_expr_push_state },
  { "expr.qualified-name",             ifc_expr_qualified_name },
  { "expr.read",                       ifc_expr_read },
  { "expr.requires",                   ifc_expr_requires },
  { "expr.simple-identifier",          ifc_expr_simple_identifier },
  { "expr.sizeof-type",                ifc_expr_sizeof_type },
  { "expr.string-sequence",            ifc_expr_string_sequence },
  { "expr.strings",                    ifc_expr_string },
  { "expr.sum-type-value",             ifc_expr_sum },
  { "expr.syntax-tree",                ifc_expr_syntax_tree },
  { "expr.template-id",                ifc_expr_template_id },
  { "expr.template-reference",         ifc_expr_template_reference },
  { "expr.temporary",                  ifc_expr_temporary },
  { "expr.this",                       ifc_expr_this },
  { "expr.tokens",                     ifc_expr_tokens },
  { "expr.triad",                      ifc_expr_triad },
  { "expr.tuple",                      ifc_expr_tuple },
  { "expr.type",                       ifc_expr_type },
  { "expr.type-trait",                 ifc_expr_type_trait },
  { "expr.typeid",                     ifc_expr_typeid },
  { "expr.unary-fold",                 ifc_expr_unaryfold },
  { "expr.unqualified-id",             ifc_expr_unqualified_id },
  { "expr.unresolved",                 ifc_expr_unresolved_id },
  { "expr.vendor-extension",           ifc_expr_vendor_extension },
  { "expr.virtual-function-conversion",ifc_expr_virtual_function },
  { "form.spec",                       ifc_form_spec },
  { "heap.attr",                       ifc_heap_attr },
  { "heap.chart",                      ifc_heap_chart },
  { "heap.decl",                       ifc_heap_decl },
  { "heap.expr",                       ifc_heap_expr },
  { "heap.form",                       ifc_heap_form },
  /* Not mentioned in spec.  Found in IFC files. */
  { "heap.pp",                         ifc_heap_pp },
  { "heap.stmt",                       ifc_heap_stmt },
  { "heap.syn",                        ifc_heap_syn },
  { "heap.type",                       ifc_heap_type },
  { "macro.function-like",             ifc_macro_func_like },
  { "macro.object-like",               ifc_macro_obj_like },
  { "module.exported",                 ifc_module_exported },
  { "module.imported",                 ifc_module_imported },
  { "name.conversion",                 ifc_name_conversion },
  { "name.guide",                      ifc_name_guide },
  { "name.literal",                    ifc_name_literal },
  { "name.operator",                   ifc_name_operator },
  { "name.source-file",                ifc_name_source_file },
  { "name.specialization",             ifc_name_specialization },
  { "name.template",                   ifc_name_template },
  { "pp.catenate",                     ifc_form_catenate },
  { "pp.char",                         ifc_form_char },
  { "pp.header",                       ifc_form_header },
  { "pp.ident",                        ifc_form_ident },
  { "pp.junk",                         ifc_form_junk },
  { "pp.key",                          ifc_form_keyword },
  { "pp.num",                          ifc_form_number },
  { "pp.op",                           ifc_form_operator },
  { "pp.param",                        ifc_form_param },
  { "pp.paren",                        ifc_form_parenthesized },
  { "pp.pragma",                       ifc_form_pragma },
  { "pp.space",                        ifc_form_whitespace },
  { "pp.string",                       ifc_form_string },
  { "pp.to-string",                    ifc_form_stringize },
  { "pp.tuple",                        ifc_form_tuple },
  /* Not mentioned in spec.  Found in IFC files. */
  { "pragma-directive.vendor-extension", ifc_pragma_vendorext },
  /* Not mentioned in spec.  Found in IFC files. */
  { "pragma.state",                    ifc_pragma_state },
  { "scope.desc",                      ifc_scope_desc },
  { "scope.member",                    ifc_scope_member },
  { "src.line",                        ifc_src_line },
  { "src.sentence",                    ifc_sentence },
  { "src.word",                        ifc_word },
  { "stmt.block",                      ifc_stmt_block },
  { "stmt.break",                      ifc_stmt_break },
  { "stmt.case",                       ifc_stmt_case },
  { "stmt.continue",                   ifc_stmt_continue },
  { "stmt.default",                    ifc_stmt_default },
  { "stmt.do-while",                   ifc_stmt_do_while },
  { "stmt.empty",                      ifc_stmt_empty },
  { "stmt.expansion",                  ifc_stmt_expansion },
  { "stmt.expression",                 ifc_stmt_expression },
  { "stmt.for",                        ifc_stmt_for },
  { "stmt.if",                         ifc_stmt_if },
  { "stmt.return",                     ifc_stmt_return },
  { "stmt.switch",                     ifc_stmt_switch },
  { "stmt.syntax-tree",                ifc_stmt_syntax_tree },
  { "stmt.variable",                   ifc_stmt_variable },
  { "stmt.vendor-extension",           ifc_stmt_vendor_extension },
  { "stmt.while",                      ifc_stmt_while },
  { "syntax.access-specifier",         ifc_syntax_access_specifier },
  { "syntax.alias-declaration",        ifc_syntax_alias_declaration },
  { "syntax.alignas",                  ifc_syntax_alignas },
  { "syntax.array-declarator",         ifc_syntax_array_declarator },
  { "syntax.array-index",              ifc_syntax_array_index },
  { "syntax.array-or-function-declarator",
                                     ifc_syntax_array_or_function_declarator },
  { "syntax.asm-statement",            ifc_syntax_asm_statement },
  { "syntax.attribute",                ifc_syntax_attribute },
  { "syntax.attribute-argument-clause",ifc_syntax_attribute_argument_clause },
  { "syntax.attribute-specifier",      ifc_syntax_attribute_specifier },
  { "syntax.attribute-specifier-seq",  ifc_syntax_attribute_specifier_seq },
  { "syntax.attribute-using-prefix",   ifc_syntax_attribute_using_prefix },
  { "syntax.attributed-declaration",   ifc_syntax_attributed_declaration },
  { "syntax.attributed-statement",     ifc_syntax_attributed_statement },
  { "syntax.base-specifier",           ifc_syntax_base_specifier },
  { "syntax.base-specifier-list",      ifc_syntax_base_specifier_list },
  { "syntax.binary-fold-expression",   ifc_syntax_binary_fold_expression },
  { "syntax.break-statement",          ifc_syntax_break_statement },
  { "syntax.capture-default",          ifc_syntax_capture_default },
  { "syntax.class-specifier",          ifc_syntax_class_specifier },
  { "syntax.compound-requirement",     ifc_syntax_compound_requirement },
  { "syntax.compound-statement",       ifc_syntax_compound_statement },
  { "syntax.concept-definition",       ifc_syntax_concept_definition },
  { "syntax.condition-declaration",    ifc_syntax_condition_declaration },
  { "syntax.continue-statement",       ifc_syntax_continue_statement },
  { "syntax.ctor-initializer",         ifc_syntax_ctor_initializer },
  { "syntax.decl-specifier-seq",       ifc_syntax_decl_specifier_seq },
  { "syntax.declaration-statement",    ifc_syntax_declaration_statement },
  { "syntax.declarator",               ifc_syntax_declarator },
  { "syntax.decltype-specifier",       ifc_syntax_decltype_specifier },
  { "syntax.do-statement",             ifc_syntax_do_statement },
  { "syntax.dynamic-exception-spec",   ifc_syntax_dynamic_exception_spec },
  { "syntax.empty-statement",          ifc_syntax_empty_statement },
  { "syntax.enum-specifier",           ifc_syntax_enum_specifier },
  { "syntax.enumerator-definition",    ifc_syntax_enumerator_definition },
  { "syntax.exception-declaration",    ifc_syntax_exception_declaration },
  { "syntax.explicit-specifier",       ifc_syntax_explicit_specifier },
  { "syntax.expression",               ifc_syntax_expression },
  { "syntax.expression-statement",     ifc_syntax_expression_statement },
  { "syntax.for-range-declaration",    ifc_syntax_for_range_declaration },
  { "syntax.for-statement",            ifc_syntax_for_statement },
  { "syntax.function-body",            ifc_syntax_function_body },
  { "syntax.function-declarator",      ifc_syntax_function_declarator },
  { "syntax.function-definition",      ifc_syntax_function_definition },
  { "syntax.function-try-block",       ifc_syntax_function_try_block },
  { "syntax.goto-statement",           ifc_syntax_goto_statement },
  { "syntax.handler",                  ifc_syntax_handler },
  { "syntax.handler-seq",              ifc_syntax_handler_seq },
  { "syntax.if-statement",             ifc_syntax_if_statement },
  { "syntax.init-capture",             ifc_syntax_init_capture },
  { "syntax.init-declarator",          ifc_syntax_init_declarator },
  { "syntax.init-statement",           ifc_syntax_init_statement },
  { "syntax.labeled-statement",        ifc_syntax_labeled_statement },
  { "syntax.lambda-declarator",        ifc_syntax_lambda_declarator },
  { "syntax.lambda-introducer",        ifc_syntax_lambda_introducer },
  { "syntax.mem-initializer",          ifc_syntax_mem_initializer },
  { "syntax.member-declaration",       ifc_syntax_member_declaration },
  { "syntax.member-declarator",        ifc_syntax_member_declarator },
  { "syntax.member-function-declaration",
                                      ifc_syntax_member_function_declaration },
  { "syntax.member-specification",     ifc_syntax_member_specification },
  { "syntax.namespace-alias-definition",
                                       ifc_syntax_namespace_alias_definition },
  { "syntax.nested-requirement",       ifc_syntax_nested_requirement },
  { "syntax.new-declarator",           ifc_syntax_new_declarator },
  { "syntax.noexcept-specification",   ifc_syntax_noexcept_specification },
  { "syntax.non-type-template-argument",
                                       ifc_syntax_non_type_template_argument },
  { "syntax.parameter-declarator",     ifc_syntax_parameter_declarator },
  { "syntax.placeholder-type-specifier",
                                       ifc_syntax_placeholder_type_specifier },
  { "syntax.pointer-declarator",       ifc_syntax_pointer_declarator },
  { "syntax.range-based-for-statement",ifc_syntax_range_based_for_statement },
  { "syntax.requirement-body",         ifc_syntax_requirement_body },
  { "syntax.requires-clause",          ifc_syntax_requires_clause },
  { "syntax.return-statement",         ifc_syntax_return_statement },
  { "syntax.seh-except",               ifc_syntax_seh_except },
  { "syntax.seh-finally",              ifc_syntax_seh_finally },
  { "syntax.seh-leave",                ifc_syntax_seh_leave },
  { "syntax.seh-try",                  ifc_syntax_seh_try },
  { "syntax.simple-capture",           ifc_syntax_simple_capture },
  { "syntax.simple-declaration",       ifc_syntax_simple_declaration },
  { "syntax.simple-requirement",       ifc_syntax_simple_requirement },
  { "syntax.simple-type-specifier",    ifc_syntax_simple_type_specifier },
  { "syntax.statement-seq",            ifc_syntax_statement_seq },
  { "syntax.static-assert-declaration",ifc_syntax_static_assert_declaration },
  { "syntax.structured-binding-declaration",
                                   ifc_syntax_structured_binding_declaration },
  { "syntax.structured-binding-identifier",
                                    ifc_syntax_structured_binding_identifier },
  { "syntax.super",                    ifc_syntax_super },
  { "syntax.switch-statement",         ifc_syntax_switch_statement },
  { "syntax.template-argument-list",   ifc_syntax_template_argument_list },
  { "syntax.template-declaration",     ifc_syntax_template_declaration },
  { "syntax.template-id",              ifc_syntax_template_id },
  { "syntax.template-parameter-list",  ifc_syntax_template_parameter_list },
  { "syntax.template-template-parameter",
                                      ifc_syntax_template_template_parameter },
  { "syntax.this-capture",             ifc_syntax_this_capture },
  { "syntax.trailing-return-type",     ifc_syntax_trailing_return_type },
  { "syntax.try-block",                ifc_syntax_try_block },
  { "syntax.tuple",                    ifc_syntax_tuple },
  { "syntax.type-id",                  ifc_syntax_type_id },
  { "syntax.type-id-list-element",     ifc_syntax_type_id_list_element },
  { "syntax.type-requirement",         ifc_syntax_type_requirement },
  { "syntax.type-specifier-seq",       ifc_syntax_type_specifier_seq },
  { "syntax.type-template-argument",   ifc_syntax_type_template_argument },
  { "syntax.type-template-parameter",  ifc_syntax_type_template_parameter },
  { "syntax.type-trait-intrinsic",     ifc_syntax_type_trait_intrinsic },
  { "syntax.unary-fold-expression",    ifc_syntax_unary_fold_expression },
  { "syntax.using-declaration",        ifc_syntax_using_declaration },
  { "syntax.using-declarator",         ifc_syntax_using_declarator },
  { "syntax.using-directive",          ifc_syntax_using_directive },
  { "syntax.using-enum-declaration",   ifc_syntax_using_enum_decl },
  { "syntax.vendor-extension",         ifc_syntax_vendor_extension },
  { "syntax.virtual-specifier-seq",    ifc_syntax_virtual_specifier_seq },
  { "syntax.while-statement",          ifc_syntax_while_statement },
  { "trait.alias-template",            ifc_trait_alias_template },
  { "trait.attribute",                 ifc_trait_attribute },
  { "trait.deduction-guides",          ifc_trait_deduction_guides },
  { "trait.deprecated",                ifc_trait_deprecated },
  { "trait.friend",                    ifc_trait_friend },
  { "trait.mapping-expr",              ifc_trait_function_definition },
  { "trait.requires",                  ifc_trait_requires },
  { "trait.specialization",            ifc_trait_specialization },
  { "type.array",                      ifc_type_array },
  { "type.base",                       ifc_type_base },
  { "type.decltype",                   ifc_type_decltype },
  { "type.designated",                 ifc_type_designated },
  { "type.expansion",                  ifc_type_expansion },
  { "type.forall",                     ifc_type_forall },
  { "type.function",                   ifc_type_function },
  { "type.fundamental",                ifc_type_fundamental },
  { "type.lvalue-reference",           ifc_type_lvalue_reference },
  { "type.nonstatic-member-function",  ifc_type_method },
  { "type.placeholder",                ifc_type_placeholder },
  { "type.pointer",                    ifc_type_pointer },
  { "type.pointer-to-member",          ifc_type_pointer_to_member },
  { "type.qualified",                  ifc_type_qualified },
  { "type.rvalue-reference",           ifc_type_rvalue_reference },
  { "type.syntactic",                  ifc_type_syntactic },
  { "type.syntax-tree",                ifc_type_syntax_tree },
  { "type.tor",                        ifc_type_tor },
  { "type.tuple",                      ifc_type_tuple },
  { "type.typename",                   ifc_type_typename },
  { "type.unaligned",                  ifc_type_unaligned },
  { "type.vendor-extension",           ifc_type_vendor_extension },
  /* No special partition - name identifiers use the string table. */
  { NULL,                              ifc_name_identifier },
  { NULL,                              ifc_none },
  { NULL,                              ifc_last } /* Must be last. */
}
#endif /* VAR_INITIALIZERS */
 ;
/*lint -restore*/

/*
An internal representation of an IFC partition.
*/
struct an_ifc_partition {
  a_const_char	*name;	/* The name of the partition in the IFC file. */
  size_t	offset;	/* An offset from the beginning of the file to the
			   start of the partition. */
  uint32_t	size;	/* The number of bytes in the partition. */
  uint32_t	entry_size;
			/* The size of an entry in the partition. */
};  /* an_ifc_partition */


struct a_str_control_block;
struct a_partial_scope_stack_state;
/*
Information specific to an IFC module.
*/
/*lint -save -e1511 -e1790 -e1540*/
struct an_ifc_module : public a_module_interface {
  /* A local type used as an array (indexed by a file index) to provide
     information about mapping of sequence numbers for the file. */
  struct a_module_sequence_number_mapping {
    a_seq_number
		starting_sequence_number;
			/* Zero until the first time the file is referenced,
			   then set to the initial sequence number for the
			   file. */
    uint32_t	max_line_number;
			/* The maximum line number that will be seen in the
			   file.  Used to allocate a block of sequence numbers
			   that map to this file. */
  };
  an_ifc_File_Header
		header = {};
			/* The values of an IFC File_Header (byte-swapped if
			   necessary). */
  an_ifc_partition
		partitions[(int)ifc_last+1] = {};
			/* Information about each of the IFC partitions that
			   could exist in a module file. */
  a_module_sequence_number_mapping
		*sequence_numbers = NULL;
			/* A mapping of sequence numbers for the module,
			   indexed by a NameSort::SourceFile index.
			   Dynamically allocated (in front end memory) once
			   the number of source files is known. */
private:
#if USE_MMAP_FOR_MEMORY_REGIONS
  unsigned char
		*byte_buffer = NULL;
			/* Pointer to the current position in the buffer
			   used by get_byte, etc. */
  unsigned char
		*buffer_end = NULL;
			/* Pointer to the last byte of the buffer used by
			   get_byte, etc. */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  a_const_char	*string_table = NULL;
			/* The string table of the IFC file. */
  a_tmpl_decl_state_ptr
		curr_templ_decl_state = NULL;
			/* The current template declaration state, NULL if
			   there is no template declaration being processed. */
  Ptr_map<a_module_ref_key, a_module_import_decl_ptr>
		referenced_modules;
			/* A map from a module reference to the corresponding
			   import decl. */
  a_boolean
		unhandled_node_diag_issued = FALSE;
			/* Flag to indicate whether an unhandled node
			   diagnostic has already been issued for this
			   module. */
  a_boolean
		suppress_default_arguments = FALSE;
			/* Flag to indicate whether default arguments should be
			   included when processing an entity in this module.
			*/
  a_boolean
		suppress_automatic_name_qualification = FALSE;
			/* Flag to indicate ExprSort_NameDecl should not be
			   interpreted as concrete declarations that should be
			   qualified. */
  Ptr_map<ifc_DeclIndex, a_symbol_ptr>
		decl_map;

public:
  an_ifc_module() : a_module_interface((a_module_kind)mk_ifc),
                    referenced_modules(/*mask_width=*/4),
                    decl_map(/*mask_width=*/8)
  {}
  VIRTUAL ~an_ifc_module() EDG_NOEXCEPT = default;

  a_boolean matches_module(a_const_char *module_name,
                           a_const_char *module_file);

  inline a_boolean is_open() const OVERRIDE {
    return f_module != NULL;
  }

  a_boolean import(a_module_import_decl_ptr midp) OVERRIDE;
  void close() OVERRIDE;
  void pch_reset(a_module_import_decl_ptr midp) OVERRIDE;

  inline a_module_entity_ptr get_ifc_module_entity_ptr(ifc_TypeIndex index)
                                                                         const;
  inline a_module_entity_ptr get_ifc_module_entity_ptr(ifc_DeclIndex index)
                                                                         const;
  void process_ifc_declaration(a_module_entity_ptr mep,
                               a_boolean           defer,
                               a_type_ptr          enumeration_type);
  void complete_definition_of_module_class(a_module_entity_ptr mep) OVERRIDE;
  a_boolean cache_function_body(a_token_cache_ptr  cache,
                                ifc_DeclIndex      decl_idx,
                                a_routine_ptr      rp,
                                a_func_info_block  *func_info);

#if DEBUG
  void debug() const OVERRIDE;
  void db_module_entity(a_module_entity_ptr mep) const OVERRIDE;
#endif /* DEBUG */

private:
  enum a_non_type_kind : uint8_t {
    ntk_none,
    ntk_ellipsis,
    ntk_namespace,
    ntk_empty_pack_expansion,
  };
  inline void init_byte_buffer(size_t offset, size_t length);
  inline void get_bytes_from_buffer(void   *addr,
                                    size_t length);
  inline void get_mismatched_endian_bytes(void   *entity,
                                          size_t length);
  inline void get_bytes(void      *entity,
                        size_t    length,
                        a_boolean header_bytes);
  inline void issue_unsupported_node_diag(a_const_char      *node,
                                          a_source_position *pos);
  a_boolean open_and_map_ifc_module_file(a_module_import_decl_ptr midp,
                                         a_boolean                issue_diag);
  static an_ifc_partition_map *find_ifc_partition(a_const_char *name);
  a_module_import_decl_ptr transitive_import_module(
                                              const ifc_ModuleReference *ref);
  void import_referenced_modules();
  template<typename a_Scope_Member_Consumer>
  inline void traverse_scope_member_sequence(ifc_Sequence            seq,
                                             a_Scope_Member_Consumer consumer);
  void process_scope_member_sequence(ifc_Sequence seq);
  void process_template_specializations(ifc_DeclIndex decl_idx) const;
  void process_ifc_scope(ifc_ScopeIndex scope_index,
                         a_scope_ptr    scope);
  size_t get_num_entries(an_ifc_partition_kind partition) const;
  /* Module entity getters. */
  a_module_entity_ptr get_ifc_module_entity_ptr(
                                       an_ifc_partition_kind partition,
                                       ifc_Index_type        index) const;
  a_module_entity_ptr get_and_process_ifc_decl_from_other_module(
                                        const an_ifc_DeclSort_Reference *ref);
  a_module_entity_ptr get_and_process_ifc_decl_from_other_module(
                                                         ifc_DeclIndex index);
  ifc_NameIndex get_ifc_name_from_primary_template(
                                                 ifc_FormSpecIndex form_index);
  /* Value retrieval visitors. */
  template<typename a_Result_T, typename a_Derived_T>
  struct Decl_value_visitor;
  struct decl_name_visitor;
  struct decl_locus_visitor;
  struct decl_home_scope_decl_visitor;
  struct decl_access_visitor;
  /* IFC NameIndex readers. */
  template<typename an_ifc_DeclSort_T>
  inline ifc_NameIndex get_ifc_name(an_ifc_DeclSort_T *decl, char)
    { unexpected_condition_str("Name resolution unknown"); }
  template<typename an_ifc_DeclSort_T>
  inline auto get_ifc_name(an_ifc_DeclSort_T *decl, int)
                              -> Is_same<decltype(decl->name), ifc_NameIndex> 
    { return decl->name; }
  template<typename an_ifc_DeclSort_T>
  inline ifc_NameIndex get_ifc_name(an_ifc_DeclSort_T *decl)
    { return get_ifc_name(decl, 0); }
  ifc_NameIndex get_ifc_name(ifc_DeclIndex decl_index);
  /* IFC SourceLocation readers. */
  template<typename an_ifc_DeclSort_T>
  inline ifc_SourceLocation get_ifc_locus(an_ifc_DeclSort_T *decl, char)
    { unexpected_condition_str("Locus resolution unknown"); }
  template<typename an_ifc_DeclSort_T>
  inline auto get_ifc_locus(an_ifc_DeclSort_T *decl, int)
                        -> Is_same<decltype(decl->locus), ifc_SourceLocation>
    { return decl->locus; }
  template<typename an_ifc_DeclSort_T>
  inline ifc_SourceLocation get_ifc_locus(an_ifc_DeclSort_T *decl)
    { return get_ifc_locus(decl, 0); }
  ifc_SourceLocation get_ifc_locus(ifc_DeclIndex decl_index);
  /* IFC Home Scope Decl readers. */
  template<typename an_ifc_DeclSort_T>
  inline ifc_DeclIndex get_ifc_home_scope_decl(an_ifc_DeclSort_T *decl, char)
    { unexpected_condition_str("Home scope decl resolution unknown"); }
  template<typename an_ifc_DeclSort_T>
  inline auto get_ifc_home_scope_decl(an_ifc_DeclSort_T *decl, int)
                        -> Is_same<decltype(decl->home_scope), ifc_DeclIndex>
    { return decl->home_scope; }
  template<typename an_ifc_DeclSort_T>
  inline ifc_DeclIndex get_ifc_home_scope_decl(an_ifc_DeclSort_T *decl)
    { return get_ifc_home_scope_decl(decl, 0); }
  ifc_DeclIndex get_ifc_home_scope_decl(ifc_DeclIndex decl_index);
  /* IFC Scope readers. */
  a_scope_ptr get_ifc_scope(ifc_DeclIndex scope_index);
  template<typename an_ifc_DeclSort_T>
  inline a_scope_ptr get_ifc_home_scope(an_ifc_DeclSort_T *decl)
    { return get_ifc_scope(get_ifc_home_scope_decl(decl)); }
  inline a_scope_ptr get_ifc_home_scope(ifc_DeclIndex decl_index)
    { return get_ifc_scope(get_ifc_home_scope_decl(decl_index)); }
  /* IFC Access readers. */
  template<typename an_ifc_DeclSort_T>
  inline ifc_Access get_ifc_access(an_ifc_DeclSort_T *decl, char)
    { unexpected_condition_str("Access resolution unknown"); }
  template<typename an_ifc_DeclSort_T>
  inline auto get_ifc_access(an_ifc_DeclSort_T *decl, int)
                                -> Is_same<decltype(decl->access), ifc_Access>;
  template<typename an_ifc_DeclSort_T>
  inline ifc_Access get_ifc_access(an_ifc_DeclSort_T *decl)
    { return get_ifc_access(decl, 0); }
  ifc_Access get_ifc_access(ifc_DeclIndex decl_index);
  a_boolean is_name_qualifiable(ifc_DeclIndex decl_index);
  a_type_ptr type_for_type_index(ifc_TypeIndex   type_index,
                                 a_non_type_kind *kind);
  a_type_ptr type_for_template_id(an_ifc_ExprSort_TemplateId *templ_id);
  a_template_arg_ptr template_arg_for_expr(
                                         a_template_parameter_ptr tmpl_param,
                                         ifc_ExprIndex            expr_index);
  void source_position_from_locus(a_source_position  *pos,
                                  ifc_SourceLocation *locus);
  inline a_const_char *get_string_at_offset(ifc_TextOffset offset) const;
  a_const_char *string_from_name_index(ifc_NameIndex    name_index,
                                       a_symbol_locator *loc);
  a_const_char *name_from_decl(ifc_DeclIndex decl);
  a_const_char *name_from_other_module_decl(
                                        const an_ifc_DeclSort_Reference *ref);
  void init_dps(a_decl_parse_state          *dps,
                ifc_SourceLocation          *locus,
                ifc_TypeIndex               type_index,
                ifc_ObjectTraits            traits,
                ifc_MsvcTraits              msvc_traits,
                ifc_BasicSpecifiers         specifiers,
                ifc_Access                  access,
                ifc_ExprIndex               alignment,
                a_partial_scope_stack_state *psssp);
  void init_locator_from_name(ifc_NameIndex      name_index,
                              ifc_TextOffset     text_offset,
                              ifc_SourceLocation *locus,
                              a_symbol_locator   *loc);
  template<typename an_ifc_DeclSort_T>
  inline void init_decl_locator(an_ifc_DeclSort_T *decl,
                                a_symbol_locator  *loc);
  template<typename an_ifc_DeclSort_T>
  inline a_boolean lazy_init_module_scope(an_ifc_DeclSort_T   *decl,
                                          a_module_entity_ptr mep);
  template<typename an_ifc_DeclSort_T>
  inline a_boolean lazy_push_module_scope(an_ifc_DeclSort_T   *decl,
                                          a_module_entity_ptr mep);
  void unsigned_integer_for_expr_index(ifc_ExprIndex    expr_index,
                                       an_integer_value *value);
  a_constant_ptr constant_for_expr_index(ifc_ExprIndex expr_index,
                                         a_type_ptr    default_type);
  a_constant_ptr constant_for_named_decl(an_ifc_ExprSort_NamedDecl *iesndp);
  /* Queries. */
  a_boolean is_class_scope(ifc_DeclIndex scope);
  /* Token caching. */
  void cache_scope_member_sequence(a_token_cache_ptr cache,
                                   ifc_DeclIndex     scope_decl,
                                   ifc_Sequence      seq);
  void cache_scope(a_token_cache_ptr  cache,
                   ifc_ScopeIndex     scope,
                   ifc_SourceLocation *locus);
  template<typename a_Name_Cache_Fn, typename a_Scope_Cache_Fn>
  inline void cache_scope_decl(a_token_cache_ptr  cache,
                               ifc_DeclIndex      decl_idx,
                               ifc_TypeIndex      type,
                               a_Name_Cache_Fn    cache_name_fn,
                               a_Scope_Cache_Fn   cache_scope_fn,
                               ifc_SourceLocation *locus);
  void cache_scope_decl(a_token_cache_ptr  cache,
                        ifc_DeclIndex      decl_idx,
                        ifc_TypeIndex      type,
                        ifc_NameIndex      name,
                        ifc_TypeIndex      base,
                        ifc_ScopeIndex     scope,
                        ifc_SourceLocation *locus);
  void cache_type_first_pass(a_token_cache_ptr  cache,
                             ifc_TypeIndex      type,
                             ifc_SourceLocation *locus);
  void cache_type_second_pass(a_token_cache_ptr  cache,
                              ifc_TypeIndex      type,
                              ifc_SourceLocation *locus);
  void cache_type(a_token_cache_ptr  cache,
                  ifc_TypeIndex      type,
                  ifc_SourceLocation *locus);
  void cache_type_param_introducer(a_token_cache_ptr  cache,
                                   ifc_ExprIndex      constraint,
                                   a_source_position  *pos);
  void cache_attr(a_token_cache_ptr  cache, ifc_AttrIndex attr);
  void cache_attrs(a_token_cache_ptr cache, ifc_DeclIndex decl_idx);
  void cache_template_head(a_token_cache_ptr     cache,
                           ifc_ChartIndex        chart_idx,
                           a_source_position_ptr pos);
  void cache_decl(a_token_cache_ptr  cache,
                  ifc_DeclIndex      decl);
  enum a_cache_expr_option {
    ceo_none = 0x0,
    ceo_qualified_name = 0x1,
    ceo_skip_assign = 0x2
  };
  void cache_expr(a_token_cache_ptr    cache,
                  ifc_ExprIndex        expr,
                  a_cache_expr_option  options = ceo_none);
  enum a_cache_statement_option {
    cso_none = 0x0,
    cso_func_body = 0x1,
    cso_no_final_semicolon = 0x2
  };
  void cache_statement(a_token_cache_ptr         cache,
                       ifc_StmtIndex             stmt_idx,
                       a_cache_statement_option  options = cso_none);
  void cache_syntax(a_token_cache_ptr cache,
                    ifc_SyntaxIndex   syntax);
  void cache_chart(a_token_cache_ptr     cache,
                   ifc_ChartIndex        chart,
                   a_source_position_ptr pos);
  void cache_chart(a_token_cache_ptr  cache,
                   ifc_ChartIndex     chart,
                   ifc_SourceLocation *locus);
  void cache_operator(a_token_cache_ptr  cache,
                      ifc_Operator       op,
                      ifc_SourceLocation *locus);
  void cache_operator(a_token_cache_ptr   cache,
                      ifc_NiladicOperator op,
                      ifc_SourceLocation  *locus);
  void cache_operator(a_token_cache_ptr   cache,
                      ifc_MonadicOperator op,
                      ifc_SourceLocation  *locus);
  void cache_operator(a_token_cache_ptr  cache,
                      ifc_DyadicOperator op,
                      ifc_SourceLocation *locus);
  void cache_operator(a_token_cache_ptr   cache,
                      ifc_TriadicOperator op,
                      ifc_SourceLocation  *locus);
  void cache_operator(a_token_cache_ptr   cache,
                      ifc_StorageOperator op,
                      ifc_SourceLocation  *locus);
  void cache_operator(a_token_cache_ptr    cache,
                      ifc_VariadicOperator op,
                      ifc_SourceLocation   *locus);
  void cache_exception_spec(a_token_cache_ptr         cache,
                            ifc_NoexceptSpecification *eh_spec,
                            a_source_position_ptr     pos);
  uint32_t cache_sentence(a_token_cache_ptr cache,
                          ifc_SentenceIndex sentence,
                          uint32_t          offset = 0,
                          a_boolean         look_for_stop_token = FALSE);
  a_boolean sentence_is_deleted(ifc_SentenceIndex sentence);
  void cache_word(a_token_cache_ptr cache,
                  an_ifc_Word       *word);
  void cache_word(a_token_cache_ptr cache,
                  ifc_NestableWord  *word);
  void cache_source_directive(a_token_cache_ptr     cache,
                              ifc_SourceDirective   directive,
                              ifc_SourceLocation    *locus);
  void cache_source_punctuator(a_token_cache_ptr     cache,
                               ifc_SourcePunctuator  punctuator,
                               ifc_SourceLocation    *locus);
  void cache_source_literal(a_token_cache_ptr     cache,
                            ifc_SourceLiteral     literal,
                            ifc_Index             index,
                            ifc_SourceLocation    *locus);
  void cache_source_operator(a_token_cache_ptr     cache,
                             ifc_SourceOperator    op,
                             ifc_SourceLocation    *locus);
  void cache_source_keyword(a_token_cache_ptr     cache,
                            ifc_SourceKeyword     keyword,
                            ifc_SourceLocation    *locus);
  void cache_source_identifier(a_token_cache_ptr     cache,
                               ifc_SourceIdentifier  id,
                               ifc_Index             index,
                               ifc_SourceLocation    *locus);
  void cache_class_definition(a_token_cache_ptr     cache,
                              an_ifc_DeclSort_Scope *decl);
  uint32_t try_cache_class_attributes_from_body(
                                             a_token_cache_ptr cache,
                                             ifc_SentenceIndex body_sentence);
  uint32_t cache_decl_template_declaration(
                                      a_token_cache_ptr        cache,
                                      an_ifc_DeclSort_Template *decl,
                                      a_boolean                add_semicolon);
  void cache_decl_template(a_token_cache_ptr        cache,
                           an_ifc_DeclSort_Template *decl);
  void cache_simple_template_id(a_token_cache_ptr  cache,
                                ifc_FormSpecIndex  form_idx,
                                ifc_SourceLocation *locus);
  void cache_decl_partial_specialization_declaration(
                                a_token_cache_ptr                     cache,
                                ifc_DeclIndex                         decl_idx,
                                an_ifc_DeclSort_PartialSpecialization *decl);
  void cache_decl_partial_specialization(
                                a_token_cache_ptr                     cache,
                                ifc_DeclIndex                         decl_idx,
                                an_ifc_DeclSort_PartialSpecialization *decl);
  void cache_decl_explicit_specialization(
                               a_token_cache_ptr                      cache,
                               ifc_DeclIndex                          decl_idx,
                               an_ifc_DeclSort_ExplicitSpecialization *decl);
  void cache_decl_explicit_instantiation(
                                a_token_cache_ptr                     cache,
                                ifc_DeclIndex                         decl_idx,
                                an_ifc_DeclSort_ExplicitInstantiation *decl);
  template<typename a_Name_Cache_Fn, typename an_Init_Cache_Fn>
  inline void cache_variable_decl(a_token_cache_ptr   cache,
                                  ifc_DeclIndex       decl_idx,
                                  a_boolean           is_class_member,
                                  ifc_Access          access,
                                  a_boolean           cache_access_spec,
                                  ifc_BasicSpecifiers specifiers,
                                  ifc_ObjectTraits    traits,
                                  ifc_ExprIndex       alignment,
                                  ifc_TypeIndex       type,
                                  a_Name_Cache_Fn     cache_name_fn,
                                  ifc_ExprIndex       width,
                                  an_Init_Cache_Fn    cache_init_fn,
                                  ifc_SourceLocation  *locus);
  void cache_variable_decl(a_token_cache_ptr   cache,
                           ifc_DeclIndex       decl_idx,
                           a_boolean           is_class_member,
                           ifc_Access          access,
                           ifc_BasicSpecifiers specifiers,
                           ifc_ObjectTraits    traits,
                           ifc_ExprIndex       alignment,
                           ifc_TypeIndex       type,
                           ifc_NameIndex       name,
                           ifc_TextOffset      raw_name,
                           ifc_ExprIndex       width,
                           ifc_ExprIndex       initializer,
                           ifc_SourceLocation  *locus);
  template<typename a_Name_Cache_Fn>
  inline void cache_function_decl(a_token_cache_ptr         cache,
                                  a_boolean                 is_class_member,
                                  a_boolean                 is_dtor,
                                  ifc_Access                access,
                                  a_boolean                 cache_access_spec,
                                  ifc_CallingConvention     calling_conv,
                                  ifc_FunctionTraits        func_traits,
                                  ifc_FunctionTypeTraits    func_type_traits,
                                  ifc_TypeIndex             return_type,
                                  a_Name_Cache_Fn           cache_name_fn,
                                  ifc_ChartIndex            params,
                                  ifc_TypeIndex             param_types,
                                  ifc_NoexceptSpecification *eh_spec,
                                  ifc_SourceLocation        *locus);
  void cache_function_decl(a_token_cache_ptr         cache,
                           a_boolean                 is_class_member,
                           a_boolean                 is_dtor,
                           ifc_Access                access,
                           ifc_CallingConvention     calling_conv,
                           ifc_FunctionTraits        func_traits,
                           ifc_FunctionTypeTraits    func_type_traits,
                           ifc_TypeIndex             return_type,
                           ifc_NameIndex             name,
                           ifc_ChartIndex            params,
                           ifc_TypeIndex             param_types,
                           ifc_NoexceptSpecification *eh_spec,
                           ifc_SourceLocation        *locus);
  void cache_name(a_token_cache_ptr  cache,
                  ifc_NameIndex      name,
                  ifc_SourceLocation *locus);
  void cache_scope_as_nested_name_specifier(a_token_cache_ptr     cache,
                                            a_scope_ptr           scope,
                                            a_source_position_ptr pos);
  void cache_nested_name_specifier_from_decl(a_token_cache_ptr     cache,
                                             ifc_DeclIndex         decl,
                                             a_source_position_ptr pos);
  void cache_qualified_name_from_decl(a_token_cache_ptr  cache,
                                      ifc_DeclIndex      decl,
                                      ifc_SourceLocation *locus);
  void cache_name_from_decl(a_token_cache_ptr  cache,
                            ifc_DeclIndex      decl,
                            ifc_SourceLocation *locus);
  /* Readers and reading helpers. */
  inline size_t file_offset_of(an_ifc_partition_kind partition,
                               ifc_Index_type        index) const;
  inline ifc_DeclIndex decl_index_of(an_ifc_partition_kind partition,
                                     size_t                file_offset) const;
  inline ifc_DeclIndex decl_index_of(a_module_entity_ptr mep) const;
  inline ifc_AttrIndex attr_index_of(ifc_DeclIndex decl_idx);
  inline void read_partition_at_offset(an_ifc_partition_kind partition,
                                       size_t                offset);
  inline void read_partition_at_index(an_ifc_partition_kind partition,
                                      ifc_Index_type        index);
  inline void read_partition_at_index(ifc_AttrSort   attr_kind,
                                      ifc_Index_type index);
  inline void read_partition_at_index(ifc_AttrIndex type);
  inline void read_partition_at_index(ifc_TypeSort   attr_kind,
                                      ifc_Index_type index);
  inline void read_partition_at_index(ifc_TypeIndex type);
  inline void read_partition_at_index(ifc_ExprSort   expr_kind,
                                      ifc_Index_type index);
  inline void read_partition_at_index(ifc_ExprIndex expr);
  inline void read_partition_at_index(ifc_StmtSort   stmt_kind,
                                      ifc_Index_type index);
  inline void read_partition_at_index(ifc_StmtIndex stmt);
  inline void read_partition_at_index(ifc_DeclSort   decl_kind,
                                      ifc_Index_type index);
  inline void read_partition_at_index(ifc_DeclIndex decl);
  inline void read_partition_at_index(ifc_NameSort   name_kind,
                                      ifc_Index_type index);
  inline void read_partition_at_index(ifc_NameIndex name);
  inline void read_partition_at_index(ifc_ChartSort  chart_kind,
                                      ifc_Index_type index);
  inline void read_partition_at_index(ifc_ChartIndex chart);
  inline void read_partition_at_index(ifc_FormSpecIndex form_spec);
  inline void read_partition_at_index(ifc_SyntaxSort syntax_kind,
                                      ifc_Index_type index);
  inline void read_partition_at_index(ifc_SyntaxIndex syntax);
  inline ifc_Index read_index_from_heap(an_ifc_partition_kind heap_partition,
                                        ifc_Index_type        index);
  template<an_ifc_partition_kind a_Partition_Kind, typename a_Trait_T>
  inline a_Trait_T *find_trait(ifc_DeclIndex decl_index,
                               a_Trait_T     *storage);
  ifc_ChartIndex get_func_params_from_trait(ifc_DeclIndex decl);
  ifc_Sequence get_specialization_sequence_from_trait(ifc_DeclIndex decl);
  a_template_ptr parse_cached_explicit_specialization(
                             a_token_cache_ptr                      cache,
                             a_scope_ptr                            encl_scope,
                             an_ifc_DeclSort_ExplicitSpecialization *decl);
  void record_pending_explicit_specialization(
                                 a_decl_parse_state                     *dps,
                                 an_ifc_DeclSort_ExplicitSpecialization *decl);
  char *parse_cached_explicit_instantiation(
                                  a_token_cache_ptr                     cache,
                                  an_ifc_DeclSort_ExplicitInstantiation *decl,
                                  a_byte_il_entry_kind                  *kind);
#if DEBUG
  void validate_is_class_type(ifc_TypeIndex type);
#endif /* DEBUG */
  /* Stringizers. */
  void str_ifc_text_offset(ifc_TextOffset     offset,
                           a_str_control_block *scbp) const;
  void str_ifc_name_index(ifc_NameIndex       name_index,
                          a_str_control_block *scbp);
  void str_ifc_class_name(ifc_DeclIndex       home_scope,
                          a_str_control_block *scbp);
  void str_ifc_add_number(a_host_large_unsigned value,
                          a_str_control_block   *scbp) const;
#if !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
  void str_ifc_add_number(an_integer_value    &value,
                          a_str_control_block *scbp) const;
#endif /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  void str_ifc_source_location(ifc_SourceLocation  *locus,
                               a_str_control_block *scbp) const;
  void str_ifc_access(ifc_Access          access,
                      a_str_control_block *scbp) const;
  void str_ifc_qualifiers(ifc_Qualifiers      qualifiers,
                          a_str_control_block *scbp) const;
  void str_ifc_basic_specifiers(ifc_BasicSpecifiers specifiers,
                                a_str_control_block *scbp) const;
  void str_ifc_object_traits(ifc_ObjectTraits    traits,
                             a_str_control_block *scbp) const;
  void str_ifc_msvc_traits(ifc_MsvcTraits      traits,
                           a_str_control_block *scbp) const;
  void str_ifc_function_type_traits(ifc_FunctionTypeTraits traits,
                                    a_str_control_block    *scbp) const;
  void str_ifc_noexcept_specification(ifc_NoexceptSpecification *eh_spec,
                                      a_str_control_block       *scbp) const;
  void str_ifc_function_traits(ifc_FunctionTraits  traits,
                               a_boolean           prefix,
                               a_str_control_block *scbp) const;
  void str_ifc_expr_index(ifc_ExprIndex       expr_index,
                          a_str_control_block *scbp);
  void str_ifc_scope_index(ifc_ScopeIndex      scope_index,
                           a_str_control_block *scbp);
  void str_ifc_type_index_first_part(ifc_TypeIndex       type_index,
                                     a_str_control_block *scbp);
  void str_ifc_type_index_second_part(ifc_TypeIndex       type_index,
                                      a_str_control_block *scbp);
  void str_ifc_type_index(ifc_TypeIndex       type_index,
                          a_str_control_block *scbp);
  void str_ifc_common_decl(ifc_SourceLocation  *locus,
                           ifc_Access          access,
                           ifc_BasicSpecifiers specifiers,
                           ifc_ObjectTraits    traits,
                           a_str_control_block *scbp) const;
  void str_ifc_class_definition(an_ifc_DeclSort_Scope *idssp,
                                a_str_control_block   *scbp);
  void str_ifc_declaration(ifc_DeclIndex       decl_index,
                           a_boolean           is_designated_type,
                           a_str_control_block *scbp);
  void str_ifc_statement(ifc_StmtIndex       stmt_index,
                         a_str_control_block *scbp);
  void str_ifc_string_literal(ifc_StringIndex     str_index,
                              a_str_control_block *scbp);
  void str_ifc_chart(ifc_ChartIndex      chart_index,
                     a_str_control_block *scbp);
  template<typename T>
  void str_ifc_associated_trait(ifc_DeclIndex       decl_index,
                                a_str_control_block *scbp);
  void str_ifc_syntax_node(ifc_SyntaxIndex     syntax_index,
                           a_str_control_block *scbp);
  void str_ifc_sentence(ifc_SentenceIndex   sentence_index,
                        a_str_control_block *scbp);
  void str_ifc_word(ifc_WordIndex       word_index,
                    a_str_control_block *scbp);

#if DEBUG
  void db_ifc_file_header() const;
  void db_locus(ifc_SourceLocation *locus);
#if EXPENSIVE_CHECKING
  void f_db_get_byte(a_const_char *value_str,
                     void         *addr,
                     size_t       length) const;
#endif /* EXPENSIVE_CHECKING */
#endif /* DEBUG */

  template<typename T>
  inline T *get(T *storage, a_boolean fill_storage = FALSE) = delete;
  /* Generate prefixes for the entity getters. */
  #define IFC_DECL_START(name) \
  inline concat(an_ifc_, name) * concat(get_, name) ( \
                                  concat(an_ifc_, name) *ptr, \
                                  ARG_UNUSED a_boolean  fill_storage = FALSE);
  #define IFC_DECL_FIELD(field, type) /**/
  #define IFC_DECL_END(name) /**/
  #include "ifc_map.h"
};  /* an_ifc_module */
/*lint -restore*/

extern a_boolean load_routine_definition_from_ifc_module(a_routine_ptr  rp);

/* Explicit specializations of an_ifc_module::get_ifc_name. */
template<>
ifc_NameIndex an_ifc_module::get_ifc_name(
                                  an_ifc_DeclSort_PartialSpecialization *decl);
template<>
ifc_NameIndex an_ifc_module::get_ifc_name(
                                 an_ifc_DeclSort_ExplicitSpecialization *decl);
template<>
ifc_NameIndex an_ifc_module::get_ifc_name(
                                  an_ifc_DeclSort_ExplicitInstantiation *decl);

/* Explicit specializations of an_ifc_module::get_ifc_locus. */
template<>
ifc_SourceLocation an_ifc_module::get_ifc_locus(
                                 an_ifc_DeclSort_ExplicitSpecialization *decl);
template<>
ifc_SourceLocation an_ifc_module::get_ifc_locus(
                                  an_ifc_DeclSort_ExplicitInstantiation *decl);

/* Explicit specializations of an_ifc_module::get_ifc_home_scope_decl. */
template<>
ifc_DeclIndex an_ifc_module::get_ifc_home_scope_decl(
                                  an_ifc_DeclSort_PartialSpecialization *decl);
template<>
ifc_DeclIndex an_ifc_module::get_ifc_home_scope_decl(
                                 an_ifc_DeclSort_ExplicitSpecialization *decl);
template<>
ifc_DeclIndex an_ifc_module::get_ifc_home_scope_decl(
                                  an_ifc_DeclSort_ExplicitInstantiation *decl);

/* Explicit specializations of an_ifc_module::str_ifc_associated_trait<T>: */
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Deprecated>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Specialization>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Friend>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_FunctionDefinition>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_AliasTemplate>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_DeductionGuides>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Requires>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Attribute>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcVendorTrait>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcUuid>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcFuncParams>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcDeclAttrs>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp);

extern void record_pending_ifc_function_body(a_routine_ptr  rp,
                                             ifc_DeclIndex  decl_idx,
                                             an_ifc_module  *ifc_module);

extern void ifc_modules_one_time_init();

extern void ifc_modules_init();

#if DEBUG
extern void db_mep(a_module_entity_ptr mep);
#endif /* DEBUG */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*lint -restore*/ /* FIXME: temporary. */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* !STANDALONE_UTILITY_PROGRAM */

#endif /* ifndef IFC_MODULES_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
