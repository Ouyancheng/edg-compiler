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
#include "literals.h"
#include "pch.h"
#include "symbol_ref.h"

#if MICROSOFT_EXTENSIONS_ALLOWED

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
The methods used by this file to access the contents of IFC modules (using
macro names) are not always lint-friendly, so disable some lint messages for
the duration of this file.
*/
/*lint -save -e534 -e641 -e1576 -e1502*/
/*lint -save -e1714*/ /* FIXME: temporarily disable "not referenced" */

/*
A control block used when turning IFC declarations into a textual
representation.
*/
struct a_str_control_block {
  a_module_ptr	module_info;
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
		operator_text_buffer;
			/* A text buffer used to prefix operator names. */

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


static void cache_token(a_token_cache_ptr     cache,
                        a_token_kind          tok,
                        a_source_position_ptr pos);

static void cache_identifier(a_token_cache_ptr     cache,
                             a_const_char          *name,
                             a_source_position_ptr pos);

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
                                            size_t length) const
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
                                                 size_t length) const
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
                                            ARG_UNUSED size_t length) const
/*
Initialize the state information used by "get_bytes", etc.  offset is the
offset from the start of the module file to be read.  length is its size, in
bytes.
*/
{
  fseek(f_module, offset, SEEK_SET);
}  /* init_byte_buffer */


inline void an_ifc_module::get_bytes_from_buffer(void   *entity,
                                                 size_t length) const
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
                                                       size_t length) const
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
                                     a_boolean header_bytes) const
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
#define GET_DelimiterSort(x, from_header)      GET_int(x, from_header)
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
                               (GET_Index((x).index, from_header), \
                                GET_SentenceIndex((x).head, from_header), \
                                GET_SentenceIndex((x).body, from_header), \
                                GET_SentenceIndex((x).attributes, from_header))

#define GET_KeywordSyntax(x, from_header) \
                                 (GET_SourceLocation((x).locus, from_header), \
                                  GET_KeywordSort((x).value, from_header), \
                                  pad(3))

/*
Create get_* functions (which "read" each entity into a structure) for each of
the IFC entities by setting the IFC_DECL macros appropriately and including
ifc_map.h.  Make sure to use the pointer that is returned by these functions
(and not the pointer that is passed as an argument).
*/

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

  static an_ifc_foo *get_foo(an_ifc_foo *ptr,
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
                /* Defaulted: */  a_boolean             fill_storage) const \
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
  }

/*
The variant for when the endianness of the IFC entity is guaranteed to be
little-endian.
*/
#define IFC_LE_DECL_START(name) \
  inline concat(an_ifc_, name) * an_ifc_module::concat(get_, name) ( \
                                  concat(an_ifc_, name) *ptr, \
                /* Defaulted: */  a_boolean             fill_storage) const \
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
                             a_boolean  fill_storage) const
  {
    GET_field1_type(ptr->field1);
    GET_field2_type(ptr->field2);
    return ptr;
  }
*/
#define IFC_DECL_START(name) \
  inline concat(an_ifc_, name) * an_ifc_module::concat(get_, name) ( \
                                  concat(an_ifc_, name) *ptr, \
                /* Defaulted: */  ARG_UNUSED a_boolean  fill_storage) const \
  {
#define IFC_DECL_FIELD(field, type) \
    concat(GET_, type)(ptr->field, /*from_header=*/FALSE);
#define IFC_DECL_END(name) \
    return ptr; \
  }

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

/*
Utility to return a "tag" given a partition (an_ifc_partition_kind) value
and the starting partition for the particular case (e.g., ifc_type_start
for TypeSort).  Relies on an_ifc_partition_kind being ordered properly (see
the comments there).
*/
#define get_tag_from_partition(partition, start) ((partition) - (start))


inline void an_ifc_module::issue_unsupported_node_diag(a_const_char      *node,
                                                       a_source_position *pos)
                                                                          const
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
    case ifc_DeclSort_ExplicitSpecialization:
                                       result = "ExplicitSpecialization";break;
    case ifc_DeclSort_ExplicitInstantiation:
                                       result = "ExplicitInstantiation"; break;
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
    case ifc_DeclSort_Last:             unexpected_condition(); break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* str_for_decl_tag */


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
      kind = opkind_basic;
      break;
    case ifc_MonadicOperator_PostIncrement:
    case ifc_MonadicOperator_PostDecrement:
      kind = opkind_post;
      break;
    case ifc_MonadicOperator_New:
    case ifc_MonadicOperator_Delete:
    case ifc_MonadicOperator_DeleteArray:
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
    case ifc_MonadicOperator_Expand:
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
    /* Read the IFC file header (which starts after the magic number). */
    init_byte_buffer(4, f_size - 4);
    get_File_Header(&header, /*fill_storage=*/TRUE);
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
    case ifc_decl_explicit_specialization:
      CHECK_SIZE(DeclSort_ExplicitSpecialization);
    case ifc_decl_explicit_instantiation:
      CHECK_SIZE(DeclSort_ExplicitInstantiation);
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
    case ifc_expr_new:
      CHECK_SIZE(ExprSort_New);
    case ifc_expr_delete:
      CHECK_SIZE(ExprSort_Delete);
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
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
#undef CHECK_SIZE
}

#else /* !CHECKING */

#define validate_partition_size(pp, kind) /* Nothing */

#endif /* CHECKING */

namespace {

constexpr ifc_Version supported_major_version = (ifc_Version)0;
constexpr ifc_Version supported_minor_version = (ifc_Version)32;

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

}  /* namespace */


a_boolean an_ifc_module::import(a_module_import_decl_ptr midp)
/*
Import an IFC module file described by midp.  The IFC file should already have
been confirmed to exist and the path stored in midp.
*/
{
  a_module_ptr mod = midp->module_info;
  unsigned int i;
  a_boolean    result = FALSE;

  check_assertion(midp->module_info->kind == (a_module_kind)mk_ifc);
  check_assertion(mod->name != NULL && mod->full_name != NULL);
  check_assertion(mod->module_interface == this);
  if (open_and_map_ifc_module_file(midp, /*issue_diag=*/TRUE)) {
    result = TRUE;
    assoc_module_info = mod;
    set_name(mod->name);
    /* Read the IFC file header (which starts after the magic number). */
    init_byte_buffer(4, f_size - 4);
    get_File_Header(&header, /*fill_storage=*/TRUE);
    if (!skip_module_version_check &&
        !check_ifc_version(header.major_version, header.minor_version)) {
      result = FALSE;
      pos_st_num2_diagnostic(es_catastrophe, ec_unsupported_ifc_file_version,
                             &midp->module_name_position, mod->full_name,
                             header.major_version, header.minor_version);
      close();
      goto done;
    }  /* if */
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
        (void)fprintf(f_debug,
            "partition %u \"%s\" offset 0x%08x cardinality %u entry_size %u\n",
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
        validate_partition_size(pp, map_ptr->kind);
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
        /* As a (hopefully) temporary measure, for each file referenced in
           the module, we need to determine the largest line number that will
           be seen in that file (we don't actually need the last line number in
           the file, just the largest one that will be seen in the source
           location, though the last number would do).  This number will be
           used (if needed) to increment the source sequence when the module is
           referenced to effectively reserve those source sequence numbers for
           the file. */
        for (size_t idx = 0, num_src_lines = get_num_entries(ifc_src_line);
             idx < num_src_lines; idx++) {
          an_ifc_Source_Line   isl, *islp;
          read_partition_at_index(ifc_src_line, idx);
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
    import_referenced_modules();
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
done:
  return result;
}  /* import */


a_module_import_decl_ptr an_ifc_module::transitive_import_module(
                                                const ifc_ModuleReference *ref)
                                                                          const
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


void an_ifc_module::import_referenced_modules() const
/*
Import all appropriate modules that have been referenced by this module.
Modules that have been imported but not re-exported are not imported at this
time, as their symbols are not visible except when referenced by symbols within
this module.
*/
{
  if (partitions[ifc_module_exported].name != NULL) {
    auto num_modules = get_num_entries(ifc_module_exported);
    read_partition_at_index(ifc_module_exported, 0);
    for (decltype(num_modules) idx = 0; idx < num_modules; ++idx) {
      ifc_ModuleReference imr;
      GET_ModuleReference(imr, /*from_header=*/FALSE);
      transitive_import_module(&imr);
    }  /* for */
  }  /* if */
}  /* import_referenced_modules */


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
  if (!open_and_map_ifc_module_file(midp, /*issue_diag=*/TRUE)) {
    /* This shouldn't happen (the PCH processing checks the existence and
       modification time of module files). */
    unexpected_condition();
  }  /* if */
}  /* ifc_modules_pch_reset */


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
  rescan_cached_tokens(cache);
  init_decl_parse_state(&dps);
  init_templ_decl_state(&decl_state, &dps);
  decl_state.pragmas_bound_to_template = extract_curr_construct_pragmas();
  decl_state.starting_token_sequence_number = curr_token_sequence_number;
  decl_state.final_token_ptr = &final_token;
  decl_state.enclosing_scope = encl_scope;
  template_or_specialization_declaration_full(&decl_state,
                                              /*is_generic=*/FALSE,
                                              /*orig_dps=*/NULL);
  if (curr_token != final_token) {
    expect_error();
    flush_tokens_without_warning();
  } else {
    (void)get_token();
    if (curr_token == tok_semicolon) {
      /* Microsoft sometimes adds a semicolon after the final closing brace. */
      (void)get_token();
    }  /* if */
  }  /* if */
  check_assertion(curr_token == tok_end_of_source);
  (void)get_token();
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
    pos_in_temp_text_buffer = 0;
    add_token_cache_to_string(cache);
    fprintf(stderr, "Reconstituted partial specialization declaration:\n%s\n"
                    "---------------------\n", temp_text_buffer);
  }  /* if */
#endif /* DEBUG */
  rescan_cached_tokens(cache);
  init_decl_parse_state(&dps);
  init_templ_decl_state(&decl_state, &dps);
  decl_state.pragmas_bound_to_template = extract_curr_construct_pragmas();
  decl_state.starting_token_sequence_number = curr_token_sequence_number;
  decl_state.final_token_ptr = &final_token;
  decl_state.enclosing_scope = encl_scope;
  template_or_specialization_declaration_full(&decl_state,
                                              /*is_generic=*/FALSE,
                                              /*orig_dps=*/NULL);
  if (curr_token != final_token) {
    expect_error();
    flush_tokens_without_warning();
  } else {
    (void)get_token();
    if (curr_token == tok_semicolon) {
      /* Microsoft sometimes adds a semicolon after the final closing brace. */
      (void)get_token();
    }  /* if */
  }  /* if */
  check_assertion(curr_token == tok_end_of_source);
  (void)get_token();
  return decl_state.il_template_entry;
}  /* parse_cached_partial_specialization */


/* FIXME: might be able to get rid of enumeration_type now that enums aren't
   deferred */
void an_ifc_module::process_ifc_declaration(
                                          a_module_entity_ptr mep,
                                          a_boolean           defer,
                                          a_type_ptr          enumeration_type)
                                                                          const

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
  a_partial_scope_stack_state
                           psss;
  char                     *il_entity = NULL;
  a_byte_il_entry_kind     kind = iek_none;
  a_boolean                scope_pushed = FALSE;
  a_boolean                skip_pop = FALSE;

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
    a_source_position saved_error_position = error_position;
    /* Prepare to read from the proper partition for this declaration. */
    read_partition_at_offset(mep->variant.ifc_partition, mep->file_offset);
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
          source_position_from_locus(&error_position, &idsvp->locus);
          init_locator_from_name(idsvp->name, (ifc_TextOffset)0, &idsvp->locus,
                                 &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here. */
            a_variable_ptr vp;
            init_dps(&dps, &idsvp->locus, idsvp->type, idsvp->traits,
                     ifc_MsvcTraits_None, idsvp->specifier, idsvp->access,
                     idsvp->alignment, &psss);
            if (mep->scope == NULL) {
              mep->scope = get_ifc_scope(idsvp->home_scope);
              scope_pushed = push_module_declaration_context(mep->scope);
            }  /* if */
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
            if (dps.alignment != 0) {
              vp->alignment = dps.alignment;
            }  /* if */
            restore_partial_scope_stack_if_necessary(&psss);
            il_entity = (char *)vp;
            kind = iek_variable;
          }  /* if */
        }
        break;
      case ifc_DeclSort_Function:
        { a_func_info_block        func_info;
          a_type_ptr               old_type;
          an_ifc_DeclSort_Function idsf, *idsfp;
          idsfp = get_DeclSort_Function(&idsf);
          source_position_from_locus(&error_position, &idsfp->locus);
          init_locator_from_name(idsfp->name, (ifc_TextOffset)0, &idsfp->locus,
                                 &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here. */
            a_routine_ptr rp;
            /* FIXME: There's a chicken-and-egg problem here when the return
               type is deduced and requires access to the class scope (e.g.,
               returning a lambda declared within the function). */
            init_dps(&dps, &idsfp->locus, idsfp->type, ifc_ObjectTraits_None,
                     ifc_MsvcTraits_None, idsfp->specifiers, idsfp->access,
                     (ifc_ExprIndex)0, &psss);
            if (mep->scope == NULL) {
              mep->scope = get_ifc_scope(idsfp->home_scope);
              scope_pushed = push_module_declaration_context(mep->scope);
            }  /* if */
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
      case ifc_DeclSort_Intrinsic:
        /* A builtin function declaration. */
        { a_func_info_block        func_info;
          a_type_ptr               old_type;
          a_routine_ptr            rp;
          an_ifc_DeclSort_Intrinsic idsi, *idsip;
          idsip = get_DeclSort_Intrinsic(&idsi);
          source_position_from_locus(&error_position, &idsip->locus);
          init_locator_from_name((ifc_NameIndex)0, idsip->name, &idsip->locus,
                                 &loc);
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
          source_position_from_locus(&error_position, &idssp->locus);
          /* Should be no unnamed namespaces or types. */
          check_assertion(idssp->name != 0);
          init_locator_from_name(idssp->name, (ifc_TextOffset)0, &idssp->locus,
                                 &loc);
          if (mep->scope == NULL) {
            mep->scope = get_ifc_scope(idssp->home_scope);
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
          /* Look at the "type" to determine whether we have a namespace or
             not. */
          check_assertion(type_tag(idssp->type) == ifc_TypeSort_Fundamental);
          read_partition_at_index(ifc_type_fundamental,
                                  type_value(idssp->type));
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
                  check_assertion((idssp->traits & ifc_ScopeTraits_Unnamed)
                                  == 0);
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
                  if (itsfp->basis == ifc_TypeBasis_Interface) {
                    class_type->variant.class_struct_union.is_interface = TRUE;
                    class_type->variant.class_struct_union.abstract = TRUE;
                  }  /* if */
                  tag_sym= enter_local_symbol(tag_kind, &loc,
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
      case ifc_DeclSort_Alias:
        { an_ifc_DeclSort_Alias idsta, *idstap;
          idstap = get_DeclSort_Alias(&idsta);
          source_position_from_locus(&error_position, &idstap->locus);
          init_locator_from_name((ifc_NameIndex)0, idstap->name,
                                 &idstap->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            ifc_TypeSort alias_tag = type_tag(idstap->type);
            if (alias_tag == ifc_TypeSort_Fundamental) {
              an_ifc_TypeSort_Fundamental itsf, *itsfp;
              /* Read the type to see what kind it is. */
              read_partition_at_index(idstap->type);
              itsfp = get_TypeSort_Fundamental(&itsf);
              if (itsfp->basis == ifc_TypeBasis_Typename) {
                /* A type alias; declare a typedef for this case. */
                if (mep->scope == NULL) {
                  mep->scope = get_ifc_scope(idstap->home_scope);
                  scope_pushed = push_module_declaration_context(mep->scope);
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
              an_ifc_TypeSort_Forall itsf, *itsfp;
              a_token_cache          cache;
              a_source_position      pos;
              read_partition_at_index(idstap->aliasee);
              itsfp = get_TypeSort_Forall(&itsf);
              source_position_from_locus(&pos, &idstap->locus);
              /* A template alias; declare a typedef for this case. */
              if (mep->scope == NULL) {
                mep->scope = get_ifc_scope(idstap->home_scope);
                scope_pushed = push_module_declaration_context(mep->scope);
              }  /* if */
              clear_token_cache(&cache, /*reuseable=*/FALSE);
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
        { an_ifc_DeclSort_Enumeration idse, *idsep;
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          a_boolean                   is_scoped_enum = FALSE;
          a_scope_ptr                 enum_scope = mep->scope;
          idsep = get_DeclSort_Enumeration(&idse);
          source_position_from_locus(&error_position, &idsep->locus);
          check_assertion(type_tag(idsep->type) == ifc_TypeSort_Fundamental);
          /* See if this is a scoped enumeration or not. */
          read_partition_at_index(ifc_type_fundamental,
                                  type_value(idsep->type));
          itsfp = get_TypeSort_Fundamental(&itsf);
          if (itsfp->basis == ifc_TypeBasis_Enum) {
            /* A classic enumeration.  Don't bother to defer in this case
               because each of the enumerators needs to be registered in the
               symbol table so they can be found. */
            defer = FALSE;
            scope_pushed = push_module_declaration_context(enum_scope);
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
          check_assertion(idsep->name != 0);
          init_locator_from_name((ifc_NameIndex)0, idsep->name, &idsep->locus,
                                 &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_type_ptr       enum_type;
            a_symbol_ptr     tag_sym;
            check_assertion(idsep->base != 0 && mep->scope != NULL);
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
      case ifc_DeclSort_Enumerator:
        { an_ifc_DeclSort_Enumerator idse, *idsep;
          idsep = get_DeclSort_Enumerator(&idse);
          source_position_from_locus(&error_position, &idsep->locus);
          check_assertion(idsep->name != 0);
          init_locator_from_name((ifc_NameIndex)0, idsep->name, &idsep->locus,
                                 &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_symbol_ptr   enum_con_sym;
            a_constant_ptr enum_con;
            a_type_ptr     enum_type;
            check_assertion(mep->scope != NULL);
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
      case ifc_DeclSort_Template:
        { an_ifc_DeclSort_Template idst, *idstp;
          idstp = get_DeclSort_Template(&idst);
          source_position_from_locus(&error_position, &idstp->locus);
          check_assertion(idstp->name != 0);
          init_locator_from_name(idstp->name, (ifc_TextOffset)0, &idstp->locus,
                                 &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_token_cache cache;
            a_non_type_kind nt_kind;
            a_type_ptr      type;
            a_boolean       do_forward_decl;
            a_boolean       saved_suppress_default_arguments;
            if (mep->scope == NULL) {
              mep->scope = get_ifc_scope(idstp->home_scope);
              scope_pushed = push_module_declaration_context(mep->scope);
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
            type = type_for_type_index(idstp->type, &nt_kind);
            do_forward_decl = (type != type_of_unknown_templ_param_nontype ||
                               idstp->entity.body == 0);
            saved_suppress_default_arguments = suppress_default_arguments;
            if (do_forward_decl) {
              /* It's possible that this entity has already been declared, in
                 which case a forward declaration isn't needed and can cause
                 problems (e.g., with default arguments being re-declared). */
              a_symbol_ptr sym = curr_scope_id_lookup(&loc, IDL_NO_OPTIONS);
              if (sym != NULL) {
                do_forward_decl = FALSE;
              } else if (idstp->entity.body != 0 &&
                         sentence_is_deleted(idstp->entity.body)) {
                /* If this is an "= delete" definition, do not issue a
                   "forward declaration" (i.e., without "= delete") since that
                   would be invalid.  Such definitions will have the
                   "Initializer" property set.  Variable templates can also
                   have that property, but they do not need "forward
                   declarations" either. */
                do_forward_decl = FALSE;
              }  /* if */
            }  /* if */
            if (do_forward_decl) {
              suppress_default_arguments = FALSE;
              clear_token_cache(&cache, /*reuseable=*/FALSE);
              cache_decl_template_declaration(&cache, idstp,
                                              /*add_semicolon=*/TRUE);
              terminate_token_cache(&cache);
              suppress_default_arguments = saved_suppress_default_arguments;
              il_entity = (char*)parse_cached_template(&cache, mep->scope);
              kind = iek_template;
            }  /* if */
            if (idstp->entity.body != 0) {
              /* There is a definition of the template.  Record the resolution
                 of the signature immediately so that the below processing
                 of the definition has access to it.  If we provided a forward
                 declaration of the entity, we will need to suppress any
                 default arguments on the definition. */
              if (do_forward_decl) {
                mep->entity.ptr = il_entity;
                mep->entity.kind = kind;
              }  /* if */
              suppress_default_arguments = do_forward_decl;
              clear_token_cache(&cache, /*reuseable=*/FALSE);
              cache_decl_template(&cache, idstp);
              terminate_token_cache(&cache);
              suppress_default_arguments = saved_suppress_default_arguments;
              il_entity = (char*)parse_cached_template(&cache, mep->scope);
              kind = iek_template;
            }  /* if */

            /* Compute the DeclIndex of the current template, then process
               the associated specializations. */
            ifc_DeclIndex decl_idx = decl_index_of(mep->variant.ifc_partition,
                                                   mep->file_offset);
            process_template_specializations(decl_idx);
          }  /* if */
        }
        break;
      case ifc_DeclSort_Parameter:
        { an_ifc_DeclSort_Parameter idsp, *idspp;
          a_type_ptr                param_type;
          a_template_param_ptr      param = NULL, *next_param;
          a_template_parameter_ptr  il_param = NULL;

          idspp = get_DeclSort_Parameter(&idsp);
          source_position_from_locus(&error_position, &idspp->locus);
          check_assertion(idspp->name != 0);
          init_locator_from_name((ifc_NameIndex)0, idspp->name, &idspp->locus,
                                 &loc);
          /* FIXME: constraint_expr = expr_for_expr_index(idspp->constraint);*/
          /* FIXME: init_expr = expr_for_expr_index(idspp->initializer); */
          if (idspp->pack) {
            /* FIXME: Currently unsupported. */
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
                                                  idspp->pack,
                                                  /*is_pack_element=*/FALSE,
                                                  /*is_non_initial=*/FALSE,
                                                  /*is_pack_expansion=*/FALSE,
                                                  &loc, error_type(),
                                                  curr_templ_decl_state);
              break;
            case ifc_ParameterSort_Type:
              /* FIXME: Handle unnamed parameters properly */
              param = decl_type_template_param(idspp->position, &loc,
                                               /*is_named=*/TRUE, idspp->pack,
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
                                                  idspp->pack,
                                                  /*is_pack_element=*/FALSE,
                                                  /*is_non_initial=*/FALSE,
                                                  /*is_pack_expansion*/FALSE,
                                                  &loc, param_type,
                                                  curr_templ_decl_state);
              break;
            case ifc_ParameterSort_Placeholder:
              /* FIXME: Currently unsupported. */
              issue_unsupported_node_diag("ParameterSort::Placeholder",
                                          &error_position);
              param = make_nontype_template_param(idspp->level,
                                                  idspp->position,
                                                  /*is_unnamed=*/FALSE,
                                                  idspp->pack,
                                                  /*is_pack_element=*/FALSE,
                                                  /*is_non_initial=*/FALSE,
                                                  /*is_pack_expansion=*/FALSE,
                                                  &loc, error_type(),
                                                  curr_templ_decl_state);
              break;
            case ifc_ParameterSort_Template:
              /* FIXME: Currently unsupported. */
              issue_unsupported_node_diag("ParameterSort::Template",
                                          &error_position);
              param = make_nontype_template_param(idspp->level,
                                                  idspp->position,
                                                  /*is_unnamed=*/FALSE,
                                                  idspp->pack,
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
          source_position_from_locus(&error_position, &idspsp->locus);
          init_locator_from_name(idspsp->name, (ifc_TextOffset)0,
                                 &idspsp->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_token_cache cache;
            if (mep->scope == NULL) {
              mep->scope = get_ifc_scope(idspsp->home_scope);
              scope_pushed = push_module_declaration_context(mep->scope);
            }  /* if */
            if (idspsp->entity.body != 0) {
              /* There is a definition of the partial specialization.  Record
                 the resolution of the signature immediately so that the below
                 processing of the definition has access to it. */
              clear_token_cache(&cache, /*reuseable=*/FALSE);
              cache_decl_partial_specialization(&cache, idspsp);
              terminate_token_cache(&cache);
              il_entity = (char*)parse_cached_partial_specialization(
                                                                   &cache,
                                                                   mep->scope);
              kind = iek_template;
            }  /* if */
          }  /* if */
        }
        break;
      case ifc_DeclSort_ExplicitSpecialization:
        { an_ifc_DeclSort_ExplicitSpecialization idses;
          get_DeclSort_ExplicitSpecialization(&idses);
          /* FIXME: Need a proper source position here. */
          error_position = null_source_position;
          goto unhandled;
        }
      case ifc_DeclSort_ExplicitInstantiation:
        { an_ifc_DeclSort_ExplicitInstantiation idsei;
          get_DeclSort_ExplicitInstantiation(&idsei);
          /* FIXME: Need a proper source position here. */
          error_position = null_source_position;
          goto unhandled;
        }
      case ifc_DeclSort_Concept:
        { an_ifc_DeclSort_Concept idsc, *idscp;
          idscp = get_DeclSort_Concept(&idsc);
          source_position_from_locus(&error_position, &idscp->locus);
          if (defer) {
            /* The concept isn't used yet.  For now, just register the
               presence of the concept name in the symbol table. */
            init_locator_from_name((ifc_NameIndex)0, idscp->name,
                                   &idscp->locus, &loc);
            defer_symbol_creation(mep, &loc);
          } else {
            /* Create a definition for the concept and scan it. */
            a_token_sequence_number  saved_tsn = curr_token_sequence_number;
            a_token_cache            cache;
            if (mep->scope == NULL) {
              mep->scope = get_ifc_scope(idscp->home_scope);
            }  /* if */
            /* Activate the parent scope if needed. */
            a_boolean  must_pop = push_module_declaration_context(mep->scope);
            clear_token_cache(&cache, /*reuseable=*/FALSE);
            /* Generate the template parameter list. */
            cache_token(&cache, tok_template, &null_source_position);
            cache_chart(&cache, idscp->chart, &idscp->locus);
            /* Generate "concept <concept-name>". */
            cache_sentence(&cache, idscp->head);
            /* Generate "= <constraint-expression> ;". */
            cache_sentence(&cache, idscp->body);
            /* Terminate the definition cache and parse it. */
            terminate_token_cache(&cache);
            (void)parse_cached_template(&cache, mep->scope);
            /* Restore the original context. */
            pop_module_declaration_context(must_pop);
            curr_token_sequence_number = saved_tsn;
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
            init_locator_from_name((ifc_NameIndex)0, idsudp->name,
                                   &idsudp->locus, &loc);
            defer_symbol_creation(mep, &loc);
          } else {
            source_position_from_locus(&error_position, &idsudp->locus);
            if (decl_tag(idsudp->resolution) == ifc_DeclSort_Tuple) {
              /* Multiple declaration case. */
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
        { an_ifc_DeclSort_Tuple idst;
          get_DeclSort_Tuple(&idst);
          /* FIXME: Need a proper source position here. */
          error_position = null_source_position;
          goto unhandled;
        }
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
      case ifc_DeclSort_Last:
        unexpected_condition();
        break;
      default_is_unexpected_str("Unexpected DeclSort");
    }  /* switch */
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


void an_ifc_module::complete_definition_of_module_class(
                                                       a_module_entity_ptr mep)
                                                                          const
/*
Complete the definition of the class referred to by mep (if needed).
*/
{
  a_type_ptr            class_type = (a_type_ptr)mep->entity.ptr;
  an_ifc_DeclSort_Scope idss, *idssp;
  a_token_cache         cache;

  check_assertion(mep->entity.kind == (an_il_entry_kind)iek_type &&
                  class_type != NULL);
  read_partition_at_offset(mep->variant.ifc_partition, mep->file_offset);
  idssp = get_DeclSort_Scope(&idss);
#if DEBUG
  /* FIXME: This section becomes irrelevant once str_ifc_class_definition is
     switched to use the token caching methods internally, and should be
     removed once that switch is done.  Until then, this serves as a useful
     tool to compare behavior changes with older versions where class
     definition used the str_ifc_class_definition function. */
  if (db_flag_is_set("ms_ifc_token_def")) {
    a_str_control_block scb;
    a_module_ptr        mod = mep->module_info;
    a_text_buffer_ptr   buffer = alloc_text_buffer(1024);

    reset_text_buffer(buffer);
    clear_str_control_block(&scb, mod, buffer);
    scb.is_generated_code = TRUE;
    str_ifc_class_definition(idssp, &scb);
    add_char_to_text_buffer(buffer, ';');
    add_char_to_text_buffer(buffer, '\0');
    fprintf(f_debug, "Class def using str_ifc_class_definition:\n%s\n"
                     "-----------------------------------------\n",
            buffer->buffer);
  }  /* if */
#endif /* DEBUG */
  if (class_type->incomplete && idssp->initializer != 0) {
    a_template_decl_info_ptr tdip;
    a_symbol_ptr             class_sym = symbol_for(class_type);
    a_scope_depth            saved_non_local_class_fixup_depth =
                                                   non_local_class_fixup_depth;
    a_source_position        saved_error_position = error_position;
    a_boolean                scope_pushed = FALSE;

    scope_pushed = push_module_declaration_context(mep->scope);
    source_position_from_locus(&error_position, &idssp->locus);
    clear_token_cache(&cache, /*reuseable=*/FALSE);
    cache_decl_class(&cache, idssp);
    terminate_token_cache(&cache);
#if DEBUG
    if (db_flag_is_set("ms_ifc_token_def")) {
      fprintf(f_debug, "Reconstituted class definition:\n");
      db_tokens(&cache);
      fprintf(f_debug, "\n---------------------\n");
    }  /* if */
#endif /* DEBUG */
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
    process_deferred_class_fixups_and_instantiations(
                                                   /*for_instantiation=*/TRUE);
    if (curr_token != tok_semicolon) {
      expect_error();
      flush_tokens_without_warning();
    } else {
      (void)get_token();
    }  /* if */
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
  (void)fprintf(f_debug, "kind: mk_ifc\n");
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

#endif /* DEBUG */

a_boolean an_ifc_module::open_and_map_ifc_module_file(
                                           a_module_import_decl_ptr midp,
                                           a_boolean                issue_diag)
/*
Open the module file and map it into the process' address space.  Note that
this is also used after restoring from a PCH file.  Returns TRUE if the
module file was successfully opened and FALSE (with an error message if
issue_diag == TRUE) otherwise.
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
  a_const_char *name;

  bool operator<(const an_ifc_partition_name& other) const
  {
    return strcmp(name, other.name) < 0;
  } /* operator< */

  bool operator==(const an_ifc_partition_name& other) const
  {
    return strcmp(name, other.name) == 0;
  } /* operator== */
};  /* an_ifc_partition_name */
}  /* namespace */


#if EXPENSIVE_CHECKING
#if DEBUG
template<>
void db_f_print_t(FILE *stream, const an_ifc_partition_name &partition_name)
/*
Provide a generic printer for an_ifc_partition_name. Note that this
specialization would normally be predeclared in util.h, however as
an_ifc_partition_name has internal linkage it's safe to declare the
specialization here.
*/
{
  fprintf(stream, partition_name.name);
} /* db_f_print_t */
#endif /* DEBUG */


static void validate_ifc_partition_map(
                                an_ifc_partition_map *map_ptr,
                                uint32_t             num_searchable_partitions)
/*
Validate that the state of the partition map for binary search.
*/
{
  uint32_t num_partitions = ifc_last + 1;
  uint32_t num_nameless_partitions = 0;

  for (uint32_t i = 0; i < num_partitions; ++i) {
    if (map_ptr->name == NULL) {
      ++num_nameless_partitions;
    } else {
      /* Assert that we have no nameless partitions to ensure, we didn't see a
         partition without a name followed by a partition with a name.  This in
         effect verifies any nameless partitions are at the end of the map. */
      check_assertion(num_nameless_partitions == 0);
    }
    ++map_ptr;
  }  /* for */
  /* Verify that we're skipping the correct number of nameless partitions. */
  check_assertion(num_partitions ==
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
  uint32_t num_partitions = ifc_last - 3;
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


void an_ifc_module::process_scope_member_sequence(ifc_Sequence seq) const
/*
Process a sequence of IFC scope member declarations.
*/
{
  /* Guard this logic behind a feature flag to prevent aborts. */
#if IFC_PROCESS_TEMPLATE_SPECIALZIATIONS
  for (size_t idx = 0; idx < seq.cardinality; ++idx) {
    an_ifc_Scope_Member ism, *ismp;
    a_module_entity_ptr dmep;

    /* Load the scope member. */
    read_partition_at_index(ifc_scope_member, seq.start + idx);
    ismp = get_Scope_Member(&ism);

    /* Get the associated IFC module entity pointer, and then use it to process
       this scope member via process_ifc_declaration. */
    dmep = get_ifc_module_entity_ptr(ismp->index);
    process_ifc_declaration(dmep, /*defer=*/FALSE, (a_type_ptr)NULL);
  }
#endif /* IFC_PROCESS_TEMPLATE_SPECIALZIATIONS */
}  /* process_scope_member_sequence */


void an_ifc_module::process_template_specializations(ifc_DeclIndex  decl_idx)
                                                                          const
/*
Process any template specializations of the template represented by DeclIndex.
*/
{
  size_t num_trait_specializations = get_num_entries(ifc_trait_specialization);
  /* Provide a value function for retrieving the trait specialization at the
     given trait specialization partition index. */
  auto value_lambda = [this](ptrdiff_t idx) {
    an_ifc_Trait_Specialization its, *itsp;

    read_partition_at_index(ifc_trait_specialization, idx);
    itsp = get_Trait_Specialization(&its);
    return itsp->decl;
  };
  /* Get the partition index (if any) for the given decl index (decl_idx). */
  ptrdiff_t partition_idx = bin_search(num_trait_specializations, decl_idx,
                                       value_lambda);

  if (partition_idx != -1) {
    /* A trait specialization was found for the given decl index (decl_idx).
       Load the trait specialization (again) to retrieve the trait, then
       processing the sequence of specializations with
       process_scope_member_sequence.

       Note that the implementation of bin_search at the time of writing does
       not guarantee that the last read value is the one who's index is
       returned.  Thus, we cannot (as an optimization) share a variable with
       the value_lambda to prevent double reading (though this is unlikely to
       ever represent a significant cost in terms of CPU time). */
    an_ifc_Trait_Specialization its, *itsp;

    read_partition_at_index(ifc_trait_specialization, partition_idx);
    itsp = get_Trait_Specialization(&its);
    process_scope_member_sequence(itsp->trait);
  }
}  /* process_template_specializations */


void an_ifc_module::process_ifc_scope(ifc_ScopeIndex scope_index,
                                      a_scope_ptr    scope) const
/*
Process the IFC scope specified by scope_index in the module file.  All items
in the IFC scope will be members of scope and their definitions will be
deferred until they are referenced.
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
    read_partition_at_index(ifc_scope_desc, scope_index - 1);
    isdp = get_Scope_Descriptor(&isd);
    for (i = 0; i < isdp->cardinality; i++) {
      /* Re-enable access to scope.member partition (it changes during the
         loop). */
      read_partition_at_index(ifc_scope_member, isdp->start + i);
      ismp = get_Scope_Member(&ism);
      dmep = get_ifc_module_entity_ptr(ismp->index);
      dmep->scope = scope;
      process_ifc_declaration(dmep, /*defer=*/TRUE, (a_type_ptr)NULL);
    }  /* for */
    pop_module_declaration_context(scope_pushed);
  }  /* if */
}  /* process_ifc_scope */


size_t an_ifc_module::get_num_entries(an_ifc_partition_kind partition) const
/*
Return the number of entries in a given partition.
*/
{
  size_t num_entries = 0;

  /* If there is an entry size defined, calculate the number of entries. */
  if (partitions[partition].entry_size != 0) {
    num_entries = partitions[partition].size /
                  partitions[partition].entry_size;
  }
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
  a_module_entity_ptr mep;

  mep = get_module_entity_ptr(assoc_module_info,
                              file_offset_of(partition, index));
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


a_module_entity_ptr an_ifc_module::get_and_process_ifc_decl_from_other_module(
                                          const an_ifc_DeclSort_Reference *ref)
                                                                          const
/*
Given a DeclSort::Reference to another module, get and return the
fully-processed entity from the referenced module.
*/
{
  a_module_entity_ptr       dmep;
  a_module_import_decl_ptr  midp;
  an_ifc_module             *iface;

  midp = transitive_import_module(&ref->unit);
  check_assertion(midp != NULL);
  iface = (an_ifc_module*)midp->module_info->module_interface;
  check_assertion(iface != NULL);
  dmep = iface->get_ifc_module_entity_ptr(ref->local_index);
  iface->process_ifc_declaration(dmep, /*defer=*/FALSE,
                                 /*enumeration_type=*/NULL);
  return dmep;
}  /* get_and_process_ifc_decl_from_other_module */


a_module_entity_ptr an_ifc_module::get_and_process_ifc_decl_from_other_module(
                                                           ifc_DeclIndex index)
                                                                          const
/*
Given an index for a DeclSort::Reference, get and return the fully-processed
entity from the referenced module.
*/
{
  an_ifc_DeclSort_Reference idsr, *idsrp;

  read_partition_at_index(index);
  idsrp = get_DeclSort_Reference(&idsr);
  return get_and_process_ifc_decl_from_other_module(idsrp);
}  /* get_and_process_ifc_decl_from_other_module */


a_scope_ptr an_ifc_module::get_ifc_scope(ifc_DeclIndex scope_index) const
/*
Given a scope index find and return the associated scope.
*/
{
  a_scope_ptr result;

  if (scope_index == 0) {
    result = il_header.primary_scope;
  } else {
    a_module_entity_ptr mep = get_ifc_module_entity_ptr(scope_index);
    process_ifc_declaration(mep, /*defer=*/FALSE, /*enumeration_type=*/NULL);
    if (mep->entity.kind == (a_byte_il_entry_kind)iek_type) {
      a_type_ptr tp = (a_type_ptr)mep->entity.ptr;
      /* We need the scope associated with this type.  Given its scope was
         referenced, it must be complete. */
      complete_class_type_is_needed(tp, /*subst_err*/NULL);
      check_assertion(!tp->incomplete);
    }  /* if */
    result = get_assoc_scope_of_il_entry(mep->entity.ptr,
                                         (an_il_entry_kind)mep->entity.kind);
  }  /* if */
  return result;
}  /* get_ifc_scope */


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


a_type_ptr an_ifc_module::type_for_type_index(ifc_TypeIndex   type_index,
                                              a_non_type_kind *kind) const
/*
Return the type that corresponds to the specified TypeIndex.  If there is no
corresponding type, set *kind to the appropriate non-type kind and return NULL.
*/
{
  a_type_ptr          result = NULL;
  a_module_entity_ptr mep = get_ifc_module_entity_ptr(type_index);
  ifc_TypeSort        tag;

  if (kind != NULL) {
    *kind = ntk_none;
  }  /* if */
  if (mep->entity.ptr != NULL) {
    /* There is already an entry for this; return it. */
    check_assertion(mep->entity.kind == iek_type);
    result = (a_type_ptr)mep->entity.ptr;
  } else {
    /* Prepare to read from the proper partition for this type. */
    read_partition_at_offset(mep->variant.ifc_partition, mep->file_offset);
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
              read_partition_at_index(ifc_type_tuple,
                                      type_value(itsfp->source));
              itstp = get_TypeSort_Tuple(&itst);
              for (i = 0; i < itstp->cardinality; i++) {
                ifc_TypeIndex ti;
                read_partition_at_index(ifc_heap_type,
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
              /* FIXME: Currently unsupported. */
              issue_unsupported_node_diag("DeclSort::Parameter",
                                          &error_position);
              result = error_type();
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
          read_partition_at_index(itssp->expr);
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
                                                                          const
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

  read_partition_at_index(expr_index);
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
      { an_ifc_ExprSort_Read iesr, *iesrp;
        a_token_cache        cache;
        a_source_position    pos;

        iesrp = get_ExprSort_Read(&iesr);
        kind = tak_nontype;
        /* Use the parameter type instead of the ExprSort::Read type, as the
           latter may not match (e.g., int& parameter type, int ExprSort::Read
           type). */
        check_assertion(param->kind == (a_template_parameter_kind)tpk_nontype);
        type = param->variant.nontype.constant->type;
        source_position_from_locus(&pos, &iesrp->locus);
        clear_token_cache(&cache, /*reuseable=*/FALSE);
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
      { an_ifc_ExprSort_Monad iesm, *iesmp;
        a_token_cache         cache;
        a_source_position     pos;

        iesmp = get_ExprSort_Monad(&iesm);
        kind = tak_nontype;
        type = type_for_type_index(iesmp->type, /*kind=*/NULL);
        source_position_from_locus(&pos, &iesmp->locus);
        clear_token_cache(&cache, /*reuseable=*/FALSE);
        cache_operator(&cache, iesmp->op, &iesmp->locus);
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
                                                                          const
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
  read_partition_at_index(templ_id->primary);
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
    read_partition_at_index(templ_id->arguments);
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


void an_ifc_module::source_position_from_locus(a_source_position  *pos,
                                               ifc_SourceLocation *locus) const
/*
Map the IFC locus source position information into the source position at pos.
*/
{
  an_ifc_Source_Line   isl, *islp;

  read_partition_at_index(ifc_src_line, locus->line);
  islp = get_Source_Line(&isl);
  check_assertion(name_tag(islp->file) == ifc_NameSort_SourceFile);
  if (islp->line == 0 && name_value(islp->file) == 0) {
    /* Visual Studio uses this to indicate that the entity doesn't have
       a source location (e.g., builtins), so use a null position. */
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
      file_name = string_from_name_index(islp->file,
                                         (a_symbol_locator *)NULL);
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
}  /* source_position_from_locus */


a_const_char *an_ifc_module::string_from_name_index(
                                                   ifc_NameIndex    name_index,
                                                   a_symbol_locator *loc) const
/*
Return the string referenced by name_index.  The returned string may not be in
the IL (it may be a pointer to an mmap'ed memory region or a pointer to a local
static buffer), so the caller should copy it if necessary.  If non-NULL, fields
(like is_operator_name) in *loc are updated accordingly.
*/
{
  a_const_char         *result = NULL, *prefix = NULL;
  ifc_NameSort         tag = name_tag(name_index);

  if (tag == ifc_NameSort_Identifier) {
    /* NameSort::Identifiers just refer to the string table. */
    result = get_string_at_offset((ifc_TextOffset)name_value(name_index));
  } else {
    read_partition_at_index(name_index);
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
            result = get_string_at_offset(insop->encoded);
          }  /* if */
        }
        break;
      case ifc_NameSort_Conversion:
        { an_ifc_NameSort_Conversion insc, *inscp;
          a_type_ptr                 target_type;
          inscp = get_NameSort_Conversion(&insc);
          prefix = "operator ";
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
          } else {
            /* Microsoft doesn't use a space after the "operator" string. */
            prefix = "operator";
          }  /* if */
        }
        break;
      case ifc_NameSort_Template:
        { an_ifc_NameSort_Template inst;
          get_NameSort_Template(&inst);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("NameSort::Template", &error_position);
          result = "<error-name>";
        }
        break;
      case ifc_NameSort_Specialization:
        { an_ifc_NameSort_Specialization inss;
          get_NameSort_Specialization(&inss);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("NameSort::Specialization",
                                      &error_position);
          result = "<error-name>";
        }
        break;
      case ifc_NameSort_Guide:
        { an_ifc_NameSort_Guide insg;
          get_NameSort_Guide(&insg);
          /* FIXME: Currently unsupported. */
          issue_unsupported_node_diag("NameSort::Guide", &error_position);
          result = "<error-name>";
        }
        break;
      case ifc_NameSort_Identifier:
      case ifc_NameSort_Last:
        unexpected_condition();
        break;
      default_is_unexpected();
    }  /* switch */
  }  /* if */
  if (operator_text_buffer == NULL) {
    operator_text_buffer = alloc_text_buffer(20);
  }  /* if */
  reset_text_buffer(operator_text_buffer);
  /* If a prefix was specified, add it now. */
  if (prefix != NULL) {
    add_string_to_text_buffer(operator_text_buffer, prefix);
  }  /* if */
  add_string_to_text_buffer(operator_text_buffer, result);
  add_char_to_text_buffer(operator_text_buffer, '\0');
  result = operator_text_buffer->buffer;
  return result;
}  /* string_from_name_index */


a_const_char *an_ifc_module::name_from_decl(ifc_DeclIndex decl) const
/*
Given a declaration, return the name associated with that declaration.
*/
{
  a_const_char *result = NULL;
  ifc_DeclSort tag = decl_tag(decl);

  read_partition_at_index(decl);
  switch (tag) {
    case ifc_DeclSort_VendorExtension:
      issue_unsupported_node_diag("DeclSort::VendorExtension",
                                  &error_position);
      break;
    case ifc_DeclSort_Enumerator:
      { an_ifc_DeclSort_Enumerator idse, *idsep;
        idsep = get_DeclSort_Enumerator(&idse);
        result = get_string_at_offset(idsep->name);
      }
      break;
    case ifc_DeclSort_Variable:
      { an_ifc_DeclSort_Variable idsv, *idsvp;
        idsvp = get_DeclSort_Variable(&idsv);
        result = string_from_name_index(idsvp->name, /*loc=*/NULL);
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
      }
      break;
    case ifc_DeclSort_Bitfield:
      { an_ifc_DeclSort_Bitfield idsb, *idsbp;
        idsbp = get_DeclSort_Bitfield(&idsb);
        result = get_string_at_offset(idsbp->name);
      }
      break;
    case ifc_DeclSort_Scope:
      { an_ifc_DeclSort_Scope idss, *idssp;
        idssp = get_DeclSort_Scope(&idss);
        result = string_from_name_index(idssp->name, /*loc=*/NULL);
      }
      break;
    case ifc_DeclSort_Enumeration:
      { an_ifc_DeclSort_Enumeration idse, *idsep;
        idsep = get_DeclSort_Enumeration(&idse);
        result = get_string_at_offset(idsep->name);
      }
      break;
    case ifc_DeclSort_Alias:
      { an_ifc_DeclSort_Alias idsa, *idsap;
        idsap = get_DeclSort_Alias(&idsa);
        result = get_string_at_offset(idsap->name);
      }
      break;
    case ifc_DeclSort_Temploid:
      unexpected_condition_str("DeclSort::Temploid does not have a name");
      break;
    case ifc_DeclSort_Template:
      { an_ifc_DeclSort_Template idst, *idstp;
        idstp = get_DeclSort_Template(&idst);
        result = string_from_name_index(idstp->name, /*loc=*/NULL);
      }
      break;
    case ifc_DeclSort_PartialSpecialization:
      { an_ifc_DeclSort_PartialSpecialization idsps, *idspsp;
        idspsp = get_DeclSort_PartialSpecialization(&idsps);
        result = string_from_name_index(idspsp->name, /*loc=*/NULL);
      }
      break;
    case ifc_DeclSort_ExplicitSpecialization:
      { an_ifc_DeclSort_ExplicitSpecialization idses, *idsesp;
        idsesp = get_DeclSort_ExplicitSpecialization(&idses);
        /* FIXME: Is this reachable? */
        result = name_from_decl(idsesp->decl);
      }
      break;
    case ifc_DeclSort_ExplicitInstantiation:
      { an_ifc_DeclSort_ExplicitInstantiation idsei, *idseip;
        idseip = get_DeclSort_ExplicitInstantiation(&idsei);
        /* FIXME: Is this reachable? */
        result = name_from_decl(idseip->decl);
      }
      break;
    case ifc_DeclSort_Concept:
      { an_ifc_DeclSort_Concept idsc, *idscp;
        idscp = get_DeclSort_Concept(&idsc);
        result = get_string_at_offset(idscp->name);
      }
      break;
    case ifc_DeclSort_Function:
      { an_ifc_DeclSort_Function idsf, *idsfp;
        idsfp = get_DeclSort_Function(&idsf);
        result = string_from_name_index(idsfp->name, /*loc=*/NULL);
      }
      break;
    case ifc_DeclSort_Method:
      { an_ifc_DeclSort_Method idsm, *idsmp;
        idsmp = get_DeclSort_Method(&idsm);
        result = string_from_name_index(idsmp->name, /*loc=*/NULL);
      }
      break;
    case ifc_DeclSort_Constructor:
      { an_ifc_DeclSort_Constructor idsc, *idscp;
        idscp = get_DeclSort_Constructor(&idsc);
        result = name_from_decl(idscp->home_scope);
      }
      break;
    case ifc_DeclSort_InheritedConstructor:
      { an_ifc_DeclSort_InheritedConstructor idscic, *idscicp;
        idscicp = get_DeclSort_InheritedConstructor(&idscic);
        result = name_from_decl(idscicp->home_scope);
      }
      break;
    case ifc_DeclSort_Destructor:
      { an_ifc_DeclSort_Destructor idsd, *idsdp;
        idsdp = get_DeclSort_Destructor(&idsd);
        result = name_from_decl(idsdp->home_scope);
      }
      break;
    case ifc_DeclSort_Reference:
      { an_ifc_DeclSort_Reference idsr, *idsrp;
        idsrp = get_DeclSort_Reference(&idsr);
        result = name_from_other_module_decl(idsrp);
      }
      break;
    case ifc_DeclSort_UsingDeclaration:
      { an_ifc_DeclSort_UsingDeclaration idsud, *idsudp;
        idsudp = get_DeclSort_UsingDeclaration(&idsud);
        result = get_string_at_offset(idsudp->name);
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
        result = name_from_decl(idsep->operand);
      }
      break;
    case ifc_DeclSort_DeductionGuide:
      { an_ifc_DeclSort_DeductionGuide idsdg;
        get_DeclSort_DeductionGuide(&idsdg);
        /* FIXME: Currently unsupported. */
        issue_unsupported_node_diag("DeclSort::DeductionGuide",
                                    &error_position);
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
        result = name_from_decl(declidx);
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
      }
      break;
    case ifc_DeclSort_Property:
      { an_ifc_DeclSort_Property idsp, *idspp;
        idspp = get_DeclSort_Property(&idsp);
        result = name_from_decl(idspp->member);
      }
      break;
    case ifc_DeclSort_OutputSegment:
      { an_ifc_DeclSort_OutputSegment idsos, *idsosp;
        idsosp = get_DeclSort_OutputSegment(&idsos);
        result = get_string_at_offset(idsosp->name);
      }
      break;
    case ifc_DeclSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected DeclSort");
  }  /* switch */
  check_assertion(result != NULL);
  return result;
}  /* name_from_decl */


a_const_char *an_ifc_module::name_from_other_module_decl(
                                          const an_ifc_DeclSort_Reference *ref)
                                                                          const
/*
Given a DeclSort::Reference to another module, get and return the name of the
referenced entity.
*/
{
  a_module_import_decl_ptr  midp;
  an_ifc_module             *iface;

  midp = transitive_import_module(&ref->unit);
  check_assertion(midp != NULL);
  iface = (an_ifc_module*)midp->module_info->module_interface;
  check_assertion(iface != NULL);
  return iface->name_from_decl(ref->local_index);
}  /* name_from_other_module_decl */


void an_ifc_module::init_dps(a_decl_parse_state          *dps,
                             ifc_SourceLocation          *locus,
                             ifc_TypeIndex               type_index,
                             ifc_ObjectTraits            traits,
                             ifc_MsvcTraits              msvc_traits,
                             ifc_BasicSpecifiers         specifiers,
                             ifc_Access                  access,
                             ifc_ExprIndex               alignment,
                             a_partial_scope_stack_state *psssp) const
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


void an_ifc_module::init_locator_from_name(ifc_NameIndex      name_index,
                                           ifc_TextOffset     text_offset,
                                           ifc_SourceLocation *locus,
                                           a_symbol_locator   *loc) const
/*
Initialize the locator specified by *loc.  The name of the entity is either
given by name_index or text_offset, whichever is non-zero.  The source position
is given by locus.
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


void an_ifc_module::unsigned_integer_for_expr_index(
                                                   ifc_ExprIndex    expr_index,
                                                   an_integer_value *value)
                                                                          const
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
  read_partition_at_index(expr_index);
  ieslp = get_ExprSort_Literal(&iesl);
  switch (literal_tag(ieslp->value)) {
    case ifc_LiteralSort_Immediate:
      /* An immediate literal (30 bits or less). */
      set_unsigned_integer_value(value,
                           (a_host_large_unsigned)literal_index(ieslp->value));
      break;
    case ifc_LiteralSort_Integer:
      /* An integer larger than 30 bits. */
      read_partition_at_index(ifc_const_i64,
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
                                                                          const
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

  /* Prepare to read from the proper partition for this expression. */
  read_partition_at_index(expr_index);
  switch (tag) {
    case ifc_ExprSort_Literal:
      { an_ifc_ExprSort_Literal iesl, *ieslp;
        a_type_ptr              constant_type, stripped_type;
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
                set_unsigned_integer_constant(cp,
                            (a_host_large_unsigned)literal_index(ieslp->value),
                            (an_integer_kind)ik_unsigned_int);
              } else {
                check_assertion(constant_type != NULL);
                stripped_type = skip_typerefs(constant_type);
                check_assertion(stripped_type->kind ==
                                                      (a_type_kind)tk_integer);
                set_unsigned_integer_constant(cp, value,
                                      stripped_type->variant.integer.int_kind);
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
              read_partition_at_index(ifc_const_f64,
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
        a_module_entity_ptr              mep;
        a_type_ptr                       tp;
        a_boolean                        is_constant = FALSE;
        a_dynamic_init_ptr               dip = NULL;
        an_expr_stack_entry              expr_stack_entry;

        iesptvp = get_ExprSort_ProductTypeValue(&iesptv);
        mep = get_ifc_module_entity_ptr(iesptvp->class_decl);
        process_ifc_declaration(mep, /*defer=*/FALSE,
                                /*enumeration_type=*/NULL);
        check_assertion(mep->entity.kind == (an_il_entry_kind)iek_type);
        tp = (a_type_ptr)mep->entity.ptr;
        complete_type_is_needed(tp);
        /* FIXME: Are there any other ways to get here? */
        push_expr_stack(ek_init_constant, &expr_stack_entry,
                        /*force_object_lifetime=*/FALSE,
                        /*suppress_object_lifetime=*/FALSE);
        value_initialization(tp, /*copy_init_context=*/FALSE,
                             /*generate_il=*/TRUE, &error_position,
                             /*ctor_called=*/NULL, &is_constant, &dip, &cp,
                             /*is=*/NULL, /*error_detected=*/NULL);
        pop_expr_stack();
        check_assertion(is_constant);
      }
      break;
    case ifc_ExprSort_NamedDecl:
      { an_ifc_ExprSort_NamedDecl iesnd, *iesndp;
        iesndp = get_ExprSort_NamedDecl(&iesnd);
        cp = constant_for_named_decl(iesndp);
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
done:
  return cp;
}  /* constant_for_expr_index */


a_constant_ptr an_ifc_module::constant_for_named_decl(
                                       an_ifc_ExprSort_NamedDecl *iesndp) const
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
  read_partition_at_index(iesndp->resolution);
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
}


a_boolean an_ifc_module::is_class_scope(ifc_DeclIndex scope) const
/*
Return TRUE if the provided scope is a class/struct/union scope, FALSE
otherwise.
*/
{
  a_boolean                   result = FALSE;

  if (decl_tag(scope) == ifc_DeclSort_Scope) {
    an_ifc_DeclSort_Scope       idss, *idssp;
    an_ifc_TypeSort_Fundamental itsf, *itsfp;
    read_partition_at_index(scope);
    idssp = get_DeclSort_Scope(&idss);
    check_assertion(type_tag(idssp->type) == ifc_TypeSort_Fundamental);
    read_partition_at_index(idssp->type);
    itsfp = get_TypeSort_Fundamental(&itsf);
    switch (itsfp->basis) {
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
  return result;
}  /* is_class_scope */


static void cache_token(a_token_cache_ptr     cache,
                        a_token_kind          tok,
                        a_source_position_ptr pos)
/*
Add tok to cache.  pos is the position of the token.
*/
{
  a_cached_token_ptr      ctp;
  a_token_sequence_number seq = NO_TOKEN_SEQUENCE_NUMBER;

  if (tok != tok_error) {
    assign_curr_token_sequence_number();
    seq = curr_token_sequence_number;
    last_token_sequence_number_of_token = seq;
  }  /* if */
  ctp = build_cached_token(tok, seq, pos);
  if (cache->first_token == NULL) {
    cache->first_token = ctp;
  } else {
    cache->last_token->next = ctp;
  }  /* if */
  cache->last_token = ctp;
#if DEBUG
  cache->token_count++;
#endif /* DEBUG */
}  /* cache_token */


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
  } else {
    check_assertion(is_error_type(lit_const->type));
    lit_kind = tok_error;
  }  /* if */
  cache_token(cache, lit_kind, pos);
  cache->last_token->extra_info_kind = (a_token_extra_info_kind)teik_constant;
  cache->last_token->variant.constant = alloc_cached_constant();
  copy_constant(lit_const, cache->last_token->variant.constant);
}  /* cache_literal */


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
                                           ifc_SourceLocation  *locus) const
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
                                            ifc_SourceLocation   *locus) const
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
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SourcePunctuator::MsvcDefaultInitStart",
                                  &error_position);
      break;
    default_is_unexpected_str("Unknown SourcePunctuator");
  }  /* switch */
}  /* cache_source_punctuator */


void an_ifc_module::cache_source_literal(a_token_cache_ptr  cache,
                                         ifc_SourceLiteral  literal,
                                         ifc_Index          index,
                                         ifc_SourceLocation *locus) const
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
      { ifc_StringIndex       str = (ifc_StringIndex)index;
        ifc_StringSort        sort = str_tag(str);
        an_ifc_String_Literal str_lit, *p_lit;
        a_character_kind      kind;

        read_partition_at_index(ifc_const_str, str_value(str));
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
          check_assertion(literal == ifc_SourceLiteral_String);
          cache_string_literal(cache, kind, get_string_at_offset(p_lit->start),
                               p_lit->length, &pos);
        } else {
          check_assertion(literal == ifc_SourceLiteral_DefinedString);
          cache_ud_literal(cache, kind, get_string_at_offset(p_lit->start),
                           p_lit->length, get_string_at_offset(p_lit->suffix),
                           &pos);
        }  /* if */
      }
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
        an_ifc_ExprSort_NamedDecl iesnd, *iesndp;
        check_assertion(expr_tag(index) == ifc_ExprSort_NamedDecl);
        source_position_from_locus(&pos, locus);
        read_partition_at_index((ifc_ExprIndex)index);
        iesndp = get_ExprSort_NamedDecl(&iesnd);
        cache_name_from_decl(cache, iesndp->resolution, &iesndp->locus);
      }
      break;
    default_is_unexpected_str("Unknown SourceLiteral");
  }  /* switch */
}  /* cache_source_literal */


void an_ifc_module::cache_source_operator(a_token_cache_ptr  cache,
                                          ifc_SourceOperator op,
                                          ifc_SourceLocation *locus) const
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
                                         ifc_SourceLocation *locus) const
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
      cache_token(cache, tok_false, &pos);
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
      cache_token(cache, tok_true, &pos);
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
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                                "SourceKeyword::MsvcBuiltinIsLayoutCompatible",
                                &error_position);
      break;
    case ifc_SourceKeyword_MsvcBuiltinIsPointerInterconvertibleBaseOf:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                   "SourceKeyword::MsvcBuiltinIsPointerInterconvertibleBaseOf",
                   &error_position);
      break;
    case ifc_SourceKeyword_MsvcBuiltinIsPointerInterconvertibleWithClass:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                "SourceKeyword::MsvcBuiltinIsPointerInterconvertibleWithClass",
                &error_position);
      break;
    case ifc_SourceKeyword_MsvcBuiltinIsCorrespondingMember:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag(
                             "SourceKeyword::MsvcBuiltinIsCorrespondingMember",
                             &error_position);
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
                                            ifc_SourceLocation   *locus) const
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


void an_ifc_module::cache_word(a_token_cache_ptr cache,
                               an_ifc_Word       *word) const
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
}  /* add_word_to_cache */


uint32_t an_ifc_module::cache_sentence(a_token_cache_ptr cache,
                                       ifc_SentenceIndex sentence,
                                       uint32_t          offset,
                                       a_boolean         look_for_stop_token)
                                                                          const
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
  read_partition_at_index(ifc_sentence, sentence-1);
  isp = get_Sentence(&is);
  for (; idx < isp->cardinality; ++idx) {
    an_ifc_Word iw, *iwp;
    read_partition_at_index(ifc_word, isp->start + idx);
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


a_boolean an_ifc_module::sentence_is_deleted(ifc_SentenceIndex  sentence) const
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
  read_partition_at_index(ifc_sentence, sentence-1);
  isp = get_Sentence(&is);
  for (uint32_t k = 0; k < isp->cardinality; ++k) {
    an_ifc_Word iw, *iwp;
    read_partition_at_index(ifc_word, isp->start + k);
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


static void cache_func_traits(a_token_cache_ptr     cache,
                              ifc_FunctionTraits    traits,
                              a_boolean             trailing,
                              a_source_position_ptr pos)
/*
Add the tokens corresponding to the given function traits to cache.  If
trailing is TRUE then cache the traits that follow a function declaration.
Otherwise, cache the traits that precede a function declaration.  pos is the
position to use for the traits.
*/
{
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
    if (traits & ifc_FunctionTraits_Inline) {
      cache_token(cache, tok_inline, pos);
    }  /* if */
    if (traits & ifc_FunctionTraits_Constexpr) {
      cache_token(cache, tok_constexpr, pos);
    }  /* if */
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
  if (traits & ifc_FunctionTraits_Vendor) {
    /* FIXME: Handle MSVC-specific traits. */
    pos_st_diagnostic(unhandled_ifc_node_severity,
                      ec_module_file_contains_unsupported_constructs,
                      &error_position, "FunctionTraits::Vendor");
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
                                         a_source_position_ptr     pos) const
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
      cache_token(cache, tok_false, pos);
      break;
    case ifc_NoexceptSort_True:
      cache_token(cache, tok_true, pos);
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
                                ifc_SourceLocation *locus) const
/*
For the given scope, cache tokens corresponding to the definition of the scope.
locus is the location of the scope.
*/
{
  an_ifc_Scope_Descriptor isd, *isdp;
  a_source_position       pos;

  if (scope == 0) goto done;
  source_position_from_locus(&pos, locus);
  read_partition_at_index(ifc_scope_desc, scope - 1);
  isdp = get_Scope_Descriptor(&isd);
  cache_token(cache, tok_lbrace, &pos);
  for (ifc_Index_type idx = 0; idx < isdp->cardinality; ++idx) {
    an_ifc_Scope_Member ism, *ismp;
    read_partition_at_index(ifc_scope_member, isdp->start + idx);
    ismp = get_Scope_Member(&ism);
    cache_decl(cache, ismp->index);
  }  /* for */
  cache_token(cache, tok_rbrace, &pos);
done:;
}  /* cache_scope */


void an_ifc_module::cache_type_first_pass(a_token_cache_ptr  cache,
                                          ifc_TypeIndex      type,
                                          ifc_SourceLocation *locus) const
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
  read_partition_at_index(type);
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
            cache_token(cache, tok_nullptr, &pos);
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
        cache_name_from_decl(cache, itsdp->decl, locus);
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
          read_partition_at_index(itsptmp->member);
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
        cache_type(cache, itslrp->referee, locus);
        cache_token(cache, tok_ampersand, &pos);
      }
      break;
    case ifc_TypeSort_RvalueReference:
      { an_ifc_TypeSort_RvalueReference itsrr, *itsrrp;
        itsrrp = get_TypeSort_RvalueReference(&itsrr);
        cache_type(cache, itsrrp->referee, locus);
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
        if (itsqp->qualifiers & ifc_Qualifier_Const) {
          cache_token(cache, tok_const, &pos);
        }  /* if */
        if (itsqp->qualifiers & ifc_Qualifier_Volatile) {
          cache_token(cache, tok_volatile, &pos);
        }  /* if */
        if (itsqp->qualifiers & ifc_Qualifier_Restrict) {
          cache_token(cache, tok_restrict, &pos);
        }  /* if */
        cache_type_first_pass(cache, itsqp->unqualified, locus);
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
          read_partition_at_index(ifc_heap_type,
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
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("TypeSort::Forall", &error_position);
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
                                           ifc_SourceLocation *locus) const
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
  read_partition_at_index(type);
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
          read_partition_at_index(itsptmp->member);
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
        cache_expr(cache, itsap->extent);
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
        cache_token(cache, tok_ellipsis, &pos);
      }
      break;
    case ifc_TypeSort_Fundamental:
    case ifc_TypeSort_Designated:
    case ifc_TypeSort_Syntactic:
    case ifc_TypeSort_LvalueReference:
    case ifc_TypeSort_RvalueReference:
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
                               ifc_SourceLocation *locus) const
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


void an_ifc_module::cache_chart(a_token_cache_ptr  cache,
                                ifc_ChartIndex     chart,
                                ifc_SourceLocation *locus) const
/*
Add the tokens corresponding to the given chart to cache.  The caller is
expected to have already cached the "template" keyword if it's required. locus
is the location of the chart.
*/
{
  ifc_ChartSort     tag = chart_tag(chart);
  ifc_ExprIndex     constraint = (ifc_ExprIndex)0;
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  cache_token(cache, tok_lt, &pos);
  read_partition_at_index(chart);
  switch (tag) {
    case ifc_ChartSort_None:
      /* No arguments to the template (i.e., specialization). */
      break;
    case ifc_ChartSort_Unilevel:
      { an_ifc_ChartSort_Unilevel icsu, *icsup;
        icsup = get_ChartSort_Unilevel(&icsu);
        constraint = icsup->constraint;
        for (ifc_Index_type idx = 0; idx < icsup->cardinality; ++idx) {
          if (idx > 0) cache_token(cache, tok_comma, &pos);
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
          if (idx > 0) cache_token(cache, tok_comma, &pos);
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
  cache_token(cache, tok_gt, &pos);
  if (constraint != (ifc_ExprIndex)0) {
    /* The template parameter list is followed by a requires-clause. */
    cache_expr(cache, constraint);
  }  /* if */
}  /* cache_chart */


void an_ifc_module::cache_operator(a_token_cache_ptr  cache,
                                   ifc_Operator       op,
                                   ifc_SourceLocation *locus) const
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
                                   ifc_SourceLocation           *locus) const
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
                                   ifc_SourceLocation  *locus) const
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
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("MonadicOperator::Cardinality",
                                  &error_position);
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
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("MonadicOperator::DeleteArray",
                                  &error_position);
      break;
    case ifc_MonadicOperator_Expand:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("MonadicOperator::Expand",
                                  &error_position);
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
    default_is_unexpected_str("Unexpected MonadicOperator");
  }  /* switch */
}  /* cache_operator */


void an_ifc_module::cache_operator(a_token_cache_ptr  cache,
                                   ifc_DyadicOperator op,
                                   ifc_SourceLocation *locus) const
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
                                   ifc_SourceLocation  *locus) const
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
                                   ifc_SourceLocation           *locus) const
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
                                   ifc_SourceLocation   *locus) const
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


void an_ifc_module::cache_variable_decl(a_token_cache_ptr   cache,
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
                                        ifc_SourceLocation  *locus) const
/*
Add the tokens corresponding to the given variable declaration to cache.
is_class_member is TRUE if this is a non-static data member of a class.
access, specifiers, traits, alignment, and type are values from the IFC file
that describe the variable declaration.  Both name and raw_name provide the
name of the variable - if name is zero, raw_name must be non-zero.  If width is
not zero, this is a bitfield and width is its size.  If the variable has an
initializer then initializer is non-zero and refers to the initializer
expression.  locus is the source location for the declaration.
*/
{
  a_source_position pos;
  a_boolean         decl_in_class;

  decl_in_class = is_class_member || access != ifc_Access_None;
  source_position_from_locus(&pos, locus);
  if (decl_in_class) {
    cache_access(cache, access, /*cache_colon=*/TRUE, &pos);
  }  /* if */
  cache_basic_specifiers(cache, specifiers, &pos);
  if (!is_class_member && decl_in_class) {
    /* This is a static data member. */
    cache_token(cache, tok_static, &pos);
  }  /* if */
  cache_object_traits(cache, traits, &pos);
  if (alignment != 0) {
    cache_token(cache, tok_alignas, &pos);
    cache_token(cache, tok_lparen, &pos);
    cache_expr(cache, alignment);
    cache_token(cache, tok_rparen, &pos);
  }  /* if */
  cache_type_first_pass(cache, type, locus);
  if (name != 0) {
    cache_name(cache, name, locus);
  } else {
    check_assertion(raw_name != 0);
    cache_identifier(cache, get_string_at_offset(raw_name), &pos);
  }  /* if */
  cache_type_second_pass(cache, type, locus);
  if (width != 0) {
    cache_token(cache, tok_colon, &pos);
    cache_expr(cache, width);
  }  /* if */
  /* FIXME: The initializer index sometimes has invalid values.  Treat all
     variables as uninitialized for now.  This will be a problem for constexpr,
     but is preferable to the alternative (aborting). */
  initializer = (ifc_ExprIndex)0;
  if (initializer != 0) {
    cache_token(cache, tok_assign, &pos);
    cache_expr(cache, initializer);
  }  /* if */
  cache_token(cache, tok_semicolon, &pos);
}  /* cache_variable_decl */


void an_ifc_module::cache_function_decl(
                                    a_token_cache_ptr         cache,
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
                                    ifc_SourceLocation        *locus) const
/*
Add the tokens corresponding to the given function declaration to cache.
is_class_member is TRUE if this is a non-static member of a class.  is_dtor is
TRUE if this is a destructor declaration.  access, calling_conv, func_traits,
func_type_traits, eh_spec, and name are values from the IFC file that describe
the function.  return_type is the return type of the function (0 if there is no
return type, e.g., the function is a constructor or destructor).  Both params
and param_types are the parameter list (0 for both if there are no parameters).
If params is non-zero, param_types will be ignored as params will already
contain the parameter types.  locus is the position of the function
declaration.
*/
{
  a_source_position pos;
  a_boolean         decl_in_class;

  decl_in_class = is_class_member || access != ifc_Access_None;
  source_position_from_locus(&pos, locus);
  if (decl_in_class) {
    cache_access(cache, access, /*cache_colon=*/TRUE, &pos);
  }  /* if */
  if (!is_class_member && decl_in_class) {
    /* This is a static member function. */
    cache_token(cache, tok_static, &pos);
  }  /* if */
  cache_func_traits(cache, func_traits, /*trailing=*/FALSE, &pos);
  if (return_type != 0) {
    cache_type(cache, return_type, locus);
  }  /* if */
  cache_calling_convention(cache, calling_conv, &pos);
  if (is_dtor) {
    cache_token(cache, tok_compl, &pos);
  }  /* if */
  cache_name(cache, name, locus);
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
  cache_func_traits(cache, func_traits, /*trailing=*/TRUE, &pos);
  cache_token(cache, tok_semicolon, &pos);
}  /* cache_function_decl */


void an_ifc_module::cache_decl_class(a_token_cache_ptr     cache,
                                     an_ifc_DeclSort_Scope *decl) const
/*
Add the tokens corresponding to the given class declaration (decl) to cache.
This will not cache the class name and type (class/struct/union).
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, &decl->locus);
  if (decl->base != 0) {
    cache_token(cache, tok_colon, &pos);
    cache_type(cache, decl->base, &decl->locus);
  }  /* if */
  cache_scope(cache, decl->initializer, &decl->locus);
  cache_token(cache, tok_semicolon, &pos);
}  /* cache_decl_class */


uint32_t an_ifc_module::try_cache_class_attributes_from_body(
                                               a_token_cache_ptr cache,
                                               ifc_SentenceIndex body_sentence)
                                                                          const
/*
MSVC puts attributes for class templates as part of the body_sentence.  As
attributes these need to go before the identifier for class templates and
partial specializations of said class templates, if any attributes are present,
this function caches the attributes into the given cache.  It then returns the
actual offset into the body_sentence where the brace wrapped
member-specification can be found (for use by later processing).
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
  }
  return offset;
}


uint32_t an_ifc_module::cache_decl_template_declaration(
                                        a_token_cache_ptr        cache,
                                        an_ifc_DeclSort_Template *decl,
                                        a_boolean                add_semicolon)
                                                                          const
/*
Add the tokens corresponding to the given template declaration (decl) to cache.
If add_semicolon is TRUE, include the terminating semicolon that would be
expected for a declaration that isn't a definition.  Return the offset into the
template declaration's body at which to find the definition, or zero
if there is no offset/the offset is not needed.
*/
{
  a_source_position pos;
  a_type_ptr        type;
  a_non_type_kind   kind;
  uint32_t          offset = 0;

  source_position_from_locus(&pos, &decl->locus);
  cache_token(cache, tok_template, &pos);
  cache_chart(cache, decl->chart, &decl->locus);
  /* FIXME: Handle attributes. */
  type = type_for_type_index(decl->type, &kind);
  check_assertion(type != NULL);
  if (type_is(type, tk_unknown)) {
    /* This is an alias template declaration. */
    /* As of IFC 0.31, this should no longer be encountered (template aliases
       are now handled by DeclSort::Alias). */
    check_assertion(name_tag(decl->name) == ifc_NameSort_Identifier);
    cache_token(cache, tok_using, &pos);
    cache_name(cache, decl->name, &decl->locus);
    cache_token(cache, tok_assign, &pos);
    cache_type(cache, (ifc_TypeIndex)decl->entity.index, &decl->locus);
    /* We always need a semicolon here. */
    add_semicolon = TRUE;
  } else if (is_class_struct_union_type(type)) {
    cache_type(cache, decl->type, &decl->locus);
    offset = try_cache_class_attributes_from_body(cache, decl->entity.body);
    cache_name(cache, decl->name, &decl->locus);
  } else {
    /* Function or variable template. */
    /* FIXME: Cache the entity corresponding to decl->entity.index instead.
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
                                        an_ifc_DeclSort_Template *decl) const
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


uint32_t an_ifc_module::cache_decl_partial_specialization_signature(
                                   a_token_cache_ptr                     cache,
                                   an_ifc_DeclSort_PartialSpecialization *decl)
                                                                          const
/*
Add the tokens corresponding to the given partial specializations declaration's
(decl) signature to cache.  Return the offset into the partial specialization's
body at which to find the definition, or zero if there is no offset/the offset
is not needed.
*/
{
  a_source_position pos;
  uint32_t          offset = 0;

  /* Reconstruct the template-head. */
  source_position_from_locus(&pos, &decl->locus);
  cache_token(cache, tok_template, &pos);
  cache_chart(cache, decl->chart, &decl->locus);
  {
    /* Reconstruct the declaration. */
    a_type_ptr               type;
    a_non_type_kind          kind;
    an_ifc_Form_Spec         ifs, *ifsp;
    an_ifc_DeclSort_Template idst, *idstp;

    /* Load the specialization form. */
    read_partition_at_index(decl->form);
    ifsp = get_Form_Spec(&ifs);
    /* Load the declaration of the primary definition. */
    read_partition_at_index(ifsp->primary_template);
    idstp = get_DeclSort_Template(&idst);
    /* Load the type from the declaration to determine what we're
       generating. */
    type = type_for_type_index(idstp->type, &kind);
    check_assertion(type != NULL);
    if (is_class_struct_union_type(type)) {
      /* We're reconstructing a class template. */
      /* FIXME: This is a hack, we're caching the parent type to get
         struct/class keyword. */
      cache_type(cache, idstp->type, &decl->locus);
      offset = try_cache_class_attributes_from_body(cache, decl->entity.body);
      /* Use the specialization form to reconstruct the simple-template-id. */
      {
        /* Reconstruct the template-name. */
        cache_name(cache, idstp->name, &decl->locus);
        /* Reconstruct the template-argument-list and enclosing angle
           brackets. */
        cache_token(cache, tok_lt, &pos);
        cache_expr(cache, ifsp->arguments);
        cache_token(cache, tok_gt, &pos);
      }
    }
  }
  return offset;
}  /* cache_decl_partial_specialization_signature */


void an_ifc_module::cache_decl_partial_specialization(
                                   a_token_cache_ptr                     cache,
                                   an_ifc_DeclSort_PartialSpecialization *decl)
                                                                          const
/*
Add the tokens corresponding to the given partial specialization declaration
(decl) to cache.
*/
{
  a_boolean decl_only = decl->entity.body == 0;
  uint32_t  offset = cache_decl_partial_specialization_signature(cache, decl);

  if (!decl_only) {
    (void)cache_sentence(cache, decl->entity.body, offset);
  }  /* if */
}  /* cache_decl_partial_specialization */


void an_ifc_module::cache_type_param_introducer(a_token_cache_ptr  cache,
                                                ifc_ExprIndex      constraint,
                                                a_source_position  *pos) const
/*
constraint is zero for unconstrained parameters or refers to a concept-id
expression otherwise.  In the former case, add a "typename" token to introduce
a template parameter, but in the latter case emit the concept-id.
*/
{
  if (constraint == (ifc_ExprIndex)0) {
    /* An unconstrained type parameter is introduced by the "typename"
       keyword (or "class", but we'll use "typename"). */
    cache_token(cache, tok_typename, pos);
  } else {
    cache_expr(cache, constraint);
  }  /* if */
}  /* cache_type_param_introducer */


void an_ifc_module::cache_decl(a_token_cache_ptr cache,
                               ifc_DeclIndex     decl) const
/*
Add the tokens corresponding to the given declaration (decl) to cache.
*/
{
  ifc_DeclSort      tag = decl_tag(decl);
  a_source_position pos;

  read_partition_at_index(decl);
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
        if (is_class_scope(idsvp->home_scope)) {
          access = idsvp->access;
        }  /* if */
        cache_variable_decl(cache, /*is_class_member=*/FALSE, access,
                            idsvp->specifier, idsvp->traits, idsvp->alignment,
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
            cache_type_param_introducer(cache, idspp->constraint, &pos);
            break;
          case ifc_ParameterSort_Object:
            /* FIXME: Currently unsupported. */
            issue_unsupported_node_diag("ParameterSort::Object",
                                        &error_position);
            break;
          case ifc_ParameterSort_NonType:
            cache_type_first_pass(cache, idspp->type, &idspp->locus);
            need_second_pass = TRUE;
            break;
          case ifc_ParameterSort_Placeholder:
            cache_token(cache, tok_auto, &pos);
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
          default_is_unexpected_str("Unexpected ParameterSort");
        }  /* switch */
        if (idspp->pack) {
          cache_token(cache, tok_ellipsis, &pos);
        }  /* if */
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
        check_assertion((idsfp->specifier & ifc_BasicSpecifiers_C) == 0);
        cache_variable_decl(cache, /*is_class_member=*/TRUE, idsfp->access,
                            idsfp->specifier, idsfp->traits, idsfp->alignment,
                            idsfp->type, (ifc_NameIndex)0, idsfp->name,
                            (ifc_ExprIndex)0, idsfp->initializer,
                            &idsfp->locus);
      }
      break;
    case ifc_DeclSort_Bitfield:
      { an_ifc_DeclSort_Bitfield idsbf, *idsbfp;
        idsbfp = get_DeclSort_Bitfield(&idsbf);
        check_assertion((idsbfp->specifier & ifc_BasicSpecifiers_C) == 0);
        check_assertion(idsbfp->width != 0);
        cache_variable_decl(cache, /*is_class_member=*/TRUE, idsbfp->access,
                            idsbfp->specifier, idsbfp->traits,
                            (ifc_ExprIndex)0, idsbfp->type, (ifc_NameIndex)0,
                            idsbfp->name, idsbfp->width, idsbfp->initializer,
                            &idsbfp->locus);
      }
      break;
    case ifc_DeclSort_Scope:
      { an_ifc_DeclSort_Scope idss, *idssp;
        an_ifc_TypeSort_Fundamental itsf, *itsfp;
        idssp = get_DeclSort_Scope(&idss);
        check_assertion(type_tag(idssp->type) == ifc_TypeSort_Fundamental);
        read_partition_at_index(idssp->type);
        itsfp = get_TypeSort_Fundamental(&itsf);
        source_position_from_locus(&pos, &idssp->locus);
        cache_type(cache, idssp->type, &idssp->locus);
        cache_name(cache, idssp->name, &idssp->locus);
        if (idssp->base != 0) {
          cache_token(cache, tok_colon, &pos);
          cache_type(cache, idssp->base, &idssp->locus);
        }  /* if */
        cache_scope(cache, idssp->initializer, &idssp->locus);
        if (itsfp->basis != ifc_TypeBasis_Namespace) {
          cache_token(cache, tok_semicolon, &pos);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Enumeration:
      { an_ifc_DeclSort_Enumeration idse, *idsep;
        an_ifc_TypeSort_Fundamental itsf, *itsfp;
        idsep = get_DeclSort_Enumeration(&idse);
        check_assertion(type_tag(idsep->type) == ifc_TypeSort_Fundamental);
        read_partition_at_index(idsep->type);
        itsfp = get_TypeSort_Fundamental(&itsf);
        source_position_from_locus(&pos, &idsep->locus);
        if (is_class_scope(idsep->home_scope)) {
          cache_access(cache, idsep->access, /*cache_colon=*/TRUE, &pos);
        }  /* if */
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
          read_partition_at_index(idsap->type);
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
          read_partition_at_index(idsap->aliasee);
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
      }
      break;
    case ifc_DeclSort_PartialSpecialization:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::PartialSpecialization",
                                  &error_position);
      break;
    case ifc_DeclSort_ExplicitSpecialization:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::ExplicitSpecialization",
                                  &error_position);
      break;
    case ifc_DeclSort_ExplicitInstantiation:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("DeclSort::ExplicitInstantiation",
                                  &error_position);
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

        idsfp = get_DeclSort_Function(&idsf);
        check_assertion(type_tag(idsfp->type) == ifc_TypeSort_Function);
        read_partition_at_index(idsfp->type);
        itsfp = get_TypeSort_Function(&itsf);
        if (itsfp->source != 0) {
          params = get_func_params_from_trait(decl);
        }  /* if */
        if (is_class_scope(idsfp->home_scope)) {
          access = idsfp->access;
        }  /* if */
        cache_function_decl(cache, /*class_member=*/FALSE, /*is_dtor=*/FALSE,
                            access, itsfp->convention, idsfp->traits,
                            itsfp->traits, itsfp->target, idsfp->name, params,
                            itsfp->source, &itsfp->eh_spec, &idsfp->locus);
      }
      break;
    case ifc_DeclSort_Method:
      { an_ifc_DeclSort_Method idsm, *idsmp;
        an_ifc_TypeSort_Method itsm, *itsmp;
        ifc_ChartIndex         params = (ifc_ChartIndex)0;
        ifc_TypeIndex          target = (ifc_TypeIndex)0;

        idsmp = get_DeclSort_Method(&idsm);
        check_assertion(type_tag(idsmp->type) == ifc_TypeSort_Method);
        read_partition_at_index(idsmp->type);
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
        cache_function_decl(cache, /*class_member=*/TRUE, /*is_dtor=*/FALSE,
                            idsmp->access, itsmp->convention, idsmp->traits,
                            itsmp->traits, target, idsmp->name, params,
                            itsmp->source, &itsmp->eh_spec, &idsmp->locus);
      }
      break;
    case ifc_DeclSort_Constructor:
      { an_ifc_DeclSort_Constructor idsc, *idscp;
        an_ifc_TypeSort_Tor         itst, *itstp;
        an_ifc_DeclSort_Scope       idss, *idssp;
        ifc_ChartIndex              params = (ifc_ChartIndex)0;

        idscp = get_DeclSort_Constructor(&idsc);
        check_assertion(decl_tag(idscp->home_scope) == ifc_DeclSort_Scope);
        read_partition_at_index(idscp->home_scope);
        idssp = get_DeclSort_Scope(&idss);
        check_assertion(type_tag(idscp->type) == ifc_TypeSort_Tor);
        read_partition_at_index(idscp->type);
        itstp = get_TypeSort_Tor(&itst);
        if (itstp->source != 0) {
          params = get_func_params_from_trait(decl);
        }  /* if */
        cache_function_decl(cache, /*class_member=*/TRUE, /*is_dtor=*/FALSE,
                            idscp->access, itstp->convention, idscp->traits,
                            (ifc_FunctionTypeTraits)0, (ifc_TypeIndex)0,
                            idssp->name, params, itstp->source,
                            &itstp->eh_spec, &idscp->locus);
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
      { an_ifc_DeclSort_Destructor idsd, *idsdp;
        an_ifc_DeclSort_Scope      idss, *idssp;

        idsdp = get_DeclSort_Destructor(&idsd);
        check_assertion(decl_tag(idsdp->home_scope) == ifc_DeclSort_Scope);
        read_partition_at_index(idsdp->home_scope);
        idssp = get_DeclSort_Scope(&idss);
        cache_function_decl(cache, /*class_member=*/TRUE, /*is_dtor=*/TRUE,
                            idsdp->access, idsdp->convention, idsdp->traits,
                            (ifc_FunctionTypeTraits)0, (ifc_TypeIndex)0,
                            idssp->name, (ifc_ChartIndex)0, (ifc_TypeIndex)0,
                            &idsdp->eh_spec, &idsdp->locus);
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
        /* FIXME: This isn't correct -- we need to (fully?) qualify the
           declindex(s) specified by idsudp->resolution.  See the other
           use of ifc_DeclSort_UsingDeclaration for an example. */
        issue_unsupported_node_diag("DeclSort::UsingDeclaration", &pos);
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
    case ifc_DeclSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected DeclSort");
  }  /* switch */
}  /* cache_decl */


void an_ifc_module::cache_expr(a_token_cache_ptr cache,
                               ifc_ExprIndex     expr,
             /* Defaulted: */  a_token_kind      tuple_separator) const
/*
Add the tokens corresponding to the given expression (expr) to cache.  When
caching a construct tagged as ifc_ExprSort_Tuple, separate the constituent
expressions by the tuple_separator token (tok_comma by default).
*/
{
  ifc_ExprSort      tag = expr_tag(expr);
  a_source_position pos;

  read_partition_at_index(expr);
  switch (tag) {
    case ifc_ExprSort_VendorExtension:
      issue_unsupported_node_diag("ExprSort::VendorExtension",
                                  &error_position);
      break;
    case ifc_ExprSort_Empty:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Empty", &error_position);
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
        cache_name_from_decl(cache, iesndp->resolution, &iesndp->locus);
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
        /* FIXME: Do we need to handle the "type" and "resolution" fields
           here? */
        if (iesuip->name != 0) {
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
        cache_expr(cache, iesqnp->elements, /*separator=*/tok_colon_colon);
      }
      break;
    case ifc_ExprSort_Path:
      { an_ifc_ExprSort_Path iesp, *iespp;
        iespp = get_ExprSort_Path(&iesp);
        cache_expr(cache, iespp->scope);
        cache_token(cache, tok_colon_colon, &null_source_position);
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
          cache_expr(cache, iesmp->argument);
          cache_token(cache, tok_rparen, &pos);
        };  /* cache_arg */

        iesmp = get_ExprSort_Monad(&iesm);
        source_position_from_locus(&pos, &iesmp->locus);
        opkind = get_operator_kind(iesmp->op);
        switch (opkind) {
          case opkind_basic:
          case opkind_func_like:
            cache_operator(cache, iesmp->op, &iesmp->locus);
            cache_arg();
            break;
          case opkind_post:
            cache_arg();
            cache_operator(cache, iesmp->op, &iesmp->locus);
            break;
          case opkind_other:
            { a_token_kind ltok, rtok;
              if (iesmp->op == ifc_MonadicOperator_Paren) {
                ltok = tok_lparen;
                rtok = tok_rparen;
              } else if (iesmp->op == ifc_MonadicOperator_Brace) {
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
        opkind = get_operator_kind(iesdp->op);
        switch (opkind) {
          case opkind_basic:
            cache_expr(cache, iesdp->arguments_0);
            cache_operator(cache, iesdp->op, &iesdp->locus);
            cache_expr(cache, iesdp->arguments_1);
            break;
          case opkind_func_like:
            cache_operator(cache, iesdp->op, &iesdp->locus);
            cache_token(cache, tok_lparen, &pos);
            cache_expr(cache, iesdp->arguments_0);
            cache_token(cache, tok_comma, &pos);
            cache_expr(cache, iesdp->arguments_1);
            cache_token(cache, tok_rparen, &pos);
            break;
          case opkind_cpp_cast:
            cache_operator(cache, iesdp->op, &iesdp->locus);
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
        switch (iestp->op) {
          case ifc_TriadicOperator_Choice:
            {
              cache_expr(cache, iestp->arguments_0);
              cache_operator(cache, iestp->op, &iestp->locus);
              cache_expr(cache, iestp->arguments_1);
              cache_token(cache, tok_colon, &pos);
              cache_expr(cache, iestp->arguments_2);
            }
            break;
          case ifc_TriadicOperator_ConstructAt:
            {
              cache_operator(cache, iestp->op, &iestp->locus);
              cache_token(cache, tok_lparen, &pos);
              cache_expr(cache, iestp->arguments_0);
              cache_token(cache, tok_rparen, &pos);
              cache_expr(cache, iestp->arguments_1);
              if (iestp->arguments_2 != 0) {
                cache_token(cache, tok_lparen, &pos);
                cache_expr(cache, iestp->arguments_2);
                cache_token(cache, tok_rparen, &pos);
              }  /* if */
            }
            break;
          default:
            unexpected_condition();
        }  /* switch */
      }
      break;
    case ifc_ExprSort_String:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::String", &error_position);
      break;
    case ifc_ExprSort_Temporary:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Temporary", &error_position);
      break;
    case ifc_ExprSort_Call:
      { an_ifc_ExprSort_Call iesc, *iescp;
        iescp = get_ExprSort_Call(&iesc);
        cache_expr(cache, iescp->operation);
        if (iescp->arguments != 0) {
#if CHECKING
          ifc_ExprSort  arguments_tag = expr_tag(iescp->arguments);
          check_assertion(arguments_tag == ifc_ExprSort_ExpressionList);
#endif /* CHECKING */
          cache_expr(cache, iescp->arguments);
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
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::MemberInitializer",
                                  &error_position);
      break;
    case ifc_ExprSort_MemberAccess:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::MemberAccess", &error_position);
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
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Cast", &error_position);
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
    case ifc_ExprSort_New:
      { an_ifc_ExprSort_New  iesn, *iesnp;
        iesnp = get_ExprSort_New(&iesn);
        if (iesnp->double_colon.line != 0) {
          source_position_from_locus(&pos, &iesnp->double_colon);
          cache_token(cache, tok_colon_colon, &pos);
        }  /* if */
        source_position_from_locus(&pos, &iesnp->new_keyword);
        cache_token(cache, tok_new, &pos);
        /* iesnp->placement appears to always represent a parenthesized
           expression list.  If the list is empty, this is not a placement-new
           expression. */
#if CHECKING
        ifc_ExprSort  placement_tag = expr_tag(iesnp->placement);
        check_assertion(placement_tag == ifc_ExprSort_ExpressionList);
#endif /* CHECKING */
        read_partition_at_index(iesnp->placement);
        an_ifc_ExprSort_ExpressionList esel, *eselp;
        eselp = get_ExprSort_ExpressionList(&esel);
        if (eselp->contents != (ifc_ExprIndex)0) {
          /* The list is not empty. */
          cache_expr(cache, iesnp->placement);
        }  /* if */
        cache_type(cache, iesnp->allocated_type, &iesnp->new_keyword);
        if (iesnp->initializer != (ifc_ExprIndex)0) {
          cache_expr(cache, iesnp->initializer);
        }  /* if */
      }
      break;
    case ifc_ExprSort_Delete:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Delete", &error_position);
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
        iesptvp = get_ExprSort_ProductTypeValue(&iesptv);
        source_position_from_locus(&pos, &iesptvp->locus);
        /* FIXME: This node seems to be for value-initialization of entities.
           Are there any other cases that get here? */
        cache_name_from_decl(cache, iesptvp->class_decl, &iesptvp->locus);
        cache_token(cache, tok_lbrace, &pos);
        cache_token(cache, tok_rbrace, &pos);
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
            cache_token(cache, tuple_separator, &pos);
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
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::TemplateReference",
                                  &error_position);
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
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::Tokens", &error_position);
      break;
    case ifc_ExprSort_AssignInitializer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("ExprSort::AssignInitializer",
                                  &error_position);
      break;
    case ifc_ExprSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unknown ExprSort");
  }  /* switch */
}  /* cache_expr */


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
/* Remove ARG_UNUSED as well. */
void an_ifc_module::cache_syntax(ARG_UNUSED a_token_cache_ptr cache,
                                 ifc_SyntaxIndex              syntax) const
/*
Add the tokens corresponding to the given syntax tree to cache.
*/
{
  ifc_SyntaxSort    tag = syntax_tag(syntax);
  a_source_position pos;

  read_partition_at_index(syntax);
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
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::SimpleRequirement",
                                  &error_position);
      break;
    case ifc_SyntaxSort_TypeRequirement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_node_diag("SyntaxSort::TypeRequirement",
                                  &error_position);
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
          read_partition_at_index(ifc_heap_syn, isstp->start+k);
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
                               ifc_SourceLocation *locus) const
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
  read_partition_at_index(name);
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
      { an_ifc_NameSort_Template inst;
        get_NameSort_Template(&inst);
        /* FIXME: Currently unsupported. */
        issue_unsupported_node_diag("NameSort::Template", &error_position);
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
        a_type_ptr                 target_type;
        a_const_char               *tp_name;
        inscp = get_NameSort_Conversion(&insc);
        target_type = type_for_type_index(inscp->target, /*kind=*/NULL);
        /* Note that inscp->encoded contains the mangled name of the
           conversion function, so use the name from the type instead. */
        tp_name = get_type_name(target_type);
        cache_token(cache, tok_operator, &pos);
        cache_tokens_from_string(tp_name, cache, &pos);
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


void an_ifc_module::cache_name_from_decl(a_token_cache_ptr  cache,
                                         ifc_DeclIndex      decl,
                                         ifc_SourceLocation *locus) const
/*
Add the tokens corresponding to the given declaration's (decl) name to cache.
locus is the location of the name use.
*/
{
  a_source_position pos;

  source_position_from_locus(&pos, locus);
  cache_identifier(cache, name_from_decl(decl), &pos);
}  /* cache_name_from_decl */


inline size_t an_ifc_module::file_offset_of(an_ifc_partition_kind partition,
                                            ifc_Index_type        index) const
/*
Return the IFC file offset of the specified index into the given partition
of the module.  This value is used as a key to uniquely identify the IFC entity
(and is purposely not kept as a pointer because the underlying address space is
typically memory-mapped and could change during PCH file processing).
*/
{
  size_t part_offset = index * partitions[partition].entry_size;

#if EXPENSIVE_CHECKING
  check_assertion(partitions[partition].offset != 0 &&
                  partitions[partition].size != 0 &&
                  partitions[partition].size > part_offset);
#endif /* EXPENSIVE_CHECKING */
  return partitions[partition].offset + part_offset;
}  /* file_offset_of */


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


inline void an_ifc_module::read_partition_at_offset(
                                               an_ifc_partition_kind partition,
                                               size_t                offset)
                                                                          const
/*
Set the read buffer to the provided offset for the given partition in
preparation for a call to GET_byte, GET_short, etc.
*/
{
#if DEBUG && EXPENSIVE_CHECKING
  debug_partition = &partitions[partition];
#endif /* DEBUG && EXPENSIVE_CHECKING */
  init_byte_buffer(offset, partitions[partition].size);
}  /* read_partition_at_offset */


inline void an_ifc_module::read_partition_at_index(
                                               an_ifc_partition_kind partition,
                                               ifc_Index_type        index)
                                                                          const
/*
Set the read buffer to the appropriate offset for an entity at the provided
index into the given partition in preparation for a call to GET_byte,
GET_short, etc.
*/
{
  read_partition_at_offset(partition, file_offset_of(partition, index));
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_TypeSort   type_kind,
                                                   ifc_Index_type index) const
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_TypeSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_type_start + type_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_TypeIndex type) const
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_TypeIndex"
into its tag and index components for convenience.
*/
{
  read_partition_at_index(type_tag(type), type_value(type));
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_ExprSort   expr_kind,
                                                   ifc_Index_type index) const
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_ExprSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_expr_start + expr_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_ExprIndex expr) const
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_ExprIndex"
into its tag and index components for convenience.
*/
{
  read_partition_at_index(expr_tag(expr), expr_value(expr));
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_StmtSort   stmt_kind,
                                                   ifc_Index_type index) const
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_StmtSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_stmt_start + stmt_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_StmtIndex stmt) const
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_StmtIndex"
into its tag and index components for convenience.
*/
{
  read_partition_at_index(stmt_tag(stmt), stmt_value(stmt));
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_DeclSort   decl_kind,
                                                   ifc_Index_type index) const
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_DeclSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_decl_start + decl_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_DeclIndex decl) const
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_DeclIndex"
into its tag and index components for convenience.
*/
{
  read_partition_at_index(decl_tag(decl), decl_value(decl));
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_NameSort   name_kind,
                                                   ifc_Index_type index) const
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_NameSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  if (name_kind != ifc_NameSort_Identifier) {
    read_partition_at_index((an_ifc_partition_kind)(ifc_name_start+name_kind),
                            index);
  }  /* if */
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_NameIndex name) const
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_NameIndex"
into its tag and index components for convenience.
*/
{
  read_partition_at_index(name_tag(name), name_value(name));
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_ChartSort  chart_kind,
                                                   ifc_Index_type index) const
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_ChartSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_chart_start +chart_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_ChartIndex chart) const
/*
Overload wrapper for "read_partition_at_index" that converts an
"ifc_ChartIndex" into its tag and index components for convenience.
*/
{
  read_partition_at_index(chart_tag(chart), chart_value(chart));
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_FormSpecIndex form_spec)
                                                                          const
/*
Overload wrapper for "read_partition_at_index" that converts an
"ifc_FormSpecIndex" into its tag and index components for convenience.
*/
{
  read_partition_at_index(ifc_form_spec, form_spec);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_SyntaxSort syntax_kind,
                                                   ifc_Index_type index) const
/*
Overload wrapper for "read_partition_at_index" that converts an
"ifc_SyntaxSort" kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_syntax_start +
                                                                  syntax_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_SyntaxIndex syntax)const
/*
Overload wrapper for "read_partition_at_index" that converts an
"ifc_SyntaxIndex" into its tag and index components for convenience.
*/
{
  read_partition_at_index(syntax_tag(syntax), syntax_value(syntax));
}  /* read_partition_at_index */


inline ifc_Index an_ifc_module::read_index_from_heap(
                                          an_ifc_partition_kind heap_partition,
                                          ifc_Index_type        index) const
/*
Read an ifc_Index from the heap indicated by heap_partition at the given index
into that partition.
*/
{
  ifc_Index result;

  read_partition_at_index(heap_partition, index);
  GET_Index(result, /*from_header=*/FALSE);
  return result;
}  /* read_index_from_heap */


template<typename T, typename an_ifc_get_func>
T* an_ifc_module::find_trait(ifc_DeclIndex         decl_index,
                             an_ifc_partition_kind partition,
                             an_ifc_get_func       get_func,
                             T*                    storage) const
/*
Given a declaration index as a key to the associated trait table identified by
partition, find and return a pointer to the associated trait, or NULL if none
is found.  get_func is the getter function to get the trait from the table.
storage is the data structure that will be provided to get_func, but cannot be
relied upon to contain the result.
*/
{
  T        *result = NULL;
  uint32_t idx, min_idx, max_idx, num_entries;

  if (partitions[partition].size == 0) goto done;
  num_entries = get_num_entries(partition);
  min_idx = 0;
  max_idx = num_entries-1;
  while (min_idx <= max_idx && max_idx < num_entries) {
    T             *tmp;
    ifc_DeclIndex decl;
    idx = (min_idx + max_idx) / 2;
    read_partition_at_index(partition, idx);
    tmp = (this->*get_func)(storage, /*from_header=*/FALSE);
    decl = tmp->decl;
    if (decl == decl_index) {
      result = tmp;
      break;
    } else if (decl < decl_index) {
      min_idx = idx + 1;
    } else /* decl > decl_index */ {
      /* This may underflow, but that will be caught by the above
         max_idx < num_entries check. */
      max_idx = idx - 1;
    }  /* if */
  }  /* while */
done:
  return result;
}  /* find_trait */


ifc_ChartIndex an_ifc_module::get_func_params_from_trait(ifc_DeclIndex decl)
                                                                          const
/*
Find and return the index to the named function parameters corresponding to
decl, or 0 if not found.
*/
{
  an_ifc_Trait_MsvcFuncParams itmfp, *itmfpp;
  ifc_ChartIndex              params = (ifc_ChartIndex)0;

  itmfpp = find_trait(decl, ifc_msvc_trait_named_func_params,
                      &an_ifc_module::get_Trait_MsvcFuncParams,
                      &itmfp);
  if (itmfpp != NULL) {
    params = itmfpp->params;
  }  /* if */
  return params;
}  /* get_func_params_from_trait */

/*
FIXME: Eliminate all str_ifc_* functions, or replace with a version that
caches tokens and stringizes that cache, in order to avoid having to duplicate
logic everywhere.
*/
void an_ifc_module::str_ifc_text_offset(ifc_TextOffset      offset,
                                        a_str_control_block *scbp) const
/*
Add the string at the specified offset to the output buffer.
*/
{
  a_const_char *str = get_string_at_offset(offset);
  add_string_to_text_buffer(scbp->text_buffer, str);
}  /* str_ifc_text_offset */


void an_ifc_module::str_ifc_name_index(ifc_NameIndex       name_index,
                                       a_str_control_block *scbp) const
/*
Add the string represented by name_index to the current output buffer.
*/
{
  a_const_char *str = string_from_name_index(name_index,
                                             (a_symbol_locator*)NULL);
  add_string_to_text_buffer(scbp->text_buffer, str);
}  /* str_ifc_name_index */


void an_ifc_module::str_ifc_class_name(ifc_DeclIndex       home_scope,
                                       a_str_control_block *scbp) const
/*
For constructors and destructors, add the name of the class specified
by home_scope to the output buffer.
*/
{
  ifc_DeclSort          tag = decl_tag(home_scope);
  an_ifc_DeclSort_Scope idss, *idssp;

  /* Prepare to read from the proper partition for this declaration. */
  read_partition_at_index(tag, decl_value(home_scope));
  check_assertion(tag == ifc_DeclSort_Scope);
  idssp = get_DeclSort_Scope(&idss);
  str_ifc_name_index(idssp->name, scbp);
}  /* str_ifc_class_name */


void an_ifc_module::str_ifc_add_number(a_host_large_unsigned value,
                                       a_str_control_block   *scbp) const
/*
Add the decimal representation of value to the output buffer.
*/
{
  char     buffer[50];
  sizeof_t len = unsigned_to_string_buf(value, buffer);

  add_to_text_buffer(scbp->text_buffer, buffer, len);
}  /* str_ifc_add_number */

#if !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER

void an_ifc_module::str_ifc_add_number(an_integer_value    &value,
                                       a_str_control_block *scbp) const
/*
Add the decimal representation of value to the output buffer.
*/
{
  char*    str = str_for_integer_value(&value);
  sizeof_t len = strlen(str);

  add_to_text_buffer(scbp->text_buffer, str, len);
}  /* str_ifc_add_number */

#endif /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

void an_ifc_module::str_ifc_source_location(
                                        ARG_UNUSED ifc_SourceLocation  *locus,
                                        ARG_UNUSED a_str_control_block *scbp)
                                                                          const
/*
Add source location to the current string.
*/
{
#if 0 /* FIXME: too noisy */
  an_ifc_Source_Line isl, *islp;

  if (!scbp->is_generated_code) {
    read_partition_at_index(scbp->module_info, ifc_src_line, locus->line);
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
                                   a_str_control_block *scbp) const
/*
Add an access specifier to the current string, if needed.
*/
{
  a_const_char *string;

  switch (access) {
    case ifc_Access_None:       string = NULL;         break;
    case ifc_Access_Private:    string = "private";    break;
    case ifc_Access_Protected:  string = "protected";  break;
    case ifc_Access_Public:     string = "public";     break;
    default_is_unexpected();
  }  /* switch */
  if (string != NULL) {
    add_string_to_text_buffer(scbp->text_buffer, string);
  }  /* if */
}  /* str_ifc_access */


void an_ifc_module::str_ifc_qualifiers(ifc_Qualifiers      qualifiers,
                                       a_str_control_block *scbp) const
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
                                             a_str_control_block *scbp) const
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
      add_string_to_text_buffer(scbp->text_buffer, "/*NonExported?*/");
    }  /* if */
  }  /* if */
}  /* str_ifc_basic_specifiers */


void an_ifc_module::str_ifc_object_traits(ifc_ObjectTraits    traits,
                                          a_str_control_block *scbp) const
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
                                        a_str_control_block *scbp) const
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
                                                                          const
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
                                                                          const
/*
Add strings representing a noexcept specification, if any.
*/
{
  switch (eh_spec->sort) {
    case ifc_NoexceptSort_None:
      break;
    case ifc_NoexceptSort_False:
      add_string_to_text_buffer(scbp->text_buffer, "noexcept(false) ");
      break;
    case ifc_NoexceptSort_True:
      add_string_to_text_buffer(scbp->text_buffer, "noexcept(true) ");
      break;
    case ifc_NoexceptSort_Expression:
    case ifc_NoexceptSort_Inferred:
    case ifc_NoexceptSort_Unenforced:
      /* FIXME: not sure what these should be */
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
}  /* str_ifc_noexcept_specification */


void an_ifc_module::str_ifc_function_traits(ifc_FunctionTraits  traits,
                                            a_boolean           prefix,
                                            a_str_control_block *scbp) const
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
                                       a_str_control_block *scbp) const
/*
Add a textual representation of the expression referenced by expr_index to
the output buffer.
*/
{
  ifc_ExprSort      tag = expr_tag(expr_index);

  /* Prepare to read from the proper partition for this expression. */
  read_partition_at_index(tag, expr_value(expr_index));
  switch (tag) {
    case ifc_ExprSort_Literal:
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
          case ifc_LiteralSort_Immediate:
            str_ifc_add_number(
                            (a_host_large_unsigned)literal_index(ieslp->value),
                            scbp);
            break;
          case ifc_LiteralSort_Integer:
            { an_integer_value value;
              char             raw_val[64/CHAR_BIT];
              read_partition_at_index(ifc_const_i64,
                                      literal_index(ieslp->value));
              GET_64bit_int(raw_val, /*from_header=*/FALSE);
              if (!conv_bytes_to_integer_value(&value, raw_val,
                                               sizeof(raw_val))) {
                unexpected_condition_str("Failed to get 64-bit integer");
              }  /* if */
              str_ifc_add_number(value, scbp);
            }
            break;
          case ifc_LiteralSort_FloatingPoint:
            /* FIXME: not yet implemented. */
            unexpected_condition();
            break;
          default_is_unexpected();
        }  /* switch */
      }
      break;
    case ifc_ExprSort_VendorExtension:
      { an_ifc_ExprSort_VendorExtension ieve;
        get_ExprSort_VendorExtension(&ieve);
        unexpected_condition_str("ExprSort::VendorExtension"
                                 " is currently unspecified.");
      }
      break;
    case ifc_ExprSort_Empty:
      { an_ifc_ExprSort_Empty iee;
        get_ExprSort_Empty(&iee);
        add_char_to_text_buffer(scbp->text_buffer, ';');
      }
      break;
    case ifc_ExprSort_Type:
      { an_ifc_ExprSort_Type iet;
        get_ExprSort_Type(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_NamedDecl:
      { an_ifc_ExprSort_NamedDecl iend;
        get_ExprSort_NamedDecl(&iend);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_UnresolvedId:
      { an_ifc_ExprSort_UnresolvedId ieuid;
        get_ExprSort_UnresolvedId(&ieuid);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_TemplateId:
      { an_ifc_ExprSort_TemplateId ietid;
        get_ExprSort_TemplateId(&ietid);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_UnqualifiedId:
      { an_ifc_ExprSort_UnqualifiedId ieuid;
        get_ExprSort_UnqualifiedId(&ieuid);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_SimpleIdentifier:
      { an_ifc_ExprSort_SimpleIdentifier iesid;
        get_ExprSort_SimpleIdentifier(&iesid);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Pointer:
      { an_ifc_ExprSort_Pointer iep;
        get_ExprSort_Pointer(&iep);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_QualifiedName:
      { an_ifc_ExprSort_QualifiedName ieqn;
        get_ExprSort_QualifiedName(&ieqn);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Path:
      { an_ifc_ExprSort_Path iep;
        get_ExprSort_Path(&iep);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Read:
      { an_ifc_ExprSort_Read ier;
        get_ExprSort_Read(&ier);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Monad:
      { an_ifc_ExprSort_Monad iem;
        get_ExprSort_Monad(&iem);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Dyad:
      { an_ifc_ExprSort_Dyad ied;
        get_ExprSort_Dyad(&ied);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Triad:
      { an_ifc_ExprSort_Triad iet;
        get_ExprSort_Triad(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Tuple:
      { an_ifc_ExprSort_Tuple iet;
        get_ExprSort_Tuple(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Tokens:
      { an_ifc_ExprSort_Tokens iet;
        get_ExprSort_Tokens(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_String:
      { an_ifc_ExprSort_String ies;
        get_ExprSort_String(&ies);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Temporary:
      { an_ifc_ExprSort_Temporary iet;
        get_ExprSort_Temporary(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Call:
      { an_ifc_ExprSort_Call iec;
        get_ExprSort_Call(&iec);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_PushState:
      { an_ifc_ExprSort_PushState iep;
        get_ExprSort_PushState(&iep);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_TypeTraitIntrinsic:
      { an_ifc_ExprSort_TypeTraitIntrinsic ietti;
        get_ExprSort_TypeTraitIntrinsic(&ietti);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_MemberInitializer:
      { an_ifc_ExprSort_MemberInitializer iemi;
        get_ExprSort_MemberInitializer(&iemi);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_MemberAccess:
      { an_ifc_ExprSort_MemberAccess iema;
        get_ExprSort_MemberAccess(&iema);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_InheritancePath:
      { an_ifc_ExprSort_InheritancePath ieip;
        get_ExprSort_InheritancePath(&ieip);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_TemplateReference:
      { an_ifc_ExprSort_TemplateReference ietr;
        get_ExprSort_TemplateReference(&ietr);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_InitializerList:
      { an_ifc_ExprSort_InitializerList ieil;
        get_ExprSort_InitializerList(&ieil);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Cast:
      { an_ifc_ExprSort_Cast iec;
        get_ExprSort_Cast(&iec);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Condition:
      { an_ifc_ExprSort_Condition iec;
        get_ExprSort_Condition(&iec);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_ExpressionList:
      { an_ifc_ExprSort_ExpressionList ieel;
        get_ExprSort_ExpressionList(&ieel);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_AssignInitializer:
      { an_ifc_ExprSort_AssignInitializer ieai;
        get_ExprSort_AssignInitializer(&ieai);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Nullptr:
      { an_ifc_ExprSort_Nullptr ienp;
        get_ExprSort_Nullptr(&ienp);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_This:
      { an_ifc_ExprSort_This iet;
        get_ExprSort_This(&iet);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_SizeofType:
      { an_ifc_ExprSort_SizeofType iesotid;
        get_ExprSort_SizeofType(&iesotid);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Alignof:
      { an_ifc_ExprSort_Alignof ieao;
        get_ExprSort_Alignof(&ieao);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_PackedTemplateArguments:
      { an_ifc_ExprSort_PackedTemplateArguments iepta;
        get_ExprSort_PackedTemplateArguments(&iepta);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_New:
      { an_ifc_ExprSort_New ien;
        get_ExprSort_New(&ien);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Delete:
      { an_ifc_ExprSort_Delete ied;
        get_ExprSort_Delete(&ied);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Lambda:
      { an_ifc_ExprSort_Lambda iel;
        get_ExprSort_Lambda(&iel);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_DestructorCall:
      { an_ifc_ExprSort_DestructorCall iedc;
        get_ExprSort_DestructorCall(&iedc);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Typeid:
      { an_ifc_ExprSort_Typeid ietid;
        get_ExprSort_Typeid(&ietid);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_SyntaxTree:
      { an_ifc_ExprSort_SyntaxTree iest;
        get_ExprSort_SyntaxTree(&iest);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_FunctionString:
      { an_ifc_ExprSort_FunctionString iefs;
        get_ExprSort_FunctionString(&iefs);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_CompoundString:
      { an_ifc_ExprSort_CompoundString iecs;
        get_ExprSort_CompoundString(&iecs);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_StringSequence:
      { an_ifc_ExprSort_StringSequence iess;
        get_ExprSort_StringSequence(&iess);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Initializer:
      { an_ifc_ExprSort_Initializer iei;
        get_ExprSort_Initializer(&iei);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_HierarchyConversion:
      { an_ifc_ExprSort_HierarchyConversion iehc;
        get_ExprSort_HierarchyConversion(&iehc);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_ProductTypeValue:
      { an_ifc_ExprSort_ProductTypeValue iep;
        get_ExprSort_ProductTypeValue(&iep);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_SumTypeValue:
      { an_ifc_ExprSort_SumTypeValue iestv;
        get_ExprSort_SumTypeValue(&iestv);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_SubobjectValue:
      { an_ifc_ExprSort_SubobjectValue ieso;
        get_ExprSort_SubobjectValue(&ieso);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_ArrayValue:
      { an_ifc_ExprSort_ArrayValue iea;
        get_ExprSort_ArrayValue(&iea);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_VirtualFunctionConversion:
      { an_ifc_ExprSort_VirtualFunctionConversion ievf;
        get_ExprSort_VirtualFunctionConversion(&ievf);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_Requires:
      { an_ifc_ExprSort_Requires ier;
        get_ExprSort_Requires(&ier);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_UnaryFold:
      { an_ifc_ExprSort_UnaryFold ieuf;
        get_ExprSort_UnaryFold(&ieuf);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_ExprSort_BinaryFold:
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
                                        a_str_control_block *scbp) const
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
    read_partition_at_index(ifc_scope_desc, scope_index - 1);
    isdp = get_Scope_Descriptor(&isd);
    for (i = 0; i < isdp->cardinality; i++) {
      /* Re-enable access to scope.member partition (it changes during the
         loop). */
      read_partition_at_index(ifc_scope_member, isdp->start + i);
      ismp = get_Scope_Member(&ism);
      str_ifc_declaration(ismp->index, /*is_designated_type=*/FALSE, scbp);
    }  /* for */
  }  /* if */
}  /* str_ifc_scope_index */


void an_ifc_module::str_ifc_type_index_first_part(
                                                ifc_TypeIndex       type_index,
                                                a_str_control_block *scbp)
                                                                          const
/*
Create a string representation for the specified type (first part).
FIXME: more specific
*/
{
  ifc_TypeSort tag = type_tag(type_index);

  /* Prepare to read from the proper partition for this type. */
  read_partition_at_index(tag, type_value(type_index));
  switch (tag) {
    case ifc_TypeSort_Fundamental:
      { a_const_char *basis_str = NULL;
        an_ifc_TypeSort_Fundamental itsf, *itsfp;
        itsfp = get_TypeSort_Fundamental(&itsf);
        /* Note: no check is made for nonsensical types (e.g., signed void). */
        switch (itsfp->sign) {
          case ifc_TypeSign_Plain:
            break;
          case ifc_TypeSign_Signed:
            add_string_to_text_buffer(scbp->text_buffer, "signed ");
            break;
          case ifc_TypeSign_Unsigned:
            add_string_to_text_buffer(scbp->text_buffer, "unsigned ");
            break;
          default_is_unexpected();
        }  /* if */
        switch (itsfp->basis) {
          case ifc_TypeBasis_Void:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "void";
            break;
          case ifc_TypeBasis_Bool:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "bool";
            break;
          case ifc_TypeBasis_Char:
            switch (itsfp->precision) {
              case ifc_TypePrecision_Default:
                basis_str = "char";
                break;
              case ifc_TypePrecision_Bit16:
                basis_str = "char16_t";
                break;
              case ifc_TypePrecision_Bit32:
                basis_str = "char32_t";
                break;
              default:
                unexpected_condition();
            }  /* switch */
            break;
          case ifc_TypeBasis_Wchar_t:
            switch (itsfp->precision) {
              case ifc_TypePrecision_Default: basis_str = "wchar_t";     break;
              case ifc_TypePrecision_Bit16:   basis_str = "wchar16_t";   break;
              case ifc_TypePrecision_Bit32:   basis_str = "wchar32_t";   break;
              /* FIXME: not sure how to map these: */
              case ifc_TypePrecision_Short:
              case ifc_TypePrecision_Long:
              case ifc_TypePrecision_Bit8:
              case ifc_TypePrecision_Bit64:
              case ifc_TypePrecision_Bit128:
                unexpected_condition();
                break;
              default_is_unexpected();
            }  /* switch */
            break;
          case ifc_TypeBasis_Int:
            switch (itsfp->precision) {
              case ifc_TypePrecision_Default: basis_str = "int";     break;
              case ifc_TypePrecision_Short:   basis_str = "short";   break;
              case ifc_TypePrecision_Long:    basis_str = "long";    break;
              case ifc_TypePrecision_Bit64:   basis_str = "long long"; break;
              /* FIXME: not sure how to map these: */
              case ifc_TypePrecision_Bit8:
              case ifc_TypePrecision_Bit16:
              case ifc_TypePrecision_Bit32:
              case ifc_TypePrecision_Bit128:
                unexpected_condition();
                break;
              default_is_unexpected();
            }  /* switch */
            break;
          case ifc_TypeBasis_Float:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "float";
            break;
          case ifc_TypeBasis_Double:
            if (itsfp->precision == ifc_TypePrecision_Long) {
              basis_str = "long double";
            } else {
              check_assertion(itsfp->precision == ifc_TypePrecision_Default);
              basis_str = "double";
            }  /* if */
            break;
          case ifc_TypeBasis_Nullptr:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "nullptr_t";
            break;
          case ifc_TypeBasis_Auto:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "auto";
            break;
          case ifc_TypeBasis_DecltypeAuto:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "decltype(auto)";
            break;
          case ifc_TypeBasis_Class:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "class";
            break;
          case ifc_TypeBasis_Struct:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "struct";
            break;
          case ifc_TypeBasis_Union:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "union";
            break;
          case ifc_TypeBasis_Namespace:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "namespace";
            break;
          case ifc_TypeBasis_Interface:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "__interface";
            break;
          case ifc_TypeBasis_Enum:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "enum";
            break;
          case ifc_TypeBasis_Typename:
            /* FIXME: not sure what goes here. */
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "typename";
            break;
          case ifc_TypeBasis_Ellipsis:
            check_assertion(itsfp->precision == ifc_TypePrecision_Default);
            basis_str = "...";
            break;
          case ifc_TypeBasis_SegmentType:
          case ifc_TypeBasis_Function:
          case ifc_TypeBasis_Empty:
          case ifc_TypeBasis_VariableTemplate:
            /* FIXME: not implemented yet */
            unexpected_condition();
            break;
          default_is_unexpected();
        }  /* switch */
        if (basis_str != NULL) {
          add_string_to_text_buffer(scbp->text_buffer, basis_str);
        }  /* if */
      }
      break;
    case ifc_TypeSort_Designated:
      /* A type's name (e.g., "A"). */
      { an_ifc_TypeSort_Designated itsd, *itsdp;
        itsdp = get_TypeSort_Designated(&itsd);
        str_ifc_declaration(itsdp->decl, /*is_designated_type=*/TRUE, scbp);
      }
      break;
    case ifc_TypeSort_Method:  /* FIXME: Merge with below. */
      { an_ifc_TypeSort_Method itsm, *itsmp;
        itsmp = get_TypeSort_Method(&itsm);
        /* FIXME: Need more here. */
        str_ifc_function_type_traits(itsmp->traits, scbp);
        /* Emit return type on the first pass (parameters are emitted on the
           second pass). */
        str_ifc_type_index(itsmp->target, scbp);
      }
      break;
    case ifc_TypeSort_Function:
      { an_ifc_TypeSort_Function itsf, *itsfp;
        itsfp = get_TypeSort_Function(&itsf);
        /* FIXME: need more here. */
        str_ifc_function_type_traits(itsfp->traits, scbp);
        /* Emit return type on the first pass (parameters are emitted on the
           second pass). */
        str_ifc_type_index(itsfp->target, scbp);
      }
      break;
    case ifc_TypeSort_Qualified:
      { an_ifc_TypeSort_Qualified itsq, *itsqp;
        itsqp = get_TypeSort_Qualified(&itsq);
        str_ifc_qualifiers(itsqp->qualifiers, scbp);
        str_ifc_type_index(itsqp->unqualified, scbp);
      }
      break;
    case ifc_TypeSort_Pointer:
      { an_ifc_TypeSort_Pointer itsp, *itspp;
        itspp = get_TypeSort_Pointer(&itsp);
        str_ifc_type_index(itspp->pointee, scbp);
        add_char_to_text_buffer(scbp->text_buffer, '*');
      }
      break;
    case ifc_TypeSort_PointerToMember:
      { an_ifc_TypeSort_PointerToMember itsptm, *itsptmp;
        itsptmp = get_TypeSort_PointerToMember(&itsptm);
        str_ifc_type_index(itsptmp->member, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_type_index(itsptmp->scope, scbp);
        add_string_to_text_buffer(scbp->text_buffer, "::");
      }
      break;
    case ifc_TypeSort_LvalueReference:
      { an_ifc_TypeSort_LvalueReference itslr, *itslrp;
        itslrp = get_TypeSort_LvalueReference(&itslr);
        str_ifc_type_index(itslrp->referee, scbp);
        add_char_to_text_buffer(scbp->text_buffer, '&');
      }
      break;
    case ifc_TypeSort_RvalueReference:
      { an_ifc_TypeSort_RvalueReference itsrr, *itsrrp;
        itsrrp = get_TypeSort_RvalueReference(&itsrr);
        str_ifc_type_index(itsrrp->referee, scbp);
        add_string_to_text_buffer(scbp->text_buffer, "&&");
      }
      break;
    case ifc_TypeSort_Tuple:
      { an_ifc_TypeSort_Tuple itst, *itstp;
        unsigned int i;
        itstp = get_TypeSort_Tuple(&itst);
        /* A list of types. */
        for (i = 0; i < itstp->cardinality; i++) {
          ifc_TypeIndex ti;
          read_partition_at_index(ifc_heap_type,
                                  itstp->start + i);
          GET_TypeIndex(ti, /*from_header=*/FALSE);
          str_ifc_type_index(ti, scbp);
          /* FIXME: not really sure what the separator should be here: */
          if (i+1 < itstp->cardinality) {
            add_char_to_text_buffer(scbp->text_buffer, ',');
          }  /* if */
        }  /* for */
      }
      break;
    case ifc_TypeSort_Array:
      { an_ifc_TypeSort_Array itsa, *itsap;
        itsap = get_TypeSort_Array(&itsa);
        str_ifc_type_index(itsap->element, scbp);
      }
      break;
    case ifc_TypeSort_Base:
      { an_ifc_TypeSort_Base itsb, *itsbp;
        itsbp = get_TypeSort_Base(&itsb);
        if (itsbp->access != ifc_Access_None) {
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
    case ifc_TypeSort_VendorExtension:
      { an_ifc_TypeSort_VendorExtension itsve;
        get_TypeSort_VendorExtension(&itsve);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_TypeSort_Tor:
      { an_ifc_TypeSort_Tor itst;
        get_TypeSort_Tor(&itst);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_TypeSort_Syntactic:
      { an_ifc_TypeSort_Syntactic itss;
        get_TypeSort_Syntactic(&itss);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_TypeSort_Expansion:
      { an_ifc_TypeSort_Expansion itse;
        get_TypeSort_Expansion(&itse);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_TypeSort_Typename:
      { an_ifc_TypeSort_Typename itst;
        get_TypeSort_Typename(&itst);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_TypeSort_Unaligned:
      { an_ifc_TypeSort_Unaligned itsu;
        get_TypeSort_Unaligned(&itsu);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_TypeSort_Decltype:
      { an_ifc_TypeSort_Decltype itsd;
        get_TypeSort_Decltype(&itsd);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_TypeSort_Forall:
      { an_ifc_TypeSort_Forall itsfa;
        get_TypeSort_Forall(&itsfa);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_TypeSort_SyntaxTree:
      { an_ifc_TypeSort_SyntaxTree itsst;
        get_TypeSort_SyntaxTree(&itsst);
        /* FIXME: Handle this. */
      }
      break;
    default:
#if DEBUG
      if (db_flag_is_set("ms_ignore")) {
        (void)fprintf(f_debug, "Unsupported type: %d, %u\n",
                      tag, type_value(type_index));
      }  /* if */
#endif /* DEBUG */
      break;
  }  /* switch */
}  /* str_ifc_type_index_first_part */


void an_ifc_module::str_ifc_type_index_second_part(
                                                ifc_TypeIndex       type_index,
                                                a_str_control_block *scbp)
                                                                          const
/*
Create a string representation for the specified type (second part).
FIXME: more specific
*/
{
  ifc_TypeSort tag = type_tag(type_index);

  /* Prepare to read from the proper partition for this type. */
  read_partition_at_index(tag, type_value(type_index));
  switch (tag) {
    case ifc_TypeSort_Fundamental:
    case ifc_TypeSort_Designated:
    case ifc_TypeSort_Qualified:
    case ifc_TypeSort_Pointer:
    case ifc_TypeSort_PointerToMember:
    case ifc_TypeSort_LvalueReference:
    case ifc_TypeSort_RvalueReference:
    case ifc_TypeSort_Tuple:
    case ifc_TypeSort_Base:
      /* Handled in str_ifc_type_index_first_part. */
      break;
    case ifc_TypeSort_Method:  /* FIXME: Merge with below. */
      { an_ifc_TypeSort_Method itsm, *itsmp;
        itsmp = get_TypeSort_Method(&itsm);
        /* FIXME: lots here. */
        if (itsmp->source == 0) {
          /* No parameters. */
          add_string_to_text_buffer(scbp->text_buffer, "()");
        } else {
          add_char_to_text_buffer(scbp->text_buffer, '(');
          str_ifc_type_index(itsmp->source, scbp);
          add_char_to_text_buffer(scbp->text_buffer, ')');
        }  /* if */
        str_ifc_noexcept_specification(&itsmp->eh_spec, scbp);
      }
      break;
    case ifc_TypeSort_Function:
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
    case ifc_TypeSort_Array:
      { an_ifc_TypeSort_Array itsa, *itsap;
        itsap = get_TypeSort_Array(&itsa);
        add_char_to_text_buffer(scbp->text_buffer, '[');
        str_ifc_expr_index(itsap->extent, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ']');
      }
      break;
    case ifc_TypeSort_VendorExtension:
    case ifc_TypeSort_Syntactic:
    case ifc_TypeSort_Expansion:
    case ifc_TypeSort_Typename:
    case ifc_TypeSort_Unaligned:
    case ifc_TypeSort_Decltype:
    case ifc_TypeSort_SyntaxTree:
    default:
#if DEBUG
      if (db_flag_is_set("ms_ignore")) {
        (void)fprintf(f_debug, "Unsupported type: %d, %u\n",
                      tag, type_value(type_index));
      }  /* if */
#endif /* DEBUG */
      break;
  }  /* switch */
}  /* str_ifc_type_index_second_part */


void an_ifc_module::str_ifc_type_index(ifc_TypeIndex       type_index,
                                       a_str_control_block *scbp) const
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
                                        a_str_control_block *scbp) const
/*
This is a utility routine to add textual representations for the common fields
of the various IFC DeclSorts (e.g., DeclSort::Variable).  If a particular
DeclSort doesn't have a SourceLocation, Access, BasicSpecifiers, ObjectTraits,
or Alignment field, a nominal value can be supplied (which will suppress its
output).
*/
{
  str_ifc_source_location(locus, scbp);
  if (access != ifc_Access_None) {
    str_ifc_access(access, scbp);
    add_string_to_text_buffer(scbp->text_buffer, ": ");
  }  /* if */
  if (specifiers != ifc_BasicSpecifiers_Cxx) {
    str_ifc_basic_specifiers(specifiers, scbp);
  }  /* if */
  if (traits != ifc_ObjectTraits_None) {
    str_ifc_object_traits(traits, scbp);
  }  /* if */
}  /* str_ifc_common_decl */


void an_ifc_module::str_ifc_class_definition(an_ifc_DeclSort_Scope *idssp,
                                             a_str_control_block   *scbp) const
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
                                        a_str_control_block *scbp) const
/*
Generate a string for the specified declaration.
FIXME: Perhaps have a "flags" argument rather than is_designated_type?
*/
{
  ifc_DeclSort             tag = decl_tag(decl_index);
  a_boolean                end_decl = TRUE;

  /* Prepare to read from the proper partition for this declaration. */
  read_partition_at_index(tag, decl_value(decl_index));
  switch (tag) {
    case ifc_DeclSort_Variable:
      { an_ifc_DeclSort_Variable idsv, *idsvp;
        idsvp = get_DeclSort_Variable(&idsv);
        /* Emit a variable declaration. */
        /* FIXME: idsvp->alignment exists but is an ExprIndex. */
        str_ifc_common_decl(&idsvp->locus, idsvp->access, idsvp->specifier,
                            idsvp->traits, scbp);
        str_ifc_type_index(idsvp->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_name_index(idsvp->name, scbp);
        if (idsvp->initializer != 0) {
          add_string_to_text_buffer(scbp->text_buffer, " = ");
          str_ifc_expr_index(idsvp->initializer, scbp);
        }  /* if */
      }
      break;
    case ifc_DeclSort_Scope:
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
    case ifc_DeclSort_Field:
      { an_ifc_DeclSort_Field idsf, *idsfp;
        idsfp = get_DeclSort_Field(&idsf);
        /* FIXME: idsfp->alignment exists but is an ExprIndex. */
        str_ifc_common_decl(&idsfp->locus, idsfp->access, idsfp->specifier,
                            idsfp->traits, scbp);
        str_ifc_type_index(idsfp->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_text_offset(idsfp->name, scbp);
      }
      break;
    case ifc_DeclSort_Bitfield:
      { an_ifc_DeclSort_Bitfield idsb, *idsbp;
        idsbp = get_DeclSort_Bitfield(&idsb);
        str_ifc_common_decl(&idsbp->locus, idsbp->access, idsbp->specifier,
                            idsbp->traits, scbp);
        str_ifc_type_index(idsbp->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_text_offset(idsbp->name, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ':');
        str_ifc_add_number((a_host_large_unsigned)idsbp->width, scbp);
      }
      break;
    case ifc_DeclSort_Function:
      { an_ifc_DeclSort_Function idsf, *idsfp;
        idsfp = get_DeclSort_Function(&idsf);
        str_ifc_common_decl(&idsfp->locus, idsfp->access,
                            (ifc_BasicSpecifiers)ifc_BasicSpecifiers_Cxx,
                            ifc_ObjectTraits_None, scbp);
        if (idsfp->access != ifc_Access_None) {
          /* This is a static member function. */
          add_string_to_text_buffer(scbp->text_buffer, "static ");
        }  /* if */
        str_ifc_function_traits(idsfp->traits, /*prefix=*/TRUE, scbp);
        str_ifc_type_index_first_part(idsfp->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_name_index(idsfp->name, scbp);
        str_ifc_type_index_second_part(idsfp->type, scbp);
        str_ifc_function_traits(idsfp->traits, /*prefix=*/FALSE, scbp);
        /* FIXME: default_arguments is unused */
      }
      break;
    case ifc_DeclSort_Method:
      { an_ifc_DeclSort_Method idsm, *idsmp;
        idsmp = get_DeclSort_Method(&idsm);
        str_ifc_common_decl(&idsmp->locus, idsmp->access,
                            (ifc_BasicSpecifiers)ifc_BasicSpecifiers_Cxx,
                            ifc_ObjectTraits_None, scbp);
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
    case ifc_DeclSort_Intrinsic:
      { an_ifc_DeclSort_Intrinsic idsi, *idsip;
        idsip = get_DeclSort_Intrinsic(&idsi);
        str_ifc_common_decl(&idsip->locus, idsip->access,
                            (ifc_BasicSpecifiers)ifc_BasicSpecifiers_Cxx,
                            ifc_ObjectTraits_None, scbp);
        str_ifc_type_index_first_part(idsip->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_text_offset(idsip->name, scbp);
        str_ifc_type_index_second_part(idsip->type, scbp);
      }
      break;
    case ifc_DeclSort_Constructor:
      { an_ifc_DeclSort_Constructor idsc, *idscp;
        an_ifc_TypeSort_Tor         itst, *itstp;
        idscp = get_DeclSort_Constructor(&idsc);
        check_assertion(type_tag(idscp->type) == ifc_TypeSort_Tor);
        read_partition_at_index(idscp->type);
        itstp = get_TypeSort_Tor(&itst);
        str_ifc_common_decl(&idscp->locus, idscp->access,
                            (ifc_BasicSpecifiers)ifc_BasicSpecifiers_Cxx,
                            ifc_ObjectTraits_None, scbp);
        str_ifc_function_traits(idscp->traits, /*prefix=*/TRUE, scbp);
        /* Note that idscp->name points to a "{ctor}" string, so get the
           type's name by going through the home_scope field. */
        str_ifc_class_name(idscp->home_scope, scbp);
        add_char_to_text_buffer(scbp->text_buffer, '(');
        if (itstp->source != 0) {
          str_ifc_type_index(itstp->source, scbp);
        }  /* if */
        /* FIXME: todo: idscp->default_arguments */
        add_char_to_text_buffer(scbp->text_buffer, ')');
        str_ifc_function_traits(idscp->traits, /*prefix=*/FALSE, scbp);
        /* FIXME: todo: idscp->eh_spec and idcsp->convention */
      }
      break;
    case ifc_DeclSort_Destructor:
      { an_ifc_DeclSort_Destructor idsd, *idsdp;
        idsdp = get_DeclSort_Destructor(&idsd);
        str_ifc_common_decl(&idsdp->locus, idsdp->access,
                            (ifc_BasicSpecifiers)ifc_BasicSpecifiers_Cxx,
                            ifc_ObjectTraits_None, scbp);
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
    case ifc_DeclSort_Alias:
      { an_ifc_DeclSort_Alias       idsta, *idstap;
        ifc_TypeSort                alias_tag;
        idstap = get_DeclSort_Alias(&idsta);
        /* FIXME: lots missing */
        alias_tag = type_tag(idstap->type);
        /* Read the type to see what kind it is. */
        read_partition_at_index(idstap->type);
        if (alias_tag == ifc_TypeSort_Fundamental) {
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          itsfp = get_TypeSort_Fundamental(&itsf);
          if (itsfp->basis == ifc_TypeBasis_Typename) {
            /* A type alias. */
            add_string_to_text_buffer(scbp->text_buffer, "typedef ");
            str_ifc_type_index(idstap->aliasee, scbp);
            add_char_to_text_buffer(scbp->text_buffer, ' ');
            str_ifc_text_offset(idstap->name, scbp);
          } else if (itsfp->basis == ifc_TypeBasis_Namespace) {
            /* A namespace alias. */
            /* FIXME: unimplemented. */
            unexpected_condition();
          } else {
            unexpected_condition();
          }  /* if */
        } else if (alias_tag == ifc_TypeSort_Forall) {
          /* FIXME: unimplemented. */
          unexpected_condition();
        } else {
          unexpected_condition();
        }  /* if */
      }
      break;
    case ifc_DeclSort_Enumeration:
      { an_ifc_DeclSort_Enumeration idse, *idsep;
        an_ifc_TypeSort_Fundamental itsf, *itsfp;
        idsep = get_DeclSort_Enumeration(&idse);
        /* Emit an enumeration declaration. */
        /* FIXME: idsep->alignment exists but is an ExprIndex. */
        str_ifc_common_decl(&idsep->locus, idsep->access, idsep->specifiers,
                            (ifc_ObjectTraits)ifc_ObjectTraits_None, scbp);
        check_assertion(type_tag(idsep->type) == ifc_TypeSort_Fundamental);
        /* Read the type to see what kind it is. */
        read_partition_at_index(ifc_type_fundamental, type_value(idsep->type));
        itsfp = get_TypeSort_Fundamental(&itsf);
        add_string_to_text_buffer(scbp->text_buffer, "enum ");
        if (itsfp->basis == ifc_TypeBasis_Class) {
          add_string_to_text_buffer(scbp->text_buffer, "class ");
        } else if (itsfp->basis == ifc_TypeBasis_Struct) {
          add_string_to_text_buffer(scbp->text_buffer, "struct ");
        } else {
          check_assertion(itsfp->basis == ifc_TypeBasis_Enum);
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
            str_ifc_declaration(make_decl_index(ifc_DeclSort_Enumerator,
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
    case ifc_DeclSort_Enumerator:
      { an_ifc_DeclSort_Enumerator idse, *idsep;
        idsep = get_DeclSort_Enumerator(&idse);
        /* Emit an enumerator declaration. */
        /* FIXME: idsep->access unused here (to suppress access field): */
        str_ifc_common_decl(&idsep->locus, ifc_Access_None, idsep->specifier,
                            (ifc_ObjectTraits)ifc_ObjectTraits_None, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_text_offset(idsep->name, scbp);
        if (idsep->initializer != 0) {
          add_string_to_text_buffer(scbp->text_buffer, " = ");
          str_ifc_expr_index(idsep->initializer, scbp);
        }  /* if */
        end_decl = FALSE;
      }
      break;
    case ifc_DeclSort_VendorExtension:
      { an_ifc_DeclSort_VendorExtension idsve;
        get_DeclSort_VendorExtension(&idsve);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_Parameter:
      { an_ifc_DeclSort_Parameter idsp;
        get_DeclSort_Parameter(&idsp);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_Temploid:
      { an_ifc_DeclSort_Temploid idst;
        get_DeclSort_Temploid(&idst);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_Template:
      { an_ifc_DeclSort_Template idst;
        get_DeclSort_Template(&idst);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_PartialSpecialization:
      { an_ifc_DeclSort_PartialSpecialization idsps;
        get_DeclSort_PartialSpecialization(&idsps);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_ExplicitSpecialization:
      { an_ifc_DeclSort_ExplicitSpecialization idses;
        get_DeclSort_ExplicitSpecialization(&idses);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_ExplicitInstantiation:
      { an_ifc_DeclSort_ExplicitInstantiation idsei;
        get_DeclSort_ExplicitInstantiation(&idsei);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_Concept:
      { an_ifc_DeclSort_Concept idsc;
        get_DeclSort_Concept(&idsc);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_InheritedConstructor:
      { an_ifc_DeclSort_InheritedConstructor idsic;
        get_DeclSort_InheritedConstructor(&idsic);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_Reference:
      { an_ifc_DeclSort_Reference idsr;
        get_DeclSort_Reference(&idsr);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_Property:
      { an_ifc_DeclSort_Property idsp;
        get_DeclSort_Property(&idsp);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_OutputSegment:
      { an_ifc_DeclSort_OutputSegment idsos;
        get_DeclSort_OutputSegment(&idsos);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_UsingDeclaration:
      { an_ifc_DeclSort_UsingDeclaration idsud;
        get_DeclSort_UsingDeclaration(&idsud);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_UsingDirective:
      { an_ifc_DeclSort_UsingDirective idsud;
        get_DeclSort_UsingDirective(&idsud);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_Friend:
      { an_ifc_DeclSort_Friend idsf;
        get_DeclSort_Friend(&idsf);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_SyntaxTree:
      { an_ifc_DeclSort_SyntaxTree idsst;
        get_DeclSort_SyntaxTree(&idsst);
        /* FIXME: Handle this. */
      }
      break;
    case ifc_DeclSort_Tuple:
      { an_ifc_DeclSort_Tuple idst;
        get_DeclSort_Tuple(&idst);
        /* FIXME: Handle this. */
      }
      break;
    default:
#if DEBUG
      if (db_flag_is_set("ms_ignore")) {
        (void)fprintf(f_debug, "[unsupported declaration: %s, %u]\n",
                      str_for_decl_tag(tag), decl_value(decl_index));
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


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
void an_ifc_module::str_ifc_statement(
                                     ifc_StmtIndex                  stmt_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                          const
/*
Generate a string for the specified statement.
*/
{
  ifc_StmtSort tag = stmt_tag(stmt_index);

  read_partition_at_index(tag, stmt_value(stmt_index));
  switch (tag) {
    case ifc_StmtSort_VendorExtension:
      { an_ifc_StmtSort_VendorExtension issve;
        get_StmtSort_VendorExtension(&issve);
        unexpected_condition_str("StmtSort::VendorExtension"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_Empty:
      { an_ifc_StmtSort_Empty isse;
        get_StmtSort_Empty(&isse);
        unexpected_condition_str("StmtSort::Empty"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_If:
      { an_ifc_StmtSort_If issi;
        get_StmtSort_If(&issi);
        unexpected_condition_str("StmtSort::If"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_For:
      { an_ifc_StmtSort_For issf;
        get_StmtSort_For(&issf);
        unexpected_condition_str("StmtSort::For"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_Case:
      { an_ifc_StmtSort_Case issc;
        get_StmtSort_Case(&issc);
        unexpected_condition_str("StmtSort::Case"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_While:
      { an_ifc_StmtSort_While issw;
        get_StmtSort_While(&issw);
        unexpected_condition_str("StmtSort::While"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_Block:
      { an_ifc_StmtSort_Block issb;
        get_StmtSort_Block(&issb);
        unexpected_condition_str("StmtSort::Block"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_Break:
      { an_ifc_StmtSort_Break issb;
        get_StmtSort_Break(&issb);
        unexpected_condition_str("StmtSort::Break"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_Switch:
      { an_ifc_StmtSort_Switch isss;
        get_StmtSort_Switch(&isss);
        unexpected_condition_str("StmtSort::Switch"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_DoWhile:
      { an_ifc_StmtSort_DoWhile issdw;
        get_StmtSort_DoWhile(&issdw);
        unexpected_condition_str("StmtSort::DoWhile"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_Default:
      { an_ifc_StmtSort_Default issd;
        get_StmtSort_Default(&issd);
        unexpected_condition_str("StmtSort::Default"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_Continue:
      { an_ifc_StmtSort_Continue issc;
        get_StmtSort_Continue(&issc);
        unexpected_condition_str("StmtSort::Continue"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_Expression:
      { an_ifc_StmtSort_Expression isse;
        get_StmtSort_Expression(&isse);
        unexpected_condition_str("StmtSort::Expression"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_Return:
      { an_ifc_StmtSort_Return issr;
        get_StmtSort_Return(&issr);
        unexpected_condition_str("StmtSort::Return"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_VariableDecl:
      { an_ifc_StmtSort_VariableDecl issvd;
        get_StmtSort_VariableDecl(&issvd);
        unexpected_condition_str("StmtSort::VariableDecl"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_Expansion:
      { an_ifc_StmtSort_Expansion isse;
        get_StmtSort_Expansion(&isse);
        unexpected_condition_str("StmtSort::Expansion"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_SyntaxTree:
      { an_ifc_StmtSort_SyntaxTree issst;
        get_StmtSort_SyntaxTree(&issst);
        unexpected_condition_str("StmtSort::SyntaxTree"
                                 " is not yet handled");
      }
      break;
    case ifc_StmtSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unknown StmtSort kind");
  }  /* switch */
}  /* str_ifc_statement */


void an_ifc_module::str_ifc_string_literal(ifc_StringIndex     str_index,
                                           a_str_control_block *scbp) const
/*
Generate a string for the specified string literal.
*/
{
  ifc_StringSort        tag = str_tag(str_index);
  an_ifc_String_Literal str_lit;

  read_partition_at_index(ifc_const_str, str_value(str_index));
  get_String_Literal(&str_lit);
  if (tag != ifc_StringSort_Ordinary) {
    unexpected_condition_str("Non-ordinary strings are not yet handled.");
  }  /* if */
  add_string_with_length_to_text_buffer(scbp->text_buffer,
                                        get_string_at_offset(str_lit.start),
                                        str_lit.length);
  if (str_lit.suffix != 0) {
    a_const_char *str = get_string_at_offset(str_lit.suffix);
    add_string_to_text_buffer(scbp->text_buffer, str);
  }  /* if */
}  /* str_ifc_string_literal */


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
void an_ifc_module::str_ifc_chart(ifc_ChartIndex                 chart_index,
                                  ARG_UNUSED a_str_control_block *scbp) const
/*
Generate a string for the specified chart.
*/
{
  ifc_ChartSort tag = chart_tag(chart_index);

  read_partition_at_index(tag, chart_value(chart_index));
  switch (tag) {
    case ifc_ChartSort_None:
      { an_ifc_ChartSort_None icsn;
        get_ChartSort_None(&icsn);
        unexpected_condition_str("ChartSort::None is unspecified.");
      }
      break;
    case ifc_ChartSort_Unilevel:
      { an_ifc_ChartSort_Unilevel icsu;
        get_ChartSort_Unilevel(&icsu);
        unexpected_condition_str("ChartSort::Unilevel is unspecified.");
      }
      break;
    case ifc_ChartSort_Multilevel:
      { an_ifc_ChartSort_Multilevel icsm;
        get_ChartSort_Multilevel(&icsm);
        unexpected_condition_str("ChartSort::Multilevel is unspecified.");
      }
      break;
    case ifc_ChartSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unknown ChartSort kind");
  }  /* switch */
}  /* str_ifc_chart */


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Deprecated>(
                                     ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                          const
/*
Generate a string for the specified associated deprecation trait.
*/
{
  an_ifc_Trait_Deprecated itd;

  read_partition_at_index(ifc_trait_deprecated, decl_index);
  get_Trait_Deprecated(&itd);
  unexpected_condition_str("AssociatedTrait<Deprecated> is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_Deprecated> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Specialization>(
                                     ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                          const
/*
Generate a string for the specified associated template specialization trait.
*/
{
  an_ifc_Trait_Specialization its;

  read_partition_at_index(ifc_trait_specialization, decl_index);
  get_Trait_Specialization(&its);
  unexpected_condition_str("AssociatedTrait<Specialization>"
                           " is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_Specialization> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Friend>(
                                     ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                          const
/*
Generate a string for the specified associated class friend trait.
*/
{
  an_ifc_Trait_Friend itf;

  read_partition_at_index(ifc_trait_friend, decl_index);
  get_Trait_Friend(&itf);
  unexpected_condition_str("AssociatedTrait<Friend> is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_Friend> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_FunctionDefinition>(
                                     ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                          const
/*
Generate a string for the specified associated constexpr function trait.
*/
{
  an_ifc_Trait_FunctionDefinition itfd;

  read_partition_at_index(ifc_trait_function_definition, decl_index);
  get_Trait_FunctionDefinition(&itfd);
  unexpected_condition_str("AssociatedTrait<FunctionDefinition>"
                           " is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_FunctionDefinition> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_AliasTemplate>(
                                     ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                          const
/*
Generate a string for the specified associated template alias trait.
*/
{
  an_ifc_Trait_AliasTemplate itat;

  read_partition_at_index(ifc_trait_alias_template, decl_index);
  get_Trait_AliasTemplate(&itat);
  unexpected_condition_str("AssociatedTrait<AliasTemplate> is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_AliasTemplate> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_DeductionGuides>(
                                     ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                          const
/*
Generate a string for the specified associated deduction guides trait.
*/
{
  an_ifc_Trait_DeductionGuides itdg;

  read_partition_at_index(ifc_trait_deduction_guides, decl_index);
  get_Trait_DeductionGuides(&itdg);
  unexpected_condition_str("AssociatedTrait<DeductionGuides>"
                           " is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_DeductionGuides> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Requires>(
                                     ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                          const
/*
Generate a string for the specified associated requires trait.
*/
{
  an_ifc_Trait_Requires itr;

  read_partition_at_index(ifc_trait_requires, decl_index);
  get_Trait_Requires(&itr);
  unexpected_condition_str("AssociatedTrait<Requires> is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_Requires> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Attribute>(
                                     ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                          const
/*
Generate a string for the specified associated template alias trait.
*/
{
  an_ifc_Trait_Attribute ita;

  read_partition_at_index(ifc_trait_attribute, decl_index);
  get_Trait_Attribute(&ita);
  unexpected_condition_str("AssociatedTrait<Attribute> is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_Attribute> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcVendorTrait>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp) const
/*
Generate a string for the specified associated MSVC Vendor trait.
*/
{
  an_ifc_Trait_MsvcVendorTrait itmsvct;

  read_partition_at_index(ifc_msvc_trait_vendor_traits, decl_index);
  get_Trait_MsvcVendorTrait(&itmsvct);
  str_ifc_msvc_traits(itmsvct.trait, scbp);
}  /* str_ifc_associated_trait<an_ifc_Trait_MsvcVendorTrait> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcUuid>(
                                             ifc_DeclIndex       decl_index,
                                             a_str_control_block *scbp) const
/*
Generate a string for the specified associated MSVC UUID trait.
*/
{
  an_ifc_Trait_MsvcUuid itmsvcuuid;
  char                  str[16];

  read_partition_at_index(ifc_msvc_trait_uuid, decl_index);
  get_Trait_MsvcUuid(&itmsvcuuid);
  snprintf(str, sizeof(str), "%04hx", itmsvcuuid.uuid);
  add_string_to_text_buffer(scbp->text_buffer, str);
}  /* str_ifc_associated_trait<an_ifc_Trait_MsvcUuid> */


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcFuncParams>(
                                    ifc_DeclIndex                  decl_index,
                                    ARG_UNUSED a_str_control_block *scbp) const
/*
Generate a string for the specified associated MSVC UUID trait.
*/
{
  an_ifc_Trait_MsvcFuncParams itmsvcfuncparams;

  read_partition_at_index(ifc_msvc_trait_uuid, decl_index);
  get_Trait_MsvcFuncParams(&itmsvcfuncparams);
  unexpected_condition_str("AssociatedTrait<MsvcFuncParams>"
                           " is not yet handled.");
}  /* str_ifc_associated_trait<an_ifc_Trait_MsvcUuid> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_MsvcDeclAttrs>(
                                    ifc_DeclIndex                  decl_index,
                                    ARG_UNUSED a_str_control_block *scbp) const
/*
Generate a string for the specified associated MSVC UUID trait.
*/
{
  an_ifc_Trait_MsvcDeclAttrs itmsvcdeclattrs;

  read_partition_at_index(ifc_msvc_trait_decl_attrs, decl_index);
  get_Trait_MsvcDeclAttrs(&itmsvcdeclattrs);
  unexpected_condition_str("AssociatedTrait<MsvcDeclAttrs>"
                           " is not yet handled.");
}  /* str_ifc_associated_trait<an_ifc_Trait_MsvcDeclAttrs> */

   
/*lint -e2707*/ /* Remove when the routine returns to its caller. */
void an_ifc_module::str_ifc_syntax_node(
                                   ifc_SyntaxIndex                syntax_index,
                                   ARG_UNUSED a_str_control_block *scbp) const
/*
Generate a string for the specified syntax tree node.
*/
{
  ifc_SyntaxSort tag = syntax_tag(syntax_index);

  read_partition_at_index(tag, syntax_value(syntax_index));
  switch (tag) {
    case ifc_SyntaxSort_VendorExtension:
      { an_ifc_SyntaxSort_VendorExtension issve;
        get_SyntaxSort_VendorExtension(&issve);
        unexpected_condition_str("SyntaxSort::VendorExtension"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_SimpleTypeSpecifier:
      { an_ifc_SyntaxSort_SimpleTypeSpecifier isssts;
        get_SyntaxSort_SimpleTypeSpecifier(&isssts);
        unexpected_condition_str("SyntaxSort::SimpleTypeSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_DecltypeSpecifier:
      { an_ifc_SyntaxSort_DecltypeSpecifier issds;
        get_SyntaxSort_DecltypeSpecifier(&issds);
        unexpected_condition_str("SyntaxSort::DecltypeSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_PlaceholderTypeSpecifier:
      { an_ifc_SyntaxSort_PlaceholderTypeSpecifier isspts;
        get_SyntaxSort_PlaceholderTypeSpecifier(&isspts);
        unexpected_condition_str("SyntaxSort::PlaceholderTypeSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TypeSpecifierSeq:
      { an_ifc_SyntaxSort_TypeSpecifierSeq isstss;
        get_SyntaxSort_TypeSpecifierSeq(&isstss);
        unexpected_condition_str("SyntaxSort::TypeSpecifierSeq"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_DeclSpecifierSeq:
      { an_ifc_SyntaxSort_DeclSpecifierSeq issdss;
        get_SyntaxSort_DeclSpecifierSeq(&issdss);
        unexpected_condition_str("SyntaxSort::DeclSpecifierSeq"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_VirtualSpecifierSeq:
      { an_ifc_SyntaxSort_VirtualSpecifierSeq issvss;
        get_SyntaxSort_VirtualSpecifierSeq(&issvss);
        unexpected_condition_str("SyntaxSort::VirtualSpecifierSeq"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_NoexceptSpecification:
      { an_ifc_SyntaxSort_NoexceptSpecification issns;
        get_SyntaxSort_NoexceptSpecification(&issns);
        unexpected_condition_str("SyntaxSort::NoexceptSpecification"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ExplicitSpecifier:
      { an_ifc_SyntaxSort_ExplicitSpecifier isses;
        get_SyntaxSort_ExplicitSpecifier(&isses);
        unexpected_condition_str("SyntaxSort::ExplicitSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_EnumSpecifier:
      { an_ifc_SyntaxSort_EnumSpecifier isses;
        get_SyntaxSort_EnumSpecifier(&isses);
        unexpected_condition_str("SyntaxSort::EnumSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_EnumeratorDefinition:
      { an_ifc_SyntaxSort_EnumeratorDefinition issed;
        get_SyntaxSort_EnumeratorDefinition(&issed);
        unexpected_condition_str("SyntaxSort::EnumeratorDefinition"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ClassSpecifier:
      { an_ifc_SyntaxSort_ClassSpecifier isscs;
        get_SyntaxSort_ClassSpecifier(&isscs);
        unexpected_condition_str("SyntaxSort::ClassSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_MemberSpecification:
      { an_ifc_SyntaxSort_MemberSpecification issms;
        get_SyntaxSort_MemberSpecification(&issms);
        unexpected_condition_str("SyntaxSort::MemberSpecification"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_MemberDeclaration:
      { an_ifc_SyntaxSort_MemberDeclaration issmd;
        get_SyntaxSort_MemberDeclaration(&issmd);
        unexpected_condition_str("SyntaxSort::MemberDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_MemberDeclarator:
      { an_ifc_SyntaxSort_MemberDeclarator issmd;
        get_SyntaxSort_MemberDeclarator(&issmd);
        unexpected_condition_str("SyntaxSort::MemberDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_AccessSpecifier:
      { an_ifc_SyntaxSort_AccessSpecifier issas;
        get_SyntaxSort_AccessSpecifier(&issas);
        unexpected_condition_str("SyntaxSort::AccessSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_BaseSpecifierList:
      { an_ifc_SyntaxSort_BaseSpecifierList issbsl;
        get_SyntaxSort_BaseSpecifierList(&issbsl);
        unexpected_condition_str("SyntaxSort::BaseSpecifierList"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_BaseSpecifier:
      { an_ifc_SyntaxSort_BaseSpecifier issbs;
        get_SyntaxSort_BaseSpecifier(&issbs);
        unexpected_condition_str("SyntaxSort::BaseSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TypeId:
      { an_ifc_SyntaxSort_TypeId isstid;
        get_SyntaxSort_TypeId(&isstid);
        unexpected_condition_str("SyntaxSort::TypeId"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TrailingReturnType:
      { an_ifc_SyntaxSort_TrailingReturnType isstrt;
        get_SyntaxSort_TrailingReturnType(&isstrt);
        unexpected_condition_str("SyntaxSort::TrailingReturnType"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_Declarator:
      { an_ifc_SyntaxSort_Declarator issd;
        get_SyntaxSort_Declarator(&issd);
        unexpected_condition_str("SyntaxSort::Declarator"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_PointerDeclarator:
      { an_ifc_SyntaxSort_PointerDeclarator isspd;
        get_SyntaxSort_PointerDeclarator(&isspd);
        unexpected_condition_str("SyntaxSort::PointerDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ArrayDeclarator:
      { an_ifc_SyntaxSort_ArrayDeclarator issad;
        get_SyntaxSort_ArrayDeclarator(&issad);
        unexpected_condition_str("SyntaxSort::ArrayDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_FunctionDeclarator:
      { an_ifc_SyntaxSort_FunctionDeclarator issfd;
        get_SyntaxSort_FunctionDeclarator(&issfd);
        unexpected_condition_str("SyntaxSort::FunctionDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ArrayOrFunctionDeclarator:
      { an_ifc_SyntaxSort_ArrayOrFunctionDeclarator issafd;
        get_SyntaxSort_ArrayOrFunctionDeclarator(&issafd);
        unexpected_condition_str("SyntaxSort::ArrayOrFunctionDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ParameterDeclarator:
      { an_ifc_SyntaxSort_ParameterDeclarator isspd;
        get_SyntaxSort_ParameterDeclarator(&isspd);
        unexpected_condition_str("SyntaxSort::ParameterDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_InitDeclarator:
      { an_ifc_SyntaxSort_InitDeclarator issid;
        get_SyntaxSort_InitDeclarator(&issid);
        unexpected_condition_str("SyntaxSort::InitDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_NewDeclarator:
      { an_ifc_SyntaxSort_NewDeclarator issnd;
        get_SyntaxSort_NewDeclarator(&issnd);
        unexpected_condition_str("SyntaxSort::NewDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_SimpleDeclaration:
      { an_ifc_SyntaxSort_SimpleDeclaration isssd;
        get_SyntaxSort_SimpleDeclaration(&isssd);
        unexpected_condition_str("SyntaxSort::SimpleDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ExceptionDeclaration:
      { an_ifc_SyntaxSort_ExceptionDeclaration issed;
        get_SyntaxSort_ExceptionDeclaration(&issed);
        unexpected_condition_str("SyntaxSort::ExceptionDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ConditionDeclaration:
      { an_ifc_SyntaxSort_ConditionDeclaration isscd;
        get_SyntaxSort_ConditionDeclaration(&isscd);
        unexpected_condition_str("SyntaxSort::ConditionDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_StaticAssertDeclaration:
      { an_ifc_SyntaxSort_StaticAssertDeclaration isssad;
        get_SyntaxSort_StaticAssertDeclaration(&isssad);
        unexpected_condition_str("SyntaxSort::StaticAssertDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_AliasDeclaration:
      { an_ifc_SyntaxSort_AliasDeclaration issad;
        get_SyntaxSort_AliasDeclaration(&issad);
        unexpected_condition_str("SyntaxSort::AliasDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ConceptDefinition:
      { an_ifc_SyntaxSort_ConceptDefinition isscd;
        get_SyntaxSort_ConceptDefinition(&isscd);
        unexpected_condition_str("SyntaxSort::ConceptDefinition"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_CompoundStatement:
      { an_ifc_SyntaxSort_CompoundStatement isscs;
        get_SyntaxSort_CompoundStatement(&isscs);
        unexpected_condition_str("SyntaxSort::CompoundStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ReturnStatement:
      { an_ifc_SyntaxSort_ReturnStatement issrs;
        get_SyntaxSort_ReturnStatement(&issrs);
        unexpected_condition_str("SyntaxSort::ReturnStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_IfStatement:
      { an_ifc_SyntaxSort_IfStatement issis;
        get_SyntaxSort_IfStatement(&issis);
        unexpected_condition_str("SyntaxSort::IfStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_WhileStatement:
      { an_ifc_SyntaxSort_WhileStatement issws;
        get_SyntaxSort_WhileStatement(&issws);
        unexpected_condition_str("SyntaxSort::WhileStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_DoWhileStatement:
      { an_ifc_SyntaxSort_DoWhileStatement issdws;
        get_SyntaxSort_DoWhileStatement(&issdws);
        unexpected_condition_str("SyntaxSort::DoWhileStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ForStatement:
      { an_ifc_SyntaxSort_ForStatement issfs;
        get_SyntaxSort_ForStatement(&issfs);
        unexpected_condition_str("SyntaxSort::ForStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_InitStatement:
      { an_ifc_SyntaxSort_InitStatement issis;
        get_SyntaxSort_InitStatement(&issis);
        unexpected_condition_str("SyntaxSort::InitStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_RangeBasedForStatement:
      { an_ifc_SyntaxSort_RangeBasedForStatement issrbfs;
        get_SyntaxSort_RangeBasedForStatement(&issrbfs);
        unexpected_condition_str("SyntaxSort::RangeBasedForStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ForRangeDeclaration:
      { an_ifc_SyntaxSort_ForRangeDeclaration issfrd;
        get_SyntaxSort_ForRangeDeclaration(&issfrd);
        unexpected_condition_str("SyntaxSort::ForRangeDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_LabeledStatement:
      { an_ifc_SyntaxSort_LabeledStatement issls;
        get_SyntaxSort_LabeledStatement(&issls);
        unexpected_condition_str("SyntaxSort::LabeledStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_BreakStatement:
      { an_ifc_SyntaxSort_BreakStatement issbs;
        get_SyntaxSort_BreakStatement(&issbs);
        unexpected_condition_str("SyntaxSort::BreakStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ContinueStatement:
      { an_ifc_SyntaxSort_ContinueStatement isscs;
        get_SyntaxSort_ContinueStatement(&isscs);
        unexpected_condition_str("SyntaxSort::ContinueStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_SwitchStatement:
      { an_ifc_SyntaxSort_SwitchStatement issss;
        get_SyntaxSort_SwitchStatement(&issss);
        unexpected_condition_str("SyntaxSort::SwitchStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_GotoStatement:
      { an_ifc_SyntaxSort_GotoStatement issgs;
        get_SyntaxSort_GotoStatement(&issgs);
        unexpected_condition_str("SyntaxSort::GotoStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_DeclarationStatement:
      { an_ifc_SyntaxSort_DeclarationStatement issds;
        get_SyntaxSort_DeclarationStatement(&issds);
        unexpected_condition_str("SyntaxSort::DeclarationStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ExpressionStatement:
      { an_ifc_SyntaxSort_ExpressionStatement isses;
        get_SyntaxSort_ExpressionStatement(&isses);
        unexpected_condition_str("SyntaxSort::ExpressionStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TryBlock:
      { an_ifc_SyntaxSort_TryBlock isstb;
        get_SyntaxSort_TryBlock(&isstb);
        unexpected_condition_str("SyntaxSort::TryBlock"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_Handler:
      { an_ifc_SyntaxSort_Handler issh;
        get_SyntaxSort_Handler(&issh);
        unexpected_condition_str("SyntaxSort::Handler"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_HandlerSeq:
      { an_ifc_SyntaxSort_HandlerSeq isshs;
        get_SyntaxSort_HandlerSeq(&isshs);
        unexpected_condition_str("SyntaxSort::HandlerSeq"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_FunctionTryBlock:
      { an_ifc_SyntaxSort_FunctionTryBlock issftb;
        get_SyntaxSort_FunctionTryBlock(&issftb);
        unexpected_condition_str("SyntaxSort::FunctionTryBlock"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TypeIdListElement:
      { an_ifc_SyntaxSort_TypeIdListElement isstidle;
        get_SyntaxSort_TypeIdListElement(&isstidle);
        unexpected_condition_str("SyntaxSort::TypeIdListElement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_DynamicExceptionSpec:
      { an_ifc_SyntaxSort_DynamicExceptionSpec issdes;
        get_SyntaxSort_DynamicExceptionSpec(&issdes);
        unexpected_condition_str("SyntaxSort::DynamicExceptionSpec"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_StatementSeq:
      { an_ifc_SyntaxSort_StatementSeq issss;
        get_SyntaxSort_StatementSeq(&issss);
        unexpected_condition_str("SyntaxSort::StatementSeq"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_FunctionBody:
      { an_ifc_SyntaxSort_FunctionBody issfb;
        get_SyntaxSort_FunctionBody(&issfb);
        unexpected_condition_str("SyntaxSort::FunctionBody"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_Expression:
      { an_ifc_SyntaxSort_Expression isse;
        get_SyntaxSort_Expression(&isse);
        unexpected_condition_str("SyntaxSort::Expression"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_FunctionDefinition:
      { an_ifc_SyntaxSort_FunctionDefinition issfd;
        get_SyntaxSort_FunctionDefinition(&issfd);
        unexpected_condition_str("SyntaxSort::FunctionDefinition"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_MemberFunctionDeclaration:
      { an_ifc_SyntaxSort_MemberFunctionDeclaration issmfd;
        get_SyntaxSort_MemberFunctionDeclaration(&issmfd);
        unexpected_condition_str("SyntaxSort::MemberFunctionDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TemplateDeclaration:
      { an_ifc_SyntaxSort_TemplateDeclaration isstd;
        get_SyntaxSort_TemplateDeclaration(&isstd);
        unexpected_condition_str("SyntaxSort::TemplateDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_RequiresClause:
      { an_ifc_SyntaxSort_RequiresClause issrc;
        get_SyntaxSort_RequiresClause(&issrc);
        unexpected_condition_str("SyntaxSort::RequiresClause"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_SimpleRequirement:
      { an_ifc_SyntaxSort_SimpleRequirement isssr;
        get_SyntaxSort_SimpleRequirement(&isssr);
        unexpected_condition_str("SyntaxSort::SimpleRequirement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TypeRequirement:
      { an_ifc_SyntaxSort_TypeRequirement isstr;
        get_SyntaxSort_TypeRequirement(&isstr);
        unexpected_condition_str("SyntaxSort::TypeRequirement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_CompoundRequirement:
      { an_ifc_SyntaxSort_CompoundRequirement isscr;
        get_SyntaxSort_CompoundRequirement(&isscr);
        unexpected_condition_str("SyntaxSort::CompoundRequirement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_NestedRequirement:
      { an_ifc_SyntaxSort_NestedRequirement issnr;
        get_SyntaxSort_NestedRequirement(&issnr);
        unexpected_condition_str("SyntaxSort::NestedRequirement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_RequirementBody:
      { an_ifc_SyntaxSort_RequirementBody issrb;
        get_SyntaxSort_RequirementBody(&issrb);
        unexpected_condition_str("SyntaxSort::RequirementBody"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TypeTemplateParameter:
      { an_ifc_SyntaxSort_TypeTemplateParameter issttp;
        get_SyntaxSort_TypeTemplateParameter(&issttp);
        unexpected_condition_str("SyntaxSort::TypeTemplateParameter"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TemplateTemplateParameter:
      { an_ifc_SyntaxSort_TemplateTemplateParameter issttp;
        get_SyntaxSort_TemplateTemplateParameter(&issttp);
        unexpected_condition_str("SyntaxSort::TemplateTemplateParameter"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TypeTemplateArgument:
      { an_ifc_SyntaxSort_TypeTemplateArgument isstta;
        get_SyntaxSort_TypeTemplateArgument(&isstta);
        unexpected_condition_str("SyntaxSort::TypeTemplateArgument"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_NonTypeTemplateArgument:
      { an_ifc_SyntaxSort_NonTypeTemplateArgument issntta;
        get_SyntaxSort_NonTypeTemplateArgument(&issntta);
        unexpected_condition_str("SyntaxSort::NonTypeTemplateArgument"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TemplateParameterList:
      { an_ifc_SyntaxSort_TemplateParameterList isstpl;
        get_SyntaxSort_TemplateParameterList(&isstpl);
        unexpected_condition_str("SyntaxSort::TemplateParameterList"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TemplateArgumentList:
      { an_ifc_SyntaxSort_TemplateArgumentList isstal;
        get_SyntaxSort_TemplateArgumentList(&isstal);
        unexpected_condition_str("SyntaxSort::TemplateArgumentList"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TemplateId:
      { an_ifc_SyntaxSort_TemplateId isstid;
        get_SyntaxSort_TemplateId(&isstid);
        unexpected_condition_str("SyntaxSort::TemplateId"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_MemInitializer:
      { an_ifc_SyntaxSort_MemInitializer issmi;
        get_SyntaxSort_MemInitializer(&issmi);
        unexpected_condition_str("SyntaxSort::MemInitializer"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_CtorInitializer:
      { an_ifc_SyntaxSort_CtorInitializer issci;
        get_SyntaxSort_CtorInitializer(&issci);
        unexpected_condition_str("SyntaxSort::CtorInitializer"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_LambdaIntroducer:
      { an_ifc_SyntaxSort_LambdaIntroducer issli;
        get_SyntaxSort_LambdaIntroducer(&issli);
        unexpected_condition_str("SyntaxSort::LambdaIntroducer"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_LambdaDeclarator:
      { an_ifc_SyntaxSort_LambdaDeclarator issld;
        get_SyntaxSort_LambdaDeclarator(&issld);
        unexpected_condition_str("SyntaxSort::LambdaDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_CaptureDefault:
      { an_ifc_SyntaxSort_CaptureDefault isscd;
        get_SyntaxSort_CaptureDefault(&isscd);
        unexpected_condition_str("SyntaxSort::CaptureDefault"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_SimpleCapture:
      { an_ifc_SyntaxSort_SimpleCapture isssc;
        get_SyntaxSort_SimpleCapture(&isssc);
        unexpected_condition_str("SyntaxSort::SimpleCapture"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_InitCapture:
      { an_ifc_SyntaxSort_InitCapture issic;
        get_SyntaxSort_InitCapture(&issic);
        unexpected_condition_str("SyntaxSort::InitCapture"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ThisCapture:
      { an_ifc_SyntaxSort_ThisCapture isstc;
        get_SyntaxSort_ThisCapture(&isstc);
        unexpected_condition_str("SyntaxSort::ThisCapture"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_AttributedStatement:
      { an_ifc_SyntaxSort_AttributedStatement issas;
        get_SyntaxSort_AttributedStatement(&issas);
        unexpected_condition_str("SyntaxSort::AttributedStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_AttributedDeclaration:
      { an_ifc_SyntaxSort_AttributedDeclaration issad;
        get_SyntaxSort_AttributedDeclaration(&issad);
        unexpected_condition_str("SyntaxSort::AttributedDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_AttributeSpecifierSeq:
      { an_ifc_SyntaxSort_AttributeSpecifierSeq issass;
        get_SyntaxSort_AttributeSpecifierSeq(&issass);
        unexpected_condition_str("SyntaxSort::AttributeSpecifierSeq"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_AttributeSpecifier:
      { an_ifc_SyntaxSort_AttributeSpecifier issas;
        get_SyntaxSort_AttributeSpecifier(&issas);
        unexpected_condition_str("SyntaxSort::AttributeSpecifier"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_AttributeUsingPrefix:
      { an_ifc_SyntaxSort_AttributeUsingPrefix issaup;
        get_SyntaxSort_AttributeUsingPrefix(&issaup);
        unexpected_condition_str("SyntaxSort::AttributeUsingPrefix"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_Attribute:
      { an_ifc_SyntaxSort_Attribute issa;
        get_SyntaxSort_Attribute(&issa);
        unexpected_condition_str("SyntaxSort::Attribute"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_AttributeArgumentClause:
      { an_ifc_SyntaxSort_AttributeArgumentClause issaac;
        get_SyntaxSort_AttributeArgumentClause(&issaac);
        unexpected_condition_str("SyntaxSort::AttributeArgumentClause"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_Alignas:
      { an_ifc_SyntaxSort_Alignas issa;
        get_SyntaxSort_Alignas(&issa);
        unexpected_condition_str("SyntaxSort::Alignas"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_UsingDeclaration:
      { an_ifc_SyntaxSort_UsingDeclaration issud;
        get_SyntaxSort_UsingDeclaration(&issud);
        unexpected_condition_str("SyntaxSort::UsingDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_UsingDeclarator:
      { an_ifc_SyntaxSort_UsingDeclarator issud;
        get_SyntaxSort_UsingDeclarator(&issud);
        unexpected_condition_str("SyntaxSort::UsingDeclarator"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_UsingDirective:
      { an_ifc_SyntaxSort_UsingDirective issud;
        get_SyntaxSort_UsingDirective(&issud);
        unexpected_condition_str("SyntaxSort::UsingDirective"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_ArrayIndex:
      { an_ifc_SyntaxSort_ArrayIndex issai;
        get_SyntaxSort_ArrayIndex(&issai);
        unexpected_condition_str("SyntaxSort::ArrayIndex"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_SEHTry:
      { an_ifc_SyntaxSort_SEHTry issseht;
        get_SyntaxSort_SEHTry(&issseht);
        unexpected_condition_str("SyntaxSort::SEHTry"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_SEHExcept:
      { an_ifc_SyntaxSort_SEHExcept isssehe;
        get_SyntaxSort_SEHExcept(&isssehe);
        unexpected_condition_str("SyntaxSort::SEHExcept"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_SEHFinally:
      { an_ifc_SyntaxSort_SEHFinally isssehf;
        get_SyntaxSort_SEHFinally(&isssehf);
        unexpected_condition_str("SyntaxSort:: is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_SEHLeave:
      { an_ifc_SyntaxSort_SEHLeave isssehl;
        get_SyntaxSort_SEHLeave(&isssehl);
        unexpected_condition_str("SyntaxSort::SEHLeave"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_TypeTraitIntrinsic:
      { an_ifc_SyntaxSort_TypeTraitIntrinsic isstti;
        get_SyntaxSort_TypeTraitIntrinsic(&isstti);
        unexpected_condition_str("SyntaxSort::TypeTraitIntrinsic"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_Tuple:
      { an_ifc_SyntaxSort_Tuple isst;
        get_SyntaxSort_Tuple(&isst);
        unexpected_condition_str("SyntaxSort::Tuple"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_AsmStatement:
      { an_ifc_SyntaxSort_AsmStatement issas;
        get_SyntaxSort_AsmStatement(&issas);
        unexpected_condition_str("SyntaxSort::AsmStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_NamespaceAliasDefinition:
      { an_ifc_SyntaxSort_NamespaceAliasDefinition issnad;
        get_SyntaxSort_NamespaceAliasDefinition(&issnad);
        unexpected_condition_str("SyntaxSort::NamespaceAliasDefinition"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_Super:
      { an_ifc_SyntaxSort_Super isss;
        get_SyntaxSort_Super(&isss);
        unexpected_condition_str("SyntaxSort::Super"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_UnaryFoldExpression:
      { an_ifc_SyntaxSort_UnaryFoldExpression issufe;
        get_SyntaxSort_UnaryFoldExpression(&issufe);
        unexpected_condition_str("SyntaxSort::UnaryFoldExpression"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_BinaryFoldExpression:
      { an_ifc_SyntaxSort_BinaryFoldExpression issbfe;
        get_SyntaxSort_BinaryFoldExpression(&issbfe);
        unexpected_condition_str("SyntaxSort::BinaryFoldExpression"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_EmptyStatement:
      { an_ifc_SyntaxSort_EmptyStatement isses;
        get_SyntaxSort_EmptyStatement(&isses);
        unexpected_condition_str("SyntaxSort::EmptyStatement"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_StructuredBindingDeclaration:
      { an_ifc_SyntaxSort_StructuredBindingDeclaration isssbd;
        get_SyntaxSort_StructuredBindingDeclaration(&isssbd);
        unexpected_condition_str("SyntaxSort::StructuredBindingDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_StructuredBindingIdentifier:
      { an_ifc_SyntaxSort_StructuredBindingIdentifier isssbi;
        get_SyntaxSort_StructuredBindingIdentifier(&isssbi);
        unexpected_condition_str("SyntaxSort::StructuredBindingIdentifier"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_UsingEnumDeclaration:
      { an_ifc_SyntaxSort_UsingEnumDeclaration issued;
        get_SyntaxSort_UsingEnumDeclaration(&issued);
        unexpected_condition_str("SyntaxSort::UsingEnumDeclaration"
                                 " is currently unspecified.");
      }
      break;
    case ifc_SyntaxSort_Last:
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected syntax sort");
  }  /* switch */
}  /* str_ifc_syntax_node */


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
void an_ifc_module::str_ifc_sentence(
                                ifc_SentenceIndex               sentence_index,
                                ARG_UNUSED a_str_control_block *scbp) const
/*
Generate a string for the specified sentence.
*/
{
  an_ifc_Sentence is;

  read_partition_at_index(ifc_sentence, sentence_index);
  get_Sentence(&is);
  unexpected_condition_str("IFC Sentences currently unspecified.");
}  /* str_ifc_sentence*/


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
void an_ifc_module::str_ifc_word(ifc_WordIndex                  word_index,
                                 ARG_UNUSED a_str_control_block *scbp) const
/*
Generate a string for the specified word.
*/
{
  an_ifc_Word iw;

  read_partition_at_index(ifc_word, word_index);
  get_Word(&iw);
  unexpected_condition_str("IFC Words currently unspecified.");
}  /* str_ifc_word */


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
}  /* db_ifc_File_header */

#endif /* DEBUG */

void ifc_modules_one_time_init(void)
/*
Do one-time initialization of static variables defined in this file.
*/
{
  top_ifc_text_buffer = NULL;
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
#if DEBUG && EXPENSIVE_CHECKING
  debug_partition = NULL;
#endif /* DEBUG && EXPENSIVE_CHECKING */
}  /* ifc_modules_init */

/*lint -restore*/ /* FIXME: temporary */
/*lint -restore*/

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
