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


static unsigned char buffer_overrun(void)
/*
This routine is called if a memory buffer (which represents a portion of
a module file) terminates prematurely.  Issue a catastrophic error.
This routine returns an unsigned char so it can be used in a ?: operation
that returns an unsigned char.
*/
{
  unexpected_condition();
  /*lint -e527*/
  return 0;
}  /* buffer_overrun */

#if USE_MMAP_FOR_MEMORY_REGIONS

inline void an_ifc_module::init_byte_buffer(size_t offset,
                                            size_t length) const noexcept
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
                                                 size_t length) const noexcept
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
                                                                 const noexcept
/*
Initialize the state information used by "get_bytes", etc.  offset is the
offset from the start of the module file to be read.  length is its size, in
bytes.
*/
{
  fseek(f_module, offset, SEEK_SET);
}  /* init_byte_buffer */


inline void an_ifc_module::get_bytes_from_buffer(void   *entity,
                                                 size_t length) const noexcept
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
                                                                 const noexcept
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
                                     a_boolean header_bytes) const noexcept
/*
Get length bytes from the IFC file.  If there's an endian mismatch between
what's being read and the host, convert the bytes to the host byte order.  If
header_bytes is TRUE, the bytes being retrieved correspond to the IFC file
header or table of contents (and are therefore known to be little-endian).
*/
{
  a_boolean reading_little_endian = ASSUME_LITTLE_ENDIAN_IFC_MODULES ||
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
                                  size_t       length) const noexcept
/*
Utility to print some debug information for every access to an IFC module file.
*/
{
  if (db_flag_is_set("ifc_modules")) {
    if (debug_partition != NULL) {
      (void)fprintf(f_debug, "[%s:0x%08lx:%d] = ",
                    debug_partition->name,
#if USE_MMAP_FOR_MEMORY_REGIONS
                    ((char *)byte_buffer -
                       ((char *)mmap_addr + debug_partition->offset) - length),
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
                    (long)(ftell(f_module)) - debug_partition->offset - length,
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

#define GET_ByteOffset(x, from_header)         GET_int(x, from_header)
#define GET_Cardinality(x, from_header)        GET_int(x, from_header)
#define GET_ChartIndex(x, from_header)         GET_int(x, from_header)
#define GET_Column(x, from_header)             GET_int(x, from_header)
#define GET_DeclIndex(x, from_header)          GET_int(x, from_header)
#define GET_EntitySize(x, from_header)         GET_int(x, from_header)
#define GET_ExprIndex(x, from_header)          GET_int(x, from_header)
#define GET_Index(x, from_header)              GET_int(x, from_header)
#define GET_LanguageVersion(x, from_header)    GET_int(x, from_header)
#define GET_LineIndex(x, from_header)          GET_int(x, from_header)
#define GET_LineNumber(x, from_header)         GET_int(x, from_header)
#define GET_LitIndex(x, from_header)           GET_int(x, from_header)
#define GET_MsvcTraits(x, from_header)         GET_int(x, from_header)
#define GET_NameIndex(x, from_header)          GET_int(x, from_header)
#define GET_Offset(x, from_header)             GET_int(x, from_header)
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

#define GET_Alignment(x, from_header)          GET_short(x, from_header)
#define GET_EHFlags(x, from_header)            GET_short(x, from_header)
#define GET_FunctionTraits(x, from_header)     GET_short(x, from_header)
#define GET_OperatorCategory(x, from_header)   GET_short(x, from_header)
#define GET_PackSize(x, from_header)           GET_short(x, from_header)

#define GET_Abi(x, from_header)                GET_byte(x, from_header)
#define GET_Access(x, from_header)             GET_byte(x, from_header)
#define GET_Architecture(x, from_header)       GET_byte(x, from_header)
#define GET_BasicSpecifiers(x, from_header)    GET_byte(x, from_header)
#define GET_CallingConvention(x, from_header)  GET_byte(x, from_header)
#define GET_ExpansionMode(x, from_header)      GET_byte(x, from_header)
#define GET_FunctionTypeTraits(x, from_header) GET_byte(x, from_header)
#define GET_NoexceptSort(x, from_header)       GET_byte(x, from_header)
#define GET_ObjectTraits(x, from_header)       GET_byte(x, from_header)
#define GET_ParameterSort(x, from_header)      GET_byte(x, from_header)
#define GET_Qualifiers(x, from_header)         GET_byte(x, from_header)
#define GET_ReachableProperties(x, from_header)GET_byte(x, from_header)
#define GET_ReadConversionSort(x, from_header) GET_byte(x, from_header)
#define GET_ScopeTraits(x, from_header)        GET_byte(x, from_header)
/*lint -esym(750,GET_SyntaxSort)*/
#define GET_SyntaxSort(x, from_header)         GET_byte(x, from_header)
#define GET_TypeBasis(x, from_header)          GET_byte(x, from_header)
#define GET_TypePrecision(x, from_header)      GET_byte(x, from_header)
#define GET_TypeSign(x, from_header)           GET_byte(x, from_header)
#define GET_Version(x, from_header)            GET_byte(x, from_header)
#define GET_WordSort(x, from_header)           GET_byte(x, from_header)

#define GET_bool(x, from_header)               GET_byte(x, from_header)
#define GET_uint8_t(x, from_header)            GET_byte(x, from_header)
#define GET_uint16_t(x, from_header)           GET_short(x, from_header)

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
                             a_boolean  fill_storage) const noexcept
  {
    if (targ_little_endian == host_little_endian) {
      if (fill_storage) {
        memcpy(ptr, byte_buffer, sizeof(an_ifc_foo);
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
                                  a_boolean             fill_storage) \
                                                               const noexcept \
  { \
    if ((a_boolean)(ASSUME_LITTLE_ENDIAN_IFC_MODULES || targ_little_endian) \
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
                                  a_boolean             fill_storage) \
                                                               const noexcept \
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
                             a_boolean  fill_storage) const noexcept
  {
    GET_field1_type(ptr->field1);
    GET_field2_type(ptr->field2);
    return ptr;
  }
*/
#define IFC_DECL_START(name) \
  inline concat(an_ifc_, name) * an_ifc_module::concat(get_, name) ( \
                                  concat(an_ifc_, name) *ptr, \
                                  ARG_UNUSED a_boolean  fill_storage) \
                                                               const noexcept \
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

/*
A macro to return a pointer to the IFC string table for a given IFC module
file and TextOffset.  Strings in the IFC file are NULL-terminated.
*/
#if EXPENSIVE_CHECKING
#define verify_offset(offset) \
  (check_assertion((ifc_Cardinality)(offset) < header.string_table_size)),
#else /* !EXPENSIVE_CHECKING */
#define verify_offset(offset) /**/
#endif /* EXPENSIVE_CHECKING */
#define get_string_at_offset(offset) \
  (verify_offset(offset) string_table + (offset))

#if DEBUG

static a_const_char *db_decl_tag(ifc_DeclSort tag)
/*
Return a string with the name that corresponds to the DeclSort tag.
*/
{
  a_const_char *result;

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
    case ifc_DeclSort_Intrinsic:        result = "Intrinsic"; break;
    case ifc_DeclSort_Function:         result = "Function"; break;
    case ifc_DeclSort_Method:           result = "Method"; break;
    case ifc_DeclSort_Constructor:      result = "Constructor"; break;
    case ifc_DeclSort_InheritedConstructor:
                                        result = "InheritedConstructor"; break;
    case ifc_DeclSort_Destructor:       result = "Destructor"; break;
    case ifc_DeclSort_Reference:        result = "Reference"; break;
    case ifc_DeclSort_Property:         result = "Property"; break;
    case ifc_DeclSort_OutputSegment:    result = "OutputSegment"; break;
    case ifc_DeclSort_UsingDeclaration: result = "UsingDeclaration"; break;
    case ifc_DeclSort_UsingDirective:   result = "UsingDirective"; break;
    case ifc_DeclSort_Friend:           result = "Friend"; break;
    case ifc_DeclSort_SyntaxTree:       result = "SyntaxTree"; break;
    case ifc_DeclSort_Tuple:            result = "Tuple"; break;
    default:
      result = "Unexpected";
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
  /*lint -e{641}*/
  switch (category) {
    case ifc_OperatorCategory_Bitand:       op = onk_ampersand; break;
    case ifc_OperatorCategory_LogicAnd:     op = onk_and_and; break;
    case ifc_OperatorCategory_Assign:       op = onk_assign; break;
    case ifc_OperatorCategory_Comma:        op = onk_comma; break;
    case ifc_OperatorCategory_Not:          op = onk_not; break;
    case ifc_OperatorCategory_Minus:        op = onk_minus; break;
    case ifc_OperatorCategory_Star:         op = onk_star; break;
    case ifc_OperatorCategory_Bitor:        op = onk_or; break;
    case ifc_OperatorCategory_LogicOr:      op = onk_or_or; break;
    case ifc_OperatorCategory_Plus:         op = onk_plus; break;
    case ifc_OperatorCategory_Quest:        op = onk_question; break;
    case ifc_OperatorCategory_Complement:   op = onk_compl; break;
    case ifc_OperatorCategory_Caret:        op = onk_excl_or; break;
    case ifc_OperatorCategory_Slash:        op = onk_divide; break;
    case ifc_OperatorCategory_Modulo:       op = onk_remainder; break;
    case ifc_OperatorCategory_New:          op = onk_new; break;
    case ifc_OperatorCategory_Delete:       op = onk_delete; break;
    case ifc_OperatorCategory_IndirectMemberAccess:
                                            op = onk_arrow_star; break;
    case ifc_OperatorCategory_PostIncrement:op = onk_plus_plus; break;
    case ifc_OperatorCategory_PostDecrement:op = onk_minus_minus; break;
    case ifc_OperatorCategory_SlashEq:      op = onk_divide_assign; break;
    case ifc_OperatorCategory_EqEq:         op = onk_eq; break;
    case ifc_OperatorCategory_NotEq:        op = onk_ne; break;
    case ifc_OperatorCategory_Greater:      op = onk_gt; break;
    case ifc_OperatorCategory_GreaterEq:    op = onk_ge; break;
    case ifc_OperatorCategory_Less:         op = onk_lt; break;
    case ifc_OperatorCategory_LessEq:       op = onk_le; break;
    case ifc_OperatorCategory_Spaceship:    op = onk_spaceship; break;
    case ifc_OperatorCategory_LshiftEq:     op = onk_shift_left_assign; break;
    case ifc_OperatorCategory_RshiftEq:     op = onk_shift_right_assign; break;
    case ifc_OperatorCategory_MinusEq:      op = onk_minus_assign; break;
    case ifc_OperatorCategory_ModuloEq:     op = onk_remainder_assign; break;
    case ifc_OperatorCategory_StarEq:       op = onk_times_assign; break;
    case ifc_OperatorCategory_BitorEq:      op = onk_or_assign; break;
    case ifc_OperatorCategory_PlusEq:       op = onk_plus_assign; break;
    case ifc_OperatorCategory_BitandEq:     op = onk_and_assign; break;
    case ifc_OperatorCategory_BitxorEq:     op = onk_excl_or_assign; break;
    case ifc_OperatorCategory_Lshift:       op = onk_shift_left; break;
    case ifc_OperatorCategory_Rshift:       op = onk_shift_right; break;
    case ifc_OperatorCategory_Arrow:        op = onk_arrow; break;
    case ifc_OperatorCategory_PreDecrement: op = onk_minus_minus; break;
    case ifc_OperatorCategory_PreIncrement: op = onk_plus_plus; break;
    case ifc_OperatorCategory_UnaryMinus:   op = onk_minus; break;
    case ifc_OperatorCategory_Address:      op = onk_ampersand; break;
    case ifc_OperatorCategory_UnaryPlus:    op = onk_plus; break;

    /* These don't have direct mappings:*/
    case ifc_OperatorCategory_Percent:           /*    operator% */
    case ifc_OperatorCategory_Sizeof:            /*    operator sizeof */
    case ifc_OperatorCategory_ExpandingSizeof:   /*    operator sizeof... */
    case ifc_OperatorCategory_Throw:             /*    operator throw */
    case ifc_OperatorCategory_Alignof:           /*    operator alignof */
    case ifc_OperatorCategory_Noexcept:          /*    operator noexcept */
    case ifc_OperatorCategory_Requires:          /*    operator requires */
    case ifc_OperatorCategory_Coreturn:          /*    operator co_return */
    case ifc_OperatorCategory_Await:             /*    operator co_yield */
    case ifc_OperatorCategory_Yield:             /*    operator co_yield */
    case ifc_OperatorCategory_StaticAssert:      /*    operator static_assert*/
    case ifc_OperatorCategory_Dot:               /*    operator. */
    case ifc_OperatorCategory_DerefMemberAccess: /*    operator.* */
    default:
#if DEBUG
      if (db_flag_is_set("ms_ignore")) {
        (void)fprintf(f_debug, "Unsupported operation: %d\n", category);
      }  /* if */
#endif /* DEBUG */
      op = onk_none;
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


a_boolean an_ifc_module::import(a_module_import_decl_ptr midp) noexcept
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
  if (open_and_map_ifc_module_file(midp)) {
    result = TRUE;
    assoc_module_info = mod;
    set_name(mod->name);
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
#if USE_MMAP_FOR_MEMORY_REGIONS
#if EDG_WIN32
    close_mapped_input_file(mapped_input, map_object);
    mapped_input = NULL;
    map_object = NULL;
#endif /* EDG_WIN32 */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  }  /* if */
}  /* close_ifc_module_file */


void an_ifc_module::pch_reset(a_module_import_decl_ptr midp) noexcept
/*
Called after a PCH file has been read to re-open and re-mmap the specified
module.  Note that the mmap-ed address does not need to be at the same
location as the original.
*/
{
  if (!open_and_map_ifc_module_file(midp)) {
    /* This shouldn't happen (the PCH processing checks the existence and
       modification time of module files). */
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
    read_partition_at_offset(mep->variant.ifc_partition, mep->file_offset);
    tag = (ifc_DeclSort)get_tag_from_partition(mep->variant.ifc_partition,
                                               ifc_decl_start);
    switch (tag) {
      case ifc_DeclSort_Variable:
        { an_ifc_DeclSort_Variable idsv, *idsvp;
          idsvp = get_DeclSort_Variable(&idsv);
          init_locator_from_name(idsvp->name, (ifc_TextOffset)0, &idsvp->locus,
                                 &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here. */
            a_variable_ptr vp;
            /* FIXME: idsvp->alignment exists but is an ExprIndex. */
            init_dps(&dps, &idsvp->locus, idsvp->type, (ifc_Alignment)0,
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
      case ifc_DeclSort_Function:
        { a_func_info_block        func_info;
          a_type_ptr               old_type;
          an_ifc_DeclSort_Function idsf, *idsfp;
          idsfp = get_DeclSort_Function(&idsf);
          init_locator_from_name(idsfp->name, (ifc_TextOffset)0, &idsfp->locus,
                                 &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here. */
            a_routine_ptr rp;
            init_dps(&dps, &idsfp->locus, idsfp->type, (ifc_Alignment)0,
                     ifc_ObjectTraits_None, ifc_MsvcTraits_None,
                     idsfp->specifiers, idsfp->access, &psss);
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
          init_locator_from_name((ifc_NameIndex)0, idsip->name, &idsip->locus,
                                 &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here (just copied
               ifc_DeclSort_Function).*/
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
          /* Should be no unnamed namespaces or types. */
          check_assertion(idssp->name != 0);
          init_locator_from_name(idssp->name, (ifc_TextOffset)0, &idssp->locus,
                                 &loc);
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
      case ifc_DeclSort_Alias:
        { an_ifc_DeclSort_Alias idsta, *idstap;
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          idstap = get_DeclSort_Alias(&idsta);
          init_locator_from_name((ifc_NameIndex)0, idstap->name,
                                 &idstap->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            check_assertion(type_tag(idstap->type)== ifc_TypeSort_Fundamental);
            /* Read the type to see what kind it is. */
            read_partition_at_index(ifc_type_fundamental,
                                    type_value(idstap->type));
            itsfp = get_TypeSort_Fundamental(&itsf);
            if (itsfp->basis == ifc_TypeBasis_Typename) {
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
            } else if (itsfp->basis == ifc_TypeBasis_Namespace) {
              /* A namespace alias. */
              /* FIXME: unimplemented. */
              unexpected_condition();
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
            a_type_ptr   enum_type;
            a_symbol_ptr tag_sym;
            check_assertion(idsep->base != 0);
            /* FIXME: idsep->alignment exists but is an ExprIndex. */
            init_dps(&dps, &idsep->locus, idsep->base, (ifc_Alignment)0,
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
          check_assertion(idsep->name != 0);
          init_locator_from_name((ifc_NameIndex)0, idsep->name, &idsep->locus,
                                 &loc);
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
      case ifc_DeclSort_Template:
        { an_ifc_DeclSort_Template idst, *idstp;
          idstp = get_DeclSort_Template(&idst);
          check_assertion(idstp->name != 0);
          init_locator_from_name(idstp->name, (ifc_TextOffset)0, &idstp->locus,
                                 &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_decl_parse_state          dps;
            a_tmpl_decl_state           decl_state;
            a_template_decl_info_ptr    tdip = NULL;
            a_partial_scope_stack_state psss;
            ifc_ChartSort               chart_sort = chart_tag(idstp->chart);
            ifc_DeclIndex               entity_decl =
                                            (ifc_DeclIndex)idstp->entity.index;
            a_module_entity_ptr         dmep;

            /* FIXME: Is this setting dps.start_pos correctly? */
            init_dps(&dps, &idstp->locus, (ifc_TypeIndex)0,
                     (ifc_Alignment)0, ifc_ObjectTraits_None,
                     ifc_MsvcTraits_None, idstp->specifiers, idstp->access,
                     &psss);
            /* This declaration holds the underlying type of the template. */
            dmep = get_ifc_module_entity_ptr(entity_decl);
            dmep->scope = mep->scope;
            process_ifc_declaration(dmep, /*defer=*/FALSE, (a_type_ptr)NULL);
            /* Prepare the template declaration. */
            init_templ_decl_state(&decl_state, &dps);
            decl_state.enclosing_scope = mep->scope;
            decl_state.il_template_entry = make_il_template_entry(&decl_state);
            /* Get the template parameters. */
            read_partition_at_index(chart_sort, chart_value(idstp->chart));
            switch (chart_sort) {
              case ifc_ChartSort_None:
                /* No arguments to the template (i.e., specialization). */
                decl_state.is_specialization = TRUE;
                break;
              case ifc_ChartSort_Unilevel:
                { an_ifc_ChartSort_Unilevel icsu, *icsup;
                  icsup = get_ChartSort_Unilevel(&icsu);
                  set_up_template_decl(&decl_state, &dps.start_pos, &tdip);
                  for (ifc_Index_type idx = 0; idx < icsup->cardinality;
                       ++idx) {
                    a_module_entity_ptr pmep = get_ifc_module_entity_ptr(
                                        make_decl_index(ifc_DeclSort_Parameter,
                                                        icsup->start + idx));
                    pmep->scope = scope_stack_top().il_scope;
                    process_ifc_declaration(pmep, /*defer=*/FALSE,
                                            (a_type_ptr)NULL);
                    check_assertion(pmep->entity.kind == iek_type);
                    /* FIXME: Construct the parameter list. */
                  }  /* for */
                  check_assertion(decl_state.decl_info->n_params ==
                                                           icsup->cardinality);
                }
                break;
              case ifc_ChartSort_Multilevel:
                unexpected_condition_str("ChartSort::Multilevel "
                                         "not handled here");
                break;
              default:
                unexpected_condition_str("Unexpected ChartSort");
            }  /* switch */
            /* FIXME: Finish processing the template declaration. */
            for (; decl_state.number_of_template_decl_scopes != 0;
                   decl_state.number_of_template_decl_scopes--) {
              pop_scope();
            }  /* for */
            restore_partial_scope_stack_if_necessary(&psss);
            unexpected_condition_str("Non-deferred DeclSort::Template "
                                     "is not yet handled.");
          }  /* if */
        }
        break;
      case ifc_DeclSort_Parameter:
        { an_ifc_DeclSort_Parameter idsp, *idspp;
          idspp = get_DeclSort_Parameter(&idsp);
          check_assertion(idspp->name != 0);
          init_locator_from_name((ifc_NameIndex)0, idspp->name, &idspp->locus,
                                 &loc);
          unexpected_condition_str("DeclSort::Parameter "
                                   "not yet properly handled");
        }
        break;
      case ifc_DeclSort_Method:
      case ifc_DeclSort_Constructor:
      case ifc_DeclSort_Destructor:
      case ifc_DeclSort_Field:
      case ifc_DeclSort_Bitfield:
      case ifc_DeclSort_Property:
        /* These entities can only exist in a class and class definitions are
           currently handled by scanning a textual representation of the
           class. */
        unexpected_condition();
      case ifc_DeclSort_VendorExtension:
      case ifc_DeclSort_Temploid:
      case ifc_DeclSort_PartialSpecialization:
      case ifc_DeclSort_ExplicitSpecialization:
      case ifc_DeclSort_ExplicitInstantiation:
      case ifc_DeclSort_Concept:
      case ifc_DeclSort_InheritedConstructor:
      case ifc_DeclSort_Reference:
      case ifc_DeclSort_OutputSegment:
      case ifc_DeclSort_UsingDeclaration:
      case ifc_DeclSort_UsingDirective:
      case ifc_DeclSort_Friend:
      case ifc_DeclSort_SyntaxTree:
      case ifc_DeclSort_Tuple:
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
  read_partition_at_offset(mep->variant.ifc_partition, mep->file_offset);
  idssp = get_DeclSort_Scope(&idss);
  str_ifc_class_definition(idssp, &scb);
  add_char_to_text_buffer(buffer, ';');
}  /* get_definition_of_module_class */

#if DEBUG

void an_ifc_module::debug() const noexcept
/*
Print debug information for an IFC module
*/
{
  (void)fprintf(f_debug, "kind: mk_ifc\n");
}  /* debug */


void an_ifc_module::db_module_entity(a_module_entity_ptr mep) const noexcept
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


a_module_entity_ptr an_ifc_module::get_ifc_module_entity_ptr(
                                        an_ifc_partition_kind partition,
                                        ifc_Index_type        index)
                                                                 const noexcept
/*
Utility to return a module entity pointer given a module, IFC partition, and
index into that partition.  For cases where the module entity has just been
created, the partition is set according to the partition supplied by the
caller.
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
                                                                 const noexcept
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
                                                                 const noexcept
/*
Overload wrapper for "get_ifc_module_entity_ptr" that extracts the decl sort
and index from the provided index.
*/
{
  an_ifc_partition_kind partition = (an_ifc_partition_kind)(ifc_decl_start +
                                                              decl_tag(index));

  return get_ifc_module_entity_ptr(partition, decl_value(index));
}  /* get_ifc_module_entity_ptr */


a_type_ptr an_ifc_module::type_for_type_index(ifc_TypeIndex type_index)
                                                                 const noexcept
/*
Returns the type that corresponds to the specified TypeIndex in the module
file indicated.  Note that NULL is a valid return (and represents an "ellipsis
type").
*/
{
  a_type_ptr          result = NULL;
  a_module_entity_ptr mep = get_ifc_module_entity_ptr(type_index);
  ifc_TypeSort        tag;

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
                default:
                  unexpected_condition();
              }  /* if */
              break;
            case ifc_TypeBasis_Wchar_t:
              switch (itsfp->precision) {
                case ifc_TypePrecision_Default:
                  result = wchar_t_type();
                  break;
                case ifc_TypePrecision_Bit16:
                  result = char16_t_type();
                  break;
                case ifc_TypePrecision_Bit32:
                  result = char32_t_type();
                  break;
                /* FIXME: not sure how to map these: */
                case ifc_TypePrecision_Short:
                case ifc_TypePrecision_Long:
                case ifc_TypePrecision_Bit64:
                case ifc_TypePrecision_Bit128:
                default:
                  unexpected_condition();
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
                case ifc_TypePrecision_Bit64:
                  ik = (itsfp->sign == ifc_TypeSign_Unsigned) ?
                                                        ik_unsigned_long_long :
                                                        ik_long_long;
                  break;
                /* FIXME: not sure how to map these: */
                case ifc_TypePrecision_Bit16:
                case ifc_TypePrecision_Bit32:
                case ifc_TypePrecision_Bit128:
                default:
                  ik = ik_none;
                  unexpected_condition();
              }  /* switch */
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
              /* The IL doesn't have a way to represent an "ellipsis type", so
                 return a NULL type and let the caller check explicitly for
                 it. */
              result = NULL;
              break;
            case ifc_TypeBasis_Class:
            case ifc_TypeBasis_Struct:
            case ifc_TypeBasis_Union:
            case ifc_TypeBasis_Auto:
            case ifc_TypeBasis_DecltypeAuto:
            case ifc_TypeBasis_Namespace:
            case ifc_TypeBasis_Interface:
            case ifc_TypeBasis_Enum:
            case ifc_TypeBasis_Typename:
            case ifc_TypeBasis_SegmentType:
            case ifc_TypeBasis_Function:
            case ifc_TypeBasis_Empty:
            case ifc_TypeBasis_VariableTemplate:
            default:
              /* FIXME: not implemented yet. */
              unexpected_condition();
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
          result = make_qualified_type(type_for_type_index(itsqp->unqualified),
                                       qualifiers);
        }
        break;
      case ifc_TypeSort_Pointer:
        { an_ifc_TypeSort_Pointer itsp, *itspp;
          itspp = get_TypeSort_Pointer(&itsp);
          result = make_pointer_type(type_for_type_index(itspp->pointee));
        }
        break;
      case ifc_TypeSort_LvalueReference:
        { an_ifc_TypeSort_LvalueReference itslr, *itslrp;
          itslrp = get_TypeSort_LvalueReference(&itslr);
          result = make_reference_type(type_for_type_index(itslrp->referee));
        }
        break;
      case ifc_TypeSort_RvalueReference:
        { an_ifc_TypeSort_RvalueReference itsrr, *itsrrp;
          itsrrp = get_TypeSort_RvalueReference(&itsrr);
          result = make_rvalue_reference_type(
                                         type_for_type_index(itsrrp->referee));
        }
        break;
      case ifc_TypeSort_Array:
        { an_ifc_TypeSort_Array itsa, *itsap;
          itsap = get_TypeSort_Array(&itsa);
          result = alloc_type((a_type_kind)tk_array);
          result->variant.array.element_type =
                                           type_for_type_index(itsap->element);
          /* FIXME: this is wrong: */
          result->variant.array.variant.number_of_elements = itsap->extent;
        }
        break;
      case ifc_TypeSort_Method: /* FIXME: for now (same structures)?): */
        unexpected_condition(); /* FIXME: No longer same structures. */
        break;
      case ifc_TypeSort_Function:
        { an_ifc_TypeSort_Function itsf, *itsfp;
          itsfp = get_TypeSort_Function(&itsf);
          /* Create a routine type with no parameters to start. */
          result = make_routine_type(type_for_type_index(itsfp->target),
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
                param_type = type_for_type_index(ti);
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
              param_type = type_for_type_index(itsfp->source);
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
      case ifc_TypeSort_Designated:
        /* A type's name (e.g., "A"). */
        { an_ifc_TypeSort_Designated itsd, *itsdp;
          a_type_ptr                 class_or_enum_type;
          a_module_entity_ptr        dmep;
          itsdp = get_TypeSort_Designated(&itsd);
          /* Prepare to read from the proper partition for the
             type scope declaration. */
          check_assertion(decl_tag(itsdp->decl) == ifc_DeclSort_Scope ||
                          decl_tag(itsdp->decl) == ifc_DeclSort_Enumeration);
          /* Find the type of the scope declaration by processing it (in
             case it has been deferred). */
          dmep = get_ifc_module_entity_ptr(itsdp->decl);
          // FIXME: scope is unknown means bad news.
          check_assertion(dmep->scope != NULL);
          process_ifc_declaration(dmep, /*defer=*/FALSE, (a_type_ptr)NULL);
          class_or_enum_type = (a_type_ptr)dmep->entity.ptr;
          check_assertion(class_or_enum_type != NULL &&
                          dmep->entity.kind == iek_type);
          result = class_or_enum_type;
        }
        break;
      case ifc_TypeSort_Deduced:
        { an_ifc_TypeSort_Deduced itsd;
          get_TypeSort_Deduced(&itsd);
          unexpected_condition_str("TypeSort::Deduced "
                                   "is not yet implemented.");
        }
        break;
      case ifc_TypeSort_PointerToMember:
        { an_ifc_TypeSort_PointerToMember itsptm;
          get_TypeSort_PointerToMember(&itsptm);
          unexpected_condition_str("TypeSort::PointerToMember "
                                   "is not yet implemented.");
        }
        break;
      case ifc_TypeSort_Tuple:
        { an_ifc_TypeSort_Tuple itst;
          get_TypeSort_Tuple(&itst);
          unexpected_condition_str("TypeSort::Tuple "
                                   "is not yet implemented.");
        }
        break;
      case ifc_TypeSort_Forall:
        { an_ifc_TypeSort_Forall itsfa;
          get_TypeSort_Forall(&itsfa);
          unexpected_condition_str("TypeSort::Forall "
                                   "is not yet implemented.");
        }
        break;
      case ifc_TypeSort_VendorExtension:
        { an_ifc_TypeSort_VendorExtension itsve, *itsvep;
          itsvep = get_TypeSort_VendorExtension(&itsve);
          (void)itsvep;
          unexpected_condition_str("TypeSort::VendorExtension "
                                   "is not yet implemented.");
        }
        break;
      case ifc_TypeSort_Syntactic:
        { an_ifc_TypeSort_Syntactic itss, *itssp;
          itssp = get_TypeSort_Syntactic(&itss);
          ifc_ExprSort tag = expr_tag(itssp->expr);
          ifc_Index    value = expr_value(itssp->expr);
          read_partition_at_index(tag, value);
          switch (tag) {
            case ifc_ExprSort_TemplateId:
              { an_ifc_ExprSort_TemplateId iestid, *iestidp;
                iestidp = get_ExprSort_TemplateId(&iestid);
                result = type_for_template_id(iestidp);
              }
              unexpected_condition_str("ExprSort::TemplateId not yet handled "
                                       "for TypeSort::Syntactic");
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
          unexpected_condition_str("TypeSort::Expansion "
                                   "is not yet implemented.");
        }
        break;
      case ifc_TypeSort_Typename:
        { an_ifc_TypeSort_Typename itstn;
          get_TypeSort_Typename(&itstn);
          unexpected_condition_str("TypeSort::Typename "
                                   "is not yet implemented.");
        }
        break;
      case ifc_TypeSort_Base:
        { an_ifc_TypeSort_Base itsb;
          get_TypeSort_Base(&itsb);
          unexpected_condition_str("TypeSort::Base "
                                   "is not yet implemented.");
        }
        break;
      case ifc_TypeSort_Unaligned:
        { an_ifc_TypeSort_Unaligned itsu;
          get_TypeSort_Unaligned(&itsu);
          unexpected_condition_str("TypeSort::Unaligned "
                                   "is not yet implemented.");
        }
        break;
      case ifc_TypeSort_Decltype:
        { an_ifc_TypeSort_Decltype itsd;
          get_TypeSort_Decltype(&itsd);
          unexpected_condition_str("TypeSort::Decltype "
                                   "is not yet implemented.");
        }
        break;
      case ifc_TypeSort_SyntaxTree:
        { an_ifc_TypeSort_SyntaxTree itsst;
          get_TypeSort_SyntaxTree(&itsst);
          unexpected_condition_str("TypeSort::SyntaxTree "
                                   "is not yet implemented.");
        }
        break;
      default:
#if DEBUG
        if (db_flag_is_set("ms_ignore")) {
          (void)fprintf(f_debug, "Unsupported type: ");
          db_module_entity(mep);
        }  /* if */
#endif /* DEBUG */
        unexpected_condition_str("Unexpected TypeSort");
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
}  /* type_for_type_index */


a_type_ptr an_ifc_module::type_for_template_id(
                                          an_ifc_ExprSort_TemplateId *templ_id)
                                                                 const noexcept
/*
Returns the type that corresponds to the provided ExprSort::TemplateId in the
module file.
*/
{
  a_type_ptr          result = NULL;
  //ifc_ExprSort        arg_tag = expr_tag(templ_id->arguments);
  ifc_ExprSort        pri_tag = expr_tag(templ_id->primary);
  an_ifc_ExprSort_NamedDecl iesnd, *iesndp;

  read_partition_at_index(pri_tag, expr_value(templ_id->primary));
  iesndp = get_ExprSort_NamedDecl(&iesnd);
  check_assertion(pri_tag == ifc_ExprSort_NamedDecl);
  if (iesndp->type != 0) {
    result = type_for_type_index(iesndp->type);
    unexpected_condition_str("Unexpected type for ExprSort::NamedDecl");
  } else {
    a_module_entity_ptr mep = get_ifc_module_entity_ptr(iesndp->resolution);
    process_ifc_declaration(mep, /*defer=*/FALSE, (a_type_ptr)NULL);
    unexpected_condition();
  }  /* if */
  return result;
}  /* type_for_template_id */


void an_ifc_module::source_position_from_locus(a_source_position  *pos,
                                               ifc_SourceLocation *locus)
                                                                 const noexcept
/*
Map the IFC locus source position information into the source position at pos.
*/
{
  an_ifc_Source_Line   isl, *islp;
#if CHECKING
  ifc_NameSort         tag;
#endif /* CHECKING */
  a_seq_number         *seq;

  read_partition_at_index(ifc_src_line, locus->line);
  islp = get_Source_Line(&isl);
#if CHECKING
  tag = name_tag(islp->file);
  check_assertion(tag == ifc_NameSort_SourceFile);
#endif /* CHECKING */
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
    result = get_string_at_offset(name_value(name_index));
  } else {
    read_partition_at_index(tag, name_value(name_index));
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
            make_opname_locator(opname_from_category(insop->category), loc,
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
          target_type = type_for_type_index(inscp->target);
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
      case ifc_NameSort_Template:
        { an_ifc_NameSort_Template inst;
          get_NameSort_Template(&inst);
          unexpected_condition_str("NameSort::Template is not yet handled.");
        }
        break;
      case ifc_NameSort_Specialization:
        { an_ifc_NameSort_Specialization inss;
          get_NameSort_Specialization(&inss);
          unexpected_condition_str("NameSort::Specialization"
                                   " is not yet handled.");
        }
        break;
      case ifc_NameSort_Identifier:
      default:
        unexpected_condition();
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
Map the IFC fields given by locus, type_index, alignment, traits, msvc_traits,
specifiers, and access to internal values used in the front end and set those
fields in *dps.  *psssp is a place in which to store various fields of the
decl_scope_level scope_stack entry (saved only if necessary).
restore_partial_scope_stack_if_necessary should be called with this pointer
after the declaration has been processed.
*/
{
  an_attribute_ptr ap = NULL;

  init_decl_parse_state(dps);
  if (psssp != NULL) psssp->saved = FALSE;
  if (type_index != 0) {
    dps->type = type_for_type_index(type_index);
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
      unexpected_condition_str("BasicSpecifiers::InitializedInClass"
                               " not yet handled"); /* FIXME */
    }  /* if */
    if (specifiers & ifc_BasicSpecifiers_NonExported) {
      unexpected_condition_str("BasicSpecifiers::NonExported"
                               " not yet handled"); /* FIXME */
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
      default:
        il_access = as_inaccessible;
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


a_constant_ptr an_ifc_module::constant_for_expr_index(
                                                    ifc_ExprIndex expr_index,
                                                    a_type_ptr    default_type)
                                                                 const noexcept
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
  read_partition_at_index(tag, expr_value(expr_index));
  switch (tag) {
    case ifc_ExprSort_Literal:
      { an_ifc_ExprSort_Literal iesl, *ieslp;
        a_type_ptr              constant_type, stripped_type;
        ieslp = get_ExprSort_Literal(&iesl);
        if (ieslp->type == 0) {
          /* If the expression doesn't have its own type, use the default
             type provided by the caller. */
          check_assertion(default_type != NULL);
          constant_type = default_type;
        } else {
          constant_type = type_for_type_index(ieslp->type);
        }  /* if */
        switch (literal_tag(ieslp->value)) {
          case ifc_LiteralSort_Immediate:
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
          case ifc_LiteralSort_Integer:
            /* An integer larger than 30 bits. */
            { a_host_large_unsigned value;
              cp = alloc_constant(ck_integer);
              read_partition_at_index(ifc_const_i64,
                                      literal_index(ieslp->value));
              GET_64bit_int(value, /*from_header=*/FALSE);
              stripped_type = skip_typerefs(constant_type);
              check_assertion(stripped_type->kind == (a_type_kind)tk_integer);
              set_unsigned_integer_constant(cp,
                                      (a_host_large_unsigned)value,
                                      stripped_type->variant.integer.int_kind);
              cp->type = constant_type;
            }
            break;
          case ifc_LiteralSort_FloatingPoint:
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


inline size_t an_ifc_module::file_offset_of(an_ifc_partition_kind partition,
                                            ifc_Index_type        index)
                                                                 const noexcept
/*
Returns the IFC file offset of the specified index into the given partition
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


inline void an_ifc_module::read_partition_at_offset(
                                               an_ifc_partition_kind partition,
                                               size_t                offset)
                                                                 const noexcept
/*
Sets the read buffer to the provided offset for the given partition in
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
                                                                 const noexcept
/*
Sets the read buffer to the appropriate offset for an entity at the provided
index into the given partition in preparation for a call to GET_byte,
GET_short, etc.
*/
{
  read_partition_at_offset(partition, file_offset_of(partition, index));
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_TypeSort   type_kind,
                                                   ifc_Index_type index)
                                                                 const noexcept
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_TypeSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_type_start + type_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_ExprSort   expr_kind,
                                                   ifc_Index_type index)
                                                                 const noexcept
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_ExprSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_expr_start + expr_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_StmtSort   stmt_kind,
                                                   ifc_Index_type index)
                                                                 const noexcept
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_StmtSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_stmt_start + stmt_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_DeclSort   decl_kind,
                                                   ifc_Index_type index)
                                                                 const noexcept
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_DeclSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_decl_start + decl_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_NameSort   name_kind,
                                                   ifc_Index_type index)
                                                                 const noexcept
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_NameSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_name_start + name_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_ChartSort  chart_kind,
                                                   ifc_Index_type index)
                                                                 const noexcept
/*
Overload wrapper for "read_partition_at_index" that converts an "ifc_ChartSort"
kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_chart_start +chart_kind),
                          index);
}  /* read_partition_at_index */


inline void an_ifc_module::read_partition_at_index(ifc_SyntaxSort syntax_kind,
                                                   ifc_Index_type index)
                                                                 const noexcept
/*
Overload wrapper for "read_partition_at_index" that converts an
"ifc_SyntaxSort" kind into an "an_ifc_partition_kind" kind for convenience.
*/
{
  read_partition_at_index((an_ifc_partition_kind)(ifc_syntax_start +
                                                                  syntax_kind),
                          index);
}  /* read_partition_at_index */


void an_ifc_module::str_ifc_text_offset(ifc_TextOffset      offset,
                                        a_str_control_block *scbp)
                                                                 const noexcept
/*
Add the string at the specified offset to the output buffer.
*/
{
  a_const_char *str = get_string_at_offset(offset);
  add_string_to_text_buffer(scbp->text_buffer, str);
}  /* str_ifc_text_offset */


void an_ifc_module::str_ifc_name_index(ifc_NameIndex       name_index,
                                       a_str_control_block *scbp)
                                                                 const noexcept
/*
Add the string represented by name_index to the current output buffer.
*/
{
  a_const_char *str = string_from_name_index(name_index,
                                             (a_symbol_locator*)NULL);
  add_string_to_text_buffer(scbp->text_buffer, str);
}  /* str_ifc_name_index */


void an_ifc_module::str_ifc_class_name(ifc_DeclIndex       home_scope,
                                       a_str_control_block *scbp)
                                                                 const noexcept
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


void an_ifc_module::str_ifc_source_location(
                                        ARG_UNUSED ifc_SourceLocation  *locus,
                                        ARG_UNUSED a_str_control_block *scbp)
                                                                 const noexcept
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
                                   a_str_control_block *scbp) const noexcept
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
    default:
      string = "Unexpected";
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
    case ifc_NoexceptSort_None:
      break;
    case ifc_NoexceptSort_False:
      add_string_to_text_buffer(scbp->text_buffer, "noexcept(false) ");
      break;
    case ifc_NoexceptSort_True:
      add_string_to_text_buffer(scbp->text_buffer, "noexcept(true) ");
      break;
    case ifc_NoexceptSort_Expression:
    case ifc_NoexceptSort_Weak:
    case ifc_NoexceptSort_Unenforced:
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
            { a_host_large_unsigned value;
              read_partition_at_index(ifc_const_i64,
                                      literal_index(ieslp->value));
              GET_64bit_int(value, /*from_header=*/FALSE);
              str_ifc_add_number(value, scbp);
            }
            break;
          case ifc_LiteralSort_FloatingPoint:
            /* FIXME: not yet implemented. */
          default:
            unexpected_condition();
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
    case ifc_ExprSort_Identifier:
      { an_ifc_ExprSort_Identifier ieid;
        get_ExprSort_Identifier(&ieid);
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
                                                                 const noexcept
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
          default:
            unexpected_condition();
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
              case ifc_TypePrecision_Bit64:
              case ifc_TypePrecision_Bit128:
              default:
                unexpected_condition();
            }  /* switch */
            break;
          case ifc_TypeBasis_Int:
            switch (itsfp->precision) {
              case ifc_TypePrecision_Default: basis_str = "int";     break;
              case ifc_TypePrecision_Short:   basis_str = "short";   break;
              case ifc_TypePrecision_Long:    basis_str = "long";    break;
              case ifc_TypePrecision_Bit64:   basis_str = "long long"; break;
              /* FIXME: not sure how to map these: */
              case ifc_TypePrecision_Bit16:
              case ifc_TypePrecision_Bit32:
              case ifc_TypePrecision_Bit128:
              default:
                unexpected_condition();
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
          default:
            /* FIXME: not implemented yet */
            unexpected_condition();
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
    case ifc_TypeSort_Deduced:
      { an_ifc_TypeSort_Deduced itsd;
        get_TypeSort_Deduced(&itsd);
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
                                                                 const noexcept
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
  read_partition_at_index(tag, decl_value(decl_index));
  switch (tag) {
    case ifc_DeclSort_Variable:
      { an_ifc_DeclSort_Variable idsv, *idsvp;
        idsvp = get_DeclSort_Variable(&idsv);
        /* Emit a variable declaration. */
        /* FIXME: idsvp->alignment exists but is an ExprIndex. */
        str_ifc_common_decl(&idsvp->locus, idsvp->access, idsvp->specifier,
                            idsvp->traits, (ifc_Alignment)0, scbp);
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
                            idsfp->traits, (ifc_Alignment)0, scbp);
        str_ifc_type_index(idsfp->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_text_offset(idsfp->name, scbp);
      }
      break;
    case ifc_DeclSort_Bitfield:
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
    case ifc_DeclSort_Function:
      { an_ifc_DeclSort_Function idsf, *idsfp;
        idsfp = get_DeclSort_Function(&idsf);
        str_ifc_common_decl(&idsfp->locus, idsfp->access,
                            (ifc_BasicSpecifiers)ifc_BasicSpecifiers_Cxx,
                            ifc_ObjectTraits_None, (ifc_Alignment)0, scbp);
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
                            ifc_ObjectTraits_None, (ifc_Alignment)0, scbp);
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
                            ifc_ObjectTraits_None, (ifc_Alignment)0, scbp);
        str_ifc_type_index_first_part(idsip->type, scbp);
        add_char_to_text_buffer(scbp->text_buffer, ' ');
        str_ifc_text_offset(idsip->name, scbp);
        str_ifc_type_index_second_part(idsip->type, scbp);
      }
      break;
    case ifc_DeclSort_Constructor:
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
    case ifc_DeclSort_Destructor:
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
    case ifc_DeclSort_Alias:
      { an_ifc_DeclSort_Alias       idsta, *idstap;
        an_ifc_TypeSort_Fundamental itsf, *itsfp;
        idstap = get_DeclSort_Alias(&idsta);
        /* FIXME: lots missing */
        check_assertion(type_tag(idstap->type) == ifc_TypeSort_Fundamental);
        /* Read the type to see what kind it is. */
        read_partition_at_index(ifc_type_fundamental,
                                type_value(idstap->type));
        itsfp = get_TypeSort_Fundamental(&itsf);
        if (itsfp->basis == ifc_TypeBasis_Typename) {
          /* A type alias. */
          add_string_to_text_buffer(scbp->text_buffer, "typedef ");
          str_ifc_type_index(idstap->initializer, scbp);
          add_char_to_text_buffer(scbp->text_buffer, ' ');
          str_ifc_text_offset(idstap->name, scbp);
        } else if (itsfp->basis == ifc_TypeBasis_Namespace) {
          /* A namespace alias. */
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
                            (ifc_ObjectTraits)ifc_ObjectTraits_None,
                            (ifc_Alignment)0, scbp);
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


void an_ifc_module::str_ifc_statement(
                                     ifc_StmtIndex                  stmt_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                 const noexcept
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
    case ifc_StmtSort_SyntaxTree:
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


void an_ifc_module::str_ifc_chart(ifc_ChartIndex                 chart_index,
                                  ARG_UNUSED a_str_control_block *scbp)
                                                                 const noexcept
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
    default:
      unexpected_condition_str("Unknown ChartSort kind");
  }  /* switch */
}  /* str_ifc_chart */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Deprecated>
                                    (ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                const noexcept
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
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Specialization>
                                    (ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                const noexcept
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
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_Friend>
                                    (ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                const noexcept
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
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_ConstexprFunction>
                                    (ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated constexpr function trait.
*/
{
  an_ifc_Trait_ConstexprFunction itcf;

  read_partition_at_index(ifc_trait_constexpr_function, decl_index);
  get_Trait_ConstexprFunction(&itcf);
  unexpected_condition_str("AssociatedTrait<ConstexprFunction>"
                           " is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_ConstexprFunction> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_FunctionTemplate>
                                    (ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated function template trait.
*/
{
  an_ifc_Trait_FunctionTemplate itft;

  read_partition_at_index(ifc_trait_function_template, decl_index);
  get_Trait_FunctionTemplate(&itft);
  unexpected_condition_str("AssociatedTrait<FunctionTemplate>"
                           " is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_FunctionTemplate> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_ClassTemplate>
                                    (ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated class template trait.
*/
{
  an_ifc_Trait_ClassTemplate itct;

  read_partition_at_index(ifc_trait_class_template, decl_index);
  get_Trait_ClassTemplate(&itct);
  unexpected_condition_str("AssociatedTrait<ClassTemplate> is not specified.");
}  /* str_ifc_associated_trait<an_ifc_Trait_ClassTemplate> */


template<>
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_AliasTemplate>
                                    (ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                const noexcept
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
void an_ifc_module::str_ifc_associated_trait<an_ifc_Trait_VariableTemplate>
                                    (ifc_DeclIndex                  decl_index,
                                     ARG_UNUSED a_str_control_block *scbp)
                                                                const noexcept
/*
Generate a string for the specified associated variable template trait.
*/
{
  an_ifc_Trait_VariableTemplate itvt;

  read_partition_at_index(ifc_trait_variable_template, decl_index);
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

  read_partition_at_index(ifc_msvc_trait_vendor_traits, decl_index);
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

  read_partition_at_index(ifc_msvc_trait_uuid, decl_index);
  get_Trait_MsvcUuid(&itmsvcuuid);
  snprintf(str, sizeof(str), "%04hx", itmsvcuuid.uuid);
  add_string_to_text_buffer(scbp->text_buffer, str);
}  /* str_ifc_associated_trait<an_ifc_Trait_MsvcUuid> */


void an_ifc_module::str_ifc_syntax_node(
                                   ifc_SyntaxIndex                syntax_index,
                                   ARG_UNUSED a_str_control_block *scbp)
                                                                 const noexcept
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
    default:
      unexpected_condition_str("Unexpected syntax sort");
      break;
  }  /* switch */
}  /* str_ifc_syntax_node */


void an_ifc_module::str_ifc_sentence(
                                ifc_SentenceIndex               sentence_index,
                                ARG_UNUSED a_str_control_block *scbp)
                                                                 const noexcept
/*
Generate a string for the specified sentence.
*/
{
  an_ifc_Sentence is;

  read_partition_at_index(ifc_sentence, sentence_index);
  get_Sentence(&is);
  unexpected_condition_str("IFC Sentences currently unspecified.");
}  /* str_ifc_sentence*/


void an_ifc_module::str_ifc_word(ifc_WordIndex                  word_index,
                                 ARG_UNUSED a_str_control_block *scbp)
                                                                 const noexcept
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

void an_ifc_module::db_ifc_file_header() const noexcept
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
* Copyright 1988-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
