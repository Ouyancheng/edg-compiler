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

ifc_modules.h -- Declarations relating to ifc_modules.c (having to do with
                 Microsoft IFC modules).

*/

/* Avoid including these declarations more than once: */
#ifndef IFC_MODULES_H
#define IFC_MODULES_H 1

#if !STANDALONE_UTILITY_PROGRAM

#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ifc_map.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#include "util.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

typedef struct a_tmpl_decl_state *a_tmpl_decl_state_ptr;

/* FIXME: Temporarily disable "not referenced" warnings until completed. */
/*lint -save -e755 -e758 -e768 -e769*/

/*lint -e1751*/
namespace {
/*
Magic numbers that identify the beginning of an IFC file.  Declared outside of
MICROSOFT_EXTENSIONS_ALLOWED to facilitate identifying the kind of a mismatched
module file.
*/
constexpr a_byte ifc_magic_numbers[] = { 0x54, 0x51, 0x45, 0x1A };
}  /* namespace */

#if MICROSOFT_EXTENSIONS_ALLOWED

typedef uint64_t a_module_ref_key;

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


struct a_str_control_block;
struct a_partial_scope_stack_state;

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
    uint32_t	max_line_number;
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
			   could exist in a module file.  Note that this must
			   allow for indexing via partition enumerators, which
			   may have gaps, and so ifc_last must be used here
			   instead of num_ifc_partitions. */
  a_module_sequence_number_mapping
		*sequence_numbers = NULL;
			/* A mapping of sequence numbers for the module,
			   indexed by a NameSort::SourceFile index.
			   Dynamically allocated (in front end memory) once
			   the number of source files is known. */
  an_ifc_version_storage
		version_major = 0;
			/* The module's major version.  Defaulted to the
			   lowest supported version until initialization is
			   complete. */
  an_ifc_version_storage
		version_minor = 33;
			/* The module's minor version.  Defaulted to the
			   lowest supported version until initialization is
			   complete. */

#if USE_MMAP_FOR_MEMORY_REGIONS
  unsigned char
		*byte_buffer = NULL;
			/* Pointer to the current position in the buffer
			   used by get_byte, etc. */
  unsigned char
		*buffer_end = NULL;
			/* Pointer to the last byte of the buffer used by
			   get_byte, etc. */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  a_const_char	*string_table = NULL;
			/* The string table of the IFC file. */
  a_tmpl_decl_state_ptr
		curr_templ_decl_state = NULL;
			/* The current template declaration state, NULL if
			   there is no template declaration being processed. */
  Ptr_map<a_module_ref_key, a_module_import_decl_ptr>
		referenced_modules;
			/* A map from a module reference to the corresponding
			   import decl. */
  an_error_severity
		unhandled_node_diag_sev = es_none;
			/* The highest severity with which an unhandled node
			   diagnostic has already been issued for this
			   module. */
  a_boolean
		suppress_default_arguments = FALSE;
			/* Flag to indicate whether default arguments should be
			   included when processing an entity in this module.
			*/
  a_boolean
		suppress_automatic_name_qualification = FALSE;
			/* Flag to indicate ExprSort_NameDecl should not be
			   interpreted as concrete declarations that should be
			   qualified. */
  a_boolean
		suppress_automatic_namespace_qualification = FALSE;
			/* Flag to indicate the namespace portion of the
			   current nested name specifier has already been

			   cached and must not be recached. */
  /* FIXME: This is module specific, should it not be, or should we use a
    structure that's module specific.  It seems very redundant to have a module
    pointer for every entry in this map. */
  Ptr_map<an_ifc_decl_index, a_symbol_ptr>
		decl_map;
public:
  an_ifc_module() : a_module_interface((a_module_kind)mk_ifc),
                    referenced_modules(/*mask_width=*/4),
                    decl_map(/*mask_width=*/8)
    {}
  VIRTUAL ~an_ifc_module() EDG_NOEXCEPT = default;

  a_boolean matches_module(a_const_char *module_name,
                           a_const_char *module_file);

  inline a_boolean is_open() const OVERRIDE {
    return f_module != NULL;
  }

  a_boolean import(a_module_import_decl_ptr midp) OVERRIDE;
  void close() OVERRIDE;
  void pch_reset(a_module_import_decl_ptr midp) OVERRIDE;

  inline a_module_entity_ptr get_ifc_module_entity_ptr(
                                                      an_ifc_type_index index);
  inline a_module_entity_ptr get_ifc_module_entity_ptr(
                                                      an_ifc_decl_index index);
  void process_ifc_declaration(a_module_entity_ptr mep,
                               a_boolean           defer,
                               a_type_ptr          enumeration_type);
  void complete_definition_of_module_class(a_module_entity_ptr mep) OVERRIDE;
  a_boolean cache_function_body(a_token_cache_ptr  cache,
                                an_ifc_decl_index  decl_idx,
                                a_routine_ptr      rp,
                                a_func_info_block  *func_info);
  static a_boolean process_template_definition(
                                  a_module_entity_ptr        mep,
                                  const an_ifc_decl_template &idst,
                                  a_boolean                  already_declared,
                                  a_boolean                  is_func_template);

#if DEBUG
  void debug() const OVERRIDE;
  void db_module_entity(a_module_entity_ptr mep) const OVERRIDE;
#endif /* DEBUG */
  enum a_non_type_kind : uint8_t {
    ntk_none,
    ntk_ellipsis,
    ntk_namespace,
    ntk_empty_pack_expansion,
  };
  inline void issue_unsupported_node_diag(a_const_char      *node,
                                          a_source_position *pos);
  inline void issue_unsupported_node_error(a_const_char      *node,
                                           a_source_position *pos);
  a_boolean init_string_table_and_header(a_module_import_decl_ptr midp,
                                         a_boolean                issue_diag);
  a_boolean initialize_members_from_ifc_module_file(
                                          a_module_import_decl_ptr midp,
                                          a_boolean                issue_diag);
  a_boolean open_and_map_ifc_module_file(a_module_import_decl_ptr midp,
                                         a_boolean                issue_diag);
  void import_referenced_modules();
  void define_ifc_macro(an_ifc_macro_index macro);
  void export_ifc_macros();
  template<typename a_Scope_Member_Consumer>
  inline void traverse_scope_member_sequence(const an_ifc_sequence   &seq,
                                             a_Scope_Member_Consumer consumer);
  void process_scope_member_sequence(const an_ifc_sequence &seq);
  void process_template_specializations(an_ifc_decl_index decl_idx) const;
  void process_ifc_scope(an_ifc_scope_index scope_index,
                         a_scope_ptr        scope);
  uint32_t get_num_entries(an_ifc_partition_kind partition) const;
  /* Module entity getters. */
  a_module_entity_ptr get_ifc_module_entity_ptr(
                                               an_ifc_partition_kind partition,
                                               an_ifc_index_type     index);
  a_module_entity_ptr get_ifc_decl_from_other_module(
                                             const an_ifc_decl_reference &ref);
  a_module_entity_ptr get_ifc_decl_from_other_module(an_ifc_decl_index index);
  void process_ifc_decl_from_other_module(a_module_entity_ptr dmep);
  a_module_entity_ptr get_and_process_ifc_decl_from_other_module(
                                             const an_ifc_decl_reference &ref);
  a_module_entity_ptr get_and_process_ifc_decl_from_other_module(
                                                      an_ifc_decl_index index);
  /* IFC Scope readers. */
  a_boolean is_home_scope_readable(an_ifc_decl_index decl_index);
  a_boolean is_name_qualifiable(an_ifc_decl_index decl_index);
  a_type_ptr type_for_type_index(an_ifc_type_index type_index,
                                 a_non_type_kind   *kind);
  a_type_ptr type_for_template_id(const an_ifc_expr_template_id &templ_id);
  a_template_arg_ptr template_arg_for_expr(
                                         a_template_parameter_ptr tmpl_param,
                                         an_ifc_expr_index        expr_index);
  a_boolean source_position_from_locus(a_source_position            *pos,
                                       const an_ifc_source_location &locus);
  inline a_const_char *get_string_at_offset(an_ifc_text_offset offset) const;
  a_const_char *string_from_name_index(an_ifc_name_index name_index,
                                       a_symbol_locator  *loc);
  a_const_char *string_from_name_index(an_ifc_name_index name_index,
                                       a_symbol_locator  *loc,
                                       a_text_buffer_ptr *result_buffer);
  a_const_char *string_from_name_index(an_ifc_text_offset text_offset,
                                       a_symbol_locator  *loc);
  a_const_char *name_from_local_decl(an_ifc_decl_index decl);
  void init_dps(a_decl_parse_state               *dps,
                const an_ifc_source_location     &locus,
                an_ifc_type_index                type_index,
                an_ifc_object_traits_bitfield    traits,
                an_ifc_msvc_traits_bitfield      msvc_traits,
                an_ifc_basic_specifiers_bitfield specifiers,
                an_ifc_access_sort               access,
                an_ifc_expr_index                alignment,
                a_partial_scope_stack_state      *psssp);
  template<typename an_Index_type>
  a_boolean init_locator_from_name(an_Index_type                ref,
                                   const an_ifc_source_location &locus,
                                   a_symbol_locator             *loc);
  template<typename an_Index_type>
  inline a_boolean init_decl_locator(an_Index_type                ref,
                                     const an_ifc_source_location &locus,
                                     a_symbol_locator             *loc);
  template<typename an_ifc_Decl_type>
  inline a_boolean init_decl_locator(const an_ifc_Decl_type &decl,
                                     a_symbol_locator       *loc);
  template<typename an_ifc_Decl_type>
  inline a_boolean lazy_init_module_scope(const an_ifc_Decl_type &decl,
                                          a_module_entity_ptr    mep);
  template<typename an_ifc_Decl_type>
  inline void lazy_push_module_scope(
                                  const an_ifc_Decl_type   &decl,
                                  a_module_entity_ptr      mep,
                                  a_module_scope_push_kind *scope_push_status);
  void unsigned_integer_for_expr_index(an_ifc_expr_index expr_index,
                                       an_integer_value  *value);
  a_constant_ptr constant_for_expr_index(an_ifc_expr_index expr_index,
                                         a_type_ptr        default_type);
  a_constant_ptr constant_for_named_decl(const an_ifc_expr_named_decl &iesndp);
  /* Token caching. */
  void cache_scope_member_sequence(a_token_cache_ptr     cache,
                                   an_ifc_decl_index     scope_decl,
                                   const an_ifc_sequence &seq);
  template<typename a_Name_Cache_Fn, typename a_Scope_Cache_Fn>
  inline void cache_scope_decl(a_token_cache_ptr            cache,
                               an_ifc_decl_index            decl_idx,
                               an_ifc_type_index            type,
                               a_Name_Cache_Fn              cache_name_fn,
                               a_Scope_Cache_Fn             cache_scope_fn,
                               const an_ifc_source_location &locus);
  void cache_scope_decl(a_token_cache_ptr            cache,
                        an_ifc_decl_index            decl_idx,
                        an_ifc_type_index            type,
                        an_ifc_name_index            name,
                        an_ifc_type_index            base,
                        an_ifc_scope_index           scope,
                        const an_ifc_source_location &locus);
  void cache_type_first_pass(a_token_cache_ptr            cache,
                             an_ifc_type_index            type,
                             const an_ifc_source_location &locus);
  void cache_type_second_pass(a_token_cache_ptr            cache,
                              an_ifc_type_index            type,
                              const an_ifc_source_location &locus);
  void cache_type(a_token_cache_ptr            cache,
                  an_ifc_type_index            type,
                  const an_ifc_source_location &locus);
  void cache_type_param_introducer(a_token_cache_ptr  cache,
                                   an_ifc_expr_index  constraint,
                                   a_boolean          is_pack,
                                   a_source_position  *pos);
  void cache_attr(a_token_cache_ptr  cache,
                  an_ifc_attr_index  attr,
                  a_boolean          cache_brackets);
  void cache_attrs(a_token_cache_ptr cache,
                   an_ifc_decl_index decl_idx);
  void cache_template_head(a_token_cache_ptr     cache,
                           an_ifc_chart_index    chart_idx,
                           a_source_position_ptr pos);
  void cache_decl(a_token_cache_ptr cache,
                  an_ifc_decl_index decl);
  inline void update_name_qualification_suppression(
                                                const an_ifc_expr_path &iespp);
  enum a_cache_expr_option {
    ceo_none                    = 0x0,
    ceo_qualified_name          = 0x1 << 0,
    ceo_skip_assign             = 0x1 << 1,
    ceo_possible_temporary_decl = 0x1 << 2,
  };
  void cache_expr(a_token_cache_ptr    cache,
                  an_ifc_expr_index    expr,
                  a_cache_expr_option  options = ceo_none);
  enum a_cache_statement_option {
    cso_none = 0x0,
    cso_func_body = 0x1,
    cso_no_final_semicolon = 0x2
  };
  void cache_statement(a_token_cache_ptr         cache,
                       an_ifc_stmt_index         stmt_idx,
                       a_cache_statement_option  options = cso_none);
  void cache_syntax(a_token_cache_ptr   cache,
                    an_ifc_syntax_index syntax);
  void cache_chart(a_token_cache_ptr     cache,
                   an_ifc_chart_index    chart,
                   a_source_position_ptr pos);
  void cache_chart(a_token_cache_ptr            cache,
                   an_ifc_chart_index           chart,
                   const an_ifc_source_location &locus);
  void cache_operator(a_token_cache_ptr            cache,
                      an_ifc_operator_category     op,
                      const an_ifc_source_location &locus);
  void cache_operator(a_token_cache_ptr            cache,
                      an_ifc_niladic_operator_sort op,
                      const an_ifc_source_location &locus);
  void cache_operator(a_token_cache_ptr            cache,
                      an_ifc_monadic_operator_sort op,
                      const an_ifc_source_location &locus);
  void cache_operator(a_token_cache_ptr            cache,
                      an_ifc_dyadic_operator_sort  op,
                      const an_ifc_source_location &locus);
  void cache_operator(a_token_cache_ptr            cache,
                      an_ifc_triadic_operator_sort op,
                      const an_ifc_source_location &locus);
  void cache_operator(a_token_cache_ptr                        cache,
                      an_ifc_storage_instruction_operator_sort op,
                      const an_ifc_source_location             &locus);
  void cache_operator(a_token_cache_ptr             cache,
                      an_ifc_variadic_operator_sort op,
                      const an_ifc_source_location &locus);
  void cache_exception_spec(a_token_cache_ptr                   cache,
                            const an_ifc_noexcept_specification &eh_spec,
                            a_source_position_ptr               pos);
  uint32_t cache_sentence(a_token_cache_ptr     cache,
                          an_ifc_sentence_index sentence,
                          uint32_t              offset = 0,
                          a_boolean             look_for_stop_token = FALSE);
  a_boolean sentence_is_deleted(an_ifc_sentence_index sentence);
  void cache_word(a_token_cache_ptr cache, const an_ifc_source_word &word);
  void cache_word(a_token_cache_ptr cache, const an_ifc_nestable_word &word);
  void cache_source_directive(a_token_cache_ptr            cache,
                              an_ifc_source_directive_sort directive,
                              const an_ifc_source_location &locus);
  void cache_source_punctuator(a_token_cache_ptr             cache,
                               an_ifc_source_punctuator_sort punctuator,
                               const an_ifc_source_location  &locus);
  void cache_source_literal(a_token_cache_ptr                    cache,
                            const an_ifc_source_literal_category &literal,
                            const an_ifc_source_location         &locus);
  void cache_source_operator(a_token_cache_ptr            cache,
                             an_ifc_source_operator_sort  op,
                             const an_ifc_source_location &locus);
  void cache_source_keyword(a_token_cache_ptr            cache,
                            an_ifc_source_keyword_sort   keyword,
                            const an_ifc_source_location &locus);
  void cache_source_identifier(a_token_cache_ptr                       cache,
                               const an_ifc_source_identifier_category &id,
                               const an_ifc_source_location            &locus);
  void cache_string(a_token_cache_ptr            cache,
                    an_ifc_string_index          string,
                    const an_ifc_source_location &locus);
  uint32_t try_cache_class_attributes_from_body(
                                          a_token_cache_ptr     cache,
                                          an_ifc_sentence_index body_sentence);
  uint32_t cache_decl_template_declaration(
                                     a_token_cache_ptr          cache,
                                     const an_ifc_decl_template &decl,
                                     a_boolean                  add_semicolon);
  void cache_decl_template(a_token_cache_ptr          cache,
                           const an_ifc_decl_template &decl);
  void cache_decl_partial_specialization_declaration(
                             a_token_cache_ptr                        cache,
                             an_ifc_decl_index                        decl_idx,
                             const an_ifc_decl_partial_specialization &decl);
  void cache_decl_partial_specialization(
                             a_token_cache_ptr                        cache,
                             an_ifc_decl_index                        decl_idx,
                             const an_ifc_decl_partial_specialization &decl);
  void cache_decl_specialization(a_token_cache_ptr                cache,
                                 an_ifc_decl_index                decl_idx,
                                 const an_ifc_decl_specialization &decl);
  template<typename a_Name_Cache_Fn, typename an_Init_Cache_Fn>
  inline void cache_variable_decl(
                            a_token_cache_ptr                cache,
                            an_ifc_decl_index                decl_idx,
                            a_boolean                        is_class_member,
                            an_ifc_access_sort               access,
                            a_boolean                        cache_access_spec,
                            an_ifc_basic_specifiers_bitfield specifiers,
                            an_ifc_object_traits_bitfield    traits,
                            an_ifc_expr_index                alignment,
                            an_ifc_type_index                type,
                            a_Name_Cache_Fn                  cache_name_fn,
                            an_ifc_expr_index                width,
                            an_Init_Cache_Fn                 cache_init_fn,
                            const an_ifc_source_location     &locus);
  void cache_variable_decl(a_token_cache_ptr                cache,
                           an_ifc_decl_index                decl_idx,
                           a_boolean                        is_class_member,
                           an_ifc_access_sort               access,
                           an_ifc_basic_specifiers_bitfield specifiers,
                           an_ifc_object_traits_bitfield    traits,
                           an_ifc_expr_index                alignment,
                           an_ifc_type_index                type,
                           an_ifc_name_index                name,
                           an_ifc_text_offset               raw_name,
                           an_ifc_expr_index                width,
                           an_ifc_expr_index                initializer,
                           const an_ifc_source_location     &locus);
  template<typename a_Name_Cache_Fn>
  inline void cache_function_decl(
                        a_token_cache_ptr                    cache,
                        an_ifc_decl_index                    decl_idx,
                        a_boolean                            is_class_member,
                        a_boolean                            is_dtor,
                        an_ifc_access_sort                   access,
                        a_boolean                            cache_access_spec,
                        an_ifc_calling_convention_sort       calling_conv,
                        an_ifc_function_traits_bitfield      func_traits,
                        an_ifc_function_type_traits_bitfield func_type_traits,
                        an_ifc_msvc_traits_bitfield          vendor_traits,
                        an_ifc_type_index                    return_type,
                        a_Name_Cache_Fn                      cache_name_fn,
                        an_ifc_chart_index                   params,
                        an_ifc_type_index                    param_types,
                        const an_ifc_noexcept_specification  &eh_spec,
                        const an_ifc_source_location         &locus);
  void cache_function_decl(
                         a_token_cache_ptr                    cache,
                         an_ifc_decl_index                    decl_idx,
                         a_boolean                            is_class_member,
                         a_boolean                            is_dtor,
                         an_ifc_access_sort                   access,
                         an_ifc_calling_convention_sort       calling_conv,
                         an_ifc_function_traits_bitfield      func_traits,
                         an_ifc_function_type_traits_bitfield func_type_traits,
                         an_ifc_msvc_traits_bitfield          vendor_traits,
                         an_ifc_type_index                    return_type,
                         an_ifc_name_index                    name,
                         an_ifc_chart_index                   params,
                         an_ifc_type_index                    param_types,
                         const an_ifc_noexcept_specification  &eh_spec,
                         const an_ifc_source_location         &locus);
  void cache_name(a_token_cache_ptr            cache,
                  an_ifc_name_index            name,
                  const an_ifc_source_location &locus);
  inline a_boolean should_cache_nested_name_specifier_for_scope(
                                                            a_scope_ptr scope);
  void cache_scope_as_nested_name_specifier(a_token_cache_ptr     cache,
                                            a_scope_ptr           scope,
                                            a_source_position_ptr pos);
  void cache_nested_name_specifier_from_decl(a_token_cache_ptr     cache,
                                             an_ifc_decl_index     decl,
                                             a_source_position_ptr pos);
  void cache_qualified_name_from_decl(a_token_cache_ptr            cache,
                                      an_ifc_decl_index            decl,
                                      const an_ifc_source_location &locus);
  void cache_name_from_decl(a_token_cache_ptr            cache,
                            an_ifc_decl_index            decl,
                            const an_ifc_source_location &locus);
  void cache_macro(a_token_cache_ptr  cache,
                   an_ifc_macro_index macro);
  void cache_form(a_token_cache_ptr cache,
                  an_ifc_form_index form,
                  a_boolean         is_parameter_form = FALSE);
  /* Index helpers. */
  inline an_ifc_decl_index decl_index_of(an_ifc_partition_kind partition,
                                         size_t                file_offset);
  inline an_ifc_decl_index decl_index_of(a_module_entity_ptr mep);
  inline an_ifc_type_index type_index_of(an_ifc_partition_kind partition,
                                         size_t                file_offset);
  inline an_ifc_type_index type_index_of(a_module_entity_ptr mep);
  inline an_ifc_attr_index attr_index_of(an_ifc_decl_index decl_idx);
  an_ifc_chart_index get_func_params_from_trait(an_ifc_decl_index decl);
  an_ifc_msvc_traits_bitfield get_vendor_traits(an_ifc_decl_index decl);
  Opt<an_ifc_sequence> get_specialization_sequence_from_trait(
                                                       an_ifc_decl_index decl);
  a_template_ptr parse_cached_explicit_specialization(
                                   a_token_cache_ptr                cache,
                                   a_scope_ptr                      encl_scope,
                                   const an_ifc_decl_specialization &decl);
  char *parse_cached_explicit_instantiation(
                                       a_token_cache_ptr                cache,
                                       const an_ifc_decl_specialization &decl,
                                       an_il_entry_kind                 *kind);
#if CHECKING
  void validate_is_class_type(an_ifc_type_index type);
#endif /* CHECKING */
#if DEBUG
  void db_ifc_file_header() const;
  void db_ifc_scope(an_ifc_scope_index scope);
  void db_ifc_declaration(an_ifc_decl_index decl);
  void db_locus(const an_ifc_source_location &locus);
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


extern void get_bytes(an_ifc_module *mod,
                      void          *entity,
                      size_t        length,
                      a_boolean     header_bytes);


inline a_boolean get_fallback_presence_value(an_ifc_module *mod)
/*
Given an IFC module instance, if the fallback assumption for versions below the
supported IFC version range for has_ifc_X checks should be TRUE, return TRUE,
otherwise return FALSE.
*/
{
  return skip_module_version_check;
}  /* get_fallback_presence_value */


extern a_boolean is_at_least(an_ifc_module          *mod,
                             an_ifc_version_storage minimum_version_major,
                             an_ifc_version_storage minimum_version_minor);

extern a_boolean has_matching_endianness(an_ifc_module *mod);


extern void init_byte_buffer(an_ifc_module     *mod,
                             size_t            offset,
                             ARG_UNUSED size_t length);

/*
An index type representing an index to a partition element for an associated
module and partition kind.
*/
struct an_ifc_partition_kind_index {
  an_ifc_module
                *mod;
                        /* The associated module. */
  an_ifc_partition_kind
                partition_kind;
                        /* The associated DeclSort value for this index. */
  an_ifc_index_type
                value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type for all partition kinds. */
};  /* an_ifc_partition_kind_index */


template<typename an_ifc_Index_type>
extern an_ifc_partition_kind get_partition_kind(an_ifc_Index_type idx);

template<typename an_ifc_Index_type>
extern an_ifc_partition_metadata *get_partition_metadata(
                                                        an_ifc_Index_type idx);

template<typename an_ifc_Index_type>
extern size_t get_partition_offset(an_ifc_Index_type idx);

extern a_const_char *get_partition_name_from_kind(
                                              an_ifc_partition_kind part_kind);

template<typename an_ifc_Storage_type>
extern an_ifc_Byte_buffer<an_ifc_Storage_type> construct_node_from_module(
                                                           an_ifc_module *mod);

template<typename an_ifc_Storage_type, typename an_ifc_Index_type>
extern void construct_node(
                          Opt<an_ifc_Byte_buffer<an_ifc_Storage_type>> *result,
                          an_ifc_Index_type                            idx);

template<typename an_ifc_Storage_type, typename an_ifc_Index_type>
extern void construct_node_prechecked(
                               an_ifc_Byte_buffer<an_ifc_Storage_type> *result,
                               an_ifc_Index_type                       idx);

template<typename an_ifc_Storage_type, typename an_ifc_Index_type>
extern void construct_node_unchecked(
                               an_ifc_Byte_buffer<an_ifc_Storage_type> *result,
                               an_ifc_Index_type                       idx);

extern a_boolean check_module(const an_ifc_module_reference &ref);

extern an_ifc_module* get_module(const an_ifc_module_reference &ref);

extern a_boolean load_routine_definition_from_ifc_module(a_routine_ptr  rp);

extern
a_boolean load_template_definition_from_ifc_module(a_template_ptr  templ);

extern a_dynamic_init_ptr load_variable_init_from_ifc_module(
                                                a_type_ptr        tp,
                                                an_ifc_expr_index init_expr);

extern void record_pending_ifc_function_body(a_routine_ptr     rp,
                                             an_ifc_decl_index decl_idx);

extern void ifc_modules_one_time_init();

extern void ifc_modules_init();

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

  an_ifc_validation_trace(an_ifc_module                 *mod_val,
                          an_ifc_partition_kind         partition_kind_val,
                          an_ifc_index_type             partition_idx,
                          const an_ifc_validation_trace *parent_val)
    : trace_kind(ifc_vtk_partition), parent(parent_val),
      partition_info{mod_val, partition_kind_val, partition_idx}
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

  /*
  The data structure used to represent tracing information when trace_kind ==
  ifc_vtk_field.
  */
  struct partition_info_trace_data {
    an_ifc_module
                *mod;   /* The module associated with this trace. */
    an_ifc_partition_kind
                kind;   /* The kind of partition associated with this trace. */
    an_ifc_index_type
                idx;    /* The index into the partition associated with this
                           trace. */
  };  /* partition_info_trace_data */

  union {
    /* When trace_kind == ifc_vtk_field. */
    field_info_trace_data
                field_info;
                        /* The associated information about the traced
                           field. */
    /* When trace_kind == ifc_vtk_partition. */
    partition_info_trace_data
                partition_info;
                        /* The associated information about the traced
                           partition element. */
  };
};  /* an_ifc_validation_trace */

extern void invalid_partition(an_ifc_module                 *mod,
                              const an_ifc_validation_trace *trace);

extern a_boolean validate_element_exists(
                                  an_ifc_module                 *mod,
                                  an_ifc_partition_kind         partition_kind,
                                  an_ifc_index_type             index,
                                  const an_ifc_validation_trace *trace);


#if DEBUG
extern void db_mep(a_module_entity_ptr mep);
#endif /* DEBUG */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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
* Copyright 1988-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
