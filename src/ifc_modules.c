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


/* Forward declarations. */
static void process_ifc_scope(a_module_ptr   mod,
                              ifc_ScopeIndex scope_index,
                              a_scope_ptr    scope);

static void str_ifc_declaration(ifc_DeclIndex       decl_index,
                                a_boolean           is_designated_type,
                                a_str_control_block *scbp);

static void str_ifc_name_index(ifc_NameIndex      name_index,
                               a_str_control_block *scbp);

static void str_ifc_type_index(ifc_TypeIndex       type_index,
                               a_str_control_block *scbp);

static a_type_ptr type_for_ifc_type_index(a_module_ptr  mod,
                                          ifc_TypeIndex type_index);

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
static an_ifc_partition
                *debug_partition;
                        /* Points to information about the partition currently
                           being read (for debugging purposes only). */
static a_module_ptr
                debug_mod;
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
                     ((char *)debug_mod->mmap_addr + debug_partition->offset) -
                      length),
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
#define GET_Sequence(x)        (GET_int((x).start), \
                                GET_int((x).cardinality))
#define GET_ContentHash(x)     (GET_64bit_int((x).bytes[0]), \
                                GET_64bit_int((x).bytes[1]), \
                                GET_64bit_int((x).bytes[2]), \
                                GET_64bit_int((x).bytes[3]))
#define GET_ModuleReference(x) (GET_int((x).owner), \
                                GET_int((x).partition))
#define GET_SourceLocation(x)  (GET_int((x).line), \
                                GET_int((x).column))
/* Note that this includes padding: */
#define GET_NoexceptSpecification(x) \
                               (GET_int((x).words), \
                                GET_byte((x).sort), \
                                pad(3))
#define GET_ParameterizedEntity(x) (unexpected_condition_str \
                         ("Lacking specification for a parameterized entity."))

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
#define GET_SentenceOffset(x)     GET_int(x)
#define GET_StmtIndex(x)          GET_int(x)
#define GET_StringIndex(x)        GET_int(x)
#define GET_SyntaxIndex(x)        GET_int(x)
#define GET_TextOffset(x)         GET_int(x)
#define GET_TokenCategory(x)      GET_int(x)
#define GET_TypeIndex(x)          GET_int(x)
#define GET_UniqueID(x)           GET_int(x)
#define GET_UnitIndex(x)          GET_int(x)

#define GET_Alignment(x)          GET_short(x)
#define GET_EHFlags(x)            GET_short(x)
#define GET_FunctionTraits(x)     GET_short(x)
#define GET_OperatorCategory(x)   GET_short(x)
#define GET_PackSize(x)           GET_short(x)

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

/* Utility to save the current partition (for display during debugging). */
#if DEBUG
#define set_debug_partition(mod, kind) \
  (debug_mod = mod, \
   debug_partition = &(mod)->variant.ifc->partitions[(kind)]),
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
#define full_ifc_file_offset_of(mod, kind, idx) \
   ((mod)->variant.ifc->partitions[kind].offset + \
    (idx) * (mod)->variant.ifc->partitions[(kind)].entry_size)

#if EXPENSIVE_CHECKING
#define ifc_file_offset_of(mod, kind, _offset) \
  (check_assertion((mod)->variant.ifc->partitions[(kind)].offset != 0 && \
                   (mod)->variant.ifc->partitions[(kind)].size != 0 && \
                   (mod)->variant.ifc->partitions[(kind)].size > (_offset)), \
   full_ifc_file_offset_of(mod, kind, _offset))
#else /* !EXPENSIVE_CHECKING */
#define ifc_file_offset_of(mod, kind, offset) \
 full_ifc_file_offset_of(mod, kind, offset)
#endif /* EXPENSIVE_CHECKING */

/*
Utilities to set the buffer to the specified partition at the desired offset
or index, in preparation for a call to GET_byte, GET_short, or GET_int.
*/
#define read_ifc_partition_at_offset(mod, kind, offset) \
  (set_debug_partition(mod, kind) \
   init_byte_buffer(((char*)(mod)->mmap_addr + (offset)), \
                    (mod)->variant.ifc->partitions[kind].size))

#define read_ifc_partition_at_index(mod, kind, idx) \
  read_ifc_partition_at_offset((mod), (kind),\
                               ifc_file_offset_of((mod), (kind), (idx)))


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
#define verify_offset(mod, offset) \
  (check_assertion((offset) < (mod)->variant.ifc->header.string_table_size)),
#else /* !EXPENSIVE_CHECKING */
#define verify_offset(mod, offset) /**/
#endif /* EXPENSIVE_CHECKING */
#define get_string_at_offset(mod, offset) \
  (verify_offset(mod, offset) \
   (a_const_char*)((char*)(mod)->mmap_addr + \
    (mod)->variant.ifc->header.string_table_bytes + (offset)))

#if DEBUG

void db_ifc_file_header(a_module_ptr mod)
/*
Display the contents of the IFC file header for the specified module.
*/
{
  an_ifc_File_Header *hdr = &mod->variant.ifc->header;

  /* FIXME: Print checksum */
  (void)fprintf(f_debug, "  major_version = %d\n", hdr->major_version);
  (void)fprintf(f_debug, "  minor_version = %d\n", hdr->minor_version);
  (void)fprintf(f_debug, "  abi = %d\n", hdr->abi);
  (void)fprintf(f_debug, "  arch = %d\n", hdr->arch);
  (void)fprintf(f_debug, "  dialect = %d\n", hdr->dialect);
  (void)fprintf(f_debug, "  string_table_bytes = 0x%08x\n",
                                                      hdr->string_table_bytes);
  (void)fprintf(f_debug, "  string_table_size = %d\n", hdr->string_table_size);
  (void)fprintf(f_debug, "  unit = %d\n", hdr->unit);
  (void)fprintf(f_debug, "  src_path = 0x%08x \"%s\"\n", hdr->src_path,
                                     get_string_at_offset(mod, hdr->src_path));
  (void)fprintf(f_debug, "  global_scope = %d\n", hdr->global_scope);
  (void)fprintf(f_debug, "  toc = 0x%08x\n", hdr->toc);
  (void)fprintf(f_debug, "  partition_count = %d\n", hdr->partition_count);
  (void)fprintf(f_debug, "  internal = %d\n", hdr->internal);
}  /* db_ifc_File_header */


static a_const_char *db_decl_tag(ifc_DeclSort tag)
/*
Return a string with the name that corresponds to the DeclSort tag.
*/
{
  a_const_char *result;

  switch (tag) {
    case ifc_DeclSort_VendorExtension:   result = "VendorExtension"; break;
    case ifc_DeclSort_Enumerator:        result = "Enumerator"; break;
    case ifc_DeclSort_Variable:          result = "Variable"; break;
    case ifc_DeclSort_Parameter:         result = "Parameter"; break;
    case ifc_DeclSort_Field:             result = "Field"; break;
    case ifc_DeclSort_Bitfield:          result = "Bitfield"; break;
    case ifc_DeclSort_Scope:             result = "Scope"; break;
    case ifc_DeclSort_Enumeration:       result = "Enumeration"; break;
    case ifc_DeclSort_Alias:             result = "Alias"; break;
    case ifc_DeclSort_Temploid:          result = "Temploid"; break;
    case ifc_DeclSort_Template:          result = "Template"; break;
    case ifc_DeclSort_PartialSpecialization:
                                       result = "PartialSpecialization"; break;
    case ifc_DeclSort_ExplicitSpecialization:
                                       result = "ExplicitSpecialization";break;
    case ifc_DeclSort_ExplicitInstantiation:
                                       result = "ExplicitInstantiation"; break;
    case ifc_DeclSort_Concept:           result = "Concept"; break;
    case ifc_DeclSort_Intrinsic:         result = "Intrinsic"; break;
    case ifc_DeclSort_Function:          result = "Function"; break;
    case ifc_DeclSort_Method:            result = "Method"; break;
    case ifc_DeclSort_Constructor:       result = "Constructor"; break;
    case ifc_DeclSort_InheritedConstructor:
                                        result = "InheritedConstructor"; break;
    case ifc_DeclSort_Destructor:        result = "Destructor"; break;
    case ifc_DeclSort_Reference:         result = "Reference"; break;
    case ifc_DeclSort_Property:          result = "Property"; break;
    case ifc_DeclSort_OutputSegment:     result = "OutputSegment"; break;
    case ifc_DeclSort_UsingDeclaration:  result = "UsingDeclaration"; break;
    case ifc_DeclSort_UsingDirective:    result = "UsingDirective"; break;
    case ifc_DeclSort_Friend:            result = "Friend"; break;
    case ifc_DeclSort_SyntaxTree:        result = "SyntaxTree"; break;
    case ifc_DeclSort_Tuple:             result = "Tuple"; break;
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* db_decl_tag */

#endif /* DEBUG */

static a_module_entity_ptr get_ifc_module_entity_ptr(
                                        a_module_ptr          mod,
                                        an_ifc_partition_kind partition,
                                        size_t                partition_offset)
/*
Utility to return a module entity pointer given a module, IFC partition, and
offset within that partition.  For cases where the module entity has
just been created, the partition is set according to the partition supplied
by the caller.
*/
{
  a_module_entity_ptr mep;

  mep = get_module_entity_ptr(mod,
                         ifc_file_offset_of(mod, partition, partition_offset));
  if (mep->variant.ifc_partition == ifc_none) {
    mep->variant.ifc_partition = partition;
  } else {
    check_assertion(mep->variant.ifc_partition == partition);
  }  /* if */
  return mep;
}  /* get_ifc_module_entity_ptr */


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


static void str_ifc_text_offset(ifc_TextOffset     offset,
                               a_str_control_block *scbp)
/*
Add the string at the specified offset to the output buffer.
*/
{
  add_string_to_text_buffer(scbp->text_buffer,
                            get_string_at_offset(scbp->module_info, offset));
}  /* str_ifc_text_offset */


static an_opname_kind opname_from_category(ifc_OperatorCategory category)
/*
Map an IFC OperatorCategory to an_opname_kind.
*/
{
  an_opname_kind op;

  /* FIXME: Note that some of these get mapped to the same entry. */
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
    case ifc_OperatorCategory_LshiftEq:     op = onk_shift_left_assign; break;
    case ifc_OperatorCategory_RshiftEq:     op = onk_shift_right_assign; break;
    case ifc_OperatorCategory_MinusEq:      op = onk_minus_assign; break;
    case ifc_OperatorCategory_ModuloEq:     op = onk_remainder_assign; break;
    case ifc_OperatorCategory_StarEq:       op = onk_times_assign; break;
    case ifc_OperatorCategroy_BitorEq:      op = onk_or_assign; break;
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

    /* These don't have direct mappings: */
    case ifc_OperatorCategory_Percent:              /* operator% */
    case ifc_OperatorCategory_Sizeof:               /* operator sizeof */
    case ifc_OperatorCategory_ExpandingSizeof:      /* operator sizeof... */
    case ifc_OperatorCategory_Throw:                /* operator throw */
    case ifc_OperatorCategory_Alignof:              /* operator alignof */
    case ifc_OperatorCategory_Noexcept:             /* operator noexcept */
    case ifc_OperatorCategory_Requires:             /* operator requires */
    case ifc_OperatorCategory_Coreturn:             /* operator co_return */
    case ifc_OperatorCategory_Await:                /* operator co_yield */
    case ifc_OperatorCategory_Yield:                /* operator co_yield */
    case ifc_OperatorCategory_StaticAssert:         /* operator static_assert*/
    case ifc_OperatorCategory_Dot:                  /* operator. */
    case ifc_OperatorCategory_DerefMemberAccess:    /* operator.* */
    default:
#if DEBUG
      if (db_flag_is_set("ms_ignore")) {
        (void)fprintf(f_debug, "Unsupported operation: %d\n", category);
      }  /* if */
#endif /* DEBUG */
      unexpected_condition();
  }  /* switch */
  return op;
}  /* opname_from_category */


static a_const_char *string_from_name_index(a_module_ptr     mod,
                                            ifc_NameIndex    name_index,
                                            a_symbol_locator *loc)
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

  if (tag == (an_ifc_NameSort)ifc_NameSort_Identifier) {
    /* NameSort::Identifiers just refer to the string table. */
    result = get_string_at_offset(mod, name_value(name_index));
  } else {
    read_ifc_partition_at_index(mod, ifc_name_start + tag,
                                name_value(name_index));
    switch (tag) {
      case ifc_NameSort_SourceFile:
        { an_ifc_NameSort_SourceFile inssf, *inssfp;
          inssfp = get_NameSort_SourceFile(&inssf);
          result = get_string_at_offset(mod, inssfp->path);
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
            result = get_string_at_offset(mod, insop->encoded);
          }  /* if */
        }
        break;
      case ifc_NameSort_Conversion:
        { an_ifc_NameSort_Conversion insc, *inscp;
          a_type_ptr                 target_type;
          inscp = get_NameSort_Conversion(&insc);
          prefix = "operator ";
          target_type = type_for_ifc_type_index(mod, inscp->target);
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
      case ifc_NameSort_Literal:
        { an_ifc_NameSort_Literal insl, *inslp;
          inslp = get_NameSort_Literal(&insl);
          result = get_string_at_offset(mod, inslp->encoded);
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
      case ifc_NameSort_Specialization:
        unexpected_condition(); /* FIXME: not implemented yet. */
        break;
      case ifc_NameSort_Identifier:
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


static void str_ifc_name_index(ifc_NameIndex       name_index,
                               a_str_control_block *scbp)
/*
Add the string represented by name_index to the current output buffer.
*/
{
  add_string_to_text_buffer(scbp->text_buffer,
                            string_from_name_index(scbp->module_info,
                                                   name_index,
                                                   (a_symbol_locator*)NULL));
}  /* str_ifc_name_index */


static void str_ifc_class_name(ifc_DeclIndex       home_scope,
                               a_str_control_block *scbp)
/*
For constructors and destructors, add the name of the class specified
by home_scope to the output buffer.
*/
{
  ifc_DeclSort          tag = decl_tag(home_scope);
  an_ifc_DeclSort_Scope idss, *idssp;

  /* Prepare to read from the proper partition for this declaration. */
  read_ifc_partition_at_index(scbp->module_info, ifc_decl_start + tag,
                              decl_value(home_scope));
  check_assertion(tag == ifc_DeclSort_Scope);
  idssp = get_DeclSort_Scope(&idss);
  str_ifc_name_index(idssp->name, scbp);
}  /* str_ifc_class_name */


static void str_ifc_add_number(a_host_large_unsigned value,
                               a_str_control_block   *scbp)
/*
Add the decimal representation of value to the output buffer.
*/
{
  char     buffer[50];
  sizeof_t len = unsigned_to_string_buf(value, buffer);

  add_to_text_buffer(scbp->text_buffer, buffer, len);
}  /* str_ifc_add_number */


static void str_ifc_source_location(ifc_SourceLocation  *locus,
                                    a_str_control_block *scbp)
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


static void str_ifc_access(ifc_Access          access,
                           a_str_control_block *scbp)
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
      unexpected_condition();
  }  /* switch */
  if (string != NULL) {
    add_string_to_text_buffer(scbp->text_buffer, string);
  }  /* if */
}  /* str_ifc_access */


static void str_ifc_alignment(ifc_Alignment       alignment,
                              a_str_control_block *scbp)
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


static void str_ifc_qualifiers(ifc_Qualifiers      qualifiers,
                               a_str_control_block *scbp)
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


static void str_ifc_basic_specifiers(ifc_BasicSpecifiers specifiers,
                                     a_str_control_block *scbp)
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


static void str_ifc_object_traits(ifc_ObjectTraits    traits,
                                  a_str_control_block *scbp)
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


static void str_ifc_msvc_traits(ifc_MsvcTraits      traits,
                                a_str_control_block *scbp)
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


static void str_ifc_function_type_traits(ifc_FunctionTypeTraits traits,
                                         a_str_control_block    *scbp)
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


static void str_ifc_noexcept_specification(ifc_NoexceptSpecification *eh_spec,
                                           a_str_control_block       *scbp)
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
    case ifc_NoexceptSort_Deduced:
      /* FIXME: not sure what these should be */
    default:
      unexpected_condition();
  }  /* switch */
}  /* str_ifc_noexcept_specification */


static void str_ifc_function_traits(ifc_FunctionTraits  traits,
                                    a_boolean           prefix,
                                    a_str_control_block *scbp)
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


static void str_ifc_expr_index(ifc_ExprIndex       expr_index,
                               a_str_control_block *scbp)
/*
Add a textual representation of the expression referenced by expr_index to
the output buffer.
*/
{
  ifc_ExprSort      tag = expr_tag(expr_index);

  /* Prepare to read from the proper partition for this expression. */
  read_ifc_partition_at_index(scbp->module_info, ifc_expr_start + tag,
                              expr_value(expr_index));
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
              read_ifc_partition_at_index(scbp->module_info, ifc_const_i64,
                                          literal_index(ieslp->value));
              GET_64bit_int(value);
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
    case ifc_ExprSort_Empty:
    case ifc_ExprSort_Type:
    case ifc_ExprSort_NamedDecl:
    case ifc_ExprSort_UnresolvedId:
    case ifc_ExprSort_TemplateId:
    case ifc_ExprSort_Identifier:
    case ifc_ExprSort_SimpleIdentifier:
    case ifc_ExprSort_Pointer:
    case ifc_ExprSort_QualifiedName:
    case ifc_ExprSort_Path:
    case ifc_ExprSort_Read:
    case ifc_ExprSort_Monad:
    case ifc_ExprSort_Dyad:
    case ifc_ExprSort_Triad:
    case ifc_ExprSort_Tuple:
    case ifc_ExprSort_Tokens:
    case ifc_ExprSort_String:
    case ifc_ExprSort_Temporary:
    case ifc_ExprSort_Call:
    case ifc_ExprSort_PushState:
    case ifc_ExprSort_TypeTraitIntrinsic:
    case ifc_ExprSort_MemberInitializer:
    case ifc_ExprSort_MemberAccess:
    case ifc_ExprSort_InheritancePath:
    case ifc_ExprSort_TemplateReference:
    case ifc_ExprSort_InitializerList:
    case ifc_ExprSort_Cast:
    case ifc_ExprSort_Condition:
    case ifc_ExprSort_ExpressionList:
    case ifc_ExprSort_AssignInitializer:
    case ifc_ExprSort_Nullptr:
    case ifc_ExprSort_This:
    case ifc_ExprSort_SizeofTypeId:
    case ifc_ExprSort_Alignof:
    case ifc_ExprSort_PackedTemplateArguments:
    case ifc_ExprSort_New:
    case ifc_ExprSort_Delete:
    case ifc_ExprSort_Lambda:
    case ifc_ExprSort_Typeid:
    case ifc_ExprSort_SyntaxTree:
    default:
      /* FIXME: for now: */
      add_string_to_text_buffer(scbp->text_buffer,
                                "<unimplemented expression>");
      break;
  }  /* if */
}  /* str_ifc_expr_index */


static void str_ifc_scope_index(ifc_ScopeIndex      scope_index,
                                a_str_control_block *scbp)
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
    read_ifc_partition_at_index(scbp->module_info, ifc_scope_desc,
                                scope_index - 1);
    isdp = get_Scope_Descriptor(&isd);
    for (i = 0; i < isdp->cardinality; i++) {
      /* Re-enable access to scope.member partition (it changes during the
         loop). */
      read_ifc_partition_at_index(scbp->module_info, ifc_scope_member,
                                  isdp->start + i);
      ismp = get_Scope_Member(&ism);
      str_ifc_declaration(ismp->index, /*is_designated_type=*/FALSE, scbp);
    }  /* for */
  }  /* if */
}  /* str_ifc_scope_index */


static a_type_ptr type_for_ifc_type_index(a_module_ptr  mod,
                                          ifc_TypeIndex type_index)
/*
Returns the type that corresponds to the specified TypeIndex in the module
file indicated by mod.  Note that NULL is a valid return (and represents
an "ellipsis type").
*/
{
  a_type_ptr                result = NULL;
  a_module_entity_ptr       mep = get_type_module_entity_ptr(mod, type_index);
  ifc_TypeSort              tag;

  if (mep->entity.ptr != NULL) {
    /* There is already an entry for this; return it. */
    check_assertion(mep->entity.kind == iek_type);
    result = (a_type_ptr)mep->entity.ptr;
  } else {
    /* Prepare to read from the proper partition for this type. */
    read_ifc_partition_at_offset(mod, mep->variant.ifc_partition,
                                 mep->file_offset);
    tag = get_tag_from_partition(mep->variant.ifc_partition, ifc_type_start);
    switch (tag) {
      case ifc_TypeSort_Fundamental:
        { an_integer_kind             ik;
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          itsfp = get_TypeSort_Fundamental(&itsf);
          /* Note: no check is made for non-sensical types (e.g., signed
             void).*/
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
          result = make_qualified_type(
                              type_for_ifc_type_index(mod, itsqp->unqualified),
                              qualifiers);
        }
        break;
      case ifc_TypeSort_Pointer:
        { an_ifc_TypeSort_Pointer itsp, *itspp;
          itspp = get_TypeSort_Pointer(&itsp);
          result = make_pointer_type(type_for_ifc_type_index(mod,
                                                             itspp->pointee));
        }
        break;
      case ifc_TypeSort_LvalueReference:
        { an_ifc_TypeSort_LvalueReference itslr, *itslrp;
          itslrp = get_TypeSort_LvalueReference(&itslr);
          result = make_reference_type(type_for_ifc_type_index(mod,
                                                             itslrp->referee));
        }
        break;
      case ifc_TypeSort_RvalueReference:
        { an_ifc_TypeSort_RvalueReference itsrr, *itsrrp;
          itsrrp = get_TypeSort_RvalueReference(&itsrr);
          result = make_rvalue_reference_type(type_for_ifc_type_index(mod,
                                                             itsrrp->referee));
        }
        break;
      case ifc_TypeSort_Array:
        { an_ifc_TypeSort_Array itsa, *itsap;
          itsap = get_TypeSort_Array(&itsa);
          result = alloc_type((a_type_kind)tk_array);
          result->variant.array.element_type =
                                  type_for_ifc_type_index(mod, itsap->element);
          /* FIXME: this is wrong: */
          result->variant.array.variant.number_of_elements = itsap->extent;
        }
        break;
      case ifc_TypeSort_Method: /* FIXME: for now (same structures?): */
        unexpected_condition(); /* FIXME: No longer same structures. */
        break;
      case ifc_TypeSort_Function:
        { an_ifc_TypeSort_Function itsf, *itsfp;
          itsfp = get_TypeSort_Function(&itsf);
          /* Create a routine type with no parameters to start. */
          result = make_routine_type(type_for_ifc_type_index(mod,
                                                             itsfp->target),
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
              read_ifc_partition_at_index(mod, ifc_type_tuple,
                                          type_value(itsfp->source));
              itstp = get_TypeSort_Tuple(&itst);
              for (i = 0; i < itstp->cardinality; i++) {
                ifc_TypeIndex type_index;
                read_ifc_partition_at_index(mod, ifc_heap_type,
                                            itstp->start + i);
                GET_TypeIndex(type_index);
                param_type = type_for_ifc_type_index(mod, type_index);
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
              param_type = type_for_ifc_type_index(mod, itsfp->source);
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
          dmep = get_decl_module_entity_ptr(mod, itsdp->decl);
          // FIXME: scope is unknown means bad news.
          check_assertion(dmep->scope != NULL);
          process_ifc_declaration(dmep, /*defer=*/FALSE, (a_type_ptr)NULL);
          class_or_enum_type = (a_type_ptr)dmep->entity.ptr;
          check_assertion(class_or_enum_type != NULL &&
                          dmep->entity.kind == iek_type);
          result = class_or_enum_type;
        }
        break;
      case ifc_TypeSort_PointerToMember:
      case ifc_TypeSort_Tuple:
      case ifc_TypeSort_VendorExtension:
      case ifc_TypeSort_Syntactic:
      case ifc_TypeSort_Expansion:
      case ifc_TypeSort_Typename:
      case ifc_TypeSort_Base:
      case ifc_TypeSort_Unaligned:
      case ifc_TypeSort_Decltype:
      case ifc_TypeSort_SyntaxTree:
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


static void str_ifc_type_index_first_part(ifc_TypeIndex       type_index,
                                          a_str_control_block *scbp)
/*
Create a string representation for the specified type (first part).
FIXME: more specific
*/
{
  ifc_TypeSort      tag = type_tag(type_index);

  /* Prepare to read from the proper partition for this type. */
  read_ifc_partition_at_index(scbp->module_info, ifc_type_start + tag,
                              type_value(type_index));
  switch (tag) {
    case ifc_TypeSort_Fundamental:
      { a_const_char *basis_str = NULL;
        an_ifc_TypeSort_Fundamental itsf, *itsfp;
        itsfp = get_TypeSort_Fundamental(&itsf);
        /* Note: no check is made for non-sensical types (e.g., signed void).*/
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
    case ifc_TypeSort_Method: /* FIXME: for now (same structures?): */
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
          ifc_TypeIndex type_index;
          read_ifc_partition_at_index(scbp->module_info, ifc_heap_type,
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
    case ifc_TypeSort_Syntactic:
    case ifc_TypeSort_Expansion:
    case ifc_TypeSort_Typename:
    case ifc_TypeSort_Unaligned:
    case ifc_TypeSort_Decltype:
    case ifc_TypeSort_SyntaxTree:
    default:
#if DEBUG
      if (db_flag_is_set("ms_ignore")) {
        (void)fprintf(f_debug, "Unsupported type: %d, %d\n",
                      tag, type_value(type_index));
      }  /* if */
#endif /* DEBUG */
      break;
  }  /* switch */
}  /* str_ifc_type_index_first_part */


static void str_ifc_type_index_second_part(ifc_TypeIndex       type_index,
                                           a_str_control_block *scbp)
/*
Create a string representation for the specified type (second part).
FIXME: more specific
*/
{
  ifc_TypeSort      tag = type_tag(type_index);

  /* Prepare to read from the proper partition for this type. */
  read_ifc_partition_at_index(scbp->module_info, ifc_type_start + tag,
                              type_value(type_index));
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
    case ifc_TypeSort_Method: /* FIXME: for now (same structures?): */
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
        (void)fprintf(f_debug, "Unsupported type: %d, %d\n",
                      tag, type_value(type_index));
      }  /* if */
#endif /* DEBUG */
      break;
  }  /* switch */
}  /* str_ifc_type_index_second_part */


static void str_ifc_type_index(ifc_TypeIndex       type_index,
                               a_str_control_block *scbp)
/*
Add a textual representation of the specified type_index to the output buffer.
*/
{
  /* FIXME: doesn't work for array (and presumably other) types. */
  /* FIXME: perhaps get the type here and pass it to each? */
  str_ifc_type_index_first_part(type_index, scbp);
  str_ifc_type_index_second_part(type_index, scbp);
}  /* str_ifc_type_index */


static void str_ifc_common_decl(ifc_SourceLocation  *locus,
                                ifc_Access          access,
                                ifc_BasicSpecifiers specifiers,
                                ifc_ObjectTraits    traits,
                                ifc_Alignment       alignment,
                                a_str_control_block *scbp)
/*
This is a utility routine to add textual representations for various aspects
common fields of a DeclIndex.  If a particular DeclIndex doesn't have a
SourceLocation, Access, BasicSpecifiers, ObjectTraits, or Alignment field, a
nominal value can be supplied (which will suppress its output).
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


static void str_ifc_class_definition(an_ifc_DeclSort_Scope *idssp,
                                     a_str_control_block   *scbp)
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


static void str_ifc_declaration(ifc_DeclIndex       decl_index,
                                a_boolean           is_designated_type,
                                a_str_control_block *scbp)
/*
Generate a string for the specified declaration.
FIXME: Perhaps have a "flags" argument rather than is_designated_type?
*/
{
  ifc_DeclSort             tag = decl_tag(decl_index);
  a_boolean                end_decl = TRUE;

  /* Prepare to read from the proper partition for this declaration. */
  read_ifc_partition_at_index(scbp->module_info, ifc_decl_start + tag,
                              decl_value(decl_index));
  switch (tag) {
    case ifc_DeclSort_Variable:
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
        str_ifc_common_decl(&idsfp->locus, idsfp->access, idsfp->specifier,
                            idsfp->traits, idsfp->alignment, scbp);
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
    case ifc_DeclSort_Method:
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
        read_ifc_partition_at_index(scbp->module_info, ifc_type_fundamental,
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
        str_ifc_common_decl(&idsep->locus, idsep->access, idsep->specifiers,
                            (ifc_ObjectTraits)ifc_ObjectTraits_None,
                            idsep->alignment, scbp);
        check_assertion(type_tag(idsep->type) == ifc_TypeSort_Fundamental);
        /* Read the type to see what kind it is. */
        read_ifc_partition_at_index(scbp->module_info, ifc_type_fundamental,
                                    type_value(idsep->type));
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
    case ifc_DeclSort_Parameter:
    case ifc_DeclSort_Temploid:
    case ifc_DeclSort_Template:
    case ifc_DeclSort_PartialSpecialization:
    case ifc_DeclSort_ExplicitSpecialization:
    case ifc_DeclSort_ExplicitInstantiation:
    case ifc_DeclSort_Concept:
    case ifc_DeclSort_InheritedConstructor:
    case ifc_DeclSort_Reference:
    case ifc_DeclSort_Property:
    case ifc_DeclSort_OutputSegment:
    case ifc_DeclSort_UsingDeclaration:
    case ifc_DeclSort_UsingDirective:
    case ifc_DeclSort_Friend:
    case ifc_DeclSort_SyntaxTree:
    case ifc_DeclSort_Tuple:
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


static void source_position_from_locus(a_module_ptr       mod,
                                       a_source_position  *pos,
                                       ifc_SourceLocation *locus)
/*
Map the IFC locus source position information into the source position at *pos.
*/
{
  an_ifc_Source_Line   isl, *islp;
  ifc_NameSort         tag;
  a_seq_number         *seq;

  read_ifc_partition_at_index(mod, ifc_src_line, locus->line);
  islp = get_Source_Line(&isl);
  tag = name_tag(islp->file);
  check_assertion(tag == (ifc_NameSort)ifc_NameSort_SourceFile);
  /* See if this file has been used before. */
  seq = &mod->variant.ifc->sequence_numbers[name_value(islp->file)];
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
    file_name = string_from_name_index(mod, islp->file,
                                       (a_symbol_locator *)NULL);
    file_name = copy_string_to_region(FILE_SCOPE_REGION_NUMBER, file_name);
    record_inclusion_of_module_source_file(file_name, pos);
    *seq = pos->seq;
  } else {
    pos->seq = *seq;
  }  /* if */
  pos->column = locus->column;
}  /* source_position_from_locus */


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


static void init_dps(a_decl_parse_state          *dps,
                     a_module_ptr                mod,
                     ifc_SourceLocation          *locus,
                     ifc_TypeIndex               type_index,
                     ifc_Alignment               alignment,
                     ifc_ObjectTraits            traits,
                     ifc_MsvcTraits              msvc_traits,
                     ifc_BasicSpecifiers         specifiers,
                     ifc_Access                  access,
                     a_partial_scope_stack_state *psssp)
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
    dps->type = type_for_ifc_type_index(mod, type_index);
  }  /* if */
  source_position_from_locus(mod, &dps->start_pos, locus);
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
  if (access != ifc_Access_None) {
    an_access_specifier il_access;
    switch (access) {
      case ifc_Access_Private:   il_access = as_private;   break;
      case ifc_Access_Protected: il_access = as_protected; break;
      case ifc_Access_Public:    il_access = as_public;    break;
      default:
        unexpected_condition();
    }  /* switch */
    save_partial_scope_stack(psssp);
    scope_stack[decl_scope_level].current_access = il_access;
  }  /* if */
}  /* init_dps */


static void init_locator_from_name(a_module_ptr       mod,
                                   ifc_NameIndex      name_index,
                                   ifc_TextOffset     text_offset,
                                   ifc_SourceLocation *locus,
                                   a_symbol_locator   *loc)
/*
Initialize the locator specified by *loc.  The entity is in the module file
indicated by mod and the name of the entity is either given by name_index or
text_offset, whichever is non-zero.  The source position is given by locus.
FIXME: Not sure if we need source location here.
*/
{
  a_source_position pos;
  a_const_char      *name;

  source_position_from_locus(mod, &pos, locus);
  clear_locator(loc, &pos);
  if (name_index != 0) {
    name = string_from_name_index(mod, name_index, loc);
  } else {
    name = get_string_at_offset(mod, text_offset);
  }  /* if */
  if (!loc->is_operator_name &&
      !loc->is_conversion_name &&
      !loc->is_udl_operator_name) {
    /* Find the symbol (if not a special case). */
    (void)find_symbol(name, (sizeof_t)strlen(name), loc);
  }  /* if */
}  /* init_locator_from_name */


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


static a_constant_ptr constant_for_expr_index(a_module_ptr  mod,
                                              ifc_ExprIndex expr_index,
                                              a_type_ptr    default_type)
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
  read_ifc_partition_at_index(mod, ifc_expr_start + tag,
                              expr_value(expr_index));
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
          constant_type = type_for_ifc_type_index(mod, ieslp->type);
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
              read_ifc_partition_at_index(mod, ifc_const_i64,
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


/* FIXME: might be able to get rid of enumeration_type now that enums aren't
   deferred */
void process_ifc_declaration(a_module_entity_ptr mep,
                             a_boolean           defer,
                             a_type_ptr          enumeration_type)

/*
Process the IFC module entity declaration specified by mep either by creating
the appropriate IL entity, or, when defer is TRUE, mark the appropriate
symbol header as having a deferred module entity (which will be lazily loaded
if referenced).  When enumeration_type is non-NULL, it represents the
enumeration type for the enumerator being defined (and is added to the list of
constants for that type).
*/
{
  a_module_ptr             mod = mep->module_info;
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
    read_ifc_partition_at_offset(mod, mep->variant.ifc_partition,
                                 mep->file_offset);
    tag = get_tag_from_partition(mep->variant.ifc_partition, ifc_decl_start);
    switch (tag) {
      case ifc_DeclSort_Variable:
        { an_ifc_DeclSort_Variable idsv, *idsvp;
          idsvp = get_DeclSort_Variable(&idsv);
          init_locator_from_name(mod, idsvp->name, 0, &idsvp->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here. */
            a_variable_ptr vp;
            init_dps(&dps, mod, &idsvp->locus, idsvp->type, idsvp->alignment,
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
              a_constant_ptr cp = constant_for_expr_index(mod,
                                                          idsvp->initializer,
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
          init_locator_from_name(mod, idsfp->name, 0, &idsfp->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here. */
            a_routine_ptr rp;
            init_dps(&dps, mod, &idsfp->locus, idsfp->type, (ifc_Alignment)0,
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
      case ifc_DeclSort_Intrinsic:
        /* A builtin function declaration. */
        { a_func_info_block        func_info;
          a_type_ptr               old_type;
          a_routine_ptr            rp;
          an_ifc_DeclSort_Intrinsic idsi, *idsip;
          idsip = get_DeclSort_Intrinsic(&idsi);
          init_locator_from_name(mod, 0, idsip->name, &idsip->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            /* FIXME: lots more to do here (just copied
               ifc_DeclSort_Function).*/
            init_dps(&dps, mod, &idsip->locus, idsip->type, (ifc_Alignment)0,
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
        { an_ifc_DeclSort_Scope idss, *idssp;
          an_ifc_TypeSort_Fundamental itsf, *itsfp;
          a_type_kind                 type_kind;
          a_symbol_kind               tag_kind;
          idssp = get_DeclSort_Scope(&idss);
          /* Should be no unnamed namespaces or types. */
          check_assertion(idssp->name != 0);
          init_locator_from_name(mod, idssp->name, 0, &idssp->locus, &loc);
          /* Look at the "type" to determine whether we have a namespace or
             not. */
          check_assertion(type_tag(idssp->type) == ifc_TypeSort_Fundamental);
          read_ifc_partition_at_index(mod, ifc_type_fundamental,
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
                  process_ifc_scope(mod, idssp->initializer,
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
          init_locator_from_name(mod, 0, idstap->name, &idstap->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            check_assertion(type_tag(idstap->type)== ifc_TypeSort_Fundamental);
            /* Read the type to see what kind it is. */
            read_ifc_partition_at_index(mod, ifc_type_fundamental,
                                        type_value(idstap->type));
            itsfp = get_TypeSort_Fundamental(&itsf);
            if (itsfp->basis == ifc_TypeBasis_Typename) {
              /* A type alias; declare a typedef for this case. */
              init_dps(&dps, mod, &idstap->locus, idstap->initializer,
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
          read_ifc_partition_at_index(mod, ifc_type_fundamental,
                                      type_value(idsep->type));
          itsfp = get_TypeSort_Fundamental(&itsf);
          if (itsfp->basis == ifc_TypeBasis_Enum) {
            /* A classic enumeration.  Don't bother to defer in this case
               because each of the enumerators need to be registered in the
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
          init_locator_from_name(mod, 0, idsep->name, &idsep->locus, &loc);
          if (defer) {
            defer_symbol_creation(mep, &loc);
          } else {
            a_type_ptr   enum_type;
            a_symbol_ptr tag_sym;
            check_assertion(idsep->base != 0);
            init_dps(&dps, mod, &idsep->locus, idsep->base, idsep->alignment,
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
                emep = get_decl_module_entity_ptr(mod, 
                                        make_decl_index(
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
          init_locator_from_name(mod, 0, idsep->name, &idsep->locus, &loc);
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
            enum_con = constant_for_expr_index(mod, idsep->initializer,
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
      case ifc_DeclSort_Parameter:
      case ifc_DeclSort_Temploid:
      case ifc_DeclSort_Template:
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


static void process_ifc_scope(a_module_ptr   mod,
                              ifc_ScopeIndex scope_index,
                              a_scope_ptr    scope)
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
    read_ifc_partition_at_index(mod, ifc_scope_desc, scope_index - 1);
    isdp = get_Scope_Descriptor(&isd);
    for (i = 0; i < isdp->cardinality; i++) {
      /* Re-enable access to scope.member partition (it changes during the
         loop). */
      read_ifc_partition_at_index(mod, ifc_scope_member, isdp->start + i);
      ismp = get_Scope_Member(&ism);
      dmep = get_decl_module_entity_ptr(mod, ismp->index);
      dmep->scope = scope;
      process_ifc_declaration(dmep, /*defer=*/TRUE, (a_type_ptr)NULL);
    }  /* for */
    pop_module_declaration_context(scope_pushed);
  }  /* if */
}  /* process_ifc_scope */


static a_boolean open_and_map_ifc_module_file(a_module_import_decl_ptr midp)
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
      open_mapped_input_file(mod->full_name, &mod->mapped_input,
                             &mod->map_object);
#endif /* EDG_WIN32 */
      mod->mmap_size = stat_buf.st_size;
      mod->mmap_addr = map_input_file_to_region(file,
#if EDG_WIN32
                                                mod->map_object,
#else /* !EDG_WIN32 */
                                                (a_windows_handle)0,
#endif /* EDG_WIN32 */
                                                /*read_only=*/TRUE,
                                                (sizeof_t)0, mod->mmap_size,
                                                NULL, mod->full_name);
      check_assertion(mod->mmap_addr != NULL);
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
      mod->f_module = file;
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


static a_boolean import_ifc_module_file(a_module_import_decl_ptr midp)
/*
Import an IFC module file as specified in the module-import-declaration.
Note that a ".ifc" suffix is appended to the module name during the search
process.
*/
{
  a_module_ptr              mod = midp->module_info;
  unsigned int              i;
  an_ifc_File_Header        header;
  a_boolean                 result = FALSE;

  check_assertion(mod->name != NULL && mod->full_name != NULL);
  /* Found an IFC file; open it and verify its magic number. */
  if (open_and_map_ifc_module_file(midp)) {
    result = TRUE;
    /* Allocate storage for the module information. */
    mod->kind = mk_ifc;
    mod->variant.ifc = alloc_il_of_type(an_ifc_module);
    /* FIXME: perhaps a routine to explicitly initialize each piece? */
    memzero((char *)mod->variant.ifc, sizeof(an_ifc_module));
    /* Read the IFC file header (which starts after the magic number). */
    init_byte_buffer((char*)mod->mmap_addr + 4, mod->mmap_size - 4);
    memcpy(&(mod->variant.ifc->header), get_File_Header(&header),
           sizeof(header));
    /* FIXME: The checksum is not yet checked. */
    /* Prepare to read the partitions (by "seeking" to the IFC Table of
       Contents). */
    init_byte_buffer((char*)mod->mmap_addr + mod->variant.ifc->header.toc,
                     mod->mmap_size - mod->variant.ifc->header.toc);
#if DEBUG
    if (db_flag_is_set("ifc_modules")) {
      db_module(mod);
    }  /* if */
#endif /* DEBUG */
    for (i = 0; i < mod->variant.ifc->header.partition_count; i++) {
      an_ifc_Partition partition, *ifc_pp;
      an_ifc_partition *pp;
      a_const_char *name_str;
      an_ifc_partition_map *map_ptr;

      /* Read information about the partition. */
      ifc_pp = get_Partition(&partition);
      name_str = get_string_at_offset(mod, ifc_pp->name);
#if DEBUG
      if (db_flag_is_set("ifc_modules")) {
        (void)fprintf(f_debug,
            "partition %d \"%s\" offset 0x%08x cardinality %d entry_size %d\n",
            i, name_str, ifc_pp->offset, ifc_pp->cardinality,
            ifc_pp->entry_size);
      }  /* if */
#endif /* DEBUG */
      check_assertion(ifc_pp->cardinality != 0 && ifc_pp->offset != 0);
      /* FIXME: replace with binary search. */
      for (map_ptr = ifc_partition_map; map_ptr->name != NULL; map_ptr++) {
        if (strcmp(name_str, map_ptr->name) == 0) {
          break;
        }  /* if */
      }  /* for */
      if (map_ptr->name == NULL) {
        str_warning(ec_unknown_ifc_partition, name_str);
      } else {
        check_assertion_str(map_ptr->kind != ifc_last,
                            "no mapping for IFC partition");
        pp = &mod->variant.ifc->partitions[map_ptr->kind];
        pp->name = map_ptr->name;
        pp->offset = ifc_pp->offset;
        pp->size = ifc_pp->cardinality * ifc_pp->entry_size;
        pp->entry_size = ifc_pp->entry_size;
      }  /* if */
    }  /* for */
    (void)fseek(mod->f_module, 0L, SEEK_SET);
    if (mod->variant.ifc->partitions[ifc_name_source_file].name != NULL) {
      /* Allocate an array to map source files to sequence numbers for the
         the module.  No information about the sequence numbers is recorded
         yet (we do that only if the source file is later referenced). */
      an_ifc_partition *nsf_pp =
                          &mod->variant.ifc->partitions[ifc_name_source_file];
      size_t size;
      check_assertion(nsf_pp->entry_size != 0);
      size = (nsf_pp->size / nsf_pp->entry_size) * sizeof(a_seq_number);
      mod->variant.ifc->sequence_numbers = (a_seq_number *)alloc_il(size);
      memzero((char *)mod->variant.ifc->sequence_numbers, size);
    }  /* if */
#if DEBUG
    if (db_flag_is_set("ms_modsrc")) {
      /* Generate a textual representation of the module file and print it. */
      a_str_control_block scb;
      clear_str_control_block(&scb, mod, (a_text_buffer*)NULL);
      str_ifc_scope_index(mod->variant.ifc->header.global_scope, &scb);
      add_char_to_text_buffer(scb.text_buffer, '\0');
      (void)fwrite(scb.text_buffer->buffer, 1, scb.text_buffer->size, f_debug);
      (void)fputc('\n', f_debug);
    }  /* if */
#endif /* DEBUG */
    /* Indicate to the lookup routines that lazy symbols are in use. */
    lazy_symbols_may_be_visible = TRUE;
    /* Process all declarations in the global scope. */
    process_ifc_scope(mod, mod->variant.ifc->header.global_scope,
                      il_header.primary_scope);
  }  /* if */
  return result;
}  /* import_ifc_module_file */


void close_ifc_module_file(a_module_import_decl_ptr midp)
/*
Close the module file specified in the module-import-declaration.
*/
{
  a_module_ptr mod = midp->module_info;

  if (mod != NULL) {
    if (mod->f_module != NULL) {
      (void)fclose(mod->f_module);
      mod->f_module = NULL;
#if EDG_WIN32
      close_mapped_input_file(mod->mapped_input, mod->map_object);
      mod->mapped_input = NULL;
      mod->map_object = NULL;
#endif /* EDG_WIN32 */
    }  /* if */
  }  /* if */
}  /* close_ifc_module_file */


void get_definition_of_module_class_from_ifc(a_module_entity_ptr mep,
                                             a_text_buffer       *buffer)
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
  read_ifc_partition_at_offset(mod, mep->variant.ifc_partition,
                               mep->file_offset);
  idssp = get_DeclSort_Scope(&idss);
  str_ifc_class_definition(idssp, &scb);
  add_char_to_text_buffer(buffer, ';');
}  /* get_definition_of_module_class_from_ifc */


void ifc_modules_pch_reset(a_module_import_decl_ptr midp)
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


a_boolean an_ifc_module::import(a_module_import_decl_ptr midp) noexcept
/*
Import an IFC module file described by midp.
*/
{
  check_assertion(midp->module_info->kind == mk_ifc);
  return import_ifc_module_file(midp);
}  /* import */


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
