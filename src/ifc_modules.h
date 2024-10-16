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

ifc_modules.h -- Declarations relating to IFC modules made available to the
                 rest of the front end.  For declarations related only to the
                 implementation of IFC modules itself see
                 ifc_modules_internal.h.

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

struct an_ifc_partition_metadata;
struct a_partial_scope_stack_state;
struct a_str_control_block;

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
                *partitions;
                        /* Information about each of the IFC partitions that
                           could exist in a module file. */
  a_module_sequence_number_mapping
                *sequence_numbers = NULL;
                        /* A mapping of sequence numbers for the module,
                           indexed by a NameSort::SourceFile index.
                           Dynamically allocated (in front end memory) once
                           the number of source files is known. */
  an_ifc_module_file
                *file = NULL;
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
  an_ifc_module(a_module_file_kind mk);
  ~an_ifc_module() EDG_NOEXCEPT;

  a_boolean is_open() const;

  a_boolean import(a_module_import_decl_ptr midp);
  void close();
  void pch_reset(a_module_import_decl_ptr midp);
  void complete_definition_of_module_class(a_module_entity_ptr mep);
  a_boolean init_header(a_module_import_decl_ptr midp,
                         a_boolean               issue_diag);
  a_boolean initialize_members_from_ifc_module_file(
                                          a_module_import_decl_ptr midp,
                                          a_boolean                issue_diag);
  a_boolean open_and_map_ifc_module_file(a_module_import_decl_ptr midp,
                                         a_boolean                issue_diag);
  void import_referenced_modules(a_boolean impl_unit_importing_self);
  void export_ifc_macros();
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

extern a_boolean is_at_least(an_ifc_module          *mod,
                             an_ifc_version_storage minimum_version_major,
                             an_ifc_version_storage minimum_version_minor);

extern a_const_char *ifc_token_name_of(a_token_kind token_kind);

template<typename an_ifc_Index_type>
extern an_ifc_Index_type from_lexical_index(a_lexical_ifc_index_reference idx);

template<typename an_ifc_Index_type>
extern a_lexical_ifc_index_reference to_lexical_index(an_ifc_Index_type idx);

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

extern a_module_entity_ptr locate_ifc_module_entity(
                                                   a_module_entry_locator loc);

extern void load_namespace_elements_from_ifc_locator(
                                                   a_module_entity_ptr    mep,
                                                   a_module_entry_locator loc);

extern void update_entity_from_new_ifc_locator(a_module_entity_ptr    mep,
                                               a_module_entry_locator new_loc);

extern Opt<a_source_position> source_position_from_ifc_of(
                                                      a_module_entity_ptr mep);

extern a_dynamic_init_ptr load_variable_init_from_ifc_module(
                                                a_type_ptr        tp,
                                                an_ifc_expr_index init_expr);

extern a_boolean extract_tokens_for_ifc_module_expr(
                              a_lexical_ifc_index_reference *index,
                              a_token_sequence_number       *expected_end_tsn);

extern void record_symbol_for_ifc_decl(a_symbol_ptr  sym);

extern a_symbol_ptr load_tok_ifc_entity_ref();

extern a_symbol_ptr load_tok_ifc_decl_ref();

extern void scan_ifc_param_ref_expr(an_operand *result);

extern void ifc_modules_one_time_init();

extern void require_ifc_modules();

extern void ifc_modules_trans_unit_init();

extern void ifc_modules_trans_unit_wrapup();

#if MAKE_FRONT_END_CALLABLE
extern void ifc_modules_cleanup();
#endif /* MAKE_FRONT_END_CALLABLE */

extern void ifc_modules_write_out();

#if DEBUG

extern a_string s_db_version_of_ifc_module(a_module_ptr mod);

extern a_string s_db_ifc_locator(a_module_entry_locator loc);

extern a_string s_db_id_of_ifc_mep(a_module_entity_ptr mep);

extern a_string s_db_lexical_ifc_index(a_lexical_ifc_index_reference idx);

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
