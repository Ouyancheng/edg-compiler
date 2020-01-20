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
typedef uint32_t ifc_Column;
typedef uint32_t ifc_DeclIndex;
typedef uint32_t ifc_EntitySize;
typedef uint32_t ifc_ExprIndex;
typedef uint32_t ifc_Index;
typedef uint32_t ifc_LineNumber;
typedef uint32_t ifc_LineOffset;
typedef uint32_t ifc_LitIndex;
typedef uint32_t ifc_NameIndex;
typedef uint32_t ifc_Offset;
typedef uint32_t ifc_ParameterLevel;
typedef uint32_t ifc_ParameterPosition;
typedef uint32_t ifc_ScopeIndex;
typedef uint32_t ifc_SentenceOffset;
typedef uint32_t ifc_StringIndex;
typedef uint32_t ifc_SyntaxIndex;
typedef uint32_t ifc_TextOffset;
typedef uint32_t ifc_TokenCategory;
typedef uint32_t ifc_TypeIndex;
typedef uint32_t ifc_UniqueID;

/* 16-bit types: */
typedef uint16_t ifc_Alignment;
typedef uint16_t ifc_EHFlags;
typedef uint16_t ifc_FunctionTraits;
typedef uint16_t ifc_ObjectTraits;
typedef uint16_t ifc_OperatorCategory;
typedef uint16_t ifc_PackSize;

/* 8-bit types: */
typedef uint8_t ifc_Abi;
typedef uint8_t ifc_Access;
typedef uint8_t ifc_Architecture;
typedef uint8_t ifc_BasicSpecifiers;
typedef uint8_t ifc_CallingConvention;
typedef uint8_t ifc_FunctionTypeTraits;
typedef uint8_t ifc_NoexceptSort;
typedef uint8_t ifc_Qualifiers;
typedef uint8_t ifc_ScopeTraits;
typedef uint8_t ifc_TypeBasis;
typedef uint8_t ifc_TypePrecision;
typedef uint8_t ifc_TypeSign;
typedef uint8_t ifc_Version;

/* Some IFC fields have "bool" type. */
typedef uint8_t ifc_bool;

/*
Define some IFC structures that are used in nested inside other IFC structures.
These are handled specially here (rather than with the ifc_map.h automated
method) because the nesting can create padding issues on some architectures.
FIXME: See if these can be handled "automatically" as well.
*/
struct ifc_SourceLocation {
  ifc_LineOffset
                line;
  ifc_Column    column;
};  /* ifc_SourceLocation */

struct ifc_Sequence {
  ifc_Index     start;
  ifc_Cardinality
                cardinality;
};  /* ifc_Sequence */

struct ifc_NoexceptSpecification {
  ifc_SentenceOffset  
                words;
  ifc_NoexceptSort
                sort;
  /* Note that there are three bytes of padding here. */
};  /* ifc_NoexceptSpecification */

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
  };

#include "ifc_map.h"  /*lint !e451 included more than once. */

/* Magic numbers that identify the beginning of an IFC file. */
#define IFC_MODULE_MAGIC_1 0x54
#define IFC_MODULE_MAGIC_2 0x51
#define IFC_MODULE_MAGIC_3 0x45
#define IFC_MODULE_MAGIC_4 0x1A

/* Enumeration for Qualifiers. */
enum an_ifc_Qualifier_kind {
  ifc_Qualifier_None     = 0,  
  ifc_Qualifier_Const    = 1 << 0,
  ifc_Qualifier_Volatile = 1 << 1,
  ifc_Qualifier_Restrict = 1 << 2,
};

/* Enumeration for Access specifiers. */
enum an_ifc_Access_kind {
  ifc_Access_None,      /* No access specifier. */
  ifc_Access_Private,   /* "private" for scope member. */
  ifc_Access_Protected, /* "protected" for scope member. */
  ifc_Access_Public,    /* "public" for scope member. */
};

/* Enumeration for BasicSpecifiers. */
enum an_ifc_BasicSpecifiers_kind {
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
enum an_ifc_ObjectTraits_kind {
  ifc_ObjectTraits_None        = 0,
  ifc_ObjectTraits_Constexpr   = 1 << 0,
  ifc_ObjectTraits_Mutable     = 1 << 1,
  ifc_ObjectTraits_ThreadLocal = 1 << 2,
  ifc_ObjectTraits_Comdat      = 1 << 3,
  ifc_ObjectTraits_SelectAny   = 1 << 4,
  ifc_ObjectTraits_Process     = 1 << 5,
  ifc_ObjectTraits_DllExport   = 1 << 6,
  ifc_ObjectTraits_DllImport   = 1 << 7,
  ifc_ObjectTraits_Allocate    = 1 << 8,
};

/* Enumeration for FunctionTraits. */
enum an_ifc_FunctionTraits_kind {
  ifc_FunctionTraits_None         = 0,
  ifc_FunctionTraits_Inline       = 1 << 0,
  ifc_FunctionTraits_ForceInline  = 1 << 1,
  ifc_FunctionTraits_Constexpr    = 1 << 2,
  ifc_FunctionTraits_Explicit     = 1 << 3,
  ifc_FunctionTraits_Virtual      = 1 << 4,
  ifc_FunctionTraits_NoReturn     = 1 << 5,
  ifc_FunctionTraits_Naked        = 1 << 6,
  ifc_FunctionTraits_NoAlias      = 1 << 7,
  ifc_FunctionTraits_NoThrow      = 1 << 8,
  ifc_FunctionTraits_NoInline     = 1 << 9,
  ifc_FunctionTraits_Restrict     = 1 << 10,
  ifc_FunctionTraits_SafeBuffers  = 1 << 11,
  ifc_FunctionTraits_DllExport    = 1 << 12,
  ifc_FunctionTraits_CodeSegment  = 1 << 13,
  ifc_FunctionTraits_PureVirtual  = 1 << 14,
  ifc_FunctionTraits_HiddenFriend = 1 << 15,
};

/* Enumeration for FunctionTypeTraits. */
enum an_ifc_FunctionTypeTraits_kind {
  ifc_FunctionTypeTraits_None     = 0,
  ifc_FunctionTypeTraits_Const    = 1 << 0,
  ifc_FunctionTypeTraits_Volatile = 1 << 1,
  ifc_FunctionTypeTraits_Lvalue   = 1 << 2,
  ifc_FunctionTypeTraits_Rvalue   = 1 << 3,
};

/* Enumeration for NoexceptSpecification. */
enum an_ifc_NoexceptSort_kind {
  ifc_NoexceptSort_None,
  ifc_NoexceptSort_False,
  ifc_NoexceptSort_True,
  ifc_NoexceptSort_Expression,
  ifc_NoexceptSort_Deduced,
};

/* Enumeration for ScopeTraits. */
enum an_ifc_ScopeTraits_kind {
  ifc_ScopeTraits_None          = 0,
  ifc_ScopeTraits_Unnamed       = 1 << 0,
  ifc_ScopeTraits_Inline        = 1 << 1,
  ifc_ScopeTraits_NoVtable      = 1 << 2,
  ifc_ScopeTraits_CodeSegment   = 1 << 3,
  ifc_ScopeTraits_IntrinsicType = 1 << 4,
  ifc_ScopeTraits_EmptyBases    = 1 << 5,
};

/* Macros used to access TypeIndex::tag and TypeIndex::value. */
#define type_tag(type) ((type) & 0x0000001F)
#define type_value(type) ((type) >> 5)

/* Enumeration for TypeSort (i.e., types of types). */
enum an_ifc_TypeSort_kind_tag {
  ifc_TypeSort_VendorExtension,
  ifc_TypeSort_Fundamental,
  ifc_TypeSort_Designated,
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
  ifc_TypeSort_SyntaxTree
};

typedef a_byte an_ifc_TypeSort_kind;

/* Macros used to access ExprIndex::tag and ExprIndex::value. */
#define expr_tag(expr) ((expr) & 0x0000003F)
#define expr_value(expr) ((expr) >> 6)

/* Enumeration for ExprSort (i.e., types of expressions). */
enum an_ifc_ExprSort_kind_tag {
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
};

typedef a_byte an_ifc_ExprSort_kind;

/* Enumeration for TypeBasis (i.e., kinds of fundamental types). */
enum an_ifc_TypeBasis {
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
  ifc_TypeBasis_DecltypeAuto
};

/* Enumeration for TypePrecision (i.e., sizes of fundamental types). */
enum an_ifc_TypePrecision {
  ifc_TypePrecision_Default,
  ifc_TypePrecision_Short,
  ifc_TypePrecision_Long,
  ifc_TypePrecision_Bit16,
  ifc_TypePrecision_Bit32,
  ifc_TypePrecision_Bit64,
  ifc_TypePrecision_Bit128,
};

/* Enumeration for TypeSign (i.e., sign of fundamental types). */
enum an_ifc_TypeSign {
  ifc_TypeSign_Plain,
  ifc_TypeSign_Signed,
  ifc_TypeSign_Unsigned,
};

/* Macros used to access DeclIndex::tag and DeclIndex::value. */
#define decl_tag(decl) ((decl) & 0x0000001F)
#define decl_value(decl) ((decl) >> 5)
#define make_decl_index(tag, idx) \
  ((ifc_DeclIndex)((idx) << 5) | ((tag) & 0x0000001F))

/* Enumeration for DeclSort (i.e., types of declarations). */
enum an_ifc_DeclSort_kind_tag {
  ifc_DeclSort_VendorExtension,
  ifc_DeclSort_Enumerator,
  ifc_DeclSort_Variable,
  ifc_DeclSort_TemplateParameter,
  ifc_DeclSort_FunctionParameter,
  ifc_DeclSort_Field,
  ifc_DeclSort_Bitfield,
  ifc_DeclSort_Scope,
  ifc_DeclSort_Enumeration,
  ifc_DeclSort_TypeAlias,
  ifc_DeclSort_Temploid,
  ifc_DeclSort_Template,
  ifc_DeclSort_PartialSpecialization,
  ifc_DeclSort_ExplicitSpecialization,
  ifc_DeclSort_ExplicitInstantiation,
  ifc_DeclSort_Intrinsic,
  ifc_DeclSort_Function,
  ifc_DeclSort_Method,
  ifc_DeclSort_Constructor,
  ifc_DeclSort_Destructor,
  ifc_DeclSort_Reference,
  ifc_DeclSort_Property,
  ifc_DeclSort_OutputSegment,
  ifc_DeclSort_UsingDeclaration,
  ifc_DeclSort_UsingDirective,
  ifc_DeclSort_SyntaxTree,
  ifc_DeclSort_Tuple
};

typedef a_byte an_ifc_DeclSort_kind;

/* Macros used to access NameIndex::tag and NameIndex::value. */
#define name_tag(name) ((name) & 0x00000007)
#define name_value(name) ((name) >> 3)

/* Enumeration for NameSort (i.e., types of names). */
enum an_ifc_NameSort_kind_tag {
  ifc_NameSort_Identifier,
  ifc_NameSort_Operator,
  ifc_NameSort_Conversion,
  ifc_NameSort_Literal,
  ifc_NameSort_Template,
  ifc_NameSort_Specialization,
  ifc_NameSort_SourceFile
};

typedef a_byte an_ifc_NameSort_kind;

/* Macros used to access LitIndex::tag and LitIndex::index. */
#define literal_tag(litindex) ((litindex) & 0x00000003)
#define literal_index(litindex) ((litindex) >> 2)

/* Enumeration for LiteralSort (i.e., types of literals). */
enum an_ifc_LiteralSort_tag {
  ifc_LiteralSort_Immediate,
  ifc_LiteralSort_Integer,
  ifc_LiteralSort_FloatingPoint,
};

/* Enumeration for OperatorCategory (i.e., types of operations). */
enum an_ifc_OperatorCategory_tag {
  ifc_OperatorCategory_Bitand = 14,                /* operator& */
  ifc_OperatorCategory_LogicAnd = 15,              /* operator&& */
  ifc_OperatorCategory_Assign = 16,                /* operator= */
  ifc_OperatorCategory_Comma = 18,                 /* operator, */
  ifc_OperatorCategory_Not = 19,                   /* operator! */
  ifc_OperatorCategory_Minus = 26,                 /* operator- */
  ifc_OperatorCategory_Star = 27,                  /* operator* */
  ifc_OperatorCategory_Bitor = 28,                 /* operator| */
  ifc_OperatorCategory_LogicOr = 29,               /* operator|| */
  ifc_OperatorCategory_Plus = 30,                  /* operator+ */
  ifc_OperatorCategory_Quest = 31,                 /* operator? */
  ifc_OperatorCategory_Complement = 32,            /* operator~ */
  ifc_OperatorCategory_Caret = 33,                 /* operator^ */
  ifc_OperatorCategory_Slash = 39,                 /* operator/ */
  ifc_OperatorCategory_Modulo = 40,                /* operator% */
  ifc_OperatorCategory_Percent = 41,               /* operator% */
  ifc_OperatorCategory_Sizeof = 62,                /* operator sizeof */
  ifc_OperatorCategory_ExpandingSizeof = 63,       /* operator sizeof... */
  ifc_OperatorCategory_New = 69,                   /* operator new */
  ifc_OperatorCategory_Delete = 70,                /* operator delete */
  ifc_OperatorCategory_Throw = 96,                 /* operator throw */
  ifc_OperatorCategory_Alignof = 99,               /* operator alignof */
  ifc_OperatorCategory_Noexcept = 166,             /* operator noexcept */
  ifc_OperatorCategory_Requires = 174,             /* operator requires */
  ifc_OperatorCategory_Coreturn = 185,             /* operator co_return */
  ifc_OperatorCategory_Await = 186,                /* operator co_yield */
  ifc_OperatorCategory_Yield = 187,                /* operator co_yield */
  ifc_OperatorCategory_StaticAssert = 261,         /* operator static_assert */
  ifc_OperatorCategory_PostIncrement = 336,        /* operator++ */
  ifc_OperatorCategory_PostDecrement = 337,        /* operator-- */
  ifc_OperatorCategory_SlashEq = 338,              /* operator/= */
  ifc_OperatorCategory_EqEq = 339,                 /* operator== */
  ifc_OperatorCategory_NotEq = 340,                /* operator!= */
  ifc_OperatorCategory_Greater = 341,              /* operator> */
  ifc_OperatorCategory_GreaterEq = 342,            /* operator>= */
  ifc_OperatorCategory_Less = 343,                 /* operator< */
  ifc_OperatorCategory_LessEq = 344,               /* operator<= */
  ifc_OperatorCategory_LshiftEq = 345,             /* operator<<= */
  ifc_OperatorCategory_RshiftEq = 346,             /* operator>>= */
  ifc_OperatorCategory_MinusEq = 347,              /* operator-= */
  ifc_OperatorCategory_ModuloEq = 348,             /* operator%= */
  ifc_OperatorCategory_StarEq = 349,               /* operator*= */
  ifc_OperatorCategroy_BitorEq = 350,              /* operator|= */
  ifc_OperatorCategory_PlusEq = 351,               /* operator+= */
  ifc_OperatorCategory_BitandEq = 352,             /* operator&= */
  ifc_OperatorCategory_BitxorEq = 353,             /* operator^= */
  ifc_OperatorCategory_Lshift = 354,               /* operator<< */
  ifc_OperatorCategory_Rshift = 355,               /* operator>> */
  ifc_OperatorCategory_Dot = 356,                  /* operator. */
  ifc_OperatorCategory_Arrow = 357,                /* operator =  */
  ifc_OperatorCategory_PreDecrement = 359,         /* operator-- */
  ifc_OperatorCategory_PreIncrement = 360,         /* operator++ */
  ifc_OperatorCategory_UnaryMinus = 361,           /* operator- */
  ifc_OperatorCategory_Address = 362,              /* operator& */
  ifc_OperatorCategory_UnaryPlus = 381,            /* operator+ */
  ifc_OperatorCategory_DerefMemberAccess = 386,    /* operator.* */
  ifc_OperatorCategory_IndirectMemberAccess = 387, /* operator->* */
};

typedef a_byte an_ifc_LiteralSort_kind;

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
  ifc_decl_vendor_extension = ifc_decl_start + ifc_DeclSort_VendorExtension,
  ifc_decl_enumerator = ifc_decl_start + ifc_DeclSort_Enumerator,
  ifc_decl_variable = ifc_decl_start + ifc_DeclSort_Variable,
  ifc_decl_template_parameter = ifc_decl_start +
                                                ifc_DeclSort_TemplateParameter,
  ifc_decl_function_parameter = ifc_decl_start +
                                                ifc_DeclSort_FunctionParameter,
  ifc_decl_field = ifc_decl_start + ifc_DeclSort_Field,
  ifc_decl_bitfield = ifc_decl_start + ifc_DeclSort_Bitfield,
  ifc_decl_scope = ifc_decl_start + ifc_DeclSort_Scope,
  ifc_decl_enum = ifc_decl_start + ifc_DeclSort_Enumeration,
  ifc_decl_type_alias = ifc_decl_start + ifc_DeclSort_TypeAlias,
  ifc_decl_temploid = ifc_decl_start + ifc_DeclSort_Temploid,
  ifc_decl_template = ifc_decl_start + ifc_DeclSort_Template,
  ifc_decl_partial_specialization = ifc_decl_start +
                                            ifc_DeclSort_PartialSpecialization,
  ifc_decl_explicit_specialization = ifc_decl_start +
                                           ifc_DeclSort_ExplicitSpecialization,
  ifc_decl_explicit_instantiation = ifc_decl_start +
                                            ifc_DeclSort_ExplicitInstantiation,
  ifc_decl_intrinsic = ifc_decl_start + ifc_DeclSort_Intrinsic,
  ifc_decl_function = ifc_decl_start + ifc_DeclSort_Function,
  ifc_decl_method = ifc_decl_start + ifc_DeclSort_Method,
  ifc_decl_constructor = ifc_decl_start + ifc_DeclSort_Constructor,
  ifc_decl_destructor = ifc_decl_start + ifc_DeclSort_Destructor,
  ifc_decl_reference = ifc_decl_start + ifc_DeclSort_Reference,
  ifc_decl_property = ifc_decl_start + ifc_DeclSort_Property,
  ifc_decl_segment = ifc_decl_start + ifc_DeclSort_OutputSegment,
  ifc_decl_using_declaration = ifc_decl_start + ifc_DeclSort_UsingDeclaration,
  ifc_decl_using_directive = ifc_decl_start + ifc_DeclSort_UsingDirective,
  ifc_decl_syntax_tree = ifc_decl_start + ifc_DeclSort_SyntaxTree,
  ifc_decl_tuple = ifc_decl_start + ifc_DeclSort_Tuple,
  /* Group all TypeIndex::tag partitions together. */
  ifc_type_start,
  ifc_type_vendor_extension = ifc_type_start + ifc_TypeSort_VendorExtension,
  ifc_type_fundamental = ifc_type_start + ifc_TypeSort_Fundamental,
  ifc_type_designated = ifc_type_start + ifc_TypeSort_Designated,
  ifc_type_syntactic = ifc_type_start + ifc_TypeSort_Syntactic,
  ifc_type_expansion = ifc_type_start + ifc_TypeSort_Expansion,
  ifc_type_pointer = ifc_type_start + ifc_TypeSort_Pointer,
  ifc_type_pointer_to_member = ifc_type_start + ifc_TypeSort_PointerToMember,
  ifc_type_lvalue_reference = ifc_type_start + ifc_TypeSort_LvalueReference,
  ifc_type_rvalue_reference = ifc_type_start + ifc_TypeSort_RvalueReference,
  ifc_type_function = ifc_type_start + ifc_TypeSort_Function,
  ifc_type_nonstatic_member_function = ifc_type_start + ifc_TypeSort_Method,
  ifc_type_array = ifc_type_start + ifc_TypeSort_Array,
  ifc_type_typename = ifc_type_start + ifc_TypeSort_Typename,
  ifc_type_qualified = ifc_type_start + ifc_TypeSort_Qualified,
  ifc_type_base = ifc_type_start + ifc_TypeSort_Base,
  ifc_type_unaligned = ifc_type_start + ifc_TypeSort_Unaligned,
  ifc_type_decltype = ifc_type_start + ifc_TypeSort_Decltype,
  ifc_type_tuple = ifc_type_start + ifc_TypeSort_Tuple,
  ifc_type_syntax_tree = ifc_type_start + ifc_TypeSort_SyntaxTree,
  /* Group all NameSort::tag partitions together.  Note that there is no
     partition for NameSort::Identifier (ifc_NameSort_Identifier). */
  ifc_name_start,
  ifc_name_operator = ifc_name_start + ifc_NameSort_Operator,
  ifc_name_conversion = ifc_name_start + ifc_NameSort_Conversion,
  ifc_name_literal = ifc_name_start + ifc_NameSort_Literal,
  ifc_name_template = ifc_name_start + ifc_NameSort_Template,
  ifc_name_specialization = ifc_name_start + ifc_NameSort_Specialization,
  ifc_name_source_file = ifc_name_start + ifc_NameSort_SourceFile,
  /* Group all ExprSort::tag partitions together. */
  ifc_expr_start,
  ifc_expr_vendor_extension = ifc_expr_start + ifc_ExprSort_VendorExtension,
  ifc_expr_empty = ifc_expr_start + ifc_ExprSort_Empty,
  ifc_expr_literal = ifc_expr_start + ifc_ExprSort_Literal,
  ifc_expr_type = ifc_expr_start + ifc_ExprSort_Type,
  ifc_expr_decl = ifc_expr_start + ifc_ExprSort_NamedDecl,
  ifc_expr_unresolved = ifc_expr_start + ifc_ExprSort_UnresolvedId,
  ifc_expr_template_id = ifc_expr_start + ifc_ExprSort_TemplateId,
  ifc_expr_identifier = ifc_expr_start + ifc_ExprSort_Identifier,
  ifc_expr_simple_identifier = ifc_expr_start + ifc_ExprSort_SimpleIdentifier,
  ifc_expr_pointer = ifc_expr_start + ifc_ExprSort_Pointer,
  ifc_expr_qualified_name = ifc_expr_start + ifc_ExprSort_QualifiedName,
  ifc_expr_path = ifc_expr_start + ifc_ExprSort_Path,
  ifc_expr_read = ifc_expr_start + ifc_ExprSort_Read,
  ifc_expr_monad = ifc_expr_start + ifc_ExprSort_Monad,
  ifc_expr_dyad = ifc_expr_start + ifc_ExprSort_Dyad,
  ifc_expr_triad = ifc_expr_start + ifc_ExprSort_Triad,
  ifc_expr_tuple = ifc_expr_start + ifc_ExprSort_Tuple,
  ifc_expr_tokens = ifc_expr_start + ifc_ExprSort_Tokens,
  ifc_expr_strings = ifc_expr_start + ifc_ExprSort_String,
  ifc_expr_temporary = ifc_expr_start + ifc_ExprSort_Temporary,
  ifc_expr_call = ifc_expr_start + ifc_ExprSort_Call,
  ifc_expr_push_state = ifc_expr_start + ifc_ExprSort_PushState,
  ifc_expr_type_trait = ifc_expr_start + ifc_ExprSort_TypeTraitIntrinsic,
  ifc_expr_member_initializer = ifc_expr_start +
                                                ifc_ExprSort_MemberInitializer,
  ifc_expr_member_access = ifc_expr_start + ifc_ExprSort_MemberAccess,
  ifc_expr_inheritance_path = ifc_expr_start + ifc_ExprSort_InheritancePath,
  ifc_expr_template_reference = ifc_expr_start +
                                                ifc_ExprSort_TemplateReference,
  ifc_expr_initializer_list = ifc_expr_start + ifc_ExprSort_InitializerList,
  ifc_expr_cast = ifc_expr_start + ifc_ExprSort_Cast,
  ifc_expr_condition = ifc_expr_start + ifc_ExprSort_Condition,
  ifc_expr_expression_list = ifc_expr_start + ifc_ExprSort_ExpressionList,
  ifc_expr_assign_initializer = ifc_expr_start +
                                                ifc_ExprSort_AssignInitializer,
  ifc_expr_nullptr = ifc_expr_start + ifc_ExprSort_Nullptr,
  ifc_expr_this = ifc_expr_start + ifc_ExprSort_This,
  ifc_expr_sizeof_type_id = ifc_expr_start + ifc_ExprSort_SizeofTypeId,
  ifc_expr_alignof_type_id = ifc_expr_start + ifc_ExprSort_Alignof,
  ifc_expr_packed_template_arguments = ifc_expr_start +
                                          ifc_ExprSort_PackedTemplateArguments,
  ifc_expr_new = ifc_expr_start + ifc_ExprSort_New,
  ifc_expr_delete = ifc_expr_start + ifc_ExprSort_Delete,
  ifc_expr_lambda = ifc_expr_start + ifc_ExprSort_Lambda,
  ifc_expr_typeid = ifc_expr_start + ifc_ExprSort_Typeid,
  ifc_expr_destructor_call = ifc_expr_start + ifc_ExprSort_DestructorCall,
  ifc_expr_syntax_tree = ifc_expr_start + ifc_ExprSort_SyntaxTree,
  ifc_expr_function_string = ifc_expr_start + ifc_ExprSort_FunctionString,
  ifc_expr_compound_string = ifc_expr_start + ifc_ExprSort_CompoundString,
  ifc_expr_string_sequence = ifc_expr_start + ifc_ExprSort_StringSequence,
  ifc_expr_initializer = ifc_expr_start + ifc_ExprSort_Initializer,
  /* No particular grouping. */
  ifc_msvc_code_segment,
  ifc_chart_multilevel,
  ifc_chart_unilevel,
  ifc_const_f64,
  ifc_const_i64,
  ifc_const_str,
  ifc_form_spec,
  ifc_heap_chart,
  ifc_heap_decl,
  ifc_heap_expr,
  ifc_heap_spec,
  ifc_heap_stmt,
  ifc_heap_syn,
  ifc_heap_type,
  ifc_module_exported,
  ifc_module_imported,
  ifc_pragma_states,
  ifc_scope_desc,
  ifc_scope_member,
  ifc_src_line,
  ifc_src_sentence,
  ifc_src_word,
  ifc_stmt_block,
  ifc_stmt_break,
  ifc_stmt_case,
  ifc_stmt_continue,
  ifc_stmt_default,
  ifc_stmt_do_while,
  ifc_stmt_empty,
  ifc_stmt_expression,
  ifc_stmt_for,
  ifc_stmt_if,
  ifc_stmt_return,
  ifc_stmt_switch,
  ifc_stmt_syntax_tree,
  ifc_stmt_variable,
  ifc_stmt_vendor_extension,
  ifc_stmt_while,
  ifc_trait_constexpr_function,
  ifc_trait_deprecated,
  ifc_trait_friend,
  ifc_trait_specialization,
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
#define get_decl_module_entity_ptr(mod, decl_index) \
  (get_ifc_module_entity_ptr((mod), \
                             ifc_decl_start + decl_tag((decl_index)), \
                             decl_value((decl_index))))

#define get_type_module_entity_ptr(mod, type_index) \
  (get_ifc_module_entity_ptr((mod), \
                             ifc_type_start + type_tag((type_index)), \
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
  { ".msvc.code-segment",              ifc_msvc_code_segment },
  { "chart.multilevel",                ifc_chart_multilevel },
  { "chart.unilevel",                  ifc_chart_unilevel },
  { "const.f64",                       ifc_const_f64 },
  { "const.i64",                       ifc_const_i64 },
  { "const.str",                       ifc_const_str },
  { "decl.bitfield",                   ifc_decl_bitfield },
  { "decl.constructor",                ifc_decl_constructor },
  { "decl.destructor",                 ifc_decl_destructor },
  { "decl.enum",                       ifc_decl_enum },
  { "decl.enumerator",                 ifc_decl_enumerator },
  { "decl.explicit-instantiation",     ifc_decl_explicit_instantiation },
  { "decl.explicit-specialization",    ifc_decl_explicit_specialization },
  { "decl.field",                      ifc_decl_field },
  { "decl.function",                   ifc_decl_function },
  { "decl.function-parameter",         ifc_decl_function_parameter },
  { "decl.intrinsic",                  ifc_decl_intrinsic },
  { "decl.method",                     ifc_decl_method },
  { "decl.partial-specialization",     ifc_decl_partial_specialization },
  { "decl.property",                   ifc_decl_property },
  { "decl.reference",                  ifc_decl_reference },
  { "decl.scope",                      ifc_decl_scope },
  { "decl.segment",                    ifc_decl_segment },
  { "decl.syntax-tree",                ifc_decl_syntax_tree },
  { "decl.template",                   ifc_decl_template },
  { "decl.template-parameter",         ifc_decl_template_parameter },
  { "decl.temploid",                   ifc_decl_temploid },
  { "decl.tuple",                      ifc_decl_tuple },
  { "decl.type-alias",                 ifc_decl_type_alias },
  { "decl.using-declaration",          ifc_decl_using_declaration },
  { "decl.using-directive",            ifc_decl_using_directive },
  { "decl.variable",                   ifc_decl_variable },
  { "decl.vendor-extension",           ifc_decl_vendor_extension },
  { "expr.alignof-type-id",            ifc_expr_alignof_type_id },
  { "expr.assign-initializer",         ifc_expr_assign_initializer },
  { "expr.call",                       ifc_expr_call },
  { "expr.cast",                       ifc_expr_cast },
  { "expr.compound-string",            ifc_expr_compound_string },
  { "expr.condition",                  ifc_expr_condition },
  { "expr.decl",                       ifc_expr_decl },
  { "expr.delete",                     ifc_expr_delete },
  { "expr.destructor-call",            ifc_expr_destructor_call },
  { "expr.dyad",                       ifc_expr_dyad },
  { "expr.empty",                      ifc_expr_empty },
  { "expr.expression-list",            ifc_expr_expression_list },
  { "expr.function-string",            ifc_expr_function_string },
  { "expr.identifier",                 ifc_expr_identifier },
  { "expr.inheritance-path",           ifc_expr_inheritance_path },
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
  { "expr.push-state",                 ifc_expr_push_state },
  { "expr.qualified-name",             ifc_expr_qualified_name },
  { "expr.read",                       ifc_expr_read },
  { "expr.simple-identifier",          ifc_expr_simple_identifier },
  { "expr.sizeof-type-id",             ifc_expr_sizeof_type_id },
  { "expr.string-sequence",            ifc_expr_string_sequence },
  { "expr.strings",                    ifc_expr_strings },
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
  { "expr.unresolved",                 ifc_expr_unresolved },
  { "expr.vendor-extension",           ifc_expr_vendor_extension },
  { "form.spec",                       ifc_form_spec },
  { "heap.chart",                      ifc_heap_chart },
  { "heap.decl",                       ifc_heap_decl },
  { "heap.expr",                       ifc_heap_expr },
  { "heap.spec",                       ifc_heap_spec },
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
  { "pragma.state",                    ifc_pragma_states },
  { "scope.desc",                      ifc_scope_desc },
  { "scope.member",                    ifc_scope_member },
  { "src.line",                        ifc_src_line },
  { "src.sentence",                    ifc_src_sentence },
  { "src.word",                        ifc_src_word },
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
  { "trait.constexpr-function",        ifc_trait_constexpr_function },
  { "trait.deprecated",                ifc_trait_deprecated },
  { "trait.friend",                    ifc_trait_friend },
  { "trait.specialization",            ifc_trait_specialization },
  { "type.array",                      ifc_type_array },
  { "type.base",                       ifc_type_base },
  { "type.decltype",                   ifc_type_decltype },
  { "type.designated",                 ifc_type_designated },
  { "type.expansion",                  ifc_type_expansion },
  { "type.function",                   ifc_type_function },
  { "type.fundamental",                ifc_type_fundamental },
  { "type.lvalue-reference",           ifc_type_lvalue_reference },
  { "type.nonstatic-member-function",  ifc_type_nonstatic_member_function },
  { "type.pointer",                    ifc_type_pointer },
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

/*
Information specific to an IFC module.
*/
struct an_ifc_module : public a_module_interface {
  virtual ~an_ifc_module() noexcept = default;

  a_boolean import(a_module_import_decl_ptr midp) noexcept override;

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
};  /* an_ifc_module */

extern void process_ifc_declaration(a_module_entity_ptr mep,
                                    a_boolean           defer,
                                    a_type_ptr          enumeration_type);

#if DEBUG
extern void db_ifc_file_header(a_module_ptr mod);
#endif /* DEBUG */

extern void get_definition_of_module_class_from_ifc(
                                                  a_module_entity_ptr mep,
                                                  a_text_buffer       *buffer);

extern void ifc_modules_pch_reset(a_module_import_decl_ptr midp);

extern void close_ifc_module_file(a_module_import_decl_ptr midp);

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
