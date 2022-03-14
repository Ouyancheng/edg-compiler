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

ifc_modules.c -- Microsoft-specific IFC module code

*/

#include "basic_hdrs.h"
#include "fe_common.h"
#include "ifc_modules.h"

#include "class_decl.h"
#include "decl_spec.h"
#include "exprutil.h"
#include "func_def.h"
#include "literals.h"
#include "pch.h"
#include "symbol_ref.h"
#include "macro.h"

#if MICROSOFT_EXTENSIONS_ALLOWED && !STANDALONE_UTILITY_PROGRAM

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
The methods used by this file to access the contents of IFC modules (using
macro names) are not always lint-friendly, so disable some lint messages for
the duration of this file.
*/
/*lint -save -e534 -e641 -e1576 -e1502*/
/*lint -save -e1714*/ /* FIXME: temporarily disable "not referenced" */

static a_text_buffer_ptr
               file_name_buffer;
                       /* A text buffer used for processing file names from the
                          IFC. */

static a_boolean
		caching_ifc_class_scope =  FALSE;
			/* TRUE while caching an IFC class scope. */

an_error_severity
		unhandled_ifc_node_severity = es_remark;
			/* The error severity to use for individually reported
			   unhandled IFC nodes.  This has the potential to be
			   extremely spammy. */

an_error_severity
		file_contains_unhandled_nodes_sev = es_warning;
			/* The error severity to use for reporting that an IFC
			   file contains unhandled nodes.  Typically only one
			   report per module will be issued. */

/*
The routines and data structures below are used to support host-independent
access to the fields of an IFC file regardless of endianness, padding, or
alignment issues.
*/

#if DEBUG && EXPENSIVE_CHECKING
static const an_ifc_partition
		*debug_partition;
			/* Points to information about the partition currently
			   being read (for debugging purposes only). */
#endif /* DEBUG && EXPENSIVE_CHECKING */


static void cache_identifier(a_token_cache_ptr     cache,
                             a_const_char          *name,
                             a_source_position_ptr pos);

template<typename an_Index_Type>
static a_boolean validate_partition_element(an_ifc_Ref<an_Index_Type> ref);


NORETURN static unsigned char buffer_overrun(void)
/*
This routine is called if a memory buffer (which represents a portion of
a module file) terminates prematurely.  Issue a catastrophic error.
This routine returns an unsigned char so it can be used in a ?: operation
that returns an unsigned char.
*/
{
  unexpected_condition();
}  /* buffer_overrun */

#if USE_MMAP_FOR_MEMORY_REGIONS

inline void an_ifc_module::init_byte_buffer(size_t offset,
                                            size_t length)
/*
Initialize the state information used by "get_bytes", etc.  offset is the
offset from the start of the memory mapped region to be read.  length is its
size, in bytes.
*/
{
  byte_buffer = (unsigned char*)mmap_addr + offset;
  buffer_end = byte_buffer + length - 1;
}  /* init_byte_buffer */


inline void an_ifc_module::get_bytes_from_buffer(void   *entity,
                                                 size_t length)
/*
Fetch a block of bytes from the IFC file, and check for reading past the end of
the buffer.
*/
{
  /* Check for fetching too many bytes. */
  if (((unsigned char*)byte_buffer + length - 1) > buffer_end) {
    (void)buffer_overrun();
  }  /* if */
  memcpy((a_byte*)entity, byte_buffer, length);
  byte_buffer += length;
}  /* get_bytes_from_buffer */


/*
Macro to fetch a single byte from the IFC file.
*/
#define get_byte(byte)                                                        \
  (*((unsigned char*)(byte)) = (byte_buffer <= buffer_end ? *byte_buffer++    \
                                                          : buffer_overrun()))

#else /* !USE_MMAP_FOR_MEMORY_REGIONS */

inline void an_ifc_module::init_byte_buffer(size_t            offset,
                                            ARG_UNUSED size_t length)
/*
Initialize the state information used by "get_bytes", etc.  offset is the
offset from the start of the module file to be read.  length is its size, in
bytes.  (This function is non-const even though if doesn't change any data
member because it "logically" changes the state of this->f_module.)
*/
{
  fseek(f_module, offset, SEEK_SET);
}  /* init_byte_buffer */


inline void an_ifc_module::get_bytes_from_buffer(void   *entity,
                                                 size_t length)
/*
Fetch a block of bytes from the IFC file, and check for reading past the end of
the buffer.
*/
{
  /* Check for fetching too many bytes. */
  if (fread(entity, 1, length, f_module) != length) {
    (void)buffer_overrun();
  }  /* if */
}  /* get_bytes_from_buffer */


/*
Macro to fetch a single byte.
*/
#define get_byte(byte)                                                        \
  get_bytes_from_buffer(byte, 1)

#endif /* USE_MMAP_FOR_MEMORY_REGIONS */

inline void an_ifc_module::get_mismatched_endian_bytes(void   *entity,
                                                       size_t length)
/*
Get length bytes from the IFC file and convert them to the host byte order.
This routine is used only when the host byte order does not match the byte
order for the entity being read.
*/
{
  unsigned char	*ptr;

  /* Get the bytes in reverse order into "entity". */
  for (ptr = (unsigned char*)entity + length - 1; length > 0;
       length--, ptr--) {
    get_byte(ptr);
  }  /* for */
}  /* get_mismatched_endian_bytes */


inline void an_ifc_module::get_bytes(void      *entity,
                                     size_t    length,
                                     a_boolean header_bytes)
/*
Get length bytes from the IFC file.  If there's an endian mismatch between
what's being read and the host, convert the bytes to the host byte order.  If
header_bytes is TRUE, the bytes being retrieved correspond to the IFC file
header or table of contents (and are therefore known to be little-endian).
*/
{
  a_boolean reading_little_endian =
                              /*lint -e506*/ASSUME_LITTLE_ENDIAN_IFC_MODULES ||
                                            targ_little_endian || header_bytes;
  if (reading_little_endian == host_little_endian) {
    get_bytes_from_buffer(entity, length);
  } else {
    get_mismatched_endian_bytes(entity, length);
  }  /* if */
}  /* get_bytes */


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

void an_ifc_module::f_db_get_byte(a_const_char *value_str,
                                  void         *addr,
                                  size_t       length) const
/*
Utility to print some debug information for every access to an IFC module file.
*/
{
  if (db_flag_is_set("ifc_modules")) {
    if (debug_partition != NULL) {
      (void)fprintf(f_debug, "[%s:0x%08lx:%d] = ",
                    debug_partition->name,
#if USE_MMAP_FOR_MEMORY_REGIONS
                    (unsigned long)((char *)byte_buffer -
                       ((char *)mmap_addr + debug_partition->offset) - length),
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
                    (unsigned long)
                          (ftell(f_module) - debug_partition->offset - length),
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
                    (int)length);
    }  /* if */
    switch (length) {
      case 1:
        (void)fprintf(f_debug, "0x%02hhx", *((uint8_t*)addr));
        break;
      case 2:
        (void)fprintf(f_debug, "0x%04hx", *((uint16_t*)addr));
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
      default_is_unexpected();
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
read_partition_at_offset or read_partition_at_index), in the proper endianness.
These macros have the GET capitalized as a visual indicator that they change
the value of their argument.
*/
#define GET_byte(value, from_header)                                    \
  (check_size(value, 1)                                                 \
   get_bytes(&(value), 1, from_header)                                  \
   db_get_byte(stringize(value), &(value), 1))

#define GET_short(value, from_header)                                   \
  (check_size(value, 2)                                                 \
   get_bytes(&(value), 2, from_header)                                  \
   db_get_byte(stringize(value), &(value), 2))

#define GET_int(value, from_header)                                     \
  (check_size(value, 4)                                                 \
   get_bytes(&(value), 4, from_header)                                  \
   db_get_byte(stringize(value), &(value), 4))

#define GET_64bit_int(value, from_header)                               \
  (check_size(value, 8)                                                 \
   get_bytes(&(value), 8, from_header)                                  \
   db_get_byte(stringize(value), &(value), 8))

#define GET_256bit_int(value, from_header)                              \
  (check_size(value, 32)                                                \
   /*lint -e545*/                                                       \
   get_bytes(&(value), 32, from_header)                                 \
   db_get_byte(stringize(value), &(value), 32))


/*
For what appear to be nested structures in the IFC specification, if the
inner structures aren't a multiple of four bytes, padding is present in the
IFC file and must be explicitly skipped when accessing the bytes linearly.
*/
#if USE_MMAP_FOR_MEMORY_REGIONS
#define pad(bytes) (byte_buffer += (bytes))
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
#define pad(bytes) (fseek(f_module, bytes, SEEK_CUR))
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */

/*
For each fundamental type in an IFC module, define a macro to interpret the
next set of bytes as that type.  These macros have the GET capitalized as a
visual indicator that they change the value of their argument.

Handle nested structures differently (and check for padding).
*/

#define GET_ActiveMember(x, from_header)       GET_int(x, from_header)
#define GET_AttrIndex(x, from_header)          GET_int(x, from_header)
#define GET_ByteOffset(x, from_header)         GET_int(x, from_header)
#define GET_Cardinality(x, from_header)        GET_int(x, from_header)
#define GET_ChartIndex(x, from_header)         GET_int(x, from_header)
#define GET_Column(x, from_header)             GET_int(x, from_header)
#define GET_DeclIndex(x, from_header)          GET_int(x, from_header)
#define GET_EntitySize(x, from_header)         GET_int(x, from_header)
#define GET_ExprIndex(x, from_header)          GET_int(x, from_header)
#define GET_FormIndex(x, from_header)          GET_int(x, from_header)
#define GET_FormSpecIndex(x, from_header)      GET_int(x, from_header)
#define GET_Index(x, from_header)              GET_int(x, from_header)
#define GET_LanguageVersion(x, from_header)    GET_int(x, from_header)
#define GET_LineIndex(x, from_header)          GET_int(x, from_header)
#define GET_LineNumber(x, from_header)         GET_int(x, from_header)
#define GET_LitIndex(x, from_header)           GET_int(x, from_header)
#define GET_MsvcTraits(x, from_header)         GET_int(x, from_header)
#define GET_NameIndex(x, from_header)          GET_int(x, from_header)
#define GET_ParameterLevel(x, from_header)     GET_int(x, from_header)
#define GET_ParameterPosition(x, from_header)  GET_int(x, from_header)
#define GET_ScopeIndex(x, from_header)         GET_int(x, from_header)
#define GET_SegmentTraits(x, from_header)      GET_int(x, from_header)
#define GET_SegmentType(x, from_header)        GET_int(x, from_header)
#define GET_SentenceIndex(x, from_header)      GET_int(x, from_header)
#define GET_StmtIndex(x, from_header)          GET_int(x, from_header)
#define GET_StringIndex(x, from_header)        GET_int(x, from_header)
#define GET_SyntaxIndex(x, from_header)        GET_int(x, from_header)
#define GET_TextOffset(x, from_header)         GET_int(x, from_header)
#define GET_TypeIndex(x, from_header)          GET_int(x, from_header)
#define GET_UniqueID(x, from_header)           GET_int(x, from_header)
#define GET_UnitIndex(x, from_header)          GET_int(x, from_header)
#define GET_WordIndex(x, from_header)          GET_int(x, from_header)

#define GET_DyadicOperator(x, from_header)     GET_short(x, from_header)
#define GET_EHFlags(x, from_header)            GET_short(x, from_header)
#define GET_FormOperator(x, from_header)       GET_short(x, from_header)
#define GET_FunctionTraits(x, from_header)     GET_short(x, from_header)
#define GET_MonadicOperator(x, from_header)    GET_short(x, from_header)
#define GET_Operator(x, from_header)           GET_short(x, from_header)
#define GET_PackSize(x, from_header)           GET_short(x, from_header)
#define GET_TriadicOperator(x, from_header)    GET_short(x, from_header)

#define GET_Abi(x, from_header)                GET_byte(x, from_header)
#define GET_Access(x, from_header)             GET_byte(x, from_header)
#define GET_Architecture(x, from_header)       GET_byte(x, from_header)
#define GET_Associativity(x, from_header)      GET_byte(x, from_header)
#define GET_BasicSpecifiers(x, from_header)    GET_byte(x, from_header)
#define GET_CallingConvention(x, from_header)  GET_byte(x, from_header)
#define GET_DelimiterSort(x, from_header)      GET_byte(x, from_header)
#define GET_DestructorSort(x, from_header)     GET_byte(x, from_header)
#define GET_ExpansionMode(x, from_header)      GET_byte(x, from_header)
#define GET_FunctionTypeTraits(x, from_header) GET_byte(x, from_header)
#define GET_GuideTraits(x, from_header)        GET_byte(x, from_header)
#define GET_InitializerSort(x, from_header)    GET_byte(x, from_header)
#define GET_KeywordSort(x, from_header)        GET_byte(x, from_header)
#define GET_NoexceptSort(x, from_header)       GET_byte(x, from_header)
#define GET_ObjectTraits(x, from_header)       GET_byte(x, from_header)
#define GET_ParameterSort(x, from_header)      GET_byte(x, from_header)
#define GET_PointerDeclaratorSort(x, from_header) GET_byte(x, from_header)
#define GET_Qualifiers(x, from_header)         GET_byte(x, from_header)
#define GET_ReachableProperties(x, from_header)GET_byte(x, from_header)
#define GET_ReadConversionSort(x, from_header) GET_byte(x, from_header)
#define GET_ScopeTraits(x, from_header)        GET_byte(x, from_header)
#define GET_SpecializationSort(x, from_header) GET_byte(x, from_header)
#define GET_SyntaxSort(x, from_header)         GET_byte(x, from_header)
#define GET_TypeBasis(x, from_header)          GET_byte(x, from_header)
#define GET_TypePrecision(x, from_header)      GET_byte(x, from_header)
#define GET_TypeSign(x, from_header)           GET_byte(x, from_header)
#define GET_Version(x, from_header)            GET_byte(x, from_header)
#define GET_WordSort(x, from_header)           GET_byte(x, from_header)

#define GET_bool(x, from_header)               GET_byte(x, from_header)
#define GET_u8(x, from_header)                 GET_byte(x, from_header)
#define GET_u16(x, from_header)                GET_short(x, from_header)
#define GET_u32(x, from_header)                GET_int(x, from_header)

#define GET_Checksum(x, from_header)           GET_256bit_int(x, from_header)

#define GET_Sequence(x, from_header) \
                                (GET_Index((x).start, from_header), \
                                 GET_Cardinality((x).cardinality, from_header))
/*lint -esym(750,GET_ModuleReference)*/
#define GET_ModuleReference(x, from_header) \
                                   (GET_TextOffset((x).owner, from_header), \
                                    GET_TextOffset((x).partition, from_header))
#define GET_SourceLocation(x, from_header) \
                                       (GET_LineIndex((x).line, from_header), \
                                        GET_Column((x).column, from_header))
/* Note that this includes padding: */
#define GET_NoexceptSpecification(x, from_header) \
                                  (GET_SentenceIndex((x).words, from_header), \
                                   GET_NoexceptSort((x).sort, from_header), \
                                   pad(3))
#define GET_ParameterizedEntity(x, from_header) \
                               (GET_Index((x).decl, from_header), \
                                GET_SentenceIndex((x).head, from_header), \
                                GET_SentenceIndex((x).body, from_header), \
                                GET_SentenceIndex((x).attributes, from_header))

#define GET_KeywordSyntax(x, from_header) \
                                 (GET_SourceLocation((x).locus, from_header), \
                                  GET_KeywordSort((x).value, from_header), \
                                  pad(3))

#define GET_NestableWord(x, from_header) \
                                 (GET_SourceLocation((x).locus, from_header), \
                                  GET_Index((x).index, from_header), \
                                  GET_u16((x).value, from_header), \
                                  GET_WordSort((x).sort, from_header), \
                                  pad(1))

/*
Create get_* functions (which "read" each entity into a structure) for each of
the IFC entities by setting the IFC_DECL macros appropriately and including
ifc_map.h.  Make sure to use the pointer that is returned by these functions
(and not the pointer that is passed as an argument).
*/

/*
Define a generic wrapper function that delegates to the corresponding
"get_name" function for a given type "an_ifc_name".

For example, when "name" is "foo", this routine effectively boils down to:

  template<>
  an_ifc_foo *an_ifc_module::get<an_ifc_foo>(an_ifc_foo *storage,
                                             a_boolean fill_storage) const
  {
    return get_foo(storage, fill_storage);
  }
*/
#define IFC_GENERIC_GET(name) \
  template<> \
  inline concat(an_ifc_, name) * an_ifc_module::get< \
                                  concat(an_ifc_, name) >( \
                                  concat(an_ifc_, name) *storage, \
                /* Defaulted: */  a_boolean             fill_storage) \
  { \
    return concat(get_, name)(storage, fill_storage); \
  }

#if USE_MMAP_FOR_MODULES

/*
IFC files have the file header and table of contents stored in little-endian
format, while multibyte scalar values within the partitions are stored with
the endianness of the target architecture.

If both the host and the target have the same endianness, then the file layout
and the alignment/padding of the host must have exactly the same
characteristics.  In this case, simply return a pointer to a suitably-cast
byte_buffer and increment it as appropriate (the local storage argument, ptr,
is unused in this scenario).  Otherwise, the storage passed in to the function
is used to store copies of each field of the structure and each field is
indivually copied (and byte-swapped if necessary).  If fill_storage is TRUE,
copy the data into *ptr.

For example, when "name" is "foo", this routine effectively boils down to:

  an_ifc_foo *an_ifc_module::get_foo(an_ifc_foo *ptr,
                                     a_boolean  fill_storage) const
  {
    if (targ_little_endian == host_little_endian) {
      if (fill_storage) {
        memcpy(ptr, byte_buffer, sizeof(an_ifc_foo));
      } else {
        ptr = (an_ifc_foo*)byte_buffer;
      }
      byte_buffer = byte_buffer + sizeof(an_ifc_foo);
    } else {
      GET_field1_type(ptr->field1);
      GET_field2_type(ptr->field2);
    }
    return ptr;
  }
*/
#define IFC_DECL_START(name) \
  inline concat(an_ifc_, name) * an_ifc_module::concat(get_, name) ( \
                                  concat(an_ifc_, name) *ptr, \
                /* Defaulted: */  a_boolean             fill_storage) \
  { \
    if ((a_boolean)(/*lint -e506*/ASSUME_LITTLE_ENDIAN_IFC_MODULES || \
                                    targ_little_endian) \
                                                     == host_little_endian) { \
      check_assertion(byte_buffer + sizeof(concat(an_ifc_, name)) <= \
                                                             (buffer_end+1)); \
      if (fill_storage) { \
        memcpy((void*)ptr, (void*)byte_buffer, sizeof(concat(an_ifc_, name)));\
      } else { \
        ptr = ( concat(an_ifc_, name) *)byte_buffer; \
      }  /* if */ \
      byte_buffer = byte_buffer + sizeof(concat(an_ifc_, name)); \
    } else {
#define IFC_DECL_FIELD(field, type) \
      concat(GET_, type)(ptr->field, /*from_header=*/FALSE);
#define IFC_DECL_END(name) \
    }  /* if */ \
    return ptr; \
  } \
  IFC_GENERIC_GET(name)

/*
The variant for when the endianness of the IFC entity is guaranteed to be
little-endian.
*/
#define IFC_LE_DECL_START(name) \
  inline concat(an_ifc_, name) * an_ifc_module::concat(get_, name) ( \
                                  concat(an_ifc_, name) *ptr, \
                /* Defaulted: */  a_boolean             fill_storage) \
  { \
    if (host_little_endian) { \
      check_assertion(byte_buffer + sizeof(concat(an_ifc_, name)) <= \
                                                             (buffer_end+1)); \
      if (fill_storage) { \
        memcpy((void*)ptr, (void*)byte_buffer, sizeof(concat(an_ifc_, name)));\
      } else { \
        ptr = ( concat(an_ifc_, name) *)byte_buffer; \
      }  /* if */ \
      byte_buffer = byte_buffer + sizeof(concat(an_ifc_, name)); \
    } else {
#define IFC_LE_DECL_FIELD(field, type) \
      concat(GET_, type)(ptr->field, /*from_header=*/TRUE);

#else /* !USE_MMAP_FOR_MODULES */

/*
Use this case when each field must be treated separately because of endianness
or alignment/padding differences.  In this case, the storage passed in to the
function is used to store copies of each field of the structure and each field
is individually copied (and byte-swapped if necessary).  Since the local
storage argument *ptr is always filled out by this, fill_storage is unused.

For example, when "name" is "foo", and the macro is applied to an entry with
two fields, "field1" and "field2", whose types are "field1_type" and
"field2_type" respectively, the following is generated:

  static an_ifc_foo *get_foo(an_ifc_foo *ptr,
                             a_boolean  fill_storage)
  {
    GET_field1_type(ptr->field1);
    GET_field2_type(ptr->field2);
    return ptr;
  }
*/
#define IFC_DECL_START(name) \
  inline concat(an_ifc_, name) * an_ifc_module::concat(get_, name) ( \
                                  concat(an_ifc_, name) *ptr, \
                /* Defaulted: */  ARG_UNUSED a_boolean  fill_storage) \
  {
#define IFC_DECL_FIELD(field, type) \
    concat(GET_, type)(ptr->field, /*from_header=*/FALSE);
#define IFC_DECL_END(name) \
    return ptr; \
  } \
  IFC_GENERIC_GET(name)

/*
The variant for when the endianness of the IFC entity is guaranteed to be
little-endian.
*/
#define IFC_LE_DECL_FIELD(field, type) \
    concat(GET_, type)(ptr->field, /*from_header=*/TRUE);

#endif /* USE_MMAP_FOR_MODULES */

/*lint -e451 included more than once. */
#include "ifc_map.h"
/*lint +e451*/

namespace {
/*
Stores an exclusive range from start to end of an_ifc_partition_kinds.
*/
struct an_ifc_partition_kind_range {
  an_ifc_partition_kind start;
  an_ifc_partition_kind end;
};  /* an_ifc_partition_kind_range */


/*
Declare a deleted function that must be specialized via the below macro to
return the correct an_ifc_partition_kind_range based on a type T.
*/
template<typename an_ifc_Partition_Kind>
inline an_ifc_partition_kind_range get_partition_kind_range() = delete;


/*
Generate a specialization that returns the exclusive range within the
ifc_partition_kind where a given ifc_SortKind resides.  As this
transformation is defined both by names in camel and snake case, accept
both forms of the name.

For example, when SortNameCamel is "Type" and SortNameSnake is "type", the
following is generated:

  template<>
  inline an_ifc_partition_kind_range get_partition_kind_range<ifc_TypeSort>()
  {
    return {ifc_type_start, ifc_type_end};
  }
*/
#define DEF_KIND_RANGE(SortNameCamel, SortNameSnake) \
  template<> \
  inline an_ifc_partition_kind_range get_partition_kind_range< \
                                            ifc_ ## SortNameCamel ## Sort >() \
  { \
    return {ifc_ ## SortNameSnake ## _start, ifc_ ## SortNameSnake ## _end}; \
  }


/* Define the used ranges */
DEF_KIND_RANGE(Attr, attr)
DEF_KIND_RANGE(Type, type)
DEF_KIND_RANGE(Expr, expr)
DEF_KIND_RANGE(Stmt, stmt)
DEF_KIND_RANGE(Decl, decl)
DEF_KIND_RANGE(Name, name)
DEF_KIND_RANGE(Chart, chart)

/* Undefine the macro to prevent unintended usage. */
#undef DEF_KIND_RANGE

template<typename an_ifc_Partition_Kind>
inline an_ifc_partition_kind get_partition_kind(
                                               an_ifc_Partition_Kind sort_kind)
/*
Return the corresponding an_ifc_partition_kind for a given ifc_Sort_type value
sort_kind or ifc_invalid_partition if the sort kind could not be mapped to a
valid partition.
*/
{
  an_ifc_partition_kind_range kind_range =
                             get_partition_kind_range<an_ifc_Partition_Kind>();
  an_ifc_partition_kind kind =
                         (an_ifc_partition_kind)(kind_range.start + sort_kind);
  if (kind > kind_range.end) {
    kind = ifc_invalid_partition;
  }  /* if */
  return kind;
}  /* get_partition_kind */
}  /* namespace */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                      const an_ifc_module *mod,
                                                      a_module_entity_ptr mep)
  : an_ifc_partition_position(mep->variant.ifc_partition, mep->file_offset)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given module
entity pointer into an ifc partition position's partition kind and file offset
in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                          const an_ifc_module   *mod,
                                          an_ifc_partition_kind partition_kind,
                                          ifc_Index_type        index)
  : an_ifc_partition_position(partition_kind,
                              mod->partitions[partition_kind].offset + (index *
                                   mod->partitions[partition_kind].entry_size))
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
partition kind and index into an ifc partition position's partition kind and
file offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                 const an_ifc_module *mod,
                                                 ifc_AttrSort        attr_kind,
                                                 ifc_Index_type      index)
  : an_ifc_partition_position(mod, get_partition_kind(attr_kind), index)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_AttrSort" and index into an ifc partition position's partition kind and
file offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                      const an_ifc_module *mod,
                                                      ifc_AttrIndex       attr)
  : an_ifc_partition_position(mod, attr_tag(attr), attr_value(attr))
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_AttrIndex" into an ifc partition position's partition kind and file offset
in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                const an_ifc_module *mod,
                                                ifc_ChartSort       chart_kind,
                                                ifc_Index_type      index)
  : an_ifc_partition_position(mod, get_partition_kind(chart_kind), index)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_ChartSort" and index into an ifc partition position's partition kind and
file offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                     const an_ifc_module *mod,
                                                     ifc_ChartIndex      chart)
  : an_ifc_partition_position(mod, chart_tag(chart), chart_value(chart))
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_ChartIndex" into an ifc partition position's partition kind and file
offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                 const an_ifc_module *mod,
                                                 ifc_DeclSort        decl_kind,
                                                 ifc_Index_type      index)
  : an_ifc_partition_position(mod, get_partition_kind(decl_kind), index)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_DeclSort" and index into an ifc partition position's partition kind and
file offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                      const an_ifc_module *mod,
                                                      ifc_DeclIndex       decl)
  : an_ifc_partition_position(mod, decl_tag(decl), decl_value(decl))
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_DeclIndex" into an ifc partition position's partition kind and file offset
in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                 const an_ifc_module *mod,
                                                 ifc_ExprSort        expr_kind,
                                                 ifc_Index_type      index)
  : an_ifc_partition_position(mod, get_partition_kind(expr_kind), index)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_ExprSort" and index into an ifc partition position's partition kind and
file offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                      const an_ifc_module *mod,
                                                      ifc_ExprIndex       expr)
  : an_ifc_partition_position(mod, expr_tag(expr), expr_value(expr))
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_ExprIndex" into an ifc partition position's partition kind and file offset
in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                 const an_ifc_module *mod,
                                                 ifc_FormSort        form_kind,
                                                 ifc_Index_type      index)
  : an_ifc_partition_position(mod, (an_ifc_partition_kind)(ifc_form_start +
                                                                    form_kind),
                              index)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_FormSort" and index into an ifc partition position's partition kind and
file offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                      const an_ifc_module *mod,
                                                      ifc_FormIndex       form)
  : an_ifc_partition_position(mod, form_tag(form), form_index(form))
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_FormIndex" into an ifc partition position's partition kind and file offset
in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                 const an_ifc_module *mod,
                                                 ifc_FormSpecIndex   form_spec)
  : an_ifc_partition_position(mod, ifc_form_spec, form_spec)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_FormSpecIndex" into an ifc partition position's partition kind and file
offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                 const an_ifc_module *mod,
                                                 ifc_LineIndex       line)
  : an_ifc_partition_position(mod, ifc_src_line, line)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_LineIndex" into an ifc partition position's partition kind and file offset
in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                const an_ifc_module *mod,
                                                ifc_MacroSort       macro_kind,
                                                ifc_Index_type      index)
  : an_ifc_partition_position(mod, (an_ifc_partition_kind)(ifc_macro_start +
                                                                   macro_kind),
                              index)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_MacroSort" and index into an ifc partition position's partition kind and
file offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                     const an_ifc_module *mod,
                                                     ifc_MacroIndex      macro)
  : an_ifc_partition_position(mod, macro_tag(macro), macro_index(macro))
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_MacroIndex" into an ifc partition position's partition kind and file
offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                 const an_ifc_module *mod,
                                                 ifc_NameSort        name_kind,
                                                 ifc_Index_type      index)
  : an_ifc_partition_position(mod, get_partition_kind(name_kind), index)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_NameSort" and index into an ifc partition position's partition kind and
file offset in the given module.
*/
{
  /* FIXME: Handle this check some other way.  This NameSort case is very
     special, there's not a corresponding partition that needs read, when
     NameSort is Identifier the string table is read from directly. */
  check_assertion(name_kind != ifc_NameSort_Identifier);
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                      const an_ifc_module *mod,
                                                      ifc_NameIndex       name)
  : an_ifc_partition_position(mod, name_tag(name), name_value(name))
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_NameIndex" into an ifc partition position's partition kind and file offset
in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                     const an_ifc_module *mod,
                                                     ifc_ScopeIndex      scope)
  : an_ifc_partition_position(mod, ifc_scope_desc, scope - 1)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_ScopeIndex" into an ifc partition position's partition kind and file
offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                 const an_ifc_module *mod,
                                                 ifc_StmtSort        stmt_kind,
                                                 ifc_Index_type      index)
  : an_ifc_partition_position(mod, get_partition_kind(stmt_kind), index)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_StmtSort" and index into an ifc partition position's partition kind and
file offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                      const an_ifc_module *mod,
                                                      ifc_StmtIndex       stmt)
  : an_ifc_partition_position(mod, stmt_tag(stmt), stmt_value(stmt))
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_StmtIndex" into an ifc partition position's partition kind and file offset
in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                               const an_ifc_module *mod,
                                               ifc_SyntaxSort      syntax_kind,
                                               ifc_Index_type      index)
  : an_ifc_partition_position(mod, (an_ifc_partition_kind)(ifc_syntax_start +
                                                                  syntax_kind),
                              index)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_SyntaxSort" and index into an ifc partition position's partition kind and
file offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                    const an_ifc_module *mod,
                                                    ifc_SyntaxIndex     syntax)
  : an_ifc_partition_position(mod, syntax_tag(syntax), syntax_value(syntax))
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_SyntaxIndex" into an ifc partition position's partition kind and file
offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                 const an_ifc_module *mod,
                                                 ifc_TypeSort        type_kind,
                                                 ifc_Index_type      index)
  : an_ifc_partition_position(mod, get_partition_kind(type_kind), index)
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_TypeSort" and index into an ifc partition position's partition kind and
file offset in the given module.
*/
{
} /* an_ifc_partition_position */


inline an_ifc_partition_position::an_ifc_partition_position(
                                                      const an_ifc_module *mod,
                                                      ifc_TypeIndex       type)
  : an_ifc_partition_position(mod, type_tag(type), type_value(type))
/*
Overload wrapper for "an_ifc_partition_position" that converts the given
"ifc_TypeIndex" into an ifc partition position's partition kind and file offset
in the given module.
*/
{
} /* an_ifc_partition_position */


namespace {
/*
An encapsulated representation of an IFC node.  This encapsulation abstracts
away the exact location of the underlying IFC node.

Construction should be performed by the "construct_node" function or a similar
"construct_node_XXX" function which uses an appropriately reduced level of
validation.

As an example, to retrieve a fully validated node at position "pos" in module
"mod" of IFC node type "foo":

  Opt<an_ifc_Node<Foo>> opt_foo;

  construct_node(&opt_foo, mod, pos);

Similarly, to retrieve a node at position "pos" in module "mod", where the node
is known to be previously validated, of IFC node type "foo":

  an_ifc_Node<Foo> foo;

  construct_node_prechecked(&foo, mod, pos);

*/
template<typename an_ifc_Node_type>
struct an_ifc_Node {
  an_ifc_Node()
    : storing_value(FALSE), node_ptr(NULL)
    {}
  an_ifc_Node(an_ifc_Node_type *node_ptr_val)
    : storing_value(FALSE), node_ptr(node_ptr_val)
    { check_assertion(node_ptr_val != NULL); }
  an_ifc_Node(an_ifc_Node_type node_val)
    : storing_value(TRUE), node(node_val)
    {}
#if CHECKING
  /* A default state is provided for forward declaration.  This struct
     shouldn't remain "uninitialized." */
  ~an_ifc_Node()
    { check_assertion(storing_value || node_ptr != NULL); }
#endif /* CHECKING */

  inline const an_ifc_Node_type *operator->() const;
private:
  a_boolean storing_value;
                        /* TRUE when the underlying IFC node is stored as a
                           member and should be obtained from the "node" data
                           member.  FALSE when the underlying IFC node is
                           stored at a different address and should be obtained
                           from the "node_ptr" data member. */
  union {
    an_ifc_Node_type
                 node;
                        /* When "storing_value" is TRUE this data member holds
                           the underlying IFC node. */
    an_ifc_Node_type
                 *node_ptr;
                        /* When "storing_value" is FALSE this is the pointer
                           used to obtain the underlying IFC node. */
  };
};  /* an_ifc_Node */


template<typename an_ifc_Node_type>
inline const an_ifc_Node_type *an_ifc_Node<an_ifc_Node_type>::operator->()
                                                                          const
/*
This function provides access to the underlying IFC node's fields by returning
a pointer to the associated IFC node type (regardless of whether the field is
stored as part of this object or a pointer to a memory mapping).
*/
{
  const an_ifc_Node_type *result;

  if (storing_value) {
    /* The value is stored as part of this object, "node" is the correct
       resolution. */
    result = &node;
  } else {
    /* Check that this isn't an "uninitialized" forward declaration that's
       being accessed. */
    check_assertion(node_ptr != NULL);
    /* The value is not stored as part of this object, "node_ptr" is the
       correct resolution. */
    result = node_ptr;
  }  /* if */
  return result;
}  /* operator-> */
}  /* namespace */


template<typename an_ifc_Node_type>
static an_ifc_Node<an_ifc_Node_type> construct_node_from_module(
                                                            an_ifc_module *mod)
/*
Using the previously initialized and validated module source buffer, initialize
and return a new IFC node of the given type.

Direct use of this function is discouraged, prefer one of the other consturct_
functions that build upon this call.
*/
{
  an_ifc_Node<an_ifc_Node_type> result;
  an_ifc_Node_type              nts, *ntsp;

  ntsp = mod->get<an_ifc_Node_type>(&nts);
  /* When memory mapping is enabled and the endianness of the IFC and the host
     match, the front end can directly refer to portions of the IFC.
     Otherwise, the front end must fall back to copying the bytes locally with
     the correct endianness.

     If the front end is not using memory mapping, the front end must always
     create a copy of the bytes locally. */
#if USE_MMAP_FOR_MEMORY_REGIONS
  if (ntsp != &nts) {
    /* The storage wasn't used, use the pointer. */
    result = an_ifc_Node<an_ifc_Node_type>(ntsp);
  } else {
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
  {
    check_assertion(ntsp == &nts);
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
    /* The storage was used, copy it. */
    result = an_ifc_Node<an_ifc_Node_type>(nts);
  }
  return result;
}  /* construct_node_from_module */


template<typename an_ifc_Node_type>
static void construct_node(
                        Opt<an_ifc_Node<an_ifc_Node_type>> *result,
                        an_ifc_module                      *mod,
                        an_ifc_partition_position          pos)
/*
Perform a recursively validated read of the given module's IFC node of type
an_ifc_Node_type at the given partition position.  If successfully read the
given optional will be updated to contain the requested IFC node.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  if (mod->read_partition_element(pos)) {
    *result = construct_node_from_module<an_ifc_Node_type>(mod);
  }  /* if */
}  /* construct_node */


template<typename an_ifc_Node_type, typename... Args>
static void construct_node(Opt<an_ifc_Node<an_ifc_Node_type>> *result,
                           an_ifc_module                      *mod,
                           Args&&...                          args)
/*
Perform a recursively validated read of the given module's IFC node of type
an_ifc_Node_type at the partition position constructed from args.  If
successfully read the given optional will be updated to contain the requested
IFC node.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  an_ifc_partition_position pos(mod, args...);

  construct_node<an_ifc_Node_type>(result, mod, pos);
}  /* construct_node */


template<typename an_ifc_Node_type>
static void construct_node_shallow(
                        Opt<an_ifc_Node<an_ifc_Node_type>> *result,
                        an_ifc_module                      *mod,
                        an_ifc_partition_position          pos)
/*
Perform a non-recursively validated read of the given module's IFC node of type
an_ifc_Node_type at the given partition position.  If successfully read the
given optional will be updated to contain the requested IFC node.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  if (mod->read_partition_element_shallow(pos)) {
    *result = construct_node_from_module<an_ifc_Node_type>(mod);
  }  /* if */
}  /* construct_node_shallow */


template<typename an_ifc_Node_type, typename... Args>
static void construct_node_shallow(Opt<an_ifc_Node<an_ifc_Node_type>> *result,
                                   an_ifc_module                      *mod,
                                   Args&&...                          args)
/*
Perform a non-recursively validated read of the given module's IFC node of type
an_ifc_Node_type at the partition position constructed from args.  If
successfully read the given optional will be updated to contain the requested
IFC node.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  an_ifc_partition_position pos(mod, args...);

  return construct_node_shallow<an_ifc_Node_type>(mod, pos);
}  /* construct_node_shallow */


template<typename an_ifc_Node_type>
static void construct_node_prechecked(
                             an_ifc_Node<an_ifc_Node_type> *result,
                             an_ifc_module                 *mod,
                             an_ifc_partition_position     pos)
/*
With the knowledge that the given position has been previously validated by a
call to validate_partition_element (or indirectly via a call to
read_partition_element), perform a read of the given module's IFC node of type
an_ifc_Node_type at the given partition position.  The given result pointer
will be updated to contain the requested IFC node.
*/
{
  mod->read_prechecked_partition_element(pos);
  *result = construct_node_from_module<an_ifc_Node_type>(mod);
}  /* construct_node_prechecked */


template<typename an_ifc_Node_type, typename... Args>
static void construct_node_prechecked(an_ifc_Node<an_ifc_Node_type> *result,
                                      an_ifc_module                 *mod,
                                      Args&&...                     args)
/*
With the knowledge that the given position has been previously validated by a
call to validate_partition_element (or indirectly via a call to
read_partition_element), perform a read of the given module's IFC node of type
an_ifc_Node_type at the partition position constructed from args.  The given
result pointer will be updated to contain the requested IFC node.
*/
{
  an_ifc_partition_position pos(mod, args...);

  return construct_node_prechecked<an_ifc_Node_type>(result, mod, pos);
}  /* construct_node_prechecked */


template<typename an_ifc_Node_type>
static void construct_node_unchecked(
                             an_ifc_Node<an_ifc_Node_type> *result,
                             an_ifc_module                 *mod,
                             an_ifc_partition_position     pos)
/*
Perform a read of the given module's IFC node of type an_ifc_Node_type at the
given partition position, without validation or validation enforcement.  The
given result pointer will be updated to contain the requested IFC node.
*/
{
  mod->read_unchecked_partition_element(pos);
  *result = construct_node_from_module<an_ifc_Node_type>(mod);
}  /* construct_node_unchecked */


template<an_ifc_partition_kind a_Partition_Kind, typename a_Trait_T>
static void find_trait(Opt<an_ifc_Node<a_Trait_T>> *result,
                       an_ifc_module               *mod,
                       ifc_DeclIndex               decl)
/*
Given the module to search and an associated declaration index (decl) as a key
to the associated trait table identified by a_Partition_Kind, find and return
the associated trait as an optional.  If the returned optional is empty the
trait either wasn't found (because it doesn't exist), or a diagnosed validation
error occurred.
*/
{
  /* If this check fails, the validator needs additional validation to prevent
     a required IFC field from being 0 (i.e., "NULL"), or there's a logic
     bug. */
  check_assertion(decl != 0);
  {
    size_t    num_traits = mod->get_num_entries(a_Partition_Kind);
    /* Provide a value function for retrieving the trait at the given trait
       partition index. */
    auto      value_lambda = [mod](ptrdiff_t idx) {
      an_ifc_partition_position pos(mod, a_Partition_Kind,
                                    (ifc_Index_type)idx);
      an_ifc_Node<a_Trait_T>    trait;

      /* As the binary search used by this value is only doing comparisons (not
         attempting to operate on the returned DeclIndex) and the result will
         be fully validated anyways, to improve performance construct the node
         unchecked. */
      construct_node_unchecked(&trait, mod, pos);
      return trait->decl;
    };
    /* Get the partition index (if any) for decl. */
    ptrdiff_t partition_idx = bin_search(num_traits, decl, value_lambda);

    if (partition_idx != -1) {
      /* A trait was found for decl.  Load the trait (again) to retrieve the
         trait.

         Note that the implementation of bin_search at the time of writing does
         not guarantee that the last read value is the one whose index is
         returned.  Thus, we cannot (as an optimization) share a variable with
         the value_lambda to prevent double reading (though this is unlikely to
         ever represent a significant cost in terms of CPU time). */
      construct_node(result, mod, a_Partition_Kind, partition_idx);
    } /* if */
  }
}  /* find_trait */


static a_boolean is_class_scope(const an_ifc_Ref<ifc_DeclIndex> scope_ref)
/*
Return TRUE if the provided scope is a class/struct/union scope, FALSE
otherwise.
*/
{
  a_boolean result = FALSE;

  if (decl_tag(scope_ref.index) == ifc_DeclSort_Scope) {
    Opt<an_ifc_Node<an_ifc_DeclSort_Scope>> opt_idss;

    construct_node(&opt_idss, scope_ref.mod, scope_ref.index);
    if (opt_idss.has_value()) {
      an_ifc_Node<an_ifc_DeclSort_Scope> idss = *opt_idss;

      /* FIXME: We may want to diagnose the non-fundamental type here. */
      if (type_tag(idss->type) == ifc_TypeSort_Fundamental) {
        an_ifc_Node<an_ifc_TypeSort_Fundamental> itsf;

        construct_node_prechecked(&itsf, scope_ref.mod, idss->type);
        switch (itsf->basis) {
          case ifc_TypeBasis_Class:
          case ifc_TypeBasis_Struct:
          case ifc_TypeBasis_Union:
          case ifc_TypeBasis_Interface:
            result = TRUE;
            break;
          default:
            result = FALSE;
            break;
        }  /* switch */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_class_scope */


static void cache_name(a_token_cache_ptr         cache,
                       an_ifc_Ref<ifc_NameIndex> name_ref,
                       ifc_SourceLocation        *locus)
/*
Add the tokens corresponding to the given name to cache.  locus is the location
of the name.
*/
{
  /* Disable spurious GCC warning about uninitialized usage of opt_name_ref
     (when this function is called by cache_simple_template_id). */
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
  name_ref.mod->cache_name(cache, name_ref.index, locus);
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
}  /* cache_name */


/*
Utility to return a "tag" given a partition (an_ifc_partition_kind) value
and the starting partition for the particular case (e.g., ifc_type_start
for TypeSort).  Relies on an_ifc_partition_kind being ordered properly (see
the comments there).
*/
#define get_tag_from_partition(partition, start) ((partition) - (start))


inline void an_ifc_module::issue_unsupported_node_diag(a_const_char      *node,
                                                       a_source_position *pos)
/*
Issue a diagnostic that an unhandled node was encountered.  node is the textual
representation of the problematic node.  pos is the source position associated
with the diagnostic.
*/
{
  if (!unhandled_node_diag_issued) {
    pos_st_diagnostic(file_contains_unhandled_nodes_sev,
                      ec_module_file_contains_unsupported_constructs,
                      &null_source_position, assoc_module_info->name);
    unhandled_node_diag_issued = TRUE;
  }  /* if */
  pos_st_diagnostic(unhandled_ifc_node_severity, ec_unhandled_ifc_construct,
                    pos, node);
}  /* issue_unsupported_node_diag */


static a_const_char *get_string_at_offset(an_ifc_Ref<ifc_TextOffset> offset)
/*
Return a pointer to the IFC string table for a given TextOffset.  Strings in
the IFC file are NULL-terminated.
*/
{
  return offset.mod->get_string_at_offset(offset.index);
}  /* get_string_at_offset */


inline a_const_char *an_ifc_module::get_string_at_offset(ifc_TextOffset offset)
                                                                          const
/*
Return a pointer to the IFC string table for a given TextOffset.  Strings in
the IFC file are NULL-terminated.
*/
{
#if EXPENSIVE_CHECKING
  check_assertion((ifc_Cardinality)(offset) < header.string_table_size);
#endif /* EXPENSIVE_CHECKING */
  return string_table + offset;
}  /* get_string_at_offset */


static a_const_char* make_ifc_temporary_unique_id(ifc_UniqueID id)
/*
Given a unique ID associated with a temporary, return a string that is the
identifier for that temporary.  The string must be used before any further
changes to temp_text_buffer occur, otherwise the result may be wiped out.
Note: This routine should not interfere with any string that's in the process
of being constructed in temp_text_buffer.
*/
{
  Value_saver<sizeof_t> pos_saver(&pos_in_temp_text_buffer);
  uint32_t              raw_id = id;
  a_const_char          *result;

  /* Index to where the string will be added to the text buffer. */
  result = temp_text_buffer + pos_in_temp_text_buffer;
  put_str_to_temp_text_buffer("__ifc_temp_");
  /* Similar to itoa, except puts characters directly to the temp text buffer,
     which will ensure enough space exists.  This will actually put out the
     value in reverse - which is incorrect for a true itoa, but sufficient for
     the purposes of creating a unique ID. */
  while (raw_id >= 10) {
    put_ch_to_temp_text_buffer((raw_id % 10) + '0');
    raw_id /= 10;
  }  /* while */
  check_assertion(raw_id < 10);
  put_ch_to_temp_text_buffer(raw_id + '0');
  put_ch_to_temp_text_buffer('\0');
  return result;
}  /* make_ifc_temporary_unique_id */


static a_const_char *str_for_decl_tag(ifc_DeclSort tag)
/*
Return a string with the name that corresponds to the DeclSort tag.
*/
{
  a_const_char *result = "Unexpected DeclSort";

  switch (tag) {
    case ifc_DeclSort_VendorExtension:  result = "VendorExtension"; break;
    case ifc_DeclSort_Enumerator:       result = "Enumerator"; break;
    case ifc_DeclSort_Variable:         result = "Variable"; break;
    case ifc_DeclSort_Parameter:        result = "Parameter"; break;
    case ifc_DeclSort_Field:            result = "Field"; break;
    case ifc_DeclSort_Bitfield:         result = "Bitfield"; break;
    case ifc_DeclSort_Scope:            result = "Scope"; break;
    case ifc_DeclSort_Enumeration:      result = "Enumeration"; break;
    case ifc_DeclSort_Alias:            result = "Alias"; break;
    case ifc_DeclSort_Temploid:         result = "Temploid"; break;
    case ifc_DeclSort_Template:         result = "Template"; break;
    case ifc_DeclSort_PartialSpecialization:
                                       result = "PartialSpecialization"; break;
    case ifc_DeclSort_Specialization:   result = "Specialization";break;
    case ifc_DeclSort_Concept:          result = "Concept"; break;
    case ifc_DeclSort_Function:         result = "Function"; break;
    case ifc_DeclSort_Method:           result = "Method"; break;
    case ifc_DeclSort_Constructor:      result = "Constructor"; break;
    case ifc_DeclSort_InheritedConstructor:
                                        result = "InheritedConstructor"; break;
    case ifc_DeclSort_Destructor:       result = "Destructor"; break;
    case ifc_DeclSort_Reference:        result = "Reference"; break;
    case ifc_DeclSort_UsingDeclaration: result = "UsingDeclaration"; break;
    case ifc_DeclSort_UsingDirective:   result = "UsingDirective"; break;
    case ifc_DeclSort_Friend:           result = "Friend"; break;
    case ifc_DeclSort_Expansion:        result = "Expansion"; break;
    case ifc_DeclSort_DeductionGuide:   result = "DeductionGuide"; break;
    case ifc_DeclSort_Barren:           result = "Barren"; break;
    case ifc_DeclSort_Tuple:            result = "Tuple"; break;
    case ifc_DeclSort_SyntaxTree:       result = "SyntaxTree"; break;
    case ifc_DeclSort_Intrinsic:        result = "Intrinsic"; break;
    case ifc_DeclSort_Property:         result = "Property"; break;
    case ifc_DeclSort_OutputSegment:    result = "OutputSegment"; break;
    /* Unused DeclSorts */
    case ifc_DeclSort_UnusedSort0:
    case ifc_DeclSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* str_for_decl_tag */


static a_const_char *str_for_ifc_operator(ifc_NiladicOperator niladic_op)
/*
Return a stringized version of the given operator.
*/
{
  a_const_char *op_str = "Unexpected NiladicOperator";

#define niladic_op_str(str) "NiladicOperator::" str
  switch (niladic_op) {
    case ifc_NiladicOperator_Unknown:
      op_str = niladic_op_str("Unknown"); break;
    case ifc_NiladicOperator_Msvc:
      op_str = niladic_op_str("Msvc"); break;
    case ifc_NiladicOperator_Phantom:
      op_str = niladic_op_str("Phantom"); break;
    case ifc_NiladicOperator_Constant:
      op_str = niladic_op_str("Constant"); break;
    case ifc_NiladicOperator_Nil:
      op_str = niladic_op_str("Nil"); break;
    case ifc_NiladicOperator_MsvcConstantObject:
      op_str = niladic_op_str("MsvcConstantObject"); break;
    case ifc_NiladicOperator_MsvcLambda:
      op_str = niladic_op_str("MsvcLambda"); break;
    default_is_unexpected_str("Unexpected NiladicOperator");
  }  /* switch */
#undef niladic_op_str
  return op_str;
}  /* str_for_ifc_operator */


static a_const_char *str_for_ifc_operator(ifc_MonadicOperator monadic_op)
/*
Return a stringized version of the given operator.
*/
{
  a_const_char *op_str = "Unexpected MonadicOperator";

#define monadic_op_str(str) "MonadicOperator::" str
  switch (monadic_op) {
    case ifc_MonadicOperator_Unknown:
      op_str = monadic_op_str("Unknown"); break;
    case ifc_MonadicOperator_Msvc:
      op_str = monadic_op_str("Msvc"); break;
    case ifc_MonadicOperator_MsvcConfusion:
      op_str = monadic_op_str("MsvcConfusion"); break;
    case ifc_MonadicOperator_Plus:
      op_str = monadic_op_str("Plus"); break;
    case ifc_MonadicOperator_Negate:
      op_str = monadic_op_str("Negate"); break;
    case ifc_MonadicOperator_Deref:
      op_str = monadic_op_str("Deref"); break;
    case ifc_MonadicOperator_Address:
      op_str = monadic_op_str("Address"); break;
    case ifc_MonadicOperator_Complement:
      op_str = monadic_op_str("Complement"); break;
    case ifc_MonadicOperator_Not:
      op_str = monadic_op_str("Not"); break;
    case ifc_MonadicOperator_PreIncrement:
      op_str = monadic_op_str("PreIncrement"); break;
    case ifc_MonadicOperator_PreDecrement:
      op_str = monadic_op_str("PreDecrement"); break;
    case ifc_MonadicOperator_PostIncrement:
      op_str = monadic_op_str("PostIncrement"); break;
    case ifc_MonadicOperator_PostDecrement:
      op_str = monadic_op_str("PostDecrement"); break;
    case ifc_MonadicOperator_Await:
      op_str = monadic_op_str("Await"); break;
    case ifc_MonadicOperator_New:
      op_str = monadic_op_str("New"); break;
    case ifc_MonadicOperator_Delete:
      op_str = monadic_op_str("Delete"); break;
    case ifc_MonadicOperator_DeleteArray:
      op_str = monadic_op_str("DeleteArray"); break;
    case ifc_MonadicOperator_Truncate:
      op_str = monadic_op_str("Truncate"); break;
    case ifc_MonadicOperator_Ceil:
      op_str = monadic_op_str("Ceil"); break;
    case ifc_MonadicOperator_Floor:
      op_str = monadic_op_str("Floor"); break;
    case ifc_MonadicOperator_Paren:
      op_str = monadic_op_str("Paren"); break;
    case ifc_MonadicOperator_Brace:
      op_str = monadic_op_str("Brace"); break;
    case ifc_MonadicOperator_Alignas:
      op_str = monadic_op_str("Alignas"); break;
    case ifc_MonadicOperator_Alignof:
      op_str = monadic_op_str("Alignof"); break;
    case ifc_MonadicOperator_Sizeof:
      op_str = monadic_op_str("Sizeof"); break;
    case ifc_MonadicOperator_Cardinality:
      op_str = monadic_op_str("Cardinality"); break;
    case ifc_MonadicOperator_Typeid:
      op_str = monadic_op_str("Typeid"); break;
    case ifc_MonadicOperator_Noexcept:
      op_str = monadic_op_str("Noexcept"); break;
    case ifc_MonadicOperator_Requires:
      op_str = monadic_op_str("Requires"); break;
    case ifc_MonadicOperator_CoReturn:
      op_str = monadic_op_str("CoReturn"); break;
    case ifc_MonadicOperator_Yield:
      op_str = monadic_op_str("Yield"); break;
    case ifc_MonadicOperator_Throw:
      op_str = monadic_op_str("Throw"); break;
    case ifc_MonadicOperator_Expand:
      op_str = monadic_op_str("Expand"); break;
    case ifc_MonadicOperator_Read:
      op_str = monadic_op_str("Read"); break;
    case ifc_MonadicOperator_Materialize:
      op_str = monadic_op_str("Materialize"); break;
    case ifc_MonadicOperator_PseudoDtorCall:
      op_str = monadic_op_str("PseudoDtorCall"); break;
    case ifc_MonadicOperator_LookupGlobally:
      op_str = monadic_op_str("LookupGlobally"); break;
    case ifc_MonadicOperator_MsvcAssume:
      op_str = monadic_op_str("MsvcAssume"); break;
    case ifc_MonadicOperator_MsvcAlignof:
      op_str = monadic_op_str("MsvcAlignof"); break;
    case ifc_MonadicOperator_MsvcUuidof:
      op_str = monadic_op_str("MsvcUuidof"); break;
    case ifc_MonadicOperator_MsvcIsClass:
      op_str = monadic_op_str("MsvcIsClass"); break;
    case ifc_MonadicOperator_MsvcIsUnion:
      op_str = monadic_op_str("MsvcIsUnion"); break;
    case ifc_MonadicOperator_MsvcIsEnum:
      op_str = monadic_op_str("MsvcIsEnum"); break;
    case ifc_MonadicOperator_MsvcIsPolymorphic:
      op_str = monadic_op_str("MsvcIsPolymorphic"); break;
    case ifc_MonadicOperator_MsvcIsEmpty:
      op_str = monadic_op_str("MsvcIsEmpty"); break;
    case ifc_MonadicOperator_MsvcIsTriviallyCopyConstructible:
      op_str = monadic_op_str("MsvcIsTriviallyCopyConstructible"); break;
    case ifc_MonadicOperator_MsvcIsTriviallyCopyAssignable:
      op_str = monadic_op_str("MsvcIsTriviallyCopyAssignable"); break;
    case ifc_MonadicOperator_MsvcIsTriviallyDestructible:
      op_str = monadic_op_str("MsvcIsTriviallyDestructible"); break;
    case ifc_MonadicOperator_MsvcHasVirtualDestructor:
      op_str = monadic_op_str("MsvcHasVirtualDestructor"); break;
    case ifc_MonadicOperator_MsvcIsNothrowCopyConstructible:
      op_str = monadic_op_str("MsvcIsNothrowCopyConstructible"); break;
    case ifc_MonadicOperator_MsvcIsNothrowCopyAssignable:
      op_str = monadic_op_str("MsvcIsNothrowCopyAssignable"); break;
    case ifc_MonadicOperator_MsvcIsPod:
      op_str = monadic_op_str("MsvcIsPod"); break;
    case ifc_MonadicOperator_MsvcIsAbstract:
      op_str = monadic_op_str("MsvcIsAbstract"); break;
    case ifc_MonadicOperator_MsvcIsTrivial:
      op_str = monadic_op_str("MsvcIsTrivial"); break;
    case ifc_MonadicOperator_MsvcIsTriviallyCopyable:
      op_str = monadic_op_str("MsvcIsTriviallyCopyable"); break;
    case ifc_MonadicOperator_MsvcIsStandardLayout:
      op_str = monadic_op_str("MsvcIsStandardLayout"); break;
    case ifc_MonadicOperator_MsvcIsLiteralType:
      op_str = monadic_op_str("MsvcIsLiteralType"); break;
    case ifc_MonadicOperator_MsvcIsTriviallyMoveConstructible:
      op_str = monadic_op_str("MsvcIsTriviallyMoveConstructible"); break;
    case ifc_MonadicOperator_MsvcHasTrivialMoveAssign:
      op_str = monadic_op_str("MsvcHasTrivialMoveAssign"); break;
    case ifc_MonadicOperator_MsvcIsTriviallyMoveAssignable:
      op_str = monadic_op_str("MsvcIsTriviallyMoveAssignable"); break;
    case ifc_MonadicOperator_MsvcIsNothrowMoveAssignable:
      op_str = monadic_op_str("MsvcIsNothrowMoveAssignable"); break;
    case ifc_MonadicOperator_MsvcUnderlyingType:
      op_str = monadic_op_str("MsvcUnderlyingType"); break;
    case ifc_MonadicOperator_MsvcIsDestructible:
      op_str = monadic_op_str("MsvcIsDestructible"); break;
    case ifc_MonadicOperator_MsvcIsNothrowDestructible:
      op_str = monadic_op_str("MsvcIsNothrowDestructible"); break;
    case ifc_MonadicOperator_MsvcHasUniqueObjectRepresentations:
      op_str = monadic_op_str("MsvcHasUniqueObjectRepresentations"); break;
    case ifc_MonadicOperator_MsvcIsAggregate:
      op_str = monadic_op_str("MsvcIsAggregate"); break;
    case ifc_MonadicOperator_MsvcBuiltinAddressOf:
      op_str = monadic_op_str("MsvcBuiltinAddressOf"); break;
    case ifc_MonadicOperator_MsvcIsRefClass:
      op_str = monadic_op_str("MsvcIsRefClass"); break;
    case ifc_MonadicOperator_MsvcIsValueClass:
      op_str = monadic_op_str("MsvcIsValueClass"); break;
    case ifc_MonadicOperator_MsvcIsSimpleValueClass:
      op_str = monadic_op_str("MsvcIsSimpleValueClass"); break;
    case ifc_MonadicOperator_MsvcIsInterfaceClass:
      op_str = monadic_op_str("MsvcIsInterfaceClass"); break;
    case ifc_MonadicOperator_MsvcIsDelegate:
      op_str = monadic_op_str("MsvcIsDelegate"); break;
    case ifc_MonadicOperator_MsvcIsFinal:
      op_str = monadic_op_str("MsvcIsFinal"); break;
    case ifc_MonadicOperator_MsvcIsSealed:
      op_str = monadic_op_str("MsvcIsSealed"); break;
    case ifc_MonadicOperator_MsvcHasFinalizer:
      op_str = monadic_op_str("MsvcHasFinalizer"); break;
    case ifc_MonadicOperator_MsvcHasCopy:
      op_str = monadic_op_str("MsvcHasCopy"); break;
    case ifc_MonadicOperator_MsvcHasAssign:
      op_str = monadic_op_str("MsvcHasAssign"); break;
    case ifc_MonadicOperator_MsvcHasUserDestructor:
      op_str = monadic_op_str("MsvcHasUserDestructor"); break;
    case ifc_MonadicOperator_MsvcConfusedExpand:
      op_str = monadic_op_str("MsvcConfusedExpand"); break;
    case ifc_MonadicOperator_MsvcConfusedDependentSizeof:
      op_str = monadic_op_str("MsvcConfusedDependentSizeof"); break;
    default_is_unexpected_str("Unexpected MonadicOperator");
  }  /* switch */
  return op_str;
#undef monadic_op_str
}  /* str_for_ifc_operator */


static a_const_char *str_for_ifc_operator(ifc_DyadicOperator dyadic_op)
/*
Return a stringized version of the given operator.
*/
{
  a_const_char *op_str = "Unexpected DyadicOperator";

#define dyadic_op_str(str) "DyadicOperator::" str
  switch (dyadic_op) {
    case ifc_DyadicOperator_Unknown:
      op_str = dyadic_op_str("Unknown"); break;
    case ifc_DyadicOperator_Msvc:
      op_str = dyadic_op_str("Msvc"); break;
    case ifc_DyadicOperator_Plus:
      op_str = dyadic_op_str("Plus"); break;
    case ifc_DyadicOperator_Minus:
      op_str = dyadic_op_str("Minus"); break;
    case ifc_DyadicOperator_Mult:
      op_str = dyadic_op_str("Mult"); break;
    case ifc_DyadicOperator_Slash:
      op_str = dyadic_op_str("Slash"); break;
    case ifc_DyadicOperator_Modulo:
      op_str = dyadic_op_str("Module"); break;
    case ifc_DyadicOperator_Remainder:
      op_str = dyadic_op_str("Remainder"); break;
    case ifc_DyadicOperator_Bitand:
      op_str = dyadic_op_str("Bitand"); break;
    case ifc_DyadicOperator_Bitor:
      op_str = dyadic_op_str("Bitor"); break;
    case ifc_DyadicOperator_Bitxor:
      op_str = dyadic_op_str("Bitxor"); break;
    case ifc_DyadicOperator_Lshift:
      op_str = dyadic_op_str("Lshift"); break;
    case ifc_DyadicOperator_Rshift:
      op_str = dyadic_op_str("Rshift"); break;
    case ifc_DyadicOperator_Equal:
      op_str = dyadic_op_str("Equal"); break;
    case ifc_DyadicOperator_NotEqual:
      op_str = dyadic_op_str("NotEqual"); break;
    case ifc_DyadicOperator_Less:
      op_str = dyadic_op_str("Less"); break;
    case ifc_DyadicOperator_LessEqual:
      op_str = dyadic_op_str("LessEqual"); break;
    case ifc_DyadicOperator_Greater:
      op_str = dyadic_op_str("Greater"); break;
    case ifc_DyadicOperator_GreaterEqual:
      op_str = dyadic_op_str("GreaterEqual"); break;
    case ifc_DyadicOperator_LogicAnd:
      op_str = dyadic_op_str("LogicAnd"); break;
    case ifc_DyadicOperator_LogicOr:
      op_str = dyadic_op_str("LogicOr"); break;
    case ifc_DyadicOperator_Assign:
      op_str = dyadic_op_str("Assign"); break;
    case ifc_DyadicOperator_PlusAssign:
      op_str = dyadic_op_str("PlusAssign"); break;
    case ifc_DyadicOperator_MinusAssign:
      op_str = dyadic_op_str("MinusAssign"); break;
    case ifc_DyadicOperator_MultAssign:
      op_str = dyadic_op_str("MultAssign"); break;
    case ifc_DyadicOperator_SlashAssign:
      op_str = dyadic_op_str("SlashAssign"); break;
    case ifc_DyadicOperator_ModuloAssign:
      op_str = dyadic_op_str("ModuloAssign"); break;
    case ifc_DyadicOperator_BitandAssign:
      op_str = dyadic_op_str("BitandAssign"); break;
    case ifc_DyadicOperator_BitorAssign:
      op_str = dyadic_op_str("BitorAssign"); break;
    case ifc_DyadicOperator_BitxorAssign:
      op_str = dyadic_op_str("BitxorAssign"); break;
    case ifc_DyadicOperator_LshiftAssign:
      op_str = dyadic_op_str("LshiftAssign"); break;
    case ifc_DyadicOperator_RshiftAssign:
      op_str = dyadic_op_str("RshiftAssign"); break;
    case ifc_DyadicOperator_Comma:
      op_str = dyadic_op_str("Comma"); break;
    case ifc_DyadicOperator_Arrow:
      op_str = dyadic_op_str("Arrow"); break;
    case ifc_DyadicOperator_ArrowStar:
      op_str = dyadic_op_str("ArrowStar"); break;
    case ifc_DyadicOperator_New:
      op_str = dyadic_op_str("New"); break;
    case ifc_DyadicOperator_NewArray:
      op_str = dyadic_op_str("NewArray"); break;
    case ifc_DyadicOperator_Compare:
      op_str = dyadic_op_str("Compare"); break;
    case ifc_DyadicOperator_Dot:
      op_str = dyadic_op_str("Dot"); break;
    case ifc_DyadicOperator_DotStar:
      op_str = dyadic_op_str("DotStar"); break;
    case ifc_DyadicOperator_Curry:
      op_str = dyadic_op_str("Curry"); break;
    case ifc_DyadicOperator_Apply:
      op_str = dyadic_op_str("Apply"); break;
    case ifc_DyadicOperator_Index:
      op_str = dyadic_op_str("Index"); break;
    case ifc_DyadicOperator_DefaultAt:
      op_str = dyadic_op_str("DefaultAt"); break;
    case ifc_DyadicOperator_Destruct:
      op_str = dyadic_op_str("Destruct"); break;
    case ifc_DyadicOperator_DestructAt:
      op_str = dyadic_op_str("DestructAt"); break;
    case ifc_DyadicOperator_Cleanup:
      op_str = dyadic_op_str("Cleanup"); break;
    case ifc_DyadicOperator_Qualification:
      op_str = dyadic_op_str("Qualification"); break;
    case ifc_DyadicOperator_Promote:
      op_str = dyadic_op_str("Promote"); break;
    case ifc_DyadicOperator_Demote:
      op_str = dyadic_op_str("Demote"); break;
    case ifc_DyadicOperator_Coerce:
      op_str = dyadic_op_str("Coerce"); break;
    case ifc_DyadicOperator_Rewrite:
      op_str = dyadic_op_str("Rewrite"); break;
    case ifc_DyadicOperator_Bless:
      op_str = dyadic_op_str("Bless"); break;
    case ifc_DyadicOperator_Cast:
      op_str = dyadic_op_str("Cast"); break;
    case ifc_DyadicOperator_ExplicitConversion:
      op_str = dyadic_op_str("ExplicitConversion"); break;
    case ifc_DyadicOperator_ReinterpretCast:
      op_str = dyadic_op_str("ReinterpretCast"); break;
    case ifc_DyadicOperator_StaticCast:
      op_str = dyadic_op_str("StaticCast"); break;
    case ifc_DyadicOperator_ConstCast:
      op_str = dyadic_op_str("ConstCast"); break;
    case ifc_DyadicOperator_DynamicCast:
      op_str = dyadic_op_str("DynamicCast"); break;
    case ifc_DyadicOperator_Narrow:
      op_str = dyadic_op_str("Narrow"); break;
    case ifc_DyadicOperator_Widen:
      op_str = dyadic_op_str("Widen"); break;
    case ifc_DyadicOperator_Pretend:
      op_str = dyadic_op_str("Pretend"); break;
    case ifc_DyadicOperator_Closure:
      op_str = dyadic_op_str("Closure"); break;
    case ifc_DyadicOperator_ZeroInitialize:
      op_str = dyadic_op_str("ZeroInitialize"); break;
    case ifc_DyadicOperator_ClearStorage:
      op_str = dyadic_op_str("ClearStorage"); break;
    case ifc_DyadicOperator_MsvcTryCast:
      op_str = dyadic_op_str("MsvcTryCast"); break;
    case ifc_DyadicOperator_MsvcCurry:
      op_str = dyadic_op_str("MsvcCurry"); break;
    case ifc_DyadicOperator_MsvcVirtualCurry:
      op_str = dyadic_op_str("MsvcVirtualCurry"); break;
    case ifc_DyadicOperator_MsvcAlign:
      op_str = dyadic_op_str("MsvcAlign"); break;
    case ifc_DyadicOperator_MsvcBitSpan:
      op_str = dyadic_op_str("MsvcBitSpan"); break;
    case ifc_DyadicOperator_MsvcBitfieldAccess:
      op_str = dyadic_op_str("MsvcBitfieldAccess"); break;
    case ifc_DyadicOperator_MsvcObscureBitfieldAccess:
      op_str = dyadic_op_str("MsvcObscureBitfieldAccess"); break;
    case ifc_DyadicOperator_MsvcInitialize:
      op_str = dyadic_op_str("MsvcInitialize"); break;
    case ifc_DyadicOperator_MsvcBuiltinOffsetOf:
      op_str = dyadic_op_str("MsvcBuiltinOffsetOf"); break;
    case ifc_DyadicOperator_MsvcIsBaseOf:
      op_str = dyadic_op_str("MsvcIsBaseOf"); break;
    case ifc_DyadicOperator_MsvcIsConvertibleTo:
      op_str = dyadic_op_str("MsvcIsConvertibleTo"); break;
    case ifc_DyadicOperator_MsvcIsTriviallyAssignable:
      op_str = dyadic_op_str("MsvcIsTriviallyAssignable"); break;
    case ifc_DyadicOperator_MsvcIsNothrowAssignable:
      op_str = dyadic_op_str("MsvcIsNothrowAssignable"); break;
    case ifc_DyadicOperator_MsvcIsAssignable:
      op_str = dyadic_op_str("MsvcIsAssignable"); break;
    case ifc_DyadicOperator_MsvcIsAssignableNocheck:
      op_str = dyadic_op_str("MsvcIsAssignableNocheck"); break;
    case ifc_DyadicOperator_MsvcBuiltinBitCast:
      op_str = dyadic_op_str("MsvcBuiltinBitCast"); break;
    case ifc_DyadicOperator_MsvcBuiltinIsLayoutCompatible:
      op_str = dyadic_op_str("MsvcBuiltinIsLayoutCompatible"); break;
    case ifc_DyadicOperator_MsvcBuiltinIsPointerInterconvertibleBaseOf:
      op_str = dyadic_op_str("MsvcBuiltinIsPointerInterconvertibleBaseOf");
      break;
    case ifc_DyadicOperator_MsvcBuiltinIsPointerInterconvertibleWithClass:
      op_str = dyadic_op_str("MsvcBuiltinIsPointerInterconvertibleWithClass");
      break;
    case ifc_DyadicOperator_MsvcBuiltinIsCorrespondingMember:
      op_str = dyadic_op_str("MsvcBuiltinIsCorrespondingMember"); break;
    case ifc_DyadicOperator_MsvcIntrinsic:
      op_str = dyadic_op_str("MsvcIntrinsic"); break;
    default_is_unexpected_str("Unexpected DyadicOperator");
  }  /* switch */
#undef dyadic_op_str
  return op_str;
}  /* str_for_ifc_operator */


static a_const_char *str_for_ifc_operator(ifc_TriadicOperator triadic_op)
/*
Return a stringized version of the given operator.
*/
{
  a_const_char *op_str = "Unexpected TriadicOperator";

#define triadic_op_str(str) "TriadicOperator::" str
  switch (triadic_op) {
    case ifc_TriadicOperator_Unknown:
      op_str = triadic_op_str("Unknown"); break;
    case ifc_TriadicOperator_Msvc:
      op_str = triadic_op_str("Msvc"); break;
    case ifc_TriadicOperator_Choice:
      op_str = triadic_op_str("Choice"); break;
    case ifc_TriadicOperator_ConstructAt:
      op_str = triadic_op_str("ConstructAt"); break;
    case ifc_TriadicOperator_Initialize:
      op_str = triadic_op_str("Initialize"); break;
    default_is_unexpected_str("Unexpected TriadicOperator");
  }  /* switch */
#undef triadic_op_str
  return op_str;
}  /* str_for_ifc_operator */


static a_const_char *str_for_ifc_operator(ifc_StorageOperator storage_op)
/*
Return a stringized version of the given operator.
*/
{
  a_const_char *op_str = "Unexpected StorageOperator";

#define storage_op_str(str) "StorageOperator::" str
  switch (storage_op) {
    case ifc_StorageOperator_Unknown:
      op_str = storage_op_str("Unknown"); break;
    case ifc_StorageOperator_Msvc:
      op_str = storage_op_str("Msvc"); break;
    case ifc_StorageOperator_AllocateSingle:
      op_str = storage_op_str("AllocateSingle"); break;
    case ifc_StorageOperator_AllocateArray:
      op_str = storage_op_str("AllocateArray"); break;
    case ifc_StorageOperator_DeallocateSingle:
      op_str = storage_op_str("DeallocateSingle"); break;
    case ifc_StorageOperator_DeallocateArray:
      op_str = storage_op_str("DeallocateArray"); break;
    default_is_unexpected_str("Unexpected StorageOperator");
  }  /* switch */
#undef storage_op_str
  return op_str;
}  /* str_for_ifc_operator */


static a_const_char *str_for_ifc_operator(ifc_VariadicOperator variadic_op)
/*
Return a stringized version of the given operator.
*/
{
  a_const_char *op_str = "Unexpected VariadicOperator";

#define variadic_op_str(str) "VariadicOperator::" str
  switch (variadic_op) {
    case ifc_VariadicOperator_Unknown:
      op_str = variadic_op_str("Unknown"); break;
    case ifc_VariadicOperator_Msvc:
      op_str = variadic_op_str("Msvc"); break;
    case ifc_VariadicOperator_Collection:
      op_str = variadic_op_str("Collection"); break;
    case ifc_VariadicOperator_Sequence:
      op_str = variadic_op_str("Sequence"); break;
    case ifc_VariadicOperator_MsvcHasTrivialConstructor:
      op_str = variadic_op_str("MsvcHasTrivialConstructor"); break;
    case ifc_VariadicOperator_MsvcIsConstructible:
      op_str = variadic_op_str("MsvcIsConstructible"); break;
    case ifc_VariadicOperator_MsvcIsNothrowConstructible:
      op_str = variadic_op_str("MsvcIsNothrowConstructible"); break;
    case ifc_VariadicOperator_MsvcIsTriviallyConstructible:
      op_str = variadic_op_str("MsvcIsTriviallyConstructible"); break;
    default_is_unexpected_str("Unexpected VariadicOperator");
  }  /* switch */
#undef variadic_op_str
  return op_str;
}  /* str_for_ifc_operator */


static an_opname_kind opname_from_niladic_op(ifc_NiladicOperator niladic_op)
/*
Map an IFC NiladicOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (niladic_op) {
    case ifc_NiladicOperator_Unknown:
    case ifc_NiladicOperator_Msvc:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for_ifc_operator(niladic_op));
      op = onk_none;
      break;
    case ifc_NiladicOperator_Phantom:
    case ifc_NiladicOperator_Constant:
    case ifc_NiladicOperator_Nil:
    case ifc_NiladicOperator_MsvcConstantObject:
    case ifc_NiladicOperator_MsvcLambda:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for_ifc_operator(niladic_op));
      op = onk_none;
      break;
    default_is_unexpected_str("Unexpected NiladicOperator");
  }  /* switch */
  return op;
}  /* opname_from_niladic_op */


static an_opname_kind opname_from_monadic_op(ifc_MonadicOperator monadic_op)
/*
Map an IFC MonadicOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (monadic_op) {
    case ifc_MonadicOperator_Unknown:
    case ifc_MonadicOperator_Msvc:
    case ifc_MonadicOperator_MsvcConfusion:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for_ifc_operator(monadic_op));
      op = onk_none;
      break;
    case ifc_MonadicOperator_Plus:               op = onk_plus;          break;
    case ifc_MonadicOperator_Negate:             op = onk_minus;         break;
    case ifc_MonadicOperator_Deref:              op = onk_star;          break;
    case ifc_MonadicOperator_Address:            op = onk_ampersand;     break;
    case ifc_MonadicOperator_Complement:         op = onk_compl;         break;
    case ifc_MonadicOperator_Not:                op = onk_not;           break;
    case ifc_MonadicOperator_PreIncrement:       op = onk_plus_plus;     break;
    case ifc_MonadicOperator_PreDecrement:       op = onk_minus_minus;   break;
    case ifc_MonadicOperator_PostIncrement:      op = onk_plus_plus;     break;
    case ifc_MonadicOperator_PostDecrement:      op = onk_minus_minus;   break;
    case ifc_MonadicOperator_Await:              op = onk_await;         break;
    case ifc_MonadicOperator_New:                op = onk_new;           break;
    case ifc_MonadicOperator_Delete:             op = onk_delete;        break;
    case ifc_MonadicOperator_DeleteArray:        op = onk_array_delete;  break;
    case ifc_MonadicOperator_Truncate:
    case ifc_MonadicOperator_Ceil:
    case ifc_MonadicOperator_Floor:
    case ifc_MonadicOperator_Paren:
    case ifc_MonadicOperator_Brace:
    case ifc_MonadicOperator_Alignas:
    case ifc_MonadicOperator_Alignof:
    case ifc_MonadicOperator_Sizeof:
    case ifc_MonadicOperator_Cardinality:
    case ifc_MonadicOperator_Typeid:
    case ifc_MonadicOperator_Noexcept:
    case ifc_MonadicOperator_Requires:
    case ifc_MonadicOperator_CoReturn:
    case ifc_MonadicOperator_Yield:
    case ifc_MonadicOperator_Throw:
    case ifc_MonadicOperator_Expand:
    case ifc_MonadicOperator_Read:
    case ifc_MonadicOperator_Materialize:
    case ifc_MonadicOperator_PseudoDtorCall:
    case ifc_MonadicOperator_LookupGlobally:
    case ifc_MonadicOperator_MsvcAssume:
    case ifc_MonadicOperator_MsvcAlignof:
    case ifc_MonadicOperator_MsvcUuidof:
    case ifc_MonadicOperator_MsvcIsClass:
    case ifc_MonadicOperator_MsvcIsUnion:
    case ifc_MonadicOperator_MsvcIsEnum:
    case ifc_MonadicOperator_MsvcIsPolymorphic:
    case ifc_MonadicOperator_MsvcIsEmpty:
    case ifc_MonadicOperator_MsvcIsTriviallyCopyConstructible:
    case ifc_MonadicOperator_MsvcIsTriviallyCopyAssignable:
    case ifc_MonadicOperator_MsvcIsTriviallyDestructible:
    case ifc_MonadicOperator_MsvcHasVirtualDestructor:
    case ifc_MonadicOperator_MsvcIsNothrowCopyConstructible:
    case ifc_MonadicOperator_MsvcIsNothrowCopyAssignable:
    case ifc_MonadicOperator_MsvcIsPod:
    case ifc_MonadicOperator_MsvcIsAbstract:
    case ifc_MonadicOperator_MsvcIsTrivial:
    case ifc_MonadicOperator_MsvcIsTriviallyCopyable:
    case ifc_MonadicOperator_MsvcIsStandardLayout:
    case ifc_MonadicOperator_MsvcIsLiteralType:
    case ifc_MonadicOperator_MsvcIsTriviallyMoveConstructible:
    case ifc_MonadicOperator_MsvcHasTrivialMoveAssign:
    case ifc_MonadicOperator_MsvcIsTriviallyMoveAssignable:
    case ifc_MonadicOperator_MsvcIsNothrowMoveAssignable:
    case ifc_MonadicOperator_MsvcUnderlyingType:
    case ifc_MonadicOperator_MsvcIsDestructible:
    case ifc_MonadicOperator_MsvcIsNothrowDestructible:
    case ifc_MonadicOperator_MsvcHasUniqueObjectRepresentations:
    case ifc_MonadicOperator_MsvcIsAggregate:
    case ifc_MonadicOperator_MsvcBuiltinAddressOf:
    case ifc_MonadicOperator_MsvcIsRefClass:
    case ifc_MonadicOperator_MsvcIsValueClass:
    case ifc_MonadicOperator_MsvcIsSimpleValueClass:
    case ifc_MonadicOperator_MsvcIsInterfaceClass:
    case ifc_MonadicOperator_MsvcIsDelegate:
    case ifc_MonadicOperator_MsvcIsFinal:
    case ifc_MonadicOperator_MsvcIsSealed:
    case ifc_MonadicOperator_MsvcHasFinalizer:
    case ifc_MonadicOperator_MsvcHasCopy:
    case ifc_MonadicOperator_MsvcHasAssign:
    case ifc_MonadicOperator_MsvcHasUserDestructor:
    case ifc_MonadicOperator_MsvcConfusedExpand:
    case ifc_MonadicOperator_MsvcConfusedDependentSizeof:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for_ifc_operator(monadic_op));
      op = onk_none;
      break;
    default_is_unexpected_str("Unexpected MonadicOperator");
  }  /* switch */
  return op;
}  /* opname_from_monadic_op */


static an_opname_kind opname_from_dyadic_op(ifc_DyadicOperator dyadic_op)
/*
Map an IFC DyadicOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (dyadic_op) {
    case ifc_DyadicOperator_Unknown:
    case ifc_DyadicOperator_Msvc:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for_ifc_operator(dyadic_op));
      op = onk_none;
      break;
    case ifc_DyadicOperator_Plus:            op = onk_plus;              break;
    case ifc_DyadicOperator_Minus:           op = onk_minus;             break;
    case ifc_DyadicOperator_Mult:            op = onk_star;              break;
    case ifc_DyadicOperator_Slash:           op = onk_divide;            break;
    case ifc_DyadicOperator_Modulo:          op = onk_remainder;         break;
    case ifc_DyadicOperator_Remainder:       op = onk_remainder;         break;
    case ifc_DyadicOperator_Bitand:          op = onk_ampersand;         break;
    case ifc_DyadicOperator_Bitor:           op = onk_or;                break;
    case ifc_DyadicOperator_Bitxor:          op = onk_excl_or;           break;
    case ifc_DyadicOperator_Lshift:          op = onk_shift_left;        break;
    case ifc_DyadicOperator_Rshift:          op = onk_shift_right;       break;
    case ifc_DyadicOperator_Equal:           op = onk_eq;                break;
    case ifc_DyadicOperator_NotEqual:        op = onk_ne;                break;
    case ifc_DyadicOperator_Less:            op = onk_lt;                break;
    case ifc_DyadicOperator_LessEqual:       op = onk_le;                break;
    case ifc_DyadicOperator_Greater:         op = onk_gt;                break;
    case ifc_DyadicOperator_GreaterEqual:    op = onk_ge;                break;
    case ifc_DyadicOperator_LogicAnd:        op = onk_and_and;           break;
    case ifc_DyadicOperator_LogicOr:         op = onk_or_or;             break;
    case ifc_DyadicOperator_Assign:          op = onk_assign;            break;
    case ifc_DyadicOperator_PlusAssign:      op = onk_plus_assign;       break;
    case ifc_DyadicOperator_MinusAssign:     op = onk_minus_assign;      break;
    case ifc_DyadicOperator_MultAssign:      op = onk_times_assign;      break;
    case ifc_DyadicOperator_SlashAssign:     op = onk_divide_assign;     break;
    case ifc_DyadicOperator_ModuloAssign:    op = onk_remainder_assign;  break;
    case ifc_DyadicOperator_BitandAssign:    op = onk_and_assign;        break;
    case ifc_DyadicOperator_BitorAssign:     op = onk_or_assign;         break;
    case ifc_DyadicOperator_BitxorAssign:    op = onk_excl_or_assign;    break;
    case ifc_DyadicOperator_LshiftAssign:    op = onk_shift_left_assign; break;
    case ifc_DyadicOperator_RshiftAssign:    op = onk_shift_right_assign;break;
    case ifc_DyadicOperator_Comma:           op = onk_comma;             break;
    case ifc_DyadicOperator_Arrow:           op = onk_arrow;             break;
    case ifc_DyadicOperator_ArrowStar:       op = onk_arrow_star;        break;
    case ifc_DyadicOperator_New:             op = onk_new;               break;
    case ifc_DyadicOperator_NewArray:        op = onk_array_new;         break;
    case ifc_DyadicOperator_Compare:         op = onk_spaceship;         break;
    case ifc_DyadicOperator_Dot:
    case ifc_DyadicOperator_DotStar:
    case ifc_DyadicOperator_Curry:
    case ifc_DyadicOperator_Apply:
    case ifc_DyadicOperator_Index:
    case ifc_DyadicOperator_DefaultAt:
    case ifc_DyadicOperator_Destruct:
    case ifc_DyadicOperator_DestructAt:
    case ifc_DyadicOperator_Cleanup:
    case ifc_DyadicOperator_Qualification:
    case ifc_DyadicOperator_Promote:
    case ifc_DyadicOperator_Demote:
    case ifc_DyadicOperator_Coerce:
    case ifc_DyadicOperator_Rewrite:
    case ifc_DyadicOperator_Bless:
    case ifc_DyadicOperator_Cast:
    case ifc_DyadicOperator_ExplicitConversion:
    case ifc_DyadicOperator_ReinterpretCast:
    case ifc_DyadicOperator_StaticCast:
    case ifc_DyadicOperator_ConstCast:
    case ifc_DyadicOperator_DynamicCast:
    case ifc_DyadicOperator_Narrow:
    case ifc_DyadicOperator_Widen:
    case ifc_DyadicOperator_Pretend:
    case ifc_DyadicOperator_Closure:
    case ifc_DyadicOperator_ZeroInitialize:
    case ifc_DyadicOperator_ClearStorage:
    case ifc_DyadicOperator_MsvcTryCast:
    case ifc_DyadicOperator_MsvcCurry:
    case ifc_DyadicOperator_MsvcVirtualCurry:
    case ifc_DyadicOperator_MsvcAlign:
    case ifc_DyadicOperator_MsvcBitSpan:
    case ifc_DyadicOperator_MsvcBitfieldAccess:
    case ifc_DyadicOperator_MsvcObscureBitfieldAccess:
    case ifc_DyadicOperator_MsvcInitialize:
    case ifc_DyadicOperator_MsvcBuiltinOffsetOf:
    case ifc_DyadicOperator_MsvcIsBaseOf:
    case ifc_DyadicOperator_MsvcIsConvertibleTo:
    case ifc_DyadicOperator_MsvcIsTriviallyAssignable:
    case ifc_DyadicOperator_MsvcIsNothrowAssignable:
    case ifc_DyadicOperator_MsvcIsAssignable:
    case ifc_DyadicOperator_MsvcIsAssignableNocheck:
    case ifc_DyadicOperator_MsvcBuiltinBitCast:
    case ifc_DyadicOperator_MsvcBuiltinIsLayoutCompatible:
    case ifc_DyadicOperator_MsvcBuiltinIsPointerInterconvertibleBaseOf:
    case ifc_DyadicOperator_MsvcBuiltinIsPointerInterconvertibleWithClass:
    case ifc_DyadicOperator_MsvcBuiltinIsCorrespondingMember:
    case ifc_DyadicOperator_MsvcIntrinsic:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for_ifc_operator(dyadic_op));
      op = onk_none;
      break;
    default_is_unexpected_str("Unexpected DyadicOperator");
  }  /* switch */
  return op;
}  /* opname_from_dyadic_op */


static an_opname_kind opname_from_triadic_op(ifc_TriadicOperator triadic_op)
/*
Map an IFC TriadicOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (triadic_op) {
    case ifc_TriadicOperator_Unknown:
    case ifc_TriadicOperator_Msvc:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for_ifc_operator(triadic_op));
      op = onk_none;
      break;
    case ifc_TriadicOperator_Choice:
    case ifc_TriadicOperator_ConstructAt:
    case ifc_TriadicOperator_Initialize:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for_ifc_operator(triadic_op));
      op = onk_none;
      break;
    default_is_unexpected_str("Unexpected TriadicOperator");
  }  /* switch */
  return op;
}  /* opname_from_triadic_op */


static an_opname_kind opname_from_storage_op(ifc_StorageOperator storage_op)
/*
Map an IFC StorageOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (storage_op) {
    case ifc_StorageOperator_Unknown:
    case ifc_StorageOperator_Msvc:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for_ifc_operator(storage_op));
      op = onk_none;
      break;
    case ifc_StorageOperator_AllocateSingle:    op = onk_new;            break;
    case ifc_StorageOperator_AllocateArray:     op = onk_array_new;      break;
    case ifc_StorageOperator_DeallocateSingle:  op = onk_delete;         break;
    case ifc_StorageOperator_DeallocateArray:   op = onk_array_delete;   break;
    default_is_unexpected_str("Unexpected StorageOperator");
  }  /* switch */
  return op;
}  /* opname_from_storage_op */


static an_opname_kind opname_from_variadic_op(ifc_VariadicOperator variadic_op)
/*
Map an IFC VariadicOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (variadic_op) {
    case ifc_VariadicOperator_Unknown:
    case ifc_VariadicOperator_Msvc:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for_ifc_operator(variadic_op));
      op = onk_none;
      break;
    case ifc_VariadicOperator_Collection:
    case ifc_VariadicOperator_Sequence:
    case ifc_VariadicOperator_MsvcHasTrivialConstructor:
    case ifc_VariadicOperator_MsvcIsConstructible:
    case ifc_VariadicOperator_MsvcIsNothrowConstructible:
    case ifc_VariadicOperator_MsvcIsTriviallyConstructible:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for_ifc_operator(variadic_op));
      op = onk_none;
      break;
    default_is_unexpected_str("Unexpected VariadicOperator");
  }  /* switch */
  return op;
}  /* opname_from_variadic_op */


static an_opname_kind opname_from_operator(ifc_Operator ifc_op)
/*
Map an IFC Operator to an_opname_kind.
*/
{
  an_opname_kind   op;
  ifc_OperatorSort ifc_op_sort = operator_tag(ifc_op);
  uint16_t         ifc_op_value = operator_index(ifc_op);

  switch (ifc_op_sort) {
    case ifc_OperatorSort_Niladic:
      op = opname_from_niladic_op((ifc_NiladicOperator)ifc_op_value);
      break;
    case ifc_OperatorSort_Monadic:
      op = opname_from_monadic_op((ifc_MonadicOperator)ifc_op_value);
      break;
    case ifc_OperatorSort_Dyadic:
      op = opname_from_dyadic_op((ifc_DyadicOperator)ifc_op_value);
      break;
    case ifc_OperatorSort_Triadic:
      op = opname_from_triadic_op((ifc_TriadicOperator)ifc_op_value);
      break;
    case ifc_OperatorSort_Storage:
      op = opname_from_storage_op((ifc_StorageOperator)ifc_op_value);
      break;
    case ifc_OperatorSort_Variadic:
      op = opname_from_variadic_op((ifc_VariadicOperator)ifc_op_value);
      break;
    default_is_unexpected_str("Unexpected OperatorSort");
  }  /* switch */
  return op;
}  /* opname_from_operator */


enum an_operator_kind : uint8_t {
  opkind_basic,     /* A basic operator (e.g., +, &&). */
  opkind_post,      /* An operator that follows the argument(s) (e.g., x++). */
  opkind_func_like, /* A function-like operator (e.g., is_assignable). */
  opkind_c_cast,    /* A C-style cast (e.g., (X*)Y). */
  opkind_cpp_cast,  /* A C++-style cast (e.g., reinterpret_cast<X*>Y). */
  opkind_other,     /* Something else. */
};


static an_operator_kind get_operator_kind(ifc_NiladicOperator op)
/*
Return the kind of operator described by op.
*/
{
  an_operator_kind kind;

  switch (op) {
    case ifc_NiladicOperator_Unknown:
    case ifc_NiladicOperator_Msvc:
      unexpected_condition();
      break;
    case ifc_NiladicOperator_Phantom:
    case ifc_NiladicOperator_Constant:
    case ifc_NiladicOperator_Nil:
    case ifc_NiladicOperator_MsvcConstantObject:
    case ifc_NiladicOperator_MsvcLambda:
      kind = opkind_other;
      break;
    default_is_unexpected_str("Unexpected NiladicOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(ifc_MonadicOperator op)
/*
Return the kind of operator described by op.
*/
{
  an_operator_kind kind;

  switch (op) {
    case ifc_MonadicOperator_Unknown:
    case ifc_MonadicOperator_Msvc:
    case ifc_MonadicOperator_MsvcConfusion:
      unexpected_condition();
      break;
    case ifc_MonadicOperator_Plus:
    case ifc_MonadicOperator_Negate:
    case ifc_MonadicOperator_Deref:
    case ifc_MonadicOperator_Address:
    case ifc_MonadicOperator_Complement:
    case ifc_MonadicOperator_Not:
    case ifc_MonadicOperator_PreIncrement:
    case ifc_MonadicOperator_PreDecrement:
    case ifc_MonadicOperator_Await:
    case ifc_MonadicOperator_CoReturn:
    case ifc_MonadicOperator_Yield:
    case ifc_MonadicOperator_Throw:
    case ifc_MonadicOperator_LookupGlobally:
    case ifc_MonadicOperator_New:
    case ifc_MonadicOperator_Delete:
    case ifc_MonadicOperator_DeleteArray:
      kind = opkind_basic;
      break;
    case ifc_MonadicOperator_PostIncrement:
    case ifc_MonadicOperator_PostDecrement:
    case ifc_MonadicOperator_Expand:
      kind = opkind_post;
      break;
    case ifc_MonadicOperator_Paren:
    case ifc_MonadicOperator_Brace:
      kind = opkind_other;
      break;
    case ifc_MonadicOperator_Truncate:
    case ifc_MonadicOperator_Ceil:
    case ifc_MonadicOperator_Floor:
    case ifc_MonadicOperator_Alignas:
    case ifc_MonadicOperator_Alignof:
    case ifc_MonadicOperator_Sizeof:
    case ifc_MonadicOperator_Cardinality:
    case ifc_MonadicOperator_Typeid:
    case ifc_MonadicOperator_Noexcept:
    case ifc_MonadicOperator_Requires:
    case ifc_MonadicOperator_Read:
    case ifc_MonadicOperator_Materialize:
    case ifc_MonadicOperator_PseudoDtorCall:
    case ifc_MonadicOperator_MsvcAssume:
    case ifc_MonadicOperator_MsvcAlignof:
    case ifc_MonadicOperator_MsvcUuidof:
    case ifc_MonadicOperator_MsvcIsClass:
    case ifc_MonadicOperator_MsvcIsUnion:
    case ifc_MonadicOperator_MsvcIsEnum:
    case ifc_MonadicOperator_MsvcIsPolymorphic:
    case ifc_MonadicOperator_MsvcIsEmpty:
    case ifc_MonadicOperator_MsvcIsTriviallyCopyConstructible:
    case ifc_MonadicOperator_MsvcIsTriviallyCopyAssignable:
    case ifc_MonadicOperator_MsvcIsTriviallyDestructible:
    case ifc_MonadicOperator_MsvcHasVirtualDestructor:
    case ifc_MonadicOperator_MsvcIsNothrowCopyConstructible:
    case ifc_MonadicOperator_MsvcIsNothrowCopyAssignable:
    case ifc_MonadicOperator_MsvcIsPod:
    case ifc_MonadicOperator_MsvcIsAbstract:
    case ifc_MonadicOperator_MsvcIsTrivial:
    case ifc_MonadicOperator_MsvcIsTriviallyCopyable:
    case ifc_MonadicOperator_MsvcIsStandardLayout:
    case ifc_MonadicOperator_MsvcIsLiteralType:
    case ifc_MonadicOperator_MsvcIsTriviallyMoveConstructible:
    case ifc_MonadicOperator_MsvcHasTrivialMoveAssign:
    case ifc_MonadicOperator_MsvcIsTriviallyMoveAssignable:
    case ifc_MonadicOperator_MsvcIsNothrowMoveAssignable:
    case ifc_MonadicOperator_MsvcUnderlyingType:
    case ifc_MonadicOperator_MsvcIsDestructible:
    case ifc_MonadicOperator_MsvcIsNothrowDestructible:
    case ifc_MonadicOperator_MsvcHasUniqueObjectRepresentations:
    case ifc_MonadicOperator_MsvcIsAggregate:
    case ifc_MonadicOperator_MsvcBuiltinAddressOf:
    case ifc_MonadicOperator_MsvcIsRefClass:
    case ifc_MonadicOperator_MsvcIsValueClass:
    case ifc_MonadicOperator_MsvcIsSimpleValueClass:
    case ifc_MonadicOperator_MsvcIsInterfaceClass:
    case ifc_MonadicOperator_MsvcIsDelegate:
    case ifc_MonadicOperator_MsvcIsFinal:
    case ifc_MonadicOperator_MsvcIsSealed:
    case ifc_MonadicOperator_MsvcHasFinalizer:
    case ifc_MonadicOperator_MsvcHasCopy:
    case ifc_MonadicOperator_MsvcHasAssign:
    case ifc_MonadicOperator_MsvcHasUserDestructor:
    case ifc_MonadicOperator_MsvcConfusedDependentSizeof:
      kind = opkind_func_like;
      break;
    case ifc_MonadicOperator_MsvcConfusedExpand:
      kind = opkind_post;
      break;
    default_is_unexpected_str("Unexpected MonadicOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(ifc_DyadicOperator op)
/*
Return the kind of operator described by op.
*/
{
  an_operator_kind kind;

  switch (op) {
    case ifc_DyadicOperator_Unknown:
    case ifc_DyadicOperator_Msvc:
      unexpected_condition();
      break;
    case ifc_DyadicOperator_Plus:
    case ifc_DyadicOperator_Minus:
    case ifc_DyadicOperator_Mult:
    case ifc_DyadicOperator_Slash:
    case ifc_DyadicOperator_Modulo:
    case ifc_DyadicOperator_Remainder:
    case ifc_DyadicOperator_Bitand:
    case ifc_DyadicOperator_Bitor:
    case ifc_DyadicOperator_Bitxor:
    case ifc_DyadicOperator_Lshift:
    case ifc_DyadicOperator_Rshift:
    case ifc_DyadicOperator_Equal:
    case ifc_DyadicOperator_NotEqual:
    case ifc_DyadicOperator_Less:
    case ifc_DyadicOperator_LessEqual:
    case ifc_DyadicOperator_Greater:
    case ifc_DyadicOperator_GreaterEqual:
    case ifc_DyadicOperator_LogicAnd:
    case ifc_DyadicOperator_LogicOr:
    case ifc_DyadicOperator_Assign:
    case ifc_DyadicOperator_PlusAssign:
    case ifc_DyadicOperator_MinusAssign:
    case ifc_DyadicOperator_MultAssign:
    case ifc_DyadicOperator_SlashAssign:
    case ifc_DyadicOperator_ModuloAssign:
    case ifc_DyadicOperator_BitandAssign:
    case ifc_DyadicOperator_BitorAssign:
    case ifc_DyadicOperator_BitxorAssign:
    case ifc_DyadicOperator_LshiftAssign:
    case ifc_DyadicOperator_RshiftAssign:
    case ifc_DyadicOperator_Comma:
    case ifc_DyadicOperator_Arrow:
    case ifc_DyadicOperator_ArrowStar:
    case ifc_DyadicOperator_Compare:
    case ifc_DyadicOperator_Dot:
    case ifc_DyadicOperator_DotStar:
      kind = opkind_basic;
      break;
    case ifc_DyadicOperator_New:
    case ifc_DyadicOperator_NewArray:
      kind = opkind_other;
      break;
    case ifc_DyadicOperator_Cast:
    case ifc_DyadicOperator_ExplicitConversion:
      kind = opkind_c_cast;
      break;
    case ifc_DyadicOperator_ReinterpretCast:
    case ifc_DyadicOperator_StaticCast:
    case ifc_DyadicOperator_ConstCast:
    case ifc_DyadicOperator_DynamicCast:
      kind = opkind_cpp_cast;
      break;
    case ifc_DyadicOperator_Curry:
    case ifc_DyadicOperator_Apply:
    case ifc_DyadicOperator_Index:
    case ifc_DyadicOperator_DefaultAt:
    case ifc_DyadicOperator_Destruct:
    case ifc_DyadicOperator_DestructAt:
    case ifc_DyadicOperator_Cleanup:
    case ifc_DyadicOperator_Qualification:
    case ifc_DyadicOperator_Promote:
    case ifc_DyadicOperator_Demote:
    case ifc_DyadicOperator_Coerce:
    case ifc_DyadicOperator_Rewrite:
    case ifc_DyadicOperator_Bless:
    case ifc_DyadicOperator_Narrow:
    case ifc_DyadicOperator_Widen:
    case ifc_DyadicOperator_Pretend:
    case ifc_DyadicOperator_Closure:
    case ifc_DyadicOperator_ZeroInitialize:
    case ifc_DyadicOperator_ClearStorage:
    case ifc_DyadicOperator_MsvcTryCast:
    case ifc_DyadicOperator_MsvcCurry:
    case ifc_DyadicOperator_MsvcVirtualCurry:
    case ifc_DyadicOperator_MsvcAlign:
    case ifc_DyadicOperator_MsvcBitSpan:
    case ifc_DyadicOperator_MsvcBitfieldAccess:
    case ifc_DyadicOperator_MsvcObscureBitfieldAccess:
    case ifc_DyadicOperator_MsvcInitialize:
    case ifc_DyadicOperator_MsvcBuiltinOffsetOf:
    case ifc_DyadicOperator_MsvcIsBaseOf:
    case ifc_DyadicOperator_MsvcIsConvertibleTo:
    case ifc_DyadicOperator_MsvcIsTriviallyAssignable:
    case ifc_DyadicOperator_MsvcIsNothrowAssignable:
    case ifc_DyadicOperator_MsvcIsAssignable:
    case ifc_DyadicOperator_MsvcIsAssignableNocheck:
    case ifc_DyadicOperator_MsvcBuiltinBitCast:
    case ifc_DyadicOperator_MsvcBuiltinIsLayoutCompatible:
    case ifc_DyadicOperator_MsvcBuiltinIsPointerInterconvertibleBaseOf:
    case ifc_DyadicOperator_MsvcBuiltinIsPointerInterconvertibleWithClass:
    case ifc_DyadicOperator_MsvcBuiltinIsCorrespondingMember:
    case ifc_DyadicOperator_MsvcIntrinsic:
      kind = opkind_func_like;
      break;
    default_is_unexpected_str("Unexpected DyadicOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(ifc_TriadicOperator op)
/*
Return the kind of operator described by op.
*/
{
  an_operator_kind kind;

  switch (op) {
    case ifc_TriadicOperator_Unknown:
    case ifc_TriadicOperator_Msvc:
      unexpected_condition();
      break;
    case ifc_TriadicOperator_Choice:
    case ifc_TriadicOperator_ConstructAt:
    case ifc_TriadicOperator_Initialize:
      kind = opkind_other;
      break;
    default_is_unexpected_str("Unexpected TriadicOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(ifc_StorageOperator op)
/*
Return the kind of operator described by op.
*/
{
  an_operator_kind kind;

  switch (op) {
    case ifc_StorageOperator_Unknown:
    case ifc_StorageOperator_Msvc:
      unexpected_condition();
      break;
    case ifc_StorageOperator_AllocateSingle:
    case ifc_StorageOperator_AllocateArray:
    case ifc_StorageOperator_DeallocateSingle:
    case ifc_StorageOperator_DeallocateArray:
      kind = opkind_other;
      break;
    default_is_unexpected_str("Unexpected StorageOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(ifc_VariadicOperator op)
/*
Return the kind of operator described by op.
*/
{
  an_operator_kind kind;

  switch (op) {
    case ifc_VariadicOperator_Unknown:
    case ifc_VariadicOperator_Msvc:
      unexpected_condition();
      break;
    case ifc_VariadicOperator_Collection:
    case ifc_VariadicOperator_Sequence:
      kind = opkind_other;
      break;
    case ifc_VariadicOperator_MsvcHasTrivialConstructor:
    case ifc_VariadicOperator_MsvcIsConstructible:
    case ifc_VariadicOperator_MsvcIsNothrowConstructible:
    case ifc_VariadicOperator_MsvcIsTriviallyConstructible:
      kind = opkind_func_like;
      break;
    default_is_unexpected_str("Unexpected VariadicOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(ifc_Operator op)
/*
Return the kind of operator described by op.
*/
{
  an_operator_kind kind;
  ifc_OperatorSort op_sort = operator_tag(op);
  uint16_t         op_value = operator_index(op);

  switch (op_sort) {
    case ifc_OperatorSort_Niladic:
      kind = get_operator_kind((ifc_NiladicOperator)op_value);
      break;
    case ifc_OperatorSort_Monadic:
      kind = get_operator_kind((ifc_MonadicOperator)op_value);
      break;
    case ifc_OperatorSort_Dyadic:
      kind = get_operator_kind((ifc_DyadicOperator)op_value);
      break;
    case ifc_OperatorSort_Triadic:
      kind = get_operator_kind((ifc_TriadicOperator)op_value);
      break;
    case ifc_OperatorSort_Storage:
      kind = get_operator_kind((ifc_StorageOperator)op_value);
      break;
    case ifc_OperatorSort_Variadic:
      kind = get_operator_kind((ifc_VariadicOperator)op_value);
      break;
    default_is_unexpected_str("Unexpected OperatorSort");
  }  /* switch */
  return kind;
}  /* is_function_like_operator */


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
  a_bit_field	default_name_linkage:NUM_BITS_FOR_NAME_LINKAGE;
			/* Previous default_name_linkage setting. */
  a_bit_field	current_access:2;
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


static a_boolean already_on_deferred_list(a_module_entity_ptr mep,
                                          a_symbol_locator    *loc)
/*
Check whether the module entity specified by mep is in the deferred entity
list for the associated symbol locator (loc).  Return TRUE if so, otherwise
return FALSE.
*/
{
  a_boolean           on_list = FALSE;
  a_module_entity_ptr list_mep;

  check_assertion(loc->symbol_header != NULL);
  for (list_mep = loc->symbol_header->deferred_module_entities;
       list_mep != NULL; list_mep = list_mep->next) {
    if (list_mep == mep) {
      on_list = TRUE;
      break;
    }  /* if */
  }  /* for */
  return on_list;
}  /* already_on_deferred_list */


static void defer_symbol_creation(a_module_entity_ptr mep,
                                  a_symbol_locator    *loc,
                                  a_boolean           make_last = FALSE)
/*
Defer the creation of the module entity specified by mep.  A "lazy loading"
mechanism is used to create symbols only for entities that are referenced.  As
part of that mechanism, when an entity in a module file is discovered (as
specified by the locator information in *loc), rather than creating a symbol
for the entity, the module entity is queued on the symbol header.  If, during
name lookup, a symbol header with a matching, non-NULL deferred_module_entities
field is encountered, an IL entity and symbol are created at that time.
If make_last is FALSE (the default), the module entity entry is added at the
front of the queue; otherwise, it is added at the end.
*/
{
  /* FIXME: Checking for being on the list every time this is called can get
     expensive.  Perhaps add a flag to mep itself to indicate whether it's
     already been added to the deferred list?  Another consideration: Is it
     possible to get here for an entity that's already been completed? */
  if (!already_on_deferred_list(mep, loc)) {
    a_symbol_header_ptr  hdr = loc->symbol_header;
    check_assertion(hdr != NULL);
    if (!make_last) {
      mep->next = hdr->deferred_module_entities;
      hdr->deferred_module_entities = mep;
    } else {
      *get_last_simple_list_link(&hdr->deferred_module_entities) = mep;
    }  /* if */
#if DEBUG
    if (db_flag_is_set("ms_symbols")) {
      (void)fprintf(f_debug, "Defer symbol creation for %s",
                    loc->symbol_header->identifier);
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
  }  /* if */
}  /* defer_symbol_creation */


a_boolean an_ifc_module::matches_module(a_const_char *module_name,
                                        a_const_char *module_file)
/*
Return TRUE if module_file is an IFC module file for module_name, FALSE
otherwise.
*/
{
  a_boolean            result = FALSE;
  a_module_import_decl mid;
  a_module             mod;

  mid.module_info = &mod;
  mod.full_name = module_file;
  if (open_and_map_ifc_module_file(&mid, /*issue_diag=*/FALSE)) {
    a_C_str_handle this_name;
    init_string_table_and_header();
    switch (unit_tag(header.unit)) {
      case ifc_UnitSort_Source:
      case ifc_UnitSort_Header:
        this_name.ptr = NULL;
        break;
      case ifc_UnitSort_Primary:
      case ifc_UnitSort_Partition:
      case ifc_UnitSort_ExportedTU:
        this_name.ptr =
                 get_string_at_offset((ifc_TextOffset)unit_value(header.unit));
        break;
      default_is_unexpected();
    }  /* switch */
    result = (this_name == module_name);
    close();
  }  /* if */
  return result;
}  /* matches_module */

#if CHECKING

static void validate_partition_size(an_ifc_partition      *pp,
                                    an_ifc_partition_kind kind)
/*
Validate that the size of a partition's elements matches the size of the
corresponding data structure for that partition.
*/
{
/* Some data structures are still unspecified (sizeof == 1).  That allowance
   should be removed once the spec is complete. */
#if DEBUG
#  define CHECK_SIZE(data) \
  if (sizeof(an_ifc_##data) != 1 && sizeof(an_ifc_##data) != pp->entry_size) {\
    (void)fprintf(f_debug, "Partition for %s expects entity to have size %u, "\
                           "but has size %lu\n", #data, pp->entry_size,       \
                           (unsigned long)sizeof(an_ifc_##data));             \
    unexpected_condition();                                                   \
  }  /* if */                                                                 \
  break /* user ; */
#else /* !DEBUG */
#  define CHECK_SIZE(data) \
  check_assertion(sizeof(an_ifc_##data) == 1 ||                               \
                  sizeof(an_ifc_##data) == pp->entry_size);                   \
  break /* user ; */
#endif /* DEBUG */
  switch (kind) {
    case ifc_decl_vendor_extension:
      CHECK_SIZE(DeclSort_VendorExtension);
    case ifc_decl_enumerator:
      CHECK_SIZE(DeclSort_Enumerator);
    case ifc_decl_variable:
      CHECK_SIZE(DeclSort_Variable);
    case ifc_decl_parameter:
      CHECK_SIZE(DeclSort_Parameter);
    case ifc_decl_field:
      CHECK_SIZE(DeclSort_Field);
    case ifc_decl_bitfield:
      CHECK_SIZE(DeclSort_Bitfield);
    case ifc_decl_scope:
      CHECK_SIZE(DeclSort_Scope);
    case ifc_decl_enumeration:
      CHECK_SIZE(DeclSort_Enumeration);
    case ifc_decl_alias:
      CHECK_SIZE(DeclSort_Alias);
    case ifc_decl_temploid:
      CHECK_SIZE(DeclSort_Temploid);
    case ifc_decl_template:
      CHECK_SIZE(DeclSort_Template);
    case ifc_decl_partial_specialization:
      CHECK_SIZE(DeclSort_PartialSpecialization);
    case ifc_decl_specialization:
      CHECK_SIZE(DeclSort_Specialization);
    case ifc_decl_concept:
      CHECK_SIZE(DeclSort_Concept);
    case ifc_decl_function:
      CHECK_SIZE(DeclSort_Function);
    case ifc_decl_method:
      CHECK_SIZE(DeclSort_Method);
    case ifc_decl_constructor:
      CHECK_SIZE(DeclSort_Constructor);
    case ifc_decl_inh_ctor:
      CHECK_SIZE(DeclSort_InheritedConstructor);
    case ifc_decl_destructor:
      CHECK_SIZE(DeclSort_Destructor);
    case ifc_decl_reference:
      CHECK_SIZE(DeclSort_Reference);
    case ifc_decl_using_declaration:
      CHECK_SIZE(DeclSort_UsingDeclaration);
    case ifc_decl_using_directive:
      CHECK_SIZE(DeclSort_UsingDirective);
    case ifc_decl_friend:
      CHECK_SIZE(DeclSort_Friend);
    case ifc_decl_expansion:
      CHECK_SIZE(DeclSort_Expansion);
    case ifc_decl_deduction_guide:
      CHECK_SIZE(DeclSort_DeductionGuide);
    case ifc_decl_barren:
      CHECK_SIZE(DeclSort_Barren);
    case ifc_decl_tuple:
      CHECK_SIZE(DeclSort_Tuple);
    case ifc_decl_syntax_tree:
      CHECK_SIZE(DeclSort_SyntaxTree);
    case ifc_decl_intrinsic:
      CHECK_SIZE(DeclSort_Intrinsic);
    case ifc_decl_property:
      CHECK_SIZE(DeclSort_Property);
    case ifc_decl_segment:
      CHECK_SIZE(DeclSort_OutputSegment);
    case ifc_type_vendor_extension:
      CHECK_SIZE(TypeSort_VendorExtension);
    case ifc_type_fundamental:
      CHECK_SIZE(TypeSort_Fundamental);
    case ifc_type_designated:
      CHECK_SIZE(TypeSort_Designated);
    case ifc_type_tor:
      CHECK_SIZE(TypeSort_Tor);
    case ifc_type_syntactic:
      CHECK_SIZE(TypeSort_Syntactic);
    case ifc_type_expansion:
      CHECK_SIZE(TypeSort_Expansion);
    case ifc_type_pointer:
      CHECK_SIZE(TypeSort_Pointer);
    case ifc_type_pointer_to_member:
      CHECK_SIZE(TypeSort_PointerToMember);
    case ifc_type_lvalue_reference:
      CHECK_SIZE(TypeSort_LvalueReference);
    case ifc_type_rvalue_reference:
      CHECK_SIZE(TypeSort_RvalueReference);
    case ifc_type_function:
      CHECK_SIZE(TypeSort_Function);
    case ifc_type_method:
      CHECK_SIZE(TypeSort_Method);
    case ifc_type_array:
      CHECK_SIZE(TypeSort_Array);
    case ifc_type_typename:
      CHECK_SIZE(TypeSort_Typename);
    case ifc_type_qualified:
      CHECK_SIZE(TypeSort_Qualified);
    case ifc_type_base:
      CHECK_SIZE(TypeSort_Base);
    case ifc_type_decltype:
      CHECK_SIZE(TypeSort_Decltype);
    case ifc_type_placeholder:
      CHECK_SIZE(TypeSort_Placeholder);
    case ifc_type_tuple:
      CHECK_SIZE(TypeSort_Tuple);
    case ifc_type_forall:
      CHECK_SIZE(TypeSort_Forall);
    case ifc_type_unaligned:
      CHECK_SIZE(TypeSort_Unaligned);
    case ifc_type_syntax_tree:
      CHECK_SIZE(TypeSort_SyntaxTree);
    case ifc_name_identifier:
      unexpected_condition_str("No partition for NameSort::Identifier"); break;
    case ifc_name_operator:
      CHECK_SIZE(NameSort_Operator);
    case ifc_name_conversion:
      CHECK_SIZE(NameSort_Conversion);
    case ifc_name_literal:
      CHECK_SIZE(NameSort_Literal);
    case ifc_name_template:
      CHECK_SIZE(NameSort_Template);
    case ifc_name_specialization:
      CHECK_SIZE(NameSort_Specialization);
    case ifc_name_source_file:
      CHECK_SIZE(NameSort_SourceFile);
    case ifc_name_guide:
      CHECK_SIZE(NameSort_Guide);
    case ifc_expr_vendor_extension:
      CHECK_SIZE(ExprSort_VendorExtension);
    case ifc_expr_empty:
      CHECK_SIZE(ExprSort_Empty);
    case ifc_expr_literal:
      CHECK_SIZE(ExprSort_Literal);
    case ifc_expr_lambda:
      CHECK_SIZE(ExprSort_Lambda);
    case ifc_expr_type:
      CHECK_SIZE(ExprSort_Type);
    case ifc_expr_decl:
      CHECK_SIZE(ExprSort_NamedDecl);
    case ifc_expr_unresolved_id:
      CHECK_SIZE(ExprSort_UnresolvedId);
    case ifc_expr_template_id:
      CHECK_SIZE(ExprSort_TemplateId);
    case ifc_expr_unqualified_id:
      CHECK_SIZE(ExprSort_UnqualifiedId);
    case ifc_expr_simple_identifier:
      CHECK_SIZE(ExprSort_SimpleIdentifier);
    case ifc_expr_pointer:
      CHECK_SIZE(ExprSort_Pointer);
    case ifc_expr_qualified_name:
      CHECK_SIZE(ExprSort_QualifiedName);
    case ifc_expr_path:
      CHECK_SIZE(ExprSort_Path);
    case ifc_expr_read:
      CHECK_SIZE(ExprSort_Read);
    case ifc_expr_monad:
      CHECK_SIZE(ExprSort_Monad);
    case ifc_expr_dyad:
      CHECK_SIZE(ExprSort_Dyad);
    case ifc_expr_triad:
      CHECK_SIZE(ExprSort_Triad);
    case ifc_expr_string:
      CHECK_SIZE(ExprSort_String);
    case ifc_expr_temporary:
      CHECK_SIZE(ExprSort_Temporary);
    case ifc_expr_call:
      CHECK_SIZE(ExprSort_Call);
    case ifc_expr_member_initializer:
      CHECK_SIZE(ExprSort_MemberInitializer);
    case ifc_expr_member_access:
      CHECK_SIZE(ExprSort_MemberAccess);
    case ifc_expr_inheritance_path:
      CHECK_SIZE(ExprSort_InheritancePath);
    case ifc_expr_initializer_list:
      CHECK_SIZE(ExprSort_InitializerList);
    case ifc_expr_cast:
      CHECK_SIZE(ExprSort_Cast);
    case ifc_expr_condition:
      CHECK_SIZE(ExprSort_Condition);
    case ifc_expr_expression_list:
      CHECK_SIZE(ExprSort_ExpressionList);
    case ifc_expr_sizeof_type:
      CHECK_SIZE(ExprSort_SizeofType);
    case ifc_expr_alignof_type:
      CHECK_SIZE(ExprSort_Alignof);
    case ifc_expr_typeid:
      CHECK_SIZE(ExprSort_Typeid);
    case ifc_expr_destructor_call:
      CHECK_SIZE(ExprSort_DestructorCall);
    case ifc_expr_syntax_tree:
      CHECK_SIZE(ExprSort_SyntaxTree);
    case ifc_expr_function_string:
      CHECK_SIZE(ExprSort_FunctionString);
    case ifc_expr_compound_string:
      CHECK_SIZE(ExprSort_CompoundString);
    case ifc_expr_string_sequence:
      CHECK_SIZE(ExprSort_StringSequence);
    case ifc_expr_initializer:
      CHECK_SIZE(ExprSort_Initializer);
    case ifc_expr_requires:
      CHECK_SIZE(ExprSort_Requires);
    case ifc_expr_unaryfold:
      CHECK_SIZE(ExprSort_UnaryFold);
    case ifc_expr_binaryfold:
      CHECK_SIZE(ExprSort_BinaryFold);
    case ifc_expr_hierarchy_conversion:
      CHECK_SIZE(ExprSort_HierarchyConversion);
    case ifc_expr_product:
      CHECK_SIZE(ExprSort_ProductTypeValue);
    case ifc_expr_sum:
      CHECK_SIZE(ExprSort_SumTypeValue);
    case ifc_expr_subobject:
      CHECK_SIZE(ExprSort_SubobjectValue);
    case ifc_expr_array:
      CHECK_SIZE(ExprSort_ArrayValue);
    case ifc_expr_dynamic_dispatch:
      CHECK_SIZE(ExprSort_DynamicDispatch);
    case ifc_expr_virtual_function:
      CHECK_SIZE(ExprSort_VirtualFunctionConversion);
    case ifc_expr_placeholder:
      CHECK_SIZE(ExprSort_Placeholder);
    case ifc_expr_expansion:
      CHECK_SIZE(ExprSort_Expansion);
    case ifc_expr_generic:
      CHECK_SIZE(ExprSort_Generic);
    case ifc_expr_tuple:
      CHECK_SIZE(ExprSort_Tuple);
    case ifc_expr_nullptr:
      CHECK_SIZE(ExprSort_Nullptr);
    case ifc_expr_this:
      CHECK_SIZE(ExprSort_This);
    case ifc_expr_template_reference:
      CHECK_SIZE(ExprSort_TemplateReference);
    case ifc_expr_push_state:
      CHECK_SIZE(ExprSort_PushState);
    case ifc_expr_type_trait:
      CHECK_SIZE(ExprSort_TypeTraitIntrinsic);
    case ifc_expr_des_init:
      CHECK_SIZE(ExprSort_DesignatedInitializer);
    case ifc_expr_packed_template_arguments:
      CHECK_SIZE(ExprSort_PackedTemplateArguments);
    case ifc_expr_tokens:
      CHECK_SIZE(ExprSort_Tokens);
    case ifc_expr_assign_initializer:
      CHECK_SIZE(ExprSort_AssignInitializer);
    case ifc_stmt_vendor_extension:
      CHECK_SIZE(StmtSort_VendorExtension);
    case ifc_stmt_empty:
      CHECK_SIZE(StmtSort_Empty);
    case ifc_stmt_if:
      CHECK_SIZE(StmtSort_If);
    case ifc_stmt_for:
      CHECK_SIZE(StmtSort_For);
    case ifc_stmt_case:
      CHECK_SIZE(StmtSort_Case);
    case ifc_stmt_while:
      CHECK_SIZE(StmtSort_While);
    case ifc_stmt_block:
      CHECK_SIZE(StmtSort_Block);
    case ifc_stmt_break:
      CHECK_SIZE(StmtSort_Break);
    case ifc_stmt_switch:
      CHECK_SIZE(StmtSort_Switch);
    case ifc_stmt_do_while:
      CHECK_SIZE(StmtSort_DoWhile);
    case ifc_stmt_default:
      CHECK_SIZE(StmtSort_Default);
    case ifc_stmt_continue:
      CHECK_SIZE(StmtSort_Continue);
    case ifc_stmt_expression:
      CHECK_SIZE(StmtSort_Expression);
    case ifc_stmt_return:
      CHECK_SIZE(StmtSort_Return);
    case ifc_stmt_variable:
      CHECK_SIZE(StmtSort_VariableDecl);
    case ifc_stmt_expansion:
      CHECK_SIZE(StmtSort_Expansion);
    case ifc_stmt_syntax_tree:
      CHECK_SIZE(StmtSort_SyntaxTree);
    case ifc_chart_none:
      CHECK_SIZE(ChartSort_None);
    case ifc_chart_unilevel:
      CHECK_SIZE(ChartSort_Unilevel);
    case ifc_chart_multilevel:
      CHECK_SIZE(ChartSort_Multilevel);
    case ifc_attr_nothing:
      unexpected_condition_str("No partition for AttrSort::Nothing"); break;
    case ifc_attr_basic:
      CHECK_SIZE(AttrSort_Basic);
    case ifc_attr_called:
      CHECK_SIZE(AttrSort_Called);
    case ifc_attr_elaborated:
      CHECK_SIZE(AttrSort_Elaborated);
    case ifc_attr_expanded:
      CHECK_SIZE(AttrSort_Expanded);
    case ifc_attr_factored:
      CHECK_SIZE(AttrSort_Factored);
    case ifc_attr_labeled:
      CHECK_SIZE(AttrSort_Labeled);
    case ifc_attr_scoped:
      CHECK_SIZE(AttrSort_Scoped);
    case ifc_attr_tuple:
      CHECK_SIZE(AttrSort_Tuple);
    case ifc_syntax_vendor_extension:
      CHECK_SIZE(SyntaxSort_VendorExtension);
    case ifc_syntax_simple_type_specifier:
      CHECK_SIZE(SyntaxSort_SimpleTypeSpecifier);
    case ifc_syntax_decltype_specifier:
      CHECK_SIZE(SyntaxSort_DecltypeSpecifier);
    case ifc_syntax_placeholder_type_specifier:
      CHECK_SIZE(SyntaxSort_PlaceholderTypeSpecifier);
    case ifc_syntax_type_specifier_seq:
      CHECK_SIZE(SyntaxSort_TypeSpecifierSeq);
    case ifc_syntax_decl_specifier_seq:
      CHECK_SIZE(SyntaxSort_DeclSpecifierSeq);
    case ifc_syntax_virtual_specifier_seq:
      CHECK_SIZE(SyntaxSort_VirtualSpecifierSeq);
    case ifc_syntax_noexcept_specification:
      CHECK_SIZE(SyntaxSort_NoexceptSpecification);
    case ifc_syntax_explicit_specifier:
      CHECK_SIZE(SyntaxSort_ExplicitSpecifier);
    case ifc_syntax_enum_specifier:
      CHECK_SIZE(SyntaxSort_EnumSpecifier);
    case ifc_syntax_enumerator_definition:
      CHECK_SIZE(SyntaxSort_EnumeratorDefinition);
    case ifc_syntax_class_specifier:
      CHECK_SIZE(SyntaxSort_VendorExtension);
    case ifc_syntax_member_specification:
      CHECK_SIZE(SyntaxSort_MemberSpecification);
    case ifc_syntax_member_declaration:
      CHECK_SIZE(SyntaxSort_MemberDeclaration);
    case ifc_syntax_member_declarator:
      CHECK_SIZE(SyntaxSort_MemberDeclarator);
    case ifc_syntax_access_specifier:
      CHECK_SIZE(SyntaxSort_AccessSpecifier);
    case ifc_syntax_base_specifier_list:
      CHECK_SIZE(SyntaxSort_BaseSpecifierList);
    case ifc_syntax_base_specifier:
      CHECK_SIZE(SyntaxSort_BaseSpecifier);
    case ifc_syntax_type_id:
      CHECK_SIZE(SyntaxSort_TypeId);
    case ifc_syntax_trailing_return_type:
      CHECK_SIZE(SyntaxSort_TrailingReturnType);
    case ifc_syntax_declarator:
      CHECK_SIZE(SyntaxSort_Declarator);
    case ifc_syntax_pointer_declarator:
      CHECK_SIZE(SyntaxSort_PointerDeclarator);
    case ifc_syntax_array_declarator:
      CHECK_SIZE(SyntaxSort_ArrayDeclarator);
    case ifc_syntax_function_declarator:
      CHECK_SIZE(SyntaxSort_FunctionDeclarator);
    case ifc_syntax_array_or_function_declarator:
      CHECK_SIZE(SyntaxSort_ArrayOrFunctionDeclarator);
    case ifc_syntax_parameter_declarator:
      CHECK_SIZE(SyntaxSort_ParameterDeclarator);
    case ifc_syntax_init_declarator:
      CHECK_SIZE(SyntaxSort_InitDeclarator);
    case ifc_syntax_new_declarator:
      CHECK_SIZE(SyntaxSort_NewDeclarator);
    case ifc_syntax_simple_declaration:
      CHECK_SIZE(SyntaxSort_SimpleDeclaration);
    case ifc_syntax_exception_declaration:
      CHECK_SIZE(SyntaxSort_ExceptionDeclaration);
    case ifc_syntax_condition_declaration:
      CHECK_SIZE(SyntaxSort_ConditionDeclaration);
    case ifc_syntax_static_assert_declaration:
      CHECK_SIZE(SyntaxSort_StaticAssertDeclaration);
    case ifc_syntax_alias_declaration:
      CHECK_SIZE(SyntaxSort_AliasDeclaration);
    case ifc_syntax_concept_definition:
      CHECK_SIZE(SyntaxSort_ConceptDefinition);
    case ifc_syntax_compound_statement:
      CHECK_SIZE(SyntaxSort_CompoundStatement);
    case ifc_syntax_return_statement:
      CHECK_SIZE(SyntaxSort_ReturnStatement);
    case ifc_syntax_if_statement:
      CHECK_SIZE(SyntaxSort_IfStatement);
    case ifc_syntax_while_statement:
      CHECK_SIZE(SyntaxSort_WhileStatement);
    case ifc_syntax_do_statement:
      CHECK_SIZE(SyntaxSort_DoWhileStatement);
    case ifc_syntax_for_statement:
      CHECK_SIZE(SyntaxSort_ForStatement);
    case ifc_syntax_init_statement:
      CHECK_SIZE(SyntaxSort_InitStatement);
    case ifc_syntax_range_based_for_statement:
      CHECK_SIZE(SyntaxSort_RangeBasedForStatement);
    case ifc_syntax_for_range_declaration:
      CHECK_SIZE(SyntaxSort_ForRangeDeclaration);
    case ifc_syntax_labeled_statement:
      CHECK_SIZE(SyntaxSort_LabeledStatement);
    case ifc_syntax_break_statement:
      CHECK_SIZE(SyntaxSort_BreakStatement);
    case ifc_syntax_continue_statement:
      CHECK_SIZE(SyntaxSort_ContinueStatement);
    case ifc_syntax_switch_statement:
      CHECK_SIZE(SyntaxSort_SwitchStatement);
    case ifc_syntax_goto_statement:
      CHECK_SIZE(SyntaxSort_GotoStatement);
    case ifc_syntax_declaration_statement:
      CHECK_SIZE(SyntaxSort_DeclarationStatement);
    case ifc_syntax_expression_statement:
      CHECK_SIZE(SyntaxSort_ExpressionStatement);
    case ifc_syntax_try_block:
      CHECK_SIZE(SyntaxSort_TryBlock);
    case ifc_syntax_handler:
      CHECK_SIZE(SyntaxSort_Handler);
    case ifc_syntax_handler_seq:
      CHECK_SIZE(SyntaxSort_HandlerSeq);
    case ifc_syntax_function_try_block:
      CHECK_SIZE(SyntaxSort_FunctionTryBlock);
    case ifc_syntax_type_id_list_element:
      CHECK_SIZE(SyntaxSort_TypeIdListElement);
    case ifc_syntax_dynamic_exception_spec:
      CHECK_SIZE(SyntaxSort_DynamicExceptionSpec);
    case ifc_syntax_statement_seq:
      CHECK_SIZE(SyntaxSort_StatementSeq);
    case ifc_syntax_function_body:
      CHECK_SIZE(SyntaxSort_FunctionBody);
    case ifc_syntax_expression:
      CHECK_SIZE(SyntaxSort_Expression);
    case ifc_syntax_function_definition:
      CHECK_SIZE(SyntaxSort_FunctionDefinition);
    case ifc_syntax_member_function_declaration:
      CHECK_SIZE(SyntaxSort_MemberFunctionDeclaration);
    case ifc_syntax_template_declaration:
      CHECK_SIZE(SyntaxSort_TemplateDeclaration);
    case ifc_syntax_requires_clause:
      CHECK_SIZE(SyntaxSort_RequiresClause);
    case ifc_syntax_simple_requirement:
      CHECK_SIZE(SyntaxSort_SimpleRequirement);
    case ifc_syntax_type_requirement:
      CHECK_SIZE(SyntaxSort_TypeRequirement);
    case ifc_syntax_compound_requirement:
      CHECK_SIZE(SyntaxSort_CompoundRequirement);
    case ifc_syntax_nested_requirement:
      CHECK_SIZE(SyntaxSort_NestedRequirement);
    case ifc_syntax_requirement_body:
      CHECK_SIZE(SyntaxSort_RequirementBody);
    case ifc_syntax_type_template_parameter:
      CHECK_SIZE(SyntaxSort_TypeTemplateParameter);
    case ifc_syntax_template_template_parameter:
      CHECK_SIZE(SyntaxSort_TemplateTemplateParameter);
    case ifc_syntax_type_template_argument:
      CHECK_SIZE(SyntaxSort_TypeTemplateArgument);
    case ifc_syntax_non_type_template_argument:
      CHECK_SIZE(SyntaxSort_NonTypeTemplateArgument);
    case ifc_syntax_template_parameter_list:
      CHECK_SIZE(SyntaxSort_TemplateParameterList);
    case ifc_syntax_template_argument_list:
      CHECK_SIZE(SyntaxSort_TemplateArgumentList);
    case ifc_syntax_template_id:
      CHECK_SIZE(SyntaxSort_TemplateId);
    case ifc_syntax_mem_initializer:
      CHECK_SIZE(SyntaxSort_MemInitializer);
    case ifc_syntax_ctor_initializer:
      CHECK_SIZE(SyntaxSort_CtorInitializer);
    case ifc_syntax_lambda_introducer:
      CHECK_SIZE(SyntaxSort_LambdaIntroducer);
    case ifc_syntax_lambda_declarator:
      CHECK_SIZE(SyntaxSort_LambdaDeclarator);
    case ifc_syntax_capture_default:
      CHECK_SIZE(SyntaxSort_CaptureDefault);
    case ifc_syntax_simple_capture:
      CHECK_SIZE(SyntaxSort_SimpleCapture);
    case ifc_syntax_init_capture:
      CHECK_SIZE(SyntaxSort_InitCapture);
    case ifc_syntax_this_capture:
      CHECK_SIZE(SyntaxSort_ThisCapture);
    case ifc_syntax_attributed_statement:
      CHECK_SIZE(SyntaxSort_AttributedStatement);
    case ifc_syntax_attributed_declaration:
      CHECK_SIZE(SyntaxSort_AttributedDeclaration);
    case ifc_syntax_attribute_specifier_seq:
      CHECK_SIZE(SyntaxSort_AttributeSpecifierSeq);
    case ifc_syntax_attribute_specifier:
      CHECK_SIZE(SyntaxSort_AttributeSpecifier);
    case ifc_syntax_attribute_using_prefix:
      CHECK_SIZE(SyntaxSort_AttributeUsingPrefix);
    case ifc_syntax_attribute:
      CHECK_SIZE(SyntaxSort_Attribute);
    case ifc_syntax_attribute_argument_clause:
      CHECK_SIZE(SyntaxSort_AttributeArgumentClause);
    case ifc_syntax_alignas:
      CHECK_SIZE(SyntaxSort_Alignas);
    case ifc_syntax_using_declaration:
      CHECK_SIZE(SyntaxSort_UsingDeclaration);
    case ifc_syntax_using_declarator:
      CHECK_SIZE(SyntaxSort_UsingDeclarator);
    case ifc_syntax_using_directive:
      CHECK_SIZE(SyntaxSort_UsingDirective);
    case ifc_syntax_array_index:
      CHECK_SIZE(SyntaxSort_ArrayIndex);
    case ifc_syntax_seh_try:
      CHECK_SIZE(SyntaxSort_SEHTry);
    case ifc_syntax_seh_except:
      CHECK_SIZE(SyntaxSort_SEHExcept);
    case ifc_syntax_seh_finally:
      CHECK_SIZE(SyntaxSort_SEHFinally);
    case ifc_syntax_seh_leave:
      CHECK_SIZE(SyntaxSort_SEHLeave);
    case ifc_syntax_type_trait_intrinsic:
      CHECK_SIZE(SyntaxSort_TypeTraitIntrinsic);
    case ifc_syntax_tuple:
      CHECK_SIZE(SyntaxSort_Tuple);
    case ifc_syntax_asm_statement:
      CHECK_SIZE(SyntaxSort_AsmStatement);
    case ifc_syntax_namespace_alias_definition:
      CHECK_SIZE(SyntaxSort_NamespaceAliasDefinition);
    case ifc_syntax_super:
      CHECK_SIZE(SyntaxSort_Super);
    case ifc_syntax_unary_fold_expression:
      CHECK_SIZE(SyntaxSort_UnaryFoldExpression);
    case ifc_syntax_binary_fold_expression:
      CHECK_SIZE(SyntaxSort_BinaryFoldExpression);
    case ifc_syntax_empty_statement:
      CHECK_SIZE(SyntaxSort_EmptyStatement);
    case ifc_syntax_structured_binding_declaration:
      CHECK_SIZE(SyntaxSort_StructuredBindingDeclaration);
    case ifc_syntax_structured_binding_identifier:
      CHECK_SIZE(SyntaxSort_StructuredBindingIdentifier);
    case ifc_syntax_using_enum_decl:
      CHECK_SIZE(SyntaxSort_UsingEnumDeclaration);
    case ifc_form_ident:
      CHECK_SIZE(FormSort_Identifier);
    case ifc_form_number:
      CHECK_SIZE(FormSort_Number);
    case ifc_form_char:
      CHECK_SIZE(FormSort_Character);
    case ifc_form_string:
      CHECK_SIZE(FormSort_String);
    case ifc_form_operator:
      CHECK_SIZE(FormSort_Operator);
    case ifc_form_keyword:
      CHECK_SIZE(FormSort_Keyword);
    case ifc_form_whitespace:
      CHECK_SIZE(FormSort_Whitespace);
    case ifc_form_param:
      CHECK_SIZE(FormSort_Parameter);
    case ifc_form_stringize:
      CHECK_SIZE(FormSort_Stringize);
    case ifc_form_catenate:
      CHECK_SIZE(FormSort_Catenate);
    case ifc_form_pragma:
      CHECK_SIZE(FormSort_Pragma);
    case ifc_form_header:
      CHECK_SIZE(FormSort_Header);
    case ifc_form_parenthesized:
      CHECK_SIZE(FormSort_Parenthesized);
    case ifc_form_tuple:
      CHECK_SIZE(FormSort_Tuple);
    case ifc_form_junk:
      CHECK_SIZE(FormSort_Junk);
    case ifc_msvc_trait_decl_attrs:
      CHECK_SIZE(Trait_MsvcDeclAttrs);
    case ifc_msvc_trait_named_func_params:
      CHECK_SIZE(Trait_MsvcFuncParams);
    case ifc_msvc_trait_uuid:
      CHECK_SIZE(Trait_MsvcUuid);
    case ifc_msvc_trait_vendor_traits:
      CHECK_SIZE(Trait_MsvcVendorTrait);
    case ifc_scope_desc:
      CHECK_SIZE(Scope_Descriptor);
    case ifc_scope_member:
      CHECK_SIZE(Scope_Member);
    case ifc_sentence:
      CHECK_SIZE(Sentence);
    case ifc_src_line:
      CHECK_SIZE(Source_Line);
    case ifc_trait_attribute:
      CHECK_SIZE(Trait_Attribute);
    case ifc_trait_alias_template:
      CHECK_SIZE(Trait_AliasTemplate);
    case ifc_trait_function_definition:
      CHECK_SIZE(Trait_FunctionDefinition);
    case ifc_trait_deduction_guides:
      CHECK_SIZE(Trait_DeductionGuides);
    case ifc_trait_deprecated:
      CHECK_SIZE(Trait_Deprecated);
    case ifc_trait_friend:
      CHECK_SIZE(Trait_Friend);
    case ifc_trait_requires:
      CHECK_SIZE(Trait_Requires);
    case ifc_trait_specialization:
      CHECK_SIZE(Trait_Specialization);
    case ifc_word:
      CHECK_SIZE(Word);
    case ifc_cmd_line:
    case ifc_const_f64:
    case ifc_const_i64:
    case ifc_const_str:
    case ifc_form_spec:
    case ifc_heap_chart:
    case ifc_heap_attr:
    case ifc_heap_decl:
    case ifc_heap_expr:
    case ifc_heap_form:
    case ifc_heap_pp:
    case ifc_heap_stmt:
    case ifc_heap_syn:
    case ifc_heap_type:
    case ifc_macro_func_like:
    case ifc_macro_obj_like:
    case ifc_msvc_trait_code_segment:
    case ifc_msvc_trait_codegen_expr_trees:
    case ifc_msvc_trait_entity_init_locus:
    case ifc_msvc_trait_impl_pragmas:
    case ifc_msvc_trait_spec_encodings:
    case ifc_msvc_trait_suppressed_warnings:
    case ifc_msvc_trait_templ_templ_param_classes:
    case ifc_module_exported:
    case ifc_module_imported:
    case ifc_pragma_state:
    case ifc_pragma_vendorext:
      /* No data structure associated with these. */
      break;
    case ifc_none:
    case ifc_last:
    case ifc_invalid_partition:
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
#undef CHECK_SIZE
}  /* validate_partition_size */

#else /* !CHECKING */

#define validate_partition_size(pp, kind) /* Nothing */

#endif /* CHECKING */

namespace {
constexpr ifc_Version supported_major_version = (ifc_Version)0;
constexpr ifc_Version supported_minor_version = (ifc_Version)41;

inline a_boolean check_ifc_version(ifc_Version major,
                                   ifc_Version minor)
/*
Return TRUE if the provided IFC major and minor versions are supported, FALSE
otherwise.
*/
{
  a_boolean result = FALSE;

  if (major == supported_major_version && minor == supported_minor_version) {
    result = TRUE;
  }  /* if */
  return result;
}  /* check_ifc_version */


inline uint32_t *alloc_validation_bit_array(uint32_t num_elements)
/*
Allocate an array of bits for validating num_elements elements, with two bits
for each element.
*/
{
  /* Over allocate by one integer as this simplifies the logic and is necessary
     in the vast majority of cases (where num_elements is not evenly divisible
     by 16) anyways. */
  size_t size = (1 + (num_elements / 16)) * sizeof(uint32_t);
  uint32_t *validation_bits = (uint32_t *)alloc_fe(size);

  memzero((char *)validation_bits, size);
  return validation_bits;
}  /* allocate_validation_bit_array */


inline void emit_unsupported_ifc_version_diagnostic(
                                           a_module_import_decl_ptr midp,
                                           an_ifc_module            *mod_iface,
                                           an_error_severity        severity)
/*
Emit a diagnostic of the given severity, indicating that the given module
import and its associated module interface, is backed by an unsupported version
of the IFC.
*/
{
  a_module_ptr mod = midp->module_info;

  check_assertion(mod == mod_iface->assoc_module_info);
  pos_st_num2_diagnostic(severity, ec_unsupported_ifc_file_version,
                         &midp->module_name_position, mod->full_name,
                         mod_iface->header.major_version,
                         mod_iface->header.minor_version);
}  /* emit_unsupported_ifc_version_diagnostic */
}  /* namespace */


a_boolean an_ifc_module::import(a_module_import_decl_ptr midp)
/*
Import an IFC module file described by midp.  The IFC file should already have
been confirmed to exist and the path stored in midp.
*/
{
  a_module_ptr mod = midp->module_info;
  a_boolean    result = FALSE;

  check_assertion(midp->module_info->kind == (a_module_kind)mk_ifc);
  check_assertion(mod->name != NULL && mod->full_name != NULL);
  check_assertion(mod->module_interface == this);
  if (open_and_map_ifc_module_file(midp, /*issue_diag=*/TRUE)) {
    result = initialize_members_from_ifc_module_file(midp,
                                                     /*issue_diag=*/TRUE);
    if (!result) {
      close();
      goto done;
    }  /* if */
    import_referenced_modules();
#if DEBUG
    if (db_flag_is_set("ms_modsrc")) {
      /* Generate a textual representation of the module file and print it. */
      db_ifc_scope(header.global_scope);
    }  /* if */
#endif /* DEBUG */
    /* Indicate to the lookup routines that lazy symbols are in use. */
    lazy_symbols_may_be_visible = TRUE;
    /* Process all declarations in the global scope. */
    process_ifc_scope(header.global_scope, il_header.primary_scope);
    if (is_header_unit(mod)) {
      export_ifc_macros();
    }  /* if */
  }  /* if */
done:
  return result;
}  /* import */


a_module_import_decl_ptr an_ifc_module::transitive_import_module(
                                                const ifc_ModuleReference *ref)
/*
Given a module reference, import the referenced module.
*/
{
  a_module_import_decl_ptr midp;
  a_const_char             *prim_name, *part_name;

  midp = referenced_modules.get(ref->as_key());
  if (midp != NULL) {
    /* Already imported this reference. */
  } else {
    midp = alloc_module_import_decl();
    referenced_modules.map(ref->as_key(), midp);
    midp->module_name_position = null_source_position;
    prim_name = ref->owner != 0 ? get_string_at_offset(ref->owner) : NULL;
    part_name = ref->partition != 0 ? get_string_at_offset(ref->partition)
                                    : NULL;
    if (prim_name == NULL) {
      /* This is a header unit. */
      check_assertion(part_name != NULL);
      midp->module_info = alloc_module((a_module_kind)mk_header);
      midp->module_info->name = copy_string_to_region(FILE_SCOPE_REGION_NUMBER,
                                                      part_name);
      import_header_module(midp);
    } else {
      a_symbol_ptr module_sym = make_module_symbol(prim_name, part_name,
                                                   /*is_interface=*/TRUE,
                                                   &null_source_position);
      midp->module_info = alloc_module((a_module_kind)mk_ifc);
      midp->module_info->name = module_sym->header->identifier;
      import_module(midp, module_sym);
    }  /* if */
  }  /* if */
  return midp;
}  /* transitive_import_module */


void an_ifc_module::import_referenced_modules()
/*
Import all appropriate modules that have been referenced by this module.
Modules that have been imported but not re-exported are not imported at this
time, as their symbols are not visible except when referenced by symbols within
this module.
*/
{
  if (partitions[ifc_module_exported].name != NULL) {
    auto num_modules = get_num_entries(ifc_module_exported);
    read_prechecked_partition_element(ifc_module_exported, 0);
    for (decltype(num_modules) idx = 0; idx < num_modules; ++idx) {
      ifc_ModuleReference imr;
      GET_ModuleReference(imr, /*from_header=*/FALSE);
      transitive_import_module(&imr);
    }  /* for */
  }  /* if */
}  /* import_referenced_modules */


void an_ifc_module::define_ifc_macro(ifc_MacroIndex macro)
/*
Given an IFC macro, process that macro definition.  Note that this function
assumes variables such as "in_preprocessing_directive", "curr_source_line", and
all related variables have been set appropriately by the calling function (see
export_ifc_macros), to avoid constantly setting and re-setting the values of
these variables.  Similarly, curr_token is assumed to be either ignorable or
already saved for restoration.
*/
{
  a_token_cache      cache;

  read_prechecked_partition_element(macro);
  clear_token_cache(&cache, /*reuseable=*/FALSE);
  cache_macro(&cache, macro);
  check_assertion(cache.first_token != NULL &&
                  cache.first_token->token == tok_identifier);
  /* Create an equivalent define directive so that we can leave the processing
     to proc_define. */
  copy_source_position(cache.first_token->source_position, pos_curr_token);
  init_token_string(&pos_curr_token, /*keep_spacing=*/FALSE,
                    /*suppress_identifier_wrapping=*/TRUE);
  add_token_cache_to_string(&cache);
  put_ch_to_temp_text_buffer(LE_ESCAPE);
  put_ch_to_temp_text_buffer(LE_NEWLINE);
  put_ch_to_temp_text_buffer(LE_ESCAPE);
  put_ch_to_temp_text_buffer(LE_END_OF_LINE);
  curr_char_loc = curr_source_line = start_of_curr_token = temp_text_buffer;
  len_of_curr_token =
           cache.first_token->variant.locator.symbol_header->identifier_length;
  after_end_of_curr_source_line = temp_text_buffer + pos_in_temp_text_buffer;
  logical_char_info_entries_used = 0;
  (void)proc_define();
  if (curr_token != tok_newline) {
    expect_error();
  }  /* if */
}  /* define_ifc_macro */


void an_ifc_module::export_ifc_macros()
/*
Export all macro definitions in this module (presumably a header unit).
*/
{
  Value_saver<a_source_position> pos_curr_token_saver(&pos_curr_token);
  Value_saver<a_boolean>         expand_macros_saver(&expand_macros,
                                                     /*new_value=*/FALSE);
  Value_saver<a_boolean>         ppd_saver(&in_preprocessing_directive,
                                           /*new_value=*/TRUE);
  /* Do not set this yet - cache_curr_token below may make the incorrect choice
     if this is set to TRUE prior to calling it. */
  Value_saver<a_boolean>         pp_tok_saver(&fetch_pp_tokens);
  Value_saver<a_boolean>         scanning_macro_saver(&scanning_module_macro,
                                                      /*new_value=*/TRUE);
  Value_saver<a_const_char*>     curr_source_saver(&curr_source_line,
                                                   /*new_value=*/NULL);
  Value_saver<a_const_char*>     end_curr_source_saver(
                                                &after_end_of_curr_source_line,
                                                /*new_value=*/NULL);
  Value_saver<a_const_char*>     curr_char_saver(&curr_char_loc,
                                                 /*new_value=*/NULL);
  a_token_cache                  cache;

  /* Save the current token to restore later so that it's not lost. */
  clear_token_cache(&cache, /*reusable=*/FALSE);
  cache_curr_token(&cache);
  /* Now that we've cached curr_token, it's safe to set fetch_pp_tokens. */
  fetch_pp_tokens = TRUE;
  /* The IFC files split macros up into two forms - object-like and
     function-like.  Both need to be processed. */
  if (partitions[ifc_macro_obj_like].size > 0) {
    uint32_t n_macros = partitions[ifc_macro_obj_like].size /
                                     partitions[ifc_macro_obj_like].entry_size;
    for (uint32_t idx = 0; idx < n_macros; ++idx) {
      define_ifc_macro(make_macro_index(ifc_MacroSort_ObjectLike, idx));
    }  /* for */
  }  /* if */
  if (partitions[ifc_macro_func_like].size > 0) {
    uint32_t n_macros = partitions[ifc_macro_func_like].size /
                                    partitions[ifc_macro_func_like].entry_size;
    for (uint32_t idx = 0; idx < n_macros; ++idx) {
      define_ifc_macro(make_macro_index(ifc_MacroSort_FunctionLike, idx));
    }  /* for */
  }  /* if */
  /* Restore the current token. */
  f_rescan_cached_tokens(&cache, /*discard_curr_token=*/TRUE);
}  /* export_ifc_macros */


void an_ifc_module::close()
/*
Close the module file specified in the module-import-declaration.
*/
{
  if (f_module != NULL) {
    (void)fclose(f_module);
    f_module = NULL;
#if USE_MMAP_FOR_MEMORY_REGIONS
#if EDG_WIN32
    close_mapped_input_file(mapped_input, map_object);
    mapped_input = NULL;
    map_object = NULL;
#endif /* EDG_WIN32 */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  }  /* if */
}  /* close_ifc_module_file */


void an_ifc_module::pch_reset(a_module_import_decl_ptr midp)
/*
Called after a PCH file has been read to re-open and re-mmap the specified
module.  Note that the mmap-ed address does not need to be at the same
location as the original.
*/
{
  /* Any diagnostics related to opening the module file were already issued
     when the PCH file was first created. */
  if (!open_and_map_ifc_module_file(midp, /*issue_diag=*/FALSE)) {
    /* This shouldn't happen (the PCH processing checks the existence and
       modification time of module files). */
    unexpected_condition();
  }  /* if */
  if (!initialize_members_from_ifc_module_file(midp, /*issue_diag=*/FALSE)) {
    /* This shouldn't happen (the original initialization succeeded). */
    unexpected_condition();
  }  /* if */
}  /* ifc_modules_pch_reset */


static void prepare_cached_template_parse(a_token_cache_ptr      cache,
                                          a_scope_ptr            encl_scope,
                                          a_decl_parse_state_ptr dps,
                                          a_tmpl_decl_state_ptr  decl_state,
                                          a_token_kind           *final_token)
/*
Prepare parsing of a cached template or template specialization declaration.
cache represents the cache to be parsed.  encl_scope is the scope containing
the declaration to be parsed.  dps is a pointer to the storage for the
associated decl parse state.  decl_state is a pointer to the storage for the
associated template decl state.  Finally, final_token is a pointer to the
associated storage for the final token seen during parsing.
*/
{
  push_stop_token_stack();
  rescan_cached_tokens(cache);
  init_decl_parse_state(dps);
  init_templ_decl_state(decl_state, dps);
  decl_state->pragmas_bound_to_template = extract_curr_construct_pragmas();
  decl_state->starting_token_sequence_number = curr_token_sequence_number;
  decl_state->final_token_ptr = final_token;
  decl_state->enclosing_scope = encl_scope;
  decl_state->orig_decl_level = decl_scope_level;
  decl_state->effective_decl_level = decl_scope_level;
}  /* prepare_cached_template_parse */


static void finish_cached_template_parse(a_token_kind *final_token)
/*
Finish parsing of a cached template or template specialization declaration.
final_token is a pointer to expected final token of the parse.
*/
{
  /* FIXME: Reintegrate the final token concept. */
  clear_stop_tokens();
  flush_tokens_without_warning();
  pop_stop_token_stack();
  check_assertion(curr_token == tok_end_of_source);
  (void)get_token();
}  /* finish_cached_template_parse */


static a_template_ptr parse_cached_template(a_token_cache_ptr cache,
                                            a_scope_ptr       encl_scope)
/*
Parse the tokens corresponding to a template declaration cache, and return the
corresponding template.  encl_scope is the scope containing the template
declaration.
*/
{
  a_decl_parse_state dps;
  a_tmpl_decl_state  decl_state;
  a_token_kind       final_token = tok_semicolon;

#if DEBUG
  if (db_flag_is_set("ms_ifc_token_def")) {
    fprintf(f_debug, "Reconstituted template declaration:\n");
    db_tokens(cache);
    fprintf(f_debug, "\n---------------------\n");
  }  /* if */
#endif /* DEBUG */
  prepare_cached_template_parse(cache, encl_scope,
                                &dps, &decl_state, &final_token);
  template_or_specialization_declaration_full(&decl_state,
                                              /*is_generic=*/FALSE,
                                              /*orig_dps=*/NULL);
  finish_cached_template_parse(&final_token);
  return decl_state.il_template_entry;
}  /* parse_cached_template */


static a_template_ptr parse_cached_partial_specialization(
                                                  a_token_cache_ptr cache,
                                                  a_scope_ptr       encl_scope)
/*
Parse the tokens corresponding to a partial specialization declaration cache,
and return the corresponding partial specialization.  encl_scope is the scope
containing the partial specialization declaration.
*/
{
  a_decl_parse_state dps;
  a_tmpl_decl_state  decl_state;
  a_token_kind       final_token = tok_semicolon;

#if DEBUG
  if (db_flag_is_set("ms_ifc_token_def")) {
    fprintf(f_debug, "Reconstituted partial specialization declaration:\n");
    db_tokens(cache);
    fprintf(f_debug, "\n---------------------\n");
  }  /* if */
#endif /* DEBUG */
  prepare_cached_template_parse(cache, encl_scope,
                                &dps, &decl_state, &final_token);
  template_or_specialization_declaration_full(&decl_state,
                                              /*is_generic=*/FALSE,
                                              /*orig_dps=*/NULL);
  finish_cached_template_parse(&final_token);
  return decl_state.il_template_entry;
}  /* parse_cached_partial_specialization */


namespace {
/*
A simple structure that can be used to locate the body of a function stored in
an IFC module file.
*/
struct an_ifc_function_body {
  ifc_DeclIndex decl;
                        /* The IFC "DeclIndex" of the routine. */
  an_ifc_module *ifc_module;
                        /* The IFC module descriptor that this body is
                           associated with. */
};


using an_ifc_function_body_map = Ptr_map<a_routine_ptr, an_ifc_function_body>;
                        /* The type of a map that associates IFC function
                           bodies with IL routine entries. */

an_ifc_function_body_map
                *ifc_function_bodies;
                        /* A map from IL routine entry pointers to entries of
                           type an_ifc_function_body that can be used to
                           retrieve the definition of a function body when
                           needed. */

}  /* namespace */


void record_pending_ifc_function_body(a_routine_ptr  rp,
                                      ifc_DeclIndex  decl_idx,
                                      an_ifc_module  *ifc_module)
/*
Record the information needed to retrieve a definition for rp if it turns out
to be needed later on.
*/
{
  (void)ifc_function_bodies->map_or_replace(
                            rp, an_ifc_function_body{ decl_idx, ifc_module });
#if CHECKING
  // FIXME: We should check that rp is from a header unit.  That should be the
  //        only way we can reach two definitions for the same routine.
#endif /* CHECKING */
}  /* record_pending_ifc_function_body */


void an_ifc_module::cache_statement(a_token_cache_ptr         cache,
                                    ifc_StmtIndex             stmt_idx,
                  /* Defaulted: */  a_cache_statement_option  options)
/*
Add tokens corresponding to the statement at stmt_idx to the given cache.  If
options & cso_func_body is nonzero (it is zero by default), the function is
being called for the top-level statement of a function: In the IFC
representation that is not always a compound statement (ifc_StmtSort_Block) and
therefore the caller takes responsibility for generating braces in that case
(i.e., when options & cso_func_body is nonzero, this routine does not cache
delimiting braces for a compound statement).  No terminating semicolon is added
to the given cache if options & cso_no_final_semicolon is nonzero.
*/
{
  a_source_position  pos;
  ifc_StmtSort       tag = stmt_tag(stmt_idx);

  read_prechecked_partition_element(stmt_idx);
  switch (tag) {
    case ifc_StmtSort_VendorExtension:
      { an_ifc_StmtSort_VendorExtension issve;
        get_StmtSort_VendorExtension(&issve);
        issue_unsupported_node_diag("StmtSort::VendorExtension",
                                    &error_position);
      }
      break;
    case ifc_StmtSort_Empty:
      { an_ifc_StmtSort_Empty isse, *issep;
        issep = get_StmtSort_Empty(&isse);
        source_position_from_locus(&pos, &issep->locus);
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_StmtSort_If:
      { an_ifc_StmtSort_If issi, *issip;
        issip = get_StmtSort_If(&issi);
        source_position_from_locus(&pos, &issip->locus);
        cache_token(cache, tok_if, &pos);
        if (issip->initialization != 0) {
          cache_statement(cache, issip->initialization);
        }  /* if */
        cache_token(cache, tok_lparen, &pos);
        cache_statement(cache, issip->condition, cso_no_final_semicolon);
        cache_token(cache, tok_rparen, &pos);
        cache_statement(cache, issip->consequence);
        if (issip->alternative != 0) {
          cache_token(cache, tok_else, &pos);
          cache_statement(cache, issip->alternative);
        }  /* if */
      }
      break;
    case ifc_StmtSort_For:
      { an_ifc_StmtSort_For issf, *issfp;
        issfp = get_StmtSort_For(&issf);
        source_position_from_locus(&pos, &issfp->locus);
        cache_token(cache, tok_for, &pos);
        cache_token(cache, tok_lparen, &pos);
        cache_statement(cache, issfp->initialization);
        cache_statement(cache, issfp->condition);
        cache_statement(cache, issfp->continuation, cso_no_final_semicolon);
        cache_token(cache, tok_rparen, &pos);
        cache_statement(cache, issfp->body);
      }
      break;
    case ifc_StmtSort_Case:
      { an_ifc_StmtSort_Case issc, *isscp;
        isscp = get_StmtSort_Case(&issc);
        source_position_from_locus(&pos, &isscp->locus);
        cache_expr(cache, isscp->expr);
        cache_token(cache, tok_colon, &pos);
      }
      break;
    case ifc_StmtSort_While:
      { an_ifc_StmtSort_While issw, *isswp;
        isswp = get_StmtSort_While(&issw);
        source_position_from_locus(&pos, &isswp->locus);
        cache_token(cache, tok_while, &pos);
        cache_token(cache, tok_lparen, &pos);
        cache_statement(cache, isswp->condition, cso_no_final_semicolon);
        cache_token(cache, tok_rparen, &pos);
        cache_statement(cache, isswp->body);
      }
      break;
    case ifc_StmtSort_Block:
      { an_ifc_StmtSort_Block issb, *issbp;
        issbp = get_StmtSort_Block(&issb);
        uint32_t  k, N = (uint32_t)issbp->cardinality;
        if (!(options & cso_func_body)) {
          cache_token(cache, tok_lbrace, &null_source_position);
        }  /* if */
        for (k = 0; k < N; ++k) {
          ifc_StmtIndex si;
          read_prechecked_partition_element(ifc_heap_stmt, issbp->start+k);
          GET_StmtIndex(si, /*from_header=*/FALSE);
          /* IFC files sometimes have a NULL statement in this list - don't
             attempt to cache these. */
          if (si == 0) continue;
          cache_statement(cache, si);
        }  /* for */
        if (!(options & cso_func_body)) {
          cache_token(cache, tok_rbrace, &null_source_position);
        }  /* if */
      }
      break;
    case ifc_StmtSort_Break:
      { an_ifc_StmtSort_Break issb, *issbp;
        issbp = get_StmtSort_Break(&issb);
        source_position_from_locus(&pos, &issbp->locus);
        cache_token(cache, tok_break, &pos);
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_StmtSort_Switch:
      { an_ifc_StmtSort_Switch isss, *isssp;
        isssp = get_StmtSort_Switch(&isss);
        source_position_from_locus(&pos, &isssp->locus);
        cache_token(cache, tok_switch, &pos);
        cache_token(cache, tok_lparen, &pos);
        cache_expr(cache, isssp->condition);
        cache_token(cache, tok_rparen, &pos);
        cache_statement(cache, isssp->body);
      }
      break;
    case ifc_StmtSort_DoWhile:
      { an_ifc_StmtSort_DoWhile issdw, *issdwp;
        issdwp = get_StmtSort_DoWhile(&issdw);
        source_position_from_locus(&pos, &issdwp->locus);
        cache_token(cache, tok_do, &pos);
        cache_statement(cache, issdwp->body);
        cache_token(cache, tok_while, &pos);
        cache_token(cache, tok_lparen, &pos);
        cache_token(cache, tok_rparen, &pos);
        cache_statement(cache, issdwp->condition, cso_no_final_semicolon);
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_StmtSort_Default:
      { an_ifc_StmtSort_Default issd, *issdp;
        issdp = get_StmtSort_Default(&issd);
        source_position_from_locus(&pos, &issdp->locus);
        cache_token(cache, tok_default, &pos);
        cache_token(cache, tok_colon, &pos);
      }
      break;
    case ifc_StmtSort_Continue:
      { an_ifc_StmtSort_Continue issc, *isscp;
        isscp = get_StmtSort_Continue(&issc);
        source_position_from_locus(&pos, &isscp->locus);
        cache_token(cache, tok_continue, &pos);
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_StmtSort_Expression:
      { an_ifc_StmtSort_Expression isse, *issep;
        issep = get_StmtSort_Expression(&isse);
        cache_expr(cache, issep->expr);
        if (!(options & cso_no_final_semicolon)) {
          cache_token(cache, tok_semicolon, &null_source_position);
        }  /* if */
      }
      break;
    case ifc_StmtSort_Return:
      { an_ifc_StmtSort_Return issr, *issrp;
        issrp = get_StmtSort_Return(&issr);
        source_position_from_locus(&pos, &issrp->locus);
        cache_token(cache, tok_return, &pos);
        if (issrp->expr != 0) {
          cache_expr(cache, issrp->expr);
        }  /* if */
        cache_token(cache, tok_semicolon, &null_source_position);
      }
      break;
    case ifc_StmtSort_VariableDecl:
      { an_ifc_StmtSort_VariableDecl  issvd, *issvdp;
        issvdp = get_StmtSort_VariableDecl(&issvd);
        read_prechecked_partition_element(issvdp->decl);
        an_ifc_DeclSort_Variable  idsv, *idsvp;
        idsvp = get_DeclSort_Variable(&idsv);
        cache_variable_decl(cache, issvdp->decl, /*is_class_member=*/FALSE,
                            ifc_Access_None,
                            idsvp->specifiers, idsvp->traits, idsvp->alignment,
                            idsvp->type, idsvp->name, (ifc_TextOffset)0,
                            (ifc_ExprIndex)0,
                            /*issvdp->initializer*/(ifc_ExprIndex)0,
                            &idsvp->locus);
      }
      break;
    case ifc_StmtSort_Expansion:
      { an_ifc_StmtSort_Expansion isse;
        get_StmtSort_Expansion(&isse);
        issue_unsupported_node_diag("StmtSort::Expansion", &error_position);
      }
      break;
    case ifc_StmtSort_SyntaxTree:
      { an_ifc_StmtSort_SyntaxTree issst;
        get_StmtSort_SyntaxTree(&issst);
        issue_unsupported_node_diag("StmtSort::SyntaxTree", &error_position);
      }
      break;
    case ifc_StmtSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unknown StmtSort kind");
  }  /* switch */
}  /* cache_statement */


static a_diagnostic_ptr start_rp_diag(
                                   a_routine_ptr     rp,
                                   an_error_severity error_severity = es_error)
/*
Start a new IFC validation diagnostic for the given routine pointer's
associated function definition, with the given error severity.
*/
{
  return pos_st_start_diagnostic(error_severity,
                                 ec_ifc_bad_function_definition,
                                 &rp->source_corresp.decl_position,
                                 rp->source_corresp.name);
}  /* start_rp_diag */


static a_boolean should_perform_implicit_this_correction(
                                            a_routine_ptr    rp,
                                            unsigned         chart_param_count,
                                            a_param_type_ptr params)
/*
Check to see if the given routine pointer needs to have an implicit this
parameter dropped to be processed correctly (with respect to the current
parameter counts so if this bug is resolved we don't emit hard errors).  IFC
files appear to regularly misrepresent the "this" parameter as an ordinary
unnamed parameter.  Skip the implicit this parameter and issue a warning.
*/
{
  /* FIXME: Note that count_list_elements can be "expensive" if the function
     has many parameters.  This is considered acceptable for ease of
     implementation since this code is presumed to be temporary pending the
     removal of the implicit this parameter. */
  return routine_type_is_nonstatic_member_function(rp->type) &&
         (chart_param_count - 1 == count_list_elements(params));
}  /* should_perform_implicit_this_correction */


static void add_bad_parameter_count_info(a_diagnostic_ptr diag_ptr,
                                         unsigned         chart_param_count,
                                         unsigned         type_param_count)
/*
Add the corresponding diagnostic to the diagnostic pointer for mismatched IFC
parameter counts between the IFC chart's parameter count and the type's
parameter count.
*/
{
  an_error_code error_code;

  /* Make sure this is a diagnostic that needs issued. */
  check_assertion(chart_param_count != type_param_count);
  if (chart_param_count != 1 && type_param_count != 1) {
    error_code = ec_ifc_bad_function_param_counts_multi_multi;
  } else if (chart_param_count != 1) {
    error_code = ec_ifc_bad_function_param_counts_multi_single;
  } else {
    error_code = ec_ifc_bad_function_param_counts_single_multi;
  }  /* if */
  num2_add_diag_info(diag_ptr, error_code, chart_param_count,
                     type_param_count);
}  /* add_bad_parameter_count_info */


static a_boolean check_parameter_counts(a_routine_ptr    rp,
                                        unsigned         chart_param_count,
                                        a_param_type_ptr params)
/*
Check for a mismatch between the number of parameters declared by the IFC
parameter chart and the number of parameters declared by the type.  Return TRUE
if parameter counts match, return FALSE otherwise.
*/
{
  a_boolean result = TRUE;
  unsigned  type_param_count = count_list_elements(params);

  if (should_perform_implicit_this_correction(rp, chart_param_count, params)) {
    a_diagnostic_ptr diag_ptr = start_rp_diag(rp, es_warning);

    add_bad_parameter_count_info(diag_ptr, chart_param_count,
                                 type_param_count);
    /* The actual adjustment will be performed when the parameters are added to
       the function info (see "should_perform_implicit_this_correction" in
       "add_function_def_parameters"), but the issue is reported here for ease
       of implementation. */
    add_diag_info(diag_ptr, ec_ifc_bad_function_param_implicit_this);
    end_diagnostic(diag_ptr);
  } else if (chart_param_count != type_param_count) {
    a_diagnostic_ptr diag_ptr = start_rp_diag(rp);

    add_bad_parameter_count_info(diag_ptr, chart_param_count,
                                 type_param_count);
    end_diagnostic(diag_ptr);
    result = FALSE;
  }  /* if */
  return result;
}  /* check_parameter_counts */


static void add_function_def_parameter(
                         an_ifc_module                          *mod,
                         an_ifc_Node<an_ifc_DeclSort_Parameter> idsp,
                         a_param_type_ptr                       ptp,
                         a_func_info_block                      *func_info,
                         a_param_id_ptr                         *last_param_id)
/*
Add the given IFC parameter, using the associated module and associated
parameter type pointer, to the given function info.  last_param_id is a pointer
to the pointer for the latest addition to the parameter list; if said list is
empty, the pointed to pointer should be NULL.
*/
{
  a_source_position pos;

  check_assertion(idsp->sort == ifc_ParameterSort_Object);
  mod->source_position_from_locus(&pos, &idsp->locus);
  {
    a_const_char     *name = mod->get_string_at_offset(idsp->name);
    a_symbol_locator sym_loc;

    clear_locator(&sym_loc, &pos);
    (void)find_symbol(name, strlen(name), &sym_loc);
    add_to_param_id_list(&sym_loc,
                         make_qualified_type(ptp->type, ptp->qualifiers),
                         &pos, (a_storage_class)sc_auto, func_info,
                         (a_source_sequence_entry_ptr)NULL, last_param_id,
                         ptp->is_pack_element);
    /* Merge the parameter with the corresponding type information. */
    a_param_id_ptr new_param_id = *last_param_id;
    new_param_id->declared_type = ptp->type;
    new_param_id->param_num = ptp->param_num;
    if (ptp->is_pack_element) {
      new_param_id->is_pack_element = TRUE;
      if (ptp->is_parameter_pack) {
        new_param_id->is_parameter_pack = TRUE;
      }  /* if */
    }  /* if */
  }
}  /* add_function_def_parameter */


static a_boolean add_function_def_parameters(
                       an_ifc_module                                *mod,
                       an_ifc_Node<an_ifc_Trait_FunctionDefinition> itfd,
                       a_routine_ptr                                rp,
                       a_func_info_block                            *func_info)
/*
Add the parameters, for the given IFC function definition, associated module,
and associated routine pointer, to the given function info.  Return TRUE if all
parameters are added successfully, return FALSE otherwise.
*/
{
  a_boolean        result = TRUE;
  ifc_ChartSort    tag = chart_tag(itfd->parameters);
  a_param_type_ptr params = function_type_params(rp->type);

  /* Parameters are represented as a uni-level IFC "chart" pointing to a
     sequence of ifc_DeclSort_Parameter entries of kind
     ifc_ParameterSort_Object.  Check for the parameter chart. */
  if (tag == ifc_ChartSort_Unilevel) {
    Opt<an_ifc_Node<an_ifc_ChartSort_Unilevel>> opt_icsul;

    construct_node(&opt_icsul, mod, ifc_ChartSort_Unilevel,
                   chart_value(itfd->parameters));
    /* Read the uni-level chart of parameters. */
    if (!opt_icsul.has_value()) {
      result = FALSE;
      goto done;
    }  /* if */
    {
      an_ifc_Node<an_ifc_ChartSort_Unilevel> icsul = *opt_icsul;
      unsigned                               num_params = icsul->cardinality;

      if (!check_parameter_counts(rp, num_params, params)) {
        result = FALSE;
        goto done;
      }  /* if */
      {
        ifc_Index_type   idx = 0;
        a_param_type_ptr ptp = params;
        a_param_id_ptr   last_param_id = nullptr;

        /* Ensure a function prototype scope exists in which sk_parameter
           symbols can be accumulated. */
        (void)push_scope((a_scope_kind)sck_func_prototype, NO_SCOPE_NUMBER,
                         rp->type, (a_routine_ptr)NULL);
        if (should_perform_implicit_this_correction(rp, num_params, ptp)) {
          idx = 1;
        }  /* if */
        func_info->scope_number = scope_stack_top().number;
        for (ptp = params; idx < num_params; ++idx, ptp = ptp->next) {
          Opt<an_ifc_Node<an_ifc_DeclSort_Parameter>> opt_idsp;

          construct_node(&opt_idsp, mod, ifc_DeclSort_Parameter,
                         icsul->start + idx);
          if (opt_idsp.has_value()) {
            add_function_def_parameter(mod, *opt_idsp, ptp, func_info,
                                       &last_param_id);
          } else {
            result = FALSE;
            goto done;
          }  /* if */
        }  /* for */
        func_info->prototype_scope_symbols =
                          assoc_pointers_block_of(&scope_stack_top())->symbols;
        pop_scope();
      }
    }
  } else if (tag == ifc_ChartSort_None) {
    /* The associated chart is empty, verify the type information also isn't
       specifying parameters. */
    if (!check_parameter_counts(rp, 0, params)) {
      result = FALSE;
    }  /* if */
  } else {
    /* The associated chart index was set, but it wasn't a uni-level chart. */
    a_diagnostic_ptr diag_ptr = start_rp_diag(rp);

    add_diag_info(diag_ptr, ec_ifc_bad_function_param_wrong_chart);
    end_diagnostic(diag_ptr);
    result = FALSE;
  }  /* if */
done:
  return result;
}  /* add_function_def_parameters */


a_boolean an_ifc_module::cache_function_body(a_token_cache_ptr  cache,
                                             ifc_DeclIndex      decl_idx,
                                             a_routine_ptr      rp,
                                             a_func_info_block  *func_info)
/*
decl_idx points to the IFC representation of rp: That representation was
already loaded previously and found to be associated with a definition in
an ifc_trait_function_definition partition.  Load that IFC partition now and
process it.  As part of this processing, the parameter names of rp are also
loaded and *func_info is updated accordingly.  Normally, TRUE is returned, but
in some cases there is no definition present after all and FALSE is returned
instead.
*/
{
  a_boolean                                         result = TRUE;
  Opt<an_ifc_Node<an_ifc_Trait_FunctionDefinition>> opt_itfd;

  check_assertion(type_is(rp->type, tk_routine));
  find_trait<ifc_trait_function_definition>(&opt_itfd, this, decl_idx);
  if (opt_itfd.has_value()) {
    an_ifc_Node<an_ifc_Trait_FunctionDefinition> itfd = *opt_itfd;

    /* A definition exists, mark that. */
    func_info->is_definition = TRUE;
    /* Add the parameters to the function info. */
    if (!add_function_def_parameters(this, itfd, rp, func_info)) {
      result = FALSE;
      goto done;
    }  /* if */
    /* Cache the mem-initializers if needed. */
    if (itfd->initializers != 0) {
      cache_token(cache, tok_colon, &null_source_position);
      cache_expr(cache, itfd->initializers);
    }  /* if */
    /* Cache the function body.  It appears that a single return statement is
       represented directly rather than as a block containing the return
       statement.  We therefore generate the braces here and inhibit them at
       the next statement level by passing the cso_func_body flag. */
    cache_token(cache, tok_lbrace, &null_source_position);
    if (itfd->body != 0) {
      cache_statement(cache, itfd->body, cso_func_body);
    }  /* if */
    cache_token(cache, tok_rbrace, &null_source_position);
#if DEBUG
    if (db_flag_is_set("ms_ifc_token_def")) {
      fprintf(f_debug, "Function body cache:\n");
      db_tokens(cache);
      fprintf(f_debug, "\n---------------------\n");
    }  /* if */
#endif /* DEBUG */
  } else {
    /* The IFC told us there would be a definition but none was written. */
    pos_st_error(ec_ifc_missing_function_definition,
                 &rp->source_corresp.decl_position, rp->source_corresp.name);
    result = FALSE;
  }  /* if */
done:
  return result;
}  /* cache_function_body */


a_boolean load_routine_definition_from_ifc_module(a_routine_ptr  rp)
/*
If the given routine has a definition in a currently-imported IFC module
process that definition and return TRUE.
*/
{
  a_boolean             result = FALSE;
  an_ifc_function_body  ifb = ifc_function_bodies->get(rp);

  if (ifb.ifc_module != NULL) {
    a_func_info_block  func_info;
    a_token_cache      def_cache;
    a_decl_flag_set    flags = SFB_NEW_STRUCT_STMT_STACK_REQUIRED;
    a_curr_token_preserver
                       guard;
    /* We are about to load the definition.  So the "pending definition" entry
       can be dropped now. */
    ifc_function_bodies->unmap(rp);
    clear_token_cache(&def_cache, /*reusable=*/FALSE);
    clear_func_info(&func_info);
    push_new_top_level_declaration();
    if (ifb.ifc_module->cache_function_body(&def_cache, ifb.decl, rp,
                                            &func_info)) {
      rescan_cached_tokens(&def_cache);
      scan_function_body(rp, &func_info, flags);
      if (curr_token == tok_rbrace) {
        result = TRUE;
        get_token();
      }  /* if */
    }  /* if */
    pop_scope();
  }  /* if */
  return result;
}  /* load_routine_definition_from_ifc_module */


static void extract_matching_template_module_entities(
                                        a_module_entity_ptr  templ_mep,
                                        a_module_entity_ptr  *p_mep,
                                        a_module_entity_ptr  *p_templates,
                                        a_module_entity_ptr  *p_end_templates,
                                        a_module_entity_ptr  *p_partial_specs)
/*
*p_mep points to a possibly empty list of module entities that don't include
templ_mep, but that share its name.  Extract from the list all the templates
with a matching scope and set *p_templates to point to the list of extracted
templates and *p_end_templates to the last element on that list (or NULL if
none).  If p_partial_specs is non-NULL, also extract all the partial
specializations and set *p_partial_specs to point to those.
*/
{
  a_module_entity_ptr  other_decls = NULL, end_other_decls = NULL,
                       partial_specs = NULL;

  while (*p_mep != NULL) {
    a_module_entity_ptr  mep = *p_mep;
    /* Traverse the list of pending module entities, and select the ones whose
       scope matches that of *templ_mep.  Among those, process templates and
       partial specializations. */
    if (mep->scope == templ_mep->scope) {
      if (mep->variant.ifc_partition == ifc_decl_template) {
        /* A template: Move it to the other_decls list. */
        *p_mep = mep->next;
        mep->next = other_decls;
        other_decls = mep;
        if (end_other_decls == NULL) end_other_decls = mep;
      } else if (mep->variant.ifc_partition ==
                                            ifc_decl_partial_specialization &&
                 p_partial_specs != NULL) {
        /* A partial specialization: Move it to the partial_specs list. */
        *p_mep = mep->next;
        mep->next = partial_specs;
        partial_specs = mep;
      } else {
        /* Anything else: Leave it on the list of pending module entities. */
        p_mep = &mep->next;
      }  /* if */
    } else {
      /* An entity from a different scope: Leave it on the list of pending
         module entities. */
      p_mep = &mep->next;
    }  /* if */
  }  /* while */
  *p_templates = other_decls;
  *p_end_templates = end_other_decls;
  if (p_partial_specs != NULL) *p_partial_specs = partial_specs;
}  /* extract_matching_template_module_entities */


a_boolean an_ifc_module::process_template_definition(
                                   a_module_entity_ptr       mep,
                                   an_ifc_DeclSort_Template  *idstp,
                                   a_boolean                 already_declared,
                                   a_boolean                 is_func_template)
/*
The given module entity pointer describes a template stored in an IFC module
file.  If already_declared is TRUE, a declaration has previously been
processed.  However, its definition, its explicit specializations, and/or
partial specializations may still be stored in the IFC module.  Load those
elements into the IL (usually, to enable instantiation).  is_func_template
is TRUE if this is called for a function template.  idstp points to the
template's IFC description structure.
*/
{
  a_boolean                 result = FALSE;
  a_module_entity_ptr       other_decls = NULL, end_other_decls = NULL,
                            partial_specs = NULL;
  an_ifc_module             *ifc_mod = (an_ifc_module*)mep->module_info
                                                          ->module_interface;

  check_assertion(ifc_mod != NULL);
  if (!is_func_template) {
    /* There may be other declarations of this same template from other
       modules.  Temporarily remove these because declaration processing will
       look for a redeclaration and trigger a second nested processing of the
       template otherwise.  Also separate out partial specializations: They
       must be processed now since they can affect which template should be
       instantiated. */
    extract_matching_template_module_entities(mep, &mep->next,
                                              &other_decls, &end_other_decls,
                                              &partial_specs);
  }  /* if */
  if (idstp->entity.body != 0 &&
      idstp->properties & ifc_ReachableProperties_Initializer) {
    /* The template has a reachable definition (IFC files sometimes include
       definitions even when they are not reachable): Load it. */
    a_template_ptr  templ;
    a_token_cache   cache;
    a_boolean       saved_suppress_default_arguments =
                                          ifc_mod->suppress_default_arguments;
    ifc_mod->suppress_default_arguments = already_declared;
    clear_token_cache(&cache, /*reusable=*/FALSE);
    ifc_mod->cache_decl_template(&cache, idstp);
    terminate_token_cache(&cache);
    ifc_mod->suppress_default_arguments = saved_suppress_default_arguments;
    templ = parse_cached_template(&cache, mep->scope);
    mep->entity.ptr = (char*)templ;
    mep->entity.kind = iek_template;
    result = TRUE;
    if (templ != NULL && templ->kind == (a_template_kind)templk_class) {
      /* For a class template, check if it has any associated deduction guides.
         Deduction guides are associated to the template through the IFC traits
         mechanism. */
      an_ifc_module  *itf = (an_ifc_module*)mep->module_info->module_interface;
      ifc_DeclIndex  decl = itf->decl_index_of(mep);
      Opt<an_ifc_Node<an_ifc_Trait_DeductionGuides>>
                     opt_itdg;
      find_trait<ifc_trait_deduction_guides>(&opt_itdg, itf, decl);
      if (opt_itdg.has_value()) {
        ifc_DeclIndex        guides_idx = (*opt_itdg)->trait;
        a_module_entity_ptr  guides_mep =
                                  itf->get_ifc_module_entity_ptr(guides_idx);
        /* A single guide will have an ifc_DeclSort_Template entry directly
           associated with it, but it doesn't record the parent scope: So set
           it here (a guide is required to be declared in the same scope as the
           class template).  If there are multiple guides, guides_mep will
           be for an ifc_DeclSort_Tuple entry instead (and its treatment will
           propagate the parent scope). */
        guides_mep->scope = templ->source_corresp.parent_scope;
        itf->process_ifc_declaration(guides_mep, /*defer=*/FALSE,
                                      (a_type*)NULL);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Compute the DeclIndex of the current template and retrieve the sequence
     of explicit specializations and instantiations. */
  ifc_DeclIndex decl = ifc_mod->decl_index_of(mep);
  ifc_Sequence  seq = ifc_mod->get_specialization_sequence_from_trait(decl);
  if (seq.cardinality != 0) {
    /* Process the sequence of explicit specializations and explicit
       instantiations. */
    ifc_mod->process_scope_member_sequence(seq);
    result = TRUE;
  }  /* if */
  /* Now process the partial specializations. */
  while (partial_specs != NULL) {
    a_module_entity_ptr ps_mep = partial_specs;
    an_ifc_module       *itf = (an_ifc_module*)
                                        ps_mep->module_info->module_interface;
    partial_specs = ps_mep->next;
    ps_mep->next = NULL;
    ps_mep->imminent = FALSE;
    itf->process_ifc_declaration(ps_mep, /*defer=*/FALSE, (a_type_ptr)NULL);
    result = TRUE;
  }  /* while */
  return result;
}  /* process_template_definition */


a_boolean load_template_definition_from_ifc_module(a_template_ptr  templ)
/*
The given template entry represents a template that was loaded from an IFC
module, but its definition hasn't been loaded yet.  Load the definition now.
*/
{
  a_boolean            result = FALSE;
  a_module_entity_ptr  mep = templ->source_corresp.module_entity;

  check_assertion(mep != NULL);
  if (mep->variant.ifc_partition != ifc_decl_template) {
    /* This should never happen.  If it does, the most likely culprit is that
       curr_module_entity was not correctly saved/cleared/restored around the
       context that created the templ entry. */
    unexpected_condition();
  } else {
    an_ifc_module  *ifc_mod = (an_ifc_module*)mep->module_info
                                                 ->module_interface;
    if (ifc_mod->read_partition_element(mep)) {
      a_source_position         saved_error_position = error_position;
      a_module_entity_ptr       saved_mep = curr_module_entity;
      an_ifc_DeclSort_Template  idst, *idstp;
      a_curr_token_preserver    guard;
      a_boolean                 scope_pushed;
      curr_module_entity = mep;
      scope_pushed = push_module_declaration_context(mep->scope);
      idstp = ifc_mod->get_DeclSort_Template(&idst);
      result = an_ifc_module::process_template_definition(
                             templ->source_corresp.module_entity,
                             idstp,
                             /*already_declared=*/TRUE,
                             templ->kind == (a_template_kind)templk_function);
      pop_module_declaration_context(scope_pushed);
      curr_module_entity = saved_mep;
      error_position = saved_error_position;
    }  /* if */
  }  /* if */
  return result;
}  /* load_template_definition_from_ifc_module */


static inline a_boolean ifc_decl_is_ignorable_redecl(
                                         a_symbol_locator      *loc,
                                         a_module_entity_ptr   mep,
                                         a_source_position_ptr pos,
                                         an_il_entry_kind      expected_kind,
                                         char                  **redecl_entity,
                                         a_byte_il_entry_kind  *redecl_kind)
/*
An entity described in a compiled module file is considered for loading into
the IL.  loc describes the name of that entity, mep its location and early
characterization in the compiled module file, pos the position in the
source file that was compiled, and expected_kind the kind of IL entity that
this appears to be.  If the entity appears to redeclare an existing IL entity,
return TRUE and set *redecl_entity and *redecl_kind to describe the entity
already in the IL.
*/
{
  a_boolean    result = FALSE;
  a_symbol_ptr redecl_sym;

  redecl_sym = check_module_symbol_redecl(loc->symbol_header, mep, pos,
                                          expected_kind);
  if (redecl_sym != NULL) {
    /* This is a redeclaration of an existing symbol. */
    an_il_entry_kind this_kind;
    *redecl_entity = il_entry_for_symbol(redecl_sym, &this_kind);
    *redecl_kind = this_kind;
    result = TRUE;
  }  /* if */
  return result;
}  /* ifc_decl_is_ignorable_redecl */


/* FIXME: might be able to get rid of enumeration_type now that enums aren't
   deferred */
void an_ifc_module::process_ifc_declaration(
                                          a_module_entity_ptr mep,
                                          a_boolean           defer,
                                          a_type_ptr          enumeration_type)
/*
Process the IFC module entity declaration specified by mep either by creating
the appropriate IL entity, or, when defer is TRUE, mark the appropriate
symbol header as having a deferred module entity (which will be lazily loaded
if referenced).  When enumeration_type is non-NULL, it represents the
enumeration type for the enumerator being defined (and is added to the list of
constants for that type).  If defer is FALSE, *mep is updated to record the
principal associated IL entity.
*/
{
  ifc_DeclSort             tag;
  a_decl_parse_state       dps;
  an_id_linkage_kind       linkage_ptr;
  a_symbol_ptr             ext_sym;
  a_decl_pos_block         decl_pos_block;
  a_symbol_locator         loc;
  a_partial_scope_stack_state
                           psss;
  char                     *il_entity = NULL;
  a_byte_il_entry_kind     kind = iek_none;
  a_boolean                scope_pushed = FALSE;
  a_boolean                skip_pop = FALSE;
  a_diagnostic_suppression diag_suppress(&this->suppressed_diagnostics,
                                         !display_module_import_diagnostics);
  Value_saver<a_boolean>   checking_pragma_saver(&no_checking_pragmas, TRUE);

  /* Ensure the module entity is being processed by the corresponding module
     interface. */
  check_assertion(mep->module_info->module_interface == this);
#if EXPENSIVE_CHECKING
  /* When this flag is set, eagerly load all entities in a module.  Entities
     are typically lazily loaded (i.e., only when needed) and eagerly loading
     all entities in a module can be used as a debugging aid to ensure that all
     entities load properly.

     As this feature is not supported in production builds and this branch is
     in an anticipated hot path, conditionally enable it with
     EXPENSIVE_CHECKING as an optimization. */
  if (eager_load_modules) {
    defer = FALSE;
  }  /* if */
#endif /* EXPENSIVE_CHECKING */
#if DEBUG
  static unsigned long nested_decls = 0;
  if (db_flag_is_set("ifc_decl")) {
    (void)fprintf(f_debug, "[>%lu]%s ", nested_decls++,
                                        defer ? " (deferred)" : "");
    db_mep(mep);
  }  /* if */
#endif /* DEBUG */
  if (!mep->imminent && mep->entity.ptr == NULL) {
    a_source_position   saved_error_position = error_position;
    a_module_entity_ptr saved_mep = curr_module_entity;
    curr_module_entity = mep;
    /* Read from the proper partition for this declaration. */
    {
      a_boolean read_result;

      /* Perform a shallow read if this is a deferral (to minimize validation
         operations for entities that may never be used), otherwise do a full
         read for entity processing. */
      if (defer) {
        read_result = read_partition_element_shallow(mep);
      } else {
        read_result = read_partition_element(mep);
      }  /* if */
      /* If the read failed, immediately skip to an invalid state. */
      if (!read_result) {
        skip_pop = TRUE;
        goto invalid;
      }  /* if */
    }
    tag = (ifc_DeclSort)get_tag_from_partition(mep->variant.ifc_partition,
                                               ifc_decl_start);
    if (!defer) {
      mep->imminent = TRUE;
      if (mep->scope != NULL) {
        /* If this module entity has a scope, re-activate it now (note that
           if it's already activated, scope_pushed will be FALSE).  If it
           does not already have a scope, one may be created below. */
        scope_pushed = push_module_declaration_context(mep->scope);
      }  /* if */
    }  /* if */
    switch (tag) {
      case ifc_DeclSort_VendorExtension:
        { an_ifc_DeclSort_VendorExtension idsve;
          get_DeclSort_VendorExtension(&idsve);
          /* FIXME: Need a proper source position for this. */
          error_position = null_source_position;
          issue_unsupported_node_diag(str_for_decl_tag(tag), &error_position);
          il_entity = (char *)error_type();
          kind = iek_type;
        }
        break;
      case ifc_DeclSort_Variable:
        { an_ifc_DeclSort_Variable idsv, *idsvp;
          idsvp = get_DeclSort_Variable(&idsv);
          if (!init_decl_locator(idsvp, &loc)) {
            goto invalid;
          }  /* if */
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_token_cache  cache;
            if (is_from_gmf(idsvp->specifiers)) {
              mep->global_module = TRUE;
            }  /* if */
            if (mep->scope == NULL) {
              mep->scope = get_ifc_home_scope(idsvp);
              scope_pushed = push_module_declaration_context(mep->scope);
            }  /* if */
            if (ifc_decl_is_ignorable_redecl(&loc, mep, &error_position,
                                             iek_variable, &il_entity,
                                             &kind)) {
              break;
            }  /* if */
            init_decl_parse_state(&dps);
            /* Naming aside, setting this is required to allow the inline
               keyword on variable declarations. */
            dps.function_definition_allowed = TRUE;
            clear_token_cache(&cache, /*reusable=*/FALSE);
            cache_decl(&cache, decl_index_of(mep));
            terminate_token_cache(&cache);
#if DEBUG
            if (db_flag_is_set("ms_ifc_token_def")) {
              fprintf(f_debug, "Reconstituted variable declaration:\n");
              db_tokens(&cache);
              fprintf(f_debug, "\n---------------------\n");
            }  /* if */
#endif /* DEBUG */
            rescan_cached_tokens(&cache);
            scan_nonmember_declaration(&dps, /*a_source_range=*/NULL);
            check_assertion(curr_token == tok_end_of_source);
            (void)get_token();
            check_assertion(dps.sym != NULL &&
                            symbol_is(dps.sym, sk_variable));
            il_entity = (char *)dps.sym->variant.variable.ptr;
            kind = iek_variable;
          }  /* if */
        }
        break;
      case ifc_DeclSort_Function:
        { a_func_info_block        func_info;
          a_type_ptr               old_type;
          an_ifc_DeclSort_Function idsf, *idsfp;
          idsfp = get_DeclSort_Function(&idsf);
          if (!init_decl_locator(idsfp, &loc)) {
            goto invalid;
          }  /* if */
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here. */
            a_routine_ptr rp;
            /* FIXME: There's a chicken-and-egg problem here when the return
               type is deduced and requires access to the class scope (e.g.,
               returning a lambda declared within the function). */
            if (mep->scope == NULL) {
              mep->scope = get_ifc_home_scope(idsfp);
              scope_pushed = push_module_declaration_context(mep->scope);
            }  /* if */
            init_dps(&dps, &idsfp->locus, idsfp->type, ifc_ObjectTraits_None,
                     ifc_MsvcTraits_None, idsfp->specifiers, idsfp->access,
                     (ifc_ExprIndex)0, &psss);
            if (idsfp->traits & ifc_FunctionTraits_Immediate) {
              dps.dso_flags |= DSO_CONSTEVAL;
            } else if (idsfp->traits & ifc_FunctionTraits_Constexpr) {
              dps.dso_flags |= DSO_CONSTEXPR;
            } else if (idsfp->traits & ifc_FunctionTraits_Inline) {
              dps.dso_flags |= DSO_INLINE;
            }  /* if */
            clear_func_info(&func_info);
            clear_decl_pos_block(&decl_pos_block);
            decl_routine(&loc, &dps, &func_info, SRK_DECLARATION, &linkage_ptr,
                         &old_type, &ext_sym, &decl_pos_block);
            restore_partial_scope_stack_if_necessary(&psss);
            rp = dps.sym->variant.routine.ptr;
            il_entity = (char *)rp;
            kind = iek_routine;
            if (idsfp->properties & ifc_ReachableProperties_Initializer) {
              /* A body is available: Record this availability in case it is
                 needed. */
              ifc_DeclIndex decl_idx = decl_index_of(mep);
              record_pending_ifc_function_body(rp, decl_idx, this);
            }  /* if */
          }  /* if */
        }
        break;
      case ifc_DeclSort_Intrinsic:
        /* A builtin function declaration. */
        { a_func_info_block        func_info;
          a_type_ptr               old_type;
          a_routine_ptr            rp;
          an_ifc_DeclSort_Intrinsic idsi, *idsip;
          idsip = get_DeclSort_Intrinsic(&idsi);
          if (!init_decl_locator(idsip, &loc)) {
            goto invalid;
          }  /* if */
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here (just copied
               ifc_DeclSort_Function).*/
            init_dps(&dps, &idsip->locus, idsip->type, ifc_ObjectTraits_None,
                     ifc_MsvcTraits_None, idsip->specifiers, idsip->access,
                     (ifc_ExprIndex)0, &psss);
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
      case ifc_DeclSort_Scope:
        /* A DeclSort::Scope, which indicates a namespace or a
           class/struct/union.  Note that although these declare "scopes",
           the IL entity that is attached to them is either a namespace or
           a type. */
        { an_ifc_DeclSort_Scope       idss, *idssp;
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          a_type_kind                 type_kind;
          a_symbol_kind               tag_kind;

          idssp = get_DeclSort_Scope(&idss);
          if (!init_decl_locator(idssp, &loc)) {
            goto invalid;
          }  /* if */
          /* Should be no unnamed namespaces or types. */
          /* FIXME: This should be a soft failure. */
          check_assertion(idssp->name != 0);
          if (is_from_gmf(idssp->specifiers)) {
            mep->global_module = TRUE;
          }  /* if */
          if (mep->scope == NULL) {
            mep->scope = get_ifc_home_scope(idssp);
            scope_pushed = push_module_declaration_context(mep->scope);
          }  /* if */
          if (scope_is(mep->scope, sck_class_struct_union)) {
            /* This is a child class.  It will have already been entered as a
               member of the parent class.  Find that entry and skip the rest
               of the processing. */
            a_type_ptr   parent_class = mep->scope->variant.assoc_type;
            a_symbol_ptr sym;
            sym = look_up_name_string_in_class(loc.symbol_header->identifier,
                                               parent_class,
                                               IDL_TENTATIVE_TYPE_LOOKUP |
                                                              IDL_MUST_BE_TAG);
            check_assertion(sym != NULL && is_class_struct_union_symbol(sym));
            il_entity = (char*)(sym->variant.class_struct_union.type);
            kind = iek_type;
            break;
          }  /* if */
          if (!read_partition_element(idssp->type)) {
            goto invalid;
          }  /* if */
          /* Look at the "type" to determine whether we have a namespace or
             not. */
          /* FIXME: This should be a soft failure. */
          check_assertion(type_tag(idssp->type) == ifc_TypeSort_Fundamental);
          itsfp = get_TypeSort_Fundamental(&itsf);
          switch (itsfp->basis) {
            case ifc_TypeBasis_Namespace:
              { a_namespace_ptr nsp = NULL;
                a_symbol_ptr    ns_sym;
                if (defer && !(idssp->traits & ifc_ScopeTraits_Inline)) {
                  /* Note that inline namespaces are not deferred. */
                  defer_symbol_creation(mep, &loc);
                } else {
                  /* FIXME: lots missing. */
                  /* FIXME: This should probably call
                     push/pop_module_declaration_context on itself, rather than
                     managing the namespace scopes through an alternative code
                     path. */
                  check_assertion((idssp->traits & ifc_ScopeTraits_Unnamed)
                                  == 0);
                  { Value_saver<a_boolean> lazy_load_saver(
                                                  &lazy_symbols_may_be_visible,
                                                  /*new_value=*/FALSE);
                    ns_sym = curr_scope_id_lookup(&loc, IDL_NO_OPTIONS);
                  }
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
                    if (idssp->traits & ifc_ScopeTraits_Inline) {
                      nsp->is_inline = TRUE;
                    }  /* if */
                    ns_sym->variant.namespace_info.ptr = nsp;
                    add_to_namespaces_list(nsp);
                    (void)push_namespace_scope((a_scope_kind)sck_namespace,
                                               nsp);
                    nsp->variant.assoc_scope->variant.assoc_namespace = nsp;
                    if (nsp->is_inline) {
                      add_implicit_using_directive(nsp, /*is_inline=*/TRUE,
                                                   /*namespace_pushed=*/TRUE);
                    }  /* if */
                  }  /* if */
                  /* Forward assign the module entity pointer's information
                     so we can self-reference. */
                  mep->entity.ptr = il_entity = (char *)nsp;
                  mep->entity.kind = kind = iek_namespace;
                  /* Process declarations in the namespace scope (which makes
                     their symbols available but not their definitions). */
                  process_ifc_scope(idssp->initializer,
                                    nsp->variant.assoc_scope);
                  pop_namespace_scope();
                }  /* if */
              }
              break;
            case ifc_TypeBasis_Class:
              type_kind = tk_class;
              tag_kind = sk_class_or_struct_tag;
              goto class_struct_union_case;
            case ifc_TypeBasis_Struct:
            case ifc_TypeBasis_Interface:
              type_kind = tk_struct;
              tag_kind = sk_class_or_struct_tag;
              goto class_struct_union_case;
            case ifc_TypeBasis_Union:
              type_kind = tk_union;
              tag_kind = sk_union_tag;
class_struct_union_case:
              /* A class/struct/union type. */
              { a_type_ptr    class_type;
                a_symbol_ptr  tag_sym;
                if (defer) {
                  defer_symbol_creation(mep, &loc);
                } else {
                  /* Allocate the appropriate class type, but leave it as
                     incomplete.  The class will be completed during a call to
                     get_definition_of_class if it is referenced. */
                  if (ifc_decl_is_ignorable_redecl(&loc, mep,
                                                   &error_position, iek_type,
                                                   &il_entity, &kind)) {
                    break;
                  }  /* if */
                  class_type = alloc_type(type_kind);
                  if (itsfp->basis == ifc_TypeBasis_Interface) {
                    class_type->variant.class_struct_union.is_interface = TRUE;
                    class_type->variant.class_struct_union.abstract = TRUE;
                  }  /* if */
                  tag_sym = enter_local_symbol(
                                             tag_kind, &loc,
                                             mep->scope->depth_in_scope_stack,
                                             /*suppress_redecl_error=*/TRUE);
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
                  /* Mark the class as incomplete.  It can be completed later
                     if needed. */
                  class_type->incomplete = TRUE;
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
      case ifc_DeclSort_Alias:
        { an_ifc_DeclSort_Alias idsta, *idstap;
          idstap = get_DeclSort_Alias(&idsta);
          if (!init_decl_locator(idstap, &loc)) {
            goto invalid;
          }  /* if */
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            if (is_from_gmf(idstap->specifiers)) {
              mep->global_module = TRUE;
            }  /* if */
            ifc_TypeSort alias_tag = type_tag(idstap->type);
            if (alias_tag == ifc_TypeSort_Fundamental) {
              an_ifc_TypeSort_Fundamental itsf, *itsfp;
              /* Read the type to see what kind it is. */
              read_prechecked_partition_element(idstap->type);
              itsfp = get_TypeSort_Fundamental(&itsf);
              if (itsfp->basis == ifc_TypeBasis_Typename) {
                /* A type alias; declare a typedef for this case. */
                if (mep->scope == NULL) {
                  mep->scope = get_ifc_home_scope(idstap);
                  scope_pushed = push_module_declaration_context(mep->scope);
                }  /* if */
                if (ifc_decl_is_ignorable_redecl(&loc, mep, &error_position,
                                                 iek_type, &il_entity,
                                                 &kind)) {
                  break;
                }  /* if */
                init_dps(&dps, &idstap->locus, idstap->aliasee,
                         ifc_ObjectTraits_None, ifc_MsvcTraits_None,
                         idstap->specifiers, idstap->access, (ifc_ExprIndex)0,
                         &psss);
                clear_decl_pos_block(&decl_pos_block);
                decl_typedef(&loc, &dps, (a_type_ptr)NULL, &decl_pos_block);
                restore_partial_scope_stack_if_necessary(&psss);
                il_entity = (char *)dps.sym->variant.type.ptr;
                kind = iek_type;
              } else if (itsfp->basis == ifc_TypeBasis_Namespace) {
                /* A namespace alias. */
                /* FIXME: unimplemented. */
                issue_unsupported_node_diag("DeclSort::Alias namespace",
                                            &error_position);
                il_entity = (char *)error_type();
                kind = iek_type;
              } else {
                unexpected_condition();
              }  /* if */
            } else if (alias_tag == ifc_TypeSort_Forall) {
              an_ifc_TypeSort_Forall  itsf, *itsfp;
              a_token_cache           cache;
              a_source_position       pos;
              a_curr_token_preserver  guard;
              read_prechecked_partition_element(idstap->aliasee);
              itsfp = get_TypeSort_Forall(&itsf);
              source_position_from_locus(&pos, &idstap->locus);
              /* An alias template; declare a typedef for this case. */
              if (mep->scope == NULL) {
                mep->scope = get_ifc_home_scope(idstap);
                scope_pushed = push_module_declaration_context(mep->scope);
              }  /* if */
              if (ifc_decl_is_ignorable_redecl(
                                           &loc, mep, &error_position,
                                           iek_template, &il_entity, &kind)) {
                break;
              }  /* if */
              clear_token_cache(&cache, /*reusable=*/FALSE);
              cache_token(&cache, tok_template, &pos);
              cache_chart(&cache, itsfp->chart, &idstap->locus);
              cache_token(&cache, tok_using, &pos);
              cache_identifier(&cache, get_string_at_offset(idstap->name),
                               &pos);
              cache_token(&cache, tok_assign, &pos);
              cache_type(&cache, itsfp->subject, &idstap->locus);
              cache_token(&cache, tok_semicolon, &pos);
              terminate_token_cache(&cache);
              il_entity = (char*)parse_cached_template(&cache, mep->scope);
              kind = iek_template;
            } else {
              unexpected_condition();
            }  /* if */
          }  /* if */
        }
        break;
      case ifc_DeclSort_Enumeration:
        { an_ifc_DeclSort_Enumeration    idse, *idsep;
          an_ifc_TypeSort_Fundamental    itsf, *itsfp;
          a_boolean                      is_scoped_enum = FALSE;
          a_scope_ptr                    enum_scope;
          Opt<an_ifc_Ref<ifc_NameIndex>> opt_idsep_name_ref;

          idsep = get_DeclSort_Enumeration(&idse);
          /* FIXME: This does a lot of stuff even when deferred. */
          if (!source_position_from_locus(&error_position, &idsep->locus)) {
            goto invalid;
          }  /* if */
          /* FIXME: This should be a soft failure. */
          check_assertion(type_tag(idsep->type) == ifc_TypeSort_Fundamental);
          if (is_from_gmf(idsep->specifiers)) {
            mep->global_module = TRUE;
          }  /* if */
          /* See if this is a scoped enumeration or not. */
          if (!read_partition_element(ifc_type_fundamental,
                                      type_value(idsep->type))) {
            goto invalid;
          }  /* if */
          itsfp = get_TypeSort_Fundamental(&itsf);
          if (itsfp->basis == ifc_TypeBasis_Enum) {
            /* A classic enumeration.  Don't bother to defer in this case
               because each of the enumerators needs to be registered in the
               symbol table so they can be found. */
            if (defer) {
              defer = FALSE;
              mep->imminent = TRUE;
              scope_pushed = push_module_declaration_context(mep->scope);
            }  /* if */
          } else if (itsfp->basis == ifc_TypeBasis_Class ||
                     itsfp->basis == ifc_TypeBasis_Struct) {
            /* A scoped enumeration.  Enumerator definitions are deferred when
               the enumeration definition is deferred (enumerators can't be
               referred to without specifying the scoped enumeration at which
               time the enumerators will be made available). */
            is_scoped_enum = TRUE;
          } else {
            unexpected_condition();
          }  /* if */
          opt_idsep_name_ref = get_ifc_name(idsep);
          if (!opt_idsep_name_ref.has_value()) {
            goto invalid;
          }  /* if */
          if (!init_locator_from_name(*opt_idsep_name_ref, &idsep->locus,
                                      &loc)) {
            goto invalid;
          }  /* if */
          /* FIXME: This should be a soft failure. */
          check_assertion(idsep->name != 0);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_type_ptr   enum_type;
            a_symbol_ptr tag_sym;
            check_assertion(idsep->base != 0);
            if (mep->scope == NULL) {
              mep->scope = get_ifc_home_scope(idsep);
              scope_pushed = push_module_declaration_context(mep->scope);
            }  /* if */
            check_assertion(mep->scope != NULL);
            enum_scope = mep->scope;
            if (ifc_decl_is_ignorable_redecl(&loc, mep, &error_position,
                                             iek_type, &il_entity, &kind)) {
              break;
            }  /* if */
            init_dps(&dps, &idsep->locus, idsep->base, ifc_ObjectTraits_None,
                     ifc_MsvcTraits_None, idsep->specifiers, idsep->access,
                     idsep->alignment, &psss);
            clear_decl_pos_block(&decl_pos_block);
            /* Allocate an integer type and set its size based on the type
               specified by idsep->base. */
            enum_type = alloc_type((a_type_kind)tk_integer);
            enum_type->variant.integer.int_kind = (an_integer_kind)ik_int;
            enum_type->variant.integer.enum_type = TRUE;
            enum_type->size = skip_typerefs(dps.type)->size;
            enum_type->variant.integer.has_explicit_enum_base = TRUE;
            integer_type_supp(enum_type)->base_type = dps.type;
            if (dps.alignment == 0) {
              /* Alignment was not specifically specified; use the alignment
                 of the base type. */
              dps.alignment = skip_typerefs(dps.type)->alignment;
            }  /* if */
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
                emep = get_ifc_module_entity_ptr(make_decl_index(
                                                ifc_DeclSort_Enumerator,
                                                idsep->initializer.start + i));
                emep->scope = enum_scope;
                this->process_ifc_declaration(emep, /*defer=*/FALSE,
                                              enum_type);
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
      case ifc_DeclSort_Enumerator:
        { an_ifc_DeclSort_Enumerator idse, *idsep;
          idsep = get_DeclSort_Enumerator(&idse);
          if (!init_decl_locator(idsep, &loc)) {
            goto invalid;
          }  /* if */
          /* FIXME: This should be a soft failure. */
          check_assertion(idsep->name != 0);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_symbol_ptr   enum_con_sym;
            a_constant_ptr enum_con;
            a_type_ptr     enum_type;
            if (is_from_gmf(idsep->specifiers)) {
              mep->global_module = TRUE;
            }  /* if */
            check_assertion(mep->scope != NULL);
            if (mep->scope->kind == (a_scope_kind)sck_enum) {
              /* This is an enumerator for a scoped enum. */
              enum_type = mep->scope->variant.assoc_type;
            } else {
              check_assertion(enumeration_type != NULL);
              enum_type = enumeration_type;
            }  /* if */
            if (ifc_decl_is_ignorable_redecl(&loc, mep, &error_position,
                                             iek_constant, &il_entity, &kind)){
              break;
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
      case ifc_DeclSort_Template:
        { an_ifc_DeclSort_Template idst, *idstp;
          a_boolean                is_deduction_guide = FALSE;
          idstp = get_DeclSort_Template(&idst);
          if (!init_decl_locator(idstp, &loc)) {
            goto invalid;
          }  /* if */
          if (idstp->type == 0) {
            /* Deduction guide templates have no associated type.  They are
               driven by the IFC "traits" system instead of by lookup (which
               means they are never "deferred"). */
            is_deduction_guide = TRUE;
            check_assertion(!defer);
          }  /* if */
          /* FIXME: This should be a soft failure. */
          check_assertion(idstp->name != 0 || is_deduction_guide);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_token_cache           cache;
            a_non_type_kind         nt_kind;
            a_type_ptr              type;
            a_boolean               do_forward_decl, delay_definition = FALSE;
            a_boolean               saved_suppress_default_arguments;
            a_module_entity_ptr     other_decls = NULL, end_other_decls = NULL;
            a_curr_token_preserver  guard;
            if (idstp->properties & ifc_ReachableProperties_Initializer) {
              mep->has_definition = TRUE;
            }  /* if */
            if (is_from_gmf(idstp->specifiers)) {
              mep->global_module = TRUE;
            }  /* if */
            if (mep->scope == NULL) {
              mep->scope = get_ifc_home_scope(idstp);
              scope_pushed = push_module_declaration_context(mep->scope);
            }  /* if */
            type = is_deduction_guide ?
                                   NULL :
                                   type_for_type_index(idstp->type, &nt_kind);
            if (!is_deduction_guide && !type_is(type, tk_routine)) {
              /* Do not call ifc_decl_is_ignorable_redecl here for function
                 templates since they can be overloaded. */
              if (ifc_decl_is_ignorable_redecl(&loc, mep, &error_position,
                                               iek_template, &il_entity,
                                               &kind)) {
                break;
              }  /* if */
              /* There may be other declarations of this same template from
                 other modules.  Temporarily remove these because declaration
                 processing will look for a redeclaration and trigger a second
                 nested processing of the template otherwise. */
              extract_matching_template_module_entities(
                              mep, &mep->next, &other_decls, &end_other_decls,
                              (a_module_entity**)NULL);
            }  /* if */
            /* It's possible for a template body to refer to itself (or to
               another entity that refers back to it) and trigger a recursive
               attempt to recreate this entity.  This can be solved with
               forward declarations (which most certainly had to exist in the
               original code).  However, non-external linkage variable
               templates cannot have both a forward declaration and a
               definition, so we cannot put out a forward declaration always.
               FIXME: This should be done in one pass instead: The template
               declaration should be processed, then the body (if needed)
               without re-parsing the declaration.  The routines in templates.c
               might need refactoring to accommodate that.
            */
            do_forward_decl = (type != type_of_unknown_templ_param_nontype ||
                               idstp->entity.body == 0);
            saved_suppress_default_arguments = suppress_default_arguments;
            if (do_forward_decl && idstp->entity.body != 0 &&
                sentence_is_deleted(idstp->entity.body)) {
              /* If this is an "= delete" definition, do not issue a "forward
                 declaration" (i.e., without "= delete") since that would be
                 invalid.  Such definitions will have the "Initializer"
                 property set.  Variable templates can also have that property,
                 but they do not need "forward declarations" either. */
              do_forward_decl = FALSE;
            }  /* if */
            if (do_forward_decl) {
              suppress_default_arguments = FALSE;
              clear_token_cache(&cache, /*reusable=*/FALSE);
              cache_decl_template_declaration(&cache, idstp,
                                              /*add_semicolon=*/TRUE);
              terminate_token_cache(&cache);
              suppress_default_arguments = saved_suppress_default_arguments;
              il_entity = (char*)parse_cached_template(&cache, mep->scope);
              kind = iek_template;
              if (is_file_or_namespace_scope(mep->scope)) {
                /* For namespace-scope entities, delay the definition until
                   it is actually needed.  FIXME: It would be good to also
                   delay the definition of member templates, but that is
                   currently more difficult to do. */
                delay_definition = TRUE;
              }  /* if */
            }  /* if */
            if (delay_definition) {
              /* There is a definition of the template, but we delay its
                 processing until it's really needed (i.e., the template is
                 instantiated). */
              check_assertion(il_entity != NULL);
            } else {
              /* There is a definition of the template.  Record the resolution
                 of the signature immediately so that the below processing
                 of the definition has access to it.  If we provided a forward
                 declaration of the entity, we will need to suppress any
                 default arguments on the definition. */
              if (do_forward_decl) {
                mep->entity.ptr = il_entity;
                mep->entity.kind = kind;
              }  /* if */
              process_template_definition(mep, idstp, do_forward_decl,
                                          type_is(type, tk_routine));
            }  /* if */
          }  /* if */
        }
        break;
      case ifc_DeclSort_Parameter:
        { an_ifc_DeclSort_Parameter idsp, *idspp;
          a_type_ptr                param_type;
          a_template_param_ptr      param = NULL, *next_param;
          a_template_parameter_ptr  il_param = NULL;
          a_boolean                 is_pack = FALSE;

          idspp = get_DeclSort_Parameter(&idsp);
          if (!init_decl_locator(idspp, &loc)) {
            goto invalid;
          }  /* if */
          /* FIXME: This should be a soft failure. */
          check_assertion(idspp->name != 0);
          /* FIXME: constraint_expr = expr_for_expr_index(idspp->constraint);*/
          /* FIXME: init_expr = expr_for_expr_index(idspp->initializer); */
          if (type_tag(idspp->type) == ifc_TypeSort_Expansion) {
            /* FIXME: Currently unsupported. */
            is_pack = TRUE;
            issue_unsupported_node_diag("DeclSort::Parameter packs",
                                        &error_position);
          }  /* if */
          /* FIXME: Currently all paths that lead here have
             curr_templ_decl_state == NULL. */
          switch (idspp->sort) {
            case ifc_ParameterSort_Object:
              /* FIXME: Currently unsupported. */
              issue_unsupported_node_diag("ParameterSort::Object",
                                          &error_position);
              param = make_nontype_template_param(idspp->level,
                                                  idspp->position,
                                                  /*is_unnamed=*/FALSE,
                                                  is_pack,
                                                  /*is_pack_element=*/FALSE,
                                                  /*is_non_initial=*/FALSE,
                                                  /*is_pack_expansion=*/FALSE,
                                                  &loc, error_type(),
                                                  curr_templ_decl_state);
              break;
            case ifc_ParameterSort_Type:
              /* FIXME: Handle unnamed parameters properly */
              param = decl_type_template_param(idspp->position, &loc,
                                               /*is_named=*/TRUE, is_pack,
                                               /*constraint=*/NULL,
                                               curr_templ_decl_state,
                                               &curr_templ_decl_state->
                                                               decl_pos_block);
              break;
            case ifc_ParameterSort_NonType:
              param_type = type_for_type_index(idspp->type, /*kind=*/NULL);
              /* FIXME: Handle unnamed parameters properly. */
              param = make_nontype_template_param(idspp->level,
                                                  idspp->position,
                                                  /*is_unnamed=*/FALSE,
                                                  is_pack,
                                                  /*is_pack_element=*/FALSE,
                                                  /*is_non_initial=*/FALSE,
                                                  /*is_pack_expansion*/FALSE,
                                                  &loc, param_type,
                                                  curr_templ_decl_state);
              break;
            case ifc_ParameterSort_Template:
              /* FIXME: Currently unsupported. */
              issue_unsupported_node_diag("ParameterSort::Template",
                                          &error_position);
              param = make_nontype_template_param(idspp->level,
                                                  idspp->position,
                                                  /*is_unnamed=*/FALSE,
                                                  is_pack,
                                                  /*is_pack_element=*/FALSE,
                                                  /*is_non_initial=*/FALSE,
                                                  /*is_pack_expansion=*/FALSE,
                                                  &loc, error_type(),
                                                  curr_templ_decl_state);
              break;
            default_is_unexpected_str("Unexpected ParameterSort");
          }  /* switch */
          if (param != NULL) {
            next_param = &curr_templ_decl_state->decl_info->parameters;
            /* Skip to end of parameter list. */
            for (; *next_param != NULL; next_param = &(*next_param)->next) {}
            *next_param = param;
            ++curr_templ_decl_state->decl_info->n_params;
            il_param = alloc_template_parameter();
            param->il_template_parameter = il_param;
            param->param_symbol->is_invisible = FALSE;
            il_entity = (char*)il_param;
            kind = iek_template_parameter;
          }  /* if */
        }
        break;
      case ifc_DeclSort_Reference:
        { an_ifc_DeclSort_Reference idsr, *idsrp;
          a_module_entity_ptr       dmep;
          idsrp = get_DeclSort_Reference(&idsr);
          dmep = get_and_process_ifc_decl_from_other_module(idsrp);
          il_entity = dmep->entity.ptr;
          kind = dmep->entity.kind;
          check_assertion(mep->scope == NULL);
          mep->scope = dmep->scope;
          /* We didn't push a context here, so don't pop it either. */
          skip_pop = TRUE;
        }
        break;
      case ifc_DeclSort_Method:
      case ifc_DeclSort_Constructor:
      case ifc_DeclSort_Destructor:
      case ifc_DeclSort_Field:
      case ifc_DeclSort_Bitfield:
      case ifc_DeclSort_Property:
        /* These entities can only exist in a class and class definitions are
           currently handled by scanning a token representation of the
           class. */
        unexpected_condition();
      case ifc_DeclSort_Temploid:
        { an_ifc_DeclSort_Temploid idst;
          get_DeclSort_Temploid(&idst);
          /* FIXME: Need a proper source position here. */
          error_position = null_source_position;
          goto unhandled;
        }
      case ifc_DeclSort_PartialSpecialization:
        { an_ifc_DeclSort_PartialSpecialization idsps, *idspsp;
          idspsp = get_DeclSort_PartialSpecialization(&idsps);
          if (!init_decl_locator(idspsp, &loc)) {
            goto invalid;
          }  /* if */
          if (defer) {
            /* Specify "make_last" as TRUE, because we want partial
               specializations to appear after their primary templates. */
            defer_symbol_creation(mep, &loc, /*make_last=*/TRUE);
            /* Mark the entry as "imminent" so that it will not be processed
               when all the other deferred entries on the associated list are
               handled.  Instead, the partial specializations will be handled
               explicitly immediately after the primary template has been
               processed (see process_template_definition). */
            mep->imminent = TRUE;
          } else {
            ifc_DeclIndex decl_idx = decl_index_of(mep);

            scope_pushed = lazy_push_module_scope(idspsp, mep);
            /* FIXME: Is it feasible to detect ignorable redeclarations of
               partial specializations? */
            if (idspsp->entity.body != 0) {
              a_token_cache cache;

              /* There is a definition of the partial specialization.  Record
                 the resolution of the signature immediately so that the below
                 processing of the definition has access to it. */
              clear_token_cache(&cache, /*reusable=*/FALSE);
              cache_decl_partial_specialization(&cache, decl_idx, idspsp);
              terminate_token_cache(&cache);
              il_entity = (char*)parse_cached_partial_specialization(
                                                                   &cache,
                                                                   mep->scope);
              kind = iek_template;
            }  /* if */
          }  /* if */
        }
        break;
      case ifc_DeclSort_Specialization:
        { an_ifc_DeclSort_Specialization idss, *idssp;
          a_boolean                      is_instantiation = FALSE;
          idssp = get_DeclSort_Specialization(&idss);
          if (idssp->sort == ifc_SpecializationSort_Instantiation) {
            if (scope_is(&scope_stack_top(), sck_class_struct_union)) {
              /* FIXME Explicit instantiations of member templates are
                 recorded as part of the enclosing class definition, but
                 explicit instantiations cannot appear in class scope.
                 For now, just skip those.  Eventually, we should either
                 delay them until we're in namespace scope, or accept such
                 constructs in code generated from modules. */
              break;
            }  /* if */
            is_instantiation = TRUE;
          }  /* if */
          if (!init_decl_locator(idssp, &loc)) {
            goto invalid;
          }  /* if */
          if (defer) {
            if (is_instantiation) {
              unexpected_condition_str("Unexpected deferral.");
            } else {
              defer_symbol_creation(mep, &loc);
            }  /* if */
          } else {
            a_token_cache cache;
            ifc_DeclIndex decl_idx = decl_index_of(mep);

            scope_pushed = lazy_push_module_scope(idssp, mep);
            clear_token_cache(&cache, /*reuseable=*/FALSE);
            cache_decl_specialization(&cache, decl_idx, idssp);
            terminate_token_cache(&cache);
            if (is_instantiation) {
              il_entity = parse_cached_explicit_instantiation(&cache, idssp,
                                                              &kind);
            } else {
              il_entity =
                        (char*)parse_cached_explicit_specialization(&cache,
                                                                    mep->scope,
                                                                    idssp);
            }  /* if */
            kind = iek_template;
          }  /* if */
        }
        break;
      case ifc_DeclSort_Concept:
        { an_ifc_DeclSort_Concept idsc, *idscp;
          idscp = get_DeclSort_Concept(&idsc);
          if (!init_decl_locator(idscp, &loc)) {
            goto invalid;
          }  /* if */
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* Create a definition for the concept and scan it. */
            a_curr_token_preserver  guard;
            a_token_cache           cache;
            if (is_from_gmf(idscp->specifiers)) {
              mep->global_module = TRUE;
            }  /* if */
            if (mep->scope == NULL) {
              mep->scope = get_ifc_home_scope(idscp);
              scope_pushed = push_module_declaration_context(mep->scope);
            }  /* if */
            if (ifc_decl_is_ignorable_redecl(&loc, mep, &error_position,
                                             iek_template, &il_entity,
                                             &kind)) {
              break;
            }  /* if */
            /* Activate the parent scope if needed. */
            clear_token_cache(&cache, /*reusable=*/FALSE);
            /* Generate the template parameter list. */
            cache_token(&cache, tok_template, &null_source_position);
            cache_chart(&cache, idscp->chart, &idscp->locus);
            /* Generate "concept <concept-name>". */
            cache_sentence(&cache, idscp->head);
            /* Generate "= <constraint-expression> ;". */
            cache_sentence(&cache, idscp->body);
            /* Terminate the definition cache and parse it. */
            terminate_token_cache(&cache);
            il_entity = (char*)parse_cached_template(&cache, mep->scope);
            kind = iek_template;
          }  /* if */
        }
        break;
      case ifc_DeclSort_InheritedConstructor:
        { an_ifc_DeclSort_InheritedConstructor idsic, *idsicp;
          idsicp = get_DeclSort_InheritedConstructor(&idsic);
          source_position_from_locus(&error_position, &idsicp->locus);
          goto unhandled;
        }
      case ifc_DeclSort_OutputSegment:
        { an_ifc_DeclSort_OutputSegment idsos;
          get_DeclSort_OutputSegment(&idsos);
          /* FIXME: Need a proper source position here. */
          error_position = null_source_position;
          goto unhandled;
        }
      case ifc_DeclSort_UsingDeclaration:
        { an_ifc_DeclSort_UsingDeclaration idsud, *idsudp;
          idsudp = get_DeclSort_UsingDeclaration(&idsud);
          if (defer) {
            Opt<an_ifc_Ref<ifc_NameIndex>> opt_name_ref;

            opt_name_ref = get_ifc_name(idsudp);
            if (!opt_name_ref.has_value()) {
              goto invalid;
            }  /* if */
            if (!init_locator_from_name(*opt_name_ref, &idsudp->locus, &loc)) {
              goto invalid;
            }  /* if */
            defer_symbol_creation(mep, &loc);
          } else {
            if (!source_position_from_locus(&error_position, &idsudp->locus)) {
              goto invalid;
            }  /* if */
            if (decl_tag(idsudp->resolution) == ifc_DeclSort_Tuple) {
              /* FIXME: Not sure what this is. */
              goto unhandled;
            }  /* if */
            a_module_entity_ptr umep =
                                 get_ifc_module_entity_ptr(idsudp->resolution);
            process_ifc_declaration(umep, /*defer=*/FALSE, (a_type_ptr)NULL);
            if (scope_is(umep->scope, sck_class_struct_union)) {
              /* FIXME: Need to call create_member_using_declaration here. */
              goto unhandled;
            } else {
              /* Non-member using declaration.  Could be file scope or
                 namespace scope. */
              a_symbol_ptr     null_sym_ptr = NULL;
              a_using_decl_ptr prev_udp = NULL;
              a_namespace_ptr  nsp = NULL;
              a_source_correspondence *scp =
                                    (a_source_correspondence*)umep->entity.ptr;
              if (scp->parent_scope == NULL) {
                /* Probably shouldn't happen, but happens now because of other
                   issues. */
                goto unhandled;
              }  /* if */
              if (scope_is(scp->parent_scope, sck_namespace)) {
                nsp = scp_parent_namespace(scp);
              }  /* if */
              /* FIXME: This will need to be re-worked when handling the
                 ifc_DeclSort_Tuple case (i.e., multiple items). */
              create_nonmember_using_declaration(
                                               (a_symbol_ptr)scp->assoc_info,
                                               &null_sym_ptr,
                                               (a_symbol_ptr)NULL,
                                               nsp,
                                               (a_type_ptr)NULL,
                                               &prev_udp,
                                               /*is_list=*/FALSE,
                                               /*suppress_redecl_error=*/TRUE);
            }  /* if */
          }  /* if */
        }
        break;
      case ifc_DeclSort_UsingDirective:
        { an_ifc_DeclSort_UsingDirective idsud;
          get_DeclSort_UsingDirective(&idsud);
          /* FIXME: Need a proper source position here. */
          error_position = null_source_position;
          goto unhandled;
        }
      case ifc_DeclSort_Friend:
        { an_ifc_DeclSort_Friend idsf;
          get_DeclSort_Friend(&idsf);
          /* FIXME: Need a proper source position here. */
          error_position = null_source_position;
          goto unhandled;
        }
      case ifc_DeclSort_SyntaxTree:
        { an_ifc_DeclSort_SyntaxTree idsst;
          get_DeclSort_SyntaxTree(&idsst);
          /* FIXME: Need a proper source position here. */
          error_position = null_source_position;
          goto unhandled;
        }
      case ifc_DeclSort_Tuple:
        { an_ifc_DeclSort_Tuple idst, *idstp;
          idstp = get_DeclSort_Tuple(&idst);
          for (unsigned k = 0; k < idstp->cardinality; ++k) {
            a_module_entity_ptr  emep;
            ifc_DeclIndex        declidx;
            declidx = (ifc_DeclIndex)read_index_from_heap(ifc_heap_decl,
                                                          idstp->start+k);
            emep = get_ifc_module_entity_ptr(declidx);
            /* In at least some cases (the handling of deduction guides), the
               caller will have filled in mep->scope and that should be
               propagated to the individual associated declarations. */
            emep->scope = mep->scope;
            this->process_ifc_declaration(emep, /*defer=*/FALSE,
                                          (a_type*)NULL);
          }  /* for */
        }
        break;
      case ifc_DeclSort_Expansion:
        { an_ifc_DeclSort_Expansion idse, *idsep;
          idsep = get_DeclSort_Expansion(&idse);
          source_position_from_locus(&error_position, &idsep->locus);
          goto unhandled;
        }
      case ifc_DeclSort_DeductionGuide:
        { an_ifc_DeclSort_DeductionGuide idsdg, *idsdgp;
          idsdgp = get_DeclSort_DeductionGuide(&idsdg);
          source_position_from_locus(&error_position, &idsdgp->locus);
          goto unhandled;
        }
      case ifc_DeclSort_Barren:
        { an_ifc_DeclSort_Barren idsb;
          get_DeclSort_Barren(&idsb);
          /* FIXME: Need a proper source position here. */
          error_position = null_source_position;
unhandled:
          issue_unsupported_node_diag(str_for_decl_tag(tag), &error_position);
          il_entity = (char *)error_type();
          kind = iek_type;
        }
        break;
      case ifc_DeclSort_UnusedSort0:
      case ifc_DeclSort_Last:
        unexpected_condition();
        break;
      default_is_unexpected_str("Unexpected DeclSort");
    }  /* switch */
    goto cleanup;
invalid:
    mep->invalid = TRUE;
cleanup:
    if (!defer) {
      /* Record the IL entity. */
      mep->entity.ptr = il_entity;
      mep->entity.kind = kind;
#if DEBUG
      if (db_flag_is_set("ms_symbols")) {
        (void)fprintf(f_debug, "Module entity defined: ");
        db_module_entity(mep);
      }  /* if */
#endif /* DEBUG */
      if (!skip_pop) {
        pop_module_declaration_context(scope_pushed);
      }  /* if */
    }  /* if */
    curr_module_entity = saved_mep;
    error_position = saved_error_position;
  }  /* if */
  /* In some cases we may enter this routine without having a scope, but we
     should not exit it without having one. */
  check_assertion(mep->scope != NULL);
#if DEBUG
  if (db_flag_is_set("ifc_decl")) {
    (void)fprintf(f_debug, "[<%lu] ", --nested_decls);
    db_mep(mep);
  }  /* if */
#endif /* DEBUG */
}  /* process_ifc_declaration */


namespace {
/* An RAII object to temporarily enable microsoft extensions even if not
   enabled otherwise.  This allows for MS specific IFC decls to be correctly
   processed.
*/
struct an_ms_extensions_parse {
  an_ms_extensions_parse() : old_ms_extensions(ms_extensions),
                             old_ms_compat(ms_compat) {
    ms_extensions = TRUE;
    ms_compat = TRUE;
  }
  ~an_ms_extensions_parse() {
    ms_compat = old_ms_compat;
    ms_extensions = old_ms_extensions;
  }
private:
  a_boolean old_ms_extensions;
  a_boolean old_ms_compat;
};  /* an_ms_extensions_parse */
}  /* namespace */


void an_ifc_module::complete_definition_of_module_class(
                                                      a_module_entity_ptr mep)
/*
Complete the definition of the class referred to by mep (if needed).
*/
{
  a_diagnostic_suppression diag_suppress(&this->suppressed_diagnostics,
                                         !display_module_import_diagnostics);
  a_type_ptr               class_type = (a_type_ptr)mep->entity.ptr;
  an_ifc_DeclSort_Scope    idss, *idssp;
  a_token_cache            cache;

  check_assertion(mep->entity.kind == (an_il_entry_kind)iek_type &&
                  class_type != NULL);
  read_prechecked_partition_element(mep);
  idssp = get_DeclSort_Scope(&idss);
  if (class_type->incomplete && idssp->initializer != 0) {
    a_template_decl_info_ptr tdip;
    a_symbol_ptr             class_sym = symbol_for(class_type);
    a_scope_depth            saved_non_local_class_fixup_depth =
                                                   non_local_class_fixup_depth;
    a_source_position        saved_error_position = error_position;
    a_boolean                scope_pushed = FALSE;
    a_curr_token_preserver   guard;

    scope_pushed = push_module_declaration_context(mep->scope);
    source_position_from_locus(&error_position, &idssp->locus);
    clear_token_cache(&cache, /*reusable=*/FALSE);
    cache_class_definition(&cache, idssp);
    terminate_token_cache(&cache);
#if DEBUG
    if (db_flag_is_set("ms_ifc_token_def")) {
      fprintf(f_debug, "Reconstituted class definition:\n");
      db_tokens(&cache);
      fprintf(f_debug, "\n---------------------\n");
    }  /* if */
#endif /* DEBUG */
    push_stop_token_stack();
    rescan_cached_tokens(&cache);
    tdip = alloc_template_decl_info();
    set_template_decl_info_for_class_definition(tdip, class_type);
    (void)push_template_instantiation_scope(
                              tdip, class_type,
                              (a_routine_ptr)NULL, class_sym,
                              class_sym, (a_template_arg_ptr)NULL,
                              /*push_lex_state=*/TRUE,
                              PS_CLASS_DEFINITION_CONTEXT);
    /* Set the fixup depth for non-local classes to the context scope pushed
       above so that classes created by this routine will be fixed up by
       process_deferred_class_fixups_and_instantiations. */
    non_local_class_fixup_depth = depth_scope_stack;
    /* By default, the instantiation scope context pushed by the call to
       push_template_instantiation_scope just copies the name linkage from the
       previous entry on the scope stack, which may not be related to that of
       the class. */
    scope_stack_top().default_name_linkage =
                                       class_type->source_corresp.name_linkage;
    {
      an_ms_extensions_parse tmp_parse;
      (void)scan_class_definition(class_type, (a_decl_parse_state*)NULL,
                                  depth_innermost_namespace_scope,
                                  /*is_partial=*/FALSE,
                                  /*is_local_class=*/FALSE,
                                  /*delayed_nested_class_def=*/
                                    class_type->source_corresp.is_class_member,
                                  /*is_template_instantiation=*/FALSE,
                                  /*is_template_specialization=*/FALSE,
                                  (a_template_ptr)NULL,
                                  (a_decl_pos_block_ptr)NULL);
    }
    process_deferred_class_fixups_and_instantiations(
                                                   /*for_instantiation=*/TRUE);
    {
      /* FIXME: Reintegrate the final token concept. */
      clear_stop_tokens();
      flush_tokens_without_warning();
      pop_stop_token_stack();
    }
    check_assertion(curr_token == tok_end_of_source);
    (void)get_token();
    check_assertion(!class_type->incomplete);
    non_local_class_fixup_depth = saved_non_local_class_fixup_depth;
    pop_template_instantiation_scope();
    free_template_decl_info(tdip);
    pop_module_declaration_context(scope_pushed);
    error_position = saved_error_position;
  }  /* if */
}  /* complete_definition_of_module_class */

#if DEBUG

void an_ifc_module::debug() const
/*
Print debug information for an IFC module
*/
{
  (void)fprintf(f_debug, ", kind: mk_ifc, version: %u.%u\n",
                header.major_version, header.minor_version);
}  /* debug */


void an_ifc_module::db_module_entity(a_module_entity_ptr mep) const
/*
Print debug information related to a module entity that refers to this module.
*/
{
  check_assertion(mep->module_info->module_interface == this);
  if (mep->variant.ifc_partition != ifc_none) {
    const an_ifc_partition &part = partitions[mep->variant.ifc_partition];
    (void)fprintf(f_debug, " IFC partition \"%s\", index %lu\n", part.name,
                  (unsigned long)(mep->file_offset - part.offset) /
                                                              part.entry_size);
  } else {
    (void)fprintf(f_debug, "\n");
  }  /* if */
}  /* db_module_entity */


void db_mep(a_module_entity_ptr mep)
/*
Print information about the module entity pointer.
*/
{
  if (mep->module_info != NULL) {
    (void)fprintf(f_debug, "[%s]: ", mep->module_info->name);
  }  /* if */
  if (mep->entity.kind != iek_none) {
    db_scp((a_source_correspondence*)mep->entity.ptr);
  } else {
    if (mep->scope != NULL) {
      db_scope(mep->scope);
    }  /* if */
    (void)fprintf(f_debug, "\n");
  }  /* if */
}  /* db_mep */


void an_ifc_module::db_locus(ifc_SourceLocation *locus)
/*
Print the corresponding file and line number for the source location
(for debugging purposes).
*/
{
  a_const_char      *full_name, *diag_file_name;
  a_line_number     line_number;
  a_boolean         at_end_of_source;
  a_source_position pos;

  diag_file_name = "";
  /* Save global information related to position before calling
     source_position_from_locus. */
#if DEBUG && EXPENSIVE_CHECKING
  const an_ifc_partition *save_debug_partition = debug_partition;
#endif /* DEBUG && EXPENSIVE_CHECKING */
#if USE_MMAP_FOR_MEMORY_REGIONS
  unsigned char *save_byte_buffer = byte_buffer;
  unsigned char *save_buffer_end = buffer_end;
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
  long save_seek = ftell(f_module);
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  source_position_from_locus(&pos, locus);
  /* Restore saved information. */
#if DEBUG && EXPENSIVE_CHECKING
  debug_partition = save_debug_partition;
#endif /* DEBUG && EXPENSIVE_CHECKING */
#if USE_MMAP_FOR_MEMORY_REGIONS
  byte_buffer = save_byte_buffer;
  buffer_end = save_buffer_end;
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
  (void)fseek(f_module, save_seek, SEEK_SET);
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  if (pos.seq != 0) {
    (void)conv_seq_to_file_and_line(pos.seq, &diag_file_name,
                                    &full_name, &line_number,
                                    &at_end_of_source);
    (void)fprintf(f_debug, "%s: line %lu colmun %lu\n", full_name,
                  (unsigned long) line_number, (unsigned long) pos.column);
  }  /* if */
}  /* db_locus */

#endif /* DEBUG */

void an_ifc_module::init_string_table_and_header()
{
  /* Read the IFC file header (which starts after the magic number). */
  init_byte_buffer(4, f_size - 4);
  get_File_Header(&header, /*fill_storage=*/TRUE);
  /* FIXME: The checksum is not yet checked. */
#if USE_MMAP_FOR_MEMORY_REGIONS
  string_table = (a_const_char*)mmap_addr + header.string_table_bytes;
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
  string_table = alloc_il(header.string_table_size);
  fseek(f_module, header.string_table_bytes, SEEK_SET);
  if (fread((void*)string_table, 1, header.string_table_size, f_module) !=
                                                  header.string_table_size) {
    unexpected_condition_str("Failed to load the IFC module string table");
  }  /* if */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
}  /* init_string_table_and_header */


a_boolean an_ifc_module::initialize_members_from_ifc_module_file(
                                           a_module_import_decl_ptr midp,
                                           a_boolean                issue_diag)
{
  unsigned int i;
  a_module_ptr mod = midp->module_info;
  a_boolean    result = TRUE;

  assoc_module_info = mod;
  set_name(mod->name, is_header_unit(mod));
  init_string_table_and_header();
  if (!check_ifc_version(header.major_version, header.minor_version)) {
    an_error_severity sev;
    if (skip_module_version_check) {
      sev = es_warning;
    } else {
      sev = es_catastrophe;
      result = FALSE;
    }  /* if */
    if (issue_diag) {
      emit_unsupported_ifc_version_diagnostic(midp, /*mod_iface=*/this, sev);
    }  /* if */
    if (!result) {
      goto done;
    }  /* if */
  }  /* if */
  /* Prepare to read the partitions (by "seeking" to the IFC Table of
     Contents). */
  init_byte_buffer(header.toc, f_size - (size_t)header.toc);
#if DEBUG
  if (db_flag_is_set("ifc_modules")) {
    db_module(mod);
  }  /* if */
#endif /* DEBUG */
  for (i = 0; i < (unsigned int)header.partition_count; i++) {
    an_ifc_Partition     partition, *ifc_pp;
    an_ifc_partition     *pp;
    a_const_char         *name_str;
    an_ifc_partition_map *map_ptr;

    /* Read information about the partition. */
    ifc_pp = get_Partition(&partition);
    name_str = get_string_at_offset(ifc_pp->name);
#if DEBUG
    if (db_flag_is_set("ifc_modules")) {
      (void)fprintf(
            f_debug,
            "partition %u \"%s\" offset 0x%08x cardinality %u entry_size %u\n",
            i, name_str, ifc_pp->offset, ifc_pp->cardinality,
            ifc_pp->entry_size);
    }  /* if */
#endif /* DEBUG */
    check_assertion(ifc_pp->cardinality != 0 && ifc_pp->offset != 0);
    map_ptr = find_ifc_partition(name_str);
    if (map_ptr == NULL) {
      if (issue_diag) {
        str_warning(ec_unknown_ifc_partition, name_str);
      }  /* if */
    } else {
      check_assertion_str(map_ptr->kind != ifc_last,
                          "no mapping for IFC partition");
      pp = &partitions[map_ptr->kind];
      pp->name = map_ptr->name;
      pp->offset = ifc_pp->offset;
      pp->size = ifc_pp->cardinality * ifc_pp->entry_size;
      pp->entry_size = ifc_pp->entry_size;
      validate_partition_size(pp, map_ptr->kind);
      pp->format_validated = alloc_validation_bit_array(pp->size /
                                                        pp->entry_size);
    }  /* if */
  }  /* for */
  (void)fseek(f_module, 0L, SEEK_SET);
  if (partitions[ifc_name_source_file].name != NULL) {
    /* Allocate an array to map source locations to sequence numbers for each
       file referenced by the module.  No information about the sequence
       numbers is recorded yet (we do that only if the source file is later
       referenced). */
    an_ifc_partition *nsf_pp = &partitions[ifc_name_source_file];
    check_assertion(nsf_pp->entry_size != 0);
    size_t num_files = nsf_pp->size / nsf_pp->entry_size;
    size_t size = num_files * sizeof(a_module_sequence_number_mapping);
    sequence_numbers = (a_module_sequence_number_mapping *)alloc_fe(size);
    memzero((char *)sequence_numbers, size);
    if (partitions[ifc_src_line].name != NULL) {
      check_assertion(partitions[ifc_src_line].entry_size != 0);
      /* As a (hopefully) temporary measure, for each file referenced in the
         module, we need to determine the largest line number that will be seen
         in that file (we don't actually need the last line number in the file,
         just the largest one that will be seen in the source location, though
         the last number would do).  This number will be used (if needed) to
         increment the source sequence when the module is referenced to
         effectively reserve those source sequence numbers for the file. */
      for (uint32_t idx = 0, num_src_lines = get_num_entries(ifc_src_line);
           idx < num_src_lines; idx++) {
        an_ifc_Source_Line   isl, *islp;

        if (!read_partition_element(ifc_src_line, idx)) {
          result = FALSE;
          goto done;
        }  /* if */
        islp = get_Source_Line(&isl);
        size_t file_index = name_value(islp->file);
        check_assertion(name_tag(islp->file) == ifc_NameSort_SourceFile &&
                        file_index < num_files);
        if (islp->line > sequence_numbers[file_index].max_line_number) {
          sequence_numbers[file_index].max_line_number = islp->line;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
done:
  return result;
}  /* initialize_members_from_ifc_module_file */


a_boolean an_ifc_module::open_and_map_ifc_module_file(
                                           a_module_import_decl_ptr midp,
                                           a_boolean                issue_diag)
/*
Open the module file and map it into the process' address space.  Note that
this is also used after restoring from a PCH file.  Return TRUE if the module
file was successfully opened and FALSE (with an error message if issue_diag ==
TRUE) otherwise.
*/
{
  a_module_ptr  mod = midp->module_info;
  a_boolean     err = FALSE;
  FILE          *file;
  a_byte        magic[4];
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
    if (!err && !magic_numbers_match(magic, ifc_magic_numbers)) {
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
      f_size = mmap_size;
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
      fseek(file, 0, SEEK_END);
      f_size = (size_t)ftell(file);
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
      f_module = file;
    }  /* if */
    if (err) {
      (void)fclose(file);
    }  /* if */
  }  /* if */
  if (err && issue_diag) {
    /* FIXME: perhaps better error messages here. */
    pos_st_error(ec_cannot_import_module, &midp->module_name_position,
                 mod->full_name);
  }  /* if */
  return !err;
}  /* open_and_map_ifc_module_file */


namespace {
/*
An internal representation of an IFC partition name used to facilitate binary
search of the partition map.
*/
struct an_ifc_partition_name {
  bool operator<(const an_ifc_partition_name& other) const
  {
    return strcmp(name, other.name) < 0;
  } /* operator< */

  bool operator==(const an_ifc_partition_name& other) const
  {
    return strcmp(name, other.name) == 0;
  } /* operator== */

  a_const_char *name;
};  /* an_ifc_partition_name */
}  /* namespace */


#if EXPENSIVE_CHECKING
#if DEBUG
template<>
void db_f_print_t(FILE *stream, const an_ifc_partition_name &partition_name)
/*
Provide a generic printer for an_ifc_partition_name.  Note that this
specialization would normally be predeclared in util.h, however as
an_ifc_partition_name has internal linkage it's safe to declare the
specialization here.
*/
{
  fprintf(stream, "%s", partition_name.name);
} /* db_f_print_t */
#endif /* DEBUG */


static void validate_ifc_partition_map(
                                an_ifc_partition_map *map_ptr,
                                uint32_t             num_searchable_partitions)
/*
Validate the state of the partition map for binary search, ensuring that
nameless partitions are at the end of the partition map and that all nameless
partitions are omitted from the search.
*/
{
  uint32_t num_nameless_partitions = 0;

  check_assertion(sizeof(ifc_partition_map) / sizeof(an_ifc_partition_map) ==
                                                           num_ifc_partitions);
  for (uint32_t i = 0; i < num_ifc_partitions; ++i) {
    if (map_ptr->name == NULL) {
      ++num_nameless_partitions;
    } else {
      /* Assert that we have no nameless partitions, to ensure we didn't see a
         partition without a name followed by a partition with a name.  This in
         effect verifies any nameless partitions are at the end of the map. */
      check_assertion(num_nameless_partitions == 0);
    }  /* if */
    ++map_ptr;
  }  /* for */
  /* Verify that we're skipping the correct number of nameless partitions. */
  check_assertion(num_ifc_partitions ==
                  num_nameless_partitions + num_searchable_partitions);
}  /* validate_ifc_partition_map */
#endif /* EXPENSIVE_CHECKING */


an_ifc_partition_map *an_ifc_module::find_ifc_partition(a_const_char *name)
/*
Find the IFC partition map entry for the partition matching name.  Return a
pointer to that entry or NULL if it could not be found.
*/
{
  /* The number of partitions is adjusted to remove any nameless partition map
     entries. */
  uint32_t num_partitions = num_ifc_partitions;
  /* Create a wrapped version of partition_name for comparisons. */
  an_ifc_partition_name partition_name{name};
  /* Provide a value function for retrieving the wrapped partition name at the
     given partition map index. */
  auto value_lambda = [](ptrdiff_t idx) {
    return an_ifc_partition_name{ifc_partition_map[idx].name};
  };
  /* Get the partition map index (if any) for the given partition name. */
  ptrdiff_t partition_map_idx = bin_search(num_partitions, partition_name,
                                           value_lambda);
  an_ifc_partition_map *map_ptr = NULL;

#if EXPENSIVE_CHECKING
  validate_ifc_partition_map(ifc_partition_map, num_partitions);
#endif /* EXPENSIVE_CHECKING */
  /* If we have a matching partition map entry, return it, otherwise return
     null. */
  if (partition_map_idx != -1) {
    map_ptr = &ifc_partition_map[partition_map_idx];
  }  /* if */
  return map_ptr;
}  /* find_ifc_partition */


template<typename a_Scope_Member_Consumer>
inline void an_ifc_module::traverse_scope_member_sequence(
                                              ifc_Sequence            seq,
                                              a_Scope_Member_Consumer consumer)
/*
Iterate over a given scope member sequence (seq) passing an_ifc_Scope_Member
pointer to the given consumer lambda for each element in the sequence.
*/
{
  for (uint32_t idx = 0; idx < seq.cardinality; ++idx) {
    an_ifc_Scope_Member ism, *ismp;

    /* Load the scope member. */
    read_prechecked_partition_element(ifc_scope_member, seq.start + idx);
    ismp = get_Scope_Member(&ism);
    /* Handle any specific processing in the consumer. */
    consumer(ismp);
  }  /* for */
}  /* traverse_scope_member_sequence */

void an_ifc_module::process_scope_member_sequence(ifc_Sequence seq)
/*
Process a sequence (seq) of IFC scope member declarations.
*/
{
  /* Provide a consumer function that accepts a given scope member and
     processes the associated IFC declaration. */
  auto decl_consumer = [this](an_ifc_Scope_Member *ismp) {
    /* Get the associated IFC module entity pointer, and then use it to process
       this scope member via process_ifc_declaration. */
    a_module_entity_ptr dmep = get_ifc_module_entity_ptr(ismp->index);
    process_ifc_declaration(dmep, /*defer=*/FALSE, (a_type_ptr)NULL);
  };
  /* Iterate over the sequence calling decl_consumer for each element. */
  traverse_scope_member_sequence(seq, decl_consumer);
}  /* process_scope_member_sequence */


void an_ifc_module::process_ifc_scope(ifc_ScopeIndex scope_index,
                                      a_scope_ptr    scope)
/*
Process the IFC scope specified by scope_index in the module file.  All items
in the IFC scope will be members of scope and their definitions will be
deferred until they are referenced.
*/
{
  an_ifc_Scope_Member     ism, *ismp;
  unsigned int            i;
  a_boolean               scope_pushed;
  a_module_entity_ptr     dmep;

  /* A scope index of 0 indicates a missing scope, in which case there
     is nothing further to do. */
  if (scope_index != 0) {
    scope_pushed = push_module_declaration_context(scope);
    /* Scope indices are 1-based, so subtract one. */
    if (read_partition_element_shallow(ifc_scope_desc, scope_index - 1)) {
      an_ifc_Scope_Descriptor isd, *isdp;

      isdp = get_Scope_Descriptor(&isd);
      for (i = 0; i < isdp->cardinality; i++) {
        /* Re-enable access to scope.member partition (it changes during the
           loop). */
        if (read_partition_element_shallow(ifc_scope_member,
                                           isdp->start + i)) {

          ismp = get_Scope_Member(&ism);
          dmep = get_ifc_module_entity_ptr(ismp->index);
          dmep->scope = scope;
          process_ifc_declaration(dmep, /*defer=*/TRUE, (a_type_ptr)NULL);
        }  /* if */
      }  /* for */
      pop_module_declaration_context(scope_pushed);
    }  /* if */
  }  /* if */
}  /* process_ifc_scope */


uint32_t an_ifc_module::get_num_entries(an_ifc_partition_kind partition) const
/*
Return the number of entries in a given partition.
*/
{
  uint32_t num_entries = 0;

  /* If there is an entry size defined, calculate the number of entries. */
  if (partitions[partition].entry_size != 0) {
    num_entries = partitions[partition].size /
                  partitions[partition].entry_size;
  }  /* if */
  return num_entries;
}  /* get_num_entries */


a_module_entity_ptr an_ifc_module::get_ifc_module_entity_ptr(
                                        an_ifc_partition_kind partition,
                                        ifc_Index_type        index) const
/*
Utility to return a module entity pointer for this module given an IFC
partition, and an index into that partition.  For cases where the module entity
has just been created, the partition is set according to the partition supplied
by the caller.
*/
{
  an_ifc_partition_position partition_pos(this, partition, index);
  a_module_entity_ptr       mep;

  /* FIXME: Handle error case, formatting. */
  mep = get_module_entity_ptr(assoc_module_info, partition_pos.file_offset);
  if (mep->variant.ifc_partition == ifc_none) {
    mep->variant.ifc_partition = partition;
  } else {
    check_assertion(mep->variant.ifc_partition == partition);
  }  /* if */
  return mep;
}  /* get_ifc_module_entity_ptr */


inline a_module_entity_ptr an_ifc_module::get_ifc_module_entity_ptr(
                                                           ifc_TypeIndex index)
                                                                          const
/*
Overload wrapper for "get_ifc_module_entity_ptr" that extracts the type sort
and index from the provided index.
*/
{
  an_ifc_partition_kind partition = (an_ifc_partition_kind)(ifc_type_start +
                                                              type_tag(index));

  return get_ifc_module_entity_ptr(partition, type_value(index));
}  /* get_ifc_module_entity_ptr */


inline a_module_entity_ptr an_ifc_module::get_ifc_module_entity_ptr(
                                                           ifc_DeclIndex index)
                                                                          const
/*
Overload wrapper for "get_ifc_module_entity_ptr" that extracts the decl sort
and index from the provided index.
*/
{
  an_ifc_partition_kind partition = (an_ifc_partition_kind)(ifc_decl_start +
                                                              decl_tag(index));

  return get_ifc_module_entity_ptr(partition, decl_value(index));
}  /* get_ifc_module_entity_ptr */


a_module_entity_ptr an_ifc_module::get_ifc_decl_from_other_module(
                                          const an_ifc_DeclSort_Reference *ref)
/*
Given a DeclSort::Reference to another module, get and return the unprocessed
entity from the referenced module.
*/
{
  a_module_import_decl_ptr  midp;
  an_ifc_module             *iface;

  midp = transitive_import_module(&ref->unit);
  check_assertion(midp != NULL);
  iface = (an_ifc_module*)midp->module_info->module_interface;
  check_assertion(iface != NULL);
  return iface->get_ifc_module_entity_ptr(ref->local_index);
}  /* get_ifc_decl_from_other_module */


a_module_entity_ptr an_ifc_module::get_ifc_decl_from_other_module(
                                                           ifc_DeclIndex index)
/*
Given an index for a DeclSort::Reference, get and return the unprocessed entity
from the referenced module.
*/
{
  an_ifc_DeclSort_Reference idsr, *idsrp;

  check_assertion(decl_tag(index) == ifc_DeclSort_Reference);
  read_prechecked_partition_element(index);
  idsrp = get_DeclSort_Reference(&idsr);
  return get_ifc_decl_from_other_module(idsrp);
}  /* get_ifc_decl_from_other_module */


static an_ifc_module *get_as_an_ifc_module(a_module_interface_ptr interface)
/*
Reinterpret the given module interface pointer as an ifc module pointer with
additional checks for debug modes.  Return the reinterpreted pointer.
*/
{
#if USE_VIRTUAL_FUNCTIONS
  check_assertion(dynamic_cast<an_ifc_module*>(interface) != NULL);
#else /* !USE_VIRTUAL_FUNCTIONS */
  check_assertion(interface->mod_kind == mk_ifc);
#endif /* USE_VIRTUAL_FUNCTIONS */
  return (an_ifc_module*)interface;
}  /* get_as_an_ifc_module */


static an_ifc_module *get_assoc_ifc_module(a_module_entity_ptr mep)
/*
Return the associated module interface for the given module entity pointer as
an ifc module.
*/
{
  a_module_interface *interface = mep->module_info->module_interface;

  return get_as_an_ifc_module(interface);
}  /* get_assoc_ifc_module */


void an_ifc_module::process_ifc_decl_from_other_module(
                                                      a_module_entity_ptr dmep)
/*
Given a module entity pointer for a different IFC module, fully process the
associated entity from the referenced module.
*/
{
  an_ifc_module *mod = get_assoc_ifc_module(dmep);

  check_assertion(mod != this);
  mod->process_ifc_declaration(dmep, /*defer=*/FALSE,
                               /*enumeration_type=*/NULL);
}  /* process_ifc_decl_from_other_module */


a_module_entity_ptr an_ifc_module::get_and_process_ifc_decl_from_other_module(
                                          const an_ifc_DeclSort_Reference *ref)
/*
Given a DeclSort::Reference to another module, get and return the
fully-processed entity from the referenced module.
*/
{
  a_module_entity_ptr dmep = get_ifc_decl_from_other_module(ref);

  process_ifc_decl_from_other_module(dmep);
  return dmep;
}  /* get_and_process_ifc_decl_from_other_module */


a_module_entity_ptr an_ifc_module::get_and_process_ifc_decl_from_other_module(
                                                           ifc_DeclIndex index)
/*
Given an index for a DeclSort::Reference, get and return the fully-processed
entity from the referenced module.
*/
{
  a_module_entity_ptr dmep = get_ifc_decl_from_other_module(index);

  process_ifc_decl_from_other_module(dmep);
  return dmep;
}  /* get_and_process_ifc_decl_from_other_module */


static Opt<an_ifc_Ref<ifc_NameIndex>> get_ifc_name_from_primary_template(
                                                  an_ifc_module     *mod,
                                                  ifc_FormSpecIndex form_index)
/*
Given a form spec index for the given ifc module, find and return the
associated name.

FIXME: This method should be removed in the future and used sparingly, as it's
effectively a hack to retrieve a non-mangled name from the primary template.

At the time of writing the name used by the templated declaration is mangled
due to a bug in MSVC, as a result we must extract the name from the primary
template's declaration.
*/
{
  Opt<an_ifc_Node<an_ifc_Form_Spec>> opt_ifs;
  Opt<an_ifc_Ref<ifc_NameIndex>>     result;

  /* Load the specialization form to figure out what the primary template's
     declaration is. */
  construct_node(&opt_ifs, mod, form_index);
  if (opt_ifs.has_value()) {
    an_ifc_Node<an_ifc_Form_Spec> ifs = *opt_ifs;

    /* Retrieve the name through the primary template. */
    result = mod->get_ifc_name(ifs->primary_template);
  }  /* if */
  return result;
}  /* get_ifc_name_from_primary_template */


/* A CRT (curiously recursive template) visitor class for dispatching to a
   visit function that should be "overridden" by implementing derived
   classes. */
/* FIXME: Upon moving to C++14 this class and its derivatives can be replaced
   by a lambda based dispatch function template.  C++11 does not provide the
   generic lambda facilities required to allow this implementation scheme. */
template<typename a_Result_T, typename a_Derived_T>
struct an_ifc_module::Decl_value_visitor {
  Decl_value_visitor(an_ifc_module *ifc_mod_val) : ifc_mod(ifc_mod_val)
    {}

  template<typename an_ifc_DeclSort_T>
  inline auto visit(an_ifc_DeclSort_T *decl) -> a_Result_T = delete;
  auto visit_index(ifc_DeclIndex decl_index) -> a_Result_T;
protected:
  inline auto getDerived() -> a_Derived_T *
    { return static_cast<a_Derived_T*>(this); }

  an_ifc_module *ifc_mod;
};  /* Decl_value_visitor */


template<typename a_Result_T, typename a_Derived_T>
auto an_ifc_module::Decl_value_visitor<a_Result_T, a_Derived_T>::visit_index(
                                                      ifc_DeclIndex decl_index)
                                                                  -> a_Result_T
/*
Facilitate dispatch to the correct derived visitor based on the given
declaration's index.  Return the associated result.
*/
{
  a_Result_T result;

  ifc_mod->read_prechecked_partition_element(decl_index);
  switch (decl_tag(decl_index)) {
#define IFC_DECL_DECLSORT_START(name) \
    case concat(ifc_DeclSort_, name): \
      { concat(an_ifc_DeclSort_, name) mem, *memp; \
        memp = ifc_mod->get<concat(an_ifc_DeclSort_, name)>(&mem); \
        result = getDerived()->visit(memp); \
      } \
      break;
#define IFC_DECL_START(name) /* nothing */
#define IFC_DECL_FIELD(field, type) /* nothing */
#define IFC_DECL_END(name) /* nothing */
/*lint -e451 included more than once. */
#include "ifc_map.h"
/*lint +e451*/
    default:
      unexpected_condition_str("Unexpected DeclSort");
  }  /* switch */
  return result;
}  /* visit_index */


/* An implementation of Decl_value_visitor with the visit function "overridden"
   to facilitate retrieval of a declaration's name. */
struct an_ifc_module::decl_name_visitor
  : public an_ifc_module::Decl_value_visitor<Opt<an_ifc_Ref<ifc_NameIndex>>,
                                             an_ifc_module::decl_name_visitor>
{
  using base = an_ifc_module::Decl_value_visitor<
                                             Opt<an_ifc_Ref<ifc_NameIndex>>,
                                             an_ifc_module::decl_name_visitor>;
  using base::base;

  template<typename an_ifc_DeclSort_T>
  inline auto visit(an_ifc_DeclSort_T *decl) -> Opt<an_ifc_Ref<ifc_NameIndex>>
    { return ifc_mod->get_ifc_name(decl); }
};  /* decl_name_visitor */


/* An implementation of Decl_value_visitor with the visit function "overridden"
   to facilitate retrieval of a declaration's locus. */
struct an_ifc_module::decl_locus_visitor
  : public an_ifc_module::Decl_value_visitor<ifc_SourceLocation,
                                             an_ifc_module::decl_locus_visitor>
{
  using base = an_ifc_module::Decl_value_visitor<
                                            ifc_SourceLocation,
                                            an_ifc_module::decl_locus_visitor>;
  using base::base;

  template<typename an_ifc_DeclSort_T>
  inline auto visit(an_ifc_DeclSort_T *decl) -> ifc_SourceLocation
    { return ifc_mod->get_ifc_locus(decl); }
};  /* decl_locus_visitor */


/* An implementation of Decl_value_visitor with the visit function "overridden"
   to facilitate retrieval of a declaration's home scope decl. */
struct an_ifc_module::decl_home_scope_decl_visitor
  : public an_ifc_module::Decl_value_visitor<
                                   Opt<an_ifc_Ref<ifc_DeclIndex>>,
                                   an_ifc_module::decl_home_scope_decl_visitor>
{
  using base = an_ifc_module::Decl_value_visitor<
                                  Opt<an_ifc_Ref<ifc_DeclIndex>>,
                                  an_ifc_module::decl_home_scope_decl_visitor>;
  using base::base;

  template<typename an_ifc_DeclSort_T>
  inline auto visit(an_ifc_DeclSort_T *decl) -> Opt<an_ifc_Ref<ifc_DeclIndex>>
    { return ifc_mod->get_ifc_home_scope_decl(decl); }
};  /* decl_home_scope_decl_visitor */


/* An implementation of Decl_value_visitor with the visit function "overridden"
   to facilitate retrieval of a declaration's access. */
struct an_ifc_module::decl_access_visitor
  : public an_ifc_module::Decl_value_visitor<
                                            Opt<ifc_Access>,
                                            an_ifc_module::decl_access_visitor>
{
  using base = an_ifc_module::Decl_value_visitor<
                                           Opt<ifc_Access>,
                                           an_ifc_module::decl_access_visitor>;
  using base::base;

  template<typename an_ifc_DeclSort_T>
  inline auto visit(an_ifc_DeclSort_T *decl) -> Opt<ifc_Access>
    { return ifc_mod->get_ifc_access(decl); }
};  /* decl_access_visitor */


static a_boolean is_home_scope_specialization_wrapper(ifc_DeclIndex decl)
/*
As of IFC 0.41, which introduced DeclSort::Specialization, some home scopes
point to the specialization declaration, rather than the associated
DeclSort::Scope.  Return TRUE if the given DeclIndex represents a
specialization and should be unwrapped; otherwise, return FALSE.
*/
{
  return decl_tag(decl) == ifc_DeclSort_Specialization;
}  /* is_specialization_wrapper */


template<typename an_ifc_DeclSort_T>
static Opt<an_ifc_Ref<ifc_NameIndex>> get_ifc_name_from_scope(
                                                       an_ifc_module     *mod,
                                                       an_ifc_DeclSort_T *decl)
/*
Return the name of the enclosing scope of the declaration represented at decl.
*/
{
  Opt<an_ifc_Ref<ifc_DeclIndex>> opt_scope =
                                            mod->get_ifc_home_scope_decl(decl);
  Opt<an_ifc_Ref<ifc_NameIndex>> result;

  if (opt_scope.has_value()) {
    an_ifc_Ref<ifc_DeclIndex> scope_ref = *opt_scope;

    result = scope_ref.mod->get_ifc_name(scope_ref.index);
  }  /* if */
  return result;
}  /* get_ifc_name_from_scope */


template<>
inline Opt<an_ifc_Ref<ifc_NameIndex>> an_ifc_module::get_ifc_name(
                                             an_ifc_DeclSort_Constructor *decl)
/*
Return the name of the declaration represented at decl.
*/
{
  Opt<an_ifc_Ref<ifc_NameIndex>> result;

  /* Constructors are named by their enclosing scope.  Specializations require
     special handling as the name of the associated templated entity is
     corrupted at the time of writing in all IFCs.  get_ifc_home_scope calls
     remove specialization information and return the templated entity instead.
     Thus, if the enclosing scope is a specialization, intercept and handle it
     directly here. */
  if (is_home_scope_specialization_wrapper(decl->home_scope)) {
    result = get_ifc_name(decl->home_scope);
  } else {
    result = get_ifc_name_from_scope(this, decl);
  }  /* if */
  return result;
}  /* get_ifc_name<an_ifc_DeclSort_Constructor> */


template<>
inline Opt<an_ifc_Ref<ifc_NameIndex>> an_ifc_module::get_ifc_name(
                                              an_ifc_DeclSort_Destructor *decl)
/*
Return the name of the declaration represented at decl.
*/
{
  Opt<an_ifc_Ref<ifc_NameIndex>> result;

  /* Destructors are named by their enclosing scope.  Specializations require
     special handling as the name of the associated templated entity is
     corrupted at the time of writing in all IFCs.  get_ifc_home_scope calls
     remove specialization information and return the templated entity instead.
     Thus, if the enclosing scope is a specialization, intercept and handle it
     directly here. */
  if (is_home_scope_specialization_wrapper(decl->home_scope)) {
    result = get_ifc_name(decl->home_scope);
  } else {
    result = get_ifc_name_from_scope(this, decl);
  }  /* if */
  return result;
}  /* get_ifc_name<an_ifc_DeclSort_Destructor> */


template<>
inline Opt<an_ifc_Ref<ifc_NameIndex>> an_ifc_module::get_ifc_name(
                                   an_ifc_DeclSort_PartialSpecialization *decl)
/*
Return the name of the declaration represented at decl.
*/
{
  /* FIXME: Both the name held by a partial specialization and the name
     held by the associated declaration are mangled.  Thus, we can't use
     the name on the partial specialization, or recurse to get the name
     from the specialized entity.  Pull the name from the primary
     template. */
  return get_ifc_name_from_primary_template(this, decl->form);
}  /* get_ifc_name<an_ifc_DeclSort_PartialSpecialization> */


template<>
inline Opt<an_ifc_Ref<ifc_NameIndex>> an_ifc_module::get_ifc_name(
                                          an_ifc_DeclSort_Specialization *decl)
/*
Return the name of the declaration represented at decl.
*/
{
  /* FIXME: A specialization doesn't hold any name information of its own
     currently, and the name held by the associated declaration is mangled so
     we can't recurse on it.  Pull the name from the primary template. */
  return get_ifc_name_from_primary_template(this, decl->form);
}  /* get_ifc_name<an_ifc_DeclSort_Specialization> */


template<>
inline Opt<an_ifc_Ref<ifc_NameIndex>> an_ifc_module::get_ifc_name(
                                               an_ifc_DeclSort_Reference *decl)
/*
Return the name of the declaration represented at decl.
*/
{
  /* References are a special case where we need to delegate to a foreign
     module. */
  a_module_entity_ptr dmep = get_ifc_decl_from_other_module(decl);
  an_ifc_module       *mod = get_assoc_ifc_module(dmep);

  /* Get the name from the referenced module. */
  return mod->get_ifc_name(mod->decl_index_of(dmep));
}  /* get_ifc_name<an_ifc_DeclSort_Reference> */


template<typename an_ifc_DeclSort_T>
inline Opt<an_ifc_Ref<ifc_NameIndex>> an_ifc_module::get_ifc_name(
                                                       an_ifc_DeclSort_T *decl)
{
  ifc_NameIndex raw_index = get_ifc_name(decl, Overload_priority<1>());

  {
    an_ifc_Ref<ifc_NameIndex> index(this, raw_index);

    return Opt<an_ifc_Ref<ifc_NameIndex>>(index);
  }
}  /* get_ifc_name */

Opt<an_ifc_Ref<ifc_NameIndex>> an_ifc_module::get_ifc_name(
                                                      ifc_DeclIndex decl_index)
/*
Return the name of the declaration represented at decl.
*/
{
  decl_name_visitor name_visitor(this);

  return name_visitor.visit_index(decl_index);
}  /* get_ifc_name */


template<>
inline ifc_SourceLocation an_ifc_module::get_ifc_locus(
                                          an_ifc_DeclSort_Specialization *decl)
/*
Return the locus of the declaration represented at decl.
*/
{
  /* A specialization doesn't hold any source location information of its own
     currently.  Recurse on the associated declaration. */
  return get_ifc_locus(decl->decl);
}  /* get_ifc_locus<an_ifc_DeclSort_Specialization> */


ifc_SourceLocation an_ifc_module::get_ifc_locus(ifc_DeclIndex decl_index)
/*
Return the locus of the declaration represented at decl.
*/
{
  decl_locus_visitor locus_visitor(this);

  return locus_visitor.visit_index(decl_index);
}  /* get_ifc_locus */


static Opt<an_ifc_Ref<ifc_DeclIndex>> skip_scope_abstractions(
                                                an_ifc_Ref<ifc_DeclIndex> decl)
/*
As of IFC 0.41, which introduced DeclSort::Specialization, some home scopes
point to the specialization declaration, rather than the associated
DeclSort::Scope.  Return the proper DeclIndex (which may be the original decl
if there are no intervening abstractions).
*/
{
  Opt<an_ifc_Ref<ifc_DeclIndex>> result = decl;

  if (is_home_scope_specialization_wrapper(decl.index)) {
    Opt<an_ifc_Node<an_ifc_DeclSort_Specialization>> opt_idss;

    construct_node(&opt_idss, decl.mod, decl.index);
    if (opt_idss.has_value()) {
      an_ifc_Ref<ifc_DeclIndex> result_ref(decl.mod, (*opt_idss)->decl);

      check_assertion(decl_tag(result_ref.index) == ifc_DeclSort_Scope);
      result = result_ref;
    } else {
      result = Opt<an_ifc_Ref<ifc_DeclIndex>>();
    }  /* if */
  }  /* if */
  return result;
}  /* skip_scope_abstractions */


template<>
inline Opt<an_ifc_Ref<ifc_DeclIndex>> an_ifc_module::get_ifc_home_scope_decl(
                                   an_ifc_DeclSort_PartialSpecialization *decl)
/*
Return the home scope of the declaration represented at decl.
*/
{
  /* A partial specialization has direct scoping information; however, it's not
     correct.  Recurse on the associated declaration. */
  Opt<an_ifc_Ref<ifc_DeclIndex>> opt_assoc_decl =
                                    get_ifc_home_scope_decl(decl->entity.decl);
  Opt<an_ifc_Ref<ifc_DeclIndex>> result;

  if (opt_assoc_decl.has_value()) {
    result = skip_scope_abstractions(*opt_assoc_decl);
  }  /* if */
  return result;
}  /* get_ifc_home_scope_decl<an_ifc_DeclSort_PartialSpecialization> */


template<>
inline Opt<an_ifc_Ref<ifc_DeclIndex>> an_ifc_module::get_ifc_home_scope_decl(
                                          an_ifc_DeclSort_Specialization *decl)
/*
Return the home scope of the declaration represented at decl.
*/
{
  /* A specialization doesn't have any direct scoping information. Recurse on
     the associated declaration. */
  Opt<an_ifc_Ref<ifc_DeclIndex>> opt_assoc_decl =
                                           get_ifc_home_scope_decl(decl->decl);
  Opt<an_ifc_Ref<ifc_DeclIndex>> result;

  if (opt_assoc_decl.has_value()) {
    result = skip_scope_abstractions(*opt_assoc_decl);
  }  /* if */
  return result;
}  /* get_ifc_home_scope_decl<an_ifc_DeclSort_Specialization> */


template<>
inline Opt<an_ifc_Ref<ifc_DeclIndex>> an_ifc_module::get_ifc_home_scope_decl(
                                               an_ifc_DeclSort_Reference *decl)
/*
Return the home scope of the declaration represented at decl.
*/
{
  /* References are a special case where we need to delegate to a foreign
     module. */
  a_module_entity_ptr dmep = get_ifc_decl_from_other_module(decl);
  an_ifc_module       *mod = get_assoc_ifc_module(dmep);

  /* Get the name from the referenced module. */
  return mod->get_ifc_home_scope_decl(mod->decl_index_of(dmep));
}  /* get_ifc_home_scope_decl<an_ifc_DeclSort_Reference> */


template<typename an_ifc_DeclSort_T>
inline Opt<an_ifc_Ref<ifc_DeclIndex>> an_ifc_module::get_ifc_home_scope_decl(
                                                       an_ifc_DeclSort_T *decl)
/*
Return the home scope of the declaration represented at decl.
*/
{
  ifc_DeclIndex index = get_ifc_home_scope_decl(decl, Overload_priority<1>());

  return skip_scope_abstractions(an_ifc_Ref<ifc_DeclIndex>(this, index));
}  /* get_ifc_home_scope_decl */


Opt<an_ifc_Ref<ifc_DeclIndex>> an_ifc_module::get_ifc_home_scope_decl(
                                                      ifc_DeclIndex decl_index)
/*
Given a declaration's index find and return its home scope decl.
*/
{
  decl_home_scope_decl_visitor home_scope_decl_visitor(this);

  return home_scope_decl_visitor.visit_index(decl_index);
}  /* get_ifc_home_scope_decl */


static Opt<an_ifc_Ref<ifc_DeclIndex>> get_ifc_home_scope_decl(
                                            an_ifc_Ref<ifc_DeclIndex> decl_ref)
/*
Given a declaration reference find and return its home scope decl.
*/
{
  return decl_ref.mod->get_ifc_home_scope_decl(decl_ref.index);
}  /* get_ifc_home_scope_decl */


static inline void ensure_type_has_scope(a_type_ptr tp)
/*
Ensure that the provided type has a scope associated with it that can be used
as the parent scope of a nested entity.  Note that if this type cannot have a
scope associated with it, it will continue to not have an associated scope.
*/
{
  /* FIXME: Do we need to worry about scoped enums here? */
  if (is_class_struct_union_type(tp)) {
    a_class_type_supplement_ptr ctsp;
    ctsp = class_type_supp(tp);
    if (ctsp->assoc_scope == NULL) {
      ctsp->assoc_scope = alloc_placeholder_scope(sck_class_struct_union,
                                                  /*assoc_routine=*/NULL);
    }  /* if */
  }  /* if */
}  /* ensure_type_has_scope */


static a_scope_ptr get_ifc_scope(an_ifc_Ref<ifc_DeclIndex> scope_ref)
/*
Given a scope reference find and return the associated scope.
*/
{
  return scope_ref.mod->get_ifc_scope(scope_ref.index);
}  /* get_ifc_scope */


a_scope_ptr an_ifc_module::get_ifc_scope(ifc_DeclIndex scope_index)
/*
Given a scope index find and return the associated scope.
*/
{
  a_scope_ptr result;

  if (scope_index == 0) {
    result = il_header.primary_scope;
  } else {
    a_module_entity_ptr mep = get_ifc_module_entity_ptr(scope_index);
    a_type_ptr          assoc_type = NULL;
    process_ifc_declaration(mep, /*defer=*/FALSE, /*enumeration_type=*/NULL);
    if (mep->entity.kind == (a_byte_il_entry_kind)iek_type) {
      assoc_type = (a_type_ptr)mep->entity.ptr;
      ensure_type_has_scope(assoc_type);
    }  /* if */
    result = get_assoc_scope_of_il_entry(mep->entity.ptr,
                                         (an_il_entry_kind)mep->entity.kind);
    if (assoc_type != NULL &&
        (scope_is(result, sck_class_struct_union) ||
         scope_is(result, sck_enum) || scope_is(result, sck_func_prototype))) {
      check_assertion(result->variant.assoc_type == NULL ||
                      result->variant.assoc_type == assoc_type);
      result->variant.assoc_type = assoc_type;
    }  /* if */
  }  /* if */
  return result;
}  /* get_ifc_scope */


template<typename an_ifc_DeclSort_T>
inline a_scope_ptr an_ifc_module::get_ifc_home_scope(an_ifc_DeclSort_T *decl)
/*
Return the associated scope for the given declaration.
*/
{
  Opt<an_ifc_Ref<ifc_DeclIndex>> opt_decl_ref;

  opt_decl_ref = get_ifc_home_scope_decl(decl);
  a_scope_ptr result = NULL;
  if (opt_decl_ref.has_value()) {
    result = EDG_PREFIX::get_ifc_scope(*opt_decl_ref);
  }  /* if */
  return result;
}  /* get_ifc_home_scope */


inline a_scope_ptr an_ifc_module::get_ifc_home_scope(ifc_DeclIndex decl_index)
/*
Return the associated scope for the given declaration index.
*/
{
  Opt<an_ifc_Ref<ifc_DeclIndex>> opt_decl_ref =
                                           get_ifc_home_scope_decl(decl_index);
  a_scope_ptr result = NULL;

  if (opt_decl_ref.has_value()) {
    result = EDG_PREFIX::get_ifc_scope(*opt_decl_ref);
  }  /* if */
  return result;
}  /* get_ifc_home_scope */


a_boolean an_ifc_module::is_home_scope_readable(ifc_DeclIndex decl_index)
/*
Return TRUE if the home scope of the declaration (indexed by decl_index) is
loaded and may contain all or part of its associated inner declarations.
*/
{
  a_module_entity_ptr mep = get_ifc_module_entity_ptr(decl_index);
  a_scope_ptr home_scope = mep->scope;
  /* FIXME: We're currently unable to retroactively retrieve a home scope
     for enumerators. Avoid a crash. */
  if (home_scope == NULL && decl_tag(decl_index) != ifc_DeclSort_Enumerator) {
    home_scope = get_ifc_home_scope(decl_index);
  }  /* if */
  return home_scope != NULL && !home_scope->is_placeholder_scope;
}  /* is_home_scope_readable */


template<typename an_ifc_DeclSort_T>
inline Opt<ifc_Access> an_ifc_module::get_ifc_access(an_ifc_DeclSort_T *decl)
/*
Check if the home scope of the declaration represented at decl is a class
scope.  If it is, return the access level of the declaration.
*/
{
  /* Check if the home scope of the given declaration is a class scope.  If it
     is, return the access level of the declaration. */
  Opt<an_ifc_Ref<ifc_DeclIndex>> opt_scope_ref = get_ifc_home_scope_decl(decl);
  Opt<ifc_Access>                result;

  if (opt_scope_ref.has_value() && is_class_scope(*opt_scope_ref)) {
    ifc_Access raw_result = get_ifc_access(decl, Overload_priority<1>());

    /* Always represent no access as an empty optional. */
    if (raw_result != ifc_Access_None) {
      result = raw_result;
    }  /* if */
  }  /* if */
  return result;
}  /* get_ifc_access */


Opt<ifc_Access> an_ifc_module::get_ifc_access(ifc_DeclIndex decl_index)
/*
Given a declaration's index, find and return its access information or none if
the declaration is not in a scope that uses access specifiers.
*/
{
  decl_access_visitor access_visitor(this);

  return access_visitor.visit_index(decl_index);
}  /* get_ifc_access */


static Opt<an_ifc_Ref<ifc_DeclIndex>> get_declaring_class_scope_decl(
                                            an_ifc_Ref<ifc_DeclIndex> decl_ref)
/*
Given a declaration that's a class member, return the scope the declaring class
is a member of.  As an example, if the given declaration is a method of "class
A" in "namespace B", this function returns the IFC declaration index of
"namespace B".
*/
{
  Opt<an_ifc_Ref<ifc_DeclIndex>> opt_declaring_class =
                                 EDG_PREFIX::get_ifc_home_scope_decl(decl_ref);
  Opt<an_ifc_Ref<ifc_DeclIndex>> result;

  if (opt_declaring_class.has_value()) {
    result = EDG_PREFIX::get_ifc_home_scope_decl(*opt_declaring_class);
  }  /* if */
  return result;
}  /* get_declaring_class_scope_decl */


a_boolean an_ifc_module::is_name_qualifiable(ifc_DeclIndex decl_index)
/*
Given a declaration's index, return true if the declaration name
can currently be qualified.
*/
{
  a_boolean result = TRUE;

  switch (decl_tag(decl_index)) {
    case ifc_DeclSort_Parameter:
      /* This declaration can never have its name qualified. */
      result = FALSE;
      break;
    case ifc_DeclSort_Enumerator:
      {
        /* FIXME: This is a hack to work around crashing when an enumerator's
           home scope isn't loaded. */
        a_module_entity_ptr mep = get_ifc_module_entity_ptr(decl_index);
        result = mep->scope != NULL;
      }
      break;
    case ifc_DeclSort_Scope:
      {
        /* Nested scopes should never be qualified. */
        an_ifc_Ref<ifc_DeclIndex>      decl_ref(this, decl_index);
        Opt<an_ifc_Ref<ifc_DeclIndex>> opt_home_scope;

        opt_home_scope = EDG_PREFIX::get_ifc_home_scope_decl(decl_ref);
        if (opt_home_scope.has_value()) {
          result = !is_class_scope(*opt_home_scope);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Method:
    case ifc_DeclSort_Constructor:
    case ifc_DeclSort_Destructor:
    case ifc_DeclSort_Field:
    case ifc_DeclSort_Bitfield:
    case ifc_DeclSort_Property:
      { /* These entities can only exist in a class. Return FALSE if that class
           is a local class, otherwise return TRUE. */
        an_ifc_Ref<ifc_DeclIndex>      decl_ref(this, decl_index);
        Opt<an_ifc_Ref<ifc_DeclIndex>> opt_declaring_class_scope;

        opt_declaring_class_scope = get_declaring_class_scope_decl(decl_ref);
        if (opt_declaring_class_scope.has_value()) {
          ifc_DeclIndex scope_index = opt_declaring_class_scope->index;

          result = decl_tag(scope_index) == ifc_DeclSort_Scope;
        } else {
          /* If retrieval of the declaring class scope failed, assume the
             name isn't qualifiable. */
          result = FALSE;
        } /* if */
      }
      break;
    case ifc_DeclSort_Reference:
      {
        /* This declaration is an indirect reference to an imported
           declaration.  We must consider the import declaration by querying
           the associated foreign module. */
        a_module_entity_ptr dmep = get_ifc_decl_from_other_module(decl_index);
        an_ifc_module       *mod = get_assoc_ifc_module(dmep);

        result = mod->is_name_qualifiable(mod->decl_index_of(dmep));
      }
      break;
    default:
      /* Assume the name can be qualified. */
      break;
  }  /* switch */
  /* If name qualification is globally suppressed, this declaration's name
     cannot be qualified. */
  if (suppress_automatic_name_qualification) {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_name_qualifiable */


static a_calling_convention conv_calling_convention(
                                              ifc_CallingConvention convention)
/*
Convert the given IFC calling convention to the corresponding EDG one.
*/
{
  a_calling_convention conv;

  switch (convention) {
    case ifc_CallingConvention_Cdecl:   conv = cc_cdecl; break;
    case ifc_CallingConvention_Fast:    conv = cc_fastcall; break;
    case ifc_CallingConvention_Std:     conv = cc_stdcall; break;
    case ifc_CallingConvention_This:    conv = cc_thiscall; break;
    case ifc_CallingConvention_Clr:     conv = cc_clrcall; break;
    case ifc_CallingConvention_Vector:  conv = cc_vectorcall; break;
    case ifc_CallingConvention_Eabi:    /* conv = cc_eabicall; break; */
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_calling_conv,
                        &error_position, "CallingConvention::Eabi");
      conv = cc_default;
      break;
    default_is_unexpected_str("Unexpected CallingConvention");
  }  /* switch */
  return conv;
}  /* conv_calling_convention */


static a_type_ptr type_for_type_index(an_ifc_Ref<ifc_TypeIndex>      type_ref,
                                      an_ifc_module::a_non_type_kind *kind)
/*
Return the type that corresponds to the specified type reference.  If there is
no corresponding type, set *kind to the appropriate non-type kind and return
NULL.
*/
{
  /* Disable spurious GCC warning about uninitialized usage of gmf_decl_type
     (when this function is called by name_from_local_decl). */
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
  return type_ref.mod->type_for_type_index(type_ref.index, kind);
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
}  /* type_for_type_index */


a_type_ptr an_ifc_module::type_for_type_index(ifc_TypeIndex   type_index,
                                              a_non_type_kind *kind)
/*
Return the type that corresponds to the specified TypeIndex.  If there is no
corresponding type, set *kind to the appropriate non-type kind and return NULL.
*/
{
  a_type_ptr          result = NULL;
  a_module_entity_ptr mep = get_ifc_module_entity_ptr(type_index);
  ifc_TypeSort        tag;
  Value_saver<a_module_entity_ptr>
                      mep_saver(&curr_module_entity);

  curr_module_entity = NULL;
  if (kind != NULL) {
    *kind = ntk_none;
  }  /* if */
  if (mep->entity.ptr != NULL) {
    /* There is already an entry for this; return it. */
    check_assertion(mep->entity.kind == iek_type);
    result = (a_type_ptr)mep->entity.ptr;
  } else {
    /* Prepare to read from the proper partition for this type. */
    read_prechecked_partition_element(mep);
    tag = (ifc_TypeSort)get_tag_from_partition(mep->variant.ifc_partition,
                                               ifc_type_start);
    switch (tag) {
      case ifc_TypeSort_Fundamental:
        { an_integer_kind             ik;
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          itsfp = get_TypeSort_Fundamental(&itsf);
          /* Note: no check is made for nonsensical types (e.g., signed
             void). */
          switch (itsfp->basis) {
            case ifc_TypeBasis_Void:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = void_type();
              break;
            case ifc_TypeBasis_Bool:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = bool_type();
              break;
            case ifc_TypeBasis_Char:
              switch (itsfp->sign) {
                case ifc_TypeSign_Plain:
                  switch (itsfp->precision) {
                    case ifc_TypePrecision_Default:
                      result = integer_type((an_integer_kind)ik_char);
                      break;
                    case ifc_TypePrecision_Bit8:
                      result = char8_t_type();
                      break;
                    case ifc_TypePrecision_Bit16:
                      result = char16_t_type();
                      break;
                    case ifc_TypePrecision_Bit32:
                      result = char32_t_type();
                      break;
                    default:
                      unexpected_condition();
                  }  /* switch */
                  break;
                case ifc_TypeSign_Signed:
                  check_assertion(itsfp->precision ==
                                                    ifc_TypePrecision_Default);
                  result = integer_type((an_integer_kind)ik_signed_char);
                  break;
                case ifc_TypeSign_Unsigned:
                  check_assertion(itsfp->precision ==
                                                    ifc_TypePrecision_Default);
                  result = integer_type((an_integer_kind)ik_unsigned_char);
                  break;
                default_is_unexpected();
              }  /* if */
              break;
            case ifc_TypeBasis_Wchar_t:
              switch (itsfp->precision) {
                case ifc_TypePrecision_Default:
                  result = wchar_t_type();
                  break;
                case ifc_TypePrecision_Bit8:
                  result = char8_t_type();
                  break;
                case ifc_TypePrecision_Bit16:
                  result = char16_t_type();
                  break;
                case ifc_TypePrecision_Bit32:
                  result = char32_t_type();
                  break;
                case ifc_TypePrecision_Short:
                case ifc_TypePrecision_Long:
                case ifc_TypePrecision_Bit64:
                case ifc_TypePrecision_Bit128:
                  unexpected_condition_str("Unexpected precision for wchar_t");
                  break;
                default_is_unexpected();
              }  /* switch */
              break;
            case ifc_TypeBasis_Int:
              switch (itsfp->precision) {
                case ifc_TypePrecision_Default:
                  ik = (itsfp->sign == ifc_TypeSign_Unsigned) ?
                                                              ik_unsigned_int :
                                                              ik_int;
                  break;
                case ifc_TypePrecision_Short:
                  ik = (itsfp->sign == ifc_TypeSign_Unsigned) ?
                                                            ik_unsigned_short :
                                                            ik_short;
                  break;
                case ifc_TypePrecision_Long:
                  ik = (itsfp->sign == ifc_TypeSign_Unsigned) ?
                                                             ik_unsigned_long :
                                                             ik_long;
                  break;
                case ifc_TypePrecision_Bit8:
                  ik = int_kind_for_bit_size(
                                         8,
                                         itsfp->sign != ifc_TypeSign_Unsigned);
                  break;
                case ifc_TypePrecision_Bit16:
                  ik = int_kind_for_bit_size(
                                         16,
                                         itsfp->sign != ifc_TypeSign_Unsigned);
                  break;
                case ifc_TypePrecision_Bit32:
                  ik = int_kind_for_bit_size(
                                         32,
                                         itsfp->sign != ifc_TypeSign_Unsigned);
                  break;
                case ifc_TypePrecision_Bit64:
                  ik = int_kind_for_bit_size(
                                         64,
                                         itsfp->sign != ifc_TypeSign_Unsigned);
                  break;
                case ifc_TypePrecision_Bit128:
                  ik = int_kind_for_bit_size(
                                         128,
                                         itsfp->sign != ifc_TypeSign_Unsigned);
                  break;
                default_is_unexpected();
              }  /* switch */
              check_assertion(ik != (an_integer_kind)ik_none);
              result = integer_type(ik);
              break;
            case ifc_TypeBasis_Float:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = float_type((a_float_kind)fk_float);
              break;
            case ifc_TypeBasis_Double:
              if (itsfp->precision == ifc_TypePrecision_Long) {
                result = float_type((a_float_kind)fk_long_double);
              } else {
                check_assertion(itsfp->precision == ifc_TypePrecision_Default);
                result = float_type((a_float_kind)fk_double);
              }  /* if */
              break;
            case ifc_TypeBasis_Nullptr:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = standard_nullptr_type();
              break;
            case ifc_TypeBasis_Ellipsis:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              check_assertion(kind != NULL);
              /*lint -e413 likely use of null pointer*/
              *kind = ntk_ellipsis;
              result = NULL;
              break;
            case ifc_TypeBasis_Class:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = alloc_type((a_type_kind)tk_class);
              break;
            case ifc_TypeBasis_Struct:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = alloc_type((a_type_kind)tk_struct);
              break;
            case ifc_TypeBasis_Union:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = alloc_type((a_type_kind)tk_union);
              break;
            case ifc_TypeBasis_Auto:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = make_auto_type(&null_source_position,
                                      /*is_decltype_auto=*/FALSE);
              break;
            case ifc_TypeBasis_DecltypeAuto:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = make_auto_type(&null_source_position,
                                      /*is_decltype_auto=*/TRUE);
              break;
            case ifc_TypeBasis_Namespace:
              check_assertion(kind != NULL);
              /*lint -e413 likely use of null pointer*/
              *kind = ntk_namespace;
              result = NULL;
              break;
            case ifc_TypeBasis_Interface:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = alloc_type((a_type_kind)tk_struct);
              result->variant.class_struct_union.is_interface = TRUE;
              break;
            case ifc_TypeBasis_Enum:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              /* FIXME: Currently unsupported. */
              issue_unsupported_node_diag("TypeBasis::Enum", &error_position);
              result = error_type();
              break;
            case ifc_TypeBasis_Typename:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = unknown_type();
              break;
            case ifc_TypeBasis_SegmentType:
              /* FIXME: Currently unsupported. */
              issue_unsupported_node_diag("TypeBasis::SegmentType",
                                          &error_position);
              result = error_type();
              break;
            case ifc_TypeBasis_Function:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = make_routine_type(unknown_type(), /*param1=*/NULL,
                                         /*param2=*/NULL, /*param3=*/NULL,
                                         /*param4=*/NULL);
              break;
            case ifc_TypeBasis_Empty:
              check_assertion(kind != NULL);
              /*lint -e413 likely use of null pointer*/
              *kind = ntk_empty_pack_expansion;
              break;
            case ifc_TypeBasis_VariableTemplate:
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              result = type_of_unknown_templ_param_nontype;
              break;
            default_is_unexpected_str("Unexpected TypeBasis kind");
          }  /* switch */
        }
        break;
      case ifc_TypeSort_Qualified:
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
          result = make_qualified_type(type_for_type_index(itsqp->unqualified,
                                                           /*kind=*/NULL),
                                       qualifiers);
        }
        break;
      case ifc_TypeSort_Pointer:
        { an_ifc_TypeSort_Pointer itsp, *itspp;
          itspp = get_TypeSort_Pointer(&itsp);
          result = make_pointer_type(type_for_type_index(itspp->pointee,
                                                         /*kind=*/NULL));
        }
        break;
      case ifc_TypeSort_LvalueReference:
        { an_ifc_TypeSort_LvalueReference itslr, *itslrp;
          itslrp = get_TypeSort_LvalueReference(&itslr);
          result = make_reference_type(type_for_type_index(itslrp->referee,
                                                           /*kind=*/NULL));
        }
        break;
      case ifc_TypeSort_RvalueReference:
        { an_ifc_TypeSort_RvalueReference itsrr, *itsrrp;
          itsrrp = get_TypeSort_RvalueReference(&itsrr);
          result = make_rvalue_reference_type(
                                           type_for_type_index(itsrrp->referee,
                                                               /*kind=*/NULL));
        }
        break;
      case ifc_TypeSort_Array:
        { an_ifc_TypeSort_Array itsa, *itsap;
          a_constant_ptr        elem_count;
          a_boolean             err = FALSE;
          itsap = get_TypeSort_Array(&itsa);
          result = alloc_type((a_type_kind)tk_array);
          result->variant.array.element_type =
                                           type_for_type_index(itsap->element,
                                                               /*kind=*/NULL);
          elem_count = constant_for_expr_index(itsap->extent,
                                               /*default_type=*/NULL);
          check_assertion(elem_count->kind== (a_constant_repr_kind)ck_integer);
          result->variant.array.variant.number_of_elements =
                          unsigned_value_of_integer_constant(elem_count, &err);
          check_assertion(!err);
          set_array_type_size(result, /*suppress_error=*/FALSE);
        }
        break;
      case ifc_TypeSort_Method:
        { an_ifc_TypeSort_Method itsm;
          get_TypeSort_Method(&itsm);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("TypeSort::Method", &error_position);
          result = error_type();
        }
        break;
      case ifc_TypeSort_Function:
        { an_ifc_TypeSort_Function itsf, *itsfp;
          itsfp = get_TypeSort_Function(&itsf);
          /* Create a routine type with no parameters to start. */
          result = make_routine_type(type_for_type_index(itsfp->target,
                                                         /*kind=*/NULL),
                                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                                     (a_type_ptr)NULL, (a_type_ptr)NULL);
          result->variant.routine.extra_info->calling_convention =
                                    conv_calling_convention(itsfp->convention);
          /* FIXME: need a thorough review of this. */
          if (itsfp->source != 0) {
            /* The function has parameters. */
            unsigned int                  i;
            a_routine_type_supplement_ptr rtsp =
                                            result->variant.routine.extra_info;
            a_param_type_ptr              ptp, *prev = &rtsp->param_type_list;
            an_ifc_TypeSort_Tuple         itst, *itstp;
            a_type_ptr                    param_type;
            a_non_type_kind               non_type_kind;

            if (type_tag(itsfp->source) == ifc_TypeSort_Tuple) {
              /* A list of parameters. */
              read_prechecked_partition_element(ifc_type_tuple,
                                                type_value(itsfp->source));
              itstp = get_TypeSort_Tuple(&itst);
              for (i = 0; i < itstp->cardinality; i++) {
                ifc_TypeIndex ti;
                read_prechecked_partition_element(ifc_heap_type,
                                        itstp->start + i);
                GET_TypeIndex(ti, /*from_header=*/FALSE);
                param_type = type_for_type_index(ti, &non_type_kind);
                if (param_type == NULL) {
                  /* This happens when an ellipsis is present as the last
                     parameter. */
                  check_assertion(non_type_kind == ntk_ellipsis &&
                                  i == itstp->cardinality-1);
                  rtsp->has_ellipsis = TRUE;
                  break;
                }  /* if */
                ptp = make_param_type(param_type, &null_source_position);
                update_param_top_level_qualifiers(ptp);
                ptp->param_num = i + 1;
                *prev = ptp;
                prev = &ptp->next;
              }  /* for */
            } else {
              /* A single parameter. */
              param_type = type_for_type_index(itsfp->source, &non_type_kind);
              if (param_type == NULL) {
                /* A single ellipsis parameter. */
                check_assertion(non_type_kind == ntk_ellipsis);
                rtsp->has_ellipsis = TRUE;
              } else {
                rtsp->param_type_list = make_param_type(param_type,
                                                        &null_source_position);
                update_param_top_level_qualifiers(rtsp->param_type_list);
              }  /* if */
            }  /* if */
          }  /* if */
        }
        break;
      case ifc_TypeSort_Designated:
        /* A type's name (e.g., "A"). */
        { an_ifc_TypeSort_Designated itsd, *itsdp;
          a_module_entity_ptr        dmep;
          ifc_DeclSort               dsort;

          itsdp = get_TypeSort_Designated(&itsd);
          dsort = decl_tag(itsdp->decl);
          switch (dsort) {
            case ifc_DeclSort_Reference:
              dmep = get_and_process_ifc_decl_from_other_module(itsdp->decl);
              result = (a_type_ptr)dmep->entity.ptr;
              check_assertion(result != NULL && dmep->entity.kind == iek_type);
              break;
            case ifc_DeclSort_Scope:
            case ifc_DeclSort_Enumeration:
              /* Find the type of the scope declaration by processing it (in
                 case it has been deferred). */
              dmep = get_ifc_module_entity_ptr(itsdp->decl);
              process_ifc_declaration(dmep, /*defer=*/FALSE, (a_type_ptr)NULL);
              result = (a_type_ptr)dmep->entity.ptr;
              check_assertion(result != NULL && dmep->entity.kind == iek_type);
              break;
            case ifc_DeclSort_Parameter:
              /* FIXME: Is this correct, or are we expected to try to resolve
                 the type in the case of template parameters? */
              { an_ifc_DeclSort_Parameter idsp, *idspp;
                read_prechecked_partition_element(itsdp->decl);
                idspp = get_DeclSort_Parameter(&idsp);
                result = type_for_type_index(idspp->type, /*kind=*/NULL);
              }
              break;
            default:
              unexpected_condition_str("Unexpected DeclSort for "
                                       "TypeSort::Designated");
          }  /* switch */
        }
        break;
      case ifc_TypeSort_Tor:
        /* This type should only be encountered when processing a constructor,
           and that is directly handled with that constructor declaration.*/
        unexpected_condition();
        break;
      case ifc_TypeSort_Placeholder:
        { an_ifc_TypeSort_Placeholder itsp, *itspp;
          itspp = get_TypeSort_Placeholder(&itsp);
          switch (itspp->basis) {
            case ifc_TypeBasis_Auto:
            case ifc_TypeBasis_DecltypeAuto:
              result = make_auto_type(&null_source_position,
                                      itspp->basis ==
                                                   ifc_TypeBasis_DecltypeAuto);
              break;
            default:
              result = error_type();
              unexpected_condition_str("Unexpected TypeBasis for "
                                       "TypeSort::Placeholder");
          }  /* switch */
        }
        break;
      case ifc_TypeSort_PointerToMember:
        { an_ifc_TypeSort_PointerToMember itsptm;
          get_TypeSort_PointerToMember(&itsptm);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("TypeSort::PointerToMember",
                                      &error_position);
          result = error_type();
        }
        break;
      case ifc_TypeSort_Tuple:
        { an_ifc_TypeSort_Tuple itst;
          get_TypeSort_Tuple(&itst);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("TypeSort::Tuple", &error_position);
          result = error_type();
        }
        break;
      case ifc_TypeSort_Forall:
        { an_ifc_TypeSort_Forall itsfa;
          get_TypeSort_Forall(&itsfa);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("TypeSort::Forall", &error_position);
          result = error_type();
        }
        break;
      case ifc_TypeSort_VendorExtension:
        { an_ifc_TypeSort_VendorExtension itsve;
          get_TypeSort_VendorExtension(&itsve);
          issue_unsupported_node_diag("TypeSort::VendorExtension",
                                      &error_position);
          result = error_type();
        }
        break;
      case ifc_TypeSort_Syntactic:
        { an_ifc_TypeSort_Syntactic itss, *itssp;
          ifc_ExprSort etag;
          itssp = get_TypeSort_Syntactic(&itss);
          etag = expr_tag(itssp->expr);
          read_prechecked_partition_element(itssp->expr);
          switch (etag) {
            case ifc_ExprSort_TemplateId:
              { an_ifc_ExprSort_TemplateId iestid, *iestidp;
                iestidp = get_ExprSort_TemplateId(&iestid);
                result = type_for_template_id(iestidp);
              }
              break;
            default:
              unexpected_condition_str("Unexpected ExprSort kind for "
                                       "TypeSort::Syntactic.");
          }  /* switch */
        }
        break;
      case ifc_TypeSort_Expansion:
        { an_ifc_TypeSort_Expansion itse;
          get_TypeSort_Expansion(&itse);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("TypeSort::Expansion", &error_position);
          result = error_type();
        }
        break;
      case ifc_TypeSort_Typename:
        { an_ifc_TypeSort_Typename itstn;
          get_TypeSort_Typename(&itstn);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("TypeSort::Typename", &error_position);
          result = error_type();
        }
        break;
      case ifc_TypeSort_Base:
        { an_ifc_TypeSort_Base itsb;
          get_TypeSort_Base(&itsb);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("TypeSort::Base", &error_position);
          result = error_type();
        }
        break;
      case ifc_TypeSort_Unaligned:
        { an_ifc_TypeSort_Unaligned itsu;
          get_TypeSort_Unaligned(&itsu);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("TypeSort::Unaligned", &error_position);
          result = error_type();
        }
        break;
      case ifc_TypeSort_Decltype:
        { an_ifc_TypeSort_Decltype itsd;
          get_TypeSort_Decltype(&itsd);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("TypeSort::Decltype", &error_position);
          result = error_type();
        }
        break;
      case ifc_TypeSort_SyntaxTree:
        { an_ifc_TypeSort_SyntaxTree itsst;
          get_TypeSort_SyntaxTree(&itsst);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("TypeSort::Syntaxtree", &error_position);
          result = error_type();
        }
        break;
      case ifc_TypeSort_Last:
        unexpected_condition();
        break;
      default_is_unexpected_str("Unexpected TypeSort");
    }  /* switch */
    /* Record this mapping for future reference. */
    if (result != NULL) {
      mep->scope = result->source_corresp.parent_scope;
    }  /* if */
    mep->entity.ptr = (char *)result;
    mep->entity.kind = iek_type;
  }  /* if */
  return result;
}  /* type_for_type_index */


a_template_arg_ptr an_ifc_module::template_arg_for_expr(
                                           a_template_parameter_ptr param,
                                           ifc_ExprIndex            expr_index)
/*
Given an IFC expression index, construct and return a corresponding template
argument.
*/
{
  a_template_arg_ptr result;
  a_templ_arg_kind   kind;
  a_type_ptr         type;
  a_constant_ptr     cp;
  ifc_ExprSort       tag = expr_tag(expr_index);

  read_prechecked_partition_element(expr_index);
  switch (tag) {
    case ifc_ExprSort_Type:
      { an_ifc_ExprSort_Type iest, *iestp;
        iestp = get_ExprSort_Type(&iest);
        kind = (a_templ_arg_kind)tak_type;
        type = type_for_type_index(iestp->denotation, /*kind=*/NULL);
      }
      break;
    case ifc_ExprSort_UnaryFold:
      { an_ifc_ExprSort_UnaryFold iesuf;
        get_ExprSort_UnaryFold(&iesuf);
        /* FIXME: Currently unsupported. */
        issue_unsupported_node_diag("ExprSort::UnaryFold", &error_position);
        kind = (a_templ_arg_kind)tak_type;
        type = error_type();
      }
      break;
    case ifc_ExprSort_PackedTemplateArguments:
      { an_ifc_ExprSort_PackedTemplateArguments iespta;
        get_ExprSort_PackedTemplateArguments(&iespta);
        /* FIXME: Currently unsupported. */
        issue_unsupported_node_diag("ExprSort::PackedTemplateArguments",
                                    &error_position);
        kind = (a_templ_arg_kind)tak_type;
        type = error_type();
      }
      break;
    case ifc_ExprSort_Read:
      { an_ifc_ExprSort_Read    iesr, *iesrp;
        a_token_cache           cache;
        a_source_position       pos;
        a_curr_token_preserver  guard;

        iesrp = get_ExprSort_Read(&iesr);
        kind = tak_nontype;
        /* Use the parameter type instead of the ExprSort::Read type, as the
           latter may not match (e.g., int& parameter type, int ExprSort::Read
           type). */
        check_assertion(param->kind == (a_template_parameter_kind)tpk_nontype);
        type = param->variant.nontype.constant->type;
        source_position_from_locus(&pos, &iesrp->locus);
        clear_token_cache(&cache, /*reusable=*/FALSE);
        cache_expr(&cache, iesrp->address);
        /* FIXME: Do we need to handle iesrp->sort here? */
        terminate_token_cache(&cache);
        rescan_cached_tokens(&cache);
        cp = fs_constant((a_constant_repr_kind)ck_error);
        scan_template_argument_constant_expression(type, cp);
        check_assertion(curr_token == tok_end_of_source);
        (void)get_token();
      }
      break;
    case ifc_ExprSort_Monad:
      { an_ifc_ExprSort_Monad   iesm, *iesmp;
        a_token_cache           cache;
        a_source_position       pos;
        a_curr_token_preserver  guard;

        iesmp = get_ExprSort_Monad(&iesm);
        kind = tak_nontype;
        type = type_for_type_index(iesmp->type, /*kind=*/NULL);
        source_position_from_locus(&pos, &iesmp->locus);
        clear_token_cache(&cache, /*reusable=*/FALSE);
        cache_operator(&cache, iesmp->assoc, &iesmp->locus);
        cache_token(&cache, tok_lparen, &pos);
        cache_expr(&cache, iesmp->argument);
        cache_token(&cache, tok_rparen, &pos);
        terminate_token_cache(&cache);
        rescan_cached_tokens(&cache);
        cp = fs_constant((a_constant_repr_kind)ck_error);
        scan_template_argument_constant_expression(type, cp);
        check_assertion(curr_token == tok_end_of_source);
        (void)get_token();
      }
      break;
    case ifc_ExprSort_NamedDecl:
    case ifc_ExprSort_Literal:
      kind = tak_nontype;
      check_assertion(param->kind == (a_template_parameter_kind)tpk_nontype);
      type = param->variant.nontype.constant->type;
      cp = constant_for_expr_index(expr_index, type);
      break;
    default:
      unexpected_condition_str("Unexpected expr kind for template arg");
  } /* switch */
  result = alloc_template_arg(kind);
  if (kind == (a_templ_arg_kind)tak_type) {
    result->variant.type = type;
  } else if (kind == (a_templ_arg_kind)tak_nontype) {
    result->variant.constant = cp;
  }  /* if */
  return result;
}  /* template_arg_for_expr */


a_type_ptr an_ifc_module::type_for_template_id(
                                         an_ifc_ExprSort_TemplateId *templ_id)
/*
Returns the type that corresponds to the provided ExprSort::TemplateId in the
module file.
*/
{
  a_type_ptr         result = NULL;
  a_symbol_ptr       inst_sym;
  a_template_ptr     tmpl;
  a_template_arg_ptr arg_list = NULL, *next_arg = &arg_list;
  a_template_parameter_ptr
                     param_list = NULL;
  a_source_position  pos;
  ifc_ExprSort       arg_tag = expr_tag(templ_id->arguments);
  an_ifc_ExprSort_NamedDecl
                     iesnd, *iesndp;

  source_position_from_locus(&pos, &templ_id->locus);
  read_prechecked_partition_element(templ_id->primary);
  check_assertion(expr_tag(templ_id->primary) == ifc_ExprSort_NamedDecl);
  iesndp = get_ExprSort_NamedDecl(&iesnd);
  if (iesndp->type != 0) {
    result = type_for_type_index(iesndp->type, /*kind=*/NULL);
    tmpl = NULL;
    unexpected_condition_str("Unexpected type for ExprSort::NamedDecl");
  } else {
    a_module_entity_ptr mep = get_ifc_module_entity_ptr(iesndp->resolution);
    process_ifc_declaration(mep, /*defer=*/FALSE, (a_type_ptr)NULL);
    check_assertion(mep->entity.kind == iek_template);
    tmpl = (a_template_ptr)mep->entity.ptr;
  }  /* if */
  if (templ_id->arguments != 0) {
    read_prechecked_partition_element(templ_id->arguments);
    check_assertion(tmpl != NULL);
    param_list = tmpl->template_decl->param_list;
    if (arg_tag == ifc_ExprSort_Tuple) {
      an_ifc_ExprSort_Tuple iest, *iestp;
      iestp = get_ExprSort_Tuple(&iest);
      for (ifc_Index_type idx = 0; idx < iestp->cardinality; ++idx) {
        ifc_ExprIndex expr_index =
                       (ifc_ExprIndex)read_index_from_heap(ifc_heap_expr,
                                                           iestp->start + idx);
        check_assertion(param_list != NULL);
        *next_arg = template_arg_for_expr(param_list, expr_index);
        next_arg = &(*next_arg)->next;
        param_list = param_list->next;
      }  /* for */
    } else {
      check_assertion(param_list != NULL);
      *next_arg = template_arg_for_expr(param_list, templ_id->arguments);
      next_arg = &(*next_arg)->next;
    }  /* if */
  }  /* if */
  switch (tmpl->kind) {
    case templk_function:
    case templk_member_function:
      inst_sym = find_template_function(symbol_for(tmpl), &arg_list,
                                        /*explicit_arg_list_present=*/FALSE,
                                        &pos);
      result = inst_sym->variant.routine.ptr->type;
      break;
    case templk_class:
    case templk_member_class:
    case templk_member_enum:
      inst_sym = find_template_class(symbol_for(tmpl), &arg_list,
                                     /*any_prototype_allowed=*/FALSE,
                                     /*specific_prototype_allowed=*/NULL,
                                     /*instantiation_nonreal=*/FALSE,
                                     /*do_not_create=*/FALSE,
                                     /*in_substitution=*/FALSE);
      result = inst_sym->variant.class_struct_union.type;
      break;
    case templk_variable:
    case templk_static_data_member:
      inst_sym = find_template_variable(symbol_for(tmpl), &arg_list,
                                        /*prototype_allowed=*/TRUE,
                                        /*is_use=*/FALSE, /*diagnose=*/TRUE);
      result = inst_sym->variant.variable.ptr->type;
      break;
    case templk_concept:
      check_assertion(arg_list == NULL);
      result = tmpl->prototype_instantiation.constraint->type;
      break;
    case templk_template_template_param:
    case templk_none:
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* type_for_template_id */


a_boolean an_ifc_module::source_position_from_locus(
                                               a_source_position        *pos,
                                               const ifc_SourceLocation *locus)
/*
Map the IFC locus source position information into the source position at pos.
Return TRUE if processing succeeded, otherwise return FALSE.
*/
{
  a_boolean            result = TRUE;
  an_ifc_Source_Line   isl, *islp;

  if (!read_partition_element(ifc_src_line, locus->line)) {
    result = FALSE;
    goto done;
  }  /* if */
  islp = get_Source_Line(&isl);
  check_assertion(name_tag(islp->file) == ifc_NameSort_SourceFile);
  if (islp->line == 0) {
    /* IFC LineNumbers start at one, a line number of zero means the source
       line isn't known.  As the front end (at the time of writing) doesn't
       support source locations for a file without a line, resolve all cases of
       this to null source position. */
    *pos = null_source_position;
  } else {
    /* See if this file has been used before. */
    a_module_sequence_number_mapping *msnmp =
                                     &sequence_numbers[name_value(islp->file)];
    if (msnmp->starting_sequence_number == 0) {
      /* First time accessing this source file; record the start of a new
         source file.  Note that this may be out-of-order as it depends on the
         order that entities are used, but the full tree of source file
         references isn't available in the IFC file. */
      a_const_char *file_name;
      reset_text_buffer(file_name_buffer);
      file_name = string_from_name_index(islp->file,
                                         (a_symbol_locator *)NULL,
                                         &file_name_buffer);
      file_name = copy_string_to_region(FILE_SCOPE_REGION_NUMBER, file_name);
      record_inclusion_of_module_source_file(file_name, pos, assoc_module_info,
                                             msnmp->max_line_number);
      msnmp->starting_sequence_number = pos->seq;
    }  /* if */
    check_assertion(islp->line <= msnmp->max_line_number);
    pos->seq = msnmp->starting_sequence_number + islp->line;
    /* Add one to map 0-based IFC column numbers to 1-based EDG numbers. */
    pos->column = locus->column+1;
#if FULLY_RESOLVED_MACRO_POSITIONS
    pos->orig_seq = pos->seq;
    pos->orig_column = pos->column;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  }  /* if */
done:
  return result;
}  /* source_position_from_locus */


static a_const_char *string_from_name_ref(
                                      const an_ifc_Ref<ifc_NameIndex> name_ref,
                                      a_symbol_locator                *loc)
/*
Return the string referenced by name_ref.  The returned string is guaranteed to
be in long-lived memory (either via pointing to IL, a constant, the memory
mapping if enabled, or in the worst case an allocated pointer).  If non-NULL,
fields (like is_operator_name) in *loc are updated accordingly.
*/
{
  return name_ref.mod->string_from_name_index(name_ref.index, loc);
}  /* string_from_name_ref */


a_const_char *an_ifc_module::string_from_name_index(
                                                  ifc_NameIndex     name_index,
                                                  a_symbol_locator  *loc)
/*
Return the string referenced by name_index.  The returned string is guaranteed
to be in long-lived memory (either via pointing to IL, a constant, the memory
mapping if enabled, or in the worst case an allocated pointer).  If non-NULL,
fields (like is_operator_name) in *loc are updated accordingly.
*/
{
  /* Do not provide a buffer, forcing one to be dynamically allocated if
     necessary.  Note that this will "leak" the buffer until the buffer list is
     cleaned up. */
  /* FIXME: Ideally there'd be some sort of "pool" of buffers that can be
     recycled to handle common allocation profiles. */
  a_text_buffer_ptr result_buffer = NULL;
  return string_from_name_index(name_index, loc, &result_buffer);
}  /* string_from_name_index */


a_const_char *an_ifc_module::string_from_name_index(
                                              ifc_NameIndex     name_index,
                                              a_symbol_locator  *loc,
                                              a_text_buffer_ptr *result_buffer)
/*
Return the string referenced by name_index.  The pointer to which result_buffer
points may be NULL.  If so, and if a buffer is required, *result_buffer will be
set to the address of a new text buffer; otherwise, if it is not NULL and a
buffer is required, the referenced text buffer must be empty and will be used
for storage.  If non-NULL, fields (like is_operator_name) in *loc are updated
accordingly.
*/
{
  a_const_char         *result = NULL, *prefix = NULL;
  ifc_NameSort         tag = name_tag(name_index);
  a_boolean            requires_buffer;

  {
    /* Initialize the default for buffer management logic.

       If the module is memory mapped, default to directly pointing to the
       pointer in that memory.  Otherwise, default to requiring a buffer. */
#if USE_MMAP_FOR_MEMORY_REGIONS
    requires_buffer = FALSE;
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
    requires_buffer = TRUE;
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  }
  if (tag == ifc_NameSort_Identifier) {
    /* NameSort::Identifiers just refer to the string table. */
    result = get_string_at_offset((ifc_TextOffset)name_value(name_index));
  } else {
    read_prechecked_partition_element(name_index);
    switch (tag) {
      case ifc_NameSort_SourceFile:
        { an_ifc_NameSort_SourceFile inssf, *inssfp;
          inssfp = get_NameSort_SourceFile(&inssf);
          result = get_string_at_offset(inssfp->path);
        }
        break;
      case ifc_NameSort_Operator:
        { an_ifc_NameSort_Operator inso, *insop;
          insop = get_NameSort_Operator(&inso);
          if (loc != NULL) {
            /* Initialize the locator with the proper operator name. */
            make_opname_locator(opname_from_operator(insop->op), loc,
                                &null_source_position);
            result = loc->symbol_header->identifier;
          } else {
            prefix = "operator";
            requires_buffer = TRUE;
            result = get_string_at_offset(insop->encoded);
          }  /* if */
        }
        break;
      case ifc_NameSort_Conversion:
        { an_ifc_NameSort_Conversion insc, *inscp;
          a_type_ptr                 target_type;
          inscp = get_NameSort_Conversion(&insc);
          prefix = "operator ";
          requires_buffer = TRUE;
          target_type = type_for_type_index(inscp->target, /*kind=*/NULL);
          /* Note that inscp->encoded contains the mangled name of the
             conversion function, so use the name from the type instead. */
          result = target_type->source_corresp.name;
          if (result == NULL) {
            result = get_type_name(target_type);
          }  /* if */
          if (loc != NULL) {
            /* Set the locator as appropriate for a conversion function. */
            make_type_conversion_locator(target_type, loc,
                                         &null_source_position);
          }  /* if */
        }
        break;
      case ifc_NameSort_Literal:
        { an_ifc_NameSort_Literal insl, *inslp;
          inslp = get_NameSort_Literal(&insl);
          result = get_string_at_offset(inslp->encoded);
          if (loc != NULL) {
            /* Set the locator as appropriate for a user-defined literal
               operator (but skip the initial ""). */
            check_assertion(result[0] == '"' && result[1] == '"');
            result += 2;
            make_literal_opname_locator(result, strlen(result), loc,
                                        (a_source_position*)NULL);
            /* FIXME: set this? */
            loc->is_udl_operator_name = TRUE;
            result = loc->symbol_header->identifier;
            /* Never require a buffer, one was already created for the locator
               memory, and there is no prefix. */
            requires_buffer = FALSE;
          } else {
            /* Microsoft doesn't use a space after the "operator" string. */
            prefix = "operator";
            requires_buffer = TRUE;
          }  /* if */
        }
        break;
      case ifc_NameSort_Template:
        { an_ifc_NameSort_Template inst, *instp;
          instp = get_NameSort_Template(&inst);
          prefix = "template ";
          requires_buffer = TRUE;
          result = string_from_name_index(instp->name, loc);
        }
        break;
      case ifc_NameSort_Specialization:
        { an_ifc_NameSort_Specialization inss;
          get_NameSort_Specialization(&inss);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("NameSort::Specialization",
                                      &error_position);
          result = "<error-name>";
          /* String literals have static duration, no buffer required. */
          requires_buffer = FALSE;
        }
        break;
      case ifc_NameSort_Guide:
        { an_ifc_NameSort_Guide insg;
          get_NameSort_Guide(&insg);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("NameSort::Guide", &error_position);
          result = "<error-name>";
          /* String literals have static duration, no buffer required. */
          requires_buffer = FALSE;
        }
        break;
      case ifc_NameSort_Identifier:
      case ifc_NameSort_Last:
        unexpected_condition();
        break;
      default_is_unexpected();
    }  /* switch */
    /* Ensure prefixes are processed with a buffer. */
    check_assertion(prefix == NULL || requires_buffer);
  }  /* if */
  if (requires_buffer) {
    /* Check to see if a result buffer was passed that should be used.  If none
       was given, allocate a new buffer. */
    if (*result_buffer == NULL) {
      *result_buffer = alloc_text_buffer(20);
    }  /* if */
    /* Ensure the caller of this function fulfilled the contract and the buffer
       is reset. */
    check_assertion((*result_buffer)->size == 0);
    /* Compose the string. */
    if (prefix != NULL) {
      add_string_to_text_buffer(*result_buffer, prefix);
    }  /* if */
    add_string_to_text_buffer(*result_buffer, result);
    add_char_to_text_buffer(*result_buffer, '\0');
    /* Update the result. */
    result = (*result_buffer)->buffer;
  }  /* if */
  return result;
}  /* string_from_name_index */


static a_const_char* name_from_decl(an_ifc_Ref<ifc_DeclIndex> decl_ref)
/*
Given a decl reference, return the name associated with that declaration.
*/
{
  return decl_ref.mod->name_from_local_decl(decl_ref.index);
}  /* name_from_decl */


a_const_char* an_ifc_module::name_from_local_decl(ifc_DeclIndex decl)
/*
Given a declaration, return the name associated with that declaration.
*/
{
  a_const_char                   *result = NULL;
  ifc_DeclSort                   tag = decl_tag(decl);
  Opt<an_ifc_Ref<ifc_DeclIndex>> gmf_decl_scope;
  Opt<an_ifc_Ref<ifc_TypeIndex>> gmf_decl_type;

  read_prechecked_partition_element(decl);
  switch (tag) {
    case ifc_DeclSort_VendorExtension:
      issue_unsupported_node_diag("DeclSort::VendorExtension",
                                  &error_position);
      break;
    case ifc_DeclSort_Enumerator:
      { an_ifc_DeclSort_Enumerator idse, *idsep;
        idsep = get_DeclSort_Enumerator(&idse);
        result = get_string_at_offset(idsep->name);
        if (is_from_gmf(idsep->specifiers)) {
          gmf_decl_type = an_ifc_Ref<ifc_TypeIndex>(this, idsep->type);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Variable:
      { an_ifc_DeclSort_Variable idsv, *idsvp;
        idsvp = get_DeclSort_Variable(&idsv);
        result = string_from_name_index(idsvp->name, /*loc=*/NULL);
        if (is_from_gmf(idsvp->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idsvp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Parameter:
      { an_ifc_DeclSort_Parameter idsp, *idspp;
        idspp = get_DeclSort_Parameter(&idsp);
        result = get_string_at_offset(idspp->name);
      }
      break;
    case ifc_DeclSort_Field:
      { an_ifc_DeclSort_Field idsf, *idsfp;
        idsfp = get_DeclSort_Field(&idsf);
        result = get_string_at_offset(idsfp->name);
        if (is_from_gmf(idsfp->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idsfp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Bitfield:
      { an_ifc_DeclSort_Bitfield idsb, *idsbp;
        idsbp = get_DeclSort_Bitfield(&idsb);
        result = get_string_at_offset(idsbp->name);
        if (is_from_gmf(idsbp->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idsbp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Scope:
      { an_ifc_DeclSort_Scope idss, *idssp;
        idssp = get_DeclSort_Scope(&idss);
        result = string_from_name_index(idssp->name, /*loc=*/NULL);
        if (is_from_gmf(idssp->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idssp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Enumeration:
      { an_ifc_DeclSort_Enumeration idse, *idsep;
        idsep = get_DeclSort_Enumeration(&idse);
        result = get_string_at_offset(idsep->name);
        if (is_from_gmf(idsep->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idsep);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Alias:
      { an_ifc_DeclSort_Alias idsa, *idsap;
        idsap = get_DeclSort_Alias(&idsa);
        result = get_string_at_offset(idsap->name);
        if (is_from_gmf(idsap->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idsap);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Temploid:
      unexpected_condition_str("DeclSort::Temploid does not have a name");
      break;
    case ifc_DeclSort_Template:
      { an_ifc_DeclSort_Template idst, *idstp;
        idstp = get_DeclSort_Template(&idst);
        result = string_from_name_index(idstp->name, /*loc=*/NULL);
        if (is_from_gmf(idstp->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idstp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_PartialSpecialization:
      { an_ifc_DeclSort_PartialSpecialization idsps, *idspsp;
        idspsp = get_DeclSort_PartialSpecialization(&idsps);
        result = string_from_name_index(idspsp->name, /*loc=*/NULL);
        if (is_from_gmf(idspsp->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idspsp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Specialization:
      { an_ifc_DeclSort_Specialization idss, *idssp;
        idssp = get_DeclSort_Specialization(&idss);

        /* FIXME: Can this be generally applied to all of these DeclSorts? */
        {
          Opt<an_ifc_Ref<ifc_NameIndex>> opt_name_ref;

          opt_name_ref = get_ifc_name(idssp->decl);
          /* FIXME: This should be a soft failure. */
          check_assertion(opt_name_ref.has_value());
          result = string_from_name_ref(*opt_name_ref, /*loc=*/NULL);
        }
      }
      break;
    case ifc_DeclSort_Concept:
      { an_ifc_DeclSort_Concept idsc, *idscp;
        idscp = get_DeclSort_Concept(&idsc);
        result = get_string_at_offset(idscp->name);
        if (is_from_gmf(idscp->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idscp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Function:
      { an_ifc_DeclSort_Function idsf, *idsfp;
        idsfp = get_DeclSort_Function(&idsf);
        result = string_from_name_index(idsfp->name, /*loc=*/NULL);
        if (is_from_gmf(idsfp->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idsfp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Method:
      { an_ifc_DeclSort_Method idsm, *idsmp;
        idsmp = get_DeclSort_Method(&idsm);
        result = string_from_name_index(idsmp->name, /*loc=*/NULL);
        if (is_from_gmf(idsmp->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idsmp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Constructor:
      { an_ifc_DeclSort_Constructor    idsc, *idscp;
        Opt<an_ifc_Ref<ifc_DeclIndex>> opt_home_scope;

        idscp = get_DeclSort_Constructor(&idsc);
        opt_home_scope = get_ifc_home_scope_decl(idscp);
        if (opt_home_scope.has_value()) {
          an_ifc_Ref<ifc_DeclIndex> home_scope = *opt_home_scope;

          result = name_from_decl(home_scope);
          if (is_from_gmf(idscp->specifiers)) {
            gmf_decl_scope = home_scope;
          }  /* if */
        }  /* if */
      }
      break;
    case ifc_DeclSort_InheritedConstructor:
      { an_ifc_DeclSort_InheritedConstructor idsic, *idsicp;
        Opt<an_ifc_Ref<ifc_DeclIndex>>       opt_home_scope;

        idsicp = get_DeclSort_InheritedConstructor(&idsic);
        opt_home_scope = get_ifc_home_scope_decl(idsicp);
        if (opt_home_scope.has_value()) {
          an_ifc_Ref<ifc_DeclIndex> home_scope = *opt_home_scope;

          result = name_from_decl(home_scope);
          if (is_from_gmf(idsicp->specifiers)) {
            gmf_decl_scope = home_scope;
          }  /* if */
        }  /* if */
      }
      break;
    case ifc_DeclSort_Destructor:
      { an_ifc_DeclSort_Destructor     idsd, *idsdp;
        Opt<an_ifc_Ref<ifc_DeclIndex>> opt_home_scope;

        idsdp = get_DeclSort_Destructor(&idsd);
        opt_home_scope = get_ifc_home_scope_decl(idsdp);
        if (opt_home_scope.has_value()) {
          an_ifc_Ref<ifc_DeclIndex> home_scope = *opt_home_scope;

          result = name_from_decl(home_scope);
          if (is_from_gmf(idsdp->specifiers)) {
            gmf_decl_scope = home_scope;
          }  /* if */
        }  /* if */
      }
      break;
    case ifc_DeclSort_Reference:
      { an_ifc_DeclSort_Reference idsr, *idsrp;
        idsrp = get_DeclSort_Reference(&idsr);

        {
          /* References are a special case where we need to recurse to a
             foreign module. */
          a_module_entity_ptr dmep = get_ifc_decl_from_other_module(idsrp);
          an_ifc_module       *mod = get_assoc_ifc_module(dmep);

          {
            an_ifc_Ref<ifc_DeclIndex> decl_ref(mod, mod->decl_index_of(dmep));

            result = name_from_decl(decl_ref);
          }
        }
      }
      break;
    case ifc_DeclSort_UsingDeclaration:
      { an_ifc_DeclSort_UsingDeclaration idsud, *idsudp;
        idsudp = get_DeclSort_UsingDeclaration(&idsud);
        result = get_string_at_offset(idsudp->name);
        if (is_from_gmf(idsudp->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idsudp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_UsingDirective:
      { an_ifc_DeclSort_UsingDirective idsud;
        get_DeclSort_UsingDirective(&idsud);
        /* FIXME: Currently unsupported. */
        issue_unsupported_node_diag("DeclSort::UsingDirective",
                                    &error_position);
      }
      break;
    case ifc_DeclSort_Friend:
      { an_ifc_DeclSort_Friend idsf;
        get_DeclSort_Friend(&idsf);
        /* FIXME: Currently unsupported. */
        issue_unsupported_node_diag("DeclSort::Friend", &error_position);
      }
      break;
    case ifc_DeclSort_Expansion:
      { an_ifc_DeclSort_Expansion idse, *idsep;
        idsep = get_DeclSort_Expansion(&idse);
        /* FIXME: Is this reachable? */
        result = name_from_local_decl(idsep->operand);
      }
      break;
    case ifc_DeclSort_DeductionGuide:
      { an_ifc_DeclSort_DeductionGuide idsdg, *idsdgp;
        idsdgp = get_DeclSort_DeductionGuide(&idsdg);
        /* FIXME: Currently unsupported. */
        issue_unsupported_node_diag("DeclSort::DeductionGuide",
                                    &error_position);
        if (is_from_gmf(idsdgp->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idsdgp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Barren:
      { an_ifc_DeclSort_Barren idsb;
        get_DeclSort_Barren(&idsb);
        /* FIXME: Currently unsupported. */
        issue_unsupported_node_diag("DeclSort::Barren", &error_position);
      }
      break;
    case ifc_DeclSort_Tuple:
      { an_ifc_DeclSort_Tuple idst, *idstp;
        ifc_DeclIndex declidx;
        /* All references here should have the same name, so we just need to
           use the first. */
        idstp = get_DeclSort_Tuple(&idst);
        declidx = (ifc_DeclIndex)read_index_from_heap(ifc_heap_decl,
                                                      idstp->start);
        result = name_from_local_decl(declidx);
      }
      break;
    case ifc_DeclSort_SyntaxTree:
      { an_ifc_DeclSort_SyntaxTree idsst;
        get_DeclSort_SyntaxTree(&idsst);
        /* FIXME: Currently unsupported. */
        issue_unsupported_node_diag("DeclSort::SyntaxTree", &error_position);
      }
      break;
    case ifc_DeclSort_Intrinsic:
      { an_ifc_DeclSort_Intrinsic idsi, *idsip;
        idsip = get_DeclSort_Intrinsic(&idsi);
        result = get_string_at_offset(idsip->name);
        if (is_from_gmf(idsip->specifiers)) {
          gmf_decl_scope = get_ifc_home_scope_decl(idsip);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Property:
      { an_ifc_DeclSort_Property idsp, *idspp;
        idspp = get_DeclSort_Property(&idsp);
        result = name_from_local_decl(idspp->member);
      }
      break;
    case ifc_DeclSort_OutputSegment:
      { an_ifc_DeclSort_OutputSegment idsos, *idsosp;
        idsosp = get_DeclSort_OutputSegment(&idsos);
        result = get_string_at_offset(idsosp->name);
      }
      break;
    case ifc_DeclSort_UnusedSort0:
    case ifc_DeclSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected DeclSort");
  }  /* switch */
  check_assertion(result != NULL);
  {
    /* FIXME: We're dodging an issue where the declaration (decl) is part of
       some enclosing class that we're currently trying to cache by checking to
       see if the home scope is "readable".  This likely means the answer to
       the below FIXME is that, no we do not want this check here.  However,
       for the moment the "check" remains useful and prevents some
       regressions. */
    a_boolean gmf_decl = gmf_decl_scope.has_value() ||
                         gmf_decl_type.has_value();

    if (gmf_decl && is_home_scope_readable(decl)) {
      a_symbol_locator loc;
      /* FIXME: Do we want to have this check here, or should we always load
         the module's version of the declaration and rely on visibility rules
         to sort it out later?  Currently visibility is not well implemented
         and so the interplay here is not well understood. */
      if (find_symbol(result, strlen(result), &loc) == NULL) {
        /* Declarations that come from the global module fragment aren't added
           to the module scope, so we need to ensure that we've added these
           names to the lazily-loaded symbols list. */
        a_module_entity_ptr mep = get_ifc_module_entity_ptr(decl);

        if (gmf_decl_scope.has_value()) {
          mep->scope = EDG_PREFIX::get_ifc_scope(*gmf_decl_scope);
        } else {
          a_type_ptr tp = EDG_PREFIX::type_for_type_index(*gmf_decl_type,
                                                          /*kind=*/NULL);
          ensure_type_has_scope(tp);
          mep->scope = get_assoc_scope_of_il_entry((char*)tp, iek_type);
        }  /* if */
        defer_symbol_creation(mep, &loc);
      }  /* if */
    }  /* if */
  }
  return result;
}  /* name_from_local_decl */


void an_ifc_module::init_dps(a_decl_parse_state          *dps,
                             ifc_SourceLocation          *locus,
                             ifc_TypeIndex               type_index,
                             ifc_ObjectTraits            traits,
                             ifc_MsvcTraits              msvc_traits,
                             ifc_BasicSpecifiers         specifiers,
                             ifc_Access                  access,
                             ifc_ExprIndex               alignment,
                             a_partial_scope_stack_state *psssp)
/*
Map the IFC fields given by locus, type_index, alignment, traits, msvc_traits,
specifiers, and access to internal values used in the front end and set those
fields in *dps.  *psssp is a place in which to store various fields of the
decl_scope_level scope_stack entry (saved only if necessary).
restore_partial_scope_stack_if_necessary should be called with this pointer
after the declaration has been processed.  Note that although dps->alignment
is (conditionally) set in this routine, the IFC file only specifies an
alignment if the alignment is explicitly specified.  Therefore callers of
this routine need to handle the case where dps->alignment is 0.
*/
{
  an_attribute_ptr ap = NULL;

  init_decl_parse_state(dps);
  if (psssp != NULL) psssp->saved = FALSE;
  if (type_index != 0) {
    dps->type = type_for_type_index(type_index, /*kind=*/NULL);
  }  /* if */
  source_position_from_locus(&dps->start_pos, locus);
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
    if (traits & ifc_ObjectTraits_Inline) {
      dps->dso_flags |= DSO_INLINE;
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
                                 (a_byte_attribute_family)af_std, ap);
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_InitializedInClass) {
      /* FIXME: Anything to do here? */
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_NonExported) {
      /* FIXME: Anything to do here? */
    }  /* if */
  }  /* if */
  if (ap != NULL) {
    dps->prefix_attributes = ap;
  }  /* if */
  if (access != ifc_Access_None) {
    an_access_specifier il_access;
    switch (access) {
      case ifc_Access_Private:   il_access = as_private;   break;
      case ifc_Access_Protected: il_access = as_protected; break;
      case ifc_Access_Public:    il_access = as_public;    break;
      case ifc_Access_None:      unexpected_condition();   break;
      default_is_unexpected();
    }  /* switch */
    save_partial_scope_stack(psssp);
    scope_stack[decl_scope_level].current_access = il_access;
  }  /* if */
  if (alignment != 0) {
    /* Not all cases use alignment (e.g., functions). */
    an_integer_value alignment_value;
    a_boolean        err;
    unsigned_integer_for_expr_index(alignment, &alignment_value);
    dps->alignment = (a_targ_alignment)unsigned_value_of_integer_value(
                                                           &alignment_value,
                                                           /*is_signed=*/FALSE,
                                                           &err);
    check_assertion(!err);
  }  /* if */
}  /* init_dps */


static a_boolean get_textual_name(an_ifc_Ref<ifc_TextOffset> text_offset,
                                  a_symbol_locator           *loc,
                                  a_const_char               **name_result)
/*
Store the textual name representation for the given textual offset and location
in the name result.  Return TRUE if this operation completed successfully,
FALSE otherwise.
*/
{
  *name_result = EDG_PREFIX::get_string_at_offset(text_offset);
  return TRUE;
}  /* get_textual_name */


static a_boolean get_textual_name(an_ifc_Ref<ifc_NameIndex> name_ref,
                                  a_symbol_locator          *loc,
                                  a_const_char              **name_result)
/*
Store the textual name representation for the given name reference and location
in the name result.  Return TRUE if this operation completed successfully,
FALSE otherwise.
*/
{
  a_boolean result = FALSE;

  if (name_tag(name_ref) == ifc_NameSort_Identifier) {
    /* Convert the name ref into a text offset. */
    ifc_NameIndex  index = name_ref.index;
    ifc_TextOffset converted_index = (ifc_TextOffset)name_value(index);

    {
      /* Create a ref for the text offset in the corresponding module, then
         retrieve the name. */
      an_ifc_Ref<ifc_TextOffset> text_offset(name_ref.mod, converted_index);

      result = get_textual_name(text_offset, loc, name_result);
    }
  } else if (EDG_PREFIX::validate_partition_element(name_ref)) {
    *name_result = string_from_name_ref(name_ref, loc);
    result = TRUE;
  }  /* if */
  return result;
}  /* get_textual_name */


template<typename an_Index_Type>
a_boolean an_ifc_module::init_locator_from_name(
                                              an_ifc_Ref<an_Index_Type> ref,
                                              ifc_SourceLocation        *locus,
                                              a_symbol_locator          *loc)
/*
Initialize the locator specified by loc.  The name of the entity is the textual
name associated with the given reference.  The source position is given by
locus.  Return TRUE if initialization was completed and the locator is usable
(i.e., the given reference and locus were valid for locator initialization),
otherwise return FALSE.

FIXME: Not sure if we need source location here.
*/
{
  a_boolean         result = FALSE;
  a_source_position pos;
  a_const_char      *name;

  if (source_position_from_locus(&pos, locus)) {
    clear_locator(loc, &pos);
    if (get_textual_name(ref, loc, &name)) {
      if (!loc->is_operator_name &&
          !loc->is_conversion_name &&
          !loc->is_udl_operator_name) {
        /* Find the symbol (if not a special case). */
        (void)find_symbol(name, (sizeof_t)strlen(name), loc);
      }  /* if */
      /* Initialization was completed successfully. */
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* init_locator_from_name */


template<typename an_Index_Type>
inline a_boolean an_ifc_module::init_decl_locator(
                                               an_ifc_Ref<an_Index_Type> ref,
                                               ifc_SourceLocation        locus,
                                               a_symbol_locator          *loc)
/*
Initialize the locator specified by loc for the declaration named by ref and
positioned at locus.  Return TRUE if processing succeeded, otherwise return
FALSE.
*/
{
  a_boolean result = TRUE;

  if (!source_position_from_locus(&error_position, &locus)) {
    result = FALSE;
  } else if (!init_locator_from_name(ref, &locus, loc)) {
    result = FALSE;
  }  /* if */
  return result;
}  /* init_decl_locator */


template<typename an_ifc_DeclSort_T>
inline a_boolean an_ifc_module::init_decl_locator(an_ifc_DeclSort_T *decl,
                                                  a_symbol_locator  *loc)
/*
Initialize the locator specified by loc for the declaration represented at decl
using a NameIndex derived declaration name.  Return TRUE if processing
succeeded, otherwise return FALSE.
*/
{
  Opt<an_ifc_Ref<ifc_NameIndex>> opt_name_ref;
  ifc_SourceLocation             locus;
  a_boolean                      result = FALSE;

  opt_name_ref = get_ifc_name(decl);
  if (opt_name_ref.has_value()) {
    locus = get_ifc_locus(decl);
    result = init_decl_locator(*opt_name_ref, locus, loc);
  }  /* if */
  return result;
}  /* init_decl_locator */


template<typename an_ifc_DeclSort_T>
inline a_boolean an_ifc_module::lazy_init_module_scope(
                                                     an_ifc_DeclSort_T   *decl,
                                                     a_module_entity_ptr mep)
/*
If the given module entity pointer's scope is not yet set, set the scope.
Return TRUE if a scope was set.
*/
{
  a_boolean scope_initialized = FALSE;

  if (mep->scope == NULL) {
    mep->scope = get_ifc_home_scope(decl);
    scope_initialized = TRUE;
  }  /* if */
  return scope_initialized;
} /* lazy_init_module_scope */


template<typename an_ifc_DeclSort_T>
inline a_boolean an_ifc_module::lazy_push_module_scope(
                                                     an_ifc_DeclSort_T   *decl,
                                                     a_module_entity_ptr mep)
/*
If the given module entity pointer's scope is not yet set, set the scope and
push the module declaration context.  Return TRUE if a scope was pushed.
*/
{
  a_boolean scope_initialized = lazy_init_module_scope(decl, mep);
  a_boolean scope_pushed = FALSE;

  if (scope_initialized) {
    scope_pushed = push_module_declaration_context(mep->scope);
  }  /* if */
  return scope_pushed;
} /* lazy_init_module_scope */


void an_ifc_module::unsigned_integer_for_expr_index(
                                                   ifc_ExprIndex    expr_index,
                                                   an_integer_value *value)
/*
Returns in *value, the unsigned integer value represented by expr_index
(which must be either a LiteralSort::Immediate or LiteralSort::Integer).
No casting is performed.
*/
{
  an_ifc_ExprSort_Literal iesl, *ieslp;
  char                    raw_val[64/CHAR_BIT];

  /* Prepare to read from the proper partition for this expression. */
  check_assertion(expr_tag(expr_index) == ifc_ExprSort_Literal);
  read_prechecked_partition_element(expr_index);
  ieslp = get_ExprSort_Literal(&iesl);
  switch (literal_tag(ieslp->value)) {
    case ifc_LiteralSort_Immediate:
      /* An immediate literal (30 bits or less). */
      set_unsigned_integer_value(value,
                           (a_host_large_unsigned)literal_index(ieslp->value));
      break;
    case ifc_LiteralSort_Integer:
      /* An integer larger than 30 bits. */
      read_prechecked_partition_element(ifc_const_i64,
                                        literal_index(ieslp->value));
      GET_64bit_int(raw_val, /*from_header=*/FALSE);
      if (!conv_bytes_to_integer_value(value, raw_val,
                                       sizeof(raw_val))) {
        unexpected_condition_str("Failed to get 64-bit integer");
      }  /* if */
      break;
    case ifc_LiteralSort_FloatingPoint:
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
}  /* unsigned_integer_for_expr_index */


a_constant_ptr an_ifc_module::constant_for_expr_index(
                                                    ifc_ExprIndex expr_index,
                                                    a_type_ptr    default_type)
/*
Returns a constant (allocated in the current IL memory region) with the value
specified by expr_index.  Assumes the expression is constant.  If the
expression's type is zero, use default_type as the expression's type.
FIXME: shared or unshared?
FIXME: what other expressions can we get here?
*/
{
  ifc_ExprSort              tag = expr_tag(expr_index);
  a_constant_ptr            cp = NULL;
  Value_saver<a_module_entity_ptr>
                            mep_saver(&curr_module_entity);

  curr_module_entity = NULL;
  /* Prepare to read from the proper partition for this expression. */
  read_prechecked_partition_element(expr_index);
  switch (tag) {
    case ifc_ExprSort_Literal:
      { an_ifc_ExprSort_Literal iesl, *ieslp;
        a_type_ptr              constant_type;
        ieslp = get_ExprSort_Literal(&iesl);
        if (ieslp->type == 0) {
          /* If the expression doesn't have its own type, use the default
             type provided by the caller. */
          constant_type = default_type;
        } else {
          constant_type = type_for_type_index(ieslp->type, /*kind=*/NULL);
        }  /* if */
        if (constant_type != NULL && is_error_type(constant_type)) {
          cp = alloc_error_constant();
          expect_error();
          goto done;
        }  /* if */
        switch (literal_tag(ieslp->value)) {
          case ifc_LiteralSort_Immediate:
          case ifc_LiteralSort_Integer:
            /* An integer. */
            { an_integer_value value;
              /* Retrieve the unsigned value of the integer. */
              unsigned_integer_for_expr_index(expr_index, &value);
              cp = alloc_constant(ck_integer);
              if (ieslp->type == 0 && constant_type == NULL) {
                /* FIXME: not sure why the type is zero in some cases. */
                set_unsigned_integer_constant(
                            cp,
                            (a_host_large_unsigned)literal_index(ieslp->value),
                            (an_integer_kind)ik_unsigned_int);
              } else {
                check_assertion(constant_type != NULL);
                if (is_void_star_type(constant_type)) {
                  /* Pointer literal, nullptr constant. */
                  set_unsigned_integer_constant(cp, value, ik_unsigned_int);
                } else {
                  a_type_ptr stripped_type = skip_typerefs(constant_type);
                  check_assertion(stripped_type->kind ==
                                                      (a_type_kind)tk_integer);
                  set_unsigned_integer_constant(
                                      cp, value,
                                      stripped_type->variant.integer.int_kind);
                }  /* if */
                cp->type = constant_type;
              }  /* if */
            }
            break;
          case ifc_LiteralSort_FloatingPoint:
            { double    value;
              a_boolean err = FALSE;
              char      buf[32];

              cp = alloc_constant(ck_float);
              cp->type = float_type(fk_double);
              read_prechecked_partition_element(ifc_const_f64,
                                                literal_index(ieslp->value));
              GET_64bit_int(value, /*from_header=*/FALSE);
              /* FIXME: Find a better way to convert the fp value. */
              sprintf(buf, "%f", value);
              fp_string_to_float(fk_double, buf, &cp->variant.float_value,
                                 &err);
              check_assertion(!err);
            }
            break;
          default_is_unexpected();
        }  /* switch */
      }
      break;
    case ifc_ExprSort_ArrayValue:
      { an_ifc_ExprSort_ArrayValue iesav;
        an_error_severity          saved_sev = unhandled_ifc_node_severity;
        get_ExprSort_ArrayValue(&iesav);
        /* FIXME: Currently unsupported. */
        /* Because we're allocating an error constant, we need to issue an
           error, otherwise this may get to lowering. */
        unhandled_ifc_node_severity = es_discretionary_error;
        issue_unsupported_node_diag("ExprSort::ArrayValue", &error_position);
        cp = alloc_error_constant();
        expect_error();
        unhandled_ifc_node_severity = saved_sev;
      }
      break;
    case ifc_ExprSort_ProductTypeValue:
      { an_ifc_ExprSort_ProductTypeValue iesptv, *iesptvp;
        a_type_ptr                       tp;
        iesptvp = get_ExprSort_ProductTypeValue(&iesptv);
        tp = type_for_type_index(iesptvp->type, /*kind=*/NULL);
        complete_type_is_needed(tp);
        cp = alloc_constant(ck_aggregate);
#if DO_IL_LOWERING
        /* This will be changed later if there are any actual fields that get
           initialized. */
        cp->initializes_empty_object = TRUE;
#endif /* DO_IL_LOWERING */
        cp->type = tp;
        if (iesptvp->base_subobjects != 0) {
          a_constant_ptr sub_con =
                              constant_for_expr_index(iesptvp->base_subobjects,
                                                      /*default_type=*/NULL);
          add_constant_to_aggregate(sub_con, cp, NULL, NULL);
#if DO_IL_LOWERING
          if (!sub_con->initializes_empty_object) {
            cp->initializes_empty_object = FALSE;
          }  /* if */
#endif /* DO_IL_LOWERING */
        }  /* if */
        if (iesptvp->members != 0) {
          a_constant_ptr mem_con =
                                constant_for_expr_index(iesptvp->members,
                                                        /*default_type=*/NULL);
          add_constant_to_aggregate(mem_con, cp, NULL, NULL);
#if DO_IL_LOWERING
          if (!mem_con->initializes_empty_object) {
            cp->initializes_empty_object = FALSE;
          }  /* if */
#endif /* DO_IL_LOWERING */
        }  /* if */
      }
      break;
    case ifc_ExprSort_SubobjectValue:
      { an_ifc_ExprSort_SubobjectValue iessv, *iessvp;
        iessvp = get_ExprSort_SubobjectValue(&iessv);
        cp = constant_for_expr_index(iessvp->value, default_type);
      }
      break;
    case ifc_ExprSort_NamedDecl:
      { an_ifc_ExprSort_NamedDecl iesnd, *iesndp;
        iesndp = get_ExprSort_NamedDecl(&iesnd);
        cp = constant_for_named_decl(iesndp);
      }
      break;
    case ifc_ExprSort_Tuple:
      { an_ifc_ExprSort_Tuple iest, *iestp;
        a_constant_ptr        *next_cp = &cp;
        iestp = get_ExprSort_Tuple(&iest);
        for (uint32_t idx = 0; idx < iestp->cardinality; ++idx) {
          ifc_ExprIndex sub_expr;
          read_prechecked_partition_element(ifc_heap_expr, iestp->start + idx);
          GET_ExprIndex(sub_expr, /*from_header=*/FALSE);
          *next_cp = constant_for_expr_index(sub_expr, /*default_type=*/NULL);
          next_cp = &(*next_cp)->next;
        }  /* for */
      }
      break;
    case ifc_ExprSort_Dyad:
      { an_ifc_ExprSort_Dyad iesd, *iesdp;
        a_token_cache        cache;
        a_decl_parse_state   dps;
        a_type_ptr           tp;

        iesdp = get_ExprSort_Dyad(&iesd);
        init_decl_parse_state(&dps);
        clear_token_cache(&cache, /*reusable=*/FALSE);
        cp = alloc_constant(ck_error);
        tp = type_for_type_index(iesdp->type, /*kind=*/NULL);
        complete_type_is_needed(tp);
        cache_expr(&cache, expr_index);
        terminate_token_cache(&cache);
        rescan_cached_tokens(&cache);
        scan_constant_initializer_expression(tp, &dps, cp);
        check_assertion(curr_token == tok_end_of_source);
        (void)get_token();
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
done:
  return cp;
}  /* constant_for_expr_index */


a_constant_ptr an_ifc_module::constant_for_named_decl(
                                            an_ifc_ExprSort_NamedDecl *iesndp)
/*
Returns a constant (allocated in the current IL memory region) with the value
corresponding to the provided named declaration.  Assumes the expression is
constant.
FIXME: shared or unshared?
FIXME: what other types of named declarations can we get here?
*/
{
  ifc_DeclSort   tag = decl_tag(iesndp->resolution);
  a_constant_ptr cp = NULL;
  a_type_ptr     type;

  type = type_for_type_index(iesndp->type, /*kind=*/NULL);
  read_prechecked_partition_element(iesndp->resolution);
  switch (tag) {
    case ifc_DeclSort_Enumerator:
      { an_ifc_DeclSort_Enumerator idse, *idsep;
        idsep = get_DeclSort_Enumerator(&idse);
        /* Skip straight to the enumerator value. */
        check_assertion(idsep->initializer != 0);
        cp = constant_for_expr_index(idsep->initializer, type);
      }
      break;
    case ifc_DeclSort_Function:
      { an_error_severity saved_sev = unhandled_ifc_node_severity;
        /* Since we're creating an error constant this needs to be an error so
           that we do not proceed to lowering. */
        unhandled_ifc_node_severity = es_error;
        issue_unsupported_node_diag("DeclSort::Function"
                                    " for ExprSort::NamedDecl",
                                    &error_position);
        cp = alloc_error_constant();
        unhandled_ifc_node_severity = saved_sev;
      }
      break;
    default:
      unexpected_condition_str("Unexpected DeclSort for ExprSort::NamedDecl");
  }  /* switch */
  return cp;
}  /* constant_for_named_decl */


static void cache_identifier(a_token_cache_ptr     cache,
                             a_const_char          *name,
                             a_source_position_ptr pos)
/*
Add a tok_identifier for name to cache.  pos is the position of the identifier.
*/
{
  a_symbol_locator  loc;

  check_assertion(name != NULL);
  clear_locator(&loc, pos);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  cache_token(cache, tok_identifier, pos);
  cache->last_token->extra_info_kind =(a_token_extra_info_kind)teik_identifier;
  cache->last_token->variant.locator = loc;
}  /* cache_identifier */


static void cache_literal(a_token_cache_ptr     cache,
                          a_constant_ptr        lit_const,
                          a_source_position_ptr pos)
/*
Add a tok_literal for lit_const to cache.  pos is the position of the literal.
Do not use this for boolean literals, string literals, or user-defined
literals (see cache_bool_literal, cache_string_literal, and cache_ud_literal
for those).
*/
{
  a_token_kind lit_kind;

  if (is_floating_type(lit_const->type)) {
    lit_kind = tok_float_constant;
#if FIXED_POINT_ALLOWED
  } else if (is_fixed_point_type(lit_const->type)) {
    lit_kind = tok_fixed_point_constant;
#endif /* FIXED_POINT_ALLOWED */
  } else if (is_character_type(lit_const->type)) {
    lit_kind = tok_char_constant;
  } else if (is_integral_type(lit_const->type)) {
    lit_kind = tok_int_constant;
  } else if (is_void_star_type(lit_const->type)) {
    /* Pointer literal, nullptr constant. */
    lit_kind = tok_nullptr;
  } else {
    check_assertion(is_error_type(lit_const->type));
    lit_kind = tok_error;
  }  /* if */
  cache_token(cache, lit_kind, pos);
  cache->last_token->extra_info_kind = (a_token_extra_info_kind)teik_constant;
  cache->last_token->variant.constant = alloc_cached_constant();
  copy_constant(lit_const, cache->last_token->variant.constant);
}  /* cache_literal */


static void cache_bool_literal(a_token_cache_ptr     cache,
                               a_boolean             value,
                               a_source_position_ptr pos)
/*
Cache a "true" or "false" token, depending on value.
*/
{
  cache_token(cache, value ? tok_true : tok_false, pos);
  cache->last_token->extra_info_kind = (a_token_extra_info_kind)teik_constant;
  cache->last_token->variant.constant = alloc_cached_constant();
  make_bool_constant_value(value, cache->last_token->variant.constant);
}  /* cache_bool_literal */


static void cache_string_literal(a_token_cache_ptr     cache,
                                 a_character_kind      kind,
                                 a_const_char          *str,
                                 a_targ_size_t         length,
                                 a_source_position_ptr pos)
/*
Add a tok_string_literal for str with the given length to cache.  kind is the
type of string literal (e.g., UTF-8, wchar, etc).  pos is the position of the
literal.
*/
{
  a_constant_ptr  cp;
  char            *val;
  a_cached_token  *prev_string = NULL;

  if (cache->last_token != NULL &&
      cache->last_token->token == tok_string_literal) {
    prev_string = cache->last_token;
  }  /* if */
  cache_token(cache, tok_string_literal, pos);
  cache->last_token->extra_info_kind = (a_token_extra_info_kind)teik_constant;
  cache->last_token->variant.constant = cp = alloc_cached_constant();
  val = alloc_text_of_string_literal((sizeof_t)length);
  (void)memcpy(val, str, length);
  clear_constant(cp, (a_constant_repr_kind)ck_string);
  cp->type = string_literal_type(kind, length);
  cp->variant.string.length = length;
  cp->variant.string.value  = val;
  if (prev_string != NULL) {
    concat_string_literals(cache, kind, prev_string);
    remove_token_from_cache(cache->last_token, &prev_string, cache);
  }  /* if */
}  /* cache_string_literal */


static void cache_ud_literal(a_token_cache_ptr     cache,
                             a_character_kind      kind,
                             a_const_char          *str,
                             a_targ_size_t         length,
                             a_const_char          *suffix,
                             a_source_position_ptr pos)
/*
Add a tok_ud_literal for str with the given length and suffix to cache.  kind
is the type of string literal (e.g., UTF-8, wchar, etc).  pos is the position
of the literal.
*/
{
  a_constant_ptr     cp;
  char*              val;
  a_targ_size_t      suffix_len = (a_targ_size_t)(strlen(suffix) + 1);
  a_cached_token_ptr ctp;

  cache_token(cache, tok_ud_literal, pos);
  ctp = cache->last_token;
  ctp->extra_info_kind = (a_token_extra_info_kind)teik_ud_lit;
  ctp->variant.ud_lit.value_con = cp = alloc_cached_constant();
  val = alloc_text_of_string_literal((sizeof_t)length);
  (void)memcpy(val, str, length);
  clear_constant(cp, (a_constant_repr_kind)ck_string);
  cp->type = string_literal_type(kind, length);
  cp->variant.string.length = length;
  cp->variant.string.value  = val;
  ctp->variant.ud_lit.spelling_con = alloc_cached_constant();
  copy_constant(cp, ctp->variant.ud_lit.spelling_con);
  ctp->variant.ud_lit.type = cp->type;
  val = alloc_text_of_string_literal(suffix_len);
  strcpy(val, suffix);
  ctp->variant.ud_lit.suffix = val;
  ctp->variant.ud_lit.op_sym = find_literal_operator(val, suffix_len-1, pos,
                                                     cp->type,
                                                     /*from_cache=*/FALSE,
                                                     (a_diagnostic_ptr)NULL);
}  /* cache_ud_literal */


static void cache_aggr_constant(a_token_cache_ptr     cache,
                                a_constant_ptr        cp,
                                a_source_position_ptr pos)
/*
Add a tok_aggr_constant token with the provided constant to cache.  pos is the
position of the constant.
*/
{
  cache_token(cache, tok_aggr_constant, pos);
  cache->last_token->extra_info_kind = (a_token_extra_info_kind)teik_constant;
  cache->last_token->variant.constant = alloc_cached_constant();
  copy_constant(cp, cache->last_token->variant.constant);
}  /* cache_aggr_constant */


static void cache_pragma(a_token_cache_ptr     cache,
                         a_pragma_kind         kind,
                         a_source_position_ptr pos)
/*
Add the pragma given by kind to cache.  pos is the position of the pragma.
*/
{
  a_pending_pragma_ptr          ppp, *next_pragma;
  a_pragma_kind_description_ptr	pkdp;

  pkdp = pragma_description_for_pragma_kind[(int)kind];
  ppp = alloc_pending_pragma(pkdp);
  ppp->id_position = *pos;
  ppp->pragma_position = *pos;
  /* Create a token to hold the pragmas if needed. */
  if (cache->last_token == NULL ||
      cache->last_token->extra_info_kind !=
                                        (a_token_extra_info_kind)teik_pragma) {
    cache_token(cache, tok_error, pos);
  }  /* if */
  next_pragma = &cache->last_token->variant.pragmas;
  /* Find the end of the pragma list. */
  for (; *next_pragma != NULL; next_pragma = &(*next_pragma)->next) {}
  *next_pragma = ppp;
#if DEBUG
  add_to_pragmas_in_reuseable_cache_count(1);
  cache->pragma_count++;
#endif /* DEBUG */
}  /* cache_pragma */


static void cache_pp_token(a_token_cache_ptr     cache,
                           a_const_char          *text,
                           a_targ_size_t         len,
                           a_source_position_ptr pos)
/*
Add the preprocessor token contained in text with the given length to cache.
pos is the position of the preprocessor token.
*/
{
  check_assertion(text != NULL);
  cache_token(cache, tok_identifier, pos);
  cache->last_token->extra_info_kind =(a_token_extra_info_kind)teik_pp_token;
  cache->last_token->variant.pp_token_descr.token_start = (char*)text;
  cache->last_token->variant.pp_token_descr.token_end = (char*)text + len;
}  /* cache_pp_token */


static void cache_access(a_token_cache_ptr     cache,
                         ifc_Access            access,
                         a_boolean             cache_colon,
                         a_source_position_ptr pos)
/*
Add tokens corresponding to access (if any) to cache.  If cache_colon is TRUE,
cache a tok_colon after the access specifier.  pos is the position of the
access specifier.
*/
{
  switch (access) {
    case ifc_Access_None:
      cache_colon = FALSE;
      /* Nothing to cache. */
      break;
    case ifc_Access_Private:
      cache_token(cache, tok_private, pos);
      break;
    case ifc_Access_Protected:
      cache_token(cache, tok_protected, pos);
      break;
    case ifc_Access_Public:
      cache_token(cache, tok_public, pos);
      break;
    default_is_unexpected();
  }  /* switch */
  if (cache_colon) {
    cache_token(cache, tok_colon, pos);
  }  /* if */
}  /* cache_access */


void an_ifc_module::cache_source_directive(a_token_cache_ptr   cache,
                                           ifc_SourceDirective directive,
                                           ifc_SourceLocation  *locus)
/*
Add tokens corresponding to directive to cache.  locus is the location of the
Sentence containing directive.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (directive) {
    case ifc_SourceDirective_Msvc:
      break;
    case ifc_SourceDirective_MsvcPragmaComment:
      cache_pragma(cache, pk_comment, &pos);
      break;
    case ifc_SourceDirective_MsvcPragmaConform:
      cache_pragma(cache, pk_conform, &pos);
      break;
    case ifc_SourceDirective_MsvcPragmaIdent:
#if IDENT_DIRECTIVE_AND_PRAGMA
      cache_pragma(cache, pk_ident_pragma, &pos);
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
      break;
    case ifc_SourceDirective_MsvcPragmaIncludeAlias:
      cache_pragma(cache, pk_include_alias, &pos);
      break;
    case ifc_SourceDirective_MsvcPragmaPack:
      cache_pragma(cache, pk_pack, &pos);
      break;
    case ifc_SourceDirective_MsvcPragmaPopMacro:
      cache_pragma(cache, pk_pop_macro, &pos);
      break;
    case ifc_SourceDirective_MsvcPragmaPushMacro:
      cache_pragma(cache, pk_push_macro, &pos);
      break;
    case ifc_SourceDirective_MsvcPragmaSetlocale:
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
      cache_pragma(cache, pk_setlocale, &pos);
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
      break;
    case ifc_SourceDirective_MsvcPragmaStartMapRegion:
      cache_pragma(cache, pk_start_map_region, &pos);
      break;
    case ifc_SourceDirective_MsvcPragmaStopMapRegion:
      cache_pragma(cache, pk_stop_map_region, &pos);
      break;
    case ifc_SourceDirective_MsvcPragmaPush:
    case ifc_SourceDirective_MsvcPragmaPop:
    case ifc_SourceDirective_MsvcDirectiveStart:
    case ifc_SourceDirective_MsvcDirectiveEnd:
    case ifc_SourceDirective_MsvcPragmaAllocText:
    case ifc_SourceDirective_MsvcPragmaAutoInline:
    case ifc_SourceDirective_MsvcPragmaBssSeg:
    case ifc_SourceDirective_MsvcPragmaCheckStack:
    case ifc_SourceDirective_MsvcPragmaCodeSeg:
    case ifc_SourceDirective_MsvcPragmaComponent:
    case ifc_SourceDirective_MsvcPragmaConstSeg:
    case ifc_SourceDirective_MsvcPragmaDataSeg:
    case ifc_SourceDirective_MsvcPragmaDeprecated:
    case ifc_SourceDirective_MsvcPragmaDetectMismatch:
    case ifc_SourceDirective_MsvcPragmaEndregion:
    case ifc_SourceDirective_MsvcPragmaExecutionCharacterSet:
    case ifc_SourceDirective_MsvcPragmaFenvAccess:
    case ifc_SourceDirective_MsvcPragmaFileHash:
    case ifc_SourceDirective_MsvcPragmaFloatControl:
    case ifc_SourceDirective_MsvcPragmaFpContract:
    case ifc_SourceDirective_MsvcPragmaFunction:
    case ifc_SourceDirective_MsvcPragmaBGI:
    case ifc_SourceDirective_MsvcPragmaImplementationKey:
    case ifc_SourceDirective_MsvcPragmaInitSeq:
    case ifc_SourceDirective_MsvcPragmaInlineDepth:
    case ifc_SourceDirective_MsvcPragmaInlineRecursion:
    case ifc_SourceDirective_MsvcPragmaIntrinsic:
    case ifc_SourceDirective_MsvcPragmaLoop:
    case ifc_SourceDirective_MsvcPragmaMakePublic:
    case ifc_SourceDirective_MsvcPragmaManaged:
    case ifc_SourceDirective_MsvcPragmaMessage:
    case ifc_SourceDirective_MsvcPragmaOMP:
    case ifc_SourceDirective_MsvcPragmaOptimize:
    case ifc_SourceDirective_MsvcPragmaPointerToMembers:
    case ifc_SourceDirective_MsvcPragmaPrefast:
    case ifc_SourceDirective_MsvcPragmaRegion:
    case ifc_SourceDirective_MsvcPragmaRuntimeChecks:
    case ifc_SourceDirective_MsvcPragmaSameSeg:
    case ifc_SourceDirective_MsvcPragmaSection:
    case ifc_SourceDirective_MsvcPragmaSegment:
    case ifc_SourceDirective_MsvcPragmaStrictGSCheck:
    case ifc_SourceDirective_MsvcPragmaSystemHeader:
    case ifc_SourceDirective_MsvcPragmaUnmanaged:
    case ifc_SourceDirective_MsvcPragmaVtordisp:
    case ifc_SourceDirective_MsvcPragmaWarning:
    case ifc_SourceDirective_MsvcPragmaP0include:
    case ifc_SourceDirective_MsvcPragmaP0line:
      /* These pragmas have no corresponding EDG pragma. */
      break;
    default_is_unexpected_str("Unknown SourceDirective");
  }  /* switch */
}  /* cache_source_directive */


void an_ifc_module::cache_source_punctuator(a_token_cache_ptr    cache,
                                            ifc_SourcePunctuator punctuator,
                                            ifc_SourceLocation   *locus)
/*
Add tokens corresponding to punctuator to cache.  locus is the location of the
Sentence containing punctuator.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (punctuator) {
    case ifc_SourcePunctuator_Unknown:
      unexpected_condition();
      break;
    case ifc_SourcePunctuator_LeftParenthesis:
      cache_token(cache, tok_lparen, &pos);
      break;
    case ifc_SourcePunctuator_RightParenthesis:
      cache_token(cache, tok_rparen, &pos);
      break;
    case ifc_SourcePunctuator_LeftBracket:
      cache_token(cache, tok_lbracket, &pos);
      break;
    case ifc_SourcePunctuator_RightBracket:
      cache_token(cache, tok_rbracket, &pos);
      break;
    case ifc_SourcePunctuator_LeftBrace:
      cache_token(cache, tok_lbrace, &pos);
      break;
    case ifc_SourcePunctuator_RightBrace:
      cache_token(cache, tok_rbrace, &pos);
      break;
    case ifc_SourcePunctuator_Colon:
      cache_token(cache, tok_colon, &pos);
      break;
    case ifc_SourcePunctuator_Question:
      cache_token(cache, tok_quest_mark, &pos);
      break;
    case ifc_SourcePunctuator_Semicolon:
      cache_token(cache, tok_semicolon, &pos);
      break;
    case ifc_SourcePunctuator_ColonColon:
      cache_token(cache, tok_colon_colon, &pos);
      break;
    case ifc_SourcePunctuator_Msvc:
      unexpected_condition();
      break;
    case ifc_SourcePunctuator_MsvcZeroWidthSpace:
      break;
    case ifc_SourcePunctuator_MsvcEndOfPhrase:
      break;
    case ifc_SourcePunctuator_MsvcFullStop:
      break;
    case ifc_SourcePunctuator_MsvcNestedTemplateStart:
      cache_token(cache, tok_template, &pos);
      break;
    case ifc_SourcePunctuator_MsvcDefaultArgumentStart:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourcePunctuator::MsvcDefaultArgumentStart",
                                  &error_position);
      break;
    case ifc_SourcePunctuator_MsvcAlignasEdictStart:
      break;
    case ifc_SourcePunctuator_MsvcDefaultInitStart:
      /* A marker to indicate the start of a default member initializer.  It
         isn't needed in the source code. */
      break;
    default_is_unexpected_str("Unknown SourcePunctuator");
  }  /* switch */
}  /* cache_source_punctuator */


void an_ifc_module::cache_source_literal(a_token_cache_ptr  cache,
                                         ifc_SourceLiteral  literal,
                                         ifc_Index          index,
                                         ifc_SourceLocation *locus)
/*
Add tokens corresponding to literal to cache.  index is the index into the IFC
file for the additional information needed, depending on the kind of literal.
locus is the location of the Sentence containing literal.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (literal) {
    case ifc_SourceLiteral_Unknown:
      unexpected_condition();
      break;
    case ifc_SourceLiteral_Scalar:
      cache_expr(cache, (ifc_ExprIndex)index);
      break;
    case ifc_SourceLiteral_String:
    case ifc_SourceLiteral_DefinedString:
      cache_string(cache, (ifc_StringIndex)index, locus);
      break;
    case ifc_SourceLiteral_Msvc:
      break;
    case ifc_SourceLiteral_MsvcFunctionNameMacro:
      { a_const_char *str = get_string_at_offset((ifc_TextOffset)index);
        a_token_kind tok;
        if (strncmp(str, "__func__", sizeof("__func__")) == 0) {
          tok = tok_func_name;
        } else if (strncmp(str, "__FUNCTION__", sizeof("__FUNCTION__")) == 0) {
          tok = tok_function_name;
        } else if (strncmp(str, "__FUNCDNAME__", sizeof("__FUNCDNAME__"))
                                                                        == 0) {
          tok = tok_decorated_function_name;
        } else {
          check_assertion(strncmp(str, "__FUNCSIG__", sizeof("__FUNCSIG__"))
                                                                         == 0);
          tok = tok_pretty_function_name;
        }  /* if */
        cache_token(cache, tok, &pos);
      }
      break;
    case ifc_SourceLiteral_MsvcStringPrefixMacro:
      { a_const_char *str = get_string_at_offset((ifc_TextOffset)index);
        a_token_kind tok;
        if (strncmp(str, "__LPREFIX", sizeof("__LPREFIX")) == 0) {
          tok = tok_microsoft_Lprefix;
        } else if (strncmp(str, "__lPREFIX", sizeof("__lPREFIX")) == 0) {
          tok = tok_microsoft_lprefix;
        } else if (strncmp(str, "__UPREFIX", sizeof("__UPREFIX"))
                                                                        == 0) {
          tok = tok_microsoft_Uprefix;
        } else {
          check_assertion(strncmp(str, "__uPREFIX", sizeof("__uPREFIX"))
                                                                         == 0);
          tok = tok_microsoft_uprefix;
        }  /* if */
        cache_token(cache, tok, &pos);
      }
      break;
    case ifc_SourceLiteral_MsvcBinding:
      {
        source_position_from_locus(&pos, locus);
        read_prechecked_partition_element((ifc_ExprIndex)index);
        switch(expr_tag(index)) {
          case ifc_ExprSort_NamedDecl:
            { an_ifc_ExprSort_NamedDecl iesnd, *iesndp;
              iesndp = get_ExprSort_NamedDecl(&iesnd);
              cache_name_from_decl(cache, iesndp->resolution, &iesndp->locus);
            }
            break;
          case ifc_ExprSort_UnresolvedId:
            { an_ifc_ExprSort_UnresolvedId iesui, *iesuip;
              iesuip = get_ExprSort_UnresolvedId(&iesui);
              cache_identifier(cache,
                               string_from_name_index(iesuip->name,
                                                      /*loc=*/NULL),
                               &pos);
            }
            break;
          default:
            unexpected_condition_str("Unexpected ExprSort for "
                                     "SourceLiteral::MsvcBinding");
        }  /* switch */
      }
      break;
    default_is_unexpected_str("Unknown SourceLiteral");
  }  /* switch */
}  /* cache_source_literal */


void an_ifc_module::cache_source_operator(a_token_cache_ptr  cache,
                                          ifc_SourceOperator op,
                                          ifc_SourceLocation *locus)
/*
Add tokens corresponding to op to cache.  locus is the location of the Sentence
containing op.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (op) {
    case ifc_SourceOperator_Unknown:
      unexpected_condition();
      break;
    case ifc_SourceOperator_Equal:
      cache_token(cache, tok_assign, &pos);
      break;
    case ifc_SourceOperator_Comma:
      cache_token(cache, tok_comma, &pos);
      break;
    case ifc_SourceOperator_Exclaim:
      cache_token(cache, tok_not, &pos);
      break;
    case ifc_SourceOperator_Plus:
      cache_token(cache, tok_plus, &pos);
      break;
    case ifc_SourceOperator_Dash:
      cache_token(cache, tok_minus, &pos);
      break;
    case ifc_SourceOperator_Star:
      cache_token(cache, tok_star, &pos);
      break;
    case ifc_SourceOperator_Slash:
      cache_token(cache, tok_divide, &pos);
      break;
    case ifc_SourceOperator_Percent:
      cache_token(cache, tok_remainder, &pos);
      break;
    case ifc_SourceOperator_LeftChevron:
      cache_token(cache, tok_shift_left, &pos);
      break;
    case ifc_SourceOperator_RightChevron:
      cache_token(cache, tok_shift_right, &pos);
      break;
    case ifc_SourceOperator_Tilde:
      cache_token(cache, tok_compl, &pos);
      break;
    case ifc_SourceOperator_Caret:
      cache_token(cache, tok_excl_or, &pos);
      break;
    case ifc_SourceOperator_Bar:
      cache_token(cache, tok_or, &pos);
      break;
    case ifc_SourceOperator_Ampersand:
      cache_token(cache, tok_ampersand, &pos);
      break;
    case ifc_SourceOperator_PlusPlus:
      cache_token(cache, tok_plus_plus, &pos);
      break;
    case ifc_SourceOperator_DashDash:
      cache_token(cache, tok_minus_minus, &pos);
      break;
    case ifc_SourceOperator_Less:
      cache_token(cache, tok_lt, &pos);
      break;
    case ifc_SourceOperator_LessEqual:
      cache_token(cache, tok_le, &pos);
      break;
    case ifc_SourceOperator_Greater:
      cache_token(cache, tok_gt, &pos);
      break;
    case ifc_SourceOperator_GreaterEqual:
      cache_token(cache, tok_ge, &pos);
      break;
    case ifc_SourceOperator_EqualEqual:
      cache_token(cache, tok_eq, &pos);
      break;
    case ifc_SourceOperator_ExclaimEqual:
      cache_token(cache, tok_ne, &pos);
      break;
    case ifc_SourceOperator_Diamond:
      cache_token(cache, tok_spaceship, &pos);
      break;
    case ifc_SourceOperator_PlusEqual:
      cache_token(cache, tok_plus_assign, &pos);
      break;
    case ifc_SourceOperator_DashEqual:
      cache_token(cache, tok_minus_assign, &pos);
      break;
    case ifc_SourceOperator_StarEqual:
      cache_token(cache, tok_times_assign, &pos);
      break;
    case ifc_SourceOperator_SlashEqual:
      cache_token(cache, tok_divide_assign, &pos);
      break;
    case ifc_SourceOperator_PercentEqual:
      cache_token(cache, tok_remainder_assign, &pos);
      break;
    case ifc_SourceOperator_AmpersandEqual:
      cache_token(cache, tok_and_assign, &pos);
      break;
    case ifc_SourceOperator_BarEqual:
      cache_token(cache, tok_or_assign, &pos);
      break;
    case ifc_SourceOperator_CaretEqual:
      cache_token(cache, tok_excl_or_assign, &pos);
      break;
    case ifc_SourceOperator_LeftChevronEqual:
      cache_token(cache, tok_shift_left_assign, &pos);
      break;
    case ifc_SourceOperator_RightChevronEqual:
      cache_token(cache, tok_shift_right_assign, &pos);
      break;
    case ifc_SourceOperator_AmpersandAmpersand:
      cache_token(cache, tok_and_and, &pos);
      break;
    case ifc_SourceOperator_BarBar:
      cache_token(cache, tok_or_or, &pos);
      break;
    case ifc_SourceOperator_Ellipsis:
      cache_token(cache, tok_ellipsis, &pos);
      break;
    case ifc_SourceOperator_Dot:
      cache_token(cache, tok_period, &pos);
      break;
    case ifc_SourceOperator_Arrow:
      cache_token(cache, tok_arrow, &pos);
      break;
    case ifc_SourceOperator_DotStar:
      cache_token(cache, tok_period_star, &pos);
      break;
    case ifc_SourceOperator_ArrowStar:
      cache_token(cache, tok_arrow_star, &pos);
      break;
    default_is_unexpected_str("Unknown SourceOperator");
  }  /* switch */
}  /* cache_source_operator */


void an_ifc_module::cache_source_keyword(a_token_cache_ptr  cache,
                                         ifc_SourceKeyword  keyword,
                                         ifc_SourceLocation *locus)
/*
Add tokens corresponding to keyword to cache.  locus is the location of the
Sentence containing keyword.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (keyword) {
    case ifc_SourceKeyword_Unknown:
      unexpected_condition();
      break;
    case ifc_SourceKeyword_Alignas:
      cache_token(cache, tok_alignas, &pos);
      break;
    case ifc_SourceKeyword_Alignof:
      cache_token(cache, tok_alignof, &pos);
      break;
    case ifc_SourceKeyword_Asm:
      cache_token(cache, tok_asm, &pos);
      break;
    case ifc_SourceKeyword_Auto:
      cache_token(cache, tok_auto, &pos);
      break;
    case ifc_SourceKeyword_Bool:
      cache_token(cache, tok_bool, &pos);
      break;
    case ifc_SourceKeyword_Break:
      cache_token(cache, tok_break, &pos);
      break;
    case ifc_SourceKeyword_Case:
      cache_token(cache, tok_case, &pos);
      break;
    case ifc_SourceKeyword_Catch:
      cache_token(cache, tok_catch, &pos);
      break;
    case ifc_SourceKeyword_Char:
      cache_token(cache, tok_char, &pos);
      break;
    case ifc_SourceKeyword_Char8T:
      cache_token(cache, tok_char8_t, &pos);
      break;
    case ifc_SourceKeyword_Char16T:
      cache_token(cache, tok_char16_t, &pos);
      break;
    case ifc_SourceKeyword_Char32T:
      cache_token(cache, tok_char32_t, &pos);
      break;
    case ifc_SourceKeyword_Class:
      cache_token(cache, tok_class, &pos);
      break;
    case ifc_SourceKeyword_Concept:
      cache_token(cache, tok_concept, &pos);
      break;
    case ifc_SourceKeyword_Const:
      cache_token(cache, tok_const, &pos);
      break;
    case ifc_SourceKeyword_Consteval:
      cache_token(cache, tok_consteval, &pos);
      break;
    case ifc_SourceKeyword_Constexpr:
      cache_token(cache, tok_constexpr, &pos);
      break;
    case ifc_SourceKeyword_Constinit:
      cache_token(cache, tok_constinit, &pos);
      break;
    case ifc_SourceKeyword_ConstCast:
      cache_token(cache, tok_const_cast, &pos);
      break;
    case ifc_SourceKeyword_Continue:
      cache_token(cache, tok_continue, &pos);
      break;
    case ifc_SourceKeyword_CoAwait:
      cache_token(cache, tok_coroutine_await, &pos);
      break;
    case ifc_SourceKeyword_CoReturn:
      cache_token(cache, tok_coroutine_return, &pos);
      break;
    case ifc_SourceKeyword_CoYield:
      cache_token(cache, tok_coroutine_yield, &pos);
      break;
    case ifc_SourceKeyword_Decltype:
      cache_token(cache, tok_decltype, &pos);
      break;
    case ifc_SourceKeyword_Default:
      cache_token(cache, tok_default, &pos);
      break;
    case ifc_SourceKeyword_Delete:
      cache_token(cache, tok_delete, &pos);
      break;
    case ifc_SourceKeyword_Do:
      cache_token(cache, tok_do, &pos);
      break;
    case ifc_SourceKeyword_Double:
      cache_token(cache, tok_double, &pos);
      break;
    case ifc_SourceKeyword_DynamicCast:
      cache_token(cache, tok_dynamic_cast, &pos);
      break;
    case ifc_SourceKeyword_Else:
      cache_token(cache, tok_else, &pos);
      break;
    case ifc_SourceKeyword_Enum:
      cache_token(cache, tok_enum, &pos);
      break;
    case ifc_SourceKeyword_Explicit:
      cache_token(cache, tok_explicit, &pos);
      break;
    case ifc_SourceKeyword_Export:
      cache_token(cache, tok_export, &pos);
      break;
    case ifc_SourceKeyword_Extern:
      cache_token(cache, tok_extern, &pos);
      break;
    case ifc_SourceKeyword_False:
      cache_bool_literal(cache, FALSE, &pos);
      break;
    case ifc_SourceKeyword_Float:
      cache_token(cache, tok_float, &pos);
      break;
    case ifc_SourceKeyword_For:
      cache_token(cache, tok_for, &pos);
      break;
    case ifc_SourceKeyword_Friend:
      cache_token(cache, tok_friend, &pos);
      break;
    case ifc_SourceKeyword_Generic:
      cache_token(cache, tok_c11_generic, &pos);
      break;
    case ifc_SourceKeyword_Goto:
      cache_token(cache, tok_goto, &pos);
      break;
    case ifc_SourceKeyword_If:
      cache_token(cache, tok_if, &pos);
      break;
    case ifc_SourceKeyword_Inline:
      cache_token(cache, tok_inline, &pos);
      break;
    case ifc_SourceKeyword_Int:
      cache_token(cache, tok_int, &pos);
      break;
    case ifc_SourceKeyword_Long:
      cache_token(cache, tok_long, &pos);
      break;
    case ifc_SourceKeyword_Mutable:
      cache_token(cache, tok_mutable, &pos);
      break;
    case ifc_SourceKeyword_Namespace:
      cache_token(cache, tok_namespace, &pos);
      break;
    case ifc_SourceKeyword_New:
      cache_token(cache, tok_new, &pos);
      break;
    case ifc_SourceKeyword_Noexcept:
      cache_token(cache, tok_noexcept, &pos);
      break;
    case ifc_SourceKeyword_Nullptr:
      cache_token(cache, tok_nullptr, &pos);
      break;
    case ifc_SourceKeyword_Operator:
      cache_token(cache, tok_operator, &pos);
      break;
    case ifc_SourceKeyword_Pragma:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourceKeyword::Pragma", &error_position);
      break;
    case ifc_SourceKeyword_Private:
      cache_token(cache, tok_private, &pos);
      break;
    case ifc_SourceKeyword_Protected:
      cache_token(cache, tok_private, &pos);
      break;
    case ifc_SourceKeyword_Public:
      cache_token(cache, tok_public, &pos);
      break;
    case ifc_SourceKeyword_Register:
      cache_token(cache, tok_register, &pos);
      break;
    case ifc_SourceKeyword_ReinterpretCast:
      cache_token(cache, tok_reinterpret_cast, &pos);
      break;
    case ifc_SourceKeyword_Requires:
      cache_token(cache, tok_requires, &pos);
      break;
    case ifc_SourceKeyword_Restrict:
      cache_token(cache, tok_restrict, &pos);
      break;
    case ifc_SourceKeyword_Return:
      cache_token(cache, tok_return, &pos);
      break;
    case ifc_SourceKeyword_Short:
      cache_token(cache, tok_short, &pos);
      break;
    case ifc_SourceKeyword_Signed:
      cache_token(cache, tok_signed, &pos);
      break;
    case ifc_SourceKeyword_Sizeof:
      cache_token(cache, tok_sizeof, &pos);
      break;
    case ifc_SourceKeyword_Static:
      cache_token(cache, tok_static, &pos);
      break;
    case ifc_SourceKeyword_StaticAssert:
      cache_token(cache, tok_static_assert, &pos);
      break;
    case ifc_SourceKeyword_StaticCast:
      cache_token(cache, tok_static_cast, &pos);
      break;
    case ifc_SourceKeyword_Struct:
      cache_token(cache, tok_struct, &pos);
      break;
    case ifc_SourceKeyword_Switch:
      cache_token(cache, tok_switch, &pos);
      break;
    case ifc_SourceKeyword_Template:
      cache_token(cache, tok_template, &pos);
      break;
    case ifc_SourceKeyword_This:
      cache_token(cache, tok_this, &pos);
      break;
    case ifc_SourceKeyword_ThreadLocal:
      cache_token(cache, tok_thread_local, &pos);
      break;
    case ifc_SourceKeyword_Throw:
      cache_token(cache, tok_throw, &pos);
      break;
    case ifc_SourceKeyword_True:
      cache_bool_literal(cache, TRUE, &pos);
      break;
    case ifc_SourceKeyword_Try:
      cache_token(cache, tok_try, &pos);
      break;
    case ifc_SourceKeyword_Typedef:
      cache_token(cache, tok_typedef, &pos);
      break;
    case ifc_SourceKeyword_Typeid:
      cache_token(cache, tok_typeid, &pos);
      break;
    case ifc_SourceKeyword_Typename:
      cache_token(cache, tok_typename, &pos);
      break;
    case ifc_SourceKeyword_Union:
      cache_token(cache, tok_union, &pos);
      break;
    case ifc_SourceKeyword_Unsigned:
      cache_token(cache, tok_unsigned, &pos);
      break;
    case ifc_SourceKeyword_Using:
      cache_token(cache, tok_using, &pos);
      break;
    case ifc_SourceKeyword_Virtual:
      cache_token(cache, tok_virtual, &pos);
      break;
    case ifc_SourceKeyword_Void:
      cache_token(cache, tok_void, &pos);
      break;
    case ifc_SourceKeyword_Volatile:
      cache_token(cache, tok_volatile, &pos);
      break;
    case ifc_SourceKeyword_WcharT:
      cache_token(cache, tok_wchar_t, &pos);
      break;
    case ifc_SourceKeyword_While:
      cache_token(cache, tok_while, &pos);
      break;
    case ifc_SourceKeyword_Msvc:
      unexpected_condition();
      break;
    case ifc_SourceKeyword_MsvcAsm:
      cache_token(cache, tok_microsoft_asm, &pos);
      break;
    case ifc_SourceKeyword_MsvcAssume:
      cache_token(cache, tok_assume, &pos);
      break;
    case ifc_SourceKeyword_MsvcAlignof:
      cache_token(cache, tok_alignof, &pos);
      break;
    case ifc_SourceKeyword_MsvcBased:
      cache_token(cache, tok_based, &pos);
      break;
    case ifc_SourceKeyword_MsvcCdecl:
      cache_token(cache, tok_cdecl, &pos);
      break;
    case ifc_SourceKeyword_MsvcClrcall:
      cache_token(cache, tok_clrcall, &pos);
      break;
    case ifc_SourceKeyword_MsvcDeclspec:
      cache_token(cache, tok_declspec, &pos);
      break;
    case ifc_SourceKeyword_MsvcEabi:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourceKeyword::MsvcEabi", &error_position);
      break;
    case ifc_SourceKeyword_MsvcEvent:
      cache_token(cache, tok_event, &pos);
      break;
    case ifc_SourceKeyword_MsvcSehExcept:
      cache_token(cache, tok_except, &pos);
      break;
    case ifc_SourceKeyword_MsvcFastcall:
      cache_token(cache, tok_fastcall, &pos);
      break;
    case ifc_SourceKeyword_MsvcSehFinally:
      cache_token(cache, tok_finally, &pos);
      break;
    case ifc_SourceKeyword_MsvcForceinline:
      cache_token(cache, tok_forceinline, &pos);
      break;
    case ifc_SourceKeyword_MsvcHook:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourceKeyword::MsvcHook", &error_position);
      break;
    case ifc_SourceKeyword_MsvcIdentifier:
      cache_token(cache, tok_microsoft_identifier, &pos);
      break;
    case ifc_SourceKeyword_MsvcIfExists:
      cache_token(cache, tok_if_exists, &pos);
      break;
    case ifc_SourceKeyword_MsvcIfNotExists:
      cache_token(cache, tok_if_not_exists, &pos);
      break;
    case ifc_SourceKeyword_MsvcInt8:
      cache_token(cache, tok_int8, &pos);
      break;
    case ifc_SourceKeyword_MsvcInt16:
      cache_token(cache, tok_int16, &pos);
      break;
    case ifc_SourceKeyword_MsvcInt32:
      cache_token(cache, tok_int32, &pos);
      break;
    case ifc_SourceKeyword_MsvcInt64:
      cache_token(cache, tok_int64, &pos);
      break;
    case ifc_SourceKeyword_MsvcInt128:
#if INT128_EXTENSIONS_ALLOWED
      cache_token(cache, tok_int128, &pos);
#endif /* INT128_EXTENSIONS_ALLOWED */
      break;
    case ifc_SourceKeyword_MsvcInterface:
      cache_token(cache, tok_interface, &pos);
      break;
    case ifc_SourceKeyword_MsvcLeave:
      cache_token(cache, tok_leave, &pos);
      break;
    case ifc_SourceKeyword_MsvcMultipleInheritance:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourceKeyword::MsvcMultipleInheritance",
                                  &error_position);
      break;
    case ifc_SourceKeyword_MsvcNullptr:
      cache_token(cache, tok_nullptr, &pos);
      break;
    case ifc_SourceKeyword_MsvcNovtordisp:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourceKeyword::MsvcNovtordisp",
                                  &error_position);
      break;
    case ifc_SourceKeyword_MsvcPragma:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourceKeyword::MsvcPragma",
                                  &error_position);
      break;
    case ifc_SourceKeyword_MsvcPtr32:
      cache_token(cache, tok_microsoft_ptr32, &pos);
      break;
    case ifc_SourceKeyword_MsvcPtr64:
      cache_token(cache, tok_microsoft_ptr64, &pos);
      break;
    case ifc_SourceKeyword_MsvcRestrict:
      cache_token(cache, tok_restrict, &pos);
      break;
    case ifc_SourceKeyword_MsvcSingleInheritance:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourceKeyword::MsvcSingleInheritance",
                                  &error_position);
      break;
    case ifc_SourceKeyword_MsvcSptr:
      cache_token(cache, tok_microsoft_sptr, &pos);
      break;
    case ifc_SourceKeyword_MsvcStdcall:
      cache_token(cache, tok_stdcall, &pos);
      break;
    case ifc_SourceKeyword_MsvcSuper:
      cache_token(cache, tok_super, &pos);
      break;
    case ifc_SourceKeyword_MsvcThiscall:
      cache_token(cache, tok_thiscall, &pos);
      break;
    case ifc_SourceKeyword_MsvcSehTry:
      cache_token(cache, tok_microsoft_try, &pos);
      break;
    case ifc_SourceKeyword_MsvcUptr:
      cache_token(cache, tok_microsoft_uptr, &pos);
      break;
    case ifc_SourceKeyword_MsvcUuidof:
      cache_token(cache, tok_uuidof, &pos);
      break;
    case ifc_SourceKeyword_MsvcUnaligned:
      cache_token(cache, tok_unaligned, &pos);
      break;
    case ifc_SourceKeyword_MsvcUnhook:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourceKeyword::MsvcUnhook",
                                  &error_position);
      break;
    case ifc_SourceKeyword_MsvcVectorcall:
      cache_token(cache, tok_vectorcall, &pos);
      break;
    case ifc_SourceKeyword_MsvcVirtualInheritance:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourceKeyword::MsvcVirtualInheritance",
                                  &error_position);
      break;
    case ifc_SourceKeyword_MsvcW64:
      cache_token(cache, tok_microsoft_w64, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsClass:
      cache_token(cache, tok_is_class, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsUnion:
      cache_token(cache, tok_is_union, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsEnum:
      cache_token(cache, tok_is_enum, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsPolymorphic:
      cache_token(cache, tok_is_polymorphic, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsEmpty:
      cache_token(cache, tok_is_empty, &pos);
      break;
    case ifc_SourceKeyword_MsvcHasTrivialConstructor:
      cache_token(cache, tok_has_trivial_constructor, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsTriviallyConstructible:
      cache_token(cache, tok_is_trivially_constructible, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsTriviallyCopyConstructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                             "SourceKeyword::MsvcIsTriviallyCopyConstructible",
                             &error_position);
      break;
    case ifc_SourceKeyword_MsvcIsTriviallyCopyAssignable:
      cache_token(cache, tok_is_trivially_copy_assignable, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsTriviallyDestructible:
      cache_token(cache, tok_is_trivially_destructible, &pos);
      break;
    case ifc_SourceKeyword_MsvcHasVirtualDestructor:
      cache_token(cache, tok_has_virtual_destructor, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsNothrowConstructible:
      cache_token(cache, tok_is_nothrow_constructible, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsNothrowCopyConstructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                               "SourceKeyword::MsvcIsNothrowCopyConstructible",
                               &error_position);
      break;
    case ifc_SourceKeyword_MsvcIsNothrowCopyAssignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourceKeyword::MsvcIsNothrowCopyAssignable",
                                  &error_position);
      break;
    case ifc_SourceKeyword_MsvcIsPod:
      cache_token(cache, tok_is_pod, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsAbstract:
      cache_token(cache, tok_is_abstract, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsBaseOf:
      cache_token(cache, tok_is_base_of, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsConvertibleto:
      cache_token(cache, tok_is_convertible_to, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsTrivial:
      cache_token(cache, tok_is_trivial, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsTriviallyCopyable:
      cache_token(cache, tok_is_trivially_copyable, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsStandardLayout:
      cache_token(cache, tok_is_standard_layout, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsLiteralType:
      cache_token(cache, tok_is_literal_type, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsTriviallyMoveConstructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                             "SourceKeyword::MsvcIsTriviallyMoveConstructible",
                             &error_position);
      break;
    case ifc_SourceKeyword_MsvcHasTrivialMoveAssign:
      cache_token(cache, tok_has_trivial_move_assign, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsTriviallyMoveAssignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                                "SourceKeyword::MsvcIsTriviallyMoveAssignable",
                                &error_position);
      break;
    case ifc_SourceKeyword_MsvcIsNothrowMoveAssignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourceKeyword::MsvcIsNothrowMoveAssignable",
                                  &error_position);
      break;
    case ifc_SourceKeyword_MsvcIsConstructible:
      cache_token(cache, tok_is_constructible, &pos);
      break;
    case ifc_SourceKeyword_MsvcUnderlyingType:
      cache_token(cache, tok_underlying_type, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsTriviallyAssignable:
      cache_token(cache, tok_is_trivially_assignable, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsNothrowAssignable:
      cache_token(cache, tok_is_nothrow_assignable, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsDestructible:
      cache_token(cache, tok_is_destructible, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsNothrowDestructible:
      cache_token(cache, tok_is_nothrow_destructible, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsAssignable:
      cache_token(cache, tok_is_assignable, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsAssignableNoCheck:
      cache_token(cache, tok_is_assignable_no_precondition_check, &pos);
      break;
    case ifc_SourceKeyword_MsvcHasUniqueObjectRepresentations:
      cache_token(cache, tok_has_unique_object_representations, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsAggregate:
      cache_token(cache, tok_is_aggregate, &pos);
      break;
    case ifc_SourceKeyword_MsvcBuiltinAddressOf:
      cache_token(cache, tok_builtin_addressof, &pos);
      break;
    case ifc_SourceKeyword_MsvcBuiltinOffsetOf:
      cache_token(cache, tok_builtin_offsetof, &pos);
      break;
    case ifc_SourceKeyword_MsvcBuiltinBitCast:
      cache_token(cache, tok_builtin_bit_cast, &pos);
      break;
    case ifc_SourceKeyword_MsvcBuiltinIsLayoutCompatible:
      cache_token(cache, tok_is_layout_compatible, &pos);
      break;
    case ifc_SourceKeyword_MsvcBuiltinIsPointerInterconvertibleBaseOf:
      cache_token(cache, tok_is_pointer_interconvertible_base_of, &pos);
      break;
    case ifc_SourceKeyword_MsvcBuiltinIsPointerInterconvertibleWithClass:
      cache_token(cache, tok_is_pointer_interconvertible_with_class, &pos);
      break;
    case ifc_SourceKeyword_MsvcBuiltinIsCorrespondingMember:
      cache_token(cache, tok_is_corresponding_member, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsRefClass:
      cache_token(cache, tok_is_ref_class, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsValueClass:
      cache_token(cache, tok_is_value_class, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsSimpleValueClass:
      cache_token(cache, tok_is_simple_value_class, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsInterfaceClass:
      cache_token(cache, tok_is_interface_class, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsDelegate:
      cache_token(cache, tok_is_delegate, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsFinal:
      cache_token(cache, tok_is_final, &pos);
      break;
    case ifc_SourceKeyword_MsvcIsSealed:
      cache_token(cache, tok_is_sealed, &pos);
      break;
    case ifc_SourceKeyword_MsvcHasFinalizer:
      cache_token(cache, tok_has_finalizer, &pos);
      break;
    case ifc_SourceKeyword_MsvcHasCopy:
      cache_token(cache, tok_has_copy, &pos);
      break;
    case ifc_SourceKeyword_MsvcHasAssign:
      cache_token(cache, tok_has_assign, &pos);
      break;
    case ifc_SourceKeyword_MsvcHasUserDestructor:
      cache_token(cache, tok_has_user_destructor, &pos);
      break;
    case ifc_SourceKeyword_MsvcPackCardinality:
      cache_token(cache, tok_sizeof, &pos);
      cache_token(cache, tok_ellipsis, &pos);
      break;
    case ifc_SourceKeyword_MsvcConfusedSizeof:
      cache_token(cache, tok_sizeof, &pos);
      break;
    case ifc_SourceKeyword_MsvcConfusedalignas:
      cache_token(cache, tok_alignas, &pos);
      break;
    default_is_unexpected_str("Unknown SourceKeyword");
  }  /* switch */
}  /* cache_source_keyword */


void an_ifc_module::cache_source_identifier(a_token_cache_ptr    cache,
                                            ifc_SourceIdentifier id,
                                            ifc_Index            index,
                                            ifc_SourceLocation   *locus)
/*
Add tokens corresponding to id to cache.  index is the index into the IFC file
for the additional information needed, depending on the kind of id.  locus is
the location of the Sentence containing id.
*/
{
  a_const_char      *name = NULL;
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (id) {
    case ifc_SourceIdentifier_Plain:
      name = get_string_at_offset((ifc_TextOffset)index);
      break;
    case ifc_SourceIdentifier_Msvc:
      unexpected_condition();
      break;
    case ifc_SourceIdentifier_MsvcBuiltinHugeVal:
      name = "__builtin_huge_val";
      break;
    case ifc_SourceIdentifier_MsvcBuiltinHugeValf:
      name = "__builtin_huge_valf";
      break;
    case ifc_SourceIdentifier_MsvcBuiltinNan:
      name = "__builtin_nan";
      break;
    case ifc_SourceIdentifier_MsvcBuiltinNanf:
      name = "__builtin_nanf";
      break;
    case ifc_SourceIdentifier_MsvcBuiltinNans:
      name = "__builtin_nans";
      break;
    case ifc_SourceIdentifier_MsvcBuiltinNansf:
      name = "__builtin_nansf";
      break;
    default_is_unexpected_str("Unknown SourceIdentifier");
  }  /* switch */
  check_assertion(name != NULL);
  /* Microsoft treats "default" as an identifier rather than a keyword.  That
     is also the case in the token sequences recorded in IFC files. */
  if (strcmp(name, "default") == 0) {
    cache_token(cache, tok_default, &pos);
  } else {
    cache_identifier(cache, name, &pos);
  }  /* if */
}  /* cache_source_identifier */


void an_ifc_module::cache_string(a_token_cache_ptr  cache,
                                 ifc_StringIndex    string,
                                 ifc_SourceLocation *locus)
/*
Add a string literal (with the appropriate character kind) corresponding to
string to cache.  locus is the location of the string.
*/
{
  ifc_StringSort        sort = str_tag(string);
  an_ifc_String_Literal str_lit, *p_lit;
  a_source_position     pos;
  a_character_kind      kind;

  source_position_from_locus(&pos, locus);
  read_prechecked_partition_element(ifc_const_str, str_value(string));
  p_lit = get_String_Literal(&str_lit);
  switch (sort) {
    case ifc_StringSort_Ordinary:
      kind = (a_character_kind)chk_char;
      break;
    case ifc_StringSort_UTF8:
      kind = (a_character_kind)chk_char8_t;
      break;
    case ifc_StringSort_Char16:
      kind = (a_character_kind)chk_char16_t;
      break;
    case ifc_StringSort_Char32:
      kind = (a_character_kind)chk_char32_t;
      break;
    case ifc_StringSort_Wide:
      kind = (a_character_kind)chk_wchar_t;
      break;
    default_is_unexpected_str("Unexpected StringSort");
  }  /* switch */
  if (p_lit->suffix == 0) {
    cache_string_literal(cache, kind, get_string_at_offset(p_lit->start),
                         p_lit->length, &pos);
  } else {
    cache_ud_literal(cache, kind, get_string_at_offset(p_lit->start),
                     p_lit->length, get_string_at_offset(p_lit->suffix),
                     &pos);
  }  /* if */
}  /* cache_string */


void an_ifc_module::cache_word(a_token_cache_ptr cache,
                               an_ifc_Word       *word)
/*
Add token(s) corresponding to word to cache.
*/
{
  ifc_SourceLocation *pos = &word->locus;

  switch (word->sort) {
    case ifc_WordSort_Unknown:
      break;
    case ifc_WordSort_Directive:
      cache_source_directive(cache, (ifc_SourceDirective)word->value, pos);
      break;
    case ifc_WordSort_Punctuator:
      cache_source_punctuator(cache, (ifc_SourcePunctuator)word->value, pos);
      break;
    case ifc_WordSort_Literal:
      cache_source_literal(cache, (ifc_SourceLiteral)word->value, word->index,
                           pos);
      break;
    case ifc_WordSort_Operator:
      cache_source_operator(cache, (ifc_SourceOperator)word->value, pos);
      break;
    case ifc_WordSort_Keyword:
      cache_source_keyword(cache, (ifc_SourceKeyword)word->value, pos);
      break;
    case ifc_WordSort_Identifier:
      cache_source_identifier(cache, (ifc_SourceIdentifier)word->value,
                              word->index, pos);
      break;
    default_is_unexpected_str("Unknown WordSort");
  }  /* switch */
}  /* cache_word */


static inline void translate_word(ifc_NestableWord *nestable_word,
                                  an_ifc_Word      *word)
/*
Convert a NestableWord to a Word.  This works around our need for two different
word types allowing us to reuse Word routines for NestableWord.
*/
{
  word->locus = nestable_word->locus;
  word->index = nestable_word->index;
  word->value = nestable_word->value;
  word->sort = nestable_word->sort;
}  /* translate_word */


void an_ifc_module::cache_word(a_token_cache_ptr cache,
                               ifc_NestableWord  *word)
/*
Add token(s) corresponding to word to cache.
*/
{
  an_ifc_Word aiw;
  translate_word(word, &aiw);
  cache_word(cache, &aiw);
}  /* cache_word */


uint32_t an_ifc_module::cache_sentence(a_token_cache_ptr cache,
                                       ifc_SentenceIndex sentence,
                                       uint32_t          offset,
                                       a_boolean         look_for_stop_token)
/*
Given a SentenceIndex, populate cache with the corresponding tokens.  offset is
the offset into the words to start caching.  If look_for_stop_token is TRUE
and a cached token matches a stop token, remove that token from the cache and
return the index of that token.  Otherwise the return value is meaningless.
*/
{
  an_ifc_Sentence    is, *isp;
  uint32_t           idx = offset;
  a_cached_token_ptr ctp;

  if (sentence == 0) {
    goto done;
  }  /* if */
  read_prechecked_partition_element(ifc_sentence, sentence-1);
  isp = get_Sentence(&is);
  for (; idx < isp->cardinality; ++idx) {
    an_ifc_Word iw, *iwp;
    read_prechecked_partition_element(ifc_word, isp->start + idx);
    iwp = get_Word(&iw);
    ctp = cache->last_token;
    cache_word(cache, iwp);
    if (look_for_stop_token && ctp != cache->last_token &&
        curr_stop_token_stack_entry->
                             stop_tokens[(int)cache->last_token->token] != 0) {
      remove_token_from_cache(cache->last_token, &ctp, cache);
      break;
    }  /* if */
  }  /* if */
done:;
  return idx;
}  /* cache_sentence */


a_boolean an_ifc_module::sentence_is_deleted(ifc_SentenceIndex  sentence)
/*
Given a SentenceIndex, return TRUE if it represents the sequence "= delete".
Otherwise, return FALSE.
*/
{
  a_boolean          result = FALSE, equal_seen = FALSE;
  an_ifc_Sentence    is, *isp;

  if (sentence == 0) {
    goto done;
  }  /* if */
  read_prechecked_partition_element(ifc_sentence, sentence-1);
  isp = get_Sentence(&is);
  for (uint32_t k = 0; k < isp->cardinality; ++k) {
    an_ifc_Word iw, *iwp;
    read_prechecked_partition_element(ifc_word, isp->start + k);
    iwp = get_Word(&iw);
    if (iwp->sort == ifc_WordSort_Directive ||
        (iwp->sort == ifc_WordSort_Punctuator &&
         (uint16_t)iwp->value > (uint16_t)ifc_SourcePunctuator_Msvc)) {
      /* Ignore MSVC-specific insertions. */
      continue;
    } else if (!equal_seen &&
               iwp->sort == ifc_WordSort_Operator &&
               iwp->value == ifc_SourceOperator_Equal) {
      equal_seen = TRUE;
    } else if (equal_seen &&
               iwp->sort == ifc_WordSort_Keyword &&
               iwp->value == ifc_SourceKeyword_Delete) {
      result = TRUE;
      break;
    } else {
      break;
    }  /* if */
  }  /* if */
done:;
  return result;
}  /* sentence_is_deleted */


static void cache_object_traits(a_token_cache_ptr     cache,
                                ifc_ObjectTraits      traits,
                                a_source_position_ptr pos)
/*
Add the tokens corresponding to the given object traits to cache.  pos is the
position to use for the traits.
*/
{
  if (traits & ifc_ObjectTraits_Mutable) {
    cache_token(cache, tok_mutable, pos);
  }  /* if */
  if (traits & ifc_ObjectTraits_Inline) {
    cache_token(cache, tok_inline, pos);
  }  /* if */
  if (traits & ifc_ObjectTraits_Constexpr) {
    cache_token(cache, tok_constexpr, pos);
  }  /* if */
  if (traits & ifc_ObjectTraits_ThreadLocal) {
    cache_token(cache, tok_thread_local, pos);
  }  /* if */
}  /* cache_object_traits */


static void cache_vendor_traits(a_token_cache_ptr     cache,
                                ifc_MsvcTraits        traits,
                                a_boolean             trailing,
                                a_source_position_ptr pos)
/*
Add tokens according to the provided vendor traits to cache.  If trailing is
TRUE, cache the traits that follow a declaration.  Otherwise, cache the traits
that precede a declaration.  pos is the position to use for the traits.
*/
{
  auto cache_declspec_fn = [&](a_const_char *str) {
    cache_token(cache, tok_declspec, pos);
    cache_token(cache, tok_lparen, pos);
    cache_identifier(cache, str, pos);
    cache_token(cache, tok_rparen, pos);
  };  /* cache_declspec_fn */

  if (trailing) {
    /* Nothing currently to do here. */
  } else {
    if (traits & ifc_MsvcTraits_ForceInline) {
      cache_token(cache, tok_forceinline, pos);
    }  /* if */
    if (traits & ifc_MsvcTraits_Naked) {
      cache_declspec_fn("naked");
    }  /* if */
    if (traits & ifc_MsvcTraits_NoAlias) {
      cache_declspec_fn("noalias");
    }  /* if */
    if (traits & ifc_MsvcTraits_NoInline) {
      cache_declspec_fn("noinline");
    }  /* if */
    if (traits & ifc_MsvcTraits_Restrict) {
      cache_declspec_fn("restrict");
    }  /* if */
    if (traits & ifc_MsvcTraits_SafeBuffers) {
      cache_declspec_fn("safebuffers");
    }  /* if */
    if (traits & ifc_MsvcTraits_DllExport) {
      cache_declspec_fn("dllexport");
    }  /* if */
    if (traits & ifc_MsvcTraits_DllImport) {
      cache_declspec_fn("dllimport");
    }  /* if */
    if (traits & ifc_MsvcTraits_Novtable) {
      cache_declspec_fn("novtable");
    }  /* if */
    if (traits & ifc_MsvcTraits_Process) {
      cache_declspec_fn("process");
    }  /* if */
    if (traits & ifc_MsvcTraits_SelectAny) {
      cache_declspec_fn("selectany");
    }  /* if */
  }  /* if */
  if (traits & ifc_MsvcTraits_CodeSegment) {
    /* FIXME: Currently unsupported. */
    pos_st_diagnostic(unhandled_ifc_node_severity,
                      ec_module_file_contains_unsupported_constructs,
                      &error_position, "MsvcTraits::CodeSegment");
  }  /* if */
  if (traits & ifc_MsvcTraits_IntrinsicType) {
    /* FIXME: Currently unsupported. */
    pos_st_diagnostic(unhandled_ifc_node_severity,
                      ec_module_file_contains_unsupported_constructs,
                      &error_position, "MsvcTraits::IntrinsicType");
  }  /* if */
  if (traits & ifc_MsvcTraits_EmptyBases) {
    /* FIXME: Currently unsupported. */
    pos_st_diagnostic(unhandled_ifc_node_severity,
                      ec_module_file_contains_unsupported_constructs,
                      &error_position, "MsvcTraits::EmptyBases");
  }  /* if */
  if (traits & ifc_MsvcTraits_Allocate) {
    /* FIXME: Currently unsupported. */
    pos_st_diagnostic(unhandled_ifc_node_severity,
                      ec_module_file_contains_unsupported_constructs,
                      &error_position, "MsvcTraits::Allocate");
  }  /* if */
  if (traits & ifc_MsvcTraits_Comdat) {
    /* FIXME: Currently unsupported. */
    pos_st_diagnostic(unhandled_ifc_node_severity,
                      ec_module_file_contains_unsupported_constructs,
                      &error_position, "MsvcTraits::Comdat");
  }  /* if */
  if (traits & ifc_MsvcTraits_Uuid) {
    /* FIXME: Currently unsupported. */
    pos_st_diagnostic(unhandled_ifc_node_severity,
                      ec_module_file_contains_unsupported_constructs,
                      &error_position, "MsvcTraits::Uuid");
  }  /* if */
}  /* cache_vendor_traits */


static void cache_func_traits(a_token_cache_ptr     cache,
                              ifc_FunctionTraits    traits,
                              ifc_MsvcTraits        vendor_traits,
                              a_boolean             trailing,
                              a_source_position_ptr pos)
/*
Add the tokens corresponding to the given function and vendor traits to cache.
If trailing is TRUE then cache the traits that follow a function declaration.
Otherwise, cache the traits that precede a function declaration.  pos is the
position to use for the traits.
*/
{
  cache_vendor_traits(cache, vendor_traits, trailing, pos);
  if (trailing) {
    if (traits & ifc_FunctionTraits_PureVirtual) {
      a_constant_ptr cp = alloc_cached_constant();
      cache_token(cache, tok_assign, pos);
      make_zero_of_proper_type(integer_type((an_integer_kind)ik_int), cp);
      cache_literal(cache, cp, pos);
    }  /* if */
    if (traits & ifc_FunctionTraits_Defaulted) {
      cache_token(cache, tok_assign, pos);
      cache_token(cache, tok_default, pos);
    }  /* if */
    if (traits & ifc_FunctionTraits_Deleted) {
      cache_token(cache, tok_assign, pos);
      cache_token(cache, tok_delete, pos);
    }  /* if */
  } else {
    if (traits & ifc_FunctionTraits_Virtual) {
      cache_token(cache, tok_virtual, pos);
    }  /* if */
    if (traits & ifc_FunctionTraits_Explicit) {
      cache_token(cache, tok_explicit, pos);
    }  /* if */
    if (traits & ifc_FunctionTraits_NoReturn) {
      cache_token(cache, tok_noreturn, pos);
    }  /* if */
    if (traits & ifc_FunctionTraits_Immediate) {
      cache_token(cache, tok_consteval, pos);
    } else if (traits & ifc_FunctionTraits_Constexpr) {
      cache_token(cache, tok_constexpr, pos);
    }  /* if */
    /* ifc_FunctionTraits_Inline is intentionally ignored as no IFC function
       should be treated as inline. */
  }  /* if */
  if (traits & ifc_FunctionTraits_HiddenFriend) {
    /* FIXME: Currently unsupported. */
    pos_st_diagnostic(unhandled_ifc_node_severity,
                      ec_module_file_contains_unsupported_constructs,
                      &error_position, "FunctionTraits::HiddenFriend");
  }  /* if */
  if (traits & ifc_FunctionTraits_Constrained) {
    /* FIXME: Currently unsupported. */
    pos_st_diagnostic(unhandled_ifc_node_severity,
                      ec_module_file_contains_unsupported_constructs,
                      &error_position, "FunctionTraits::Constrained");
  }  /* if */
}  /* cache_func_traits */


static void cache_func_type_traits(a_token_cache_ptr      cache,
                                   ifc_FunctionTypeTraits traits,
                                   a_source_position_ptr  pos)
/*
Add the tokens corresponding to the given function type traits to cache.  pos
is the position of the traits.
*/
{
  if (traits & ifc_FunctionTypeTraits_Const) {
    cache_token(cache, tok_const, pos);
  }  /* if */
  if (traits & ifc_FunctionTypeTraits_Volatile) {
    cache_token(cache, tok_volatile, pos);
  }  /* if */
  if (traits & ifc_FunctionTypeTraits_Lvalue) {
    cache_token(cache, tok_ampersand, pos);
  } else if (traits & ifc_FunctionTypeTraits_Rvalue) {
    cache_token(cache, tok_and_and, pos);
  }  /* if */
}  /* cache_func_type_traits */


static void cache_calling_convention(a_token_cache_ptr     cache,
                                     ifc_CallingConvention convention,
                                     a_source_position_ptr pos)
/*
Add the tokens corresponding to the given calling convention to cache.  pos is
the position of the calling convention.
*/
{
  switch (convention) {
    case ifc_CallingConvention_Cdecl:
      cache_token(cache, tok_cdecl, pos);
      break;
    case ifc_CallingConvention_Fast:
      cache_token(cache, tok_fastcall, pos);
      break;
    case ifc_CallingConvention_Std:
      cache_token(cache, tok_stdcall, pos);
      break;
    case ifc_CallingConvention_This:
      cache_token(cache, tok_thiscall, pos);
      break;
    case ifc_CallingConvention_Clr:
      cache_token(cache, tok_clrcall, pos);
      break;
    case ifc_CallingConvention_Vector:
      cache_token(cache, tok_vectorcall, pos);
      break;
    case ifc_CallingConvention_Eabi:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_calling_conv,
                        &error_position, "CallingConvention::Eabi");
      break;
    default_is_unexpected_str("Unexpected CallingConvention");
  }  /* switch */
}  /* cache_calling_convention */


void an_ifc_module::cache_exception_spec(a_token_cache_ptr         cache,
                                         ifc_NoexceptSpecification *eh_spec,
                                         a_source_position_ptr     pos)
/*
Add the tokens corresponding to the given exception specification (eh_spec) to
cache.  pos is the position of the exception specification.
*/
{
  if (eh_spec->sort == ifc_NoexceptSort_None ||
      eh_spec->sort == ifc_NoexceptSort_Inferred) goto done;
  cache_token(cache, tok_noexcept, pos);
  cache_token(cache, tok_lparen, pos);
  switch (eh_spec->sort) {
    case ifc_NoexceptSort_False:
      cache_bool_literal(cache, false, pos);
      break;
    case ifc_NoexceptSort_True:
      cache_bool_literal(cache, true, pos);
      break;
    case ifc_NoexceptSort_Expression:
      cache_sentence(cache, eh_spec->words);
      break;
    case ifc_NoexceptSort_Unenforced:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("NoexceptSort::Unenforced", &error_position);
      break;
    case ifc_NoexceptSort_Inferred:
      /* Unreachable, but place here to have all enums covered. */
      unexpected_condition_str("NoexceptSort::Inferred is not expected here");
      break;
    case ifc_NoexceptSort_None:
      /* Unreachable, but place here to have all enums covered. */
      unexpected_condition_str("NoexceptSort::None is not expected here");
      break;
    default_is_unexpected_str("Unexpected NoexceptSpecification");
  }  /* switch */
  cache_token(cache, tok_rparen, pos);
done:;
}  /* cache_exception_spec */


void an_ifc_module::cache_scope_member_sequence(
                                          a_token_cache_ptr         cache,
                                          an_ifc_Ref<ifc_DeclIndex> scope_decl,
                                          ifc_Sequence              seq)
/*
Cache a sequence (seq) of IFC scope member declarations into the cache.
scope_decl specifies the current home scope declaration being processed prior
to this call -- so that we can determine if caching of a given declaration in
(seq) can be performed in the current scope or should be deferred.
*/
{
  /* Provide a consumer function that accepts a given scope member and caches
     the associated IFC declaration into the cache. */
  auto decl_consumer = [this, cache, scope_decl](an_ifc_Scope_Member *ismp) {
    /* FIXME: We need to queue declarations in a different scope for later
       caching.  E.g., the status quo doesn't work for explicit instantiations
       of member functions. */
    Opt<an_ifc_Ref<ifc_DeclIndex>> opt_scope_ref;

    opt_scope_ref = get_ifc_home_scope_decl(ismp->index);
    if (opt_scope_ref.has_value() && *opt_scope_ref == scope_decl) {
      cache_decl(cache, ismp->index);
    }  /* if */
  };
  /* Iterate over the sequence calling decl_consumer for each element. */
  traverse_scope_member_sequence(seq, decl_consumer);
}  /* cache_scope_member_sequence */


static void cache_basic_specifiers(a_token_cache_ptr     cache,
                                   ifc_BasicSpecifiers   specifiers,
                                   a_source_position_ptr pos)
/*
Add tokens corresponding to specifiers to cache.  pos is the position of the
specifier(s).
*/
{
  if (specifiers & ifc_BasicSpecifiers_C) {
    cache_token(cache, tok_extern, pos);
    cache_string_literal(cache, chk_char, "C", 2, pos);
  }  /* if */
  if (specifiers & ifc_BasicSpecifiers_Deprecated) {
    cache_token(cache, tok_lbracket, pos);
    cache_token(cache, tok_lbracket, pos);
    cache_identifier(cache, "deprecated", pos);
    cache_token(cache, tok_rbracket, pos);
    cache_token(cache, tok_rbracket, pos);
  }  /* if */
}  /* cache_basic_specifiers */


static void cache_qualifiers(a_token_cache_ptr     cache,
                             ifc_Qualifiers        qualifiers,
                             a_source_position_ptr pos)
/*
Add tokens corresponding to qualifiers to cache.  pos is the position of the
qualifier(s).
*/
{
  if (qualifiers & ifc_Qualifier_Const) {
    cache_token(cache, tok_const, pos);
  }  /* if */
  if (qualifiers & ifc_Qualifier_Volatile) {
    cache_token(cache, tok_volatile, pos);
  }  /* if */
  if (qualifiers & ifc_Qualifier_Restrict) {
    cache_token(cache, tok_restrict, pos);
  }  /* if */
}  /* cache_qualifiers */


void an_ifc_module::cache_scope(a_token_cache_ptr  cache,
                                ifc_ScopeIndex     scope,
                                ifc_SourceLocation *locus)
/*
For the given IFC scope (i.e., a class or namespace definition), cache tokens
corresponding to the brace-enclosed declarations of the scope (including the
braces).  Note that in the case of a class scope this does not include the
base class specifiers list.  locus is the location of the scope.  A null IFC
scope is handled by not caching any tokens.
*/
{
  an_ifc_Scope_Descriptor isd, *isdp;
  a_source_position       pos;

  if (scope == 0) goto done;
  source_position_from_locus(&pos, locus);
  read_prechecked_partition_element(ifc_scope_desc, scope - 1);
  isdp = get_Scope_Descriptor(&isd);
  cache_token(cache, tok_lbrace, &pos);
  for (ifc_Index_type idx = 0; idx < isdp->cardinality; ++idx) {
    an_ifc_Scope_Member ism, *ismp;
    read_prechecked_partition_element(ifc_scope_member, isdp->start + idx);
    ismp = get_Scope_Member(&ism);
    cache_decl(cache, ismp->index);
  }  /* for */
  cache_token(cache, tok_rbrace, &pos);
done:;
}  /* cache_scope */


template<typename a_Name_Cache_Fn, typename a_Scope_Cache_Fn>
inline void an_ifc_module::cache_scope_decl(a_token_cache_ptr  cache,
                                            ifc_DeclIndex      decl_idx,
                                            ifc_TypeIndex      type,
                                            a_Name_Cache_Fn    cache_name_fn,
                                            a_Scope_Cache_Fn   cache_scope_fn,
                                            ifc_SourceLocation *locus)
/*
Cache the tokens corresponding to the given scope decl (indexed in the IFC by
decl_idx).  type represents the IFC type representing the introducing keyword.
cache_name_fn is a lambda accepting a_source_position_ptr interpretation of
locus that's called to cache the name of the scope.  cache_scope_fn is a lambda
accepting a_source_position_ptr interpretation of locus that's called to cache
the scope's body (e.g., for a class the member-specification).  Finally, locus
is the location of the given scope decl.

FIXME: Remove this version of cache_scope_decl once names can be cached
properly for specializations using a NameIndex, and similarly the scope's body
can be consistently cached via a ScopeIndex.
*/
{
  a_source_position pos;

  check_assertion(type_tag(type) == ifc_TypeSort_Fundamental);
  source_position_from_locus(&pos, locus);
  /* Cache the struct/class/union/namespace/__interface keyword. */
  cache_type(cache, type, locus);
  /* Cache any attributes. */
  cache_attrs(cache, decl_idx);
  /* Cache the name. */
  cache_name_fn(&pos);
  /* Cache the scope's body. e.g., for a class the member-specification. */
  cache_scope_fn(&pos);
}  /* cache_scope_decl */


void an_ifc_module::cache_scope_decl(a_token_cache_ptr  cache,
                                     ifc_DeclIndex      decl_idx,
                                     ifc_TypeIndex      type,
                                     ifc_NameIndex      name,
                                     ifc_TypeIndex      base,
                                     ifc_ScopeIndex     scope,
                                     ifc_SourceLocation *locus)
/*
Cache the tokens corresponding to the given scope decl (indexed in the IFC by
decl_idx).  type represents the IFC type representing the introducing keyword.
name represents the name of the scope decl.  base represents any associated
base classes and as such is only valid for a class declaration.  scope
represents the declaration's body (e.g., for a class the member-specification).
Finally, locus is the location of the given scope decl.
*/
{
  auto cache_name_fn = [this, cache, name, locus](a_source_position_ptr pos) {
    cache_name(cache, name, locus);
  };
  auto cache_scope_fn = [this, cache, base, type, scope, locus](
                                                   a_source_position_ptr pos) {
    /* If there are bases specified, cache the bases. */
    if (base != 0) {
      cache_token(cache, tok_colon, pos);
      cache_type(cache, base, locus);
    }  /* if */
    cache_scope(cache, scope, locus);
    {
      /* Read the fundamental type so that we can determine if we're caching a
         namespace. */
      an_ifc_TypeSort_Fundamental itsf, *itsfp;

      read_prechecked_partition_element(type);
      itsfp = get_TypeSort_Fundamental(&itsf);
      /* If we aren't caching a namespace, add a semicolon. */
      if (itsfp->basis != ifc_TypeBasis_Namespace) {
        cache_token(cache, tok_semicolon, pos);
      }  /* if */
    }
  };
  cache_scope_decl(cache, decl_idx, type, cache_name_fn, cache_scope_fn,
                   locus);
}  /* cache_scope_decl */


void an_ifc_module::cache_type_first_pass(a_token_cache_ptr  cache,
                                          ifc_TypeIndex      type,
                                          ifc_SourceLocation *locus)
/*
Add the tokens to cache corresponding to the portion of the given type that
precedes an identifier.  locus is the source location of the entity referring
to the type.  This routine will often need to be called in concert with
cache_type_second_pass, which will cache tokens corresponding to the portion
of type that follows an identifier.  For example:

   ~v~ This routine caches this portion of the type
   int arr[3];
          ~^~ but not this portion.

See form_type_first_part and form_type_second_part for more details as to why
this is needed.
*/
{
  ifc_TypeSort      tag = type_tag(type);
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  read_prechecked_partition_element(type);
  switch (tag) {
    case ifc_TypeSort_VendorExtension:
      issue_unsupported_node_diag("TypeSort::VendorExtension",
                                  &error_position);
      break;
    case ifc_TypeSort_Fundamental:
      { an_ifc_TypeSort_Fundamental itsf, *itsfp;
        itsfp = get_TypeSort_Fundamental(&itsf);
        switch (itsfp->sign) {
          case ifc_TypeSign_Plain:
            break;
          case ifc_TypeSign_Signed:
            cache_token(cache, tok_signed, &pos);
            break;
          case ifc_TypeSign_Unsigned:
            cache_token(cache, tok_unsigned, &pos);
            break;
          default_is_unexpected_str("Unexpected TypeSign");
        }  /* if */
        switch (itsfp->basis) {
          case ifc_TypeBasis_Void:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_void, &pos);
            break;
          case ifc_TypeBasis_Bool:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_bool, &pos);
            break;
          case ifc_TypeBasis_Char:
            switch (itsfp->precision) {
              case ifc_TypePrecision_Default:
                cache_token(cache, tok_char, &pos);
                break;
              case ifc_TypePrecision_Bit8:
                cache_token(cache, tok_char8_t, &pos);
                break;
              case ifc_TypePrecision_Bit16:
                cache_token(cache, tok_char16_t, &pos);
                break;
              case ifc_TypePrecision_Bit32:
                cache_token(cache, tok_char32_t, &pos);
                break;
              default:
                unexpected_condition();
            }  /* switch */
            break;
          case ifc_TypeBasis_Wchar_t:
            cache_token(cache, tok_wchar_t, &pos);
            break;
          case ifc_TypeBasis_Int:
            switch (itsfp->precision) {
              case ifc_TypePrecision_Default:
                cache_token(cache, tok_int, &pos);
                break;
              case ifc_TypePrecision_Short:
                cache_token(cache, tok_short, &pos);
                break;
              case ifc_TypePrecision_Long:
                cache_token(cache, tok_long, &pos);
                break;
              case ifc_TypePrecision_Bit8:
                cache_token(cache, tok_int8, &pos);
                break;
              case ifc_TypePrecision_Bit16:
                cache_token(cache, tok_int16, &pos);
                break;
              case ifc_TypePrecision_Bit32:
                cache_token(cache, tok_int32, &pos);
                break;
              case ifc_TypePrecision_Bit64:
                cache_token(cache, tok_int64, &pos);
                break;
              case ifc_TypePrecision_Bit128:
#if INT128_EXTENSIONS_ALLOWED
                cache_token(cache, tok_int128, &pos);
#endif /* INT128_EXTENSIONS_ALLOWED */
                break;
              default_is_unexpected();
            }  /* switch */
            break;
          case ifc_TypeBasis_Float:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_float, &pos);
            break;
          case ifc_TypeBasis_Double:
            if (itsfp->precision == ifc_TypePrecision_Long) {
              cache_token(cache, tok_long, &pos);
            } else {
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            }  /* if */
            cache_token(cache, tok_double, &pos);
            break;
          case ifc_TypeBasis_Nullptr:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_resolved_type_token(cache, standard_nullptr_type(), &pos);
            break;
          case ifc_TypeBasis_Ellipsis:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_ellipsis, &pos);
            break;
          case ifc_TypeBasis_SegmentType:
            /* FIXME: Currently unsupported. */
            issue_unsupported_node_diag("TypeBasis::SegmentType",
                                        &error_position);
            break;
          case ifc_TypeBasis_Class:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_class, &pos);
            break;
          case ifc_TypeBasis_Struct:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_struct, &pos);
            break;
          case ifc_TypeBasis_Union:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_union, &pos);
            break;
          case ifc_TypeBasis_Enum:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_enum, &pos);
            break;
          case ifc_TypeBasis_Typename:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_typename, &pos);
            break;
          case ifc_TypeBasis_Namespace:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_namespace, &pos);
            break;
          case ifc_TypeBasis_Interface:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_interface, &pos);
            break;
          case ifc_TypeBasis_Function:
            /* FIXME: Currently unsupported. */
            issue_unsupported_node_diag("TypeBasis::Function",
                                        &error_position);
            break;
          case ifc_TypeBasis_Empty:
            break;
          case ifc_TypeBasis_VariableTemplate:
            /* FIXME: Currently unsupported. */
            issue_unsupported_node_diag("TypeBasis::VariableTemplate",
                                        &error_position);
            break;
          case ifc_TypeBasis_Auto:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_auto_type, &pos);
            break;
          case ifc_TypeBasis_DecltypeAuto:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            cache_token(cache, tok_decltype, &pos);
            cache_token(cache, tok_lparen, &pos);
            cache_token(cache, tok_auto_type, &pos);
            cache_token(cache, tok_rparen, &pos);
            break;
          default_is_unexpected_str("Unexpected TypeBasis");
        }  /* switch */
      }
      break;
    case ifc_TypeSort_Designated:
      { an_ifc_TypeSort_Designated itsd, *itsdp;
        itsdp = get_TypeSort_Designated(&itsd);
        if (is_name_qualifiable(itsdp->decl)) {
          cache_qualified_name_from_decl(cache, itsdp->decl, locus);
        } else {
          /* We are contextually forbidden from qualifying this name or the
             declaration type otherwise is considered to never appear
             with a qualified name.

             This can happen when building an unqualified-id within a dependent
             context. */
          cache_name_from_decl(cache, itsdp->decl, locus);
        }  /* if */
      }
      break;
    case ifc_TypeSort_Tor:
      /* This type should only be encountered when processing a constructor,
         and that is directly handled with that constructor declaration.*/
      unexpected_condition();
      break;
    case ifc_TypeSort_Syntactic:
      { an_ifc_TypeSort_Syntactic itss, *itssp;
        itssp = get_TypeSort_Syntactic(&itss);
        cache_expr(cache, itssp->expr);
      }
      break;
    case ifc_TypeSort_Expansion:
      { an_ifc_TypeSort_Expansion itse, *itsep;
        itsep = get_TypeSort_Expansion(&itse);
        cache_type_first_pass(cache, itsep->pack, locus);
        cache_token(cache, tok_ellipsis, &pos);
      }
      break;
    case ifc_TypeSort_Pointer:
      { an_ifc_TypeSort_Pointer itsp, *itspp;
        itspp = get_TypeSort_Pointer(&itsp);
        cache_type_first_pass(cache, itspp->pointee, locus);
        if (type_tag(itspp->pointee) != ifc_TypeSort_PointerToMember) {
          /* The tok_star will already have been cached if the pointee is a
             pointer-to-member. */
          cache_token(cache, tok_star, &pos);
        }  /* if */
      }
      break;
    case ifc_TypeSort_PointerToMember:
      { an_ifc_TypeSort_PointerToMember itsptm, *itsptmp;
        itsptmp = get_TypeSort_PointerToMember(&itsptm);
        if (type_tag(itsptmp->member) == ifc_TypeSort_Method) {
          an_ifc_TypeSort_Method itsm, *itsmp;
          read_prechecked_partition_element(itsptmp->member);
          itsmp = get_TypeSort_Method(&itsm);
          cache_type(cache, itsmp->target, locus);
          cache_token(cache, tok_lparen, &pos);
          cache_calling_convention(cache, itsmp->convention, &pos);
          cache_type(cache, itsmp->scope, locus);
          cache_token(cache, tok_colon_colon, &pos);
        } else {
          cache_type(cache, itsptmp->scope, locus);
          cache_token(cache, tok_colon_colon, &pos);
          cache_type(cache, itsptmp->member, locus);
        }  /* if */
        cache_token(cache, tok_star, &pos);
      }
      break;
    case ifc_TypeSort_LvalueReference:
      { an_ifc_TypeSort_LvalueReference itslr, *itslrp;
        itslrp = get_TypeSort_LvalueReference(&itslr);
        cache_type_first_pass(cache, itslrp->referee, locus);
        cache_token(cache, tok_ampersand, &pos);
      }
      break;
    case ifc_TypeSort_RvalueReference:
      { an_ifc_TypeSort_RvalueReference itsrr, *itsrrp;
        itsrrp = get_TypeSort_RvalueReference(&itsrr);
        cache_type_first_pass(cache, itsrrp->referee, locus);
        cache_token(cache, tok_and_and, &pos);
      }
      break;
    case ifc_TypeSort_Function:
      { an_ifc_TypeSort_Function itsf, *itsfp;
        itsfp = get_TypeSort_Function(&itsf);
        cache_type(cache, itsfp->target, locus);
        cache_token(cache, tok_lparen, &pos);
        cache_calling_convention(cache, itsfp->convention, &pos);
      }
      break;
    case ifc_TypeSort_Method:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("TypeSort::Method", &error_position);
      break;
    case ifc_TypeSort_Array:
      { an_ifc_TypeSort_Array itsa, *itsap;
        itsap = get_TypeSort_Array(&itsa);
        cache_type_first_pass(cache, itsap->element, locus);
      }
      break;
    case ifc_TypeSort_Typename:
      { an_ifc_TypeSort_Typename itst, *itstp;
        itstp = get_TypeSort_Typename(&itst);
        cache_token(cache, tok_typename, &pos);
        cache_expr(cache, itstp->path);
      }
      break;
    case ifc_TypeSort_Qualified:
      { an_ifc_TypeSort_Qualified itsq, *itsqp;
        itsqp = get_TypeSort_Qualified(&itsq);
        cache_type_first_pass(cache, itsqp->unqualified, locus);
        if (itsqp->qualifiers & ifc_Qualifier_Const) {
          cache_token(cache, tok_const, &pos);
        }  /* if */
        if (itsqp->qualifiers & ifc_Qualifier_Volatile) {
          cache_token(cache, tok_volatile, &pos);
        }  /* if */
        if (itsqp->qualifiers & ifc_Qualifier_Restrict) {
          cache_token(cache, tok_restrict, &pos);
        }  /* if */
      }
      break;
    case ifc_TypeSort_Base:
      { an_ifc_TypeSort_Base itsb, *itsbp;
        itsbp = get_TypeSort_Base(&itsb);
        cache_access(cache, itsbp->access, /*cache_colon=*/FALSE, &pos);
        if (itsbp->shared) {
          cache_token(cache, tok_virtual, &pos);
        }  /* if */
        cache_type(cache, itsbp->type, locus);
        if (itsbp->pack_expanded) {
          cache_token(cache, tok_ellipsis, &pos);
        }  /* if */
      }
      break;
    case ifc_TypeSort_Decltype:
      { an_ifc_TypeSort_Decltype itsd, *itsdp;
        itsdp = get_TypeSort_Decltype(&itsd);
        /* decltype constructs are currently represented as token sequences. */
        cache_syntax(cache, itsdp->expr);
      }
      break;
    case ifc_TypeSort_Placeholder:
      { an_ifc_TypeSort_Placeholder itsp, *itspp;
        itspp = get_TypeSort_Placeholder(&itsp);
        if (itspp->basis == ifc_TypeBasis_Auto) {
          cache_token(cache, tok_auto, &pos);
        } else {
          check_assertion(itspp->basis == ifc_TypeBasis_DecltypeAuto);
          cache_token(cache, tok_decltype, &pos);
          cache_token(cache, tok_lparen, &pos);
          cache_token(cache, tok_auto, &pos);
          cache_token(cache, tok_rparen, &pos);
        }  /* if */
      }
      break;
    case ifc_TypeSort_Tuple:
      { an_ifc_TypeSort_Tuple itst, *itstp;
        unsigned int i;
        itstp = get_TypeSort_Tuple(&itst);
        for (i = 0; i < itstp->cardinality; ++i) {
          ifc_TypeIndex ti;
          read_prechecked_partition_element(ifc_heap_type,
                                            itstp->start + i);
          GET_TypeIndex(ti, /*from_header=*/FALSE);
          cache_type(cache, ti, locus);
          if (i+1 < itstp->cardinality) {
            cache_token(cache, tok_comma, &pos);
          }  /* if */
        }  /* for */
      }
      break;
    case ifc_TypeSort_Forall:
      { an_ifc_TypeSort_Forall itsfa, *itsfap;
        itsfap = get_TypeSort_Forall(&itsfa);
        cache_template_head(cache, itsfap->chart, &pos);
        cache_type(cache, itsfap->subject, locus);
      }
      break;
    case ifc_TypeSort_Unaligned:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("TypeSort::Unaligned", &error_position);
      break;
    case ifc_TypeSort_SyntaxTree:
      { an_ifc_TypeSort_SyntaxTree itsst, *itsstp;
        itsstp = get_TypeSort_SyntaxTree(&itsst);
        cache_syntax(cache, itsstp->syntax);
      }
      break;
    case ifc_TypeSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected TypeSort");
  }  /* switch */
}  /* cache_type_first_pass */


void an_ifc_module::cache_type_second_pass(a_token_cache_ptr  cache,
                                           ifc_TypeIndex      type,
                                           ifc_SourceLocation *locus)
/*
Add the tokens to cache corresponding to the portion of the given type that
follows an identifier.  locus is the source location of the entity referring
to the type.  This routine will often need to be called in concert with
cache_type_first_pass, which will cache tokens corresponding to the portion
of type that precedes an identifier.  For example:

          ~v~ This routine caches this portion of the type
   int arr[3];
   ~^~ but not this portion.

See form_type_first_part and form_type_second_part for more details as to why
this is needed.
*/
{
  ifc_TypeSort      tag = type_tag(type);
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  read_prechecked_partition_element(type);
  switch (tag) {
    case ifc_TypeSort_VendorExtension:
      issue_unsupported_node_diag("TypeSort::VendorExtension",
                                  &error_position);
      break;
    case ifc_TypeSort_Tor:
      /* This type should only be encountered when processing a constructor,
         and that is directly handled with that constructor declaration.*/
      unexpected_condition();
      break;
    case ifc_TypeSort_Pointer:
      { an_ifc_TypeSort_Pointer itsp, *itspp;
        itspp = get_TypeSort_Pointer(&itsp);
        cache_type_second_pass(cache, itspp->pointee, locus);
      }
      break;
    case ifc_TypeSort_PointerToMember:
      { an_ifc_TypeSort_PointerToMember itsptm, *itsptmp;
        itsptmp = get_TypeSort_PointerToMember(&itsptm);
        if (type_tag(itsptmp->member) == ifc_TypeSort_Method) {
          an_ifc_TypeSort_Method itsm, *itsmp;
          read_prechecked_partition_element(itsptmp->member);
          itsmp = get_TypeSort_Method(&itsm);
          cache_token(cache, tok_rparen, &pos);
          cache_token(cache, tok_lparen, &pos);
          if (itsmp->source != 0) {
            cache_type(cache, itsmp->source, locus);
          }  /* if */
          cache_token(cache, tok_rparen, &pos);
          cache_exception_spec(cache, &itsmp->eh_spec, &pos);
          cache_func_type_traits(cache, itsmp->traits, &pos);
        }  /* if */
      }
      break;
    case ifc_TypeSort_LvalueReference:
      { an_ifc_TypeSort_LvalueReference itslr, *itslrp;
        itslrp = get_TypeSort_LvalueReference(&itslr);
        cache_type_second_pass(cache, itslrp->referee, locus);
      }
      break;
    case ifc_TypeSort_RvalueReference:
      { an_ifc_TypeSort_RvalueReference itsrr, *itsrrp;
        itsrrp = get_TypeSort_RvalueReference(&itsrr);
        cache_type_second_pass(cache, itsrrp->referee, locus);
      }
      break;
    case ifc_TypeSort_Function:
      { an_ifc_TypeSort_Function itsf, *itsfp;
        itsfp = get_TypeSort_Function(&itsf);
        cache_token(cache, tok_rparen, &pos);
        cache_token(cache, tok_lparen, &pos);
        if (itsfp->source != 0) {
          cache_type(cache, itsfp->source, locus);
        }  /* if */
        cache_token(cache, tok_rparen, &pos);
        cache_func_type_traits(cache, itsfp->traits, &pos);
        cache_exception_spec(cache, &itsfp->eh_spec, &pos);
      }
      break;
    case ifc_TypeSort_Array:
      { an_ifc_TypeSort_Array itsa, *itsap;
        itsap = get_TypeSort_Array(&itsa);
        cache_token(cache, tok_lbracket, &pos);
        if (itsap->extent != 0) {
          cache_expr(cache, itsap->extent);
        }  /* if */
        cache_token(cache, tok_rbracket, &pos);
        cache_type_second_pass(cache, itsap->element, locus);
      }
      break;
    case ifc_TypeSort_Qualified:
      { an_ifc_TypeSort_Qualified itsq, *itsqp;
        itsqp = get_TypeSort_Qualified(&itsq);
        cache_type_second_pass(cache, itsqp->unqualified, locus);
      }
      break;
    case ifc_TypeSort_Expansion:
      { an_ifc_TypeSort_Expansion itse, *itsep;
        itsep = get_TypeSort_Expansion(&itse);
        cache_type_second_pass(cache, itsep->pack, locus);
      }
      break;
    case ifc_TypeSort_Fundamental:
    case ifc_TypeSort_Designated:
    case ifc_TypeSort_Syntactic:
    case ifc_TypeSort_Method:
    case ifc_TypeSort_Typename:
    case ifc_TypeSort_Base:
    case ifc_TypeSort_Decltype:
    case ifc_TypeSort_Placeholder:
    case ifc_TypeSort_Tuple:
    case ifc_TypeSort_Forall:
    case ifc_TypeSort_Unaligned:
    case ifc_TypeSort_SyntaxTree:
      /* All of these were completely handled by the first pass. */
      break;
    case ifc_TypeSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected TypeSort");
  }  /* switch */
}  /* cache_type_second_pass */


void an_ifc_module::cache_type(a_token_cache_ptr  cache,
                               ifc_TypeIndex      type,
                               ifc_SourceLocation *locus)
/*
Add the tokens to cache corresponding to the given type.  locus is the source
location of the entity referring to the type.  This routine should only be
called when there is no identifier portion involved and therefore both the
preceding and following portions of the type can be immediately cached.  If
there is an identifier portion involved, cache_type_first_pass and
cache_type_second_pass should be used instead.
*/
{
  cache_type_first_pass(cache, type, locus);
  cache_type_second_pass(cache, type, locus);
}  /* cache_type */


void an_ifc_module::cache_chart(a_token_cache_ptr     cache,
                                ifc_ChartIndex        chart,
                                a_source_position_ptr pos)
/*
Add the tokens corresponding to the given chart to cache.  The caller is
expected to have already cached the "template" keyword if it's required.  pos
is the position of the chart.
*/
{
  ifc_ChartSort     tag = chart_tag(chart);
  ifc_ExprIndex     constraint = (ifc_ExprIndex)0;

  cache_token(cache, tok_lt, pos);
  read_prechecked_partition_element(chart);
  switch (tag) {
    case ifc_ChartSort_None:
      /* No arguments to the template (i.e., specialization). */
      break;
    case ifc_ChartSort_Unilevel:
      { an_ifc_ChartSort_Unilevel icsu, *icsup;
        icsup = get_ChartSort_Unilevel(&icsu);
        constraint = icsup->constraint;
        for (ifc_Index_type idx = 0; idx < icsup->cardinality; ++idx) {
          if (idx > 0) cache_token(cache, tok_comma, pos);
          cache_decl(cache,
                     make_decl_index(ifc_DeclSort_Parameter,
                                     icsup->start + idx));
        }  /* for */
      }
      break;
    case ifc_ChartSort_Multilevel:
      { an_ifc_ChartSort_Multilevel icsm, *icsmp;
        icsmp = get_ChartSort_Multilevel(&icsm);
        for (ifc_Index_type idx = 0; idx < icsmp->cardinality; ++idx) {
          if (idx > 0) cache_token(cache, tok_comma, pos);
          /* FIXME: Is this correct? */
          cache_decl(cache,
                     make_decl_index(ifc_DeclSort_Temploid,
                                     icsmp->start + idx));
        }  /* for */
        issue_unsupported_node_diag("ChartSort::Multilevel", &error_position);
      }
      break;
    case ifc_ChartSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected ChartSort");
  }  /* switch */
  cache_token(cache, tok_gt, pos);
  if (constraint != (ifc_ExprIndex)0) {
    /* The template parameter list is followed by a requires-clause. */
    cache_expr(cache, constraint);
  }  /* if */
}  /* cache_chart */


void an_ifc_module::cache_chart(a_token_cache_ptr  cache,
                                ifc_ChartIndex     chart,
                                ifc_SourceLocation *locus)
/*
Add the tokens corresponding to the given chart to cache.  The caller is
expected to have already cached the "template" keyword if it's required.  locus
is the location of the chart.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  cache_chart(cache, chart, &pos);
}  /* cache_chart */


void an_ifc_module::cache_operator(a_token_cache_ptr  cache,
                                   ifc_Operator       op,
                                   ifc_SourceLocation *locus)
/*
Add the tokens corresponding to the given Operator to cache.  locus is the
location of the operator.
*/
{
  uint16_t op_val = operator_index(op);

  switch (operator_tag(op)) {
    case ifc_OperatorSort_Niladic:
      cache_operator(cache, (ifc_NiladicOperator)op_val, locus);
      break;
    case ifc_OperatorSort_Monadic:
      cache_operator(cache, (ifc_MonadicOperator)op_val, locus);
      break;
    case ifc_OperatorSort_Dyadic:
      cache_operator(cache, (ifc_DyadicOperator)op_val, locus);
      break;
    case ifc_OperatorSort_Triadic:
      cache_operator(cache, (ifc_TriadicOperator)op_val, locus);
      break;
    case ifc_OperatorSort_Storage:
      cache_operator(cache, (ifc_StorageOperator)op_val, locus);
      break;
    case ifc_OperatorSort_Variadic:
      cache_operator(cache, (ifc_VariadicOperator)op_val, locus);
      break;
    default_is_unexpected_str("Unexpected OperatorSort");
  }  /* switch */
}  /* cache_operator */


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
/* Remove ARG_UNUSED as well. */
void an_ifc_module::cache_operator(ARG_UNUSED a_token_cache_ptr cache,
                                   ifc_NiladicOperator          op,
                                   ifc_SourceLocation           *locus)
/*
Add the tokens corresponding to the given Niladic Operator to cache.  locus is
the location of the operator.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (op) {
    case ifc_NiladicOperator_Unknown:
      unexpected_condition();
      break;
    case ifc_NiladicOperator_Phantom:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("NiladicOperator::Phantom", &error_position);
      break;
    case ifc_NiladicOperator_Constant:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("NiladicOperator::Constant",
                                  &error_position);
      break;
    case ifc_NiladicOperator_Nil:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("NiladicOperator::Nil", &error_position);
      break;
    case ifc_NiladicOperator_Msvc:
      unexpected_condition();
      break;
    case ifc_NiladicOperator_MsvcConstantObject:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("NiladicOperator::MsvcConstantObject",
                                  &error_position);
      break;
    case ifc_NiladicOperator_MsvcLambda:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("NiladicOperator::MsvcLambda",
                                  &error_position);
      break;
    default_is_unexpected_str("Unexpected NiladicOperator");
  }  /* switch */
}  /* cache_operator */


void an_ifc_module::cache_operator(a_token_cache_ptr   cache,
                                   ifc_MonadicOperator op,
                                   ifc_SourceLocation  *locus)
/*
Add the tokens corresponding to the given Monadic Operator to cache.  locus is
the location of the operator.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (op) {
    case ifc_MonadicOperator_Unknown:
      unexpected_condition();
      break;
    case ifc_MonadicOperator_Plus:
      cache_token(cache, tok_plus, &pos);
      break;
    case ifc_MonadicOperator_Negate:
      cache_token(cache, tok_minus, &pos);
      break;
    case ifc_MonadicOperator_Deref:
      cache_token(cache, tok_star, &pos);
      break;
    case ifc_MonadicOperator_Address:
      cache_token(cache, tok_ampersand, &pos);
      break;
    case ifc_MonadicOperator_Complement:
      cache_token(cache, tok_compl, &pos);
      break;
    case ifc_MonadicOperator_Not:
      cache_token(cache, tok_not, &pos);
      break;
    case ifc_MonadicOperator_PreIncrement:
      cache_token(cache, tok_plus_plus, &pos);
      break;
    case ifc_MonadicOperator_PreDecrement:
      cache_token(cache, tok_minus_minus, &pos);
      break;
    case ifc_MonadicOperator_PostIncrement:
      cache_token(cache, tok_plus_plus, &pos);
      break;
    case ifc_MonadicOperator_PostDecrement:
      cache_token(cache, tok_minus_minus, &pos);
      break;
    case ifc_MonadicOperator_Truncate:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("MonadicOperator::Truncate",
                                  &error_position);
      break;
    case ifc_MonadicOperator_Ceil:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("MonadicOperator::Ceil", &error_position);
      break;
    case ifc_MonadicOperator_Floor:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("MonadicOperator::Floor", &error_position);
      break;
    case ifc_MonadicOperator_Paren:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("MonadicOperator::Paren", &error_position);
      break;
    case ifc_MonadicOperator_Brace:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("MonadicOperator::Brace", &error_position);
      break;
    case ifc_MonadicOperator_Alignas:
      cache_token(cache, tok_alignas, &pos);
      break;
    case ifc_MonadicOperator_Alignof:
      cache_token(cache, tok_alignof, &pos);
      break;
    case ifc_MonadicOperator_Sizeof:
      cache_token(cache, tok_sizeof, &pos);
      break;
    case ifc_MonadicOperator_Cardinality:
      cache_token(cache, tok_sizeof, &pos);
      cache_token(cache, tok_ellipsis, &pos);
      break;
    case ifc_MonadicOperator_Typeid:
      cache_token(cache, tok_typeid, &pos);
      break;
    case ifc_MonadicOperator_Noexcept:
      cache_token(cache, tok_noexcept, &pos);
      break;
    case ifc_MonadicOperator_Requires:
      cache_token(cache, tok_requires, &pos);
      break;
    case ifc_MonadicOperator_CoReturn:
      cache_token(cache, tok_coroutine_return, &pos);
      break;
    case ifc_MonadicOperator_Await:
      cache_token(cache, tok_coroutine_await, &pos);
      break;
    case ifc_MonadicOperator_Yield:
      cache_token(cache, tok_coroutine_yield, &pos);
      break;
    case ifc_MonadicOperator_Throw:
      cache_token(cache, tok_throw, &pos);
      break;
    case ifc_MonadicOperator_New:
      cache_token(cache, tok_new, &pos);
      break;
    case ifc_MonadicOperator_Delete:
      cache_token(cache, tok_delete, &pos);
      break;
    case ifc_MonadicOperator_DeleteArray:
      cache_token(cache, tok_delete, &pos);
      cache_token(cache, tok_lbracket, &pos);
      cache_token(cache, tok_rbracket, &pos);
      break;
    case ifc_MonadicOperator_Expand:
      cache_token(cache, tok_ellipsis, &pos);
      break;
    case ifc_MonadicOperator_Read:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("MonadicOperator::Read",
                                  &error_position);
      break;
    case ifc_MonadicOperator_Materialize:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("MonadicOperator::Materialize",
                                  &error_position);
      break;
    case ifc_MonadicOperator_PseudoDtorCall:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("MonadicOperator::PseudoDtorCall",
                                  &error_position);
      break;
    case ifc_MonadicOperator_Msvc:
      unexpected_condition();
      break;
    case ifc_MonadicOperator_MsvcAssume:
      cache_token(cache, tok_assume, &pos);
      break;
    case ifc_MonadicOperator_MsvcAlignof:
      cache_token(cache, tok_alignof, &pos);
      break;
    case ifc_MonadicOperator_MsvcUuidof:
      cache_token(cache, tok_uuidof, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsClass:
      cache_token(cache, tok_is_class, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsUnion:
      cache_token(cache, tok_is_union, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsEnum:
      cache_token(cache, tok_is_enum, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsPolymorphic:
      cache_token(cache, tok_is_polymorphic, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsEmpty:
      cache_token(cache, tok_is_empty, &pos);
      break;
    case ifc_MonadicOperator_LookupGlobally:
      cache_token(cache, tok_colon_colon, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsTriviallyCopyConstructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                           "MonadicOperator::MsvcIsTriviallyCopyConstructible",
                           &error_position);
      break;
    case ifc_MonadicOperator_MsvcIsTriviallyCopyAssignable:
      cache_token(cache, tok_is_trivially_copy_assignable, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsTriviallyDestructible:
      cache_token(cache, tok_is_trivially_destructible, &pos);
      break;
    case ifc_MonadicOperator_MsvcHasVirtualDestructor:
      cache_token(cache, tok_has_virtual_destructor, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsNothrowCopyConstructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                             "MonadicOperator::MsvcIsNothrowCopyConstructible",
                             &error_position);
      break;
    case ifc_MonadicOperator_MsvcIsNothrowCopyAssignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                                "MonadicOperator::MsvcIsNothrowCopyAssignable",
                                &error_position);
      break;
    case ifc_MonadicOperator_MsvcIsPod:
      cache_token(cache, tok_is_pod, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsAbstract:
      cache_token(cache, tok_is_abstract, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsTrivial:
      cache_token(cache, tok_is_trivial, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsTriviallyCopyable:
      cache_token(cache, tok_is_trivially_copyable, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsStandardLayout:
      cache_token(cache, tok_is_standard_layout, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsLiteralType:
      cache_token(cache, tok_is_literal_type, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsTriviallyMoveConstructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                           "MonadicOperator::MsvcIsTriviallyMoveConstructible",
                           &error_position);
      break;
    case ifc_MonadicOperator_MsvcHasTrivialMoveAssign:
      cache_token(cache, tok_has_trivial_move_assign, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsTriviallyMoveAssignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                              "MonadicOperator::MsvcIsTriviallyMoveAssignable",
                              &error_position);
      break;
    case ifc_MonadicOperator_MsvcIsNothrowMoveAssignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                                "MonadicOperator::MsvcIsNothrowMoveAssignable",
                                &error_position);
      break;
    case ifc_MonadicOperator_MsvcUnderlyingType:
      cache_token(cache, tok_underlying_type, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsDestructible:
      cache_token(cache, tok_is_destructible, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsNothrowDestructible:
      cache_token(cache, tok_is_nothrow_destructible, &pos);
      break;
    case ifc_MonadicOperator_MsvcHasUniqueObjectRepresentations:
      cache_token(cache, tok_has_unique_object_representations, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsAggregate:
      cache_token(cache, tok_is_aggregate, &pos);
      break;
    case ifc_MonadicOperator_MsvcBuiltinAddressOf:
      cache_token(cache, tok_builtin_addressof, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsRefClass:
      cache_token(cache, tok_is_ref_class, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsValueClass:
      cache_token(cache, tok_is_value_class, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsSimpleValueClass:
      cache_token(cache, tok_is_simple_value_class, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsInterfaceClass:
      cache_token(cache, tok_is_interface_class, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsDelegate:
      cache_token(cache, tok_is_delegate, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsFinal:
      cache_token(cache, tok_is_final, &pos);
      break;
    case ifc_MonadicOperator_MsvcIsSealed:
      cache_token(cache, tok_is_sealed, &pos);
      break;
    case ifc_MonadicOperator_MsvcHasFinalizer:
      cache_token(cache, tok_has_finalizer, &pos);
      break;
    case ifc_MonadicOperator_MsvcHasCopy:
      cache_token(cache, tok_has_copy, &pos);
      break;
    case ifc_MonadicOperator_MsvcHasAssign:
      cache_token(cache, tok_has_assign, &pos);
      break;
    case ifc_MonadicOperator_MsvcHasUserDestructor:
      cache_token(cache, tok_has_user_destructor, &pos);
      break;
    case ifc_MonadicOperator_MsvcConfusion:
      /* This is just a placeholder value separating legitimate operators
         from operators that are anticipated to be removed in the future. */
      unexpected_condition();
    case ifc_MonadicOperator_MsvcConfusedExpand:
      cache_token(cache, tok_ellipsis, &pos);
      break;
    case ifc_MonadicOperator_MsvcConfusedDependentSizeof:
      cache_token(cache, tok_sizeof, &pos);
      break;
    default_is_unexpected_str("Unexpected MonadicOperator");
  }  /* switch */
}  /* cache_operator */


void an_ifc_module::cache_operator(a_token_cache_ptr  cache,
                                   ifc_DyadicOperator op,
                                   ifc_SourceLocation *locus)
/*
Add the tokens corresponding to the given Dyadic Operator to cache.  locus is
the location of the operator.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (op) {
    case ifc_DyadicOperator_Unknown:
      unexpected_condition();
      break;
    case ifc_DyadicOperator_Plus:
      cache_token(cache, tok_plus, &pos);
      break;
    case ifc_DyadicOperator_Minus:
      cache_token(cache, tok_minus, &pos);
      break;
    case ifc_DyadicOperator_Mult:
      cache_token(cache, tok_star, &pos);
      break;
    case ifc_DyadicOperator_Slash:
      cache_token(cache, tok_divide, &pos);
      break;
    case ifc_DyadicOperator_Modulo:
      cache_token(cache, tok_remainder, &pos);
      break;
    case ifc_DyadicOperator_Remainder:
      cache_token(cache, tok_remainder, &pos);
      break;
    case ifc_DyadicOperator_Bitand:
      cache_token(cache, tok_ampersand, &pos);
      break;
    case ifc_DyadicOperator_Bitor:
      cache_token(cache, tok_or, &pos);
      break;
    case ifc_DyadicOperator_Bitxor:
      cache_token(cache, tok_excl_or, &pos);
      break;
    case ifc_DyadicOperator_Lshift:
      cache_token(cache, tok_shift_left, &pos);
      break;
    case ifc_DyadicOperator_Rshift:
      cache_token(cache, tok_shift_right, &pos);
      break;
    case ifc_DyadicOperator_Equal:
      cache_token(cache, tok_eq, &pos);
      break;
    case ifc_DyadicOperator_NotEqual:
      cache_token(cache, tok_ne, &pos);
      break;
    case ifc_DyadicOperator_Less:
      cache_token(cache, tok_lt, &pos);
      break;
    case ifc_DyadicOperator_LessEqual:
      cache_token(cache, tok_le, &pos);
      break;
    case ifc_DyadicOperator_Greater:
      cache_token(cache, tok_gt, &pos);
      break;
    case ifc_DyadicOperator_GreaterEqual:
      cache_token(cache, tok_ge, &pos);
      break;
    case ifc_DyadicOperator_Compare:
      cache_token(cache, tok_spaceship, &pos);
      break;
    case ifc_DyadicOperator_LogicAnd:
      cache_token(cache, tok_and_and, &pos);
      break;
    case ifc_DyadicOperator_LogicOr:
      cache_token(cache, tok_or_or, &pos);
      break;
    case ifc_DyadicOperator_Assign:
      cache_token(cache, tok_assign, &pos);
      break;
    case ifc_DyadicOperator_PlusAssign:
      cache_token(cache, tok_plus_assign, &pos);
      break;
    case ifc_DyadicOperator_MinusAssign:
      cache_token(cache, tok_minus_assign, &pos);
      break;
    case ifc_DyadicOperator_MultAssign:
      cache_token(cache, tok_times_assign, &pos);
      break;
    case ifc_DyadicOperator_SlashAssign:
      cache_token(cache, tok_divide_assign, &pos);
      break;
    case ifc_DyadicOperator_ModuloAssign:
      cache_token(cache, tok_remainder_assign, &pos);
      break;
    case ifc_DyadicOperator_BitandAssign:
      cache_token(cache, tok_and_assign, &pos);
      break;
    case ifc_DyadicOperator_BitorAssign:
      cache_token(cache, tok_or_assign, &pos);
      break;
    case ifc_DyadicOperator_BitxorAssign:
      cache_token(cache, tok_excl_or_assign, &pos);
      break;
    case ifc_DyadicOperator_LshiftAssign:
      cache_token(cache, tok_shift_left_assign, &pos);
      break;
    case ifc_DyadicOperator_RshiftAssign:
      cache_token(cache, tok_shift_right_assign, &pos);
      break;
    case ifc_DyadicOperator_Comma:
      cache_token(cache, tok_comma, &pos);
      break;
    case ifc_DyadicOperator_Dot:
      cache_token(cache, tok_period, &pos);
      break;
    case ifc_DyadicOperator_Arrow:
      cache_token(cache, tok_arrow, &pos);
      break;
    case ifc_DyadicOperator_DotStar:
      cache_token(cache, tok_period_star, &pos);
      break;
    case ifc_DyadicOperator_ArrowStar:
      cache_token(cache, tok_arrow_star, &pos);
      break;
    case ifc_DyadicOperator_Curry:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Curry", &error_position);
      break;
    case ifc_DyadicOperator_Apply:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Apply", &error_position);
      break;
    case ifc_DyadicOperator_Index:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Index", &error_position);
      break;
    case ifc_DyadicOperator_DefaultAt:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::DefaultAt",
                                  &error_position);
      break;
    case ifc_DyadicOperator_New:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::New", &error_position);
      break;
    case ifc_DyadicOperator_NewArray:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::NewArray", &error_position);
      break;
    case ifc_DyadicOperator_Destruct:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Destruct", &error_position);
      break;
    case ifc_DyadicOperator_DestructAt:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::DestructAt",
                                  &error_position);
      break;
    case ifc_DyadicOperator_Cleanup:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Cleanup", &error_position);
      break;
    case ifc_DyadicOperator_Qualification:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Qualification",
                                  &error_position);
      break;
    case ifc_DyadicOperator_Promote:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Promote", &error_position);
      break;
    case ifc_DyadicOperator_Demote:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Demote", &error_position);
      break;
    case ifc_DyadicOperator_Coerce:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Coerce", &error_position);
      break;
    case ifc_DyadicOperator_Rewrite:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Rewrite", &error_position);
      break;
    case ifc_DyadicOperator_Bless:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Bless", &error_position);
      break;
    case ifc_DyadicOperator_Cast:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Cast", &error_position);
      break;
    case ifc_DyadicOperator_ExplicitConversion:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::ExplicitConversion",
                                  &error_position);
      break;
    case ifc_DyadicOperator_ReinterpretCast:
      cache_token(cache, tok_reinterpret_cast, &pos);
      break;
    case ifc_DyadicOperator_StaticCast:
      cache_token(cache, tok_static_cast, &pos);
      break;
    case ifc_DyadicOperator_ConstCast:
      cache_token(cache, tok_const_cast, &pos);
      break;
    case ifc_DyadicOperator_DynamicCast:
      cache_token(cache, tok_dynamic_cast, &pos);
      break;
    case ifc_DyadicOperator_Narrow:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Narrow", &error_position);
      break;
    case ifc_DyadicOperator_Widen:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Widen", &error_position);
      break;
    case ifc_DyadicOperator_Pretend:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Pretend", &error_position);
      break;
    case ifc_DyadicOperator_Closure:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::Closure", &error_position);
      break;
    case ifc_DyadicOperator_ZeroInitialize:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::ZeroInitialize",
                                  &error_position);
      break;
    case ifc_DyadicOperator_ClearStorage:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::ClearStorage",
                                  &error_position);
      break;
    case ifc_DyadicOperator_Msvc:
      unexpected_condition();
      break;
    case ifc_DyadicOperator_MsvcTryCast:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::MsvcTryCast",
                                  &error_position);
      break;
    case ifc_DyadicOperator_MsvcCurry:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::MsvcCurry",
                                  &error_position);
      break;
    case ifc_DyadicOperator_MsvcVirtualCurry:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::MsvcVirtualCurry",
                                  &error_position);
      break;
    case ifc_DyadicOperator_MsvcAlign:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::MsvcAlign",
                                  &error_position);
      break;
    case ifc_DyadicOperator_MsvcBitSpan:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::MsvcBitSpan",
                                  &error_position);
      break;
    case ifc_DyadicOperator_MsvcBitfieldAccess:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::MsvcBitfieldAccess",
                                  &error_position);
      break;
    case ifc_DyadicOperator_MsvcObscureBitfieldAccess:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::MsvcObscureBitfieldAccess",
                                  &error_position);
      break;
    case ifc_DyadicOperator_MsvcInitialize:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::MsvcInitialize",
                                  &error_position);
      break;
    case ifc_DyadicOperator_MsvcBuiltinOffsetOf:
      cache_token(cache, tok_builtin_offsetof, &pos);
      break;
    case ifc_DyadicOperator_MsvcIsBaseOf:
      cache_token(cache, tok_is_base_of, &pos);
      break;
    case ifc_DyadicOperator_MsvcIsConvertibleTo:
      cache_token(cache, tok_is_convertible_to, &pos);
      break;
    case ifc_DyadicOperator_MsvcIsTriviallyAssignable:
      cache_token(cache, tok_is_trivially_assignable, &pos);
      break;
    case ifc_DyadicOperator_MsvcIsNothrowAssignable:
      cache_token(cache, tok_is_nothrow_assignable, &pos);
      break;
    case ifc_DyadicOperator_MsvcIsAssignable:
      cache_token(cache, tok_is_assignable, &pos);
      break;
    case ifc_DyadicOperator_MsvcIsAssignableNocheck:
      cache_token(cache, tok_is_assignable_no_precondition_check, &pos);
      break;
    case ifc_DyadicOperator_MsvcBuiltinBitCast:
      cache_token(cache, tok_builtin_bit_cast, &pos);
      break;
    case ifc_DyadicOperator_MsvcBuiltinIsLayoutCompatible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                               "DyadicOperator::MsvcBuiltinIsLayoutCompatible",
                               &error_position);
      break;
    case ifc_DyadicOperator_MsvcBuiltinIsPointerInterconvertibleBaseOf:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                  "DyadicOperator::MsvcBuiltinIsPointerInterconvertibleBaseOf",
                  &error_position);
      break;
    case ifc_DyadicOperator_MsvcBuiltinIsPointerInterconvertibleWithClass:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
               "DyadicOperator::MsvcBuiltinIsPointerInterconvertibleWithClass",
               &error_position);
      break;
    case ifc_DyadicOperator_MsvcBuiltinIsCorrespondingMember:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                            "DyadicOperator::MsvcBuiltinIsCorrespondingMember",
                            &error_position);
      break;
    case ifc_DyadicOperator_MsvcIntrinsic:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DyadicOperator::MsvcIntrinsic",
                                  &error_position);
      break;
    default_is_unexpected_str("Unexpected DyadicOperator");
  }  /* switch */
}  /* cache_operator */


void an_ifc_module::cache_operator(a_token_cache_ptr   cache,
                                   ifc_TriadicOperator op,
                                   ifc_SourceLocation  *locus)
/*
Add the tokens corresponding to the given Triadic Operator to cache.  locus is
the location of the operator.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (op) {
    case ifc_TriadicOperator_Unknown:
      unexpected_condition();
      break;
    case ifc_TriadicOperator_Choice:
      cache_token(cache, tok_quest_mark, &pos);
      break;
    case ifc_TriadicOperator_ConstructAt:
      cache_token(cache, tok_new, &pos);
      break;
    case ifc_TriadicOperator_Initialize:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("TriadicOperator::Initialize",
                                  &error_position);
      break;
    case ifc_TriadicOperator_Msvc:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected TriadicOperator");
  }  /* switch */
}  /* cache_operator */


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
/* Remove ARG_UNUSED as well. */
void an_ifc_module::cache_operator(ARG_UNUSED a_token_cache_ptr cache,
                                   ifc_StorageOperator          op,
                                   ifc_SourceLocation           *locus)
/*
Add the tokens corresponding to the given Storage Operator to cache.  locus is
the location of the operator.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (op) {
    case ifc_StorageOperator_Unknown:
      unexpected_condition();
      break;
    case ifc_StorageOperator_AllocateSingle:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("StorageOperator::AllocateSingle",
                                  &error_position);
      break;
    case ifc_StorageOperator_AllocateArray:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("StorageOperator::AllocateArray",
                                  &error_position);
      break;
    case ifc_StorageOperator_DeallocateSingle:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("StorageOperator::DeallocateSingle",
                                  &error_position);
      break;
    case ifc_StorageOperator_DeallocateArray:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("StorageOperator::DeallocateArray",
                                  &error_position);
      break;
    case ifc_StorageOperator_Msvc:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected StorageOperator");
  }  /* switch */
}  /* cache_operator */


void an_ifc_module::cache_operator(a_token_cache_ptr    cache,
                                   ifc_VariadicOperator op,
                                   ifc_SourceLocation   *locus)
/*
Add the tokens corresponding to the given Variadic Operator to cache.  locus is
the location of the operator.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  switch (op) {
    case ifc_VariadicOperator_Unknown:
      unexpected_condition();
      break;
    case ifc_VariadicOperator_Collection:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("VariadicOperator::Collection",
                                  &error_position);
      break;
    case ifc_VariadicOperator_Sequence:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("VariadicOperator::Sequence",
                                  &error_position);
      break;
    case ifc_VariadicOperator_Msvc:
      unexpected_condition();
      break;
    case ifc_VariadicOperator_MsvcHasTrivialConstructor:
      cache_token(cache, tok_has_trivial_constructor, &pos);
      break;
    case ifc_VariadicOperator_MsvcIsConstructible:
      cache_token(cache, tok_is_constructible, &pos);
      break;
    case ifc_VariadicOperator_MsvcIsNothrowConstructible:
      cache_token(cache, tok_is_nothrow_constructible, &pos);
      break;
    case ifc_VariadicOperator_MsvcIsTriviallyConstructible:
      cache_token(cache, tok_is_trivially_constructible, &pos);
      break;
    default_is_unexpected_str("Unexpected VariadicOperator");
  }  /* switch */
}  /* cache_operator */


template<typename a_Name_Cache_Fn, typename an_Init_Cache_Fn>
inline void an_ifc_module::cache_variable_decl(
                                         a_token_cache_ptr   cache,
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
                                         ifc_SourceLocation  *locus)
/*
Add the tokens corresponding to the given variable declaration (indexed in the
IFC by decl_idx) to cache.  is_class_member is TRUE if this is a non-static
data member of a class.  access, specifiers, traits, alignment, and type are
values from the IFC file that describe the variable declaration.
cache_access_spec is TRUE if the access specifier caching was not handled by
the caller.  cache_name_fn is a lambda accepting a_source_position_ptr
interpretation of locus that's called to cache the name of the variable.  If
width is not zero, this is a bitfield and width is its size.  cache_init_fn is
a lambda accepting a_source_position_ptr interpretation of locus that's called
to cache the variable initializer (if any).  locus is the source location for
the declaration.

FIXME: Remove this version of cache_variable_decl once names can be cached
properly for specializations using a NameIndex, and similarly the variable's
initializer can be consistently cached via a ScopeIndex.
*/
{
  a_source_position pos;
  a_boolean         decl_in_class;

  source_position_from_locus(&pos, locus);
  decl_in_class = is_class_member || access != ifc_Access_None;
  if (decl_in_class && cache_access_spec) {
    cache_access(cache, access, /*cache_colon=*/TRUE, &pos);
  }  /* if */
  /* Cache tokens for MSVC "basic specifiers" (at the time of writing this
     includes extern "C" and [[deprecated]]). */
  /* FIXME: Because we cache attributes properly now, this can result in two
     deprecated attributes. */
  cache_basic_specifiers(cache, specifiers, &pos);
  /* Cache any associated attributes. */
  cache_attrs(cache, decl_idx);
  if (!is_class_member && decl_in_class) {
    /* This is a static data member. */
    cache_token(cache, tok_static, &pos);
  }  /* if */
  /* Cache the alignment if specified. */
  if (alignment != 0) {
    cache_token(cache, tok_alignas, &pos);
    cache_token(cache, tok_lparen, &pos);
    cache_expr(cache, alignment);
    cache_token(cache, tok_rparen, &pos);
  }  /* if */
  /* Cache the "object traits", roughly an MSVC subset of the
     decl-specifier-seq. */
  cache_object_traits(cache, traits, &pos);
  /* Cache the name surrounded by the respective type qualifiers. */
  cache_type_first_pass(cache, type, locus);
  cache_name_fn(&pos);
  cache_type_second_pass(cache, type, locus);
  /* Cache the variable with if any. */
  if (width != 0) {
    cache_token(cache, tok_colon, &pos);
    cache_expr(cache, width);
  }  /* if */
  /* Cache the initializer (if any). */
  cache_init_fn(&pos);
}  /* cache_variable_decl */


void an_ifc_module::cache_variable_decl(a_token_cache_ptr   cache,
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
                                        ifc_SourceLocation  *locus)
/*
Add the tokens corresponding to the given variable declaration (indexed in the
IFC by decl_idx) to cache.  is_class_member is TRUE if this is a non-static
data member of a class.  access, specifiers, traits, alignment, and type are
values from the IFC file that describe the variable declaration.  Both name and
raw_name provide the name of the variable - if name is zero, raw_name must be
non-zero.  If width is not zero, this is a bitfield and width is its size.  If
the variable has an initializer then initializer is non-zero and refers to the
initializer expression.  locus is the source location for the declaration.
*/
{
  auto cache_name_fn = [this, cache, name, raw_name, locus](
                                                   a_source_position_ptr pos) {
    if (name != 0) {
      cache_name(cache, name, locus);
    } else {
      check_assertion(raw_name != 0);
      cache_identifier(cache, get_string_at_offset(raw_name), pos);
    }  /* if */
  };
  auto cache_init_fn = [this, cache, initializer](a_source_position_ptr pos) {
    if (initializer != 0) {
      /* An initializer where the type is ExprSort::Tokens will have the braces
         included as part of the token stream. */
      a_boolean cache_braces = expr_tag(initializer) != ifc_ExprSort_Tokens;
      if (cache_braces) {
        cache_token(cache, tok_lbrace, pos);
      }  /* if */
      cache_expr(cache, initializer);
      if (cache_braces) {
        cache_token(cache, tok_rbrace, pos);
      }  /* if */
    }  /* if */
    if (cache->last_token->token != tok_semicolon) {
      /* Add a terminating semicolon, unless one was already added (which can
         happen when the initializer cached by cache_expr above is of kind
         ifc_ExprSort_Tokens). */
      cache_token(cache, tok_semicolon, pos);
    }  /* if */
  };
  cache_variable_decl(cache, decl_idx, is_class_member, access,
                      /*cache_access_spec=*/TRUE, specifiers, traits,
                      alignment, type, cache_name_fn, width,
                      cache_init_fn, locus);
}  /* cache_variable_decl */


template<typename a_Name_Cache_Fn>
inline void an_ifc_module::cache_function_decl(
                                   a_token_cache_ptr         cache,
                                   a_boolean                 is_class_member,
                                   a_boolean                 is_dtor,
                                   ifc_Access                access,
                                   a_boolean                 cache_access_spec,
                                   ifc_CallingConvention     calling_conv,
                                   ifc_FunctionTraits        func_traits,
                                   ifc_FunctionTypeTraits    func_type_traits,
                                   ifc_MsvcTraits            vendor_traits,
                                   ifc_TypeIndex             return_type,
                                   a_Name_Cache_Fn           cache_name_fn,
                                   ifc_ChartIndex            params,
                                   ifc_TypeIndex             param_types,
                                   ifc_NoexceptSpecification *eh_spec,
                                   ifc_SourceLocation        *locus)
/*
Add the tokens corresponding to the given function declaration to cache.
is_class_member is TRUE if this is a non-static member of a class.  is_dtor is
TRUE if this is a destructor declaration.  access, calling_conv, func_traits,
func_type_traits, vendor_traits, and eh_spec are values from the IFC file that
describe the function.  cache_access_spec is TRUE if the access specifier
caching was not handled by the caller.  return_type is the return type of the
function (0 if there is no return type, e.g., the function is a constructor or
destructor).  cache_name_fn is a lambda accepting a_source_position_ptr
interpretation of locus that's called to cache the name of the scope.  Both
params and param_types are the parameter list (0 for both if there are no
parameters).  If params is non-zero, param_types will be ignored as params will
already contain the parameter types.  locus is the position of the function
declaration.

FIXME: Remove this version of cache_function_decl once names can be cached
properly for specializations using a NameIndex.
*/
{
  a_source_position pos;
  a_boolean         decl_in_class;

  decl_in_class = is_class_member || access != ifc_Access_None;
  source_position_from_locus(&pos, locus);
  if (decl_in_class && cache_access_spec) {
    cache_access(cache, access, /*cache_colon=*/TRUE, &pos);
  }  /* if */
  if (!is_class_member && decl_in_class) {
    /* This is a static member function. */
    cache_token(cache, tok_static, &pos);
  }  /* if */
  cache_func_traits(cache, func_traits, vendor_traits, /*trailing=*/FALSE,
                    &pos);
  if (return_type != 0) {
    cache_type(cache, return_type, locus);
  }  /* if */
  cache_calling_convention(cache, calling_conv, &pos);
  if (is_dtor) {
    cache_token(cache, tok_compl, &pos);
  }  /* if */
  cache_name_fn(&pos);
  cache_token(cache, tok_lparen, &pos);
  if (params != 0) {
    /* The parameters have detailed information associated with them, use
       that. */
    cache_chart(cache, params, locus);
  } else if (param_types != 0) {
    /* The only information we have on the parameters are their types. */
    cache_type(cache, param_types, locus);
  }  /* if */
  cache_token(cache, tok_rparen, &pos);
  cache_func_type_traits(cache, func_type_traits, &pos);
  cache_exception_spec(cache, eh_spec, &pos);
  cache_func_traits(cache, func_traits, vendor_traits, /*trailing=*/TRUE,
                    &pos);
  cache_token(cache, tok_semicolon, &pos);
}  /* cache_function_decl */


void an_ifc_module::cache_function_decl(
                                    a_token_cache_ptr         cache,
                                    a_boolean                 is_class_member,
                                    a_boolean                 is_dtor,
                                    ifc_Access                access,
                                    ifc_CallingConvention     calling_conv,
                                    ifc_FunctionTraits        func_traits,
                                    ifc_FunctionTypeTraits    func_type_traits,
                                    ifc_MsvcTraits            vendor_traits,
                                    ifc_TypeIndex             return_type,
                                    ifc_NameIndex             name,
                                    ifc_ChartIndex            params,
                                    ifc_TypeIndex             param_types,
                                    ifc_NoexceptSpecification *eh_spec,
                                    ifc_SourceLocation        *locus)
/*
Add the tokens corresponding to the given function declaration to cache.
is_class_member is TRUE if this is a non-static member of a class.  is_dtor is
TRUE if this is a destructor declaration.  access, calling_conv, func_traits,
func_type_traits, vendor_traits, eh_spec, and name are values from the IFC file
that describe the function.  return_type is the return type of the function (0
if there is no return type, e.g., the function is a constructor or destructor).
Both params and param_types are the parameter list (0 for both if there are no
parameters).  If params is non-zero, param_types will be ignored as params will
already contain the parameter types.  locus is the position of the function
declaration.
*/
{
  auto cache_name_fn = [this, cache, name, locus](a_source_position_ptr pos) {
    cache_name(cache, name, locus);
  };
  cache_function_decl(cache, is_class_member, is_dtor, access,
                      /*cache_access_spec=*/TRUE, calling_conv, func_traits,
                      func_type_traits, vendor_traits, return_type,
                      cache_name_fn, params, param_types, eh_spec, locus);
}  /* cache_function_decl */


void an_ifc_module::cache_function_decl(
                                    a_token_cache_ptr         cache,
                                    a_boolean                 is_class_member,
                                    a_boolean                 is_dtor,
                                    ifc_Access                access,
                                    ifc_CallingConvention     calling_conv,
                                    ifc_FunctionTraits        func_traits,
                                    ifc_FunctionTypeTraits    func_type_traits,
                                    ifc_MsvcTraits            vendor_traits,
                                    ifc_TypeIndex             return_type,
                                    an_ifc_Ref<ifc_NameIndex> name,
                                    ifc_ChartIndex            params,
                                    ifc_TypeIndex             param_types,
                                    ifc_NoexceptSpecification *eh_spec,
                                    ifc_SourceLocation        *locus)
/*
Add the tokens corresponding to the given function declaration to cache.
is_class_member is TRUE if this is a non-static member of a class.  is_dtor is
TRUE if this is a destructor declaration.  access, calling_conv, func_traits,
func_type_traits, vendor_traits, eh_spec, and name are values from the IFC file
that describe the function.  return_type is the return type of the function (0
if there is no return type, e.g., the function is a constructor or destructor).
Both params and param_types are the parameter list (0 for both if there are no
parameters).  If params is non-zero, param_types will be ignored as params will
already contain the parameter types.  locus is the position of the function
declaration.
*/
{
  auto cache_name_fn = [cache, name, locus](a_source_position_ptr pos) {
    EDG_PREFIX::cache_name(cache, name, locus);
  };
  cache_function_decl(cache, is_class_member, is_dtor, access,
                      /*cache_access_spec=*/TRUE, calling_conv, func_traits,
                      func_type_traits, vendor_traits, return_type,
                      cache_name_fn, params, param_types, eh_spec, locus);
}  /* cache_function_decl */


void an_ifc_module::cache_class_definition(a_token_cache_ptr     cache,
                                           an_ifc_DeclSort_Scope *decl)
/*
Add the tokens corresponding to the given class definition (decl) to cache.
The cached tokens are suitable for parsing with scan_class_definition (i.e.,
the first token is a colon introducing base classes or a left brace introducing
the member declarations).
FIXME: There's a relationship with cache_scope_decl here, but it's not entirely
clear what that is yet.
*/
{
  a_source_position       pos;
  Value_saver<a_boolean>  saved(&caching_ifc_class_scope);

  caching_ifc_class_scope = TRUE;
  source_position_from_locus(&pos, &decl->locus);
  if (decl->base != 0) {
    cache_token(cache, tok_colon, &pos);
    cache_type(cache, decl->base, &decl->locus);
  }  /* if */
  cache_scope(cache, decl->initializer, &decl->locus);
  cache_token(cache, tok_semicolon, &pos);
}  /* cache_class_definition */


uint32_t an_ifc_module::try_cache_class_attributes_from_body(
                                               a_token_cache_ptr cache,
                                               ifc_SentenceIndex body_sentence)
/*
MSVC puts attributes for class templates as part of the body_sentence.  This
function caches those attributes independently (so that they can be placed
prior to the identifier).  The return value is the new offset into the body
sentence where the brace-wrapped member-specification can be found (for use by
later processing).
*/
{
  uint32_t offset = 0;

  if (body_sentence != 0) {
    push_stop_token_stack();
    add_stop_token(tok_lbrace);
    add_stop_token(tok_colon);
    offset = cache_sentence(cache, body_sentence, /*offset=*/0,
                            /*look_for_stop_token=*/TRUE);
    clear_stop_tokens();
    pop_stop_token_stack();
  }  /* if */
  return offset;
}  /* try_cache_class_attributes_from_body */


uint32_t an_ifc_module::cache_decl_template_declaration(
                                        a_token_cache_ptr        cache,
                                        an_ifc_DeclSort_Template *decl,
                                        a_boolean                add_semicolon)
/*
Add the tokens corresponding to the given template declaration (decl) to cache.
If add_semicolon is TRUE, include the terminating semicolon that would be
expected for a declaration that isn't a definition.  Return the offset into the
template declaration's body at which to find the definition, or zero
if there is no offset/the offset is not needed.
*/
{
  a_type_ptr        type;
  a_non_type_kind   kind;
  a_source_position pos;
  uint32_t          offset = 0;

  source_position_from_locus(&pos, &decl->locus);
  /* Attempt to cache the access specifier if one is specified. */
  /* FIXME: As of IFC 0.32, decl is not always present. */
  if (decl->entity.decl != 0) {
    Opt<ifc_Access> opt_access = get_ifc_access(decl->entity.decl);

    if (opt_access.has_value()) {
      cache_access(cache, *opt_access, /*cache_colon=*/TRUE, &pos);
    }  /* if */
  }  /* if */
  /* Reconstruct the template-head. */
  cache_template_head(cache, decl->chart, &pos);
  /* FIXME: Handle attributes. */
  type = decl->type == 0 ? (a_type_ptr)NULL
                         : type_for_type_index(decl->type, &kind);
  if (type != NULL && type_is(type, tk_unknown)) {
    /* As of IFC 0.31, this should no longer be encountered (alias templates
       are now handled by DeclSort::Alias). */
    unexpected_condition_str("Unexpected alias template");
  } else if (type != NULL && is_class_struct_union_type(type)) {
    cache_type(cache, decl->type, &decl->locus);
    offset = try_cache_class_attributes_from_body(cache, decl->entity.body);
    cache_name(cache, decl->name, &decl->locus);
  } else {
    /* Function or variable template. */
    /* FIXME: Cache the entity corresponding to decl->entity.decl instead.
       Currently this may be missing information, so use the soon-to-be-removed
       entity.head instead. */
    cache_sentence(cache, decl->entity.head);
  }  /* if */
  if (add_semicolon) {
    cache_token(cache, tok_semicolon, &pos);
  }  /* if */
  return offset;
}  /* cache_decl_template_declaration */


void an_ifc_module::cache_decl_template(a_token_cache_ptr        cache,
                                        an_ifc_DeclSort_Template *decl)
/*
Add the tokens corresponding to the given template declaration (decl) to cache.
*/
{
  a_boolean         decl_only = decl->entity.body == 0;
  uint32_t          offset;

  offset = cache_decl_template_declaration(cache, decl,
                                           /*add_semicolon=*/decl_only);
  if (!decl_only) {
    (void)cache_sentence(cache, decl->entity.body, offset);
  }  /* if */
}  /* cache_decl_template */


void an_ifc_module::cache_simple_template_id(a_token_cache_ptr  cache,
                                             ifc_FormSpecIndex  form_idx,
                                             ifc_SourceLocation *locus)
/*
Add the tokens for a simple-template-id via the associated form spec (form_idx)
to the cache.  locus is the location of the simple-template-id.
*/
{
  an_ifc_Form_Spec ifs, *ifsp;

  /* Load the specialization form to figure out what the primary
     template's declaration is. */
  read_prechecked_partition_element(form_idx);
  ifsp = get_Form_Spec(&ifs);
  /* Reconstruct the template-name. */
  {
    Opt<an_ifc_Ref<ifc_NameIndex>> opt_name_ref;

    opt_name_ref = get_ifc_name_from_primary_template(this, form_idx);
    /* FIXME: This should be a soft failure. */
    check_assertion(opt_name_ref.has_value());
    EDG_PREFIX::cache_name(cache, *opt_name_ref, locus);
  }
  /* Reconstruct the template-argument-list and enclosing angle
     brackets. */
  {
    a_source_position pos;
    source_position_from_locus(&pos, locus);
    cache_token(cache, tok_lt, &pos);
    cache_expr(cache, ifsp->arguments);
    cache_token(cache, tok_gt, &pos);
  }
}  /* cache_simple_template_id */


void an_ifc_module::cache_decl_partial_specialization(
                                a_token_cache_ptr                     cache,
                                ifc_DeclIndex                         decl_idx,
                                an_ifc_DeclSort_PartialSpecialization *decl)
/*
Add the tokens corresponding to the given partial specialization declaration
(decl indexed in the IFC by decl_idx) to cache.
*/
{
  ifc_DeclIndex     templated_decl_idx = decl->entity.decl;
  a_source_position pos;

  source_position_from_locus(&pos, &decl->locus);
  {
    /* Attempt to cache the access specifier if one is specified. */
    Opt<ifc_Access> opt_access = get_ifc_access(templated_decl_idx);

    if (opt_access.has_value()) {
      cache_access(cache, *opt_access, /*cache_colon=*/TRUE, &pos);
    }  /* if */
  }
  /* Reconstruct the template-head. */
  cache_template_head(cache, decl->chart, &pos);
  {
    /* Reconstruct the declaration. */
    /* FIXME: Eventually this entire block should be replaceable by a
       cache_decl call (due to problems in the IFC -- namely the templated decl
       having a mangled NameSort Identifier name instead of a NameSort
       Specialization -- this is not yet possible). */

    /* Read the partition for the templated declaration. */
    read_prechecked_partition_element(templated_decl_idx);
    switch (decl_tag(templated_decl_idx)) {
      case ifc_DeclSort_Scope:
        { /* We're reconstructing a class. */
          an_ifc_DeclSort_Scope idss, *idssp;

          idssp = get_DeclSort_Scope(&idss);
#if CHECKING
          validate_is_class_type(idssp->type);
#endif /* CHECKING */
          { /* Reconstruct the templated declaration. */
            auto cache_name_fn = [this, cache, decl, idssp](
                                              a_source_position_ptr decl_pos) {
              cache_simple_template_id(cache, decl->form, &idssp->locus);
            };
            auto cache_scope_fn = [this, cache, decl](
                                              a_source_position_ptr decl_pos) {
              if (decl->entity.body != 0) {
                /* We have a body for this declaration, cache it. */
                (void)cache_sentence(cache, decl->entity.body);
              }  /* if */
            };
            cache_scope_decl(cache, decl_idx, idssp->type, cache_name_fn,
                             cache_scope_fn, &idssp->locus);
          }
        }
        break;
      case ifc_DeclSort_Variable:
        { /* We're reconstructing a variable. */
          an_ifc_DeclSort_Variable idsv, *idsvp;

          idsvp = get_DeclSort_Variable(&idsv);
          { /* Reconstruct the templated declaration. */
            auto cache_name_fn = [this, cache, decl, idsvp](
                                              a_source_position_ptr decl_pos) {
              cache_simple_template_id(cache, decl->form, &idsvp->locus);
            };
            auto cache_init_fn = [this, cache, decl](
                                              a_source_position_ptr decl_pos) {
              if (decl->entity.body != 0) {
                /* We have a body for this declaration, cache it. */
                (void)cache_sentence(cache, decl->entity.body);
              }  /* if */
            };

            /* We've already cached the access specifier above, suppress
               cache_variable_decl's access specifier caching. */
            cache_variable_decl(cache, decl_idx, /*is_class_member=*/FALSE,
                                idsvp->access, /*cache_access_spec=*/FALSE,
                                idsvp->specifiers, idsvp->traits,
                                idsvp->alignment, idsvp->type, cache_name_fn,
                                (ifc_ExprIndex)0, cache_init_fn,
                                &idsvp->locus);
          }
        }
        break;
      default:
        unexpected_condition_str("Unexpected DeclSort");
    }  /* switch */
  }
}  /* cache_decl_partial_specialization */

#if CHECKING

template<typename an_ifc_DeclSort_T>
static void assert_in_class_scope(an_ifc_module     *mod,
                                  an_ifc_DeclSort_T *decl)
/*
Given a declaration and its associated module, assert that the declaration
appears within a class scope.
*/
{
  Opt<an_ifc_Ref<ifc_DeclIndex>> opt_home_scope;

  opt_home_scope = mod->get_ifc_home_scope_decl(decl);
  /* FIXME: Should these be diagnostics? */
  check_assertion(opt_home_scope.has_value());
  check_assertion(is_class_scope(*opt_home_scope));
}  /* assert_in_class_scope */

#endif /* CHECKING */

void an_ifc_module::cache_decl_specialization(
                                       a_token_cache_ptr              cache,
                                       ifc_DeclIndex                  decl_idx,
                                       an_ifc_DeclSort_Specialization *decl)
/*
Add the tokens corresponding to the given specialization declaration (decl
indexed in the IFC by decl_idx) to cache.
*/
{
  ifc_DeclIndex      templated_decl_idx = decl->decl;
  ifc_SourceLocation decl_locus = get_ifc_locus(templated_decl_idx);
  a_source_position  pos;
  a_boolean          is_instantiation;

  source_position_from_locus(&pos, &decl_locus);
  if (decl->sort == ifc_SpecializationSort_Instantiation) {
    if (caching_ifc_class_scope) {
      /* FIXME Explicit instantiations of member templates are
         recorded as part of the enclosing class definition, but
         explicit instantiations cannot appear in class scope.
         For now, just skip those.  Eventually, we should either
         delay them until we're in namespace scope, or accept such
         constructs in code generated from modules. */
      cache_token(cache, tok_semicolon, &pos);
      goto done;
    }  /* if */
    is_instantiation = TRUE;
  } else {
    is_instantiation = FALSE;
  }  /* if */
  {
    /* Attempt to cache the access specifier if one is specified. */
    Opt<ifc_Access> opt_access = get_ifc_access(templated_decl_idx);

    if (opt_access.has_value()) {
      cache_access(cache, *opt_access, /*cache_colon=*/TRUE, &pos);
    }  /* if */
  }
  if (is_instantiation) {
    /* If an explicit instantiation appeared in a module definition, that
       instantiation need not be done in client code (other than for inlining
       or constant-evaluation purposes). */
    cache_token(cache, tok_template, &pos);
  } else {
    /* Reconstruct the template-head. */
    cache_template_head(cache, (ifc_ChartIndex)0, &pos);
  }  /* if */
  {
    /* Reconstruct the declaration. */
    /* FIXME: Eventually this entire block should be replaceable by a
       cache_decl call (due to problems in the IFC -- namely the templated decl
       having a mangled NameSort Identifier name instead of a NameSort
       Specialization -- this is not yet possible). */

    /* Read the partition for the templated declaration. */
    read_prechecked_partition_element(templated_decl_idx);
    switch (decl_tag(templated_decl_idx)) {
      case ifc_DeclSort_Scope:
        { /* We're reconstructing a class. */
          an_ifc_DeclSort_Scope idss, *idssp;

          idssp = get_DeclSort_Scope(&idss);
#if CHECKING
          validate_is_class_type(idssp->type);
#endif /* CHECKING */
          { /* Reconstruct the templated declaration. */
            auto cache_name_fn = [this, cache, decl, idssp](
                                              a_source_position_ptr decl_pos) {
              cache_simple_template_id(cache, decl->form, &idssp->locus);
            };
            auto cache_spec_scope_fn = [this, cache, idssp](
                                              a_source_position_ptr decl_pos) {
              /* If there are bases specified, cache the bases. */
              if (idssp->base != 0) {
                cache_token(cache, tok_colon, decl_pos);
                cache_type(cache, idssp->base, &idssp->locus);
              }  /* if */
              cache_scope(cache, idssp->initializer, &idssp->locus);
              cache_token(cache, tok_semicolon, decl_pos);
            };
            auto cache_inst_scope_fn = [cache](a_source_position_ptr decl_pos){
              cache_token(cache, tok_semicolon, decl_pos);
            };
            if (is_instantiation) {
              cache_scope_decl(cache, decl_idx, idssp->type, cache_name_fn,
                               cache_inst_scope_fn, &idssp->locus);
            } else {
              cache_scope_decl(cache, decl_idx, idssp->type, cache_name_fn,
                               cache_spec_scope_fn, &idssp->locus);
            }  /* if */
          }
        }
        break;
      case ifc_DeclSort_Variable:
        { /* We're reconstructing a variable. */
          an_ifc_DeclSort_Variable idsv, *idsvp;

          idsvp = get_DeclSort_Variable(&idsv);
          { /* Reconstruct the templated declaration. */
            auto cache_name_fn = [this, cache, decl, idsvp](
                                              a_source_position_ptr decl_pos) {
              cache_simple_template_id(cache, decl->form, &idsvp->locus);
            };
            auto cache_spec_init_fn = [this, cache, idsvp](
                                              a_source_position_ptr decl_pos) {
              if (idsvp->initializer != 0) {
                cache_token(cache, tok_lparen, decl_pos);
                cache_expr(cache, idsvp->initializer);
                cache_token(cache, tok_rparen, decl_pos);
              }  /* if */
              cache_token(cache, tok_semicolon, decl_pos);
            };
            auto cache_inst_init_fn = [cache](a_source_position_ptr decl_pos) {
              cache_token(cache, tok_semicolon, decl_pos);
            };

            /* We've already cached the access specifier above, suppress
               cache_variable_decl's access specifier caching. */
            {
              Opt<an_ifc_Ref<ifc_DeclIndex>> opt_scope_ref;

              opt_scope_ref = get_ifc_home_scope_decl(idsvp);
              /* FIXME: This should be a soft failure. */
              check_assertion(opt_scope_ref.has_value());
              /* Disable spurious GCC warning about uninitialized usage of
                 opt_scope_ref). */
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
              if (is_instantiation) {
                cache_variable_decl(cache, decl_idx,
                                    is_class_scope(*opt_scope_ref),
                                    idsvp->access, /*cache_access_spec=*/FALSE,
                                    idsvp->specifiers, idsvp->traits,
                                    idsvp->alignment, idsvp->type,
                                    cache_name_fn, (ifc_ExprIndex)0,
                                    cache_inst_init_fn, &idsvp->locus);
              } else {
                cache_variable_decl(cache, decl_idx,
                                    is_class_scope(*opt_scope_ref),
                                    idsvp->access, /*cache_access_spec=*/FALSE,
                                    idsvp->specifiers, idsvp->traits,
                                    idsvp->alignment, idsvp->type,
                                    cache_name_fn, (ifc_ExprIndex)0,
                                    cache_spec_init_fn, &idsvp->locus);
              }  /* if */
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
            }
          }
        }
        break;
      case ifc_DeclSort_Function:
        { /* We're reconstructing a function. */
          an_ifc_DeclSort_Function idsf, *idsfp;
          ifc_MsvcTraits           vendor_traits;

          idsfp = get_DeclSort_Function(&idsf);
          { /* Reconstruct the templated declaration. */
            auto cache_name_fn = [this, cache, decl, idsfp](
                                              a_source_position_ptr decl_pos) {
              cache_simple_template_id(cache, decl->form, &idsfp->locus);
            };
            an_ifc_TypeSort_Function itsf, *itsfp;
            ifc_ChartIndex           params = (ifc_ChartIndex)0;

            check_assertion(type_tag(idsfp->type) == ifc_TypeSort_Function);
            read_prechecked_partition_element(idsfp->type);
            itsfp = get_TypeSort_Function(&itsf);
            if (itsfp->source != 0) {
              params = get_func_params_from_trait(decl_idx);
            }  /* if */
            vendor_traits = get_vendor_traits(decl_idx);
            {
              Opt<an_ifc_Ref<ifc_DeclIndex>> opt_scope_ref;

              opt_scope_ref = get_ifc_home_scope_decl(idsfp);
              /* FIXME: This should be a soft failure. */
              check_assertion(opt_scope_ref.has_value());
              /* Disable spurious GCC warning about uninitialized usage of
                 opt_scope_ref. */
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
              cache_function_decl(cache, is_class_scope(*opt_scope_ref),
                                  /*is_dtor=*/FALSE, idsfp->access,
                                  /*cache_access_spec=*/FALSE,
                                  itsfp->convention, idsfp->traits,
                                  itsfp->traits, vendor_traits, itsfp->target,
                                  cache_name_fn, params, itsfp->source,
                                  &itsfp->eh_spec, &idsfp->locus);
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
            }
          }
        }
        break;
      case ifc_DeclSort_Method:
        { /* We're reconstructing a method. */
          an_ifc_DeclSort_Method idsm, *idsmp;
          ifc_MsvcTraits         vendor_traits;

          check_assertion(is_instantiation);
          idsmp = get_DeclSort_Method(&idsm);
#if CHECKING
          assert_in_class_scope(this, idsmp);
#endif /* CHECKING */
          { /* Reconstruct the templated declaration. */
            auto cache_name_fn = [this, cache, decl, idsmp](
                                              a_source_position_ptr decl_pos) {
              cache_simple_template_id(cache, decl->form, &idsmp->locus);
            };
            an_ifc_TypeSort_Method itsm, *itsmp;
            ifc_ChartIndex         params = (ifc_ChartIndex)0;

            check_assertion(type_tag(idsmp->type) == ifc_TypeSort_Method);
            read_prechecked_partition_element(idsmp->type);
            itsmp = get_TypeSort_Method(&itsm);
            if (itsmp->source != 0) {
              params = get_func_params_from_trait(decl_idx);
            }  /* if */
            vendor_traits = get_vendor_traits(decl_idx);
            cache_function_decl(cache, /*class_member=*/TRUE,
                                /*is_dtor=*/FALSE, idsmp->access,
                                /*cache_access_spec=*/FALSE,
                                itsmp->convention, idsmp->traits,
                                itsmp->traits, vendor_traits, itsmp->target,
                                cache_name_fn, params, itsmp->source,
                                &itsmp->eh_spec, &idsmp->locus);
          }
        }
        break;
      case ifc_DeclSort_Constructor:
        { /* We're reconstructing a constructor. */
          an_ifc_DeclSort_Constructor idsc, *idscp;
          ifc_MsvcTraits              vendor_traits;

          check_assertion(is_instantiation);
          idscp = get_DeclSort_Constructor(&idsc);
#if CHECKING
          assert_in_class_scope(this, idscp);
#endif /* CHECKING */
          { /* Reconstruct the templated declaration. */
            auto cache_name_fn = [this, cache, decl, idscp](
                                              a_source_position_ptr decl_pos) {
              cache_simple_template_id(cache, decl->form, &idscp->locus);
            };
            an_ifc_TypeSort_Tor itst, *itstp;
            ifc_ChartIndex      params = (ifc_ChartIndex)0;

            check_assertion(type_tag(idscp->type) == ifc_TypeSort_Tor);
            read_prechecked_partition_element(idscp->type);
            itstp = get_TypeSort_Tor(&itst);
            if (itstp->source != 0) {
              params = get_func_params_from_trait(decl_idx);
            }  /* if */
            vendor_traits = get_vendor_traits(decl_idx);
            cache_function_decl(cache, /*class_member=*/TRUE,
                                /*is_dtor=*/FALSE, idscp->access,
                                /*cache_access_spec=*/FALSE,
                                itstp->convention, idscp->traits,
                                (ifc_FunctionTypeTraits)0, vendor_traits,
                                (ifc_TypeIndex)0, cache_name_fn, params,
                                itstp->source, &itstp->eh_spec, &idscp->locus);
          }
        }
        break;
      default:
        unexpected_condition_str("Unexpected DeclSort");
    }  /* switch */
  }
done:;
}  /* cache_decl_specialization */


void an_ifc_module::cache_type_param_introducer(a_token_cache_ptr  cache,
                                                ifc_ExprIndex      constraint,
                                                a_boolean          is_pack,
                                                a_source_position  *pos)
/*
cache is the token cache to update.  constraint is zero for unconstrained
parameters or refers to a concept-id expression otherwise.  In the former case,
add a "typename" token to introduce a template parameter, but in the latter
case emit the concept-id.  is_pack is TRUE if the type introducer represents a
parameter pack, FALSE otherwise.
*/
{
  if (constraint == (ifc_ExprIndex)0) {
    /* An unconstrained type parameter is introduced by the "typename"
       keyword (or "class", but we'll use "typename"). */
    cache_token(cache, tok_typename, pos);
  } else {
    cache_expr(cache, constraint);
  }  /* if */
  if (is_pack) {
    cache_token(cache, tok_ellipsis, pos);
  }  /* if */
}  /* cache_type_param_introducer */


template<typename a_Cache_fn>
static inline void cache_attr_fn(a_token_cache_ptr     cache,
                                 a_Cache_fn            cache_fn,
                                 a_source_position_ptr pos)
/*
Helper function for cache_attr to avoid code duplication for the brackets.
Add tokens for the leading and trailing attribute brackets to cache and
call the provided cache_fn to cache the actual attribute, in the appropriate
places.  pos is the position to use for the brackets.
*/
{
  cache_token(cache, tok_lbracket, pos);
  cache_token(cache, tok_lbracket, pos);
  cache_fn();
  cache_token(cache, tok_rbracket, pos);
  cache_token(cache, tok_rbracket, pos);
}  /* cache_attr_fn */


void an_ifc_module::cache_attr(a_token_cache_ptr cache,
                               ifc_AttrIndex     attr,
                               a_boolean         cache_brackets)
/*
Add the tokens corresponding to the given attribute (attr) to cache.  If
cache_brackets is TRUE, include the attribute brackets.  Otherwise, the caller
is responsible for ensuring that the brackets are cached appropriately.
*/
{
  ifc_AttrSort      tag = attr_tag(attr);
  a_source_position pos;

  read_prechecked_partition_element(attr);
  switch (tag) {
    case ifc_AttrSort_Nothing:
      if (cache_brackets) {
        /* FIXME: Is there a way to get a position for this? */
        pos = null_source_position;
        cache_attr_fn(cache, [](){}, &pos);
      }  /* if */
      break;
    case ifc_AttrSort_Basic:
      { an_ifc_AttrSort_Basic iasb, *iasbp;
        iasbp = get_AttrSort_Basic(&iasb);
        source_position_from_locus(&pos, &iasbp->word.locus);
        auto cache_fn = [cache, iasbp, this]() {
          cache_word(cache, &iasbp->word);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn, &pos);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_AttrSort_Scoped:
      { an_ifc_AttrSort_Scoped iass, *iassp;
        iassp = get_AttrSort_Scoped(&iass);
        source_position_from_locus(&pos, &iassp->scope.locus);
        auto cache_fn = [cache, iassp, &pos, this]() {
          cache_word(cache, &iassp->scope);
          cache_token(cache, tok_colon_colon, &pos);
          cache_word(cache, &iassp->member);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn, &pos);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_AttrSort_Labeled:
      { an_ifc_AttrSort_Labeled iasl, *iaslp;
        iaslp = get_AttrSort_Labeled(&iasl);
        source_position_from_locus(&pos, &iaslp->label.locus);
        auto cache_fn = [cache, iaslp, &pos, this]() {
          cache_word(cache, &iaslp->label);
          cache_token(cache, tok_colon, &pos);
          cache_attr(cache, iaslp->attribute, /*cache_brackets=*/FALSE);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn, &pos);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_AttrSort_Called:
      { an_ifc_AttrSort_Called iasc, *iascp;
        iascp = get_AttrSort_Called(&iasc);
        /* FIXME: Find a way to get a proper position for this. */
        pos = null_source_position;
        auto cache_fn = [cache, iascp, &pos, this]() {
          cache_attr(cache, iascp->function, /*cache_brackets=*/FALSE);
          cache_token(cache, tok_lparen, &pos);
          cache_attr(cache, iascp->arguments, /*cache_brackets=*/FALSE);
          cache_token(cache, tok_rparen, &pos);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn, &pos);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_AttrSort_Expanded:
      { an_ifc_AttrSort_Expanded iase, *iasep;
        iasep = get_AttrSort_Expanded(&iase);
        /* FIXME: Find a way to get a proper position for this. */
        pos = null_source_position;
        auto cache_fn = [cache, iasep, &pos, this]() {
          cache_attr(cache, iasep->operand, /*cache_brackets=*/FALSE);
          cache_token(cache, tok_ellipsis, &pos);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn, &pos);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_AttrSort_Factored:
      { an_ifc_AttrSort_Factored iasf, *iasfp;
        iasfp = get_AttrSort_Factored(&iasf);
        source_position_from_locus(&pos, &iasfp->factor.locus);
        auto cache_fn = [cache, iasfp, &pos, this]() {
          cache_token(cache, tok_using, &pos);
          cache_word(cache, &iasfp->factor);
          cache_token(cache, tok_colon, &pos);
          cache_attr(cache, iasfp->terms, /*cache_brackets=*/FALSE);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn, &pos);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_AttrSort_Elaborated:
      { an_ifc_AttrSort_Elaborated iase, *iasep;
        iasep = get_AttrSort_Elaborated(&iase);
        /* FIXME: Find a way to get a proper position for this. */
        pos = null_source_position;
        auto cache_fn = [cache, iasep, this]() {
          cache_expr(cache, iasep->expression);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn, &pos);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_AttrSort_Tuple:
      /* FIXME: The IFC specification states:

           An AttrIndex reference with tag AttrSort::Tuple denotes a sequence
           of comma-separated attributes.

         This isn't quite right, and in the binary IFC files [[foo]] [[bar]] is
         treated equivalently to [[foo, bar]].  In other words, this is used to
         represent a group of attributes regardless of syntax.  We could choose
         to reconstruct the attributes with the latter representation, but to
         simplify the logic of this function (at least until all AttrSorts are
         implemented) we use the former representation (as this ensures we
         don't end up with an empty attribute, i.e., "[[]]"). */
      { an_ifc_AttrSort_Tuple iast, *iastp;
        iastp = get_AttrSort_Tuple(&iast);
        /* Retrieve the attribute indexes from the attribute heap, then recurse
           to process the attributes at the retrieved indexes. */
        for (uint32_t idx = 0; idx < iastp->cardinality; ++idx) {
          ifc_AttrIndex attr_idx =
                       (ifc_AttrIndex)read_index_from_heap(ifc_heap_attr,
                                                           iastp->start + idx);
          cache_attr(cache, attr_idx, /*cache_brackets=*/TRUE);
        }  /* for */
      }
      break;
    case ifc_AttrSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected AttrSort");
  }  /* switch */
}  /* cache_attr */


void an_ifc_module::cache_attrs(a_token_cache_ptr cache,
                                ifc_DeclIndex     decl_idx)
/*
Add the tokens corresponding to the attributes of the given declaration at
decl_idx to the cache.
*/
{
  ifc_AttrIndex attr_idx = attr_index_of(decl_idx);

  if (attr_idx != 0) {
    cache_attr(cache, attr_idx, /*cache_brackets=*/TRUE);
  }  /* if */
}  /* cache_attrs */


void an_ifc_module::cache_template_head(a_token_cache_ptr     cache,
                                        ifc_ChartIndex        chart_idx,
                                        a_source_position_ptr pos)
/*
Add the tokens corresponding to the template head described by the given
chart_idx to the cache.  pos is the position of the associated template or
specialization.
*/
{
  cache_token(cache, tok_template, pos);
  if (chart_idx == 0) {
    cache_token(cache, tok_lt, pos);
    cache_token(cache, tok_gt, pos);
  } else {
    cache_chart(cache, chart_idx, pos);
  }  /* if */
}  /* cache_template_head */


template<typename an_ifc_DeclSort_T>
static a_boolean function_is_user_defined(an_ifc_DeclSort_T *decl)
/*
Return TRUE if the given function-like IFC declaration node has a definition
that is not "= default" or "= delete"; otherwise, return FALSE.
*/
{
  /* For the IFC to provide a function definition, the function must be
     constexpr and the definition must be exported (marked by the presence of a
     reachable initializer property). */
  return (decl->properties & ifc_ReachableProperties_Initializer) &&
         !(decl->traits & (ifc_FunctionTraits_Defaulted |
                           ifc_FunctionTraits_Deleted)) &&
         (decl->traits & ifc_FunctionTraits_Constexpr ||
          decl->traits & ifc_FunctionTraits_Immediate);
}  /* function_is_user_defined */


void an_ifc_module::cache_decl(a_token_cache_ptr cache,
                               ifc_DeclIndex     decl)
/*
Add the tokens corresponding to the given declaration (decl) to cache.
*/
{
  ifc_DeclSort      tag = decl_tag(decl);
  a_source_position pos;

  read_prechecked_partition_element(decl);
  switch (tag) {
    case ifc_DeclSort_VendorExtension:
      issue_unsupported_node_diag("DeclSort::VendorExtension",
                                  &error_position);
      break;
    case ifc_DeclSort_Enumerator:
      { an_ifc_DeclSort_Enumerator idse, *idsep;
        /* An enumerator declaration is part of an enumeration declaration.
           The type, access, and specifiers should all be handled by the parent
           declaration. */
        idsep = get_DeclSort_Enumerator(&idse);
        source_position_from_locus(&pos, &idsep->locus);
        cache_identifier(cache, get_string_at_offset(idsep->name), &pos);
        if (idsep->initializer != 0) {
          cache_token(cache, tok_assign, &pos);
          cache_expr(cache, idsep->initializer);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Variable:
      { an_ifc_DeclSort_Variable idsv, *idsvp;
        ifc_Access               access = ifc_Access_None;

        idsvp = get_DeclSort_Variable(&idsv);
        source_position_from_locus(&pos, &idsvp->locus);
        {
          /* Retrieve access information. */
          Opt<ifc_Access> opt_access;

          opt_access = get_ifc_access(idsvp);
          if (opt_access.has_value()) {
            access = *opt_access;
          }  /* if */
        }
        cache_variable_decl(cache, decl, /*is_class_member=*/FALSE, access,
                            idsvp->specifiers, idsvp->traits, idsvp->alignment,
                            idsvp->type, idsvp->name, (ifc_TextOffset)0,
                            (ifc_ExprIndex)0, idsvp->initializer,
                            &idsvp->locus);
      }
      break;
    case ifc_DeclSort_Parameter:
      { an_ifc_DeclSort_Parameter idsp, *idspp;
        a_boolean                 need_second_pass = FALSE;
        idspp = get_DeclSort_Parameter(&idsp);
        source_position_from_locus(&pos, &idspp->locus);
        switch (idspp->sort) {
          case ifc_ParameterSort_Type:
            { a_boolean is_pack = type_tag(idspp->type) ==
                                                        ifc_TypeSort_Expansion;

              cache_type_param_introducer(cache, idspp->constraint, is_pack,
                                          &pos);
            }
            break;
          case ifc_ParameterSort_NonType:
            cache_type_first_pass(cache, idspp->type, &idspp->locus);
            need_second_pass = TRUE;
            break;
          case ifc_ParameterSort_Template:
            if (idspp->type != 0) {
              cache_type(cache, idspp->type, &idspp->locus);
            } else {
              /* FIXME: Confirm this is actually what is implied by a NULL
                 type. */
              cache_token(cache, tok_template, &pos);
              cache_token(cache, tok_lt, &pos);
              cache_token(cache, tok_typename, &pos);
              cache_token(cache, tok_ellipsis, &pos);
              cache_token(cache, tok_gt, &pos);
              cache_token(cache, tok_typename, &pos);
            }  /* if */
            break;
          case ifc_ParameterSort_Object:
            /* This is a function parameter rather than a template parameter.
               We should not run into those here. */
            unexpected_condition();
            break;
          default_is_unexpected_str("Unexpected ParameterSort");
        }  /* switch */
        if (idspp->name != 0) {
          cache_identifier(cache, get_string_at_offset(idspp->name), &pos);
        }  /* if */
        if (need_second_pass) {
          cache_type_second_pass(cache, idspp->type, &idspp->locus);
        }  /* if */
        if (idspp->initializer != 0 && !suppress_default_arguments) {
          cache_token(cache, tok_assign, &pos);
          cache_expr(cache, idspp->initializer);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Field:
      { an_ifc_DeclSort_Field idsf, *idsfp;
        idsfp = get_DeclSort_Field(&idsf);
        check_assertion((idsfp->specifiers & ifc_BasicSpecifiers_C) == 0);
        cache_variable_decl(cache, decl, /*is_class_member=*/TRUE,
                            idsfp->access, idsfp->specifiers, idsfp->traits,
                            idsfp->alignment, idsfp->type, (ifc_NameIndex)0,
                            idsfp->name, (ifc_ExprIndex)0, idsfp->initializer,
                            &idsfp->locus);
      }
      break;
    case ifc_DeclSort_Bitfield:
      { an_ifc_DeclSort_Bitfield idsbf, *idsbfp;
        idsbfp = get_DeclSort_Bitfield(&idsbf);
        check_assertion((idsbfp->specifiers & ifc_BasicSpecifiers_C) == 0);
        check_assertion(idsbfp->width != 0);
        cache_variable_decl(cache, decl, /*is_class_member=*/TRUE,
                            idsbfp->access, idsbfp->specifiers, idsbfp->traits,
                            (ifc_ExprIndex)0, idsbfp->type, (ifc_NameIndex)0,
                            idsbfp->name, idsbfp->width, idsbfp->initializer,
                            &idsbfp->locus);
      }
      break;
    case ifc_DeclSort_Scope:
      { an_ifc_DeclSort_Scope idss, *idssp;
        idssp = get_DeclSort_Scope(&idss);
        cache_scope_decl(cache, decl, idssp->type, idssp->name, idssp->base,
                         idssp->initializer, &idssp->locus);
      }
      break;
    case ifc_DeclSort_Enumeration:
      { an_ifc_DeclSort_Enumeration idse, *idsep;
        an_ifc_TypeSort_Fundamental itsf, *itsfp;
        idsep = get_DeclSort_Enumeration(&idse);
        check_assertion(type_tag(idsep->type) == ifc_TypeSort_Fundamental);
        read_prechecked_partition_element(idsep->type);
        itsfp = get_TypeSort_Fundamental(&itsf);
        source_position_from_locus(&pos, &idsep->locus);
        {
          Opt<ifc_Access> opt_access = get_ifc_access(idsep);

          if (opt_access.has_value()) {
            cache_access(cache, *opt_access, /*cache_colon=*/TRUE, &pos);
          }  /* if */
        }
        cache_basic_specifiers(cache, idsep->specifiers, &pos);
        switch (itsfp->basis) {
          case ifc_TypeBasis_Enum:
            cache_token(cache, tok_enum, &pos);
            break;
          case ifc_TypeBasis_Struct:
            cache_token(cache, tok_enum_struct, &pos);
            break;
          case ifc_TypeBasis_Class:
            cache_token(cache, tok_enum_class, &pos);
            break;
          default:
            unexpected_condition();
        }  /* switch */
        if (idsep->alignment != 0) {
          cache_token(cache, tok_alignas, &pos);
          cache_token(cache, tok_lparen, &pos);
          cache_expr(cache, idsep->alignment);
          cache_token(cache, tok_rparen, &pos);
        }  /* if */
        cache_identifier(cache, get_string_at_offset(idsep->name), &pos);
        if (idsep->base != 0) {
          cache_token(cache, tok_colon, &pos);
          cache_type(cache, idsep->base, &idsep->locus);
        }  /* if */
        if (idsep->initializer.cardinality != 0) {
          cache_token(cache, tok_lbrace, &pos);
          for (uint32_t idx = 0; idx < idsep->initializer.cardinality; ++idx) {
            if (idx > 0) {
              cache_token(cache, tok_comma, &pos);
            }  /* if */
            cache_decl(cache, make_decl_index(ifc_DeclSort_Enumerator,
                                              idsep->initializer.start + idx));
          }  /* for */
          cache_token(cache, tok_rbrace, &pos);
        }  /* if */
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_DeclSort_Alias:
      { an_ifc_DeclSort_Alias       idsa, *idsap;
        ifc_TypeSort                alias_tag;
        idsap = get_DeclSort_Alias(&idsa);
        alias_tag = type_tag(idsap->type);
        source_position_from_locus(&pos, &idsap->locus);
        cache_access(cache, idsap->access, /*cache_colon=*/TRUE, &pos);
        if (alias_tag == ifc_TypeSort_Fundamental) {
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          read_prechecked_partition_element(idsap->type);
          itsfp = get_TypeSort_Fundamental(&itsf);
          if (itsfp->basis == ifc_TypeBasis_Typename) {
            cache_token(cache, tok_using, &pos);
          } else {
            check_assertion(itsfp->basis == ifc_TypeBasis_Namespace);
            cache_token(cache, tok_namespace, &pos);
          }  /* if */
          cache_identifier(cache, get_string_at_offset(idsap->name), &pos);
          cache_token(cache, tok_assign, &pos);
          cache_type(cache, idsap->aliasee, &idsap->locus);
        } else if (alias_tag == ifc_TypeSort_Forall) {
          an_ifc_TypeSort_Forall itsf, *itsfp;
          check_assertion(type_tag(idsap->aliasee) == ifc_TypeSort_Forall);
          read_prechecked_partition_element(idsap->aliasee);
          itsfp = get_TypeSort_Forall(&itsf);
          cache_token(cache, tok_template, &pos);
          cache_chart(cache, itsfp->chart, &idsap->locus);
          cache_token(cache, tok_using, &pos);
          cache_identifier(cache, get_string_at_offset(idsap->name), &pos);
          cache_token(cache, tok_assign, &pos);
          cache_type(cache, itsfp->subject, &idsap->locus);
        } else {
          unexpected_condition();
        }  /* if */
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_DeclSort_Temploid:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::Temploid", &error_position);
      break;
    case ifc_DeclSort_Template:
      { an_ifc_DeclSort_Template idst, *idstp;
        idstp = get_DeclSort_Template(&idst);
        cache_decl_template(cache, idstp);
        {
          /* Cache the associated specializations. */
          Opt<an_ifc_Ref<ifc_DeclIndex>> opt_home_scope;

          opt_home_scope = get_ifc_home_scope_decl(decl);
          if (opt_home_scope.has_value()) {
            ifc_Sequence seq = get_specialization_sequence_from_trait(decl);

            cache_scope_member_sequence(cache, *opt_home_scope, seq);
          }  /* if */
        }
      }
      break;
    case ifc_DeclSort_PartialSpecialization:
      { an_ifc_DeclSort_PartialSpecialization idsps, *idspsp;
        idspsp = get_DeclSort_PartialSpecialization(&idsps);
        cache_decl_partial_specialization(cache, decl, idspsp);
      }
      break;
    case ifc_DeclSort_Specialization:
      { an_ifc_DeclSort_Specialization idss, *idssp;
        idssp = get_DeclSort_Specialization(&idss);
        cache_decl_specialization(cache, decl, idssp);
      }
      break;
    case ifc_DeclSort_Concept:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::Concept", &error_position);
      break;
    case ifc_DeclSort_Function:
      { an_ifc_DeclSort_Function idsf, *idsfp;
        an_ifc_TypeSort_Function itsf, *itsfp;
        ifc_ChartIndex           params = (ifc_ChartIndex)0;
        ifc_Access               access = ifc_Access_None;
        ifc_MsvcTraits           vendor_traits;

        idsfp = get_DeclSort_Function(&idsf);
        check_assertion(type_tag(idsfp->type) == ifc_TypeSort_Function);
        read_prechecked_partition_element(idsfp->type);
        itsfp = get_TypeSort_Function(&itsf);
        if (itsfp->source != 0) {
          params = get_func_params_from_trait(decl);
        }  /* if */
        {
          /* Retrieve access information. */
          Opt<ifc_Access> opt_access = get_ifc_access(idsfp);

          if (opt_access.has_value()) {
            access = idsfp->access;
          }  /* if */
        }
        vendor_traits = get_vendor_traits(decl);
        cache_function_decl(cache, /*class_member=*/FALSE, /*is_dtor=*/FALSE,
                            access, itsfp->convention, idsfp->traits,
                            itsfp->traits, vendor_traits, itsfp->target,
                            idsfp->name, params, itsfp->source,
                            &itsfp->eh_spec, &idsfp->locus);
      }
      break;
    case ifc_DeclSort_Method:
      { an_ifc_DeclSort_Method idsm, *idsmp;
        an_ifc_TypeSort_Method itsm, *itsmp;
        ifc_ChartIndex         params = (ifc_ChartIndex)0;
        ifc_TypeIndex          target = (ifc_TypeIndex)0;
        ifc_MsvcTraits         vendor_traits;

        idsmp = get_DeclSort_Method(&idsm);
        check_assertion(type_tag(idsmp->type) == ifc_TypeSort_Method);
        read_prechecked_partition_element(idsmp->type);
        itsmp = get_TypeSort_Method(&itsm);
        check_assertion(idsmp->access != ifc_Access_None);
        if (name_tag(idsmp->name) == ifc_NameSort_Conversion) {
          /* This is a conversion function, so the return type should not be
             cached. */
        } else {
          target = itsmp->target;
        }  /* if */
        if (itsmp->source != 0) {
          params = get_func_params_from_trait(decl);
        }  /* if */
        vendor_traits = get_vendor_traits(decl);
        cache_function_decl(cache, /*class_member=*/TRUE, /*is_dtor=*/FALSE,
                            idsmp->access, itsmp->convention, idsmp->traits,
                            itsmp->traits, vendor_traits, target, idsmp->name,
                            params, itsmp->source, &itsmp->eh_spec,
                            &idsmp->locus);
        if (idsmp->properties & ifc_ReachableProperties_Initializer) {
          /* A body is likely available: Record this availability using a
             pseudo-token that will be translated when the declaration is
             parsed. */
          cache_token(cache, tok_pending_ifc_func_body, &null_source_position);
          cache->last_token->extra_info_kind = teik_ifc_decl;
          cache->last_token->variant.ifc_decl.index = decl;
          cache->last_token->variant.ifc_decl.module = this;
        }  /* if */
      }
      break;
    case ifc_DeclSort_Constructor:
      { an_ifc_DeclSort_Constructor    idsc, *idscp;
        an_ifc_TypeSort_Tor            itst, *itstp;
        ifc_ChartIndex                 params = (ifc_ChartIndex)0;
        ifc_MsvcTraits                 vendor_traits;
        Opt<an_ifc_Ref<ifc_NameIndex>> opt_name_ref;

        idscp = get_DeclSort_Constructor(&idsc);
        check_assertion(type_tag(idscp->type) == ifc_TypeSort_Tor);
        read_prechecked_partition_element(idscp->type);
        itstp = get_TypeSort_Tor(&itst);
        if (itstp->source != 0) {
          params = get_func_params_from_trait(decl);
        }  /* if */
        vendor_traits = get_vendor_traits(decl);
        opt_name_ref = get_ifc_name(idscp);
        /* FIXME: This should be a soft failure. */
        check_assertion(opt_name_ref.has_value());
        cache_function_decl(cache, /*class_member=*/TRUE, /*is_dtor=*/FALSE,
                            idscp->access, itstp->convention, idscp->traits,
                            (ifc_FunctionTypeTraits)0, vendor_traits,
                            (ifc_TypeIndex)0, *opt_name_ref, params,
                            itstp->source, &itstp->eh_spec, &idscp->locus);
        if (function_is_user_defined(idscp)) {
          /* A body is likely available: Record this availability using a
             pseudo-token that will be translated when the declaration is
             parsed. */
          cache_token(cache, tok_pending_ifc_func_body, &null_source_position);
          cache->last_token->extra_info_kind = teik_ifc_decl;
          cache->last_token->variant.ifc_decl.index = decl;
          cache->last_token->variant.ifc_decl.module = this;
        }  /* if */
      }
      break;
    case ifc_DeclSort_InheritedConstructor:
      { an_ifc_DeclSort_InheritedConstructor idsic, *idsicp;
        idsicp = get_DeclSort_InheritedConstructor(&idsic);
        source_position_from_locus(&pos, &idsicp->locus);
        cache_token(cache, tok_using, &pos);
        cache_name_from_decl(cache, idsicp->base_ctor, &idsicp->locus);
        cache_token(cache, tok_colon_colon, &pos);
        cache_name_from_decl(cache, idsicp->base_ctor, &idsicp->locus);
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_DeclSort_Destructor:
      { an_ifc_DeclSort_Destructor     idsd, *idsdp;
        ifc_MsvcTraits                 vendor_traits;
        Opt<an_ifc_Ref<ifc_NameIndex>> opt_name_ref;

        idsdp = get_DeclSort_Destructor(&idsd);
        vendor_traits = get_vendor_traits(decl);
        opt_name_ref = get_ifc_name(idsdp);
        /* FIXME: This should be a soft failure. */
        check_assertion(opt_name_ref.has_value());
        cache_function_decl(cache, /*class_member=*/TRUE, /*is_dtor=*/TRUE,
                            idsdp->access, idsdp->convention, idsdp->traits,
                            (ifc_FunctionTypeTraits)0, vendor_traits,
                            (ifc_TypeIndex)0, *opt_name_ref, (ifc_ChartIndex)0,
                            (ifc_TypeIndex)0, &idsdp->eh_spec, &idsdp->locus);
        if (function_is_user_defined(idsdp)) {
          /* A body is likely available: Record this availability using a
             pseudo-token that will be translated when the declaration is
             parsed. */
          cache_token(cache, tok_pending_ifc_func_body, &null_source_position);
          cache->last_token->extra_info_kind = teik_ifc_decl;
          cache->last_token->variant.ifc_decl.index = decl;
          cache->last_token->variant.ifc_decl.module = this;
        }  /* if */
      }
      break;
    case ifc_DeclSort_Reference:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::Reference", &error_position);
      break;
    case ifc_DeclSort_UsingDeclaration:
      { an_ifc_DeclSort_UsingDeclaration idsud, *idsudp;
        idsudp = get_DeclSort_UsingDeclaration(&idsud);
        source_position_from_locus(&pos, &idsudp->locus);
        cache_access(cache, idsudp->access, /*cache_colon=*/TRUE, &pos);
        cache_basic_specifiers(cache, idsudp->specifiers, &pos);
        cache_token(cache, tok_using, &pos);
        if (idsudp->parent != 0) {
          cache_expr(cache, idsudp->parent);
          cache_token(cache, tok_colon_colon, &pos);
        }  /* if */
        cache_identifier(cache, get_string_at_offset(idsudp->name), &pos);
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_DeclSort_UsingDirective:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::UsingDirective", &error_position);
      break;
    case ifc_DeclSort_Friend:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::Friend", &error_position);
      break;
    case ifc_DeclSort_Expansion:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::Expansion", &error_position);
      break;
    case ifc_DeclSort_DeductionGuide:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::DeductionGuide", &error_position);
      break;
    case ifc_DeclSort_Barren:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::Barren", &error_position);
      break;
    case ifc_DeclSort_Tuple:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::Tuple", &error_position);
      break;
    case ifc_DeclSort_SyntaxTree:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::SyntaxTree", &error_position);
      break;
    case ifc_DeclSort_Intrinsic:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::Intrinsic", &error_position);
      break;
    case ifc_DeclSort_Property:
      { an_ifc_DeclSort_Property idsp, *idspp;
        idspp = get_DeclSort_Property(&idsp);
        /* FIXME: The declaration for idspp->member contains the preamble that
           must appear before the property declspec, but also the remainder
           that must appear after.  For now, just ignore the property. */
#if 0
        cache_token(cache, tok_declspec, &pos);
        cache_token(cache, tok_lparen, &pos);
        cache_identifier(cache, "property", &pos);
        cache_token(cache, tok_lparen, &pos);
        cache_identifier(cache, "get", &pos);
        cache_token(cache, tok_assign, &pos);
        cache_identifier(cache, get_string_at_offset(idspp->getter), &pos);
        cache_token(cache, tok_comma, &pos);
        cache_identifier(cache, "put", &pos);
        cache_token(cache, tok_assign, &pos);
        cache_identifier(cache, get_string_at_offset(idspp->setter), &pos);
        cache_token(cache, tok_rparen, &pos);
        cache_token(cache, tok_rparen, &pos);
#endif /* 0 */
        cache_decl(cache, idspp->member);
      }
      break;
    case ifc_DeclSort_OutputSegment:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::OutputSegment", &error_position);
      break;
    case ifc_DeclSort_UnusedSort0:
    case ifc_DeclSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected DeclSort");
  }  /* switch */
}  /* cache_decl */


inline void an_ifc_module::update_name_qualification_suppression(
                                                   an_ifc_ExprSort_Path *iespp)
/*
An internal method for setting any necessary automatic nested name specifier
qualification suppression flags based on the given path.

As part of the contract for this function, the caller is responsible for
restoring previous state of the potentially affected suppression flags.
Currently this includes the variable(s):
suppress_automatic_namespace_qualification.
*/
{
  if (expr_tag(iespp->scope) == ifc_ExprSort_NamedDecl) {
    an_ifc_ExprSort_NamedDecl iesnd, *iesndp;

    read_prechecked_partition_element(iespp->scope);
    iesndp = get_ExprSort_NamedDecl(&iesnd);
    if (decl_tag(iesndp->resolution) == ifc_DeclSort_Scope) {
      an_ifc_DeclSort_Scope idss, *idssp;

      read_prechecked_partition_element(iesndp->resolution);
      idssp = get_DeclSort_Scope(&idss);
      {
        an_ifc_TypeSort_Fundamental itsf, *itsfp;

        read_prechecked_partition_element(idssp->type);
        itsfp = get_TypeSort_Fundamental(&itsf);
        switch (itsfp->basis) {
        case ifc_TypeBasis_Namespace:
          /* This path already contains its namespace qualification, suppress
             automatic namespace qualification. */
          suppress_automatic_namespace_qualification = TRUE;
          break;
        default:
          break;
        }  /* switch */
      }
    }  /* if */
  }  /* if */
}  /* suppress_automatic_qualification */


static void cache_args_with_parens(a_token_cache_ptr  cache,
                                   an_ifc_module      *ifc_mod,
                                   ifc_ExprIndex      args,  
                                   a_source_position  *pos)
/*
Record tokens for the IFC expression described by args in the given cache and
enclose them with parentheses.  If args is an IFC ExpressionList, be sure to
avoid double parentheses.  ifc_mod points to the associated IFC module reader
and pos is the associated source position.
*/
{
  ifc_ExprSort  arguments_tag = expr_tag(args);

  if (arguments_tag != ifc_ExprSort_ExpressionList) {
    cache_token(cache, tok_lparen, pos);
  }  /* if */
  ifc_mod->cache_expr(cache, args);
  if (arguments_tag != ifc_ExprSort_ExpressionList) {
    cache_token(cache, tok_rparen, pos);
  }  /* if */
}  /* cache_args_with_parens */


void an_ifc_module::cache_expr(a_token_cache_ptr    cache,
                               ifc_ExprIndex        expr,
             /* Defaulted: */  a_cache_expr_option  options)
/*
Add the tokens corresponding to the given expression (expr) to cache.
If options & ceo_qualified_name is nonzero, separate tuple elements by '::'
instead of ','.  If options & ceo_skip_assign is nonzero, only render the
second operand of an assignment.
*/
{
  ifc_ExprSort      tag = expr_tag(expr);
  a_source_position pos;

  read_prechecked_partition_element(expr);
  switch (tag) {
    case ifc_ExprSort_VendorExtension:
      issue_unsupported_node_diag("ExprSort::VendorExtension",
                                  &error_position);
      break;
    case ifc_ExprSort_Empty:
      /* Nothing to cache here - literally an empty expression. */
      break;
    case ifc_ExprSort_Literal:
      { an_ifc_ExprSort_Literal iesl, *ieslp;
        a_constant_ptr          cp;
        ieslp = get_ExprSort_Literal(&iesl);
        if (type_tag(ieslp->type) == ifc_TypeSort_Designated) {
          /* FIXME: This can show up as a literal type in some cases, but isn't
             really a literal in the sense that there's a constant to cache. */
          cache_type(cache, ieslp->type, &ieslp->locus);
        } else {
          cp = constant_for_expr_index(expr, /*default_type=*/NULL);
          source_position_from_locus(&pos, &ieslp->locus);
          cache_literal(cache, cp, &pos);
        }  /* if */
      }
      break;
    case ifc_ExprSort_Lambda:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Lambda", &error_position);
      break;
    case ifc_ExprSort_Type:
      { an_ifc_ExprSort_Type iest, *iestp;
        iestp = get_ExprSort_Type(&iest);
        cache_type(cache, iestp->denotation, &iestp->locus);
      }
      break;
    case ifc_ExprSort_NamedDecl:
      { an_ifc_ExprSort_NamedDecl iesnd, *iesndp;
        iesndp = get_ExprSort_NamedDecl(&iesnd);
        source_position_from_locus(&pos, &iesndp->locus);
        if (is_name_qualifiable(iesndp->resolution)) {
          cache_qualified_name_from_decl(cache, iesndp->resolution,
                                         &iesndp->locus);
        } else {
          /* We are contextually forbidden from qualifying this name or the
             declaration type otherwise is considered to never appear
             with a qualified name.

             This can happen when building an unqualified-id within a dependent
             context. */
          cache_name_from_decl(cache, iesndp->resolution, &iesndp->locus);
        }  /* if */
      }
      break;
    case ifc_ExprSort_UnresolvedId:
      { an_ifc_ExprSort_UnresolvedId iesuid, *iesuidp;
        iesuidp = get_ExprSort_UnresolvedId(&iesuid);
        source_position_from_locus(&pos, &iesuidp->locus);
        cache_name(cache, iesuidp->name, &iesuidp->locus);
      }
      break;
    case ifc_ExprSort_TemplateId:
      { an_ifc_ExprSort_TemplateId iestid, *iestidp;
        iestidp = get_ExprSort_TemplateId(&iestid);
        source_position_from_locus(&pos, &iestidp->locus);
        cache_expr(cache, iestidp->primary);
        cache_token(cache, tok_lt, &pos);
        if (iestidp->arguments != 0) {
          cache_expr(cache, iestidp->arguments);
        }  /* if */
        cache_token(cache, tok_gt, &pos);
      }
      break;
    case ifc_ExprSort_UnqualifiedId:
      { an_ifc_ExprSort_UnqualifiedId iesui, *iesuip;
        iesuip = get_ExprSort_UnqualifiedId(&iesui);
        source_position_from_locus(&pos, &iesuip->locus);
        if (iesuip->template_keyword.line != 0) {
          cache_token(cache, tok_template, &pos);
        }  /* if */
        /* It's possible to have this with no name/resolution at all.  For
           example, if a construct of the form "::foo::bar" has been encoded,
           the first element of the ExprSort::Tuple will refer to the empty
           string that precedes the first "::". */
        if (iesuip->resolution != 0) {
          Value_saver<a_boolean> suppression(
                                        &suppress_automatic_name_qualification,
                                        /*new_value=*/TRUE);

          cache_expr(cache, iesuip->resolution);
        } else if (iesuip->name != 0) {
          cache_name(cache, iesuip->name, &iesuip->locus);
        }  /* if */
      }
      break;
    case ifc_ExprSort_SimpleIdentifier:
      { an_ifc_ExprSort_SimpleIdentifier iessi, *iessip;
        iessip = get_ExprSort_SimpleIdentifier(&iessi);
        /* FIXME: Do we need to handle the "type" field here? */
        cache_name(cache, iessip->name, &iessip->locus);
      }
      break;
    case ifc_ExprSort_Pointer:
      { an_ifc_ExprSort_Pointer iesp, *iespp;
        iespp = get_ExprSort_Pointer(&iesp);
        source_position_from_locus(&pos, &iespp->locus);
        cache_token(cache, tok_star, &pos);
      }
      break;
    case ifc_ExprSort_QualifiedName:
      { an_ifc_ExprSort_QualifiedName iesqn, *iesqnp;
        iesqnp = get_ExprSort_QualifiedName(&iesqn);
        source_position_from_locus(&pos, &iesqnp->locus);
        /* FIXME: Do we need to handle the "type" field here? */
        if (iesqnp->typename_keyword.line != 0) {
          cache_token(cache, tok_typename, &pos);
        }  /* if */
        cache_expr(cache, iesqnp->elements, ceo_qualified_name);
      }
      break;
    case ifc_ExprSort_Path:
      { Value_saver<a_boolean> suppression(
                                  &suppress_automatic_namespace_qualification);
        an_ifc_ExprSort_Path   iesp, *iespp;

        iespp = get_ExprSort_Path(&iesp);
        if (!suppress_automatic_name_qualification) {
          /* FIXME: It shouldn't be possible to enter this switch case when
             processing an ifc_ExprSort_UnqualifiedId (i.e., when
             suppress_automatic_name_qualification is TRUE).  That implies an
             unqualified id contains a qualifier. */
          cache_expr(cache, iespp->scope);
          cache_token(cache, tok_colon_colon, &null_source_position);
          update_name_qualification_suppression(iespp);
        }  /* if */
        cache_expr(cache, iespp->member);
      }
      break;
    case ifc_ExprSort_Read:
      { an_ifc_ExprSort_Read iesr, *iesrp;
        iesrp = get_ExprSort_Read(&iesr);
        cache_expr(cache, iesrp->address);
        /* FIXME: Do we need to handle iesrp->sort here? */
      }
      break;
    case ifc_ExprSort_Monad:
      { an_ifc_ExprSort_Monad iesm, *iesmp;
        an_operator_kind      opkind;
        auto                  cache_arg = [&, this] {
          cache_token(cache, tok_lparen, &pos);
          if (iesmp->argument != 0) {
            cache_expr(cache, iesmp->argument);
          } /* if */
          cache_token(cache, tok_rparen, &pos);
        };  /* cache_arg */

        iesmp = get_ExprSort_Monad(&iesm);
        source_position_from_locus(&pos, &iesmp->locus);
        opkind = get_operator_kind(iesmp->assoc);
        switch (opkind) {
          case opkind_basic:
          case opkind_func_like:
            cache_operator(cache, iesmp->assoc, &iesmp->locus);
            if (iesmp->assoc == ifc_MonadicOperator_LookupGlobally) {
              /* Do not produce parentheses after a "::". */
              cache_expr(cache, iesmp->argument);
            } else {
              cache_arg();
            }  /* if */
            break;
          case opkind_post:
            cache_arg();
            cache_operator(cache, iesmp->assoc, &iesmp->locus);
            break;
          case opkind_other:
            { a_token_kind ltok, rtok;
              if (iesmp->assoc == ifc_MonadicOperator_Paren) {
                ltok = tok_lparen;
                rtok = tok_rparen;
              } else if (iesmp->assoc == ifc_MonadicOperator_Brace) {
                ltok = tok_lbrace;
                rtok = tok_rbrace;
              } else {
                unexpected_condition();
              }  /* if */
              cache_token(cache, ltok, &pos);
              cache_expr(cache, iesmp->argument);
              cache_token(cache, rtok, &pos);
            }
            break;
          case opkind_c_cast:
          case opkind_cpp_cast:
            unexpected_condition();
            break;
          default_is_unexpected();
        }  /* switch */
      }
      break;
    case ifc_ExprSort_Dyad:
      { an_ifc_ExprSort_Dyad iesd, *iesdp;
        an_operator_kind     opkind;

        iesdp = get_ExprSort_Dyad(&iesd);
        source_position_from_locus(&pos, &iesdp->locus);
        opkind = get_operator_kind(iesdp->assoc);
        switch (opkind) {
          case opkind_basic:
            if (iesdp->assoc == ifc_DyadicOperator_Comma) {
              cache_token(cache, tok_lparen, &pos);
            }  /* if */
            if ((options & ceo_skip_assign) == 0 ||
                iesdp->assoc != ifc_DyadicOperator_Assign) {
              cache_expr(cache, iesdp->arguments_0,
                         ceo_possible_temporary_decl);
              cache_operator(cache, iesdp->assoc, &iesdp->locus);
            }  /* if */
            cache_expr(cache, iesdp->arguments_1);
            if (iesdp->assoc == ifc_DyadicOperator_Comma) {
              cache_token(cache, tok_rparen, &pos);
            }  /* if */
            break;
          case opkind_func_like:
            if (iesdp->assoc == ifc_DyadicOperator_MsvcAlign) {
              /* An expression like "this->i" is represented in IFC files as
                 "this->__MsvcAlign(4, i)".  That has no equivalent in the
                 EDG IL.  So just cache the second argument. */
              cache_expr(cache, iesdp->arguments_1);
            } else {
              cache_operator(cache, iesdp->assoc, &iesdp->locus);
              cache_token(cache, tok_lparen, &pos);
              cache_expr(cache, iesdp->arguments_0);
              cache_token(cache, tok_comma, &pos);
              cache_expr(cache, iesdp->arguments_1);
              cache_token(cache, tok_rparen, &pos);
            }  /* if */
            break;
          case opkind_cpp_cast:
            cache_operator(cache, iesdp->assoc, &iesdp->locus);
            FALLTHROUGH
          case opkind_c_cast:
            if (opkind == opkind_c_cast) {
              cache_token(cache, tok_lparen, &pos);
            } else {
              cache_token(cache, tok_lt, &pos);
            }  /* if */
            cache_expr(cache, iesdp->arguments_0);
            if (opkind == opkind_c_cast) {
              cache_token(cache, tok_rparen, &pos);
            } else {
              cache_token(cache, tok_gt, &pos);
            }  /* if */
            cache_token(cache, tok_lparen, &pos);
            cache_expr(cache, iesdp->arguments_1);
            cache_token(cache, tok_rparen, &pos);
            break;
          case opkind_post:
          case opkind_other:
            unexpected_condition();
            break;
          default_is_unexpected();
        }  /* switch */
      }
      break;
    case ifc_ExprSort_Triad:
      { an_ifc_ExprSort_Triad iest, *iestp;
        iestp = get_ExprSort_Triad(&iest);
        source_position_from_locus(&pos, &iestp->locus);
        switch (iestp->assoc) {
          case ifc_TriadicOperator_Choice:
            {
              cache_expr(cache, iestp->arguments_0);
              cache_operator(cache, iestp->assoc, &iestp->locus);
              cache_expr(cache, iestp->arguments_1);
              cache_token(cache, tok_colon, &pos);
              cache_expr(cache, iestp->arguments_2);
            }
            break;
          case ifc_TriadicOperator_ConstructAt:
            {
              cache_operator(cache, iestp->assoc, &iestp->locus);
              cache_token(cache, tok_lparen, &pos);
              cache_expr(cache, iestp->arguments_0);
              cache_token(cache, tok_rparen, &pos);
              cache_expr(cache, iestp->arguments_1);
              if (iestp->arguments_2 != 0) {
                cache_args_with_parens(cache, this, iestp->arguments_2, &pos);
              }  /* if */
            }
            break;
          default:
            unexpected_condition();
        }  /* switch */
      }
      break;
    case ifc_ExprSort_String:
      { an_ifc_ExprSort_String iess, *iessp;
        iessp = get_ExprSort_String(&iess);
        cache_string(cache, iessp->string_index, &iessp->locus);
      }
      break;
    case ifc_ExprSort_Temporary:
      { an_ifc_ExprSort_Temporary iest, *iestp;
        iestp = get_ExprSort_Temporary(&iest);
        source_position_from_locus(&pos, &iestp->locus);
        if ((options & ceo_possible_temporary_decl) != 0) {
          cache_type(cache, iestp->type, &iestp->locus);
        }  /* if */
        cache_identifier(cache, make_ifc_temporary_unique_id(iestp->id), &pos);
      }
      break;
    case ifc_ExprSort_Call:
      { an_ifc_ExprSort_Call iesc, *iescp;
        iescp = get_ExprSort_Call(&iesc);
        source_position_from_locus(&pos, &iescp->locus);
        cache_expr(cache, iescp->operation);
        if (iescp->arguments != 0) {
          cache_args_with_parens(cache, this, iescp->arguments, &pos);
        } else {
          /* Sometimes (but not always) an empty argument list appears to be
             represented using a null "arguments" field. */
          source_position_from_locus(&pos, &iescp->locus);
          cache_token(cache, tok_lparen, &pos);
          cache_token(cache, tok_rparen, &pos);
        }  /* if */
      }
      break;
    case ifc_ExprSort_MemberInitializer:
      { an_ifc_ExprSort_MemberInitializer iesmi, *iesmip;
        iesmip = get_ExprSort_MemberInitializer(&iesmi);
        if (iesmip->member != 0) {
          /* A nonstatic member initialization. */
          an_ifc_Ref<ifc_DeclIndex> member_decl_ref(this, iesmip->member);

          source_position_from_locus(&pos, &iesmip->locus);
          cache_identifier(cache, name_from_decl(member_decl_ref), &pos);
        } else if (iesmip->base != 0) {
          /* A base subobject initialization. */
          cache_type(cache, iesmip->base, &iesmip->locus);
        } else {
          /* A delegating constructor. */
          issue_unsupported_node_diag("ExprSort::MemberInitializer",
                                      &error_position);
        }  /* if */
        cache_token(cache, tok_lparen, &null_source_position);
        cache_expr(cache, iesmip->initializer, ceo_skip_assign);
        cache_token(cache, tok_rparen, &null_source_position);
      }
      break;
    case ifc_ExprSort_MemberAccess:
      { an_ifc_ExprSort_MemberAccess iesma, *iesmap;
        iesmap = get_ExprSort_MemberAccess(&iesma);
        source_position_from_locus(&pos, &iesmap->locus);
        cache_identifier(cache, get_string_at_offset(iesmap->name), &pos);
      }
      break;
    case ifc_ExprSort_InheritancePath:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::InheritancePath",
                                  &error_position);
      break;
    case ifc_ExprSort_InitializerList:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::InitializerList",
                                  &error_position);
      break;
    case ifc_ExprSort_Cast:
      { an_ifc_ExprSort_Cast iesc, *iescp;
        iescp = get_ExprSort_Cast(&iesc);
        source_position_from_locus(&pos, &iescp->locus);
        switch (iescp->op) {
          case ifc_DyadicOperator_ExplicitConversion:
            cache_type(cache, (ifc_TypeIndex)iescp->target, &iescp->locus);
            cache_expr(cache, iescp->source);
            break;
          case ifc_DyadicOperator_Pretend:
            cache_token(cache, tok_lparen, &pos);
            cache_type(cache, (ifc_TypeIndex)iescp->target, &iescp->locus);
            cache_token(cache, tok_rparen, &pos);
            cache_expr(cache, iescp->source);
            break;
          case ifc_DyadicOperator_ReinterpretCast:
            cache_token(cache, tok_reinterpret_cast, &pos);
            goto common_cast;
          case ifc_DyadicOperator_StaticCast:
            cache_token(cache, tok_static_cast, &pos);
            goto common_cast;
          case ifc_DyadicOperator_ConstCast:
            cache_token(cache, tok_const_cast, &pos);
            goto common_cast;
          case ifc_DyadicOperator_DynamicCast:
            cache_token(cache, tok_dynamic_cast, &pos);
common_cast:
            cache_token(cache, tok_lt, &pos);
            cache_type(cache, (ifc_TypeIndex)iescp->target, &iescp->locus);
            cache_token(cache, tok_gt, &pos);
            cache_token(cache, tok_lparen, &pos);
            cache_expr(cache, iescp->source);
            cache_token(cache, tok_rparen, &pos);
            break;
          default:
            cache_type(cache, (ifc_TypeIndex)iescp->target, &iescp->locus);
            cache_expr(cache, iescp->source);
            unexpected_condition_str("Unexpected DyadicOperator "
                                     "for ExprSort::Cast");
        }  /* switch */
      }
      break;
    case ifc_ExprSort_Condition:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Condition", &error_position);
      break;
    case ifc_ExprSort_ExpressionList:
      { an_ifc_ExprSort_ExpressionList esel, *eselp;
        eselp = get_ExprSort_ExpressionList(&esel);
        if (eselp->delimiter != ifc_Delimiter_Unknown) {
          source_position_from_locus(&pos, &eselp->left);
          cache_token(cache,
                      eselp->delimiter == ifc_Delimiter_Brace ? tok_lbrace
                                                              : tok_lparen,
                      &pos);
        }  /* if */
        if (eselp->contents != (ifc_ExprIndex)0) {
          cache_expr(cache, eselp->contents);
        }  /* if */
        if (eselp->delimiter != ifc_Delimiter_Unknown) {
          source_position_from_locus(&pos, &eselp->right);
          cache_token(cache,
                      eselp->delimiter == ifc_Delimiter_Brace ? tok_rbrace
                                                              : tok_rparen,
                      &pos);
        }  /* if */
      }
      break;
    case ifc_ExprSort_SizeofType:
      { an_ifc_ExprSort_SizeofType iessot, *iessotp;
        iessotp = get_ExprSort_SizeofType(&iessot);
        source_position_from_locus(&pos, &iessotp->locus);
        cache_token(cache, tok_sizeof, &pos);
        cache_token(cache, tok_lparen, &pos);
        cache_type(cache, iessotp->operand, &iessotp->locus);
        cache_token(cache, tok_rparen, &pos);
      }
      break;
    case ifc_ExprSort_Alignof:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Alignof", &error_position);
      break;
    case ifc_ExprSort_Typeid:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Typeid", &error_position);
      break;
    case ifc_ExprSort_DestructorCall:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::DestructorCall", &error_position);
      break;
    case ifc_ExprSort_SyntaxTree:
      { an_ifc_ExprSort_SyntaxTree iesst, *iesstp;
        iesstp = get_ExprSort_SyntaxTree(&iesst);
        cache_syntax(cache, iesstp->syntax);
      }
      break;
    case ifc_ExprSort_FunctionString:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::FunctionString", &error_position);
      break;
    case ifc_ExprSort_CompoundString:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::CompoundString", &error_position);
      break;
    case ifc_ExprSort_StringSequence:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::StringSequence", &error_position);
      break;
    case ifc_ExprSort_Initializer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Initializer", &error_position);
      break;
    case ifc_ExprSort_Requires:
      { an_ifc_ExprSort_Requires  iesr, *iesrp;
        iesrp = get_ExprSort_Requires(&iesr);
        source_position_from_locus(&pos, &iesrp->locus);
        cache_token(cache, tok_requires, &pos);
        if (iesrp->parameters != 0) {
          cache_syntax(cache, iesrp->parameters);
        }  /* if */
        cache_syntax(cache, iesrp->body);
      }
      break;
    case ifc_ExprSort_UnaryFold:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::UnaryFold", &error_position);
      break;
    case ifc_ExprSort_BinaryFold:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::BinaryFold", &error_position);
      break;
    case ifc_ExprSort_HierarchyConversion:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::HierarchyConversion",
                                  &error_position);
      break;
    case ifc_ExprSort_ProductTypeValue:
      { an_ifc_ExprSort_ProductTypeValue iesptv, *iesptvp;
        a_type_ptr                       tp;
        a_constant_ptr                   cp;
        iesptvp = get_ExprSort_ProductTypeValue(&iesptv);
        source_position_from_locus(&pos, &iesptvp->locus);
        tp = type_for_type_index(iesptvp->type, /*kind=*/NULL);
        cp = constant_for_expr_index(expr, tp);
        cache_aggr_constant(cache, cp, &pos);
      }
      break;
    case ifc_ExprSort_SumTypeValue:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::SumTypeValue", &error_position);
      break;
    case ifc_ExprSort_SubobjectValue:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::SubobjectValue", &error_position);
      break;
    case ifc_ExprSort_ArrayValue:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::ArrayValue", &error_position);
      break;
    case ifc_ExprSort_DynamicDispatch:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::DynamicDispatch",
                                  &error_position);
      break;
    case ifc_ExprSort_VirtualFunctionConversion:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::VirtualFunctionConversion",
                                  &error_position);
      break;
    case ifc_ExprSort_Placeholder:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Placeholder", &error_position);
      break;
    case ifc_ExprSort_Expansion:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Expansion", &error_position);
      break;
    case ifc_ExprSort_Generic:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Generic", &error_position);
      break;
    case ifc_ExprSort_Tuple:
      { an_ifc_ExprSort_Tuple iest, *iestp;
        iestp = get_ExprSort_Tuple(&iest);
        source_position_from_locus(&pos, &iestp->locus);
        for (uint32_t idx = 0; idx < iestp->cardinality; ++idx) {
          ifc_ExprIndex eidx =
                       (ifc_ExprIndex)read_index_from_heap(ifc_heap_expr,
                                                           iestp->start + idx);
          if (idx > 0) {
            a_token_kind  sep = options & ceo_qualified_name ? tok_colon_colon
                                                             : tok_comma;
            cache_token(cache, sep, &pos);
          }  /* if */
          cache_expr(cache, eidx);
        }  /* for */
      }
      break;
    case ifc_ExprSort_Nullptr:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Nullptr", &error_position);
      break;
    case ifc_ExprSort_This:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::This", &error_position);
      break;
    case ifc_ExprSort_TemplateReference:
      { an_ifc_ExprSort_TemplateReference iestr, *iestrp;
        iestrp = get_ExprSort_TemplateReference(&iestr);
        source_position_from_locus(&pos, &iestrp->locus);
        cache_type(cache, iestrp->scope, &iestrp->locus);
        cache_token(cache, tok_colon_colon, &pos);
        cache_identifier(cache, string_from_name_index(iestrp->member_name,
                                                       /*loc=*/NULL),
                         &pos);
        if (iestrp->arguments != 0) {
          cache_token(cache, tok_lt, &pos);
          cache_expr(cache, iestrp->arguments);
          cache_token(cache, tok_gt, &pos);
        }  /* if */
      }
      break;
    case ifc_ExprSort_PushState:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::PushState", &error_position);
      break;
    case ifc_ExprSort_TypeTraitIntrinsic:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::TypeTraitIntrinsic",
                                  &error_position);
      break;
    case ifc_ExprSort_DesignatedInitializer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::DesignatedInitializer",
                                  &error_position);
      break;
    case ifc_ExprSort_PackedTemplateArguments:
      { an_ifc_ExprSort_PackedTemplateArguments iespta, *iesptap;
        iesptap = get_ExprSort_PackedTemplateArguments(&iespta);
        cache_expr(cache, iesptap->arguments);
      }
      break;
    case ifc_ExprSort_Tokens:
      { an_ifc_ExprSort_Tokens iest, *iestp;
        iestp = get_ExprSort_Tokens(&iest);
        cache_sentence(cache, iestp->words);
      }
      break;
    case ifc_ExprSort_AssignInitializer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::AssignInitializer",
                                  &error_position);
      break;
    case ifc_ExprSort_UnusedSort0:
    case ifc_ExprSort_UnusedSort1:
    case ifc_ExprSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unknown ExprSort");
  }  /* switch */
}  /* cache_expr */


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
/* Remove ARG_UNUSED as well. */
void an_ifc_module::cache_syntax(ARG_UNUSED a_token_cache_ptr cache,
                                 ifc_SyntaxIndex              syntax)
/*
Add the tokens corresponding to the given syntax tree to cache.
*/
{
  ifc_SyntaxSort    tag = syntax_tag(syntax);
  a_source_position pos;

  read_prechecked_partition_element(syntax);
  switch (tag) {
    case ifc_SyntaxSort_VendorExtension:
      issue_unsupported_node_diag("SyntaxSort::VendorExtension",
                                  &error_position);
      break;
    case ifc_SyntaxSort_SimpleTypeSpecifier:
      { an_ifc_SyntaxSort_SimpleTypeSpecifier isssts, *issstsp;
        issstsp = get_SyntaxSort_SimpleTypeSpecifier(&isssts);
        if (issstsp->type != 0) {
          check_assertion(issstsp->expr == 0);
          cache_type(cache, issstsp->type, &issstsp->locus);
        } else {
          check_assertion(issstsp->expr != 0);
          cache_expr(cache, issstsp->expr);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_DecltypeSpecifier:
      { an_ifc_SyntaxSort_DecltypeSpecifier issds, *issdsp;
        issdsp = get_SyntaxSort_DecltypeSpecifier(&issds);
        source_position_from_locus(&pos, &issdsp->decltype_keyword);
        cache_token(cache, tok_decltype, &pos);
        cache_token(cache, tok_lparen, &pos);
        cache_expr(cache, issdsp->expr);
        cache_token(cache, tok_rparen, &pos);
      }
      break;
    case ifc_SyntaxSort_PlaceholderTypeSpecifier:
      { an_ifc_SyntaxSort_PlaceholderTypeSpecifier isspts, *issptsp;
        issptsp = get_SyntaxSort_PlaceholderTypeSpecifier(&isspts);
        source_position_from_locus(&pos, &issptsp->locus);
        /* FIXME: Handle the constraint field. */
        if (issptsp->basis == ifc_TypeBasis_Auto) {
          cache_token(cache, tok_auto, &pos);
        } else {
          check_assertion(issptsp->basis == ifc_TypeBasis_DecltypeAuto);
          cache_token(cache, tok_decltype, &pos);
          cache_token(cache, tok_lparen, &pos);
          cache_token(cache, tok_auto, &pos);
          cache_token(cache, tok_rparen, &pos);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_TypeSpecifierSeq:
      { an_ifc_SyntaxSort_TypeSpecifierSeq isstss, *isstssp;
        isstssp = get_SyntaxSort_TypeSpecifierSeq(&isstss);
        source_position_from_locus(&pos, &isstssp->locus);
        cache_qualifiers(cache, isstssp->qualifiers, &pos);
        if (isstssp->type != 0) {
          check_assertion(isstssp->type_name == 0);
          cache_type(cache, isstssp->type, &isstssp->locus);
        } else {
          check_assertion(isstssp->type_name != 0);
          cache_syntax(cache, isstssp->type_name);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_DeclSpecifierSeq:
      { an_ifc_SyntaxSort_DeclSpecifierSeq issdss, *issdssp;
        issdssp = get_SyntaxSort_DeclSpecifierSeq(&issdss);
        source_position_from_locus(&pos, &issdssp->locus);
        /* FIXME: Handle storage_class field. */
        if (issdssp->declspec != 0) {
          cache_sentence(cache, issdssp->declspec);
        }  /* if */
        if (issdssp->explicit_kw != 0) {
          cache_syntax(cache, issdssp->explicit_kw);
        }  /* if */
        cache_qualifiers(cache, issdssp->qualifiers, &pos);
        if (issdssp->type != 0) {
          check_assertion(issdssp->type_name == 0);
          cache_type(cache, issdssp->type, &issdssp->locus);
        } else {
          check_assertion(issdssp->type_name != 0);
          cache_syntax(cache, issdssp->type_name);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_VirtualSpecifierSeq:
      { an_ifc_SyntaxSort_VirtualSpecifierSeq issvss, *issvssp;
        issvssp = get_SyntaxSort_VirtualSpecifierSeq(&issvss);
        source_position_from_locus(&pos, &issvssp->locus);
        if (issvssp->override_kw.line != 0) {
          cache_token(cache, tok_override, &pos);
        }  /* if */
        if (issvssp->final_kw.line != 0) {
          cache_token(cache, tok_final, &pos);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_NoexceptSpecification:
      { an_ifc_SyntaxSort_NoexceptSpecification issns, *issnsp;
        issnsp = get_SyntaxSort_NoexceptSpecification(&issns);
        source_position_from_locus(&pos, &issnsp->locus);
        cache_token(cache, tok_noexcept, &pos);
        cache_token(cache, tok_lparen, &pos);
        cache_syntax(cache, issnsp->expr);
        cache_token(cache, tok_rparen, &pos);
      }
      break;
    case ifc_SyntaxSort_ExplicitSpecifier:
      { an_ifc_SyntaxSort_ExplicitSpecifier isses, *issesp;
        issesp = get_SyntaxSort_ExplicitSpecifier(&isses);
        source_position_from_locus(&pos, &issesp->locus);
        cache_token(cache, tok_explicit, &pos);
        if (issesp->condition != 0) {
          cache_token(cache, tok_lparen, &pos);
          cache_expr(cache, issesp->condition);
          cache_token(cache, tok_rparen, &pos);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_EnumSpecifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::EnumSpecifier",
                                  &error_position);
      break;
    case ifc_SyntaxSort_EnumeratorDefinition:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::EnumeratorDefinition",
                                  &error_position);
      break;
    case ifc_SyntaxSort_ClassSpecifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::ClassSpecifier",
                                  &error_position);
      break;
    case ifc_SyntaxSort_MemberSpecification:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::MemberSpecification",
                                  &error_position);
      break;
    case ifc_SyntaxSort_MemberDeclaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::MemberDeclaration",
                                  &error_position);
      break;
    case ifc_SyntaxSort_MemberDeclarator:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::MemberDeclarator",
                                  &error_position);
      break;
    case ifc_SyntaxSort_AccessSpecifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::AccessSpecifier",
                                  &error_position);
      break;
    case ifc_SyntaxSort_BaseSpecifierList:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::BaseSpecifierList",
                                  &error_position);
      break;
    case ifc_SyntaxSort_BaseSpecifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::BaseSpecifier",
                                  &error_position);
      break;
    case ifc_SyntaxSort_TypeId:
      { an_ifc_SyntaxSort_TypeId issti, *isstip;
        isstip = get_SyntaxSort_TypeId(&issti);
        cache_syntax(cache, isstip->type_specifier);
        if (isstip->abstract_declarator != 0) {
          cache_syntax(cache, isstip->abstract_declarator);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_TrailingReturnType:
      { an_ifc_SyntaxSort_TrailingReturnType isstrt, *isstrtp;
        isstrtp = get_SyntaxSort_TrailingReturnType(&isstrt);
        source_position_from_locus(&pos, &isstrtp->arrow);
        cache_token(cache, tok_arrow, &pos);
        cache_syntax(cache, isstrtp->target);
      }
      break;
    case ifc_SyntaxSort_Declarator:
      { an_ifc_SyntaxSort_Declarator issd, *issdp;
        issdp = get_SyntaxSort_Declarator(&issd);
        source_position_from_locus(&pos, &issdp->locus);
        if (issdp->convention != 0) {
          cache_calling_convention(cache, issdp->convention, &pos);
        }  /* if */
        if (issdp->pointer != 0) {
          cache_syntax(cache, issdp->pointer);
        } else if (issdp->parenthesized != 0) {
          cache_token(cache, tok_lparen, &pos);
          cache_syntax(cache, issdp->parenthesized);
          cache_token(cache, tok_rparen, &pos);
        } else if (issdp->array_or_function != 0) {
          cache_syntax(cache, issdp->array_or_function);
        }  /* if */
        cache_qualifiers(cache, issdp->qualifiers, &pos);
        if (issdp->virtual_specifiers != 0) {
          cache_syntax(cache, issdp->virtual_specifiers);
        }  /* if */
        if (issdp->name != 0) {
          /* FIXME: Confirm and ensure that the name is cached at the right
             location in the sequence of tokens. */
          cache_expr(cache, issdp->name);
        }  /* if */
        if (issdp->trailing_target != 0) {
          cache_syntax(cache, issdp->trailing_target);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_PointerDeclarator:
      { an_ifc_SyntaxSort_PointerDeclarator isspd, *isspdp;
        isspdp = get_SyntaxSort_PointerDeclarator(&isspd);
        source_position_from_locus(&pos, &isspdp->locus);
        cache_qualifiers(cache, isspdp->qualifiers, &pos);
        if (isspdp->convention != 0) {
          cache_calling_convention(cache, isspdp->convention, &pos);
        }  /* if */
        switch (isspdp->sort) {
          case ifc_PointerDeclaratorSort_None:
            unexpected_condition();
            break;
          case ifc_PointerDeclaratorSort_Pointer:
            cache_token(cache, tok_star, &pos);
            break;
          case ifc_PointerDeclaratorSort_LvalueReference:
            cache_token(cache, tok_ampersand, &pos);
            break;
          case ifc_PointerDeclaratorSort_RvalueReference:
            cache_token(cache, tok_and_and, &pos);
            break;
          case ifc_PointerDeclaratorSort_PointerToMember:
            check_assertion(isspdp->whole != 0);
            cache_syntax(cache, isspdp->whole);
            cache_token(cache, tok_colon_colon, &pos);
            cache_token(cache, tok_star, &pos);
            break;
          default_is_unexpected_str("Unexpected PointerDeclaratorSort");
        }  /* switch */
        if (isspdp->next != 0) {
          cache_syntax(cache, isspdp->next);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_ArrayDeclarator:
      { an_ifc_SyntaxSort_ArrayDeclarator issad, *issadp;
        issadp = get_SyntaxSort_ArrayDeclarator(&issad);
        source_position_from_locus(&pos, &issadp->left_bracket);
        cache_token(cache, tok_lbracket, &pos);
        if (issadp->bound != 0) {
          cache_expr(cache, issadp->bound);
        }  /* if */
        source_position_from_locus(&pos, &issadp->right_bracket);
        cache_token(cache, tok_rbracket, &pos);
      }
      break;
    case ifc_SyntaxSort_FunctionDeclarator:
      { an_ifc_SyntaxSort_FunctionDeclarator issfd, *issfdp;
        issfdp = get_SyntaxSort_FunctionDeclarator(&issfd);
        source_position_from_locus(&pos, &issfdp->left_paren);
        cache_token(cache, tok_lparen, &pos);
        if (issfdp->parameters != 0) {
          cache_syntax(cache, issfdp->parameters);
        }  /* if */
        source_position_from_locus(&pos, &issfdp->right_paren);
        cache_token(cache, tok_rparen, &pos);
        if (issfdp->eh_spec != 0) {
          cache_syntax(cache, issfdp->eh_spec);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_ArrayOrFunctionDeclarator:
      { an_ifc_SyntaxSort_ArrayOrFunctionDeclarator issafd, *issafdp;
        issafdp = get_SyntaxSort_ArrayOrFunctionDeclarator(&issafd);
        cache_syntax(cache, issafdp->declarator);
        if (issafdp->next != 0) {
          cache_syntax(cache, issafdp->next);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_ParameterDeclarator:
      { an_ifc_SyntaxSort_ParameterDeclarator isspd, *isspdp;
        isspdp = get_SyntaxSort_ParameterDeclarator(&isspd);
        source_position_from_locus(&pos, &isspdp->locus);
        if (isspdp->decl_specifiers != 0) {
          cache_syntax(cache, isspdp->decl_specifiers);
        }  /* if */
        cache_syntax(cache, isspdp->declarator);
        if (isspdp->default_expr != 0) {
          cache_token(cache, tok_eq, &pos);
          cache_expr(cache, isspdp->default_expr);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_InitDeclarator:
      { an_ifc_SyntaxSort_InitDeclarator issid, *issidp;
        issidp = get_SyntaxSort_InitDeclarator(&issid);
        cache_syntax(cache, issidp->declarator);
        /* FIXME: Handle the constraint field. */
        if (issidp->initializer != 0) {
          /* FIXME: Find a way to get a proper source position for this. */
          cache_token(cache, tok_eq, &null_source_position);
          cache_expr(cache, issidp->initializer);
        }  /* if */
        if (issidp->comma.line != 0) {
          source_position_from_locus(&pos, &issidp->comma);
          cache_token(cache, tok_comma, &pos);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_NewDeclarator:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::NewDeclarator",
                                  &error_position);
      break;
    case ifc_SyntaxSort_SimpleDeclaration:
      { an_ifc_SyntaxSort_SimpleDeclaration isssd, *isssdp;
        isssdp = get_SyntaxSort_SimpleDeclaration(&isssd);
        if (isssdp->decl_specifiers != 0) {
          cache_syntax(cache, isssdp->decl_specifiers);
        }  /* if */
        cache_syntax(cache, isssdp->declarators);
        source_position_from_locus(&pos, &isssdp->semicolon);
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_SyntaxSort_ExceptionDeclaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::ExceptionDeclaration",
                                  &error_position);
      break;
    case ifc_SyntaxSort_ConditionDeclaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::ConditionDeclaration",
                                  &error_position);
      break;
    case ifc_SyntaxSort_StaticAssertDeclaration:
      { an_ifc_SyntaxSort_StaticAssertDeclaration isssad, *isssadp;
        isssadp = get_SyntaxSort_StaticAssertDeclaration(&isssad);
        source_position_from_locus(&pos, &isssadp->locus);
        cache_token(cache, tok_static_assert, &pos);
        cache_token(cache, tok_lparen, &pos);
        cache_expr(cache, isssadp->condition);
        if (isssadp->message != 0) {
          cache_token(cache, tok_comma, &pos);
          cache_expr(cache, isssadp->message);
        }  /* if */
        cache_token(cache, tok_rparen, &pos);
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_SyntaxSort_AliasDeclaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::AliasDeclaration",
                                  &error_position);
      break;
    case ifc_SyntaxSort_ConceptDefinition:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::ConceptDefinition",
                                  &error_position);
      break;
    case ifc_SyntaxSort_CompoundStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::CompoundStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_ReturnStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::ReturnStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_IfStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::IfStatement", &error_position);
      break;
    case ifc_SyntaxSort_WhileStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::WhileStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_DoWhileStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::DoWhileStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_ForStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::ForStatement", &error_position);
      break;
    case ifc_SyntaxSort_InitStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::InitStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_RangeBasedForStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::RangeBasedForStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_ForRangeDeclaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::ForRangeDeclaration",
                                  &error_position);
      break;
    case ifc_SyntaxSort_LabeledStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::LabeledStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_BreakStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::BreakStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_ContinueStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::ContinueStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_SwitchStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::SwitchStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_GotoStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::GotoStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_DeclarationStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::DeclarationStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_ExpressionStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::ExpressionStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_TryBlock:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::TryBlock", &error_position);
      break;
    case ifc_SyntaxSort_Handler:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::Handler", &error_position);
      break;
    case ifc_SyntaxSort_HandlerSeq:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::HandlerSeq", &error_position);
      break;
    case ifc_SyntaxSort_FunctionTryBlock:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::FunctionTryBlock",
                                  &error_position);
      break;
    case ifc_SyntaxSort_TypeIdListElement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::TypeIdListElement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_DynamicExceptionSpec:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::DynamicExceptionSpec",
                                  &error_position);
      break;
    case ifc_SyntaxSort_StatementSeq:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::StatementSeq", &error_position);
      break;
    case ifc_SyntaxSort_FunctionBody:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::FunctionBody", &error_position);
      break;
    case ifc_SyntaxSort_Expression:
      { an_ifc_SyntaxSort_Expression isse, *issep;
        issep = get_SyntaxSort_Expression(&isse);
        cache_expr(cache, issep->expression);
      }
      break;
    case ifc_SyntaxSort_FunctionDefinition:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::FunctionDefinition",
                                  &error_position);
      break;
    case ifc_SyntaxSort_MemberFunctionDeclaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::MemberFunctionDeclaration",
                                  &error_position);
      break;
    case ifc_SyntaxSort_TemplateDeclaration:
      { an_ifc_SyntaxSort_TemplateDeclaration isstd, *isstdp;
        isstdp = get_SyntaxSort_TemplateDeclaration(&isstd);
        source_position_from_locus(&pos, &isstdp->locus);
        cache_token(cache, tok_template, &pos);
        cache_syntax(cache, isstdp->parameters);
        cache_syntax(cache, isstdp->subject);
      }
      break;
    case ifc_SyntaxSort_RequiresClause:
      { an_ifc_SyntaxSort_RequiresClause issrc, *issrcp;
        issrcp = get_SyntaxSort_RequiresClause(&issrc);
        source_position_from_locus(&pos, &issrcp->locus);
        cache_token(cache, tok_requires, &pos);
        cache_expr(cache, issrcp->condition);
      }
      break;
    case ifc_SyntaxSort_SimpleRequirement:
      { an_ifc_SyntaxSort_SimpleRequirement isssr, *isssrp;
        isssrp = get_SyntaxSort_SimpleRequirement(&isssr);
        source_position_from_locus(&pos, &isssrp->locus);
        cache_expr(cache, isssrp->condition);
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_SyntaxSort_TypeRequirement:
      { an_ifc_SyntaxSort_TypeRequirement isstr, *isstrp;
        isstrp = get_SyntaxSort_TypeRequirement(&isstr);
        source_position_from_locus(&pos, &isstrp->locus);
        cache_expr(cache, isstrp->type);
        cache_token(cache, tok_semicolon, &pos);
      }
      break;
    case ifc_SyntaxSort_CompoundRequirement:
      { an_ifc_SyntaxSort_CompoundRequirement isscr, *isscrp;
        isscrp = get_SyntaxSort_CompoundRequirement(&isscr);
        source_position_from_locus(&pos, &isscrp->locus);
        cache_token(cache, tok_lbrace, &pos);
        cache_expr(cache, isscrp->condition);
        source_position_from_locus(&pos, &isscrp->right_curly);
        cache_token(cache, tok_rbrace, &pos);
        if (isscrp->noexcept_loc.line != 0) {
          source_position_from_locus(&pos, &isscrp->noexcept_loc);
          cache_token(cache, tok_noexcept, &pos);
        }  /* if */
        cache_token(cache, tok_arrow, &pos);
        cache_expr(cache, isscrp->constraint);
      }
      break;
    case ifc_SyntaxSort_NestedRequirement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::NestedRequirement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_RequirementBody:
      { an_ifc_SyntaxSort_RequirementBody issrb, *issrbp;
        issrbp = get_SyntaxSort_RequirementBody(&issrb);
        source_position_from_locus(&pos, &issrbp->locus);
        cache_token(cache, tok_lbrace, &pos);
        cache_syntax(cache, issrbp->requirements);
        source_position_from_locus(&pos, &issrbp->right_curly);
        cache_token(cache, tok_rbrace, &pos);
      }
      break;
    case ifc_SyntaxSort_TypeTemplateParameter:
      { an_ifc_SyntaxSort_TypeTemplateParameter issttp, *issttpp;
        issttpp = get_SyntaxSort_TypeTemplateParameter(&issttp);
        source_position_from_locus(&pos, &issttpp->locus);
        cache_token(cache, tok_typename, &pos);
        if (issttpp->ellipsis.line != 0) {
          cache_token(cache, tok_ellipsis, &pos);
        }  /* if */
        cache_identifier(cache, get_string_at_offset(issttpp->name), &pos);
        /* FIXME: Handle constraint field. */
        if (issttpp->argument != 0) {
          cache_token(cache, tok_eq, &pos);
          cache_syntax(cache, issttpp->argument);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_TemplateTemplateParameter:
      { an_ifc_SyntaxSort_TemplateTemplateParameter issttp, *issttpp;
        issttpp = get_SyntaxSort_TemplateTemplateParameter(&issttp);
        source_position_from_locus(&pos, &issttpp->locus);
        cache_token(cache, tok_template, &pos);
        cache_syntax(cache, issttpp->parameters);
        cache_token(cache, tok_gt, &pos);
        if (issttpp->ellipsis.line != 0) {
          cache_token(cache, tok_ellipsis, &pos);
        }  /* if */
        cache_identifier(cache, get_string_at_offset(issttpp->name), &pos);
        if (issttpp->argument != 0) {
          cache_token(cache, tok_eq, &pos);
          cache_syntax(cache, issttpp->argument);
        }  /* if */
        if (issttpp->comma.line != 0) {
          cache_token(cache, tok_comma, &pos);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_TypeTemplateArgument:
      { an_ifc_SyntaxSort_TypeTemplateArgument isstta, *issttap;
        issttap = get_SyntaxSort_TypeTemplateArgument(&isstta);
        cache_syntax(cache, issttap->argument);
        if (issttap->ellipsis.line != 0) {
          source_position_from_locus(&pos, &issttap->ellipsis);
          cache_token(cache, tok_ellipsis, &pos);
        }  /* if */
        if (issttap->comma.line != 0) {
          source_position_from_locus(&pos, &issttap->comma);
          cache_token(cache, tok_comma, &pos);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_NonTypeTemplateArgument:
      { an_ifc_SyntaxSort_NonTypeTemplateArgument issntta, *issnttap;
        issnttap = get_SyntaxSort_NonTypeTemplateArgument(&issntta);
        cache_expr(cache, issnttap->argument);
        if (issnttap->ellipsis.line != 0) {
          source_position_from_locus(&pos, &issnttap->ellipsis);
          cache_token(cache, tok_ellipsis, &pos);
        }  /* if */
        if (issnttap->comma.line != 0) {
          source_position_from_locus(&pos, &issnttap->comma);
          cache_token(cache, tok_comma, &pos);
        }  /* if */
      }
      break;
    case ifc_SyntaxSort_TemplateParameterList:
      { an_ifc_SyntaxSort_TemplateParameterList isstpl, *isstplp;
        isstplp = get_SyntaxSort_TemplateParameterList(&isstpl);
        source_position_from_locus(&pos, &isstplp->left_angle);
        cache_token(cache, tok_lt, &pos);
        cache_syntax(cache, isstplp->parameters);
        /* FIXME: Handle the "clause" field. */
        source_position_from_locus(&pos, &isstplp->right_angle);
        cache_token(cache, tok_gt, &pos);
      }
      break;
    case ifc_SyntaxSort_TemplateArgumentList:
      { an_ifc_SyntaxSort_TemplateArgumentList isstal, *isstalp;
        isstalp = get_SyntaxSort_TemplateArgumentList(&isstal);
        source_position_from_locus(&pos, &isstalp->left_angle);
        cache_token(cache, tok_lt, &pos);
        cache_syntax(cache, isstalp->arguments);
        /* FIXME: Handle the "clause" field. */
        source_position_from_locus(&pos, &isstalp->right_angle);
        cache_token(cache, tok_gt, &pos);
      }
      break;
    case ifc_SyntaxSort_TemplateId:
      { an_ifc_SyntaxSort_TemplateId issti, *isstip;
        isstip = get_SyntaxSort_TemplateId(&issti);
        source_position_from_locus(&pos, &isstip->locus);
        if (isstip->template_kw.line != 0) {
          cache_token(cache, tok_template, &pos);
        }  /* if */
        if (isstip->name != 0) {
          cache_syntax(cache, isstip->name);
        } else {
          check_assertion(isstip->symbol != 0);
          cache_expr(cache, isstip->symbol);
        }  /* if */
        cache_syntax(cache, isstip->arguments);
      }
      break;
    case ifc_SyntaxSort_MemInitializer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::MemInitializer",
                                  &error_position);
      break;
    case ifc_SyntaxSort_CtorInitializer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::CtorInitializer",
                                  &error_position);
      break;
    case ifc_SyntaxSort_LambdaIntroducer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::LambdaIntroducer",
                                  &error_position);
      break;
    case ifc_SyntaxSort_LambdaDeclarator:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::LambdaDeclarator",
                                  &error_position);
      break;
    case ifc_SyntaxSort_CaptureDefault:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::CaptureDefault",
                                  &error_position);
      break;
    case ifc_SyntaxSort_SimpleCapture:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::SimpleCapture",
                                  &error_position);
      break;
    case ifc_SyntaxSort_InitCapture:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::InitCapture", &error_position);
      break;
    case ifc_SyntaxSort_ThisCapture:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::ThisCapture", &error_position);
      break;
    case ifc_SyntaxSort_AttributedStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::AttributedStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_AttributedDeclaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::AttributedDeclaration",
                                  &error_position);
      break;
    case ifc_SyntaxSort_AttributeSpecifierSeq:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::AttributeSpecifierSeq",
                                  &error_position);
      break;
    case ifc_SyntaxSort_AttributeSpecifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::AttributeSpecifier",
                                  &error_position);
      break;
    case ifc_SyntaxSort_AttributeUsingPrefix:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::AttributeUsingPrefix",
                                  &error_position);
      break;
    case ifc_SyntaxSort_Attribute:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::Attribute", &error_position);
      break;
    case ifc_SyntaxSort_AttributeArgumentClause:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::AttributeArgumentClause",
                                  &error_position);
      break;
    case ifc_SyntaxSort_Alignas:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::Alignas", &error_position);
      break;
    case ifc_SyntaxSort_UsingDeclaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::UsingDeclaration",
                                  &error_position);
      break;
    case ifc_SyntaxSort_UsingDeclarator:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::UsingDeclarator",
                                  &error_position);
      break;
    case ifc_SyntaxSort_UsingDirective:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::UsingDirective",
                                  &error_position);
      break;
    case ifc_SyntaxSort_ArrayIndex:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::ArrayIndex", &error_position);
      break;
    case ifc_SyntaxSort_SEHTry:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::SEHTry", &error_position);
      break;
    case ifc_SyntaxSort_SEHExcept:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::SEHExcept", &error_position);
      break;
    case ifc_SyntaxSort_SEHFinally:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::SEHFinally", &error_position);
      break;
    case ifc_SyntaxSort_SEHLeave:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::SEHLeave", &error_position);
      break;
    case ifc_SyntaxSort_TypeTraitIntrinsic:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::TypeTraitIntrinsic",
                                  &error_position);
      break;
    case ifc_SyntaxSort_Tuple:
      { an_ifc_SyntaxSort_Tuple isst, *isstp;
        isstp = get_SyntaxSort_Tuple(&isst);
        uint32_t  k, N = (uint32_t)isstp->cardinality;
        for (k = 0; k<N; ++k) {
          ifc_SyntaxIndex si;
          read_prechecked_partition_element(ifc_heap_syn, isstp->start+k);
          GET_SyntaxIndex(si, /*from_header=*/FALSE);
          cache_syntax(cache, si);
        }  /* for */
      }
      break;
    case ifc_SyntaxSort_AsmStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::AsmStatement", &error_position);
      break;
    case ifc_SyntaxSort_NamespaceAliasDefinition:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::NamespaceAliasDefinition",
                                  &error_position);
      break;
    case ifc_SyntaxSort_Super:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::Super", &error_position);
      break;
    case ifc_SyntaxSort_UnaryFoldExpression:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::UnaryFoldExpression",
                                  &error_position);
      break;
    case ifc_SyntaxSort_BinaryFoldExpression:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::BinaryFoldExpression",
                                  &error_position);
      break;
    case ifc_SyntaxSort_EmptyStatement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::EmptyStatement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_StructuredBindingDeclaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::StructuredBindingDeclaration",
                                  &error_position);
      break;
    case ifc_SyntaxSort_StructuredBindingIdentifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::StructuredBindingIdentifier",
                                  &error_position);
      break;
    case ifc_SyntaxSort_UsingEnumDeclaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::UsingEnumDeclaration",
                                  &error_position);
      break;
    case ifc_SyntaxSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected SyntaxSort");
  }  /* switch */
}  /* cache_syntax */


void an_ifc_module::cache_name(a_token_cache_ptr  cache,
                               ifc_NameIndex      name,
                               ifc_SourceLocation *locus)
/*
Add the tokens corresponding to the given name to cache.  locus is the location
of the name.
*/
{
  ifc_NameSort      tag = name_tag(name);
  ifc_TextOffset    ident;
  a_source_position pos;
  a_boolean         interpret_name_as_tokens = FALSE;

  source_position_from_locus(&pos, locus);
  if (tag != ifc_NameSort_Identifier) {
    read_prechecked_partition_element(name);
  }  /* if */
  switch (tag) {
    case ifc_NameSort_Identifier:
      /* NameSort::Identifiers just refer to the string table. */
      ident = (ifc_TextOffset)name_value(name);
      goto cache_ident;
    case ifc_NameSort_SourceFile:
      { an_ifc_NameSort_SourceFile inssf, *inssfp;
        inssfp = get_NameSort_SourceFile(&inssf);
        ident = inssfp->path;
        goto cache_ident;
      }
    case ifc_NameSort_Template:
      { an_ifc_NameSort_Template inst, *instp;
        instp = get_NameSort_Template(&inst);
        cache_token(cache, tok_template, &pos);
        cache_name(cache, instp->name, locus);
      }
      break;
    case ifc_NameSort_Specialization:
      { an_ifc_NameSort_Specialization inss, *inssp;
        inssp = get_NameSort_Specialization(&inss);
        cache_token(cache, tok_template, &pos);
        cache_name(cache, inssp->primary, locus);
        cache_token(cache, tok_lt, &pos);
        if (inssp->arguments != 0) {
          cache_expr(cache, inssp->arguments);
        }  /* if */
        cache_token(cache, tok_gt, &pos);
      }
      break;
    case ifc_NameSort_Operator:
      { an_ifc_NameSort_Operator inso, *insop;
        an_operator_kind         opkind;
        insop = get_NameSort_Operator(&inso);
        opkind = get_operator_kind(insop->op);
        if (opkind == opkind_c_cast || opkind == opkind_cpp_cast) {
          unexpected_condition_str("Unexpected operator kind");
        }  /* if */
        ident = insop->encoded;
        interpret_name_as_tokens = TRUE;
        goto cache_op;
      }
    case ifc_NameSort_Conversion:
      { an_ifc_NameSort_Conversion insc, *inscp;
        inscp = get_NameSort_Conversion(&insc);
        cache_token(cache, tok_operator, &pos);
        cache_type(cache, inscp->target, locus);
      }
      break;
    case ifc_NameSort_Literal:
      { an_ifc_NameSort_Literal insl, *inslp;
        inslp = get_NameSort_Literal(&insl);
        ident = inslp->encoded;
cache_op:
        cache_token(cache, tok_operator, &pos);
cache_ident:
        if (interpret_name_as_tokens) {
          cache_tokens_from_string(get_string_at_offset(ident), cache, &pos);
        } else {
          cache_identifier(cache, get_string_at_offset(ident), &pos);
        }  /* if */
      }
      break;
    case ifc_NameSort_Guide:
      { an_ifc_NameSort_Guide insg;
        get_NameSort_Guide(&insg);
        /* FIXME: Currently unsupported. */
        issue_unsupported_node_diag("NameSort::Guide", &error_position);
      }
      break;
    case ifc_NameSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
}  /* cache_name */


inline a_boolean an_ifc_module::should_cache_nested_name_specifier_for_scope(
                                                             a_scope_ptr scope)
/*
This function tests to see if the given scope and its parents should be cached
as part of the currently nested name specifier currently being cached.  Return
TRUE if the scope and its parents should be cached, FALSE otherwise.
*/
{
  a_boolean result = TRUE;

  if (scope == NULL) {
    result = FALSE;
  } else if (scope->kind == (a_scope_kind)sck_namespace &&
             suppress_automatic_namespace_qualification) {
    result = FALSE;
  }  /* if */
  return result;
}  /* should_cache_nested_name_specifier_for_scope */


void an_ifc_module::cache_scope_as_nested_name_specifier(
                                                   a_token_cache_ptr     cache,
                                                   a_scope_ptr           scope,
                                                   a_source_position_ptr pos)
/*
Add the tokens to cache representing a nested-name-specifier for scope.  pos is
the position of the qualified-id this nested-name-specifier is part of.
*/
{
  if (should_cache_nested_name_specifier_for_scope(scope)) {
    /* Attempt to generate any parent scope's qualifiers. */
    cache_scope_as_nested_name_specifier(cache, scope->parent, pos);
    /* Generate the current scope's qualifier. */
    if (scope_is(scope, sck_class_struct_union) ||
        scope_is(scope, sck_enum)) {
      a_type_ptr type_ptr = scope->variant.assoc_type;
      check_assertion(type_ptr != NULL);
      cache_identifier(cache, type_ptr->source_corresp.name, pos);
      cache_token(cache, tok_colon_colon, pos);
    } else if (scope->kind == (a_scope_kind)sck_namespace) {
      a_namespace_ptr namespace_ptr = scope->variant.assoc_namespace;
      cache_identifier(cache, namespace_ptr->source_corresp.name, pos);
      cache_token(cache, tok_colon_colon, pos);
    }  /* if */
  }  /* if */
}  /* cache_scope_as_nested_name_specifier */


void an_ifc_module::cache_nested_name_specifier_from_decl(
                                                   a_token_cache_ptr     cache,
                                                   ifc_DeclIndex         decl,
                                                   a_source_position_ptr pos)
/*
Add the tokens corresponding to the given declaration's (decl)
nested-name-specifier to cache.  pos is the position of the qualified-id this
nested-name-specifier is part of.
*/
{
  a_module_entity_ptr mep = get_ifc_module_entity_ptr(decl);
  if (mep->scope == NULL) {
    /* Load the module entity pointer scope if not already processed. */
    mep->scope = get_ifc_home_scope(decl);
  }  /* if */
  cache_scope_as_nested_name_specifier(cache, mep->scope, pos);
}  /* cache_nested_name_specifier_from_decl */


void an_ifc_module::cache_qualified_name_from_decl(a_token_cache_ptr  cache,
                                                   ifc_DeclIndex      decl,
                                                   ifc_SourceLocation *locus)
/*
Add the tokens corresponding to the given declaration's (decl) qualified-id to
cache.  locus is the location of the name use.
*/
{
  an_ifc_Ref<ifc_DeclIndex> decl_ref(this, decl);
  a_source_position         pos;

  source_position_from_locus(&pos, locus);
  cache_nested_name_specifier_from_decl(cache, decl, &pos);
  cache_identifier(cache, name_from_decl(decl_ref), &pos);
}  /* cache_qualified_name_from_decl */


void an_ifc_module::cache_name_from_decl(a_token_cache_ptr  cache,
                                         ifc_DeclIndex      decl,
                                         ifc_SourceLocation *locus)
/*
Add the tokens corresponding to the given declaration's (decl) name to cache.
locus is the location of the name use.
*/
{
  an_ifc_Ref<ifc_DeclIndex> decl_ref(this, decl);
  a_source_position         pos;

  source_position_from_locus(&pos, locus);
  cache_identifier(cache, name_from_decl(decl_ref), &pos);
}  /* cache_name_from_decl */


void an_ifc_module::cache_macro(a_token_cache_ptr cache,
                                ifc_MacroIndex    macro)
/*
Add the tokens corresponding to the given macro's definition to cache.
*/
{
  ifc_MacroSort     tag = macro_tag(macro);
  a_source_position pos;

  read_prechecked_partition_element(macro);
  switch (tag) {
    case ifc_MacroSort_ObjectLike:
      { an_ifc_MacroSort_ObjectLike imsol, *imsolp;
        imsolp = get_MacroSort_ObjectLike(&imsol);
        source_position_from_locus(&pos, &imsolp->locus);
        cache_identifier(cache, get_string_at_offset(imsolp->name), &pos);
        /* Ensure there's a space between the macro identifier and the macro
           body. */
        cache_pp_token(cache, " ", 1, &pos);
        cache_form(cache, imsolp->body);
      }
      break;
    case ifc_MacroSort_FunctionLike:
      { an_ifc_MacroSort_FunctionLike imsfl, *imsflp;
        imsflp = get_MacroSort_FunctionLike(&imsfl);
        source_position_from_locus(&pos, &imsflp->locus);
        cache_identifier(cache, get_string_at_offset(imsflp->name), &pos);
        cache_token(cache, tok_lparen, &pos);
        if (func_macro_is_variadic(imsflp)) {
          cache_token(cache, tok_ellipsis, &pos);
        } else {
          check_assertion(imsflp->parameters != 0);
          cache_form(cache, imsflp->parameters, /*is_parameter_form=*/TRUE);
        }  /* if */
        cache_token(cache, tok_rparen, &pos);
        /* Ensure there's a space between the macro parameter list and the
           macro body. */
        cache_pp_token(cache, " ", 1, &pos);
        cache_form(cache, imsflp->body);
      }
      break;
    case ifc_MacroSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected MacroSort");
  }  /* switch */
}  /* cache_macro */


void an_ifc_module::cache_form(a_token_cache_ptr cache,
                               ifc_FormIndex     form,
                               a_boolean         is_parameter_form)
/*
Add tokens corresponding to the given preprocessing "form" to cache.  If this
is for a function-like macro's parameters then is_parameter_form is TRUE,
otherwise is_parameter_form is FALSE.

FIXME: Many of these see non-identifiers cached as identifiers due to the
raw-text spelling.
*/
{
  ifc_FormSort       tag = form_tag(form);
  a_source_position  pos;
  ifc_SourceLocation *locus;
  ifc_TextOffset     spelling;

  read_prechecked_partition_element(form);
  switch (tag) {
    case ifc_FormSort_Identifier:
      { an_ifc_FormSort_Identifier ifsi, *ifsip;
        ifsip = get_FormSort_Identifier(&ifsi);
        source_position_from_locus(&pos, &ifsip->locus);
        cache_identifier(cache, get_string_at_offset(ifsip->spelling), &pos);
      }
      break;
    case ifc_FormSort_Number:
      { an_ifc_FormSort_Number ifsn, *ifsnp;
        ifsnp = get_FormSort_Number(&ifsn);
        locus = &ifsnp->locus;
        spelling = ifsnp->spelling;
        goto cache_spelling;
      }
      /*break;*/
    case ifc_FormSort_Character:
      { an_ifc_FormSort_Character ifsc, *ifscp;
        ifscp = get_FormSort_Character(&ifsc);
        locus = &ifscp->locus;
        spelling = ifscp->spelling;
        goto cache_spelling;
      }
      /*break;*/
    case ifc_FormSort_String:
      { an_ifc_FormSort_String ifss, *ifssp;
        ifssp = get_FormSort_String(&ifss);
        locus = &ifssp->locus;
        spelling = ifssp->spelling;
        goto cache_spelling;
      }
      /*break;*/
    case ifc_FormSort_Operator:
      { an_ifc_FormSort_Operator ifso, *ifsop;
        ifsop = get_FormSort_Operator(&ifso);
        locus = &ifsop->locus;
        spelling = ifsop->spelling;
        goto cache_spelling;
      }
      /*break;*/
    case ifc_FormSort_Keyword:
      { an_ifc_FormSort_Keyword ifsk, *ifskp;
        ifskp = get_FormSort_Keyword(&ifsk);
        locus = &ifskp->locus;
        spelling = ifskp->spelling;
        goto cache_spelling;
      }
      /*break;*/
    case ifc_FormSort_Parameter:
      { an_ifc_FormSort_Parameter ifsp, *ifspp;
        ifspp = get_FormSort_Parameter(&ifsp);
        locus = &ifspp->locus;
        spelling = ifspp->spelling;
        goto cache_spelling;
      }
      /*break;*/
    case ifc_FormSort_Header:
      { an_ifc_FormSort_Header ifsh, *ifshp;
        ifshp = get_FormSort_Header(&ifsh);
        locus = &ifshp->locus;
        spelling = ifshp->spelling;
        goto cache_spelling;
      }
      /*break;*/
    case ifc_FormSort_Junk:
      { an_ifc_FormSort_Junk ifsj, *ifsjp;
        ifsjp = get_FormSort_Junk(&ifsj);
        source_position_from_locus(&pos, &ifsjp->locus);
        locus = &ifsjp->locus;
        spelling = ifsjp->spelling;
cache_spelling:
        { a_const_char *str = get_string_at_offset(spelling);
          source_position_from_locus(&pos, locus);
          cache_pp_token(cache, str, strlen(str), &pos);
        }
      }
      break;
    case ifc_FormSort_Whitespace:
      { an_ifc_FormSort_Whitespace ifsw, *ifswp;
        ifswp = get_FormSort_Whitespace(&ifsw);
        source_position_from_locus(&pos, &ifswp->locus);
        /* FIXME: Ideally the whitespace information will contain the amount
           and kind of whitespace, and we'd cache that. */
        cache_pp_token(cache, " ", 1, &pos);
      }
      break;
    case ifc_FormSort_Stringize:
      { an_ifc_FormSort_Stringize ifss, *ifssp;
        ifssp = get_FormSort_Stringize(&ifss);
        source_position_from_locus(&pos, &ifssp->locus);
        cache_pp_token(cache, "#", /*len=*/1, &pos);
        cache_form(cache, ifssp->operand);
      }
      break;
    case ifc_FormSort_Catenate:
      { an_ifc_FormSort_Catenate ifsc, *ifscp;
        ifscp = get_FormSort_Catenate(&ifsc);
        source_position_from_locus(&pos, &ifscp->locus);
        cache_form(cache, ifscp->first);
        cache_pp_token(cache, "##", /*len=*/2, &pos);
        cache_form(cache, ifscp->second);
      }
      break;
    case ifc_FormSort_Pragma:
      { an_ifc_FormSort_Pragma ifsp, *ifspp;
        ifspp = get_FormSort_Pragma(&ifsp);
        source_position_from_locus(&pos, &ifspp->locus);
        cache_pp_token(cache, "_Pragma", /*len=*/7, &pos);
        cache_token(cache, tok_lparen, &pos);
        cache_form(cache, ifspp->operand);
        cache_token(cache, tok_rparen, &pos);
      }
      break;
    case ifc_FormSort_Parenthesized:
      { an_ifc_FormSort_Parenthesized ifsp, *ifspp;
        ifspp = get_FormSort_Parenthesized(&ifsp);
        source_position_from_locus(&pos, &ifspp->locus);
        cache_token(cache, tok_lparen, &pos);
        cache_form(cache, ifspp->operand);
        cache_token(cache, tok_rparen, &pos);
      }
      break;
    case ifc_FormSort_Tuple:
      { an_ifc_FormSort_Tuple ifst, *ifstp;
        ifstp = get_FormSort_Tuple(&ifst);
        for (uint32_t idx = 0; idx < ifstp->cardinality; ++idx) {
          ifc_FormIndex tform =
                       (ifc_FormIndex)read_index_from_heap(ifc_heap_pp,
                                                           ifstp->start + idx);
          if (idx > 0) {
            /* FIXME: Find a proper source position for these. */
            if (is_parameter_form) {
              cache_token(cache, tok_comma, &null_source_position);
            } else {
              /* FIXME: Currently IFC macros do not have whitespace encoded,
                 so add our own to ensure entities do not bleed together.
                 This is not always the correct thing to do, but there are
                 fewer issues than with not doing this at all. */
              cache_pp_token(cache, " ", 1, &null_source_position);
            }  /* if */
          }  /* if */
          cache_form(cache, tform);
        }  /* for */
      }
      break;
    case ifc_FormSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected FormSort");
  }  /* switch */
}  /* cache_form */


inline ifc_DeclIndex an_ifc_module::decl_index_of(
                                             an_ifc_partition_kind partition,
                                             size_t                file_offset)
                                                                          const
/*
Return the ifc_DeclIndex derived from the partition kind and file offset.
*/
{
  /* Compute the index into the partition by first subtracting the start of the
     partition in the file, producing "part_offset".  Then compute our index in
     the partition by dividing our offset by the size of entries in the
     partition.  Then adapt the EDG partition kind to the IFC partition
     encoding by subtracting the starting index.  Use the IFC partition
     information and the index value to form an ifc_DeclIndex. */
  size_t part_offset = file_offset - partitions[partition].offset;
  size_t part_index = part_offset / partitions[partition].entry_size;
  uint32_t ifc_partition = partition - ifc_decl_start;

  check_assertion(ifc_decl_start < partition && partition < ifc_decl_end);
  return make_decl_index(ifc_partition, part_index);
}  /* decl_index_of */


inline ifc_DeclIndex an_ifc_module::decl_index_of(a_module_entity_ptr mep)
                                                                          const
/*
Return the ifc_DeclIndex derived from the partition kind and file offset stored
on the given module entity pointer.
*/
{
  return decl_index_of(mep->variant.ifc_partition, mep->file_offset);
}  /* decl_index_of */


ifc_AttrIndex an_ifc_module::attr_index_of(ifc_DeclIndex decl_idx)
/*
Search the ".msvc.trait.vendor-traits" partition for any attribute associated
with a given ifc_DeclIndex (decl_idx).  If a matching attribute is found return
its ifc_AttrIndex.
*/
{
  size_t num_trait_attributes = get_num_entries(ifc_msvc_trait_decl_attrs);
  /* Provide a value function for retrieving the trait attribute at the
     given trait specialization partition index. */
  auto value_lambda = [this](ptrdiff_t idx) {
    an_ifc_Trait_MsvcDeclAttrs ita, *itap;

    read_prechecked_partition_element(ifc_msvc_trait_decl_attrs,
                                      (ifc_Index_type)idx);
    itap = get_Trait_MsvcDeclAttrs(&ita);
    return itap->decl;
  };
  /* Get the partition index (if any) for decl_idx. */
  ptrdiff_t partition_idx = bin_search(num_trait_attributes, decl_idx,
                                       value_lambda);
  /* Set up the return value, default to 0 which represents no result. */
  ifc_AttrIndex attr_idx = (ifc_AttrIndex)0;

  if (partition_idx != -1) {
    /* A trait attribute was found for decl_idx.  Load the trait attribute
       (again) to retrieve the trait.

       Note that the implementation of bin_search at the time of writing does
       not guarantee that the last read value is the one whose index is
       returned.  Thus, we cannot (as an optimization) share a variable with
       the value_lambda to prevent double reading (though this is unlikely to
       ever represent a significant cost in terms of CPU time). */
    an_ifc_Trait_MsvcDeclAttrs ita, *itap;

    read_prechecked_partition_element(ifc_msvc_trait_decl_attrs,
                                      (ifc_Index_type)partition_idx);
    itap = get_Trait_MsvcDeclAttrs(&ita);
    attr_idx = itap->trait;
  }  /* if */
  return attr_idx;
}  /* attr_index_of */


/* A CRT (curiously recursive template) visitor class for dispatching to a
   visit function that should be "overridden" by implementing derived
   classes. */
template<typename a_Derived_T>
struct an_ifc_module::Element_visitor {
  Element_visitor()
    {}

  template<typename an_ifc_Element_Type>
  inline void visit(an_ifc_partition_position pos) = delete;
  void visit_position(an_ifc_partition_position pos);
protected:
  inline auto getDerived() -> a_Derived_T *
    { return static_cast<a_Derived_T*>(this); }
};  /* Element_visitor */


template<typename a_Derived_T>
void an_ifc_module::Element_visitor<a_Derived_T>::visit_position(
                                                 an_ifc_partition_position pos)
/*
Dispatch to the visit function designated to handle the partition element of
the position's partition kind.
*/
{
  switch (pos.partition) {
    case ifc_invalid_partition:
      unexpected_condition_str("ifc_invalid_partition visited");
    /* DeclIndex::tag partitions together. */
    case ifc_decl_vendor_extension:
      getDerived()->template visit<an_ifc_DeclSort_VendorExtension>(pos);
      break;
    case ifc_decl_enumerator:
      getDerived()->template visit<an_ifc_DeclSort_Enumerator>(pos);
      break;
    case ifc_decl_variable:
      getDerived()->template visit<an_ifc_DeclSort_Variable>(pos);
      break;
    case ifc_decl_parameter:
      getDerived()->template visit<an_ifc_DeclSort_Parameter>(pos);
      break;
    case ifc_decl_field:
      getDerived()->template visit<an_ifc_DeclSort_Field>(pos);
      break;
    case ifc_decl_bitfield:
      getDerived()->template visit<an_ifc_DeclSort_Bitfield>(pos);
      break;
    case ifc_decl_scope:
      getDerived()->template visit<an_ifc_DeclSort_Scope>(pos);
      break;
    case ifc_decl_enumeration:
      getDerived()->template visit<an_ifc_DeclSort_Enumeration>(pos);
      break;
    case ifc_decl_alias:
      getDerived()->template visit<an_ifc_DeclSort_Alias>(pos);
      break;
    case ifc_decl_temploid:
      getDerived()->template visit<an_ifc_DeclSort_Temploid>(pos);
      break;
    case ifc_decl_template:
      getDerived()->template visit<an_ifc_DeclSort_Template>(pos);
      break;
    case ifc_decl_partial_specialization:
      getDerived()->template visit<an_ifc_DeclSort_PartialSpecialization>(pos);
      break;
    case ifc_decl_specialization:
      getDerived()->template visit<an_ifc_DeclSort_Specialization>(pos);
      break;
    case ifc_decl_concept:
      getDerived()->template visit<an_ifc_DeclSort_Concept>(pos);
      break;
    case ifc_decl_function:
      getDerived()->template visit<an_ifc_DeclSort_Function>(pos);
      break;
    case ifc_decl_method:
      getDerived()->template visit<an_ifc_DeclSort_Method>(pos);
      break;
    case ifc_decl_constructor:
      getDerived()->template visit<an_ifc_DeclSort_Constructor>(pos);
      break;
    case ifc_decl_inh_ctor:
      getDerived()->template visit<an_ifc_DeclSort_InheritedConstructor>(pos);
      break;
    case ifc_decl_destructor:
      getDerived()->template visit<an_ifc_DeclSort_Destructor>(pos);
      break;
    case ifc_decl_reference:
      getDerived()->template visit<an_ifc_DeclSort_Reference>(pos);
      break;
    case ifc_decl_using_declaration:
      getDerived()->template visit<an_ifc_DeclSort_UsingDeclaration>(pos);
      break;
    case ifc_decl_using_directive:
      getDerived()->template visit<an_ifc_DeclSort_UsingDirective>(pos);
      break;
    case ifc_decl_friend:
      getDerived()->template visit<an_ifc_DeclSort_Friend>(pos);
      break;
    case ifc_decl_expansion:
      getDerived()->template visit<an_ifc_DeclSort_Expansion>(pos);
      break;
    case ifc_decl_deduction_guide:
      getDerived()->template visit<an_ifc_DeclSort_DeductionGuide>(pos);
      break;
    case ifc_decl_barren:
      getDerived()->template visit<an_ifc_DeclSort_Barren>(pos);
      break;
    case ifc_decl_tuple:
      getDerived()->template visit<an_ifc_DeclSort_Tuple>(pos);
      break;
    case ifc_decl_syntax_tree:
      getDerived()->template visit<an_ifc_DeclSort_SyntaxTree>(pos);
      break;
    case ifc_decl_intrinsic:
      getDerived()->template visit<an_ifc_DeclSort_Intrinsic>(pos);
      break;
    case ifc_decl_property:
      getDerived()->template visit<an_ifc_DeclSort_Property>(pos);
      break;
    case ifc_decl_segment:
      getDerived()->template visit<an_ifc_DeclSort_OutputSegment>(pos);
      break;
    /* Group all TypeIndex::tag partitions together. */
    case ifc_type_vendor_extension:
      getDerived()->template visit<an_ifc_TypeSort_VendorExtension>(pos);
      break;
    case ifc_type_fundamental:
      getDerived()->template visit<an_ifc_TypeSort_Fundamental>(pos);
      break;
    case ifc_type_designated:
      getDerived()->template visit<an_ifc_TypeSort_Designated>(pos);
      break;
    case ifc_type_tor:
      getDerived()->template visit<an_ifc_TypeSort_Tor>(pos);
      break;
    case ifc_type_syntactic:
      getDerived()->template visit<an_ifc_TypeSort_Syntactic>(pos);
      break;
    case ifc_type_expansion:
      getDerived()->template visit<an_ifc_TypeSort_Expansion>(pos);
      break;
    case ifc_type_pointer:
      getDerived()->template visit<an_ifc_TypeSort_Pointer>(pos);
      break;
    case ifc_type_pointer_to_member:
      getDerived()->template visit<an_ifc_TypeSort_PointerToMember>(pos);
      break;
    case ifc_type_lvalue_reference:
      getDerived()->template visit<an_ifc_TypeSort_LvalueReference>(pos);
      break;
    case ifc_type_rvalue_reference:
      getDerived()->template visit<an_ifc_TypeSort_RvalueReference>(pos);
      break;
    case ifc_type_function:
      getDerived()->template visit<an_ifc_TypeSort_Function>(pos);
      break;
    case ifc_type_method:
      getDerived()->template visit<an_ifc_TypeSort_Method>(pos);
      break;
    case ifc_type_array:
      getDerived()->template visit<an_ifc_TypeSort_Array>(pos);
      break;
    case ifc_type_typename:
      getDerived()->template visit<an_ifc_TypeSort_Typename>(pos);
      break;
    case ifc_type_qualified:
      getDerived()->template visit<an_ifc_TypeSort_Qualified>(pos);
      break;
    case ifc_type_base:
      getDerived()->template visit<an_ifc_TypeSort_Base>(pos);
      break;
    case ifc_type_decltype:
      getDerived()->template visit<an_ifc_TypeSort_Decltype>(pos);
      break;
    case ifc_type_placeholder:
      getDerived()->template visit<an_ifc_TypeSort_Placeholder>(pos);
      break;
    case ifc_type_tuple:
      getDerived()->template visit<an_ifc_TypeSort_Tuple>(pos);
      break;
    case ifc_type_forall:
      getDerived()->template visit<an_ifc_TypeSort_Forall>(pos);
      break;
    case ifc_type_unaligned:
      getDerived()->template visit<an_ifc_TypeSort_Unaligned>(pos);
      break;
    case ifc_type_syntax_tree:
      getDerived()->template visit<an_ifc_TypeSort_SyntaxTree>(pos);
      break;
    /* Group all NameIndex::tag partitions together (excluding identifier at
       least for now, it's a special case). */
    case ifc_name_operator:
      getDerived()->template visit<an_ifc_NameSort_Operator>(pos);
      break;
    case ifc_name_conversion:
      getDerived()->template visit<an_ifc_NameSort_Conversion>(pos);
      break;
    case ifc_name_literal:
      getDerived()->template visit<an_ifc_NameSort_Literal>(pos);
      break;
    case ifc_name_template:
      getDerived()->template visit<an_ifc_NameSort_Template>(pos);
      break;
    case ifc_name_specialization:
      getDerived()->template visit<an_ifc_NameSort_Specialization>(pos);
      break;
    case ifc_name_source_file:
      getDerived()->template visit<an_ifc_NameSort_SourceFile>(pos);
      break;
    case ifc_name_guide:
      getDerived()->template visit<an_ifc_NameSort_Guide>(pos);
      break;
    /* Group all ExprIndex::tag partitions together. */
    case ifc_expr_vendor_extension:
      getDerived()->template visit<an_ifc_ExprSort_VendorExtension>(pos);
      break;
    case ifc_expr_empty:
      getDerived()->template visit<an_ifc_ExprSort_Empty>(pos);
      break;
    case ifc_expr_literal:
      getDerived()->template visit<an_ifc_ExprSort_Literal>(pos);
      break;
    case ifc_expr_lambda:
      getDerived()->template visit<an_ifc_ExprSort_Lambda>(pos);
      break;
    case ifc_expr_type:
      getDerived()->template visit<an_ifc_ExprSort_Type>(pos);
      break;
    case ifc_expr_decl:
      getDerived()->template visit<an_ifc_ExprSort_NamedDecl>(pos);
      break;
    case ifc_expr_unresolved_id:
      getDerived()->template visit<an_ifc_ExprSort_UnresolvedId>(pos);
      break;
    case ifc_expr_template_id:
      getDerived()->template visit<an_ifc_ExprSort_TemplateId>(pos);
      break;
    case ifc_expr_unqualified_id:
      getDerived()->template visit<an_ifc_ExprSort_UnqualifiedId>(pos);
      break;
    case ifc_expr_simple_identifier:
      getDerived()->template visit<an_ifc_ExprSort_SimpleIdentifier>(pos);
      break;
    case ifc_expr_pointer:
      getDerived()->template visit<an_ifc_ExprSort_Pointer>(pos);
      break;
    case ifc_expr_qualified_name:
      getDerived()->template visit<an_ifc_ExprSort_QualifiedName>(pos);
      break;
    case ifc_expr_path:
      getDerived()->template visit<an_ifc_ExprSort_Path>(pos);
      break;
    case ifc_expr_read:
      getDerived()->template visit<an_ifc_ExprSort_Read>(pos);
      break;
    case ifc_expr_monad:
      getDerived()->template visit<an_ifc_ExprSort_Monad>(pos);
      break;
    case ifc_expr_dyad:
      getDerived()->template visit<an_ifc_ExprSort_Dyad>(pos);
      break;
    case ifc_expr_triad:
      getDerived()->template visit<an_ifc_ExprSort_Triad>(pos);
      break;
    case ifc_expr_string:
      getDerived()->template visit<an_ifc_ExprSort_String>(pos);
      break;
    case ifc_expr_temporary:
      getDerived()->template visit<an_ifc_ExprSort_Temporary>(pos);
      break;
    case ifc_expr_call:
      getDerived()->template visit<an_ifc_ExprSort_Call>(pos);
      break;
    case ifc_expr_member_initializer:
      getDerived()->template visit<an_ifc_ExprSort_MemberInitializer>(pos);
      break;
    case ifc_expr_member_access:
      getDerived()->template visit<an_ifc_ExprSort_MemberAccess>(pos);
      break;
    case ifc_expr_inheritance_path:
      getDerived()->template visit<an_ifc_ExprSort_InheritancePath>(pos);
      break;
    case ifc_expr_initializer_list:
      getDerived()->template visit<an_ifc_ExprSort_InitializerList>(pos);
      break;
    case ifc_expr_cast:
      getDerived()->template visit<an_ifc_ExprSort_Cast>(pos);
      break;
    case ifc_expr_condition:
      getDerived()->template visit<an_ifc_ExprSort_Condition>(pos);
      break;
    case ifc_expr_expression_list:
      getDerived()->template visit<an_ifc_ExprSort_ExpressionList>(pos);
      break;
    case ifc_expr_sizeof_type:
      getDerived()->template visit<an_ifc_ExprSort_SizeofType>(pos);
      break;
    case ifc_expr_alignof_type:
      getDerived()->template visit<an_ifc_ExprSort_Alignof>(pos);
      break;
    case ifc_expr_typeid:
      getDerived()->template visit<an_ifc_ExprSort_Typeid>(pos);
      break;
    case ifc_expr_destructor_call:
      getDerived()->template visit<an_ifc_ExprSort_DestructorCall>(pos);
      break;
    case ifc_expr_syntax_tree:
      getDerived()->template visit<an_ifc_ExprSort_SyntaxTree>(pos);
      break;
    case ifc_expr_function_string:
      getDerived()->template visit<an_ifc_ExprSort_FunctionString>(pos);
      break;
    case ifc_expr_compound_string:
      getDerived()->template visit<an_ifc_ExprSort_CompoundString>(pos);
      break;
    case ifc_expr_string_sequence:
      getDerived()->template visit<an_ifc_ExprSort_StringSequence>(pos);
      break;
    case ifc_expr_initializer:
      getDerived()->template visit<an_ifc_ExprSort_Initializer>(pos);
      break;
    case ifc_expr_requires:
      getDerived()->template visit<an_ifc_ExprSort_Requires>(pos);
      break;
    case ifc_expr_unaryfold:
      getDerived()->template visit<an_ifc_ExprSort_UnaryFold>(pos);
      break;
    case ifc_expr_binaryfold:
      getDerived()->template visit<an_ifc_ExprSort_BinaryFold>(pos);
      break;
    case ifc_expr_hierarchy_conversion:
      getDerived()->template visit<an_ifc_ExprSort_HierarchyConversion>(pos);
      break;
    case ifc_expr_product:
      getDerived()->template visit<an_ifc_ExprSort_ProductTypeValue>(pos);
      break;
    case ifc_expr_sum:
      getDerived()->template visit<an_ifc_ExprSort_SumTypeValue>(pos);
      break;
    case ifc_expr_subobject:
      getDerived()->template visit<an_ifc_ExprSort_SubobjectValue>(pos);
      break;
    case ifc_expr_array:
      getDerived()->template visit<an_ifc_ExprSort_ArrayValue>(pos);
      break;
    case ifc_expr_dynamic_dispatch:
      getDerived()->template visit<an_ifc_ExprSort_DynamicDispatch>(pos);
      break;
    case ifc_expr_virtual_function:
      getDerived()->template visit<an_ifc_ExprSort_VirtualFunctionConversion>(
                                                                          pos);
      break;
    case ifc_expr_placeholder:
      getDerived()->template visit<an_ifc_ExprSort_Placeholder>(pos);
      break;
    case ifc_expr_expansion:
      getDerived()->template visit<an_ifc_ExprSort_Expansion>(pos);
      break;
    case ifc_expr_generic:
      getDerived()->template visit<an_ifc_ExprSort_Generic>(pos);
      break;
    case ifc_expr_tuple:
      getDerived()->template visit<an_ifc_ExprSort_Tuple>(pos);
      break;
    case ifc_expr_nullptr:
      getDerived()->template visit<an_ifc_ExprSort_Nullptr>(pos);
      break;
    case ifc_expr_this:
      getDerived()->template visit<an_ifc_ExprSort_This>(pos);
      break;
    case ifc_expr_template_reference:
      getDerived()->template visit<an_ifc_ExprSort_TemplateReference>(pos);
      break;
    case ifc_expr_push_state:
      getDerived()->template visit<an_ifc_ExprSort_PushState>(pos);
      break;
    case ifc_expr_type_trait:
      getDerived()->template visit<an_ifc_ExprSort_TypeTraitIntrinsic>(pos);
      break;
    case ifc_expr_des_init:
      getDerived()->template visit<an_ifc_ExprSort_DesignatedInitializer>(pos);
      break;
    case ifc_expr_packed_template_arguments:
      getDerived()->template visit<an_ifc_ExprSort_PackedTemplateArguments>(
                                                                          pos);
      break;
    case ifc_expr_tokens:
      getDerived()->template visit<an_ifc_ExprSort_Tokens>(pos);
      break;
    case ifc_expr_assign_initializer:
      getDerived()->template visit<an_ifc_ExprSort_AssignInitializer>(pos);
      break;
    /* Group all StmtIndex::Tag partitions together. */
    case ifc_stmt_vendor_extension:
      getDerived()->template visit<an_ifc_StmtSort_VendorExtension>(pos);
      break;
    case ifc_stmt_empty:
      getDerived()->template visit<an_ifc_StmtSort_Empty>(pos);
      break;
    case ifc_stmt_if:
      getDerived()->template visit<an_ifc_StmtSort_If>(pos);
      break;
    case ifc_stmt_for:
      getDerived()->template visit<an_ifc_StmtSort_For>(pos);
      break;
    case ifc_stmt_case:
      getDerived()->template visit<an_ifc_StmtSort_Case>(pos);
      break;
    case ifc_stmt_while:
      getDerived()->template visit<an_ifc_StmtSort_While>(pos);
      break;
    case ifc_stmt_block:
      getDerived()->template visit<an_ifc_StmtSort_Block>(pos);
      break;
    case ifc_stmt_break:
      getDerived()->template visit<an_ifc_StmtSort_Break>(pos);
      break;
    case ifc_stmt_switch:
      getDerived()->template visit<an_ifc_StmtSort_Switch>(pos);
      break;
    case ifc_stmt_do_while:
      getDerived()->template visit<an_ifc_StmtSort_DoWhile>(pos);
      break;
    case ifc_stmt_default:
      getDerived()->template visit<an_ifc_StmtSort_Default>(pos);
      break;
    case ifc_stmt_continue:
      getDerived()->template visit<an_ifc_StmtSort_Continue>(pos);
      break;
    case ifc_stmt_expression:
      getDerived()->template visit<an_ifc_StmtSort_Expression>(pos);
      break;
    case ifc_stmt_return:
      getDerived()->template visit<an_ifc_StmtSort_Return>(pos);
      break;
    case ifc_stmt_variable:
      getDerived()->template visit<an_ifc_StmtSort_VariableDecl>(pos);
      break;
    case ifc_stmt_expansion:
      getDerived()->template visit<an_ifc_StmtSort_Expansion>(pos);
      break;
    case ifc_stmt_syntax_tree:
      getDerived()->template visit<an_ifc_StmtSort_SyntaxTree>(pos);
      break;
    /* Group all ChartIndex::Tag partitions together. */
    case ifc_chart_none:
      getDerived()->template visit<an_ifc_ChartSort_None>(pos);
      break;
    case ifc_chart_unilevel:
      getDerived()->template visit<an_ifc_ChartSort_Unilevel>(pos);
      break;
    case ifc_chart_multilevel:
      getDerived()->template visit<an_ifc_ChartSort_Multilevel>(pos);
      break;
    /* Group all AttrIndex::Tag partitions together. */
    case ifc_attr_nothing:
      getDerived()->template visit<an_ifc_AttrSort_Nothing>(pos);
      break;
    case ifc_attr_basic:
      getDerived()->template visit<an_ifc_AttrSort_Basic>(pos);
      break;
    case ifc_attr_scoped:
      getDerived()->template visit<an_ifc_AttrSort_Scoped>(pos);
      break;
    case ifc_attr_labeled:
      getDerived()->template visit<an_ifc_AttrSort_Labeled>(pos);
      break;
    case ifc_attr_called:
      getDerived()->template visit<an_ifc_AttrSort_Called>(pos);
      break;
    case ifc_attr_expanded:
      getDerived()->template visit<an_ifc_AttrSort_Expanded>(pos);
      break;
    case ifc_attr_factored:
      getDerived()->template visit<an_ifc_AttrSort_Factored>(pos);
      break;
    case ifc_attr_elaborated:
      getDerived()->template visit<an_ifc_AttrSort_Elaborated>(pos);
      break;
    case ifc_attr_tuple:
      getDerived()->template visit<an_ifc_AttrSort_Tuple>(pos);
      break;
    /* Group all SyntaxIndex::Tag partitions together. */
    case ifc_syntax_vendor_extension:
      getDerived()->template visit<an_ifc_SyntaxSort_VendorExtension>(pos);
      break;
    case ifc_syntax_simple_type_specifier:
      getDerived()->template visit<an_ifc_SyntaxSort_SimpleTypeSpecifier>(pos);
      break;
    case ifc_syntax_decltype_specifier:
      getDerived()->template visit<an_ifc_SyntaxSort_DecltypeSpecifier>(pos);
      break;
    case ifc_syntax_placeholder_type_specifier:
      getDerived()->template visit<an_ifc_SyntaxSort_PlaceholderTypeSpecifier>(
                                                                          pos);
      break;
    case ifc_syntax_type_specifier_seq:
      getDerived()->template visit<an_ifc_SyntaxSort_TypeSpecifierSeq>(pos);
      break;
    case ifc_syntax_decl_specifier_seq:
      getDerived()->template visit<an_ifc_SyntaxSort_DeclSpecifierSeq>(pos);
      break;
    case ifc_syntax_virtual_specifier_seq:
      getDerived()->template visit<an_ifc_SyntaxSort_VirtualSpecifierSeq>(pos);
      break;
    case ifc_syntax_noexcept_specification:
      getDerived()->template visit<an_ifc_SyntaxSort_NoexceptSpecification>(
                                                                          pos);
      break;
    case ifc_syntax_explicit_specifier:
      getDerived()->template visit<an_ifc_SyntaxSort_ExplicitSpecifier>(pos);
      break;
    case ifc_syntax_enum_specifier:
      getDerived()->template visit<an_ifc_SyntaxSort_EnumSpecifier>(pos);
      break;
    case ifc_syntax_enumerator_definition:
      getDerived()->template visit<an_ifc_SyntaxSort_EnumeratorDefinition>(
                                                                          pos);
      break;
    case ifc_syntax_class_specifier:
      getDerived()->template visit<an_ifc_SyntaxSort_ClassSpecifier>(pos);
      break;
    case ifc_syntax_member_specification:
      getDerived()->template visit<an_ifc_SyntaxSort_MemberSpecification>(pos);
      break;
    case ifc_syntax_member_declaration:
      getDerived()->template visit<an_ifc_SyntaxSort_MemberDeclaration>(pos);
      break;
    case ifc_syntax_member_declarator:
      getDerived()->template visit<an_ifc_SyntaxSort_MemberDeclarator>(pos);
      break;
    case ifc_syntax_access_specifier:
      getDerived()->template visit<an_ifc_SyntaxSort_AccessSpecifier>(pos);
      break;
    case ifc_syntax_base_specifier_list:
      getDerived()->template visit<an_ifc_SyntaxSort_BaseSpecifierList>(pos);
      break;
    case ifc_syntax_base_specifier:
      getDerived()->template visit<an_ifc_SyntaxSort_BaseSpecifier>(pos);
      break;
    case ifc_syntax_type_id:
      getDerived()->template visit<an_ifc_SyntaxSort_TypeId>(pos);
      break;
    case ifc_syntax_trailing_return_type:
      getDerived()->template visit<an_ifc_SyntaxSort_TrailingReturnType>(pos);
      break;
    case ifc_syntax_declarator:
      getDerived()->template visit<an_ifc_SyntaxSort_Declarator>(pos);
      break;
    case ifc_syntax_pointer_declarator:
      getDerived()->template visit<an_ifc_SyntaxSort_PointerDeclarator>(pos);
      break;
    case ifc_syntax_array_declarator:
      getDerived()->template visit<an_ifc_SyntaxSort_ArrayDeclarator>(pos);
      break;
    case ifc_syntax_function_declarator:
      getDerived()->template visit<an_ifc_SyntaxSort_FunctionDeclarator>(pos);
      break;
    case ifc_syntax_array_or_function_declarator:
      getDerived()->template visit<
                             an_ifc_SyntaxSort_ArrayOrFunctionDeclarator>(pos);
      break;
    case ifc_syntax_parameter_declarator:
      getDerived()->template visit<an_ifc_SyntaxSort_ParameterDeclarator>(pos);
      break;
    case ifc_syntax_init_declarator:
      getDerived()->template visit<an_ifc_SyntaxSort_InitDeclarator>(pos);
      break;
    case ifc_syntax_new_declarator:
      getDerived()->template visit<an_ifc_SyntaxSort_NewDeclarator>(pos);
      break;
    case ifc_syntax_simple_declaration:
      getDerived()->template visit<an_ifc_SyntaxSort_SimpleDeclaration>(pos);
      break;
    case ifc_syntax_exception_declaration:
      getDerived()->template visit<an_ifc_SyntaxSort_ExceptionDeclaration>(
                                                                          pos);
      break;
    case ifc_syntax_condition_declaration:
      getDerived()->template visit<an_ifc_SyntaxSort_ConditionDeclaration>(
                                                                          pos);
      break;
    case ifc_syntax_static_assert_declaration:
      getDerived()->template visit<an_ifc_SyntaxSort_StaticAssertDeclaration>(
                                                                          pos);
      break;
    case ifc_syntax_alias_declaration:
      getDerived()->template visit<an_ifc_SyntaxSort_AliasDeclaration>(pos);
      break;
    case ifc_syntax_concept_definition:
      getDerived()->template visit<an_ifc_SyntaxSort_ConceptDefinition>(pos);
      break;
    case ifc_syntax_compound_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_CompoundStatement>(pos);
      break;
    case ifc_syntax_return_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_ReturnStatement>(pos);
      break;
    case ifc_syntax_if_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_IfStatement>(pos);
      break;
    case ifc_syntax_while_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_WhileStatement>(pos);
      break;
    case ifc_syntax_do_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_DoWhileStatement>(pos);
      break;
    case ifc_syntax_for_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_ForStatement>(pos);
      break;
    case ifc_syntax_init_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_InitStatement>(pos);
      break;
    case ifc_syntax_range_based_for_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_RangeBasedForStatement>(
                                                                          pos);
      break;
    case ifc_syntax_for_range_declaration:
      getDerived()->template visit<an_ifc_SyntaxSort_ForRangeDeclaration>(pos);
      break;
    case ifc_syntax_labeled_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_LabeledStatement>(pos);
      break;
    case ifc_syntax_break_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_BreakStatement>(pos);
      break;
    case ifc_syntax_continue_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_ContinueStatement>(pos);
      break;
    case ifc_syntax_switch_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_SwitchStatement>(pos);
      break;
    case ifc_syntax_goto_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_GotoStatement>(pos);
      break;
    case ifc_syntax_declaration_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_DeclarationStatement>(
                                                                          pos);
      break;
    case ifc_syntax_expression_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_ExpressionStatement>(pos);
      break;
    case ifc_syntax_try_block:
      getDerived()->template visit<an_ifc_SyntaxSort_TryBlock>(pos);
      break;
    case ifc_syntax_handler:
      getDerived()->template visit<an_ifc_SyntaxSort_Handler>(pos);
      break;
    case ifc_syntax_handler_seq:
      getDerived()->template visit<an_ifc_SyntaxSort_HandlerSeq>(pos);
      break;
    case ifc_syntax_function_try_block:
      getDerived()->template visit<an_ifc_SyntaxSort_FunctionTryBlock>(pos);
      break;
    case ifc_syntax_type_id_list_element:
      getDerived()->template visit<an_ifc_SyntaxSort_TypeIdListElement>(pos);
      break;
    case ifc_syntax_dynamic_exception_spec:
      getDerived()->template visit<an_ifc_SyntaxSort_DynamicExceptionSpec>(
                                                                          pos);
      break;
    case ifc_syntax_statement_seq:
      getDerived()->template visit<an_ifc_SyntaxSort_StatementSeq>(pos);
      break;
    case ifc_syntax_function_body:
      getDerived()->template visit<an_ifc_SyntaxSort_FunctionBody>(pos);
      break;
    case ifc_syntax_expression:
      getDerived()->template visit<an_ifc_SyntaxSort_Expression>(pos);
      break;
    case ifc_syntax_function_definition:
      getDerived()->template visit<an_ifc_SyntaxSort_FunctionDefinition>(pos);
      break;
    case ifc_syntax_member_function_declaration:
      getDerived()->template visit<
                             an_ifc_SyntaxSort_MemberFunctionDeclaration>(pos);
      break;
    case ifc_syntax_template_declaration:
      getDerived()->template visit<an_ifc_SyntaxSort_TemplateDeclaration>(pos);
      break;
    case ifc_syntax_requires_clause:
      getDerived()->template visit<an_ifc_SyntaxSort_RequiresClause>(pos);
      break;
    case ifc_syntax_simple_requirement:
      getDerived()->template visit<an_ifc_SyntaxSort_SimpleRequirement>(pos);
      break;
    case ifc_syntax_type_requirement:
      getDerived()->template visit<an_ifc_SyntaxSort_TypeRequirement>(pos);
      break;
    case ifc_syntax_compound_requirement:
      getDerived()->template visit<an_ifc_SyntaxSort_CompoundRequirement>(pos);
      break;
    case ifc_syntax_nested_requirement:
      getDerived()->template visit<an_ifc_SyntaxSort_NestedRequirement>(pos);
      break;
    case ifc_syntax_requirement_body:
      getDerived()->template visit<an_ifc_SyntaxSort_RequirementBody>(pos);
      break;
    case ifc_syntax_type_template_parameter:
      getDerived()->template visit<an_ifc_SyntaxSort_TypeTemplateParameter>(
                                                                          pos);
      break;
    case ifc_syntax_template_template_parameter:
      getDerived()->template visit<
                             an_ifc_SyntaxSort_TemplateTemplateParameter>(pos);
      break;
    case ifc_syntax_type_template_argument:
      getDerived()->template visit<an_ifc_SyntaxSort_TypeTemplateArgument>(
                                                                          pos);
      break;
    case ifc_syntax_non_type_template_argument:
      getDerived()->template visit<an_ifc_SyntaxSort_NonTypeTemplateArgument>(
                                                                          pos);
      break;
    case ifc_syntax_template_parameter_list:
      getDerived()->template visit<an_ifc_SyntaxSort_TemplateParameterList>(
                                                                          pos);
      break;
    case ifc_syntax_template_argument_list:
      getDerived()->template visit<an_ifc_SyntaxSort_TemplateArgumentList>(
                                                                          pos);
      break;
    case ifc_syntax_template_id:
      getDerived()->template visit<an_ifc_SyntaxSort_TemplateId>(pos);
      break;
    case ifc_syntax_mem_initializer:
      getDerived()->template visit<an_ifc_SyntaxSort_MemInitializer>(pos);
      break;
    case ifc_syntax_ctor_initializer:
      getDerived()->template visit<an_ifc_SyntaxSort_CtorInitializer>(pos);
      break;
    case ifc_syntax_lambda_introducer:
      getDerived()->template visit<an_ifc_SyntaxSort_LambdaIntroducer>(pos);
      break;
    case ifc_syntax_lambda_declarator:
      getDerived()->template visit<an_ifc_SyntaxSort_LambdaDeclarator>(pos);
      break;
    case ifc_syntax_capture_default:
      getDerived()->template visit<an_ifc_SyntaxSort_CaptureDefault>(pos);
      break;
    case ifc_syntax_simple_capture:
      getDerived()->template visit<an_ifc_SyntaxSort_SimpleCapture>(pos);
      break;
    case ifc_syntax_init_capture:
      getDerived()->template visit<an_ifc_SyntaxSort_InitCapture>(pos);
      break;
    case ifc_syntax_this_capture:
      getDerived()->template visit<an_ifc_SyntaxSort_ThisCapture>(pos);
      break;
    case ifc_syntax_attributed_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_AttributedStatement>(pos);
      break;
    case ifc_syntax_attributed_declaration:
      getDerived()->template visit<an_ifc_SyntaxSort_AttributedDeclaration>(
                                                                          pos);
      break;
    case ifc_syntax_attribute_specifier_seq:
      getDerived()->template visit<an_ifc_SyntaxSort_AttributeSpecifierSeq>(
                                                                          pos);
      break;
    case ifc_syntax_attribute_specifier:
      getDerived()->template visit<an_ifc_SyntaxSort_AttributeSpecifier>(pos);
      break;
    case ifc_syntax_attribute_using_prefix:
      getDerived()->template visit<an_ifc_SyntaxSort_AttributeUsingPrefix>(
                                                                          pos);
      break;
    case ifc_syntax_attribute:
      getDerived()->template visit<an_ifc_SyntaxSort_Attribute>(pos);
      break;
    case ifc_syntax_attribute_argument_clause:
      getDerived()->template visit<an_ifc_SyntaxSort_AttributeArgumentClause>(
                                                                          pos);
      break;
    case ifc_syntax_alignas:
      getDerived()->template visit<an_ifc_SyntaxSort_Alignas>(pos);
      break;
    case ifc_syntax_using_declaration:
      getDerived()->template visit<an_ifc_SyntaxSort_UsingDeclaration>(pos);
      break;
    case ifc_syntax_using_declarator:
      getDerived()->template visit<an_ifc_SyntaxSort_UsingDeclarator>(pos);
      break;
    case ifc_syntax_using_directive:
      getDerived()->template visit<an_ifc_SyntaxSort_UsingDirective>(pos);
      break;
    case ifc_syntax_array_index:
      getDerived()->template visit<an_ifc_SyntaxSort_ArrayIndex>(pos);
      break;
    case ifc_syntax_seh_try:
      getDerived()->template visit<an_ifc_SyntaxSort_SEHTry>(pos);
      break;
    case ifc_syntax_seh_except:
      getDerived()->template visit<an_ifc_SyntaxSort_SEHExcept>(pos);
      break;
    case ifc_syntax_seh_finally:
      getDerived()->template visit<an_ifc_SyntaxSort_SEHFinally>(pos);
      break;
    case ifc_syntax_seh_leave:
      getDerived()->template visit<an_ifc_SyntaxSort_SEHLeave>(pos);
      break;
    case ifc_syntax_type_trait_intrinsic:
      getDerived()->template visit<an_ifc_SyntaxSort_TypeTraitIntrinsic>(pos);
      break;
    case ifc_syntax_tuple:
      getDerived()->template visit<an_ifc_SyntaxSort_Tuple>(pos);
      break;
    case ifc_syntax_asm_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_AsmStatement>(pos);
      break;
    case ifc_syntax_namespace_alias_definition:
      getDerived()->template visit<
                              an_ifc_SyntaxSort_NamespaceAliasDefinition>(pos);
      break;
    case ifc_syntax_super:
      getDerived()->template visit<an_ifc_SyntaxSort_Super>(pos);
      break;
    case ifc_syntax_unary_fold_expression:
      getDerived()->template visit<an_ifc_SyntaxSort_UnaryFoldExpression>(pos);
      break;
    case ifc_syntax_binary_fold_expression:
      getDerived()->template visit<an_ifc_SyntaxSort_BinaryFoldExpression>(
                                                                          pos);
      break;
    case ifc_syntax_empty_statement:
      getDerived()->template visit<an_ifc_SyntaxSort_EmptyStatement>(pos);
      break;
    case ifc_syntax_structured_binding_declaration:
      getDerived()->template visit<
                          an_ifc_SyntaxSort_StructuredBindingDeclaration>(pos);
      break;
    case ifc_syntax_structured_binding_identifier:
      getDerived()->template visit<
                           an_ifc_SyntaxSort_StructuredBindingIdentifier>(pos);
      break;
    case ifc_syntax_using_enum_decl:
      getDerived()->template visit<
                                  an_ifc_SyntaxSort_UsingEnumDeclaration>(pos);
      break;
    /* Group all MacroIndex::Tag partitions together. */
    case ifc_macro_obj_like:
      getDerived()->template visit<an_ifc_MacroSort_ObjectLike>(pos);
      break;
    case ifc_macro_func_like:
      getDerived()->template visit<an_ifc_MacroSort_FunctionLike>(pos);
      break;
    /* Group all FormIndex::Tag partitions together. */
    case ifc_form_ident:
      getDerived()->template visit<an_ifc_FormSort_Identifier>(pos);
      break;
    case ifc_form_number:
      getDerived()->template visit<an_ifc_FormSort_Number>(pos);
      break;
    case ifc_form_char:
      getDerived()->template visit<an_ifc_FormSort_Character>(pos);
      break;
    case ifc_form_string:
      getDerived()->template visit<an_ifc_FormSort_String>(pos);
      break;
    case ifc_form_operator:
      getDerived()->template visit<an_ifc_FormSort_Operator>(pos);
      break;
    case ifc_form_keyword:
      getDerived()->template visit<an_ifc_FormSort_Keyword>(pos);
      break;
    case ifc_form_whitespace:
      getDerived()->template visit<an_ifc_FormSort_Whitespace>(pos);
      break;
    case ifc_form_param:
      getDerived()->template visit<an_ifc_FormSort_Parameter>(pos);
      break;
    case ifc_form_stringize:
      getDerived()->template visit<an_ifc_FormSort_Stringize>(pos);
      break;
    case ifc_form_catenate:
      getDerived()->template visit<an_ifc_FormSort_Catenate>(pos);
      break;
    case ifc_form_pragma:
      getDerived()->template visit<an_ifc_FormSort_Pragma>(pos);
      break;
    case ifc_form_header:
      getDerived()->template visit<an_ifc_FormSort_Header>(pos);
      break;
    case ifc_form_parenthesized:
      getDerived()->template visit<an_ifc_FormSort_Parenthesized>(pos);
      break;
    case ifc_form_tuple:
      getDerived()->template visit<an_ifc_FormSort_Tuple>(pos);
      break;
    case ifc_form_junk:
      getDerived()->template visit<an_ifc_FormSort_Junk>(pos);
      break;
    /* Group scope sequences together. */
    case ifc_scope_desc:
      getDerived()->template visit<an_ifc_Scope_Descriptor>(pos);
      break;
    case ifc_scope_member:
      getDerived()->template visit<an_ifc_Scope_Member>(pos);
      break;
    /* Group heaps together. */
    case ifc_heap_attr:
    case ifc_heap_chart:
    case ifc_heap_decl:
    case ifc_heap_expr:
    case ifc_heap_form:
    case ifc_heap_pp:
    case ifc_heap_stmt:
    case ifc_heap_syn:
    case ifc_heap_type:
      /* FIXME: Actually visit these. */
      break;
    /* No grouping, but visited. */
    case ifc_src_line:
      getDerived()->template visit<an_ifc_Source_Line>(pos);
      break;
    /* EDG utility partitions.  These should never be seen here.  Diagnose
       these as unknown partitions if we reach this point at runtime. */
    case ifc_none:
    case ifc_last:
      unexpected_condition_str("unknown partition visited");
    /* Currently unvisited. */
    case ifc_cmd_line:
    case ifc_const_f64:
    case ifc_const_i64:
    case ifc_const_str:
    case ifc_form_spec:
    case ifc_msvc_trait_code_segment:
    case ifc_msvc_trait_codegen_expr_trees:
    case ifc_msvc_trait_decl_attrs:
    case ifc_msvc_trait_entity_init_locus:
    case ifc_msvc_trait_impl_pragmas:
    case ifc_msvc_trait_named_func_params:
    case ifc_msvc_trait_spec_encodings:
    case ifc_msvc_trait_suppressed_warnings:
    case ifc_msvc_trait_templ_templ_param_classes:
    case ifc_msvc_trait_uuid:
    case ifc_msvc_trait_vendor_traits:
    case ifc_module_exported:
    case ifc_module_imported:
    case ifc_pragma_state:
    case ifc_pragma_vendorext:
    case ifc_sentence:
    case ifc_trait_alias_template:
    case ifc_trait_attribute:
    case ifc_trait_deduction_guides:
    case ifc_trait_deprecated:
    case ifc_trait_friend:
    case ifc_trait_function_definition:
    case ifc_trait_requires:
    case ifc_trait_specialization:
    case ifc_word:
    case ifc_name_identifier:
      break;
    default_is_unexpected();
  } /* switch */
}  /* visit_position */


/* A CRT (curiously recursive template) visitor class used to visit fields of a
   partition element that generalizes field types for easier visitor
   implementation.

   Derived classes should implement their visitors with an "overridden" visit
   function accessible to the instantiated Element_field_visitor.

   Derived classes may also choose to implement or replace their own field type
   conversion rules by "overriding" the visit_pre_conversion function.

   Visitation is started by calling one of the visit_element functions.  The
   visit_element function will call the derived visit_pre_conversion on all
   elements.  The visit_pre_conversion functions then apply any relevant
   conversions before calling the respective derived visit function. */
template<typename a_Derived_T>
struct an_ifc_module::Element_field_visitor {
  Element_field_visitor(an_ifc_module *ifc_mod_val)
    : ifc_mod(ifc_mod_val)
    {}

#define IFC_DECL_START(name) \
  inline void visit_element(concat(an_ifc_, name) *memp);
#define IFC_DECL_FIELD(name, type) /* nothing */
#define IFC_DECL_END(name) /* nothing */

/*lint -e451 included more than once. */
#include "ifc_map.h"
/*lint +e451*/
protected:
  template<typename a_Type>
  inline void visit(a_Type val, a_const_char *field_name) = delete;
  /* General conversion function that forwards to the derived visitor class.
     This function is selected as a fallback if no conversion visit function is
     declared below. */
  template<typename a_Type>
  inline void visit_pre_conversion(a_Type val, a_const_char *field_name)
    { getDerived()->visit(val, field_name); }
  /* Specialized conversion visit functions that reduce their argument to an
     element position. */
  inline void visit_pre_conversion(ifc_AttrIndex val,
                                   a_const_char  *field_name);
  inline void visit_pre_conversion(ifc_ChartIndex val,
                                   a_const_char   *field_name);
  inline void visit_pre_conversion(ifc_DeclIndex val,
                                   a_const_char  *field_name);
  inline void visit_pre_conversion(ifc_ExprIndex val,
                                   a_const_char  *field_name);
  inline void visit_pre_conversion(ifc_FormIndex val,
                                   a_const_char  *field_name);
  inline void visit_pre_conversion(ifc_FormSpecIndex val,
                                   a_const_char      *field_name);
  inline void visit_pre_conversion(ifc_LineIndex val,
                                   a_const_char  *field_name);
  inline void visit_pre_conversion(ifc_MacroIndex val,
                                   a_const_char   *field_name);
  inline void visit_pre_conversion(ifc_NameIndex val,
                                   a_const_char  *field_name);
  inline void visit_pre_conversion(ifc_ScopeIndex val,
                                   a_const_char   *field_name);
  inline void visit_pre_conversion(ifc_SourceLocation val,
                                   a_const_char       *field_name);
  inline void visit_pre_conversion(ifc_StmtIndex val,
                                   a_const_char  *field_name);
  inline void visit_pre_conversion(ifc_SyntaxIndex val,
                                   a_const_char    *field_name);
  inline void visit_pre_conversion(ifc_TypeIndex val,
                                   a_const_char  *field_name);
  /* Utility functions */
  template<typename... Args>
  inline an_ifc_partition_position convert_to_pos(Args&&... args) const
    { return an_ifc_partition_position(ifc_mod, args...); }
  inline auto getDerived() -> a_Derived_T *
    { return static_cast<a_Derived_T*>(this); }
  an_ifc_module *ifc_mod;
};  /* Element_field_visitor */


/* Automatically generate a specialization for visiting the members of
   an IFC module partition element. */

#define IFC_DECL_START(name) \
template<typename a_Derived_T> \
inline void \
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_element( \
                                                 concat(an_ifc_, name) *memp) \
{
#define IFC_DECL_FIELD(name, type) \
  getDerived()->visit_pre_conversion(memp->name, #name);
#define IFC_DECL_END(name) \
}  /* def_visit_members<concat(an_ifc_, name)> */

/*lint -e451 included more than once. */
#include "ifc_map.h"
/*lint +e451*/

template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                     ifc_AttrIndex val,
                                                     a_const_char  *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                    ifc_ChartIndex val,
                                                    a_const_char   *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                     ifc_DeclIndex val,
                                                     a_const_char  *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                     ifc_ExprIndex val,
                                                     a_const_char  *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                     ifc_FormIndex val,
                                                     a_const_char  *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                 ifc_FormSpecIndex val,
                                                 a_const_char      *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                     ifc_LineIndex val,
                                                     a_const_char  *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                    ifc_MacroIndex val,
                                                    a_const_char   *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                     ifc_NameIndex val,
                                                     a_const_char  *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    if (name_tag(val) != ifc_NameSort_Identifier) {
      getDerived()->visit(convert_to_pos(val), field_name);
    }  /* if */
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                    ifc_ScopeIndex val,
                                                    a_const_char   *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                ifc_SourceLocation val,
                                                a_const_char       *field_name)
/*
If val is set to a source location with a valid line index, this function
converts the index to a partition position and calls the visitor with the
converted index.
*/
{
  if (val.line != 0) {
    getDerived()->visit(convert_to_pos(val.line), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                     ifc_StmtIndex val,
                                                     a_const_char  *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                   ifc_SyntaxIndex val,
                                                   a_const_char    *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


template<typename a_Derived_T>
inline void
an_ifc_module::Element_field_visitor<a_Derived_T>::visit_pre_conversion(
                                                     ifc_TypeIndex val,
                                                     a_const_char  *field_name)
/*
If val is set to a valid index, this function converts the index to a partition
position and calls the visitor with the converted index.
*/
{
  if (val != 0) {
    getDerived()->visit(convert_to_pos(val), field_name);
  }  /* if */
}  /* visit_pre_conversion */


/* An implementation of Element_field_visitor with the visit function
   "overridden" to call Element_validation_state_clearer's reset function on
   all fields converted to ifc_partition_positions (all other visit functions
   are no ops). */
struct an_ifc_module::Element_field_validation_state_clearer
       : public Element_field_visitor<Element_field_validation_state_clearer> {
  inline Element_field_validation_state_clearer(
                          Element_validation_state_clearer *state_clearer_val);

private:
  friend Element_field_visitor<Element_field_validation_state_clearer>;
  /* General no op visit function.  This function is selected as a fallback if
     no other reset visit function is declared below. */
  template<typename a_Type>
  inline void visit(a_Type val, const char *field_name)
    {}
  /* Specialized visit functions. */
  inline void visit(an_ifc_partition_position val,
                    const char                *field_name);
  Element_validation_state_clearer *state_clearer;
};  /* Element_field_validation_state_clearer */


inline a_boolean an_ifc_module::validate_partition_position(
                                                 an_ifc_partition_position pos)
{
  a_boolean        result = TRUE;
  an_ifc_partition *partition = &partitions[pos.partition];

  if (partition->size == 0) {
    /* Check that anything is stored in the requested partition. */
    result = FALSE;
  } else if (pos.file_offset < partition->offset) {
    /* Check that this position follows the requested partition. */
    result = FALSE;
  } else {
    size_t relative_offset = pos.file_offset - partition->offset;
    if ((relative_offset + partition->entry_size) > partition->size) {
      /* Check that the relative offset is within the partition. */
      result = FALSE;
    } else if ((relative_offset % partition->entry_size) != 0) {
      /* Check that the relative offset is at a given position. */
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* validate_partition_position */


/* An implementation of Element_visitor with the visit function "overridden" to
   perform resetting of the validation cache for the element. */
struct an_ifc_module::Element_validation_state_clearer
                   : public Element_visitor<Element_validation_state_clearer> {
  Element_validation_state_clearer(an_ifc_module *ifc_mod_val)
    : ifc_mod(ifc_mod_val), field_visitor(this)
    {}
  void reset(an_ifc_partition_position pos)
    { reset(pos, ""); }
private:
  friend Element_visitor<Element_validation_state_clearer>;
  friend Element_field_validation_state_clearer;
  /* State reset functions. */
  inline void reset(an_ifc_partition_position pos, a_const_char *field_name);
  /* Function template specialized by code generation from the IFC map. */
  template<typename an_ifc_Element_Type>
  inline void def_visit(an_ifc_partition_position pos) = delete;
  /* General visit function that by default calls the respective def_visit
     explicit function specialization.  This function can be explicitly
     specialized to override the default behavior for its respective type. */
  template<typename an_ifc_Element_Type>
  inline void visit(an_ifc_partition_position pos)
    { def_visit<an_ifc_Element_Type>(pos); }
  an_ifc_module *ifc_mod;
  Element_field_validation_state_clearer
                field_visitor;
};  /* Element_validation_state_clearer */


/* Complete the Element_field_validation_state_clearer now that the
   Element_validation_state_clearer is a complete type. */

/* FIXME: How do we want to format this?  Is there a nice proposal for a
   shorter name? */
inline
an_ifc_module::Element_field_validation_state_clearer::
                                        Element_field_validation_state_clearer(
                 Element_validation_state_clearer *state_clearer_val)
  : Element_field_visitor(state_clearer_val->ifc_mod),
    state_clearer(state_clearer_val)
/*
Construct an Element_field_validation_state_clearer that delegates to the given
state clearer for validation state resets of referenced partition elements.
*/
{
}  /* Element_field_validation_state_clearer */


inline void
an_ifc_module::Element_field_validation_state_clearer::visit(
                                         an_ifc_partition_position val,
                                         a_const_char              *field_name)
/*
Reset the validation state for the given partition element position by
recursing.
*/
{
  state_clearer->reset(val);
}  /* visit */


inline void
an_ifc_module::Element_validation_state_clearer::reset(
                                         an_ifc_partition_position pos,
                                         a_const_char              *field_name)
/*
Reset the validation state for the given partition element position by
recursing.
*/
{
  if (ifc_mod->validate_partition_position(pos)) {
    /* This is a sane position, the validation cache can now be consulted
       to see if this address has been previously checked. */
    if (ifc_mod->has_been_validated(pos)) {
      /* This element was previously validated, reset it and any children. */
      ifc_mod->reset_validation_state(pos);
      visit_position(pos);
    } /* if */
  } /* if */
}  /* visit */


/* Automatically generate a specialization for visiting an IFC module partition
   element and visiting its members with the automatically generated member
   validation function. */

#define IFC_DECL_START(name) \
template<> \
inline void \
an_ifc_module::Element_validation_state_clearer::def_visit< \
                        concat(an_ifc_, name)>(an_ifc_partition_position pos) \
{ \
  concat(an_ifc_, name) mem, *memp; \
  ifc_mod->read_unchecked_partition_element(pos); \
  memp = ifc_mod->get<concat(an_ifc_, name)>(&mem); \
  field_visitor.visit_element(memp);
#define IFC_DECL_FIELD(field, type) /* nothing */
/* Generate the end of the function. */
#define IFC_DECL_END(name) \
}  /* def_visit<concat(an_ifc_, name)> */

/*lint -e451 included more than once. */
#include "ifc_map.h"
/*lint +e451*/


/* An implementation of Element_field_visitor with the visit function
   "overridden" to call Element_validator's validate function on all fields
   converted to ifc_partition_positions (all other visit functions are no
   ops). */
struct an_ifc_module::Element_field_validator
                      : public Element_field_visitor<Element_field_validator> {
  inline Element_field_validator(Element_validator *validator_val);
private:
  friend Element_field_visitor<Element_field_validator>;
  /* General no op visit function.  This function is selected as a
     fallback if no other validation visit function is declared below. */
  template<typename a_Type>
  inline void visit(a_Type val, const char *field_name)
    {}
  /* Specialized visit functions. */
  inline void visit(an_ifc_partition_position val,
                    const char                *field_name);
  Element_validator *validator;
};  /* Element_field_validator */


/* An implementation of Element_visitor with the visit function "overridden" to
   perform validation on the element. */
struct an_ifc_module::Element_validator
                                  : public Element_visitor<Element_validator> {
  a_boolean invalid;

  Element_validator(an_ifc_module *ifc_mod_val,
                    a_boolean     recursively_val)
    : invalid(false), ifc_mod(ifc_mod_val), field_visitor(this),
      recursively(recursively_val), tail(nullptr), emit_diagnostics(TRUE)
    {}

  void validate(an_ifc_partition_position pos)
    { validate(pos, ""); }
private:
  friend Element_visitor<Element_validator>;
  friend Element_field_validator;
  struct Validation_stage;
  /* Error handling functions. */
  inline void mark_invalid();
  inline void add_backtrace(a_diagnostic_ptr diag_ptr) const;
  inline void invalid_partition(an_ifc_partition_position pos);
  inline void undefined_partition(an_ifc_partition_position pos);
  inline void invalid_position(an_ifc_partition_position pos);
  /* Validation functions. */
  inline void validate(an_ifc_partition_position pos, const char *field_name);
  /* Function template specialized by code generation from the IFC map. */
  template<typename an_ifc_Element_Type>
  inline void def_visit(an_ifc_partition_position pos) = delete;
  /* General visit function that by default calls the respective def_visit
     explicit function specialization.  This function can be explicitly
     specialized to override the default behavior for its respective type. */
  template<typename an_ifc_Element_Type>
  inline void visit(an_ifc_partition_position pos)
    { def_visit<an_ifc_Element_Type>(pos); }
  an_ifc_module *ifc_mod;
  Element_field_validator
                field_visitor;
  a_boolean     recursively;
  Validation_stage
                *tail;
  a_boolean     emit_diagnostics;
};  /* Decl_value_visitor */


/* A structure that's responsible for making the stack stored state related to
   the current "validate" call and all parents accessible.  This allows the
   call stack to be traversed (to retrieve information about the requester)
   without storing or duplicating this information in a dynamically allocated
   stack.

   A pointer to the current position, the field name referencing said position,
   and the position validity are made accessible via this system.

   The tail pointer is used to enter this information and points to the top of
   the validation stack.  It's maintained by storing the tail pointer's current
   value (prev), then updating the tail pointer to point to the current
   validation stage.  When destroyed the tail pointer is restored to its
   previous state. */
struct an_ifc_module::Element_validator::Validation_stage {
  Validation_stage          **tail_ptr;
  Validation_stage          *prev;
  an_ifc_partition_position *pos;
  const char                *field_name;
  a_boolean                 invalid;

  Validation_stage(Validation_stage          **tail_ptr_val,
                   an_ifc_partition_position *pos_val,
                   const char                *field_name_val)
    : tail_ptr(tail_ptr_val), prev(*tail_ptr_val), pos(pos_val),
      field_name(field_name_val), invalid(FALSE)
    { *tail_ptr = this; }
  ~Validation_stage()
    { *tail_ptr = prev; }
};  /* Validation_stage */


/* Complete the Element_field_validator now that the
   Element_validator is a complete type. */

inline
an_ifc_module::Element_field_validator::Element_field_validator(
                                              Element_validator *validator_val)
  : Element_field_visitor(validator_val->ifc_mod), validator(validator_val)
/*
Construct an Element_field_validator that delegates to the given validator for
validation of referenced partition elements.
*/
{
}  /* Element_field_validator */


inline void an_ifc_module::Element_field_validator::visit(
                                         an_ifc_partition_position val,
                                         const char                *field_name)
/*
Check the validity of the given partition element position by recursing.
*/
{
  validator->validate(val, field_name);
}  /* visit */


inline void an_ifc_module::Element_validator::validate(
                                         an_ifc_partition_position pos,
                                         const char                *field_name)
/*
Validate the given position.  This function always checks to see if the
specified position is a valid position.  If the position is determined to be a
valid position, it will then perform additional checks depending on context.

When this is a recursive validation it will validate the element's fields.
This follows ifc_IndexType derived (and similar) fields of the element at the
given position recursively until it's covered all index types.  Index types
that provide access to a set of elements (e.g. ScopeIndex) are validated.
However, the members of their set are not validated.  Results of this
validation are cached with the module to prevent excessive tree walking across
this and other Element_validator uses.

When this is a top level (the first position checked) non-recursive validation
it will validate the element's fields. This follows ifc_IndexType derived (and
similar) fields of the element at the given position up to one level down.  It
thus checks that the given position is a valid position, and that the element
contains fields that pass initial validation.  Results of this validation are
not cached.

When this is not a recursive validation or a top level non-recursive
validation, only the initial position validity is validated.  This case only
occurs as a sub-step of non-recursive validation.

Any validation failures will result in a call to the element validator's
mark_invalid function.
*/
{
  /* Expose the current validation call stack as a validation stack. */
  Validation_stage stage(&tail, &pos, field_name);

  /* First check to see if the given position is a real element that needs
     validation (i.e., check the position itself). */
  if (pos.partition == ifc_invalid_partition) {
    /* The partition is invalid, no processing can be done at this position. */
    invalid_partition(pos);
  } else if (ifc_mod->partitions[pos.partition].name == NULL) {
    /* The partition is undefined, no processing can be done at this
       position. */
    undefined_partition(pos);
  } else if (!ifc_mod->validate_partition_position(pos)) {
    /* The position is invalid, no processing can be done at this position. */
    invalid_position(pos);
  } else if (recursively || tail->prev == nullptr) {
    /* This is a sane position and deeper validation has been requested.  The
       validation cache can now be consulted to see if this address has been
       previously checked. */
    if (ifc_mod->has_been_validated(pos)) {
      /* This element was previously validated, use its previous state. */
      if (ifc_mod->is_marked_invalid(pos)) {
        mark_invalid();
      }  /* if */
    } else {
      /* This element was not previously validated.  There are two cases where
         validation is performed here with different implications.

         If doing recursive validation, this position will be fully validated,
         and can be cached with both valid and invalid status.

         If doing only top level (non-recursive/shallow) validation, the
         position will only be marked valid if it was determined to be invalid
         -- as without checking its dependencies we can't possibly know if
         it's valid, only that it's invalid. */
      if (recursively) {
        /* Handle first case (recursive validation). */
        /* Mark the element validated before doing validation if this is a
           recursive validation to prevent infinite recursion when the
           traversal self references.

           Note that this means references back to this partition element
           implicitly trust its validity, so if it ends up being invalid, we
           must clear everything that's potentially been validated as part of
           this operation, then retraverse everything with the correct
           validity.  This is slow but it happens only in the non-happy path so
           it's acceptable. */
        ifc_mod->mark_validated(pos);
        visit_position(pos);
        if (stage.invalid) {
          /* Note a call to mark_invalid has already been performed as part of
             marking the stage invalid.  A further call would be redundant. */
          Element_validation_state_clearer state_clearer(ifc_mod);

          /* Reset the state. */
          state_clearer.reset(pos);
          /* Mark the position as validated and invalid. */
          ifc_mod->mark_validated(pos);
          ifc_mod->mark_invalid(pos);
          /* Traverse again without diagnostics.  Any errors found have already
             been reported, this is just about fixing validation state if there
             was a self reference to this partition element -- which was
             originally assumed valid and is now invalid. */
          emit_diagnostics = FALSE;
          visit_position(pos);
          emit_diagnostics = TRUE;
        }  /* if */
      } else if (tail->prev == nullptr) {
        /* Handle second case (top level validation). */
        visit_position(pos);
      }  /* if */
    }  /* if */
  } /* if */
}  /* validate */


inline void an_ifc_module::Element_validator::mark_invalid()
/*
Marks the top level element invalid.  Additionally, any validation stages
between the current element and the top level element are marked as invalid --
as those elements depend on this element, and thus are invalid as well.
*/
{
  Validation_stage *cur = tail;

  while (cur != nullptr) {
    /* If something already marked parents invalid, stop to prevent duplicated
       effort. */
    if (cur->invalid) {
      break;
    }  /* if */
    /* Mark this stage and all parents as invalid. */
    cur->invalid = TRUE;
    /* Move down the stack. */
    cur = cur->prev;
  }  /* while */
  invalid = true;
}  /* mark_invalid */


inline void an_ifc_module::Element_validator::add_backtrace(
                                                     a_diagnostic_ptr diag_ptr)
                                                                          const
/*
Add information about the validation stack to the given diagnostic pointer to
produce a more detailed contextual diagnostic.
*/
{
  Validation_stage *cur = tail;
  a_boolean        deepest_element = TRUE;

  while (cur != nullptr) {
    an_ifc_partition_position *pos = cur->pos;
    a_const_char              *field_name = cur->field_name;
    size_t                    rel_offset = ifc_mod->to_relative_offset(*pos);

    /* Add this partition position request to the diagnostic. */
    if (!deepest_element) {
      uint32_t part_index = ifc_mod->to_partition_index(*pos);

      /* FIXME: Migrate to allowing diagnostics with size_t. */
      st_num3_add_diag_info(diag_ptr, ec_invalid_ifc_position_backtrace_pos,
                            ifc_mod->partitions[pos->partition].name,
                            part_index, (int32_t)pos->file_offset,
                            (int32_t)rel_offset);
    } else {
      deepest_element = FALSE;
    }  /* if */
    if (field_name[0] != '\0') {
      str_add_diag_info(diag_ptr, ec_invalid_ifc_position_backtrace_field,
                        field_name);
    }  /* if */
    /* Move down the stack. */
    cur = cur->prev;
  }  /* while */
}  /* add_backtrace */


inline void an_ifc_module::Element_validator::invalid_partition(
                                                 an_ifc_partition_position pos)
/*
Handle failure and, if enabled, diagnostics for an encountered invalid
partition.
*/
{
  if (emit_diagnostics) {
    a_diagnostic_ptr diag_ptr;

    /* FIXME: Use a better source position. */
    diag_ptr = pos_st_start_error(ec_invalid_ifc_partition,
                                  &null_source_position,
                                  ifc_mod->assoc_module_info->name);
    add_backtrace(diag_ptr);
    end_diagnostic(diag_ptr);
  }  /* if */
  mark_invalid();
}  /* invalid_partition */


inline void an_ifc_module::Element_validator::undefined_partition(
                                                 an_ifc_partition_position pos)
/*
Handle failure and, if enabled, diagnostics for an encountered undefined
partition.
*/
{
  for (uint32_t index = 0; index < num_ifc_partitions; ++index) {
    an_ifc_partition_map *map_entry = &ifc_partition_map[index];

    if (map_entry->kind == pos.partition) {
      if (emit_diagnostics) {
        a_diagnostic_ptr diag_ptr;

        /* FIXME: Use a better source position. */
        diag_ptr = pos_st2_start_error(ec_undefined_ifc_partition,
                                       &null_source_position,
                                       ifc_mod->assoc_module_info->name,
                                       map_entry->name);
        add_backtrace(diag_ptr);
        end_diagnostic(diag_ptr);
      }  /* if */
      mark_invalid();
      goto found;
    }  /* if */
  }
  /* Assert that we found the requested partition. */
  /* FIXME: Handle cases where we didn't find the requested partition. */
  unexpected_condition_str("undefined_partition: partition not found");
found:
  ;
}  /* undefined_partition */


inline void an_ifc_module::Element_validator::invalid_position(
                                                 an_ifc_partition_position pos)
/*
Handle failure and, if enabled, diagnostics for an encountered invalid
partition position.
*/
{
  an_ifc_partition *partition = &ifc_mod->partitions[pos.partition];
  size_t           relative_offset = ifc_mod->to_relative_offset(pos);
  an_error_code    error_code = ec_no_error;

  if (partition->size == 0) {
    error_code = ec_invalid_empty_ifc_position;
  } else if (pos.file_offset < partition->offset) {
    error_code = ec_invalid_preceding_ifc_position;
  } else {
    if ((relative_offset + partition->entry_size) > partition->size) {
      error_code = ec_invalid_overflowing_ifc_position;
    } else if ((relative_offset % partition->entry_size) != 0) {
      error_code = ec_invalid_misaligned_ifc_position;
    }  /* if */
  }  /* if */
  check_assertion(error_code != ec_no_error);
  if (emit_diagnostics) {
    a_diagnostic_ptr diag_ptr;

    /* FIXME: Use a better source position. */
    /* FIXME: Migrate to allowing diagnostics with size_t. */
    diag_ptr = pos_st2_num2_start_error(
                                       error_code, &null_source_position,
                                       ifc_mod->assoc_module_info->name,
                                       ifc_mod->partitions[pos.partition].name,
                                       (int32_t)pos.file_offset,
                                       (int32_t)relative_offset);
    add_backtrace(diag_ptr);
    end_diagnostic(diag_ptr);
  }  /* if */
  mark_invalid();
}  /* invalid_position */

/* Automatically generate a specialization for visiting an IFC module partition
   element and visiting its members with the automatically generated member
   validation function. */

#define IFC_DECL_START(name) \
template<> \
inline void an_ifc_module::Element_validator::def_visit< \
                        concat(an_ifc_, name)>(an_ifc_partition_position pos) \
{ \
  concat(an_ifc_, name) mem, *memp; \
  ifc_mod->read_unchecked_partition_element(pos); \
  memp = ifc_mod->get<concat(an_ifc_, name)>(&mem); \
  field_visitor.visit_element(memp);
#define IFC_DECL_FIELD(field, type) /* nothing */
/* Generate the end of the function. */
#define IFC_DECL_END(name) \
}  /* def_visit<concat(an_ifc_, name)> */

/*lint -e451 included more than once. */
#include "ifc_map.h"
/*lint +e451*/

/* Declare and define manual overrides that should be used in place of the
   automatically generated validation visit functions. */

template<>
inline void an_ifc_module::Element_validator::visit<
                      an_ifc_DeclSort_Reference>(an_ifc_partition_position pos)
/*
Visit and validate an IFC reference partition element.

FIXME: This partition element represents a reference to a declaration of
another module.  While the caller potentially depends on this, for now, treat
the reference itself as implicitly trusted to avoid the complexity.  Treating
this as a no-op implies that the function ordering the import of the external
declaration (if any) is responsible for validation, which may be the right
decision.
*/
{
}  /* visit<an_ifc_Reference> */


inline a_boolean an_ifc_module::validate_partition_element_shallow(
                                                 an_ifc_partition_position pos)
/*
Perform non-recursive validation checks on the given partition position.
Return TRUE if determined to be valid, FALSE otherwise.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  Element_validator validator(this, /*recursively=*/FALSE);

  validator.validate(pos);
  return !validator.invalid;
}  /* validate_partition_element_shallow */


template<typename an_Index_Type>
static a_boolean validate_partition_element(an_ifc_Ref<an_Index_Type> ref)
/*
Perform recursive validation checks on the partition position corresponding to
the given reference.  Return TRUE if determined to be valid, FALSE otherwise.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  return ref.mod->validate_partition_element(ref.index);
}  /* validate_partition_element */


inline a_boolean an_ifc_module::validate_partition_element(
                                                 an_ifc_partition_position pos)
/*
Perform recursive validation checks on the given partition position.  Returns
TRUE if determined to be valid, FALSE otherwise.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  Element_validator validator(this, /*recursively=*/TRUE);

  validator.validate(pos);
  return !validator.invalid;
}  /* validate_partition_element */


template<typename... Args>
inline a_boolean an_ifc_module::validate_partition_element(Args&&... args)
/*
Perform recursive validation checks on the partition position constructed from
args.  Return TRUE if determined to be valid, FALSE otherwise.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  an_ifc_partition_position pos(this, args...);

  return validate_partition_element(pos);
}  /* validate_partition_element */


inline void an_ifc_module::read_unchecked_partition_element(
                                                 an_ifc_partition_position pos)
/*
Initialize the byte buffer for the given partition position, without validation
or validation enforcement.
*/
{
#if DEBUG && EXPENSIVE_CHECKING
  debug_partition = &partitions[pos.partition];
#endif /* DEBUG && EXPENSIVE_CHECKING */
  init_byte_buffer(pos.file_offset, partitions[pos.partition].size);
}  /* read_unchecked_partition_element */


inline a_boolean an_ifc_module::read_partition_element_shallow(
                                                 an_ifc_partition_position pos)
/*
Perform non-recursive validation checks on the given partition position.  If
validation success initialize the byte buffer for the given partition.  Return
TRUE if determined to be valid, FALSE otherwise.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  a_boolean result = FALSE;
  if (validate_partition_element_shallow(pos)) {
    result = TRUE;
    read_unchecked_partition_element(pos);
  }  /* if */
  /* FIXME: Emit diagnostic here if false, or perhaps leave to caller? */
  return result;
}  /* read_partition_element_shallow */


template<typename... Args>
inline a_boolean an_ifc_module::read_partition_element_shallow(Args&&... args)
/*
Perform non-recursive validation checks on the partition position constructed
from args.  If validation succeeds initialize the byte buffer for the given
partition.  Return TRUE if determined to be valid, FALSE otherwise.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  an_ifc_partition_position pos(this, args...);

  return read_partition_element_shallow(pos);
}  /* read_partition_element_shallow */


inline a_boolean an_ifc_module::read_partition_element(
                                                 an_ifc_partition_position pos)
/*
Perform recursive validation checks on the given partition position.  If
validation success initialize the byte buffer for the given partition.  Return
TRUE if determined to be valid, FALSE otherwise.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  a_boolean result = FALSE;

  if (validate_partition_element(pos)) {
    result = TRUE;
    read_unchecked_partition_element(pos);
  }  /* if */
  return result;
}  /* read_partition_element */


template<typename... Args>
inline a_boolean an_ifc_module::read_partition_element(Args&&... args)
/*
Perform recursive validation checks on the partition position constructed from
args.  If validation succeeds initialize the byte buffer for the given
partition.  Return TRUE if determined to be valid, FALSE otherwise.

See Element_validator's position validation function for more details about
non-recursive and recursive validation.
*/
{
  an_ifc_partition_position pos(this, args...);

  return read_partition_element(pos);
}  /* read_partition_element */


inline void an_ifc_module::read_prechecked_partition_element(
                                                 an_ifc_partition_position pos)
/*
With the knowledge that the given position has been previously validated by a
call to validate_partition_element (or indirectly via a call to
read_partition_element), initialize the byte buffer for the given partition
without further validation.
*/
{
  check_assertion(validate_partition_position(pos));
  /* FIXME: We can't validate enough to enforce the precheck semantic yet. */
  /* check_assertion(has_been_validated(pos)); */
#if DEBUG && EXPENSIVE_CHECKING
  debug_partition = &partitions[pos.partition];
#endif /* DEBUG && EXPENSIVE_CHECKING */
  init_byte_buffer(pos.file_offset, partitions[pos.partition].size);
}  /* read_prechecked_partition_element */


template<typename... Args>
inline void an_ifc_module::read_prechecked_partition_element(Args&&... args)
/*
With the knowledge that the partition position position constructed from args
has been previously validated by a call to validate_partition_element (or
indirectly via a call to read_partition_element), initialize the byte buffer
for the given partition without further validation.
*/
{
  an_ifc_partition_position pos(this, args...);

  read_prechecked_partition_element(pos);
}  /* read_prechecked_partition_element */


inline size_t an_ifc_module::to_relative_offset(an_ifc_partition_position pos)
                                                                          const
/*
Return the relative offset of the position from the start of its associated
partition.
*/
{
  const an_ifc_partition *partition = &partitions[pos.partition];

  return pos.file_offset - partition->offset;
}  /* to_relative_offset */


inline uint32_t an_ifc_module::to_partition_index(
                                                 an_ifc_partition_position pos)
                                                                          const
/*
Return the position as an index into the partition.
*/
{
  const an_ifc_partition *partition = &partitions[pos.partition];
  size_t                 relative_offset = to_relative_offset(pos);

  return (uint32_t)(relative_offset / partition->entry_size);
}  /* to_partition_index */


inline a_boolean an_ifc_module::has_been_validated(
                                                 an_ifc_partition_position pos)
                                                                          const
/*
Return TRUE if this the element at this position has already been validated.
*/
{
  /* Setup a bit mask that can be used to check the validated status of a
     position's element.  Operationally, this creates a 32 bit mask working on
     the lower 16 bits:

       0000 0000 0000 0000 - 0000 0000 0000 0001

     Shifts the bit into the correct bit positions:

       0000 0000 0000 0000 - 0000 0000 0000 0100

     This then represents the true state for this position's validated bit.

     Then a bit-and operation is used, checking if said validated bit was
     set. */
  uint32_t index = to_partition_index(pos);
  size_t   block = index / 16;
  size_t   bit_index = index % 16;
  unsigned bit_mask = 0x1 << bit_index;

  return partitions[pos.partition].format_validated[block] & bit_mask;
}  /* has_been_validated */


inline void an_ifc_module::mark_validated(an_ifc_partition_position pos)
/*
Mark the element at the given position as having been validated.
*/
{
  /* Setup a bit mask that can be used to mark a position's element as
     validated.  Operationally, this creates a 32 bit mask working on the lower
     16 bits:

       0000 0000 0000 0000 - 0000 0000 0000 0001

     Shifts the bit into the correct bit positions:

       0000 0000 0000 0000 - 0000 0000 0000 0100

     This then represents the true state for this position's validated bit.

     Then a bit-or assignment operation is used, setting said validated bit
     while leaving the others untouched. */
  uint32_t index = to_partition_index(pos);
  size_t   block = index / 16;
  size_t   bit = index % 16;
  unsigned bit_mask = 0x1 << bit;

  partitions[pos.partition].format_validated[block] |= bit_mask;
}  /* mark_validated */


inline a_boolean an_ifc_module::is_marked_invalid(
                                                 an_ifc_partition_position pos)
                                                                          const
/*
Return TRUE if the element at this position was invalid when previously
validated.  This is only a valid operation if has_been_validated returns TRUE.
*/
{
  check_assertion(has_been_validated(pos));
  {
    /* Setup a bit mask that can be used to check the invalid status of a
       position's element.  Operationally, this creates a 32 bit mask working
       on the higher 16 bits:

         0000 0000 0000 0001 - 0000 0000 0000 0000

       Shifts the bit into the correct bit positions:

         0000 0000 0000 0100 - 0000 0000 0000 0000

       This then represents the true state for this position's invalid bit.

       Then a bit-and operation is used, checking if said invalid bit was
       set. */
    uint32_t index = to_partition_index(pos);
    size_t   block = index / 16;
    size_t   bit_index = index % 16;
    unsigned bit_mask = (0x1 << 16) << bit_index;

    return partitions[pos.partition].format_validated[block] & bit_mask;
  }
}  /* is_marked_invalid */


inline void an_ifc_module::mark_invalid(an_ifc_partition_position pos)
/*
Mark the element at the given position as having been validated.  This is only
a valid operation if has_been_validated returns TRUE.
*/
{
  check_assertion(has_been_validated(pos));
  {
    /* Setup a bit mask that can be used to mark a position's element as
       invalid.  Operationally, this creates a 32 bit mask working on the
       higher 16 bits:

         0000 0000 0000 0001 - 0000 0000 0000 0000

       Shifts the bit into the correct bit positions:

         0000 0000 0000 0100 - 0000 0000 0000 0000

       This then represents the true state for this position's invalid bit.

       Then a bit-or assignment operation is used, setting said invalid bit
       while leaving the others untouched. */
    uint32_t index = to_partition_index(pos);
    size_t   block = index / 16;
    size_t   bit = index % 16;
    unsigned bit_mask = (0x1 << 16) << bit;

    partitions[pos.partition].format_validated[block] |= bit_mask;
  }
}  /* mark_invalid */


inline void an_ifc_module::reset_validation_state(
                                                 an_ifc_partition_position pos)
/*
Reset the validation state bits for the element at the given position.  This
results in the element's validation state bits (i.e., validated and invalid)
being reset to their original pre-validation -- FALSE -- states.
*/
{
  /* Setup a bit inverted bit mask that can be used to reset the validation
     bits of a position's element.  Operationally, this creates a 32 bit mask
     of two 16 bit partitions:

       0000 0000 0000 0001 - 0000 0000 0000 0001

     Shifts the bits into the correct bit positions:

       0000 0000 0000 0100 - 0000 0000 0000 0100

     This then represents the TRUE state for this position's validated and
     invalid bits.  The bits are then inverted:

       1111 1111 1111 1011 - 1111 1111 1111 1011

     Then a bit-and assignment operation is used, clearing said validated and
     invalid bits while leaving the others untouched. */
  uint32_t index = to_partition_index(pos);
  size_t   block = index / 16;
  size_t   bit = index % 16;
  unsigned bit_mask = ~(((0x1 << 16) | 0x1) << bit);

  partitions[pos.partition].format_validated[block] &= bit_mask;
}  /* reset_validation_state */


inline ifc_Index an_ifc_module::read_index_from_heap(
                                          an_ifc_partition_kind heap_partition,
                                          ifc_Index_type        index)
/*
Read an ifc_Index from the heap indicated by heap_partition at the given index
into that partition.
*/
{
  ifc_Index result;

  read_prechecked_partition_element(heap_partition, index);
  GET_Index(result, /*from_header=*/FALSE);
  return result;
}  /* read_index_from_heap */


ifc_ChartIndex an_ifc_module::get_func_params_from_trait(ifc_DeclIndex decl)
/*
Find and return the index to the named function parameters corresponding to
decl, or 0 if not found.
*/
{
  ifc_ChartIndex                                params = (ifc_ChartIndex)0;
  Opt<an_ifc_Node<an_ifc_Trait_MsvcFuncParams>> opt_itmfp;

  find_trait<ifc_msvc_trait_named_func_params>(&opt_itmfp, this, decl);
  if (opt_itmfp.has_value()) {
    params = (*opt_itmfp)->params;
  }  /* if */
  return params;
}  /* get_func_params_from_trait */


ifc_MsvcTraits an_ifc_module::get_vendor_traits(ifc_DeclIndex decl)
/*
Find and return the associated vendor traits corresponding to the given decl.
If not found, return the appropriate trait to indicate "none".
*/
{
  ifc_MsvcTraits                                 result = ifc_MsvcTraits_None;
  Opt<an_ifc_Node<an_ifc_Trait_MsvcVendorTrait>> opt_itmvt;

  find_trait<ifc_msvc_trait_vendor_traits>(&opt_itmvt, this, decl);
  if (opt_itmvt.has_value()) {
    result = (*opt_itmvt)->trait;
  }  /* if */
  return result;
}  /* get_vendor_traits */


ifc_Sequence an_ifc_module::get_specialization_sequence_from_trait(
                                                            ifc_DeclIndex decl)
/*
Find and return the sequence of specializations corresponding to the template
decl, or an empty sequence if not found.
*/
{
  ifc_Sequence                                  result = {(ifc_Index)0,
                                                          (ifc_Cardinality)0};
  Opt<an_ifc_Node<an_ifc_Trait_Specialization>> opt_its;

  check_assertion(decl_tag(decl) == ifc_DeclSort_Template);
  find_trait<ifc_trait_specialization>(&opt_its, this, decl);
  if (opt_its.has_value()) {
    result = (*opt_its)->trait;
  }  /* if */
  return result;
}  /* get_specialization_sequence_from_trait */


a_template_ptr an_ifc_module::parse_cached_explicit_specialization(
                                     a_token_cache_ptr              cache,
                                     a_scope_ptr                    encl_scope,
                                     an_ifc_DeclSort_Specialization *decl)
/*
Parse the tokens corresponding to the given explicit specialization
declaration's (decl) cache, and return the corresponding explicit
specialization.  encl_scope is the scope containing the explicit specialization
declaration.
*/
{
  a_decl_parse_state dps;
  a_tmpl_decl_state  decl_state;
  a_token_kind       final_token = tok_semicolon;

#if DEBUG
  if (db_flag_is_set("ms_ifc_token_def")) {
    fprintf(f_debug, "Reconstituted explicit specialization declaration:\n");
    db_tokens(cache);
    fprintf(f_debug, "\n---------------------\n");
  }  /* if */
#endif /* DEBUG */
  check_assertion(decl->sort == ifc_SpecializationSort_Explicit);
  prepare_cached_template_parse(cache, encl_scope,
                                &dps, &decl_state, &final_token);
  {
    an_ms_extensions_parse tmp_parse;
    template_or_specialization_declaration_full(&decl_state,
                                                /*is_generic=*/FALSE,
                                                /*orig_dps=*/NULL);
  }
  finish_cached_template_parse(&final_token);
  record_pending_explicit_specialization(&dps, decl);
  return decl_state.il_template_entry;
}  /* parse_cached_explicit_specialization */


void an_ifc_module::record_pending_explicit_specialization(
                                          a_decl_parse_state             *dps,
                                          an_ifc_DeclSort_Specialization *decl)
/*
Record the presence of a pending explicit specialization declaration's (decl)
definition if any.  dps is the associated decl parse state from the parsing of
the declaration of the entity.
*/
{
  ifc_DeclIndex templated_decl_idx = decl->decl;

  check_assertion(decl->sort == ifc_SpecializationSort_Explicit);
  /* Read the partition for the templated declaration. */
  read_prechecked_partition_element(templated_decl_idx);
  switch (decl_tag(templated_decl_idx)) {
  case ifc_DeclSort_Scope:
    { /* We're reconstructing a class. */
      /* FIXME: We should be handling classes definitions lazily here. */
    }
    break;
  case ifc_DeclSort_Variable:
    { /* We're reconstructing a variable. */
      /* FIXME: We should be handling variable initializers lazily here. */
    }
    break;
  case ifc_DeclSort_Function:
    { /* We're reconstructing a function. */
      /* FIXME: Ideally checking for a symbol is unnecessary.  In practice,
         because we have code generation issues and we don't always create
         something that can be parsed, this prevents crashes. */
      if (dps->sym != NULL) {
        a_routine_ptr            rp = dps->sym->variant.routine.ptr;
        an_ifc_DeclSort_Function idsf, *idsfp;

        idsfp = get_DeclSort_Function(&idsf);
        if (idsfp->properties & ifc_ReachableProperties_Initializer) {
          record_pending_ifc_function_body(rp, templated_decl_idx, this);
        }  /* if */
      }
    }
    break;
  default:
    unexpected_condition_str("Unexpected DeclSort");
  }  /* switch */
}  /* record_pending_explicit_specialization */


static char *get_il_entity(a_symbol_ptr sym, a_byte_il_entry_kind *kind)
/*
Return the associated IL entity and update kind with the associated entity kind
for the given symbol (sym).
FIXME: This should likely be extracted as a general function for symbols.
*/
{
  char *il_entity = NULL;

  if (sym != NULL) {
    switch (sym->kind) {
    case sk_class_or_struct_tag:
      {
        il_entity = (char*)sym->variant.class_struct_union.type;
        *kind = iek_type;
      }
      break;
    case sk_routine:
    case sk_member_function:
      {
        il_entity = (char*)sym->variant.routine.ptr;
        *kind = iek_routine;
      }
      break;
    case sk_variable:
      {
        il_entity = (char*)sym->variant.variable.ptr;
        *kind = iek_variable;
      }
      break;
    default:
      unexpected_condition_str("Unexpected DeclSort");
    }  /* switch */
  }  /* if */
  return il_entity;
}  /* get_il_entity */


char *an_ifc_module::parse_cached_explicit_instantiation(
                                          a_token_cache_ptr              cache,
                                          an_ifc_DeclSort_Specialization *decl,
                                          a_byte_il_entry_kind           *kind)
/*
Parse the tokens corresponding to the given explicit instantiation
declaration's (decl) cache.  Return a pointer to the corresponding
explicitly-instantiated entity and update kind with the associated entity kind.
*/
{
  a_decl_parse_state dps;
  a_token_kind       final_token = tok_semicolon;
  ifc_SourceLocation template_locus = get_ifc_locus(decl);
  a_source_position  template_kw_pos;

#if DEBUG
  if (db_flag_is_set("ms_ifc_token_def")) {
    fprintf(f_debug, "Reconstituted explicit instantiation declaration:\n");
    db_tokens(cache);
    fprintf(f_debug, "\n---------------------\n");
  }  /* if */
#endif /* DEBUG */
  check_assertion(decl->sort == ifc_SpecializationSort_Instantiation);
  push_stop_token_stack();
  rescan_cached_tokens(cache);
  source_position_from_locus(&template_kw_pos, &template_locus);
  {
    an_ms_extensions_parse      tmp_parse;
    a_template_decl_options_set options = TDO_EXTERN;
    /* An explicit instantiation in a module means that the module clients can
       handle the equivalent of an "extern template" directive.  However, when
       importing a header unit (which should behave more like a #include) the
       explicit instantiation directive should remain an ordinary instantiation
       directive. */
    if (is_header_unit(this->assoc_module_info)) options = TDO_NO_OPTIONS;
    explicit_instantiation(&dps, options, &template_kw_pos);
  }
  finish_cached_template_parse(&final_token);
  return get_il_entity(dps.sym, kind);
}  /* parse_cached_explicit_instantiation */

#if CHECKING

void an_ifc_module::validate_is_class_type(ifc_TypeIndex type)
/*
Validate that the given type is a fundamental type representing a class.
*/
{
  an_ifc_TypeSort_Fundamental itsf, *itsfp;

  check_assertion(type_tag(type) == ifc_TypeSort_Fundamental);
  read_prechecked_partition_element(type);
  itsfp = get_TypeSort_Fundamental(&itsf);
  switch (itsfp->basis) {
    case ifc_TypeBasis_Class:
    case ifc_TypeBasis_Struct:
      break;
    default:
      unexpected_condition_str("Unexpected TypeSort");
  }  /* switch */
}  /* validate_is_class_type */

#endif /* CHECKING */

#if DEBUG

void an_ifc_module::db_ifc_file_header() const
/*
Display the contents of the IFC file header.
*/
{
  /* FIXME: Print checksum */
  (void)fprintf(f_debug, "  major_version = %hhu\n", header.major_version);
  (void)fprintf(f_debug, "  minor_version = %hhu\n", header.minor_version);
  (void)fprintf(f_debug, "  abi = %hhu\n", header.abi);
  (void)fprintf(f_debug, "  arch = %d\n", header.arch);
  (void)fprintf(f_debug, "  dialect = %u\n", header.dialect);
  (void)fprintf(f_debug, "  string_table_bytes = 0x%08x\n",
                                                    header.string_table_bytes);
  (void)fprintf(f_debug, "  string_table_size = %u\n",
                                                     header.string_table_size);
  (void)fprintf(f_debug, "  unit = %u\n", header.unit);
  (void)fprintf(f_debug, "  src_path = 0x%08x \"%s\"\n", header.src_path,
                                     get_string_at_offset(header.src_path));
  (void)fprintf(f_debug, "  global_scope = %u\n", header.global_scope);
  (void)fprintf(f_debug, "  toc = 0x%08x\n", header.toc);
  (void)fprintf(f_debug, "  partition_count = %u\n", header.partition_count);
  (void)fprintf(f_debug, "  internal = %d\n", header.internal);
}  /* db_ifc_file_header */


void an_ifc_module::db_ifc_scope(ifc_ScopeIndex scope)
/*
Display the contents of the specified scope.
*/
{
  an_ifc_Scope_Descriptor isd, *isdp;
  an_ifc_Scope_Member     ism, *ismp;
  unsigned int            i;

  /* A scope index of 0 indicates a missing scope, in which case there
     is nothing further to do. */
  if (scope != 0) {
    /* Scope indices are 1-based, so subtract one. */
    read_prechecked_partition_element(ifc_scope_desc, scope - 1);
    isdp = get_Scope_Descriptor(&isd);
    for (i = 0; i < isdp->cardinality; i++) {
      /* Re-enable access to scope.member partition (it changes during the
         loop). */
      read_prechecked_partition_element(ifc_scope_member, isdp->start + i);
      ismp = get_Scope_Member(&ism);
      db_ifc_declaration(ismp->index);
    }  /* for */
  }  /* if */
}  /* db_ifc_scope */


void an_ifc_module::db_ifc_declaration(ifc_DeclIndex decl)
/*
Display the contents of the specified declaration.
*/
{
  a_token_cache cache;

  clear_token_cache(&cache, /*reuseable=*/FALSE);
  cache_decl(&cache, decl);
  db_tokens(&cache);
}  /* db_ifc_declaration */

#endif /* DEBUG */

void ifc_modules_one_time_init(void)
/*
Do one-time initialization of static variables defined in this file.
*/
{
  /* Allocate a buffer for processing source file names.  These can get fairly
     long and this is shared across all IFC module processing so be generous
     with the initial allocation. */
  file_name_buffer = alloc_text_buffer(200);
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
#if DEBUG && EXPENSIVE_CHECKING
  debug_partition = NULL;
#endif /* DEBUG && EXPENSIVE_CHECKING */
  ifc_function_bodies = alloc_fe_of_type(an_ifc_function_body_map);
  construct(ifc_function_bodies, /*mask_width=*/10);
}  /* ifc_modules_init */

/*lint -restore*/ /* FIXME: temporary */
/*lint -restore*/

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* MICROSOFT_EXTENSIONS_ALLOWED && !STANDALONE_UTILITY_PROGRAM */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
