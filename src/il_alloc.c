/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il_alloc.c -- Allocation of intermediate language entries.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#if !STANDALONE_UTILITY_PROGRAM
#include "pch.h"
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if DEBUG
#if !STANDALONE_UTILITY_PROGRAM
/*
Counts of tables allocated, to track total use of memory.
*/
static unsigned long
		num_source_files_allocated,
		num_constants_allocated,
		num_param_types_allocated,
		num_routine_type_supplements_allocated,
		num_based_type_list_members_allocated,
		num_class_type_supplements_allocated,
		num_class_list_entries_allocated,
		num_routine_list_entries_allocated,
		num_overriding_virtual_functions_allocated,
		num_derivation_steps_allocated,
		num_base_class_derivations_allocated,
		num_base_classes_allocated,
		num_template_args_allocated,
		num_template_param_type_supplements_allocated,
		num_typeref_type_supplements_allocated,
		num_integer_type_supplements_allocated,
		num_types_allocated,
		num_dynamic_inits_allocated,
		num_local_static_variable_inits_allocated,
		num_vla_dimensions_allocated,
		num_variables_allocated,
		num_fields_allocated,
		num_routines_allocated,
		num_exception_specifications_allocated,
		num_exception_specification_types_allocated,
		num_asm_entries_allocated,
		num_labels_allocated,
		num_expr_nodes_allocated,
		num_new_delete_supplements_allocated,
#if MICROSOFT_EXTENSIONS_ALLOWED
		num_gcnew_supplements_allocated,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
		num_throw_supplements_allocated,
		num_condition_supplements_allocated,
#if !ABI_CHANGES_FOR_RTTI
		num_accessible_base_classes_allocated,
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
		num_eh_prologue_supplements_allocated,
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
		num_switch_case_entries_allocated,
                num_switch_stmt_descriptions_allocated,
		num_handlers_allocated,
		num_try_supplements_allocated,
#if MICROSOFT_EXTENSIONS_ALLOWED
		num_microsoft_try_supplements_allocated,
		num_ms_attributes_allocated,
		num_ms_attribute_args_allocated,
		num_property_index_types_allocated,
		num_property_or_event_descriptions_allocated,
		num_generic_constraints_allocated,
		num_generic_constraint_clauses_allocated,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
		num_ms_if_exists_allocated,
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
		num_blocks_allocated,
		num_for_loops_allocated,
		num_statements_allocated,
		num_constructor_inits_allocated,
		num_pragmas_allocated,
		num_object_lifetimes_allocated,
		num_namespaces_allocated,
		num_using_decls_allocated,
		num_scopes_allocated,
		num_local_scope_refs_allocated,
		num_il_entry_prefixes_allocated,
		string_literal_text_space_allocated,
		num_seq_number_lookup_entries_allocated,
                num_trans_unit_copy_address_pointers_allocated,
                num_il_entity_list_entries_allocated,
                num_attributes_allocated,
                num_attribute_args_allocated,
                num_attribute_groups_allocated;
#if GENERATE_SOURCE_SEQUENCE_LISTS
static unsigned long
		num_source_sequence_entries_allocated,
		num_src_seq_secondary_decls_allocated,
		num_src_seq_end_of_constructs_allocated,
		num_src_seq_sublists_allocated,
		num_instantiation_directives_allocated,
		num_static_assertions;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ORPHAN_PROCESSING_NEEDED
static unsigned long
		num_fs_orphan_pointers_allocated;
#endif /* ORPHAN_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
static unsigned long
		num_scope_orphaned_list_headers_allocated;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_HIDDEN_NAMES_IN_IL
static unsigned long
		num_hidden_names_allocated;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
static unsigned long
		num_template_decls_allocated;
static unsigned long
		num_template_parameters_allocated;
static unsigned long
		num_templates_allocated;
static unsigned long
		num_name_references_allocated,
                num_name_qualifiers_allocated;
#if RECORD_MACROS_IN_IL
static unsigned long
		num_macros_allocated;
#endif /* RECORD_MACROS_IN_IL */
#if RECORD_MACRO_INVOCATIONS
static unsigned long
		num_macro_invocation_record_blocks_allocated;
#endif /* RECORD_MACRO_INVOCATIONS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
static unsigned long
		num_decl_position_supplements_allocated;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
static unsigned long
		num_per_instantiation_needed_flags_entries_allocated;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if ASM_SUPPORT_NEEDED
static unsigned long
		asm_function_body_space_allocated;
#endif /* ASM_SUPPORT_NEEDED */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* DEBUG */

/* Static variable and macro for quickly initializing the source_corresp
   field of an IL entry to default values. */
static a_source_correspondence
		def_source_corresp;
#define set_default_source_corresp(sc) (sc) = def_source_corresp;

static int	file_scope_entry_prefix_size;
			/* The size of the entry prefix for IL entries
			   allocated in the file scope of the current
			   translation unit. */

static int	file_scope_entry_prefix_alignment_offset;
			/* The offset from the beginning of the space allocated
			   for an IL entry to where the prefix actually
			   begins for file scope allocations.  This is a
			   translation unit variable. */

static int	non_file_scope_entry_prefix_size;
			/* The size of the entry prefix for IL entries
			   not allocated in the file scope.  This is not a
			   translation unit variable. */

static int	non_file_scope_entry_prefix_alignment_offset;
			/* The offset from the beginning of the space allocated
			   for an IL entry to where the prefix actually
			   begins for non-file-scope allocations.  This is
			   not a translation unit variable. */

/*
Macro to increment the entry prefix allocation count only if DEBUG
is TRUE.  Used in do_alloc.
*/
#if DEBUG && !STANDALONE_UTILITY_PROGRAM
#define incr_num_il_entry_prefixes_allocated()                        \
  num_il_entry_prefixes_allocated++
#else /* !(DEBUG && ...) */
#define incr_num_il_entry_prefixes_allocated() /* Nothing */
#endif /* DEBUG && ... */


/*
Allocate an IL entry of size "size" preceded by an_il_entry_prefix, and
initialize the latter to default values.  ptr is a "char *" pointer and is
set to point to the entry proper.  The allocation is done in the memory
region region_number.  file_scope is TRUE if the allocation is in the
file scope.  (Yes, that could be determined from region_number, but it
happens that it is usually known by the caller).
*/
#define do_alloc(ptr, region_number, file_scope, size)                \
{ ptr = alloc_in_region((region_number),                              \
                         (sizeof_t)((size)+non_file_scope_entry_prefix_size));\
  /* There may be padding before the prefix if needed for alignment. */	\
  ptr += non_file_scope_entry_prefix_alignment_offset;			\
  incr_num_il_entry_prefixes_allocated();                             \
  clear_il_entry_prefix(ptr, file_scope, !is_primary_translation_unit); \
  ptr += SPACE_FOR_IL_ENTRY_PREFIX;                                   \
}  /* do_alloc */


/*
Macro to increment the count of next-orphan pointers allocated only if
DEBUG is TRUE.  Used in do_fs_alloc.
*/
#if ORPHAN_PROCESSING_NEEDED && DEBUG && !STANDALONE_UTILITY_PROGRAM
#define incr_num_fs_orphan_pointers_allocated()                       \
  num_fs_orphan_pointers_allocated++
#else /* !(ORPHAN_PROCESSING_NEEDED && ...) */
#define incr_num_fs_orphan_pointers_allocated() /* Nothing */
#endif /* ORPHAN_PROCESSING_NEEDED && ... */

/*
Macro to increment the count of translation unit copy address pointers
allocated.  When not generating debugging code, this expands to nothing.
*/
#if DEBUG && !STANDALONE_UTILITY_PROGRAM
#define incr_num_trans_unit_copy_address_pointers_allocated()              \
  num_trans_unit_copy_address_pointers_allocated++
#else /* !(DEBUG && !STANDALONE_UTILITY_PROGRAM) */
#define incr_num_trans_unit_copy_address_pointers_allocated() /* Nothing */
#endif /* DEBUG && !STANDALONE_UTILITY_PROGRAM */

/*
Macro that clears the orphan pointer, increments the count of orphan
pointers allocated, and updates the pointer provided to point past
the orphan pointer.  When orphan pointers are not used, this macro
expands to nothing.
*/
#if ORPHAN_PROCESSING_NEEDED
#define clear_and_incr_past_orphan_pointer(ptr)				\
  incr_num_fs_orphan_pointers_allocated();                            \
  *(char **)ptr = NULL;                                               \
  ptr += SPACE_FOR_FS_ORPHAN_POINTER;
#else /* !ORPHAN_PROCESSING_NEEDED */
#define clear_and_incr_past_orphan_pointer(ptr) /* nothing */
#endif /* ORPHAN_PROCESSING_NEEDED */

/*
Macro that clears the translation unit copy address pointer when compiling
multiple translation units.
*/
#define clear_and_incr_past_trans_unit_copy_address_pointer(ptr)	      \
  incr_num_trans_unit_copy_address_pointers_allocated();                      \
  *(char **)ptr = NULL;                                               \
  ptr += SPACE_FOR_TRANS_UNIT_COPY_ADDRESS_POINTER;

/*
Allocate a file-scope IL entry of size "size" preceded by an_il_entry_prefix
and (if appropriate) an orphan list pointer, and initialize the prefix
and orphan pointer to default values.  ptr is a "char *" pointer and is set
to point to the entry proper.  fs_region_number indicates the file scope
region number to be used (there can be several, when secondary translation
units are involved).
*/
#define do_fs_alloc(ptr, size, fs_region_number)                      \
{ ptr = alloc_in_region(					      \
          fs_region_number,                                           \
          (sizeof_t)((size) + file_scope_entry_prefix_size));         \
  /* There may be padding before the prefix if needed for alignment. */	\
  ptr += file_scope_entry_prefix_alignment_offset;			\
  if (!is_primary_translation_unit) {				      \
    clear_and_incr_past_trans_unit_copy_address_pointer(ptr);		      \
  }  /* if */							      \
  clear_and_incr_past_orphan_pointer(ptr);			      \
  incr_num_il_entry_prefixes_allocated();                             \
  clear_il_entry_prefix(ptr, TRUE, !is_primary_translation_unit);     \
  ptr += SPACE_FOR_IL_ENTRY_PREFIX;                                   \
}  /* do_fs_alloc */


/*
Allocate space in an arbitrary memory region (i.e., choose between the
file-scope and normal allocation methods as necessary).
*/
#define do_any_alloc(ptr, region_number, size)                        \
{ if ((region_number) == file_scope_region_number) {                  \
    do_fs_alloc((ptr), (size), file_scope_region_number);             \
  } else {                                                            \
    do_alloc((ptr), (region_number), FALSE, (size));                  \
  }  /* if */                                                         \
}  /* do_any_alloc */


#define clear_tagged_ptr(tagged_ptr)                                        \
  (((tagged_ptr).kind = (a_byte_il_entry_kind)iek_none),                    \
   ((tagged_ptr).ptr = NULL))

#ifdef TRACE_ALLOC
/*
If a problem is found with a node allocated at address A, it is often useful
to find where that node was created.  The following simple facility allows
this to be traced as follows:
   (a) determine the suspect node address A in a debugger
   (b) set a breakpoint on main() and on alloc_intercept()
   (c) rerun the same binary with the same options and input
   (d) when the breakpoint on main() is hit, set trace_alloc_ptr to A
           (e.g., "p trace_alloc_ptr=0x123456" in gdb)
   (e) continue execution: the breakpoint on alloc_intercept() will be hit
       when the suspect node is created

Note that this may not work as expected for addresses determined after the IL
has been read from a file, because the nodes were allocated at a different
address before the IL was written out.

Note that this variable is not re-initialized if the front end is
called multiple times.
*/
static void *trace_alloc_ptr = NULL;

static void alloc_intercept(void)
/*
This routine's main purpose is to have a breakpoint set on it from a symbolic
debugger.  The routine is called if memory is allocated at the address pointed
to by trace_alloc_ptr.
*/
{
#if DEBUG
  fprintf(f_debug, "Created node at %p.\n", (void*)trace_alloc_ptr);
#endif /* DEBUG */
}  /* alloc_intercept */


void trace_alloc_check(void *ptr)
/*
Check if the given pointer ptr matches the address stored in trace_alloc_ptr.
If so, call alloc_intercept.
*/
{
  if (ptr == trace_alloc_ptr) alloc_intercept();
}  /* trace_alloc_check */

#endif /* TRACE_ALLOC */

char *alloc_il(sizeof_t size)
/*
Allocate and return "size" bytes of storage in the file scope memory region.
*/
{
  char *ptr;
  do_fs_alloc(ptr, size, file_scope_region_number);
#ifdef TRACE_ALLOC
  trace_alloc_check(ptr);
#endif /* TRACE_ALLOC */
  return ptr;
}  /* alloc_il */


char *alloc_primary_file_scope_il(sizeof_t size)
/*
Allocate and return "size" bytes of storage in the file scope memory region
of the primary translation unit.
*/
{
  char *ptr;
  a_boolean saved_is_primary_translation_unit = is_primary_translation_unit;
  is_primary_translation_unit = TRUE;
  if (!saved_is_primary_translation_unit) compute_il_prefix_size();
  do_fs_alloc(ptr, size, FILE_SCOPE_REGION_NUMBER);
  is_primary_translation_unit = saved_is_primary_translation_unit;
  if (!saved_is_primary_translation_unit) compute_il_prefix_size();
#ifdef TRACE_ALLOC
  trace_alloc_check(ptr);
#endif /* TRACE_ALLOC */
  return ptr;
}  /* alloc_primary_file_scope_il */


static char *alloc_secondary_file_scope_il(sizeof_t               size,
                                           a_translation_unit_ptr tup)
/*
Allocate and return "size" bytes of storage in the file scope memory region
of the secondary translation unit identified by tup.
*/
{
  char *ptr;
  a_boolean saved_is_primary_translation_unit = is_primary_translation_unit;
  is_primary_translation_unit = FALSE;
  if (saved_is_primary_translation_unit) compute_il_prefix_size();
  do_fs_alloc(ptr, size, tup->file_scope_region_number);
  is_primary_translation_unit = saved_is_primary_translation_unit;
  if (saved_is_primary_translation_unit) compute_il_prefix_size();
#ifdef TRACE_ALLOC
  trace_alloc_check(ptr);
#endif /* TRACE_ALLOC */
  return ptr;
}  /* alloc_secondary_file_scope_il */

#if !STANDALONE_UTILITY_PROGRAM

static char *alloc_cil(sizeof_t size)
/*
Allocate and return "size" bytes of storage in the current IL memory region.
*/
{
  char *ptr;
  do_any_alloc(ptr, curr_il_region_number, size);
#ifdef TRACE_ALLOC
  trace_alloc_check(ptr);
#endif /* TRACE_ALLOC */
  return ptr;
}  /* alloc_cil */


static char *alloc_in_same_region_as(a_source_correspondence *scp,
                                     sizeof_t                size)
/*
Allocate and return "size" bytes of storage in the same memory region
as the entity whose source correspondence is given by scp.  If scp is
NULL, allocate the space in the current file scope memory region.
*/
{
  char *ptr;

  if (scp == NULL) {
    ptr = alloc_il(size);
  } else if (!in_file_scope(scp)) {
    ptr = alloc_cil(size);
  } else if (!in_secondary_trans_unit(scp)) {
    ptr = alloc_primary_file_scope_il(size);
  } else {
    /* Allocate in some secondary translation unit's file scope memory
       region. */
    a_translation_unit_ptr tup;
    check_assertion(in_front_end);
    if (scp->assoc_info != NULL) {
      tup = trans_unit_for_source_corresp(scp);
      check_assertion(tup != translation_units);
    } else {
      /* No associated symbol, so pick an arbitrary secondary translation
         unit. */
      if (!is_primary_translation_unit) {
        tup = curr_translation_unit;
      } else {
        tup = translation_units->next;
      }  /* if */
    }  /* if */
    ptr = alloc_secondary_file_scope_il(size, tup);
  }  /* if */
  return ptr;
}  /* alloc_in_same_region_as */


char *alloc_text_of_string_literal(sizeof_t size)
/*
Allocate space for the text of a string literal, and return a pointer to it.
The space allocated is large enough to contain "size" characters.
This routine exists as a way of tracking the space use.  The space is always
allocated at the file scope, because string values can be shared (at
least in non-pcc mode).
*/
{
#if DEBUG
  string_literal_text_space_allocated += (unsigned long)size;
#endif /* DEBUG */
  return alloc_il(size);
}  /* alloc_text_of_string_literal */


char *copy_string_to_region(a_memory_region_number region,
                            char                   *string)
/*
Make a copy of the specified string in the memory region indicated by
"region" (which must be the front end or file scope region number).
*/
{
  sizeof_t	length;
  char		*new_string;

  length = strlen(string);
  if (region == FRONT_END_REGION_NUMBER) {
    new_string = (char *)alloc_fe(length+1);
  } else {
    check_assertion(region == file_scope_region_number);
    new_string = alloc_il(length+1);
  }  /* if */
  (void)strcpy(new_string, string);
  return new_string;
}  /* copy_string_to_region */


char *copy_string_of_length_to_region(a_memory_region_number region,
				      char                   *string,
				      sizeof_t		     length)
/*
Make a copy of the specified string, whose length is specified by "length"
in the memory region indicated by "region" (which must be the front end or
file scope region number).
*/
{
  char		*new_string;

  if (region == FRONT_END_REGION_NUMBER) {
    new_string = (char *)alloc_fe(length+1);
  } else {
    check_assertion(region == file_scope_region_number);
    new_string = alloc_il(length+1);
  }  /* if */
  (void)strncpy(new_string, string, size_t_arg(length));
  /* Terminate the string. */
  new_string[length] = '\0';
  return new_string;
}  /* copy_string_of_length_to_region */


#endif /* !STANDALONE_UTILITY_PROGRAM */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED

a_scope_orphaned_list_header_ptr alloc_scope_orphaned_list_header(
                                                 a_routine_ptr   assoc_routine,
                                                 a_scope_number  scope_number)
/*
Allocate a scope orphaned list header, initialize it with the indicated
routine and scope number, and return a pointer to it.
*/
{
  a_scope_orphaned_list_header_ptr solhp;

  solhp = (a_scope_orphaned_list_header_ptr)
                                alloc_il(sizeof(a_scope_orphaned_list_header));
#if DEBUG && !STANDALONE_UTILITY_PROGRAM
  num_scope_orphaned_list_headers_allocated++;
#endif /* DEBUG  && !STANDALONE_UTILITY_PROGRAM */
  solhp->assoc_routine = assoc_routine,
  solhp->scope_number =  scope_number,
  solhp->orphaned_types = NULL;
  solhp->orphaned_variables = NULL;
  solhp->orphaned_namespaces = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  solhp->orphaned_src_seq_sublists = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  solhp->next = NULL;

  return solhp;
}  /* alloc_scope_orphaned_list_header */

#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

a_source_file_ptr alloc_source_file(void)
/*
Allocate a source file entry, initialize it, and return a pointer to it.
*/
{
  a_source_file_ptr  sfp;

  /* Entries for secondary translation units are allocated in the primary
     translation unit file scope memory region. */
  sfp = (a_source_file_ptr)alloc_primary_file_scope_il(sizeof(a_source_file));
#if DEBUG
  num_source_files_allocated++;
#endif /* DEBUG */
  sfp->file_name        = NULL;
  sfp->full_name        = NULL;
  sfp->name_as_written  = NULL;
  sfp->first_seq_number = 0;
  sfp->last_seq_number  = MAX_SEQ_NUMBER;  /* Not yet entered. */
  sfp->first_line_number= 0;
  sfp->first_child_file = NULL;
  sfp->last_child_file  = NULL;
  sfp->next             = NULL;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  sfp->related_file_implicit_include_done = FALSE;
  sfp->is_implicit_include = FALSE;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  sfp->is_include_file = FALSE;
  sfp->included_by_system_include = FALSE;
  sfp->included_by_preinclude = FALSE;
  sfp->preinclude_macros_only = FALSE;
  sfp->from_system_include_dir = FALSE;
  sfp->top_level_file = FALSE;
  sfp->top_level_file_from_pch = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  sfp->is_assembly_file = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  return sfp;
}  /* alloc_source_file */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_cli_metadata_file_ptr alloc_cli_metadata_file(void)
/*
Allocate a CLI metadata file entry, clear it to default values, and return
a pointer to it.
*/
{
  a_cli_metadata_file_ptr cmfp;

  cmfp = alloc_il_of_type(a_cli_metadata_file);
  cmfp->name_as_written = NULL;
  cmfp->full_name       = NULL;
  cmfp->next            = NULL;
  cmfp->position        = null_source_position;
  cmfp->assembly_index  = 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  cmfp->assembly_file   = NULL;
  cmfp->inserted_position = null_source_position;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  cmfp->as_friend       = FALSE;
  cmfp->referenced_by_preusing     = FALSE;
  cmfp->referenced_by_system_using = FALSE;

  return cmfp;
}  /* alloc_cli_metadata_file */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if ONE_INSTANTIATION_PER_OBJECT

a_per_instantiation_needed_flags_entry_ptr
            alloc_per_instantiation_needed_flags_entry(a_boolean at_file_scope)
/*
Allocate a per-instantiation needed flags entry, clear it to default values,
and return a pointer to it.  The entry is allocated in the file scope memory
region if at_file_scope is TRUE.
*/
{
  a_per_instantiation_needed_flags_entry_ptr pinfep;

  if (at_file_scope) {
    pinfep = (a_per_instantiation_needed_flags_entry_ptr)
                     alloc_primary_file_scope_il(
                               sizeof(a_per_instantiation_needed_flags_entry));
  } else {
    pinfep = (a_per_instantiation_needed_flags_entry_ptr)
                     alloc_cil(sizeof(a_per_instantiation_needed_flags_entry));
  }  /* if */
#if DEBUG
  num_per_instantiation_needed_flags_entries_allocated++;
#endif /* DEBUG */
  pinfep->next = NULL;
  memzero((char *)pinfep->bytes, sizeof(pinfep->bytes));
  return pinfep;
}  /* alloc_per_instantiation_needed_flags_entry */

#endif /* ONE_INSTANTIATION_PER_OBJECT */

void set_template_param_constant_kind(a_constant                     *cp,
                                      a_template_param_constant_kind kind)
/*
Set the kind of a template parameter constant.  *cp is already a
ck_template_param constant.
*/
{
  check_assertion_str(cp->kind == (a_constant_repr_kind)ck_template_param,
                    "set_template_param_constant_kind: not ck_template_param");
  cp->variant.template_param.kind = kind;
  cp->variant.template_param.is_qualified_name = FALSE;
  cp->variant.template_param.is_pack = FALSE;
  switch (kind) {
    case tpck_param:
      cp->variant.template_param.variant.coordinates.position = 0;
      cp->variant.template_param.variant.coordinates.depth = NO_NESTING_DEPTH;
      break;
    case tpck_expression:
      cp->variant.template_param.variant.expr = NULL;
      break;
    case tpck_member:
      break;
    case tpck_unknown_function:
      cp->variant.template_param.variant.unknown_function.conversion_type =
                                                                          NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      cp->variant.template_param.variant.unknown_function
                                               .property_or_event_descr = NULL;
      cp->variant.template_param.variant.unknown_function.special_kind =
                                             (a_special_function_kind)sfk_none;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      cp->variant.template_param.variant.unknown_function.symbol = NULL;
      cp->variant.template_param.variant.unknown_function.opname_kind =
                                                      (an_opname_kind)onk_none;
      break;
    case tpck_cast:
    case tpck_address:
      cp->variant.template_param.variant.constant = NULL;
      break;
    case tpck_sizeof:
    case tpck_alignof:
    case tpck_uuidof:
    case tpck_typeid:
      cp->variant.template_param.variant.templ_sizeof.type = NULL;
      cp->variant.template_param.variant.templ_sizeof.expr = NULL;
#if PROTOTYPE_INSTANTIATIONS_IN_IL
      cp->variant.template_param.variant.templ_sizeof.local_expr_ref = FALSE;
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */
      break;
    case tpck_template_ref:
      cp->variant.template_param.variant.template_ref.con = NULL;
      cp->variant.template_param.variant.template_ref.arg_list = NULL;
      break;
    default:
      unexpected_condition_str("set_template_param_constant_kind: bad kind");
  }  /* switch */
}  /* set_template_param_constant_kind */


void set_constant_kind(a_constant           *cp,
                       a_constant_repr_kind kind)
/*
Set the kind of the constant to "kind", and set the associated variant
fields to default values.
*/
{
  /* When changing this routine because the structure of a_constant
     has changed, be sure to change eq_constants as well. */
  cp->kind = kind;
  switch (kind) {
    case ck_error:
      /* No variant fields to set. */
      break;
#if UPC_EXTENSIONS_ALLOWED
    case ck_upc_mythread:
      /* No variant fields to set. */
      break;
    /* Handle UPC thread constants like integers. */
    case ck_upc_threads:
#endif /* UPC_EXTENSIONS_ALLOWED */
    case ck_integer:
      set_integer_value(&cp->variant.integer_value,
                        (a_host_large_integer)0);
      break;
#if FIXED_POINT_ALLOWED
    case ck_fixed_point:
      fxp_init_value(&cp->variant.fixed_point_value);
      break;
#endif /* FIXED_POINT_ALLOWED */
    case ck_string:
      cp->variant.string.length = 0;
      cp->variant.string.value = NULL;
#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
      cp->variant.string.sequence_number = 0;
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
      break;
    case ck_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      /* The entire float_value must be zeroed to allow use of memcmp
         and the like on the field. */
      memzero((char *)&cp->variant.float_value,
              sizeof(cp->variant.float_value));
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_complex:
      /* The entire float_value must be zeroed to allow use of memcmp
         and the like on the field. */
      cp->variant.complex_value = (an_internal_complex_value_ptr)
                                 alloc_il(sizeof(*cp->variant.complex_value));
      memzero((char *)cp->variant.complex_value,
              sizeof(*cp->variant.complex_value));
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case ck_address:
      cp->variant.address.kind = (an_address_base_kind)abk_variable;
      cp->variant.address.variant.variable = NULL;
      cp->variant.address.offset = 0;
      break;
    case ck_ptr_to_member:
      cp->variant.ptr_to_member.casting_base_class = NULL;
      cp->variant.ptr_to_member.name_reference     = NULL;
      cp->variant.ptr_to_member.cast_to_base    = FALSE;
      cp->variant.ptr_to_member.is_function_ptr = FALSE;
#if CENTERLINE_CHECKING
      cp->variant.ptr_to_member.avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
      cp->variant.ptr_to_member.variant.field   = NULL;
      break;
#if GNU_EXTENSIONS_ALLOWED
    case ck_label_difference:
      cp->variant.label_difference.from_address = NULL;
      cp->variant.label_difference.to_address = NULL;
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
      cp->variant.stack_offset.variable = NULL;
      cp->variant.stack_offset.offset   = 0;
      break;
#endif /* DO_IL_LOWERING && ... */
    case ck_dynamic_init:
      cp->variant.dynamic_init = NULL;
      break;
    case ck_aggregate:
      cp->variant.aggregate.first_constant = NULL;
      cp->variant.aggregate.last_constant  = NULL;
      break;
    case ck_init_repeat:
      cp->variant.init_repeat.constant = NULL;
      cp->variant.init_repeat.count = 0;
      cp->variant.init_repeat.multidimensional_aggr_tail_not_repeated = FALSE;
      break;
    case ck_template_param:
      set_template_param_constant_kind(cp, 
                                  (a_template_param_constant_kind)tpck_param);
      break;
    case ck_designator:
      cp->variant.designator.field = NULL;
      cp->variant.designator.array_element = 0;
      break;
#if CHECKING
    default:
      internal_error("set_constant_kind: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* set_constant_kind */


void clear_constant(a_constant           *cp,
                    a_constant_repr_kind kind)
/*
Clear the indicated constant entry, set the kind as given, and set the
associated variant fields to default values.
*/
{
  /* When changing this routine because the structure of a_constant
     has changed, be sure to change eq_constants as well. */
  set_default_source_corresp(cp->source_corresp);
  cp->next           = NULL;
  cp->type           = NULL;
  cp->expr           = NULL;
  cp->rescan_info    = NULL;
#if DO_IL_LOWERING
  cp->assoc_var      = NULL;
#endif /* DO_IL_LOWERING */
  cp->character_kind = (a_character_kind)chk_default;
  cp->implicit_cast  = FALSE;
  cp->explicit_cast_applied = FALSE;
  cp->is_reinterpret_cast = FALSE;
  cp->non_arithmetic = FALSE;
  cp->is_simple_zero = FALSE;
  cp->null_pointer_constant_ruled_out = FALSE;
#if GNU_EXTENSIONS_ALLOWED
  cp->null_keyword = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  cp->nullptr_keyword = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  cp->native_nullptr_keyword = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  cp->explicit_braces_on_aggregate = FALSE;
  cp->from_undefined_preproc_id = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
  cp->flexible_array_initializer = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
  cp->uses_designated_initializers = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  cp->is_literal_field = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  cp->is_pack_expansion = FALSE;
#if BACK_END_IS_C_GEN_BE
  cp->elide_aggregate_braces = FALSE;
#endif /* BACK_END_IS_C_GEN_BE */
  cp->is_named_constant_definition = FALSE;
#if CENTERLINE_CHECKING
  cp->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
  set_constant_kind(cp, kind);
}  /* clear_constant */


a_constant_ptr alloc_constant(a_constant_repr_kind kind)
/*
Allocate a constant entry of the indicated kind, set its fields to default
values, and return a pointer to it.
*/
{
  register a_constant_ptr cp;

  db_enter(5, "alloc_constant");

  cp = (a_constant_ptr)alloc_cil(sizeof(a_constant));
#if DEBUG
  num_constants_allocated++;
#endif /* DEBUG */
  clear_constant(cp, kind);

  db_exit();
  return cp;
}  /* alloc_constant */


a_constant_ptr fs_constant(a_constant_repr_kind kind)
/*
Same as alloc_constant, but allocates a constant in the file scope memory
region.
*/
{
  a_constant_ptr         cp;
  a_memory_region_number region_to_switch_back_to;

  switch_to_file_scope_region(&region_to_switch_back_to);
  cp = alloc_constant(kind);
  switch_back_to_original_region(region_to_switch_back_to);
  return cp;
}  /* fs_constant */


a_param_type_ptr alloc_param_type(a_type_ptr type)
/*
Allocate a new parameter type entry and return a pointer to it.  Set its
fields to default values and its type to "type".  It is always allocated
in the file scope memory region.
*/
{
  a_param_type_ptr        ptp;

  db_enter(5, "alloc_param_type");

  ptp = (a_param_type_ptr)alloc_il(sizeof(a_param_type));
#if DEBUG
  num_param_types_allocated++;
#endif /* DEBUG */
  ptp->next = NULL;
  ptp->type = type;
  ptp->declared_type = NULL;
  ptp->name = NULL;
  ptp->passed_via_copy_constructor = FALSE;
  ptp->has_default_arg = FALSE;
  ptp->default_arg_appeared_in_class_definition = FALSE;
  ptp->has_unevaluated_template_default = FALSE;
  ptp->default_being_instantiated = FALSE;
  ptp->type_involves_deduced_template_param = FALSE;
  ptp->type_involves_template_param = FALSE;
  ptp->is_parameter_pack = FALSE;
  ptp->is_pack_element = FALSE;
  ptp->qualifiers = TQ_NONE;
#if GNU_EXTENSIONS_ALLOWED
  ptp->is_transparent = FALSE;
  ptp->nonnull = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  ptp->duplicate_name = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  ptp->is_cli_param_array = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ptp->move_ctor_or_assign_parameter = FALSE;
#if CENTERLINE_CHECKING
  ptp->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
  ptp->param_num = 0;
  ptp->default_arg_expr = NULL;
  ptp->orig_param_type_for_unevaluated_default_arg_expr = NULL;
  ptp->entities_defined_in_default_arg = NULL;
  ptp->attributes = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  ptp->ms_attributes = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  ptp->decl_pos_info = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  ptp->pack_expansion_descr = NULL;
  db_exit();
  return ptp;
}  /* alloc_param_type */


a_derivation_step_ptr alloc_derivation_step(void)
/*
Allocate and initialize a derivation step entry and return a pointer to it.
*/
{
  a_derivation_step_ptr  dsp;

  db_enter(5, "alloc_derivation_step");

  dsp = (a_derivation_step_ptr)alloc_il(sizeof(a_derivation_step));
#if DEBUG
  num_derivation_steps_allocated++;
#endif /* DEBUG */
  dsp->next       = NULL;
  dsp->base_class = NULL;

  db_exit();
  return dsp;
}  /* alloc_derivation_step */


a_base_class_derivation_ptr alloc_base_class_derivation(void)
/*
Allocate and initialize a base class derivation entry and return a pointer
to it.
*/
{
  a_base_class_derivation_ptr  bcdp;

  db_enter(5, "alloc_base_class_derivation");
  bcdp = (a_base_class_derivation_ptr)
                              alloc_il(sizeof(a_base_class_derivation));
#if DEBUG
  num_base_class_derivations_allocated++;
#endif /* DEBUG */
  bcdp->next       = NULL;
  bcdp->path       = NULL;
  bcdp->preferred  = FALSE;
  bcdp->direct     = FALSE;
  bcdp->access     = (an_access_specifier)as_public;
#if CENTERLINE_CHECKING
  bcdp->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
  db_exit();
  return bcdp;
}  /* alloc_base_class_derivation */

#if DO_IL_LOWERING && IA64_ABI

a_vcall_offset_entry_ptr alloc_vcall_offset_entry(void)
/*
Allocate a vcall offset entry, initialize its fields, and return a pointer to
it.
*/
{
  a_vcall_offset_entry_ptr voep;

  voep = (a_vcall_offset_entry_ptr)alloc_il(sizeof(a_vcall_offset_entry));
  voep->next               = NULL;
  voep->routine            = NULL;
  voep->base_class         = NULL;
  voep->vcall_offset_index = 0;
  voep->is_primary         = FALSE;

  return voep;
}  /* alloc_vcall_offset_entry */

#endif /* DO_IL_LOWERING && IA64_ABI */

an_overriding_virtual_function_ptr alloc_overriding_virtual_function(void)
/*
Allocate an overriding-virtual-function entry, initialize its fields, and
return a pointer to it.
*/
{
  an_overriding_virtual_function_ptr ovfp;

  ovfp = (an_overriding_virtual_function_ptr)alloc_il(
                                     sizeof(an_overriding_virtual_function));
#if DEBUG
  num_overriding_virtual_functions_allocated++;
#endif /* DEBUG */
  ovfp->next                         = NULL;
  ovfp->overriding_function          = NULL;
  ovfp->primary_function             = NULL;
  ovfp->base_class                   = NULL;
  ovfp->return_adjustment_base_class = NULL;

  return ovfp;
}  /* alloc_overriding_virtual_function */

static a_template_arg_ptr
		avail_template_args;
			/* List of freed template arg entries that are
			   available for reuse. */


a_template_arg_ptr alloc_template_arg(a_templ_arg_kind kind)
/*
Allocate a template argument entry, initialize its fields, and return
a pointer to it.  "kind" is the kind of template argument to be
allocated.
*/
{
  a_template_arg_ptr tap;

  if (avail_template_args != NULL) {
    tap = avail_template_args;
    avail_template_args = avail_template_args->next;
  } else {
    tap = alloc_il_of_type(a_template_arg);
#if DEBUG
    num_template_args_allocated++;
#endif /* DEBUG */
  }  /* if */
  tap->next = NULL;
  tap->kind = kind;
  tap->pack_expansion_descr = NULL;
  tap->is_array_bound_of_unknown_type = FALSE;
  tap->explicitly_specified = FALSE;
  tap->template_template_param_checked = FALSE;
  tap->is_pack_element = FALSE;
  tap->is_pack = FALSE;
#if CENTERLINE_CHECKING
  tap->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
  switch (kind) {
    case tak_type:
      tap->variant.type = NULL;
      break;
    case tak_template:
      tap->variant.templ.ptr = NULL;
      tap->variant.templ.substituted_param_template = NULL;
      break;
    case tak_nontype:
      /* It is not really necessary to initialize all of these fields, but
         this can be important in certain debugging modes. */
      tap->variant.integer_value = 0;
      tap->variant.constant = NULL;
      break;
    case tak_start_of_pack_expansion:
      break;
    default:
      unexpected_condition_str2("alloc_template_arg:", "bad kind");
      break;
  }  /* switch */
  tap->arg_operand = NULL;
  return tap;
}  /* alloc_template_arg */


void free_template_arg_list(a_template_arg_ptr  tap)
/*
Return a list of template argument entries to the available list.
*/
{
  a_template_arg_ptr  next_tap;

  while (tap != NULL) {
    next_tap = tap->next;
    tap->next = avail_template_args;
    avail_template_args = tap;
    tap = next_tap;
  }  /* while */
}  /* free_template_arg_list */


static
a_template_param_type_supplement_ptr alloc_template_param_type_supplement(void)
/*
Allocate a template parameter type supplement entry, initialize its fields,
and return a pointer to it.
*/
{
  a_template_param_type_supplement_ptr tptsp;

  tptsp = (a_template_param_type_supplement_ptr)alloc_il(
                                     sizeof(a_template_param_type_supplement));
#if DEBUG
  num_template_param_type_supplements_allocated++;
#endif /* DEBUG */
  tptsp->class_type = NULL;
  tptsp->orig_nested_type = NULL;
  tptsp->template_symbol = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  tptsp->generic_constraints = NULL;
  tptsp->generic_param_seq_number = 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return tptsp;
}  /* alloc_template_param_type_supplement */


static a_typeref_type_supplement_ptr alloc_typeref_type_supplement(void)
/*
Allocate a typeref type supplement entry, initialize its fields, and return
a pointer to it.
*/
{
  a_typeref_type_supplement_ptr ttsp;

  ttsp = (a_typeref_type_supplement_ptr)alloc_il(
                                            sizeof(a_typeref_type_supplement));
#if DEBUG
  num_typeref_type_supplements_allocated++;
#endif /* DEBUG */
  ttsp->min_template_arguments = -1;
  ttsp->expr = NULL;
  ttsp->template_arg_list = NULL;
  ttsp->assoc_template = NULL;
  ttsp->proxy_class = NULL;
  ttsp->operator_type_arg = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  ttsp->type_id_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return ttsp;
}  /* alloc_typeref_type_supplement */


static an_integer_type_supplement_ptr alloc_integer_type_supplement(void)
/*
Allocate an integer type supplement entry, initialize its fields, and return
a pointer to it.
*/
{
  an_integer_type_supplement_ptr  itsp;

  itsp = (an_integer_type_supplement_ptr)alloc_il(
                                          sizeof(an_integer_type_supplement));
#if DEBUG
  num_integer_type_supplements_allocated++;
#endif /* DEBUG */
  itsp->enumerator_list_seen = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  itsp->declared_assembly_visibility = (an_assembly_visibility)av_none;
  itsp->assembly_visibility = (an_assembly_visibility)av_none;
  itsp->assembly_scope_index = 0;
  itsp->metadata_type_def_token = 0;
  itsp->uuid_string = NULL;
  itsp->boxed_type = NULL;
#if DO_IL_LOWERING
  itsp->uuid_variable = NULL;
#endif /* DO_IL_LOWERING */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  itsp->base_type = NULL;
  return itsp;
}  /* alloc_integer_type_supplement */


a_base_class_ptr alloc_base_class(void)
/*
Allocate a base class entry, initialize its fields, and return a pointer
to it.
*/
{
  a_base_class_ptr bcp;

  bcp = (a_base_class_ptr)alloc_il(sizeof(a_base_class));

#if DEBUG
  num_base_classes_allocated++;
#endif /* DEBUG */
  bcp->next                            = NULL;
#if IA64_ABI
  bcp->next_preorder                   = NULL;
  bcp->primary_base_class              = NULL;
#endif /* IA64_ABI */
  bcp->attributes                      = NULL;
  bcp->type                            = NULL;
  bcp->orig_type                       = NULL;
  bcp->derived_class                   = NULL;
  bcp->trans_unit_corresp              = NULL;
  bcp->decl_position                   = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  bcp->base_specifier_range            = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  bcp->is_virtual                      = FALSE;
  bcp->direct                          = FALSE;
  bcp->ambiguous                       = FALSE;
  bcp->shares_virtual_function_info    = FALSE;
  bcp->ignore_during_dependent_lookup  = FALSE;
  bcp->is_optimized_empty_base         = FALSE;
#if IA64_ABI
  bcp->offset_is_set                   = FALSE;
#endif /* IA64_ABI */
  bcp->is_pack_expansion               = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  bcp->is_implicit_direct_base         = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  bcp->direct_base_number	       = 0;
  bcp->offset                          = 0;
#if !IA64_ABI
  bcp->pointer_offset                  = 0;
  bcp->pointer_base_class              = NULL;
#endif /* !IA64_ABI */
  bcp->derivation                      = NULL;
  bcp->overriding_virtual_functions    = NULL;
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  bcp->complete_subobject              = FALSE;
  bcp->pointer_offset_is_set           = FALSE;
  bcp->data_section_base_class         = NULL;
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#if DO_IL_LOWERING
#if !IA64_ABI
  bcp->virtual_function_table_var      = NULL;
#else /* IA64_ABI */
  bcp->virtual_function_table_offset   = -1;
#endif /* IA64_ABI */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  bcp->index_in_construction_vtbl_array = 0;
  bcp->base_subarray_index_in_construction_vtbl_array = 0;
#if !IA64_ABI
  bcp->base_construction_vtbls         = NULL;
#endif /* !IA64_ABI */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if IA64_ABI
  bcp->vbase_offset_index              = 0;
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING */
#if CENTERLINE_CHECKING
  bcp->avoid_codecenter_warnings       = 0;
#endif /* CENTERLINE_CHECKING */

  return bcp;
}  /* alloc_base_class */


a_class_list_entry_ptr alloc_list_entry_for_class_full(
                                                  a_source_correspondence *scp)
/*
Allocate a class-list-entry, initialize its fields, and return a pointer to it.
If scp is non-NULL, allocate the entry in the same memory region as scp;
otherwise, allocate it in the current file-scope memory region.
*/
{
  a_class_list_entry_ptr clep;

  clep = (a_class_list_entry_ptr)alloc_in_same_region_as(
                                                   scp,
                                                   sizeof(a_class_list_entry));
#if DEBUG
  num_class_list_entries_allocated++;
#endif /* DEBUG */
  clep->next  = NULL;
  clep->class_type = NULL;

  return clep;
}  /* alloc_list_entry_for_class_full */


a_class_list_entry_ptr alloc_list_entry_for_class(void)
/*
Allocate a class-list-entry, initialize its fields, and return a pointer to it.
*/
{
  return alloc_list_entry_for_class_full((a_source_correspondence *)NULL);
}  /* alloc_list_entry_for_class */


a_routine_list_entry_ptr alloc_list_entry_for_routine(void)
/*
Allocate a routine-list-entry, initialize its fields, and return a pointer
to it.
*/
{
  a_routine_list_entry_ptr rlep;

  rlep = (a_routine_list_entry_ptr)alloc_il(sizeof(a_routine_list_entry));
#if DEBUG
  num_routine_list_entries_allocated++;
#endif /* DEBUG */
  rlep->next  = NULL;
  rlep->routine = NULL;

  return rlep;
}  /* alloc_list_entry_for_routine */


a_based_type_list_member_ptr alloc_based_type_list_member(
                                               a_based_type_kind  kind,
                                               a_type_ptr         base_type)
/*
Allocate a based type list member, initialize it to the indicated kind, and
return a pointer to it.  Allocate the entry in the same memory region as
base_type, because base_type will point to the entry.
*/
{
  a_based_type_list_member_ptr btlmp;

  btlmp = (a_based_type_list_member_ptr)alloc_in_same_region_as(
                                             &base_type->source_corresp,
                                             sizeof(a_based_type_list_member));
#if DEBUG
  num_based_type_list_members_allocated++;
#endif /* DEBUG */
  btlmp->next = NULL;
  btlmp->based_type = NULL;
  btlmp->kind = kind;
  btlmp->front_end_only = FALSE;

  return btlmp;
}  /* alloc_based_type_list_member */


static void clear_class_type_supplement_definition_fields(
                                            a_class_type_supplement_ptr  ctsp)
/*
Given an pointer to a class-type-supplement entry, clear its fields that are
only meaningful when a definition (as opposed to just a declaration) of the
class is available.
*/
{
  ctsp->base_classes                      = NULL;
#if IA64_ABI
  ctsp->preorder_base_classes             = NULL;
  ctsp->primary_base_class                = NULL;
#endif /* IA64_ABI */
  ctsp->size_without_virtual_base_classes = 0;
  ctsp->alignment_without_virtual_base_classes = 1;
  ctsp->highest_virtual_function_number   = VIRTUAL_FUNCTION_NUMBER_NONE;
#if DO_IL_LOWERING && IA64_ABI
  /* There are always two entries below the address point of the virtual
     table: the offset-to-top and RTTI information. */
  ctsp->next_negative_virtual_table_index = -3;
  ctsp->first_vcall_offset_index          = 0;
  ctsp->vcall_offsets                     = NULL;
#endif /* DO_IL_LOWERING && IA64_ABI */
  ctsp->virtual_function_info_offset      = 0;
  ctsp->virtual_function_info_base_class  = NULL;
#if DECL_MODIFIERS_IN_USE
  ctsp->decl_modifiers                    = DM_NONE;
#endif /* DECL_MODIFIERS_IN_USE */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ctsp->inheritance_kind                  = (an_inheritance_kind)ihk_none;
  ctsp->inheritance_kind_is_explicit      = FALSE;
  ctsp->has_direct_property_or_event      = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  ctsp->ELF_visibility                    =
                                      (an_ELF_visibility_kind)evk_unspecified;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if BACK_END_IS_CP_GEN_BE
  ctsp->surrounding_name_linkage_state    = (a_name_linkage_kind)nlk_none;
#endif /* BACK_END_IS_CP_GEN_BE */
#if NEAR_AND_FAR_ALLOWED
  ctsp->qualifiers                        = TQ_NONE;
#endif /* NEAR_AND_FAR_ALLOWED */
#if RECORD_HIDDEN_NAMES_IN_IL
  ctsp->hidden_names_processed            = FALSE;
  ctsp->base_class_hiding_in_progress     = FALSE;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  ctsp->named_in_inline_template_directive
                                          = FALSE;
#if GNU_EXTENSIONS_ALLOWED && GCC_BUILTIN_VARARGS && USE_X86_64
  ctsp->is_va_list_tag                    = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED && GCC_BUILTIN_VARARGS && USE_X86_64 */
  ctsp->anonymous_union_kind              = (an_anonymous_union_kind)auk_none;
  ctsp->anonymous_union_field             = NULL;
  ctsp->friend_routines                   = NULL;
  ctsp->friend_classes                    = NULL;
  ctsp->assoc_scope                       = NULL;
  ctsp->partial_spec_template_arg_list    = NULL;
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  ctsp->assoc_operator_new_routine        = NULL;
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
  ctsp->assoc_operator_delete_routine     = NULL;
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if DO_IL_LOWERING
  ctsp->virtual_function_table_var        = NULL;
#if IA64_ABI
  ctsp->virtual_table_table_var           = NULL;
#endif /* IA64_ABI */
  ctsp->type_as_subobject                 = NULL;
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
  ctsp->promoted_local_types              = NULL;
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  ctsp->construction_vtbls                = NULL;
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#endif /* DO_IL_LOWERING */
}  /* clear_class_type_supplement_definition_fields */


void clear_class_type_definition_fields(a_type_ptr  class_type)
/*
Given an pointer to a class type entry, clear its fields that are only
meaningful when a definition (as opposed to just a declaration) of the
class is available (including such fields in the class type supplement).
This includes discarding the field list (if any), and making the type
incomplete (which affects the recorded size and alignment).
*/
{
  clear_class_type_supplement_definition_fields(class_type_supp(class_type));
  class_type->size = 0;
  class_type->alignment = 1;
  class_type->incomplete = TRUE;
  class_type->variant.class_struct_union.field_list = NULL;
  class_type->variant.class_struct_union.any_const_member = FALSE;
  class_type->variant.class_struct_union.any_virtual_base_classes = FALSE;
  class_type->variant.class_struct_union.abstract = FALSE;
  class_type->variant.class_struct_union.any_virtual_functions = FALSE;
  class_type->variant.class_struct_union.any_pure_virtual_functions = FALSE;
  class_type->variant.class_struct_union.
                      any_virtual_functions_including_in_base_classes = FALSE;
  class_type->variant.class_struct_union.
                               nested_class_defined_outside_of_parent = FALSE;
  class_type->variant.class_struct_union.is_empty_class = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* A delegate must be a defined ref class.  If the definition is discarded,
       it should be treated as an ordinary ref class. */
    class_type->variant.class_struct_union.is_delegate_class = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* clear_class_type_definition_fields */


static void clear_class_type_supplement(a_class_type_supplement_ptr  ctsp)
/*
Give an pointer to a class-type-supplement entry, initialize its fields.
*/
{
  clear_class_type_supplement_definition_fields(ctsp);
#if MICROSOFT_EXTENSIONS_ALLOWED
  ctsp->uuid_string                       = NULL;
  ctsp->orig_type_kind                    = (a_type_kind)tk_error;
  ctsp->declared_assembly_visibility      = (an_assembly_visibility)av_none;
  ctsp->assembly_visibility               = (an_assembly_visibility)av_none;
  ctsp->cli_class_type_kind               =
                                         (a_cli_class_type_kind)cctk_standard;
  ctsp->is_hide_by_sig                    = FALSE;
  ctsp->is_cli_array                      = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  ctsp->compiler_generated                = FALSE;
#endif /* DO_IL_LOWERING */
  ctsp->is_lambda_closure_class           = FALSE;
  ctsp->is_initializer_list               = FALSE;
  ctsp->has_initializer_list_ctor         = FALSE;
  ctsp->has_anonymous_union_member        = FALSE;
#if NEED_NAME_MANGLING
  ctsp->defined_in_static_data_member_initializer
                                          = FALSE;
#endif /* NEED_NAME_MANGLING */
  ctsp->befriending_classes               = NULL;
  ctsp->assoc_template                    = NULL;
  ctsp->template_arg_list                 = NULL;
#if DO_IL_LOWERING
#if MICROSOFT_EXTENSIONS_ALLOWED
  ctsp->uuid_variable                     = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* DO_IL_LOWERING */
  ctsp->min_template_arguments            = -1;
#if NEED_NAME_MANGLING
  ctsp->lambda_parent.routine             = NULL;
#endif /* NEED_NAME_MANGLING */
  ctsp->hash_value = 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  ctsp->corresponding_basic_type          = NULL;
  ctsp->assembly_scope_index              = 0;
  ctsp->metadata_type_def_token           = 0;
  ctsp->base_dispose_bool_routine         = NULL;
  ctsp->base_idisposable_dispose_routine  = NULL;
  ctsp->base_object_finalize_routine      = NULL;
  ctsp->invocation_type                   = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* clear_class_type_supplement */


void set_type_kind(a_type_ptr  pte,
                   a_type_kind kind)
/*
Set the kind of the type to "kind", and set the associated variant fields
to default values.
*/
{
  a_routine_type_supplement_ptr rtsp;

  pte->kind = kind;
  switch (kind) {
    case tk_error:
    case tk_unknown:
    case tk_void:
    case tk_nullptr:
      /* No variant fields to set. */
      break;
    case tk_integer:
      pte->variant.integer.int_kind = (an_integer_kind)ik_int;
      pte->variant.integer.explicitly_signed = FALSE;
      pte->variant.integer.enum_type = FALSE;
      pte->variant.integer.is_scoped_enum = FALSE;
#if DO_IL_LOWERING
      pte->variant.integer.originally_a_scoped_enum = FALSE;
#endif /* DO_IL_LOWERING */
      pte->variant.integer.has_explicit_enum_base = FALSE;
#if GNU_EXTENSIONS_ALLOWED
      pte->variant.integer.packed = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
      pte->variant.integer.wchar_t_type = FALSE;
      pte->variant.integer.char16_t_type = FALSE;
      pte->variant.integer.char32_t_type = FALSE;
      pte->variant.integer.bool_type = FALSE;
      pte->variant.integer.originally_unnamed = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.integer.microsoft_sized_int_type = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if CENTERLINE_CHECKING
      pte->variant.integer.avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
      pte->variant.integer.enum_info.affiliated_type = NULL;
      pte->variant.integer.extra_info = alloc_integer_type_supplement();
      break;
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
      pte->variant.fixed_point.precision = (a_fixed_point_precision)fpp_short;
      pte->variant.fixed_point.is_unsigned = FALSE;
      pte->variant.fixed_point.is_fract_type = FALSE;
      pte->variant.fixed_point.saturating = FALSE;
      break;
#endif /* FIXED_POINT_ALLOWED */
    case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
    case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      pte->variant.float_kind = (a_float_kind)fk_float;
      break;
    case tk_pointer:
      pte->variant.pointer.type = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.pointer.base_variable = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pte->variant.pointer.is_reference = FALSE;
      pte->variant.pointer.is_rvalue_reference = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.pointer.is_handle = FALSE;
      pte->variant.pointer.is_interior_ptr = FALSE;
      pte->variant.pointer.is_pin_ptr = FALSE;
      pte->variant.pointer.modifiers = PM_NONE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      break;
    case tk_array:
      pte->variant.array.element_type = NULL;
      pte->variant.array.qualifiers = TQ_NONE;
      pte->variant.array.is_template_dependent_size_array = FALSE;
      pte->variant.array.is_variable_size_array = FALSE;
      pte->variant.array.is_vla = FALSE;
      pte->variant.array.constant_bound_expr_in_local_expr_node_ref = FALSE;
      pte->variant.array.dep_constant_bound_expr_in_local_expr_node_ref=FALSE;
      pte->variant.array.has_assoc_vla_dimension = FALSE;
      pte->variant.array.bound_is_zero = FALSE;
      pte->variant.array.is_static = FALSE;
      pte->variant.array.variant.number_of_elements = 0;
      pte->variant.array.bound_constant = NULL;
#if UPC_EXTENSIONS_ALLOWED
      pte->variant.array.is_threads_dimension = FALSE;
#endif /* UPC_EXTENSIONS_ALLOWED */
      break;
    case tk_class:
    case tk_struct:
    case tk_union:
      pte->variant.class_struct_union.field_list = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.class_struct_union.is_interface = FALSE;
      pte->variant.class_struct_union.is_interface_like = FALSE;
      pte->variant.class_struct_union.is_delegate_class = FALSE;
      pte->variant.class_struct_union.is_generic_definition = FALSE;
      pte->variant.class_struct_union.is_generic_instance = FALSE;
      pte->variant.class_struct_union.is_open_constructed_type = FALSE;
      pte->variant.class_struct_union.is_generic_constraint = FALSE;
      pte->variant.class_struct_union.is_hybrid_constraint = FALSE;
      pte->variant.class_struct_union.any_interface_constraints = FALSE;
      pte->variant.class_struct_union.unconstrained = FALSE;
      pte->variant.class_struct_union.sealed = FALSE;
#if BACK_END_IS_CP_GEN_BE
      pte->variant.class_struct_union.
                 defined_with_abstract_class_modifier = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pte->variant.class_struct_union.final = FALSE;
      pte->variant.class_struct_union.any_const_member = FALSE;
      pte->variant.class_struct_union.any_volatile_member = FALSE;
      pte->variant.class_struct_union.any_mutable_member = FALSE;
      pte->variant.class_struct_union.any_virtual_base_classes = FALSE;
      pte->variant.class_struct_union.abstract = FALSE;
      pte->variant.class_struct_union.any_virtual_functions = FALSE;
      pte->variant.class_struct_union.any_pure_virtual_functions = FALSE;
      pte->variant.class_struct_union.
                 any_virtual_functions_including_in_base_classes = FALSE;
      pte->variant.class_struct_union.
                 nested_class_defined_outside_of_parent = FALSE;
      pte->variant.class_struct_union.originally_unnamed = FALSE;
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
      pte->variant.class_struct_union.is_nonstd_anonymous_union_type = FALSE;
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
      pte->variant.class_struct_union.is_template_class = FALSE;
      pte->variant.class_struct_union.is_nonreal_class = FALSE;
      pte->variant.class_struct_union.is_ms_instantiated_nonreal_class = FALSE;
      pte->variant.class_struct_union.is_prototype_instantiation = FALSE;
      pte->variant.class_struct_union.is_specialized = FALSE;
      pte->variant.class_struct_union.specialized_with_old_syntax = FALSE;
      pte->variant.class_struct_union.is_in_class_specialization = FALSE;
      pte->variant.class_struct_union.explicitly_instantiated = FALSE;
      pte->variant.class_struct_union.do_not_instantiate = FALSE;
      pte->variant.class_struct_union.proxy_class = FALSE;
#if MAINTAIN_NEEDED_FLAGS
      pte->variant.class_struct_union.definition_needed = FALSE;
      pte->variant.class_struct_union.keep_definition_in_il = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
      pte->variant.class_struct_union.is_empty_class = FALSE;
      pte->variant.class_struct_union.has_zero_init_component = FALSE;
      pte->variant.class_struct_union.contains_flexible_array_member = FALSE;
#if GNU_EXTENSIONS_ALLOWED
      pte->variant.class_struct_union.is_transparent = FALSE;
#if USER_CONTROL_OF_STRUCT_PACKING
      pte->variant.class_struct_union.is_packed = FALSE;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#endif /* GNU_EXTENSIONS_ALLOWED */
      pte->variant.class_struct_union.has_operator_ampersand = FALSE;
      pte->variant.class_struct_union.virtual_functions_marked_as_required =
                                                                         FALSE;
      pte->variant.class_struct_union.copy_assignment_decl_suppressed = FALSE;
      pte->variant.class_struct_union.copy_ctor_decl_suppressed = FALSE;
      pte->variant.class_struct_union.default_ctor_decl_suppressed = FALSE;
      pte->variant.class_struct_union.dtor_decl_suppressed = FALSE;
      pte->variant.class_struct_union.inc_class_used_in_array_type = FALSE;
#if CENTERLINE_CHECKING
      pte->variant.class_struct_union.avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
#if USER_CONTROL_OF_STRUCT_PACKING
      pte->variant.class_struct_union.max_member_alignment = 0;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if PROTOTYPE_INSTANTIATIONS_IN_IL
      pte->variant.class_struct_union.template_parameter_type = NULL;
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */
      /* Allocate the class type supplement. */
      {
        a_class_type_supplement_ptr  ctsp;
        ctsp = (a_class_type_supplement_ptr)alloc_il(
                                             sizeof(a_class_type_supplement));
#if DEBUG
        num_class_type_supplements_allocated++;
#endif /* DEBUG */
        clear_class_type_supplement(ctsp);
        pte->variant.class_struct_union.extra_info = ctsp;
#if MICROSOFT_EXTENSIONS_ALLOWED
        ctsp->orig_type_kind = kind;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
      break;
    case tk_routine:
      pte->variant.routine.return_type = NULL;
      pte->variant.routine.extra_info = rtsp =
           (a_routine_type_supplement_ptr)alloc_il(
                                           sizeof(a_routine_type_supplement));
#if DEBUG
      num_routine_type_supplements_allocated++;
#endif /* DEBUG */
      rtsp->param_type_list          = NULL;
      rtsp->assoc_routine            = NULL;
      rtsp->has_ellipsis             = FALSE;
      rtsp->prototyped               = FALSE;
      rtsp->old_style_params_scanned = FALSE;
      rtsp->trailing_return_type     = FALSE;
      rtsp->lint_argsused_flag       = FALSE;
      rtsp->value_returned_by_cctor  = FALSE;
#if DO_IL_LOWERING
      rtsp->value_returned_as_parameter = FALSE;
      rtsp->return_value_parameter_follows_this = FALSE;
#endif /* DO_IL_LOWERING */
      rtsp->assoc_routine_is_ctor    = FALSE;
      rtsp->assoc_routine_is_dtor    = FALSE;
      rtsp->suppress_diagnostic_on_incomplete_return_type = FALSE;
      rtsp->routine_name_linkage     = default_routine_name_linkage;
      rtsp->routine_name_linkage_is_explicit = FALSE;
      rtsp->does_not_return          = FALSE;
#if GNU_EXTENSIONS_ALLOWED
      rtsp->result_should_be_used    = FALSE;
      rtsp->is_const                 = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
      rtsp->is_variadic_instance     = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
      rtsp->explicit_calling_convention = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
#if CENTERLINE_CHECKING
      rtsp->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
      rtsp->lint_varargs_count       = NOT_LINT_VARARGS;
      rtsp->arg_pragma               = (a_pragma_kind)pk_none;
#if GNU_EXTENSIONS_ALLOWED
      rtsp->fmt_arg                  = 0;
      rtsp->sentinel_pos             = 0;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
      rtsp->calling_convention       = (a_calling_convention)cc_default;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
      rtsp->this_class               = NULL;
      rtsp->qualifiers               = TQ_NONE;
      rtsp->this_qualifiers          = TQ_NONE;
      rtsp->prototype_scope          = NULL;
      rtsp->exception_specification  = NULL;
      break;
    case tk_typeref:
      pte->variant.typeref.type        = NULL;
      pte->variant.typeref.extra_info = alloc_typeref_type_supplement();
#if DO_IL_LOWERING
      pte->variant.typeref.orig_type   = NULL;
#endif /* DO_IL_LOWERING */
#if UPC_EXTENSIONS_ALLOWED
      pte->variant.typeref.upc_block_size = UPC_BLOCK_SIZE_NONE;
#endif /* UPC_EXTENSIONS_ALLOWED */
      pte->variant.typeref.qualifiers  = TQ_NONE;
      pte->variant.typeref.predeclared = FALSE;
#if NEAR_AND_FAR_ALLOWED
      pte->variant.typeref.explicit_memory_attribute_made_implicit = FALSE;
#endif /* NEAR_AND_FAR_ALLOWED */
      pte->variant.typeref.has_variably_modified_type = FALSE;
#if BACK_END_IS_CP_GEN_BE
      pte->variant.typeref.surrounding_name_linkage_state
                                       = (a_name_linkage_kind)nlk_none;
#endif /* BACK_END_IS_CP_GEN_BE */
      pte->variant.typeref.is_decltype = FALSE;
      pte->variant.typeref.decltype_expr_not_parenthesized = FALSE;
      pte->variant.typeref.is_underlying_type = FALSE;
#if GNU_EXTENSIONS_ALLOWED
      pte->variant.typeref.is_typeof = FALSE;
      pte->variant.typeref.is_typeof_with_type_operand = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
      pte->variant.typeref.is_dependent_type_operator = FALSE;
      pte->variant.typeref.for_type_attributes = FALSE;
      pte->variant.typeref.is_alias = FALSE;
      pte->variant.typeref.is_template_alias = FALSE;
      pte->variant.typeref.is_nonreal = FALSE;
      pte->variant.typeref.is_prototype_instantiation = FALSE;
#if CENTERLINE_CHECKING
      pte->variant.typeref.avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
      /* Clear size and alignment because they aren't used in typerefs. */
      pte->size = 0;
      pte->alignment = 1;
      break;
    case tk_ptr_to_member:
      pte->variant.ptr_to_member.class_of_which_a_member = NULL;
      pte->variant.ptr_to_member.type                    = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.ptr_to_member.modifiers = PM_NONE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      break;
    case tk_template_param:
      {
        a_template_param_type_supplement_ptr	tptsp;
        pte->variant.template_param.kind =
                                       (a_template_param_type_kind)tptk_param;
        pte->variant.template_param.is_pack = FALSE;
        pte->variant.template_param.is_generic_param = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        pte->variant.template_param.being_checked = FALSE;
        pte->variant.template_param.is_generic_function_param = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        tptsp = alloc_template_param_type_supplement();
        pte->variant.template_param.extra_info = tptsp;
        tptsp->coordinates.position = 0;
        tptsp->coordinates.depth = NO_NESTING_DEPTH;
      }
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      pte->variant.vector.element_type = NULL;
      pte->variant.vector.size_constant = NULL;
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if CHECKING
    default:
      internal_error("set_type_kind: bad type kind");
#endif /* CHECKING */
  }  /* switch */
}  /* set_type_kind */


void clear_type(a_type_ptr  pte,
                a_type_kind kind)
/*
Clear the indicated type entry, set the kind as given, and set the associated
variant fields to default values.
*/
{
  set_default_source_corresp(pte->source_corresp);
  pte->next = NULL;
  pte->based_types = NULL;
  pte->size = 0;
  pte->alignment = 1;
  if (kind == (a_type_kind)tk_class ||
      kind == (a_type_kind)tk_struct ||
      kind == (a_type_kind)tk_union ||
      kind == (a_type_kind)tk_array ||
      kind == (a_type_kind)tk_void) {
    /* These types are incomplete by default.  (void is always incomplete.) */
    pte->incomplete = TRUE;
  } else {
    /* Other types are always complete. */
    pte->incomplete = FALSE;
  }  /* if */
  pte->used_in_exception_or_rtti = FALSE;
  pte->declared_in_function_prototype = FALSE;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  pte->use_cfront_transitional_nested_type_name_mangling = FALSE;
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if BACK_END_IS_C_GEN_BE
  pte->prototype_scope_types_if_any_promoted = FALSE;
  pte->has_been_defined = FALSE;
  pte->typedef_pending = FALSE;
  pte->generated_as_empty_struct = FALSE;
#endif /* BACK_END_IS_C_GEN_BE */
  pte->typedef_definition_has_been_put_out = FALSE;
#if BACK_END_IS_CP_GEN_BE
  pte->has_been_declared = FALSE;
  pte->definition_delayed = FALSE;
  pte->elaborated_type_specifier_needed = FALSE;
  pte->replace_by_generated_typedef = FALSE;
  pte->typedef_for_vacuous_dtor_call_put_out = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  pte->emit_microsoft_class_decl_modifiers = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* BACK_END_IS_CP_GEN_BE */
#if USER_CONTROL_OF_STRUCT_PACKING
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  pte->alignment_set_explicitly = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if GNU_EXTENSIONS_ALLOWED
  pte->variables_are_implicitly_referenced = FALSE;
  pte->may_alias = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  pte->has_microsoft_w64_specifier = FALSE;
  pte->is_microsoft_intrinsic = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  pte->autonomous_primary_tag_decl = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  pte->is_builtin_va_list = FALSE;
  pte->is_builtin_va_list_from_cstdarg = FALSE;
#ifdef GUARD_MACRO_FOR_VA_LIST
  pte->va_list_guard_macro_was_defined = FALSE;
#endif /* ifdef GUARD_MACRO_FOR_VA_LIST */
#ifdef GUARD_MACRO2_FOR_VA_LIST
  pte->va_list_guard_macro2_was_defined = FALSE;
#endif /* ifdef GUARD_MACRO2_FOR_VA_LIST */
#if DO_IL_LOWERING
#if ENSURE_LOWERED_TYPE_LIST_ORDERING
  pte->type_processed_for_ordering = FALSE;
  pte->type_processed_as_complete_for_ordering = FALSE;
#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING */
#if LOWER_VARIABLE_LENGTH_ARRAYS
  pte->visited_for_vla_lowering = FALSE;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  pte->typeinfo_var = NULL;
#endif /* DO_IL_LOWERING */
  set_type_kind(pte, kind);
}  /* clear_type */


a_type_ptr alloc_type(a_type_kind kind)
/*
Allocate a new type entry in the file scope memory region and return a pointer
to it.  Set general fields, set kind to the indicated value, and set the
associated variant fields to default values.
*/
{
  a_type_ptr tp;

  db_enter(5, "alloc_type");
  tp = (a_type_ptr)alloc_il(sizeof(a_type));
#if DEBUG
  num_types_allocated++;
#endif /* DEBUG */
  clear_type(tp, kind);
  db_exit();
  return tp;
}  /* alloc_type */


void set_dynamic_init_kind(a_dynamic_init_ptr  dip,
                           a_dynamic_init_kind kind)
/*
Set the kind of the indicated dynamic initialization entry to "kind", and set
the associated variant fields to default values.
*/
{
  dip->kind = kind;
  switch (kind) {
    case dik_none:
    case dik_zero:
    case dik_bitwise_copy:
      break;
    case dik_constant:
    case dik_nonconstant_aggregate:
      dip->variant.constant = NULL;
      break;
    case dik_expression:
    case dik_call_returning_class_via_cctor:
      dip->variant.expression = NULL;
      break;
    case dik_constructor:
      dip->variant.constructor.ptr = NULL;
      dip->variant.constructor.args = NULL;
      dip->variant.constructor.is_copy_constructor_with_implied_source = FALSE;
      dip->variant.constructor.is_implicit_copy_for_copy_initialization= FALSE;
      dip->variant.constructor.value_initialization = FALSE;
      dip->variant.constructor.has_sequenced_arguments = FALSE;
#if CENTERLINE_CHECKING
      dip->variant.constructor.avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
      break;
#if CHECKING
    default:
      internal_error("set_dynamic_init_kind: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* set_dynamic_init_kind */


static void clear_dynamic_init(a_dynamic_init_ptr  dip,
                               a_dynamic_init_kind kind)
/*
Initialize a dynamic_init entry of the kind specified.
*/
{
  dip->next                          = NULL;
  dip->variable                      = NULL;
  dip->destructor                    = NULL;
  dip->lifetime                      = NULL;
  dip->next_in_destruction_list      = NULL;
  dip->init_expr_lifetime            = NULL;
  dip->static_temp                   = FALSE;
  dip->follows_an_exec_statement     = FALSE;
  dip->inside_conditional_expression = FALSE;
  dip->unordered                     = FALSE;
  dip->has_temporary_lifetime        = FALSE;
  dip->is_constructor_init           = FALSE;
  dip->is_freeing_of_storage_on_exception = FALSE;
  dip->is_array_freeing              = FALSE;
  dip->destruction_is_for_partially_constructed_aggregate = FALSE;
#if DO_IL_LOWERING
  dip->is_guard_var_for_local_static_var_init = FALSE;
#endif /* DO_IL_LOWERING */
  dip->overlaps_temps_in_inner_lifetime = FALSE;
#if DO_IL_LOWERING && MULTIPLE_INIT_ROUTINES
  dip->included_in_slice = FALSE;
#endif /* DO_IL_LOWERING && MULTIPLE_INIT_ROUTINES */
  dip->is_explicit_cast = FALSE;
  dip->is_compound_literal = FALSE;
  dip->is_braced_initializer = FALSE;
  dip->is_partially_initialized = FALSE;
  dip->is_result_for_class_rvalue_question_mark = FALSE;
  dip->is_optimized_class_rvalue_question_mark = FALSE;
  dip->is_reused_value = FALSE;
#if DO_IL_LOWERING
  dip->is_vla_deallocation = FALSE;
#if GENERATE_EH_TABLES
  dip->is_freeing_of_exception_object = FALSE;
#endif /* GENERATE_EH_TABLES */
#endif /* DO_IL_LOWERING */
  dip->is_creation_of_initializer_list_object = FALSE;
#if CENTERLINE_CHECKING
  dip->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
  set_dynamic_init_kind(dip, kind);
#if DO_IL_LOWERING
  dip->destructible_entity_descr     = NULL;
  dip->init_destination              = NULL;
  dip->assoc_new                     = NULL;
#endif /* DO_IL_LOWERING */
  dip->lifetime_of_overlapping_temps = NULL;
  dip->master_entry                  = NULL;
  dip->rescan_info                   = NULL;
}  /* clear_dynamic_init */


a_dynamic_init_ptr alloc_dynamic_init(a_dynamic_init_kind kind)
/*
Allocate a dynamic initialization entry, clear it to default values, set
its kind to kind, and return a pointer to it.
*/
{
  a_dynamic_init_ptr dip;

  db_enter(5, "alloc_dynamic_init");

  dip = (a_dynamic_init_ptr)alloc_cil(sizeof(a_dynamic_init));
#if DEBUG
  num_dynamic_inits_allocated++;
#endif /* DEBUG */
  clear_dynamic_init(dip, kind);

  db_exit();
  return dip;
}  /* alloc_dynamic_init */


a_local_static_variable_init_ptr alloc_local_static_variable_init(void)
/*
Allocate a_local_static_variable_init entry, initialize its fields, and
return a pointer to it.
*/
{
  a_local_static_variable_init_ptr lsvip;

  db_enter(5, "alloc_local_static_variable_init");
  lsvip = (a_local_static_variable_init_ptr)alloc_cil(
                                        sizeof(a_local_static_variable_init));
#if DEBUG
  num_local_static_variable_inits_allocated++;
#endif /* DEBUG */
  lsvip->next = NULL;
  lsvip->variable = NULL;
  lsvip->init_kind = (an_init_kind)initk_none;
  lsvip->lifetime = NULL;
  db_exit();
  return lsvip;
}  /* alloc_local_static_variable_init */


a_vla_dimension_ptr alloc_vla_dimension(void)
/*
Allocate a_vla_dimension entry, initialize its fields, and return a
pointer to it.
*/
{
  a_vla_dimension_ptr vdp;

  db_enter(5, "alloc_vla_dimension");
  vdp = (a_vla_dimension_ptr)alloc_cil(sizeof(a_vla_dimension));
#if DEBUG
  num_vla_dimensions_allocated++;
#endif /* DEBUG */
  vdp->next = NULL;
  vdp->type = NULL;
  vdp->dimension_expr = NULL;
  vdp->original_dimension = NULL;
  vdp->in_prototype_scope = FALSE;
  vdp->position = null_source_position;
#if DO_IL_LOWERING
#if LOWER_VARIABLE_LENGTH_ARRAYS
  vdp->total_number_of_elements = NULL;
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
  vdp->dimension_variable = NULL;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#endif /* DO_IL_LOWERING */
  db_exit();
  return vdp;
}  /* alloc_vla_dimension */


static void clear_variable(a_variable_ptr vp)
/*
Clear the fields of the given variable to default values.
*/
{
  set_default_source_corresp(vp->source_corresp);
  vp->next                        = NULL;
  vp->type                        = NULL;
  vp->assoc_param_type            = NULL;
  vp->storage_class               = (a_storage_class)sc_unspecified;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  vp->declared_storage_class      = (a_storage_class)sc_unspecified;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DECL_MODIFIERS_IN_USE
  vp->decl_modifiers              = DM_NONE;
#endif /* DECL_MODIFIERS_IN_USE */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED || \
    NAMED_REGISTERS_ALLOWED
  vp->asm_name_or_reg.name        = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED || ... */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED || \
    USER_CONTROL_OF_STRUCT_PACKING
  vp->alignment                   = 0;
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED || ... */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  vp->init_priority               = 0;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  vp->cleanup_routine             = NULL;
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  vp->ELF_visibility              = (an_ELF_visibility_kind)evk_unspecified;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  vp->is_weak                     = FALSE;
  vp->is_weakref                  = FALSE;
  vp->has_gnu_unused_attribute    = FALSE;
  vp->has_gnu_used_attribute      = FALSE;
  vp->is_not_common               = FALSE;
  vp->is_common                   = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  vp->asm_name_is_valid           = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if NAMED_REGISTERS_ALLOWED
  vp->has_named_register_storage_class = FALSE;
#endif /* NAMED_REGISTERS_ALLOWED */
  vp->address_taken               = FALSE;
  vp->is_parameter                = FALSE;
  vp->declared_using_type_without_linkage
                                  = FALSE;
  vp->is_parameter_pack           = FALSE;
  vp->is_pack_element             = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  vp->is_initonly                 = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  vp->is_enhanced_for_iterator    = FALSE;
  vp->initializer_in_class        = FALSE;
  vp->init_kind                   = (an_init_kind)initk_none;
  /* One of the variant fields, chosen arbitrarily, is initialized. */
  vp->initializer.constant        = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  vp->initializer_range           = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  vp->entities_defined_in_initializer = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  vp->property_or_event_descr     = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  vp->assoc_template              = NULL;
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  vp->section                     = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  vp->aliased_variable            = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && IA64_ABI
  vp->comdat_group                = NULL;
#endif /* DO_IL_LOWERING && IA64_ABI */
#if DO_IL_LOWERING
  vp->vla_element_count_variable  = NULL;
#endif /* DO_IL_LOWERING */
  vp->referenced_non_locally      = FALSE;
  vp->modified_within_try_block   = FALSE;
  vp->is_template_static_data_member
                                  = FALSE;
  vp->is_specialized              = FALSE;
  vp->specialized_with_old_syntax = FALSE;
  vp->explicit_instantiation      = FALSE;
  vp->class_explicitly_instantiated = FALSE;
  vp->explicit_do_not_instantiate = FALSE;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  vp->can_be_instantiated         = FALSE;
  vp->do_not_instantiate          = FALSE;
  vp->instance_required           = FALSE;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  vp->param_value_has_been_changed= FALSE;
#if MINIMAL_INLINING
  vp->param_used_as_lvalue        = FALSE;
#endif /* MINIMAL_INLINING */
  vp->param_used_more_than_once   = FALSE;
  vp->is_handler_param            = FALSE;
  vp->is_this_parameter           = FALSE;
  vp->is_anonymous_parent_object  = FALSE;
  vp->is_member_constant          = FALSE;
  vp->superseded_external         = FALSE;
  vp->has_variably_modified_type  = FALSE;
  vp->is_vla                      = FALSE;
#if DO_IL_LOWERING
  vp->initialization_rewritten_as_assignment = FALSE;
#if MINIMAL_INLINING
  vp->is_temp_for_unmodified_inlined_param = FALSE;
  vp->is_temp_for_constructor_this_inlined_param = FALSE;
#endif /* MINIMAL INLINING */
  vp->promoted_local_static_init  = FALSE;
  vp->promoted_local_static       = FALSE;
  vp->is_optional_vtable          = FALSE;
  vp->lowering_generated          = FALSE;
#endif /* DO_IL_LOWERING */
  vp->is_compound_literal         = FALSE;
  vp->has_parenthesized_initializer = FALSE;
  vp->has_direct_braced_initializer = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
  vp->has_flexible_array_initializer = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
  vp->declared_with_auto_type_specifier = FALSE;
#if BACK_END_IS_CP_GEN_BE
  vp->definition_has_been_put_out = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  vp->embedded_source_sequence_entries = FALSE;
  vp->declared_type               = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  vp->instantiation_needed_bit_number = 0;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if MINIMAL_INLINING
  vp->remapping_for_inlining      = NULL;
#endif /* MINIMAL_INLINING */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  vp->dynamic_init_routine        = NULL;
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
}  /* clear_variable */


a_variable_ptr alloc_variable(a_storage_class  storage_class)
/*
Allocate a variable entry, clear it to default values, and return a pointer
to it.
*/
{
  a_variable_ptr vp;

  db_enter(5, "alloc_variable");

  if (storage_class == (a_storage_class)sc_extern ||
      storage_class == (a_storage_class)sc_unspecified ||
      storage_class == (a_storage_class)sc_static) {
    /* Variable that will have static storage should always be allocated in
       the file scope memory region. */
    vp = (a_variable_ptr)alloc_il(sizeof(a_variable));
  } else {
    vp = (a_variable_ptr)alloc_cil(sizeof(a_variable));
  }  /* if */
#if DEBUG
  num_variables_allocated++;
#endif /* DEBUG */
  clear_variable(vp);
  vp->storage_class = storage_class;
  db_exit();
  return vp;
}  /* alloc_variable */


a_field_ptr alloc_field(void)
/*
Allocate a field entry, clear it to default values, and return a pointer
to it.
*/
{
  a_field_ptr fp;

  db_enter(5, "alloc_field");

  fp = (a_field_ptr)alloc_il(sizeof(a_field));
#if DEBUG
  num_fields_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(fp->source_corresp);
  fp->next                 = NULL;
  fp->type                 = NULL;
  fp->offset               = 0;
  fp->offset_bit_remainder = 0;
  fp->bit_size             = 0;
#if USER_CONTROL_OF_STRUCT_PACKING
  fp->alignment            = 0;
#if GNU_EXTENSIONS_ALLOWED
  fp->is_packed            = 0;
#endif /* GNU_EXTENSIONS_ALLOWED */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if IA64_ABI
  fp->offset_is_set        = FALSE;
#endif /* IA64_ABI */
  fp->is_bit_field         = FALSE;
  fp->bit_field_is_signed  = FALSE;
  fp->is_anonymous_parent_object = FALSE;
  fp->is_mutable           = FALSE;
  fp->compiler_generated   = FALSE;
  fp->is_captured_this     = FALSE;
  fp->is_captured_pack_element = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  fp->is_initonly          = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  fp->vla_treated_as_zero_length_array = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  fp->base_class_subobject_with_tail_padding = FALSE;
#endif /* DO_IL_LOWERING */
#if CENTERLINE_CHECKING
  fp->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
  fp->bit_size_constant    = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  fp->property_or_event_descr = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  fp->declared_bit_size        = 0;
#if BACK_END_IS_C_GEN_BE
  fp->bit_field_alignment_type = NULL;
#endif /* BACK_END_IS_C_GEN_BE */

  db_exit();
  return fp;
}  /* alloc_field */


an_exception_specification_ptr alloc_exception_specification(void)
/*
Allocate an exception specification entry, clear it to default values, and
return a pointer to it.  The entry is allocated in the file scope memory
region.
*/
{
  an_exception_specification_ptr  esp;

  esp = (an_exception_specification_ptr)alloc_il(
                                          sizeof(an_exception_specification));
#if DEBUG
  num_exception_specifications_allocated++;
#endif /* DEBUG */
  esp->is_noexcept = FALSE;
  esp->throw_any = FALSE;
  esp->compiler_generated = FALSE;
  esp->arg_cached = FALSE;
  esp->variant.exception_specification_type_list = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  esp->source_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return esp;
}  /* alloc_exception_specification */


an_exception_specification_type_ptr alloc_exception_specification_type(void)
/*
Allocate an exception specification type entry, clear it to default values,
and return a pointer to it.  The entry is allocated in the file scope memory
region.
*/
{
  an_exception_specification_type_ptr  estp;

  estp = (an_exception_specification_type_ptr)alloc_il(
                                   sizeof(an_exception_specification_type));
#if DEBUG
  num_exception_specification_types_allocated++;
#endif /* DEBUG */
  estp->next = NULL;
  estp->type = NULL;
  estp->redundant = FALSE;
  estp->is_pack_expansion = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  estp->source_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  return estp;
}  /* alloc_exception_specification_type */


void set_routine_special_kind(a_routine_ptr           rp,
                              a_special_function_kind special_kind)
/*
Set the special_kind field of the indicated routine to the indicated
value.  Also clear related variant fields to default values.
*/
{
  rp->special_kind = special_kind;
  switch (special_kind) {
    case sfk_conversion:
      break;
    case sfk_operator:
      rp->variant.opname_kind = (an_opname_kind)onk_none;
      break;
    case sfk_none:
#if GNU_EXTENSIONS_ALLOWED
      rp->variant.builtin_function_kind = (a_builtin_function_kind)bfk_none;
#endif /* GNU_EXTENSIONS_ALLOWED */
      break;
    case sfk_constructor:
    case sfk_destructor:
#if IA64_ABI && DO_IL_LOWERING
      rp->variant.ctor_dtor.alternate_entry_points = 
                                                (a_routine_list_entry_ptr)NULL;
      rp->variant.ctor_dtor.base_name_offset = 0;
#endif /* IA64_ABI && DO_IL_LOWERING */
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sfk_static_constructor:
    case sfk_finalizer:
    case sfk_idisposable_dispose:
    case sfk_dispose_bool:
    case sfk_object_finalize:
      check_assertion(cppcli_enabled);
      break;
    case sfk_property_get:
    case sfk_property_set:
    case sfk_event_add:
    case sfk_event_remove:
    case sfk_event_raise:
      rp->variant.property_or_event_descr = NULL;
      check_assertion(cppcli_enabled);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case sfk_lambda_entry_point:
      rp->variant.lambda_call_operator = NULL;
      break;
    default:
      unexpected_condition_str("set_routine_special_kind: bad kind");
  }  /* switch */
}  /* set_routine_special_kind */


a_routine_ptr alloc_routine(void)
/*
Allocate a routine entry, clear it to default values, and return a pointer
to it.  The entry is allocated in the file scope memory region.
*/
{
  a_routine_ptr rp;

  db_enter(5, "alloc_routine");

  rp = (a_routine_ptr)alloc_il(sizeof(a_routine));
#if DEBUG
  num_routines_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(rp->source_corresp);
  rp->next                        = NULL;
  rp->type                        = NULL;
  rp->assoc_scope                 = NULL_region_number;
  rp->storage_class               = (a_storage_class)sc_unspecified;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  rp->declared_storage_class      = (a_storage_class)sc_unspecified;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  set_routine_special_kind(rp, (a_special_function_kind)sfk_none);
  rp->address_taken               = FALSE;
  rp->is_virtual                  = FALSE;
  rp->overrides_base_member       = FALSE;
  rp->pure_virtual                = FALSE;
  rp->final                       = FALSE;
  rp->override                    = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  rp->abstract                    = FALSE;
  rp->sealed                      = FALSE;
  rp->new_member                  = FALSE;
  rp->interface_slot              = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  rp->covariant_return_virtual_override
                                  = FALSE;
  rp->is_inline                   = FALSE;
  rp->compiler_generated          = FALSE;
  rp->defined                     = FALSE;
  rp->called                      = FALSE;
  rp->is_explicit_constructor     = FALSE;
  rp->is_explicit_conversion_function = FALSE;
  rp->is_trivial_default_constructor = FALSE;
  rp->is_trivial_copy_function    = FALSE;
  rp->is_initializer_list_ctor    = FALSE;
#if ASSIGNMENT_TO_THIS_ALLOWED
  rp->assignment_to_this_done     = FALSE;
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  rp->is_template_function        = FALSE;
  rp->is_specialized              = FALSE;
  rp->specialized_with_old_syntax = FALSE;
  rp->is_prototype_instantiation  = FALSE;
  rp->explicit_instantiation      = FALSE;
  rp->class_explicitly_instantiated = FALSE;
  rp->explicit_do_not_instantiate = FALSE;
  rp->never_throws                = FALSE;
  rp->is_in_class_specialization  = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  rp->declared_only_as_friend     = FALSE;
  rp->explicit_extern_inline      = FALSE;
  rp->direct_linkage_specifier_on_nondef_decl = FALSE;
  rp->is_reverse_conversion_function
                                  = FALSE;
  rp->is_generic_definition       = FALSE;
  rp->is_generic_instance         = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  rp->ELF_visibility              = (an_ELF_visibility_kind)evk_unspecified;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  rp->is_initialization_routine   = FALSE;
  rp->is_finalization_routine     = FALSE;
  rp->is_pure                     = FALSE;
  rp->is_weak                     = FALSE;
  rp->is_weakref                  = FALSE;
  rp->has_gnu_unused_attribute    = FALSE;
  rp->has_gnu_used_attribute      = FALSE;
  rp->allocates_memory            = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  rp->never_inline                = FALSE;
#if GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  rp->is_naked                    = FALSE;
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  rp->no_instrument_function      = FALSE;
  rp->no_check_memory_usage       = FALSE;
  rp->always_inline               = FALSE;
  rp->gnu_c89_inline              = FALSE;
  rp->implicit_alias              = FALSE;
#if GNU_COMPLEX_EXTENSIONS_ALLOWED && LOWER_COMPLEX && BACK_END_IS_C_GEN_BE
  rp->builtin_using_complex_type  = FALSE;
#endif /* GNU_COMPLEX_EXTENSIONS_ALLOWED && ... */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  rp->can_be_instantiated         = FALSE;
  rp->do_not_instantiate          = FALSE;
  rp->instance_required           = FALSE;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  rp->contains_try_block          = FALSE;
  rp->contains_local_class_type   = FALSE;
  rp->superseded_external         = FALSE;
  rp->defined_in_friend_decl      = FALSE;
  rp->defined_outside_of_parent   = FALSE;
#if MINIMAL_INLINING
  rp->inlinable                   = FALSE;
#endif /* MINIMAL_INLINING */
#if MAINTAIN_NEEDED_FLAGS
  rp->definition_needed           = FALSE;
  rp->keep_definition_in_il       = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  rp->expl_template_arg_list_used = FALSE;
#if BACK_END_IS_CP_GEN_BE
  rp->surrounding_name_linkage_state
                                  = (a_name_linkage_kind)nlk_none;
  rp->definition_C_name_linkage_specified
                                  = FALSE;
  rp->definition_has_direct_linkage_specifier
                                  = FALSE;
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  rp->has_been_defined            = FALSE;
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* BACK_END_IS_CP_GEN_BE */
  rp->definition_for_inlining_only = FALSE;
#if INSTANTIATE_EXTERN_INLINE
  rp->inline_instance_required    = FALSE;
#endif /* INSTANTIATE_EXTERN_INLINE */
  rp->suppress_inline_body        = FALSE;
  rp->on_inline_function_list     = FALSE;
  rp->need_out_of_line_copy       = FALSE;
  rp->fp_contract                 = (a_stdc_pragma_value)stdc_pv_none;
  rp->fenv_access                 = (a_stdc_pragma_value)stdc_pv_none;
  rp->cx_limited_range            = (a_stdc_pragma_value)stdc_pv_none;
#if FIXED_POINT_ALLOWED
  rp->fx_full_precision           = (a_stdc_pragma_value)stdc_pv_none;
  rp->fx_fract_overflow           = (a_stdc_pragma_value)stdc_pv_none;
  rp->fx_accum_overflow           = (a_stdc_pragma_value)stdc_pv_none;
#endif /* FIXED_POINT_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  rp->upc_access_method = (a_upc_access_method)upc_access_unspecified;
#endif /* UPC_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  rp->contains_statement_expression = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if IA64_ABI
  rp->inline_in_class_definition  = FALSE;
#endif /* IA64_ABI */
#if DO_IL_LOWERING && IA64_ABI
  rp->use_comdat                  = FALSE;
  rp->ctor_dtor_kind              = (a_ctor_or_dtor_kind)cdk_none;
  rp->is_alias_entry              = FALSE;
#endif /* DO_IL_LOWERING && IA64_ABI */
#if DO_IL_LOWERING
  rp->lowering_delayed_on_nested_function = FALSE;
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
  rp->has_no_effect               = FALSE;
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
#endif /* DO_IL_LOWERING */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
  rp->statics_have_been_promoted  = FALSE;
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
  rp->is_lambda_body              = FALSE;
  rp->declared_using_type_without_linkage
                                  = FALSE;
  rp->is_defaulted                = FALSE;
  rp->is_deleted                  = FALSE;
  rp->contains_local_static_variable = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  rp->embedded_source_sequence_entries = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if CENTERLINE_CHECKING
  rp->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
#if DECL_MODIFIERS_IN_USE
  rp->decl_modifiers              = DM_NONE;
#endif /* DECL_MODIFIERS_IN_USE */
  rp->virtual_function_number     = VIRTUAL_FUNCTION_NUMBER_NONE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  rp->overridden_functions        = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  rp->befriending_classes         = NULL;
  rp->template_arg_list           = NULL;
  rp->assoc_template              = NULL;
#if GNU_EXTENSIONS_ALLOWED
  rp->section                     = NULL;
  rp->aliased_routine             = NULL;
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  rp->ctor_priority               = 0;
  rp->dtor_priority               = 0;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  rp->asm_name                    = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  rp->declared_type               = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  rp->overriding_function_for_wrapper = NULL;
  rp->overridden_function_for_wrapper = NULL;
#if IA64_ABI
  rp->delta                       = 0;
  rp->vcall_index                 = 0;
  rp->return_delta                = 0;
  rp->vbase_index                 = 0;
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if DO_IL_LOWERING && IA64_ABI
  rp->primary_ctor_or_dtor        = NULL;
#endif /* DO_IL_LOWERING && IA64_ABI */
#if ONE_INSTANTIATION_PER_OBJECT
  rp->instantiation_needed_bit_number = 0;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  rp->routine_fixup = NULL;
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED && DO_IL_LOWERING
  rp->init_priority               = 0;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED && DO_IL_LOWERING */
  db_exit();
  return rp;
}  /* alloc_routine */


an_asm_entry_ptr alloc_asm_entry(void)
/*
Allocate an asm entry, clear it to default values, and return a pointer
to it.
*/
{
  an_asm_entry_ptr ap;

  db_enter(5, "alloc_asm_entry");
  ap = (an_asm_entry_ptr)alloc_cil(sizeof(an_asm_entry));
#if DEBUG
  num_asm_entries_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(ap->source_corresp);
  ap->next = NULL;
  ap->asm_string = NULL;
#if GNU_EXTENSIONS_ALLOWED
  ap->gnu_asm_form = FALSE;
  ap->is_volatile = FALSE;
  ap->operands = NULL;
  ap->clobbers = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED */
  db_exit();
  return ap;
}  /* alloc_asm_entry */

#if ASM_SUPPORT_NEEDED

char *alloc_asm_function_body(sizeof_t  len)
/*
Allocate space for an asm function body and return a pointer to it.
*/
{
#if DEBUG
  asm_function_body_space_allocated += (unsigned long)len;
#endif /* DEBUG */
  return (char *)alloc_cil(len);
}  /* alloc_asm_function_body */

#endif /* ASM_SUPPORT_NEEDED */

#if GNU_EXTENSIONS_ALLOWED
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS

an_asm_operand_constraint_ptr alloc_asm_operand_constraint(
                                            an_asm_operand_constraint_kind ck)
/*
Allocate space for an asm operand constraint and return a pointer to it.
*/
{
  an_asm_operand_constraint_ptr aocp;

  aocp = (an_asm_operand_constraint_ptr)
                                 alloc_cil(sizeof(an_asm_operand_constraint));
  aocp->kind = ck;
  aocp->next = NULL;

  return aocp;
}  /* alloc_asm_operand_constraint */

#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

an_asm_operand_ptr alloc_asm_operand(void)
/*
Allocate space for an asm operand and return a pointer to it.
*/
{
  an_asm_operand_ptr  aop = (an_asm_operand_ptr)
                                            alloc_cil(sizeof(an_asm_operand));

  aop->next = NULL;
  aop->name = NULL;
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  aop->is_output_operand = FALSE;
  aop->constraints_string = NULL;
#else /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  aop->constraints = NULL;
  aop->modifiers = (an_asm_operand_modifier)aom_invalid;
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  aop->position = null_source_position;
  aop->expression = NULL;
  return aop;
}  /* alloc_asm_operand */


a_named_register_list_ptr alloc_named_register_list(void)
/*
Allocate space for an a named register list and return a pointer to
it.
*/
{
  a_named_register_list_ptr  nrl = (a_named_register_list_ptr)
                                     alloc_cil(sizeof(a_named_register_list));

  nrl->next = NULL;
  nrl->reg = (a_named_register)anr_invalid;
  return nrl;
}  /* alloc_named_register_list */

#endif /* GNU_EXTENSIONS_ALLOWED */

a_label_ptr alloc_label(void)
/*
Allocate a label entry, clear it to default values, and return a pointer
to it.
*/
{
  a_label_ptr lp;

  db_enter(5, "alloc_label");

  /* Labels should always be allocated in a function scope memory region. */
  check_assertion(curr_il_region_number != file_scope_region_number);
  lp = (a_label_ptr)alloc_cil(sizeof(a_label));
#if DEBUG
  num_labels_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(lp->source_corresp);
  lp->source_corresp.is_local_to_function = TRUE;
  lp->next = NULL;
  lp->reachable_by_fall_through = TRUE;
  lp->break_label = FALSE;
  lp->switch_break_label = FALSE;
  lp->continue_label = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  lp->leave_label = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  lp->address_taken = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  lp->locally_declared = FALSE;
  lp->has_gnu_unused_attribute = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if CENTERLINE_CHECKING
  lp->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
  lp->exec_stmt = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  lp->num_microsoft_trys_inside_of = 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_exit();
  return lp;
}  /* alloc_label */


a_local_expr_node_ref_ptr alloc_local_expr_node_ref(void)
/*
Allocate an entry to represent an outside reference to a local expression
node, initialize it, and return a pointer to it.
*/
{
  a_local_expr_node_ref_ptr  ptr;

  ptr = (a_local_expr_node_ref_ptr)alloc_cil(sizeof(a_local_expr_node_ref));
  ptr->next = NULL;
  ptr->expr = NULL;
  ptr->kind = (a_local_expr_node_ref_kind)lerk_none;
  clear_tagged_ptr(ptr->referrer);
  return ptr;
}  /* alloc_local_expr_node_ref */


void set_expr_node_kind(an_expr_node_ptr  node,
                        an_expr_node_kind kind)
/*
Set the kind of the indicated expression node.  Also set associated variant
fields to default values.
*/
{
  a_new_delete_supplement_ptr ndsp;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_gcnew_supplement_ptr      gnsp;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_throw_supplement_ptr      tsp;
  a_condition_supplement_ptr  csp;

  node->kind = kind;
  switch (kind) {
    case enk_error:
    case enk_address_of_ellipsis:
      /* No variant fields. */
      break;
    case enk_operation:
      node->variant.operation.kind = (an_expr_operator_kind)eok_last;
      node->variant.operation.type_kind = (a_type_kind)tk_unknown;
      node->variant.operation.returns_lvalue_instead_of_usual_rvalue = FALSE;
      node->variant.operation.compiler_generated = FALSE;
      node->variant.operation.is_reinterpret_cast = FALSE;
      node->variant.operation.is_const_cast = FALSE;
      node->variant.operation.is_reference_cast = FALSE;
      node->variant.operation.is_rvalue_reference_cast = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      node->variant.operation.is_tracking_reference_cast = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      node->variant.operation.implicit_in_member_naming = FALSE;
      node->variant.operation.implicit_step_of_explicit_cast = FALSE;
      node->variant.operation.is_conversion_call = FALSE;
      node->variant.operation.arg_dependent_lookup_suppressed_on_call = FALSE;
      node->variant.operation.call_with_qualified_function_name = FALSE;
#if BACK_END_IS_CP_GEN_BE
      node->variant.operation.only_found_through_arg_dependent_lookup = FALSE;
      node->variant.operation.keep_cast_for_cp_gen_be = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
      node->variant.operation.call_uses_operator_syntax = FALSE;
#if GNU_EXTENSIONS_ALLOWED
      node->variant.operation.is_gnu_two_operand_question_mark = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
      node->variant.operation.pointer_operand_is_second = FALSE;
      node->variant.operation.is_virtual_call = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      node->variant.operation.rewritten_property_reference_kind =
                                (a_rewritten_property_reference_kind)rprk_none;
      node->variant.operation.requires_runtime_cast_check = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      node->variant.operation.operands = NULL;
      break;
    case enk_constant:
      node->variant.constant = NULL;
      break;
    case enk_variable:
      node->variant.variable = NULL;
      break;
    case enk_routine:
      node->variant.routine.ptr = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING
      node->variant.routine.property_or_event_descr = NULL;
      node->variant.routine.special_kind = (a_special_function_kind)sfk_none;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING */
      break;
    case enk_field:
      node->variant.field = NULL;
      break;
    case enk_temp_init:
      node->variant.init.dynamic_init = NULL;
      break;
    case enk_new_delete:
      /* Allocate the supplement for new/delete. */
      ndsp = (a_new_delete_supplement_ptr)
                                    alloc_cil(sizeof(a_new_delete_supplement));
      node->variant.new_delete = ndsp;
#if DEBUG
      num_new_delete_supplements_allocated++;
#endif /* DEBUG */
      ndsp->is_new                          = TRUE;
      ndsp->placement_new                   = FALSE;
      ndsp->array_delete                    = FALSE;
      ndsp->global_new_or_delete            = FALSE;
      ndsp->has_new_initializer             = FALSE;
      ndsp->new_initializer_is_brace_enclosed = FALSE;
      ndsp->type_contains_auto_specifier    = FALSE;
      ndsp->type                            = NULL;
      ndsp->routine                         = NULL;
      ndsp->arg                             = NULL;
      ndsp->dynamic_init                    = NULL;
      ndsp->freeing_of_storage_on_exception = NULL;
      ndsp->number_of_elements              = NULL;
      break;
    case enk_lambda:
      node->variant.lambda.ptr            = NULL;
      node->variant.lambda.initialization = NULL;
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case enk_gcnew:
      gnsp = (a_gcnew_supplement_ptr)alloc_cil(sizeof(a_gcnew_supplement));
      node->variant.gcnew_info = gnsp;
#if DEBUG
      num_gcnew_supplements_allocated++;
#endif /* DEBUG */
      gnsp->has_new_initializer         = FALSE;
      gnsp->is_cli_array                = FALSE;
      gnsp->compiler_generated          = FALSE;
      gnsp->type                        = NULL;
      gnsp->cli_array_dimension_lengths = NULL;
      gnsp->dynamic_init                = NULL;
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case enk_throw:
      /* Allocate the supplement for a throw. */
      tsp = (a_throw_supplement_ptr)alloc_cil(sizeof(a_throw_supplement));
      node->variant.throw_info = tsp;
#if DEBUG
      num_throw_supplements_allocated++;
#endif /* DEBUG */
      tsp->type         = NULL;
      tsp->dynamic_init = NULL;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
      tsp->expr         = NULL;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if !ABI_CHANGES_FOR_RTTI
      tsp->accessible_base_classes = NULL;
#endif /* !ABI_CHANGES_FOR_RTTI */
      tsp->destructor   = NULL;
      break;
    case enk_condition:
      csp = (a_condition_supplement_ptr)
                                 alloc_cil(sizeof(a_condition_supplement));
      node->variant.condition = csp;
#if DEBUG
      num_condition_supplements_allocated++;
#endif /* DEBUG */
      csp->scope        = NULL;
      csp->dynamic_init = NULL;
      csp->expr         = NULL;
      break;
    case enk_object_lifetime:
      node->variant.object_lifetime.expr = NULL;
      node->variant.object_lifetime.ptr  = NULL;
      break;
    case enk_typeid:
      node->variant.typeid_info.type = NULL;
      node->variant.typeid_info.expr = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      node->variant.typeid_info.is_cli_typeid = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      break;
    case enk_sizeof:
    case enk_alignof:
      node->variant.sizeof_info.is_type = TRUE;
      node->variant.sizeof_info.variant.type = NULL;
      break;
    case enk_sizeof_pack:
      node->variant.sizeof_pack.is_type = TRUE;
      node->variant.sizeof_pack.is_template_template = FALSE;
      node->variant.sizeof_pack.variant.type = NULL;
      break;
#if GNU_EXTENSIONS_ALLOWED
    case enk_statement:
      node->variant.statement = NULL;
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    case enk_reuse_value:
      node->variant.reused_value_init = NULL;
      break;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    /* Nodes generated by IL lowering for partial lowering of exception
       handling features. */
    case enk_lowered_eh_construct:
      /* See set_lowered_eh_construct_node_kind. */
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
      /* Node generated as part of the body of an entry function used
         as a wrapper for a call of an overriding virtual function
         with a covariant return type. */
      /* No variant fields. */
      break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if VLA_DEALLOCATIONS_IN_IL
    case enk_vla_dealloc:
      node->variant.vla_variable = NULL;
      break;
#endif /* VLA_DEALLOCATIONS_IN_IL */
    case enk_type_operand:
      node->variant.type_operand.type = NULL;
      node->variant.type_operand.definition_needed = FALSE;
      break;
    case enk_builtin_operation:
      node->variant.builtin_operation.kind =
                                           (a_builtin_operation_kind)bok_last;
      node->variant.builtin_operation.operands = NULL;
      break;
    case enk_param_ref:
      node->variant.param_ref.param_num = 0;
      node->variant.param_ref.levels_up = 0;
      break;
    case enk_braced_init_list:
      node->variant.braced_init_list = NULL;
      break;
    default:
      unexpected_condition_str("set_expr_node_kind: bad kind");
  }  /* switch */
}  /* set_expr_node_kind */


void clear_expr_node(an_expr_node_ptr  node,
                     an_expr_node_kind kind)
/*
Set the fixed fields of the given expression node to default values, and
its kind to the indicated kind.
*/
{
  node->type = NULL;
  node->next = NULL;
  node->is_lvalue = FALSE;
  node->result_is_not_used = FALSE;
  node->is_initialization_guard = FALSE;
  node->generated_default_arg = FALSE;
#if GNU_EXTENSIONS_ALLOWED
  node->marked_as_gnu_extension = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  node->is_static_cast = FALSE;
  node->is_objectless_nonstatic_data_mem_ref = FALSE;
  node->is_pack_expansion = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  node->is_safe_cast = FALSE;
  node->element_of_cli_param_array_arg = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  node->is_non_normalized_boolean_controlling_expr = FALSE;
#endif /* DO_IL_LOWERING */
#if CENTERLINE_CHECKING
  node->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  node->expr_range = null_source_range; 
  node->operator_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  node->name_reference = NULL;
  node->rescan_info = NULL;
  set_expr_node_kind(node, kind);
}  /* clear_expr_node */


an_expr_node_ptr alloc_expr_node(an_expr_node_kind kind)
/*
Allocate and initialize an expression node.
*/
{
  register an_expr_node_ptr ptr;

  db_enter(5, "alloc_expr_node");

  ptr = (an_expr_node_ptr)alloc_cil(sizeof(an_expr_node));
#if DEBUG
  num_expr_nodes_allocated++;
#endif /* DEBUG */
  clear_expr_node(ptr, kind);

  db_exit();
  return ptr;
}  /* alloc_expr_node */

#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING

void set_lowered_eh_construct_node_kind(an_expr_node_ptr node,
                                        a_lowered_eh_construct_kind kind)
/*
node is an enk_lowered_eh_construct node, used to represent a (partially)
lowered exception handling construct after IL lowering.  Set its kind field
to kind, and set dependent variant fields to default values.
*/
{
  node->variant.lowered_eh.kind = kind;
  switch (kind) {
    case leck_caught_object_address:
      node->variant.lowered_eh.variant.caught_object_handler = NULL;
      break;
    case leck_thrown_object_address:
      /* No variant fields. */
      break;
    case leck_cleanup_state:
    case leck_unreachable_cleanup_state:
#if GENERATE_EH_TABLES
      node->variant.lowered_eh.variant.cleanup_region_number = 0;
#else /* !GENERATE_EH_TABLES */
      node->variant.lowered_eh.variant.cleanup_ptr = NULL;
#endif /* GENERATE_EH_TABLES */
      break;
    case leck_function_prologue:
      { an_eh_prologue_supplement_ptr psp = (an_eh_prologue_supplement_ptr)
                                  alloc_cil(sizeof(an_eh_prologue_supplement));
        node->variant.lowered_eh.variant.prologue_info = psp;
#if DEBUG
        num_eh_prologue_supplements_allocated++;
#endif /* DEBUG */
        psp->routine = NULL;
#if GENERATE_EH_TABLES
        psp->region_table = NULL;
        psp->array_table = NULL;
#endif /* GENERATE_EH_TABLES */
      }
      break;
    case leck_function_epilogue:
      node->variant.lowered_eh.variant.epilogue_routine = NULL;
      break;
    case leck_catch_epilogue:
      node->variant.lowered_eh.variant.epilogue_handler = NULL;
      break;
    case leck_try_epilogue:
      node->variant.lowered_eh.variant.epilogue_try_block = NULL;
      break;
    case leck_exception_caught:
    case leck_exception_started:
      /* No variant fields. */
      break;
#if !GENERATE_EH_TABLES
    case leck_initialization_completed:
      node->variant.lowered_eh.variant.dynamic_init = NULL;
      break;
#endif /* !GENERATE_EH_TABLES */
    case leck_internal_try:
      node->variant.lowered_eh.variant.internal_try.try_expr = NULL;
      node->variant.lowered_eh.variant.internal_try.catch_expr = NULL;
      break;
    default:
      unexpected_condition_str(
          "set_lowered_eh_construct_node_kind: bad lowered eh construct kind");
  }  /* switch */
}  /* set_lowered_eh_construct_node_kind */


an_expr_node_ptr alloc_lowered_eh_construct_node(
                                              a_lowered_eh_construct_kind kind)
/*
Allocate an expression node of kind enk_lowered_eh_construct, used to
represent a (partially) lowered exception handling construct.  Set its
kind field to kind, and set dependent variant fields to default values.
Return a pointer to the entry.
*/
{
  an_expr_node_ptr node =
                  alloc_expr_node((an_expr_node_kind)enk_lowered_eh_construct);

  /* For most kinds, void type is correct. */
  node->type = void_type();
  set_lowered_eh_construct_node_kind(node, kind);
  return node;
}  /* alloc_lowered_eh_construct_node */

#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */

static void clear_range_based_for_loop(a_range_based_for_loop_ptr rbflp)
/*
Clear the indicated range-based-for loop entry.
*/
{
  rbflp->iterator = NULL;
  rbflp->range = NULL;
  rbflp->range_based_for_scope = NULL;
  rbflp->begin_end_scope = NULL;
  rbflp->iterator_scope = NULL;
  rbflp->begin = NULL;
  rbflp->end = NULL;
  rbflp->ne_call_expr = NULL;
  rbflp->incr_call_expr = NULL;
}  /* clear_range_based_for_loop */

#if MICROSOFT_EXTENSIONS_ALLOWED

void set_for_each_loop_kind(a_for_each_loop_ptr     felp,
                            a_for_each_pattern_kind kind)
/*
Set the kind of the for-each loop to "kind", and set the associated variant
fields to default values.
*/
{
  felp->kind = kind;
  switch (kind) {
    case sfepk_none:
      break;
    case sfepk_stl_pattern:
    case sfepk_array_pattern:
      felp->variant.stl_array_pattern.end_variable = NULL;
      felp->variant.stl_array_pattern.ne_call_expr = NULL;
      felp->variant.stl_array_pattern.incr_call_expr = NULL;
      break;
    case sfepk_cli_pattern:
      felp->variant.cli_pattern.movenext_call_expression = NULL;
      break;
    case sfepk_cli_array_pattern:
      felp->variant.cli_array_pattern.upper_bound_vars = NULL;
      felp->variant.cli_array_pattern.loop_vars = NULL;
      break;
    default:
      unexpected_condition_str("set_for_each_loop_kind: bad kind");
  }  /* switch */
}  /* set_for_each_loop_kind */


static void clear_for_each_loop(a_for_each_loop_ptr     felp,
                                a_for_each_pattern_kind kind)
/*
Clear the indicated for-each loop entry, set the kind as given, and set the
associated variant fields to default values.
*/
{
  felp->uses_prev_decl_iterator = FALSE;
  /* Clear fields of inactive variant too for union-as-struct testing. */
  { felp->iterator.prev_decl.variable = NULL;
    felp->iterator.prev_decl.field = NULL;
    felp->iterator.prev_decl.assign_expr = NULL;
  }
  felp->iterator.variable = NULL;
  felp->collection_expr_ref = NULL;
  felp->for_each_scope = NULL;
  felp->iterator_scope = NULL;
  felp->temporary_variable = NULL;
  set_for_each_loop_kind(felp, kind);
}  /* clear_for_each_loop */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_switch_case_entry_ptr alloc_switch_case_entry(void)
/*
Allocate storage to describe an individual switch case, clear it to default
values, and return a pointer to it.
*/
{
  a_switch_case_entry_ptr  entry;

  entry = (a_switch_case_entry_ptr)alloc_cil(sizeof(a_switch_case_entry));
  entry->stmt = NULL;
  entry->case_value = NULL;
#if GNU_EXTENSIONS_ALLOWED
  entry->range_end = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED */
  entry->next = NULL;
  entry->next_on_sorted_list = NULL;
  entry->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  entry->end_position = null_source_position;
  entry->colon_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  entry->reachable_by_fall_through = TRUE;
#if CENTERLINE_CHECKING
  entry->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
#if DEBUG
  num_switch_case_entries_allocated++;
#endif /* DEBUG */
  return entry;
}  /* alloc_switch_case_entry */


static a_switch_stmt_descr_ptr alloc_switch_stmt_descr(void)
/*
Allocate storage to describe the details of a switch statement, clear it to
default values, and return a pointer to it.
*/
{
  a_switch_stmt_descr_ptr  descr;

  descr = (a_switch_stmt_descr_ptr)alloc_cil(sizeof(a_switch_stmt_descr));
  descr->cases = NULL;
  descr->default_case = NULL;
  descr->sorted_cases = NULL;
#if DEBUG
  num_switch_stmt_descriptions_allocated++;
#endif /* DEBUG */
  return descr;
}  /* alloc_switch_stmt_descr */

#if !ABI_CHANGES_FOR_RTTI

an_accessible_base_class_ptr alloc_accessible_base_class(a_base_class_ptr bcp)
/*
Allocate an accessible_base_class, clear it to default values and set the
base class to bcp, and return a pointer to it.
*/
{
  register an_accessible_base_class_ptr abcp;

  abcp = (an_accessible_base_class_ptr)alloc_cil(
                                            sizeof(an_accessible_base_class));
#if DEBUG
  num_accessible_base_classes_allocated++;
#endif /* DEBUG */
  abcp->next       = NULL;
  abcp->base_class = bcp;

  return abcp;
}  /* alloc_accessible_base_class */

#endif /* !ABI_CHANGES_FOR_RTTI */

a_handler_ptr alloc_handler(void)
/*
Allocate a handler, clear it to default values, and return a pointer to it.
*/
{
  register a_handler_ptr hp;

  hp = (a_handler_ptr)alloc_cil(sizeof(a_handler));
#if DEBUG
  num_handlers_allocated++;
#endif /* DEBUG */
  hp->next         = NULL;
  hp->parameter    = NULL;
  hp->statement    = NULL;
  hp->dynamic_init = NULL;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  hp->typeinfo_var = NULL;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
  clear_stmt_source_position(hp->catch_position);

  return hp;
}  /* alloc_handler */


void set_statement_kind(a_statement_ptr  sp,
                        a_statement_kind stmt_kind)
/*
Set the kind of the statement sp to stmt_kind, and set the associated variant
fields to default values.
*/
{
  a_block_ptr          bp;
  a_for_loop_ptr       flip;
  a_range_based_for_loop_ptr
                       rbflp;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_for_each_loop_ptr  felp;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_try_supplement_ptr tsp;

  sp->kind = stmt_kind;
  sp->expr = NULL;
  switch (stmt_kind) {
    case stmk_empty:
    case stmk_expr:
#if GNU_EXTENSIONS_ALLOWED
    case stmk_assigned_goto:
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
    case stmk_upc_notify:
    case stmk_upc_wait:
    case stmk_upc_barrier:
    case stmk_upc_fence:
#endif /* UPC_EXTENSIONS_ALLOWED */
      /* No variant fields. */
      break;
    case stmk_if:
      sp->variant.if_stmt.then_statement =
          sp->variant.if_stmt.else_statement = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      clear_stmt_source_position(sp->variant.if_stmt.else_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      break;
    case stmk_while:
    case stmk_end_test_while:
      sp->variant.loop_statement = NULL;
      break;
#if UPC_EXTENSIONS_ALLOWED
    /* The UPC forall statement is handled like the normal for statement. */
    case stmk_upc_forall:
#endif /* UPC_EXTENSIONS_ALLOWED */
    case stmk_for:
      sp->variant.for_loop.statement = NULL;
      sp->variant.for_loop.extra_info = flip = alloc_cil_of_type(a_for_loop);
#if DEBUG
      num_for_loops_allocated++;
#endif /* DEBUG */
      flip->initialization = NULL;
      flip->increment = NULL;
      flip->for_init_scope = NULL;
#if UPC_EXTENSIONS_ALLOWED
      flip->affinity = NULL;
#endif /* UPC_EXTENSIONS_ALLOWED */
      break;
    case stmk_range_based_for:
      sp->variant.range_based_for_loop.statement = NULL;
      sp->variant.range_based_for_loop.extra_info = rbflp =
                                     alloc_cil_of_type(a_range_based_for_loop);
      clear_range_based_for_loop(rbflp);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case stmk_for_each:
      sp->variant.for_each_loop.statement = NULL;
      sp->variant.for_each_loop.extra_info = felp =
                      (a_for_each_loop_ptr)alloc_cil(sizeof(a_for_each_loop));
      clear_for_each_loop(felp, (a_for_each_pattern_kind)sfepk_none);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case stmk_switch_case:
      sp->variant.switch_case.switch_statement = NULL;
      sp->variant.switch_case.extra_info       = NULL;
      break;
    case stmk_switch:
      sp->variant.switch_stmt.body_statement = NULL;
      sp->variant.switch_stmt.extra_info     = alloc_switch_stmt_descr();
      break;
    case stmk_goto:
    case stmk_label:
      sp->variant.label.ptr      = NULL;
      sp->variant.label.lifetime = NULL;
      break;
    case stmk_return:
      sp->variant.return_dynamic_init = NULL;
      break;
    case stmk_block:
      sp->variant.block.statements = NULL;
      sp->variant.block.extra_info = bp =
                        (a_block_ptr)alloc_cil(sizeof(a_block));
#if DEBUG
      num_blocks_allocated++;
#endif /* DEBUG */
      clear_stmt_source_position(bp->final_position);
      bp->assoc_scope            = NULL;
      bp->lifetime               = NULL;
      bp->end_of_block_reachable = TRUE;
#if GNU_EXTENSIONS_ALLOWED
      bp->is_statement_expression = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
      bp->implicit_scope_not_allowed = FALSE;
#if UPC_EXTENSIONS_ALLOWED
      bp->upc_access_method      = (a_upc_access_method)upc_access_unspecified;
#endif /* UPC_EXTENSIONS_ALLOWED */
      break;
    case stmk_init:
      sp->variant.dynamic_init = NULL;
      break;
    case stmk_asm:
      sp->variant.asm_entry = NULL;
      break;
#if ASM_FUNCTION_ALLOWED
    case stmk_asm_func_body:
      sp->variant.asm_func_body = NULL;
      break;
#endif /* ASM_FUNCTION_ALLOWED */
    case stmk_try_block:
      sp->variant.try_block = tsp =
                  (a_try_supplement_ptr)alloc_cil(sizeof(a_try_supplement));
#if DEBUG
      num_try_supplements_allocated++;
#endif /* DEBUG */
      tsp->is_function_try_block = FALSE;
      tsp->statement         = NULL;
      tsp->handlers          = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      tsp->finally_statement = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      tsp->lifetime          = NULL;
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case stmk_microsoft_try:
      { a_microsoft_try_supplement_ptr mtsp = (a_microsoft_try_supplement_ptr)
                                 alloc_cil(sizeof(a_microsoft_try_supplement));
        sp->variant.microsoft_try = mtsp;
#if DEBUG
        num_microsoft_try_supplements_allocated++;
#endif /* DEBUG */
        mtsp->guarded_statement = NULL;
        mtsp->except_expr       = NULL;
        mtsp->cleanup_statement = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        mtsp->except_or_finally_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case stmk_decl:
      sp->variant.decl.entities = NULL;
      break;
    case stmk_set_vla_size:
      sp->variant.vla_dimension = NULL;
      break;
    case stmk_vla_decl:
      sp->variant.vla.is_typedef_decl  = FALSE;
      sp->variant.vla.variant.variable = NULL;
      break;
#if CHECKING
    default:
      internal_error("set_statement_kind: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* set_statement_kind */


a_statement_ptr alloc_statement(a_statement_kind stmt_kind)
/*
Allocate a statement entry, clear it to default values, and return a pointer
to it.  The statement kind is set as indicated.
*/
{
  a_statement_ptr sp;

  db_enter(5, "alloc_statement");

  sp = (a_statement_ptr)alloc_cil(sizeof(a_statement));
#if DEBUG
  num_statements_allocated++;
#endif /* DEBUG */
  clear_stmt_source_position(sp->position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  clear_stmt_source_position(sp->end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  sp->next                    = NULL;
  sp->attributes              = NULL;
  sp->has_associated_pragma   = FALSE;
  sp->is_initialization_guard = FALSE;
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
  sp->is_lowering_boilerplate = FALSE;
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
#if CENTERLINE_CHECKING
  sp->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sp->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  set_statement_kind(sp, stmt_kind);
  db_exit();
  return sp;
}  /* alloc_statement */


a_constructor_init_ptr alloc_ctor_init(a_constructor_init_kind  kind)
/*
Allocate a constructor initializer entry, initialize it, and return a
pointer to it.
*/
{
  a_constructor_init_ptr  cip;

  cip = (a_constructor_init_ptr)alloc_cil(sizeof(a_constructor_init));
#if DEBUG
  num_constructor_inits_allocated++;
#endif /* DEBUG */
  cip->next = NULL;
  cip->kind = kind;
  cip->compiler_generated = FALSE;
  cip->is_pack_expansion = FALSE;
  cip->is_braced = FALSE;
  switch (kind) {
    case cik_virtual_base_class:
    case cik_direct_base_class:
      cip->variant.base_class = NULL;
      break;
    case cik_field:
      cip->variant.field = NULL;
      break;
#if CHECKING
    default:
      internal_error("alloc_ctor_init: bad kind");
#endif /* CHECKING */
  }  /* switch */
  cip->initializer = NULL;
  cip->source_expr = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  cip->ctor_init_range = null_source_range; 
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  return cip;
}  /* alloc_ctor_init */

#if GNU_EXTENSIONS_ALLOWED

void clear_gcc_pragma_descr(a_gcc_pragma_descr  *gpd)
/*
Clear the given GCC pragma description.
*/
{
  gpd->kind = (a_gcc_pragma_kind)gcc_pk_none;
}  /* clear_gcc_pragma_descr */

#endif /* GNU_EXTENSIONS_ALLOWED */

a_pragma_ptr alloc_pragma(a_pragma_kind           kind,
                          a_source_correspondence *scp)
/*
Allocate a pragma entry of the required kind, initialize it, and return a
pointer to it.  If scp is non-NULL, the pragma will point to the declarative
entity with the indicated source correspondence, so allocate the pragma
in the same memory region as that entity; otherwise, allocate the pragma
in the current IL memory region.
*/
{
  a_pragma_ptr  pp;

  if (scp == NULL) {
    pp = (a_pragma_ptr)alloc_cil(sizeof(a_pragma));
  } else {
    pp = (a_pragma_ptr)alloc_in_same_region_as(scp, sizeof(a_pragma));
  }  /* if */
#if DEBUG
  num_pragmas_allocated++;
#endif /* DEBUG */
  pp->next                  = NULL;
  pp->kind                  = kind;
  pp->ignore_in_back_end    = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  pp->is_microsoft_pragma_operator = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  clear_tagged_ptr(pp->entity);
  pp->position              = null_source_position;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  pp->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  pp->pragma_text           = NULL;
  switch (kind) {
#if IDENT_DIRECTIVE_AND_PRAGMA
    case pk_ident:
      pp->variant.ident_string = NULL;
      break;
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
    case pk_none:
      break;
#if USER_CONTROL_OF_STRUCT_PACKING
    case pk_pack:
#if BACK_END_IS_CP_GEN_BE
      pp->variant.alignment = 0;
#endif /* BACK_END_IS_CP_GEN_BE */
      break;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if PRAGMA_WEAK_ALLOWED
    case pk_weak:
#endif /* PRAGMA_WEAK_ALLOWED */
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
    case pk_redefine_extname:
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if SUN_EXTENSIONS_ALLOWED
    case pk_enable_ldscope:
    case pk_disable_ldscope:
#endif /* SUN_EXTENSIONS_ALLOWED */
    case pk_diag_suppress:
    case pk_diag_remark:
    case pk_diag_warning:
    case pk_diag_error:
    case pk_diag_once:
    case pk_diag_default:
#if INCLUDE_EDG_TEST_PRAGMAS
    case pk_test_next_statement:
    case pk_test_next_decl:
    case pk_test_immediate:
    case pk_test_immediate_text:
    case pk_test_immediate_pp_text:
    case pk_test_other:
    case pk_test_bind_next_pass:
#endif /* INCLUDE_EDG_TEST_PRAGMAS */
      break;
#if ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING
    case pk_checking_pragma:
      break;
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */
#if DEBUG
    case pk_db_opt:
    case pk_db_name:
      break;
#endif /* DEBUG */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case pk_push_macro:
    case pk_pop_macro:
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
    case pk_setlocale:
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
    case pk_unrecognized:
      /* No special initialization is required. */
      break;
#endif /* INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* The following identify pragmas that have immediate effect in the
       front end and do not get passed to the back end; therefore, no IL
       pragma entries are created for them.  The exception is when
       source-sequence lists are being put out, since all pragmas need to be
       included on such lists. */
    case pk_printf_args:
    case pk_scanf_args:
    case pk_lint_argsused:
    case pk_lint_varargs_count:
    case pk_lint_notreached:
    case pk_instantiate:
    case pk_do_not_instantiate:
    case pk_can_instantiate:
    case pk_inline_template:
    case pk_define_type_info:
      break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
    case pk_if_exists:
      break;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
    case pk_stdc:
      pp->variant.stdc.kind = (a_stdc_pragma_kind)stdc_pk_none;
      break;
#if UPC_EXTENSIONS_ALLOWED
    case pk_upc:
      pp->variant.upc.kind = (a_upc_pragma_kind)upc_pk_access;
      pp->variant.upc.value.access_method =
                                   (a_upc_access_method)upc_access_unspecified;
      break;
#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case pk_comment:
      pp->variant.comment.kind =
                                (a_microsoft_pragma_comment_type)mpct_compiler;
      pp->variant.comment.str = NULL;
      break;
    case pk_conform:
      pp->variant.conform.kind =
                                (a_microsoft_pragma_conform_kind)mpck_forScope;
      pp->variant.conform.on = FALSE;
      pp->variant.conform.off = FALSE;
      pp->variant.conform.show = FALSE;
      pp->variant.conform.push = FALSE;
      pp->variant.conform.pop = FALSE;
      pp->variant.conform.identifier = NULL;
      break;
    case pk_include_alias:
      pp->variant.include_alias.long_file_name = NULL;
      pp->variant.include_alias.short_file_name = NULL;
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    case pk_gcc:
      clear_gcc_pragma_descr(&pp->variant.gcc);
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if CHECKING
    default:
      internal_error("alloc_pragma: bad pragma kind");
#endif /* CHECKING */
  }  /* switch */

  return pp;
}  /* alloc_pragma */


an_object_lifetime_ptr alloc_object_lifetime(an_object_lifetime_kind  kind)
/*
Allocate an object lifetime entry, initialize its fields, and return a pointer
to it.
*/
{
  an_object_lifetime_ptr  olp, *avail_list_ptr;
  a_scope_depth           scope_depth;

  db_enter(5, "alloc_object_lifetime");
  /* Use an object lifetime entry that is on an available list, if possible;
     otherwise, allocate a new one. */
  /* Note that the file scope and every function scope (i.e., each scope for
     which there is a unique memory region) has its own available list. */
  if (curr_il_region_number == file_scope_region_number) {
    /* Use the file scope. */
    scope_depth = DEPTH_OF_FILE_SCOPE;
  } else {
    /* Use the current function scope. */
    scope_depth = depth_innermost_function_scope;
  }  /* if */
  /* When IL lowering generates routines, there is no scope stack entry,
     and therefore no available list can be maintained. */
  if (scope_depth != NO_SCOPE_DEPTH &&
      (avail_list_ptr = &scope_stack[scope_depth].object_lifetime_avail_list,
       *avail_list_ptr != NULL)) {
    /* Reuse a previously freed entry. */
    olp = *avail_list_ptr;
    *avail_list_ptr = olp->next;
  } else {
    /* Allocate a new entry. */
    olp = (an_object_lifetime_ptr)alloc_cil(sizeof(an_object_lifetime));
#if DEBUG
    num_object_lifetimes_allocated++;
#endif /* DEBUG */
  }  /* if */
  /* Set the fields to default values. */
  clear_tagged_ptr(olp->entity);
  olp->kind                       = kind;
  olp->has_block_after_label_child_lifetime
                                  = FALSE;
  olp->has_implicit_child         = FALSE;
  olp->destructions               = NULL;
  olp->parent_lifetime            = NULL;
  olp->parent_destruction_sublist = NULL;
  olp->child_lifetime             = NULL;
  olp->next                       = NULL;
  db_exit();
  return olp;
}  /* alloc_object_lifetime */


void clear_namespace(a_namespace_ptr nsp,
                     a_boolean       is_alias)
/*
Initialize the namespace pointed to by nsp.  The namespace is an alias if
is_alias is TRUE.
*/
{
  set_default_source_corresp(nsp->source_corresp);
  nsp->next = NULL;
  nsp->is_namespace_alias = is_alias;
#if MICROSOFT_EXTENSIONS_ALLOWED
  nsp->proxy_class = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  nsp->is_inline = FALSE;
  nsp->named_in_strong_using = FALSE;
  if (is_alias) {
    nsp->variant.assoc_namespace = NULL;
  } else {
    nsp->variant.assoc_scope = NULL;
  }  /* if */
}  /* clear_namespace */


a_namespace_ptr alloc_namespace(a_boolean  is_alias)
/*
Allocate a namespace entry, initialize its fields, and return a pointer to
it.  The entry is allocated in the file scope memory region (even when
creating an entry for a local namespace alias).
*/
{
  a_namespace_ptr nsp;

  db_enter(5, "alloc_namespace");
  nsp = (a_namespace_ptr)alloc_il(sizeof(a_namespace));
#if DEBUG
  num_namespaces_allocated++;
#endif /* DEBUG */
  clear_namespace(nsp, is_alias);
  db_exit();
  return nsp;
}  /* alloc_namespace */


a_using_decl_ptr alloc_using_decl(void)
/*
Allocate a using-decl entry, initialize its fields, and return a pointer to it.
*/
{
  a_using_decl_ptr  udp;

  db_enter(5, "alloc_using_decl");
  udp = (a_using_decl_ptr)alloc_cil(sizeof(a_using_decl));
#if DEBUG
  num_using_decls_allocated++;
#endif /* DEBUG */
  udp->next                  = NULL;
  udp->position              = null_source_position;
  clear_tagged_ptr(udp->entity);
  udp->attributes            = NULL;
  udp->is_using_directive    = FALSE;
  udp->is_class_member       = FALSE;
  udp->hidden                = FALSE;
  udp->compiler_generated    = FALSE;
  udp->inline_namespace      = FALSE;
  udp->strong                = FALSE;
  udp->access                = (an_access_specifier)as_public;
  udp->qualifier.namespace_ptr
                             = NULL;
  udp->decl_sequence_number  = 0;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  udp->source_sequence_entry = NULL;
  udp->next_in_overload_set  = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_exit();
  return udp;
}  /* alloc_using_decl */


void set_scope_kind(a_scope_ptr    sp,
                    a_scope_kind   kind,
                    a_routine_ptr  assoc_routine)
/*
Initialize the variable fields of the scope entry pointed to by sp.
*/
{
  check_assertion_str(assoc_routine == NULL ||
                        kind == (a_scope_kind)sck_function,
                      "set_scope_kind: assoc_routine is non-NULL");
  sp->kind   = kind;
  switch (kind) {
    case sck_file:
    case sck_template_declaration:
      break;
    case sck_block:
      sp->variant.assoc_handler = NULL;
      break;
    case sck_func_prototype:
    case sck_class_struct_union:
    case sck_enum:
      sp->variant.assoc_type = NULL;
      break;
    case sck_function:
      sp->variant.routine.ptr                           = assoc_routine;
      sp->variant.routine.parameters                    = NULL;
      sp->variant.routine.constructor_inits             = NULL;
      sp->variant.routine.lifetime_of_local_static_vars = NULL;
      sp->variant.routine.this_param_variable           = NULL;
      sp->variant.routine.return_value_variable         = NULL;
      break;
    case sck_condition:
      sp->variant.assoc_statement = NULL;
      break;
    case sck_namespace:
      sp->variant.assoc_namespace = NULL;
      break;
#if CHECKING
    default:
      internal_error("set_scope_kind: bad scope kind");
#endif /* CHECKING */
  }  /* switch */
}  /* set_scope_kind */


a_scope_ptr alloc_scope(a_scope_kind   kind,
                        a_scope_number number,
                        a_routine_ptr  assoc_routine)
/*
Allocate a scope entry, and return a pointer to it.  Set fixed fields to
default values.  kind indicates the scope kind (e.g., function, block),
number indicates the unique number for the scope, and assoc_routine
points to the associated routine if the kind is sck_function.
*/
{
  a_scope_ptr sp;

  db_enter(5, "alloc_scope");

  sp = (a_scope_ptr)alloc_cil(sizeof(a_scope));
#if DEBUG
  num_scopes_allocated++;
#endif /* DEBUG */
  sp->next   = NULL;
  sp->parent = NULL;
  sp->number = number;
  sp->function_body_processing_finished = FALSE;
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  sp->scope_orphaned_list_header_generated = FALSE;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  set_scope_kind(sp, kind, assoc_routine);
  sp->assoc_block                 = NULL;
  sp->lifetime                    = NULL;
  sp->constants                   = NULL;
  sp->types                       = NULL;
  sp->variables                   = NULL;
  sp->nonstatic_variables         = NULL;
  sp->labels                      = NULL;
  sp->routines                    = NULL;
  sp->asm_entries                 = NULL;
  sp->scopes                      = NULL;
  sp->namespaces                  = NULL;
  sp->using_decls                 = NULL;
  sp->dynamic_inits               = NULL;
  sp->local_static_variable_inits = NULL;
  sp->vla_dimensions              = NULL;
  sp->expr_node_refs              = NULL;
  sp->scope_refs                  = NULL;
  sp->pragmas                     = NULL;
  sp->depth_in_scope_stack        = NO_SCOPE_DEPTH;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sp->source_sequence_list        = NULL;
  sp->src_seq_sublist_list        = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL
  sp->hidden_names                = NULL;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  sp->templates                   = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  sp->ms_attributes               = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  sp->ms_if_exists                = NULL;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  db_exit();
  return sp;
}  /* alloc_scope */


a_local_scope_ref_ptr alloc_local_scope_ref(void)
/*
Allocate an entry to represent an outside reference to a local scope,
initialize it, and return a pointer to it.
*/
{
  a_local_scope_ref_ptr  ptr;

  ptr = (a_local_scope_ref_ptr)alloc_cil(sizeof(a_local_scope_ref));
#if DEBUG
  num_local_scope_refs_allocated++;
#endif /* DEBUG */
  ptr->next = NULL;
  ptr->scope = NULL;
  clear_tagged_ptr(ptr->referrer);
  return ptr;
}  /* alloc_local_scope_ref */


#if GENERATE_SOURCE_SEQUENCE_LISTS

a_source_sequence_entry_ptr alloc_source_sequence_entry(void)
/*
Allocate a source sequence entry, initialize its fields, and return a pointer
to it.
*/
{
  a_source_sequence_entry_ptr  ssep, *avail_list_ptr;
  a_scope_depth                scope_depth;

  /* Use a source sequence entry that is on an available list, if possible;
     otherwise, allocate a new one. */
  /* Note that each scope that has a source sequence list (there is one such
     scope per memory region) also has its own available list. */
  if (curr_il_region_number == file_scope_region_number) {
    /* Use the file scope. */
    scope_depth = DEPTH_OF_FILE_SCOPE;
  } else {
    /* Use the current function scope. */
    check_assertion(depth_innermost_function_scope != NO_SCOPE_DEPTH);
    scope_depth = depth_innermost_function_scope;
  }  /* if */
  /* Copy the address of the available list. */
  avail_list_ptr = &scope_stack[scope_depth].source_sequence_avail_list;
  if (*avail_list_ptr != NULL) {
    ssep = *avail_list_ptr;
#ifdef TRACE_ALLOC
    trace_alloc_check(ssep);
#endif /* TRACE_ALLOC */
    *avail_list_ptr = ssep->next;
  } else {
    ssep = (a_source_sequence_entry_ptr)
                                   alloc_cil(sizeof(a_source_sequence_entry));
#if DEBUG
    num_source_sequence_entries_allocated++;
#endif /* DEBUG */
  }  /* if */
  /* Initialize the fields. */
  ssep->next        = NULL;
  ssep->prev        = NULL;
  clear_tagged_ptr(ssep->entity);

  return ssep;
}  /* alloc_source_sequence_entry */


a_src_seq_secondary_decl_ptr alloc_src_seq_secondary_decl(void)
/*
Allocate a source sequence secondary declaration entry, initialize its fields,
and return a pointer to it.
*/
{
  a_src_seq_secondary_decl_ptr  sssdp;

  sssdp = (a_src_seq_secondary_decl_ptr)
                                  alloc_cil(sizeof(a_src_seq_secondary_decl));
#if DEBUG
  num_src_seq_secondary_decls_allocated++;
#endif /* DEBUG */
  sssdp->decl_position               = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sssdp->decl_pos_info               = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  clear_tagged_ptr(sssdp->entity);
  sssdp->name_reference              = NULL;
  sssdp->attributes                  = NULL;
  sssdp->declared_type               = NULL;
  sssdp->declared_storage_class      = (a_storage_class)sc_unspecified;
  sssdp->autonomous_tag_decl         = FALSE;
  sssdp->embedded_source_sequence_entries = FALSE;
  sssdp->friend_decl                 = FALSE;
  sssdp->implicit_decl               = FALSE;
  sssdp->declared_in_func_prototype  = FALSE;
  sssdp->specialized_with_new_syntax = FALSE;
  sssdp->first_declaration           = FALSE;
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  sssdp->is_partial_instantiation    = FALSE;
  sssdp->compiler_generated_forward_decl = FALSE;
  sssdp->originally_nonautonomous_definition = FALSE;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#if GNU_EXTENSIONS_ALLOWED
  sssdp->marked_as_gnu_extension     = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  sssdp->is_decl_after_first_in_comma_list = FALSE;
  sssdp->explicit_storage_class      = FALSE;
  sssdp->is_alias                    = FALSE;
#if CENTERLINE_CHECKING
  sssdp->avoid_codecenter_warnings = 0;
#endif /* CENTERLINE_CHECKING */

  return sssdp;
}  /* alloc_src_seq_secondary_decl */


a_src_seq_end_of_construct_ptr alloc_src_seq_end_of_construct(void)
/*
Allocate an end-of-construct declaration entry, initialize its fields, and
return a pointer to it.
*/
{
  a_src_seq_end_of_construct_ptr  sseocp;

  sseocp = (a_src_seq_end_of_construct_ptr)alloc_cil(
                                           sizeof(a_src_seq_end_of_construct));
#if DEBUG
  num_src_seq_end_of_constructs_allocated++;
#endif /* DEBUG */
  sseocp->position    = null_source_position;
  clear_tagged_ptr(sseocp->entity);

  return sseocp;
}  /* alloc_src_seq_end_of_construct */


a_src_seq_sublist_ptr alloc_src_seq_sublist(void)
/*
Allocate a source sequence sublist header, initialize its fields, and return
a pointer to it.
*/
{
  a_src_seq_sublist_ptr  sssp;

  sssp = (a_src_seq_sublist_ptr)alloc_il(sizeof(a_src_seq_sublist));
#if DEBUG
  num_src_seq_sublists_allocated++;
#endif /* DEBUG */
  sssp->next = NULL;
  sssp->source_sequence_list = NULL;
  sssp->last_source_sequence_entry = NULL;

  return sssp;
}  /* alloc_src_seq_sublist */


an_instantiation_directive_ptr alloc_instantiation_directive(void)
/*
Allocate an instantiation-directive entry, initialize its fields, and return
a pointer to it.
*/
{
  an_instantiation_directive_ptr  idp;

  idp = (an_instantiation_directive_ptr)alloc_il(
                                          sizeof(an_instantiation_directive));
#if DEBUG
  num_instantiation_directives_allocated++;
#endif /* DEBUG */
  idp->position    = null_source_position;
  clear_tagged_ptr(idp->entity);
  idp->do_not_instantiate = FALSE;
  idp->attributes = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  idp->decl_pos_info = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  return idp;
}  /* alloc_instantiation_directive */


a_static_assertion_ptr alloc_static_assertion(void)
/*
Allocate a static assertion entry, initialize its fields, and return a
pointer to it.
*/
{
  a_static_assertion_ptr  entry;

  db_enter(5, "alloc_static_assertion");
  entry = alloc_cil_of_type(a_static_assertion);
#if DEBUG
  num_static_assertions++;
#endif /* DEBUG */
  entry->condition = NULL;
  entry->string_literal = NULL;
  entry->position = null_source_position;
  db_exit();
  return entry;
}  /* alloc_static_assertion */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL

a_hidden_name_ptr alloc_hidden_name(void)
/*
Allocate a hidden-name entry in the current memory region, initialize its
fields, and return a pointer to it.
*/
{
  a_hidden_name_ptr  hnp;

  hnp = (a_hidden_name_ptr)alloc_cil(sizeof(a_hidden_name));
#if DEBUG
  num_hidden_names_allocated++;
#endif /* DEBUG */
  hnp->next                             = NULL;
  clear_tagged_ptr(hnp->entity);
  hnp->qualification_needed             = FALSE;
  hnp->elaborated_type_specifier_needed = FALSE;
  hnp->partially_hidden_by_microsoft_injected_class_name
                                        = FALSE;
  hnp->is_class_member                  = FALSE;
  hnp->hidden_by_simulated_injected_class_name
                                        = FALSE;
#if CENTERLINE_CHECKING
  hnp->avoid_codecenter_warnings        = 0;
#endif /* CENTERLINE_CHECKING */

  return hnp;
}  /* alloc_hidden_name */

#endif /* RECORD_HIDDEN_NAMES_IN_IL */


a_template_parameter_ptr alloc_template_parameter(void)
/*
Allocate a template parameter entry in the file-scope memory region,
initialize its fields, and return a pointer to it.
*/
{
  a_template_parameter_ptr  tpp;

  tpp = (a_template_parameter_ptr)alloc_il(sizeof(a_template_parameter));
#if DEBUG
  num_template_parameters_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(tpp->source_corresp);
  tpp->next = NULL;
  tpp->kind = (a_template_parameter_kind)tpk_error;
  tpp->is_pack = FALSE;
  return tpp; 
}  /* alloc_template_parameter */


a_template_decl_ptr alloc_template_decl(void)
/*
Allocate a template declaration entry in the file-scope memory region,
initialize its fields, and return a pointer to it.
*/
{
  a_template_decl_ptr  tdp;

  tdp = (a_template_decl_ptr)alloc_il(sizeof(a_template_decl));
#if DEBUG
  num_template_decls_allocated++;
#endif /* DEBUG */
  tdp->parent = NULL;
  tdp->param_list = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  tdp->generic_constraint_clauses = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  tdp->scope = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  tdp->template_pos = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  return tdp; 
}  /* alloc_template_decl */


a_template_ptr alloc_template(void)
/*
Allocate a template entry in the file-scope memory region, initialize its
fields, and return a pointer to it.
*/
{
  a_template_ptr  tp;

  tp = (a_template_ptr)alloc_il(sizeof(a_template));
#if DEBUG
  num_templates_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(tp->source_corresp);
  tp->next = NULL;
  tp->kind = (a_template_kind)templk_none;
  tp->is_exported = FALSE;
  tp->ignore_export = FALSE;
  tp->is_pack = FALSE;
#if RECORD_TEMPLATE_STRINGS
  tp->text = NULL;
#endif /* RECORD_TEMPLATE_STRINGS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  tp->export_position = null_source_position;
  tp->definition_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  tp->template_info = NULL;
  tp->coordinates.position = 0;
  tp->coordinates.depth = NO_NESTING_DEPTH;
  tp->template_decl = NULL;
  tp->prototype_instantiation.type = NULL;
  tp->prototype_instantiation.routine = NULL;
  tp->prototype_instantiation.variable = NULL;
  tp->canonical_template = NULL;
  tp->definition_template = NULL;
  tp->prototype_template = NULL;
  tp->cache_checksum = 0;
#if BACK_END_IS_CP_GEN_BE
#if USER_CONTROL_OF_STRUCT_PACKING
  tp->final_alignment = 0;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  tp->min_template_arguments = -1;
#endif /* BACK_END_IS_CP_GEN_BE */
  return tp;
}  /* alloc_template */

#if RECORD_MACROS_IN_IL

a_macro_ptr alloc_macro(void)
/*
Allocate a macro entry in the file-scope memory region, initialize its
fields, and return a pointer to it.
*/
{
  a_macro_ptr  mp;

  mp = (a_macro_ptr)alloc_il(sizeof(a_macro));
#if DEBUG
  num_macros_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(mp->source_corresp);
  mp->next = NULL;
  mp->is_undef = FALSE;
  mp->is_command_line_definition = FALSE;
  mp->is_predefined = FALSE;
  mp->object_like = TRUE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  mp->replacement_text_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  mp->text = NULL;

  return mp;
}  /* alloc_macro */

#endif /* RECORD_MACROS_IN_IL */
#if RECORD_MACRO_INVOCATIONS

a_macro_invocation_record_block_ptr alloc_macro_invocation_record_block(void)
/*
Allocate a macro invocation record block entry in the file-scope memory
region, initialize the fields, and return a pointer to it.
*/
{
  a_macro_invocation_record_block_ptr mirbp;
  int                                 i;

  mirbp = (a_macro_invocation_record_block_ptr)
                             alloc_il(sizeof(a_macro_invocation_record_block));
#if DEBUG
  num_macro_invocation_record_blocks_allocated++;
#endif /* DEBUG */
  mirbp->first_record_in_block = 0;
  mirbp->left_subtree = NULL;
  mirbp->right_subtree = NULL;
  mirbp->prev = NULL;
  mirbp->next = NULL;
  for (i = 0; i < MACRO_INVOCATION_RECORDS_PER_BLOCK; ++i) {
    mirbp->records[i].parent_macro_index = NO_PARENT_MACRO_INVOCATION;
    mirbp->records[i].assoc_macro = NULL;
    mirbp->records[i].start.seq = 0;
    mirbp->records[i].start.column = SP_COL_UNKNOWN;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    mirbp->records[i].end.seq = 0;
    mirbp->records[i].end.column = SP_COL_UNKNOWN;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* for */

  return mirbp;
}  /* alloc_macro_invocation_record_block */
#endif /* RECORD_MACRO_INVOCATIONS */
#if EXTRA_SOURCE_POSITIONS_IN_IL

void clear_decl_position_supplement(a_decl_position_supplement_ptr  dpsp)
/*
Clear the fields of the specified decl-position-supplement entry.
*/
{
  dpsp->identifier_range = null_source_range;
  dpsp->specifiers_range = null_source_range;
  dpsp->variant.declarator_range = null_source_range;
}  /* clear_decl_position_supplement */


a_decl_position_supplement_ptr alloc_decl_position_supplement(
                                                    a_boolean  at_file_scope)
/*
Allocate a decl-position-supplement entry in the appropriate memory region,
initialize its fields, and return a pointer to it.
*/
{
  a_decl_position_supplement_ptr  dpsp;

  if (at_file_scope) {
    dpsp = (a_decl_position_supplement_ptr)alloc_il(
                                     sizeof(a_decl_position_supplement));
  } else {
    dpsp = (a_decl_position_supplement_ptr)alloc_cil(
                                     sizeof(a_decl_position_supplement));
  }  /* if */
#if DEBUG
  num_decl_position_supplements_allocated++;
#endif /* DEBUG */
  clear_decl_position_supplement(dpsp);
  return dpsp;
}  /* alloc_decl_position_supplement */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */


a_name_qualifier_ptr alloc_name_qualifier(void)
/*
Allocate a name qualifier entry, initialize its fields, and return a pointer
to it.
*/
{
  a_name_qualifier_ptr nqp;

  nqp = alloc_il_of_type(a_name_qualifier);
#if DEBUG
  num_name_qualifiers_allocated++;
#endif /* DEBUG */
  nqp->next = NULL;
  nqp->qualifier.class_type = NULL;
  nqp->qualifier.namespace_ptr = NULL;
  nqp->previous_qualifier = NULL;
  nqp->is_class = FALSE;
  return nqp;
}  /* alloc_name_qualifier */


void clear_name_reference(a_name_reference_ptr	nrp)
/*
Initialize the fields of a name reference entry.
*/
{
  nrp->next = NULL;
  nrp->qualifier = NULL;
  nrp->destructor_type = NULL;
  nrp->num_template_arguments = -1L;
  nrp->is_global_qualified_name = FALSE;
  nrp->is_template_id = FALSE;
  nrp->is_super_qualified = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  nrp->used_in_primary_declarator = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  nrp->from_prototype_instantiation = FALSE;
}  /* clear_name_reference */


a_name_reference_ptr alloc_name_reference(void)
/*
Allocate a name reference entry, initialize its fields, and return a pointer
to it.
*/
{
  a_name_reference_ptr nrp;

  nrp = alloc_il_of_type(a_name_reference);
#if DEBUG
  num_name_references_allocated++;
#endif /* DEBUG */
  clear_name_reference(nrp);
  return nrp;
}  /* alloc_name_reference */


a_seq_number_lookup_entry_ptr alloc_seq_number_lookup_entry(void)
/*
Allocate a sequence number lookup entry, initialize its fields, and return
a pointer to it.
*/
{
  a_seq_number_lookup_entry_ptr snlep;

  snlep = (a_seq_number_lookup_entry_ptr)
                alloc_primary_file_scope_il(sizeof(a_seq_number_lookup_entry));
  snlep->first = 0;
  snlep->last = MAX_SEQ_NUMBER;
  snlep->line_number = 0;
  snlep->next = NULL;
  snlep->source_file = NULL;
#if DEBUG
  num_seq_number_lookup_entries_allocated++;
#endif /* DEBUG */
  return snlep;
}  /* alloc_seq_number_lookup_entry */

#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES

an_ms_if_exists_ptr alloc_ms_if_exists(void)
/*
Allocate a Microsoft __if_exists entry, set its fields to default values,
and return a pointer to it.
*/
{
  an_ms_if_exists_ptr msiep;

  msiep = alloc_cil_of_type(an_ms_if_exists);
#if DEBUG
  num_ms_if_exists_allocated++;
#endif /* DEBUG */
  msiep->next = NULL;
  clear_tagged_ptr(msiep->entity);
  msiep->position = null_source_position;
  msiep->name_reference = NULL;
  msiep->is_if_exists = FALSE;
  msiep->pending = FALSE;
  return msiep;
}  /* alloc_ms_if_exists */

#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

#if MICROSOFT_EXTENSIONS_ALLOWED

an_ms_attribute_ptr alloc_ms_attribute(void)
/*
Allocate a Microsoft attribute entry, set its fields to default values,
and return a pointer to it.
*/
{
  an_ms_attribute_ptr msap;

  msap = alloc_cil_of_type(an_ms_attribute);
#if DEBUG
  num_ms_attributes_allocated++;
#endif /* DEBUG */
  msap->name = NULL;
  msap->kind = (an_ms_attribute_kind)msak_none;
  msap->next = NULL;
  msap->next_in_block = NULL;
  clear_tagged_ptr(msap->entity);
  msap->string = NULL;
  msap->arg_list = NULL;
  msap->name = NULL;
  msap->position = null_source_position;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  msap->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  msap->kind_descr = NULL;
  return msap;
}  /* alloc_ms_attribute */


an_ms_attribute_arg_ptr alloc_ms_attribute_arg(an_ms_attribute_arg_kind	kind)
/*
Allocate a Microsoft attribute argument entry, set its fields to default
values, and return a pointer to it.
*/
{
  an_ms_attribute_arg_ptr msaap;

  msaap = alloc_cil_of_type(an_ms_attribute_arg);
#if DEBUG
  num_ms_attribute_args_allocated++;
#endif /* DEBUG */
  msaap->kind = kind;
  msaap->next = NULL;
  msaap->param_name = NULL;
  switch (kind) {
    case msaak_integer:
      msaap->variant.integer_value = 0;
      break;
    case msaak_boolean:
      msaap->variant.bool_value = 0;
      break;
    case msaak_string:
      msaap->variant.string_constant = NULL;
      break;
    case msaak_other:
      msaap->variant.other_string = NULL;
      break;
    case msaak_uuid:
      msaap->variant.uuid_string = NULL;
      break;
    case msaak_enumeration:
      msaap->variant.enum_value = 0;
      break;
    case msaak_none:
    default:
      unexpected_condition_str("alloc_ms_attribute_arg: bad kind");
      break;
  }  /* switch */
  return msaap;
}  /* alloc_ms_attribute_arg */


a_property_index_type_ptr alloc_property_index_type(void)
/*
Allocate a property index type entry, clear it to default values, and return a
pointer to it.
*/
{
  a_property_index_type_ptr  pitp = (a_property_index_type_ptr)
                                      alloc_il(sizeof(a_property_index_type));
#if DEBUG
  num_property_index_types_allocated++;
#endif /* DEBUG */
  pitp->next = NULL;
  pitp->type = NULL;
  pitp->position = null_source_position;
  return pitp;
}  /* alloc_property_index_type */


a_property_or_event_descr_ptr alloc_property_or_event_descr(
                                               a_property_or_event_kind  kind)
/*
Allocate a property/event description of the given kind, clear it to default
values, and return a pointer to it.
*/
{
  a_property_or_event_descr_ptr  pdp;

  pdp = alloc_il_of_type(a_property_or_event_descr);
#if DEBUG
  num_property_or_event_descriptions_allocated++;
#endif /* DEBUG */
  pdp->kind = kind;
  pdp->is_trivial = FALSE;
  pdp->is_default_indexed = FALSE;
  pdp->is_virtual = FALSE;
  pdp->is_static = FALSE;
  pdp->indices = NULL;
  /* Clear field of inactive variant too for union-as-struct testing. */
  pdp->variant.variable = NULL;
  pdp->variant.field = NULL;
  switch (kind) {
    case pek_declspec_property:
      pdp->get_routine.name = NULL;
      pdp->set_routine.name = NULL;
      break;
    case pek_cli_property:
      pdp->get_routine.ptr = NULL;
      pdp->set_routine.ptr = NULL;
      break;
    case pek_cli_event:
      /* get_routine/set_routine is not used for events. */
      pdp->get_routine.ptr = NULL;
      pdp->set_routine.ptr = NULL;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  pdp->add_routine = NULL;
  pdp->remove_routine = NULL;
  pdp->raise_routine = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  pdp->property_or_event_position = null_source_position;
  pdp->indices_range = null_source_range;
  pdp->definition_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return pdp;
}  /* alloc_property_or_event_descr */


a_generic_constraint_ptr alloc_generic_constraint(void)
/*
Allocate an entry describing a C++/CLI generic constraint and return a
pointer to it.
*/
{
  a_generic_constraint_ptr	gcp;

  gcp = alloc_il_of_type(a_generic_constraint);
#if DEBUG
  num_generic_constraints_allocated++;
#endif /* DEBUG */
  gcp->kind = (a_generic_constraint_kind)gck_none;
  gcp->implicit_constraint = FALSE;
  gcp->next = NULL;
  gcp->type = NULL;
  gcp->type_cache = NULL;
  gcp->position = null_source_position;
  return gcp;
}  /* alloc_generic_constraint */


void clear_generic_constraint_clause(a_generic_constraint_clause_ptr gccp)
/*
Initialize the fields of a C++/CLI generic constraint clause entry.
*/
{
  gccp->next = NULL;
  gccp->type = NULL;
  gccp->type_position = null_source_position;
  gccp->constraints = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  gccp->where_position = null_source_position;
  gccp->colon_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* clear_generic_constraint_clause */


a_generic_constraint_clause_ptr alloc_generic_constraint_clause(void)
/*
Allocate an entry describing a C++/CLI generic constraint clause and return a
pointer to it.
*/
{
  a_generic_constraint_clause_ptr	gccp;

  gccp = alloc_il_of_type(a_generic_constraint_clause);
#if DEBUG
  num_generic_constraint_clauses_allocated++;
#endif /* DEBUG */
  clear_generic_constraint_clause(gccp);
  return gccp;
}  /* alloc_generic_constraint_clause */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_lambda_ptr alloc_lambda(void)
/*
Allocate an entry describing a C++11 lambda and return a pointer to it.  The
entry is allocated in the current memory region.
*/
{
  a_lambda_ptr  entry = (a_lambda_ptr)alloc_cil(sizeof(a_lambda));

  entry->capture_list = NULL;
  entry->closure_class = NULL;
  entry->lambda_routine = NULL;
  entry->is_mutable = FALSE;
  entry->has_capture_default = FALSE;
  entry->default_is_by_reference = FALSE;
  entry->explicit_return_type = FALSE;
  entry->has_parameter_decl = FALSE;
  entry->start_position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  entry->capture_end_position = null_source_position;
  entry->mutable_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return entry;
}  /* alloc_lambda */


a_lambda_capture_ptr alloc_lambda_capture(void)
/*
Allocate an entry describing an entity (variable, reference, or this parameter)
captured by a C++11 lambda and return a pointer to it.  The entry is allocated
in the current memory region.
*/
{
  a_lambda_capture_ptr  entry = (a_lambda_capture_ptr)
                                           alloc_cil(sizeof(a_lambda_capture));

  entry->next = NULL;
  entry->variable = NULL;
  entry->source_closure_field = NULL;
  entry->closure_field = NULL;
  entry->capture_by_reference = FALSE;
  entry->is_implicit = FALSE;
  entry->is_pack_expansion = FALSE;
  entry->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  entry->end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return entry;
}  /* alloc_lambda_capture */


an_il_entity_list_entry_ptr alloc_il_entity_list_entry(void)
/*
Allocate an entry for a list of arbitrary IL entries, and return a pointer to
it.  The entry is allocated in the current memory region.
*/
{
  an_il_entity_list_entry_ptr  entry;

  entry = (an_il_entity_list_entry_ptr)
                                  alloc_cil(sizeof(an_il_entity_list_entry));
  entry->next = NULL;
  clear_tagged_ptr(entry->entity);
#if DEBUG
  ++num_il_entity_list_entries_allocated;
#endif /* DEBUG */
  return entry;
}  /* alloc_il_entity_list_entry */


an_attribute_ptr alloc_attribute(void)
/*
Allocate an attribute in file scope memory and return a pointer to it.
*/
{
  an_attribute_ptr  ap;

  ap = alloc_il_of_type(an_attribute);
  ap->next = NULL;
  ap->kind = (a_byte_attribute_kind)ak_unrecognized;
  ap->family = (a_byte_attribute_family)af_internal;
  ap->syntactic_location = (a_byte_attribute_location)al_implicit;
  ap->on_primary_declaration = FALSE;
  ap->transforms_type_specifier = FALSE;
  ap->must_be_preserved_in_trans_unit_copy = FALSE;
  ap->is_pack_expansion = FALSE;
  ap->name = NULL;
  ap->namespace_name = NULL;
  ap->arguments = NULL;
  ap->group = NULL;
  ap->assoc_info = NULL;
  ap->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  ap->end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if DEBUG
  ++num_attributes_allocated;
#endif /* DEBUG */
  return ap;
}  /* alloc_attribute */


an_attribute_arg_ptr alloc_attribute_arg(void)
/*
Allocate an attribute argument in file scope memory and return a pointer to it.
*/
{
  an_attribute_arg_ptr  aap;

  aap = alloc_il_of_type(an_attribute_arg);
  aap->next = NULL;
  aap->kind = (an_attribute_arg_kind)aak_empty;
  aap->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  aap->end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  aap->token_kind = (a_small_token_kind)tok_error;
  aap->variant.token = NULL;
#if DEBUG
  ++num_attribute_args_allocated;
#endif /* DEBUG */
  return aap;
}  /* alloc_attribute_arg */


an_attribute_group_ptr alloc_attribute_group(void)
/*
Allocate an attribute group in file scope memory and return a pointer to it.
*/
{
  an_attribute_group_ptr  agp;

  agp = alloc_il_of_type(an_attribute_group);
  agp->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  agp->end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if DEBUG
  ++num_attribute_groups_allocated;
#endif /* DEBUG */
  return agp;
}  /* alloc_attribute_group */


#if DEBUG

unsigned long show_il_alloc_space_used(unsigned long grand_total)
/*
Display and return the amount of space used for various IL tables.
*/
{
  unsigned long num, size, total;

  db_space_used_header("IL table use:");

  db_space_used("source file", num_source_files_allocated, a_source_file);
  db_space_used("seq number lookup entries",
                num_seq_number_lookup_entries_allocated,
                a_seq_number_lookup_entry);
  db_space_used("constant", num_constants_allocated, a_constant);
  db_space_used("String literal text", string_literal_text_space_allocated,
                char);
  db_space_used("IL entity list entries", num_il_entity_list_entries_allocated,
                an_il_entity_list_entry);
  db_space_used("param type", num_param_types_allocated, a_param_type);
  db_space_used("routine type supplement",
                num_routine_type_supplements_allocated,
                a_routine_type_supplement);
  db_space_used("based type list member",
                num_based_type_list_members_allocated,
                a_based_type_list_member);
  db_space_used("class type supplement", num_class_type_supplements_allocated,
                a_class_type_supplement);
  db_space_used("class list entry", num_class_list_entries_allocated,
                a_class_list_entry);
  db_space_used("routine list entry", num_routine_list_entries_allocated,
                a_routine_list_entry);
  db_space_used("overriding virtual func",
                num_overriding_virtual_functions_allocated,
                an_overriding_virtual_function);
  db_space_used("derivation steps", num_derivation_steps_allocated,
                a_derivation_step);
  db_space_used("base class derivations", num_base_class_derivations_allocated,
                a_base_class_derivation);
  db_space_used("base class", num_base_classes_allocated, a_base_class);
  db_space_used("template args", num_template_args_allocated, a_template_arg);
  db_space_used("templ param supplement",
                num_template_param_type_supplements_allocated,
                a_template_param_type_supplement);
  db_space_used("typeref type supplement",
                num_typeref_type_supplements_allocated,
                a_typeref_type_supplement);
  db_space_used("integer type supplement",
                num_integer_type_supplements_allocated,
                an_integer_type_supplement);
  db_space_used("type", num_types_allocated, a_type);
  db_space_used("dynamic init", num_dynamic_inits_allocated, a_dynamic_init);
  db_space_used("local static var inits",
                num_local_static_variable_inits_allocated,
                a_local_static_variable_init);
  db_space_used("vla dimensions", num_vla_dimensions_allocated,
                a_vla_dimension);
  db_space_used("variable", num_variables_allocated, a_variable);
  db_space_used("field", num_fields_allocated, a_field);
  db_space_used("routine", num_routines_allocated, a_routine);
  db_space_used("exception specification",
                num_exception_specifications_allocated,
                an_exception_specification);
  db_space_used("exception spec type",
                num_exception_specification_types_allocated,
                an_exception_specification_type);
  db_space_used("asm entry", num_asm_entries_allocated, an_asm_entry);
  db_space_used("label", num_labels_allocated, a_label);
  db_space_used("expr node", num_expr_nodes_allocated, an_expr_node);
  db_space_used("new/delete supplement", num_new_delete_supplements_allocated,
                a_new_delete_supplement);
#if MICROSOFT_EXTENSIONS_ALLOWED
  db_space_used("gcnew supplement", num_gcnew_supplements_allocated,
                a_gcnew_supplement);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  db_space_used("throw supplement", num_throw_supplements_allocated,
                a_throw_supplement);
  db_space_used("condition supplement", num_condition_supplements_allocated,
                a_condition_supplement);
#if !ABI_CHANGES_FOR_RTTI
  db_space_used("accessible base class", num_accessible_base_classes_allocated,
                an_accessible_base_class);
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  db_space_used("eh prologue supplement",
                num_eh_prologue_supplements_allocated,
                an_eh_prologue_supplement);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
  db_space_used("switch case entry",
                num_switch_case_entries_allocated, a_switch_case_entry);
  db_space_used("switch stmt descr",
                num_switch_stmt_descriptions_allocated, a_switch_stmt_descr);
  db_space_used("handler", num_handlers_allocated, a_handler);
  db_space_used("try supplement", num_try_supplements_allocated,
                a_try_supplement);
#if MICROSOFT_EXTENSIONS_ALLOWED
  db_space_used("Microsoft try supplement",
                num_microsoft_try_supplements_allocated,
                a_microsoft_try_supplement);
  db_space_used("Microsoft attributes",
                num_ms_attributes_allocated,
                an_ms_attribute);
  db_space_used("Microsoft attribute args",
                num_ms_attribute_args_allocated,
                an_ms_attribute_arg);
  db_space_used("property index types", num_property_index_types_allocated,
                a_property_index_type);
  db_space_used("property/event descrs",
                num_property_or_event_descriptions_allocated,
                a_property_or_event_descr);
  db_space_used("generic constraint",
                num_generic_constraints_allocated,
                a_generic_constraint);
  db_space_used("generic constraint clause",
                num_generic_constraint_clauses_allocated,
                a_generic_constraint_clause);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  db_space_used("Microsoft __if_exists",
                num_ms_if_exists_allocated,
                an_ms_if_exists);
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  db_space_used("block", num_blocks_allocated, a_block);
  db_space_used("for_loop", num_for_loops_allocated, a_for_loop);
  db_space_used("statement", num_statements_allocated, a_statement);
  db_space_used("constructor init", num_constructor_inits_allocated,
                a_constructor_init);
  db_space_used("pragma", num_pragmas_allocated, a_pragma);
  db_space_used("object lifetime", num_object_lifetimes_allocated,
                an_object_lifetime);
  db_space_used("namespace", num_namespaces_allocated, a_namespace);
  db_space_used("using-decl", num_using_decls_allocated, a_using_decl);
  db_space_used("scope", num_scopes_allocated, a_scope);
  db_space_used("local-scope-refs", num_local_scope_refs_allocated,
                a_local_scope_ref);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  db_space_used("source sequence entry", num_source_sequence_entries_allocated,
                a_source_sequence_entry);
  db_space_used("src-seq secondary decl",
                num_src_seq_secondary_decls_allocated,
                a_src_seq_secondary_decl);
  db_space_used("src-seq end of construct",
                num_src_seq_end_of_constructs_allocated,
                a_src_seq_end_of_construct);
  db_space_used("src-seq sublist", num_src_seq_sublists_allocated,
                a_src_seq_sublist);
  db_space_used("instantiation_directive",
                num_instantiation_directives_allocated,
                an_instantiation_directive);
  db_space_used("static-assertion", num_static_assertions, a_static_assertion);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL
  db_space_used("hidden names", num_hidden_names_allocated, a_hidden_name);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  db_space_used("template_parameters", num_template_parameters_allocated,
                a_template_parameter);
  db_space_used("template_decls", num_template_decls_allocated,
                a_template_decl);
  db_space_used("templates", num_templates_allocated, a_template);
  db_space_used("name references", num_name_references_allocated,
                a_name_reference);
  db_space_used("name qualifiers", num_name_qualifiers_allocated,
                a_name_qualifier);
#if RECORD_MACROS_IN_IL
  db_space_used("macros", num_macros_allocated, a_macro);
#endif /* RECORD_MACROS_IN_IL */
#if RECORD_MACRO_INVOCATIONS
  db_space_used("macro_invocation_record_blocks",
                num_macro_invocation_record_blocks_allocated,
                a_macro_invocation_record_block);
#endif /* RECORD_MACRO_INVOCATIONS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  db_space_used("decl-position supplement",
                num_decl_position_supplements_allocated,
                a_decl_position_supplement);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
  db_space_used("per inst needed flags",
                num_per_instantiation_needed_flags_entries_allocated,
                a_per_instantiation_needed_flags_entry);
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  db_space_used("orphaned list headers",
                num_scope_orphaned_list_headers_allocated,
                a_scope_orphaned_list_header);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if ORPHAN_PROCESSING_NEEDED
  db_space_used_nontype("fs orphan pointers", num_fs_orphan_pointers_allocated,
                        SPACE_FOR_FS_ORPHAN_POINTER);
#endif /* ORPHAN_PROCESSING_NEEDED */
  db_space_used_nontype("trans. unit copy addr.",
                        num_trans_unit_copy_address_pointers_allocated,
                        SPACE_FOR_TRANS_UNIT_COPY_ADDRESS_POINTER);
  db_space_used("IL entry prefix", num_il_entry_prefixes_allocated,
                an_il_entry_prefix);
#if ASM_SUPPORT_NEEDED
  db_space_used_other("asm function bodies",
                      asm_function_body_space_allocated, "");
#endif /* ASM_SUPPORT_NEEDED */
  db_space_used("attribute", num_attributes_allocated, an_attribute);
  db_space_used("attribute arg", num_attribute_args_allocated,
                an_attribute_arg);
  db_space_used("attribute group", num_attribute_groups_allocated,
                an_attribute_group);

  db_space_used_total();

  return grand_total;
}  /* show_il_alloc_space_used */
#endif /* DEBUG */

#if CHECKING && defined(offsetof)

static void check_host_alignment_parameters(void)
/*
Make sure that the host alignment macros are set to the appropriate values.

If you want to use an alignment value larger than what is actually required,
you will need to modify or remove these tests.
*/
{
  int	expected;
  struct pointer_alignment_test {
    char	dummy;  /*lint -esym(754, pointer_alignment_test::dummy)*/
    void	*ptr;
  };
  struct il_entry_prefix_alignment_test {
    char	dummy;
		/*lint -esym(754, il_entry_prefix_alignment_test::dummy)*/
    an_il_entry_prefix
		prefix;
  };
  struct host_alignment_test {
    char	dummy;  /*lint -esym(754, host_alignment_test::dummy)*/
    a_constant	constant;
  };
  expected = offsetof(struct host_alignment_test, constant);  /*lint !e413*/
  if (expected != HOST_ALIGNMENT_REQUIRED) {
    fprintf(f_error, "Expected HOST_ALIGNMENT_REQUIRED is %d\n", expected);
    internal_error(
    "check_host_alignment...: HOST_ALIGNMENT_REQUIRED set incorrectly");
  }  /* if */
  expected = offsetof(struct pointer_alignment_test, ptr);  /*lint !e413*/
  if (expected != HOST_POINTER_ALIGNMENT) {
    fprintf(f_error, "Expected HOST_POINTER_ALIGNMENT is %d\n", expected);
    internal_error(
    "check_host_alignment...: HOST_POINTER_ALIGNMENT set incorrectly");
  }  /* if */
  expected = offsetof(struct il_entry_prefix_alignment_test,
                      prefix);  /*lint !e413*/
  if (expected > HOST_IL_ENTRY_PREFIX_ALIGNMENT) {
    /* The specified alignment can be greater than or equal to the expected
       alignment.  This is required because the prefix alignment must be
       a multiple of the pointer alignment. */
    fprintf(f_error, "Expected HOST_IL_ENTRY_PREFIX_ALIGNMENT is %d\n",
            expected);
    internal_error(
    "check_host_alignment...: HOST_IL_ENTRY_PREFIX_ALIGNMENT set incorrectly");
  }  /* if */
}  /* check_host_alignment_parameters */
#endif /* CHECKING && defined(offsetof) */

void il_alloc_one_time_init(void)
/*
Do one-time initialization of variables related to the IL. (Variables
that need to be reinitialized with each new compilation are handled
in il_alloc_init.)
*/
{
  /* Set the default "routine name linkage", which is the value to which the
     routine_name_linkage field of a routine type supplement is initialized. */
  default_routine_name_linkage = C_mode() ?
                                   (a_name_linkage_kind)nlk_external :
                                   (a_name_linkage_kind)nlk_cplusplus_external;

  /* Set the default source correspondence variable to default values. */
  def_source_corresp.assoc_info = NULL;
  def_source_corresp.name = NULL;
#if NEED_NAME_MANGLING
  def_source_corresp.unmangled_name_or_mangled_encoding = NULL;
#endif /* NEED_NAME_MANGLING */
  def_source_corresp.trans_unit_corresp = NULL;
  def_source_corresp.parent_scope = NULL;
  def_source_corresp.enclosing_routine = NULL;
  def_source_corresp.decl_position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  def_source_corresp.decl_pos_info = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  def_source_corresp.name_references = NULL;
  /* access is set to "public" because "no access restriction" is the default
     for everything except class members.  For the latter the field must be
     set manually. */
  def_source_corresp.access = (an_access_specifier)as_public;
#if MICROSOFT_EXTENSIONS_ALLOWED
  def_source_corresp.assembly_access = (an_access_specifier)as_public;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* referenced is set TRUE because initially the entity is not associated
     with one in the source program.  All unassociated entities are assumed
     to be referenced (otherwise, they wouldn't be created).  This does away
     with the difficult job of setting the referenced flag in a lot of
     different places for unassociated entities. set_source_corresp resets
     the flag to FALSE for associated entities, for which the flag is then
     set to TRUE (for an actual reference) by record_symbol_reference. */
  def_source_corresp.referenced = TRUE;
#if MAINTAIN_NEEDED_FLAGS
  def_source_corresp.needed = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  def_source_corresp.name_linkage = (a_name_linkage_kind)nlk_none;
  def_source_corresp.has_associated_pragma = FALSE;
  def_source_corresp.is_local_to_function = FALSE;
  def_source_corresp.parent_via_local_scope_ref = FALSE;
  def_source_corresp.is_class_member = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  def_source_corresp.has_associated_attribute = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEED_NAME_MANGLING
  def_source_corresp.name_has_been_mangled = FALSE;
  def_source_corresp.mangled_name_cannot_be_included_in_other_name = FALSE;
  def_source_corresp.final_name_mangling_pending = FALSE;
  def_source_corresp.unnamed_entity_given_fabricated_name = FALSE;
#endif /* NEED_NAME_MANGLING */
#if BACK_END_IS_CP_GEN_BE
  def_source_corresp.qualification_needed = FALSE;
  def_source_corresp.partially_hidden_by_microsoft_injected_class_name = FALSE;
  def_source_corresp.visible_as_unqualified_name = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  def_source_corresp.is_decl_after_first_in_comma_list = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  def_source_corresp.static_used_by_instantiation = FALSE;
#if DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
  def_source_corresp.duplicate_static_in_instantiation_slices = FALSE;
#endif /* DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if MAINTAIN_NEEDED_FLAGS
  def_source_corresp.okay_to_walk_subtree_of_local_entity = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  def_source_corresp.copied_from_secondary_trans_unit = FALSE;
  def_source_corresp.same_name_as_external_entity_in_secondary_trans_unit =
                                                                        FALSE;
  def_source_corresp.member_of_unknown_base = FALSE;
  def_source_corresp.qualified_unknown_base_member = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  def_source_corresp.member_of_unknown_super = FALSE;
  def_source_corresp.microsoft_identifier_used = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED && GENERATE_SOURCE_SEQUENCE_LISTS
  def_source_corresp.marked_as_gnu_extension = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED && GENERATE_SOURCE_SEQUENCE_LISTS */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  def_source_corresp.is_deprecated = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
  def_source_corresp.externalized = FALSE;
#if RECORD_SCOPE_DEPTH_IN_IL
  def_source_corresp.scope_depth = NO_SCOPE_DEPTH;
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  def_source_corresp.source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  def_source_corresp.per_instantiation_needed_flags = NULL;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  def_source_corresp.attributes = NULL;

#if CHECKING && defined(offsetof)
  /* Make sure the host alignment macros are set properly. */
  check_host_alignment_parameters();
#endif /* CHECKING && defined(offsetof) */

  /* Save static variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_template_args),
#if DEBUG
#if !ABI_CHANGES_FOR_RTTI
      pch_saved_var_array_elem(num_accessible_base_classes_allocated),
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
      pch_saved_var_array_elem(num_eh_prologue_supplements_allocated),
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
      pch_saved_var_array_elem(num_asm_entries_allocated),
      pch_saved_var_array_elem(num_base_class_derivations_allocated),
      pch_saved_var_array_elem(num_base_classes_allocated),
      pch_saved_var_array_elem(num_based_type_list_members_allocated),
      pch_saved_var_array_elem(num_blocks_allocated),
      pch_saved_var_array_elem(num_class_list_entries_allocated),
      pch_saved_var_array_elem(num_class_type_supplements_allocated),
      pch_saved_var_array_elem(num_constants_allocated),
      pch_saved_var_array_elem(num_constructor_inits_allocated),
      pch_saved_var_array_elem(num_derivation_steps_allocated),
      pch_saved_var_array_elem(num_dynamic_inits_allocated),
      pch_saved_var_array_elem(num_local_static_variable_inits_allocated),
      pch_saved_var_array_elem(num_vla_dimensions_allocated),
      pch_saved_var_array_elem(num_exception_specification_types_allocated),
      pch_saved_var_array_elem(num_exception_specifications_allocated),
      pch_saved_var_array_elem(num_expr_nodes_allocated),
      pch_saved_var_array_elem(num_fields_allocated),
      pch_saved_var_array_elem(num_for_loops_allocated),
      pch_saved_var_array_elem(num_handlers_allocated),
      pch_saved_var_array_elem(num_try_supplements_allocated),
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(num_microsoft_try_supplements_allocated),
      pch_saved_var_array_elem(num_ms_attributes_allocated),
      pch_saved_var_array_elem(num_ms_attribute_args_allocated),
      pch_saved_var_array_elem(num_property_index_types_allocated),
      pch_saved_var_array_elem(num_property_or_event_descriptions_allocated),
      pch_saved_var_array_elem(num_generic_constraints_allocated),
      pch_saved_var_array_elem(num_generic_constraint_clauses_allocated),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
      pch_saved_var_array_elem(num_ms_if_exists_allocated),
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
      pch_saved_var_array_elem(num_seq_number_lookup_entries_allocated),
      pch_saved_var_array_elem(num_il_entry_prefixes_allocated),
      pch_saved_var_array_elem(num_labels_allocated),
      pch_saved_var_array_elem(num_new_delete_supplements_allocated),
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(num_gcnew_supplements_allocated),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pch_saved_var_array_elem(num_overriding_virtual_functions_allocated),
      pch_saved_var_array_elem(num_param_types_allocated),
      pch_saved_var_array_elem(num_pragmas_allocated),
      pch_saved_var_array_elem(num_routine_list_entries_allocated),
      pch_saved_var_array_elem(num_routine_type_supplements_allocated),
      pch_saved_var_array_elem(num_routines_allocated),
      pch_saved_var_array_elem(num_object_lifetimes_allocated),
      pch_saved_var_array_elem(num_namespaces_allocated),
      pch_saved_var_array_elem(num_using_decls_allocated),
      pch_saved_var_array_elem(num_scopes_allocated),
      pch_saved_var_array_elem(num_source_files_allocated),
      pch_saved_var_array_elem(num_statements_allocated),
      pch_saved_var_array_elem(num_switch_case_entries_allocated),
      pch_saved_var_array_elem(num_switch_stmt_descriptions_allocated),
      pch_saved_var_array_elem(num_template_args_allocated),
      pch_saved_var_array_elem(num_template_param_type_supplements_allocated),
      pch_saved_var_array_elem(num_typeref_type_supplements_allocated),
      pch_saved_var_array_elem(num_integer_type_supplements_allocated),
      pch_saved_var_array_elem(num_throw_supplements_allocated),
      pch_saved_var_array_elem(num_condition_supplements_allocated),
      pch_saved_var_array_elem(num_types_allocated),
      pch_saved_var_array_elem(num_variables_allocated),
      pch_saved_var_array_elem(string_literal_text_space_allocated),
#if GENERATE_SOURCE_SEQUENCE_LISTS
      pch_saved_var_array_elem(num_source_sequence_entries_allocated),
      pch_saved_var_array_elem(num_src_seq_secondary_decls_allocated),
      pch_saved_var_array_elem(num_src_seq_end_of_constructs_allocated),
      pch_saved_var_array_elem(num_src_seq_sublists_allocated),
      pch_saved_var_array_elem(num_instantiation_directives_allocated),
      pch_saved_var_array_elem(num_static_assertions),
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      pch_saved_var_array_elem(num_trans_unit_copy_address_pointers_allocated),
#if ORPHAN_PROCESSING_NEEDED
      pch_saved_var_array_elem(num_fs_orphan_pointers_allocated),
#endif /* ORPHAN_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
      pch_saved_var_array_elem(num_scope_orphaned_list_headers_allocated),
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_HIDDEN_NAMES_IN_IL
      pch_saved_var_array_elem(num_hidden_names_allocated),
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
      pch_saved_var_array_elem(num_template_parameters_allocated),
      pch_saved_var_array_elem(num_template_decls_allocated),
      pch_saved_var_array_elem(num_templates_allocated),
      pch_saved_var_array_elem(num_name_references_allocated),
      pch_saved_var_array_elem(num_name_qualifiers_allocated),
#if RECORD_MACROS_IN_IL
      pch_saved_var_array_elem(num_macros_allocated),
#endif /* RECORD_MACROS_IN_IL */
#if RECORD_MACRO_INVOCATIONS
      pch_saved_var_array_elem(num_macro_invocation_record_blocks_allocated),
#endif /* RECORD_MACRO_INVOCATIONS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      pch_saved_var_array_elem(num_decl_position_supplements_allocated),
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
      pch_saved_var_array_elem(
                         num_per_instantiation_needed_flags_entries_allocated),
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if ASM_SUPPORT_NEEDED
      pch_saved_var_array_elem(asm_function_body_space_allocated),
#endif /* ASM_SUPPORT_NEEDED */
      pch_saved_var_array_elem(num_il_entity_list_entries_allocated),
      pch_saved_var_array_elem(num_attributes_allocated),
      pch_saved_var_array_elem(num_attribute_args_allocated),
      pch_saved_var_array_elem(num_attribute_groups_allocated),
#endif /* if DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  register_trans_unit_variable(file_scope_entry_prefix_size);
  register_trans_unit_variable(avail_template_args);
  register_trans_unit_variable(file_scope_entry_prefix_alignment_offset);
}  /* il_alloc_one_time_init */


void compute_il_prefix_size(void)
/*
Compute the size of the IL entry prefix for file scope IL entries in this
translation unit.  On the initial call, also compute the prefix size
for non-file-scope entities.
*/
{
  int	aligned_size;

  /* All entries allocated in the file scope have a prefix.  If we are
     doing orphan processing, they also have an orphan pointer.  In
     secondary translation units they also have a translation unit
     copy address pointer. */
  file_scope_entry_prefix_size =
            (is_primary_translation_unit ? 0
                                : SPACE_FOR_TRANS_UNIT_COPY_ADDRESS_POINTER) +
#if ORPHAN_PROCESSING_NEEDED
            SPACE_FOR_FS_ORPHAN_POINTER +
#endif /* ORPHAN_PROCESSING_NEEDED */
            SPACE_FOR_IL_ENTRY_PREFIX;
  /* Compute the additional space required so that the prefix is a multiple
     of the host alignment that is required. */
  aligned_size = file_scope_entry_prefix_size;
  do_host_alignment(aligned_size);
  file_scope_entry_prefix_alignment_offset = aligned_size - 
                                             file_scope_entry_prefix_size;
  /* Set the prefix size to the aligned size. */
  file_scope_entry_prefix_size = aligned_size;
  /* Compute the size of the non-file-scope entry prefix and the associated
     alignment offset. */
  if (is_primary_translation_unit) {
    non_file_scope_entry_prefix_size = SPACE_FOR_IL_ENTRY_PREFIX;
    do_host_alignment(non_file_scope_entry_prefix_size);
    non_file_scope_entry_prefix_alignment_offset =
                  non_file_scope_entry_prefix_size - SPACE_FOR_IL_ENTRY_PREFIX;
  }  /* if */
}  /* compute_il_prefix_size */


void il_alloc_trans_unit_init(void)
/*
Initialize static variables related to IL allocation.  These are variables
that need initialization for every (primary and secondary) translation unit.
*/
{
  avail_template_args = NULL;
}  /* il_alloc_trans_unit_init */


void il_alloc_init(void)
/*
Initialize static variables related to IL allocation.  These are
initializations that are done for each compilation.
*/
{
  /* Static variables. */
#if DEBUG
  num_source_files_allocated             = 0;
  num_constants_allocated                = 0;
  num_param_types_allocated              = 0;
  num_routine_type_supplements_allocated = 0;
  num_based_type_list_members_allocated  = 0;
  num_class_type_supplements_allocated   = 0;
  num_class_list_entries_allocated       = 0;
  num_routine_list_entries_allocated     = 0;
  num_overriding_virtual_functions_allocated
                                         = 0;
  num_derivation_steps_allocated         = 0;
  num_base_class_derivations_allocated   = 0;
  num_base_classes_allocated             = 0;
  num_template_args_allocated            = 0;
  num_template_param_type_supplements_allocated
                                         = 0;
  num_typeref_type_supplements_allocated = 0;
  num_integer_type_supplements_allocated = 0;
  num_types_allocated                    = 0;
  num_dynamic_inits_allocated            = 0;
  num_local_static_variable_inits_allocated
                                         = 0;
  num_vla_dimensions_allocated           = 0;
  num_variables_allocated                = 0;
  num_fields_allocated                   = 0;
  num_routines_allocated                 = 0;
  num_exception_specifications_allocated = 0;
  num_exception_specification_types_allocated
                                         = 0;
  num_asm_entries_allocated              = 0;
  num_labels_allocated                   = 0;
  num_expr_nodes_allocated               = 0;
  num_new_delete_supplements_allocated   = 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  num_gcnew_supplements_allocated        = 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  num_throw_supplements_allocated        = 0;
  num_condition_supplements_allocated    = 0;
#if !ABI_CHANGES_FOR_RTTI
  num_accessible_base_classes_allocated  = 0;
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  num_eh_prologue_supplements_allocated  = 0;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
  num_switch_case_entries_allocated      = 0;
  num_switch_stmt_descriptions_allocated = 0;
  num_handlers_allocated                 = 0;
  num_try_supplements_allocated          = 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  num_microsoft_try_supplements_allocated= 0;
  num_ms_attributes_allocated            = 0;
  num_ms_attribute_args_allocated        = 0;
  num_property_index_types_allocated     = 0;
  num_property_or_event_descriptions_allocated
                                         = 0;
  num_generic_constraints_allocated      = 0;
  num_generic_constraint_clauses_allocated
                                         = 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  num_ms_if_exists_allocated             = 0;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  num_seq_number_lookup_entries_allocated
                                         = 0;
  num_blocks_allocated                   = 0;
  num_for_loops_allocated                = 0;
  num_statements_allocated               = 0;
  num_constructor_inits_allocated        = 0;
  num_pragmas_allocated                  = 0;
  num_object_lifetimes_allocated         = 0;
  num_namespaces_allocated               = 0;
  num_using_decls_allocated              = 0;
  num_scopes_allocated                   = 0;
  num_local_scope_refs_allocated         = 0;
  num_il_entry_prefixes_allocated        = 0;
  string_literal_text_space_allocated    = 0;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  num_source_sequence_entries_allocated  = 0;
  num_src_seq_secondary_decls_allocated  = 0;
  num_src_seq_end_of_constructs_allocated
                                         = 0;
  num_src_seq_sublists_allocated         = 0;
  num_instantiation_directives_allocated = 0;
  num_static_assertions                  = 0;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  num_trans_unit_copy_address_pointers_allocated = 0;
#if ORPHAN_PROCESSING_NEEDED
  num_fs_orphan_pointers_allocated       = 0;
#endif /* ORPHAN_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  num_scope_orphaned_list_headers_allocated = 0;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_HIDDEN_NAMES_IN_IL
  num_hidden_names_allocated             = 0;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  num_template_parameters_allocated      = 0;
  num_template_decls_allocated           = 0;
  num_templates_allocated                = 0;
  num_name_references_allocated          = 0;
  num_name_qualifiers_allocated          = 0;
#if RECORD_MACROS_IN_IL
  num_macros_allocated                   = 0;
#endif /* RECORD_MACROS_IN_IL */
#if RECORD_MACRO_INVOCATIONS
  num_macro_invocation_record_blocks_allocated
                                         = 0;
#endif /* RECORD_MACRO_INVOCATIONS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  num_decl_position_supplements_allocated = 0;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
  num_per_instantiation_needed_flags_entries_allocated = 0;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if ASM_SUPPORT_NEEDED
  asm_function_body_space_allocated      = 0;
#endif /* ASM_SUPPORT_NEEDED */
  num_il_entity_list_entries_allocated   = 0;
#endif /* DEBUG */
}  /* il_alloc_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
