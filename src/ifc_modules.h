/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2017-2023 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

ifc_modules.h -- Declarations relating to ifc_modules.c (having to do with
                 IFC-based modules).

*/

/* Avoid including these declarations more than once: */
#ifndef IFC_MODULES_H
#define IFC_MODULES_H 1

#if !STANDALONE_UTILITY_PROGRAM

#include "ifc_map.h"

#include "util.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

typedef struct a_tmpl_decl_state *a_tmpl_decl_state_ptr;

/* FIXME: Temporarily disable "not referenced" warnings until completed. */
/*lint -save -e755 -e758 -e768 -e769*/

typedef uint64_t a_module_ref_key;

/*
Magic numbers that identify the beginning of a Microsoft IFC file.
*/
constexpr a_byte ms_ifc_magic_numbers[] = { 0x54, 0x51, 0x45, 0x1A };

/*
Magic numbers that identify the beginning of an EDG IFC file.
*/
constexpr a_byte edg_ifc_magic_numbers[] = { 0x54, 0x51, 0x45, 0x2C };

/*
An internal representation of an IFC partition metadata.
*/
struct an_ifc_partition_metadata {
  a_const_char  *name;  /* The name of the partition in the IFC file. */
  size_t        offset; /* An offset from the beginning of the file to the
                           start of the partition. */
  uint32_t      size;   /* The number of bytes in the partition. */
  uint32_t      entry_size;
                        /* The size of an entry in the partition. */
  uint32_t      *format_validated;
                        /* An array of bits for checking IFC format validation.

                           The lower 16 bits of each "block" (uint32_t) are
                           used to represented whether validation was performed
                           (1 is TRUE, 0 is FALSE).  The higher 16 bits are
                           used to represent whether the validated element was
                           invalid (1 is TRUE, 0 is FALSE).

                           This approach is taken to optimize both for space
                           (as only two bits are used for each element) and
                           memory locality (as the validated and invalid flags
                           are always contained within the same 32-bit
                           integer).  Invalid is represented as the TRUE state
                           to reduce the number of writes in the "happy
                           path." */
};  /* an_ifc_partition_metadata */

namespace detail {

/*
The following specializations provide Is_trivially_copyable and
Is_trivially_destructible support for ifc_map.h types.
*/

template<>
struct Is_trivially_copyable_edg_impl<an_ifc_attr_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_copyable_edg_impl<an_ifc_decl_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_copyable_edg_impl<an_ifc_edg_basic_token_sort> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_copyable_edg_impl<an_ifc_edg_complex_token_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_copyable_edg_impl<an_ifc_type_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_attr_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_decl_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_edg_basic_token_sort> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_edg_complex_token_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_type_index> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */


/*
The internal bit representation of an_ifc_cache_info.  This type is extracted
to allow easy zeroing via aggregate initialization.  This type should not be
used directly.
*/
struct an_ifc_cache_info_zero_bits {
  a_bit_field   dependent_name:1;
                        /* TRUE if a (potentially resolved) IFC name EDG must
                           treat as dependent is being cached. */
  a_bit_field   qualified_name:1;
                        /* TRUE if a qualified name is being cached. */
  a_bit_field   skip_assign:1;
                        /* TRUE if assignments/initializers should be excluded
                           from the cache. */
  a_bit_field   possible_temporary_decl:1;
                        /* TRUE if the entity being cached could potentially
                           be a temporary declaration. */
  a_bit_field   in_block_scope:1;
                        /* TRUE if the entities being cached are part of a
                           block scope. */
  a_bit_field   in_lambda_body:1;
                        /* TRUE if currently processing a lambda. */
  a_bit_field   is_specialization:1;
                        /* TRUE if the entity being cached is a template
                           specialization. */
  a_bit_field   inline_data_member_type:1;
                        /* TRUE if the entity being cached is a data member
                           with an anonymous inline type. */
  a_bit_field   requires_body:1;
                        /* TRUE if the entity being cached is part of a
                           requires clause body. */
  a_bit_field   no_final_semicolon:1;
                        /* TRUE if the final semicolon should be omitted. */
  a_bit_field   nested_expr:1;
                        /* TRUE if the expression being cached is nested within
                           another expression and should be parenthesized. */
  a_bit_field   ignore_definition:1;
                        /* TRUE if any definition being cached should be
                           ignored. */
  a_bit_field   ignore_default_arguments:1;
                        /* TRUE if any function declaration being cached should
                           be cached without its default arguments. */
  a_bit_field   ignore_default_template_arguments:1;
                        /* TRUE if any template declaration being cached should
                           be cached without its default template arguments. */
  a_bit_field   no_access_specifier:1;
                        /* TRUE if access specifiers should not be cached . */
};  /* an_ifc_cache_info_bits */

}  /* detail */

/*
A structure to contain flags and other information needed to properly cache
IFC entities.
*/
struct an_ifc_cache_info : public detail::an_ifc_cache_info_zero_bits {
  an_ifc_cache_info() :
    an_ifc_cache_info_zero_bits{}, lexical_scope{}, parameterizing_entity{}
  {}
  an_ifc_decl_index
                lexical_scope;
                        /* The declaration index of the scope that this entity
                           will be cached into. */
  an_ifc_decl_index
                parameterizing_entity;
                        /* If the entity being cached is parameterized (e.g.,
                           is an entity representing the parameterized
                           declaration of a template or one of its
                           specialization), this is the declaration index of
                           the parameterizing entity (i.e., the template or
                           specialization declaration itself); otherwise a null
                           index. */
};  /* an_ifc_cache_info */

struct a_partial_scope_stack_state;
struct a_str_control_block;
struct an_ifc_module;

#if !ASSUME_LITTLE_ENDIAN_IFC_MODULES

enum an_ifc_module_primary_endianness {
  ifc_mpe_little,       /* The primary endianness of the module is
                           little-endian. */
  ifc_mpe_big,          /* The primary endianness of the module is
                           big-endian. */
  ifc_mpe_unknown       /* The primary endianness of the module is
                           unknown. */
};

#endif /* !ASSUME_LITTLE_ENDIAN_IFC_MODULES */

/*
The minimum major and minor supported version combination.
*/
#define IFC_MIN_VER_MAJOR 0
#define IFC_MIN_VER_MINOR 33

/*
State for an_ifc_module_file when the object represents an IFC file read.
*/
struct an_ifc_module_file_read_state {
  an_ifc_module *mod = NULL;
                        /* The IFC module using this file (if any). */
  size_t        f_size = 0;
                        /* The size of the module file. */
#if USE_MMAP_FOR_MEMORY_REGIONS
  void          *mmap_addr = NULL;
                        /* A pointer to the memory-mapped beginning of the
                           module file. */
  size_t        mmap_size = 0;
                        /* The size of the memory-mapped partition. */
#if EDG_WIN32
  a_windows_handle
                mapped_input = NULL;
                        /* A HANDLE returned by CreateFile_interface during
                           the mapping process on Windows. */
  a_windows_handle
                map_object = NULL;
                        /* A HANDLE returned by CreateFileMapping during the
                           mapping process on Windows. */
#endif /* EDG_WIN32 */
  unsigned char
                *byte_buffer = NULL;
                        /* Pointer to the current position in the buffer
                           used by get_byte, etc. */
  unsigned char
                *buffer_end = NULL;
                        /* Pointer to the last byte of the buffer used by
                           get_byte, etc. */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
};  /* an_ifc_module_file_read_state */

/*
State for an_ifc_module_file when the object represents an IFC file write.
*/
struct an_ifc_module_file_write_state {
  an_error_code file_kind = ec_edg_ifc_file;
                        /* If an error occurs related to IO with this output
                           file, this error code will be used to name the kind
                           of file for the error. */
  a_const_char  *source_file_name;
                        /* The source file name that was used to build this
                           module file. */
};  /* an_ifc_module_file_write_state */

/*
A structure abstracting the representation of the IFC module file and the
associated memory managed.
*/
struct an_ifc_module_file {
  an_ifc_module_file(a_module_file_kind mk, a_boolean for_read_val = TRUE);
  an_ifc_module_file(an_ifc_module_file &&old);
  ~an_ifc_module_file();

  an_ifc_module_file &operator=(an_ifc_module_file &&old);

  void close();

  a_boolean is_for_read() const
    { return this->for_read; }

  an_ifc_module_file_read_state& get_read_state()
    { check_assertion(this->for_read); return this->read_state; }
  an_ifc_module_file_read_state const& get_read_state() const
    { check_assertion(this->for_read); return this->read_state; }

  an_ifc_module_file_write_state& get_write_state()
    { check_assertion(!this->for_read); return this->write_state; }
  an_ifc_module_file_write_state const& get_write_state() const
    { check_assertion(!this->for_read); return this->write_state; }

  a_module_file_kind
                module_kind;
                        /* The module file kind. */
  an_ifc_version_storage
                version_major = IFC_MIN_VER_MAJOR;
                        /* The module file's major version.  Defaulted to the
                           lowest supported version until initialization is
                           complete. */
  an_ifc_version_storage
                version_minor = IFC_MIN_VER_MINOR;
                        /* The module file's minor version.  Defaulted to the
                           lowest supported version until initialization is
                           complete. */
#if !ASSUME_LITTLE_ENDIAN_IFC_MODULES
  an_ifc_module_primary_endianness
                endianness = ifc_mpe_unknown;
                        /* The primary endianness of the module file. */
#endif /* !ASSUME_LITTLE_ENDIAN_IFC_MODULES */
  FILE          *f_module = NULL;
                        /* The file descriptor for the file. */
private:
  a_boolean     for_read;
                        /* TRUE if this represents an IFC module file that's
                           being read from.  FALSE if this represents an IFC
                           module file that's being written to. */
#ifdef UNION_AS_STRUCT
/* FIXME: Workaround for union-as-struct build issue. */
#undef union
#endif /* ifdef UNION_AS_STRUCT */
  union {
    /* When for_read is TRUE: */
    an_ifc_module_file_read_state
                read_state;
                        /* The state associated with reading an IFC file. */
    /* When for_read is FALSE: */
    an_ifc_module_file_write_state
                write_state;
                        /* The state associated with writing an IFC file. */
  };
#ifdef UNION_AS_STRUCT
#define union struct
#endif /* ifdef UNION_AS_STRUCT */
};  /* an_ifc_module_file */


/*
A structure abstracting the representation of the IFC string table and the
associated memory managed.
*/
struct an_ifc_module_string_table {
  an_ifc_module_string_table()
    : contents(NULL), size(0)
    {}
  an_ifc_module_string_table(char *contents_ptr, size_t size_val)
    : contents(contents_ptr), size(size_val)
    {}
  inline an_ifc_module_string_table(an_ifc_module_string_table &&old);
  inline ~an_ifc_module_string_table();

  inline an_ifc_module_string_table &operator=(
                                             an_ifc_module_string_table &&old);

  char          *contents;
                        /* The string table contents. */
  size_t        size;   /* The size of the string table. */
};  /* an_ifc_module_string_table */


an_ifc_module_string_table::an_ifc_module_string_table(
                                              an_ifc_module_string_table &&old)
/*
Move construct from the given IFC string table.
*/
  : an_ifc_module_string_table()
{
  swap_at(&old.contents, &this->contents);
  swap_at(&old.size, &this->size);
}  /* an_ifc_module_string_table::an_ifc_module_string_table */


an_ifc_module_string_table::~an_ifc_module_string_table()
/*
Destroy the given IFC string table.
*/
{
#if !USE_MMAP_FOR_MEMORY_REGIONS
  if (this->contents != NULL) {
    free_general(this->contents, this->size);
  }  /* if */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
}  /* an_ifc_module_string_table::~an_ifc_module_string_table */


an_ifc_module_string_table &an_ifc_module_string_table::operator=(
                                              an_ifc_module_string_table &&old)
/*
Move from the given IFC string table, returning self.
*/
{
  swap_at(&old.contents, &this->contents);
  swap_at(&old.size, &this->size);
  return *this;
}  /* an_ifc_module_string_table::operator= */


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
    an_ifc_line_number_storage
                max_line_number;
                        /* The maximum line number that will be seen in the
                           file.  Used to allocate a block of sequence numbers
                           that map to this file. */
  };
  an_ifc_file_header
                header = {};
                        /* The values of an IFC File_Header (byte-swapped if
                           necessary). */
  an_ifc_partition_metadata
                partitions[IFC_PARTITION_COUNT] = {};
                        /* Information about each of the IFC partitions that
                           could exist in a module file. */
  a_module_sequence_number_mapping
                *sequence_numbers = NULL;
                        /* A mapping of sequence numbers for the module,
                           indexed by a NameSort::SourceFile index.
                           Dynamically allocated (in front end memory) once
                           the number of source files is known. */
  an_ifc_module_file
                file = {mfk_unknown};
                        /* Information about the file backing this IFC module
                           interface. */
  an_ifc_module_string_table
                string_table = {};
                        /* The string table of the IFC file. */
  Small_dyn_array<a_token_kind, 10>
                textual_tokens = {};
                        /* An array mapping the values in the
                           .edg.token.complex.textual values to the
                           corresponding front end token kind or tok_error if
                           the token is not available.  This is resolved during
                           the initial file load to allow textual tokens to be
                           mapped to the corresponding token kind based on
                           their index (rather than the name associated with
                           the token) during caching. */
  a_tmpl_decl_state_ptr
                curr_templ_decl_state = NULL;
                        /* The current template declaration state, NULL if
                           there is no template declaration being processed. */
  a_boolean     references_any_modules = FALSE;
                        /* A flag set to TRUE when this module makes reference
                           to one or more external modules.  This is set when a
                           module is loaded. */
  Ptr_map<a_module_ref_key, a_module_import_decl_ptr>
                referenced_modules;
                        /* A map from a module reference to the corresponding
                           import decl.  Since modules are lazily added to this
                           list, it should not be assumed to be a complete
                           set. */
  a_boolean     suppress_friend_token = FALSE;
                        /* Flag to indicate that a "friend" keyword should be
                           suppressed (typically because the friendship is
                           handled at a higher level). */
public:
  an_ifc_module(a_module_file_kind mk)
    : a_module_interface(mk), referenced_modules(/*mask_width=*/4)
    {}
  ~an_ifc_module() EDG_NOEXCEPT = default;

  inline a_boolean is_open() const
    { return file.f_module != NULL; }

  a_boolean import(a_module_import_decl_ptr midp);
  void close();
  void pch_reset(a_module_import_decl_ptr midp);

  an_ifc_partition_metadata &get_partition_metadata(
                                              an_ifc_partition_kind part_kind);
  const an_ifc_partition_metadata &get_partition_metadata(
                                        an_ifc_partition_kind part_kind) const;

  void complete_definition_of_module_class(a_module_entity_ptr mep);
  a_boolean init_header(a_module_import_decl_ptr midp,
                         a_boolean               issue_diag);
  a_boolean initialize_members_from_ifc_module_file(
                                          a_module_import_decl_ptr midp,
                                          a_boolean                issue_diag);
  a_boolean open_and_map_ifc_module_file(a_module_import_decl_ptr midp,
                                         a_boolean                issue_diag);
  void import_referenced_modules(a_boolean impl_unit_importing_self);
  void define_ifc_macro(an_ifc_macro_index macro);
  void export_ifc_macros();
  uint32_t get_num_entries(an_ifc_partition_kind partition) const;
  /* IFC Scope readers. */
  a_type_ptr type_for_template_id(const an_ifc_expr_template_id &templ_id);
  a_boolean source_position_from_locus(a_source_position            *pos,
                                       const an_ifc_source_location &locus);
  a_boolean init_dps(a_decl_parse_state               *dps,
                     const an_ifc_source_location     &locus,
                     an_ifc_type_index                type_index,
                     an_ifc_object_traits_bitfield    traits,
                     an_ifc_msvc_traits_bitfield      msvc_traits,
                     an_ifc_basic_specifiers_bitfield specifiers,
                     an_ifc_access_sort               access,
                     an_ifc_expr_index                alignment,
                     a_partial_scope_stack_state      *psssp);
  void unsigned_integer_for_expr_index(an_ifc_expr_index expr_index,
                                       an_integer_value  *value);
  a_boolean fill_in_routine_parameter_defaults(
                                              an_ifc_chart_index params,
                                              a_type_ptr         rout_type,
                                              a_boolean          is_consteval);
  /* Token caching. */
  template<typename a_Name_Cache_Fn, typename a_Scope_Cache_Fn>
  inline void cache_scope_decl(a_module_token_cache_ptr     cache,
                               an_ifc_decl_index            decl_idx,
                               an_ifc_type_index            type,
                               a_Name_Cache_Fn              cache_name_fn,
                               a_Scope_Cache_Fn             cache_scope_fn);
  void cache_scope_decl(a_module_token_cache_ptr     cache,
                        an_ifc_decl_index            decl_idx,
                        an_ifc_type_index            type,
                        an_ifc_name_index            name,
                        an_ifc_type_index            base,
                        an_ifc_scope_offset          scope,
                        const an_ifc_cache_info      &cinfo);
  void cache_type_param_introducer(a_module_token_cache_ptr cache,
                                   an_ifc_expr_index        constraint,
                                   a_boolean                is_pack);
  void cache_attr(a_module_token_cache_ptr cache,
                  an_ifc_attr_index        attr,
                  a_boolean                cache_brackets);
  void cache_attrs(a_module_token_cache_ptr cache,
                   an_ifc_decl_index        decl_idx);
  void cache_function_parameters(a_module_token_cache_ptr     cache,
                                 an_ifc_chart_index           params,
                                 an_ifc_type_index            param_types,
                                 const an_ifc_source_location &pos);
  void cache_decl(a_module_token_cache_ptr cache,
                  an_ifc_decl_index        decl,
                  const an_ifc_cache_info  &cinfo);
  void cache_statement(a_module_token_cache_ptr cache,
                       an_ifc_stmt_index        stmt_idx,
                       const an_ifc_cache_info  &cinfo);
  void cache_syntax(a_module_token_cache_ptr cache,
                    an_ifc_syntax_index      syntax,
                    const an_ifc_cache_info  &cinfo);
  void cache_operator(a_module_token_cache_ptr cache,
                      an_ifc_operator_category op);
  void cache_operator(a_module_token_cache_ptr     cache,
                      an_ifc_niladic_operator_sort op);
  void cache_operator(a_module_token_cache_ptr     cache,
                      an_ifc_monadic_operator_sort op);
  void cache_operator(a_module_token_cache_ptr     cache,
                      an_ifc_dyadic_operator_sort op);
  void cache_operator(a_module_token_cache_ptr    cache,
                      an_ifc_triadic_operator_sort op);
  void cache_operator(a_module_token_cache_ptr                 cache,
                      an_ifc_storage_instruction_operator_sort op);
  void cache_operator(a_module_token_cache_ptr      cache,
                      an_ifc_variadic_operator_sort op);
  uint32_t try_cache_class_attributes_from_body(
                                       a_module_token_cache_ptr cache,
                                       an_ifc_sentence_index    body_sentence);
  uint32_t cache_decl_template_declaration(a_module_token_cache_ptr   cache,
                                           an_ifc_decl_index          decl_idx,
                                           const an_ifc_decl_template &decl,
                                           const an_ifc_cache_info    &cinfo);
  void cache_decl_template(a_module_token_cache_ptr   cache,
                           an_ifc_decl_index          decl_idx,
                           const an_ifc_decl_template &decl,
                           const an_ifc_cache_info    &cinfo);
  void cache_decl_partial_specialization(
                             a_module_token_cache_ptr                 cache,
                             an_ifc_decl_index                        decl_idx,
                             const an_ifc_decl_partial_specialization &decl,
                             an_ifc_cache_info                        cinfo);
  void cache_decl_specialization(a_module_token_cache_ptr         cache,
                                 an_ifc_decl_index                decl_idx,
                                 const an_ifc_decl_specialization &decl,
                                 an_ifc_cache_info                cinfo);
  void cache_name(a_module_token_cache_ptr     cache,
                  an_ifc_name_index            name);
  void cache_name_of_decl(a_module_token_cache_ptr cache,
                          an_ifc_decl_index        decl);
  void cache_macro(a_module_token_cache_ptr cache,
                   an_ifc_macro_index       macro);
  void cache_form(a_module_token_cache_ptr cache,
                  an_ifc_form_index        form,
                  a_boolean                is_parameter_form = FALSE);
  an_ifc_msvc_traits_bitfield get_vendor_traits(an_ifc_decl_index decl);
  char *parse_cached_explicit_specialization(
                                   a_module_token_cache_ptr         cache,
                                   a_scope_ptr                      encl_scope,
                                   const an_ifc_decl_specialization &decl,
                                   an_il_entry_kind                 *kind);
  char *parse_cached_explicit_instantiation(
                                       a_module_token_cache_ptr         cache,
                                       const an_ifc_decl_specialization &decl,
                                       an_il_entry_kind                 *kind);
#if DEBUG
  void db_ifc_file_header() const;
  void db_ifc_scope(an_ifc_scope_offset scope);
  void db_ifc_declaration(an_ifc_decl_index decl);
#if EXPENSIVE_CHECKING
  void f_db_get_byte(a_const_char *value_str,
                     void         *addr,
                     size_t       length) const;
#endif /* EXPENSIVE_CHECKING */
#endif /* DEBUG */
};  /* an_ifc_module */
/*lint -restore*/


#if DEBUG && EXPENSIVE_CHECKING
EXTERN const an_ifc_partition_metadata
                *debug_partition;
                        /* Points to information about the partition currently
                           being read (for debugging purposes only). */
#endif /* DEBUG && EXPENSIVE_CHECKING */


extern void get_bytes(an_ifc_module_file *file,
                      void               *entity,
                      size_t             length,
                      a_boolean          header_bytes);

#if USE_MMAP_FOR_MEMORY_REGIONS

inline unsigned char* get_byte_buffer(an_ifc_module_file *file)
/*
Return the byte buffer position that should be read from for the given
read-only memory mapped IFC module file.
*/
{
  /* The byte buffer can only be retrieved in read contexts. */
  return file->get_read_state().byte_buffer;
}  /* get_byte_buffer */

#endif /* USE_MMAP_FOR_MEMORY_REGIONS */

extern a_boolean is_at_least(an_ifc_module_file     *file,
                             an_ifc_version_storage minimum_version_major,
                             an_ifc_version_storage minimum_version_minor);


inline a_boolean is_at_least(an_ifc_module          *mod,
                             an_ifc_version_storage minimum_version_major,
                             an_ifc_version_storage minimum_version_minor)
/*
A convenience function for is_at_least that works directly on the module
interface instead of the underlying file.
*/
{
  return is_at_least(&mod->file, minimum_version_major, minimum_version_minor);
}  /* is_at_least */

#if ASSUME_LITTLE_ENDIAN_IFC_MODULES

constexpr inline a_boolean has_matching_endianness(
                                           ARG_UNUSED an_ifc_module_file *file)
/*
In modes where little endian modules are assumed, simply return TRUE
unconditionally.
*/
{
  return TRUE;
}  /* has_matching_endianness */

#else /* !ASSUME_LITTLE_ENDIAN_IFC_MODULES */

extern a_boolean has_matching_endianness(an_ifc_module_file *file);

#endif /* ASSUME_LITTLE_ENDIAN_IFC_MODULES */


extern void init_byte_buffer(an_ifc_module_file *file,
                             size_t             offset,
                             ARG_UNUSED size_t  length);

/*
An index type representing an index to a partition element for an associated
module and partition kind.
*/
struct an_ifc_partition_kind_index : public an_ifc_module_entity {
  an_ifc_partition_kind_index()
    : an_ifc_module_entity(NULL), partition_kind(ifc_pk_none),
      value(0)
    {}
  an_ifc_partition_kind_index(an_ifc_module_file    *file_val,
                              an_ifc_partition_kind partition_kind_val,
                              an_ifc_index_type     value_val)
    : an_ifc_module_entity(file_val), partition_kind(partition_kind_val),
      value(value_val)
    {}

  inline a_boolean operator==(const an_ifc_partition_kind_index &other) const;
  a_boolean operator!=(const an_ifc_partition_kind_index &other) const
    { return !(*this == other); }

  an_ifc_partition_kind
                partition_kind;
                        /* The associated partition kind value for this
                           index. */
  an_ifc_index_type
                value;  /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type for all partition kinds. */
};  /* an_ifc_partition_kind_index */


a_boolean an_ifc_partition_kind_index::operator==(
                                      const an_ifc_partition_kind_index &other)
                                                                          const
/*
Return TRUE if two an_ifc_partition_kind_index structures refer to the same
partition element in the same file; otherwise, return FALSE.
*/
{
  a_boolean result = TRUE;

  if (this->file != other.file) {
    result = FALSE;
  } else if (this->partition_kind != other.partition_kind) {
    result = FALSE;
  } else if (this->value != other.value) {
    result = FALSE;
  }  /* if */
  return result;
}  /* an_ifc_partition_kind_index::operator== */


inline a_boolean is_null_index(an_ifc_partition_kind_index idx)
/*
Given an IFC partition kind index, return TRUE if the given index is considered
a null index; otherwise, return FALSE.
*/
{
  return idx.partition_kind == ifc_pk_none;
}  /* is_null_index */


inline an_ifc_module *module_of(const an_ifc_module_entity &entity)
/*
Given an IFC module entity, return the corresponding an_ifc_module instance.
*/
{
  an_ifc_module *mod = entity.file->get_read_state().mod;

  /* If this assertion fails, the caller is using an IFC file instance that
     does not have a corresponding module set on it.  This can happen when
     using IFC processing logic with an IFC module file lacking an associated
     IFC module interface. */
  check_assertion_str(mod != NULL, "module requested but not bound");
  return mod;
}  /* module_of */


extern a_const_char *ifc_token_name_of(a_token_kind token_kind);

template<typename an_ifc_Index_type>
extern an_ifc_partition_kind get_partition_kind(an_ifc_Index_type idx);

template<typename an_ifc_Index_type>
extern an_ifc_index_type get_partition_index(an_ifc_Index_type idx);

template<typename an_ifc_Index_type>
extern an_ifc_partition_metadata *get_partition_metadata(
                                                        an_ifc_Index_type idx);

template<typename an_ifc_Index_type>
extern Opt<size_t> get_partition_offset(an_ifc_Index_type idx);

extern a_const_char *get_partition_name_from_kind(
                                              an_ifc_partition_kind part_kind);

template<typename an_ifc_Node_type>
extern an_ifc_Node_type construct_node_from_module(an_ifc_module_file *file);

template<typename an_ifc_Node_type, typename an_ifc_Index_type>
extern void construct_node(Opt<an_ifc_Node_type> *result,
                           an_ifc_Index_type     idx);

template<typename an_ifc_Node_type, typename an_ifc_Index_type>
extern void construct_node_prechecked(an_ifc_Node_type  *result,
                                      an_ifc_Index_type idx);

template<typename an_ifc_Node_type, typename an_ifc_Index_type>
extern void construct_node_unchecked(an_ifc_Node_type  *result,
                                     an_ifc_Index_type idx);

template<typename an_ifc_Index_type>
extern an_ifc_Index_type from_lexical_index(a_lexical_ifc_index_reference idx);

template<typename an_ifc_Index_type>
extern a_lexical_ifc_index_reference to_lexical_index(an_ifc_Index_type idx);

extern a_boolean check_module(const an_ifc_module_reference &ref);

extern an_ifc_module_file* get_module(const an_ifc_module_reference &ref);

extern Opt<a_string> get_name_of_ifc_module(a_const_char *file_name);

extern void process_ifc_declaration(a_module_entity_ptr mep);

extern a_boolean has_variable_initializer_from_ifc_module(a_variable_ptr  vp);

extern a_boolean load_variable_initializer_from_ifc_module(a_variable_ptr  vp);

extern a_boolean has_routine_definition_from_ifc_module(a_routine_ptr  rp);

extern a_boolean load_routine_definition_from_ifc_module(a_routine_ptr  rp);

extern a_boolean has_template_definition_from_ifc_module(a_template_ptr templ);

extern
a_boolean load_template_definition_from_ifc_module(a_template_ptr  templ);

extern a_boolean has_template_specializations_from_ifc_module(
                                                        a_template_ptr  templ);

extern
a_boolean load_template_specializations_from_ifc_module(a_template_ptr  templ);

extern a_boolean has_type_definition_from_ifc_module(a_type_ptr  ty);

extern a_boolean load_type_definition_from_ifc_module(a_type_ptr  ty);

extern a_dynamic_init_ptr load_variable_init_from_ifc_module(
                                                a_type_ptr        tp,
                                                an_ifc_expr_index init_expr);

extern a_boolean extract_tokens_for_ifc_module_expr(
                              a_lexical_ifc_index_reference *index,
                              a_token_sequence_number       *expected_end_tsn);

extern void record_symbol_for_ifc_decl(a_symbol_ptr  sym);

extern a_symbol_ptr load_tok_ifc_entity_ref();

extern a_symbol_ptr load_tok_ifc_decl_ref();

extern void ifc_modules_one_time_init();

extern void ifc_modules_trans_unit_init();

extern void ifc_modules_trans_unit_wrapup();

#if MAKE_FRONT_END_CALLABLE
extern void ifc_modules_cleanup();
#endif /* MAKE_FRONT_END_CALLABLE */

extern void ifc_modules_write_out();

/*
An enum representing the kind of validation trace.
*/
enum an_ifc_validation_trace_kind {
  ifc_vtk_field,
  ifc_vtk_partition
};

/*
A structure used to track the "path" taken during validation of an IFC node.
*/
struct an_ifc_validation_trace {
  an_ifc_validation_trace(a_const_char                  *field_name_val,
                          size_t                        offset_val,
                          const an_ifc_validation_trace *parent_val)
    : trace_kind(ifc_vtk_field), parent(parent_val),
      field_info{field_name_val, offset_val}
    {}

  an_ifc_validation_trace(an_ifc_module_file            *file_val,
                          an_ifc_partition_kind         partition_kind_val,
                          an_ifc_index_type             partition_idx,
                          const an_ifc_validation_trace *parent_val)
    : trace_kind(ifc_vtk_partition), parent(parent_val),
      partition_info{file_val, partition_kind_val, partition_idx}
    {}

  an_ifc_validation_trace_kind
                trace_kind;
                        /* The variant describing this trace. */
  const an_ifc_validation_trace
                *parent;
                        /* The parent of this trace or NULL if none. */

  /*
  The data structure used to represent tracing information when trace_kind ==
  ifc_vtk_field.
  */
  struct field_info_trace_data {
    a_const_char
                *name;  /* The field name associated with this trace. */
    size_t      offset; /* The relative offset of the field from the node
                           start associated with this trace. */
  };  /* field_info_trace_data */

#ifdef UNION_AS_STRUCT
/* FIXME: Workaround for union-as-struct build issue. */
#undef union
#endif /* ifdef UNION_AS_STRUCT */
  union {
    /* When trace_kind == ifc_vtk_field. */
    field_info_trace_data
                field_info;
                        /* The associated information about the traced
                           field. */
    /* When trace_kind == ifc_vtk_partition. */
    an_ifc_partition_kind_index
                partition_info;
                        /* The associated information about the traced
                           partition element. */
#ifdef UNION_AS_STRUCT
#define union struct
#endif /* ifdef UNION_AS_STRUCT */
  };
};  /* an_ifc_validation_trace */

extern void invalid_sort(an_ifc_module_file            *file,
                         const an_ifc_validation_trace *trace);

extern void invalid_partition(an_ifc_module_file            *file,
                              const an_ifc_validation_trace *trace);

extern a_boolean validate_element_exists(
                                  an_ifc_module_file            *file,
                                  an_ifc_partition_kind         partition_kind,
                                  an_ifc_index_type             index,
                                  const an_ifc_validation_trace *trace);

extern void unknown_partition_conversion(
                                      an_ifc_module_file            *file,
                                      const char                    *sort_name,
                                      an_ifc_index_type             index,
                                      const an_ifc_validation_trace *trace);

#if DEBUG
extern a_string s_db_version_of_ifc_module(a_module_ptr mod);

extern a_string s_db_id_of_ifc_mep(a_module_entity_ptr mep);

extern void db_mep_stack();

extern void db_node_at_tsn(a_token_cache_ptr        cache,
                           a_token_sequence_number  tsn);

extern void db_node_at_tsn(a_module_token_cache_ptr cache,
                           a_token_sequence_number  tsn);

extern void db_locus(const an_ifc_source_location &locus);

#endif /* DEBUG */

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
* Copyright 2017-2023 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
