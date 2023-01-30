/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2017-2022 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

ifc_modules.c -- Microsoft-specific IFC module code

*/

#include "basic_hdrs.h"
#include "fe_common.h"
#include "ifc_modules.h"
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ifc_map_functions.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#include "class_decl.h"
#include "decl_spec.h"
#include "exprutil.h"
#include "func_def.h"
#include "literals.h"
#include "pch.h"
#include "symbol_ref.h"
#include "macro.h"
#include "interpret.h"
#include "folding.h"

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
                        /* A text buffer used for processing file names from
                           the IFC. */

namespace {

struct a_module_entity_stack_state;

a_module_entity_stack_state
                *curr_mep_state = NULL;
                        /* A global stack of module entity pointers currently
                           being processed.  This stack can be printed with
                           db_mep_stack(). */

/*
A class used to represent an element on the module entity state stack.  This is
an RAII object that automatically manages the value of curr_mep_state for the
module given during construction.
*/
struct a_module_entity_stack_state {
  a_module_entity_stack_state(a_module_entity_ptr mep_val)
    : parent(curr_mep_state), mep(mep_val)
    { curr_mep_state = this; }

  ~a_module_entity_stack_state()
    { curr_mep_state = this->parent; }

  void invalidate()
    { this->mep->invalid = TRUE; }

  a_module_entity_stack_state
                *parent;
                        /* The parent (old) module entity stack state, prior
                           to this object's construction. */
  a_module_entity_ptr
                mep;
                        /* The current module entity pointer. */
};  /* a_module_entity_stack_state */

}  /* namespace */

static void ifc_requirement_impl(ARG_UNUSED int          line_number,
                                 ARG_UNUSED a_const_char *function,
                                 an_ifc_module           *mod,
                                 a_boolean               condition,
                                 ARG_UNUSED a_const_char *string)
/*
This function takes the line number and the name of the function where the
failure occurred, an associated module, a condition to check, and a string
describing the condition being checked.  If the given condition is FALSE, the
processing of any associated module entity pointer will be failed, the TU
processing will be failed, and an error will be emitted.

The caller is responsible for ensuring there is at least one module entity
stack state pushed.

If there's any risk of the input data triggering the condition failure, and the
front end can reasonably recover/continue, this function should be used in
place of check_assertion.  This is particularly important for incomplete
features where the front end partially implements the IFC spec for the given
node, but needs to catch cases that are not yet implemented (ensuring the TU
does not compile with a possibly-misinterpreted IFC import).
*/
{
  /* Checking a condition without a module entity pointer state.  The caller
     did not set up the module entity state stack properly. */
  check_assertion(curr_mep_state != NULL);
  if (!condition) {
    a_diagnostic_ptr diag = pos_st_start_error(ec_ifc_requirement_failure,
                                               &null_source_position,
                                               mod->assoc_module_info->name);

#if DEBUG
    num_st2_add_diag_info(diag, ec_ifc_requirement_failure_fill_in,
                          line_number, function, string);
#endif /* DEBUG */
    end_diagnostic(diag);
    curr_mep_state->invalidate();
  }  /* if */
}  /* ifc_requirement_impl */


template<template<typename> class Allocator>
static void ifc_requirement_impl(int                               line_number,
                                 a_const_char                      *function,
                                 an_ifc_module                     *mod,
                                 a_boolean                         condition,
                                 const Allocated_string<Allocator> &string)
/*
An overload of ifc_requirement_impl to allow use of Allocated_string for the
string argument.
*/
{
  ifc_requirement_impl(line_number, function, mod, condition,
                       string.as_temp_characters());
}  /* ifc_requirement_impl */


#define ifc_requirement(mod, condition, string)                         \
  ifc_requirement_impl(__LINE__, __EDG_func__,                          \
                       mod, condition, string)

#define ifc_unexpected(mod, string)                                     \
  ifc_requirement_impl(__LINE__, __EDG_func__,                          \
                       mod, FALSE, string)

static a_boolean source_position_from_locus(
                                           a_source_position            *pos,
                                           const an_ifc_source_location &locus)
/*
Map the IFC locus source position information into the source position at pos.
Return TRUE if processing succeeded, otherwise return FALSE.
*/
{
  a_boolean                   result = TRUE;
  Opt<an_ifc_source_line>     opt_isl;
  an_ifc_module               *mod = locus.get_module();
  an_ifc_line_index           line_number = get_ifc_line(locus);
  an_ifc_partition_kind_index src_line_idx{mod, ifc_pk_src_line, line_number};

  /* FIXME: This should be addressed in the validator. */
  if (!validate_element_exists(mod, ifc_pk_src_line, line_number,
                               /*trace=*/NULL)) {
    a_string err_msg("bad source line index ", (an_ifc_index_type)line_number);

    ifc_unexpected(mod, err_msg.as_temp_characters());
    goto done;
  }  /* if */
  construct_node(&opt_isl, src_line_idx);
  if (opt_isl.has_value()) {
    an_ifc_source_line isl = *opt_isl;
    an_ifc_name_index  file = get_ifc_file(isl);
    an_ifc_line_number line = get_ifc_line(isl);

    check_assertion(file.sort == ifc_ns_name_source_file);
    if (line == 0) {
      /* IFC LineNumbers start at one, a line number of zero means the source
         line isn't known.  As the front end (at the time of writing) doesn't
         support source locations for a file without a line, resolve all cases
         of this to null source position. */
      *pos = null_source_position;
    } else {
      /* See if this file has been used before. */
      an_ifc_module::a_module_sequence_number_mapping *msnmp =
                                            &mod->sequence_numbers[file.value];

      if (msnmp->starting_sequence_number == 0) {
        /* First time accessing this source file; record the start of a new
           source file.  Note that this may be out-of-order as it depends on
           the order that entities are used, but the full tree of source file
           references isn't available in the IFC file. */
        a_const_char *file_name;
        reset_text_buffer(file_name_buffer);
        file_name = mod->string_from_name_index(file, (a_symbol_locator *)NULL,
                                                &file_name_buffer);
        file_name = copy_string_to_region(FILE_SCOPE_REGION_NUMBER, file_name);
        record_inclusion_of_module_source_file(file_name, pos,
                                               mod->assoc_module_info,
                                               msnmp->max_line_number);
        msnmp->starting_sequence_number = pos->seq;
      }  /* if */
      check_assertion(line <= msnmp->max_line_number);
      pos->seq = msnmp->starting_sequence_number + line;
      /* Add one to map 0-based IFC column numbers to 1-based EDG numbers. */
      pos->column = get_ifc_column(locus) + 1;
#if FULLY_RESOLVED_MACRO_POSITIONS
      pos->orig_seq = pos->seq;
      pos->orig_column = pos->column;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
    }  /* if */
  }  /* if */
done:
  return result;
}  /* source_position_from_locus */

namespace {

/*
An RAII type representing a source position hint formed from an IFC locus.
This type ensures that the token cache does not hold onto a position hint
beyond the lifetime of the hint.
*/
struct an_ifc_source_position_hint {
  inline an_ifc_source_position_hint(a_module_token_cache_ptr     cache,
                                     const an_ifc_source_location &locus);
  template<typename an_ifc_Index_type>
  inline an_ifc_source_position_hint(a_module_token_cache_ptr cache,
                                     an_ifc_Index_type        idx);
  inline ~an_ifc_source_position_hint();
  a_source_position_ptr as_pos()
    { return &this->pos; }
private:
  a_module_token_cache_ptr
                cache_ptr;
                        /* A pointer to the cache that's being provided with
                           this hint. */
  a_source_position
                pos;    /* The storage for the source position. */
  a_boolean     hint_given;
                        /* TRUE if the stored source position was ever
                           actually given to the cache as a hint. */
};  /* an_ifc_source_poisiton_hint */


an_ifc_source_position_hint::an_ifc_source_position_hint(
                                           a_module_token_cache_ptr     cache,
                                           const an_ifc_source_location &locus)
/*
Create a position hint for the given cache from the given locus.  The position
hint will last until it's consumed during token caching, or until the lifetime
of this object expires (whichever is sooner).
*/
: cache_ptr(cache), pos(), hint_given(TRUE)
{
  source_position_from_locus(&this->pos, locus);
  this->cache_ptr->set_position_hint(&this->pos);
}  /* an_ifc_source_position_hint::an_ifc_source_position_hint */


template<typename an_ifc_Index_type>
an_ifc_source_position_hint::an_ifc_source_position_hint(
                                                a_module_token_cache_ptr cache,
                                                an_ifc_Index_type        idx)
/*
Create a position hint for the given cache from the given index.  If the index
has no associated locus, the null_source_position will instead be used.  The
position hint will last until it's consumed during token caching, or until the
lifetime of this object expires (whichever is sooner).
*/
: cache_ptr(cache), pos(null_source_position), hint_given(TRUE)
{
  if (!is_null_index(idx) && validate(idx) && has_ifc_locus(idx)) {
    an_ifc_source_location locus = get_ifc_locus(idx);

    source_position_from_locus(&this->pos, locus);
    if (cmp_source_positions(this->pos, null_source_position) != 0) {
      this->cache_ptr->set_position_hint(&this->pos);
    } else {
      /* The IFC gave us a null source location locus value, emit a remark. */
      a_diagnostic_ptr            diag = pos_st_start_diagnostic(
                                           es_remark,
                                           ec_ifc_null_source_position_ignored,
                                           &pos,
                                           idx.mod->assoc_module_info->name);
      an_ifc_partition_kind       kind = to_partition_kind(idx.sort);
      an_ifc_partition_kind_index part_kind_idx = {idx.mod, kind, idx.value};
      an_ifc_partition_metadata   *part_meta =
                                         get_partition_metadata(part_kind_idx);
      size_t                      part_start = part_meta->offset;
      size_t                      abs_offset =
                                           get_partition_offset(part_kind_idx);
      size_t                      rel_offset = abs_offset - part_start;

      /* FIXME: Migrate to allowing diagnostics with size_t. */
      st_num3_add_diag_info(diag, ec_ifc_null_source_position_ignored_info,
                            get_partition_name_from_kind(kind), idx.value,
                            (uint32_t)abs_offset, (uint32_t)rel_offset);
      end_diagnostic(diag);
      hint_given = FALSE;
    }  /* if */
  } else {
    hint_given = FALSE;
  }  /* if */
}  /* an_ifc_source_position_hint::an_ifc_source_position_hint */


an_ifc_source_position_hint::~an_ifc_source_position_hint()
/*
Cleanup the module token cache's source position hint if it's still set to
the source position managed by this object.
*/
{
  if (hint_given && this->cache_ptr->get_position_hint() == &this->pos) {
    this->cache_ptr->set_position_hint(NULL);
  }  /* if */
}  /* an_ifc_source_position_hint::an_ifc_source_position_hint */

}  /* namespace */

static inline void cache_token(a_module_token_cache_ptr cache,
                               a_token_kind             tok,
                               a_source_position_ptr    pos = NULL)
/*
This function proxies calls to the common cache_token function when using a
module token cache.

If a position is passed it will be used as the source position; otherwise the
position will be inferred (see infer_next_source_position for details about the
rules of position inference).
*/
{
  pos = infer_next_source_position(cache, pos);
  cache_token(cache->as_canonical(), tok, pos);
}  /* cache_token */


static void cache_resolved_type_token(a_module_token_cache_ptr cache,
                                      a_type_ptr               type,
                                      a_source_position_ptr    pos = NULL)
/*
This function proxies calls to the common cache_resolved_type_token function
when using a module token cache.

If a position is passed it will be used as the source position; otherwise the
position will be inferred (see infer_next_source_position for details about the
rules of position inference).
*/
{
  pos = infer_next_source_position(cache, pos);
  cache_resolved_type_token(cache->as_canonical(), type, pos);
}  /* cache_resolved_type_token */


static void cache_tokens_from_string(a_const_char             *str,
                                     a_module_token_cache_ptr cache,
                                     a_source_position_ptr    pos = NULL)
/*
This function proxies calls to the common cache_tokens_from_string function
when using a module token cache.

If a position is passed it will be used as the source position; otherwise the
position will be inferred (see infer_next_source_position for details about the
rules of position inference).
*/
{
  pos = infer_next_source_position(cache, pos);
  cache_tokens_from_string(str, cache->as_canonical(), pos);
}  /* cache_tokens_from_string */


static void cache_identifier(a_module_token_cache_ptr cache,
                             a_const_char             *name,
                             a_source_position_ptr    pos = NULL);

/*
The routines and data structures below are used to support host-independent
access to the fields of an IFC file regardless of endianness, padding, or
alignment issues.
*/

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

void init_byte_buffer(an_ifc_module     *mod,
                      size_t            offset,
                      ARG_UNUSED size_t length)
/*
Initialize the state information used by "get_bytes", etc.  mod is the module
owning the byte buffer.  offset is the offset from the start of the memory
mapped region to be read.  length is its size, in bytes.
*/
{
  mod->byte_buffer = (unsigned char*)mod->mmap_addr + offset;
  mod->buffer_end = mod->byte_buffer + length - 1;
}  /* init_byte_buffer */


static void get_bytes_from_buffer(an_ifc_module *mod,
                                  void          *entity,
                                  size_t        length)
/*
Fetch a block of bytes from the IFC file, and check for reading past the end of
the buffer.
*/
{
  /* Check for fetching too many bytes. */
  if (((unsigned char*)mod->byte_buffer + length - 1) > mod->buffer_end) {
    (void)buffer_overrun();
  }  /* if */
  memcpy((a_byte*)entity, mod->byte_buffer, length);
  mod->byte_buffer += length;
}  /* get_bytes_from_buffer */


/*
Macro to fetch a single byte from the IFC file.
*/
#define get_byte(mod, byte)                                                   \
  (*((unsigned char*)(byte)) = (mod->byte_buffer <= mod->buffer_end ?         \
                                     *(mod->byte_buffer)++ : buffer_overrun()))

#else /* !USE_MMAP_FOR_MEMORY_REGIONS */

void init_byte_buffer(an_ifc_module     *mod,
                      size_t            offset,
                      ARG_UNUSED size_t length)
/*
Initialize the state information used by "get_bytes", etc.  mod is the module
owning the byte buffer. offset is the offset from the start of the module file
to be read.  length is its size, in bytes.
*/
{
  fseek(mod->f_module, offset, SEEK_SET);
}  /* init_byte_buffer */


static void get_bytes_from_buffer(an_ifc_module *mod,
                                  void          *entity,
                                  size_t        length)
/*
Fetch a block of bytes from the IFC file, and check for reading past the end of
the buffer.
*/
{
  /* Check for fetching too many bytes. */
  if (fread(entity, 1, length, mod->f_module) != length) {
    (void)buffer_overrun();
  }  /* if */
}  /* get_bytes_from_buffer */


/*
Macro to fetch a single byte.
*/
#define get_byte(mod, byte)                                                   \
  get_bytes_from_buffer(mod, byte, 1)

#endif /* USE_MMAP_FOR_MEMORY_REGIONS */

static void get_mismatched_endian_bytes(an_ifc_module *mod,
                                        void          *entity,
                                        size_t        length)
/*
Get length bytes from the IFC file and convert them to the host byte order.
This routine is used only when the host byte order does not match the byte
order for the entity being read.
*/
{
  unsigned char *ptr;

  /* Get the bytes in reverse order into "entity". */
  for (ptr = (unsigned char*)entity + length - 1; length > 0;
       length--, ptr--) {
    get_byte(mod, ptr);
  }  /* for */
}  /* get_mismatched_endian_bytes */


void get_bytes(an_ifc_module *mod,
               void          *entity,
               size_t        length,
               a_boolean     header_bytes)
/*
Get length bytes from the IFC file.  If there's an endian mismatch between
what's being read and the host, convert the bytes to the host byte order.  If
header_bytes is TRUE, the bytes being retrieved correspond to the IFC file
header or table of contents (and are therefore known to be little-endian).
*/
{
  if (has_matching_endianness(mod) || (header_bytes && host_little_endian)) {
    get_bytes_from_buffer(mod, entity, length);
  } else {
    get_mismatched_endian_bytes(mod, entity, length);
  }  /* if */
}  /* get_bytes */


a_boolean is_at_least(an_ifc_module          *mod,
                      an_ifc_version_storage minimum_version_major,
                      an_ifc_version_storage minimum_version_minor)
/*
Check to see if the given module's version has at least the minimum version
"major.minor".
*/
{
  a_boolean result;

  if (mod->version_major > minimum_version_major) {
    result = TRUE;
  } else if (mod->version_major == minimum_version_major &&
             mod->version_minor >= minimum_version_minor) {
    result = TRUE;
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_at_least */


a_boolean has_matching_endianness(an_ifc_module *mod)
/*
Check to see if the given module's endianness matches the endianness the
front end was compiled under.
*/
{
  a_boolean result;

#if ASSUME_LITTLE_ENDIAN_IFC_MODULES
  result = TRUE;
#else /* !ASSUME_LITTLE_ENDIAN_IFC_MODULES */
  switch (get_ifc_arch(mod->header)) {
    /* FIXME: In this code ARM is assumed to be a little endian architecture
       for IFC purposes, this may not be correct.  We should get
       clarification. */
    case ifc_as_arm32:
    case ifc_as_arm64:
    case ifc_as_hybrid_x86_arm64:
    case ifc_as_x64:
    case ifc_as_x86:
      /* If the host is little endian then for purposes of IFC processing, the
         byte orders match based on the above architecture's endianness. */
      result = host_little_endian;
      break;
    case ifc_as_unknown:
      /* Make a best guess based on the compiler's target. */
      result = targ_little_endian == host_little_endian;
      break;
    default_is_unexpected();
  }  /* switch */
#endif /* ASSUME_LITTLE_ENDIAN_IFC_MODULES */
  return result;
}  /* has_matching_endianness */


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

an_ifc_partition_metadata &an_ifc_module::get_partition_metadata(
                                               an_ifc_partition_kind part_kind)
/*
Given a partition kind, return a reference to the corresponding metadata entry.
*/
{
  /* Subtract 1 as ifc_pk_none isn't included in the partition metadata map. */
  check_assertion(part_kind != ifc_pk_none);
  return this->partitions[part_kind - 1];
}  /* get_partition_metadata */


const an_ifc_partition_metadata &an_ifc_module::get_partition_metadata(
                                               an_ifc_partition_kind part_kind)
                                                                          const
/*
Given a partition kind, return a reference to the corresponding metadata entry.
*/
{
  /* Delegate to the non-const implementation. */
  return const_cast<an_ifc_module*>(this)->get_partition_metadata(part_kind);
}  /* get_partition_metadata */


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


static an_ifc_index_type to_partition_index(an_ifc_module         *mod,
                                            an_ifc_partition_kind partition,
                                            size_t                file_offset)
/*
Give a partition kind and an offset into the give module's file for an element,
return the respective partition index.
*/
{
  /* Compute the index into the partition by first subtracting the start of the
     partition in the file, producing "part_offset".  Then compute the index
     into the partition by dividing the offset by the size of entries in the
     partition. */
  an_ifc_partition_metadata &part_meta =
                                       mod->get_partition_metadata(partition);
  size_t                    part_offset = file_offset - part_meta.offset;

  return (an_ifc_index_type)(part_offset / part_meta.entry_size);
}  /* to_partition_index */


static a_module_entity_ptr get_ifc_module_entity_ptr(
                                               an_ifc_module         *mod,
                                               an_ifc_partition_kind partition,
                                               an_ifc_index_type     index)
/*
Utility to return a module entity pointer for the given module, IFC partition,
and index into that partition.  For cases where the module entity has just
been created, the partition is set according to the partition supplied by the
caller.
*/
{
  an_ifc_partition_kind_index element_idx{mod, partition, index};
  a_module_entity_ptr         mep = get_module_entity_ptr(
                                            mod->assoc_module_info,
                                            get_partition_offset(element_idx));

  /* FIXME: Handle error case, formatting. */
  if (mep->variant.ifc_partition == ifc_pk_none) {
    mep->variant.ifc_partition = partition;
  } else {
    check_assertion(mep->variant.ifc_partition == partition);
  }  /* if */
  return mep;
}  /* get_ifc_module_entity_ptr */


template<typename an_ifc_Index_type>
static inline a_module_entity_ptr
get_ifc_module_entity_ptr(an_ifc_Index_type index)
/*
Overload wrapper for "get_ifc_module_entity_ptr" that extracts the type sort
and index from the provided index.
*/
{
  return get_ifc_module_entity_ptr(index.mod, get_partition_kind(index),
                                   index.value);
}  /* get_ifc_module_entity_ptr */


static inline an_ifc_decl_index
decl_index_of(an_ifc_module         *mod,
              an_ifc_partition_kind partition,
              size_t                file_offset)
/*
Return the an_ifc_decl_index in the given module, derived from the partition
kind and file offset.
*/
{
  an_ifc_index_type part_index = to_partition_index(mod, partition,
                                                    file_offset);

  return an_ifc_decl_index{mod, to_decl_sort(partition), part_index};
}  /* decl_index_of */


static inline an_ifc_decl_index decl_index_of(a_module_entity_ptr mep)
/*
Return the an_ifc_decl_index derived from the partition kind and file offset
stored on the given module entity pointer.
*/
{
  an_ifc_module *mod = get_assoc_ifc_module(mep);

  return decl_index_of(mod, mep->variant.ifc_partition, mep->file_offset);
}  /* decl_index_of */


static inline an_ifc_type_index
type_index_of(an_ifc_module         *mod,
              an_ifc_partition_kind partition,
              size_t                file_offset)
/*
Return the an_ifc_type_index in the given module, derived from the partition
kind and file offset.
*/
{
  an_ifc_index_type part_index = to_partition_index(mod, partition,
                                                    file_offset);

  return an_ifc_type_index{mod, to_type_sort(partition), part_index};
}  /* type_index_of */


static inline an_ifc_type_index type_index_of(a_module_entity_ptr mep)
/*
Return the an_ifc_type_index derived from the partition kind and file offset
stored on the given module entity pointer.
*/
{
  an_ifc_module *mod = get_assoc_ifc_module(mep);

  return type_index_of(mod, mep->variant.ifc_partition, mep->file_offset);
}  /* type_index_of */


a_const_char *get_partition_name_from_kind(an_ifc_partition_kind part_kind)
/*
Given a partition kind that corresponds to a real partition, return the
corresponding name.
*/
{
  /* Subtract 1 to ignore pk_none. */
  static_assert(ifc_pk_none == 0,
                "pk_none does not hold the expected value");
  check_assertion(part_kind != ifc_pk_none);

  an_ifc_partition_map *map_entry = &ifc_partition_map[part_kind - 1];
  /* Make sure the right map entry is going to be returned. */
  check_assertion(map_entry->kind == part_kind);
  return map_entry->name;
}  /* get_partition_name_from_kind */


static a_module_ref_key as_key(const an_ifc_module_reference &ref)
/*
Given a module reference, return the associated module ref key (i.e., a hash
key).
*/
{
  an_ifc_text_offset_storage partition = get_ifc_partition(ref);
  an_ifc_text_offset_storage owner = get_ifc_owner(ref);

  static_assert((sizeof(a_module_ref_key) >=
                 (sizeof(an_ifc_text_offset_storage) * 2)),
                "Key is not large enough");
  return (((a_module_ref_key)partition) << (sizeof(owner) * CHAR_BIT)) |
                                                       (a_module_ref_key)owner;
}  /* as_key */


static a_module_import_decl_ptr transitive_import_module(
                                            const an_ifc_module_reference &ref)
/*
Given a module reference, import the referenced module.
*/
{
  an_ifc_module            *mod = ref.get_module();
  a_module_ref_key         ref_key = as_key(ref);
  a_module_import_decl_ptr midp;
  a_const_char             *prim_name, *part_name;

  midp = mod->referenced_modules.get(ref_key);
  if (midp != NULL) {
    /* Already imported this reference. */
  } else {
    an_ifc_text_offset owner = get_ifc_owner(ref);
    an_ifc_text_offset partition = get_ifc_partition(ref);

    midp = alloc_module_import_decl();
    mod->referenced_modules.map(ref_key, midp);
    midp->module_name_position = null_source_position;
    prim_name = owner != 0 ? mod->get_string_at_offset(owner) : NULL;
    part_name = partition != 0 ? mod->get_string_at_offset(partition) : NULL;
    if (prim_name == NULL) {
      /* This is a header unit. */
      check_assertion(part_name != NULL);
      midp->module_info = alloc_module((a_module_kind)mk_header);
      midp->module_info->name = copy_string_to_region(FILE_SCOPE_REGION_NUMBER,
                                                      part_name);
      /* A non-header-unit module cannot leak macros, but may transitively
         import a header unit.  Ensure that the transitive import does not leak
         macro definitions. */
      if (mod->assoc_module_info->suppress_macro_export ||
          !is_header_unit(mod->assoc_module_info)) {
        midp->module_info->suppress_macro_export = TRUE;
      }  /* if */
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


a_boolean check_module(const an_ifc_module_reference &ref)
/*
Given a module reference, check that the referenced module can be located and
has a valid an_ifc_module handler.  Return TRUE if the module reference
satisfies these requirements; otherwise, return FALSE.
*/
{
  /* FIXME: There's *definitely* more to be done here, and in reality
     it's unlikely this "works" on any level; this function needs a lot
     of work. */
  return transitive_import_module(ref) != NULL;
}  /* check_module */


an_ifc_module* get_module(const an_ifc_module_reference &ref)
/*
Load and return the an_ifc_module handler for the referenced module.
*/
{
  a_module_import_decl_ptr midp = transitive_import_module(ref);
  check_assertion(midp != NULL);
  return (an_ifc_module*)(midp->module_info->module_interface);
}  /* get_module */


static inline uintptr_t hash_ptr(an_ifc_decl_index  idx)
/*
Return a hash value for the given IFC declaration index.
*/
{
  uintptr_t  result = 17*31 + hash_ptr((void*)idx.mod);

  result = result*31 + (uintptr_t)idx.sort;
  result = result*31 + (uintptr_t)idx.value;
  return result;
}  /* hash_ptr */

namespace {

template<typename an_ifc_Node_type>
struct a_Sequence_traverser;

/*
A struct representing a traversed node's value and index.
*/
template<typename an_ifc_Node_type>
struct an_Indexed {
  inline an_Indexed(an_ifc_module *mod, an_ifc_index_type idx);
  a_boolean has_value() const
    { return node_value.has_value(); }
  const an_ifc_Node_type &operator*() const
    { return *node_value; }
  Opt<an_ifc_Node_type>
                  node_value;
                          /* The traversed node's value (if any). */
  an_ifc_partition_kind_index
                  node_idx;
                          /* The traversed node's index information. */
};  /* an_Indexed */


template<typename an_ifc_Node_type>
an_Indexed<an_ifc_Node_type>::an_Indexed(an_ifc_module     *mod,
                                         an_ifc_index_type idx)
  : node_value(),
    node_idx{mod, get_ifc_partition_kind<an_ifc_Node_type>(), idx}
/*
Construct an indexed representation of the node of the corresponding type at
the given index.
*/
{
  construct_node(&this->node_value, this->node_idx);
}  /* an_Indexed */


/*
An internal iterator type for a_Sequence_traverser.  This type should only be
constructed after its index has been validated to point to a (possibly
invalid, but still present) node.
*/
template<typename an_ifc_Node_type>
struct a_Sequence_traversal_iterator {
  a_Sequence_traversal_iterator()
    : mod(NULL), index(0)
    {}

  inline a_Sequence_traversal_iterator<an_ifc_Node_type> operator++();
  inline an_Indexed<an_ifc_Node_type> operator*() const;

  a_boolean
  operator==(const a_Sequence_traversal_iterator<an_ifc_Node_type>& other)
                                                                          const
    { return this->mod == other.mod && this->index == other.index; }
  a_boolean
  operator!=(const a_Sequence_traversal_iterator<an_ifc_Node_type>& other)
                                                                          const
    { return !(*this == other); }
private:
  a_Sequence_traversal_iterator(an_ifc_module     *mod_val,
                                an_ifc_index_type index_val)
    : mod(mod_val), index(index_val)
    {}

  an_ifc_module *mod;   /* The module owning the sequence. */
  an_ifc_index_type
                index;  /* The current position in the sequence. */
  friend a_Sequence_traverser<an_ifc_Node_type>;
};  /* a_sequence_traversal_iterator */


template<typename an_ifc_Node_type>
inline a_Sequence_traversal_iterator<an_ifc_Node_type>
a_Sequence_traversal_iterator<an_ifc_Node_type>::operator++()
/*
Increment the current iterator and return the iterator state.
*/
{
  ++this->index;
  return *this;
}  /* operator++ */


template<typename an_ifc_Node_type>
inline an_Indexed<an_ifc_Node_type>
a_Sequence_traversal_iterator<an_ifc_Node_type>::operator*() const
/*
Return the current iterator value.
*/
{
  return an_Indexed<an_ifc_Node_type>(this->mod, this->index);
}  /* operator* */


/*
A structure for quickly implementing validated IFC sequence traversals.
*/
template<typename an_ifc_Node_type>
struct a_Sequence_traverser {
  a_Sequence_traverser(an_ifc_module              *mod_val,
                       an_ifc_index_type          start_val,
                       an_ifc_cardinality_storage cardinality_val);
  a_Sequence_traverser(an_ifc_module              *mod_val,
                       an_ifc_index_type          start_val);
  template<typename an_ifc_Traversal_node_type>
  a_Sequence_traverser(const an_ifc_Traversal_node_type &node,
                       an_ifc_index_type                offset = 0)
    : a_Sequence_traverser(node.get_module(), get_ifc_start(node) + offset,
                           get_ifc_cardinality(node) - offset)
    { check_assertion(get_ifc_cardinality(node) >= offset); }
  inline a_Sequence_traversal_iterator<an_ifc_Node_type> begin() const;
  inline a_Sequence_traversal_iterator<an_ifc_Node_type> end() const;

  an_ifc_index_type get_start_index() const
    { return start; }
  an_ifc_index_type get_end_index() const
    { return start + cardinality; }
private:
  an_ifc_module *mod;   /* The module owning the sequence. */
  an_ifc_index_type
                start;  /* The start of the sequence. */
  an_ifc_cardinality_storage
                cardinality;
                        /* The number of elements in the sequence. */
};  /* a_Sequence_traverser */


template<typename an_ifc_Node_type>
a_Sequence_traverser<an_ifc_Node_type>::a_Sequence_traverser(
                                    an_ifc_module              *mod_val,
                                    an_ifc_index_type          start_val,
                                    an_ifc_cardinality_storage cardinality_val)
    : mod(mod_val), start(start_val), cardinality(cardinality_val)
/*
Construct a sequence traversal object that will traverse the partition
associated with an_ifc_Node_type in the given module from start_val through
start_val + cardinality_val (exclusive).
*/
{
  if (this->cardinality > 0) {
    an_ifc_partition_kind part_kind =
                                    get_ifc_partition_kind<an_ifc_Node_type>();
    an_ifc_index_type     last = this->start + this->cardinality - 1;

    /* Check to see if the last element exists.  This effectively validates the
       full range of IFC values defined by this sequence in 1 step (as the
       presence of N implies N-1 exists). */
    if (!validate_element_exists(this->mod, part_kind, last, /*trace=*/NULL)) {
      this->cardinality = 0;
    }  /* if */
  }  /* if */
}  /* a_Sequence_traverser */


template<typename an_ifc_Node_type>
a_Sequence_traverser<an_ifc_Node_type>::a_Sequence_traverser(
                                    an_ifc_module              *mod_val,
                                    an_ifc_index_type          start_val)
    : mod(mod_val), start(start_val),
      cardinality(
             mod->get_num_entries(get_ifc_partition_kind<an_ifc_Node_type>()) -
             start_val)
/*
Construct a sequence traversal object that will traverse the partition
associated with an_ifc_Node_type in the given module from start_val through
the end of the partition.
*/
{
}  /* a_Sequence_traverser */


template<typename an_ifc_Node_type>
inline a_Sequence_traversal_iterator<an_ifc_Node_type>
a_Sequence_traverser<an_ifc_Node_type>::begin() const
/*
Return an iterator to the start of the sequence, or an invalid iterator if the
sequence is not valid.
*/
{
  a_Sequence_traversal_iterator<an_ifc_Node_type> result;

  if (this->cardinality != 0) {
    result = {mod, start};
  }  /* if */
  return result;
}  /* begin */


template<typename an_ifc_Node_type>
inline a_Sequence_traversal_iterator<an_ifc_Node_type>
a_Sequence_traverser<an_ifc_Node_type>::end() const
/*
Return an iterator to the end of the sequence, or an invalid iterator if the
sequence is not valid.
*/
{
  a_Sequence_traversal_iterator<an_ifc_Node_type> result;

  if (this->cardinality != 0) {
    result = {mod, start + cardinality};
  }  /* if */
  return result;
}  /* end */


template<typename an_ifc_Node_type>
inline an_ifc_index_type
get_relative_index(const a_Sequence_traverser<an_ifc_Node_type> &traverser,
                   const an_Indexed<an_ifc_Node_type>           &indexed_value)
/*
Given a traverser and a derived indexed value, return the relative index of the
indexed value from the start of the traverser.
*/
{
  an_ifc_index_type start_idx = traverser.get_start_index();
  an_ifc_index_type node_idx = indexed_value.node_idx.value;

#if EXPENSIVE_CHECKING
  check_assertion(node_idx >= start_idx);
#endif /* EXPENSIVE_CHECKING */
  return node_idx - start_idx;
}  /* get_relative_index */


template<typename an_ifc_Node_type>
inline a_boolean
is_first(const a_Sequence_traverser<an_ifc_Node_type> &traverser,
         const an_Indexed<an_ifc_Node_type>           &indexed_value)
/*
Given a traverser and a derived indexed value, return TRUE if the indexed value
is the first value; otherwise, return FALSE.
*/
{
  an_ifc_index_type rel_idx = get_relative_index(traverser, indexed_value);

  return rel_idx == 0;
}  /* is_first */

}  /* namespace */

/* Convenience aliases for a_Sequence_traverser. */
using a_attr_heap_traverser = a_Sequence_traverser<an_ifc_heap_attr>;
using a_decl_enumerator_traverser =
                                  a_Sequence_traverser<an_ifc_decl_enumerator>;
using a_decl_heap_traverser = a_Sequence_traverser<an_ifc_heap_decl>;
using a_decl_parameter_traverser = a_Sequence_traverser<an_ifc_decl_parameter>;
using a_decl_partial_specialization_traverser =
                      a_Sequence_traverser<an_ifc_decl_partial_specialization>;
using a_decl_specialization_traverser =
                              a_Sequence_traverser<an_ifc_decl_specialization>;
using a_decl_temploid_traverser = a_Sequence_traverser<an_ifc_decl_temploid>;
using an_expr_heap_traverser = a_Sequence_traverser<an_ifc_heap_expr>;
using a_pp_heap_traverser = a_Sequence_traverser<an_ifc_heap_pp_form>;
using a_scope_member_traverser = a_Sequence_traverser<an_ifc_scope_member>;
using a_source_word_traverser = a_Sequence_traverser<an_ifc_source_word>;
using a_stmt_heap_traverser = a_Sequence_traverser<an_ifc_heap_stmt>;
using a_syntax_heap_traverser = a_Sequence_traverser<an_ifc_heap_syntax>;
using a_type_heap_traverser = a_Sequence_traverser<an_ifc_heap_type>;

using an_ifc_decl_lookup_table = Ptr_map<an_ifc_decl_index, a_symbol_ptr>;
			/* The type of a table that maps IFC declaration
			   indices to corresponding front end symbols. */


static an_ifc_decl_lookup_table
		*ifc_decl_lookup_table;
			/* A hash table to map IFC declaration indices to
			   corresponding front end symbols. */


template<typename an_ifc_Node_type>
static void find_trait(Opt<an_ifc_Node_type> *result,
                       an_ifc_decl_index     decl)
/*
Given the declaration index (decl) to use as a trait table key (for the trait
table associated with the given trait type), find and return the associated
trait as an optional.  If the returned optional is empty the trait either
wasn't found (because it doesn't exist), or a diagnosed validation error
occurred.
*/
{
  /* If this check fails, the validator needs additional validation to prevent
     a required IFC field from being 0 (i.e., "NULL"), or there's a logic
     bug. */
  check_assertion(!is_null_index(decl));
  an_ifc_partition_kind trait_part_kind =
                                    get_ifc_partition_kind<an_ifc_Node_type>();
  an_ifc_module         *mod = decl.mod;
  size_t                num_traits = mod->get_num_entries(trait_part_kind);
  /* Provide a value function for retrieving the trait at the given trait
     partition index. */
  auto                  value_lambda = [mod, trait_part_kind](ptrdiff_t idx) {
    an_ifc_partition_kind_index part_idx{mod, trait_part_kind,
                                         (an_ifc_index_type)idx};
    an_ifc_Node_type            trait;

    /* As the binary search used by this value is only doing comparisons (not
       attempting to operate on the returned DeclIndex) and the result will
       be fully validated anyways, to improve performance construct the node
       unchecked. */
    construct_node_unchecked(&trait, part_idx);
    return get_ifc_encoded_decl(trait);
  };
  /* Get the partition index (if any) for decl. */
  ptrdiff_t             partition_idx = bin_search(num_traits,
                                                   to_encoded(mod, decl),
                                                   value_lambda);

  if (partition_idx != -1) {
    an_ifc_partition_kind_index part_idx{mod, trait_part_kind,
                                         (an_ifc_index_type)partition_idx};

    /* A trait was found for decl.  Load the trait (again) to retrieve the
       trait.

       Note that the implementation of bin_search at the time of writing does
       not guarantee that the last read value is the one whose index is
       returned.  Thus, we cannot (as an optimization) share a variable with
       the value_lambda to prevent double reading (though this is unlikely to
       ever represent a significant cost in terms of CPU time). */
    construct_node(result, part_idx);
  }  /* if */
}  /* find_trait */


static inline an_ifc_attr_index attr_index_of(an_ifc_decl_index decl_idx)
/*
Search the ".msvc.trait.vendor-traits" partition for any attribute associated
with a given ifc_DeclIndex (decl_idx).  If a matching attribute is found return
its ifc_AttrIndex.
*/
{
  an_ifc_attr_index                 result = {};
  Opt<an_ifc_trait_msvc_decl_attrs> opt_itmda;

  find_trait(&opt_itmda, decl_idx);
  if (opt_itmda.has_value()) {
    result = get_ifc_trait(*opt_itmda);
  }  /* if */
  return result;
}  /* attr_index_of */


static a_boolean is_class_scope(const an_ifc_decl_index scope_ref)
/*
Return TRUE if the provided scope is a class/struct/union scope, FALSE
otherwise.
*/
{
  a_boolean result = FALSE;

  if (scope_ref.sort == ifc_ds_decl_scope) {
    Opt<an_ifc_decl_scope> opt_ids;

    construct_node(&opt_ids, scope_ref);
    if (!opt_ids.has_value()) {
      goto done;
    }  /* if */

    an_ifc_type_index type = get_ifc_type(*opt_ids);
    /* FIXME: We may want to diagnose the non-fundamental type here. */
    if (type.sort != ifc_ts_type_fundamental) {
      goto done;
    }  /* if */

    Opt<an_ifc_type_fundamental> opt_itf;
    construct_node(&opt_itf, type);
    if (!opt_itf.has_value()) {
      goto done;
    }  /* if */
    switch (get_ifc_basis(*opt_itf)) {
      case ifc_tbs_class:
      case ifc_tbs_struct:
      case ifc_tbs_union:
      case ifc_tbs_interface:
        result = TRUE;
        break;
      default:
        result = FALSE;
        break;
    }  /* switch */
  }  /* if */
done:
  return result;
}  /* is_class_scope */


static inline void ensure_type_has_scope(a_type_ptr tp)
/*
Ensure that the provided type has a scope associated with it that can be used
as the parent scope of a nested entity.  Note that if this type cannot have a
scope associated with it, it will continue to not have an associated scope.
*/
{
  /* FIXME: Do we need to worry about scoped enums here? */
  if (is_immediate_class_type(tp)) {
    a_class_type_supplement_ptr ctsp;
    ctsp = class_type_supp(tp);
    if (ctsp->assoc_scope == NULL) {
      ctsp->assoc_scope = alloc_placeholder_scope(sck_class_struct_union,
                                                  /*assoc_routine=*/NULL);
    }  /* if */
  }  /* if */
}  /* ensure_type_has_scope */


static
a_module_entity_ptr process_decl_at_index(an_ifc_decl_index decl_idx,
                                          a_boolean         defer = FALSE)
/*
Process the IFC module entity declaration specified at the given declaration
index either by creating the appropriate IL entity, or, when defer is TRUE,
mark the appropriate symbol header as having a deferred module entity (which
will be lazily loaded if referenced).
*/
{
  /* Get the associated IFC module entity pointer, and then use it to
     process this declaration via process_ifc_declaration. */
  an_ifc_module       *mod = decl_idx.mod;
  a_module_entity_ptr dmep = get_ifc_module_entity_ptr(decl_idx);

  mod->process_ifc_declaration(dmep, defer);
  return dmep;
}  /* process_decl_at_index */


static a_module_entity_ptr
process_decl_via_reference(const an_ifc_decl_reference &ref,
                           a_boolean                   defer = FALSE)
/*
Process the IFC module entity declaration specified by the given declaration
reference either by creating the appropriate IL entity, or, when defer is TRUE,
mark the appropriate symbol header as having a deferred module entity (which
will be lazily loaded if referenced).
*/
{
  an_ifc_decl_index decl_idx = get_ifc_index(ref);

  return process_decl_at_index(decl_idx, defer);
}  /* process_decl_via_reference */


static a_scope_ptr get_scope(an_ifc_decl_index scope_ref)
/*
Given a scope reference find and return the associated scope.
*/
{
  a_scope_ptr result = NULL;

  if (is_null_index(scope_ref)) {
    result = il_header.primary_scope;
  } else {
    a_module_entity_ptr mep = process_decl_at_index(scope_ref);
    a_type_ptr          assoc_type = NULL;

    if (!mep->invalid) {
      if (mep->entity.kind == iek_type) {
        assoc_type = (a_type_ptr)mep->entity.ptr;
        ensure_type_has_scope(assoc_type);
      }  /* if */
      result = get_assoc_scope_of_il_entry(mep->entity.ptr,
                                           (an_il_entry_kind)mep->entity.kind);
      if (!scope_is(result, sck_file) && result->parent == NULL) {
        /* Record the parent scope if it hasn't been recorded yet.  Don't do
           that for the file scope, because that would make the file scope
           entry point at itself. */
        result->parent = mep->scope;
        if (assoc_type != NULL &&
            (scope_is(result, sck_class_struct_union) ||
             scope_is(result, sck_enum) ||
             scope_is(result, sck_func_prototype))) {
          check_assertion(result->variant.assoc_type == NULL ||
                          result->variant.assoc_type == assoc_type);
          result->variant.assoc_type = assoc_type;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* get_scope */


#if EXPENSIVE_CHECKING

static a_boolean
		in_get_home_scope = FALSE;
			/* Flag set while evaluating a call to get_home_scope.
			   This is used for eager loading mode to avoid
			   unbounded recursive loading. */

#endif /* EXPENSIVE_CHECKING */
 

template<typename an_ifc_Node_type>
static a_scope_ptr get_home_scope(const an_ifc_Node_type &node)
/*
Return the associated scope for the given declaration.
*/
{
#if EXPENSIVE_CHECKING
  /* Do not eagerly load home-scope members just because we query the home
     scope, because that leads to aborts due to recursive loading. */
  Value_saver<a_boolean>  suppression(&in_get_home_scope, /*new_value=*/TRUE);
#endif /* EXPENSIVE_CHECKING */
 
  return get_scope(get_ifc_home_scope(node));
}  /* get_home_scope */


static a_scope_ptr get_home_scope(an_ifc_decl_index decl_ref)
/*
Return the associated scope for the given declaration index.
*/
{
#if EXPENSIVE_CHECKING
  /* Do not eagerly load home-scope members just because we query the home
     scope, because that leads to aborts due to recursive loading. */
  Value_saver<a_boolean>  suppression(&in_get_home_scope, /*new_value=*/TRUE);
#endif /* EXPENSIVE_CHECKING */

  return get_scope(get_ifc_home_scope(decl_ref));
}  /* get_home_scope */


static void cache_name(a_module_token_cache_ptr     cache,
                       an_ifc_name_index            name_ref)
/*
Add the tokens corresponding to the given name to cache.
*/
{
  /* Disable spurious GCC warning about uninitialized usage of opt_name_ref
     (when this function is called by cache_simple_template_id). */
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
  name_ref.mod->cache_name(cache, name_ref);
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
}  /* cache_name */


static void issue_unsupported_construct_error(an_ifc_module     *mod,
                                              a_const_char      *node,
                                              a_source_position *pos)
/*
Issue an error that an unhandled node was encountered.  node is the textual
representation of the problematic node.  pos is the source position associated
with the diagnostic.  Call this function when encountering unsupported IFC
nodes.
*/
{
  if (!mod->assoc_module_info->contains_unsupported_constructs) {
    pos_st_error(ec_module_file_contains_unsupported_constructs,
                 &null_source_position, mod->assoc_module_info->name);
    mod->assoc_module_info->contains_unsupported_constructs = TRUE;
  }  /* if */
  pos_st_error(ec_unhandled_ifc_construct, pos, node);
}  /* issue_unsupported_construct_error */


static a_const_char *get_string_at_offset(an_ifc_text_offset offset)
/*
Return a pointer to the IFC string table for a given TextOffset.  Strings in
the IFC file are NULL-terminated.
*/
{
  an_ifc_module              *mod = offset.mod;
  an_ifc_text_offset_storage raw_offset = offset;
  char                       *result;

#if EXPENSIVE_CHECKING
  check_assertion(raw_offset < get_ifc_string_table_size(mod->header));
#endif /* EXPENSIVE_CHECKING */
  result = mod->string_table + raw_offset;
  if (*result == '<') {
    /* Unnamed entities get "synthesized" names that start with "<unnamed-".
       Turn such names into valid reserved C identifiers, except for
       "<unnamed-tag>" which is used for anonymous struct/unions. */
#define UNNAMED_TAG_NAME "<unnamed-tag>"
#define UNNAMED_PREFIX "<unnamed-"
#define REPLACE_PREFIX "__noname_"
    static_assert(sizeof(UNNAMED_PREFIX) == sizeof(REPLACE_PREFIX), "");
    if (strncmp(result, UNNAMED_PREFIX, sizeof(UNNAMED_PREFIX)-1) == 0 &&
        strcmp(result, UNNAMED_TAG_NAME) != 0) {
      memcpy(result, (void*)REPLACE_PREFIX, sizeof(UNNAMED_PREFIX)-1);
      for (char  *p = result+sizeof(UNNAMED_PREFIX); *p; ++p) {
        if (*p == '-' || *p == '>') *p = '_';
      }  /* while */
    }  /* if */
#undef REPLACE_PREFIX
#undef UNNAMED_PREFIX
#undef UNNAMED_TAG_NAME
  }  /* if */
  return (a_const_char*)result;
}  /* get_string_at_offset */


inline a_const_char *an_ifc_module::get_string_at_offset(
                                                     an_ifc_text_offset offset)
                                                                          const
/*
Return a pointer to the IFC string table for a given TextOffset.  Strings in
the IFC file are NULL-terminated.
*/
{
  return EDG_PREFIX::get_string_at_offset(offset);
}  /* get_string_at_offset */


static a_const_char* make_ifc_temporary_unique_id(an_ifc_unique_id id)
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


static an_opname_kind opname_from_niladic_op(
                                       an_ifc_niladic_operator_sort niladic_op)
/*
Map an IFC NiladicOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (niladic_op) {
    case ifc_nos_unknown:
    case ifc_nos_msvc:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for(niladic_op));
      op = onk_none;
      break;
    case ifc_nos_phantom:
    case ifc_nos_constant:
    case ifc_nos_nil:
    case ifc_nos_msvc_constant_object:
    case ifc_nos_msvc_lambda:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for(niladic_op));
      op = onk_none;
      break;
    default_is_unexpected_str("Unexpected NiladicOperator");
  }  /* switch */
  return op;
}  /* opname_from_niladic_op */


static an_opname_kind opname_from_monadic_op(
                                       an_ifc_monadic_operator_sort monadic_op)
/*
Map an IFC MonadicOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (monadic_op) {
    case ifc_mos_unknown:
    case ifc_mos_msvc:
    case ifc_mos_msvc_confusion:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for(monadic_op));
      op = onk_none;
      break;
    case ifc_mos_plus:               op = onk_plus;          break;
    case ifc_mos_negate:             op = onk_minus;         break;
    case ifc_mos_deref:              op = onk_star;          break;
    case ifc_mos_address:            op = onk_ampersand;     break;
    case ifc_mos_complement:         op = onk_compl;         break;
    case ifc_mos_not:                op = onk_not;           break;
    case ifc_mos_pre_increment:      op = onk_plus_plus;     break;
    case ifc_mos_pre_decrement:      op = onk_minus_minus;   break;
    case ifc_mos_post_increment:     op = onk_plus_plus;     break;
    case ifc_mos_post_decrement:     op = onk_minus_minus;   break;
    case ifc_mos_await:              op = onk_await;         break;
    case ifc_mos_new:                op = onk_new;           break;
    case ifc_mos_delete:             op = onk_delete;        break;
    case ifc_mos_delete_array:       op = onk_array_delete;  break;
    case ifc_mos_truncate:
    case ifc_mos_ceil:
    case ifc_mos_floor:
    case ifc_mos_paren:
    case ifc_mos_brace:
    case ifc_mos_alignas:
    case ifc_mos_alignof:
    case ifc_mos_sizeof:
    case ifc_mos_cardinality:
    case ifc_mos_typeid:
    case ifc_mos_noexcept:
    case ifc_mos_requires:
    case ifc_mos_co_return:
    case ifc_mos_yield:
    case ifc_mos_throw:
    case ifc_mos_expand:
    case ifc_mos_read:
    case ifc_mos_materialize:
    case ifc_mos_pseudo_dtor_call:
    case ifc_mos_lookup_globally:
    case ifc_mos_msvc_assume:
    case ifc_mos_msvc_alignof:
    case ifc_mos_msvc_uuidof:
    case ifc_mos_msvc_is_class:
    case ifc_mos_msvc_is_union:
    case ifc_mos_msvc_is_enum:
    case ifc_mos_msvc_is_polymorphic:
    case ifc_mos_msvc_is_empty:
    case ifc_mos_msvc_is_trivially_copy_constructible:
    case ifc_mos_msvc_is_trivially_copy_assignable:
    case ifc_mos_msvc_is_trivially_destructible:
    case ifc_mos_msvc_has_virtual_destructor:
    case ifc_mos_msvc_is_nothrow_copy_constructible:
    case ifc_mos_msvc_is_nothrow_copy_assignable:
    case ifc_mos_msvc_is_pod:
    case ifc_mos_msvc_is_abstract:
    case ifc_mos_msvc_is_trivial:
    case ifc_mos_msvc_is_trivially_copyable:
    case ifc_mos_msvc_is_standard_layout:
    case ifc_mos_msvc_is_literal_type:
    case ifc_mos_msvc_is_trivially_move_constructible:
    case ifc_mos_msvc_has_trivial_move_assign:
    case ifc_mos_msvc_is_trivially_move_assignable:
    case ifc_mos_msvc_is_nothrow_move_assignable:
    case ifc_mos_msvc_underlying_type:
    case ifc_mos_msvc_is_destructible:
    case ifc_mos_msvc_is_nothrow_destructible:
    case ifc_mos_msvc_has_unique_object_representations:
    case ifc_mos_msvc_is_aggregate:
    case ifc_mos_msvc_builtin_address_of:
    case ifc_mos_msvc_is_ref_class:
    case ifc_mos_msvc_is_value_class:
    case ifc_mos_msvc_is_simple_value_class:
    case ifc_mos_msvc_is_interface_class:
    case ifc_mos_msvc_is_delegate:
    case ifc_mos_msvc_is_final:
    case ifc_mos_msvc_is_sealed:
    case ifc_mos_msvc_has_finalizer:
    case ifc_mos_msvc_has_copy:
    case ifc_mos_msvc_has_assign:
    case ifc_mos_msvc_has_user_destructor:
    case ifc_mos_msvc_confused_expand:
    case ifc_mos_msvc_confused_dependent_sizeof:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for(monadic_op));
      op = onk_none;
      break;
    default_is_unexpected_str("Unexpected MonadicOperator");
  }  /* switch */
  return op;
}  /* opname_from_monadic_op */


static an_opname_kind opname_from_dyadic_op(
                                         an_ifc_dyadic_operator_sort dyadic_op)
/*
Map an IFC DyadicOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (dyadic_op) {
    case ifc_dos_unknown:
    case ifc_dos_msvc:
    case ifc_dos_msvc_saturated_arithmetic:
    case ifc_dos_select:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for(dyadic_op));
      op = onk_none;
      break;
    case ifc_dos_plus:            op = onk_plus;              break;
    case ifc_dos_minus:           op = onk_minus;             break;
    case ifc_dos_mult:            op = onk_star;              break;
    case ifc_dos_slash:           op = onk_divide;            break;
    case ifc_dos_modulo:          op = onk_remainder;         break;
    case ifc_dos_remainder:       op = onk_remainder;         break;
    case ifc_dos_bitand:          op = onk_ampersand;         break;
    case ifc_dos_bitor:           op = onk_or;                break;
    case ifc_dos_bitxor:          op = onk_excl_or;           break;
    case ifc_dos_lshift:          op = onk_shift_left;        break;
    case ifc_dos_rshift:          op = onk_shift_right;       break;
    case ifc_dos_equal:           op = onk_eq;                break;
    case ifc_dos_not_equal:       op = onk_ne;                break;
    case ifc_dos_less:            op = onk_lt;                break;
    case ifc_dos_less_equal:      op = onk_le;                break;
    case ifc_dos_greater:         op = onk_gt;                break;
    case ifc_dos_greater_equal:   op = onk_ge;                break;
    case ifc_dos_logic_and:       op = onk_and_and;           break;
    case ifc_dos_logic_or:        op = onk_or_or;             break;
    case ifc_dos_assign:          op = onk_assign;            break;
    case ifc_dos_plus_assign:     op = onk_plus_assign;       break;
    case ifc_dos_minus_assign:    op = onk_minus_assign;      break;
    case ifc_dos_mult_assign:     op = onk_times_assign;      break;
    case ifc_dos_slash_assign:    op = onk_divide_assign;     break;
    case ifc_dos_modulo_assign:   op = onk_remainder_assign;  break;
    case ifc_dos_bitand_assign:   op = onk_and_assign;        break;
    case ifc_dos_bitor_assign:    op = onk_or_assign;         break;
    case ifc_dos_bitxor_assign:   op = onk_excl_or_assign;    break;
    case ifc_dos_lshift_assign:   op = onk_shift_left_assign; break;
    case ifc_dos_rshift_assign:   op = onk_shift_right_assign;break;
    case ifc_dos_comma:           op = onk_comma;             break;
    case ifc_dos_arrow:           op = onk_arrow;             break;
    case ifc_dos_arrow_star:      op = onk_arrow_star;        break;
    case ifc_dos_new:             op = onk_new;               break;
    case ifc_dos_new_array:       op = onk_array_new;         break;
    case ifc_dos_compare:         op = onk_spaceship;         break;
    case ifc_dos_dot:
    case ifc_dos_dot_star:
    case ifc_dos_curry:
    case ifc_dos_apply:
    case ifc_dos_index:
    case ifc_dos_default_at:
    case ifc_dos_destruct:
    case ifc_dos_destruct_at:
    case ifc_dos_cleanup:
    case ifc_dos_qualification:
    case ifc_dos_promote:
    case ifc_dos_demote:
    case ifc_dos_coerce:
    case ifc_dos_rewrite:
    case ifc_dos_bless:
    case ifc_dos_cast:
    case ifc_dos_explicit_conversion:
    case ifc_dos_reinterpret_cast:
    case ifc_dos_static_cast:
    case ifc_dos_const_cast:
    case ifc_dos_dynamic_cast:
    case ifc_dos_narrow:
    case ifc_dos_widen:
    case ifc_dos_pretend:
    case ifc_dos_closure:
    case ifc_dos_zero_initialize:
    case ifc_dos_clear_storage:
    case ifc_dos_msvc_try_cast:
    case ifc_dos_msvc_curry:
    case ifc_dos_msvc_virtual_curry:
    case ifc_dos_msvc_align:
    case ifc_dos_msvc_bit_span:
    case ifc_dos_msvc_bitfield_access:
    case ifc_dos_msvc_obscure_bitfield_access:
    case ifc_dos_msvc_initialize:
    case ifc_dos_msvc_builtin_offset_of:
    case ifc_dos_msvc_is_base_of:
    case ifc_dos_msvc_is_convertible_to:
    case ifc_dos_msvc_is_trivially_assignable:
    case ifc_dos_msvc_is_nothrow_assignable:
    case ifc_dos_msvc_is_assignable:
    case ifc_dos_msvc_is_assignable_nocheck:
    case ifc_dos_msvc_builtin_bit_cast:
    case ifc_dos_msvc_builtin_is_layout_compatible:
    case ifc_dos_msvc_builtin_is_pointer_interconvertible_base_of:
    case ifc_dos_msvc_builtin_is_pointer_interconvertible_with_class:
    case ifc_dos_msvc_builtin_is_corresponding_member:
    case ifc_dos_msvc_intrinsic:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for(dyadic_op));
      op = onk_none;
      break;
    default_is_unexpected_str("Unexpected DyadicOperator");
  }  /* switch */
  return op;
}  /* opname_from_dyadic_op */


static an_opname_kind opname_from_triadic_op(
                                       an_ifc_triadic_operator_sort triadic_op)
/*
Map an IFC TriadicOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (triadic_op) {
    case ifc_tos_unknown:
    case ifc_tos_msvc:
    case ifc_tos_msvc_confusion:
    case ifc_tos_msvc_confused_choice:
    case ifc_tos_msvc_confused_push_state:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for(triadic_op));
      op = onk_none;
      break;
    case ifc_tos_choice:
    case ifc_tos_construct_at:
    case ifc_tos_initialize:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for(triadic_op));
      op = onk_none;
      break;
    default_is_unexpected_str("Unexpected TriadicOperator");
  }  /* switch */
  return op;
}  /* opname_from_triadic_op */


static an_opname_kind opname_from_storage_op(
                           an_ifc_storage_instruction_operator_sort storage_op)
/*
Map an IFC StorageOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (storage_op) {
    case ifc_sios_unknown:
    case ifc_sios_msvc:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for(storage_op));
      op = onk_none;
      break;
    case ifc_sios_allocate_single:    op = onk_new;            break;
    case ifc_sios_allocate_array:     op = onk_array_new;      break;
    case ifc_sios_deallocate_single:  op = onk_delete;         break;
    case ifc_sios_deallocate_array:   op = onk_array_delete;   break;
    default_is_unexpected_str("Unexpected StorageOperator");
  }  /* switch */
  return op;
}  /* opname_from_storage_op */


static an_opname_kind opname_from_variadic_op(
                                     an_ifc_variadic_operator_sort variadic_op)
/*
Map an IFC VariadicOperator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (variadic_op) {
    case ifc_vos_unknown:
    case ifc_vos_msvc:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for(variadic_op));
      op = onk_none;
      break;
    case ifc_vos_collection:
    case ifc_vos_sequence:
    case ifc_vos_msvc_has_trivial_constructor:
    case ifc_vos_msvc_is_constructible:
    case ifc_vos_msvc_is_nothrow_constructible:
    case ifc_vos_msvc_is_trivially_constructible:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_operator, &error_position,
                        str_for(variadic_op));
      op = onk_none;
      break;
    default_is_unexpected_str("Unexpected VariadicOperator");
  }  /* switch */
  return op;
}  /* opname_from_variadic_op */


static an_opname_kind opname_from_operator(an_ifc_operator_category ifc_op)
/*
Map an IFC Operator to an_opname_kind.
*/
{
  an_opname_kind op;

  switch (ifc_op.sort) {
    case ifc_os_dyadic_operator:
      op = opname_from_dyadic_op(ifc_op.variant.dyadic_operator);
      break;
    case ifc_os_monadic_operator:
      op = opname_from_monadic_op(ifc_op.variant.monadic_operator);
      break;
    case ifc_os_niladic_operator:
      op = opname_from_niladic_op(ifc_op.variant.niladic_operator);
      break;
    case ifc_os_storage_instruction_operator:
      op = opname_from_storage_op(ifc_op.variant.storage_instruction_operator);
      break;
    case ifc_os_triadic_operator:
      op = opname_from_triadic_op(ifc_op.variant.triadic_operator);
      break;
    case ifc_os_variadic_operator:
      op = opname_from_variadic_op(ifc_op.variant.variadic_operator);
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
  opkind_error      /* Invalid/error operator value. */
};


static an_operator_kind get_operator_kind(an_ifc_module                *mod,
                                          an_ifc_niladic_operator_sort op)
/*
Return the kind of operator described by op in the context of the given module.
*/
{
  an_operator_kind kind = opkind_error;

  switch (op) {
    case ifc_nos_unknown:
    case ifc_nos_msvc:
      { a_string err_msg(str_for(op), " is not a supported NiladicOperator");

        ifc_unexpected(mod, err_msg);
      }
      break;
    case ifc_nos_phantom:
    case ifc_nos_constant:
    case ifc_nos_nil:
    case ifc_nos_msvc_constant_object:
    case ifc_nos_msvc_lambda:
      kind = opkind_other;
      break;
    default_is_unexpected_str("Unexpected NiladicOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(an_ifc_module                *mod,
                                          an_ifc_monadic_operator_sort op)
/*
Return the kind of operator described by op in the context of the given module.
*/
{
  an_operator_kind kind = opkind_error;

  switch (op) {
    case ifc_mos_unknown:
    case ifc_mos_msvc:
    case ifc_mos_msvc_confusion:
      { a_string err_msg(str_for(op), " is not a supported MonadicOperator");

        ifc_unexpected(mod, err_msg);
      }
      break;
    case ifc_mos_plus:
    case ifc_mos_negate:
    case ifc_mos_deref:
    case ifc_mos_address:
    case ifc_mos_complement:
    case ifc_mos_not:
    case ifc_mos_pre_increment:
    case ifc_mos_pre_decrement:
    case ifc_mos_await:
    case ifc_mos_co_return:
    case ifc_mos_yield:
    case ifc_mos_throw:
    case ifc_mos_lookup_globally:
    case ifc_mos_new:
    case ifc_mos_delete:
    case ifc_mos_delete_array:
      kind = opkind_basic;
      break;
    case ifc_mos_post_increment:
    case ifc_mos_post_decrement:
    case ifc_mos_expand:
      kind = opkind_post;
      break;
    case ifc_mos_paren:
    case ifc_mos_brace:
      kind = opkind_other;
      break;
    case ifc_mos_truncate:
    case ifc_mos_ceil:
    case ifc_mos_floor:
    case ifc_mos_alignas:
    case ifc_mos_alignof:
    case ifc_mos_sizeof:
    case ifc_mos_cardinality:
    case ifc_mos_typeid:
    case ifc_mos_noexcept:
    case ifc_mos_requires:
    case ifc_mos_read:
    case ifc_mos_materialize:
    case ifc_mos_pseudo_dtor_call:
    case ifc_mos_msvc_assume:
    case ifc_mos_msvc_alignof:
    case ifc_mos_msvc_uuidof:
    case ifc_mos_msvc_is_class:
    case ifc_mos_msvc_is_union:
    case ifc_mos_msvc_is_enum:
    case ifc_mos_msvc_is_polymorphic:
    case ifc_mos_msvc_is_empty:
    case ifc_mos_msvc_is_trivially_copy_constructible:
    case ifc_mos_msvc_is_trivially_copy_assignable:
    case ifc_mos_msvc_is_trivially_destructible:
    case ifc_mos_msvc_has_virtual_destructor:
    case ifc_mos_msvc_is_nothrow_copy_constructible:
    case ifc_mos_msvc_is_nothrow_copy_assignable:
    case ifc_mos_msvc_is_pod:
    case ifc_mos_msvc_is_abstract:
    case ifc_mos_msvc_is_trivial:
    case ifc_mos_msvc_is_trivially_copyable:
    case ifc_mos_msvc_is_standard_layout:
    case ifc_mos_msvc_is_literal_type:
    case ifc_mos_msvc_is_trivially_move_constructible:
    case ifc_mos_msvc_has_trivial_move_assign:
    case ifc_mos_msvc_is_trivially_move_assignable:
    case ifc_mos_msvc_is_nothrow_move_assignable:
    case ifc_mos_msvc_underlying_type:
    case ifc_mos_msvc_is_destructible:
    case ifc_mos_msvc_is_nothrow_destructible:
    case ifc_mos_msvc_has_unique_object_representations:
    case ifc_mos_msvc_is_aggregate:
    case ifc_mos_msvc_builtin_address_of:
    case ifc_mos_msvc_is_ref_class:
    case ifc_mos_msvc_is_value_class:
    case ifc_mos_msvc_is_simple_value_class:
    case ifc_mos_msvc_is_interface_class:
    case ifc_mos_msvc_is_delegate:
    case ifc_mos_msvc_is_final:
    case ifc_mos_msvc_is_sealed:
    case ifc_mos_msvc_has_finalizer:
    case ifc_mos_msvc_has_copy:
    case ifc_mos_msvc_has_assign:
    case ifc_mos_msvc_has_user_destructor:
    case ifc_mos_msvc_confused_dependent_sizeof:
      kind = opkind_func_like;
      break;
    case ifc_mos_msvc_confused_expand:
      kind = opkind_post;
      break;
    default_is_unexpected_str("Unexpected MonadicOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(an_ifc_module               *mod,
                                          an_ifc_dyadic_operator_sort op)
/*
Return the kind of operator described by op in the context of the given module.
*/
{
  an_operator_kind kind = opkind_error;

  switch (op) {
    case ifc_dos_unknown:
    case ifc_dos_select:
    case ifc_dos_msvc:
    case ifc_dos_msvc_saturated_arithmetic:
      { a_string err_msg(str_for(op), " is not a supported DyadicOperator");

        ifc_unexpected(mod, err_msg);
      }
      break;
    case ifc_dos_plus:
    case ifc_dos_minus:
    case ifc_dos_mult:
    case ifc_dos_slash:
    case ifc_dos_modulo:
    case ifc_dos_remainder:
    case ifc_dos_bitand:
    case ifc_dos_bitor:
    case ifc_dos_bitxor:
    case ifc_dos_lshift:
    case ifc_dos_rshift:
    case ifc_dos_equal:
    case ifc_dos_not_equal:
    case ifc_dos_less:
    case ifc_dos_less_equal:
    case ifc_dos_greater:
    case ifc_dos_greater_equal:
    case ifc_dos_logic_and:
    case ifc_dos_logic_or:
    case ifc_dos_assign:
    case ifc_dos_plus_assign:
    case ifc_dos_minus_assign:
    case ifc_dos_mult_assign:
    case ifc_dos_slash_assign:
    case ifc_dos_modulo_assign:
    case ifc_dos_bitand_assign:
    case ifc_dos_bitor_assign:
    case ifc_dos_bitxor_assign:
    case ifc_dos_lshift_assign:
    case ifc_dos_rshift_assign:
    case ifc_dos_comma:
    case ifc_dos_arrow:
    case ifc_dos_arrow_star:
    case ifc_dos_compare:
    case ifc_dos_dot:
    case ifc_dos_dot_star:
      kind = opkind_basic;
      break;
    case ifc_dos_new:
    case ifc_dos_new_array:
      kind = opkind_other;
      break;
    case ifc_dos_cast:
    case ifc_dos_explicit_conversion:
      kind = opkind_c_cast;
      break;
    case ifc_dos_reinterpret_cast:
    case ifc_dos_static_cast:
    case ifc_dos_const_cast:
    case ifc_dos_dynamic_cast:
      kind = opkind_cpp_cast;
      break;
    case ifc_dos_curry:
    case ifc_dos_apply:
    case ifc_dos_index:
    case ifc_dos_default_at:
    case ifc_dos_destruct:
    case ifc_dos_destruct_at:
    case ifc_dos_cleanup:
    case ifc_dos_qualification:
    case ifc_dos_promote:
    case ifc_dos_demote:
    case ifc_dos_coerce:
    case ifc_dos_rewrite:
    case ifc_dos_bless:
    case ifc_dos_narrow:
    case ifc_dos_widen:
    case ifc_dos_pretend:
    case ifc_dos_closure:
    case ifc_dos_zero_initialize:
    case ifc_dos_clear_storage:
    case ifc_dos_msvc_try_cast:
    case ifc_dos_msvc_curry:
    case ifc_dos_msvc_virtual_curry:
    case ifc_dos_msvc_align:
    case ifc_dos_msvc_bit_span:
    case ifc_dos_msvc_bitfield_access:
    case ifc_dos_msvc_obscure_bitfield_access:
    case ifc_dos_msvc_initialize:
    case ifc_dos_msvc_builtin_offset_of:
    case ifc_dos_msvc_is_base_of:
    case ifc_dos_msvc_is_convertible_to:
    case ifc_dos_msvc_is_trivially_assignable:
    case ifc_dos_msvc_is_nothrow_assignable:
    case ifc_dos_msvc_is_assignable:
    case ifc_dos_msvc_is_assignable_nocheck:
    case ifc_dos_msvc_builtin_bit_cast:
    case ifc_dos_msvc_builtin_is_layout_compatible:
    case ifc_dos_msvc_builtin_is_pointer_interconvertible_base_of:
    case ifc_dos_msvc_builtin_is_pointer_interconvertible_with_class:
    case ifc_dos_msvc_builtin_is_corresponding_member:
    case ifc_dos_msvc_intrinsic:
      kind = opkind_func_like;
      break;
    default_is_unexpected_str("Unexpected DyadicOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(an_ifc_module                *mod,
                                          an_ifc_triadic_operator_sort op)
/*
Return the kind of operator described by op in the context of the given module.
*/
{
  an_operator_kind kind = opkind_error;

  switch (op) {
    case ifc_tos_unknown:
    case ifc_tos_msvc:
    case ifc_tos_msvc_confusion:
    case ifc_tos_msvc_confused_choice:
    case ifc_tos_msvc_confused_push_state:
      { a_string err_msg(str_for(op), " is not a supported TriadicOperator");

        ifc_unexpected(mod, err_msg);
      }
      break;
    case ifc_tos_choice:
    case ifc_tos_construct_at:
    case ifc_tos_initialize:
      kind = opkind_other;
      break;
    default_is_unexpected_str("Unexpected TriadicOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(
                                 an_ifc_module                            *mod,
                                 an_ifc_storage_instruction_operator_sort op)
/*
Return the kind of operator described by op in the context of the given module.
*/
{
  an_operator_kind kind = opkind_error;

  switch (op) {
    case ifc_sios_unknown:
    case ifc_sios_msvc:
      { a_string err_msg(str_for(op),
                         " is not a supported StorageInstructionOperator");

        ifc_unexpected(mod, err_msg);
      }
      break;
    case ifc_sios_allocate_single:
    case ifc_sios_allocate_array:
    case ifc_sios_deallocate_single:
    case ifc_sios_deallocate_array:
      kind = opkind_other;
      break;
    default_is_unexpected_str("Unexpected StorageOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(an_ifc_module                 *mod,
                                          an_ifc_variadic_operator_sort op)
/*
Return the kind of operator described by op in the context of the given module.
*/
{
  an_operator_kind kind = opkind_error;

  switch (op) {
    case ifc_vos_unknown:
    case ifc_vos_msvc:
      { a_string err_msg(str_for(op), " is not a supported VariadicOperator");

        ifc_unexpected(mod, err_msg);
      }
      break;
    case ifc_vos_collection:
    case ifc_vos_sequence:
      kind = opkind_other;
      break;
    case ifc_vos_msvc_has_trivial_constructor:
    case ifc_vos_msvc_is_constructible:
    case ifc_vos_msvc_is_nothrow_constructible:
    case ifc_vos_msvc_is_trivially_constructible:
      kind = opkind_func_like;
      break;
    default_is_unexpected_str("Unexpected VariadicOperator");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


static an_operator_kind get_operator_kind(an_ifc_module            *mod,
                                          an_ifc_operator_category op)
/*
Return the kind of operator described by op in the context of the given module.
*/
{
  an_operator_kind kind = opkind_error;

  switch (op.sort) {
    case ifc_os_dyadic_operator:
      kind = get_operator_kind(mod, op.variant.dyadic_operator);
      break;
    case ifc_os_monadic_operator:
      kind = get_operator_kind(mod, op.variant.monadic_operator);
      break;
    case ifc_os_niladic_operator:
      kind = get_operator_kind(mod, op.variant.niladic_operator);
      break;
    case ifc_os_storage_instruction_operator:
      kind = get_operator_kind(mod, op.variant.storage_instruction_operator);
      break;
    case ifc_os_triadic_operator:
      kind = get_operator_kind(mod, op.variant.triadic_operator);
      break;
    case ifc_os_variadic_operator:
      kind = get_operator_kind(mod, op.variant.variadic_operator);
      break;
    default_is_unexpected_str("Unexpected OperatorSort");
  }  /* switch */
  return kind;
}  /* get_operator_kind */


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
  ENUM_TYPE_FOR_BIT_FIELD(a_name_linkage_kind)
		default_name_linkage:NUM_BITS_FOR_NAME_LINKAGE;
			/* Previous default_name_linkage setting. */
  ENUM_TYPE_FOR_BIT_FIELD(an_access_specifier)
		current_access:2;
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
  /* FIXME: Using a fake module import decl creates a possibility for using an
     improperly initialized module import decl.  This should be
     reconsidered. */
  a_module_import_decl mid;
  a_module             mod;

  mid.module_info = &mod;
  mod.full_name = module_file;
  /* FIXME: Use a better source position. */
  mid.position = null_source_position;
  mid.module_name_position = null_source_position;
  if (!open_and_map_ifc_module_file(&mid, /*issue_diag=*/FALSE)) {
    goto done;
  }  /* if */
  /* FIXME: The current setup here in some cases leads to multiple diagnostics
     for the same module as it's reconsidered.  However, suppressing
     diagnostics here results in significantly worse diagnostics for a matching
     module with a bad version. */
  if (!init_string_table_and_header(&mid, /*issue_diag=*/TRUE)) {
    goto done_with_close;
  }  /* if */
  {
    an_ifc_unit_index unit_idx = get_ifc_unit(header);
    a_C_str_handle    this_name;

    switch (unit_idx.sort) {
      case ifc_us_source:
      case ifc_us_header:
        this_name.ptr = NULL;
        break;
      case ifc_us_primary:
      case ifc_us_partition:
      case ifc_us_exported_tu:
        { an_ifc_text_offset text_offset{this, unit_idx.value};

          this_name.ptr = get_string_at_offset(text_offset);
        }
        break;
      default_is_unexpected();
    }  /* switch */
    result = (this_name == module_name);
  }
done_with_close:
  close();
done:
  return result;
}  /* matches_module */

static size_t calculate_validation_block_byte_count(uint32_t num_elements)
/*
For a given number of elements, return the number of bytes required to
represent the elements' validation status.
*/
{
  /* Over count by one integer as this simplifies the logic and is necessary in
     the vast majority of cases (where num_elements is not evenly divisible by
     16) anyways. */
  return (1 + (num_elements / 16)) * sizeof(uint32_t);
}  /* calculate_validation_block_byte_count */


static uint32_t* alloc_validation_bit_array(uint32_t num_elements)
/*
Allocate an array of bits for validating num_elements elements, with two bits
for each element.
*/
{
  size_t   size = calculate_validation_block_byte_count(num_elements);
  uint32_t *validation_bits = (uint32_t *)alloc_fe(size);

  memzero((char *)validation_bits, size);
  return validation_bits;
}  /* allocate_validation_bit_array */


static void invalidate_all_validation_bits(uint32_t *validation_bits,
                                           uint32_t num_elements)
/*
Mark all elements in the given validation bits array as invalid.
*/
{
  size_t size = calculate_validation_block_byte_count(num_elements);
  memset((char *)validation_bits, 0xFF, size);
}  /* invalidate_all_validation_bits */


static void emit_unsupported_ifc_version_diagnostic(
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

  check_assertion(mod == mod_iface->assoc_module_info ||
                  mod_iface->assoc_module_info == NULL);
  pos_st_num2_diagnostic(severity, ec_unsupported_ifc_file_version,
                         &midp->module_name_position, mod->full_name,
                         get_ifc_major_version(mod_iface->header),
                         get_ifc_minor_version(mod_iface->header));
}  /* emit_unsupported_ifc_version_diagnostic */


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
    import_referenced_modules(midp->impl_unit_importing_self);
#if DEBUG
    if (db_flag_is_set("ms_modsrc")) {
      /* Generate a textual representation of the module file and print it. */
      db_ifc_scope(get_ifc_global_scope(header));
    }  /* if */
#endif /* DEBUG */
    /* Indicate to the lookup routines that lazy symbols are in use. */
    lazy_symbols_may_be_visible = TRUE;
    /* Process all declarations in the global scope. */
    process_ifc_scope(get_ifc_global_scope(header), il_header.primary_scope);
    if (!mod->suppress_macro_export && is_header_unit(mod)) {
      export_ifc_macros();
    }  /* if */
  }  /* if */
done:
  return result;
}  /* import */


void an_ifc_module::import_referenced_modules(
                                            a_boolean impl_unit_importing_self)
/*
Import all appropriate modules that have been referenced by this module.
Modules that have been imported but not re-exported are not imported at this
time, as their symbols are not visible except when referenced by symbols within
this module.  If impl_unit_importing_self is TRUE, this is the case of a module
implementation unit importing its own interface unit (which means that non-
exported imports need to be imported).
*/
{
  if (get_partition_metadata(ifc_pk_module_exported).name != NULL) {
    auto num_modules = get_num_entries(ifc_pk_module_exported);

    for (decltype(num_modules) idx = 0; idx < num_modules; ++idx) {
      Opt<an_ifc_module_export_reference> opt_imer;
      an_ifc_partition_kind_index         ref_idx{this, ifc_pk_module_exported,
                                                  idx};

      construct_node(&opt_imer, ref_idx);
      /* Allow single failures to be "ignored", continue processing. */
      if (!opt_imer.has_value()) {
        continue;
      }  /* if */
      transitive_import_module(get_ifc_reference(*opt_imer));
    }  /* for */
  }  /* if */
  if (impl_unit_importing_self &&
      get_partition_metadata(ifc_pk_module_imported).name != NULL) {
    auto num_modules = get_num_entries(ifc_pk_module_imported);

    for (decltype(num_modules) idx = 0; idx < num_modules; ++idx) {
      Opt<an_ifc_module_import_reference> opt_imir;
      an_ifc_partition_kind_index         ref_idx{this, ifc_pk_module_imported,
                                                  idx};

      construct_node(&opt_imir, ref_idx);
      /* Allow single failures to be "ignored", continue processing. */
      if (!opt_imir.has_value()) {
        continue;
      }  /* if */
      transitive_import_module(get_ifc_reference(*opt_imir));
    }  /* for */
  }  /* if */
}  /* import_referenced_modules */


void an_ifc_module::define_ifc_macro(an_ifc_macro_index macro)
/*
Given an IFC macro, process that macro definition.  Note that this function
assumes variables such as "in_preprocessing_directive", "curr_source_line", and
all related variables have been set appropriately by the calling function (see
export_ifc_macros), to avoid constantly setting and re-setting the values of
these variables.  Similarly, curr_token is assumed to be either ignorable or
already saved for restoration.
*/
{
  a_module_token_cache        cache;
  a_module_entity_ptr         mep = get_ifc_module_entity_ptr(macro);
  a_module_entity_stack_state mep_state(mep);

  cache_macro(&cache, macro);
  if (cache.is_valid()) {
    {
      a_cached_token_ptr first_token = cache.get_first_token();

      ifc_requirement(this, (first_token != NULL &&
                             first_token->token == tok_identifier),
                      "expected the first macro token to be an identifier");
      /* Create an equivalent define directive so that we can leave the
         processing to proc_define. */
      copy_source_position(first_token->source_position,
                           pos_curr_token);
    }
    init_token_string(&pos_curr_token, /*keep_spacing=*/FALSE,
                      /*suppress_identifier_wrapping=*/TRUE);
    add_token_cache_to_string(cache.as_canonical());
    put_ch_to_temp_text_buffer(LE_ESCAPE);
    put_ch_to_temp_text_buffer(LE_NEWLINE);
    put_ch_to_temp_text_buffer(LE_ESCAPE);
    put_ch_to_temp_text_buffer(LE_END_OF_LINE);
    curr_char_loc = curr_source_line = start_of_curr_token = temp_text_buffer;

    a_symbol_header_ptr sym_header =
                        cache.get_first_token()->variant.locator.symbol_header;
    len_of_curr_token = sym_header->identifier_length;
    after_end_of_curr_source_line = temp_text_buffer + pos_in_temp_text_buffer;
    logical_char_info_entries_used = 0;
    {
      a_diag_count_snapshot diag_cnt_snapshot;

      (void)proc_define();
      if (curr_token != tok_newline) {
        expect_error_since(diag_cnt_snapshot,
                           "expected error from proc_define");
      }  /* if */
    }
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
  an_ifc_partition_metadata object_like_part =
                              get_partition_metadata(ifc_pk_macro_object_like);
  an_ifc_partition_metadata func_like_part =
                            get_partition_metadata(ifc_pk_macro_function_like);
  if (object_like_part.size > 0) {
    uint32_t n_macros = object_like_part.size / object_like_part.entry_size;

    for (uint32_t idx = 0; idx < n_macros; ++idx) {
      an_ifc_macro_index macro_idx{this, ifc_ms_macro_object_like, idx};

      define_ifc_macro(macro_idx);
    }  /* for */
  }  /* if */
  if (func_like_part.size > 0) {
    uint32_t n_macros = func_like_part.size / func_like_part.entry_size;

    for (uint32_t idx = 0; idx < n_macros; ++idx) {
      an_ifc_macro_index macro_idx{this, ifc_ms_macro_function_like, idx};

      define_ifc_macro(macro_idx);
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


static void prepare_cached_template_parse(
                                         a_module_token_cache_ptr cache,
                                         a_scope_ptr              encl_scope,
                                         a_decl_parse_state_ptr   dps,
                                         a_tmpl_decl_state_ptr    decl_state,
                                         a_token_kind             *final_token)
/*
Prepare parsing of a cached template or template specialization declaration.
cache represents the cache to be parsed.  encl_scope is the scope containing
the declaration to be parsed.  dps is a pointer to the storage for the
associated decl parse state.  decl_state is a pointer to the storage for the
associated template decl state.  Finally, final_token is a pointer to the
associated storage for the final token seen during parsing.
*/
{
  init_decl_parse_state(dps);
  init_templ_decl_state(decl_state, dps);
  decl_state->pragmas_bound_to_template = extract_curr_construct_pragmas();
  decl_state->starting_token_sequence_number = curr_token_sequence_number;
  decl_state->final_token_ptr = final_token;
  decl_state->enclosing_scope = encl_scope;
  decl_state->orig_decl_level = decl_scope_level;
  decl_state->effective_decl_level = decl_scope_level;
}  /* prepare_cached_template_parse */


static inline char *get_parsed_entity(a_decl_parse_state *dps,
                                      an_il_entry_kind   *kind)
/*
Given a declaration parse state, return a pointer to the corresponding entity
and update kind with the associated entity kind.
*/
{
  char *result = NULL;

  if (dps->sym != NULL) {
    result = il_entry_for_symbol_null_okay(dps->sym, kind);
  } else {
    *kind = iek_none;
  }  /* if */
  return result;
}  /* get_parsed_entity */


static inline char *get_parsed_template_entity(a_tmpl_decl_state  *decl_state,
                                               an_il_entry_kind   *kind)
/*
Given a template declaration parse state, return a pointer to the corresponding
entity and update kind with the associated entity kind.
*/
{
  char *result = NULL;

  if (decl_state->il_template_entry != NULL) {
    result = (char*)decl_state->il_template_entry;
    *kind = iek_template;
  } else {
    *kind = iek_none;
  }  /* if */
  return result;
}  /* get_parsed_template_entity */


static char *parse_cached_template(a_module_token_cache_ptr cache,
                                   a_scope_ptr              encl_scope,
                                   an_il_entry_kind         *kind)
/*
Parse the tokens corresponding to a template declaration cache.  encl_scope is
the scope containing the template declaration.  Return a pointer to the
corresponding template entity and update kind with the associated entity kind.
*/
{
  a_decl_parse_state dps;
  a_tmpl_decl_state  decl_state;
  a_token_kind       final_token = tok_error;

#if DEBUG
  if (db_flag_is_set("ms_ifc_token_def")) {
    fprintf(f_debug, "Reconstituted template declaration:\n");
    db_tokens(cache);
    fprintf(f_debug, "\n---------------------\n");
  }  /* if */
#endif /* DEBUG */
  {
    a_module_entity_rescan rescan(cache, &final_token);

    prepare_cached_template_parse(cache, encl_scope, &dps, &decl_state,
                                  &final_token);
    template_or_specialization_declaration_full(&decl_state,
                                                /*is_generic=*/FALSE,
                                                /*orig_dps=*/NULL);
  }
  return get_parsed_template_entity(&decl_state, kind);
}  /* parse_cached_template */


static char *parse_cached_partial_specialization(
                                           a_module_token_cache_ptr cache,
                                           a_scope_ptr              encl_scope,
                                           an_il_entry_kind         *kind)
/*
Parse the tokens corresponding to a partial specialization declaration cache.
encl_scope is the scope containing the partial specialization declaration.
Return a pointer to the corresponding partial specialization entity and update
kind with the associated entity kind.
*/
{
  a_decl_parse_state dps;
  a_tmpl_decl_state  decl_state;
  a_token_kind       final_token = tok_error;

#if DEBUG
  if (db_flag_is_set("ms_ifc_token_def")) {
    fprintf(f_debug, "Reconstituted partial specialization declaration:\n");
    db_tokens(cache);
    fprintf(f_debug, "\n---------------------\n");
  }  /* if */
#endif /* DEBUG */
  {
    a_module_entity_rescan rescan(cache, &final_token);

    prepare_cached_template_parse(cache, encl_scope, &dps, &decl_state,
                                  &final_token);
    template_or_specialization_declaration_full(&decl_state,
                                                /*is_generic=*/FALSE,
                                                /*orig_dps=*/NULL);
  }
  return get_parsed_template_entity(&decl_state, kind);
}  /* parse_cached_partial_specialization */

namespace {

using an_ifc_function_body_map = Ptr_map<a_routine_ptr, an_ifc_decl_index>;
                        /* The type of a map that associates IFC function
                           bodies with IL routine entries. */

an_ifc_function_body_map
                *ifc_function_bodies;
                        /* A map from IL routine entry pointers to entries of
                           type an_ifc_decl_index that can be used to retrieve
                           the definition of a function body when needed. */

}  /* namespace */


template<typename an_ifc_Node_type>
static a_boolean function_has_generated_definition(
                                                  const an_ifc_Node_type &node)
/*
Return TRUE if the given function-like IFC declaration node has a definition
that's compiler generated (i.e., "= default" or "= delete;"); otherwise, return
FALSE.
*/
{
  a_boolean                       result = FALSE;
  an_ifc_function_traits_bitfield traits = get_ifc_traits(node);

  if (test_bitmask<ifc_ftb_defaulted>(traits)) {
    result = TRUE;
  } else if (test_bitmask<ifc_ftb_deleted>(traits)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* function_has_generated_definition */


template<typename an_ifc_Node_type>
static a_boolean function_is_user_defined(const an_ifc_Node_type &node)
/*
Return TRUE if the given function-like IFC declaration node has a definition
that is not "= default" or "= delete"; otherwise, return FALSE.
*/
{
  an_ifc_reachable_properties_bitfield properties = get_ifc_properties(node);
  an_ifc_function_traits_bitfield      traits = get_ifc_traits(node);

  /* For the IFC to provide a function definition, the function must be
     constexpr and the definition must be exported (marked by the presence of a
     reachable initializer property). */
  return (test_bitmask<ifc_rpb_initializer>(properties) &&
          !function_has_generated_definition(node) &&
          (test_bitmask<ifc_ftb_constexpr>(traits) ||
           test_bitmask<ifc_ftb_immediate>(traits)));
}  /* function_is_user_defined */


template<typename an_ifc_Node_type>
static a_boolean function_is_defined(const an_ifc_Node_type &node)
/*
Return TRUE if the given function-like IFC declaration node has a definition;
otherwise, return FALSE.
*/
{
  return (function_has_generated_definition(node) ||
          function_is_user_defined(node));
}  /* function_is_defined */


void record_pending_ifc_function_body(a_routine_ptr     rp,
                                      an_ifc_decl_index decl_idx)
/*
Record the information needed to retrieve a definition for rp if it turns out
to be needed later on.
*/
{
  (void)ifc_function_bodies->map_or_replace(rp, decl_idx);
#if CHECKING
  // FIXME: We should check that rp is from a header unit.  That should be the
  //        only way we can reach two definitions for the same routine.
#endif /* CHECKING */
}  /* record_pending_ifc_function_body */


void record_symbol_for_ifc_decl(a_symbol_ptr  sym)
/*
The current token is a tok_ifc_decl token.  Map its associated IFC declaration
index information to the given symbol.
*/
{
  an_ifc_decl_index decl_idx =
               from_lexical_index<an_ifc_decl_index>(ifc_index_for_curr_token);

  ifc_decl_lookup_table->map(decl_idx, sym);
}  /* record_symbol_for_ifc_decl */


static a_symbol_ptr overload_set_from_il_entity_list(
                                            an_il_entity_list_entry_ptr ielep)
/*
The given list should contain a_routine and a_template entries: Build an
ad-hoc overload set symbol from them and return that symbol.  For an empty
list return NULL.  For a singleton list return the symbol for the one routine
or template.
*/
{
  a_symbol_ptr             result;
  a_source_correspondence  *scp;

  if (ielep == NULL) {
    result = NULL;
  } else if (ielep->next == NULL) {
    scp = source_corresp_for_il_entry(ielep->entity.ptr, ielep->entity.kind);
    result = (a_symbol_ptr)scp->assoc_info;
  } else {
    a_symbol_ptr  src_sym, dst_sym, sym_list = NULL;
    do {
      scp = source_corresp_for_il_entry(ielep->entity.ptr, ielep->entity.kind);
      src_sym = (a_symbol_ptr)scp->assoc_info;
      if (symbol_is(src_sym, sk_variable_template)) {
        /* Currently, some IFC function templates are parsed erroneously,
           producing a variable template instead. */
        pos_sy_error(ec_ifc_function_template_parse_failure, &error_position,
                     src_sym);
      } else {
        dst_sym = alloc_symbol(src_sym->kind, src_sym->header,
                               &src_sym->decl_position);
        *dst_sym = *src_sym;
        dst_sym->next = sym_list;
        sym_list = dst_sym;
      }  /* if */
      ielep = ielep->next;
    } while (ielep != NULL);
    result = alloc_symbol((a_symbol_kind)sk_overloaded_function,
                          sym_list->header, &error_position);
    result->variant.overloaded_function.symbols = sym_list;
  }  /* if */
  return result;
}  /* overload_set_from_il_entity_list */


static a_symbol_ptr symbol_for_decl_index(an_ifc_decl_index  decl_idx)
/*
Return the symbol associated with the declaration corresponding to decl_idx.
Return NULL if none is found.
*/
{
  a_symbol_ptr  result = ifc_decl_lookup_table->get(decl_idx);
  a_boolean     load_ifc_entry = FALSE; 

  if (result != NULL) {
    /* We found the symbol above. */
  } else if (decl_idx.sort == ifc_ds_decl_template) {
    Opt<an_ifc_decl_template>  opt_template;
    construct_node(&opt_template, decl_idx);
    if (opt_template.has_value()) {
      a_scope_ptr  parent_scope = get_home_scope(*opt_template);
      if (!scope_is(parent_scope, sck_class_struct_union)) {
        /* Non-member templates can be individually loaded from an IFC file. */
        load_ifc_entry = TRUE;
      } else {
        a_type_ptr  parent_class = parent_scope->variant.assoc_type;
        if (parent_class->incomplete) {
          /* Attempt to complete the class type.  This may include friend
             declarations within the type. */
          an_ifc_module           *mod = decl_idx.mod;
          Value_saver<a_boolean>  suppression(&mod->suppress_friend_token,
                                              /*new_value=*/FALSE);
          complete_type_is_needed(parent_class);
          result = ifc_decl_lookup_table->get(decl_idx);
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (decl_idx.sort != ifc_ds_decl_method &&
             decl_idx.sort != ifc_ds_decl_constructor &&
             decl_idx.sort != ifc_ds_decl_destructor &&
             decl_idx.sort != ifc_ds_decl_field &&
             decl_idx.sort != ifc_ds_decl_bitfield &&
             decl_idx.sort != ifc_ds_decl_property) {
    load_ifc_entry = TRUE;
  }  /* if */
  if (load_ifc_entry) {
    /* Load a namespace-scope entity from the IFC file and determine its
       front-end symbol. */
    a_source_correspondence  *scp;
    a_module_entity          *mep;
    an_ifc_module            *mod = decl_idx.mod;
    /* Disable the "friend" specifier since we are parsing the entity itself
       (in namespace scope) and not its friendship (which is handled
       elsewhere). */
    Value_saver<a_boolean>   suppression(&mod->suppress_friend_token,
                                         /*new_value=*/TRUE);
    mep = get_ifc_module_entity_ptr(decl_idx);
    if (mep->entity.ptr == NULL) {
      mod->process_ifc_declaration(mep, /*defer=*/FALSE);
      result = ifc_decl_lookup_table->get(decl_idx);
    }  /* if */
    if (result != NULL) {
      /* This function may be called recursively for the same index, in which
         case a result symbol might already have gotten mapped. */
    } else {
      if (mep->entity.kind == iek_il_entity_list_entry) {
        result = overload_set_from_il_entity_list(
                                (an_il_entity_list_entry_ptr)mep->entity.ptr);
      } else {
        /* Note that source_corresp_for_il_entry returns NULL for iek_none. */
        scp = source_corresp_for_il_entry(mep->entity.ptr, mep->entity.kind);
        if (scp != NULL) {
          result = (a_symbol_ptr)scp->assoc_info;
        }  /* if */
      }  /* if */
      if (result != NULL) {
        ifc_decl_lookup_table->map(decl_idx, result);
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* symbol_for_decl_index */


static
a_symbol_ptr load_ifc_entity_ref(an_ifc_expr_index  expr_idx)
/*
Load the entity referred to by expr_idx (currently, this handles a "named
declaration" or a "template id") and return a symbol entry for it.  If any
error occurs, return NULL.
*/
{
  a_symbol_ptr  result = NULL;

  switch (expr_idx.sort) {
    case ifc_es_expr_named_decl:
      { Opt<an_ifc_expr_named_decl> opt_named_decl;
        construct_node(&opt_named_decl, expr_idx);
        if (!opt_named_decl.has_value()) goto invalid;

        an_ifc_expr_named_decl named_decl = *opt_named_decl;
        an_ifc_decl_index      resolution = get_ifc_resolution(named_decl);
        result = symbol_for_decl_index(resolution);
      }
      break;
    case ifc_es_expr_template_id:
      { a_symbol_ptr                  templ_sym = NULL;
        Opt<an_ifc_expr_template_id>  opt_template_id;
        construct_node(&opt_template_id, expr_idx);
        if (!opt_template_id.has_value()) goto invalid;

        /* Obtain the primary template and resolve it to a front end symbol. */
        an_ifc_expr_template_id  template_id = *opt_template_id;
        an_ifc_expr_index        primary = get_ifc_primary(template_id);
        an_ifc_module            *mod = primary.mod;
        templ_sym = load_ifc_entity_ref(primary);
        if (templ_sym == NULL) goto invalid;

        /* Construct a token cache with the template arguments. */
        a_module_token_cache        arg_cache;
        an_ifc_source_location      locus = get_ifc_locus(template_id);
        an_ifc_source_position_hint pos_hint(&arg_cache, locus);
        a_template_arg_ptr          t_args = NULL;
        an_ifc_expr_index           args = get_ifc_arguments(template_id);
        a_boolean                   err = FALSE;
        long                        first_defaulted_arg = -1;
        if (!is_null_index(args)) {
          mod->cache_expr(&arg_cache, args, /*cinfo=*/{});
        }  /* if */
        cache_token(&arg_cache, tok_gt);
        if (!arg_cache.is_valid()) {
          goto invalid;
        }  /* if */
        {
          a_module_entity_rescan rescan(&arg_cache);

          t_args = scan_template_argument_list(templ_sym,
                                               /*type_constraint=*/FALSE, &err,
                                               GID_NO_OPTIONS,
                                               &first_defaulted_arg);
        }
        /* Apply the template argument list to the template symbol to obtain an
           instance symbol. */
        if (is_class_template_symbol(templ_sym)) {
          result = find_template_class(templ_sym, &t_args,
                                       /*any_prototype_allowed=*/FALSE,
                                       (a_symbol_ptr)NULL,
                                       /*instantiate_nonreal=*/FALSE,
                                       /*do_not_create=*/FALSE,
                                       /*in_substitution=*/FALSE);
        } else if (symbol_is(templ_sym, sk_function_template)) {
          result = find_template_function(templ_sym, &t_args,
                                          /*explicit_arg_list_present=*/TRUE,
                                          pos_hint.as_pos());
        } else {
          /* FIXME: Handle other template kinds. */
          goto invalid;
        }  /* if */
      }
      break;
    default:
      issue_unsupported_construct_error(expr_idx.mod, "ExprIndex entity",
                                        &error_position);
      break;
  }  /* switch */
invalid:
  return result;
}  /* load_ifc_entity_ref */


a_symbol_ptr load_tok_ifc_entity_ref(void)
/*
A wrapper for load_ifc_entity_ref that uses the current token as a source for
an IFC expression index (that token should be a tok_ifc_entity_ref).
*/
{
  a_lexical_ifc_index_reference
                     *idx = &ifc_index_for_curr_token;
  an_ifc_module*     mod = (an_ifc_module*)idx->module;
  an_ifc_expr_index  expr_idx = from_lexical_index<an_ifc_expr_index>(*idx);
  a_source_position  pos = pos_curr_token;
  a_symbol_ptr       result = load_ifc_entity_ref(expr_idx);

  if (result == NULL) {
    /* Something went wrong loading the IFC representation. */
    a_diagnostic_ptr            diag = pos_st_start_error(
                                                 ec_ifc_entity_ref_failure,
                                                 &pos,
                                                 mod->assoc_module_info->name);
    an_ifc_partition_kind       kind = to_partition_kind(expr_idx.sort);
    an_ifc_partition_kind_index part_kind_idx = {mod, kind, expr_idx.value};
    an_ifc_partition_metadata   *part_meta =
                                         get_partition_metadata(part_kind_idx);
    size_t                      part_start = part_meta->offset;
    size_t                      abs_offset =
                                           get_partition_offset(part_kind_idx);
    size_t                      rel_offset = abs_offset - part_start;

    /* FIXME: Migrate to allowing diagnostics with size_t. */
    st_num3_add_diag_info(diag, ec_ifc_entity_ref_failure_info,
                          get_partition_name_from_kind(kind), expr_idx.value,
                          (uint32_t)abs_offset, (uint32_t)rel_offset);
    end_diagnostic(diag);
  }  /* if */
  return result;
}  /* load_tok_ifc_entity_ref */


a_symbol_ptr load_tok_ifc_template_param(void)
/*
The current token is tok_ifc_template_param, which encodes template parameter
coordinates.  Return the symbol for the corresponding parameter.
*/
{
  a_lexical_ifc_index_reference
                     *idx = &ifc_index_for_curr_token;
  an_ifc_decl_index  decl_idx = from_lexical_index<an_ifc_decl_index>(*idx);
  a_symbol_ptr       result = NULL;
  Opt<an_ifc_decl_parameter>
                     opt_idp;

  construct_node(&opt_idp, decl_idx);
  if (opt_idp.has_value()) {
    an_ifc_decl_parameter      idp = *opt_idp;
    a_template_nesting_depth   pdepth = get_ifc_level(idp);
    a_template_param_list_pos  pnum = get_ifc_position(idp);
    a_scope_depth              sd = depth_scope_stack;
    /* We currently have no structure that maps parameter coordinates to the
       parameter representation.  We therefore just search the scope stack for
       the required information. */
    do {
      a_template_param_ptr  tpp = NULL;
      a_template_decl_info  *tdip = scope_stack[sd].template_decl_info;
      a_symbol_ptr          tsym = scope_stack[sd].template_sym;
      /* In some cases, the parameters can be retrieved from the template
         declaration scope (via tdip) and in some cases from the associated
         template symbol. */
      if (tdip != NULL) {
        tpp = tdip->parameters;
      } else if (tsym != NULL) {
        tpp = templ_params_of(tsym);
      }  /* if */
      if (tpp != NULL) {
        a_template_param_coordinate_ptr  coord;
        coord = coordinates_of_template_param(tpp);
        if (coord->depth == pdepth) {
          for (; tpp != NULL; tpp = tpp->next) {
            if (tpp->param_num == pnum) {
              result = tpp->param_symbol;
              goto done;
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
    } while (--sd != DEPTH_OF_FILE_SCOPE);
  }  /* if */
done:
  return result;
}  /* load_tok_ifc_template_param */

template<typename an_ifc_Index_type>
static void cache_token_with_index(a_module_token_cache_ptr cache,
                                   a_token_kind             tok_to_cache,
                                   an_ifc_Index_type        idx)
/*
Add tok_to_cache to cache and associate it with index.
*/
{
  cache_token(cache, tok_to_cache);

  a_cached_token_ptr last_token = cache->get_last_token();
  last_token->extra_info_kind = teik_ifc_index;
  last_token->variant.ifc_index = to_lexical_index(idx);
}  /* cache_token_with_index */


template<typename an_ifc_Node_type>
static a_boolean cache_decl_stmt(a_module_token_cache_ptr cache,
                                 const an_ifc_Node_type   &node)
/*
Cache the given stmt.decl (StmtDecl) or stmt.variable (StmtVariableDecl) node
into the given cache.  Return TRUE if caching succeeded; otherwise, return
FALSE.
*/
{
  a_boolean                 result = FALSE;
  Opt<an_ifc_decl_variable> opt_idv;
  an_ifc_decl_index         decl_idx = get_ifc_decl(node);

  construct_node(&opt_idv, decl_idx);
  if (opt_idv.has_value()) {
    an_ifc_module               *mod = node.get_module();
    an_ifc_decl_variable        idv = *opt_idv;
    an_ifc_source_location      locus = get_ifc_locus(idv);
    an_ifc_source_position_hint pos_hint(cache, locus);

    mod->cache_variable_decl(cache, decl_idx, /*is_class_member=*/FALSE,
                             get_ifc_specifiers(idv), get_ifc_traits(idv),
                             get_ifc_alignment(idv), get_ifc_type(idv),
                             get_ifc_name(idv), an_ifc_text_offset{},
                             an_ifc_expr_index{}, /*get_ifc_initializer(node)*/
                             an_ifc_expr_index{});
    result = TRUE;
  }  /* if */
  return result;
}  /* cache_decl_stmt */


void an_ifc_module::cache_statement(a_module_token_cache_ptr cache,
                                    an_ifc_stmt_index        stmt_idx,
                                    const an_ifc_cache_info  &cinfo)
/*
Add tokens corresponding to the statement at stmt_idx to the given cache.
cinfo contains information about the current cache context to help inform
decisions about what to cache.  For example, if cinfo.func_body is TRUE, the
function is being called for the top-level statement of a function: In the IFC
representation that is not always a compound statement (ifc_StmtSort_Block) and
therefore the caller takes responsibility for generating braces in that case
(i.e., when cinfo.func_body is TRUE, this routine does not cache delimiting
braces for a compound statement).
*/
{
  an_ifc_source_position_hint pos_hint(cache, stmt_idx);

  switch (stmt_idx.sort) {
    case ifc_ss_stmt_vendor_extension:
      issue_unsupported_construct_error(this, "StmtSort::VendorExtension",
                                        &error_position);
      break;
    case ifc_ss_stmt_empty:
      { Opt<an_ifc_stmt_empty> opt_ise;

        construct_node(&opt_ise, stmt_idx);
        if (!opt_ise.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ss_stmt_if:
      { Opt<an_ifc_stmt_if> opt_isi;

        construct_node(&opt_isi, stmt_idx);
        if (!opt_isi.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_stmt_if    isi = *opt_isi;
        an_ifc_stmt_index alternative = get_ifc_alternative(isi);
        an_ifc_stmt_index initialization = get_ifc_initialization(isi);

        cache_token(cache, tok_if);
        cache_token(cache, tok_lparen);
        if (!is_null_index(initialization)) {
          /* FIXME: An IFC bug has these statements wrapped with an extraneous
             StmtSort::Block.  Suppress the braces for the block.  Note that
             this is a brittle approach that has the potential to break things,
             so it should be removed ASAP. */
          an_ifc_cache_info cache_info = cinfo;
          cache_info.func_body = TRUE;
          cache_statement(cache, initialization, cache_info);
        }  /* if */
        { an_ifc_cache_info cache_info = cinfo;
          cache_info.no_final_semicolon = TRUE;
          /* FIXME: Temporary workaround: Suppress braces for an intervening
             StmtSort::Block.  See the above comment for details. */
          cache_info.func_body = TRUE;
          cache_statement(cache, get_ifc_condition(isi), cache_info);
        }
        cache_token(cache, tok_rparen);
        /* FIXME: These also have the intervening StmtSort::Block issue, but
           they do not cause parsing issues - leave them be. */
        cache_statement(cache, get_ifc_consequence(isi), cinfo);
        if (!is_null_index(alternative)) {
          cache_token(cache, tok_else);
          cache_statement(cache, alternative, cinfo);
        }  /* if */
      }
      break;
    case ifc_ss_stmt_for:
      { Opt<an_ifc_stmt_for> opt_isf;

        construct_node(&opt_isf, stmt_idx);
        if (!opt_isf.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_for);
        cache_token(cache, tok_lparen);
        cache_statement(cache, get_ifc_initialization(*opt_isf), cinfo);
        cache_statement(cache, get_ifc_condition(*opt_isf), cinfo);
        { an_ifc_cache_info cache_info = cinfo;
          cache_info.no_final_semicolon = TRUE;
          cache_statement(cache, get_ifc_continuation(*opt_isf), cache_info);
        }
        cache_token(cache, tok_rparen);
        cache_statement(cache, get_ifc_body(*opt_isf), cinfo);
      }
      break;
    case ifc_ss_stmt_goto:
      issue_unsupported_construct_error(this, "StmtSort::Goto",
                                        &error_position);
      break;
    case ifc_ss_stmt_handler:
      issue_unsupported_construct_error(this, "StmtSort::Handler",
                                        &error_position);
      break;
    case ifc_ss_stmt_case:
      { Opt<an_ifc_stmt_case> opt_isc;

        construct_node(&opt_isc, stmt_idx);
        if (!opt_isc.has_value()) {
          goto invalid;
        }  /* if */
        cache_expr(cache, get_ifc_expr(*opt_isc), cinfo);
        cache_token(cache, tok_colon);
      }
      break;
    case ifc_ss_stmt_while:
      { Opt<an_ifc_stmt_while> opt_isw;

        construct_node(&opt_isw, stmt_idx);
        if (!opt_isw.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_while);
        cache_token(cache, tok_lparen);
        { an_ifc_cache_info cache_info = cinfo;
          cache_info.no_final_semicolon = TRUE;
          cache_statement(cache, get_ifc_condition(*opt_isw), cache_info);
        }
        cache_token(cache, tok_rparen);
        cache_statement(cache, get_ifc_body(*opt_isw), cinfo);
      }
      break;
    case ifc_ss_stmt_block:
      { Opt<an_ifc_stmt_block> opt_isb;

        construct_node(&opt_isb, stmt_idx);
        if (!opt_isb.has_value()) {
          goto invalid;
        }  /* if */

        a_stmt_heap_traverser traverser(*opt_isb);
        if (!cinfo.func_body) {
          cache_token(cache, tok_lbrace);
        }  /* if */
        { an_ifc_cache_info cache_info = cinfo;
          cache_info.func_body = FALSE;
          for (an_Indexed<an_ifc_heap_stmt> indexed_ihs : traverser) {
            if (!indexed_ihs.has_value()) {
              goto invalid;
            }  /* if */

            an_ifc_stmt_index value = get_ifc_value(*indexed_ihs);
            /* IFC files sometimes have a NULL statement in this list - don't
               attempt to cache these. */
            if (is_null_index(value)) continue;
            cache_statement(cache, value, cache_info);
          }  /* for */
        }
        if (!cinfo.func_body) {
          cache_token(cache, tok_rbrace);
        }  /* if */
      }
      break;
    case ifc_ss_stmt_break:
      { Opt<an_ifc_stmt_break> opt_isb;

        construct_node(&opt_isb, stmt_idx);
        if (!opt_isb.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_break);
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ss_stmt_switch:
      { Opt<an_ifc_stmt_switch> opt_iss;

        construct_node(&opt_iss, stmt_idx);
        if (!opt_iss.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_switch);
        cache_token(cache, tok_lparen);
        cache_expr(cache, get_ifc_condition(*opt_iss), cinfo);
        cache_token(cache, tok_rparen);
        cache_statement(cache, get_ifc_body(*opt_iss), cinfo);
      }
      break;
    case ifc_ss_stmt_do_while:
      { Opt<an_ifc_stmt_do_while> opt_isdw;

        construct_node(&opt_isdw, stmt_idx);
        if (!opt_isdw.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_do);
        cache_statement(cache, get_ifc_body(*opt_isdw), cinfo);
        cache_token(cache, tok_while);
        cache_token(cache, tok_lparen);
        cache_token(cache, tok_rparen);
        { an_ifc_cache_info cache_info = cinfo;
          cache_info.no_final_semicolon = TRUE;
          cache_statement(cache, get_ifc_condition(*opt_isdw), cache_info);
        }
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ss_stmt_decl:
      { Opt<an_ifc_stmt_decl> opt_isd;

        construct_node(&opt_isd, stmt_idx);
        if (!opt_isd.has_value()) {
          goto invalid;
        }  /* if */
        if (!cache_decl_stmt(cache, *opt_isd)) {
          goto invalid;
        }  /* if */
      }
      break;
    case ifc_ss_stmt_default:
      { Opt<an_ifc_stmt_default> opt_isd;

        construct_node(&opt_isd, stmt_idx);
        if (!opt_isd.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_default);
        cache_token(cache, tok_colon);
      }
      break;
    case ifc_ss_stmt_continue:
      { Opt<an_ifc_stmt_continue> opt_isc;

        construct_node(&opt_isc, stmt_idx);
        if (!opt_isc.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_continue);
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ss_stmt_expression:
      { Opt<an_ifc_stmt_expression> opt_ise;

        construct_node(&opt_ise, stmt_idx);
        if (!opt_ise.has_value()) {
          goto invalid;
        }  /* if */
        cache_expr(cache, get_ifc_expr(*opt_ise), cinfo);
        if (!cinfo.no_final_semicolon) {
          cache_token(cache, tok_semicolon);
        }  /* if */
      }
      break;
    case ifc_ss_stmt_labeled:
      issue_unsupported_construct_error(this, "StmtSort::Labeled",
                                        &error_position);
      break;
    case ifc_ss_stmt_return:
      { Opt<an_ifc_stmt_return> opt_isr;

        construct_node(&opt_isr, stmt_idx);
        if (!opt_isr.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_index expr = get_ifc_expr(*opt_isr);
        cache_token(cache, tok_return);
        if (!is_null_index(expr)) {
          cache_expr(cache, expr, cinfo);
        }  /* if */
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ss_stmt_tuple:
      issue_unsupported_construct_error(this, "StmtSort::Tuple",
                                        &error_position);
      break;
    case ifc_ss_stmt_variable_decl:
      { Opt<an_ifc_stmt_variable_decl> opt_isvd;

        construct_node(&opt_isvd, stmt_idx);
        if (!opt_isvd.has_value()) {
          goto invalid;
        }  /* if */
        if (!cache_decl_stmt(cache, *opt_isvd)) {
          goto invalid;
        }  /* if */
      }
      break;
    case ifc_ss_stmt_expansion:
      { Opt<an_ifc_stmt_expansion> opt_ise;

        construct_node(&opt_ise, stmt_idx);
        if (!opt_ise.has_value()) {
          goto invalid;
        }  /* if */
        issue_unsupported_construct_error(this, "StmtSort::Expansion",
                                          &error_position);
      }
      break;
    case ifc_ss_stmt_syntax_tree:
      issue_unsupported_construct_error(this, "StmtSort::SyntaxTree",
                                        &error_position);
      break;
    default_is_unexpected_str("Unknown StmtSort kind");
  }  /* switch */
  goto done;
invalid:
  check_assertion_str(is_at_least_one_error(),
                      "expected errors for bad statement cache");
  cache->invalidate();
done:;
}  /* cache_statement */


template<typename an_ifc_Node_type>
static
a_boolean cache_direct_decl(a_module_token_cache_ptr cache,
                            const an_ifc_Node_type   &node,
                            const an_ifc_cache_info  &cinfo) DELETED_FN_DEF


template<>
a_boolean cache_direct_decl(a_module_token_cache_ptr  cache,
                            const an_ifc_decl_concept &idc,
                            const an_ifc_cache_info   &cinfo)
/*
Add the given decl concept into the cache.  cinfo contains information about
the current cache context to help inform decisions about what to cache.  Return
TRUE if caching succeeds, FALSE otherwise.
*/
{
  an_ifc_module               *mod = idc.get_module();
  an_ifc_source_location      locus = get_ifc_locus(idc);
  an_ifc_source_position_hint pos_hint(cache, locus);

  /* Generate the template parameter list. */
  cache_token(cache, tok_template);
  mod->cache_chart(cache, get_ifc_chart(idc), cinfo);
  /* Generate "concept <concept-name>". */
  mod->cache_sentence(cache, get_ifc_head(idc));
  if (cinfo.ignore_definition) {
    cache_token(cache, tok_semicolon);
  } else {
    /* Generate "= <constraint-expression> ;". */
    mod->cache_sentence(cache, get_ifc_body(idc));
  }  /* if */
  return TRUE;
}  /* cache_direct_decl */


template<>
a_boolean cache_direct_decl(a_module_token_cache_ptr     cache,
                            const an_ifc_decl_enumerator &ide,
                            const an_ifc_cache_info      &cinfo)
/*
Add the given decl enumerator into the cache.  cinfo contains information about
the current cache context to help inform decisions about what to cache.  Return
TRUE if caching succeeds, FALSE otherwise.
*/
{
  /* An enumerator declaration is part of an enumeration declaration.
     The type, access, and specifiers should all be handled by the parent
     declaration. */
  an_ifc_source_location      locus = get_ifc_locus(ide);
  an_ifc_source_position_hint pos_hint(cache, locus);
  an_ifc_expr_index           initializer = get_ifc_initializer(ide);

  cache_identifier(cache, get_string_at_offset(get_ifc_name(ide)));
  if (!is_null_index(initializer)) {
    cache_token(cache, tok_assign);
    ide.get_module()->cache_expr(cache, initializer, /*cinfo=*/{});
  }  /* if */
  return TRUE;
}  /* cache_direct_decl */


template<>
a_boolean cache_direct_decl(a_module_token_cache_ptr    cache,
                            const an_ifc_decl_parameter &idp,
                            const an_ifc_cache_info     &cinfo)
/*
Add the given decl parameter into the cache.  cinfo contains information about
the current cache context to help inform decisions about what to cache.  Return
TRUE if caching succeeds, FALSE otherwise.
*/
{
  an_ifc_module               *mod = idp.get_module();
  an_ifc_source_location      locus = get_ifc_locus(idp);
  an_ifc_source_position_hint pos_hint(cache, locus);
  an_ifc_type_index           type = get_ifc_type(idp);
  an_ifc_text_offset          name = get_ifc_name(idp);
  an_ifc_expr_index           initializer = get_ifc_initializer(idp);
  an_ifc_parameter_sort       param_sort = get_ifc_sort(idp);
  a_boolean                   need_second_pass = FALSE;
  a_boolean                   defer_initializer_expr = FALSE;

  switch (param_sort) {
    case ifc_ps_type:
      { a_boolean is_pack = type.sort == ifc_ts_type_expansion;

        mod->cache_type_param_introducer(cache, get_ifc_constraint(idp),
                                         is_pack);
      }
      break;
    case ifc_ps_object:
      /* This is a function parameter rather than a template parameter, but is
         handled the same as non-type template parameters.  We do need to
         apply special handling to defer the initializer (if one exists),
         however, as the default argument expression could reference a parent
         class that's in the process of being completed. */
      defer_initializer_expr = TRUE;
      FALLTHROUGH
    case ifc_ps_non_type:
      mod->cache_type_first_part(cache, type);
      need_second_pass = TRUE;
      break;
    case ifc_ps_template:
      if (is_null_index(type)) {
        /* FIXME: Confirm this is actually what is implied by a NULL
           type. */
        cache_token(cache, tok_template);
        cache_token(cache, tok_lt);
        cache_token(cache, tok_typename);
        cache_token(cache, tok_ellipsis);
        cache_token(cache, tok_gt);
        cache_token(cache, tok_typename);
      } else {
        mod->cache_type(cache, type);
      }  /* if */
      break;
    default_is_unexpected_str("Unexpected ParameterSort");
  }  /* switch */
  if (name != 0) {
    cache_identifier(cache, get_string_at_offset(name));
  }  /* if */
  if (need_second_pass) {
    mod->cache_type_second_part(cache, type);
  }  /* if */

  a_boolean is_template_param = param_sort != ifc_ps_object;
  if (!is_null_index(initializer) &&
      ((!cinfo.ignore_default_arguments && !is_template_param) ||
       (!cinfo.ignore_default_template_arguments && is_template_param))) {
    cache_token(cache, tok_assign);
    if (defer_initializer_expr) {
      cache_token_with_index(cache, tok_pending_ifc_expr, initializer);
    } else {
      mod->cache_expr(cache, initializer, /*cinfo=*/{});
    }  /* if */
  }  /* if */
  return TRUE;
}  /* cache_direct_decl */


template<>
a_boolean cache_direct_decl(a_module_token_cache_ptr   cache,
                            const an_ifc_decl_temploid &idt,
                            const an_ifc_cache_info    &cinfo)
/*
Add the given decl temploid into the cache.  cinfo contains information about
the current cache context to help inform decisions about what to cache.  Return
TRUE if caching succeeds, FALSE otherwise.
*/
{
  an_ifc_module *mod = idt.get_module();

  /* FIXME: Currently unsupported. */
  issue_unsupported_construct_error(mod, "DeclSort::Temploid",
                                    &error_position);
  return FALSE;
}  /* cache_direct_decl */

/* FIXME: This code should be transition an_ifc_func_param_context (and
   an_ifc_func_param_context updated in the process) to improve the overall
   quality of IFC parameter handling. */

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


static a_boolean check_parameter_counts(
                               a_routine_ptr              rp,
                               an_ifc_cardinality_storage chart_param_count,
                               unsigned                   decl_param_count)
/*
Check for a mismatch between the number of parameters declared by the IFC
parameter chart and the number of parameters declared by the type.  rp is the
IL routine for which parameter counts are being checked.  chart_param_count is
the number of parameters specified by the IFC function definition.
decl_param_count is the number of parameters specified by the IL function
type's parameters (which corresponds to the IFC declaration's parameter count
information).  Return TRUE if the parameter counts match, return FALSE
otherwise.
*/
{
  a_boolean result = TRUE;

  if (chart_param_count != decl_param_count) {
    a_diagnostic_ptr diag_ptr = start_rp_diag(rp);

    add_bad_parameter_count_info(diag_ptr, chart_param_count,
                                 decl_param_count);
    end_diagnostic(diag_ptr);
    result = FALSE;
  }  /* if */
  return result;
}  /* check_parameter_counts */


static a_boolean is_bad_ifc_parameter(const an_ifc_decl_parameter &param)
/*
Given an IFC parameter, check to see if the parameter has defects that suggest
it should be skipped.
*/
{
  a_boolean    result = FALSE;
  a_const_char *name = get_string_at_offset(get_ifc_name(param));
  size_t       name_len = strlen(name);

  switch (name_len) {
    case 4:
      if (strcmp(name, "this") == 0) {
        result = TRUE;
      }  /* if */
      break;
    case 12:
      if (strcmp(name, "__$ReturnUdt") == 0) {
        result = TRUE;
      }  /* if */
      break;
    default:
      break;
  }  /* switch */
  return result;
}  /* is_bad_ifc_parameter */


static a_boolean check_for_param_count_correction(
                                        an_ifc_chart_unilevel icul,
                                        unsigned              decl_param_count)
/*
Check to see if one or more of the parameters specified by icul is a bad
(implicitly generated as part of the calling convention) parameter.  Return
TRUE if dropping these parameters would correct a parameter count mismatch;
otherwise, return FALSE.
*/
{
  a_boolean                  valid_data = TRUE;
  an_ifc_cardinality_storage used_param_count = 0;
  a_decl_parameter_traverser traverser(icul);

  for (an_Indexed<an_ifc_decl_parameter> indexed_idp : traverser) {
    if (!indexed_idp.has_value()) {
      valid_data = FALSE;
      break;
    }  /* if */
    if (is_bad_ifc_parameter(*indexed_idp)) {
      continue;
    }  /* if */
    ++used_param_count;
  }  /* for */
  return valid_data && used_param_count == decl_param_count;
}  /* check_for_param_count_correction */


static a_boolean check_parameter_counts(
                               a_routine_ptr         rp,
                               an_ifc_chart_unilevel icul,
                               a_param_type_ptr      params,
                               a_boolean             *perform_param_correction)
/*
Check for a mismatch between the number of parameters declared by the IFC
parameter chart and the number of parameters declared by the type.  rp is the
IL routine for which parameter counts are being checked.  icul is the unilevel
chart being considered that defines the definition's parameters.  params is the
first param in the list of the function type's parameters (which corresponds to
the IFC declaration's parameter count information).  perform_param_correction
is a pointer to a boolean that will be set to TRUE if the parameter processing
logic should check for bad parameters and omit them.  Return TRUE if the
parameter counts are equivalent or a correction can be applied to make them
equivalent; otherwise, return FALSE.
*/
{
  a_boolean                  result = TRUE;
  an_ifc_cardinality_storage chart_param_count = get_ifc_cardinality(icul);
  unsigned                   decl_param_count = count_list_elements(params);

  if (chart_param_count != decl_param_count) {
    /* FIXME: This is a hack to work around an IFC defect. */
    if (check_for_param_count_correction(icul, decl_param_count)) {
      /* The front end has determined that dropping problematic parameters will
         correct this mismatch.  Create a diagnostic warning about what's going
         to happen. */
      a_diagnostic_ptr diag_ptr = start_rp_diag(rp, es_warning);

      add_bad_parameter_count_info(diag_ptr, chart_param_count,
                                   decl_param_count);

      a_decl_parameter_traverser traverser(icul);
      for (an_Indexed<an_ifc_decl_parameter> indexed_idp : traverser) {
        /* Already constructed by check_for_param_count_correction, which
           fails if there was a problem. */
        check_assertion(indexed_idp.has_value());
        if (is_bad_ifc_parameter(*indexed_idp)) {
          an_ifc_text_offset name_idx = get_ifc_name(*indexed_idp);
          a_const_char       *name = get_string_at_offset(name_idx);
          an_ifc_index_type  relative_idx = get_relative_index(traverser,
                                                               indexed_idp);

          st_num_add_diag_info(diag_ptr, ec_ifc_bad_function_param_name, name,
                               relative_idx);
        }  /* if */
      }  /* if */
      end_diagnostic(diag_ptr);
      *perform_param_correction = TRUE;
    } else {
      a_diagnostic_ptr diag_ptr = start_rp_diag(rp);

      add_bad_parameter_count_info(diag_ptr, chart_param_count,
                                   decl_param_count);
      end_diagnostic(diag_ptr);
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* check_parameter_counts */


static void add_function_def_parameter(
                                    const an_ifc_decl_parameter &idp,
                                    a_param_type_ptr            ptp,
                                    a_func_info_block           *func_info,
                                    a_param_id_ptr              *last_param_id)
/*
Add the given IFC parameter, using the associated parameter type pointer, to
the given function info.  last_param_id is a pointer to the pointer for the
latest addition to the parameter list; if said list is empty, the pointed to
pointer should be NULL.
*/
{
  a_source_position pos;

  check_assertion(get_ifc_sort(idp) == ifc_ps_object);
  idp.get_module()->source_position_from_locus(&pos, get_ifc_locus(idp));

  a_const_char     *name = get_string_at_offset(get_ifc_name(idp));
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
}  /* add_function_def_parameter */


static a_boolean add_function_def_parameters(
                             const an_ifc_trait_function_definition &itfd,
                             a_routine_ptr                          rp,
                             a_func_info_block                      *func_info)
/*
Add the parameters, for the given IFC function definition, and associated
routine pointer, to the given function info.  Return TRUE if all parameters are
added successfully, return FALSE otherwise.
*/
{
  a_boolean          result = TRUE;
  an_ifc_chart_index chart_params = get_ifc_parameters(itfd);
  a_param_type_ptr   params = function_type_params(rp->type);

  /* Parameters are represented as a uni-level IFC "chart" pointing to a
     sequence of ifc_DeclSort_Parameter entries of kind
     ifc_ParameterSort_Object.  Check for the parameter chart. */
  if (chart_params.sort == ifc_cs_chart_unilevel) {
    Opt<an_ifc_chart_unilevel> opt_icul;

    construct_node(&opt_icul, chart_params);
    /* Read the uni-level chart of parameters. */
    if (!opt_icul.has_value()) {
      result = FALSE;
      goto done;
    }  /* if */

    an_ifc_chart_unilevel icul = *opt_icul;
    a_boolean             perform_param_correction = FALSE;
    if (!check_parameter_counts(rp, icul, params, &perform_param_correction)) {
      result = FALSE;
      goto done;
    }  /* if */
    /* Ensure a function prototype scope exists in which sk_parameter
       symbols can be accumulated. */
    (void)push_scope((a_scope_kind)sck_func_prototype, NO_SCOPE_NUMBER,
                     rp->type, (a_routine_ptr)NULL);

    a_param_type_ptr  ptp = params;
    func_info->scope_number = scope_stack_top().number;

    a_param_id_ptr             last_param_id = nullptr;
    a_decl_parameter_traverser traverser(icul);
    for (an_Indexed<an_ifc_decl_parameter> indexed_idp : traverser) {
      if (!indexed_idp.has_value()) {
        result = FALSE;
        goto done;
      }  /* if */
      /* FIXME: This is a hack to work around an IFC defect. */
      if (perform_param_correction && is_bad_ifc_parameter(*indexed_idp)) {
        continue;
      }  /* if */
      add_function_def_parameter(*indexed_idp, ptp, func_info, &last_param_id);
      ptp = ptp->next;
    }  /* for */
    func_info->prototype_scope_symbols =
                          assoc_pointers_block_of(&scope_stack_top())->symbols;
    pop_scope();
  } else if (chart_params.sort == ifc_cs_chart_none) {
    /* The associated chart is empty, verify the type information also isn't
       specifying parameters. */
    if (!check_parameter_counts(rp, 0, count_list_elements(params))) {
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


a_boolean an_ifc_module::cache_function_body(
                                           a_module_token_cache_ptr cache,
                                           an_ifc_decl_index        decl_idx,
                                           a_routine_ptr            rp,
                                           a_func_info_block        *func_info)
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
  a_boolean                             result = TRUE;
  Opt<an_ifc_trait_function_definition> opt_itfd;

  check_assertion(type_is(rp->type, tk_routine));
  find_trait(&opt_itfd, decl_idx);
  if (opt_itfd.has_value()) {
    an_ifc_trait_function_definition itfd = *opt_itfd;

    /* A definition exists, mark that. */
    func_info->is_definition = TRUE;
    /* Add the parameters to the function info. */
    if (!add_function_def_parameters(itfd, rp, func_info)) {
      result = FALSE;
      goto done;
    }  /* if */

    an_ifc_expr_index initializers = get_ifc_initializers(itfd);
    an_ifc_stmt_index body = get_ifc_body(itfd);
    /* Cache the mem-initializers if needed. */
    if (!is_null_index(initializers)) {
      cache_token(cache, tok_colon);
      cache_expr(cache, initializers, /*cinfo=*/{});
    }  /* if */
    /* Cache the function body.  It appears that a single return statement is
       represented directly rather than as a block containing the return
       statement.  We therefore generate the braces here and inhibit them at
       the next statement level by passing the cso_func_body flag. */
    cache_token(cache, tok_lbrace);
    if (!is_null_index(body)) {
      an_ifc_cache_info cache_info;
      cache_info.func_body = TRUE;
      cache_statement(cache, body, cache_info);
    }  /* if */
    cache_token(cache, tok_rbrace);
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

namespace {

using an_ifc_function_failure_map = Ptr_map<a_routine_ptr, a_boolean>;
                        /* The type of a map that pairs a routine with a
                           failure flag state to prevent reprocessing. */

an_ifc_function_failure_map
                *ifc_bad_function_bodies;
                        /* A map from IL routine entry pointers to boolean
                           values.  This is conceptually a set where routines
                           with previously processed (failed) function bodies
                           are stored. */

}  /* namespace */


a_boolean has_routine_definition_from_ifc_module(a_routine_ptr  rp)
/*
If the given routine has a definition in a currently-imported IFC module
return TRUE.
*/
{
  an_ifc_decl_index ifb = ifc_function_bodies->get(rp);

  return ifb.mod != NULL;
}  /* has_routine_definition_from_ifc_module */


a_boolean load_routine_definition_from_ifc_module(a_routine_ptr  rp)
/*
The given routine claims to have a definition in a currently-imported IFC
module; process said definition and return TRUE.  If problems are encountered
during processing, return FALSE.

The presence of a routine definition should be checked for via
has_routine_definition_from_ifc_module prior to attempting to load the routine
definition.
*/
{
  a_boolean  result = FALSE;

  check_assertion(has_routine_definition_from_ifc_module(rp));
  /* Make sure that we don't attempt to process a definition that we're already
     processing, and check the (effective) set of routines that have previously
     failed definition processing.  This prevents repeating errors and
     mitigates the performance impact if a problematic routine is called many
     times. */
  if (!rp->definition_pending && !ifc_bad_function_bodies->get(rp)) {
    an_ifc_decl_index           ifb = ifc_function_bodies->get(rp);
    a_func_info_block           func_info;
    a_module_token_cache        def_cache;
    a_decl_flag_set             flags = SFB_NEW_STRUCT_STMT_STACK_REQUIRED;
    a_curr_token_preserver      guard;
    a_module_entity_stack_state mep_state(get_ifc_module_entity_ptr(ifb));
    a_diagnostic_suppression    diag_suppress(
                                           &ifb.mod->suppressed_diagnostics,
                                           !display_module_import_diagnostics);

    rp->definition_pending = TRUE;
    clear_func_info(&func_info);
    push_new_top_level_declaration();
    if (ifb.mod->cache_function_body(&def_cache, ifb, rp, &func_info)) {
      if (def_cache.is_valid()) {
        a_token_kind           expected_tok = tok_rbrace;
        a_module_entity_rescan rescan(&def_cache, &expected_tok);

        scan_function_body(rp, &func_info, flags);
        if (curr_token == expected_tok) {
          result = TRUE;
          /* We have successfully loaded the definition.  So the "pending
             definition" entry can be dropped now.  Dropping it without
             successful completion will result in errors at call sites. */
          ifc_function_bodies->unmap(rp);
        }  /* if */
      }  /* if */
    }  /* if */
    pop_scope();
    rp->definition_pending = FALSE;
    /* If this function failed to process successfully, mark the failure so we
       don't reenter this branch. */
    if (!result) {
      ifc_bad_function_bodies->map(rp, TRUE);
      check_assertion_str(is_at_least_one_error(),
                          "expected errors for bad function body");
    }  /* if */
  }  /* if */
  return result;
}  /* load_routine_definition_from_ifc_module */

namespace {

/*
An enum used to encode additional context when processing an IFC type.
*/
enum a_non_type_kind : uint8_t {
  ntk_none,
  ntk_ellipsis,
  ntk_namespace,
  ntk_empty_pack_expansion,
};

a_type_ptr type_for_type_index(an_ifc_type_index type_index,
                               a_non_type_kind   *kind);

a_type_kind type_kind_for_type_index(an_ifc_type_index type_idx)
/*
Given a type index, return the corresponding type kind.
*/
{
  a_type_kind      result = tk_unknown;
  an_ifc_type_sort sort = type_idx.sort;

  switch (sort) {
    case ifc_ts_type_fundamental:
      { Opt<an_ifc_type_fundamental> opt_itf;

        construct_node(&opt_itf, type_idx);
        if (!opt_itf.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_fundamental itf = *opt_itf;
        an_ifc_type_basis_sort  basis = get_ifc_basis(itf);
        switch (basis) {
          case ifc_tbs_ellipsis:
          case ifc_tbs_auto:
          case ifc_tbs_decltype_auto:
          case ifc_tbs_namespace:
          case ifc_tbs_interface:
          case ifc_tbs_segment_type:
          case ifc_tbs_empty:
          case ifc_tbs_concept:
            { a_string err_msg("Unexpected ", str_for(basis));

              ifc_unexpected(itf.get_module(), err_msg);
            }
            goto invalid;
          case ifc_tbs_void:
            result = tk_void;
            break;
          case ifc_tbs_bool:
          case ifc_tbs_char:
          case ifc_tbs_wchar_t:
          case ifc_tbs_int:
            result = tk_integer;
            break;
          case ifc_tbs_float:
          case ifc_tbs_double:
            result = tk_float;
            break;
          case ifc_tbs_nullptr:
            result = tk_nullptr;
            break;
          case ifc_tbs_class:
            result = tk_class;
            break;
          case ifc_tbs_struct:
            result = tk_struct;
            break;
          case ifc_tbs_union:
            result = tk_union;
            break;
          case ifc_tbs_enum:
            result = tk_enum;
            break;
          case ifc_tbs_typename:
            result = tk_typeref;
            break;
          case ifc_tbs_function:
          case ifc_tbs_overload:
            result = tk_routine;
            break;
          case ifc_tbs_variable_template:
            result = tk_unknown;
            break;
          default_is_unexpected_str("Unexpected TypeBasis kind");
        }  /* switch */
      }
      break;
    default:
      { a_type_ptr type = type_for_type_index(type_idx, /*kind=*/NULL);

        result = type->kind;
      }
      break;
  }  /* switch */
invalid:;
  return result;
}  /* type_kind_for_type_index */


a_boolean type_represents_ellipsis(an_ifc_type_index type_idx)
/*
Given a type index, return TRUE if the type index represents an ellipsis.
*/
{
  a_boolean result = FALSE;

  if (type_idx.sort == ifc_ts_type_fundamental) {
    Opt<an_ifc_type_fundamental> opt_itf;

    construct_node(&opt_itf, type_idx);
    if (opt_itf.has_value()) {
      an_ifc_type_fundamental itf = *opt_itf;
      an_ifc_type_basis_sort  basis = get_ifc_basis(itf);

      result = basis == ifc_tbs_ellipsis;
    }  /* if */
  }  /* if */
  return result;
}  /* type_represents_ellipsis */


a_boolean type_represents_templ_param_ref(an_ifc_type_index type_idx)
/*
Given a type index, return TRUE if the type index represents a reference to a
type template parameter.
*/
{
  a_boolean result = FALSE;

  if (type_idx.sort == ifc_ts_type_fundamental) {
    Opt<an_ifc_type_fundamental> opt_itf;

    construct_node(&opt_itf, type_idx);
    if (opt_itf.has_value()) {
      an_ifc_type_fundamental itf = *opt_itf;
      an_ifc_type_basis_sort  basis = get_ifc_basis(itf);

      result = basis == ifc_tbs_typename;
    }  /* if */
  }  /* if */
  return result;
}  /* type_represents_templ_param_ref */


a_boolean is_redeclared_basic_entity(a_symbol            *sym,
                                     a_module_entity_ptr mep,
                                     an_il_entry_kind    expected_kind,
                                     char                **redecl_entity,
                                     an_il_entry_kind    *redecl_kind)
/*
For a given module entity's potentially previously declared symbol, module
entity pointer, and expected kind, check to see if the symbol is indeed a
redeclaration.  If the symbol is a redeclaration, return TRUE and set
*redecl_entity and *redecl_kind to the redeclared entity and its associated
kind; otherwise, return FALSE.
*/
{
  a_boolean        result = FALSE;
  an_il_entry_kind kind;
  char             *entity = il_entry_for_symbol_null_okay(sym, &kind);

  if (entity != NULL && kind == expected_kind) {
    a_source_correspondence_ptr scp =
                                     source_corresp_for_il_entry(entity, kind);

    if (scp != NULL) {
      a_scope_ptr scope = get_parent_scope_of(scp);

      if (mep->scope == scope) {
        *redecl_entity = entity;
        *redecl_kind = kind;
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_redeclared_basic_entity */


a_boolean
find_redeclared_basic_entity_in_list(a_symbol            *sym_list,
                                     a_module_entity_ptr mep,
                                     an_il_entry_kind    expected_kind,
                                     char                **redecl_entity,
                                     an_il_entry_kind    *redecl_kind)
/*
For a given module entity's associated symbol list, module entity pointer, and
expected kind, search the symbols for a redeclaration.  If a redeclaration is
found, return TRUE and set *redecl_entity and *redecl_kind to the redeclared
entity and its associated kind; otherwise, return FALSE.

This function should not be used directly in modules code, instead see
check_and_set_redeclaration.
*/
{
  a_boolean result = FALSE;

  for (a_symbol_ptr sym = sym_list; sym != NULL; sym = sym->next) {
    if (is_redeclared_basic_entity(sym, mep, expected_kind, redecl_entity,
                                   redecl_kind)) {
      result = TRUE;
      break;
    }  /* if */
  }  /* if */
  return result;
}  /* find_redeclared_basic_symbol_in_list */


a_boolean find_redeclared_basic_entity(a_symbol_header     *sym_header,
                                       a_module_entity_ptr mep,
                                       an_il_entry_kind    expected_kind,
                                       char                **redecl_entity,
                                       an_il_entry_kind    *redecl_kind)
/*
For a given module entity's symbol header, module entity pointer, and expected
kind, search the active and inactive symbols for a redeclaration.  If a
redeclaration is found, return TRUE and set *redecl_entity and *redecl_kind to
the redeclared entity and its associated kind; otherwise, return FALSE.

This function should not be used directly in modules code, instead see
check_and_set_redeclaration.
*/
{
  a_boolean    result = FALSE;
  a_symbol_ptr active_symbols = sym_header->symbol;
  a_symbol_ptr inactive_symbols = sym_header->inactive_symbols;

  if (find_redeclared_basic_entity_in_list(active_symbols, mep, expected_kind,
                                           redecl_entity, redecl_kind) ||
      find_redeclared_basic_entity_in_list(inactive_symbols, mep,
                                           expected_kind, redecl_entity,
                                           redecl_kind)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* find_redeclared_basic_entity */


a_boolean check_and_set_redeclaration(
                              a_symbol_locator                 *loc,
                              a_module_entity_ptr              mep,
                              ARG_UNUSED a_source_position_ptr pos,
                              an_il_entry_kind                 expected_kind,
                              char                             **redecl_entity,
                              an_il_entry_kind                 *redecl_kind)
/*
Given an IFC module entity's symbol locator, module entity pointer, source
position, and expected kind check for a redeclaration.  If a redeclaration is
found, return TRUE and set *redecl_entity and *redecl_kind to the redeclared
entity and its associated kind; otherwise, return FALSE.

The caller is responsible for handling any required merging of the
declarations.  In the case of duplicate definitions the caller is responsible
for ignoring the latter definition for header units and erroring for named
modules.

This function should generally be avoided for more specific redeclaration
checking functions, as it only has a primitive understanding of what a
redeclaration is.
*/
{
  a_boolean result = find_redeclared_basic_entity(loc->symbol_header, mep,
                                                  expected_kind, redecl_entity,
                                                  redecl_kind);

  if (result) {
    /* This is a redeclaration of an existing symbol. */
    /* FIXME: This should also trigger if the redecl_sym's module is a named
       module. */
#if 0
    if (!is_header_unit(mep->module_info)) {
      pos_error(ec_module_entity_redeclaration, pos);
    }  /* if */
#endif /* 0 */
  }  /* if */
  return result;
}  /* check_and_set_redeclaration */


a_boolean has_matching_func_param(const an_ifc_decl_parameter &mod_templ_param,
                                  a_param_type_ptr            il_param_type)
/*
Return TRUE if the given IFC node information for a parameter declaration
represents the same type as the given IL param type.
*/
{
  an_ifc_type_index ifc_type_idx = get_ifc_type(mod_templ_param);
  a_type_ptr        mod_param_type = type_for_type_index(ifc_type_idx,
                                                         /*kind=*/NULL);

  return il_identical_types(il_param_type->declared_type, mod_param_type);
}  /* has_matching_func_param */


a_boolean is_function_template_redecl(a_module_entity_ptr mep,
                                      a_template_ptr      templ)
/*
For a given module entity's module entity pointer and the potentially
previously-declared template IL entity, check to see if the symbol is indeed a
redeclaration.  Return TRUE if the symbol is a redeclaration; otherwise return
FALSE.
*/
{
  a_boolean         result = TRUE;
  an_ifc_module     *mod = get_assoc_ifc_module(mep);
  an_ifc_decl_index decl_idx = decl_index_of(mep);

  /* The module entity pointer should always refer to a template. */
  check_assertion(decl_idx.sort == ifc_ds_decl_template);

  an_ifc_decl_template decl_templ;
  construct_node_prechecked(&decl_templ, decl_idx);

  an_ifc_decl_index entity_decl_idx = get_ifc_decl(get_ifc_entity(decl_templ));
  if (entity_decl_idx.sort == ifc_ds_decl_function) {
    Opt<an_ifc_decl_function> opt_decl_func;

    construct_node(&opt_decl_func, entity_decl_idx);
    if (!opt_decl_func.has_value()) {
      goto no_match;
    }  /* if */

    an_ifc_decl_function decl_func = *opt_decl_func;
    an_ifc_chart_index   param_idx = get_ifc_chart(decl_func);
    a_routine_ptr        routine = templ->prototype_instantiation.routine;
    a_param_type_ptr     curr = get_routine_param_types(routine);
    if (!is_null_index(param_idx)) {
      switch (param_idx.sort) {
        case ifc_cs_chart_unilevel:
          { Opt<an_ifc_chart_unilevel> opt_params;

            construct_node(&opt_params, param_idx);
            if (opt_params.has_value()) {
              an_ifc_chart_unilevel      params = *opt_params;
              a_decl_parameter_traverser traverser(params);

              for (an_Indexed<an_ifc_decl_parameter> idxd_param : traverser) {
                if (curr == NULL) {
                  /* If there are no more IL template parameters, fail. */
                  goto no_match;
                }  /* if */
                if (!idxd_param.has_value()) {
                  goto no_match;
                }  /* if */

                an_ifc_decl_parameter param = *idxd_param;
                if (!has_matching_func_param(param, curr)) {
                  /* If the template parameter doesn't match, fail. */
                  goto no_match;
                }  /* if */
                curr = curr->next;
              }  /* for */
            }  /* if */
          }
          break;
        case ifc_cs_chart_multilevel:
        case ifc_cs_chart_none:
          ifc_unexpected(mod, "only unilevel or absent function templates "
                         "are supported for template declarations");
          goto no_match;
        default_is_unexpected();
      }  /* switch */
    }  /* if */
    if (curr != NULL) {
      /* If there are more IL template parameters than module template
         parameters, fail. */
      goto no_match;
    }  /* if */
  }  /* if */
  goto done;
no_match:
  result = FALSE;
done:
  return result;
}  /* is_function_template_redecl */


a_boolean is_redeclared_template_entity(a_symbol            *sym,
                                        a_module_entity_ptr mep,
                                        char                **redecl_entity,
                                        an_il_entry_kind    *redecl_kind)
/*
For a given module entity's potentially previously-declared symbol and module
entity pointer, check to see if the symbol is indeed a redeclaration.  If the
symbol is a redeclaration, return TRUE and set *redecl_entity and *redecl_kind
to the redeclared entity and its associated kind; otherwise, return FALSE.

This function should not be used directly in modules code, instead see
check_and_set_template_redeclaration.
*/
{
  a_boolean        result = FALSE;
  an_il_entry_kind kind;
  char             *entity = il_entry_for_symbol_null_okay(sym, &kind);

  if (entity != NULL && kind == iek_template) {
    a_source_correspondence_ptr scp =
                                     source_corresp_for_il_entry(entity, kind);

    if (scp != NULL) {
      a_scope_ptr scope = get_parent_scope_of(scp);

      if (mep->scope == scope) {
        a_template_ptr  templ = (a_template_ptr)entity;
        a_template_kind templ_kind = templ->kind;

        switch (templ_kind) {
          case templk_none:
            break;
          case templk_class:
          case templk_member_class:
          case templk_member_enum:
          case templk_template_template_param:
          case templk_concept:
          case templk_variable:
          case templk_static_data_member:
            /* Cases that can't be overloaded. */
            result = TRUE;
            break;
          case templk_function:
          case templk_member_function:
            /* Function (potentially) overloaded case. */
            result = is_function_template_redecl(mep, templ);
            break;
          default_is_unexpected();
        }  /* switch */
        if (result) {
          *redecl_entity = entity;
          *redecl_kind = kind;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_redeclared_template_entity */


a_boolean
find_redeclared_template_entity_in_list(a_symbol            *sym_list,
                                        a_module_entity_ptr mep,
                                        char                **redecl_entity,
                                        an_il_entry_kind    *redecl_kind)
/*
For a given module entity's associated symbol list and module entity pointer,
search the symbols for a redeclaration.  If a redeclaration is found, return
TRUE and set *redecl_entity and *redecl_kind to the redeclared entity and its
associated kind; otherwise, return FALSE.

This function should not be used directly in modules code, instead see
check_and_set_template_redeclaration.
*/
{
  a_boolean result = FALSE;

  for (a_symbol_ptr sym = sym_list; sym != NULL; sym = sym->next) {
    if (is_redeclared_template_entity(sym, mep, redecl_entity, redecl_kind)) {
      result = TRUE;
      break;
    }  /* if */
  }  /* if */
  return result;
}  /* find_redeclared_template_entity_in_list */


a_boolean find_redeclared_template_entity(a_symbol_header     *sym_header,
                                          a_module_entity_ptr mep,
                                          char                **redecl_entity,
                                          an_il_entry_kind    *redecl_kind)
/*
For a given module entity's symbol header and module entity pointer, search the
active and inactive symbols for a redeclaration.  If a redeclaration is found,
return TRUE and set *redecl_entity and *redecl_kind to the redeclared entity
and its associated kind; otherwise, return FALSE.

This function should not be used directly in modules code, instead see
check_and_set_template_redeclaration.
*/
{
  a_boolean    result = FALSE;
  a_symbol_ptr active_symbols = sym_header->symbol;
  a_symbol_ptr inactive_symbols = sym_header->inactive_symbols;

  if (find_redeclared_template_entity_in_list(active_symbols, mep,
                                              redecl_entity, redecl_kind) ||
      find_redeclared_template_entity_in_list(inactive_symbols, mep,
                                              redecl_entity, redecl_kind)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* find_redeclared_template_entity */


a_boolean check_and_set_template_redeclaration(
                              a_symbol_locator                 *loc,
                              a_module_entity_ptr              mep,
                              ARG_UNUSED a_source_position_ptr pos,
                              char                             **redecl_entity,
                              an_il_entry_kind                 *redecl_kind)
/*
Given an IFC module entity's symbol locator, module entity pointer, and source
position, check for a redeclaration of a template.  If a redeclaration is
found, return TRUE and set *redecl_entity and *redecl_kind to the redeclared
entity and its associated kind; otherwise, return FALSE.

The caller is responsible for handling any required merging of the
declarations.  In the case of duplicate definitions the caller is responsible
for ignoring the latter definition for header units and erroring for named
modules.
*/
{
  a_boolean result = find_redeclared_template_entity(loc->symbol_header, mep,
                                                     redecl_entity,
                                                     redecl_kind);

  if (result) {
    /* This is a redeclaration of an existing symbol. */
    /* FIXME: This should also trigger if the redecl_sym's module is a named
       module. */
#if 0
    if (!is_header_unit(mep->module_info)) {
      pos_error(ec_module_entity_redeclaration, pos);
    }  /* if */
#endif /* 0 */
  }  /* if */
  return result;
}  /* check_and_set_template_redeclaration */


a_template_arg_ptr create_templ_args_for_comparison(
                             const an_ifc_decl_specialization &decl_spec,
                             a_template_ptr                   primary_template)
/*
Given the IFC node information for a template specialization and the IL primary
template, create and return a corresponding template argument set.  If a
problem is encountered during reconstruction, NULL is returned instead.
*/
{
  a_template_arg_ptr     result = NULL;
  an_ifc_form_spec_index form_idx = get_ifc_form(decl_spec);
  Opt<an_ifc_form_spec>  opt_form_spec;

  construct_node(&opt_form_spec, form_idx);
  if (opt_form_spec.has_value()) {
    an_ifc_form_spec         form_spec = *opt_form_spec;
    an_ifc_expr_index        form_arg_idx = get_ifc_arguments(form_spec);
    an_ifc_module            *mod = form_arg_idx.mod;
    a_template_decl_ptr      template_decl = primary_template->template_decl;
    a_template_parameter_ptr il_param_list = template_decl->param_list;

    result = mod->template_args_for_expr_list(il_param_list, form_arg_idx);
  }  /* if */
  return result;
}  /* create_templ_args_for_comparison */


a_boolean is_redeclared_specialized_entity(a_symbol_ptr        sym,
                                           a_template_ptr      primary_templ,
                                           a_template_arg_ptr  templ_args,
                                           char                **redecl_entity,
                                           an_il_entry_kind    *redecl_kind)
/*
For a given module entity's potentially previously-declared symbol, primary
template, and template arguments, check to see if the symbol is indeed a
redeclaration.  If the symbol is a redeclaration, return TRUE and set
*redecl_entity and *redecl_kind to the redeclared entity and its associated
kind; otherwise, return FALSE.

This function should not be used directly in modules code, instead see
check_and_set_specialization_redeclaration.
*/
{
  a_boolean        result = FALSE;
  an_il_entry_kind templ_kind;
  char             *templ_entity = il_entry_for_symbol_null_okay(sym,
                                                                 &templ_kind);

  /* Confirm the symbol refers to a template entity. */
  check_assertion(templ_entity != NULL && templ_kind == iek_template);
  /* Confirm the specialization is of the template referenced by this
     symbol. */
  if (primary_templ->canonical_template == (a_template_ptr)templ_entity) {
    a_symbol_ptr existing = find_template_instantiation(sym, templ_args);

    if (existing != NULL) {
      result = TRUE;
      *redecl_entity = il_entry_for_symbol_null_okay(existing, redecl_kind);
      check_assertion(*redecl_entity != NULL);
    }  /* if */
  }  /* if */
  return result;
}  /* is_redeclared_specialized_entity */


a_boolean
find_redeclared_specialized_entity_in_list(a_symbol_ptr        sym_list,
                                           a_template_ptr      primary_templ,
                                           a_template_arg_ptr  templ_args,
                                           char                **redecl_entity,
                                           an_il_entry_kind    *redecl_kind)
/*
For a given module entity's associated symbol list, primary template, and
template arguments, search the symbols for a redeclaration.  If a redeclaration
is found, return TRUE and set *redecl_entity and *redecl_kind to the redeclared
entity and its associated kind; otherwise, return FALSE.

This function should not be used directly in modules code, instead see
check_and_set_specialization_redeclaration.
*/
{
  a_boolean result = FALSE;

  for (a_symbol_ptr sym = sym_list; sym != NULL; sym = sym->next) {
    if (!is_template_symbol(sym)) {
      continue;
    }  /* if */
    if (is_redeclared_specialized_entity(sym, primary_templ, templ_args,
                                         redecl_entity, redecl_kind)) {
      result = TRUE;
      break;
    }  /* if */
  }  /* if */
  return result;
}  /* find_redeclared_specialized_entity_in_list */


a_boolean find_redeclared_specialized_entity(
                              a_symbol_header                  *sym_header,
                              const an_ifc_decl_specialization &decl_spec,
                              char                             **redecl_entity,
                              an_il_entry_kind                 *redecl_kind)
/*
For a given module entity's symbol header and IFC node information, search the
active and inactive symbols for a redeclaration.  If a redeclaration is found,
return TRUE and set *redecl_entity and *redecl_kind to the redeclared entity
and its associated kind; otherwise, return FALSE.

This function should not be used directly in modules code, instead see
check_and_set_specialization_redeclaration.
*/
{
  a_boolean           result = FALSE;
  /* The template should already be processed, this is just fetching the IL
     entity associated with the primary template. */
  an_ifc_decl_index   templ_idx = get_ifc_primary_template(decl_spec);
  a_module_entity_ptr templ_mep = process_decl_at_index(templ_idx);
  a_tagged_pointer    templ_entity = templ_mep->entity;

  if (!templ_mep->invalid && templ_entity.kind == iek_template) {
    a_template_ptr     primary_templ = (a_template_ptr)templ_entity.ptr;
    /* FIXME: It would be nice to lazy load this, while comparisons are
       optimized by doing this up front, we're paying an unnecessary cost if
       there are no symbols to compare against. */
    a_template_arg_ptr templ_args = create_templ_args_for_comparison(
                                                                decl_spec,
                                                                primary_templ);

    if (templ_args != NULL) {
      a_symbol_ptr active_symbols = sym_header->symbol;
      a_symbol_ptr inactive_symbols = sym_header->inactive_symbols;

      /* FIXME: Due to the lack of the canonical template symbol on the module
         entity pointer, the front end doesn't have a symbol to use to perform
         lookup of the instantiation (i.e., use the front end's
         find_template_instantiation function).  Thus, we must traverse the
         symbol lists. */
      if (find_redeclared_specialized_entity_in_list(active_symbols,
                                                     primary_templ, templ_args,
                                                     redecl_entity,
                                                     redecl_kind) ||
          find_redeclared_specialized_entity_in_list(inactive_symbols,
                                                     primary_templ, templ_args,
                                                     redecl_entity,
                                                     redecl_kind)) {
        result = TRUE;
      }  /* if */
      /* Free the allocated template arguments. */
      free_template_arg_list(templ_args);
    }  /* if */
  }  /* if */
  return result;
}  /* find_redeclared_specialized_entity */


a_boolean check_and_set_specialization_redeclaration(
                              a_symbol_locator                 *loc,
                              a_module_entity_ptr              mep,
                              const an_ifc_decl_specialization &decl_spec,
                              ARG_UNUSED a_source_position_ptr pos,
                              char                             **redecl_entity,
                              an_il_entry_kind                 *redecl_kind)
/*
Given an IFC module entity's symbol locator, module entity pointer, IFC node
information, and source position, check for a redeclaration of a template
specialization.  If a redeclaration is found, return TRUE and set
*redecl_entity and *redecl_kind to the redeclared entity and its associated
kind; otherwise, return FALSE.

The caller is responsible for handling any required merging of the
declarations.  In the case of duplicate definitions the caller is responsible
for ignoring the latter definition for header units and erroring for named
modules.
*/
{
  a_boolean result = find_redeclared_specialized_entity(loc->symbol_header,
                                                        decl_spec,
                                                        redecl_entity,
                                                        redecl_kind);

  if (result) {
    /* This is a redeclaration of an existing symbol. */
    /* FIXME: This should also trigger if the redecl_sym's module is a named
       module. */
#if 0
    if (!is_header_unit(mep->module_info)) {
      pos_error(ec_module_entity_redeclaration, pos);
    }  /* if */
#endif /* 0 */
  }  /* if */
  return result;
}  /* check_and_set_specialization_redeclaration */


a_boolean has_default_arguments(char *entity_ptr, an_il_entry_kind kind)
/*
Given an entity pointer and its corresponding tag (kind), return TRUE if the
entity has default arguments; otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  switch (kind) {
    case iek_routine:
      { a_routine_ptr    routine = (a_routine_ptr)entity_ptr;
        a_param_type_ptr param_ty = get_routine_param_types(routine);

        for (; param_ty != NULL; param_ty = param_ty->next) {
          if (param_ty->has_default_arg) {
            result = TRUE;
            break;
          }  /* if */
        }  /* for */
      }
      break;
    case iek_template:
      { a_template_ptr  templ = (a_template_ptr)entity_ptr;
        a_template_kind templ_kind = templ->kind;

        switch (templ_kind) {
          case templk_function:
          case templk_member_function:
            { a_routine_ptr routine = templ->prototype_instantiation.routine;

              result = has_default_arguments((char*)routine, iek_routine);
            }
            break;
          default:
            break;
        }  /* switch */
      }
      break;
    default:
      break;
  }  /* switch */
  return result;
}  /* has_default_arguments */


/*
An RAII object to temporarily enable Microsoft features even if not enabled
otherwise.  This allows for MS specific IFC decls to be correctly processed.
*/
struct a_ms_mode_parse {
  inline a_ms_mode_parse(an_ifc_module *mod);
  inline ~a_ms_mode_parse();
private:
  a_boolean
                old_microsoft_mode;
                        /* The previous value of "microsoft_mode". */
  unsigned long
                old_microsoft_version;
                        /* The previous value of "microsoft_version". */
  a_boolean
                old_ms_extensions;
                        /* The previous value of "ms_extensions". */
  a_boolean
                old_ms_compat;
                        /* The previous value of "ms_compat". */
  a_boolean
                old_allow_in_class_specializations;
                        /* The previous value of
                           "allow_in_class_specializations". */
  a_boolean
                old_allow_in_class_instantiations;
                        /* The previous value of
                           "allow_in_class_instantiations". */
};  /* a_ms_mode_parse */


unsigned long get_microsoft_version(ARG_UNUSED an_ifc_module *mod)
/*
Given an IFC module, determine the appropriate version level of Microsoft mode
to emulate.
*/
{
  /* FIXME: This should depend on version information in the IFC file. */
  return 1928;
}  /* get_microsoft_version */


a_ms_mode_parse::a_ms_mode_parse(an_ifc_module *mod)
    : old_microsoft_mode(microsoft_mode),
      old_microsoft_version(microsoft_version),
      old_ms_extensions(ms_extensions), old_ms_compat(ms_compat),
      old_allow_in_class_specializations(allow_in_class_specializations),
      old_allow_in_class_instantiations(allow_in_class_instantiations)
/*
Temporarily enter a limited Microsoft mode that's catered to the compiler used
to produce the IFC file to process Microsoft specific features in context.
*/
{
  microsoft_mode = TRUE;
  microsoft_version = get_microsoft_version(mod);
  ms_extensions = TRUE;
  ms_compat = TRUE;
  allow_in_class_specializations = TRUE;
  allow_in_class_instantiations = TRUE;
}  /* a_ms_mode_parse */


a_ms_mode_parse::~a_ms_mode_parse()
/*
Restore to the previous compiler modes.
*/
{
  allow_in_class_instantiations = old_allow_in_class_instantiations;
  allow_in_class_specializations = old_allow_in_class_specializations;
  ms_compat = old_ms_compat;
  ms_extensions = old_ms_extensions;
  microsoft_version = old_microsoft_version;
  microsoft_mode = old_microsoft_mode;
}  /* ~a_ms_mode_parse */


a_boolean is_from_gmf(an_ifc_basic_specifiers_bitfield specifier)
/*
Return TRUE if the specifier indicates the associated entity came from the
global module fragment, FALSE otherwise.
*/
{
  /* Lambda closure class members are marked as being members of the GMF
     without actually being members of the GMF.  More generally, if something
     is the member of a class then it's reasonable to expect that we can treat
     it as not a member of the GMF and allow the containing class' membership
     in the GMF to cover it, if necessary. */
  return (test_bitmask<ifc_bsb_is_member_of_global_module>(specifier) &&
          !test_bitmask<ifc_bsb_initialized_in_class>(specifier));
}  /* is_from_gmf */


inline a_boolean is_unnamed_tag(a_const_char  *name)
/*
Return TRUE If the given string is "<unnamed-tag>".
*/
{
#define UNNAMED_TAG_NAME "<unnamed-tag>"
  return strncmp(name, UNNAMED_TAG_NAME, sizeof(UNNAMED_TAG_NAME)-1) == 0;
#undef UNNAMED_TAG_NAME
}  /* is_unnamed_tag */

#if BUILTIN_FUNCTIONS_ENABLED

a_boolean is_builtin_function(const an_ifc_decl_function &func,
                              a_symbol_locator           *loc)
/*
Given an IFC function declaration, return TRUE if this function should be
processed by loading a builtin; otherwise, return FALSE and load the function
using the normal IFC modules function loading logic.
*/
{
  a_boolean result = TRUE;

  if (!is_null_index(get_ifc_home_scope(func))) {
    result = FALSE;
  } else if (loc->symbol_header == NULL) {
    result = FALSE;
  } else if (!loc->symbol_header->is_builtin_function) {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_builtin_function */

#endif /* BUILTIN_FUNCTIONS_ENABLED */

using an_ifc_decl_array = Dyn_array<an_ifc_decl_index>;
using an_ifc_template_lookup_table = Ptr_map<an_ifc_decl_index,
                                             an_ifc_decl_array*>;
			/* The type of a table that maps IFC template
			   declaration indices to the associated list of
			   specializations. */

an_ifc_template_lookup_table
		*ifc_decl_template_lookup_table;
			/* A hash table to map IFC declaration indices to
			   corresponding front end symbols. */


an_ifc_decl_index to_decl_index(an_ifc_partition_kind_index index)
/*
Given an IFC partition kind index, return an IFC decl index.
*/
{
  an_ifc_decl_sort sort = to_decl_sort(index.partition_kind);

  return an_ifc_decl_index{index.mod, sort, index.value};
}  /* to_decl_index */


an_ifc_decl_array*
get_or_alloc_specialization_list(an_ifc_decl_index templ_idx)
/*
Return a dynamic array of specializations for the given template index that are
found in supplementary IFC files (i.e., those specializations for the template,
but are declared by other IFC module files).
*/
{
  an_ifc_decl_array *specializations =
                                ifc_decl_template_lookup_table->get(templ_idx);

  if (specializations == NULL) {
    specializations = alloc_fe_of_type(an_ifc_decl_array);
    construct(specializations);
    ifc_decl_template_lookup_table->map(templ_idx, specializations);
  }  /* if */
  return specializations;
}  /* get_or_alloc_specialization_list */


void free_specialization_list(an_ifc_decl_index templ_idx)
/*
Give the index of the associated template, destroy the associated
specialization list (if any).
*/
{
  an_ifc_decl_array *specializations =
                                ifc_decl_template_lookup_table->get(templ_idx);

  if (specializations != NULL) {
    destroy(specializations);
    free_fe(specializations);
    ifc_decl_template_lookup_table->unmap(templ_idx);
  }  /* if */
}  /* free_specialization_list */


template<typename an_ifc_Node_type>
void associate_spec_with_template(an_ifc_decl_index      node_idx,
                                  const an_ifc_Node_type &node)
/*
Given an IFC declaration index and its associated node information, associate
the specialization with its primary template for later processing.  If later
processing is insufficient for proper reconstruction, this function will
instead process the specialization immediately.

Most of the time this function is a no-op as the IFC-provided
"trait.specialization" provides any specializations declared in the same module
as the associated template (i.e., this function takes care of cases where the
specialization is declared in an additional module).
*/
{
  if (node.get_module()->references_any_modules) {
    an_ifc_decl_index raw_templ_idx = get_ifc_primary_template(node);

    /* If we cross a module boundary this specialization won't be in the
       specializations, so we need to register it for later processing. */
    if (raw_templ_idx.sort == ifc_ds_decl_reference) {
      Opt<an_ifc_decl_reference> opt_decl_ref;

      construct_node(&opt_decl_ref, raw_templ_idx);
      if (opt_decl_ref.has_value()) {
        an_ifc_decl_index   templ_idx = get_ifc_index(*opt_decl_ref);
        a_module_entity_ptr templ_mep = get_ifc_module_entity_ptr(templ_idx);

        /* Typically the entity will not already be set.  There are two
           exceptions to this where the front end needs to enter the else case
           here and "catch up" as the template IL entity has already been
           processed.

           The first case is seemingly a Microsoft extension where code can be
           placed between two different import declarations.

           The second case is with eager loading.  When eager loading mode is
           enabled, if the specialization is encountered after the template,
           the specialization needs to be processed immediately. */
        /* FIXME: Is the Microsoft extension where code can exist between
           import declarations correctly handled here. */
        if (templ_mep->entity.ptr == NULL) {
          an_ifc_decl_array *decl_arr =
                                   get_or_alloc_specialization_list(templ_idx);

          decl_arr->push_back(node_idx);
        } else {
          (void)process_decl_at_index(node_idx);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* associate_spec_with_template */


Opt<an_ifc_sequence> get_specialization_sequence_from_trait(
                                                        an_ifc_decl_index decl)
/*
Find and return the sequence of specializations corresponding to the template
decl, or an empty sequence if not found.
*/
{
  Opt<an_ifc_sequence>             result;
  Opt<an_ifc_trait_specialization> opt_its;

  check_assertion(decl.sort == ifc_ds_decl_template);
  find_trait(&opt_its, decl);
  if (opt_its.has_value()) {
    result = get_ifc_trait(*opt_its);
  }  /* if */
  return result;
}  /* get_specialization_sequence_from_trait */


/*
A structure used to aggregate information provided by the IFC file and the
current state of the ifc_decl_template_lookup_table for ordered processing of
template specializations and explicit template instantiations.
*/
struct an_ifc_template_spec_info {
  inline an_ifc_template_spec_info(an_ifc_decl_index templ_idx_val);
  a_boolean has_specs()
    { return specializations.length() + explicit_instantiations.length() > 0; }
  void process_specializations();
  void process_instantiations();
private:
  void traverse_data(Opt<an_ifc_sequence> spec_sequence,
                     an_ifc_decl_array    *spec_references);
  an_ifc_decl_index
                templ_idx;
                        /* The IFC index for the associated template
                           declaration. */
  an_ifc_decl_array
                specializations;
                        /* An array of IFC declaration indexes that represent
                           specializations of the associated template. */
  an_ifc_decl_array
                explicit_instantiations;
                        /* An array of IFC declaration indexes that represent
                           explicit instantiations of the associated
                           template. */
};  /* an_ifc_template_spec_info */


an_ifc_template_spec_info::an_ifc_template_spec_info(
                                               an_ifc_decl_index templ_idx_val)
  : templ_idx(templ_idx_val)
/*
Construct a new template spec info object for the template at the given IFC
index.
*/
{
  Opt<an_ifc_sequence> opt_spec_seq =
                         get_specialization_sequence_from_trait(templ_idx_val);
  an_ifc_decl_array*   spec_references =
                            ifc_decl_template_lookup_table->get(templ_idx_val);

  this->traverse_data(opt_spec_seq, spec_references);
}  /* an_ifc_template_spec_info */


a_boolean is_explicit_instantiation(an_ifc_decl_index decl_idx)
/*
If the given decl index refers to the declaration of an explicit
instantiation, return TRUE; otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  if (decl_idx.sort == ifc_ds_decl_specialization) {
    Opt<an_ifc_decl_specialization> opt_decl_spec;

    construct_node(&opt_decl_spec, decl_idx);
    if (opt_decl_spec.has_value()) {
      an_ifc_decl_specialization decl_spec = *opt_decl_spec;

      if (get_ifc_sort(decl_spec) == ifc_ss_instantiation) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_explicit_instantiation */


void
an_ifc_template_spec_info::traverse_data(Opt<an_ifc_sequence> spec_sequence,
                                         an_ifc_decl_array    *spec_references)
/*
Traverse the given IFC sequence (if provided) and the given array of IFC
declaration indexes; these are effectively independent containers each
containing a mix of explicit instantiations and specializations.  This function
aggregates the associated elements and splits them into dedicated lists of
explicit instantiations and specializations.
*/
{
  /* Process the specializations directly associated with the template declared
     in the same IFC file. */
  if (spec_sequence.has_value()) {
    an_ifc_sequence          seq = *spec_sequence;
    a_scope_member_traverser traverser(seq);

    for (an_Indexed<an_ifc_scope_member> indexed_scope_mem : traverser) {
      if (!indexed_scope_mem.has_value()) {
        continue;
      }  /* if */

      an_ifc_scope_member scope_mem = *indexed_scope_mem;
      an_ifc_decl_index   mem_decl_idx = get_ifc_index(scope_mem);
      if (is_explicit_instantiation(mem_decl_idx)) {
        this->explicit_instantiations.push_back(mem_decl_idx);
      } else {
        this->specializations.push_back(mem_decl_idx);
      }  /* if */
    }  /* for */
  }  /* if */
  /* Process the specializations indirectly associated with the template
     declared by a supplementary IFC file. */
  if (spec_references != NULL) {
    for (an_ifc_decl_index decl_idx : *spec_references) {
      if (is_explicit_instantiation(decl_idx)) {
        this->explicit_instantiations.push_back(decl_idx);
      } else {
        this->specializations.push_back(decl_idx);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* traverse_data */


void an_ifc_template_spec_info::process_specializations()
/*
Process the specializations associated with a given template.
*/
{
  check_assertion(this->has_specs());
  /* Process the specializations. */
  for (an_ifc_decl_index decl_idx : this->specializations) {
    (void)process_decl_at_index(decl_idx);
  }  /* for */
  free_specialization_list(this->templ_idx);
}  /* process_specializations */


void an_ifc_template_spec_info::process_instantiations()
/*
Process the explicit instantiations associated with a given template.  This
should be called after all specializations are setup.
*/
{
  check_assertion(this->has_specs());
  /* Process the explicit instantiations. */
  for (an_ifc_decl_index decl_idx : this->explicit_instantiations) {
    (void)process_decl_at_index(decl_idx);
  }  /* for */
}  /* process_instantiations */


inline void update_cache_info_for_template(an_ifc_cache_info *cache_info,
                                           a_template_ptr    templ)
/*
Update the given IFC cache information to set flags for suppressing details the
given IL template already contains.
*/
{
  if (has_default_template_arguments(templ)) {
    cache_info->ignore_default_template_arguments = TRUE;
  }  /* if */
  if (has_default_arguments((char*)templ, iek_template)) {
    cache_info->ignore_default_arguments = TRUE;
  }  /* if */
  if (is_defined((char*)templ, iek_template)) {
    cache_info->ignore_definition = TRUE;
  }  /* if */
}  /* update_cache_info_for_template */


void process_template_deduction_guides(a_template_ptr    templ,
                                       an_ifc_decl_index decl_idx)
/*
For the given class template that is identified by the given declaration index,
check if it has any associated deduction guides, and process them.
*/
{
  check_assertion(templ->kind == templk_class);
  /* Deduction guides are associated to the template through the IFC traits
     mechanism.*/
  Opt<an_ifc_trait_deduction_guide> opt_guide_trait;

  find_trait(&opt_guide_trait, decl_idx);
  if (opt_guide_trait.has_value()) {
    an_ifc_decl_index   guides_idx = get_ifc_trait(*opt_guide_trait);
    a_module_entity_ptr guides_mep = get_ifc_module_entity_ptr(guides_idx);

    /* A single guide will have an ifc_DeclSort_Template entry directly
       associated with it, but it doesn't record the parent scope: So set
       it here (a guide is required to be declared in the same scope as
       the class template).  If there are multiple guides, guides_mep
       will be for an ifc_DeclSort_Tuple entry instead (and its treatment
       will propagate the parent scope). */
    guides_mep->scope = templ->source_corresp.parent_scope;
    process_decl_at_index(guides_idx);
  }  /* if */
}  /* process_template_deduction_guides */


a_boolean
process_template_definition(const an_ifc_decl_template &decl_templ,
                            a_module_entity_ptr        mep,
                            an_ifc_template_spec_info  spec_info,
                            a_boolean                  process_specializations,
                            char                       **il_entity,
                            an_il_entry_kind           *kind)
/*
Given a module entity's IFC node information, module entity pointer, and
aggregated template specialization information, attempt to complete the
entity's definition and specializations.  If process_specializations is TRUE,
specializations will be processed as well; otherwise, they will be ignored.
Return TRUE if processing succeeds without error; otherwise, return FALSE.  If
processing succeeds *il_entity and *kind will be set to the new IL entity for
definition.

Notably, if there's a previous declaration *il_entity and *kind must be set to
the existing template declaration; otherwise, *il_entity should be NULL.
*/
{
  check_assertion(*il_entity == NULL || *kind == iek_template);
  a_boolean         result = TRUE;
  an_ifc_cache_info cache_info;
  an_ifc_module     *mod = get_assoc_ifc_module(mep);

  if (*il_entity != NULL) {
    check_assertion(*kind == iek_template);
    update_cache_info_for_template(&cache_info, (a_template_ptr)*il_entity);
  }  /* if */
  {
    a_module_token_cache cache;

    mod->cache_decl_template(&cache, decl_templ, cache_info);
    if (!cache.is_valid()) {
      goto invalid;
    }  /* if */
    *il_entity = parse_cached_template(&cache, mep->scope, kind);
    if (*kind != iek_template) {
      goto invalid;
    }  /* if */
  }
  /* Process any class template deduction guides. */
  {
    a_template_ptr templ = (a_template_ptr)*il_entity;

    if (templ->kind == templk_class) {
      an_ifc_decl_index decl_idx = decl_index_of(mep);

      process_template_deduction_guides(templ, decl_idx);
    }  /* if */
  }
  /* Process any specializations and explicit instantiations. */
  if (spec_info.has_specs()) {
    if (process_specializations) {
      spec_info.process_specializations();
    }  /* if */
    spec_info.process_instantiations();
  }  /* if */
  goto done;
invalid:
  result = FALSE;
done:
  return result;
}  /* process_template_definition */


a_boolean
process_delayed_template_definition(const an_ifc_decl_template &decl_templ,
                                    a_module_entity_ptr        mep,
                                    char                       **il_entity,
                                    an_il_entry_kind           *kind)
/*
Given a module entity's IFC node information and module entity pointer, attempt
to complete the entity's definition and specializations.  If processing
succeeds without error, return TRUE and set *il_entity and *kind to the
redeclared entity and its associated kind; otherwise, return FALSE.

Notably, if there's a previous declaration *il_entity and *kind must be set to
the existing template declaration; otherwise, *il_entity should be NULL with
kind set to iek_template.
*/
{
  an_ifc_template_spec_info spec_info(decl_index_of(mep));

  /* Delayed template definitions always follow a forward declaration,
     so specializations have already been handled. */
  return process_template_definition(decl_templ, mep, spec_info,
                                     /*process_specializations=*/FALSE,
                                     il_entity, kind);
}  /* process_delayed_template_definition */


using an_ifc_template_def_map = Ptr_map<a_template_ptr, an_ifc_decl_index>;
                        /* The type of a map that associates IFC template
                           definitions with IL template entries. */

an_ifc_template_def_map
                *ifc_template_definitions;
                        /* A map from canonical template IL pointers to entries
                           of type an_ifc_decl_index that can be used to
                           retrieve the definition of the template when
                           needed. */


void record_pending_ifc_template_definition(a_template_ptr    templ,
                                            an_ifc_decl_index decl_idx)
/*
Record the information needed to retrieve a definition for templ if it turns
out to be needed later on.
*/
{
  /* Ensure the canonical template IL entity is what's being mapped onto. */
  check_assertion(templ != NULL && templ->canonical_template != NULL);
  templ = templ->canonical_template;
  (void)ifc_template_definitions->map_or_replace(templ, decl_idx);
}  /* record_pending_ifc_template_definition */

}  /* namespace */

a_boolean has_template_definition_from_ifc_module(a_template_ptr  templ)
/*
If the given template has a definition in a currently-imported IFC module
return TRUE.  Note that templ must refer to the canonical template.
*/
{
  check_assertion(templ != NULL && templ->canonical_template == templ);
  an_ifc_decl_index def_decl_idx = ifc_template_definitions->get(templ);

  return def_decl_idx.mod != NULL;
}  /* has_template_definition_from_ifc_module */


a_boolean load_template_definition_from_ifc_module(a_template_ptr  templ)
/*
The given template entry represents a template that was loaded from an IFC
module, but its definition hasn't been loaded yet.  Load the definition now.
*/
{
  check_assertion(has_template_definition_from_ifc_module(templ));
  a_boolean            result = FALSE;
  an_ifc_decl_index    def_decl_idx = ifc_template_definitions->get(templ);

  /* When the ifc_template_definitions were inserted, this should've been
     validated and not added to the map if invalid. */
  an_ifc_decl_template template_decl;
  construct_node_prechecked(&template_decl, def_decl_idx);

  /* Reconstruct the module entity state. */
  Value_saver<a_source_position>
                              saved_error_position(&error_position);
  Value_saver<a_module_entity_ptr>
                              saved_mep(&curr_module_entity);
  a_curr_token_preserver      guard;
  a_module_entity_ptr         mep = get_ifc_module_entity_ptr(def_decl_idx);
  a_module_entity_stack_state mep_state(mep);
  a_module_scope_push_kind    scope_push_status = mspk_unattempted;
  curr_module_entity = mep;
  push_module_declaration_context(mep->scope, &scope_push_status);

  /* Process the definition. */
  char              *il_entity = (char*)templ;
  an_il_entry_kind  kind = iek_template;
  if (process_delayed_template_definition(template_decl, mep,
                                          &il_entity, &kind)) {
    /* FIXME: We need to be able to better determine that the template has been
       defined before removing from the map. */
    /* The definition was successfully loaded, unmap the pending definition. */
    /* ifc_template_definitions->unmap(templ); */
  }  /* if */
  /* Update the module entity pointer to refer to the defining IL template. */
  mep->entity.kind = kind;
  mep->entity.ptr = (char*)il_entity;
  /* Restore the module declaration context stack; all other cleanup is RAII
     based. */
  pop_module_declaration_context(scope_push_status);
  return result;
}  /* load_template_definition_from_ifc_module */


static void process_decl_to_il_entity(a_module_entity_ptr mep,
                                      a_boolean           defer)
/*
Process the IFC module entity declaration specified by mep.  When defer is
TRUE, no IL entity is created; instead, the appropriate symbol header is
updated to note the deferred entity for lazy loading.  When defer is FALSE, we
immediately process *mep and attempt to form an IL entity; if no IL entity can
be formed, *mep will marked as invalid.

Only module entity pointers for IL entities not in class scope should be passed
to this function.
*/
{
  an_ifc_module            *mod = get_assoc_ifc_module(mep);
  a_decl_parse_state       dps;
  an_id_linkage_kind       linkage_ptr;
  a_symbol_ptr             ext_sym;
  a_decl_pos_block         decl_pos_block;
  a_symbol_locator         loc;
  a_partial_scope_stack_state
                           psss;
  char                     *&il_entity = mep->entity.ptr;
  an_il_entry_kind         &kind = mep->entity.kind;
  a_module_scope_push_kind scope_push_status = mspk_unattempted;
  a_diagnostic_suppression diag_suppress(&mod->suppressed_diagnostics,
                                         !display_module_import_diagnostics);
  Value_saver<a_boolean>   checking_pragma_saver(&no_checking_pragmas, TRUE);
  a_source_position        saved_error_position = error_position;
  a_module_entity_ptr      saved_mep = curr_module_entity;
  a_ms_mode_parse          tmp_ms_parse(mod);
  an_ifc_decl_index        decl_idx = decl_index_of(mep);
  a_module_entity_stack_state
                           mep_state(mep);

#if DEBUG
  ++num_module_decls_attempted;
  if (db_flag_is_set("ifc_idx")) {
    (void)fprintf(f_debug, "Processing %s (%d)\n", str_for(decl_idx.sort),
                  decl_idx.value);
  }  /* if */
#endif /* DEBUG */
  curr_module_entity = mep;
  if (!defer) {
    mep->imminent = TRUE;
    if (mep->scope != NULL) {
      /* If this module entity has a scope, attempt to re-activate it now.  If
         it does not already have a scope, one may be created below. */
      push_module_declaration_context(mep->scope, &scope_push_status);
    }  /* if */
  }  /* if */
  switch (decl_idx.sort) {
    case ifc_ds_decl_vendor_extension:
    case ifc_ds_decl_explicit_instantiation:
    case ifc_ds_decl_explicit_specialization:
      /* FIXME: Need a proper source position for this. */
      error_position = null_source_position;
      issue_unsupported_construct_error(mod, str_for(decl_idx.sort),
                                        &error_position);
      il_entity = (char *)error_type();
      kind = iek_type;
      break;
    case ifc_ds_decl_variable:
      { Opt<an_ifc_decl_variable> opt_idv;

        construct_node(&opt_idv, decl_idx);
        if (!opt_idv.has_value()) {
          goto invalid;
        }  /* if */
        if (!mod->init_decl_locator(*opt_idv, &loc)) {
          goto invalid;
        }  /* if */
        if (defer) {
          defer_symbol_creation(mep, &loc);
        } else {
          an_ifc_decl_variable idv = *opt_idv;
          a_module_token_cache cache;

          if (is_from_gmf(get_ifc_specifiers(idv))) {
            mep->global_module = TRUE;
          }  /* if */
          if (mep->scope == NULL) {
            mep->scope = get_home_scope(idv);
            push_module_declaration_context(mep->scope, &scope_push_status);
          }  /* if */
          init_decl_parse_state(&dps);
          /* Naming aside, setting this is required to allow the inline keyword
             on variable declarations. */
          dps.function_definition_allowed = TRUE;

          an_ifc_cache_info cache_info;
          if (check_and_set_redeclaration(&loc, mep, &error_position,
                                          iek_variable, &il_entity, &kind)) {
            if (is_defined(il_entity, kind)) {
              cache_info.ignore_definition = TRUE;
            }  /* if */
            check_assertion(kind == iek_variable);

            /* Skip any further processing when the variable is not declared
               "extern" (as variable declarations cannot otherwise be
               meaningfully "merged"). */
            a_variable_ptr variable = (a_variable_ptr)il_entity;
            if (variable->storage_class != sc_extern) {
              goto done;
            }  /* if */
          }  /* if */
          mod->cache_decl(&cache, decl_idx, cache_info);
#if DEBUG
          if (db_flag_is_set("ms_ifc_token_def")) {
            fprintf(f_debug, "Reconstituted variable declaration:\n");
            db_tokens(&cache);
            fprintf(f_debug, "\n---------------------\n");
          }  /* if */
#endif /* DEBUG */
          if (!cache.is_valid()) {
            goto invalid;
          }  /* if */
          {
            a_module_entity_rescan rescan(&cache);

            scan_nonmember_declaration(&dps, /*a_source_range=*/NULL);
          }
          if (dps.sym != NULL && symbol_is(dps.sym, sk_variable)) {
            il_entity = (char *)dps.sym->variant.variable.ptr;
            kind = iek_variable;
          }  /* if */
        }  /* if */
      }
      break;
    case ifc_ds_decl_function:
      { Opt<an_ifc_decl_function> opt_idf;

        construct_node(&opt_idf, decl_idx);
        if (!opt_idf.has_value()) {
          goto invalid;
        }  /* if */
        if (!mod->init_decl_locator(*opt_idf, &loc)) {
          goto invalid;
        }  /* if */
        if (defer) {
          defer_symbol_creation(mep, &loc);
        } else {
          /* FIXME: lots more to do here. */
          an_ifc_decl_function idf = *opt_idf;

#if BUILTIN_FUNCTIONS_ENABLED
          if (is_builtin_function(idf, &loc)) {
            /* This is a builtin function, trigger builtin processing. */
            a_symbol_ptr builtin_sym;

            if (builtin_needs_to_be_loaded(loc.symbol_header)) {
              builtin_sym = load_matching_builtin_function(loc.symbol_header);
            } else {
              builtin_sym =
                       file_scope_id_lookup(il_header.primary_scope, &loc,
                                            IDL_DIRECT_NAMESPACE_MEMBERS_ONLY |
                                            IDL_SUPPRESS_DECL_SEQ_CHECK);
            }  /* if */
            if (builtin_sym == NULL) {
              goto invalid;
            }  /* if */
            il_entity = (char *)builtin_sym->variant.routine.ptr;
            kind = iek_routine;
          } else
#endif /* BUILTIN_FUNCTIONS_ENABLED */
          /* Do not add code here. */
          {
            an_ifc_function_traits_bitfield traits = get_ifc_traits(idf);
            a_func_info_block               func_info;
            a_type_ptr                      old_type;
            a_routine_ptr                   rp;
            a_boolean                       is_consteval;

            /* FIXME: There's a chicken-and-egg problem here when the return
               type is deduced and requires access to the class scope (e.g.,
               returning a lambda declared within the function). */
            if (mep->scope == NULL) {
              mep->scope = get_home_scope(idf);
              push_module_declaration_context(mep->scope,
                                              &scope_push_status);
            }  /* if */
            mod->init_dps(&dps, get_ifc_locus(idf), get_ifc_type(idf),
                          an_ifc_object_traits_bitfield{},
                          an_ifc_msvc_traits_bitfield{},
                          get_ifc_specifiers(idf), get_ifc_access(idf),
                          an_ifc_expr_index{}, &psss);
            is_consteval = test_bitmask<ifc_ftb_immediate>(traits);
            if (is_consteval) {
              dps.dso_flags |= DSO_CONSTEVAL;
            } else if (test_bitmask<ifc_ftb_constexpr>(traits)) {
              dps.dso_flags |= DSO_CONSTEXPR;
            } else if (test_bitmask<ifc_ftb_inline>(traits)) {
              dps.dso_flags |= DSO_INLINE;
            }  /* if */
            clear_func_info(&func_info);
            clear_decl_pos_block(&decl_pos_block);
            decl_routine(&loc, &dps, &func_info, SRK_DECLARATION,
                         &linkage_ptr, &old_type, &ext_sym, &decl_pos_block);
            restore_partial_scope_stack_if_necessary(&psss);
            rp = dps.sym->variant.routine.ptr;
            il_entity = (char *)rp;
            kind = iek_routine;
            if (!mod->fill_in_routine_parameter_defaults(get_ifc_chart(idf),
                                                         dps.type,
                                                         is_consteval)) {
              goto invalid;
            }  /* if */
            if (function_is_user_defined(idf)) {
              /* A body is available: Record this availability in case it is
                 needed. */
              record_pending_ifc_function_body(rp, decl_idx);
            }  /* if */
          }  /* if */
        }  /* if */
      }
      break;
    case ifc_ds_decl_intrinsic:
      /* A builtin function declaration. */
      { Opt<an_ifc_decl_intrinsic> opt_idi;

        construct_node(&opt_idi, decl_idx);
        if (!opt_idi.has_value()) {
          goto invalid;
        }  /* if */
        if (!mod->init_decl_locator(*opt_idi, &loc)) {
          goto invalid;
        }  /* if */
        if (defer) {
          defer_symbol_creation(mep, &loc);
        } else {
          an_ifc_decl_intrinsic idi = *opt_idi;
          a_func_info_block     func_info;
          a_type_ptr            old_type;
          a_routine_ptr         rp;

          /* FIXME: lots more to do here (just copied ifc_DeclSort_Function).*/
          mod->init_dps(&dps, get_ifc_locus(idi), get_ifc_type(idi),
                        an_ifc_object_traits_bitfield{},
                        an_ifc_msvc_traits_bitfield{}, get_ifc_specifiers(idi),
                        get_ifc_access(idi), an_ifc_expr_index{}, &psss);
          clear_func_info(&func_info);
          clear_decl_pos_block(&decl_pos_block);
          decl_routine(&loc, &dps, &func_info, SRK_DECLARATION, &linkage_ptr,
                       &old_type, &ext_sym, &decl_pos_block);
          restore_partial_scope_stack_if_necessary(&psss);
          rp = dps.sym->variant.routine.ptr;
          mep->scope = rp->source_corresp.parent_scope;
          il_entity = (char *)rp;
          kind = iek_routine;
        }  /* if */
      }
      break;
    case ifc_ds_decl_scope:
      /* A DeclSort::Scope, which indicates a namespace or a
         class/struct/union.  Note that although these declare "scopes", the IL
         entity that is attached to them is either a namespace or a type. */
      { Opt<an_ifc_decl_scope> opt_ids;

        construct_node(&opt_ids, decl_idx);
        if (!opt_ids.has_value()) {
          goto invalid;
        }  /* if */
        if (!mod->init_decl_locator(*opt_ids, &loc)) {
          goto invalid;
        }  /* if */

        an_ifc_decl_scope ids = *opt_ids;
        a_type_kind       type_kind;
        a_symbol_kind     tag_kind;
        /* Should be no unnamed namespaces or types. */
        /* FIXME: This should be a soft failure. */
        check_assertion(!is_null_index(get_ifc_name(ids)));
        if (is_from_gmf(get_ifc_specifiers(ids))) {
          mep->global_module = TRUE;
        }  /* if */
        if (mep->scope == NULL) {
          mep->scope = get_home_scope(ids);
          push_module_declaration_context(mep->scope, &scope_push_status);
        }  /* if */
        if (scope_is(mep->scope, sck_class_struct_union)) {
          /* This is a child class.  It will have already been entered as a
             member of the parent class.  Find that entry and skip the rest of
             the processing. */
          a_type_ptr   parent_class = mep->scope->variant.assoc_type;
          a_symbol_ptr sym;

          /* The parent class may contain references to this child class.
             Clear the imminent flag to allow re-processing to occur if need be
             (otherwise, we may fail to find the type of this entity during
             completion of the parent class). */
          mep->imminent = FALSE;
          sym = look_up_name_string_in_class(loc.symbol_header->identifier,
                                             parent_class,
                                             IDL_TENTATIVE_TYPE_LOOKUP |
                                                              IDL_MUST_BE_TAG);
          mep->imminent = TRUE;
          if (sym == NULL || !is_class_struct_union_symbol(sym)) {
            expect_error();
            goto invalid;
          }  /* if */
          il_entity = (char*)(sym->variant.class_struct_union.type);
          kind = iek_type;
          break;
        }  /* if */

        /* Look at the "type" to determine whether we have a namespace or
           not. */
        Opt<an_ifc_type_fundamental> opt_itf;
        construct_node(&opt_itf, get_ifc_type(ids));
        if (!opt_itf.has_value()) {
          goto invalid;
        }  /* if */
        switch (get_ifc_basis(*opt_itf)) {
          case ifc_tbs_namespace:
            { an_ifc_scope_traits_bitfield traits = get_ifc_traits(ids);
              a_namespace_ptr              nsp = NULL;
              a_symbol_ptr                 ns_sym;

              if (defer && !test_bitmask<ifc_stb_inline>(traits)) {
                /* Note that inline namespaces are not deferred. */
                defer_symbol_creation(mep, &loc);
              } else {
                /* FIXME: lots missing. */
                /* FIXME: This should probably call
                   push/pop_module_declaration_context on itself, rather than
                   managing the namespace scopes through an alternative code
                   path. */
                check_assertion(!test_bitmask<ifc_stb_unnamed>(traits));
                { Value_saver<a_boolean> lazy_load_saver(
                                                &lazy_symbols_may_be_visible,
                                                /*new_value=*/FALSE);
                  if (loc.symbol_header == symbol_for_namespace_std->header &&
                      (scope_is(&scope_stack_top(), sck_module_decl_import) ||
                       scope_is(&scope_stack_top(), sck_file)) &&
                      !loc.is_error) {
                    /* Be sure to use the pre-created namespace std symbol.  It
                       might not be entered in the symbol table yet: Hence,
                       call enter_symbol_for_namespace_std. */
                    ns_sym = symbol_for_namespace_std;
                    enter_symbol_for_namespace_std(&loc);
                  } else {
                    ns_sym = curr_scope_id_lookup(&loc, IDL_NO_OPTIONS);
                  }  /* if */
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
                  if (test_bitmask<ifc_stb_inline>(traits)) {
                    nsp->is_inline = TRUE;
                  }  /* if */
                  ns_sym->variant.namespace_info.ptr = nsp;
                  add_to_namespaces_list(nsp);
                  (void)push_namespace_scope((a_scope_kind)sck_namespace, nsp);
                  nsp->variant.assoc_scope->variant.assoc_namespace = nsp;
                  if (nsp->is_inline) {
                    add_implicit_using_directive(nsp, /*is_inline=*/TRUE,
                                                 /*namespace_pushed=*/TRUE);
                  }  /* if */
                }  /* if */
                /* Forward assign the module entity pointer's information so we
                   can self-reference. */
                il_entity = (char *)nsp;
                kind = iek_namespace;
                /* Process declarations in the namespace scope (which makes
                   their symbols available but not their definitions). */
                mod->process_ifc_scope(get_ifc_initializer(ids),
                                       nsp->variant.assoc_scope);
                pop_namespace_scope();
              }  /* if */
            }
            break;
          case ifc_tbs_class:
            type_kind = tk_class;
            tag_kind = sk_class_or_struct_tag;
            goto class_struct_union_case;
          case ifc_tbs_struct:
          case ifc_tbs_interface:
            type_kind = tk_struct;
            tag_kind = sk_class_or_struct_tag;
            goto class_struct_union_case;
          case ifc_tbs_union:
            type_kind = tk_union;
            tag_kind = sk_union_tag;
class_struct_union_case:
            /* A class/struct/union type. */
            { if (defer) {
                defer_symbol_creation(mep, &loc);
              } else {
                an_ifc_reachable_properties_bitfield properties =
                                                       get_ifc_properties(ids);

                /* Allocate the appropriate class type, but leave it as
                   incomplete.  The class will be completed during a call to
                   get_definition_of_class if it is referenced. */
                if (check_and_set_redeclaration(&loc, mep, &error_position,
                                                iek_type, &il_entity, &kind)) {
                  a_type_ptr type = (a_type_ptr)il_entity;

                  /* FIXME: Is this right? */
                  if (is_incomplete_type(type) &&
                      test_bitmask<ifc_rpb_initializer>(properties)) {
                    /* Record the presence of a definition on an existing
                       type. */
                    type->source_corresp.module_entity = mep;
                  }  /* if */
                  break;
                }  /* if */

                a_type_ptr   tag_type = alloc_type(type_kind);
                a_symbol_ptr tag_sym;
                if (get_ifc_basis(*opt_itf) == ifc_tbs_interface) {
                  tag_type->variant.class_struct_union.is_interface = TRUE;
                  tag_type->variant.class_struct_union.abstract = TRUE;
                }  /* if */
                tag_sym = enter_local_symbol(tag_kind, &loc,
                                             mep->scope->depth_in_scope_stack,
                                             /*suppress_redecl_error=*/TRUE);
                tag_sym->variant.class_struct_union.type = tag_type;
                set_source_corresp(&(tag_type->source_corresp), tag_sym);
                /* Set parent class or namespace pointers, if appropriate, and
                   adjust related fields (e.g., name linkage). */
                /* FIXME: all of these values need to be checked. */
                update_membership_of_class(tag_sym,
                                           /*def_or_vacuous_decl=*/FALSE,
                                           /*is_event_interface=*/FALSE,
                                           mep->scope->depth_in_scope_stack,
                                           &null_source_position);
                /* FIXME: need to call record_symbol_declaration? */
                add_to_types_list(tag_type, mep->scope->depth_in_scope_stack);
                /* Mark the class as incomplete.  It can be completed later if
                   needed. */
                tag_type->incomplete = TRUE;
                /* FIXME: idssp->alignment, idssp->pack_size, idssp->traits,
                          idssp->specifiers, all need to be set manually
                          here. */
                /* FIXME: for now: */
                tag_type->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
                il_entity = (char*)tag_type;
                kind = iek_type;
              }  /* if */
            }
            break;
          default:
            unexpected_condition();
        }  /* switch */
      }
      break;
    case ifc_ds_decl_alias:
      { Opt<an_ifc_decl_alias> opt_ida;

        construct_node(&opt_ida, decl_idx);
        if (!opt_ida.has_value()) {
          goto invalid;
        }  /* if */
        if (!mod->init_decl_locator(*opt_ida, &loc)) {
          goto invalid;
        }  /* if */
        if (defer) {
          defer_symbol_creation(mep, &loc);
        } else {
          an_ifc_decl_alias ida = *opt_ida;
          an_ifc_type_index type = get_ifc_type(ida);

          if (is_from_gmf(get_ifc_specifiers(ida))) {
            mep->global_module = TRUE;
          }  /* if */
          if (type.sort == ifc_ts_type_fundamental) {
            Opt<an_ifc_type_fundamental> opt_itf;

            construct_node(&opt_itf, type);
            if (!opt_itf.has_value()) {
              goto invalid;
            }  /* if */

            an_ifc_type_basis_sort basis = get_ifc_basis(*opt_itf);
            if (basis == ifc_tbs_typename) {
              /* A type alias; declare a typedef for this case. */
              if (mep->scope == NULL) {
                mep->scope = get_home_scope(ida);
                push_module_declaration_context(mep->scope,
                                                &scope_push_status);
              }  /* if */
              if (check_and_set_redeclaration(&loc, mep, &error_position,
                                              iek_type, &il_entity, &kind)) {
                break;
              }  /* if */
              mod->init_dps(&dps, get_ifc_locus(ida), get_ifc_aliasee(ida),
                            an_ifc_object_traits_bitfield{},
                            an_ifc_msvc_traits_bitfield{},
                            get_ifc_specifiers(ida), get_ifc_access(ida),
                            an_ifc_expr_index{}, &psss);
              clear_decl_pos_block(&decl_pos_block);
              decl_typedef(&loc, &dps, (a_type_ptr)NULL, &decl_pos_block);
              restore_partial_scope_stack_if_necessary(&psss);
              il_entity = (char *)dps.sym->variant.type.ptr;
              kind = iek_type;
            } else if (basis == ifc_tbs_namespace) {
              /* A namespace alias. */
              /* FIXME: unimplemented. */
              issue_unsupported_construct_error(mod,
                                                "DeclSort::Alias namespace",
                                                &error_position);
              goto invalid;
            } else {
              a_string err_msg("Unexpected ", str_for(basis));

              ifc_unexpected(mod, err_msg);
              goto invalid;
            }  /* if */
          } else if (type.sort == ifc_ts_type_forall) {
            Opt<an_ifc_type_forall> opt_itf;

            construct_node(&opt_itf, get_ifc_aliasee(ida));
            if (!opt_itf.has_value()) {
              goto invalid;
            }  /* if */

            an_ifc_type_forall          itf = *opt_itf;
            a_module_token_cache        cache;
            an_ifc_source_location      locus = get_ifc_locus(ida);
            an_ifc_source_position_hint pos_hint(&cache, locus);
            a_curr_token_preserver      guard;
            /* An alias template; declare a typedef for this case. */
            if (mep->scope == NULL) {
              mep->scope = get_home_scope(ida);
              push_module_declaration_context(mep->scope, &scope_push_status);
            }  /* if */
            if (check_and_set_redeclaration(&loc, mep, &error_position,
                                            iek_template, &il_entity, &kind)) {
              break;
            }  /* if */
            cache_token(&cache, tok_template);
            mod->cache_chart(&cache, get_ifc_chart(itf), /*cinfo=*/{});
            cache_token(&cache, tok_using);
            cache_identifier(&cache, get_string_at_offset(get_ifc_name(ida)));
            cache_token(&cache, tok_assign);
            mod->cache_type(&cache, get_ifc_subject(itf));
            cache_token(&cache, tok_semicolon);
            if (!cache.is_valid()) {
              goto invalid;
            }  /* if */
            il_entity = parse_cached_template(&cache, mep->scope, &kind);
            if (kind != iek_template) {
              goto invalid;
            }  /* if */
          } else {
            a_string err_msg("Unexpected ", str_for(type.sort));

            ifc_unexpected(mod, err_msg);
            goto invalid;
          }  /* if */
        }  /* if */
      }
      break;
    case ifc_ds_decl_enumeration:
      { Opt<an_ifc_decl_enumeration> opt_ide;

        construct_node(&opt_ide, decl_idx);
        if (!opt_ide.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_enumeration          ide = *opt_ide;
        an_ifc_source_location           locus = get_ifc_locus(ide);
        an_ifc_type_index                type = get_ifc_type(ide);
        an_ifc_basic_specifiers_bitfield specifiers = get_ifc_specifiers(ide);
        /* FIXME: This does a lot of stuff even when deferred. */
        if (!source_position_from_locus(&error_position, locus)) {
          goto invalid;
        }  /* if */
        if (is_from_gmf(specifiers)) {
          mep->global_module = TRUE;
        }  /* if */

        Opt<an_ifc_type_fundamental> opt_itf;
        construct_node(&opt_itf, type);
        if (!opt_itf.has_value()) {
          goto invalid;
        }  /* if */

        /* See if this is a scoped enumeration or not. */
        an_ifc_type_fundamental itf = *opt_itf;
        an_ifc_type_basis_sort  basis = get_ifc_basis(itf);
        a_boolean               is_scoped_enum = FALSE;
        if (basis == ifc_tbs_enum) {
          /* A classic enumeration.  Don't bother to defer in this case because
             each of the enumerators needs to be registered in the symbol table
             so they can be found. */
          if (defer) {
            defer = FALSE;
            mep->imminent = TRUE;
            push_module_declaration_context(mep->scope, &scope_push_status);
          }  /* if */
        } else if (basis == ifc_tbs_class || basis == ifc_tbs_struct) {
          /* A scoped enumeration.  Enumerator definitions are deferred when
             the enumeration definition is deferred (enumerators can't be
             referred to without specifying the scoped enumeration at which
             time the enumerators will be made available). */
          is_scoped_enum = TRUE;
        } else {
          a_string err_msg("Unexpected ", str_for(basis));

          ifc_unexpected(mod, err_msg);
          goto invalid;
        }  /* if */

        an_ifc_text_offset enum_name = get_ifc_name(ide);
        if (!mod->init_locator_from_name(enum_name, locus, &loc)) {
          goto invalid;
        }  /* if */
        /* FIXME: This should be a soft failure. */
        check_assertion(enum_name != 0);
        if (defer) {
          defer_symbol_creation(mep, &loc);
        } else {
          an_ifc_type_index base = get_ifc_base(ide);
          a_type_ptr        enum_type;
          a_scope_ptr       enum_scope;
          a_symbol_ptr      tag_sym;
          a_scope_depth     scope_depth;

          check_assertion(!is_null_index(base));
          if (mep->scope == NULL) {
            mep->scope = get_home_scope(ide);
            push_module_declaration_context(mep->scope, &scope_push_status);
          }  /* if */
          check_assertion(mep->scope != NULL);
          enum_scope = mep->scope;
          scope_depth = enum_scope->depth_in_scope_stack;
          if (check_and_set_redeclaration(&loc, mep, &error_position,
                                          iek_type, &il_entity, &kind)) {
            /* FIXME: Is this right? */
            if (is_defined(il_entity, kind)) {
              break;
            }  /* if */
          }  /* if */
          mod->init_dps(&dps, locus, base, an_ifc_object_traits_bitfield{},
                        an_ifc_msvc_traits_bitfield{}, specifiers,
                        get_ifc_access(ide), get_ifc_alignment(ide), &psss);
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
            /* Alignment was not specifically specified; use the alignment of
               the base type. */
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
          if (is_unnamed_tag(loc.symbol_header->identifier)) {
            Value_saver<a_scope_depth>  saver(&decl_scope_level,
                                              /*new_value=*/scope_depth);
            tag_sym = make_unnamed_tag_symbol((a_symbol_kind)sk_enum_tag,
                                              &loc.source_position);
            enum_type->variant.integer.originally_unnamed = TRUE;
          } else {
            tag_sym = enter_local_symbol(sk_enum_tag, &loc, scope_depth,
                                         /*suppress_redecl_error=*/FALSE);
          } /* if */
          tag_sym->variant.enumeration.type = enum_type;
          set_source_corresp(&(enum_type->source_corresp), tag_sym);
#if NEED_NAME_MANGLING
          compute_name_collision_discriminator(tag_sym, scope_depth);
#endif /* NEED_NAME_MANGLING */
          add_to_types_list(enum_type, scope_depth);
          /* Set parent membership. */
          if (scope_is(mep->scope, sck_class_struct_union)) {
            /* FIXME: never get here because enumerations in classes are loaded
               via the string mechanism. */
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
          an_ifc_sequence             initializer = get_ifc_initializer(ide);
          a_decl_enumerator_traverser traverser(initializer);
          for (an_Indexed<an_ifc_decl_enumerator> idxed_enmtr : traverser) {
            if (!idxed_enmtr.has_value()) {
              goto invalid;
            }  /* if */

            an_ifc_decl_index   enumerator_idx =
                                           to_decl_index(idxed_enmtr.node_idx);
            a_module_entity_ptr emep =
                                     get_ifc_module_entity_ptr(enumerator_idx);
            emep->scope = enum_scope;
            mod->process_ifc_declaration(emep, /*defer=*/FALSE);
          }  /* for */
          integer_type_supp(enum_type)->enumerator_list_seen = TRUE;
          if (is_scoped_enum) {
            pop_scope();
          }  /* if */
          restore_partial_scope_stack_if_necessary(&psss);
        }  /* if */
      }
      break;
    case ifc_ds_decl_enumerator:
      { Opt<an_ifc_decl_enumerator> opt_ide;

        construct_node(&opt_ide, decl_idx);
        if (!opt_ide.has_value()) {
          goto invalid;
        }  /* if */
        if (!mod->init_decl_locator(*opt_ide, &loc)) {
          goto invalid;
        }  /* if */
        /* FIXME: This should be a soft failure. */
        check_assertion(get_ifc_name(*opt_ide) != 0);
        if (defer) {
          defer_symbol_creation(mep, &loc);
        } else {
          an_ifc_decl_enumerator ide = *opt_ide;
          a_symbol_ptr           enum_con_sym;
          a_constant_ptr         enum_con;
          a_memory_region_number region_to_switch_back_to;

          if (is_from_gmf(get_ifc_specifiers(ide))) {
            mep->global_module = TRUE;
          }  /* if */
          if (mep->scope == NULL) {
            mep->scope = get_home_scope(ide);
            check_assertion(mep->scope != NULL);
          }  /* if */

          an_ifc_type_index  type_idx = get_ifc_type(ide);
          a_type_ptr         enum_type = type_for_type_index(type_idx,
                                                             /*kind=*/NULL);
          if (is_error_type(enum_type)) {
            goto invalid;
          }  /* if */
          if (check_and_set_redeclaration(&loc, mep, &error_position,
                                          iek_constant, &il_entity, &kind)){
            break;
          }  /* if */
          switch_to_file_scope_region(&region_to_switch_back_to);
          enum_con = mod->constant_for_expr_index(get_ifc_initializer(ide),
                                                  enum_type);
          switch_back_to_original_region(region_to_switch_back_to);
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
          /* FIXME: not currently doing anything with specifiers or access. */
          /* Add the constant to the list of constants for the enumeration type
             (note that the order of enumerators on the list is not necessarily
             the order that they appear in the enumeration, rather it's the
             order they are accessed in if definitions are deferred). */
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
    case ifc_ds_decl_template:
      { Opt<an_ifc_decl_template> opt_idt;

        construct_node(&opt_idt, decl_idx);
        if (!opt_idt.has_value()) {
          goto invalid;
        }  /* if */
        if (!mod->init_decl_locator(*opt_idt, &loc)) {
          goto invalid;
        }  /* if */

        an_ifc_decl_template idt = *opt_idt;
#if CHECKING
        {
          an_ifc_type_index ifc_type = get_ifc_type(idt);
          a_boolean         is_deduction_guide = FALSE;

          if (is_null_index(ifc_type)) {
            /* Deduction guide templates have no associated type.  They are
               driven by the IFC "traits" system instead of by lookup (which
               means they are never "deferred"). */
            is_deduction_guide = TRUE;
            check_assertion(!defer);
          }  /* if */
          /* FIXME: This should be a soft failure. */
          check_assertion(!is_null_index(get_ifc_name(idt)) ||
                          is_deduction_guide);
        }
#endif /* CHECKING */
        if (defer) {
          defer_symbol_creation(mep, &loc);
        } else {
          an_ifc_basic_specifiers_bitfield
                                 specifiers = get_ifc_specifiers(idt);
          a_curr_token_preserver guard;
          an_ifc_cache_info      cache_info;

          if (is_from_gmf(specifiers)) {
            mep->global_module = TRUE;
          }  /* if */
          if (mep->scope == NULL) {
            mep->scope = get_home_scope(idt);
            push_module_declaration_context(mep->scope, &scope_push_status);
          }  /* if */

          if (check_and_set_template_redeclaration(&loc, mep, &error_position,
                                                   &il_entity, &kind)) {
            a_template_ptr templ = (a_template_ptr)il_entity;

            update_cache_info_for_template(&cache_info, templ);
            /* Skip any further processing when the variable template is not
               declared "extern" (as variable template declarations cannot
               otherwise be meaningfully "merged"). */
            if (templ->kind == templk_variable) {
              a_variable_ptr variable =
                                       templ->prototype_instantiation.variable;

              if (variable->storage_class != sc_extern) {
                goto done;
              }  /* if */
            }  /* if */
          }  /* if */
          /* Compute the DeclIndex of the current template and retrieve the
             sequence of explicit specializations and instantiations. */
          an_ifc_template_spec_info spec_info(decl_idx);
          /* FIXME: Find a better way to handle self references; currently we
             forward declare in almost all cases in part to handle self
             references. */
          an_ifc_decl_index         entity_decl =
                                             get_ifc_decl(get_ifc_entity(idt));
          a_boolean                 forward_declare =
                                              (!cache_info.ignore_definition &&
                                   (entity_decl.sort != ifc_ds_decl_variable));
          a_boolean                 defer_definition = FALSE;
          if (forward_declare) {
            a_module_token_cache cache;
            an_ifc_cache_info    local_cache_info = cache_info;

            /* Disable definition caching, this is a forward declaration for
               the partial specializations that follow. */
            local_cache_info.ignore_definition = TRUE;

            mod->cache_decl_template(&cache, idt, local_cache_info);
            if (!cache.is_valid()) {
              goto invalid;
            }  /* if */
            il_entity = parse_cached_template(&cache, mep->scope, &kind);
            if (kind != iek_template) {
              goto invalid;
            }  /* if */
            if (is_file_or_namespace_scope(mep->scope)) {
              defer_definition = TRUE;
            }  /* if */
            if (spec_info.has_specs()) {
              spec_info.process_specializations();
            }  /* if */
          }  /* if */
          if (defer_definition) {
            check_assertion(il_entity != NULL && kind == iek_template);
            record_pending_ifc_template_definition((a_template_ptr)il_entity,
                                                   decl_idx);
          } else {
            /* If the definition hasn't been deferred (from a compatible
               forward declaration being processed), process the definition
               now.  If the declaration was not forward declared, be sure to
               process any specializations as part of definition processing. */
            if (!process_template_definition(
                                            idt, mep, spec_info,
                                            /*process_specs=*/!forward_declare,
                                            &il_entity,
                                            &kind)) {
              goto invalid;
            }  /* if */
          }  /* if */
        }  /* if */
      }
      break;
    case ifc_ds_decl_parameter:
      { Opt<an_ifc_decl_parameter> opt_idp;

        construct_node(&opt_idp, decl_idx);
        if (!opt_idp.has_value()) {
          goto invalid;
        }  /* if */
        if (!mod->init_decl_locator(*opt_idp, &loc)) {
          goto invalid;
        }  /* if */
        /* FIXME: This should be a soft failure. */
        check_assertion(get_ifc_name(*opt_idp) != 0);

        an_ifc_decl_parameter     idp = *opt_idp;
        an_ifc_type_index         type_idx = get_ifc_type(idp);
        a_type_ptr                param_type;
        a_template_param_ptr      param = NULL, *next_param;
        a_template_parameter_ptr  il_param = NULL;
        a_boolean                 is_pack = FALSE;
        /* FIXME: constraint_expr = expr_for_expr_index(idspp->constraint);*/
        /* FIXME: init_expr = expr_for_expr_index(idspp->initializer); */
        if (type_idx.sort == ifc_ts_type_expansion) {
          /* FIXME: Currently unsupported. */
          is_pack = TRUE;
          issue_unsupported_construct_error(mod, "DeclSort::Parameter packs",
                                            &error_position);
        }  /* if */
        /* FIXME: Currently all paths that lead here have
           curr_templ_decl_state == NULL. */
        switch (get_ifc_sort(idp)) {
          case ifc_ps_object:
            /* FIXME: Currently unsupported. */
            issue_unsupported_construct_error(mod, "ParameterSort::Object",
                                              &error_position);
            param = make_nontype_template_param(get_ifc_level(idp),
                                                get_ifc_position(idp),
                                                /*is_unnamed=*/FALSE,
                                                is_pack,
                                                /*is_pack_element=*/FALSE,
                                                /*is_non_initial=*/FALSE,
                                                /*is_pack_expansion=*/FALSE,
                                                &loc, error_type(),
                                                mod->curr_templ_decl_state);
            break;
          case ifc_ps_type:
            /* FIXME: Handle unnamed parameters properly */
            param = decl_type_template_param(get_ifc_position(idp), &loc,
                                             /*is_named=*/TRUE, is_pack,
                                             /*constraint=*/NULL,
                                             mod->curr_templ_decl_state,
                                             &mod->curr_templ_decl_state->
                                                               decl_pos_block);
            break;
          case ifc_ps_non_type:
            param_type = type_for_type_index(get_ifc_type(idp), /*kind=*/NULL);
            /* FIXME: Handle unnamed parameters properly. */
            param = make_nontype_template_param(get_ifc_level(idp),
                                                get_ifc_position(idp),
                                                /*is_unnamed=*/FALSE,
                                                is_pack,
                                                /*is_pack_element=*/FALSE,
                                                /*is_non_initial=*/FALSE,
                                                /*is_pack_expansion*/FALSE,
                                                &loc, param_type,
                                                mod->curr_templ_decl_state);
            break;
          case ifc_ps_template:
            /* FIXME: Currently unsupported. */
            issue_unsupported_construct_error(mod, "ParameterSort::Template",
                                              &error_position);
            param = make_nontype_template_param(get_ifc_level(idp),
                                                get_ifc_position(idp),
                                                /*is_unnamed=*/FALSE,
                                                is_pack,
                                                /*is_pack_element=*/FALSE,
                                                /*is_non_initial=*/FALSE,
                                                /*is_pack_expansion=*/FALSE,
                                                &loc, error_type(),
                                                mod->curr_templ_decl_state);
            break;
          default_is_unexpected_str("Unexpected ParameterSort");
        }  /* switch */
        if (param != NULL) {
          next_param = &mod->curr_templ_decl_state->decl_info->parameters;
          /* Skip to end of parameter list. */
          for (; *next_param != NULL; next_param = &(*next_param)->next) {}
          *next_param = param;
          ++mod->curr_templ_decl_state->decl_info->n_params;
          il_param = alloc_template_parameter();
          param->il_template_parameter = il_param;
          param->param_symbol->is_invisible = FALSE;
          il_entity = (char*)il_param;
          kind = iek_template_parameter;
        }  /* if */
      }
      break;
    case ifc_ds_decl_reference:
      { Opt<an_ifc_decl_reference> opt_idr;

        construct_node(&opt_idr, decl_idx);
        if (!opt_idr.has_value()) {
          goto invalid;
        }  /* if */

        a_module_entity_ptr dmep = process_decl_via_reference(*opt_idr);
        il_entity = dmep->entity.ptr;
        kind = dmep->entity.kind;
        /* The module entity pointer sometimes already has a scope (e.g.,
           from lighter nested-name-specified processing). */
        ifc_requirement(get_assoc_ifc_module(dmep),
                        mep->scope == NULL || dmep->scope == mep->scope,
                        "expected a matching scope");
        mep->scope = dmep->scope;
      }
      break;
    case ifc_ds_decl_method:
    case ifc_ds_decl_constructor:
    case ifc_ds_decl_destructor:
    case ifc_ds_decl_field:
    case ifc_ds_decl_bitfield:
    case ifc_ds_decl_property:
      /* These entities can only exist in a class and class definitions are
         currently handled by scanning a token representation of the class. */
      { a_string err_msg("Unexpected ", str_for(decl_idx.sort));

        ifc_unexpected(mod, err_msg);
      }
      goto invalid;
    case ifc_ds_decl_temploid:
      { Opt<an_ifc_decl_temploid> opt_idt;

        construct_node(&opt_idt, decl_idx);
        if (!opt_idt.has_value()) {
          goto invalid;
        }  /* if */
        /* FIXME: Need a proper source position here. */
        error_position = null_source_position;
        goto unhandled;
      }
    case ifc_ds_decl_partial_specialization:
      { Opt<an_ifc_decl_partial_specialization> opt_idps;

        construct_node(&opt_idps, decl_idx);
        if (!opt_idps.has_value()) {
          goto invalid;
        }  /* if */
        if (!mod->init_decl_locator(*opt_idps, &loc)) {
          goto invalid;
        }  /* if */
        /* Partial specializations should not be added to the deferred symbol
           list as they'll be processed when the primary template is processed
           via the associated entries in "trait.specialization". */
        if (defer) {
          associate_spec_with_template(decl_idx, *opt_idps);
        } else {
          an_ifc_decl_partial_specialization idps = *opt_idps;

          mod->lazy_push_module_scope(idps, mep, &scope_push_status);
          /* FIXME: Is it feasible to detect ignorable redeclarations of
             partial specializations? */
          if (get_ifc_body(get_ifc_entity(idps)) != 0) {
            a_module_token_cache cache;

            /* There is a definition of the partial specialization.  Record the
               resolution of the signature immediately so that the below
               processing of the definition has access to it. */
            mod->cache_decl_partial_specialization(&cache, decl_idx, idps);
            if (!cache.is_valid()) {
              goto invalid;
            }  /* if */
            il_entity = parse_cached_partial_specialization(&cache,
                                                            mep->scope,
                                                            &kind);
            if (kind != iek_template) {
              goto invalid;
            }  /* if */
          }  /* if */
        }  /* if */
      }
      break;
    case ifc_ds_decl_specialization:
      { Opt<an_ifc_decl_specialization> opt_ids;

        construct_node(&opt_ids, decl_idx);
        if (!opt_ids.has_value()) {
          goto invalid;
        }  /* if */
        if (!mod->init_decl_locator(*opt_ids, &loc)) {
          goto invalid;
        }  /* if */
        /* Specializations should not be added to the deferred symbol list as
           they'll be processed when the primary template is processed via the
           associated entries in "trait.specialization". */
        if (defer) {
          associate_spec_with_template(decl_idx, *opt_ids);
        } else {
          an_ifc_decl_specialization ids = *opt_ids;
          a_module_token_cache       cache;
          an_ifc_cache_info          cache_info;

          mod->lazy_push_module_scope(ids, mep, &scope_push_status);
          if (check_and_set_specialization_redeclaration(&loc, mep, ids,
                                                         &error_position,
                                                         &il_entity,
                                                         &kind)) {
            /* Specializations are fairly simple in nature; if the
               specialization is already declared and already defined, there's
               nothing to add; skip processing. */
            if (is_defined(il_entity, kind)) {
              break;
            }  /* if */
          }  /* if */
          mod->cache_decl_specialization(&cache, decl_idx, ids, cache_info);
          if (!cache.is_valid()) {
            goto invalid;
          }  /* if */
          if (get_ifc_sort(ids) == ifc_ss_instantiation) {
            il_entity = mod->parse_cached_explicit_instantiation(&cache, ids,
                                                                 &kind);
          } else {
            il_entity = mod->parse_cached_explicit_specialization(&cache,
                                                                  mep->scope,
                                                                  ids,
                                                                  &kind);
          }  /* if */
          if (kind != iek_template) {
            goto invalid;
          }  /* if */
        }  /* if */
      }
      break;
    case ifc_ds_decl_concept:
      { Opt<an_ifc_decl_concept> opt_idc;

        construct_node(&opt_idc, decl_idx);
        if (!opt_idc.has_value()) {
          goto invalid;
        }  /* if */
        if (!mod->init_decl_locator(*opt_idc, &loc)) {
          goto invalid;
        }  /* if */
        if (defer) {
          defer_symbol_creation(mep, &loc);
        } else {
          /* Create a definition for the concept and scan it. */
          an_ifc_decl_concept    idc = *opt_idc;
          a_curr_token_preserver guard;

          if (is_from_gmf(get_ifc_specifiers(idc))) {
            mep->global_module = TRUE;
          }  /* if */
          /* Activate the parent scope if needed. */
          if (mep->scope == NULL) {
            mep->scope = get_home_scope(idc);
            push_module_declaration_context(mep->scope, &scope_push_status);
          }  /* if */

          a_module_token_cache cache;
          an_ifc_cache_info    cache_info;
          if (check_and_set_redeclaration(&loc, mep, &error_position,
                                          iek_template, &il_entity, &kind)) {
            goto done;
          }  /* if */
          cache_direct_decl(&cache, idc, cache_info);
          /* Parse the definition cache. */
          if (!cache.is_valid()) {
            goto invalid;
          }  /* if */
          il_entity = parse_cached_template(&cache, mep->scope, &kind);
        }  /* if */
      }
      break;
    case ifc_ds_decl_inherited_constructor:
      { Opt<an_ifc_decl_inherited_constructor> opt_idic;

        construct_node(&opt_idic, decl_idx);
        if (!opt_idic.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_source_location locus = get_ifc_locus(*opt_idic);
        source_position_from_locus(&error_position, locus);
        goto unhandled;
      }
    case ifc_ds_decl_output_segment:
      { Opt<an_ifc_decl_output_segment> opt_idos;

        construct_node(&opt_idos, decl_idx);
        if (!opt_idos.has_value()) {
          goto invalid;
        }  /* if */
        /* FIXME: Need a proper source position here. */
        error_position = null_source_position;
        goto unhandled;
      }
    case ifc_ds_decl_using_declaration:
      { Opt<an_ifc_decl_using_declaration> opt_idud;

        construct_node(&opt_idud, decl_idx);
        if (!opt_idud.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_using_declaration idud = *opt_idud;
        an_ifc_source_location        locus = get_ifc_locus(idud);
        if (defer) {
          if (!mod->init_locator_from_name(get_ifc_name(idud), locus, &loc)) {
            goto invalid;
          }  /* if */
          defer_symbol_creation(mep, &loc);
        } else {
          an_ifc_decl_index resolution = get_ifc_resolution(idud);

          if (!source_position_from_locus(&error_position, locus)) {
            goto invalid;
          }  /* if */
          if (resolution.sort == ifc_ds_decl_tuple) {
            /* FIXME: Overload set? */
            goto unhandled;
          }  /* if */
          a_module_entity_ptr umep = process_decl_at_index(resolution);
          if (scope_is(umep->scope, sck_class_struct_union)) {
            /* FIXME: Need to call create_member_using_declaration here. */
            goto unhandled;
          } else {
            /* Non-member using declaration.  Could be file scope or namespace
               scope. */
            a_symbol_ptr            null_sym_ptr = NULL, aliased_sym;
            a_using_decl_ptr        prev_udp = NULL;
            a_namespace_ptr         nsp = NULL;
            a_source_correspondence *scp =
                                    (a_source_correspondence*)umep->entity.ptr;
            aliased_sym = (a_symbol_ptr)scp->assoc_info;
            if (scp->parent_scope == NULL) {
              /* Probably shouldn't happen, but happens now because of other
                 issues. */
              goto unhandled;
            }  /* if */
            if (scope_is(scp->parent_scope, sck_namespace)) {
              nsp = scp_parent_namespace(scp);
            }  /* if */
            if (symbol_is(aliased_sym, sk_namespace)) {
              /* A namespace alias. */
              a_symbol_ptr  ns_sym;
              if (!mod->init_locator_from_name(get_ifc_name(idud), locus,
                                               &loc)) {
                goto invalid;
              }  /* if */
              /* Check for a conflict with another declaration. */
              ns_sym = curr_scope_id_lookup(&loc, IDL_NO_OPTIONS);
              if (ns_sym != NULL) {
                /* The alias name is already present in this scope.  That is
                   okay only if it names an equivalent alias. */
                a_boolean  mismatch = FALSE;
                if (!symbol_is(ns_sym, sk_namespace)) {
                  mismatch = TRUE;
                } else {
                  a_namespace_ptr  prev = ns_sym->variant.namespace_info.ptr;
                  if (!prev->is_namespace_alias) {
                    mismatch = TRUE;
                  } else if (skip_namespace_aliases(prev) !=
                             skip_namespace_aliases(
                                    aliased_sym->variant.namespace_info.ptr)) {
                    mismatch = TRUE;
                  }  /* if */
                }  /* if */
                if (mismatch) {
                  pos_sy_error(ec_already_defined, &error_position, ns_sym);
                }  /* if */
              }  /* if */
              (void)make_namespace_alias(ns_sym, &loc, aliased_sym,
                                         (a_source_sequence_entry*)NULL);
            } else {
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
                                                /*suppress_redecl_error=*/TRUE,
                                                (an_attribute_ptr)NULL,
                                                /*copy_attributes=*/FALSE);
            }  /* if */
          }  /* if */
        }  /* if */
      }
      break;
    case ifc_ds_decl_friend:
      { Opt<an_ifc_decl_friend> opt_idf;

        construct_node(&opt_idf, decl_idx);
        if (!opt_idf.has_value()) {
          goto invalid;
        }  /* if */
        /* FIXME: Need a proper source position here. */
        error_position = null_source_position;
        goto unhandled;
      }
    case ifc_ds_decl_tuple:
      /* An overload set. */
      { Opt<an_ifc_decl_tuple> opt_idt;

        construct_node(&opt_idt, decl_idx);
        if (!opt_idt.has_value()) {
          goto invalid;
        }  /* if */

        /* Collect the entries of the overload set in a list of IL entries
           (an_il_entity_list_entry_ptr). */
        a_decl_heap_traverser  traverser(*opt_idt);
        a_scope_ptr            scope = mep->scope;
        for (an_Indexed<an_ifc_heap_decl> indexed_ihd : traverser) {
          if (!indexed_ihd.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_decl_index   heap_value = get_ifc_value(*indexed_ihd);
          a_module_entity_ptr emep = get_ifc_module_entity_ptr(heap_value);
          /* In at least some cases (the handling of deduction guides), the
             caller will have filled in mep->scope and that should be
             propagated to the individual associated declarations. */
          if (scope != NULL) emep->scope = scope;
          mod->process_ifc_declaration(emep, /*defer=*/FALSE);
          if (scope == NULL) mep->scope = emep->scope;

          a_source_correspondence  *scp;
          scp = source_corresp_for_il_entry(emep->entity.ptr,
                                            emep->entity.kind);
          if (scp == NULL || scp->assoc_info == NULL) {
            /* Something went wrong loading this member of the set.  Ignore it
               in what follows. */
          } else {
            an_il_entity_list_entry_ptr ielep = alloc_il_entity_list_entry();

            ielep->next = (an_il_entity_list_entry*)il_entity;
            il_entity = (char*)ielep;
            ielep->entity = emep->entity;
          }  /* if */
        }  /* for */
        kind = iek_il_entity_list_entry;
      }
      break;
    case ifc_ds_decl_expansion:
      { Opt<an_ifc_decl_expansion> opt_ide;

        construct_node(&opt_ide, decl_idx);
        if (!opt_ide.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_source_location locus = get_ifc_locus(*opt_ide);
        source_position_from_locus(&error_position, locus);
        goto unhandled;
      }
    case ifc_ds_decl_deduction_guide:
      { Opt<an_ifc_decl_deduction_guide> opt_iddg;

        construct_node(&opt_iddg, decl_idx);
        if (!opt_iddg.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_source_location locus = get_ifc_locus(*opt_iddg);
        source_position_from_locus(&error_position, locus);
        goto unhandled;
      }
    case ifc_ds_decl_barren:
    case ifc_ds_decl_using_directive:
    case ifc_ds_decl_syntax_tree:
      { /* FIXME: Need a proper source position here. */
        error_position = null_source_position;
unhandled:
        issue_unsupported_construct_error(mod, str_for(decl_idx.sort),
                                          &error_position);
        il_entity = (char *)error_type();
        kind = iek_type;
      }
      break;
    default_is_unexpected_str("Unexpected DeclSort");
  }  /* switch */
  goto done;
invalid:
#if DEBUG
  ++num_module_decls_failed;
#endif /* DEBUG */
  mep->invalid = TRUE;
done:
  if (!defer) {
    /* Handle all cases where we didn't successfully consturct an IL entity by
       marking the mep invalid. */
    if (il_entity == NULL) {
      mep->invalid = TRUE;
    }  /* if */
#if DEBUG
    if (db_flag_is_set("ms_symbols")) {
      (void)fprintf(f_debug, "Module entity defined: ");
      db_module_entity(mep);
    }  /* if */
#endif /* DEBUG */
    /* If a scope was pushed, scope popping should always occur. */
    if (scope_push_status != mspk_unattempted) {
      pop_module_declaration_context(scope_push_status);
    }  /* if */
  }  /* if */
#if DEBUG
  if (db_flag_is_set("ifc_idx")) {
    (void)fprintf(f_debug, "Done with %s (%d)\n", str_for(decl_idx.sort),
                  decl_idx.value);
  }  /* if */
#endif /* DEBUG */
  curr_module_entity = saved_mep;
  error_position = saved_error_position;
}  /* process_decl_to_il_entity */


void an_ifc_module::process_ifc_declaration(a_module_entity_ptr mep,
                                            a_boolean           defer)
/*
Process the IFC module entity declaration specified by mep.  When defer is
TRUE, no IL entity is created; instead, the appropriate symbol header is
updated to note the deferred entity for lazy loading.  When defer is FALSE, we
immediately process *mep and attempt to form an IL entity; if no IL entity can
be formed, *mep will marked as invalid.
*/
{

  /* Ensure the module entity is being processed by the corresponding module
     interface. */
  check_assertion(mep->module_info->module_interface == this);
#if DEBUG
  static unsigned long nested_decls = 0;
  if (db_flag_is_set("ifc_decl")) {
    (void)fprintf(f_debug, "[>%lu]%s ", nested_decls++,
                                        defer ? " (deferred)" : "");
    db_mep(mep);
  }  /* if */
#endif /* DEBUG */
  if (!mep->imminent && mep->entity.ptr == NULL) {
    process_decl_to_il_entity(mep, defer);
  }  /* if */
  /* In some cases we may enter this routine without having a scope, but we
     should not exit it without having one. */
  check_assertion(mep->invalid || mep->scope != NULL);
#if DEBUG
  if (db_flag_is_set("ifc_decl")) {
    (void)fprintf(f_debug, "[<%lu] ", --nested_decls);
    db_mep(mep);
  }  /* if */
#endif /* DEBUG */
}  /* process_ifc_declaration */


template<typename a_Scope_member_consumer>
static a_boolean traverse_scope_member_range(
                                           an_ifc_index            start,
                                           an_ifc_cardinality      cardinality,
                                           a_Scope_member_consumer consumer)
/*
Traverse "cardinality" elements from the given scope member range from the
starting position (start), in start's module.  Validated scope members are
passed to the given consumer.  Upon completion of the traversal this function
returns TRUE if all elements were passed to the consumer; otherwise, returns
FALSE.
*/
{
  a_boolean                result = TRUE;
  a_scope_member_traverser traverser(start.mod, start, cardinality);

  for (an_Indexed<an_ifc_scope_member> indexed_ism : traverser) {
    /* Allow single failures to be "ignored", continue processing. */
    if (!indexed_ism.has_value()) {
      result = FALSE;
      continue;
    }  /* if */
    /* Handle any specific processing in the consumer. */
    consumer(*indexed_ism);
  }  /* for */
  return result;
}  /* traverse_scope_member_range */


template<typename a_Scope_member_consumer>
static a_boolean traverse_scope_members(an_ifc_scope_index      scope,
                                        a_Scope_member_consumer consumer)
/*
Traverse the scope referenced by the given scope index, passing any encountered
scope members to the given consumer.  This function returns TRUE if the
traversal was completed for all elements; otherwise it returns FALSE.
*/
{
  a_boolean result = TRUE;

  /* A scope index of 0 indicates a missing scope, in which case there
     is nothing further to do. */
  if (scope != 0) {
    Opt<an_ifc_scope_descriptor> opt_isd;
    /* Scope indices are 1-based, so subtract one. */
    an_ifc_partition_kind_index  scope_desc_index{scope.mod, ifc_pk_scope_desc,
                                                  scope.value - 1};

    if (!validate_element_exists(scope.mod, ifc_pk_scope_desc, scope.value - 1,
                                 /*parent=*/NULL)) {
      goto done;
    }  /* if */
    construct_node(&opt_isd, scope_desc_index);
    if (!opt_isd.has_value()) {
      result = FALSE;
      goto done;
    }  /* if */

    an_ifc_scope_descriptor isd = *opt_isd;
    an_ifc_index            start = get_ifc_start(isd);
    an_ifc_cardinality      cardinality = get_ifc_cardinality(isd);
    result = traverse_scope_member_range(start, cardinality, consumer);
  }  /* if */
done:
  return result;
}  /* traverse_scope_members */


static void cache_scope(an_ifc_module            *mod,
                        a_module_token_cache_ptr cache,
                        an_ifc_scope_index       scope)
/*
For the given IFC scope (i.e., a class or namespace definition), cache tokens
corresponding to the brace-enclosed declarations of the scope (including the
braces).  Note that in the case of a class scope this does not include the base
class specifiers list.  A null IFC scope is handled by not caching any tokens.
*/
{
  auto cache_scope_member = [mod, cache](const an_ifc_scope_member &ism) {
    mod->cache_decl(cache, get_ifc_index(ism), /*cinfo=*/{});
  };

  if (scope != 0) {
    cache_token(cache, tok_lbrace);
    traverse_scope_members(scope, cache_scope_member);
    cache_token(cache, tok_rbrace);
  }  /* if */
}  /* cache_scope */


static void add_friend_to_class(a_type_ptr    class_type,
                                a_symbol_ptr  sym)
/*
Record the entity described by sym as being a "friend" of the given class type.
sym might be a class type, a function or member function, or a class or
function template.  If sym is NULL, it is simply ignored.
*/
{
  if (sym == NULL) {
    /* Something went wrong upstream.  Ignore this call. */
  } else if (is_class_struct_union_symbol(sym)) {
    decl_friend_class(class_type, sym->variant.class_struct_union.type,
                      /*for_friend_template=*/FALSE,
                      (a_decl_pos_block*)NULL);
  } else if (is_simple_function_symbol(sym)) {
    update_friend_function_info(sym->variant.routine.ptr, class_type);
  } else if (symbol_is(sym, sk_function_template)) {
    add_friend_function_to_lookup_list_for_class(sym, class_type);
    add_befriending_class_to_function_template(sym->variant.template_info,
                                               class_type);
  } else if (symbol_is(sym, sk_class_template)) {
    add_befriending_class_to_class_template(sym->variant.template_info,
                                            class_type);
  }  /* if */
}  /* add_friend_to_class */


static void add_ifc_friends_to_class(an_ifc_module      *mod,
                                     a_type_ptr         class_type,
                                     an_ifc_decl_index  class_idx)
/*
The given class_type associated with the given module and IFC declaration
index has just been completed.  Record its associated friend entities.  This
function is called after the class is completed because IFC "friend
declarations" may include template definitions that rely on the completeness
of the class type.
*/
{
  /* Check if there are friends. */
  Opt<an_ifc_trait_friend>  opt_friends;

  find_trait(&opt_friends, class_idx);
  if (opt_friends.has_value()) {
    /* There are friends: Record them. */
    Opt<an_ifc_sequence>  friends = get_ifc_trait(*opt_friends);
    if (friends.has_value()) {
      mod->traverse_scope_member_sequence(
        *friends,
        [=](const an_ifc_scope_member  &ism) {
          an_ifc_decl_index  friend_decl_idx = get_ifc_index(ism);
          Opt<an_ifc_decl_friend> opt_df;
          construct_node(&opt_df, friend_decl_idx);
          if (opt_df.has_value()) {
            an_ifc_decl_friend friend_decl = *opt_df;
            an_ifc_expr_index  friend_id = get_ifc_entity(friend_decl);
            add_friend_to_class(class_type, load_ifc_entity_ref(friend_id));
          }  /* if */
        });
    }  /* if */
  }  /* if */
}  /* add_ifc_friends_to_class */


void an_ifc_module::complete_definition_of_module_class(
                                                       a_module_entity_ptr mep)
/*
Complete the definition of the class referred to by mep (if needed).
*/
{
  a_diagnostic_suppression diag_suppress(&this->suppressed_diagnostics,
                                         !display_module_import_diagnostics);
  Opt<an_ifc_decl_scope>   opt_ids;
  an_ifc_decl_index        decl_idx = decl_index_of(mep);

  construct_node(&opt_ids, decl_idx);
  /* FIXME: Error handling could be improved here. */
  if (opt_ids.has_value()) {
    an_ifc_decl_scope  ids = *opt_ids;
    an_ifc_scope_index initializer = get_ifc_initializer(ids);
    a_type_ptr         class_type = (a_type_ptr)mep->entity.ptr;

    check_assertion(mep->entity.kind == (an_il_entry_kind)iek_type &&
                    class_type != NULL);
    if (class_type->incomplete && initializer != 0) {
      a_template_decl_info_ptr    tdip;
      a_symbol_ptr                class_sym = symbol_for(class_type);
      a_scope_depth               saved_non_local_class_fixup_depth =
                                                   non_local_class_fixup_depth;
      a_source_position           saved_error_position = error_position;
      a_module_scope_push_kind    scope_push_status = mspk_unattempted;
      a_curr_token_preserver      guard;
      a_ms_mode_parse             tmp_ms_parse(this);
      a_module_entity_stack_state mep_state(mep);
      a_module_token_cache        cache;
      an_ifc_source_location      locus = get_ifc_locus(ids);
      an_ifc_source_position_hint pos_hint(&cache, locus);

      push_module_declaration_context(mep->scope, &scope_push_status);
      error_position = *pos_hint.as_pos();
      {
        an_ifc_type_index  base = get_ifc_base(ids);
        if (!is_null_index(base)) {
          /* There are base classes: Cache source code for them. */
          cache_token(&cache, tok_colon);
          this->cache_type(&cache, base);
        }  /* if */
        an_ifc_scope_index  class_members = get_ifc_initializer(ids);
        if (class_members != 0) {
          cache_token(&cache, tok_lbrace);
          /* Emit ordinary members: */
          auto cache_member = [this, &cache](const an_ifc_scope_member  &ism) {
            an_ifc_decl_index  member_idx = get_ifc_index(ism);
            this->cache_decl(&cache, member_idx, /*cinfo=*/{});
            cache_token_with_index(&cache, tok_ifc_decl, member_idx);
          };
          traverse_scope_members(class_members, cache_member);
          cache_token(&cache, tok_rbrace);
        }  /* if */
        cache_token(&cache, tok_semicolon);
      }
#if DEBUG
      if (db_flag_is_set("ms_ifc_token_def")) {
        fprintf(f_debug, "Reconstituted class definition: ");
        if (unmangled_name_of(&class_type->source_corresp) != NULL) {
          fprintf(f_debug, "%s",
                  unmangled_name_of(&class_type->source_corresp));
        }  /* if */
        fprintf(f_debug, "\n");
        db_tokens(&cache);
        fprintf(f_debug, "\n---------------------\n");
      }  /* if */
#endif /* DEBUG */
      if (cache.is_valid()) {
        a_boolean template_scope_pushed;

        tdip = alloc_template_decl_info();
        set_template_decl_info_for_class_definition(tdip, class_type);
        template_scope_pushed = push_template_instantiation_scope(
                                          tdip, class_type,
                                          (a_routine_ptr)NULL, class_sym,
                                          class_sym, (a_template_arg_ptr)NULL,
                                          /*push_lex_state=*/TRUE,
                                          PS_CLASS_DEFINITION_CONTEXT);
        /* Set the fixup depth for non-local classes to the context scope
           pushed above so that classes created by this routine will be fixed
           up by process_deferred_class_fixups_and_instantiations. */
        non_local_class_fixup_depth = depth_scope_stack;
        /* By default, the instantiation scope context pushed by the call to
           push_template_instantiation_scope just copies the name linkage from
           the previous entry on the scope stack, which may not be related to
           that of the class. */
        scope_stack_top().default_name_linkage =
                                       class_type->source_corresp.name_linkage;
        curr_class_fixup_header(/*for_instantiation=*/TRUE)->
                                                   pending_class_definitions++;
        {
          a_module_entity_rescan rescan(&cache);

          (void)scan_class_definition(
                                    class_type, (a_decl_parse_state*)NULL,
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
        add_ifc_friends_to_class(this, class_type, decl_idx);
        curr_class_fixup_header(/*for_instantiation=*/TRUE)->
                                                   pending_class_definitions--;
        process_deferred_class_fixups_and_instantiations(
                                                   /*for_instantiation=*/TRUE);
        non_local_class_fixup_depth = saved_non_local_class_fixup_depth;
        if (template_scope_pushed) {
          pop_template_instantiation_scope();
        }  /* if */
        free_template_decl_info(tdip);
        pop_module_declaration_context(scope_push_status);
      }  /* if */
      error_position = saved_error_position;
    }  /* if */
  }  /* if */
}  /* complete_definition_of_module_class */

#if DEBUG

void an_ifc_module::debug() const
/*
Print debug information for an IFC module
*/
{
  (void)fprintf(f_debug, ", kind: mk_ifc, version: %u.%u\n",
                (an_ifc_version_storage)get_ifc_major_version(header),
                (an_ifc_version_storage)get_ifc_minor_version(header));
}  /* debug */


void an_ifc_module::db_module_entity(a_module_entity_ptr mep) const
/*
Print debug information related to a module entity that refers to this module.
*/
{
  check_assertion(mep->module_info->module_interface == this);
  if (mep->variant.ifc_partition != ifc_pk_none) {
    const an_ifc_partition_metadata &part =
                            get_partition_metadata(mep->variant.ifc_partition);
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
    if (mep->invalid) {
      (void)fprintf(f_debug, "(Invalid)\n");
    } else {
      db_scp((a_source_correspondence*)mep->entity.ptr);
    }  /* if */
  } else {
    if (mep->scope != NULL) {
      db_scope(mep->scope);
    }  /* if */
    (void)fprintf(f_debug, "\n");
  }  /* if */
}  /* db_mep */


void db_mep_stack()
/*
Print information about the module entity stack.
*/
{
  a_module_entity_stack_state *state = curr_mep_state;

  while (state != NULL) {
    db_mep(state->mep);
    state = state->parent;
  }  /* while */
}  /* db_mep_stack */


void db_node_at_tsn(a_token_cache_ptr        cache,
                    a_token_sequence_number  tsn)
/*
Print diagnostic information about the node identified by a given token (if
any).
*/
{
  a_cached_token_ptr ctp = get_cache_token(cache, tsn);

  if (ctp->extra_info_kind == teik_ifc_index) {
    a_lexical_ifc_index_reference ifc_idx = ctp->variant.ifc_index;

    switch (ifc_idx.reference_kind) {
      case liik_decl_index:
        db_node_at_idx(from_lexical_index<an_ifc_decl_index>(ifc_idx));
        break;
      case liik_expr_index:
        db_node_at_idx(from_lexical_index<an_ifc_expr_index>(ifc_idx));
        break;
      default_is_unexpected();
    }  /* switch */
  }  /* if */
}  /* db_node_at_tsn */


void db_node_at_tsn(a_module_token_cache_ptr cache,
                    a_token_sequence_number  tsn)
/*
This function proxies calls to the common db_node_at_tsn function when using a
module token cache pointer.
*/
{
  db_node_at_tsn(cache->as_canonical(), tsn);
}  /* db_node_at_tsn */


void an_ifc_module::db_locus(const an_ifc_source_location &locus)
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
  const an_ifc_partition_metadata *save_debug_partition = debug_partition;
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

a_boolean an_ifc_module::init_string_table_and_header(
                                           a_module_import_decl_ptr midp,
                                           a_boolean                issue_diag)
/*
Initialize a module header and string table for the given module import decl.
If the initialization succeeds without error the value of issue_diag is
ignored, and TRUE is returned.  Otherwise, if the initialization fails return
FALSE, and issue diagnostics if issue_diag is TRUE.
*/
{
  a_boolean result = TRUE;

  /* Read the IFC file header (which starts after the magic number). */
  init_byte_buffer(this, 4, this->f_size - 4);
  this->header = construct_node_from_module<an_ifc_file_header>(this);
  /* Set the version information.  This is presumed to be version stable, and
     is critical for all following calls. */
  check_assertion_str(has_ifc_major_version(this->header),
                      "The default value of version_major needs updated.");
  check_assertion_str(has_ifc_minor_version(this->header),
                      "The default value of version_minor needs updated.");
  /* FIXME: We should likely check the version here, but for now assume all
     headers are the same.  Until the version is initialized in
     initialize_members_from_ifc_module_file this module will be treated as
     the current minimum supported version. */
  if (!validate(this->header, /*parent=*/NULL)) {
    result = FALSE;
    goto done;
  }
  /* FIXME: The checksum is not yet checked. */
  {
    an_ifc_byte_offset string_table_bytes = get_ifc_string_table_bytes(
                                                                 this->header);
#if USE_MMAP_FOR_MEMORY_REGIONS
    this->string_table = (char*)this->mmap_addr + string_table_bytes;
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */

    an_ifc_cardinality string_table_size = get_ifc_string_table_size(
                                                                 this->header);

    this->string_table = alloc_il(get_ifc_string_table_size(header));
    fseek(this->f_module, string_table_bytes, SEEK_SET);

    size_t bytes_read = fread((void*)this->string_table, 1, string_table_size,
                              this->f_module);
    if (bytes_read != string_table_size) {
      unexpected_condition_str("Failed to load the IFC module string table");
    }  /* if */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  }
done:
  return result;
}  /* init_string_table_and_header */

namespace {

/*
An internal representation of an IFC partition name used to facilitate binary
search of the partition map.
*/
struct an_ifc_partition_name {
  a_boolean operator<(const an_ifc_partition_name& other) const
    { return strcmp(this->name, other.name) < 0; }

  a_boolean operator==(const an_ifc_partition_name& other) const
    { return strcmp(this->name, other.name) == 0; }

  a_const_char *name;
};  /* an_ifc_partition_name */

}  /* namespace */

static an_ifc_partition_kind find_ifc_partition(a_const_char *name)
/*
Find and return the IFC partition kind for the partition matching name.  If
the partition could not be found, return ifc_pk_none.
*/
{
  an_ifc_partition_kind result = ifc_pk_none;
  /* Create a wrapped version of partition_name for comparisons. */
  an_ifc_partition_name partition_name{name};
  /* Provide a value function for retrieving the wrapped partition name at the
     given partition map index. */
  auto value_lambda = [](ptrdiff_t idx) {
    return an_ifc_partition_name{ifc_partition_map[idx].name};
  };
  /* Get the partition map index (if any) for the given partition name. */
  ptrdiff_t partition_map_idx = bin_search(IFC_PARTITION_COUNT, partition_name,
                                           value_lambda);

  /* If we have a matching partition map entry, return it, otherwise return
     null. */
  if (partition_map_idx != -1) {
    result = ifc_partition_map[partition_map_idx].kind;
  }  /* if */
  return result;
}  /* find_ifc_partition */


a_boolean an_ifc_module::initialize_members_from_ifc_module_file(
                                           a_module_import_decl_ptr midp,
                                           a_boolean                issue_diag)
/*
Initialize a module for the given module import decl.  If the initialization
succeeds without error the value of issue_diag is ignored, and TRUE is
returned.  Otherwise, if the initialization fails return FALSE, and issue
diagnostics if issue_diag is TRUE.
*/
{
  a_module_ptr mod = midp->module_info;
  a_boolean    result = TRUE;

  assoc_module_info = mod;
  set_name(mod->name, is_header_unit(mod));
  if (!init_string_table_and_header(midp, issue_diag)) {
    result = FALSE;
    goto done;
  }  /* if */
  {
    /* FIXME: Initialize version information now, this should really be done
       as part of the header initialization, however, that currently results
       in multiple version mismatch diagnostics. */
    an_ifc_version version_major_val = get_ifc_major_version(this->header);
    an_ifc_version version_minor_val = get_ifc_minor_version(this->header);

    if (!is_supported_ifc_version(version_major_val, version_minor_val)) {
      an_error_severity sev;

      /* FIXME: Does skipping the module version check still make sense?
         Perhaps this should require being forced to a specific version? */
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
    this->version_major = version_major_val;
    this->version_minor = version_minor_val;
  }
#if DEBUG
  if (db_flag_is_set("ifc_modules")) {
    db_module(mod);
  }  /* if */
#endif /* DEBUG */
  {
    an_ifc_byte_offset toc = get_ifc_toc(header);
    an_ifc_cardinality partition_count = get_ifc_partition_count(header);

    for (an_ifc_cardinality_storage i = 0; i < partition_count; ++i) {
      /* Manually initialize the byte buffer, then read the partition. */
      static_assert(sizeof(an_ifc_partition_storage) == 16,
                    "Partition storage is larger than expected");
      init_byte_buffer(this, toc + (16 * i), f_size - (size_t)toc);
      an_ifc_partition ip = construct_node_from_module<an_ifc_partition>(this);

      /* If the partition fails to validate, don't load it. */
      if (!validate(ip, /*parent=*/NULL)) {
        continue;
      }  /* if */

      /* Read information about the partition. */
      a_const_char          *name_str = get_string_at_offset(get_ifc_name(ip));
#if DEBUG
      if (db_flag_is_set("ifc_modules")) {
        (void)fprintf(
            f_debug,
            "partition %u \"%s\" offset 0x%08x cardinality %u entry_size %u\n",
            i, name_str, (an_ifc_byte_offset_storage)get_ifc_offset(ip),
            (an_ifc_cardinality_storage)get_ifc_cardinality(ip),
            (an_ifc_entity_size_storage)get_ifc_entry_size(ip));
      }  /* if */
#endif /* DEBUG */
      /* FIXME: Do we need this assertion? */
      check_assertion(get_ifc_cardinality(ip) != 0 && get_ifc_offset(ip) != 0);
      an_ifc_partition_kind part_kind = find_ifc_partition(name_str);
      if (part_kind == ifc_pk_none) {
        if (issue_diag) {
          str_remark(ec_unknown_ifc_partition, name_str);
        }  /* if */
      } else {
        an_ifc_partition_metadata  *pp;
        an_ifc_cardinality         cardinality = get_ifc_cardinality(ip);
        an_ifc_entity_size         entry_size = get_ifc_entry_size(ip);
        an_ifc_entity_size_storage expected_entry_size =
                               get_ifc_partition_element_size(this, part_kind);

        pp = &get_partition_metadata(part_kind);
        pp->name = name_str;
        pp->offset = get_ifc_offset(ip);
        pp->size = cardinality * entry_size;
        pp->entry_size = entry_size;
        pp->format_validated = alloc_validation_bit_array(cardinality);
        if (entry_size != expected_entry_size) {
          pos_st_num2_diagnostic(es_error, ec_ifc_partition_bad_entry_size,
                                 &midp->module_name_position,
                                 name_str, entry_size, expected_entry_size);
          /* Invalidate all elements of the partition; this stops processing of
             these partition elements without further diagnostics, and without
             ending further diagnostics. */
          invalidate_all_validation_bits(pp->format_validated, cardinality);
        }  /* if */
      }  /* if */
    }  /* for */
  }
  (void)fseek(f_module, 0L, SEEK_SET);
  if (get_partition_metadata(ifc_pk_name_source_file).name != NULL) {
    /* Allocate an array to map source locations to sequence numbers for each
       file referenced by the module.  No information about the sequence
       numbers is recorded yet (we do that only if the source file is later
       referenced). */
    an_ifc_partition_metadata *nsf_pp =
                              &get_partition_metadata(ifc_pk_name_source_file);
    check_assertion(nsf_pp->entry_size != 0);
    size_t num_files = nsf_pp->size / nsf_pp->entry_size;
    size_t size = num_files * sizeof(a_module_sequence_number_mapping);
    sequence_numbers = (a_module_sequence_number_mapping *)alloc_fe(size);
    memzero((char *)sequence_numbers, size);
    uint32_t num_src_lines = get_num_entries(ifc_pk_src_line);
    if (num_src_lines > 0) {
      /* As a (hopefully) temporary measure, for each file referenced in the
         module, we need to determine the largest line number that will be seen
         in that file (we don't actually need the last line number in the file,
         just the largest one that will be seen in the source location, though
         the last number would do).  This number will be used (if needed) to
         increment the source sequence when the module is referenced to
         effectively reserve those source sequence numbers for the file. */
      for (uint32_t idx = 0; idx < num_src_lines; idx++) {
        Opt<an_ifc_source_line>     opt_isl;
        an_ifc_partition_kind_index src_idx{this, ifc_pk_src_line, idx};

        construct_node(&opt_isl, src_idx);
        if (!opt_isl.has_value()) {
          result = FALSE;
          goto done;
        }  /* if */

        an_ifc_name_index  file = get_ifc_file(*opt_isl);
        size_t             file_index = file.value;
        an_ifc_line_number line_number = get_ifc_line(*opt_isl);
        check_assertion(file.sort == ifc_ns_name_source_file &&
                        file_index < num_files);
        if (line_number > sequence_numbers[file_index].max_line_number) {
          sequence_numbers[file_index].max_line_number = line_number;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  /* Set a flag so that later code can optimize on whether this module makes
     reference to any other modules. */
  this->references_any_modules = get_num_entries(ifc_pk_decl_reference) > 0;
#if EXPENSIVE_CHECKING
  if (eager_load_modules && this->references_any_modules) {
    using an_indexed_spec = an_Indexed<an_ifc_decl_specialization>;
    using an_indexed_partial_spec =
                                an_Indexed<an_ifc_decl_partial_specialization>;
    /* When eager loading mode is enabled, deferred specialization processing
       never occurs so if external modules are referenced, those
       specializations need to be processed now.  Note this code is only
       relevant for eager loading mode, which is a feature that is hidden
       behind EXPENSIVE_CHECKING (hence the surrounding preprocessor block). */
    a_decl_specialization_traverser spec_traverser(this, /*start=*/0);
    for (an_indexed_spec indexed_spec : spec_traverser) {
      if (indexed_spec.has_value()) {
        an_ifc_decl_index decl_idx = to_decl_index(indexed_spec.node_idx);

        associate_spec_with_template(decl_idx, *indexed_spec);
      }  /* if */
    }  /* for */

    a_decl_partial_specialization_traverser part_spec_traverser(this,
                                                                /*start=*/0);
    for (an_indexed_partial_spec indexed_spec : part_spec_traverser) {
      if (indexed_spec.has_value()) {
        an_ifc_decl_index decl_idx = to_decl_index(indexed_spec.node_idx);

        associate_spec_with_template(decl_idx, *indexed_spec);
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* EXPENSIVE_CHECKING */
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
    /* FIXME: Do we actually still need to do this? */
    static_assert(sizeof(an_ifc_file_header_storage) == 69,
                  "file header size needs to be dealt with per-version");
    if (!err && (size_t)stat_buf.st_size < (sizeof(magic) + 69)) {
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
         process is a little different on Windows environments.  Note that we
         do not map the file "read-only" because some strings from the string
         table are rewritten to become valid C names (e.g., "<unnamed-enum-x>"
         becomes "__noname_enum_x_"). */
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
                                           /*read_only=*/FALSE, (sizeof_t)0,
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


template<typename a_Scope_Member_Consumer>
inline void an_ifc_module::traverse_scope_member_sequence(
                                              const an_ifc_sequence   &seq,
                                              a_Scope_Member_Consumer consumer)
/*
Iterate over a given scope member sequence (seq) passing an_ifc_Scope_Member
pointer to the given consumer lambda for each element in the sequence.
*/
{
  an_ifc_index       start = get_ifc_start(seq);
  an_ifc_cardinality cardinality = get_ifc_cardinality(seq);

  traverse_scope_member_range(start, cardinality, consumer);
}  /* traverse_scope_member_sequence */


void an_ifc_module::process_ifc_scope(an_ifc_scope_index scope_index,
                                      a_scope_ptr        scope)
/*
Process the IFC scope specified by scope_index in the module file.  All items
in the IFC scope will be members of scope and their definitions will be
deferred until they are referenced.
*/
{
  auto cache_scope_member = [this, scope](const an_ifc_scope_member &ism) {
    an_ifc_decl_index   decl_idx = get_ifc_index(ism);
    a_module_entity_ptr dmep = get_ifc_module_entity_ptr(decl_idx);
    a_boolean           defer = TRUE;

#if EXPENSIVE_CHECKING
    /* When this flag is set, eagerly load all entities in a module.  Entities
       are typically lazily loaded (i.e., only when needed) and eagerly loading
       all entities in a module can be used as a debugging aid to ensure that
       all entities load properly.

       As this feature is not supported in production builds and this branch is
       in an anticipated hot path, conditionally enable it with
       EXPENSIVE_CHECKING as an optimization. */
    if (eager_load_modules && !in_get_home_scope) {
      defer = FALSE;
    }  /* if */
#endif /* EXPENSIVE_CHECKING */
    dmep->scope = scope;
    this->process_ifc_declaration(dmep, defer);
  };

  if (scope != 0) {
    a_module_scope_push_kind scope_push_status = mspk_unattempted;

    push_module_declaration_context(scope, &scope_push_status);
    traverse_scope_members(scope_index, cache_scope_member);
    pop_module_declaration_context(scope_push_status);
  }  /* if */
}  /* process_ifc_scope */


uint32_t an_ifc_module::get_num_entries(an_ifc_partition_kind partition) const
/*
Return the number of entries in a given partition.
*/
{
  uint32_t                        num_entries = 0;
  const an_ifc_partition_metadata &metadata =
                                             get_partition_metadata(partition);

  /* If there is an entry size defined, calculate the number of entries. */
  if (metadata.entry_size != 0) {
    num_entries = metadata.size / metadata.entry_size;
  }  /* if */
  return num_entries;
}  /* get_num_entries */


a_boolean an_ifc_module::is_home_scope_readable(an_ifc_decl_index decl_index)
/*
Return TRUE if the home scope of the declaration (indexed by decl_index) is
loaded and may contain all or part of its associated inner declarations.
*/
{
  a_module_entity_ptr mep = get_ifc_module_entity_ptr(decl_index);
  a_scope_ptr home_scope = mep->scope;
  /* FIXME: There's currently some more work required to retroactively retrieve
     a home scope for enumerators in the context where this is called.  Avoid a
     crash. */
  if (home_scope == NULL && decl_index.sort != ifc_ds_decl_enumerator) {
    home_scope = get_home_scope(decl_index);
  }  /* if */
  return home_scope != NULL && !home_scope->is_placeholder_scope;
}  /* is_home_scope_readable */


static an_ifc_decl_index get_declaring_class_scope_decl(
                                                    an_ifc_decl_index decl_ref)
/*
Given a declaration that's a class member, return the scope the declaring class
is a member of.  As an example, if the given declaration is a method of "class
A" in "namespace B", this function returns the IFC declaration index of
"namespace B".
*/
{
  an_ifc_decl_index declaring_class = get_ifc_home_scope(decl_ref);

  return get_ifc_home_scope(declaring_class);
}  /* get_declaring_class_scope_decl */


a_boolean an_ifc_module::is_name_qualifiable(an_ifc_decl_index decl_index)
/*
Given a declaration's index, return true if the declaration name
can currently be qualified.
*/
{
  a_boolean result = TRUE;

  switch (decl_index.sort) {
    case ifc_ds_decl_parameter:
      /* This declaration can never have its name qualified. */
      result = FALSE;
      break;
    case ifc_ds_decl_enumerator:
    case ifc_ds_decl_enumeration:
      {
        /* FIXME: This is a hack to work around crashing when an enumerator's
           home scope isn't loaded. */
        a_module_entity_ptr mep = get_ifc_module_entity_ptr(decl_index);
        result = mep->scope != NULL;
      }
      break;
    case ifc_ds_decl_scope:
      {
        /* Nested scopes should never be qualified. */
        result = !is_class_scope(get_ifc_home_scope(decl_index));
      }
      break;
    case ifc_ds_decl_method:
    case ifc_ds_decl_constructor:
    case ifc_ds_decl_destructor:
    case ifc_ds_decl_field:
    case ifc_ds_decl_bitfield:
    case ifc_ds_decl_property:
      { /* These entities can only exist in a class. Return FALSE if that class
           is a local class, otherwise return TRUE. */
        an_ifc_decl_index declaring_class_scope =
                                    get_declaring_class_scope_decl(decl_index);

        result = declaring_class_scope.sort == ifc_ds_decl_scope;
      }
      break;
    case ifc_ds_decl_reference:
      { Opt<an_ifc_decl_reference> opt_decl_ref;

        construct_node(&opt_decl_ref, decl_index);
        if (!opt_decl_ref.has_value()) {
          goto invalid;
        }  /* if */

        /* References are a special case where we need to recurse to a foreign
           module. */
        an_ifc_decl_reference decl_ref = *opt_decl_ref;
        an_ifc_decl_index     remote_index = get_ifc_index(decl_ref);
        an_ifc_module         *remote_mod = remote_index.mod;
        result = remote_mod->is_name_qualifiable(remote_index);
      }
      break;
    case ifc_ds_decl_variable:
      {
        an_ifc_decl_index home_scope = get_ifc_home_scope(decl_index);
        if (home_scope.sort == ifc_ds_decl_vendor_extension) {
          /* Local variables appear to be associated with a "vendor extension"
             home scope.  Such variables cannot be qualified. */
          result = FALSE;
        } else {
          result = TRUE;
        }  /* if */
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
  goto done;
invalid:
  result = FALSE;
done:;
  return result;
}  /* is_name_qualifiable */


static a_calling_convention conv_calling_convention(
                                     an_ifc_calling_convention_sort convention)
/*
Convert the given IFC calling convention to the corresponding EDG one.
*/
{
  a_calling_convention conv;

  switch (convention) {
    case ifc_ccs_cdecl:   conv = cc_cdecl; break;
    case ifc_ccs_fast:    conv = cc_fastcall; break;
    case ifc_ccs_std:     conv = cc_stdcall; break;
    case ifc_ccs_this:    conv = cc_thiscall; break;
    case ifc_ccs_clr:     conv = cc_clrcall; break;
    case ifc_ccs_vector:  conv = cc_vectorcall; break;
    case ifc_ccs_eabi:    /* conv = cc_eabicall; break; */
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_calling_conv,
                        &error_position, "CallingConvention::Eabi");
      conv = cc_default;
      break;
    default_is_unexpected_str("Unexpected CallingConvention");
  }  /* switch */
  return conv;
}  /* conv_calling_convention */


an_exception_specification_ptr an_ifc_module::exception_specification(
                                         an_ifc_noexcept_specification eh_spec,
                                         a_source_position             *pos)
/*
Convert the given IFC exception specification to the corresponding EDG one.
pos is the position of the exception specification if available, or the
position of the function declaration if not.
*/
{
  an_exception_specification_ptr result = NULL;
  an_ifc_noexcept_sort           sort = get_ifc_sort(eh_spec);

  if (!exceptions_enabled || sort == ifc_ns_none) {
    goto done;
  }  /* if */
  result = alloc_exception_specification();
  result->is_noexcept = TRUE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  result->source_range.start = *pos;
  result->source_range.end = *pos;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  switch (sort) {
    case ifc_ns_true:
      check_assertion(result->throw_any == FALSE);
      break;
    case ifc_ns_false:
      result->throw_any = TRUE;
      break;
    case ifc_ns_expression:
      { a_module_token_cache cache;
        result->indeterminate = TRUE;
        cache_sentence(&cache, get_ifc_words(eh_spec));
        if (!cache.is_valid()) {
          /* FIXME: Should we issue a diagnostic here? */
          result->variant.noexcept_arg = alloc_error_constant();
        } else {
          a_token_cache *base_cache = cache.as_canonical();
          result->arg_cached = TRUE;
          result->variant.token_cache = alloc_token_cache();
          clear_token_cache(result->variant.token_cache, /*reusable=*/TRUE);
          copy_tokens_from_cache(
                                base_cache,
                                base_cache->first_token->token_sequence_number,
                                base_cache->last_token->token_sequence_number,
                                /*include_last_token=*/TRUE,
                                result->variant.token_cache);
        }  /* if */
      }
      break;
    /* FIXME: It is unclear what these sorts mean - leave as unsupported for
       now. */
    case ifc_ns_inferred:
      issue_unsupported_construct_error(this, "NoexceptSort::Inferred", pos);
      break;
    case ifc_ns_unenforced:
      issue_unsupported_construct_error(this, "NoexceptSort::Unenforced", pos);
      break;
    case ifc_ns_none:
      /* This shouldn't be reachable, but is included here to prevent compiler
         warnings for unhandled enumerations. */
      unexpected_condition();
      break;
    default_is_unexpected_str("Unexpected NoexceptSort");
  }  /* switch */
done:
  return result;
}  /* exception_specification */

namespace {

a_param_type_ptr make_param_type_from_ifc(
                                      a_routine_type_supplement_ptr rtsp,
                                      an_ifc_type_index             param_type)
/*
Given the associated routine type supplement and IFC parameter type node index,
construct and return the corresponding parameter.  If the IFC specifies that
this is an index parameter, update the type supplement and instead return NULL.
*/
{
  a_param_type_ptr result = NULL;

  if (type_represents_ellipsis(param_type)) {
    /* This happens when an ellipsis is present as the last
       parameter. */
    rtsp->has_ellipsis = TRUE;
  } else {
    a_type_ptr il_type = type_for_type_index(param_type, /*kind=*/NULL);

    result = make_param_type(il_type, &null_source_position);
    update_param_top_level_qualifiers(result);
  }  /* if */
  return result;
}  /* make_param_type_from_ifc */


a_type_ptr type_for_type_index(an_ifc_type_index type_index,
                               a_non_type_kind   *kind)
/*
Return the type that corresponds to the specified TypeIndex.  If there is no
corresponding type, set *kind to the appropriate non-type kind and return an
error type.
*/
{
  a_type_ptr          result = NULL;
  a_module_entity_ptr mep = get_ifc_module_entity_ptr(type_index);
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
    an_ifc_type_index type_idx = type_index_of(mep);
    an_ifc_module     *mod = type_index.mod;

    switch (type_idx.sort) {
      case ifc_ts_type_fundamental:
        { Opt<an_ifc_type_fundamental> opt_itf;

          construct_node(&opt_itf, type_idx);
          if (!opt_itf.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_fundamental    itf = *opt_itf;
          an_ifc_type_basis_sort     basis = get_ifc_basis(itf);
          an_ifc_type_sign_sort      sign = get_ifc_sign(itf);
          an_ifc_type_precision_sort precision = get_ifc_precision(itf);
          an_integer_kind            ik;
          /* Note: no check is made for nonsensical types (e.g., signed
             void). */
          switch (basis) {
            case ifc_tbs_void:
              check_assertion(precision == ifc_tps_default);
              result = void_type();
              break;
            case ifc_tbs_bool:
              check_assertion(precision == ifc_tps_default);
              result = bool_type();
              break;
            case ifc_tbs_char:
              switch (sign) {
                case ifc_tss_plain:
                  switch (precision) {
                    case ifc_tps_default:
                      result = integer_type((an_integer_kind)ik_char);
                      break;
                    case ifc_tps_bit8:
                      result = char8_t_type();
                      break;
                    case ifc_tps_bit16:
                      result = char16_t_type();
                      break;
                    case ifc_tps_bit32:
                      result = char32_t_type();
                      break;
                    default:
                      { a_string err_msg("Unexpected ", str_for(precision),
                                         " for ", str_for(basis));

                        ifc_unexpected(mod, err_msg);
                      }
                      goto invalid;
                  }  /* switch */
                  break;
                case ifc_tss_signed:
                  check_assertion(precision == ifc_tps_default);
                  result = integer_type((an_integer_kind)ik_signed_char);
                  break;
                case ifc_tss_unsigned:
                  check_assertion(precision == ifc_tps_default);
                  result = integer_type((an_integer_kind)ik_unsigned_char);
                  break;
                default_is_unexpected();
              }  /* if */
              break;
            case ifc_tbs_wchar_t:
              switch (precision) {
                case ifc_tps_default:
                  result = wchar_t_type();
                  break;
                case ifc_tps_bit8:
                  result = char8_t_type();
                  break;
                case ifc_tps_bit16:
                  result = char16_t_type();
                  break;
                case ifc_tps_bit32:
                  result = char32_t_type();
                  break;
                case ifc_tps_short:
                case ifc_tps_long:
                case ifc_tps_bit64:
                case ifc_tps_bit128:
                  { a_string err_msg("Unexpected ", str_for(precision),
                                     " for ", str_for(basis));

                    ifc_unexpected(mod, err_msg);
                  }
                  goto invalid;
                default_is_unexpected();
              }  /* switch */
              break;
            case ifc_tbs_int:
              switch (precision) {
                case ifc_tps_default:
                  ik = (sign == ifc_tss_unsigned) ?
                                  ik_unsigned_int : ik_int;
                  break;
                case ifc_tps_short:
                  ik = (sign == ifc_tss_unsigned) ?
                                ik_unsigned_short : ik_short;
                  break;
                case ifc_tps_long:
                  ik = (sign == ifc_tss_unsigned) ?
                                 ik_unsigned_long : ik_long;
                  break;
                case ifc_tps_bit8:
                  ik = int_kind_for_bit_size(8, sign != ifc_tss_unsigned);
                  break;
                case ifc_tps_bit16:
                  ik = int_kind_for_bit_size(16, sign != ifc_tss_unsigned);
                  break;
                case ifc_tps_bit32:
                  ik = int_kind_for_bit_size(32, sign != ifc_tss_unsigned);
                  break;
                case ifc_tps_bit64:
                  ik = int_kind_for_bit_size(64, sign != ifc_tss_unsigned);
                  break;
                case ifc_tps_bit128:
                  ik = int_kind_for_bit_size(128, sign != ifc_tss_unsigned);
                  break;
                default_is_unexpected();
              }  /* switch */
              check_assertion(ik != (an_integer_kind)ik_none);
              result = integer_type(ik);
              break;
            case ifc_tbs_float:
              check_assertion(precision == ifc_tps_default);
              result = float_type((a_float_kind)fk_float);
              break;
            case ifc_tbs_double:
              if (precision == ifc_tps_long) {
                result = float_type((a_float_kind)fk_long_double);
              } else {
                check_assertion(precision == ifc_tps_default);
                result = float_type((a_float_kind)fk_double);
              }  /* if */
              break;
            case ifc_tbs_nullptr:
              check_assertion(precision == ifc_tps_default);
              result = standard_nullptr_type();
              break;
            case ifc_tbs_ellipsis:
            case ifc_tbs_class:
            case ifc_tbs_struct:
            case ifc_tbs_union:
            case ifc_tbs_auto:
            case ifc_tbs_decltype_auto:
            case ifc_tbs_namespace:
            case ifc_tbs_interface:
            case ifc_tbs_enum:
            case ifc_tbs_typename:
            case ifc_tbs_segment_type:
            case ifc_tbs_function:
            case ifc_tbs_empty:
            case ifc_tbs_variable_template:
            case ifc_tbs_concept:
            case ifc_tbs_overload:
              { a_string err_msg("Unexpected ", str_for(basis));

                ifc_unexpected(mod, err_msg);
              }
              goto invalid;
            default_is_unexpected_str("Unexpected TypeBasis kind");
          }  /* switch */
        }
        break;
      case ifc_ts_type_qualified:
        { Opt<an_ifc_type_qualified> opt_itq;

          construct_node(&opt_itq, type_idx);
          if (!opt_itq.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_qualified     itq = *opt_itq;
          an_ifc_type_index         unqualified = get_ifc_unqualified(itq);
          a_type_ptr                unqualified_ptr =
                               type_for_type_index(unqualified, /*kind=*/NULL);
          an_ifc_qualifier_bitfield ifc_qualifiers = get_ifc_qualifiers(itq);
          a_type_qualifier_set      qualifiers = TQ_NONE;
          if (test_bitmask<ifc_qb_const>(ifc_qualifiers)) {
            qualifiers |= TQ_CONST;
          }  /* if */
          if (test_bitmask<ifc_qb_volatile>(ifc_qualifiers)) {
            qualifiers |= TQ_VOLATILE;
          }  /* if */
          if (test_bitmask<ifc_qb_restrict>(ifc_qualifiers)) {
            qualifiers |= TQ_RESTRICT;
          }  /* if */
          result = make_qualified_type(unqualified_ptr, qualifiers);
        }
        break;
      case ifc_ts_type_pointer:
        { Opt<an_ifc_type_pointer> opt_itp;

          construct_node(&opt_itp, type_idx);
          if (!opt_itp.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_index pointee = get_ifc_pointee(*opt_itp);
          result = make_pointer_type(type_for_type_index(pointee,
                                                         /*kind=*/NULL));
        }
        break;
      case ifc_ts_type_lvalue_reference:
        { Opt<an_ifc_type_lvalue_reference> opt_itlr;

          construct_node(&opt_itlr, type_idx);
          if (!opt_itlr.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_index referee = get_ifc_referee(*opt_itlr);
          result = make_reference_type(type_for_type_index(referee,
                                                           /*kind=*/NULL));
        }
        break;
      case ifc_ts_type_rvalue_reference:
        { Opt<an_ifc_type_rvalue_reference> opt_itrr;

          construct_node(&opt_itrr, type_idx);
          if (!opt_itrr.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_index referee = get_ifc_referee(*opt_itrr);
          result = make_rvalue_reference_type(
                                           type_for_type_index(referee,
                                                               /*kind=*/NULL));
        }
        break;
      case ifc_ts_type_array:
        { Opt<an_ifc_type_array> opt_ita;

          construct_node(&opt_ita, type_idx);
          if (!opt_ita.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_array ita = *opt_ita;
          an_ifc_type_index element = get_ifc_element(ita);
          an_ifc_expr_index extent = get_ifc_extent(ita);
          result = alloc_type((a_type_kind)tk_array);
          result->variant.array.element_type =
                                            type_for_type_index(element,
                                                                /*kind=*/NULL);

          a_constant_ptr    elem_count = mod->constant_for_expr_index(
                                                        extent,
                                                        /*default_type=*/NULL);
          if (elem_count == NULL || elem_count->kind != ck_integer) {
            ifc_unexpected(mod, "bad element count for type array");
            goto invalid;
          }  /* if */

          a_boolean         err = FALSE;
          result->variant.array.variant.number_of_elements =
                          unsigned_value_of_integer_constant(elem_count, &err);
          if (err) {
            ifc_unexpected(mod, "integer overflow on type array");
            goto invalid;
          }  /* if */
          set_array_type_size(result, /*suppress_error=*/FALSE);
        }
        break;
      case ifc_ts_type_method:
        { Opt<an_ifc_type_method> opt_itm;

          construct_node(&opt_itm, type_idx);
          if (!opt_itm.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(mod, "TypeSort::Method",
                                            &error_position);
          goto invalid;
        }
      case ifc_ts_type_function:
        { Opt<an_ifc_type_function> opt_itf;

          construct_node(&opt_itf, type_idx);
          if (!opt_itf.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_function itf = *opt_itf;
          an_ifc_type_index    target = get_ifc_target(itf);
          an_ifc_type_index    source = get_ifc_source(itf);
          an_ifc_function_type_traits_bitfield
                               traits = get_ifc_traits(itf);
          a_routine_type_supplement_ptr
                               rtsp;
          /* Create a routine type with no parameters to start. */
          result = make_routine_type(type_for_type_index(target,
                                                         /*kind=*/NULL),
                                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                                     (a_type_ptr)NULL, (a_type_ptr)NULL);
          rtsp = rout_type_supp(result);
          rtsp->calling_convention =
                              conv_calling_convention(get_ifc_convention(itf));
          rtsp->exception_specification =
                             mod->exception_specification(get_ifc_eh_spec(itf),
                                                          &error_position);
          if (test_bitmask<ifc_fttb_const>(traits)) {
            rtsp->qualifiers |= TQ_CONST;
          }  /* if */
          if (test_bitmask<ifc_fttb_volatile>(traits)) {
            rtsp->qualifiers |= TQ_VOLATILE;
          }  /* if */
          if (test_bitmask<ifc_fttb_lvalue>(traits)) {
            rtsp->ref_qualifiers = rqk_lvalue;
          } else if (test_bitmask<ifc_fttb_rvalue>(traits)) {
            rtsp->ref_qualifiers = rqk_rvalue;
          }  /* if */
          if (!is_null_index(source)) {
            /* The function has parameters. */
            if (source.sort == ifc_ts_type_tuple) {
              /* A list of parameters. */
              Opt<an_ifc_type_tuple> opt_itt;

              construct_node(&opt_itt, source);
              if (!opt_itt.has_value()) {
                goto invalid;
              }  /* if */

              an_ifc_type_tuple     itt = *opt_itt;
              a_type_heap_traverser traverser(itt);
              a_param_type_ptr      *prev = &rtsp->param_type_list;
              for (an_Indexed<an_ifc_heap_type> indexed_iht : traverser) {
                if (!indexed_iht.has_value()) {
                  goto invalid;
                }  /* if */

                an_ifc_heap_type  iht = *indexed_iht;
                an_ifc_index_type idx = get_relative_index(traverser,
                                                           indexed_iht);
                an_ifc_type_index indexed_type = get_ifc_value(iht);
                a_param_type_ptr  ptp = make_param_type_from_ifc(rtsp,
                                                                 indexed_type);
                if (ptp == NULL && rtsp->has_ellipsis) {
                  ifc_requirement(indexed_type.mod,
                                  idx == get_ifc_cardinality(itt) - 1,
                                  "expected ellipsis to appear at "
                                  "end of parameter list");
                  break;
                }  /* if */

                ptp->param_num = idx + 1;
                *prev = ptp;
                prev = &ptp->next;
              }  /* for */
            } else {
              /* A single parameter. */
              rtsp->param_type_list = make_param_type_from_ifc(rtsp, source);
            }  /* if */
          }  /* if */
        }
        break;
      case ifc_ts_type_designated:
        /* A type's name (e.g., "A"). */
        { Opt<an_ifc_type_designated> opt_itd;

          construct_node(&opt_itd, type_idx);
          if (!opt_itd.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_designated itd = *opt_itd;
          an_ifc_decl_index      decl = get_ifc_decl(itd);
          switch (decl.sort) {
            case ifc_ds_decl_reference:
            case ifc_ds_decl_scope:
            case ifc_ds_decl_enumeration:
              /* Find the type of the scope declaration by processing it (in
                 case it has been deferred). */
              { a_module_entity_ptr dmep = process_decl_at_index(decl);

                if (dmep->invalid) {
                  goto invalid;
                }  /* if */
                if (dmep->entity.kind != iek_type) {
                  ifc_unexpected(get_assoc_ifc_module(dmep),
                                 "expected a type from TypeDesignated");
                  goto invalid;
                }  /* if */
                result = (a_type_ptr)dmep->entity.ptr;
              }
              break;
            case ifc_ds_decl_parameter:
              { Opt<an_ifc_decl_parameter> opt_decl_param;

                construct_node(&opt_decl_param, decl);
                if (!opt_decl_param.has_value()) {
                  goto invalid;
                }  /* if */

                an_ifc_decl_parameter decl_param = *opt_decl_param;
                an_ifc_type_index     type = get_ifc_type(decl_param);
                if (type_represents_templ_param_ref(type)) {
                  result = alloc_type((a_type_kind)tk_template_param);
                  result->variant.template_param.is_pack = FALSE;
                  result->variant.template_param.is_generic_param = FALSE;

                  a_template_param_type_supplement_ptr tptsp =
                                     result->variant.template_param.extra_info;
                  tptsp->coordinates.depth = get_ifc_level(decl_param);
                  tptsp->coordinates.position = get_ifc_position(decl_param);
                  set_type_size(result);
                } else {
                  result = type_for_type_index(get_ifc_type(decl_param),
                                               /*kind=*/NULL);
                }  /* if */
              }
              break;
            default:
              { a_string err_msg("Unexpected ", str_for(decl.sort),
                                 " for ", str_for(type_idx.sort));

                ifc_unexpected(mod, err_msg);
              }
              goto invalid;
          }  /* switch */
        }
        break;
      case ifc_ts_type_tor:
        /* This type should only be encountered when processing a constructor,
           and that is directly handled with that constructor declaration.*/
        { a_string err_msg("Unexpected ", str_for(type_idx.sort));

          ifc_unexpected(mod, err_msg);
        }
        goto invalid;
      case ifc_ts_type_placeholder:
        { Opt<an_ifc_type_placeholder> opt_itp;

          construct_node(&opt_itp, type_idx);
          if (!opt_itp.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_placeholder itp = *opt_itp;
          an_ifc_type_basis_sort  basis = get_ifc_basis(itp);
          switch (basis) {
            case ifc_tbs_auto:
            case ifc_tbs_decltype_auto:
              result = make_auto_type(&null_source_position,
                                      basis == ifc_tbs_decltype_auto);
              break;
            default:
              { a_string err_msg("Unexpected ", str_for(basis),
                                 " for ", str_for(type_idx.sort));

                ifc_unexpected(mod, err_msg);
              }
              goto invalid;
          }  /* switch */
        }
        break;
      case ifc_ts_type_pointer_to_member:
        { Opt<an_ifc_type_pointer_to_member> opt_itptm;

          construct_node(&opt_itptm, type_idx);
          if (!opt_itptm.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(mod, "TypeSort::PointerToMember",
                                            &error_position);
          goto invalid;
        }
      case ifc_ts_type_tuple:
        { Opt<an_ifc_type_tuple> opt_itt;

          construct_node(&opt_itt, type_idx);
          if (!opt_itt.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(mod, "TypeSort::Tuple",
                                            &error_position);
          goto invalid;
        }
      case ifc_ts_type_forall:
        { Opt<an_ifc_type_forall> opt_itf;

          construct_node(&opt_itf, type_idx);
          if (!opt_itf.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(mod, "TypeSort::Forall",
                                            &error_position);
          goto invalid;
        }
      case ifc_ts_type_syntactic:
        { Opt<an_ifc_type_syntactic> opt_its;

          construct_node(&opt_its, type_idx);
          if (!opt_its.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_expr_index expr = get_ifc_expr(*opt_its);
          switch (expr.sort) {
            case ifc_es_expr_template_id:
              { Opt<an_ifc_expr_template_id> opt_ieti;

                construct_node(&opt_ieti, expr);
                if (!opt_ieti.has_value()) {
                  goto invalid;
                }  /* if */
                result = mod->type_for_template_id(*opt_ieti);
              }
              break;
            default:
              { a_string err_msg("Unexpected ", str_for(expr.sort),
                                 " for ", str_for(type_idx.sort));

                ifc_unexpected(mod, err_msg);
              }
              goto invalid;
          }  /* switch */
        }
        break;
      case ifc_ts_type_expansion:
        { Opt<an_ifc_type_expansion> opt_ite;

          construct_node(&opt_ite, type_idx);
          if (!opt_ite.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(mod, "TypeSort::Expansion",
                                            &error_position);
          goto invalid;
        }
      case ifc_ts_type_typename:
        { Opt<an_ifc_type_typename> opt_itt;

          construct_node(&opt_itt, type_idx);
          if (!opt_itt.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(mod, "TypeSort::Typename",
                                            &error_position);
          goto invalid;
        }
      case ifc_ts_type_base:
        { Opt<an_ifc_type_base> opt_itb;

          construct_node(&opt_itb, type_idx);
          if (!opt_itb.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(mod, "TypeSort::Base",
                                            &error_position);
          goto invalid;
        }
      case ifc_ts_type_unaligned:
        { Opt<an_ifc_type_unaligned> opt_itu;

          construct_node(&opt_itu, type_idx);
          if (!opt_itu.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(mod, "TypeSort::Unaligned",
                                            &error_position);
          goto invalid;
        }
      case ifc_ts_type_decltype:
        { Opt<an_ifc_type_decltype> opt_itd;

          construct_node(&opt_itd, type_idx);
          if (!opt_itd.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(mod, "TypeSort::Decltype",
                                            &error_position);
          goto invalid;
        }
      case ifc_ts_type_syntax_tree:
        { Opt<an_ifc_type_syntax_tree> opt_itst;

          construct_node(&opt_itst, type_idx);
          if (!opt_itst.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(mod, "TypeSort::Syntaxtree",
                                            &error_position);
          goto invalid;
        }
      case ifc_ts_type_vendor_extension:
        issue_unsupported_construct_error(mod, str_for(type_idx.sort),
                                          &error_position);
        goto invalid;
      default_is_unexpected_str("Unexpected TypeSort");
    }  /* switch */
    /* Record this mapping for future reference. */
    if (result != NULL) {
      mep->scope = result->source_corresp.parent_scope;
    }  /* if */
    mep->entity.ptr = (char *)result;
    mep->entity.kind = iek_type;
  }  /* if */
  goto done;
invalid:
  result = error_type();
done:;
  check_assertion(result != NULL);
  return result;
}  /* type_for_type_index */


a_constant_ptr create_constant_for_nttp(const a_template_parameter *param,
                                        an_ifc_expr_index          expr_idx)
/*
Create and return a constant for the given non-type template parameter from the
given expression.
*/
{
  a_constant_ptr result = NULL;

  check_assertion(param->kind == tpk_nontype);
  switch (expr_idx.sort) {
    case ifc_es_expr_read:
      { Opt<an_ifc_expr_read> opt_ier;

        construct_node(&opt_ier, expr_idx);
        if (!opt_ier.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_read  ier = *opt_ier;
        an_ifc_expr_index address_idx = get_ifc_address(ier);
        result = create_constant_for_nttp(param, address_idx);
        /* FIXME: Update the constant with the appropriate read sort
           transformation applied (i.e., perform things like LvalueToRvalue
           conversion on the non-type constant). */
      }
      break;
    case ifc_es_expr_named_decl:
      { Opt<an_ifc_expr_named_decl> opt_named_decl;

        construct_node(&opt_named_decl, expr_idx);
        if (!opt_named_decl.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_named_decl named_decl = *opt_named_decl;
        an_ifc_decl_index      decl_idx = get_ifc_resolution(named_decl);
        switch (decl_idx.sort) {
          case ifc_ds_decl_parameter:
            { Opt<an_ifc_decl_parameter> opt_decl_param;

              construct_node(&opt_decl_param, decl_idx);
              if (!opt_decl_param.has_value()) {
                goto invalid;
              }  /* if */

              an_ifc_decl_parameter decl_param = *opt_decl_param;
              /* FIXME: Refactor this to not duplicate code from
                 make_nontype_template_param_symbol */
              result = fs_constant(ck_template_param);
              result->type = param->variant.nontype.constant->type;;
              result->variant.template_param.variant.coordinates.depth =
                                                     get_ifc_level(decl_param);
              result->variant.template_param.variant.coordinates.position =
                                                  get_ifc_position(decl_param);
            }
            break;
          default:
            /* Fallback. */
            /* FIXME: Do we need this? */
            { a_type_ptr    type = param->variant.nontype.constant->type;
              an_ifc_module *mod = expr_idx.mod;

              result = mod->constant_for_expr_index(expr_idx, type);
            }
            break;
        }  /* switch */
      }
      break;
    case ifc_es_expr_monad:
      { Opt<an_ifc_expr_monad> opt_iem;

        construct_node(&opt_iem, expr_idx);
        if (!opt_iem.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_monad           iem = *opt_iem;
        an_ifc_source_location      locus = get_ifc_locus(iem);
        a_module_token_cache        cache;
        an_ifc_source_position_hint pos_hint(&cache, locus);
        a_curr_token_preserver      guard;
        a_type_ptr                  type =
                                         type_for_type_index(get_ifc_type(iem),
                                                             /*kind=*/NULL);
        an_ifc_module               *mod = expr_idx.mod;

        mod->cache_operator(&cache, get_ifc_assoc(iem));
        cache_token(&cache, tok_lparen);
        mod->cache_expr(&cache, get_ifc_argument(iem), /*cinfo=*/{});
        cache_token(&cache, tok_rparen);
        if (cache.is_valid()) {
          a_module_entity_rescan rescan(&cache);

          result = fs_constant((a_constant_repr_kind)ck_error);
          scan_template_argument_constant_expression(type, result);
        }  /* if */
      }
      break;
    case ifc_es_expr_literal:
      { a_type_ptr    type = param->variant.nontype.constant->type;
        an_ifc_module *mod = expr_idx.mod;

        result = mod->constant_for_expr_index(expr_idx, type);
      }
      break;
    default:
      { a_string err_msg(str_for(expr_idx.sort),
                         " is not expected for non-type template argument"
                         " formation");
        ifc_unexpected(expr_idx.mod, err_msg);
      }
      goto invalid;
  }  /* switch */
  goto done;
invalid:
  result = alloc_error_constant();
done:
  return result;
}  /* create_constant_for_nttp */

}  /* namespace */

a_template_arg_ptr an_ifc_module::template_arg_for_expr(
                                           const a_template_parameter *param,
                                           an_ifc_expr_index          expr_idx)
/*
Given an IFC expression index, construct and return a corresponding template
argument for param.  IFC expressions that can contain more than one argument
(e.g., ExprSort::Tuple, ExprSort::PackedTemplateArguments) should use
template_args_for_expr_list instead.
*/
{
  a_template_arg_ptr result = NULL;

  switch (expr_idx.sort) {
    case ifc_es_expr_type:
      { Opt<an_ifc_expr_type> opt_expr_type;

        construct_node(&opt_expr_type, expr_idx);
        if (!opt_expr_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_type  expr_type = *opt_expr_type;
        an_ifc_type_index denotation = get_ifc_denotation(expr_type);

        a_non_type_kind arg_kind;
        /* FIXME: This should be refactored so this detection isn't
           necessary/possible and/or the following check should be moved to the
           validator. */
        a_type_ptr type = type_for_type_index(denotation, &arg_kind);
        if (arg_kind != ntk_none) {
          a_string err_msg(str_for(denotation.sort),
                           " referenced something that wasn't a type");

          ifc_unexpected(this, err_msg);
          goto invalid;
        }  /* if */
        if (is_error_type(type)) {
          goto invalid;
        }  /* if */
        result = alloc_template_arg(tak_type);
        result->variant.type = type;
      }
      break;
    case ifc_es_expr_unary_fold:
      { Opt<an_ifc_expr_unary_fold> opt_ieuf;

        construct_node(&opt_ieuf, expr_idx);
        if (!opt_ieuf.has_value()) {
          goto invalid;
        }  /* if */
        /* FIXME: Currently unsupported. */
        issue_unsupported_construct_error(this, "ExprSort::UnaryFold",
                                          &error_position);
      }
      goto invalid;
    /* Non-type template parameters. */
    default:
      result = alloc_template_arg(tak_nontype);
      result->variant.constant = create_constant_for_nttp(param, expr_idx);
      break;
  } /* switch */
  if (param->is_pack) {
    result->is_pack_element = TRUE;
  }  /* if */
  goto done;
invalid:
  result = NULL;
done:
  return result;
}  /* template_arg_for_expr */


a_template_arg_ptr an_ifc_module::template_args_for_expr_list(
                                        const a_template_parameter *param_list,
                                        an_ifc_expr_index          arguments)
/*
Given an IFC expression list (that may or may not cover multiple template
parameters), construct and return corresponding template arguments for
param_list.
*/
{
  a_template_arg_ptr result = NULL;

  if (!is_null_index(arguments)) {
    check_assertion(param_list != NULL);
    if (arguments.sort == ifc_es_expr_tuple) {
      Opt<an_ifc_expr_tuple> opt_iet;
      a_template_arg_ptr     *next_arg = &result;

      construct_node(&opt_iet, arguments);
      if (!opt_iet.has_value()) {
        goto invalid;
      }  /* if */

      an_ifc_expr_tuple      iet = *opt_iet;
      an_expr_heap_traverser traverser(iet);
      for (an_Indexed<an_ifc_heap_expr> indexed_ihe : traverser) {
        if (!indexed_ihe.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_heap_expr  ihe = *indexed_ihe;
        an_ifc_expr_index expr_index = get_ifc_value(ihe);
        check_assertion(param_list != NULL);
        /* Packed template arguments shouldn't be part of a tuple.  If this is
           ever encountered, the code below should be changed to recursively
           call this function and iterate over *next_arg until it's NULL. */
        if (expr_index.sort == ifc_es_expr_packed_template_arguments) {
          ifc_unexpected(expr_index.mod,
                         "unexpected recursive pack template argument");
          goto invalid;
        }  /* if */

        a_template_arg_ptr arg = template_arg_for_expr(param_list, expr_index);
        if (arg == NULL) {
          goto invalid;
        }  /* if */
        *next_arg = arg;
        next_arg = &(*next_arg)->next;
        check_assertion(*next_arg == NULL);
        if (!param_list->is_pack) {
          param_list = param_list->next;
        }  /* if */
      }  /* for */
    } else if (arguments.sort == ifc_es_expr_packed_template_arguments) {
      Opt<an_ifc_expr_packed_template_arguments> opt_iepta;

      construct_node(&opt_iepta, arguments);
      if (!opt_iepta.has_value()) {
        goto invalid;
      }  /* if */
      check_assertion(param_list->is_pack);
      result = alloc_template_arg(tak_start_of_pack_expansion);

      an_ifc_expr_index pack_args = get_ifc_arguments(*opt_iepta);
      if (pack_args.sort != ifc_es_expr_empty) {
        a_template_arg_ptr sub_arg = template_args_for_expr_list(param_list,
                                                                 pack_args);

        if (sub_arg == NULL) {
          goto invalid;
        }  /* if */
        result->next = sub_arg;
      }  /* if */
    } else {
      result = template_arg_for_expr(param_list, arguments);
    }  /* if */
  }  /* if */
  goto done;
invalid:
  expect_error();
  result = NULL;
done:
  return result;
}  /* template_args_for_expr_list */


static a_template_ptr get_template_from_id_expr(
                                       const an_ifc_expr_template_id &templ_id)
/*
Given an IFC template id expression return the associated template ptr if
valid; otherwise, return NULL.
*/
{
  a_template_ptr              result = NULL;
  an_ifc_expr_index           primary = get_ifc_primary(templ_id);
  Opt<an_ifc_expr_named_decl> opt_iend;

  construct_node(&opt_iend, primary);
  if (opt_iend.has_value()) {
    an_ifc_expr_named_decl   iend = *opt_iend;
    an_ifc_source_location   locus = get_ifc_locus(templ_id);
    a_source_position        pos;

    source_position_from_locus(&pos, locus);

    an_ifc_decl_index   resolution = get_ifc_resolution(iend);
    a_module_entity_ptr mep = process_decl_at_index(resolution);
    if (mep->entity.kind == iek_template) {
      result = (a_template_ptr)mep->entity.ptr;
    }  /* if */
  }  /* if */
  return result;
}  /* get_template_from_args */


a_type_ptr an_ifc_module::type_for_template_id(
                                       const an_ifc_expr_template_id &templ_id)
/*
Returns the type that corresponds to the provided ExprSort::TemplateId in the
module file.
*/
{
  a_type_ptr     result;
  a_template_ptr templ = get_template_from_id_expr(templ_id);

  if (templ != NULL && templ->kind != templk_none) {
    an_ifc_expr_index  arguments = get_ifc_arguments(templ_id);
    a_template_arg_ptr arg_list =
                  template_args_for_expr_list(templ->template_decl->param_list,
                                              arguments);
    if (arg_list == NULL) {
      goto invalid;
    }  /* if */
    a_symbol_ptr       inst_sym;

    switch (templ->kind) {
      case templk_function:
      case templk_member_function:
        { an_ifc_source_location locus = get_ifc_locus(templ_id);
          a_source_position      pos;

          source_position_from_locus(&pos, locus);
          inst_sym = find_template_function(
                                           symbol_for(templ), &arg_list,
                                           /*explicit_arg_list_present=*/FALSE,
                                           &pos);
          result = inst_sym->variant.routine.ptr->type;
        }
        break;
      case templk_class:
      case templk_member_class:
      case templk_member_enum:
        inst_sym = find_template_class(symbol_for(templ), &arg_list,
                                       /*any_prototype_allowed=*/FALSE,
                                       /*specific_prototype_allowed=*/NULL,
                                       /*instantiation_nonreal=*/FALSE,
                                       /*do_not_create=*/FALSE,
                                       /*in_substitution=*/FALSE);
        result = inst_sym->variant.class_struct_union.type;
        break;
      case templk_variable:
      case templk_static_data_member:
        inst_sym = find_template_variable(symbol_for(templ), &arg_list,
                                          /*prototype_allowed=*/TRUE,
                                          /*is_use=*/FALSE, /*diagnose=*/TRUE);
        result = inst_sym->variant.variable.ptr->type;
        break;
      case templk_concept:
        ifc_requirement(templ_id.get_module(), arg_list == NULL,
                        "expected no arguments to be specified for concepts");
        result = templ->prototype_instantiation.constraint->type;
        break;
      case templk_template_template_param:
      case templk_none:
        unexpected_condition();
        break;
      default_is_unexpected();
    }  /* switch */
    goto done;
  }
invalid:
  result = error_type();
done:
  return result;
}  /* type_for_template_id */


a_boolean an_ifc_module::source_position_from_locus(
                                           a_source_position            *pos,
                                           const an_ifc_source_location &locus)
/*
Map the IFC locus source position information into the source position at pos.
Return TRUE if processing succeeded, otherwise return FALSE.
*/
{
  return EDG_PREFIX::source_position_from_locus(pos, locus);
}  /* source_position_from_locus */


static a_const_char *string_from_name_ref(an_ifc_name_index name_ref,
                                          a_symbol_locator  *loc)
/*
Return the string referenced by name_ref.  The returned string is guaranteed to
be in long-lived memory (either via pointing to IL, a constant, the memory
mapping if enabled, or in the worst case an allocated pointer).  If non-NULL,
fields (like is_operator_name) in *loc are updated accordingly.
*/
{
  return name_ref.mod->string_from_name_index(name_ref, loc);
}  /* string_from_name_ref */


a_const_char *an_ifc_module::string_from_name_index(
                                                  an_ifc_name_index name_index,
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
                                              an_ifc_name_index name_index,
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
  a_const_char *result = NULL, *prefix = NULL;
  a_boolean    requires_buffer;

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
  if (name_index.sort == ifc_ns_text_offset) {
    /* NameSort::Identifiers just refer to the string table. */
    an_ifc_text_offset text_offset{name_index.mod, name_index.value};

    result = get_string_at_offset(text_offset);
  } else {
    switch (name_index.sort) {
      case ifc_ns_name_source_file:
        { Opt<an_ifc_name_source_file> opt_insf;

          construct_node(&opt_insf, name_index);
          if (!opt_insf.has_value()) {
            goto invalid;
          }  /* if */
          result = get_string_at_offset(get_ifc_path(*opt_insf));
        }
        break;
      case ifc_ns_name_operator:
        { Opt<an_ifc_name_operator> opt_ino;

          construct_node(&opt_ino, name_index);
          if (!opt_ino.has_value()) {
            goto invalid;
          }  /* if */
          if (loc != NULL) {
            an_ifc_operator_category op = get_ifc_operator(*opt_ino);

            /* Initialize the locator with the proper operator name. */
            make_opname_locator(opname_from_operator(op), loc,
                                &null_source_position);
            result = loc->symbol_header->identifier;
          } else {
            prefix = "operator";
            requires_buffer = TRUE;
            result = get_string_at_offset(get_ifc_encoded(*opt_ino));
          }  /* if */
        }
        break;
      case ifc_ns_name_conversion:
        { Opt<an_ifc_name_conversion> opt_inc;

          construct_node(&opt_inc, name_index);
          if (!opt_inc.has_value()) {
            goto invalid;
          }  /* if */

          a_type_ptr target_type = type_for_type_index(
                                                      get_ifc_target(*opt_inc),
                                                      /*kind=*/NULL);
          prefix = "operator ";
          requires_buffer = TRUE;
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
      case ifc_ns_name_literal:
        { Opt<an_ifc_name_literal> opt_inl;

          construct_node(&opt_inl, name_index);
          if (!opt_inl.has_value()) {
            goto invalid;
          }  /* if */
          result = get_string_at_offset(get_ifc_encoded(*opt_inl));
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
      case ifc_ns_name_template:
        { Opt<an_ifc_name_template> opt_int;

          construct_node(&opt_int, name_index);
          if (!opt_int.has_value()) {
            goto invalid;
          }  /* if */
          prefix = "template ";
          requires_buffer = TRUE;
          result = string_from_name_index(get_ifc_name(*opt_int), loc);
        }
        break;
      case ifc_ns_name_specialization:
        { Opt<an_ifc_name_specialization> opt_ins;

          construct_node(&opt_ins, name_index);
          if (!opt_ins.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(this, "NameSort::Specialization",
                                            &error_position);
          result = "<error-name>";
          /* String literals have static duration, no buffer required. */
          requires_buffer = FALSE;
          goto invalid;
        }
      case ifc_ns_name_guide:
        { Opt<an_ifc_name_guide> opt_ing;

          construct_node(&opt_ing, name_index);
          if (!opt_ing.has_value()) {
            goto invalid;
          }  /* if */
          /* FIXME: Currently unsupported. */
          issue_unsupported_construct_error(this, "NameSort::Guide",
                                            &error_position);
          result = "<error-name>";
          /* String literals have static duration, no buffer required. */
          requires_buffer = FALSE;
          goto invalid;
        }
      case ifc_ns_text_offset:
        /* This should be handled by the condition enclosing the switch. */
        unexpected_condition();
        break;
      default_is_unexpected();
    }  /* switch */
    /* Ensure prefixes are processed with a buffer. */
    check_assertion(prefix == NULL || requires_buffer);
  }  /* if */
  goto done;
invalid:
  /* FIXME: This function's error handling could be significantly improved. */
  result = "";
done:
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


a_const_char *an_ifc_module::string_from_name_index(
                                                an_ifc_text_offset text_offset,
                                                a_symbol_locator   *loc)
/*
Return the string referenced by text_offset.  The returned string is guaranteed
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
  an_ifc_name_index name_index{text_offset.mod, ifc_ns_text_offset,
                               text_offset};

  return string_from_name_index(name_index, loc, &result_buffer);
}  /* string_from_name_index */


static a_const_char* name_from_decl(an_ifc_decl_index decl_ref)
/*
Given a decl reference, return the name associated with that declaration.
*/
{
  return decl_ref.mod->name_from_local_decl(decl_ref);
}  /* name_from_decl */


a_const_char* an_ifc_module::name_from_local_decl(an_ifc_decl_index decl_idx)
/*
Given a declaration, return the name associated with that declaration.
*/
{
  a_const_char           *result = NULL;
  Opt<an_ifc_decl_index> gmf_decl_scope;
  Opt<an_ifc_type_index> gmf_decl_type;

  switch (decl_idx.sort) {
    case ifc_ds_decl_vendor_extension:
    case ifc_ds_decl_explicit_instantiation:
    case ifc_ds_decl_explicit_specialization:
      issue_unsupported_construct_error(this, str_for(decl_idx.sort),
                                        &error_position);
      goto invalid;
    case ifc_ds_decl_enumerator:
      { Opt<an_ifc_decl_enumerator> opt_ide;

        construct_node(&opt_ide, decl_idx);
        if (!opt_ide.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_enumerator ide = *opt_ide;
        result = get_string_at_offset(get_ifc_name(ide));
        if (is_from_gmf(get_ifc_specifiers(ide))) {
          gmf_decl_type = get_ifc_type(ide);
        }  /* if */
      }
      break;
    case ifc_ds_decl_variable:
      { Opt<an_ifc_decl_variable> opt_idv;

        construct_node(&opt_idv, decl_idx);
        if (!opt_idv.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_variable idv = *opt_idv;
        result = string_from_name_index(get_ifc_name(idv), /*loc=*/NULL);
        if (is_from_gmf(get_ifc_specifiers(idv))) {
          gmf_decl_scope = get_ifc_home_scope(idv);
        }  /* if */
      }
      break;
    case ifc_ds_decl_parameter:
      { Opt<an_ifc_decl_parameter> opt_idp;

        construct_node(&opt_idp, decl_idx);
        if (!opt_idp.has_value()) {
          goto invalid;
        }  /* if */
        result = get_string_at_offset(get_ifc_name(*opt_idp));
      }
      break;
    case ifc_ds_decl_field:
      { Opt<an_ifc_decl_field> opt_idf;

        construct_node(&opt_idf, decl_idx);
        if (!opt_idf.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_field idf = *opt_idf;
        result = get_string_at_offset(get_ifc_name(idf));
        if (is_from_gmf(get_ifc_specifiers(idf))) {
          gmf_decl_scope = get_ifc_home_scope(idf);
        }  /* if */
      }
      break;
    case ifc_ds_decl_bitfield:
      { Opt<an_ifc_decl_bitfield> opt_idb;

        construct_node(&opt_idb, decl_idx);
        if (!opt_idb.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_bitfield idb = *opt_idb;
        result = get_string_at_offset(get_ifc_name(idb));
        if (is_from_gmf(get_ifc_specifiers(idb))) {
          gmf_decl_scope = get_ifc_home_scope(idb);
        }  /* if */
      }
      break;
    case ifc_ds_decl_scope:
      { Opt<an_ifc_decl_scope> opt_ids;

        construct_node(&opt_ids, decl_idx);
        if (!opt_ids.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_scope ids = *opt_ids;
        result = string_from_name_index(get_ifc_name(ids), /*loc=*/NULL);
        if (is_from_gmf(get_ifc_specifiers(ids))) {
          gmf_decl_scope = get_ifc_home_scope(ids);
        }  /* if */
      }
      break;
    case ifc_ds_decl_enumeration:
      { Opt<an_ifc_decl_enumeration> opt_ide;

        construct_node(&opt_ide, decl_idx);
        if (!opt_ide.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_enumeration ide = *opt_ide;
        result = get_string_at_offset(get_ifc_name(ide));
        if (is_from_gmf(get_ifc_specifiers(ide))) {
          gmf_decl_scope = get_ifc_home_scope(ide);
        }  /* if */
      }
      break;
    case ifc_ds_decl_alias:
      { Opt<an_ifc_decl_alias> opt_ida;

        construct_node(&opt_ida, decl_idx);
        if (!opt_ida.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_alias ida = *opt_ida;
        result = get_string_at_offset(get_ifc_name(ida));
        if (is_from_gmf(get_ifc_specifiers(ida))) {
          gmf_decl_scope = get_ifc_home_scope(ida);
        }  /* if */
      }
      break;
    case ifc_ds_decl_temploid:
      { a_string err_msg(str_for(decl_idx.sort), " does not have a name");

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_ds_decl_template:
      { Opt<an_ifc_decl_template> opt_idt;

        construct_node(&opt_idt, decl_idx);
        if (!opt_idt.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_template idt = *opt_idt;
        result = string_from_name_index(get_ifc_name(idt), /*loc=*/NULL);
        if (is_from_gmf(get_ifc_specifiers(idt))) {
          gmf_decl_scope = get_ifc_home_scope(idt);
        }  /* if */
      }
      break;
    case ifc_ds_decl_partial_specialization:
      { Opt<an_ifc_decl_partial_specialization> opt_idps;

        construct_node(&opt_idps, decl_idx);
        if (!opt_idps.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_partial_specialization idps = *opt_idps;
        result = string_from_name_index(get_ifc_name(idps), /*loc=*/NULL);
        if (is_from_gmf(get_ifc_specifiers(idps))) {
          gmf_decl_scope = get_ifc_home_scope(idps);
        }  /* if */
      }
      break;
    case ifc_ds_decl_specialization:
      { Opt<an_ifc_decl_specialization> opt_ids;

        construct_node(&opt_ids, decl_idx);
        if (!opt_ids.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_specialization ids = *opt_ids;
        result = string_from_name_ref(get_ifc_name(ids), /*loc=*/NULL);
      }
      break;
    case ifc_ds_decl_concept:
      { Opt<an_ifc_decl_concept> opt_idc;

        construct_node(&opt_idc, decl_idx);
        if (!opt_idc.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_concept idc = *opt_idc;
        result = get_string_at_offset(get_ifc_name(idc));
        if (is_from_gmf(get_ifc_specifiers(idc))) {
          gmf_decl_scope = get_ifc_home_scope(idc);
        }  /* if */
      }
      break;
    case ifc_ds_decl_function:
      { Opt<an_ifc_decl_function> opt_idf;

        construct_node(&opt_idf, decl_idx);
        if (!opt_idf.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_function idf = *opt_idf;
        result = string_from_name_index(get_ifc_name(idf), /*loc=*/NULL);
        if (is_from_gmf(get_ifc_specifiers(idf))) {
          gmf_decl_scope = get_ifc_home_scope(idf);
        }  /* if */
      }
      break;
    case ifc_ds_decl_method:
      { Opt<an_ifc_decl_method> opt_idm;

        construct_node(&opt_idm, decl_idx);
        if (!opt_idm.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_method idm = *opt_idm;
        result = string_from_name_index(get_ifc_name(idm), /*loc=*/NULL);
        if (is_from_gmf(get_ifc_specifiers(idm))) {
          gmf_decl_scope = get_ifc_home_scope(idm);
        }  /* if */
      }
      break;
    case ifc_ds_decl_constructor:
      { Opt<an_ifc_decl_constructor> opt_idc;

        construct_node(&opt_idc, decl_idx);
        if (!opt_idc.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_constructor idc = *opt_idc;
        an_ifc_decl_index       home_scope = get_ifc_home_scope(idc);
        result = name_from_decl(home_scope);
        if (is_from_gmf(get_ifc_specifiers(idc))) {
          gmf_decl_scope = home_scope;
        }  /* if */
      }
      break;
    case ifc_ds_decl_inherited_constructor:
      { Opt<an_ifc_decl_inherited_constructor> opt_idic;

        construct_node(&opt_idic, decl_idx);
        if (!opt_idic.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_inherited_constructor idic = *opt_idic;
        an_ifc_decl_index                 home_scope =
                                                      get_ifc_home_scope(idic);
        result = name_from_decl(home_scope);
        if (is_from_gmf(get_ifc_specifiers(idic))) {
          gmf_decl_scope = home_scope;
        }  /* if */
      }
      break;
    case ifc_ds_decl_destructor:
      { Opt<an_ifc_decl_destructor> opt_idd;

        construct_node(&opt_idd, decl_idx);
        if (!opt_idd.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_destructor idd = *opt_idd;
        an_ifc_decl_index      home_scope = get_ifc_home_scope(idd);
        result = name_from_decl(home_scope);
        if (is_from_gmf(get_ifc_specifiers(idd))) {
          gmf_decl_scope = home_scope;
        }  /* if */
      }
      break;
    case ifc_ds_decl_reference:
      { Opt<an_ifc_decl_reference> opt_decl_ref;

        construct_node(&opt_decl_ref, decl_idx);
        if (!opt_decl_ref.has_value()) {
          goto invalid;
        }  /* if */

        /* References are a special case where we need to recurse to a foreign
           module. */
        an_ifc_decl_reference decl_ref = *opt_decl_ref;
        an_ifc_decl_index     remote_index = get_ifc_index(decl_ref);
        result = name_from_decl(remote_index);
      }
      break;
    case ifc_ds_decl_using_declaration:
      { Opt<an_ifc_decl_using_declaration> opt_idud;

        construct_node(&opt_idud, decl_idx);
        if (!opt_idud.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_using_declaration idud = *opt_idud;
        result = get_string_at_offset(get_ifc_name(idud));
        if (is_from_gmf(get_ifc_specifiers(idud))) {
          gmf_decl_scope = get_ifc_home_scope(idud);
        }  /* if */
      }
      break;
    case ifc_ds_decl_friend:
      { Opt<an_ifc_decl_friend> opt_idf;

        construct_node(&opt_idf, decl_idx);
        if (!opt_idf.has_value()) {
          goto invalid;
        }  /* if */
        /* FIXME: Currently unsupported. */
        issue_unsupported_construct_error(this, "DeclSort::Friend",
                                          &error_position);
        goto invalid;
      }
    case ifc_ds_decl_expansion:
      { Opt<an_ifc_decl_expansion> opt_ide;

        construct_node(&opt_ide, decl_idx);
        if (!opt_ide.has_value()) {
          goto invalid;
        }  /* if */
        /* FIXME: Is this reachable? */
        result = name_from_local_decl(get_ifc_operand(*opt_ide));
      }
      break;
    case ifc_ds_decl_deduction_guide:
      { Opt<an_ifc_decl_deduction_guide> opt_ddg;

        construct_node(&opt_ddg, decl_idx);
        if (!opt_ddg.has_value()) {
          goto invalid;
        }  /* if */
        /* FIXME: Currently unsupported. */
        issue_unsupported_construct_error(this, "DeclSort::DeductionGuide",
                                          &error_position);
        goto invalid;
      }
    case ifc_ds_decl_tuple:
      /* An overload set. */
      { Opt<an_ifc_decl_tuple> opt_idt;

        construct_node(&opt_idt, decl_idx);
        if (!opt_idt.has_value()) {
          goto invalid;
        }  /* if */

        /* All references here should have the same name, so we just need to
           use the first. */
        an_ifc_decl_tuple           idt = *opt_idt;
        an_ifc_index                start = get_ifc_start(idt);
        Opt<an_ifc_heap_decl>       opt_ihd;
        an_ifc_partition_kind_index heap_decl_idx{start.mod, ifc_pk_heap_decl,
                                                  start};
        construct_node(&opt_ihd, heap_decl_idx);
        if (!opt_ihd.has_value()) {
          goto invalid;
        }  /* if */
        result = name_from_local_decl(get_ifc_value(*opt_ihd));
      }
      break;
    case ifc_ds_decl_intrinsic:
      { Opt<an_ifc_decl_intrinsic> opt_idi;

        construct_node(&opt_idi, decl_idx);
        if (!opt_idi.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_intrinsic idi = *opt_idi;
        result = get_string_at_offset(get_ifc_name(idi));
        if (is_from_gmf(get_ifc_specifiers(idi))) {
          gmf_decl_scope = get_ifc_home_scope(idi);
        }  /* if */
      }
      break;
    case ifc_ds_decl_property:
      { Opt<an_ifc_decl_property> opt_idp;

        construct_node(&opt_idp, decl_idx);
        if (!opt_idp.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_property idp = *opt_idp;
        result = name_from_local_decl(get_ifc_member(idp));
      }
      break;
    case ifc_ds_decl_output_segment:
      { Opt<an_ifc_decl_output_segment> opt_idos;

        construct_node(&opt_idos, decl_idx);
        if (!opt_idos.has_value()) {
          goto invalid;
        }  /* if */
        result = get_string_at_offset(get_ifc_name(*opt_idos));
      }
      break;
    case ifc_ds_decl_barren:
    case ifc_ds_decl_using_directive:
    case ifc_ds_decl_syntax_tree:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, str_for(decl_idx.sort),
                                        &error_position);
      goto invalid;
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
    if (gmf_decl && is_home_scope_readable(decl_idx)) {
      a_symbol_locator loc;

      /* FIXME: Do we want to have this check here, or should we always load
         the module's version of the declaration and rely on visibility rules
         to sort it out later?  Currently visibility is not well implemented
         and so the interplay here is not well understood. */
      if (find_symbol(result, strlen(result), &loc) == NULL) {
        /* Declarations that come from the global module fragment aren't added
           to the module scope, so we need to ensure that we've added these
           names to the lazily-loaded symbols list. */
        a_module_entity_ptr mep = get_ifc_module_entity_ptr(decl_idx);

        if (gmf_decl_scope.has_value()) {
          mep->scope = get_scope(*gmf_decl_scope);
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
invalid:
  return result;
}  /* name_from_local_decl */


void an_ifc_module::init_dps(a_decl_parse_state               *dps,
                             const an_ifc_source_location     &locus,
                             an_ifc_type_index                type_index,
                             an_ifc_object_traits_bitfield    traits,
                             an_ifc_msvc_traits_bitfield      msvc_traits,
                             an_ifc_basic_specifiers_bitfield specifiers,
                             an_ifc_access_sort               access,
                             an_ifc_expr_index                alignment,
                             a_partial_scope_stack_state      *psssp)
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
  psssp->saved = FALSE;
  source_position_from_locus(&dps->start_pos, locus);
  error_position = dps->start_pos;
  if (!is_null_index(type_index)) {
    dps->type = type_for_type_index(type_index, /*kind=*/NULL);
  }  /* if */
  if (!is_null_bitfield(traits)) {
    if (test_bitmask<ifc_otb_constexpr>(traits)) {
      dps->dso_flags |= DSO_CONSTEXPR;
    }  /* if */
    if (test_bitmask<ifc_otb_mutable>(traits)) {
      dps->dso_flags |= DSO_MUTABLE;
    }  /* if */
    if (test_bitmask<ifc_otb_thread_local>(traits)) {
      dps->dso_flags |= DSO_THREAD_LOCAL;
    }  /* if */
    if (test_bitmask<ifc_otb_inline>(traits)) {
      dps->dso_flags |= DSO_INLINE;
    }  /* if */
  }  /* if */
  if (!is_null_bitfield(msvc_traits)) {
    if (test_bitmask<ifc_mtb_comdat>(msvc_traits)) {
      unexpected_condition(); /* FIXME */
    }  /* if */
    if (test_bitmask<ifc_mtb_select_any>(msvc_traits)) {
      ap = make_module_attribute("selectany", af_ms_declspec, ap);
    }  /* if */
    if (test_bitmask<ifc_mtb_process>(msvc_traits)) {
      ap = make_module_attribute("process", af_ms_declspec, ap);
    }  /* if */
    if (test_bitmask<ifc_mtb_dll_export>(msvc_traits)) {
      ap = make_module_attribute("dllexport", af_ms_declspec, ap);
    }  /* if */
    if (test_bitmask<ifc_mtb_dll_import>(msvc_traits)) {
      ap = make_module_attribute("dllimport", af_ms_declspec, ap);
    }  /* if */
    if (test_bitmask<ifc_mtb_allocate>(msvc_traits)) {
      ap = make_module_attribute("allocate", af_ms_declspec, ap);
    }  /* if */
  }  /* if */
  if (!is_null_bitfield(specifiers)) {
    if (test_bitmask<ifc_bsb_c>(specifiers)) {
      /* Save the existing name linkage and use "C" linkage for the next
         declaration. */
      save_partial_scope_stack(psssp);
      scope_stack[decl_scope_level].name_linkage_is_explicit = TRUE;
      scope_stack[decl_scope_level].default_name_linkage =
                                             (a_name_linkage_kind)nlk_external;
      dps->decl_modifiers.direct_linkage_specifier = TRUE;
    }  /* if */
    if (test_bitmask<ifc_bsb_internal>(specifiers)) {
      dps->storage_class = sc_static;
    }  /* if */
    if (test_bitmask<ifc_bsb_vague>(specifiers)) {
      /* FIXME: unexpected_condition(); (for now) */
    }  /* if */
    if (test_bitmask<ifc_bsb_external>(specifiers)) {
      dps->storage_class = sc_extern;
    }  /* if */
    if (test_bitmask<ifc_bsb_deprecated>(specifiers)) {
      ap = make_module_attribute("deprecated", af_std, ap);
    }  /* if */
    if (test_bitmask<ifc_bsb_initialized_in_class>(specifiers)) {
      /* FIXME: Anything to do here? */
    }  /* if */
    if (test_bitmask<ifc_bsb_non_exported>(specifiers)) {
      /* FIXME: Anything to do here? */
    }  /* if */
  }  /* if */
  if (ap != NULL) {
    dps->prefix_attributes = ap;
  }  /* if */
  if (access != ifc_as_none) {
    an_access_specifier il_access;

    switch (access) {
      case ifc_as_private:   il_access = as_private;   break;
      case ifc_as_protected: il_access = as_protected; break;
      case ifc_as_public:    il_access = as_public;    break;
      case ifc_as_none:      unexpected_condition();   break;
      default_is_unexpected();
    }  /* switch */
    if (!psssp->saved) {
      /* Save some elements from the scope stack before temporarily changing
         some of them.  Don't do this if we already made such changes above,
         since that would promote the earlier temporary changes to be part of
         the "original" state. */
      save_partial_scope_stack(psssp);
    }  /* if */
    scope_stack[decl_scope_level].current_access = il_access;
  }  /* if */
  if (!is_null_index(alignment)) {
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


static a_boolean get_textual_name(an_ifc_text_offset text_offset,
                                  a_symbol_locator   *loc,
                                  a_const_char       **name_result)
/*
Store the textual name representation for the given textual offset and location
in the name result.  Return TRUE if this operation completed successfully,
FALSE otherwise.
*/
{
  *name_result = EDG_PREFIX::get_string_at_offset(text_offset);
  return TRUE;
}  /* get_textual_name */


static a_boolean get_textual_name(an_ifc_name_index name_ref,
                                  a_symbol_locator  *loc,
                                  a_const_char      **name_result)
/*
Store the textual name representation for the given name reference and location
in the name result.  Return TRUE if this operation completed successfully,
FALSE otherwise.
*/
{
  a_boolean result = FALSE;

  if (name_ref.sort == ifc_ns_text_offset) {
    /* Convert the name ref into a text offset. */
    an_ifc_text_offset text_offset{name_ref.mod, name_ref.value};

    result = get_textual_name(text_offset, loc, name_result);
  } else {
    *name_result = string_from_name_ref(name_ref, loc);
    result = TRUE;
  }  /* if */
  return result;
}  /* get_textual_name */


template<typename an_ifc_Index_type>
a_boolean an_ifc_module::init_locator_from_name(
                                           an_ifc_Index_type            ref,
                                           const an_ifc_source_location &locus,
                                           a_symbol_locator             *loc)
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


template<typename an_ifc_Index_type>
inline a_boolean an_ifc_module::init_decl_locator(
                                           an_ifc_Index_type            ref,
                                           const an_ifc_source_location &locus,
                                           a_symbol_locator             *loc)
/*
Initialize the locator specified by loc for the declaration named by ref and
positioned at locus.  Return TRUE if processing succeeded, otherwise return
FALSE.
*/
{
  a_boolean result = TRUE;

  if (!source_position_from_locus(&error_position, locus)) {
    result = FALSE;
  } else if (!init_locator_from_name(ref, locus, loc)) {
    result = FALSE;
  }  /* if */
  return result;
}  /* init_decl_locator */


template<typename an_ifc_Decl_type>
inline a_boolean an_ifc_module::init_decl_locator(const an_ifc_Decl_type &decl,
                                                  a_symbol_locator       *loc)
/*
Initialize the locator specified by loc for the declaration represented at decl
using a NameIndex derived declaration name.  Return TRUE if processing
succeeded, otherwise return FALSE.
*/
{
  a_boolean result = FALSE;

  if (has_ifc_name(decl) && has_ifc_locus(decl)) {
    /* name_idx can be typed an_ifc_text_offset or an_ifc_name_index, use auto
       to capture this flexibility (at least until we allow text_offset
       to automatically convert into name_index). */
    auto                   name_idx = get_ifc_name(decl);
    an_ifc_source_location locus = get_ifc_locus(decl);

    result = init_decl_locator(name_idx, locus, loc);
  }  /* if */
  return result;
}  /* init_decl_locator */


template<typename an_ifc_Decl_type>
inline a_boolean an_ifc_module::lazy_init_module_scope(
                                                const an_ifc_Decl_type   &decl,
                                                a_module_entity_ptr      mep)
/*
If the given module entity pointer's scope is not yet set, set the scope.
Return TRUE if a scope was set.
*/
{
  a_boolean scope_initialized = FALSE;

  if (mep->scope == NULL) {
    mep->scope = get_home_scope(decl);
    scope_initialized = TRUE;
  }  /* if */
  return scope_initialized;
} /* lazy_init_module_scope */


template<typename an_ifc_Decl_type>
inline void an_ifc_module::lazy_push_module_scope(
                                   const an_ifc_Decl_type   &decl,
                                   a_module_entity_ptr      mep,
                                   a_module_scope_push_kind *scope_push_status)
/*
If the given module entity pointer's scope is not yet set, set the scope and
push the module declaration context.  Update scope_push_status to
mspk_unattempted if no scope push was attempted, mspk_unnecessary if the
current scope is already the correct scope, or mspk_new if a new scope was
pushed.
*/
{
  a_boolean scope_initialized = lazy_init_module_scope(decl, mep);

  if (scope_initialized) {
    push_module_declaration_context(mep->scope, scope_push_status);
  }  /* if */
} /* lazy_init_module_scope */


void an_ifc_module::unsigned_integer_for_expr_index(
                                                  an_ifc_expr_index expr_index,
                                                  an_integer_value  *value)
/*
Returns in *value, the unsigned integer value represented by expr_index
(which must be either a LiteralSort::Immediate or LiteralSort::Integer).
No casting is performed.
*/
{
  Opt<an_ifc_expr_literal> opt_iel;

  /* Prepare to read from the proper partition for this expression. */
  construct_node(&opt_iel, expr_index);
  if (opt_iel.has_value()) {
    an_ifc_lit_index lit_index = get_ifc_value(*opt_iel);
    switch (lit_index.sort) {
      case ifc_ls_immediate:
        { /* An immediate literal (30 bits or less). */
          a_host_large_unsigned encoded =
                                        (a_host_large_unsigned)lit_index.value;

          set_unsigned_integer_value(value, encoded);
        }
        break;
      case ifc_ls_integer:
        { /* An integer larger than 30 bits. */
          Opt<an_ifc_const_i64>       opt_ici64;
          an_ifc_partition_kind_index int_idx{expr_index.mod, ifc_pk_const_i64,
                                              lit_index.value};

          construct_node(&opt_ici64, int_idx);
          if (!opt_ici64.has_value()) {
            goto invalid;
          }  /* if */

          char               raw_val[8];
          an_ifc_u64_storage raw_value = get_ifc_value(*opt_ici64);
          static_assert(sizeof(raw_val) == sizeof(raw_value),
                        "Generated storage doesn't match expected byte size.");
          static_assert(sizeof(raw_val) == sizeof(uint64_t),
                        "Expected byte size isn't 64 bits wide.");
          memcpy(&raw_val, &raw_value, 8);
          if (!conv_bytes_to_integer_value(value, raw_val, sizeof(raw_val))) {
            a_string err_msg("Failed to get a 64-bit integer from ",
                             str_for(lit_index.sort));

            ifc_unexpected(lit_index.mod, err_msg);
            goto invalid;
          }  /* if */
        }
        break;
      case ifc_ls_floating_point:
        { a_string err_msg("Unexpected ", str_for(lit_index.sort));

          ifc_unexpected(lit_index.mod, err_msg);
        }
        goto invalid;
      default_is_unexpected();
    }  /* switch */
  } else {
invalid:
    /* FIXME: Error handling could be improved here. */
    set_unsigned_integer_value(value, (a_host_large_unsigned)0);
  }  /* if */
}  /* unsigned_integer_for_expr_index */


a_constant_ptr an_ifc_module::constant_for_expr_index(
                                                an_ifc_expr_index expr_idx,
                                                a_type_ptr        default_type)
/*
Returns a constant (allocated in the current IL memory region) with the value
specified by expr_idx.  Assumes the expression is constant.  If the
expression's type is zero, use default_type as the expression's type.
FIXME: shared or unshared?
FIXME: what other expressions can we get here?
*/
{
  a_constant_ptr                   cp = NULL;
  an_ifc_module                    *mod = expr_idx.mod;
  Value_saver<a_module_entity_ptr> mep_saver(&curr_module_entity);

  curr_module_entity = NULL;
  /* FIXME: Can this entire thing be replaced via caching the expression and
     then calling scan_expr_or_braced_init_list, as is done for
     ifc_ExprSort_Tokens below? */
  switch (expr_idx.sort) {
    case ifc_es_expr_literal:
      { Opt<an_ifc_expr_literal> opt_iel;

        construct_node(&opt_iel, expr_idx);
        if (!opt_iel.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_literal   iel = *opt_iel;
        an_ifc_type_index     type = get_ifc_type(iel);

        an_ifc_lit_index lit_value = get_ifc_value(iel);
        switch (lit_value.sort) {
          case ifc_ls_immediate:
          case ifc_ls_integer:
            /* An integer. */
            { an_integer_value value;

              /* Retrieve the unsigned value of the integer. */
              unsigned_integer_for_expr_index(expr_idx, &value);
              cp = alloc_constant(ck_integer);
              if (is_null_index(type) && default_type == NULL) {
                /* FIXME: not sure why the type is zero in some cases. */
                set_unsigned_integer_constant(
                                        cp,
                                        (a_host_large_unsigned)lit_value.value,
                                        (an_integer_kind)ik_unsigned_int);
              } else {
                a_type_ptr constant_type = default_type;

                if (!is_null_index(type)) {
                  constant_type = type_for_type_index(type, /*kind=*/NULL);
                  if (is_error_type(constant_type)) {
                    goto invalid;
                  }  /* if */
                }  /* if */
                if (is_pointer_type(constant_type)) {
                  /* Pointer literal. */
                  set_unsigned_integer_constant(cp, value,
                                                targ_size_t_int_kind);
                } else if (is_nullptr_type(constant_type)) {
                  /* nullptr.  Make an integer zero and convert its type to
                     nullptr_t. */
                  a_boolean did_not_fold;
                  set_integer_constant(cp, (a_host_large_integer)0,
                                       (an_integer_kind)ik_int);
                  type_change_constant(cp, constant_type,
                                       /*is_implicit_cast=*/TRUE,
                                       /*maintain_expression=*/FALSE,
                                       &did_not_fold, &error_position);
                  if (did_not_fold) {
                    ifc_unexpected(this, "could not fold nullptr");
                  }  /* if */
                } else {
                  a_type_ptr stripped_type = skip_typerefs(constant_type);
                  if (type_is(stripped_type, tk_integer)) {
                    if (int_type_is_signed(stripped_type)) {
                      sign_extend_integer_value(&value,
                                 (int)(stripped_type->size * targ_char_bit));
                      set_integer_constant(cp, value,
                                    stripped_type->variant.integer.int_kind);
                    } else {
                      set_unsigned_integer_constant(cp, value,
                                    stripped_type->variant.integer.int_kind);
                    }  /* if */
                  } else {
                    ifc_unexpected(this, "expected an integer type");
                  }  /* if */
                }  /* if */
                cp->type = constant_type;
              }  /* if */
            }
            break;
          case ifc_ls_floating_point:
            { Opt<an_ifc_const_f64>       opt_icf;
              an_ifc_partition_kind_index float_index{lit_value.mod,
                                                      ifc_pk_const_f64,
                                                      lit_value.value};
              construct_node(&opt_icf, float_index);
              if (!opt_icf.has_value()) {
                goto invalid;
              }  /* if */

              /* FIXME: Find a better way to do the float conversion. */
              an_ifc_ieeele_float value = get_ifc_value(*opt_icf);
              double              float_value;
              static_assert(sizeof(an_ifc_ieeele_float_storage) ==
                                                                sizeof(double),
                            "Float storage bytes were not 64 bits wide.");
              memcpy(&float_value, value.get_storage(), sizeof(double));

              char      buf[32];
              a_boolean err = FALSE;
              sprintf(buf, "%f", float_value);
              cp = alloc_constant(ck_float);
              cp->type = float_type(fk_double);
              fp_string_to_float(fk_double, buf, &cp->variant.float_value,
                                 &err);
              check_assertion(!err);
            }
            break;
          default_is_unexpected();
        }  /* switch */
      }
      break;
    case ifc_es_expr_array_value:
      { Opt<an_ifc_expr_array_value> opt_ieav;

        construct_node(&opt_ieav, expr_idx);
        if (!opt_ieav.has_value()) {
          goto invalid;
        }  /* if */
        /* FIXME: Currently unsupported. */
        issue_unsupported_construct_error(this, "ExprSort::ArrayValue",
                                          &error_position);
        goto invalid;
      }
    case ifc_es_expr_product_type_value:
      { Opt<an_ifc_expr_product_type_value> opt_ieptv;

        construct_node(&opt_ieptv, expr_idx);
        if (!opt_ieptv.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_product_type_value ieptv = *opt_ieptv;
        an_ifc_type_index              type = get_ifc_type(ieptv);
        a_type_ptr                     tp = type_for_type_index(type,
                                                                /*kind=*/NULL);
        if (is_error_type(tp)) {
          goto invalid;
        }  /* if */
        complete_type_is_needed(tp);
        cp = alloc_constant(ck_aggregate);
#if DO_IL_LOWERING
        /* This will be changed later if there are any actual fields that get
           initialized. */
        cp->initializes_empty_object = TRUE;
#endif /* DO_IL_LOWERING */
        cp->type = tp;

        an_ifc_expr_index base_subobjects = get_ifc_base_subobjects(ieptv);
        if (!is_null_index(base_subobjects)) {
          a_constant_ptr sub_con = constant_for_expr_index(
                                                        base_subobjects,
                                                        /*default_type=*/NULL);

          add_constant_to_aggregate(sub_con, cp, NULL, NULL);
#if DO_IL_LOWERING
          if (!sub_con->initializes_empty_object) {
            cp->initializes_empty_object = FALSE;
          }  /* if */
#endif /* DO_IL_LOWERING */
        }  /* if */

        an_ifc_expr_index members = get_ifc_members(ieptv);
        if (!is_null_index(members)) {
          a_constant_ptr mem_con = constant_for_expr_index(
                                                        members,
                                                        /*default_type=*/NULL);

          if (mem_con == NULL) {
            goto invalid;
          }  /* if */
          add_constant_to_aggregate(mem_con, cp, NULL, NULL);
#if DO_IL_LOWERING
          if (!mem_con->initializes_empty_object) {
            cp->initializes_empty_object = FALSE;
          }  /* if */
#endif /* DO_IL_LOWERING */
        }  /* if */
      }
      break;
    case ifc_es_expr_subobject_value:
      { Opt<an_ifc_expr_subobject_value> opt_iesv;

        construct_node(&opt_iesv, expr_idx);
        if (!opt_iesv.has_value()) {
          goto invalid;
        }  /* if */
        cp = constant_for_expr_index(get_ifc_value(*opt_iesv), default_type);
      }
      break;
    case ifc_es_expr_named_decl:
      { Opt<an_ifc_expr_named_decl> opt_iend;

        construct_node(&opt_iend, expr_idx);
        if (!opt_iend.has_value()) {
          goto invalid;
        }  /* if */
        cp = constant_for_named_decl(*opt_iend);
      }
      break;
    case ifc_es_expr_tuple:
      { Opt<an_ifc_expr_tuple> opt_iet;

        construct_node(&opt_iet, expr_idx);
        if (!opt_iet.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_tuple      iet = *opt_iet;
        an_expr_heap_traverser traverser(iet);
        a_constant_ptr         *curr_const_ptr = &cp;
        for (an_Indexed<an_ifc_heap_expr> traversed_ihe : traverser) {
          if (!traversed_ihe.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_expr_index heap_idx = get_ifc_value(*traversed_ihe);
          *curr_const_ptr = constant_for_expr_index(heap_idx,
                                                    /*default_type=*/NULL);
          if (*curr_const_ptr == NULL) {
            goto invalid;
          }  /* if */
          curr_const_ptr = &((*curr_const_ptr)->next);
        }  /* for */
      }
      break;
    case ifc_es_expr_dyad:
      { Opt<an_ifc_expr_dyad> opt_ied;

        construct_node(&opt_ied, expr_idx);
        if (!opt_ied.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_dyad     ied = *opt_ied;
        a_module_token_cache cache;
        a_decl_parse_state   dps;
        a_type_ptr           tp;
        init_decl_parse_state(&dps);
        tp = type_for_type_index(get_ifc_type(ied), /*kind=*/NULL);
        complete_type_is_needed(tp);
        cache_expr(&cache, expr_idx, /*cinfo=*/{});
        if (cache.is_valid()) {
          a_module_entity_rescan rescan(&cache);

          cp = alloc_constant(ck_error);
          scan_constant_initializer_expression(tp, &dps, cp);
        }  /* if */
      }
      break;
    case ifc_es_expr_tokens:
      { Opt<an_ifc_expr_tokens> opt_iet;

        construct_node(&opt_iet, expr_idx);
        if (!opt_iet.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_tokens    iet = *opt_iet;
        an_ifc_type_index     type = get_ifc_type(iet);
        a_module_token_cache  cache;
        a_decl_parse_state    dps;
        a_type_ptr            tp;
        an_init_component_ptr icp;
        an_expr_stack_entry   expr_stack_entry, *saved_expr_stack;
        init_decl_parse_state(&dps);
        if (!is_null_index(type)) {
          tp = type_for_type_index(type, /*kind=*/NULL);
        } else {
          tp = default_type;
        }  /* if */
        check_assertion(tp != NULL);
        complete_type_is_needed(tp);
        cache_sentence(&cache, get_ifc_words(iet));
        if (!cache.is_valid()) {
          goto invalid;
        }  /* if */
        {
          a_module_entity_rescan rescan(&cache);

          cp = alloc_constant(ck_error);
          if (curr_token == tok_assign) {
            dps.init_state.direct_init = FALSE;
            (void)get_token();
          } else {
            dps.init_state.direct_init = TRUE;
          }  /* if */
          dps.type = tp;
          dps.init_state.initializer_must_be_constant = TRUE;
          push_expr_stack_for_initializer(&expr_stack_entry, &saved_expr_stack,
                                          ek_integral_constant,
                                          /*is_full_expr=*/TRUE, &dps,
                                          &dps.init_state);
          icp = scan_expr_or_braced_init_list(/*bundle=*/FALSE,
                                              /*always_allow_braced=*/TRUE);
          an_operand_ptr operand = is_expression_component(icp) ?
                                  operand_of_arg_list_elem(icp) : NULL;
          /* FIXME: This is done to work around initializing array types with
             string literals, as we run into "initializing an array with an
             array" errors.  It's possible it's better to use initializer()
             directly, however, surgery is required to make that work. */
          if (operand != NULL && operand->kind == ok_constant &&
              identical_types(tp, operand->type)) {
            copy_constant(&operand->variant.constant, cp);
          } else {
            /* is_var_init is set to FALSE here, as otherwise it expects
               dps.sym to be non-NULL and point to a variable symbol, which we
               do not have available here. */
            convert_initializer(icp, dps.type, /*is_var_init=*/FALSE,
                                /*fill_in_dtor=*/FALSE, &dps.init_state);
            if (dps.init_state.init_error) {
              set_error_constant(cp);
            } else if (dps.init_state.init_dip != NULL) {
              a_diag_list diag_list;

              clear_diag_list(&diag_list);
              if (!interpret_dynamic_init(dps.init_state.init_dip,
                                          init_component_pos(icp), dps.type,
                                          /*is_constant_evaluated=*/TRUE,
                                          cp, &diag_list)) {
                set_error_constant(cp);
              }  /* if */
              discard_more_info_list(&diag_list);
            } else {
              check_assertion(dps.init_state.init_con != NULL);
              copy_constant(dps.init_state.init_con, cp);
            }  /* if */
          }  /* if */
          free_init_component_list(icp);
          pop_expr_stack_for_initializer(saved_expr_stack,
                                         /*is_full_expr=*/TRUE, &dps,
                                         (an_init_state *)NULL);
        }
      }
      break;
    case ifc_es_expr_string:
      { Opt<an_ifc_expr_string> opt_ies;

        construct_node(&opt_ies, expr_idx);
        if (!opt_ies.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_string          ies = *opt_ies;
        Opt<an_ifc_const_str>       opt_ics;
        an_ifc_string_index         raw_str_index = get_ifc_string_index(ies);
        an_ifc_partition_kind_index str_idx{raw_str_index.mod,
                                            ifc_pk_const_str,
                                            raw_str_index.value};
        construct_node(&opt_ics, str_idx);
        if (!opt_ics.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_const_str ics = *opt_ics;
        sizeof_t         length = (sizeof_t)get_ifc_length(ics);
        char             *str_val = alloc_text_of_string_literal(length);

        memcpy(str_val, get_string_at_offset(get_ifc_start(ics)), length);
        check_assertion(str_val != NULL);
        cp = alloc_constant(ck_string);
        cp->type = type_for_type_index(get_ifc_type(ies), /*kind=*/NULL);
        cp->variant.string.length = length;
        cp->variant.string.value  = str_val;
        cp->variant.string.literal_kind = SCLK_ORDINARY_STRING_LITERAL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        source_position_from_locus(&cp->end_position, get_ifc_locus(ies));
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }
      break;
    default:
      { a_string err_msg("Unexpected ", str_for(expr_idx.sort),
                         " for constant synthesis");

        ifc_unexpected(mod, err_msg);
      }
      goto invalid;
  }  /* switch */
  goto done;
invalid:
  cp = alloc_error_constant();
  expect_error_str("expected errors for bad constant");
done:
  return cp;
}  /* constant_for_expr_index */


a_constant_ptr an_ifc_module::constant_for_named_decl(
                                           const an_ifc_expr_named_decl &iendp)
/*
Returns a constant (allocated in the current IL memory region) with the value
corresponding to the provided named declaration.  Assumes the expression is
constant.
FIXME: shared or unshared?
FIXME: what other types of named declarations can we get here?
*/
{
  an_ifc_decl_index decl_idx = get_ifc_resolution(iendp);
  a_type_ptr        type = type_for_type_index(get_ifc_type(iendp),
                                               /*kind=*/NULL);
  a_constant_ptr    cp = NULL;

  switch (decl_idx.sort) {
    case ifc_ds_decl_enumerator:
      { Opt<an_ifc_decl_enumerator> opt_ide;

        construct_node(&opt_ide, decl_idx);
        if (!opt_ide.has_value()) {
          goto invalid;
        }  /* if */

        /* Skip straight to the enumerator value. */
        an_ifc_decl_enumerator ide = *opt_ide;
        an_ifc_expr_index      initializer = get_ifc_initializer(ide);
        check_assertion(!is_null_index(initializer));
        cp = constant_for_expr_index(initializer, type);
      }
      break;
    case ifc_ds_decl_function:
      issue_unsupported_construct_error(this, "DeclSort::Function"
                                        " for ExprSort::NamedDecl",
                                        &error_position);
      goto invalid;
    default:
      { a_string err_msg("Unexpected ", str_for(decl_idx.sort),
                         " for ExprSort::NamedDecl");

        ifc_unexpected(decl_idx.mod, err_msg);
      }
      goto invalid;
  }  /* switch */
  goto done;
invalid:
  cp = alloc_error_constant();
  expect_error_str("expected errors for bad constant");
done:
  return cp;
}  /* constant_for_named_decl */


a_boolean an_ifc_module::fill_in_routine_parameter_defaults(
                                               an_ifc_chart_index params,
                                               a_type_ptr         rout_type,
                                               a_boolean          is_consteval)
/*
Fill in any parameter defaults for a routine with type rout_type.  The
parameter type list must already be populated.  params is the chart index
associated with the routine and contains the default argument information.  If
the routine is a consteval routine, is_consteval is TRUE.  Return TRUE if
successful, FALSE if any errors were encountered.
*/
{
  a_routine_type_supplement_ptr rtsp = rout_type_supp(rout_type);
  Opt<an_ifc_chart_unilevel>    opt_icu;
  a_param_type_ptr              ptp = rtsp->param_type_list;
  a_boolean                     result = TRUE;

  if (is_null_index(params)) {
    goto done;
  }  /* if */
  check_assertion(params.sort == ifc_cs_chart_unilevel);
  construct_node(&opt_icu, params);
  if (opt_icu.has_value()) {
    an_ifc_chart_unilevel      icu = *opt_icu;
    a_decl_parameter_traverser traverser(icu);

    for (an_Indexed<an_ifc_decl_parameter> indexed_param : traverser) {
      if (ptp == NULL) {
        /* This should only be possible to encounter when the function has
           an ellipsis parameter. */
        check_assertion((get_relative_index(traverser, indexed_param) ==
                         get_ifc_cardinality(icu) - 1) &&
                        rtsp->has_ellipsis);
        break;
      }  /* if */
      if (!indexed_param.has_value()) {
        result = FALSE;
        goto done;
      }  /* if */
      an_ifc_decl_parameter curr_param = *indexed_param;
      an_ifc_expr_index     initializer_expr = get_ifc_initializer(curr_param);
      /* FIXME: Should we issue a diagnostic or attempt to determine expression
         equivalencies here if the parameter already has a default argument?
         Due to the way that IFC handles these, duplicate expressions are very
         possible. */
      if (!is_null_index(initializer_expr)) {
        a_module_token_cache cache;

        ptp->has_default_arg = TRUE;
        cache_expr(&cache, initializer_expr, /*cinfo=*/{});
        if (!cache.is_valid()) {
          ptp->default_arg_expr = error_node();
          result = FALSE;
          continue;
        }  /* if */

        a_module_entity_rescan rescan(&cache);
        scan_default_arg_expr(ptp, /*is_member_or_friend=*/FALSE,
                              is_consteval);
      }  /* if */
      ptp = ptp->next;
    }  /* for */
  }  /* if */
done:
  return result;
}  /* fill_in_routine_parameter_defaults */

namespace {

/*
An enumeration returned by get_ident_res to specify how to handle a purported
"identifier" in an IFC file.
*/
enum an_ifc_identifier_resolution {
  iir_direct_cache,     /* Identifier contains valid characters. */
  iir_recover_via_skip, /* Skip (and warn) on something unknown. */
  iir_cache_from_string,/* Used for things like "operator<" where it's not
                           technically an identifier, but parsing the string
                           into tokens will give the desired result (e.g.,
                           tok_operator tok_lt in this case). */
  iir_error             /* Give an error on what appears to be an invalid
                           identifier. */
};

}  /* namespace */


static a_boolean identifier_is_valid(a_const_char *id_start)
/*
Given the start of a null-terminated IFC character sequence, determine if the
sequence is a valid UTF-8 identifier.  Return TRUE if the identifier is valid;
otherwise, return FALSE.
*/
{
  a_boolean              valid = TRUE;
  int                    char_len = 1;
  /* Force on UTF-8 relevant compiler flags to ensure the identifier is
     properly interpreted. */
#if UNICODE_SOURCE_SUPPORTED
  Value_saver<a_unicode_source_kind>
                         force_unicode(&curr_file_unicode_source_kind,
                                       /*new_value=*/usk_utf8);
#endif /* UNICODE_SOURCE_SUPPORTED */
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  Value_saver<a_boolean> force_multibyte(&multibyte_chars_in_source_enabled,
                                         /*new_value=*/TRUE);
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

  /* Loop through the characters while the identifier is still considered
     valid, up until the null character terminating the string. */
  for (a_const_char *p = id_start; valid && *p != '\0'; p += char_len) {
    valid = is_identifier_char(p, &char_len,
                               /*is_identifier_start=*/(p == id_start));
  }  /* for */
  return valid;
}  /* identifier_is_valid */


static an_ifc_identifier_resolution get_ident_res(a_const_char *id_start)
/*
Consider the given null-terminated IFC character sequence as a C++ identifier.
If the sequence is a valid C++ identifier return iir_direct_cache.  If the
sequence is an invalid identifier conceptually representing "no identifier"
return iir_recover_via_skip.  Otherwise, the sequence is an invalid identifier
with no known special meaning and iir_error will be returned.
*/
{
  an_ifc_identifier_resolution result = iir_direct_cache;

  check_assertion(id_start != NULL);
  if (!identifier_is_valid(id_start)) {
    result = iir_error;
    if (is_unnamed_tag(id_start)) {
      /* The IFC files also contain synthesized names for "unnamed" types of
         the form "<unnamed-XXX>" (where XXX is a placeholder).

         As an example, the following may be given the name "<unnamed-enum-x>".

           enum { x };

         This will be written before getting to this point as
         "__noname_enum_x_".  However, at the time of writing no such special
         handling exists for "<unnamed-tag>", instead we handle it by skipping
         the generation of the identifier token at this point.

         This is checked after identifier validity as doing so prevents any
         negative performance impact on the "happy path." */
      result = iir_recover_via_skip;
    } else if (strlen(id_start) > 8 && strncmp(id_start, "operator", 8) == 0) {
      /* Technically not an "identifier", but assume strings that start with
         "operator" can be handled by caching the appropriate token sequence
         from the string. */
      result = iir_cache_from_string;
    } else {
      /* FIXME: Other currently un-handled "identifiers" that get here:
         {ctor}, __$ReturnUdt, ?$numeric_limits@C, ?$numeric_limits@F. */
    }  /* if */
  }  /* if */
  return result;
}  /* get_ident_res */


static void cache_identifier(a_module_token_cache_ptr cache,
                             a_const_char             *name,
                             a_source_position_ptr    pos)
/*
Add a tok_identifier for name to cache.

If a position is passed it will be used as the source position; otherwise the
position will be inferred (see infer_next_source_position for details about the
rules of position inference).
*/
{
  pos = infer_next_source_position(cache, pos);
  /* FIXME: Eventually the IFC should mature to a point this can be handled in
     the validator during format reading, rather than during token caching and
     (hopefully) without corrections being required.  Until that time, handle
     identifier validity "on use" and potentially attempt some primitive
     corrections. */
  switch (get_ident_res(name)) {
    case iir_error:
      cache->invalidate();
      pos_st_error(ec_ifc_bad_identifier, pos, name);
      break;
    case iir_recover_via_skip:
      pos_st_warning(ec_ifc_bad_identifier_skipped, pos, name);
      break;
    case iir_direct_cache:
      { sizeof_t  len = strlen(name);
        if (len == sizeof("__formal")-1 &&
            strcmp(name, "__formal") == 0) {
          /* "__formal" is the IFC name given to unnamed parameters.  If there
             are multiple such names, an error would ensue.  We therefore do
             not render this name.  E.g., instead of
               void f(int __formal, int __formal);
             we produce
               void f(int, int); */
        } else {
          a_symbol_locator loc;
          clear_locator(&loc, pos);
          (void)find_symbol(name, len, &loc);
          cache_token(cache, tok_identifier, pos);

          a_cached_token_ptr last_token = cache->get_last_token();
          last_token->extra_info_kind = teik_identifier;
          last_token->variant.locator = loc;
        }  /* if */
      }
      break;
    case iir_cache_from_string:
      /* Tokenize the string into the cache (e.g., "operator<" would become
         tok_operator tok_lt). */
      cache_tokens_from_string(name, cache->as_canonical(), pos);
      break;
    default_is_unexpected();
  }  /* switch */
}  /* cache_identifier */


static void cache_literal(an_ifc_module            *mod,
                          a_module_token_cache_ptr cache,
                          a_constant_ptr           lit_const,
                          a_source_position_ptr    pos = NULL)
/*
Add a tok_literal for lit_const (a literal constant formed from information in
the given module) to cache.  Do not use this for boolean literals, string
literals, or user-defined literals (see cache_bool_literal,
cache_string_literal, and cache_ud_literal for those).

If a position is passed it will be used as the source position; otherwise the
position will be inferred (see infer_next_source_position for details about the
rules of position inference).
*/
{
  a_type_ptr   lit_type = lit_const->type;
  a_token_kind lit_kind;

  pos = infer_next_source_position(cache, pos);
  if (is_floating_type(lit_type)) {
    lit_kind = tok_float_constant;
#if FIXED_POINT_ALLOWED
  } else if (is_fixed_point_type(lit_type)) {
    lit_kind = tok_fixed_point_constant;
#endif /* FIXED_POINT_ALLOWED */
  } else if (is_character_type(lit_type)) {
    lit_kind = tok_char_constant;
  } else if (is_integral_type(lit_type) || is_pointer_type(lit_type) ||
             is_nullptr_type(lit_type)) {
    /* When caching pointer literals, cast to the correct pointer type. */
    if (is_pointer_type(lit_type)) {
      cache_token(cache, tok_lparen, pos);
      cache_resolved_type_token(cache, lit_type, pos);
      cache_token(cache, tok_rparen, pos);
    }  /* if */
    lit_kind = tok_int_constant;
  } else {
    ifc_requirement(mod, is_error_type(lit_type), "unhandled literal type");
    lit_kind = tok_error;
  }  /* if */
  cache_token(cache, lit_kind, pos);
  if (lit_kind != tok_error) {
    a_cached_token_ptr last_token = cache->get_last_token();

    last_token->extra_info_kind = (a_token_extra_info_kind)teik_constant;
    last_token->variant.constant = alloc_cached_constant();
    copy_constant(lit_const, last_token->variant.constant);
  }  /* if */
}  /* cache_literal */


static void cache_bool_literal(a_module_token_cache_ptr cache,
                               a_boolean                value,
                               a_source_position_ptr    pos = NULL)
/*
Cache a "true" or "false" token, depending on value.

If a position is passed it will be used as the source position; otherwise the
position will be inferred (see infer_next_source_position for details about the
rules of position inference).
*/
{
  pos = infer_next_source_position(cache, pos);
  cache_token(cache, value ? tok_true : tok_false, pos);

  a_cached_token_ptr last_token = cache->get_last_token();
  last_token->extra_info_kind = (a_token_extra_info_kind)teik_constant;
  last_token->variant.constant = alloc_cached_constant();
  make_bool_constant_value(value, last_token->variant.constant);
}  /* cache_bool_literal */


static void cache_string_literal(a_module_token_cache_ptr cache,
                                 a_character_kind         kind,
                                 a_const_char             *str,
                                 a_targ_size_t            length)
/*
Add a tok_string_literal for str with the given length to cache.  kind is the
type of string literal (e.g., UTF-8, wchar, etc).
*/
{
  a_constant_ptr     cp;
  char               *val;
  a_cached_token     *prev_string = NULL;

  {
    a_cached_token_ptr last_token = cache->get_last_token();

    if (last_token != NULL && last_token->token == tok_string_literal) {
      prev_string = last_token;
    }  /* if */
  }
  cache_token(cache, tok_string_literal);
  {
    a_cached_token_ptr last_token = cache->get_last_token();

    last_token->extra_info_kind = (a_token_extra_info_kind)teik_constant;
    last_token->variant.constant = cp = alloc_cached_constant();
  }
  val = alloc_text_of_string_literal((sizeof_t)length);
  (void)memcpy(val, str, length);
  clear_constant(cp, (a_constant_repr_kind)ck_string);
  cp->type = string_literal_type(kind, length);
  cp->variant.string.length = length;
  cp->variant.string.value  = val;
  cp->variant.string.literal_kind = SCLK_ORDINARY_STRING_LITERAL;
  if (prev_string != NULL) {
    a_token_cache_ptr canonical_cache = cache->as_canonical();

    concat_string_literals(canonical_cache, kind, prev_string);
    remove_token_from_cache(canonical_cache->last_token, &prev_string,
                            canonical_cache);
  }  /* if */
}  /* cache_string_literal */


static void cache_ud_literal(a_module_token_cache_ptr cache,
                             a_character_kind         kind,
                             a_const_char             *str,
                             a_targ_size_t            length,
                             a_const_char             *suffix,
                             a_source_position_ptr    pos = NULL)
/*
Add a tok_ud_literal for str with the given length and suffix to cache.  kind
is the type of string literal (e.g., UTF-8, wchar, etc).

If a position is passed it will be used as the source position; otherwise the
position will be inferred (see infer_next_source_position for details about the
rules of position inference).
*/
{
  a_constant_ptr     cp;
  char*              val;
  a_targ_size_t      suffix_len = (a_targ_size_t)(strlen(suffix) + 1);
  a_cached_token_ptr ctp;

  pos = infer_next_source_position(cache, pos);
  cache_token(cache, tok_ud_literal, pos);
  ctp = cache->get_last_token();
  ctp->extra_info_kind = (a_token_extra_info_kind)teik_ud_lit;
  ctp->variant.ud_lit.value_con = cp = alloc_cached_constant();
  val = alloc_text_of_string_literal((sizeof_t)length);
  (void)memcpy(val, str, length);
  clear_constant(cp, (a_constant_repr_kind)ck_string);
  cp->type = string_literal_type(kind, length);
  cp->variant.string.length = length;
  cp->variant.string.value  = val;
  cp->variant.string.literal_kind = SCLK_ORDINARY_STRING_LITERAL;
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


static void cache_aggr_constant(a_module_token_cache_ptr cache,
                                a_constant_ptr           cp)
/*
Add a tok_aggr_constant token with the provided constant to cache.
*/
{
  cache_token(cache, tok_aggr_constant);

  a_cached_token_ptr last_token = cache->get_last_token();
  last_token->extra_info_kind = (a_token_extra_info_kind)teik_constant;
  last_token->variant.constant = alloc_cached_constant();
  copy_constant(cp, last_token->variant.constant);
}  /* cache_aggr_constant */


a_dynamic_init_ptr load_variable_init_from_ifc_module(
                                                 a_type_ptr        tp,
                                                 an_ifc_expr_index init_expr)
/*
Return the initializer for a variable or field (not provided) of given type tp,
with the initializer expression referred to by init_expr.
*/
{
  a_dynamic_init_ptr result;
  a_constant_ptr     cp;
  an_ifc_module      *ifc_module = init_expr.mod;

  result = alloc_dynamic_init(dik_module);
  cp = ifc_module->constant_for_expr_index(init_expr, tp);
  if (cp != NULL && !is_error_constant(cp)) {
    result->variant.constant.ptr = alloc_unshared_constant(cp);
  }  /* if */
  return result;
}  /* load_variable_init_from_ifc_module */


static void cache_pragma(a_module_token_cache_ptr cache,
                         a_pragma_kind            kind,
                         a_source_position_ptr    pos = NULL)
/*
Add the pragma given by kind to cache.

If a position is passed it will be used as the source position; otherwise the
position will be inferred (see infer_next_source_position for details about the
rules of position inference).
*/
{
  a_pending_pragma_ptr          ppp, *next_pragma;
  a_pragma_kind_description_ptr pkdp;

  pos = infer_next_source_position(cache, pos);
  pkdp = pragma_description_for_pragma_kind[(int)kind];
  ppp = alloc_pending_pragma(pkdp);
  ppp->id_position = *pos;
  ppp->pragma_position = *pos;
  {
    /* Create a token to hold the pragmas if needed. */
    a_cached_token_ptr last_token = cache->get_last_token();

    if (last_token == NULL || last_token->extra_info_kind !=
                                        (a_token_extra_info_kind)teik_pragma) {
      cache_token(cache, tok_error, pos);
    }  /* if */
  }
  next_pragma = &(cache->get_last_token()->variant.pragmas);
  /* Find the end of the pragma list. */
  for (; *next_pragma != NULL; next_pragma = &(*next_pragma)->next) {}
  *next_pragma = ppp;
#if DEBUG
  add_to_pragmas_in_reuseable_cache_count(1);
  cache->as_canonical()->pragma_count++;
#endif /* DEBUG */
}  /* cache_pragma */


static void cache_pp_token(a_module_token_cache_ptr cache,
                           a_const_char             *text,
                           a_targ_size_t            len)
/*
Add the preprocessor token contained in text with the given length to cache.
*/
{
  check_assertion(text != NULL);
  cache_token(cache, tok_identifier);

  a_cached_token_ptr last_token = cache->get_last_token();
  last_token->extra_info_kind =(a_token_extra_info_kind)teik_pp_token;
  last_token->variant.pp_token_descr.token_start = (char*)text;
  last_token->variant.pp_token_descr.token_end = (char*)text + len;
}  /* cache_pp_token */


static void cache_access_specifier(a_module_token_cache_ptr cache,
                                   an_ifc_access_sort       access)
/*
Add tokens corresponding to access (if any) to cache.
*/
{
  switch (access) {
    case ifc_as_none:
      /* Nothing to cache. */
      break;
    case ifc_as_private:
      cache_token(cache, tok_private);
      break;
    case ifc_as_protected:
      cache_token(cache, tok_protected);
      break;
    case ifc_as_public:
      cache_token(cache, tok_public);
      break;
    default_is_unexpected();
  }  /* switch */
}  /* cache_access_specifier */


void an_ifc_module::cache_source_directive(
                                       a_module_token_cache_ptr      cache,
                                       an_ifc_source_directive_sort  directive)
/*
Add tokens corresponding to directive to cache.
*/
{
  switch (directive) {
    case ifc_sds_msvc:
      break;
    case ifc_sds_msvc_pragma_comment:
      cache_pragma(cache, pk_comment);
      break;
    case ifc_sds_msvc_pragma_conform:
      cache_pragma(cache, pk_conform);
      break;
    case ifc_sds_msvc_pragma_ident:
#if IDENT_DIRECTIVE_AND_PRAGMA
      cache_pragma(cache, pk_ident_pragma);
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
      break;
    case ifc_sds_msvc_pragma_include_alias:
      cache_pragma(cache, pk_include_alias);
      break;
    case ifc_sds_msvc_pragma_pack:
      cache_pragma(cache, pk_pack);
      break;
    case ifc_sds_msvc_pragma_pop_macro:
      cache_pragma(cache, pk_pop_macro);
      break;
    case ifc_sds_msvc_pragma_push_macro:
      cache_pragma(cache, pk_push_macro);
      break;
    case ifc_sds_msvc_pragma_setlocale:
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
      cache_pragma(cache, pk_setlocale);
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
      break;
    case ifc_sds_msvc_pragma_start_map_region:
      cache_pragma(cache, pk_start_map_region);
      break;
    case ifc_sds_msvc_pragma_stop_map_region:
      cache_pragma(cache, pk_stop_map_region);
      break;
    case ifc_sds_msvc_pragma_push:
    case ifc_sds_msvc_pragma_pop:
    case ifc_sds_msvc_directive_start:
    case ifc_sds_msvc_directive_end:
    case ifc_sds_msvc_pragma_alloc_text:
    case ifc_sds_msvc_pragma_auto_inline:
    case ifc_sds_msvc_pragma_bss_seg:
    case ifc_sds_msvc_pragma_check_stack:
    case ifc_sds_msvc_pragma_code_seg:
    case ifc_sds_msvc_pragma_component:
    case ifc_sds_msvc_pragma_const_seg:
    case ifc_sds_msvc_pragma_data_seg:
    case ifc_sds_msvc_pragma_deprecated:
    case ifc_sds_msvc_pragma_detect_mismatch:
    case ifc_sds_msvc_pragma_endregion:
    case ifc_sds_msvc_pragma_execution_character_set:
    case ifc_sds_msvc_pragma_fenv_access:
    case ifc_sds_msvc_pragma_file_hash:
    case ifc_sds_msvc_pragma_float_control:
    case ifc_sds_msvc_pragma_fp_contract:
    case ifc_sds_msvc_pragma_function:
    case ifc_sds_msvc_pragma_bgi:
    case ifc_sds_msvc_pragma_implementation_key:
    case ifc_sds_msvc_pragma_init_seq:
    case ifc_sds_msvc_pragma_inline_depth:
    case ifc_sds_msvc_pragma_inline_recursion:
    case ifc_sds_msvc_pragma_intrinsic:
    case ifc_sds_msvc_pragma_loop:
    case ifc_sds_msvc_pragma_make_public:
    case ifc_sds_msvc_pragma_managed:
    case ifc_sds_msvc_pragma_message:
    case ifc_sds_msvc_pragma_omp:
    case ifc_sds_msvc_pragma_optimize:
    case ifc_sds_msvc_pragma_pointer_to_members:
    case ifc_sds_msvc_pragma_prefast:
    case ifc_sds_msvc_pragma_region:
    case ifc_sds_msvc_pragma_runtime_checks:
    case ifc_sds_msvc_pragma_same_seg:
    case ifc_sds_msvc_pragma_section:
    case ifc_sds_msvc_pragma_segment:
    case ifc_sds_msvc_pragma_strict_gs_check:
    case ifc_sds_msvc_pragma_system_header:
    case ifc_sds_msvc_pragma_unmanaged:
    case ifc_sds_msvc_pragma_vtordisp:
    case ifc_sds_msvc_pragma_warning:
    case ifc_sds_msvc_pragma_p0include:
    case ifc_sds_msvc_pragma_p0line:
      /* These pragmas have no corresponding EDG pragma. */
      break;
    default_is_unexpected_str("Unknown SourceDirective");
  }  /* switch */
}  /* cache_source_directive */


void an_ifc_module::cache_source_punctuator(
                                      a_module_token_cache_ptr      cache,
                                      an_ifc_source_punctuator_sort punctuator)
/*
Add tokens corresponding to punctuator to cache.
*/
{
  switch (punctuator) {
    case ifc_sps_unknown:
    case ifc_sps_msvc:
      { a_string err_msg("Unexpected ", str_for(punctuator));

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_sps_left_parenthesis:
      cache_token(cache, tok_lparen);
      break;
    case ifc_sps_right_parenthesis:
      cache_token(cache, tok_rparen);
      break;
    case ifc_sps_left_bracket:
      cache_token(cache, tok_lbracket);
      break;
    case ifc_sps_right_bracket:
      cache_token(cache, tok_rbracket);
      break;
    case ifc_sps_left_brace:
      cache_token(cache, tok_lbrace);
      break;
    case ifc_sps_right_brace:
      cache_token(cache, tok_rbrace);
      break;
    case ifc_sps_colon:
      cache_token(cache, tok_colon);
      break;
    case ifc_sps_question:
      cache_token(cache, tok_quest_mark);
      break;
    case ifc_sps_semicolon:
      cache_token(cache, tok_semicolon);
      break;
    case ifc_sps_colon_colon:
      cache_token(cache, tok_colon_colon);
      break;
    case ifc_sps_msvc_zero_width_space:
      break;
    case ifc_sps_msvc_end_of_phrase:
      break;
    case ifc_sps_msvc_full_stop:
      break;
    case ifc_sps_msvc_nested_template_start:
      cache_token(cache, tok_template);
      break;
    case ifc_sps_msvc_default_argument_start:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                                  this,
                                  "SourcePunctuator::MsvcDefaultArgumentStart",
                                  &error_position);
      goto invalid;
    case ifc_sps_msvc_alignas_edict_start:
      break;
    case ifc_sps_msvc_default_init_start:
      /* A marker to indicate the start of a default member initializer.  It
         isn't needed in the source code. */
      break;
    default_is_unexpected_str("Unknown SourcePunctuator");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad source punctuator cache");
  cache->invalidate();
done:;
}  /* cache_source_punctuator */


void an_ifc_module::cache_source_literal(
                                 a_module_token_cache_ptr             cache,
                                 const an_ifc_source_literal_category &literal)
/*
Add tokens corresponding to literal to cache.  index is the index into the IFC
file for the additional information needed, depending on the kind of literal.
*/
{
  switch (literal.sort) {
    case ifc_sls_unknown:
      { a_string err_msg("Unexpected ", str_for(literal.sort));

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_sls_scalar:
      cache_expr(cache, literal.variant.scalar, /*cinfo=*/{});
      break;
    case ifc_sls_string:
      cache_string(cache, literal.variant.string);
      break;
    case ifc_sls_defined_string:
      cache_string(cache, literal.variant.defined_string);
      break;
    case ifc_sls_msvc:
      break;
    case ifc_sls_msvc_function_name_macro:
      { an_ifc_text_offset name_offset =
                                      literal.variant.msvc_function_name_macro;
        a_const_char       *str = get_string_at_offset(name_offset);
        a_token_kind       tok;

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
        cache_token(cache, tok);
      }
      break;
    case ifc_sls_msvc_string_prefix_macro:
      { an_ifc_text_offset prefix_offset =
                                      literal.variant.msvc_function_name_macro;
        a_const_char       *str = get_string_at_offset(prefix_offset);
        a_token_kind       tok;

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
        cache_token(cache, tok);
      }
      break;
    case ifc_sls_msvc_binding:
      { an_ifc_expr_index binding_expr = literal.variant.msvc_binding;

        switch(binding_expr.sort) {
          case ifc_es_expr_named_decl:
            cache_token_with_index(cache, tok_ifc_entity_ref, binding_expr);
            break;
          case ifc_es_expr_unresolved_id:
            { Opt<an_ifc_expr_unresolved_id> opt_ieud;

              construct_node(&opt_ieud, binding_expr);
              if (!opt_ieud.has_value()) {
                goto invalid;
              }  /* if */
              cache_identifier(cache,
                               string_from_name_index(get_ifc_name(*opt_ieud),
                                                      /*loc=*/NULL));
            }
            break;
          default:
            { a_string err_msg("Unexpected ", str_for(binding_expr.sort),
                               " for ", str_for(literal.sort));

              ifc_unexpected(this, err_msg);
            }
            goto invalid;
        }  /* switch */
      }
      break;
    case ifc_sls_msvc_resolved_type:
      cache_type(cache, literal.variant.msvc_resolved_type);
      break;
    case ifc_sls_msvc_defined_constant:
      /* FIXME: Is this correct? */
      cache_expr(cache, literal.variant.msvc_defined_constant, /*cinfo=*/{});
      break;
    case ifc_sls_msvc_cast_target_type:
      /* FIXME: Is this correct? */
      cache_token(cache, tok_lparen);
      cache_type(cache, literal.variant.msvc_cast_target_type);
      cache_token(cache, tok_rparen);
      break;
    default_is_unexpected_str("Unknown SourceLiteral");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad source literal cache");
  cache->invalidate();
done:;
}  /* cache_source_literal */


void an_ifc_module::cache_source_operator(a_module_token_cache_ptr     cache,
                                          an_ifc_source_operator_sort  op)
/*
Add tokens corresponding to op to cache.
*/
{
  switch (op) {
    case ifc_sos_unknown:
      { a_string err_msg("Unexpected ", str_for(op));

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_sos_equal:
      cache_token(cache, tok_assign);
      break;
    case ifc_sos_comma:
      cache_token(cache, tok_comma);
      break;
    case ifc_sos_exclaim:
      cache_token(cache, tok_not);
      break;
    case ifc_sos_plus:
      cache_token(cache, tok_plus);
      break;
    case ifc_sos_dash:
      cache_token(cache, tok_minus);
      break;
    case ifc_sos_star:
      cache_token(cache, tok_star);
      break;
    case ifc_sos_slash:
      cache_token(cache, tok_divide);
      break;
    case ifc_sos_percent:
      cache_token(cache, tok_remainder);
      break;
    case ifc_sos_left_chevron:
      cache_token(cache, tok_shift_left);
      break;
    case ifc_sos_right_chevron:
      cache_token(cache, tok_shift_right);
      break;
    case ifc_sos_tilde:
      cache_token(cache, tok_compl);
      break;
    case ifc_sos_caret:
      cache_token(cache, tok_excl_or);
      break;
    case ifc_sos_bar:
      cache_token(cache, tok_or);
      break;
    case ifc_sos_ampersand:
      cache_token(cache, tok_ampersand);
      break;
    case ifc_sos_plus_plus:
      cache_token(cache, tok_plus_plus);
      break;
    case ifc_sos_dash_dash:
      cache_token(cache, tok_minus_minus);
      break;
    case ifc_sos_less:
      cache_token(cache, tok_lt);
      break;
    case ifc_sos_less_equal:
      cache_token(cache, tok_le);
      break;
    case ifc_sos_greater:
      cache_token(cache, tok_gt);
      break;
    case ifc_sos_greater_equal:
      cache_token(cache, tok_ge);
      break;
    case ifc_sos_equal_equal:
      cache_token(cache, tok_eq);
      break;
    case ifc_sos_exclaim_equal:
      cache_token(cache, tok_ne);
      break;
    case ifc_sos_diamond:
      cache_token(cache, tok_spaceship);
      break;
    case ifc_sos_plus_equal:
      cache_token(cache, tok_plus_assign);
      break;
    case ifc_sos_dash_equal:
      cache_token(cache, tok_minus_assign);
      break;
    case ifc_sos_star_equal:
      cache_token(cache, tok_times_assign);
      break;
    case ifc_sos_slash_equal:
      cache_token(cache, tok_divide_assign);
      break;
    case ifc_sos_percent_equal:
      cache_token(cache, tok_remainder_assign);
      break;
    case ifc_sos_ampersand_equal:
      cache_token(cache, tok_and_assign);
      break;
    case ifc_sos_bar_equal:
      cache_token(cache, tok_or_assign);
      break;
    case ifc_sos_caret_equal:
      cache_token(cache, tok_excl_or_assign);
      break;
    case ifc_sos_left_chevron_equal:
      cache_token(cache, tok_shift_left_assign);
      break;
    case ifc_sos_right_chevron_equal:
      cache_token(cache, tok_shift_right_assign);
      break;
    case ifc_sos_ampersand_ampersand:
      cache_token(cache, tok_and_and);
      break;
    case ifc_sos_bar_bar:
      cache_token(cache, tok_or_or);
      break;
    case ifc_sos_ellipsis:
      cache_token(cache, tok_ellipsis);
      break;
    case ifc_sos_dot:
      cache_token(cache, tok_period);
      break;
    case ifc_sos_arrow:
      cache_token(cache, tok_arrow);
      break;
    case ifc_sos_dot_star:
      cache_token(cache, tok_period_star);
      break;
    case ifc_sos_arrow_star:
      cache_token(cache, tok_arrow_star);
      break;
    default_is_unexpected_str("Unknown SourceOperator");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad source operator cache");
  cache->invalidate();
done:;
}  /* cache_source_operator */


void an_ifc_module::cache_source_keyword(a_module_token_cache_ptr   cache,
                                         an_ifc_source_keyword_sort keyword)
/*
Add tokens corresponding to keyword to cache.
*/
{
  switch (keyword) {
    case ifc_sks_unknown:
    case ifc_sks_msvc:
      { a_string err_msg("Unexpected ", str_for(keyword));

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_sks_alignas:
      cache_token(cache, tok_alignas);
      break;
    case ifc_sks_alignof:
      cache_token(cache, tok_alignof);
      break;
    case ifc_sks_asm:
      cache_token(cache, tok_asm);
      break;
    case ifc_sks_auto:
      cache_token(cache, tok_auto);
      break;
    case ifc_sks_bool:
      cache_token(cache, tok_bool);
      break;
    case ifc_sks_break:
      cache_token(cache, tok_break);
      break;
    case ifc_sks_case:
      cache_token(cache, tok_case);
      break;
    case ifc_sks_catch:
      cache_token(cache, tok_catch);
      break;
    case ifc_sks_char:
      cache_token(cache, tok_char);
      break;
    case ifc_sks_char8_t:
      cache_token(cache, tok_char8_t);
      break;
    case ifc_sks_char16_t:
      cache_token(cache, tok_char16_t);
      break;
    case ifc_sks_char32_t:
      cache_token(cache, tok_char32_t);
      break;
    case ifc_sks_class:
      cache_token(cache, tok_class);
      break;
    case ifc_sks_concept:
      cache_token(cache, tok_concept);
      break;
    case ifc_sks_const:
      cache_token(cache, tok_const);
      break;
    case ifc_sks_consteval:
      cache_token(cache, tok_consteval);
      break;
    case ifc_sks_constexpr:
      cache_token(cache, tok_constexpr);
      break;
    case ifc_sks_constinit:
      cache_token(cache, tok_constinit);
      break;
    case ifc_sks_const_cast:
      cache_token(cache, tok_const_cast);
      break;
    case ifc_sks_continue:
      cache_token(cache, tok_continue);
      break;
    case ifc_sks_co_await:
      cache_token(cache, tok_coroutine_await);
      break;
    case ifc_sks_co_return:
      cache_token(cache, tok_coroutine_return);
      break;
    case ifc_sks_co_yield:
      cache_token(cache, tok_coroutine_yield);
      break;
    case ifc_sks_decltype:
      cache_token(cache, tok_decltype);
      break;
    case ifc_sks_default:
      cache_token(cache, tok_default);
      break;
    case ifc_sks_delete:
      cache_token(cache, tok_delete);
      break;
    case ifc_sks_do:
      cache_token(cache, tok_do);
      break;
    case ifc_sks_double:
      cache_token(cache, tok_double);
      break;
    case ifc_sks_dynamic_cast:
      cache_token(cache, tok_dynamic_cast);
      break;
    case ifc_sks_else:
      cache_token(cache, tok_else);
      break;
    case ifc_sks_enum:
      cache_token(cache, tok_enum);
      break;
    case ifc_sks_explicit:
      cache_token(cache, tok_explicit);
      break;
    case ifc_sks_export:
      cache_token(cache, tok_export);
      break;
    case ifc_sks_extern:
      cache_token(cache, tok_extern);
      break;
    case ifc_sks_false:
      cache_bool_literal(cache, FALSE);
      break;
    case ifc_sks_float:
      cache_token(cache, tok_float);
      break;
    case ifc_sks_for:
      cache_token(cache, tok_for);
      break;
    case ifc_sks_friend:
      if (this->suppress_friend_token) {
        /* Do not cache the "friend" keyword.  (Friends are loaded via class
           traits and the keyword will presumably already have been emitted
           when the trait is processed as part of the class definition). */
      } else {
        cache_token(cache, tok_friend);
      }  /* if */
      break;
    case ifc_sks_generic:
      cache_token(cache, tok_c11_generic);
      break;
    case ifc_sks_goto:
      cache_token(cache, tok_goto);
      break;
    case ifc_sks_if:
      cache_token(cache, tok_if);
      break;
    case ifc_sks_inline:
      cache_token(cache, tok_inline);
      break;
    case ifc_sks_int:
      cache_token(cache, tok_int);
      break;
    case ifc_sks_long:
      cache_token(cache, tok_long);
      break;
    case ifc_sks_mutable:
      cache_token(cache, tok_mutable);
      break;
    case ifc_sks_namespace:
      cache_token(cache, tok_namespace);
      break;
    case ifc_sks_new:
      cache_token(cache, tok_new);
      break;
    case ifc_sks_noexcept:
      cache_token(cache, tok_noexcept);
      break;
    case ifc_sks_nullptr:
      cache_token(cache, tok_nullptr);
      break;
    case ifc_sks_operator:
      cache_token(cache, tok_operator);
      break;
    case ifc_sks_pragma:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SourceKeyword::Pragma",
                                        &error_position);
      goto invalid;
    case ifc_sks_private:
      cache_token(cache, tok_private);
      break;
    case ifc_sks_protected:
      cache_token(cache, tok_protected);
      break;
    case ifc_sks_public:
      cache_token(cache, tok_public);
      break;
    case ifc_sks_register:
      cache_token(cache, tok_register);
      break;
    case ifc_sks_reinterpret_cast:
      cache_token(cache, tok_reinterpret_cast);
      break;
    case ifc_sks_requires:
      cache_token(cache, tok_requires);
      break;
    case ifc_sks_restrict:
      cache_token(cache, tok_restrict);
      break;
    case ifc_sks_return:
      cache_token(cache, tok_return);
      break;
    case ifc_sks_short:
      cache_token(cache, tok_short);
      break;
    case ifc_sks_signed:
      cache_token(cache, tok_signed);
      break;
    case ifc_sks_sizeof:
      cache_token(cache, tok_sizeof);
      break;
    case ifc_sks_static:
      cache_token(cache, tok_static);
      break;
    case ifc_sks_static_assert:
      cache_token(cache, tok_static_assert);
      break;
    case ifc_sks_static_cast:
      cache_token(cache, tok_static_cast);
      break;
    case ifc_sks_struct:
      cache_token(cache, tok_struct);
      break;
    case ifc_sks_switch:
      cache_token(cache, tok_switch);
      break;
    case ifc_sks_template:
      cache_token(cache, tok_template);
      break;
    case ifc_sks_this:
      cache_token(cache, tok_this);
      break;
    case ifc_sks_thread_local:
      cache_token(cache, tok_thread_local);
      break;
    case ifc_sks_throw:
      cache_token(cache, tok_throw);
      break;
    case ifc_sks_true:
      cache_bool_literal(cache, TRUE);
      break;
    case ifc_sks_try:
      cache_token(cache, tok_try);
      break;
    case ifc_sks_typedef:
      cache_token(cache, tok_typedef);
      break;
    case ifc_sks_typeid:
      cache_token(cache, tok_typeid);
      break;
    case ifc_sks_typename:
      cache_token(cache, tok_typename);
      break;
    case ifc_sks_union:
      cache_token(cache, tok_union);
      break;
    case ifc_sks_unsigned:
      cache_token(cache, tok_unsigned);
      break;
    case ifc_sks_using:
      cache_token(cache, tok_using);
      break;
    case ifc_sks_virtual:
      cache_token(cache, tok_virtual);
      break;
    case ifc_sks_void:
      cache_token(cache, tok_void);
      break;
    case ifc_sks_volatile:
      cache_token(cache, tok_volatile);
      break;
    case ifc_sks_wchar_t:
      cache_token(cache, tok_wchar_t);
      break;
    case ifc_sks_while:
      cache_token(cache, tok_while);
      break;
    case ifc_sks_msvc_asm:
      cache_token(cache, tok_microsoft_asm);
      break;
    case ifc_sks_msvc_assume:
      cache_token(cache, tok_assume);
      break;
    case ifc_sks_msvc_alignof:
      cache_token(cache, tok_alignof);
      break;
    case ifc_sks_msvc_based:
      cache_token(cache, tok_based);
      break;
    case ifc_sks_msvc_cdecl:
      cache_token(cache, tok_cdecl);
      break;
    case ifc_sks_msvc_clrcall:
      cache_token(cache, tok_clrcall);
      break;
    case ifc_sks_msvc_declspec:
      cache_token(cache, tok_declspec);
      break;
    case ifc_sks_msvc_eabi:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SourceKeyword::MsvcEabi",
                                        &error_position);
      goto invalid;
    case ifc_sks_msvc_event:
      cache_token(cache, tok_event);
      break;
    case ifc_sks_msvc_seh_except:
      cache_token(cache, tok_except);
      break;
    case ifc_sks_msvc_fastcall:
      cache_token(cache, tok_fastcall);
      break;
    case ifc_sks_msvc_seh_finally:
      cache_token(cache, tok_finally);
      break;
    case ifc_sks_msvc_forceinline:
      cache_token(cache, tok_forceinline);
      break;
    case ifc_sks_msvc_hook:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SourceKeyword::MsvcHook",
                                        &error_position);
      goto invalid;
    case ifc_sks_msvc_identifier:
      cache_token(cache, tok_microsoft_identifier);
      break;
    case ifc_sks_msvc_if_exists:
      cache_token(cache, tok_if_exists);
      break;
    case ifc_sks_msvc_if_not_exists:
      cache_token(cache, tok_if_not_exists);
      break;
    case ifc_sks_msvc_int8:
      cache_token(cache, tok_int8);
      break;
    case ifc_sks_msvc_int16:
      cache_token(cache, tok_int16);
      break;
    case ifc_sks_msvc_int32:
      cache_token(cache, tok_int32);
      break;
    case ifc_sks_msvc_int64:
      cache_token(cache, tok_int64);
      break;
    case ifc_sks_msvc_int128:
#if INT128_EXTENSIONS_ALLOWED
      cache_token(cache, tok_int128);
#endif /* INT128_EXTENSIONS_ALLOWED */
      break;
    case ifc_sks_msvc_interface:
      cache_token(cache, tok_interface);
      break;
    case ifc_sks_msvc_leave:
      cache_token(cache, tok_leave);
      break;
    case ifc_sks_msvc_multiple_inheritance:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                                      this,
                                      "SourceKeyword::MsvcMultipleInheritance",
                                      &error_position);
      goto invalid;
    case ifc_sks_msvc_nullptr:
      cache_token(cache, tok_nullptr);
      break;
    case ifc_sks_msvc_novtordisp:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SourceKeyword::MsvcNovtordisp",
                                          &error_position);
      goto invalid;
    case ifc_sks_msvc_pragma:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SourceKeyword::MsvcPragma",
                                        &error_position);
      goto invalid;
    case ifc_sks_msvc_ptr32:
      cache_token(cache, tok_microsoft_ptr32);
      break;
    case ifc_sks_msvc_ptr64:
      cache_token(cache, tok_microsoft_ptr64);
      break;
    case ifc_sks_msvc_restrict:
      cache_token(cache, tok_restrict);
      break;
    case ifc_sks_msvc_single_inheritance:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SourceKeyword::MsvcSingleInheritance",
                                        &error_position);
      goto invalid;
    case ifc_sks_msvc_sptr:
      cache_token(cache, tok_microsoft_sptr);
      break;
    case ifc_sks_msvc_stdcall:
      cache_token(cache, tok_stdcall);
      break;
    case ifc_sks_msvc_super:
      cache_token(cache, tok_super);
      break;
    case ifc_sks_msvc_thiscall:
      cache_token(cache, tok_thiscall);
      break;
    case ifc_sks_msvc_seh_try:
      cache_token(cache, tok_microsoft_try);
      break;
    case ifc_sks_msvc_uptr:
      cache_token(cache, tok_microsoft_uptr);
      break;
    case ifc_sks_msvc_uuidof:
      cache_token(cache, tok_uuidof);
      break;
    case ifc_sks_msvc_unaligned:
      cache_token(cache, tok_unaligned);
      break;
    case ifc_sks_msvc_unhook:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SourceKeyword::MsvcUnhook",
                                        &error_position);
      goto invalid;
    case ifc_sks_msvc_vectorcall:
      cache_token(cache, tok_vectorcall);
      break;
    case ifc_sks_msvc_virtual_inheritance:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                                       this,
                                       "SourceKeyword::MsvcVirtualInheritance",
                                       &error_position);
      goto invalid;
    case ifc_sks_msvc_w64:
      cache_token(cache, tok_microsoft_w64);
      break;
    case ifc_sks_msvc_is_class:
      cache_token(cache, tok_is_class);
      break;
    case ifc_sks_msvc_is_union:
      cache_token(cache, tok_is_union);
      break;
    case ifc_sks_msvc_is_enum:
      cache_token(cache, tok_is_enum);
      break;
    case ifc_sks_msvc_is_polymorphic:
      cache_token(cache, tok_is_polymorphic);
      break;
    case ifc_sks_msvc_is_empty:
      cache_token(cache, tok_is_empty);
      break;
    case ifc_sks_msvc_has_trivial_constructor:
      cache_token(cache, tok_has_trivial_constructor);
      break;
    case ifc_sks_msvc_is_trivially_constructible:
      cache_token(cache, tok_is_trivially_constructible);
      break;
    case ifc_sks_msvc_is_trivially_copy_constructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                             this,
                             "SourceKeyword::MsvcIsTriviallyCopyConstructible",
                             &error_position);
      goto invalid;
    case ifc_sks_msvc_is_trivially_copy_assignable:
      cache_token(cache, tok_is_trivially_copy_assignable);
      break;
    case ifc_sks_msvc_is_trivially_destructible:
      cache_token(cache, tok_is_trivially_destructible);
      break;
    case ifc_sks_msvc_has_virtual_destructor:
      cache_token(cache, tok_has_virtual_destructor);
      break;
    case ifc_sks_msvc_is_nothrow_constructible:
      cache_token(cache, tok_is_nothrow_constructible);
      break;
    case ifc_sks_msvc_is_nothrow_copy_constructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                               this,
                               "SourceKeyword::MsvcIsNothrowCopyConstructible",
                               &error_position);
      goto invalid;
    case ifc_sks_msvc_is_nothrow_copy_assignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                                  this,
                                  "SourceKeyword::MsvcIsNothrowCopyAssignable",
                                  &error_position);
      goto invalid;
    case ifc_sks_msvc_is_pod:
      cache_token(cache, tok_is_pod);
      break;
    case ifc_sks_msvc_is_abstract:
      cache_token(cache, tok_is_abstract);
      break;
    case ifc_sks_msvc_is_base_of:
      cache_token(cache, tok_is_base_of);
      break;
    case ifc_sks_msvc_is_convertibleto:
      cache_token(cache, tok_is_convertible_to);
      break;
    case ifc_sks_msvc_is_trivial:
      cache_token(cache, tok_is_trivial);
      break;
    case ifc_sks_msvc_is_trivially_copyable:
      cache_token(cache, tok_is_trivially_copyable);
      break;
    case ifc_sks_msvc_is_standard_layout:
      cache_token(cache, tok_is_standard_layout);
      break;
    case ifc_sks_msvc_is_literal_type:
      cache_token(cache, tok_is_literal_type);
      break;
    case ifc_sks_msvc_is_trivially_move_constructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                             this,
                             "SourceKeyword::MsvcIsTriviallyMoveConstructible",
                             &error_position);
      goto invalid;
    case ifc_sks_msvc_has_trivial_move_assign:
      cache_token(cache, tok_has_trivial_move_assign);
      break;
    case ifc_sks_msvc_is_trivially_move_assignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                                this,
                                "SourceKeyword::MsvcIsTriviallyMoveAssignable",
                                &error_position);
      goto invalid;
    case ifc_sks_msvc_is_nothrow_move_assignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                                  this,
                                  "SourceKeyword::MsvcIsNothrowMoveAssignable",
                                  &error_position);
      goto invalid;
    case ifc_sks_msvc_is_constructible:
      cache_token(cache, tok_is_constructible);
      break;
    case ifc_sks_msvc_underlying_type:
      cache_token(cache, tok_underlying_type);
      break;
    case ifc_sks_msvc_is_trivially_assignable:
      cache_token(cache, tok_is_trivially_assignable);
      break;
    case ifc_sks_msvc_is_nothrow_assignable:
      cache_token(cache, tok_is_nothrow_assignable);
      break;
    case ifc_sks_msvc_is_destructible:
      cache_token(cache, tok_is_destructible);
      break;
    case ifc_sks_msvc_is_nothrow_destructible:
      cache_token(cache, tok_is_nothrow_destructible);
      break;
    case ifc_sks_msvc_is_assignable:
      cache_token(cache, tok_is_assignable);
      break;
    case ifc_sks_msvc_is_assignable_no_check:
      cache_token(cache, tok_is_assignable_no_precondition_check);
      break;
    case ifc_sks_msvc_has_unique_object_representations:
      cache_token(cache, tok_has_unique_object_representations);
      break;
    case ifc_sks_msvc_is_aggregate:
      cache_token(cache, tok_is_aggregate);
      break;
    case ifc_sks_msvc_builtin_address_of:
      cache_token(cache, tok_builtin_addressof);
      break;
    case ifc_sks_msvc_builtin_offset_of:
      cache_token(cache, tok_builtin_offsetof);
      break;
    case ifc_sks_msvc_builtin_bit_cast:
      cache_token(cache, tok_builtin_bit_cast);
      break;
    case ifc_sks_msvc_builtin_is_layout_compatible:
      cache_token(cache, tok_is_layout_compatible);
      break;
    case ifc_sks_msvc_builtin_is_pointer_interconvertible_base_of:
      cache_token(cache, tok_is_pointer_interconvertible_base_of);
      break;
    case ifc_sks_msvc_builtin_is_pointer_interconvertible_with_class:
      cache_token(cache, tok_is_pointer_interconvertible_with_class);
      break;
    case ifc_sks_msvc_builtin_is_corresponding_member:
      cache_token(cache, tok_is_corresponding_member);
      break;
    case ifc_sks_msvc_is_ref_class:
      cache_token(cache, tok_is_ref_class);
      break;
    case ifc_sks_msvc_is_value_class:
      cache_token(cache, tok_is_value_class);
      break;
    case ifc_sks_msvc_is_simple_value_class:
      cache_token(cache, tok_is_simple_value_class);
      break;
    case ifc_sks_msvc_is_interface_class:
      cache_token(cache, tok_is_interface_class);
      break;
    case ifc_sks_msvc_is_delegate:
      cache_token(cache, tok_is_delegate);
      break;
    case ifc_sks_msvc_is_final:
      cache_token(cache, tok_is_final);
      break;
    case ifc_sks_msvc_is_sealed:
      cache_token(cache, tok_is_sealed);
      break;
    case ifc_sks_msvc_has_finalizer:
      cache_token(cache, tok_has_finalizer);
      break;
    case ifc_sks_msvc_has_copy:
      cache_token(cache, tok_has_copy);
      break;
    case ifc_sks_msvc_has_assign:
      cache_token(cache, tok_has_assign);
      break;
    case ifc_sks_msvc_has_user_destructor:
      cache_token(cache, tok_has_user_destructor);
      break;
    case ifc_sks_msvc_pack_cardinality:
      cache_token(cache, tok_sizeof);
      cache_token(cache, tok_ellipsis);
      break;
    case ifc_sks_msvc_confused_sizeof:
      cache_token(cache, tok_sizeof);
      break;
    case ifc_sks_msvc_confused_alignas:
      cache_token(cache, tok_alignas);
      break;
    default_is_unexpected_str("Unknown SourceKeyword");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad source keyword cache");
  cache->invalidate();
done:;
}  /* cache_source_keyword */


void an_ifc_module::cache_source_identifier(
                                 a_module_token_cache_ptr                cache,
                                 const an_ifc_source_identifier_category &id)
/*
Add tokens corresponding to id to cache.  index is the index into the IFC file
for the additional information needed, depending on the kind of id.
*/
{
  a_const_char *name = NULL;

  switch (id.sort) {
    case ifc_sis_msvc:
      { a_string err_msg("Unexpected ", str_for(id.sort));

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_sis_plain:
      name = get_string_at_offset(id.variant.plain);
      break;
    case ifc_sis_msvc_builtin_huge_val:
      name = "__builtin_huge_val";
      break;
    case ifc_sis_msvc_builtin_huge_valf:
      name = "__builtin_huge_valf";
      break;
    case ifc_sis_msvc_builtin_nan:
      name = "__builtin_nan";
      break;
    case ifc_sis_msvc_builtin_nanf:
      name = "__builtin_nanf";
      break;
    case ifc_sis_msvc_builtin_nans:
      name = "__builtin_nans";
      break;
    case ifc_sis_msvc_builtin_nansf:
      name = "__builtin_nansf";
      break;
    default_is_unexpected_str("Unknown SourceIdentifier");
  }  /* switch */
  check_assertion(name != NULL);
  /* Microsoft treats "default" as an identifier rather than a keyword.  That
     is also the case in the token sequences recorded in IFC files. */
  if (strcmp(name, "default") == 0) {
    cache_token(cache, tok_default);
  } else {
    cache_identifier(cache, name);
  }  /* if */
  goto done;
invalid:
  expect_error_str("expected errors for bad source identifier cache");
  cache->invalidate();
done:;
}  /* cache_source_identifier */


void an_ifc_module::cache_string(a_module_token_cache_ptr     cache,
                                 an_ifc_string_index          string)
/*
Add a string literal (with the appropriate character kind) corresponding to
string to cache.
*/
{
  a_character_kind  kind;

  switch (string.sort) {
    case ifc_ss_ordinary:
      kind = (a_character_kind)chk_char;
      break;
    case ifc_ss_utf8:
      kind = (a_character_kind)chk_char8_t;
      break;
    case ifc_ss_char16:
      kind = (a_character_kind)chk_char16_t;
      break;
    case ifc_ss_char32:
      kind = (a_character_kind)chk_char32_t;
      break;
    case ifc_ss_wide:
      kind = (a_character_kind)chk_wchar_t;
      break;
    default_is_unexpected_str("Unexpected StringSort");
  }  /* switch */

  Opt<an_ifc_const_str>       opt_ics;
  an_ifc_partition_kind_index string_part_idx{string.mod, ifc_pk_const_str,
                                              string.value};
  construct_node(&opt_ics, string_part_idx);
  if (opt_ics.has_value()) {
    an_ifc_const_str   ics = *opt_ics;
    an_ifc_text_offset start = get_ifc_start(ics);
    an_ifc_cardinality length = get_ifc_length(ics);
    an_ifc_text_offset suffix = get_ifc_suffix(ics);

    if (suffix == 0) {
      cache_string_literal(cache, kind, get_string_at_offset(start), length);
    } else {
      cache_ud_literal(cache, kind, get_string_at_offset(start), length,
                       get_string_at_offset(suffix));
    }  /* if */
  }  /* if */
}  /* cache_string */


template<typename an_ifc_Word_type>
static void cache_word(an_ifc_module            *mod,
                       a_module_token_cache_ptr cache,
                       const an_ifc_Word_type   &word)
/*
Add the token(s) corresponding to word to cache.
*/
{
  an_ifc_source_location      locus = get_ifc_locus(word);
  an_ifc_source_position_hint pos_hint(cache, locus);
  an_ifc_word_category        category = get_ifc_category(word);

  switch (category.sort) {
    case ifc_ws_unknown:
      break;
    case ifc_ws_source_directive:
      mod->cache_source_directive(cache, category.variant.source_directive);
      break;
    case ifc_ws_source_punctuator:
      mod->cache_source_punctuator(cache, category.variant.source_punctuator);
      break;
    case ifc_ws_source_literal:
      mod->cache_source_literal(cache, category.variant.source_literal);
      break;
    case ifc_ws_source_operator:
      mod->cache_source_operator(cache, category.variant.source_operator);
      break;
    case ifc_ws_source_keyword:
      mod->cache_source_keyword(cache, category.variant.source_keyword);
      break;
    case ifc_ws_source_identifier:
      mod->cache_source_identifier(cache, category.variant.source_identifier);
      break;
    default_is_unexpected_str("Unknown WordSort");
  }  /* switch */
}  /* cache_word */


void an_ifc_module::cache_word(a_module_token_cache_ptr cache,
                               const an_ifc_source_word &word)
/*
Add token(s) corresponding to word to cache.
*/
{
  EDG_PREFIX::cache_word(this, cache, word);
}  /* cache_word */


void an_ifc_module::cache_word(a_module_token_cache_ptr   cache,
                               const an_ifc_nestable_word &word)
/*
Add token(s) corresponding to word to cache.
*/
{
  EDG_PREFIX::cache_word(this, cache, word);
}  /* cache_word */


uint32_t an_ifc_module::cache_sentence(
                                  a_module_token_cache_ptr cache,
                                  an_ifc_sentence_index    sentence,
                                  uint32_t                 offset,
                                  a_boolean                look_for_stop_token)
/*
Given a SentenceIndex, populate cache with the corresponding tokens.  offset is
the offset into the words to start caching.  If look_for_stop_token is TRUE
and a cached token matches a stop token, remove that token from the cache and
return the index of that token.  Otherwise the return value is meaningless.
*/
{
  an_ifc_cardinality_storage idx = offset;

  if (sentence != 0) {
    Opt<an_ifc_source_sentence> opt_iss;
    an_ifc_partition_kind_index sen_idx{sentence.mod, ifc_pk_src_sentence,
                                        sentence - 1};

    construct_node(&opt_iss, sen_idx);
    if (!opt_iss.has_value()) {
      goto invalid;
    }  /* if */

    an_ifc_source_sentence  iss = *opt_iss;
    a_source_word_traverser traverser(iss, idx);
    for (an_Indexed<an_ifc_source_word> indexed_isw : traverser) {
      if (!indexed_isw.has_value()) {
        goto invalid;
      }  /* if */

      a_cached_token_ptr ctp = cache->get_last_token();
      this->cache_word(cache, *indexed_isw);
      if (look_for_stop_token && ctp != cache->get_last_token() &&
          curr_stop_token_stack_entry->
                       stop_tokens[(int)cache->get_last_token()->token] != 0) {
        remove_token_from_cache(cache->get_last_token(), &ctp,
                                cache->as_canonical());
        break;
      }  /* if */
      ++idx;
    }  /* for */
  }  /* if */
  goto done;
invalid:
  expect_error_str("expected errors for bad sentence cache");
  cache->invalidate();
done:
  return idx;
}  /* cache_sentence */


/* FIXME: Codegen this? */
static a_boolean is_msvc_punctuator(an_ifc_source_punctuator_sort punctuator)
/*
Given a punctuator, return TRUE if the punctuator is MSVC specific; otherwise,
return FALSE.
*/
{
  a_boolean result;

  switch (punctuator) {
    case ifc_sps_colon:
    case ifc_sps_colon_colon:
    case ifc_sps_left_brace:
    case ifc_sps_left_bracket:
    case ifc_sps_left_parenthesis:
    case ifc_sps_question:
    case ifc_sps_right_brace:
    case ifc_sps_right_bracket:
    case ifc_sps_right_parenthesis:
    case ifc_sps_semicolon:
    case ifc_sps_unknown:
      result = FALSE;
      break;
    case ifc_sps_msvc:
    case ifc_sps_msvc_alignas_edict_start:
    case ifc_sps_msvc_default_argument_start:
    case ifc_sps_msvc_default_init_start:
    case ifc_sps_msvc_end_of_phrase:
    case ifc_sps_msvc_full_stop:
    case ifc_sps_msvc_nested_template_start:
    case ifc_sps_msvc_zero_width_space:
      result = TRUE;
      break;
    default_is_unexpected_str("Unexpected SourcePunctuator");
  }  /* switch */
  return result;
}  /* is_msvc_punctuator */


a_boolean an_ifc_module::sentence_is_deleted(an_ifc_sentence_index sentence)
/*
Given a SentenceIndex, return TRUE if it represents the sequence "= delete".
Otherwise, return FALSE.
*/
{
  a_boolean result = FALSE, equal_seen = FALSE;

  if (sentence != 0) {
    Opt<an_ifc_source_sentence> opt_iss;
    an_ifc_partition_kind_index sen_idx{sentence.mod, ifc_pk_src_sentence,
                                        sentence - 1};

    construct_node(&opt_iss, sen_idx);
    if (!opt_iss.has_value()) {
      goto done;
    }  /* if */

    an_ifc_source_sentence  iss = *opt_iss;
    a_source_word_traverser traverser(iss);
    for (an_Indexed<an_ifc_source_word> indexed_isw : traverser) {
      if (!indexed_isw.has_value()) {
        goto done;
      }  /* if */

      an_ifc_source_word   isw = *indexed_isw;
      an_ifc_word_category category = get_ifc_category(isw);
      if (category.sort == ifc_ws_source_directive ||
          (category.sort == ifc_ws_source_punctuator &&
           is_msvc_punctuator(category.variant.source_punctuator))) {
        /* Ignore MSVC-specific insertions. */
        continue;
      } else if (!equal_seen &&
                 category.sort == ifc_ws_source_operator &&
                 category.variant.source_operator == ifc_sos_equal) {
        equal_seen = TRUE;
      } else if (equal_seen &&
                 category.sort == ifc_ws_source_keyword &&
                 category.variant.source_keyword == ifc_sks_delete) {
        result = TRUE;
        break;
      } else {
        break;
      }  /* if */
    }  /* for */
  }  /* if */
done:
  return result;
}  /* sentence_is_deleted */


static void cache_object_traits(a_module_token_cache_ptr      cache,
                                an_ifc_object_traits_bitfield traits)
/*
Add the tokens corresponding to the given object traits to cache.
*/
{
  if (test_bitmask<ifc_otb_mutable>(traits)) {
    cache_token(cache, tok_mutable);
  }  /* if */
  if (test_bitmask<ifc_otb_inline>(traits)) {
    cache_token(cache, tok_inline);
  }  /* if */
  if (test_bitmask<ifc_otb_constexpr>(traits)) {
    cache_token(cache, tok_constexpr);
  }  /* if */
  if (test_bitmask<ifc_otb_thread_local>(traits)) {
    cache_token(cache, tok_thread_local);
  }  /* if */
}  /* cache_object_traits */


template<typename an_ifc_Node_type>
static void cache_func_type_cv_qualifiers(a_module_token_cache_ptr     cache,
                                          const an_ifc_Node_type       &node)
/*
Cache the cv-qualifiers for the given function-like IFC type.
*/
{
  an_ifc_function_type_traits_bitfield traits = get_ifc_traits(node);

  if (test_bitmask<ifc_fttb_const>(traits)) {
    cache_token(cache, tok_const);
  }  /* if */
  if (test_bitmask<ifc_fttb_volatile>(traits)) {
    cache_token(cache, tok_volatile);
  }  /* if */
}  /* cache_func_type_cv_qualifiers */


template<typename an_ifc_Node_type>
static void cache_func_type_ref_qualifier(a_module_token_cache_ptr     cache,
                                          const an_ifc_Node_type       &type)
/*
Cache the ref-qualifier for the given function-like IFC type.
*/
{
  an_ifc_function_type_traits_bitfield traits = get_ifc_traits(type);

  if (test_bitmask<ifc_fttb_lvalue>(traits)) {
    cache_token(cache, tok_ampersand);
  } else if (test_bitmask<ifc_fttb_rvalue>(traits)) {
    cache_token(cache, tok_and_and);
  }  /* if */
}  /* cache_func_type_ref_qualifier */


static void cache_noexcept_specifier(
                                  a_module_token_cache_ptr            cache,
                                  const an_ifc_noexcept_specification &eh_spec)
/*
Cache the noexcept-specifier for the given noexcept specification.
*/
{
  an_ifc_noexcept_sort sort = get_ifc_sort(eh_spec);

  if (sort != ifc_ns_none && sort != ifc_ns_inferred) {
    cache_token(cache, tok_noexcept);
    cache_token(cache, tok_lparen);
    switch (sort) {
      case ifc_ns_none:
      case ifc_ns_inferred:
        /* Unreachable, but place here to have all enums covered. */
        { a_string err_msg("Unexpected ", str_for(sort));

          ifc_unexpected(eh_spec.get_module(), err_msg);
        }
        goto invalid;
      case ifc_ns_false:
        cache_bool_literal(cache, false);
        break;
      case ifc_ns_true:
        cache_bool_literal(cache, true);
        break;
      case ifc_ns_expression:
        { an_ifc_sentence_index word_idx = get_ifc_words(eh_spec);

          word_idx.mod->cache_sentence(cache, word_idx);
        }
        break;
      case ifc_ns_unenforced:
        /* FIXME: Currently unsupported. */
        issue_unsupported_construct_error(eh_spec.get_module(),
                                          "NoexceptSort::Unenforced",
                                          &error_position);
        goto invalid;
      default_is_unexpected_str("Unexpected NoexceptSpecification");
    }  /* switch */
    cache_token(cache, tok_rparen);
  }  /* if */
  goto done;
invalid:
  expect_error_str("expected errors for bad noexcept-specifier cache");
  cache->invalidate();
done:;
}  /* cache_noexcept_specifier */


template<typename an_ifc_Node_type>
static void cache_func_type_noexcept_specifier(
                                           a_module_token_cache_ptr     cache,
                                           const an_ifc_Node_type       &type)
/*
Cache the noexcept-specifier for the given function-like type.
*/
{
  an_ifc_noexcept_specification eh_spec = get_ifc_eh_spec(type);

  cache_noexcept_specifier(cache, eh_spec);
}  /* cache_func_type_noexcept_specifier */


static void cache_calling_convention(a_module_token_cache_ptr       cache,
                                     an_ifc_calling_convention_sort convention)
/*
Cache a token representing the calling convention for the given calling
convention sort value.
*/
{
  switch (convention) {
    case ifc_ccs_cdecl:
      cache_token(cache, tok_cdecl);
      break;
    case ifc_ccs_fast:
      cache_token(cache, tok_fastcall);
      break;
    case ifc_ccs_std:
      cache_token(cache, tok_stdcall);
      break;
    case ifc_ccs_this:
      cache_token(cache, tok_thiscall);
      break;
    case ifc_ccs_clr:
      cache_token(cache, tok_clrcall);
      break;
    case ifc_ccs_vector:
      cache_token(cache, tok_vectorcall);
      break;
    case ifc_ccs_eabi:
      pos_st_diagnostic(es_discretionary_error,
                        ec_ifc_no_corresponding_calling_conv,
                        &error_position, "CallingConvention::Eabi");
      break;
    default_is_unexpected_str("Unexpected CallingConvention");
  }  /* switch */
}  /* cache_calling_convention */


template<typename an_ifc_Node_type>
static void cache_func_type_calling_convention(
                                           a_module_token_cache_ptr     cache,
                                           const an_ifc_Node_type       &type)
/*
Cache a token representing the calling convention for the given function-like
type.
*/
{
  an_ifc_calling_convention_sort convention = get_ifc_convention(type);

  cache_calling_convention(cache, convention);
}  /* cache_func_type_calling_convention */


template<typename an_ifc_Node_type>
static void cache_func_type_return_type(a_module_token_cache_ptr     cache,
                                        const an_ifc_Node_type       &type)
/*
Cache the return type declarator for the given function-like type.
*/
{
  an_ifc_type_index return_type = get_ifc_target(type);

  return_type.mod->cache_type(cache, return_type);
}  /* cache_func_type_return_type */


template<typename an_ifc_Node_type>
static void cache_func_type_parameter_declaration_clause(
                                           a_module_token_cache_ptr     cache,
                                           const an_ifc_Node_type       &type)
/*
Cache the parameter-declaration-clause for the given function-like type.
*/
{
  an_ifc_type_index source_params = get_ifc_source(type);

  if (!is_null_index(source_params)) {
    source_params.mod->cache_type(cache, source_params);
  }  /* if */
}  /* cache_func_type_parameter_declaration_clause */


template<typename an_ifc_Node_type>
static void cache_func_cv_qualifiers(a_module_token_cache_ptr cache,
                                     const an_ifc_Node_type   &decl)
/*
Cache the cv-qualifiers for the given function-like declaration.
*/
{
  an_ifc_type_index func_type_idx = get_ifc_type(decl);

  switch (func_type_idx.sort) {
    case ifc_ts_type_function:
      { Opt<an_ifc_type_function> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_function func_type = *opt_func_type;
        cache_func_type_cv_qualifiers(cache, func_type);
      }
      break;
    case ifc_ts_type_method:
      { Opt<an_ifc_type_method> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_method func_type = *opt_func_type;
        cache_func_type_cv_qualifiers(cache, func_type);
      }
      break;
    case ifc_ts_type_tor:
      /* Constructor/destructor types do not have cv-qualifiers. */
      break;
    default:
      { a_string err_msg("Unexpected ", str_for(func_type_idx.sort));

        ifc_unexpected(func_type_idx.mod, err_msg);
      }
      break;
  }  /* if */
  goto done;
invalid:
  expect_error_str("expected errors for bad cv-qualifiers cache");
  cache->invalidate();
done:;
}  /* cache_func_cv_qualifiers */


template<typename an_ifc_Node_type>
static void cache_func_ref_qualifier(a_module_token_cache_ptr cache,
                                     const an_ifc_Node_type   &decl)
/*
Cache the ref-qualifier for the given function-like declaration.
*/
{
  an_ifc_type_index func_type_idx = get_ifc_type(decl);

  switch (func_type_idx.sort) {
    case ifc_ts_type_function:
      { Opt<an_ifc_type_function> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_function func_type = *opt_func_type;
        cache_func_type_ref_qualifier(cache, func_type);
      }
      break;
    case ifc_ts_type_method:
      { Opt<an_ifc_type_method> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_method func_type = *opt_func_type;
        cache_func_type_ref_qualifier(cache, func_type);
      }
      break;
    case ifc_ts_type_tor:
      /* Constructor/destructor types do not have ref-qualifiers. */
      break;
    default:
      { a_string err_msg("Unexpected ", str_for(func_type_idx.sort));

        ifc_unexpected(func_type_idx.mod, err_msg);
      }
      break;
  }  /* if */
  goto done;
invalid:
  expect_error_str("expected errors for bad ref-qualifier cache");
  cache->invalidate();
done:;
}  /* cache_func_ref_qualifier */


template<typename an_ifc_Node_type>
static void cache_func_noexcept_specifier(a_module_token_cache_ptr cache,
                                          const an_ifc_Node_type   &decl)
/*
Cache the noexcept-specifier for the given function-like declaration.
*/
{
  an_ifc_type_index func_type_idx = get_ifc_type(decl);

  switch (func_type_idx.sort) {
    case ifc_ts_type_function:
      { Opt<an_ifc_type_function> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_function func_type = *opt_func_type;
        cache_func_type_noexcept_specifier(cache, func_type);
      }
      break;
    case ifc_ts_type_method:
      { Opt<an_ifc_type_method> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_method func_type = *opt_func_type;
        cache_func_type_noexcept_specifier(cache, func_type);
      }
      break;
    case ifc_ts_type_tor:
      { Opt<an_ifc_type_tor> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_tor func_type = *opt_func_type;
        cache_func_type_noexcept_specifier(cache, func_type);
      }
      break;
    default:
      { a_string err_msg("Unexpected ", str_for(func_type_idx.sort));

        ifc_unexpected(func_type_idx.mod, err_msg);
      }
      break;
  }  /* if */
  goto done;
invalid:
  expect_error_str("expected errors for bad noexcept-specifier cache");
  cache->invalidate();
done:;
}  /* cache_func_noexcept_specifier */


template<>
void cache_func_noexcept_specifier(a_module_token_cache_ptr     cache,
                                   const an_ifc_decl_destructor &decl)
/*
Cache the noexcept-specifier for the given destructor.
*/
{
  an_ifc_noexcept_specification eh_spec = get_ifc_eh_spec(decl);

  cache_noexcept_specifier(cache, eh_spec);
}  /* cache_func_noexcept_specifier */


static void cache_func_vendor_decl_specifier_seq(
                                             a_module_token_cache_ptr cache,
                                             an_ifc_decl_index        decl_idx)
/*
Cache the vendor-specific portion of the decl-specifier-seq for the
function-like declaration at the given declaration index.
*/
{
  an_ifc_module               *mod = decl_idx.mod;
  an_ifc_msvc_traits_bitfield msvc_traits = mod->get_vendor_traits(decl_idx);

  if (test_bitmask<ifc_mtb_force_inline>(msvc_traits)) {
    cache_token(cache, tok_forceinline);
  }  /* if */

  auto cache_declspec_fn = [&cache](a_const_char *str) {
    cache_token(cache, tok_declspec);
    cache_token(cache, tok_lparen);
    cache_identifier(cache, str);
    cache_token(cache, tok_rparen);
  };  /* cache_declspec_fn */
  if (test_bitmask<ifc_mtb_naked>(msvc_traits)) {
    cache_declspec_fn("naked");
  }  /* if */
  if (test_bitmask<ifc_mtb_no_alias>(msvc_traits)) {
    cache_declspec_fn("noalias");
  }  /* if */
  if (test_bitmask<ifc_mtb_no_inline>(msvc_traits)) {
    cache_declspec_fn("noinline");
  }  /* if */
  if (test_bitmask<ifc_mtb_restrict>(msvc_traits)) {
    cache_declspec_fn("restrict");
  }  /* if */
  if (test_bitmask<ifc_mtb_safe_buffers>(msvc_traits)) {
    cache_declspec_fn("safebuffers");
  }  /* if */
  if (test_bitmask<ifc_mtb_dll_export>(msvc_traits)) {
    cache_declspec_fn("dllexport");
  }  /* if */
  if (test_bitmask<ifc_mtb_dll_import>(msvc_traits)) {
    cache_declspec_fn("dllimport");
  }  /* if */
  if (test_bitmask<ifc_mtb_novtable>(msvc_traits)) {
    cache_declspec_fn("novtable");
  }  /* if */
  if (test_bitmask<ifc_mtb_process>(msvc_traits)) {
    cache_declspec_fn("process");
  }  /* if */
  if (test_bitmask<ifc_mtb_select_any>(msvc_traits)) {
    cache_declspec_fn("selectany");
  }  /* if */
}  /* cache_func_vendor_decl_specifier_seq */


template<typename an_ifc_Node_type>
static void cache_func_decl_specifier_seq(a_module_token_cache_ptr cache,
                                          const an_ifc_Node_type   &decl)
/*
Cache the non-vendor specific part of the decl-specifier-seq for the given
function-like declaration.
*/
{
  an_ifc_function_traits_bitfield func_traits = get_ifc_traits(decl);

  if (test_bitmask<ifc_ftb_virtual>(func_traits)) {
    cache_token(cache, tok_virtual);
  }  /* if */
  if (test_bitmask<ifc_ftb_explicit>(func_traits)) {
    cache_token(cache, tok_explicit);
  }  /* if */
  if (test_bitmask<ifc_ftb_no_return>(func_traits)) {
    cache_token(cache, tok_noreturn);
  }  /* if */
  if (test_bitmask<ifc_ftb_immediate>(func_traits)) {
    cache_token(cache, tok_consteval);
  } else if (test_bitmask<ifc_ftb_constexpr>(func_traits)) {
    cache_token(cache, tok_constexpr);
  }  /* if */
}  /* cache_func_decl_specifier_seq */


template<typename an_ifc_Node_type>
static void cache_func_virt_specifier_seq(a_module_token_cache_ptr cache,
                                          const an_ifc_Node_type   &decl)

/*
Cache the virt-specifier-seq for the given function-like declaration.
*/
{
  an_ifc_function_traits_bitfield traits = get_ifc_traits(decl);

  if (test_bitmask<ifc_ftb_pure_virtual>(traits)) {
    a_constant_ptr cp = alloc_cached_constant();

    cache_token(cache, tok_assign);
    make_zero_of_proper_type(integer_type((an_integer_kind)ik_int), cp);
    cache_literal(decl.get_module(), cache, cp);
  }  /* if */
}  /* cache_func_virt_specifier_seq */


template<typename an_ifc_Node_type>
static void cache_func_body(a_module_token_cache_ptr cache,
                            an_ifc_decl_index        decl_idx,
                            const an_ifc_Node_type   &decl,
                            const an_ifc_cache_info  &cinfo)

/*
Cache the function-body for the given function-like declaration (identified by
decl_idx).

If the IFC provides a user-defined definition for said function, instead of a
proper function-body being cached, a token indicating the presence of a
lazy-loadable definition will instead be cached.  cinfo contains information
about the current cache context to help inform decisions about what to cache.
*/
{
  check_assertion(function_is_defined(decl));
  an_ifc_function_traits_bitfield traits = get_ifc_traits(decl);

  if (function_is_user_defined(decl)) {
    cache_token(cache, tok_semicolon);
    /* A body is likely available: Record this availability using a
       pseudo-token that will be translated when the declaration is
       parsed. */
    cache_token_with_index(cache, tok_pending_ifc_func_body, decl_idx);
    /* FIXME: We cache a new semicolon here to avoid complicating
       full_specialization logic (which does not provide an easy way to
       suppress the semicolon check), it might be better to rework this logic
       to "insert" the tok_pending_ifc_func_body before the existing
       semicolon. */
    if (!cinfo.no_final_semicolon) {
      cache_token(cache, tok_semicolon);
    }  /* if */
  } else {
    if (test_bitmask<ifc_ftb_defaulted>(traits)) {
      cache_token(cache, tok_assign);
      cache_token(cache, tok_default);
      if (!cinfo.no_final_semicolon) {
        cache_token(cache, tok_semicolon);
      }  /* if */
    } else if (test_bitmask<ifc_ftb_deleted>(traits)) {
      cache_token(cache, tok_assign);
      cache_token(cache, tok_delete);
      if (!cinfo.no_final_semicolon) {
        cache_token(cache, tok_semicolon);
      }  /* if */
    } else {
      /* If this condition was violated, we're not correctly handling a case of
         the function-body grammar. */
      unexpected_condition();
    }  /* if */
  }  /* if */
}  /* cache_func_body */


template<typename an_ifc_Node_type>
static void cache_func_body_or_end_decl(a_module_token_cache_ptr cache,
                                        an_ifc_decl_index        decl_idx,
                                        const an_ifc_Node_type   &decl,
                                        const an_ifc_cache_info  &cinfo)
/*
Cache the function-body for the given function-like declaration (identified by
decl_idx) if its definition is specified; otherwise, cache a semicolon to
terminate the declaration.  cinfo contains information about the current cache
context to help inform decisions about what to cache.
*/
{
  if (!cinfo.ignore_definition && function_is_defined(decl)) {
    cache_func_body(cache, decl_idx, decl, cinfo);
  } else if (!cinfo.no_final_semicolon) {
    cache_token(cache, tok_semicolon);
  }  /* if */
}  /* cache_func_body_or_end_decl */


template<typename an_ifc_Node_type>
static void cache_func_parameters_and_qualifiers(
                                             a_module_token_cache_ptr cache,
                                             an_ifc_decl_index        decl_idx,
                                             const an_ifc_Node_type   &decl,
                                             const an_ifc_cache_info  &cinfo)
/*
Cache the parameters-and-qualifiers for the given function-like declaration
(indexed by decl_idx).  cinfo contains information about the current cache
context to help inform decisions about what to cache.
*/
{
  cache_token(cache, tok_lparen);
  cache_func_parameter_declaration_clause(cache, decl_idx, decl, cinfo);
  cache_token(cache, tok_rparen);
  cache_func_cv_qualifiers(cache, decl);
  cache_func_ref_qualifier(cache, decl);
  cache_func_noexcept_specifier(cache, decl);
}  /* cache_func_parameters_and_qualifiers */


template<typename an_ifc_Node_type>
static void cache_func_calling_convention(a_module_token_cache_ptr cache,
                                          const an_ifc_Node_type   &decl)
/*
Cache a token representing the calling convention for the given function-like
declaration.
*/
{
  an_ifc_type_index func_type_idx = get_ifc_type(decl);

  switch (func_type_idx.sort) {
    case ifc_ts_type_function:
      { Opt<an_ifc_type_function> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_function func_type = *opt_func_type;
        cache_func_type_calling_convention(cache, func_type);
      }
      break;
    case ifc_ts_type_method:
      { Opt<an_ifc_type_method> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_method func_type = *opt_func_type;
        cache_func_type_calling_convention(cache, func_type);
      }
      break;
    case ifc_ts_type_tor:
      { Opt<an_ifc_type_tor> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_tor func_type = *opt_func_type;
        cache_func_type_calling_convention(cache, func_type);
      }
      break;
    default:
      { a_string err_msg("Unexpected ", str_for(func_type_idx.sort));

        ifc_unexpected(func_type_idx.mod, err_msg);
      }
      break;
  }  /* if */
  goto done;
invalid:
  expect_error_str("expected errors for bad calling convention cache");
  cache->invalidate();
done:;
}  /* cache_func_calling_convention */


template<>
void cache_func_calling_convention(a_module_token_cache_ptr     cache,
                                   const an_ifc_decl_destructor &decl)
/*
Cache a token representing the calling convention for the given destructor.
*/
{
  an_ifc_calling_convention_sort convention = get_ifc_convention(decl);

  cache_calling_convention(cache, convention);
}  /* cache_func_calling_convention */


template<typename an_ifc_Node_type>
static void cache_func_return_type(a_module_token_cache_ptr cache,
                                   const an_ifc_Node_type   &decl)
/*
Cache the return type declarator for the given function-like declaration.
*/
{
  an_ifc_type_index func_type_idx = get_ifc_type(decl);

  switch (func_type_idx.sort) {
    case ifc_ts_type_function:
      { Opt<an_ifc_type_function> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_function func_type = *opt_func_type;
        cache_func_type_return_type(cache, func_type);
      }
      break;
    case ifc_ts_type_method:
      { Opt<an_ifc_type_method> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_method func_type = *opt_func_type;
        cache_func_type_return_type(cache, func_type);
      }
      break;
    default:
      { a_string err_msg("Unexpected ", str_for(func_type_idx.sort));

        ifc_unexpected(func_type_idx.mod, err_msg);
      }
      break;
  }  /* if */
  goto done;
invalid:
  expect_error_str("expected errors for bad return type declarator cache");
  cache->invalidate();
done:;
}  /* cache_func_return_type */


template<typename an_ifc_Node_type>
static an_ifc_type_index get_func_param_type(const an_ifc_Node_type &decl)
/*
Return the IFC type index corresponding to the parameters of the given
function-like declaration.  Failure to load the parameter type will result
in a null IFC type index.

FIXME: A null IFC type index is also a valid result representing no parameters.
This isn't an issue for current uses of this function, but could become an
issue.  A little more thought needs put into an_ifc_func_param_context.
*/
{
  an_ifc_type_index result = {};
  an_ifc_type_index func_type_idx = get_ifc_type(decl);

  switch (func_type_idx.sort) {
    case ifc_ts_type_function:
      { Opt<an_ifc_type_function> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_function func_type = *opt_func_type;
        result = get_ifc_source(func_type);
      }
      break;
    case ifc_ts_type_method:
      { Opt<an_ifc_type_method> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_method func_type = *opt_func_type;
        result = get_ifc_source(func_type);
      }
      break;
    case ifc_ts_type_tor:
      { Opt<an_ifc_type_tor> opt_func_type;

        construct_node(&opt_func_type, func_type_idx);
        if (!opt_func_type.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_tor func_type = *opt_func_type;
        result = get_ifc_source(func_type);
      }
      break;
    default:
      { a_string err_msg("Unexpected ", str_for(func_type_idx.sort));

        ifc_unexpected(func_type_idx.mod, err_msg);
      }
      break;
  }  /* if */
  goto done;
invalid:
  expect_error_str("expected errors for bad function parameter type query");
done:
  return result;
}  /* get_func_param_type */

namespace {

/*
This class implements a lazily-loadable function parameter chart abstraction
(i.e., the chart remains unloaded until information about a parameter is
requested).
*/
struct a_lazy_ifc_func_param_chart {
  a_lazy_ifc_func_param_chart(an_ifc_chart_index chart_idx_val)
    : chart_idx(chart_idx_val), opt_node{}, node_invalid(FALSE)
    {}
  inline Opt<an_ifc_decl_parameter> get(an_ifc_index_type param_idx);
private:
  inline a_boolean try_load_chart();
  an_ifc_chart_index
                chart_idx;
                        /* The index for the function parameter chart. */
  Opt<an_ifc_chart_unilevel>
                opt_node;
                        /* The (potentially unloaded) underlying parameter
                           chart corresponding to chart_idx. */
  a_boolean     node_invalid;
                        /* TRUE if loading of opt_node was attempted but
                           ultimately failed. */
};  /* a_lazy_ifc_func_param_chart */


static an_ifc_chart_index get_msvc_trait_func_param_chart_idx(
                                                    an_ifc_decl_index decl_idx)
/*
Given a declaration index for a function-like declaration, return the chart
index from the MSVC function parameters trait describing the function's
parameters; otherwise, return a null chart index.
*/
{
  an_ifc_chart_index                 result = {};
  Opt<an_ifc_trait_msvc_func_params> opt_itmfp;

  find_trait(&opt_itmfp, decl_idx);
  if (opt_itmfp.has_value()) {
    an_ifc_trait_msvc_func_params itmfp = *opt_itmfp;

    result = get_ifc_params(itmfp);
  }  /* if */
  return result;
}  /* get_msvc_trait_func_param_chart_idx */


static an_ifc_chart_index get_func_defition_param_chart_idx(
                                                    an_ifc_decl_index decl_idx)
/*
Given a declaration index for a function-like declaration, return the chart
index from the function definition trait describing the function's parameters;
otherwise, return a null chart index.
*/
{
  an_ifc_chart_index                    result = {};
  Opt<an_ifc_trait_function_definition> opt_itfd;

  find_trait(&opt_itfd, decl_idx);
  if (opt_itfd.has_value()) {
    an_ifc_trait_function_definition itfd = *opt_itfd;

    result = get_ifc_parameters(itfd);
  }  /* if */
  return result;
}  /* get_func_defition_param_chart_idx */


Opt<an_ifc_decl_parameter>
a_lazy_ifc_func_param_chart::get(an_ifc_index_type param_idx)
/*
Attempt to load the chart (if not already loaded).  Then, using the given
relative index for the parameter, attempt to resolve and return the associated
IFC decl parameter (if possible); otherwise, return an empty optional.
*/
{
  Opt<an_ifc_decl_parameter> result = {};

  if (this->try_load_chart()) {
    an_ifc_chart_unilevel uni_chart = *opt_node;
    an_ifc_index          start_idx = get_ifc_start(uni_chart);
    an_ifc_cardinality    cardinality = get_ifc_cardinality(uni_chart);

    if (param_idx >= cardinality) {
      goto invalid;
    }  /* if */

    /* FIXME: This is a bit of an abuse of the traverser, but the traverser
       provides a much simplified validation scheme vs rolling out all the
       loading code for the element "manually".  We should have a better
       way to get a single element out of a heap. */
    an_ifc_index_type          element_idx = start_idx + param_idx;
    a_decl_parameter_traverser traverser(start_idx.mod, element_idx, 1);
    for (an_Indexed<an_ifc_decl_parameter> indexed_idp : traverser) {
      if (!indexed_idp.has_value()) {
        goto invalid;
      }  /* if */

      result = *indexed_idp;
      goto done;
    }  /* if */
  }  /* if */
  goto done;
invalid:
  result.clear();
done:
  return result;
}  /* a_lazy_ifc_func_param_chart::get */


a_boolean a_lazy_ifc_func_param_chart::try_load_chart()
/*
Attempt to load the node value using the associated chart index.  If the chart
is loaded successfully (or was already loaded), return TRUE; otherwise, return
FALSE.
*/
{
  if (!this->node_invalid) {
    if (is_null_index(this->chart_idx)) {
      /* This isn't strictly speaking invalid, it just means we don't have this
         chart; the invalid case prevents reentry if this function is called
         again. */
      goto invalid;
    }  /* if */

    an_ifc_chart_sort chart_sort = this->chart_idx.sort;
    if (chart_sort == ifc_cs_chart_none) {
      /* This isn't strictly speaking invalid, it just means the chart contains
         nothing; the invalid case prevents reentry if this function is called
         again. */
      goto invalid;
    }  /* if */
    if (chart_sort != ifc_cs_chart_unilevel) {
      /* The associated chart index was set to a "real" chart index, but it
         isn't a uni-level chart; so it's not a valid function parameter
         chart. */
      /* FIXME: We may want an error here. */
      goto invalid;
    }  /* if */
    construct_node(&this->opt_node, this->chart_idx);
    if (!this->opt_node.has_value()) {
      goto invalid;
    }  /* if */
  }  /* if */
  goto done;
invalid:
  this->node_invalid = TRUE;
done:
  return this->opt_node.has_value();
}  /* a_lazy_ifc_func_param_chart::try_load_chart */


/*
This class implements a "function parameter context" which abstracts away the
various locations the IFC stores parameter information to provide a consistent
view of the "best" information the IFC provides.

This type is implemented in terms of relative indexes vs something more like
the Sequence_traverser type to allow for maximum flexibility (given the current
IFC status-quo's potential for mismatched information).
*/
struct an_ifc_func_param_context {
  template<typename an_ifc_Node_type>
  inline an_ifc_func_param_context(an_ifc_decl_index      decl_idx,
                                   const an_ifc_Node_type &decl);
  an_ifc_index_type get_num_params() const
    { return num_params; }
  inline an_ifc_type_index get_param_type(an_ifc_index_type param_idx);
  inline an_ifc_expr_index get_default_arg_expr(an_ifc_index_type param_idx);
private:
  inline an_ifc_index_type determine_param_count();
  an_ifc_index_type
                num_params;
                        /* The determined number of parameters (see
                           determine_param_count for more information). */
  an_ifc_type_index
                params_type;
                        /* The type representing the function's parameters.
                           This is typically the IFC source field of the
                           function-like declaration's type. */
  a_lazy_ifc_func_param_chart
                decl_param_chart;
                        /* The function parameter chart associated directly
                           with this declaration.  This is typically the IFC
                           chart field of the function-like declaration. */
  a_lazy_ifc_func_param_chart
                def_param_chart;
                        /* The function parameter chart pulled from the
                           associated IFC function definition trait. */
  a_lazy_ifc_func_param_chart
                msvc_traits_param_chart;
                        /* The function parameter chart pulled from the
                           associated MSVC function parameters trait. */
};  /* an_ifc_func_param_context */

template<typename an_ifc_Node_type>
inline an_ifc_func_param_context::an_ifc_func_param_context(
                                               an_ifc_decl_index      decl_idx,
                                               const an_ifc_Node_type &decl)
/*
Given a function-like declaration (indexed by decl_idx) initialize its function
parameter context.
*/
  : num_params(0), params_type(get_func_param_type(decl)),
    decl_param_chart(get_ifc_chart(decl)),
    def_param_chart(get_func_defition_param_chart_idx(decl_idx)),
    msvc_traits_param_chart(get_msvc_trait_func_param_chart_idx(decl_idx))
{
  this->num_params = this->determine_param_count();
}  /* an_ifc_func_param_context:an_ifc_func_param_context */


an_ifc_index_type an_ifc_func_param_context::determine_param_count()
/*
Given the information known to this function parameter context, attempt to
determine and return the correct number of parameters.
*/
{
  an_ifc_index_type result = 0;

  if (!is_null_index(this->params_type)) {
    if (this->params_type.sort == ifc_ts_type_tuple) {
      Opt<an_ifc_type_tuple> opt_tuple_type;

      construct_node(&opt_tuple_type, this->params_type);
      if (opt_tuple_type.has_value()) {
        an_ifc_type_tuple tuple_type = *opt_tuple_type;

        result = get_ifc_cardinality(tuple_type);
      }  /* if */
    } else {
      result = 1;
    }  /* if */
  }  /* if */
  return result;
}  /* an_ifc_func_param_context::determine_param_count */


an_ifc_type_index
an_ifc_func_param_context::get_param_type(an_ifc_index_type param_idx)
/*
Given the relative index of a parameter for the current function parameter
context, return the type index for the parameter's type (if possible); if no
valid type can be found, return a null type index.
*/
{
  an_ifc_type_index result = {};

  if (!is_null_index(this->params_type)) {
    if (this->params_type.sort == ifc_ts_type_tuple) {
      Opt<an_ifc_type_tuple> opt_tuple_type;

      construct_node(&opt_tuple_type, this->params_type);
      if (opt_tuple_type.has_value()) {
        an_ifc_type_tuple  tuple_type = *opt_tuple_type;
        an_ifc_index       start_idx = get_ifc_start(tuple_type);
        an_ifc_cardinality cardinality = get_ifc_cardinality(tuple_type);

        if (param_idx >= cardinality) {
          goto invalid;
        }  /* if */

        /* FIXME: This is a bit of an abuse of the traverser, but the traverser
           provides a much simplified validation scheme vs rolling out all the
           loading code for the element "manually".  We should have a better
           way to get a single element out of a heap. */
        an_ifc_index_type     element_idx = start_idx + param_idx;
        a_type_heap_traverser traverser(start_idx.mod, element_idx, 1);
        for (an_Indexed<an_ifc_heap_type> indexed_iht : traverser) {
          if (!indexed_iht.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_heap_type heap_type = *indexed_iht;
          result = get_ifc_value(heap_type);
          goto done;
        }  /* for */
      }  /* if */
    } else {
      result = this->params_type;
      goto done;
    }  /* if */
  }  /* if */
  /* FIXME: Attempt other sources if we still don't have a type. */
  goto done;
invalid:
  result = {};
done:
  return result;
}  /* an_ifc_func_param_context::get_param_type */


static an_ifc_expr_index get_default_arg_from_chart(
                                         a_lazy_ifc_func_param_chart &chart,
                                         an_ifc_index_type           param_idx)
/*
Given a function parameter chart and the relative index of a parameter, return
the expr index for the parameter's default argument (if possible); if no valid
default argument expression can be found, return a null expr index.
*/
{
  an_ifc_expr_index          result = {};
  Opt<an_ifc_decl_parameter> opt_decl_param = chart.get(param_idx);

  if (opt_decl_param.has_value()) {
    an_ifc_decl_parameter decl_param = *opt_decl_param;

    result = get_ifc_initializer(decl_param);
  }  /* if */
  return result;
}  /* get_default_arg_from_chart */


an_ifc_expr_index
an_ifc_func_param_context::get_default_arg_expr(an_ifc_index_type param_idx)
/*
Given the relative index of a parameter for the current function parameter
context, return the expr index for the parameter's default argument (if
possible); if no valid default argument expression can be found, return a null
expr index.
*/
{
  an_ifc_expr_index result;

  /* FIXME: Is this the best order? */
  result = get_default_arg_from_chart(this->decl_param_chart, param_idx);
  if (!is_null_index(result)) {
    goto done;
  }  /* if */
  result = get_default_arg_from_chart(this->msvc_traits_param_chart,
                                      param_idx);
  if (!is_null_index(result)) {
    goto done;
  }  /* if */
  result = get_default_arg_from_chart(this->def_param_chart, param_idx);
  /* FIXME: Do we need to do any "quality" comparison on the possible
     results? */
done:
  return result;
}  /* an_ifc_func_param_context::get_param_type */

}  /* namespace */


template<typename an_ifc_Node_type>
static void cache_func_parameter_declaration_clause(
                                             a_module_token_cache_ptr cache,
                                             an_ifc_decl_index        decl_idx,
                                             const an_ifc_Node_type   &decl,
                                             const an_ifc_cache_info  &cinfo)
/*
Cache the parameter-declaration-clause for the given function-like declaration
(indexed by decl_idx).  cinfo contains information about the current cache
context to help inform decisions about what to cache.
*/
{
  an_ifc_func_param_context param_context(decl_idx, decl);
  a_boolean                 first = TRUE;

  for (an_ifc_index_type i = 0; i < param_context.get_num_params(); ++i) {
    if (!first) {
      cache_token(cache, tok_comma);
    }  /* if */

    an_ifc_type_index arg_type = param_context.get_param_type(i);
    /* If the type couldn't be resolved, fail; this isn't reasonably
       recoverable. */
    if (is_null_index(arg_type)) {
      goto invalid;
    }  /* if */
    arg_type.mod->cache_type(cache, arg_type);
    /* Cache the default argument if we're not ignoring default arguments in
       this context, and a default argument is found. */
    if (!cinfo.ignore_default_arguments) {
      an_ifc_expr_index arg_expr = param_context.get_default_arg_expr(i);

      if (!is_null_index(arg_expr)) {
        cache_token(cache, tok_assign);
        cache_token_with_index(cache, tok_pending_ifc_expr, arg_expr);
      }  /* if */
    }  /* if */
    first = FALSE;
  }  /* for */
  goto done;
invalid:
  expect_error_str("expected errors for bad parameter-declaration-clause"
                   " cache");
  cache->invalidate();
done:;
}  /* cache_func_parameter_declaration_clause */


void an_ifc_module::cache_scope_member_sequence(
                                           a_module_token_cache_ptr cache,
                                           an_ifc_decl_index        scope_decl,
                                           const an_ifc_sequence    &seq)
/*
Cache a sequence (seq) of IFC scope member declarations into the cache.
scope_decl specifies the current home scope declaration being processed prior
to this call -- so that we can determine if caching of a given declaration in
(seq) can be performed in the current scope or should be deferred.
*/
{
  /* Provide a consumer function that accepts a given scope member and caches
     the associated IFC declaration into the cache. */
  auto decl_consumer = [this, cache](const an_ifc_scope_member &ismp) {
    this->cache_decl(cache, get_ifc_index(ismp), /*cinfo=*/{});
  };

  /* Iterate over the sequence calling decl_consumer for each element. */
  traverse_scope_member_sequence(seq, decl_consumer);
}  /* cache_scope_member_sequence */


static void cache_basic_specifiers(a_module_token_cache_ptr         cache,
                                   an_ifc_basic_specifiers_bitfield specifiers)
/*
Add tokens corresponding to specifiers to cache.
*/
{
  if (test_bitmask<ifc_bsb_c>(specifiers)) {
    cache_token(cache, tok_extern);
    cache_string_literal(cache, chk_char, "C", 2);
  }  /* if */
  if (test_bitmask<ifc_bsb_deprecated>(specifiers)) {
    cache_token(cache, tok_lbracket);
    cache_token(cache, tok_lbracket);
    cache_identifier(cache, "deprecated");
    cache_token(cache, tok_rbracket);
    cache_token(cache, tok_rbracket);
  }  /* if */
}  /* cache_basic_specifiers */


static void cache_qualifiers(a_module_token_cache_ptr  cache,
                             an_ifc_qualifier_bitfield qualifiers)
/*
Add tokens corresponding to qualifiers to cache.
*/
{
  if (test_bitmask<ifc_qb_const>(qualifiers)) {
    cache_token(cache, tok_const);
  }  /* if */
  if (test_bitmask<ifc_qb_volatile>(qualifiers)) {
    cache_token(cache, tok_volatile);
  }  /* if */
  if (test_bitmask<ifc_qb_restrict>(qualifiers)) {
    cache_token(cache, tok_restrict);
  }  /* if */
}  /* cache_qualifiers */


template<typename a_Name_Cache_Fn, typename a_Scope_Cache_Fn>
inline void an_ifc_module::cache_scope_decl(
                                   a_module_token_cache_ptr     cache,
                                   an_ifc_decl_index            decl_idx,
                                   an_ifc_type_index            type,
                                   a_Name_Cache_Fn              cache_name_fn,
                                   a_Scope_Cache_Fn             cache_scope_fn)
/*
Cache the tokens corresponding to the given scope decl (indexed in the IFC by
decl_idx).  type represents the IFC type representing the introducing keyword.
cache_name_fn is a lambda that's called to cache the name of the scope.
cache_scope_fn is a lambda that's called to cache the scope's body (e.g., for a
class the member-specification).

FIXME: Remove this version of cache_scope_decl once names can be cached
properly for specializations using a NameIndex, and similarly the scope's body
can be consistently cached via a ScopeIndex.
*/
{
  check_assertion(type.sort == ifc_ts_type_fundamental);
  /* Cache the struct/class/union/namespace/__interface keyword. */
  cache_type(cache, type);
  /* Cache any attributes. */
  cache_attrs(cache, decl_idx);
  /* Cache the name. */
  cache_name_fn();
  /* Cache the scope's body. e.g., for a class the member-specification. */
  cache_scope_fn();
}  /* cache_scope_decl */


void an_ifc_module::cache_scope_decl(a_module_token_cache_ptr cache,
                                     an_ifc_decl_index        decl_idx,
                                     an_ifc_type_index        type,
                                     an_ifc_name_index        name,
                                     an_ifc_type_index        base,
                                     an_ifc_scope_index       scope)
/*
Cache the tokens corresponding to the given scope decl (indexed in the IFC by
decl_idx).  type represents the IFC type representing the introducing keyword.
name represents the name of the scope decl.  base represents any associated
base classes and as such is only valid for a class declaration.  scope
represents the declaration's body (e.g., for a class the member-specification).
*/
{
  auto cache_name_fn = [this, cache, name]() {
    cache_name(cache, name);
  };
  auto cache_scope_fn = [this, cache, base, type, scope]() {
    /* If there are bases specified, cache the bases. */
    if (!is_null_index(base)) {
      cache_token(cache, tok_colon);
      cache_type(cache, base);
    }  /* if */
    cache_scope(this, cache, scope);

    /* Read the fundamental type so that we can determine if we're caching a
       namespace. */
    Opt<an_ifc_type_fundamental> opt_itf;
    construct_node(&opt_itf, type);
    if (opt_itf.has_value()) {
      an_ifc_type_fundamental itf = *opt_itf;

      /* If we aren't caching a namespace, add a semicolon. */
      if (get_ifc_basis(itf) != ifc_tbs_namespace) {
        cache_token(cache, tok_semicolon);
      }  /* if */
    }  /* if */
  };
  cache_scope_decl(cache, decl_idx, type, cache_name_fn, cache_scope_fn);
}  /* cache_scope_decl */


void an_ifc_module::cache_type_first_part(a_module_token_cache_ptr cache,
                                          an_ifc_type_index        type)
/*
Add the tokens to cache corresponding to the portion of the given type that
precedes an identifier.  This routine will often need to be called in concert
with cache_type_second_part, which will cache tokens corresponding to the
portion of type that follows an identifier.  For example:

   ~v~ This routine caches this portion of the type
   int arr[3];
          ~^~ but not this portion.

See form_type_first_part and form_type_second_part for more details as to why
this is needed.
*/
{
  switch (type.sort) {
    case ifc_ts_type_vendor_extension:
      issue_unsupported_construct_error(this, "TypeSort::VendorExtension",
                                        &error_position);
      goto invalid;
    case ifc_ts_type_fundamental:
      { Opt<an_ifc_type_fundamental> opt_itf;

        construct_node(&opt_itf, type);
        if (!opt_itf.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_fundamental    itf = *opt_itf;
        an_ifc_type_sign_sort      sign = get_ifc_sign(itf);
        an_ifc_type_basis_sort     basis = get_ifc_basis(itf);
        an_ifc_type_precision_sort precision = get_ifc_precision(itf);
        switch (sign) {
          case ifc_tss_plain:
            break;
          case ifc_tss_signed:
            cache_token(cache, tok_signed);
            break;
          case ifc_tss_unsigned:
            cache_token(cache, tok_unsigned);
            break;
          default_is_unexpected_str("Unexpected TypeSign");
        }  /* if */
        switch (basis) {
          case ifc_tbs_void:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_void);
            break;
          case ifc_tbs_bool:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_bool);
            break;
          case ifc_tbs_char:
            switch (precision) {
              case ifc_tps_default:
                cache_token(cache, tok_char);
                break;
              case ifc_tps_bit8:
                cache_token(cache, tok_char8_t);
                break;
              case ifc_tps_bit16:
                cache_token(cache, tok_char16_t);
                break;
              case ifc_tps_bit32:
                cache_token(cache, tok_char32_t);
                break;
              default:
                { a_string err_msg("Unexpected ", str_for(precision),
                                   " for ", str_for(basis));

                  ifc_unexpected(this, err_msg);
                }
                goto invalid;
            }  /* switch */
            break;
          case ifc_tbs_wchar_t:
            cache_token(cache, tok_wchar_t);
            break;
          case ifc_tbs_int:
            switch (precision) {
              case ifc_tps_default:
                cache_token(cache, tok_int);
                break;
              case ifc_tps_short:
                cache_token(cache, tok_short);
                break;
              case ifc_tps_long:
                cache_token(cache, tok_long);
                break;
              case ifc_tps_bit8:
                cache_token(cache, tok_int8);
                break;
              case ifc_tps_bit16:
                cache_token(cache, tok_int16);
                break;
              case ifc_tps_bit32:
                cache_token(cache, tok_int32);
                break;
              case ifc_tps_bit64:
                cache_token(cache, tok_int64);
                break;
              case ifc_tps_bit128:
#if INT128_EXTENSIONS_ALLOWED
                cache_token(cache, tok_int128);
#endif /* INT128_EXTENSIONS_ALLOWED */
                break;
              default_is_unexpected();
            }  /* switch */
            break;
          case ifc_tbs_float:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_float);
            break;
          case ifc_tbs_double:
            if (precision == ifc_tps_long) {
              cache_token(cache, tok_long);
            } else {
              check_assertion(precision == ifc_tps_default);
            }  /* if */
            cache_token(cache, tok_double);
            break;
          case ifc_tbs_nullptr:
            check_assertion(precision == ifc_tps_default);
            cache_resolved_type_token(cache, standard_nullptr_type());
            break;
          case ifc_tbs_ellipsis:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_ellipsis);
            break;
          case ifc_tbs_segment_type:
            /* FIXME: Currently unsupported. */
            issue_unsupported_construct_error(this, "TypeBasis::SegmentType",
                                              &error_position);
            goto invalid;
          case ifc_tbs_class:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_class);
            break;
          case ifc_tbs_struct:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_struct);
            break;
          case ifc_tbs_union:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_union);
            break;
          case ifc_tbs_enum:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_enum);
            break;
          case ifc_tbs_typename:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_typename);
            break;
          case ifc_tbs_namespace:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_namespace);
            break;
          case ifc_tbs_interface:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_interface);
            break;
          case ifc_tbs_function:
            /* FIXME: Currently unsupported. */
            issue_unsupported_construct_error(this, "TypeBasis::Function",
                                              &error_position);
            goto invalid;
          case ifc_tbs_empty:
            break;
          case ifc_tbs_variable_template:
            /* FIXME: Currently unsupported. */
            issue_unsupported_construct_error(this,
                                              "TypeBasis::VariableTemplate",
                                              &error_position);
            goto invalid;
          case ifc_tbs_auto:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_auto_type);
            break;
          case ifc_tbs_decltype_auto:
            check_assertion(precision == ifc_tps_default);
            cache_token(cache, tok_decltype);
            cache_token(cache, tok_lparen);
            cache_token(cache, tok_auto_type);
            cache_token(cache, tok_rparen);
            break;
          case ifc_tbs_concept:
            /* FIXME: Currently unsupported. */
            issue_unsupported_construct_error(this,
                                              "TypeBasis::Concept",
                                              &error_position);
            goto invalid;
          case ifc_tbs_overload:
            /* FIXME: Currently unsupported. */
            issue_unsupported_construct_error(this, "TypeBasis::Overload",
                                              &error_position);
            goto invalid;
          default_is_unexpected_str("Unexpected TypeBasis");
        }  /* switch */
      }
      break;
    case ifc_ts_type_designated:
      { Opt<an_ifc_type_designated> opt_itd;
        a_boolean                   is_closure_type = FALSE;

        construct_node(&opt_itd, type);
        if (!opt_itd.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_designated itf = *opt_itd;
        an_ifc_decl_index      decl = get_ifc_decl(itf);
        if (!validate(decl)) {
          goto invalid;
        }  /* if */
        if (decl.sort == ifc_ds_decl_scope) {
          /* Check for the special case of a closure type.  Such a type
             cannot be expressed as a normal type name, but it can appear
             when "auto" was used as a type specifier. */
          Opt<an_ifc_decl_scope> opt_ids;
          construct_node(&opt_ids, decl);
          if (!opt_ids.has_value()) {
            goto invalid;
          }  /* if */
          an_ifc_decl_scope  ids = *opt_ids;
          an_ifc_scope_traits_bitfield
                             traits = get_ifc_traits(ids);
          is_closure_type = test_bitmask<ifc_stb_closure_type>(traits);
        }  /* if */
        if (is_closure_type) {
          cache_token(cache, tok_auto);
        } else if (is_name_qualifiable(decl)) {
          cache_qualified_name_from_decl(cache, decl);
        } else if (decl.sort == ifc_ds_decl_parameter) {
          /* Represent the reference to a template parameter with a special
             token because the name recorded for a parameter may not be the
             one that the parameter was declared with in the current
             context. */
          cache_token_with_index(cache, tok_ifc_template_param, decl);
        } else {
          /* We are contextually forbidden from qualifying this name or the
             declaration type otherwise is considered to never appear
             with a qualified name.

             This can happen when building an unqualified-id within a dependent
             context. */
          cache_name_from_decl(cache, decl);
        }  /* if */
      }
      break;
    case ifc_ts_type_tor:
      /* This type should only be encountered when processing a constructor,
         and that is directly handled with that constructor declaration.*/
      unexpected_condition();
      break;
    case ifc_ts_type_syntactic:
      { Opt<an_ifc_type_syntactic> opt_its;

        construct_node(&opt_its, type);
        if (!opt_its.has_value()) {
          goto invalid;
        }  /* if */
        cache_expr(cache, get_ifc_expr(*opt_its), /*cinfo=*/{});
      }
      break;
    case ifc_ts_type_expansion:
      { Opt<an_ifc_type_expansion> opt_ite;

        construct_node(&opt_ite, type);
        if (!opt_ite.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_expansion ite = *opt_ite;
        cache_type_first_part(cache, get_ifc_pack(ite));
        cache_token(cache, tok_ellipsis);
      }
      break;
    case ifc_ts_type_pointer:
      { Opt<an_ifc_type_pointer> opt_itp;

        construct_node(&opt_itp, type);
        if (!opt_itp.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_pointer itp = *opt_itp;
        an_ifc_type_index   pointee = get_ifc_pointee(itp);
        cache_type_first_part(cache, pointee);
        if (pointee.sort != ifc_ts_type_pointer_to_member) {
          /* The tok_star will already have been cached if the pointee is a
             pointer-to-member. */
          cache_token(cache, tok_star);
        }  /* if */
      }
      break;
    case ifc_ts_type_pointer_to_member:
      { Opt<an_ifc_type_pointer_to_member> opt_itptm;

        construct_node(&opt_itptm, type);
        if (!opt_itptm.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_pointer_to_member itptm = *opt_itptm;
        an_ifc_type_index             member = get_ifc_member(itptm);
        if (member.sort == ifc_ts_type_method) {
          Opt<an_ifc_type_method> opt_itm;

          construct_node(&opt_itm, member);
          if (!opt_itm.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_method itm = *opt_itm;
          cache_type(cache, get_ifc_target(itm));
          cache_token(cache, tok_lparen);
          cache_calling_convention(cache, get_ifc_convention(itm));
          cache_type(cache, get_ifc_scope(itm));
          cache_token(cache, tok_colon_colon);
        } else {
          cache_type(cache, get_ifc_scope(itptm));
          cache_token(cache, tok_colon_colon);
          cache_type(cache, member);
        }  /* if */
        cache_token(cache, tok_star);
      }
      break;
    case ifc_ts_type_lvalue_reference:
      { Opt<an_ifc_type_lvalue_reference> opt_itlr;

        construct_node(&opt_itlr, type);
        if (!opt_itlr.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_lvalue_reference itlr = *opt_itlr;
        cache_type_first_part(cache, get_ifc_referee(itlr));
        cache_token(cache, tok_ampersand);
      }
      break;
    case ifc_ts_type_rvalue_reference:
      { Opt<an_ifc_type_rvalue_reference> opt_itrr;

        construct_node(&opt_itrr, type);
        if (!opt_itrr.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_rvalue_reference itrr = *opt_itrr;
        cache_type_first_part(cache, get_ifc_referee(itrr));
        cache_token(cache, tok_and_and);
      }
      break;
    case ifc_ts_type_function:
      { Opt<an_ifc_type_function> opt_itf;

        construct_node(&opt_itf, type);
        if (!opt_itf.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_function itf = *opt_itf;
        cache_type(cache, get_ifc_target(itf));
        cache_token(cache, tok_lparen);
        cache_calling_convention(cache, get_ifc_convention(itf));
      }
      break;
    case ifc_ts_type_method:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "TypeSort::Method",
                                        &error_position);
      goto invalid;
    case ifc_ts_type_array:
      { Opt<an_ifc_type_array> opt_ita;

        construct_node(&opt_ita, type);
        if (!opt_ita.has_value()) {
          goto invalid;
        }  /* if */
        cache_type_first_part(cache, get_ifc_element(*opt_ita));
      }
      break;
    case ifc_ts_type_typename:
      { Opt<an_ifc_type_typename> opt_itt;

        construct_node(&opt_itt, type);
        if (!opt_itt.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_typename);
        cache_expr(cache, get_ifc_path(*opt_itt), /*cinfo=*/{});
      }
      break;
    case ifc_ts_type_qualified:
      { Opt<an_ifc_type_qualified> opt_itq;

        construct_node(&opt_itq, type);
        if (!opt_itq.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_qualified     itq = *opt_itq;
        an_ifc_qualifier_bitfield qualifiers = get_ifc_qualifiers(itq);
        cache_type_first_part(cache, get_ifc_unqualified(itq));
        if (test_bitmask<ifc_qb_const>(qualifiers)) {
          cache_token(cache, tok_const);
        }  /* if */
        if (test_bitmask<ifc_qb_volatile>(qualifiers)) {
          cache_token(cache, tok_volatile);
        }  /* if */
        if (test_bitmask<ifc_qb_restrict>(qualifiers)) {
          cache_token(cache, tok_restrict);
        }  /* if */
      }
      break;
    case ifc_ts_type_base:
      { Opt<an_ifc_type_base> opt_itb;

        construct_node(&opt_itb, type);
        if (!opt_itb.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_base itb = *opt_itb;
        cache_access_specifier(cache, get_ifc_access(itb));
        if (get_ifc_shared(itb)) {
          cache_token(cache, tok_virtual);
        }  /* if */
        cache_type(cache, get_ifc_type(itb));
        if (get_ifc_pack_expanded(itb)) {
          cache_token(cache, tok_ellipsis);
        }  /* if */
      }
      break;
    case ifc_ts_type_decltype:
      { Opt<an_ifc_type_decltype> opt_itd;

        construct_node(&opt_itd, type);
        if (!opt_itd.has_value()) {
          goto invalid;
        }  /* if */
        /* decltype constructs are currently represented as token sequences. */
        cache_syntax(cache, get_ifc_expr(*opt_itd), /*cinfo=*/{});
      }
      break;
    case ifc_ts_type_placeholder:
      { Opt<an_ifc_type_placeholder> opt_itp;

        construct_node(&opt_itp, type);
        if (!opt_itp.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_placeholder itp = *opt_itp;
        an_ifc_type_basis_sort  basis = get_ifc_basis(itp);
        if (basis == ifc_tbs_auto) {
          cache_token(cache, tok_auto);
        } else {
          check_assertion(basis == ifc_tbs_decltype_auto);
          cache_token(cache, tok_decltype);
          cache_token(cache, tok_lparen);
          cache_token(cache, tok_auto);
          cache_token(cache, tok_rparen);
        }  /* if */
      }
      break;
    case ifc_ts_type_tuple:
      { Opt<an_ifc_type_tuple> opt_itt;

        construct_node(&opt_itt, type);
        if (!opt_itt.has_value()) {
          goto invalid;
        }  /* if */

        a_type_heap_traverser traverser(*opt_itt);
        a_boolean             first = TRUE;
        for (an_Indexed<an_ifc_heap_type> indexed_iht : traverser) {
          if (!indexed_iht.has_value()) {
            goto invalid;
          }  /* if */
          if (!first) {
            cache_token(cache, tok_comma);
          }  /* if */
          cache_type(cache, get_ifc_value(*indexed_iht));
          first = FALSE;
        }  /* for */
      }
      break;
    case ifc_ts_type_forall:
      { Opt<an_ifc_type_forall> opt_itfa;

        construct_node(&opt_itfa, type);
        if (!opt_itfa.has_value()) {
          goto invalid;
        }  /* if */
        cache_template_head(cache, get_ifc_chart(*opt_itfa), /*cinfo=*/{});
        cache_type(cache, get_ifc_subject(*opt_itfa));
      }
      break;
    case ifc_ts_type_unaligned:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "TypeSort::Unaligned",
                                        &error_position);
      goto invalid;
    case ifc_ts_type_syntax_tree:
      { Opt<an_ifc_type_syntax_tree> opt_tst;

        construct_node(&opt_tst, type);
        if (!opt_tst.has_value()) {
          goto invalid;
        }  /* if */
        cache_syntax(cache, get_ifc_syntax(*opt_tst), /*cinfo=*/{});
      }
      break;
    default_is_unexpected_str("Unexpected TypeSort");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad type cache");
  cache->invalidate();
done:;
}  /* cache_type_first_part */


void an_ifc_module::cache_type_second_part(a_module_token_cache_ptr     cache,
                                           an_ifc_type_index            type)
/*
Add the tokens to cache corresponding to the portion of the given type that
follows an identifier.  This routine will often need to be called in concert
with cache_type_first_part, which will cache tokens corresponding to the
portion of type that precedes an identifier.  For example:

          ~v~ This routine caches this portion of the type
   int arr[3];
   ~^~ but not this portion.

See form_type_first_part and form_type_second_part for more details as to why
this is needed.
*/
{
  switch (type.sort) {
    case ifc_ts_type_tor:
      /* This type should only be encountered when processing a constructor,
         and that is directly handled with that constructor declaration.*/
      { a_string err_msg("Unexpected ", str_for(type.sort));

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_ts_type_pointer:
      { Opt<an_ifc_type_pointer> opt_itp;

        construct_node(&opt_itp, type);
        if (!opt_itp.has_value()) {
          goto invalid;
        }  /* if */
        cache_type_second_part(cache, get_ifc_pointee(*opt_itp));
      }
      break;
    case ifc_ts_type_pointer_to_member:
      { Opt<an_ifc_type_pointer_to_member> opt_itptm;

        construct_node(&opt_itptm, type);
        if (!opt_itptm.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_pointer_to_member itptm = *opt_itptm;
        an_ifc_type_index             member = get_ifc_member(itptm);
        if (member.sort == ifc_ts_type_method) {
          Opt<an_ifc_type_method> opt_itm;

          construct_node(&opt_itm, member);
          if (!opt_itm.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_method itm = *opt_itm;
          an_ifc_type_index  source = get_ifc_source(itm);
          cache_token(cache, tok_rparen);
          cache_token(cache, tok_lparen);
          if (!is_null_index(source)) {
            cache_type(cache, source);
          }  /* if */
          cache_token(cache, tok_rparen);
          cache_func_type_noexcept_specifier(cache, itm);
          cache_func_type_cv_qualifiers(cache, itm);
          cache_func_type_ref_qualifier(cache, itm);
        }  /* if */
      }
      break;
    case ifc_ts_type_lvalue_reference:
      { Opt<an_ifc_type_lvalue_reference> opt_itlr;

        construct_node(&opt_itlr, type);
        if (!opt_itlr.has_value()) {
          goto invalid;
        }  /* if */
        cache_type_second_part(cache, get_ifc_referee(*opt_itlr));
      }
      break;
    case ifc_ts_type_rvalue_reference:
      { Opt<an_ifc_type_rvalue_reference> opt_itrr;

        construct_node(&opt_itrr, type);
        if (!opt_itrr.has_value()) {
          goto invalid;
        }  /* if */
        cache_type_second_part(cache, get_ifc_referee(*opt_itrr));
      }
      break;
    case ifc_ts_type_function:
      { Opt<an_ifc_type_function> opt_itf;

        construct_node(&opt_itf, type);
        if (!opt_itf.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_function itf = *opt_itf;
        an_ifc_type_index    source = get_ifc_source(itf);
        cache_token(cache, tok_rparen);
        cache_token(cache, tok_lparen);
        if (!is_null_index(source)) {
          cache_type(cache, source);
        }  /* if */
        cache_token(cache, tok_rparen);
        cache_func_type_cv_qualifiers(cache, itf);
        cache_func_type_ref_qualifier(cache, itf);
        cache_func_type_noexcept_specifier(cache, itf);
      }
      break;
    case ifc_ts_type_array:
      { Opt<an_ifc_type_array> opt_ita;

        construct_node(&opt_ita, type);
        if (!opt_ita.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_array ita = *opt_ita;
        an_ifc_expr_index extent = get_ifc_extent(ita);
        cache_token(cache, tok_lbracket);
        if (!is_null_index(extent)) {
          cache_expr(cache, extent, /*cinfo=*/{});
        }  /* if */
        cache_token(cache, tok_rbracket);
        cache_type_second_part(cache, get_ifc_element(ita));
      }
      break;
    case ifc_ts_type_qualified:
      { Opt<an_ifc_type_qualified> opt_itq;

        construct_node(&opt_itq, type);
        if (!opt_itq.has_value()) {
          goto invalid;
        }  /* if */
        cache_type_second_part(cache, get_ifc_unqualified(*opt_itq));
      }
      break;
    case ifc_ts_type_expansion:
      { Opt<an_ifc_type_expansion> opt_ite;

        construct_node(&opt_ite, type);
        if (!opt_ite.has_value()) {
          goto invalid;
        }  /* if */
        cache_type_second_part(cache, get_ifc_pack(*opt_ite));
      }
      break;
    case ifc_ts_type_fundamental:
    case ifc_ts_type_designated:
    case ifc_ts_type_syntactic:
    case ifc_ts_type_method:
    case ifc_ts_type_typename:
    case ifc_ts_type_base:
    case ifc_ts_type_decltype:
    case ifc_ts_type_placeholder:
    case ifc_ts_type_tuple:
    case ifc_ts_type_forall:
    case ifc_ts_type_unaligned:
    case ifc_ts_type_syntax_tree:
      /* All of these were completely handled by the first pass. */
      break;
    case ifc_ts_type_vendor_extension:
      issue_unsupported_construct_error(this, str_for(type.sort),
                                        &error_position);
      goto invalid;
    default_is_unexpected_str("Unexpected TypeSort");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad type cache");
  cache->invalidate();
done:;
}  /* cache_type_second_part */


void an_ifc_module::cache_type(a_module_token_cache_ptr     cache,
                               an_ifc_type_index            type)
/*
Add the tokens to cache corresponding to the given type.  This routine should
only be called when there is no identifier portion involved and therefore both
the preceding and following portions of the type can be immediately cached.  If
there is an identifier portion involved, cache_type_first_part and
cache_type_second_part should be used instead.
*/
{
  cache_type_first_part(cache, type);
  cache_type_second_part(cache, type);
}  /* cache_type */


void an_ifc_module::cache_chart(a_module_token_cache_ptr cache,
                                an_ifc_chart_index       chart,
                                const an_ifc_cache_info  &cinfo)
/*
Add the tokens corresponding to the given chart to cache.  The caller is
expected to have already cached the "template" keyword if it's required.  cinfo
contains information about the current cache context to help inform decisions
about what to cache.
*/
{
  an_ifc_expr_index constraint = {};

  cache_token(cache, tok_lt);
  switch (chart.sort) {
    case ifc_cs_chart_none:
      /* No arguments to the template (i.e., specialization). */
      break;
    case ifc_cs_chart_unilevel:
      { Opt<an_ifc_chart_unilevel> opt_icu;

        construct_node(&opt_icu, chart);
        if (!opt_icu.has_value()) {
          goto invalid;
        }  /* if */
        constraint = get_ifc_constraint(*opt_icu);

        a_decl_parameter_traverser traverser(*opt_icu);
        a_boolean                  first = TRUE;
        for (an_Indexed<an_ifc_decl_parameter> indexed_idp : traverser) {
          if (!indexed_idp.has_value()) {
            goto invalid;
          }  /* if */
          if (!first) {
            cache_token(cache, tok_comma);
          }  /* if */
          if (!cache_direct_decl(cache, *indexed_idp, cinfo)) {
            goto invalid;
          }  /* if */
          first = FALSE;
        }  /* for */
      }
      break;
    case ifc_cs_chart_multilevel:
      { Opt<an_ifc_chart_multilevel> opt_icm;

        construct_node(&opt_icm, chart);
        if (!opt_icm.has_value()) {
          goto invalid;
        }  /* if */

        a_decl_temploid_traverser traverser(*opt_icm);
        a_boolean                 first = TRUE;
        for (an_Indexed<an_ifc_decl_temploid> indexed_idt : traverser) {
          if (!first) {
            cache_token(cache, tok_comma);
          }  /* if */
          /* FIXME: Is this correct? */
          if (!cache_direct_decl(cache, *indexed_idt, cinfo)) {
            goto invalid;
          }  /* if */
          first = FALSE;
        }  /* for */
      }
      break;
    default_is_unexpected_str("Unexpected ChartSort");
  }  /* switch */
  cache_token(cache, tok_gt);
  if (!is_null_index(constraint)) {
    /* The template parameter list is followed by a requires-clause. */
    cache_expr(cache, constraint, cinfo);
  }  /* if */
  goto done;
invalid:
  expect_error_str("expected errors for bad chart cache");
  cache->invalidate();
done:;
}  /* cache_chart */


void an_ifc_module::cache_operator(a_module_token_cache_ptr     cache,
                                   an_ifc_operator_category     op)
/*
Add the tokens corresponding to the given Operator to cache.
*/
{
  switch (op.sort) {
    case ifc_os_dyadic_operator:
      cache_operator(cache, op.variant.dyadic_operator);
      break;
    case ifc_os_monadic_operator:
      cache_operator(cache, op.variant.monadic_operator);
      break;
    case ifc_os_niladic_operator:
      cache_operator(cache, op.variant.niladic_operator);
      break;
    case ifc_os_storage_instruction_operator:
      cache_operator(cache, op.variant.storage_instruction_operator);
      break;
    case ifc_os_triadic_operator:
      cache_operator(cache, op.variant.triadic_operator);
      break;
    case ifc_os_variadic_operator:
      cache_operator(cache, op.variant.variadic_operator);
      break;
    default_is_unexpected_str("Unexpected OperatorSort");
  }  /* switch */
}  /* cache_operator */


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
/* Remove ARG_UNUSED as well. */
void an_ifc_module::cache_operator(ARG_UNUSED a_module_token_cache_ptr cache,
                                   an_ifc_niladic_operator_sort        op)
/*
Add the tokens corresponding to the given Niladic Operator to cache.
*/
{
  switch (op) {
    case ifc_nos_unknown:
    case ifc_nos_msvc:
      { a_string err_msg("Unexpected ", str_for(op));

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_nos_phantom:
    case ifc_nos_constant:
    case ifc_nos_nil:
    case ifc_nos_msvc_constant_object:
    case ifc_nos_msvc_lambda:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, str_for(op), &error_position);
      goto invalid;
    default_is_unexpected_str("Unexpected NiladicOperator");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad operator cache");
  cache->invalidate();
done:;
}  /* cache_operator */


void an_ifc_module::cache_operator(a_module_token_cache_ptr     cache,
                                   an_ifc_monadic_operator_sort op)
/*
Add the tokens corresponding to the given Monadic Operator to cache.
*/
{
  switch (op) {
    case ifc_mos_unknown:
    case ifc_mos_msvc:
    case ifc_mos_msvc_confusion:
      { a_string err_msg("Unexpected ", str_for(op));

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_mos_plus:
      cache_token(cache, tok_plus);
      break;
    case ifc_mos_negate:
      cache_token(cache, tok_minus);
      break;
    case ifc_mos_deref:
      cache_token(cache, tok_star);
      break;
    case ifc_mos_address:
      cache_token(cache, tok_ampersand);
      break;
    case ifc_mos_complement:
      cache_token(cache, tok_compl);
      break;
    case ifc_mos_not:
      cache_token(cache, tok_not);
      break;
    case ifc_mos_pre_increment:
      cache_token(cache, tok_plus_plus);
      break;
    case ifc_mos_pre_decrement:
      cache_token(cache, tok_minus_minus);
      break;
    case ifc_mos_post_increment:
      cache_token(cache, tok_plus_plus);
      break;
    case ifc_mos_post_decrement:
      cache_token(cache, tok_minus_minus);
      break;
    case ifc_mos_truncate:
    case ifc_mos_ceil:
    case ifc_mos_floor:
    case ifc_mos_paren:
    case ifc_mos_brace:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, str_for(op),
                                        &error_position);
      goto invalid;
    case ifc_mos_alignas:
      cache_token(cache, tok_alignas);
      break;
    case ifc_mos_alignof:
      cache_token(cache, tok_alignof);
      break;
    case ifc_mos_sizeof:
      cache_token(cache, tok_sizeof);
      break;
    case ifc_mos_cardinality:
      cache_token(cache, tok_sizeof);
      cache_token(cache, tok_ellipsis);
      break;
    case ifc_mos_typeid:
      cache_token(cache, tok_typeid);
      break;
    case ifc_mos_noexcept:
      cache_token(cache, tok_noexcept);
      break;
    case ifc_mos_requires:
      cache_token(cache, tok_requires);
      break;
    case ifc_mos_co_return:
      cache_token(cache, tok_coroutine_return);
      break;
    case ifc_mos_await:
      cache_token(cache, tok_coroutine_await);
      break;
    case ifc_mos_yield:
      cache_token(cache, tok_coroutine_yield);
      break;
    case ifc_mos_throw:
      cache_token(cache, tok_throw);
      break;
    case ifc_mos_new:
      cache_token(cache, tok_new);
      break;
    case ifc_mos_delete:
      cache_token(cache, tok_delete);
      break;
    case ifc_mos_delete_array:
      cache_token(cache, tok_delete);
      cache_token(cache, tok_lbracket);
      cache_token(cache, tok_rbracket);
      break;
    case ifc_mos_expand:
      cache_token(cache, tok_ellipsis);
      break;
    case ifc_mos_read:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "MonadicOperator::Read",
                                        &error_position);
      goto invalid;
    case ifc_mos_materialize:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "MonadicOperator::Materialize",
                                        &error_position);
      goto invalid;
    case ifc_mos_pseudo_dtor_call:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "MonadicOperator::PseudoDtorCall",
                                        &error_position);
      goto invalid;
    case ifc_mos_msvc_assume:
      cache_token(cache, tok_assume);
      break;
    case ifc_mos_msvc_alignof:
      cache_token(cache, tok_alignof);
      break;
    case ifc_mos_msvc_uuidof:
      cache_token(cache, tok_uuidof);
      break;
    case ifc_mos_msvc_is_class:
      cache_token(cache, tok_is_class);
      break;
    case ifc_mos_msvc_is_union:
      cache_token(cache, tok_is_union);
      break;
    case ifc_mos_msvc_is_enum:
      cache_token(cache, tok_is_enum);
      break;
    case ifc_mos_msvc_is_polymorphic:
      cache_token(cache, tok_is_polymorphic);
      break;
    case ifc_mos_msvc_is_empty:
      cache_token(cache, tok_is_empty);
      break;
    case ifc_mos_lookup_globally:
      cache_token(cache, tok_colon_colon);
      break;
    case ifc_mos_msvc_is_trivially_copy_constructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                           this,
                           "MonadicOperator::MsvcIsTriviallyCopyConstructible",
                           &error_position);
      goto invalid;
    case ifc_mos_msvc_is_trivially_copy_assignable:
      cache_token(cache, tok_is_trivially_copy_assignable);
      break;
    case ifc_mos_msvc_is_trivially_destructible:
      cache_token(cache, tok_is_trivially_destructible);
      break;
    case ifc_mos_msvc_has_virtual_destructor:
      cache_token(cache, tok_has_virtual_destructor);
      break;
    case ifc_mos_msvc_is_nothrow_copy_constructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                             this,
                             "MonadicOperator::MsvcIsNothrowCopyConstructible",
                             &error_position);
      goto invalid;
    case ifc_mos_msvc_is_nothrow_copy_assignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                                this,
                                "MonadicOperator::MsvcIsNothrowCopyAssignable",
                                &error_position);
      goto invalid;
    case ifc_mos_msvc_is_pod:
      cache_token(cache, tok_is_pod);
      break;
    case ifc_mos_msvc_is_abstract:
      cache_token(cache, tok_is_abstract);
      break;
    case ifc_mos_msvc_is_trivial:
      cache_token(cache, tok_is_trivial);
      break;
    case ifc_mos_msvc_is_trivially_copyable:
      cache_token(cache, tok_is_trivially_copyable);
      break;
    case ifc_mos_msvc_is_standard_layout:
      cache_token(cache, tok_is_standard_layout);
      break;
    case ifc_mos_msvc_is_literal_type:
      cache_token(cache, tok_is_literal_type);
      break;
    case ifc_mos_msvc_is_trivially_move_constructible:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                           this,
                           "MonadicOperator::MsvcIsTriviallyMoveConstructible",
                           &error_position);
      goto invalid;
    case ifc_mos_msvc_has_trivial_move_assign:
      cache_token(cache, tok_has_trivial_move_assign);
      break;
    case ifc_mos_msvc_is_trivially_move_assignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                              this,
                              "MonadicOperator::MsvcIsTriviallyMoveAssignable",
                              &error_position);
      goto invalid;
    case ifc_mos_msvc_is_nothrow_move_assignable:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                                this,
                                "MonadicOperator::MsvcIsNothrowMoveAssignable",
                                &error_position);
      goto invalid;
    case ifc_mos_msvc_underlying_type:
      cache_token(cache, tok_underlying_type);
      break;
    case ifc_mos_msvc_is_destructible:
      cache_token(cache, tok_is_destructible);
      break;
    case ifc_mos_msvc_is_nothrow_destructible:
      cache_token(cache, tok_is_nothrow_destructible);
      break;
    case ifc_mos_msvc_has_unique_object_representations:
      cache_token(cache, tok_has_unique_object_representations);
      break;
    case ifc_mos_msvc_is_aggregate:
      cache_token(cache, tok_is_aggregate);
      break;
    case ifc_mos_msvc_builtin_address_of:
      cache_token(cache, tok_builtin_addressof);
      break;
    case ifc_mos_msvc_is_ref_class:
      cache_token(cache, tok_is_ref_class);
      break;
    case ifc_mos_msvc_is_value_class:
      cache_token(cache, tok_is_value_class);
      break;
    case ifc_mos_msvc_is_simple_value_class:
      cache_token(cache, tok_is_simple_value_class);
      break;
    case ifc_mos_msvc_is_interface_class:
      cache_token(cache, tok_is_interface_class);
      break;
    case ifc_mos_msvc_is_delegate:
      cache_token(cache, tok_is_delegate);
      break;
    case ifc_mos_msvc_is_final:
      cache_token(cache, tok_is_final);
      break;
    case ifc_mos_msvc_is_sealed:
      cache_token(cache, tok_is_sealed);
      break;
    case ifc_mos_msvc_has_finalizer:
      cache_token(cache, tok_has_finalizer);
      break;
    case ifc_mos_msvc_has_copy:
      cache_token(cache, tok_has_copy);
      break;
    case ifc_mos_msvc_has_assign:
      cache_token(cache, tok_has_assign);
      break;
    case ifc_mos_msvc_has_user_destructor:
      cache_token(cache, tok_has_user_destructor);
      break;
    case ifc_mos_msvc_confused_expand:
      cache_token(cache, tok_ellipsis);
      break;
    case ifc_mos_msvc_confused_dependent_sizeof:
      cache_token(cache, tok_sizeof);
      break;
    default_is_unexpected_str("Unexpected MonadicOperator");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad operator cache");
  cache->invalidate();
done:;
}  /* cache_operator */


void an_ifc_module::cache_operator(a_module_token_cache_ptr     cache,
                                   an_ifc_dyadic_operator_sort  op)
/*
Add the tokens corresponding to the given Dyadic Operator to cache.
*/
{
  switch (op) {
    case ifc_dos_unknown:
    case ifc_dos_select:
    case ifc_dos_msvc:
    case ifc_dos_msvc_saturated_arithmetic:
      { a_string err_msg("Unexpected ", str_for(op));

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_dos_plus:
      cache_token(cache, tok_plus);
      break;
    case ifc_dos_minus:
      cache_token(cache, tok_minus);
      break;
    case ifc_dos_mult:
      cache_token(cache, tok_star);
      break;
    case ifc_dos_slash:
      cache_token(cache, tok_divide);
      break;
    case ifc_dos_modulo:
      cache_token(cache, tok_remainder);
      break;
    case ifc_dos_remainder:
      cache_token(cache, tok_remainder);
      break;
    case ifc_dos_bitand:
      cache_token(cache, tok_ampersand);
      break;
    case ifc_dos_bitor:
      cache_token(cache, tok_or);
      break;
    case ifc_dos_bitxor:
      cache_token(cache, tok_excl_or);
      break;
    case ifc_dos_lshift:
      cache_token(cache, tok_shift_left);
      break;
    case ifc_dos_rshift:
      cache_token(cache, tok_shift_right);
      break;
    case ifc_dos_equal:
      cache_token(cache, tok_eq);
      break;
    case ifc_dos_not_equal:
      cache_token(cache, tok_ne);
      break;
    case ifc_dos_less:
      cache_token(cache, tok_lt);
      break;
    case ifc_dos_less_equal:
      cache_token(cache, tok_le);
      break;
    case ifc_dos_greater:
      cache_token(cache, tok_gt);
      break;
    case ifc_dos_greater_equal:
      cache_token(cache, tok_ge);
      break;
    case ifc_dos_compare:
      cache_token(cache, tok_spaceship);
      break;
    case ifc_dos_logic_and:
      cache_token(cache, tok_and_and);
      break;
    case ifc_dos_logic_or:
      cache_token(cache, tok_or_or);
      break;
    case ifc_dos_assign:
      cache_token(cache, tok_assign);
      break;
    case ifc_dos_plus_assign:
      cache_token(cache, tok_plus_assign);
      break;
    case ifc_dos_minus_assign:
      cache_token(cache, tok_minus_assign);
      break;
    case ifc_dos_mult_assign:
      cache_token(cache, tok_times_assign);
      break;
    case ifc_dos_slash_assign:
      cache_token(cache, tok_divide_assign);
      break;
    case ifc_dos_modulo_assign:
      cache_token(cache, tok_remainder_assign);
      break;
    case ifc_dos_bitand_assign:
      cache_token(cache, tok_and_assign);
      break;
    case ifc_dos_bitor_assign:
      cache_token(cache, tok_or_assign);
      break;
    case ifc_dos_bitxor_assign:
      cache_token(cache, tok_excl_or_assign);
      break;
    case ifc_dos_lshift_assign:
      cache_token(cache, tok_shift_left_assign);
      break;
    case ifc_dos_rshift_assign:
      cache_token(cache, tok_shift_right_assign);
      break;
    case ifc_dos_comma:
      cache_token(cache, tok_comma);
      break;
    case ifc_dos_dot:
      cache_token(cache, tok_period);
      break;
    case ifc_dos_arrow:
      cache_token(cache, tok_arrow);
      break;
    case ifc_dos_dot_star:
      cache_token(cache, tok_period_star);
      break;
    case ifc_dos_arrow_star:
      cache_token(cache, tok_arrow_star);
      break;
    case ifc_dos_reinterpret_cast:
      cache_token(cache, tok_reinterpret_cast);
      break;
    case ifc_dos_static_cast:
      cache_token(cache, tok_static_cast);
      break;
    case ifc_dos_const_cast:
      cache_token(cache, tok_const_cast);
      break;
    case ifc_dos_dynamic_cast:
      cache_token(cache, tok_dynamic_cast);
      break;
    case ifc_dos_msvc_builtin_offset_of:
      cache_token(cache, tok_builtin_offsetof);
      break;
    case ifc_dos_msvc_is_base_of:
      cache_token(cache, tok_is_base_of);
      break;
    case ifc_dos_msvc_is_convertible_to:
      cache_token(cache, tok_is_convertible_to);
      break;
    case ifc_dos_msvc_is_trivially_assignable:
      cache_token(cache, tok_is_trivially_assignable);
      break;
    case ifc_dos_msvc_is_nothrow_assignable:
      cache_token(cache, tok_is_nothrow_assignable);
      break;
    case ifc_dos_msvc_is_assignable:
      cache_token(cache, tok_is_assignable);
      break;
    case ifc_dos_msvc_is_assignable_nocheck:
      cache_token(cache, tok_is_assignable_no_precondition_check);
      break;
    case ifc_dos_msvc_builtin_bit_cast:
      cache_token(cache, tok_builtin_bit_cast);
      break;
    case ifc_dos_curry:
    case ifc_dos_apply:
    case ifc_dos_index:
    case ifc_dos_default_at:
    case ifc_dos_new:
    case ifc_dos_new_array:
    case ifc_dos_destruct:
    case ifc_dos_destruct_at:
    case ifc_dos_cleanup:
    case ifc_dos_qualification:
    case ifc_dos_promote:
    case ifc_dos_demote:
    case ifc_dos_coerce:
    case ifc_dos_rewrite:
    case ifc_dos_bless:
    case ifc_dos_cast:
    case ifc_dos_explicit_conversion:
    case ifc_dos_narrow:
    case ifc_dos_widen:
    case ifc_dos_pretend:
    case ifc_dos_closure:
    case ifc_dos_zero_initialize:
    case ifc_dos_clear_storage:
    case ifc_dos_msvc_try_cast:
    case ifc_dos_msvc_curry:
    case ifc_dos_msvc_virtual_curry:
    case ifc_dos_msvc_align:
    case ifc_dos_msvc_bit_span:
    case ifc_dos_msvc_bitfield_access:
    case ifc_dos_msvc_obscure_bitfield_access:
    case ifc_dos_msvc_initialize:
    case ifc_dos_msvc_builtin_is_layout_compatible:
    case ifc_dos_msvc_builtin_is_pointer_interconvertible_base_of:
    case ifc_dos_msvc_builtin_is_pointer_interconvertible_with_class:
    case ifc_dos_msvc_builtin_is_corresponding_member:
    case ifc_dos_msvc_intrinsic:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, str_for(op), &error_position);
      goto invalid;
    default_is_unexpected_str("Unexpected DyadicOperator");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad operator cache");
  cache->invalidate();
done:;
}  /* cache_operator */


void an_ifc_module::cache_operator(a_module_token_cache_ptr     cache,
                                   an_ifc_triadic_operator_sort op)
/*
Add the tokens corresponding to the given Triadic Operator to cache.
*/
{
  switch (op) {
    case ifc_tos_choice:
      cache_token(cache, tok_quest_mark);
      break;
    case ifc_tos_construct_at:
      cache_token(cache, tok_new);
      break;
    case ifc_tos_unknown:
    case ifc_tos_initialize:
    case ifc_tos_msvc:
    case ifc_tos_msvc_confusion:
    case ifc_tos_msvc_confused_choice:
    case ifc_tos_msvc_confused_push_state:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, str_for(op), &error_position);
      goto invalid;
    default_is_unexpected_str("Unexpected TriadicOperator");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad operator cache");
  cache->invalidate();
done:;
}  /* cache_operator */


/*lint -e2707*/ /* Remove when the routine returns to its caller. */
/* Remove ARG_UNUSED as well. */
void an_ifc_module::cache_operator(
                                ARG_UNUSED a_module_token_cache_ptr      cache,
                                an_ifc_storage_instruction_operator_sort op)
/*
Add the tokens corresponding to the given Storage Operator to cache.
*/
{
  switch (op) {
    case ifc_sios_unknown:
    case ifc_sios_msvc:
      { a_string err_msg("Unexpected ", str_for(op));

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_sios_allocate_single:
    case ifc_sios_allocate_array:
    case ifc_sios_deallocate_single:
    case ifc_sios_deallocate_array:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, str_for(op), &error_position);
      goto invalid;
    default_is_unexpected_str("Unexpected StorageOperator");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad operator cache");
  cache->invalidate();
done:;
}  /* cache_operator */


void an_ifc_module::cache_operator(a_module_token_cache_ptr      cache,
                                   an_ifc_variadic_operator_sort op)
/*
Add the tokens corresponding to the given Variadic Operator to cache.
*/
{
  switch (op) {
    case ifc_vos_unknown:
    case ifc_vos_msvc:
      { a_string err_msg("Unexpected ", str_for(op));

        ifc_unexpected(this, err_msg);
      }
      goto invalid;
    case ifc_vos_msvc_has_trivial_constructor:
      cache_token(cache, tok_has_trivial_constructor);
      break;
    case ifc_vos_msvc_is_constructible:
      cache_token(cache, tok_is_constructible);
      break;
    case ifc_vos_msvc_is_nothrow_constructible:
      cache_token(cache, tok_is_nothrow_constructible);
      break;
    case ifc_vos_msvc_is_trivially_constructible:
      cache_token(cache, tok_is_trivially_constructible);
      break;
    case ifc_vos_collection:
    case ifc_vos_sequence:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, str_for(op), &error_position);
      goto invalid;
    default_is_unexpected_str("Unexpected VariadicOperator");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad operator cache");
  cache->invalidate();
done:;
}  /* cache_operator */


template<typename a_Name_Cache_Fn, typename an_Init_Cache_Fn>
inline void an_ifc_module::cache_variable_decl(
                            a_module_token_cache_ptr         cache,
                            an_ifc_decl_index                decl_idx,
                            a_boolean                        is_data_member,
                            an_ifc_basic_specifiers_bitfield specifiers,
                            an_ifc_object_traits_bitfield    traits,
                            an_ifc_expr_index                alignment,
                            an_ifc_type_index                type,
                            a_Name_Cache_Fn                  cache_name_fn,
                            an_ifc_expr_index                width,
                            an_Init_Cache_Fn                 cache_init_fn)
/*
Add the tokens corresponding to the given variable declaration (indexed in the
IFC by decl_idx) to cache.  is_data_member is TRUE if this is a non-static data
member of a class.  specifiers, traits, alignment, and type are values from the
IFC file that describe the variable declaration.  cache_name_fn is a lambda
that's called to cache the name of the variable.  If width is not zero, this is
a bitfield and width is its size.  cache_init_fn is a lambda that's called to
cache the variable initializer (if any).

FIXME: Remove this version of cache_variable_decl once names can be cached
properly for specializations using a NameIndex, and similarly the variable's
initializer can be consistently cached via a ScopeIndex.
*/
{
  a_boolean decl_in_class = is_class_scope(get_ifc_home_scope(decl_idx));

  /* Cache tokens for MSVC "basic specifiers" (at the time of writing this
     includes extern "C" and [[deprecated]]). */
  /* Cache any associated attributes. */
  cache_attrs(cache, decl_idx);
  /* FIXME: Because we cache attributes properly now, this can result in two
     deprecated attributes. */
  cache_basic_specifiers(cache, specifiers);
  if (!is_data_member && decl_in_class) {
    /* This is a static data member. */
    cache_token(cache, tok_static);
  }  /* if */
  /* Cache the alignment if specified. */
  if (!is_null_index(alignment)) {
    cache_token(cache, tok_alignas);
    cache_token(cache, tok_lparen);
    cache_expr(cache, alignment, /*cinfo=*/{});
    cache_token(cache, tok_rparen);
  }  /* if */
  /* Cache the "object traits", roughly an MSVC subset of the
     decl-specifier-seq. */
  cache_object_traits(cache, traits);
  /* Cache the name surrounded by the respective type qualifiers. */
  cache_type_first_part(cache, type);
  cache_name_fn();
  cache_type_second_part(cache, type);
  /* Cache the variable with if any. */
  if (!is_null_index(width)) {
    cache_token(cache, tok_colon);
    cache_expr(cache, width, /*cinfo=*/{});
  }  /* if */
  /* Cache the initializer (if any). */
  cache_init_fn();
}  /* cache_variable_decl */


void an_ifc_module::cache_variable_decl(
                              a_module_token_cache_ptr         cache,
                              an_ifc_decl_index                decl_idx,
                              a_boolean                        is_data_member,
                              an_ifc_basic_specifiers_bitfield specifiers,
                              an_ifc_object_traits_bitfield    traits,
                              an_ifc_expr_index                alignment,
                              an_ifc_type_index                type,
                              an_ifc_name_index                name,
                              an_ifc_text_offset               raw_name,
                              an_ifc_expr_index                width,
                              an_ifc_expr_index                initializer)
/*
Add the tokens corresponding to the given variable declaration (indexed in the
IFC by decl_idx) to cache.  is_data_member is TRUE if this is a non-static data
member of a class.  specifiers, traits, alignment, and type are values from the
IFC file that describe the variable declaration.  Both name and raw_name
provide the name of the variable - if name is zero, raw_name must be non-zero.
If width is not zero, this is a bitfield and width is its size.  If the
variable has an initializer then initializer is non-zero and refers to the
initializer expression.
*/
{
  auto cache_name_fn = [this, cache, name, raw_name]() {
    if (is_null_index(name)) {
      check_assertion(raw_name != 0);
      cache_identifier(cache, get_string_at_offset(raw_name));
    } else {
      cache_name(cache, name);
    }  /* if */
  };

  if (is_class_scope(get_ifc_home_scope(decl_idx))) {
    auto cache_class_mem_init_fn = [cache, initializer]() {
      if (!is_null_index(initializer)) {
        /* This is an initializer for a member variable of a class.  This is
           already stored in the object file associated with the module TU, and
           is only needed for member variables that are eligible to be used in
           a constant expression.  These initializers may include recursive
           self-references.  Cache a special pseudo-token to indicate that such
           an initializer exists, along with its constant value. */
        cache_token_with_index(cache, tok_pending_ifc_var_init, initializer);
      }  /* if */
      cache_token(cache, tok_semicolon);
    };

    cache_variable_decl(cache, decl_idx, is_data_member, specifiers, traits,
                        alignment, type, cache_name_fn, width,
                        cache_class_mem_init_fn);
  } else {
    auto cache_init_fn = [this, cache, initializer]() {
      if (!is_null_index(initializer)) {
        /* An initializer where the type is ExprSort::Tokens will have the
           braces included as part of the token stream. */
        a_boolean cache_braces = initializer.sort != ifc_es_expr_tokens;
        if (cache_braces) {
          cache_token(cache, tok_lbrace);
        }  /* if */
        cache_expr(cache, initializer, /*cinfo=*/{});
        if (cache_braces) {
          cache_token(cache, tok_rbrace);
        }  /* if */
      }  /* if */
      if (cache->get_last_token()->token != tok_semicolon) {
        /* Add a terminating semicolon, unless one was already added (which can
           happen when the initializer cached by cache_expr above is of kind
           ifc_ExprSort_Tokens). */
        cache_token(cache, tok_semicolon);
      }  /* if */
    };

    cache_variable_decl(cache, decl_idx, is_data_member, specifiers, traits,
                        alignment, type, cache_name_fn, width,
                        cache_init_fn);
  }  /* if */
}  /* cache_variable_decl */


uint32_t an_ifc_module::try_cache_class_attributes_from_body(
                                        a_module_token_cache_ptr cache,
                                        an_ifc_sentence_index    body_sentence)
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
                                      a_module_token_cache_ptr   cache,
                                      const an_ifc_decl_template &decl,
                                      const an_ifc_cache_info    &cinfo)
/*
Add the tokens corresponding to the given template declaration (decl) to cache.
cinfo contains information about the current cache context to help inform
decisions about what to cache.  Return the offset into the template
declaration's body at which to find the definition, or zero if there is no
offset/the offset is not needed.
*/
{
  an_ifc_parameterized_entity entity = get_ifc_entity(decl);
  an_ifc_source_location      locus = get_ifc_locus(decl);
  an_ifc_source_position_hint pos_hint(cache, locus);
  uint32_t                    offset = 0;

  /* Reconstruct the template-head. */
  cache_template_head(cache, get_ifc_chart(decl), cinfo);
  /* FIXME: Handle attributes. */

  an_ifc_type_index type_index = get_ifc_type(decl);
  a_type_kind       type_kind = is_null_index(type_index) ?
                             tk_unknown : type_kind_for_type_index(type_index);
  if (type_kind == tk_class || type_kind == tk_struct ||
      type_kind == tk_union) {
    an_ifc_name_index     name = get_ifc_name(decl);
    an_ifc_sentence_index body = get_ifc_body(entity);

    cache_type(cache, type_index);
    offset = try_cache_class_attributes_from_body(cache, body);
    cache_name(cache, name);
  } else {
    /* Function or variable template. */
    /* FIXME: Cache the entity corresponding to decl->entity.decl instead.
       Currently this may be missing information, so use the soon-to-be-removed
       entity.head instead. */
    cache_sentence(cache, get_ifc_head(entity));
  }  /* if */
  if (!cinfo.no_final_semicolon) {
    cache_token(cache, tok_semicolon);
  }  /* if */
  return offset;
}  /* cache_decl_template_declaration */


void an_ifc_module::cache_decl_template(a_module_token_cache_ptr   cache,
                                        const an_ifc_decl_template &decl,
                                        const an_ifc_cache_info    &cinfo)
/*
Add the tokens corresponding to the given template declaration (decl) to cache.
cinfo contains information about the current cache context to help inform
decisions about what to cache.
*/
{
  an_ifc_sentence_index decl_body = get_ifc_body(get_ifc_entity(decl));
  an_ifc_cache_info     cache_info = cinfo;
  uint32_t              offset;

  /* Flag that the definition is ignored if there's no definition to cache. */
  cache_info.ignore_definition |= decl_body == 0;
  /* If we're caching a definition, the final semicolon must be cached. */
  cache_info.no_final_semicolon = !cache_info.ignore_definition;
  offset = cache_decl_template_declaration(cache, decl, cache_info);
  if (!cache_info.ignore_definition) {
    (void)cache_sentence(cache, decl_body, offset);
  }  /* if */
}  /* cache_decl_template */


static void cache_template_argument_list(a_module_token_cache_ptr cache,
                                         an_ifc_form_spec_index   form_idx)
/*
Add the tokens for the portion of a simple-template-id following the
template-name (i.e., '<' template-argument-listopt '>') via the associated form
spec (form_idx) to the cache.
*/
{
  cache_token(cache, tok_lt);

  /* Load the argument list from the specialization form. */
  Opt<an_ifc_form_spec> opt_ifs;
  construct_node(&opt_ifs, form_idx);
  if (opt_ifs.has_value()) {
    an_ifc_expr_index arg_expr_idx = get_ifc_arguments(*opt_ifs);

    arg_expr_idx.mod->cache_expr(cache, arg_expr_idx, /*cinfo=*/{});
  }  /* if */
  cache_token(cache, tok_gt);
}  /* cache_template_argument_list */


template<typename an_ifc_Node_type>
static void cache_simple_template_id(a_module_token_cache_ptr cache,
                                     const an_ifc_Node_type   &decl)
/*
Add the tokens for a simple-template-id via the associated decl to the cache.
locus is the location of the simple-template-id.
*/
{
  EDG_PREFIX::cache_name(cache, get_ifc_name(decl));
  cache_template_argument_list(cache, get_ifc_form(decl));
}  /* cache_simple_template_id */


static a_boolean validate_is_class_type(an_ifc_type_index type)
/*
If the type is an IFC FundamentalType representing a class, return TRUE;
otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;
  Opt<an_ifc_type_fundamental> opt_fundamental_type;

  construct_node(&opt_fundamental_type, type);
  if (opt_fundamental_type.has_value()) {
    an_ifc_type_fundamental fundamental_type = *opt_fundamental_type;
    an_ifc_type_basis_sort  basis = get_ifc_basis(fundamental_type);

    switch (basis) {
      case ifc_tbs_class:
      case ifc_tbs_struct:
        result = TRUE;
        break;
      default:
        { a_string err_msg("Unexpected ", str_for(basis));

          ifc_unexpected(fundamental_type.get_module(), err_msg);
        }
        break;
    }  /* switch */
  }  /* if */
  return result;
}  /* validate_is_class_type */


void an_ifc_module::cache_decl_partial_specialization(
                             a_module_token_cache_ptr                 cache,
                             an_ifc_decl_index                        decl_idx,
                             const an_ifc_decl_partial_specialization &decl)
/*
Add the tokens corresponding to the given partial specialization declaration
(decl indexed in the IFC by decl_idx) to cache.
*/
{
  an_ifc_decl_index           templated_decl_idx =
                                            get_ifc_decl(get_ifc_entity(decl));
  an_ifc_source_location      locus = get_ifc_locus(decl);
  an_ifc_source_position_hint pos_hint(cache, locus);

  /* Reconstruct the template-head. */
  cache_template_head(cache, get_ifc_chart(decl), /*cinfo=*/{});
  /* Reconstruct the declaration. */
  /* FIXME: Eventually this entire block should be replaceable by a cache_decl
     call (due to problems in the IFC -- namely the templated decl having a
     mangled NameSort Identifier name instead of a NameSort Specialization --
     this is not yet possible). */
  an_ifc_source_position_hint ent_pos_hint(cache, templated_decl_idx);
  switch (templated_decl_idx.sort) {
    case ifc_ds_decl_scope:
      { /* We're reconstructing a class. */
        Opt<an_ifc_decl_scope> opt_ids;

        construct_node(&opt_ids, templated_decl_idx);
        if (!opt_ids.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_scope ids = *opt_ids;
        if (!validate_is_class_type(get_ifc_type(ids))) {
          goto invalid;
        }  /* if */
        /* Reconstruct the templated declaration. */
        auto cache_name_fn = [cache, &decl]() {
          cache_simple_template_id(cache, decl);
        };
        auto cache_scope_fn = [this, cache, &decl]() {
          an_ifc_sentence_index body = get_ifc_body(get_ifc_entity(decl));

          if (body != 0) {
            /* We have a body for this declaration, cache it. */
            cache_sentence(cache, body);
          }  /* if */
        };
        cache_scope_decl(cache, decl_idx, get_ifc_type(ids), cache_name_fn,
                         cache_scope_fn);
      }
      break;
    case ifc_ds_decl_variable:
      { /* We're reconstructing a variable. */
        Opt<an_ifc_decl_variable> opt_idv;

        construct_node(&opt_idv, templated_decl_idx);
        if (!opt_idv.has_value()) {
          goto invalid;
        }  /* if */

        /* Reconstruct the templated declaration. */
        an_ifc_decl_variable idv = *opt_idv;
        auto cache_name_fn = [cache, &decl]() {
          cache_simple_template_id(cache, decl);
        };
        auto cache_init_fn = [this, cache, &decl]() {
          an_ifc_sentence_index body = get_ifc_body(get_ifc_entity(decl));

          if (body != 0) {
            /* We have a body for this declaration, cache it. */
            cache_sentence(cache, body);
          }  /* if */
        };

        cache_variable_decl(cache, decl_idx, /*is_data_member=*/FALSE,
                            get_ifc_specifiers(idv), get_ifc_traits(idv),
                            get_ifc_alignment(idv), get_ifc_type(idv),
                            cache_name_fn, an_ifc_expr_index{}, cache_init_fn);
      }
      break;
    default:
      unexpected_condition_str("Unexpected DeclSort");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad partial specialization cache");
  cache->invalidate();
done:;
}  /* cache_decl_partial_specialization */


void an_ifc_module::cache_decl_specialization(
                                  a_module_token_cache_ptr         cache,
                                  an_ifc_decl_index                decl_idx,
                                  const an_ifc_decl_specialization &decl,
                                  const an_ifc_cache_info          &cinfo)
/*
Add the tokens corresponding to the given specialization declaration (decl
indexed in the IFC by decl_idx) to cache.  cinfo contains information about the
current cache context to help inform decisions about what to cache.
*/
{
  an_ifc_decl_index           templated_decl_idx = get_ifc_decl(decl);
  a_boolean                   is_instantiation =
                                    get_ifc_sort(decl) == ifc_ss_instantiation;
  an_ifc_source_location      locus = get_ifc_locus(decl);
  an_ifc_source_position_hint pos_hint(cache, locus);

  if (is_instantiation) {
    /* If an explicit instantiation appeared in a module definition, that
       instantiation need not be done in client code (other than for inlining
       or constant-evaluation purposes). */
    cache_token(cache, tok_template);
  } else {
    /* Reconstruct the template-head. */
    cache_template_head(cache, an_ifc_chart_index{}, cinfo);
  }  /* if */
  /* Reconstruct the declaration. */
  /* FIXME: Eventually this entire block should be replaceable by a
     cache_decl call (due to problems in the IFC -- namely the templated decl
     having a mangled NameSort Identifier name instead of a NameSort
     Specialization -- this is not yet possible). */
  /* Read the partition for the templated declaration. */
  an_ifc_source_position_hint ent_pos_hint(cache, templated_decl_idx);
  switch (templated_decl_idx.sort) {
    case ifc_ds_decl_scope:
      { /* We're reconstructing a class. */
        Opt<an_ifc_decl_scope> opt_ids;

        construct_node(&opt_ids, templated_decl_idx);
        if (!opt_ids.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_scope ids = *opt_ids;
        if (!validate_is_class_type(get_ifc_type(ids))) {
          goto invalid;
        }  /* if */
        /* Reconstruct the templated declaration. */
        auto cache_name_fn = [cache, &decl]() {
          cache_simple_template_id(cache, decl);
        };

        if (is_instantiation) {
          auto cache_scope_fn = [cache]() {
            cache_token(cache, tok_semicolon);
          };

          cache_scope_decl(cache, decl_idx, get_ifc_type(ids), cache_name_fn,
                           cache_scope_fn);
        } else {
          auto cache_scope_fn = [this, cache, &cinfo, &ids]() {
            if (!cinfo.ignore_definition) {
              an_ifc_type_index base = get_ifc_base(ids);

              /* If there are bases specified, cache the bases. */
              if (!is_null_index(base)) {
                cache_token(cache, tok_colon);
                cache_type(cache, base);
              }  /* if */
              cache_scope(this, cache, get_ifc_initializer(ids));
            }  /* if */
            cache_token(cache, tok_semicolon);
          };

          cache_scope_decl(cache, decl_idx, get_ifc_type(ids), cache_name_fn,
                           cache_scope_fn);
        }  /* if */
      }
      break;
    case ifc_ds_decl_variable:
      { /* We're reconstructing a variable. */
        Opt<an_ifc_decl_variable> opt_idv;

        construct_node(&opt_idv, templated_decl_idx);
        if (!opt_idv.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_variable idv = *opt_idv;
        /* Reconstruct the templated declaration. */
        auto cache_name_fn = [cache, &decl]() {
          cache_simple_template_id(cache, decl);
        };
        auto cache_spec_init_fn = [this, cache, &cinfo, &idv]() {
          if (!cinfo.ignore_definition) {
            an_ifc_expr_index initializer = get_ifc_initializer(idv);

            if (!is_null_index(initializer)) {
              cache_token(cache, tok_lparen);
              cache_expr(cache, initializer, cinfo);
              cache_token(cache, tok_rparen);
            }  /* if */
          }  /* if */
          cache_token(cache, tok_semicolon);
        };
        auto cache_inst_init_fn = [cache]() {
          cache_token(cache, tok_semicolon);
        };

        an_ifc_decl_index scope_ref = get_ifc_home_scope(idv);
        /* Disable spurious GCC warning about uninitialized usage of
           opt_scope_ref). */
BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
        if (is_instantiation) {
          cache_variable_decl(cache, decl_idx, is_class_scope(scope_ref),
                              get_ifc_specifiers(idv), get_ifc_traits(idv),
                              get_ifc_alignment(idv), get_ifc_type(idv),
                              cache_name_fn, an_ifc_expr_index{},
                              cache_inst_init_fn);
        } else {
          cache_variable_decl(cache, decl_idx, is_class_scope(scope_ref),
                              get_ifc_specifiers(idv), get_ifc_traits(idv),
                              get_ifc_alignment(idv), get_ifc_type(idv),
                              cache_name_fn, an_ifc_expr_index{},
                              cache_spec_init_fn);
        }  /* if */
END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED
      }
      break;
    case ifc_ds_decl_function:
      { /* We're reconstructing a function. */
        Opt<an_ifc_decl_function> opt_idf;

        construct_node(&opt_idf, templated_decl_idx);
        if (!opt_idf.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_function idf = *opt_idf;
        this->cache_attrs(cache, templated_decl_idx);
        cache_func_vendor_decl_specifier_seq(cache, templated_decl_idx);
        cache_func_decl_specifier_seq(cache, idf);
        cache_func_return_type(cache, idf);
        cache_func_calling_convention(cache, idf);
        cache_simple_template_id(cache, decl);
        cache_func_parameters_and_qualifiers(cache, templated_decl_idx, idf,
                                             cinfo);
        cache_func_body_or_end_decl(cache, templated_decl_idx, idf, cinfo);
      }
      break;
    case ifc_ds_decl_method:
      { /* We're reconstructing a method. */
        Opt<an_ifc_decl_method> opt_idm;

        construct_node(&opt_idm, templated_decl_idx);
        if (!opt_idm.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_method idm = *opt_idm;
        this->cache_attrs(cache, templated_decl_idx);
        cache_func_vendor_decl_specifier_seq(cache, templated_decl_idx);
        cache_func_decl_specifier_seq(cache, idm);

        an_ifc_name_index name_idx = get_ifc_name(idm);
        if (name_idx.sort != ifc_ns_name_conversion) {
          cache_func_return_type(cache, idm);
        }  /* if */
        cache_func_calling_convention(cache, idm);
        cache_simple_template_id(cache, decl);
        cache_func_parameters_and_qualifiers(cache, templated_decl_idx, idm,
                                             cinfo);
        cache_func_virt_specifier_seq(cache, idm);
        cache_func_body_or_end_decl(cache, templated_decl_idx, idm, cinfo);
      }
      break;
    case ifc_ds_decl_constructor:
      { /* We're reconstructing a constructor. */
        Opt<an_ifc_decl_constructor> opt_idc;

        construct_node(&opt_idc, templated_decl_idx);
        if (!opt_idc.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_decl_constructor idc = *opt_idc;
        this->cache_attrs(cache, templated_decl_idx);
        cache_func_vendor_decl_specifier_seq(cache, templated_decl_idx);
        cache_func_decl_specifier_seq(cache, idc);
        cache_func_calling_convention(cache, idc);
        /* Constructor specializations are special cases that don't include
           the template argument list.  Call the lower level cache_name
           instead of cache_simple_template_id. */
        EDG_PREFIX::cache_name(cache, get_ifc_name(decl));
        cache_func_parameters_and_qualifiers(cache, templated_decl_idx, idc,
                                             cinfo);
        cache_func_virt_specifier_seq(cache, idc);
        cache_func_body_or_end_decl(cache, templated_decl_idx, idc, cinfo);
      }
      break;
    default:
      unexpected_condition_str("Unexpected DeclSort");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad specialization cache");
  cache->invalidate();
done:;
}  /* cache_decl_specialization */


void an_ifc_module::cache_type_param_introducer(
                                           a_module_token_cache_ptr cache,
                                           an_ifc_expr_index        constraint,
                                           a_boolean                is_pack)
/*
cache is the token cache to update.  constraint is zero for unconstrained
parameters or refers to a concept-id expression otherwise.  In the former case,
add a "typename" token to introduce a template parameter, but in the latter
case emit the concept-id.  is_pack is TRUE if the type introducer represents a
parameter pack, FALSE otherwise.
*/
{
  if (is_null_index(constraint)) {
    /* An unconstrained type parameter is introduced by the "typename"
       keyword (or "class", but we'll use "typename"). */
    cache_token(cache, tok_typename);
  } else {
    cache_expr(cache, constraint, /*cinfo=*/{});
  }  /* if */
  if (is_pack) {
    cache_token(cache, tok_ellipsis);
  }  /* if */
}  /* cache_type_param_introducer */


template<typename a_Cache_fn>
static inline void cache_attr_fn(a_module_token_cache_ptr cache,
                                 a_Cache_fn               cache_fn)
/*
Helper function for cache_attr to avoid code duplication for the brackets.
Add tokens for the leading and trailing attribute brackets to cache and
call the provided cache_fn to cache the actual attribute, in the appropriate
places.
*/
{
  cache_token(cache, tok_lbracket);
  cache_token(cache, tok_lbracket);
  cache_fn();
  cache_token(cache, tok_rbracket);
  cache_token(cache, tok_rbracket);
}  /* cache_attr_fn */


void an_ifc_module::cache_attr(a_module_token_cache_ptr cache,
                               an_ifc_attr_index        attr,
                               a_boolean                cache_brackets)
/*
Add the tokens corresponding to the given attribute (attr) to cache.  If
cache_brackets is TRUE, include the attribute brackets.  Otherwise, the caller
is responsible for ensuring that the brackets are cached appropriately.
*/
{
  switch (attr.sort) {
    case ifc_as_attr_nothing:
      if (cache_brackets) {
        cache_attr_fn(cache, [](){});
      }  /* if */
      break;
    case ifc_as_attr_basic:
      { Opt<an_ifc_attr_basic> opt_iab;

        construct_node(&opt_iab, attr);
        if (!opt_iab.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_attr_basic           iab = *opt_iab;
        an_ifc_nestable_word        word = get_ifc_word(iab);
        an_ifc_source_location      locus = get_ifc_locus(word);
        an_ifc_source_position_hint pos_hint(cache, locus);
        auto cache_fn = [cache, &word, this]() {
          cache_word(cache, word);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_as_attr_scoped:
      { Opt<an_ifc_attr_scoped> opt_ias;

        construct_node(&opt_ias, attr);
        if (!opt_ias.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_attr_scoped          ias = *opt_ias;
        an_ifc_nestable_word        scope = get_ifc_scope(ias);
        an_ifc_nestable_word        member = get_ifc_member(ias);
        an_ifc_source_location      locus = get_ifc_locus(scope);
        an_ifc_source_position_hint pos_hint(cache, locus);
        auto cache_fn = [cache, &scope, &member, this]() {
          cache_word(cache, scope);
          cache_token(cache, tok_colon_colon);
          cache_word(cache, member);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_as_attr_labeled:
      { Opt<an_ifc_attr_labeled> opt_ial;

        construct_node(&opt_ial, attr);
        if (!opt_ial.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_attr_labeled         ial = *opt_ial;
        an_ifc_nestable_word        label = get_ifc_label(ial);
        an_ifc_attr_index           attribute = get_ifc_attribute(ial);
        an_ifc_source_location      locus = get_ifc_locus(label);
        an_ifc_source_position_hint pos_hint(cache, locus);
        auto cache_fn = [cache, &label, attribute, this]() {
          cache_word(cache, label);
          cache_token(cache, tok_colon);
          cache_attr(cache, attribute, /*cache_brackets=*/FALSE);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_as_attr_called:
      { Opt<an_ifc_attr_called> opt_iac;

        construct_node(&opt_iac, attr);
        if (!opt_iac.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_attr_called iac = *opt_iac;
        auto cache_fn = [cache, &iac, this]() {
          cache_attr(cache, get_ifc_function(iac), /*cache_brackets=*/FALSE);
          cache_token(cache, tok_lparen);
          cache_attr(cache, get_ifc_arguments(iac), /*cache_brackets=*/FALSE);
          cache_token(cache, tok_rparen);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_as_attr_expanded:
      { Opt<an_ifc_attr_expanded> opt_iae;

        construct_node(&opt_iae, attr);
        if (!opt_iae.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_attr_expanded iae = *opt_iae;
        auto cache_fn = [cache, &iae, this]() {
          cache_attr(cache, get_ifc_operand(iae), /*cache_brackets=*/FALSE);
          cache_token(cache, tok_ellipsis);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_as_attr_factored:
      { Opt<an_ifc_attr_factored> opt_iaf;

        construct_node(&opt_iaf, attr);
        if (!opt_iaf.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_attr_factored        iaf = *opt_iaf;
        an_ifc_nestable_word        factor = get_ifc_factor(iaf);
        an_ifc_attr_index           terms = get_ifc_terms(iaf);
        an_ifc_source_location      locus = get_ifc_locus(factor);
        an_ifc_source_position_hint pos_hint(cache, locus);
        auto cache_fn = [this, cache, &factor, &terms]() {
          cache_token(cache, tok_using);
          cache_word(cache, factor);
          cache_token(cache, tok_colon);
          cache_attr(cache, terms, /*cache_brackets=*/FALSE);
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_as_attr_elaborated:
      { Opt<an_ifc_attr_elaborated> opt_iae;

        construct_node(&opt_iae, attr);
        if (!opt_iae.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_attr_elaborated iae = *opt_iae;
        auto cache_fn = [cache, &iae, this]() {
          cache_expr(cache, get_ifc_expression(iae), /*cinfo=*/{});
        };
        if (cache_brackets) {
          cache_attr_fn(cache, cache_fn);
        } else {
          cache_fn();
        }  /* if */
      }
      break;
    case ifc_as_attr_tuple:
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
      { Opt<an_ifc_attr_tuple> opt_iat;

        construct_node(&opt_iat, attr);
        if (!opt_iat.has_value()) {
          goto invalid;
        }  /* if */

        a_attr_heap_traverser traverser(*opt_iat);
        /* Retrieve the attribute indexes from the attribute heap, then recurse
           to process the attributes at the retrieved indexes. */
        for (an_Indexed<an_ifc_heap_attr> indexed_iha : traverser) {
          if (!indexed_iha.has_value()) {
            goto invalid;
          }  /* if */
          cache_attr(cache, get_ifc_value(*indexed_iha),
                     /*cache_brackets=*/TRUE);
        }  /* for */
      }
      break;
    default_is_unexpected_str("Unexpected AttrSort");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad attr cache");
  cache->invalidate();
done:;
}  /* cache_attr */


void an_ifc_module::cache_attrs(a_module_token_cache_ptr cache,
                                an_ifc_decl_index        decl_idx)
/*
Add the tokens corresponding to the attributes of the given declaration at
decl_idx to the cache.
*/
{
  an_ifc_attr_index attr_idx = attr_index_of(decl_idx);

  if (!is_null_index(attr_idx)) {
    cache_attr(cache, attr_idx, /*cache_brackets=*/TRUE);
  }  /* if */
}  /* cache_attrs */


void an_ifc_module::cache_template_head(a_module_token_cache_ptr cache,
                                        an_ifc_chart_index       chart_idx,
                                        const an_ifc_cache_info  &cinfo)
/*
Add the tokens corresponding to the template head described by the given
chart_idx to the cache.  cinfo contains information about the current cache
context to help inform decisions about what to cache.
*/
{
  cache_token(cache, tok_template);
  if (is_null_index(chart_idx)) {
    cache_token(cache, tok_lt);
    cache_token(cache, tok_gt);
  } else {
    cache_chart(cache, chart_idx, cinfo);
  }  /* if */
}  /* cache_template_head */


void an_ifc_module::cache_decl(a_module_token_cache_ptr cache,
                               an_ifc_decl_index        decl,
                               const an_ifc_cache_info  &cinfo)
/*
Add the tokens corresponding to the given declaration (decl) to cache.  cinfo
contains information about the current cache context to help inform decisions
about what to cache.
*/
{
  an_ifc_source_position_hint pos_hint(cache, decl);

  /* Pre-validate so that access specifiers can be grabbed by the visitor
     and cached ahead of the declaration when relevant. */
  if (!validate(decl)) {
    goto invalid;
  }  /* if */
  /* Cache the access specifier if in class scope and access information is
     provided. */
  if (!cinfo.no_access_specifier && has_ifc_access(decl)) {
    an_ifc_access_sort access = get_ifc_access(decl);

    if (has_ifc_home_scope(decl) && is_class_scope(get_ifc_home_scope(decl))) {
      cache_access_specifier(cache, access);
      cache_token(cache, tok_colon);
    }  /* if */
  }  /* if */
  switch (decl.sort) {
    case ifc_ds_decl_vendor_extension:
    case ifc_ds_decl_explicit_instantiation:
    case ifc_ds_decl_explicit_specialization:
      issue_unsupported_construct_error(this, str_for(decl.sort),
                                        &error_position);
      goto invalid;
    case ifc_ds_decl_enumerator:
      { an_ifc_decl_enumerator enumerator_decl;

        construct_node_prechecked(&enumerator_decl, decl);
        if (!cache_direct_decl(cache, enumerator_decl, cinfo)) {
          goto invalid;
        }  /* if */
      }
      break;
    case ifc_ds_decl_variable:
      { an_ifc_decl_variable idv;

        construct_node_prechecked(&idv, decl);
        cache_variable_decl(cache, decl, /*is_data_member=*/FALSE,
                            get_ifc_specifiers(idv), get_ifc_traits(idv),
                            get_ifc_alignment(idv), get_ifc_type(idv),
                            get_ifc_name(idv), an_ifc_text_offset{},
                            an_ifc_expr_index{}, get_ifc_initializer(idv));
      }
      break;
    case ifc_ds_decl_parameter:
      { an_ifc_decl_parameter decl_parameter;

        construct_node_prechecked(&decl_parameter, decl);
        if (!cache_direct_decl(cache, decl_parameter, cinfo)) {
          goto invalid;
        }  /* if */
      }
      break;
    case ifc_ds_decl_field:
      { an_ifc_decl_field idf;

        construct_node_prechecked(&idf, decl);

        an_ifc_basic_specifiers_bitfield specifiers = get_ifc_specifiers(idf);
        check_assertion(!test_bitmask<ifc_bsb_c>(specifiers));
        cache_variable_decl(cache, decl, /*is_data_member=*/TRUE, specifiers,
                            get_ifc_traits(idf), get_ifc_alignment(idf),
                            get_ifc_type(idf), an_ifc_name_index{},
                            get_ifc_name(idf), an_ifc_expr_index{},
                            get_ifc_initializer(idf));
      }
      break;
    case ifc_ds_decl_bitfield:
      { an_ifc_decl_bitfield idbf;

        construct_node_prechecked(&idbf, decl);

        an_ifc_basic_specifiers_bitfield specifiers = get_ifc_specifiers(idbf);
        an_ifc_expr_index                width = get_ifc_width(idbf);
        check_assertion(!test_bitmask<ifc_bsb_c>(specifiers));
        check_assertion(!is_null_index(width));
        cache_variable_decl(cache, decl, /*is_data_member=*/TRUE, specifiers,
                            get_ifc_traits(idbf), an_ifc_expr_index{},
                            get_ifc_type(idbf), an_ifc_name_index{},
                            get_ifc_name(idbf), width,
                            get_ifc_initializer(idbf));
      }
      break;
    case ifc_ds_decl_scope:
      { an_ifc_decl_scope ids;

        construct_node_prechecked(&ids, decl);
        cache_scope_decl(cache, decl, get_ifc_type(ids), get_ifc_name(ids),
                         get_ifc_base(ids), get_ifc_initializer(ids));
      }
      break;
    case ifc_ds_decl_enumeration:
      { an_ifc_decl_enumeration ide;

        construct_node_prechecked(&ide, decl);

        an_ifc_type_index            type = get_ifc_type(ide);
        Opt<an_ifc_type_fundamental> opt_itf;
        construct_node(&opt_itf, type);
        if (!opt_itf.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_type_fundamental itf = *opt_itf;
        an_ifc_expr_index       alignment = get_ifc_alignment(ide);
        an_ifc_type_index       base = get_ifc_base(ide);
        cache_basic_specifiers(cache, get_ifc_specifiers(ide));

        an_ifc_type_basis_sort basis = get_ifc_basis(itf);
        switch (basis) {
          case ifc_tbs_enum:
            cache_token(cache, tok_enum);
            break;
          case ifc_tbs_struct:
            cache_token(cache, tok_enum_struct);
            break;
          case ifc_tbs_class:
            cache_token(cache, tok_enum_class);
            break;
          default:
            { a_string err_msg("Unexpected ", str_for(basis));

              ifc_unexpected(this, err_msg);
            }
            goto invalid;
        }  /* switch */
        if (!is_null_index(alignment)) {
          cache_token(cache, tok_alignas);
          cache_token(cache, tok_lparen);
          cache_expr(cache, alignment, cinfo);
          cache_token(cache, tok_rparen);
        }  /* if */
        cache_identifier(cache, get_string_at_offset(get_ifc_name(ide)));
        if (!is_null_index(base)) {
          cache_token(cache, tok_colon);
          cache_type(cache, base);
        }  /* if */

        an_ifc_sequence    initializer = get_ifc_initializer(ide);
        an_ifc_cardinality cardinality = get_ifc_cardinality(initializer);
        if (cardinality != 0) {
          a_decl_enumerator_traverser traverser(initializer);

          cache_token(cache, tok_lbrace);
          for (an_Indexed<an_ifc_decl_enumerator> indexed_iden : traverser) {
            if (!indexed_iden.has_value()) {
              goto invalid;
            }  /* if */
            if (!is_first(traverser, indexed_iden)) {
              cache_token(cache, tok_comma);
            }  /* if */
            if (!cache_direct_decl(cache, *indexed_iden, cinfo)) {
              goto invalid;
            }  /* if */
          }  /* for */
          cache_token(cache, tok_rbrace);
        }  /* if */
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ds_decl_alias:
      { an_ifc_decl_alias ida;

        construct_node_prechecked(&ida, decl);

        an_ifc_type_index type = get_ifc_type(ida);
        if (type.sort == ifc_ts_type_fundamental) {
          Opt<an_ifc_type_fundamental> opt_itf;

          construct_node(&opt_itf, type);
          if (!opt_itf.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_fundamental itf = *opt_itf;
          an_ifc_type_basis_sort  basis = get_ifc_basis(itf);
          if (basis == ifc_tbs_typename) {
            cache_token(cache, tok_using);
          } else {
            check_assertion(basis == ifc_tbs_namespace);
            cache_token(cache, tok_namespace);
          }  /* if */
          cache_identifier(cache, get_string_at_offset(get_ifc_name(ida)));
          cache_token(cache, tok_assign);
          cache_type(cache, get_ifc_aliasee(ida));
        } else if (type.sort == ifc_ts_type_forall) {
          Opt<an_ifc_type_forall> opt_itf;

          construct_node(&opt_itf, get_ifc_aliasee(ida));
          if (!opt_itf.has_value()) {
            goto invalid;
          }  /* if */

          an_ifc_type_forall itf = *opt_itf;
          check_assertion(get_ifc_aliasee(ida).sort == ifc_ts_type_forall);
          cache_token(cache, tok_template);
          cache_chart(cache, get_ifc_chart(itf), cinfo);
          cache_token(cache, tok_using);
          cache_identifier(cache, get_string_at_offset(get_ifc_name(ida)));
          cache_token(cache, tok_assign);
          cache_type(cache, get_ifc_subject(itf));
        } else {
          a_string err_msg("Unexpected ", str_for(type.sort));

          ifc_unexpected(this, err_msg);
          goto invalid;
        }  /* if */
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ds_decl_temploid:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "DeclSort::Temploid",
                                        &error_position);
      goto invalid;
    case ifc_ds_decl_template:
      { an_ifc_decl_template template_decl;

        construct_node_prechecked(&template_decl, decl);
        cache_decl_template(cache, template_decl, cinfo);

        Opt<an_ifc_sequence> opt_seq =
                                  get_specialization_sequence_from_trait(decl);
        if (opt_seq.has_value()) {
          /* Cache the associated specializations. */
          an_ifc_decl_index home_scope = get_ifc_home_scope(template_decl);

          cache_scope_member_sequence(cache, home_scope, *opt_seq);
        }  /* if */
      }
      break;
    case ifc_ds_decl_partial_specialization:
      { an_ifc_decl_partial_specialization partial_spec_decl;

        construct_node_prechecked(&partial_spec_decl, decl);
        cache_decl_partial_specialization(cache, decl, partial_spec_decl);
      }
      break;
    case ifc_ds_decl_specialization:
      { an_ifc_decl_specialization spec_decl;

        construct_node_prechecked(&spec_decl, decl);
        cache_decl_specialization(cache, decl, spec_decl, cinfo);
      }
      break;
    case ifc_ds_decl_concept:
      { an_ifc_decl_concept concept_decl;

        construct_node_prechecked(&concept_decl, decl);
        if (!cache_direct_decl(cache, concept_decl, cinfo)) {
          goto invalid;
        }  /* if */
      }
      break;
    case ifc_ds_decl_function:
      { an_ifc_decl_function idf;

        construct_node_prechecked(&idf, decl);
        this->cache_attrs(cache, decl);
        cache_func_vendor_decl_specifier_seq(cache, decl);
        if (is_class_scope(get_ifc_home_scope(decl))) {
          cache_token(cache, tok_static);
        }  /* if */
        cache_func_decl_specifier_seq(cache, idf);
        cache_func_return_type(cache, idf);
        cache_func_calling_convention(cache, idf);
        cache_name(cache, get_ifc_name(idf));
        cache_func_parameters_and_qualifiers(cache, decl, idf, cinfo);
        cache_func_body_or_end_decl(cache, decl, idf, cinfo);
      }
      break;
    case ifc_ds_decl_method:
      { an_ifc_decl_method idm;

        construct_node_prechecked(&idm, decl);
        this->cache_attrs(cache, decl);
        cache_func_vendor_decl_specifier_seq(cache, decl);
        cache_func_decl_specifier_seq(cache, idm);

        an_ifc_name_index name_idx = get_ifc_name(idm);
        if (name_idx.sort != ifc_ns_name_conversion) {
          cache_func_return_type(cache, idm);
        }  /* if */
        cache_func_calling_convention(cache, idm);
        cache_name(cache, name_idx);
        cache_func_parameters_and_qualifiers(cache, decl, idm, cinfo);
        cache_func_virt_specifier_seq(cache, idm);
        cache_func_body_or_end_decl(cache, decl, idm, cinfo);
      }
      break;
    case ifc_ds_decl_constructor:
      { an_ifc_decl_constructor idc;

        construct_node_prechecked(&idc, decl);
        this->cache_attrs(cache, decl);
        cache_func_vendor_decl_specifier_seq(cache, decl);
        cache_func_decl_specifier_seq(cache, idc);
        cache_func_calling_convention(cache, idc);
        cache_name(cache, get_ifc_name(idc));
        cache_func_parameters_and_qualifiers(cache, decl, idc, cinfo);
        cache_func_virt_specifier_seq(cache, idc);
        cache_func_body_or_end_decl(cache, decl, idc, cinfo);
      }
      break;
    case ifc_ds_decl_inherited_constructor:
      { an_ifc_decl_inherited_constructor idic;

        construct_node_prechecked(&idic, decl);

        an_ifc_decl_index base_ctor = get_ifc_base_ctor(idic);
        cache_token(cache, tok_using);
        cache_name_from_decl(cache, base_ctor);
        cache_token(cache, tok_colon_colon);
        cache_name_from_decl(cache, base_ctor);
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ds_decl_destructor:
      { an_ifc_decl_destructor idd;

        construct_node_prechecked(&idd, decl);
        this->cache_attrs(cache, decl);
        cache_func_vendor_decl_specifier_seq(cache, decl);
        cache_func_decl_specifier_seq(cache, idd);
        cache_func_calling_convention(cache, idd);
        cache_token(cache, tok_compl);
        cache_name(cache, get_ifc_name(idd));
        cache_token(cache, tok_lparen);
        cache_token(cache, tok_rparen);
        cache_func_noexcept_specifier(cache, idd);
        cache_func_virt_specifier_seq(cache, idd);
        cache_func_body_or_end_decl(cache, decl, idd, cinfo);
      }
      break;
    case ifc_ds_decl_reference:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "DeclSort::Reference",
                                        &error_position);
      goto invalid;
    case ifc_ds_decl_using_declaration:
      { an_ifc_decl_using_declaration idud;

        construct_node_prechecked(&idud, decl);

        an_ifc_expr_index parent = get_ifc_parent(idud);
        cache_basic_specifiers(cache, get_ifc_specifiers(idud));
        cache_token(cache, tok_using);
        if (!is_null_index(parent)) {
          cache_expr(cache, parent, /*cinfo=*/{});
          cache_token(cache, tok_colon_colon);
        }  /* if */
        cache_identifier(cache, get_string_at_offset(get_ifc_name(idud)));
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ds_decl_using_directive:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "DeclSort::UsingDirective",
                                        &error_position);
      goto invalid;
    case ifc_ds_decl_friend:
      /* A friend declaration.  IFC encodes the friend as an expression that
         refers to the friend entity (normally, ifc_es_expr_named_decl or
         ifc_es_expr_template_id).  Rather than trying to reconstruct a friend
         declaration that resolves to the construct, we record the IFC
         expression information for later processing by class member
         declaration parsing.  The pseudo-declaration is of the form
              friend <ifc-entity-ref> ;
         where ifc-entity-ref is a pseudo-token that carries the IFC expression
         index. */
      { an_ifc_decl_friend friend_decl;

        construct_node_prechecked(&friend_decl, decl);

        an_ifc_expr_index           friend_id = get_ifc_entity(friend_decl);
        an_ifc_source_position_hint friend_pos(cache, friend_id);
        cache_token(cache, tok_friend);
        cache_token_with_index(cache, tok_ifc_entity_ref, friend_id);
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ds_decl_expansion:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "DeclSort::Expansion",
                                        &error_position);
      goto invalid;
    case ifc_ds_decl_deduction_guide:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "DeclSort::DeductionGuide",
                                        &error_position);
      goto invalid;
    case ifc_ds_decl_barren:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "DeclSort::Barren",
                                        &error_position);
      goto invalid;
    case ifc_ds_decl_tuple:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "DeclSort::Tuple",
                                        &error_position);
      goto invalid;
    case ifc_ds_decl_syntax_tree:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "DeclSort::SyntaxTree",
                                        &error_position);
      goto invalid;
    case ifc_ds_decl_intrinsic:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "DeclSort::Intrinsic",
                                        &error_position);
      goto invalid;
    case ifc_ds_decl_property:
      { an_ifc_decl_property idp;

        construct_node_prechecked(&idp, decl);
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
        cache_identifier(cache, get_string_at_offset(get_ifc_getter(idp)),
                         &pos);
        cache_token(cache, tok_comma, &pos);
        cache_identifier(cache, "put", &pos);
        cache_token(cache, tok_assign, &pos);
        cache_identifier(cache, get_string_at_offset(get_ifc_setter(idp)),
                         &pos);
        cache_token(cache, tok_rparen, &pos);
        cache_token(cache, tok_rparen, &pos);
#endif /* 0 */
        cache_decl(cache, get_ifc_member(idp), cinfo);
      }
      break;
    case ifc_ds_decl_output_segment:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "DeclSort::OutputSegment",
                                        &error_position);
      goto invalid;
    default_is_unexpected_str("Unexpected DeclSort");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad decl cache");
  cache->invalidate();
done:;
}  /* cache_decl */


inline void an_ifc_module::update_name_qualification_suppression(
                                                   const an_ifc_expr_path &iep)
/*
An internal method for setting any necessary automatic nested name specifier
qualification suppression flags based on the given path.

As part of the contract for this function, the caller is responsible for
restoring previous state of the potentially affected suppression flags.
Currently this includes the variable(s):
suppress_automatic_namespace_qualification.
*/
{
  an_ifc_expr_index scope = get_ifc_scope(iep);

  if (scope.sort == ifc_es_expr_named_decl) {
    Opt<an_ifc_expr_named_decl> opt_iend;

    construct_node(&opt_iend, scope);
    if (!opt_iend.has_value()) {
      goto invalid;
    }  /* if */

    an_ifc_expr_named_decl iend = *opt_iend;
    an_ifc_decl_index      resolution = get_ifc_resolution(iend);
    if (resolution.sort == ifc_ds_decl_scope) {
      Opt<an_ifc_decl_scope> opt_ids;

      construct_node(&opt_ids, resolution);
      if (!opt_ids.has_value()) {
        goto invalid;
      }  /* if */

      an_ifc_decl_scope            ids = *opt_ids;
      an_ifc_type_index            type = get_ifc_type(ids);
      Opt<an_ifc_type_fundamental> opt_itf;
      construct_node(&opt_itf, type);
      if (!opt_itf.has_value()) {
        goto invalid;
      }  /* if */

      an_ifc_type_fundamental itf = *opt_itf;
      an_ifc_type_basis_sort  basis = get_ifc_basis(itf);
      switch (basis) {
        case ifc_tbs_namespace:
          /* This path already contains its namespace qualification, suppress
             automatic namespace qualification. */
          suppress_automatic_namespace_qualification = TRUE;
          break;
        case ifc_tbs_class:
        case ifc_tbs_struct:
        case ifc_tbs_union:
          /* This path already contains its class qualification.  Suppress any
             further automatic name qualification. */
          suppress_automatic_name_qualification = TRUE;
          break;
        default:
          break;
      }  /* switch */
    }  /* if */
  }  /* if */
invalid:;
}  /* update_name_qualification_suppression */


static void cache_args_with_parens(a_module_token_cache_ptr cache,
                                   an_ifc_expr_index        args,
                                   const an_ifc_cache_info  &cinfo)
/*
Record tokens for the IFC expression described by args in the given cache and
enclose them with parentheses.  If args is an IFC ExpressionList, be sure to
avoid double parentheses.  cinfo contains the context associated with the
expression being cached.
*/
{
  if (args.sort != ifc_es_expr_expression_list) {
    cache_token(cache, tok_lparen);
  }  /* if */
  args.mod->cache_expr(cache, args, cinfo);
  if (args.sort != ifc_es_expr_expression_list) {
    cache_token(cache, tok_rparen);
  }  /* if */
}  /* cache_args_with_parens */


void an_ifc_module::cache_expr(a_module_token_cache_ptr cache,
                               an_ifc_expr_index        expr,
                               const an_ifc_cache_info  &cinfo)
/*
Add the tokens corresponding to the given expression (expr) to cache.  cinfo
contains information about the current cache context to help inform decisions
about what to cache.  For example, if cinfo.qualified_name is TRUE, separate
tuple elements by '::' instead of ','.
*/
{
  an_ifc_source_position_hint pos_hint(cache, expr);

  switch (expr.sort) {
    case ifc_es_expr_vendor_extension:
    case ifc_es_expr_delete:
    case ifc_es_expr_new:
    case ifc_es_expr_label:
      issue_unsupported_construct_error(this, str_for(expr.sort),
                                        &error_position);
      goto invalid;
    case ifc_es_expr_empty:
      /* Nothing to cache here - literally an empty expression. */
      break;
    case ifc_es_expr_literal:
      { Opt<an_ifc_expr_literal> opt_iel;

        construct_node(&opt_iel, expr);
        if (!opt_iel.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_literal iel = *opt_iel;
        an_ifc_type_index   type = get_ifc_type(iel);
        if (type.sort == ifc_ts_type_designated) {
          /* FIXME: This can show up as a literal type in some cases, but isn't
             really a literal in the sense that there's a constant to cache. */
          cache_type(cache, type);
        } else {
          a_constant_ptr cp = constant_for_expr_index(expr,
                                                      /*default_type=*/NULL);

          cache_literal(this, cache, cp);
        }  /* if */
      }
      break;
    case ifc_es_expr_lambda:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::Lambda",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_type:
      { Opt<an_ifc_expr_type> opt_iet;

        construct_node(&opt_iet, expr);
        if (!opt_iet.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_type iet = *opt_iet;
        cache_type(cache, get_ifc_denotation(iet));
      }
      break;
    case ifc_es_expr_named_decl:
      { Opt<an_ifc_expr_named_decl> opt_iend;

        construct_node(&opt_iend, expr);
        if (!opt_iend.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_named_decl iend = *opt_iend;
        an_ifc_decl_index      resolution = get_ifc_resolution(iend);
        if (!validate(resolution)) {
          goto invalid;
        }  /* if */
        if (is_name_qualifiable(resolution)) {
          cache_qualified_name_from_decl(cache, resolution);
        } else {
          /* We are contextually forbidden from qualifying this name or the
             declaration type otherwise is considered to never appear
             with a qualified name.

             This can happen when building an unqualified-id within a dependent
             context. */
          cache_name_from_decl(cache, resolution);
        }  /* if */
      }
      break;
    case ifc_es_expr_unresolved_id:
      { Opt<an_ifc_expr_unresolved_id> opt_ieui;

        construct_node(&opt_ieui, expr);
        if (!opt_ieui.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_unresolved_id ieui = *opt_ieui;
        cache_name(cache, get_ifc_name(ieui));
      }
      break;
    case ifc_es_expr_template_id:
      { Opt<an_ifc_expr_template_id> opt_ieti;

        construct_node(&opt_ieti, expr);
        if (!opt_ieti.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_template_id ieti = *opt_ieti;
        an_ifc_expr_index       arguments = get_ifc_arguments(ieti);
        cache_expr(cache, get_ifc_primary(ieti), cinfo);
        cache_token(cache, tok_lt);
        if (!is_null_index(arguments)) {
          cache_expr(cache, arguments, cinfo);
        }  /* if */
        cache_token(cache, tok_gt);
      }
      break;
    case ifc_es_expr_unqualified_id:
      { Opt<an_ifc_expr_unqualified_id> opt_ieui;

        construct_node(&opt_ieui, expr);
        if (!opt_ieui.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_unqualified_id ieui = *opt_ieui;
        an_ifc_expr_index          resolution = get_ifc_resolution(ieui);
        an_ifc_name_index          name = get_ifc_name(ieui);
        if (get_ifc_line(get_ifc_template_keyword(ieui)) != 0) {
          cache_token(cache, tok_template);
        }  /* if */
        /* It's possible to have this with no name/resolution at all.  For
           example, if a construct of the form "::foo::bar" has been encoded,
           the first element of the ExprSort::Tuple will refer to the empty
           string that precedes the first "::". */
        if (!is_null_index(resolution)) {
          /* Note: Do not suppress automatic name qualification here, as the
             resolution can contain a qualified identifier, despite this being
             an "unqualified" identifier. */
          cache_expr(cache, resolution, cinfo);
        } else if (!is_null_index(name)) {
          cache_name(cache, name);
        }  /* if */
      }
      break;
    case ifc_es_expr_simple_identifier:
      { Opt<an_ifc_expr_simple_identifier> opt_iesi;

        construct_node(&opt_iesi, expr);
        if (!opt_iesi.has_value()) {
          goto invalid;
        }  /* if */
        /* FIXME: Do we need to handle the "type" field here? */
        cache_name(cache, get_ifc_name(*opt_iesi));
      }
      break;
    case ifc_es_expr_pointer:
      { Opt<an_ifc_expr_pointer> opt_iep;

        construct_node(&opt_iep, expr);
        if (!opt_iep.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_star);
      }
      break;
    case ifc_es_expr_qualified_name:
      { Opt<an_ifc_expr_qualified_name> opt_ieqn;

        construct_node(&opt_ieqn, expr);
        if (!opt_ieqn.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_qualified_name ieqn = *opt_ieqn;
        an_ifc_cache_info          cache_info = cinfo;
        cache_info.qualified_name = TRUE;
        /* FIXME: Do we need to handle the "type" field here? */
        if (get_ifc_line(get_ifc_typename_keyword(ieqn)) != 0) {
          cache_token(cache, tok_typename);
        }  /* if */
        cache_expr(cache, get_ifc_elements(ieqn), cache_info);
      }
      break;
    case ifc_es_expr_path:
      { Value_saver<a_boolean> suppression(
                                  &suppress_automatic_namespace_qualification);
        Value_saver<a_boolean> automatic_name_qualification(
                                       &suppress_automatic_name_qualification);
        Opt<an_ifc_expr_path>  opt_iep;

        construct_node(&opt_iep, expr);
        if (!opt_iep.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_path iep = *opt_iep;
        if (!suppress_automatic_name_qualification) {
          cache_expr(cache, get_ifc_scope(iep), cinfo);
          cache_token(cache, tok_colon_colon);
          update_name_qualification_suppression(iep);
        }  /* if */
        cache_expr(cache, get_ifc_member(iep), cinfo);
      }
      break;
    case ifc_es_expr_read:
      { Opt<an_ifc_expr_read> opt_ier;

        construct_node(&opt_ier, expr);
        if (!opt_ier.has_value()) {
          goto invalid;
        }  /* if */
        an_ifc_expr_read  ier = *opt_ier;
        an_ifc_read_conversion_sort
                          read_sort = get_ifc_sort(ier);
        an_ifc_cache_info cache_info = cinfo;
        switch (read_sort) {
          case ifc_rcs_indirection:
            cache_token(cache, tok_star);
            cache_info.nested_expr = TRUE;
            break;
          case ifc_rcs_dereference:
            cache_token(cache, tok_ampersand);
            cache_info.nested_expr = TRUE;
            break;
          case ifc_rcs_identity:
          case ifc_rcs_integral_conversion:
          case ifc_rcs_lvalue_to_rvalue:
            /* These all seem to have their semantics properly conveyed during
               caching as identity. */
            break;
          default_is_unexpected_str("Unexpected ReadConversionSort");
        };
        cache_expr(cache, get_ifc_address(ier), cache_info);
      }
      break;
    case ifc_es_expr_monad:
      { Opt<an_ifc_expr_monad> opt_iem;

        construct_node(&opt_iem, expr);
        if (!opt_iem.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_monad            iem = *opt_iem;
        an_ifc_expr_index            argument = get_ifc_argument(iem);
        an_ifc_monadic_operator_sort assoc = get_ifc_assoc(iem);
        auto                         cache_arg =
                                              [this, cache, cinfo, &argument] {
          cache_token(cache, tok_lparen);
          if (!is_null_index(argument)) {
            cache_expr(cache, argument, cinfo);
          } /* if */
          cache_token(cache, tok_rparen);
        };  /* cache_arg */

        an_operator_kind opkind = get_operator_kind(expr.mod, assoc);
        switch (opkind) {
          case opkind_error:
          case opkind_c_cast:
          case opkind_cpp_cast:
            ifc_unexpected(this, "Unexpected operator kind");
            goto invalid;
          case opkind_basic:
          case opkind_func_like:
            cache_operator(cache, assoc);
            if (assoc == ifc_mos_lookup_globally) {
              /* Do not produce parentheses after a "::". */
              cache_expr(cache, argument, cinfo);
            } else {
              cache_arg();
            }  /* if */
            break;
          case opkind_post:
            cache_arg();
            cache_operator(cache, assoc);
            break;
          case opkind_other:
            { a_token_kind ltok, rtok;
              if (assoc == ifc_mos_paren) {
                ltok = tok_lparen;
                rtok = tok_rparen;
              } else if (assoc == ifc_mos_brace) {
                ltok = tok_lbrace;
                rtok = tok_rbrace;
              } else {
                a_string err_msg("Unexpected ", str_for(assoc));

                ifc_unexpected(this, err_msg);
                goto invalid;
              }  /* if */
              cache_token(cache, ltok);
              cache_expr(cache, argument, cinfo);
              cache_token(cache, rtok);
            }
            break;
          default_is_unexpected();
        }  /* switch */
      }
      break;
    case ifc_es_expr_dyad:
      { Opt<an_ifc_expr_dyad> opt_ied;

        construct_node(&opt_ied, expr);
        if (!opt_ied.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_dyad            ied = *opt_ied;
        an_ifc_dyadic_operator_sort assoc = get_ifc_assoc(ied);
        an_operator_kind            opkind =
                                    get_operator_kind(ied.get_module(), assoc);
        an_ifc_expr_index           arg_0 = get_ifc_argument_0(ied);
        an_ifc_expr_index           arg_1 = get_ifc_argument_1(ied);
        switch (opkind) {
          case opkind_error:
          case opkind_post:
          case opkind_other:
            ifc_unexpected(this, "Unexpected operator kind");
            goto invalid;
          case opkind_basic:
            { an_ifc_cache_info  cache_info = cinfo;
              if (assoc != ifc_dos_assign) {
                /* Parenthesize dyadic operators to capture the precedence
                   that is implicit in the tree form, but not in the rendered
                   form. */
                cache_info.nested_expr = TRUE;
              }  /* if */
              if (cache_info.nested_expr) {
                cache_token(cache, tok_lparen);
              }  /* if */
              if (!cache_info.skip_assign || assoc != ifc_dos_assign) {
                an_ifc_cache_info lhs_cache_info = cache_info;
                lhs_cache_info.possible_temporary_decl = TRUE;
                cache_expr(cache, arg_0, lhs_cache_info);
                cache_operator(cache, assoc);
              }  /* if */
              cache_expr(cache, arg_1, cache_info);
              if (cache_info.nested_expr) {
                cache_token(cache, tok_rparen);
              }  /* if */
            }
            break;
          case opkind_func_like:
            if (assoc == ifc_dos_msvc_align) {
              /* An expression like "this->i" is represented in IFC files as
                 "this->__MsvcAlign(4, i)".  That has no equivalent in the
                 EDG IL.  So just cache the second argument. */
              cache_expr(cache, arg_1, cinfo);
            } else {
              cache_operator(cache, assoc);
              cache_token(cache, tok_lparen);
              cache_expr(cache, arg_0, cinfo);
              cache_token(cache, tok_comma);
              cache_expr(cache, arg_1, cinfo);
              cache_token(cache, tok_rparen);
            }  /* if */
            break;
          case opkind_cpp_cast:
            cache_operator(cache, assoc);
            FALLTHROUGH
          case opkind_c_cast:
            if (opkind == opkind_c_cast) {
              cache_token(cache, tok_lparen);
            } else {
              cache_token(cache, tok_lt);
            }  /* if */
            cache_expr(cache, arg_0, cinfo);
            if (opkind == opkind_c_cast) {
              cache_token(cache, tok_rparen);
            } else {
              cache_token(cache, tok_gt);
            }  /* if */
            cache_token(cache, tok_lparen);
            cache_expr(cache, arg_1, cinfo);
            cache_token(cache, tok_rparen);
            break;
          default_is_unexpected();
        }  /* switch */
      }
      break;
    case ifc_es_expr_triad:
      { Opt<an_ifc_expr_triad> opt_iet;

        construct_node(&opt_iet, expr);
        if (!opt_iet.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_triad            iet = *opt_iet;
        an_ifc_triadic_operator_sort assoc = get_ifc_assoc(iet);
        an_ifc_expr_index            arg_0 = get_ifc_argument_0(iet);
        an_ifc_expr_index            arg_1 = get_ifc_argument_1(iet);
        an_ifc_expr_index            arg_2 = get_ifc_argument_2(iet);
        switch (assoc) {
          case ifc_tos_choice:
            cache_expr(cache, arg_0, cinfo);
            cache_operator(cache, assoc);
            cache_expr(cache, arg_1, cinfo);
            cache_token(cache, tok_colon);
            cache_expr(cache, arg_2, cinfo);
            break;
          case ifc_tos_construct_at:
            cache_operator(cache, assoc);
            cache_token(cache, tok_lparen);
            cache_expr(cache, arg_0, cinfo);
            cache_token(cache, tok_rparen);
            cache_expr(cache, arg_1, cinfo);
            if (!is_null_index(arg_2)) {
              cache_args_with_parens(cache, arg_2, cinfo);
            }  /* if */
            break;
          default:
            issue_unsupported_construct_error(this, "TriadicOperator::???",
                                              &error_position);
            goto invalid;
        }  /* switch */
      }
      break;
    case ifc_es_expr_string:
      { Opt<an_ifc_expr_string> opt_ies;

        construct_node(&opt_ies, expr);
        if (!opt_ies.has_value()) {
          goto invalid;
        }  /* if */
        cache_string(cache, get_ifc_string_index(*opt_ies));
      }
      break;
    case ifc_es_expr_temporary:
      { Opt<an_ifc_expr_temporary> opt_iet;

        construct_node(&opt_iet, expr);
        if (!opt_iet.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_temporary       iet = *opt_iet;
        if (cinfo.possible_temporary_decl) {
          cache_type(cache, get_ifc_type(iet));
        }  /* if */
        cache_identifier(cache, make_ifc_temporary_unique_id(get_ifc_id(iet)));
      }
      break;
    case ifc_es_expr_call:
      { Opt<an_ifc_expr_call> opt_iec;

        construct_node(&opt_iec, expr);
        if (!opt_iec.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_call  iec = *opt_iec;
        an_ifc_expr_index arguments = get_ifc_arguments(iec);
        cache_expr(cache, get_ifc_operation(iec), cinfo);
        if (is_null_index(arguments)) {
          /* Sometimes (but not always) an empty argument list appears to be
             represented using a null "arguments" field. */
          cache_token(cache, tok_lparen);
          cache_token(cache, tok_rparen);
        } else {
          cache_args_with_parens(cache, arguments, cinfo);
        }  /* if */
      }
      break;
    case ifc_es_expr_member_initializer:
      { Opt<an_ifc_expr_member_initializer> opt_iemi;

        construct_node(&opt_iemi, expr);
        if (!opt_iemi.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_member_initializer iemi = *opt_iemi;
        an_ifc_decl_index              member = get_ifc_member(iemi);
        an_ifc_type_index              base = get_ifc_base(iemi);
        if (!is_null_index(member)) {
          /* A nonstatic member initialization. */
          cache_identifier(cache, name_from_decl(member));
        } else if (!is_null_index(base)) {
          /* A base subobject initialization. */
          cache_type(cache, base);
        } else {
          /* A delegating constructor. */
          issue_unsupported_construct_error(this,
                                            "ExprSort::MemberInitializer",
                                            &error_position);
          goto invalid;
        }  /* if */
        cache_token(cache, tok_lparen);
        { an_ifc_cache_info cache_info = cinfo;
          cache_info.skip_assign = TRUE;
          cache_expr(cache, get_ifc_initializer(iemi), cache_info);
        }
        cache_token(cache, tok_rparen);
      }
      break;
    case ifc_es_expr_member_access:
      { Opt<an_ifc_expr_member_access> opt_iema;

        construct_node(&opt_iema, expr);
        if (!opt_iema.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_member_access iema = *opt_iema;
        cache_identifier(cache, get_string_at_offset(get_ifc_name(iema)));
      }
      break;
    case ifc_es_expr_inheritance_path:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::InheritancePath",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_initializer_list:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::InitializerList",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_cast:
      { Opt<an_ifc_expr_cast> opt_iec;

        construct_node(&opt_iec, expr);
        if (!opt_iec.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_cast  iec = *opt_iec;
        an_ifc_cache_info cache_info = cinfo;
        cache_info.nested_expr = TRUE;

        an_ifc_dyadic_operator_sort op_sort = get_ifc_op(iec);
        switch (op_sort) {
          case ifc_dos_explicit_conversion:
            cache_type(cache, get_ifc_target(iec));
            cache_expr(cache, get_ifc_source(iec), cache_info);
            break;
          case ifc_dos_pretend:
            cache_token(cache, tok_lparen);
            cache_type(cache, get_ifc_target(iec));
            cache_token(cache, tok_rparen);
            cache_expr(cache, get_ifc_source(iec), cache_info);
            break;
          case ifc_dos_reinterpret_cast:
            cache_token(cache, tok_reinterpret_cast);
            goto common_cast;
          case ifc_dos_static_cast:
            cache_token(cache, tok_static_cast);
            goto common_cast;
          case ifc_dos_const_cast:
            cache_token(cache, tok_const_cast);
            goto common_cast;
          case ifc_dos_dynamic_cast:
            cache_token(cache, tok_dynamic_cast);
common_cast:
            cache_token(cache, tok_lt);
            cache_type(cache, get_ifc_target(iec));
            cache_token(cache, tok_gt);
            cache_token(cache, tok_lparen);
            cache_expr(cache, get_ifc_source(iec), cinfo);
            cache_token(cache, tok_rparen);
            break;
          default:
            { a_string err_msg("Unexpected ", str_for(op_sort),
                               " for ", str_for(expr.sort));

              ifc_unexpected(this, err_msg);
            }
            break;
        }  /* switch */
      }
      break;
    case ifc_es_expr_condition:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::Condition",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_expression_list:
      { Opt<an_ifc_expr_expression_list> opt_eel;

        construct_node(&opt_eel, expr);
        if (!opt_eel.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_expression_list eel = *opt_eel;
        an_ifc_delimiter_sort       delimiter = get_ifc_delimiter(eel);
        an_ifc_expr_index           contents = get_ifc_contents(eel);
        if (delimiter != ifc_ds_unknown) {
          an_ifc_source_location      locus = get_ifc_left(eel);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache,
                      delimiter == ifc_ds_brace ? tok_lbrace : tok_lparen);
        }  /* if */
        if (!is_null_index(contents)) {
          cache_expr(cache, contents, cinfo);
        }  /* if */
        if (delimiter != ifc_ds_unknown) {
          an_ifc_source_location      locus = get_ifc_right(eel);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache,
                      delimiter == ifc_ds_brace ? tok_rbrace : tok_rparen);
        }  /* if */
      }
      break;
    case ifc_es_expr_sizeof_type:
      { Opt<an_ifc_expr_sizeof_type> opt_iest;

        construct_node(&opt_iest, expr);
        if (!opt_iest.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_sizeof_type iest = *opt_iest;
        cache_token(cache, tok_sizeof);
        cache_token(cache, tok_lparen);
        cache_type(cache, get_ifc_operand(iest));
        cache_token(cache, tok_rparen);
      }
      break;
    case ifc_es_expr_alignof:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::Alignof",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_typeid:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::Typeid",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_destructor_call:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::DestructorCall",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_syntax_tree:
      { Opt<an_ifc_expr_syntax_tree> opt_iest;

        construct_node(&opt_iest, expr);
        if (!opt_iest.has_value()) {
          goto invalid;
        }  /* if */
        cache_syntax(cache, get_ifc_syntax(*opt_iest), cinfo);
      }
      break;
    case ifc_es_expr_function_string:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::FunctionString",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_compound_string:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::CompoundString",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_string_sequence:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::StringSequence",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_initializer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::Initializer",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_requires:
      { Opt<an_ifc_expr_requires> opt_ier;

        construct_node(&opt_ier, expr);
        if (!opt_ier.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_requires ier = *opt_ier;
        an_ifc_syntax_index  parameters = get_ifc_parameters(ier);
        cache_token(cache, tok_requires);
        if (!is_null_index(parameters)) {
          cache_syntax(cache, parameters, cinfo);
        }  /* if */
        /* FIXME: Is it necessary to set requires_body = TRUE here, or will
           this always be a SyntaxSort::RequirementsBody? */
        { an_ifc_cache_info    cache_info = cinfo;
          cache_info.requires_body = TRUE;
          cache_syntax(cache, get_ifc_body(ier), cache_info);
        }
      }
      break;
    case ifc_es_expr_unary_fold:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::UnaryFold",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_binary_fold:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::BinaryFold",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_hierarchy_conversion:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::HierarchyConversion",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_product_type_value:
      { Opt<an_ifc_expr_product_type_value> opt_ieptv;

        construct_node(&opt_ieptv, expr);
        if (!opt_ieptv.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_product_type_value ieptv = *opt_ieptv;
        a_type_ptr                     tp;
        a_constant_ptr                 cp;
        tp = type_for_type_index(get_ifc_type(ieptv), /*kind=*/NULL);
        cp = constant_for_expr_index(expr, tp);
        cache_aggr_constant(cache, cp);
      }
      break;
    case ifc_es_expr_sum_type_value:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::SumTypeValue",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_subobject_value:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::SubobjectValue",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_array_value:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::ArrayValue",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_dynamic_dispatch:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::DynamicDispatch",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_virtual_function_conversion:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "ExprSort::VirtualFunctionConversion",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_placeholder:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::Placeholder",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_expansion:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::Expansion",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_generic:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::Generic",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_tuple:
      { Opt<an_ifc_expr_tuple> opt_iet;

        construct_node(&opt_iet, expr);
        if (!opt_iet.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_tuple      iet = *opt_iet;
        an_expr_heap_traverser traverser(iet);
        Value_saver<a_boolean> suppression(
                                       &suppress_automatic_name_qualification);

        if (cinfo.qualified_name) {
          /* This tuple contains a series of name qualifiers - we don't want to
             perform automatic name qualification, as this would result in
             duplicate qualifiers. */
          suppress_automatic_name_qualification = TRUE;
        }  /* if */
        for (an_Indexed<an_ifc_heap_expr> indexed_ihe : traverser) {
          if (!indexed_ihe.has_value()) {
            goto invalid;
          }  /* if */
          if (!is_first(traverser, indexed_ihe)) {
            a_token_kind sep = cinfo.qualified_name ? tok_colon_colon
                                                    : tok_comma;
            cache_token(cache, sep);
          }  /* if */
          cache_expr(cache, get_ifc_value(*indexed_ihe), cinfo);
        }  /* for */
      }
      break;
    case ifc_es_expr_nullptr:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::Nullptr",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_this:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::This",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_template_reference:
      { Opt<an_ifc_expr_template_reference> opt_ietr;

        construct_node(&opt_ietr, expr);
        if (!opt_ietr.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_expr_template_reference ietr = *opt_ietr;
        an_ifc_expr_index              arguments = get_ifc_arguments(ietr);
        cache_type(cache, get_ifc_scope(ietr));
        cache_token(cache, tok_colon_colon);
        cache_identifier(cache,
                         string_from_name_index(get_ifc_member_name(ietr),
                                                /*loc=*/NULL));
        if (!is_null_index(arguments)) {
          cache_token(cache, tok_lt);
          cache_expr(cache, arguments, cinfo);
          cache_token(cache, tok_gt);
        }  /* if */
      }
      break;
    case ifc_es_expr_push_state:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::PushState",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_type_trait_intrinsic:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "ExprSort::TypeTraitIntrinsic",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_designated_initializer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "ExprSort::DesignatedInitializer",
                                        &error_position);
      goto invalid;
    case ifc_es_expr_packed_template_arguments:
      { Opt<an_ifc_expr_packed_template_arguments> opt_iepta;

        construct_node(&opt_iepta, expr);
        if (!opt_iepta.has_value()) {
          goto invalid;
        }  /* if */
        cache_expr(cache, get_ifc_arguments(*opt_iepta), cinfo);
      }
      break;
    case ifc_es_expr_tokens:
      { Opt<an_ifc_expr_tokens> opt_iet;

        construct_node(&opt_iet, expr);
        if (!opt_iet.has_value()) {
          goto invalid;
        }  /* if */
        cache_sentence(cache, get_ifc_words(*opt_iet));
      }
      break;
    case ifc_es_expr_assign_initializer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "ExprSort::AssignInitializer",
                                        &error_position);
      goto invalid;
    default_is_unexpected_str("Unknown ExprSort");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad expr cache");
  cache->invalidate();
done:;
}  /* cache_expr */


void an_ifc_module::cache_syntax(a_module_token_cache_ptr cache,
                                 an_ifc_syntax_index      syntax,
                                 const an_ifc_cache_info  &cinfo)
/*
Add the tokens corresponding to the given syntax tree to cache.  cinfo contains
information about the current cache context to help inform decisions about what
to cache.  For example, if cinfo.requires_param_decl is TRUE, parameter
references should include decl specifiers, type, and any default expression.
Otherwise, parameter references should only include the parameter name.
*/
{
  an_ifc_source_position_hint pos_hint(cache, syntax);

  switch (syntax.sort) {
    case ifc_ss_syntax_vendor_extension:
      issue_unsupported_construct_error(this, "SyntaxSort::VendorExtension",
                                        &error_position);
      break;
    case ifc_ss_syntax_simple_type_specifier:
      { Opt<an_ifc_syntax_simple_type_specifier> opt_issts;

        construct_node(&opt_issts, syntax);
        if (!opt_issts.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_simple_type_specifier issts = *opt_issts;
        an_ifc_type_index                   type = get_ifc_type(issts);
        an_ifc_expr_index                   expr = get_ifc_expr(issts);
        if (!is_null_index(type)) {
          check_assertion(is_null_index(expr));
          cache_type(cache, type);
        } else {
          check_assertion(!is_null_index(expr));
          cache_expr(cache, expr, cinfo);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_decltype_specifier:
      { Opt<an_ifc_syntax_decltype_specifier> opt_isds;

        construct_node(&opt_isds, syntax);
        if (!opt_isds.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_decltype_specifier isds = *opt_isds;
        cache_token(cache, tok_decltype);
        cache_token(cache, tok_lparen);
        cache_expr(cache, get_ifc_expr(isds), cinfo);
        cache_token(cache, tok_rparen);
      }
      break;
    case ifc_ss_syntax_placeholder_type_specifier:
      { Opt<an_ifc_syntax_placeholder_type_specifier> opt_ispt;

        construct_node(&opt_ispt, syntax);
        if (!opt_ispt.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_placeholder_type_specifier ispt = *opt_ispt;
        an_ifc_type_basis_sort                   basis = get_ifc_basis(ispt);
        /* FIXME: Handle the constraint field. */
        if (basis == ifc_tbs_auto) {
          cache_token(cache, tok_auto);
        } else {
          check_assertion(basis == ifc_tbs_decltype_auto);
          cache_token(cache, tok_decltype);
          cache_token(cache, tok_lparen);
          cache_token(cache, tok_auto);
          cache_token(cache, tok_rparen);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_type_specifier_seq:
      { Opt<an_ifc_syntax_type_specifier_seq> opt_istss;

        construct_node(&opt_istss, syntax);
        if (!opt_istss.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_type_specifier_seq istss = *opt_istss;
        an_ifc_type_index                type = get_ifc_type(istss);
        an_ifc_syntax_index              type_name = get_ifc_type_name(istss);
        Value_saver<a_boolean>           suppression(
                                        &suppress_automatic_name_qualification,
                                        /*new_value=*/TRUE);

        /* We have explicit qualifiers - don't do automatic name qualification
           (flag set above). */
        cache_qualifiers(cache, get_ifc_qualifiers(istss));
        if (is_null_index(type)) {
          check_assertion(!is_null_index(type_name));
          cache_syntax(cache, type_name, cinfo);
        } else {
          check_assertion(is_null_index(type_name));
          cache_type(cache, type);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_decl_specifier_seq:
      { Opt<an_ifc_syntax_decl_specifier_seq> opt_isdss;

        construct_node(&opt_isdss, syntax);
        if (!opt_isdss.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_decl_specifier_seq isdss = *opt_isdss;
        an_ifc_sentence_index            declspec = get_ifc_declspec(isdss);
        an_ifc_syntax_index              explicit_kw =
                                                    get_ifc_explicit_kw(isdss);
        an_ifc_type_index                type = get_ifc_type(isdss);
        an_ifc_syntax_index              type_name = get_ifc_type_name(isdss);
        /* FIXME: Handle storage_class field. */
        if (declspec != 0) {
          cache_sentence(cache, declspec);
        }  /* if */
        if (!is_null_index(explicit_kw)) {
          cache_syntax(cache, explicit_kw, cinfo);
        }  /* if */
        cache_qualifiers(cache, get_ifc_qualifiers(isdss));
        if (is_null_index(type)) {
          check_assertion(!is_null_index(type_name));
          cache_syntax(cache, type_name, cinfo);
        } else {
          check_assertion(is_null_index(type_name));
          cache_type(cache, type);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_virtual_specifier_seq:
      { Opt<an_ifc_syntax_virtual_specifier_seq> opt_isvss;

        construct_node(&opt_isvss, syntax);
        if (!opt_isvss.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_virtual_specifier_seq isvss = *opt_isvss;
        if (get_ifc_line(get_ifc_override_kw(isvss)) != 0) {
          cache_token(cache, tok_override);
        }  /* if */
        if (get_ifc_line(get_ifc_final_kw(isvss)) != 0) {
          cache_token(cache, tok_final);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_noexcept_specification:
      { Opt<an_ifc_syntax_noexcept_specification> opt_sns;

        construct_node(&opt_sns, syntax);
        if (!opt_sns.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_noexcept_specification sns = *opt_sns;
        cache_token(cache, tok_noexcept);
        cache_token(cache, tok_lparen);
        cache_syntax(cache, get_ifc_expr(sns), cinfo);
        cache_token(cache, tok_rparen);
      }
      break;
    case ifc_ss_syntax_explicit_specifier:
      { Opt<an_ifc_syntax_explicit_specifier> opt_ises;

        construct_node(&opt_ises, syntax);
        if (!opt_ises.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_explicit_specifier ises = *opt_ises;
        an_ifc_expr_index                condition = get_ifc_condition(ises);
        cache_token(cache, tok_explicit);
        if (!is_null_index(condition)) {
          cache_token(cache, tok_lparen);
          cache_expr(cache, condition, cinfo);
          cache_token(cache, tok_rparen);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_enum_specifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::EnumSpecifier",
                                        &error_position);
      break;
    case ifc_ss_syntax_enumerator_definition:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::EnumeratorDefinition",
                                        &error_position);
      break;
    case ifc_ss_syntax_class_specifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::ClassSpecifier",
                                        &error_position);
      break;
    case ifc_ss_syntax_member_specification:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::MemberSpecification",
                                        &error_position);
      break;
    case ifc_ss_syntax_member_declaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::MemberDeclaration",
                                        &error_position);
      break;
    case ifc_ss_syntax_member_declarator:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::MemberDeclarator",
                                        &error_position);
      break;
    case ifc_ss_syntax_access_specifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::AccessSpecifier",
                                        &error_position);
      break;
    case ifc_ss_syntax_base_specifier_list:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::BaseSpecifierList",
                                        &error_position);
      break;
    case ifc_ss_syntax_base_specifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::BaseSpecifier",
                                        &error_position);
      break;
    case ifc_ss_syntax_type_id:
      { Opt<an_ifc_syntax_type_id> opt_isti;

        construct_node(&opt_isti, syntax);
        if (!opt_isti.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_type_id isti = *opt_isti;
        an_ifc_syntax_index   abstract_declarator =
                                             get_ifc_abstract_declarator(isti);
        cache_syntax(cache, get_ifc_type_specifier(isti), cinfo);
        if (!is_null_index(abstract_declarator)) {
          cache_syntax(cache, abstract_declarator, cinfo);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_trailing_return_type:
      { Opt<an_ifc_syntax_trailing_return_type> opt_istrt;

        construct_node(&opt_istrt, syntax);
        if (!opt_istrt.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_arrow);
        cache_syntax(cache, get_ifc_target(*opt_istrt), cinfo);
      }
      break;
    case ifc_ss_syntax_declarator:
      { Opt<an_ifc_syntax_declarator> opt_isd;

        construct_node(&opt_isd, syntax);
        if (!opt_isd.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_declarator       isd = *opt_isd;
        an_ifc_calling_convention_sort convention = get_ifc_convention(isd);
        an_ifc_syntax_index            pointer = get_ifc_pointer(isd);
        an_ifc_syntax_index            parenthesized =
                                                    get_ifc_parenthesized(isd);
        an_ifc_syntax_index            array_or_function =
                                                get_ifc_array_or_function(isd);
        an_ifc_syntax_index            virtual_specifiers =
                                               get_ifc_virtual_specifiers(isd);
        an_ifc_expr_index              name = get_ifc_name(isd);
        an_ifc_syntax_index            trailing_target =
                                                  get_ifc_trailing_target(isd);
        if (convention != ifc_ccs_cdecl) {
          cache_calling_convention(cache, convention);
        }  /* if */
        if (!is_null_index(pointer)) {
          cache_syntax(cache, pointer, cinfo);
        } else if (!is_null_index(parenthesized)) {
          cache_token(cache, tok_lparen);
          cache_syntax(cache, parenthesized, cinfo);
          cache_token(cache, tok_rparen);
        } else if (!is_null_index(array_or_function)) {
          cache_syntax(cache, array_or_function, cinfo);
        }  /* if */
        cache_qualifiers(cache, get_ifc_qualifiers(isd));
        if (!is_null_index(virtual_specifiers)) {
          cache_syntax(cache, virtual_specifiers, cinfo);
        }  /* if */
        if (!is_null_index(name)) {
          /* FIXME: Confirm and ensure that the name is cached at the right
             location in the sequence of tokens. */
          cache_expr(cache, name, cinfo);
        }  /* if */
        if (!is_null_index(trailing_target)) {
          cache_syntax(cache, trailing_target, cinfo);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_pointer_declarator:
      { Opt<an_ifc_syntax_pointer_declarator> opt_ispd;

        construct_node(&opt_ispd, syntax);
        if (!opt_ispd.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_pointer_declarator ispd = *opt_ispd;
        an_ifc_calling_convention_sort   convention = get_ifc_convention(ispd);
        an_ifc_syntax_index              next = get_ifc_next(ispd);
        cache_qualifiers(cache, get_ifc_qualifiers(ispd));
        if (convention != ifc_ccs_cdecl) {
          cache_calling_convention(cache, convention);
        }  /* if */

        an_ifc_pointer_declarator_sort declar_sort = get_ifc_sort(ispd);
        switch (declar_sort) {
          case ifc_pds_none:
            { a_string err_msg("Unexpected ", str_for(declar_sort));

              ifc_unexpected(this, err_msg);
            }
            goto invalid;
          case ifc_pds_pointer:
            cache_token(cache, tok_star);
            break;
          case ifc_pds_lvalue_reference:
            cache_token(cache, tok_ampersand);
            break;
          case ifc_pds_rvalue_reference:
            cache_token(cache, tok_and_and);
            break;
          case ifc_pds_pointer_to_member:
            { an_ifc_syntax_index whole = get_ifc_whole(ispd);

              check_assertion(!is_null_index(whole));
              cache_syntax(cache, whole, cinfo);
              cache_token(cache, tok_colon_colon);
              cache_token(cache, tok_star);
            }
            break;
          default_is_unexpected_str("Unexpected PointerDeclaratorSort");
        }  /* switch */
        if (!is_null_index(next)) {
          cache_syntax(cache, next, cinfo);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_array_declarator:
      { Opt<an_ifc_syntax_array_declarator> opt_isad;

        construct_node(&opt_isad, syntax);
        if (!opt_isad.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_array_declarator isad = *opt_isad;
        an_ifc_expr_index              bound = get_ifc_bound(isad);
        {
          an_ifc_source_location      locus = get_ifc_left_bracket(isad);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_lbracket);
        }
        if (!is_null_index(bound)) {
          cache_expr(cache, bound, cinfo);
        }  /* if */
        {
          an_ifc_source_location      locus = get_ifc_right_bracket(isad);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_rbracket);
        }
      }
      break;
    case ifc_ss_syntax_function_declarator:
      { Opt<an_ifc_syntax_function_declarator> opt_isfd;

        construct_node(&opt_isfd, syntax);
        if (!opt_isfd.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_function_declarator isfd = *opt_isfd;
        an_ifc_syntax_index               parameters =
                                                      get_ifc_parameters(isfd);
        an_ifc_syntax_index               eh_spec = get_ifc_eh_spec(isfd);
        {
          an_ifc_source_location      locus = get_ifc_left_paren(isfd);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_lparen);
        }
        if (!is_null_index(parameters)) {
          cache_syntax(cache, parameters, cinfo);
        }  /* if */
        {
          an_ifc_source_location      locus = get_ifc_right_paren(isfd);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_rparen);
        }
        if (!is_null_index(eh_spec)) {
          cache_syntax(cache, eh_spec, cinfo);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_array_or_function_declarator:
      { Opt<an_ifc_syntax_array_or_function_declarator> opt_isaofd;

        construct_node(&opt_isaofd, syntax);
        if (!opt_isaofd.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_array_or_function_declarator isaofd = *opt_isaofd;
        an_ifc_syntax_index                        next = get_ifc_next(isaofd);
        cache_syntax(cache, get_ifc_declarator(isaofd), cinfo);
        if (!is_null_index(next)) {
          cache_syntax(cache, next, cinfo);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_parameter_declarator:
      { Opt<an_ifc_syntax_parameter_declarator> opt_ispd;

        construct_node(&opt_ispd, syntax);
        if (!opt_ispd.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_parameter_declarator ispd = *opt_ispd;
        an_ifc_syntax_index                decl_specifiers =
                                                 get_ifc_decl_specifiers(ispd);
        an_ifc_expr_index                  default_expr =
                                                    get_ifc_default_expr(ispd);
        if (!cinfo.requires_body && !is_null_index(decl_specifiers)) {
          cache_syntax(cache, decl_specifiers, cinfo);
        }  /* if */
        cache_syntax(cache, get_ifc_declarator(ispd), cinfo);
        if (!cinfo.requires_body && !is_null_index(default_expr)) {
          cache_token(cache, tok_eq);
          cache_expr(cache, default_expr, cinfo);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_init_declarator:
      { Opt<an_ifc_syntax_init_declarator> opt_isid;

        construct_node(&opt_isid, syntax);
        if (!opt_isid.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_init_declarator isid = *opt_isid;
        an_ifc_expr_index             initializer = get_ifc_initializer(isid);
        an_ifc_source_location        comma = get_ifc_comma(isid);
        cache_syntax(cache, get_ifc_declarator(isid), cinfo);
        /* FIXME: Handle the constraint field. */
        if (!is_null_index(initializer)) {
          cache_token(cache, tok_eq);
          cache_expr(cache, initializer, cinfo);
        }  /* if */
        if (get_ifc_line(comma) != 0) {
          an_ifc_source_location      locus = get_ifc_comma(isid);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_comma);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_new_declarator:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::NewDeclarator",
                                        &error_position);
      break;
    case ifc_ss_syntax_simple_declaration:
      { Opt<an_ifc_syntax_simple_declaration> opt_issd;

        construct_node(&opt_issd, syntax);
        if (!opt_issd.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_simple_declaration issd = *opt_issd;
        an_ifc_syntax_index              decl_specifiers =
                                                 get_ifc_decl_specifiers(issd);
        if (!is_null_index(decl_specifiers)) {
          cache_syntax(cache, decl_specifiers, cinfo);
        }  /* if */
        cache_syntax(cache, get_ifc_declarators(issd), cinfo);
        {
          an_ifc_source_location      locus = get_ifc_semicolon(issd);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_semicolon);
        }
      }
      break;
    case ifc_ss_syntax_exception_declaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::ExceptionDeclaration",
                                        &error_position);
      break;
    case ifc_ss_syntax_condition_declaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::ConditionDeclaration",
                                        &error_position);
      break;
    case ifc_ss_syntax_static_assert_declaration:
      { Opt<an_ifc_syntax_static_assert_declaration> opt_issad;

        construct_node(&opt_issad, syntax);
        if (!opt_issad.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_static_assert_declaration issad = *opt_issad;
        an_ifc_expr_index                       message =
                                                        get_ifc_message(issad);
        cache_token(cache, tok_static_assert);
        cache_token(cache, tok_lparen);
        cache_expr(cache, get_ifc_condition(issad), cinfo);
        if (!is_null_index(message)) {
          cache_token(cache, tok_comma);
          cache_expr(cache, message, cinfo);
        }  /* if */
        cache_token(cache, tok_rparen);
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ss_syntax_alias_declaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::AliasDeclaration",
                                        &error_position);
      break;
    case ifc_ss_syntax_concept_definition:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::ConceptDefinition",
                                        &error_position);
      break;
    case ifc_ss_syntax_compound_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::CompoundStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_return_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::ReturnStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_if_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::IfStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_while_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::WhileStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_do_while_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::DoWhileStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_for_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::ForStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_init_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::InitStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_range_based_for_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::RangeBasedForStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_for_range_declaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::ForRangeDeclaration",
                                        &error_position);
      break;
    case ifc_ss_syntax_labeled_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::LabeledStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_break_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::BreakStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_continue_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::ContinueStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_switch_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::SwitchStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_goto_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::GotoStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_declaration_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::DeclarationStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_expression_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::ExpressionStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_try_block:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::TryBlock",
                                        &error_position);
      break;
    case ifc_ss_syntax_handler:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::Handler",
                                        &error_position);
      break;
    case ifc_ss_syntax_handler_seq:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::HandlerSeq",
                                        &error_position);
      break;
    case ifc_ss_syntax_function_try_block:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::FunctionTryBlock",
                                        &error_position);
      break;
    case ifc_ss_syntax_type_id_list_element:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::TypeIdListElement",
                                        &error_position);
      break;
    case ifc_ss_syntax_dynamic_exception_spec:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::DynamicExceptionSpec",
                                        &error_position);
      break;
    case ifc_ss_syntax_statement_seq:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::StatementSeq",
                                        &error_position);
      break;
    case ifc_ss_syntax_function_body:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::FunctionBody",
                                        &error_position);
      break;
    case ifc_ss_syntax_expression:
      { Opt<an_ifc_syntax_expression> opt_ise;

        construct_node(&opt_ise, syntax);
        if (!opt_ise.has_value()) {
          goto invalid;
        }  /* if */
        cache_expr(cache, get_ifc_expression(*opt_ise), cinfo);
      }
      break;
    case ifc_ss_syntax_function_definition:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::FunctionDefinition",
                                        &error_position);
      break;
    case ifc_ss_syntax_member_function_declaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                                       this,
                                       "SyntaxSort::MemberFunctionDeclaration",
                                       &error_position);
      break;
    case ifc_ss_syntax_template_declaration:
      { Opt<an_ifc_syntax_template_declaration> opt_istd;

        construct_node(&opt_istd, syntax);
        if (!opt_istd.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_template_declaration istd = *opt_istd;
        cache_token(cache, tok_template);
        cache_syntax(cache, get_ifc_parameters(istd), cinfo);
        cache_syntax(cache, get_ifc_subject(istd), cinfo);
      }
      break;
    case ifc_ss_syntax_requires_clause:
      { Opt<an_ifc_syntax_requires_clause> opt_isrc;

        construct_node(&opt_isrc, syntax);
        if (!opt_isrc.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_requires_clause isrc = *opt_isrc;
        cache_token(cache, tok_requires);
        cache_expr(cache, get_ifc_condition(isrc), cinfo);
      }
      break;
    case ifc_ss_syntax_simple_requirement:
      { Opt<an_ifc_syntax_simple_requirement> opt_issr;

        construct_node(&opt_issr, syntax);
        if (!opt_issr.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_simple_requirement issr = *opt_issr;
        cache_expr(cache, get_ifc_condition(issr), cinfo);
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ss_syntax_type_requirement:
      { Opt<an_ifc_syntax_type_requirement> opt_istr;

        construct_node(&opt_istr, syntax);
        if (!opt_istr.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_type_requirement istr = *opt_istr;
        cache_token(cache, tok_typename);
        cache_expr(cache, get_ifc_type(istr), cinfo);
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ss_syntax_compound_requirement:
      { Opt<an_ifc_syntax_compound_requirement> opt_iscr;

        construct_node(&opt_iscr, syntax);
        if (!opt_iscr.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_compound_requirement iscr = *opt_iscr;
        an_ifc_source_location             noexcept_loc =
                                                    get_ifc_noexcept_loc(iscr);
        cache_token(cache, tok_lbrace);
        cache_expr(cache, get_ifc_condition(iscr), cinfo);
        {
          an_ifc_source_location      locus = get_ifc_right_curly(iscr);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_rbrace);
        }  /* if */
        if (get_ifc_line(noexcept_loc) != 0) {
          an_ifc_source_position_hint tok_pos_hint(cache, noexcept_loc);

          cache_token(cache, tok_noexcept);
        }  /* if */
        cache_token(cache, tok_arrow);
        cache_expr(cache, get_ifc_constraint(iscr), cinfo);
        cache_token(cache, tok_semicolon);
      }
      break;
    case ifc_ss_syntax_nested_requirement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::NestedRequirement",
                                        &error_position);
      break;
    case ifc_ss_syntax_requirement_body:
      { Opt<an_ifc_syntax_requirement_body> opt_isrb;

        construct_node(&opt_isrb, syntax);
        if (!opt_isrb.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_requirement_body isrb = *opt_isrb;
        an_ifc_cache_info              cache_info = cinfo;
        cache_info.requires_body = TRUE;
        cache_token(cache, tok_lbrace);
        cache_syntax(cache, get_ifc_requirements(isrb), cache_info);
        {
          an_ifc_source_location      locus = get_ifc_right_curly(isrb);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_rbrace);
        }
      }
      break;
    case ifc_ss_syntax_type_template_parameter:
      { Opt<an_ifc_syntax_type_template_parameter> opt_isttp;

        construct_node(&opt_isttp, syntax);
        if (!opt_isttp.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_type_template_parameter isttp = *opt_isttp;
        an_ifc_syntax_index                   argument =
                                                       get_ifc_argument(isttp);
        cache_token(cache, tok_typename);
        if (get_ifc_line(get_ifc_ellipsis(isttp)) != 0) {
          cache_token(cache, tok_ellipsis);
        }  /* if */
        cache_identifier(cache, get_string_at_offset(get_ifc_name(isttp)));
        /* FIXME: Handle constraint field. */
        if (!is_null_index(argument)) {
          cache_token(cache, tok_eq);
          cache_syntax(cache, argument, cinfo);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_template_template_parameter:
      { Opt<an_ifc_syntax_template_template_parameter> opt_isttp;

        construct_node(&opt_isttp, syntax);
        if (!opt_isttp.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_template_template_parameter isttp = *opt_isttp;
        an_ifc_syntax_index                       argument =
                                                       get_ifc_argument(isttp);
        cache_token(cache, tok_template);
        cache_syntax(cache, get_ifc_parameters(isttp), cinfo);
        cache_token(cache, tok_gt);

        an_ifc_source_location ellipsis = get_ifc_ellipsis(isttp);
        if (get_ifc_line(ellipsis) != 0) {
          an_ifc_source_position_hint tok_pos_hint(cache, ellipsis);

          cache_token(cache, tok_ellipsis);
        }  /* if */
        cache_identifier(cache, get_string_at_offset(get_ifc_name(isttp)));
        if (!is_null_index(argument)) {
          cache_token(cache, tok_eq);
          cache_syntax(cache, argument, cinfo);
        }  /* if */

        an_ifc_source_location comma = get_ifc_comma(isttp);
        if (get_ifc_line(comma) != 0) {
          an_ifc_source_position_hint tok_pos_hint(cache, comma);

          cache_token(cache, tok_comma);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_type_template_argument:
      { Opt<an_ifc_syntax_type_template_argument> opt_istta;

        construct_node(&opt_istta, syntax);
        if (!opt_istta.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_type_template_argument istta = *opt_istta;
        cache_syntax(cache, get_ifc_argument(istta), cinfo);

        an_ifc_source_location ellipsis = get_ifc_ellipsis(istta);
        if (get_ifc_line(ellipsis) != 0) {
          an_ifc_source_position_hint tok_pos_hint(cache, ellipsis);

          cache_token(cache, tok_ellipsis);
        }  /* if */

        an_ifc_source_location comma = get_ifc_comma(istta);
        if (get_ifc_line(comma) != 0) {
          an_ifc_source_position_hint tok_pos_hint(cache, comma);

          cache_token(cache, tok_comma);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_non_type_template_argument:
      { Opt<an_ifc_syntax_non_type_template_argument> opt_isntta;

        construct_node(&opt_isntta, syntax);
        if (!opt_isntta.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_non_type_template_argument isntta = *opt_isntta;
        cache_expr(cache, get_ifc_argument(isntta), cinfo);

        an_ifc_source_location ellipsis = get_ifc_ellipsis(isntta);
        if (get_ifc_line(ellipsis) != 0) {
          an_ifc_source_position_hint tok_pos_hint(cache, ellipsis);

          cache_token(cache, tok_ellipsis);
        }  /* if */

        an_ifc_source_location comma = get_ifc_comma(isntta);
        if (get_ifc_line(comma) != 0) {
          an_ifc_source_position_hint tok_pos_hint(cache, comma);

          cache_token(cache, tok_comma);
        }  /* if */
      }
      break;
    case ifc_ss_syntax_template_parameter_list:
      { Opt<an_ifc_syntax_template_parameter_list> opt_istpl;

        construct_node(&opt_istpl, syntax);
        if (!opt_istpl.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_template_parameter_list istpl = *opt_istpl;
        {
          an_ifc_source_location      locus = get_ifc_left_angle(istpl);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_lt);
        }
        cache_syntax(cache, get_ifc_parameters(istpl), cinfo);
        /* FIXME: Handle the "clause" field. */
        {
          an_ifc_source_location      locus = get_ifc_right_angle(istpl);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_gt);
        }
      }
      break;
    case ifc_ss_syntax_template_argument_list:
      { Opt<an_ifc_syntax_template_argument_list> opt_istal;

        construct_node(&opt_istal, syntax);
        if (!opt_istal.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_template_argument_list istal = *opt_istal;
        {
          an_ifc_source_location      locus = get_ifc_left_angle(istal);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_lt);
        }
        cache_syntax(cache, get_ifc_arguments(istal), cinfo);
        /* FIXME: Handle the "clause" field. */
        {
          an_ifc_source_location      locus = get_ifc_right_angle(istal);
          an_ifc_source_position_hint tok_pos_hint(cache, locus);

          cache_token(cache, tok_gt);
        }
      }
      break;
    case ifc_ss_syntax_template_id:
      { Opt<an_ifc_syntax_template_id> opt_isti;

        construct_node(&opt_isti, syntax);
        if (!opt_isti.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_template_id isti = *opt_isti;
        an_ifc_syntax_index       name = get_ifc_name(isti);
        an_ifc_source_location    template_kw = get_ifc_template_kw(isti);
        if (get_ifc_line(template_kw) != 0) {
          an_ifc_source_position_hint tok_pos_hint(cache, template_kw);

          cache_token(cache, tok_template);
        }  /* if */
        if (!is_null_index(name)) {
          cache_syntax(cache, name, cinfo);
        } else {
          an_ifc_expr_index symbol = get_ifc_symbol(isti);

          check_assertion(!is_null_index(symbol));
          cache_expr(cache, symbol, cinfo);
        }  /* if */
        cache_syntax(cache, get_ifc_arguments(isti), cinfo);
      }
      break;
    case ifc_ss_syntax_mem_initializer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::MemInitializer",
                                        &error_position);
      break;
    case ifc_ss_syntax_ctor_initializer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::CtorInitializer",
                                        &error_position);
      break;
    case ifc_ss_syntax_lambda_introducer:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::LambdaIntroducer",
                                        &error_position);
      break;
    case ifc_ss_syntax_lambda_declarator:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::LambdaDeclarator",
                                        &error_position);
      break;
    case ifc_ss_syntax_capture_default:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::CaptureDefault",
                                        &error_position);
      break;
    case ifc_ss_syntax_simple_capture:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::SimpleCapture",
                                        &error_position);
      break;
    case ifc_ss_syntax_init_capture:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::InitCapture",
                                        &error_position);
      break;
    case ifc_ss_syntax_this_capture:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::ThisCapture",
                                        &error_position);
      break;
    case ifc_ss_syntax_attributed_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::AttributedStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_attributed_declaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::AttributedDeclaration",
                                        &error_position);
      break;
    case ifc_ss_syntax_attribute_specifier_seq:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::AttributeSpecifierSeq",
                                        &error_position);
      break;
    case ifc_ss_syntax_attribute_specifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::AttributeSpecifier",
                                        &error_position);
      break;
    case ifc_ss_syntax_attribute_using_prefix:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::AttributeUsingPrefix",
                                        &error_position);
      break;
    case ifc_ss_syntax_attribute:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::Attribute",
                                        &error_position);
      break;
    case ifc_ss_syntax_attribute_argument_clause:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::AttributeArgumentClause",
                                        &error_position);
      break;
    case ifc_ss_syntax_alignas:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::Alignas",
                                        &error_position);
      break;
    case ifc_ss_syntax_using_declaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::UsingDeclaration",
                                        &error_position);
      break;
    case ifc_ss_syntax_using_declarator:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::UsingDeclarator",
                                        &error_position);
      break;
    case ifc_ss_syntax_using_directive:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::UsingDirective",
                                        &error_position);
      break;
    case ifc_ss_syntax_array_index:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::ArrayIndex",
                                        &error_position);
      break;
    case ifc_ss_syntax_seh_try:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::SEHTry",
                                        &error_position);
      break;
    case ifc_ss_syntax_seh_except:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::SEHExcept",
                                        &error_position);
      break;
    case ifc_ss_syntax_seh_finally:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::SEHFinally",
                                        &error_position);
      break;
    case ifc_ss_syntax_seh_leave:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::SEHLeave",
                                        &error_position);
      break;
    case ifc_ss_syntax_type_trait_intrinsic:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::TypeTraitIntrinsic",
                                        &error_position);
      break;
    case ifc_ss_syntax_tuple:
      { Opt<an_ifc_syntax_tuple> opt_ist;

        construct_node(&opt_ist, syntax);
        if (!opt_ist.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_syntax_tuple     ist = *opt_ist;
        a_syntax_heap_traverser traverser(ist);
        for (an_Indexed<an_ifc_heap_syntax> indexed_ihs : traverser) {
          if (!indexed_ihs.has_value()) {
            goto invalid;
          }  /* if */
          cache_syntax(cache, get_ifc_value(*indexed_ihs), cinfo);
        }  /* for */
      }
      break;
    case ifc_ss_syntax_asm_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::AsmStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_namespace_alias_definition:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::NamespaceAliasDefinition",
                                        &error_position);
      break;
    case ifc_ss_syntax_super:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::Super",
                                        &error_position);
      break;
    case ifc_ss_syntax_unary_fold_expression:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::UnaryFoldExpression",
                                        &error_position);
      break;
    case ifc_ss_syntax_binary_fold_expression:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::BinaryFoldExpression",
                                        &error_position);
      break;
    case ifc_ss_syntax_empty_statement:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this, "SyntaxSort::EmptyStatement",
                                        &error_position);
      break;
    case ifc_ss_syntax_structured_binding_declaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                                    this,
                                    "SyntaxSort::StructuredBindingDeclaration",
                                    &error_position);
      break;
    case ifc_ss_syntax_structured_binding_identifier:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(
                                     this,
                                     "SyntaxSort::StructuredBindingIdentifier",
                                     &error_position);
      break;
    case ifc_ss_syntax_using_enum_declaration:
      /* FIXME: Currently unsupported. */
      issue_unsupported_construct_error(this,
                                        "SyntaxSort::UsingEnumDeclaration",
                                        &error_position);
      break;
    default_is_unexpected_str("Unexpected SyntaxSort");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad syntax cache");
  cache->invalidate();
done:;
}  /* cache_syntax */


void an_ifc_module::cache_name(a_module_token_cache_ptr cache,
                               an_ifc_name_index        name)
/*
Add the tokens corresponding to the given name to cache.
*/
{
  switch (name.sort) {
    case ifc_ns_text_offset:
      { an_ifc_text_offset ident = {name.mod, name.value};

        /* NameSort::Identifiers just refer to the string table. */
        cache_identifier(cache, get_string_at_offset(ident));
      }
      break;
    case ifc_ns_name_source_file:
      { Opt<an_ifc_name_source_file> opt_insf;

        construct_node(&opt_insf, name);
        if (!opt_insf.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_name_source_file insf = *opt_insf;
        an_ifc_text_offset      path = get_ifc_path(insf);
        cache_identifier(cache, get_string_at_offset(path));
      }
      break;
    case ifc_ns_name_template:
      { Opt<an_ifc_name_template> opt_int;

        construct_node(&opt_int, name);
        if (!opt_int.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_template);
        cache_name(cache, get_ifc_name(*opt_int));
      }
      break;
    case ifc_ns_name_specialization:
      { Opt<an_ifc_name_specialization> opt_ins;

        construct_node(&opt_ins, name);
        if (!opt_ins.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_name_specialization ins = *opt_ins;
        an_ifc_expr_index          args = get_ifc_arguments(ins);
        cache_token(cache, tok_template);
        cache_name(cache, get_ifc_primary(ins));
        cache_token(cache, tok_lt);
        if (!is_null_index(args)) {
          cache_expr(cache, args, /*cinfo=*/{});
        }  /* if */
        cache_token(cache, tok_gt);
      }
      break;
    case ifc_ns_name_operator:
      { Opt<an_ifc_name_operator> opt_ino;

        construct_node(&opt_ino, name);
        if (!opt_ino.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_name_operator ino = *opt_ino;
        an_operator_kind     opkind = get_operator_kind(ino.get_module(),
                                                        get_ifc_operator(ino));
        an_ifc_text_offset   encoded = get_ifc_encoded(ino);
        if (opkind == opkind_c_cast || opkind == opkind_cpp_cast) {
          ifc_unexpected(this, "Unexpected operator kind");
          goto invalid;
        }  /* if */
        cache_token(cache, tok_operator);
        cache_tokens_from_string(get_string_at_offset(encoded),
                                 cache);
      }
      break;
    case ifc_ns_name_conversion:
      { Opt<an_ifc_name_conversion> opt_inc;

        construct_node(&opt_inc, name);
        if (!opt_inc.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_operator);
        cache_type(cache, get_ifc_target(*opt_inc));
      }
      break;
    case ifc_ns_name_literal:
      { Opt<an_ifc_name_literal> opt_inl;

        construct_node(&opt_inl, name);
        if (!opt_inl.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_name_literal inl = *opt_inl;
        an_ifc_text_offset  encoded = get_ifc_encoded(inl);
        cache_token(cache, tok_operator);
        cache_identifier(cache, get_string_at_offset(encoded));
      }
      break;
    case ifc_ns_name_guide:
      { Opt<an_ifc_name_guide> opt_ing;

        construct_node(&opt_ing, name);
        if (!opt_ing.has_value()) {
          goto invalid;
        }  /* if */
        /* FIXME: Currently unsupported. */
        issue_unsupported_construct_error(this, "NameSort::Guide",
                                          &error_position);
      }
      break;
    default_is_unexpected();
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad name cache");
  cache->invalidate();
done:;
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
  } else if (suppress_automatic_namespace_qualification &&
             (scope->kind == sck_namespace || scope->kind == sck_file)) {
    result = FALSE;
  }  /* if */
  return result;
}  /* should_cache_nested_name_specifier_for_scope */


void an_ifc_module::cache_scope_as_nested_name_specifier(
                                                a_module_token_cache_ptr cache,
                                                a_scope_ptr              scope)
/*
Add the tokens to cache representing a nested-name-specifier for scope.
*/
{
  if (should_cache_nested_name_specifier_for_scope(scope)) {
    if (scope_is(scope, sck_file) || scope_is(scope->parent, sck_file)) {
      cache_token(cache, tok_colon_colon);
    } else {
      /* Attempt to generate any parent scope's qualifiers. */
      cache_scope_as_nested_name_specifier(cache, scope->parent);
    }  /* if */
    /* Generate the current scope's qualifier. */
    if (scope_is(scope, sck_class_struct_union) ||
        scope_is(scope, sck_enum)) {
      a_type_ptr type_ptr = scope->variant.assoc_type;
      check_assertion(type_ptr != NULL);
      cache_identifier(cache, type_ptr->source_corresp.name);
      cache_token(cache, tok_colon_colon);
    } else if (scope_is(scope, sck_namespace)) {
      a_namespace_ptr namespace_ptr = scope->variant.assoc_namespace;
      cache_identifier(cache, namespace_ptr->source_corresp.name);
      cache_token(cache, tok_colon_colon);
    }  /* if */
  }  /* if */
}  /* cache_scope_as_nested_name_specifier */


void an_ifc_module::cache_nested_name_specifier_from_decl(
                                                a_module_token_cache_ptr cache,
                                                an_ifc_decl_index        decl)
/*
Add the tokens corresponding to the given declaration's (decl)
nested-name-specifier to cache.
*/
{
  a_module_entity_ptr mep = get_ifc_module_entity_ptr(decl);

  if (mep->scope == NULL && has_ifc_home_scope(decl)) {
    /* Load the module entity pointer scope if not already processed. */
    mep->scope = get_home_scope(decl);
  }  /* if */
  this->cache_scope_as_nested_name_specifier(cache, mep->scope);
}  /* cache_nested_name_specifier_from_decl */


void an_ifc_module::cache_qualified_name_from_decl(
                                                a_module_token_cache_ptr cache,
                                                an_ifc_decl_index        decl)
/*
Add the tokens corresponding to the given declaration's (decl) qualified-id to
cache.
*/
{
  cache_nested_name_specifier_from_decl(cache, decl);
  cache_identifier(cache, name_from_decl(decl));
}  /* cache_qualified_name_from_decl */


void an_ifc_module::cache_name_from_decl(a_module_token_cache_ptr cache,
                                         an_ifc_decl_index        decl)
/*
Add the tokens corresponding to the given declaration's (decl) name to cache.
*/
{
  a_const_char *name = name_from_decl(decl);

  if (name != NULL) {
    cache_identifier(cache, name);
  } else {
    check_assertion_str(is_at_least_one_error(),
                        "expected errors from name_from_decl");
  } /* if */
}  /* cache_name_from_decl */


static a_boolean func_macro_is_variadic(const an_ifc_variadic_arity& arity)
/*
Given a variadic arity return TRUE if the associated function macro is
variadic; otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  /* FIXME: This is odd, we should refine this part of the spec with Microsoft.
     The endianness concerns/handling of the IFC seemingly compete with the
     layout of this data. */
  static_assert(sizeof(an_ifc_variadic_arity_storage) == 4,
                "Unexpected variadic arity storage size.");
  a_byte variadic_byte = (a_byte)((*arity.get_storage())[3]);
  if (variadic_byte & (0x1 << 7)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* func_macro_is_variadic */


void an_ifc_module::cache_macro(a_module_token_cache_ptr cache,
                                an_ifc_macro_index       macro)
/*
Add the tokens corresponding to the given macro's definition to cache.
*/
{
  an_ifc_source_position_hint pos_hint(cache, macro);

  switch (macro.sort) {
    case ifc_ms_macro_object_like:
      { Opt<an_ifc_macro_object_like> opt_imol;

        construct_node(&opt_imol, macro);
        if (!opt_imol.has_value()) {
          goto invalid;
        }  /* if */
        cache_identifier(cache, get_string_at_offset(get_ifc_name(*opt_imol)));
        /* Ensure there's a space between the macro identifier and the macro
           body. */
        cache_pp_token(cache, " ", 1);
        cache_form(cache, get_ifc_body(*opt_imol));
      }
      break;
    case ifc_ms_macro_function_like:
      { Opt<an_ifc_macro_function_like> opt_imfl;

        construct_node(&opt_imfl, macro);
        if (!opt_imfl.has_value()) {
          goto invalid;
        }  /* if */

        cache_identifier(cache, get_string_at_offset(get_ifc_name(*opt_imfl)));
        cache_token(cache, tok_lparen);
        if (func_macro_is_variadic(get_ifc_arity_variadic(*opt_imfl))) {
          cache_token(cache, tok_ellipsis);
        } else {
          an_ifc_form_index parameters = get_ifc_parameters(*opt_imfl);
          check_assertion(!is_null_index(parameters));
          cache_form(cache, parameters, /*is_parameter_form=*/TRUE);
        }  /* if */
        cache_token(cache, tok_rparen);
        /* Ensure there's a space between the macro parameter list and the
           macro body. */
        cache_pp_token(cache, " ", 1);
        cache_form(cache, get_ifc_body(*opt_imfl));
      }
      break;
    default_is_unexpected_str("Unexpected MacroSort");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad macro cache");
  cache->invalidate();
done:;
}  /* cache_macro */


static void cache_form_spelling(a_module_token_cache_ptr     cache,
                                an_ifc_text_offset           spelling)
/*
Cache the given preprocessed form.
*/
{
  a_const_char *str = get_string_at_offset(spelling);

  cache_pp_token(cache, str, strlen(str));
}  /* cache_form_spelling */


void an_ifc_module::cache_form(a_module_token_cache_ptr cache,
                               an_ifc_form_index        form,
                               a_boolean                is_parameter_form)
/*
Add tokens corresponding to the given preprocessing "form" to cache.  If this
is for a function-like macro's parameters then is_parameter_form is TRUE,
otherwise is_parameter_form is FALSE.

FIXME: Many of these see non-identifiers cached as identifiers due to the
raw-text spelling.
*/
{
  an_ifc_source_position_hint pos_hint(cache, form);

  switch (form.sort) {
    case ifc_fs_form_identifier:
      { Opt<an_ifc_form_identifier> opt_ifi;

        construct_node(&opt_ifi, form);
        if (!opt_ifi.has_value()) {
          goto invalid;
        }  /* if */
        cache_identifier(cache,
                         get_string_at_offset(get_ifc_spelling(*opt_ifi)));
      }
      break;
    case ifc_fs_form_number:
      { Opt<an_ifc_form_number> opt_ifn;

        construct_node(&opt_ifn, form);
        if (!opt_ifn.has_value()) {
          goto invalid;
        }  /* if */
        cache_form_spelling(cache, get_ifc_spelling(*opt_ifn));
      }
      break;
    case ifc_fs_form_character:
      { Opt<an_ifc_form_character> opt_ifc;

        construct_node(&opt_ifc, form);
        if (!opt_ifc.has_value()) {
          goto invalid;
        }  /* if */
        cache_form_spelling(cache, get_ifc_spelling(*opt_ifc));
      }
      break;
    case ifc_fs_form_string:
      { Opt<an_ifc_form_string> opt_ifs;

        construct_node(&opt_ifs, form);
        if (!opt_ifs.has_value()) {
          goto invalid;
        }  /* if */
        cache_form_spelling(cache, get_ifc_spelling(*opt_ifs));
      }
      break;
    case ifc_fs_form_operator:
      { Opt<an_ifc_form_operator> opt_ifo;

        construct_node(&opt_ifo, form);
        if (!opt_ifo.has_value()) {
          goto invalid;
        }  /* if */
        cache_form_spelling(cache, get_ifc_spelling(*opt_ifo));
      }
      break;
    case ifc_fs_form_keyword:
      { Opt<an_ifc_form_keyword> opt_ifk;

        construct_node(&opt_ifk, form);
        if (!opt_ifk.has_value()) {
          goto invalid;
        }  /* if */
        cache_form_spelling(cache, get_ifc_spelling(*opt_ifk));
      }
      break;
    case ifc_fs_form_parameter:
      { Opt<an_ifc_form_parameter> opt_ifp;

        construct_node(&opt_ifp, form);
        if (!opt_ifp.has_value()) {
          goto invalid;
        }  /* if */
        cache_form_spelling(cache, get_ifc_spelling(*opt_ifp));
      }
      break;
    case ifc_fs_form_header:
      { Opt<an_ifc_form_header> opt_ifh;

        construct_node(&opt_ifh, form);
        if (!opt_ifh.has_value()) {
          goto invalid;
        }  /* if */
        cache_form_spelling(cache, get_ifc_spelling(*opt_ifh));
      }
      break;
    case ifc_fs_form_junk:
      { Opt<an_ifc_form_junk> opt_ifj;

        construct_node(&opt_ifj, form);
        if (!opt_ifj.has_value()) {
          goto invalid;
        }  /* if */
        cache_form_spelling(cache, get_ifc_spelling(*opt_ifj));
      }
      break;
    case ifc_fs_form_whitespace:
      { Opt<an_ifc_form_whitespace> opt_ifw;

        construct_node(&opt_ifw, form);
        if (!opt_ifw.has_value()) {
          goto invalid;
        }  /* if */
        /* FIXME: Ideally the whitespace information will contain the amount
           and kind of whitespace, and we'd cache that. */
        cache_pp_token(cache, " ", 1);
      }
      break;
    case ifc_fs_form_stringize:
      { Opt<an_ifc_form_stringize> opt_ifs;

        construct_node(&opt_ifs, form);
        if (!opt_ifs.has_value()) {
          goto invalid;
        }  /* if */
        cache_pp_token(cache, "#", /*len=*/1);
        cache_form(cache, get_ifc_operand(*opt_ifs));
      }
      break;
    case ifc_fs_form_catenate:
      { Opt<an_ifc_form_catenate> opt_ifc;

        construct_node(&opt_ifc, form);
        if (!opt_ifc.has_value()) {
          goto invalid;
        }  /* if */
        cache_form(cache, get_ifc_first(*opt_ifc));
        cache_pp_token(cache, "##", /*len=*/2);
        cache_form(cache, get_ifc_second(*opt_ifc));
      }
      break;
    case ifc_fs_form_pragma:
      { Opt<an_ifc_form_pragma> opt_ifp;

        construct_node(&opt_ifp, form);
        if (!opt_ifp.has_value()) {
          goto invalid;
        }  /* if */

        an_ifc_form_pragma ifp = *opt_ifp;
        an_ifc_form_index  operand = get_ifc_operand(ifp);
        if (operand.sort == ifc_fs_form_string) {
          cache_pp_token(cache, "_Pragma", /*len=*/7);
        } else {
          check_assertion(operand.sort == ifc_fs_form_tuple);
          cache_pp_token(cache, "__pragma", /*len=*/8);
        }  /* if */
        cache_token(cache, tok_lparen);
        cache_form(cache, operand);
        cache_token(cache, tok_rparen);
      }
      break;
    case ifc_fs_form_parenthesized:
      { Opt<an_ifc_form_parenthesized> opt_ifp;

        construct_node(&opt_ifp, form);
        if (!opt_ifp.has_value()) {
          goto invalid;
        }  /* if */
        cache_token(cache, tok_lparen);
        cache_form(cache, get_ifc_operand(*opt_ifp));
        cache_token(cache, tok_rparen);
      }
      break;
    case ifc_fs_form_tuple:
      { Opt<an_ifc_form_tuple> opt_ift;

        construct_node(&opt_ift, form);
        if (!opt_ift.has_value()) {
          goto invalid;
        }  /* if */

        a_pp_heap_traverser traverser(*opt_ift);
        for (an_Indexed<an_ifc_heap_pp_form> indexed_ihpf : traverser) {
          if (!indexed_ihpf.has_value()) {
            goto invalid;
          }  /* if */

          if (!is_first(traverser, indexed_ihpf)) {
            /* FIXME: Find a proper source position for these. */
            if (is_parameter_form) {
              cache_token(cache, tok_comma);
            } else {
              /* FIXME: Currently IFC macros do not have whitespace encoded,
                 so add our own to ensure entities do not bleed together.
                 This is not always the correct thing to do, but there are
                 fewer issues than with not doing this at all. */
              cache_pp_token(cache, " ", 1);
            }  /* if */
          }  /* if */
          cache_form(cache, get_ifc_value(*indexed_ihpf));
        }  /* for */
      }
      break;
    default_is_unexpected_str("Unexpected FormSort");
  }  /* switch */
  goto done;
invalid:
  expect_error_str("expected errors for bad form cache");
  cache->invalidate();
done:;
}  /* cache_form */


static void add_backtrace(a_diagnostic_ptr              diag_ptr,
                          const an_ifc_validation_trace *trace)
/*
Add information about the validation stack to the given diagnostic pointer to
produce a more detailed contextual diagnostic.
*/
{
  const an_ifc_validation_trace *cur = trace;

  while (cur != NULL) {
    switch (cur->trace_kind) {
      case ifc_vtk_field:
        { a_const_char *field_name = cur->field_info.name;
          size_t       offset = cur->field_info.offset;

          /* FIXME: Migrate to allowing diagnostics with size_t. */
          st_num_add_diag_info(diag_ptr,
                               ec_invalid_ifc_position_backtrace_field,
                               field_name, (uint32_t)offset);
        }
        break;
      case ifc_vtk_partition:
        { an_ifc_module               *mod = cur->partition_info.mod;
          an_ifc_partition_kind       kind = cur->partition_info.kind;
          an_ifc_index_type           idx = cur->partition_info.idx;
          an_ifc_partition_kind_index part_kind_idx = {mod, kind, idx};
          an_ifc_partition_metadata   *part_meta =
                                         get_partition_metadata(part_kind_idx);
          size_t                      part_start = part_meta->offset;
          size_t                      abs_offset =
                                           get_partition_offset(part_kind_idx);
          size_t                      rel_offset = abs_offset - part_start;

          /* FIXME: Migrate to allowing diagnostics with size_t. */
          st_num3_add_diag_info(diag_ptr,
                                ec_invalid_ifc_position_backtrace_pos,
                                get_partition_name_from_kind(kind), idx,
                                (uint32_t)abs_offset, (uint32_t)rel_offset);
        }
        break;
      default_is_unexpected();
    }  /* switch */
    /* Move down the stack. */
    cur = cur->parent;
  }  /* while */
}  /* add_backtrace */


void invalid_sort(an_ifc_module                 *mod,
                  const an_ifc_validation_trace *trace)
/*
Given the associated module and validation trace, emit a diagnostic for an
encountered invalid sort value.
*/
{
  a_diagnostic_ptr diag_ptr;

  /* FIXME: Use a better source position. */
  diag_ptr = pos_st_start_error(ec_invalid_ifc_sort_value,
                                &null_source_position,
                                mod->assoc_module_info->name);
  add_backtrace(diag_ptr, trace);
  end_diagnostic(diag_ptr);
}  /* invalid_partition */


void invalid_partition(an_ifc_module                 *mod,
                       const an_ifc_validation_trace *trace)
/*
Given the associated module and validation trace, emit a diagnostic for an
encountered invalid partition.
*/
{
  a_diagnostic_ptr diag_ptr;

  /* FIXME: Use a better source position. */
  diag_ptr = pos_st_start_error(ec_invalid_ifc_partition,
                                &null_source_position,
                                mod->assoc_module_info->name);
  add_backtrace(diag_ptr, trace);
  end_diagnostic(diag_ptr);
}  /* invalid_partition */


static void diag_undefined_partition(an_ifc_module                 *mod,
                                     an_ifc_partition_kind         part_kind,
                                     const an_ifc_validation_trace *trace)
/*
Given the associated module, encountered undefined partition kind, and
validation trace, handle failure and diagnostics for an encountered undefined
partition.
*/
{
  a_const_char     *part_name = get_partition_name_from_kind(part_kind);
  a_diagnostic_ptr diag_ptr;

  /* FIXME: Use a better source position. */
  diag_ptr = pos_st2_start_error(ec_undefined_ifc_partition,
                                 &null_source_position,
                                 mod->assoc_module_info->name, part_name);
  add_backtrace(diag_ptr, trace);
  end_diagnostic(diag_ptr);
}  /* undefined_partition */


static void diag_partition_position(
                                 an_error_code                 error_code,
                                 an_ifc_module                 *mod,
                                 an_ifc_partition_kind         part_kind,
                                 size_t                        file_offset,
                                 size_t                        relative_offset,
                                 const an_ifc_validation_trace *trace)
/*
Given the error code to diagnose with, the associated module, partition kind,
offset into the file, relative offset from the start of the partition, and
validation trace, handle failure and diagnostics for an encountered overflowing
partition.
*/
{
  a_const_char     *part_name = get_partition_name_from_kind(part_kind);
  a_diagnostic_ptr diag_ptr;

  /* FIXME: Use a better source position. */
  diag_ptr = pos_st2_num2_start_error(error_code, &null_source_position,
                                      mod->assoc_module_info->name,
                                      part_name, (int32_t)file_offset,
                                      (int32_t)relative_offset);
  add_backtrace(diag_ptr, trace);
  end_diagnostic(diag_ptr);
}  /* diag_overflowing_partition */


a_boolean validate_element_exists(an_ifc_module                 *mod,
                                  an_ifc_partition_kind         partition_kind,
                                  an_ifc_index_type             index,
                                  const an_ifc_validation_trace *trace)
/*
Check that the given partition index exists for the given module.  If the index
exists, return TRUE.  Otherwise, return FALSE and emit an appropriate
diagnostic using the validation trace.
*/
{
  a_boolean                   result = TRUE;
  an_ifc_partition_kind_index part_index = {mod, partition_kind, index};
  an_ifc_partition_metadata   *partition_metadata =
                                            get_partition_metadata(part_index);
  size_t                      partition_entry_size =
                                                partition_metadata->entry_size;
  size_t                      partition_offset = partition_metadata->offset;
  size_t                      partition_size = partition_metadata->size;

  if (partition_size == 0) {
    /* Check that anything is stored in the requested partition. */
    result = FALSE;
    diag_undefined_partition(mod, partition_kind, trace);
  } else {
    size_t file_offset = get_partition_offset(part_index);
    size_t relative_offset = file_offset - partition_offset;

    if ((relative_offset + partition_entry_size) > partition_size) {
      /* Check that the relative offset is within the partition. */
      result = FALSE;
      diag_partition_position(ec_invalid_overflowing_ifc_position, mod,
                              partition_kind, file_offset, relative_offset,
                              trace);
    } else if ((relative_offset % partition_entry_size) != 0) {
      /* Check that the relative offset is at a given position. */
      result = FALSE;
      diag_partition_position(ec_invalid_misaligned_ifc_position, mod,
                              partition_kind, file_offset, relative_offset,
                              trace);
    }  /* if */
  }  /* if */
  return result;
}  /* validate_element_exists */


an_ifc_msvc_traits_bitfield an_ifc_module::get_vendor_traits(
                                                        an_ifc_decl_index decl)
/*
Find and return the associated vendor traits corresponding to the given decl.
If not found, return the appropriate trait to indicate "none".
*/
{
  an_ifc_msvc_traits_bitfield         result = {};
  Opt<an_ifc_trait_msvc_vendor_trait> opt_itmvt;

  find_trait(&opt_itmvt, decl);
  if (opt_itmvt.has_value()) {
    result = get_ifc_trait(*opt_itmvt);
  }  /* if */
  return result;
}  /* get_vendor_traits */


char *an_ifc_module::parse_cached_explicit_specialization(
                                   a_module_token_cache_ptr         cache,
                                   a_scope_ptr                      encl_scope,
                                   const an_ifc_decl_specialization &decl,
                                   an_il_entry_kind                 *kind)
/*
Parse the tokens corresponding to the given explicit specialization
declaration's (decl) cache.  encl_scope is the scope containing the explicit
specialization declaration.  Return a pointer to the corresponding explicit
specialization entity and update kind with the associated entity kind.
*/
{
  a_decl_parse_state dps;
  a_tmpl_decl_state  decl_state;
  a_token_kind       final_token = tok_error;

#if DEBUG
  if (db_flag_is_set("ms_ifc_token_def")) {
    fprintf(f_debug, "Reconstituted explicit specialization declaration:\n");
    db_tokens(cache);
    fprintf(f_debug, "\n---------------------\n");
  }  /* if */
#endif /* DEBUG */
  check_assertion(get_ifc_sort(decl) == ifc_ss_explicit);
  {
    a_module_entity_rescan rescan(cache, &final_token);

    prepare_cached_template_parse(cache, encl_scope, &dps, &decl_state,
                                  &final_token);
    template_or_specialization_declaration_full(&decl_state,
                                                /*is_generic=*/FALSE,
                                                /*orig_dps=*/NULL);
  }
  return get_parsed_entity(&dps, kind);
}  /* parse_cached_explicit_specialization */


char *an_ifc_module::parse_cached_explicit_instantiation(
                                        a_module_token_cache_ptr         cache,
                                        const an_ifc_decl_specialization &decl,
                                        an_il_entry_kind                 *kind)
/*
Parse the tokens corresponding to the given explicit instantiation
declaration's (decl) cache.  Return a pointer to the corresponding
explicitly-instantiated entity and update kind with the associated entity kind.
*/
{
  a_decl_parse_state dps;
  a_source_position  template_kw_pos;

#if DEBUG
  if (db_flag_is_set("ms_ifc_token_def")) {
    fprintf(f_debug, "Reconstituted explicit instantiation declaration:\n");
    db_tokens(cache);
    fprintf(f_debug, "\n---------------------\n");
  }  /* if */
#endif /* DEBUG */
  check_assertion(get_ifc_sort(decl) == ifc_ss_instantiation);

  {
    a_module_entity_rescan rescan(cache);

    source_position_from_locus(&template_kw_pos, get_ifc_locus(decl));
    a_template_decl_options_set options = TDO_EXTERN;
    /* An explicit instantiation in a module means that the module clients can
       handle the equivalent of an "extern template" directive.  However, when
       importing a header unit (which should behave more like a #include) the
       explicit instantiation directive should remain an ordinary instantiation
       directive. */
    if (is_header_unit(this->assoc_module_info)) options = TDO_NO_OPTIONS;
    explicit_instantiation(&dps, options, &template_kw_pos);
  }
  return get_parsed_entity(&dps, kind);
}  /* parse_cached_explicit_instantiation */

#if DEBUG

void an_ifc_module::db_ifc_file_header() const
/*
Display the contents of the IFC file header.
*/
{
#if 0
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
#endif /* 0 */
}  /* db_ifc_file_header */


void an_ifc_module::db_ifc_scope(an_ifc_scope_index scope)
/*
Display the contents of the specified scope.
*/
{
  auto db_scope_member = [this](const an_ifc_scope_member &ism) {
    this->db_ifc_declaration(get_ifc_index(ism));
  };

  traverse_scope_members(scope, db_scope_member);
}  /* db_ifc_scope */


void an_ifc_module::db_ifc_declaration(an_ifc_decl_index decl)
/*
Display the contents of the specified declaration.
*/
{
  a_module_token_cache cache;

  cache_decl(&cache, decl, /*cinfo=*/{});
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
      pch_saved_var_array_elem(ifc_function_bodies),
      pch_saved_var_array_elem(ifc_template_definitions),
      pch_saved_var_array_elem(ifc_decl_lookup_table),
      pch_saved_var_array_elem(ifc_decl_template_lookup_table),
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
  ifc_template_definitions = alloc_fe_of_type(an_ifc_template_def_map);
  construct(ifc_template_definitions, /*mask_width=*/10);
  ifc_bad_function_bodies = alloc_fe_of_type(an_ifc_function_failure_map);
  construct(ifc_bad_function_bodies, /*mask_width=*/10);
  ifc_decl_lookup_table = alloc_fe_of_type(an_ifc_decl_lookup_table);
  construct(ifc_decl_lookup_table, /*mask_width=*/10);
  ifc_decl_template_lookup_table = alloc_fe_of_type(
                                                 an_ifc_template_lookup_table);
  construct(ifc_decl_template_lookup_table, /*mask_width=*/10);
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
* Copyright 1988-2022 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
