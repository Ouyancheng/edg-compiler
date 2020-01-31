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

ifc_modules.h -- Declarations relating to ifc_modules.c (having to do with
                 Microsoft IFC modules).

*/

/* Avoid including these declarations more than once: */
#ifndef IFC_MODULES_H
#define IFC_MODULES_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

namespace {
/*
Magic numbers that identify the beginning of an IFC file.  Declared outside of
MICROSOFT_EXTENSIONS_ALLOWED to facilitate identifying the kind of a mismatched
module file.
*/
constexpr char ifc_magic_numbers[] = { '\x54', '\x51', '\x45', '\x1A' };
}  /* namespace */

#if MICROSOFT_EXTENSIONS_ALLOWED

/* These types are described by the IFC document. */
/* 32-bit types: */
typedef uint32_t ifc_ByteOffset;
typedef uint32_t ifc_Cardinality;
typedef uint32_t ifc_ChartIndex;
typedef uint32_t ifc_Column;
typedef uint32_t ifc_DeclIndex;
typedef uint32_t ifc_EntitySize;
typedef uint32_t ifc_ExprIndex;
typedef uint32_t ifc_Index;
typedef uint32_t ifc_LanguageVersion;
typedef uint32_t ifc_LineIndex;
typedef uint32_t ifc_LineNumber;
typedef uint32_t ifc_LitIndex;
typedef uint32_t ifc_MsvcTraits;
typedef uint32_t ifc_NameIndex;
typedef uint32_t ifc_Offset;
typedef uint32_t ifc_ParameterLevel;
typedef uint32_t ifc_ParameterPosition;
typedef uint32_t ifc_ScopeIndex;
typedef uint32_t ifc_SentenceIndex;
typedef uint32_t ifc_StmtIndex;
typedef uint32_t ifc_StringIndex;
typedef uint32_t ifc_SyntaxIndex;    /* Referenced, not defined. */
typedef uint32_t ifc_TextOffset;
typedef uint32_t ifc_TypeIndex;
typedef uint32_t ifc_UniqueID;
typedef uint32_t ifc_UnitIndex;
typedef uint32_t ifc_WordIndex;

/* 16-bit types: */
typedef uint16_t ifc_Alignment;
typedef uint16_t ifc_EHFlags;
typedef uint16_t ifc_FunctionTraits;
typedef uint16_t ifc_OperatorCategory_type;
enum class ifc_OperatorCategory : ifc_OperatorCategory_type;
typedef uint16_t ifc_PackSize;
typedef uint16_t ifc_WordCategory;   /* Referenced, not defined. */

/* 8-bit types: */
typedef uint8_t ifc_Abi;
typedef uint8_t ifc_Access_type;
enum class ifc_Access : ifc_Access_type;
typedef uint8_t ifc_Architecture_type;
enum class ifc_Architecture : ifc_Architecture_type;
typedef uint8_t ifc_BasicSpecifiers;
typedef uint8_t ifc_CallingConvention_type;
enum class ifc_CallingConvention : ifc_CallingConvention_type;
typedef uint8_t ifc_FunctionTypeTraits;
typedef uint8_t ifc_NoexceptSort_type;
enum class ifc_NoexceptSort : ifc_NoexceptSort_type;
typedef uint8_t ifc_ObjectTraits;
typedef uint8_t ifc_ParameterSort_type;
enum class ifc_ParameterSort : ifc_ParameterSort_type;
typedef uint8_t ifc_Qualifiers;
typedef uint8_t ifc_ReadConversionSort_type;
enum class ifc_ReadConversionSort : ifc_ReadConversionSort_type;
typedef uint8_t ifc_ScopeTraits;
typedef uint8_t ifc_SyntaxSort_type;
enum class ifc_SyntaxSort : ifc_SyntaxSort_type;
typedef uint8_t ifc_TypeBasis_type;
enum class ifc_TypeBasis : ifc_TypeBasis_type;
typedef uint8_t ifc_TypePrecision_type;
enum class ifc_TypePrecision : ifc_TypePrecision_type;
typedef uint8_t ifc_TypeSign_type;
enum class ifc_TypeSign : ifc_TypeSign_type;
typedef uint8_t ifc_Version;

/* Embedded tags: */
typedef uint8_t ifc_ChartSort_type;
enum class ifc_ChartSort : ifc_ChartSort_type;
typedef uint8_t ifc_DeclSort_type;
enum class ifc_DeclSort : ifc_DeclSort_type;
typedef uint8_t ifc_ExprSort_type;
enum class ifc_ExprSort : ifc_ExprSort_type;
typedef uint8_t ifc_LiteralSort_type;
enum class ifc_LiteralSort : ifc_LiteralSort_type;
typedef uint8_t ifc_NameSort_type;
enum class ifc_NameSort : ifc_NameSort_type;
typedef uint8_t ifc_StmtSort_type;
enum class ifc_StmtSort : ifc_StmtSort_type;
typedef uint8_t ifc_StringSort_type;
enum class ifc_StringSort : ifc_StringSort_type;
typedef uint8_t ifc_TypeSort_type;
enum class ifc_TypeSort : ifc_TypeSort_type;
typedef uint8_t ifc_UnitSort_type;
enum class ifc_UnitSort : ifc_UnitSort_type;

/* Some IFC fields have fundamental types. */
typedef uint8_t  ifc_bool;
typedef uint8_t  ifc_uint8_t;
typedef uint16_t ifc_uint16_t;

/* SHA256 checksum. */
typedef uint8_t sha256_t[32];
typedef sha256_t ifc_Checksum;

/*
Define some IFC structures that are used in nested inside other IFC structures.
These are handled specially here (rather than with the ifc_map.h automated
method) because the nesting can create padding issues on some architectures.
FIXME: See if these can be handled "automatically" as well.
*/
struct ifc_Sequence {
  ifc_Index     start;
  ifc_Cardinality
                cardinality;
};  /* ifc_Sequence */

struct ifc_ModuleReference {
  ifc_TextOffset
                owner;
  ifc_TextOffset
                partition;
};  /* ifc_ModuleReference */

struct ifc_SourceLocation {
  ifc_LineIndex line;
  ifc_Column    column;
};  /* ifc_SourceLocation */

struct ifc_NoexceptSpecification {
  ifc_SentenceIndex
                words;
  ifc_NoexceptSort
                sort;
  /* Note that there are three bytes of padding here. */
};  /* ifc_NoexceptSpecification */

struct ifc_ParameterizedEntity {
  ifc_Index     index;
  ifc_SentenceIndex
                head;
  ifc_SentenceIndex
                body;
  ifc_SentenceIndex
                attributes;
};

/*
Create structures for each of the IFC entities by setting the IFC_DECL macros
appropriately and including ifc_map.h.  The net result is something like:

  struct an_ifc_foo {
    ifc_field1_type field1;
    ifc_field2_type field2;
    ...
  };
*/

#define IFC_DECL_START(name) \
  struct concat(an_ifc_, name) {
#define IFC_DECL_FIELD(field, type) \
    concat(ifc_, type)  field;
#define IFC_DECL_END(name) \
  };  /* #name */

#include "ifc_map.h"  /*lint !e451 included more than once. */

#define Architecture(arch) (ifc_Architecture::ifc_Architecture_##arch)
#define ArchitectureAsType(arch) ((ifc_Architecture_type)Architecture(arch))

/* Enumeration for Architectures. */
enum class ifc_Architecture : ifc_Architecture_type {
  ifc_Architecture_Unknown = 0,
  ifc_Architecture_X86     = 0x01,
  ifc_Architecture_X64     = 0x02,
  ifc_Architecture_ARM32   = 0x03,
  ifc_Architecture_ARM64   = 0x04,
};

/* Enumeration for Qualifiers. */
enum an_ifc_Qualifier {
  ifc_Qualifier_None     = 0,  
  ifc_Qualifier_Const    = 1 << 0,
  ifc_Qualifier_Volatile = 1 << 1,
  ifc_Qualifier_Restrict = 1 << 2,
};

#define Access(access) (ifc_Access::ifc_Access_##access)
#define AccessAsType(access) ((ifc_Access_type)Access(access))

/* Enumeration for Access specifiers. */
enum class ifc_Access : ifc_Access_type {
  ifc_Access_None,      /* No access specifier. */
  ifc_Access_Private,   /* "private" for scope member. */
  ifc_Access_Protected, /* "protected" for scope member. */
  ifc_Access_Public,    /* "public" for scope member. */
};

/* Enumeration for BasicSpecifiers. */
enum an_ifc_BasicSpecifiers {
  ifc_BasicSpecifiers_Cxx               = 0,      /* C++ language linkage */
  ifc_BasicSpecifiers_C                 = 1 << 0, /* C language linkage */
  ifc_BasicSpecifiers_Internal          = 1 << 1,
  ifc_BasicSpecifiers_Vague             = 1 << 2, /* Vague linkage, e.g.
                                                     COMDAT, still external */
  ifc_BasicSpecifiers_External          = 1 << 3, /* External linkage */
  ifc_BasicSpecifiers_Deprecated        = 1 << 4, /* [[deprecated("bar")]] */
  ifc_BasicSpecifiers_InitializedInClass= 1 << 5, /* Defined or initialized in
                                                     class */
  ifc_BasicSpecifiers_NonExported       = 1 << 6, /* Not explicitly exported */
};

/* Enumeration for ObjectTraits. */
enum an_ifc_ObjectTraits {
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
enum an_ifc_MsvcTraits {
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
enum an_ifc_FunctionTraits {
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
  ifc_FunctionTraits_Vendor       = 1 << 15,
};

/* Enumeration for FunctionTypeTraits. */
enum an_ifc_FunctionTypeTraits {
  ifc_FunctionTypeTraits_None     = 0,
  ifc_FunctionTypeTraits_Const    = 1 << 0,
  ifc_FunctionTypeTraits_Volatile = 1 << 1,
  ifc_FunctionTypeTraits_Lvalue   = 1 << 2,
  ifc_FunctionTypeTraits_Rvalue   = 1 << 3,
};

#define CallingConvention(cc) \
                            (ifc_CallingConvention::ifc_CallingConvention_##cc)
#define CallingConventionAsType(cc) \
                            ((ifc_CallingConvention_type)CallingConventioo(cc))

/* Enumeration for CallingConventions. */
enum class ifc_CallingConvention : ifc_CallingConvention_type {
  ifc_CallingConvention_Cdecl,
  ifc_CallingConvention_Fast,
  ifc_CallingConvention_Std,
  ifc_CallingConvention_This,
  ifc_CallingConvention_Clr,
  ifc_CallingConvention_Vector,
  ifc_CallingConvention_Eabi,
};

#define NoexceptSort(noex) (ifc_NoexceptSort::ifc_NoexceptSort_##noex)
#define NoexceptSortAsType(noex) ((ifc_NoexceptSort_type)NoexceptSort(noex))

/* Enumeration for NoexceptSpecification. */
enum class ifc_NoexceptSort : ifc_NoexceptSort_type {
  ifc_NoexceptSort_None,
  ifc_NoexceptSort_False,
  ifc_NoexceptSort_True,
  ifc_NoexceptSort_Expression,
  ifc_NoexceptSort_Weak,
  ifc_NoexceptSort_Unenforced,
};

/* Enumeration for ScopeTraits. */
enum an_ifc_ScopeTraits {
  ifc_ScopeTraits_None          = 0,
  ifc_ScopeTraits_Unnamed       = 1 << 0,
  ifc_ScopeTraits_Inline        = 1 << 1,
  ifc_ScopeTraits_InitializerExported
                                = 1 << 2,
  ifc_ScopeTraits_ClosureType   = 1 << 3,
  ifc_ScopeTraits_Vendor        = 1 << 7,
};

/* Macros used to access UnitIndex::tag and UnitIndex::value. */
#define unit_tag_as_type(unit) ((unit) & 0x00000007)
#define unit_tag(unit) ((ifc_UnitSort)unit_sort_as_type(unit))
#define unit_value(unit) ((unit) >> 3)

#define UnitSort(unit) (ifc_UnitSort::ifc_UnitSort_##unit)
#define UnitSortAsType(unit) ((ifc_UnitSort_type)UnitSort(unit))

/* Enumeration for UnitSort (i.e., types of modules). */
enum class ifc_UnitSort : ifc_UnitSort_type {
  ifc_UnitSort_Source,
  ifc_UnitSort_Primary,
  ifc_UnitSort_Partition,
  ifc_UnitSort_Header,
  ifc_UnitSort_ExportedTU,
};

#define ParameterSort(param) (ifc_ParameterSort::ifc_ParameterSort_##param)
#define ParameterSortAsType(param) \
                                 ((ifc_ParameterSort_type)ParameterSort(param))

/* Enumeration for ParameterSort (i.e., types of parameters). */
enum class ifc_ParameterSort : ifc_ParameterSort_type {
  ifc_ParameterSort_Object,           /* Function parameter. */
  ifc_ParameterSort_Type,             /* Type template parameter. */
  ifc_ParameterSort_NonType,          /* Non-type template parameter. */
  ifc_ParameterSort_Template,         /* Template template parameter. */
};

/* Macros used to access TypeIndex::tag and TypeIndex::value. */
#define type_tag_as_type(type) ((type) & 0x0000001F)
#define type_tag(type) ((ifc_TypeSort)type_tag_as_type(type))
#define type_value(type) ((type) >> 5)

#define TypeSort(type) (ifc_TypeSort::ifc_TypeSort_##type)
#define TypeSortAsType(type) ((ifc_TypeSort_type)TypeSort(type))

/* Enumeration for TypeSort (i.e., types of types). */
enum class ifc_TypeSort : ifc_TypeSort_type {
  ifc_TypeSort_VendorExtension,
  ifc_TypeSort_Fundamental,
  ifc_TypeSort_Designated,
  ifc_TypeSort_Deduced,
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
  ifc_TypeSort_Unaligned,
  ifc_TypeSort_Decltype,
  ifc_TypeSort_Tuple,
  ifc_TypeSort_Forall,
  ifc_TypeSort_SyntaxTree,
  /* Must always equal the last enumerator above. */
  ifc_TypeSort_Last = ifc_TypeSort_SyntaxTree
};

/* Macros used to access StmtIndex::tag and StmtIndex::value. */
#define stmt_tag_as_type(stmt) ((stmt) & 0x0000000F)
#define stmt_tag(stmt) ((ifc_StmtSort)stmt_tag_as_type(stmt))
#define stmt_value(stmt) ((stmt) >> 4)

#define StmtSort(stmt) (ifc_StmtSort::ifc_StmtSort_##stmt)
#define StmtSortAsType(stmt) ((ifc_StmtSort_type)StmtSort(stmt))

/* Enumeration for StmtSort (i.e., types of statements). */
enum class ifc_StmtSort : ifc_StmtSort_type {
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
  ifc_StmtSort_SyntaxTree,
  /* Must always equal the last enumerator above. */
  ifc_StmtSort_Last = ifc_StmtSort_SyntaxTree
};

/* Macros used to access ExprIndex::tag and ExprIndex::value. */
#define expr_tag_as_type(expr) ((expr) & 0x0000003F)
#define expr_tag(expr) ((ifc_ExprSort)expr_tag_as_type(expr))
#define expr_value(expr) ((expr) >> 6)

#define ExprSort(expr) (ifc_ExprSort::ifc_ExprSort_##expr)
#define ExprSortAsType(expr) ((ifc_ExprSort_type)ExprSort(expr))

/* Enumeration for ExprSort (i.e., types of expressions). */
enum class ifc_ExprSort : ifc_ExprSort_type {
  ifc_ExprSort_VendorExtension,
  ifc_ExprSort_Empty,
  ifc_ExprSort_Literal,
  ifc_ExprSort_Type,
  ifc_ExprSort_NamedDecl,
  ifc_ExprSort_UnresolvedId,
  ifc_ExprSort_TemplateId,
  ifc_ExprSort_Identifier,
  ifc_ExprSort_SimpleIdentifier,
  ifc_ExprSort_Pointer,
  ifc_ExprSort_QualifiedName,
  ifc_ExprSort_Path,
  ifc_ExprSort_Read,
  ifc_ExprSort_Monad,
  ifc_ExprSort_Dyad,
  ifc_ExprSort_Triad,
  ifc_ExprSort_Tuple,
  ifc_ExprSort_Tokens,
  ifc_ExprSort_String,
  ifc_ExprSort_Temporary,
  ifc_ExprSort_Call,
  ifc_ExprSort_PushState,
  ifc_ExprSort_TypeTraitIntrinsic,
  ifc_ExprSort_MemberInitializer,
  ifc_ExprSort_MemberAccess,
  ifc_ExprSort_InheritancePath,
  ifc_ExprSort_TemplateReference,
  ifc_ExprSort_InitializerList,
  ifc_ExprSort_Cast,
  ifc_ExprSort_Condition,
  ifc_ExprSort_ExpressionList,
  ifc_ExprSort_AssignInitializer,
  ifc_ExprSort_Nullptr,
  ifc_ExprSort_This,
  ifc_ExprSort_SizeofTypeId,
  ifc_ExprSort_Alignof,
  ifc_ExprSort_PackedTemplateArguments,
  ifc_ExprSort_New,
  ifc_ExprSort_Delete,
  ifc_ExprSort_Lambda,
  ifc_ExprSort_Typeid,
  ifc_ExprSort_DestructorCall,
  ifc_ExprSort_SyntaxTree,
  ifc_ExprSort_FunctionString,
  ifc_ExprSort_CompoundString,
  ifc_ExprSort_StringSequence,
  ifc_ExprSort_Initializer,
  ifc_ExprSort_HierarchyConversion,
  ifc_ExprSort_Product,
  ifc_ExprSort_Sum,
  ifc_ExprSort_Subobject,
  ifc_ExprSort_Array,
  ifc_ExprSort_VirtualFunction,
  ifc_ExprSort_Requires,
  ifc_ExprSort_UnaryFold,
  ifc_ExprSort_BinaryFold,
  /* Must always equal the last enumerator above. */
  ifc_ExprSort_Last = ifc_ExprSort_BinaryFold,
};

#define ReadConversionSort(readconv) \
                    (ifc_ReadConversionSort::ifc_ReadConversionSort_##readconv)
#define ReadConversionSortAsType(readconv) \
                    ((ifc_ReadConversionSort_type)ReadConversionSort(readconv))

/* Enumeration for ReadConversionSort (i.e., kinds of read conversions). */
enum class ifc_ReadConversionSort : ifc_ReadConversionSort_type {
  ifc_ReadConversionSort_Identity,
  ifc_ReadConversionSort_Indirection,
  ifc_ReadConversionSort_Dereference,
  ifc_ReadConversionSort_LvalueToRvalue,
  ifc_ReadConversionSort_IntegralConversion,
};

/* Macros used to access StringIndex::tag and StringIndex::value. */
#define str_tag_as_type(str) ((str) & 0x0000000F)
#define str_tag(str) ((ifc_StringSort)str_tag_as_type(str))
#define str_value(str) ((str) >> 4)

#define StringSort(str) (ifc_StringSort::ifc_StringSort_##str)
#define StringSortAsType(str) ((ifc_StringSort_type)StringSort(str))

/* Enumeration for StringSort (i.e., kinds of strings). */
enum class ifc_StringSort : ifc_StringSort_type {
  ifc_StringSort_Ordinary,
  ifc_StringSort_UTF8,
  ifc_StringSort_Char16,
  ifc_StringSort_Char32,
  ifc_StringSort_Wide,
};

#define TypeBasis(type) (ifc_TypeBasis::ifc_TypeBasis_##type)
#define TypeBasisAsType(type) ((ifc_TypeBasis_type)TypeBasis(type))

/* Enumeration for TypeBasis (i.e., kinds of fundamental types). */
enum class ifc_TypeBasis : ifc_TypeBasis_type {
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

#define TypePrecision(type) (ifc_TypePrecision::ifc_TypePrecision_##type)
#define TypePrecisionAsType(type) ((ifc_TypePrecision_type)TypePrecision(type))

/* Enumeration for TypePrecision (i.e., sizes of fundamental types). */
enum class ifc_TypePrecision : ifc_TypePrecision_type {
  ifc_TypePrecision_Default,
  ifc_TypePrecision_Short,
  ifc_TypePrecision_Long,
  /* ifc_TypePrecision_Bit8 supposed to be here? */
  ifc_TypePrecision_Bit16 = 4,
  ifc_TypePrecision_Bit32,
  ifc_TypePrecision_Bit64,
  ifc_TypePrecision_Bit128,
};

#define TypeSign(type) (ifc_TypeSign::ifc_TypeSign_##type)
#define TypeSignAsType(type) ((ifc_TypeSign_type)TypeSign(type))

/* Enumeration for TypeSign (i.e., sign of fundamental types). */
enum class ifc_TypeSign : ifc_TypeSign_type {
  ifc_TypeSign_Plain,
  ifc_TypeSign_Signed,
  ifc_TypeSign_Unsigned,
};

/* Macros used to access DeclIndex::tag and DeclIndex::value. */
#define decl_tag_as_type(decl) ((decl) & 0x0000001F)
#define decl_tag(decl) ((ifc_DeclSort)decl_tag_as_type(decl))
#define decl_value(decl) ((decl) >> 5)
#define make_decl_index(tag, idx) \
  ((ifc_DeclIndex)((idx) << 5) | ((tag) & 0x0000001F))

#define DeclSort(decl) (ifc_DeclSort::ifc_DeclSort_##decl)
#define DeclSortAsType(decl) ((ifc_DeclSort_type)DeclSort(decl))

/* Enumeration for DeclSort (i.e., types of declarations). */
enum class ifc_DeclSort : ifc_DeclSort_type {
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
  ifc_DeclSort_Intrinsic,
  ifc_DeclSort_Function,
  ifc_DeclSort_Method,
  ifc_DeclSort_Constructor,
  ifc_DeclSort_InheritedConstructor,
  ifc_DeclSort_Destructor,
  ifc_DeclSort_Reference,
  ifc_DeclSort_Property,
  ifc_DeclSort_OutputSegment,
  ifc_DeclSort_UsingDeclaration,
  ifc_DeclSort_UsingDirective,
  ifc_DeclSort_Friend,
  ifc_DeclSort_SyntaxTree,
  ifc_DeclSort_Tuple,
  /* Must always equal the last enumerator above. */
  ifc_DeclSort_Last = ifc_DeclSort_Tuple
};

/* Macros used to access NameIndex::tag and NameIndex::value. */
#define name_tag_as_type(name) ((name) & 0x00000007)
#define name_tag(name) ((ifc_NameSort)name_tag_as_type(name))
#define name_value(name) ((name) >> 3)

#define NameSort(name) (ifc_NameSort::ifc_NameSort_##name)
#define NameSortAsType(name) ((ifc_NameSort_type)NameSort(name))

/* Enumeration for NameSort (i.e., types of names). */
enum class ifc_NameSort : ifc_NameSort_type {
  ifc_NameSort_Identifier,
  ifc_NameSort_Operator,
  ifc_NameSort_Conversion,
  ifc_NameSort_Literal,
  ifc_NameSort_Template,
  ifc_NameSort_Specialization,
  ifc_NameSort_SourceFile,
  /* Must always equal the last enumerator above. */
  ifc_NameSort_Last = ifc_NameSort_SourceFile
};

/* Macros used to access LitIndex::tag and LitIndex::index. */
#define literal_tag_as_type(litindex) ((litindex) & 0x00000003)
#define literal_tag(litindex) ((ifc_LiteralSort)literal_tag_as_type(litindex))
#define literal_index(litindex) ((litindex) >> 2)

#define LiteralSort(lit) (ifc_LiteralSort::ifc_LiteralSort_##lit)
#define LiteralSortAsType(lit) ((ifc_LiteralSort_type)LiteralSort(lit))

/* Enumeration for LiteralSort (i.e., types of literals). */
enum class ifc_LiteralSort : ifc_LiteralSort_type {
  ifc_LiteralSort_Immediate,
  ifc_LiteralSort_Integer,
  ifc_LiteralSort_FloatingPoint,
};

#define OperatorCategory(opcat) \
                           (ifc_OperatorCategory::ifc_OperatorCategory_##opcat)
#define OperatorCategoryAsType(opcat) \
                           ((ifc_OperatorCategory_type)OperatorCategory(opcat))

#if 0
/* The enumeration according to the IFC spec. */
/* Enumeration for OperatorCategory (i.e., types of operations). */
enum class ifc_OperatorCategory : ifc_OperatorCategory_type {
  /* Values in IFC files match this. */
  ifc_OperatorCategory_Bitand = 14,                /* operator& */
  /* Values in IFC files match this. */
  ifc_OperatorCategory_LogicAnd = 15,              /* operator&& */
  ifc_OperatorCategory_Assign = 16,                /* operator= */
  /* Values in IFC files say this is 19. */
  ifc_OperatorCategory_Comma = 18,                 /* operator, */
  /* Values in IFC files say this is 20. */
  ifc_OperatorCategory_Not = 19,                   /* operator! */
  /* Values in IFC files say this is 27. */
  ifc_OperatorCategory_Minus = 26,                 /* operator- */
  /* Values in IFC files say this is 28. */
  ifc_OperatorCategory_Star = 27,                  /* operator* */
  /* Values in IFC files say this is 29. */
  ifc_OperatorCategory_Bitor = 28,                 /* operator| */
  /* Values in IFC files say this is 30. */
  ifc_OperatorCategory_LogicOr = 29,               /* operator|| */
  /* Values in IFC files say this is 31. */
  ifc_OperatorCategory_Plus = 30,                  /* operator+ */
  ifc_OperatorCategory_Quest = 31,                 /* operator? */
  /* Values in IFC files say this is 33. */
  ifc_OperatorCategory_Complement = 32,            /* operator~ */
  /* Values in IFC files say this is 34. */
  ifc_OperatorCategory_Caret = 33,                 /* operator^ */
  /* Values in IFC files say this is 40. */
  ifc_OperatorCategory_Slash = 39,                 /* operator/ */
  /* Values in IFC files say this is 41. */
  ifc_OperatorCategory_Modulo = 40,                /* operator% */
  ifc_OperatorCategory_Percent = 41,               /* operator% */
  ifc_OperatorCategory_Sizeof = 62,                /* operator sizeof */
  ifc_OperatorCategory_ExpandingSizeof = 63,       /* operator sizeof... */
  /* Values in IFC files say this is 71. */
  ifc_OperatorCategory_New = 69,                   /* operator new */
  /* Values in IFC files say this is 72. */
  ifc_OperatorCategory_Delete = 70,                /* operator delete */
  ifc_OperatorCategory_Throw = 96,                 /* operator throw */
  ifc_OperatorCategory_Alignof = 99,               /* operator alignof */
  ifc_OperatorCategory_Noexcept = 166,             /* operator noexcept */
  ifc_OperatorCategory_Requires = 174,             /* operator requires */
  ifc_OperatorCategory_Coreturn = 185,             /* operator co_return */
  ifc_OperatorCategory_Await = 186,                /* operator co_await */
  ifc_OperatorCategory_Yield = 187,                /* operator co_yield */
  ifc_OperatorCategory_StaticAssert = 261,         /* operator static_assert */
  /* Values in IFC files say this is 381. */
  ifc_OperatorCategory_PostIncrement = 336,        /* operator++ */
  /* Values in IFC files say this is 380. */
  ifc_OperatorCategory_PostDecrement = 337,        /* operator-- */
  /* Values in IFC files say this is 358. */
  ifc_OperatorCategory_SlashEq = 338,              /* operator/= */
  /* Values in IFC files say this is 359. */
  ifc_OperatorCategory_EqEq = 339,                 /* operator== */
  /* Values in IFC files say this is 360. */
  ifc_OperatorCategory_NotEq = 340,                /* operator!= */
  /* Values in IFC files say this is 361. */
  ifc_OperatorCategory_Greater = 341,              /* operator> */
  /* Values in IFC files say this is 362. */
  ifc_OperatorCategory_GreaterEq = 342,            /* operator>= */
  /* Values in IFC files say this is 363. */
  ifc_OperatorCategory_Less = 343,                 /* operator< */
  /* Values in IFC files say this is 364. */
  ifc_OperatorCategory_LessEq = 344,               /* operator<= */
  /* Encountered in spaceship tests. */
  ifc_OperatorCategory_Spaceship = 365,            /* operator<=> */
  /* Values in IFC files say this is 366. */
  ifc_OperatorCategory_LshiftEq = 345,             /* operator<<= */
  /* Values in IFC files say this is 367. */
  ifc_OperatorCategory_RshiftEq = 346,             /* operator>>= */
  /* Values in IFC files say this is 368. */
  ifc_OperatorCategory_MinusEq = 347,              /* operator-= */
  /* Values in IFC files say this is 369. */
  ifc_OperatorCategory_ModuloEq = 348,             /* operator%= */
  /* Values in IFC files say this is 370. */
  ifc_OperatorCategory_StarEq = 349,               /* operator*= */
  /* Values in IFC files say this is 371. */
  ifc_OperatorCategory_BitorEq = 350,              /* operator|= */
  /* Values in IFC files say this is 372. */
  ifc_OperatorCategory_PlusEq = 351,               /* operator+= */
  /* Values in IFC files say this is 373. */
  ifc_OperatorCategory_BitandEq = 352,             /* operator&= */
  /* Values in IFC files say this is 374. */
  ifc_OperatorCategory_BitxorEq = 353,             /* operator^= */
  /* Values in IFC files say this is 375. */
  ifc_OperatorCategory_Lshift = 354,               /* operator<< */
  /* Values in IFC files say this is 376. */
  ifc_OperatorCategory_Rshift = 355,               /* operator>> */
  ifc_OperatorCategory_Dot = 356,                  /* operator. */
  ifc_OperatorCategory_Arrow = 357,                /* operator =  */
  /* Values in IFC files say this is also 380. */
  ifc_OperatorCategory_PreDecrement = 359,         /* operator-- */
  /* Values in IFC files say this is also 381. */
  ifc_OperatorCategory_PreIncrement = 360,         /* operator++ */
  ifc_OperatorCategory_UnaryMinus = 361,           /* operator- */
  ifc_OperatorCategory_Address = 362,              /* operator& */
  ifc_OperatorCategory_UnaryPlus = 381,            /* operator+ */
  ifc_OperatorCategory_DerefMemberAccess = 386,    /* operator.* */
  /* Values in IFC files say this is 410. */
  ifc_OperatorCategory_IndirectMemberAccess = 387, /* operator->* */
};
#else /* !0 */
/* The enumeration according to tests. */
/* Enumeration for OperatorCategory (i.e., types of operations). */
enum class ifc_OperatorCategory : ifc_OperatorCategory_type {
  ifc_OperatorCategory_Bitand = 14,                /* operator& */
  ifc_OperatorCategory_LogicAnd = 15,              /* operator&& */
  /* Untested */
  ifc_OperatorCategory_Assign = 16,                /* operator= */
  ifc_OperatorCategory_Comma = 19,                 /* operator, */
  ifc_OperatorCategory_Not = 20,                   /* operator! */
  ifc_OperatorCategory_Minus = 27,                 /* operator- */
  ifc_OperatorCategory_Star = 28,                  /* operator* */
  ifc_OperatorCategory_Bitor = 29,                 /* operator| */
  ifc_OperatorCategory_LogicOr = 30,               /* operator|| */
  ifc_OperatorCategory_Plus = 31,                  /* operator+ */
  /* Untested */
  ifc_OperatorCategory_Quest = 32,                 /* operator? */
  ifc_OperatorCategory_Complement = 33,            /* operator~ */
  ifc_OperatorCategory_Caret = 34,                 /* operator^ */
  ifc_OperatorCategory_Slash = 40,                 /* operator/ */
  ifc_OperatorCategory_Modulo = 41,                /* operator% */
  /* Untested */
  ifc_OperatorCategory_Percent = 42,               /* operator% */
  /* Untested */
  ifc_OperatorCategory_Sizeof = 62,                /* operator sizeof */
  /* Untested */
  ifc_OperatorCategory_ExpandingSizeof = 63,       /* operator sizeof... */
  ifc_OperatorCategory_New = 71,                   /* operator new */
  ifc_OperatorCategory_Delete = 72,                /* operator delete */
  /* Untested */
  ifc_OperatorCategory_Throw = 96,                 /* operator throw */
  /* Untested */
  ifc_OperatorCategory_Alignof = 99,               /* operator alignof */
  /* Untested */
  ifc_OperatorCategory_Noexcept = 166,             /* operator noexcept */
  /* Untested */
  ifc_OperatorCategory_Requires = 174,             /* operator requires */
  /* Untested */
  ifc_OperatorCategory_Coreturn = 185,             /* operator co_return */
  /* Untested */
  ifc_OperatorCategory_Await = 186,                /* operator co_await */
  /* Untested */
  ifc_OperatorCategory_Yield = 187,                /* operator co_yield */
  /* Untested */
  ifc_OperatorCategory_StaticAssert = 261,         /* operator static_assert */
  /* Values in IFC files say this is 381 - same as preinc, leaving as-was. */
  ifc_OperatorCategory_PostIncrement = 336,        /* operator++ */
  /* Values in IFC files say this is 380 - same as predec, leaving as-was. */
  ifc_OperatorCategory_PostDecrement = 337,        /* operator-- */
  ifc_OperatorCategory_SlashEq = 358,              /* operator/= */
  ifc_OperatorCategory_EqEq = 359,                 /* operator== */
  ifc_OperatorCategory_NotEq = 360,                /* operator!= */
  ifc_OperatorCategory_Greater = 361,              /* operator> */
  ifc_OperatorCategory_GreaterEq = 362,            /* operator>= */
  ifc_OperatorCategory_Less = 363,                 /* operator< */
  ifc_OperatorCategory_LessEq = 364,               /* operator<= */
  ifc_OperatorCategory_Spaceship = 365,            /* operator<=> */
  ifc_OperatorCategory_LshiftEq = 366,             /* operator<<= */
  ifc_OperatorCategory_RshiftEq = 367,             /* operator>>= */
  ifc_OperatorCategory_MinusEq = 368,              /* operator-= */
  ifc_OperatorCategory_ModuloEq = 369,             /* operator%= */
  ifc_OperatorCategory_StarEq = 370,               /* operator*= */
  ifc_OperatorCategory_BitorEq = 371,              /* operator|= */
  ifc_OperatorCategory_PlusEq = 372,               /* operator+= */
  ifc_OperatorCategory_BitandEq = 373,             /* operator&= */
  ifc_OperatorCategory_BitxorEq = 374,             /* operator^= */
  ifc_OperatorCategory_Lshift = 375,               /* operator<< */
  ifc_OperatorCategory_Rshift = 376,               /* operator>> */
  /* Untested */
  ifc_OperatorCategory_Dot = 377,                  /* operator. */
  /* Untested */
  ifc_OperatorCategory_Arrow = 378,                /* operator =  */
  ifc_OperatorCategory_PreDecrement = 380,         /* operator-- */
  ifc_OperatorCategory_PreIncrement = 381,         /* operator++ */
  /* Untested */
  ifc_OperatorCategory_UnaryMinus = 382,           /* operator- */
  /* Untested */
  ifc_OperatorCategory_Address = 383,              /* operator& */
  /* Untested */
  ifc_OperatorCategory_UnaryPlus = 404,            /* operator+ */
  /* Untested */
  ifc_OperatorCategory_DerefMemberAccess = 409,    /* operator.* */
  ifc_OperatorCategory_IndirectMemberAccess = 410, /* operator->* */
};

#endif /* 0*/

/* Macros used to access ChartIndex::tag and ChartIndex::value. */
#define chart_tag_as_type(chart) ((chart) & 0x00000003)
#define chart_tag(chart) ((ifc_ChartSort)chart_tag_as_type(chart))
#define chart_value(chart) ((chart) >> 2)

#define ChartSort(chart) (ifc_ChartSort::ifc_ChartSort_##chart)
#define ChartSortAsType(chart) ((ifc_ChartSort_type)ChartSort(chart))

/* Enumeration for ChartSort (i.e., kinds of charts). */
enum class ifc_ChartSort : ifc_ChartSort_type {
  ifc_ChartSort_None,
  ifc_ChartSort_Unilevel,
  ifc_ChartSort_Multilevel,
  /* Must always equal the last enumerator above. */
  ifc_ChartSort_Last
};

/* Macros used to access SyntaxIndex::tag and SyntaxIndex::value. */
#define syntax_tag_as_type(syn) ((syn) & 0x0000007F)
#define syntax_tag(syn) ((ifc_SyntaxSort)syntax_tag_as_type(syn))
#define syntax_value(syn) ((syn) >> 7)

#define SyntaxSort(syn) (ifc_SyntaxSort::ifc_SyntaxSort_##syn)
#define SyntaxSortAsType(syn) ((ifc_SyntaxSort_type)SyntaxSort(syn))

/* Enumeration for SyntaxSort (i.e., kinds of syntax). */
enum class ifc_SyntaxSort : ifc_SyntaxSort_type {
  ifc_SyntaxSort_VendorExtension,
             /* Vendor-specific extension for syntax. */
  ifc_SyntaxSort_SimpleTypeSpecifier,
             /* A simple type-specifier (i.e. no declarator) */
  ifc_SyntaxSort_DecltypeSpecifier,
             /* A decltype-specifier - 'decltype(expression)' */
  ifc_SyntaxSort_DecltypeAutoSpecifier,
             /* A decltype-specifier - 'decltype(auto)' */
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
  /* Must always equal the last enumerator above. */
  ifc_SyntaxSort_Last = ifc_SyntaxSort_StructuredBindingIdentifier
};

/*
An enumeration of IFC partitions.  Note that generally the order of
partitions doesn't matter, but in cases where a "tag" is used to identify
a particular set of partitions, group those together in such a way that the
tag can be added to a base value to get the proper partition.  This is
essential when using get_tag_from_partition.
*/
enum an_ifc_partition_kind_tag {
  ifc_none,
  /* Group all DeclIndex::tag partitions together. */
  ifc_decl_start,
  ifc_decl_vendor_extension = ifc_decl_start + DeclSortAsType(VendorExtension),
  ifc_decl_enumerator = ifc_decl_start + DeclSortAsType(Enumerator),
  ifc_decl_variable = ifc_decl_start + DeclSortAsType(Variable),
  ifc_decl_parameter = ifc_decl_start + DeclSortAsType(Parameter),
  ifc_decl_field = ifc_decl_start + DeclSortAsType(Field),
  ifc_decl_bitfield = ifc_decl_start + DeclSortAsType(Bitfield),
  ifc_decl_scope = ifc_decl_start + DeclSortAsType(Scope),
  ifc_decl_enum = ifc_decl_start + DeclSortAsType(Enumeration),
  ifc_decl_alias = ifc_decl_start + DeclSortAsType(Alias),
  ifc_decl_temploid = ifc_decl_start + DeclSortAsType(Temploid),
  ifc_decl_template = ifc_decl_start + DeclSortAsType(Template),
  ifc_decl_partial_specialization = ifc_decl_start +
                                         DeclSortAsType(PartialSpecialization),
  ifc_decl_explicit_specialization = ifc_decl_start +
                                        DeclSortAsType(ExplicitSpecialization),
  ifc_decl_explicit_instantiation = ifc_decl_start +
                                         DeclSortAsType(ExplicitInstantiation),
  ifc_decl_concept = ifc_decl_start + DeclSortAsType(Concept),
  ifc_decl_intrinsic = ifc_decl_start + DeclSortAsType(Intrinsic),
  ifc_decl_function = ifc_decl_start + DeclSortAsType(Function),
  ifc_decl_method = ifc_decl_start + DeclSortAsType(Method),
  ifc_decl_constructor = ifc_decl_start + DeclSortAsType(Constructor),
  ifc_decl_inh_ctor = ifc_decl_start + DeclSortAsType(InheritedConstructor),
  ifc_decl_destructor = ifc_decl_start + DeclSortAsType(Destructor),
  ifc_decl_reference = ifc_decl_start + DeclSortAsType(Reference),
  ifc_decl_property = ifc_decl_start + DeclSortAsType(Property),
  ifc_decl_segment = ifc_decl_start + DeclSortAsType(OutputSegment),
  ifc_decl_using_declaration = ifc_decl_start +
                                              DeclSortAsType(UsingDeclaration),
  ifc_decl_using_directive = ifc_decl_start + DeclSortAsType(UsingDirective),
  ifc_decl_friend = ifc_decl_start + DeclSortAsType(Friend),
  ifc_decl_syntax_tree = ifc_decl_start + DeclSortAsType(SyntaxTree),
  ifc_decl_tuple = ifc_decl_start + DeclSortAsType(Tuple),
  ifc_decl_end = ifc_decl_start + DeclSortAsType(Last),
  /* Group all TypeIndex::tag partitions together. */
  ifc_type_start,
  ifc_type_vendor_extension = ifc_type_start + TypeSortAsType(VendorExtension),
  ifc_type_fundamental = ifc_type_start + TypeSortAsType(Fundamental),
  ifc_type_designated = ifc_type_start + TypeSortAsType(Designated),
  ifc_type_deduced = ifc_type_start + TypeSortAsType(Deduced),
  ifc_type_syntactic = ifc_type_start + TypeSortAsType(Syntactic),
  ifc_type_expansion = ifc_type_start + TypeSortAsType(Expansion),
  ifc_type_pointer = ifc_type_start + TypeSortAsType(Pointer),
  ifc_type_pointer_to_member = ifc_type_start +
                                               TypeSortAsType(PointerToMember),
  ifc_type_lvalue_reference = ifc_type_start + TypeSortAsType(LvalueReference),
  ifc_type_rvalue_reference = ifc_type_start + TypeSortAsType(RvalueReference),
  ifc_type_function = ifc_type_start + TypeSortAsType(Function),
  ifc_type_nonstatic_member_function = ifc_type_start + TypeSortAsType(Method),
  ifc_type_array = ifc_type_start + TypeSortAsType(Array),
  ifc_type_typename = ifc_type_start + TypeSortAsType(Typename),
  ifc_type_qualified = ifc_type_start + TypeSortAsType(Qualified),
  ifc_type_base = ifc_type_start + TypeSortAsType(Base),
  ifc_type_unaligned = ifc_type_start + TypeSortAsType(Unaligned),
  ifc_type_decltype = ifc_type_start + TypeSortAsType(Decltype),
  ifc_type_tuple = ifc_type_start + TypeSortAsType(Tuple),
  ifc_type_forall = ifc_type_start + TypeSortAsType(Forall),
  ifc_type_syntax_tree = ifc_type_start + TypeSortAsType(SyntaxTree),
  ifc_type_end = ifc_type_start + TypeSortAsType(Last),
  /* Group all NameSort::tag partitions together.  Note that there is no
     partition for NameSort::Identifier (ifc_NameSort_Identifier). */
  ifc_name_start,
  ifc_name_operator = ifc_name_start + NameSortAsType(Operator),
  ifc_name_conversion = ifc_name_start + NameSortAsType(Conversion),
  ifc_name_literal = ifc_name_start + NameSortAsType(Literal),
  ifc_name_template = ifc_name_start + NameSortAsType(Template),
  ifc_name_specialization = ifc_name_start + NameSortAsType(Specialization),
  ifc_name_source_file = ifc_name_start + NameSortAsType(SourceFile),
  ifc_name_end = ifc_name_start + NameSortAsType(Last),
  /* Group all ExprSort::tag partitions together. */
  ifc_expr_start,
  ifc_expr_vendor_extension = ifc_expr_start + ExprSortAsType(VendorExtension),
  ifc_expr_empty = ifc_expr_start + ExprSortAsType(Empty),
  ifc_expr_literal = ifc_expr_start + ExprSortAsType(Literal),
  ifc_expr_type = ifc_expr_start + ExprSortAsType(Type),
  ifc_expr_decl = ifc_expr_start + ExprSortAsType(NamedDecl),
  ifc_expr_unresolved = ifc_expr_start + ExprSortAsType(UnresolvedId),
  ifc_expr_template_id = ifc_expr_start + ExprSortAsType(TemplateId),
  ifc_expr_identifier = ifc_expr_start + ExprSortAsType(Identifier),
  ifc_expr_simple_identifier = ifc_expr_start +
                                              ExprSortAsType(SimpleIdentifier),
  ifc_expr_pointer = ifc_expr_start + ExprSortAsType(Pointer),
  ifc_expr_qualified_name = ifc_expr_start + ExprSortAsType(QualifiedName),
  ifc_expr_path = ifc_expr_start + ExprSortAsType(Path),
  ifc_expr_read = ifc_expr_start + ExprSortAsType(Read),
  ifc_expr_monad = ifc_expr_start + ExprSortAsType(Monad),
  ifc_expr_dyad = ifc_expr_start + ExprSortAsType(Dyad),
  ifc_expr_triad = ifc_expr_start + ExprSortAsType(Triad),
  ifc_expr_tuple = ifc_expr_start + ExprSortAsType(Tuple),
  ifc_expr_tokens = ifc_expr_start + ExprSortAsType(Tokens),
  ifc_expr_strings = ifc_expr_start + ExprSortAsType(String),
  ifc_expr_temporary = ifc_expr_start + ExprSortAsType(Temporary),
  ifc_expr_call = ifc_expr_start + ExprSortAsType(Call),
  ifc_expr_push_state = ifc_expr_start + ExprSortAsType(PushState),
  ifc_expr_type_trait = ifc_expr_start + ExprSortAsType(TypeTraitIntrinsic),
  ifc_expr_member_initializer = ifc_expr_start +
                                             ExprSortAsType(MemberInitializer),
  ifc_expr_member_access = ifc_expr_start + ExprSortAsType(MemberAccess),
  ifc_expr_inheritance_path = ifc_expr_start + ExprSortAsType(InheritancePath),
  ifc_expr_template_reference = ifc_expr_start +
                                             ExprSortAsType(TemplateReference),
  ifc_expr_initializer_list = ifc_expr_start + ExprSortAsType(InitializerList),
  ifc_expr_cast = ifc_expr_start + ExprSortAsType(Cast),
  ifc_expr_condition = ifc_expr_start + ExprSortAsType(Condition),
  ifc_expr_expression_list = ifc_expr_start + ExprSortAsType(ExpressionList),
  ifc_expr_assign_initializer = ifc_expr_start +
                                             ExprSortAsType(AssignInitializer),
  ifc_expr_nullptr = ifc_expr_start + ExprSortAsType(Nullptr),
  ifc_expr_this = ifc_expr_start + ExprSortAsType(This),
  ifc_expr_sizeof_type_id = ifc_expr_start + ExprSortAsType(SizeofTypeId),
  ifc_expr_alignof_type_id = ifc_expr_start + ExprSortAsType(Alignof),
  ifc_expr_packed_template_arguments = ifc_expr_start +
                                       ExprSortAsType(PackedTemplateArguments),
  ifc_expr_new = ifc_expr_start + ExprSortAsType(New),
  ifc_expr_delete = ifc_expr_start + ExprSortAsType(Delete),
  ifc_expr_lambda = ifc_expr_start + ExprSortAsType(Lambda),
  ifc_expr_typeid = ifc_expr_start + ExprSortAsType(Typeid),
  ifc_expr_destructor_call = ifc_expr_start + ExprSortAsType(DestructorCall),
  ifc_expr_syntax_tree = ifc_expr_start + ExprSortAsType(SyntaxTree),
  ifc_expr_function_string = ifc_expr_start + ExprSortAsType(FunctionString),
  ifc_expr_compound_string = ifc_expr_start + ExprSortAsType(CompoundString),
  ifc_expr_string_sequence = ifc_expr_start + ExprSortAsType(StringSequence),
  ifc_expr_initializer = ifc_expr_start + ExprSortAsType(Initializer),
  ifc_expr_hierarchy_conversion = ifc_expr_start +
                                           ExprSortAsType(HierarchyConversion),
  ifc_expr_product = ifc_expr_start + ExprSortAsType(Product),
  ifc_expr_sum = ifc_expr_start + ExprSortAsType(Sum),
  ifc_expr_subobject = ifc_expr_start + ExprSortAsType(Subobject),
  ifc_expr_array = ifc_expr_start + ExprSortAsType(Array),
  ifc_expr_virtual_function = ifc_expr_start + ExprSortAsType(VirtualFunction),
  ifc_expr_requires = ifc_expr_start + ExprSortAsType(Requires),
  ifc_expr_unaryfold = ifc_expr_start + ExprSortAsType(UnaryFold),
  ifc_expr_binaryfold = ifc_expr_start + ExprSortAsType(BinaryFold),
  ifc_expr_end = ifc_expr_start + ExprSortAsType(Last),
  /* Group all StmtSort::Tag partitions together. */
  ifc_stmt_start,
  ifc_stmt_vendor_extension = ifc_stmt_start + StmtSortAsType(VendorExtension),
  ifc_stmt_empty = ifc_stmt_start + StmtSortAsType(Empty),
  ifc_stmt_if = ifc_stmt_start + StmtSortAsType(If),
  ifc_stmt_for = ifc_stmt_start + StmtSortAsType(For),
  ifc_stmt_case = ifc_stmt_start + StmtSortAsType(Case),
  ifc_stmt_while = ifc_stmt_start + StmtSortAsType(While),
  ifc_stmt_block = ifc_stmt_start + StmtSortAsType(Block),
  ifc_stmt_break = ifc_stmt_start + StmtSortAsType(Break),
  ifc_stmt_switch = ifc_stmt_start + StmtSortAsType(Switch),
  ifc_stmt_do_while = ifc_stmt_start + StmtSortAsType(DoWhile),
  ifc_stmt_default = ifc_stmt_start + StmtSortAsType(Default),
  ifc_stmt_continue = ifc_stmt_start + StmtSortAsType(Continue),
  ifc_stmt_expression = ifc_stmt_start + StmtSortAsType(Expression),
  ifc_stmt_return = ifc_stmt_start + StmtSortAsType(Return),
  ifc_stmt_variable = ifc_stmt_start + StmtSortAsType(VariableDecl),
  ifc_stmt_syntax_tree = ifc_stmt_start + StmtSortAsType(SyntaxTree),
  ifc_stmt_end = ifc_stmt_start + StmtSortAsType(Last),
  /* Group all ChartSort::Tag partitions together. */
  ifc_chart_start,
  ifc_chart_none = ifc_chart_start + ChartSortAsType(None),
  ifc_chart_multilevel = ifc_chart_start + ChartSortAsType(Multilevel),
  ifc_chart_unilevel = ifc_chart_start + ChartSortAsType(Unilevel),
  ifc_chart_end = ifc_chart_start + ChartSortAsType(Last),
  /* Group all SyntaxSort::Tag partitions together. */
  ifc_syntax_start,
  ifc_syntax_vendor_extension = ifc_syntax_start +
                                             SyntaxSortAsType(VendorExtension),
  ifc_syntax_simple_type_specifier = ifc_syntax_start +
                                         SyntaxSortAsType(SimpleTypeSpecifier),
  ifc_syntax_decltype_specifier = ifc_syntax_start +
                                           SyntaxSortAsType(DecltypeSpecifier),
  ifc_syntax_decltype_auto_specifier = ifc_syntax_start +
                                       SyntaxSortAsType(DecltypeAutoSpecifier),
  ifc_syntax_type_specifier_seq = ifc_syntax_start +
                                            SyntaxSortAsType(TypeSpecifierSeq),
  ifc_syntax_decl_specifier_seq = ifc_syntax_start +
                                            SyntaxSortAsType(DeclSpecifierSeq),
  ifc_syntax_virtual_specifier_seq = ifc_syntax_start +
                                         SyntaxSortAsType(VirtualSpecifierSeq),
  ifc_syntax_noexcept_specification = ifc_syntax_start +
                                       SyntaxSortAsType(NoexceptSpecification),
  ifc_syntax_explicit_specifier = ifc_syntax_start +
                                           SyntaxSortAsType(ExplicitSpecifier),
  ifc_syntax_enum_specifier = ifc_syntax_start +
                                               SyntaxSortAsType(EnumSpecifier),
  ifc_syntax_enumerator_definition = ifc_syntax_start +
                                        SyntaxSortAsType(EnumeratorDefinition),
  ifc_syntax_class_specifier = ifc_syntax_start +
                                              SyntaxSortAsType(ClassSpecifier),
  ifc_syntax_member_specification = ifc_syntax_start +
                                         SyntaxSortAsType(MemberSpecification),
  ifc_syntax_member_declaration = ifc_syntax_start +
                                           SyntaxSortAsType(MemberDeclaration),
  ifc_syntax_member_declarator = ifc_syntax_start +
                                            SyntaxSortAsType(MemberDeclarator),
  ifc_syntax_access_specifier = ifc_syntax_start +
                                             SyntaxSortAsType(AccessSpecifier),
  ifc_syntax_base_specifier_list = ifc_syntax_start +
                                           SyntaxSortAsType(BaseSpecifierList),
  ifc_syntax_base_specifier = ifc_syntax_start +
                                               SyntaxSortAsType(BaseSpecifier),
  ifc_syntax_type_id = ifc_syntax_start + SyntaxSortAsType(TypeId),
  ifc_syntax_trailing_return_type = ifc_syntax_start +
                                          SyntaxSortAsType(TrailingReturnType),
  ifc_syntax_declarator = ifc_syntax_start + SyntaxSortAsType(Declarator),
  ifc_syntax_pointer_declarator = ifc_syntax_start +
                                           SyntaxSortAsType(PointerDeclarator),
  ifc_syntax_array_declarator = ifc_syntax_start +
                                             SyntaxSortAsType(ArrayDeclarator),
  ifc_syntax_function_declarator = ifc_syntax_start +
                                          SyntaxSortAsType(FunctionDeclarator),
  ifc_syntax_array_or_function_declarator = ifc_syntax_start +
                                   SyntaxSortAsType(ArrayOrFunctionDeclarator),
  ifc_syntax_parameter_declarator = ifc_syntax_start +
                                         SyntaxSortAsType(ParameterDeclarator),
  ifc_syntax_init_declarator = ifc_syntax_start +
                                              SyntaxSortAsType(InitDeclarator),
  ifc_syntax_new_declarator = ifc_syntax_start +
                                               SyntaxSortAsType(NewDeclarator),
  ifc_syntax_simple_declaration = ifc_syntax_start +
                                           SyntaxSortAsType(SimpleDeclaration),
  ifc_syntax_exception_declaration = ifc_syntax_start +
                                        SyntaxSortAsType(ExceptionDeclaration),
  ifc_syntax_condition_declaration = ifc_syntax_start +
                                        SyntaxSortAsType(ConditionDeclaration),
  ifc_syntax_static_assert_declaration = ifc_syntax_start +
                                     SyntaxSortAsType(StaticAssertDeclaration),
  ifc_syntax_alias_declaration = ifc_syntax_start +
                                            SyntaxSortAsType(AliasDeclaration),
  ifc_syntax_concept_definition = ifc_syntax_start +
                                           SyntaxSortAsType(ConceptDefinition),
  ifc_syntax_compound_statement = ifc_syntax_start +
                                           SyntaxSortAsType(CompoundStatement),
  ifc_syntax_return_statement = ifc_syntax_start +
                                             SyntaxSortAsType(ReturnStatement),
  ifc_syntax_if_statement = ifc_syntax_start + SyntaxSortAsType(IfStatement),
  ifc_syntax_while_statement = ifc_syntax_start +
                                              SyntaxSortAsType(WhileStatement),
  ifc_syntax_do_statement = ifc_syntax_start +
                                            SyntaxSortAsType(DoWhileStatement),
  ifc_syntax_for_statement = ifc_syntax_start + SyntaxSortAsType(ForStatement),
  ifc_syntax_init_statement = ifc_syntax_start +
                                               SyntaxSortAsType(InitStatement),
  ifc_syntax_range_based_for_statement = ifc_syntax_start +
                                      SyntaxSortAsType(RangeBasedForStatement),
  ifc_syntax_for_range_declaration = ifc_syntax_start +
                                         SyntaxSortAsType(ForRangeDeclaration),
  ifc_syntax_labeled_statement = ifc_syntax_start +
                                            SyntaxSortAsType(LabeledStatement),
  ifc_syntax_break_statement = ifc_syntax_start +
                                              SyntaxSortAsType(BreakStatement),
  ifc_syntax_continue_statement = ifc_syntax_start +
                                           SyntaxSortAsType(ContinueStatement),
  ifc_syntax_switch_statement = ifc_syntax_start +
                                             SyntaxSortAsType(SwitchStatement),
  ifc_syntax_declaration_statement = ifc_syntax_start +
                                        SyntaxSortAsType(DeclarationStatement),
  ifc_syntax_expression_statement = ifc_syntax_start +
                                         SyntaxSortAsType(ExpressionStatement),
  ifc_syntax_try_block = ifc_syntax_start + SyntaxSortAsType(TryBlock),
  ifc_syntax_handler = ifc_syntax_start + SyntaxSortAsType(Handler),
  ifc_syntax_handler_seq = ifc_syntax_start + SyntaxSortAsType(HandlerSeq),
  ifc_syntax_function_try_block = ifc_syntax_start +
                                            SyntaxSortAsType(FunctionTryBlock),
  ifc_syntax_type_id_list_element = ifc_syntax_start +
                                           SyntaxSortAsType(TypeIdListElement),
  ifc_syntax_dynamic_exception_spec = ifc_syntax_start +
                                        SyntaxSortAsType(DynamicExceptionSpec),
  ifc_syntax_statement_seq = ifc_syntax_start + SyntaxSortAsType(StatementSeq),
  ifc_syntax_function_body = ifc_syntax_start + SyntaxSortAsType(FunctionBody),
  ifc_syntax_expression = ifc_syntax_start + SyntaxSortAsType(Expression),
  ifc_syntax_function_definition = ifc_syntax_start +
                                          SyntaxSortAsType(FunctionDefinition),
  ifc_syntax_member_function_declaration = ifc_syntax_start +
                                   SyntaxSortAsType(MemberFunctionDeclaration),
  ifc_syntax_template_declaration = ifc_syntax_start +
                                         SyntaxSortAsType(TemplateDeclaration),
  ifc_syntax_requires_clause = ifc_syntax_start +
                                              SyntaxSortAsType(RequiresClause),
  ifc_syntax_simple_requirement = ifc_syntax_start +
                                           SyntaxSortAsType(SimpleRequirement),
  ifc_syntax_type_requirement = ifc_syntax_start +
                                             SyntaxSortAsType(TypeRequirement),
  ifc_syntax_compound_requirement = ifc_syntax_start +
                                         SyntaxSortAsType(CompoundRequirement),
  ifc_syntax_nested_requirement = ifc_syntax_start +
                                           SyntaxSortAsType(NestedRequirement),
  ifc_syntax_requirement_body = ifc_syntax_start +
                                             SyntaxSortAsType(RequirementBody),
  ifc_syntax_type_template_parameter = ifc_syntax_start +
                                       SyntaxSortAsType(TypeTemplateParameter),
  ifc_syntax_type_template_argument = ifc_syntax_start +
                                        SyntaxSortAsType(TypeTemplateArgument),
  ifc_syntax_non_type_template_argument = ifc_syntax_start +
                                     SyntaxSortAsType(NonTypeTemplateArgument),
  ifc_syntax_template_parameter_list = ifc_syntax_start +
                                       SyntaxSortAsType(TemplateParameterList),
  ifc_syntax_template_argument_list = ifc_syntax_start +
                                        SyntaxSortAsType(TemplateArgumentList),
  ifc_syntax_template_id = ifc_syntax_start + SyntaxSortAsType(TemplateId),
  ifc_syntax_mem_initializer = ifc_syntax_start +
                                              SyntaxSortAsType(MemInitializer),
  ifc_syntax_ctor_initializer = ifc_syntax_start +
                                             SyntaxSortAsType(CtorInitializer),
  ifc_syntax_lambda_introducer = ifc_syntax_start +
                                            SyntaxSortAsType(LambdaIntroducer),
  ifc_syntax_lambda_declarator = ifc_syntax_start +
                                            SyntaxSortAsType(LambdaDeclarator),
  ifc_syntax_capture_default = ifc_syntax_start +
                                              SyntaxSortAsType(CaptureDefault),
  ifc_syntax_simple_capture = ifc_syntax_start +
                                               SyntaxSortAsType(SimpleCapture),
  ifc_syntax_init_capture = ifc_syntax_start + SyntaxSortAsType(InitCapture),
  ifc_syntax_this_capture = ifc_syntax_start + SyntaxSortAsType(ThisCapture),
  ifc_syntax_attributed_statement = ifc_syntax_start +
                                         SyntaxSortAsType(AttributedStatement),
  ifc_syntax_attributed_declaration = ifc_syntax_start +
                                       SyntaxSortAsType(AttributedDeclaration),
  ifc_syntax_attribute_specifier_seq = ifc_syntax_start +
                                       SyntaxSortAsType(AttributeSpecifierSeq),
  ifc_syntax_attribute_specifier = ifc_syntax_start +
                                          SyntaxSortAsType(AttributeSpecifier),
  ifc_syntax_attribute_using_prefix = ifc_syntax_start +
                                        SyntaxSortAsType(AttributeUsingPrefix),
  ifc_syntax_attribute = ifc_syntax_start + SyntaxSortAsType(Attribute),
  ifc_syntax_attribute_argument_clause = ifc_syntax_start +
                                     SyntaxSortAsType(AttributeArgumentClause),
  ifc_syntax_alignas = ifc_syntax_start + SyntaxSortAsType(Alignas),
  ifc_syntax_using_declaration = ifc_syntax_start +
                                            SyntaxSortAsType(UsingDeclaration),
  ifc_syntax_using_declarator = ifc_syntax_start +
                                             SyntaxSortAsType(UsingDeclarator),
  ifc_syntax_using_directive = ifc_syntax_start +
                                              SyntaxSortAsType(UsingDirective),
  ifc_syntax_array_index = ifc_syntax_start + SyntaxSortAsType(ArrayIndex),
  ifc_syntax_seh_try = ifc_syntax_start + SyntaxSortAsType(SEHTry),
  ifc_syntax_seh_except = ifc_syntax_start + SyntaxSortAsType(SEHExcept),
  ifc_syntax_seh_finally = ifc_syntax_start + SyntaxSortAsType(SEHFinally),
  ifc_syntax_seh_leave = ifc_syntax_start + SyntaxSortAsType(SEHLeave),
  ifc_syntax_type_trait_intrinsic = ifc_syntax_start +
                                          SyntaxSortAsType(TypeTraitIntrinsic),
  ifc_syntax_tuple = ifc_syntax_start + SyntaxSortAsType(Tuple),
  ifc_syntax_asm_statement = ifc_syntax_start + SyntaxSortAsType(AsmStatement),
  ifc_syntax_namespace_alias_definition = ifc_syntax_start +
                                    SyntaxSortAsType(NamespaceAliasDefinition),
  ifc_syntax_super = ifc_syntax_start + SyntaxSortAsType(Super),
  ifc_syntax_unary_fold_expression = ifc_syntax_start +
                                         SyntaxSortAsType(UnaryFoldExpression),
  ifc_syntax_binary_fold_expression = ifc_syntax_start +
                                        SyntaxSortAsType(BinaryFoldExpression),
  ifc_syntax_empty_statement = ifc_syntax_start +
                                              SyntaxSortAsType(EmptyStatement),
  ifc_syntax_structured_binding_declaration = ifc_syntax_start +
                                SyntaxSortAsType(StructuredBindingDeclaration),
  ifc_syntax_structured_binding_identifier = ifc_syntax_start +
                                 SyntaxSortAsType(StructuredBindingIdentifier),
  ifc_syntax_end = ifc_syntax_start + SyntaxSortAsType(Last),
  /* No particular grouping. */
  ifc_cmd_line,
  ifc_const_f64,
  ifc_const_i64,
  ifc_const_str,
  ifc_decl_other_segment,
  ifc_expr_vfunc_conversion,
  ifc_form_spec,
  ifc_heap_chart,
  ifc_heap_decl,
  ifc_heap_expr,
  ifc_heap_stmt,
  ifc_heap_syn,
  ifc_heap_type,
  ifc_msvc_trait_code_segment,
  ifc_msvc_trait_named_func_params,
  ifc_msvc_trait_spec_encodings,
  ifc_msvc_trait_suppressed_warnings,
  ifc_msvc_trait_templ_templ_param_classes,
  ifc_msvc_trait_uuid,
  ifc_msvc_trait_vendor_traits,
  ifc_module_exported,
  ifc_module_imported,
  ifc_pragma_state,
  ifc_scope_desc,
  ifc_scope_member,
  ifc_sentence,
  ifc_src_line,
  ifc_trait_alias_template,
  ifc_trait_class_template,
  ifc_trait_constexpr_function,
  ifc_trait_deprecated,
  ifc_trait_friend,
  ifc_trait_function_template,
  ifc_trait_requires,
  ifc_trait_specialization,
  ifc_trait_variable_template,
  ifc_word,
  ifc_last
};

typedef uint32_t an_ifc_partition_kind;


/*
Utility to return a "tag" given a partition (an_ifc_partition_kind) value
and the starting partition for the particular case (e.g., ifc_type_start
for TypeSort).  Relies on an_ifc_partition_kind being ordered properly (see
the comments there).
*/
#define get_tag_from_partition(partition, start) ((partition) - (start))

/*
Some convenience macros to get module entities of various IFC sorts when
the sort is known by the context.
*/
#define get_decl_module_entity_ptr(decl_index) \
  (get_ifc_module_entity_ptr(ifc_decl_start + decl_tag_as_type((decl_index)), \
                             decl_value((decl_index))))

#define get_type_module_entity_ptr(type_index) \
  (get_ifc_module_entity_ptr(ifc_type_start + type_tag_as_type((type_index)), \
                             type_value((type_index))))

/*
A method for mapping partition names to an_ifc_partition_kind values.  Used
when reading an IFC file to identify which partitions are which.  The list
should be sorted by the partition name.
*/
struct an_ifc_partition_map {
  a_const_char  *name;  /* The name of an IFC partition. */
  an_ifc_partition_kind
                kind;   /* The internal representation of the partition. */
};  /* an_ifc_partition_map */

EXTERN an_ifc_partition_map ifc_partition_map[(int)ifc_last+1]
#if VAR_INITIALIZERS
= {
  { ".msvc.code-segment",              ifc_decl_segment },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.code-segment",        ifc_msvc_trait_code_segment },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.named-function-parameters",
                                       ifc_msvc_trait_named_func_params },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.specialization-encodings",
                                       ifc_msvc_trait_spec_encodings },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.template-template-parameter-classes",
                                    ifc_msvc_trait_templ_templ_param_classes },
  /* Not mentioned in spec.  Found in IFC files. */
  { ".msvc.trait.suppressed-warnings", ifc_msvc_trait_suppressed_warnings },
  { ".msvc.trait.uuid",                ifc_msvc_trait_uuid },
  { ".msvc.trait.vendor-traits",       ifc_msvc_trait_vendor_traits },
  { "chart.multilevel",                ifc_chart_multilevel },
  { "chart.none",                      ifc_chart_none },
  { "chart.unilevel",                  ifc_chart_unilevel },
  /* Not mentioned in spec.  Found in IFC files. */
  { "command_line",                    ifc_cmd_line },
  { "const.f64",                       ifc_const_f64 },
  { "const.i64",                       ifc_const_i64 },
  { "const.str",                       ifc_const_str },
  /* Spec says this is the partition name for DeclSort::Alias, however the IFC
     files themselves still use "decl.type-alias" instead. */
  /*{ "decl.alias",                      ifc_decl_alias },*/
  { "decl.type-alias",                 ifc_decl_alias },
  { "decl.bitfield",                   ifc_decl_bitfield },
  { "decl.concept",                    ifc_decl_concept },
  { "decl.constructor",                ifc_decl_constructor },
  { "decl.destructor",                 ifc_decl_destructor },
  { "decl.enum",                       ifc_decl_enum },
  { "decl.enumerator",                 ifc_decl_enumerator },
  { "decl.explicit-instantiation",     ifc_decl_explicit_instantiation },
  { "decl.explicit-specialization",    ifc_decl_explicit_specialization },
  { "decl.field",                      ifc_decl_field },
  { "decl.friend-declaration",         ifc_decl_friend },
  { "decl.function",                   ifc_decl_function },
  /* Not mentioned in spec.  DeclSort::InheritedConstructor not elaborated. */
  { "decl.inherited-constructor",      ifc_decl_inh_ctor },
  { "decl.intrinsic",                  ifc_decl_intrinsic },
  { "decl.method",                     ifc_decl_method },
  { "decl.parameter",                  ifc_decl_parameter },
  { "decl.partial-specialization",     ifc_decl_partial_specialization },
  { "decl.property",                   ifc_decl_property },
  { "decl.reference",                  ifc_decl_reference },
  { "decl.scope",                      ifc_decl_scope },
  /* Not mentioned in spec.  DeclSort::OutputSegment refers to
    ".msvc.code-segment". */
  { "decl.segment",                    ifc_decl_other_segment },
  { "decl.syntax-tree",                ifc_decl_syntax_tree },
  { "decl.template",                   ifc_decl_template },
  { "decl.temploid",                   ifc_decl_temploid },
  { "decl.tuple",                      ifc_decl_tuple },
  { "decl.using-declaration",          ifc_decl_using_declaration },
  { "decl.using-directive",            ifc_decl_using_directive },
  { "decl.variable",                   ifc_decl_variable },
  { "decl.vendor-extension",           ifc_decl_vendor_extension },
  { "expr.alignof-type-id",            ifc_expr_alignof_type_id },
  /* Not mentioned in spec.  ExprSort::Array is not elaborated. */
  { "expr.array-value",                ifc_expr_array },
  { "expr.assign-initializer",         ifc_expr_assign_initializer },
  { "expr.call",                       ifc_expr_call },
  { "expr.cast",                       ifc_expr_cast },
  /* Not mentioned in spec.  ExprSort::Subobject is not elaborated. */
  { "expr.class-subobject-value",      ifc_expr_subobject },
  { "expr.compound-string",            ifc_expr_compound_string },
  { "expr.condition",                  ifc_expr_condition },
  { "expr.decl",                       ifc_expr_decl },
  { "expr.delete",                     ifc_expr_delete },
  { "expr.destructor-call",            ifc_expr_destructor_call },
  { "expr.dyad",                       ifc_expr_dyad },
  /* Not mentioned in spec.  ExprSort::VirtualFunction is not elaborated. */
  { "expr.dynamic-dispatch",           ifc_expr_virtual_function },
  { "expr.empty",                      ifc_expr_empty },
  { "expr.expression-list",            ifc_expr_expression_list },
  { "expr.function-string",            ifc_expr_function_string },
  { "expr.hierarchy-conversion",       ifc_expr_hierarchy_conversion },
  { "expr.identifier",                 ifc_expr_identifier },
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
  { "expr.pointer",                    ifc_expr_pointer },
  /* Not mentioned in spec.  ExprSort::Product is not elaborated. */
  { "expr.product-type-value",         ifc_expr_product },
  { "expr.push-state",                 ifc_expr_push_state },
  { "expr.qualified-name",             ifc_expr_qualified_name },
  { "expr.read",                       ifc_expr_read },
  /* Not mentioned in spec.  ExprSort::Requires is not elaborated. */
  { "expr.requires-expression",        ifc_expr_requires },
  { "expr.simple-identifier",          ifc_expr_simple_identifier },
  { "expr.sizeof-type-id",             ifc_expr_sizeof_type_id },
  { "expr.string-sequence",            ifc_expr_string_sequence },
  { "expr.strings",                    ifc_expr_strings },
  /* Not mentioned in spec.  ExprSort::Sum is not elaborated. */
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
  /* Not mentioned in spec.  ExprSort::UnaryFold is not elaborated. */
  { "expr.unary-fold-expression",      ifc_expr_unaryfold },
  { "expr.unresolved",                 ifc_expr_unresolved },
  { "expr.vendor-extension",           ifc_expr_vendor_extension },
  /* Not mentioned in spec.  Found in IFC files. */
  { "expr.virtual-function-conversion",ifc_expr_vfunc_conversion },
  /* Not mentioned in spec.  Found in IFC files. */
  { "form.spec",                       ifc_form_spec },
  { "heap.chart",                      ifc_heap_chart },
  { "heap.decl",                       ifc_heap_decl },
  { "heap.expr",                       ifc_heap_expr },
  { "heap.stmt",                       ifc_heap_stmt },
  { "heap.syn",                        ifc_heap_syn },
  { "heap.type",                       ifc_heap_type },
  { "module.exported",                 ifc_module_exported },
  { "module.imported",                 ifc_module_imported },
  { "name.conversion",                 ifc_name_conversion },
  { "name.literal",                    ifc_name_literal },
  { "name.operator",                   ifc_name_operator },
  { "name.source-file",                ifc_name_source_file },
  { "name.specialization",             ifc_name_specialization },
  { "name.template",                   ifc_name_template },
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
  { "syntax.decltype-auto-specifier",  ifc_syntax_decltype_auto_specifier },
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
  { "syntax.vendor-extension",         ifc_syntax_vendor_extension },
  { "syntax.virtual-specifier-seq",    ifc_syntax_virtual_specifier_seq },
  { "syntax.while-statement",          ifc_syntax_while_statement },
  { "trait.alias-template",            ifc_trait_alias_template },
  { "trait.class-template",            ifc_trait_class_template },
  { "trait.constexpr-function",        ifc_trait_constexpr_function },
  { "trait.deprecated",                ifc_trait_deprecated },
  { "trait.friend",                    ifc_trait_friend },
  { "trait.function-template",         ifc_trait_function_template },
  /* Not mentioned in spec.  Found in IFC files. */
  { "trait.requires",                  ifc_trait_requires },
  { "trait.specialization",            ifc_trait_specialization },
  { "trait.variable-template",         ifc_trait_variable_template },
  { "type.array",                      ifc_type_array },
  { "type.base",                       ifc_type_base },
  { "type.decltype",                   ifc_type_decltype },
  { "type.deduced",                    ifc_type_deduced },
  { "type.designated",                 ifc_type_designated },
  { "type.expansion",                  ifc_type_expansion },
  { "type.forall",                     ifc_type_forall },
  { "type.function",                   ifc_type_function },
  { "type.fundamental",                ifc_type_fundamental },
  { "type.lvalue-reference",           ifc_type_lvalue_reference },
  { "type.nonstatic-member-function",  ifc_type_nonstatic_member_function },
  { "type.pointer",                    ifc_type_pointer },
  /* Spec says this is the partition name for TypeSort::LvalueReference,
     however the IFC files themselves use "type.lvalue-reference" instead. */
  /*{ "type.pointer-lvalue-reference",   ifc_type_lvalue_reference },*/
  { "type.pointer-to-member",          ifc_type_pointer_to_member },
  { "type.qualified",                  ifc_type_qualified },
  { "type.rvalue-reference",           ifc_type_rvalue_reference },
  { "type.syntactic",                  ifc_type_syntactic },
  { "type.syntax-tree",                ifc_type_syntax_tree },
  { "type.tuple",                      ifc_type_tuple },
  { "type.typename",                   ifc_type_typename },
  { "type.unaligned",                  ifc_type_unaligned },
  { "type.vendor-extension",           ifc_type_vendor_extension },
  { NULL,                              ifc_last } /* Must be last. */
}
#endif /* VAR_INITIALIZERS */
 ;

/*
An internal representation of an IFC partition.
*/
struct an_ifc_partition {
  a_const_char  *name;  /* The name of the partition in the IFC file. */
  size_t        offset; /* An offset from the beginning of the file to the
                           start of the partition. */
  uint32_t      size;   /* The number of bytes in the partition. */
  uint32_t      entry_size;
                        /* The size of an entry in the partition. */
};  /* an_ifc_partition */


struct a_str_control_block;
struct a_partial_scope_stack_state;
/*
Information specific to an IFC module.
*/
struct an_ifc_module : public a_module_interface {
  an_ifc_File_Header
                header = {};
                        /* The values of an IFC File_Header (byte-swapped if
                           necessary). */
  an_ifc_partition
                partitions[(int)ifc_last+1] = {};
                        /* Information about each of the IFC partitions that
                           could exist in a module file. */
  a_seq_number  *sequence_numbers = NULL;
                        /* An array of sequence numbers, indexed by a
                           NameSort::SourceFile index, yields the sequence
                           number to be used for all items in the associated
                           file.  Could be extended to a range of sequence
                           numbers (but that would require knowing the last
                           line in each file and that's time-consuming to
                           compute).  Dynamically allocated once the number of
                           source files is known. */

  virtual ~an_ifc_module() noexcept = default;

  inline a_boolean is_open() const noexcept override {
    return f_module != NULL;
  }

  a_boolean import(a_module_import_decl_ptr midp) noexcept override;
  void close() noexcept override;
  void pch_reset(a_module_import_decl_ptr midp) noexcept override;

  void process_ifc_declaration(a_module_entity_ptr mep,
                               a_boolean           defer,
                               a_type_ptr          enumeration_type)
                                                                const noexcept;
  void get_definition_of_module_class(a_module_entity_ptr mep,
                                      a_text_buffer       *buffer)
                                                       const noexcept override;

#if DEBUG
  void debug() const noexcept override;
  void db_module_entity(a_module_entity_ptr mep) const noexcept override;
#endif /* DEBUG */

private:
  a_boolean open_and_map_ifc_module_file(a_module_import_decl_ptr midp)
                                                                      noexcept;
  static an_ifc_partition_map *find_ifc_partition(a_const_char *name) noexcept;
  void process_ifc_scope(ifc_ScopeIndex scope_index,
                         a_scope_ptr    scope) const noexcept;
  a_module_entity_ptr get_ifc_module_entity_ptr(
                                       an_ifc_partition_kind partition,
                                       size_t                partition_offset)
                                                                const noexcept;
  a_type_ptr type_for_ifc_type_index(ifc_TypeIndex type_index) const noexcept;
  void source_position_from_locus(a_source_position  *pos,
                                  ifc_SourceLocation *locus) const noexcept;
  a_const_char *string_from_name_index(ifc_NameIndex    name_index,
                                       a_symbol_locator *loc) const noexcept;
  void init_dps(a_decl_parse_state          *dps,
                ifc_SourceLocation          *locus,
                ifc_TypeIndex               type_index,
                ifc_Alignment               alignment,
                ifc_ObjectTraits            traits,
                ifc_MsvcTraits              msvc_traits,
                ifc_BasicSpecifiers         specifiers,
                ifc_Access                  access,
                a_partial_scope_stack_state *psssp) const noexcept;
  void init_locator_from_name(ifc_NameIndex      name_index,
                              ifc_TextOffset     text_offset,
                              ifc_SourceLocation *locus,
                              a_symbol_locator   *loc) const noexcept;
  a_constant_ptr constant_for_expr_index(ifc_ExprIndex expr_index,
                                         a_type_ptr    default_type)
                                                                const noexcept;
  void str_ifc_text_offset(ifc_TextOffset     offset,
                           a_str_control_block *scbp) const noexcept;
  void str_ifc_name_index(ifc_NameIndex       name_index,
                          a_str_control_block *scbp) const noexcept;
  void str_ifc_class_name(ifc_DeclIndex       home_scope,
                          a_str_control_block *scbp) const noexcept;
  void str_ifc_add_number(a_host_large_unsigned value,
                          a_str_control_block   *scbp) const noexcept;
  void str_ifc_source_location(ifc_SourceLocation  *locus,
                               a_str_control_block *scbp) const noexcept;
  void str_ifc_access(ifc_Access          access,
                      a_str_control_block *scbp) const noexcept;
  void str_ifc_alignment(ifc_Alignment       alignment,
                         a_str_control_block *scbp) const noexcept;
  void str_ifc_qualifiers(ifc_Qualifiers      qualifiers,
                          a_str_control_block *scbp) const noexcept;
  void str_ifc_basic_specifiers(ifc_BasicSpecifiers specifiers,
                                a_str_control_block *scbp) const noexcept;
  void str_ifc_object_traits(ifc_ObjectTraits    traits,
                             a_str_control_block *scbp) const noexcept;
  void str_ifc_msvc_traits(ifc_MsvcTraits      traits,
                           a_str_control_block *scbp) const noexcept;
  void str_ifc_function_type_traits(ifc_FunctionTypeTraits traits,
                                    a_str_control_block    *scbp)
                                                                const noexcept;
  void str_ifc_noexcept_specification(ifc_NoexceptSpecification *eh_spec,
                                      a_str_control_block       *scbp)
                                                                const noexcept;
  void str_ifc_function_traits(ifc_FunctionTraits  traits,
                               a_boolean           prefix,
                               a_str_control_block *scbp) const noexcept;
  void str_ifc_expr_index(ifc_ExprIndex       expr_index,
                          a_str_control_block *scbp) const noexcept;
  void str_ifc_scope_index(ifc_ScopeIndex      scope_index,
                           a_str_control_block *scbp) const noexcept;
  void str_ifc_type_index_first_part(ifc_TypeIndex       type_index,
                                     a_str_control_block *scbp) const noexcept;
  void str_ifc_type_index_second_part(ifc_TypeIndex       type_index,
                                      a_str_control_block *scbp)
                                                                const noexcept;
  void str_ifc_type_index(ifc_TypeIndex       type_index,
                          a_str_control_block *scbp) const noexcept;
  void str_ifc_common_decl(ifc_SourceLocation  *locus,
                           ifc_Access          access,
                           ifc_BasicSpecifiers specifiers,
                           ifc_ObjectTraits    traits,
                           ifc_Alignment       alignment,
                           a_str_control_block *scbp) const noexcept;
  void str_ifc_class_definition(an_ifc_DeclSort_Scope *idssp,
                                a_str_control_block   *scbp) const noexcept;
  void str_ifc_declaration(ifc_DeclIndex       decl_index,
                           a_boolean           is_designated_type,
                           a_str_control_block *scbp) const noexcept;
  void str_ifc_statement(ifc_StmtIndex       stmt_index,
                         a_str_control_block *scbp) const noexcept;
  void str_ifc_string_literal(ifc_StringIndex     str_index,
                              a_str_control_block *scbp) const noexcept;
  void str_ifc_name(ifc_NameIndex       name_index,
                    a_str_control_block *scbp) const noexcept;
  void str_ifc_chart(ifc_ChartIndex      chart_index,
                     a_str_control_block *scbp) const noexcept;
  template<typename T>
  void str_ifc_associated_trait(ifc_DeclIndex       decl_index,
                                a_str_control_block *scbp) const noexcept;
  void str_ifc_syntax_node(ifc_SyntaxIndex     syntax_index,
                           a_str_control_block *scbp) const noexcept;
  void str_ifc_sentence(ifc_SentenceIndex   sentence_index,
                        a_str_control_block *scbp) const noexcept;
  void str_ifc_word(ifc_WordIndex       word_index,
                    a_str_control_block *scbp) const noexcept;

#if DEBUG
  void db_ifc_file_header() const noexcept;
#endif /* DEBUG */
};  /* an_ifc_module */

/* Expected instantiations of an_ifc_module::str_ifc_associated_trait<T>: */
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Deprecated>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept;
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Specialization>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept;
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Friend>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept;
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_ConstexprFunction>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept;
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_FunctionTemplate>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept;
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_ClassTemplate>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept;
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_AliasTemplate>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept;
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_VariableTemplate>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept;
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcVendorTrait>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept;
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcUuid>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept;

extern void ifc_modules_one_time_init();

extern void ifc_modules_init();

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef IFC_MODULES_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
