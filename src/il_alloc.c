/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
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
		num_class_member_using_decls_allocated,
		num_class_list_entries_allocated,
		num_routine_list_entries_allocated,
		num_overriding_virtual_functions_allocated,
		num_derivation_steps_allocated,
		num_base_class_derivations_allocated,
		num_base_classes_allocated,
		num_template_args_allocated,
		num_template_param_type_descrs_allocated,
		num_types_allocated,
		num_dynamic_inits_allocated,
		num_local_static_variable_inits_allocated,
		num_variables_allocated,
		num_fields_allocated,
		num_routines_allocated,
		num_exception_specifications_allocated,
		num_exception_specification_types_allocated,
		num_asm_entries_allocated,
		num_labels_allocated,
		num_expr_nodes_allocated,
		num_new_delete_supplements_allocated,
		num_throw_supplements_allocated,
		num_condition_supplements_allocated,
		num_accessible_base_classes_allocated,
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
		num_eh_prologue_supplements_allocated,
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
		num_switch_clauses_allocated,
		num_handlers_allocated,
		num_try_supplements_allocated,
#if MICROSOFT_EXTENSIONS_ALLOWED
		num_microsoft_try_supplements_allocated,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
		num_blocks_allocated,
		num_for_loops_allocated,
		num_statements_allocated,
		num_constructor_inits_allocated,
		num_pragmas_allocated,
		num_object_lifetimes_allocated,
		num_namespaces_allocated,
		num_using_directives_allocated,
		num_scopes_allocated,
		num_il_entry_prefixes_allocated,
		string_literal_text_space_allocated;
#if GENERATE_SOURCE_SEQUENCE_LISTS
static unsigned long
		num_source_sequence_entries_allocated,
		num_src_seq_secondary_decls_allocated,
		num_src_seq_end_of_constructs_allocated,
		num_src_seq_sublists_allocated;
#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
static unsigned long
		num_comments_allocated;
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
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
#if RECORD_TEMPLATES_IN_IL
static unsigned long
		num_templates_allocated;
#endif /* RECORD_TEMPLATES_IN_IL */
#if RECORD_MACROS_IN_IL
static unsigned long
		num_macros_allocated;
#endif /* RECORD_MACROS_IN_IL */
#if ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
static unsigned long
		asm_function_body_space_allocated;
#endif /* ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* DEBUG */

/* Static variable and macro for quickly initializing the source_corresp
   field of an IL entry to default values. */
static a_source_correspondence
		def_source_corresp;
#define set_default_source_corresp(sc) (sc) = def_source_corresp;


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
region memory_region.  file_scope is TRUE if the allocation is in the
file scope.  (Yes, that could be determined from region_number, but it
happens that it is usually known by the caller).
*/
#define do_alloc(ptr, region_number, file_scope, size)                \
{ ptr = alloc_in_region((region_number),                              \
                         (sizeof_t)((size)+SPACE_FOR_IL_ENTRY_PREFIX)); \
  incr_num_il_entry_prefixes_allocated();                             \
  clear_il_entry_prefix(ptr, file_scope);                             \
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
Allocate a file-scope IL entry of size "size" preceded by an_il_entry_prefix
and (if appropriate) an orphan list pointer, and initialize the prefix
and orphan pointer to default values.  ptr is a "char *" pointer and is set
to point to the entry proper.
*/
#if ORPHAN_PROCESSING_NEEDED
/*
When orphan processing is needed, also allocate space for the
next-orphaned-entry pointer preceding the entry and the entry prefix.
*/
#define do_fs_alloc(ptr, size)                                        \
{ ptr = alloc_in_region(FILE_SCOPE_REGION_NUMBER,                     \
                        (sizeof_t)((size) +                           \
                                   SPACE_FOR_FS_ORPHAN_POINTER +      \
                                   SPACE_FOR_IL_ENTRY_PREFIX));       \
  incr_num_fs_orphan_pointers_allocated();                            \
  *(char **)ptr = NULL;                                               \
  ptr += SPACE_FOR_FS_ORPHAN_POINTER;                                 \
  incr_num_il_entry_prefixes_allocated();                             \
  clear_il_entry_prefix(ptr, TRUE);                                   \
  ptr += SPACE_FOR_IL_ENTRY_PREFIX;                                   \
}  /* do_fs_alloc */
#else /* !ORPHAN_PROCESSING_NEEDED */
/* When orphan processing is not needed, file-scope allocation is like
   allocation in any other memory region. */
#define do_fs_alloc(ptr, size)                                        \
  do_alloc((ptr), FILE_SCOPE_REGION_NUMBER, TRUE, (size))
#endif /* ORPHAN_PROCESSING_NEEDED */


/*
Allocate space in an arbitrary memory region (i.e., choose between the
file-scope and normal allocation methods as necessary).
*/
#if ORPHAN_PROCESSING_NEEDED
#define do_any_alloc(ptr, region_number, size)                        \
{ if ((region_number) == FILE_SCOPE_REGION_NUMBER) {                  \
    do_fs_alloc((ptr), (size));                                       \
  } else {                                                            \
    do_alloc((ptr), (region_number), FALSE, (size));                  \
  }  /* if */                                                         \
}  /* do_any_alloc */
#else /* !ORPHAN_PROCESSING_NEEDED */
/* No space needed for next-orphan pointer.  Allocation in the file scope
   is the same as allocation in any other region. */
#define do_any_alloc(ptr, region_number, size)                        \
  do_alloc((ptr), (region_number),                                    \
           ((region_number) == FILE_SCOPE_REGION_NUMBER), (size))
#endif /* ORPHAN_PROCESSING_NEEDED */


char *alloc_il(sizeof_t size)
/*
Allocate and return "size" bytes of storage in the file scope memory region.
*/
{
  char *ptr;
  do_fs_alloc(ptr, size);
  return ptr;
}  /* alloc_il */

#if !STANDALONE_UTILITY_PROGRAM

static char *alloc_cil(sizeof_t size)
/*
Allocate and return "size" bytes of storage in the current IL memory region.
*/
{
  char *ptr;
  do_any_alloc(ptr, curr_il_region_number, size);
  return ptr;
}  /* alloc_cil */


char *alloc_text_of_string_literal(sizeof_t size)
/*
Allocate space for the text of a string literal, and return a pointer to it.
The space allocated is large enough to contain "size" characters.
This routine exists as a way of tracking the space use.  The space is always
allocated at the file scope, because (a) string values can be shared (at
least in non-pcc mode), and (b) strings that are values of manifest constant
macros need to be there.
*/
{
#if DEBUG
  string_literal_text_space_allocated += size;
#endif /* DEBUG */
  return alloc_il(size);
}  /* alloc_text_of_string_literal */

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

  sfp = (a_source_file_ptr)alloc_il(sizeof(a_source_file));
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
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  sfp->is_include_file = FALSE;
  sfp->included_by_system_include = FALSE;

  return sfp;
}  /* alloc_source_file */


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
    case ck_integer:
      set_integer_value(&cp->variant.integer_value, 0L);
      break;
    case ck_string:
      cp->variant.string.length = 0;
      cp->variant.string.value = NULL;
      break;
    case ck_float:
      /* The entire float_value must be zeroed to allow use of memcmp
         and the like on the field. */
      memzero((char *)&cp->variant.float_value,
              sizeof(cp->variant.float_value));
      break;
    case ck_address:
      cp->variant.address.kind = (an_address_base_kind)abk_variable;
      cp->variant.address.variant.variable = NULL;
      cp->variant.address.offset = 0;
      break;
    case ck_ptr_to_member:
      cp->variant.ptr_to_member.casting_base_class = NULL;
      cp->variant.ptr_to_member.cast_to_base    = FALSE;
      cp->variant.ptr_to_member.is_function_ptr = FALSE;
#if CHECKING
      cp->variant.ptr_to_member.avoid_codecenter_warnings = 0;
#endif /* CHECKING */
      cp->variant.ptr_to_member.variant.field   = NULL;
      break;
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
      break;
    case ck_template_param:
      cp->variant.template_param.kind =
                                  (a_template_param_constant_kind)tpck_param;
      cp->variant.template_param.variant.list_position = 0;
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
  cp->implicit_cast  = FALSE;
  cp->non_arithmetic = FALSE;
  cp->is_simple_zero = FALSE;
#if DO_IL_LOWERING || BACK_END_IS_C_GEN_BE
  cp->assoc_var_assigned = FALSE;
#endif /* DO_IL_LOWERING || BACK_END_IS_C_GEN_BE */
  cp->null_pointer_constant_ruled_out = FALSE;
#if CHECKING
  cp->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
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
at file scope.
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
  ptp->passed_via_copy_constructor = FALSE;
  ptp->has_default_arg = FALSE;
  ptp->type_involves_template_param = FALSE;
  ptp->qualifiers = TQ_NONE;
#if CHECKING
  ptp->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
  ptp->default_arg_expr = NULL;

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
#if CHECKING
  bcdp->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
  db_exit();
  return bcdp;
}  /* alloc_base_class_derivation */


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
  ovfp->next                = NULL;
  ovfp->overriding_function = NULL;
  ovfp->primary_function    = NULL;
  ovfp->base_class          = NULL;

  return ovfp;
}  /* alloc_overriding_virtual_function */

static a_template_arg_ptr
		avail_template_args;
			/* List of freed template arg entries that are
			   available for reuse. */


a_template_arg_ptr alloc_template_arg(a_boolean is_type_arg)
/*
Allocate a template argument entry, initialize its fields, and return
a pointer to it.
*/
{
  a_template_arg_ptr tap;

  if (avail_template_args != NULL) {
    tap = avail_template_args;
    avail_template_args = avail_template_args->next;
  } else {
    tap = (a_template_arg_ptr)alloc_il(sizeof(a_template_arg));
#if DEBUG
    num_template_args_allocated++;
#endif /* DEBUG */
  }  /* if */
  tap->next             = NULL;
  tap->is_type          = is_type_arg;
  tap->is_array_bound_of_unknown_type = FALSE;
#if CHECKING
  tap->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
  if (is_type_arg) {
    tap->variant.type     = NULL;
  } else {
    tap->variant.constant = NULL;
  }  /* if */
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


a_template_param_type_descr_ptr alloc_template_param_type_descr(void)
/*
Allocate a template parameter type description entry, initialize its fields,
and return a pointer to it.
*/
{
  a_template_param_type_descr_ptr tptdp;

  tptdp = (a_template_param_type_descr_ptr)alloc_il(
                                       sizeof(a_template_param_type_descr));
#if DEBUG
  num_template_param_type_descrs_allocated++;
#endif /* DEBUG */
  tptdp->class_type = NULL;
  tptdp->tag_kind = (a_type_kind)tk_unknown;
  return tptdp;
}  /* alloc_template_param_type_descr */


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
  bcp->type                            = NULL;
  bcp->derived_class                   = NULL;
  bcp->decl_position                   = null_source_position;
  bcp->is_virtual                      = FALSE;
  bcp->direct                          = FALSE;
  bcp->ambiguous                       = FALSE;
  bcp->shares_virtual_function_info    = FALSE;
  bcp->offset                          = 0;
  bcp->pointer_offset                  = 0;
  bcp->pointer_base_class              = NULL;
  bcp->derivation                      = NULL;
  bcp->overriding_virtual_functions    = NULL;
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  bcp->complete_subobject              = FALSE;
  bcp->pointer_offset_is_set           = FALSE;
  bcp->data_section_base_class         = NULL;
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#if DO_IL_LOWERING
  bcp->virtual_function_table_var      = NULL;
#endif /* DO_IL_LOWERING */
#if CHECKING
  bcp->avoid_codecenter_warnings       = 0;
#endif /* CHECKING */

  return bcp;
}  /* alloc_base_class */


a_class_member_using_decl_ptr alloc_class_member_using_decl(
                                                       an_il_entry_kind kind)
/*
Allocate a class-member-using-decl entry, initialize its fields, and return a
pointer to it.
*/
{
  a_class_member_using_decl_ptr cmudp;

  cmudp = (a_class_member_using_decl_ptr)alloc_il(
                                           sizeof(a_class_member_using_decl));
#if DEBUG
  num_class_member_using_decls_allocated++;
#endif /* DEBUG */
  cmudp->next        = NULL;
  cmudp->access      = (an_access_specifier)as_public;
  cmudp->entity.kind = (a_byte_il_entry_kind)kind;
  cmudp->entity.ptr  = (char *)NULL;

  return cmudp;
}  /* alloc_class_member_using_decl */


a_class_list_entry_ptr alloc_list_entry_for_class(void)
/*
Allocate a class-list-entry, initialize its fields, and return a pointer to it.
*/
{
  a_class_list_entry_ptr clep;

  clep = (a_class_list_entry_ptr)alloc_il(sizeof(a_class_list_entry));
#if DEBUG
  num_class_list_entries_allocated++;
#endif /* DEBUG */
  clep->next  = NULL;
  clep->class_type = NULL;

  return clep;
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
                                               a_based_type_kind  kind)
/*
Allocate a based type list member, initialize it to the indicated kind, and
return a pointer to it.
*/
{
  a_based_type_list_member_ptr btlmp;

  btlmp = (a_based_type_list_member_ptr)alloc_il(
                                             sizeof(a_based_type_list_member));
#if DEBUG
  num_based_type_list_members_allocated++;
#endif /* DEBUG */
  btlmp->next = NULL;
  btlmp->based_type = NULL;
  btlmp->kind = kind;

  return btlmp;
}  /* alloc_based_type_list_member */


static a_class_type_supplement_ptr alloc_class_type_supplement(void)
/*
Allocate a class-type-supplement entry, initialize its fields, and return
a pointer to it.
*/
{
  a_class_type_supplement_ptr ctsp;

  ctsp = (a_class_type_supplement_ptr)alloc_il(
			      sizeof(a_class_type_supplement));
#if DEBUG
  num_class_type_supplements_allocated++;
#endif /* DEBUG */
  ctsp->base_classes                      = NULL;
  ctsp->size_without_virtual_base_classes = 0;
  ctsp->alignment_without_virtual_base_classes = 1;
  ctsp->highest_virtual_function_number   = 0;
  ctsp->virtual_function_info_offset      = 0;
  ctsp->virtual_function_info_base_class  = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  ctsp->decl_modifiers                    = DM_NONE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ctsp->anonymous_union_kind              = (an_anonymous_union_kind)auk_none;
  ctsp->anonymous_union_field             = NULL;
  ctsp->class_member_using_decls          = NULL;
  ctsp->befriending_classes               = NULL;
  ctsp->friend_routines                   = NULL;
  ctsp->friend_classes                    = NULL;
  ctsp->assoc_scope                       = NULL;
  ctsp->template_arg_list                 = NULL;
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  ctsp->assoc_operator_new_routine        = NULL;
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
  ctsp->assoc_operator_delete_routine     = NULL;
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if DO_IL_LOWERING
  ctsp->virtual_function_table_var        = NULL;
  ctsp->type_as_subobject                 = NULL;
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
  ctsp->promoted_local_types              = NULL;
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#endif /* DO_IL_LOWERING */

  return ctsp;
}  /* alloc_class_type_supplement */


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
      /* No variant fields to set. */
      break;
    case tk_integer:
      pte->variant.integer.int_kind = (an_integer_kind)ik_int;
      pte->variant.integer.explicitly_signed = FALSE;
      pte->variant.integer.enum_type = FALSE;
      pte->variant.integer.wchar_t_type = FALSE;
      pte->variant.integer.bool_type = FALSE;
#if CHECKING
      pte->variant.integer.avoid_codecenter_warnings = 0;
#endif /* CHECKING */
      pte->variant.integer.enum_info.affiliated_type = NULL;
      break;
    case tk_float:
      pte->variant.float_kind = (a_float_kind)fk_float;
      break;
    case tk_pointer:
      pte->variant.pointer.type = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      pte->variant.pointer.base_variable = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pte->variant.pointer.is_reference = FALSE;
      break;
    case tk_array:
      pte->variant.array.element_type = NULL;
      pte->variant.array.is_variable_size_array = FALSE;
      pte->variant.array.variant.number_of_elements = 0;
      break;
    case tk_class:
    case tk_struct:
    case tk_union:
      pte->variant.class_struct_union.field_list = NULL;
      pte->variant.class_struct_union.any_const_member = FALSE;
      pte->variant.class_struct_union.any_virtual_base_classes = FALSE;
      pte->variant.class_struct_union.abstract = FALSE;
      pte->variant.class_struct_union.any_virtual_functions = FALSE;
      pte->variant.class_struct_union.any_pure_virtual_functions = FALSE;
      pte->variant.class_struct_union.
                       any_virtual_functions_including_in_base_classes = FALSE;
      pte->variant.class_struct_union.
                       referenced_by_placeholder_typeref = FALSE;
      pte->variant.class_struct_union.originally_unnamed = FALSE;
      /* The class type supplement is only allocated in C++ mode. */
      pte->variant.class_struct_union.extra_info = 
                                           (C_dialect == C_dialect_cplusplus) ?
                                                alloc_class_type_supplement() :
                                                NULL;
#if USER_CONTROL_OF_STRUCT_PACKING
      pte->variant.class_struct_union.max_member_alignment = 0;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
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
      rtsp->lint_argsused_flag       = FALSE;
      rtsp->value_returned_by_cctor  = FALSE;
      rtsp->assoc_routine_is_ctor    = FALSE;
      rtsp->assoc_routine_is_dtor    = FALSE;
      rtsp->suppress_diagnostic_on_incomplete_return_type = FALSE;
      rtsp->routine_name_linkage     = default_routine_name_linkage;
#if CHECKING
      rtsp->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
      rtsp->lint_varargs_count       = NOT_LINT_VARARGS;
      rtsp->arg_pragma               = (a_pragma_kind)pk_none;
#if MICROSOFT_EXTENSIONS_ALLOWED
      rtsp->calling_convention       = (a_calling_convention)cc_default;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      rtsp->implicit_this_param_type = NULL;
      rtsp->prototype_scope          = NULL;
      rtsp->exception_specification  = NULL;
      break;
    case tk_typeref:
      pte->variant.typeref.type        = NULL;
#if DO_IL_LOWERING
      pte->variant.typeref.orig_type   = NULL;
#endif /* DO_IL_LOWERING */
      pte->variant.typeref.qualifiers  = TQ_NONE;
      pte->variant.typeref.is_placeholder_for_file_scope_type = FALSE;
      pte->variant.typeref.is_placeholder_for_namespace_type = FALSE;
#if CHECKING
      pte->variant.typeref.avoid_codecenter_warnings = 0;
#endif /* CHECKING */
      /* Clear size and alignment because they aren't used in typerefs. */
      pte->size = 0;
      pte->alignment = 1;
      break;
    case tk_ptr_to_member:
      pte->variant.ptr_to_member.class_of_which_a_member = FALSE;
      pte->variant.ptr_to_member.type                    = FALSE;
      break;
    case tk_template_param:
      pte->variant.template_param.kind =
                                   (a_template_param_type_kind)tptk_param;
      pte->variant.template_param.list_position = 0;
      pte->variant.template_param.descr = NULL;
      break;
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
  pte->used_in_exception = FALSE;
  pte->declared_in_function_prototype = FALSE;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  pte->use_cfront_transitional_nested_type_name_mangling = FALSE;
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if BACK_END_IS_C_GEN_BE
  pte->prototype_scope_types_if_any_promoted = FALSE;
#endif /* BACK_END_IS_C_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
  pte->declaration_put_out = FALSE;
  pte->definition_delayed = FALSE;
  pte->elaborated_type_specifier_needed = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  pte->autonomous_primary_tag_decl = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DO_IL_LOWERING
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
      break;
#if CHECKING
    default:
      internal_error("set_dynamic_init_kind: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* set_dynamic_init_kind */


void clear_dynamic_init(a_dynamic_init_ptr  dip,
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
  dip->follows_an_exec_statement     = FALSE;
  dip->inside_conditional_expression = FALSE;
  dip->unordered                     = FALSE;
  dip->has_temporary_lifetime        = FALSE;
  dip->is_constructor_init           = FALSE;
  dip->is_freeing_of_storage_on_exception = FALSE;
#if CHECKING
  dip->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
  set_dynamic_init_kind(dip, kind);
#if DO_IL_LOWERING
  dip->destructible_entity_descr     = NULL;
#endif /* DO_IL_LOWERING */
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
  db_exit();
  return lsvip;
}  /* alloc_local_static_variable_init */


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
  set_default_source_corresp(vp->source_corresp);
  vp->next                        = NULL;
  vp->type                        = NULL;
  vp->assoc_param_type            = NULL;
  vp->storage_class               = storage_class;
  vp->address_taken               = FALSE;
  vp->is_parameter                = FALSE;
  vp->init_kind                   = (an_init_kind)initk_none;
  /* One of the variant fields, chosen arbitrarily, is initialized. */
  vp->initializer.constant        = NULL;
#ifdef CIL
  vp->referenced_non_locally      = FALSE;
  vp->modified_within_try_block   = FALSE;
  vp->is_template_static_data_member
                                  = FALSE;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  vp->can_be_instantiated         = FALSE;
  vp->do_not_instantiate          = FALSE;
  vp->instance_required           = FALSE;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  vp->specific_def		  = FALSE;
  vp->param_value_has_been_changed= FALSE;
  vp->param_used_more_than_once   = FALSE;
  vp->is_handler_param            = FALSE;
  vp->is_this_parameter           = FALSE;
  vp->is_partially_initialized    = FALSE;
  vp->is_anonymous_parent_object  = FALSE;
  vp->is_member_constant          = FALSE;
  vp->superseded_external         = FALSE;
#if DO_IL_LOWERING
  vp->initialization_rewritten_as_assignment = FALSE;
#if MINIMAL_INLINING
  vp->is_temp_for_unmodified_inlined_param = FALSE;
  vp->is_temp_for_constructor_this_inlined_param = FALSE;
#endif /* MINIMAL INLINING */
#endif /* DO_IL_LOWERING */
#if DECL_MODIFIERS_IN_USE
  vp->decl_modifiers              = DM_NONE;
#endif /* DECL_MODIFIERS_IN_USE */
#endif /* ifdef CIL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  vp->declared_type               = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#ifdef FIL
  vp->by_address                  = FALSE;
  vp->base_var                    = NULL;
  vp->association_offset          = 0;
  vp->function_result_var_function= NULL;
#endif /* ifdef FIL */

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
  fp->is_bit_field         = FALSE;
  fp->bit_field_is_signed  = FALSE;
  fp->is_anonymous_parent_object = FALSE;
  fp->is_mutable           = FALSE;
#if CHECKING
  fp->avoid_codecenter_warnings = 0;
#endif /*CHECKING */

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
  esp->exception_specification_type_list = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  esp->throw_position = null_source_position;
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

  return estp;
}  /* alloc_exception_specification_type */


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
  rp->next                    = NULL;
  rp->type                    = NULL;
  rp->assoc_scope             = NULL_region_number;
  rp->storage_class           = (a_storage_class)sc_unspecified;
  rp->special_kind            = (a_special_function_kind)sfk_none;
  rp->opname_kind             = (an_opname_kind)onk_none;
  rp->address_taken           = FALSE;
  rp->is_virtual              = FALSE;
  rp->pure_virtual            = FALSE;
  rp->is_inline               = FALSE;
  rp->compiler_generated      = FALSE;
  rp->called                  = FALSE;
#if ASSIGNMENT_TO_THIS_ALLOWED
  rp->assignment_to_this_done = FALSE;
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  rp->is_template_function    = FALSE;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  rp->can_be_instantiated     = FALSE;
  rp->do_not_instantiate      = FALSE;
  rp->instance_required       = FALSE;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  rp->specific_def            = FALSE;
  rp->contains_try_block      = FALSE;
  rp->superseded_external     = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  rp->defined_in_friend_decl  = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if MINIMAL_INLINING
  rp->inlinable               = FALSE;
  rp->need_out_of_line_copy   = FALSE;
#endif /* MINIMAL_INLINING */
#if DECL_MODIFIERS_IN_USE
  rp->decl_modifiers              = DM_NONE;
#endif /* DECL_MODIFIERS_IN_USE */
  rp->befriending_classes     = NULL;
  rp->virtual_function_number = 0;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  rp->declared_type           = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#ifdef FIL
  rp->is_fortran_entry        = FALSE;
  rp->local_routine_scope     = NULL;
  rp->intrinsic_func_code     = (an_intrinsic_function_code)ifc_none;
#endif /* ifdef FIL */

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
  ap->is_asm_block = FALSE;
  db_exit();
  return ap;
}  /* alloc_asm_entry */

#if ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED

char *alloc_asm_function_body(sizeof_t  len)
/*
Allocate space for an asm function body and return a pointer to it.
*/
{
#if DEBUG
  asm_function_body_space_allocated += len;
#endif /* DEBUG */
  return (char *)alloc_cil(len);
}  /* alloc_asm_function_body */

#endif /* ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */

a_label_ptr alloc_label(void)
/*
Allocate a label entry, clear it to default values, and return a pointer
to it.
*/
{
  a_label_ptr lp;

  db_enter(5, "alloc_label");

  lp = (a_label_ptr)alloc_cil(sizeof(a_label));
#if DEBUG
  num_labels_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(lp->source_corresp);
  lp->source_corresp.is_local_to_function = TRUE;
  lp->next = NULL;
  lp->reachable_by_fall_through = TRUE;
  lp->break_label = FALSE;
  lp->continue_label = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  lp->leave_label = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if CHECKING
  lp->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
  lp->variant.exec_stmt = NULL;
#ifdef FIL
  lp->kind = (a_label_kind)lk_executable;
  lp->used_in_assign = FALSE;
#endif /* ifdef FIL */
#if MICROSOFT_EXTENSIONS_ALLOWED
  lp->num_microsoft_trys_inside_of = 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_exit();
  return lp;
}  /* alloc_label */


void set_expr_node_kind(an_expr_node_ptr  node,
                        an_expr_node_kind kind)
/*
Set the kind of the indicated expression node.  Also set associated variant
fields to default values.
*/
{
  a_new_delete_supplement_ptr ndsp;
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
      node->variant.operation.returns_lvalue_instead_of_usual_rvalue = FALSE;
      node->variant.operation.compiler_generated = FALSE;
      node->variant.operation.implicit_in_member_naming = FALSE;
#if CHECKING
      node->variant.operation.avoid_codecenter_warnings = 0;
#endif /* CHECKING */
      node->variant.operation.operands = NULL;
      break;
    case enk_constant:
      node->variant.constant = NULL;
      break;
    case enk_variable:
    case enk_variable_address:
      node->variant.variable = NULL;
      break;
    case enk_routine_address:
      node->variant.routine = NULL;
      break;
    case enk_field:
      node->variant.field = NULL;
      break;
    case enk_temp_init:
      node->variant.init.result_is_addr = FALSE;
      node->variant.init.static_temp    = FALSE;
#if CHECKING
      node->variant.init.avoid_codecenter_warnings = 0;
#endif /* CHECKING */
      node->variant.init.dynamic_init   = NULL;
      break;
    case enk_new_delete:
      /* Allocate the supplement for new/delete. */
      ndsp = (a_new_delete_supplement_ptr)
                                    alloc_cil(sizeof(a_new_delete_supplement));
      node->variant.new_delete = ndsp;
#if DEBUG
      num_new_delete_supplements_allocated++;
#endif /* DEBUG */
      ndsp->is_new       = TRUE;
      ndsp->placement_new= FALSE;
      ndsp->array_delete = FALSE;
      ndsp->type         = NULL;
      ndsp->routine      = NULL;
      ndsp->arg          = NULL;
      ndsp->dynamic_init = NULL;
      ndsp->freeing_of_storage_on_exception = NULL;
      break;
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
      tsp->accessible_base_classes = NULL;
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
      break;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    /* Nodes generated by IL lowering for partial lowering of exception
       handling features. */
    case enk_lowered_eh_construct:
      /* See set_lowered_eh_construct_node_kind. */
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
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
  node->result_is_not_used = FALSE;
  node->implicit_reference_indirection = FALSE;
#ifdef FIL
  node->allow_reordering = FALSE;
#endif /* ifdef FIL */
  node->is_initialization_guard = FALSE;
#if CHECKING
  node->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
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

static void set_lowered_eh_construct_node_kind(
                                              an_expr_node_ptr node,
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

a_switch_clause_ptr alloc_switch_clause(void)
/*
Allocate a switch clause, clear it to default values, and return a pointer
to it.
*/
{
  register a_switch_clause_ptr scp;

  scp = (a_switch_clause_ptr)alloc_cil(sizeof(a_switch_clause));
#if DEBUG
  num_switch_clauses_allocated++;
#endif /* DEBUG */
  scp->next                 = NULL;
  scp->constant_list        = NULL;
  scp->statements           = NULL;
  scp->implied_break_at_end = FALSE;
  clear_stmt_source_position(scp->break_position);
  return scp;
}  /* alloc_switch_clause */


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
  a_try_supplement_ptr tsp;

  sp->kind = stmt_kind;
  sp->expr = NULL;
  switch (stmt_kind) {
    case stmk_expr:
      /* No variant fields. */
      break;
    case stmk_if:
      sp->variant.if_stmt.then_statement =
          sp->variant.if_stmt.else_statement = NULL;
      break;
    case stmk_while:
    case stmk_end_test_while:
      sp->variant.loop_statement = NULL;
      break;
    case stmk_for:
      sp->variant.for_loop.statement = NULL;
      sp->variant.for_loop.extra_info = flip =
                      (a_for_loop_ptr)alloc_cil(sizeof(a_for_loop));
#if DEBUG
      num_for_loops_allocated++;
#endif /* DEBUG */
      flip->initialization = NULL;
      flip->increment = NULL;
      break;
    case stmk_switch:
      sp->variant.switch_stmt.clause_list    = NULL;
      sp->variant.switch_stmt.body_statement = NULL;
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
      tsp->statement = NULL;
      tsp->handlers  = NULL;
      tsp->lifetime  = NULL;
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
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    case stmk_decl:
      /* No variant fields. */
      break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
  sp->next                  = NULL;
  sp->has_associated_pragma = FALSE;
  sp->is_initialization_guard = FALSE;
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

  return cip;
}  /* alloc_ctor_init */


a_pragma_ptr alloc_pragma(a_pragma_kind  kind)
/*
Allocate a pragma entry of the required kind, initialize it, and return a
pointer to it.
*/
{
  a_pragma_ptr  pp;

  pp = (a_pragma_ptr)alloc_cil(sizeof(a_pragma));
#if DEBUG
  num_pragmas_allocated++;
#endif /* DEBUG */
  pp->next                  = NULL;
  pp->kind                  = kind;
  pp->ignore_in_back_end    = FALSE;
  pp->entity.kind           = (a_byte_il_entry_kind)iek_none;
  pp->entity.ptr            = NULL;
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
#if USER_CONTROL_OF_STRUCT_PACKING
    case pk_pack:
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if PRAGMA_WEAK_ALLOWED
    case pk_weak:
#endif /* PRAGMA_WEAK_ALLOWED */
#if INCLUDE_EDG_TEST_PRAGMAS
    case pk_test_next_statement:
    case pk_test_next_decl:
    case pk_test_immediate:
    case pk_test_other:
    case pk_test_bind_next_pass:
      pp->variant.dummy = 0;
#endif /* INCLUDE_EDG_TEST_PRAGMAS */
      break;
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
    case pk_define_type_info:
      break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
  if (curr_il_region_number == FILE_SCOPE_REGION_NUMBER) {
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
  olp->entity.kind                = (a_byte_il_entry_kind)iek_none;
  olp->entity.ptr                 = NULL;
  olp->kind                       = kind;
  olp->has_block_after_label_child_lifetime
                                  = FALSE;
  olp->destructions               = NULL;
  olp->parent_lifetime            = NULL;
  olp->parent_destruction_sublist = NULL;
  olp->child_lifetime             = NULL;
  olp->next                       = NULL;
  db_exit();
  return olp;
}  /* alloc_object_lifetime */


a_namespace_ptr alloc_namespace(a_boolean  is_alias)
/*
Allocate a namespace entry, initialize its fields, and return a pointer to
it.  The entry is allocated in the file scope memory region.
*/
{
  a_namespace_ptr nsp;

  db_enter(5, "alloc_namespace");
  nsp = (a_namespace_ptr)alloc_cil(sizeof(a_namespace));
#if DEBUG
  num_namespaces_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(nsp->source_corresp);
  nsp->next = NULL;
  nsp->is_namespace_alias = is_alias;
  if (is_alias) {
    nsp->variant.assoc_namespace = NULL;
  } else {
    nsp->variant.assoc_scope = NULL;
  }  /* if */
  db_exit();
  return nsp;
}  /* alloc_namespace */


a_using_directive_ptr alloc_using_directive(void)
/*
Allocate a using-directive entry, initialize its fields, and return a pointer
to it.
*/
{
  a_using_directive_ptr  udp;

  db_enter(5, "alloc_using_directive");
  udp = (a_using_directive_ptr)alloc_cil(sizeof(a_using_directive));
#if DEBUG
  num_using_directives_allocated++;
#endif /* DEBUG */
  udp->next = NULL;
  udp->assoc_namespace = NULL;
  udp->position = null_source_position;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  udp->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  db_exit();
  return udp;
}  /* alloc_using_directive */


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
      /* No variant fields. */
      break;
    case sck_block:
      sp->variant.assoc_handler = NULL;
      break;
    case sck_func_prototype:
    case sck_class_struct_union:
      sp->variant.assoc_type = NULL;
      break;
    case sck_function:
      sp->variant.routine.ptr                           = assoc_routine;
      sp->variant.routine.parameters                    = NULL;
      sp->variant.routine.constructor_inits             = NULL;
      sp->variant.routine.lifetime_of_local_static_vars = NULL;
      sp->variant.routine.this_param_variable           = NULL;
      sp->variant.routine.return_value_variable         = NULL;
#ifdef FIL
      sp->variant.routine.function_result_var = NULL;
#endif /* ifdef FIL */
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
  sp->number = number;
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
  sp->using_directives            = NULL;
  sp->dynamic_inits               = NULL;
  sp->local_static_variable_inits = NULL;
  sp->pragmas                     = NULL;
  sp->depth_in_scope_stack        = NO_SCOPE_DEPTH;
#ifdef FIL
  sp->entries                     = NULL;
  sp->namelist_groups             = NULL;
#endif /* ifdef FIL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sp->source_sequence_list        = NULL;
  sp->src_seq_sublist_list        = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL
  sp->hidden_names                = NULL;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if RECORD_TEMPLATES_IN_IL
  sp->templates                   = NULL;
#endif /* RECORD_TEMPLATES_IN_IL */

  db_exit();
  return sp;
}  /* alloc_scope */

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
  if (curr_il_region_number == FILE_SCOPE_REGION_NUMBER) {
    /* Use the file scope. */
    scope_depth = DEPTH_OF_FILE_SCOPE;
  } else {
    /* Use the current function scope. */
    check_assertion(depth_innermost_ss_list_scope != NO_SCOPE_DEPTH);
    scope_depth = depth_innermost_ss_list_scope;
  }  /* if */
  /* Copy the address of the available list. */
  avail_list_ptr = &scope_stack[scope_depth].source_sequence_avail_list;
  if (*avail_list_ptr != NULL) {
    ssep = *avail_list_ptr;
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
  ssep->entity.kind = (a_byte_il_entry_kind)iek_none;
  ssep->entity.ptr  = NULL;

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
  sssdp->decl_position = null_source_position;
  sssdp->entity.kind   = (a_byte_il_entry_kind)iek_none;
  sssdp->entity.ptr    = NULL;
  sssdp->declared_type = NULL;
  sssdp->autonomous_tag_decl  = FALSE;
  sssdp->friend_decl          = FALSE;
#if CHECKING
  sssdp->avoid_codecenter_warnings = 0;
#endif /* CHECKING */

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
  sseocp->source_position = null_source_position;
  sseocp->entity.kind     = (a_byte_il_entry_kind)iek_none;
  sseocp->entity.ptr      = NULL;

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

#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS

a_comment_ptr alloc_comment(void)
/*
Allocate a comment entry, initialize its fields, and return a pointer to it.
*/
{
  a_comment_ptr  cp;

  cp = (a_comment_ptr)alloc_cil(sizeof(a_comment));
#if DEBUG
  num_comments_allocated++;
#endif /* DEBUG */
  cp->range.start_position = null_source_position;
  cp->range.end_position   = null_source_position;

  return cp;
}  /* alloc_comment */

#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
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
  hnp->entity.kind                      = (a_byte_il_entry_kind)iek_none;
  hnp->entity.ptr                       = NULL;
  hnp->global_qualification_needed      = FALSE;
  hnp->elaborated_type_specifier_needed = FALSE;
#if CHECKING
  hnp->avoid_codecenter_warnings        = 0;
#endif /* CHECKING */

  return hnp;
}  /* alloc_hidden_name */

#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if RECORD_TEMPLATES_IN_IL

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
  tp->text = NULL;

  return tp;
}  /* alloc_template */

#endif /* RECORD_TEMPLATES_IN_IL */
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
  mp->text = NULL;

  return mp;
}  /* alloc_macro */

#endif /* RECORD_MACROS_IN_IL */
#if DEBUG
unsigned long show_il_alloc_space_used(unsigned long grand_total)
/*
Display and return the amount of space used for various IL tables.
*/
{
  unsigned long num, size, total;

  db_space_used_header("IL table use:");

  db_space_used("source file", num_source_files_allocated, a_source_file);
  db_space_used("constant", num_constants_allocated, a_constant);
  db_space_used("String literal text", string_literal_text_space_allocated,
                char);
  db_space_used("param type", num_param_types_allocated, a_param_type);
  db_space_used("routine type supplement",
                num_routine_type_supplements_allocated,
                a_routine_type_supplement);
  db_space_used("based type list member",
                num_based_type_list_members_allocated,
                a_based_type_list_member);
  db_space_used("class type supplement", num_class_type_supplements_allocated,
                a_class_type_supplement);
  db_space_used("class member using decls",
                num_class_member_using_decls_allocated,
                a_class_member_using_decl);
  db_space_used("class list entry", num_class_list_entries_allocated,
                a_class_list_entry);
  db_space_used("routine list entry", num_routine_list_entries_allocated,
                a_routine_list_entry);
  db_space_used("overriding virtual func",
                num_overriding_virtual_functions_allocated,
                an_overriding_virtual_function_ptr);
  db_space_used("derivation steps", num_derivation_steps_allocated,
                a_derivation_step);
  db_space_used("base class derivations", num_base_class_derivations_allocated,
                a_base_class_derivation);
  db_space_used("base class", num_base_classes_allocated, a_base_class);
  db_space_used("template args", num_template_args_allocated, a_template_arg);
  db_space_used("templ param type descrs",
                num_template_param_type_descrs_allocated,
                a_template_param_type_descr);
  db_space_used("type", num_types_allocated, a_type);
  db_space_used("dynamic init", num_dynamic_inits_allocated, a_dynamic_init);
  db_space_used("local static var inits",
                num_local_static_variable_inits_allocated,
                a_local_static_variable_init);
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
  db_space_used("throw supplement", num_throw_supplements_allocated,
                a_throw_supplement);
  db_space_used("condition supplement", num_condition_supplements_allocated,
                a_condition_supplement);
  db_space_used("accessible base class", num_accessible_base_classes_allocated,
                an_accessible_base_class);
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  db_space_used("eh prologue supplement",
                num_eh_prologue_supplements_allocated,
                an_eh_prologue_supplement);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
  db_space_used("switch clause",
                num_switch_clauses_allocated, a_switch_clause);
  db_space_used("handler", num_handlers_allocated, a_handler);
  db_space_used("try supplement", num_try_supplements_allocated,
                a_try_supplement);
#if MICROSOFT_EXTENSIONS_ALLOWED
  db_space_used("Microsoft try supplement",
                num_microsoft_try_supplements_allocated,
                a_microsoft_try_supplement);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  db_space_used("block", num_blocks_allocated, a_block);
  db_space_used("for_loop", num_for_loops_allocated, a_for_loop);
  db_space_used("statement", num_statements_allocated, a_statement);
  db_space_used("constructor init", num_constructor_inits_allocated,
                a_constructor_init);
  db_space_used("pragma", num_pragmas_allocated, a_pragma);
  db_space_used("object lifetime", num_object_lifetimes_allocated,
                an_object_lifetime);
  db_space_used("namespace", num_namespaces_allocated, a_namespace);
  db_space_used("using-directive", num_using_directives_allocated,
                a_using_directive);
  db_space_used("scope", num_scopes_allocated, a_scope);
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
#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
  db_space_used("comment", num_comments_allocated, a_comment);
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL
  db_space_used("hidden names", num_hidden_names_allocated, a_hidden_name);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if RECORD_TEMPLATES_IN_IL
  db_space_used("templates", num_templates_allocated, a_template);
#endif /* RECORD_TEMPLATES_IN_IL */
#if RECORD_MACROS_IN_IL
  db_space_used("macros", num_macros_allocated, a_macro);
#endif /* RECORD_MACROS_IN_IL */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  db_space_used("orphaned list headers",
                num_scope_orphaned_list_headers_allocated,
                a_scope_orphaned_list_header);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if ORPHAN_PROCESSING_NEEDED
  db_space_used_nontype("fs orphan pointers", num_fs_orphan_pointers_allocated,
                        SPACE_FOR_FS_ORPHAN_POINTER);
#endif /* ORPHAN_PROCESSING_NEEDED */
  db_space_used("IL entry prefix", num_il_entry_prefixes_allocated,
                an_il_entry_prefix);
#if ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  db_space_used_other("asm function bodies",
                      asm_function_body_space_allocated, "");
#endif /* ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */

  db_space_used_total();

#if 0
  (void)fputc('\n', f_debug);
  db_space_used_other("get_based_type_calls", num_get_based_type_calls, "");
  (void)fputc('\n', f_debug);
  db_space_used_other("num_shareable_constants", num_shareable_constants, "");
  db_space_used_other("Percent of buckets used",
                      (100 * num_used_shareable_constant_buckets) /
                      SIZE_SHAREABLE_CONSTANTS_TABLE, "");
  if (num_used_shareable_constant_buckets != 0) {
    db_space_used_float_other("Avg non-empty bucket len",
                             (double)num_shareable_constants /
                             (double)num_used_shareable_constant_buckets, "");
  }  /* if */
  db_space_used_other("num func shareable consts",
                      num_func_shareable_constants, "");
  db_space_used_other("Number of searches", 
                      num_searches_for_shareable_constants, "");
  if (num_searches_for_shareable_constants != 0) {
    db_space_used_float_other("Avg compares/search",
                             (double)num_compares_for_shareable_constants /
                             (double)num_searches_for_shareable_constants, "");
  }  /* if */
  (void)fputc('\n', f_debug);
  db_space_used_general_buffer("temp text buffer",
                               (unsigned long)size_temp_text_buffer);
#endif /* if 0 */

  return grand_total;
}  /* show_il_space_used */
#endif /* DEBUG */

void il_alloc_one_time_init(void)
/*
Do one-time initialization of variables related to the IL. (Variables
that need to be reinitialized with each new translation unit are handled
in il_init.)
*/
{
  /* Set the default "routine name linkage", which is the value to which the
     routine_name_linkage field of a routine type supplement is initialized. */
  default_routine_name_linkage = C_mode() ?
                                   (a_name_linkage_kind)nlk_none :
                                   (a_name_linkage_kind)nlk_cplusplus_external;

  /* Set the default source correspondence variable to default values. */
  def_source_corresp.assoc_info = NULL;
  def_source_corresp.name = NULL;
  def_source_corresp.parent.class_type = NULL;
  def_source_corresp.decl_position = null_source_position;
  /* access is set to "public" because "no access restriction" is the default
     for everything except class members.  For the latter the field must be
     set manually. */
  def_source_corresp.access = (an_access_specifier)as_public;
  /* referenced is set TRUE because initially the entity is not associated
     with one in the source program.  All unassociated entities are assumed
     to be referenced (otherwise, they wouldn't be created).  This does away
     with the difficult job of setting the referenced flag in a lot of
     different places for unassociated entities. set_source_corresp resets
     the flag to FALSE for associated entities, for which the flag is then
     set to TRUE (for an actual reference) by record_symbol_reference. */
  def_source_corresp.referenced = TRUE;
  def_source_corresp.name_linkage = (a_name_linkage_kind)nlk_none;
  def_source_corresp.has_associated_pragma = FALSE;
  def_source_corresp.is_local_to_function = FALSE;
  def_source_corresp.is_class_member = FALSE;
#if NEED_NAME_MANGLING
  def_source_corresp.name_has_been_mangled = FALSE;
  def_source_corresp.nested_type_mangling_has_been_done = FALSE;
#endif /* NEED_NAME_MANGLING */
#if BACK_END_IS_CP_GEN_BE
  def_source_corresp.global_qualification_needed = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
#if RECORD_SCOPE_DEPTH_IN_IL
  def_source_corresp.scope_depth = NO_SCOPE_DEPTH;
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  def_source_corresp.source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  /* Save static variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_template_args),
#if DEBUG
      pch_saved_var_array_elem(num_class_member_using_decls_allocated),
      pch_saved_var_array_elem(num_accessible_base_classes_allocated),
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
      pch_saved_var_array_elem(num_exception_specification_types_allocated),
      pch_saved_var_array_elem(num_exception_specifications_allocated),
      pch_saved_var_array_elem(num_expr_nodes_allocated),
      pch_saved_var_array_elem(num_fields_allocated),
      pch_saved_var_array_elem(num_for_loops_allocated),
      pch_saved_var_array_elem(num_handlers_allocated),
      pch_saved_var_array_elem(num_try_supplements_allocated),
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(num_microsoft_try_supplements_allocated),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pch_saved_var_array_elem(num_il_entry_prefixes_allocated),
      pch_saved_var_array_elem(num_labels_allocated),
      pch_saved_var_array_elem(num_new_delete_supplements_allocated),
      pch_saved_var_array_elem(num_overriding_virtual_functions_allocated),
      pch_saved_var_array_elem(num_param_types_allocated),
      pch_saved_var_array_elem(num_pragmas_allocated),
      pch_saved_var_array_elem(num_routine_list_entries_allocated),
      pch_saved_var_array_elem(num_routine_type_supplements_allocated),
      pch_saved_var_array_elem(num_routines_allocated),
      pch_saved_var_array_elem(num_object_lifetimes_allocated),
      pch_saved_var_array_elem(num_namespaces_allocated),
      pch_saved_var_array_elem(num_using_directives_allocated),
      pch_saved_var_array_elem(num_scopes_allocated),
      pch_saved_var_array_elem(num_source_files_allocated),
      pch_saved_var_array_elem(num_statements_allocated),
      pch_saved_var_array_elem(num_switch_clauses_allocated),
      pch_saved_var_array_elem(num_template_args_allocated),
      pch_saved_var_array_elem(num_template_param_type_descrs_allocated),
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
#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
      pch_saved_var_array_elem(num_comments_allocated),
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ORPHAN_PROCESSING_NEEDED
      pch_saved_var_array_elem(num_fs_orphan_pointers_allocated),
#endif /* ORPHAN_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
      pch_saved_var_array_elem(num_scope_orphaned_list_headers_allocated),
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_HIDDEN_NAMES_IN_IL
      pch_saved_var_array_elem(num_hidden_names_allocated),
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if RECORD_TEMPLATES_IN_IL
      pch_saved_var_array_elem(num_templates_allocated),
#endif /* RECORD_TEMPLATES_IN_IL */
#if RECORD_MACROS_IN_IL
      pch_saved_var_array_elem(num_macros_allocated),
#endif /* RECORD_MACROS_IN_IL */
#if ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(asm_function_body_space_allocated),
#endif /* ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* if DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* il_alloc_one_time_init */


void il_alloc_init(void)
/*
Initialize static variables related to IL allocation.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Static variables. */
  avail_template_args = NULL;
#if DEBUG
  num_source_files_allocated             = 0;
  num_constants_allocated                = 0;
  num_param_types_allocated              = 0;
  num_routine_type_supplements_allocated = 0;
  num_based_type_list_members_allocated  = 0;
  num_class_type_supplements_allocated   = 0;
  num_class_member_using_decls_allocated = 0;
  num_class_list_entries_allocated       = 0;
  num_routine_list_entries_allocated     = 0;
  num_overriding_virtual_functions_allocated
                                         = 0;
  num_derivation_steps_allocated         = 0;
  num_base_class_derivations_allocated   = 0;
  num_base_classes_allocated             = 0;
  num_template_args_allocated            = 0;
  num_template_param_type_descrs_allocated
                                         = 0;
  num_types_allocated                    = 0;
  num_dynamic_inits_allocated            = 0;
  num_local_static_variable_inits_allocated
                                         = 0;
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
  num_throw_supplements_allocated        = 0;
  num_condition_supplements_allocated    = 0;
  num_accessible_base_classes_allocated  = 0;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  num_eh_prologue_supplements_allocated  = 0;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
  num_switch_clauses_allocated           = 0;
  num_handlers_allocated                 = 0;
  num_try_supplements_allocated          = 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  num_microsoft_try_supplements_allocated= 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  num_blocks_allocated                   = 0;
  num_for_loops_allocated                = 0;
  num_statements_allocated               = 0;
  num_constructor_inits_allocated        = 0;
  num_pragmas_allocated                  = 0;
  num_object_lifetimes_allocated         = 0;
  num_namespaces_allocated               = 0;
  num_using_directives_allocated         = 0;
  num_scopes_allocated                   = 0;
  num_il_entry_prefixes_allocated        = 0;
  string_literal_text_space_allocated    = 0;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  num_source_sequence_entries_allocated  = 0;
  num_src_seq_secondary_decls_allocated  = 0;
  num_src_seq_end_of_constructs_allocated
                                         = 0;
  num_src_seq_sublists_allocated         = 0;
#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
  num_comments_allocated                 = 0;
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ORPHAN_PROCESSING_NEEDED
  num_fs_orphan_pointers_allocated       = 0;
#endif /* ORPHAN_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  num_scope_orphaned_list_headers_allocated = 0;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_HIDDEN_NAMES_IN_IL
  num_hidden_names_allocated             = 0;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if RECORD_TEMPLATES_IN_IL
  num_templates_allocated                = 0;
#endif /* RECORD_TEMPLATES_IN_IL */
#if RECORD_MACROS_IN_IL
  num_macros_allocated                   = 0;
#endif /* RECORD_MACROS_IN_IL */
#if ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  asm_function_body_space_allocated      = 0;
#endif /* ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* DEBUG */
}  /* il_alloc_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
