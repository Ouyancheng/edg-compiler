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

ifc_modules.c -- Microsoft-specific IFC module code

*/

#include "basic_hdrs.h"
#include "fe_common.h"
#include "ifc_modules.h"
#include "decl_spec.h"
#include "symbol_ref.h"
#include "class_decl.h"
#include "pch.h"

#if MICROSOFT_EXTENSIONS_ALLOWED

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Macro that is TRUE if the host has big-endian byte ordering.
FIXME: Move this, set automatically, add to dump_config, dettarg
*/
#ifndef HOST_BIG_ENDIAN
#define HOST_BIG_ENDIAN FALSE
#endif /* HOST_BIG_ENDIAN */

/*
When set to FALSE, the macros that access the bytes of the IFC file are
conservative in that they access the file one byte at a time and optionally
swap bytes as needed.  When TRUE, it is assumed that the host's architecture
and layout match those of the machine on which the IFC file was created and
data is accessed directly from a memory-mapped version of the IFC file.
The safe value is FALSE.
FIXME: Move this and add to dump_config
*/
#ifndef USE_MMAP_POINTERS_TO_IFC
#define USE_MMAP_POINTERS_TO_IFC FALSE
#endif /* USE_MMAP_POINTERS_TO_IFC */

#if USE_MMAP_POINTERS_TO_IFC && HOST_BIG_ENDIAN
  #error USE_MMAP_POINTERS_TO_IFC cannot be TRUE on big-endian host
#endif /* USE_MMAP_POINTERS_TO_IFC && HOST_BIG_ENDIAN */

/*
A control block used when turning IFC declarations into a textual
representation.
*/
struct a_str_control_block {
  a_module_ptr  module_info;
                        /* The module being traversed. */
  a_text_buffer_ptr
                text_buffer;
                        /* The text buffer into which the generated text is
                           placed. */
  a_byte_boolean
                is_generated_code;
                        /* A flag to indicate whether the textual
                           representation being generated will be consumed
                           by the front end (otherwise it's just for debug
                           output). */
};  /* a_str_control_block */


static a_text_buffer_ptr
                top_ifc_text_buffer;
                        /* A static text buffer into which a textual
                           representation is placed. */

static a_text_buffer_ptr
		ifc_file_name_text_buffer;
			/* A text buffer used to build IFC file names. */

static a_text_buffer_ptr
		operator_text_buffer;
			/* A text buffer used to prefix operator names. */


/*
The routines and data structures below are used to support host-independent
access to the fields of an IFC file regardless of endianness, padding, or
alignment issues.
*/

static unsigned char
		*byte_buffer;
			/* Pointer to the current position in the buffer
			   used by get_byte, etc. */

static unsigned char
		*buffer_end;
			/* Pointer to the last byte of the buffer used by
			   get_byte, etc. */

#if DEBUG
static const an_ifc_partition
                *debug_partition;
                        /* Points to information about the partition currently
                           being read (for debugging purposes only). */
static const a_module
                *debug_mod;
                        /* Points to information about the module currently
                           being read (for debugging purposes only). */
#endif /* DEBUG */


static unsigned char buffer_overrun(void)
/*
This routine is called if a memory buffer (which represents a portion of
a module file) terminates prematurely.  Issue a catastrophic error.
This routine returns an unsigned char so it can be used in a ?: operation
that returns an unsigned char.
*/
{
  unexpected_condition();
  return 0;
}  /* buffer_overrun */


static void init_byte_buffer(void*	memory,
			     size_t	length)
/*
Initialize the state information used by "get_bytes", etc.  "memory"
is the start of the buffer to be read.  "length" is its size, in bytes.
*/
{
  byte_buffer = (unsigned char*)memory;
  buffer_end = byte_buffer + length - 1;
}  /* init_byte_buffer */


/*
Macro to fetch a byte from a memory buffer and check for reading past
the end of the buffer.
*/
#define get_byte_from_buffer()						\
  (byte_buffer <= buffer_end ? *byte_buffer++ : buffer_overrun())


static void get_bytes_from_buffer(void		*addr,
				  size_t	length)
/*
Fetch a block of bytes from a memory buffer, and check for reading
past the end of the buffer.
*/
{
  /* Check for fetching too many bytes. */
  if (((unsigned char*)byte_buffer + length - 1) > buffer_end) {
    (void)buffer_overrun();
  }  /* if */
  memcpy(addr, byte_buffer, length);
  byte_buffer += length;
}  /* get_bytes_from_buffer */


/*
Macro to fetch a byte from memory.
*/
#define get_byte(byte)							\
{									\
  int	ch;								\
  ch = get_byte_from_buffer();						\
  *byte = ch;								\
}  /* get_byte */

/*
Macro to get a sequence of bytes from memory.
*/
#define get_bytes(value, length)					\
  get_bytes_from_buffer((void*)value, (size_t)length)

/*
Get "length" bytes in big-endian form and convert them to the host byte
order.
*/
#if HOST_BIG_ENDIAN
#define get_big_endian_bytes(entity, length)				\
  f_get_big_endian_bytes((void*)entity, length)

static void f_get_big_endian_bytes(void		*entity,
			           size_t	length)
/*
Get "length" bytes from memory and convert them to the host byte order.
This routine is used only when the host byte order is big-endian.
*/
{
  unsigned char	*ptr;

  /* Get the bytes in reverse order into "entity". */
  for (ptr = (unsigned char*)entity + length - 1; length > 0;
       length--, ptr--) {
    get_byte(ptr);
  }  /* for */
}  /* f_get_big_endian_bytes */

#else /* !HOST_BIG_ENDIAN */
#define get_big_endian_bytes(entity, length)				\
  get_bytes(entity, length)
#endif /* !HOST_BIG_ENDIAN */

/*
Verify that the variable being used to read a value is the same size as the
value being read.
*/
#if EXPENSIVE_CHECKING
#define check_size(variable, size) check_assertion(sizeof(variable) == size),
#else /* !EXPENSIVE_CHECKING */
#define check_size(variable, size) /**/
#endif /* EXPENSIVE_CHECKING */

/*
Debug hook to print all accesses to IFC partitions.
*/
#if DEBUG && EXPENSIVE_CHECKING
#define db_get_byte(value_str, addr, len) ,f_db_get_byte(value_str, addr, len)

static void f_db_get_byte(a_const_char *value_str,
                          void         *addr,
                          size_t       length)
/*
Utility to print some debug information for every access to an IFC module file.
*/
{
  if (db_flag_is_set("ifc_modules")) {
    if (debug_partition != NULL) {
      (void)fprintf(f_debug, "[%s:0x%08lx:%d] = ",
                    debug_partition->name,
                    ((char *)byte_buffer -
                     ((char *)(debug_mod->module_interface->mmap_addr) +
                      debug_partition->offset)
                     - length),
                    (int)length);
    }  /* if */
    switch (length) {
      case 1:
        (void)fprintf(f_debug, "0x%02x", *((uint8_t*)addr));
        break;
      case 2:
        (void)fprintf(f_debug, "0x%04x", *((uint16_t*)addr));
        break;
      case 4:
        (void)fprintf(f_debug, "0x%08x", *((uint32_t*)addr));
        break;
      case 8:
        (void)fprintf(f_debug, "0x%08x%08x", *((uint32_t*)addr),
                      *(((uint32_t*)addr)+1));
        break;
      case 32:
        (void)fprintf(f_debug, "%08x%08x%08x%08x", *((uint32_t*)addr),
                      *(((uint32_t*)addr)+1), *(((uint32_t*)addr)+2),
                      *(((uint32_t*)addr)+3));
        break;
      default:
        unexpected_condition();
    }  /* switch */
    (void)fprintf(f_debug, " (%s)\n", value_str);
  }  /* if */
}  /* f_db_get_byte */

#else /* !(DEBUG && EXPENSIVE_CHECKING) */
#define db_get_byte(value_str, addr, len) /*nothing*/
#endif /* DEBUG && EXPENSIVE_CHECKING */

/*
Macros used to retrieve the next 1, 2, 4, or 8 bytes from the current buffer
(as set by init_byte_buffer, which is typically called through
read_ifc_partition_at_offset or read_ifc_partition_at_index), in the proper
endianness.  These macros have the GET capitalized as a visual indicator that
they change the value of their argument.
*/
/* FIXME: Header/TOC is little-endian, partitions use big-endian */
#define GET_byte(value)						        \
  (check_size(value, 1)                                                 \
   get_big_endian_bytes(&(value), 1)                                    \
   db_get_byte(stringize(value), &(value), 1))

#define GET_short(value)						\
  (check_size(value, 2)                                                 \
   get_big_endian_bytes(&(value), 2)                                    \
   db_get_byte(stringize(value), &(value), 2))

#define GET_int(value)							\
  (check_size(value, 4)                                                 \
   get_big_endian_bytes(&(value), 4)                                    \
   db_get_byte(stringize(value), &(value), 4))

#define GET_64bit_int(value)						\
  (check_size(value, 8)                                                 \
   get_big_endian_bytes(&(value), 8)                                    \
   db_get_byte(stringize(value), &(value), 8))

#define GET_256bit_int(value)                                           \
  (check_size(value, 32)                                                \
   get_big_endian_bytes(&(value), 32)                                   \
   db_get_byte(stringize(value), &(value), 32))


/*
For what appear to be nested structures in the IFC specification, if the
inner structures aren't a multiple of four bytes, padding is present in the
IFC file and must be explicitly skipped when accessing the bytes linearly.
*/
#define pad(bytes) (byte_buffer += (bytes))

/*
For each fundamental type in an IFC module, define a macro to interpret the
next set of bytes as that type.  These macros have the GET capitalized as a
visual indicator that they change the value of their argument.

Handle nested structures differently (and check for padding).
*/

#define GET_ByteOffset(x)         GET_int(x)
#define GET_Cardinality(x)        GET_int(x)
#define GET_ChartIndex(x)         GET_int(x)
#define GET_Column(x)             GET_int(x)
#define GET_DeclIndex(x)          GET_int(x)
#define GET_EntitySize(x)         GET_int(x)
#define GET_ExprIndex(x)          GET_int(x)
#define GET_Index(x)              GET_int(x)
#define GET_LanguageVersion(x)    GET_int(x)
#define GET_LineIndex(x)          GET_int(x)
#define GET_LineNumber(x)         GET_int(x)
#define GET_LitIndex(x)           GET_int(x)
#define GET_MsvcTraits(x)         GET_int(x)
#define GET_NameIndex(x)          GET_int(x)
#define GET_Offset(x)             GET_int(x)
#define GET_ParameterLevel(x)     GET_int(x)
#define GET_ParameterPosition(x)  GET_int(x)
#define GET_ScopeIndex(x)         GET_int(x)
#define GET_SentenceIndex(x)      GET_int(x)
#define GET_StmtIndex(x)          GET_int(x)
#define GET_StringIndex(x)        GET_int(x)
#define GET_SyntaxIndex(x)        GET_int(x)
#define GET_TextOffset(x)         GET_int(x)
#define GET_TypeIndex(x)          GET_int(x)
#define GET_UniqueID(x)           GET_int(x)
#define GET_UnitIndex(x)          GET_int(x)
#define GET_WordIndex(x)          GET_int(x)

#define GET_Alignment(x)          GET_short(x)
#define GET_EHFlags(x)            GET_short(x)
#define GET_FunctionTraits(x)     GET_short(x)
#define GET_OperatorCategory(x)   GET_short(x)
#define GET_PackSize(x)           GET_short(x)
#define GET_WordCategory(x)       GET_short(x)

#define GET_Abi(x)                GET_byte(x)
#define GET_Access(x)             GET_byte(x)
#define GET_Architecture(x)       GET_byte(x)
#define GET_BasicSpecifiers(x)    GET_byte(x)
#define GET_CallingConvention(x)  GET_byte(x)
#define GET_FunctionTypeTraits(x) GET_byte(x)
#define GET_NoexceptSort(x)       GET_byte(x)
#define GET_ObjectTraits(x)       GET_byte(x)
#define GET_ParameterSort(x)      GET_byte(x)
#define GET_Qualifiers(x)         GET_byte(x)
#define GET_ReadConversionSort(x) GET_byte(x)
#define GET_ScopeTraits(x)        GET_byte(x)
#define GET_SyntaxSort(x)         GET_byte(x)
#define GET_TypeBasis(x)          GET_byte(x)
#define GET_TypePrecision(x)      GET_byte(x)
#define GET_TypeSign(x)           GET_byte(x)
#define GET_Version(x)            GET_byte(x)

#define GET_bool(x)               GET_byte(x)
#define GET_uint8_t(x)            GET_byte(x)
#define GET_uint16_t(x)           GET_short(x)

#define GET_Checksum(x)           GET_256bit_int(x)

#define GET_Sequence(x)        (GET_Index((x).start), \
                                GET_Cardinality((x).cardinality))
#define GET_ModuleReference(x) (GET_TextOffset((x).owner), \
                                GET_TextOffset((x).partition))
#define GET_SourceLocation(x)  (GET_LineIndex((x).line), \
                                GET_Column((x).column))
/* Note that this includes padding: */
#define GET_NoexceptSpecification(x) (GET_SentenceIndex((x).words), \
                                      GET_NoexceptSort((x).sort), \
                                      pad(3))
#define GET_ParameterizedEntity(x) (GET_Index((x).index), \
                                    GET_SentenceIndex((x).head), \
                                    GET_SentenceIndex((x).body), \
                                    GET_SentenceIndex((x).attributes))

/* Utility to save the current partition (for display during debugging). */
#if DEBUG
#define set_debug_partition(kind) \
  (debug_mod = assoc_module_info, \
   debug_partition = &partitions[(kind)]),
#else /* !DEBUG */
#define set_debug_partition(mod, kind) /**/
#endif /* DEBUG */


/*
Returns the IFC file offset of the specified index into the "kind" partition
of the module specified by "mod".  This value is used as a key to uniquely
identify the IFC entity (and is purposely not kept as a pointer because
the underlying address space is typically memory-mapped and could change
during PCH file processing).
*/
#define full_ifc_file_offset_of(kind, idx) \
   (partitions[kind].offset + \
    (idx) * partitions[(kind)].entry_size)

#if EXPENSIVE_CHECKING
#define ifc_file_offset_of(kind, _offset) \
  (check_assertion(partitions[(kind)].offset != 0 && \
                   partitions[(kind)].size != 0 && \
                   partitions[(kind)].size > (_offset)), \
   full_ifc_file_offset_of(kind, _offset))
#else /* !EXPENSIVE_CHECKING */
#define ifc_file_offset_of(kind, offset) \
 full_ifc_file_offset_of(kind, offset)
#endif /* EXPENSIVE_CHECKING */

/*
Utilities to set the buffer to the specified partition at the desired offset
or index, in preparation for a call to GET_byte, GET_short, or GET_int.
*/
#define read_ifc_partition_at_offset(kind, offset) \
  (set_debug_partition(kind) \
   init_byte_buffer(((char*)mmap_addr + (offset)), \
                    partitions[kind].size))

#define read_ifc_partition_at_index(kind, idx) \
  read_ifc_partition_at_offset((kind),\
                               ifc_file_offset_of((kind), (idx)))


/*
Create get_* functions (which "read" each entity into a structure) for each of
the IFC entities by setting the IFC_DECL macros appropriately and including
ifc_map.h.  Make sure to use the pointer that is returned by these functions
(and not the pointer that is passed as an argument).
*/

#if USE_MMAP_POINTERS_TO_IFC
/*
In this configuration, the file layout and the alignment/padding of the host
must have exactly the same characteristics.  Simply return a pointer to a
suitably-cast byte_buffer and increment it as appropriate (the local storage
argument, ptr, is unused in this scenario).

For example, when "name" is "foo", this routine effectively boils down to:

  static an_ifc_foo *get_foo(an_ifc_foo *ptr) {
    ptr = (an_ifc_foo*)byte_buffer;
    byte_buffer = byte_buffer + sizeof(an_ifc_foo);
    return ptr;
  }
*/
#define IFC_DECL_START(name) \
  /*ARGSUSED*/ \
  static concat(an_ifc_, name) * concat(get_, name) \
                                                 (concat(an_ifc_, name) *ptr) \
  { \
    ptr = ( concat(an_ifc_, name) *)byte_buffer; \
    byte_buffer = byte_buffer + sizeof(concat(an_ifc_, name)); \
    check_assertion(byte_buffer <= (buffer_end+1));
#define IFC_DECL_FIELD(field, type) /**/
#define IFC_DECL_END(name) \
    return ptr; \
  }
#else /* !USE_MMAP_POINTERS_TO_IFC */
/*
Use this case when each field must be treated separately because of endianness
or alignment/padding differences.  In this case, the storage passed in to the
function is used to store copies of each field of the structure and each field
is individually copied (and byte-swapped if necessary).

For example, when "name" is "foo", and the macro is applied to an entry with
two fields, "field1" and "field2", whose types are "field1_type" and
"field2_type" respectively, the following is generated:

  static an_ifc_foo *get_foo(an_ifc_foo *ptr) {
    GET_field1_type(ptr->field1);
    GET_field2_type(ptr->field2);
    return ptr;
  }
*/
#define IFC_DECL_START(name) \
  static concat(an_ifc_, name) * concat(get_, name) \
                                                 (concat(an_ifc_, name) *ptr) \
  {
#define IFC_DECL_FIELD(field, type) \
    concat(GET_, type)(ptr->field);
#define IFC_DECL_END(name) \
    return ptr; \
  }
#endif /* USE_MMAP_POINTERS_TO_IFC */

#include "ifc_map.h"  /*lint !e451 included more than once. */

/*
A macro to return a pointer to the IFC string table for a given IFC module
file and TextOffset.  Strings in the IFC file are NULL-terminated.
FIXME: may want to store mmap_addr + string_table_bytes somewhere
*/
#if EXPENSIVE_CHECKING
#define verify_offset(offset) \
  (check_assertion((offset) < header.string_table_size)),
#else /* !EXPENSIVE_CHECKING */
#define verify_offset(mod, offset) /**/
#endif /* EXPENSIVE_CHECKING */
#define get_string_at_offset(offset) \
  (verify_offset(offset) \
   (a_const_char*)((char*)(mmap_addr) + \
    header.string_table_bytes + (offset)))

#if DEBUG

static a_const_char *db_decl_tag(ifc_DeclSort tag)
/*
Return a string with the name that corresponds to the DeclSort tag.
*/
{
  a_const_char *result;

  switch (tag) {
    case DeclSort(VendorExtension):   result = "VendorExtension"; break;
    case DeclSort(Enumerator):        result = "Enumerator"; break;
    case DeclSort(Variable):          result = "Variable"; break;
    case DeclSort(Parameter):         result = "Parameter"; break;
    case DeclSort(Field):             result = "Field"; break;
    case DeclSort(Bitfield):          result = "Bitfield"; break;
    case DeclSort(Scope):             result = "Scope"; break;
    case DeclSort(Enumeration):       result = "Enumeration"; break;
    case DeclSort(Alias):             result = "Alias"; break;
    case DeclSort(Temploid):          result = "Temploid"; break;
    case DeclSort(Template):          result = "Template"; break;
    case DeclSort(PartialSpecialization):
                                       result = "PartialSpecialization"; break;
    case DeclSort(ExplicitSpecialization):
                                       result = "ExplicitSpecialization";break;
    case DeclSort(ExplicitInstantiation):
                                       result = "ExplicitInstantiation"; break;
    case DeclSort(Concept):           result = "Concept"; break;
    case DeclSort(Intrinsic):         result = "Intrinsic"; break;
    case DeclSort(Function):          result = "Function"; break;
    case DeclSort(Method):            result = "Method"; break;
    case DeclSort(Constructor):       result = "Constructor"; break;
    case DeclSort(InheritedConstructor):
                                        result = "InheritedConstructor"; break;
    case DeclSort(Destructor):        result = "Destructor"; break;
    case DeclSort(Reference):         result = "Reference"; break;
    case DeclSort(Property):          result = "Property"; break;
    case DeclSort(OutputSegment):     result = "OutputSegment"; break;
    case DeclSort(UsingDeclaration):  result = "UsingDeclaration"; break;
    case DeclSort(UsingDirective):    result = "UsingDirective"; break;
    case DeclSort(Friend):            result = "Friend"; break;
    case DeclSort(SyntaxTree):        result = "SyntaxTree"; break;
    case DeclSort(Tuple):             result = "Tuple"; break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* db_decl_tag */

#endif /* DEBUG */

static void clear_str_control_block(a_str_control_block *scbp,
                                    a_module_ptr        mod,
                                    a_text_buffer       *text_buffer)
/*
Initialize the string control block pointed to by scbp.  The string being
constructed is for the module referred to by mod.  If non-NULL, text_buffer
is used to capture the string, otherwise a static buffer is used.
*/
{
  if (text_buffer == NULL) {
    if (top_ifc_text_buffer == NULL) {
      top_ifc_text_buffer = alloc_text_buffer(1024);
    }  /* if */
    text_buffer = top_ifc_text_buffer;
  }  /* if */
  reset_text_buffer(text_buffer);
  scbp->module_info = mod;
  scbp->text_buffer = text_buffer;
  scbp->is_generated_code = FALSE;
}  /* clear_str_control_block */


static an_opname_kind opname_from_category(ifc_OperatorCategory category)
/*
Map an IFC OperatorCategory to an_opname_kind.
*/
{
  an_opname_kind op;

  /* FIXME: Note that some of these get mapped to the same entry. */
  switch (category) {
    case OperatorCategory(Bitand):          op = onk_ampersand; break;
    case OperatorCategory(LogicAnd):        op = onk_and_and; break;
    case OperatorCategory(Assign):          op = onk_assign; break;
    case OperatorCategory(Comma):           op = onk_comma; break;
    case OperatorCategory(Not):             op = onk_not; break;
    case OperatorCategory(Minus):           op = onk_minus; break;
    case OperatorCategory(Star):            op = onk_star; break;
    case OperatorCategory(Bitor):           op = onk_or; break;
    case OperatorCategory(LogicOr):         op = onk_or_or; break;
    case OperatorCategory(Plus):            op = onk_plus; break;
    case OperatorCategory(Quest):           op = onk_question; break;
    case OperatorCategory(Complement):      op = onk_compl; break;
    case OperatorCategory(Caret):           op = onk_excl_or; break;
    case OperatorCategory(Slash):           op = onk_divide; break;
    case OperatorCategory(Modulo):          op = onk_remainder; break;
    case OperatorCategory(New):             op = onk_new; break;
    case OperatorCategory(Delete):          op = onk_delete; break;
    case OperatorCategory(IndirectMemberAccess):
                                            op = onk_arrow_star; break;
    case OperatorCategory(PostIncrement):   op = onk_plus_plus; break;
    case OperatorCategory(PostDecrement):   op = onk_minus_minus; break;
    case OperatorCategory(SlashEq):         op = onk_divide_assign; break;
    case OperatorCategory(EqEq):            op = onk_eq; break;
    case OperatorCategory(NotEq):           op = onk_ne; break;
    case OperatorCategory(Greater):         op = onk_gt; break;
    case OperatorCategory(GreaterEq):       op = onk_ge; break;
    case OperatorCategory(Less):            op = onk_lt; break;
    case OperatorCategory(LessEq):          op = onk_le; break;
    case OperatorCategory(LshiftEq):        op = onk_shift_left_assign; break;
    case OperatorCategory(RshiftEq):        op = onk_shift_right_assign; break;
    case OperatorCategory(MinusEq):         op = onk_minus_assign; break;
    case OperatorCategory(ModuloEq):        op = onk_remainder_assign; break;
    case OperatorCategory(StarEq):          op = onk_times_assign; break;
    case OperatorCategory(BitorEq):         op = onk_or_assign; break;
    case OperatorCategory(PlusEq):          op = onk_plus_assign; break;
    case OperatorCategory(BitandEq):        op = onk_and_assign; break;
    case OperatorCategory(BitxorEq):        op = onk_excl_or_assign; break;
    case OperatorCategory(Lshift):          op = onk_shift_left; break;
    case OperatorCategory(Rshift):          op = onk_shift_right; break;
    case OperatorCategory(Arrow):           op = onk_arrow; break;
    case OperatorCategory(PreDecrement):    op = onk_minus_minus; break;
    case OperatorCategory(PreIncrement):    op = onk_plus_plus; break;
    case OperatorCategory(UnaryMinus):      op = onk_minus; break;
    case OperatorCategory(Address):         op = onk_ampersand; break;
    case OperatorCategory(UnaryPlus):       op = onk_plus; break;

    /* These don't have direct mappings: */
    case OperatorCategory(Percent):              /*    operator% */
    case OperatorCategory(Sizeof):               /*    operator sizeof */
    case OperatorCategory(ExpandingSizeof):      /*    operator sizeof... */
    case OperatorCategory(Throw):                /*    operator throw */
    case OperatorCategory(Alignof):              /*    operator alignof */
    case OperatorCategory(Noexcept):             /*    operator noexcept */
    case OperatorCategory(Requires):             /*    operator requires */
    case OperatorCategory(Coreturn):             /*    operator co_return */
    case OperatorCategory(Await):                /*    operator co_yield */
    case OperatorCategory(Yield):                /*    operator co_yield */
    case OperatorCategory(StaticAssert):         /*    operator static_assert*/
    case OperatorCategory(Dot):                  /*    operator. */
    case OperatorCategory(DerefMemberAccess):    /*    operator.* */
    default:
#if DEBUG
      if (db_flag_is_set("ms_ignore")) {
        (void)fprintf(f_debug, "Unsupported operation: %d\n",
                      (ifc_OperatorCategory_type)category);
      }  /* if */
#endif /* DEBUG */
      unexpected_condition();
  }  /* switch */
  return op;
}  /* opname_from_category */


/*
Structure used to hold the existing name linkage state if the name linkage
state needs to be modified.
*/
struct a_partial_scope_stack_state {
  a_byte_boolean
                saved;
                        /* TRUE if the state has been saved. */
  a_byte_boolean
                name_linkage_is_explicit;
                        /* Previous name_linkage_is_explicit setting. */
  a_bit_field   default_name_linkage:NUM_BITS_FOR_NAME_LINKAGE;
                        /* Previous default_name_linkage setting. */
  a_bit_field   current_access:2;
                        /* Previous current_access setting. */
};  /* a_partial_scope_stack_state */


static void save_partial_scope_stack(a_partial_scope_stack_state *psssp)
/*
Save portions of the decl_scope_level scope stack entry.
*/
{
  check_assertion(psssp != NULL);
  psssp->saved = TRUE;
  psssp->name_linkage_is_explicit =
                        scope_stack[decl_scope_level].name_linkage_is_explicit;
  psssp->default_name_linkage =
                            scope_stack[decl_scope_level].default_name_linkage;
  psssp->current_access = scope_stack[decl_scope_level].current_access;
}  /* save_partial_scope_stack */


/*
Utility to restore the previous name linkage state if it was previously saved.
*/
#define restore_partial_scope_stack_if_necessary(psssp) \
{ \
  if ((psssp)->saved) { \
    scope_stack[decl_scope_level].name_linkage_is_explicit = \
                                           (psssp)->name_linkage_is_explicit; \
    scope_stack[decl_scope_level].default_name_linkage = \
                                               (psssp)->default_name_linkage; \
    scope_stack[decl_scope_level].current_access = (psssp)->current_access; \
  }  /* if */ \
}  /* restore_partial_scope_stack_if_necessary */


static void defer_symbol_creation(a_module_entity_ptr mep,
                                  a_symbol_locator    *loc)
/*
Defer the creation of the module entity specified by mep.  A "lazy loading"
mechanism is used to create symbols only for entities that are referenced.  As
part of that mechanism, when an entity in a module file is discovered (as
specified by the locator information in *loc), rather than creating a symbol
for the entity, the module entity is queued on the symbol header.  If, during
name lookup, a symbol header with a matching, non-NULL deferred_module_entities
field is encountered, an IL entity and symbol are created at that time.
*/
{
  check_assertion(loc->symbol_header != NULL);
  mep->next = loc->symbol_header->deferred_module_entities;
  loc->symbol_header->deferred_module_entities = mep;
#if DEBUG
  if (db_flag_is_set("ms_symbols")) {
    (void)fprintf(f_debug, "Defer symbol creation for %s",
                  loc->symbol_header->identifier);
    (void)fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
}  /* defer_symbol_creation */


a_boolean an_ifc_module::import(a_module_import_decl_ptr midp) noexcept
/*
Import an IFC module file described by midp.  The IFC file should already have
been confirmed to exist and the path stored in midp.
*/
{
  a_module_ptr              mod = midp->module_info;
  unsigned int              i;
  a_boolean                 result = FALSE;

  check_assertion(midp->module_info->kind == mk_ifc);
  check_assertion(mod->name != NULL && mod->full_name != NULL);
  check_assertion(mod->module_interface == this);
  if (open_and_map_ifc_module_file(midp)) {
    result = TRUE;
    assoc_module_info = mod;
    set_name(mod->name);
    /* Read the IFC file header (which starts after the magic number). */
    init_byte_buffer((char*)mmap_addr + 4, mmap_size - 4);
    (void)get_File_Header(&header);
    /* FIXME: The checksum is not yet checked. */
    /* Prepare to read the partitions (by "seeking" to the IFC Table of
       Contents). */
    init_byte_buffer((char*)mmap_addr + header.toc,
                     mmap_size - header.toc);
#if DEBUG
    if (db_flag_is_set("ifc_modules")) {
      db_module(mod);
    }  /* if */
#endif /* DEBUG */
    for (i = 0; i < header.partition_count; i++) {
      an_ifc_Partition partition, *ifc_pp;
      an_ifc_partition *pp;
      a_const_char *name_str;
      an_ifc_partition_map *map_ptr;

      /* Read information about the partition. */
      ifc_pp = get_Partition(&partition);
      name_str = get_string_at_offset(ifc_pp->name);
#if DEBUG
      if (db_flag_is_set("ifc_modules")) {
        (void)fprintf(f_debug,
            "partition %d \"%s\" offset 0x%08x cardinality %d entry_size %d\n",
            i, name_str, ifc_pp->offset, ifc_pp->cardinality,
            ifc_pp->entry_size);
      }  /* if */
#endif /* DEBUG */
      check_assertion(ifc_pp->cardinality != 0 && ifc_pp->offset != 0);
      map_ptr = find_ifc_partition(name_str);
      if (map_ptr == NULL) {
        str_warning(ec_unknown_ifc_partition, name_str);
      } else {
        check_assertion_str(map_ptr->kind != ifc_last,
                            "no mapping for IFC partition");
        pp = &partitions[map_ptr->kind];
        pp->name = map_ptr->name;
        pp->offset = ifc_pp->offset;
        pp->size = ifc_pp->cardinality * ifc_pp->entry_size;
        pp->entry_size = ifc_pp->entry_size;
      }  /* if */
    }  /* for */
    (void)fseek(f_module, 0L, SEEK_SET);
    if (partitions[ifc_name_source_file].name != NULL) {
      /* Allocate an array to map source files to sequence numbers for the
         the module.  No information about the sequence numbers is recorded
         yet (we do that only if the source file is later referenced). */
      an_ifc_partition *nsf_pp = &partitions[ifc_name_source_file];
      size_t size;
      check_assertion(nsf_pp->entry_size != 0);
      size = (nsf_pp->size / nsf_pp->entry_size) * sizeof(a_seq_number);
      sequence_numbers = (a_seq_number *)alloc_il(size);
      memzero((char *)sequence_numbers, size);
    }  /* if */
#if DEBUG
    if (db_flag_is_set("ms_modsrc")) {
      /* Generate a textual representation of the module file and print it. */
      a_str_control_block scb;
      clear_str_control_block(&scb, mod, (a_text_buffer*)NULL);
      str_ifc_scope_index(header.global_scope, &scb);
      add_char_to_text_buffer(scb.text_buffer, '\0');
      (void)fwrite(scb.text_buffer->buffer, 1, scb.text_buffer->size, f_debug);
      (void)fputc('\n', f_debug);
    }  /* if */
#endif /* DEBUG */
    /* Indicate to the lookup routines that lazy symbols are in use. */
    lazy_symbols_may_be_visible = TRUE;
    /* Process all declarations in the global scope. */
    process_ifc_scope(header.global_scope, il_header.primary_scope);
  }  /* if */
  return result;
}  /* import */


void an_ifc_module::close() noexcept
/*
Close the module file specified in the module-import-declaration.
*/
{
  if (f_module != NULL) {
    (void)fclose(f_module);
    f_module = NULL;
#if EDG_WIN32
    close_mapped_input_file(mapped_input, map_object);
    mapped_input = NULL;
    map_object = NULL;
#endif /* EDG_WIN32 */
  }  /* if */
}  /* close_ifc_module_file */


void an_ifc_module::ifc_modules_pch_reset(a_module_import_decl_ptr midp)
                                                                       noexcept
/*
Called after a PCH file has been read to re-open and re-mmap the specified
module.  Note that the mmap-ed address does not need to be at the same
location as the original.
*/
{
  if (!open_and_map_ifc_module_file(midp)) {
    /* This shouldn't happen (the PCH processing checks the existence and
       modification time of module files. */
    unexpected_condition();
  }  /* if */
}  /* ifc_modules_pch_reset */


/* FIXME: might be able to get rid of enumeration_type now that enums aren't
   deferred */
void an_ifc_module::process_ifc_declaration(
                                          a_module_entity_ptr mep,
                                          a_boolean           defer,
                                          a_type_ptr          enumeration_type)
                                                                 const noexcept

/*
Process the IFC module entity declaration specified by mep either by creating
the appropriate IL entity, or, when defer is TRUE, mark the appropriate
symbol header as having a deferred module entity (which will be lazily loaded
if referenced).  When enumeration_type is non-NULL, it represents the
enumeration type for the enumerator being defined (and is added to the list of
constants for that type).
*/
{
  ifc_DeclSort             tag;
  a_decl_parse_state       dps;
  an_id_linkage_kind       linkage_ptr;
  a_symbol_ptr             ext_sym;
  a_decl_pos_block         decl_pos_block;
  a_symbol_locator         loc;
  a_partial_scope_stack_state     psss;
  char                     *il_entity = NULL;
  a_byte_il_entry_kind     kind = iek_none;

  if (mep->entity.ptr == NULL) {
    check_assertion(mep->scope != NULL);
    /* Prepare to read from the proper partition for this declaration. */
    read_ifc_partition_at_offset(mep->variant.ifc_partition,
                                 mep->file_offset);
    tag = (ifc_DeclSort)get_tag_from_partition(mep->variant.ifc_partition,
                                               ifc_decl_start);
    switch (tag) {
      case DeclSort(Variable):
        { an_ifc_DeclSort_Variable idsv, *idsvp;
          idsvp = get_DeclSort_Variable(&idsv);
          init_locator_from_name(idsvp->name, 0, &idsvp->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here. */
            a_variable_ptr vp;
            init_dps(&dps, &idsvp->locus, idsvp->type, idsvp->alignment,
                     idsvp->traits, ifc_MsvcTraits_None, idsvp->specifier,
                     idsvp->access, &psss);
            clear_decl_pos_block(&decl_pos_block);
            decl_variable(&loc, &dps, SRK_DEFINITION, &linkage_ptr, &ext_sym,
                          &decl_pos_block);
            vp = dps.sym->variant.variable.ptr;
            if (idsvp->initializer != 0) {
              /* Variable has an initializer. */
              /* FIXME: for now, assume it's a static constant initialization,
                 but lots more to do here. */
              a_constant_ptr cp = constant_for_expr_index(idsvp->initializer,
                                                          vp->type);
              vp->initializer.constant = cp;
              vp->init_kind = (an_init_kind)initk_static;
            }  /* if */
            restore_partial_scope_stack_if_necessary(&psss);
            il_entity = (char *)vp;
            kind = iek_variable;
          }  /* if */
        }
        break;
      case DeclSort(Function):
        { a_func_info_block        func_info;
          a_type_ptr               old_type;
          an_ifc_DeclSort_Function idsf, *idsfp;
          idsfp = get_DeclSort_Function(&idsf);
          init_locator_from_name(idsfp->name, 0, &idsfp->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here. */
            a_routine_ptr rp;
            init_dps(&dps, &idsfp->locus, idsfp->type, (ifc_Alignment)0,
                     idsfp->traits, ifc_MsvcTraits_None, idsfp->specifiers,
                     idsfp->access, &psss);
            clear_func_info(&func_info);
            clear_decl_pos_block(&decl_pos_block);
            decl_routine(&loc, &dps, &func_info, SRK_DECLARATION, &linkage_ptr,
                         &old_type, &ext_sym, &decl_pos_block);
            restore_partial_scope_stack_if_necessary(&psss);
            rp = dps.sym->variant.routine.ptr;
            il_entity = (char *)rp;
            kind = iek_routine;
          }  /* if */
        }
        break;
      case DeclSort(Intrinsic):
        /* A builtin function declaration. */
        { a_func_info_block        func_info;
          a_type_ptr               old_type;
          a_routine_ptr            rp;
          an_ifc_DeclSort_Intrinsic idsi, *idsip;
          idsip = get_DeclSort_Intrinsic(&idsi);
          init_locator_from_name(0, idsip->name, &idsip->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here (just copied
               DeclSort(Function)).*/
            init_dps(&dps, &idsip->locus, idsip->type, (ifc_Alignment)0,
                     ifc_ObjectTraits_None, ifc_MsvcTraits_None,
                     idsip->specifiers, idsip->access, &psss);
            clear_func_info(&func_info);
            clear_decl_pos_block(&decl_pos_block);
            decl_routine(&loc, &dps, &func_info, SRK_DECLARATION, &linkage_ptr,
                         &old_type, &ext_sym, &decl_pos_block);
            restore_partial_scope_stack_if_necessary(&psss);
            rp = dps.sym->variant.routine.ptr;
            il_entity = (char *)rp;
            kind = iek_routine;
          }  /* if */
        }
        break;
      case DeclSort(Scope):
        /* A DeclSort::Scope, which indicates a namespace or a
           class/struct/union.  Note that although these declare "scopes",
           the IL entity that is attached to them is either a namespace or
           a type. */
        { an_ifc_DeclSort_Scope idss, *idssp;
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          a_type_kind                 type_kind;
          a_symbol_kind               tag_kind;
          idssp = get_DeclSort_Scope(&idss);
          /* Should be no unnamed namespaces or types. */
          check_assertion(idssp->name != 0);
          init_locator_from_name(idssp->name, 0, &idssp->locus, &loc);
          /* Look at the "type" to determine whether we have a namespace or
             not. */
          check_assertion(type_tag(idssp->type) == TypeSort(Fundamental));
          read_ifc_partition_at_index(ifc_type_fundamental,
                                      type_value(idssp->type));
          itsfp = get_TypeSort_Fundamental(&itsf);
          switch (itsfp->basis) {
            case TypeBasis(Namespace):
              { a_namespace_ptr nsp = NULL;
                a_symbol_ptr    ns_sym;
                if (defer && !(idssp->traits & ifc_ScopeTraits_Inline)) {
                  /* Note that inline namespaces are not deferred. */
                  defer_symbol_creation(mep, &loc);
                } else {
                  /* FIXME: lots missing. */
                  check_assertion(!(idssp->traits & ifc_ScopeTraits_Unnamed));
                  ns_sym = curr_scope_id_lookup(&loc, IDL_NO_OPTIONS);
                  if (ns_sym != NULL) {
                    if (ns_sym->kind == (a_symbol_kind)sk_namespace &&
                        !ns_sym->variant.namespace_info.ptr->
                                                          is_namespace_alias) {
                      /* A namespace already exists; use it. */
                      nsp = ns_sym->variant.namespace_info.ptr;
                      (void)push_namespace_scope(
                                         (a_scope_kind)sck_namespace_extension,
                                         nsp);
                    } else {
                      /* FIXME: need to fix this. */
                      unexpected_condition();
                    }  /* if */
                  } else {
                    /* No namespace in the specified scope; create one. */
                    ns_sym = enter_symbol((a_symbol_kind)sk_namespace, &loc,
                                          mep->scope->depth_in_scope_stack,
                                          /*suppress_redecl_error=*/FALSE);
                    nsp = alloc_namespace(/*is_alias=*/FALSE);
                    set_source_corresp(&nsp->source_corresp, ns_sym);
                    set_namespace_membership(ns_sym, &nsp->source_corresp,
                                             (a_namespace_ptr)NULL);
                    nsp->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
                    ns_sym->variant.namespace_info.ptr = nsp;
                    add_to_namespaces_list(nsp);
                    (void)push_namespace_scope((a_scope_kind)sck_namespace,
                                               nsp);
                    nsp->variant.assoc_scope->variant.assoc_namespace = nsp;
                  }  /* if */
                  /* Process declarations in the namespace scope (which makes
                     their symbols available but not their definitions). */
                  process_ifc_scope(idssp->initializer,
                                    nsp->variant.assoc_scope);
                  pop_namespace_scope();
                  il_entity = (char *)nsp;
                  kind = iek_namespace;
                }  /* if */
              }
              break;
            case TypeBasis(Class):
              type_kind = tk_class;
              tag_kind = sk_class_or_struct_tag;
              goto class_struct_union_case;
            case TypeBasis(Struct):
            case TypeBasis(Interface):
              type_kind = tk_struct;
              tag_kind = sk_class_or_struct_tag;
              goto class_struct_union_case;
            case TypeBasis(Union):
              type_kind = tk_union;
              tag_kind = sk_union_tag;
class_struct_union_case:
              /* A class/struct/union type. */
              { a_type_ptr                  class_type;
                a_class_type_supplement_ptr ctsp;
                a_symbol_ptr                tag_sym;
                if (defer) {
                  defer_symbol_creation(mep, &loc);
                } else {
                  /* Allocate the appropriate class type, but leave it as
                     incomplete.  The class will be completed during a call to
                     get_definition_of_class if it is referenced. */
                  class_type = alloc_type(type_kind);
                  ctsp = class_type_supp(class_type);
                  if (itsfp->basis == TypeBasis(Interface)) {
                    class_type->variant.class_struct_union.is_interface = TRUE;
                    class_type->variant.class_struct_union.abstract = TRUE;
                  }  /* if */
                  tag_sym = enter_local_symbol(tag_kind, &loc,
                                              mep->scope->depth_in_scope_stack,
                                              /*suppress_redecl_error=*/FALSE);
                  tag_sym->variant.class_struct_union.type = class_type;
                  set_source_corresp(&(class_type->source_corresp), tag_sym);
                  /* Set parent class or namespace pointers, if appropriate,
                     and adjust related fields (e.g., name linkage). */
                  /* FIXME: all of these values need to be checked. */
                  update_membership_of_class(tag_sym,
                                             /*def_or_vacuous_decl=*/FALSE,
                                             /*is_event_interface=*/FALSE,
                                             mep->scope->depth_in_scope_stack,
                                             &null_source_position);
                  /* FIXME: need to call record_symbol_declaration? */
                  add_to_types_list(class_type,
                                    mep->scope->depth_in_scope_stack);
                  /* Mark the class as incomplete, but record the module and
                     declaration so the class can be completed later (if
                     referenced). */
                  class_type->incomplete = TRUE;
                  ctsp->module_entity = mep;
                  /* FIXME: idssp->alignment, idssp->pack_size, idssp->traits,
                            idssp->specifiers, all need to be set manually
                            here*/
                  /* FIXME: for now: */
                  class_type->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
                  il_entity = (char*)class_type;
                  kind = iek_type;
                }  /* if */
              }
              break;
            default:
              unexpected_condition();
          }  /* switch */
        }
        break;
      case DeclSort(Alias):
        { an_ifc_DeclSort_Alias idsta, *idstap;
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          idstap = get_DeclSort_Alias(&idsta);
          init_locator_from_name(0, idstap->name, &idstap->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            check_assertion(type_tag(idstap->type)== TypeSort(Fundamental));
            /* Read the type to see what kind it is. */
            read_ifc_partition_at_index(ifc_type_fundamental,
                                        type_value(idstap->type));
            itsfp = get_TypeSort_Fundamental(&itsf);
            if (itsfp->basis == TypeBasis(Typename)) {
              /* A type alias; declare a typedef for this case. */
              init_dps(&dps, &idstap->locus, idstap->initializer,
                       (ifc_Alignment)0, ifc_ObjectTraits_None,
                       ifc_MsvcTraits_None, idstap->specifiers, idstap->access,
                       &psss);
              clear_decl_pos_block(&decl_pos_block);
              decl_typedef(&loc, &dps, (a_type_ptr)NULL, &decl_pos_block);
              restore_partial_scope_stack_if_necessary(&psss);
              il_entity = (char *)dps.sym->variant.type.ptr;
              kind = iek_type;
            } else if (itsfp->basis == TypeBasis(Namespace)) {
              /* A namespace alias. */
              /* FIXME: unimplemented. */
              unexpected_condition();
            } else {
              unexpected_condition();
            }  /* if */
          }  /* if */
        }
        break;
      case DeclSort(Enumeration):
        { an_ifc_DeclSort_Enumeration idse, *idsep;
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          a_boolean                   is_scoped_enum = FALSE;
          a_scope_ptr                 enum_scope = mep->scope;
          idsep = get_DeclSort_Enumeration(&idse);
          check_assertion(type_tag(idsep->type) == TypeSort(Fundamental));
          /* See if this is a scoped enumeration or not. */
          read_ifc_partition_at_index(ifc_type_fundamental,
                                      type_value(idsep->type));
          itsfp = get_TypeSort_Fundamental(&itsf);
          if (itsfp->basis == TypeBasis(Enum)) {
            /* A classic enumeration.  Don't bother to defer in this case
               because each of the enumerators need to be registered in the
               symbol table so they can be found. */
            defer = FALSE;
          } else if (itsfp->basis == TypeBasis(Class) ||
                     itsfp->basis == TypeBasis(Struct)) {
            /* A scoped enumeration.  Enumerator definitions are deferred when
               the enumeration definition is deferred (enumerators can't be
               referred to without specifying the scoped enumeration at which
               time the enumerators will be made available). */
            is_scoped_enum = TRUE;
          } else {
            unexpected_condition();
          }  /* if */
          check_assertion(idsep->name != 0);
          init_locator_from_name(0, idsep->name, &idsep->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_type_ptr   enum_type;
            a_symbol_ptr tag_sym;
            check_assertion(idsep->base != 0);
            init_dps(&dps, &idsep->locus, idsep->base, idsep->alignment,
                     ifc_ObjectTraits_None, ifc_MsvcTraits_None,
                     idsep->specifiers, idsep->access, &psss);
            clear_decl_pos_block(&decl_pos_block);
            /* Allocate an integer type and set its size based on the type
               specified by idsep->base. */
            enum_type = alloc_type((a_type_kind)tk_integer);
            enum_type->variant.integer.int_kind = (an_integer_kind)ik_int;
            enum_type->variant.integer.enum_type = TRUE;
            enum_type->size = skip_typerefs(dps.type)->size;
            integer_type_supp(enum_type)->base_type = dps.type;
            enum_type->variant.integer.has_explicit_enum_base = TRUE;
            enum_type->alignment = dps.alignment;
            /* FIXME: for now: */
            enum_type->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
            /* FIXME: enum_type->variant.integer.is_template_enum? */
            enum_type->source_corresp.access =
                                  scope_stack[decl_scope_level].current_access;
            /* Process any attributes. */
            attach_tag_attributes(dps.prefix_attributes, enum_type, &dps,
                                  /*is_definition=*/TRUE,
                                  /*is_forward_decl=*/FALSE,
                                  /*ignore_gnu_attributes=*/TRUE);
            /* FIXME: not sure about this and its effect on alignment: */
            set_type_size(enum_type);
            if (is_scoped_enum) {
              enum_type->variant.integer.is_scoped_enum = TRUE;
              enum_scope = push_scope((a_scope_kind)sck_enum, NO_SCOPE_NUMBER,
                                      enum_type, (a_routine_ptr)NULL);
              enum_type->variant.integer.enum_info.assoc_scope = enum_scope;
            }  /* if */
            /* Create a symbol for the enumeration. */
            tag_sym = enter_local_symbol(sk_enum_tag, &loc,
                                         mep->scope->depth_in_scope_stack,
                                         /*suppress_redecl_error=*/FALSE);
            tag_sym->variant.enumeration.type = enum_type;
            set_source_corresp(&(enum_type->source_corresp), tag_sym);
            add_to_types_list(enum_type, mep->scope->depth_in_scope_stack);
            /* Set parent membership. */
            if (scope_is(mep->scope, sck_class_struct_union)) {
              /* FIXME: never get here because enumerations in classes are
                 loaded via the string mechanism. */
              set_class_membership(tag_sym, &enum_type->source_corresp,
                                   mep->scope->variant.assoc_type);
            } else if (scope_is(mep->scope, sck_namespace) ||
                       scope_is(mep->scope, sck_namespace_extension)) {
              set_namespace_membership(tag_sym, &enum_type->source_corresp,
                                       mep->scope->variant.assoc_namespace);
            }  /* if */
            il_entity = (char *)enum_type;
            kind = iek_type;
            /* Process the enumerators. */
            if (idsep->initializer.cardinality != 0) {
              unsigned int i;
              for (i = 0; i < idsep->initializer.cardinality; i++) {
                /* FIXME: for now, don't defer creation of enumerators.  There
                   are two problems: for non-scoped enums, we need to have the
                   enum type available so the constant can be queued on the
                   constant_list; for scoped enums, enum_qualified_id_lookup
                   relies on looking through the constants list of the scope,
                   but those aren't created if deferred. */
                /* The sequence doesn't specifically contain DeclIndex values;
                   compute one so we can invoke ourselves recursively to
                   process the enumerators. */
                a_module_entity_ptr emep;
                emep = get_decl_module_entity_ptr(make_decl_index(
                                                DeclSortAsType(Enumerator),
                                                idsep->initializer.start + i));
                emep->scope = enum_scope;
                process_ifc_declaration(emep, /*defer=*/FALSE, enum_type);
              }  /* for */
              integer_type_supp(enum_type)->enumerator_list_seen = TRUE;
            }  /* if */
            if (is_scoped_enum) {
              pop_scope();
            }  /* if */
            restore_partial_scope_stack_if_necessary(&psss);
          }  /* if */
        }
        break;
      case DeclSort(Enumerator):
        { an_ifc_DeclSort_Enumerator idse, *idsep;
          idsep = get_DeclSort_Enumerator(&idse);
          check_assertion(idsep->name != 0);
          init_locator_from_name(0, idsep->name, &idsep->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_symbol_ptr   enum_con_sym;
            a_constant_ptr enum_con;
            a_type_ptr     enum_type;
            if (mep->scope->kind == (a_scope_kind)sck_enum) {
              /* This is an enumerator for a scoped enum. */
              enum_type = mep->scope->variant.assoc_type;
            } else {
              check_assertion(enumeration_type != NULL);
              enum_type = enumeration_type;
            }  /* if */
            enum_con = constant_for_expr_index(idsep->initializer,
                                               enum_type);
            enum_con->type = enum_type;
            enum_con->is_named_constant_definition = TRUE;
            enum_con->source_corresp.parent_scope = mep->scope;
            enum_con->source_corresp.name_linkage =
                                        enum_type->source_corresp.name_linkage;
            enum_con_sym = enter_local_symbol((a_symbol_kind)sk_constant, &loc,
                                              mep->scope->depth_in_scope_stack,
                                              /*suppress_redecl_error=*/FALSE);
            set_source_corresp(&(enum_con->source_corresp), enum_con_sym);
            enum_con_sym->variant.constant = enum_con;
            if (!enum_type->variant.integer.is_scoped_enum &&
                sym_is_namespace_member(symbol_for(enum_type))) {
              /* Unscoped enumerators in a namespace are placed in the
                 namespace. */
              /* FIXME: Note that we don't handle the class case here (because
                 that's currently handled with strings). */
              set_namespace_membership(enum_con_sym, &enum_con->source_corresp,
                                       sym_parent_namespace(
                                                      symbol_for(enum_type)));
            }  /* if */
            /* FIXME: not currently doing anything with specifiers or
               access. */
            /* Add the constant to the list of constants for the enumeration
               type (note that the order of enumerators on the list is not
               necessarily the order that they appear in the enumeration,
               rather it's the order they are accessed in if definitions are
               deferred). */
            if (enum_type->variant.integer.is_scoped_enum) {
              enum_con->next = mep->scope->constants;
              mep->scope->constants = enum_con;
            } else {
              enum_con->next =
                            enum_type->variant.integer.enum_info.constant_list;
              enum_type->variant.integer.enum_info.constant_list = enum_con;
            }  /* if */
            il_entity = (char *)enum_con;
            kind = iek_constant;
          }  /* if */
        }
        break;
      case DeclSort(Method):
      case DeclSort(Constructor):
      case DeclSort(Destructor):
      case DeclSort(Field):
      case DeclSort(Bitfield):
      case DeclSort(Property):
        /* These entities can only exist in a class and class definitions are
           currently handled by scanning a textual representation of the
           class. */
        unexpected_condition();
      case DeclSort(VendorExtension):
      case DeclSort(Parameter):
      case DeclSort(Temploid):
      case DeclSort(Template):
      case DeclSort(PartialSpecialization):
      case DeclSort(ExplicitSpecialization):
      case DeclSort(ExplicitInstantiation):
      case DeclSort(Concept):
      case DeclSort(InheritedConstructor):
      case DeclSort(Reference):
      case DeclSort(OutputSegment):
      case DeclSort(UsingDeclaration):
      case DeclSort(UsingDirective):
      case DeclSort(Friend):
      case DeclSort(SyntaxTree):
      case DeclSort(Tuple):
      default:
#if DEBUG
        if (db_flag_is_set("ms_ignore")) {
          (void)fprintf(f_debug, "Ignoring declaration: [%s]\n",
                        db_decl_tag(tag));
        }  /* if */
        break;
#else /* !DEBUG */
        unexpected_condition(); /* FIXME: for now */
#endif /* DEBUG */
    }  /* switch */
    if (!defer) {
      /* Record the IL entity. */
      mep->entity.ptr = (char *)il_entity;
      mep->entity.kind = kind;
#if DEBUG
      if (db_flag_is_set("ms_symbols")) {
        (void)fprintf(f_debug, "Module entity defined: ");
        db_module_entity(mep);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
}  /* process_ifc_declaration */


void an_ifc_module::get_definition_of_module_class(a_module_entity_ptr mep,
                                                   a_text_buffer       *buffer)
                                                                 const noexcept
/*
This routine is called (from get_definition_of_class) when the front end has
determined that the class that corresponds to module entity mep needs a
definition.  Create a textual representation of that class (starting with the
base class list, if any) from information in the module file and return it in
buffer.
*/
{
  a_str_control_block   scb;
  an_ifc_DeclSort_Scope idss, *idssp;
  a_module_ptr          mod = mep->module_info;

  clear_str_control_block(&scb, mod, buffer);
  scb.is_generated_code = TRUE;
  read_ifc_partition_at_offset(mep->variant.ifc_partition,
                               mep->file_offset);
  idssp = get_DeclSort_Scope(&idss);
  str_ifc_class_definition(idssp, &scb);
  add_char_to_text_buffer(buffer, ';');
}  /* get_definition_of_module_class_from_ifc */

#if DEBUG

void an_ifc_module::debug() const noexcept
/*
Print debug information for an IFC module
*/
{
  (void)fprintf(f_debug, "kind: mk_ifc\n");
}  /* debug */

#endif /* DEBUG */

a_boolean an_ifc_module::open_and_map_ifc_module_file(
                                                 a_module_import_decl_ptr midp)
                                                                       noexcept
/*
Open the module file and map it into the process' address space.  Note that
this is also used after restoring from a PCH file.  Returns TRUE if the
module file was successfully opened and FALSE (with an error message)
otherwise.
*/
{
  a_module_ptr  mod = midp->module_info;
  a_boolean     err = FALSE;
  FILE          *file;
  char          magic[4];
  struct stat   stat_buf;

  check_assertion(mod != NULL && mod->full_name != NULL);
  file = fopen_with_error(mod->full_name, FOPEN_MODE_FOR_BINARY_READ,
                          OFF_NO_OPTIONS, ec_module_file);
  if (file == NULL) {
    err = TRUE;
  } else {
    if (fstat(fileno(file), &stat_buf) != 0) {
      err = TRUE;
    }  /* if */
    /* Make sure file is at least large enough to have the magic number
       and an IFC header. */
    if (!err &&
        (size_t)stat_buf.st_size <
                                (sizeof(magic) + sizeof(an_ifc_File_Header))) {
      err = TRUE;
    }  /* if */
    /* Read the magic number from the beginning of the file. */
    if (!err &&
        fread(magic, (size_t)1, sizeof(magic), file) != sizeof(magic)) {
      err = TRUE;
    }  /* if */
    /* Verify the magic number (this works for both big and little endian
       machines). */
    if (!err &&
        (magic[0] != 0x54 ||
         magic[1] != 0x51 ||
         magic[2] != 0x45 ||
         magic[3] != 0x1A)) {
      /* FIXME: Visual Studio silently ignores this case (and continues to
         search for a proper IFC file). */
      err = TRUE;
    }  /* if */
    if (!err) {
      /* Map the module file into the address space of the process.  The
         process is a little different on Windows environments. */
#if USE_MMAP_FOR_MEMORY_REGIONS
#if EDG_WIN32
      open_mapped_input_file(mod->full_name, &mapped_input, &map_object);
#endif /* EDG_WIN32 */
      mmap_size = stat_buf.st_size;
      mmap_addr = map_input_file_to_region(file,
#if EDG_WIN32
                                           map_object,
#else /* !EDG_WIN32 */
                                           (a_windows_handle)0,
#endif /* EDG_WIN32 */
                                           /*read_only=*/TRUE, (sizeof_t)0,
                                           mmap_size, NULL, mod->full_name);
      check_assertion(mmap_addr != NULL);
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
      f_module = file;
    }  /* if */
    if (err) {
      (void)fclose(file);
    }  /* if */
  }  /* if */
  if (err) {
    /* FIXME: perhaps better error messages here. */
    pos_st_error(ec_cannot_import_module, &midp->module_name_position,
                 mod->full_name);
  }  /* if */
  return !err;
}  /* open_and_map_ifc_module_file */


an_ifc_partition_map *an_ifc_module::find_ifc_partition(a_const_char *name)
                                                                       noexcept
/*
Find the IFC partition map entry for the partition matching name.  Return a
pointer to that entry or NULL if it could not be found.
*/
{
  an_ifc_partition_map *map_ptr;

  /* FIXME: replace with binary search. */
  for (map_ptr = ifc_partition_map; map_ptr->name != NULL; map_ptr++) {
    if (strcmp(name, map_ptr->name) == 0) {
      break;
    }  /* if */
  }  /* for */
  if (map_ptr->name == NULL) {
    map_ptr = NULL;
  }  /* if */
  return map_ptr;
}  /* find_ifc_partition */


void an_ifc_module::process_ifc_scope(ifc_ScopeIndex scope_index,
                                      a_scope_ptr    scope) const noexcept
/*
Process the IFC scope specified by scope_index in the module file pointed to
by mod.  All items in the IFC scope will be members of "scope" and their
definitions will be deferred until they are referenced.
*/
{
  an_ifc_Scope_Descriptor isd, *isdp;
  an_ifc_Scope_Member     ism, *ismp;
  unsigned int            i;
  a_boolean               scope_pushed;
  a_module_entity_ptr     dmep;

  /* A scope index of 0 indicates a missing scope, in which case there
     is nothing further to do. */
  if (scope_index != 0) {
    scope_pushed = push_module_declaration_context(scope);
    /* Scope indices are 1-based, so subtract one. */
    read_ifc_partition_at_index(ifc_scope_desc, scope_index - 1);
    isdp = get_Scope_Descriptor(&isd);
    for (i = 0; i < isdp->cardinality; i++) {
      /* Re-enable access to scope.member partition (it changes during the
         loop). */
      read_ifc_partition_at_index(ifc_scope_member, isdp->start + i);
      ismp = get_Scope_Member(&ism);
      dmep = get_decl_module_entity_ptr(ismp->index);
      dmep->scope = scope;
      process_ifc_declaration(dmep, /*defer=*/TRUE, (a_type_ptr)NULL);
    }  /* for */
    pop_module_declaration_context(scope_pushed);
  }  /* if */
}  /* process_ifc_scope */


a_module_entity_ptr an_ifc_module::get_ifc_module_entity_ptr(
                                        an_ifc_partition_kind partition,
                                        size_t                partition_offset)
                                                                 const noexcept
/*
Utility to return a module entity pointer given a module, IFC partition, and
offset within that partition.  For cases where the module entity has
just been created, the partition is set according to the partition supplied
by the caller.
*/
{
  a_module_entity_ptr mep;

  mep = get_module_entity_ptr(assoc_module_info,
                              ifc_file_offset_of(partition, partition_offset));
  if (mep->variant.ifc_partition == ifc_none) {
    mep->variant.ifc_partition = partition;
  } else {
    check_assertion(mep->variant.ifc_partition == partition);
  }  /* if */
  return mep;
}  /* get_ifc_module_entity_ptr */


a_type_ptr an_ifc_module::type_for_ifc_type_index(ifc_TypeIndex type_index)
                                                                 const noexcept
/*
Returns the type that corresponds to the specified TypeIndex in the module
file indicated by mod.  Note that NULL is a valid return (and represents
an "ellipsis type").
*/
{
  a_type_ptr                result = NULL;
  a_module_entity_ptr       mep = get_type_module_entity_ptr(type_index);
  ifc_TypeSort              tag;

  if (mep->entity.ptr != NULL) {
    /* There is already an entry for this; return it. */
    check_assertion(mep->entity.kind == iek_type);
    result = (a_type_ptr)mep->entity.ptr;
  } else {
    /* Prepare to read from the proper partition for this type. */
    read_ifc_partition_at_offset(mep->variant.ifc_partition,
                                 mep->file_offset);
    tag = (ifc_TypeSort)get_tag_from_partition(mep->variant.ifc_partition,
                                               ifc_type_start);
    switch (tag) {
      case TypeSort(Fundamental):
        { an_integer_kind             ik;
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          itsfp = get_TypeSort_Fundamental(&itsf);
          /* Note: no check is made for non-sensical types (e.g., signed
             void).*/
          switch (itsfp->basis) {
            case TypeBasis(Void):
              check_assertion(itsfp->precision == TypePrecision(Default));
              result = void_type();
              break;
            case TypeBasis(Bool):
              check_assertion(itsfp->precision == TypePrecision(Default));
              result = bool_type();
              break;
            case TypeBasis(Char):
              switch (itsfp->sign) {
                case TypeSign(Plain):
                  switch (itsfp->precision) {
                    case TypePrecision(Default):
                      result = integer_type((an_integer_kind)ik_char);
                      break;
                    case TypePrecision(Bit16):
                      result = char16_t_type();
                      break;
                    case TypePrecision(Bit32):
                      result = char32_t_type();
                      break;
                    default:
                      unexpected_condition();
                  }  /* switch */
                  break;
                case TypeSign(Signed):
                  check_assertion(itsfp->precision ==
                                                    TypePrecision(Default));
                  result = integer_type((an_integer_kind)ik_signed_char);
                  break;
                case TypeSign(Unsigned):
                  check_assertion(itsfp->precision ==
                                                    TypePrecision(Default));
                  result = integer_type((an_integer_kind)ik_unsigned_char);
                  break;
                default:
                  unexpected_condition();
              }  /* if */
              break;
            case TypeBasis(Wchar_t):
              switch (itsfp->precision) {
                case TypePrecision(Default):
                  result = wchar_t_type();
                  break;
                case TypePrecision(Bit16):
                  result = char16_t_type();
                  break;
                case TypePrecision(Bit32):
                  result = char32_t_type();
                  break;
                /* FIXME: not sure how to map these: */
                case TypePrecision(Short):
                case TypePrecision(Long):
                case TypePrecision(Bit64):
                case TypePrecision(Bit128):
                default:
                  unexpected_condition();
              }  /* switch */
              break;
            case TypeBasis(Int):
              switch (itsfp->precision) {
                case TypePrecision(Default):
                  ik = (itsfp->sign == TypeSign(Unsigned)) ?
                                                              ik_unsigned_int :
                                                              ik_int;
                  break;
                case TypePrecision(Short):
                  ik = (itsfp->sign == TypeSign(Unsigned)) ?
                                                            ik_unsigned_short :
                                                            ik_short;
                  break;
                case TypePrecision(Long):
                  ik = (itsfp->sign == TypeSign(Unsigned)) ?
                                                             ik_unsigned_long :
                                                             ik_long;
                  break;
                case TypePrecision(Bit64):
                  ik = (itsfp->sign == TypeSign(Unsigned)) ?
                                                        ik_unsigned_long_long :
                                                        ik_long_long;
                  break;
                /* FIXME: not sure how to map these: */
                case TypePrecision(Bit16):
                case TypePrecision(Bit32):
                case TypePrecision(Bit128):
                default:
                  unexpected_condition();
              }  /* switch */
              result = integer_type(ik);
              break;
            case TypeBasis(Float):
              check_assertion(itsfp->precision == TypePrecision(Default));
              result = float_type((a_float_kind)fk_float);
              break;
            case TypeBasis(Double):
              if (itsfp->precision == TypePrecision(Long)) {
                result = float_type((a_float_kind)fk_long_double);
              } else {
                check_assertion(itsfp->precision == TypePrecision(Default));
                result = float_type((a_float_kind)fk_double);
              }  /* if */
              break;
            case TypeBasis(Nullptr):
              check_assertion(itsfp->precision == TypePrecision(Default));
              result = standard_nullptr_type();
              break;
            case TypeBasis(Ellipsis):
              check_assertion(itsfp->precision == TypePrecision(Default));
              /* The IL doesn't have a way to represent an "ellipsis type", so
                 return a NULL type and let the caller check explicitly for
                 it. */
              result = NULL;
              break;
            case TypeBasis(Class):
            case TypeBasis(Struct):
            case TypeBasis(Union):
            case TypeBasis(Auto):
            case TypeBasis(DecltypeAuto):
            case TypeBasis(Namespace):
            case TypeBasis(Interface):
            case TypeBasis(Enum):
            case TypeBasis(Typename):
            case TypeBasis(SegmentType):
            case TypeBasis(Function):
            case TypeBasis(Empty):
            case TypeBasis(VariableTemplate):
            default:
              /* FIXME: not implemented yet. */
              unexpected_condition();
          }  /* switch */
        }
        break;
      case TypeSort(Qualified):
        { a_type_qualifier_set      qualifiers = TQ_NONE;
          an_ifc_TypeSort_Qualified itsq, *itsqp;
          itsqp = get_TypeSort_Qualified(&itsq);
          if (itsqp->qualifiers & ifc_Qualifier_Const) {
            qualifiers |= TQ_CONST;
          }  /* if */
          if (itsqp->qualifiers & ifc_Qualifier_Volatile) {
            qualifiers |= TQ_VOLATILE;
          }  /* if */
          if (itsqp->qualifiers & ifc_Qualifier_Restrict) {
            qualifiers |= TQ_RESTRICT;
          }  /* if */
          result = make_qualified_type(
                                   type_for_ifc_type_index(itsqp->unqualified),
                                   qualifiers);
        }
        break;
      case TypeSort(Pointer):
        { an_ifc_TypeSort_Pointer itsp, *itspp;
          itspp = get_TypeSort_Pointer(&itsp);
          result = make_pointer_type(type_for_ifc_type_index(itspp->pointee));
        }
        break;
      case TypeSort(LvalueReference):
        { an_ifc_TypeSort_LvalueReference itslr, *itslrp;
          itslrp = get_TypeSort_LvalueReference(&itslr);
          result =
                 make_reference_type(type_for_ifc_type_index(itslrp->referee));
        }
        break;
      case TypeSort(RvalueReference):
        { an_ifc_TypeSort_RvalueReference itsrr, *itsrrp;
          itsrrp = get_TypeSort_RvalueReference(&itsrr);
          result = make_rvalue_reference_type(
                                     type_for_ifc_type_index(itsrrp->referee));
        }
        break;
      case TypeSort(Array):
        { an_ifc_TypeSort_Array itsa, *itsap;
          itsap = get_TypeSort_Array(&itsa);
          result = alloc_type((a_type_kind)tk_array);
          result->variant.array.element_type =
                                       type_for_ifc_type_index(itsap->element);
          /* FIXME: this is wrong: */
          result->variant.array.variant.number_of_elements = itsap->extent;
        }
        break;
      case TypeSort(Method): /* FIXME: for now (same structures)?): */
        unexpected_condition(); /* FIXME: No longer same structures. */
        break;
      case TypeSort(Function):
        { an_ifc_TypeSort_Function itsf, *itsfp;
          itsfp = get_TypeSort_Function(&itsf);
          /* Create a routine type with no parameters to start. */
          result = make_routine_type(type_for_ifc_type_index(itsfp->target),
                                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                                     (a_type_ptr)NULL, (a_type_ptr)NULL);
          /* FIXME: need a thorough review of this. */
          if (itsfp->source != 0) {
            /* The function has parameters. */
            unsigned int                  i;
            a_routine_type_supplement_ptr rtsp =
                                            result->variant.routine.extra_info;
            a_param_type_ptr              ptp, *prev = &rtsp->param_type_list;
            an_ifc_TypeSort_Tuple         itst, *itstp;
            a_type_ptr                    param_type;
            if (type_tag(itsfp->source) == TypeSort(Tuple)) {
              /* A list of parameters. */
              read_ifc_partition_at_index(ifc_type_tuple,
                                          type_value(itsfp->source));
              itstp = get_TypeSort_Tuple(&itst);
              for (i = 0; i < itstp->cardinality; i++) {
                ifc_TypeIndex type_index;
                read_ifc_partition_at_index(ifc_heap_type,
                                            itstp->start + i);
                GET_TypeIndex(type_index);
                param_type = type_for_ifc_type_index(type_index);
                if (param_type == NULL) {
                  /* This happens when an ellipsis is present as the last
                     parameter. */
                  check_assertion(i == itstp->cardinality-1);
                  rtsp->has_ellipsis = TRUE;
                  break;
                }  /* if */
                ptp = make_param_type(param_type, &null_source_position);
                ptp->param_num = i + 1;
                *prev = ptp;
                prev = &ptp->next;
              }  /* for */
            } else {
              /* A single parameter. */
              param_type = type_for_ifc_type_index(itsfp->source);
              if (param_type == NULL) {
                /* A single ellipsis parameter. */
                rtsp->has_ellipsis = TRUE;
              } else {
                rtsp->param_type_list = make_param_type(param_type,
                                                        &null_source_position);
              }  /* if */
            }  /* if */
          }  /* if */
        }
        break;
      case TypeSort(Designated):
        /* A type's name (e.g., "A"). */
        { an_ifc_TypeSort_Designated itsd, *itsdp;
          a_type_ptr                 class_or_enum_type;
          a_module_entity_ptr        dmep;
          itsdp = get_TypeSort_Designated(&itsd);
          /* Prepare to read from the proper partition for the
             type scope declaration. */
          check_assertion(decl_tag(itsdp->decl) == DeclSort(Scope) ||
                          decl_tag(itsdp->decl) == DeclSort(Enumeration));
          /* Find the type of the scope declaration by processing it (in
             case it has been deferred). */
          dmep = get_decl_module_entity_ptr(itsdp->decl);
          // FIXME: scope is unknown means bad news.
          check_assertion(dmep->scope != NULL);
          process_ifc_declaration(dmep, /*defer=*/FALSE, (a_type_ptr)NULL);
          class_or_enum_type = (a_type_ptr)dmep->entity.ptr;
          check_assertion(class_or_enum_type != NULL &&
                          dmep->entity.kind == iek_type);
          result = class_or_enum_type;
        }
        break;
      case TypeSort(Deduced):
      case TypeSort(PointerToMember):
      case TypeSort(Tuple):
      case TypeSort(Forall):
      case TypeSort(VendorExtension):
      case TypeSort(Syntactic):
      case TypeSort(Expansion):
      case TypeSort(Typename):
      case TypeSort(Base):
      case TypeSort(Unaligned):
      case TypeSort(Decltype):
      case TypeSort(SyntaxTree):
      default:
        /* FIXME: not yet implemented. */
#if DEBUG
        if (db_flag_is_set("ms_ignore")) {
          (void)fprintf(f_debug, "Unsupported type: ");
          db_module_entity(mep);
        }  /* if */
#endif /* DEBUG */
        unexpected_condition();
        break;
    }  /* switch */
    /* Record this mapping for future reference. */
    if (result != NULL) {
      mep->scope = result->source_corresp.parent_scope;
    }  /* if */
    mep->entity.ptr = (char *)result;
    mep->entity.kind = iek_type;
  }  /* if */
  return result;
}  /* type_for_ifc_type_index */


void an_ifc_module::source_position_from_locus(a_source_position  *pos,
                                               ifc_SourceLocation *locus)
                                                                 const noexcept
/*
Map the IFC locus source position information into the source position at *pos.
*/
{
  an_ifc_Source_Line   isl, *islp;
  ifc_NameSort         tag;
  a_seq_number         *seq;

  read_ifc_partition_at_index(ifc_src_line, locus->line);
  islp = get_Source_Line(&isl);
  tag = name_tag(islp->file);
  check_assertion(tag == NameSort(SourceFile));
  /* See if this file has been used before. */
  seq = &sequence_numbers[name_value(islp->file)];
  if (*seq == 0) {
    /* First time accessing this source file; record the start of a new
       source file.  Note that this may be out-of-order as it depends on the
       order that entities are used, but the full tree of source file
       references isn't available in the IFC file.  Note also that a single
       source sequence entry is allocated to the entire file (so locus->line
       is unused here -- perhaps in the future a series of sequence numbers
       can be allocated and locus->line can be added to this base sequence
       number). */
    a_const_char *file_name;
    file_name = string_from_name_index(islp->file,
                                       (a_symbol_locator *)NULL);
    file_name = copy_string_to_region(FILE_SCOPE_REGION_NUMBER, file_name);
    record_inclusion_of_module_source_file(file_name, pos);
    *seq = pos->seq;
  } else {
    pos->seq = *seq;
  }  /* if */
  pos->column = locus->column;
}  /* source_position_from_locus */


a_const_char *an_ifc_module::string_from_name_index(
                                                   ifc_NameIndex    name_index,
                                                   a_symbol_locator *loc)
                                                                 const noexcept
/*
Return the string referenced by name_index in the module specified by mod.
The returned string may not be in the IL (it may be a pointer to an mmap'ed
memory region or a pointer to a local static buffer), so the caller should copy
it if necessary.  If non-NULL, fields (like is_operator_name) in *loc are
updated accordingly.
*/
{
  a_const_char         *result = NULL, *prefix = NULL;
  ifc_NameSort         tag = name_tag(name_index);

  if (tag == NameSort(Identifier)) {
    /* NameSort::Identifiers just refer to the string table. */
    result = get_string_at_offset(name_value(name_index));
  } else {
    read_ifc_partition_at_index(ifc_name_start + (ifc_NameSort_type)tag,
                                name_value(name_index));
    switch (tag) {
      case NameSort(SourceFile):
        { an_ifc_NameSort_SourceFile inssf, *inssfp;
          inssfp = get_NameSort_SourceFile(&inssf);
          result = get_string_at_offset(inssfp->path);
        }
        break;
      case NameSort(Operator):
        { an_ifc_NameSort_Operator inso, *insop;
          insop = get_NameSort_Operator(&inso);
          if (loc != NULL) {
            /* Initialize the locator with the proper operator name. */
            make_opname_locator(opname_from_category(insop->category), loc,
                                &null_source_position);
            result = loc->symbol_header->identifier;
          } else {
            prefix = "operator";
            result = get_string_at_offset(insop->encoded);
          }  /* if */
        }
        break;
      case NameSort(Conversion):
        { an_ifc_NameSort_Conversion insc, *inscp;
          a_type_ptr                 target_type;
          inscp = get_NameSort_Conversion(&insc);
          prefix = "operator ";
          target_type = type_for_ifc_type_index(inscp->target);
          /* Note that inscp->encoded contains the mangled name of the
             conversion function, so use the name from the type instead. */
          result = target_type->source_corresp.name;
          if (loc != NULL) {
            /* Set the locator as appropriate for a conversion function. */
            make_type_conversion_locator(target_type, loc,
                                         &null_source_position);
          }  /* if */
        }
        break;
      case NameSort(Literal):
        { an_ifc_NameSort_Literal insl, *inslp;
          inslp = get_NameSort_Literal(&insl);
          result = get_string_at_offset(inslp->encoded);
          if (loc != NULL) {
            /* Set the locator as appropriate for a user-defined literal
               operator (but skip the initial ""). */
            check_assertion(result[0] == '"' && result[1] == '"');
            result += 2;
            make_literal_opname_locator(result, strlen(result), loc,
                                        &null_source_position);
            /* FIXME: set this? */
            loc->is_udl_operator_name = TRUE;
            result = loc->symbol_header->identifier;
          } else {
            /* Microsoft doesn't use a space after the "operator" string. */
            prefix = "operator";
          }  /* if */
        }
        break;
      case NameSort(Template):
      case NameSort(Specialization):
        unexpected_condition(); /* FIXME: not implemented yet. */
        break;
      case NameSort(Identifier):
      default:
        unexpected_condition();
    }  /* switch */
  }  /* if */
  if (prefix != NULL) {
    /* If a prefix was specified, add it now. */
    if (operator_text_buffer == NULL) {
      operator_text_buffer = alloc_text_buffer(20);
    }  /* if */
    reset_text_buffer(operator_text_buffer);
    add_string_to_text_buffer(operator_text_buffer, prefix);
    add_string_to_text_buffer(operator_text_buffer, result);
    add_char_to_text_buffer(operator_text_buffer, '\0');
    result = operator_text_buffer->buffer;
  }  /* if */
  return result;
}  /* string_from_name_index */


void an_ifc_module::init_dps(a_decl_parse_state          *dps,
                             ifc_SourceLocation          *locus,
                             ifc_TypeIndex               type_index,
                             ifc_Alignment               alignment,
                             ifc_ObjectTraits            traits,
                             ifc_MsvcTraits              msvc_traits,
                             ifc_BasicSpecifiers         specifiers,
                             ifc_Access                  access,
                             a_partial_scope_stack_state *psssp) const noexcept
/*
Map the IFC fields given by locus, type_index, alignment, traits, specifiers
and access to internal values used in the front end and set those fields in
*dps.  mod indicates the module file that contains the declaration.  *psssp
is a place in which to store various fields of the decl_scope_level scope_stack
entry (saved only if necessary).  restore_partial_scope_stack_if_necessary
should be called with this pointer after the declaration has been processed.
*/
{
  an_attribute_ptr ap = NULL;

  init_decl_parse_state(dps);
  if (psssp != NULL) psssp->saved = FALSE;
  if (type_index != 0) {
    dps->type = type_for_ifc_type_index(type_index);
  }  /* if */
  source_position_from_locus(&dps->start_pos, locus);
  check_assertion(alignment < targ_maximum_pack_alignment);
  dps->alignment = (a_targ_alignment)alignment;
  if (traits != ifc_ObjectTraits_None) {
    if (traits & ifc_ObjectTraits_Constexpr) {
      dps->dso_flags |= DSO_CONSTEXPR;
    }  /* if */
    if (traits & ifc_ObjectTraits_Mutable) {
      dps->dso_flags |= DSO_MUTABLE;
    }  /* if */
    if (traits & ifc_ObjectTraits_ThreadLocal) {
      dps->dso_flags |= DSO_THREAD_LOCAL;
    }  /* if */
    if (msvc_traits & ifc_MsvcTraits_Comdat) {
      unexpected_condition(); /* FIXME */
    }  /* if */
    if (msvc_traits & ifc_MsvcTraits_SelectAny) {
      ap = make_module_attribute("selectany",
                                 (a_byte_attribute_family)af_ms_declspec, ap);
    }  /* if */
    if (msvc_traits & ifc_MsvcTraits_Process) {
      ap = make_module_attribute("process",
                                 (a_byte_attribute_family)af_ms_declspec, ap);
    }  /* if */
    if (msvc_traits & ifc_MsvcTraits_DllExport) {
      ap = make_module_attribute("dllexport",
                                 (a_byte_attribute_family)af_ms_declspec, ap);
    }  /* if */
    if (msvc_traits & ifc_MsvcTraits_DllImport) {
      ap = make_module_attribute("dllimport",
                                 (a_byte_attribute_family)af_ms_declspec, ap);
    }  /* if */
    if (msvc_traits & ifc_MsvcTraits_Allocate) {
      ap = make_module_attribute("allocate",
                                 (a_byte_attribute_family)af_ms_declspec, ap);
    }  /* if */
  }  /* if */
  if (specifiers != ifc_BasicSpecifiers_Cxx) {
    if (specifiers & ifc_BasicSpecifiers_C) {
      /* Save the existing name linkage and use "C" linkage for the next
         declaration. */
      save_partial_scope_stack(psssp);
      scope_stack[decl_scope_level].name_linkage_is_explicit = TRUE;
      scope_stack[decl_scope_level].default_name_linkage =
                                             (a_name_linkage_kind)nlk_external;
      dps->decl_modifiers.direct_linkage_specifier = TRUE;
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_Internal) {
      dps->storage_class = sc_static;
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_Vague) {
      /* FIXME: unexpected_condition(); (for now) */
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_External) {
      dps->storage_class = sc_extern;
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_Deprecated) {
      ap = make_module_attribute("deprecated",
                                 (a_byte_attribute_family)af_ms_declspec, ap);
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_InitializedInClass) {
      unexpected_condition(); /* FIXME */
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_NonExported) {
      unexpected_condition(); /* FIXME */
    }  /* if */
  }  /* if */
  if (ap != NULL) {
    dps->prefix_attributes = ap;
  }  /* if */
  if (access != Access(None)) {
    an_access_specifier il_access;
    switch (access) {
      case Access(Private):   il_access = as_private;   break;
      case Access(Protected): il_access = as_protected; break;
      case Access(Public):    il_access = as_public;    break;
      default:
        unexpected_condition();
    }  /* switch */
    save_partial_scope_stack(psssp);
    scope_stack[decl_scope_level].current_access = il_access;
  }  /* if */
}  /* init_dps */


void an_ifc_module::init_locator_from_name(ifc_NameIndex      name_index,
                                           ifc_TextOffset     text_offset,
                                           ifc_SourceLocation *locus,
                                           a_symbol_locator   *loc)
                                                                 const noexcept
/*
Initialize the locator specified by *loc.  The entity is in the module file
indicated by mod and the name of the entity is either given by name_index or
text_offset, whichever is non-zero.  The source position is given by locus.
FIXME: Not sure if we need source location here.
*/
{
  a_source_position pos;
  a_const_char      *name;

  source_position_from_locus(&pos, locus);
  clear_locator(loc, &pos);
  if (name_index != 0) {
    name = string_from_name_index(name_index, loc);
  } else {
    name = get_string_at_offset(text_offset);
  }  /* if */
  if (!loc->is_operator_name &&
      !loc->is_conversion_name &&
      !loc->is_udl_operator_name) {
    /* Find the symbol (if not a special case). */
    (void)find_symbol(name, (sizeof_t)strlen(name), loc);
  }  /* if */
}  /* init_locator_from_name */


a_constant_ptr an_ifc_module::constant_for_expr_index(
                                                    ifc_ExprIndex expr_index,
                                                    a_type_ptr    default_type)
                                                                 const noexcept
/*
Returns a constant (allocated in the current IL memory region) with the value
specified by the ExprIndex.  Assumes the expression is constant.  If the
expression's type is zero, use the default_type as the expression's type.
FIXME: shared or unshared?
FIXME: what other expressions can we get here?
*/
{
  ifc_ExprSort              tag = expr_tag(expr_index);
  a_constant_ptr            cp = NULL;

  /* Prepare to read from the proper partition for this expression. */
  read_ifc_partition_at_index(ifc_expr_start + (ifc_ExprSort_type)tag,
                              expr_value(expr_index));
  switch (tag) {
    case ExprSort(Literal):
      { an_ifc_ExprSort_Literal iesl, *ieslp;
        a_type_ptr              constant_type, stripped_type;
        ieslp = get_ExprSort_Literal(&iesl);
        if (ieslp->type == 0) {
          /* If the expression doesn't have its own type, use the default
             type provided by the caller. */
          check_assertion(default_type != NULL);
          constant_type = default_type;
        } else {
          constant_type = type_for_ifc_type_index(ieslp->type);
        }  /* if */
        switch (literal_tag(ieslp->value)) {
          case LiteralSort(Immediate):
            /* An immediate literal (30 bits or less). */
            cp = alloc_constant(ck_integer);
            if (ieslp->type == 0) {
              /* FIXME: not sure why the type is zero in some cases. */
              set_unsigned_integer_constant(cp,
                            (a_host_large_unsigned)literal_index(ieslp->value),
                            (an_integer_kind)ik_unsigned_int);
            } else {
              stripped_type = skip_typerefs(constant_type);
              check_assertion(stripped_type->kind == (a_type_kind)tk_integer);
              set_unsigned_integer_constant(cp,
                            (a_host_large_unsigned)literal_index(ieslp->value),
                            stripped_type->variant.integer.int_kind);
              cp->type = constant_type;
            }  /* if */
            break;
          case LiteralSort(Integer):
            /* An integer larger than 30 bits. */
            { a_host_large_unsigned value;
              cp = alloc_constant(ck_integer);
              read_ifc_partition_at_index(ifc_const_i64,
                                          literal_index(ieslp->value));
              GET_64bit_int(value);
              stripped_type = skip_typerefs(constant_type);
              check_assertion(stripped_type->kind == (a_type_kind)tk_integer);
              set_unsigned_integer_constant(cp,
                                      (a_host_large_unsigned)value,
                                      stripped_type->variant.integer.int_kind);
              cp->type = constant_type;
            }
            break;
          case LiteralSort(FloatingPoint):
            /* FIXME: not yet implemented. */
          default:
            unexpected_condition();
        }  /* switch */
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return cp;
}  /* constant_for_expr_index */


void an_ifc_module::str_ifc_text_offset(ifc_TextOffset     offset,
                                        a_str_control_block *scbp)
                                                                 const noexcept
/*
Add the string at the specified offset to the output buffer.
*/
{
  add_string_to_text_buffer(scbp->text_buffer,
                            get_string_at_offset(offset));
}  /* str_ifc_text_offset */


void an_ifc_module::str_ifc_name_index(ifc_NameIndex       name_index,
                                       a_str_control_block *scbp)
                                                                 const noexcept
/*
Add the string represented by name_index to the current output buffer.
*/
{
  add_string_to_text_buffer(scbp->text_buffer,
                            string_from_name_index(name_index,
                                                   (a_symbol_locator*)NULL));
}  /* str_ifc_name_index */


void an_ifc_module::str_ifc_class_name(ifc_DeclIndex       home_scope,
                                       a_str_control_block *scbp)
                                                                 const noexcept
/*
For constructors and destructors, add the name of the class specified
by home_scope to the output buffer.
*/
{
  ifc_DeclSort_type     tag = decl_tag_as_type(home_scope);
  an_ifc_DeclSort_Scope idss, *idssp;

  /* Prepare to read from the proper partition for this declaration. */
  read_ifc_partition_at_index(ifc_decl_start + tag,
                              decl_value(home_scope));
  check_assertion(tag == DeclSortAsType(Scope));
  idssp = get_DeclSort_Scope(&idss);
  str_ifc_name_index(idssp->name, scbp);
}  /* str_ifc_class_name */


void an_ifc_module::str_ifc_add_number(a_host_large_unsigned value,
                                       a_str_control_block   *scbp)
                                                                 const noexcept
/*
Add the decimal representation of value to the output buffer.
*/
{
  char     buffer[50];
  sizeof_t len = unsigned_to_string_buf(value, buffer);

  add_to_text_buffer(scbp->text_buffer, buffer, len);
}  /* str_ifc_add_number */


void an_ifc_module::str_ifc_source_location(ifc_SourceLocation  *locus,
                                            a_str_control_block *scbp)
                                                                 const noexcept
/*
Add source location to the current string.
*/
{
#if 0 /* FIXME: too noisy */
  an_ifc_Source_Line isl, *islp;

  if (!scbp->is_generated_code) {
    read_ifc_partition_at_index(scbp->module_info, ifc_src_line, locus->line);
    islp = get_Source_Line(&isl);
    add_string_to_text_buffer(scbp->text_buffer, "/*file:");
    str_ifc_name_index(islp->file, scbp);
    add_char_to_text_buffer(scbp->text_buffer, '[');
    str_ifc_add_number((a_host_large_unsigned)islp->line, scbp);
    add_char_to_text_buffer(scbp->text_buffer, ':');
    str_ifc_add_number((a_host_large_unsigned)locus->column, scbp);
    add_string_to_text_buffer(scbp->text_buffer, "]*/ ");
  }  /* if */
#endif /* 0 */
}  /* str_ifc_source_location */


void an_ifc_module::str_ifc_access(ifc_Access          access,
                                   a_str_control_block *scbp) const noexcept
/*
Add an access specifier to the current string, if needed.
*/
{
  a_const_char *string;

  switch (access) {
    case Access(None):       string = NULL;         break;
    case Access(Private):    string = "private";    break;
    case Access(Protected):  string = "protected";  break;
    case Access(Public):     string = "public";     break;
    default:
      unexpected_condition();
  }  /* switch */
  if (string != NULL) {
    add_string_to_text_buffer(scbp->text_buffer, string);
  }  /* if */
}  /* str_ifc_access */


void an_ifc_module::str_ifc_alignment(ifc_Alignment       alignment,
                                      a_str_control_block *scbp) const noexcept
/*
Add an alignment specifier to the current string, if needed.
*/
{
  if (alignment != 0) {
    add_string_to_text_buffer(scbp->text_buffer, "alignas(");
    str_ifc_add_number((a_host_large_unsigned)alignment, scbp);
    add_string_to_text_buffer(scbp->text_buffer, ") ");
  }  /* if */
}  /* str_ifc_alignment */


void an_ifc_module::str_ifc_qualifiers(ifc_Qualifiers      qualifiers,
                                       a_str_control_block *scbp)
                                                                 const noexcept
/*
Add strings representing qualifiers, if any.
*/
{
  if (qualifiers != ifc_Qualifier_None) {
    if (qualifiers & ifc_Qualifier_Const) {
      add_string_to_text_buffer(scbp->text_buffer, "const ");
    }  /* if */
    if (qualifiers & ifc_Qualifier_Volatile) {
      add_string_to_text_buffer(scbp->text_buffer, "volatile ");
    }  /* if */
    if (qualifiers & ifc_Qualifier_Restrict) {
      add_string_to_text_buffer(scbp->text_buffer, "__restrict ");
    }  /* if */
  }  /* if */
}  /* str_ifc_qualifiers */


void an_ifc_module::str_ifc_basic_specifiers(ifc_BasicSpecifiers specifiers,
                                             a_str_control_block *scbp)
                                                                 const noexcept
/*
Add strings representing basic specifiers, if any.
*/
{
  if (specifiers != ifc_BasicSpecifiers_Cxx) {
    if (specifiers & ifc_BasicSpecifiers_C) {
      add_string_to_text_buffer(scbp->text_buffer, "extern \"C\" ");
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_Internal) {
      add_string_to_text_buffer(scbp->text_buffer, "static ");
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_Vague) {
      /* FIXME: */
      add_string_to_text_buffer(scbp->text_buffer, "vague? ");
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_External) {
      add_string_to_text_buffer(scbp->text_buffer, "extern ");
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_Deprecated) {
      add_string_to_text_buffer(scbp->text_buffer, "[[deprecated]] ");
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_InitializedInClass) {
      /* FIXME: */
      add_string_to_text_buffer(scbp->text_buffer, "/*InitializedInClass?*/");
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_NonExported) {
      /* FIXME: */
      add_string_to_text_buffer(scbp->text_buffer, "NonExported? ");
    }  /* if */
  }  /* if */
}  /* str_ifc_basic_specifiers */


void an_ifc_module::str_ifc_object_traits(ifc_ObjectTraits    traits,
                                          a_str_control_block *scbp)
                                                                 const noexcept
/*
Add strings representing object traits, if any.
*/
{
  if (traits != ifc_ObjectTraits_None) {
    if (traits & ifc_ObjectTraits_Constexpr) {
      add_string_to_text_buffer(scbp->text_buffer, "constexpr ");
    }  /* if */
    if (traits & ifc_ObjectTraits_Mutable) {
      add_string_to_text_buffer(scbp->text_buffer, "mutable ");
    }  /* if */
    if (traits & ifc_ObjectTraits_ThreadLocal) {
      add_string_to_text_buffer(scbp->text_buffer, "thread_local ");
    }  /* if */
    if (traits & ifc_ObjectTraits_Inline) {
      add_string_to_text_buffer(scbp->text_buffer, "inline ");
    }  /* if */
  }  /* if */
}  /* str_ifc_object_traits */


void an_ifc_module::str_ifc_msvc_traits(ifc_MsvcTraits      traits,
                                        a_str_control_block *scbp)
                                                                 const noexcept
{
  if (traits != ifc_MsvcTraits_None) {
    if (traits & ifc_MsvcTraits_ForceInline) {
      add_string_to_text_buffer(scbp->text_buffer, "__forceinline ");
    }  /* if */
    if (traits & ifc_MsvcTraits_Naked) {
      add_string_to_text_buffer(scbp->text_buffer, "__declspec(naked) ");
    }  /* if */
    if (traits & ifc_MsvcTraits_NoAlias) {
      add_string_to_text_buffer(scbp->text_buffer, "__declspec(noalias) ");
    }  /* if */
    if (traits & ifc_MsvcTraits_NoInline) {
      add_string_to_text_buffer(scbp->text_buffer, "__declspec(noinline) ");
    }  /* if */
    if (traits & ifc_MsvcTraits_Restrict) {
      add_string_to_text_buffer(scbp->text_buffer, "__declspec(restrict) ");
    }  /* if */
    if (traits & ifc_MsvcTraits_SafeBuffers) {
      add_string_to_text_buffer(scbp->text_buffer,
                                                  "__declspec(safebuffers) ");
    }  /* if */
    if (traits & ifc_MsvcTraits_DllExport) {
      add_string_to_text_buffer(scbp->text_buffer, "__declspec(dllexport) ");
    }  /* if */
    if (traits & ifc_MsvcTraits_DllImport) {
      add_string_to_text_buffer(scbp->text_buffer, "__declspec(dllimport) ");
    }  /* if */
    if (traits & ifc_MsvcTraits_CodeSegment) {
      add_string_to_text_buffer(scbp->text_buffer, "__declspec(code_seg) ");
    }  /* if */
    /* FIXME: Novtable */
    /* FIXME: IntrinsicType */
    /* FIXME: EmptyBases */
    if (traits & ifc_MsvcTraits_Process) {
      add_string_to_text_buffer(scbp->text_buffer, "__declspec(process) ");
    }  /* if */
    if (traits & ifc_MsvcTraits_Allocate) {
      add_string_to_text_buffer(scbp->text_buffer, "__declspec(allocate) ");
    }  /* if */
    if (traits & ifc_MsvcTraits_SelectAny) {
      add_string_to_text_buffer(scbp->text_buffer, "__declspec(selectany) ");
    }  /* if */
    if (traits & ifc_MsvcTraits_Comdat) {
      /* FIXME */
      add_string_to_text_buffer(scbp->text_buffer, "__declspec(comdat?) ");
    }  /* if */
    /* FIXME: Uuid */
  }  /* if */
}  /* str_ifc_msvc_traits */


void an_ifc_module::str_ifc_function_type_traits(ifc_FunctionTypeTraits traits,
                                                 a_str_control_block    *scbp)
                                                                 const noexcept
/*
Add strings representing function type traits, if any.
*/
{
  if (traits != ifc_FunctionTypeTraits_None) {
    if (traits & ifc_FunctionTypeTraits_Const) {
      add_string_to_text_buffer(scbp->text_buffer, "const ");
    }  /* if */
    if (traits & ifc_FunctionTypeTraits_Volatile) {
      add_string_to_text_buffer(scbp->text_buffer, "volatile ");
    }  /* if */
    if (traits & ifc_FunctionTypeTraits_Lvalue) {
      add_string_to_text_buffer(scbp->text_buffer, "& ");
    }  /* if */
    if (traits & ifc_FunctionTypeTraits_Rvalue) {
      add_string_to_text_buffer(scbp->text_buffer, "&& ");
    }  /* if */
  }  /* if */
}  /* str_ifc_function_type_traits */


void an_ifc_module::str_ifc_noexcept_specification(
                                            ifc_NoexceptSpecification *eh_spec,
                                            a_str_control_block       *scbp)
                                                                 const noexcept
/*
Add strings representing a noexcept specification, if any.
*/
{
  switch (eh_spec->sort) {
    case NoexceptSort(None):
      break;
    case NoexceptSort(False):
      add_string_to_text_buffer(scbp->text_buffer, "noexcept(false) ");
      break;
    case NoexceptSort(True):
      add_string_to_text_buffer(scbp->text_buffer, "noexcept(true) ");
      break;
    case NoexceptSort(Expression):
    case NoexceptSort(Weak):
    case NoexceptSort(Unenforced):
      /* FIXME: not sure what these should be */
    default:
      unexpected_condition();
  }  /* switch */
}  /* str_ifc_noexcept_specification */


void an_ifc_module::str_ifc_function_traits(ifc_FunctionTraits  traits,
                                            a_boolean           prefix,
                                            a_str_control_block *scbp)
                                                                 const noexcept
/*
Add strings representing function traits, if any.  If prefix is TRUE, emit
traits that prefix a function declaration, otherwise emit postfix traits.
*/
{
  if (traits != ifc_FunctionTraits_None) {
    if (prefix) {
      if (traits & ifc_FunctionTraits_Inline) {
        add_string_to_text_buffer(scbp->text_buffer, "inline ");
      }  /* if */
      if (traits & ifc_FunctionTraits_Constexpr) {
        add_string_to_text_buffer(scbp->text_buffer, "constexpr ");
      }  /* if */
      if (traits & ifc_FunctionTraits_Explicit) {
        add_string_to_text_buffer(scbp->text_buffer, "explicit ");
      }  /* if */
      if (traits & ifc_FunctionTraits_Virtual) {
        add_string_to_text_buffer(scbp->text_buffer, "virtual ");
      }  /* if */
      if (traits & ifc_FunctionTraits_NoReturn) {
        add_string_to_text_buffer(scbp->text_buffer, "[[noreturn]] ");
      }  /* if */
      if (traits & ifc_FunctionTraits_HiddenFriend) {
        /* FIXME: don't know what this is. */
        add_string_to_text_buffer(scbp->text_buffer,
                                                 "__declspec(hiddenfriend?) ");
      }  /* if */
    } else {
      if (traits & ifc_FunctionTraits_PureVirtual) {
        add_string_to_text_buffer(scbp->text_buffer, "= 0 ");
      }  /* if */
    }  /* if */
  }  /* if */
}  /* str_ifc_function_traits */


void an_ifc_module::str_ifc_expr_index(ifc_ExprIndex       expr_index,
                                       a_str_control_block *scbp)
                                                                 const noexcept
/*
Add a textual representation of the expression referenced by expr_index to
the output buffer.
*/
{
  ifc_ExprSort      tag = expr_tag(expr_index);

  /* Prepare to read from the proper partition for this expression. */
  read_ifc_partition_at_index(ifc_expr_start + (ifc_ExprSort_type)tag,
                              expr_value(expr_index));
  switch (tag) {
    case ExprSort(Literal):
      { an_ifc_ExprSort_Literal iesl, *ieslp;
        ieslp = get_ExprSort_Literal(&iesl);
        if (ieslp->type != 0) {
          /* FIXME: not sure why the type is zero in some cases. */
          /* Add a cast to the appropriate type. */
          add_char_to_text_buffer(scbp->text_buffer, '(');
          str_ifc_type_index(ieslp->type, scbp);
          add_char_to_text_buffer(scbp->text_buffer, ')');
        }  /* if */
        switch (literal_tag(ieslp->value)) {
          case LiteralSort(Immediate):
            str_ifc_add_number(
                            (a_host_large_unsigned)literal_index(ieslp->value),
                            scbp);
            break;
          case LiteralSort(Integer):
            { a_host_large_unsigned value;
              read_ifc_partition_at_index(ifc_const_i64,
                                          literal_index(ieslp->value));
              GET_64bit_int(value);
              str_ifc_add_number(value, scbp);
            }
            break;
          case LiteralSort(FloatingPoint):
            /* FIXME: not yet implemented. */
          default:
            unexpected_condition();
        }  /* switch */
      }
      break;
    case ExprSort(VendorExtension):
      { an_ifc_ExprSort_VendorExtension ieve;
        get_ExprSort_VendorExtension(&ieve);
        unexpected_condition_str("ExprSort::VendorExtension"
                                 " is currently unspecified.");
      }
      break;
    case ExprSort(Empty):
      { an_ifc_ExprSort_Empty iee;
        get_ExprSort_Empty(&iee);
        add_char_to_text_buffer(scbp->text_buffer, ';');
      }
      break;
    case ExprSort(Type):
      { an_ifc_ExprSort_Type iet;
        get_ExprSort_Type(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(NamedDecl):
      { an_ifc_ExprSort_NamedDecl iend;
        get_ExprSort_NamedDecl(&iend);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(UnresolvedId):
      { an_ifc_ExprSort_UnresolvedId ieuid;
        get_ExprSort_UnresolvedId(&ieuid);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(TemplateId):
      { an_ifc_ExprSort_TemplateId ietid;
        get_ExprSort_TemplateId(&ietid);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Identifier):
      { an_ifc_ExprSort_Identifier ieid;
        get_ExprSort_Identifier(&ieid);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(SimpleIdentifier):
      { an_ifc_ExprSort_SimpleIdentifier iesid;
        get_ExprSort_SimpleIdentifier(&iesid);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Pointer):
      { an_ifc_ExprSort_Pointer iep;
        get_ExprSort_Pointer(&iep);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(QualifiedName):
      { an_ifc_ExprSort_QualifiedName ieqn;
        get_ExprSort_QualifiedName(&ieqn);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Path):
      { an_ifc_ExprSort_Path iep;
        get_ExprSort_Path(&iep);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Read):
      { an_ifc_ExprSort_Read ier;
        get_ExprSort_Read(&ier);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Monad):
      { an_ifc_ExprSort_Monad iem;
        get_ExprSort_Monad(&iem);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Dyad):
      { an_ifc_ExprSort_Dyad ied;
        get_ExprSort_Dyad(&ied);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Triad):
      { an_ifc_ExprSort_Triad iet;
        get_ExprSort_Triad(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Tuple):
      { an_ifc_ExprSort_Tuple iet;
        get_ExprSort_Tuple(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Tokens):
      { an_ifc_ExprSort_Tokens iet;
        get_ExprSort_Tokens(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(String):
      { an_ifc_ExprSort_String ies;
        get_ExprSort_String(&ies);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Temporary):
      { an_ifc_ExprSort_Temporary iet;
        get_ExprSort_Temporary(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Call):
      { an_ifc_ExprSort_Call iec;
        get_ExprSort_Call(&iec);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(PushState):
      { an_ifc_ExprSort_PushState iep;
        get_ExprSort_PushState(&iep);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(TypeTraitIntrinsic):
      { an_ifc_ExprSort_TypeTraitIntrinsic ietti;
        get_ExprSort_TypeTraitIntrinsic(&ietti);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(MemberInitializer):
      { an_ifc_ExprSort_MemberInitializer iemi;
        get_ExprSort_MemberInitializer(&iemi);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(MemberAccess):
      { an_ifc_ExprSort_MemberAccess iema;
        get_ExprSort_MemberAccess(&iema);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(InheritancePath):
      { an_ifc_ExprSort_InheritancePath ieip;
        get_ExprSort_InheritancePath(&ieip);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(TemplateReference):
      { an_ifc_ExprSort_TemplateReference ietr;
        get_ExprSort_TemplateReference(&ietr);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(InitializerList):
      { an_ifc_ExprSort_InitializerList ieil;
        get_ExprSort_InitializerList(&ieil);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Cast):
      { an_ifc_ExprSort_Cast iec;
        get_ExprSort_Cast(&iec);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Condition):
      { an_ifc_ExprSort_Condition iec;
        get_ExprSort_Condition(&iec);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(ExpressionList):
      { an_ifc_ExprSort_ExpressionList ieel;
        get_ExprSort_ExpressionList(&ieel);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(AssignInitializer):
      { an_ifc_ExprSort_AssignInitializer ieai;
        get_ExprSort_AssignInitializer(&ieai);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Nullptr):
      { an_ifc_ExprSort_Nullptr ienp;
        get_ExprSort_Nullptr(&ienp);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(This):
      { an_ifc_ExprSort_This iet;
        get_ExprSort_This(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(SizeofTypeId):
      { an_ifc_ExprSort_SizeofTypeId iesotid;
        get_ExprSort_SizeofTypeId(&iesotid);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Alignof):
      { an_ifc_ExprSort_Alignof ieao;
        get_ExprSort_Alignof(&ieao);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(PackedTemplateArguments):
      { an_ifc_ExprSort_PackedTemplateArguments iepta;
        get_ExprSort_PackedTemplateArguments(&iepta);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(New):
      { an_ifc_ExprSort_New ien;
        get_ExprSort_New(&ien);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Delete):
      { an_ifc_ExprSort_Delete ied;
        get_ExprSort_Delete(&ied);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Lambda):
      { an_ifc_ExprSort_Lambda iel;
        get_ExprSort_Lambda(&iel);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(DestructorCall):
      { an_ifc_ExprSort_DestructorCall iedc;
        get_ExprSort_DestructorCall(&iedc);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Typeid):
      { an_ifc_ExprSort_Typeid ietid;
        get_ExprSort_Typeid(&ietid);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(SyntaxTree):
      { an_ifc_ExprSort_SyntaxTree iest;
        get_ExprSort_SyntaxTree(&iest);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(FunctionString):
      { an_ifc_ExprSort_FunctionString iefs;
        get_ExprSort_FunctionString(&iefs);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(CompoundString):
      { an_ifc_ExprSort_CompoundString iecs;
        get_ExprSort_CompoundString(&iecs);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(StringSequence):
      { an_ifc_ExprSort_StringSequence iess;
        get_ExprSort_StringSequence(&iess);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Initializer):
      { an_ifc_ExprSort_Initializer iei;
        get_ExprSort_Initializer(&iei);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(HierarchyConversion):
      { an_ifc_ExprSort_HierarchyConversion iehc;
        get_ExprSort_HierarchyConversion(&iehc);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Product):
      { an_ifc_ExprSort_Product iep;
        get_ExprSort_Product(&iep);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Sum):
      { an_ifc_ExprSort_SumTypeValue iestv;
        get_ExprSort_SumTypeValue(&iestv);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Subobject):
      { an_ifc_ExprSort_Subobject ieso;
        get_ExprSort_Subobject(&ieso);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Array):
      { an_ifc_ExprSort_Array iea;
        get_ExprSort_Array(&iea);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(VirtualFunction):
      { an_ifc_ExprSort_VirtualFunction ievf;
        get_ExprSort_VirtualFunction(&ievf);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(Requires):
      { an_ifc_ExprSort_Requires ier;
        get_ExprSort_Requires(&ier);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(UnaryFold):
      { an_ifc_ExprSort_UnaryFold ieuf;
        get_ExprSort_UnaryFold(&ieuf);
        /* FIXME: Handle this. */
      }
      break;
    case ExprSort(BinaryFold):
      { an_ifc_ExprSort_BinaryFold iebf;
        get_ExprSort_BinaryFold(&iebf);
        /* FIXME: Handle this. */
      }
      break;
    default:
      /* FIXME: for now: */
      add_string_to_text_buffer(scbp->text_buffer,
                                "<unimplemented expression>");
      break;
  }  /* if */
}  /* str_ifc_expr_index */


void an_ifc_module::str_ifc_scope_index(ifc_ScopeIndex      scope_index,
                                        a_str_control_block *scbp)
                                                                 const noexcept
/*
Append a textual representation of the specified scope_index to the current
output buffer.
*/
{
  an_ifc_Scope_Descriptor isd, *isdp;
  an_ifc_Scope_Member     ism, *ismp;
  unsigned int            i;

  /* A scope index of 0 indicates a missing scope, in which case there
     is nothing further to do. */
  if (scope_index != 0) {
    /* Scope indices are 1-based, so subtract one. */
    read_ifc_partition_at_index(ifc_scope_desc,
                                scope_index - 1);
    isdp = get_Scope_Descriptor(&isd);
    for (i = 0; i < isdp->cardinality; i++) {
      /* Re-enable access to scope.member partition (it changes during the
         loop). */
      read_ifc_partition_at_index(ifc_scope_member,
                                  isdp->start + i);
      ismp = get_Scope_Member(&ism);
      str_ifc_declaration(ismp->index, /*is_designated_type=*/FALSE, scbp);
    }  /* for */
  }  /* if */
}  /* str_ifc_scope_index */


void an_ifc_module::str_ifc_type_index_first_part(
                                                ifc_TypeIndex       type_index,
                                                a_str_control_block *scbp)
                                                                 const noexcept
/*
Create a string representation for the specified type (first part).
FIXME: more specific
*/
{
  ifc_TypeSort      tag = type_tag(type_index);

  /* Prepare to read from the proper partition for this type. */
  read_ifc_partition_at_index(ifc_type_start + (ifc_TypeSort_type)tag,
                              type_value(type_index));
  switch (tag) {
    case TypeSort(Fundamental):
      { a_const_char *basis_str = NULL;
        an_ifc_TypeSort_Fundamental itsf, *itsfp;
        itsfp = get_TypeSort_Fundamental(&itsf);
        /* Note: no check is made for non-sensical types (e.g., signed void).*/
        switch (itsfp->sign) {
          case TypeSign(Plain):
            break;
          case TypeSign(Signed):
            add_string_to_text_buffer(scbp->text_buffer, "signed ");
            break;
          case TypeSign(Unsigned):
            add_string_to_text_buffer(scbp->text_buffer, "unsigned ");
            break;
          default:
            unexpected_condition();
        }  /* if */
        switch (itsfp->basis) {
          case TypeBasis(Void):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "void";
            break;
          case TypeBasis(Bool):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "bool";
            break;
          case TypeBasis(Char):
            switch (itsfp->precision) {
              case TypePrecision(Default):
                basis_str = "char";
                break;
              case TypePrecision(Bit16):
                basis_str = "char16_t";
                break;
              case TypePrecision(Bit32):
                basis_str = "char32_t";
                break;
              default:
                unexpected_condition();
            }  /* switch */
            break;
          case TypeBasis(Wchar_t):
            switch (itsfp->precision) {
              case TypePrecision(Default): basis_str = "wchar_t";     break;
              case TypePrecision(Bit16):   basis_str = "wchar16_t";   break;
              case TypePrecision(Bit32):   basis_str = "wchar32_t";   break;
              /* FIXME: not sure how to map these: */
              case TypePrecision(Short):
              case TypePrecision(Long):
              case TypePrecision(Bit64):
              case TypePrecision(Bit128):
              default:
                unexpected_condition();
            }  /* switch */
            break;
          case TypeBasis(Int):
            switch (itsfp->precision) {
              case TypePrecision(Default): basis_str = "int";     break;
              case TypePrecision(Short):   basis_str = "short";   break;
              case TypePrecision(Long):    basis_str = "long";    break;
              case TypePrecision(Bit64):   basis_str = "long long"; break;
              /* FIXME: not sure how to map these: */
              case TypePrecision(Bit16):
              case TypePrecision(Bit32):
              case TypePrecision(Bit128):
              default:
                unexpected_condition();
            }  /* switch */
            break;
          case TypeBasis(Float):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "float";
            break;
          case TypeBasis(Double):
            if (itsfp->precision == TypePrecision(Long)) {
              basis_str = "long double";
            } else {
              check_assertion(itsfp->precision == TypePrecision(Default));
              basis_str = "double";
            }  /* if */
            break;
          case TypeBasis(Nullptr):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "nullptr_t";
            break;
          case TypeBasis(Auto):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "auto";
            break;
          case TypeBasis(DecltypeAuto):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "decltype(auto)";
            break;
          case TypeBasis(Class):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "class";
            break;
          case TypeBasis(Struct):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "struct";
            break;
          case TypeBasis(Union):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "union";
            break;
          case TypeBasis(Namespace):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "namespace";
            break;
          case TypeBasis(Interface):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "__interface";
            break;
          case TypeBasis(Enum):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "enum";
            break;
          case TypeBasis(Typename):
            /* FIXME: not sure what goes here. */
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "typename";
            break;
          case TypeBasis(Ellipsis):
            check_assertion(itsfp->precision == TypePrecision(Default));
            basis_str = "...";
            break;
          case TypeBasis(SegmentType):
          case TypeBasis(Function):
          case TypeBasis(Empty):
          case TypeBasis(VariableTemplate):
          default:
            /* FIXME: not implemented yet */
            unexpected_condition();
        }  /* switch */
        if (basis_str != NULL) {
          add_string_to_text_buffer(scbp->text_buffer, basis_str);
        }  /* if */
      }
      break;
    case TypeSort(Designated):
      /* A type's name (e.g., "A"). */
      { an_ifc_TypeSort_Designated itsd, *itsdp;
        itsdp = get_TypeSort_Designated(&itsd);
        str_ifc_declaration(itsdp->decl, /*is_designated_type=*/TRUE, scbp);
      }
      break;
    case TypeSort(Method): /* FIXME: for now (same structures)?): */
      { an_ifc_TypeSort_Method itsm;
        get_TypeSort_Method(&itsm);
        /* FIXME: Handle this. */
      }
      unexpected_condition(); /* FIXME: No longer same structures. */
      break;
    case TypeSort(Function):
      { an_ifc_TypeSort_Function itsf, *itsfp;
        itsfp = get_TypeSort_Function(&itsf);
        /* FIXME: need more here. */
        str_ifc_function_type_traits(itsfp->traits, scbp);
        /* Emit return type on the first pass (parameters are emitted on the
           second pass). */
        str_ifc_type_index(itsfp->target, scbp);
      }
      break;
    case TypeSort(Qualified):
      { an_ifc_TypeSort_Qualified itsq, *itsqp;
        itsqp = get_TypeSort_Qualified(&itsq);
        str_ifc_qualifiers(itsqp->qualifiers, scbp);
        str_ifc_type_index(itsqp->unqualified, scbp);
      }
      break;
    case TypeSort(Pointer):
      { an_ifc_TypeSort_Pointer itsp, *itspp;
        itspp = get_TypeSort_Pointer(&itsp);
        str_ifc_type_index(itspp->pointee, scbp);
        add_char_to_text_buffer(scbp->text_buffer, '*');
      }
      break;
    case TypeSort(PointerToMember):
      { an_ifc_TypeSort_PointerToMember itsptm, *itsptmp;
        itsptmp = get_TypeSort_PointerToMember(&itsptm);
        str_ifc_type_index(itsptmp->member, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_type_index(itsptmp->scope, scbp);
        add_string_to_text_buffer(scbp->text_buffer, "::");
      }
      break;
    case TypeSort(LvalueReference):
      { an_ifc_TypeSort_LvalueReference itslr, *itslrp;
        itslrp = get_TypeSort_LvalueReference(&itslr);
        str_ifc_type_index(itslrp->referee, scbp);
        add_char_to_text_buffer(scbp->text_buffer, '&');
      }
      break;
    case TypeSort(RvalueReference):
      { an_ifc_TypeSort_RvalueReference itsrr, *itsrrp;
        itsrrp = get_TypeSort_RvalueReference(&itsrr);
        str_ifc_type_index(itsrrp->referee, scbp);
        add_string_to_text_buffer(scbp->text_buffer, "&&");
      }
      break;
    case TypeSort(Tuple):
      { an_ifc_TypeSort_Tuple itst, *itstp;
        unsigned int i;
        itstp = get_TypeSort_Tuple(&itst);
        /* A list of types. */
        for (i = 0; i < itstp->cardinality; i++) {
          ifc_TypeIndex type_index;
          read_ifc_partition_at_index(ifc_heap_type,
                                      itstp->start + i);
          GET_TypeIndex(type_index);
          str_ifc_type_index(type_index, scbp);
          /* FIXME: not really sure what the separator should be here: */
          if (i+1 < itstp->cardinality) {
            add_char_to_text_buffer(scbp->text_buffer, ',');
          }  /* if */
        }  /* for */
      }
      break;
    case TypeSort(Array):
      { an_ifc_TypeSort_Array itsa, *itsap;
        itsap = get_TypeSort_Array(&itsa);
        str_ifc_type_index(itsap->element, scbp);
      }
      break;
    case TypeSort(Base):
      { an_ifc_TypeSort_Base itsb, *itsbp;
        itsbp = get_TypeSort_Base(&itsb);
        if (itsbp->access != Access(None)) {
          str_ifc_access(itsbp->access, scbp);
          add_char_to_text_buffer(scbp->text_buffer, ' ');
        }  /* if */
        if (itsbp->shared) {
          add_string_to_text_buffer(scbp->text_buffer, "virtual ");
        }  /* if */
        /* FIXME: just need name here: */
        str_ifc_type_index(itsbp->type, scbp);
        if (itsbp->pack_expanded) {
          add_string_to_text_buffer(scbp->text_buffer, "... ");
        }  /* if */
      }
      break;
    case TypeSort(VendorExtension):
      { an_ifc_TypeSort_VendorExtension itsve;
        get_TypeSort_VendorExtension(&itsve);
        /* FIXME: Handle this. */
      }
      break;
    case TypeSort(Deduced):
      { an_ifc_TypeSort_Deduced itsd;
        get_TypeSort_Deduced(&itsd);
        /* FIXME: Handle this. */
      }
      break;
    case TypeSort(Syntactic):
      { an_ifc_TypeSort_Syntactic itss;
        get_TypeSort_Syntactic(&itss);
        /* FIXME: Handle this. */
      }
      break;
    case TypeSort(Expansion):
      { an_ifc_TypeSort_Expansion itse;
        get_TypeSort_Expansion(&itse);
        /* FIXME: Handle this. */
      }
      break;
    case TypeSort(Typename):
      { an_ifc_TypeSort_Typename itst;
        get_TypeSort_Typename(&itst);
        /* FIXME: Handle this. */
      }
      break;
    case TypeSort(Unaligned):
      { an_ifc_TypeSort_Unaligned itsu;
        get_TypeSort_Unaligned(&itsu);
        /* FIXME: Handle this. */
      }
      break;
    case TypeSort(Decltype):
      { an_ifc_TypeSort_Decltype itsd;
        get_TypeSort_Decltype(&itsd);
        /* FIXME: Handle this. */
      }
      break;
    case TypeSort(Forall):
      { an_ifc_TypeSort_Forall itsfa;
        get_TypeSort_Forall(&itsfa);
        /* FIXME: Handle this. */
      }
      break;
    case TypeSort(SyntaxTree):
      { an_ifc_TypeSort_SyntaxTree itsst;
        get_TypeSort_SyntaxTree(&itsst);
        /* FIXME: Handle this. */
      }
      break;
    default:
#if DEBUG
      if (db_flag_is_set("ms_ignore")) {
        (void)fprintf(f_debug, "Unsupported type: %d, %d\n",
                      (ifc_TypeSort_type)tag, type_value(type_index));
      }  /* if */
#endif /* DEBUG */
      break;
  }  /* switch */
}  /* str_ifc_type_index_first_part */


void an_ifc_module::str_ifc_type_index_second_part(
                                                ifc_TypeIndex       type_index,
                                                a_str_control_block *scbp)
                                                                 const noexcept
/*
Create a string representation for the specified type (second part).
FIXME: more specific
*/
{
  ifc_TypeSort      tag = type_tag(type_index);

  /* Prepare to read from the proper partition for this type. */
  read_ifc_partition_at_index(ifc_type_start + (ifc_TypeSort_type)tag,
                              type_value(type_index));
  switch (tag) {
    case TypeSort(Fundamental):
    case TypeSort(Designated):
    case TypeSort(Qualified):
    case TypeSort(Pointer):
    case TypeSort(PointerToMember):
    case TypeSort(LvalueReference):
    case TypeSort(RvalueReference):
    case TypeSort(Tuple):
    case TypeSort(Base):
      /* Handled in str_ifc_type_index_first_part. */
      break;
    case TypeSort(Method): /* FIXME: for now (same structures)?): */
      unexpected_condition(); /* FIXME: No longer same structures. */
      break;
    case TypeSort(Function):
      { an_ifc_TypeSort_Function itsf, *itsfp;
        itsfp = get_TypeSort_Function(&itsf);
        /* FIXME: lots here. */
        if (itsfp->source == 0) {
          /* No parameters. */
          add_string_to_text_buffer(scbp->text_buffer, "()");
        } else {
          add_char_to_text_buffer(scbp->text_buffer, '(');
          str_ifc_type_index(itsfp->source, scbp);
          add_char_to_text_buffer(scbp->text_buffer, ')');
        }  /* if */
        str_ifc_noexcept_specification(&itsfp->eh_spec, scbp);
      }
      break;
    case TypeSort(Array):
      { an_ifc_TypeSort_Array itsa, *itsap;
        itsap = get_TypeSort_Array(&itsa);
        add_char_to_text_buffer(scbp->text_buffer, '[');
        str_ifc_expr_index(itsap->extent, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ']');
      }
      break;
    case TypeSort(VendorExtension):
    case TypeSort(Syntactic):
    case TypeSort(Expansion):
    case TypeSort(Typename):
    case TypeSort(Unaligned):
    case TypeSort(Decltype):
    case TypeSort(SyntaxTree):
    default:
#if DEBUG
      if (db_flag_is_set("ms_ignore")) {
        (void)fprintf(f_debug, "Unsupported type: %d, %d\n",
                      (ifc_TypeSort_type)tag, type_value(type_index));
      }  /* if */
#endif /* DEBUG */
      break;
  }  /* switch */
}  /* str_ifc_type_index_second_part */


void an_ifc_module::str_ifc_type_index(ifc_TypeIndex       type_index,
                                       a_str_control_block *scbp)
                                                                 const noexcept
/*
Add a textual representation of the specified type_index to the output buffer.
*/
{
  /* FIXME: doesn't work for array (and presumably other) types. */
  /* FIXME: perhaps get the type here and pass it to each? */
  str_ifc_type_index_first_part(type_index, scbp);
  str_ifc_type_index_second_part(type_index, scbp);
}  /* str_ifc_type_index */


void an_ifc_module::str_ifc_common_decl(ifc_SourceLocation  *locus,
                                        ifc_Access          access,
                                        ifc_BasicSpecifiers specifiers,
                                        ifc_ObjectTraits    traits,
                                        ifc_Alignment       alignment,
                                        a_str_control_block *scbp)
                                                                 const noexcept
/*
This is a utility routine to add textual representations for various aspects
common fields of a DeclIndex.  If a particular DeclIndex doesn't have a
SourceLocation, Access, BasicSpecifiers, ObjectTraits, or Alignment field, a
nominal value can be supplied (which will suppress its output).
*/
{
  str_ifc_source_location(locus, scbp);
  if (access != Access(None)) {
    str_ifc_access(access, scbp);
    add_string_to_text_buffer(scbp->text_buffer, ": ");
  }  /* if */
  if (specifiers != ifc_BasicSpecifiers_Cxx) {
    str_ifc_basic_specifiers(specifiers, scbp);
  }  /* if */
  if (traits != ifc_ObjectTraits_None) {
    str_ifc_object_traits(traits, scbp);
  }  /* if */
  if (alignment != 0) {
    str_ifc_alignment(alignment, scbp);
  }  /* if */
}  /* str_ifc_common_decl */


void an_ifc_module::str_ifc_class_definition(an_ifc_DeclSort_Scope *idssp,
                                             a_str_control_block   *scbp)
                                                                 const noexcept
/*
A utility routine to emit the textual representation of the class definition
specified by *idssp.  This is a separate routine because the front end can
scan the text generated here into an internal representation
(see scan_class_definition).  It emits the base class list and member
definitions (if any).
*/
{
  if (idssp->base != 0) {
    add_char_to_text_buffer(scbp->text_buffer, ':');
    str_ifc_type_index(idssp->base, scbp);
    add_char_to_text_buffer(scbp->text_buffer, ' ');
  }  /* if */
  if (idssp->initializer != 0) {
    add_char_to_text_buffer(scbp->text_buffer, '{');
    if (!scbp->is_generated_code) {
      add_char_to_text_buffer(scbp->text_buffer, '\n');
    }  /* if */
    str_ifc_scope_index(idssp->initializer, scbp);
    add_char_to_text_buffer(scbp->text_buffer, '}');
  }  /* if */
}  /* str_ifc_class_definition */


void an_ifc_module::str_ifc_declaration(ifc_DeclIndex       decl_index,
                                        a_boolean           is_designated_type,
                                        a_str_control_block *scbp)
                                                                 const noexcept
/*
Generate a string for the specified declaration.
FIXME: Perhaps have a "flags" argument rather than is_designated_type?
*/
{
  ifc_DeclSort             tag = decl_tag(decl_index);
  a_boolean                end_decl = TRUE;

  /* Prepare to read from the proper partition for this declaration. */
  read_ifc_partition_at_index(ifc_decl_start + (ifc_DeclSort_type)tag,
                              decl_value(decl_index));
  switch (tag) {
    case DeclSort(Variable):
      { an_ifc_DeclSort_Variable idsv, *idsvp;
        idsvp = get_DeclSort_Variable(&idsv);
        /* Emit a variable declaration. */
        str_ifc_common_decl(&idsvp->locus, idsvp->access, idsvp->specifier,
                            idsvp->traits, idsvp->alignment, scbp);
        str_ifc_type_index(idsvp->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_name_index(idsvp->name, scbp);
        if (idsvp->initializer != 0) {
          add_string_to_text_buffer(scbp->text_buffer, " = ");
          str_ifc_expr_index(idsvp->initializer, scbp);
        }  /* if */
      }
      break;
    case DeclSort(Scope):
      { an_ifc_DeclSort_Scope idss, *idssp;
        idssp = get_DeclSort_Scope(&idss);
        /* FIXME: lots missing. */
        if (!is_designated_type) {
          /* Suppress the "class/struct/union" keyword for a designated
             type. */
          str_ifc_type_index(idssp->type, scbp);
        }  /* if */
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_name_index(idssp->name, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        if (is_designated_type) {
          /* When using a designated type, only the type name needs to be
             emitted. */
          end_decl = FALSE;
        } else {
          /* Emit the type definition. */
          str_ifc_class_definition(idssp, scbp);
        }  /* if */
      }
      break;
    case DeclSort(Field):
      { an_ifc_DeclSort_Field idsf, *idsfp;
        idsfp = get_DeclSort_Field(&idsf);
        str_ifc_common_decl(&idsfp->locus, idsfp->access, idsfp->specifier,
                            idsfp->traits, idsfp->alignment, scbp);
        str_ifc_type_index(idsfp->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_text_offset(idsfp->name, scbp);
      }
      break;
    case DeclSort(Bitfield):
      { an_ifc_DeclSort_Bitfield idsb, *idsbp;
        idsbp = get_DeclSort_Bitfield(&idsb);
        str_ifc_common_decl(&idsbp->locus, idsbp->access, idsbp->specifier,
                            idsbp->traits, (ifc_Alignment)0, scbp);
        str_ifc_type_index(idsbp->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_text_offset(idsbp->name, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ':');
        str_ifc_add_number((a_host_large_unsigned)idsbp->width, scbp);
      }
      break;
    case DeclSort(Function):
      { an_ifc_DeclSort_Function idsf, *idsfp;
        idsfp = get_DeclSort_Function(&idsf);
        str_ifc_common_decl(&idsfp->locus, idsfp->access,
                            (ifc_BasicSpecifiers)ifc_BasicSpecifiers_Cxx,
                            idsfp->traits, (ifc_Alignment)0, scbp);
        str_ifc_function_traits(idsfp->traits, /*prefix=*/TRUE, scbp);
        str_ifc_type_index_first_part(idsfp->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_name_index(idsfp->name, scbp);
        str_ifc_type_index_second_part(idsfp->type, scbp);
        str_ifc_function_traits(idsfp->traits, /*prefix=*/FALSE, scbp);
        /* FIXME: default_arguments is unused */
      }
      break;
    case DeclSort(Method):
      { an_ifc_DeclSort_Method idsm, *idsmp;
        idsmp = get_DeclSort_Method(&idsm);
        str_ifc_common_decl(&idsmp->locus, idsmp->access,
                            (ifc_BasicSpecifiers)ifc_BasicSpecifiers_Cxx,
                            idsmp->traits, (ifc_Alignment)0, scbp);
        str_ifc_function_traits(idsmp->traits, /*prefix=*/TRUE, scbp);
        /* FIXME: Need to suppress return type on conversion functions. */
        str_ifc_type_index_first_part(idsmp->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_name_index(idsmp->name, scbp);
        str_ifc_type_index_second_part(idsmp->type, scbp);
        str_ifc_function_traits(idsmp->traits, /*prefix=*/FALSE, scbp);
        /* FIXME: default_arguments is unused */
      }
      break;
    case DeclSort(Intrinsic):
      { an_ifc_DeclSort_Intrinsic idsi, *idsip;
        idsip = get_DeclSort_Intrinsic(&idsi);
        str_ifc_common_decl(&idsip->locus, idsip->access,
                            (ifc_BasicSpecifiers)ifc_BasicSpecifiers_Cxx,
                            ifc_ObjectTraits_None, (ifc_Alignment)0, scbp);
        str_ifc_type_index_first_part(idsip->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_text_offset(idsip->name, scbp);
        str_ifc_type_index_second_part(idsip->type, scbp);
      }
      break;
    case DeclSort(Constructor):
      { an_ifc_DeclSort_Constructor idsc, *idscp;
        idscp = get_DeclSort_Constructor(&idsc);
        str_ifc_common_decl(&idscp->locus, idscp->access,
                            (ifc_BasicSpecifiers)ifc_BasicSpecifiers_Cxx,
                            ifc_ObjectTraits_None, (ifc_Alignment)0, scbp);
        str_ifc_function_traits(idscp->traits, /*prefix=*/TRUE, scbp);
        /* Note that idscp->name points to a "{ctor}" string, so get the
           type's name by going through the home_scope field. */
        str_ifc_class_name(idscp->home_scope, scbp);
        add_char_to_text_buffer(scbp->text_buffer, '(');
        if (idscp->source != 0) {
          str_ifc_type_index(idscp->source, scbp);
        }  /* if */
        /* FIXME: todo: idscp->default_arguments */
        add_char_to_text_buffer(scbp->text_buffer, ')');
        str_ifc_function_traits(idscp->traits, /*prefix=*/FALSE, scbp);
        /* FIXME: todo: idscp->eh_spec and idcsp->convention */
      }
      break;
    case DeclSort(Destructor):
      { an_ifc_DeclSort_Destructor idsd, *idsdp;
        idsdp = get_DeclSort_Destructor(&idsd);
        str_ifc_common_decl(&idsdp->locus, idsdp->access,
                            (ifc_BasicSpecifiers)ifc_BasicSpecifiers_Cxx,
                            ifc_ObjectTraits_None, (ifc_Alignment)0, scbp);
        str_ifc_function_traits(idsdp->traits, /*prefix=*/TRUE, scbp);
        add_char_to_text_buffer(scbp->text_buffer, '~');
        /* Note that idsdp->name points to a "{dtor}" string, so get the
           type's name by going through the home_scope field. */
        str_ifc_class_name(idsdp->home_scope, scbp);
        add_string_to_text_buffer(scbp->text_buffer, "()");
        str_ifc_function_traits(idsdp->traits, /*prefix=*/FALSE, scbp);
        /* FIXME: todo: idscp->eh_spec and idcsp->convention */
      }
      break;
    case DeclSort(Alias):
      { an_ifc_DeclSort_Alias       idsta, *idstap;
        an_ifc_TypeSort_Fundamental itsf, *itsfp;
        idstap = get_DeclSort_Alias(&idsta);
        /* FIXME: lots missing */
        check_assertion(type_tag(idstap->type) == TypeSort(Fundamental));
        /* Read the type to see what kind it is. */
        read_ifc_partition_at_index(ifc_type_fundamental,
                                    type_value(idstap->type));
        itsfp = get_TypeSort_Fundamental(&itsf);
        if (itsfp->basis == TypeBasis(Typename)) {
          /* A type alias. */
          add_string_to_text_buffer(scbp->text_buffer, "typedef ");
          str_ifc_type_index(idstap->initializer, scbp);
          add_char_to_text_buffer(scbp->text_buffer, ' ');
          str_ifc_text_offset(idstap->name, scbp);
        } else if (itsfp->basis == TypeBasis(Namespace)) {
          /* A namespace alias. */
          /* FIXME: unimplemented. */
          unexpected_condition();
        } else {
          unexpected_condition();
        }  /* if */
      }
      break;
    case DeclSort(Enumeration):
      { an_ifc_DeclSort_Enumeration idse, *idsep;
        an_ifc_TypeSort_Fundamental itsf, *itsfp;
        idsep = get_DeclSort_Enumeration(&idse);
        /* Emit an enumeration declaration. */
        str_ifc_common_decl(&idsep->locus, idsep->access, idsep->specifiers,
                            (ifc_ObjectTraits)ifc_ObjectTraits_None,
                            idsep->alignment, scbp);
        check_assertion(type_tag(idsep->type) == TypeSort(Fundamental));
        /* Read the type to see what kind it is. */
        read_ifc_partition_at_index(ifc_type_fundamental,
                                    type_value(idsep->type));
        itsfp = get_TypeSort_Fundamental(&itsf);
        add_string_to_text_buffer(scbp->text_buffer, "enum ");
        if (itsfp->basis == TypeBasis(Class)) {
          add_string_to_text_buffer(scbp->text_buffer, "class ");
        } else if (itsfp->basis == TypeBasis(Struct)) {
          add_string_to_text_buffer(scbp->text_buffer, "struct ");
        } else {
          check_assertion(itsfp->basis == TypeBasis(Enum));
        }  /* if */
        str_ifc_text_offset(idsep->name, scbp);
        if (idsep->initializer.cardinality != 0) {
          unsigned int i;
          add_char_to_text_buffer(scbp->text_buffer, '{');
          if (!scbp->is_generated_code) {
            add_char_to_text_buffer(scbp->text_buffer, '\n');
          }  /* if */
          for (i = 0; i < idsep->initializer.cardinality; i++) {
            /* The sequence doesn't specifically contain DeclIndex values --
               compute one so we can invoke ourselves recursively to process
               the enumerators. */
            str_ifc_declaration(make_decl_index(DeclSortAsType(Enumerator),
                                                idsep->initializer.start + i),
                                /*is_designated_type=*/FALSE, scbp);
            add_char_to_text_buffer(scbp->text_buffer, ',');
            if (!scbp->is_generated_code) {
              add_char_to_text_buffer(scbp->text_buffer, '\n');
            }  /* if */
          }  /* for */
          add_char_to_text_buffer(scbp->text_buffer, '}');
        }  /* if */
      }
      break;
    case DeclSort(Enumerator):
      { an_ifc_DeclSort_Enumerator idse, *idsep;
        idsep = get_DeclSort_Enumerator(&idse);
        /* Emit an enumerator declaration. */
        /* FIXME: idsep->access unused here (to suppress access field): */
        str_ifc_common_decl(&idsep->locus, Access(None), idsep->specifier,
                            (ifc_ObjectTraits)ifc_ObjectTraits_None,
                            (ifc_Alignment)0, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_text_offset(idsep->name, scbp);
        if (idsep->initializer != 0) {
          add_string_to_text_buffer(scbp->text_buffer, " = ");
          str_ifc_expr_index(idsep->initializer, scbp);
        }  /* if */
        end_decl = FALSE;
      }
      break;
    case DeclSort(VendorExtension):
      { an_ifc_DeclSort_VendorExtension idsve;
        get_DeclSort_VendorExtension(&idsve);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(Parameter):
      { an_ifc_DeclSort_Parameter idsp;
        get_DeclSort_Parameter(&idsp);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(Temploid):
      { an_ifc_DeclSort_Temploid idst;
        get_DeclSort_Temploid(&idst);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(Template):
      { an_ifc_DeclSort_Template idst;
        get_DeclSort_Template(&idst);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(PartialSpecialization):
      { an_ifc_DeclSort_PartialSpecialization idsps;
        get_DeclSort_PartialSpecialization(&idsps);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(ExplicitSpecialization):
      { an_ifc_DeclSort_ExplicitSpecialization idses;
        get_DeclSort_ExplicitSpecialization(&idses);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(ExplicitInstantiation):
      { an_ifc_DeclSort_ExplicitInstantiation idsei;
        get_DeclSort_ExplicitInstantiation(&idsei);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(Concept):
      { an_ifc_DeclSort_Concept idsc;
        get_DeclSort_Concept(&idsc);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(InheritedConstructor):
      { an_ifc_DeclSort_InheritedConstructor idsic;
        get_DeclSort_InheritedConstructor(&idsic);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(Reference):
      { an_ifc_DeclSort_Reference idsr;
        get_DeclSort_Reference(&idsr);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(Property):
      { an_ifc_DeclSort_Property idsp;
        get_DeclSort_Property(&idsp);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(OutputSegment):
      { an_ifc_DeclSort_OutputSegment idsos;
        get_DeclSort_OutputSegment(&idsos);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(UsingDeclaration):
      { an_ifc_DeclSort_UsingDeclaration idsud;
        get_DeclSort_UsingDeclaration(&idsud);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(UsingDirective):
      { an_ifc_DeclSort_UsingDirective idsud;
        get_DeclSort_UsingDirective(&idsud);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(Friend):
      { an_ifc_DeclSort_Friend idsf;
        get_DeclSort_Friend(&idsf);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(SyntaxTree):
      { an_ifc_DeclSort_SyntaxTree idsst;
        get_DeclSort_SyntaxTree(&idsst);
        /* FIXME: Handle this. */
      }
      break;
    case DeclSort(Tuple):
      { an_ifc_DeclSort_Tuple idst;
        get_DeclSort_Tuple(&idst);
        /* FIXME: Handle this. */
      }
      break;
    default:
#if DEBUG
      if (db_flag_is_set("ms_ignore")) {
        (void)fprintf(f_debug, "[unsupported declaration: %s, %d]\n",
                      db_decl_tag(tag), decl_value(decl_index));
      }  /* if */
#endif /* DEBUG */
      break;
  }  /* switch */
  if (end_decl) {
    add_char_to_text_buffer(scbp->text_buffer, ';');
    if (!scbp->is_generated_code) {
      add_char_to_text_buffer(scbp->text_buffer, '\n');
    }  /* if */
  }  /* if */
}  /* str_ifc_declaration */


void an_ifc_module::str_ifc_statement(ifc_StmtIndex       stmt_index,
                                      a_str_control_block *scbp) const noexcept
/*
Generate a string for the specified statement.
*/
{
  ifc_StmtSort tag = stmt_tag(stmt_index);

  read_ifc_partition_at_index(ifc_stmt_start + (ifc_StmtSort_type)tag,
                              stmt_value(stmt_index));
  switch (tag) {
    case StmtSort(VendorExtension):
      { an_ifc_StmtSort_VendorExtension issve;
        get_StmtSort_VendorExtension(&issve);
        unexpected_condition_str("StmtSort::VendorExtension"
                                 " is not yet handled");
      }
      break;
    case StmtSort(Empty):
      { an_ifc_StmtSort_Empty isse;
        get_StmtSort_Empty(&isse);
        unexpected_condition_str("StmtSort::Empty"
                                 " is not yet handled");
      }
      break;
    case StmtSort(If):
      { an_ifc_StmtSort_If issi;
        get_StmtSort_If(&issi);
        unexpected_condition_str("StmtSort::If"
                                 " is not yet handled");
      }
      break;
    case StmtSort(For):
      { an_ifc_StmtSort_For issf;
        get_StmtSort_For(&issf);
        unexpected_condition_str("StmtSort::For"
                                 " is not yet handled");
      }
      break;
    case StmtSort(Case):
      { an_ifc_StmtSort_Case issc;
        get_StmtSort_Case(&issc);
        unexpected_condition_str("StmtSort::Case"
                                 " is not yet handled");
      }
      break;
    case StmtSort(While):
      { an_ifc_StmtSort_While issw;
        get_StmtSort_While(&issw);
        unexpected_condition_str("StmtSort::While"
                                 " is not yet handled");
      }
      break;
    case StmtSort(Block):
      { an_ifc_StmtSort_Block issb;
        get_StmtSort_Block(&issb);
        unexpected_condition_str("StmtSort::Block"
                                 " is not yet handled");
      }
      break;
    case StmtSort(Break):
      { an_ifc_StmtSort_Break issb;
        get_StmtSort_Break(&issb);
        unexpected_condition_str("StmtSort::Break"
                                 " is not yet handled");
      }
      break;
    case StmtSort(Switch):
      { an_ifc_StmtSort_Switch isss;
        get_StmtSort_Switch(&isss);
        unexpected_condition_str("StmtSort::Switch"
                                 " is not yet handled");
      }
      break;
    case StmtSort(DoWhile):
      { an_ifc_StmtSort_DoWhile issdw;
        get_StmtSort_DoWhile(&issdw);
        unexpected_condition_str("StmtSort::DoWhile"
                                 " is not yet handled");
      }
      break;
    case StmtSort(Default):
      { an_ifc_StmtSort_Default issd;
        get_StmtSort_Default(&issd);
        unexpected_condition_str("StmtSort::Default"
                                 " is not yet handled");
      }
      break;
    case StmtSort(Continue):
      { an_ifc_StmtSort_Continue issc;
        get_StmtSort_Continue(&issc);
        unexpected_condition_str("StmtSort::Continue"
                                 " is not yet handled");
      }
      break;
    case StmtSort(Expression):
      { an_ifc_StmtSort_Expression isse;
        get_StmtSort_Expression(&isse);
        unexpected_condition_str("StmtSort::Expression"
                                 " is not yet handled");
      }
      break;
    case StmtSort(Return):
      { an_ifc_StmtSort_Return issr;
        get_StmtSort_Return(&issr);
        unexpected_condition_str("StmtSort::Return"
                                 " is not yet handled");
      }
      break;
    case StmtSort(VariableDecl):
      { an_ifc_StmtSort_VariableDecl issvd;
        get_StmtSort_VariableDecl(&issvd);
        unexpected_condition_str("StmtSort::VariableDecl"
                                 " is not yet handled");
      }
      break;
    case StmtSort(SyntaxTree):
      { an_ifc_StmtSort_SyntaxTree issst;
        get_StmtSort_SyntaxTree(&issst);
        unexpected_condition_str("StmtSort::SyntaxTree"
                                 " is not yet handled");
      }
      break;
    default:
      unexpected_condition_str("Unknown StmtSort kind");
  }  /* switch */
}  /* str_ifc_statement */


void an_ifc_module::str_ifc_string_literal(ifc_StringIndex     str_index,
                                           a_str_control_block *scbp)
                                                                 const noexcept
/*
Generate a string for the specified string literal.
*/
{
  ifc_StringSort        tag = str_tag(str_index);
  an_ifc_String_Literal str_lit;

  read_ifc_partition_at_index(ifc_const_str, str_value(str_index));
  get_String_Literal(&str_lit);
  if (tag != StringSort(Ordinary)) {
    unexpected_condition_str("Non-ordinary strings are not yet handled.");
  }  /* if */
  add_string_with_length_to_text_buffer(scbp->text_buffer,
                                        get_string_at_offset(str_lit.start),
                                        str_lit.length);
  if (str_lit.suffix != 0) {
    add_string_to_text_buffer(scbp->text_buffer,
                              get_string_at_offset(str_lit.suffix));
  }  /* if */
}  /* str_ifc_string_literal */


void an_ifc_module::str_ifc_name(ifc_NameIndex       name_index,
                                 a_str_control_block *scbp) const noexcept
/*
Generate a string for the specified name.
*/
{
  ifc_NameSort tag = name_tag(name_index);

  read_ifc_partition_at_index(ifc_name_start + (ifc_NameSort_type)tag,
                              name_value(name_index));
  switch (tag) {
    case NameSort(Identifier):
      str_ifc_name_index(name_value(name_index), scbp);
      break;
    case NameSort(Operator):
      { an_ifc_NameSort_Operator inso;
        get_NameSort_Operator(&inso);
        unexpected_condition_str("NameSort::Operator is not yet handled.");
      }
      break;
    case NameSort(Conversion):
      { an_ifc_NameSort_Conversion insc;
        get_NameSort_Conversion(&insc);
        unexpected_condition_str("NameSort::Conversion is not yet handled.");
      }
      break;
    case NameSort(Literal):
      { an_ifc_NameSort_Literal insl;
        get_NameSort_Literal(&insl);
        unexpected_condition_str("NameSort::Literal is not yet handled.");
      }
      break;
    case NameSort(Template):
      { an_ifc_NameSort_Template inst;
        get_NameSort_Template(&inst);
        unexpected_condition_str("NameSort::Template is not yet handled.");
      }
      break;
    case NameSort(Specialization):
      { an_ifc_NameSort_Specialization inss;
        get_NameSort_Specialization(&inss);
        unexpected_condition_str("NameSort::Specialization"
                                 " is not yet handled.");
      }
      break;
    case NameSort(SourceFile):
      { an_ifc_NameSort_SourceFile inssf;
        get_NameSort_SourceFile(&inssf);
        unexpected_condition_str("NameSort::SourceFile is not yet handled.");
      }
      break;
    default:
      unexpected_condition_str("Unknown ChartSort kind");
  }  /* switch */
}  /* str_ifc_name */


void an_ifc_module::str_ifc_chart(ifc_ChartIndex      chart_index,
                                  a_str_control_block *scbp) const noexcept
/*
Generate a string for the specified chart.
*/
{
  ifc_ChartSort tag = chart_tag(chart_index);

  read_ifc_partition_at_index(ifc_chart_start + (ifc_ChartSort_type)tag,
                              chart_value(chart_index));
  switch (tag) {
    case ChartSort(None):
      { an_ifc_ChartSort_None icsn;
        get_ChartSort_None(&icsn);
        unexpected_condition_str("ChartSort::None is unspecified.");
      }
      break;
    case ChartSort(Unilevel):
      { an_ifc_ChartSort_Unilevel icsu;
        get_ChartSort_Unilevel(&icsu);
        unexpected_condition_str("ChartSort::Unilevel is unspecified.");
      }
      break;
    case ChartSort(Multilevel):
      { an_ifc_ChartSort_Multilevel icsm;
        get_ChartSort_Multilevel(&icsm);
        unexpected_condition_str("ChartSort::Multilevel is unspecified.");
      }
      break;
    default:
      unexpected_condition_str("Unknown ChartSort kind");
  }  /* switch */
}  /* str_ifc_chart */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Deprecated>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated deprecation trait.
*/
{
  an_ifc_Trait_Deprecated itd;

  read_ifc_partition_at_index(ifc_trait_deprecated, decl_index);
  get_Trait_Deprecated(&itd);
  unexpected_condition_str("AssociatedTrait<Deprecated> is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_Deprecated> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Specialization>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated template specialization trait.
*/
{
  an_ifc_Trait_Specialization its;

  read_ifc_partition_at_index(ifc_trait_specialization, decl_index);
  get_Trait_Specialization(&its);
  unexpected_condition_str("AssociatedTrait<Specialization>"
                           " is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_Specialization> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Friend>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated class friend trait.
*/
{
  an_ifc_Trait_Friend itf;

  read_ifc_partition_at_index(ifc_trait_friend, decl_index);
  get_Trait_Friend(&itf);
  unexpected_condition_str("AssociatedTrait<Friend> is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_Friend> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_ConstexprFunction>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated constexpr function trait.
*/
{
  an_ifc_Trait_ConstexprFunction itcf;

  read_ifc_partition_at_index(ifc_trait_constexpr_function, decl_index);
  get_Trait_ConstexprFunction(&itcf);
  unexpected_condition_str("AssociatedTrait<ConstexprFunction>"
                           " is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_ConstexprFunction> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_FunctionTemplate>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated function template trait.
*/
{
  an_ifc_Trait_FunctionTemplate itft;

  read_ifc_partition_at_index(ifc_trait_function_template, decl_index);
  get_Trait_FunctionTemplate(&itft);
  unexpected_condition_str("AssociatedTrait<FunctionTemplate>"
                           " is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_FunctionTemplate> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_ClassTemplate>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated class template trait.
*/
{
  an_ifc_Trait_ClassTemplate itct;

  read_ifc_partition_at_index(ifc_trait_class_template, decl_index);
  get_Trait_ClassTemplate(&itct);
  unexpected_condition_str("AssociatedTrait<ClassTemplate> is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_ClassTemplate> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_AliasTemplate>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated template alias trait.
*/
{
  an_ifc_Trait_AliasTemplate itat;

  read_ifc_partition_at_index(ifc_trait_alias_template, decl_index);
  get_Trait_AliasTemplate(&itat);
  unexpected_condition_str("AssociatedTrait<AliasTemplate> is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_AliasTemplate> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_VariableTemplate>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated variable template trait.
*/
{
  an_ifc_Trait_VariableTemplate itvt;

  read_ifc_partition_at_index(ifc_trait_variable_template, decl_index);
  get_Trait_VariableTemplate(&itvt);
  unexpected_condition_str("AssociatedTrait<VariableTemplate>"
                           " is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_VariableTemplate> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcVendorTrait>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated MSVC Vendor trait.
*/
{
  an_ifc_Trait_MsvcVendorTrait itmsvct;

  read_ifc_partition_at_index(ifc_msvc_trait_vendor_traits, decl_index);
  get_Trait_MsvcVendorTrait(&itmsvct);
  str_ifc_msvc_traits(itmsvct.trait, scbp);
}  /* str_ifc_associated_trait<an_ifc_Trait_MsvcVendorTrait> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcUuid>
                                            (ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated MSVC UUID trait.
*/
{
  an_ifc_Trait_MsvcUuid itmsvcuuid;
  char                  str[16];

  read_ifc_partition_at_index(ifc_msvc_trait_uuid, decl_index);
  get_Trait_MsvcUuid(&itmsvcuuid);
  snprintf(str, sizeof(str), "%04x", itmsvcuuid.uuid);
  add_string_to_text_buffer(scbp->text_buffer, str);
}  /* str_ifc_associated_trait<an_ifc_Trait_MsvcUuid> */


void an_ifc_module::str_ifc_syntax_node(ifc_SyntaxIndex     syntax_index,
                                        a_str_control_block *scbp)
                                                                 const noexcept
/*
Generate a string for the specified syntax tree node.
*/
{
  ifc_SyntaxSort tag = syntax_tag(syntax_index);

  read_ifc_partition_at_index(ifc_syntax_start + (ifc_SyntaxSort_type)tag,
                              syntax_value(syntax_index));
  switch (tag) {
    case SyntaxSort(VendorExtension):
      { an_ifc_SyntaxSort_VendorExtension issve;
        get_SyntaxSort_VendorExtension(&issve);
        unexpected_condition_str("SyntaxSort::VendorExtension"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(SimpleTypeSpecifier):
      { an_ifc_SyntaxSort_SimpleTypeSpecifier isssts;
        get_SyntaxSort_SimpleTypeSpecifier(&isssts);
        unexpected_condition_str("SyntaxSort::SimpleTypeSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(DecltypeSpecifier):
      { an_ifc_SyntaxSort_DecltypeSpecifier issds;
        get_SyntaxSort_DecltypeSpecifier(&issds);
        unexpected_condition_str("SyntaxSort::DecltypeSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(DecltypeAutoSpecifier):
      { an_ifc_SyntaxSort_DecltypeAutoSpecifier issdas;
        get_SyntaxSort_DecltypeAutoSpecifier(&issdas);
        unexpected_condition_str("SyntaxSort::DecltypeAutoSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TypeSpecifierSeq):
      { an_ifc_SyntaxSort_TypeSpecifierSeq isstss;
        get_SyntaxSort_TypeSpecifierSeq(&isstss);
        unexpected_condition_str("SyntaxSort::TypeSpecifierSeq"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(DeclSpecifierSeq):
      { an_ifc_SyntaxSort_DeclSpecifierSeq issdss;
        get_SyntaxSort_DeclSpecifierSeq(&issdss);
        unexpected_condition_str("SyntaxSort::DeclSpecifierSeq"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(VirtualSpecifierSeq):
      { an_ifc_SyntaxSort_VirtualSpecifierSeq issvss;
        get_SyntaxSort_VirtualSpecifierSeq(&issvss);
        unexpected_condition_str("SyntaxSort::VirtualSpecifierSeq"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(NoexceptSpecification):
      { an_ifc_SyntaxSort_NoexceptSpecification issns;
        get_SyntaxSort_NoexceptSpecification(&issns);
        unexpected_condition_str("SyntaxSort::NoexceptSpecification"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ExplicitSpecifier):
      { an_ifc_SyntaxSort_ExplicitSpecifier isses;
        get_SyntaxSort_ExplicitSpecifier(&isses);
        unexpected_condition_str("SyntaxSort::ExplicitSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(EnumSpecifier):
      { an_ifc_SyntaxSort_EnumSpecifier isses;
        get_SyntaxSort_EnumSpecifier(&isses);
        unexpected_condition_str("SyntaxSort::EnumSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(EnumeratorDefinition):
      { an_ifc_SyntaxSort_EnumeratorDefinition issed;
        get_SyntaxSort_EnumeratorDefinition(&issed);
        unexpected_condition_str("SyntaxSort::EnumeratorDefinition"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ClassSpecifier):
      { an_ifc_SyntaxSort_ClassSpecifier isscs;
        get_SyntaxSort_ClassSpecifier(&isscs);
        unexpected_condition_str("SyntaxSort::ClassSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(MemberSpecification):
      { an_ifc_SyntaxSort_MemberSpecification issms;
        get_SyntaxSort_MemberSpecification(&issms);
        unexpected_condition_str("SyntaxSort::MemberSpecification"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(MemberDeclaration):
      { an_ifc_SyntaxSort_MemberDeclaration issmd;
        get_SyntaxSort_MemberDeclaration(&issmd);
        unexpected_condition_str("SyntaxSort::MemberDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(MemberDeclarator):
      { an_ifc_SyntaxSort_MemberDeclarator issmd;
        get_SyntaxSort_MemberDeclarator(&issmd);
        unexpected_condition_str("SyntaxSort::MemberDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(AccessSpecifier):
      { an_ifc_SyntaxSort_AccessSpecifier issas;
        get_SyntaxSort_AccessSpecifier(&issas);
        unexpected_condition_str("SyntaxSort::AccessSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(BaseSpecifierList):
      { an_ifc_SyntaxSort_BaseSpecifierList issbsl;
        get_SyntaxSort_BaseSpecifierList(&issbsl);
        unexpected_condition_str("SyntaxSort::BaseSpecifierList"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(BaseSpecifier):
      { an_ifc_SyntaxSort_BaseSpecifier issbs;
        get_SyntaxSort_BaseSpecifier(&issbs);
        unexpected_condition_str("SyntaxSort::BaseSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TypeId):
      { an_ifc_SyntaxSort_TypeId isstid;
        get_SyntaxSort_TypeId(&isstid);
        unexpected_condition_str("SyntaxSort::TypeId"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TrailingReturnType):
      { an_ifc_SyntaxSort_TrailingReturnType isstrt;
        get_SyntaxSort_TrailingReturnType(&isstrt);
        unexpected_condition_str("SyntaxSort::TrailingReturnType"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(Declarator):
      { an_ifc_SyntaxSort_Declarator issd;
        get_SyntaxSort_Declarator(&issd);
        unexpected_condition_str("SyntaxSort::Declarator"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(PointerDeclarator):
      { an_ifc_SyntaxSort_PointerDeclarator isspd;
        get_SyntaxSort_PointerDeclarator(&isspd);
        unexpected_condition_str("SyntaxSort::PointerDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ArrayDeclarator):
      { an_ifc_SyntaxSort_ArrayDeclarator issad;
        get_SyntaxSort_ArrayDeclarator(&issad);
        unexpected_condition_str("SyntaxSort::ArrayDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(FunctionDeclarator):
      { an_ifc_SyntaxSort_FunctionDeclarator issfd;
        get_SyntaxSort_FunctionDeclarator(&issfd);
        unexpected_condition_str("SyntaxSort::FunctionDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ArrayOrFunctionDeclarator):
      { an_ifc_SyntaxSort_ArrayOrFunctionDeclarator issafd;
        get_SyntaxSort_ArrayOrFunctionDeclarator(&issafd);
        unexpected_condition_str("SyntaxSort::ArrayOrFunctionDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ParameterDeclarator):
      { an_ifc_SyntaxSort_ParameterDeclarator isspd;
        get_SyntaxSort_ParameterDeclarator(&isspd);
        unexpected_condition_str("SyntaxSort::ParameterDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(InitDeclarator):
      { an_ifc_SyntaxSort_InitDeclarator issid;
        get_SyntaxSort_InitDeclarator(&issid);
        unexpected_condition_str("SyntaxSort::InitDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(NewDeclarator):
      { an_ifc_SyntaxSort_NewDeclarator issnd;
        get_SyntaxSort_NewDeclarator(&issnd);
        unexpected_condition_str("SyntaxSort::NewDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(SimpleDeclaration):
      { an_ifc_SyntaxSort_SimpleDeclaration isssd;
        get_SyntaxSort_SimpleDeclaration(&isssd);
        unexpected_condition_str("SyntaxSort::SimpleDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ExceptionDeclaration):
      { an_ifc_SyntaxSort_ExceptionDeclaration issed;
        get_SyntaxSort_ExceptionDeclaration(&issed);
        unexpected_condition_str("SyntaxSort::ExceptionDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ConditionDeclaration):
      { an_ifc_SyntaxSort_ConditionDeclaration isscd;
        get_SyntaxSort_ConditionDeclaration(&isscd);
        unexpected_condition_str("SyntaxSort::ConditionDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(StaticAssertDeclaration):
      { an_ifc_SyntaxSort_StaticAssertDeclaration isssad;
        get_SyntaxSort_StaticAssertDeclaration(&isssad);
        unexpected_condition_str("SyntaxSort::StaticAssertDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(AliasDeclaration):
      { an_ifc_SyntaxSort_AliasDeclaration issad;
        get_SyntaxSort_AliasDeclaration(&issad);
        unexpected_condition_str("SyntaxSort::AliasDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ConceptDefinition):
      { an_ifc_SyntaxSort_ConceptDefinition isscd;
        get_SyntaxSort_ConceptDefinition(&isscd);
        unexpected_condition_str("SyntaxSort::ConceptDefinition"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(CompoundStatement):
      { an_ifc_SyntaxSort_CompoundStatement isscs;
        get_SyntaxSort_CompoundStatement(&isscs);
        unexpected_condition_str("SyntaxSort::CompoundStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ReturnStatement):
      { an_ifc_SyntaxSort_ReturnStatement issrs;
        get_SyntaxSort_ReturnStatement(&issrs);
        unexpected_condition_str("SyntaxSort::ReturnStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(IfStatement):
      { an_ifc_SyntaxSort_IfStatement issis;
        get_SyntaxSort_IfStatement(&issis);
        unexpected_condition_str("SyntaxSort::IfStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(WhileStatement):
      { an_ifc_SyntaxSort_WhileStatement issws;
        get_SyntaxSort_WhileStatement(&issws);
        unexpected_condition_str("SyntaxSort::WhileStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(DoWhileStatement):
      { an_ifc_SyntaxSort_DoWhileStatement issdws;
        get_SyntaxSort_DoWhileStatement(&issdws);
        unexpected_condition_str("SyntaxSort::DoWhileStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ForStatement):
      { an_ifc_SyntaxSort_ForStatement issfs;
        get_SyntaxSort_ForStatement(&issfs);
        unexpected_condition_str("SyntaxSort::ForStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(InitStatement):
      { an_ifc_SyntaxSort_InitStatement issis;
        get_SyntaxSort_InitStatement(&issis);
        unexpected_condition_str("SyntaxSort::InitStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(RangeBasedForStatement):
      { an_ifc_SyntaxSort_RangeBasedForStatement issrbfs;
        get_SyntaxSort_RangeBasedForStatement(&issrbfs);
        unexpected_condition_str("SyntaxSort::RangeBasedForStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ForRangeDeclaration):
      { an_ifc_SyntaxSort_ForRangeDeclaration issfrd;
        get_SyntaxSort_ForRangeDeclaration(&issfrd);
        unexpected_condition_str("SyntaxSort::ForRangeDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(LabeledStatement):
      { an_ifc_SyntaxSort_LabeledStatement issls;
        get_SyntaxSort_LabeledStatement(&issls);
        unexpected_condition_str("SyntaxSort::LabeledStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(BreakStatement):
      { an_ifc_SyntaxSort_BreakStatement issbs;
        get_SyntaxSort_BreakStatement(&issbs);
        unexpected_condition_str("SyntaxSort::BreakStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ContinueStatement):
      { an_ifc_SyntaxSort_ContinueStatement isscs;
        get_SyntaxSort_ContinueStatement(&isscs);
        unexpected_condition_str("SyntaxSort::ContinueStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(SwitchStatement):
      { an_ifc_SyntaxSort_SwitchStatement issss;
        get_SyntaxSort_SwitchStatement(&issss);
        unexpected_condition_str("SyntaxSort::SwitchStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(GotoStatement):
      { an_ifc_SyntaxSort_GotoStatement issgs;
        get_SyntaxSort_GotoStatement(&issgs);
        unexpected_condition_str("SyntaxSort::GotoStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(DeclarationStatement):
      { an_ifc_SyntaxSort_DeclarationStatement issds;
        get_SyntaxSort_DeclarationStatement(&issds);
        unexpected_condition_str("SyntaxSort::DeclarationStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ExpressionStatement):
      { an_ifc_SyntaxSort_ExpressionStatement isses;
        get_SyntaxSort_ExpressionStatement(&isses);
        unexpected_condition_str("SyntaxSort::ExpressionStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TryBlock):
      { an_ifc_SyntaxSort_TryBlock isstb;
        get_SyntaxSort_TryBlock(&isstb);
        unexpected_condition_str("SyntaxSort::TryBlock"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(Handler):
      { an_ifc_SyntaxSort_Handler issh;
        get_SyntaxSort_Handler(&issh);
        unexpected_condition_str("SyntaxSort::Handler"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(HandlerSeq):
      { an_ifc_SyntaxSort_HandlerSeq isshs;
        get_SyntaxSort_HandlerSeq(&isshs);
        unexpected_condition_str("SyntaxSort::HandlerSeq"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(FunctionTryBlock):
      { an_ifc_SyntaxSort_FunctionTryBlock issftb;
        get_SyntaxSort_FunctionTryBlock(&issftb);
        unexpected_condition_str("SyntaxSort::FunctionTryBlock"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TypeIdListElement):
      { an_ifc_SyntaxSort_TypeIdListElement isstidle;
        get_SyntaxSort_TypeIdListElement(&isstidle);
        unexpected_condition_str("SyntaxSort::TypeIdListElement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(DynamicExceptionSpec):
      { an_ifc_SyntaxSort_DynamicExceptionSpec issdes;
        get_SyntaxSort_DynamicExceptionSpec(&issdes);
        unexpected_condition_str("SyntaxSort::DynamicExceptionSpec"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(StatementSeq):
      { an_ifc_SyntaxSort_StatementSeq issss;
        get_SyntaxSort_StatementSeq(&issss);
        unexpected_condition_str("SyntaxSort::StatementSeq"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(FunctionBody):
      { an_ifc_SyntaxSort_FunctionBody issfb;
        get_SyntaxSort_FunctionBody(&issfb);
        unexpected_condition_str("SyntaxSort::FunctionBody"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(Expression):
      { an_ifc_SyntaxSort_Expression isse;
        get_SyntaxSort_Expression(&isse);
        unexpected_condition_str("SyntaxSort::Expression"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(FunctionDefinition):
      { an_ifc_SyntaxSort_FunctionDefinition issfd;
        get_SyntaxSort_FunctionDefinition(&issfd);
        unexpected_condition_str("SyntaxSort::FunctionDefinition"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(MemberFunctionDeclaration):
      { an_ifc_SyntaxSort_MemberFunctionDeclaration issmfd;
        get_SyntaxSort_MemberFunctionDeclaration(&issmfd);
        unexpected_condition_str("SyntaxSort::MemberFunctionDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TemplateDeclaration):
      { an_ifc_SyntaxSort_TemplateDeclaration isstd;
        get_SyntaxSort_TemplateDeclaration(&isstd);
        unexpected_condition_str("SyntaxSort::TemplateDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(RequiresClause):
      { an_ifc_SyntaxSort_RequiresClause issrc;
        get_SyntaxSort_RequiresClause(&issrc);
        unexpected_condition_str("SyntaxSort::RequiresClause"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(SimpleRequirement):
      { an_ifc_SyntaxSort_SimpleRequirement isssr;
        get_SyntaxSort_SimpleRequirement(&isssr);
        unexpected_condition_str("SyntaxSort::SimpleRequirement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TypeRequirement):
      { an_ifc_SyntaxSort_TypeRequirement isstr;
        get_SyntaxSort_TypeRequirement(&isstr);
        unexpected_condition_str("SyntaxSort::TypeRequirement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(CompoundRequirement):
      { an_ifc_SyntaxSort_CompoundRequirement isscr;
        get_SyntaxSort_CompoundRequirement(&isscr);
        unexpected_condition_str("SyntaxSort::CompoundRequirement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(NestedRequirement):
      { an_ifc_SyntaxSort_NestedRequirement issnr;
        get_SyntaxSort_NestedRequirement(&issnr);
        unexpected_condition_str("SyntaxSort::NestedRequirement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(RequirementBody):
      { an_ifc_SyntaxSort_RequirementBody issrb;
        get_SyntaxSort_RequirementBody(&issrb);
        unexpected_condition_str("SyntaxSort::RequirementBody"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TypeTemplateParameter):
      { an_ifc_SyntaxSort_TypeTemplateParameter issttp;
        get_SyntaxSort_TypeTemplateParameter(&issttp);
        unexpected_condition_str("SyntaxSort::TypeTemplateParameter"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TemplateTemplateParameter):
      { an_ifc_SyntaxSort_TemplateTemplateParameter issttp;
        get_SyntaxSort_TemplateTemplateParameter(&issttp);
        unexpected_condition_str("SyntaxSort::TemplateTemplateParameter"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TypeTemplateArgument):
      { an_ifc_SyntaxSort_TypeTemplateArgument isstta;
        get_SyntaxSort_TypeTemplateArgument(&isstta);
        unexpected_condition_str("SyntaxSort::TypeTemplateArgument"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(NonTypeTemplateArgument):
      { an_ifc_SyntaxSort_NonTypeTemplateArgument issntta;
        get_SyntaxSort_NonTypeTemplateArgument(&issntta);
        unexpected_condition_str("SyntaxSort::NonTypeTemplateArgument"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TemplateParameterList):
      { an_ifc_SyntaxSort_TemplateParameterList isstpl;
        get_SyntaxSort_TemplateParameterList(&isstpl);
        unexpected_condition_str("SyntaxSort::TemplateParameterList"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TemplateArgumentList):
      { an_ifc_SyntaxSort_TemplateArgumentList isstal;
        get_SyntaxSort_TemplateArgumentList(&isstal);
        unexpected_condition_str("SyntaxSort::TemplateArgumentList"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TemplateId):
      { an_ifc_SyntaxSort_TemplateId isstid;
        get_SyntaxSort_TemplateId(&isstid);
        unexpected_condition_str("SyntaxSort::TemplateId"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(MemInitializer):
      { an_ifc_SyntaxSort_MemInitializer issmi;
        get_SyntaxSort_MemInitializer(&issmi);
        unexpected_condition_str("SyntaxSort::MemInitializer"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(CtorInitializer):
      { an_ifc_SyntaxSort_CtorInitializer issci;
        get_SyntaxSort_CtorInitializer(&issci);
        unexpected_condition_str("SyntaxSort::CtorInitializer"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(LambdaIntroducer):
      { an_ifc_SyntaxSort_LambdaIntroducer issli;
        get_SyntaxSort_LambdaIntroducer(&issli);
        unexpected_condition_str("SyntaxSort::LambdaIntroducer"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(LambdaDeclarator):
      { an_ifc_SyntaxSort_LambdaDeclarator issld;
        get_SyntaxSort_LambdaDeclarator(&issld);
        unexpected_condition_str("SyntaxSort::LambdaDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(CaptureDefault):
      { an_ifc_SyntaxSort_CaptureDefault isscd;
        get_SyntaxSort_CaptureDefault(&isscd);
        unexpected_condition_str("SyntaxSort::CaptureDefault"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(SimpleCapture):
      { an_ifc_SyntaxSort_SimpleCapture isssc;
        get_SyntaxSort_SimpleCapture(&isssc);
        unexpected_condition_str("SyntaxSort::SimpleCapture"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(InitCapture):
      { an_ifc_SyntaxSort_InitCapture issic;
        get_SyntaxSort_InitCapture(&issic);
        unexpected_condition_str("SyntaxSort::InitCapture"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ThisCapture):
      { an_ifc_SyntaxSort_ThisCapture isstc;
        get_SyntaxSort_ThisCapture(&isstc);
        unexpected_condition_str("SyntaxSort::ThisCapture"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(AttributedStatement):
      { an_ifc_SyntaxSort_AttributedStatement issas;
        get_SyntaxSort_AttributedStatement(&issas);
        unexpected_condition_str("SyntaxSort::AttributedStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(AttributedDeclaration):
      { an_ifc_SyntaxSort_AttributedDeclaration issad;
        get_SyntaxSort_AttributedDeclaration(&issad);
        unexpected_condition_str("SyntaxSort::AttributedDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(AttributeSpecifierSeq):
      { an_ifc_SyntaxSort_AttributeSpecifierSeq issass;
        get_SyntaxSort_AttributeSpecifierSeq(&issass);
        unexpected_condition_str("SyntaxSort::AttributeSpecifierSeq"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(AttributeSpecifier):
      { an_ifc_SyntaxSort_AttributeSpecifier issas;
        get_SyntaxSort_AttributeSpecifier(&issas);
        unexpected_condition_str("SyntaxSort::AttributeSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(AttributeUsingPrefix):
      { an_ifc_SyntaxSort_AttributeUsingPrefix issaup;
        get_SyntaxSort_AttributeUsingPrefix(&issaup);
        unexpected_condition_str("SyntaxSort::AttributeUsingPrefix"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(Attribute):
      { an_ifc_SyntaxSort_Attribute issa;
        get_SyntaxSort_Attribute(&issa);
        unexpected_condition_str("SyntaxSort::Attribute"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(AttributeArgumentClause):
      { an_ifc_SyntaxSort_AttributeArgumentClause issaac;
        get_SyntaxSort_AttributeArgumentClause(&issaac);
        unexpected_condition_str("SyntaxSort::AttributeArgumentClause"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(Alignas):
      { an_ifc_SyntaxSort_Alignas issa;
        get_SyntaxSort_Alignas(&issa);
        unexpected_condition_str("SyntaxSort::Alignas"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(UsingDeclaration):
      { an_ifc_SyntaxSort_UsingDeclaration issud;
        get_SyntaxSort_UsingDeclaration(&issud);
        unexpected_condition_str("SyntaxSort::UsingDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(UsingDeclarator):
      { an_ifc_SyntaxSort_UsingDeclarator issud;
        get_SyntaxSort_UsingDeclarator(&issud);
        unexpected_condition_str("SyntaxSort::UsingDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(UsingDirective):
      { an_ifc_SyntaxSort_UsingDirective issud;
        get_SyntaxSort_UsingDirective(&issud);
        unexpected_condition_str("SyntaxSort::UsingDirective"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(ArrayIndex):
      { an_ifc_SyntaxSort_ArrayIndex issai;
        get_SyntaxSort_ArrayIndex(&issai);
        unexpected_condition_str("SyntaxSort::ArrayIndex"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(SEHTry):
      { an_ifc_SyntaxSort_SEHTry issseht;
        get_SyntaxSort_SEHTry(&issseht);
        unexpected_condition_str("SyntaxSort::SEHTry"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(SEHExcept):
      { an_ifc_SyntaxSort_SEHExcept isssehe;
        get_SyntaxSort_SEHExcept(&isssehe);
        unexpected_condition_str("SyntaxSort::SEHExcept"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(SEHFinally):
      { an_ifc_SyntaxSort_SEHFinally isssehf;
        get_SyntaxSort_SEHFinally(&isssehf);
        unexpected_condition_str("SyntaxSort:: is currently unspecified.");
      }
      break;
    case SyntaxSort(SEHLeave):
      { an_ifc_SyntaxSort_SEHLeave isssehl;
        get_SyntaxSort_SEHLeave(&isssehl);
        unexpected_condition_str("SyntaxSort::SEHLeave"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(TypeTraitIntrinsic):
      { an_ifc_SyntaxSort_TypeTraitIntrinsic isstti;
        get_SyntaxSort_TypeTraitIntrinsic(&isstti);
        unexpected_condition_str("SyntaxSort::TypeTraitIntrinsic"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(Tuple):
      { an_ifc_SyntaxSort_Tuple isst;
        get_SyntaxSort_Tuple(&isst);
        unexpected_condition_str("SyntaxSort::Tuple"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(AsmStatement):
      { an_ifc_SyntaxSort_AsmStatement issas;
        get_SyntaxSort_AsmStatement(&issas);
        unexpected_condition_str("SyntaxSort::AsmStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(NamespaceAliasDefinition):
      { an_ifc_SyntaxSort_NamespaceAliasDefinition issnad;
        get_SyntaxSort_NamespaceAliasDefinition(&issnad);
        unexpected_condition_str("SyntaxSort::NamespaceAliasDefinition"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(Super):
      { an_ifc_SyntaxSort_Super isss;
        get_SyntaxSort_Super(&isss);
        unexpected_condition_str("SyntaxSort::Super"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(UnaryFoldExpression):
      { an_ifc_SyntaxSort_UnaryFoldExpression issufe;
        get_SyntaxSort_UnaryFoldExpression(&issufe);
        unexpected_condition_str("SyntaxSort::UnaryFoldExpression"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(BinaryFoldExpression):
      { an_ifc_SyntaxSort_BinaryFoldExpression issbfe;
        get_SyntaxSort_BinaryFoldExpression(&issbfe);
        unexpected_condition_str("SyntaxSort::BinaryFoldExpression"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(EmptyStatement):
      { an_ifc_SyntaxSort_EmptyStatement isses;
        get_SyntaxSort_EmptyStatement(&isses);
        unexpected_condition_str("SyntaxSort::EmptyStatement"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(StructuredBindingDeclaration):
      { an_ifc_SyntaxSort_StructuredBindingDeclaration isssbd;
        get_SyntaxSort_StructuredBindingDeclaration(&isssbd);
        unexpected_condition_str("SyntaxSort::StructuredBindingDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case SyntaxSort(StructuredBindingIdentifier):
      { an_ifc_SyntaxSort_StructuredBindingIdentifier isssbi;
        get_SyntaxSort_StructuredBindingIdentifier(&isssbi);
        unexpected_condition_str("SyntaxSort::StructuredBindingIdentifier"
                                 " is currently unspecified.");
      }
      break;
    default:
      unexpected_condition_str("Unexpected syntax sort");
      break;
  }  /* switch */
}  /* str_ifc_syntax_node */


void an_ifc_module::str_ifc_sentence(ifc_SentenceIndex   sentence_index,
                                     a_str_control_block *scbp) const noexcept
/*
Generate a string for the specified sentence.
*/
{
  an_ifc_Sentence is;

  read_ifc_partition_at_index(ifc_sentence, sentence_index);
  get_Sentence(&is);
  unexpected_condition_str("IFC Sentences currently unspecified.");
}  /* str_ifc_sentence*/


void an_ifc_module::str_ifc_word(ifc_WordIndex       word_index,
                                 a_str_control_block *scbp) const noexcept
/*
Generate a string for the specified sentence.
*/
{
  an_ifc_Word iw;

  read_ifc_partition_at_index(ifc_word, word_index);
  get_Word(&iw);
  unexpected_condition_str("IFC Words currently unspecified.");
}  /* str_ifc_word */


#if DEBUG

void an_ifc_module::db_ifc_file_header() const noexcept
/*
Display the contents of the IFC file header for the specified module.
*/
{
  /* FIXME: Print checksum */
  (void)fprintf(f_debug, "  major_version = %d\n", header.major_version);
  (void)fprintf(f_debug, "  minor_version = %d\n", header.minor_version);
  (void)fprintf(f_debug, "  abi = %d\n", header.abi);
  (void)fprintf(f_debug, "  arch = %d\n", (ifc_Architecture_type)header.arch);
  (void)fprintf(f_debug, "  dialect = %d\n", header.dialect);
  (void)fprintf(f_debug, "  string_table_bytes = 0x%08x\n",
                                                    header.string_table_bytes);
  (void)fprintf(f_debug, "  string_table_size = %d\n",
                                                     header.string_table_size);
  (void)fprintf(f_debug, "  unit = %d\n", header.unit);
  (void)fprintf(f_debug, "  src_path = 0x%08x \"%s\"\n", header.src_path,
                                     get_string_at_offset(header.src_path));
  (void)fprintf(f_debug, "  global_scope = %d\n", header.global_scope);
  (void)fprintf(f_debug, "  toc = 0x%08x\n", header.toc);
  (void)fprintf(f_debug, "  partition_count = %d\n", header.partition_count);
  (void)fprintf(f_debug, "  internal = %d\n", header.internal);
}  /* db_ifc_File_header */

#endif /* DEBUG */

void ifc_modules_one_time_init(void)
/*
Do one-time initialization of static variables defined in this file.
*/
{
  top_ifc_text_buffer = NULL;
  ifc_file_name_text_buffer = NULL;
  operator_text_buffer = NULL;
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(lazy_symbols_may_be_visible),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* ifc_modules_one_time_init */


void ifc_modules_init(void)
/*
Initialize static variables related to this file that must be initialized
for each compilation.
*/
{
  byte_buffer = NULL;
  buffer_end = NULL;
#if DEBUG
  debug_partition = NULL;
  debug_mod = NULL;
#endif /* DEBUG */
}  /* ifc_modules_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
